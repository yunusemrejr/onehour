#include "art.h"
#include "art2.h"
#include "paint.h"

using namespace paint;

// Everything is authored in 64x64 canvas pixels, facing +x. Shapes are signed distance fields: each pixel gets
// anti-aliased coverage and a bevel lit from the north-west, which is what gives the hulls their metal look.
namespace {

// Palette in use for the unit being drawn. Every unit type has its own tone (setTone), so a faction's army is a family of
// related materials rather than one repainted hull: Cyber is graphite, cobalt, ceramic and teal lit by an emitter colour;
// Clanker is olive, sand, brick and iron with rust, hazard yellow and steel trim.
Color CYH, CYP, CYD, CYG, CYW, CYM;
Color CKH, CKP, CKD, CKR, CKY, CKS, CKT;
const Color CYH0 = rgb(66, 86, 120), CYP0 = rgb(108, 140, 182), CYD0 = rgb(24, 32, 48), CYG0 = rgb(110, 232, 255), CYW0 = rgb(216, 238, 248), CYM0 = rgb(42, 54, 78);
const Color CKH0 = rgb(94, 104, 64), CKP0 = rgb(142, 134, 98), CKD0 = rgb(38, 40, 32), CKR0 = rgb(156, 90, 48), CKY0 = rgb(226, 182, 62), CKS0 = rgb(124, 126, 124), CKT0 = rgb(160, 144, 102);

struct CyTone { Color h, p, m, d, g; };
struct CkTone { Color h, p, d, r, y, s, t; };
const CyTone CY_TONES[12] = {
    { rgb(104, 114, 128), rgb(152, 164, 178), rgb(64, 72, 86),  rgb(30, 34, 42), rgb(255, 190, 92)  },   // Fabricator: brushed titanium, amber work lights
    { rgb(34, 104, 112),  rgb(82, 154, 160),  rgb(26, 62, 70),  rgb(16, 38, 44), rgb(120, 255, 214) },   // E-Hauler: deep teal
    { rgb(70, 92, 130),   rgb(112, 146, 192), rgb(44, 56, 82),  rgb(24, 32, 48), rgb(110, 232, 255) },   // Trooper: slate blue
    { rgb(150, 168, 190), rgb(200, 216, 232), rgb(88, 104, 126),rgb(40, 50, 66), rgb(96, 226, 255)  },   // Laser Trooper: pale ceramic
    { rgb(52, 56, 74),    rgb(92, 98, 124),   rgb(32, 34, 50),  rgb(16, 18, 28), rgb(206, 232, 255) },   // Shock Trooper: charcoal, white arcs
    { rgb(38, 82, 156),   rgb(88, 134, 208),  rgb(26, 50, 98),  rgb(14, 26, 54), rgb(110, 232, 255) },   // Photon Tank: cobalt
    { rgb(28, 76, 80),    rgb(60, 126, 126),  rgb(18, 44, 50),  rgb(10, 26, 30), rgb(140, 255, 202) },   // Volt Walker: dark teal
    { rgb(46, 50, 64),    rgb(90, 96, 114),   rgb(30, 32, 44),  rgb(14, 16, 22), rgb(232, 250, 255) },   // Railgun Tank: graphite
    { rgb(32, 38, 54),    rgb(68, 76, 98),    rgb(22, 26, 40),  rgb(10, 12, 20), rgb(110, 232, 255) },   // Wraith Drone: matte stealth
    { rgb(178, 192, 206), rgb(224, 234, 244), rgb(102, 116, 134),rgb(44, 52, 66), rgb(150, 255, 255) },  // Ion Lancer: polar white
    { rgb(82, 90, 106),   rgb(134, 144, 160), rgb(52, 58, 74),  rgb(22, 26, 36), rgb(255, 214, 124) },   // Aegis Titan: titanium, gold emitters
    { rgb(44, 54, 76),    rgb(92, 112, 146),  rgb(26, 32, 48),  rgb(12, 16, 26), rgb(120, 240, 255) },   // Specter Jet: gunmetal, ice-blue emitters
};
const CkTone CK_TONES[12] = {
    { rgb(96, 104, 68),  rgb(142, 134, 98),  rgb(38, 40, 32), rgb(156, 90, 48),  rgb(232, 186, 58),  rgb(124, 126, 124), rgb(160, 144, 102) },   // Dozer: yellow machine
    { rgb(112, 88, 60),  rgb(158, 130, 92),  rgb(40, 34, 28), rgb(164, 84, 44),  rgb(226, 182, 62),  rgb(124, 122, 116), rgb(168, 140, 96)  },   // Supply Truck: earth brown
    { rgb(92, 106, 62),  rgb(138, 142, 96),  rgb(36, 40, 30), rgb(150, 92, 50),  rgb(226, 182, 62),  rgb(124, 126, 124), rgb(150, 138, 96)  },   // Rifleman: olive drab
    { rgb(162, 142, 96), rgb(196, 176, 128), rgb(50, 44, 34), rgb(170, 84, 44),  rgb(234, 190, 70),  rgb(128, 126, 120), rgb(190, 166, 116) },   // RPG Trooper: desert khaki
    { rgb(58, 68, 54),   rgb(98, 108, 90),   rgb(26, 30, 26), rgb(140, 84, 48),  rgb(220, 178, 62),  rgb(112, 116, 112), rgb(120, 122, 96)  },   // Gunner: charcoal green
    { rgb(82, 98, 54),   rgb(126, 138, 86),  rgb(34, 40, 28), rgb(154, 90, 46),  rgb(226, 182, 62),  rgb(122, 126, 120), rgb(152, 142, 96)  },   // Brute Tank: heavy olive
    { rgb(172, 150, 98), rgb(204, 184, 132), rgb(54, 46, 34), rgb(168, 88, 46),  rgb(236, 192, 72),  rgb(128, 126, 118), rgb(196, 172, 118) },   // Gatling Tank: sand
    { rgb(120, 78, 54),  rgb(166, 118, 88),  rgb(44, 32, 26), rgb(174, 96, 50),  rgb(230, 184, 60),  rgb(128, 120, 112), rgb(176, 138, 100) },   // Rocket Launcher: brick
    { rgb(74, 88, 96),   rgb(116, 132, 140), rgb(28, 34, 38), rgb(150, 92, 52),  rgb(226, 182, 62),  rgb(132, 136, 138), rgb(150, 150, 130) },   // Vulture: slate blue-grey
    { rgb(108, 62, 56),  rgb(150, 100, 90),  rgb(40, 26, 24), rgb(180, 96, 50),  rgb(236, 190, 66),  rgb(126, 118, 112), rgb(170, 128, 104) },   // Grenadier: maroon
    { rgb(74, 76, 72),   rgb(118, 118, 108), rgb(30, 30, 28), rgb(162, 92, 48),  rgb(238, 190, 60),  rgb(140, 142, 140), rgb(150, 146, 120) },   // Behemoth: cast iron, hazard trim
    { rgb(112, 120, 92), rgb(158, 160, 124), rgb(34, 36, 30), rgb(168, 94, 48),  rgb(236, 190, 62),  rgb(150, 152, 150), rgb(176, 160, 116) },   // Talon Jet: olive drab over sand
};

// type < 0 selects the neutral default palette (structures' turret heads)
// position inside the army's block; the medics live after both blocks but are drawn as the 13th design. The newest designs follow the
// Cyber helicopter in pairs: spies 15, spy drones 16, cargo lifters 17 (Cyber first in each pair, as everywhere else).
// The Supersonic Bombers are the newest pair: local 23, pinned explicitly so appending further pairs to the enum can never
// silently repaint them (the pair rule below happens to land on 23 today, but it is a spacer rule, not a contract).
// The Anti-Radiation Trucks follow as local 24, pinned for the same reason.
static int unitLocal(int type) {
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
    if (type >= U_C_SPY) return 15 + (type - U_C_SPY) / 2;
    return type >= U_C_SNIPER ? 13 : type >= U_C_MEDIC ? 12 : type - firstUnitOf(UNITS[type].faction);
}

void setTone(int type) {
    CYH = CYH0; CYP = CYP0; CYD = CYD0; CYG = CYG0; CYW = CYW0; CYM = CYM0;
    CKH = CKH0; CKP = CKP0; CKD = CKD0; CKR = CKR0; CKY = CKY0; CKS = CKS0; CKT = CKT0;
    if (type < 0) return;
    int local = unitLocal(type);
    if (local < 0 || local > 11) return;
    if (UNITS[type].faction == F_CYBER) {
        const CyTone& t = CY_TONES[local];
        CYH = shade(t.h, 1.03f); CYP = shade(t.p, 1.08f); CYM = t.m; CYD = shade(t.d, 0.92f); CYG = t.g;   // plates catch more light, recesses sit deeper
        CYW = mix(t.g, rgb(255, 255, 255), 0.72f);
    } else {
        const CkTone& t = CK_TONES[local];
        CKH = shade(t.h, 1.03f); CKP = shade(t.p, 1.08f); CKD = shade(t.d, 0.92f); CKR = t.r; CKY = t.y; CKS = t.s; CKT = t.t;
    }
}

// The player's mark on a unit: a painted roundel, the way real armies mark their vehicles (no glass bead): a thin dark rim for contrast on any hull
// tone, the team colour as flat paint with only the faint relief of the plate under it, and a paler inner ring so it still reads at half size.
void teamDot(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 0.9f, rgba(rgb(18, 20, 24), 225), 0.6f, 0.15f);
    A.circle(cx, cy, r, shade(A.team, 0.94f), 0.8f, 0.22f);
    A.ring(cx, cy, r * 0.66f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.42f), 210), 0.4f, 0);
    A.dot(cx, cy, r * 0.28f, rgba(shade(A.team, 0.7f), 220));
}

// crawler track run: rubber band with grousers, idler wheels at both ends. phase (0..1) slides the grousers.
void trackRun(Art& A, float cx, float cy, float hl, float hh, float phase, Color dark) {
    A.box(cx, cy, hl, hh, hh * 0.6f, dark, 1.6f, 0.3f);
    float pitch = 3.4f;
    for (float x = cx - hl + 1.5f + phase * pitch; x < cx + hl - 1.5f; x += pitch) A.line(x, cy - hh + 0.9f, x, cy + hh - 0.9f, 1.0f, rgba(shade(dark, 2.3f), 150));
    A.circle(cx - hl + hh * 0.75f, cy, hh * 0.78f, shade(dark, 1.5f), 1.6f, 0.45f);
    A.circle(cx + hl - hh * 0.75f, cy, hh * 0.78f, shade(dark, 1.5f), 1.6f, 0.45f);
    A.dot(cx - hl + hh * 0.75f, cy, hh * 0.28f, shade(dark, 0.7f));
    A.dot(cx + hl - hh * 0.75f, cy, hh * 0.28f, shade(dark, 0.7f));
}

void hatch(Art& A, float cx, float cy, float r, Color base) {
    A.circle(cx, cy, r, base, 1.8f, 0.55f);
    A.circle(cx, cy, r * 0.62f, shade(base, 0.72f), 1.2f, 0.5f);
    A.dot(cx - r * 0.2f, cy - r * 0.2f, r * 0.14f, shade(base, 1.6f));
}

void rivets(Art& A, float x0, float y0, float x1, float y1, int n, Color c) {
    for (int i = 0; i < n; i++) { float t = n > 1 ? i / (float)(n - 1) : 0.5f; A.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.75f, c); }
}

void weatherClanker(Art& A, u32 seed, int x0, int y0, int x1, int y1) {
    A.grain(x0, y0, x1, y1, 0.16f, seed);
}

// ---------------------------------------------------------------- infantry
struct Trooper { Color armor, dark, helmet, gun; };

void soldier(Art& A, bool cy, int local, int frame) {
    Canvas& c = A.c; (void)c;
    Color armor = cy ? shade(CYH, 1.05f) : shade(CKH, 0.95f);
    Color dark = cy ? CYD : CKD;
    Color helmet = cy ? CYP : shade(CKH, 0.85f);
    float step = frame == 0 ? 0 : (frame == 1 ? 3.0f : -3.0f);
    bool heavy = local == 4 || (!cy && local == 9);
    float sw = heavy ? 10.5f : 9.5f;   // shoulder half-width
    // boots, alternating in the walk cycle
    A.ell(32 + step, 32 - 4.0f, 3.6f, 2.4f, shade(dark, 1.2f), 1.4f, 0.4f);
    A.ell(32 - step, 32 + 4.0f, 3.6f, 2.4f, shade(dark, 1.2f), 1.4f, 0.4f);
    // pack
    if (cy) {
        A.box(26.5f, 32, 4.4f, heavy ? 7.2f : 5.8f, 1.6f, shade(CYM, 1.2f), 1.8f, 0.5f);
        A.line(24.6f, 32 - 3.2f, 24.6f, 32 + 3.2f, 1.1f, rgba(CYG, 210));
        if (heavy) { A.glow(25, 32, 6, rgba(CYG, 110)); }
        if (local == 9) { A.cap(25, 29, 19, 24.5f, 0.9f, CYW, 1.0f, 0.4f); A.dot(18.6f, 24.2f, 1.2f, CYG); A.glow(18.6f, 24.2f, 4, rgba(CYG, 130)); }   // sniper mast
    } else {
        A.box(26.5f, 32, 4.6f, heavy ? 7.6f : 6.2f, 1.8f, shade(CKT, 0.85f), 1.8f, 0.5f);
        A.line(27, 32 - 5.0f, 27, 32 + 5.0f, 1.0f, rgba(CKD, 150));
        if (local == 3) A.cap(24, 32 - 5, 24, 32 + 5, 1.5f, CKR, 1.2f, 0.4f);   // rocket tubes
        if (local == 9) for (int i = 0; i < 4; i++) A.cap(24.5f, 32 - 4.5f + i * 3.0f, 27.5f, 32 - 4.5f + i * 3.0f, 1.2f, i % 2 ? CKY : CKR, 1.0f, 0.4f);   // grenade bandolier
    }
    // torso
    A.ell(32.5f, 32, heavy ? 6.6f : 6.0f, sw, armor, 3.4f, 0.55f);
    if (cy) { A.line(31, 27, 31, 37, 1.0f, rgba(CYG, 170)); if (heavy) { A.ell(32.5f, 32 - sw + 1.2f, 3.6f, 3.0f, CYP, 1.6f, 0.5f); A.ell(32.5f, 32 + sw - 1.2f, 3.6f, 3.0f, CYP, 1.6f, 0.5f); } }
    else { A.line(30, 28, 30, 36, 1.0f, rgba(CKD, 140)); A.box(33.5f, 32 + 3.2f, 1.8f, 1.4f, 0.4f, shade(CKT, 0.9f), 1, 0.3f); A.box(33.5f, 32 - 3.2f, 1.8f, 1.4f, 0.4f, shade(CKT, 0.9f), 1, 0.3f); }
    // arms and weapon
    Color sleeve = shade(armor, 0.82f);
    float gx = 41;   // hands
    A.cap(33.5f, 32 - sw + 2.4f, gx, 32 - 2.2f, 2.1f, sleeve, 1.4f, 0.5f);
    A.cap(33.5f, 32 + sw - 2.4f, gx + 3, 32 + 1.2f, 2.1f, sleeve, 1.4f, 0.5f);
    if (cy) {
        if (local == 2) {        // pulse rifle
            A.cap(35, 32, 50, 32, 1.5f, shade(CYM, 1.3f), 1.2f, 0.5f);
            A.box(43, 32 + 2.8f, 1.6f, 2.0f, 0.5f, CYD, 1, 0.4f);
            A.dot(50.5f, 32, 1.3f, CYG); A.glow(51, 32, 4, rgba(CYG, 130));
        } else if (local == 3) { // laser rifle: long barrel, lens, scope
            A.cap(34, 32, 52, 32, 1.3f, CYW, 1.2f, 0.55f);
            A.box(42, 32 - 2.4f, 3, 1.1f, 0.5f, CYD, 1, 0.4f);
            A.circle(52.5f, 32, 1.9f, CYG, 1.2f, 0.4f); A.glow(53, 32, 5, rgba(CYG, 150));
            A.line(37, 31.2f, 47, 31.2f, 0.7f, rgba(CYG, 200));
        } else if (local == 9) {  // ion lance: very long emitter rail with a scope and a bright muzzle
            A.cap(33, 32, 58, 32, 1.35f, CYW, 1.2f, 0.55f);
            A.cap(36, 32, 46, 32, 2.1f, shade(CYM, 1.3f), 1.4f, 0.5f);
            A.box(41, 32 - 3.1f, 4.2f, 1.2f, 0.5f, CYD, 1, 0.4f); A.dot(45.5f, 32 - 3.1f, 1.1f, CYG);
            for (int i = 0; i < 4; i++) A.box(49 + i * 2.4f, 32, 0.5f, 2.3f, 0.2f, rgba(CYG, 220), 1, 0);
            A.circle(58.4f, 32, 1.7f, CYG, 1.2f, 0.4f); A.glow(59, 32, 6, rgba(CYG, 170));
        } else {                 // arc caster: forked emitter
            A.cap(35, 32, 47, 32, 2.2f, shade(CYM, 1.4f), 1.4f, 0.5f);
            A.cap(46, 32 - 2.6f, 54, 32 - 3.4f, 1.0f, CYW, 1, 0.4f); A.cap(46, 32 + 2.6f, 54, 32 + 3.4f, 1.0f, CYW, 1, 0.4f);
            A.glow(52, 32, 7, rgba(CYG, 170)); A.line(50, 32 - 3.0f, 53, 32, 0.9f, rgba(CYW, 230)); A.line(50, 32 + 3.0f, 53, 32, 0.9f, rgba(CYW, 230));
        }
    } else {
        if (local == 2) {        // assault rifle
            A.cap(34, 32, 40, 32, 2.0f, shade(CKR, 0.75f), 1.2f, 0.4f);           // stock/receiver
            A.cap(39, 32, 51, 32, 1.1f, CKS, 1.0f, 0.5f);
            A.box(43.5f, 32 + 3.0f, 1.3f, 2.4f, 0.5f, CKD, 1, 0.4f);              // magazine
            A.dot(51.5f, 32, 1.0f, CKD);
        } else if (local == 3) { // RPG on the shoulder
            A.cap(28, 31, 46, 31, 2.7f, shade(CKH, 1.05f), 1.8f, 0.55f);
            A.cap(46, 31, 52, 31, 3.0f, CKR, 1.4f, 0.5f);                          // warhead
            A.dot(52.5f, 31, 1.5f, shade(CKY, 0.9f));
            A.line(27, 31, 27, 31, 1, rgba(CKD, 255)); A.circle(27.5f, 31, 2.6f, CKD, 1.2f, 0.4f);   // rear cone
            A.line(38, 28.6f, 38, 33.4f, 1.0f, rgba(CKD, 160));
        } else if (local == 9) { // grenade launcher: fat short barrel, revolving drum and a stubby sight
            A.cap(33, 32, 47, 32, 3.2f, shade(CKD, 1.5f), 1.6f, 0.55f);
            A.cap(45, 32, 51, 32, 3.7f, CKS, 1.4f, 0.5f);
            A.circle(51.4f, 32, 2.0f, CKD, 1.2f, 0.4f);
            A.circle(38.5f, 32 + 4.2f, 3.6f, shade(CKS, 0.9f), 1.8f, 0.55f);
            for (int i = 0; i < 6; i++) { float a = i * 1.0472f; A.dot(38.5f + std::cos(a) * 2.0f, 32 + 4.2f + std::sin(a) * 2.0f, 0.65f, CKD); }
            A.box(40, 32 - 3.6f, 2.4f, 1.0f, 0.4f, CKD, 1, 0.3f);
        } else {                 // heavy machine gun with bipod and belt
            A.cap(35, 32, 53, 32, 1.6f, CKD, 1.2f, 0.5f);
            A.box(40, 32, 4.5f, 2.6f, 0.8f, shade(CKS, 0.9f), 1.4f, 0.5f);
            A.cap(50, 32, 53.5f, 32, 2.2f, CKD, 1.0f, 0.3f);                       // flash hider
            A.line(48, 32, 52, 27.5f, 1.0f, CKD); A.line(48, 32, 52, 36.5f, 1.0f, CKD);   // bipod
            A.box(40, 32 + 4.6f, 2.6f, 2.2f, 0.6f, shade(CKT, 0.8f), 1.2f, 0.4f);   // ammo box
            A.cap(41, 32 + 3.0f, 38, 32 + 5.5f, 0.7f, CKY, 1, 0.3f);
        }
    }
    // head
    A.circle(33.5f, 32, heavy ? 5.2f : 4.7f, helmet, 3.0f, 0.65f);
    if (cy) { A.box(36.8f, 32, 1.6f, 2.8f, 0.8f, CYG, 1, 0.2f); A.glow(37.5f, 32, 4, rgba(CYG, 90)); }
    else { A.line(34.5f, 32 - 3.2f, 34.5f, 32 + 3.2f, 1.1f, rgba(CKD, 200)); A.dot(31.5f, 32, 1.0f, rgba(CKD, 140)); }
    teamDot(A, 32.0f, 32, heavy ? 2.3f : 2.1f);            // helmet crest
}

// ---------------------------------------------------------------- vehicles
// Undercarriage of the two dozers: a steel track band whose shoe bars and sprocket teeth turn with the phase, four road wheels in its
// outer half (the hull and the track guard cover the inner half, as on the real machines), a toothed sprocket at the rear and a big
// idler at the front. s is -1 for the top track and +1 for the bottom one.
void dozerTrack(Art& A, float cx, float cy, float hl, float hh, float phase, Color shoe, float s) {
    Color steel = shade(shoe, 1.7f);
    A.box(cx, cy, hl, hh, hh * 0.7f, shoe, 1.2f, 0.4f);
    for (float x = cx - hl + 1.6f + phase * 3.0f; x < cx + hl - 1.6f; x += 3.0f) A.line(x, cy - hh + 0.7f, x, cy + hh - 0.7f, 0.7f, rgba(shade(shoe, 2.4f), 150));   // shoe bars
    float sx = cx - hl + hh * 0.85f, ix = cx + hl - hh * 0.85f;   // sprocket (rear) and idler (front)
    A.circle(sx, cy, hh * 0.8f, steel, 1.0f, 0.45f); A.circle(ix, cy, hh * 0.8f, steel, 1.0f, 0.45f);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f + phase * 1.0472f; A.dot(sx + std::cos(a) * hh * 0.45f, cy + std::sin(a) * hh * 0.45f, 0.5f, shade(shoe, 0.7f)); }   // sprocket teeth
    A.dot(ix, cy, hh * 0.3f, shade(shoe, 0.8f));   // idler hub
    for (int i = 0; i < 4; i++) { float x = cx - 6.6f + i * 4.4f, y = cy + s * hh * 0.5f; A.circle(x, y, 1.9f, steel, 0.9f, 0.5f); A.dot(x, y, 0.6f, shade(shoe, 0.8f)); }   // road wheels and hubs
}

// A bowed band as an exact distance field: the annulus between the cutting-face arc (radius ro) and the inner arc (ri) about (bx, 32),
// cut square at the two ends (angles -th..th). poly() fills the convex hull of its corners, which is wrong for a curved strip.
void bowBand(Art& A, float bx, float ro, float ri, float th, Color base, float bevel, float kk) {
    float cx = A.X(bx), cy = A.Y(32), k = A.k, R = ro * k, r0 = ri * k, mid = 0.5f * (R + r0);
    auto sdf = [=](float x, float y) {
        float dx = x - cx, dy = y - cy, r = std::sqrt(dx * dx + dy * dy), ang = std::atan2(dy, dx);
        return std::max(std::max(r - R, r0 - r), (std::fabs(ang) - th) * mid);
    };
    A.shape((int)(cx - R - 3), (int)(cy - R - 3), (int)(cx + R + 3), (int)(cy + R + 3), sdf, base, bevel * k, kk, 1.0f);
}
// One slanted hazard stripe across that band: outer arc from a to a+w, inner arc skewed forward by sk
std::vector<P> hazardStripe(float cx, float cy, float ro, float ri, float a, float w, float sk) {
    return { {cx + std::cos(a) * ro, cy + std::sin(a) * ro}, {cx + std::cos(a + w) * ro, cy + std::sin(a + w) * ro},
             {cx + std::cos(a + w + sk) * ri, cy + std::sin(a + w + sk) * ri}, {cx + std::cos(a + sk) * ri, cy + std::sin(a + sk) * ri} };
}

// The dozer blade and its push arms (both armies): a bowed moldboard on two push arms, lift rams to the hood, a mount bracket behind the
// hood nose, panel seams and bolts on the steel cutting edge. Clanker blades carry hazard stripes on the band, under the cutting edge.
void dozerBlade(Art& A, float bx, Color arm, Color band, Color edge, Color ram, bool hazard, Color stripe) {
    const float RO = 24.4f, RI = 19.6f, TH = 0.66f;
    A.cap(40.5f, 23.0f, 48.2f, 25.2f, 1.5f, arm, 1.2f, 0.5f); A.cap(40.5f, 41.0f, 48.2f, 38.8f, 1.5f, arm, 1.2f, 0.5f);   // push arms
    A.cap(43.0f, 26.6f, 49.2f, 27.4f, 0.9f, ram, 0.9f, 0.5f); A.cap(43.0f, 37.4f, 49.2f, 36.6f, 0.9f, ram, 0.9f, 0.5f);   // lift rams
    A.cap(47.0f, 32, bx + RI - 0.4f, 32, 1.0f, ram, 0.8f, 0.4f);   // mount bracket
    bowBand(A, bx, RO, RI, TH, band, 1.6f, 0.55f);   // moldboard
    if (hazard) for (int k = 0; k < 5; k++) A.poly(hazardStripe(bx, 32, RO - 0.2f, RI + 0.2f, -0.55f + k * 0.28f, 0.14f, 0.1f), stripe, 0.6f, 0.3f);
    for (int s = -1; s <= 1; s += 2) A.line(bx + std::cos(0.3f * s) * RI, 32 + std::sin(0.3f * s) * RI, bx + std::cos(0.3f * s) * (RO - 1.4f), 32 + std::sin(0.3f * s) * (RO - 1.4f), 0.5f, rgba(shade(band, 0.55f), 170));   // panel seams
    bowBand(A, bx, RO, RO - 1.4f, TH, edge, 0.8f, 0.6f);   // steel cutting edge
    for (int i = 0; i < 5; i++) { float a = -0.5f + i * 0.25f; A.dot(bx + std::cos(a) * (RO - 0.7f), 32 + std::sin(a) * (RO - 0.7f), 0.5f, shade(edge, 1.4f)); }   // edge bolts
}

// A sealed supply container seen from above: roof ribs, a rear door seam with two handles, corner castings
void supplyContainer(Art& A, float x0, float y0, float x1, float y1, Color body) {
    float cx = (x0 + x1) * 0.5f, cy = (y0 + y1) * 0.5f, hw = (x1 - x0) * 0.5f, hh = (y1 - y0) * 0.5f;
    A.box(cx, cy, hw, hh, 0.8f, body, 1.6f, 0.55f);
    for (int i = 1; i < 4; i++) { float y = y0 + (y1 - y0) * i / 4.0f; A.line(x0 + 3.0f, y, x1 - 1.4f, y, 0.5f, rgba(shade(body, 0.6f), 130)); }   // roof ribs
    A.line(x0 + 2.2f, y0 + 0.9f, x0 + 2.2f, y1 - 0.9f, 0.6f, rgba(shade(body, 0.55f), 200));   // rear door seam
    for (int s = -1; s <= 1; s += 2) A.box(x0 + 1.2f, cy + s * 2.4f, 0.9f, 0.8f, 0.3f, shade(body, 0.45f), 0.6f, 0.3f);   // door handles
    for (int s = -1; s <= 1; s += 2) for (int t = -1; t <= 1; t += 2) A.dot(cx + t * (hw - 0.9f), cy + s * (hh - 0.9f), 0.5f, shade(body, 0.5f));   // corner castings
}

// Six wheels in three axles (one per side) of the 6x6 haulers: the tyres stick out past the deck, a mudguard plate covers the inner
// half of each, and the tread bars slide with the phase so the wheels turn in the animation. reach = the tyre's outer edge from the centre line.
void truckWheels(Art& A, float ph, Color tyre, Color guard, Color tread, float reach) {
    const float axle[3] = { 19.0f, 33.5f, 48.5f };
    for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2) {
        float x = axle[i], y = 32 + s * (reach - 2.2f);
        A.box(x, y, 3.4f, 2.2f, 1.0f, tyre, 1.2f, 0.4f);
        for (float t = x - 3.4f + ph * 2.0f; t < x + 3.4f; t += 2.0f) A.line(t, y - 2.0f, t, y + 2.0f, 0.5f, rgba(tread, 170));   // tread bars
        A.box(x, 32 + s * (reach - 3.6f), 3.9f, 1.4f, 0.6f, guard, 1.0f, 0.5f);   // mudguard over the inner half
    }
}

void dozerCyber(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    dozerTrack(A, 30, 21.2f, 17, 5.2f, ph, CYD, -1); dozerTrack(A, 30, 42.8f, 17, 5.2f, ph, CYD, 1);
    // ripper: a crossbeam across the back with one steel tooth
    A.cap(15.6f, 21.5f, 15.6f, 42.5f, 0.9f, CYM, 1.0f, 0.5f);
    A.cap(15.6f, 26.0f, 14.0f, 28.0f, 0.9f, CYM, 1.0f, 0.5f); A.cap(15.6f, 38.0f, 14.0f, 36.0f, 0.9f, CYM, 1.0f, 0.5f);
    A.poly({ {14.6f, 28.0f}, {13.4f, 31.6f}, {13.4f, 32.4f}, {14.6f, 36.0f} }, shade(CYP, 0.85f), 0.8f, 0.5f);
    // hull: blue-grey composite chassis between the tracks, bolt rows along both skirts
    A.box(29.5f, 32, 13.8f, 9.6f, 2.4f, CYH, 2.6f, 0.55f);
    rivets(A, 17.5f, 24.2f, 39.5f, 24.2f, 8, shade(CYP, 1.2f)); rivets(A, 17.5f, 39.8f, 39.5f, 39.8f, 8, shade(CYP, 1.2f));
    // rear housing: hydraulic and fuel block with louvres (the team roundel sits on it)
    A.box(21.5f, 32, 5.4f, 7.6f, 1.6f, CYM, 2.0f, 0.5f);
    for (int i = 0; i < 3; i++) A.line(23.2f + i * 1.2f, 25.6f, 23.2f + i * 1.2f, 38.4f, 0.6f, rgba(CYD, 170));
    // engine hood at the front: radiator grille on the nose, exhaust stack on the rear corner
    A.box(44.0f, 32, 3.4f, 3.8f, 1.6f, shade(CYP, 0.95f), 1.8f, 0.55f);
    for (int i = 0; i < 4; i++) A.line(45.4f, 29.4f + i * 1.8f, 46.6f, 29.4f + i * 1.8f, 0.5f, rgba(CYD, 200));
    A.circle(41.2f, 29.4f, 1.2f, shade(CYM, 1.3f), 1.0f, 0.5f); A.dot(41.2f, 29.4f, 0.5f, CYD);
    // operator's cab: composite roof, armoured glass windscreen, roll-over cage behind it, amber warning lights on the front corners
    A.box(34.0f, 32, 6.6f, 7.2f, 2.0f, shade(CYP, 1.0f), 2.2f, 0.55f);
    A.box(38.6f, 32, 1.5f, 5.2f, 0.8f, shade(CYG, 0.5f), 1.2f, 0.5f);
    A.line(37.8f, 27.6f, 39.4f, 28.4f, 0.6f, rgba(CYW, 200));
    A.line(28.6f, 25.7f, 36.4f, 25.7f, 0.7f, rgba(CYD, 220)); A.line(28.6f, 38.3f, 36.4f, 38.3f, 0.7f, rgba(CYD, 220)); A.line(28.6f, 25.7f, 28.6f, 38.3f, 0.7f, rgba(CYD, 220));
    for (int s = -1; s <= 1; s += 2) {
        A.dot(28.6f, 32 + s * 6.3f, 0.8f, shade(CYD, 1.3f));
        A.dot(39.2f, 32 + s * 6.4f, 0.8f, rgb(255, 170, 60)); A.glow(39.2f, 32 + s * 6.4f, 2.4f, rgba(rgb(255, 170, 60), 70));
    }
    A.line(28.2f, 24.6f, 39.4f, 24.6f, 0.5f, rgba(CYG, 170));   // subtle cyan running light along the cab
    dozerBlade(A, 31.9f, CYM, CYP, shade(CYM, 1.2f), shade(CYD, 1.6f), false, CYD);
    A.grain(10, 12, 58, 52, 0.06f, 93);   // faint tone shift on the composite
    teamDot(A, 19.5f, 32, 3.0f);
}
void dozerClanker(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    dozerTrack(A, 29, 20.5f, 18, 5.5f, ph, CKD, -1); dozerTrack(A, 29, 43.5f, 18, 5.5f, ph, CKD, 1);
    // ripper: a crossbeam across the back with one steel tooth
    A.cap(14.4f, 21.5f, 14.4f, 42.5f, 0.9f, CKS, 1.0f, 0.5f);
    A.cap(14.4f, 26.0f, 12.6f, 28.0f, 0.9f, CKS, 1.0f, 0.5f); A.cap(14.4f, 38.0f, 12.6f, 36.0f, 0.9f, CKS, 1.0f, 0.5f);
    A.poly({ {12.6f, 28.0f}, {10.6f, 31.4f}, {10.6f, 32.6f}, {12.6f, 36.0f} }, shade(CKS, 0.9f), 0.8f, 0.5f);
    // hull: welded yellow chassis between the tracks, bolt rows along both skirts
    A.box(28.5f, 32, 13.5f, 9.8f, 2.2f, CKY, 2.4f, 0.5f);
    rivets(A, 17.0f, 23.2f, 39.0f, 23.2f, 8, shade(CKY, 1.25f)); rivets(A, 17.0f, 40.8f, 39.0f, 40.8f, 8, shade(CKY, 1.25f));
    // rear housing: fuel and hydraulic block with louvres (the team roundel sits on it)
    A.box(20.5f, 32, 5.2f, 7.6f, 1.2f, shade(CKY, 0.8f), 2.0f, 0.45f);
    for (int i = 0; i < 3; i++) A.line(23.0f + i * 1.2f, 25.4f, 23.0f + i * 1.2f, 38.6f, 0.6f, rgba(CKD, 170));
    // engine hood at the front: radiator grille on the nose, exhaust stack on the rear corner
    A.box(43.4f, 32, 3.2f, 3.8f, 1.2f, shade(CKY, 0.95f), 1.8f, 0.5f);
    for (int i = 0; i < 4; i++) A.line(45.4f, 29.4f + i * 1.8f, 46.4f, 29.4f + i * 1.8f, 0.5f, rgba(CKD, 200));
    A.circle(41.2f, 29.4f, 1.2f, CKS, 1.0f, 0.5f); A.dot(41.2f, 29.4f, 0.5f, CKD);
    // operator's cab: olive steel roof, dark glass windscreen, roll-over cage behind it, amber warning lights on the front corners
    A.box(33.8f, 32, 6.4f, 7.2f, 1.8f, CKH, 2.2f, 0.5f);
    A.box(38.4f, 32, 1.5f, 5.4f, 0.7f, rgb(64, 88, 100), 1.2f, 0.5f);
    A.line(37.6f, 27.4f, 39.4f, 28.2f, 0.6f, rgba(rgb(200, 220, 230), 170));
    hatch(A, 30.4f, 30.4f, 2.0f, shade(CKH, 1.12f));
    A.line(28.4f, 25.6f, 36.4f, 25.6f, 0.8f, rgba(CKD, 220)); A.line(28.4f, 38.4f, 36.4f, 38.4f, 0.8f, rgba(CKD, 220)); A.line(28.4f, 25.6f, 28.4f, 38.4f, 0.8f, rgba(CKD, 220));
    for (int s = -1; s <= 1; s += 2) {
        A.dot(28.4f, 32 + s * 6.4f, 0.8f, shade(CKD, 1.3f));
        A.dot(39.2f, 32 + s * 6.4f, 0.8f, rgb(255, 170, 40)); A.glow(39.2f, 32 + s * 6.4f, 2.4f, rgba(rgb(255, 170, 40), 70));
    }
    dozerBlade(A, 31.0f, CKS, CKY, shade(CKS, 1.1f), shade(CKD, 1.5f), true, CKD);
    weatherClanker(A, 91, 10, 12, 58, 52);
    teamDot(A, 17.5f, 32, 3.0f);
}

void harvCyber(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    truckWheels(A, ph, shade(CYD, 1.2f), shade(CYH, 1.12f), shade(CYD, 0.6f), 15.8f);
    A.box(24.5f, 32, 17.5f, 11.0f, 1.6f, shade(CYH, 0.92f), 2.4f, 0.5f);   // flatbed deck
    A.line(7.5f, 21.6f, 41.5f, 21.6f, 0.7f, rgba(shade(CYH, 0.6f), 180)); A.line(7.5f, 42.4f, 41.5f, 42.4f, 0.7f, rgba(shade(CYH, 0.6f), 180));   // side rails
    Color grey = mix(CYP, rgb(134, 152, 170), 0.45f);   // sealed grey-blue supply containers
    supplyContainer(A, 12.6f, 22.2f, 39.6f, 30.6f, grey);
    supplyContainer(A, 12.6f, 33.4f, 39.6f, 41.8f, grey);
    // folding crane: pedestal, boom laid across the gap between the containers, hook at its end
    A.circle(35.6f, 32, 2.0f, shade(CYM, 1.3f), 1.2f, 0.55f);
    A.cap(35.6f, 32, 15.0f, 32, 0.8f, shade(CYP, 1.3f), 0.9f, 0.5f);
    A.ring(15.0f, 32, 1.0f, 0.5f, shade(CYD, 1.6f), 0.5f, 0.3f);
    // spare wheel behind the cab
    A.ring(41.2f, 32, 2.0f, 1.2f, shade(CYD, 1.3f), 1.0f, 0.4f); A.dot(41.2f, 32, 0.8f, shade(CYP, 1.0f));
    // cab: composite roof, armoured glass windscreen, roof hatch, mirrors, cyan running light and amber marker lights
    A.box(50.2f, 32, 6.8f, 9.2f, 2.6f, shade(CYP, 0.95f), 2.4f, 0.55f);
    A.box(54.6f, 32, 1.5f, 6.8f, 0.8f, shade(CYG, 0.5f), 1.2f, 0.5f);
    A.line(53.4f, 26.2f, 55.4f, 27.4f, 0.6f, rgba(CYW, 200));
    A.line(44.6f, 23.6f, 55.0f, 23.6f, 0.5f, rgba(CYG, 170));
    hatch(A, 46.8f, 32, 2.0f, shade(CYP, 1.1f));
    for (int s = -1; s <= 1; s += 2) { A.cap(55.0f, 32 + s * 9.4f, 55.0f, 32 + s * 11.8f, 0.5f, CYD, 0.6f, 0.3f); A.box(55.0f, 32 + s * 12.8f, 1.3f, 0.9f, 0.4f, CYD, 0.6f, 0.3f); }
    A.dot(56.2f, 25.2f, 0.8f, rgb(255, 170, 60)); A.dot(56.2f, 38.8f, 0.8f, rgb(255, 170, 60));
    A.dot(6.9f, 25.4f, 0.8f, rgb(214, 54, 44)); A.dot(6.9f, 38.6f, 0.8f, rgb(214, 54, 44));   // rear lamps
    teamDot(A, 10.5f, 32, 2.8f);
}
void harvClanker(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    truckWheels(A, ph, CKD, shade(CKH, 1.1f), shade(CKD, 0.6f), 16.2f);
    A.box(24.5f, 32, 17.5f, 11.2f, 1.2f, shade(CKH, 0.92f), 2.2f, 0.5f);   // flatbed in earth brown
    A.line(7.5f, 21.2f, 41.5f, 21.2f, 0.7f, rgba(CKD, 170)); A.line(7.5f, 42.8f, 41.5f, 42.8f, 0.7f, rgba(CKD, 170));   // sideboards
    for (int i = 0; i < 6; i++) A.line(8.0f, 23.0f + i * 3.2f, 42.0f, 23.0f + i * 3.2f, 0.4f, rgba(CKD, 80));   // deck planks
    // wooden crates, two rows of three, at the rear of the deck
    for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) {
        float x = 15.6f + i * 5.4f, y = 26.4f + j * 11.2f;
        A.box(x, y, 2.6f, 4.0f, 0.5f, shade(rgb(154, 118, 70), 0.9f + ((i + j * 3) % 3) * 0.07f), 1.4f, 0.6f);
        A.line(x - 2.2f, y - 1.6f, x + 2.2f, y - 1.6f, 0.4f, rgba(CKD, 150)); A.line(x - 2.2f, y + 1.6f, x + 2.2f, y + 1.6f, 0.4f, rgba(CKD, 150));
    }
    // tarp tied down over the middle load, with a rope lashing to the sideboards
    A.box(32.6f, 32, 3.0f, 10.4f, 0.8f, rgb(104, 108, 86), 1.6f, 0.45f);
    A.line(29.6f, 22.4f, 35.6f, 41.6f, 0.6f, rgba(CKD, 130)); A.line(35.6f, 22.4f, 29.6f, 41.6f, 0.6f, rgba(CKD, 130));
    for (int i = 0; i < 3; i++) { A.dot(30.6f + i * 2.5f, 21.9f, 0.5f, CKS); A.dot(30.6f + i * 2.5f, 42.1f, 0.5f, CKS); }   // tie-down points
    // oil drums at the front of the deck
    Color drum = rgb(70, 92, 110);
    for (int i = 0; i < 3; i++) { float y = 25.0f + i * 7.0f; A.circle(36.9f, y, 2.3f, drum, 1.6f, 0.6f); A.ring(36.9f, y, 1.2f, 0.5f, shade(drum, 1.5f), 0.5f, 0.3f); }
    // cab: rust-brown armoured cab, dark glass, roof hatch, mirrors, headlamps
    A.box(50.4f, 32, 6.6f, 9.4f, 2.2f, CKR, 2.2f, 0.55f);
    A.box(49.4f, 32, 4.6f, 7.4f, 1.4f, shade(CKR, 1.06f), 1.4f, 0.5f);
    A.box(54.6f, 32, 1.5f, 6.4f, 0.6f, rgb(72, 96, 108), 1.2f, 0.5f);
    hatch(A, 46.0f, 32, 2.0f, shade(CKR, 0.8f));
    for (int s = -1; s <= 1; s += 2) { A.cap(55.0f, 32 + s * 9.4f, 55.0f, 32 + s * 11.8f, 0.5f, CKS, 0.6f, 0.3f); A.box(55.0f, 32 + s * 12.8f, 1.3f, 0.9f, 0.4f, CKS, 0.6f, 0.3f); }
    A.dot(56.2f, 25.4f, 0.8f, rgb(255, 240, 190)); A.dot(56.2f, 38.6f, 0.8f, rgb(255, 240, 190));
    A.dot(6.9f, 25.4f, 0.8f, rgb(214, 54, 44)); A.dot(6.9f, 38.6f, 0.8f, rgb(214, 54, 44));   // rear lamps
    weatherClanker(A, 17, 8, 12, 60, 52);   // dirt and rust over the deck and cab
    teamDot(A, 10.0f, 32, 2.8f);
}

void medicBody(Art& A, bool cy, int frame) {
    Color hull = cy ? rgb(226, 236, 240) : rgb(222, 218, 196), trim = cy ? rgb(70, 190, 210) : rgb(150, 150, 96), cross = cy ? rgb(40, 220, 170) : rgb(220, 60, 52);
    Color dark = cy ? CYD : CKD;
    for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) A.box(19 + i * 9.5f, 32 + s * 13.6f, 3.9f, 2.8f, 1.2f, dark, 1.4f, 0.4f);
    A.box(26, 32, 20, 13, 2.0f, hull, 2.4f, 0.55f);                                      // box body
    A.box(26, 32, 17, 10, 1.0f, shade(hull, 0.93f), 1.2f, 0.4f);
    A.box(26, 32, 3.0f, 9.0f, 0.8f, cross, 1.4f, 0.55f); A.box(26, 32, 9.0f, 3.0f, 0.8f, cross, 1.4f, 0.55f);   // the cross
    A.box(49, 32, 8.5f, 10.2f, 2, trim, 2.4f, 0.55f);                                    // cab
    A.box(52.5f, 32, 3.2f, 7.6f, 1, rgb(80, 110, 124), 1.2f, 0.5f);
    A.box(16, 32, 2.0f, 13.5f, 1.0f, shade(trim, 0.8f), 1.2f, 0.5f);
    A.dot(56, 27.4f, 1.0f, rgb(255, 245, 200)); A.dot(56, 36.6f, 1.0f, rgb(255, 245, 200));
    float pulse = frame == 1 ? 1.0f : 0.5f;
    A.glow(44, 32, 7 * pulse + 3, rgba(cross, 130)); A.dot(44, 28, 1.3f, cross); A.dot(44, 36, 1.3f, cross);    // beacon bar
    teamDot(A, 10.0f, 32, 2.8f);
}

// Sniper: the ordinary soldier body with a hooded cloak, a very long rifle with a scope, and a bipod
void sniperBody(Art& A, bool cy, int frame) {
    soldier(A, cy, 2, frame);
    Color cloak = cy ? rgb(40, 70, 74) : rgb(78, 84, 52), leaf = cy ? rgb(70, 120, 120) : rgb(112, 120, 70), dark = cy ? CYD : CKD;
    A.ell(30.5f, 32, 8.2f, 10.5f, cloak, 2.6f, 0.5f);                                   // cloak over the shoulders and pack
    for (int i = 0; i < 7; i++) A.ell(25.5f + (i % 3) * 3.2f, 32 - 8.5f + i * 2.8f, 1.8f, 1.1f, i % 2 ? leaf : shade(leaf, 0.8f), 1.0f, 0.3f);
    A.ell(33, 32, 4.6f, 5.4f, shade(cloak, 1.1f), 2.0f, 0.5f);                          // hood
    A.cap(34, 32, 60, 32, 1.05f, cy ? CYW : shade(CKD, 1.7f), 1.2f, 0.55f);             // long barrel
    A.cap(36, 32, 44, 32, 1.9f, cy ? shade(CYM, 1.3f) : shade(CKR, 0.85f), 1.2f, 0.4f); // stock and receiver
    A.box(43, 32 - 2.6f, 5.2f, 1.5f, 0.6f, dark, 1.2f, 0.5f); A.dot(47.4f, 32 - 2.6f, 1.0f, cy ? CYG : rgb(255, 214, 120));   // scope
    A.line(50, 32, 54, 29, 0.7f, rgba(dark, 220)); A.line(50, 32, 54, 35, 0.7f, rgba(dark, 220));   // bipod
    if (cy) { A.dot(60.4f, 32, 1.2f, CYG); A.glow(60.8f, 32, 4, rgba(CYG, 120)); }
    teamDot(A, 31.0f, 32, 2.4f);
}

// Tank track (both armies): a rubber band with cross-bar grousers that slide with the phase, road-wheel hubs in its outer half
// (the skirts cover the inner half, as on the real vehicles), a toothed sprocket at the rear and an idler at the front.
// s is -1 for the top run and +1 for the bottom run.
void tankTrack(Art& A, float cx, float cy, float hl, float hh, float phase, Color rubber, float s) {
    Color steel = shade(rubber, 1.6f);
    A.box(cx, cy, hl, hh, hh * 0.85f, rubber, 1.0f, 0.35f);
    for (float x = cx - hl + 1.6f + phase * 2.4f; x < cx + hl - 1.6f; x += 2.4f) A.line(x, cy - hh + 0.6f, x, cy + hh - 0.6f, 0.6f, rgba(shade(rubber, 2.4f), 120));
    float sx = cx - hl + hh * 0.9f, ix = cx + hl - hh * 0.9f;   // sprocket (rear) and idler (front)
    A.circle(sx, cy, hh * 0.85f, steel, 1.0f, 0.45f); A.circle(ix, cy, hh * 0.85f, steel, 1.0f, 0.45f);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f + phase * 1.0472f; A.dot(sx + std::cos(a) * hh * 0.55f, cy + std::sin(a) * hh * 0.55f, 0.5f, rgba(shade(rubber, 0.7f), 220)); }   // sprocket teeth
    A.dot(ix, cy, hh * 0.3f, shade(rubber, 0.8f));
    for (int i = 0; i < 7; i++) A.circle(cx + (i - 3) * 4.4f, cy + s * hh * 0.5f, 1.25f, steel, 0.6f, 0.45f);   // road-wheel hubs
}

void tankCyber(Art& A, int frame) {   // Photon Tank body: blue-grey composite hull, armoured skirts, sloped glacis and an engine deck
    float ph = frame == 2 ? 0.5f : 0;
    Color comp = mix(CYH, rgb(92, 104, 120), 0.7f), panel = shade(comp, 1.14f);   // the cobalt tone greyed down into composite
    tankTrack(A, 30, 15.4f, 21, 4.0f, ph, CYD, -1); tankTrack(A, 30, 48.6f, 21, 4.0f, ph, CYD, 1);
    A.poly({ {11, 20.5f}, {40, 20.5f}, {49.5f, 25.2f}, {52.5f, 28.4f}, {52.5f, 35.6f}, {49.5f, 38.8f}, {40, 43.5f}, {11, 43.5f} }, comp, 2.6f, 0.6f);   // chassis
    A.poly({ {40.5f, 22.4f}, {48.6f, 25.6f}, {51.3f, 28.5f}, {51.3f, 35.5f}, {48.6f, 38.4f}, {40.5f, 41.6f} }, panel, 1.4f, 0.5f);   // glacis plate
    A.line(40.5f, 22.4f, 48.6f, 25.6f, 0.6f, rgba(CYD, 170)); A.line(40.5f, 41.6f, 48.6f, 38.4f, 0.6f, rgba(CYD, 170));   // glacis seams
    A.box(28.5f, 18.7f, 16.5f, 2.1f, 0.6f, panel, 1.0f, 0.35f);   // upper side skirt, covering the inner half of the track
    A.box(28.5f, 45.3f, 16.5f, 2.1f, 0.6f, panel, 1.0f, 0.35f);   // lower side skirt
    A.box(16.6f, 32, 5.2f, 9.6f, 1.2f, shade(CYM, 1.15f), 1.8f, 0.5f);   // engine deck
    A.ring(32, 32, 10.6f, 1.2f, shade(CYD, 1.3f), 0.6f, 0.3f);   // turret ring: the bearing the turret sits on
}
void tankClanker(Art& A, int frame) {   // Brute Tank body: olive hull, bolted skirts, sloped glacis and a louvred rear deck
    float ph = frame == 2 ? 0.5f : 0;
    tankTrack(A, 30, 15.8f, 22, 4.2f, ph, CKD, -1); tankTrack(A, 30, 48.2f, 22, 4.2f, ph, CKD, 1);
    A.poly({ {11, 20.5f}, {40, 20.5f}, {49.5f, 25.0f}, {52.5f, 28.0f}, {52.5f, 36.0f}, {49.5f, 39.0f}, {40, 43.5f}, {11, 43.5f} }, CKH, 2.8f, 0.6f);   // hull
    A.poly({ {40.5f, 22.0f}, {48.4f, 25.0f}, {51.0f, 28.0f}, {51.0f, 36.0f}, {48.4f, 39.0f}, {40.5f, 42.0f} }, shade(CKH, 1.08f), 1.4f, 0.5f);   // glacis plate
    A.box(28.5f, 18.5f, 16.5f, 2.1f, 0.5f, shade(CKH, 0.9f), 1.4f, 0.5f);   // upper side skirt
    A.box(28.5f, 45.5f, 16.5f, 2.1f, 0.5f, shade(CKH, 0.9f), 1.4f, 0.5f);   // lower side skirt
    A.box(16.0f, 32, 4.6f, 9.0f, 1.2f, shade(CKD, 1.25f), 2, 0.5f);   // rear deck
    A.ring(32, 32, 10.6f, 1.2f, shade(CKD, 1.3f), 0.6f, 0.3f);   // turret ring
    weatherClanker(A, 33, 8, 14, 56, 50);
}

void walkerCyber(Art& A, int frame) {   // Volt Walker: four articulated legs
    float sw = frame == 0 ? 0 : (frame == 1 ? 3.2f : -3.2f);
    struct Leg { float hx, hy, kx, ky, fx, fy; };
    Leg legs[4] = {
        { 38, 25, 44 + sw, 17, 47 + sw, 11 }, { 38, 39, 44 - sw, 47, 47 - sw, 53 },
        { 26, 25, 21 - sw, 16, 17 - sw, 12 }, { 26, 39, 21 + sw, 48, 17 + sw, 52 } };
    for (auto& l : legs) {
        A.cap(l.hx, l.hy, l.kx, l.ky, 2.7f, shade(CYM, 1.05f), 1.8f, 0.5f);
        A.cap(l.kx, l.ky, l.fx, l.fy, 2.1f, shade(CYD, 1.3f), 1.6f, 0.5f);
        A.circle(l.kx, l.ky, 2.9f, shade(CYP, 0.85f), 1.8f, 0.55f);
        A.circle(l.fx, l.fy, 3.0f, shade(CYD, 1.5f), 1.6f, 0.5f);
    }
    A.circle(32, 32, 13.5f, CYH, 3.4f, 0.6f);
    A.circle(32, 32, 10, shade(CYH, 0.82f), 2.4f, 0.5f);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f; A.line(32 + std::cos(a) * 8.2f, 32 + std::sin(a) * 8.2f, 32 + std::cos(a) * 11.6f, 32 + std::sin(a) * 11.6f, 1.1f, rgba(CYD, 200)); }
    for (int i = 0; i < 16; i++) { float a = i * 0.3927f; A.dot(32 + std::cos(a) * 12.6f, 32 + std::sin(a) * 12.6f, 0.8f, rgba(CYG, 190)); }
}
// Composite blue-grey of the railgun tank: the graphite Cyber tone pushed toward steel blue (shared by its hull, turret and detail)
Color railComp() { return mix(CYH, rgb(92, 110, 134), 0.6f); }

void tankCyberRail(Art& A, int frame) {   // Railgun Tank body: a long, low tank destroyer on wide tracks, blue-grey composite hull with armoured skirts and a sloped glacis
    float ph = frame == 2 ? 0.5f : 0;
    const Color comp = railComp(), panel = shade(comp, 1.16f), seam = shade(comp, 0.6f);
    tankTrack(A, 30, 16.5f, 24, 5.0f, ph, CYD, -1); tankTrack(A, 30, 47.5f, 24, 5.0f, ph, CYD, 1);
    A.poly({ {7, 20.5f}, {42, 20.5f}, {55, 26.5f}, {55, 37.5f}, {42, 43.5f}, {7, 43.5f} }, comp, 2.6f, 0.6f);   // hull
    A.poly({ {42.5f, 22.2f}, {52.4f, 26.6f}, {53.8f, 32}, {52.4f, 37.4f}, {42.5f, 41.8f} }, panel, 1.4f, 0.5f);   // glacis plate
    A.line(42.5f, 22.2f, 52.4f, 26.6f, 0.6f, rgba(seam, 200)); A.line(42.5f, 41.8f, 52.4f, 37.4f, 0.6f, rgba(seam, 200));   // glacis seams
    A.box(28.5f, 18.2f, 17.5f, 2.0f, 0.5f, panel, 1.1f, 0.4f); A.box(28.5f, 45.8f, 17.5f, 2.0f, 0.5f, panel, 1.1f, 0.4f);   // armoured skirts over the inner half of the tracks
    A.box(12.0f, 32, 5.0f, 9.0f, 1.0f, shade(comp, 0.86f), 1.4f, 0.4f);   // rear engine deck
    A.ring(32, 32, 10.6f, 1.2f, shade(CYD, 1.3f), 0.6f, 0.3f);   // turret ring
}
void gatlingBody(Art& A, int frame) {   // Gatling Tank body (2S6 / ZSU-23-4 family): olive-drab tracked hull, skirts over the tracks, a sloped glacis and a slatted engine deck
    float ph = frame == 2 ? 0.5f : 0;
    const Color olive = rgb(92, 100, 64), olD = rgb(44, 48, 32), skirt = rgb(74, 80, 52);
    tankTrack(A, 31, 17, 20, 4.9f, ph, CKD, -1); tankTrack(A, 31, 47, 20, 4.9f, ph, CKD, 1);
    A.poly({ {11, 21.5f}, {40, 21.5f}, {51, 27}, {51, 37}, {40, 42.5f}, {11, 42.5f} }, olive, 2.6f, 0.62f);   // hull
    A.poly({ {40.5f, 23.2f}, {48.4f, 27.0f}, {49.8f, 32}, {48.4f, 37.0f}, {40.5f, 40.8f} }, shade(olive, 1.08f), 1.4f, 0.5f);   // glacis plate
    A.line(40.5f, 23.2f, 48.4f, 27.0f, 0.6f, rgba(olD, 170)); A.line(40.5f, 40.8f, 48.4f, 37.0f, 0.6f, rgba(olD, 170));   // glacis seams
    A.box(29, 18.4f, 17.6f, 2.0f, 0.5f, skirt, 1.3f, 0.5f); A.box(29, 45.6f, 17.6f, 2.0f, 0.5f, skirt, 1.3f, 0.5f);   // side skirts
    A.box(16.0f, 32, 5.4f, 8.6f, 1.2f, shade(olD, 1.5f), 2.0f, 0.5f);   // engine deck
    for (int i = 0; i < 5; i++) A.line(12.4f + i * 1.8f, 25.0f, 12.4f + i * 1.8f, 39.0f, 0.7f, rgba(olD, 200));   // grille slats
    A.ring(32, 32, 10.6f, 1.2f, shade(olD, 1.3f), 0.6f, 0.3f);   // turret ring
    weatherClanker(A, 71, 9, 14, 56, 50);
}
void launcherBody(Art& A, int frame) {   // Rocket Launcher carrier (M270 MLRS style): tracked olive chassis, an armoured cab forward with a hazard-yellow bumper, the sand-decked turntable under the pod
    float ph = frame == 2 ? 0.5f : 0;
    const Color olive = rgb(96, 104, 68), olD = rgb(44, 48, 32), sand = rgb(170, 150, 106);
    tankTrack(A, 29, 17, 23, 5.2f, ph, CKD, -1); tankTrack(A, 29, 47, 23, 5.2f, ph, CKD, 1);
    A.box(28.5f, 18.4f, 20.5f, 2.0f, 0.5f, shade(olive, 0.88f), 1.3f, 0.5f); A.box(28.5f, 45.6f, 20.5f, 2.0f, 0.5f, shade(olive, 0.88f), 1.3f, 0.5f);   // skirts
    A.poly({ {8, 21.4f}, {42, 21.4f}, {47, 22.6f}, {47, 41.4f}, {42, 42.6f}, {8, 42.6f} }, olive, 2.6f, 0.62f);   // chassis
    A.box(49.6f, 32, 6.0f, 8.8f, 2.0f, shade(olive, 1.06f), 2.4f, 0.6f);   // cab, forward of the pod
    A.box(48.6f, 32, 3.8f, 6.2f, 1.2f, sand, 1.4f, 0.45f);                 // sand cab roof
    A.box(54.6f, 32, 0.9f, 6.4f, 0.5f, rgb(44, 62, 68), 0.9f, 0.5f);       // windscreen
    A.box(56.3f, 32, 0.6f, 9.6f, 0.3f, rgb(226, 182, 62), 0.6f, 0.3f);     // hazard-yellow bumper
    A.dot(56.3f, 27.2f, 0.7f, rgb(255, 240, 190)); A.dot(56.3f, 36.8f, 0.7f, rgb(255, 240, 190));
    for (int i = 0; i < 4; i++) A.line(9.4f + i * 1.2f, 25.0f, 9.4f + i * 1.2f, 39.0f, 0.6f, rgba(olD, 200));   // rear deck louvres
    weatherClanker(A, 5, 6, 12, 60, 52);
}

void titanCyber(Art& A, int frame) {   // Aegis Titan body: broad tracked hull, armoured shoulders, gold emitter trim
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 13.5f, 25, 6.4f, ph, CYD); trackRun(A, 30, 50.5f, 25, 6.4f, ph, CYD);
    A.poly({ {5, 19}, {40, 19}, {57, 25.5f}, {57, 38.5f}, {40, 45}, {5, 45} }, CYH, 3.0f, 0.62f);
    A.box(30, 20.6f, 22, 2.4f, 0.8f, shade(CYP, 1.0f), 1.4f, 0.5f); A.box(30, 43.4f, 22, 2.4f, 0.8f, shade(CYP, 1.0f), 1.4f, 0.5f);   // side armour plates
    A.line(8, 22.6f, 40, 22.6f, 1.2f, rgba(CYG, 235)); A.line(8, 41.4f, 40, 41.4f, 1.2f, rgba(CYG, 235));
    A.glow(24, 22.6f, 8, rgba(CYG, 60)); A.glow(24, 41.4f, 8, rgba(CYG, 60));
    A.box(14, 32, 8.5f, 10.5f, 1.6f, shade(CYM, 1.15f), 2.2f, 0.5f);                          // reactor deck
    A.circle(13, 32, 5.2f, CYD, 1.8f, 0.5f); A.circle(13, 32, 3.4f, CYG, 1.4f, 0.3f); A.glow(13, 32, 10, rgba(CYG, 120));
    for (int i = 0; i < 3; i++) A.line(19.5f + i * 2.4f, 25.5f, 19.5f + i * 2.4f, 38.5f, 0.8f, rgba(CYD, 200));
    A.line(42, 21, 53, 26.6f, 0.9f, rgba(CYD, 170)); A.line(42, 43, 53, 37.4f, 0.9f, rgba(CYD, 170));
    A.dot(54.5f, 28.5f, 1.2f, CYW); A.dot(54.5f, 35.5f, 1.2f, CYW);
    A.grain(6, 15, 58, 49, 0.06f, 9);
}
void behemothBody(Art& A, int frame) {   // Behemoth body: cast-iron slab hull on four heavy track pods, hazard rear
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 29, 13.5f, 25, 6.4f, ph, CKD); trackRun(A, 29, 50.5f, 25, 6.4f, ph, CKD);
    A.poly({ {4, 19}, {42, 19}, {57, 25}, {57, 39}, {42, 45}, {4, 45} }, CKH, 3.0f, 0.62f);
    A.box(30, 20.8f, 24, 2.8f, 0.8f, shade(CKH, 0.84f), 1.4f, 0.5f); A.box(30, 43.2f, 24, 2.8f, 0.8f, shade(CKH, 0.84f), 1.4f, 0.5f);
    rivets(A, 8, 22.4f, 50, 22.4f, 13, shade(CKH, 1.4f)); rivets(A, 8, 41.6f, 50, 41.6f, 13, shade(CKH, 1.4f));
    A.box(14.5f, 32, 8, 10.4f, 1.4f, shade(CKD, 1.3f), 2, 0.5f);
    for (int i = 0; i < 5; i++) A.line(9.8f + i * 2.4f, 25, 9.8f + i * 2.4f, 39, 0.9f, rgba(CKD, 220));
    A.circle(7.5f, 26, 2.8f, CKS, 1.6f, 0.55f); A.dot(7.5f, 26, 1.1f, CKD); A.circle(7.5f, 38, 2.8f, CKS, 1.6f, 0.55f); A.dot(7.5f, 38, 1.1f, CKD);   // exhaust stacks
    for (int i = 0; i < 5; i++) A.box(4.6f + 0, 22.5f + i * 4.7f, 1.6f, 1.9f, 0.4f, i % 2 ? CKY : CKD, 1, 0.2f);                              // hazard rear
    A.line(44, 21.5f, 53, 26.4f, 0.9f, rgba(CKD, 170)); A.line(44, 42.5f, 53, 37.6f, 0.9f, rgba(CKD, 170));
    A.dot(55, 28.6f, 1.2f, rgb(255, 240, 190)); A.dot(55, 35.4f, 1.2f, rgb(255, 240, 190));
    weatherClanker(A, 81, 6, 15, 60, 49);
}

// ---------------------------------------------------------------- anti-radiation truck

// One truck wheel seen from above: a tyre band across the axle whose tread slides by the frame phase, half a tread pitch apart between
// the two rolling frames (the same frame convention as the jeep's wheels in art_jeep.cpp). Cyber: slim tyre with pale hex nubs and a cyan
// rim line. Clanker: fat lugged tyre with mud on the outer edge.
void radWheel(Art& A, float cx, float cy, float ph, bool cyber, bool top) {
    const float hl = 5.2f, hh = 2.6f;
    Color tyre = cyber ? rgb(38, 44, 56) : rgb(40, 37, 29);
    float outerY = top ? cy - hh + 0.7f : cy + hh - 0.7f;
    A.box(cx, cy, hl, hh, 1.3f, tyre, 1.6f, 0.5f);
    A.ell(cx, cy, hl * 0.66f, hh * 0.6f, shade(tyre, 1.55f), 1.2f, 0.4f);        // rim shadow inside
    float pitch = cyber ? 2.7f : 3.6f;
    for (float x = cx - hl + 1.2f + ph * pitch; x < cx + hl - 1.0f; x += pitch) {
        if (cyber) A.dot(x, cy, 0.8f, rgb(128, 142, 160));                        // pale hex nubs
        else A.box(x, cy, 0.95f, hh - 0.7f, 0.3f, rgb(80, 72, 52), 0.7f, 0.35f);  // chunky lugs
    }
    if (cyber) {
        A.line(cx - hl + 1.5f, outerY, cx + hl - 1.5f, outerY, 0.8f, rgba(rgb(110, 232, 255), 235));
        A.glow(cx, outerY, 4.2f, rgba(rgb(110, 232, 255), 45));
    } else {
        A.ell(cx + 1.0f, outerY, 2.6f, 0.8f, rgba(rgb(70, 54, 36), 140), 1, 0);   // mud on the outer edge
    }
}

// The radiation trefoil seen from above on the scrubber roof: a hazard field disc with three dark blades round the hub, which is how
// the symbol reads as a marking at unit-sprite size.
void radTrefoil(Art& A, float cx, float cy, float r, Color field, Color blade) {
    A.circle(cx, cy, r, field, 1.4f, 0.45f);
    for (int i = 0; i < 3; i++) {
        float a = -1.5708f + i * 2.0944f;
        A.poly({{cx, cy}, {cx + std::cos(a - 0.46f) * r * 0.84f, cy + std::sin(a - 0.46f) * r * 0.84f}, {cx + std::cos(a + 0.46f) * r * 0.84f, cy + std::sin(a + 0.46f) * r * 0.84f}}, blade, 0.5f, 0.2f);
    }
    A.dot(cx, cy, r * 0.26f, blade);
}

// Anti-Radiation Truck (both armies): a six-wheel decontamination truck - boxy cab forward (+x), a long scrubber tank behind, a roof
// sprayer boom and sensor dish, side sprayer arms and rear nozzles, bolted radiation-shield skirts, exhausts and the green decon beacon
// whose light sells the unit at 1x. Cyber: white ceramic and steel with cyan emitter strips, a pale-green trefoil and a sensor dish.
// Clanker: olive over sand with hazard-yellow and decon-green bands, chunky lugged tyres, rust, a big soot-blackened stack and a yellow
// trefoil. Frames 1/2 roll (the tyre tread shifts by half a pitch).
void radTruckBody(Art& A, bool cyber, int frame) {
    const Color hull = cyber ? rgb(214, 222, 232) : rgb(102, 110, 78);
    const Color hullD = cyber ? rgb(142, 154, 172) : rgb(74, 82, 56);
    const Color deck = cyber ? rgb(236, 240, 246) : rgb(150, 142, 100);
    const Color steel = cyber ? rgb(120, 134, 152) : rgb(146, 148, 142);
    const Color dark = cyber ? rgb(42, 50, 64) : rgb(34, 34, 28);
    const Color glass = cyber ? rgb(38, 84, 120) : rgb(76, 100, 96);
    const Color trim = cyber ? rgb(110, 232, 255) : rgb(234, 188, 60);        // faction light: cyan / hazard yellow
    const Color deco = cyber ? rgb(126, 255, 176) : rgb(168, 236, 110);       // decontamination green
    const Color sand = cyber ? rgb(192, 202, 214) : rgb(180, 162, 116);
    const Color rust = rgb(158, 86, 44);
    const Color lamp = rgb(255, 244, 200);
    // the two rolling frames are half a tread pitch apart, the standing frame sits between them
    float ph = frame == 1 ? 0.0f : (frame == 2 ? 0.5f : 0.25f);
    // six wheels first, sticking out past the hull so they read as tyres: the body covers their inner edge only
    for (float wx : { 13.0f, 22.5f, 46.5f }) {
        radWheel(A, wx, 21.2f, ph, cyber, true);
        radWheel(A, wx, 42.8f, ph, cyber, false);
    }
    A.box(31, 32, 25, 7.8f, 2.0f, hullD, 2.2f, 0.5f);                          // chassis rails
    if (cyber) {
        // ---- scrubber tank: a clean white cylinder with pale bands, a roof walkway and a hose reel
        A.cap(11.5f, 32, 37.5f, 32, 7.4f, hull, 2.8f, 0.65f);
        A.ell(37.5f, 32, 3.0f, 7.5f, shade(hull, 1.04f), 2.0f, 0.5f);
        A.ell(11.5f, 32, 2.6f, 7.3f, shade(hull, 0.9f), 1.8f, 0.5f);
        for (float bx : { 15.5f, 23.0f, 30.5f }) A.box(bx, 32, 0.8f, 7.5f, 0.3f, deck, 1.0f, 0.45f);
        A.line(13.5f, 24.8f, 40.5f, 24.8f, 0.6f, rgba(hullD, 210)); A.line(13.5f, 39.2f, 40.5f, 39.2f, 0.6f, rgba(hullD, 210));
        // bolted radiation-shield skirts and the cyan emitter strips
        A.box(26.5f, 23.9f, 13.0f, 1.2f, 0.4f, steel, 1.0f, 0.4f);
        A.box(26.5f, 40.1f, 13.0f, 1.2f, 0.4f, steel, 1.0f, 0.4f);
        for (int i = 0; i < 6; i++) { A.dot(15.5f + i * 4.4f, 23.9f, 0.4f, rgba(rgb(226, 234, 244), 230)); A.dot(15.5f + i * 4.4f, 40.1f, 0.4f, rgba(rgb(226, 234, 244), 230)); }
        A.line(14.0f, 25.5f, 40.0f, 25.5f, 0.7f, rgba(trim, 225)); A.line(14.0f, 38.5f, 40.0f, 38.5f, 0.7f, rgba(trim, 225));
        A.glow(27.0f, 25.5f, 8, rgba(trim, 45)); A.glow(27.0f, 38.5f, 8, rgba(trim, 45));
        A.box(24.0f, 32, 10.0f, 2.6f, 0.7f, deck, 1.4f, 0.4f);                  // roof walkway
        A.circle(15.8f, 32, 2.6f, rgb(150, 164, 180), 1.4f, 0.5f); A.circle(15.8f, 32, 1.7f, rgb(60, 70, 86), 1.1f, 0.3f);   // hose reel
        A.line(15.0f, 30.0f, 12.2f, 27.6f, 0.8f, rgb(70, 82, 100));             // hose
        // roof sprayer boom across the tank, then the trefoil under a soft green wash
        A.cap(23.0f, 25.2f, 23.0f, 38.8f, 0.85f, steel, 1.0f, 0.45f);
        for (float ny : { 26.2f, 32.0f, 37.8f }) { A.dot(23.0f, ny, 0.8f, deco); A.glow(23.0f, ny, 3.0f, rgba(deco, 120)); }
        A.glow(32.5f, 32, 8, rgba(deco, 75));
        radTrefoil(A, 32.5f, 32, 3.9f, rgb(176, 246, 198), rgb(22, 28, 24));
        // rear manifold and the side sprayer arms reaching out behind the wheels
        A.box(8.8f, 32, 1.3f, 6.6f, 0.5f, steel, 0.9f, 0.4f);
        for (float ny : { 27.4f, 30.6f, 33.4f, 36.6f }) { A.dot(7.4f, ny, 0.8f, deco); A.glow(7.2f, ny, 3.0f, rgba(deco, 130)); }
        A.cap(14.0f, 24.0f, 9.6f, 20.2f, 0.9f, steel, 1.0f, 0.45f);
        A.cap(14.0f, 40.0f, 9.6f, 43.8f, 0.9f, steel, 1.0f, 0.45f);
        A.dot(9.2f, 19.8f, 0.9f, deco); A.dot(9.2f, 44.2f, 0.9f, deco);
        A.glow(9.0f, 19.8f, 3.4f, rgba(deco, 140)); A.glow(9.0f, 44.2f, 3.4f, rgba(deco, 140));
        // ---- cab: white composite over a steel roof, dark windshield, sensor dish and twin slim stacks
        A.box(49.6f, 32, 8.4f, 8.1f, 2.0f, hull, 2.6f, 0.65f);
        A.box(48.4f, 32, 6.2f, 5.7f, 1.2f, rgb(164, 176, 192), 1.4f, 0.45f);   // steel cab roof
        A.box(54.7f, 32, 2.3f, 6.2f, 0.9f, glass, 1.4f, 0.5f);                  // windshield band
        A.line(51.4f, 29.6f, 54.2f, 29.6f, 0.5f, rgba(trim, 170)); A.line(51.4f, 34.4f, 54.2f, 34.4f, 0.5f, rgba(trim, 170));
        A.box(57.3f, 32, 0.8f, 6.6f, 0.3f, shade(hull, 0.96f), 0.9f, 0.4f);     // bonnet strip
        for (int i = 0; i < 3; i++) A.line(56.4f, 28.4f + i * 3.2f, 58.0f, 28.4f + i * 3.2f, 0.7f, rgba(trim, 120));   // front grille
        A.box(58.6f, 32, 0.6f, 8.8f, 0.3f, steel, 1.0f, 0.4f);                  // bumper
        A.dot(57.8f, 26.8f, 0.8f, lamp); A.dot(57.8f, 37.2f, 0.8f, lamp);
        for (int s = -1; s <= 1; s += 2) {                                      // mirrors
            float my = s < 0 ? 22.6f : 41.4f, ey = s < 0 ? 24.4f : 39.6f;
            A.line(52.6f, ey, 50.8f, my, 0.6f, rgba(hullD, 230)); A.box(50.4f, my, 1.2f, 0.9f, 0.3f, dark, 0.7f, 0.3f);
        }
        A.box(50.4f, 32, 1.7f, 0.9f, 0.2f, A.team, 0.6f, 0.3f);                 // team stripe on the cab roof
        A.circle(46.4f, 32, 2.9f, steel, 1.2f, 0.5f); A.circle(46.4f, 32, 2.0f, dark, 1.0f, 0.3f);   // sensor dish
        A.dot(46.4f, 32, 0.75f, deco); A.glow(46.4f, 32, 4.6f, rgba(deco, 120));
        A.line(48.4f, 30.6f, 47.0f, 31.4f, 0.4f, steel);                        // dish feed arm
        A.circle(39.6f, 32, 1.6f, steel, 1.0f, 0.4f); A.dot(39.6f, 32, 1.05f, deco); A.glow(39.6f, 32, 6.2f, rgba(deco, 155));   // decon beacon on the tank's front shoulder
        A.circle(42.8f, 24.9f, 1.4f, steel, 1.0f, 0.4f); A.dot(42.8f, 24.9f, 0.7f, dark);   // exhaust stacks
        A.circle(42.8f, 39.1f, 1.4f, steel, 1.0f, 0.4f); A.dot(42.8f, 39.1f, 0.7f, dark);
        A.grain(7, 18, 59, 46, 0.05f, 12);
    } else {
        // ---- riveted box tank with a decon-green band at the back and a hazard-yellow band at the front
        A.box(24.0f, 32, 16.0f, 8.6f, 1.8f, hull, 2.6f, 0.6f);
        A.box(24.0f, 32, 13.6f, 6.2f, 1.0f, shade(hull, 1.06f), 1.4f, 0.45f);  // raised deck
        A.box(8.6f, 32, 1.0f, 7.6f, 0.4f, shade(hull, 0.88f), 1.0f, 0.4f);      // bolted rear plate
        for (int i = 0; i < 5; i++) A.box(8.6f, 27.4f + i * 2.4f, 1.05f, 0.55f, 0.1f, i % 2 ? deco : dark, 0.3f, 0.1f);   // rear hazard chevrons
        rivets(A, 12, 26.6f, 39.0f, 26.6f, 7, shade(hull, 1.35f));
        rivets(A, 12, 37.4f, 39.0f, 37.4f, 7, shade(hull, 1.35f));
        A.box(32.0f, 32, 1.1f, 8.9f, 0.3f, trim, 0.7f, 0.3f);                   // hazard-yellow band
        for (int i = 0; i < 4; i++) A.box(32.0f, 26.0f + i * 4.0f, 1.15f, 0.55f, 0.1f, dark, 0.3f, 0.1f);
        A.box(17.6f, 32, 0.8f, 8.9f, 0.3f, rgba(deco, 230), 0.7f, 0.3f);        // decon-green band
        A.ell(25.0f, 28.0f, 3.4f, 1.5f, rgba(sand, 200), 1, 0);                 // sand camouflage patches
        A.ell(28.0f, 36.4f, 3.0f, 1.4f, rgba(rgb(86, 92, 64), 190), 1, 0);
        A.glow(23.5f, 32, 7.5f, rgba(deco, 60));
        radTrefoil(A, 23.5f, 32, 3.9f, rgb(232, 198, 74), rgb(34, 34, 26));
        // roof sprayer boom and rear manifold
        A.cap(20.2f, 24.6f, 20.2f, 39.4f, 1.0f, steel, 1.1f, 0.5f);
        for (float ny : { 25.6f, 32.0f, 38.4f }) { A.dot(20.2f, ny, 0.9f, deco); A.glow(20.2f, ny, 3.4f, rgba(deco, 120)); }
        A.box(12.2f, 32, 1.1f, 6.8f, 0.4f, steel, 1.0f, 0.4f);
        for (float ny : { 27.2f, 30.4f, 33.6f, 36.8f }) { A.dot(11.0f, ny, 0.8f, deco); A.glow(10.8f, ny, 3.0f, rgba(deco, 130)); }
        // side sprayer arms and bolted radiation-shield skirts (they sit over the tyres like fenders)
        A.box(26.0f, 23.1f, 13.0f, 1.3f, 0.5f, rgb(118, 120, 110), 1.0f, 0.4f);
        A.box(26.0f, 40.9f, 13.0f, 1.3f, 0.5f, rgb(118, 120, 110), 1.0f, 0.4f);
        rivets(A, 14, 23.1f, 38, 23.1f, 7, shade(steel, 1.25f));
        rivets(A, 14, 40.9f, 38, 40.9f, 7, shade(steel, 1.25f));
        A.cap(14.0f, 23.6f, 9.6f, 19.8f, 1.0f, steel, 1.0f, 0.45f);
        A.cap(14.0f, 40.4f, 9.6f, 44.2f, 1.0f, steel, 1.0f, 0.45f);
        A.dot(9.2f, 19.4f, 0.95f, deco); A.dot(9.2f, 44.6f, 0.95f, deco);
        A.glow(9.0f, 19.4f, 3.6f, rgba(deco, 140)); A.glow(9.0f, 44.6f, 3.6f, rgba(deco, 140));
        // ---- cab: olive drab, sand roof, cracked glass, brush guard and the big sooty exhaust stack
        A.box(49.8f, 32, 8.4f, 8.8f, 1.8f, shade(hull, 0.97f), 2.6f, 0.6f);
        A.box(48.6f, 32, 6.2f, 6.2f, 1.2f, sand, 1.4f, 0.45f);                  // sand cab roof
        for (int i = 0; i < 3; i++) A.line(43.8f, 28.6f + i * 3.3f, 53.2f, 28.6f + i * 3.3f, 0.5f, rgba(hullD, 180));
        A.box(54.9f, 32, 2.3f, 6.6f, 0.9f, glass, 1.4f, 0.5f);                  // windshield band
        A.line(53.4f, 30.2f, 54.8f, 33.8f, 0.4f, rgba(rgb(220, 226, 220), 150));   // cracked glass
        for (int i = 0; i < 4; i++) A.line(56.5f, 28.2f + i * 2.3f, 57.9f, 28.2f + i * 2.3f, 0.7f, rgba(dark, 220));   // grille slats
        A.line(58.7f, 24.6f, 58.7f, 39.4f, 1.1f, steel);                        // brush guard
        A.dot(58.3f, 24.6f, 0.85f, steel); A.dot(58.3f, 39.4f, 0.85f, steel);
        A.dot(57.2f, 26.9f, 0.9f, lamp); A.dot(57.2f, 37.1f, 0.9f, lamp);
        for (int s = -1; s <= 1; s += 2) {                                      // mirrors
            float my = s < 0 ? 23.0f : 41.0f, ey = s < 0 ? 24.8f : 39.4f;
            A.line(52.8f, ey, 51.0f, my, 0.6f, rgba(dark, 200)); A.box(50.6f, my, 1.2f, 0.9f, 0.3f, rgb(30, 30, 24), 0.7f, 0.3f);
        }
        A.box(50.6f, 32, 1.7f, 0.9f, 0.2f, A.team, 0.6f, 0.3f);                 // team stripe on the cab roof
        A.box(52.0f, 28.4f, 1.1f, 0.9f, 0.2f, trim, 0.6f, 0.3f);                // yellow filler cap
        A.circle(43.4f, 22.8f, 2.4f, steel, 1.4f, 0.5f); A.circle(43.4f, 22.8f, 1.5f, dark, 1.1f, 0.3f);   // big exhaust stack
        A.ell(42.8f, 25.4f, 2.4f, 1.5f, rgba(rgb(22, 22, 18), 135), 1, 0);      // soot stain
        A.box(43.4f, 22.8f, 2.1f, 0.45f, 0.2f, dark, 0.5f, 0.2f);
        A.box(44.6f, 32, 3.2f, 1.0f, 0.3f, dark, 0.6f, 0.3f);                   // beacon housing
        A.dot(44.6f, 32, 1.2f, deco); A.glow(44.6f, 32, 6.4f, rgba(deco, 150));
        weatherClanker(A, 63, 6, 18, 59, 46);
        Rng r(63);
        for (int i = 0; i < 5; i++) A.ell(r.f(11, 38), r.f(25, 39), r.f(1.2f, 2.5f), r.f(0.9f, 1.7f), rgba(rust, 115), 1, 0);
        A.line(10.4f, 28.0f, 22.0f, 26.8f, 0.5f, rgba(rust, 140));
    }
    teamDot(A, 12.0f, 32, 1.9f);
}

// ---------------------------------------------------------------- aircraft


void jetCyber(Art& A) {   // Specter Jet: single-seat strike fighter (F/A-18 and F-35 family): a cropped delta with a chined nose, a bubble canopy, broad twin fins canted outward with team-coloured tips, twin nozzles, and bombs and a wingtip missile on the pylons
    auto mir = [](float y) { return 64 - y; };
    auto sym = [&](std::vector<P> up, Color col, float bev, float kk) {
        std::vector<P> lo;
        for (const P& p : up) lo.push_back({p.x, mir(p.y)});
        A.poly(up, col, bev, kk); A.poly(lo, col, bev, kk);
    };
    auto capS = [&](float ax, float ay, float bx, float by, float r, Color col, float bev, float kk) {
        A.cap(ax, ay, bx, by, r, col, bev, kk); A.cap(ax, mir(ay), bx, mir(by), r, col, bev, kk);
    };
    auto stroke = [&](float ax, float ay, float bx, float by, float w, Color col) {
        A.line(ax, ay, bx, by, w, col); A.line(ax, mir(ay), bx, mir(by), w, col);
    };
    // a bomb or a missile hung under the wing, seen from above, with a soft shadow to the south-east
    auto store = [&](float x0, float x1, float y, float r, Color body, bool both) {
        for (int m = 0; m < (both ? 2 : 1); m++) {
            float yy = m ? mir(y) : y;
            A.soft((int)x0 - 4, (int)(yy - r - 5), (int)x1 + 5, (int)(yy + r + 6), [=](float px, float py) { return sdCap(px, py, x0 + 1.0f, yy + 1.2f, x1 - r + 0.9f, yy + 1.2f, r); }, rgba(rgb(8, 10, 16), 130), 1.6f);
            A.box(x0 + r * 1.5f, yy, r * 0.55f, r * 2.2f, 0.15f, shade(body, 0.72f), 0.5f, 0.3f);
            A.cap(x0 + r * 1.2f, yy, x1 - r, yy, r, body, 0.7f, 0.55f);
            A.circle(x1 - r * 0.6f, yy, r * 0.75f, shade(body, 0.45f), 0.6f, 0.4f);
        }
    };
    const Color GLS = rgb(24, 62, 86), INK = rgb(8, 12, 18);
    // cropped delta wing with a swept leading edge and a straight trailing edge
    sym({{44, 28.2f}, {20, 9.0f}, {17.5f, 11.6f}, {14.5f, 28.5f}}, CYH, 2.4f, 0.6f);
    stroke(44, 28.2f, 20, 9.2f, 2.6f, rgba(CYG, 45));
    stroke(44, 28.2f, 20, 9.2f, 0.8f, rgba(CYG, 225));
    stroke(16.5f, 26.8f, 41, 28.0f, 0.5f, rgba(INK, 140));
    // chined nose and the fuselage spine
    A.cap(8, 32, 49, 32, 4.1f, shade(CYH, 1.1f), 2.6f, 0.6f);
    sym({{42, 27.8f}, {52, 29.2f}, {59, 30.8f}, {62, 32}, {42, 32}}, shade(CYP, 0.95f), 1.6f, 0.55f);
    stroke(60, 31, 44, 28.2f, 0.7f, rgba(CYG, 220));
    A.line(12, 32, 52, 32, 0.6f, rgba(CYG, 140));
    // bubble canopy with a frame and a glint
    A.ell(47.2f, 32, 4.6f, 2.3f, GLS, 1.5f, 0.65f);
    A.ell(46.0f, 31.0f, 2.4f, 0.6f, rgba(CYW, 220), 1, 0);
    A.line(43.5f, 32, 50.5f, 32, 0.35f, rgba(INK, 190));
    // intake slots under the wing root
    for (int m = 0; m < 2; m++) A.box(34.5f, m ? mir(28.3f) : 28.3f, 3.4f, 1.3f, 0.5f, INK, 1, 0.3f);
    // stabilisers, then the broad canted twin fins with team-coloured tips
    sym({{20, 29.8f}, {11.5f, 26.5f}, {12.6f, 28.4f}, {17, 30.8f}}, shade(CYH, 1.05f), 1.1f, 0.5f);
    capS(15.5f, 26.8f, 9.6f, 14.8f, 1.5f, shade(CYP, 0.9f), 1.0f, 0.5f);
    capS(11.2f, 19.4f, 9.4f, 14.8f, 1.05f, A.team, 0.8f, 0.5f);
    // twin nozzles with glowing rings
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 3.8f;
        A.box(7.0f, y, 3.0f, 2.4f, 0.8f, shade(CYD, 1.5f), 1.2f, 0.5f);
        A.dot(5.2f, y, 1.6f, CYG);
        A.glow(4.4f, y, 6, rgba(CYG, 150));
    }
    // bombs under the wing (two a side) and an AIM-9 on each wingtip rail
    store(21.0f, 33.5f, 18.5f, 1.7f, rgb(150, 158, 166), true);
    store(18.5f, 29.0f, 25.5f, 1.7f, rgb(150, 158, 166), true);
    store(15.8f, 27.5f, 10.2f, 0.85f, rgb(214, 218, 222), true);
    // team roundel on the wing, and the position lights at the tips
    teamDot(A, 21.4f, 13.6f, 1.8f); teamDot(A, 21.4f, mir(13.6f), 1.8f);
    A.dot(14.3f, 5.6f, 0.9f, rgb(255, 80, 70)); A.dot(14.3f, 58.4f, 0.9f, rgb(80, 255, 120));
    A.grain(8, 4, 63, 60, 0.05f, 55);
}

void jetClanker(Art& A) {   // Talon Jet: Su-34 / Su-25 family ground-attack jet: a broad flat-sided fuselage with a blunt duck-bill nose and a wide cockpit, a swept wing carrying rocket pods, twin canted fins and twin nozzles
    auto mir = [](float y) { return 64 - y; };
    auto sym = [&](std::vector<P> up, Color col, float bev, float kk) {
        std::vector<P> lo;
        for (const P& p : up) lo.push_back({p.x, mir(p.y)});
        A.poly(up, col, bev, kk); A.poly(lo, col, bev, kk);
    };
    auto capS = [&](float ax, float ay, float bx, float by, float r, Color col, float bev, float kk) {
        A.cap(ax, ay, bx, by, r, col, bev, kk); A.cap(ax, mir(ay), bx, mir(by), r, col, bev, kk);
    };
    auto stroke = [&](float ax, float ay, float bx, float by, float w, Color col) {
        A.line(ax, ay, bx, by, w, col); A.line(ax, mir(ay), bx, mir(by), w, col);
    };
    auto ellS = [&](float cx, float cy, float rx, float ry, Color col) {
        A.ell(cx, cy, rx, ry, col, 1, 0); A.ell(cx, mir(cy), rx, ry, col, 1, 0);
    };
    // an unguided rocket pod seen from above: a steel tube, a rust warhead, a dark motor bell and a soft shadow to the south-east
    auto pod = [&](float x0, float x1, float y, float r) {
        for (int m = 0; m < 2; m++) {
            float yy = m ? mir(y) : y;
            A.soft((int)x0 - 3, (int)(yy - r - 4), (int)x1 + 5, (int)(yy + r + 5), [=](float px, float py) { return sdCap(px, py, x0 + 0.9f, yy + 1.0f, x1 - r * 0.5f + 0.9f, yy + 1.0f, r); }, rgba(rgb(8, 10, 16), 120), 1.8f);
            A.cap(x0, yy, x1 - r * 0.5f, yy, r, shade(CKS, 1.05f), 0.8f, 0.6f);
            A.circle(x1 - r * 0.1f, yy, r * 0.78f, CKR, 0.7f, 0.4f);
            A.dot(x1 + 0.2f, yy - r * 0.2f, r * 0.22f, rgba(rgb(255, 255, 255), 200));
            A.dot(x0 + 0.5f, yy, r * 0.55f, rgb(8, 10, 16));
        }
    };
    const Color GLS = rgb(74, 110, 126);
    // stabilisers, then the swept wing, then the fuselage over them
    sym({{20, 30.2f}, {10, 25.2f}, {7.6f, 27.4f}, {15.8f, 32}}, shade(CKH, 0.92f), 1.6f, 0.58f);
    sym({{42, 27.4f}, {19, 8.2f}, {15.5f, 10.4f}, {24, 28.2f}}, CKH, 2.4f, 0.6f);
    // sand and dark-olive camouflage over the wing
    ellS(23.0f, 15.8f, 3.6f, 1.6f, rgba(CKT, 200));
    ellS(28.5f, 21.5f, 4.0f, 1.6f, rgba(CKD, 170));
    ellS(30.5f, 24.2f, 3.0f, 1.2f, rgba(CKT, 180));
    stroke(42, 27.4f, 19, 8.2f, 0.8f, rgba(CKD, 150));
    stroke(24, 28.2f, 15.5f, 10.4f, 0.6f, rgba(CKD, 130));
    // broad fuselage with a blunt duck-bill nose
    A.cap(7.5f, 32, 48, 32, 4.7f, shade(CKH, 1.06f), 2.8f, 0.62f);
    sym({{42, 27.4f}, {53, 27.8f}, {59.2f, 29.2f}, {61.8f, 31.4f}, {61.8f, 32}, {42, 32}}, shade(CKH, 0.92f), 1.8f, 0.55f);
    // wide side-by-side cockpit canopy with its centre frame and a glint
    A.ell(50.5f, 32, 5.0f, 3.2f, GLS, 2.0f, 0.75f);
    A.ell(49.2f, 30.9f, 3.0f, 0.9f, rgba(rgb(210, 232, 246), 200), 1, 0);
    A.line(46.5f, 32, 55.0f, 32, 0.5f, rgba(CKD, 190));
    A.line(14, 32, 40, 32, 0.8f, rgba(CKD, 150));   // spine seam
    // twin canted fins with team-coloured tips
    capS(14.8f, 28.8f, 8.8f, 15.8f, 1.8f, shade(CKH, 0.95f), 1.2f, 0.55f);
    capS(10.6f, 20.6f, 8.6f, 15.8f, 1.2f, A.team, 0.8f, 0.5f);
    // twin nozzles, side by side, with an orange glow
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 3.4f;
        A.circle(5.8f, y, 2.4f, shade(CKD, 1.5f), 1.6f, 0.5f);
        A.circle(5.8f, y, 1.4f, rgb(36, 20, 14), 1.0f, 0.4f);
        A.glow(4.6f, y, 6, rgba(rgb(255, 150, 50), 150));
    }
    // three rocket pods a side on the wing pylons
    pod(16.5f, 26.5f, 12.0f, 1.5f);
    pod(17.5f, 28.5f, 17.0f, 1.5f);
    pod(19.0f, 30.0f, 22.0f, 1.5f);
    // team roundel on the wing root, hazard stripes at the wingtips, rust and scratches
    teamDot(A, 31.0f, 25.5f, 1.9f); teamDot(A, 31.0f, mir(25.5f), 1.9f);
    A.dot(19.2f, 6.9f, 0.9f, rgb(255, 80, 70)); A.dot(19.2f, 57.1f, 0.9f, rgb(80, 255, 120));
    stroke(33, 29.6f, 18, 26, 0.5f, rgba(CKR, 130));
    A.grain(4, 4, 64, 60, 0.12f, 77);
}
void droneBody(Art& A) {   // Wraith Drone: stealth flying wing
    A.poly({ {58, 32}, {40, 24}, {13, 8}, {8, 12}, {19, 26}, {15, 32}, {19, 38}, {8, 52}, {13, 56}, {40, 40} }, CYH, 2.8f, 0.6f);
    // centre spine and canopy sensor
    A.poly({ {54, 32}, {40, 28}, {20, 29}, {17, 32}, {20, 35}, {40, 36} }, shade(CYP, 0.9f), 2.6f, 0.6f);
    A.ell(38, 32, 5, 2.6f, rgb(20, 60, 84), 1.6f, 0.5f); A.line(34, 31, 42, 31, 0.8f, rgba(CYG, 210));
    // wing panel lines and weapon pods
    A.line(20, 26, 40, 27, 0.8f, rgba(CYD, 170)); A.line(20, 38, 40, 37, 0.8f, rgba(CYD, 170));
    A.cap(28, 18.5f, 40, 22, 1.6f, CYM, 1.2f, 0.5f); A.cap(28, 45.5f, 40, 42, 1.6f, CYM, 1.2f, 0.5f);
    A.dot(41, 22.2f, 1.1f, CYG); A.dot(41, 41.8f, 1.1f, CYG);
    // twin engines with glowing exhaust
    for (int s = -1; s <= 1; s += 2) {
        A.box(15, 32 + s * 5, 4, 2.6f, 1.2f, shade(CYD, 1.3f), 1.4f, 0.5f);
        A.dot(11.2f, 32 + s * 5, 2.4f, CYG); A.glow(10, 32 + s * 5, 8, rgba(CYG, 190));
    }
    teamDot(A, 25.0f, 32, 2.9f);
    A.dot(9.5f, 12, 0.9f, rgb(255, 80, 70)); A.dot(9.5f, 52, 0.9f, rgb(80, 255, 120));   // nav lights
}
void hornetBody(Art& A) {   // Hornet Gunship: slim electric attack helicopter, ducted tail fan, stub wings with twin laser pods (rotor hub at the centre)
    // tail boom and the glowing ducted fan
    A.cap(30, 32, 7, 32, 2.2f, shade(CYH, 0.92f), 1.6f, 0.55f);
    A.line(26, 32, 10, 32, 0.6f, rgba(CYG, 170));
    A.circle(6.5f, 32, 4.6f, shade(CYD, 1.3f), 1.6f, 0.5f); A.circle(6.5f, 32, 3.0f, rgb(14, 30, 44), 1.2f, 0.3f);
    A.glow(6.5f, 32, 7, rgba(CYG, 150)); A.dot(6.5f, 32, 1.1f, CYG);
    A.box(11, 32, 1.2f, 6.0f, 0.5f, shade(CYP, 1.05f), 1.0f, 0.5f);                    // tailplane
    A.dot(11, 26.3f, 0.8f, rgb(255, 80, 70)); A.dot(11, 37.7f, 0.8f, rgb(80, 255, 120));
    // stub wings with laser pods
    A.box(31, 32, 3.6f, 18.5f, 1.4f, shade(CYP, 0.95f), 1.8f, 0.5f);
    for (int sd = -1; sd <= 1; sd += 2) {
        float y = 32 + sd * 15.0f;
        A.cap(27, y, 41, y, 2.0f, CYM, 1.4f, 0.6f);
        A.line(29, y, 42.5f, y, 0.8f, rgba(CYG, 220)); A.glow(43, y, 5, rgba(CYG, 160)); A.dot(42.6f, y, 1.0f, CYW);
    }
    // fuselage: a narrow armoured pod, stepped tandem canopy, sensor nose
    A.poly({ {54, 32}, {49, 26.8f}, {36, 25.6f}, {24, 27.2f}, {21, 32}, {24, 36.8f}, {36, 38.4f}, {49, 37.2f} }, CYH, 2.8f, 0.62f);
    A.poly({ {50, 32}, {46, 28.6f}, {37, 27.8f}, {28, 29}, {26, 32}, {28, 35}, {37, 36.2f}, {46, 35.4f} }, shade(CYP, 1.05f), 2.0f, 0.5f);
    A.ell(44.5f, 32, 5.6f, 3.0f, rgb(18, 52, 74), 1.8f, 0.7f); A.ell(38, 32, 4.0f, 2.6f, rgb(18, 52, 74), 1.6f, 0.7f);
    A.ell(43.4f, 31, 2.6f, 0.9f, rgba(CYW, 200), 1, 0); A.ell(37.2f, 31, 1.8f, 0.8f, rgba(CYW, 180), 1, 0);
    A.circle(55.5f, 32, 2.1f, shade(CYD, 1.4f), 1.2f, 0.5f); A.dot(56.2f, 32, 1.1f, CYG); A.glow(56.5f, 32, 5, rgba(CYG, 130));   // sensor ball
    A.line(24, 28.4f, 34, 27.4f, 0.6f, rgba(CYD, 160)); A.line(24, 35.6f, 34, 36.6f, 0.6f, rgba(CYD, 160));
    A.circle(32, 32, 3.0f, shade(CYD, 1.5f), 1.6f, 0.55f); A.dot(32, 32, 1.2f, CYG);   // rotor mast
    teamDot(A, 26.5f, 32, 2.4f);
    A.grain(6, 13, 58, 51, 0.05f, 141);
}
void gunshipBody(Art& A) {   // Vulture Gunship: fuselage, tail boom, stub wings with rocket pods
    A.cap(28, 32, 4, 32, 2.6f, shade(CKH, 0.9f), 1.8f, 0.5f);                          // tail boom
    A.box(6, 32, 1.4f, 7.5f, 0.6f, shade(CKH, 0.8f), 1.2f, 0.4f);                       // tail stabiliser
    A.circle(4.5f, 32, 4.2f, rgba(CKD, 120), 1, 0, 0.5f);                                // tail rotor disc
    A.cap(4.5f, 27.6f, 4.5f, 36.4f, 0.9f, CKD, 1, 0.3f);
    // stub wings + pods
    A.box(30, 32, 4.4f, 20, 1.6f, shade(CKH, 0.95f), 2, 0.5f);
    for (int s = -1; s <= 1; s += 2) {
        A.box(34, 32 + s * 14.6f, 7, 3.0f, 1.2f, CKS, 1.6f, 0.55f);
        for (int k = 0; k < 3; k++) A.dot(40.6f, 32 + s * (12.8f + k * 1.8f), 0.9f, CKR);
        A.dot(41.5f, 32 + s * 14.6f, 1.2f, rgb(60, 40, 30));
    }
    // fuselage
    A.ell(34, 32, 19, 8.6f, CKH, 3.2f, 0.6f);
    A.ell(30, 32, 10, 6.2f, shade(CKH, 0.85f), 2, 0.5f);
    A.line(20, 27.8f, 39, 27.2f, 0.7f, rgba(CKD, 150)); A.line(20, 36.2f, 39, 36.8f, 0.7f, rgba(CKD, 150));
    A.ell(43.5f, 32, 8.2f, 5.6f, rgb(70, 108, 128), 3.0f, 0.75f);                        // canopy glass
    A.ell(42, 30, 3.6f, 1.8f, rgba(rgb(200, 230, 245), 190), 1, 0);                         // glint
    A.cap(51, 32, 56, 32, 1.2f, CKD, 1.2f, 0.5f);                                         // chin gun
    A.circle(28, 32, 3.0f, CKS, 1.8f, 0.55f);                                              // rotor hub
    A.grain(14, 22, 56, 42, 0.14f, 44);
    teamDot(A, 21.5f, 32, 2.7f);
}

// Spy (Infiltrator / Spy): the infantry body with no rifle. Cyber is a black bodysuit with a glowing visor and a hacking slab; Clanker is a
// long trench coat whose tails swing with the stride, a brimmed hat and a briefcase. Walk frames 1/2 swing the feet.
void spyBody(Art& A, bool cy, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 3.0f : -3.0f);
    if (cy) {
        Color suit = rgb(46, 50, 60), sleeve = rgb(62, 66, 78), boot = rgb(16, 18, 24);
        A.ell(32 + step, 28, 3.6f, 2.4f, boot, 1.4f, 0.4f);
        A.ell(32 - step, 36, 3.6f, 2.4f, boot, 1.4f, 0.4f);
        A.ell(32.5f, 32, 6.0f, 9.0f, suit, 3.0f, 0.55f);                                     // bodysuit: slimmer than a trooper
        A.line(31.5f, 25, 31.5f, 39, 0.9f, rgba(CYG, 170));                                    // spine seam, faintly lit
        A.cap(33.5f, 32 - 7.6f, 40.0f, 28.4f, 2.0f, sleeve, 1.4f, 0.5f);                       // empty left hand, forward
        A.cap(33.5f, 32 + 7.6f, 41.0f, 35.0f, 2.0f, sleeve, 1.4f, 0.5f);                       // right hand on the hacking slab
        A.box(43.8f, 35.6f, 3.0f, 2.2f, 0.6f, rgb(24, 26, 32), 1.2f, 0.5f);                     // hacking slab
        A.box(44.2f, 35.6f, 1.6f, 1.2f, 0.3f, CYG, 0.6f, 0.2f); A.glow(44.4f, 35.6f, 5.0f, rgba(CYG, 110));
        A.circle(33.5f, 32, 4.6f, rgb(30, 33, 40), 2.0f, 0.6f);                                 // hood
        A.line(30.5f, 28.4f, 30.5f, 35.6f, 0.6f, rgba(CYG, 110));                               // shoulder seam
        A.box(37.4f, 32, 1.2f, 3.0f, 0.5f, CYG, 1, 0.2f); A.glow(38.2f, 32, 5.0f, rgba(CYG, 110));   // visor
        teamDot(A, 28.4f, 32, 2.0f);
    } else {
        Color coat = rgb(178, 152, 104), coatD = rgb(120, 98, 62), boot = rgb(44, 36, 28);
        A.ell(32 + step, 28, 3.6f, 2.4f, boot, 1.4f, 0.4f);
        A.ell(32 - step, 36, 3.6f, 2.4f, boot, 1.4f, 0.4f);
        float tl = step * 0.7f;   // the coat's front and back tails swing opposite each other
        A.poly({ {18 - tl, 25.5f}, {25, 21.8f}, {40, 21.4f}, {46.5f + tl, 24.5f}, {47 + tl, 39.5f}, {40, 42.6f}, {25, 42.2f}, {18 - tl, 38.5f} }, coat, 2.4f, 0.55f);
        A.line(37, 22.5f, 41.5f, 31, 0.8f, rgba(coatD, 200)); A.line(37, 41.5f, 41.5f, 33, 0.8f, rgba(coatD, 200));   // lapels
        A.line(21, 32, 45, 32, 0.7f, rgba(coatD, 160));                                        // belt
        A.box(23.2f, 32, 1.2f, 9.0f, 0.6f, coatD, 1.0f, 0.4f);                                 // turned-up collar at the back
        A.line(40.5f, 22.5f, 44, 24, 0.6f, rgba(coatD, 150)); A.line(40.5f, 41.5f, 44, 40, 0.6f, rgba(coatD, 150));   // hem seams
        A.dot(35.5f, 27.5f, 0.6f, coatD); A.dot(35.5f, 36.5f, 0.6f, coatD);                     // buttons
        A.dot(29.5f, 25.0f, 1.0f, shade(coat, 1.15f)); A.dot(29.5f, 39.0f, 1.0f, shade(coat, 1.15f));   // shoulder tabs
        A.cap(33, 32 - 8.3f, 40, 28.6f, 2.0f, coatD, 1.4f, 0.5f);                               // sleeves
        A.cap(33, 32 + 8.3f, 40.5f, 35.2f, 2.0f, coatD, 1.4f, 0.5f);
        A.box(43.6f, 35.8f, 3.6f, 2.4f, 0.6f, rgb(96, 62, 38), 1.6f, 0.5f);                    // briefcase
        A.line(42.2f, 34.4f, 45, 34.4f, 0.5f, rgba(rgb(60, 38, 22), 200));
        A.dot(45.0f, 35.8f, 0.6f, rgb(214, 176, 80));
        A.circle(33.5f, 32, 7.0f, rgb(58, 44, 30), 2.2f, 0.5f);                                // brim, seen from above
        A.circle(33.5f, 32, 4.4f, rgb(86, 64, 42), 1.8f, 0.5f);                                // crown
        teamDot(A, 33.5f, 32, 2.0f);
    }
}

// Ghost Drone (Cyber) / Recon Drone (Clanker): a small matte pod on four stub arms with ducted rotor pods at the tips, a sensor lens at the
// nose and a tail boom. The rotor is drawn live over the hub at the centre.
void spyDroneBody(Art& A, bool cy) {
    Color body = cy ? rgb(28, 30, 38) : rgb(64, 68, 54), arm = cy ? rgb(44, 46, 56) : rgb(86, 90, 70), duct = cy ? rgb(10, 11, 16) : rgb(40, 42, 32);
    Color lens = cy ? CYG0 : rgb(255, 176, 60);
    A.cap(25, 32, 15, 32, 1.1f, arm, 1.0f, 0.5f);                                              // tail boom
    A.box(14.6f, 32, 1.1f, 4.6f, 0.5f, arm, 1.0f, 0.4f);                                       // tail fin
    A.line(24, 30, 19, 24.5f, 0.5f, rgba(lens, 150));                                          // faint antenna
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
        float ex = 32 + sx * 13.5f, ey = 32 + sy * 13.5f;
        A.cap(32 + sx * 5, 32 + sy * 4, ex, ey, 1.2f, arm, 1.0f, 0.5f);                        // stub arm
        A.circle(ex, ey, 3.6f, arm, 1.6f, 0.5f);
        A.circle(ex, ey, 2.4f, duct, 1.0f, 0.2f);
        A.ring(ex, ey, 2.4f, 0.4f, rgba(lens, cy ? 150 : 110), 0.5f, 0);
    }
    A.ell(32, 32, 8.0f, 6.0f, body, 2.4f, 0.6f);                                               // pod
    A.ell(31.5f, 32, 4.4f, 3.4f, shade(body, 1.35f), 1.4f, 0.4f);                              // top deck
    A.circle(38.5f, 32, 2.5f, duct, 1.4f, 0.5f);                                               // sensor dome
    A.dot(38.8f, 31.6f, 1.1f, lens); A.glow(39.0f, 32, 5.0f, rgba(lens, 140));
    teamDot(A, 28.0f, 32, 1.9f);
}

// Atlas Lifter (Cyber) / Mammoth Lifter (Clanker): a huge tandem-rotor cargo helicopter. The rotors are drawn live over the two hubs; the
// sprite is the fuselage with its rear loading ramp, glazed nose, side sponsons, engine pods and the hub fairings. Faces +x.
void cargoBody(Art& A, bool cy) {
    const float HUB = 16.8f;   // rotor hubs sit 20.8 screen px either side of the centre: 20.8 / (UNIT_SCALE * the lifter's scale of 2)
    Color hull = cy ? rgb(196, 206, 216) : rgb(96, 104, 64), hullD = cy ? rgb(96, 106, 122) : rgb(60, 66, 40);
    Color ramp = cy ? rgb(92, 100, 114) : rgb(156, 90, 48), glass = rgb(18, 52, 74), hubC = cy ? rgb(70, 78, 92) : rgb(70, 72, 56);
    Color trim = cy ? rgb(110, 232, 255) : rgb(232, 186, 58);
    // side sponsons, then the fuselage capsule over them
    for (int s = -1; s <= 1; s += 2) {
        A.box(28, 32 + s * 10.6f, 15, 2.4f, 1.2f, hullD, 1.6f, 0.5f);
        for (int i = 0; i < 6; i++) {
            if (cy) A.box(16 + i * 4.6f, 32 + s * 10.6f, 1.2f, 1.0f, 0.3f, i % 2 ? trim : hullD, 0.6f, 0.2f);   // hazard-trimmed sponson edge
            else A.dot(16 + i * 4.6f, 32 + s * 10.6f, 0.45f, rgba(hullD, 200));
        }
    }
    A.cap(12, 32, 52, 32, 8.6f, hull, 3.4f, 0.6f);     // the fuselage capsule; its tail is hidden by the ramp
    // emitter strips (Cyber) or hazard trim along the nose and sides (Clanker)
    if (cy) { A.line(14, 25.0f, 50, 25.0f, 0.9f, rgba(trim, 210)); A.line(14, 39.0f, 50, 39.0f, 0.9f, rgba(trim, 210)); A.glow(30, 25, 8, rgba(trim, 60)); A.glow(30, 39, 8, rgba(trim, 60)); }
    else {
        for (int i = 0; i < 4; i++) { A.box(44 + i * 3.4f, 24.3f, 1.4f, 1.0f, 0.2f, i % 2 ? trim : hullD, 0.6f, 0.2f); A.box(44 + i * 3.4f, 39.7f, 1.4f, 1.0f, 0.2f, i % 2 ? trim : hullD, 0.6f, 0.2f); }
        A.line(10, 24.4f, 12, 24.4f, 0.8f, rgba(trim, 220)); A.line(10, 39.6f, 12, 39.6f, 0.8f, rgba(trim, 220));
        for (int i = 0; i < 9; i++) A.dot(14 + i * 4.4f, 27.6f, 0.55f, rgba(hullD, 210));   // rivets
        for (int i = 0; i < 9; i++) A.dot(14 + i * 4.4f, 36.4f, 0.55f, rgba(hullD, 210));
    }
    // rear loading ramp, let down at the tail: a wedge with its hinge and the tread lines of the ramp
    A.poly({ {10, 24.6f}, {1.8f, 27.2f}, {1.8f, 36.8f}, {10, 39.4f} }, ramp, 1.6f, 0.5f);
    A.line(3.4f, 28.4f, 3.4f, 35.6f, 0.5f, rgba(hullD, 170));
    for (int i = 0; i < 3; i++) A.line(5.4f + i * 1.6f, 25.8f, 5.4f + i * 1.6f, 38.2f, 0.3f, rgba(hullD, 90));
    // glazed cockpit at the nose, with a glint
    A.ell(56, 32, 4.6f, 6.0f, glass, 2.0f, 0.6f);
    A.ell(57.2f, 30.4f, 2.0f, 0.9f, rgba(rgb(200, 230, 245), 190), 1, 0);
    A.line(52, 32, 61, 32, 0.6f, rgba(hullD, 150));
    // engine pods on the rear pylon, one either side of the hub
    for (int s = -1; s <= 1; s += 2) { A.ell(32 - HUB, 32 + s * 9.6f, 3.6f, 2.8f, hullD, 1.4f, 0.5f); A.dot(32 - HUB - 0.8f, 32 + s * 9.6f, 0.8f, cy ? trim : rgb(255, 180, 60)); }
    // hub fairings: a steel collar and a bright mast cap
    for (int s = -1; s <= 1; s += 2) {
        float hx = 32 + s * HUB;
        A.circle(hx, 32, 5.2f, hubC, 2.0f, 0.55f);
        A.circle(hx, 32, 2.8f, shade(hubC, 1.5f), 1.4f, 0.5f);
        A.dot(hx, 32, 1.0f, cy ? trim : rgb(255, 214, 124));
    }
    teamDot(A, 36, 32, 2.8f);
    A.grain(4, 20, 60, 44, cy ? 0.04f : 0.14f, cy ? 201 : 202);
}


// ---------------------------------------------------------------- detail layer
// Extra geometry drawn over every base sprite: soldier kit, hull skirts, stowage, vents, lamps, markings and weathering. It sits in its own layer so the
// base drawings stay readable, and everything is placed over shapes the base drawing already guarantees.
void scratches(Art& A, float x0, float y0, float x1, float y1, int n, u32 seed, int alpha = 80) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        float x = r.f(x0, x1), y = r.f(y0, y1), an = r.f(-0.5f, 0.5f) + (r.f() < 0.5f ? 0.0f : 1.5708f), l = r.f(1.5f, 4.0f);
        A.line(x, y, x + std::cos(an) * l, y + std::sin(an) * l, 0.5f, rgba(rgb(238, 240, 244), alpha));
    }
}
void mud(Art& A, float x0, float y0, float x1, float y1, int n, u32 seed) {
    Rng r(seed);
    for (int i = 0; i < n; i++) A.ell(r.f(x0, x1), r.f(y0, y1), r.f(1.0f, 2.8f), r.f(0.7f, 1.6f), rgba(rgb(70, 54, 38), 110), 1, 0);
}

void soldierDetail(Art& A, bool cy, int local) {
    bool heavy = local == 4 || (!cy && local == 9);
    float sw = heavy ? 10.5f : 9.5f;
    if (cy) {
        for (int s = -1; s <= 1; s += 2) {
            if (!heavy) { A.ell(33.2f, 32 + s * (sw - 1.4f), 3.4f, 2.7f, shade(CYP, 1.05f), 1.6f, 0.55f); A.line(31.6f, 32 + s * (sw - 1.4f), 34.8f, 32 + s * (sw - 1.4f), 0.7f, rgba(CYG, 190)); }
            A.box(28.6f, 32 + s * 6.4f, 1.8f, 1.5f, 0.5f, shade(CYM, 1.4f), 1.0f, 0.5f); A.dot(28.6f, 32 + s * 6.4f, 0.55f, CYG);   // hip emitters
            A.dot(31.3f, 32 + s * 3.6f, 0.8f, rgba(CYG, 220));                                                                            // ear comms
        }
        A.box(35.6f, 32, 1.6f, 3.2f, 0.6f, shade(CYM, 1.35f), 1.1f, 0.5f);       // chest rig
        A.line(29, 32 - sw + 2.2f, 29, 32 + sw - 2.2f, 0.8f, rgba(CYD, 190));    // waist band
    } else {
        for (int s = -1; s <= 1; s += 2) {
            if (!heavy) A.ell(33.2f, 32 + s * (sw - 1.4f), 3.2f, 2.6f, shade(CKH, 0.78f), 1.6f, 0.55f);
            A.box(29.2f, 32 + s * 6.2f, 1.9f, 1.6f, 0.5f, shade(CKT, 0.82f), 1.0f, 0.5f);        // belt pouches
            A.dot(36.2f, 32 + s * 2.6f, 0.7f, rgba(CKD, 160));                                    // helmet strap rivets
        }
        A.line(30.4f, 32 - sw + 2.4f, 36.4f, 32 + sw - 2.4f, 0.8f, rgba(CKD, 140));  // chest straps in an X
        A.line(30.4f, 32 + sw - 2.4f, 36.4f, 32 - sw + 2.4f, 0.8f, rgba(CKD, 140));
        A.circle(24.8f, 32 + 7.2f, 1.6f, shade(CKS, 0.95f), 1.2f, 0.55f);           // canteen
        A.cap(24.2f, 32 - 6.0f, 24.2f, 32 + 4.0f, 0.9f, shade(CKR, 0.9f), 0.8f, 0.4f);   // rolled blanket on the pack
        A.ell(33.5f, 32, 5.0f, 5.2f, rgba(CKD, 0), 1, 0);
        for (int i = 0; i < 5; i++) { float an = 3.7f + i * 0.5f; A.dot(33.5f + std::cos(an) * 3.6f, 32 + std::sin(an) * 3.6f, 0.55f, rgba(CKD, 150)); }   // helmet netting
    }
    scratches(A, 28, 24, 38, 40, 4, 100 + local + (cy ? 0 : 20), 60);
}

void vehicleDetail(Art& A, bool cy, int local) {
    switch (local) {
    case 0:   // dozer: the sensors, lamps and field dirt the body does not carry
        if (cy) {
            A.dot(29.6f, 36.2f, 0.8f, rgba(CYD, 230)); A.dot(29.6f, 36.2f, 0.4f, rgba(CYG, 220));   // comms dome on the cab's rear corner
        } else {
            mud(A, 16, 17, 44, 23, 6, 51); mud(A, 16, 41, 44, 47, 6, 52);   // dirt on the track bands
            A.ell(30, 37.6f, 4.0f, 1.1f, rgba(CKR, 80), 1, 0);   // rust on the hull
            A.ell(44.0f, 31.4f, 1.6f, 1.0f, rgba(CKR, 90), 1, 0);   // rust on the hood
            A.grain(47, 18, 55, 46, 0.22f, 61);   // dirt mottling on the blade
        }
        break;
    case 1:   // hauler: the placard, road dirt and rust the body does not carry
        if (cy) {
            A.box(36.0f, 26.4f, 1.8f, 1.2f, 0.3f, rgb(236, 232, 210), 0.6f, 0.2f);   // ID placard on the front container
            A.line(34.9f, 25.6f, 37.1f, 25.6f, 0.4f, rgba(CYD, 200)); A.line(34.9f, 27.2f, 36.6f, 27.2f, 0.4f, rgba(CYD, 200));
        } else {
            A.grain(6, 15, 42, 49, 0.25f, 41);   // road dirt mottling on the wheels and mudguards
            A.ell(46.0f, 37.4f, 2.6f, 1.0f, rgba(CKR, 90), 1, 0);   // rust on the cab roof
            A.ell(24.0f, 22.0f, 3.2f, 0.7f, rgba(CKR, 80), 1, 0);   // rust on the sideboard
        }
        break;
    case 5:   // main battle tank hull: skirt bolts and joints, driver's hatch and periscopes, rear deck vents and stowage
        if (cy) {
            A.line(12, 16.7f, 45, 16.7f, 0.5f, rgba(CYD, 230)); A.line(12, 47.3f, 45, 47.3f, 0.5f, rgba(CYD, 230));   // dark gap between the skirts and the track band
            rivets(A, 14, 17.2f, 44, 17.2f, 9, shade(CYP, 1.1f)); rivets(A, 14, 46.8f, 44, 46.8f, 9, shade(CYP, 1.1f));   // skirt bolts
            for (int i = 0; i < 6; i++) { float x = 16 + i * 5.0f; A.line(x, 16.8f, x, 20.2f, 0.5f, rgba(CYD, 170)); A.line(x, 43.8f, x, 47.2f, 0.5f, rgba(CYD, 170)); }   // skirt panel joints
            for (int i = 0; i < 5; i++) A.line(12.4f + i * 1.8f, 22.6f, 12.4f + i * 1.8f, 41.4f, 0.6f, rgba(CYD, 190));   // engine deck louvres
            for (int s = -1; s <= 1; s += 2) { A.ell(12.2f, 32 + s * 9.6f, 1.6f, 1.1f, shade(CYD, 1.4f), 1, 0.3f); A.box(14.8f, 32 + s * 13.2f, 2.2f, 1.5f, 0.4f, shade(CYM, 1.3f), 1.0f, 0.5f); }   // exhaust outlets and rear stowage boxes
            hatch(A, 50.2f, 32, 2.2f, shade(CYH, 1.2f));   // driver's hatch
            for (int s = -1; s <= 1; s += 2) { A.box(48.4f, 32 + s * 5.8f, 0.9f, 0.9f, 0.3f, CYD, 0.6f, 0.3f); A.dot(48.4f, 32 + s * 5.8f, 0.4f, rgba(CYW, 230)); }   // periscopes
        } else {
            A.line(12, 16.7f, 45, 16.7f, 0.5f, rgba(CKD, 230)); A.line(12, 47.3f, 45, 47.3f, 0.5f, rgba(CKD, 230));   // dark gap between the skirts and the track band
            rivets(A, 12, 17.2f, 46, 17.2f, 11, shade(CKH, 1.3f)); rivets(A, 12, 46.8f, 46, 46.8f, 11, shade(CKH, 1.3f));   // skirt bolts
            for (int i = 0; i < 4; i++) A.line(12.6f + i * 2.0f, 24.2f, 12.6f + i * 2.0f, 39.8f, 0.9f, rgba(CKD, 210));   // rear deck louvres
            hatch(A, 49.8f, 32, 2.2f, shade(CKH, 1.1f));   // driver's hatch
            for (int s = -1; s <= 1; s += 2) { A.box(47.4f, 32 + s * 5.0f, 0.9f, 0.9f, 0.3f, CKD, 0.6f, 0.3f); A.dot(47.4f, 32 + s * 5.0f, 0.4f, rgba(CKY, 200)); }   // periscopes
            for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) A.box(44.5f + i * 2.2f, 32 + s * 9.2f, 1.0f, 1.0f, 0.3f, shade(CKS, 0.95f), 0.8f, 0.5f);   // spare track links on the glacis
            A.circle(12.6f, 26.5f, 2.4f, shade(CKR, 0.65f), 1.6f, 0.6f); A.circle(12.6f, 37.5f, 2.4f, shade(CKR, 0.65f), 1.6f, 0.6f);   // fuel drums, sun-faded rust
            A.line(10.4f, 26.5f, 14.6f, 26.5f, 0.7f, rgba(CKD, 140)); A.line(10.4f, 37.5f, 14.6f, 37.5f, 0.7f, rgba(CKD, 140));
            A.ell(17.0f, 23.4f, 2.2f, 1.1f, shade(CKD, 0.9f), 1, 0.5f); A.ell(17.0f, 40.6f, 2.2f, 1.1f, shade(CKD, 0.9f), 1, 0.5f);   // exhaust outlets
            A.ell(36, 36, 3.6f, 1.1f, rgba(CKR, 80), 1, 0); A.ell(22, 46.2f, 3.2f, 0.8f, rgba(CKR, 70), 1, 0);   // rust on the cab and skirt
            mud(A, 12, 14, 50, 20, 6, 51); mud(A, 12, 44, 50, 50, 6, 52);   // road dust on the tracks and skirts
        }
        break;
    case 7:   // railgun tank / rocket launcher carrier
        if (cy) {   // railgun: skirt joints and bolts, rear deck vents, a driver's hatch with periscopes on the glacis
            for (int i = 0; i < 6; i++) { float x = 13 + i * 5.6f; A.line(x, 16.7f, x, 19.8f, 0.5f, rgba(CYD, 190)); A.line(x, 43.8f, x, 46.9f, 0.5f, rgba(CYD, 190)); }
            rivets(A, 12, 17.0f, 42, 17.0f, 8, shade(CYP, 1.15f)); rivets(A, 12, 47.0f, 42, 47.0f, 8, shade(CYP, 1.15f));
            for (int i = 0; i < 4; i++) A.line(8.2f + i * 1.5f, 25.0f, 8.2f + i * 1.5f, 39.0f, 0.5f, rgba(CYD, 200));   // rear deck vents
            hatch(A, 47.6f, 26.8f, 1.9f, shade(railComp(), 1.2f));   // driver's hatch
            for (int s = -1; s <= 1; s += 2) A.box(50.4f, 32 + s * 5.2f, 0.8f, 0.8f, 0.3f, CYD, 0.5f, 0.2f);   // periscopes
        } else {    // launcher: skirt bolts, cab mirrors and a commander's hatch on the sand roof, yellow tow points, a rust streak and road dust
            rivets(A, 12, 17.0f, 44, 17.0f, 9, rgb(118, 124, 88)); rivets(A, 12, 47.0f, 44, 47.0f, 9, rgb(118, 124, 88));
            for (int s = -1; s <= 1; s += 2) { A.line(48.8f, 32 + s * 7.6f, 51.4f, 32 + s * 10.4f, 0.6f, rgba(CKD, 230)); A.box(51.6f, 32 + s * 10.9f, 1.2f, 0.9f, 0.3f, rgb(30, 32, 24), 0.7f, 0.3f); }   // cab mirrors
            hatch(A, 49.2f, 28.0f, 1.7f, rgb(150, 134, 96));   // commander's hatch
            A.dot(7.4f, 25.6f, 0.8f, rgb(226, 182, 62)); A.dot(7.4f, 38.4f, 0.8f, rgb(226, 182, 62));   // tow points
            A.ell(46.0f, 22.2f, 2.6f, 0.9f, rgba(rgb(158, 86, 44), 85), 1, 0);   // rust streak on the chassis
            mud(A, 8, 14, 52, 20, 6, 91); mud(A, 8, 44, 52, 50, 6, 92);
        }
        break;
    case 6:   // gatling tank (walker keeps its own detail)
        if (!cy) {   // gatling: skirt joints and bolts, driver's hatch and periscopes, exhaust outlets, rust and road dust
            for (int i = 0; i < 6; i++) { float x = 16 + i * 5.4f; A.line(x, 16.7f, x, 19.8f, 0.5f, rgba(CKD, 190)); A.line(x, 44.2f, x, 47.3f, 0.5f, rgba(CKD, 190)); }
            rivets(A, 13, 17.0f, 44, 17.0f, 9, rgb(120, 126, 90)); rivets(A, 13, 47.0f, 44, 47.0f, 9, rgb(120, 126, 90));
            A.ell(12.8f, 24.4f, 1.6f, 1.0f, rgb(40, 40, 34), 1, 0.3f); A.ell(12.8f, 39.6f, 1.6f, 1.0f, rgb(40, 40, 34), 1, 0.3f);   // exhaust outlets
            hatch(A, 44.2f, 26.4f, 1.9f, rgb(104, 112, 76));   // driver's hatch
            for (int s = -1; s <= 1; s += 2) { A.box(47.2f, 32 + s * 3.4f, 0.7f, 0.7f, 0.2f, CKD, 0.5f, 0.2f); A.dot(47.2f, 32 + s * 3.4f, 0.35f, rgba(CKY, 200)); }   // periscopes
            A.ell(36, 37.4f, 3.6f, 1.1f, rgba(rgb(158, 86, 44), 85), 1, 0); A.ell(22, 24.8f, 3.0f, 1.0f, rgba(rgb(158, 86, 44), 70), 1, 0);   // rust streaks
            mud(A, 12, 14, 50, 20, 6, 61); mud(A, 12, 44, 50, 50, 6, 62);
        }
        else { for (int i = 0; i < 6; i++) { float an = i * 1.0472f + 0.5f; A.dot(32 + std::cos(an) * 6.0f, 32 + std::sin(an) * 6.0f, 0.5f, rgba(CYW, 180)); } }
        break;
    case 10:  // super-heavy
        if (cy) {
            for (int s = -1; s <= 1; s += 2) { A.box(30, 32 + s * 12.4f, 22, 1.0f, 0.4f, shade(CYP, 1.1f), 1.0f, 0.55f); A.box(46.5f, 32 + s * 9.0f, 3.0f, 1.8f, 0.6f, shade(CYM, 1.4f), 1.2f, 0.55f); A.dot(48.5f, 32 + s * 9.0f, 0.8f, rgb(255, 214, 124)); A.glow(48.5f, 32 + s * 9.0f, 3, rgba(rgb(255, 214, 124), 120)); }
            for (int i = 0; i < 5; i++) A.dot(11 + i * 7.0f, 22.0f, 0.5f, rgba(CYG, 190)), A.dot(11 + i * 7.0f, 42.0f, 0.5f, rgba(CYG, 190));
        } else {
            for (int s = -1; s <= 1; s += 2) { A.line(10, 32 + s * 11.6f, 50, 32 + s * 11.6f, 0.8f, rgba(CKD, 190)); for (int i = 0; i < 4; i++) A.dot(14 + i * 10.0f, 32 + s * 11.6f, 0.8f, shade(CKS, 1.15f)); }
            A.box(32, 32, 0.8f, 7.5f, 0.3f, rgba(CKY, 150), 0.6f, 0.2f);
            A.ell(20, 30, 3.0f, 1.4f, rgba(CKR, 90), 1, 0);
            mud(A, 8, 14, 52, 20, 7, 71); mud(A, 8, 44, 52, 50, 7, 72);
        }
        break;
    }
}

// local contrast: an unsharp mask on the opaque pixels (alpha-weighted so the sprite edge does not halo), which crisps panel seams and rivets at display size
void enhance(Canvas& c, float amount) {
    std::vector<u32> src = c.px;
    auto at = [&](int x, int y) { return Canvas::unpack(src[clampi(y, 0, c.h - 1) * c.w + clampi(x, 0, c.w - 1)]); };
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        Color p = at(x, y);
        if (p.a < 200) continue;
        float r = 0, g = 0, b = 0, w = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            Color q = at(x + dx, y + dy);
            if (q.a < 200) continue;
            float k = (dx == 0 && dy == 0) ? 2.0f : 1.0f;
            r += q.r * k; g += q.g * k; b += q.b * k; w += k;
        }
        r /= w; g /= w; b /= w;
        Color o{(u8)clampf(p.r + amount * (p.r - r), 0, 255), (u8)clampf(p.g + amount * (p.g - g), 0, 255), (u8)clampf(p.b + amount * (p.b - b), 0, 255), p.a};
        c.px[y * c.w + x] = Canvas::pack(o);
    }
}

}  // namespace

// ---------------------------------------------------------------- public entry points
int artCanvas(int type) { int l = unitLocal(type); return l == 29 || l == 30 ? ART_BIG : ART_SIZE; }
void artUnitBody(Canvas& c, int type, Color team, int frame) {
    setTone(type);
    Art A(c, team);
    bool cy = UNITS[type].faction == F_CYBER;
    int local = unitLocal(type);
    switch (local) {
    case 0: if (cy) dozerCyber(A, frame); else dozerClanker(A, frame); break;
    case 1: if (cy) harvCyber(A, frame); else harvClanker(A, frame); break;
    case 2: case 3: case 4: soldier(A, cy, local, frame); break;
    case 5: if (cy) tankCyber(A, frame); else tankClanker(A, frame); break;
    case 6: if (cy) walkerCyber(A, frame); else gatlingBody(A, frame); break;
    case 7: if (cy) tankCyberRail(A, frame); else launcherBody(A, frame); break;
    case 8: if (cy) droneBody(A); else gunshipBody(A); break;
    case 9: soldier(A, cy, local, frame); break;
    case 10: if (cy) titanCyber(A, frame); else behemothBody(A, frame); break;
    case 11: if (cy) jetCyber(A); else jetClanker(A); break;
    case 12: medicBody(A, cy, frame); break;
    case 13: sniperBody(A, cy, frame); break;
    case 14: hornetBody(A); break;
    case 15: spyBody(A, cy, frame); break;
    case 16: spyDroneBody(A, cy); break;
    case 17: cargoBody(A, cy); break;
    case 18: artJeepBody(c, team, cy, frame); break;           // the jeeps, ships and strike fighters have their own files (art_jeep.cpp, art_ship.cpp, art_jet.cpp)
    case 19: artShipBody(c, team, cy, false); break;
    case 20: artShipBody(c, team, cy, true); break;
    case 21: artAirstrikeJetBody(c, team, cy); break;        // the air strike's light-grey fighters (art_ocean.cpp)
    case 23: artSupersonicBomberBody(c, team, cy); break;   // Supersonic Bomber (art_jet.cpp)
    case 25: artFighterBody(c, team, cy); break;            // plain fighter (art_jet.cpp)
    case 26: artNukeBomberBody(c, team, cy); break;         // call-in strategic bomber (art_jet.cpp)
    case 27: artPatrolBoatBody(c, team, cy); break;         // patrol boat (art_ship.cpp)
    case 29: artCarrierBody(c, team, cy); break;            // aircraft carrier (art_ocean.cpp)
    case 30: artCargoShipBody(c, team, cy); break;          // cargo ship (art_ocean.cpp)
    case 31: artCarrierJetBody(c, team, cy); break;         // carrier jet (art_ocean.cpp)
    case 32: artCommandoBody(c, team, cy, frame); break;    // laser commando (art_commando.cpp)
    case 28: artRioterBody(c, team, type == U_FIREBOMBER ? 1 : 0, frame); break;   // civilian rioters (art_riot.cpp)
    case 24:   // Anti-Radiation Truck: a ground vehicle, so it paints its own detail and finishes with the generic layers' enhance/outline
        radTruckBody(A, cy, frame);
        enhance(c, 0.7f);
        c.outline(rgb(8, 10, 14, 200));
        break;
    case 22:   // civilian dozer: the army's dozer in civic white and safety orange, with a roof light bar
        if (cy) { CYH = rgb(214, 220, 226); CYP = rgb(236, 240, 244); CYM = rgb(232, 120, 44); CYD = rgb(70, 74, 82); CYG = rgb(255, 176, 60); CYW = rgb(255, 236, 200); dozerCyber(A, frame); }
        else { CKY = rgb(236, 128, 40); CKH = rgb(226, 222, 210); CKS = rgb(150, 152, 150); CKD = rgb(52, 50, 46); dozerClanker(A, frame); }
        A.box(cy ? 34 : 35, 32, 1.6f, 6.2f, 0.6f, rgb(40, 44, 52), 1.0f, 0.4f);   // light bar across the cab roof: amber and white
        A.dot(cy ? 34 : 35, 29.6f, 0.9f, rgb(255, 170, 40)); A.dot(cy ? 34 : 35, 34.4f, 0.9f, rgb(255, 255, 255));
        A.glow(cy ? 34 : 35, 29.6f, 3.5f, rgba(rgb(255, 170, 40), 120));
        break;
    }
    if (local >= 18 && local != 22) return;   // (they paint, enhance and outline themselves)
    if (local == 22) local = 0;                // (the civilian dozers take the dozer's detail layers)
    // the new designs draw all their own detail, so the generic soldier and vehicle layers stay off them
    if (local == 2 || local == 3 || local == 4 || local == 9) soldierDetail(A, cy, local);
    else if (local != 8 && local <= 10) vehicleDetail(A, cy, local);
    enhance(c, local == 8 || local == 11 || local == 14 || local == 16 || local == 17 ? 0.45f : 0.7f);
    if (local == 8 && !cy) c.outline(rgb(10, 12, 14, 150)); else c.outline(rgb(8, 10, 14, 200));
}

// Cargo plane of the paradrop (not a unit type: a transport that crosses the map and releases its load). Faces +x, 64x64 canvas, wingspan 56.
void artCargoPlane(Canvas& c, Color team, bool cy) {
    setTone(-1);
    Art A(c, team);
    Color hull = cy ? shade(CYH, 1.05f) : shade(CKH, 1.0f), pan = cy ? shade(CYP, 1.05f) : shade(CKH, 1.2f), dark = cy ? CYD : CKD, acc = cy ? CYG : CKY;
    // high swept wings, tailplanes
    for (int s = -1; s <= 1; s += 2) {
        auto Y = [&](float y) { return 32 + s * (y - 32); };
        A.poly({ {39, Y(27)}, {34, Y(4)}, {25.5f, Y(4)}, {24, Y(27)} }, hull, 2.4f, 0.55f);
        A.poly({ {37, Y(26)}, {33.5f, Y(8)}, {28, Y(8)}, {26.5f, Y(26)} }, pan, 1.6f, 0.4f);
        A.line(30, Y(27), 29.5f, Y(5), 0.7f, rgba(dark, 140));
        A.poly({ {14, Y(28)}, {10.5f, Y(14.5f)}, {5.5f, Y(14.5f)}, {7.5f, Y(28)} }, hull, 1.8f, 0.5f);
        // two engines per wing
        for (int k = 0; k < 2; k++) {
            float ey = Y(32 + (k ? 21.0f : 11.0f)), ex = k ? 33.0f : 37.0f;
            A.cap(ex - 6, ey, ex + 5, ey, 2.4f, shade(dark, 1.6f), 1.6f, 0.55f);
            if (cy) { A.dot(ex - 7.2f, ey, 1.7f, acc); A.glow(ex - 8.5f, ey, 6, rgba(acc, 190)); }
            else { A.circle(ex + 6.4f, ey, 4.4f, rgba(rgb(210, 214, 220), 70), 1, 0); A.dot(ex + 5.4f, ey, 1.0f, dark); }
        }
    }
    // fuselage with a glazed nose, loading ramp and a fin
    A.cap(8, 32, 55, 32, 6.0f, hull, 3.4f, 0.6f);
    A.ell(57.5f, 32, 5.5f, 4.2f, shade(hull, 1.08f), 2.2f, 0.5f);
    A.ell(55.5f, 32, 3.4f, 2.7f, rgb(60, 92, 112), 1.8f, 0.7f); A.ell(54.7f, 31, 1.7f, 0.9f, rgba(rgb(200, 230, 245), 180), 1, 0);
    A.cap(15, 32, 45, 32, 4.4f, pan, 2.2f, 0.4f);
    for (int i = 0; i < 4; i++) A.line(19 + i * 7, 27.6f, 19 + i * 7, 36.4f, 0.7f, rgba(dark, 150));
    A.box(8.5f, 32, 3.6f, 4.6f, 1.2f, shade(dark, 1.3f), 1.4f, 0.5f);
    A.box(10, 32, 5, 1.2f, 0.5f, shade(hull, 1.4f), 1.0f, 0.5f);      // tail fin seen from above
    A.line(10, 30.4f, 18, 30.4f, 0.7f, rgba(acc, 200)); A.line(10, 33.6f, 18, 33.6f, 0.7f, rgba(acc, 200));
    teamDot(A, 38.0f, 32, 2.9f);
    teamDot(A, 30.0f, 8.5f, 1.8f); teamDot(A, 30.0f, 55.5f, 1.8f);
    A.dot(26.5f, 5.0f, 0.9f, rgb(255, 80, 70)); A.dot(26.5f, 59.0f, 0.9f, rgb(80, 255, 120));
    A.grain(6, 4, 62, 60, 0.12f, cy ? 301 : 302);
    enhance(c, 0.5f);
    c.outline(rgb(8, 10, 14, 190));
}

// Relief plane of the Aid Drop (the human player's own power): a big white four-engine transport with a sky-blue cheatline and aid
// roundels on the wings, nobody's war colours. Faces +x, 64x64 canvas, wingspan 60.
void artAidPlane(Canvas& c) {
    setTone(-1);
    Art A(c, rgb(90, 170, 235));
    const Color hull = rgb(236, 238, 240), pan = rgb(214, 218, 224), dark = rgb(92, 100, 112), blue = rgb(70, 150, 225), sky = rgb(150, 205, 245);
    for (int s = -1; s <= 1; s += 2) {
        auto Y = [&](float y) { return 32 + s * (y - 32); };
        A.poly({ {40, Y(27)}, {35, Y(2.5f)}, {25, Y(2.5f)}, {23.5f, Y(27)} }, hull, 2.4f, 0.5f);           // long straight wing
        A.poly({ {38, Y(26)}, {34, Y(7)}, {28, Y(7)}, {26.5f, Y(26)} }, pan, 1.6f, 0.35f);
        A.line(26, Y(4), 34.6f, Y(4), 1.1f, rgba(blue, 220));                                         // blue wing tips
        A.poly({ {13.5f, Y(28.5f)}, {10, Y(14)}, {4.5f, Y(14)}, {6.5f, Y(28.5f)} }, hull, 1.8f, 0.45f);  // tailplane
        for (int k = 0; k < 2; k++) {                                                                // four turboprops
            float ey = Y(32 + (k ? 20.5f : 11.0f)), ex = k ? 34.0f : 38.0f;
            A.cap(ex - 6.5f, ey, ex + 5, ey, 2.5f, shade(hull, 0.92f), 1.6f, 0.5f);
            A.circle(ex + 6.6f, ey, 4.6f, rgba(rgb(200, 206, 214), 70), 1, 0);
            A.dot(ex + 5.6f, ey, 1.1f, dark);
        }
        // aid roundel on each wing: a sky-blue ring around a white disc with a blue dot
        A.circle(29.5f, Y(15.5f), 4.6f, blue, 1.2f, 0.4f);
        A.circle(29.5f, Y(15.5f), 3.2f, rgb(250, 250, 250), 1.0f, 0.3f);
        A.dot(29.5f, Y(15.5f), 1.4f, blue);
    }
    A.cap(7, 32, 56, 32, 6.4f, hull, 3.4f, 0.55f);                                                   // fuselage
    A.ell(58, 32, 5.6f, 4.4f, shade(hull, 1.04f), 2.2f, 0.45f);
    A.ell(56, 32, 3.4f, 2.7f, rgb(64, 96, 122), 1.8f, 0.7f); A.ell(55.2f, 31, 1.7f, 0.9f, rgba(rgb(210, 235, 250), 190), 1, 0);
    A.cap(12, 32, 50, 32, 4.4f, pan, 2.0f, 0.35f);
    A.line(10, 29.2f, 52, 29.2f, 1.0f, rgba(sky, 230)); A.line(10, 34.8f, 52, 34.8f, 1.0f, rgba(sky, 230));   // cheatline
    for (int i = 0; i < 5; i++) A.line(17 + i * 7, 28.2f, 17 + i * 7, 35.8f, 0.6f, rgba(dark, 110));
    A.box(9, 32, 3.8f, 1.3f, 0.5f, rgb(246, 248, 250), 1.0f, 0.4f);                                  // T-tail fin seen from above
    A.box(9.2f, 32, 1.4f, 1.0f, 0.4f, blue, 1.0f, 0.3f);
    A.dot(25.5f, 3.4f, 0.9f, rgb(255, 80, 70)); A.dot(25.5f, 60.6f, 0.9f, rgb(80, 255, 120));
    A.grain(4, 2, 63, 62, 0.06f, 331);
    enhance(c, 0.45f);
    c.outline(rgb(40, 46, 56, 200));
}

// Wooden aid crate seen from above, 64x64: a big plank box with a frame, cross braces, steel corners and a sky-blue band.
// open: the lid is off and the supplies show (bundles of cash, sacks, tool boxes).
void artCrate(Canvas& c, bool open) {
    setTone(-1);
    Art A(c, rgb(90, 170, 235));
    const Color wood = rgb(176, 128, 74), plank = rgb(196, 148, 90), dark = rgb(92, 62, 34), steel = rgb(150, 156, 162), blue = rgb(80, 160, 230);
    A.box(32, 32, 19, 19, 1.6f, wood, 2.4f, 0.5f);
    if (!open) {
        for (int i = 0; i < 5; i++) A.box(32, 17.5f + i * 7.25f, 16.4f, 3.0f, 0.6f, shade(plank, 0.92f + 0.05f * (i % 3)), 1.2f, 0.35f);   // lid planks
        A.line(15.5f, 15.5f, 48.5f, 48.5f, 3.2f, shade(wood, 0.85f));                                       // cross brace
        A.line(15.5f, 48.5f, 48.5f, 15.5f, 3.2f, shade(wood, 0.85f));
        A.line(15.5f, 15.5f, 48.5f, 48.5f, 1.0f, rgba(shade(plank, 1.12f), 160));
        A.box(32, 32, 19, 2.6f, 0.4f, blue, 1.0f, 0.3f);                                                   // band
        A.box(32, 32, 5.2f, 5.2f, 0.8f, rgb(246, 246, 240), 1.0f, 0.3f);                                    // the aid mark
        A.dot(32, 32, 2.6f, blue);
    } else {
        A.box(32, 32, 16, 16, 0.8f, shade(dark, 0.8f), 1.4f, 0.6f);                                          // inside
        for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++)                                             // cash bundles
            A.box(24 + i * 6.5f, 23 + j * 5.2f, 2.9f, 2.2f, 0.4f, rgb(110 + 10 * ((i + j) % 2), 170, 100), 1.0f, 0.35f);
        for (int i = 0; i < 3; i++) for (int j = 0; j < 2; j++) A.line(21.5f + i * 6.5f, 23 + j * 5.2f, 26.5f + i * 6.5f, 23 + j * 5.2f, 0.6f, rgba(rgb(240, 240, 220), 200));
        A.ell(25, 39, 5.5f, 4.2f, rgb(200, 186, 150), 1.6f, 0.45f);                                         // sacks
        A.ell(36, 40, 5.0f, 4.0f, rgb(188, 174, 138), 1.6f, 0.45f);
        A.box(42, 30, 3.6f, 7, 0.6f, rgb(200, 70, 50), 1.2f, 0.4f);                                         // tool box
        A.line(15, 13.5f, 49, 13.5f, 2.6f, shade(plank, 0.9f));                                             // broken lid planks on the edge
        A.line(13.5f, 50.5f, 30, 50.5f, 2.6f, shade(plank, 0.86f));
    }
    for (int i = 0; i < 4; i++) { float x = i & 1 ? 49 : 15, y = i & 2 ? 49 : 15; A.box(x, y, 3.0f, 3.0f, 0.5f, steel, 1.0f, 0.4f); A.dot(x, y, 0.8f, shade(steel, 0.6f)); }
    A.grain(12, 12, 52, 52, 0.18f, open ? 342 : 341);
    enhance(c, 0.4f);
    c.outline(rgb(30, 22, 14, 200));
}

// Parachute canopy seen from above: eight gores alternating the owner's colour and off-white, seams, a vent
void artChute(Canvas& c, Color team) {
    Art A(c, team);
    const float R = 23.0f;
    A.circle(32, 32, R, rgb(232, 230, 220), 3.6f, 0.55f);
    for (int i = 0; i < 8; i += 2) {
        float a0 = i * 0.785398f - 0.3927f, a1 = a0 + 0.785398f;
        A.poly({ {32, 32}, {32 + std::cos(a0) * (R - 0.8f), 32 + std::sin(a0) * (R - 0.8f)}, {32 + std::cos((a0 + a1) * 0.5f) * (R - 0.3f), 32 + std::sin((a0 + a1) * 0.5f) * (R - 0.3f)}, {32 + std::cos(a1) * (R - 0.8f), 32 + std::sin(a1) * (R - 0.8f)} }, team, 1.2f, 0.25f);
    }
    for (int i = 0; i < 8; i++) { float a = i * 0.785398f - 0.3927f; A.line(32, 32, 32 + std::cos(a) * R, 32 + std::sin(a) * R, 0.6f, rgba(rgb(60, 58, 54), 130)); }
    A.circle(32, 32, 2.6f, rgb(58, 56, 52), 1.2f, 0.4f);
    A.grain(8, 8, 56, 56, 0.1f, 311);
    c.outline(rgb(20, 22, 26, 160));
}

bool artUnitTurret(Canvas& c, int type, Color team) {
    setTone(type);
    Art A(c, team);
    bool cy = UNITS[type].faction == F_CYBER;
    int local = unitLocal(type);
    switch (local) {
    case 19: case 20: return artShipMount(c, team, cy, local == 20);   // (the warship mounts; the jeeps and fighters have no rotating part)
    case 27: return artPatrolBoatMount(c, team, cy);                  // (the patrol boat's gun and rocket mount)
    case 5:
        if (cy) {   // Photon Tank turret: a low, faceted composite wedge with slab sides, roof hatches and a slim energy gun
            Color comp = mix(CYH, rgb(92, 104, 120), 0.7f), panel = shade(comp, 1.14f);
            A.shadowCircle(32, 32, 11.5f, 1.0f, 1.2f, 1.8f, 90);   // the turret's cast shadow on the hull
            A.box(14.4f, 32, 3.0f, 6.0f, 0.8f, shade(CYM, 1.2f), 1.4f, 0.5f);   // bustle rack at the rear
            for (int i = 0; i < 3; i++) A.line(12.6f + i * 1.8f, 26.6f, 12.6f + i * 1.8f, 37.4f, 0.5f, rgba(CYD, 170));   // stowage netting
            A.poly({ {17.5f, 22.8f}, {37.5f, 21.6f}, {43.5f, 23.2f}, {46.8f, 27.2f}, {47.4f, 32}, {46.8f, 36.8f}, {43.5f, 40.8f}, {37.5f, 42.4f}, {17.5f, 41.2f} }, shade(comp, 0.9f), 2.6f, 0.7f);   // slab-sided body
            A.poly({ {20, 24.2f}, {36.5f, 23.2f}, {39.6f, 24.0f}, {39.6f, 40.0f}, {36.5f, 40.8f}, {20, 39.8f} }, shade(comp, 1.3f), 1.6f, 0.6f);   // lit roof plate
            A.poly({ {39.2f, 24.0f}, {43.5f, 23.2f}, {46.8f, 27.2f}, {47.4f, 32}, {46.8f, 36.8f}, {43.5f, 40.8f}, {39.2f, 40.0f} }, shade(comp, 0.78f), 1.6f, 0.8f);   // sloped front wedge, turned away from the light
            A.line(22, 29.6f, 36.5f, 29.1f, 0.5f, rgba(CYD, 170)); A.line(22, 34.4f, 36.5f, 34.9f, 0.5f, rgba(CYD, 170));   // composite panel seams
            A.line(18, 23.4f, 37, 22.4f, 0.6f, rgba(CYD, 200)); A.line(18, 40.6f, 37, 41.6f, 0.6f, rgba(CYD, 200));   // side bevel lines
            for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) A.cap(40.6f, 32 + s * (8.4f + i * 0.9f), 43.4f, 32 + s * (8.4f + i * 0.9f), 0.6f, shade(CYD, 1.8f), 0.6f, 0.4f);   // smoke grenade launchers
            A.box(45.0f, 32, 2.0f, 4.0f, 1.2f, shade(comp, 0.85f), 1.4f, 0.5f);   // gun mantlet
            A.line(44.5f, 29.8f, 58, 29.8f, 0.9f, rgba(CYM, 230)); A.line(44.5f, 34.2f, 58, 34.2f, 0.9f, rgba(CYM, 230));   // coolant and power conduits
            A.cap(46, 32, 60.6f, 32, 1.2f, shade(CYM, 1.25f), 1.0f, 0.5f);   // slim barrel
            A.line(47.5f, 31.0f, 59.5f, 31.0f, 0.4f, rgba(CYP, 200));   // barrel highlight
            A.line(47.5f, 32.6f, 59.5f, 32.6f, 0.4f, rgba(CYG, 120));   // energy channel
            A.box(52.6f, 32, 1.4f, 2.0f, 0.5f, shade(CYM, 1.3f), 1.1f, 0.5f);   // fume extractor
            A.box(61.6f, 32, 1.3f, 2.0f, 0.5f, shade(CYP, 0.9f), 1.1f, 0.5f);   // emitter housing
            A.dot(63.1f, 32, 0.6f, CYG); A.glow(62.6f, 32, 2.4f, rgba(CYG, 110));   // tiny emitter tip
            hatch(A, 24.5f, 27.0f, 2.9f, panel);   // commander's cupola
            hatch(A, 24.5f, 38.0f, 2.2f, panel);   // loader's hatch
            A.box(37.4f, 27.6f, 1.6f, 1.4f, 0.5f, shade(CYM, 1.4f), 1.0f, 0.5f); A.dot(37.4f, 27.6f, 0.5f, rgba(CYW, 230));   // gunner's sight
            teamDot(A, 31.0f, 32, 2.5f);   // the owner's marking
            A.line(20.5f, 23.2f, 15.6f, 18.8f, 0.6f, rgba(CYW, 220));   // antenna
        } else {    // Brute Tank turret: a rounded cast turret with reactive armour bricks on the front flanks and a long 125mm gun
            A.shadowCircle(32, 32, 11.5f, 1.0f, 1.2f, 1.8f, 90);   // the turret's cast shadow on the hull
            A.box(16.8f, 32, 2.6f, 5.4f, 1.4f, shade(CKT, 0.82f), 1.4f, 0.5f);   // rear stowage bag
            A.poly({ {16.2f, 32}, {18.0f, 26.6f}, {23.5f, 22.6f}, {31.0f, 21.6f}, {38.0f, 22.8f}, {42.2f, 26.0f}, {43.0f, 32}, {42.2f, 38.0f}, {38.0f, 41.2f}, {31.0f, 42.4f}, {23.5f, 41.4f}, {18.0f, 37.4f} }, shade(CKH, 0.92f), 3.0f, 0.7f);   // rounded cast turret
            A.ell(29.8f, 32, 10.2f, 7.8f, shade(CKH, 1.12f), 3.0f, 0.55f);   // raised dome over the turret roof
            for (int s = -1; s <= 1; s += 2) { A.box(33.0f, 32 + s * 9.7f, 2.9f, 1.5f, 0.3f, shade(CKD, 1.5f), 1.0f, 0.5f); A.box(39.2f, 32 + s * 7.5f, 2.4f, 1.5f, 0.3f, shade(CKD, 1.5f), 1.0f, 0.5f); }   // reactive armour bricks
            A.box(44.0f, 32, 2.4f, 5.0f, 1.6f, shade(CKH, 0.9f), 1.6f, 0.5f);   // mantlet
            A.cap(44.0f, 32, 61.6f, 32, 1.35f, shade(CKD, 1.6f), 1.2f, 0.5f);   // barrel
            A.cap(46.0f, 32, 50.0f, 32, 1.9f, shade(CKD, 1.25f), 1.4f, 0.5f);   // thermal sleeve
            A.line(46, 30.9f, 61, 30.9f, 0.4f, rgba(CKS, 200));   // barrel highlight
            A.box(55.0f, 32, 1.4f, 2.0f, 0.5f, shade(CKD, 1.4f), 1.0f, 0.5f);   // fume extractor
            A.box(60.8f, 32, 1.9f, 2.8f, 0.6f, shade(CKD, 1.5f), 1.2f, 0.5f);   // muzzle brake
            hatch(A, 24.0f, 26.0f, 2.9f, shade(CKH, 0.82f));   // commander's cupola
            A.cap(24.0f, 26.0f, 31.5f, 26.0f, 0.8f, CKD, 1, 0.3f);   // pintle machine gun
            hatch(A, 24.5f, 38.5f, 2.2f, shade(CKH, 1.05f));   // loader's hatch
            A.box(40.5f, 27.5f, 1.8f, 1.4f, 0.5f, shade(CKD, 1.4f), 1, 0.5f);   // gunner's sight
            A.ell(28, 40.5f, 4.0f, 1.2f, rgba(CKR, 70), 1, 0);   // rust streak
            teamDot(A, 30.0f, 33.5f, 2.5f);   // the owner's marking
            A.line(18.5f, 25.0f, 14.6f, 19.2f, 0.7f, rgba(CKD, 230));   // antenna
            A.grain(14, 20, 48, 44, 0.16f, 12);
        }
        break;
    case 6:
        if (cy) {   // volt coil
            A.glow(32, 32, 15, rgba(CYG, 120));
            A.cap(36, 29, 55, 29, 1.5f, shade(CYM, 1.5f), 1.4f, 0.55f); A.cap(36, 35, 55, 35, 1.5f, shade(CYM, 1.5f), 1.4f, 0.55f);
            for (int i = 0; i < 4; i++) { float x = 40 + i * 4.2f; A.box(x, 32, 1.2f, 6.6f, 0.5f, i % 2 ? CYG : CYW, 1, 0.3f); }
            A.glow(56, 32, 6, rgba(CYW, 160)); A.dot(56, 32, 1.6f, CYW);
            A.circle(32, 32, 8.6f, CYP, 2.6f, 0.65f);
            A.circle(32, 32, 5.4f, shade(CYM, 1.2f), 2, 0.5f);
            A.circle(32, 32, 3.2f, CYW, 1.6f, 0.55f); A.glow(32, 32, 7, rgba(CYG, 160));
            for (int i = 0; i < 8; i++) { float a = i * 0.7854f; A.dot(32 + std::cos(a) * 7.0f, 32 + std::sin(a) * 7.0f, 0.9f, CYG); }
            teamDot(A, 25.5f, 32, 2.3f);
        } else {    // gatling: a boxy olive turret with a radar dish on the rear roof and a six-barrel rotary cannon in a shroud (ZSU-23-4 / 2S6 style)
            const Color olive = rgb(96, 104, 68), olD = rgb(44, 48, 32), steel = rgb(118, 120, 114);
            A.shadowCircle(32, 32, 11.5f, 1.0f, 1.2f, 1.8f, 90);   // the turret's cast shadow on the hull
            A.poly({ {21, 24.2f}, {37.4f, 23.4f}, {40.2f, 25.4f}, {40.2f, 38.6f}, {37.4f, 40.6f}, {21, 39.8f} }, olive, 2.6f, 0.7f);   // boxy turret
            A.poly({ {23.5f, 26.4f}, {36.2f, 25.8f}, {37.6f, 27.4f}, {37.6f, 36.6f}, {36.2f, 38.2f}, {23.5f, 37.6f} }, shade(olive, 1.18f), 1.6f, 0.6f);   // roof plate
            A.line(23.8f, 32, 36.0f, 32, 0.5f, rgba(olD, 160));   // roof seam
            A.box(29.5f, 43.4f, 4.4f, 2.2f, 0.6f, shade(olD, 1.4f), 1.2f, 0.5f);   // ammunition box on the turret side
            A.cap(33.4f, 41.4f, 38.4f, 38.4f, 0.6f, rgb(190, 152, 70), 0.8f, 0.3f);   // brass belt to the gun
            A.circle(18.8f, 32, 4.4f, shade(olive, 1.25f), 1.6f, 0.6f);   // radar dish on the rear roof
            A.ring(18.8f, 32, 3.0f, 0.8f, rgba(steel, 230), 0.6f, 0.4f);   // dish rim
            A.dot(18.8f, 32, 0.9f, olD);   // feed horn
            A.cap(41, 32, 54, 32, 2.8f, rgb(74, 78, 60), 1.4f, 0.55f);   // rotary cannon shroud
            A.line(42, 30.6f, 53, 30.6f, 0.5f, rgba(rgb(128, 134, 112), 170));   // shroud highlight
            A.circle(56, 32, 3.0f, steel, 1.2f, 0.5f);   // muzzle carrier
            for (int i = 0; i < 6; i++) { float a = i * 1.0472f; A.dot(56 + std::cos(a) * 1.5f, 32 + std::sin(a) * 1.5f, 0.55f, olD); }   // six barrels
            A.dot(56, 32, 0.7f, rgb(70, 72, 68));
            A.box(40.5f, 32, 2.0f, 4.8f, 1.0f, shade(olD, 1.5f), 1.4f, 0.5f);   // gun mount
            hatch(A, 26.0f, 26.0f, 2.2f, shade(olive, 1.1f));   // commander's hatch
            hatch(A, 26.0f, 38.2f, 1.8f, shade(olive, 1.05f));   // loader's hatch
            A.box(37.0f, 27.4f, 1.4f, 1.2f, 0.4f, shade(olD, 1.6f), 0.8f, 0.4f);   // gunner's sight
            teamDot(A, 30.4f, 32, 2.5f);   // the owner's marking
            A.grain(16, 22, 44, 42, 0.14f, 21);
        }
        break;
    case 7:
        if (cy) {   // railgun: a low composite turret with a sloped front and a long rail gun, two parallel conductor rails with coil rings across them and a slim muzzle
            const Color comp = railComp(), panel = shade(comp, 1.16f), rail = rgb(196, 206, 218);
            A.shadowCircle(32, 32, 11.5f, 1.0f, 1.2f, 1.8f, 90);   // the turret's cast shadow on the hull
            A.box(15.6f, 32, 2.4f, 5.8f, 0.8f, shade(CYM, 1.2f), 1.2f, 0.5f);   // power bus at the rear
            A.poly({ {17, 24}, {38, 24.5f}, {43, 28.5f}, {43, 35.5f}, {38, 39.5f}, {17, 40}, {14, 36}, {14, 28} }, comp, 2.6f, 0.6f);   // turret body
            A.poly({ {19.5f, 26.6f}, {36, 26.2f}, {39.4f, 28.6f}, {39.4f, 35.4f}, {36, 37.8f}, {19.5f, 37.4f} }, panel, 1.6f, 0.5f);   // roof plate
            A.line(20, 29.6f, 36, 29.4f, 0.5f, rgba(CYD, 170)); A.line(20, 34.4f, 36, 34.6f, 0.5f, rgba(CYD, 170));   // composite panel seams
            hatch(A, 22.5f, 27.8f, 2.2f, panel);   // commander's cupola
            hatch(A, 22.5f, 36.6f, 1.8f, panel);   // loader's hatch
            A.box(35.8f, 27.6f, 1.6f, 1.2f, 0.5f, shade(CYM, 1.4f), 0.8f, 0.4f); A.dot(35.8f, 27.6f, 0.45f, CYG);   // sensor pod with a tiny status light
            A.line(16.5f, 25.0f, 13.0f, 19.0f, 0.6f, rgba(CYW, 220));   // antenna
            A.box(41.6f, 32, 2.0f, 4.8f, 1.0f, shade(comp, 0.8f), 1.4f, 0.5f);   // breech housing
            A.cap(41, 28.7f, 61.5f, 28.7f, 1.2f, rail, 1.0f, 0.6f); A.cap(41, 35.3f, 61.5f, 35.3f, 1.2f, rail, 1.0f, 0.6f);   // conductor rails
            A.line(42, 32, 60, 32, 1.0f, rgba(CYD, 230));   // insulated spine between the rails
            for (float x : { 45.5f, 50.0f, 54.5f, 58.5f }) A.box(x, 32, 0.9f, 4.5f, 0.3f, shade(comp, 1.7f), 0.9f, 0.4f);   // coil rings across the rails
            A.dot(54.5f, 27.6f, 0.45f, CYG); A.dot(54.5f, 36.4f, 0.45f, CYG);   // the coil light, small
            A.box(62.0f, 32, 0.9f, 2.5f, 0.4f, shade(CYM, 1.4f), 0.8f, 0.4f);   // slim muzzle
            A.dot(62.9f, 32, 0.5f, CYG); A.glow(62.6f, 32, 2.6f, rgba(CYG, 80));
            teamDot(A, 32.5f, 32, 2.3f);   // the owner's marking
        } else {    // rocket pod: a sand-coloured launcher box with a 4x5 grid of tube mouths on a turntable (M270 / BM-21 style), yellow side rails
            const Color sand = rgb(170, 150, 106), olD = rgb(44, 48, 32), steel = rgb(126, 128, 122), hazard = rgb(214, 170, 52);
            A.shadowRect(11.0f, 19.4f, 33.0f, 25.2f, 1.0f, 1.2f, 1.8f, 90);   // the pod's cast shadow on the chassis
            A.box(28.0f, 32, 15.6f, 12.9f, 1.8f, shade(olD, 1.6f), 2.0f, 0.5f);   // dark pod shell
            A.box(28.0f, 32, 14.9f, 12.2f, 1.6f, sand, 2.0f, 0.55f);               // sand pod frame
            A.box(28.0f, 20.4f, 14.4f, 0.9f, 0.3f, hazard, 1.0f, 0.4f); A.box(28.0f, 43.6f, 14.4f, 0.9f, 0.3f, hazard, 1.0f, 0.4f);   // hazard-yellow side rails
            for (int r = 0; r < 4; r++) for (int c = 0; c < 5; c++) {
                float x = 28.0f + (c - 2) * 4.8f, y = 23.6f + r * 5.7f;
                A.circle(x, y, 2.0f, shade(steel, 1.2f), 1.0f, 0.6f);   // tube lip
                A.circle(x, y, 1.4f, olD, 1.0f, 0.3f);                   // bore
                A.dot(x, y, 0.7f, rgb(150, 150, 96));                     // rocket nose
            }
            A.grain(11, 19, 45, 45, 0.16f, 60);
            teamDot(A, 14.2f, 32, 2.3f);   // the owner's marking, on the rear of the pod beside the tube grid
        }
        break;
    case 10:
        if (cy) {   // twin ion cannons
            A.circle(32, 32, 12.5f, rgba(CYD, 110), 1, 0);
            for (int s = -1; s <= 1; s += 2) {
                float y = 32 + s * 3.8f;
                A.cap(41, y, 63, y, 2.1f, shade(CYM, 1.4f), 1.4f, 0.55f);
                A.line(43, y, 62, y, 1.2f, rgba(CYG, 235));
                for (int i = 0; i < 4; i++) A.box(46 + i * 4.4f, y, 0.9f, 3.2f, 0.3f, shade(CYP, 1.1f), 1, 0.2f);
                A.circle(63.5f, y, 2.1f, CYW, 1.2f, 0.4f); A.glow(64, y, 6, rgba(CYG, 150));
            }
            A.poly({ {16, 22}, {37, 21.5f}, {46, 27.5f}, {46, 36.5f}, {37, 42.5f}, {16, 42}, {12.5f, 36}, {12.5f, 28} }, CYP, 3.0f, 0.65f);
            A.poly({ {19, 25}, {35, 24.5f}, {41, 29}, {41, 35}, {35, 39.5f}, {19, 39}, {17, 35}, {17, 29} }, shade(CYP, 1.14f), 2.2f, 0.5f);
            A.line(18, 25.4f, 34, 25, 1.0f, rgba(CYG, 200)); A.line(18, 38.6f, 34, 39, 1.0f, rgba(CYG, 200));
            hatch(A, 23.5f, 32, 3.7f, shade(CYH, 1.1f));
            A.circle(29.5f, 32, 3.0f, shade(CYM, 1.2f), 1.6f, 0.5f); A.dot(29.5f, 32, 1.3f, CYG); A.glow(29.5f, 32, 6, rgba(CYG, 130));
            teamDot(A, 36.0f, 32, 2.5f);
            A.line(15, 24, 9, 15, 0.8f, rgba(CYW, 220));
        } else {    // Behemoth: twin 150mm barrels
            A.circle(32, 32, 12.5f, rgba(CKD, 110), 1, 0);
            for (int s = -1; s <= 1; s += 2) {
                float y = 32 + s * 3.9f;
                A.cap(42, y, 64, y, 2.1f, shade(CKD, 1.5f), 1.6f, 0.6f);
                A.cap(49, y, 54, y, 2.9f, shade(CKD, 1.7f), 1.4f, 0.5f);
                A.box(62.5f, y, 2.1f, 3.0f, 0.7f, shade(CKD, 1.3f), 1.2f, 0.5f);
            }
            A.poly({ {14, 21.5f}, {38, 21}, {48, 27}, {48, 37}, {38, 43}, {14, 42.5f}, {10.5f, 37}, {10.5f, 27} }, shade(CKH, 1.1f), 3.2f, 0.65f);
            A.box(45.5f, 32, 3.6f, 8.2f, 1.4f, shade(CKH, 0.78f), 1.6f, 0.5f);                    // mantlet
            rivets(A, 15, 23.4f, 36, 23.4f, 7, shade(CKH, 1.4f)); rivets(A, 15, 40.6f, 36, 40.6f, 7, shade(CKH, 1.4f));
            hatch(A, 21.5f, 26.5f, 3.2f, shade(CKH, 1.05f)); hatch(A, 21.5f, 37.5f, 3.0f, shade(CKH, 1.0f));
            A.cap(22, 26.5f, 28, 26.5f, 0.9f, CKD, 1, 0.3f);
            A.box(15.5f, 32, 3.2f, 3.4f, 0.6f, shade(CKT, 0.85f), 1.4f, 0.5f);                    // stowage
            A.box(31, 32, 0.8f, 9.6f, 0.3f, rgba(CKD, 150), 1, 0);
            teamDot(A, 35.0f, 32, 2.7f);
            A.grain(11, 20, 50, 44, 0.16f, 14);
            A.line(13, 23, 7, 14, 0.8f, rgba(CKD, 230));
        }
        break;
    default: return false;
    }
    c.outline(rgb(8, 10, 14, 170));
    return true;
}

void artRotor(Canvas& c, bool blades) {
    setTone(-1);
    Art A(c, rgb(255, 255, 255));
    if (!blades) {
        for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
            float d = hyp(x + 0.5f - 32, y + 0.5f - 32) / 30.0f;
            if (d < 1) { float a = 34.0f * (0.35f + 0.65f * d * d); put(c, x, y, Color{90, 92, 96, (u8)a}); }
        }
        return;
    }
    A.cap(3, 32, 61, 32, 1.2f, rgb(36, 36, 38), 1, 0.3f);
    A.cap(32, 3, 32, 61, 1.2f, rgb(36, 36, 38), 1, 0.3f);
    A.dot(32, 32, 2.6f, rgb(150, 152, 156));
}

void artTurretHead(Canvas& c, int idx) {
    setTone(-1);
    Art A(c, rgb(255, 255, 255));
    switch (idx) {
    case 0:   // laser turret
        A.cap(38, 32, 63, 32, 2.4f, CYM, 1.6f, 0.6f);
        A.line(40, 32, 62, 32, 1.4f, rgba(CYG, 230)); A.circle(62, 32, 3.2f, CYW, 1.6f, 0.5f); A.glow(62, 32, 8, rgba(CYG, 170));
        A.circle(32, 32, 10.5f, CYP, 3, 0.65f); A.circle(32, 32, 6.5f, shade(CYM, 1.3f), 2, 0.5f); A.dot(32, 32, 2.4f, CYG); A.glow(32, 32, 9, rgba(CYG, 110));
        A.box(37, 32, 2.6f, 6, 0.8f, CYD, 1.4f, 0.4f);
        break;
    case 1:   // gun nest: twin MGs on a shielded mount
        for (int s = -1; s <= 1; s += 2) { A.cap(38, 32 + s * 3.4f, 60, 32 + s * 3.4f, 1.6f, shade(CKD, 1.7f), 1.4f, 0.55f); A.dot(60.5f, 32 + s * 3.4f, 1.8f, CKD); }
        A.box(42, 32, 2, 8.5f, 0.8f, shade(CKS, 0.9f), 1.6f, 0.55f);   // gun shield
        A.circle(30, 32, 10, CKS, 3, 0.65f); A.circle(30, 32, 6, shade(CKD, 1.6f), 2, 0.5f);
        A.box(26, 32 + 8, 4, 3, 0.8f, shade(CKT, 0.85f), 1.4f, 0.5f);
        rivets(A, 22, 25, 38, 25, 5, shade(CKS, 1.5f));
        break;
    case 2:   // patriot launcher box, angled up: pods pointing +x
        A.box(30, 32, 20, 13, 2.4f, rgb(206, 210, 214), 2.6f, 0.6f);
        for (int j = 0; j < 4; j++) { float y = 24.5f + j * 5; A.cap(14, y, 48, y, 2.2f, rgb(64, 66, 74), 1.6f, 0.5f); A.dot(48.6f, y, 1.8f, rgb(232, 232, 236)); A.dot(48.6f, y, 0.8f, rgb(190, 40, 40)); }
        A.box(11.5f, 32, 3, 10, 1, rgb(150, 152, 160), 1.6f, 0.5f);
        A.box(30, 32, 0.9f, 13, 0.3f, rgba(rgb(30, 30, 36), 190), 1, 0);
        break;
    case 3:   // rocket battery pod
        A.box(30, 32, 20, 13, 2.4f, CKS, 2.6f, 0.6f);
        for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) { A.circle(21 + i * 9.5f, 25 + j * 14, 3.6f, shade(CKD, 1.3f), 1.8f, 0.5f); A.dot(21 + i * 9.5f, 25 + j * 14, 1.7f, CKR); }
        A.box(52, 32, 3.4f, 12, 1.2f, shade(CKR, 0.9f), 1.8f, 0.55f);
        rivets(A, 12, 20, 12, 44, 5, shade(CKS, 1.5f));
        A.grain(10, 19, 50, 45, 0.14f, 3);
        break;
    }
    c.outline(rgb(8, 10, 14, 170));
}

float artScale(int type) {
    int local = unitLocal(type);
    if (local == 10) return 1.26f;   // Aegis Titan / Behemoth
    if (local == 11) return 1.28f;   // supersonic jets: long and slender, drawn large enough to read at a glance
    if (local == 9) return 1.06f;    // elite infantry
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
    return 1.0f;
}
