# CyberFoil
[![Latest version](https://img.shields.io/github/v/release/luketanti/CyberFoil?label=version)](https://github.com/luketanti/CyberFoil/releases/latest)
![GitHub all releases downloads](https://img.shields.io/github/downloads/luketanti/CyberFoil/total)
![GitHub latest release downloads](https://img.shields.io/github/downloads/luketanti/CyberFoil/latest/total)
[![Discord](https://img.shields.io/badge/Discord-Join%20Server-5865F2?logo=discord&logoColor=white)](https://discord.gg/gGy7hWxJeP)


## Features
- Simple homebrew file installer
- Works best with [AeroFoil](https://github.com/luketanti/AeroFoil)
- Save Data Backup manager: browse console/server saves, upload with notes, download specific own save data versions, and delete save data files from server

## Save Sync (Only works with compatible [AeroFoil](https://github.com/luketanti/AeroFoil))
- Available when the account has backup access.
- Shows local console saves and remote server save data backups in a dedicated `Saves` section.
- Upload supports version notes.
- Download/delete of remote save data backups supports per-version selection with an in-page selector layout.

## Cheats (Currently only works with [AeroFoil](https://github.com/luketanti/AeroFoil))
- Browse cheats published by a compatible [AeroFoil](https://github.com/luketanti/AeroFoil) server in the dedicated `Cheats` section.
- Download and install individual cheats, or install all available cheats at once.
- Cheats are installed to `atmosphere/contents/<title-id>/cheats/<build-id>.txt`.
- Manage local cheats: view, create, edit, and delete cheat files; upload edited cheats to [AeroFoil](https://github.com/luketanti/AeroFoil) when permitted.

## Offline Metadata/Icons
- CyberFoil can use local title metadata and local icons from `sdmc:/switch/CyberFoil/offline_db/`.
- Optional `custom_titles.pack` and `custom_icons.pack` in that directory add entries alongside the default `titles.pack` and `icons.pack`. When a title ID appears in both, the custom title metadata or icon takes priority. Either custom pack can be used independently and must use the same binary format as its default pack. Offline database updates preserve these files; restart CyberFoil after manually replacing them.

### Creating custom packs

Use Python 3.8 or newer. Install Pillow with WebP support for image conversion:

```sh
python -m pip install "Pillow>=9.1"
python tools/create_custom_packs.py --titles my_titles.json --icons-dir my_icons --output custom_packs
```

Either `--titles` or `--icons-dir` can be omitted. The tool writes `custom_titles.pack` when metadata is supplied and `custom_icons.pack` when images are supplied. Copy the generated packs into `switch/CyberFoil/offline_db/` on the SD card, then restart CyberFoil. Existing output packs of the same kind are replaced; an output pack for an omitted kind is left untouched.

Title information is a UTF-8 JSON object keyed by 16-digit hexadecimal base title ID, for example:

```json
{
  "0100000000001000": {
    "name": "My Custom Title",
    "publisher": "My Publisher",
    "intro": "Short introduction",
    "description": "Full description",
    "size": 1048576,
    "version": 65536,
    "releaseDate": 20261013,
    "isDemo": false,
    "icon": "images/my-title.png"
  }
}
```

All fields are optional, but each entry needs metadata or an `icon`. `size` is in bytes, `version` is the numeric title version, `releaseDate` uses the numeric `YYYYMMDD` format, and `isDemo` is a JSON boolean. A custom title entry replaces the entire default entry for that ID, so include every metadata field you want to keep.

An `icon` path is relative to the JSON file (absolute paths also work). Alternatively, place images named after title IDs, such as `0100000000001000.png`, directly inside `--icons-dir`. Explicit JSON icon paths take priority over images in that directory. Supported input extensions are PNG, JPG/JPEG, WebP, BMP, and TIF/TIFF; images must match their file extension. Icons are processed with the same settings as Offline-DB-Maker: EXIF orientation is applied, RGBA transparency is composited onto white, other color modes are converted to RGB, and images are center-cropped and resized to **128×128** with Lanczos, then encoded as **WebP, quality 80, method 6**. Original image files are left untouched. Converted icons must be no larger than 16 MiB each. Duplicate IDs, invalid fields, missing or corrupt images, and pack size limits are checked before output is written. Titles-only generation does not require Pillow.

Run the pack format regression tests with `python -m unittest discover -s tests -p custom_packs_tests.py`.

## Remote Configuration
- For LAN remote, in Settings > Remote, choose **Discover LAN Servers**. Scans take about three seconds; B cancels and X scans again. Results show name, LAN address/HTTP port, version, and public/private status. Select a result, then use the normal Remote form and **Save Remote** to save/apply it. Manual entry remains available and discovery does not overwrite a saved URL on its own.
- [AeroFoil](https://github.com/luketanti/AeroFoil)'s **LAN discovery** setting must be enabled, and UDP **8465** must be allowed through the server firewall. Wi-Fi client isolation and separate subnets can prevent discovery. Docker must deliver LAN broadcasts to the container; host networking works on supported platforms (publishing a UDP port alone may not deliver broadcast traffic).
- Each Remote can have a **LAN URL** and a **Remote URL**. Enter only the IP or hostname and optional `:port`, e.g. `192.168.1.100:8465` and `remote.example.com:443`; schemes are supplied automatically. **LAN protocol** has an HTTP/HTTPS dropdown. **Remote URL** always uses HTTPS. Omitted ports use HTTP 80 or HTTPS 443. Either address may be left empty, and existing optional paths remain supported. Username/password are shared between both addresses.
- **Connection** can be **Auto (LAN first)**, **LAN only**, or **WAN only**. Auto checks LAN connectivity with a roughly two-second timeout, then uses WAN if LAN is unreachable. With one address configured, it uses that address directly. Authentication and catalogue errors do not trigger fallback. The selected address stays fixed during browsing/downloads; reopening Remote after the console's IP changes, or explicitly refreshing the catalogue, rechecks connectivity.
- Discovery fills LAN URL from the reply source IPv4 address and advertised HTTP port. A valid optional `remote` value can suggest the WAN URL for review; it never replaces an existing WAN URL. A configured host without a scheme is suggested as HTTPS; explicit HTTP remote metadata is ignored. Saved discovery UUIDs let rediscovery update the same profile; manually entered profiles with the same LAN URL can also be reused. Discovery supplies no credentials and preserves the profile's credentials, Remote mode, and User-Agent settings. Private servers still need the normal username/password configuration.
- Existing profiles retain their original URL and credentials. Legacy public HTTPS addresses go into Remote URL; private IPs and local hostnames remain in LAN URL. Importing a standalone URL from an old config runs once; modern config reloads and runtime WAN selection do not create additional Remote entries. Remote JSON files additionally store `lanUrl`, `wanUrl`, `connection` (`auto`/`lan`/`wan`), and `uid`; the previous protocol/host/port/path fields remain for compatibility. `remoteProfileFile` in `config.json` identifies the active saved profile regardless of which endpoint is used at runtime.
- `sdmc:/switch/CyberFoil/config.json` supports optional `httpUserAgentMode` and `httpUserAgent`.
- `httpUserAgentMode` can be `default`, `chrome`, `safari`, `firefox`, `tinfoil`, or `custom`.
- `default` uses `cyberfoil`.
- `tinfoil` sends an empty User-Agent value.
- Preset/custom modes are applied across network requests (downloads, Remote, and save sync).
- `httpUserAgent` is used when `httpUserAgentMode` is `custom`.

## Installation controls

- The USB SSD/HDD browser hides dot-prefixed files and folders by default, including macOS `._` sidecar files and `.Trashes`, plus Windows metadata names `$RECYCLE.BIN`, `System Volume Information`, `desktop.ini`, and `Thumbs.db` (matched case-insensitively). Press **−** or tap **Show hidden / Hide hidden** in the bottom bar to toggle visibility. Show hidden reveals filtered folders and installable files; `.ini` and `.db` files remain excluded by the installable-file extension filter. The preference is saved and is also available in **Settings > General > Show hidden files on USB drives**. Hidden entries are excluded from Select All and new install selections while hiding is enabled.
- **B** immediately requests cancellation of the active title and stops the remaining batch. The screen stays responsive while rollback finishes. New content and placeholders from that title are removed; previously installed content and completed titles are retained. Title records are published after content transfer completes.
- **X** blanks the screen and turns off the handheld backlight while installation continues. Any button or touch wakes the screen; the wake gesture does not activate a control. The screen also wakes when the batch finishes or fails.
- The installation screen shows an active-title card and six numbered pending rows with cached artwork, type/size details, a selection highlight, and waiting/completed counts. **Up/Down** selects a row, **Left/Right** pages six rows, and a touch selects a row. **A** removes the selected pending title.
- **Y Add titles** opens the normal shop browser during Remote installation. Search, sections, list/grid view, and multi-selection remain available. Select titles with **A**, then press **+ Add to queue** to append them to the existing storage target and return to the queue; **B Queue** returns without adding. Active, queued, and completed titles cannot be added twice. The session stays open if the queue drains while choosing more titles. SD/USB drive installs return to the current folder's file browser. PC USB/network batches remain limited to the supplied batch; MTP transfers remain host-controlled.
- Batch installs run on a worker while the UI polls input and renders independently of transfer progress. Holding a direction repeats queue navigation. Progress updates are coalesced; all dialogs and texture changes execute on the UI thread.
- Queue and UI-dispatch regression tests are in `tests/install_queue_tests.cpp` and `tests/ui_dispatcher_tests.cpp` and run with `make -C tests test` on a POSIX host. Device checks should cover short button taps and held navigation during slow HTTP requests and local writes, canceling an NSP/XCI midway, preserving existing content, editing the queue during transfer, and waking the screen without canceling.

## Release Package Layout
- Build output: `cyberfoil.nro`
- SD layout in release zip: `switch/CyberFoil/cyberfoil.nro`

## Note
- Uses [XorTroll's Plutonium](https://github.com/XorTroll/Plutonium) for a pretty graphical interface
- Forked from [Awoo Installer](https://github.com/Huntereb/Awoo-Installer)




