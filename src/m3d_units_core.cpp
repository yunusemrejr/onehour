// One Hour - 3D models: the line vehicles and infantry of both armies (see m3d.h / m3d_reg.h).
// Cyber: pale ceramic plate over graphite, cyan light. Clanker: olive and earth camouflage, rust, gunmetal, amber light.
#include "m3d_parts.h"

using namespace m3d;

// ---------------------------------------------------------------- Clanker battle tank
static void kTank(Model& b, Model& t, int) {
    using namespace pal;
    b.dirt = 0.55f;
    tracks(b, 42, 7.5f, 6.0f, 7.5f, kHull2, true);
    // hull: a sloped glacis, flat deck, undercut rear
    b.profileY({ { -20, 3 }, { -20.5f, 8.5f }, { 9, 9.8f }, { 20.5f, 5.2f }, { 18, 2.2f }, { -17, 2.2f } }, -8.2f, 8.2f, kHull).t(T_CAMO, 7, kHull2).bev(0.9f);
    b.box(-13, 0, 10.0f, 5.5f, 6.4f, 0.7f, kDark).t(T_GRILLE, 1.6f).bev(0.2f);            // engine deck louvres
    for (int s = -1; s <= 1; s += 2) {
        b.box(-21.5f, s * 4.2f, 7.2f, 1.6f, 1.0f, 1.0f, kRust).m(METAL);                     // exhausts
        b.box(15.5f, s * 6.2f, 7.6f, 0.9f, 0.9f, 0.7f, kGlow).m(EMIT);                       // headlights
        b.box(-4, s * 11.6f, 9.0f, 7, 1.7f, 1.0f, kHull2).t(T_PANEL, 4).bev(0.3f);           // stowage bins on the fenders
    }
    b.box(-19.6f, 0, 7.6f, 0.9f, 5.5f, 1.3f, OWNER).bev(0.3f);                               // the owner's band across the tail
    b.cyl('z', 0, 0, 10.2f, 7.6f, 0.5f, kDark).bev(0.3f);                                    // turret ring
    b.cyl('z', -1, 0, 13.4f, 8.5f, 3.0f, kHull).f(F_SHADOWONLY);                             // (the turret's shade on the deck)
    // turret: a cast, tapered shell with a long gun
    t.ground = false; t.dirt = 0.15f;
    t.taper(-1.0f, 0, 10.4f, 9.2f, 7.6f, 5.6f, 6.4f, 5.2f, kHull, -0.8f, 0).t(T_CAMO, 6, kHull2).bev(1.1f);
    t.box(-2.0f, 0, 16.15f, 4.4f, 3.6f, 0.25f, OWNER).bev(0.2f);                             // roof panel in the owner's colour
    t.box(8.6f, 0, 12.8f, 1.8f, 3.2f, 2.0f, kSteel).m(METAL).bev(0.6f);                      // mantlet
    t.rod({ 9, 0, 12.9f }, { 29, 0, 12.9f }, 1.15f, kSteel).m(METAL).bev(0.2f);
    t.cyl('x', 19, 0, 12.9f, 1.6f, 2.2f, kDark).bev(0.3f);                                   // bore evacuator
    t.cyl('x', 29.5f, 0, 12.9f, 1.7f, 1.4f, kDark).bev(0.3f);                                // muzzle brake
    t.cyl('z', -3.5f, -2.6f, 16.6f, 2.3f, 0.7f, kHull2).bev(0.4f);                           // commander's hatch
    t.rod({ -2.5f, -2.6f, 17.6f }, { 3.5f, -2.6f, 18.2f }, 0.45f, kDark).f(F_NOSHADOW);      // roof machine gun
    t.box(-9.6f, 0, 12.6f, 1.6f, 5.6f, 1.8f, kHull2).t(T_PLANKS, 1.4f).bev(0.3f);            // bustle rack
    t.rod({ -7, 4.6f, 16 }, { -9.5f, 5.2f, 26 }, 0.22f, kDark).f(F_NOSHADOW);                // aerial
}
M3D_UNIT(U_K_TANK, kTank)

// ---------------------------------------------------------------- Cyber battle tank (hover-skirted, low and wide, a slab turret with a beam emitter)
static void cTank(Model& b, Model& t, int) {
    using namespace pal;
    b.dirt = 0.25f;
    tracks(b, 40, 8.0f, 5.5f, 6.6f, cHull2, false);
    for (int s = -1; s <= 1; s += 2)   // faired track guards
        b.profileY({ { -20.5f, 3.4f }, { -19, 8.0f }, { 14, 8.0f }, { 21, 3.6f } }, s * 13.6f - 0.9f, s * 13.6f + 0.9f, cHull).t(T_PANEL, 8).bev(0.5f);
    b.profileY({ { -19.5f, 3 }, { -18.5f, 9.2f }, { 6, 9.6f }, { 21.5f, 4.6f }, { 19, 2.2f }, { -17, 2.2f } }, -8.6f, 8.6f, cHull).t(T_PANEL, 7).bev(1.0f);
    b.box(-19.2f, 0, 6.4f, 0.8f, 6.0f, 1.6f, cDark).t(T_GRILLE, 1.3f).bev(0.2f);              // rear radiator
    b.box(-12, 0, 9.75f, 4.6f, 5.0f, 0.3f, cMid).t(T_RIBS, 1.5f).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(17.2f, s * 5.8f, 6.6f, 0.7f, 1.3f, 0.5f, cGlow).m(EMIT);                         // light bars
        b.box(-6, s * 8.75f, 7.6f, 9, 0.3f, 0.8f, OWNER).bev(0.1f);                            // the owner's flank stripes
        b.box(-20.2f, s * 5.6f, 5.2f, 0.4f, 1.0f, 0.6f, C(0xFF5A48)).m(EMIT);                  // tail lights
    }
    b.cyl('z', -1, 0, 9.9f, 7.2f, 0.4f, cDark).bev(0.3f);
    b.cyl('z', -1, 0, 12.6f, 8.2f, 2.6f, cHull).f(F_SHADOWONLY);
    t.ground = false; t.dirt = 0;
    t.taper(-1.5f, 0, 10.2f, 9.6f, 7.2f, 4.6f, 7.8f, 5.6f, cHull, -0.6f, 0).t(T_PANEL, 6).bev(1.0f);
    t.box(-3.2f, 0, 14.95f, 4.8f, 4.2f, 0.22f, OWNER).bev(0.2f);
    t.box(8.4f, 0, 12.4f, 2.4f, 2.6f, 1.6f, cMid).bev(0.5f);
    for (int s = -1; s <= 1; s += 2) t.rod({ 9, s * 1.5f, 12.4f }, { 25, s * 1.5f, 12.4f }, 0.75f, cDark).m(METAL).bev(0.2f);   // twin rails
    t.box(17, 0, 12.4f, 1.0f, 2.6f, 1.0f, cMid).bev(0.3f);
    t.box(25.6f, 0, 12.4f, 0.8f, 2.4f, 1.1f, cGlow).m(EMIT);                                   // the emitter between the rails
    t.cyl('z', -4.5f, 2.6f, 15.3f, 1.9f, 0.6f, cHull2).bev(0.4f);
    t.dome(-4.5f, -2.8f, 15.0f, 1.7f, 1.7f, 1.5f, cGlass).m(GLASS);                           // sensor dome
    t.box(-10.4f, 0, 12.2f, 1.2f, 5.0f, 1.6f, cDark).t(T_GRILLE, 1.2f).bev(0.2f);
}
M3D_UNIT(U_C_TANK, cTank)

static void kInf1(Model& b, Model&, int f) {   // Clanker rifleman: khaki fatigues, steel helmet, wooden-stocked rifle
    Trooper p; p.suit = C(0x6F6A46); p.armour = C(0x545A38); p.helmet = C(0x4C5236); p.pack = C(0x5B4A32); p.gun = C(0x3A2C22);
    trooper(b, p, f);
}
M3D_UNIT(U_K_INF1, kInf1)
static void cInf1(Model& b, Model&, int f) {   // Cyber trooper: white plate over a dark undersuit, cyan visor, pulse rifle
    Trooper p; p.suit = C(0x2E3440); p.armour = C(0xD9DFE6); p.helmet = C(0xE4E9EE); p.pack = C(0x596577); p.head = 1; p.light = C(0x58ECFF); p.gun = C(0x30363F);
    trooper(b, p, f);
}
M3D_UNIT(U_C_INF1, cInf1)
