// One Hour - static game data: factions, weapons, unit and structure types
#pragma once
#include "common.h"

enum Faction { F_CYBER = 0, F_CLANKER = 1, F_COUNT = 2 };
static const char* const FACTION_NAME[F_COUNT] = { "Cyber Army", "Clanker Army" };

enum Armor { AR_INF = 0, AR_LIGHT, AR_HEAVY, AR_STRUCT, AR_AIR, AR_COUNT };

enum ProjKind { PJ_BULLET = 0, PJ_LASER, PJ_ARC, PJ_SHELL, PJ_ROCKET, PJ_RAIL };

enum Sound {
    SND_NONE = 0, SND_RIFLE, SND_MG, SND_LASER, SND_LASER_HEAVY, SND_ARC, SND_CANNON, SND_ROCKET,
    SND_RAIL, SND_EXPLODE_S, SND_EXPLODE_L, SND_HIT, SND_SELECT, SND_ORDER, SND_PLACE,
    SND_BUILD_DONE, SND_UNIT_READY, SND_NOFUNDS, SND_LOWPOWER, SND_ATTACKED, SND_VICTORY, SND_DEFEAT,
    SND_CLICK, SND_EMP, SND_SUPPLY, SND_AIR, SND_CANT, SND_COUNT
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
enum UnitRole { UR_COMBAT = 0, UR_DOZER, UR_HARVESTER };

enum UnitTypeId {
    U_C_DOZER = 0, U_C_HARV, U_C_INF1, U_C_INF2, U_C_INF3, U_C_TANK, U_C_VOLT, U_C_RAIL, U_C_AIR, U_C_ELITE, U_C_TITAN,
    U_K_DOZER, U_K_HARV, U_K_INF1, U_K_INF2, U_K_INF3, U_K_TANK, U_K_GATLING, U_K_MLRS, U_K_AIR, U_K_ELITE, U_K_TITAN,
    U_COUNT
};

enum BuildRole { BR_HQ = 0, BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_AIRFIELD, BR_TECH, BR_TURRET, BR_AATURRET, BR_INCOME, BR_NUKE };

enum BuildTypeId {
    B_C_HQ = 0, B_C_POWER, B_C_SUPPLY, B_C_BARRACKS, B_C_FACTORY, B_C_AIRFIELD, B_C_TECH, B_C_LASER, B_C_PATRIOT, B_C_MINER, B_C_NUKE,
    B_K_HQ, B_K_POWER, B_K_SUPPLY, B_K_BARRACKS, B_K_FACTORY, B_K_AIRFIELD, B_K_TECH, B_K_MG, B_K_ROCKET, B_K_OILWELL, B_K_NUKE,
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
};

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

static const int START_CASH = 10000;
static const int SUPPLY_PER_TRIP = 300;
static const float HARVEST_TIME = 3.0f;   // seconds at pile
static const float UNLOAD_TIME = 1.0f;
static const float NUKE_COOLDOWN = 300.0f;    // seconds between launches, per Nuke Ramp
static const float NUKE_RADIUS = 6.0f;        // tiles
static const float INCOME_INTERVAL = 5.0f;    // seconds between payouts of an income structure
static const int   INCOME_MAX = 4;            // income structures per player
static const float REARM_TIME = 0.45f;    // seconds to reload one round of aircraft ammunition on the pad

int firstUnitOf(Faction f);   // range helpers for iterating faction units/structures
int firstBuildOf(Faction f);
static const int UNITS_PER_FACTION = 11;
static const int BUILDS_PER_FACTION = 11;
