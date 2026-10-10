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
    U_COUNT
};

enum BuildRole { BR_HQ = 0, BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_AIRFIELD, BR_TECH, BR_TURRET, BR_AATURRET, BR_INCOME, BR_NUKE, BR_BUNKER };

enum BuildTypeId {
    B_C_HQ = 0, B_C_POWER, B_C_SUPPLY, B_C_BARRACKS, B_C_FACTORY, B_C_AIRFIELD, B_C_TECH, B_C_LASER, B_C_PATRIOT, B_C_MINER, B_C_NUKE,
    B_K_HQ, B_K_POWER, B_K_SUPPLY, B_K_BARRACKS, B_K_FACTORY, B_K_AIRFIELD, B_K_TECH, B_K_MG, B_K_ROCKET, B_K_OILWELL, B_K_NUKE,
    B_C_BUNKER, B_K_BUNKER,   // the underground bunkers sit after both armies' blocks so every earlier id stays put
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
enum UpgradeId { UPG_RUGGED = 0, UPG_GUNS, UPG_REPAIR, UPG_COUNT };
struct UpgradeType { const char* name; const char* hotkey; const char* desc; int cost; float time; };
extern const UpgradeType UPGRADES[F_COUNT][UPG_COUNT];
static const float RUGGED_HP = 1.5f;          // Rugged: structure health multiplier
static const float RUGGED_ARMOR = 0.5f;       // Rugged: fraction of incoming damage a structure takes
static const float RUGGED_SWAMP_LO = 0.03f;  // Rugged: the plating starts to give once this share of max health lands within ~2 s ...
static const float RUGGED_SWAMP_HI = 0.11f;  // ... and is overwhelmed (full damage) from this share on: only a massive assault breaks through
static const float RUGGED_NUKE = 0.55f;       // Rugged: fraction of max health one warhead takes at ground zero (two close nukes bring it down)
static const float SELF_REPAIR_RATE = 0.012f; // Self-Repair: fraction of max health a structure restores per second once out of combat
static const float SELF_REPAIR_HOT = 0.25f;   // ... and the share of that rate it keeps while still taking fire
static const float SELF_REPAIR_DELAY = 3.0f;  // seconds after the last hit before full-rate repair resumes
static const int   W_DEFENSE_LASER = 29, W_DEFENSE_MG = 30;   // the roof guns of the Defense Guns upgrade (Cyber laser, Clanker machine gun)

// Paradrop support power of the tech structure (Zero Hour style): a cargo plane flies in over a spot of your choice and drops a free force on parachutes
static const int DROP_INF = 15, DROP_VEH = 7, DROP_AIR = 4;
struct DropType { const char* name; const char* desc; float cooldown; float radius; };   // radius: tiles around the target the troops land in
extern const DropType DROPS[F_COUNT];
extern const int DROP_INF_TYPES[F_COUNT][DROP_INF];
extern const int DROP_VEH_TYPES[F_COUNT][DROP_VEH];
extern const int DROP_AIR_TYPES[F_COUNT][DROP_AIR];

// Aid Drop: the human player's own power at the tech structure (Data Center / Arms Lab), never a computer army's. A white relief plane
// nobody shoots at drops crates on parachutes; the army nearest to the spot, other than the sender, receives the money and a dozer.
static const int   AID_MONEY = 20000;
static const float AID_COOLDOWN = 60.0f;      // seconds, per tech structure (more of them, more flights)
static const int   AID_CRATES = 5;            // crates in one drop (the dozer comes out of the middle one)
static const float AID_RADIUS = 3.0f;         // tiles: the crates come down within this of the spot

// Underground Bunker: a deep, hidden shelter that holds up to 50 infantry and one dozer. The garrison fires from a metal hatch on the surface;
// the hatch can be blown off (a nuke does it) which silences the bunker, but only infantry and vehicles can ever destroy the bunker itself.
static const int   BUNKER_INF_CAP = 50, BUNKER_DOZER_CAP = 1;
static const float BUNKER_HATCH_HP = 2800.0f;     // the surface opening
static const float BUNKER_PLATING = 0.22f;        // share of the damage that reaches the buried bunker once the hatch is gone
static const float BUNKER_FLOOR = 0.10f;          // aircraft, structures, shells from the sky, radiation and nukes can never take it below this share of its health
static const float BUNKER_LINGER = 25.0f;         // seconds a bunker a spy drone found stays known after the drone has moved on
static const float BUNKER_UNLOAD_GAP = 0.12f;     // seconds between two soldiers climbing out
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
static const int AIRFIELD_CAP = 4;            // fixed-wing aircraft (drones, jets) based at one airfield: one per pad. Helicopters do not count:
                                              // they land on helipads around the airfield, so an airfield can build and house any number of them
static const int BUILDS_PER_FACTION = 11;
static const float HELI_SPACE = 46.0f;        // px between the centres of neighbouring hovering helicopters (the drawn hulls are about 40 px
                                              // across, so this leaves a small gap between them)
