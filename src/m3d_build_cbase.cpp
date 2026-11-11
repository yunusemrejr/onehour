// One Hour - 3D models: the CYBER base structures (Command Core, Supply Hub, Barracks, Assembly Plant, Drone Pad, Data Center,
// Laser Turret, Patriot Battery, Bitcoin Datacenter, Nuke Ramp) plus the two aiming heads the game turns itself.
// Plan positions follow the old 2D painters in artb.cpp (Bld::block(x, y, w, h, ht)); old heights convert with z = ht / 0.7.
#include "m3d_parts.h"

using namespace m3d;

namespace {

const float HT = 1.0f / 0.7f;          // model units of height per old design pixel of height
// a few Cyber colours beyond the shared palette, in the same spirit: concrete, white paint, minted gold, an aviation red
const Color cConc = C(0xB2BAC4), cPaint = C(0xEDEFF3), cSlabDk = C(0x6F7C8C);
const Color cCoin = C(0xD9A63C), cCoinLit = C(0xF2D486), cCoinSeam = C(0x8A5E16);
const Color cAvRed = C(0xE8493A), cGlaze = C(0x9FEBFF);

// rooftop air cooler: a pale casing with a fan well on its lid, or a louvred screen on its south face
inline void plant(Model& m, float cx, float cy, float w, float h, float z0, float ht, bool fan, float fr) {
    m.slab(cx, cy, w * 0.5f - 0.4f, h * 0.5f - 0.4f, z0, ht, pal::cRoof).m(PAINT).t(T_PANEL, 6).bev(0.5f);
    if (fan) {
        m.cyl('z', cx, cy, z0 + ht + 0.05f, fr, 0.3f, pal::cDark).bev(0.2f);
        for (int k = 0; k < 2; k++) m.box(cx, cy, z0 + ht + 0.35f, fr * 0.78f, fr * 0.3f, 0.22f, pal::cSteel).m(METAL).rz(45 + k * 90).f(F_NOSHADOW);
        m.cyl('z', cx, cy, z0 + ht + 0.55f, fr * 0.2f, 0.2f, pal::cHull2).m(METAL).bev(0.15f);
    } else {
        m.box(cx, cy + h * 0.5f - 0.25f, z0 + ht * 0.55f, w * 0.38f, 0.35f, ht * 0.32f, pal::cDark).t(T_GRILLE, 1.5f).bev(0.1f);
    }
}
// a roller shutter in a striped frame, set into a south wall whose face is at y = yf
inline void rollDoor(Model& m, float cx, float yf, float z0, float w, float dh, Color frame) {
    m.box(cx, yf + 0.35f, z0 + dh * 0.5f, w * 0.5f + 1.4f, 0.4f, dh * 0.5f + 1.0f, frame).bev(0.3f);
    m.box(cx, yf + 0.9f, z0 + dh * 0.5f, w * 0.5f, 0.3f, dh * 0.5f, pal::cDark).m(MATTE).t(T_RIBS, 1.6f).bev(0.15f);
    m.box(cx, yf + 1.2f, z0 + dh + 1.1f, w * 0.5f + 1.4f, 0.3f, 0.7f, cPaint).t(T_HAZARD, 2.2f, pal::cDark).bev(0.1f);
}
// a glazed band across a south wall (the clerestory / office windows of the old art)
inline void winBand(Model& m, float cx, float yf, float cz, float w, float hh, float pitch) {
    m.box(cx, yf + 0.3f, cz, w * 0.5f, 0.3f, hh * 0.5f, pal::cWall2).t(T_WINDOWS, pitch, pal::cWin).bev(0.2f);
}
// lattice comms mast on a roof: two splayed legs, braces, a cross-arm and an aviation light
inline void mast(Model& m, float x, float y, float zBase, float h, Color tip) {
    for (int s = -1; s <= 1; s += 2) m.rod({ x + s * 3.4f, y, zBase }, { x + s * 0.5f, y, zBase + h }, 0.42f, pal::cSteel).m(METAL).bev(0.1f);
    for (int k = 1; k <= 3; k++) { const float z = zBase + h * 0.25f * k, hw = 3.4f - 2.9f * 0.25f * k; m.box(x, y, z, 0.3f, hw, 0.22f, pal::cSteel).m(METAL).bev(0.1f); }
    m.box(x, y, zBase + h * 0.78f, 0.3f, 6.2f, 0.28f, pal::cSteel).m(METAL).bev(0.1f);
    m.rod({ x, y, zBase + h }, { x, y, zBase + h + h * 0.18f }, 0.3f, pal::cHull2).m(METAL).f(F_NOSHADOW);
    m.ball(x, y, zBase + h + h * 0.19f, 0.75f, 0.75f, 0.75f, tip).m(EMIT);
}
// fixed satellite dish: a tilted bowl on a short pedestal, feed horn toward the south-east
inline void dish(Model& m, float x, float y, float zBase, float r, Color rim) {
    m.cyl('z', x, y, zBase + 0.6f, r * 0.28f, 0.7f, pal::cSteel).m(METAL).bev(0.2f);
    m.cyl('z', x, y, zBase + r * 0.55f, r, r * 0.22f, rim).m(PAINT).t(T_PANEL, 5).bev(0.4f).rx(58);
    m.rod({ x, y, zBase + r * 0.5f }, { x + r * 0.8f, y + r * 0.8f, zBase + r * 1.15f }, 0.28f, pal::cSteel).m(METAL).f(F_NOSHADOW);
    m.ball(x + r * 0.82f, y + r * 0.82f, zBase + r * 1.18f, 0.7f, 0.7f, 0.7f, pal::cHull).m(METAL);
}
// radome: a drum with a seamed pale dome
inline void radome(Model& m, float x, float y, float zBase, float r, float ht, Color drum) {
    m.cyl('z', x, y, zBase + ht * 0.5f, r * 0.9f, ht * 0.5f, drum).m(MATTE).t(T_PANEL, 7).bev(0.5f);
    m.dome(x, y, zBase + ht, r, r, r * 0.8f, pal::cRoof).t(T_PANEL, 5);
    m.cyl('z', x, y, zBase + ht + r * 0.72f, r * 0.16f, 0.3f, pal::cTrim).bev(0.15f);
}

// ---------------------------------------------------------------- Command Core, 4 x 4 tiles
// A blast-hardened concrete base with a second storey of tinted glass and a helipad, a radome, coolers, dishes and a comms mast on the roof.
static void cHQ(Model& m, int) {
    using namespace pal;
    const float W = 4 * BPX, H = 4 * BPX, zLo = 16 * HT, zUp = zLo + 12 * HT;
    m.dirt = 0.10f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // lower floor: thick concrete, a row of blast windows and a heavy door on the south face
    m.slab(64, 87, 56, 31, 0.9f, zLo - 0.9f, cWall).m(PAINT).t(T_PANEL, 14).bev(0.8f);
    m.box(64, 118.4f, zLo * 0.52f, 55, 0.35f, 4.2f, cWall2).t(T_WINDOWS, 8, cWin).bev(0.2f);
    m.box(64, 87, zLo + 0.35f, 53, 28, 0.4f, cWall2).m(MATTE).t(T_PANEL, 12).bev(0.3f);      // recessed roof panel
    rollDoor(m, 64, 118, 0.9f, 16, 12 * HT, cSteel);
    m.box(64, 118.6f, 12 * HT + 2.4f, 1.2f, 0.5f, 0.7f, cGlow).m(EMIT);
    for (int s = -1; s <= 1; s += 2) m.slab(64 + s * 22, 125, 7, 2, 0.9f, 3 * HT, cConc).m(MATTE).t(T_PANEL, 6).bev(0.5f);   // jersey barriers
    // second storey: pale cladding, glazed office band, its own roof with a helipad
    m.slab(64, 75, 34, 15, zLo, 12 * HT, cHull).m(PAINT).t(T_PANEL, 9).bev(0.7f);
    m.box(64, 90.4f, zLo + 6 * HT, 33, 0.35f, 4.0f, cWall2).t(T_WINDOWS, 7.5f, cWin).bev(0.2f);
    m.slab(64, 75, 33, 14, zUp, 0.45f, cRoof).m(PAINT).t(T_PANEL, 8).bev(0.2f);
    m.cyl('z', 64, 75, zUp + 0.5f, 9.2f, 0.22f, OWNER).bev(0.2f);                             // helipad ring in the owner's colour
    m.box(60.6f, 75, zUp + 0.62f, 0.6f, 4.6f, 0.2f, cRoof).bev(0.1f).f(F_NOSHADOW);
    m.box(67.4f, 75, zUp + 0.62f, 0.6f, 4.6f, 0.2f, cRoof).bev(0.1f).f(F_NOSHADOW);
    m.box(64, 75, zUp + 0.62f, 3.4f, 0.6f, 0.2f, cRoof).bev(0.1f).f(F_NOSHADOW);
    m.slab(64, 63.2f, 30, 1.6f, zUp + 0.45f, 0.5f, OWNER).bev(0.15f);                         // owner's band on the roof's north edge
    mast(m, 36, 69, zUp, 9 * HT, cAvRed);
    // roof plant west and east of the storey, dishes and the comms mast on the lower roof
    plant(m, 18, 93, 16, 10, zLo, 5 * HT, true, 3.9f);
    plant(m, 108.5f, 75.5f, 17, 11, zLo, 5 * HT, true, 4.2f);
    radome(m, 19, 80, zLo, 8, 12 * HT, cConc);
    dish(m, 108, 73, zLo, 6.5f, cHull2);
    dish(m, 111, 104, zLo, 4.5f, cHull2);
    mast(m, 103, 62, zLo, 29 * HT, cAvRed);
    for (int s = -1; s <= 1; s += 2) m.rod({ 103 + s * 2.0f, 62, zLo }, { 103 + s * 6.5f, 62, zLo + 20 }, 0.24f, cSteel).m(METAL).f(F_NOSHADOW);
    m.box(52, 124, 0.9f + 1.0f, 8, 2.0f, 0.25f, cPaint).t(T_HAZARD, 2.2f, cDark).bev(0.1f);   // kerb stripes on the apron
    m.box(76, 124, 0.9f + 1.0f, 8, 2.0f, 0.25f, cPaint).t(T_HAZARD, 2.2f, cDark).bev(0.1f);
    m.ball(24, 116, 2.2f, 1.0f, 1.0f, 1.0f, cGlow).m(EMIT); m.ball(104, 116, 2.2f, 1.0f, 1.0f, 1.0f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_HQ, cHQ)

// ---------------------------------------------------------------- Supply Hub, 3 x 3 tiles
// A steel-clad warehouse with a skylit roof over a loading dock, an office block, stacked containers, pallets and a forklift.
static void cSupply(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX, zWh = 0.9f + 16 * HT, zOf = 0.9f + 12 * HT;
    m.dirt = 0.14f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // loading dock apron: a raised concrete deck edged in hazard stripes
    m.slab(37, 58, 35, 5, 0.9f, 1.4f, cConc).m(MATTE).t(T_PANEL, 8).bev(0.3f);
    m.box(37, 62.6f, 2.4f, 35, 0.4f, 0.9f, cPaint).t(T_HAZARD, 2.4f, cDark).bev(0.1f);
    // the warehouse: clad walls, a ribbed roof with four glazed skylights and three roller doors on the south wall
    m.slab(38, 29, 32, 23, 0.9f, zWh - 0.9f, cHull).m(PAINT).t(T_PANEL, 10).bev(0.8f);
    m.slab(38, 29, 31, 22, zWh, 0.5f, cRoof).m(PAINT).t(T_RIBS, 3.0f).bev(0.25f);
    for (int i = 0; i < 4; i++) {
        const float x = 12 + i * 17.0f;
        m.slab(x, 29, 4.6f, 19, zWh + 0.5f, 0.8f, cSteel).m(METAL).bev(0.2f);
        m.slab(x, 29, 3.8f, 18.2f, zWh + 1.1f, 0.6f, cGlass).m(GLASS).bev(0.1f);
        m.box(x, 29, zWh + 1.5f, 1.2f, 18.2f, 0.15f, cHull2).f(F_NOSHADOW).bev(0.1f);
    }
    for (int i = 0; i < 3; i++) rollDoor(m, 18 + i * 18, 52, 0.9f, 12, 11 * HT, cSteel);
    m.slab(22, 43.5f, 9, 1.6f, zWh - 1.5f, 1.0f, OWNER).bev(0.15f);                    // owner's band on the wall
    m.box(38, 52.4f, zWh - 6, 30, 0.3f, 1.0f, cWall2).t(T_GRILLE, 1.6f).bev(0.15f);
    // the control office on the east side: pale cladding, two glazed bands, a roof fan and a mast
    m.slab(83, 27, 7, 17, 0.9f, zOf - 0.9f, cHull).m(PAINT).t(T_PANEL, 7).bev(0.6f);
    m.slab(83, 27, 7.6f, 17.6f, zOf, 0.4f, cRoof).bev(0.2f);
    winBand(m, 83, 44, zOf * 0.58f, 12, 5 * HT, 6);
    m.cyl('z', 83, 22, zOf + 0.6f, 3.6f, 0.4f, cDark).bev(0.2f);
    for (int k = 0; k < 2; k++) m.box(83, 22, zOf + 0.95f, 2.8f, 1.1f, 0.2f, cSteel).m(METAL).rz(30 + k * 90).f(F_NOSHADOW);
    mast(m, 88, 12, zOf, 10 * HT, cAvRed);
    m.ball(90, 38, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
    // container yard: two stacks to the west, two single boxes along the south edge
    const Color cA = C(0x62758A), cB = C(0x7E8C9C), cC = C(0x9AA7B4);
    for (int i = 0; i < 2; i++) {
        const float x = 20 + i * 28.0f;
        m.slab(x, 85, 12, 5, 0.9f, 6.0f, i ? cB : cA).m(PAINT).t(T_RIBS, 2.6f).bev(0.5f);
        m.slab(x, 85, 12, 5, 6.9f, 6.0f, i ? cC : cA).m(PAINT).t(T_RIBS, 2.6f).bev(0.5f);
        m.box(x, 85, 13.2f, 11.6f, 0.4f, 0.3f, cSteel).m(METAL).bev(0.1f);
        m.slab(x, 93, 12, 3, 0.9f, 6.0f, i ? cA : cC).m(PAINT).t(T_RIBS, 2.6f).bev(0.5f);
        m.box(x, 95.9f, 3.9f, 10, 0.3f, 1.6f, cDark).m(MATTE).t(T_GRILLE, 1.4f).bev(0.1f);
    }
    m.slab(20 + 0 * 28, 85, 11, 4, 12.9f, 0.35f, OWNER).bev(0.1f);                     // the owner's colour on a stacked lid
    // pallets by the dock and a forklift parked on it
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++)
        m.slab(68 + i * 10, 70 + j * 9, 4, 3.5, 0.9f, 1.6f + (float)(i + j) * 0.5f, C(0xA8946E)).m(MATTE).t(T_PLANKS, 2.4f).bev(0.2f);
    m.slab(45, 69, 5, 3, 2.3f, 3.2f, C(0xD9AE42)).m(PAINT).bev(0.5f);
    m.slab(45, 69, 3.4f, 2.4f, 5.5f, 2.6f, cDark).m(MATTE).bev(0.3f);
    m.profileY({ { 6.5f, 1.6f }, { 9.5f, 1.6f }, { 9.5f, 6.6f }, { 6.5f, 6.6f } }, 45 - 2.6f, 45 + 2.6f, cSteel).m(METAL).bev(0.2f);
    m.box(50, 69, 2.0f, 3.4f, 2.4f, 0.35f, cDark).m(RUBBER).bev(0.1f);
    m.cyl('z', 50, 69, 8.0f, 0.5f, 0.9f, cSteel).m(METAL).rz(90).bev(0.1f);
    m.ball(4, 4, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_SUPPLY, cSupply)

// ---------------------------------------------------------------- Barracks, 3 x 2 tiles
// A two-storey block with a floor band of windows, an entrance canopy, roof plant and a water tank, a parade strip and a flagpole.
static void cBarracks(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 2 * BPX, zBk = 0.9f + 20 * HT;
    m.dirt = 0.13f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the block: pale walls, two rows of windows over a floor line, a flat ceramic roof
    m.slab(48, 23, 44, 15, 0.9f, zBk - 0.9f, cWall).m(PAINT).t(T_PANEL, 12).bev(0.8f);
    winBand(m, 48, 38.4f, 0.9f + 13 * HT, 80, 4.6f, 8);
    winBand(m, 48, 38.4f, 0.9f + 6 * HT, 80, 4.6f, 8);
    m.box(48, 38.6f, 0.9f + 9.6f, 44, 0.3f, 0.6f, cWall2).bev(0.1f);                    // the floor band
    m.slab(48, 23, 44.6f, 15.6f, zBk, 0.55f, cRoof).m(PAINT).t(T_PANEL, 9).bev(0.25f);
    m.slab(20, 23, 16, 14, zBk + 0.55f, 0.5f, OWNER).bev(0.15f);                         // owner's roof panel
    // entrance canopy on two posts, centred on the south wall
    m.slab(48, 41, 8.5f, 3.2f, 11 * HT, 0.7f, cHull2).m(METAL).bev(0.3f);
    for (int s = -1; s <= 1; s += 2) m.slab(48 + s * 7.0f, 42.5f, 0.9f, 0.9f, 0.9f, 11 * HT - 0.9f, cSteel).m(METAL).bev(0.2f);
    m.box(48, 38.5f, 0.9f + 5.2f, 6, 0.35f, 5.2f, cDark).m(MATTE).t(T_PANEL, 5).bev(0.2f);
    m.box(48, 38.9f, 0.9f + 8.6f, 1.6f, 0.3f, 1.4f, cGlow).m(EMIT);
    // parade strip and the training yard: tyres, a hurdle and a balance beam
    m.slab(70, 54, 14, 6, 0.9f, 0.35f, cPaint).t(T_STRIPE, 2.6f, cSteel).bev(0.1f);
    m.slab(24, 52, 20, 7.5f, 0.9f, 0.5f, C(0x8F8B80)).m(MATTE).t(T_NOISE, 4, cSlabDk).bev(0.2f);
    for (int i = 0; i < 3; i++) m.cyl('z', 10 + i * 9, 56, 0.9f + 0.7f, 1.4f, 0.7f, C(0x2A2D33)).m(RUBBER).t(T_TREAD, 1.2f).bev(0.2f);
    m.slab(17, 47.5f, 5, 1.5f, 0.9f, 3 * HT, cConc).m(MATTE).bev(0.4f);
    for (int s = -1; s <= 1; s += 2) m.box(36 + s * 3.4f, 49.5f, 0.9f + 2.6f, 0.5f, 0.5f, 2.6f, cSteel).m(METAL).bev(0.15f);
    m.box(36, 49.5f, 0.9f + 5.6f, 6.4f, 0.7f, 0.5f, C(0x9B7A56)).m(MATTE).bev(0.2f);
    // roof plant: two coolers, a water tank on a steel stand, a vent duct and a mast
    plant(m, 19, 8.5f, 14, 9, zBk + 0.55f, 5 * HT, true, 3.2f);
    plant(m, 67, 8.5f, 14, 9, zBk + 0.55f, 5 * HT, true, 3.2f);
    m.box(48, 30, zBk + 1.2f, 16, 1.0f, 1.4f, cSteel).m(METAL).t(T_GRILLE, 1.6f).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) m.box(38 + s * 5.5f, 10, zBk + 0.7f, 0.7f, 4.4f, 4.4f, cSteel).m(METAL).bev(0.2f);
    m.cyl('z', 38, 10, zBk + 4.6f + 9 * HT * 0.5f, 6, 9 * HT * 0.5f, cHull2).m(METAL).t(T_BAND, 1.2f, cDark).bev(0.5f);
    m.cyl('z', 38, 10, zBk + 4.6f + 9 * HT + 0.2f, 5.4f, 0.3f, cTrim).bev(0.15f);
    m.rod({ 44, 10, zBk + 4 }, { 51, 12, zBk + 3 }, 0.55f, cSteel).m(METAL);
    mast(m, 86, 26, zBk + 0.55f, 12 * HT, cAvRed);
    // flagpole in the front yard: a mast with the owner's colour as the flag
    m.cyl('z', 88, 60, 0.9f + 15, 0.5f, 15, cSteel).m(METAL).bev(0.1f);
    m.box(91, 60, 26, 3.2f, 0.4f, 2.4f, OWNER).bev(0.1f);
    m.ball(88, 60, 31.4f, 0.8f, 0.8f, 0.8f, cHull).m(METAL);
    m.ball(4, 4, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT); m.ball(92, 4, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_BARRACKS, cBarracks)

// ---------------------------------------------------------------- Assembly Plant, 4 x 3 tiles
// A sawtooth-roofed assembly hall with the vehicle bay door in the middle of its south wall, a gantry crane over the apron,
// a half-built hull under the hook, a paint annex and a cooling-water tank.
static void cFactory(Model& m, int) {
    using namespace pal;
    const float W = 4 * BPX, H = 3 * BPX, zH = 0.9f + 18 * HT, zA = 0.9f + 9 * HT;
    m.dirt = 0.15f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the hall, x 8..120 and y 6..58, with its south face at y = 58
    m.slab(64, 32, 56, 26, 0.9f, zH - 0.9f, cWall).m(PAINT).t(T_PANEL, 12).bev(0.8f);
    m.box(64, 58.3f, 0.9f + 4.3f, 56, 0.35f, 4.3f, cWall2).m(MATTE).t(T_PANEL, 9).bev(0.2f);      // dark plinth
    winBand(m, 26, 58.3f, 0.9f + 11 * HT, 28, 6 * HT, 6);
    winBand(m, 100, 58.3f, 0.9f + 11 * HT, 26, 6 * HT, 6);
    // sawtooth roof: a base sheet, then three teeth, each a glazed north screen over a shallow sloped south sheet
    m.slab(64, 32, 56, 26, zH, 0.6f, cRoof).m(PAINT).t(T_RIBS, 3.4f).bev(0.25f);
    for (int k = 0; k < 3; k++) {
        const float y0 = 6 + k * 17.4f;
        m.profileX({ { y0 + 7.3f, zH + 0.6f }, { y0 + 17.4f, zH + 0.6f }, { y0 + 17.4f, zH + 5.0f }, { y0 + 7.3f, zH + 1.4f } }, 10, 118, cRoof).m(PAINT).t(T_RIBS, 3.2f).bev(0.3f);
        m.box(64, y0 + 1.6f, zH + 3.0f, 54, 0.7f, 2.6f, cGlass).m(GLASS).bev(0.15f);
        m.box(64, y0 + 0.7f, zH + 5.6f, 54, 0.9f, 0.5f, cSteel).m(METAL).bev(0.2f);
        for (float x = 19.0f; x < 112.0f; x += 9.0f) m.box(x, y0 + 1.6f, zH + 3.0f, 0.35f, 0.85f, 2.6f, cSteel).m(METAL).bev(0.1f).f(F_NOSHADOW);
    }
    // exhaust stacks and roof ventilators
    m.cyl('z', 30, 20, zH + 15.7f, 3.6f, 15.7f, cHull).m(METAL).t(T_BAND, 1.4f, cDark).bev(0.5f);
    m.cyl('z', 30, 20, zH + 31.4f, 3.4f, 0.6f, cDark).bev(0.2f);
    m.ball(30, 20, zH + 32.6f, 0.7f, 0.7f, 0.7f, cAvRed).m(EMIT);
    m.cyl('z', 46, 22, zH + 11.4f, 3.2f, 11.4f, cHull).m(METAL).t(T_BAND, 1.4f, cDark).bev(0.5f);
    m.cyl('z', 46, 22, zH + 22.9f, 3.0f, 0.5f, cDark).bev(0.2f);
    for (int i = 0; i < 3; i++) { const float x = 70 + i * 18.0f; m.cyl('z', x, 54, zH + 1.4f, 4.0f, 1.4f, cHull).m(METAL).bev(0.4f); m.cyl('z', x, 54, zH + 2.9f, 2.0f, 0.3f, cDark).bev(0.15f); }
    m.slab(105, 9, 11, 3.2f, zH + 0.4f, 0.6f, OWNER).bev(0.15f);                                   // owner's band on the roof's north edge
    // the vehicle bay door, centred under the hall as the old art has it, opening onto the apron
    rollDoor(m, 64, 58, 0.9f, 40, 16.5 * HT, cPaint);
    m.box(64, 58.6f, 0.9f + 25 * HT, 2.2f, 0.6f, 0.8f, cGlow).m(EMIT);
    m.box(64, 93, 0.9f + 1.2f, 15, 1.5f, 0.3f, cPaint).t(T_HAZARD, 3.0f, cDark).bev(0.1f);         // striped kerb on the south edge
    // gantry crane over the apron: two legs, a girder, a trolley and a hook
    for (int s = -1; s <= 1; s += 2) m.box(46 + s * 36, 77, 0.9f + 14, 2.2f, 11, 14, cSteel).m(METAL).t(T_PANEL, 8).bev(0.3f);
    m.box(83, 67, 0.9f + 28.5f, 39, 1.7f, 1.6f, cSteel).m(METAL).t(T_PANEL, 8).bev(0.3f);
    m.slab(64, 67, 6, 3, 0.9f + 30.5f, 4.6f, cHull).m(PAINT).bev(0.4f);
    m.box(64, 79, 0.9f + 28, 3.0f, 0.5f, 0.5f, cDark).f(F_NOSHADOW);
    m.rod({ 64, 79, 12 }, { 64, 79, 26 }, 0.28f, cDark).m(METAL).f(F_NOSHADOW);
    m.ball(64, 79, 11.4f, 1.4f, 1.4f, 1.0f, cSteel).m(METAL).bev(0.2f);
    // a hull in the paint booth on the hook: wheels, frame and cab
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 2; k++)
        m.cyl('y', 60 + k * 12, 79 + s * 4.5f, 2.4f, 1.7f, 1.9f, C(0x2A2D33)).m(RUBBER).t(T_TREAD, 1.2f).bev(0.2f);
    m.slab(66, 79, 14, 5.5f, 3.6f, 1.6f, cSteel).m(METAL).t(T_PANEL, 6).bev(0.25f);
    m.slab(61, 79, 6, 4.4f, 5.2f, 3.4f, cHull).m(PAINT).bev(0.5f);
    m.box(61, 79, 8.4f, 5.6f, 4.0f, 0.3f, cGlass).m(GLASS).bev(0.1f);
    // the paint and ventilation annex, south-west, with its intake louvres
    m.slab(24, 78, 16, 12, 0.9f, zA - 0.9f, cWall2).m(PAINT).t(T_PANEL, 9).bev(0.6f);
    m.slab(24, 78, 16.4f, 12.4f, zA, 0.5f, cRoof).t(T_RIBS, 2.8f).bev(0.2f);
    plant(m, 17, 75, 12, 9, zA + 0.5f, 4.5f, true, 3.0f);
    m.slab(31.5f, 74, 5.5f, 4, zA + 0.5f, 2.6f, cHull).t(T_GRILLE, 1.5f).bev(0.2f);
    m.box(24, 90.2f, 0.9f + 7.4f, 14, 0.4f, 4.4f, cDark).t(T_GRILLE, 2.0f).bev(0.1f);
    m.rod({ 40, 82, 4 }, { 46, 82, 4 }, 0.8f, cSteel).m(METAL);
    // the cooling-water tank, south-east, with its pipework
    m.cyl('z', 106, 86, 0.9f + 10, 9, 10, cHull).m(METAL).t(T_PANEL, 8).bev(0.7f);
    m.cyl('z', 106, 86, 0.9f + 20.4f, 6.5f, 0.4f, cSteel).m(METAL).bev(0.2f);
    m.cyl('z', 106, 86, 0.9f + 21.4f, 2.0f, 0.6f, cTrim).bev(0.15f);
    m.box(106, 95.3f, 10, 8, 0.4f, 2.2f, cDark).t(T_GRILLE, 1.6f).bev(0.1f);
    m.rod({ 97, 86, 5 }, { 88, 86, 5 }, 0.9f, cSteel).m(METAL);
    m.rod({ 88, 86, 5 }, { 88, 86, 12 }, 0.9f, cSteel).m(METAL);
    for (int i = 0; i < 2; i++) m.ball(64 + i * 0, 60 + i * 30, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_FACTORY, cFactory)

// ---------------------------------------------------------------- Drone Pad, 10 x 7 tiles
// A concrete flight apron: two marked runways, the 5 x 5 grid of pads the aircraft use, a glazed hangar with a curved roof,
// a control tower, munitions shelters and a fuel bund. Everything low and flat but the tower and the hangar.
static void cAirfield(Model& m, int) {
    using namespace pal;
    const float W = 10 * BPX, H = 7 * BPX, cx0 = W * 0.5f, cy0 = H * 0.5f;
    const float zTw = 0.9f + 24 * HT, zCab = zTw + 9 * HT, zHg = 0.9f + 15 * HT;
    m.dirt = 0.08f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 24).bev(0.4f);
    // the pads: dark discs on the apron in the game's 5 x 5 grid, an H stroke painted on each
    for (int row = 0; row < 5; row++) for (int col = 0; col < 5; col++) {
        const float px = cx0 + (col - 2) * 42, py = cy0 + (row - 2) * 34;
        m.cyl('z', px, py, 1.05f, 12.5f, 0.35f, cSlabDk).m(MATTE).t(T_PANEL, 6).bev(0.25f);
        m.cyl('z', px, py, 1.5f, 10.8f, 0.12f, cPaint).f(F_NOSHADOW).bev(0.1f);
        m.cyl('z', px, py, 1.6f, 9.4f, 0.12f, C(0x46535F)).m(MATTE).f(F_NOSHADOW).bev(0.1f);   // (only a painted ring shows: the pad itself stays dark)
        if (row == 2 && col == 2) {
            for (int s = -1; s <= 1; s += 2) m.box(px + s * 4, py, 1.62f, 0.7f, 4.6f, 0.35f, cPaint).bev(0.1f).f(F_NOSHADOW);
            m.box(px, py, 1.62f, 4.0f, 0.7f, 0.35f, cPaint).bev(0.1f).f(F_NOSHADOW);
        }
    }
    // two runways: thin painted strips with centre dashes
    for (int r = 0; r < 2; r++) {
        const float y = H * (0.36f + 0.28f * r);
        m.slab(cx0, y, W / 2 - 6, 5.5f, 0.9f, 0.22f, cSlabDk).m(MATTE).t(T_PANEL, 14).bev(0.1f);
        for (int i = 0; i < 8; i++) m.box(20 + i * 40, y, 1.12f, 8, 1.1f, 0.15f, cPaint).bev(0.05f).f(F_NOSHADOW);
    }
    // apron edge lights along the north and south sides
    for (int i = 0; i < 6; i++) { const float x = 24 + i * 54; m.ball(x, 5, 1.5f, 0.7f, 0.7f, 0.5f, cGlow).m(EMIT); m.ball(x, H - 5, 1.5f, 0.7f, 0.7f, 0.5f, cGlow).m(EMIT); }
    // control tower: a concrete shaft, a glazed cab, a roof band and a mast
    m.slab(23, 21, 13, 9, 0.9f, zTw - 0.9f, cWall).m(PAINT).t(T_PANEL, 9).bev(0.7f);
    winBand(m, 23, 30.3f, 0.9f + 12 * HT, 22, 6 * HT, 9);
    rollDoor(m, 23, 30, 0.9f, 8, 9 * HT, cSteel);
    m.slab(23, 21, 13.6f, 9.6f, zTw, 0.5f, cRoof).t(T_PANEL, 7).bev(0.2f);
    m.slab(23, 21, 10, 6, zTw + 0.5f, 9 * HT, cGlass).m(GLASS).t(T_PANEL, 6).bev(0.4f);
    for (int i = 0; i < 4; i++) m.box(15 + i * 5.4f, 27.2f, zTw + 0.5f + 4.5f, 0.6f, 0.35f, 4.5f, cSteel).m(METAL).bev(0.1f).f(F_NOSHADOW);
    m.slab(23, 21, 11.4f, 7.4f, zCab, 0.5f, cRoof).t(T_PANEL, 6).bev(0.2f);
    m.slab(23, 19, 8, 1.6f, zCab + 0.5f, 0.6f, OWNER).bev(0.15f);                        // owner's band on the cab roof
    mast(m, 33, 16, zCab + 0.5f, 12 * HT, cAvRed);
    // hangar: a curved sheet roof over a clad shell, a big striped roller door on its south face
    m.slab(222, 16, 48, 10, 0.9f, zHg - 0.9f, cWall2).m(PAINT).t(T_PANEL, 12).bev(0.6f);
    m.cyl('x', 222, 16, zHg + 1.0f, 11, 48, cRoof).m(PAINT).t(T_RIBS, 3.2f).bev(0.4f);
    for (int i = 0; i < 6; i++) m.box(186 + i * 14, 16, zHg + 11.4f, 0.5f, 9.5f, 0.4f, cSteel).m(METAL).bev(0.1f).f(F_NOSHADOW);
    rollDoor(m, 222, 26, 0.9f, 64, 12 * HT, cPaint);
    m.slab(252, 6, 7, 2.2f, zHg + 6.5f, 0.6f, OWNER).bev(0.15f);
    plant(m, 196, 8, 14, 8, zHg + 6.0f, 3.2f, true, 3.0f);
    // munitions shelters, south-west: a low block with four roller doors and two roof units
    m.slab(41, H - 29, 31, 13, 0.9f, 9 * HT, cWall2).m(PAINT).t(T_PANEL, 10).bev(0.5f);
    m.slab(41, H - 29, 31.4f, 13.4f, 0.9f + 9 * HT, 0.5f, cRoof).t(T_RIBS, 2.8f).bev(0.2f);
    for (int i = 0; i < 4; i++) m.box(20 + i * 14, H - 15.6f, 0.9f + 4.0f, 4.6f, 0.35f, 4.0f, cDark).m(MATTE).t(T_GRILLE, 1.8f).bev(0.1f);
    plant(m, 21, H - 32, 14, 8, 0.9f + 9 * HT + 0.5f, 2.8f, true, 3.2f);
    plant(m, 41, H - 32, 14, 8, 0.9f + 9 * HT + 0.5f, 2.8f, false, 0);
    m.slab(27, H - 22, 10, 1.8f, 0.9f + 9 * HT - 1.5f, 0.9f, OWNER).bev(0.15f);
    // fuel farm, south-east: two white tanks in a raised bund with a bowser and a hose run
    m.slab(285, 201, 22, 12, 0.9f, 1.6f, cConc).m(MATTE).t(T_PANEL, 8).bev(0.3f);
    for (int i = 0; i < 2; i++) {
        const float x = 276 + i * 18.0f;
        m.cyl('z', x, 201, 0.9f + 1.6f + 7, 6, 7, cHull).m(METAL).t(T_PANEL, 6).bev(0.6f);
        m.cyl('z', x, 201, 0.9f + 1.6f + 14.3f, 3.4f, 0.4f, cSteel).m(METAL).bev(0.2f);
    }
    m.rod({ 276, 201, 8 }, { 294, 201, 8 }, 0.7f, cSteel).m(METAL);
    m.slab(270, 178, 8, 4.5, 0.9f, 5 * HT, cHull).m(PAINT).bev(0.4f);
    m.slab(265, 178, 3.2f, 3.6f, 0.9f + 5 * HT, 2.4f, cWall2).bev(0.3f);
    m.box(270, 173.4f, 0.9f + 5 * HT + 0.5f, 8, 0.4f, 0.5f, cAvRed).t(T_STRIPE, 3, cPaint).bev(0.1f);
    m.box(255, H - 10, 0.9f + 1.2f, 28, 1.5f, 0.3f, cPaint).t(T_HAZARD, 3.0f, cDark).bev(0.1f);
}
M3D_BUILD(B_C_AIRFIELD, cAirfield)

// ---------------------------------------------------------------- Data Center, 3 x 3 tiles
// A secure research block with a glazed south band, a roof crowded with plant, an antenna farm, two dishes,
// a turning-dish mount on the roof and a fenced backup yard with generators.
static void cTech(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX, zH = 0.9f + 14 * HT;
    m.dirt = 0.10f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the block, x 8..88 and y 14..62, its south face at y = 62
    m.slab(48, 38, 40, 24, 0.9f, zH - 0.9f, cWall).m(PAINT).t(T_PANEL, 11).bev(0.8f);
    m.box(48, 62.3f, 0.9f + 2.8f, 40, 0.35f, 2.8f, cWall2).m(MATTE).t(T_PANEL, 8).bev(0.2f);   // plinth
    winBand(m, 23, 62.3f, 0.9f + 7 * HT, 24, 5.4f, 6);
    winBand(m, 66, 62.3f, 0.9f + 7 * HT, 34, 5.4f, 6);
    rollDoor(m, 40, 62, 0.9f, 12, 11 * HT, cSteel);
    m.box(30, 62.4f, 0.9f + 7.8f, 1.2f, 0.5f, 2.0f, cDark).m(MATTE).bev(0.15f);                  // card reader
    m.ball(30.6f, 62.7f, 0.9f + 9.0f, 0.45f, 0.45f, 0.7f, cGlow).m(EMIT);
    m.slab(62, 62.3f, 11, 1.4f, 0.9f + 8.6f, 0.5f, OWNER).bev(0.12f);                             // owner's band over the door line
    m.slab(48, 38, 40.6f, 24.6f, zH, 0.55f, cRoof).m(PAINT).t(T_PANEL, 9).bev(0.25f);
    // roof plant in two rows
    for (int i = 0; i < 3; i++) plant(m, 17 + i * 12, 28, 10, 7, zH + 0.55f, 3.4f, i == 1, 2.6f);
    for (int i = 0; i < 6; i++) plant(m, 17 + i * 12, 38.5f, 10, 7, zH + 0.55f, 3.4f, (i == 1 || i == 4), 2.6f);
    // antenna farm and dishes on the north-west roof, and the mount for the live turning dish
    dish(m, 22, 9, zH + 0.55f, 5.0f, cHull);
    dish(m, 34, 14, zH + 0.55f, 3.8f, cHull);
    mast(m, 12, 10, zH + 0.55f, 22 * HT, cAvRed);
    m.box(44, 8, zH + 0.55f + 11.4f, 0.4f, 0.4f, 16 * HT * 0.5f, cSteel).m(METAL).bev(0.1f);
    m.ball(44, 8, zH + 0.55f + 22 * HT + 0.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
    m.cyl('z', 70, 12, zH + 0.9f, 9.2f, 0.5f, cSteel).m(METAL).t(T_PANEL, 6).bev(0.2f);
    m.cyl('z', 70, 12, zH + 1.6f, 6.0f, 0.5f, cRoof).bev(0.2f);
    m.slab(78, 22, 5, 1.5f, zH + 0.55f, 0.6f, OWNER).bev(0.12f);
    // the fenced backup yard: three generator canopies with stacks and a day tank
    for (int i = 0; i < 3; i++) {
        const float x = 19 + i * 26, y = 74;
        m.slab(x, y, 9, 6, 0.9f, 6.0f, cHull2).m(PAINT).t(T_PANEL, 6).bev(0.5f);
        m.box(x, y + 6.1f, 3.6f, 7, 0.35f, 2.4f, cDark).t(T_GRILLE, 1.8f).bev(0.1f);
        m.cyl('z', x + 4, y - 0.5f, 6.5f, 1.0f, 5, cSteel).m(METAL).bev(0.2f);
        m.box(x, y, 12.6f, 8.6f, 5.6f, 0.5f, cRoof).t(T_RIBS, 2.4f).bev(0.15f);
    }
    m.cyl('z', 84, 84, 0.9f + 5, 4, 5, cHull).m(METAL).t(T_BAND, 1.2f, cDark).bev(0.4f);
    m.cyl('z', 84, 84, 0.9f + 10.3f, 3.2f, 0.3f, cDark).bev(0.15f);
    for (int i = 0; i <= 5; i++) { const float x = 4 + i * 17.6f; m.box(x, 66.6f, 3.2f, 0.35f, 0.35f, 3.2f, cSteel).m(METAL).bev(0.1f); m.box(x, 93.4f, 3.2f, 0.35f, 0.35f, 3.2f, cSteel).m(METAL).bev(0.1f); }
    m.box(48, 66.3f, 5.4f, 44, 0.25f, 0.25f, cSteel).m(METAL).f(F_NOSHADOW);
    m.box(48, 93.7f, 5.4f, 44, 0.25f, 0.25f, cSteel).m(METAL).f(F_NOSHADOW);
    for (int s = -1; s <= 1; s += 2) { m.box(4.3f, 80, 5.4f, 0.25f, 14, 0.25f, cSteel).m(METAL).f(F_NOSHADOW); m.box(91.7f, 80, 5.4f, 0.25f, 14, 0.25f, cSteel).m(METAL).f(F_NOSHADOW); }
    m.ball(10, 90, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_TECH, cTech)

// ---------------------------------------------------------------- Bitcoin Datacenter, 3 x 2 tiles
// Three containerised server halls with louvred south faces, a chiller and two fans on each lid, the minted coin on the
// middle roof, a cable tray and coolant loop in front, dry coolers and a transformer with bushings.
static void cMiner(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 2 * BPX, zC = 0.9f + 12 * HT;
    m.dirt = 0.10f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    for (int i = 0; i < 3; i++) {
        const float x = 3 + i * 31.0f, cx = x + 15.0f;
        m.slab(cx, 23, 15, 17, 0.9f, zC - 0.9f, cWall).m(PAINT).t(T_RIBS, 3.4f).bev(0.6f);       // the container, ribs along its length
        m.slab(cx, 23, 15, 17, zC, 0.5f, cRoof).t(T_PANEL, 7).bev(0.2f);
        m.box(cx, 40.3f, 0.9f + 7.4f, 12.5f, 0.35f, 4.4f, cDark).m(MATTE).t(T_GRILLE, 2.2f).bev(0.1f);   // louvred intake
        for (int s = -1; s <= 1; s += 2) m.box(cx + s * 13.8f, 40, 0.9f + 6.5f, 0.7f, 0.7f, 6.5f, cSteel).m(METAL).bev(0.15f);   // corner posts
        // the rooftop chiller: a pale casing with two big fans
        m.slab(cx, 21.5f, 12, 5.5f, zC + 0.5f, 4.0f, cHull).m(PAINT).t(T_PANEL, 6).bev(0.4f);
        for (int k = 0; k < 2; k++) {
            const float fx = cx - 9 + k * 12.0f;
            m.cyl('z', fx, 21.5f, zC + 4.6f, 4.4f, 0.35f, cDark).bev(0.2f);
            for (int q = 0; q < 2; q++) m.box(fx, 21.5f, zC + 5.1f, 3.4f, 1.4f, 0.35f, cSteel).m(METAL).rz(35 + q * 90).f(F_NOSHADOW);
            m.cyl('z', fx, 21.5f, zC + 5.3f, 0.9f, 0.3f, cHull2).m(METAL).bev(0.1f);
        }
        m.slab(cx + 5.5f, 9, 5.5f, 1.5f, zC + 0.5f, 2.2f, cHull2).m(METAL).t(T_GRILLE, 1.4f).bev(0.15f);   // roof vent
        m.box(cx + 11.5f, 40.2f, 0.9f + 6.4f, 1.0f, 0.4f, 0.5f, cGlow).m(EMIT);                             // status light
        if (i == 0) m.slab(cx - 3, 12.5f, 5.5f, 1.3f, zC + 0.5f, 0.5f, OWNER).bev(0.12f);                    // owner's band on the first lid
    }
    // the minted coin on the middle roof (the game draws its live glow here)
    m.cyl('z', 48, 10, zC + 1.0f, 5.6f, 0.35f, cCoin).m(METAL).bev(0.2f);
    m.cyl('z', 48, 10, zC + 1.4f, 4.7f, 0.15f, cCoinLit).m(METAL).bev(0.1f);
    m.box(46.6f, 10, zC + 1.6f, 0.45f, 2.8f, 0.12f, cCoinSeam).bev(0.05f).f(F_NOSHADOW);
    m.box(48.4f, 8.6f, zC + 1.6f, 1.5f, 0.45f, 0.12f, cCoinSeam).bev(0.05f).f(F_NOSHADOW);
    m.box(48.6f, 11.4f, zC + 1.6f, 1.7f, 0.45f, 0.12f, cCoinSeam).bev(0.05f).f(F_NOSHADOW);
    m.box(47.5f, 10, zC + 1.6f, 0.4f, 0.4f, 0.12f, cCoinSeam).bev(0.05f).f(F_NOSHADOW);
    // the cable tray and the coolant loop along the front of the containers
    m.slab(45, 42.5f, 42, 1.1f, 0.9f, 2.2f, cSteel).m(METAL).t(T_PANEL, 6).bev(0.2f);
    m.rod({ 3, 46, 1.8f }, { 93, 46, 1.8f }, 1.3f, cSteel).m(METAL).bev(0.2f);
    m.rod({ 3, 49, 1.5f }, { 93, 49, 1.5f }, 1.0f, C(0xA8C4D8)).m(METAL).bev(0.2f);
    for (float x = 8.0f; x < 92.0f; x += 8.0f) m.box(x, 47.5f, 1.0f, 0.5f, 2.6f, 0.5f, cSteel).m(METAL).bev(0.1f);
    // two dry coolers, south-west, each with a fan on its lid
    for (int i = 0; i < 2; i++) plant(m, 12.5f + i * 16.0f, 56.5f, 13, 9, 0.9f, 7 * HT, true, 3.4f);
    // the transformer, south-east: a finned tank, three bushings and an owner's band on the lid
    m.slab(77, 58, 13, 6, 0.9f, 8 * HT, cWall).m(PAINT).t(T_RIBS, 2.2f).bev(0.5f);
    m.slab(77, 58, 13, 6, 0.9f + 8 * HT, 0.5f, cRoof).t(T_PANEL, 5).bev(0.2f);
    for (int i = 0; i < 3; i++) { const float bx = 71 + i * 6.0f; m.cyl('z', bx, 52.5f, 0.9f + 8 * HT + 2.6f, 0.9f, 2.6f, cHull).m(PAINT).bev(0.2f); m.cyl('z', bx, 52.5f, 0.9f + 8 * HT + 5.4f, 1.3f, 0.4f, cSteel).m(METAL).bev(0.1f); }
    m.slab(71, 56.5f, 5.5f, 1.2f, 0.9f + 8 * HT + 0.5f, 0.5f, OWNER).bev(0.12f);
    m.rod({ 64, 58, 3 }, { 59, 58, 3 }, 0.6f, cSteel).m(METAL);
    m.ball(90, 60, 1.6f, 0.7f, 0.7f, 0.7f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_MINER, cMiner)

// ---------------------------------------------------------------- Laser Turret, 1 x 1 tile
// An octagonal armoured plinth, a steel drum and the round pedestal the game turns its head on. Pedestal top z = 9.5.
static void cLaser(Model& m, int) {
    using namespace pal;
    m.dirt = 0.08f;
    m.slab(16, 16, 15, 15, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 12).bev(0.4f);
    std::vector<P2> oct;
    for (int i = 0; i < 8; i++) { const float a = 0.3927f + i * 0.7854f; oct.push_back({ 16 + std::cos(a) * 14.6f, 16 + std::sin(a) * 14.6f }); }
    m.plan(oct, 0.9f, 4.8f, cWall2).m(PAINT).t(T_PANEL, 7).bev(0.7f);                     // the armour slab
    m.slab(16, 16, 12.2f, 12.2f, 4.8f, 0.4f, cSlabDk).m(MATTE).t(T_PANEL, 6).bev(0.2f);
    m.cyl('z', 16, 16, 6.5f, 11.5f, 1.7f, cSteel).m(METAL).t(T_RIBS, 2.6f).bev(0.4f);      // the drum
    m.cyl('z', 16, 16, 8.5f, 10.2f, 0.28f, cGlow).m(EMIT).bev(0.1f);                       // the cyan ring
    m.cyl('z', 16, 16, 9.2f, 9.4f, 0.7f, cHull2).m(PAINT).t(T_PANEL, 5).bev(0.3f);         // the pedestal
    m.cyl('z', 16, 16, 9.55f, 5.4f, 0.25f, cTrim).bev(0.15f);
    for (int i = 0; i < 4; i++) { const float a = 0.7854f + i * 1.5708f; m.slab(16 + std::cos(a) * 13.0f, 16 + std::sin(a) * 13.0f, 1.6f, 1.6f, 4.8f, 3.0f, cRoof).m(PAINT).bev(0.4f); }
    m.slab(16, 27.2f, 2.0f, 1.6f, 4.8f, 0.9f, OWNER).bev(0.2f);                            // the owner's stud on the south shoulder
    m.box(16, 29.6f, 1.9f, 8, 0.5f, 0.5f, cSteel).m(METAL).bev(0.15f);
}
M3D_BUILD(B_C_LASER, cLaser)

// The laser head: a compact housing with a long emitter barrel, pointing +x on the pedestal.
static void cLaserHead(Model& m, int) {
    using namespace pal;
    const float zP = 9.5f;
    m.ground = false;
    m.dirt = 0.0f;
    m.cyl('z', 0, 0, zP + 0.5f, 8.6f, 0.5f, cSteel).m(METAL).t(T_PANEL, 5).bev(0.25f);      // the turn ring
    m.taper(0, 0, zP + 1.0f, 7.6f, 8.0f, 4.6f, 6.2f, 6.6f, cHull).m(PAINT).t(T_PANEL, 6).bev(0.6f);
    m.box(0, 0, zP + 5.0f, 6.0f, 7.4f, 0.4f, cRoof).t(T_PANEL, 5).bev(0.2f);
    m.slab(-1.0f, 0, 4.4f, 6.6f, zP + 5.4f, 0.5f, OWNER).bev(0.15f);                          // owner's colour on the housing lid
    m.slab(-7.4f, 0, 2.6f, 4.6f, zP + 2.0f, 4.2f, cDark).m(MATTE).t(T_PANEL, 5).bev(0.4f);    // the rear power pack
    m.box(-7.4f, 4.9f, zP + 4.2f, 2.0f, 0.3f, 1.4f, cSlabDk).t(T_GRILLE, 1.4f).bev(0.1f);
    m.cyl('x', 18.5f, 0, zP + 4.6f, 2.4f, 12.5f, cSteel).m(METAL).t(T_PANEL, 8).bev(0.3f);     // the emitter barrel
    m.cyl('x', 8.0f, 0, zP + 4.6f, 3.1f, 3.0f, cHull2).m(PAINT).bev(0.3f);                     // barrel collar
    m.cyl('x', 30.6f, 0, zP + 4.6f, 2.6f, 0.7f, cDark).m(METAL).bev(0.15f);
    m.cyl('x', 31.2f, 0, zP + 4.6f, 1.5f, 0.2f, cGlow).m(EMIT).bev(0.1f);                      // the lens
    m.cyl('x', 20.0f, 0, zP + 4.6f, 0.9f, 9.0f, cGlow).m(EMIT).bev(0.0f);                      // the pumped line
    for (int s = -1; s <= 1; s += 2) m.rod({ -2.0f, s * 5.5f, zP + 3.0f }, { 6.0f, s * 4.0f, zP + 4.6f }, 0.6f, cSteel).m(METAL).bev(0.1f);
    m.ball(0, 0, zP + 6.6f, 1.1f, 1.1f, 0.9f, cGlaze).m(EMIT);
}
M3D_PROP("head.c_laser", cLaserHead)

// ---------------------------------------------------------------- Patriot Battery, 2 x 2 tiles
// An armoured plinth with a generator, a radar mast and a dish, and the round centred pedestal the launcher turns on.
// Pedestal top z = 14.6.
static void cPatriot(Model& m, int) {
    using namespace pal;
    const float W = 2 * BPX, H = 2 * BPX, zPl = 0.9f + 7 * HT;
    m.dirt = 0.10f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the plinth, x 10..54 and y 18..54, with a row of cyan windows on its south face
    m.slab(32, 36, 22, 18, 0.9f, zPl - 0.9f, cMid).m(PAINT).t(T_PANEL, 9).bev(0.7f);
    winBand(m, 32, 54.3f, 0.9f + 3.5f, 36, 4.0f, 7);
    m.slab(32, 36, 19, 15, zPl, 0.5f, cHull2).m(PAINT).t(T_PANEL, 7).bev(0.25f);              // the armour plate on the roof
    m.box(32, 36, zPl + 0.6f, 15, 0.35f, 0.2f, cGlow).m(EMIT).bev(0.1f);                       // the cyan strip
    m.slab(32, 32, 13, 13, 4.8f + 6.0f, 0.0f, cTrim);                                          // (spacer: nothing drawn here)
    // the centred pedestal the launcher turns on: two drums and a glowing race
    m.cyl('z', 32, 32, zPl + 0.5f + 1.4f, 13, 1.4f, cSteel).m(METAL).t(T_PANEL, 6).bev(0.4f);
    m.cyl('z', 32, 32, zPl + 0.5f + 3.05f, 10.6f, 0.25f, cGlow).m(EMIT).bev(0.1f);
    m.cyl('z', 32, 32, zPl + 0.5f + 3.7f, 10.5f, 0.65f, cHull2).m(PAINT).t(T_PANEL, 5).bev(0.3f);
    // the generator shed north-west, with a louvred vent
    m.slab(11, 11, 7, 5, 0.9f, 6 * HT, cHull).m(PAINT).t(T_PANEL, 6).bev(0.5f);
    m.box(11, 16.2f, 0.9f + 3.0f, 5, 0.35f, 2.2f, cDark).m(MATTE).t(T_GRILLE, 1.5f).bev(0.1f);
    m.slab(11, 11, 7.2f, 5.2f, 0.9f + 6 * HT, 0.4f, cRoof).t(T_RIBS, 2.4f).bev(0.15f);
    m.rod({ 15, 8, 0.9f + 6 * HT }, { 15, 8, 0.9f + 6 * HT + 5 }, 0.7f, cSteel).m(METAL);
    // the radar mast and its dish on the north-east corner
    mast(m, 56, 14, 0.9f, 22 * HT, cAvRed);
    dish(m, 52, 8, 0.9f, 4.4f, cHull2);
    // four corner pads and the owner's colour band along the south edge
    for (int i = 0; i < 4; i++) { const float x = i < 2 ? 6.0f : 58.0f, y = (i & 1) ? 54.0f : 24.0f; m.slab(x, y, 2, 3, 0.9f, 2.0f, cRoof).m(PAINT).t(T_PANEL, 4).bev(0.3f); }
    m.slab(48, 56.5f, 7, 1.6f, 0.9f, 1.6f, OWNER).bev(0.2f);
    m.box(24, 58, 1.6f, 12, 1.6f, 0.25f, cPaint).t(T_HAZARD, 2.4f, cDark).bev(0.1f);
    m.ball(2, 2, 1.5f, 0.6f, 0.6f, 0.5f, cGlow).m(EMIT); m.ball(62, 2, 1.5f, 0.6f, 0.6f, 0.5f, cGlow).m(EMIT);
}
M3D_BUILD(B_C_PATRIOT, cPatriot)

// The Patriot launcher head: a box of missile cells angled up, pointing +x on the pedestal.
static void cPatriotHead(Model& m, int) {
    using namespace pal;
    const float zP = 14.6f;
    m.ground = false;
    m.dirt = 0.0f;
    m.cyl('z', 0, 0, zP + 0.6f, 9.2f, 0.6f, cSteel).m(METAL).t(T_PANEL, 5).bev(0.25f);        // the turn ring
    m.slab(-1, 0, 5.0f, 5.6f, zP + 1.2f, 3.2f, cMid).m(PAINT).t(T_PANEL, 5).bev(0.4f);        // the yoke base
    for (int s = -1; s <= 1; s += 2) m.box(-1, s * 5.2f, zP + 5.0f, 4.0f, 1.1f, 3.4f, cHull2).m(PAINT).bev(0.3f);
    m.frame(0, 0, zP + 5.2f, -14);                                                            // the launcher, muzzle end up
    m.taper(0, 0, 0, 17, 12.5f, 5.0f, 17, 12.5f, cHull).m(PAINT).t(T_PANEL, 7).bev(0.5f);
    m.slab(0, 0, 16.5f, 12.0f, 5.0f, 0.5f, OWNER).bev(0.15f);                                  // owner's colour on the launcher lid
    for (int j = 0; j < 4; j++) {
        const float ty = -7.5f + j * 5.0f;
        m.cyl('x', -1.0f, ty, 2.4f, 2.2f, 16.0f, C(0x3E4450)).m(METAL).t(T_RIBS, 3.0f).bev(0.25f);
        m.cyl('x', 15.2f, ty, 2.4f, 2.4f, 0.7f, cHull2).m(METAL).bev(0.15f);
        m.cyl('x', 15.9f, ty, 2.4f, 1.5f, 0.25f, C(0xE6E9EE)).m(EMIT).bev(0.1f);                     // the warhead nose
    }
    m.box(-18.5f, 0, 2.4f, 1.5f, 10.0f, 2.6f, cDark).m(MATTE).t(T_PANEL, 4).bev(0.2f);          // the rear plate
    for (int s = -1; s <= 1; s += 2) m.rod({ -12, s * 6.0f, 0.4f }, { -2, s * 10.5f, -3.0f }, 0.7f, cSteel).m(METAL).bev(0.1f);
    m.slab(-14, 0, 3.0f, 4.0f, -2.0f, 1.2f, cSlabDk).m(MATTE).t(T_GRILLE, 1.6f).bev(0.1f);
    m.frame();
}
M3D_PROP("head.c_patriot", cPatriotHead)

// ---------------------------------------------------------------- Nuke Ramp, 3 x 3 tiles
// An open silo pit with its two blast doors slid aside, a ring of guide rings and studs, two gantry towers and two fuel tanks.
// state 1 adds the missile standing in the shaft.
static void cNuke(Model& m, int state) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX, cx = 48, cy = 50, zG = 0.9f + 18 * HT;
    m.dirt = 0.10f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    m.box(cx, 5.7f, 1.15f, 44, 1.7f, 0.25f, cPaint).t(T_HAZARD, 3.0f, cDark).bev(0.1f);
    m.box(cx, H - 5.7f, 1.15f, 44, 1.7f, 0.25f, cPaint).t(T_HAZARD, 3.0f, cDark).bev(0.1f);
    // the pit: a raised concrete collar, the dark shaft and its guide rings
    m.cyl('z', cx, cy, 1.6f, 29, 0.7f, cSlabDk).m(MATTE).t(T_PANEL, 8).bev(0.3f);
    m.cyl('z', cx, cy, 1.9f, 24, 0.5f, C(0x0C0F14)).m(MATTE).bev(0.2f);
    for (int i = 0; i < 3; i++) m.cyl('z', cx, cy, 2.45f, 8.0f + i * 5.5f, 0.12f, cGlow).m(EMIT).bev(0.05f);
    m.cyl('z', cx, cy, 2.5f, 27, 0.15f, cAvRed).m(EMIT).bev(0.1f);
    for (int i = 0; i < 12; i++) { const float a = i * 0.5236f; m.ball(cx + std::cos(a) * 22.0f, cy + std::sin(a) * 22.0f, 2.9f, 0.9f, 0.9f, 0.5f, i % 3 ? cGlow : cAvRed).m(EMIT); }
    // the two blast doors, slid aside to either hand
    for (int s = -1; s <= 1; s += 2) {
        const float dx = cx + s * 21.0f;
        m.slab(dx, cy, 12, 22, 0.9f, 4.6f, cHull2).m(PAINT).t(T_PANEL, 8).bev(0.6f);
        m.slab(dx, cy, 10, 20, 5.5f, 1.6f, cSteel).m(METAL).t(T_RIBS, 3.2f).bev(0.3f);
        m.box(dx, cy, 7.3f, 0.35f, 19, 0.15f, cGlow).m(EMIT).f(F_NOSHADOW);
    }
    // the gantry towers either side, plated and lit, with a door on their south faces
    for (int s = -1; s <= 1; s += 2) {
        const float tx = s < 0 ? 11.0f : W - 11.0f;
        m.slab(tx, 40, 6, 28, 0.9f, zG - 0.9f, cMid).m(PAINT).t(T_PANEL, 10).bev(0.7f);
        m.slab(tx, 40, 6.2f, 28.2f, zG, 0.6f, cRoof).t(T_RIBS, 2.8f).bev(0.2f);
        for (int i = 0; i < 4; i++) m.box(tx, 40, zG - 3.0f - i * 4.6f, 6.1f, 28.1f, 0.3f, cSteel).m(METAL).bev(0.1f).f(F_NOSHADOW);
        winBand(m, tx, 68.3f, 0.9f + 10 * HT, 9, 5 * HT, 9);
        m.box(tx, 68.4f, 0.9f + 4.0f, 3.2f, 0.35f, 4.0f, cDark).m(MATTE).t(T_GRILLE, 1.8f).bev(0.1f);
        m.slab(tx, 30, 4.4f, 1.4f, zG + 0.6f, 0.6f, OWNER).bev(0.15f);
        m.ball(tx, 40, zG + 1.8f, 1.2f, 1.2f, 0.9f, cAvRed).m(EMIT);
    }
    // fuel tanks along the south edge
    for (int i = 0; i < 2; i++) {
        const float tx = 34 + i * 28.0f;
        m.cyl('z', tx, H - 8, 0.9f + 5, 6, 5, cHull).m(METAL).t(T_PANEL, 6).bev(0.5f);
        m.cyl('z', tx, H - 8, 0.9f + 10.4f, 4.2f, 0.4f, cSteel).m(METAL).bev(0.2f);
        m.rod({ tx - 6, H - 8, 4 }, { tx + 6, H - 8, 4 }, 0.6f, cSteel).m(METAL);
    }
    m.rod({ 48, H - 8, 3 }, { 48, H - 20, 3 }, 0.7f, cSteel).m(METAL);
    if (state) {
        // the warhead standing in the shaft: body, band, nose and fins
        m.cyl('z', cx, cy, 3.0f + 8.0f, 6, 8.0f, C(0xE8EDEF)).m(PAINT).t(T_PANEL, 9).bev(0.6f);
        m.cyl('z', cx, cy, 3.0f + 16.3f, 6.2f, 0.35f, cPaint).m(METAL).t(T_BAND, 1.4f, C(0xE6BA3C)).bev(0.15f);
        m.cyl('z', cx, cy, 3.0f + 19.5f, 6, 3.0f, C(0xE8EDEF), 0.3f).m(PAINT).bev(0.3f);
        m.cyl('z', cx, cy, 3.0f + 23.4f, 1.9f, 1.0f, cAvRed).m(PAINT).bev(0.2f);
        for (int i = 0; i < 3; i++) { const float a = i * 2.0944f + 0.5f; m.frame(cx, cy, 4.0f, a * 57.3f); m.plan({ { 4.0f, 0 }, { 12.5f, 0 }, { 4.0f, 8.0f } }, 0, 0.7f, cHull2).m(PAINT).bev(0.15f); m.frame(); }
        m.cyl('z', cx, cy, 2.6f, 9.0f, 0.15f, C(0xFF8A3C)).m(EMIT).bev(0.05f);
    }
}
M3D_BUILD(B_C_NUKE, cNuke)

}  // namespace
