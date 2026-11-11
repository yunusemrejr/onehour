// One Hour - 3D models: the seven neutral map structures (see m3d.h / m3d_reg.h). Weathered stone, timber, concrete and steel of their own,
// not either army's palette: whoever holds one shows through OWNER on flags, bands and doors. Plan positions follow the old 2D painters
// (artb_civic.cpp, artb_special.cpp); an old lift of ht pixels is z = ht / 0.7 (zh below).
#include "m3d_parts.h"
#include <cmath>

using namespace m3d;

namespace nt {
const Color stone = C(0xA29D8F), stoneD = C(0x6B665C), stoneL = C(0xBDB7A8);
const Color brick = C(0x8A5C42), brickD = C(0x5A3B2A), plaster = C(0xB7A98C);
const Color grass = C(0x6C7A46), grassD = C(0x4A5A2D), moss = C(0x56683A), earth = C(0x6E5F47);
const Color slate = C(0x4F5659), slateD = C(0x30363A), tile = C(0x6F6456), tileD = C(0x4B4238);
const Color timber = C(0x6E5139), timberD = C(0x45301F), plank = C(0x8A6A4C), rust = C(0x7A4B2E);
const Color concrete = C(0x8C8A82), concreteD = C(0x5C5B56), concreteL = C(0xB4B2A9);
const Color steel = C(0x5D6367), steelD = C(0x363B3F), steelL = C(0x9BA2A6);
const Color glass = C(0x2C3D40), warm = C(0xFFC06A), dark = C(0x17181A), rubber = C(0x1D1D20);
const Color hazard = C(0xD8AE3C), hazardD = C(0x2A2A27);
const Color foliage = C(0x46603A), foliageD = C(0x2E4226), sand = C(0xA99E84), sandD = C(0x8F8570);
const Color rock = C(0x6E685E), rockD = C(0x4B4740), gravel = C(0x8A8270), gravelD = C(0x5F5A4C);
const Color red = C(0xFF5A48), redD = C(0xB8322A), white = C(0xE6E9EC);
}

// an old painter's lift in pixels -> model height
inline float zh(float ht) { return ht / 0.7f; }

// a box from the plan corners (x0, y0) to (x1, y1), standing from height z0 to z1
inline Prim& bx(Model& m, float x0, float x1, float y0, float y1, float z0, float z1, Color c) {
    return m.box((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f, (z1 - z0) * 0.5f, c);
}

// battlements: merlons on a wall top at height z, along the line y, from x0 to x1
inline void merlons(Model& m, float x0, float x1, float y, float z, Color c) {
    for (float x = x0; x + 4.8f <= x1; x += 8.0f) bx(m, x, x + 4.8f, y - 1.2f, y + 1.2f, z, z + 3.2f, c).t(T_BRICK, 6, nt::stoneD).bev(0.3f);
}

// a sandbag lying on the ground at (x, y)
inline void sandbag(Model& m, float x, float y) { m.box(x, y, 1.0f, 2.2f, 1.4f, 1.0f, nt::sand).t(T_NOISE, 3, nt::sandD).bev(0.8f); }

// ---------------------------------------------------------------- Old Castle, 4 x 4 tiles
// A curtain wall with battlements on the north and a foreground wall with a gatehouse on the south, four corner towers (the far two with
// slate cones, the near two battlemented with a flag), a slate-roofed keep, a well and a tree in the bailey.
static void nCastle(Model& m, int) {
    using namespace nt;
    const float TH = zh(31);   // tower height
    m.dirt = 0.25f;
    m.slab(64, 64, 64, 64, 0, 0.9f, grass).m(MATTE).t(T_NOISE, 9, grassD).bev(0.4f);                        // the meadow
    m.slab(64, 67, 52, 50, 0.5f, 0.6f, stoneL).m(MATTE).t(T_PANEL, 8, stoneD).bev(0.3f);                   // the flagged bailey

    // curtain walls: the far wall with battlements, the two side walls, the foreground wall with a gatehouse
    bx(m, 18, 110, 20, 35, 0, zh(18), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    merlons(m, 20, 108, 34.2f, zh(18), stone);
    bx(m, 16, 31, 25, 102, 0, zh(15), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    bx(m, 97, 112, 25, 102, 0, zh(15), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    bx(m, 20, 51, 105, 117, 0, zh(12), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    bx(m, 77, 108, 105, 117, 0, zh(12), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    merlons(m, 22, 49, 116.2f, zh(12), stone);
    merlons(m, 79, 106, 116.2f, zh(12), stone);

    // the keep: a slate gable running north to south, three arrow windows on its south wall, two small wings
    bx(m, 39, 89, 36, 70, 0, zh(27), stoneL).t(T_BRICK, 6, stoneD).bev(0.5f);
    m.profileY({ { 36, zh(27) - 1 }, { 64, zh(27) + 20 }, { 92, zh(27) - 1 } }, 36, 70, slate).t(T_TILES, 3, slateD).bev(0.5f);
    for (float x : {47.0f, 63.0f, 79.0f}) bx(m, x - 3.2f, x + 3.2f, 69.6f, 70.4f, 17.1f, 30, glass).m(GLASS).bev(0.2f);
    bx(m, 43, 49, 61, 73, 0, zh(15), stoneD).t(T_BRICK, 6, stoneD).bev(0.5f);
    bx(m, 79, 85, 61, 73, 0, zh(15), stoneD).t(T_BRICK, 6, stoneD).bev(0.5f);
    m.rod({64, 53, zh(27) + 20}, {64, 53, 72}, 0.5f, steelL).m(METAL).bev(0);
    bx(m, 64.4f, 76.4f, 52.8f, 53.2f, 62, 70, OWNER).bev(0.2f);                                           // the owner's flag
    bx(m, 39.5f, 88.5f, 69.8f, 70.6f, 33.4f, 36.0f, OWNER).bev(0.2f);                                     // the owner's band under the eaves

    // the four corner towers, 44 tall: stone drums with arrow slits on the south side
    auto tower = [&](float cx, float cy, bool cone) {
        m.cyl('z', cx, cy, TH * 0.5f, 11, TH * 0.5f, stone).t(T_BRICK, 6, stoneD).bev(0.6f);
        for (float z : {14.0f, 26.0f}) bx(m, cx - 0.45f, cx + 0.45f, cy + 10.4f, cy + 11.4f, z, z + 5, dark).m(MATTE).bev(0);
        if (cone) {
            m.cyl('z', cx, cy, TH + 7.5f, 12.2f, 7.5f, slate, 0).t(T_TILES, 2.5f, slateD).bev(0.3f);
        } else {
            for (int k = 0; k < 8; k++) {
                const float an = k * 0.7854f;
                m.box(cx + std::cos(an) * 9.6f, cy + std::sin(an) * 9.6f, TH + 1.6f, 2.2f, 1.1f, 1.6f, stone).t(T_BRICK, 6, stoneD).bev(0.3f).rz(k * 45.0f);
            }
            m.rod({cx + 4, cy, TH}, {cx + 4, cy, TH + 22.9f}, 0.5f, steelL).m(METAL).bev(0);
            bx(m, cx + 4.4f, cx + 16.4f, cy - 0.2f, cy + 0.2f, TH + 13, TH + 22, OWNER).bev(0.2f);     // the owner's flag
        }
    };
    tower(17, 29, true);
    tower(111, 29, true);
    tower(17, 110, false);
    tower(111, 110, false);

    // the gatehouse on the south wall: planked gate with iron straps under a stone arch
    bx(m, 49, 79, 98, 117, 0, zh(24), stone).t(T_BRICK, 6, stoneD).bev(0.5f);
    m.dome(64, 117.2f, zh(16), 9.5f, 1.2f, zh(24) - zh(16), stoneL).t(T_BRICK, 4, stoneD).bev(0);
    bx(m, 56.3f, 71.7f, 117.2f, 118.0f, 0, zh(16), OWNER).bev(0.2f);                                      // the owner's gate
    for (float z : {5.0f, 11.0f, 17.0f}) bx(m, 56.3f, 71.7f, 118.0f, 118.4f, z - 0.35f, z + 0.35f, steel).m(METAL).bev(0.1f);

    // the well with its winch frame, and a tree in the bailey
    m.cyl('z', 37, 83, 3.2f, 6, 3.2f, stone).t(T_BRICK, 4, stoneD).bev(0.4f);
    m.cyl('z', 37, 83, 6.45f, 4.2f, 0.35f, C(0x2E4A4F)).m(MATTE).bev(0);
    m.rod({31, 82, 0}, {31, 82, 14.3f}, 0.7f, timber).m(MATTE).bev(0);
    m.rod({43, 82, 0}, {43, 82, 14.3f}, 0.7f, timber).m(MATTE).bev(0);
    m.rod({30.5f, 82, 14.3f}, {43.5f, 82, 14.3f}, 0.8f, timberD).m(MATTE).bev(0);
    m.rod({91, 85, 0}, {91, 85, 9}, 1.1f, timber).m(MATTE).bev(0);
    m.ball(91, 85, 15, 6, 6, 5.5f, foliage).m(FOLIAGE).t(T_NOISE, 3, foliageD).bev(0);
}

// ---------------------------------------------------------------- Civilian Bunker, 3 x 3 tiles
// A low, thick concrete shelter with sloped walls and a sloped blast roof, firing slits on the south face and a steel door, earth banked
// against its back and sides, sandbags and a crate out front, a radio mast and a flag on a pole.
static void nBunker(Model& m, int) {
    using namespace nt;
    m.dirt = 0.2f;
    m.slab(48, 48, 48, 48, 0, 0.9f, grassD).m(MATTE).t(T_NOISE, 8, moss).bev(0.4f);                         // the yard
    m.slab(48, 53, 40, 31, 0, 1.0f, concrete).m(MATTE).t(T_PANEL, 12, concreteD).bev(0.4f);               // the apron
    m.dome(42, 40, 0.9f, 40, 24, 13, earth).m(MATTE).t(T_NOISE, 6, grassD).bev(0);                        // earth banked round it

    // the low shelter: sloped walls; the blast roof rises from its middle, also sloped
    m.taper(48, 55, 0, 36, 19, zh(22), 33, 16, concrete).m(MATTE).t(T_PANEL, 10, concreteD).bev(0.8f);
    m.taper(48, 48, 0, 26, 12, zh(38), 18, 9, concreteL).m(MATTE).t(T_PANEL, 10, concreteD).bev(0.8f);
    m.cyl('z', 56, 47, zh(38) + 0.9f, 3.6f, 0.9f, concreteL).bev(0.3f);                                    // roof vent
    m.cyl('z', 56, 47, zh(38) + 1.15f, 2.4f, 0.15f, dark).m(MATTE).bev(0);
    m.rod({36, 46, zh(38)}, {36, 46, 76}, 0.5f, steelL).m(METAL).bev(0);                                    // radio mast
    m.ball(36, 46, 76.8f, 1.0f, 1.0f, 1.0f, warm).m(EMIT);

    // firing slits on the sloped south face (the face is at y 74 at the ground, 71.9 at the top), the steel door on the ground
    for (float x : {20.0f, 31.0f, 42.0f, 53.0f, 64.0f, 75.0f}) bx(m, x, x + 7, 71.0f, 72.4f, 22.5f, 26.5f, dark).m(MATTE).bev(0);
    bx(m, 16, 80, 72.3f, 73.3f, 13.4f, 17.6f, OWNER).bev(0.2f);                                           // the owner's band on the shelter wall
    bx(m, 44, 54, 72.0f, 73.9f, 1.0f, 12.9f, OWNER).bev(0.2f);                                            // the owner's door
    bx(m, 42.5f, 55.5f, 73.4f, 74.3f, 1.0f, 13.8f, concreteD).bev(0.3f);
    m.ball(49, 74.0f, 6.5f, 0.9f, 0.5f, 0.9f, steelL).m(METAL);

    // yard: a hazard strip along the apron edge, sandbags, a crate and a drum on the ground
    bx(m, 12, 84, 74.6f, 76.2f, 1.0f, 1.6f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
    for (float x : {7.5f, 12.0f, 16.5f, 21.0f, 25.5f}) sandbag(m, x, 86.0f);
    for (float x : {68.0f, 72.5f, 77.0f, 81.5f}) sandbag(m, x, 86.0f);
    bx(m, 89, 95, 56, 62, 0, 6.5f, plank).t(T_PLANKS, 2.5f, timberD).bev(0.3f);
    m.cyl('z', 91, 72, 4.0f, 2.9f, 4.0f, rust).m(METAL).t(T_BAND, 1.0f, steelD).bev(0.3f);

    // the flag: a pole on the apron's east corner, the owner's cloth hanging off its top
    m.rod({88, 82, 0}, {88, 82, 40}, 0.45f, steelL).m(METAL).bev(0);
    bx(m, 75.6f, 88.4f, 81.8f, 82.2f, 28, 38, OWNER).bev(0.2f);
}

// ---------------------------------------------------------------- Watchtower, 2 x 2 tiles
// A stone shaft carries a timber gallery on posts and braces; the open cabin has a glazed band and a hatch, a slate pyramid roof, a ladder
// up the south face and the owner's flag on the apex.
static void nWatch(Model& m, int) {
    using namespace nt;
    m.dirt = 0.2f;
    m.slab(32, 32, 32, 32, 0, 0.9f, grass).m(MATTE).t(T_NOISE, 6, grassD).bev(0.4f);
    bx(m, 22, 42, 32, 60, 0, zh(20), stone).t(T_BRICK, 5, stoneD).bev(0.5f);                    // the stone shaft
    bx(m, 21, 43, 31, 61, zh(20), zh(20) + 1.2f, stoneL).m(MATTE).bev(0.4f);                     // its cap
    bx(m, 21.6f, 42.4f, 59.7f, 60.5f, 21.5f, 22.3f, stoneL).bev(0.2f);                           // a string course
    bx(m, 30.4f, 32.8f, 59.6f, 60.4f, 14.3f, 25.7f, dark).m(MATTE).bev(0);                       // an arrow slit

    // the gallery floor, the cabin, its glazed band and an open hatch
    const float PL = zh(20) + 1.2f;
    bx(m, 17, 47, 30, 54, PL, PL + 1.4f, timber).t(T_PLANKS, 3, timberD).bev(0.3f);
    bx(m, 19, 45, 32, 52, PL + 1.4f, 60, plank).t(T_PLANKS, 3, timberD).bev(0.3f);
    bx(m, 20.5f, 43.5f, 51.9f, 52.5f, 54.5f, 59.5f, glass).m(GLASS).bev(0.2f);
    bx(m, 29.5f, 34.5f, 51.9f, 52.4f, 31.2f, 42, dark).m(MATTE).bev(0);

    // timber posts at the corners of the gallery, with rails and cross braces on the south face
    for (float x : {17.0f, 47.0f}) for (float y : {30.0f, 54.0f}) m.rod({x, y, PL}, {x, y, 63}, 0.9f, timber).m(MATTE).bev(0);
    m.rod({17, 54, 37}, {47, 54, 37}, 0.5f, timber).m(MATTE).bev(0);
    m.rod({17, 54, 50}, {47, 54, 50}, 0.5f, timber).m(MATTE).bev(0);
    m.rod({17, 54, 31}, {47, 54, 49}, 0.45f, timber).m(MATTE).bev(0);
    m.rod({47, 54, 31}, {17, 54, 49}, 0.45f, timber).m(MATTE).bev(0);

    // the slate pyramid roof, the owner's band under its south eave, the flag on the apex
    m.taper(32, 42, 60, 16, 13, 16, 0.6f, 0.6f, slate).t(T_TILES, 2.5f, slateD).bev(0.4f);
    bx(m, 16, 48, 54.6f, 55.6f, 56.0f, 60.2f, OWNER).bev(0.2f);
    m.rod({32, 42, 76}, {32, 42, 94}, 0.45f, steelL).m(METAL).bev(0);
    bx(m, 32.4f, 44.4f, 41.8f, 42.2f, 82, 92, OWNER).bev(0.2f);

    // a ladder up the south face, from the ground to the gallery floor
    for (float x : {25.0f, 29.0f}) m.rod({x, 61, 0}, {x, 61, 29.5f}, 0.4f, steelD).m(METAL).bev(0);
    for (float z = 3; z < 29; z += 4) bx(m, 25, 29, 60.8f, 61.5f, z - 0.25f, z + 0.25f, steelL).m(METAL).bev(0.1f);
}

// ---------------------------------------------------------------- Abandoned Cottage, 2 x 2 tiles
// A one-storey stone cottage with a tiled pitched roof and a ridge, a chimney, a door, two windows (one lit), a roof band in the owner's colour,
// a low fence and a woodpile.
static void nCottage(Model& m, int) {
    using namespace nt;
    m.dirt = 0.2f;
    m.slab(32, 32, 32, 32, 0, 0.9f, grass).m(MATTE).t(T_NOISE, 7, grassD).bev(0.4f);
    bx(m, 29, 35, 50, 64, 0, 1.0f, stoneL).m(MATTE).t(T_PANEL, 4, stoneD).bev(0.2f);            // the path to the gate

    // walls, roof (a gable running east to west), ridge cap, owner's band on the south slope, chimney
    bx(m, 6, 58, 22, 50, 0, zh(14), plaster).t(T_BRICK, 4, stoneD).bev(0.4f);
    m.profileX({ { 20.5f, 19.5f }, { 36, 33.5f }, { 51.5f, 19.5f } }, 4, 60, tile).t(T_TILES, 2.5f, tileD).bev(0.4f);
    bx(m, 4, 60, 35.4f, 36.6f, 33.0f, 34.0f, tileD).bev(0.3f);
    m.profileX({ { 44, 26.27f }, { 47, 23.56f }, { 47, 24.56f }, { 44, 27.27f } }, 12, 24, OWNER);
    bx(m, 41, 46, 25, 31, 18, 37, brick).t(T_BRICK, 3, brickD).bev(0.3f);
    bx(m, 40, 47, 24.5f, 31.5f, 37, 38, stoneD).bev(0.3f);

    // the door (in a frame), two windows with their frames, one of them lit
    bx(m, 26.4f, 37.6f, 49.6f, 50.3f, 0, 16.0f, stoneD).bev(0.2f);
    bx(m, 27.5f, 36.5f, 50.0f, 50.6f, 0, 15.7f, OWNER).bev(0.1f);                                         // the owner's door
    m.cyl('z', 56, 54, 3.0f, 2.2f, 3.0f, rust).m(METAL).t(T_BAND, 0.9f, steelD).bev(0.3f);                // a rain barrel
    bx(m, 14.6f, 23.4f, 49.9f, 50.3f, 7.4f, 14.6f, stoneD).bev(0.2f);
    bx(m, 15.25f, 22.75f, 50.2f, 50.6f, 8, 14, glass).m(GLASS).bev(0.1f);
    bx(m, 40.6f, 49.4f, 49.9f, 50.3f, 7.4f, 14.6f, stoneD).bev(0.2f);
    bx(m, 41.25f, 48.75f, 50.2f, 50.6f, 8, 14, warm).m(EMIT);

    // a low fence along the yard, with a gap for the path
    for (float x : {3.0f, 10.0f, 17.0f, 25.0f, 39.0f, 46.0f, 53.0f, 61.0f}) m.rod({x, 60, 0}, {x, 60, 6}, 0.5f, timber).m(MATTE).bev(0);
    bx(m, 3, 25, 59.6f, 60.4f, 2.2f, 2.9f, timber).t(T_PLANKS, 2, timberD).bev(0.1f);
    bx(m, 3, 25, 59.6f, 60.4f, 4.6f, 5.3f, timber).t(T_PLANKS, 2, timberD).bev(0.1f);
    bx(m, 39, 61, 59.6f, 60.4f, 2.2f, 2.9f, timber).t(T_PLANKS, 2, timberD).bev(0.1f);
    bx(m, 39, 61, 59.6f, 60.4f, 4.6f, 5.3f, timber).t(T_PLANKS, 2, timberD).bev(0.1f);

    // a woodpile against the west wall, and a shrub by the east wall
    m.cyl('y', 2.6f, 43, 1.3f, 1.3f, 4.5f, plank).m(MATTE).t(T_RIBS, 1.5f, timberD).bev(0.3f);
    m.cyl('y', 4.8f, 43, 1.3f, 1.3f, 4.5f, plank).m(MATTE).t(T_RIBS, 1.5f, timberD).bev(0.3f);
    m.cyl('y', 3.7f, 43, 3.7f, 1.3f, 4.5f, plank).m(MATTE).t(T_RIBS, 1.5f, timberD).bev(0.3f);
    m.ball(58.5f, 40, 2.6f, 2.6f, 2.6f, 2.6f, foliage).m(FOLIAGE).t(T_NOISE, 2, foliageD).bev(0);
}

// ---------------------------------------------------------------- Roadside Workshop, 3 x 2 tiles
// A brick shed with a ribbed gable roof, a wide steel door, a glazed window, an owner band on the wall, a concrete yard with an anvil, a
// bench, barrels and a crate, a brick chimney stack with a flag.
static void nWorkshop(Model& m, int) {
    using namespace nt;
    m.dirt = 0.3f;
    m.slab(48, 32, 48, 32, 0, 0.9f, gravel).m(MATTE).t(T_NOISE, 8, gravelD).bev(0.4f);
    bx(m, 7, 89, 21, 53, 0, zh(17), brick).t(T_BRICK, 5, brickD).bev(0.5f);
    m.profileX({ { 19.5f, zh(17) }, { 37, 33.0f }, { 54.5f, zh(17) } }, 4, 92, steel).t(T_RIBS, 2.6f, steelD).bev(0.4f);

    // the wide door, its frame, the glazed window, the owner band
    bx(m, 41.2f, 43, 52.4f, 53.6f, 0, zh(17) + 0.6f, concreteD).bev(0.2f);
    bx(m, 67, 68.8f, 52.4f, 53.6f, 0, zh(17) + 0.6f, concreteD).bev(0.2f);
    bx(m, 43, 67, 52.6f, 53.4f, 0, 23.6f, OWNER).bev(0.2f);                                               // the owner's door
    bx(m, 40, 70, 52.0f, 52.9f, 25.2f, 27.6f, steelD).m(METAL).t(T_PANEL, 3, steel).bev(0.2f);            // the sign over it
    m.ball(38, 53.2f, 19.5f, 0.9f, 0.6f, 0.9f, warm).m(EMIT);                                             // a lamp by the door
    m.rod({5.5f, 53.4f, 0.5f}, {5.5f, 53.4f, 17.0f}, 0.4f, steelD).m(METAL).bev(0);                       // a downpipe
    m.cyl('z', 21, 60, 2.1f, 1.2f, 2.1f, steelD).m(METAL).bev(0.2f);                                       // gas bottles
    m.cyl('z', 23.2f, 60, 2.1f, 1.2f, 2.1f, rust).m(METAL).bev(0.2f);
    bx(m, 14.4f, 32.6f, 53.0f, 53.5f, 11, 21.8f, steelD).bev(0.2f);
    bx(m, 15, 32, 53.1f, 53.6f, 11.4f, 21.4f, glass).m(GLASS).bev(0.1f);
    bx(m, 13, 30, 52.7f, 53.3f, 2.9f, 7.1f, OWNER).bev(0.1f);

    // the yard: a concrete apron, an anvil with its horn, a bench, three barrels and a crate
    bx(m, 8, 88, 53, 62, 0, 1.0f, concreteL).m(MATTE).t(T_PANEL, 8, concreteD).bev(0.2f);
    bx(m, 16, 24, 56, 60, 1.0f, 4.0f, steelD).m(METAL).bev(0.4f);
    bx(m, 13, 27, 56.2f, 59.8f, 4.0f, 5.4f, steel).m(METAL).bev(0.4f);
    m.rod({13, 58, 4.7f}, {9.5f, 58, 4.7f}, 0.6f, steel).m(METAL).bev(0);
    bx(m, 31, 41, 56.5f, 59.5f, 3.2f, 4.0f, timber).t(T_PLANKS, 2, timberD).bev(0.2f);
    for (float x : {31.5f, 40.5f}) for (float y : {57.0f, 59.0f}) m.rod({x, y, 1.0f}, {x, y, 3.2f}, 0.4f, steelD).m(METAL).bev(0);
    m.cyl('z', 60, 58, 5.0f, 2.6f, 4.0f, rust).m(METAL).t(T_BAND, 0.9f, steelD).bev(0.3f);
    m.cyl('z', 68, 60, 5.0f, 2.6f, 4.0f, rust).m(METAL).t(T_BAND, 0.9f, steelD).bev(0.3f);
    m.cyl('z', 72, 56, 5.0f, 2.6f, 4.0f, rust).m(METAL).t(T_BAND, 0.9f, steelD).bev(0.3f);
    bx(m, 76, 84, 55, 61, 1.0f, 7.0f, plank).t(T_PLANKS, 3, timberD).bev(0.3f);

    // the chimney stack with its cap, and the owner's flag on a pole beside it
    bx(m, 78, 84, 24, 30, 18, 44, brick).t(T_BRICK, 3, brickD).bev(0.3f);
    bx(m, 77, 85, 23, 31, 44, 45.5f, steelD).bev(0.2f);
    m.rod({81, 27, 45.5f}, {81, 27, 58}, 0.4f, steelL).m(METAL).bev(0);
    bx(m, 81.4f, 89, 26.8f, 27.2f, 50, 56, OWNER).bev(0.2f);
}

// ---------------------------------------------------------------- Mountain Fortress, 5 x 4 tiles
// A concrete bunker complex dug into the rock: a front casemate on an apron with two blast doors and two roof gun pits, bastions at its
// ends, a main casemate behind it with a blast door and a camouflage net, and a high command bunker with an observation cupola, a mast and
// a flag. Rock rises at the back and in the north corners; tunnel mouths open on the west and east flanks.
static void nFortress(Model& m, int) {
    using namespace nt;
    m.dirt = 0.3f;
    // the ground: packed earth with moss, rock at the back and in the north corners, boulders in the gaps
    m.slab(80, 64, 80, 64, 0, 0.9f, earth).m(MATTE).t(T_NOISE, 10, gravelD).bev(0.4f);
    m.slab(40, 88, 12, 3.5f, 0.9f, 0.3f, moss).m(MATTE).t(T_NOISE, 4, grassD).bev(0.3f);
    m.slab(118, 90, 12, 3.5f, 0.9f, 0.3f, moss).m(MATTE).t(T_NOISE, 4, grassD).bev(0.3f);
    m.dome(80, 10, 0, 80, 9, 13, rock).m(MATTE).t(T_NOISE, 6, rockD).bev(0);
    m.dome(14, 26, 0, 12, 14, 16, rock).m(MATTE).t(T_NOISE, 6, rockD).bev(0);
    m.dome(146, 26, 0, 12, 14, 16, rock).m(MATTE).t(T_NOISE, 6, rockD).bev(0);
    m.ball(24, 88, 3.5f, 4, 4, 3.5f, rock).m(MATTE).t(T_NOISE, 3, rockD).bev(0.3f);
    m.ball(138, 88, 3.5f, 4, 4, 3.5f, rock).m(MATTE).t(T_NOISE, 3, rockD).bev(0.3f);
    m.ball(70, 90, 3.0f, 3.5f, 3.5f, 3.0f, rock).m(MATTE).t(T_NOISE, 3, rockD).bev(0.3f);

    // tunnel mouths on the flanks: a concrete lip with a dark throat on its south face
    bx(m, 0, 11, 58, 80, 0, 11, concreteL).m(MATTE).t(T_PANEL, 4, concreteD).bev(0.6f);
    bx(m, 3, 8, 79.5f, 80.4f, 2.5f, 8.5f, dark).m(MATTE).bev(0);
    bx(m, 149, 160, 58, 80, 0, 11, concreteL).m(MATTE).t(T_PANEL, 4, concreteD).bev(0.6f);
    bx(m, 152, 157, 79.5f, 80.4f, 2.5f, 8.5f, dark).m(MATTE).bev(0);

    // the high command bunker at the back: slits on its south face, a cupola, a mast, a periscope, a vent, a flag, earth on the roof
    bx(m, 30, 130, 18, 48, 0, zh(30), concrete).t(T_PANEL, 16, concreteD).bev(0.8f);
    bx(m, 38, 60, 47.6f, 48.4f, 31.6f, 35.2f, dark).m(MATTE).bev(0);
    bx(m, 70, 96, 47.6f, 48.4f, 31.6f, 35.2f, dark).m(MATTE).bev(0);
    bx(m, 106, 124, 47.6f, 48.4f, 31.6f, 35.2f, dark).m(MATTE).bev(0);
    m.slab(40, 30, 10, 8, zh(30), 1.6f, earth).m(MATTE).t(T_NOISE, 4, moss).bev(0.3f);
    m.cyl('z', 92, 30, zh(30) + 6.43f, 6.2f, 6.43f, concreteL).t(T_PANEL, 4, concreteD).bev(0.4f);
    m.cyl('z', 92, 30, zh(30) + 13.0f, 6.6f, 0.35f, concreteD).bev(0.3f);
    bx(m, 89.8f, 94.2f, 35.9f, 36.6f, 45.5f, 47.5f, glass).m(GLASS).bev(0.1f);
    m.rod({64, 28, zh(30)}, {64, 28, 72}, 0.5f, steelL).m(METAL).bev(0);
    m.ball(64, 28, 72.6f, 1.0f, 1.0f, 1.0f, red).m(EMIT);
    m.rod({50, 40, zh(30)}, {50, 40, zh(30) + 5.7f}, 0.5f, steelL).m(METAL).bev(0);
    bx(m, 48.5f, 51.5f, 39.5f, 40.5f, zh(30) + 5.7f, zh(30) + 7.4f, dark).m(MATTE).bev(0.2f);
    m.cyl('z', 112, 24, zh(30) + 2.9f, 3.4f, 2.9f, concreteL).t(T_PANEL, 3, concreteD).bev(0.3f);
    m.rod({40, 26, zh(30)}, {40, 26, 66}, 0.5f, steelL).m(METAL).bev(0);
    bx(m, 40.4f, 51.4f, 25.8f, 26.2f, 59, 66, OWNER).bev(0.2f);

    // the main casemate: slits on its south face, a blast door on a ramp with hazard-striped frame, a camouflage net and earth on the roof
    bx(m, 14, 146, 50, 76, 0, zh(18), concrete).t(T_PANEL, 16, concreteD).bev(0.8f);
    bx(m, 22, 54, 75.6f, 76.4f, 14.6f, 18.2f, dark).m(MATTE).bev(0);
    bx(m, 94, 128, 75.6f, 76.4f, 14.6f, 18.2f, dark).m(MATTE).bev(0);
    m.slab(75, 80.3f, 18, 3.7f, 0, 0.9f, concreteL).m(MATTE).t(T_PANEL, 8, concreteD).bev(0.3f);
    bx(m, 67, 83, 75.6f, 76.8f, 0.9f, 16.4f, OWNER).bev(0.2f);                                           // the owner's blast door
    bx(m, 65, 67, 76.0f, 77.0f, 0.9f, 17.5f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
    bx(m, 83, 85, 76.0f, 77.0f, 0.9f, 17.5f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
    bx(m, 65, 85, 76.0f, 77.0f, 16.4f, 17.5f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
    m.slab(106, 62, 20, 12, zh(18), 0.8f, rubber).m(MATTE).t(T_CAMO, 3, C(0x7E7E5C)).bev(0.2f);
    m.slab(134, 62, 8, 9, zh(18), 1.4f, earth).m(MATTE).t(T_NOISE, 4, moss).bev(0.3f);
    m.cyl('z', 26, 64, zh(18) + 4.3f, 3.6f, 4.3f, concreteL).t(T_PANEL, 3, concreteD).bev(0.3f);
    m.cyl('z', 62, 70, zh(18) + 5.0f, 4.2f, 5.0f, concreteL).t(T_PANEL, 3, concreteD).bev(0.3f);

    // the front casemate: slits on its south face, two blast doors with hazard frames, a team band, a vent, bastions at both ends
    bx(m, 4, 156, 96, 120, 0, zh(14), concrete).t(T_PANEL, 16, concreteD).bev(0.8f);
    bx(m, 6, 16, 119.6f, 120.4f, 10.1f, 13.9f, dark).m(MATTE).bev(0);
    bx(m, 52, 80, 119.6f, 120.4f, 10.1f, 13.9f, dark).m(MATTE).bev(0);
    bx(m, 86, 108, 119.6f, 120.4f, 10.1f, 13.9f, dark).m(MATTE).bev(0);
    bx(m, 140, 152, 119.6f, 120.4f, 10.1f, 13.9f, dark).m(MATTE).bev(0);
    bx(m, 60, 94, 119.9f, 120.5f, 2.1f, 6.4f, OWNER).bev(0.1f);
    for (float x0 : {22.0f, 114.0f}) {
        bx(m, x0 + 0.6f, x0 + 20.8f, 119.6f, 120.8f, 1.1f, 15.7f, OWNER).bev(0.2f);                         // the owner's blast door
        bx(m, x0 - 3, x0, 119.8f, 121.2f, 1.1f, 16.4f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
        bx(m, x0 + 20, x0 + 23, 119.8f, 121.2f, 1.1f, 16.4f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
        bx(m, x0 - 3, x0 + 23, 119.8f, 121.2f, 15.7f, 16.4f, hazard).t(T_HAZARD, 2.0f, hazardD).bev(0.1f);
    }
    m.cyl('z', 80, 112, zh(14) + 2.86f, 3.0f, 2.86f, concreteL).t(T_PANEL, 3, concreteD).bev(0.3f);
    for (float x : {6.0f, 150.0f}) {
        const float bx0 = x == 6.0f ? 0.0f : 144.0f;
        m.taper(bx0 + 6, 106, 0, 6, 12, 34, 4.5f, 10, concreteD).m(MATTE).t(T_PANEL, 8, concrete).bev(0.6f);
        bx(m, bx0 + 2.5f, bx0 + 4.5f, 116.9f, 118.4f, 14, 18, dark).m(MATTE).bev(0);
        m.rod({x, 106, 34}, {x, 106, 50}, 0.5f, steelL).m(METAL).bev(0);
        bx(m, x + 0.4f, x + 8.4f, 105.8f, 106.2f, 44, 50, OWNER).bev(0.2f);
    }

    // two roof gun pits: a concrete lip, a dark floor, a twin gun on a round mount, sandbags on the south rim
    auto gunPit = [&](float cx, float cy) {
        m.cyl('z', cx, cy, 20.3f, 8.8f, 0.3f, concreteL).t(T_PANEL, 4, concreteD).bev(0.2f);
        m.cyl('z', cx, cy, 20.65f, 7.0f, 0.05f, dark).m(MATTE).bev(0);
        m.cyl('z', cx, cy, 21.2f, 1.8f, 0.6f, steel).m(METAL).bev(0.2f);
        for (int s = -1; s <= 1; s += 2) {
            const float ox = s * 1.1f, oy = s * 1.2f;
            m.rod({cx + ox, cy + oy, 21.6f}, {cx + ox + 7.3f, cy + oy - 6.8f, 24.3f}, 0.5f, steelL).m(METAL).bev(0);
        }
        for (int k = 0; k < 4; k++) {
            const float an = 0.6f + k * 0.75f;
            m.box(cx + std::cos(an) * 11.5f, cy + std::sin(an) * 11.5f, 21.1f, 2.1f, 1.5f, 1.1f, sand).t(T_NOISE, 3, sandD).bev(0.8f).rz(an * 57.2958f);
        }
    };
    gunPit(36, 106);
    gunPit(124, 106);

    // the apron in front of the complex, with its hazard-free concrete face
    m.slab(80, 124, 80, 4, 0, 1.1f, concreteL).m(MATTE).t(T_PANEL, 10, concreteD).bev(0.3f);
}

// ---------------------------------------------------------------- Island Nuke Launcher, 3 x 3 tiles
// An armoured octagonal base with a missile standing in its erector cradle, six emitter pylons on a ring with a laser cage between their tips,
// a gantry tower at the west edge, a control bunker at the south-east corner, two cables and hazard-striped pad edges.
static void nIsland(Model& m, int) {
    using namespace nt;
    const float PH = zh(16);   // pylon height
    m.dirt = 0.1f;
    m.slab(48, 48, 48, 48, 0, 0.9f, concreteD).m(MATTE).t(T_PANEL, 16, concrete).bev(0.4f);                 // the pad
    bx(m, 3, 93, 3, 5.6f, 0.9f, 1.4f, hazard).t(T_HAZARD, 2.2f, hazardD).bev(0.1f);
    bx(m, 3, 93, 90.4f, 93, 0.9f, 1.4f, hazard).t(T_HAZARD, 2.2f, hazardD).bev(0.1f);
    bx(m, 3, 5.6f, 5.6f, 90.4f, 0.9f, 1.4f, hazard).t(T_HAZARD, 2.2f, hazardD).bev(0.1f);
    bx(m, 90.4f, 93, 5.6f, 90.4f, 0.9f, 1.4f, hazard).t(T_HAZARD, 2.2f, hazardD).bev(0.1f);
    bx(m, 7, 18, 8, 14, 0.9f, 1.4f, steelL).t(T_GRILLE, 1.2f, steelD).bev(0.1f);

    // the octagonal base: concrete walls with a steel band on each face that looks south, a plated roof, two vent grilles
    std::vector<P2> oct, ring;
    for (int k = 0; k < 8; k++) {
        const float an = 0.3927f + k * 0.7854f;
        oct.push_back({ 48 + std::cos(an) * 31, 50 + std::sin(an) * 31 });
        ring.push_back({ 48 + std::cos(an) * 24, 50 + std::sin(an) * 24 });
    }
    m.plan(oct, 0, zh(9), concrete).m(MATTE).t(T_PANEL, 8, concreteD).bev(0.4f);
    m.plan(ring, zh(9), zh(9) + 0.6f, steelD).m(METAL).bev(0.3f);
    std::vector<P2> collar;                                   // the owner's collar round the base, a little proud of the walls
    for (int k = 0; k < 8; k++) {
        const float an = 0.3927f + k * 0.7854f;
        collar.push_back({ 48 + std::cos(an) * 31.6f, 50 + std::sin(an) * 31.6f });
    }
    m.plan(collar, 0.9f, 6.5f, OWNER).m(MATTE).bev(0.2f);
    for (int k = 0; k < 3; k++) {
        const float th = (45.0f + 45.0f * k) * 0.0174533f;   // the outward normal of the face
        const float r = 28.6f + 0.5f;
        m.box(48 + std::cos(th) * r, 50 + std::sin(th) * r, 8.8f, 11.85f, 0.45f, 0.45f, steel).rz(th * 57.2958f + 90.0f).m(METAL).bev(0.1f);
    }
    bx(m, 26, 34, 46, 51, 13.0f, 14.1f, steelL).t(T_GRILLE, 1.0f, steelD).bev(0.2f);
    bx(m, 62, 70, 46, 51, 13.0f, 14.1f, steelL).t(T_GRILLE, 1.0f, steelD).bev(0.2f);

    // the erector cradle, arms and the missile standing in it: white body with a red band, a nose cone and four fins
    bx(m, 42, 54, 49, 51, zh(9) + 0.5f, 14.3f, steelL).m(METAL).bev(0.2f);
    m.rod({43.6f, 50, 13.4f}, {41.6f, 50, 33}, 0.9f, steel).m(METAL).bev(0);
    m.rod({52.4f, 50, 13.4f}, {54.4f, 50, 33}, 0.9f, steel).m(METAL).bev(0);
    m.cyl('z', 48, 50, 32.85f, 2.7f, 18.55f, white).bev(0.4f);
    m.cyl('z', 48, 50, 37.85f, 2.9f, 2.15f, redD).bev(0.2f);
    m.cyl('z', 48, 50, 58.55f, 2.7f, 7.15f, white, 0).bev(0.3f);
    m.ball(48, 50, 66.0f, 0.8f, 0.8f, 0.8f, red).m(EMIT);
    m.profileY({ { 46, 24.3f }, { 41.8f, 15.7f }, { 46, 17.1f } }, 49.3f, 50.7f, steel).m(METAL).bev(0.1f);
    m.profileY({ { 50, 24.3f }, { 54.2f, 15.7f }, { 50, 17.1f } }, 49.3f, 50.7f, steel).m(METAL).bev(0.1f);
    m.profileX({ { 46, 24.3f }, { 41.8f, 15.7f }, { 46, 17.1f } }, 47.3f, 48.7f, steel).m(METAL).bev(0.1f);
    m.profileX({ { 54, 24.3f }, { 58.2f, 15.7f }, { 54, 17.1f } }, 47.3f, 48.7f, steel).m(METAL).bev(0.1f);
    m.rod({48, 88, 0.6f}, {48, 79.5f, 12.5f}, 0.5f, rubber).m(RUBBER).bev(0);            // cables from the pylons
    m.rod({15.1f, 31, 0.6f}, {21, 39, 12.5f}, 0.5f, rubber).m(RUBBER).bev(0);

    // six emitter pylons on a ring of radius 38, with red tips and a laser cage between them
    float tx[6], ty[6];
    for (int i = 0; i < 6; i++) {
        const float an = -1.5708f + i * 1.0472f;
        tx[i] = 48 + std::cos(an) * 38;
        ty[i] = 50 + std::sin(an) * 38;
        m.cyl('z', tx[i], ty[i], PH * 0.5f, 3.2f, PH * 0.5f, steel).m(METAL).bev(0.4f);
        m.cyl('z', tx[i], ty[i], PH + 0.3f, 3.4f, 0.3f, steelD).bev(0.2f);
        m.ball(tx[i], ty[i], PH + 1.0f, 1.9f, 1.9f, 1.9f, red).m(EMIT);
    }
    for (int i = 0; i < 6; i++) {
        const int j = (i + 1) % 6;
        m.rod({tx[i], ty[i], PH + 1.0f}, {tx[j], ty[j], PH + 1.0f}, 0.45f, red).m(EMIT).bev(0);
    }

    // the gantry tower on the west margin: four legs, two braced levels and a platform
    const float GH = 40.0f;
    for (float x : {5.0f, 13.0f}) for (float y : {39.0f, 53.0f}) m.rod({x, y, 0.5f}, {x, y, GH}, 0.8f, steel).m(METAL).bev(0);
    for (float z : {12.0f, 26.0f}) {
        m.rod({5, 39, z}, {13, 39, z}, 0.5f, steelL).m(METAL).bev(0);
        m.rod({5, 53, z}, {13, 53, z}, 0.5f, steelL).m(METAL).bev(0);
        m.rod({5, 39, z}, {5, 53, z}, 0.5f, steelL).m(METAL).bev(0);
        m.rod({13, 39, z}, {13, 53, z}, 0.5f, steelL).m(METAL).bev(0);
    }
    m.rod({5, 39, 12}, {13, 53, 26}, 0.4f, steelD).m(METAL).bev(0);
    bx(m, 4, 14, 38, 54, GH, GH + 1.2f, steelL).m(METAL).t(T_GRILLE, 2, steelD).bev(0.3f);
    m.rod({9, 46, GH + 1.2f}, {9, 46, GH + 13}, 0.4f, steelL).m(METAL).bev(0);                             // the owner's flag on the gantry
    bx(m, 9.4f, 19.4f, 45.8f, 46.2f, GH + 6, GH + 12, OWNER).bev(0.2f);

    // the control bunker at the south-east corner: a sloped block with a window, a steel door and an antenna
    m.taper(87, 86, 0, 6, 5, 8, 5.4f, 4.4f, concreteD).m(MATTE).t(T_PANEL, 6, concrete).bev(0.6f);
    bx(m, 84.5f, 89.5f, 90.2f, 91.4f, 5.0f, 6.4f, glass).m(GLASS).bev(0.1f);
    bx(m, 90.2f, 92.2f, 90.4f, 91.4f, 0, 4.5f, steelD).m(METAL).bev(0.1f);
    m.rod({84, 84, 8}, {84, 84, 16}, 0.4f, steelL).m(METAL).bev(0);
}

M3D_BUILD(B_C_CASTLE, nCastle)
M3D_BUILD(B_C_CIVBUNKER, nBunker)
M3D_BUILD(B_C_WATCHTOWER, nWatch)
M3D_BUILD(B_C_COTTAGE, nCottage)
M3D_BUILD(B_C_WORKSHOP, nWorkshop)
M3D_BUILD(B_C_FORTRESS, nFortress)
M3D_BUILD(B_C_ISLANDNUKE, nIsland)
