// One Hour - 3D models: the two underground bunkers (the Cyber Deep Bunker and the Clanker Underground Bunker). The layout is the painted one in
// artb_bunker.cpp, kept at the same spots: a low mound of three stepped solids (armour plinth, glacis, revetment) whose top is the deck, the
// surface hatch in its collar at the centre, recessed miniguns' bays east and west, flush overdrive hatches at two corners, access hatches and
// vents, a blast door at the top of a ramp down the south-east glacis, and the team flag on the north edge.
// The spots the upgrades fit (hospital, triple tubes, rain rack, lift pad, laser pads, guidance array, radio mast) stay bare: the overlays are
// drawn over them.
// Cyber: pale ceramic plates on graphite, cobalt trim, cyan light strips. Clanker: olive steel, earth glacis, riveted concrete, rust, hazard paint.
// Screen and model: a point at height z is drawn OBL_K * z pixels higher, so a part whose top sits at height zt and reads at screen (sx, sy)
// stands at ground y = sy + OBL_K * zt. The helpers below work in screen positions for the deck, which keeps the overlays' spots exact.
#include "m3d_parts.h"
#include <cmath>

using namespace m3d;

namespace {

const float ZD = 8.6f;                   // the deck top (the revetment lifts 6 px on screen)
const float DIRX = 0.6f, DIRY = 0.8f;    // the lane out of the south-east corner
const float LANE_DEG = 53.13f;           // the turn about z that lays a solid's local x along the lane (atan2(0.8, 0.6))
const float LANE_T = 51.25f;             // the ramp's top end: along the lane from the mound's centre, where the glacis is at half-size 41

// a part whose top sits at height zt, its top centre reading at screen (sx, sy): its ground centre
inline float gy(float sy, float zt) { return sy + OBL_K * zt; }
// a box on the deck: its top at zt, its top rectangle centred on screen (sx, sy) with half sizes hx, hy, and h tall
Prim& deckBox(Model& m, float sx, float sy, float hx, float hy, float zt, float h, Color c) { return m.box(sx, gy(sy, zt), zt - h * 0.5f, hx, hy, h * 0.5f, c); }
// a round part on the deck, its top at zt and its top centre at screen (sx, sy)
Prim& deckCyl(Model& m, float sx, float sy, float r, float zt, float h, Color c) { return m.cyl('z', sx, gy(sy, zt), zt - h * 0.5f, r, h * 0.5f, c); }

void bunker(Model& m, bool cy) {
    using namespace pal;
    m.dirt = cy ? 0.2f : 0.35f;
    const Color dark = cy ? cDark : kDark, steel = cy ? cSteel : kSteel, lamp = cy ? C(0x58ECFF) : C(0xFFB23E);
    const Color plinthC = cy ? C(0x3A4250) : C(0x6C6E60), glacisC = cy ? C(0x98A7B8) : C(0x74644A), revetC = cy ? cWall : C(0x9C9072);
    const Color rampC = cy ? C(0x424E60) : C(0x78725C), stackC = cy ? cWall2 : C(0x8F8A6A);

    // the mound on one ground centre (48, 54): the armour plinth (the thick apron), the glacis, and the revetment up to the deck, whose top
    // reads 12..84 across with its centre at (48, 48). Cyber's plates are pale ceramic; Clanker's glacis is earth with grass on it
    m.taper(48, 54, 0, 46, 46, 3.2f, 44.5f, 44.5f, plinthC).m(PAINT).t(T_PANEL, 6, dark).bev(0.6f);
    if (cy) m.taper(48, 54, 3.2f, 44.5f, 44.5f, 3.4f, 38.5f, 38.5f, glacisC).m(MATTE).t(T_PANEL, 4, dark).bev(0.6f);
    else    m.taper(48, 54, 3.2f, 44.5f, 44.5f, 3.4f, 38.5f, 38.5f, glacisC).m(MATTE).t(T_NOISE, 5, C(0x586434)).bev(0.6f);
    m.taper(48, 54, 6.6f, 38.5f, 38.5f, 2.0f, 36, 36, revetC).m(MATTE).t(T_PANEL, 7, dark).bev(0.5f);

    // the blast ramp: from the glacis face at the south-east (its top end at z 4.9) down to the ground, under a blast door
    const float px = 48 + DIRX * LANE_T, py = 54 + DIRY * LANE_T;   // the ramp's top end
    const float L = 11.0f, W = 5.5f, ZR = 4.9f;
    const float cxr = px + DIRX * L, cyr = py + DIRY * L;             // its centre
    m.profileY({ { cxr - L, 0 }, { cxr + L, 0 }, { cxr - L, ZR } }, cyr - W, cyr + W, rampC).m(MATTE).t(T_PANEL, 4, dark).bev(0.3f).rz(LANE_DEG);
    const float ux = px + DIRX * 0.45f, uy = py + DIRY * 0.45f;      // the door, standing just out on the ramp's top end
    m.box(ux, uy, ZR + 1.9f, 0.45f, W, 1.9f, cy ? cSteel : C(0x6C6C60)).m(METAL).bev(0.2f).rz(LANE_DEG);
    m.box(ux, uy, ZD + 0.3f, 0.6f, W, 0.15f, OWNER).bev(0.1f).rz(LANE_DEG);                    // the door's rail in the owner's colour
    for (int s = -1; s <= 1; s += 2) m.ball(ux + s * (W + 1.0f) * 0.8f, uy - s * (W + 1.0f) * 0.6f, 5.6f, 0.8f, 0.8f, 0.8f, lamp).m(EMIT);

    // the deck: the surface hatch in its steel collar (its leaves are drawn live over it, the bolts round the rim)
    m.cyl('z', 48, gy(48, 9.8f), 9.2f, 27.5f, 0.6f, steel).m(METAL).bev(0.3f);
    m.cyl('z', 48, gy(48, 9.8f), 9.9f, 21.5f, 0.1f, dark).bev(0.1f);
    for (int k = 0; k < 8; k++) {
        const float a = k * 0.7854f + 0.39f, sx = 48 + std::cos(a) * 24.6f, sy = 48 + std::sin(a) * 24.6f;
        m.ball(sx, gy(sy, 10.0f), 10.0f - 0.2f, 0.7f, 0.7f, 0.5f, steel).m(METAL);
    }
    // recessed miniguns' bays at (20, 48) and (76, 48): a steel frame with a dark pocket in it
    for (float sx : { 20.0f, 76.0f }) {
        deckBox(m, sx, 48, 8.2f, 8.2f, ZD + 0.4f, 0.4f, steel).m(METAL).bev(0.3f);
        deckBox(m, sx, 48, 6.4f, 6.4f, ZD + 0.8f, 0.4f, dark).bev(0.2f);
    }
    // flush overdrive hatches at (20, 22) and (76, 74): a steel rim round a dark plate
    for (float sx : { 20.0f, 76.0f }) {
        const float sy = sx < 50 ? 22.0f : 74.0f;
        deckCyl(m, sx, sy, 7.0f, ZD + 0.2f, 0.2f, steel).m(METAL).bev(0.2f);
        deckCyl(m, sx, sy, 5.8f, ZD + 0.4f, 0.2f, C(0x3E4450)).bev(0.1f);
    }
    // access hatches with a wheel handle, west and east of the collar
    for (float sx : { 16.5f, 77.5f }) {
        deckBox(m, sx, 34.5f, 3.5f, 3.5f, ZD + 0.5f, 0.5f, steel).m(METAL).bev(0.3f);
        deckCyl(m, sx, 34.5f, 1.1f, ZD + 1.0f, 0.5f, cy ? cGlow : dark).bev(0.2f);
    }
    // vent grilles: dark slots on the deck (Clanker keeps its camouflage net over the one at the south-east)
    deckBox(m, 13, 60, 3.0f, 2.25f, ZD + 0.5f, 0.5f, dark).t(T_GRILLE, 1.2f, C(0x2A2C2E)).bev(0.1f);
    deckBox(m, 62, 22.5f, 2.75f, 1.75f, ZD + 0.5f, 0.5f, dark).t(T_GRILLE, 1.2f, C(0x2A2C2E)).bev(0.1f);
    if (cy) deckBox(m, 78, 60, 3.0f, 2.25f, ZD + 0.5f, 0.5f, dark).t(T_GRILLE, 1.2f, C(0x2A2C2E)).bev(0.1f);
    if (cy) {   // Cyber: cyan light strips along the north and south rims
        deckBox(m, 48, 14.6f, 22, 0.45f, ZD + 0.1f, 0.1f, cGlow).m(EMIT);
        deckBox(m, 48, 81.4f, 22, 0.45f, ZD + 0.1f, 0.1f, cGlow).m(EMIT);
    } else {    // Clanker: hazard paint on the north rim, and a camouflage net over the south-east patch
        deckBox(m, 30, 13.0f, 8.0f, 1.0f, ZD + 0.1f, 0.1f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.05f);
        deckBox(m, 65, 13.0f, 7.0f, 1.0f, ZD + 0.1f, 0.1f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.05f);
        deckBox(m, 75, 60, 9.0f, 4.0f, ZD + 0.4f, 0.4f, C(0x5C6A46)).m(MATTE).t(T_CAMO, 3.0f, C(0x3E4A2C)).bev(0.2f);
    }

    // the team flag on a pole on the north edge: the cloth is the owner's colour
    const float fx = 74, fy = gy(13, ZD);
    m.rod({ fx, fy, ZD }, { fx, fy, 40.0f }, 0.45f, steel).m(METAL);
    m.box(fx + 3.25f, fy, 36.0f, 3.25f, 0.2f, 2.5f, OWNER).bev(0.1f);

    // vent stacks standing on the ground at the north-west and east corners
    m.cyl('z', 2, 12, 15.7f, 4.0f, 15.7f, stackC).m(PAINT).bev(0.4f);
    m.cyl('z', 2, 12, 31.6f, 2.6f, 0.4f, dark).bev(0.2f);
    m.cyl('z', 96, 18, 12.85f, 4.0f, 12.85f, stackC).m(PAINT).bev(0.4f);
    m.cyl('z', 96, 18, 25.9f, 2.6f, 0.4f, dark).bev(0.2f);

    // two floodlights on poles in the far corners
    for (int k = 0; k < 2; k++) {
        const float lx = k ? -4.0f : 110.0f, ly = k ? 110.0f : 40.0f, lh = (k ? 24.0f : 30.0f) / 0.7f;
        m.rod({ lx, ly, 0 }, { lx, ly, lh }, 0.7f, steel).m(METAL).bev(0.2f);
        m.box(lx, ly, lh + 1.0f, 2.4f, 1.2f, 1.0f, lamp).m(EMIT);
    }
    // a whip aerial with its warning light, south-east of the ramp
    m.rod({ 106, 92, 0 }, { 106, 92, 31.4f }, 0.35f, dark).f(F_NOSHADOW);
    m.ball(106, 92, 31.8f, 0.9f, 0.9f, 0.9f, cy ? cGlow : C(0xFF463C)).m(EMIT);
    // tank traps on the west ground
    for (float y : { 58.0f, 68.0f, 78.0f }) m.box(-3.5f, y + 3.0f, 2.85f, 2.5f, 3.0f, 2.85f, cy ? cWall : C(0xA49874)).m(MATTE).t(T_PANEL, 3.0f, cy ? cWall2 : C(0x7A7054)).bev(0.4f);
}

void cyberBunker(Model& m, int) { bunker(m, true); }
void clankerBunker(Model& m, int) { bunker(m, false); }

}  // namespace

M3D_BUILD(B_C_BUNKER, cyberBunker)
M3D_BUILD(B_K_BUNKER, clankerBunker)
