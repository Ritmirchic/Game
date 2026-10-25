# SPACE MINER — Windows deployment notes

## Artifacts

| File | Size | What it is |
| --- | --- | --- |
| `SPACE_MINER_SETUP.exe` | ~1.1 MB | Complete installer with the game embedded |
| `SPACE_MINER.exe` | ~316 KB | Native launcher (needs a `game\index.html` beside it) |
| `SPACE_MINER_PORTABLE.zip` | ~314 KB | Launcher + game folder + read-me, unzip and run |
| `SPACE_MINER_SOURCE.zip` | ~380 KB | Sources, build scripts, tests, C sources |

Both executables are PE32+ GUI binaries (subsystem 2) with an embedded 7-size
icon, product version `1.0.0.1` and no console window.

## How the launcher works

```
SPACE_MINER.exe
 ├─ verifies game\index.html exists (else: message box, exit code 1)
 ├─ creates %LOCALAPPDATA%\SpaceMiner as the browser profile
 ├─ starts a loopback HTTP server on 127.0.0.1:47821 (+ fallback range)
 ├─ shows a splash window while the page loads
 ├─ finds Edge → Chrome → Brave (App Paths registry, then Program Files)
 │    └─ CreateProcess "<browser>" --app=http://127.0.0.1:47821/index.html
 │         --window-size=1600,900 --user-data-dir=%LOCALAPPDATA%\SpaceMiner
 ├─ the page pings /__ping every 5 s; when the pings stop (window closed)
 │  the launcher waits 25 s and exits, releasing the port
 └─ /uninstall  → removes shortcuts, game files, browser profile, registry keys
```

Security of the local server:

* binds loopback only (`127.0.0.1`), never `0.0.0.0`;
* only `GET`/`HEAD` are served, everything else is `405`;
* paths are URL-decoded, then rejected on `..`, NUL, control characters, drive
  letters and backslash-smuggling — traversal is not possible;
* unknown routes return the game shell (single-page behaviour);
* no directory listing, no writes, no remote access;
* `Connection: close` on every response and 4-second socket timeouts, so a stuck
  client cannot stall the launcher.

## Installer behaviour

* **Scope:** current user only, no UAC prompt. Default target
  `%LOCALAPPDATA%\Programs\SpaceMiner`; the folder can be changed with *Browse…*
  or `/D=<path>`.
* **Options:** desktop shortcut, launch when finished.
* **Creates:** `SPACE_MINER.exe`, `game\index.html`, `game\icon.ico`,
  `game\BUILD.txt`, a Start Menu shortcut, and
  `HKCU\Software\Microsoft\Windows\CurrentVersion\Uninstall\SpaceMiner` so the game
  appears in *Settings → Apps*.
* **Payload:** a *stored* ZIP appended to the executable, found through a 16-byte
  footer (`SPMNRSET` + offset + length). Extraction runs on a worker thread with a
  progress bar, verifies each entry with CRC32, refuses `..` paths, and finally
  checks that `game\index.html` really landed on disk before reporting success.
* **Silent install:** `SPACE_MINER_SETUP.exe /S [/D=C:\Games\SpaceMiner]`.
* **Exit codes:** `0` success, non-zero when the payload is damaged or the target
  is not writable (a message box explains which).

## Uninstall

Run `SPACE_MINER.exe /uninstall` (the registered uninstaller) or use
*Settings → Apps → SPACE MINER → Uninstall*. It confirms first, then removes:

1. `Desktop\SPACE MINER.lnk` and `Start Menu\Programs\SPACE MINER.lnk`;
2. `game\` (recursively);
3. `%LOCALAPPDATA%\SpaceMiner` (browser profile that holds the save);
4. `HKCU\…\Uninstall\SpaceMiner` and `HKCU\Software\SpaceMiner`;
5. the executable and its folder — deleted by a short-lived `cmd.exe` helper after
   the launcher exits.

## Deploying a new build

```bash
ZIG=/path/to/zig node tools/release.js
# → dist/SPACE_MINER.exe, dist/SPACE_MINER_SETUP.exe,
#   dist/SPACE_MINER_PORTABLE.zip, dist/SPACE_MINER_SOURCE.zip
```

The release gate fails (non-zero exit) if any of the four suites reports a
failure, so a broken build cannot be shipped by accident.

Before publishing a public release: bump `APP_VER` in `win/launcher.c` and
`win/installer.c`, keep the version info strings in `tools/win.js` in sync, and
sign both executables if a certificate is available (SmartScreen otherwise warns
on first download).

## Verifying an artifact by hand

```bash
python3 - <<'PY'
import pefile
for f in ("dist/SPACE_MINER.exe", "dist/SPACE_MINER_SETUP.exe"):
    pe = pefile.PE(f)
    print(f, "subsystem", pe.OPTIONAL_HEADER.Subsystem, "machine", hex(pe.FILE_HEADER.Machine))
    print("  resources", [(t.id, len(t.directory.entries)) for t in pe.DIRECTORY_ENTRY_RESOURCE.entries])
PY
```

`Subsystem` must be `2` (GUI) and the resource list must contain `3`, `14` and
`16` (icons, icon group, version).
