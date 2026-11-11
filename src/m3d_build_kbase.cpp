// One Hour - 3D models: the Clanker base structures (see m3d.h / m3d_reg.h). Olive plate, corrugated khaki roofs, brick, rust,
// gunmetal, hazard paint, sandbags and small amber lights. Every structure keeps the plan layout of its old 2D painter.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// a few Clanker colours the shared palette does not carry
const Color kSand = C(0xC6B084), kSand2 = C(0xA78F63), kWood = C(0x9A7A4C), kTarpaulin = C(0x5A6A46), kRoofD = C(0x6E6852);
const Color kBrickL = C(0x9A6248), kSteelL = C(0x8A8C88), kSoot = C(0x33322D), kPlank = C(0x7A6040);

// a line of sandbags lying along (x0,y0)-(x1,y1): low matte lumps, staggered from course to course
static void sandLine(Model& m, float x0, float y0, float x1, float y1, float z0, int rows) {
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    const int n = std::max(2, (int)(len / 5.6f));
    for (int r = 0; r < rows; r++)
        for (int i = 0; i < n; i++) {
            const float k = (i + 0.5f + 0.35f * (r & 1)) / n;
            m.ball(x0 + dx * k, y0 + dy * k, z0 + 1.6f + r * 2.5f, 3.1f, 2.3f, 1.85f, (i + 2 * r) % 3 ? kSand : kSand2).m(MATTE);
        }
}
// a low sandbag parapet: a matte berm with a scalloped crest, much cheaper than filling it with bags
static void sandWall(Model& m, float x0, float y0, float x1, float y1, float z0, float h) {
    const float dx = x1 - x0, dy = y1 - y0, len = std::sqrt(dx * dx + dy * dy);
    if (len < 1.0f) return;
    const float ang = std::atan2(dy, dx) * 57.29578f;
    m.box((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, z0 + h * 0.5f, len * 0.5f, 1.9f, h * 0.5f, kSand).m(MATTE).t(T_NOISE, 3.2f, kSand2).bev(0.6f).rz(ang);
    const int n = std::max(2, (int)(len / 11.0f));
    for (int i = 0; i < n; i++) {
        const float k = (i + 0.5f) / n;
        m.ball(x0 + dx * k, y0 + dy * k, z0 + h + 0.7f, 3.4f, 2.2f, 1.7f, i % 2 ? kSand : kSand2).m(MATTE);
    }
}
// a tarred drum
static void drum(Model& m, float cx, float cy, float z0, float r, Color c) {
    m.cyl('z', cx, cy, z0 + r * 1.1f, r, r * 1.1f, c).t(T_BAND, 0.7f, kSoot).bev(0.35f);
    m.cyl('z', cx, cy, z0 + r * 2.2f, r * 0.4f, 0.2f, kSoot).bev(0.1f);
}
// a crate or pallet stack
static void crate(Model& m, float cx, float cy, float z0, float hx, float hy, float h, Color c) {
    m.box(cx, cy, z0 + h * 0.5f, hx, hy, h * 0.5f, c).m(MATTE).t(T_PLANKS, 2.6f).bev(0.45f);
    if (h > 2.5f) m.box(cx, cy, z0 + h - 0.2f, hx * 0.88f, hy * 0.88f, 0.2f, kPlank).m(MATTE).bev(0.1f);
}
// a radio mast with two spreaders and a beacon
static void mast(Model& m, float x, float y, float z0, float h, Color tip) {
    m.rod({ x, y, z0 }, { x, y, z0 + h }, 0.55f, kSteelL).m(METAL).bev(0).f(F_NOSHADOW);
    for (int i = 1; i <= 2; i++) m.rod({ x - 1.8f, y, z0 + h * i * 0.33f }, { x + 1.8f, y, z0 + h * i * 0.33f }, 0.3f, kSteelL).f(F_NOSHADOW);
    m.ball(x, y, z0 + h + 0.4f, 1.0f, 1.0f, 1.0f, tip).m(EMIT);
}
// a riveted chimney with a sooty rim
static void stack(Model& m, float x, float y, float z0, float r, float h, Color c) {
    m.cyl('z', x, y, z0 + h * 0.5f, r, h * 0.5f, c, 0.88f).m(METAL).t(T_PANEL, 7).bev(0.4f);
    m.cyl('z', x, y, z0 + h + 0.35f, r * 0.96f, 0.55f, kSoot).bev(0.3f);
    m.cyl('z', x, y, z0 + h * 0.55f, r * 1.06f, 0.35f, kSoot).bev(0.1f);
}
// a tarp thrown over a heap of stores, with two ropes over it
static void tarpHeap(Model& m, float cx, float cy, float z0, float rx, float ry, float rz) {
    m.dome(cx, cy, z0, rx, ry, rz, kTarpaulin).m(MATTE).t(T_NOISE, 3.4f, C(0x6E7C54));
    for (int s = -1; s <= 1; s += 2) m.rod({ cx - rx * 0.55f, cy + s * ry * 0.45f, z0 + rz * 0.75f }, { cx + rx * 0.55f, cy + s * ry * 0.45f, z0 + rz * 0.75f }, 0.3f, C(0x3E4630)).f(F_NOSHADOW);
}

// ---------------------------------------------------------------- Clanker Command Post, 4 x 4 tiles
// Old painter: hqClanker. A khaki-roofed command bunker in the middle, three fuel tanks across the back, a radio shack with the radar
// turntable, a generator shed, a sandbagged perimeter and the gate open on the south.
static void kHQ(Model& m, int) {
    using namespace pal;
    const float W = 4 * BPX, H = 4 * BPX;
    m.dirt = 0.4f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);

    // ---- fuel tanks across the back, each on a pair of concrete saddles
    for (int i = 0; i < 3; i++) {
        const float x = 34 + i * 30.0f, y = 22, h = 8 / 0.7f;
        m.cyl('z', x, y, 0.9f + h * 0.5f, 8, h * 0.5f, kRust, 0.94f).t(T_PANEL, 8).bev(0.5f);
        m.cyl('z', x, y, 0.9f + h + 0.25f, 7.4f, 0.25f, kSteel).m(METAL).bev(0.2f);
        m.cyl('z', x, y, 0.9f + h * 0.5f, 8.3f, 0.5f, kDark).bev(0.2f);                       // belly band
        m.box(x, y + 9.2f, 1.9f + h * 0.5f, 0.35f, 0.3f, h * 0.42f, kHaz).t(T_HAZARD, 1.8f, kDark).bev(0.1f);
        for (int s = -1; s <= 1; s += 2) m.box(x + s * 6.5f, y, 1.6f, 1.4f, 4.6f, 1.2f, kConc).m(MATTE);
        m.rod({ x + 9.4f, y - 2.0f, 0.9f }, { x + 9.4f, y - 2.0f, 0.9f + h * 0.8f }, 0.4f, kSteel).m(METAL).f(F_NOSHADOW);   // ladder
    }
    // ---- radio shack: a windowed hut with a ribbed gable, the radar turntable on top
    m.slab(20, 58, 12, 14, 0.9f, 14.3f, kHull2).t(T_WINDOWS, 6.5f, kWin).bev(0.6f);
    m.profileX({ { 44, 15.2f }, { 44, 16.4f }, { 58, 20.6f }, { 72, 16.4f }, { 72, 15.2f } }, 8, 32, kRoof).t(T_RIBS, 2.6f).bev(0.4f);
    m.cyl('z', 25, 51, 21.9f, 7.2f, 1.3f, kSteel).m(METAL).bev(0.4f);                         // radar turntable
    m.cyl('z', 25, 51, 23.4f, 3.2f, 0.4f, kDark).bev(0.2f);
    m.slab(20, 71.4f, 5, 0.5f, 8.0f, 3.4f, kGlow).m(EMIT);                                     // lit window over the door
    m.box(20, 72.2f, 4.6f, 3.2f, 0.6f, 4.4f, kDark).t(T_PLANKS, 1.6f).bev(0.2f);               // its door, south wall
    // ---- generator shed with a gable roof and a louvred fan
    m.slab(108, 64, 12, 16, 0.9f, 14.3f, kHull).t(T_PANEL, 6).bev(0.6f);
    m.profileX({ { 48, 15.2f }, { 48, 16.4f }, { 64, 22.0f }, { 80, 16.4f }, { 80, 15.2f } }, 96, 120, kRoofD).t(T_RIBS, 2.6f).bev(0.4f);
    m.cyl('z', 108, 56, 16.4f, 4.2f, 0.5f, kDark).t(T_GRILLE, 1.5f).bev(0.2f).rx(-90);          // roof fan well
    m.box(108, 80.4f, 5.0f, 6.5f, 0.5f, 3.2f, kDark).t(T_GRILLE, 1.4f).bev(0.1f);               // intake louvres, south wall
    m.box(104, 80.6f, 12.6f, 5, 0.4f, 1.0f, OWNER).bev(0.1f);                                   // owner's band on the shed
    m.rod({ 112, 48, 9 }, { 112, 32, 9 }, 1.4f, kSteel).m(METAL).t(T_PANEL, 4);                 // exhaust run to the north
    m.cyl('z', 112, 32, 9, 1.6f, 0.7f, kSoot).bev(0.2f);
    // ---- the command bunker: olive plate below, lit window band over it, a khaki ribbed roof
    m.slab(64, 64, 30, 28, 0.9f, 13.0f, kHull2).t(T_PANEL, 9).bev(0.7f);
    m.slab(64, 64, 29.4f, 27.4f, 13.9f, 9.0f, kHull).t(T_WINDOWS, 7.2f, kWin).bev(0.6f);
    m.slab(64, 64, 30.6f, 28.6f, 22.9f, 0.9f, kRoof).t(T_PANEL, 12).bev(0.5f);                  // roof slab
    m.slab(64, 64, 26, 24, 23.8f, 0.6f, kRoofD).t(T_RIBS, 3.6f).bev(0.3f);                      // ribbed sheeting inside the parapet
    m.box(74, 78, 24.6f, 16, 5.5f, 0.25f, OWNER).bev(0.15f);                                     // the owner's colour on the roof
    m.box(46, 34, 24.6f, 12, 5.0f, 0.25f, OWNER).bev(0.15f);
    m.box(104, 60, 22.4f, 7.0f, 4.0f, 0.3f, OWNER).bev(0.15f).ry(14);                           // owner's sheet on the east gable
    // roof furniture: hatch, vent stack, sandbagged corners
    m.box(46, 46, 24.5f, 6, 4.5f, 0.6f, kSteel).t(T_PANEL, 3).bev(0.3f);
    m.box(46, 46, 25.3f, 4.4f, 3.2f, 0.35f, kHull2).bev(0.2f);
    stack(m, 82, 48, 24.0f, 5.0f, 9.0f, kRust);
    m.cyl('z', 40, 66, 24.6f, 4.4f, 0.6f, kDark).t(T_GRILLE, 1.6f).bev(0.2f);                  // deck vent
    for (int i = 0; i < 4; i++) m.ball(37 + (i & 1) * 6.5f, 79 + (i >> 1) * 5.0f, 25.0f, 3.0f, 2.2f, 1.7f, i % 2 ? kSand : kSand2).m(MATTE);
    // south face: firing slits, the blast door, a hazard bar and a banner in the owner's colour
    for (int i = 0; i < 4; i++) m.box(44 + i * 13.0f, 92.2f, 19.0f, 4.0f, 0.35f, 1.1f, kDark).bev(0.1f);
    m.box(64, 92.4f, 9.4f, 7.0f, 0.5f, 9.0f, kDark).t(T_PLANKS, 2.2f).bev(0.3f);               // the blast door
    m.box(64, 92.8f, 17.6f, 8.4f, 0.45f, 0.9f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);
    m.box(64, 93.0f, 13.0f, 6.0f, 0.35f, 1.1f, OWNER).bev(0.1f);                                // door banner
    m.box(50, 92.6f, 20.4f, 2.0f, 0.4f, 0.7f, kGlow).m(EMIT);                                   // lamp over the door
    // a plank lean-to on the west of the gate, stores under it
    m.box(51, 89, 11.2f, 10, 3.4f, 0.4f, kPlank).m(MATTE).t(T_PLANKS, 1.8f).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) m.rod({ 51 + s * 8.0f, 91.6f, 0.9f }, { 51 + s * 8.0f, 91.6f, 11.2f }, 0.5f, kWood).m(MATTE);
    for (int i = 0; i < 3; i++) drum(m, 40 + (i & 1) * 5.0f, 84 + (i >> 1) * 5.0f, 0.9f, 2.7f, i == 1 ? kRust : kHull2);
    for (int i = 0; i < 2; i++) crate(m, 46 + i * 8.0f, 86, 0.9f, 4.0f, 3.4f, 4.5f, kWood);
    // ---- the mast on the west pad, braced to the shack roof
    mast(m, 14, 40, 0.9f, 46, C(0xFF5A48));
    for (int s = -1; s <= 1; s += 2) m.rod({ 14, 40, 40 }, { 14 + s * 7.0f, 40 + 12.0f, 16 }, 0.25f, kDark).f(F_NOSHADOW);
    m.cyl('z', 14, 24, 0.9f + 3.0f, 2.6f, 3.0f, kConc).m(MATTE);                                // its footing
    // ---- sandbag perimeter, the gate left open on the south
    sandWall(m, 4, 124, 44, 124, 0.9f, 3.4f);
    sandWall(m, 84, 124, 124, 124, 0.9f, 3.4f);
    sandWall(m, 4, 12, 4, 120, 0.9f, 3.4f);
    sandWall(m, 124, 12, 124, 120, 0.9f, 3.4f);
}
M3D_BUILD(B_K_HQ, kHQ)

// ---------------------------------------------------------------- Clanker Supply Depot, 3 x 3 tiles
// Old painter: supplyClanker. A long corrugated warehouse with two roll-up doors, a yard of crate stacks, a tarped heap under a
// small derrick and sandbags along the south edge.
static void kSupply(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX;
    m.dirt = 0.4f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the warehouse: corrugated walls, a ribbed roof with a ridge, rusted sheets
    m.slab(48, 29, 40, 21, 0.9f, 20.0f, kHull2).t(T_PANEL, 9).bev(0.7f);
    m.slab(48, 29, 40.4f, 21.4f, 20.9f, 0.7f, kRoof).t(T_RIBS, 3.0f).bev(0.4f);
    m.box(48, 29, 21.7f, 40.2f, 0.9f, 0.4f, kRoofD).t(T_RIBS, 2.0f).bev(0.2f);                   // ridge over the sheeting
    m.box(74, 15, 22.1f, 14, 6.0f, 0.3f, OWNER).bev(0.15f);                                         // owner's colour on the roof
    m.box(16, 18, 21.1f, 6, 4.0f, 1.0f, kSteel).t(T_GRILLE, 1.5f).bev(0.3f).rx(-14);              // roof louvre
    stack(m, 84, 20, 21.0f, 2.6f, 7.0f, kRust);
    // loading doors on the south wall: one wide, one man-wide, with slat lines and a hazard bar
    m.box(28, 50.5f, 9.9f, 12, 0.55f, 9.0f, kDark).t(T_RIBS, 2.0f).bev(0.2f);
    m.box(28, 51.0f, 19.4f, 13, 0.5f, 0.8f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.1f);
    m.box(64, 50.5f, 8.2f, 8, 0.55f, 7.3f, kDark).t(T_PLANKS, 1.8f).bev(0.2f);
    m.box(64, 50.9f, 16.4f, 3.0f, 0.4f, 0.9f, kGlow).m(EMIT);
    m.box(38, 50.8f, 2.2f, 4, 0.4f, 0.5f, kHaz).t(T_HAZARD, 1.6f, kDark).bev(0.1f);              // kerb marks
    // the yard: two rows of crate stacks
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 4; i++)
            crate(m, 14.5f + i * 11.0f, 68.5f + j * 11.0f, 0.9f, 4.5f, 4.5f, 4.6f + 0.5f * ((i + j) % 3), C(0x9E7C4A));
    for (int i = 0; i < 2; i++) crate(m, 14.5f + i * 11.0f, 68.5f, 5.6f, 3.6f, 3.6f, 3.4f, kWood);   // a second tier
    // the tarpaulin heap and its little derrick
    tarpHeap(m, 70, 78, 0.9f, 15, 9, 6.0f);
    m.box(56, 64, 3.1f, 3.0f, 3.0f, 2.2f, kSteel).m(METAL).t(T_PANEL, 2).bev(0.3f);
    m.rod({ 57.5f, 65.5f, 5.2f }, { 57.5f, 65.5f, 15.0f }, 1.0f, kSteel).m(METAL);
    m.rod({ 57.5f, 65.5f, 15.0f }, { 86, 65.5f, 14.4f }, 0.9f, kSteel).m(METAL);
    m.rod({ 86, 65.5f, 14.4f }, { 86, 65.5f, 8.4f }, 0.25f, kDark).f(F_NOSHADOW);
    m.box(86, 65.5f, 7.6f, 1.0f, 1.0f, 0.9f, kRust).m(METAL).bev(0.2f);
    // sandbags along the south edge, drums and a stack of boards by the doors
    sandLine(m, 8, 91, 40, 91, 0.9f, 2);
    for (int i = 0; i < 3; i++) drum(m, 78 + (i & 1) * 6.0f, 88 + (i >> 1) * 5.0f, 0.9f, 2.6f, i == 1 ? kRust : kHull2);
    for (int i = 0; i < 3; i++) m.box(70 + i * 3.5f, 60, 2.6f, 1.6f, 5.0f, 1.7f, kPlank).m(MATTE).t(T_PLANKS, 1.8f).bev(0.2f);
}
M3D_BUILD(B_K_SUPPLY, kSupply)

// ---------------------------------------------------------------- Clanker Barracks, 3 x 2 tiles
// Old painter: barracksClanker. A long hut under a pitched khaki roof with a brick chimney, a lit window row, sandbagged doorways
// and a training yard with crates, barrels and a flag.
static void kBarracks(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 2 * BPX;
    m.dirt = 0.45f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the hut: plate walls with a lit window row, a pitched roof with the ridge down the middle
    m.slab(48, 26, 42, 18, 0.9f, 12.4f, kHull2).t(T_WINDOWS, 6.4f, kWin).bev(0.6f);
    m.profileX({ { 8, 13.3f }, { 8, 14.0f }, { 26, 19.4f }, { 44, 14.0f }, { 44, 13.3f } }, 6, 90, kRoof).t(T_RIBS, 3.0f).bev(0.4f);
    m.box(48, 26, 19.9f, 42, 1.0f, 0.35f, kRoofD).t(T_RIBS, 2.2f).bev(0.2f);                     // ridge cap
    m.box(70, 26, 20.0f, 11, 1.3f, 0.45f, OWNER).bev(0.15f);                                      // owner's colour along the ridge
    m.profileX({ { 32.1f, 18.2f }, { 41.2f, 15.5f }, { 40.9f, 14.85f }, { 31.9f, 17.55f } }, 60, 80, OWNER).bev(0.1f);   // and a painted panel on the south slope
    // brick chimney through the north slope, a stove pipe beside it
    stack(m, 18, 16, 16.0f, 3.2f, 10.0f, kBrickL);
    m.rod({ 30, 14, 17.2f }, { 30, 14, 24.0f }, 1.2f, kSoot).m(METAL);
    m.cyl('z', 30, 14, 24.2f, 1.7f, 0.4f, kDark).bev(0.2f);
    // south wall: the door with its hazard bar, a lamp and a poster board
    m.box(48, 44.5f, 6.6f, 6.0f, 0.55f, 6.0f, kDark).t(T_PLANKS, 1.8f).bev(0.25f);
    m.box(48, 45.0f, 12.9f, 7.2f, 0.5f, 0.8f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.1f);
    m.box(48, 45.2f, 4.2f, 4.6f, 0.4f, 2.2f, OWNER).bev(0.1f);                                    // the banner over the door
    m.box(34, 44.8f, 9.4f, 1.7f, 0.4f, 0.7f, kGlow).m(EMIT);
    m.box(74, 44.5f, 8.0f, 5.0f, 0.35f, 3.0f, kPlank).m(MATTE).t(T_PLANKS, 1.6f).bev(0.15f);       // notice board
    // sandbagged bays either side of the door
    sandLine(m, 30, 52, 41, 52, 0.9f, 2);
    sandLine(m, 55, 52, 66, 52, 0.9f, 2);
    // the yard: crates, barrels, a flagpole
    for (int i = 0; i < 3; i++) crate(m, 11 + i * 9.0f, 52, 0.9f, 3.5f, 3.5f, 4.4f, C(0x9E7C4A));
    for (int i = 0; i < 2; i++) drum(m, 84 - i * 6.5f, 55, 0.9f, 2.8f, i ? kHull2 : kRust);
    m.cyl('z', 80, 40, 0.9f + 9.0f, 1.4f, 9.0f, kConc).m(MATTE);                                   // flagpole footing
    mast(m, 80, 40, 10.0f, 30, kGlow);
    m.box(84.5f, 40, 33.2f, 6.0f, 0.35f, 4.2f, OWNER).bev(0.1f);                                    // the pennant
    // a lean-to rack of stores against the west gable
    m.box(8, 30, 3.4f, 3.4f, 8.0f, 2.6f, kWood).m(MATTE).t(T_PLANKS, 1.8f).bev(0.2f);
}
M3D_BUILD(B_K_BARRACKS, kBarracks)

// ---------------------------------------------------------------- Clanker War Factory, 4 x 3 tiles
// Old painter: factoryClanker. A long assembly shed under a sawtooth roof, two brick stacks, the vehicle bay door in the middle of
// the south wall, a gantry crane over the forecourt and a sandbagged apron corner.
static void kFactory(Model& m, int) {
    using namespace pal;
    const float W = 4 * BPX, H = 3 * BPX;
    m.dirt = 0.5f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the hall: plate walls, a lit window row over the bay, a sawtooth roof of six teeth (the old plan: x 8..120, y 8..56)
    m.slab(64, 32, 56, 24, 0.9f, 27.7f, kHull2).t(T_PANEL, 10).bev(0.8f);
    m.box(64, 56.4f, 24.6f, 54, 0.5f, 3.4f, kWin).m(GLASS).bev(0.15f);                            // clerestory band
    for (int i = 0; i < 10; i++) m.box(15 + i * 11.0f, 56.9f, 24.6f, 0.5f, 0.35f, 3.2f, kHull).bev(0.1f);
    for (int i = 0; i < 6; i++) {
        const float xa = 11 + i * 17.6f, xb = xa + 17.6f, top = 33.6f;
        m.profileY({ { xa, 28.6f }, { xb, top }, { xb, 28.6f } }, 8, 36, i % 2 ? kRoof : kRoofD).t(T_RIBS, 2.4f).bev(0.3f);
        m.profileY({ { xb - 6.0f, 30.2f }, { xb - 0.4f, top - 1.4f }, { xb - 0.4f, 30.0f } }, 12, 32, kWin).m(PAINT).t(T_GRILLE, 1.8f);   // glazed tooth face
        m.box(xb - 0.35f, 22, (top + 28.6f) * 0.5f, 0.45f, 13.6f, (top - 28.6f) * 0.5f, kHull2).bev(0.15f);            // its frame
    }
    m.box(64, 8.4f, 29.4f, 56, 0.95f, 1.8f, kRoofD).t(T_RIBS, 2.4f).bev(0.2f);                      // the high back wall the teeth run to
    m.box(64, 36.4f, 29.4f, 56, 1.2f, 1.9f, OWNER).bev(0.2f);                                        // owner's colour along the front eave
    m.box(96, 22, 29.6f, 14, 7.0f, 0.3f, kRoofD).bev(0.15f);                                         // a patched sheet on the roof
    // two brick stacks at the back
    stack(m, 22, 18, 28.6f, 5.2f, 30.0f, kBrickL);
    stack(m, 36, 18, 28.6f, 4.6f, 25.0f, kBrickL);
    m.box(22, 18, 26.0f, 4.6f, 4.6f, 0.6f, kDark).bev(0.2f);
    // the vehicle bay: a wide roller door with slatted sheets and a hazard bar, in a plate frame
    m.box(64, 56.7f, 13.2f, 20.5f, 0.8f, 12.3f, kDark).t(T_RIBS, 2.2f).bev(0.3f);
    for (int i = 0; i < 5; i++) m.box(64, 57.2f, 3.4f + i * 4.6f, 19.0f, 0.4f, 0.4f, kSteel).m(METAL).bev(0.1f);
    m.box(64, 57.6f, 26.4f, 22.0f, 0.5f, 1.1f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);
    m.box(64, 57.4f, 1.9f, 22.0f, 0.4f, 0.5f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) m.box(64 + s * 23.4f, 56.9f, 13.0f, 2.6f, 0.7f, 13.0f, kHull).bev(0.3f);
    m.box(48, 57.8f, 25.0f, 2.4f, 0.4f, 0.8f, kGlow).m(EMIT);
    m.box(80, 57.8f, 25.0f, 2.4f, 0.4f, 0.8f, kGlow).m(EMIT);
    m.box(44, 56.6f, 20.4f, 8.0f, 0.4f, 1.6f, OWNER).bev(0.15f);                                     // the bay's banner
    // the gantry crane over the forecourt: two legs, a beam, a trolley and its hook
    for (int s = -1; s <= 1; s += 2) {
        m.box(16 + (s > 0 ? 34 : 0), 84, 7.0f, 1.8f, 1.8f, 6.2f, kSteel).m(METAL).t(T_PANEL, 3).bev(0.3f);
        m.box(16 + (s > 0 ? 34 : 0), 84, 1.6f, 3.0f, 3.0f, 0.8f, kConc).m(MATTE).bev(0.3f);
    }
    m.box(33, 84, 14.6f, 18.0f, 1.4f, 1.0f, kSteel).m(METAL).t(T_PANEL, 4).bev(0.3f);
    m.box(29, 84, 16.4f, 4.0f, 2.4f, 1.3f, kRust).m(METAL).bev(0.3f);
    m.rod({ 29, 84, 15.4f }, { 29, 84, 9.6f }, 0.3f, kDark).f(F_NOSHADOW);
    m.rod({ 29, 84, 9.4f }, { 29, 84, 8.2f }, 1.2f, kSteel).m(METAL);
    // scrap hulls and drums on the apron, sandbags along the south-east corner
    m.box(22, 82, 3.0f, 13, 5.5f, 2.4f, kHull2).t(T_CAMO, 4, kHull).bev(0.4f);
    m.box(22, 82, 5.4f, 9, 3.6f, 1.4f, kRust).m(METAL).bev(0.3f);
    m.box(22, 82, 6.8f, 6, 2.2f, 1.0f, kDark).bev(0.2f);
    for (int i = 0; i < 3; i++) drum(m, 112, 66 + i * 7.0f, 0.9f, 3.2f, i == 1 ? kRust : kHull2);
    sandLine(m, 96, 90, 120, 90, 0.9f, 2);
    for (int i = 0; i < 3; i++) crate(m, 86 + i * 9.0f, 68, 0.9f, 3.4f, 3.4f, 4.0f, C(0x9E7C4A));
}
M3D_BUILD(B_K_FACTORY, kFactory)

// ---------------------------------------------------------------- Clanker Airstrip, 10 x 7 tiles
// Old painter: airfieldClanker. A concrete apron marked with two runways and the 25 parking pads the aircraft use, a tall control
// cab, a quonset hangar, fuel tanks, a windsock and sandbags. All markings are slabs a few centimetres thick.
static void kAirfield(Model& m, int) {
    using namespace pal;
    const float W = 10 * BPX, H = 7 * BPX;
    m.dirt = 0.25f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.7f, C(0x726E62)).m(MATTE).t(T_PANEL, 26).bev(0.4f);
    // the two runways: a darker strip and its white centre dashes
    for (int r = 0; r < 2; r++) {
        const float y = H * (0.36f + 0.28f * r);
        m.slab(W * 0.5f, y, W * 0.5f - 6, 5.0f, 0.7f, 0.16f, C(0x63605A)).m(MATTE).bev(0.05f);
        for (int i = 0; i < 8; i++) m.slab(22 + i * 32.0f, y, 5.0f, 0.9f, 0.86f, 0.12f, C(0xD6D2C4)).m(MATTE).bev(0.05f);
    }
    // the 5 x 5 parking grid of the aircraft: a yellow ring on each pad (a ring is a wide octagon under a pad-coloured one)
    const std::vector<P2> oct = { { 12, 0 }, { 8.5f, 8.5f }, { 0, 12 }, { -8.5f, 8.5f }, { -12, 0 }, { -8.5f, -8.5f }, { 0, -12 }, { 8.5f, -8.5f } };
    for (int row = 0; row < 5; row++)
        for (int col = 0; col < 5; col++) {
            const float cx = W * 0.5f + (col - 2) * 42, cy = H * 0.5f + (row - 2) * 34;
            std::vector<P2> big, small;
            for (const P2& p : oct) { big.push_back({ cx + p.a, cy + p.b }); small.push_back({ cx + p.a * 0.86f, cy + p.b * 0.86f }); }
            m.plan(big, 0.7f, 0.78f, kHaz).m(MATTE).bev(0.05f);
            m.plan(small, 0.7f, 0.84f, C(0x726E62)).m(MATTE).bev(0.05f);
        }
    // yellow edge markers along the north and south edges
    for (int i = 0; i < 6; i++) {
        const float x = 16 + i * (W - 32.0f) / 5;
        m.slab(x, 5.0f, 0.8f, 1.8f, 0.7f, 0.14f, kHaz).m(MATTE).bev(0.05f);
        m.slab(x, H - 5.0f, 0.8f, 1.8f, 0.7f, 0.14f, kHaz).m(MATTE).bev(0.05f);
    }
    // ---- the control tower on the north-west: a concrete leg, a glazed cab, a mast
    m.slab(19, 18, 9, 8, 0.7f, 38.0f, kConc).m(MATTE).t(T_BRICK, 4.0f).bev(0.6f);
    m.slab(19, 18, 10.6f, 9.6f, 38.7f, 1.0f, kHull2).t(T_PANEL, 4).bev(0.3f);              // the cab's floor plate
    m.slab(19, 18, 9.6f, 8.6f, 39.7f, 8.6f, kWin).m(GLASS).t(T_WINDOWS, 4.4f, C(0x1E2A26)).bev(0.3f);
    for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2) m.box(19 + s * 9.6f, 10 + i * 8.0f, 44.0f, 0.5f, 0.6f, 4.4f, kHull2).bev(0.1f);
    m.slab(19, 18, 11.2f, 10.2f, 48.3f, 0.9f, kHull).t(T_PANEL, 5).bev(0.3f);               // cab roof with a lip
    m.slab(19, 24, 9.6f, 8.6f, 49.2f, 0.25f, kHull2).bev(0.1f);                             // the cab's roof deck
    m.slab(19, 24, 8.4f, 5.0f, 49.45f, 0.2f, OWNER).bev(0.1f);                              // the owner's colour on it
    mast(m, 27, 11, 49.2f, 15, C(0xFF5A48));
    m.box(19, 26.4f, 6.4f, 2.2f, 0.4f, 5.6f, kDark).t(T_PLANKS, 1.8f).bev(0.2f);            // its door
    // ---- the quonset hangar: an arched shell, ribbed, with a wide roller door
    {
        const float x0 = W - 126, x1 = x0 + 96, cy = 27;
        std::vector<P2> arc;
        for (int i = 0; i <= 8; i++) { const float a = 3.1416f * i / 8; arc.push_back({ cy + std::cos(a) * 17.0f, 0.7f + std::sin(a) * 23.0f }); }
        m.profileX(arc, x0, x1, kRoof).t(T_RIBS, 6.6f).bev(0.5f);
        m.box(x0 + 47, cy, 12.4f, 47, 0.6f, 0.35f, kRoofD).t(T_RIBS, 2.4f).bev(0.2f);       // spine along the crown
        m.box(x0 + 48, cy + 17.8f, 18.4f, 30, 0.9f, 1.6f, OWNER).bev(0.15f);                 // the owner's band over the hangar door
        m.box(x0 + 48, cy - 17.8f, 18.4f, 30, 0.9f, 1.6f, OWNER).bev(0.15f);                 // and over the back door
        m.box(x0 + 48, cy + 17.2f, 8.4f, 30, 0.7f, 8.2f, kDark).t(T_RIBS, 2.4f).bev(0.3f);   // roller door
        m.box(x0 + 48, cy + 17.6f, 17.4f, 32, 0.6f, 1.0f, kHaz).t(T_HAZARD, 2.6f, kDark).bev(0.1f);
        for (int i = 0; i < 7; i++) {   // arch ribs on the front face
            const float a = 0.45f + i * 0.375f;
            m.rod({ x0 + 2, cy + std::cos(a) * 17.2f, 0.7f + std::sin(a) * 23.2f }, { x0 + 2, cy + std::cos(a) * 17.2f, 0.7f }, 0.55f, kRoofD).f(F_NOSHADOW);
        }
        m.cyl('z', x0 + 78, cy - 9, 23.0f, 2.2f, 2.4f, kRust).m(METAL).bev(0.3f);            // a vent on the crown
    }
    // a painted marshalling box on the apron, the way airstrips mark their taxi lines
    for (int i = 0; i < 4; i++) {
        const bool side = i > 1;
        const float bx = side ? (i == 2 ? 78.0f : 214.0f) : 146.0f, by = side ? 196.0f : (i == 0 ? 186.0f : 206.0f);
        m.box(bx, by, 0.94f, side ? 2.2f : 68.0f, side ? 10.0f : 2.2f, 0.12f, OWNER).bev(0.1f);
    }
    // ---- the fuel dump on the south-west: two tanks, a pipe and drums
    for (int i = 0; i < 2; i++) {
        const float x = 16 + i * 20.0f, y = H - 26;
        m.cyl('z', x, y, 0.7f + 6.4f, 8, 6.4f, i ? kRust : kHull2, 0.95f).t(T_PANEL, 7).bev(0.5f);
        m.cyl('z', x, y, 0.7f + 12.9f, 3.0f, 0.4f, kSteel).m(METAL).bev(0.2f);
        for (int s = -1; s <= 1; s += 2) m.box(x + s * 6.0f, y, 1.5f, 1.2f, 4.2f, 1.0f, kConc).m(MATTE);
    }
    m.rod({ 16, H - 35, 4 }, { 36, H - 35, 4 }, 1.0f, kSteel).m(METAL);
    for (int i = 0; i < 5; i++) drum(m, 50 + i * 6.0f, H - 18, 0.7f, 2.7f, i == 1 ? kRust : kHull2);
    // ---- the windsock on the south-east
    m.cyl('z', W - 14, H - 15, 0.7f + 5.0f, 0.6f, 5.0f, kSteelL).m(METAL).f(F_NOSHADOW);
    m.cyl('x', W - 22, H - 15, 12.6f, 0.35f, 7.0f, kSteelL).m(METAL).f(F_NOSHADOW);
    for (int i = 0; i < 3; i++) m.cyl('x', W - 26 + i * 4.0f, H - 15, 12.6f + 0.3f * (i & 1), 2.1f - i * 0.3f, 1.6f, i & 1 ? C(0xE8E2D0) : C(0xE06E2E), 0.82f).m(MATTE);
    // ---- sandbags along the south edge
    sandLine(m, W * 0.42f, H - 8.0f, W * 0.72f, H - 8.0f, 0.7f, 2);
    // a shed for the ground crew
    m.slab(W - 40, H - 20, 12, 9, 0.7f, 9.0f, kHull2).t(T_WINDOWS, 6.0f, kWin).bev(0.5f);
    m.profileX({ { H - 29, 9.0f }, { H - 29, 10.2f }, { H - 20, 15.0f }, { H - 11, 10.2f }, { H - 11, 9.0f } }, W - 52, W - 28, kRoof).t(T_RIBS, 2.6f).bev(0.4f);
    m.box(W - 40, H - 10.6f, 4.6f, 4.0f, 0.4f, 4.2f, kDark).t(T_PLANKS, 1.6f).bev(0.2f);
}
M3D_BUILD(B_K_AIRFIELD, kAirfield)

// ---------------------------------------------------------------- Clanker Arms Lab, 3 x 3 tiles
// Old painter: techClanker. A research hall with a lit window row, a radome on a drum, a test barrel poking out east over an
// ammunition rack, a mast array and a hazard-striped pipe run.
static void kTech(Model& m, int) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX;
    m.dirt = 0.4f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    // the lab hall (old plan: x 8..58, y 16..74): plate walls with a window row south, a ribbed roof with a louvred vent
    m.slab(33, 45, 25, 29, 0.9f, 20.0f, kHull2).t(T_WINDOWS, 7.0f, kWin).bev(0.7f);
    m.slab(33, 45, 25.4f, 29.4f, 20.9f, 0.8f, kRoof).t(T_RIBS, 3.0f).bev(0.4f);
    m.box(33, 45, 21.9f, 25.2f, 0.8f, 0.35f, kRoofD).t(T_RIBS, 2.2f).bev(0.2f);
    m.box(26, 26, 22.2f, 11, 6.5f, 0.3f, OWNER).bev(0.15f);                                     // owner's colour on the roof
    m.box(46, 36, 21.6f, 6, 4.0f, 1.0f, kSteel).t(T_GRILLE, 1.5f).bev(0.3f).rx(-14);             // roof vent
    m.box(14, 60, 22.0f, 4.6f, 4.6f, 0.5f, kSteel).t(T_GRILLE, 1.4f).bev(0.2f);                 // a louvred hatch
    // south wall: the door under its hazard bar, a lamp, a hazard stripe at the foot
    m.box(22, 74.4f, 6.2f, 6.0f, 0.55f, 5.6f, kDark).t(T_PLANKS, 1.8f).bev(0.25f);
    m.box(22, 74.9f, 12.2f, 7.0f, 0.5f, 0.8f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.1f);
    m.box(22, 74.7f, 9.4f, 4.0f, 0.4f, 2.0f, OWNER).bev(0.1f);
    m.box(38, 74.7f, 8.2f, 1.7f, 0.4f, 0.7f, kGlow).m(EMIT);
    m.box(33, 74.6f, 1.6f, 25.0f, 0.35f, 0.7f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);
    // the observatory: a steel drum with a pale radome, standing against the hall's east wall
    m.cyl('z', 68, 46, 0.9f + 7.2f, 13.0f, 7.2f, kSteel).m(METAL).t(T_PANEL, 9).bev(0.6f);
    m.cyl('z', 68, 46, 0.9f + 14.6f, 13.4f, 0.5f, kDark).bev(0.2f);
    m.dome(68, 46, 15.5f, 12.6f, 12.6f, 12.4f, C(0xD6D8D4)).m(PAINT).t(T_PANEL, 6).bev(0.4f);
    m.box(68, 46, 28.3f, 2.6f, 2.6f, 0.6f, kSteel).m(METAL).bev(0.2f);                          // crown cap
    m.box(68, 58.2f, 19.0f, 3.0f, 0.4f, 1.6f, kDark).t(T_GRILLE, 1.2f).bev(0.1f);              // the dome's shutter
    m.box(68, 33.0f, 10.0f, 11.0f, 0.4f, 6.0f, OWNER).bev(0.15f);                                // owner's band on the drum
    // the ordnance test barrel on its proof mount, poking out east over an ammunition rack
    m.box(64, 60, 4.6f, 6.0f, 6.0f, 3.7f, kConc).m(MATTE).t(T_PANEL, 4).bev(0.4f);
    m.box(64, 60, 9.4f, 5.0f, 5.0f, 1.4f, kSteel).m(METAL).t(T_PANEL, 3).bev(0.3f).ry(-8);
    m.rod({ 64, 61, 11.4f }, { 93, 55, 12.4f }, 2.6f, kSteel).m(METAL).t(T_PANEL, 5).bev(0.4f);
    m.cyl('x', 84.0f, 56.1f, 12.2f, 3.4f, 1.2f, kSoot).m(METAL).bev(0.3f);                      // the muzzle swell
    m.cyl('x', 93.6f, 54.9f, 12.4f, 2.2f, 0.6f, kDark).bev(0.2f);
    for (int i = 0; i < 4; i++) m.cyl('x', 70 + i * 3.2f, 78, 2.4f, 1.3f, 8.0f, i % 2 ? kBrickL : kHaz).bev(0.35f);
    m.box(78, 78, 1.4f, 12, 3.6f, 0.5f, kPlank).m(MATTE).t(T_PLANKS, 2.0f).bev(0.2f);            // the rack they lie on
    m.box(56, 66, 1.5f, 4.0f, 7.0f, 0.6f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);               // proof-firing hazard stripe
    // the mast array on the west, and a pipe run along the front
    for (int i = 0; i < 3; i++) mast(m, 14 + i * 8.0f, 18 + (i & 1) * 5.0f, 0.9f, 32 - i * 8.0f, i == 2 ? kGlow : C(0xFF5A48));
    m.rod({ 8, 84, 4.6f }, { 40, 84, 4.6f }, 1.3f, kRust).m(METAL).t(T_PANEL, 5);
    for (int i = 0; i < 3; i++) drum(m, 48 + i * 5.0f, 84, 0.9f, 2.6f, i == 1 ? kRust : kHull2);
    sandLine(m, 8, 90, 38, 90, 0.9f, 2);
    m.box(8, 82, 3.4f, 8, 5, 2.4f, kWood).m(MATTE).t(T_PLANKS, 2.0f).bev(0.3f);
}
M3D_BUILD(B_K_TECH, kTech)

// ---------------------------------------------------------------- Clanker Gun Nest, 1 x 1 tiles: the sandbagged pit and its pedestal
// Old painter: gunBase. The aiming head (a shielded twin MG) is the prop "head.k_mg"; its base sits at PED_MG.
static void kMG(Model& m, int) {
    using namespace pal;
    m.dirt = 0.45f;
    m.cyl('z', 16, 16, 0.45f, 15.4f, 0.45f, kPave).m(MATTE).t(T_NOISE, 6.0f, kConc).bev(0.4f);   // the pad
    m.cyl('z', 16, 16, 1.15f, 13.0f, 0.7f, kConc).m(MATTE).t(T_PANEL, 5.0f).bev(0.4f);            // the concrete ring
    m.cyl('z', 16, 16, 1.0f, 10.4f, 0.12f, C(0x3A3226)).m(MATTE).bev(0.1f);                       // the pit floor
    // duckboards across the pit
    for (int i = 0; i < 3; i++) m.box(10.5f + i * 4.4f, 17.5f, 1.35f, 1.8f, 5.6f, 0.35f, kPlank).m(MATTE).t(T_PLANKS, 1.6f).bev(0.1f);
    // the sandbag ring: two staggered courses, kept low so the head has a clear field
    for (int r = 0; r < 2; r++)
        for (int i = 0; i < 11; i++) {
            const float an = (i + 0.5f + 0.45f * r) * 0.5712f;
            m.ball(16 + std::cos(an) * 12.0f, 16 + std::sin(an) * 12.0f, 2.4f + r * 2.2f, 3.4f, 2.5f, 1.9f, (i + r) % 3 ? kSand : kSand2).m(MATTE);
        }
    // the pedestal: an armoured drum on a riveted skirt, a hazard band and a ring of bolts
    m.cyl('z', 16, 16, 2.6f, 9.0f, 1.7f, kSteel).m(METAL).t(T_PANEL, 4.5f).bev(0.45f);
    m.cyl('z', 16, 16, 4.5f, 8.6f, 0.3f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);
    m.cyl('z', 16, 16, 5.55f, 8.2f, 0.75f, kHull2, 0.96f).t(T_PANEL, 4).bev(0.4f);
    m.cyl('z', 16, 16, 6.3f, 7.4f, 0.28f, kDark).bev(0.15f);
    m.cyl('z', 16, 16, 6.5f, 5.6f, 0.16f, OWNER).bev(0.05f);                                      // the owner's ring on the deck
    m.cyl('z', 16, 16, 6.66f, 4.6f, 0.06f, kHull2).bev(0.05f);
    for (int i = 0; i < 8; i++) { const float an = i * 0.7854f; m.ball(16 + std::cos(an) * 6.8f, 16 + std::sin(an) * 6.8f, 6.42f, 0.6f, 0.6f, 0.3f, kSteelL).m(METAL); }
    // ammunition crate and a spare barrel on the pit floor
    m.box(24, 11.5f, 2.4f, 3.0f, 2.2f, 1.5f, kHull2).m(MATTE).t(T_PLANKS, 1.6f).bev(0.25f);
    m.rod({ 7.5f, 23.0f, 1.7f }, { 16.0f, 23.0f, 1.7f }, 0.8f, kSteel).m(METAL).bev(0);
    m.box(8.0f, 20.5f, 1.6f, 1.4f, 1.4f, 0.6f, kHaz).t(T_HAZARD, 1.4f, kDark).bev(0.1f);           // the serial plate on the ring
}
M3D_BUILD(B_K_MG, kMG)

// the aiming head of the Gun Nest: a shielded twin heavy machine gun (old artTurretHead index 1). Pivots at (0,0), points +x.
static const float PED_MG = 6.5f;   // the pedestal top of B_K_MG
static void headMG(Model& m, int) {
    using namespace pal;
    m.ground = false;
    const float z0 = PED_MG;
    m.cyl('z', 0, 0, z0 + 1.0f, 7.0f, 1.0f, kSteel).m(METAL).t(T_PANEL, 4.0f).bev(0.4f);           // the turntable
    m.box(0, 0, z0 + 2.9f, 3.6f, 6.4f, 1.0f, kHull2).bev(0.4f);                                    // the cradle
    m.box(0, 0, z0 + 6.2f, 5.4f, 8.6f, 2.6f, kHull2).t(T_CAMO, 5.0f, kHull).bev(0.6f);              // the gun shield
    m.box(5.6f, 0, z0 + 6.0f, 0.5f, 8.9f, 2.4f, kDark).bev(0.15f);                                 // its dark face
    m.box(0, 0, z0 + 8.4f, 5.0f, 8.2f, 0.9f, kHull2).bev(0.15f);                                    // the shield's crown
    m.box(0, 0, z0 + 9.3f, 4.0f, 2.4f, 0.22f, OWNER).bev(0.1f);                                     // the owner's flash on it
    m.box(5.7f, 0, z0 + 8.6f, 0.4f, 8.4f, 0.35f, kDark).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) {
        m.rod({ 4.0f, s * 3.4f, z0 + 6.6f }, { 27.0f, s * 3.4f, z0 + 6.6f }, 1.35f, kSteel).m(METAL).bev(0.15f);
        m.cyl('x', 21.0f, s * 3.4f, z0 + 6.6f, 1.8f, 1.2f, kDark).bev(0.25f);                      // cooling sleeve
        m.cyl('x', 27.6f, s * 3.4f, z0 + 6.6f, 1.5f, 1.0f, kDark).bev(0.25f);                      // muzzle
        m.box(2.0f, s * 5.6f, z0 + 4.4f, 2.4f, 1.4f, 1.6f, kSteel).m(METAL).bev(0.3f);              // feed trays
    }
    m.box(-5.6f, 0, z0 + 5.4f, 1.8f, 4.6f, 1.6f, kHull).bev(0.3f);                                 // rear counterweight
    m.box(-5.6f, 0, z0 + 6.9f, 0.6f, 2.2f, 0.9f, kGlow).m(EMIT);                                   // the sight's light
    m.box(-8.4f, -5.0f, z0 + 4.4f, 1.4f, 1.6f, 1.2f, kDark).bev(0.2f);                             // spent-case bag
    m.rod({ -4.0f, -4.0f, z0 + 8.6f }, { -1.0f, -6.2f, z0 + 12.0f }, 0.22f, kDark).f(F_NOSHADOW);   // aerial
}
M3D_PROP("head.k_mg", headMG)

// ---------------------------------------------------------------- Clanker Rocket Battery, 2 x 2 tiles: the apron and its pedestal
// Old painter: rocketBase. The rocket rack is the prop "head.k_rocket"; its base sits at PED_ROCKET.
static void kRocket(Model& m, int) {
    using namespace pal;
    const float W = 2 * BPX, H = 2 * BPX;
    m.dirt = 0.4f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 12).bev(0.4f);
    m.slab(W * 0.5f, H * 0.5f, 24, 22, 0.9f, 0.7f, kConc).m(MATTE).t(T_PANEL, 8).bev(0.35f);        // the firing apron
    m.box(32, 54.0f, 1.75f, 22, 0.8f, 0.25f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);              // hazard edge on the south
    for (int i = 0; i < 4; i++) {   // blast arrows painted on the apron
        m.box(14 + i * 12.0f, 46, 1.66f, 1.2f, 3.0f, 0.1f, kHaz).m(MATTE).bev(0.05f);
        m.box(14 + i * 12.0f, 43.0f, 1.66f, 2.2f, 1.2f, 0.1f, kHaz).m(MATTE).bev(0.05f);
    }
    // ammunition: crates across the north, a rack of rockets on the east, a cable reel
    for (int i = 0; i < 3; i++) crate(m, 10 + i * 8.0f, 8.0f, 0.9f, 3.6f, 3.2f, 4.0f, kHull2);
    for (int i = 0; i < 4; i++) m.cyl('x', 46, 6.0f + i * 2.6f, 2.8f, 1.1f, 8.0f, i % 2 ? kRust : kHaz).bev(0.3f);
    for (int s = -1; s <= 1; s += 2) m.box(54, 10.0f + s * 4.6f, 1.6f, 2.0f, 2.6f, 0.7f, kSteel).m(METAL).bev(0.2f);
    m.cyl('z', 52, 49, 1.6f + 1.6f, 3.4f, 1.6f, kSteel).m(METAL).bev(0.3f);                        // cable reel
    for (int i = 0; i < 2; i++) m.cyl('x', 52, 49, 1.6f + 3.2f - i * 0.4f, 1.2f, 0.4f, kDark).bev(0.1f);
    // sandbags round the pit and the gate on the south-west, drums in the north-west corner
    sandLine(m, 6, 60, 24, 60, 0.9f, 2);
    sandLine(m, 42, 60, 58, 60, 0.9f, 2);
    sandLine(m, 5, 14, 5, 52, 0.9f, 1);
    sandLine(m, 59, 14, 59, 52, 0.9f, 1);
    for (int i = 0; i < 2; i++) drum(m, 8 + i * 6.0f, 20, 0.9f, 2.6f, i ? kHull2 : kRust);
    // the pedestal: a concrete plinth under an armoured drum, hazard band, owner's ring
    m.cyl('z', 32, 32, 1.6f + 0.9f, 12.0f, 0.9f, kConc, 0.94f).m(MATTE).t(T_PANEL, 6).bev(0.4f);
    m.cyl('z', 32, 32, 3.4f + 1.3f, 10.0f, 1.3f, kSteel).m(METAL).t(T_PANEL, 5).bev(0.45f);
    m.cyl('z', 32, 32, 5.9f, 9.8f, 0.3f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);
    m.cyl('z', 32, 32, 6.85f, 9.0f, 0.65f, kHull2, 0.95f).t(T_PANEL, 4).bev(0.4f);
    m.cyl('z', 32, 32, 7.35f, 6.4f, 0.2f, OWNER).bev(0.05f);
    // a painted keep-clear box round the pit, in the owner's colour
    for (int i = 0; i < 4; i++) {
        const bool side = i > 1;
        const float bx = side ? (i == 2 ? 10.0f : 54.0f) : 32.0f, by = side ? 32.0f : (i == 0 ? 10.0f : 54.0f);
        m.box(bx, by, 1.66f, side ? 1.4f : 9.4f, side ? 9.4f : 1.4f, 0.12f, OWNER).bev(0.1f);
    }
    m.cyl('z', 32, 32, 7.45f, 5.6f, 0.15f, kDark).bev(0.1f);
    for (int i = 0; i < 6; i++) { const float an = i * 1.0472f + 0.3f; m.ball(32 + std::cos(an) * 7.6f, 32 + std::sin(an) * 7.6f, 7.5f, 0.6f, 0.6f, 0.3f, kSteelL).m(METAL); }
    // the generator set beside the plinth and its exhaust
    m.box(45, 44, 2.6f, 4.0f, 3.4f, 2.6f, kHull2).t(T_GRILLE, 1.6f).bev(0.35f);
    m.rod({ 47, 44, 5.2f }, { 47, 44, 12.0f }, 0.8f, kSoot).m(METAL);
}
M3D_BUILD(B_K_ROCKET, kRocket)

// the aiming head of the Rocket Battery: a six-tube rocket rack (old artTurretHead index 3). Pivots at (0,0), points +x.
static const float PED_ROCKET = 7.6f;   // the pedestal top of B_K_ROCKET
static void headRocket(Model& m, int) {
    using namespace pal;
    m.ground = false;
    const float z0 = PED_ROCKET;
    m.cyl('z', 0, 0, z0 + 1.1f, 8.2f, 1.1f, kSteel).m(METAL).t(T_PANEL, 4.5f).bev(0.4f);           // the turntable
    m.box(-1.0f, 0, z0 + 3.2f, 5.0f, 7.4f, 1.2f, kHull2).bev(0.4f);                                 // the trunnion yoke
    for (int s = -1; s <= 1; s += 2) m.box(1.6f, s * 8.2f, z0 + 5.4f, 3.4f, 1.0f, 2.4f, kSteel).m(METAL).bev(0.3f);
    // the launcher pod: six tubes facing +x in a riveted frame, the tube mouths stacked two high on the front face
    m.box(2.0f, 0, z0 + 8.4f, 13.0f, 12.8f, 5.0f, kHull2).t(T_PANEL, 6.0f).bev(0.7f);
    m.box(2.0f, 0, z0 + 13.5f, 12.6f, 12.0f, 0.4f, kHull).bev(0.2f);
    m.box(-2.0f, 0, z0 + 14.0f, 4.0f, 4.0f, 0.3f, OWNER).bev(0.15f);                                // the owner's panel on the pod's roof
    for (int j = 0; j < 2; j++)
        for (int i = 0; i < 3; i++) {
            const float ty = -8.2f + i * 8.2f, tz = z0 + 5.8f + j * 5.6f;
            m.cyl('x', 15.4f, ty, tz, 3.6f, 1.2f, kDark).bev(0.3f);                                  // the tube mouth
            m.cyl('x', 16.4f, ty, tz, 2.6f, 0.5f, kSoot).bev(0.1f);
            m.ball(16.7f, ty, tz, 1.4f, 1.4f, 1.4f, C(0xC8443A)).m(PAINT);                            // the rocket nose in its tube
            m.box(6.0f, ty, tz, 8.0f, 3.4f, 2.2f, kHull2).t(T_PANEL, 4).bev(0.3f);                   // the tube's own sleeve
        }
    m.box(-12.0f, 0, z0 + 8.4f, 1.4f, 10.0f, 4.0f, kHull).bev(0.3f);                                 // rear hatch
    m.box(-13.6f, 0, z0 + 8.4f, 0.5f, 4.4f, 2.4f, kDark).t(T_GRILLE, 1.4f).bev(0.15f);
    for (int s = -1; s <= 1; s += 2) m.box(2.0f, s * 13.2f, z0 + 8.4f, 10.0f, 0.5f, 1.2f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.1f);
    m.rod({ -4.0f, 5.0f, z0 + 14.2f }, { 1.0f, 7.6f, z0 + 18.0f }, 0.22f, kDark).f(F_NOSHADOW);      // aerial
    m.box(-6.0f, -6.0f, z0 + 14.0f, 2.2f, 2.6f, 0.5f, kGlow).m(EMIT);                                // sighting light
}
M3D_PROP("head.k_rocket", headRocket)

// ---------------------------------------------------------------- Clanker Oil Well, 2 x 2 tiles
// Old painter: oilwell. An oil-stained pad, the wellhead pit, a steel lattice derrick with a crown block, a storage tank and drums.
// The game draws the pump-jack beam live over the middle, so the derrick straddles the pit and the centre stays clear.
static void kOilwell(Model& m, int) {
    using namespace pal;
    m.dirt = 0.55f;
    m.slab(32, 32, 31, 31, 0, 0.9f, C(0x544E42)).m(MATTE).t(T_NOISE, 8.0f, C(0x3E3A30)).bev(0.4f);
    // the oil pool and the pit ring at the middle
    m.cyl('z', 32, 40, 0.95f, 13, 0.12f, C(0x120F0E)).m(MATTE).bev(0.1f);
    m.cyl('z', 32, 40, 1.0f, 9, 0.1f, C(0x2A2028)).m(MATTE).bev(0.1f);
    m.cyl('z', 32, 40, 1.6f, 6.0f, 0.6f, kSteel).m(METAL).t(T_PANEL, 3.0f).bev(0.3f);              // the wellhead
    m.cyl('z', 32, 40, 3.0f, 2.2f, 1.2f, kRust).m(METAL).bev(0.3f);
    m.ball(32, 40, 4.4f, 1.6f, 1.6f, 1.0f, kHaz).bev(0.1f);
    // the derrick: four raked legs, six brace levels per side, a crown block and a beacon
    for (int sx = -1; sx <= 1; sx += 2)
        for (int sy = -1; sy <= 1; sy += 2)
            m.rod({ 32 + sx * 11.0f, 48 + sy * 4.0f, 0.9f }, { 32 + sx * 2.6f, 40 + sy * 1.2f, 34.0f }, 0.75f, kSteelL).m(METAL);
    for (int i = 0; i < 6; i++) {
        const float k = (i + 1) / 6.0f, z = 0.9f + 33.0f * k, hw = 11.0f - 8.4f * k, hy = 4.0f - 2.8f * k, cy = 48 - 8.0f * k;
        m.box(32, cy - hy, z, hw, 0.35f, 0.35f, kSteelL).m(METAL).f(F_NOSHADOW);
        m.box(32, cy + hy, z, hw, 0.35f, 0.35f, kSteelL).m(METAL).f(F_NOSHADOW);
        for (int s = -1; s <= 1; s += 2) m.box(32 + s * hw, cy, z, 0.35f, hy, 0.35f, kSteelL).m(METAL).f(F_NOSHADOW);
        if (i < 4) m.box(32, cy - hy, z + 3.0f, hw * 0.8f, 0.25f, 2.6f, kSteelL).m(METAL).f(F_NOSHADOW).rx(38);   // zig-zag bracing
    }
    m.box(32, 40, 34.6f, 5.4f, 3.4f, 1.0f, kRust).m(METAL).t(T_PANEL, 4).bev(0.3f);                 // crown block
    m.cyl('z', 32, 40, 35.9f, 2.2f, 0.4f, kDark).bev(0.2f);
    m.rod({ 32, 40, 35.4f }, { 32, 44, 8.0f }, 0.25f, kDark).f(F_NOSHADOW);                          // drilling line
    m.ball(32, 37.0f, 37.0f, 1.2f, 1.2f, 1.2f, C(0xFF5A48)).m(EMIT);
    // the storage tank on its cradle, piped back to the head
    m.cyl('z', 52, 24, 0.9f + 7.2f, 7.0f, 7.2f, kRust).t(T_PANEL, 6.0f).bev(0.5f);
    m.cyl('z', 52, 24, 0.9f + 14.6f, 7.3f, 0.3f, kSteel).m(METAL).bev(0.2f);
    m.cyl('z', 52, 24, 0.9f + 7.2f, 7.3f, 0.4f, kDark).bev(0.1f);
    m.box(52, 24, 9.6f, 4.0f, 0.3f, 2.4f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) m.box(52 + s * 5.0f, 24, 1.5f, 1.0f, 5.4f, 1.1f, kConc).m(MATTE);
    m.rod({ 46, 36, 3.4f }, { 52, 30, 6.0f }, 1.3f, kSteel).m(METAL).t(T_PANEL, 4);
    m.rod({ 32, 40, 2.2f }, { 46, 36, 3.2f }, 1.1f, kRust).m(METAL);
    // drums in the west corner, a hazard strip and the owner's colour on the pad
    for (int i = 0; i < 3; i++) drum(m, 8 + i * 5.0f, 54, 0.9f, 2.6f, i == 1 ? kRust : kHull2);
    m.box(18, 57, 0.98f, 14, 1.4f, 0.12f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.05f);
    m.box(52, 52, 0.98f, 8.0f, 7.0f, 0.12f, OWNER).bev(0.1f);   // owner's painted square on the pad beside the tank
    m.box(8, 8, 1.3f, 5.0f, 4.0f, 0.8f, kWood).m(MATTE).t(T_PLANKS, 1.8f).bev(0.2f);                 // a tool chest
    m.rod({ 12, 10, 2.0f }, { 20, 10, 2.0f }, 0.6f, kSteel).m(METAL);
}
M3D_BUILD(B_K_OILWELL, kOilwell)

// ---------------------------------------------------------------- Clanker Nuke Ramp, 3 x 3 tiles
// Old painter: nukeClanker with the artArmed overlay. A launch ramp truss running north-east over an exhaust pit, fuel tanks, a
// blockhouse and sandbag revetments. state 0: the empty ramp. state 1: the same ramp with the missile on its rails
// (the old overlay's position: 14% to 86% along the rails, wide body, yellow band and a red nose).
static void kNuke(Model& m, int state) {
    using namespace pal;
    const float W = 3 * BPX, H = 3 * BPX;
    m.dirt = 0.4f;
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f - 1, H * 0.5f - 1, 0, 0.9f, kPave).m(MATTE).t(T_PANEL, 16).bev(0.4f);
    m.box(48, 5.6f, 1.25f, 40, 1.6f, 0.35f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);               // hazard edges
    m.box(48, 90.4f, 1.25f, 40, 1.6f, 0.35f, kHaz).t(T_HAZARD, 2.4f, kDark).bev(0.1f);
    // the exhaust pit under the west end of the rails
    m.cyl('z', 24, 74, 1.0f, 15, 0.12f, C(0x171412)).m(MATTE).bev(0.1f);
    m.cyl('z', 24, 74, 1.05f, 11, 0.1f, C(0x2E2620)).m(MATTE).bev(0.1f);
    for (int i = 0; i < 3; i++) m.cyl('z', 24, 74, 1.1f + i * 0.06f, 9.0f - i * 3.0f, 0.06f, kHaz).m(MATTE).bev(0.05f);
    // the launch rails: two beams from the pit up to the north-east, on four trestle legs
    const V3 a = { 18, 78, 3.2f }, b = { 76, 24, 15.4f };
    for (int s = -1; s <= 1; s += 2) {
        m.rod({ a.x, a.y + s * 4.6f, a.z }, { b.x, b.y + s * 4.6f, b.z }, 1.2f, kSteel).m(METAL).t(T_PANEL, 6).bev(0.2f);
        m.rod({ a.x, a.y + s * 4.6f, a.z + 1.4f }, { b.x, b.y + s * 4.6f, b.z + 1.4f }, 0.5f, kSteelL).m(METAL);
    }
    for (int i = 0; i < 5; i++) {
        const float k = 0.09f + i * 0.2f;
        const float px = a.x + (b.x - a.x) * k, py = a.y + (b.y - a.y) * k, pz = a.z + (b.z - a.z) * k;
        for (int s = -1; s <= 1; s += 2) m.box(px, py + s * 4.6f, pz * 0.5f, 1.0f, 1.0f, pz * 0.5f, kRust).m(METAL).bev(0.2f);
        m.box(px, py, pz, 0.9f, 5.6f, 0.55f, kSteelL).m(METAL).f(F_NOSHADOW);                        // cross ties under the beams
    }
    m.box(48, 50, 2.0f, 4.0f, 4.0f, 1.1f, kConc).m(MATTE).bev(0.3f);                                 // the rail footings
    // the missile (state 1): a wide white body with a yellow band and a red nose, on the rails
    if (state == 1) {
        const V3 p0 = { a.x + (b.x - a.x) * 0.14f, a.y + (b.y - a.y) * 0.14f, a.z + (b.z - a.z) * 0.14f + 3.2f };
        const V3 p1 = { a.x + (b.x - a.x) * 0.86f, a.y + (b.y - a.y) * 0.86f, a.z + (b.z - a.z) * 0.86f + 3.2f };
        m.rod(p0, p1, 4.6f, C(0xD2D4D2)).m(PAINT).t(T_PANEL, 7).bev(0.6f);
        const V3 band = { p0.x + (p1.x - p0.x) * 0.46f, p0.y + (p1.y - p0.y) * 0.46f, p0.z + (p1.z - p0.z) * 0.46f };
        m.rod({ band.x - (p1.x - p0.x) * 0.06f, band.y - (p1.y - p0.y) * 0.06f, band.z - (p1.z - p0.z) * 0.06f },
              { band.x + (p1.x - p0.x) * 0.06f, band.y + (p1.y - p0.y) * 0.06f, band.z + (p1.z - p0.z) * 0.06f }, 4.8f, kHaz).m(PAINT);
        m.rod(p1, { p1.x + 6.4f, p1.y - 4.3f, p1.z + 1.9f }, 4.4f, C(0xD4463A), 0.12f).m(PAINT).bev(0.6f);   // the nose cone
        for (int i = 0; i < 4; i++) {                                                                 // the tail fins
            const float an = i * 1.5708f + 0.785f;
            m.plan({ { p0.x - 2.0f, p0.y - 2.0f }, { p0.x + 9.0f, p0.y - 1.4f }, { p0.x + 9.0f, p0.y + 1.4f }, { p0.x - 2.0f, p0.y + 2.0f } }, p0.z, p0.z + 1.2f, kSteel)
                .m(METAL).rz(an * 57.29578f);
        }
        m.box(48, 46, 6.0f, 5.0f, 3.0f, 1.2f, kRust).m(METAL).bev(0.3f);                              // the launch cradle
        m.cyl('z', 24, 74, 2.6f, 6.0f, 0.9f, kGlow).m(EMIT);                                           // the pit lights up
    }
    // fuel tanks and a blockhouse in the north-west, a pipe running to the ramp head
    m.cyl('z', 74, 74, 0.9f + 6.4f, 7.0f, 6.4f, kHull2, 0.96f).t(T_PANEL, 6).bev(0.5f);
    m.cyl('z', 74, 74, 0.9f + 12.9f, 3.0f, 0.35f, kSteel).m(METAL).bev(0.2f);
    m.box(74, 82.0f, 1.6f + 3.0f, 0.4f, 0.4f, 3.0f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.1f);
    m.cyl('z', 58, 80, 0.9f + 5.6f, 6.0f, 5.6f, kRust).t(T_PANEL, 6).bev(0.5f);
    m.cyl('z', 58, 80, 0.9f + 11.3f, 2.6f, 0.35f, kSteel).m(METAL).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) m.box(58 + s * 4.4f, 80, 1.5f, 1.0f, 4.4f, 1.0f, kConc).m(MATTE);
    m.rod({ 74, 67, 9.0f }, { 64, 34, 11.0f }, 1.1f, kSteel).m(METAL).t(T_PANEL, 4);
    m.slab(17, 19, 11, 9, 0.9f, 14.3f, kHull2).t(T_WINDOWS, 6.0f, kWin).bev(0.6f);
    m.profileX({ { 10, 15.2f }, { 10, 16.4f }, { 19, 21.4f }, { 28, 16.4f }, { 28, 15.2f } }, 6, 28, kRoof).t(T_RIBS, 2.6f).bev(0.4f);
    m.box(17, 28.2f, 5.6f, 5.0f, 0.4f, 4.6f, kDark).t(T_PLANKS, 1.8f).bev(0.2f);                      // its door
    m.box(17, 28.6f, 12.4f, 10.0f, 0.35f, 2.2f, OWNER).bev(0.1f);                                    // the owner's band on the blockhouse
    m.box(17, 16, 22.0f, 8.0f, 6.0f, 0.35f, OWNER).bev(0.15f);                                       // and a panel on its roof
    mast(m, 27, 12, 21.4f, 14, C(0xFF5A48));
    // the firing console beside the pit, under its shield
    m.box(14, 58, 2.4f, 3.4f, 3.4f, 1.9f, kHull2).bev(0.3f);
    m.box(14, 58, 5.1f, 4.4f, 4.4f, 0.5f, kSteel).m(METAL).t(T_PANEL, 3).bev(0.2f).rx(-16);
    m.box(16.6f, 58, 4.2f, 0.4f, 1.6f, 0.6f, kGlow).m(EMIT);
    // sandbag revetments: a wall along the north-east and the south-west
    sandLine(m, 66, 8, 90, 8, 0.9f, 2);
    sandLine(m, 4, 86, 30, 86, 0.9f, 2);
    sandLine(m, 90, 30, 90, 82, 0.9f, 2);
}
M3D_BUILD(B_K_NUKE, kNuke)

}  // namespace
