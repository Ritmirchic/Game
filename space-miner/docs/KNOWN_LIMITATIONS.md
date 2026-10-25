# Known limitations

Written plainly, because a release note that hides its gaps is worse than no
release note.

## 1. The executables were cross-compiled, not run, on this machine

`dist/SPACE_MINER.exe` and `dist/SPACE_MINER_SETUP.exe` are genuine x86-64
Windows PE32+ GUI binaries produced from the C sources in `win/` with Zig's
`clang`/`lld` (target `x86_64-windows-gnu`). They were **not executed on Windows
during development** — the build environment has no Windows machine, no Wine and
no network access to a Windows runner.

What that means in practice:

* The *game* is fully tested: 198 assertions, including a test that boots the
  shipped HTML file in a mocked DOM and plays it.
* The *installer payload logic* is tested against the real release binary
  (byte-identical extraction, CRC validation, corruption rejection).
* The *launcher's HTTP server* is the same code compiled for Linux and driven
  over real TCP by `tools/testhttp.js`.
* What is verified only structurally for the Windows side: PE headers, subsystem,
  sections, imported DLLs, resource tree, version info, icon payloads, installer
  footer/payload geometry. The Win32 calls themselves (window creation, browser
  discovery via registry/App Paths, `CreateProcess` with `--app=`, shortcut
  creation through `IShellLink`, uninstall) were verified by review, not by
  execution.

If anything misbehaves on a real machine it will be in that thin Win32 layer,
not in the game or in the file server. The launcher degrades safely: if the
loopback server cannot start, it opens the game over `file://`; if no
Chromium-based browser is found, it uses the default browser; the game itself has
no network dependency at all and can always be opened by double-clicking
`game\index.html`.

## 2. Browser requirement

The launcher opens the game in a chromeless app window using Microsoft Edge or
Google Chrome (both ship with / are routinely installed on Windows 10/11). With
neither present, the game opens in the default browser with normal browser UI.
There is no embedded engine: the game is HTML5 canvas + WebAudio, deliberately,
so there is no 100 MB runtime to download and no unsigned browser engine to
worry about.

## 3. Code signing

The executables are **not Authenticode-signed** — there is no code-signing
certificate in this environment. Windows SmartScreen will show "Windows protected
your PC" for a freshly downloaded, unsigned installer; "More info → Run anyway"
is required. Signing the two binaries with an EV certificate is the only way to
remove that prompt, and can be done without changing any code.

## 4. Minimum OS

Windows 10 or 11, 64-bit. Both binaries import the UCRT API sets
(`api-ms-win-crt-*`), which are part of Windows 10/11. Windows 7/8 are not
supported.

## 5. Save data and the browser origin

The save lives in the browser profile the launcher creates for the game
(`%LOCALAPPDATA%\SpaceMiner`), because that is where a browser keeps
`localStorage`. Browser storage is keyed by origin, and an origin includes the
port — so the launcher deliberately binds a **stable port** (`47821`, scanning
`47821–47830` if that one is taken) instead of a random one. In practice the game
therefore always runs on the same origin and progress persists between sessions.

Consequences worth knowing:

* If all ten ports are occupied by unrelated software, the launcher falls back to
  an ephemeral port; that session starts from a fresh profile, and progress made
  in it stays with that port's origin. Restarting after freeing a port returns to
  the normal (persisted) profile.
* The save is not stored as a file in the game folder. Removing the
  `%LOCALAPPDATA%\SpaceMiner` folder (or uninstalling) clears it — the uninstaller
  warns about this explicitly.
* Saving is automatic (on docking, sector change, purchase and tab close), and the
  loader validates the payload: corrupt, truncated or unknown-version data starts a
  clean profile with a visible warning instead of breaking the game.

## 6. Rendering and audio

* Canvas 2D with GPU-accelerated browsers; identical visuals on integrated
  graphics. Measured software-rasteriser cost is ~6 ms/frame at 1600×900, so even
  a machine without GPU acceleration stays above 60 fps.
* Audio is fully synthesised (WebAudio). It starts only after the first user
  gesture, as required by browser autoplay policy — the title screen shows a
  "click to enable audio" hint.

## 7. Gameplay scope

Deliberately bounded, not stubbed:

* 5 sectors, 4 ships, 15 resources, 16 upgrades, 5 enemy types, 18 achievements,
  drone automation, station economy, combat, hazards, save/load.
* No multiplayer, no modding API, no controller/gamepad mapping (keyboard +
  mouse only), no localisation beyond English.
* Difficulty is tuned for a solo first playthrough (~3–5 hours to unlock the
  Void Frontier and the Leviathan hull).

## 8. Tooling

* Rebuilding the native binaries needs a Zig toolchain (it cross-compiles
  `x86_64-windows-gnu` without MSVC or a MinGW installation). `tools/win.js`
  finds `zig` on `PATH` or via `ZIG=/path/to/zig` and otherwise still produces the
  portable archive.
* Screenshot/icon generation needs `skia-canvas`; the game itself needs nothing.
