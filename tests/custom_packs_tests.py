"""Run with: python -m unittest discover -s tests -p custom_packs_tests.py"""

import base64
from contextlib import redirect_stdout, redirect_stderr
import importlib.util
import io
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest.mock import patch
from PIL import Image, ImageOps


spec = importlib.util.spec_from_file_location(
    "create_custom_packs", Path(__file__).resolve().parents[1] / "tools/create_custom_packs.py"
)
packs = importlib.util.module_from_spec(spec)
spec.loader.exec_module(packs)
PNG = base64.b64decode(
    "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAQAAAC1HAwCAAAAC0lEQVR42mP8/x8AAwMCAO+jRZkAAAAASUVORK5CYII="
)
ID = "0100000000001000"
OTHER_ID = "0100000000002000"


class CustomPackTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = Path(self.temp.name)
        self.output = self.root / "output"
        self.titles = self.root / "titles.json"

    def run_tool(self, *args):
        with redirect_stdout(io.StringIO()), redirect_stderr(io.StringIO()):
            return packs.main([*map(str, args), "--output", str(self.output)])

    def test_both_packs_match_reader_layout(self):
        (self.root / "art.png").write_bytes(PNG)
        self.titles.write_text(json.dumps({
            OTHER_ID: {"name": "Other"},
            ID: {"name": "Pokémon 日本語", "publisher": "Publisher", "intro": "Intro",
                 "description": "Description", "size": 2**40, "version": 65536,
                 "releaseDate": 20261013, "isDemo": False, "icon": "art.png"},
        }), encoding="utf-8")
        self.assertEqual(self.run_tool("--titles", self.titles), 0)
        data = (self.output / "custom_titles.pack").read_bytes()
        # Decode independently using the production reader's exact binary layouts.
        self.assertEqual(struct.unpack_from("<8sIIIIQ", data), (b"CFTITLE1", 1, 48, 2, 0, 128))
        row = struct.unpack_from("<QIIIIQIIiI", data, 32)
        self.assertEqual(row[0], int(ID, 16))
        self.assertEqual(row[5:], (2**40, 65536, 20261013, 0, 255))
        blob = data[128:]
        self.assertEqual(blob[0], 0)
        for offset, expected in zip(row[1:5], ("Pokémon 日本語", "Publisher", "Intro", "Description")):
            self.assertEqual(blob[offset:].split(b"\0", 1)[0].decode("utf-8"), expected)
        self.assertEqual(struct.unpack_from("<Q", data, 80)[0], int(OTHER_ID, 16))
        icons = (self.output / "custom_icons.pack").read_bytes()
        self.assertEqual(struct.unpack_from("<8sIIIIQ", icons), (b"CFICONP1", 1, 32, 1, 0, 64))
        self.assertEqual(struct.unpack_from("<QQI8sI", icons, 32),
                         (int(ID, 16), 0, len(icons) - 64, b"webp\0\0\0\0", 0))
        with Image.open(io.BytesIO(icons[64:])) as image:
            self.assertEqual(image.format, "WEBP")
            self.assertEqual(image.size, (128, 128))
            self.assertEqual(image.mode, "RGB")

    def test_icons_only_and_json_path_priority(self):
        directory = self.root / "icons"
        directory.mkdir()
        (directory / f"{ID}.PNG").write_bytes(PNG)
        (directory / f"{OTHER_ID}.png").write_bytes(PNG)
        self.run_tool("--icons-dir", directory)
        self.assertFalse((self.output / "custom_titles.pack").exists())
        original = (self.output / "custom_icons.pack").read_bytes()
        first_size = struct.unpack_from("<QQI8sI", original, 32)[2]
        first_icon = original[96:96 + first_size]
        self.assertEqual(original[96:], first_icon + first_icon)
        Image.new("RGB", (512, 256), "red").save(self.root / "chosen.png")
        self.titles.write_text(json.dumps({ID: {"icon": "chosen.png"}}), encoding="utf-8")
        self.run_tool("--icons-dir", directory, "--titles", self.titles)
        updated = (self.output / "custom_icons.pack").read_bytes()
        size = struct.unpack_from("<QQI8sI", updated, 32)[2]
        second = struct.unpack_from("<QQI8sI", updated, 64)
        self.assertEqual(second[1], size)
        self.assertEqual(updated[96 + size:], first_icon)
        with Image.open(io.BytesIO(updated[96:96 + size])) as image:
            self.assertGreater(image.getpixel((64, 64))[0], 245)
            self.assertLess(image.getpixel((64, 64))[1], 10)

    def test_titles_only_presence_flags_and_deterministic_output(self):
        self.titles.write_text(json.dumps({ID: {"name": "Demo", "size": 0, "isDemo": True}}))
        self.run_tool("--titles", self.titles)
        data = (self.output / "custom_titles.pack").read_bytes()
        row = struct.unpack_from("<QIIIIQIIiI", data, 32)
        self.assertEqual(row[-2:], (1, 145))
        self.assertFalse((self.output / "custom_icons.pack").exists())
        self.run_tool("--titles", self.titles)
        self.assertEqual((self.output / "custom_titles.pack").read_bytes(), data)

    def test_invalid_inputs_do_not_replace_existing_outputs(self):
        self.output.mkdir()
        for name in ("custom_titles.pack", "custom_icons.pack"):
            (self.output / name).write_bytes(b"existing")
        cases = [
            {"bad-id": {"name": "Bad"}},
            {ID: {"size": -1}}, {ID: {"version": 2**32}},
            {ID: {"isDemo": 1}}, {ID: {"name": "bad\0string"}},
            {ID: {"name": ""}}, {ID: {"nmae": "Typo"}},
            {ID: {"name": "Valid", "icon": "missing.png"}},
        ]
        for case in cases:
            with self.subTest(case=case):
                self.titles.write_text(json.dumps(case))
                with self.assertRaises(SystemExit) as error:
                    self.run_tool("--titles", self.titles)
                self.assertEqual(error.exception.code, 2)
                for name in ("custom_titles.pack", "custom_icons.pack"):
                    self.assertEqual((self.output / name).read_bytes(), b"existing")

    def test_duplicate_ids_and_invalid_images(self):
        self.titles.write_text('{"' + ID + '": {"name":"One"}, "' + ID + '": {"name":"Two"}}')
        with self.assertRaises(SystemExit):
            self.run_tool("--titles", self.titles)
        directory = self.root / "icons"
        directory.mkdir()
        (directory / f"{ID}.png").write_bytes(b"not an image")
        with self.assertRaises(SystemExit):
            self.run_tool("--icons-dir", directory)
        (directory / f"{ID}.jpg").write_bytes(b"\xff\xd8\xff")
        with self.assertRaises(SystemExit):
            self.run_tool("--icons-dir", directory)

    def test_pack_and_image_limits(self):
        with patch.object(packs, "MAX_TITLE_INDEX", 47):
            with self.assertRaises(ValueError):
                packs.build_titles({int(ID, 16): {"name": "Name"}})
        with patch.object(packs, "MAX_STRINGS", 4):
            with self.assertRaises(ValueError):
                packs.build_titles({int(ID, 16): {"name": "Name"}})
        image = self.root / "large.png"
        image.write_bytes(PNG)
        with patch.object(packs, "MAX_ICON", 1):
            with self.assertRaises(ValueError):
                packs.convert_icon(image)
        with patch.object(packs, "MAX_ICON_INDEX", 31):
            with self.assertRaises(ValueError):
                packs.build_icon_index({int(ID, 16): image}, self.root)

    def test_conversion_matches_offline_db_maker(self):
        for mode in ("RGB", "RGBA", "L"):
            with self.subTest(mode=mode):
                path = self.root / "source.png"
                image = Image.new(mode, (400, 200))
                color = {"RGB": (220, 40, 20), "RGBA": (220, 40, 20, 100), "L": 80}[mode]
                image.paste(color, (100, 0, 300, 200))
                image.save(path)
                with Image.open(path) as reference:
                    reference = ImageOps.exif_transpose(reference)
                    if reference.mode not in ("RGB", "RGBA"):
                        reference = reference.convert("RGB")
                    elif reference.mode == "RGBA":
                        background = Image.new("RGB", reference.size, (255, 255, 255))
                        background.paste(reference, mask=reference.split()[-1])
                        reference = background
                    reference = ImageOps.fit(reference, (128, 128), method=Image.Resampling.LANCZOS)
                    expected = io.BytesIO()
                    reference.save(expected, format="WEBP", quality=80, method=6)
                self.assertEqual(packs.convert_icon(path), expected.getvalue())

    def test_exif_orientation_and_corrupt_input(self):
        path = self.root / "oriented.jpg"
        image = Image.new("RGB", (128, 128), "red")
        image.paste("blue", (64, 0, 128, 128))
        exif = image.getexif()
        exif[274] = 6  # Rotate 90 degrees clockwise: left/right becomes top/bottom.
        image.save(path, exif=exif)
        with Image.open(io.BytesIO(packs.convert_icon(path))) as converted:
            self.assertGreater(converted.getpixel((64, 20))[0], 240)
            self.assertGreater(converted.getpixel((64, 108))[2], 240)
        path = self.root / "broken.png"
        path.write_bytes(b"\x89PNG\r\n\x1a\n" + b"truncated")
        self.titles.write_text(json.dumps({ID: {"name": "Valid", "icon": "broken.png"}}))
        with self.assertRaises(SystemExit):
            self.run_tool("--titles", self.titles)
        self.assertFalse(self.output.exists())


if __name__ == "__main__":
    unittest.main()
