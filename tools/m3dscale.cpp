// One Hour - model viewer helper: the size a unit type is drawn at, copied from art.cpp so the viewer needs none of the old painters.
#include "../src/data.h"
static int unitLocal(int type) {
    if (subType(type)) return 35;                                       // nuclear submarines (art_ship.cpp, 128 px canvas)
    if (battleshipType(type)) return 36;                                // battleships (art_ship.cpp, 128 px canvas)
    if (relicUnit(type)) return 37 + (type - U_R_JUGGERNAUT);           // the relic war machines (art_relic.cpp): 37 Juggernaut, 38 Strider, 39 Thunderhawk
    if (civicDozerType(type)) return 22;   // the civilian dozers: a dozer hull in civilian livery
    if (type == U_C_HELI) return 14;
    if (type == U_C_SBOMBER || type == U_K_SBOMBER) return 23;
    if (type == U_C_RADTRUCK || type == U_K_RADTRUCK) return 24;
    if (type == U_C_FIGHTER || type == U_K_FIGHTER) return 25;          // the plain fighters (art_jet.cpp)
    if (type == U_C_NUKEBOMBER || type == U_K_NUKEBOMBER) return 26;    // the call-in strategic bombers (art_jet.cpp)
    if (type == U_C_PATROL || type == U_K_PATROL) return 27;            // the patrol boats (art_ship.cpp)
    if (type == U_RIOTER || type == U_FIREBOMBER) return 28;            // the civilian rioters (art_riot.cpp)
    if (type == U_C_CARRIER || type == U_K_CARRIER) return 29;          // aircraft carriers (art_ocean.cpp, 128 px canvas)
    if (type == U_C_CARGOSHIP || type == U_K_CARGOSHIP) return 30;      // cargo ships (art_ocean.cpp, 128 px canvas)
    if (type == U_C_CARRIERJET || type == U_K_CARRIERJET) return 31;    // carrier jets (art_ocean.cpp)
    if (type == U_C_LASERCMD || type == U_K_LASERCMD) return 32;        // laser commandos (art_commando.cpp)
    if (chosenType(type)) return 33;                                    // The Chosen (art_chosen.cpp)
    if (shadowType(type)) return 34;                                    // its Shadow Guard (art_chosen.cpp)
    if (type >= U_C_SPY) return 15 + (type - U_C_SPY) / 2;
    return type >= U_C_SNIPER ? 13 : type >= U_C_MEDIC ? 12 : type - firstUnitOf(UNITS[type].faction);
}
static float artScale(int type) {
    int local = unitLocal(type);
    if (local == 10) return 1.26f;   // Aegis Titan / Behemoth
    if (local == 11) return 1.28f;   // supersonic jets: long and slender, drawn large enough to read at a glance
    if (local == 9) return 1.06f;    // elite infantry
    if (local == 33) return 1.12f;   // The Chosen: a laser commando's footprint
    if (local == 17) return 2.0f;    // cargo lifters: the biggest airframes on the field
    if (local == 18) return 1.2f;    // jeeps
    if (local == 19) return 1.75f;   // destroyers
    if (local == 20) return 1.95f;   // cruisers
    if (local == 21) return 1.4f;    // Hotline Strike fighters
    if (local == 23) return 1.16f;   // Supersonic Bombers: small and fast, a notch smaller than the strike jets
    if (local == 24) return 1.06f;   // Anti-Radiation Trucks: a medium truck, a touch smaller than the jeeps
    if (local == 25) return 1.22f;   // plain fighters
    if (local == 26) return 2.1f;    // the call-in strategic bombers: the biggest thing in the sky
    if (local == 27) return 1.2f;    // patrol boats: well under half a destroyer's length
    if (local == 29) return 4.6f;    // aircraft carriers: about two and a half destroyers long (128 px canvas: see artCanvas)
    if (local == 30) return 4.0f;    // cargo ships
    if (local == 31) return 1.16f;   // carrier jets: a little smaller than the plain fighters
    if (local == 32) return 1.12f;   // laser commandos: bulkier than an elite trooper, still infantry-sized
    if (local == 16) return 0.75f;   // spy drones: small and delicate
    if (local == 35) return 1.7f;    // nuclear submarines: about a destroyer long, slim (128 px canvas)
    if (local == 36) return 3.0f;    // battleships: about 1.7 destroyers long (128 px canvas)
    if (local == 37) return 1.22f;   // Relic Juggernaut: a super-heavy's length
    if (local == 38) return 1.15f;   // Relic Strider: its legs reach wide
    if (local == 39) return 1.10f;   // Relic Thunderhawk
    return 1.0f;
}
float viewScale(int type) { int l = unitLocal(type); bool big = l == 29 || l == 30 || l == 35 || l == 36; return 0.65f * artScale(type) * (big ? 0.5f : 1.0f); }
