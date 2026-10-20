# One Hour

A compact, low-resource skirmish RTS in the spirit of Command & Conquer: Generals – Zero Hour.
One map, two armies, you against one to three AI opponents, with an optional computer-controlled ally on your side. Every opposing army (and the ally) has its own difficulty. No campaign, no tutorial, no multiplayer.

    ./run.sh              # builds (first run only) and starts the game
    ./run.sh --scale 2    # force a UI scale (default: automatic)
    ./run.sh --size 1600x900   # initial window size
    ./run.sh --software   # software renderer instead of the GPU

Requirements: g++, make, and the SDL2 runtime (`libsdl2-2.0-0`). SDL2 headers are vendored
under `third_party/`, so no `-dev` package is needed. Ubuntu 24.04+ works out of the box.

## Defensive overhaul (walls, Hyper Protection, Anti-Rocket, 25-jet airfields)

- **Wall** (key `W`, 1x1, $180, 3 s, **no build limit**): **1,800 hp** — sturdy against small arms and light vehicles, but no harder to kill than any other small structure: a nuclear warhead flattens one at ground zero like it does a turret or a reactor. Hyper Protection doubles its health and caps a single warhead at 42% (so an upgraded wall survives one hit). Build many and chain them edge to edge;
  walls may be placed adjacent to each other and are deliberately exempt from the one-tile gap and corridor-plugin checks.
- **Hyper Protection** (Data Center / Arms Lab, key `H`, $6000, 55 s): every structure gets **2x health**, takes 60% less conventional damage, and
  no single warhead (including the silver Underground Nuke) can take more than **42% of its max health**. A full-health protected building can
  therefore never be erased by one warhead: only already-damaged structures in the blast can be finished.
- **Anti-Rocket** (Data Center / Arms Lab, key `A`, $4000, 45 s): every wall mounts a 10-tile point-defence laser that burns incoming rockets,
  shells and **nuclear warheads** (including Underground Nukes) and shoots down **aircraft**. Walls are unlimited, so overlapping wall lines
  blanket a base. The Q anti-nuke defences also engage rockets, warheads and aircraft; only a laser can stop a silver warhead.
- **Anti-nuke balance**: the **Anti-Nuke Laser is now 40% weaker at shooting nuclear warheads down** (18% base instead of 30%), and an
  **Anti-Rocket wall's laser 70% weaker at it** (11.4% base instead of 38%). Only warheads are affected: their fire against rockets, shells,
  bombs and aircraft is unchanged, and the **Clanker Anti-Nuke Missiles keep their 30%**. The Underground Nuke's **armoured silver warhead halves
  every defence's chance again** (a lone Anti-Nuke Laser rolls 9% against it, a lone Anti-Rocket wall 5.7%). See **Anti-nuke defences** below.
- **Command Core / Post**: **18,000/18,500 hp** (down from 60,000/62,000), takes 35% of conventional damage, and a warhead can take at most **40%** of its max health: three direct hits bring it down. It remains the toughest building: the **durability order is Command structure (18,000/18,500) > Underground Nuke silo
  (14,000/14,500) > underground bunker (11,000) > Data Center / Arms Lab (7,000/7,500) > wall (1,800)**.
- **Barracks** infantry production is always **3x** the old rate.
- **Airfield / Airstrip** is now **10x7 tiles** with **25 fixed-wing pads** (one per fixed-wing aircraft: drone, jet or nuclear bomber); helicopters
  still have no limit.
- **Underground bunkers are now 11,000 hp** (less than half of what they had) and **are no longer stealthy**: their surface hatch is plainly visible to both sides, and they are seen and targeted like any other building. Deep plating and hatch rules remain, but warheads are lethal to them now: repeated direct nukes destroy a bunker (only aircraft, shells, structures and fallout still cannot).

## The map

One **128x128 map** (4096x4096 world pixels): **23% wider and 51% more ground** than the previous 104x104 arena. The central lake has 1,561 connected water tiles, bays, a sandy beach, two rocky islets and a larger buildable island around its watchtower. The ring road runs 32.6 tiles out; the four base quadrants, ridge gaps, rivers, bridges, fords, woods and supply positions scale with the arena. Structures, road widths and unit silhouettes keep their readable size. A dozer still has to be airlifted to the island, so holding the water and sky opens room for a small fortified foothold.
The lake is the arena of the warships (see **The lake and the navy** below); the four central supply piles sit between the beach and the ring road, within reach of the ships' guns, and four more piles hide in the quadrant woods for whoever contests them (20 piles in all).
Roads cross the rivers on timber bridges and each river also has a sandy ford, so crossings are chokepoints but nothing is walled off. A bridge closes a river to ships (it is road, not water), so only the lake itself is open sea. The ground is dotted with fallen logs and stumps, lily pads on the
river shallows and in the lake's bays, ruined courtyards, stone circles, burnt-out vehicle hulks and the craters of some older war along the approaches to the lake (decoration only, units drive over them). Every supply pile stands on a gravel depot pad and every start on a prepared apron. `--mapdump` prints the tile map; `ONEHOUR_WORLDBMP=file.bmp` dumps the baked ground.

**Neutral civilian strongholds.** The map has thirteen old civilian strongholds: four **castles**, four **civilian bunkers**, four **watchtowers** (one rotated set per quadrant) and a watchtower on the lake island. Eight smaller settlement buildings join them: a stone **cottage** (2,400 hp, six infantry) and a brick **workshop** (3,200 hp, eight infantry) in each quadrant. They provide garrison cover without income or production. Short worn access tracks lead toward nearby roads. All 21 landmarks are **empty and unowned at the start**. Any army's infantry can walk in: the first soldier inside **claims** the place for their army, its flag changes colour, and the garrison fires from its windows and loopholes with its own weapons. A claimed stronghold is open to its owner and allies; an enemy must break it. The castle (15,000 hp), bunker (11,000 hp) and watchtower (7,000 hp) are heavy masonry: 62% conventional damage and a per-warhead ceiling mean they take a real siege, while no army upgrade applies to them. Right-click a neutral stronghold with infantry selected to send them in; select your claimed stronghold and press **U** to let the garrison out. The structures are placed on fair, path-checked ground and never offered in the build menu. `--forttest` covers placement, claiming, capacity, fire from the windows, toughness, unload and destruction; `ONEHOUR_REVEAL=1 ONEHOUR_CAM=40,43 --shot out.bmp --ticks 1` frames one of the north-west groups.

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
Wraith Drones, Hornet Gunships (attack helicopter), Supersonic Bombers, Anti-Radiation Trucks, Laser Turrets and Patriot Batteries. Stealth and special forces: the Ghost Sniper, Infiltrator (spy), Ghost Drone (stealth spy drone), Atlas Lifter (cargo helicopter) and the Deep Bunker. Tech structure: Data Center (EMP Strike, Orbital Scan, Airlift Drop, Hotline Strike, Aid Drop,
Overclock Program → Ion Lancer and Aegis Titan super-heavy walker; structure upgrades Rugged Nanoshell, Defense Lasers, Nanite Repair).

**Clanker Army** — diesel and gunpowder. Riflemen, RPG troopers, heavy gunners, Brute Tanks,
Gatling Tanks, Rocket Launchers (artillery), Vulture Gunships, Supersonic Bombers, Anti-Radiation Trucks, Gun Nests and Rocket Batteries. Stealth and special forces: the Sniper, Spy, Recon Drone (stealth spy drone), Mammoth Lifter (cargo helicopter) and the Underground Bunker.
Tech structure: Arms Lab (Shell Storm, Recon Flight, Paradrop, Hotline Strike, Aid Drop, Heavy Ordnance → Grenadier and Behemoth; structure upgrades Rugged Bulwark, Bunker Guns, Repair Crews).

**New in the lake pass.** Both armies also have: the **Naval Yard** (Cyber *Nano Drydock*, Clanker *Shipyard*) on the lake shore, which builds warships (**Pulse Destroyer** / **Iron Destroyer**, and with the tech structure the **Lancer Cruiser** / **Rocket Cruiser**
missile ships); the **Jeep** (built at the Assembly Plant / War Factory, the same vehicle in both armies, seven seats); the **Anti-Nuke Laser** (Cyber) and **Anti-Nuke Missiles** (Clanker) defences, small structures that shoot incoming warheads down;
the **Guard Drones** structure upgrade (protector drones for every ship and a second one for every jeep); and two special **strike fighters** (Valkyrie / Warhawk) that every Hotline Strike brings. Each has its own section below. **New in the balance pass**: both armies also build the **Supersonic Bomber** (key `B`, $8000, 40 s, at the airfield once the tech structure stands), a one-bomb nuclear bomber, and the **Anti-Radiation Truck** (key `R`, $1400, 14 s, at the Assembly Plant / War Factory), an unarmed support vehicle that scrubs fallout; see **Supersonic Bomber** and **Anti-Radiation Truck** below.

**Base defenses** (Laser Turret, Patriot Battery, Gun Nest, Rocket Battery) are fortified: 5200 to 6800 hp each (more than double a Barracks' health) and
brutal weapons: the Laser Turret burns 310 per shot, the Patriot Battery fires pairs of 240-damage missiles out to 10.5 tiles (more than double
against aircraft), the Gun Nest hammers out 42-damage rounds seven times a second and the Rocket Battery fires salvos of three, so a few of them hold
a choke point against a large force. Artillery and long-range rails can still outrange them, and a nuke still flattens them (unless they are Rugged).

**Tech structure powers** (select the Data Center / Arms Lab):

| Button | Key | Effect |
| --- | --- | --- |
| Strike (EMP Strike / Shell Storm) | X | call a strike on a circle of your choice (3 min cooldown); also on the Command Core/Post |
| Scan (Orbital Scan / Recon Flight) | V | the whole map and every enemy on it is visible for 30 s (3.5 min cooldown) |
| Paradrop (Airlift Drop / Paradrop) | P | a cargo plane drops **what your army can field at that moment** on parachutes wherever you click (tiered: fresh tech structure → Advanced Program → upgrades; 120/110/95 s per tech structure, see below) |
| **Hotline Strike** (both armies) | I | call in a **swarm** of helicopters, jets and drones on a spot of your choice: they cross the map, attack what stands there and then go home (180 s cooldown per tech structure, see below) |
| Aid Drop (human players only) | D | a white relief plane drops $20000 and a Dozer in crates to the army nearest the spot you click, ally or enemy, never you (1 min cooldown per tech structure, see below) |
| Advanced Program | R | $2500, 45 s (half speed on low power): unlocks the army's two elite units for good |
| Guard Drones (Aegis / Flak) | N | $2500, 40 s: every ship flies with a screen of three protector drones that shoot down rockets and shells aimed at it, and every jeep gets a second drone (see **Protector drones**) |
| Rugged (Nanoshell / Bulwark) | U | $3000, 40 s: every structure gets 50% more health and takes half damage. One nuke no longer flattens anything (a warhead takes at most 55% at ground zero, so it takes two); the plating only gives way when a massive assault lands a big share of a structure's health within a couple of seconds |
| Hyper Protection | H | $6000, 55 s: every structure gets 2x health, takes 60% less conventional damage, and one warhead can never take more than 42% of its max health (even an Underground Nuke): it can no longer erase a full-health protected base in one hit |
| Anti-Rocket | A | $4000, 45 s: every wall gets a 10-tile point-defence laser that shoots down rockets, shells, nuclear warheads and aircraft; unlimited walls give overlapping cover |
| Defense Guns (Defense Lasers / Bunker Guns) | G | $3500, 45 s: every structure gets a roof gun (Cyber laser, Clanker machine gun, 7.5 tiles) that shoots aircraft, vehicles and infantry; half rate on low power |
| Self-Repair (Nanite Repair / Repair Crews) | E | $2000, 30 s: every structure mends itself, 1.2% of its health a second once it has been out of fire for 3 s and a quarter of that under fire (never inside fallout). Units are not repaired: that is still the medics' job |

Structure upgrades are researched side by side, last the whole match and cover every structure the army owns, the ones standing and every one
it builds later, even after the tech structure that researched them is gone. The computer armies buy them too (easy only buys Self-Repair); `ONEHOUR_UPGMASK=bits` limits which ones they may buy, for balance runs).

Team colours: every unit type has its own tone (graphite, cobalt, ceramic and teal for Cyber; olive, sand, brick and iron for
Clanker), a small jewel-like dot in the owner's colour marks each unit, and every finished structure flies its owner's own flag
(rectangle+chevron, swallowtail+roundel, pennant, burgee+cross) from a mast on the roof.

**Airdrop (Airlift Drop / Paradrop)** — select the Data Center / Arms Lab and press `P` (or click the button), then click the map or minimap. A cargo plane flies in from the map
edge along the line from your base through the drop zone and, as it crosses the circle, releases **the force your army can field at that moment**. The load is never a fixed list:
each slot (soldiers, vehicles, aircraft) is filled from that army's own order of battle, cheapest first, as far up as its **drop tier** reaches. A fresh Data Center / Arms Lab
already lands its snipers, a medic, its rail tank / rocket artillery and a gunship — the units the tech structure itself unlocks — and the Advanced Program and every finished
structure upgrade then deepen and enlarge the load:

| Tier | When | Soldiers | Vehicles | Aircraft | Rides along |
| --- | --- | --- | --- | --- | --- |
| **Reinforced** | a fresh Data Center / Arms Lab | 15 | 7 + 3 jeeps | 4 | a medic and a sniper |
| **Battle Group** | the Advanced Program is in, or one structure upgrade finished | 18 | 9 + 4 to 6 jeeps | 5 | a medic, two snipers, a scout drone |
| **Spearhead** | the Advanced Program plus two upgrades (or all three) | 22 | 11 + 8 to 10 jeeps | 6 | two medics, two snipers, a spy, a scout drone |

**Jeeps in the drop**: on top of the vehicle slots every drop carries **three to ten Jeeps, each already holding its seven soldiers**: three from a fresh tech structure, plus one for the Advanced Program and two for each finished structure upgrade
(Rugged, Guns, Repair), which tops out at ten. The crew of a dropped jeep is one rifleman, one sniper, one thrower (Shock Trooper's arc caster / Grenadier's grenade launcher; the Gunner until the Advanced Program is in) and **rocket troopers in the other four seats**
(Laser Troopers for Cyber, RPG Troopers for Clanker); whatever the army cannot field yet is replaced by a rocket trooper. The soldiers ride down under the jeep's canopy and shoot from its seats the moment it lands, so a full Spearhead drop puts ten jeeps and seventy soldiers on the ground at once.

**The fighting slots never carry support**: a medic and a spy do no damage at all and a sniper and a scout drone none against a structure, so they ride along *on top* of the
soldier slots (the last column) instead of taking one — otherwise every unlock would quietly make a drop weaker at fighting than the one before it. Anything the army cannot
field right now (a structure it needs was bombed away, the program is not researched) is skipped for the best thing it *can* field, so a drop never carries a unit you could not
build; a computer army's drops grow exactly the way yours do as its own program and upgrades land, and the button's tooltip lists the exact load the next drop will carry and
the tier it is at. The soldiers and vehicles hang
under parachutes for a few seconds (they cannot be hit or act until they land) and the aircraft take up guard over the zone. The load is free, the power
recharges **per tech structure** (120 s at Reinforced, 110 s at Battle Group, 100 s at Spearhead; first available at 2:30): like nuke ramps, every Data Center / Arms Lab you own has its own cooldown, so three of them can send three airlifts back to back and the button shows `READY x3`. The plane is not invulnerable: every anti-air gun that
reaches it on its way in shoots at it (1100 hp, a few Patriot Batteries bring it down) and a plane lost before the drop takes its whole load with it.
The enemy is told when an airlift is on its way, and the drop circle is public. The computer commander uses it to reinforce a base under attack
and to land on the objective of a wave, avoiding targets with a lot of anti-air. `--droptest` covers the call, the tiered load (nothing the army has not unlocked
can appear, and the same structure sends a bigger and better force once the program and the upgrades land — for a human and for a computer army), the parachute
descent and the shoot-down.

**Hotline Strike (Phone Strike)** — the tech structure's other support power, and the loudest thing in either army's book. Select the Data Center / Arms Lab, press `I`
(or click **Hotline Strike**) and click the map or minimap over whatever you want flattened: an enemy line, a base, a choke point. A **swarm** of real aircraft crosses the map
from beyond the edge along the line from your base and works the spot you picked for about a minute before breaking off home: Cyber answers with Hornet Gunships, Wraith
Drones, Specter Jets and a Ghost Drone; Clanker with Vulture Gunships, Talon Jets and a Recon Drone (half of a full package is helicopters, plus jets and a drone).
**Two special fighters join every call** (Cyber **Valkyrie**, Clanker **Warhawk**): they are lent to the army for one run and cannot be built, selected or commanded. They cross the map at about 900 px/s (nearly twice a Specter Jet), line up on the spot and let a salvo of
twelve unguided rockets go on each of **three passes** (130 damage, 1.5 tile blast each, most of them aimed at what actually stands in the circle), come round and do it again, then leave over the nearest edge. They have 4200 hp and **about 60% of the hits on them miss**, so a couple of anti-air batteries cannot bring them down: they are
meant to be survived, not shot out of the sky. A call also drops a red smoke flare and a pulsing reticle on the spot for 45 s ("AIR STRIKE"), visible to your team and to anybody who has explored the ground; while you aim, the cursor shows the called circle, the rocket scatter circle and the lane the two fighters and the swarm come in along.
The swarm itself, the gunships, bombers and jets, are ordinary aircraft once they are over the spot — they attack whatever stands there, take return fire, and anti-air on the way in can shoot the package down, so a
strike into a wall of Patriot Batteries costs you the swarm. The stealth drone in the package is there to **find what is hiding** (snipers and enemy spy drones) so the
rest can hit it. Free of charge, once per **180 seconds per tech structure** (three Data Centers take three calls back to back and the button shows `READY x3`), first
available when the support powers open up at 2:30. **The swarm grows with the army, on the same ladder as the airdrop**: 9 aircraft at Reinforced, 12 at Battle
Group (the Advanced Program or an upgrade) and 16 at Spearhead, and **every call brings all three kinds** — gunships, jets and a drone — so even the first strike a
fresh Data Center can call is a swarm; a computer commander's strikes grow the same way. The enemy is warned when a package is inbound. Computer commanders call strikes of their own at the
juiciest enemy cluster they can see, and never on top of their own troops. `--striketest` covers the refusal without a tech structure, the per-structure cooldown, the
makeup of the swarm, its growth with the tiers, the flight in, the attack on an enemy line and the calls coming back.

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
(your units and the computer ally's get out of the circle if they can). Every aircraft inside the blast, yours included, goes down at once (the one exception is the Supersonic Bomber that has just dropped its own compact bomb: it pulls away from its fireball), and so does a paradrop plane flying over it.
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
  dozers included (aircraft never try: nothing flying gets out of a nuke), and pick their work up again once it has landed; idle units walk out of fallout (an Anti-Radiation Truck is the exception: it is built to sit in it), guards do not walk back into a poisoned slot, and haulers prefer clean piles.
- Whatever it takes on itself stays **leashed** to its post (about 12 tiles for ground units, 20 for aircraft) and it **returns** there afterwards. Any order you give replaces it.

Units jammed in a one-tile gap between structures (two haulers head-on, a soldier standing in a lane) now slip past each other instead of shoving forever.

**Snipers** — both armies train one at the Barracks / Command Post once the tech structure stands: the **Ghost Sniper** (Cyber, needle rifle) and the **Sniper** (Clanker, rifle).
$550, fragile, long sight, 11.5 tile range: they kill any infantryman in a couple of shots and can shoot nothing else, not vehicles, structures, aircraft or other snipers. A sniper is **stealth**:
nothing sees it, not soldiers, not tanks, not turrets, not even aircraft. **Only an enemy spy drone finds it** (see below), and only for as long as the drone stays within nine tiles;
until then it cannot be selected, targeted or shot (an area blast that happens to land on it, and a nuke, still hurt it). It stays hidden while it fires. The AI builds them against infantry-heavy enemies and
builds spy drones to find yours.

**Stealth, detection and spy drones** — two kinds of things hide from the enemy: snipers and the spy drone itself (class *drone*). Finished underground bunkers are **not** stealthy any more: their surface hatch is plainly visible. A hidden thing is invisible to the
enemy team (not drawn, not on the minimap, not selectable, not a legal target, unknown to the computer commanders; its own side sees it as a faint ghost). It becomes visible only while one of the enemy team's **detectors** is close enough:

| Detector | Finds | Range |
| --- | --- | --- |
| **Spy drone** (Ghost Drone / Recon Drone) | snipers and enemy spy drones | 9 tiles |
| Bomber and attack drones (Wraith Drone, Vulture Gunship, Hornet Gunship) | enemy spy drones only | 3.5 tiles ("your own drones need to be there") |
| everything else (soldiers, tanks, jets, turrets, lifters) | nothing | - |

A sniper or spy drone that leaves the detector's range is lost again after a couple of seconds (three for a sniper, one and a half for a spy drone). Detection is shared by the whole team. An army is **out of the game only when nothing of its own still stands**. Hidden things count: a sniper or a spy drone that nobody has found keeps the game going until the enemy finds it, and so does whatever sits inside a bunker or a transport (the bunker itself is plainly visible and counts like any other building; a lone bunker is not a defeat either).
The **spy drone** ($900, Drone Pad / Airstrip, needs the tech structure) is a small unarmed hovering stealth helicopter, 15 tiles of sight, fragile (220 hp), needs no pad. Because only enemy drones can spot it, anti-air batteries and fighters never shoot at it
until a drone of the enemy is practically on top of it. **Autonomous swarm intelligence**: when a Ghost Drone or Recon Drone spawns (or is called to a spot by the player), it holds briefly for player orders, then autonomously sweeps the map to hunt hidden enemy elements (hidden snipers and enemy spy drones). The more spy drones you or the AI field, the more they coordinate as an intelligent swarm — dividing sectors, avoiding overlap, and locking onto hidden enemy targets.

**Spies** — the **Infiltrator** (Cyber) and the **Spy** (Clanker): $650, Barracks / Command Post, needs the tech structure. Unarmed, fragile, quick. Select one and right-click an enemy structure: it walks up and **captures it**
after 4 s + 0.7 s per footprint tile (8 s for a Barracks, 15 s for a Command Core); a progress bar shows over the spy and the owner gets an alarm. The structure changes hands with everything on it: the production queue is
refunded to the old owner, a bunker's garrison is turned out onto the ground, its health carries over. The spy **survives** the capture and can take the next one; if it is killed first, nothing changes. A captured production structure
trains the new owner's units, a captured power plant, supply hub, tech structure or nuke ramp works for its new owner. The AI sends two spies (Hard and up) after valuable structures nobody is guarding.

**Cargo lifters** — the **Atlas Lifter** (Cyber) and the **Mammoth Lifter** (Clanker): $2600, Drone Pad / Airstrip, needs the tech structure. A huge tandem-rotor helicopter, 2400-2600 hp, unarmed, 40 cargo places (soldier 1, light vehicle 4, tank 6,
super-heavy walker 10: for example 30 soldiers, a tank and a light vehicle, or six tanks and four soldiers). Select soldiers or vehicles and right-click the lifter: they walk to it and climb aboard while it hovers. Select the lifter and press **U** to set down where it is and let
everybody out, or **D** (Drop Off) and click a spot: it flies there, sets down and unloads (soldiers first, vehicles after). A lifter shot down in the air takes everyone aboard with it; one on the ground lets them climb out. Use it to put a spy, a dozer or an army behind the enemy lines.

**Underground bunkers** — the **Deep Bunker** (Cyber) and the **Underground Bunker** (Clanker): $7500, built by a dozer, needs a Barracks. Only a small steel hatch (2x2 tiles) shows on the surface; the bunker is **not stealthy** and is plainly visible to both sides (no detector is needed). It holds **50 infantry and one dozer** (right-click the bunker with soldiers or a dozer selected; U unloads, the rally point is where they walk to). Everybody inside is untouchable. In a fight the whole garrison **fires from the hatch**, each soldier with its own weapon (rocket troopers
hit tanks and aircraft, snipers pick off infantry), and heals slowly. The rest of the rules:

- the **hatch** (3200 hp) takes the damage first; once it is blown off the garrison cannot shoot, though it stays safe. A **nuke wrecks the hatch and hurts the bunker**, and enough direct hits destroy it: warheads ignore the buried floor. Aircraft, bombs, turrets, shells and fallout still cannot take it below 10% of its health;
- only **infantry and vehicles can finish a bunker by conventional fire**, and it is still a grind: **11,000 hp** and plating that stops 75% of what gets past the hatch (a column of fifteen tanks needs a while; it is plainly visible, so no spy drone is needed);
- a **dozer inside** patches the hatch (6% a second) and the bunker once out of fire for three seconds; any dozer next to it mends it too;
- when the bunker is destroyed (or captured by a spy) **the whole garrison is back on the ground, exposed**; it still counts, so the army is only beaten once the bunker **and** everybody who was inside are dead;
- aircraft and turrets stop picking a bunker whose hatch is gone and whose body sits at the floor: they cannot hurt it any more, so they no longer hover over it (ground units keep grinding it).
- **a bunker runs entirely by itself**, with no power, no other structure, no supplies and nobody inside it (a lone bunker with the rest of the army gone keeps all of this going):
  - **income**: $85 every 5 seconds (a Bitcoin Datacenter pays $450 / 5 s, an Oil Well $380): small, but a few bunkers over a long game add up. It counts as mined money;
  - **self-repair**: once left alone for 6 seconds the body mends 0.22% of its health a second and the hatch 0.75% a second. A hatch that was blown off is refitted by the bunker itself 30 seconds after the last hit (a dozer does it faster);
  - **two miniguns** on plates either side of the hatch (16 damage, a burst every 0.06 s each, 6.8 tiles): each picks its own target and shoots **vehicles, aircraft and infantry** (not structures). Like the garrison they sit on the hatch: while it is blown off they are silent.
  - **it fights back when it is worn down**: for 6 seconds after every hit a **rocket pod** pops up (shown on the plate) and fires pairs of anti-armour/anti-air rockets (98 damage, every 2.4 s, 10.5 tiles) at whoever is attacking it: the last unit to hit it and anything that has it as its target, **even with the hatch blown off** and even an artillery piece beyond its sight. It never fires at an enemy that leaves it alone, so it does not pick fights it was not dragged into. While the hatch is up the miniguns go for those attackers first, and they **swat rockets** aimed at the bunker that come within 4.2 tiles;
  - **surrounded overdrive & automatic radiation defense**: when surrounded by enemy forces (3+ armed enemy units within 9.5 tiles), the bunker activates **overdrive laser / heavy quad-machinegun mounts** that shred aircraft, incoming nuclear warheads, tanks and infantry, and when taking heavy hits while surrounded it **automatically vents a ring of enemy-only radiation** around itself;
  - **anti-nuke defense**: bunkers engage incoming enemy nuclear warheads in flight within 16 tiles using their overdrive lasers and heavy guns (guaranteed interception when in surrounded overdrive mode);
  - **mini nuke salvo**: every launch is **two missiles, 1.2 s apart, at the same spot**, each with **a fifth of a Nuke Ramp's blast area** (radius 11 x sqrt(0.2) = 4.9 tiles, the same strength at each spot: it still flattens everything near ground zero, wrecks the hatch of an enemy bunker and leaves a small cloud of radiation). Ready 30 seconds after the bunker is finished, then once every 65 seconds per bunker, from any number of bunkers, with no ramp and no power. Select a bunker (or none, any ready one fires) and press `K`, then click the target; like a full nuke it hurts your own and your allies' ground units in the circle. The climbing missile **cannot be missed**: a launch is the loudest thing a bunker ever does. Computer commanders also build their own bunkers in creative flanking corridors, tree/rock cover, and expansion overlooks — never next to player bunkers.

  - **it watches over its neighbours** (the civilian town above all): every friendly structure within **9 tiles** is under its wing. When one of them is hit, the bunker treats whoever is shooting at it as an attacker of its own: the **rocket pod** pops up and the **hatch guns** go for that attacker first, out to the pod's 10.5 tiles (beyond the miniguns' reach). Once the shooting has stopped for 5 seconds its crew **mends** those structures (0.6% of their health a second). Its **seismic sensors** feel hidden soldiers: an enemy **sniper within 7 tiles is revealed** to the bunker's team (a spy drone in the air goes unfelt: the sensors are for soldiers, not aircraft).

**The civilian town** — every army can raise a town next to its base. The Command Core / Post trains a **civilian dozer** (*Civic Fabricator* / *Civil Dozer*, key V, $1000 like a dozer, painted civic white and safety orange). Civilian dozers build **only** the town and ordinary dozers build **only** military structures; a dozer mends anything, a civilian dozer only the town (and it cannot take a bunker's dozer slot). Selecting a civilian dozer shows the town's build menu:

| Structure | Key | Cost | Size | Pays every 10 s | Free dozer every | Townsfolk |
| --- | --- | --- | --- | --- | --- | --- |
| House | H | $600 | 2x2 | $30 | 480 s | 3 |
| Apartments | A | $1200 | 3x2 | $65 | 400 s | 7 |
| Skyscraper (needs a Town Hall) | S | $4000 | 3x3 | $220 | 280 s | 10 |
| Gas Station | G | $900 | 3x2 | $50 | 440 s | 4 |
| Grocery Store | R | $1000 | 3x2 | $55 | 420 s | 7 |
| Event Hall (concerts, weddings, fairs) | E | $1600 | 3x3 | $90 | 340 s | 10 |
| Town Square (where the townsfolk meet) | Q | $700 | 3x3 | $35 | 400 s | 12 |
| Town Hall (one per army) | T | $3000 | 4x3 | $170 | 220 s | 8 |
| Government Office (needs a Town Hall) | O | $3500 | 4x3 | $190 | 260 s | 7 |

- **It runs on its own**: no power, no supplies, nobody to manage. Each structure pays its army every 10 seconds (it pays for itself in about three minutes) and sends out a **free dozer** at its interval, a military dozer and a civilian dozer in turn, as long as the army has fewer than 5 dozers of both kinds together. Civilians do not keep a beaten army in the war: an army whose last things are civilian dozers is out (its town's structures do count, and can still send it a military dozer). Up to 30 civilian structures per army. The info panel shows its pay, its townsfolk and when its next dozer comes out.
- **It cannot defend itself**: no guns, no plating (the Rugged and Defense Guns upgrades skip it), the full damage of every hit lands. Only the army's military protects it: turrets, Patriot / Rocket Batteries, anti-nuke towers, bunkers (see above) and troops placed around it. The enemy's AI picks civilian structures as targets (they pay) and sends its spies after them.
- **Spies capture it like any structure**: a captured house pays its new owner from then on.
- **Townsfolk**: every civilian structure has its people (3 at a house, 12 on the Town Square) who stroll in pairs along the front and sides of the building (on the plaza of the Town Square and the Event Hall), stop to chat with a speech bubble over one, then the other, and walk on; under fire they hurry and nobody stops to talk. They are decoration only: drawn from the clock, never units, nobody can shoot them.
- The computer armies build a town too, a small one late in the match (2 structures on Normal, 3 on Hard and Brutal: a civilian dozer once the base runs, then the best pay for the money first, behind the base, only with money to spare), and their attack waves raze enemy towns like income structures. Bigger, earlier computer towns made AI-vs-AI wars stall: with the town's pay and free dozers both sides rebuilt faster than they could be flattened (`--selftest 2400`, 4 armies: 17 of 30 games undecided instead of 6). With the small late town, 25 of 60 games are undecided against 20 of 60 before the town, within the noise of the measurement.

**Underground Nuke** — a buried silo for both armies (key G with a dozer, **$50000**: ten times a Nuke Ramp, needs a Nuke Ramp, 60 s to build, 6 power). It fires the same kind of warhead as a ramp (60 s arming, then one every 5 minutes, 7 s flight, key **K** with the silo selected, as many silos as you can pay for), but **silver** and far stronger:

- **3x the blast radius** (33 tiles instead of 11), **2.7x the damage** (10% weaker than it used to be, still enough to flatten a large structure at ground zero, a Rugged one included), and **radiation 3x as strong that lasts 3x as long** (240 s — an **Anti-Radiation Truck** can scrub it away in seconds, see below). The warhead, its exhaust, the warning circle, the fireball, the mushroom cloud and the radiation glow are all pure silver-grey.
- **Only laser defences can stop it**: the Anti-Nuke Laser and an Anti-Rocket wall's point-defence laser. Anti-Nuke Missiles and the bunkers' own anti-nuke guns let it through; a laser engages it like any warhead (its blast reaching into the laser's cover is enough), rolling its **warhead chance** (the Anti-Nuke Laser's is 40% lower than it used to be, a wall's 70% lower) and then **halving it for the armoured silver casing** (a lone Anti-Nuke Laser: 9%; a lone Anti-Rocket wall: 5.7%), and a warhead the lasers shoot down still drops 30% of the damage, which for a silver one is 90% of a ramp's.
- **Second only to the Command structure**: **14,000 hp** (Clanker 14,500) and **5,000 hp of blast doors**. The blast doors take the damage first, the plating stops 75% of the rest, and aircraft, structures, shells and radiation can never take it below 10% of its health: only soldiers and vehicles can finish it by conventional fire. Warheads ignore the floor, so repeated direct hits destroy the silo too (it is a hardened bunker, not a vault nukes bounce off). It mends itself like a bunker and refits blown doors after 30 s. The armed warhead shows in the open doors.
- A rich computer army (Normal and up, with a Nuke Ramp and over $53000 in the bank) sinks one silo (two on Brutal) and fires the silver warhead at a target worth twice what a ramp's warhead needs.

**Veterancy and rocket infantry** — infantry earn promotions: **Veteran** after 4 kills (+25% health, +20% damage) and **Elite** after 10 (+50% health, +40% damage), shown as chevrons beside the health bar, in the bunker as well. The **RPG Trooper** is the rocket infantryman:
its rocket hits **vehicles and aircraft hard** (64 damage, 1.3x against tanks, 1.6x against aircraft, faster missile) and works from inside a bunker alongside the rest of the garrison. `--stealthtest` covers all of the above.

**Aircraft** — each army has a cheap, fast, fragile bomber (Wraith Drone / Vulture Gunship: $850, 520 / 560 hp, 340 / 300 px/s) and an expensive, fast, powerful fighter-bomber
(Specter Jet / Talon Jet: $2400, 1500 / 1550 hp, 560 / 540 px/s, 10 passes a sortie). Both kinds hit air and ground: the bombers carpet-bomb the ground and shoot aircraft down with
their guns, the jets use heavy plasma lances / homing Sidewinders on everything (about 85-100% against structures) and fight aircraft on the turn.
Once the tech structure stands, both armies also build the **Supersonic Bomber** (key `B`, $8000, 40 s, 1000 hp, one nuclear bomb a sortie): the fastest
aircraft in the game while its bomb is aboard, and a slow sitting duck once it is gone (see below).
The Cyber Drone Pad also builds the **Hornet Gunship** (key `H`, $950, 640 hp, 280 px/s), an electric attack helicopter whose twin lasers hit ground and air;
it never needs to rearm.

**Helicopters have no limit** — an airfield has **25 pads**, one per fixed-wing aircraft (drones, jets and nuclear bombers), and a plane waits in the queue until a pad is free.
Helicopters (Hornet Gunship, Vulture Gunship) do not take a pad: they land on helipads in rings around the airfield, so one airfield builds and houses as
many of them as you can pay for. The airfield's panel shows `Planes n / 25` and `Helicopters n (no limit)`. **Helicopters never pile up**: wherever several are sent to
the same place (one move order, a rally point, a crowded guard zone, a shared helipad), each one hovers on a spot of its own a small gap from the next (they pack in
hexagonal rings around the spot, and the one already there keeps it). **Guarding an area, a helicopter hovers still** on its own post in the circle, nose turned out,
instead of flying circles like the planes; it goes after what enters the zone and comes back to hover on its post.

**Flight** — the jets, the Supersonic Bomber and the Wraith Drone (a flying wing) are **fixed-wing aircraft** and fly like aeroplanes: they hold an airspeed between a stall speed (36% of their top speed)
and their top speed, accelerate and brake at finite rates, and turn under a g-limit, so the slower they fly the tighter they turn (up to twice their turn rate at top speed). They
**never stand still in the air**: an aircraft waiting somewhere circles the spot, and only the final approach to the pad is flown below the stall speed. Sent somewhere with a
move order, aircraft circle there for a minute (picking off what they see) before flying home; after a kill they look around for a few seconds for more. The Vulture Gunship
and the Hornet Gunship are helicopters and hover. **Dogfights**: a fixed-wing fighter flies lead pursuit, pulls round at its corner speed (where it turns tightest) whenever the target is off its
nose, slows to the target's pace when tucked in behind it, extends straight out when the target sits inside its turn circle behind the wing and comes round again, and fires
whenever the target is inside its weapon cone (wide for homing missiles, narrow for lasers). It stays on the target after a burst instead of breaking away, so a jet out-turns
and shoots down bombers and helicopters, two jets fight each other properly, and an aircraft being shot at by another one turns on it. Aircraft near the map edge pull round
toward the middle in time, and a plane that loses its airfield on final approach goes round again.

**Bombers** — the Wraith Drone and the Vulture Gunship are the carpet bombers (the Supersonic Bomber is a nuclear bomber, below). Against anything on the ground they fly real bombing runs: they line up on the
target (leading a moving one), release a stick of four heavy bombs a few tens of pixels apart along the flight line, fly straight on for a second and
swing round for another pass (12 / 16 bombs per sortie, reloaded in under half a second per bomb on the pad). A bomb splashes two tiles, hits
structures about one and a half times as hard as soldiers' small arms, and goes off in a fireball with secondary bursts, a shockwave ring, flung debris,
a dust ring, a climbing smoke column and a scorch mark. A flight of three takes a factory down in one or two passes, so anti-air batteries matter.
Against aircraft they keep using their guns. They are fragile on purpose (520 / 560 hp): their price is low and they are fast.

**Supersonic jets** — the Drone Pad and the Airstrip each build another aircraft once the tech structure stands: the **Specter Jet**
(Cyber, stealth delta, plasma lances) and the **Talon Jet** (Clanker, swept-wing fighter-bomber, homing Sidewinders). They are
fast (about 550 px/s, 16 tiles a second; only the Supersonic Bomber and a Hotline Strike's fighters are quicker) and fly like fixed-wing aircraft (see Flight above): no hovering, a turn
circle of ~165 px at full speed and much tighter at corner speed, strafing passes that dive on the target, fire a stream of rounds, break
away and come round again. They are fragile, shred aircraft and light vehicles, are poor against structures, take off from and land on the pad, and
leave afterburner flames and vapour trails behind. Idle jets scramble against anything that comes within 11 tiles of their field.

**Supersonic Bomber** — both armies build it at the Drone Pad / Airstrip with the tech structure (key `B`, $8000, 40 s, 1000 hp, one per pad). It is a
small fixed-wing nuclear bomber and, while its one bomb is still aboard, **the fastest aircraft in the game: 960 px/s**, against a Specter Jet's 560 and a
Hotline Strike fighter's 900. It flies like every fixed-wing aircraft (see **Flight** above): it lines up on its target, makes one pass, releases its bomb
and flies straight on; for **about 1.2 s after the release it holds the dash**, just long enough to clear the fireball it has just made, and then drops
to a **340 px/s** cruise and is easy prey for anti-air until it gets home. On its pad it **rearms its one bomb** and gets the dash back for the next
sortie. It also carries a **tail cannon** (26 damage, 6 tiles, ground and air, about twice against aircraft) so it can answer fighters on the way home:
it is a bomber with a sting in the tail, not a dogfighter. Computer commanders build it too.

The bomb is a **compact casing around a silver warhead**, and it is aimed at the target: it falls for about a second and goes off with an **Underground
Nuke's damage** (2.7x a Nuke Ramp's, which levels a large structure in the inner blast, Rugged or not) inside a **Nuke Ramp-sized fireball**: an 11-tile
blast radius where the silo warhead has 33, so the fireball is a third of the silo's and a target more than 11 tiles from ground zero is untouched by it.
The radiation is a silver warhead's: **three times as strong and lasting three times as long** as a ramp's, and an **Anti-Radiation Truck** can scrub that cloud away like any other fallout. The bomber is **the one aircraft its own bomb
does not bring down** (every other aircraft in the blast falls, but the plane that dropped it is already pulling out); the bomb itself is a real falling
object, so a Q defence or an Anti-Rocket wall whose envelope it crosses can **burn it out of the air like any other bomb**, and a sortie can be lost
before it lands.

**Rebuilding a base** — the Command Core / Command Post is no longer unique: any dozer can found a new one ($3000, 45 s) from its build menu
(key `C`, last entry), so a player who keeps a dozer alive and has the money can rebuild a lost base from nothing, or raise a second
one as an expansion (three at most at a time). A Command Core is the only source of dozers, strike powers and the home position: when
the last one stands the camera Home key and the AI's base layout move to the new one. The AI does the same: it banks the price and
sends its dozer when its Command Core falls.

**Dozer priorities** — an idle dozer looks for work on its own in this order: **the sites it was pulled away from**, then **any unfinished site** of its army (one with fewer than two builders, not under fire; idle dozers fan out over the open sites), and only then **repairs**. A dozer that went mending on its own drops it for a site nobody is building; a repair you ordered is kept.

- **Build orders persist**: move a dozer off a site it is building (or give it another site or job) and it remembers the unfinished site (up to four) and drives back to finish it as soon as it is idle; a dozer that flees under fire or ducks out of a nuke's circle goes back too, once the site is no longer being hit. `S` (Stop) is the way to drop them.
- **A manual order holds autonomy off**: after you move a dozer (or point it at a job, or stop it) it does no repair rounds of its own for 12 seconds after it arrives, so it stays where you put it. Going back to its own kept site is not held.
`--dozertest` covers it.

Both sides: Command Core/Post trains dozers, power plants provide 10 power, Supply Hub/Depot builds
haulers that carry $300 per trip from supply piles. Running out of power halves production and
switches powered defenses off, exactly the pressure Zero Hour puts on you.

**The lake and the navy** — the **Naval Yard** (Cyber **Nano Drydock**, Clanker **Shipyard**, key `Y`): $2200, 28 s, 3x3 tiles, needs the Assembly Plant / War Factory, at most three per army. It must **touch the open lake** (a body of at least 60 water tiles with room to sail out into:
ponds and rivers cut off by a bridge do not count; the build ghost outlines the open water it touches and says so when it does not) and it grows a pier into the water on the side that faces it. Ships leave the yard into the lake; the rally point of a yard is a place on the water.
Every dozer can reach the shore, but the ground there has to be explored first (scout it, or send the dozer).

| Ship | Cost / time | Needs | Health, speed | Weapon |
| --- | --- | --- | --- | --- |
| **Pulse Destroyer** (Cyber) | $1800, 18 s | Naval Yard | 1500 hp, 72 px/s | twin Pulse Cannons: lasers, 78 damage a barrel, 8.5 tiles, ground and air |
| **Iron Destroyer** (Clanker) | $1800, 18 s | Naval Yard | 1600 hp, 68 px/s | twin Naval Guns: 92 damage shells with a 0.7 tile blast, 9 tiles, ground and air (little against air) |
| **Lancer Cruiser** (Cyber) | $2400, 24 s | Naval Yard + Data Center | 1200 hp, 56 px/s | Cruise Missiles: salvos of three guided rockets, 125 damage, 1.25 tile blast, **14 tiles** (nothing closer than 3), ground only |
| **Rocket Cruiser** (Clanker) | $2400, 24 s | Naval Yard + Arms Lab | 1300 hp, 54 px/s | Rocket Barrage: six rockets, 96 damage each, 1 tile blast, **13.5 tiles** (nothing closer than 3), ground only |

Ships are heavy-armoured vehicles that live on water only. A ship has weight: it turns at a limited rate (about 60 degrees a second), builds up speed along its heading and eases off before a stop, leaves a bow wave, two foam lines and a churned wake, and sails over open water the whole way
(an A* over open water that keeps off the shore; a ship sent to a land spot sails to the nearest open water, and one sent to shell a target goes as close as it can get). Destroyers fight ships and anything on the beach; cruisers shell the shore, the ring road and the central supply piles from the middle of the lake and are poor close in.
Rifles are weak against a hull; tank guns, rockets, aircraft and artillery are the answer, and every ground weapon that reaches a ship from the shore can hit it. **A nuke sinks every ship inside its circle**, whatever its health, and so does a bunker's mini nuke. A ship that goes down breaks up in a string of blasts and sinks into an oil slick.
`--navytest` covers the lake, the yard (placement rules, production, exit into the lake), sailing, ship-versus-ship and ship-versus-shore fights, and nukes sinking ships.

**Jeeps** — the **Jeep** (the same vehicle in both armies, only the look differs: Cyber graphite and cobalt with cyan emitters, Clanker olive drab with rust and hazard paint): $650, 9 s, Assembly Plant / War Factory, 420 hp, the fastest ground vehicle (126 px/s), light armour and a pintle gun
(12 damage, 5.5 tiles, ground and a little air). **Seven seats for infantry only**: right-click a jeep with soldiers selected and they run to it and climb in while it stands still (`U` unloads them; non-infantry cannot board, and a loaded jeep cannot be lifted by a cargo chopper).
**The soldiers shoot from their seats with their own weapons**, moving or not, at the best target each can hit from where the jeep is: rifle bullets, rocket troopers' rockets on tanks and aircraft, grenadiers' grenades, arc casters, a sniper's long shot (11.5 tiles, infantry only) and so on; they can be hit only through the jeep.
A jeep that blows up kills about a third of its crew and the rest climb out hurt; one caught in a nuke takes the whole crew with it. A jeep flies with one **mini guard drone** (two once the Guard Drones upgrade is in). Veterancy works from inside the seats. The helmets on the seats and the muzzle flashes are drawn live. Dropped jeeps come loaded: see the airdrop above. `--jeeptest` covers boarding, the capacity, firing from inside (including the sniper and moving), unloading, casualties, and the drones.

**Protector drones** — the **Guard Drones** upgrade ($2500, 40 s at the tech structure, key `N`; *Aegis* for Cyber, *Flak* for Clanker) gives **every ship three protector drones** and every jeep a second one (a jeep has one from the start). They circle their host and **shoot down rockets and shells homing in on it**: a drone has two interceptors a quarter of a second apart (85% to hit), then reloads for 2.4 s;
lasers, bullets, bombs and blasts are too quick or too wide for them. Three drones stop all six rockets of a lone Rocket Cruiser's barrage; a whole fleet's salvos saturate the screen, and a pulse that switches the host off switches its drones off too. The selection panel shows how many are ready. `--dronetest` measures it: the same cruiser against the same destroyer does 1181 damage bare and none with the screen,
a shell-firing destroyer 1352 against 386, and lasers 5242 against 5242.

**Anti-nuke defences** — the **Anti-Nuke Laser** (Cyber, $1900, 20 s, 2x2, 1900 hp, power -5, a tall pylon with a focusing lens) and the **Anti-Nuke Missiles** silo (Clanker, $1800, 20 s, 2x2, 2100 hp, power -2, four interceptors in an open hatch), key `Q`, need the tech structure; at most eight per army. A defence watches the sky: a warhead heading for a spot
**inside its cover** (17 tiles for the laser, 19 for the missiles; the circle shows when it is selected, hovered or placed) is engaged: the silo when the warhead is 4.6 s from impact (two interceptors 1.5 s apart that take a second to arrive), the laser earlier, 6.6 s out (three beams 0.8 s apart). **Every defence rolls its own chance at each warhead, so the more of them cover a spot the likelier it is stopped: the Clanker missile silo keeps its 30% (1 stops 30%, 2 stop 51%, 3 stop 66%, 4 stop 76%, 6 stop 88%, 8 stop 94%), while the Anti-Nuke Laser's chance against warheads is now 40% lower (18% base: 1 stops 18%, 2 stop 33%, 3 stop 45%, 4 stop 55%, 6 stop 70%, 8 stop 80%).** An **Anti-Rocket wall's point-defence laser** rolls against a warhead too, at 11.4% base (70% weaker than its 38% against rockets, shells, bombs and aircraft, which is unchanged). **Against the Underground Nuke's armoured silver warhead every defence's chance is halved again**: the Anti-Nuke Laser's 18% becomes 9% (1 stops 9%, 3 stop 25%, 6 stop 43%, 8 stop 53%), and an Anti-Rocket wall's 11.4% becomes 5.7%; the Clanker Anti-Nuke Missiles cannot engage a silver warhead at all. A missile silo reloads for 12 s after engaging (so a second warhead right behind the first meets silos that are still reloading). Defences need power (an unpowered or EMP-ed defence is blind) and cover mini nukes from bunkers exactly as they cover a Nuke Ramp's warhead.
The **Anti-Nuke Laser has no blind spots**: it also engages a warhead aimed outside its circle whose blast would reach into it (17 tiles plus the blast radius: 28 for a full warhead), and its capacitors recharge 0.8 s after its last beam, so it takes on every missile of a salvo. **Lasers stack**: towers within 9 tiles of each other link up (up to three links each), and every link gives a tower one more beam, fires its beams closer together (0.8 s / (1 + 0.4 x links)) and adds 9% to its own chance (a tower in a cluster of four: 57% against rockets and shells, 34% against warheads and 17% against a silver one, and six beams). The selected tower shows its beams, its chance and its links.
A warhead that is hit blows up in an airburst high above the ground, and **only its pieces fall on the target: 30% of the damage it would have done** (30% of what each unit, structure and aircraft in the circle would have lost, never more than it had left) and a radiation zone at 30% strength, with scattered blasts instead of a fireball and a mushroom; the message says "Warhead shot down: its pieces and radiation will still fall". The armoured silver casing does not change what falls: a stopped silver warhead still drops 30% of its damage.
While you aim a nuke the cursor tells you the cover the enemy has there (only the defences you have seen) and the chance it is shot down. `--antinuketest` fires a few hundred warheads at a ring of defences and checks the rates (the Clanker missile ring stops about 30%, 66% and 88% with one, three and six silos, the Cyber laser ring about 18%, 45% and 70%), mini nukes, the cover circle, power and EMP, the reload,
and that the pieces really do 30% of a barracks and of a tank at ground zero where a whole warhead does 100%.

**Anti-Radiation Truck** — both armies build it at the Assembly Plant / War Factory (key `R`, $1400, 14 s, 620 hp, light armour, 115 px/s,
unarmed). It is the answer to what the warheads leave behind: a **6-tile decontamination field** that **scrubs any fallout zone it touches** and
**shields every unit and structure inside it** — yours, your ally's and the enemy's, the field does not pick sides — from radiation. The zone itself
shrinks while a truck works on it: its strength and radius decay exponentially, fast at first and then the last wisps, and the hazy green (or silver)
cloud shrinks with them. One truck washes a tactical crater out in **about 11 seconds**; a silver warhead's cloud starts three times as strong, so it
takes a few seconds longer, about 14; **two trucks stack** and halve that. This is the one way to end a crater's life early: the radiation of a Nuke
Ramp's crater (80 s) and of a silver warhead's cloud (240 s) can be scrubbed away in seconds instead of outlasting the battle. A zone the truck cannot
reach is untouched, and it cannot stop an incoming warhead: it only cleans up after one.

The truck pays for that in its own hull: it absorbs the radiation it neutralises, about **5% of its max health a second** in a full-strength tactical
crater and about **8.7% a second** in a silver one (the dose scales with the square root of the cloud's power), capped at 20% a second so a cloud cannot
dissolve it in one go. A full-health truck that sees one whole tactical crater through comes out with **about three quarters of its health**, and one that
sits through a whole silver cloud with a **bit over half** (about 55%). A **medic** heals it back up; dozers repair structures, not vehicles, so a truck
that gets too hot has to be pulled out or patched by a medic. Its field is **off while the truck is EMP-ed or stowed** (loaded into a cargo lifter), and
a truck parked away from any fallout does nothing: the field works where the truck is, so it has to be taken to the cloud.

Select a truck to see its field; the info panel says if it is standing in a zone and warns that it is taking damage. The computer commanders build one
to three of them once enemy warheads have been seen or fallout is already on the ground (from Normal up, more after more warheads and on Brutal), and
send an idle truck to the nearest fallout within 70 tiles of it to scrub it; they never send one with an attack wave. `--radtest` covers the field's
shielding, the scrubbing and the silver cloud's heavier dose.

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
| Dozer selected | build menu with hotkeys (including `C`: a new Command Core / Post, `U`: Underground Bunker, `Y`: Naval Yard on the lake shore, `Q`: anti-nuke defence); click the ground to place, Shift to place several. With more than twelve buttons the grid shrinks to five compact rows |
| Bunker selected, `K` then click | launch the bunker's mini nuke salvo (two missiles 1.2 s apart, each a fifth of a full warhead's area; 65 s per bunker) |
| Spy selected, right-click an enemy structure | capture it |
| Soldiers / dozer selected, right-click your bunker, cargo lifter or **jeep** | climb aboard (a jeep takes seven soldiers, and only while it stands still); with the bunker, lifter or jeep selected `U` unloads, lifter `D` then click: fly there and drop everybody off |
| Ships selected | right-click the lake to sail (a land spot sends them to the nearest open water), attack-move, guard area and force fire work as for tanks; ships pick by their whole hull |
| Naval Yard selected | train ships, right-click the lake to set where new ships sail to |
| Structure selected | train units (hotkeys shown), Del sells |
| X | strike power (from the tech structure, or the Command Core/Post once it exists); V map scan, R Advanced Program, P paradrop, I hotline strike, D aid drop, U / G / E / N / H / A structure upgrades (N: Guard Drones, H: Hyper Protection, A: Anti-Rocket) with a tech structure selected |
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
  out of the circle (its aircraft stay and burn); afterwards non-attacking units leave radiation zones, rally points move out of them and attack waves avoid targets inside them, while its idle anti-radiation trucks drive into the fallout to scrub it away.
- **Bombers**: a flight waits until it is loaded, then goes together for the target with the best value (nuke ramps, income structures, supply, power, then production; harvesters;
  clusters of units) minus the flak over it; heavily defended sites are skipped.
- **Anti-air**: batteries follow the enemy's aircraft and airfields and are placed beside the structures most worth protecting.
- **Medics** (one per 14 fighters, up to three) trail the army and are never sent ahead as part of a wave.
- **Waves** break off when the fighting around them is clearly lost (two bad readings in a row), not only when half of them are dead.
- **Special forces**: from Hard (one spy drone from Normal) the commander keeps spy drones with its army, since they are the only thing that finds hidden snipers (and enemy spy drones). When the enemy has only hidden things left, every level builds one and the army sweeps the ground around the enemy base until the drone finds them (the hunt). It trains two spies that go for the most valuable structure nobody is guarding (`AiFeat` `FEAT_SPECIAL`: over 88 mirror games it beat the same commander without it 14-2, 72 unresolved). It does not use cargo lifters or bunkers itself.
- **Allies**: a computer ally defends *your* base too (it sends most of its army to wherever enemy fighters are closing on a teammate's structures), shares its vision with you, and
  plays at the difficulty set for it. When *you* push out (five or more of your fighters away from home, near an enemy structure) it moves its army up to the same front and picks its next targets near it,
  telling you so ("Ally: joining your attack", "Ally: sending units to defend your base"). `--allytest` covers it.
- **The navy** (`FEAT_NAVY`): from Normal up, once it has a factory, power and a bank (from minute 7.5 on Hard and Brutal, 10.5 on Normal, and more than $5200), the commander raises a Naval Yard on the
  shore (the site avoids the ring road and bridge decks, and keeps the one-tile lane open) and keeps a fleet of 3 + difficulty ships (two more once the enemy has ships, one more with a very full bank): destroyers first,
  a cruiser for every two destroyers once the tech exists, destroyers only while an enemy navy is about. The fleet gathers at a post on the lake toward its own base, picks the closest visible target the guns can reach from the water
  (ships first; a land target must be within 12 tiles of open water and not behind a wall of guns), sails to the yard when it is hit, and otherwise regroups at its post. It buys the Guard Drones upgrade once it has two ships or jeeps to screen, and keeps a few jeeps: idle
  infantry beside a standing jeep climb in (the anti-armour troopers first) so the column rolls out with its soldiers riding.
- **Anti-nuke** (`FEAT_AEGIS`): once it has seen an enemy nuke ramp or a warhead (or the clock passes minute 10 on Brutal, 15 elsewhere) a commander with power and a tech structure raises lasers or missile silos in a ring round its base: 1 / 2 / 3 by difficulty,
  one more after a warhead has been seen, one per two warheads, one for a second enemy ramp, one with a bank over $9000 (never more than the cap). Its own nuke targeting discounts a spot by up to 80% of the enemy's chance to stop the warhead, and
  it fires a *salvo* of two at a covered target (the defences that engage the first reload before the second arrives, so one of the two gets through; with one ramp it waits up to 40 s for a second).
- **Recovery** (`FEAT_RECOVER`): an army that has lost most of its base (its Command structure and one more core piece, or two of: supply hub, power, barracks, factory) switches to recovery mode as soon as it has a dozer and money for it (a relief drop, a bunker's trickle, what its haulers still bring in): every free dozer, not one per decision, raises the next missing core piece it can pay for (the Command structure, a hub, which comes first while the Command structure is out of reach, power, a barracks, a factory), the Command structure trains up to three dozers even on a small bank, soldiers are only bought with what is left over the price of the missing core, and no attack wave leaves while it lasts. A dozer sitting in a bunker is let out when no other dozer is free (it used to be sent to a site it could never reach, which stalled the army for good). `--recoverytest` strips a computer army to three soldiers + a relief drop, to its Command structure + $6000, and to a bunker with a dozer inside + $15000, and checks that each rebuilds a working base.
- **The town** (`FEAT_CIVIC`) and **the Underground Nuke** (`FEAT_DEEPNUKE`): see the civilian town and the Underground Nuke above. A civilian dozer never counts as one of the commander's dozers (it cannot raise the base). `ONEHOUR_FEATOFF=bits` switches pro features off for every computer army in a headless run (`ONEHOUR_FEATOFF=512 --selftest 2400` plays without towns).
- **Last resort**: when no enemy structure or hauler is left in view, an attack wave goes for the nearest enemy combat unit it has seen instead of idling (then for any other unit: a last dozer, a spy, a medic), so a stranded fleet, a last tank or a last dozer cannot stall a game.
- **Measured** (`--evalai`, 96 games per run, `ONEHOUR_BASEMASK` / `ONEHOUR_NEWMASK`): the commander with the navy against the one without it won 32 and lost 30 in one run and won 30 and lost 32 in another (34 games unresolved after 1500 s each:
  neutral, as the lake is one corner of the map); with the anti-nuke defences and the covered-target nuke salvos against without it won 46 and lost 30, 20 unresolved.

`--evalai N` plays the current commander against the previous generation of the AI (over 72 games it loses none at Normal, Hard or Brutal: 55, 66 and 66 wins, the rest unresolved after 1500 s); `--evaldiff N` with
`ONEHOUR_DA` / `ONEHOUR_DB` checks that the difficulty ladder holds (Normal and Hard beat Easy 16-0, Hard beats Normal 10-0, Brutal beats Hard 16-0 over 16 games each). Every part of the new commander has its own switch (`AiFeat` in `src/ai.h`),
so each was measured alone: `ONEHOUR_BASEMASK` / `ONEHOUR_NEWMASK` (bit sets) make the two sides of `--evalai` play with different feature sets, `ONEHOUR_BRAIN2=path|prior` gives the baseline its own brain (what is training worth?) and
`ONEHOUR_EVALGAME=i` replays game *i* of a series and prints the final position, `ONEHOUR_EVALTEAMS=1` plays two new commanders against two previous ones (2v2: 35-0, 47-0 and 42-0 over 48 games at Normal, Hard and Brutal), `ONEHOUR_OLDDIFF=d` sets the baseline's difficulty. Ideas that did not beat the baseline over about a hundred games each were taken out again: staging a wave outside the target's defences (it lost 30-47: the whole defending army
reacts to the muster point), judging a running wave by the estimate instead of by what it cost, choosing the target by the estimate, falling back under the turrets when outgunned, focus-fire target scoring, matching a column's speed to its slowest member, and an early third supply hub.

**Combat learning and local decisions (October 2026).** Unit regressions now use completed combat lives: actual enemy health-value removed divided by unit cost, with the visible local enemy mix recorded at first contact. A unit still travelling or waiting in reserve produces no training label. Combatants killed in action and survivors of a completed match each contribute once; quiet cleanup is excluded. Friendly fire, overkill and damage stopped by a bunker floor earn no extra value. Learning uses normalized, regularized updates and the production preference grows with sample confidence. Wave outcomes credit the wave's own members and reinforcements, rather than unrelated reserve fire. These are improvements to the evidence feeding the existing regressions; the controlled prediction test does **not** establish a higher match win rate.

Computer armies can secure reachable neutral buildings with small reserve infantry groups, keep garrisons out of attack-wave counts, and preserve exits when placing walls. Ground units choose reachable escape routes when a wall or shoreline blocks straight reverse, including artillery inside minimum firing range. Replanning no longer resets the ground progress timer. These movement and garrison decisions are tactical rules; the learned models remain the unit regressions, wave classifier and doctrine bandit.

Brain files retain version 14 and compatibility with versions 2–14. Saves write and flush a temporary file beside the destination, then replace it atomically; failed writes keep the previous brain. Loading rejects nonfinite coefficients and invalid core sample counts before changing the live state. The player's existing brain is never reset by the update.

## Engineering notes

- C++17, SDL2 only. Every sprite, the terrain, the font and every sound effect are generated at startup; there are no asset files.
- **Painter** (`src/paint.h`): shapes are signed distance fields with anti-aliased coverage and a bevel lit from the north-west, drawn on 2x
  supersampled canvases. Units (`src/art.cpp`) have walk / track frames, turrets, spinning rotors and burning wrecks; structures
  (`src/artb.cpp`) are raised 3/4-view blocks with south walls, roof props, windows, soft cast shadows, a construction-site sprite and a ruin
  sprite, painted once per type and shared by every owner (the owner's colour comes from a white team mask tinted at draw time, plus the flag).
- **Terrain** (`src/terrain.cpp`): the whole 104x104 map is baked once into one 3328x3328 texture on worker threads: smoothed class coverage with
  noise-displaced borders, procedural grass / dirt / sand / asphalt / rock / water shading (meadow-scale sun-bleached and lush drifts, a three-stop
  lake-depth ramp with sand bars, weed beds and a sun-glitter path, scree aprons at cliff feet), a height field that lights cliffs and lake banks and
  casts soft shadows from rocks and tree canopies, foam and wet sand on the shore, painted road markings with worn edge lines, depot pads under the
  supply piles, old craters, individually drawn trees, boulders, flowers, pebbles and reeds, finished with a split-tone grade (warm highlights, cool
  shadows) so the whole map sits in one sunlight. The minimap is a downsample of the same texture. On a GPU the texture is stored as dithered
  RGB565 (22 MB instead of 44 MB at 32-bit — less than the old 80x80 map's 26 MB); under the software renderer, which has no fast 565 blit, it stays
  32-bit. Unit and structure sprites are cropped to their painted area for the same reason. Water caustics draw from map-generated lists of aligned
  4x4 interior-water blocks plus leftover shore tiles, so a screen full of lake costs dozens of draws instead of hundreds; the shroud's mottling
  noise is baked into lookup tables once. Vehicles bob on their suspension, ships roll and heave slowly, helicopters sway in the hover, infantry
  stride with the ground they cover, beams flicker, and electric arcs throw side forks — all by nudging existing draws, no new ones.
- **Effects** (`src/fx.cpp`): fireball frames, flame tongues, smoke and dust puffs, muzzle stars, beams, shock rings and scorch decals are baked
  sprites; a capped (900) render-side particle system adds debris, sparks, smoke columns, vehicle dust, vapour trails and nuke mushroom clouds,
  spawned the first frame a simulation effect appears. It never touches the simulation and freezes with the game.
- The simulation is untouched by any of it; the whole game still uses a few percent of one core: `./build/onehour --bench` renders 300 frames of
  an eight-minute four-army battle with the CPU-only software renderer (about 11 ms a frame against 13 ms for the old 80x80 build measured
  interleaved on the same machine, simulation at parity in `--selftest`; about 78 MB resident under the software renderer — the 32-bit world
  texture — and less on a GPU, where the world is the 22 MB RGB565 version; startup, which includes baking a 69%-larger world, is two to three seconds).
- The window is resizable (F11 fullscreen); the UI scale adapts and the HUD re-anchors. The whole terrain is
  baked once into a single texture with domain-warped, noise-blended transitions; fog of war is a bilinear mask.
- Fixed 20 Hz simulation with interpolated 60 fps rendering.
- **The lake pass** is spread over a few files so each stays readable: `src/sim2.cpp` (jeep crews, protector drones, ships' drive, anti-nuke engagement, the Hotline Strike fighters, shore sites), `src/ui2.cpp` (wakes, ship mounts, jeep seats, drones, docks, anti-nuke shots, the strike marker),
  `src/art2.h` with `art_jeep.cpp`, `art_ship.cpp` and `art_jet.cpp` (the new unit sprites: a jeep with seven seats at fixed seat positions the renderer puts helmets on, ship hulls with separate rotating mounts, the strike fighters), `src/artb_naval.cpp` (Naval Yards and anti-nuke structures) on the structure painter that moved to `src/bld.h`,
  and `src/tests3.cpp` / `src/testutil.h` (the tests below). Ships plan with `Map::findNavalPath` (an A* over *open* water: a tile and its four neighbours are water, shore tiles cost more, the goal moves to the nearest open water), and `Map::bigWater` (water bodies are labelled at map generation; the lake is the only one big enough to sail or to build a yard on).
  The anti-nuke engagement is rolled when a warhead enters a defence's window and then played out in time (`Sim::interceptors`), so the shots on screen are exactly what decides the outcome and a test can count outcomes over hundreds of warheads. The AI brain file is v14 (v13 files with 47 unit rows still load).
- **Movement physics**: ground vehicles accelerate over about a third of a second, ease off over the last tile of a path and slow while the hull is still swinging toward the next waypoint (`Sim::moveAlong`; the speed lives in `Entity::airspeed`, which
  a ground unit has no other use for); ships turn at 1.05 rad/s, build up speed along their heading, sail where they point and slide along a shore they brush instead of stopping on a tile edge (`Sim::moveShip`). `--movetest` checks a tank's ramp-up and its slow-down while it swings round (and that infantry have no ramp), and the `--selftest` stuck monitor (`ONEHOUR_STUCK=1`) reports any unit that has an order and does not move for 30 s (24 games of 15 minutes: 2 reports, both haulers deliberately stepping away from a raider, against 16 in 3 games on the build before this pass).
- **Build rules the lake needed**: nothing can be built on a bridge deck (`Map::buildable`), and a structure may not cut a walkable region in two (`Map::wouldPlug` counts the 4-connected regions of the 6-tile window around the footprint before and after; `Sim::canPlace` refuses a
  plug), so a hub dropped on the ring road can no longer wall an army off from the beach. A ground unit sent to a point it cannot reach (out in the lake, or a pocket behind structures) walks to the nearest shore or stays where it is, instead of pushing at the terrain in a straight line.
- `--navytest`, `--jeeptest`, `--dronetest`, `--strikejettest` and `--antinuketest` (all in `src/tests3.cpp`) cover the lake pass: the map's land connectivity, the yard's placement rules and production, sailing and ship fights, nukes sinking ships; jeep seats, firing from inside, unloading and casualties; the drone screens against rockets, shells and lasers; the strike fighters (two a call, not buildable, not commandable, big damage, hard to shoot down, gone afterwards);
  and the anti-nuke rates (the Anti-Nuke Laser's 40% warhead nerf included). `--fuzztest` throws the new commands at the simulation too. Screenshot helpers: `ONEHOUR_NEWUNITS=1 --sheet out.bmp` (jeeps, ships, strike fighters and drones at 4x with seat and mount markers), `ONEHOUR_BLD=0|1` now also shows the Naval Yard and the anti-nuke defence (slots 12 and 13),
  `ONEHOUR_NAVY=ticks --shot out.bmp --ticks 10 --view 0` (the lake with both yards, fleets under their drone screens, jeeps with crews and an anti-nuke defence; `ONEHOUR_NAVYSEL=ship|yard|jeep|an|dozer` picks what is selected). `ONEHOUR_AIR=strike|drop|nuke` (with `ONEHOUR_AIRTICKS=n`) stages the Hotline Strike fighters' rocket pass, the jeep airdrop or an anti-nuke interception the same way.
- `--civictest` (`src/tests6.cpp`) covers the town (only civilian dozers build it, the pay, the alternating free dozers and their cap, no plating or roof guns, one Town Hall, spy capture), the Underground Nuke (price, prerequisite, 3x radius and radiation, 2.7x damage, the silo's floor, only lasers engage the silver warhead), the bunker's watch over its neighbours and a computer army's town. Showcase: `ONEHOUR_TOWN=1|deep|boom --shot out.bmp --ticks 400` (a town with its townsfolk, an armed silo and a bunker; the silver warhead in flight; a silver detonation, `ONEHOUR_TOWNBOOM` ticks after impact, printing the average frame time; `ONEHOUR_TOWNRAMP=1` uses a ramp's warhead to compare), and `ONEHOUR_BLD=0|1 ONEHOUR_CIVIC=1 --sheet out.bmp` (the town and the silo). Nuke scorch marks are drawn unrotated and the explosion particles of a nuke are capped at a ramp's size: the software renderer spent 100-300 ms a frame on them.
- `--walltest` (`src/tests7.cpp`) covers the defensive overhaul (walls, Hyper Protection, Anti-Rocket, durability, the barracks rate and the 25-pad airfield), and
  `--supersonictest` (`src/tests8.cpp`) covers the balance and durability passes: the Anti-Nuke Laser's 40% and the Anti-Rocket wall's 70% warhead-only nerf against the rates actually rolled, the new durability order (**Command 18,000/18,500 > Underground Nuke silo 14,000/14,500 > underground bunker 11,000 > Data Center / Arms Lab 7,000/7,500 > wall 1,800**), the silo's 5,000-hp blast doors, the armoured silver warhead (a lone Anti-Nuke Laser rolls 9% against it, a lone Anti-Rocket wall 5.7%, and the rate actually rolled is halved too), the Underground Nuke's 2.7x damage, the underground bunkers' 11,000 hp and their visibility and targetability as a normal building, and the Supersonic Bomber (both armies, stats and key, one bomb a sortie, the dash and the slow cruise after the drop, surviving its own fireball, the much smaller fireball, the silver radiation and the rearm). Showcase: `ONEHOUR_SBOMBER=N --shot out.bmp --ticks 10` (a nuclear strike on an enemy outpost; `=park` shows the bomber on its pad) and `ONEHOUR_SBOMBER_SPRITES=1 --sheet out.bmp` (the bomber at 4.5x in all four team colours, a 1x strip below).
- `--radtest` (`src/tests9.cpp`) covers the Anti-Radiation Truck: the data row and factory production of both armies, the field shielding friendly and enemy units and structures from a crater while the truck absorbs the dose, a silver cloud hurting it more, the zone scrubbed to nothing (strength, radius and haze) with a zone out of reach untouched, two trucks stacking, and a full-health truck surviving a whole silver cloud. Showcase: `ONEHOUR_RADTRUCK=N --shot out.bmp --ticks 10` (two trucks scrubbing a fresh crater; N ticks) and `ONEHOUR_RADTRUCK_SPRITES=1 --sheet out.bmp` (the truck at 4.5x in all four team colours, a 1x strip below).
- `--forttest` (`src/tests10.cpp`) covers the map's neutral civilian strongholds: the rotated castle / bunker / watchtower placement, their empty neutral start, infantry walking in and claiming one, owner and ally access, the data-row capacity, the garrison firing from inside, the 62% masonry and survival of a direct warhead, unload and the garrison being turned out when the building falls.
- `./build/onehour --selftest 900` runs an AI-only game headless and checks invariants;
  `--scenario` drives every structure/unit/order for both factions; `--uitest` feeds synthetic
  mouse and keyboard events through the real input code; `--shot out.bmp --ticks N` renders a frame
  headless; `--train N` self-plays N games to train the AI brain, `--eval N` pits the learned AI against the plain heuristic AI;
  `--soundcheck` prints statistics for the synthesized sounds; `--hqtest` rebuilds a lost Command Core (human dozer and AI), `--jettest` flies
  the jets (speed, banking, strafing, rearming), `--econtest` and `--areatest` cover income structures, nukes and area orders, and `--bench [N]`
  times N rendered frames. `--airmatrix` and `--groundmatrix` send every aircraft and every ground combat unit (idle, attack-move, guard area, attack order; moving targets; flights of four) against every kind of target and require damage within a time limit, `--turrettest` does the same for turrets and anti-air batteries, `--helitest` builds 18 helicopters from one airfield (and checks they park on helipads with a gap between them, stop side by side on a group move, hover still on posts of their own when guarding a small circle, and that planes still wait for a pad), `--aidtest` covers the Aid Drop, `--droptest` and `--striketest` cover the unlock-driven airdrop tiers and the phone strike swarm, `--stealthtest` covers stealth snipers and spy drones, spy captures, cargo lifters, underground bunkers (garrison, hatch, nukes, destruction; and a lone bunker's income, self-repair, miniguns and mini nuke) and promotions, `--allytest` covers a computer ally moving up to the front where your army fights, `--haultest` covers haulers escaping a raider and returning to work, `--upgradetest` covers the structure upgrades (research rules, Rugged under one and two nukes and under a massive assault, roof guns against aircraft, vehicles and infantry, self-repair on structures but never units, upgrades lasting after the tech structure falls) and dozers sharing out repairs, `--fuzztest [secs]` throws random commands at the simulation (run it under `-fsanitize=address,undefined`), and `ONEHOUR_STUCK=1 --selftest` reports units that hold a movement order without moving. `--autotest` covers units left on their own (answering fire from beyond their reach, backing away from aircraft they cannot hit, helping an ally, a small guard zone, nuke dodging), force fire through the Force Fire button with no ally or own unit shooting back, jets shooting down bombers and fighting each other, and no fixed-wing aircraft hanging still; every `--selftest` also fails if a fixed-wing aircraft away from its airfield ever flies slower than its stall speed. Screenshot helpers: `ONEHOUR_CAM=tx,ty`, `ONEHOUR_REVEAL=1`, `ONEHOUR_JETS=1`, `ONEHOUR_BOMBS=N`, `ONEHOUR_DROP=N` (+ `ONEHOUR_CAMBACK=px`), `ONEHOUR_AID=N` (an aid drop near the enemy base; + `ONEHOUR_CAMBACK=px`, `ONEHOUR_CAMPLANE=1`), `ONEHOUR_NUKEDMG=N`, `ONEHOUR_MENUDEMO=row`, `ONEHOUR_NEWB=1|boom`, `ONEHOUR_HELI=N` (N helicopters of the viewed army over its base; `ONEHOUR_HELIGUARD=1` has them guard a small circle there), `ONEHOUR_UPG=1|raid` (every army starts with all structure upgrades; `raid` then sends aircraft and tanks into the viewed base), and `--sheet FILE`
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
- **Nuke Ramp** (both armies, $5000, needs the tech structure): each ramp can launch one tactical nuke every 5 minutes (60 s arming after it is built). The warhead flies for 7 s, so the enemy gets a warning circle, and it tears up an 11 tile radius around ground zero. **Every enemy ground unit in the blast collapses** (a lethal core, then falling damage), **every aircraft in the blast falls out of the sky, whoever owns it** (parked, hovering or passing through, the launcher's own and its allies' too; nothing flies fast enough to get away, so aircraft never try; a paradrop plane over the blast goes down with its load, and so does your own aid plane in your own blast), **small structures collapse** (anything of six tiles or less: turrets, batteries, reactors, barracks, income structures, walls) while **large structures survive heavily damaged** (about 80% of their health lost at ground zero, a fifth at the rim, never lethal, and knocked offline for a while). Survivors are stunned and hurled, and chain explosions go off under a towering mushroom cloud (a rolling vortex cap lit by the fireball, a turbulent stem, a condensation collar and a base surge of dust). The missile itself is a large two-livery warhead with a long exhaust, a smoke trail, a ground shadow and a glowing nose on re-entry. A computer army's friendly ground units are thrown but never hurt by the blast itself; a human's warhead hurts the human's own army and allies exactly as hard as an enemy's. The crater stays radioactive for 80 s: every unit on the ground inside it, friend or foe, keeps taking damage, while structures only suffer a slow drain that stops at a tenth of their health — unless an **Anti-Radiation Truck** scrubs the crater out first (see **Anti-Radiation Truck** above). Any number of ramps can be built; more ramps means more warheads per cycle.
- **The civilian town** (both armies, built by a civilian dozer): nine kinds of civilian structure that pay $30 to $220 every 10 s and send out free dozers, with no power and no limit but 30 per army (see **The civilian town** above).
- **Underground Nuke** (both armies, $50000, needs a Nuke Ramp): a silver warhead every 5 minutes with three times a ramp's radius and radiation and 2.7 times its damage; only laser defences stop it, at half the usual chance (its casing is armoured), and the silo is the second-toughest structure at 14,000 / 14,500 hp (behind only the Command structure) with 5,000-hp blast doors; its 240-second cloud can be scrubbed away early by an **Anti-Radiation Truck** (see **Underground Nuke** above).
- Airfields honor their rally point: new aircraft fly there and wait, going back to the pad only to rearm.

## Frontier verification (2026-10-20)

The expanded battlefield, combat-learning attribution, navigation and procedural art changes were checked with 39 regression commands, both combat matrices, deterministic replay, randomized placement against a flood-fill oracle, sanitizer fuzzing, a 2,000-unit stress scene and isolated interactive SDL gameplay. See [the verification record](docs/verification-2026-10-20.md) for measurements, reproduction commands and limits.

New checks: `--frontiermaptest`, `--learningtest`, `--placementtest`, `--placementbench`, `--determinismtest [seconds]` and `--crowdtest`. `ONEHOUR_BLD=0 ONEHOUR_FORTS=1 --sheet forts.bmp` shows the five neutral building designs. The F1 controls panel wraps to fit the current window, and neutral landmark labels appear on hover.

A separate sanitizer build is supported without replacing the normal executable:

```sh
make -j4 BUILD_DIR=build-sanitize CXXFLAGS='-std=c++17 -O1 -g -fno-exceptions -fno-rtti -fsanitize=address,undefined -fno-omit-frame-pointer' LDFLAGS='-fsanitize=address,undefined'
ASAN_OPTIONS=detect_leaks=0:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ./build-sanitize/onehour --fuzztest 300
```
