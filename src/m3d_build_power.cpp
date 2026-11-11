// One Hour - 3D models: the two power plants (the worked examples for structures; see m3d.h / m3d_reg.h).
#include "m3d_parts.h"

using namespace m3d;

// Clanker Generator Hall, 3 x 2 tiles: a corrugated engine hall, two rusty exhaust stacks, a day tank and fuel drums.
static void kPower(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 2 * BPX;
    m.dirt = 0.35f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);                 // the yard
    // the hall: plate walls with a row of lit windows, a ribbed gable roof
    m.slab(36, 31, 31, 19, 0.9f, 15, kWall2).t(T_WINDOWS, 7.5f, kWin).bev(0.5f);
    m.profileX({ { 10.5f, 15.9f }, { 31, 22.5f }, { 51.5f, 15.9f } }, 3.5f, 68.5f, kRoof).t(T_RIBS, 2.6f).bev(0.5f);
    m.box(36, 31, 22.3f, 30, 1.2f, 0.6f, kWall2).bev(0.3f);                                                        // ridge cap
    m.box(58, 31, 22.75f, 7, 1.6f, 0.35f, OWNER).bev(0.2f);                                                        // the owner's colour on the ridge
    m.box(17, 24, 20.6f, 5, 3, 1.2f, kSteel).t(T_GRILLE, 1.4f).bev(0.3f).rx(-18);                                   // roof vent
    m.box(46, 50.5f, 6.4f, 9, 0.6f, 5.5f, kDark).t(T_RIBS, 1.8f).bev(0.2f);                                         // roller door, south wall
    m.box(46, 50.9f, 12.6f, 10, 0.5f, 0.9f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);
    m.box(24, 50.4f, 13.2f, 2.2f, 0.5f, 0.7f, kGlow).m(EMIT);                                                       // lamp over the side door
    // exhaust stacks, braced to the wall
    for (int i = 0; i < 2; i++) {
        const float x = 76 + i * 11.0f, top = 37 - i * 6.0f;
        m.cyl('z', x, 28, 0.9f + top / 2, 4.4f, top / 2, kRust, 0.86f).m(METAL).t(T_PANEL, 7).bev(0.4f);
        m.cyl('z', x, 28, top + 0.6f, 4.3f, 0.9f, kDark).bev(0.3f);                                                 // sooty rim
        m.box(x - 5.5f, 28, 14, 2.4f, 0.5f, 0.5f, kSteel).m(METAL).f(F_NOSHADOW);
    }
    // the day tank on its cradle, a feed pipe into the hall
    m.cyl('x', 80, 52, 7.6f, 5.6f, 9.5f, kWall).t(T_PANEL, 6).bev(0.8f);
    for (int s = -1; s <= 1; s += 2) m.box(80 + s * 6.0f, 52, 2.2f, 1.0f, 5.0f, 1.4f, kConc).m(MATTE);
    m.box(80, 52, 13.4f, 3.0f, 1.2f, 0.3f, kHaz).t(T_HAZARD, 1.6f, kDark).bev(0.1f);
    m.rod({ 69, 44, 8 }, { 73, 50, 8 }, 0.9f, kSteel).m(METAL);
    for (int i = 0; i < 3; i++) m.cyl('z', 9 + i * 5.4f, 58, 3.4f, 2.3f, 2.5f, i == 1 ? kRust : kWall).t(T_BAND, 0.8f, kDark).bev(0.3f);   // drums
}
M3D_BUILD(B_K_POWER, kPower)

// Cyber Fusion Reactor, 3 x 2 tiles: two cooler blocks, a ribbed containment dome, a steel pressure vessel, a transformer row and a pylon.
static void cPower(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 2 * BPX;
    m.dirt = 0.12f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // cooler blocks at the back: louvred sides, a dark fan well on top
    for (int i = 0; i < 2; i++) {
        const float cx = i ? 49.0f : 18.0f, hw = i ? 11.0f : 14.0f;
        m.slab(cx, 12, hw, 6, 0.9f, 9.5f, cWall).t(T_PANEL, 7).bev(0.7f);
        m.box(cx, 18.2f, 5.6f, hw - 2, 0.4f, 2.8f, cDark).t(T_GRILLE, 1.3f).bev(0.1f);
        m.cyl('z', cx - 4, 12, 10.6f, 3.6f, 0.4f, cDark).bev(0.2f); m.cyl('z', cx + 5, 12, 10.6f, 3.6f, 0.4f, cDark).bev(0.2f);
    }
    m.box(11, 18.5f, 9.2f, 5, 0.3f, 1.0f, OWNER).bev(0.1f);                                                         // owner's band on the first cooler
    // containment: a drum with a ribbed dome and a glowing viewing port
    m.cyl('z', 28, 42, 6.9f, 12, 6.0f, cWall2).t(T_PANEL, 6).bev(0.6f);
    m.dome(28, 42, 12.9f, 11.4f, 11.4f, 8.5f, cRoof).t(T_RIBS, 3.2f);
    m.cyl('z', 28, 42, 21.6f, 2.2f, 0.7f, cTrim).bev(0.3f);
    m.box(28, 54.1f, 6.6f, 2.4f, 0.4f, 1.5f, cGlow).m(EMIT);
    m.cyl('z', 28, 42, 12.6f, 12.3f, 0.5f, OWNER).bev(0.2f);                                                        // owner's ring under the dome
    // the pressure vessel: banded steel, domed lid
    m.cyl('z', 70, 42, 13.4f, 9.4f, 12.5f, cSteel).m(METAL).t(T_BAND, 1.2f, cDark).bev(0.5f);
    m.dome(70, 42, 25.9f, 9.4f, 9.4f, 3.6f, cSteel).m(METAL);
    m.cyl('z', 70, 42, 29.6f, 2.6f, 0.5f, cDark).bev(0.2f);
    m.rod({ 40, 38, 7 }, { 61, 38, 7 }, 1.3f, cSteel).m(METAL); m.rod({ 40, 44, 4 }, { 61, 44, 4 }, 1.0f, cTrim);   // coolant lines
    // transformer row and the pylon
    for (int i = 0; i < 3; i++) {
        m.slab(12.5f + i * 17.0f, 58.5f, 6.0f, 2.8f, 0.9f, 5.0f, cWall2).t(T_RIBS, 1.5f).bev(0.4f);
        m.cyl('z', 9.5f + i * 17.0f, 58.5f, 7.2f, 0.8f, 1.4f, cTrim); m.cyl('z', 15.5f + i * 17.0f, 58.5f, 7.2f, 0.8f, 1.4f, cTrim);
    }
    for (int s = -1; s <= 1; s += 2) m.rod({ 88 + s * 3.5f, 60, 0.9f }, { 88 + s * 0.6f, 60, 44 }, 0.55f, cSteel).m(METAL);
    m.box(88, 60, 30, 5.0f, 0.4f, 0.4f, cSteel).m(METAL); m.box(88, 60, 40, 4.0f, 0.4f, 0.4f, cSteel).m(METAL);
    m.ball(88, 60, 45, 1.0f, 1.0f, 1.0f, C(0xFF5A48)).m(EMIT);
    m.box(66, 61.5f, 1.2f, 10, 1.2f, 0.25f, C(0xE8C23E)).t(T_HAZARD, 2.2f, cDark).bev(0.1f);
}
M3D_BUILD(B_C_POWER, cPower)
