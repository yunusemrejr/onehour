// One Hour - 3D models: the relic structures (an abandoned third army's bases, taken back by the jungle) and their turret heads.
// Dusty warm concrete and dark weathered bronze, verdigris stains, moss and creepers, dim red and teal lamps. See m3d.h / m3d_reg.h.
#include "m3d_parts.h"
#include <cmath>
#include <vector>

using namespace m3d;

namespace {

// palette
const Color rConc = C(0x9C927E), rConcD = C(0x6C6456), rConcL = C(0xBDB39B), rPad = C(0x857C68), rCam = C(0x7F7656);
const Color rBronze = C(0x6A5030), rBronzeL = C(0x8E6E42), rRust = C(0x8C4A27), rVerd = C(0x7C9A86), rVerdD = C(0x3F5F55);
const Color rIron = C(0x4A4944), rIronL = C(0x7C7A6E), rLeafD = C(0x1E3A22), rLeafM = C(0x2F4E26), rLeafL = C(0x4E7433);
const Color rGlass = C(0x2A3C38), rDark = C(0x1E1C19), rRed = C(0xD8402C), rTeal = C(0x35D6BE);

// the old painters lift a roof or a top by 'px' screen pixels; in the model a point at height z is drawn 0.7 * z pixels higher
float zh(float px) { return px / 0.7f; }
// a block on the ground: plan rectangle x..x+w, y..y+h, from the ground up to the old lift ht
Prim& blk(Model& m, float x, float y, float w, float h, float ht, Color c) { return m.slab(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, 0, zh(ht), c); }
// a vertical cylinder standing on the plan point (cx, cy), from z0 up to z1
Prim& vcyl(Model& m, float cx, float cy, float z0, float z1, float r, Color c) { return m.cyl('z', cx, cy, (z0 + z1) * 0.5f, r, (z1 - z0) * 0.5f, c); }
// a clump of creeper foliage centred at (x, y, z)
void leaves(Model& m, float x, float y, float z, float r) { m.ball(x, y, z, r, r, r * 0.8f, rLeafM).m(FOLIAGE).t(T_NOISE, 3, rLeafL); }
// the ground slab: cracked concrete plates with dark seams
void ground(Model& m, float W, float H) {
    m.dirt = 0.25f;
    m.slab(W / 2, H / 2, W / 2 - 1, H / 2 - 1, 0, 0.9f, rPad).m(MATTE).t(T_PANEL, 16, rConcD).bev(0.4f);
}
// a lattice mast standing at (x, y) from z0 to z1: two legs, three cross bars and a teal lamp on top
void mast(Model& m, float x, float y, float z0, float z1) {
    const float w = 3.0f;
    m.rod({ x - w, y, z0 }, { x - 0.8f, y, z1 }, 0.7f, rIronL).m(METAL);
    m.rod({ x + w, y, z0 }, { x + 0.8f, y, z1 }, 0.7f, rIronL).m(METAL);
    for (int k = 1; k <= 3; k++) {
        const float z = z0 + (z1 - z0) * k / 4.0f, hw = w - (w - 0.8f) * k / 4.0f;
        m.rod({ x - hw, y, z }, { x + hw, y, z }, 0.45f, rIron).m(METAL);
    }
    m.ball(x, y, z1 + 1.2f, 1.2f, 1.2f, 1.2f, rTeal).m(EMIT);
}

// ---------------------------------------------------------------- the structures

// 4 x 4 tiles: a domed command post. Two low wings flank a hall under a great concrete dome, with comms dishes and two lattice masts.
void rCommand(Model& m, int) {
    ground(m, 4 * BPX, 4 * BPX);
    // the wings: flat verdigris-stained roofs, a vent each, the owner's band on the east roof
    blk(m, 6, 92, 40, 30, 10, rConc).t(T_NOISE, 9, rVerd).bev(1.0f);
    blk(m, 82, 92, 40, 30, 10, rConc).t(T_NOISE, 9, rVerd).bev(1.0f);
    m.box(102, 107, zh(10) + 0.4f, 13, 9, 0.4f, OWNER).bev(0.2f);
    m.box(94, 122.4f, 9, 9, 0.5f, 5.5f, OWNER);
    vcyl(m, 30, 102, zh(10), zh(10) + 4, 2.5f, rIron).t(T_GRILLE, 1.2f);
    vcyl(m, 94, 102, zh(10), zh(10) + 4, 2.5f, rIron).t(T_GRILLE, 1.2f);
    leaves(m, 12, 100, zh(10) + 2.5f, 5.0f);
    leaves(m, 14, 116, zh(10) + 2.0f, 4.0f);
    leaves(m, 110, 118, zh(10) + 2.0f, 4.5f);
    // the hall: a long block under a great dome, its south wall broken into glazed bays
    blk(m, 18, 34, 92, 52, 20, rConc).t(T_NOISE, 10, rVerd).bev(1.2f);
    m.dome(64, 60, zh(20), 22, 22, 17, rConc).t(T_PANEL, 5, rConcD);
    vcyl(m, 64, 60, zh(20), zh(20) + 1.6f, 22.8f, rVerd).t(T_NOISE, 3, rVerdD);       // the verdigris band round the dome's foot
    vcyl(m, 64, 60, zh(20) + 15, zh(20) + 17, 3.5f, rIron).m(METAL);                   // the crown vent
    mast(m, 26, 50, zh(20), zh(20) + zh(42));
    mast(m, 104, 50, zh(20), zh(20) + zh(42));
    for (float x : { 28.0f, 100.0f }) {                                                  // comms dishes on the hall roof
        m.rod({ x, 76, zh(20) }, { x, 76, zh(20) + 5 }, 0.8f, rIron).m(METAL);
        m.ball(x, 76, zh(20) + 6.5f, 8, 8, 2.2f, rConcL);
    }
    leaves(m, 40, 44, zh(20) + 2.0f, 6.0f);
    leaves(m, 92, 40, zh(20) + 2.0f, 5.0f);
    // the hall's south face: a dark door under a bronze lintel, four glazed bays, rust streaks and a verdigris plate
    m.box(64, 86.3f, 10, 8, 0.4f, 10, rDark);
    m.box(64, 86.5f, 21.5f, 9.5f, 0.6f, 1.5f, rBronze).bev(0.3f);
    for (float x : { 26.0f, 42.0f, 84.0f, 98.0f }) m.box(x + 4.5f, 86.2f, 14.3f, 4.5f, 0.35f, 5.5f, rGlass).m(GLASS);
    m.box(79, 86.25f, 12.8f, 3, 0.3f, 4.2f, rVerd);
    for (float x : { 52.0f, 92.0f }) m.box(x, 86.25f, 14, 1.2f, 0.25f, 11, rRust).m(MATTE);
    for (float x : { 38.0f, 90.0f }) m.box(x, 86.6f, 13, 4, 0.6f, 12, rLeafD).m(FOLIAGE).t(T_NOISE, 3, rLeafL);   // creepers hanging from the wall
    m.box(22, 122.3f, 5, 9, 0.6f, 5, rLeafD).m(FOLIAGE).t(T_NOISE, 3, rLeafL);
    m.box(112, 122.3f, 5, 7, 0.6f, 4, rLeafD).m(FOLIAGE).t(T_NOISE, 3, rLeafL);
}
M3D_BUILD(B_R_COMMAND, rCommand)

// 3 x 3 tiles: a squat containment drum under a cracked dome, a radiator bank of cooling fins beside it, a red lamp at its foot.
void rReactor(Model& m, int) {
    ground(m, 3 * BPX, 3 * BPX);
    // the drum: its crown at the top of the taper, two bronze bands and a column of ribs on its south face
    m.cyl('z', 44, 68, zh(22) * 0.5f, 27, zh(22) * 0.5f, rConc, 0.92f).t(T_NOISE, 8, rVerd).bev(0.8f);
    vcyl(m, 44, 68, 5, 6.5f, 27.4f, OWNER).m(METAL);
    vcyl(m, 44, 68, 24, 25.5f, 25.6f, rBronze).m(METAL);
    m.dome(44, 68, zh(22), 24, 24, 9, rConc).t(T_PANEL, 4, rConcD);
    m.box(50, 62, 40, 3.5f, 0.35f, 0.5f, rDark).rz(-40);                                  // the crack across the crown
    vcyl(m, 44, 68, 39.5f, 41.5f, 4.5f, rIron).m(METAL);                                 // the crown hatch
    for (float x : { 24.0f, 30.0f, 36.0f, 44.0f, 52.0f, 58.0f, 64.0f }) {                // ribs down the drum face
        const float fy = 68 + std::sqrt(27.0f * 27.0f - (x - 44) * (x - 44));
        m.box(x, fy - 0.3f, zh(22) * 0.5f, 0.7f, 1.0f, zh(22) * 0.5f, rConcD);
    }
    m.box(44, 93.5f, 2.2f, 5, 0.5f, 1.2f, rDark);                                        // vent slot at the foot
    m.box(44, 94.8f, 12, 1.3f, 0.4f, 0.9f, rRed).m(EMIT);                                // the red lamp
    // the radiator bank: corrugated fins, a pipe over its roof to the drum
    blk(m, 74, 52, 20, 38, 10, rConc).t(T_RIBS, 3, rConcD).bev(0.8f);
    for (float x : { 78.0f, 84.0f, 90.0f }) m.box(x, 90.5f, zh(10) * 0.5f, 0.5f, 0.6f, zh(10) * 0.5f, rConcD);
    m.box(83, 76, zh(10) + 0.3f, 7, 6, 0.3f, OWNER);                              // the owner's plate on the radiator roof
    m.rod({ 72, 54, zh(10) + 1.5f }, { 90, 50, zh(10) + 1.6f }, 1.3f, rVerd).m(METAL);
    m.rod({ 66, 70, 4 }, { 74, 66, 9 }, 1.0f, rBronze).m(METAL);
    leaves(m, 90, 56, zh(10) + 2.8f, 4.5f);
    leaves(m, 14, 90, 3.5f, 4.0f);
    leaves(m, 80, 14, 2.5f, 3.0f);
}
M3D_BUILD(B_R_REACTOR, rReactor)

// 5 x 4 tiles: a vehicle hall with a gantry crane on its roof, three big doors on the south wall and creepers in the gutters.
void rFoundry(Model& m, int) {
    ground(m, 5 * BPX, 4 * BPX);
    blk(m, 6, 20, 148, 84, 26, rConc).t(T_NOISE, 12, rVerd).bev(1.2f);
    m.box(80, 62, zh(26) + 0.3f, 74, 41, 0.3f, rBronze).t(T_RIBS, 3, rBronzeL).bev(0.2f);   // the corrugated roof
    for (float x : { 30.0f, 72.0f, 118.0f }) m.box(x, 62, zh(26) + 0.9f, 9, 14, 0.4f, rGlass).m(GLASS);   // skylights
    vcyl(m, 40, 86, zh(26) + 0.6f, zh(26) + 9, 4.5f, rVerd).t(T_NOISE, 2, rVerdD);
    vcyl(m, 112, 92, zh(26) + 0.6f, zh(26) + 9, 4.5f, rVerd).t(T_NOISE, 2, rVerdD);
    leaves(m, 96, 96, zh(26) + 7, 8.0f);                                                  // a tree grown out of the roof
    leaves(m, 150, 30, zh(26) + 5, 7.0f);
    // the gantry crane: two rails on the roof, portal legs standing on them, a beam across the hall, a trolley and a cab
    m.box(80, 60.5f, zh(26) + 1.0f, 66, 1.5f, 0.4f, rIron).m(METAL);
    m.box(80, 86.5f, zh(26) + 1.0f, 66, 1.5f, 0.4f, rIron).m(METAL);
    for (float x : { 24.0f, 132.0f }) m.box(x, 60.5f, (zh(26) + 1.4f + 116) * 0.5f, 2.5f, 2.5f, (116 - zh(26) - 1.4f) * 0.5f, rIron).m(METAL);
    m.box(79.5f, 61, 114, 58.5f, 3, 2.5f, rBronzeL).m(METAL).bev(0.3f);
    m.box(77, 61, 108, 7, 4, 3.2f, rVerdD).m(METAL);
    m.rod({ 77, 61, 104 }, { 77, 61, 78 }, 0.5f, rDark).m(METAL);
    m.ball(77, 61, 75, 3, 3, 3, rIronL).m(METAL);
    m.box(123, 62, 101, 4.5f, 4, 3.5f, rBronze).bev(0.4f);
    m.ball(123, 57.5f, 100, 1, 1, 1, rRed).m(EMIT);
    // three shutter doors on the south wall, each lifted a little so a dark gap shows under it
    for (float x : { 20.0f, 65.0f, 110.0f }) {
        m.box(x + 15, 104.3f, zh(20) * 0.5f, 15, 0.4f, zh(20) * 0.5f, rDark);
        m.box(x + 15, 104.6f, zh(20) + 0.5f, 16, 0.6f, 1.1f, rVerdD).bev(0.2f);
        m.box(x - 0.5f, 104.6f, zh(20) * 0.5f, 1.2f, 0.6f, zh(20) * 0.5f, rVerdD);
        m.box(x + 30.5f, 104.6f, zh(20) * 0.5f, 1.2f, 0.6f, zh(20) * 0.5f, rVerdD);
        m.box(x + 15, 104.6f, 17.9f, 14.4f, 0.8f, 10.7f, rConcD).t(T_RIBS, 3, rConcL);
    }
    m.box(146, 104.45f, 13, 4.5f, 0.4f, 8.5f, OWNER);
    m.box(124, 30, zh(26) + 0.7f, 24, 11, 0.3f, OWNER);                                        // the owner's panel on the wall
    m.box(147, 104.3f, 30, 3, 0.4f, 3, rVerd);
    m.box(57.5f, 104.3f, 22.9f, 3.5f, 0.35f, 5.8f, rGlass).m(GLASS);                      // broken windows
    m.box(102.5f, 104.3f, 22.9f, 3.5f, 0.35f, 5.8f, rGlass).m(GLASS);
    for (float x : { 30.0f, 130.0f }) m.box(x, 104.25f, 17, 1.2f, 0.25f, 11.5f, rRust).m(MATTE);
    m.box(36, 104.5f, 33, 15, 0.6f, 4, rLeafD).m(FOLIAGE).t(T_NOISE, 3, rLeafL);            // creepers over the doors
    m.box(124, 104.5f, 33, 15, 0.6f, 4, rLeafD).m(FOLIAGE).t(T_NOISE, 3, rLeafL);
    leaves(m, 14, 110, 4, 6.0f);
    leaves(m, 150, 118, 3.5f, 5.0f);
}
M3D_BUILD(B_R_FOUNDRY, rFoundry)

// 5 x 4 tiles: a VTOL hangar with a barrel roof and a rolled-up door, a round gunship pad to the south-east and a control tower with a dish.
void rAirstrip(Model& m, int) {
    ground(m, 5 * BPX, 4 * BPX);
    // the round pad: cracked concrete with a faded H, moss in its rim
    vcyl(m, 124, 94, 0, 1.2f, 33, rConcL).t(T_PANEL, 6, rConcD);
    m.box(116, 94, 1.35f, 1.0f, 8, 0.15f, rConcD);
    m.box(132, 94, 1.35f, 1.0f, 8, 0.15f, rConcD);
    m.box(124, 94, 1.35f, 8, 0.9f, 0.15f, rConcD);
    leaves(m, 96, 116, 2.6f, 4.5f);
    leaves(m, 150, 70, 2.4f, 4.0f);
    // the hangar: a box of walls under a ribbed barrel roof, a rolled door, a VTOL inside the dark bay
    blk(m, 6, 10, 88, 58, 24, rConc).t(T_NOISE, 10, rVerd).bev(1.2f);
    m.profileX(std::vector<P2>{ { 10, 34.3f }, { 16, 38.5f }, { 26, 41.5f }, { 39, 43 }, { 52, 41.5f }, { 62, 38.5f }, { 68, 34.3f } }, 6, 94, rConc)
        .t(T_RIBS, 4, rConcD).bev(0.4f);
    m.box(50, 24, 40.4f, 44, 4, 0.9f, OWNER);
    m.box(85, 68.4f, 20, 9, 0.4f, 6, OWNER);                                           // the owner's stripe along the arch
    m.box(48, 68.3f, 12.9f, 26, 0.4f, 12.9f, rDark);                                       // the dark bay
    m.cyl('x', 48, 66.2f, 31, 2.4f, 26, rConcL).m(METAL);                                  // the rolled-up door
    m.box(50, 60, 13, 16, 3.4f, 3.4f, rCam).bev(0.8f);                                     // the VTOL: fuselage
    m.box(64, 56, 20, 4, 0.6f, 6, rCam);                                                   // its tail fin
    m.ball(16, 67.6f, 30, 1, 1, 1, rRed).m(EMIT);
    m.ball(80, 67.6f, 30, 1, 1, 1, rRed).m(EMIT);
    leaves(m, 10, 66, 2.8f, 4.0f);
    leaves(m, 84, 14, 40, 5.0f);
    // the control tower to the west: a radar dish on its roof, cracked windows and a windsock on a pole
    blk(m, 8, 84, 26, 24, 14, rConc).t(T_NOISE, 8, rVerd).bev(0.8f);
    m.box(17, 103.5f, zh(14) + 0.3f, 8, 4, 0.3f, OWNER);
    m.rod({ 21, 94, zh(14) }, { 21, 94, zh(14) + 3.5f }, 0.8f, rIron).m(METAL);
    m.ball(21, 94, zh(14) + 5.5f, 7, 7, 2.2f, rConcL);
    m.box(16.5f, 108.3f, 8, 3.5f, 0.35f, 5, rGlass).m(GLASS);
    m.box(28.5f, 108.3f, 8, 3.5f, 0.35f, 5, rGlass).m(GLASS);
    leaves(m, 35, 100, 6, 4.5f);
    leaves(m, 30, 72, 8, 8.0f);                                                            // a bush in the open ground by the tower
    m.rod({ 44, 121, 0 }, { 44, 121, 50 }, 0.9f, rIronL).m(METAL);                          // the windsock pole
    m.box(51, 121, 47, 6, 0.5f, 3, C(0xD06A2A));
    leaves(m, 16, 120, 3, 4);
}
M3D_BUILD(B_R_AIRSTRIP, rAirstrip)

// 2 x 2 tiles: a flak battery. A round armoured plinth on a cracked pad, a verdigris turret ring, plates and ammunition by it.
// The live launcher head is a prop (head.r_flak); its base sits on the plinth top at z 17.1.
void rFlak(Model& m, int) {
    m.dirt = 0.25f;
    std::vector<P2> oct;
    for (int i = 0; i < 8; i++) oct.push_back({ 32 + 30 * std::cos(i * 0.7854f + 0.3927f), 32 + 30 * std::sin(i * 0.7854f + 0.3927f) });
    m.plan(oct, 0, 1.0f, rConc).m(MATTE).t(T_PANEL, 12, rConcD);
    vcyl(m, 32, 32, 1.0f, zh(12), 22, rConc).t(T_NOISE, 6, rVerd).bev(0.8f);   // the plinth: its top at z 17.1
    vcyl(m, 32, 32, zh(12), zh(12) + 1.2f, 16.5f, rVerd).m(METAL);             // the turret ring
    for (int i = 0; i < 4; i++) {
        const float a = i * 1.5708f;
        m.ball(32 + 19.5f * std::cos(a), 32 + 19.5f * std::sin(a), zh(12) + 1.2f, 0.9f, 0.9f, 0.9f, rVerd).m(METAL);   // bolts
    }
    m.box(32, 54.0f, 9, 8, 0.3f, 4, OWNER);                                          // the verdigris plate
    m.box(32, 54.3f, 13, 1.2f, 0.4f, 0.8f, rRed).m(EMIT);                                  // the red lamp
    for (int i = 0; i < 4; i++) {                                                          // armour plates round the plinth
        const float a = i * 1.5708f + 0.785f;
        m.box(32 + 22 * std::cos(a), 32 + 22 * std::sin(a), 6, 3.5f * std::fabs(std::cos(a)) + 1.5f, 3.5f * std::fabs(std::sin(a)) + 1.5f, 5, rBronze).bev(0.3f);
    }
    m.box(9, 8.5f, 4, 4.5f, 4, 4, rCam).bev(0.4f);                                        // ammunition crates
    m.box(53, 53, 3.5f, 4, 4, 3.5f, rCam).bev(0.4f);
    vcyl(m, 56, 14, 0, 4, 2.2f, rBronze).m(METAL);                                         // ammunition cans
    vcyl(m, 6, 54, 0, 4, 2.2f, rBronze).m(METAL);
    m.rod({ 4, 46, 1.2f }, { 18, 56, 1.2f }, 0.9f, C(0x4E3B2A)).m(MATTE);                  // a root across the pad
    leaves(m, 56, 10, 3, 3.5f);
    leaves(m, 8, 56, 3, 3.5f);
    m.rod({ 52, 30, 14 }, { 60, 32, 2.5f }, 0.8f, rIron).m(METAL);                         // a feed cable down to the pad
}
M3D_BUILD(B_R_FLAK, rFlak)

// 2 x 2 tiles: a laser tower. A pad, four cable pylons at its corners with lamps, cables sagging to a short pedestal.
// The emitter head is a prop (head.r_laser); its base sits on the pedestal top at z 17.1.
void rLaser(Model& m, int) {
    ground(m, 2 * BPX, 2 * BPX);
    const float px[4] = { 7, 57, 7, 57 }, py[4] = { 15, 15, 61, 61 };
    for (int i = 0; i < 4; i++) {
        vcyl(m, px[i], py[i], 0, zh(12), 3.4f, rConc).t(T_NOISE, 4, rVerd).bev(0.4f);
        m.ball(px[i], py[i], zh(12) + 0.8f, 1.2f, 1.2f, 1.2f, i < 2 ? rRed : rTeal).m(EMIT);
        m.rod({ px[i], py[i], zh(12) - 2 }, { 32, 32, zh(12) - 3 }, 0.4f, rDark);       // cables to the centre, under the head
    }
    m.rod({ 7, 15, 15 }, { 57, 15, 15 }, 0.5f, rDark);
    m.rod({ 7, 61, 15 }, { 57, 61, 15 }, 0.5f, rDark);
    m.rod({ 7, 15, 15 }, { 7, 61, 15 }, 0.5f, rDark);
    m.rod({ 57, 15, 15 }, { 57, 61, 15 }, 0.5f, rDark);
    vcyl(m, 32, 32, 0, zh(12), 13, rConc).t(T_NOISE, 6, rVerd).bev(0.6f);      // the pedestal: its top at z 17.1
    vcyl(m, 32, 32, zh(12), zh(12) + 1.2f, 10, rVerd).m(METAL);                // its verdigris turntable ring
    m.box(32, 45.1f, 9, 10, 0.3f, 5, OWNER);                             // the plate on its front
    m.box(32, 18.9f, 9, 8, 0.3f, 4, OWNER);                              // and on its back
    for (int i = 0; i < 6; i++) {
        const float a = i * 1.0472f;
        m.ball(32 + 11 * std::cos(a), 32 + 11 * std::sin(a), zh(12) + 1.4f, 0.7f, 0.7f, 0.7f, rVerdD).m(METAL);
    }
    leaves(m, 56, 20, 3, 4);
    leaves(m, 12, 60, 2.5f, 3);
    m.box(56, 56, 1.3f, 3.5f, 3, 1.2f, rConcD).bev(0.3f);                      // a fallen slab in the grass
}
M3D_BUILD(B_R_LASER, rLaser)

// 3 x 3 tiles: a heavy cannon emplacement. A round platform with an armoured ring of bolted plates round a turntable, a small ammunition
// bunker on the west rim and moss on the edge. The gun is a prop (head.r_cannon); its base sits on the pedestal top at z 14.3.
void rCannon(Model& m, int) {
    ground(m, 3 * BPX, 3 * BPX);
    vcyl(m, 48, 48, 0, 1.2f, 45, rConc).t(T_PANEL, 10, rConcD);               // the round platform
    for (int i = 0; i < 4; i++) {
        const float a = i * 1.5708f + 0.4f;
        leaves(m, 48 + 41 * std::cos(a), 48 + 41 * std::sin(a), 3.4f, 3.6f);
    }
    vcyl(m, 48, 48, 1.2f, 5.7f, 30, rIron).m(METAL).t(T_NOISE, 4, rVerd);   // the armoured ring
    for (int i = 0; i < 8; i++) {
        const float a = i * 0.7854f;
        m.ball(48 + 27.8f * std::cos(a), 48 + 27.8f * std::sin(a), 5.9f, 1.1f, 1.1f, 1.1f, rVerd).m(METAL);   // its bolts
    }
    m.box(48, 17.6f, 3.4f, 3.4f, 0.9f, 2.0f, rVerd);                            // plates on the four sides of the ring
    m.box(48, 78.4f, 3.4f, 3.4f, 0.9f, 2.0f, rVerd);
    m.box(17.6f, 48, 3.4f, 0.9f, 3.4f, 2.0f, rVerd);
    m.box(78.4f, 48, 3.4f, 0.9f, 3.4f, 2.0f, rVerd);
    vcyl(m, 48, 48, 5.7f, zh(10), 22, rConcL).t(T_NOISE, 6, rVerd).bev(0.6f);   // the pedestal: its top at z 14.3
    vcyl(m, 48, 48, zh(10), zh(10) + 0.7f, 20, rIronL).m(METAL);               // the turntable ring
    m.box(18, 70, zh(8) * 0.5f, 8, 8, zh(8) * 0.5f, rConc).t(T_NOISE, 6, rVerd).bev(0.8f);   // the ammunition bunker, west rim
    m.box(18, 78.25f, 5, 3, 0.25f, 3, rRust).m(MATTE);
    m.box(77, 72.5f, 2.5f, 5, 4.5f, 2.5f, rBronze).bev(0.3f);                   // a crate
    m.box(48, 85, 1.5f, 7, 5, 0.3f, OWNER);
    m.box(84, 48, 1.5f, 4, 6, 0.3f, OWNER);
    m.box(48, 13, 1.5f, 6, 3, 0.3f, OWNER);                                       // the owner's disc on the platform
    leaves(m, 80, 40, 5, 4.5f);
    leaves(m, 66, 80, 3.5f, 3.5f);
}
M3D_BUILD(B_R_CANNON, rCannon)

// the turret heads are props: each pivots about its own origin, points +x and stands at the pedestal top height of its base

// the flak launcher: a box of eight missile tubes, two per row in four rows, on a turntable
void pFlak(Model& m, int) {
    m.ground = false;
    vcyl(m, 0, 0, 17.1f, 19.2f, 8.5f, rConcD).t(T_NOISE, 4, rVerd);
    m.box(0, 0, 25.4f, 6, 6.5f, 6.2f, rCam).bev(0.6f);
    for (int r = 0; r < 4; r++) for (int s = -1; s <= 1; s += 2) m.rod({ 5, s * 3.2f, 21.2f + r * 3.2f }, { 17, s * 3.2f, 21.2f + r * 3.2f }, 1.1f, rIron).m(METAL);
    m.box(-2, 0, 32.5f, 2.5f, 3.2f, 0.8f, OWNER);
}
M3D_PROP("head.r_flak", pFlak)

// the laser emitter: a prism lens on a verdigris yoke over a turntable, its beam glowing teal
void pLaser(Model& m, int) {
    m.ground = false;
    vcyl(m, 0, 0, 19.2f, 19.8f, 3.0f, OWNER);
    vcyl(m, 0, 0, 17.1f, 19.2f, 7, rConcD).t(T_NOISE, 4, rVerd);
    m.rod({ 10, -10, 31 }, { 10, 10, 31 }, 1.4f, rVerd).m(METAL);
    m.rod({ 0, -1.5f, 22 }, { 10, -10, 31 }, 1.2f, rVerd).m(METAL);
    m.rod({ 0, 1.5f, 22 }, { 10, 10, 31 }, 1.2f, rVerd).m(METAL);
    m.box(12.5f, 0, 31, 2.6f, 3.5f, 3.5f, rGlass).m(GLASS).bev(0.4f);
    m.ball(10, 0, 31, 1.6f, 1.6f, 1.6f, rTeal).m(EMIT);
    m.rod({ 14.5f, 0, 31 }, { 22, 0, 31 }, 0.8f, rTeal).m(EMIT);
}
M3D_PROP("head.r_laser", pLaser)

// the heavy cannon: a long barrel with a muzzle brake on a cradle, a recoil cylinder above it, on a turntable
void pCannon(Model& m, int) {
    m.ground = false;
    vcyl(m, 0, 0, 14.3f, 15.8f, 8, rConc).t(T_NOISE, 4, rVerd);
    m.box(-1, 0, 18, 4.5f, 5, 3.7f, rCam).bev(0.5f);
    m.box(-1, 0, 22, 2.5f, 3, 0.3f, OWNER);
    m.rod({ -2, 0, 19 }, { 30, 0, 19 }, 2.2f, rIron).m(METAL);
    m.box(31.2f, 0, 19, 1.2f, 3.2f, 3.2f, rDark).m(METAL);
    m.rod({ -2, 0, 25 }, { 20, 0, 25 }, 1.4f, rConcD).m(METAL);
}
M3D_PROP("head.r_cannon", pCannon)

// the turret heads are props: each pivots about its own origin and points +x. (The structures and props above live in the anonymous
// namespace opened at the top of the file; they are registered with the macros.)

// 3 x 3 tiles: a missile silo. A raised collar round the shaft with a two-leaf hatch: shut in state 0; in state 1 the leaves are slid back,
// the shaft is open and the warhead's nose stands in it. A maintenance tower stands to the north-east, a hut to the west.
void rSilo(Model& m, int state) {
    ground(m, 3 * BPX, 3 * BPX);
    // the maintenance tower: two legs to a platform with a ladder, cross braces and a teal lamp
    m.rod({ 68, 22, 0 }, { 68, 22, 46 }, 1.1f, rIronL).m(METAL);
    m.rod({ 84, 22, 0 }, { 84, 22, 46 }, 1.1f, rIronL).m(METAL);
    m.box(76, 22, 46, 9, 3, 0.6f, rIron).m(METAL);
    m.rod({ 68, 22, 10 }, { 84, 22, 30 }, 0.5f, rIron).m(METAL);
    m.rod({ 84, 22, 10 }, { 68, 22, 30 }, 0.5f, rIron).m(METAL);
    for (int i = 0; i < 4; i++) m.box(76, 22, 12 + i * 9.0f, 8, 0.4f, 0.4f, rIronL).m(METAL);
    m.ball(76, 22, 48.5f, 1, 1, 1, rTeal).m(EMIT);
    // the hut west of the shaft: a block with a camouflage net over its roof, the owner's band and a window
    blk(m, 8, 64, 18, 22, 10, rConc).t(T_NOISE, 6, rVerd).bev(0.8f);
    m.box(17, 79, zh(10) + 0.5f, 8, 4.5f, 0.5f, C(0x7F7656)).m(MATTE);
    m.box(16, 69.5f, zh(10) + 0.3f, 9, 5, 0.3f, OWNER);
    m.box(15, 86.3f, 6.5f, 3, 0.35f, 3.5f, rGlass).m(GLASS);
    leaves(m, 14, 66, zh(10) + 1.5f, 3.5f);
    // the collar: a raised drum round the shaft with a band of faded ochre round its rim
    vcyl(m, 48, 56, 0, zh(8), 34, rConc).t(T_NOISE, 8, rVerd).bev(0.6f);
    vcyl(m, 48, 56, zh(8) - 3.2f, zh(8), 34.6f, OWNER).m(METAL);
    m.box(48, 56, zh(8) + 0.25f, 23, 23, 0.25f, rVerdD);                    // the hatch frame, flush with the collar top
    m.box(48, 32.25f, 12.2f, 24, 1.2f, 0.5f, rIron).m(METAL);               // the rails of the hatch
    m.box(48, 71.75f, 12.2f, 24, 1.2f, 0.5f, rIron).m(METAL);
    if (state == 0) {
        m.box(38, 56, 12.2f, 10, 20, 0.8f, rIron).m(METAL).t(T_PANEL, 4, rDark);    // the two leaves, shut
        m.box(58, 56, 12.2f, 10, 20, 0.8f, rIron).m(METAL).t(T_PANEL, 4, rDark);
    } else {
        m.box(24, 56, 12.2f, 4, 16, 0.8f, rIron).m(METAL).t(T_PANEL, 4, rDark);     // the leaves, slid back to the sides
        m.box(72, 56, 12.2f, 4, 16, 0.8f, rIron).m(METAL).t(T_PANEL, 4, rDark);
        vcyl(m, 48, 56, 11.4f, 12.1f, 20, rDark);                                   // the open shaft
        vcyl(m, 48, 56, 0, 20, 6, rIronL).m(METAL);                                 // the warhead body
        vcyl(m, 48, 56, 14.5f, 16, 6.4f, rBronze).m(METAL);                         // a band round it
        m.cyl('z', 48, 56, 29.3f, 6, 9.3f, rIronL, 0.05f).m(METAL);                 // the nose cone
    }
    m.box(74.5f, 78, 4, 4.5f, 4, 4, rBronze).bev(0.3f);                              // a crate by the collar
    leaves(m, 20, 90, 4, 4.5f);
    leaves(m, 88, 90, 3.5f, 4);
    leaves(m, 16, 30, 4, 4.5f);
}
M3D_BUILD(B_R_SILO, rSilo)

// 1 x 1 tile: a thick wall segment, edge to edge, with a coping, a rust bleed, a few vines hanging down the south face and moss on top.
void rWall(Model& m, int) {
    blk(m, 0, 0, 32, 32, 10, rConc).t(T_NOISE, 6, rVerd).bev(0.6f);
    m.box(16, 16, zh(10) + 0.4f, 14.5f, 14.5f, 0.4f, rConcL).bev(0.3f);          // the coping
    m.box(26, 31.95f, 6.5f, 1.2f, 0.2f, 5.5f, rRust).m(MATTE);
    m.box(16, 31.95f, 7, 5, 0.2f, 4.5f, OWNER);
    m.box(9, 31.95f, 6, 1, 0.2f, 5, rLeafD).m(FOLIAGE).t(T_NOISE, 2, rLeafL);
    m.box(21, 31.95f, 5, 0.9f, 0.2f, 4, rLeafD).m(FOLIAGE).t(T_NOISE, 2, rLeafL);
    leaves(m, 6, 26, zh(10) + 1.0f, 2.4f);
    leaves(m, 24, 8, zh(10) + 0.6f, 2.0f);
}
M3D_BUILD(B_R_WALL, rWall)

// 3 x 3 tiles: a secret bunker. A concrete dome sunk into a mound of earth, its south face pierced by gun slits and an owner's hatch,
// camouflage netting over the west flank, a canopy of leaves over the dome and a radio mast through it.
void rBunker(Model& m, int) {
    const Color earth = C(0x6E5D40);
    m.dirt = 0.3f;
    m.slab(48, 48, 47, 47, 0, 0.9f, earth).m(MATTE).t(T_NOISE, 5, rLeafM);
    m.dome(48, 58, 0, 42, 36, 9, earth).m(MATTE).t(T_NOISE, 6, rLeafM);           // the earth heaped over the dome
    vcyl(m, 48, 60, 0, 8, 30, rConcD).t(T_PANEL, 6, rConc);                       // the drum
    m.dome(48, 60, 8, 28, 28, 6.3f, rPad).t(T_PANEL, 5, rConcD);                // its cap, crown at z 14.3
    for (float x : { 27.0f, 43.0f, 57.0f }) {                                     // gun slits on the south face
        const float fy = 60 + std::sqrt(30.0f * 30.0f - (x - 48) * (x - 48));
        m.box(x, fy - 0.3f, 11, 3, 0.4f, 1.2f, rDark);
    }
    m.box(67, 83.5f, 6, 4.2f, 0.4f, 4.6f, rVerdD);                                 // the steel hatch, its owner's plate
    m.box(67, 83.9f, 6, 9, 0.4f, 7.5f, OWNER);
    m.box(24, 78.3f, 6, 5, 0.4f, 5, OWNER);
    vcyl(m, 48, 60, 6.5f, 7.8f, 30.5f, OWNER);                                      // the owner's band round the drum
    m.rod({ 70, 50, 7 }, { 70, 50, 42 }, 0.6f, rIronL).m(METAL);                   // the radio mast
    m.ball(70, 50, 43.5f, 1, 1, 1, rRed).m(EMIT);
    m.box(24, 52, 6.5f, 10, 10, 0.4f, C(0x7F7656)).m(MATTE).t(T_NOISE, 3, C(0x5C5640));   // camouflage netting
    vcyl(m, 40, 72, 8, 11, 1.4f, rIron).m(METAL);                                  // air vents on the mound
    vcyl(m, 60, 74, 8, 10.5f, 1.4f, rIron).m(METAL);
    m.ball(30, 84, 1.5f, 2, 2, 1.5f, rConcD);                                      // rubble at the foot
    m.ball(84, 84, 1.5f, 2.5f, 2, 1.5f, rConcD);
    m.ball(8, 48, 1.5f, 2, 2, 1.5f, rConcD);
    leaves(m, 40, 38, 13, 9);                                                      // the canopy over the crown
    leaves(m, 64, 34, 12, 8);
    leaves(m, 50, 20, 9, 7);
    leaves(m, 16, 62, 8, 7);
    leaves(m, 84, 52, 7, 6);
    leaves(m, 10, 38, 4, 5);
    leaves(m, 86, 30, 4, 4);
    leaves(m, 14, 74, 3, 4);
    leaves(m, 84, 78, 3, 4);
}
M3D_BUILD(B_R_BUNKER, rBunker)

}  // namespace
