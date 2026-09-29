# One Hour

A compact, low-resource skirmish RTS in the spirit of Command & Conquer: Generals – Zero Hour.
One map, two armies, you against one to three AI opponents. No campaign, no tutorial, no multiplayer.

    ./run.sh              # builds (first run only) and starts the game
    ./run.sh --scale 2    # same 1024x640 game, drawn in a bigger window
    ./run.sh --software   # software renderer instead of the GPU

Requirements: g++, make, and the SDL2 runtime (`libsdl2-2.0-0`). SDL2 headers are vendored
under `third_party/`, so no `-dev` package is needed. Ubuntu 24.04+ works out of the box.

## Armies

**Cyber Army** — electric and optical. Pulse rifles, laser troopers, shock troopers, Photon Tanks,
Volt Walkers (arc lightning, anti-infantry/anti-air), Railgun Tanks (long range, pierces a line),
Wraith Drones, Laser Turrets and Patriot Batteries. Tech structure: Data Center. Power: **EMP Strike**.

**Clanker Army** — diesel and gunpowder. Riflemen, RPG troopers, heavy gunners, Brute Tanks,
Gatling Tanks, Rocket Launchers (artillery), Vulture Gunships, Gun Nests and Rocket Batteries.
Tech structure: Arms Lab. Power: **Shell Storm**.

Both sides: Command Core/Post trains dozers, power plants provide 10 power, Supply Hub/Depot builds
haulers that carry $300 per trip from supply piles. Running out of power halves production and
switches powered defenses off, exactly the pressure Zero Hour puts on you.

## Controls

| Input | Action |
| --- | --- |
| Left click / drag | select (double-click: every unit of that type on screen, Tab: army on screen) |
| Right click | move, attack, gather, repair/continue construction, set rally point (also on the minimap) |
| A + click | attack-move, S: stop |
| Ctrl+0–9 / 0–9 | assign / recall control group (Alt+#: jump to it) |
| Arrows, screen edge, middle drag, wheel | scroll, Home: your base |
| Dozer selected | build menu with hotkeys; click the ground to place, Shift to place several |
| Structure selected | train units (hotkeys shown), Del sells |
| X | special power from the Command Core/Post once the tech structure exists |
| Space | pause, + / - game speed, F1 help, F2 mute, F11 fullscreen, F12 screenshot, Esc (twice) menu |

## AI

Each opponent runs a utility-driven commander: economy and power management, prerequisite-aware
base layout, defenses placed toward the enemy, threat assessment, army composition weighted against
the observed enemy mix and continually re-weighted by which of its unit types are actually earning
kills per credit, wave attacks that launch only when they outweigh the local defense, regroup and
reinforcement waves, harassment of haulers, aircraft strikes and special powers aimed at the
densest cluster. Difficulty changes reaction time, aggression and starting cash (Brutal cheats a
little, like Zero Hour's).

## Engineering notes

- C++17, SDL2 only. Sprites, terrain, the font and every sound effect are generated at startup;
  there are no asset files.
- Fixed 20 Hz simulation with interpolated 60 fps rendering; the whole game uses a few percent of
  one core and ~90 MB.
- `./build/onehour --selftest 900` runs an AI-only game headless and checks invariants;
  `--scenario` drives every structure/unit/order for both factions; `--uitest` feeds synthetic
  mouse and keyboard events through the real input code; `--shot out.bmp --ticks N` renders a frame
  headless; `--soundcheck` prints statistics for the synthesized sounds.
