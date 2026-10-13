#!/usr/bin/env python3
"""Build CyberFoil custom packs, converting icons to 128x128 WebP with Pillow."""

import argparse
import io
import json
import os
from pathlib import Path
import re
import struct
import tempfile


HEADER = struct.Struct("<8sIIIIQ")
TITLE_ENTRY = struct.Struct("<QIIIIQIIiI")
ICON_ENTRY = struct.Struct("<QQI8sI")
MAX_TITLE_INDEX = 32 * 1024 * 1024
MAX_STRINGS = 128 * 1024 * 1024
MAX_ICON_INDEX = 16 * 1024 * 1024
MAX_ICON = 16 * 1024 * 1024
STRING_FIELDS = ("name", "publisher", "intro", "description")
NUMBER_FIELDS = ("size", "version", "releaseDate")
FIELDS = set(STRING_FIELDS + NUMBER_FIELDS + ("isDemo", "icon"))
IMAGE_EXTENSIONS = {"png", "jpg", "jpeg", "webp", "bmp", "tif", "tiff"}


def title_id(value):
    if not isinstance(value, str) or not re.fullmatch(r"[0-9a-fA-F]{16}", value):
        raise ValueError(f"Title ID must be exactly 16 hexadecimal digits: {value!r}")
    return int(value, 16)


def unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def load_titles(path):
    with path.open(encoding="utf-8-sig") as source:
        root = json.load(source, object_pairs_hook=unique_object)
    if not isinstance(root, dict):
        raise ValueError("Titles JSON must be an object keyed by title ID")
    titles, icons, seen = {}, {}, set()
    for key, info in root.items():
        tid = title_id(key)
        if tid in seen:
            raise ValueError(f"Duplicate title ID: {key}")
        seen.add(tid)
        if not isinstance(info, dict) or not info:
            raise ValueError(f"{key}: expected a nonempty title information object")
        unknown = info.keys() - FIELDS
        if unknown:
            raise ValueError(f"{key}: unknown fields: {', '.join(sorted(unknown))}")
        metadata = {field: value for field, value in info.items() if field != "icon"}
        for field, value in metadata.items():
            if field in STRING_FIELDS:
                if not isinstance(value, str) or "\0" in value:
                    raise ValueError(f"{key}.{field}: expected a string without NUL characters")
            elif field == "isDemo":
                if not isinstance(value, bool):
                    raise ValueError(f"{key}.isDemo: expected true or false")
            else:
                maximum = (1 << (64 if field == "size" else 32)) - 1
                if type(value) is not int or not 0 <= value <= maximum:
                    raise ValueError(f"{key}.{field}: expected an integer from 0 to {maximum}")
        if metadata:
            if not any(metadata.get(field) for field in STRING_FIELDS) and not any(
                field in metadata for field in NUMBER_FIELDS + ("isDemo",)
            ):
                raise ValueError(f"{key}: metadata must contain a nonempty string or numeric/boolean field")
            titles[tid] = metadata
        if "icon" in info:
            icon = info["icon"]
            if not isinstance(icon, str) or not icon:
                raise ValueError(f"{key}.icon: expected an image file path")
            icons[tid] = path.parent / icon
    return titles, icons


def directory_icons(directory):
    if not directory.is_dir():
        raise ValueError(f"Icon directory does not exist: {directory}")
    icons = {}
    for path in sorted(directory.iterdir()):
        if not path.is_file() or path.suffix.lower().lstrip(".") not in IMAGE_EXTENSIONS:
            continue
        tid = title_id(path.stem)
        if tid in icons:
            raise ValueError(f"Multiple images for title ID {path.stem} in {directory}")
        icons[tid] = path
    return icons


def convert_icon(path):
    try:
        from PIL import Image, ImageOps, features
    except ImportError as error:
        raise ValueError('Image conversion requires Pillow: python -m pip install "Pillow>=9.1"') from error
    if not features.check("webp"):
        raise ValueError("Pillow must be built with WebP support")
    ext = path.suffix.lower().lstrip(".")
    if ext not in IMAGE_EXTENSIONS:
        raise ValueError(f"Unsupported image extension: {path}")
    with path.open("rb") as source:
        prefix = source.read(12)
    signatures = {
        "png": prefix.startswith(b"\x89PNG\r\n\x1a\n"),
        "jpg": prefix.startswith(b"\xff\xd8\xff"),
        "jpeg": prefix.startswith(b"\xff\xd8\xff"),
        "webp": prefix[:4] == b"RIFF" and prefix[8:12] == b"WEBP",
        "bmp": prefix.startswith(b"BM"),
        "tif": prefix.startswith((b"II\x2a\x00", b"MM\x00\x2a")),
        "tiff": prefix.startswith((b"II\x2a\x00", b"MM\x00\x2a")),
    }
    if not signatures[ext]:
        raise ValueError(f"Image signature does not match its extension: {path}")
    try:
        with Image.open(path) as image:
            image = ImageOps.exif_transpose(image)
            # Match Offline-DB-Maker's icon pipeline, including alpha handling.
            if image.mode not in ("RGB", "RGBA"):
                image = image.convert("RGB")
            elif image.mode == "RGBA":
                background = Image.new("RGB", image.size, (255, 255, 255))
                background.paste(image, mask=image.split()[-1])
                image = background
            fitted = ImageOps.fit(image, (128, 128), method=Image.Resampling.LANCZOS)
            output = io.BytesIO()
            fitted.save(output, format="WEBP", quality=80, method=6)
            data = output.getvalue()
    except (OSError, ValueError, Image.DecompressionBombError) as error:
        raise ValueError(f"Cannot convert image {path}: {error}") from error
    if not 0 < len(data) <= MAX_ICON:
        raise ValueError(f"Converted icon exceeds CyberFoil's 16 MiB limit: {path}")
    return data


def build_titles(titles):
    if len(titles) * TITLE_ENTRY.size > MAX_TITLE_INDEX:
        raise ValueError("Title index exceeds CyberFoil's 32 MiB limit")
    strings = bytearray(b"\0")  # Offset zero means no string.
    offsets = {"": 0}
    entries = bytearray()
    for tid, info in sorted(titles.items()):
        flags = 0
        row_offsets = []
        for bit, field in enumerate(STRING_FIELDS):
            value = info.get(field, "")
            if field in info:
                flags |= 1 << bit
            if value not in offsets:
                encoded = value.encode("utf-8") + b"\0"
                if len(strings) + len(encoded) > MAX_STRINGS:
                    raise ValueError("Title strings exceed CyberFoil's 128 MiB limit")
                offsets[value] = len(strings)
                strings.extend(encoded)
            row_offsets.append(offsets[value])
        for bit, field in enumerate(NUMBER_FIELDS + ("isDemo",), start=4):
            if field in info:
                flags |= 1 << bit
        entries.extend(TITLE_ENTRY.pack(
            tid, *row_offsets, info.get("size", 0), info.get("version", 0),
            info.get("releaseDate", 0), int(info.get("isDemo", False)), flags,
        ))
    return HEADER.pack(b"CFTITLE1", 1, TITLE_ENTRY.size, len(titles), 0,
                       HEADER.size + len(entries)) + entries + strings


def build_icon_index(icons, converted_dir):
    if len(icons) * ICON_ENTRY.size > MAX_ICON_INDEX:
        raise ValueError("Icon index exceeds CyberFoil's 16 MiB limit")
    entries, sources = bytearray(), []
    offset = 0
    for tid, path in sorted(icons.items()):
        data = convert_icon(path)
        size = len(data)
        converted = converted_dir / f"{tid:016x}.webp"
        converted.write_bytes(data)
        entries.extend(ICON_ENTRY.pack(tid, offset, size, b"webp", 0))
        sources.append((converted, size))
        offset += size
    return HEADER.pack(b"CFICONP1", 1, ICON_ENTRY.size, len(icons), 0,
                       HEADER.size + len(entries)) + entries, sources


def write_pack(destination, prefix, images=()):
    # Replace a pack only after the complete new file has been written.
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as output:
            temporary = Path(output.name)
            output.write(prefix)
            for path, expected_size in images:
                with path.open("rb") as source:
                    remaining = expected_size
                    while remaining:
                        chunk = source.read(min(remaining, 1024 * 1024))
                        if not chunk:
                            raise ValueError(f"Image changed while building: {path}")
                        output.write(chunk)
                        remaining -= len(chunk)
                    if source.read(1):
                        raise ValueError(f"Image changed while building: {path}")
        os.replace(temporary, destination)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--titles", type=Path, help="JSON object keyed by 16-digit title ID; icon paths are relative to this file")
    parser.add_argument("--icons-dir", type=Path, help="Directory of images named <16-digit-title-id>.<extension>")
    parser.add_argument("--output", type=Path, default=Path("custom_packs"), help="Output directory (default: custom_packs)")
    args = parser.parse_args(argv)
    if args.titles is None and args.icons_dir is None:
        parser.error("provide --titles, --icons-dir, or both")
    try:
        titles, explicit_icons = load_titles(args.titles) if args.titles else ({}, {})
        icons = directory_icons(args.icons_dir) if args.icons_dir else {}
        icons.update(explicit_icons)  # Explicit JSON paths take priority over the directory.
        if not titles and not icons:
            raise ValueError("No title information or images were supplied")
        # Validate both inputs before replacing either existing output pack.
        title_data = build_titles(titles) if titles else None
        with tempfile.TemporaryDirectory(prefix="cyberfoil-icons-") as temporary:
            icon_data = build_icon_index(icons, Path(temporary)) if icons else None
            args.output.mkdir(parents=True, exist_ok=True)
            if title_data is not None:
                destination = args.output / "custom_titles.pack"
                write_pack(destination, title_data)
                print(f"Created {destination} ({len(titles)} titles)")
            if icon_data is not None:
                destination = args.output / "custom_icons.pack"
                write_pack(destination, *icon_data)
                print(f"Created {destination} ({len(icons)} icons)")
    except (OSError, ValueError) as error:
        parser.error(str(error))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
