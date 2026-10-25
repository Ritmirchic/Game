# Building THE LAST NIGHT

## Toolchain

| Piece | Version used |
|---|---|
| C compiler (Linux dev build) | gcc 12 (`-std=c11 -O2`) |
| Windows cross-compiler | Zig 0.17.0 (`pip install ziglang`) — mingw-w64 headers/libc + lld |
| Resource compiler | `zig rc` (resinator, drop-in `windres`/`rc` replacement) |
| Asset generation | Python 3.11 + Pillow + NumPy |

No engine, SDK or third-party runtime is required: the renderer, animation,
audio mixer, PNG codec and networking are all implemented in `engine/` and
`src/`.

## Windows release (the shipped .exe)

```sh
pip install ziglang            # provides zig 0.17 with the mingw-w64 toolchain
./tools/build_win.sh           # -> dist/win/THE_LAST_NIGHT.exe + assets/
```

`tools/build_win.sh` does three things:

1. `zig rc /:output-format coff /:target x86_64 -fo build/win_res.obj src/win/the_last_night.rc`
   — compiles the icon (`assets/ui/icon.ico`) and the version resource.
2. `zig cc -target x86_64-windows-gnu -O2 -std=c11 -DTLN_GUI -Wl,--subsystem,windows ...`
   — builds a **GUI-subsystem** PE32+ executable (no console window) with
   `WinMain` calling the normal entry point. Linked against
   `winmm ws2_32 gdi32 user32 shell32 ole32`.
3. Stages `assets/` next to the exe.

Verify the result:

```sh
python3 - <<'PY'
import struct
d=open('dist/win/THE_LAST_NIGHT.exe','rb').read()
pe=struct.unpack_from('<I',d,0x3c)[0]
print('subsystem', struct.unpack_from('<H',d,pe+24+68)[0])  # 2 = GUI
PY
```

## Linux development build

```sh
gcc -O2 -std=c11 -o THE_LAST_NIGHT src/*.c engine/src/core.c engine/src/render.c \
  engine/src/threads.c engine/src/inflate.c engine/src/png.c engine/src/fileio.c \
  engine/src/png_write.c engine/src/geom.c engine/src/anim.c engine/src/audio.c \
  engine/platform/plat_headless.c -lm
```

Sanitiser build used for triage:

```sh
gcc -g -O1 -std=c11 -fsanitize=address,undefined -o tln_asan src/*.c \
  engine/src/core.c engine/src/render.c engine/src/threads.c engine/src/inflate.c \
  engine/src/png.c engine/src/fileio.c engine/src/png_write.c engine/src/geom.c \
  engine/src/anim.c engine/src/audio.c engine/platform/plat_headless.c -lm
ASAN_OPTIONS=detect_leaks=0 ./tln_asan --headless --tour --shots /tmp/tour
```

## Regenerating assets

```sh
python3 tools/gen_assets.py            # textures, normal/roughness maps, UI art, fonts
python3 tools/tex_char.py              # character texture sets
python3 tools/tex_props.py             # prop/weapon texture sets
python3 tools/tex_world.py             # world texture sets
```

The `.ico` and the logo are produced by the snippets documented in
`docs/CREDITS.md` (Pillow, fully procedural).

## Runtime flags

| Flag | Meaning |
|---|---|
| `--headless` | no window, fixed 60 Hz simulation |
| `--tour` | run the automated visual tour, writes 37 PNGs |
| `--nettest host` / `--nettest client <ip>` | LAN replication test |
| `--shots DIR` | capture directory (default `shots`) |
| `--night N` | start directly at night N |
| `--frames N` | stop after N frames |
