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
    /* 43 */{ "Tail Cannon",     26, 6.0f, 0, 0.32f, PJ_BULLET, 0,    true, true,  {1.0f, 0.70f, 0.40f, 0.25f, 1.9f}, 1, 1150, SND_MG,          rgb(255, 226, 130) },
    /* 44 */{ "Nuclear Bomb",     0, 1.0f, 0, 0.00f, PJ_BOMB,   0,    true, false, {1.0f, 1.0f, 1.0f, 1.0f, 0.0f}, 1, 0,    SND_EXPLODE_L,   rgb(238, 240, 246) },
    /* 45 */{ "Arrow Missiles",  68, 7.0f, 0, 1.10f, PJ_ROCKET, 0.3f, true, true,  {0.7f, 1.00f, 0.85f, 0.70f, 2.1f}, 2, 950,  SND_ROCKET,      rgb(150, 236, 255) },
    /* 46 */{ "Saber Missiles",  72, 7.0f, 0, 1.15f, PJ_ROCKET, 0.3f, true, true,  {0.7f, 1.00f, 0.85f, 0.70f, 2.0f}, 2, 920,  SND_ROCKET,      rgb(255, 206, 130) },
    /* 47 */{ "Skiff Cannon & Micro-Missiles", 28, 6.5f, 0, 0.90f, PJ_ROCKET, 0.25f, true, true, {1.15f, 1.00f, 0.80f, 0.70f, 1.40f}, 2, 760, SND_ROCKET, rgb(150, 236, 255) },
    /* 48 */{ "Gunboat Cannon & Rockets",      30, 6.5f, 0, 0.95f, PJ_ROCKET, 0.25f, true, true, {1.15f, 1.00f, 0.80f, 0.70f, 1.35f}, 2, 720, SND_ROCKET, rgb(255, 206, 130) },
    /* 49 */{ "Hunting Rifle",    10, 5.0f, 0, 0.75f, PJ_BULLET, 0,    true, false, {1.0f, 0.30f, 0.15f, 0.35f, 0.0f}, 1, 850,  SND_RIFLE,       rgb(255, 222, 150) },
    /* 50 */{ "Petrol Bomb",      34, 4.2f, 0, 2.30f, PJ_SHELL,  0.7f, true, false, {0.9f, 0.70f, 0.35f, 1.60f, 0.0f}, 1, 300,  SND_EXPLODE_S,   rgb(255, 150, 50) },
    /* 51 */{ "Close-In Guns",    15, 7.0f, 0, 0.10f, PJ_BULLET, 0,    true, true,  {1.0f, 0.55f, 0.25f, 0.20f, 1.9f}, 1, 1300, SND_MG,          rgb(255, 228, 150) },
    /* 52 */{ "Strike Bombs",    125, 1.0f, 0, 0.00f, PJ_BOMB,   1.7f, true, false, {1.25f, 1.20f, 1.00f, 1.40f, 0.0f}, 1, 0,    SND_EXPLODE_L,   rgb(255, 196, 120) },
    /* 53 */{ "Bunker Sky Laser",  140, 12.0f,0, 0.55f, PJ_LASER,  0,    true, true,  {1.2f, 1.10f, 0.95f, 0.50f, 2.6f}, 1, 0,    SND_LASER_HEAVY, rgb(130, 250, 255) },
    /* 54 */{ "Toxic Lance",     480, 7.0f, 0, 1.50f, PJ_LASER,  0.9f, true, true,  {0.4f, 1.00f, 1.00f, 0.60f, 0.9f}, 1, 0,    SND_LASER_HEAVY, rgb(150, 255, 80) },   // ten Laser Rifles in one (W 1), a splash and a toxic cloud
};
const int WEAPON_COUNT = sizeof(WEAPONS) / sizeof(WEAPONS[0]);

const UnitType UNITS[U_COUNT] = {
    // name, key, faction, kind, role, armor, hp, speed, cost, time, sight, radius, weapon, ammo, builtBy, requires, desc, program
    { "Fabricator",     "D", F_CYBER, UK_VEH, UR_DOZER,     AR_LIGHT, 400, 88,  1000, 12, 6, 12, -1, 0, BR_HQ,       -1,         "Constructs structures" },
    { "E-Hauler",       "H", F_CYBER, UK_VEH, UR_HARVESTER, AR_LIGHT, 420, 100, 700,  9,  6, 12, -1, 0, BR_SUPPLY,   -1,         "Hauls supplies to a Supply Hub" },
    { "Trooper",        "R", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   115, 58,  210,  5,  6, 6,  0,  0, BR_BARRACKS, -1,         "Pulse rifle, anti-infantry" },
    { "Laser Trooper",  "L", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   100, 55,  325,  7,  6, 6,  1,  0, BR_BARRACKS, -1,         "Laser rifle, anti-armor and anti-air" },
    { "Shock Trooper",  "S", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   140, 52,  400,  8,  6, 6,  2,  0, BR_BARRACKS, B_C_TECH,   "Arc caster, shreds infantry groups" },
    { "Photon Tank",    "T", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 720, 84,  1100,  15, 7, 13, 3,  0, BR_FACTORY,  -1,         "Fast electric tank with photon cannon" },
    { "Volt Walker",    "V", F_CYBER, UK_VEH, UR_COMBAT,    AR_LIGHT, 560, 76,  950,  13, 7, 12, 4,  0, BR_FACTORY,  -1,         "Volt coil, arcs across infantry and drones" },
    { "Railgun Tank",   "G", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 700, 62,  1500, 20, 8, 14, 5,  0, BR_FACTORY,  B_C_TECH,   "Long range railgun, punches heavy armor" },
    { "Wraith Drone",   "W", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   520, 340, 850,  12, 9, 10, 6,  12, BR_AIRFIELD, -1,        "Cheap, fast, fragile bomber drone: carpet-bombs ground targets with plasma bombs (12 a sortie), laser kills aircraft", 0, 0, 1, 0, 0, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL },
    { "Ion Lancer",     "I", F_CYBER, UK_INF, UR_COMBAT,    AR_INF,   150, 50,  750,  12, 8, 6,  19, 0, BR_BARRACKS, B_C_TECH,   "Elite lancer, piercing ion lance out to 9 tiles", 1 },
    { "Aegis Titan",    "N", F_CYBER, UK_VEH, UR_COMBAT,    AR_HEAVY, 1700, 58, 3000, 34, 8, 17, 20, 0, BR_FACTORY,  B_C_TECH,   "Super-heavy walker, twin ion cannons hit ground and air", 1 },
    { "Specter Jet",    "J", F_CYBER, UK_AIR, UR_COMBAT,    AR_AIR,   1500, 560, 2800, 28, 11, 11, 23, 10, BR_AIRFIELD, B_C_TECH,  "Powerful supersonic fighter-bomber: heavy plasma lances, 10 strafing passes, tough, kills aircraft and ground targets alike", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL },

    { "Dozer",          "D", F_CLANKER, UK_VEH, UR_DOZER,     AR_LIGHT, 420, 82,  1000, 12, 6, 12, -1, 0, BR_HQ,       -1,         "Constructs structures" },
    { "Supply Truck",   "H", F_CLANKER, UK_VEH, UR_HARVESTER, AR_LIGHT, 450, 96,  700,  9,  6, 12, -1, 0, BR_SUPPLY,   -1,         "Hauls supplies to a Supply Depot" },
    { "Rifleman",       "R", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   120, 56,  200,  5,  6, 6,  9,  0, BR_BARRACKS, -1,         "Assault rifle, anti-infantry" },
    { "RPG Trooper",    "P", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   115, 54,  300,  7,  6, 6,  10, 0, BR_BARRACKS, -1,         "Rocket launcher, anti-armor and anti-air" },
    { "Gunner",         "G", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   220, 50,  380,  8,  6, 6,  11, 0, BR_BARRACKS, B_K_TECH,   "Heavy machine gun, sustained fire" },
    { "Brute Tank",     "T", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 900, 74,  1100,  15, 7, 14, 12, 0, BR_FACTORY,  -1,         "Diesel main battle tank, 120mm cannon" },
    { "Gatling Tank",   "A", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 580, 80,  950,  13, 7, 12, 13, 0, BR_FACTORY,  -1,         "Gatling gun, anti-infantry and anti-air" },
    { "Rocket Launcher","M", F_CLANKER, UK_VEH, UR_COMBAT,    AR_LIGHT, 450, 60,  1300, 19, 8, 13, 14, 0, BR_FACTORY,  B_K_TECH,   "Long range rocket artillery" },
    { "Vulture Gunship","W", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   560, 300, 850,  12, 9, 11, 15, 16, BR_AIRFIELD, -1,        "Cheap, fast, fragile bomber helicopter: drops sticks of heavy bombs on ground targets (16 a sortie), rockets for aircraft. Helicopters do not use pads: build as many as you like", 0, 0, 1, 0, 1, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL },
    { "Grenadier",      "B", F_CLANKER, UK_INF, UR_COMBAT,    AR_INF,   170, 48,  700,  12, 7, 6,  21, 0, BR_BARRACKS, B_K_TECH,   "Elite lobber, splash grenades crack infantry and bunkers", 1 },
    { "Behemoth",       "N", F_CLANKER, UK_VEH, UR_COMBAT,    AR_HEAVY, 2000, 52, 3000, 34, 8, 17, 22, 0, BR_FACTORY,  B_K_TECH,   "Super-heavy tank, twin 150mm shells with splash", 1 },
    { "Talon Jet",      "J", F_CLANKER, UK_AIR, UR_COMBAT,    AR_AIR,   1550, 540, 2800, 28, 11, 12, 24, 10, BR_AIRFIELD, B_K_TECH,  "Powerful supersonic fighter-bomber: homing Sidewinders, 10 strafing passes, tough, kills aircraft and ground targets alike", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL },

    { "Medic Rig",      "Y", F_CYBER,   UK_VEH, UR_HEALER, AR_LIGHT, 520, 92, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Nano-repair field: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
    { "Field Medic",    "Y", F_CLANKER, UK_VEH, UR_HEALER, AR_LIGHT, 540, 88, 900, 12, 7, 13, -1, 0, BR_FACTORY, -1, "Repair crew: heals every friendly soldier, vehicle and aircraft near it, structures slowly" },
    { "Ghost Sniper",   "Q", F_CYBER,   UK_INF, UR_COMBAT, AR_INF,   85, 52, 550, 10, 11, 6, 27, 0, BR_BARRACKS, B_C_TECH, "Stealth marksman: needle rifle kills any infantry from 11 tiles but nothing else. Invisible to everything except an enemy spy drone. Its scope finds infiltrators out to 7 tiles", 0, 0, 0, 1, 0, ST_SPOTTER, 7.0f, DET_INFIL },
    { "Sniper",         "Q", F_CLANKER, UK_INF, UR_COMBAT, AR_INF,   90, 50, 550, 10, 11, 6, 28, 0, BR_BARRACKS, B_K_TECH, "Dug-in stealth marksman: rifle kills any infantry from 11 tiles but nothing else. Invisible to everything except an enemy spy drone. Its scope finds infiltrators out to 7 tiles", 0, 0, 0, 1, 0, ST_SPOTTER, 7.0f, DET_INFIL },
    { "Hornet Gunship", "H", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR,   640, 280, 950, 13, 9, 11, 31, 0, BR_AIRFIELD, -1, "Electric attack helicopter: twin lasers hit ground and air, hovers, never needs to rearm. Helicopters do not use pads: build as many as you like", 0, 0, 0, 0, 1, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL },

    { "Infiltrator",    "Z", F_CYBER,   UK_INF, UR_SPY,    AR_INF,   90,  66, 650, 8, 7, 6, -1, 0, BR_BARRACKS, B_C_TECH, "Stealthy unarmed infiltrator: walks up to an enemy (or neutral island / fortress) structure and captures it (right-click it), or takes a whole town round the lake at once; yours can take over an ally's structures too. Survives the capture and can take another. Only snipers, drones, aircraft other than heavy bombers, jeeps, other infiltrators and Laser Turrets / Gun Nests can see it", 0, 0, 0, 0, 0, ST_INFIL, INFIL_DETECT, DET_INFIL },
    { "Spy",            "Z", F_CLANKER, UK_INF, UR_SPY,    AR_INF,   90,  64, 650, 8, 7, 6, -1, 0, BR_BARRACKS, B_K_TECH, "Stealthy unarmed infiltrator: walks up to an enemy (or neutral island / fortress) structure and captures it (right-click it), or takes a whole town round the lake at once; yours can take over an ally's structures too. Survives the capture and can take another. Only snipers, drones, aircraft other than heavy bombers, jeeps, other infiltrators and Laser Turrets / Gun Nests can see it", 0, 0, 0, 0, 0, ST_INFIL, INFIL_DETECT, DET_INFIL },
    { "Ghost Drone",    "D", F_CYBER,   UK_AIR, UR_SCOUT,  AR_AIR,   220, 230, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_C_TECH, "Autonomous stealth spy drone: sweeps the map on its own in coordinated swarms to hunt hidden snipers and enemy spy drones, or flies where you call it. Only enemy drones can spot it", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE | DET_INFIL },
    { "Recon Drone",    "D", F_CLANKER, UK_AIR, UR_SCOUT,  AR_AIR,   220, 220, 900, 12, 15, 9, -1, 0, BR_AIRFIELD, B_K_TECH, "Autonomous stealth spy drone: sweeps the map on its own in coordinated swarms to hunt hidden snipers and enemy spy drones, or flies where you call it. Only enemy drones can spot it", 0, 0, 0, 0, 1, ST_DRONE, 9.0f, DET_SPOTTER | DET_DRONE | DET_INFIL },
    { "Atlas Lifter",   "C", F_CYBER,   UK_AIR, UR_TRANSPORT, AR_AIR, 2400, 150, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_C_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, INFIL_DETECT, DET_INFIL, 40 },
    { "Mammoth Lifter", "C", F_CLANKER, UK_AIR, UR_TRANSPORT, AR_AIR, 2600, 140, 2600, 22, 10, 26, -1, 0, BR_AIRFIELD, B_K_TECH, "Huge cargo helicopter: carries 40 infantry, or tanks and other vehicles (a tank takes 6 places, a super-heavy 10). Unarmed but tough. Load by right-clicking it, unload where it hovers", 0, 0, 0, 0, 1, ST_NONE, INFIL_DETECT, DET_INFIL, 40 },

    // jeeps: the same in both armies, only the look differs. Fields after cargoCap: naval, evade, temp, drones
    { "Jeep",           "J", F_CYBER,   UK_VEH, UR_COMBAT, AR_LIGHT, 420, 126, 650, 9, 7, 12, W_JEEP_MG, 0, BR_FACTORY, -1, "Fast open-top jeep with a pintle gun and seven seats: infantry ride inside and shoot from it with their own weapons (rifles, rockets, grenades, sniper shots). A mini guard drone shoots rockets and shells down. Load it by right-clicking it with soldiers selected", 0, 0, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL, JEEP_SEATS, 0, 0.0f, 0, JEEP_DRONES },
    { "Jeep",           "J", F_CLANKER, UK_VEH, UR_COMBAT, AR_LIGHT, 420, 126, 650, 9, 7, 12, W_JEEP_MG, 0, BR_FACTORY, -1, "Fast open-top jeep with a pintle gun and seven seats: infantry ride inside and shoot from it with their own weapons (rifles, rockets, grenades, sniper shots). A mini guard drone shoots rockets and shells down. Load it by right-clicking it with soldiers selected", 0, 0, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL, JEEP_SEATS, 0, 0.0f, 0, JEEP_DRONES },
    // ships of the lake (a Naval Yard builds them); the Guard Drones upgrade gives each a screen of protector drones
    { "Pulse Destroyer","D", F_CYBER,   UK_VEH, UR_COMBAT, AR_HEAVY, 1500, 96, 1800, 18, 9, 21, W_DESTROYER_C, 0, BR_NAVAL, -1, "Warship: twin pulse cannons burn ships, shore targets and aircraft out to 8.5 tiles. With the Guard Drones upgrade a screen of drones shoots down rockets and shells aimed at it. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Iron Destroyer", "D", F_CLANKER, UK_VEH, UR_COMBAT, AR_HEAVY, 1600, 92, 1800, 18, 9, 21, W_DESTROYER_K, 0, BR_NAVAL, -1, "Warship: twin naval guns shell ships, shore targets and aircraft out to 9 tiles. With the Guard Drones upgrade a screen of drones shoots down rockets and shells aimed at it. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Lancer Cruiser", "C", F_CYBER,   UK_VEH, UR_COMBAT, AR_HEAVY, 1200, 78, 2400, 24, 10, 22, W_CRUISER_C, 0, BR_NAVAL, B_C_TECH, "Missile ship: salvos of guided cruise missiles hit ships, vehicles and structures up to 14 tiles away (nothing closer than 3). Weak against aircraft. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Rocket Cruiser", "C", F_CLANKER, UK_VEH, UR_COMBAT, AR_HEAVY, 1300, 74, 2400, 24, 10, 22, W_CRUISER_K, 0, BR_NAVAL, B_K_TECH, "Missile ship: barrages of rockets hit ships, vehicles and structures up to 13.5 tiles away (nothing closer than 3). Weak against aircraft. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    // the fighters of an Air Strike: never built (no structure makes them), they fly one bombing pass over the called spot and leave the map
    { "Valkyrie",       "V", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR, 2200, 760, 3000, 0, 12, 15, W_STRIKE_C, STRIKE_ROCKETS, BR_NONE, -1, "Strike fighter of an Air Strike: comes in from the sea in a V formation, fires two rockets and lays a stick of six bombs across the called spot, then breaks away and leaves over the ocean. Hard to hit. Nobody commands it", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL, 0, 0, 0.45f, 1, 0 },
    { "Warhawk",        "V", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR, 2300, 740, 3000, 0, 12, 15, W_STRIKE_K, STRIKE_ROCKETS, BR_NONE, -1, "Strike fighter of an Air Strike: comes in from the sea in a V formation, fires two rockets and lays a stick of six bombs across the called spot, then breaks away and leaves over the ocean. Hard to hit. Nobody commands it", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_INFIL, 0, 0, 0.42f, 1, 0 },
    // civilian dozers: the price of a dozer, built at the Command structure, they raise only the civilian town (and mend anything, like any dozer)
    { "Civic Fabricator","V", F_CYBER,   UK_VEH, UR_DOZER, AR_LIGHT, 400, 88, 1000, 12, 6, 12, -1, 0, BR_HQ, -1, "Civilian builder: raises houses, apartments, skyscrapers, gas stations, grocery stores, event halls, town squares, the town hall and government offices. Each pays on its own and sends out free dozers. Cannot build military structures" },
    { "Civil Dozer",    "V", F_CLANKER, UK_VEH, UR_DOZER, AR_LIGHT, 420, 82, 1000, 12, 6, 12, -1, 0, BR_HQ, -1, "Civilian builder: raises houses, apartments, skyscrapers, gas stations, grocery stores, event halls, town squares, the town hall and government offices. Each pays on its own and sends out free dozers. Cannot build military structures" },
    // Supersonic Bomber (both armies, built at the airfield with a tech structure): a small, very fast nuclear bomber that carries one compact
    // nuclear bomb. Fields: str(uts). After drones: nukeBomb=1, sprintSpeed=SBOMBER_DASH.
    { "Supersonic Bomber","B", F_CYBER, UK_AIR, UR_COMBAT,   AR_AIR,   1000, SBOMBER_SLOW, 10000, 55, 11, 12, W_SBOMBER_GUN, 1, BR_AIRFIELD, B_C_TECH, "Supersonic nuclear bomber: the fastest aircraft in the sky (960 px/s) while its one compact nuclear bomb is aboard. The bomb hits with an Underground Nuke's damage (a silver warhead in a small casing, a much smaller fireball) and flattens the inner blast even through heavy structures. Once it is gone the plane slows to 340 px/s and is easy prey; it rearms one bomb on its pad", 0, 1, 1, 0, 0, ST_NONE, 3.5f, DET_DRONE, 0, 0, 0.0f, 0, 0, 1, SBOMBER_DASH },
    { "Supersonic Bomber","B", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR,   1000, SBOMBER_SLOW, 10000, 55, 11, 12, W_SBOMBER_GUN, 1, BR_AIRFIELD, B_K_TECH, "Supersonic nuclear bomber: the fastest aircraft in the sky (960 px/s) while its one compact nuclear bomb is aboard. The bomb hits with an Underground Nuke's damage (a silver warhead in a small casing, a much smaller fireball) and flattens the inner blast even through heavy structures. Once it is gone the plane slows to 340 px/s and is easy prey; it rearms one bomb on its pad", 0, 1, 1, 0, 0, ST_NONE, 3.5f, DET_DRONE, 0, 0, 0.0f, 0, 0, 1, SBOMBER_DASH },
    // Anti-Radiation Truck (both armies, built at the Assembly Plant / War Factory): an unarmed support vehicle whose decontamination field
    // scrubs fallout (see RADTRUCK_*), shields the units and structures inside it and takes the absorbed radiation as damage.
    { "Anti-Radiation Truck","R", F_CYBER, UK_VEH, UR_DECON, AR_LIGHT, 620, 115, 1400, 14, 8, 13, -1, 0, BR_FACTORY, -1, "Decontamination truck: scrubs nuclear fallout in a 6-tile field. The zone's strength and radius decay while it works and units and structures inside the field are shielded from radiation, but the truck takes damage from what it absorbs (a silver cloud hurts it much more). Drive it into the worst of a crater, then pull it out or heal it with a medic (dozers cannot repair vehicles)" },
    { "Anti-Radiation Truck","R", F_CLANKER, UK_VEH, UR_DECON, AR_LIGHT, 620, 115, 1400, 14, 8, 13, -1, 0, BR_FACTORY, -1, "Decontamination truck: scrubs nuclear fallout in a 6-tile field. The zone's strength and radius decay while it works and units and structures inside the field are shielded from radiation, but the truck takes damage from what it absorbs (a silver cloud hurts it much more). Drive it into the worst of a crater, then pull it out or heal it with a medic (dozers cannot repair vehicles)" },
    // the plain fighter jets (both armies, built at the airfield, no tech needed): air superiority first, but their missiles strafe ground targets too.
    // Told to guard an area they guard it against aircraft and ground forces alike. They also spot infiltrators from the air.
    { "Raptor Fighter", "F", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR, 950, 600, 1600, 16, 11, 11, W_FIGHTER_C, 8, BR_AIRFIELD, -1, "Fighter jet: twin-engine air-superiority fighter. Arrow missiles hit aircraft hard and strafe ground targets; set to guard an area it guards against both. 8 missile passes, then it rearms on its pad. Spots infiltrators", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL },
    { "Saber Fighter",  "F", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR, 1000, 580, 1600, 16, 11, 12, W_FIGHTER_K, 8, BR_AIRFIELD, -1, "Fighter jet: rugged twin-engine air-superiority fighter. Saber missiles hit aircraft hard and strafe ground targets; set to guard an area it guards against both. 8 missile passes, then it rearms on its pad. Spots infiltrators", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL },
    // the strategic bombers of the Nuclear Bomber call-in: never built, never commanded; one pass, one mid-sized nuclear bomb, gone. Fields after desc:
    // program, jet, bomber, sniper, heli, stealth, detect, detectMask, cargoCap, naval, evade, temp
    { "Nightfall Bomber",   "-", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR, 3400, 640, 0, 0, 10, 20, -1, 0, BR_NONE, -1, "Strategic bomber of a Nuclear Bomber call-in: crosses the map once at altitude, drops one mid-sized nuclear bomb on the called spot and leaves. Strong anti-air can bring it down first", 0, 1, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 0, 0.18f, 1 },
    { "Thunderhead Bomber", "-", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR, 3600, 620, 0, 0, 10, 21, -1, 0, BR_NONE, -1, "Strategic bomber of a Nuclear Bomber call-in: crosses the map once at altitude, drops one mid-sized nuclear bomb on the called spot and leaves. Strong anti-air can bring it down first", 0, 1, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 0, 0.15f, 1 },
    // the patrol boats (both armies, built at the Naval Yard, nothing else needed): cheap, fast and fragile; one turret whose autocannon and rocket pod
    // hit aircraft, ships, troops, vehicles and shore structures alike. Weaker than a destroyer in every way, but a swarm of them adds up.
    { "Pulse Skiff",    "P", F_CYBER,   UK_VEH, UR_COMBAT, AR_LIGHT, 420, 150, 600, 9, 8, 13, W_PATROL_C, 0, BR_NAVAL, -1, "Patrol boat: cheap, fast and fragile. Its autocannon and micro-missiles hit aircraft, ships, infantry, vehicles and shore structures out to 6.5 tiles. Weak alone, a pack of them adds up. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Gunboat",        "P", F_CLANKER, UK_VEH, UR_COMBAT, AR_LIGHT, 450, 142, 600, 9, 8, 13, W_PATROL_K, 0, BR_NAVAL, -1, "Patrol boat: cheap, fast and fragile. Its autocannon and rocket pod hit aircraft, ships, infantry, vehicles and shore structures out to 6.5 tiles. Weak alone, a pack of them adds up. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    // the riot crowd (RIOT_PLAYER): civilians who took up arms against an army that lost control. Nobody builds or commands them (see Sim::updateRiots)
    { "Rioter",         "-", F_CLANKER, UK_INF, UR_COMBAT, AR_INF, 110, 58, 150, 0, 6, 6, W_RIOT_RIFLE, 0, BR_NONE, -1, "An armed civilian rioting against the army that lost control here: an old hunting rifle. Belongs to no army and fights every army" },
    { "Firebomber",     "-", F_CLANKER, UK_INF, UR_COMBAT, AR_INF, 100, 62, 150, 0, 6, 6, W_MOLOTOV, 0, BR_NONE, -1, "A rioter with a bag of petrol bombs: short range, but the fire wrecks structures. Belongs to no army and fights every army" },
    // aircraft carriers (Naval Yard with a tech structure): very tough, slow, close-in guns only; their real weapon is the wing of jets on deck (CARRIER_*)
    { "Horizon Carrier",   "A", F_CYBER,   UK_VEH, UR_COMBAT, AR_HEAVY, 7000, 70, 6500, 60, 13, 44, W_CARRIER_CIWS, 0, BR_NAVAL, B_C_TECH, "Aircraft carrier: a floating airbase with a wing of six Sea Raptor jets. Whenever an enemy comes within 22 tiles the jets launch on their own, hit aircraft, ships and anything near the shore, and land again to rearm; lost jets are replaced on deck. Very tough, slow, close-in guns for defence. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    { "Leviathan Carrier", "A", F_CLANKER, UK_VEH, UR_COMBAT, AR_HEAVY, 7400, 66, 6500, 60, 13, 44, W_CARRIER_CIWS, 0, BR_NAVAL, B_K_TECH, "Aircraft carrier: a floating airbase with a wing of six Sea Saber jets. Whenever an enemy comes within 22 tiles the jets launch on their own, hit aircraft, ships and anything near the shore, and land again to rearm; lost jets are replaced on deck. Very tough, slow, close-in guns for defence. A nuke sinks it", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, 0, 1, 0.0f, 0, 0 },
    // military cargo ships (Naval Yard, nothing else needed): unarmed, carry a battalion across the sea (CARGOSHIP_*)
    { "Tidewalker Transport", "T", F_CYBER,   UK_VEH, UR_TRANSPORT, AR_HEAVY, 3600, 100, 2400, 30, 9, 38, -1, 0, BR_NAVAL, -1, "Military cargo ship: carries 120 places of troops and vehicles (a soldier takes 1, a tank 6, a super-heavy 10) - Fabricators, haulers and whole tank columns. Bring it to a shore and right-click it with units selected to load; unload here, or pick a beach and it sails there and lands them. Unarmed. If it sinks, everyone aboard is lost", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, CARGOSHIP_CAP, 1, 0.0f, 0, 0 },
    { "Ironhold Freighter",   "T", F_CLANKER, UK_VEH, UR_TRANSPORT, AR_HEAVY, 3900, 96, 2400, 30, 9, 38, -1, 0, BR_NAVAL, -1, "Military cargo ship: carries 120 places of troops and vehicles (a soldier takes 1, a tank 6, a super-heavy 10) - Dozers, trucks and whole tank columns. Bring it to a shore and right-click it with units selected to load; unload here, or pick a beach and it sails there and lands them. Unarmed. If it sinks, everyone aboard is lost", 0, 0, 0, 0, 0, ST_NONE, 0.0f, 0, CARGOSHIP_CAP, 1, 0.0f, 0, 0 },
    // a carrier's jets: never built, never commanded (temp); they live on their carrier's deck (Sim::updateCarrier, Sim::carrierJetRun)
    { "Sea Raptor", "-", F_CYBER,   UK_AIR, UR_COMBAT, AR_AIR, 700, 610, 1200, 0, 10, 10, W_FIGHTER_C, 6, BR_NONE, -1, "Carrier jet of a Horizon Carrier: launches on its own against enemies near the carrier, six missile passes, then lands on the deck to rearm", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL, 0, 0, 0.08f, 1, 0 },
    { "Sea Saber",  "-", F_CLANKER, UK_AIR, UR_COMBAT, AR_AIR, 740, 590, 1200, 0, 10, 10, W_FIGHTER_K, 6, BR_NONE, -1, "Carrier jet of a Leviathan Carrier: launches on its own against enemies near the carrier, six missile passes, then lands on the deck to rearm", 0, 1, 0, 0, 0, ST_NONE, INFIL_DETECT, DET_DRONE | DET_INFIL, 0, 0, 0.08f, 1, 0 },
    // the laser commandos: never built (no barracks trains them), they only jump with a bunker's Para Backup. Ten Laser Troopers in one, and the
    // Toxic Lance leaves a small cloud where it hits that poisons the enemy's soldiers and vehicles round the target
    { "Prism Commando", "-", F_CYBER,   UK_INF, UR_COMBAT, AR_INF, 1000, 56, 3250, 0, 8, 7, W_TOXIC_LANCE, 0, BR_NONE, -1, "Laser commando of a bunker's Para Backup (no barracks trains them): ten times a Laser Trooper's health and firepower against aircraft, tanks and massed infantry. The Toxic Lance splashes and leaves a toxic cloud round its target that poisons enemy soldiers and vehicles" },
    { "Venom Commando", "-", F_CLANKER, UK_INF, UR_COMBAT, AR_INF, 1000, 56, 3250, 0, 8, 7, W_TOXIC_LANCE, 0, BR_NONE, -1, "Laser commando of a bunker's Para Backup (no barracks trains them): ten times a Laser Trooper's health and firepower against aircraft, tanks and massed infantry. The Toxic Lance splashes and leaves a toxic cloud round its target that poisons enemy soldiers and vehicles" },
};

const BuildType BUILDS[B_COUNT] = {
    // name, key, faction, role, hp, cost, time, w, h, power, sight, weapon, requires, desc
    { "Command Core",    "C", F_CYBER, BR_HQ,       18000, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Fabricators. The keystone of the army: the toughest structure in the game, heavily armoured against conventional fire, and one warhead takes at most 40% of its health - three direct hits (or a sustained assault) bring it down. A Fabricator can raise another one if this falls (max 3)" },
    { "Fusion Reactor",  "P", F_CYBER, BR_POWER,    1200, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Hub",      "S", F_CYBER, BR_SUPPLY,   2400, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds E-Haulers" },
    { "Barracks",        "B", F_CYBER, BR_BARRACKS, 2000, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "Assembly Plant",  "F", F_CYBER, BR_FACTORY,  2800, 2000, 25, 4, 3, -3, 7,  -1, B_C_POWER, "Builds vehicles" },
    { "Drone Pad",       "A", F_CYBER, BR_AIRFIELD, 3600, 1000, 20, 10, 7, -2, 8,  -1, B_C_FACTORY,"Huge airbase: builds and rearms Wraith Drones, Hornet Gunships and jets, with 25 fixed-wing pads and room for any number of helicopters" },
    { "Data Center",     "E", F_CYBER, BR_TECH,     7000, 2000, 30, 3, 3, -4, 8,  -1, B_C_FACTORY,"Unlocks Shock Trooper, Railgun Tank, strike powers and structure upgrades. A durable research building, but a direct warhead wrecks it" },
    { "Laser Turret",    "L", F_CYBER, BR_TURRET,   5200, 1000, 14, 1, 1, -3, 8,  7,  B_C_POWER, "Fortified ground defense laser, needs power: short reach, very hard to kill" },
    { "Patriot Battery", "T", F_CYBER, BR_AATURRET, 6500, 1200, 16, 2, 2, -3, 9,  8,  B_C_POWER, "Fortified missile defense: brutal vs air, good vs ground, very hard to kill" },
    { "Bitcoin Datacenter","M", F_CYBER, BR_INCOME,  1600, 1600, 22, 3, 2, -5, 7, -1, B_C_POWER, "Mines $450 every 5s (max 4), half rate on low power" },
    { "Nuke Ramp",       "K", F_CYBER, BR_NUKE,     2600, 8000, 60, 3, 3, -6, 8, -1, B_C_TECH,  "A nuke every 6 min per ramp (key K): huge blast, mushroom cloud, fallout" },

    { "Command Post",    "C", F_CLANKER, BR_HQ,       18500, 3000, 45, 4, 4, 0,  10, -1, -1,        "Trains Dozers. The keystone of the army: the toughest structure in the game, heavily armoured against conventional fire, and one warhead takes at most 40% of its health - three direct hits (or a sustained assault) bring it down. A Dozer can raise another one if this falls (max 3)" },
    { "Diesel Generator","P", F_CLANKER, BR_POWER,    1300, 800,  12, 3, 2, 10, 7,  -1, -1,        "Provides 10 power" },
    { "Supply Depot",    "S", F_CLANKER, BR_SUPPLY,   2600, 1500, 18, 3, 3, -1, 7,  -1, -1,        "Receives supplies, builds Supply Trucks" },
    { "Barracks",        "B", F_CLANKER, BR_BARRACKS, 2200, 500,  12, 3, 2, -1, 7,  -1, -1,        "Trains infantry" },
    { "War Factory",     "F", F_CLANKER, BR_FACTORY,  3000, 2000, 25, 4, 3, -3, 7,  -1, B_K_POWER, "Builds vehicles" },
    { "Airstrip",        "A", F_CLANKER, BR_AIRFIELD, 3800, 1000, 20, 10, 7, -2, 8,  -1, B_K_FACTORY,"Huge airbase: builds and rearms Vulture Gunships and jets, with 25 fixed-wing pads and room for any number of helicopters" },
    { "Arms Lab",        "E", F_CLANKER, BR_TECH,     7500, 2000, 30, 3, 3, -3, 8,  -1, B_K_FACTORY,"Unlocks Gunner, Rocket Launcher, strike powers and structure upgrades. A durable research building, but a direct warhead wrecks it" },
    { "Gun Nest",        "N", F_CLANKER, BR_TURRET,   6200, 900,  14, 1, 1, -1, 8,  16, -1,        "Fortified machine gun bunker, ground and light air, very hard to kill" },
    { "Rocket Battery",  "T", F_CLANKER, BR_AATURRET, 5400, 1200, 16, 2, 2, -3, 9,  17, B_K_POWER, "Fortified rocket defense, ground and air, very hard to kill" },
    { "Oil Well",        "O", F_CLANKER, BR_INCOME,   1500, 1400, 20, 2, 2, 0,  6, -1, B_K_POWER, "Pumps $380 every 5s forever (max 4), no power needed" },
    { "Nuke Ramp",       "K", F_CLANKER, BR_NUKE,     2800, 8000, 60, 3, 3, -5, 8, -1, B_K_TECH,  "A nuke every 6 min per ramp (key K): huge blast, mushroom cloud, fallout" },

    { "Deep Bunker",     "U", F_CYBER,   BR_BUNKER, 11000, 7500, 30, 3, 3, 0, 8, -1, B_C_BARRACKS, "Fortified deep shelter (visible on the surface): 200 infantry and five Fabricators. Earns $85 every 5s, mends itself fast, twin hatch miniguns, rocket pod and a salvo of two mini-nukes (every 90s). Nuclear-hardened: a warhead only scrapes it (it takes 20-30 at once to destroy one), and radiation never hurts it - its filters scrub the fallout round it. Each bunker can be fitted with its own upgrades. When surrounded, overdrive lasers shred aircraft, incoming nukes, tanks and infantry, and heavy hits vent a ring of radiation" },
    { "Underground Bunker","U", F_CLANKER, BR_BUNKER, 11000, 7500, 30, 3, 3, 0, 8, -1, B_K_BARRACKS, "Fortified deep shelter (visible on the surface): 200 infantry and five Dozers. Earns $85 every 5s, mends itself fast, twin hatch miniguns, rocket pod and a salvo of two mini-nukes (every 90s). Nuclear-hardened: a warhead only scrapes it (it takes 20-30 at once to destroy one), and radiation never hurts it - its filters scrub the fallout round it. Each bunker can be fitted with its own upgrades. When surrounded, overdrive heavy machineguns shred aircraft, incoming nukes, tanks and infantry, and heavy hits vent a ring of radiation" },

    { "Nano Drydock",   "Y", F_CYBER,   BR_NAVAL,    4200, 2600, 32, 5, 4, -3, 8, -1, B_C_FACTORY, "Naval Yard: a big drydock on the shore of the ocean or the lake (its ships stay on that sea). Builds patrol boats, Pulse Destroyers and Tidewalker cargo ships; with a Data Center also the Lancer Cruiser and the Horizon aircraft carrier" },
    { "Shipyard",       "Y", F_CLANKER, BR_NAVAL,    4500, 2600, 32, 5, 4, -3, 8, -1, B_K_FACTORY, "Naval Yard: a big shipyard on the shore of the ocean or the lake (its ships stay on that sea). Builds gunboats, Iron Destroyers and Ironhold cargo ships; with an Arms Lab also the Rocket Cruiser and the Leviathan aircraft carrier" },
    { "Anti-Nuke Laser","Q", F_CYBER,   BR_ANTINUKE, 1900, 1500, 18, 2, 2, -5, 9, -1, B_C_TECH, "Small tower that spots a nuclear warhead closing in on its 17 tile area (or whose blast would reach into it) and burns it from the sky, early and with no reload gap between warheads. Every one has its own chance; towers within 9 tiles of each other link up for more beams, faster tracking and a better chance each. A warhead shot down still drops pieces and radiation: 30% of the damage. Its chance against warheads is 40% lower than it used to be (rockets, shells, bombs and aircraft are still engaged as before). Needs power (max 8)" },
    { "Anti-Nuke Missiles","Q", F_CLANKER, BR_ANTINUKE, 2100, 1400, 18, 2, 2, -2, 9, -1, B_K_TECH, "Small silo that spots a nuclear warhead closing in on its 19 tile area and launches interceptors at it. Every one has its own chance, so the more you build, the likelier a warhead is stopped. A warhead shot down still drops pieces and radiation: 30% of the damage (max 8)" },
    // civilian infrastructure (Cyber, then Clanker), CivicKind order; hp, cost, time, w, h; no power, no sight beyond its yard, no weapon
    { "House", "H", F_CYBER, BR_CIVIC, 600, 600, 10, 2, 2, 0, 6, -1, -1, "A family home. Pays $30 every 10s and sends out a free dozer every 480s. No guns, no plating: only your defences protect it" },
    { "Apartments", "A", F_CYBER, BR_CIVIC, 900, 1200, 16, 3, 2, 0, 6, -1, -1, "An apartment block. Pays $65 every 10s and sends out a free dozer every 400s. No guns, no plating: only your defences protect it" },
    { "Skyscraper", "S", F_CYBER, BR_CIVIC, 1500, 4000, 34, 3, 3, 0, 6, -1, B_C_TOWNHALL, "A glass office tower. Pays $220 every 10s and sends out a free dozer every 280s. Needs a Town Hall. No guns, no plating: only your defences protect it" },
    { "Gas Station", "G", F_CYBER, BR_CIVIC, 650, 900, 12, 3, 2, 0, 6, -1, -1, "Fuel and a corner shop. Pays $50 every 10s and sends out a free dozer every 440s. No guns, no plating: only your defences protect it" },
    { "Grocery Store", "R", F_CYBER, BR_CIVIC, 800, 1000, 14, 3, 2, 0, 6, -1, -1, "The town's food market. Pays $55 every 10s and sends out a free dozer every 420s. No guns, no plating: only your defences protect it" },
    { "Event Hall", "E", F_CYBER, BR_CIVIC, 1000, 1600, 20, 3, 3, 0, 6, -1, -1, "Concerts, weddings and fairs: the social heart of the town. Pays $90 every 10s and sends out a free dozer every 340s. No guns, no plating: only your defences protect it" },
    { "Town Square", "Q", F_CYBER, BR_CIVIC, 600, 700, 10, 3, 3, 0, 6, -1, -1, "An open plaza with a fountain where the townsfolk meet. Pays $35 every 10s and sends out a free dozer every 400s. No guns, no plating: only your defences protect it" },
    { "Town Hall", "T", F_CYBER, BR_CIVIC, 1800, 3000, 30, 4, 3, 0, 6, -1, -1, "Where the town meets and is run (one per army). Pays $170 every 10s and sends out a free dozer every 220s. Unlocks skyscrapers and government offices. No guns, no plating: only your defences protect it" },
    { "Government Office", "O", F_CYBER, BR_CIVIC, 1600, 3500, 30, 4, 3, 0, 6, -1, B_C_TOWNHALL, "Ministries and records. Pays $190 every 10s and sends out a free dozer every 260s. Needs a Town Hall. No guns, no plating: only your defences protect it" },
    { "House", "H", F_CLANKER, BR_CIVIC, 600, 600, 10, 2, 2, 0, 6, -1, -1, "A family home. Pays $30 every 10s and sends out a free dozer every 480s. No guns, no plating: only your defences protect it" },
    { "Apartments", "A", F_CLANKER, BR_CIVIC, 900, 1200, 16, 3, 2, 0, 6, -1, -1, "An apartment block. Pays $65 every 10s and sends out a free dozer every 400s. No guns, no plating: only your defences protect it" },
    { "Skyscraper", "S", F_CLANKER, BR_CIVIC, 1500, 4000, 34, 3, 3, 0, 6, -1, B_K_TOWNHALL, "A glass office tower. Pays $220 every 10s and sends out a free dozer every 280s. Needs a Town Hall. No guns, no plating: only your defences protect it" },
    { "Gas Station", "G", F_CLANKER, BR_CIVIC, 650, 900, 12, 3, 2, 0, 6, -1, -1, "Fuel and a corner shop. Pays $50 every 10s and sends out a free dozer every 440s. No guns, no plating: only your defences protect it" },
    { "Grocery Store", "R", F_CLANKER, BR_CIVIC, 800, 1000, 14, 3, 2, 0, 6, -1, -1, "The town's food market. Pays $55 every 10s and sends out a free dozer every 420s. No guns, no plating: only your defences protect it" },
    { "Event Hall", "E", F_CLANKER, BR_CIVIC, 1000, 1600, 20, 3, 3, 0, 6, -1, -1, "Concerts, weddings and fairs: the social heart of the town. Pays $90 every 10s and sends out a free dozer every 340s. No guns, no plating: only your defences protect it" },
    { "Town Square", "Q", F_CLANKER, BR_CIVIC, 600, 700, 10, 3, 3, 0, 6, -1, -1, "An open plaza with a fountain where the townsfolk meet. Pays $35 every 10s and sends out a free dozer every 400s. No guns, no plating: only your defences protect it" },
    { "Town Hall", "T", F_CLANKER, BR_CIVIC, 1800, 3000, 30, 4, 3, 0, 6, -1, -1, "Where the town meets and is run (one per army). Pays $170 every 10s and sends out a free dozer every 220s. Unlocks skyscrapers and government offices. No guns, no plating: only your defences protect it" },
    { "Government Office", "O", F_CLANKER, BR_CIVIC, 1600, 3500, 30, 4, 3, 0, 6, -1, B_K_TOWNHALL, "Ministries and records. Pays $190 every 10s and sends out a free dozer every 260s. Needs a Town Hall. No guns, no plating: only your defences protect it" },
    // Underground Nuke silos: ten times the price of a Nuke Ramp, tougher than a bunker (only the Command structure outlasts them), a silver warhead 2.7x as strong
    { "Underground Nuke","G", F_CYBER,   BR_UNUKE, 28000, 8000, 80, 3, 3, -6, 8, -1, B_C_NUKE, "Buried nuclear silo (key K with it selected): a silver warhead every 6 min with three times a Nuke Ramp's blast radius and radiation and 2.7 times its damage. Blast doors and deep plating make it the toughest building of all; warheads ignore the floor and repeated direct hits destroy it. Only a laser (Anti-Nuke Laser or an Anti-Rocket wall) can shoot its warhead down, and its armoured silver casing halves even that chance" },
    { "Underground Nuke","G", F_CLANKER, BR_UNUKE, 29000, 8000, 80, 3, 3, -5, 8, -1, B_K_NUKE, "Buried nuclear silo (key K with it selected): a silver warhead every 6 min with three times a Nuke Ramp's blast radius and radiation and 2.7 times its damage. Blast doors and deep plating make it the toughest building of all; warheads ignore the floor and repeated direct hits destroy it. Only a laser (Anti-Nuke Laser or an Anti-Rocket wall) can shoot its warhead down, and its armoured silver casing halves even that chance" },
    // wall segments: unlimited and cheap; sturdy against small arms and light vehicles, but a warhead flattens one at ground zero
    { "Wall", "W", F_CYBER, BR_WALL, WALL_HP, 180, 3, 1, 1, 0, 5, -1, -1, "A reinforced wall segment. Build as many as you like and chain them into a line: it soaks conventional fire, but a nuclear warhead flattens it at ground zero like any small structure. Hyper Protection doubles its health; Anti-Rocket gives it a point-defence laser" },
    { "Wall", "W", F_CLANKER, BR_WALL, WALL_HP, 180, 3, 1, 1, 0, 5, -1, -1, "A reinforced wall segment. Build as many as you like and chain them into a line: it soaks conventional fire, but a nuclear warhead flattens it at ground zero like any small structure. Hyper Protection doubles its health; Anti-Rocket gives it a point-defence laser" },
    // Neutral civilian strongholds: pre-placed map landmarks, never built. Empty and unowned at the start; any army's infantry can garrison one,
    // and its garrison fires from the windows. They are heavy masonry and deliberately tough, but no army upgrade applies to them.
    { "Old Castle", "-", F_CYBER, BR_STRONGHOLD, 15000, 0, 0, 4, 4, 0, 11, -1, -1, "An abandoned civilian castle: a weathered stone keep with towers and loopholes. Empty by default. Any army's infantry can walk in and claim it; while a garrison is inside they shoot from the arrow slits and windows. Very heavy masonry (no army upgrade applies), so it takes a sustained siege or repeated warheads to bring down" , 24 },
    { "Civilian Bunker", "-", F_CYBER, BR_STRONGHOLD, 11000, 0, 0, 3, 3, 0, 10, -1, -1, "A civilian shelter of reinforced concrete left over from an older war. Empty by default. Any army's infantry can garrison it and fire from its firing ports; the shelter protects them until the building itself is destroyed. Thick walls shrug off most conventional fire, and no single warhead can erase a full-health bunker" , 14 },
    { "Watchtower", "-", F_CYBER, BR_STRONGHOLD, 7000, 0, 0, 2, 2, 0, 12, -1, -1, "A tall stone watchtower with a commanding view. Empty by default. Any army's infantry can climb in and shoot from the top; its height gives the garrison a clear field of fire. Strong enough to hold a crossing or a wood line until the enemy brings artillery or a warhead" , 8 },
    { "Abandoned Cottage", "-", F_CYBER, BR_STRONGHOLD, 2400, 0, 0, 2, 2, 0, 7, -1, -1, "A small stone house. Infantry can claim it for cover; no income or production", 6 },
    { "Roadside Workshop", "-", F_CYBER, BR_STRONGHOLD, 3200, 0, 0, 3, 2, 0, 8, -1, -1, "An old brick workshop. Infantry can claim its firing positions; no income or production", 8 },
    { "Mountain Fortress", "-", F_CYBER, BR_STRONGHOLD, 52000, 0, 0, 5, 4, 0, 13, -1, -1, "A great concrete bunker complex dug into the mountainside: casemates, firing slits, roof gun pits and steel blast doors. Empty and unclaimed at the start; the first army to march infantry in claims it. Up to 100 infantry fire from it at once, at the ground and the sky, with extra reach from the high galleries. Its reinforced concrete shrugs off most conventional fire and no warhead takes more than a quarter of it: it takes several nukes. An enemy spy can take it (snipers inside will see one coming)", 100 },
    { "Island Nuke Launcher", "-", F_CYBER, BR_ISLANDNUKE, 20000, 0, 0, 3, 3, 0, 10, -1, -1, "A launcher in a laser chamber on a lake island. Two stand on each island. Neutral at the start; an infiltrator takes it (it swims out on its raft, or a lifter flies it in) and its army may then fire a Super Nuke every 75 s (K with it selected): twice an Underground Nuke's blast radius, very fast, untouchable by bunkers and interceptor missiles, and five times harder for a laser to stop than an Underground Nuke. Nothing destroys it: warheads and heavy fire only set it burning, and it keeps working. Only a capture by the enemy stops it" },
    { "Bastion Wall", "X", F_CYBER,   BR_FWALL, WALL_HP, 180, 3, 1, 1, 0, 5, -1, -1, "An indestructible wall segment, the same price as a wall: nothing breaks it, not even a nuke (it only burns). It carries no guns and no point-defence laser (only the ordinary wall does), and an enemy spy can take it over" },
    { "Bastion Wall", "X", F_CLANKER, BR_FWALL, WALL_HP, 180, 3, 1, 1, 0, 5, -1, -1, "An indestructible wall segment, the same price as a wall: nothing breaks it, not even a nuke (it only burns). It carries no guns and no point-defence laser (only the ordinary wall does), and an enemy spy can take it over" },
    // Waymakers: placed like a structure, they clear trees, rock and ridge under their 3x3 footprint into open ground by themselves, then vanish
    { "Waymaker", "Z", F_CYBER,   BR_WAYMAKER, 500, 100, WAYMAKER_TIME, 3, 3, 0, 3, -1, -1, "Clears the way: place it on trees, rocks or a ridge and in a few seconds the 3x3 patch under it becomes open, walkable ground (nothing is destroyed). No Fabricator has to reach it, so chain them to cut a road into rough ground. Not on water" },
    { "Waymaker", "Z", F_CLANKER, BR_WAYMAKER, 500, 100, WAYMAKER_TIME, 3, 3, 0, 3, -1, -1, "Clears the way: place it on trees, rocks or a ridge and in a few seconds the 3x3 patch under it becomes open, walkable ground (nothing is destroyed). No Dozer has to reach it, so chain them to cut a road into rough ground. Not on water" },
    // Healing Antennas: cheap, unlimited, fragile; they mend everything friendly round them and slowly scrub radiation
    { "Healing Antenna", "H", F_CYBER,   BR_HEALANT, 800, 250, 6, 1, 1, 0, 6, -1, -1, "A cheap repair-and-medical mast; build as many as you like. Every half second it mends everything of yours and your allies' within 5 tiles: soldiers, vehicles, ships, aircraft overhead and structures (several antennas over one spot do not add up). It also slowly scrubs radiation it reaches (much slower than an Anti-Radiation Truck). As fragile as any small structure" },
    { "Healing Antenna", "H", F_CLANKER, BR_HEALANT, 800, 250, 6, 1, 1, 0, 6, -1, -1, "A cheap repair-and-medical mast; build as many as you like. Every half second it mends everything of yours and your allies' within 5 tiles: soldiers, vehicles, ships, aircraft overhead and structures (several antennas over one spot do not add up). It also slowly scrubs radiation it reaches (much slower than an Anti-Radiation Truck). As fragile as any small structure" },
};

// income per payout (every CIVIC_INTERVAL s), seconds between free dozers, townsfolk walking about it (decoration only)
const CivicType CIVICS[CV_COUNT] = {
    { 30, 480.0f, 3 },   // House
    { 65, 400.0f, 7 },   // Apartments
    { 220, 280.0f, 10 },   // Skyscraper
    { 50, 440.0f, 4 },   // Gas Station
    { 55, 420.0f, 7 },   // Grocery Store
    { 90, 340.0f, 10 },   // Event Hall
    { 35, 400.0f, 12 },   // Town Square
    { 170, 220.0f, 8 },   // Town Hall
    { 190, 260.0f, 7 },   // Government Office
};

const PowerType POWERS[F_COUNT] = {
    { "EMP Strike",  "Disables enemy vehicles and structures (tech included) for 8s and downs aircraft in the area", 180.0f, 4.5f },
    { "Shell Storm", "Rains artillery shells on the area for 4s",                 180.0f, 3.5f },
};
const ScanType SCANS[F_COUNT] = {
    { "Orbital Scan", "A satellite pass shows the whole map, bright as day even at night, for 3 minutes",                   210.0f, 180.0f },
    { "Recon Flight", "A spotter plane shows the whole map, bright as day even at night (flares and night optics), for 3 minutes", 210.0f, 180.0f },
};
const ProgramType PROGRAMS[F_COUNT] = {
    { "Overclock Program", "Unlocks Ion Lancer and Aegis Titan", 2500, 45.0f },
    { "Heavy Ordnance",    "Unlocks Grenadier and Behemoth",     2500, 45.0f },
};

const UpgradeType UPGRADES[F_COUNT][UPG_COUNT] = {
    { { "Rugged Nanoshell", "U", "Every structure gets 50% more health and shrugs off half of all damage. One nuke can no longer flatten it: it takes two close nukes or a massive assault at once", 3000, 40.0f },
      { "Defense Lasers",   "G", "Every structure gets a roof laser that shoots down aircraft and burns vehicles and infantry", 3500, 45.0f },
      { "Nanite Repair",    "E", "Every structure repairs itself over time (slower while under fire). Units still need Medic Rigs", 2000, 30.0f },
      { "Aegis Guard Drones","N", "Every ship flies with a screen of three protector drones that shoot down rockets and shells aimed at it (lasers and bullets are too quick), and every jeep gets a second drone", 2500, 40.0f },
      { "Hyper Protection", "H", "Every structure gets 100% more health, takes 60% less damage from conventional fire, and can never lose more than 42% of its full health to a single warhead. Even an Underground Nuke can no longer erase a full-health protected base: it only finishes buildings that were already damaged", 6000, 55.0f },
      { "Anti-Rocket",      "A", "Every wall mounts a point-defence laser (10 tiles): it burns incoming rockets, shells and nuclear warheads, and shoots down aircraft. Walls are unlimited, so a line of them blankets a base in overlapping fire", 4000, 45.0f } },
    { { "Rugged Bulwark",   "U", "Every structure gets 50% more health and shrugs off half of all damage. One nuke can no longer flatten it: it takes two close nukes or a massive assault at once", 3000, 40.0f },
      { "Bunker Guns",      "G", "Every structure gets a roof machine gun that shoots down aircraft and shreds infantry and vehicles", 3500, 45.0f },
      { "Repair Crews",     "E", "Every structure repairs itself over time (slower while under fire). Units still need Field Medics", 2000, 30.0f },
      { "Flak Guard Drones","N", "Every ship flies with a screen of three protector drones that shoot down rockets and shells aimed at it (lasers and bullets are too quick), and every jeep gets a second drone", 2500, 40.0f },
      { "Hyper Protection", "H", "Every structure gets 100% more health, takes 60% less damage from conventional fire, and can never lose more than 42% of its full health to a single warhead. Even an Underground Nuke can no longer erase a full-health protected base: it only finishes buildings that were already damaged", 6000, 55.0f },
      { "Anti-Rocket",      "A", "Every wall mounts a point-defence laser (10 tiles): it burns incoming rockets, shells and nuclear warheads, and shoots down aircraft. Walls are unlimited, so a line of them blankets a base in overlapping fire", 4000, 45.0f } },
};

// Per-bunker upgrades (see BunkerUpg): bought for the selected bunker alone, one fitting at a time
const BunkerUpgType BUNKER_UPGS[BU_COUNT] = {
    { "Thick Shell", "Shell",   "T", "Super-thick shell for this bunker: 80% more health, a hatch two and a half times as strong, 45% less damage from guns and 40% less from every warhead", 3000, 30.0f },
    { "Laser Battery", "Lasers", "L", "Two laser turrets on this bunker's roof: they burn down incoming warheads (24 tiles round it), rockets, shells and bombs aimed at anything of yours nearby, and shoot down any aircraft in 12 tiles", 4000, 35.0f },
    { "Field Hospital", "Hospital","H", "A field hospital in this bunker: everything of yours within 8 tiles - soldiers, vehicles, aircraft and structures - is mended all the time, even under fire", 2500, 25.0f },
    { "Triple Launch", "Triple", "N", "Three launch tubes for this bunker: every Mini Nuke launch fires three missiles at once, spread over the target (instead of a pair)", 5000, 40.0f },
    { "Swift Warheads", "Swift","W", "Boosted, armoured missiles for this bunker: its mini-nukes reach the target in half the time and every defence's chance to shoot one down drops to 30%", 3500, 30.0f },
    { "Para Backup", "Para",   "B", "A radio mast for this bunker: once a minute it calls a cargo plane that drops 35 paratroopers anywhere you pick - five laser commandos no barracks can train (ten times a Laser Trooper, with toxic lances), riflemen, anti-air troopers and snipers, and every other kind of soldier your army has unlocked (never infiltrators)", 4500, 35.0f },
    { "Rain Rack", "Rain",     "R", "A rack of small warheads for this bunker: the Rain Nukes power - five small nukes fall at random spots in a wide circle you pick (10 tiles), in under two seconds. Almost nothing can shoot them down. Once every two minutes", 6000, 40.0f },
    { "Heavy Lift Pad", "Tanks","V", "A heavy lift pad for this bunker: the Tank Drop power - a heavy cargo plane drops ten tanks and a medic vehicle on the spot you pick (super-heavy tanks among them once unlocked). Once every two and a half minutes", 5500, 35.0f },
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
    { "Air Strike", "One call to Cyber Command: waves of light-grey Valkyrie strike fighters come in from the sea in V formations, each fires two rockets and lays a stick of bombs across the spot you pick, then breaks away and vanishes over the ocean. 10 fighters, 14 with the Advanced Program, 18 with the structure upgrades. Needs a Data Center; one call per 4 min per Data Center" },
    { "Air Strike", "One call to the battery: waves of light-grey Warhawk strike fighters come in from the sea in V formations, each fires two rockets and lays a stick of bombs across the spot you pick, then breaks away and vanishes over the ocean. 10 fighters, 14 with the Advanced Program, 18 with the structure upgrades. Needs an Arms Lab; one call per 4 min per Arms Lab" },
};

const FleetType FLEETS[F_COUNT] = {
    { "Fleet Call", "Cyber Command sends a battle fleet from the open sea: ten Pulse Skiffs, three destroyers, three missile cruisers and a Horizon Carrier with its full wing. They come out of the haze at the map border nearest the spot you pick on the ocean, sail in and form up round it, then they are yours to command. Needs a Data Center; one fleet per paradrop cooldown per Data Center" },
    { "Fleet Call", "The admiralty sends a battle fleet from the open sea: ten Gunboats, three destroyers, three missile cruisers and a Leviathan Carrier with its full wing. They come out of the haze at the map border nearest the spot you pick on the ocean, sail in and form up round it, then they are yours to command. Needs an Arms Lab; one fleet per paradrop cooldown per Arms Lab" },
};

int firstUnitOf(Faction f) { return f == F_CYBER ? U_C_DOZER : U_K_DOZER; }
int firstBuildOf(Faction f) { return f == F_CYBER ? B_C_HQ : B_K_HQ; }
