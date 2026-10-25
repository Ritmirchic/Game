# Known limitations

An honest list of what is missing, rough, or only partly verified. Nothing here is hidden in
the build notes.

## Verification gaps

* **The Windows executables have never been run.** They are cross-compiled with `zig c++`
  (clang + mingw-w64) in a Linux container; there is no Windows machine, no `wine` and no
  installable `mingw-w64` in this environment. The binaries are validated by PE inspection
  (signature, `x86_64`, GUI subsystem, icon and version resources, import table: only OS DLLs,
  static C++ runtime) and by code review of the host layer, but the first real launch is on the
  user's machine. The game logic, renderer, UI, save system, economy and audio mixer are all
  exercised by the 89-check headless suite, which shares every source file except
  `platform_win32.cpp`.
* **Audio has not been heard.** The synthesiser is verified only structurally (it renders a
  buffer of the right length in the right format, at 44.1 kHz stereo, well under real time).
  Volume balance, clipping and voice-level tuning are guesses until someone listens.
* Frame times were measured headless on a server CPU. A typical desktop will be faster, but
  very old hardware, or running at a large scaled window size, may drop below 30 fps
  (`GDI StretchDIBits` cost scales with the window, not the 1280×720 canvas).

## Platform

* **Windows 10 or newer only.** The binary links the Universal CRT
  (`api-ms-win-crt-*.dll`), which ships with Windows 10/11. Windows 7/8 would need a separate
  build against the legacy `msvcrt`.
* x86_64 only — no 32-bit or ARM build was produced.
* No exclusive fullscreen and no resolution selection: the game is a fixed 1280×720 canvas
  scaled to the window, with a borderless mode that maximises to the desktop. There is no
  refresh-rate/VSync control (the frame pacer is a 60 Hz software limiter).
* The game is single-threaded apart from the audio callback. Asset generation (~4 s) runs on the
  main thread; the loading bar advances between stages but does not animate during a stage.
* The installer is unsigned, so SmartScreen will warn on first run. It also has no uninstaller
  entry beyond the shortcut; deleting the folder removes everything.

## Gameplay

* No controller support, no key rebinding, and no localisation (English only, 3×5 pixel font,
  uppercase).
* Enemy AI is intentionally simple: approach, strafe, shoot, retreat when damaged. There is no
  pathfinding around rocks, and hostiles can occasionally clip a large asteroid.
* Drones use a simple pick-target/fly/mine/return state machine. They can get stuck on a rock
  the player is also mining, and they do not defend themselves.
* There is no pause menu in flight besides `ESC` → settings; the simulation keeps running while
  the settings screen is open in some situations (it is a modal screen, so in practice the ship
  drifts to a stop).
* Balance was tuned against the automated tests and the economy table, not against playtesters.
  Early sectors pay well; the Void Frontier is deliberately grindy.
* Victory is a single screen; there is no endless-mode scoring beyond continuing to fly.

## Rendering

* The rasteriser is CPU-only: no bloom, no real depth of field, no dynamic lighting. Glows are
  layered sprites, "fog" is a flat tint, and scanlines are a cheap overlay.
* Rotated sprites are sampled nearest-neighbour, so at large scales diagonal edges show
  stair-stepping (it fits the pixel-art look, but it is not filtered).
* The background is generated per sector into a 2048×1280 canvas; the camera samples it with
  baked-in parallax, so very fast travel can reveal the wrap seam on the dust lanes.
* Ring systems exist for two of the five planets only.

## Data

* Saves are not forward-compatible: a future version bump will reject (and back up) an older
  file rather than migrate it.
* There is one save slot and no cloud sync / export.
