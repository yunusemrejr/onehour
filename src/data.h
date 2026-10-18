// One Hour - static game data: factions, weapons, unit and structure types
#pragma once
#include "common.h"

enum Faction { F_CYBER = 0, F_CLANKER = 1, F_COUNT = 2 };
static const char* const FACTION_NAME[F_COUNT] = { "Cyber Army", "Clanker Army" };

enum Armor { AR_INF = 0, AR_LIGHT, AR_HEAVY, AR_STRUCT, AR_AIR, AR_COUNT };

enum ProjKind { PJ_BULLET = 0, PJ_LASER, PJ_ARC, PJ_SHELL, PJ_ROCKET, PJ_RAIL, PJ_BOMB };

enum Sound {
    SND_NONE = 0, SND_RIFLE, SND_MG, SND_LASER, SND_LASER_HEAVY, SND_ARC, SND_CANNON, SND_ROCKET,
    SND_RAIL, SND_EXPLODE_S, SND_EXPLODE_L, SND_HIT, SND_SELECT, SND_ORDER, SND_PLACE,
    SND_BUILD_DONE, SND_UNIT_READY, SND_NOFUNDS, SND_LOWPOWER, SND_ATTACKED, SND_VICTORY, SND_DEFEAT,
    SND_CLICK, SND_EMP, SND_SUPPLY, SND_AIR, SND_CANT, SND_JET, SND_COUNT
};

struct Weapon {
    const char* name;
    float dmg;          // per shot
    float range;        // tiles
    float minRange;     // tiles (0 = none)
    float cooldown;     // seconds
    ProjKind proj;
    float splash;       // tiles (0 = none)
    bool ground, air;
    float mult[AR_COUNT];
    int burst;          // shots per volley
    float projSpeed;    // px/s (0 = instant)
    Sound sound;
    Color color;
};

enum UnitKind { UK_INF = 0, UK_VEH, UK_AIR };
enum UnitRole { UR_COMBAT = 0, UR_DOZER, UR_HARVESTER, UR_HEALER,
                UR_SPY,         // unarmed infantry that captures enemy structures
                UR_SCOUT,       // stealth spy drone: unarmed, sees what nothing else can
                UR_TRANSPORT }; // cargo chopper: carries infantry and vehicles
// Stealth classes: a stealthy unit or structure is invisible to an enemy (cannot be seen, selected or shot) unless one of the enemy's
// own detectors is close enough. Detector masks say which classes a unit can see.
enum StealthClass { ST_NONE = 0, ST_SPOTTER = 1,    // snipers and underground bunkers: only a spy drone finds them
                    ST_DRONE = 2 };                  // the spy drone itself: only drones (its own kind, or the cheap bomber drones up close) find it
static const int DET_SPOTTER = 1, DET_DRONE = 2;

enum UnitTypeId {
    U_C_DOZER = 0, U_C_HARV, U_C_INF1, U_C_INF2, U_C_INF3, U_C_TANK, U_C_VOLT, U_C_RAIL, U_C_AIR, U_C_ELITE, U_C_TITAN, U_C_JET,
    U_K_DOZER, U_K_HARV, U_K_INF1, U_K_INF2, U_K_INF3, U_K_TANK, U_K_GATLING, U_K_MLRS, U_K_AIR, U_K_ELITE, U_K_TITAN, U_K_JET,
    U_C_MEDIC, U_K_MEDIC,   // field medics sit after both armies' blocks so every earlier id stays put
    U_C_SNIPER, U_K_SNIPER, // so do the snipers
    U_C_HELI,               // and the Cyber attack helicopter (the Clanker helicopter is the Vulture Gunship, U_K_AIR)
    U_C_SPY, U_K_SPY,       // spies: capture enemy structures
    U_C_SDRONE, U_K_SDRONE, // stealth spy drones
    U_C_CARGO, U_K_CARGO,   // cargo choppers
    U_C_JEEP, U_K_JEEP,     // jeeps: fast open-top transports that carry seven infantry who shoot from inside
    U_C_DESTROYER, U_K_DESTROYER,   // warships of the lake (built at a Naval Yard on the shore)
    U_C_CRUISER, U_K_CRUISER,       // missile ships
    U_C_STRIKEJET, U_K_STRIKEJET,   // the two fighters a Hotline Strike brings: never built, they leave again when the run is over
    U_C_CIVDOZER, U_K_CIVDOZER,     // civilian dozers: they raise the town (houses, shops, towers, the town hall), never military structures
    U_COUNT
};

enum BuildRole { BR_HQ = 0, BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_AIRFIELD, BR_TECH, BR_TURRET, BR_AATURRET, BR_INCOME, BR_NUKE, BR_BUNKER,
                 BR_NAVAL,       // Naval Yard: on the shore of the lake, builds the ships
                 BR_ANTINUKE,    // Anti-Nuke Laser / Anti-Nuke Missiles: small structures that shoot incoming warheads down
                 BR_CIVIC,       // civilian infrastructure (houses, shops, towers, the town hall): raised by a civilian dozer, pays and sends dozers on its own
                 BR_UNUKE,       // Underground Nuke silo: a buried launcher as hard to destroy as a bunker, its silver warhead three times a ramp's
                 BR_WALL,        // thick concrete/steel wall segment: unlimited, cheap, very hard to nuke through
                 BR_NONE };      // (no structure builds it: the Hotline Strike fighters)

enum BuildTypeId {
    B_C_HQ = 0, B_C_POWER, B_C_SUPPLY, B_C_BARRACKS, B_C_FACTORY, B_C_AIRFIELD, B_C_TECH, B_C_LASER, B_C_PATRIOT, B_C_MINER, B_C_NUKE,
    B_K_HQ, B_K_POWER, B_K_SUPPLY, B_K_BARRACKS, B_K_FACTORY, B_K_AIRFIELD, B_K_TECH, B_K_MG, B_K_ROCKET, B_K_OILWELL, B_K_NUKE,
    B_C_BUNKER, B_K_BUNKER,   // the underground bunkers sit after both armies' blocks so every earlier id stays put
    B_C_NAVAL, B_K_NAVAL,     // Naval Yards
    B_C_ANTINUKE, B_K_ANTINUKE,   // anti-nuke defences
    // civilian infrastructure, the same nine kinds for each army in CivicKind order (see civicKind)
    B_C_HOUSE, B_C_APARTMENTS, B_C_SKYSCRAPER, B_C_GAS, B_C_GROCERY, B_C_EVENTHALL, B_C_SQUARE, B_C_TOWNHALL, B_C_GOVERNMENT,
    B_K_HOUSE, B_K_APARTMENTS, B_K_SKYSCRAPER, B_K_GAS, B_K_GROCERY, B_K_EVENTHALL, B_K_SQUARE, B_K_TOWNHALL, B_K_GOVERNMENT,
    B_C_UNUKE, B_K_UNUKE,     // Underground Nuke silos
    B_C_WALL, B_K_WALL,       // wall segments (unlimited; see BR_WALL)
    B_COUNT
};

struct UnitType {
    const char* name;
    const char* hotkey;
    Faction faction;
    UnitKind kind;
    UnitRole role;
    Armor armor;
    float hp;
    float speed;        // px/s
    int cost;
    float buildTime;    // seconds
    float sight;        // tiles
    float radius;       // px
    int weapon;         // index into WEAPONS, -1 none
    int ammo;           // aircraft: shots before rearm (0 = unlimited)
    BuildRole builtBy;  // which structure role produces it
    int requires_;      // BuildTypeId prerequisite or -1
    const char* desc;
    int program;        // 1 = also needs the faction's Advanced Program (researched at the tech structure)
    int jet;            // 1 = supersonic fighter: fixed-wing strafing passes with a limited turn rate instead of hovering
    int bomber;         // 1 = carries bombs: flies bombing runs over ground targets (sticks of heavy bombs), uses its gun on aircraft
    int sniper;         // 1 = sniper: shoots only infantry (never another sniper) and can only be spotted and hit by vehicles and aircraft
    int heli;           // 1 = helicopter: hovers and turns on the spot; every other aircraft is fixed-wing and never stands still in the air
    int stealth;        // StealthClass: invisible to an enemy that has no detector close enough
    float detect;       // tiles: how far this unit finds stealthy enemies (0 = it finds none)
    int detectMask;     // which StealthClass es it finds (DET_SPOTTER / DET_DRONE bits)
    int cargoCap;       // transport: cargo slots (an infantryman takes one, see cargoSize)
    int naval;          // 1 = ship: lives and moves on water only (the lake), see Map::findNavalPath
    float evade;        // 0..1: the share of weapon hits that miss it (the Hotline Strike fighters are very hard to shoot down)
    int temp;           // 1 = a loan from a support power: cannot be built, selected or commanded, and leaves the map when its run is over
    int drones;         // protector drones it flies with (see Sim::droneCount): they shoot down rockets and shells aimed at it
};
// Cargo slots a unit takes in a transport: infantry 1, light vehicles 4, tanks 6, super-heavy walkers 10 (aircraft cannot be carried)
inline int cargoSize(const UnitType& u) { return u.kind == UK_INF ? 1 : (u.kind == UK_VEH ? (u.hp >= 1500 ? 10 : (u.armor == AR_HEAVY ? 6 : 4)) : 0); }

struct BuildType {
    const char* name;
    const char* hotkey;
    Faction faction;
    BuildRole role;
    float hp;
    int cost;
    float buildTime;    // seconds
    int w, h;           // tiles
    int power;          // + produces, - consumes
    float sight;
    int weapon;
    int requires_;      // BuildTypeId prerequisite or -1
    const char* desc;
};

extern const Weapon WEAPONS[];
extern const int WEAPON_COUNT;
extern const UnitType UNITS[U_COUNT];
extern const BuildType BUILDS[B_COUNT];

// Faction special powers of the tech structure (Data Center / Arms Lab): a strike, a map scan and a research program
struct PowerType { const char* name; const char* desc; float cooldown; float radius; };
extern const PowerType POWERS[F_COUNT];
struct ScanType { const char* name; const char* desc; float cooldown; float duration; };
extern const ScanType SCANS[F_COUNT];
struct ProgramType { const char* name; const char* desc; int cost; float time; };
extern const ProgramType PROGRAMS[F_COUNT];

// Structure upgrades researched at the tech structure (Data Center / Arms Lab). Once finished an upgrade lasts the whole match and
// covers every structure the army owns, the ones standing and the ones it builds later, even if the tech structure falls afterwards.
enum UpgradeId { UPG_RUGGED = 0, UPG_GUNS, UPG_REPAIR,
                 UPG_DRONES,     // protector drones: every ship flies with a screen of them, and every jeep gets a second one
                 UPG_HYPER,      // Hyper Protection: far more health, heavy damage reduction, and a hard per-warhead damage cap
                 UPG_ANTIROCKET, // Anti-Rocket: every wall becomes a laser point-defence that burns rockets, warheads and aircraft
                 UPG_COUNT };
static const int UPG_STRUCTURE_COUNT = UPG_DRONES;   // the first three change what structures are; they and the Advanced Program set the drop tier
struct UpgradeType { const char* name; const char* hotkey; const char* desc; int cost; float time; };
extern const UpgradeType UPGRADES[F_COUNT][UPG_COUNT];
static const float RUGGED_HP = 1.5f;          // Rugged: structure health multiplier
static const float RUGGED_ARMOR = 0.5f;       // Rugged: fraction of incoming damage a structure takes
static const float RUGGED_SWAMP_LO = 0.03f;  // Rugged: the plating starts to give once this share of max health lands within ~2 s ...
static const float RUGGED_SWAMP_HI = 0.11f;  // ... and is overwhelmed (full damage) from this share on: only a massive assault breaks through
static const float RUGGED_NUKE = 0.55f;       // Rugged: fraction of max health one warhead takes at ground zero (two close nukes bring it down)
// Hyper Protection: the tech structure's ultimate plating. Beyond doubling structural health and shrugging off most conventional fire, it puts a
// hard ceiling on what one warhead can do to a protected structure: a full-health building can never be erased by a single nuke (even the
// silver Underground Nuke), so a warhead can only finish structures that were already damaged. Damage that would exceed the cap is stopped by
// the plating.
static const float HYPER_HP = 2.0f;           // structure health multiplier
static const float HYPER_ARMOR = 0.40f;       // fraction of conventional damage a protected structure takes
static const float HYPER_NUKE = 0.42f;        // hard ceiling: one warhead takes at most this share of max health
// The Command Core / Post is the keystone: enormous health, heavy armour against conventional fire, and a hard nuke ceiling. It cannot be
// destroyed by warheads alone; it takes a sustained conventional assault. The tech structure is deliberately the second-toughest building.
static const float HQ_ARMOR = 0.35f;          // fraction of conventional damage a Command structure takes
static const float HQ_NUKE = 0.12f;           // hard ceiling: one warhead takes at most this share of its max health
// Walls: a cheap, unlimited 1x1 segment with a huge health pool and a very low per-warhead ceiling. At ground zero a normal warhead takes
// about a tenth of a wall's health, so even a direct hit is only a chip: it takes a long barrage to open a breach. Hyper Protection doubles
// the pool, so it takes even longer.
static const float WALL_HP = 18000.0f;
static const float WALL_NUKE = 0.10f;         // hard ceiling per warhead (fraction of max health)
static const float WALL_ARMOR = 0.60f;        // fraction of conventional damage a wall takes
// Anti-Rocket walls and the Q defences share a point-defence envelope. A wall's laser is shorter-ranged than a dedicated anti-nuke tower but
// walls are unlimited, so a line of them can blanket a base. Each defence rolls its own chance and has its own reload.
static const float WALL_PD_COVER = 10.0f;     // tiles around an Anti-Rocket wall
static const float WALL_PD_CHANCE = 0.38f;    // chance to burn down one incoming rocket / shell / warhead
static const float WALL_PD_RELOAD = 3.2f;     // seconds before the same wall can engage again
static const float WALL_PD_AA = 190.0f;       // damage a wall laser does to an aircraft it catches
static const float ANTINUKE_PD_AA = 230.0f;   // damage a Q defence does to an aircraft
static const float PD_AIRCRAFT_RELOAD = 1.1f; // seconds between aircraft shots
static const int   AIRFIELD_CAP = 25;         // fixed-wing aircraft based at one oversized airfield: one per pad
static const float BARRACKS_SPEED = 3.0f;     // barracks infantry production multiplier (always, even on low power)
static const float SELF_REPAIR_RATE = 0.012f; // Self-Repair: fraction of max health a structure restores per second once out of combat
static const float SELF_REPAIR_HOT = 0.25f;   // ... and the share of that rate it keeps while still taking fire
static const float SELF_REPAIR_DELAY = 3.0f;  // seconds after the last hit before full-rate repair resumes
static const int   W_JEEP_MG = 33;                        // the pintle gun of a jeep (WEAPONS index)
static const int   W_DESTROYER_C = 34, W_DESTROYER_K = 35, W_CRUISER_C = 36, W_CRUISER_K = 37;   // the ships' guns and rocket salvos
static const int   W_STRIKE_C = 38, W_STRIKE_K = 39;      // the rockets of the Hotline Strike fighters
static const int   W_DEFENSE_LASER = 29, W_DEFENSE_MG = 30;   // the roof guns of the Defense Guns upgrade (Cyber laser, Clanker machine gun)

// Paradrop support power of the tech structure (Zero Hour style): a cargo plane flies in over a spot of your choice and drops a free force on parachutes.
// What the plane carries is never a fixed list: it is composed from what the army can actually field right now (Sim::dropManifest).
static const int DROP_INF = 15, DROP_VEH = 7, DROP_AIR = 4;
struct DropType { const char* name; const char* desc; float cooldown; float radius; };   // radius: tiles around the target the troops land in
extern const DropType DROPS[F_COUNT];
// The armies' order of battle in the three drop slots, the cheapest first, combat troops only. Sim::dropManifest walks these until the army's
// drop tier rings the bell for that slot, so a drop is exactly the best the army can field at that moment. Support (medics, snipers, spies, a
// scout drone) does NOT sit in these slots: it rides along on top (DROP_SUPPORT), so a load never trades its fighting strength for it.
// Every id in the cheap part is fieldable with nothing but the producing structure; the ones behind the Advanced Program sit at the deep end.
extern const int DROP_INF_POOL[F_COUNT][DROP_INF];
extern const int DROP_VEH_POOL[F_COUNT][DROP_VEH];
extern const int DROP_AIR_POOL[F_COUNT][DROP_AIR];
// Drop tiers: how far the single supply plane's manifest reaches into those pools, and how big the load is. The first tier is what the game
// has always sent (15 soldiers, 7 vehicles, 4 aircraft) so a fresh tech structure is no weaker than before - but the composition is now the
// army's own roster, and the tiers above it are bigger and better, so the same Data Center / Arms Lab sends a heavier force as the match goes
// on, for a human and for a computer army alike.
struct DropTierType { const char* name; const char* note; float infBudget, vehBudget, airBudget; float cooldown; };
enum DropTierId { DTI_1 = 0, DTI_2, DTI_3, DTI_COUNT };
extern const DropTierType DROP_TIERS[DTI_COUNT];
// The support the army brings besides its troops, per tier: medics, snipers, spies and scout drones. None of them is line fighting (a medic and a
// spy do no damage at all, a sniper and a scout drone none against a structure), so they ride on top of the combat slots rather than taking one -
// and each still appears only if the army can field it, which is what puts the snipers in the drop the moment the tech structure unlocks them.
static const int DROP_SUPPORT[DTI_COUNT][4] = { { 1, 1, 0, 0 }, { 1, 2, 0, 1 }, { 2, 2, 1, 1 } };

// Phone Strike ("hotline"): the tech structure's other support power. One call, and a strike package of helicopters, jets and drones
// crosses the map to a spot of your choice and tears up whatever stands there, then it goes home. Free of charge, once per cooldown per
// tech structure (more Data Centers / Arms Labs: more calls), and what answers the phone grows with the army the same way a drop does.
static const float STRIKE_COOLDOWN = 180.0f;    // seconds, per tech structure
static const float STRIKE_X = 3.5f;             // tiles: how far to either side of the target the package is spread when it comes in
static const int   STRIKE_MAX = 16;             // hard cap on aircraft in one package
struct StrikeType { const char* name; const char* desc; };
extern const StrikeType STRIKES[F_COUNT];
// How many aircraft answer a call at each drop tier: gunships, bombers, jets, scout drones. The swarm is a support power like the airdrop,
// so it grows with the same ladder - a fresh tech structure sends a swarm, a finished arms race sends a bigger one. Every tier brings all
// three kinds the power promises (helicopters, jets and a drone), so a strike called early is a real swarm too.
static const int STRIKE_LOAD[DTI_COUNT][4] = { { 4, 2, 2, 1 }, { 5, 3, 3, 1 }, { 6, 4, 4, 2 } };

// Hotline Strike fighters: with every call two special fighters (U_C_STRIKEJET / U_K_STRIKEJET) join the swarm. They are never built: a call
// lends them to the army for one run. They cross the map at STRIKE_JET_SPEED, make STRIKE_PASSES passes over the called spot, fire a
// salvo of STRIKE_SALVO unguided rockets on each, and leave. Very tough, and most hits on them miss (UnitType.evade).
static const int   STRIKE_JETS = 2;
static const int   STRIKE_PASSES = 3;
static const int   STRIKE_SALVO = 12;
static const float STRIKE_SPREAD = 2.4f;       // tiles: the radius of the circle a salvo is scattered over
static const float STRIKE_LIFE = 60.0f;        // seconds after which a strike fighter that is still around is withdrawn

// Jeeps: fast open-top transports. Up to JEEP_SEATS infantry ride inside and shoot from it with their own weapons; a jeep is also a gun.
static const int   JEEP_SEATS = 7;
static const int   DROP_JEEPS_MIN = 3, DROP_JEEPS_MAX = 10;   // an airdrop brings between these many loaded jeeps, by what the army has unlocked
static const float JEEP_CASUALTIES = 0.35f;    // share of the crew killed when the jeep blows up (the rest climbs out hurt)

// Protector drones: a screen of small drones that shoots down rockets and shells aimed at their host (ships, jeeps). Lasers and bullets are too
// quick for them. Each drone carries DRONE_SHOTS interceptors, a quarter-second apart, then needs DRONE_RELOAD seconds; a salvo bigger than the
// screen gets through (three drones stop a lone ship's volley of six rockets; a fleet's massed fire saturates them).
static const int   SHIP_DRONES = 3;            // with the Guard Drones upgrade
static const int   JEEP_DRONES = 1;            // a jeep flies with one; the Guard Drones upgrade adds a second (and gives every ship SHIP_DRONES)
static const float DRONE_RANGE = 3.2f * TILE;  // px: how close to the host a projectile must come to be engaged
static const float DRONE_RELOAD = 2.4f;
static const int   DRONE_SHOTS = 2;
static const float DRONE_GAP = 0.25f;
static const float DRONE_CHANCE = 0.85f;

// The lake: ships live on it and a Naval Yard must touch it (a body of at least NAVAL_WATER_MIN connected water tiles).
static const int   NAVAL_WATER_MIN = 60;

// Anti-nuke defences (Anti-Nuke Laser for Cyber, Anti-Nuke Missiles for Clanker): small structures that watch the sky over their area and shoot
// down a warhead heading for it. Each one has its own chance of getting the warhead (the more of them, the likelier it is stopped); one that
// is shot down still rains its pieces and its radiation on the target, but at ANTINUKE_FALLOFF of the damage.
static const float ANTINUKE_COVER_C = 17.0f, ANTINUKE_COVER_K = 19.0f;   // tiles around the structure it protects: a warhead aimed inside is engaged
static const float ANTINUKE_CHANCE = 0.30f;    // per defence per warhead
static const float ANTINUKE_WINDOW = 4.6f;     // seconds before impact it opens fire (a warhead is detected "closing in")
static const float ANTINUKE_RELOAD = 12.0f;    // seconds before the same defence can engage another warhead
static const float ANTINUKE_FALLOFF = 0.30f;   // what a shot-down warhead still does: falling pieces and radiation
static const int   ANTINUKE_MAX = 8;           // per player
// The Anti-Nuke Laser has no blind spots: it engages any warhead whose blast would reach into its cover (not only one aimed inside it), opens fire
// earlier on the way down, and its capacitors recharge as soon as the beams are spent, so a salvo cannot slip past it on a reload. Lasers standing
// close together share their tracking: each linked neighbour (up to ANTINUKE_LINK_MAX) adds beams, fires them closer together and raises the hit chance.
static const float ANTINUKE_LASER_WINDOW = 6.6f;    // seconds before impact a laser opens fire (the missiles wait for ANTINUKE_WINDOW)
static const float ANTINUKE_LASER_RECHARGE = 0.8f;  // seconds after its last beam before a laser can engage the next warhead
static const float ANTINUKE_LINK = 9.0f;            // tiles: lasers this close to each other are linked
static const int   ANTINUKE_LINK_MAX = 3;
static const float ANTINUKE_LINK_BONUS = 0.09f;     // hit chance added per linked laser
static const float ANTINUKE_LINK_TRACK = 0.4f;      // beams fire 1 / (1 + this * linked) as far apart

// Aid Drop: the human player's own power at the tech structure (Data Center / Arms Lab), never a computer army's. A white relief plane
// nobody shoots at drops crates on parachutes; the army nearest to the spot, other than the sender, receives the money and a dozer.
static const int   AID_MONEY = 20000;
static const float AID_COOLDOWN = 60.0f;      // seconds, per tech structure (more of them, more flights)
static const int   AID_CRATES = 5;            // crates in one drop (the dozer comes out of the middle one)
static const float AID_RADIUS = 3.0f;         // tiles: the crates come down within this of the spot

// Underground Bunker: a deep, hidden shelter that holds up to 50 infantry and one dozer. The garrison fires from a metal hatch on the surface;
// the hatch can be blown off (a nuke does it) which silences the bunker, but only infantry and vehicles can ever destroy the bunker itself.
static const int   BUNKER_INF_CAP = 50, BUNKER_DOZER_CAP = 1;
static const float BUNKER_HATCH_HP = 3200.0f;     // the surface opening (reinforced plating)
static const float BUNKER_PLATING = 0.25f;        // share of the damage that reaches the buried bunker once the hatch is gone
static const float BUNKER_FLOOR = 0.10f;          // aircraft, structures, shells from the sky, radiation and nukes can never take it below this share of its health
static const float BUNKER_LINGER = 25.0f;         // seconds a bunker a spy drone found stays known after the drone has moved on
static const float BUNKER_UNLOAD_GAP = 0.12f;     // seconds between two soldiers climbing out
// Dozers: a direct order holds off their own repair rounds for a while; a site they are pulled away from is kept and resumed.
static const float DOZER_MANUAL_HOLD = 12.0f;     // seconds after a manual move or target order (counted from its arrival) before it mends things on its own again
static const int   DOZER_SITE_QUEUE = 4;          // unfinished sites a dozer remembers
static const int   DOZER_SITE_CREW = 2;           // idle dozers join a site with fewer builders than this on their own (then they go mending)
// Ghost / Recon Drones: autonomous swarm reconnaissance that divides the map, hunts hidden enemy elements and waits when called by the player.
static const float SCOUT_MANUAL_HOLD = 10.0f;     // seconds a spy drone holds at a spot the player manually called it to before resuming autonomous swarm search
static const float SCOUT_SWARM_SEP = 18.0f;       // tiles: friendly spy drones coordinate as a swarm and spread their search sectors at least this far apart
// A bunker runs by itself, needing no power, no other structure and no supplies: it earns a little money, mends itself, mounts two
// miniguns on its hatch and keeps a salvo of small nuclear missiles (a fifth of a Nuke Ramp's warhead) ready.
static const int   BUNKER_INCOME = 85;            // credits per payout
static const float BUNKER_INCOME_INTERVAL = 5.0f; // seconds between payouts
static const float BUNKER_REGEN = 0.0022f;        // fraction of max health the body mends per second ...
static const float BUNKER_HATCH_REGEN = 0.0075f;  // ... and the hatch: both only once the bunker has been left alone for BUNKER_REGEN_DELAY seconds
static const float BUNKER_REGEN_DELAY = 6.0f;
static const float BUNKER_HATCH_REFIT = 30.0f;   // a blown-off hatch is refitted by the bunker itself this long after the last hit (a dozer does it faster)
static const int   BUNKER_GUNS = 2;               // miniguns on the hatch (they stop when the hatch is blown off, like the garrison)
static const int   W_BUNKER_MINIGUN = 32;         // WEAPONS index of those miniguns
static const float MINI_NUKE_SCALE = 0.2f;        // a bunker's missile: a fifth of the blast area (radius NUKE_RADIUS * sqrt(scale)), same strength per spot
static const float MINI_NUKE_COOLDOWN = 65.0f;    // seconds between two launches of one bunker
static const float MINI_NUKE_ARM = 30.0f;         // seconds after a bunker is finished before its first missile is ready
static const float MINI_NUKE_REVEAL = 10.0f;      // seconds a launch gives the bunker away to the enemy (the missile climbs out of its hatch)
static const int   MINI_NUKE_SALVO = 2;           // missiles per launch: the same warhead twice, one after the other ...
static const float MINI_NUKE_SALVO_GAP = 1.2f;    // ... this many seconds apart
// A bunker that is being worn down does not sit still: while it has been hit in the last BUNKER_RETALIATE seconds a rocket pod pops up (even with the
// hatch blown off) and answers whoever is shooting at it, and the hatch guns go for the attackers first and swat rockets and shells aimed at it.
static const float BUNKER_RETALIATE = 6.0f;
static const int   W_BUNKER_ROCKETS = 40;         // WEAPONS index of the rocket pod
static const float BUNKER_CIWS_RANGE = 4.2f;      // tiles from the bunker an incoming rocket or shell is engaged
static const float BUNKER_CIWS_GAP = 0.42f;       // seconds between two swats
static const float BUNKER_CIWS_CHANCE = 0.62f;
// Surrounded bunker overdrive: when enemy forces surround a bunker, heavy pop-up lasers / quad-machineguns engage aircraft, incoming nukes,
// tanks and infantry; if the surrounded bunker is taking heavy damage, it automatically vents a ring of radiation around itself.
static const int   W_BUNKER_LASER = 41;           // WEAPONS index of Cyber bunker overdrive laser
static const int   W_BUNKER_HEAVY_MG = 42;        // WEAPONS index of Clanker bunker overdrive heavy machinegun
static const float BUNKER_SURROUND_RADIUS = 9.5f; // tiles within which enemy units count as surrounding the bunker
static const int   BUNKER_SURROUND_MIN = 3;       // enemy units needed to trigger surrounded overdrive mode
static const float BUNKER_RAD_DMG = 320.0f;       // recent damage load (or heavy hull/hatch loss under fire) that triggers automatic radiation venting
static const float BUNKER_RAD_RADIUS = 7.5f;      // tiles: radius of the bunker's automatic radiation cloud
static const float BUNKER_RAD_COOLDOWN = 16.0f;   // seconds between automatic radiation vents
static const float BUNKER_ANTINUKE_COVER = 16.0f; // tiles around the bunker protected against incoming enemy nuclear warheads
static const float BUNKER_ANTINUKE_WINDOW = 5.8f; // seconds before impact the bunker's lasers/guns open fire on an incoming warhead
static const float BUNKER_ANTINUKE_CHANCE = 0.85f;// base interception chance (1.0 when surrounded in overdrive)
// A bunker also watches over the structures around it, the civilian town included: it answers whoever attacks them as if they attacked the bunker,
// its seismic sensors find hidden enemies nearby, and its crew mends what is damaged around it once the shooting stops.
static const float BUNKER_GUARD_RADIUS = 9.0f;    // tiles: friendly structures it protects
static const float BUNKER_MEND_RATE = 0.006f;     // fraction of max health per second those structures get back (only out of combat) ...
static const float BUNKER_MEND_DELAY = 5.0f;      // ... this long after they were last hit
static const float BUNKER_SENSOR = 7.0f;          // tiles: seismic sensors find hidden soldiers (snipers) near the bunker (a spy drone in the air goes unfelt)
// Civilian infrastructure: raised only by a civilian dozer (same price as a dozer, it mends only the town), it needs no power and runs on its own. Every civilian
// structure pays its army every CIVIC_INTERVAL seconds and sends out a free dozer now and then (a military one and a civilian one in turn).
// It has no guns and no plating of its own: only the army's defences (turrets, batteries, anti-nuke towers, troops) protect it, and spies can take it.
enum CivicKind { CV_HOUSE = 0, CV_APARTMENTS, CV_SKYSCRAPER, CV_GAS, CV_GROCERY, CV_EVENTHALL, CV_SQUARE, CV_TOWNHALL, CV_GOVERNMENT, CV_COUNT };
struct CivicType { int income; float dozerEvery; int people; };   // credits per payout, seconds between free dozers, townsfolk walking about it
extern const CivicType CIVICS[CV_COUNT];
inline int civicKind(int buildType) {
    if (buildType >= B_C_HOUSE && buildType <= B_C_GOVERNMENT) return buildType - B_C_HOUSE;
    if (buildType >= B_K_HOUSE && buildType <= B_K_GOVERNMENT) return buildType - B_K_HOUSE;
    return -1;
}
inline bool civicDozerType(int unitType) { return unitType == U_C_CIVDOZER || unitType == U_K_CIVDOZER; }
static const float CIVIC_INTERVAL = 10.0f;        // seconds between two payouts of a civilian structure
static const int   CIVIC_MAX = 30;                // civilian structures per player
static const int   CIVIC_DOZER_CAP = 5;           // no free dozers once the army has this many (both kinds counted: more made bases rebuild faster than they fell)
// Underground Nuke: a buried silo that fires the same kind of warhead as a Nuke Ramp, but silver and three times as strong: three times the
// blast radius, the damage and the radiation (strength and how long it lasts). It is as hard to destroy as a bunker (blast doors, plating, and
// only soldiers and vehicles can take it below BUNKER_FLOOR), and only an Anti-Nuke Laser can shoot its warhead down.
static const float UNUKE_SCALE = 9.0f;            // share of a full warhead's blast AREA: radius NUKE_RADIUS * sqrt(9) = three times a ramp's
static const float UNUKE_DAMAGE = 3.0f;           // damage multiplier over a ramp's warhead
static const float UNUKE_FALLOUT = 3.0f;          // radiation: strength and duration multiplier
static const float UNUKE_DOOR_HP = 4000.0f;       // the silo's blast doors (they take the damage first, like a bunker's hatch)
static const float UNUKE_ARM = 60.0f;             // seconds after it is finished before the first warhead is ready (then NUKE_COOLDOWN between launches)
static const Color UNUKE_GLOW = rgb(206, 210, 218); // the silver of its warhead, cloud and radiation
// Spies
static const float SPY_CAPTURE_BASE = 4.0f, SPY_CAPTURE_PER_TILE = 0.7f;   // seconds to capture a structure: base + per footprint tile
static const float SPY_REACH = 22.0f;             // px from the structure's edge
// Veterancy of infantry: kills promote a soldier
static const int   VET_KILLS = 4, ELITE_KILLS = 10;

static const int START_CASH = 10000;
static const int SUPPLY_PER_TRIP = 300;
static const float HARVEST_TIME = 3.0f;   // seconds at pile
static const float UNLOAD_TIME = 1.0f;
static const float NUKE_COOLDOWN = 300.0f;    // seconds between launches, per Nuke Ramp
static const float NUKE_RADIUS = 11.0f;        // tiles
static const float INCOME_INTERVAL = 5.0f;    // seconds between payouts of an income structure
static const int   INCOME_CYBER = 450;        // credits per payout of a Bitcoin Datacenter
static const int   INCOME_CLANKER = 380;      // credits per payout of an Oil Well
static const int   INCOME_MAX = 4;            // income structures per player
static const float REARM_TIME = 0.45f;    // seconds to reload one round of aircraft ammunition on the pad
static const int   HQ_MAX = 3;                // Command Cores / Posts per player (a dozer can rebuild a lost one, or found a second base)
static const float HEAL_RADIUS = 5.0f;        // tiles: a Medic's healing aura
static const float HEAL_RATE = 0.06f;         // fraction of max health restored per second to units in the aura (a third of that for structures)
static const float BOMBER_TURN = 2.3f;        // rad/s: a bomber swings a wide circle (about 110 px at its cruise speed) between runs
static const int   BOMB_STICK = 4;            // bombs released in one pass, a few tens of pixels apart along the flight line
static const int   W_NEEDLE = 27, W_SNIPER_RIFLE = 28;   // the snipers' rifles
static const int   W_BOMB_CYBER = 25, W_BOMB_CLANKER = 26;   // WEAPONS indices of the two armies' bombs
static const float JET_TURN = 3.4f;           // rad/s at top speed: a supersonic jet at 560 px/s swings a turn circle of about 165 px
// fixed-wing flight (jets and the Wraith flying wing): the turn is g-limited, so a slower aircraft turns tighter (up to AIR_TURN_MAX x its
// turn rate at top speed); below AIR_STALL x top speed it would stall, so only the final approach to the pad is flown slower than that
static const float AIR_STALL = 0.36f;
static const float AIR_TURN_MAX = 2.0f;
static const float AIR_LOITER_TIME = 60.0f;   // seconds an aircraft circles (or hovers over) a spot it was sent to before it flies home
static const float AIR_PAD_REPAIR = 0.03f;    // fraction of max health an aircraft parked on its pad gets back per second

int firstUnitOf(Faction f);   // range helpers for iterating faction units/structures
int firstBuildOf(Faction f);
static const int UNITS_PER_FACTION = 12;
// (AIRFIELD_CAP is declared with the other structure constants above: an oversized airfield houses 25 fixed-wing aircraft.)

static const int BUILDS_PER_FACTION = 11;
static const float HELI_SPACE = 46.0f;        // px between the centres of neighbouring hovering helicopters (the drawn hulls are about 40 px
                                              // across, so this leaves a small gap between them)
