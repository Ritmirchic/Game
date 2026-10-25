# Build instructions

Two builds are supported from the same sources:

* **Windows x86_64** — the shipping build (`platform_win32.cpp`).
* **Headless** (Linux/macOS/Windows console) — used for tests and screenshot generation
  (`platform_headless.cpp`).

No third-party libraries are required in either case.

---

## 1. Headless build (tests, screenshots, asset sheet)

```bash
cd space_miner
g++ -O2 -std=c++17 -I src -o build/smtests tools/tests.cpp \
    src/core.cpp src/gfx.cpp src/texture.cpp src/art.cpp src/content.cpp \
    src/game_core.cpp src/game_update.cpp src/game_render.cpp src/ui.cpp \
    src/audio.cpp src/platform_headless.cpp

./build/smtests --selftest            # 89 functional checks + a frame-time measurement
./build/smtests --shots shots/        # writes 18 PNGs + the asset sheet into shots/
```

Add `-fsanitize=address,undefined` for a memory-safety run (needs
`ASAN_OPTIONS=verify_asan_link_order=0` on Debian/Ubuntu because of the static libstdc++ link
order). The suite passes clean under ASan/UBSan.

Optional: the profiler is compiled in with `-DSM_PROFILE`; build a harness with
`src/game_render.cpp` and call `profFlush()` to print per-section frame timings
(asteroids / station / loot / drones / enemies / player / projectiles / particles).

---

## 2. Windows build (cross-compile with zig, no MSVC needed)

`zig c++` ships a full clang + mingw-w64 sysroot, so a Windows binary can be produced on Linux.
The icon must be compiled into a COFF resource object first.

```bash
export PATH=/path/to/zig:$PATH
cd space_miner

# --- step 1: native helper that generates the icon, .rc file and installer payload
g++ -O2 -std=c++17 -I src -o build/gen_assets tools/gen_assets.cpp \
    src/core.cpp src/gfx.cpp src/texture.cpp src/art.cpp src/content.cpp \
    src/game_core.cpp src/game_update.cpp src/game_render.cpp src/ui.cpp \
    src/audio.cpp src/platform_headless.cpp
./build/gen_assets build          # -> build/space_miner.ico, build/space_miner.rc
                                  #    build/icon_preview.png, build/setup_payload.h/.cpp

# --- step 2: compile the resource script (icon + version info) into an object
#     (zig's rc resolves relative paths from the .rc file, so build from build/)
zig rc /fo build/rc_resources.o build/space_miner.rc

# --- step 3: the game itself
zig c++ -target x86_64-windows-gnu -std=c++17 -I src -I build -O2 -static \
        -Wl,--subsystem,windows \
        -o release/SPACE_MINER.exe \
        src/main.cpp src/core.cpp src/gfx.cpp src/texture.cpp src/art.cpp src/content.cpp \
        src/game_core.cpp src/game_update.cpp src/game_render.cpp src/ui.cpp src/audio.cpp \
        src/platform_win32.cpp build/rc_resources.o \
        -lgdi32 -luser32 -lwinmm -lshell32 -lole32

# --- step 4: re-run the payload generator so the installer embeds *this* exe + README
./build/gen_assets build

# --- step 5: the installer (icon + version resource for the setup program)
zig rc /fo build/setup_res.o build/setup.rc        # see note below
zig c++ -target x86_64-windows-gnu -std=c++17 -I build -O2 -static \
        -Wl,--subsystem,windows \
        -o release/SPACE_MINER_SETUP.exe \
        tools/installer_win32.cpp build/setup_payload.cpp build/setup_res.o \
        -lcomctl32 -lole32 -lshell32 -luser32 -lgdi32 -ladvapi32
```

`tools/build_windows.sh` runs all of the above; `build/setup.rc` is the same as
`build/space_miner.rc` with the description changed to "Space Miner Installer".

### Notes and gotchas

* `-static` is required; without it the binary would need `libstdc++-6.dll`.
* The binary imports the Windows *Universal CRT* (`api-ms-win-crt-*.dll`). Those are part of
  Windows 10/11 — the reason the game asks for Windows 10+.
* `zig rc` wants `/fo <out.o> <in.rc>` in that order. Its preprocessor resolves relative include
  and icon paths **relative to the .rc file**, so the .rc is generated with a path that works from
  `build/`.
* `.pdb` files that zig emits next to the executables are build artefacts and can be deleted.

---

## 3. Verifying a Windows build without Windows

```bash
python3 - <<'PY'
import struct
d = open('release/SPACE_MINER.exe','rb').read()
off = struct.unpack_from('<I', d, 0x3c)[0]
print('signature  ', d[off:off+4])
print('machine    ', hex(struct.unpack_from('<H', d, off+4)[0]), '(0x8664 = x86_64)')
print('subsystem  ', struct.unpack_from('<H', d, off+0x5c)[0], '(2 = GUI, no console window)')
print('has icon   ', b'\x00\x00\x01\x00\x06\x00' in d)   # 6-entry .ico
print('size       ', len(d))
PY
```

The test suite also confirms that the installer payload is byte-identical to
`release/SPACE_MINER.exe`.

---

## 4. Regenerating the icon / installer payload

`tools/gen_assets.cpp` draws the icon with the game's own `Canvas` code (a hexagonal hull with a
cutting beam) and writes:

* `space_miner.ico` — 16, 24, 32, 48, 64, 128, 256 px, 32-bit BGRA DIB entries,
* `icon_preview.png` — a contact sheet for eyeballing the result,
* `space_miner.rc` — icon + VERSIONINFO,
* `setup_payload.h/.cpp` — the game exe and `release/README.txt` as byte arrays.

Run it after every Windows build, otherwise the installer will embed an older exe.
