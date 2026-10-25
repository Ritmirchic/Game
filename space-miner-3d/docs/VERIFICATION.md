# Verified release — 2026-10-25

**SPACE MINER: BUILD & CONQUER 0.1.0**, Godot 4.4.1, Windows x64.

Status: early playable development release, not a finished production game. See [STATUS.md](STATUS.md).

## Actual uploaded downloads

- [Windows portable ZIP](https://github.com/Ritmirchic/Game/releases/download/build-conquer-v0.1.0/SPACE_MINER_WINDOWS.zip) — 34,672,156 bytes
- [NSIS installer](https://github.com/Ritmirchic/Game/releases/download/build-conquer-v0.1.0/SPACE_MINER_SETUP.exe) — 33,617,357 bytes
- [Complete source ZIP](https://github.com/Ritmirchic/Game/releases/download/build-conquer-v0.1.0/SPACE_MINER_SOURCE.zip) — 1,844,087 bytes
- [Release report](https://github.com/Ritmirchic/Game/releases/download/build-conquer-v0.1.0/RELEASE_REPORT.md)
- [Checksums](https://github.com/Ritmirchic/Game/releases/download/build-conquer-v0.1.0/SHA256SUMS.txt)

Windows archive SHA-256:

```
3fb4bbbdc475b993956fe1678b1ccfe1fd9cd94d019101f9159752ddd21c777f
```

The GitHub release API independently reports the same SHA-256 digest and `uploaded` status for the archive. Source commit: `b473da2f98a72f8c055de258c83f375b5680c35c`. No old browser-based release assets were reused.

## Tests actually executed

[Successful Windows workflow](https://github.com/Ritmirchic/Game/actions/runs/40205997361):

- Official Godot editor source import: passed.
- Logic suite on Windows: **137 passed / 0 failed**.
- Same suite in the actual exported Windows executable: **137 passed / 0 failed**.
- Source and exported scene integration: hangar construction; eight menu pages; actual thrust physics; mining ray damage; projectile ray collision; physical ore collection; cargo sale; save/load; card reveal — passed.
- PE x86-64 signature/architecture check: passed.
- NSIS installer compilation and silent installation: passed.
- Installed executable scene integration: passed.
- Silent uninstaller process execution: passed. Complete post-uninstall filesystem cleanup was not asserted.

Raw logs: [runs/40205997361/](runs/40205997361/).

[Successful visual workflow](https://github.com/Ritmirchic/Game/actions/runs/40205997384): real OpenGL viewport captures through Mesa llvmpipe/Xvfb. Hangar, crew collection, expedition and card reveal images are in [visual/](visual/). Screenshots were inspected; module armor gaps and reveal-card sizing were corrected. This is not evidence of interactive Windows rendering or hardware performance validation.

## Not verified

No human interactive playthrough, Windows graphical-driver matrix, extended gameplay balance/soak, signed-binary trust, or production-readiness certification. Local official-engine downloads were blocked by the sandbox network policy; local source compilation was stopped once official-engine CI testing was operational. All engine/runtime test results above came from the identified GitHub runners, not a completed local engine build.
