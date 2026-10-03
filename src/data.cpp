#include "data.h"

// mult order: INF, LIGHT, HEAVY, STRUCT, AIR
const Weapon WEAPONS[] = {
    /* 0 */ { "Pulse Rifle",     11, 5.0f, 0, 0.55f, PJ_BULLET, 0,    true, false, {1.0f, 0.35f, 0.20f, 0.15f, 0.0f}, 1, 900,  SND_RIFLE,       rgb(140, 230, 255) },
    /* 1 */ { "Laser Rifle",     48, 6.0f, 0, 1.50f, PJ_LASER,  0,    true, true,  {0.4f, 1.00f, 1.00f, 0.60f, 0.9f}, 1, 0,    SND_LASER,       rgb(90, 220, 255) },
    /* 2 */ { "Arc Caster",      20, 4.2f, 0, 0.85f, PJ_ARC,    0.6f, true, false, {1.5f, 0.30f, 0.15f, 0.20f, 0.0f}, 1, 0,    SND_ARC,         rgb(190, 220, 255) },
    /* 3 */ { "Photon Cannon",   62, 6.5f, 0, 1.50f, PJ_LASER,  0,    true, false, {0.6f, 1.00f, 0.90f, 0.60f, 0.0f}, 1, 0,    SND_LASER_HEAVY, rgb(80, 200, 255) },
    /* 4 */ { "Volt Coil",       28, 5.0f, 0, 0.95f, PJ_ARC,    0.8f, true, true,  {1.6f, 0.45f, 0.25f, 0.30f, 0.7f}, 1, 0,    SND_ARC,         rgb(200, 230, 255) },
    /* 5 */ { "Railgun",        115, 8.5f, 2, 4.00f, PJ_RAIL,   0,    true, false, {0.5f, 1.00f, 1.20f, 1.00f, 0.0f}, 1, 0,    SND_RAIL,        rgb(255, 255, 255) },
    /* 6 */ { "Drone Laser",     70, 5.5f, 0, 0.85f, PJ_LASER,  0,    true, true,  {0.9f, 1.10f, 0.90f, 0.85f, 1.1f}, 1, 0,    SND_LASER,       rgb(120, 230, 255) },
    /* 7 */ { "Laser Turret",    90, 8.0f, 0, 1.10f, PJ_LASER,  0,    true, false, {0.9f, 1.00f, 0.80f, 0.50f, 0.0f}, 1, 0,    SND_LASER_HEAVY, rgb(80, 200, 255) },
    /* 8 */ { "Patriot Missile",110, 9.0f, 0, 1.20f, PJ_ROCKET, 0.6f, true, true,  {0.6f, 1.00f, 1.00f, 0.60f, 2.0f}, 1, 520,  SND_ROCKET,      rgb(240, 240, 240) },
    /* 9 */ { "Assault Rifle",   10, 5.0f, 0, 0.50f, PJ_BULLET, 0,    true, false, {1.0f, 0.35f, 0.20f, 0.15f, 0.0f}, 1, 900,  SND_RIFLE,       rgb(255, 220, 120) },
    /* 10 */{ "RPG",             52, 6.0f, 0, 2.00f, PJ_ROCKET, 0.3f, true, true,  {0.3f, 1.00f, 1.00f, 0.80f, 0.8f}, 1, 420,  SND_ROCKET,      rgb(255, 200, 120) },
    /* 11 */{ "Heavy MG",        12, 5.5f, 0, 0.16f, PJ_BULLET, 0,    true, true,  {1.2f, 0.40f, 0.20f, 0.15f, 0.3f}, 1, 950,  SND_MG,          rgb(255, 230, 140) },
    /* 12 */{ "120mm Cannon",    62, 6.0f, 0, 2.10f, PJ_SHELL,  0.4f,true, false, {0.5f, 1.00f, 1.00f, 0.70f, 0.0f}, 1, 480,  SND_CANNON,      rgb(255, 210, 120) },
    /* 13 */{ "Gatling Gun",      8, 5.5f, 0, 0.12f, PJ_BULLET, 0,    true, true,  {1.3f, 0.40f, 0.20f, 0.15f, 0.9f}, 1, 1000, SND_MG,          rgb(255, 240, 160) },
    /* 14 */{ "Rocket Salvo",    40, 10.0f,3, 4.20f, PJ_ROCKET, 0.85f,true, false, {1.0f, 0.90f, 0.80f, 1.10f, 0.0f}, 4, 380,  SND_ROCKET,      rgb(255, 190, 110) },
    /* 15 */{ "Gunship Rockets", 58, 5.5f, 0, 1.00f, PJ_ROCKET, 0.5f, true, true,  {1.0f, 1.10f, 0.90f, 1.00f, 0.7f}, 2, 520,  SND_ROCKET,      rgb(255, 200, 120) },
    /* 16 */{ "Gun Nest",        13, 6.5f, 0, 0.20f, PJ_BULLET, 0,    true, true,  {1.3f, 0.55f, 0.40f, 0.20f, 0.5f}, 1, 1000, SND_MG,          rgb(255, 230, 140) },
    /* 17 */{ "Rocket Battery",  56, 8.0f, 0, 1.60f, PJ_ROCKET, 0.4f, true, true,  {0.5f, 0.90f, 0.90f, 0.50f, 1.5f}, 1, 520,  SND_ROCKET,      rgb(255, 210, 140) },
    /* 18 */{ "Storm Shell",     95, 0.0f, 0, 1.00f, PJ_SHELL,  1.0f, true, false, {1.2f, 1.00f, 0.90f, 1.00f, 0.0f}, 1, 600,  SND_CANNON,      rgb(255, 210, 120) },
    /* 19 */{ "Ion Lance",      120, 9.0f, 2, 3.20f, PJ_RAIL,   0,    true, false, {1.4f, 1.10f, 0.90f, 0.50f, 0.0f}, 1, 0,    SND_RAIL,        rgb(190, 250, 255) },
    /* 20 */{ "Twin Ion Cannon", 74, 7.5f, 0, 1.30f, PJ_LASER,  0,    true, true,  {0.7f, 1.10f, 1.10f, 0.90f, 0.7f}, 2, 0,    SND_LASER_HEAVY, rgb(120, 240, 255) },
    /* 21 */{ "Grenade Launcher",70, 6.5f, 1.5f, 2.20f, PJ_SHELL, 1.1f, true, false, {1.4f, 0.90f, 0.70f, 1.20f, 0.0f}, 1, 330,  SND_CANNON,      rgb(255, 190, 110) },
    /* 22 */{ "Behemoth Cannon", 78, 6.8f, 0, 2.00f, PJ_SHELL,  0.7f, true, false, {0.6f, 1.00f, 1.10f, 0.90f, 0.0f}, 2, 520,  SND_CANNON,      rgb(255, 200, 110) },
    /* 23 */{ "Plasma Lances",   44, 6.0f, 0, 2.30f, PJ_LASER,  0,    true, true,  {0.8f, 1.00f, 0.70f, 0.40f, 1.8f}, 6, 0,    SND_LASER_HEAVY, rgb(150, 245, 255) },
    /* 24 */{ "Sidewinder Salvo",62, 7.0f, 0, 2.30f, PJ_ROCKET, 0.55f,true, true,  {0.9f, 1.10f, 0.90f, 0.80f, 1.6f}, 4, 880,  SND_ROCKET,      rgb(255, 214, 140) },
};
const int WEAPON_COUNT = sizeof(WEAPONS) / sizeof(WEAPONS[0]);

const UnitType UNITS[U_COUNT] = {
    // name, key, faction, kind, role, armor, hp, speed, cost, time, sight, radius, weapon, ammo, builtBy, requires, desc, program
    { "Fabricator",     "D", F_CYBER, UK_VEH, UR_DOZER,     AR_LIGHT, 400, 88,  1000, 12, 6, 12, -1, 0, BR_HQ,       -1,         "Constructs structures" },
    { "E-Hauler",       "H", F_CYBER, UK_VEH, UR_HARVESTER, AR_LIGHT, 420, 100, 700,  9,  6, 12, -1, 0, BR_SUPPLY,   -1,         "Hauls supplies to a Supply Hub" },
    { "Trooper",        "R", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   115, 58,  210,  5,  6, 6,  0,  0, BR_BARRACKS, -1,         "Pulse rifle, anti-infantry" },
    { "Laser Trooper",  "L", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   100, 55,  325,  7,  6, 6,  1,  0, BR_BARRACKS, -1,         "Laser rifle, anti-armor and anti-air" },
    { "Shock Trooper",  "S", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   140, 52,  400,  8,  6, 6,  2,  0, BR_BARRACKS, B_C_TECH,   "Arc caster, shreds infantry groups" },
    { "Photon Tank",    "T", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 720, 84,  900,  12, 7, 13, 3,  0, BR_FACTORY,  -1,         "Fast electric tank with photon cannon" },
    { "Volt Walker",    "V", F_CYBER, UK_VEH, UR_COMBAT,    AR_LIGHT, 560, 76,  800,  11, 7, 12, 4,  0, BR_FACTORY,  -1,         "Volt coil, arcs across infantry and drones" },
    { "Railgun Tank",   "G", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 700, 62,  1300, 17, 8, 14, 5,  0, BR_FACTORY,  B_C_TECH,   "Long range railgun, punches heavy armor" },
    { "Wraith Drone",   "W", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   720, 250, 1200, 16, 9, 10, 6,  12, BR_AIRFIELD, -1,        "Strike drone, 12 rounds, reloads in seconds on the Drone Pad" },
    { "Ion Lancer",     "I", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   150, 50,  750,  12, 8, 6,  19, 0, BR_BARRACKS, B_C_TECH,   "Elite sniper, piercing ion lance out to 9 tiles", 1 },
    { "Aegis Titan",    "N", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 1700, 58, 2300, 26, 8, 17, 20, 0, BR_FACTORY,  B_C_TECH,   "Super-heavy walker, twin ion cannons hit ground and air", 1 },
    { "Specter Jet",    "J", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   900, 560, 1800, 20, 11, 11, 23, 8, BR_AIRFIELD, B_C_TECH,  "Supersonic fighter: plasma lances, 8 strafing passes, shreds aircraft and light armor", 0, 1 },

    { "Dozer",          "D", F_CLANKER, UK_VEH, UR_DOZER,     AR_LIGHT, 420, 82,  1000, 12, 6, 12, -1, 0, BR_HQ,       -1,         "Constructs structures" },
    { "Supply Truck",   "H", F_CLANKER, UK_VEH, UR_HARVESTER, AR_LIGHT, 450, 96,  700,  9,  6, 12, -1, 0, BR_SUPPLY,   -1,         "Hauls supplies to a Supply Depot" },
    { "Rifleman",       "R", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   120, 56,  200,  5,  6, 6,  9,  0, BR_BARRACKS, -1,         "Assault rifle, anti-infantry" },
    { "RPG Trooper",    "P", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   115, 54,  300,  7,  6, 6,  10, 0, BR_BARRACKS, -1,         "Rocket launcher, anti-armor and anti-air" },
    { "Gunner",         "G", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   220, 50,  380,  8,  6, 6,  11, 0, BR_BARRACKS, B_K_TECH,   "Heavy machine gun, sustained fire" },
    { "Brute Tank",     "T", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 900, 74,  900,  12, 7, 14, 12, 0, BR_FACTORY,  -1,         "Diesel main battle tank, 120mm cannon" },
    { "Gatling Tank",   "A", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 580, 80,  800,  11, 7, 12, 13, 0, BR_FACTORY,  -1,         "Gatling gun, anti-infantry and anti-air" },
    { "Rocket Launcher","M", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 450, 60,  1100, 16, 8, 13, 14, 0, BR_FACTORY,  B_K_TECH,   "Long range rocket artillery" },
    { "Vulture Gunship","W", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   850, 210, 1200, 16, 9, 11, 15, 16, BR_AIRFIELD, -1,        "Twin-rocket gunship, 16 rounds, reloads in seconds on the Airstrip" },
    { "Grenadier",      "B", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   170, 48,  700,  12, 7, 6,  21, 0, BR_BARRACKS, B_K_TECH,   "Elite lobber, splash grenades crack infantry and bunkers", 1 },
    { "Behemoth",       "N", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 2000, 52, 2300, 26, 8, 17, 22, 0, BR_FACTORY,  B_K_TECH,   "Super-heavy tank, twin 150mm shells with splash", 1 },
    { "Talon Jet",      "J", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   950, 540, 1800, 20, 11, 12, 24, 8, BR_AIRFIELD, B_K_TECH,  "Supersonic fighter: homing Sidewinders, 8 strafing passes, hunts aircraft and vehicles", 0, 1 },

    { "Medic Rig",      "Y", F_CYBER,   UK_VEH, UR_HEALER, AR_LIGHT, 520, 92, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Nano-repair field: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
    { "Field Medic",    "Y", F_CLANKER, UK_VEH, UR_HEALER, AR_LIGHT, 540, 88, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Repair crew: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
};

const BuildType BUILDS[B_COUNT] = {
    // name, key, faction, role, hp, cost, time, w, h, power, sight, weapon, requires, desc
    { "Command Core",    "C", F_CYBER, BR_HQ,       5000, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Fabricators. A Fabricator can raise another one if this falls (max 3)" },
    { "Fusion Reactor",  "P", F_CYBER, BR_POWER,    1400, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Hub",      "S", F_CYBER, BR_SUPPLY,   2400, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds E-Haulers" },
    { "Barracks",        "B", F_CYBER, BR_BARRACKS, 1800, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "Assembly Plant",  "F", F_CYBER, BR_FACTORY,  2800, 2000, 25, 4, 3, -3, 7,  -1, B_C_POWER, "Builds vehicles" },
    { "Drone Pad",       "A", F_CYBER, BR_AIRFIELD, 3400, 1000, 20, 5, 3, -2, 8,  -1, B_C_FACTORY,"Builds and rearms Wraith Drones" },
    { "Data Center",     "E", F_CYBER, BR_TECH,     2000, 2000, 30, 3, 3, -4, 8,  -1, B_C_FACTORY,"Unlocks Shock Trooper, Railgun Tank; EMP Strike, Orbital Scan, Overclock" },
    { "Laser Turret",    "L", F_CYBER, BR_TURRET,   1300, 1000, 14, 1, 1, -3, 8,  7,  B_C_POWER, "Heavy ground defense laser, needs power" },
    { "Patriot Battery", "T", F_CYBER, BR_AATURRET, 1700, 1200, 16, 2, 2, -3, 9,  8,  B_C_POWER, "Heavy missile defense, brutal against air, good on ground" },
    { "Bitcoin Datacenter","M", F_CYBER, BR_INCOME,  1600, 1600, 22, 3, 2, -5, 7, -1, B_C_POWER, "Mines $450 every 5s (max 4), half rate on low power" },
    { "Nuke Ramp",       "K", F_CYBER, BR_NUKE,     2600, 5000, 40, 3, 3, -6, 8, -1, B_C_TECH,  "Launches a devastating nuke every 5 min per ramp (key K): huge blast, mushroom cloud, lingering radiation" },

    { "Command Post",    "C", F_CLANKER, BR_HQ,       5200, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Dozers. A Dozer can raise another one if this falls (max 3)" },
    { "Diesel Generator","P", F_CLANKER, BR_POWER,    1500, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Depot",    "S", F_CLANKER, BR_SUPPLY,   2600, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds Supply Trucks" },
    { "Barracks",        "B", F_CLANKER, BR_BARRACKS, 2000, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "War Factory",     "F", F_CLANKER, BR_FACTORY,  3000, 2000, 25, 4, 3, -3, 7,  -1, B_K_POWER, "Builds vehicles" },
    { "Airstrip",        "A", F_CLANKER, BR_AIRFIELD, 3600, 1000, 20, 5, 3, -2, 8,  -1, B_K_FACTORY,"Builds and rearms Vulture Gunships" },
    { "Arms Lab",        "E", F_CLANKER, BR_TECH,     2200, 2000, 30, 3, 3, -3, 8,  -1, B_K_FACTORY,"Unlocks Gunner, Rocket Launcher; Shell Storm, Recon Flight, Ordnance" },
    { "Gun Nest",        "N", F_CLANKER, BR_TURRET,   1600, 900,  14, 1, 1, -1, 8,  16, -1,        "Machine gun bunker, ground and light air" },
    { "Rocket Battery",  "T", F_CLANKER, BR_AATURRET, 1400, 1200, 16, 2, 2, -3, 9,  17, B_K_POWER, "Rocket defense, ground and air" },
    { "Oil Well",        "O", F_CLANKER, BR_INCOME,   1500, 1400, 20, 2, 2, 0,  6, -1, B_K_POWER, "Pumps $380 every 5s forever (max 4), no power needed" },
    { "Nuke Ramp",       "K", F_CLANKER, BR_NUKE,     2800, 5000, 40, 3, 3, -5, 8, -1, B_K_TECH,  "Launches a devastating nuke every 5 min per ramp (key K): huge blast, mushroom cloud, lingering radiation" },
};

const PowerType POWERS[F_COUNT] = {
    { "EMP Strike",  "Disables enemy vehicles and structures in the area for 8s", 180.0f, 4.5f },
    { "Shell Storm", "Rains artillery shells on the area for 4s",                 180.0f, 3.5f },
};
const ScanType SCANS[F_COUNT] = {
    { "Orbital Scan", "Reveals the whole map for 30s",                               210.0f, 30.0f },
    { "Recon Flight", "A spotter plane reveals the whole map for 30s",               210.0f, 30.0f },
};
const ProgramType PROGRAMS[F_COUNT] = {
    { "Overclock Program", "Unlocks Ion Lancer and Aegis Titan", 2500, 45.0f },
    { "Heavy Ordnance",    "Unlocks Grenadier and Behemoth",     2500, 45.0f },
};

int firstUnitOf(Faction f) { return f == F_CYBER ? U_C_DOZER : U_K_DOZER; }
int firstBuildOf(Faction f) { return f == F_CYBER ? B_C_HQ : B_K_HQ; }
