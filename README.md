# CyberFoil
[![Latest version](https://img.shields.io/github/v/release/luketanti/CyberFoil?label=version)](https://github.com/luketanti/CyberFoil/releases/latest)
![GitHub all releases downloads](https://img.shields.io/github/downloads/luketanti/CyberFoil/total)
![GitHub latest release downloads](https://img.shields.io/github/downloads/luketanti/CyberFoil/latest/total)
[![Discord](https://img.shields.io/badge/Discord-Join%20Server-5865F2?logo=discord&logoColor=white)](https://discord.gg/gGy7hWxJeP)


## Features
- Simple homebrew file installer
- Works best with AeroFoil
- Save Data Backup manager: browse console/server saves, upload with notes, download specific own save data versions, and delete save data files from server

## Save Sync (Only works with compatible AeroFoil)
- Available when the account has backup access.
- Shows local console saves and remote server save data backups in a dedicated `Saves` section.
- Upload supports version notes.
- Download/delete of remote save data backups supports per-version selection with an in-page selector layout.

## Cheats (Currently only works with AeroFoil)
- Browse cheats published by a compatible AeroFoil server in the dedicated `Cheats` section.
- Download and install individual cheats, or install all available cheats at once.
- Cheats are installed to `atmosphere/contents/<title-id>/cheats/<build-id>.txt`.
- Manage local cheats: view, create, edit, and delete cheat files; upload edited cheats to AeroFoil when permitted.

## Offline Metadata/Icons
- CyberFoil can use local title metadata and local icons from `sdmc:/switch/CyberFoil/offline_db/`.

## Configuration
- In Settings > Remote, choose **Discover LAN Servers**. Scans take about three seconds; B cancels and X scans again. Results show name, LAN address/HTTP port, version, and public/private status. Select a result, then use the normal Remote form and **Save Remote** to save/apply it. Manual entry remains available and discovery does not overwrite a saved URL on its own.
- AeroFoil's **LAN discovery** setting must be enabled, and UDP **8465** must be allowed through the server firewall. Wi-Fi client isolation and separate subnets can prevent discovery. Docker must deliver LAN broadcasts to the container; host networking works on supported platforms (publishing a UDP port alone may not deliver broadcast traffic).
- Each Remote can have a **LAN URL** and a **Remote URL**. Enter only the IP or hostname and optional `:port`, e.g. `192.168.1.100:8465` and `shop.example.com:443`; schemes are supplied automatically. **LAN protocol** has an HTTP/HTTPS dropdown. **Remote URL** always uses HTTPS. Omitted ports use HTTP 80 or HTTPS 443. Either address may be left empty, and existing optional paths remain supported. Username/password are shared between both addresses.
- **Connection** can be **Auto (LAN first)**, **LAN only**, or **WAN only**. Auto checks LAN connectivity with a roughly two-second timeout, then uses WAN if LAN is unreachable. With one address configured, it uses that address directly. Authentication and catalogue errors do not trigger fallback. The selected address stays fixed during browsing/downloads; reopening Remote after the console's IP changes, or explicitly refreshing the catalogue, rechecks connectivity.
- Discovery fills LAN URL from the reply source IPv4 address and advertised HTTP port. A valid optional `remote` value can suggest the WAN URL for review; it never replaces an existing WAN URL. A configured host without a scheme is suggested as HTTPS; explicit HTTP remote metadata is ignored. Saved discovery UUIDs let rediscovery update the same profile; manually entered profiles with the same LAN URL can also be reused. Discovery supplies no credentials and preserves the profile's credentials, Remote mode, and User-Agent settings. Private servers still need the normal username/password configuration.
- Existing profiles retain their original URL and credentials. Legacy public HTTPS addresses go into Remote URL; private IPs and local hostnames remain in LAN URL. Importing a standalone URL from an old config runs once; modern config reloads and runtime WAN selection do not create additional Remote entries. Remote JSON files additionally store `lanUrl`, `wanUrl`, `connection` (`auto`/`lan`/`wan`), and `uid`; the previous protocol/host/port/path fields remain for compatibility. `remoteProfileFile` in `config.json` identifies the active saved profile regardless of which endpoint is used at runtime.
- `sdmc:/switch/CyberFoil/config.json` supports optional `httpUserAgentMode` and `httpUserAgent`.
- `httpUserAgentMode` can be `default`, `chrome`, `safari`, `firefox`, `tinfoil`, or `custom`.
- `default` uses `cyberfoil`.
- `tinfoil` sends an empty User-Agent value.
- Preset/custom modes are applied across network requests (downloads, Remote, and save sync).
- `httpUserAgent` is used when `httpUserAgentMode` is `custom`.

## Release Package Layout
- Build output: `cyberfoil.nro`
- SD layout in release zip: `switch/CyberFoil/cyberfoil.nro`

## Discovery validation
- Run `make -C tests test` on a POSIX host with a C++20 compiler. Tests compile the production parser/scanner and exercise JSON validation, string/reply bounds, LAN URLs, UUID deduplication, exact request bytes, spaced retries, unicast replies, cancellation/repeated scans, no results, send failures, and socket cleanup. Endpoint tests cover full URL validation/default ports, Auto/forced selection, one-address profiles, legacy LAN/WAN classification, one-time config import without duplicate entries, runtime-versus-saved URL persistence, and shared credentials/User-Agent settings. The test responder uses loopback UDP 8465; that port must be free locally.
- On a Switch, check Wi-Fi and Ethernet discovery with multiple AeroFoil servers, B cancellation, X rescans, touch/controller selection, long/Unicode labels, and no-network/no-results messages. Check that dismissing results leaves the saved URL intact, and that selecting then saving uses the advertised HTTP port and preserves credentials. Verify private-server authentication, catalogue/downloads, and the configured User-Agent through the existing Remote flow. Confirm broadcast delivery and Docker networking on the actual LAN.
- For dual addresses, check Auto on the home LAN and an external network, LAN-only/WAN-only selection, independent paths and ports, incorrect credentials without fallback, stable download/save-sync addresses, and catalogue refresh after changing networks. Rediscover a saved server and confirm its WAN URL, credentials, and file identity are retained. Check that a legacy profile loads and saves without changing its original address.

## Note
- Uses [XorTroll's Plutonium](https://github.com/XorTroll/Plutonium) for a pretty graphical interface



