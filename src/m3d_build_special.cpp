// One Hour - 3D models: the special structures: naval yards, anti-nuke defences, underground silos, healing antennas, waymakers, walls and
// bastion walls. Cyber is pale ceramic over graphite with cobalt trim and cyan light; Clanker is olive, khaki, rust and hazard paint.
// Heights come from the old painters' lifts: z = px / 0.7. See m3d.h / m3d_reg.h.
#include "m3d_parts.h"
#include <cmath>

using namespace m3d;

namespace {

const Color sGraphite = C(0x3A4250), sBlack = C(0x15171C), sSilver = C(0xC9CED6), sSilverD = C(0x7D8590);

// the old painters lift a roof or a top by px screen pixels; a point at height z is drawn 0.7 * z pixels higher
float zh(float px) { return px / 0.7f; }
// a solid on the plan rectangle x..x+w, y..y+h, standing on z0 and rising by ht
Prim& blockAt(Model& m, float x, float y, float w, float h, float z0, float ht, Color c) { return m.slab(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, z0, ht, c); }
// a round post standing on the plan point (cx, cy) from z0 up to z1
Prim& post(Model& m, float cx, float cy, float z0, float z1, float r, Color c) { return m.cyl('z', cx, cy, (z0 + z1) * 0.5f, r, (z1 - z0) * 0.5f, c); }

// ---------------------------------------------------------------- naval yard (5 x 4 tiles, standing in the water)
// Quays and piers on piles over the sea, an open basin, a slipway ramp down into it and a gantry crane straddling the covered shed.
void yard(Model& m, bool cy) {
    using namespace pal;
    const float DZ = 8.0f;   // deck top: the quays stand this high over the water
    const Color deck = cy ? cPave : kPave, pile = cy ? sGraphite : kRust, dark = cy ? cDark : kDark;
    const Color shedW = cy ? cWall : kWall, shedR = cy ? cRoof : kRoof, steel = cy ? cSteel : kSteel;
    m.dirt = cy ? 0.1f : 0.3f;
    // quays and piers: decks on piles, so the sea shows between the piles and in the basin
    blockAt(m, 0, 0, 160, 14, DZ - 2, 2, deck).m(MATTE).t(T_PANEL, 10).bev(0.4f);          // north quay
    blockAt(m, 18, 112, 132, 16, DZ - 2, 2, deck).m(MATTE).t(T_PANEL, 10).bev(0.4f);       // south pier
    blockAt(m, 12, 28, 112, 60, DZ - 2, 2, deck).m(MATTE).t(T_PANEL, 10).bev(0.4f);        // shed platform
    blockAt(m, 128, 26, 28, 70, DZ - 2, 2, deck).m(MATTE).t(T_PANEL, 10).bev(0.4f);        // east platform
    for (float x = 8; x < 160; x += 18) post(m, x, 12.5f, 0, DZ - 2, 0.9f, pile).m(METAL);
    for (float x = 24; x < 150; x += 15) post(m, x, 125.5f, 0, DZ - 2, 0.9f, pile).m(METAL);
    for (float x = 20; x < 118; x += 16) post(m, x, 87.0f, 0, DZ - 2, 0.9f, pile).m(METAL);
    for (float x = 134; x < 152; x += 8) post(m, x, 95.5f, 0, DZ - 2, 0.9f, pile).m(METAL);
    // the shed: plate walls with a dark bay on the south face where the ramp leaves, a ribbed roof and the owner's band across it
    blockAt(m, 12, 30, 112, 56, DZ, zh(18) - 1.2f, shedW).t(T_PANEL, 12).bev(0.6f);
    blockAt(m, 12, 30, 112, 56, DZ + zh(18) - 1.2f, 1.2f, shedR).t(T_RIBS, 3.5f).bev(0.5f);
    m.box(64, 86.2f, DZ + 4.5f, 16, 0.6f, 4.5f, sBlack).bev(0.1f);
    m.box(68, 34.5f, DZ + zh(18) + 0.5f, 52, 1.4f, 0.5f, OWNER).bev(0.2f);
    m.box(104, 66, DZ + zh(18) + 0.5f, 10, 5, 0.5f, OWNER).bev(0.2f);
    // the control tower on the east platform: a glazed south face, a link dish (cyber) or a chimney (clanker)
    blockAt(m, 130, 32, 24, 26, DZ, zh(26) - 0.8f, cy ? cWall2 : kWall2).t(T_WINDOWS, 6, cy ? cWin : kWin).bev(0.5f);
    blockAt(m, 130, 32, 24, 26, DZ + zh(26) - 0.8f, 0.8f, shedR).bev(0.3f);
    m.box(142, 52.5f, DZ + zh(26) + 0.4f, 9, 2.4f, 0.4f, OWNER).bev(0.1f);
    if (cy) {
        post(m, 138, 37, DZ + zh(26), DZ + zh(26) + 9, 0.7f, cSteel);
        m.cyl('z', 138, 37, DZ + zh(26) + 9.5f, 4.4f, 0.8f, cRoof).bev(0.2f);
    } else {
        post(m, 144, 44, DZ + zh(26), DZ + zh(26) + 14, 2.6f, kRust).t(T_PANEL, 4);
        m.cyl('z', 144, 44, DZ + zh(26) + 14.4f, 2.8f, 0.5f, kDark).bev(0.2f);
    }
    // fuel tanks on the east platform, a pipe reaching back to the shed
    post(m, 138, 86, DZ, DZ + zh(14), 6.0f, steel).m(METAL);
    post(m, 152, 86, DZ, DZ + zh(14), 6.0f, steel).m(METAL);
    m.rod({ 124, 82, DZ + 3 }, { 134, 82, DZ + 3 }, 1.2f, steel).m(METAL);
    // the gantry crane: two tapered legs on the quays, a bridge over the shed, a trolley and a hook over the basin
    m.taper(88, 5, DZ, 5, 5, zh(22), 3.2f, 3.2f, steel).bev(0.3f);
    m.taper(88, 119, DZ, 5, 5, zh(22), 3.2f, 3.2f, steel).bev(0.3f);
    m.box(88, 63, 35, 4, 59, 2, steel).t(T_PANEL, 8).bev(0.3f);
    m.box(88, 90, 38.5f, 7, 4.5f, 1.5f, dark).bev(0.3f);
    m.rod({ 88, 90, 36.5f }, { 88, 90, 14 }, 0.5f, steel).m(METAL);
    m.ball(88, 90, 13, 1.6f, 1.6f, 1.6f, steel).m(METAL);
    // slipway: a concrete ramp from the bay down into the basin, with two rails
    m.profileX({ { 88, 0 }, { 88, DZ }, { 108, 0 } }, 56, 72, deck).m(MATTE).t(T_PANEL, 4).bev(0.2f);
    for (float x : { 59.0f, 69.0f }) m.rod({ x, 88.5f, DZ + 0.4f }, { x, 106, 1.0f }, 0.45f, steel).m(METAL);
    // bollards, containers on the pier, a lamp at each end of the yard
    for (float x : { 30.0f, 70.0f, 110.0f }) post(m, x, 4.5f, DZ, DZ + 3.0f, 0.9f, dark);
    for (float x : { 60.0f, 120.0f }) post(m, x, 123.5f, DZ, DZ + 3.0f, 0.9f, dark);
    for (float x : { 31.0f, 137.0f }) {
        m.box(x, 120, DZ + 3.5f, 9, 4, 3.5f, cy ? cTrim : kWall).t(T_PANEL, 3).bev(0.2f);
        m.box(x - 0.5f, 120, DZ + 10.5f, 8, 3.8f, 3.5f, cy ? cRoof : kRust).t(T_RIBS, 2).bev(0.2f);
    }
    if (!cy) m.box(70, 89, DZ + 0.25f, 40, 0.5f, 0.25f, kHaz).t(T_HAZARD, 2, kDark).bev(0.05f);
    post(m, 14, 30, DZ, DZ + 11, 0.45f, steel); m.ball(14, 30, DZ + 11.8f, 1.0f, 1.0f, 1.0f, cy ? cGlow : kGlow).m(EMIT);
    post(m, 146, 113.5f, DZ, DZ + 11, 0.45f, steel); m.ball(146, 113.5f, DZ + 11.8f, 1.0f, 1.0f, 1.0f, cy ? cGlow : kGlow).m(EMIT);
}
static void cNaval(Model& m, int) { yard(m, true); }
M3D_BUILD(B_C_NAVAL, cNaval)
static void kNaval(Model& m, int) { yard(m, false); }
M3D_BUILD(B_K_NAVAL, kNaval)

// ---------------------------------------------------------------- anti-nuke defences (2 x 2 tiles)
// a regular polygon in plan (for plinths and pads)
std::vector<P2> ngon(float cx, float cy, float r, int n) {
    std::vector<P2> v;
    for (int i = 0; i < n; i++) { const float t = i * 6.2831853f / n; v.push_back({ cx + r * std::cos(t), cy + r * std::sin(t) }); }
    return v;
}

// Cyber laser: a hex plinth with a cobalt trim ring, capacitor banks and a slim pylon whose focusing emitter points straight up.
void antiNukeLaser(Model& m) {
    using namespace pal;
    const float cx = 32, cy = 34, R = 24;
    m.dirt = 0.0f;
    m.slab(32, 32, 31, 31, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);                          // the pad
    m.plan(ngon(cx, cy, R, 6), 0.9f, 5, cWall2).bev(0.6f);                                          // the plinth skirt
    m.plan(ngon(cx, cy, R + 0.5f, 6), 5, 5.5f, cTrim);                                              // cobalt trim ring
    m.plan(ngon(cx, cy, R - 4.5f, 6), 5.5f, 7, cRoof).t(T_PANEL, 5).bev(0.4f);                      // armoured top deck
    // capacitor banks flanking the pylon, cyan lights on their faces
    blockAt(m, 14, 28, 9, 12, 7, zh(7), cRoof).bev(0.5f);
    blockAt(m, 41, 28, 9, 12, 7, zh(7), cRoof).bev(0.5f);
    blockAt(m, 26, 43, 12, 8, 7, zh(5), cRoof).bev(0.5f);
    m.box(18.5f, 34, 17.4f, 4.0f, 4.0f, 0.4f, OWNER).bev(0.1f);                                    // owner's colour on the bank tops
    m.box(45.5f, 34, 17.4f, 4.0f, 4.0f, 0.4f, OWNER).bev(0.1f);
    m.box(18.5f, 40.2f, 13.5f, 3.2f, 0.4f, 0.5f, cGlow).m(EMIT);
    m.box(45.5f, 40.2f, 13.5f, 3.2f, 0.4f, 0.5f, cGlow).m(EMIT);
    // the pylon with two cyan emitter rings, the drum on top and the focusing lens facing up
    post(m, cx, cy + 2, 7, 63.6f, 3.6f, cRoof).bev(0.3f);
    for (float z : { 30.0f, 40.0f }) m.cyl('z', cx, cy + 2, z, 4.4f, 0.5f, cGlow).m(EMIT);
    post(m, cx, cy + 2, 63.6f, 66.0f, 6.4f, OWNER).bev(0.4f);                                       // the drum wears the owner's colour
    m.cyl('z', cx, cy + 2, 66.4f, 5.0f, 0.4f, cGlow).m(EMIT);
    m.cyl('z', cx, cy + 2, 66.9f, 1.8f, 0.4f, sSilver).m(METAL);                                  // hot focus at the centre
}
static void cAntiNuke(Model& m, int) { antiNukeLaser(m); }
M3D_BUILD(B_C_ANTINUKE, cAntiNuke)

// Clanker interceptor silo: a reinforced concrete block with a dark hatch, four interceptors standing in it and a radar mast.
void antiNukeMissiles(Model& m) {
    using namespace pal;
    m.dirt = 0.35f;
    m.slab(32, 32, 31, 31, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 12).bev(0.4f);                          // the pad
    blockAt(m, 6, 16, 52, 40, 0.9f, zh(12) - 0.9f, kConc).m(MATTE).t(T_PANEL, 10).bev(0.6f);       // silo block: the launch slab
    m.box(32, 56.2f, 2.5f, 26, 0.5f, 1.2f, kHaz).t(T_HAZARD, 1.6f, kDark).bev(0.1f);                // hazard edge on the south face
    m.box(32, 32, zh(12) + 0.2f, 12, 8, 0.2f, kSteel).m(METAL).bev(0.2f);                            // steel rim of the hatch
    m.box(32, 32, zh(12) + 0.45f, 10, 6, 0.25f, sBlack).bev(0.1f);                                   // the dark well
    // four interceptors standing in the well, nose up
    const float mx[4] = { 26, 32, 38, 32 }, my[4] = { 31, 29, 32, 36 };
    for (int i = 0; i < 4; i++) {
        m.cyl('z', mx[i], my[i], 22.4f, 1.4f, 4.9f, sSilver).m(METAL).bev(0.2f);
        m.cyl('z', mx[i], my[i], 28.5f, 1.4f, 1.5f, kRust, 0.0f).bev(0.2f);
    }
    // radar mast on the north-east corner, a dish on top
    post(m, 55, 12, zh(12), zh(12) + zh(10), 0.45f, kSteel).m(METAL);
    m.ball(55, 12, zh(12) + zh(10) + 0.5f, 4.0f, 4.0f, 1.0f, kSteel).m(METAL);
    m.box(20, 38.6f, zh(12) + 0.3f, 12, 3.0f, 0.3f, OWNER).bev(0.1f);                                 // owner's team mark on the slab
    m.box(56, 47, 1.9f, 2.5f, 2.6f, 1.9f, kSteel).t(T_RIBS, 1.2f).bev(0.2f);
    m.box(4.5f, 50, 2.4f, 2.2f, 2.6f, 2.4f, kWall).t(T_PANEL, 2).bev(0.2f);
}
static void kAntiNuke(Model& m, int) { antiNukeMissiles(m); }
M3D_BUILD(B_K_ANTINUKE, kAntiNuke)

// ---------------------------------------------------------------- underground nuke silo (3 x 3 tiles)
// A low armoured hatch flush with the ground: a steel lip round a dark well, blast doors, corner vents and hazard markings.
// State 1: the doors slid back to the lip and a warhead stands in the well with its nose above the lip.
void silo(Model& m, bool cy, int state) {
    using namespace pal;
    const Color pad = cy ? cPave : kPave, lip = cy ? cWall2 : kConc, door = cy ? sSilver : kSteel;
    m.dirt = cy ? 0.1f : 0.35f;
    m.slab(48, 48, 47, 47, 0, 0.9f, pad).m(MATTE).t(T_PANEL, 16).bev(0.4f);                        // the pad
    m.box(48, 3.5f, 1.05f, 46, 1.5f, 0.15f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.05f);              // hazard markings round the pad
    m.box(48, 92.5f, 1.05f, 46, 1.5f, 0.15f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.05f);
    m.box(3.5f, 48, 1.05f, 1.5f, 45, 0.15f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.05f);
    m.box(92.5f, 48, 1.05f, 1.5f, 45, 0.15f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.05f);
    // the armoured lip round the sunk well
    m.box(48, 10.5f, 2.05f, 39, 1.5f, 1.15f, lip).t(T_PANEL, 6).bev(0.4f);
    m.box(48, 85.5f, 2.05f, 39, 1.5f, 1.15f, lip).t(T_PANEL, 6).bev(0.4f);
    m.box(10.5f, 48, 2.05f, 1.5f, 36, 1.15f, lip).t(T_PANEL, 6).bev(0.4f);
    m.box(85.5f, 48, 2.05f, 1.5f, 36, 1.15f, lip).t(T_PANEL, 6).bev(0.4f);
    m.box(48, 48, 1.0f, 36, 36, 0.1f, sBlack).bev(0.05f);                                            // the well
    // corner vents with the owner's colour on their caps, and the owner's band on the south lip
    for (float x : { 6.0f, 90.0f }) for (float y : { 6.0f, 90.0f }) {
        m.box(x, y, 2.3f, 2.6f, 2.6f, 1.4f, lip).t(T_GRILLE, 1.2f, kDark).bev(0.3f);
        m.box(x, y, 3.8f, 3.0f, 3.0f, 0.1f, OWNER).bev(0.05f);
    }
    m.box(48, 86.1f, 3.3f, 36, 1.2f, 0.1f, OWNER).bev(0.05f);
    if (state == 0) {
        // the closed blast doors: two leaves meeting at a seam, the locking wheel and a radiation disc on the west leaf
        m.box(31.5f, 48, 2.0f, 16.5f, 33, 1.1f, door).t(T_PANEL, 8).bev(0.4f);
        m.box(64.5f, 48, 2.0f, 16.5f, 33, 1.1f, door).t(T_PANEL, 8).bev(0.4f);
        m.box(48, 48, 2.1f, 0.5f, 33, 1.2f, sBlack).bev(0.1f);
        m.cyl('z', 48, 48, 3.5f, 5.5f, 0.6f, sSilverD).m(METAL).bev(0.3f);
        m.cyl('z', 31, 46, 3.2f, 3.0f, 0.1f, kHaz).bev(0.1f);
    } else {
        // the doors slid back to the lip and a warhead stands in the well: body, a band and a nose cone above the lip
        m.box(22.5f, 48, 2.0f, 7.5f, 33, 1.1f, door).t(T_PANEL, 8).bev(0.4f);
        m.box(73.5f, 48, 2.0f, 7.5f, 33, 1.1f, door).t(T_PANEL, 8).bev(0.4f);
        m.cyl('z', 48, 48, 7.0f, 5.6f, 6.0f, sSilver).m(METAL).bev(0.2f);
        m.cyl('z', 48, 48, 5.5f, 5.8f, 0.5f, sSilverD).m(METAL);
        m.cyl('z', 48, 48, 14.5f, 5.6f, 1.5f, sSilver, 0.0f).m(METAL);
    }
}
static void cUnuke(Model& m, int s) { silo(m, true, s); }
M3D_BUILD(B_C_UNUKE, cUnuke)
static void kUnuke(Model& m, int s) { silo(m, false, s); }
M3D_BUILD(B_K_UNUKE, kUnuke)

// ---------------------------------------------------------------- healing antenna (1 x 1 tile)
// Cyber: a hex plinth with a glowing heal panel on its south face, a slim mast with collars and an emitter dish on top.
void healCyber(Model& m) {
    using namespace pal;
    m.dirt = 0.0f;
    m.slab(16, 16, 16, 16, 0, 0.9f, cPave).m(MATTE).t(T_PANEL, 8).bev(0.4f);                        // the pad
    m.plan(ngon(16, 16, 10, 6), 0.9f, 4, cWall2).bev(0.5f);                                          // hex plinth skirt
    m.plan(ngon(16, 16, 9, 6), 4, 5, cRoof).bev(0.4f);                                               // lit top face
    m.plan(ngon(16, 16, 7.2f, 6), 5, 5.4f, cSteel).bev(0.2f);                                        // inset ring
    m.plan(ngon(16, 16, 10, 6), 4, 4.3f, OWNER);                                                     // owner's colour: a ring round the skirt top
    m.box(16, 24.9f, 2.6f, 3.5f, 0.4f, 1.2f, OWNER).bev(0.1f);
    // heal panel: dark glass with a glowing cross, on the south face
    m.box(16, 19.2f, 9.5f, 4.0f, 0.4f, 4.0f, cGlass).m(GLASS).bev(0.2f);
    m.box(16, 18.7f, 9.5f, 0.5f, 0.2f, 2.5f, cGlow).m(EMIT);
    m.box(16, 18.7f, 9.5f, 2.5f, 0.2f, 0.5f, cGlow).m(EMIT);
    // the mast, its collars, the emitter dish and a glowing lens, with two whips and their tip lights
    m.rod({ 16, 16, 5 }, { 16, 16, 32 }, 0.8f, cRoof).m(METAL);
    for (float z : { 14.0f, 20.0f }) m.cyl('z', 16, 16, z, 1.3f, 0.25f, cSteel).m(METAL);
    m.dome(16, 16, 32, 4.6f, 4.6f, 2.4f, cRoof).bev(0.3f);
    m.cyl('z', 16, 16, 34.6f, 1.8f, 0.3f, cGlow).m(EMIT);
    m.rod({ 16, 16, 32.5f }, { 10, 16, 35.5f }, 0.35f, cSteel).m(METAL);
    m.rod({ 16, 16, 32.5f }, { 22, 16, 35.5f }, 0.35f, cSteel).m(METAL);
    m.ball(10, 16, 35.5f, 0.6f, 0.6f, 0.6f, cGlow).m(EMIT);
    m.ball(22, 16, 35.5f, 0.6f, 0.6f, 0.6f, cGlow).m(EMIT);
}
static void cHeal(Model& m, int) { healCyber(m); }
M3D_BUILD(B_C_HEALANT, cHeal)

// Clanker: a riveted olive base plate with a battery box and a hazard label, a steel pole with guys and a cream dish with a red lamp.
void healClanker(Model& m) {
    using namespace pal;
    m.dirt = 0.4f;
    m.slab(16, 16, 16, 16, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 8).bev(0.4f);                        // the pad
    m.slab(16, 22, 13, 7, 0, 2.6f, kWall).t(T_PANEL, 4).bev(0.4f);                                   // the riveted base plate
    m.box(7.2f, 22, 4.4f, 2.4f, 3.4f, 1.8f, kDark).bev(0.3f);                                        // battery box
    m.box(7.2f, 22, 6.3f, 2.0f, 2.4f, 0.2f, kHaz).t(T_HAZARD, 1.2f, kDark).bev(0.05f);
    m.rod({ 16, 22, 2.6f }, { 16, 22, 24 }, 0.7f, kSteel).m(METAL);                                  // the pole
    m.rod({ 16, 22, 20 }, { 22, 22, 2.6f }, 0.25f, kDark).m(METAL);                                  // guys
    m.rod({ 16, 22, 20 }, { 10, 22, 2.6f }, 0.25f, kDark).m(METAL);
    m.dome(16, 22, 24, 4.6f, 4.6f, 2.0f, C(0xE2DCC8)).bev(0.3f);                                    // the dish, facing up
    m.ball(16, 22, 26.5f, 1.0f, 1.0f, 1.0f, C(0xCE342A)).m(EMIT);                                     // red lamp on top
    m.box(20, 17.5f, 2.85f, 4.0f, 2.0f, 0.25f, OWNER).bev(0.05f);                                    // owner's colour on the base
    m.box(16, 29.2f, 1.3f, 4.0f, 0.2f, 0.8f, kHaz).t(T_HAZARD, 1.0f, kDark).bev(0.05f);              // hazard label on the front
}
static void kHeal(Model& m, int) { healClanker(m); }
M3D_BUILD(B_K_HEALANT, kHeal)

// ---------------------------------------------------------------- waymaker (3 x 3 tiles: the patch it clears)
// Survey stakes at the corners with owner-coloured tape between them, and in the middle a small tracked rock grinder with a toothed drum
// and a beacon on its cab. It stands on the rock it clears, so there is no pad.
void waymaker(Model& m, bool cy) {
    using namespace pal;
    const Color body = cy ? C(0xCEC478) : C(0xE2B234), bodyD = cy ? C(0x84804E) : C(0x966F1E);
    const Color dark = C(0x1E1E20), steel = C(0x54565A), stake = C(0x96703E), orange = C(0xFF7828);
    m.dirt = cy ? 0.1f : 0.4f;
    // survey tape along the four sides, in the owner's colour and black
    m.box(48, 5, 6.0f, 43, 0.5f, 0.5f, OWNER).t(T_HAZARD, 2.2f, dark).bev(0.05f);
    m.box(48, 91, 6.0f, 43, 0.5f, 0.5f, kHaz).t(T_HAZARD, 2.2f, dark).bev(0.05f);
    m.box(5, 48, 6.0f, 0.5f, 43, 0.5f, kHaz).t(T_HAZARD, 2.2f, dark).bev(0.05f);
    m.box(91, 48, 6.0f, 0.5f, 43, 0.5f, OWNER).t(T_HAZARD, 2.2f, dark).bev(0.05f);
    // timber stakes with orange tops
    for (float x : { 5.0f, 91.0f }) for (float y : { 5.0f, 91.0f }) {
        post(m, x, y, 0, 8.6f, 0.9f, stake).m(PAINT).bev(0.1f);
        post(m, x, y, 8.6f, 10.6f, 1.2f, orange).bev(0.2f);
    }
    // the grinder, bow to the east: tracks, engine deck, cab with a glass front, drum arms, the toothed drum and a beacon
    const float cx = 44, cyy = 50;
    for (int s = -1; s <= 1; s += 2) m.box(cx - 3, cyy + s * 9.5f, 1.8f, 15, 3.2f, 1.8f, dark).m(RUBBER).t(T_TREAD, 2.2f).bev(0.4f);
    m.slab(cx - 3.5f, cyy, 12.5f, 7.5f, 3.6f, 4.0f, body).t(T_PANEL, 6).bev(0.5f);
    m.slab(cx - 10.5f, cyy, 4.5f, 6.5f, 7.6f, 1.0f, bodyD).t(T_RIBS, 2.0f).bev(0.3f);
    m.slab(cx, cyy, 4.0f, 5.0f, 7.6f, 5.0f, body).bev(0.5f);
    m.box(cx + 4.2f, cyy, 10.2f, 0.25f, 2.8f, 1.4f, C(0x3C7896)).m(GLASS).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) m.rod({ cx + 6, cyy + s * 7.0f, 6.0f }, { cx + 15, cyy + s * 7.0f, 9.0f }, 1.0f, steel).m(METAL);
    m.cyl('y', cx + 16, cyy, 5.0f, 4.5f, 10.0f, steel).m(METAL).t(T_RIBS, 1.5f).bev(0.3f);
    for (float y = -8; y <= 8; y += 4) m.box(cx + 20.8f, cyy + y, 5.0f, 0.6f, 0.7f, 0.7f, C(0xBEBEBA)).bev(0.1f);
    m.ball(cx - 1, cyy, 13.4f, 1.0f, 1.0f, 1.0f, C(0xFFA028)).m(EMIT);
}
static void cWay(Model& m, int) { waymaker(m, true); }
M3D_BUILD(B_C_WAYMAKER, cWay)
static void kWay(Model& m, int) { waymaker(m, false); }
M3D_BUILD(B_K_WAYMAKER, kWay)

// ---------------------------------------------------------------- walls (1 x 1 tile, edge to edge)
// One solid block that fills its tile, so neighbouring walls join without gaps: a plated cap, crenellations on both long sides,
// a base band and an owner-coloured stripe across the cap. Height is the old wall's (9 px lift).
void wall(Model& m, bool cy) {
    using namespace pal;
    const float H = zh(9);
    const Color face = cy ? cWall : kBrick, top = cy ? cRoof : kRoof, band = cy ? cTrim : kHaz;
    m.dirt = cy ? 0.0f : 0.3f;
    m.slab(16, 16, 16, 16, 0, H, face).t(cy ? T_PANEL : T_BRICK, 6, cy ? cWall2 : kConc).bev(0.6f);   // the wall block, edge to edge
    m.slab(16, 16, 16, 16, H, 0.8f, top).bev(0.3f);                                                    // the plated cap
    for (float k = 0; k < 4; k++) {                                                                    // crenellations on both long sides
        m.box(8 * k + 2, 1.8f, H + 1.7f, 2, 1.8f, 0.9f, face).bev(0.2f);
        m.box(8 * k + 2, 30.2f, H + 1.7f, 2, 1.8f, 0.9f, face).bev(0.2f);
    }
    m.box(16, 16, H + 0.95f, 16, 1.6f, 0.15f, OWNER).bev(0.05f);                                       // owner's stripe across the cap
    m.box(16, 31.9f, 1.3f, 16, 0.3f, 0.9f, band).t(cy ? T_PANEL : T_HAZARD, 2.2f, kDark).bev(0.05f);   // base band
    m.box(16, 31.8f, 7.0f, 6, 0.3f, 0.8f, sBlack).bev(0.05f);                                          // firing slit
    if (cy) {
        m.box(3, 31.9f, 4.5f, 0.5f, 0.3f, 0.5f, C(0x58ECFF)).m(EMIT);
        m.box(29, 31.9f, 4.5f, 0.5f, 0.3f, 0.5f, C(0x58ECFF)).m(EMIT);
    }
}
static void wallCyber(Model& m, int) { wall(m, true); }
M3D_BUILD(B_C_WALL, wallCyber)
static void wallClanker(Model& m, int) { wall(m, false); }
M3D_BUILD(B_K_WALL, wallClanker)

// ---------------------------------------------------------------- bastion walls (1 x 1 tile, edge to edge)
// Heavier and taller than a wall: a dark armoured block that fills its tile, a thick cap, a hex armour plate on top, an owner stripe
// across the cap and a light seam (Cyber) or hazard chevrons (Clanker) on the south face.
void bastion(Model& m, bool cy) {
    using namespace pal;
    const float H = zh(12);
    const Color face = cy ? C(0x242830) : C(0x2C2822), top = cy ? C(0x424854) : C(0x524E44), plate = cy ? C(0x5C6472) : C(0x68645A);
    m.dirt = cy ? 0.0f : 0.3f;
    m.slab(16, 16, 16, 16, 0, H, face).t(T_PANEL, 8, top).bev(1.0f);                                  // the heavy block, edge to edge
    m.slab(16, 16, 16, 16, H, 1.2f, top).t(T_PANEL, 6, face).bev(0.5f);                              // the thick armoured cap
    m.plan(ngon(16, 6, 5, 6), H + 1.2f, H + 1.8f, plate).bev(0.3f);                                  // hex armour plate on top
    m.box(16, 24, H + 1.35f, 13, 2.5f, 0.15f, OWNER).bev(0.05f);                                     // owner's stripe across the cap
    for (float x : { 2.5f, 10.5f, 21.5f, 29.5f }) m.box(x, 31.6f, H * 0.5f, 1.0f, 0.5f, H * 0.5f, top).bev(0.2f);   // buttress ribs on the face
    for (float y : { 1.0f, 30.0f }) m.box(16, y, H + 1.7f, 16, 1.0f, 0.5f, plate).bev(0.2f);                       // ridges along the cap
    for (float a = 0; a < 3; a++) m.ball(16 + std::cos(a * 2.094f) * 2.5f, 6 + std::sin(a * 2.094f) * 2.5f, H + 2.1f, 0.6f, 0.6f, 0.6f, top); // bolts on the plate
    if (cy) {
        m.box(16, 31.9f, 9.0f, 16, 0.2f, 0.25f, cGlow).m(EMIT);                                       // a seam of light along the face
    } else {
        m.box(16, 31.9f, 3.0f, 16, 0.3f, 1.2f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.05f);             // hazard chevrons along the face
    }
}
static void fwallCyber(Model& m, int) { bastion(m, true); }
M3D_BUILD(B_C_FWALL, fwallCyber)
static void fwallClanker(Model& m, int) { bastion(m, false); }
M3D_BUILD(B_K_FWALL, fwallClanker)

}  // namespace
