DON'T LOOK BACK - building from source
======================================

Requirements
------------
- A C++17 compiler. On Windows: zig (https://ziglang.org) or MinGW-w64 g++.
- On Linux the same sources build a headless/Linux binary for testing.
- No third-party libraries are used. The engine, renderer, audio mixer, PNG codec,
  font baker and all content generators are original code in src/.

Layout
------
src/nf_core.*       math, colour, image, PNG codec, noise, IO
src/nf_render.*     software rasteriser, shadow map, volumetrics, post, 2D painter, fonts
src/nf_audio.*      mixer, 3D spatialisation, reverb, waveOut backend
src/nf_platform.*   Win32 window/input/present, headless fallback
src/dl_world.*      materials, apartment complex, props, doors, nav grid
src/dl_geom.cpp     primitives and the furniture/prop library
src/dl_sfx.cpp      procedural sound bank (40+ sounds, synthesised at start-up)
src/dl_game.*       player, physics, interaction, progression, save/load
src/dl_anom.cpp     the 30 anomalies: definitions, apply/reset, staging
src/dl_ai.cpp       the three entities: visibility, pathing, elevator
src/dl_ui.cpp       world render, HUD, menus, journal, settings, endings
src/dl_test.cpp     built-in self test (--selftest)
tools/procgen.py    59 procedural textures -> assets/tex
tools/fontgen.py    bakes the four bitmap fonts -> assets/fonts
tools/shots*.cpp    headless screenshot harness
tools/installer.cpp Win32 self-extracting installer (payload appended by tools/make_setup.py)
tools/bench.cpp     frame-time benchmark (per-pass timings, real gameplay camera)

Build (Windows, cross or native)
--------------------------------
    make win            # -> dist/DONT_LOOK_BACK.exe  (zig c++ -target x86_64-windows-gnu)
    make                # native Linux test binary
    ./build/DONT_LOOK_BACK_linux --selftest   # 114 automated checks

Performance tooling
-------------------
    make bench && ./build/bench 1280 720 150 corridor_main
        prints update/render percentiles and the final fps for a real gameplay
        camera at the given window size (rooms: corridor_main, apt47_kitchen,
        boiler, stairwell, basement_main ...)

The renderer keeps a frame-time budget (Renderer::setTargetFps / setQuality).
Its hot path is:
    * horizontal bands, one per worker thread, through a persistent thread
      pool (nf::Parallel) with per-band depth/HDR targets
    * fully incremental scanline traversal (no per-pixel barycentric setup)
    * per-triangle light culling, fast inverse-sqrt shading, texture sampling
      at a single mip level
    * a small multithreaded shadow-map rasteriser that only refreshes when the
      torch actually moves
    * shading and tonemapping at the internal render resolution, then a
      separable bilinear upscale into the window framebuffer

Regenerating content
--------------------
    python3 tools/procgen.py assets/tex      # ~90 s, writes all textures
    python3 tools/fontgen.py                 # writes assets/fonts/*.png + *.nfp

Packaging
---------
    make setup                               # builds dist/_setup_stub.exe, then
                                             # dist/DONT_LOOK_BACK_SETUP.exe with the
                                             # game payload appended (74 files)

Controls / gameplay are described in dist/README.txt.
