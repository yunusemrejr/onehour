// One Hour - 3D models: the prize landmarks (an oil derrick, a lighthouse, a field depot). Nobody's at the start, so they wear no army's look:
// weathered timber, whitewash, rust and tar. The owner's colour shows on a few bands and panels once somebody holds them.
#include "m3d_parts.h"

using namespace m3d;

namespace {
const Color pEarth = C(0x5E5444), pTar = C(0x2B2622), pTimber = C(0x7A5A3A), pTimberD = C(0x5A4129), pRustP = C(0x9A4E2C), pSteelP = C(0x7F8890),
            pWhite = C(0xE9E4D6), pStone = C(0x8C8676), pStoneD = C(0x6A6558), pSlate = C(0x3E4650), pLamp = C(0xFFE9A0), pTile = C(0xA5472E),
            pCanvas = C(0x8C8F5E), pCrate = C(0x9B7B4A), pDrum = C(0x3F6A52), pGlassP = C(0x16303C);
}

// 2 x 2 tiles: a timber-and-steel drilling tower over the well head, a walking-beam pump, a riveted storage tank and a tar-stained yard.
static void derrick(Model& m, int) {
    m.dirt = 0.5f;
    m.slab(32, 32, 31, 31, 0, 0.9f, pEarth).m(MATTE).t(T_NOISE, 7, pTar).bev(0.5f);
    m.cyl('z', 30, 34, 1.0f, 9.5f, 0.15f, pTar).m(MATTE).bev(0.1f);                             // the spill round the well
    // the tower: four legs leaning in to the crown, three rings of bracing
    const float cx = 30, cy = 30, b = 13, t = 3.2f, H = 74;
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2)
        m.rod({ cx + sx * b, cy + sy * b, 0.9f }, { cx + sx * t, cy + sy * t, H }, 1.25f, pTimber).m(MATTE).bev(0.2f);
    for (int k = 1; k <= 3; k++) {
        const float f = k / 4.0f, r = b + (t - b) * f, z = 0.9f + (H - 0.9f) * f;
        for (int s = -1; s <= 1; s += 2) {
            m.rod({ cx - r, cy + s * r, z }, { cx + r, cy + s * r, z }, 0.7f, pTimberD).m(MATTE).bev(0.1f);
            m.rod({ cx + s * r, cy - r, z }, { cx + s * r, cy + r, z }, 0.7f, pTimberD).m(MATTE).bev(0.1f);
        }
        const float r0 = b + (t - b) * (f - 0.25f), z0 = 0.9f + (H - 0.9f) * (f - 0.25f);
        m.rod({ cx - r0, cy + r0, z0 }, { cx + r, cy + r, z }, 0.5f, pSteelP).m(METAL).f(F_NOSHADOW);   // diagonals on the face we see
    }
    m.box(cx, cy, H + 1.6f, 4.6f, 4.6f, 1.6f, pSteelP).m(METAL).bev(0.5f);                       // crown block
    m.box(cx, cy, H + 4.2f, 5.2f, 5.2f, 0.9f, OWNER).bev(0.3f);                                  // the holder's cap on the crown
    m.rod({ cx, cy, H }, { cx, cy, 6 }, 0.55f, pSteelP).m(METAL).f(F_NOSHADOW);                  // drill string
    m.cyl('z', cx, cy, 3.4f, 3.0f, 2.6f, pRustP).m(METAL).bev(0.5f);                             // well head
    m.cyl('z', cx, cy, 6.6f, 3.8f, 0.5f, pSteelP).m(METAL).bev(0.2f);
    // the pump jack to the south-west
    m.slab(13, 50, 8, 5, 0.9f, 1.4f, pStoneD).m(MATTE).bev(0.4f);
    m.rod({ 13, 50, 2 }, { 13, 50, 14 }, 1.3f, pRustP).m(METAL);                                 // samson post
    m.rod({ 4, 50, 15.5f }, { 23, 50, 13 }, 1.2f, pSteelP).m(METAL).bev(0.2f);                   // walking beam
    m.box(23.5f, 50, 11.2f, 1.6f, 1.2f, 3.2f, pRustP).m(METAL).bev(0.5f);                        // horse head
    m.cyl('y', 6, 50, 6.5f, 4.0f, 1.3f, pTar).bev(0.3f);                                         // counterweight
    m.box(6, 50, 3.0f, 2.6f, 3.0f, 2.0f, pDrum).t(T_PANEL, 3).bev(0.4f);                         // engine
    // the tank to the east, a pipe run from the well
    m.cyl('z', 51, 18, 9.5f, 9.5f, 8.6f, pRustP).m(METAL).t(T_PANEL, 6, pTar).bev(0.6f);
    m.cyl('z', 51, 18, 18.4f, 9.9f, 0.5f, pSteelP).m(METAL).bev(0.2f);
    m.cyl('z', 51, 18, 12.5f, 9.75f, 1.6f, OWNER).bev(0.1f);                                     // the holder's band round the tank
    m.rod({ 33, 30, 3 }, { 42, 24, 3 }, 0.8f, pSteelP).m(METAL);
    m.rod({ 42, 24, 3 }, { 44, 20, 3 }, 0.8f, pSteelP).m(METAL);
    m.rod({ 58, 26, 1 }, { 58, 26, 19 }, 0.45f, pSteelP).m(METAL).f(F_NOSHADOW);                 // ladder rail
    // drums and a shed by the gate
    for (int k = 0; k < 3; k++) m.cyl('z', 44.0f + k * 5.2f, 52.0f + (k & 1) * 3.0f, 3.5f, 2.2f, 2.6f, k == 1 ? pRustP : pDrum).m(METAL).bev(0.3f);
    m.slab(52, 40, 6.5f, 4.5f, 0.9f, 7.0f, pTimber).m(MATTE).t(T_PLANKS, 2.2f, pTimberD).bev(0.3f);
    m.profileX({ { 34.5f, 7.6f }, { 40, 11.4f }, { 45.5f, 7.6f } }, 44.5f, 59.5f, pSlate).t(T_RIBS, 1.6f).bev(0.2f);
    m.box(52, 44.6f, 4.0f, 1.6f, 0.15f, 2.8f, pTar).bev(0);                                       // its door
    m.box(60.4f, 40, 6.0f, 0.25f, 1.2f, 0.9f, pLamp).m(EMIT);
}
M3D_BUILD(B_N_DERRICK, derrick)

// 2 x 2 tiles: a whitewashed tower on a stone plinth with the holder's bands, a gallery and a glass lamp room, and the keeper's cottage at its foot.
static void lighthouse(Model& m, int) {
    m.dirt = 0.1f;
    m.slab(32, 32, 31, 31, 0, 0.9f, pStone).m(MATTE).t(T_NOISE, 6, pStoneD).bev(0.6f);
    const float cx = 28, cy = 28;
    m.cyl('z', cx, cy, 4.0f, 15.0f, 3.2f, pStoneD).m(MATTE).t(T_BRICK, 4, pStone).bev(0.8f);     // plinth
    m.cyl('z', cx, cy, 8.0f, 12.6f, 0.9f, pStone).m(MATTE).bev(0.4f);
    // the shaft: three tapering drums, a band of the holder's colour between them
    m.cyl('z', cx, cy, 20.5f, 10.4f, 11.8f, pWhite, 0.86f).m(MATTE).bev(0.4f);
    m.cyl('z', cx, cy, 34.6f, 9.0f, 2.4f, OWNER, 0.97f).bev(0.2f);
    m.cyl('z', cx, cy, 48.0f, 8.7f, 11.2f, pWhite, 0.84f).m(MATTE).bev(0.3f);
    m.cyl('z', cx, cy, 61.4f, 7.4f, 2.3f, OWNER, 0.97f).bev(0.2f);
    m.cyl('z', cx, cy, 70.0f, 7.1f, 6.4f, pWhite, 0.9f).m(MATTE).bev(0.3f);
    for (int k = 0; k < 3; k++) m.box(cx, cy + 9.4f - k * 1.0f, 18.0f + k * 22.0f, 1.1f, 0.3f, 1.7f, pGlassP).m(GLASS).bev(0.1f);   // slit windows up the south face
    m.box(cx, cy + 10.5f, 10.6f, 1.8f, 0.3f, 2.8f, pTimberD).bev(0.1f);                           // door
    // gallery, lamp room, cap
    m.cyl('z', cx, cy, 77.2f, 10.2f, 0.8f, pSlate).bev(0.3f);
    for (int k = 0; k < 10; k++) { const float a = k * 0.6283f; m.rod({ cx + std::cos(a) * 9.6f, cy + std::sin(a) * 9.6f, 78 }, { cx + std::cos(a) * 9.6f, cy + std::sin(a) * 9.6f, 82 }, 0.3f, pSlate).f(F_NOSHADOW); }
    m.cyl('z', cx, cy, 82.2f, 9.8f, 0.3f, pSlate).bev(0.1f).f(F_NOSHADOW);
    m.cyl('z', cx, cy, 83.5f, 5.6f, 5.4f, pGlassP).m(GLASS).bev(0.2f);
    m.ball(cx, cy, 83.5f, 3.0f, 3.0f, 3.4f, pLamp).m(EMIT);
    m.cyl('z', cx, cy, 89.6f, 6.6f, 0.7f, pSlate).bev(0.2f);
    m.cyl('z', cx, cy, 93.4f, 6.2f, 3.2f, C(0xA8362A), 0.08f).bev(0.2f);                          // the red cap
    m.rod({ cx, cy, 96 }, { cx, cy, 103 }, 0.35f, pSlate).f(F_NOSHADOW);
    // the keeper's cottage, a woodpile, a rowing boat pulled up on the stones
    m.slab(49, 47, 9.5f, 6.5f, 0.9f, 8.5f, pWhite).m(MATTE).t(T_WINDOWS, 5.5f, pGlassP).bev(0.4f);
    m.profileX({ { 39.5f, 9.2f }, { 47, 15.2f }, { 54.5f, 9.2f } }, 38.5f, 59.5f, pTile).t(T_TILES, 2.2f, C(0x7E3320)).bev(0.3f);
    m.box(55, 44, 15.5f, 1.3f, 1.3f, 3.0f, pStoneD).m(MATTE).t(T_BRICK, 2, pStone).bev(0.2f);     // chimney
    m.box(46, 53.7f, 4.4f, 1.5f, 0.2f, 3.0f, OWNER).bev(0.1f);                                    // the door in the holder's colour
    m.box(12, 54, 2.2f, 5.0f, 2.2f, 1.3f, pTimber).m(MATTE).t(T_PLANKS, 1.2f, pTimberD).bev(0.4f);
    m.plan({ { 8, 14 }, { 19, 11.5f }, { 22, 14 }, { 19, 16.5f } }, 0.9f, 3.0f, C(0x2F5F78)).bev(0.6f).rz(-18);
}
M3D_BUILD(B_N_LIGHTHOUSE, lighthouse)

// 3 x 2 tiles: a brick workshop hall with a ribbed roof and a wide bay door, a lean-to canvas shelter with a vehicle on blocks, a hoist gantry over
// the yard, drums, crates and tyres.
static void depot(Model& m, int) {
    m.dirt = 0.45f;
    m.slab(48, 32, 47, 31, 0, 0.9f, pEarth).m(MATTE).t(T_PANEL, 16, pTar).bev(0.5f);
    // the hall (west)
    m.slab(27, 23, 22, 15, 0.9f, 15.0f, C(0x8F5A41)).m(MATTE).t(T_BRICK, 3.2f, C(0x6C4230)).bev(0.4f);
    m.profileX({ { 7, 15.4f }, { 14, 23.5f }, { 32, 23.5f }, { 39, 15.4f } }, 4, 50, pSlate).t(T_RIBS, 2.0f).bev(0.3f);
    m.box(27, 23, 23.9f, 14, 4.2f, 0.3f, OWNER).t(T_RIBS, 2.0f).bev(0.1f);                        // the holder's colour along the ridge
    m.box(20, 38.15f, 7.4f, 8.5f, 0.2f, 6.0f, pTar).t(T_RIBS, 1.2f).bev(0);                       // the roller door, half up
    m.box(20, 38.3f, 12.6f, 8.9f, 0.3f, 1.0f, pSteelP).m(METAL).bev(0.1f);
    m.box(39, 38.15f, 9.0f, 4.0f, 0.2f, 2.6f, pGlassP).m(GLASS).bev(0.1f);                        // office window
    m.box(49.3f, 23, 11.5f, 0.2f, 3.0f, 0.9f, pLamp).m(EMIT);
    m.cyl('z', 12, 14, 26.0f, 1.6f, 3.0f, pRustP).m(METAL).bev(0.3f);                             // flue
    // the canvas lean-to (east) with a truck chassis on blocks under it
    for (int k = 0; k < 2; k++) for (int j = 0; j < 2; j++) m.rod({ 58.0f + k * 30.0f, 12.0f + j * 20.0f, 0.9f }, { 58.0f + k * 30.0f, 12.0f + j * 20.0f, j ? 12.5f : 15.5f }, 0.8f, pTimberD).m(MATTE);
    m.profileY({ { 56, 14.0f }, { 56, 15.0f }, { 90, 15.0f }, { 90, 14.0f } }, 10.5f, 34, pCanvas).m(MATTE).t(T_STRIPE, 6, OWNER).bev(0.3f).rx(8);
    m.box(73, 23, 4.2f, 10.5f, 5.0f, 1.6f, C(0x4C5236)).t(T_PANEL, 5).bev(0.6f);                  // chassis
    m.box(80.5f, 23, 7.6f, 3.2f, 4.6f, 2.4f, C(0x5F6A3E)).bev(0.8f);                              // its cab
    for (int s = -1; s <= 1; s += 2) for (int k = 0; k < 2; k++) m.box(66.0f + k * 14.0f, 23 + s * 4.6f, 1.9f, 1.4f, 1.0f, 1.0f, pTimber).m(MATTE).bev(0.2f);   // blocks
    // the hoist gantry over the yard (south)
    m.rod({ 56, 46, 0.9f }, { 56, 46, 22 }, 1.1f, pRustP).m(METAL);
    m.rod({ 86, 46, 0.9f }, { 86, 46, 22 }, 1.1f, pRustP).m(METAL);
    m.box(71, 46, 22.6f, 16.5f, 1.1f, 1.0f, C(0xE6BA3C)).t(T_HAZARD, 3.0f, pTar).bev(0.2f);
    m.rod({ 68, 46, 22 }, { 68, 46, 10 }, 0.3f, pTar).f(F_NOSHADOW);
    m.box(68, 46, 8.6f, 2.6f, 2.0f, 1.8f, pSteelP).m(METAL).bev(0.5f);                            // an engine block on the hook
    // yard clutter
    m.slab(40, 52, 4.0f, 3.2f, 0.9f, 5.0f, pCrate).m(MATTE).t(T_PLANKS, 1.6f, pTimberD).bev(0.4f);
    m.slab(47, 54, 3.0f, 2.6f, 0.9f, 3.6f, pCrate).m(MATTE).t(T_PLANKS, 1.6f, pTimberD).bev(0.4f);
    m.slab(41, 52, 2.6f, 2.2f, 5.9f, 3.2f, OWNER).bev(0.3f);                                      // a crate under the holder's tarpaulin
    for (int k = 0; k < 4; k++) m.cyl('z', 8.0f + (k % 2) * 5.0f, 47.0f + (k / 2) * 5.0f, 3.5f, 2.2f, 2.6f, k == 2 ? pRustP : pDrum).m(METAL).bev(0.3f);
    for (int k = 0; k < 3; k++) m.cyl('z', 90, 56, 1.8f + k * 1.9f, 3.3f, 0.9f, pTar).m(RUBBER).bev(0.5f);   // tyres
    m.rod({ 24, 54, 0.9f }, { 24, 54, 9 }, 0.4f, pSteelP).m(METAL).f(F_NOSHADOW);
    m.box(24, 54, 9.6f, 2.2f, 0.3f, 1.3f, pWhite).bev(0.1f);                                      // the yard sign
}
M3D_BUILD(B_N_DEPOT, depot)
