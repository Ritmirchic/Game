# Test report — THE LAST NIGHT

Everything below was executed in the build container. Where something could
**not** be executed it is listed under *Not tested* rather than assumed to work.

Environment: Linux x86-64 container, gcc 12.2, Zig 0.17.0 (cross-linker +
`zig rc`), Python 3.11 with Pillow/NumPy. No Windows host, no Wine and no VM
were available, so the Windows executable was verified as a PE image only.

---

## 1. Automated visual tour (executed)

```sh
gcc -O2 -std=c11 -o tln src/*.c engine/src/core.c engine/src/render.c \
  engine/src/threads.c engine/src/inflate.c engine/src/png.c engine/src/fileio.c \
  engine/src/png_write.c engine/src/geom.c engine/src/anim.c engine/src/audio.c \
  engine/platform/plat_headless.c -lm
./tln --headless --tour --shots shots/tour
```

Result: **exit 0, 37/37 captures written (800×450), ~9 s wall clock**, log
`the_last_night.log` has no `[ERR]`/`[WRN]` lines for the tour.

Captured states: 14 interior rooms → exterior (parking lot, cars, storefront) →
night with 10 mixed zombies → all 7 weapons (firing, ADS, sniper scope,
crossbow) → flashlight off → wooden and metal barricades → low-health HUD →
supply desk → settings → controls → LAN lobby → pause → main menu → death →
victory → night-10 boss.

Level figures printed at init: `41 meshes, 156 colliders, 65 lights, 11 barricade slots`,
`68 texture sets loaded`, character model sample `21 parts, 23 bones`.

Visual review found and fixed (each verified by re-capturing the shot):

| Finding | Fix |
|---|---|
| camera looked *behind* the player (characters were culled, world upside-down feel) | view-space yaw = gameplay yaw + π |
| weapon view-model was below the screen / invisible | pass the eye position (`cam.pos`), not the feet position, and re-tuned grip offsets |
| silhouettes had black holes, zombies looked washed-out white | tinted skin/cloth material multipliers, material tints corrected |
| flashlight blew out as a flat white disc, blinding the frame | range/intensity/cone re-balanced (0.46–0.62, 26–31°) |
| button labels drawn at ~10× scale, giant letters over every menu | label scale derived from the font's line height |
| menu logo overlapped the buttons, subtitle drawn twice | new logo art, logo placed above the list, duplicate label removed |
| screenshots silently missing when the target dir did not exist | `--tour` creates its capture directory |
| texture alias/moiré on floors and shelves | per-triangle mip selection + generated mip chain |

## 2. LAN replication test (executed)

```sh
./tools/nettest.sh ./tln      # starts "nettest host" and "nettest client 127.0.0.1"
```

Both processes are headless builds of the real game, using the real UDP socket
code in `src/net.c` (no simulation of networking).

Observed, from the two logs:

```
host:   net: peer 0 joined
host:   nettest t= 1.05s zombies=0 peers=1 ... status=HOSTING - 1 CLIENT(S) CONNECTED
client: net: welcomed as player 1
client: nettest t= 2.50s zombies=1 ... status=CONNECTED AS PLAYER 2
host:   nettest: done zombies=5 players=2 peers=1 running=1
client: nettest: done zombies=5 players=2 peers=0 running=0
client: nettest: client local player=1 pos=(1.20,13.00) hp=72 worst-reconcile=1.61 m
```

What that demonstrates:

* HELLO/WELCOME handshake over real datagrams (client retries until answered —
  a dropped first packet no longer strands the client, which the test caught).
* The host spawns and owns the wave; the client's zombie array is rebuilt purely
  from snapshots (counts track 1→5 in lock-step).
* The client's local player takes damage from host-simulated zombies
  (`hp=72` on the client, never simulated locally) and its position reconciles
  against the authoritative state (worst correction 1.61 m over 12 s).
* Host disconnect (`PKT_BYE`) correctly drops the client out of the session.

Limitation of this test: only **two** processes were run, so a full 4-player
session and a multi-machine LAN were not exercised. Same-machine loopback also
means packet loss/reordering behaviour is optimistic.

## 3. AddressSanitizer / UndefinedBehaviorSanitizer (executed)

```sh
gcc -g -O1 -std=c11 -fsanitize=address,undefined -o tlnasan ...
ASAN_OPTIONS=detect_leaks=0 ./tlnasan --headless --tour --shots /tmp/tourasan
ASAN_OPTIONS=detect_leaks=0 ./tools/nettest.sh ./tlnasan
```

Result: **exit 0, no ASan report, no UBSan runtime error** in either run.

Leak checking (`detect_leaks=1`) went from
`2,130,608 bytes in 440 allocations` to **no leaks reported** after fixing:

* `clip_make()` animation clips were never released (`clip_free()` added, and
  `char_free()` now frees each distinct clip).
* `place_model()` in the level builder copied triangles out of one-off prop
  models (doors, posters, directory signs) and leaked the source models; those
  call sites now use `place_model_take()`.

Earlier sanitizer runs also caught and fixed: a stack buffer overflow in
`clip_key`, a NULL model matrix in `rd_draw_mesh`, and a bad `free()` in
`assets_free`.

## 4. Windows binary verification (executed, without running it)

```sh
./tools/build_win.sh
```

* `dist/win/THE_LAST_NIGHT.exe`, 720,896 bytes, PE32+ x86-64.
* `OptionalHeader.Subsystem = 2` (GUI) — no console window is created.
* Section table contains `.rsrc` (60,424 bytes) with `RT_ICON`,
  `RT_GROUP_ICON` and a `VS_VERSIONINFO` block (compiled by `zig rc`).
* Static imports: `winmm`, `ws2_32`, `gdi32`, `user32`, `kernel32`.
* `WinMain` parses the command line (quoted arguments included) and forwards to
  the same entry point used by the Linux build, so `--tour`/`--nettest` work on
  Windows too (a headless path exists in `plat_win32.c`).

## Not tested (do not assume these work)

* **Executing the Windows build.** No Windows machine, Wine or VM was
  available. Window creation, DIB blit, raw mouse input and `waveOut` audio on
  Windows are compiled but unrun.
* **Interactive play.** Every frame in this report was produced by the automated
  harness with scripted input; no human play session happened.
* **A complete 10-night run.** The tour jumps between nights and states; it does
  not play night 1 → 10 back to back.
* **4-player co-op and real-LAN latency.**
* **Installer.** `THE_LAST_NIGHT_SETUP.exe` was not produced (see README).
* **Audio output.** The mixer runs in every test (735 frames per tick) and is
  leak/UB clean, but nothing was listened to.

## Known limitations

* Menus are laid out for the 1280×720-class internal resolution and scale with
  the window; at very small window sizes some labels get tight.
* Client-side prediction is a simple velocity model, so the local player can be
  corrected by up to ~1.6 m when walking into remote obstacles.
* The zone/navmesh graph is rebuilt on level load; very large modded levels would
  increase load time noticeably.
* Boss audio cues rely on the shared positional system; there is no dedicated
  music track (the score is synthesised ambience).
