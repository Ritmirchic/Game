# SPACE MINER — Verification report

Date: 2026-10-25. Engine: Godot **4.7.1 stable**, official build `a13da4feb`.

## Engine selection
No Unreal Editor, UE installation, UE project toolchain or Windows UE packaging toolchain was present in the Linux sandbox. UE project creation/Blueprints/import/runtime/export therefore could not be exercised. Godot fallback was used. Godot project creation, GDScript parsing, original PNG/JPEG/font import, runtime startup and Web/Linux export were executed locally. Official Windows export templates and NSIS were used through GitHub Actions because the sandbox blocks GitHub's release-asset download host.

## Automated gameplay checks — 44 passed, 0 failed
`tests/suite.gd` loads the actual game scene and checks:

- Field count, player/refinery spawn safety, pairwise non-overlap of asteroid collision envelopes.
- Camera parenting/smoothing; physical keyboard thrust and CharacterBody2D movement.
- Asteroid health depletion, destruction, real ore drops, XP and physical collection.
- Strict cargo capacity; exact catalog-price selling and inventory clearing.
- Real cargo, mining-throughput, speed, acceleration, hull, shield and scanner upgrades.
- Maximum upgrade tier, insufficient funds and away-from-refinery rejection.
- Drone deployment, target search, navigation, mining, collection, return and unloading.
- Enemy navigation, projectile attacks, shield overflow, swept projectile collision, kill and bounty.
- Sector research, resource distribution, prerequisite enforcement and all five jumps.
- Cargo, upgrades, map, showroom, drones, achievements, settings, manual and refinery UI construction/pause state.
- Atomic versioned saving, complete round-trip, damaged JSON backup recovery, invalid schema and malicious inventory-type rejection.
- Bounded particles and enemy counts during repeated simulation.

A save-validation issue involving JSON numeric arrays was found by the suite and fixed before the passing release build. Spawn-placement overlap and unreachable corner-field issues were also corrected.

Recent local performance sample: **600 headless simulation updates in 52.2 ms** on the sandbox CPU. This is a simulation-only microbenchmark, **not rendered FPS**, a GPU test or a claim about Windows hardware.

## Runtime and rendering
- Source project: actual headless Godot runtime, successful `RUNTIME_SMOKE_OK` after three seconds.
- Packaged Linux x86_64 release: successful independent startup with embedded PCK, same smoke marker, exit code 0.
- Packaged Web release: actually launched in Chromium with SwiftShader WebGL 2. Actual mouse input mined a nearby asteroid; actual keyboard flight collected **3 units of iron**; inventory showed **36 CR** valuation. Sector chart opened through the M key. Screenshots inspected for menu, flight, mining, cargo and map. Browser console/page errors: **none** in the final browser interaction run.
- Web export is single-threaded for compatibility with embedded previews. It does not require cross-origin-isolated SharedArrayBuffer.
- Headless audio initialization is skipped; browser synthesized audio uses stream playback rather than unsupported sampled playback.

## Native Windows / installer verification
GitHub Actions workflow: `.github/workflows/space-miner-build.yml`.

Verified on a **Windows runner**, not simulated by Linux:
1. Extract portable ZIP.
2. Launch `SPACE_MINER.exe --headless --log-file ... -- --smoke`.
3. Require exit code 0 and `RUNTIME_SMOKE_OK` in the application log.
4. Run NSIS installer silently for the current user; require exit code 0 and installed executable.
5. Run the installed executable with the same headless smoke check.
6. Run the uninstaller silently and require removal of the installed executable.

Workflow run **40200911036** completed successfully for both release and Windows-smoke jobs. The final rebuild uses the same checks and includes packaging notices and the headless-audio cleanup.

## Known limitations / unverified claims
- **No manual native Windows graphical playthrough, GPU-driver matrix, SmartScreen signing/reputation test, or interactive installer-wizard inspection.** Native Windows tests above are headless/silent.
- No rendered-FPS benchmark, long-duration soak, gamepad/touch testing or broad balance playtest.
- This is a compact original 2D game, not a premium-budget Unreal 3D production. Visuals are deterministic procedural raster art, custom beam/exhaust rendering and bounded custom particle effects, not purchased assets or GPU-particle scenes.
- Enemy classes share a steering framework, and advanced hazards share periodic pulse logic despite distinct sector presentation and stats.
- No narrative campaign, multiplayer, localization, planetary terrain or infinite universe.
- Saves preserve progression and settings, not transient field state, exact ship position or individual drones' in-flight holds.
- The Windows release is unsigned; reputation prompts may appear. Save data intentionally survives uninstall.

## Reproduce
```sh
godot --headless --path space_miner --editor --import
XDG_DATA_HOME=/tmp/space-miner-test godot --headless --path space_miner --script res://tests/suite.gd
SPACE_MINER.exe --headless -- --smoke
```
The test suite uses real game functions and isolated temporary save data, rather than a duplicate JavaScript model of the mechanics.
