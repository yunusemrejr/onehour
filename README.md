# One Hour

A compact, low-resource skirmish RTS in the spirit of Command & Conquer: Generals – Zero Hour.
One map, two armies, you against one to three AI opponents. No campaign, no tutorial, no multiplayer.

    ./run.sh              # builds (first run only) and starts the game
    ./run.sh --scale 2    # force a UI scale (default: automatic)
    ./run.sh --size 1600x900   # initial window size
    ./run.sh --software   # software renderer instead of the GPU

Requirements: g++, make, and the SDL2 runtime (`libsdl2-2.0-0`). SDL2 headers are vendored
under `third_party/`, so no `-dev` package is needed. Ubuntu 24.04+ works out of the box.

## Armies

**Cyber Army** — electric and optical. Pulse rifles, laser troopers, shock troopers, Photon Tanks,
Volt Walkers (arc lightning, anti-infantry/anti-air), Railgun Tanks (long range, pierces a line),
Wraith Drones, Laser Turrets and Patriot Batteries. Tech structure: Data Center (EMP Strike, Orbital Scan,
Overclock Program → Ion Lancer sniper and Aegis Titan super-heavy walker).

**Clanker Army** — diesel and gunpowder. Riflemen, RPG troopers, heavy gunners, Brute Tanks,
Gatling Tanks, Rocket Launchers (artillery), Vulture Gunships, Gun Nests and Rocket Batteries.
Tech structure: Arms Lab (Shell Storm, Recon Flight, Heavy Ordnance → Grenadier and Behemoth).

**Tech structure powers** (select the Data Center / Arms Lab):

| Button | Key | Effect |
| --- | --- | --- |
| Strike (EMP Strike / Shell Storm) | X | call a strike on a circle of your choice (3 min cooldown); also on the Command Core/Post |
| Scan (Orbital Scan / Recon Flight) | V | the whole map and every enemy on it is visible for 30 s (3.5 min cooldown) |
| Advanced Program | R | $2500, 45 s (half speed on low power): unlocks the army's two elite units for good |

Team colours: every unit type has its own tone (graphite, cobalt, ceramic and teal for Cyber; olive, sand, brick and iron for
Clanker), a small jewel-like dot in the owner's colour marks each unit, and every finished structure flies its owner's own flag
(rectangle+chevron, swallowtail+roundel, pennant, burgee+cross) from a mast on the roof.

Aircraft carry 12 (Wraith) / 16 (Vulture) rounds, reload in under half a second per round on the pad, and hit harder and take more
punishment than before.

Both sides: Command Core/Post trains dozers, power plants provide 10 power, Supply Hub/Depot builds
haulers that carry $300 per trip from supply piles. Running out of power halves production and
switches powered defenses off, exactly the pressure Zero Hour puts on you.

## Controls

| Input | Action |
| --- | --- |
| Left click / drag | select (double-click: every unit of that type on screen, Tab: army on screen) |
| Right click | move, attack, gather, repair/continue construction, set rally point (also on the minimap) |
| A + click | attack-move, S: stop |
| G, then click or drag | **guard an area**: fighters hold spread-out posts in the circle and engage what enters it (chasing only while the target stays inside), aircraft patrol it and return to rearm; **gather in an area**: haulers search the circle for supply piles and work them until it is empty. Any move/stop order cancels it. |
| Select a turret / battery, or hover any defense | shows the area it covers (ground, air, or both; grey when unpowered; enemy defenses in red). The range also follows the cursor while you place a defense |
| Ctrl+0–9 / 0–9 | assign / recall control group (Alt+#: jump to it) |
| Arrows, screen edge, middle drag, wheel | scroll, Home: your base |
| Dozer selected | build menu with hotkeys; click the ground to place, Shift to place several |
| Structure selected | train units (hotkeys shown), Del sells |
| X | strike power (from the tech structure, or the Command Core/Post once it exists); V map scan, R Advanced Program with a tech structure selected |
| Space | pause, + / - game speed (x0.5 to x4), F1 help, F2 mute, F11 fullscreen, F12 screenshot |
| Esc | cancels a pending order, otherwise opens the pause menu: resume, game speed slider, sound, help, restart, surrender, quit to menu / desktop |
| K | with a Nuke Ramp selected: pick a target for a tactical nuke |

## AI

Each opponent runs a utility-driven commander: economy and power management, prerequisite-aware
base layout, defenses placed toward the enemy, threat assessment, army composition weighted against
the observed enemy mix and continually re-weighted by which of its unit types are actually earning
kills per credit, wave attacks that launch only when they outweigh the local defense, regroup and
reinforcement waves, harassment of haulers, aircraft strikes and special powers aimed at the
densest cluster. Two parts are learned online (`src/brain.cpp`): a logistic model that predicts whether
an attack wave will trade favourably (launch/hold decision), and per-unit-type regressions of value destroyed
per credit given the enemy's infantry/vehicle/air mix (production choice). Idle fighters of every AI guard a circle at their rally point, a few guard the mining area, drones patrol a circle over the base between strikes, and haulers are spread across gather circles around the piles near each hub. Weights persist in
`~/.local/share/onehour/brain.txt` (override with `ONEHOUR_BRAIN`). Difficulty changes reaction time, aggression and starting cash (Brutal cheats a
little, like Zero Hour's).

## Engineering notes

- C++17, SDL2 only. Sprites, terrain, the font and every sound effect are generated at startup; unit art (`src/art.cpp`) is drawn with
  lit, anti-aliased distance-field shapes on a 2x canvas, with walk / track animation frames, spinning rotors and burning wrecks;
  there are no asset files.
- The window is resizable (F11 fullscreen); the UI scale adapts and the HUD re-anchors. The whole terrain is
  baked once into a single texture with domain-warped, noise-blended transitions; fog of war is a bilinear mask.
- Fixed 20 Hz simulation with interpolated 60 fps rendering; the whole game uses a few percent of
  one core and ~90 MB.
- `./build/onehour --selftest 900` runs an AI-only game headless and checks invariants;
  `--scenario` drives every structure/unit/order for both factions; `--uitest` feeds synthetic
  mouse and keyboard events through the real input code; `--shot out.bmp --ticks N` renders a frame
  headless; `--train N` self-plays N games to train the AI brain, `--eval N` pits the learned AI against the plain heuristic AI;
  `--soundcheck` prints statistics for the synthesized sounds.

## Income and nukes

- **Oil Well** (Clanker, $1400) pumps $60 every 5 s for as long as it stands, no power needed. **Bitcoin Datacenter** (Cyber, $1600) mines $75 every 5 s and runs at half rate on low power. Up to 4 each, so income continues after the supply piles run dry.
- **Nuke Ramp** (both armies, $5000, needs the tech structure): each ramp can launch one tactical nuke every 5 minutes (60 s arming after it is built). The warhead flies for 7 s, so the enemy gets a warning circle, and it devastates a 6 tile radius of enemy units and structures. Any number of ramps can be built; more ramps means more warheads per cycle.
- Airfields honor their rally point: new aircraft fly there and wait, going back to the pad only to rearm.
