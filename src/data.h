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
                UR_TRANSPORT,   // cargo chopper: carries infantry and vehicles
                UR_DECON };     // anti-radiation truck: unarmed, scrubs fallout in a field around itself (and takes damage doing it)
// Stealth classes: a stealthy unit or structure is invisible to an enemy (cannot be seen, selected or shot) unless one of the enemy's
// own detectors is close enough. Detector masks say which classes a unit can see.
enum StealthClass { ST_NONE = 0, ST_SPOTTER = 1,    // snipers: only a spy drone finds them (buildings are never stealthy)
                    ST_DRONE = 2,                    // the spy drone itself: only drones (its own kind, or the cheap bomber drones up close) find it
                    ST_INFIL = 3 };                  // infiltrators / spies (a class c is found by detector bit 1 << (c - 1)): only snipers, drones, aircraft (not the heavy bombers), jeeps, other
                                                     // infiltrators and the light defence turrets (Laser Turret / Gun Nest) find them
static const int DET_SPOTTER = 1, DET_DRONE = 2, DET_INFIL = 4;
static const float INFIL_DETECT = 5.0f;   // tiles: how close an ordinary infiltrator-spotter (infantry, jeep, aircraft) has to be to find one
// Infiltrators swim: one sent somewhere it cannot walk to (an island launcher out in the lake, the far bank of a river with no crossing near)
// paddles across on a small inflatable raft. Land is preferred: a water tile costs SWIM_COST in the route search, and the raft is slow.
static const float SWIM_COST = 1.6f;
static const float SWIM_SPEED = 0.6f;     // share of its walking speed an infiltrator keeps on the water

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
    U_C_STRIKEJET, U_K_STRIKEJET,   // the Air Strike's fighters: never built, one bombing pass each, then they leave the map (see STRIKE_*)
    U_C_CIVDOZER, U_K_CIVDOZER,     // civilian dozers: they raise the town (houses, shops, towers, the town hall), never military structures
    U_C_SBOMBER, U_K_SBOMBER,       // Supersonic Bomber: a very fast nuclear bomber with one compact nuclear bomb (see SBOMBER_* and nukeBomb)
    U_C_RADTRUCK, U_K_RADTRUCK,     // Anti-Radiation Truck: an unarmed support vehicle whose decontamination field scrubs nuclear fallout
    U_C_FIGHTER, U_K_FIGHTER,       // the plain fighter jet (Raptor / Saber): air superiority first, strafes ground targets too; no tech needed
    U_C_NUKEBOMBER, U_K_NUKEBOMBER, // the strategic bomber of a Nuclear Bomber call-in: never built, it crosses the map once, drops a mid-sized nuke and leaves
    U_C_PATROL, U_K_PATROL,         // the cheap patrol boat of the Naval Yard: small, fast and fragile, its gun and rockets hit aircraft, ships, troops and shore structures
    U_RIOTER, U_FIREBOMBER,         // civilian rioters (owned by RIOT_PLAYER, never built): an old rifle / petrol bombs (see Sim::updateRiots)
    U_C_CARRIER, U_K_CARRIER,       // aircraft carriers (Naval Yard + tech structure): a floating airbase whose own wing of jets flies from its deck
    U_C_CARGOSHIP, U_K_CARGOSHIP,   // military cargo ships: carry a whole battalion (vehicles, dozers, infantry) over the sea and land it on any shore
    U_C_CARRIERJET, U_K_CARRIERJET, // a carrier's jets: never built, they take off from their carrier, fight round it and land on it again (Sim::carrierJetRun)
    U_COUNT
};

enum BuildRole { BR_HQ = 0, BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_AIRFIELD, BR_TECH, BR_TURRET, BR_AATURRET, BR_INCOME, BR_NUKE, BR_BUNKER,
                 BR_NAVAL,       // Naval Yard: on the shore of the lake, builds the ships
                 BR_ANTINUKE,    // Anti-Nuke Laser / Anti-Nuke Missiles: small structures that shoot incoming warheads down
                 BR_CIVIC,       // civilian infrastructure (houses, shops, towers, the town hall): raised by a civilian dozer, pays and sends dozers on its own
                 BR_UNUKE,       // Underground Nuke silo: a buried launcher with blast doors and deep plating; its silver warhead has 3x a ramp's radius and radiation
                 BR_WALL,        // thick concrete/steel wall segment: unlimited, cheap, very hard to nuke through
                 BR_STRONGHOLD,  // neutral civilian castle / bunker / watchtower: empty at the start, any army's infantry may garrison it and fire from it
                 BR_ISLANDNUKE,  // neutral Island Nuke Launcher on a lake island: indestructible, taken only by an infiltrator, fires small nukes
                 BR_FWALL,       // Bastion Wall: indestructible wall segment (spies can take it), no guns and no point defence of its own
                 BR_WAYMAKER,    // Waymaker: placed like a structure, it clears the trees and rock under it into open ground and is gone (see WAYMAKER_*)
                 BR_HEALANT,     // Healing Antenna: a cheap, unlimited 1x1 mast that mends everything friendly round it and slowly scrubs radiation (see HEALANT_*)
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
    // neutral civilian strongholds placed by the map generator (never built; see BR_STRONGHOLD)
    B_C_CASTLE, B_C_CIVBUNKER, B_C_WATCHTOWER, B_C_COTTAGE, B_C_WORKSHOP,
    B_C_FORTRESS,             // the Mountain Fortress: a huge neutral stronghold in the hills (100 infantry)
    B_C_ISLANDNUKE,           // the Island Nuke Launcher (two of them, one on each lake island)
    B_C_FWALL, B_K_FWALL,     // Bastion Walls (see BR_FWALL)
    B_C_WAYMAKER, B_K_WAYMAKER,   // Waymakers (see BR_WAYMAKER)
    B_C_HEALANT, B_K_HEALANT,     // Healing Antennas (see BR_HEALANT)
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
    int naval;          // 1 = ship: lives and moves on water only (the lake or the ocean, whichever it was launched on), see Map::findNavalPath
    float evade;        // 0..1: the share of weapon hits that miss it (the Hotline Strike fighters are very hard to shoot down)
    int temp;           // 1 = a loan from a support power: cannot be built, selected or commanded, and leaves the map when its run is over
    int drones;         // protector drones it flies with (see Sim::droneCount): they shoot down rockets and shells aimed at it
    int nukeBomb;       // 1 = a nuclear bomber: its single bomb is a compact silver warhead with an Underground Nuke's damage (see W_NUKE_BOMB)
    float sprintSpeed;  // px/s: the top speed while it still carries its payload (0 = no sprint); it drops to 'speed' once the payload is gone
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
    int garrison = 0;   // neutral stronghold: infantry it can shelter (0 for every normal structure)
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
// The Command Core / Post is the keystone: the highest health pool in the game, heavy armour against conventional fire, and a high
// per-warhead ceiling - three direct hits bring it down and nothing less. The Underground Nuke silo is second, the bunker third, the
// tech structure fourth (Hyper Protection and Rugged Nanoshell multiply every structure the same way, so the order holds).
static const float HQ_ARMOR = 0.35f;          // fraction of conventional damage a Command structure takes
static const float HQ_NUKE = 0.40f;           // hard ceiling: one warhead takes at most this share of its max health (three direct hits kill it)
// Walls: a cheap, unlimited 1x1 segment. Sturdy enough to soak small arms and light vehicles (their armour), but a warhead flattens one
// at ground zero like any other small structure; Hyper Protection doubles its pool and caps a single warhead.
static const float WALL_HP = 1800.0f;
static const float WALL_ARMOR = 0.50f;        // fraction of conventional damage a wall takes
// Anti-Rocket walls and the Q defences share a point-defence envelope. A wall's laser is shorter-ranged than a dedicated anti-nuke tower but
// walls are unlimited, so a line of them can blanket a base. Each defence rolls its own chance and has its own reload.
static const float WALL_PD_COVER = 10.0f;     // tiles around an Anti-Rocket wall
static const float WALL_PD_CHANCE = 0.38f;    // chance to burn down one incoming rocket / shell / warhead
static const float WALL_PD_RELOAD = 3.2f;     // seconds before the same wall can engage again
static const float WALL_PD_AA = 190.0f;       // damage a wall laser does to an aircraft it catches
// Bastion Walls: the same price as a wall but nothing breaks them, not even a warhead. They catch fire like the island launchers, an enemy spy can
// take a segment (and its new owner may sell it), and they never mount a gun or a point-defence laser: only the ordinary wall does that.
// Neutral civilian strongholds (Old Castle / Civilian Bunker / Watchtower): map landmarks that start empty and unowned. Any army's infantry
// can walk in; the first one inside claims the place for their army and the garrison fires from its windows and loopholes. They are deliberately
// tough: heavy masonry, a damage reduction and a per-warhead ceiling, but no army upgrade ever applies to them.
static const float STRONGHOLD_ARMOR = 0.62f;  // fraction of conventional damage a stronghold takes
static const float STRONGHOLD_NUKE = 0.58f;   // hard ceiling: one warhead takes at most this share of its max health
inline bool strongholdRole(BuildRole r) { return r == BR_STRONGHOLD; }
inline bool wallRole(BuildRole r) { return r == BR_WALL || r == BR_FWALL; }   // 1x1 segments meant to be chained into a line (no gap rules)
inline bool neutralRole(BuildRole r) { return r == BR_STRONGHOLD || r == BR_ISLANDNUKE; }   // placed by the map generator: never raised by a dozer, never sold
inline bool invulnerableRole(BuildRole r) { return r == BR_ISLANDNUKE || r == BR_FWALL; }   // nothing damages them: they burn (see Entity.heat) and only a spy takes them
inline bool civilianRole(BuildRole r) { return r == BR_CIVIC || r == BR_STRONGHOLD || r == BR_ISLANDNUKE || r == BR_FWALL || r == BR_WAYMAKER; }   // no Rugged / Hyper / roof-gun upgrades
// Waymaker: a dozer places it like a structure on trees, rocks or a ridge (never on water, a bridge, a structure or a supply pile) and it clears the
// whole footprint into open, walkable ground by itself in WAYMAKER_TIME seconds: no dozer has to reach it, so a dozer can cut its own way into
// rocky ground one Waymaker at a time and lay a road with a chain of them. It is gone once the ground is clear (nothing stays to defend or sell).
static const float WAYMAKER_TIME = 2.5f;
// Healing Antenna (both armies): a cheap 1x1 mast with no limit on how many stand. Every half second it mends everything of its own army and its
// allies within HEALANT_RADIUS: infantry, vehicles, ships, aircraft overhead and structures (finished ones; another antenna too). Several antennas
// over one spot do not add up (one pulse per tick mends a thing). It also scrubs fallout zones it reaches, slowly: all the antennas on one zone
// together stay well below a single Anti-Radiation Truck (RADTRUCK_SCRUB), and they shield nobody from the radiation. As fragile as an ordinary small structure.
static const float HEALANT_RADIUS = 5.0f;      // tiles
static const float HEALANT_RATE = 0.03f;       // share of max health restored per second to units (half of a Medic's HEAL_RATE) ...
static const float HEALANT_STRUCT = 0.5f;      // ... and this share of that to structures
static const float HEALANT_SCRUB = 0.04f;      // share of a fallout zone's strength and radius scrubbed per second per antenna (exponential) ...
static const float HEALANT_SCRUB_MAX = 0.12f;  // ... and by all the antennas on one zone together, at most (a truck does RADTRUCK_SCRUB alone)
// The Mountain Fortress: the biggest neutral stronghold, dug into the hills. 100 infantry fire from it at once (ground and air, each with its own
// weapon), its masonry shrugs off most conventional fire and no warhead takes more than FORTRESS_NUKE of its full health: it takes several nukes.
static const float FORTRESS_ARMOR = 0.30f;    // fraction of conventional damage it takes
static const float FORTRESS_NUKE = 0.24f;     // hard ceiling per warhead (five direct hits from full health)
static const float FORTRESS_REACH = 1.5f;     // tiles of extra reach its garrison gets from the high firing galleries
// Island Nuke Launchers: two on each lake island, neutral at the start. Indestructible (fire and fallout only make them burn), taken only by an
// infiltrator, and the only way to silence one is to take it back. Each launch is a Super Nuke (see ISLAND_NUKE_SCALE and the rest below the
// Underground Nuke). Its laser chamber flares when something hits it.
static const float ISLAND_NUKE_ARM = 30.0f;          // seconds after the match starts (or after a capture) before it can fire
// Indestructible structures catch fire instead of losing health: heat rises with every hit (a warhead sets it ablaze at once) and bleeds away.
static const float HEAT_PER_DAMAGE = 1.0f / 700.0f;  // heat added per point of weapon damage that hits it (two tanks firing set a segment ablaze within seconds;
                                                     // a lone rifleman's fire barely outpaces the cooling)
static const float HEAT_COOL = 1.0f / 45.0f;         // heat lost per second (a full blaze dies down in about 45 s)
static const float HEAT_MAX = 1.6f;
static const float ANTINUKE_PD_AA = 230.0f;   // damage a Q defence does to an aircraft
// Nuclear-interception nerf: the Anti-Nuke Laser is 40% weaker against warheads, and an Anti-Rocket wall's laser is 70% weaker against them.
// Only warheads are affected: burning down rockets, shells, bombs and aircraft is unchanged, and the Clanker missile silo keeps its chance.
static const float ANTINUKE_LASER_NUKE_MUL = 0.60f;   // chance of an Anti-Nuke Laser to stop a warhead
static const float WALL_PD_NUKE_MUL = 0.30f;          // chance of an Anti-Rocket wall to stop a warhead
static const float PD_AIRCRAFT_RELOAD = 1.1f; // seconds between aircraft shots
static const int   AIRFIELD_CAP = 25;         // fixed-wing aircraft based at one oversized airfield: one per pad
static const float BARRACKS_SPEED = 3.0f;     // barracks infantry production multiplier (always, even on low power)
static const float SELF_REPAIR_RATE = 0.012f; // Self-Repair: fraction of max health a structure restores per second once out of combat
static const float SELF_REPAIR_HOT = 0.25f;   // ... and the share of that rate it keeps while still taking fire
static const float SELF_REPAIR_DELAY = 3.0f;  // seconds after the last hit before full-rate repair resumes
static const int   W_JEEP_MG = 33;                        // the pintle gun of a jeep (WEAPONS index)
static const int   W_DESTROYER_C = 34, W_DESTROYER_K = 35, W_CRUISER_C = 36, W_CRUISER_K = 37;   // the ships' guns and rocket salvos
static const int   W_STRIKE_C = 38, W_STRIKE_K = 39;      // the rockets of the Air Strike fighters
static const int   W_DEFENSE_LASER = 29, W_DEFENSE_MG = 30;   // the roof guns of the Defense Guns upgrade (Cyber laser, Clanker machine gun)
static const int   W_SBOMBER_GUN = 43;   // the Supersonic Bomber's defensive tail cannon (WEAPONS index)
static const int   W_NUKE_BOMB = 44;     // the compact nuclear bomb it drops (PJ_BOMB: handled by Sim::bombImpact, never fired like a gun)
static const int   W_FIGHTER_C = 45, W_FIGHTER_K = 46;   // the plain fighters' missiles and cannon
static const int   W_PATROL_C = 47, W_PATROL_K = 48;     // the patrol boats' autocannon and rocket pod (one mount that hits everything)
static const int   W_RIOT_RIFLE = 49, W_MOLOTOV = 50;     // the rioters' hunting rifles and petrol bombs
static const int   W_CARRIER_CIWS = 51;                   // an aircraft carrier's close-in guns (aircraft, missiles' launchers, boats that come close)
// Supersonic Bomber: a small fixed-wing nuclear bomber of both armies, built at the airfield with a tech structure. It is the fastest
// aircraft in the game while its one bomb is aboard; after the drop it holds the dash just long enough to clear its own fireball, then
// slows right down and is easy prey for anti-air. Its bomb is a compact casing (drawn small) holding a silver warhead: the damage is an
// Underground Nuke's (UNUKE_DAMAGE, lethal like a silver warhead) but the blast is only a Nuke Ramp's in area, so it looks much smaller.
static const float SBOMBER_DASH = 960.0f;         // px/s: top speed while the payload is still aboard (the strike jets do 900)
static const float SBOMBER_SLOW = 340.0f;         // px/s: top speed once the bomb has been dropped (it is a sitting duck then)
static const float SBOMBER_CLEAR = 1.2f;          // seconds after the bomb leaves it that it keeps the dash to clear its own blast
static const float SBOMBER_BOMB_SCALE = 1.0f;     // blast AREA as a share of a Nuke Ramp's: the compact fireball is a ramp's (not a silo's)
static const float SBOMBER_BOMB_FALL = 0.9f;      // seconds the compact bomb takes to fall from altitude to the ground
// Nuclear Bomber call-in (tech structure power): a strategic bomber enters from the map corner nearest its own base, flies straight over the
// target at altitude, drops one mid-sized nuclear bomb and flies on off the map. It is not an army unit (nobody commands it); strong anti-air
// (Patriots, Rocket Batteries, Anti-Nuke Lasers' point defence, Anti-Rocket walls, fighters) can bring it down before the drop.
static const float NUKEBOMBER_SCALE = 0.55f;      // share of a full warhead's blast area: between a bunker missile and a Nuke Ramp
static const float NUKEBOMBER_COOLDOWN = 210.0f;  // seconds, per tech structure
static const float NUKEBOMBER_OPEN = 240.0f;      // seconds into the match before the first call can be made
static const float NUKEBOMBER_FALL = 1.4f;        // seconds the bomb falls from the bomber to the ground
static const float NUKEBOMBER_LIFE = 45.0f;       // seconds after which a bomber still on the map is withdrawn
// Anti-Radiation Truck (both armies, built at the Assembly Plant / War Factory): an unarmed support vehicle with a decontamination field.
// Any fallout zone the field touches is scrubbed: its strength and radius decay exponentially, units and structures inside the field are
// shielded from fallout, and the truck itself takes damage from the radiation it absorbs (medics heal it). It cannot heal a zone the enemy
// just fired: radiation also fades on its own, and a silver warhead's much bigger cloud takes the same time but hurts the truck more.
static const float RADTRUCK_RADIUS = 6.0f;     // tiles: the decontamination field around the truck
static const float RADTRUCK_SCRUB = 0.35f;     // share of a fallout zone's strength and radius scrubbed per second by one truck (exponential)
static const float RADTRUCK_DAMAGE = 0.05f;    // fraction of the truck's max health it absorbs per second per zone at full strength ...
static const float RADTRUCK_DRAIN_CAP = 0.20f; // ... per second at most (so a silver cloud cannot dissolve a truck in one tick)

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

// Air Strike (the tech structure's strike power, key I): a flight of light-grey strike fighters comes in low from the open sea behind the caller's
// base, in waves of V formations a couple of seconds apart, each wave crossing the called spot on a different heading so the whole area is worked
// over. Every fighter makes ONE pass: a pair of rockets at whatever stands in the circle on the run-in, then a stick of bombs laid along its own
// lane across the spot. Then it breaks hard - a barrel roll, a jink, a spray of flares - and runs on out to the edge of the map, where it fades into
// the haze over the ocean. They are not the army's aircraft: nobody commands them and none come home. Free of charge, once per STRIKE_COOLDOWN per
// tech structure (more Data Centers / Arms Labs: more calls); the flight grows with the army's drop tier.
static const float STRIKE_COOLDOWN = 240.0f;    // seconds, per tech structure
static const int   STRIKE_FLIGHT[DTI_COUNT] = { 10, 14, 18 };   // fighters in one call, by drop tier
static const int   STRIKE_WAVE = 6;             // fighters in one V formation
static const float STRIKE_WAVE_GAP = 2.4f;      // seconds between two waves
static const float STRIKE_TURN = 0.6f;          // radians between the headings of two successive waves (the second comes in from one side, the third the other)
static const int   STRIKE_BOMBS = 6;            // bombs in each fighter's stick
static const float STRIKE_BOMB_GAP = 34.0f;     // px between two bombs of a stick on the ground
static const int   STRIKE_ROCKETS = 2;          // rockets each fighter fires on its run-in
static const float STRIKE_SPREAD = 4.0f;        // tiles: the flight's lanes cover this far either side of the called spot
static const float STRIKE_LIFE = 50.0f;         // seconds after which a strike fighter still on the map is withdrawn
static const float STRIKE_FADE = 360.0f;        // px from the map edge over which a departing fighter fades out
static const int   W_STRIKE_BOMB = 52;          // their bombs (WEAPONS index)
struct StrikeType { const char* name; const char* desc; };
extern const StrikeType STRIKES[F_COUNT];

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

// The seas: ships live on the lake or on the ocean and a Naval Yard must touch one of them (a body of at least NAVAL_WATER_MIN connected water tiles).
// A ship stays on the sea it was launched on: the lake and the ocean never meet.
static const int   NAVAL_WATER_MIN = 60;
// Aircraft carrier: a floating airbase. Its wing of CARRIER_WING jets waits on deck; whenever the carrier or its allies see an enemy within
// CARRIER_RANGE of it, jets launch one after another and attack (air first, then ships, vehicles, troops and structures near the shore), come back
// to land when their missiles are spent or nothing is left to fight, and wait CARRIER_REARM on deck. A jet that is shot down is replaced on deck
// after CARRIER_REPLACE seconds. The jets belong to the carrier: nobody commands them, and if the carrier sinks the ones in the air fly off the map.
static const int   CARRIER_WING = 6;
static const float CARRIER_RANGE = 22.0f;      // tiles from the carrier
static const float CARRIER_LAUNCH_GAP = 1.2f;  // seconds between two launches
static const float CARRIER_REARM = 8.0f;       // seconds a jet spends on deck between sorties
static const float CARRIER_REPLACE = 30.0f;    // seconds to ready a replacement for a jet that was lost
// Cargo ship: an unarmed transport of CARGOSHIP_CAP cargo places (an infantryman 1, a light vehicle 4, a tank 6, a super-heavy 10). Units board it from
// a shore it lies beside (right-click it with units selected) and it lands them on any shore: "unload here" lets them out onto the nearest land,
// "unload at" sails it to the water nearest the spot first. If it sinks, everyone aboard is lost.
static const int   CARGOSHIP_CAP = 120;
static const float CARGOSHIP_REACH = 3.0f;     // tiles from the hull a boarding unit may stand
static const float CARGOSHIP_LAND = 5.0f;      // tiles: how close land must be to let people out

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
static const float BUNKER_FLOOR = 0.10f;          // aircraft, structures, shells from the sky and radiation can never take it below this share of its health - nuclear warheads ignore it entirely (a direct hit or a barrage destroys the bunker)
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
static const float MINI_NUKE_COOLDOWN = 90.0f;    // seconds between two launches of one bunker
static const float MINI_NUKE_ARM = 30.0f;         // seconds after a bunker is finished before its first missile is ready
static const float MINI_NUKE_REVEAL = 10.0f;      // seconds a launch is announced to the enemy (the missile climbs out of its hatch); the bunker itself is always visible
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
// Underground Nuke: a buried silo that fires the same kind of warhead as a Nuke Ramp, but silver and much stronger: three times the blast
// radius, 2.7 times the damage (10% weaker than it used to be) and three times the radiation (strength and how long it lasts). It has blast
// doors and deep plating (only soldiers and vehicles can take it below BUNKER_FLOOR; warheads ignore the floor and can destroy it), and only a laser (an Anti-Nuke Laser or an Anti-Rocket wall) can
// shoot its warhead down at all - and the armoured silver warhead halves even that chance (UNUKE_INTERCEPT_RESIST).
static const float UNUKE_SCALE = 9.0f;            // share of a full warhead's blast AREA: radius NUKE_RADIUS * sqrt(9) = three times a ramp's
static const float UNUKE_DAMAGE = 2.7f;           // damage multiplier over a ramp's warhead (10% weaker than the original 3x)
static const float UNUKE_FALLOUT = 3.0f;          // radiation: strength and duration multiplier
static const float UNUKE_INTERCEPT_RESIST = 0.5f; // the silver warhead is armoured against interception: every defence's chance to shoot it down is halved
static const float UNUKE_DOOR_HP = 10000.0f;      // the silo's blast doors (they take the damage first, like a bunker's hatch)
static const float UNUKE_ARM = 60.0f;             // seconds after it is finished before the first warhead is ready (then NUKE_COOLDOWN between launches)
static const Color UNUKE_GLOW = rgb(206, 210, 218); // the silver of its warhead, cloud and radiation
// Super Nuke: what an Island Nuke Launcher fires. The biggest warhead in the game: twice an Underground Nuke's blast radius (six times a ramp's),
// hitting harder still, and very fast (ISLAND_NUKE_FLIGHT instead of a ramp's seven seconds). Bunkers and interceptor missiles cannot touch it;
// only a laser (an Anti-Nuke Laser or an Anti-Rocket wall) can try, and every such chance is five times smaller than against an Underground Nuke.
static const float ISLAND_NUKE_SCALE = UNUKE_SCALE * 4.0f;   // share of a full warhead's blast AREA: radius twice an Underground Nuke's (NUKE_RADIUS * 6)
static const float ISLAND_NUKE_DAMAGE = 3.0f;        // damage multiplier over a ramp's warhead (an Underground Nuke's is UNUKE_DAMAGE)
static const float ISLAND_NUKE_FLIGHT = 2.6f;        // seconds from launch to impact (Sim::NUKE_FLIGHT for every other warhead)
static const float ISLAND_NUKE_INTERCEPT_RESIST = UNUKE_INTERCEPT_RESIST / 5.0f;   // each laser's chance to shoot it down, as a share of its usual chance
static const float ISLAND_NUKE_COOLDOWN = 75.0f;     // seconds between two launches of one launcher
static const float ISLAND_NUKE_FALLOUT_R = 0.45f;    // its fallout zone's radius, as a share of the blast radius (the outer blast is a pressure wave, not a crater)
static const Color ISLAND_NUKE_GLOW = rgb(255, 120, 210);   // the magenta-white of its warhead, fireball and radiation
// Civilian areas: the towns and cities round the lake (Map::districts). Every one starts neutral; an infiltrator of any army takes the WHOLE area
// at once (any of its buildings will do), and from then on it pays its holder and is a target for the holder's enemies. It is never wiped out:
// a block that is razed collapses into a ruin that its people raise again by themselves (a dozer of the holder, an ally, or of anybody while the
// area is neutral speeds that up a lot), and a damaged block is patched up the same way once the shooting stops. A badly damaged area pays nothing.
static const int   DISTRICT_MAX = 16;
static const float DISTRICT_PAY_INTERVAL = 10.0f;    // seconds between two payouts
static const float DISTRICT_PAY_SHARE = 0.35f;       // pay per block as a share of the same civic building's pay in a player's own town (CIVICS[].income): a whole town pays about half an income structure
static const float DISTRICT_PAY_MIN = 0.5f;          // integrity (health of the whole area, ruins included) below which it pays nothing
static const float DISTRICT_HP = 1.6f;               // a block's health as a multiple of the civic building's (brick and concrete, full of people)
static const float DISTRICT_MEND = 0.010f;           // share of max health a damaged block patches up per second (once out of combat) ...
static const float DISTRICT_MEND_DELAY = 6.0f;       // ... this long after the last hit
static const float DISTRICT_REBUILD = 75.0f;         // seconds the townsfolk alone take to raise a razed block from its ruins ...
static const float DISTRICT_REBUILD_DELAY = 8.0f;    // ... starting this long after it was last hit (a dozer works at its own build rate on top: about 5x faster)
static const float DISTRICT_RUIN_SAFE = 0.25f;       // a ruin is not worth a shot until it has been raised this far again (nobody pounds rubble)
static const float DISTRICT_CAPTURE = 9.0f;          // seconds an infiltrator needs to take a whole area
static const float DISTRICT_HELP_RANGE = 30.0f;      // tiles: how far an idle dozer goes on its own to help raise its army's ruined blocks
// Riots: when an army loses control - its forces outgunned by its enemies, its base burning, its losses piling up - its own population turns on it.
// Groups of armed civilians (owned by RIOT_PLAYER: they belong to no army and fight every army) gather in the open ground near the base, pick
// the weakest-guarded infrastructure and wreck it, and some march on the towns the army holds round the lake. A strong army sees only the odd small
// group; a beaten one faces bigger crowds, more often. Unrest builds per army (Sim::unrest) and a group forms each time it fills.
static const float RIOT_GRACE = 300.0f;              // seconds into the match before anyone riots
static const float RIOT_CALM_RATE = 1.0f / 480.0f;   // unrest per second of a fully in-control army (one small group every eight minutes)
static const float RIOT_ANGRY_RATE = 1.0f / 40.0f;   // ... and of an army that has lost all control (a big crowd every 40 s)
static const int   RIOT_MIN_GROUP = 3, RIOT_MAX_GROUP = 12;
static const int   RIOT_CAP = 72;                    // rioters on the map at once, all armies together
static const float RIOT_LIFE = 210.0f;               // seconds a group keeps at it before the survivors melt away
static const float RIOT_HP = 0.8f;                   // a rioter's health as a share of the type's (they wear no armour)
// Spies
static const float SPY_CAPTURE_BASE = 4.0f, SPY_CAPTURE_PER_TILE = 0.7f;   // seconds to capture a structure: base + per footprint tile
static const float SPY_REACH = 22.0f;             // px from the structure's edge
// Veterancy of infantry: kills promote a soldier
static const int   VET_KILLS = 4, ELITE_KILLS = 10;

static const int START_CASH = 10000;
static const int SUPPLY_PER_TRIP = 300;
static const float HARVEST_TIME = 3.0f;   // seconds at pile
static const float UNLOAD_TIME = 1.0f;
static const float NUKE_COOLDOWN = 360.0f;    // seconds between launches, per Nuke Ramp (and Underground Nuke)
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
