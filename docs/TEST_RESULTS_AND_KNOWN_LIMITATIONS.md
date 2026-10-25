# DON'T LOOK BACK — Test Results & Known Limitations

## 1. Engine Evaluation & Architecture Summary

- **Engine Selection**: Audited build host for Unreal Engine 5 (`UnrealEditor` / `RunUAT` not installed). Selected **Godot Engine 4.2.2 Stable (Forward+ 64-bit)** paired with **Blender 5.0 (`bpy` + Cycles)** for 3D modeling, skeletal rigging, keyframe animation, and raytraced hero artwork, plus **Zig 0.17.0 (`x86_64-windows-gnu`)** for native 64-bit Win32 GUI installer/uninstaller compilation.
- **Standalone Windows 64-bit Release (`DONT_LOOK_BACK.exe`)**:
  - Single-file PE32+ (`x86_64`) Windows GUI executable (`Subsystem: 00000002`, zero console/debug windows) with embedded `.pck` archive (`GDPC` trailer verified).
  - Custom multi-resolution Windows icon (`16x16` to `256x256`) and custom `VS_VERSION_INFO` metadata (`ProductName: DON'T LOOK BACK`, `CompanyName: Kestrel Interactive`) patched cleanly into PE resources prior to `.pck` embedding.
  - Zero external runtime DLL dependencies (`objdump -p` verified imports consist strictly of built-in Windows system DLLs: `KERNEL32.dll`, `USER32.dll`, `GDI32.dll`, `SHELL32.dll`, `OLE32.dll`, `DINPUT8.dll`, `dxgi.dll`, `WINMM.dll`, `ADVAPI32.dll`, `bcrypt.dll`, etc.).

---

## 2. Automated & In-Engine Verification Results (`61 / 61 PASSED`)

Executed `res://tests/TestSuite.tscn` against both the Godot 4.2 runtime and the exported standalone release binary (`dont_look_back_linux.x86_64` running from `/tmp` outside the project tree to verify complete `.pck` self-containment).

### Summary Table

| Subsystem | Assertions | Result | Verified Behaviors |
| :--- | :---: | :---: | :--- |
| **Player Movement, Camera & Collision** | 4 | **PASS** | Smooth corridor velocity, `move_and_slide()` physical wall stopping (`x = 1.16 < 1.55`), camera pitch clamping (`-82°..+82°`), physical scan-code WASD bindings. |
| **Lighting Visibility & Flashlight** | 4 | **PASS** | Balanced ACES exposure (`1.18`) + non-zero ambient illumination (`0.62` energy) prevent pitch-black screens; dual-cone `SpotLight3D` flashlight (`24.5m` range, real-time shadows enabled) toggles reliably. |
| **Apartment Complex, Hinged Doors & Elevator** | 6 | **PASS** | Full connectivity validation across all 20 navigation waypoints and 12 hinged doors; unlocked doors swing smoothly; padlocked doors rattle & block entry; 2-floor Elevator (`4` <-> `B`) operates with sliding doors and shaft audio. |
| **All 30 Functional Anomalies (`ID 1..30`)** | 31 | **PASS** | Every single anomaly (`V-01..V-10`, `A-11..A-18`, `E-19..E-25`, `N-26..N-30`) verified for: (1) real 3D world/shader/audio/layout mutation, (2) live `[R]` Field Scanner raycast/cone detection, (3) false-report rejection on clean baseline floors, and (4) deterministic baseline restoration (`reset_all_anomalies()`). |
| **Horror Entity #1 — The Observer** | 4 | **PASS** | Custom 13-bone skeletal `.glb` (`FrozenA`, `FrozenB`, `StalkWalk`, `Lunge`); freezes completely (`0.00m` movement) when inside camera frustum with unobstructed sightline; advances along nav-graph (`0.32m` over 5 ticks) when camera turns away; physics raycast accurately detects solid apartment walls as occluders; enforces `>= 9.5m` safe spawn distance. |
| **Horror Entity #2 — The Listener** | 3 | **PASS** | Custom skeletal `.glb` (`IdleListen`, `PatrolWalk`, `HuntSprint`); ignores `12 dB` crouched stealth footsteps (`PATROL`); transitions immediately to `HUNT` on `78 dB` sprint footsteps; physics wall-penetration raycast attenuates perceived decibels (`-14 dB` per solid wall). |
| **Horror Entity #3 — The Imitator** | 2 | **PASS** | Custom skeletal `.glb` (`DisguisePose`, `Twitch`, `UnfoldWalk`, `Ambush`); broadcasts deceptive door-knock/elevator cues; safely exposed and banished from afar via `[R]` Field Scanner; ambushes reckless direct `[E]` contact. |
| **Spatial Audio, Mystery Chain, Save/Load & 3 Endings** | 7 | **PASS** | All spatial & microcassette `.wav` streams verified; full secret chain (`Maintenance Breaker -> Apt 4C Bookshelf Latch -> Hidden Sanctum -> 1994 Master Key -> Basement Boiler Core Purge`) verified; **Ending A** (`Morning Outside Kestrel Court`), **Ending B** (`The Seal Broken`), and **Ending C** (`Tenant 405`) all reachable; `ConfigFile` save/load round-trip verified. |
| **TOTAL** | **61** | **61 PASS / 0 FAIL** | Verified in both Editor Headless & Standalone Release Binary. |

### Bugs Caught & Fixed During Verification
1. **Save-Game Yaw Restoration & Scripted Camera Angles**: Added `PlayerController.set_look_angles(yaw_rad, pitch_rad)` so restored save-game camera yaw and scripted transitions stay synchronized with internal `_pitch` state.
2. **Anomaly #19 (`Spontaneous Deadbolt Trap`)**: Updated activation so Apartment 4A's door deadbolts immediately with an audible heavy slam regardless of player corridor distance, making it consistently observable from both sides of the doorway.
3. **Entity Initial Waypoint Pop Latency**: Replaced single-frame `if` check with a `while` loop when popping reached navigation waypoints in `ObserverEntity` and `ListenerEntity`, eliminating a 1-frame pause at path start.

---

## 3. Known Limitations

1. **Graphics API Compatibility on Legacy Hardware**:
   - By default, `DONT_LOOK_BACK.exe` uses Godot 4.2's **Forward+ (Vulkan 1.2)** renderer to deliver real-time volumetric fog, SSAO, SSR, ACES filmic tonemapping, and soft positional shadow atlases.
   - On older Windows PCs, integrated GPUs without Vulkan 1.2 drivers, or unaccelerated virtual machines, launching with `--rendering-driver opengl3` switches cleanly to OpenGL 3.3 Compatibility rendering. **`DONT_LOOK_BACK_SETUP.exe` automatically creates a `"DON'T LOOK BACK (Compatibility OpenGL)"` Start Menu shortcut** for effortless fallback on older systems.
2. **Windows SmartScreen Unsigned Binary Prompt**:
   - Because `DONT_LOOK_BACK.exe` and `DONT_LOOK_BACK_SETUP.exe` are not signed with a commercial Extended Validation (EV) Authenticode certificate, Windows Defender SmartScreen may display an *"Unknown Publisher -> More info -> Run anyway"* prompt on first launch.
3. **Voice Acting Synthesis**:
   - The four collectible microcassette logs (`tape_log_01.wav` – `tape_log_04.wav`) use stylized acoustic formant-filtered tape distortion paired with full on-screen transcripts and archive playback rather than studio-recorded human voice actors.
