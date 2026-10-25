# DON'T LOOK BACK

**DON'T LOOK BACK** is a standalone 64-bit Windows psychological horror, anomaly-observation, exploration, and containment mystery game set inside **Kestrel Court Building 4** at 03:14 AM.

---

## Deliverables Included

| Deliverable | Description |
| :--- | :--- |
| **`DONT_LOOK_BACK.exe`** | Standalone Windows 64-bit Release Executable (single self-contained binary with embedded game data, custom icon, custom boot splash, custom window title, zero editor/debug UI, and zero external runtime DLL requirements). |
| **`DONT_LOOK_BACK_SETUP.exe`** | Native Windows 64-bit GUI Setup Wizard bundling `DONT_LOOK_BACK.exe`, `UNINSTALL.exe`, and `README.txt` with destination folder selection, Start Menu shortcuts (Standard + OpenGL Compatibility Mode), optional Desktop shortcut, and Windows Apps & Features uninstaller registration. |
| **`DONT_LOOK_BACK_SOURCE.zip`** | Complete, reproducible Godot 4.2 source project, Blender 5.0 3D asset generator, procedural PBR & spatial audio generators, custom shaders, C++/Zig Win32 installer source, and automated verification suite. |
| **`docs/TEST_RESULTS_AND_KNOWN_LIMITATIONS.md`** | Detailed verification report (61/61 automated in-engine assertions across movement, collision, doors, lighting, all 30 anomalies, all 3 horror entities, raycast visibility/occlusion, acoustic tracking, save/load, and all 3 endings) plus known limitations. |

---

## Controls & Inspection Protocol

- **Move**: `W`, `A`, `S`, `D` or `Arrow Keys` (physical scan-codes — works seamlessly regardless of active OS keyboard language layout)
- **Look**: `Mouse`
- **Sprint**: `Hold Shift` *(Warning: raises footstep volume to ~78 dB)*
- **Crouch / Stealth**: `Hold Ctrl` or `C` *(lowers footstep volume to ~12 dB to evade The Listener)*
- **Interact (Doors / Levers / Documents / Tapes / Elevator)**: `E`
- **Flashlight Toggle**: `F` or `Right Mouse Button`
- **Field Scanner (Log Anomaly / Expose Imitator)**: `R`
- **Open Anomaly Journal & Archive**: `J` or `Tab`
- **Pause / Settings / Save Shift**: `Esc`

### Core Gameplay Loop
1. Explore Floor 4 (Main Corridor, Apartments 4A / 4B / 4C, Cross Hall, Laundry Room, Storage Rooms, Maintenance Room, Stairwell, Elevator) and Basement Level B (Boiler Room, Pipe Vault, Emergency Exit).
2. Scan suspicious distortions from a safe distance using **`[R]`** to verify anomalies in real time and catalog them in your 30-Anomaly Compendium.
3. Return to the **South Threshold Security Desk**:
   - Pull the **GREEN CLEARANCE LEVER** *only* when the complex is completely baseline-clean (0 anomalies).
   - Pull the **RED CONTAINMENT LEVER** whenever one or more anomalies exist.
4. Reach **8/8 Verified Clearance Cycles** to unlock the Ground Emergency Exit (**Ending A**), uncover the **Hidden Architect's Sanctum** & **1994 Master Override Key** to purge the Basement Resonance Core (**True Ending B**), or cross the perceptual threshold (**Ending C**).

---

## Rebuilding From Source (`DONT_LOOK_BACK_SOURCE.zip`)

1. Open `project.godot` in **Godot Engine 4.2.x** (Forward+ renderer).
2. Optional asset regeneration:
   - `python3 tools/generate_textures_and_ui.py` (PBR albedo/normal/roughness textures, custom vector UI icons, `.ico`, boot splash)
   - `python3 tools/generate_audio.py` (41 spatial horror `.wav` streams & distorted microcassette voice logs)
   - `python3 tools/build_models_blender.py` (requires Python `bpy >= 4.0/5.0`; builds all 36 `.glb` architectural/furniture/rigged entity models and Cycles hero renders)
3. Run automated verification suite headless:
   ```bash
   godot4 --headless --path . res://tests/TestSuite.tscn
   ```
4. Export Windows 64-bit Release via **Project -> Export -> Windows Desktop (`DONT_LOOK_BACK.exe`)**.
