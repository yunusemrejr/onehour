#include "data.h"

// mult order: INF, LIGHT, HEAVY, STRUCT, AIR
const Weapon WEAPONS[] = {
    /* 0 */ { "Pulse Rifle",     11, 5.0f, 0, 0.55f, PJ_BULLET, 0,    true, false, {1.0f, 0.35f, 0.20f, 0.15f, 0.0f}, 1, 900,  SND_RIFLE,       rgb(140, 230, 255) },
    /* 1 */ { "Laser Rifle",     48, 6.0f, 0, 1.50f, PJ_LASER,  0,    true, true,  {0.4f, 1.00f, 1.00f, 0.60f, 0.9f}, 1, 0,    SND_LASER,       rgb(90, 220, 255) },
    /* 2 */ { "Arc Caster",      20, 4.2f, 0, 0.85f, PJ_ARC,    0.6f, true, false, {1.5f, 0.30f, 0.15f, 0.20f, 0.0f}, 1, 0,    SND_ARC,         rgb(190, 220, 255) },
    /* 3 */ { "Photon Cannon",   62, 6.5f, 0, 1.50f, PJ_LASER,  0,    true, false, {0.6f, 1.00f, 0.90f, 0.60f, 0.0f}, 1, 0,    SND_LASER_HEAVY, rgb(80, 200, 255) },
    /* 4 */ { "Volt Coil",       28, 5.0f, 0, 0.95f, PJ_ARC,    0.8f, true, true,  {1.6f, 0.45f, 0.25f, 0.30f, 0.7f}, 1, 0,    SND_ARC,         rgb(200, 230, 255) },
    /* 5 */ { "Railgun",        115, 8.5f, 2, 4.00f, PJ_RAIL,   0,    true, false, {0.5f, 1.00f, 1.20f, 1.00f, 0.0f}, 1, 0,    SND_RAIL,        rgb(255, 255, 255) },
    /* 6 */ { "Drone Laser",     66, 5.8f, 0, 0.70f, PJ_LASER,  0,    true, true,  {0.9f, 1.10f, 0.90f, 0.85f, 1.5f}, 1, 0,    SND_LASER,       rgb(120, 230, 255) },
    /* 7 */ { "Laser Turret",   310, 9.0f, 0, 0.90f, PJ_LASER,  0,    true, false, {1.0f, 1.10f, 1.00f, 0.60f, 0.0f}, 1, 0,    SND_LASER_HEAVY, rgb(80, 200, 255) },
    /* 8 */ { "Patriot Missile",240, 10.5f,0, 1.30f, PJ_ROCKET, 0.9f, true, true,  {0.8f, 1.10f, 1.10f, 0.70f, 2.2f}, 2, 640,  SND_ROCKET,      rgb(240, 240, 240) },
    /* 9 */ { "Assault Rifle",   10, 5.0f, 0, 0.50f, PJ_BULLET, 0,    true, false, {1.0f, 0.35f, 0.20f, 0.15f, 0.0f}, 1, 900,  SND_RIFLE,       rgb(255, 220, 120) },
    /* 10 */{ "RPG",             64, 6.5f, 0, 1.90f, PJ_ROCKET, 0.35f,true, true,  {0.35f, 1.20f, 1.30f, 0.90f, 1.6f}, 1, 600,  SND_ROCKET,      rgb(255, 200, 120) },
    /* 11 */{ "Heavy MG",        12, 5.5f, 0, 0.16f, PJ_BULLET, 0,    true, true,  {1.2f, 0.40f, 0.20f, 0.15f, 0.3f}, 1, 950,  SND_MG,          rgb(255, 230, 140) },
    /* 12 */{ "120mm Cannon",    62, 6.0f, 0, 2.10f, PJ_SHELL,  0.4f,true, false, {0.5f, 1.00f, 1.00f, 0.70f, 0.0f}, 1, 480,  SND_CANNON,      rgb(255, 210, 120) },
    /* 13 */{ "Gatling Gun",      8, 5.5f, 0, 0.12f, PJ_BULLET, 0,    true, true,  {1.3f, 0.40f, 0.20f, 0.15f, 0.9f}, 1, 1000, SND_MG,          rgb(255, 240, 160) },
    /* 14 */{ "Rocket Salvo",    40, 10.0f,3, 4.20f, PJ_ROCKET, 0.85f,true, false, {1.0f, 0.90f, 0.80f, 1.10f, 0.0f}, 4, 380,  SND_ROCKET,      rgb(255, 190, 110) },
    /* 15 */{ "Gunship Rockets", 58, 5.8f, 0, 0.90f, PJ_ROCKET, 0.5f, true, true,  {1.0f, 1.10f, 0.90f, 1.00f, 1.5f}, 2, 520,  SND_ROCKET,      rgb(255, 200, 120) },
    /* 16 */{ "Gun Nest",        42, 7.5f, 0, 0.14f, PJ_BULLET, 0,    true, true,  {1.4f, 0.80f, 0.50f, 0.25f, 0.9f}, 1, 1100, SND_MG,          rgb(255, 230, 140) },
    /* 17 */{ "Rocket Battery", 150, 9.5f, 0, 1.80f, PJ_ROCKET, 0.7f, true, true,  {0.8f, 1.00f, 1.00f, 0.60f, 1.8f}, 3, 620,  SND_ROCKET,      rgb(255, 210, 140) },
    /* 18 */{ "Storm Shell",     95, 0.0f, 0, 1.00f, PJ_SHELL,  1.0f, true, false, {1.2f, 1.00f, 0.90f, 1.00f, 0.0f}, 1, 600,  SND_CANNON,      rgb(255, 210, 120) },
    /* 19 */{ "Ion Lance",      120, 9.0f, 2, 3.20f, PJ_RAIL,   0,    true, false, {1.4f, 1.10f, 0.90f, 0.50f, 0.0f}, 1, 0,    SND_RAIL,        rgb(190, 250, 255) },
    /* 20 */{ "Twin Ion Cannon", 74, 7.5f, 0, 1.30f, PJ_LASER,  0,    true, true,  {0.7f, 1.10f, 1.10f, 0.90f, 0.7f}, 2, 0,    SND_LASER_HEAVY, rgb(120, 240, 255) },
    /* 21 */{ "Grenade Launcher",70, 6.5f, 1.5f, 2.20f, PJ_SHELL, 1.1f, true, false, {1.4f, 0.90f, 0.70f, 1.20f, 0.0f}, 1, 330,  SND_CANNON,      rgb(255, 190, 110) },
    /* 22 */{ "Behemoth Cannon", 78, 6.8f, 0, 2.00f, PJ_SHELL,  0.7f, true, false, {0.6f, 1.00f, 1.10f, 0.90f, 0.0f}, 2, 520,  SND_CANNON,      rgb(255, 200, 110) },
    /* 23 */{ "Plasma Lances",   60, 6.0f, 0, 2.10f, PJ_LASER,  0,    true, true,  {0.9f, 1.10f, 0.90f, 0.85f, 2.0f}, 6, 0,    SND_LASER_HEAVY, rgb(150, 245, 255) },
    /* 24 */{ "Sidewinder Salvo",78, 7.0f, 0, 2.10f, PJ_ROCKET, 0.55f,true, true,  {1.0f, 1.10f, 1.00f, 1.00f, 1.9f}, 4, 880,  SND_ROCKET,      rgb(255, 214, 140) },
    /* 25 */{ "Plasma Bombs",   150, 1.0f, 0, 0.00f, PJ_BOMB,   2.0f, true, false, {1.2f, 1.25f, 1.00f, 1.50f, 0.0f}, 4, 0,    SND_EXPLODE_L,   rgb(120, 230, 255) },
    /* 26 */{ "Carpet Bombs",   165, 1.0f, 0, 0.00f, PJ_BOMB,   2.1f, true, false, {1.3f, 1.20f, 1.00f, 1.60f, 0.0f}, 4, 0,    SND_EXPLODE_L,   rgb(255, 176, 90) },
    /* 27 */{ "Needle Rifle",   150, 11.5f, 0, 2.80f, PJ_LASER,  0,    true, false, {1.00f, 0.00f, 0.00f, 0.00f, 0.0f}, 1, 0,    SND_RAIL,        rgb(170, 255, 236) },
    /* 28 */{ "Sniper Rifle",   140, 11.5f, 0, 2.60f, PJ_BULLET, 0,    true, false, {1.00f, 0.00f, 0.00f, 0.00f, 0.0f}, 1, 2200, SND_RIFLE,        rgb(255, 232, 160) },
    /* 29 */{ "Defense Laser",   58, 7.5f, 0, 0.80f, PJ_LASER,  0,    true, true,  {1.0f, 1.00f, 0.80f, 0.00f, 1.6f}, 1, 0,    SND_LASER,       rgb(130, 236, 255) },
    /* 30 */{ "Defense MG",      15, 7.5f, 0, 0.15f, PJ_BULLET, 0,    true, true,  {1.4f, 0.80f, 0.45f, 0.00f, 1.5f}, 1, 1100, SND_MG,          rgb(255, 226, 130) },
    /* 31 */{ "Hornet Lasers",   36, 6.0f, 0, 0.60f, PJ_LASER,  0,    true, true,  {1.0f, 1.10f, 0.85f, 0.60f, 1.3f}, 2, 0,    SND_LASER,       rgb(120, 236, 255) },
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
    { "Wraith Drone",   "W", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   520, 340, 850,  12, 9, 10, 6,  12, BR_AIRFIELD, -1,        "Cheap, fast, fragile bomber drone: carpet-bombs ground targets with plasma bombs (12 a sortie), laser kills aircraft", 0, 0, 1, 0, 0, ST_NONE, 3.5f, DET_DRONE },
    { "Ion Lancer",     "I", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   150, 50,  750,  12, 8, 6,  19, 0, BR_BARRACKS, B_C_TECH,   "Elite lancer, piercing ion lance out to 9 tiles", 1 },
    { "Aegis Titan",    "N", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 1700, 58, 2300, 26, 8, 17, 20, 0, BR_FACTORY,  B_C_TECH,   "Super-heavy walker, twin ion cannons hit ground and air", 1 },
    { "Specter Jet",    "J", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   1500, 560, 2400, 24, 11, 11, 23, 10, BR_AIRFIELD, B_C_TECH,  "Powerful supersonic fighter-bomber: heavy plasma lances, 10 strafing passes, tough, kills aircraft and ground targets alike", 0, 1 },

    { "Dozer",          "D", F_CLANKER, UK_VEH, UR_DOZER,     AR_LIGHT, 420, 82,  1000, 12, 6, 12, -1, 0, BR_HQ,       -1,         "Constructs structures" },
    { "Supply Truck",   "H", F_CLANKER, UK_VEH, UR_HARVESTER, AR_LIGHT, 450, 96,  700,  9,  6, 12, -1, 0, BR_SUPPLY,   -1,         "Hauls supplies to a Supply Depot" },
    { "Rifleman",       "R", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   120, 56,  200,  5,  6, 6,  9,  0, BR_BARRACKS, -1,         "Assault rifle, anti-infantry" },
    { "RPG Trooper",    "P", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   115, 54,  300,  7,  6, 6,  10, 0, BR_BARRACKS, -1,         "Rocket launcher, anti-armor and anti-air" },
    { "Gunner",         "G", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   220, 50,  380,  8,  6, 6,  11, 0, BR_BARRACKS, B_K_TECH,   "Heavy machine gun, sustained fire" },
    { "Brute Tank",     "T", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 900, 74,  900,  12, 7, 14, 12, 0, BR_FACTORY,  -1,         "Diesel main battle tank, 120mm cannon" },
    { "Gatling Tank",   "A", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 580, 80,  800,  11, 7, 12, 13, 0, BR_FACTORY,  -1,         "Gatling gun, anti-infantry and anti-air" },
    { "Rocket Launcher","M", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 450, 60,  1100, 16, 8, 13, 14, 0, BR_FACTORY,  B_K_TECH,   "Long range rocket artillery" },
    { "Vulture Gunship","W", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   560, 300, 850,  12, 9, 11, 15, 16, BR_AIRFIELD, -1,        "Cheap, fast, fragile bomber helicopter: drops sticks of heavy bombs on ground targets (16 a sortie), rockets for aircraft. Helicopters do not use pads: build as many as you like", 0, 0, 1, 0, 1, ST_NONE, 3.5f, DET_DRONE },
    { "Grenadier",      "B", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   170, 48,  700,  12, 7, 6,  21, 0, BR_BARRACKS, B_K_TECH,   "Elite lobber, splash grenades crack infantry and bunkers", 1 },
    { "Behemoth",       "N", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 2000, 52, 2300, 26, 8, 17, 22, 0, BR_FACTORY,  B_K_TECH,   "Super-heavy tank, twin 150mm shells with splash", 1 },
    { "Talon Jet",      "J", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   1550, 540, 2400, 24, 11, 12, 24, 10, BR_AIRFIELD, B_K_TECH,  "Powerful supersonic fighter-bomber: homing Sidewinders, 10 strafing passes, tough, kills aircraft and ground targets alike", 0, 1 },

    { "Medic Rig",      "Y", F_CYBER,   UK_VEH, UR_HEALER, AR_LIGHT, 520, 92, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Nano-repair field: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
    { "Field Medic",    "Y", F_CLANKER, UK_VEH, UR_HEALER, AR_LIGHT, 540, 88, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Repair crew: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
    { "Ghost Sniper",   "Q", F_CYBER,   UK_INF, UR_COMBAT, AR_INF,   85, 52, 550, 10, 11, 6, 27, 0, BR_BARRACKS, B_C_TECH, "Stealth marksman: needle rifle kills any infantry from 11 tiles but nothing else. Invisible to everything except an enemy spy drone", 0, 0, 0, 1, 0, ST_SPOTTER },
    { "Sniper",         "Q", F_CLANKER, UK_INF, UR_COMBAT, AR_INF,   90, 50, 550, 10, 11, 6, 28, 0, BR_BARRACKS, B_K_TECH, "Dug-in stealth marksman: rifle kills any infantry from 11 tiles but nothing else. Invisible to everything except an enemy spy drone", 0, 0, 0, 1, 0, ST_SPOTTER },
    { "Hornet Gunship", "H", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR,   640, 280, 950, 13, 9, 11, 31, 0, BR_AIRFIELD, -1, "Electric attack helicopter: twin lasers hit ground and air, hovers, never needs to rearm. Helicopters do not use pads: build as many as you like", 0, 0, 0, 0, 1, ST_NONE, 3.5f, DET_DRONE },

    { "Infiltrator",    "Z", F_CYBER,   UK_INF, UR_SPY,    AR_INF,   90,  66, 650, 8, 7, 6, -1, 0, BR_BARRACKS, B_C_TECH, "Unarmed spy: walks up to an enemy structure and captures it (right-click it). Survives the capture and can take another" },
    { "Spy",            "Z", F_CLANKER, UK_INF, UR_SPY,    AR_INF,   90,  64, 650, 8, 7, 6, -1, 0, BR_BARRACKS, B_K_TECH, "Unarmed spy: walks up to an enemy structure and captures it (right-click it). Survives the capture and can take another" },
    { "Ghost Drone",    "D", F_CYBER,   UK_AIR, UR_SCOUT,  AR_AIR,   220, 230, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_C_TECH, "Stealth spy drone, no weapons: sees the whole area around it and finds hidden snipers and underground bunkers. Only enemy drones can spot it. Hovers, needs no pad", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE },
    { "Recon Drone",    "D", F_CLANKER, UK_AIR, UR_SCOUT,  AR_AIR,   220, 220, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_K_TECH, "Stealth spy drone, no weapons: sees the whole area around it and finds hidden snipers and underground bunkers. Only enemy drones can spot it. Hovers, needs no pad", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE },
    { "Atlas Lifter",   "C", F_CYBER,   UK_AIR, UR_TRANSPORT, AR_AIR, 2400, 150, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_C_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, 0.0f, 0, 40 },
    { "Mammoth Lifter", "C", F_CLANKER, UK_AIR, UR_TRANSPORT, AR_AIR, 2600, 140, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_K_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, 0.0f, 0, 40 },
};

const BuildType BUILDS[B_COUNT] = {
    // name, key, faction, role, hp, cost, time, w, h, power, sight, weapon, requires, desc
    { "Command Core",    "C", F_CYBER, BR_HQ,       5000, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Fabricators. A Fabricator can raise another one if this falls (max 3)" },
    { "Fusion Reactor",  "P", F_CYBER, BR_POWER,    1400, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Hub",      "S", F_CYBER, BR_SUPPLY,   2400, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds E-Haulers" },
    { "Barracks",        "B", F_CYBER, BR_BARRACKS, 1800, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "Assembly Plant",  "F", F_CYBER, BR_FACTORY,  2800, 2000, 25, 4, 3, -3, 7,  -1, B_C_POWER, "Builds vehicles" },
    { "Drone Pad",       "A", F_CYBER, BR_AIRFIELD, 3400, 1000, 20, 5, 3, -2, 8,  -1, B_C_FACTORY,"Builds and rearms Wraith Drones" },
    { "Data Center",     "E", F_CYBER, BR_TECH,     2000, 2000, 30, 3, 3, -4, 8,  -1, B_C_FACTORY,"Unlocks Shock Trooper, Railgun Tank, strike powers and structure upgrades" },
    { "Laser Turret",    "L", F_CYBER, BR_TURRET,   5200, 1000, 14, 1, 1, -3, 8,  7,  B_C_POWER, "Fortified ground defense laser, needs power: short reach, very hard to kill" },
    { "Patriot Battery", "T", F_CYBER, BR_AATURRET, 6800, 1200, 16, 2, 2, -3, 9,  8,  B_C_POWER, "Fortified missile defense: brutal vs air, good vs ground, very hard to kill" },
    { "Bitcoin Datacenter","M", F_CYBER, BR_INCOME,  1600, 1600, 22, 3, 2, -5, 7, -1, B_C_POWER, "Mines $450 every 5s (max 4), half rate on low power" },
    { "Nuke Ramp",       "K", F_CYBER, BR_NUKE,     2600, 5000, 40, 3, 3, -6, 8, -1, B_C_TECH,  "A nuke every 5 min per ramp (key K): huge blast, mushroom cloud, fallout" },

    { "Command Post",    "C", F_CLANKER, BR_HQ,       5200, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Dozers. A Dozer can raise another one if this falls (max 3)" },
    { "Diesel Generator","P", F_CLANKER, BR_POWER,    1500, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Depot",    "S", F_CLANKER, BR_SUPPLY,   2600, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds Supply Trucks" },
    { "Barracks",        "B", F_CLANKER, BR_BARRACKS, 2000, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "War Factory",     "F", F_CLANKER, BR_FACTORY,  3000, 2000, 25, 4, 3, -3, 7,  -1, B_K_POWER, "Builds vehicles" },
    { "Airstrip",        "A", F_CLANKER, BR_AIRFIELD, 3600, 1000, 20, 5, 3, -2, 8,  -1, B_K_FACTORY,"Builds and rearms Vulture Gunships" },
    { "Arms Lab",        "E", F_CLANKER, BR_TECH,     2200, 2000, 30, 3, 3, -3, 8,  -1, B_K_FACTORY,"Unlocks Gunner, Rocket Launcher, strike powers and structure upgrades" },
    { "Gun Nest",        "N", F_CLANKER, BR_TURRET,   6400, 900,  14, 1, 1, -1, 8,  16, -1,        "Fortified machine gun bunker, ground and light air, very hard to kill" },
    { "Rocket Battery",  "T", F_CLANKER, BR_AATURRET, 5600, 1200, 16, 2, 2, -3, 9,  17, B_K_POWER, "Fortified rocket defense, ground and air, very hard to kill" },
    { "Oil Well",        "O", F_CLANKER, BR_INCOME,   1500, 1400, 20, 2, 2, 0,  6, -1, B_K_POWER, "Pumps $380 every 5s forever (max 4), no power needed" },
    { "Nuke Ramp",       "K", F_CLANKER, BR_NUKE,     2800, 5000, 40, 3, 3, -5, 8, -1, B_K_TECH,  "A nuke every 5 min per ramp (key K): huge blast, mushroom cloud, fallout" },

    { "Deep Bunker",     "U", F_CYBER,   BR_BUNKER, 30000, 2500, 30, 2, 2, 0, 8, -1, B_C_BARRACKS, "Stealth shelter deep underground: 50 infantry and a Fabricator. The garrison fires from a metal hatch. A nuke can wreck the hatch but never the bunker; only a spy drone reveals it, only soldiers and vehicles can destroy it" },
    { "Underground Bunker","U", F_CLANKER, BR_BUNKER, 30000, 2500, 30, 2, 2, 0, 8, -1, B_K_BARRACKS, "Stealth shelter deep underground: 50 infantry and a Dozer. The garrison fires from a metal hatch. A nuke can wreck the hatch but never the bunker; only a spy drone reveals it, only soldiers and vehicles can destroy it" },
};

const PowerType POWERS[F_COUNT] = {
    { "EMP Strike",  "Disables enemy vehicles and structures (tech included) for 8s and downs aircraft in the area", 180.0f, 4.5f },
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

const UpgradeType UPGRADES[F_COUNT][UPG_COUNT] = {
    { { "Rugged Nanoshell", "U", "Every structure gets 50% more health and shrugs off half of all damage. One nuke can no longer flatten it: it takes two close nukes or a massive assault at once", 3000, 40.0f },
      { "Defense Lasers",   "G", "Every structure gets a roof laser that shoots down aircraft and burns vehicles and infantry", 3500, 45.0f },
      { "Nanite Repair",    "E", "Every structure repairs itself over time (slower while under fire). Units still need Medic Rigs", 2000, 30.0f } },
    { { "Rugged Bulwark",   "U", "Every structure gets 50% more health and shrugs off half of all damage. One nuke can no longer flatten it: it takes two close nukes or a massive assault at once", 3000, 40.0f },
      { "Bunker Guns",      "G", "Every structure gets a roof machine gun that shoots down aircraft and shreds infantry and vehicles", 3500, 45.0f },
      { "Repair Crews",     "E", "Every structure repairs itself over time (slower while under fire). Units still need Field Medics", 2000, 30.0f } },
};

const DropType DROPS[F_COUNT] = {
    { "Airlift Drop", "A stealth cargo plane drops 15 troopers, 7 vehicles and 4 aircraft on parachutes anywhere you choose (2 min cooldown per Data Center)", 120.0f, 4.5f },
    { "Paradrop",     "A cargo plane drops 15 infantry, 7 vehicles and 4 aircraft on parachutes anywhere you choose (2 min cooldown per Arms Lab)",          120.0f, 4.5f },
};
const int DROP_INF_TYPES[F_COUNT][DROP_INF] = {
    { U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF2, U_C_INF2, U_C_INF2, U_C_INF2, U_C_INF2, U_C_INF3, U_C_INF3, U_C_INF3 },
    { U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF2, U_K_INF2, U_K_INF2, U_K_INF2, U_K_INF2, U_K_INF3, U_K_INF3, U_K_INF3 },
};
const int DROP_VEH_TYPES[F_COUNT][DROP_VEH] = {
    { U_C_TANK, U_C_TANK, U_C_TANK, U_C_VOLT, U_C_VOLT, U_C_RAIL, U_C_MEDIC },
    { U_K_TANK, U_K_TANK, U_K_TANK, U_K_GATLING, U_K_GATLING, U_K_MLRS, U_K_MEDIC },
};
const int DROP_AIR_TYPES[F_COUNT][DROP_AIR] = {
    { U_C_AIR, U_C_AIR, U_C_JET, U_C_JET },
    { U_K_AIR, U_K_AIR, U_K_JET, U_K_JET },
};

int firstUnitOf(Faction f) { return f == F_CYBER ? U_C_DOZER : U_K_DOZER; }
int firstBuildOf(Faction f) { return f == F_CYBER ? B_C_HQ : B_K_HQ; }
