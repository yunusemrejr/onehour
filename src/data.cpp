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
    /* 32 */{ "Bunker Minigun",  16, 6.8f, 0, 0.06f, PJ_BULLET, 0,    true, true,  {1.5f, 1.05f, 0.75f, 0.00f, 1.65f}, 1, 1200, SND_MG,          rgb(255, 226, 130) },
    /* 33 */{ "Jeep Gun",        12, 5.5f, 0, 0.20f, PJ_BULLET, 0,    true, true,  {1.3f, 0.45f, 0.22f, 0.15f, 0.45f}, 1, 950,  SND_MG,         rgb(255, 232, 150) },
    /* 34 */{ "Pulse Cannons",   78, 8.5f, 0, 1.30f, PJ_LASER,  0,    true, true,  {0.9f, 1.10f, 1.05f, 0.80f, 0.75f}, 2, 0,    SND_LASER_HEAVY, rgb(110, 232, 255) },
    /* 35 */{ "Naval Guns",      92, 9.0f, 0, 1.90f, PJ_SHELL,  0.7f, true, true,  {1.0f, 1.10f, 1.05f, 0.85f, 0.55f}, 2, 560,  SND_CANNON,      rgb(255, 206, 120) },
    /* 36 */{ "Cruise Missiles",125, 14.0f,3.0f, 5.5f, PJ_ROCKET, 1.25f,true, false, {1.0f, 1.10f, 1.05f, 1.35f, 0.0f}, 3, 540,  SND_ROCKET,      rgb(170, 244, 255) },
    /* 37 */{ "Rocket Barrage",  96, 13.5f,3.0f, 4.8f, PJ_ROCKET, 1.0f, true, false, {1.0f, 1.10f, 1.05f, 1.35f, 0.0f}, 6, 500,  SND_ROCKET,      rgb(255, 196, 110) },
    /* 38 */{ "Strike Rockets", 130, 11.0f,0, 0.00f, PJ_ROCKET, 1.5f, true, false, {1.1f, 1.20f, 1.10f, 1.40f, 0.0f}, 1, 1150, SND_ROCKET,      rgb(150, 240, 255) },
    /* 39 */{ "Strike Rockets", 130, 11.0f,0, 0.00f, PJ_ROCKET, 1.5f, true, false, {1.1f, 1.20f, 1.10f, 1.40f, 0.0f}, 1, 1150, SND_ROCKET,      rgb(255, 190, 100) },
    /* 40 */{ "Bunker Rocket Pod", 98,10.5f,0, 2.40f, PJ_ROCKET, 0.5f, true, true,  {0.85f, 1.20f, 1.18f, 0.85f, 1.35f}, 2, 700, SND_ROCKET,      rgb(255, 210, 140) },
    /* 41 */{ "Bunker Overdrive Laser", 96, 9.5f, 0, 0.28f, PJ_LASER, 0, true, true, {1.6f, 1.35f, 1.35f, 0.85f, 1.95f}, 2, 0,   SND_LASER_HEAVY, rgb(100, 245, 255) },
    /* 42 */{ "Bunker Heavy Quad-MG",   30, 9.5f, 0, 0.05f, PJ_BULLET,0, true, true, {1.8f, 1.30f, 1.15f, 0.75f, 1.90f}, 2, 1300,SND_MG,          rgb(255, 232, 120) },
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
    { "Ghost Drone",    "D", F_CYBER,   UK_AIR, UR_SCOUT,  AR_AIR,   220, 230, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_C_TECH, "Autonomous stealth spy drone: sweeps the map on its own in coordinated swarms to hunt hidden snipers and underground bunkers, or flies where you call it. Only enemy drones can spot it", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE },
    { "Recon Drone",    "D", F_CLANKER, UK_AIR, UR_SCOUT,  AR_AIR,   220, 220, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_K_TECH, "Autonomous stealth spy drone: sweeps the map on its own in coordinated swarms to hunt hidden snipers and underground bunkers, or flies where you call it. Only enemy drones can spot it", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE },
    { "Atlas Lifter",   "C", F_CYBER,   UK_AIR, UR_TRANSPORT, AR_AIR, 2400, 150, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_C_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, 0.0f, 0, 40 },
    { "Mammoth Lifter", "C", F_CLANKER, UK_AIR, UR_TRANSPORT, AR_AIR, 2600, 140, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_K_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, 0.0f, 0, 40 },

    // jeeps: the same in both armies, only the look differs. Fields after cargoCap: naval, evade, temp, drones
    { "Jeep",           "J", F_CYBER,   UK_VEH, UR_COMBAT, AR_LIGHT, 420, 126, 650, 9, 7, 12, W_JEEP_MG, 0, BR_FACTORY, -1, "Fast open-top jeep with a pintle gun and seven seats: infantry ride inside and shoot from it with their own weapons (rifles, rockets, grenades, sniper shots). A mini guard drone shoots rockets and shells down. Load it by right-clicking it with soldiers selected", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, JEEP_SEATS, 0, 0.0f, 0, JEEP_DRONES },
    { "Jeep",           "J", F_CLANKER, UK_VEH, UR_COMBAT, AR_LIGHT, 420, 126, 650, 9, 7, 12, W_JEEP_MG, 0, BR_FACTORY, -1, "Fast open-top jeep with a pintle gun and seven seats: infantry ride inside and shoot from it with their own weapons (rifles, rockets, grenades, sniper shots). A mini guard drone shoots rockets and shells down. Load it by right-clicking it with soldiers selected", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, JEEP_SEATS, 0, 0.0f, 0, JEEP_DRONES },
    // ships of the lake (a Naval Yard builds them); the Guard Drones upgrade gives each a screen of protector drones
    { "Pulse Destroyer","D", F_CYBER,   UK_VEH, UR_COMBAT, AR_HEAVY, 1500, 72, 1800, 18, 9, 21, W_DESTROYER_C, 0, BR_NAVAL, -1, "Warship of the lake: twin pulse cannons burn ships, shore targets and aircraft out to 8.5 tiles. With the Guard Drones upgrade a screen of drones shoots down rockets and shells aimed at it. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Iron Destroyer", "D", F_CLANKER, UK_VEH, UR_COMBAT, AR_HEAVY, 1600, 68, 1800, 18, 9, 21, W_DESTROYER_K, 0, BR_NAVAL, -1, "Warship of the lake: twin naval guns shell ships, shore targets and aircraft out to 9 tiles. With the Guard Drones upgrade a screen of drones shoots down rockets and shells aimed at it. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Lancer Cruiser", "C", F_CYBER,   UK_VEH, UR_COMBAT, AR_HEAVY, 1200, 56, 2400, 24, 10, 22, W_CRUISER_C, 0, BR_NAVAL, B_C_TECH, "Missile ship: salvos of guided cruise missiles hit ships, vehicles and structures up to 14 tiles away (nothing closer than 3). Weak against aircraft. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Rocket Cruiser", "C", F_CLANKER, UK_VEH, UR_COMBAT, AR_HEAVY, 1300, 54, 2400, 24, 10, 22, W_CRUISER_K, 0, BR_NAVAL, B_K_TECH, "Missile ship: barrages of rockets hit ships, vehicles and structures up to 13.5 tiles away (nothing closer than 3). Weak against aircraft. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    // the two fighters of a Hotline Strike: never built (no structure makes them), lent to the army for one run
    { "Valkyrie",       "V", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR, 4200, 900, 4000, 0, 12, 15, W_STRIKE_C, 36, BR_NONE, -1, "Special strike fighter on loan from a Hotline Strike: fastest thing in the sky, three rocket passes over the called spot, armoured and nearly impossible to shoot down. It leaves when its run is over", 0, 1, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 0, 0.62f, 1, 0 },
    { "Warhawk",        "V", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR, 4400, 880, 4000, 0, 12, 15, W_STRIKE_K, 36, BR_NONE, -1, "Special strike fighter on loan from a Hotline Strike: fastest thing in the sky, three rocket passes over the called spot, armoured and nearly impossible to shoot down. It leaves when its run is over", 0, 1, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 0, 0.58f, 1, 0 },
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

    { "Deep Bunker",     "U", F_CYBER,   BR_BUNKER, 32000, 7500, 30, 2, 2, 0, 8, -1, B_C_BARRACKS, "Fortified stealth shelter deep underground: 50 infantry and a Fabricator. Earns $85 every 5s, mends itself, twin hatch miniguns, rocket pod and a salvo of two mini-nukes (every 65s). When surrounded by enemies, overdrive lasers and heavy guns shred aircraft, incoming nukes, tanks and infantry, and heavy hits vent a ring of radiation" },
    { "Underground Bunker","U", F_CLANKER, BR_BUNKER, 32000, 7500, 30, 2, 2, 0, 8, -1, B_K_BARRACKS, "Fortified stealth shelter deep underground: 50 infantry and a Dozer. Earns $85 every 5s, mends itself, twin hatch miniguns, rocket pod and a salvo of two mini-nukes (every 65s). When surrounded by enemies, overdrive lasers and heavy machineguns shred aircraft, incoming nukes, tanks and infantry, and heavy hits vent a ring of radiation" },

    { "Nano Drydock",   "Y", F_CYBER,   BR_NAVAL,    3000, 2200, 28, 3, 3, -3, 8, -1, B_C_FACTORY, "Naval Yard: must touch the lake. Builds the Pulse Destroyer and, with a Data Center, the Lancer Cruiser" },
    { "Shipyard",       "Y", F_CLANKER, BR_NAVAL,    3200, 2200, 28, 3, 3, -3, 8, -1, B_K_FACTORY, "Naval Yard: must touch the lake. Builds the Iron Destroyer and, with an Arms Lab, the Rocket Cruiser" },
    { "Anti-Nuke Laser","Q", F_CYBER,   BR_ANTINUKE, 1900, 1900, 20, 2, 2, -5, 9, -1, B_C_TECH, "Small tower that spots a nuclear warhead closing in on its 17 tile area (or whose blast would reach into it) and burns it from the sky, early and with no reload gap between warheads. Every one has its own chance; towers within 9 tiles of each other link up for more beams, faster tracking and a better chance each. A warhead shot down still drops pieces and radiation: 30% of the damage. Needs power (max 8)" },
    { "Anti-Nuke Missiles","Q", F_CLANKER, BR_ANTINUKE, 2100, 1800, 20, 2, 2, -2, 9, -1, B_K_TECH, "Small silo that spots a nuclear warhead closing in on its 19 tile area and launches interceptors at it. Every one has its own chance, so the more you build, the likelier a warhead is stopped. A warhead shot down still drops pieces and radiation: 30% of the damage (max 8)" },
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
      { "Nanite Repair",    "E", "Every structure repairs itself over time (slower while under fire). Units still need Medic Rigs", 2000, 30.0f },
      { "Aegis Guard Drones","N", "Every ship flies with a screen of three protector drones that shoot down rockets and shells aimed at it (lasers and bullets are too quick), and every jeep gets a second drone", 2500, 40.0f } },
    { { "Rugged Bulwark",   "U", "Every structure gets 50% more health and shrugs off half of all damage. One nuke can no longer flatten it: it takes two close nukes or a massive assault at once", 3000, 40.0f },
      { "Bunker Guns",      "G", "Every structure gets a roof machine gun that shoots down aircraft and shreds infantry and vehicles", 3500, 45.0f },
      { "Repair Crews",     "E", "Every structure repairs itself over time (slower while under fire). Units still need Field Medics", 2000, 30.0f },
      { "Flak Guard Drones","N", "Every ship flies with a screen of three protector drones that shoot down rockets and shells aimed at it (lasers and bullets are too quick), and every jeep gets a second drone", 2500, 40.0f } },
};

const DropType DROPS[F_COUNT] = {
    { "Airlift Drop", "A stealth cargo plane drops the force your Data Center can field right now on parachutes, anywhere you choose: soldiers and vehicles on the silk, aircraft take up guard over the zone. Three to ten jeeps, each with seven soldiers already inside, come with it. The load grows as you learn (2 min cooldown per Data Center)", 120.0f, 4.5f },
    { "Paradrop",     "A cargo plane drops the force your Arms Lab can field right now on parachutes, anywhere you choose: soldiers and vehicles on the silk, aircraft take up guard over the zone. Three to ten jeeps, each with seven soldiers already inside, come with it. The load grows as you learn (2 min cooldown per Arms Lab)",          120.0f, 4.5f },
};
// Slots: entry 0 is the cheapest thing the army can put in that slot, the next entry the next step up, and so on. An army that has not
// learned a step yet simply stops before it (Sim::dropManifest), so the pool may hold units it cannot field today.
const int DROP_INF_POOL[F_COUNT][DROP_INF] = {
    { U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF1, U_C_INF2, U_C_INF2, U_C_INF2, U_C_INF2, U_C_INF3, U_C_INF3, U_C_ELITE, U_C_ELITE, U_C_ELITE, U_C_INF3, U_C_INF3 },
    { U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF1, U_K_INF2, U_K_INF2, U_K_INF2, U_K_INF2, U_K_INF3, U_K_INF3, U_K_ELITE, U_K_ELITE, U_K_ELITE, U_K_INF3, U_K_INF3 },
};
const int DROP_VEH_POOL[F_COUNT][DROP_VEH] = {
    { U_C_TANK, U_C_TANK, U_C_TANK, U_C_VOLT, U_C_VOLT, U_C_RAIL, U_C_TITAN },
    { U_K_TANK, U_K_TANK, U_K_TANK, U_K_GATLING, U_K_GATLING, U_K_MLRS, U_K_TITAN },
};
const int DROP_AIR_POOL[F_COUNT][DROP_AIR] = {
    { U_C_AIR, U_C_AIR, U_C_HELI, U_C_JET },
    { U_K_AIR, U_K_AIR, U_K_AIR, U_K_JET },
};

const DropTierType DROP_TIERS[DTI_COUNT] = {
    { "Reinforced",   "what a fresh tech structure sends: the army's own roster, the tech-gated snipers included",                        15.0f, 7.0f, 4.0f, 120.0f },
    { "Battle Group", "something has opened up (the Advanced Program or an upgrade finishes): a medic joins and the armour gets heavier", 18.0f, 9.0f, 5.0f, 110.0f },
    { "Spearhead",    "the arms race is done: elite infantry, the best armour, gunships and jets, and the most of them",                  22.0f, 11.0f, 6.0f, 100.0f },
};

const StrikeType STRIKES[F_COUNT] = {
    { "Hotline Strike", "One call to Cyber Command: a swarm of Hornet gunships, Wraith drones and Specter jets crosses the map to rake the spot you pick, and two special Valkyrie strike fighters (not buildable, nearly impossible to shoot down) make three rocket passes over it. Needs a Data Center; one call per cooldown per Data Center" },
    { "Hotline Strike", "One call to the battery: a swarm of Vulture gunships, Talon jets and a recon drone crosses the map to rake the spot you pick, and two special Warhawk strike fighters (not buildable, nearly impossible to shoot down) make three rocket passes over it. Needs an Arms Lab; one call per cooldown per Arms Lab" },
};

int firstUnitOf(Faction f) { return f == F_CYBER ? U_C_DOZER : U_K_DOZER; }
int firstBuildOf(Faction f) { return f == F_CYBER ? B_C_HQ : B_K_HQ; }
