# Known limitations

## Platform

- The supplied native build targets **Windows 10/11, 64-bit**. The Godot source can be opened on Linux, macOS and Windows, but no macOS or Linux executables are included.
- The executables are **not code-signed with a commercial certificate**. Windows SmartScreen may show an unknown-publisher warning. SHA-256 hashes are recorded in `BUILD_REPORT.md` for release verification.
- Keyboard and mouse are the supported input method. Touch controls and gamepad mappings are not included in this release.

## Design and rendering

- SPACE MINER is a **2D top-down game**, not a 3D flight simulator. Orthographic presentation, layered sprites, physics bodies, shaders and particles are intentional parts of the design.
- The compatibility renderer is used to support a broad range of Windows GPUs. Very old or virtualized GPUs without OpenGL 3.3 support may need updated drivers.
- Enemy and drone navigation uses direct steering plus physics collision response rather than navigation-mesh pathfinding. Dense moving fields can occasionally make a unit take a longer route.
- Gameplay is offline and single-player. There is no multiplayer, cloud save, workshop or online leaderboard.

## Saves and installation

- One active local profile is provided. Starting a new profile replaces the active profile; a backup is retained by the save system.
- The uninstaller removes program files and shortcuts but deliberately preserves the player's save directory.
- CI verifies silent installation and execution on a Windows runner. Individual hardware/driver combinations and antivirus policy decisions cannot be exhaustively tested.

## Content scope

- The five sectors are procedural replayable fields rather than a story campaign with cinematics.
- All art and audio are original procedural/vector-style assets. The project does not attempt photorealistic 3D rendering.
