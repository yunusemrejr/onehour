# One Hour

A compact, low-resource skirmish RTS in the spirit of Command & Conquer: Generals – Zero Hour.
One map, two armies, you against one to three AI opponents, with an optional computer-controlled ally on your side. Every opposing army (and the ally) has its own difficulty. No campaign, no tutorial, no multiplayer.

    ./run.sh              # builds (first run only) and starts the game
    ./run.sh --scale 2    # force a UI scale (default: automatic)
    ./run.sh --size 1600x900   # initial window size
    ./run.sh --software   # software renderer instead of the GPU

Requirements: g++, make, and the SDL2 runtime (`libsdl2-2.0-0`). SDL2 headers are vendored
under `third_party/`, so no `-dev` package is needed. Ubuntu 24.04+ works out of the box.

## The map

One 80x80 map: a central lake ringed by road, four base quadrants split by rock ridges with a choke gap each, and a river leaving the lake into every quadrant (mirrored, so no side is favoured).
Roads cross the rivers on timber bridges and each river also has a sandy ford, so crossings are chokepoints but nothing is walled off. The ground is dotted with fallen logs and stumps, lily pads on the
river shallows, ruined courtyards, stone circles and burnt-out vehicle hulks (decoration only, units drive over them). `--mapdump` prints the tile map; `ONEHOUR_WORLDBMP=file.bmp` dumps the baked ground.

## Skirmish setup

The main menu is a small table: **You** (your army), **Ally**, **Enemy 1-3** and **Teams**. Every opposing army picks its own army (Cyber, Clanker, Random, or Off
for the second and third enemy) and its own difficulty (Easy, Normal, Hard, Brutal), so one enemy can be Brutal while another is Normal. An **ally** (None, Cyber, Clanker or Random,
with a difficulty of its own) is a computer-controlled army on your team: green, sharing its vision with you, defending your base when enemy fighters close on it, and going for the same
enemies. There are four seats in all, so with an ally there are at most two enemies. **Teams** says whether the enemies are allied against your team or fight everybody (free for all).
The match is won when every enemy army is gone and lost when you are out, even if your ally still stands. Keys: Up/Down pick a row, Left/Right change, Tab switches between the army and difficulty
column, Enter starts.

## Armies

**Cyber Army** — electric and optical. Pulse rifles, laser troopers, shock troopers, Photon Tanks,
Volt Walkers (arc lightning, anti-infantry/anti-air), Railgun Tanks (long range, pierces a line),
Wraith Drones, Hornet Gunships (attack helicopter), Laser Turrets and Patriot Batteries. Stealth and special forces: the Ghost Sniper, Infiltrator (spy), Ghost Drone (stealth spy drone), Atlas Lifter (cargo helicopter) and the Deep Bunker. Tech structure: Data Center (EMP Strike, Orbital Scan, Airlift Drop, Aid Drop,
Overclock Program → Ion Lancer and Aegis Titan super-heavy walker; structure upgrades Rugged Nanoshell, Defense Lasers, Nanite Repair).

**Clanker Army** — diesel and gunpowder. Riflemen, RPG troopers, heavy gunners, Brute Tanks,
Gatling Tanks, Rocket Launchers (artillery), Vulture Gunships, Gun Nests and Rocket Batteries. Stealth and special forces: the Sniper, Spy, Recon Drone (stealth spy drone), Mammoth Lifter (cargo helicopter) and the Underground Bunker.
Tech structure: Arms Lab (Shell Storm, Recon Flight, Paradrop, Aid Drop, Heavy Ordnance → Grenadier and Behemoth; structure upgrades Rugged Bulwark, Bunker Guns, Repair Crews).

**Base defenses** (Laser Turret, Patriot Battery, Gun Nest, Rocket Battery) are fortified: 5200 to 6800 hp each (more than a Command Core) and
brutal weapons: the Laser Turret burns 310 per shot, the Patriot Battery fires pairs of 240-damage missiles out to 10.5 tiles (more than double
against aircraft), the Gun Nest hammers out 42-damage rounds seven times a second and the Rocket Battery fires salvos of three, so a few of them hold
a choke point against a large force. Artillery and long-range rails can still outrange them, and a nuke still flattens them (unless they are Rugged).

**Tech structure powers** (select the Data Center / Arms Lab):

| Button | Key | Effect |
| --- | --- | --- |
| Strike (EMP Strike / Shell Storm) | X | call a strike on a circle of your choice (3 min cooldown); also on the Command Core/Post |
| Scan (Orbital Scan / Recon Flight) | V | the whole map and every enemy on it is visible for 30 s (3.5 min cooldown) |
| Paradrop (Airlift Drop / Paradrop) | P | a cargo plane drops a free force on parachutes wherever you click (2 min cooldown per tech structure, see below) |
| Aid Drop (human players only) | D | a white relief plane drops $20000 and a Dozer in crates to the army nearest the spot you click, ally or enemy, never you (1 min cooldown per tech structure, see below) |
| Advanced Program | R | $2500, 45 s (half speed on low power): unlocks the army's two elite units for good |
| Rugged (Nanoshell / Bulwark) | U | $3000, 40 s: every structure gets 50% more health and takes half damage. One nuke no longer flattens anything (a warhead takes at most 55% at ground zero, so it takes two); the plating only gives way when a massive assault lands a big share of a structure's health within a couple of seconds |
| Defense Guns (Defense Lasers / Bunker Guns) | G | $3500, 45 s: every structure gets a roof gun (Cyber laser, Clanker machine gun, 7.5 tiles) that shoots aircraft, vehicles and infantry; half rate on low power |
| Self-Repair (Nanite Repair / Repair Crews) | E | $2000, 30 s: every structure mends itself, 1.2% of its health a second once it has been out of fire for 3 s and a quarter of that under fire (never inside fallout). Units are not repaired: that is still the medics' job |

Structure upgrades are researched side by side, last the whole match and cover every structure the army owns, the ones standing and every one
it builds later, even after the tech structure that researched them is gone. The computer armies buy them too (easy only buys Self-Repair); `ONEHOUR_UPGMASK=bits` limits which ones they may buy, for balance runs).

Team colours: every unit type has its own tone (graphite, cobalt, ceramic and teal for Cyber; olive, sand, brick and iron for
Clanker), a small jewel-like dot in the owner's colour marks each unit, and every finished structure flies its owner's own flag
(rectangle+chevron, swallowtail+roundel, pennant, burgee+cross) from a mast on the roof.

**Paradrop** — select the Data Center / Arms Lab and press `P` (or click the button), then click the map or minimap. A cargo plane flies in from the map
edge along the line from your base through the drop zone and, as it crosses the circle, releases **15 infantry, 7 vehicles and 4 aircraft** (Cyber: 7 Troopers,
5 Laser Troopers, 3 Shock Troopers, 3 Photon Tanks, 2 Volt Walkers, a Railgun Tank, a Medic Rig, 2 Wraith Drones and 2 Specter Jets; Clanker: the matching
Riflemen, RPG Troopers, Gunners, Brute Tanks, Gatling Tanks, Rocket Launcher, Field Medic, Vulture Gunships and Talon Jets). The soldiers and vehicles hang
under parachutes for a few seconds (they cannot be hit or act until they land) and the aircraft take up guard over the zone. The load is free, the power
recharges for **2 minutes per tech structure** (first available at 2:30): like nuke ramps, every Data Center / Arms Lab you own has its own cooldown, so three of them can send three airlifts back to back and the button shows `READY x3`. The plane is not invulnerable: every anti-air gun that
reaches it on its way in shoots at it (1100 hp, a few Patriot Batteries bring it down) and a plane lost before the drop takes its whole load with it.
The enemy is told when an airlift is on its way, and the drop circle is public. The computer commander uses it to reinforce a base under attack
and to land on the objective of a wave, avoiding targets with a lot of anti-air. `--droptest` covers the call, the load, the parachute descent and the shoot-down.

**Aid Drop (yours alone)** — a power only a human player has; the computer armies, your ally's included, never get it. With a Data Center / Arms Lab selected,
press `D` (or click **Aid Drop**), then click the map or minimap. A big white relief plane with sky-blue aid roundels flies in along the same kind of track as a paradrop
and drops **five wooden crates on parachutes** over the spot. Nobody shoots at it, enemies included, since it brings them help: no gun, no anti-air and no computer army's
nuke ever harms it. Only your own nuke can bring it down, like every aircraft in your blast: crates still aboard are lost, crates already falling still land.
When the last crate is down, the **army nearest to the spot gets $20000 and a Dozer** (the Dozer rolls out beside the opened crates):
- nearest means the shortest distance from the spot to any of that army's units or structures, measured to a structure's edge (0 when the spot is on it), so the army you drop on gets it, and where two bases are intertwined the structure nearest the spot decides;
- **you never get it**: drop it on your own base and it still goes to the nearest other army, however far away that one is;
- ally or enemy alike; on an exact tie between armies, one of them is drawn at random (still never you).

While you aim, the cursor names the army that would get it and a line points to its nearest unit or structure. Once it lands, a message names the receiving army and a label floats over the crates.
Like the paradrop it recharges **per tech structure** (1 minute each), so three Data Centers can send three aid flights back to back and the button shows `READY x3`.
`--aidtest` covers who may call it, the cooldowns, the money and the Dozer, dropping on your own base, an enemy nuke (spared) and your own nuke (downed) on the plane, and fair draws on a tie.

**Force fire (human players only)** — attack a unit or structure of your own or of your ally on purpose, exactly as you would an enemy: select your units and press the
**Force Fire** button (key `F`), then click the target, or hold `Ctrl` and right-click it (or attack-move onto it with `A`). Tanks and infantry shoot it, jets and bombers strafe and
bomb it, turrets can be turned on it (and take it up again whenever it comes back into reach), and area weapons (shells, arcs, bombs, rails, grenades) hurt everything near the
impact, friends included. **Your nukes hit everybody under them**: a warhead you launch flattens your own and your ally's units and structures exactly as it would an enemy's
(your units and the computer ally's get out of the circle if they can). Every aircraft inside the blast, yours included, goes down at once, and so does a paradrop plane flying over it.
Your strike power spares your own army unless you hold `Ctrl` or press `F` while aiming it. A computer army's nukes never hurt its allies on the ground.
Apart from your nukes nothing ever hurts a friend by accident, and a plain right-click on an ally's unit or structure just moves your selection there. **Your allies never shoot back**: their
units and the computer ally's commander only ever fight enemies, no unit of anybody's comes to "help" against friendly fire, a computer ally simply gets its units out of the circle
of a nuke you aim at it, friendly kills do not count in the statistics, and no "your ally is under attack" alarm goes off. Enemies are unaffected: the game goes on as usual.
Computer armies (the AI commander, the learned brain and the AI ally) can never force fire: the simulation drops the flag for any army that is not human. `--rulestest` and
`--autotest` cover it.

**Units on their own** — a unit that is not carrying out one of your orders (standing idle, holding the spot it stopped on, guarding an area, or busy with something it picked
itself) looks after itself and its friends twice a second:

- **Answers fire from beyond its reach or its guard zone**: shot by something it can hit (artillery out-ranging it, a sniper's spotter, a raider just outside the guard circle),
  it goes after the shooter when it and the friends around it would win the local fight, even well outside a small guard zone.
- **Haulers keep clear of raiders**: a hauler at work whose pile has an enemy gun closing in (within 6.5 tiles, no two friendly fighters near enough to see it off) does not wait to be shot: it drives out of range
  toward the depot, away from the raider and never into a corner of the map, and goes back to the pile once the danger has passed (`--haultest`: it escapes with two hits and resumes work, with the behaviour off it is killed).
- **Backs away from danger it cannot fight**: hit by something it cannot hit back (a tank bombed from the air, a rifleman strafed by a jet), or losing a fight away from your
  base, it pulls back a few tiles toward its friends and holds there for a while. A badly hurt soldier or vehicle drives over to a nearby medic; a badly hurt aircraft under fire
  flies home and is patched up on its pad (aircraft parked on their pad are repaired, about 3% of their health a second).
- **Helps friends**: when one of your or your ally's units or structures nearby is hit by an enemy, idle and guarding units (within about 9 tiles of their post, more for a guard
  zone) and aircraft (about 16 tiles around their airfield, rally point or patrol circle) go after the attacker.
- **Gets out of a nuke's circle and out of radiation** (your units; the computer commanders move their own): seeing a warhead coming, ground units leave the blast circle, haulers and
  dozers included (aircraft never try: nothing flying gets out of a nuke), and pick their work up again once it has landed; idle units walk out of fallout, guards do not walk back into a poisoned slot, and haulers prefer clean piles.
- Whatever it takes on itself stays **leashed** to its post (about 12 tiles for ground units, 20 for aircraft) and it **returns** there afterwards. Any order you give replaces it.

Units jammed in a one-tile gap between structures (two haulers head-on, a soldier standing in a lane) now slip past each other instead of shoving forever.

**Snipers** — both armies train one at the Barracks / Command Post once the tech structure stands: the **Ghost Sniper** (Cyber, needle rifle) and the **Sniper** (Clanker, rifle).
$550, fragile, long sight, 11.5 tile range: they kill any infantryman in a couple of shots and can shoot nothing else, not vehicles, structures, aircraft or other snipers. A sniper is **stealth**:
nothing sees it, not soldiers, not tanks, not turrets, not even aircraft. **Only an enemy spy drone finds it** (see below), and only for as long as the drone stays within nine tiles;
until then it cannot be selected, targeted or shot (an area blast that happens to land on it, and a nuke, still hurt it). It stays hidden while it fires. The AI builds them against infantry-heavy enemies and
builds spy drones to find yours.

**Stealth, detection and spy drones** — three kinds of things hide from the enemy: snipers, finished underground bunkers (class *spotter*) and the spy drone itself (class *drone*). A hidden thing is invisible to the
enemy team (not drawn, not on the minimap, not selectable, not a legal target, unknown to the computer commanders; its own side sees it as a faint ghost). It becomes visible only while one of the enemy team's **detectors** is close enough:

| Detector | Finds | Range |
| --- | --- | --- |
| **Spy drone** (Ghost Drone / Recon Drone) | snipers, bunkers and enemy spy drones | 9 tiles |
| Bomber and attack drones (Wraith Drone, Vulture Gunship, Hornet Gunship) | enemy spy drones only | 3.5 tiles ("your own drones need to be there") |
| everything else (soldiers, tanks, jets, turrets, lifters) | nothing | - |

A sniper or spy drone that leaves the detector's range is lost again after a couple of seconds; a **bunker stays known for 25 seconds** after the drone moves on, so a force has time to attack it. Detection is shared by the whole team.
The **spy drone** ($900, Drone Pad / Airstrip, needs the tech structure) is a small unarmed hovering stealth helicopter, 15 tiles of sight, fragile (220 hp), needs no pad. Because only enemy drones can spot it, anti-air batteries and fighters never shoot at it
until a drone of the enemy is practically on top of it. The AI keeps one (two from Hard) with its army.

**Spies** — the **Infiltrator** (Cyber) and the **Spy** (Clanker): $650, Barracks / Command Post, needs the tech structure. Unarmed, fragile, quick. Select one and right-click an enemy structure: it walks up and **captures it**
after 4 s + 0.7 s per footprint tile (8 s for a Barracks, 15 s for a Command Core); a progress bar shows over the spy and the owner gets an alarm. The structure changes hands with everything on it: the production queue is
refunded to the old owner, a bunker's garrison is turned out onto the ground, its health carries over. The spy **survives** the capture and can take the next one; if it is killed first, nothing changes. A captured production structure
trains the new owner's units, a captured power plant, supply hub, tech structure or nuke ramp works for its new owner. The AI sends two spies (Hard and up) after valuable structures nobody is guarding. A hidden bunker can only be captured once a spy drone has found it.

**Cargo lifters** — the **Atlas Lifter** (Cyber) and the **Mammoth Lifter** (Clanker): $2600, Drone Pad / Airstrip, needs the tech structure. A huge tandem-rotor helicopter, 2400-2600 hp, unarmed, 40 cargo places (soldier 1, light vehicle 4, tank 6,
super-heavy walker 10: for example 30 soldiers, a tank and a light vehicle, or six tanks and four soldiers). Select soldiers or vehicles and right-click the lifter: they walk to it and climb aboard while it hovers. Select the lifter and press **U** to set down where it is and let
everybody out, or **D** (Drop Off) and click a spot: it flies there, sets down and unloads (soldiers first, vehicles after). A lifter shot down in the air takes everyone aboard with it; one on the ground lets them climb out. Use it to put a spy, a dozer or an army behind the enemy lines.

**Underground bunkers** — the **Deep Bunker** (Cyber) and the **Underground Bunker** (Clanker): $2500, built by a dozer, needs a Barracks. Only a small steel hatch (2x2 tiles) shows on the surface, and not even that to the enemy: a finished bunker is **stealth**, found
only by an enemy spy drone. It holds **50 infantry and one dozer** (right-click the bunker with soldiers or a dozer selected; U unloads, the rally point is where they walk to). Everybody inside is untouchable. In a fight the whole garrison **fires from the hatch**, each soldier with its own weapon (rocket troopers
hit tanks and aircraft, snipers pick off infantry), and heals slowly. The rest of the rules:
- the **hatch** (2800 hp) takes the damage first; once it is blown off the garrison cannot shoot, though it stays safe. A **nuke wrecks the hatch and hurts the bunker, but can never destroy it**, however many land (buried deep): the same goes for aircraft, bombs, turrets, shells and fallout, none of which takes it below 10% of its health;
- only **infantry and vehicles can destroy a bunker**, and it is very hard: 30000 hp and plating that stops 78% of what gets past the hatch (a column of fifteen tanks needs several minutes; they only see it while a spy drone is nearby);
- a **dozer inside** patches the hatch (6% a second) and the bunker once out of fire for three seconds; any dozer next to it mends it too;
- when the bunker is destroyed (or captured by a spy) **the whole garrison is back on the ground, exposed**.

**Veterancy and rocket infantry** — infantry earn promotions: **Veteran** after 4 kills (+25% health, +20% damage) and **Elite** after 10 (+50% health, +40% damage), shown as chevrons beside the health bar, in the bunker as well. The **RPG Trooper** is the rocket infantryman:
its rocket hits **vehicles and aircraft hard** (64 damage, 1.3x against tanks, 1.6x against aircraft, faster missile) and works from inside a bunker alongside the rest of the garrison. `--stealthtest` covers all of the above.

**Aircraft** — each army has a cheap, fast, fragile bomber (Wraith Drone / Vulture Gunship: $850, 520 / 560 hp, 340 / 300 px/s) and an expensive, fast, powerful fighter-bomber
(Specter Jet / Talon Jet: $2400, 1500 / 1550 hp, 560 / 540 px/s, 10 passes a sortie). Both kinds hit air and ground: the bombers carpet-bomb the ground and shoot aircraft down with
their guns, the jets use heavy plasma lances / homing Sidewinders on everything (about 85-100% against structures) and fight aircraft on the turn.
The Cyber Drone Pad also builds the **Hornet Gunship** (key `H`, $950, 640 hp, 280 px/s), an electric attack helicopter whose twin lasers hit ground and air;
it never needs to rearm.

**Helicopters have no limit** — an airfield has four pads, one per fixed-wing aircraft (drones and jets), and a plane waits in the queue until a pad is free.
Helicopters (Hornet Gunship, Vulture Gunship) do not take a pad: they land on helipads in rings around the airfield, so one airfield builds and houses as
many of them as you can pay for. The airfield's panel shows `Planes n / 4` and `Helicopters n (no limit)`. **Helicopters never pile up**: wherever several are sent to
the same place (one move order, a rally point, a crowded guard zone, a shared helipad), each one hovers on a spot of its own a small gap from the next (they pack in
hexagonal rings around the spot, and the one already there keeps it). **Guarding an area, a helicopter hovers still** on its own post in the circle, nose turned out,
instead of flying circles like the planes; it goes after what enters the zone and comes back to hover on its post.

**Flight** — the jets and the Wraith Drone (a flying wing) are **fixed-wing aircraft** and fly like aeroplanes: they hold an airspeed between a stall speed (36% of their top speed)
and their top speed, accelerate and brake at finite rates, and turn under a g-limit, so the slower they fly the tighter they turn (up to twice their turn rate at top speed). They
**never stand still in the air**: an aircraft waiting somewhere circles the spot, and only the final approach to the pad is flown below the stall speed. Sent somewhere with a
move order, aircraft circle there for a minute (picking off what they see) before flying home; after a kill they look around for a few seconds for more. The Vulture Gunship
and the Hornet Gunship are helicopters and hover. **Dogfights**: a fixed-wing fighter flies lead pursuit, pulls round at its corner speed (where it turns tightest) whenever the target is off its
nose, slows to the target's pace when tucked in behind it, extends straight out when the target sits inside its turn circle behind the wing and comes round again, and fires
whenever the target is inside its weapon cone (wide for homing missiles, narrow for lasers). It stays on the target after a burst instead of breaking away, so a jet out-turns
and shoots down bombers and helicopters, two jets fight each other properly, and an aircraft being shot at by another one turns on it. Aircraft near the map edge pull round
toward the middle in time, and a plane that loses its airfield on final approach goes round again.

**Bombers** — the Wraith Drone and the Vulture Gunship are bombers. Against anything on the ground they fly real bombing runs: they line up on the
target (leading a moving one), release a stick of four heavy bombs a few tens of pixels apart along the flight line, fly straight on for a second and
swing round for another pass (12 / 16 bombs per sortie, reloaded in under half a second per bomb on the pad). A bomb splashes two tiles, hits
structures about one and a half times as hard as soldiers' small arms, and goes off in a fireball with secondary bursts, a shockwave ring, flung debris,
a dust ring, a climbing smoke column and a scorch mark. A flight of three takes a factory down in one or two passes, so anti-air batteries matter.
Against aircraft they keep using their guns. They are fragile on purpose (520 / 560 hp): their price is low and they are fast.

**Supersonic jets** — the Drone Pad and the Airstrip each build a second aircraft once the tech structure stands: the **Specter Jet**
(Cyber, stealth delta, plasma lances) and the **Talon Jet** (Clanker, swept-wing fighter-bomber, homing Sidewinders). They are the
fastest units in the game (about 550 px/s, 16 tiles a second) and fly like fixed-wing aircraft (see Flight above): no hovering, a turn
circle of ~165 px at full speed and much tighter at corner speed, strafing passes that dive on the target, fire a stream of rounds, break
away and come round again. They are fragile, shred aircraft and light vehicles, are poor against structures, take off from and land on the pad, and
leave afterburner flames and vapour trails behind. Idle jets scramble against anything that comes within 11 tiles of their field.

**Rebuilding a base** — the Command Core / Command Post is no longer unique: any dozer can found a new one ($3000, 45 s) from its build menu
(key `C`, last entry), so a player who keeps a dozer alive and has the money can rebuild a lost base from nothing, or raise a second
one as an expansion (three at most at a time). A Command Core is the only source of dozers, strike powers and the home position: when
the last one stands the camera Home key and the AI's base layout move to the new one. The AI does the same: it banks the price and
sends its dozer when its Command Core falls.

Both sides: Command Core/Post trains dozers, power plants provide 10 power, Supply Hub/Depot builds
haulers that carry $300 per trip from supply piles. Running out of power halves production and
switches powered defenses off, exactly the pressure Zero Hour puts on you.

## Controls

| Input | Action |
| --- | --- |
| Left click / drag | select (double-click: every unit of that type on screen, Tab: army on screen) |
| Right click | move, attack, gather, repair/continue construction, set rally point (also on the minimap); on an ally's unit or structure: move there |
| A + click | attack-move, S: stop |
| F, then click (or Ctrl + right click) | **force fire**: attack any unit or structure on purpose, your own and your allies' included (they never shoot back). F or Ctrl while aiming a strike power: friendly fire (a nuke always hits everything under it) |
| G, then click or drag | **guard an area**: fighters hold spread-out posts in the circle and engage what enters it (chasing only while the target stays inside), aircraft patrol it and return to rearm; **gather in an area**: haulers search the circle for supply piles and work them until it is empty. Any move/stop order cancels it. |
| Select a turret / battery, or hover any defense | shows the area it covers (ground, air, or both; grey when unpowered; enemy defenses in red). The range also follows the cursor while you place a defense |
| Ctrl+0–9 / 0–9 | assign / recall control group (Alt+#: jump to it) |
| Arrows, screen edge, middle drag, wheel | scroll, Home: your base |
| Dozer selected | build menu with hotkeys (including `C`: a new Command Core / Post, `U`: Underground Bunker); click the ground to place, Shift to place several |
| Spy selected, right-click an enemy structure | capture it |
| Soldiers / dozer selected, right-click your bunker or cargo lifter | climb aboard; with the bunker or lifter selected `U` unloads, lifter `D` then click: fly there and drop everybody off |
| Structure selected | train units (hotkeys shown), Del sells |
| X | strike power (from the tech structure, or the Command Core/Post once it exists); V map scan, R Advanced Program, P paradrop, D aid drop, U / G / E structure upgrades with a tech structure selected |
| Space | pause, + / - game speed (x0.5 to x4), F1 help, F2 mute, F11 fullscreen, F12 screenshot |
| Esc | cancels a pending order, otherwise opens the pause menu: resume, game speed slider, sound, help, restart, surrender, quit to menu / desktop |
| K | with a Nuke Ramp selected: pick a target for a tactical nuke |

## AI

Each opponent runs a utility-driven commander: economy and power management, prerequisite-aware
base layout, defenses placed toward the enemy, threat assessment, army composition weighted against
the observed enemy mix and continually re-weighted by which of its unit types are actually earning
kills per credit, wave attacks that launch only when they outweigh the local defense, regroup and
reinforcement waves, harassment of haulers, aircraft strikes and special powers aimed at the
densest cluster. Three parts are learned online (`src/brain.cpp`): a logistic model that predicts whether
an attack wave will trade favourably (launch/hold decision), per-unit-type regressions of value destroyed
per credit given the enemy's infantry/vehicle/air mix (production choice), and the **opening doctrine**: a commander opens a match as
*balanced*, *rush* (early, smaller waves, few defenses), *turtle* (heavy defenses, early tech and nukes, big waves), *air* (early airfields, ten bombers, extra anti-air) or
*boom* (an extra income structure, then a larger army), picked by UCB1 from each doctrine's running win rate and rewarded when the match ends, so it
learns what beats the people it plays: the win rates are kept per opponent (a person or a computer, Cyber or Clanker) and shrunk toward the overall table while a
context has few games, so a doctrine that works against the computer commanders does not decide what it opens with against *you*. The attack-wave model scores the launch from the
combat estimate below (a logistic regression on the log of the assault ratio, the ratio against the whole enemy army, the minute and the army size) and learns online, pulled back toward its calibrated prior so a few lucky waves cannot throw it off. Idle fighters of every AI guard a circle at their rally point, a few guard the mining area, drones patrol a circle over the base between strikes, and haulers are spread across gather circles around the piles near each hub. Weights persist in
`~/.local/share/onehour/brain.txt` (override with `ONEHOUR_BRAIN`). Difficulty changes reaction time, aggression, how many income structures it raises and
how soon, whether it dodges nukes, and starting cash. **Brutal** cheats hard: $6000 extra at the start, 1.5x income from deliveries and income structures,
1.6x faster production, construction and research, 25% more damage against its enemies, four dozers and four factories, up to 90 units, nine defenses,
two nuke ramps from minute 7, an early first wave (2:45) on a low threshold, harassment squads every minute and a 0.3 s reaction to incoming nukes.

Beyond the learned parts the commander plays tactically:

- **Spending**: a commander never sits on a bank. It measures its income (credits per second from hauled supplies and income structures), raises factories and barracks until its
  production keeps up with it (up to 2 / 3 / 5 / 7 factories and 2 / 3 / 4 / 5 barracks from Easy to Brutal), keeps more dozers while the bank is full, lifts its army cap with the bank, and
  runs the timed gates for tech, airfields, upgrades and nuke ramps on *effective* minutes (the clock plus what the bank allows), so a rich commander is not made to wait. From Normal up it also keeps a
  bigger hauler fleet on the road (a hauler repays itself in about half a minute); Normal uses all of this at half strength and Easy none of it, so the tiers stay apart (the new Normal is about the old Hard's equal, the new Hard beats the old Brutal 50-21). Opponents that used to sit on $50 000 at minute five now field the army it buys.
- **Combat estimate**: the Lanchester square law over the weapon and armour tables. A side is its health split by armour class and the damage per second it deals to each class (zero where a weapon cannot
  reach: aircraft, snipers); a force beats another by the ratio of (its damage on the other's armour mix x its health) to the reverse. The commander launches a wave when the estimate says the army outguns the
  defence at the target (armed structures count in full, the defender's fighters in full nearby and less the farther away they stand), and **production counters what it sees**: for every candidate unit
  it asks what a few thousand credits of it would add to the army's strength against the enemy force actually observed, so a field of aircraft calls for anti-air, armour for armour-piercing guns, and so on.

- **Economy**: income structures pay for themselves in under a minute, so it banks for them as soon as it has power and a first army (2 / 3 / 4 of them on Easy / Normal / Hard and Brutal).
- **Nukes**: it aims at what a warhead really costs the enemy (units and small structures in the blast, a share of large structures' value, minus friendly units in the fallout) and
  fires only for a big enough pay-off. When an enemy nuke is launched it notices after a delay set by its difficulty (4.5 s Normal, 2 s Hard, 0.3 s Brutal, out of the 7 s flight; Easy never does) and walks its ground units and haulers
  out of the circle (its aircraft stay and burn); afterwards non-attacking units leave radiation zones, rally points move out of them and attack waves avoid targets inside them.
- **Bombers**: a flight waits until it is loaded, then goes together for the target with the best value (nuke ramps, income structures, supply, power, then production; harvesters;
  clusters of units) minus the flak over it; heavily defended sites are skipped.
- **Anti-air**: batteries follow the enemy's aircraft and airfields and are placed beside the structures most worth protecting.
- **Medics** (one per 14 fighters, up to three) trail the army and are never sent ahead as part of a wave.
- **Waves** break off when the fighting around them is clearly lost (two bad readings in a row), not only when half of them are dead.
- **Special forces**: from Hard (one spy drone from Normal) the commander keeps spy drones with its army, since they are the only thing that finds hidden snipers and underground bunkers, and trains two spies that go for the most valuable structure nobody is guarding (`AiFeat` `FEAT_SPECIAL`: over 88 mirror games it beat the same commander without it 14-2, 72 unresolved). It does not use cargo lifters or bunkers itself.
- **Allies**: a computer ally defends *your* base too (it sends most of its army to wherever enemy fighters are closing on a teammate's structures), shares its vision with you, and
  plays at the difficulty set for it. When *you* push out (five or more of your fighters away from home, near an enemy structure) it moves its army up to the same front and picks its next targets near it,
  telling you so ("Ally: joining your attack", "Ally: sending units to defend your base"). `--allytest` covers it.

`--evalai N` plays the current commander against the previous generation of the AI (over 72 games it loses none at Normal, Hard or Brutal: 55, 66 and 66 wins, the rest unresolved after 1500 s); `--evaldiff N` with
`ONEHOUR_DA` / `ONEHOUR_DB` checks that the difficulty ladder holds (Normal and Hard beat Easy 16-0, Hard beats Normal 10-0, Brutal beats Hard 16-0 over 16 games each). Every part of the new commander has its own switch (`AiFeat` in `src/ai.h`),
so each was measured alone: `ONEHOUR_BASEMASK` / `ONEHOUR_NEWMASK` (bit sets) make the two sides of `--evalai` play with different feature sets, `ONEHOUR_BRAIN2=path|prior` gives the baseline its own brain (what is training worth?) and
`ONEHOUR_EVALGAME=i` replays game *i* of a series and prints the final position, `ONEHOUR_EVALTEAMS=1` plays two new commanders against two previous ones (2v2: 35-0, 47-0 and 42-0 over 48 games at Normal, Hard and Brutal), `ONEHOUR_OLDDIFF=d` sets the baseline's difficulty. Ideas that did not beat the baseline over about a hundred games each were taken out again: staging a wave outside the target's defences (it lost 30-47: the whole defending army
reacts to the muster point), judging a running wave by the estimate instead of by what it cost, choosing the target by the estimate, falling back under the turrets when outgunned, focus-fire target scoring, matching a column's speed to its slowest member, and an early third supply hub.

## Engineering notes

- C++17, SDL2 only. Every sprite, the terrain, the font and every sound effect are generated at startup; there are no asset files.
- **Painter** (`src/paint.h`): shapes are signed distance fields with anti-aliased coverage and a bevel lit from the north-west, drawn on 2x
  supersampled canvases. Units (`src/art.cpp`) have walk / track frames, turrets, spinning rotors and burning wrecks; structures
  (`src/artb.cpp`) are raised 3/4-view blocks with south walls, roof props, windows, soft cast shadows, a construction-site sprite and a ruin
  sprite, painted once per type and shared by every owner (the owner's colour comes from a white team mask tinted at draw time, plus the flag).
- **Terrain** (`src/terrain.cpp`): the whole 80x80 map is baked once into one 2560x2560 texture on worker threads: smoothed class coverage with
  noise-displaced borders, procedural grass / dirt / sand / asphalt / rock / water shading, a height field that lights cliffs and lake banks and
  casts soft shadows from rocks and tree canopies, foam and wet sand on the shore, painted road markings, individually drawn trees, boulders,
  flowers, pebbles and reeds. The minimap is a downsample of the same texture.
- **Effects** (`src/fx.cpp`): fireball frames, flame tongues, smoke and dust puffs, muzzle stars, beams, shock rings and scorch decals are baked
  sprites; a capped (900) render-side particle system adds debris, sparks, smoke columns, vehicle dust, vapour trails and nuke mushroom clouds,
  spawned the first frame a simulation effect appears. It never touches the simulation and freezes with the game.
- The simulation is untouched by any of it; the whole game still uses a few percent of one core: `./build/onehour --bench` renders 300 frames of
  an eight-minute four-army battle with the CPU-only software renderer (about 5 ms a frame, 65 MB resident; startup under a second).
- The window is resizable (F11 fullscreen); the UI scale adapts and the HUD re-anchors. The whole terrain is
  baked once into a single texture with domain-warped, noise-blended transitions; fog of war is a bilinear mask.
- Fixed 20 Hz simulation with interpolated 60 fps rendering.
- `./build/onehour --selftest 900` runs an AI-only game headless and checks invariants;
  `--scenario` drives every structure/unit/order for both factions; `--uitest` feeds synthetic
  mouse and keyboard events through the real input code; `--shot out.bmp --ticks N` renders a frame
  headless; `--train N` self-plays N games to train the AI brain, `--eval N` pits the learned AI against the plain heuristic AI;
  `--soundcheck` prints statistics for the synthesized sounds; `--hqtest` rebuilds a lost Command Core (human dozer and AI), `--jettest` flies
  the jets (speed, banking, strafing, rearming), `--econtest` and `--areatest` cover income structures, nukes and area orders, and `--bench [N]`
  times N rendered frames. `--airmatrix` and `--groundmatrix` send every aircraft and every ground combat unit (idle, attack-move, guard area, attack order; moving targets; flights of four) against every kind of target and require damage within a time limit, `--turrettest` does the same for turrets and anti-air batteries, `--helitest` builds 18 helicopters from one airfield (and checks they park on helipads with a gap between them, stop side by side on a group move, hover still on posts of their own when guarding a small circle, and that planes still wait for a pad), `--aidtest` covers the Aid Drop, `--stealthtest` covers stealth snipers and spy drones, spy captures, cargo lifters, underground bunkers (garrison, hatch, nukes, destruction) and promotions, `--allytest` covers a computer ally moving up to the front where your army fights, `--haultest` covers haulers escaping a raider and returning to work, `--upgradetest` covers the structure upgrades (research rules, Rugged under one and two nukes and under a massive assault, roof guns against aircraft, vehicles and infantry, self-repair on structures but never units, upgrades lasting after the tech structure falls) and dozers sharing out repairs, `--fuzztest [secs]` throws random commands at the simulation (run it under `-fsanitize=address,undefined`), and `ONEHOUR_STUCK=1 --selftest` reports units that hold a movement order without moving. `--autotest` covers units left on their own (answering fire from beyond their reach, backing away from aircraft they cannot hit, helping an ally, a small guard zone, nuke dodging), force fire through the Force Fire button with no ally or own unit shooting back, jets shooting down bombers and fighting each other, and no fixed-wing aircraft hanging still; every `--selftest` also fails if a fixed-wing aircraft away from its airfield ever flies slower than its stall speed. Screenshot helpers: `ONEHOUR_CAM=tx,ty`, `ONEHOUR_REVEAL=1`, `ONEHOUR_JETS=1`, `ONEHOUR_BOMBS=N`, `ONEHOUR_DROP=N` (+ `ONEHOUR_CAMBACK=px`), `ONEHOUR_AID=N` (an aid drop near the enemy base; + `ONEHOUR_CAMBACK=px`, `ONEHOUR_CAMPLANE=1`), `ONEHOUR_NUKEDMG=N`, `ONEHOUR_MENUDEMO=row`, `ONEHOUR_NEWB=1|boom`, `ONEHOUR_HELI=N` (N helicopters of the viewed army over its base; `ONEHOUR_HELIGUARD=1` has them guard a small circle there), `ONEHOUR_UPG=1|raid` (every army starts with all structure upgrades; `raid` then sends aircraft and tanks into the viewed base), and `--sheet FILE`
  with `ONEHOUR_BLD=0|1` (every structure of an army) or `ONEHOUR_BIG=0|1` (every unit, enlarged).

**Medics and mending** — each army's factory builds a healing vehicle (Cyber **Medic Rig**, Clanker **Field Medic**, key `Y`, $900). Its aura
heals every friendly soldier, vehicle and aircraft within 5 tiles (6% of max health per second; structures at a third of that), and an idle
medic drifts toward the nearest wounded friend. Idle dozers also repair damaged structures on their own, and they share the work out: each dozer
weighs distance against how badly a structure is hurt and how much it matters (defenses, power, Command Cores and tech come first), a structure
takes only as many dozers as it has room for (one for a turret or a scratch, up to three for a big hurt building), a dozer that finishes a repair
moves straight on to the next one nearby, and a group of dozers ordered onto one structure leaves its crew there and fans the rest out to the other
damaged structures around it. Jets now peel away from each strafing
pass instead of flying through the defences, steer clear of heavily covered targets when softer ones exist, and carry more armour and ammo.
`--supporttest` covers medics, dozer repair, nukes and fallout; `--bombtest` covers the nuke damage model (what collapses, what is left standing, who falls from the sky) and the bombing runs, and `--nuketest` drops warheads on every aircraft type of a computer and a human army (none may get away).

## Income and nukes

- **Oil Well** (Clanker, $1400) pumps $380 every 5 s for as long as it stands, no power needed. **Bitcoin Datacenter** (Cyber, $1600) mines $450 every 5 s and runs at half rate on low power. Up to 4 each, so income continues after the supply piles run dry.
- **Nuke Ramp** (both armies, $5000, needs the tech structure): each ramp can launch one tactical nuke every 5 minutes (60 s arming after it is built). The warhead flies for 7 s, so the enemy gets a warning circle, and it tears up an 11 tile radius around ground zero. **Every enemy ground unit in the blast collapses** (a lethal core, then falling damage), **every aircraft in the blast falls out of the sky, whoever owns it** (parked, hovering or passing through, the launcher's own and its allies' too; nothing flies fast enough to get away, so aircraft never try; a paradrop plane over the blast goes down with its load, and so does your own aid plane in your own blast), **small structures collapse** (anything of six tiles or less: turrets, batteries, reactors, barracks, income structures) while **large structures survive heavily damaged** (about 80% of their health lost at ground zero, a fifth at the rim, never lethal, and knocked offline for a while). Survivors are stunned and hurled, and chain explosions go off under a towering mushroom cloud (a rolling vortex cap lit by the fireball, a turbulent stem, a condensation collar and a base surge of dust). The missile itself is a large two-livery warhead with a long exhaust, a smoke trail, a ground shadow and a glowing nose on re-entry. A computer army's friendly ground units are thrown but never hurt by the blast itself; a human's warhead hurts the human's own army and allies exactly as hard as an enemy's. The crater stays radioactive for 80 s: every unit on the ground inside it, friend or foe, keeps taking damage, while structures only suffer a slow drain that stops at a tenth of their health. Any number of ramps can be built; more ramps means more warheads per cycle.
- Airfields honor their rally point: new aircraft fly there and wait, going back to the pad only to rearm.
