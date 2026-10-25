# Credits and asset provenance — THE LAST NIGHT

## Code

All engine and game code in `engine/` and `src/` was written for this project
(C11, no third-party runtime or library dependencies beyond the platform SDK).
The PNG decoder (`engine/src/inflate.c`, `engine/src/png.c`), PNG writer,
software renderer, animation system, audio mixer and UDP replication layer are
original implementations.

## Art

Every texture, UI image, logo, icon and 3D model is **generated procedurally by
scripts in `tools/`** — there are no downloaded or scanned image assets:

| Asset group | Generator |
|---|---|
| world/ material albedo, normal and roughness maps (`assets/tex/`) | `tools/gen_assets.py`, `tools/tex_world.py`, `tools/tex_props.py`, `tools/tex_char.py` |
| UI art (`assets/ui/`: logo, panel, icons, crosshair, noise, vignette, blood, flash, smoke, rain) | `tools/gen_assets.py` + `tools/tex_ui.py` |
| font atlases (`assets/fonts/*.png` + `*.txt` metrics) | `tools/tex_ui.py` (`font_atlas`) |
| application icon (`assets/ui/icon.ico`, `icon_256.png`) | generated with Pillow (see below) |
| all character, weapon and prop meshes | generated in C at load time (`src/models_char.c`, `src/models_weapons.c`, `src/models_props.c`) |

The icon is a Pillow drawing (rounded plate, moon, storefront, barricade boards)
rendered at 512 px and downsampled into a multi-size `.ico`
(16/24/32/48/64/128/256). No third-party icon set is used.

### Font licence (required notice)

The font atlases are rasterised from **DejaVu Sans / DejaVu Sans Bold /
DejaVu Sans Mono Bold**. DejaVu fonts are derived from Bitstream Vera and are
distributed under the permissive Bitstream Vera licence; the full text is
included at `assets/fonts/LICENSE_DejaVu.txt` and is kept in every build:

```
Copyright (c) 2003 by Bitstream, Inc. All Rights Reserved.
Bitstream Vera is a trademark of Bitstream, Inc.
DejaVu changes are in public domain.
Permission is hereby granted, free of charge, to any person obtaining a copy of
the fonts ... to reproduce and distribute the Font Software ...
```

The game embeds only rasterised glyph atlases of these typefaces, generated at
build time.

## Audio

All sound is synthesised at runtime by `sounds_build()` (`src/sounds.c`) and the
mixer in `engine/src/audio.c` — noise bursts, filtered sweeps, FM tones and
procedural reverb tails. **No sampled or downloaded audio is included**, so there
are no third-party audio licences to clear.

## Engine choice

Unreal Engine 5 and Godot 4 could not be obtained in the build environment, so
the game runs on the original C engine in this repository and is cross-compiled
with Zig/LLVM (`zig cc`, mingw-w64 runtime) and `zig rc`. Zig, LLVM and
mingw-w64 are used as toolchain only; no Zig or mingw runtime code is statically
linked into anything that would change the licence of this game's sources.

## Trademarks

"Save-N-Stock", "The Storekeeper", "The Butcher" and all in-game brands are
fictional and created for this project. No real retailer, product or person is
depicted.
