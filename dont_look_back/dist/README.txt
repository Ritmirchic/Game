DON'T LOOK BACK
an observation of the third floor

A psychological horror game about noticing what is wrong.

------------------------------------------------------------
HOW TO RUN
------------------------------------------------------------
Installer:  run DONT_LOOK_BACK_SETUP.exe (no administrator rights needed).
            It installs to %LOCALAPPDATA%\Programs\DontLookBack, adds a
            desktop and Start-menu shortcut and starts the game.

Portable:   run DONT_LOOK_BACK.exe from this folder. The "assets" folder
            must stay next to the executable.

------------------------------------------------------------
CONTROLS
------------------------------------------------------------
W A S D          walk
SHIFT            run (drains stamina)
MOUSE            look
F                flashlight
E                use / open / read / pick up
R                report an anomaly
TAB              journal
ESC              pause / settings

------------------------------------------------------------
HOW TO PLAY
------------------------------------------------------------
1. Walk the third floor of the building. Doors open with E.
2. Every so often something in the world is wrong: a painting changed,
   a chair moved, footsteps follow you, a door appeared in a blank wall.
3. Stand near the thing that is wrong and press R to report it.
   - A correct report is recorded in the journal and moves you forward.
   - A wrong report costs you. Five wrong reports and the building
     decides you belong to it.
4. Report twelve anomalies to be given the maintenance key.
   Three endings are reachable:
     ESCAPE       - open the fire door on the north landing with the key.
     DESCENT      - restore the boiler, pull the brass valve in the
                    basement, then enter the warm brick in the coal store.
     ASSIMILATION - let the building win.

------------------------------------------------------------
PERFORMANCE
------------------------------------------------------------
The game is drawn in software (the NIGHTFALL engine) - it does not use
your GPU - and the frame is split across all CPU cores.

Settings that matter:
  Render quality    LOW / MEDIUM / HIGH  (shadow map size, light shafts)
  Frame rate target 30 / 60 / 120 FPS
  Brightness, grain, volumes, fullscreen

The internal render resolution adapts automatically: the game measures the
frame time and moves the resolution between 32% and 100% to hold the frame
rate target. If your machine is faster the picture sharpens by itself; if
it is slower the picture softens instead of the game turning into a slide
show. The interface is always drawn at native window resolution.

------------------------------------------------------------
NOTES
------------------------------------------------------------
- All geometry, textures, fonts and audio are generated procedurally;
  the game ships no third-party assets.
- Save data: %APPDATA%\DontLookBack\profile.sav
