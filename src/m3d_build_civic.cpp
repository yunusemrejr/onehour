// One Hour - 3D models: the nine civilian town buildings in both armies' styles (see m3d.h / m3d_reg.h).
// Cyber town: pale concrete, big glass fronts, flat roofs with roof gardens, solar panels and AC units, cobalt trim, cool light.
// Clanker town: brick and plaster, pitched tile roofs, chimneys, wooden doors, striped awnings, warm lit windows.
// Each kind is one function with a style flag. Heights come from the old painters' lifts (z = lift / 0.7) except the skyscraper, kept to 90.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// Cyber town
const Color cWall = C(0xDCE3EA), cWallD = C(0xAFBBC9), cConc = C(0xC3CDD8), cGlassB = C(0x3F8DB8), cGlassD = C(0x163C52),
            cCob = C(0x2C5AA0), cCobL = C(0x4F86C8), cPave = C(0xB7C2CE), cSteel = C(0x8E9AA8), cLight = C(0x58ECFF),
            cLawn = C(0x6FA488), cLawnD = C(0x4F7E68), cLeaf = C(0x4E9C7A), cAsph = C(0x4B5566), cWood = C(0xA0785A), cCar = C(0xE6ECF2);
// Clanker town
const Color kBrick = C(0x8F5A41), kMort = C(0xC9BBA0), kTile = C(0x9A5638), kTileD = C(0x6E3A28), kPlaster = C(0xD8CBAA), kStone = C(0xB4AA92),
            kStoneD = C(0x8C8470), kWood = C(0x6B4A2E), kWoodL = C(0x9A7048), kSand = C(0xC2AE80), kAsph = C(0x5B5A52), kLawn = C(0x7C8A4C),
            kLawnD = C(0x5E6A3A), kLeaf = C(0x6E7A3A), kWin = C(0x3A3024), kWarm = C(0xFFC766), kIron = C(0x3A3C3C), kCopper = C(0x6FA08A),
            kRust = C(0x9A5A34), kRoofK = C(0x8E866C), kRed = C(0xB8483A), kCar = C(0x6F7476);
const Color METALC = C(0x3A3E46), TRUNK = C(0x5A4632), LINE = C(0xE8ECEF), DARK = C(0x2A2D33);

// a box from its plan rectangle (x0..x1, y0..y1) and its height range (z0..z1)
Prim& bx(Model& m, float x0, float x1, float y0, float y1, float z0, float z1, Color c) {
    return m.box((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f, (z1 - z0) * 0.5f, c);
}
// the footprint's ground: a thin slab over the whole lot
void pad(Model& m, float W, float H, Color c, Tex tx, Color c2, float s) {
    m.slab(W * 0.5f, H * 0.5f, W * 0.5f, H * 0.5f, 0, 0.6f, c).m(MATTE).t(tx, s, c2).bev(0.2f);
}
// a tree: a trunk and a ball of leaves
void tree(Model& m, float x, float y, float trunk, float r, Color leaf) {
    m.rod({ x, y, 0.5f }, { x, y, trunk }, 0.7f, TRUNK).m(MATTE).bev(0);
    m.ball(x, y, trunk + r * 0.7f, r, r, r * 0.8f, leaf).m(FOLIAGE);
}
// a street lamp with a lit head
void lamp(Model& m, float x, float y, float h, Color lit) {
    m.cyl('z', x, y, h * 0.5f, 0.45f, h * 0.5f, METALC).m(METAL).bev(0.2f);
    m.ball(x, y, h + 0.9f, 1.0f, 1.0f, 1.0f, lit).m(EMIT);
}
// a parked car, long axis east-west
void car(Model& m, float x, float y, Color body) {
    bx(m, x, x + 12, y, y + 6, 0.6f, 4.4f, body).bev(0.6f);
    bx(m, x + 3, x + 9.5f, y + 0.5f, y + 5.5f, 4.4f, 7.2f, C(0x1E2A36)).m(GLASS).bev(0.5f);
}
// a park bench with two legs
void bench(Model& m, float x, float y, float len, Color wood) {
    bx(m, x, x + len, y, y + 3.2f, 2.4f, 3.2f, wood).m(MATTE).t(T_PLANKS, 2.5f, TRUNK).bev(0.2f);
    bx(m, x + 1, x + 2, y + 0.4f, y + 2.8f, 0.6f, 2.4f, METALC).m(METAL).bev(0.1f);
    bx(m, x + len - 2, x + len - 1, y + 0.4f, y + 2.8f, 0.6f, 2.4f, METALC).m(METAL).bev(0.1f);
}
// a low fence along x or along y: two rails and posts every 8 units
void fenceX(Model& m, float x0, float x1, float y, float h, Color c) {
    bx(m, x0, x1, y - 0.3f, y + 0.3f, h * 0.35f, h * 0.45f, c).bev(0.05f);
    bx(m, x0, x1, y - 0.3f, y + 0.3f, h * 0.8f, h * 0.9f, c).bev(0.05f);
    for (float x = x0; x <= x1 + 0.01f; x += 8.0f) bx(m, x - 0.45f, x + 0.45f, y - 0.45f, y + 0.45f, 0, h, c).bev(0.1f);
}
void fenceY(Model& m, float x, float y0, float y1, float h, Color c) {
    bx(m, x - 0.3f, x + 0.3f, y0, y1, h * 0.35f, h * 0.45f, c).bev(0.05f);
    bx(m, x - 0.3f, x + 0.3f, y0, y1, h * 0.8f, h * 0.9f, c).bev(0.05f);
    for (float y = y0; y <= y1 + 0.01f; y += 8.0f) bx(m, x - 0.45f, x + 0.45f, y - 0.45f, y + 0.45f, 0, h, c).bev(0.1f);
}
// a flagpole with the owner's flag at its top (the cloth runs east from the pole, in the owner's colour)
void flag(Model& m, float x, float y, float h, float fw, float fh, Color pole) {
    m.cyl('z', x, y, h * 0.5f, 0.5f, h * 0.5f, pole).m(METAL).bev(0.2f);
    bx(m, x + 0.5f, x + 0.5f + fw, y - 0.2f, y + 0.2f, h - fh - 0.5f, h - 0.5f, OWNER).bev(0.1f);
}
// a striped awning slanting down to the front (y1) from the wall line (y0); its top edge at height z
void awningX(Model& m, float x0, float x1, float y0, float y1, float z, Color a, Color b, int n) {
    const float pw = (x1 - x0) / n;
    for (int i = 0; i < n; i++)
        m.profileX({ { y0, z }, { y1, z - 2.6f }, { y1, z - 3.6f }, { y0, z - 1.0f } }, x0 + i * pw, x0 + (i + 1) * pw, (i & 1) ? b : a).bev(0.2f);
}
// a glass or lit pane standing just proud of a south face (yf)
void pane(Model& m, float x0, float x1, float yf, float z0, float z1, Color c, Mat mt) {
    bx(m, x0, x1, yf - 0.1f, yf + 0.3f, z0, z1, c).m(mt).bev(0.2f);
}
// a solar panel tilted about y by deg, lying on a roof at height z
void solar(Model& m, float x, float y, float z, float hx, float hy, float deg) {
    m.box(x, y, z, hx, hy, 0.3f, cGlassD).m(GLASS).ry(deg).bev(0.1f);
}
// a rooftop box (AC unit, vent) from plan corners
void unit(Model& m, float x0, float x1, float y0, float y1, float z0, float z1) {
    bx(m, x0, x1, y0, y1, z0, z1, cSteel).m(METAL).bev(0.4f);
}

// ================================================================= HOUSE (2 x 2)
// A two-storey family home on a lawn with a drive, a car and a fence. Cyber: a pale box with a glass front, an upper floor that
// reaches over the drive, a roof garden and solar panels. Clanker: brick walls under a tiled gable, a chimney and a wooden door under an awning.
void house(Model& m, bool cy) {
    const Color walk = cy ? cPave : kStone, fenceC = cy ? cWall : kWood;
    pad(m, 64, 64, cy ? cLawn : kLawn, T_NOISE, cy ? cLawnD : kLawnD, 8);
    bx(m, 29, 35, 50, 62, 0.6f, 1.0f, walk).m(MATTE).bev(0.1f);          // path to the gate
    bx(m, 37, 58, 52, 63, 0.6f, 0.9f, walk).m(MATTE).bev(0.1f);          // the drive
    car(m, 40, 54, cy ? cCar : kCar);
    fenceX(m, 3, 27, 62, 4.0f, fenceC); fenceX(m, 37, 61, 62, 4.0f, fenceC);
    fenceY(m, 3, 40, 62, 4.0f, fenceC); fenceY(m, 61, 40, 62, 4.0f, fenceC);
    tree(m, 13, 57, 6.5f, 5.2f, cy ? cLeaf : kLeaf);
    if (cy) {
        bx(m, 6, 58, 22, 50, 0.6f, 12, cWall).t(T_PANEL, 6, cWallD).bev(0.4f);     // ground floor, pale concrete
        pane(m, 12, 52, 50, 1.4f, 10.6f, cGlassD, GLASS);                          // the big glass front
        bx(m, 30, 36, 50, 50.6f, 0.6f, 9.0f, cCob).bev(0.2f);                       // the door
        bx(m, 6, 58, 22, 50, 12, 21, cWall).t(T_PANEL, 6, cWallD).bev(0.4f);       // upper floor
        pane(m, 10, 54, 50, 13.5f, 18.5f, cGlassB, GLASS);                         // a ribbon window
        bx(m, 4, 60, 20, 52, 21, 22.2f, cConc).bev(0.4f);                           // roof slab with a little overhang
        bx(m, 8, 46, 33, 39, 22.2f, 22.6f, OWNER).bev(0.1f);                       // team strip on the roof
        bx(m, 6, 58, 50, 50.6f, 12, 12.8f, OWNER).bev(0.1f);                       // team band between the floors
        bx(m, 10, 30, 26, 34, 22.2f, 23.4f, cWallD).bev(0.2f);                     // roof garden planter
        for (float x : { 14.0f, 20.0f, 26.0f }) m.ball(x, 30, 24.2f, 2.3f, 2.3f, 1.6f, cLeaf).m(FOLIAGE);
        solar(m, 44, 30, 24.2f, 6.5f, 3.5f, -14); solar(m, 44, 41, 24.2f, 6.5f, 3.5f, -14);
        unit(m, 52, 57, 26, 32, 22.2f, 24.0f);                                      // AC unit
        bx(m, 25, 39, 50, 53.5f, 9.6f, 10.6f, OWNER).bev(0.2f);                     // the door canopy in the team colour
        m.ball(23, 50.6f, 8.2f, 0.6f, 0.6f, 0.6f, cLight).m(EMIT);                  // door light
    } else {
        bx(m, 6, 58, 22, 50, 0.6f, 20.0f, kBrick).t(T_BRICK, 4, kMort).bev(0.4f);  // two storeys of brick
        m.profileX({ { 20.5f, 20.0f }, { 36.0f, 30.5f }, { 51.5f, 20.0f } }, 4, 60, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // tiled gable, ridge east-west
        bx(m, 41, 46, 25, 30, 14, 35, kBrick).t(T_BRICK, 4, kMort).bev(0.3f);      // chimney
        bx(m, 40.5f, 46.5f, 24.5f, 30.5f, 35, 36, kStone).bev(0.3f);               // chimney cap
        bx(m, 5, 59, 50, 51.4f, 18.6f, 20.0f, OWNER).bev(0.2f);                    // fascia board in the team colour
        bx(m, 5, 59, 35, 37, 30.5f, 31.3f, OWNER).bev(0.2f);                       // team ridge cap
        bx(m, 27.5f, 36.5f, 50, 50.6f, 0.6f, 11, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // wooden door
        m.profileX({ { 50.0f, 12.6f }, { 54.5f, 10.6f }, { 54.5f, 9.8f }, { 50.0f, 11.6f } }, 25, 39, OWNER).bev(0.2f);   // awning
        for (float x : { 10.0f, 46.0f }) {
            pane(m, x, x + 8, 50, 12.0f, 17.5f, kWarm, GLASS);                       // upper windows, lit
            pane(m, x, x + 8, 50, 3.0f, 9.0f, kWarm, GLASS);                         // lower windows, lit
        }
        m.ball(24, 50.6f, 8.0f, 0.6f, 0.6f, 0.6f, kWarm).m(EMIT);                  // lantern by the door
    }
}

// ================================================================= APARTMENTS (3 x 2)
// A five-storey block on a car park. Cyber: a flat pale block with blue glass bands and cobalt balconies, AC units and solar panels on
// the roof. Clanker: a brick block under a long tiled gable, chimneys, an iron fire escape and warm windows.
void apartments(Model& m, bool cy) {
    pad(m, 96, 64, cy ? cAsph : kAsph, T_PANEL, cy ? cAsph : kAsph, 12);
    car(m, 4, 57, cy ? cCar : kCar); car(m, 20, 57, cy ? cCobL : kIron); car(m, 74, 57, cy ? cCar : kRust);
    if (cy) {
        bx(m, 6, 90, 22, 56, 0.6f, 43, cWall).t(T_WINDOWS, 8.6f, cGlassB).bev(0.5f);   // five storeys, pale concrete
        for (int k = 1; k < 5; k++) bx(m, 8, 88, 55.4f, 56.6f, 8.6f * k, 8.6f * k + 0.6f, cCob).bev(0.1f);   // balcony slabs
        for (int k = 1; k < 5; k++) bx(m, 40, 46, 55.4f, 56.4f, 8.6f * k + 0.6f, 8.6f * k + 1.4f, cLeaf).m(FOLIAGE);   // balcony planters
        bx(m, 6, 90, 22, 23, 43, 44.4f, cWall).bev(0.2f);                           // parapets
        bx(m, 6, 90, 55, 56, 43, 44.4f, cWall).bev(0.2f);
        bx(m, 6, 7, 22, 56, 43, 44.4f, cWall).bev(0.2f);
        bx(m, 89, 90, 22, 56, 43, 44.4f, cWall).bev(0.2f);
        unit(m, 10, 22, 28, 34, 43.4f, 46.0f); unit(m, 26, 38, 28, 34, 43.4f, 46.0f);
        unit(m, 50, 60, 46, 52, 43.4f, 46.0f); unit(m, 66, 76, 46, 52, 43.4f, 46.0f);   // more AC units
        bx(m, 80, 88, 44, 52, 43.4f, 48.0f, cWall).bev(0.3f);                        // stair housing
        bx(m, 8, 88, 36, 42, 43.4f, 43.8f, OWNER).bev(0.1f);                         // team strip across the roof
        solar(m, 46, 30, 44.6f, 9, 4, -12); solar(m, 66, 30, 44.6f, 9, 4, -12);
        bx(m, 8, 88, 44.4f, 44.8f, 43.4f, 43.8f, OWNER).bev(0.1f);                // team band on the parapet
        pane(m, 36, 60, 56, 0.6f, 7.0f, cGlassD, GLASS);                            // lobby glass
        bx(m, 32, 64, 56, 59, 7.4f, 8.4f, OWNER).bev(0.2f);                          // door canopy
    } else {
        bx(m, 6, 90, 22, 56, 0.6f, 43, kBrick).t(T_WINDOWS, 8.6f, kWin).bev(0.5f);
        m.profileX({ { 20.0f, 43.0f }, { 39.0f, 52.4f }, { 58.0f, 43.0f } }, 4, 92, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // long tiled gable
        for (float x : { 24.0f, 70.0f }) {
            bx(m, x, x + 5, 30, 35, 43, 53, kBrick).t(T_BRICK, 4, kMort).bev(0.3f);   // chimneys
            bx(m, x - 0.5f, x + 5.5f, 29.5f, 35.5f, 53, 54, kStone).bev(0.3f);
        }
        for (int k = 1; k < 5; k++) bx(m, 64, 86, 55.4f, 57.4f, 8.6f * k, 8.6f * k + 0.4f, kIron).m(METAL).bev(0.1f);   // fire escape landings
        m.rod({ 84, 57.8f, 1 }, { 84, 57.8f, 42 }, 0.3f, kIron).m(METAL).bev(0);
        m.rod({ 70, 57.8f, 1 }, { 70, 57.8f, 42 }, 0.3f, kIron).m(METAL).bev(0);
        for (int k = 1; k < 5; k++) {
            bx(m, 64, 86, 57.4f, 57.9f, 8.6f * k + 0.4f, 8.6f * k + 1.2f, kIron).m(METAL).bev(0.1f);   // landing rails
            pane(m, 66, 72, 56, 8.6f * k + 1.4f, 8.6f * k + 6.4f, kWarm, GLASS);                         // lit windows
        }
        bx(m, 4, 92, 38.2f, 39.8f, 51.8f, 52.6f, OWNER).bev(0.1f);                                       // team ridge
        tree(m, 93, 40, 6, 4, kLeaf);
        bx(m, 40, 56, 56, 56.5f, 0.6f, 9.5f, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);    // lobby door
        m.profileX({ { 56.0f, 11.0f }, { 60.0f, 9.0f }, { 60.0f, 8.0f }, { 56.0f, 10.0f } }, 34, 62, OWNER).bev(0.2f);   // awning
    }
}

// ================================================================= SKYSCRAPER (3 x 3)
// A stepped tower on a podium: pale podium, a middle glass block, a crown and an antenna. Cyber: glass with white fins and a helipad.
// Clanker: brick and plaster stepped blocks, with a copper hip roof over the crown and a spire.
void skyscraper(Model& m, bool cy) {
    pad(m, 96, 96, cy ? cPave : kSand, T_PANEL, cy ? cSteel : kStoneD, 16);
    tree(m, 8, 90, 6, 4.4f, cy ? cLeaf : kLeaf); tree(m, 88, 90, 6, 4.4f, cy ? cLeaf : kLeaf);
    bench(m, 26, 90, 12, cy ? cWood : kWood); bench(m, 58, 90, 12, cy ? cWood : kWood);
    lamp(m, 44, 94, 12, cy ? cLight : kWarm);
    if (cy) {
        bx(m, 6, 90, 66, 82, 0.6f, 26, cWall).t(T_PANEL, 6, cWallD).bev(0.4f);     // podium
        bx(m, 7, 17, 67, 79, 26, 26.6f, OWNER).bev(0.1f);                           // team strips on the podium roof
        bx(m, 79, 89, 67, 79, 26, 26.6f, OWNER).bev(0.1f);
        bx(m, 18, 78, 66, 82, 26, 52, cGlassB).m(GLASS).bev(0.6f);                  // middle: glass
        for (float x = 22; x < 78; x += 8) bx(m, x, x + 0.8f, 82, 82.9f, 26, 52, cWall).bev(0.2f);   // fins
        bx(m, 28, 68, 66, 82, 52, 80, cGlassB).m(GLASS).bev(0.6f);                  // crown: glass
        for (float x = 32; x < 66; x += 8) bx(m, x, x + 0.8f, 82, 82.9f, 52, 80, cWall).bev(0.2f);
        bx(m, 26, 70, 64, 84, 80, 81.2f, cWall).bev(0.4f);                          // crown cap
        bx(m, 30, 66, 82, 82.6f, 70, 73, OWNER).bev(0.1f);                          // team band across the crown
        m.cyl('z', 48, 74, 81.2f, 5.8f, 0.3f, cCobL).bev(0.2f);                     // helipad
        m.cyl('z', 48, 74, 81.6f, 4.4f, 0.1f, cCob).bev(0.1f);
        m.rod({ 48, 74, 81.5f }, { 48, 74, 90 }, 0.5f, cSteel).m(METAL).bev(0);     // antenna
        m.ball(48, 74, 90.6f, 0.9f, 0.9f, 0.9f, cLight).m(EMIT);
    } else {
        bx(m, 6, 90, 66, 82, 0.6f, 26, kPlaster).t(T_WINDOWS, 6, kWin).bev(0.5f);   // podium: plaster with tall windows
        bx(m, 7, 17, 67, 79, 26, 26.6f, OWNER).bev(0.1f);                           // team strips on the podium roof
        bx(m, 79, 89, 67, 79, 26, 26.6f, OWNER).bev(0.1f);
        bx(m, 18, 78, 66, 82, 26, 52, kBrick).t(T_WINDOWS, 6, kWin).bev(0.5f);      // middle: brick
        bx(m, 28, 68, 66, 82, 52, 70, kPlaster).t(T_WINDOWS, 6, kWin).bev(0.5f);    // crown
        bx(m, 70, 77, 68, 74, 52, 56, kPlaster).t(T_WINDOWS, 6, kWin).bev(0.3f);    // stair housing on the middle roof
        bx(m, 20, 25, 68, 74, 52, 54.5f, kIron).m(METAL).bev(0.4f);                   // rooftop unit
        bx(m, 20, 25, 78, 80, 52, 53.6f, kRust).m(METAL).bev(0.3f);                   // vent
        m.taper(48, 74, 70, 20, 8, 10, 5, 4, kCopper).t(T_TILES, 3, kTileD).bev(0.4f);   // copper hip roof
        m.rod({ 48, 74, 79.5f }, { 48, 74, 90 }, 0.5f, kRust).m(METAL).bev(0);      // spire
        m.ball(48, 74, 90.6f, 0.9f, 0.9f, 0.9f, kWarm).m(EMIT);
        awningX(m, 40, 56, 82, 86, 14, OWNER, kPlaster, 6);                          // entrance awning
        bx(m, 44, 52, 82, 82.6f, 0.6f, 10, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // entrance door
    }
}

// ================================================================= GAS STATION (3 x 2)
// Three pump islands under a canopy with a team fascia, a shop with a glass front and a tall price sign on a pole.
void gasStation(Model& m, bool cy) {
    pad(m, 96, 64, cy ? cAsph : kAsph, T_PANEL, cy ? cAsph : kAsph, 12);
    for (float x = 8; x < 60; x += 16) bx(m, x, x + 6, 58, 58.6f, 0.6f, 0.7f, LINE).bev(0);   // lane dashes
    if (cy) {
        bx(m, 72, 94, 4, 28, 0.6f, 17, cWall).t(T_PANEL, 6, cWallD).bev(0.4f);     // shop: pale walls
        bx(m, 71, 95, 3, 29, 17, 18, cConc).bev(0.3f);                              // roof slab
        bx(m, 76, 90, 8, 12, 18, 18.8f, OWNER).bev(0.1f);                           // team sign on the roof
        pane(m, 75, 91, 28, 1.4f, 11, cGlassD, GLASS);                              // shop front
        bx(m, 84, 88, 28, 28.6f, 0.6f, 9.5f, cCob).bev(0.2f);                       // door
    } else {
        bx(m, 72, 94, 4, 28, 0.6f, 17, kBrick).t(T_BRICK, 4, kMort).bev(0.4f);     // shop: brick
        m.profileX({ { 3.0f, 17.0f }, { 16.0f, 25.0f }, { 29.0f, 17.0f } }, 71, 95, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // pitched roof
        pane(m, 75, 82, 28, 5.0f, 12.0f, kWarm, GLASS); pane(m, 86, 92, 28, 5.0f, 12.0f, kWarm, GLASS);   // lit windows
        bx(m, 82, 86, 28, 28.6f, 0.6f, 10, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // door
        bx(m, 75, 91, 28, 28.5f, 13.0f, 15.0f, OWNER).bev(0.2f);                    // team board above the windows
    }
    // the canopy over the pumps, on four pillars
    if (cy) bx(m, 4, 70, 26, 44, 15, 16.4f, cConc).bev(0.4f);
    else bx(m, 4, 70, 26, 44, 15, 16.4f, kRoofK).t(T_RIBS, 2.6f, kStoneD).bev(0.4f);
    for (float x0 : { 5.0f, 64.0f }) for (float y0 : { 27.0f, 40.0f }) bx(m, x0, x0 + 3, y0, y0 + 3, 0.6f, 15, cy ? cSteel : kRust).bev(0.2f);
    bx(m, 4, 70, 43.2f, 44, 14.2f, 16.4f, OWNER).bev(0.2f);                          // team fascia
    bx(m, 4, 70, 26, 29, 16.4f, 16.9f, OWNER).bev(0.1f);                             // team stripes on the canopy top
    bx(m, 4, 70, 41, 44, 16.4f, 16.9f, OWNER).bev(0.1f);
    for (float x : { 10.0f, 36.0f, 62.0f }) {
        bx(m, x, x + 14, 36, 42, 0.6f, 1.4f, cy ? cConc : kStoneD).bev(0.3f);        // island
        bx(m, x + 4, x + 10, 30.5f, 35.5f, 1.4f, 10.5f, cy ? cWall : kRed).bev(0.6f);   // pump
        bx(m, x + 5, x + 9, 35.5f, 35.9f, 6.5f, 8.5f, cy ? C(0x58FFB0) : kWarm).m(EMIT);   // display
        bx(m, x + 4, x + 10, 30.5f, 35.5f, 10.5f, 11.2f, OWNER).bev(0.1f);            // owner cap on the pump
        m.rod({ x + 7, 36.6f, 5.6f }, { x + 7, 45, 1.8f }, 0.3f, DARK).m(RUBBER).bev(0);   // hose
    }
    car(m, 8, 47, cy ? cCar : kCar); car(m, 36, 47, cy ? cCobL : kIron);
    bx(m, 86.2f, 87.8f, 40, 41.6f, 0.6f, 22, cy ? cSteel : kStoneD).m(METAL).bev(0.1f);   // price sign pole
    bx(m, 80, 94, 39.4f, 41.4f, 22, 34, cy ? cCob : kWood).bev(0.3f);                     // price sign
    bx(m, 83, 91, 41.4f, 41.8f, 25, 31, cy ? cLight : kWarm).m(EMIT).bev(0.1f);            // price digits
}

// ================================================================= GROCERY STORE (3 x 2)
// A shop front with a big glass window, a sign board on the roof edge, an awning, parking and carts.
void grocery(Model& m, bool cy) {
    pad(m, 96, 64, cy ? cAsph : kAsph, T_PANEL, cy ? cAsph : kAsph, 12);
    car(m, 8, 57, cy ? cCar : kCar); car(m, 58, 57, cy ? cCobL : kRust);
    if (cy) {
        bx(m, 6, 90, 16, 50, 0.6f, 28.6f, cWall).t(T_PANEL, 6, cWallD).bev(0.4f);   // body
        bx(m, 4, 92, 14, 52, 28.6f, 29.6f, cConc).bev(0.4f);                         // roof slab
        bx(m, 10, 86, 48, 52, 29.6f, 34.6f, cCob).bev(0.3f);                         // sign board on the roof edge
        bx(m, 12, 30, 52, 52.6f, 30.8f, 33.4f, OWNER).bev(0.1f);                     // team badge on the sign
        bx(m, 36, 70, 52, 52.4f, 31.8f, 32.6f, cLight).m(EMIT).bev(0.1f);            // lit letters
        pane(m, 10, 86, 50, 1.2f, 14.0f, cGlassD, GLASS);                            // glass front
        for (float x : { 26.0f, 46.0f, 66.0f }) bx(m, x, x + 0.8f, 50, 50.4f, 1.2f, 14.0f, cWallD).bev(0.1f);   // mullions
        bx(m, 46, 52, 50, 50.6f, 0.6f, 11, cCob).bev(0.2f);                          // door
        awningX(m, 6, 90, 50, 56, 15, OWNER, cWall, 8);                              // striped awning
    } else {
        bx(m, 6, 90, 16, 50, 0.6f, 20, kBrick).t(T_BRICK, 4, kMort).bev(0.4f);       // brick walls
        m.profileX({ { 16.0f, 28.0f }, { 50.5f, 21.0f }, { 50.5f, 18.0f }, { 16.0f, 18.0f } }, 4, 92, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // shed roof, falling to the front
        pane(m, 12, 84, 50, 1.2f, 11.0f, kWarm, GLASS);                              // shop window, lit
        for (float x : { 26.0f, 36.0f, 66.0f, 76.0f }) bx(m, x, x + 1.0f, 50, 50.4f, 1.2f, 11.0f, kWood).bev(0.1f);   // window bars
        bx(m, 46, 52, 50, 50.6f, 0.6f, 11, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // wooden door
        bx(m, 12, 84, 50, 51.2f, 15.2f, 17.8f, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // sign board
        bx(m, 12, 84, 50, 51.4f, 17.8f, 18.5f, OWNER).bev(0.1f);                     // team fascia on the sign
        awningX(m, 6, 90, 50, 55, 14.6f, OWNER, kPlaster, 8);
    }
    bx(m, 80, 87, 58, 63, 0.6f, 3.6f, cy ? cSteel : kWood).m(cy ? METAL : MATTE).bev(0.2f);   // carts / crates
    bx(m, 82, 86, 62, 66, 0.6f, 2.4f, cy ? cSteel : kWood).m(cy ? METAL : MATTE).bev(0.2f);
    for (float x : { 16.0f, 28.0f, 40.0f, 64.0f, 76.0f, 88.0f }) bx(m, x - 0.5f, x + 0.5f, 51.5f, 52.5f, 0.6f, 1.4f, cy ? cCob : kIron).m(METAL).bev(0.1f);   // bollards
}

// ================================================================= EVENT HALL (3 x 3)
// A wide hall with a long roof and a marquee over the entrance, team banners on the walls and a plaza with a stage and string lights.
// Cyber: a curved shell roof of pale ceramic over a glass band. Clanker: a tiled gable over brick with warm windows and a striped canopy.
void eventHall(Model& m, bool cy) {
    pad(m, 96, 96, cy ? cPave : kSand, T_PANEL, cy ? cSteel : kStoneD, 16);
    tree(m, 92, 62, 6, 4, cy ? cLeaf : kLeaf);
    if (cy) {
        bx(m, 8, 88, 40, 80, 0.6f, 31.4f, cWall).t(T_PANEL, 8, cWallD).bev(0.4f);   // walls
        std::vector<P2> arc;                                                          // the curved shell roof: a half-round over y 40..80
        for (int i = 0; i <= 8; i++) {
            const float th = 3.14159265f * i / 8.0f;
            arc.push_back({ 60.0f - 20.0f * std::cos(th), 31.4f + 18.0f * std::sin(th) });
        }
        m.profileX(arc, 8, 88, cWall).bev(0.3f);
        pane(m, 22, 74, 80, 13.0f, 26.0f, cGlassB, GLASS);
        for (float x : { 12.0f, 30.0f, 66.0f, 84.0f }) bx(m, x - 0.8f, x + 0.8f, 79.6f, 80.6f, 0.6f, 30.0f, cWallD).bev(0.2f);   // pilasters
        bx(m, 8, 88, 58.5f, 61.5f, 48.9f, 49.9f, OWNER).bev(0.1f);                  // team ridge on the shell                            // the glass band
        bx(m, 34, 62, 80, 81.2f, 26.4f, 30.6f, OWNER).bev(0.3f);                       // marquee
        bx(m, 37, 59, 81.2f, 81.5f, 27.4f, 28.4f, cLight).m(EMIT).bev(0.1f);
        bx(m, 38, 58, 80, 84.5f, 11.6f, 12.6f, cConc).bev(0.3f);                     // entrance canopy
        pane(m, 42, 54, 80, 0.6f, 9.5f, cGlassD, GLASS);                              // doors
        bx(m, 14, 19, 80, 80.6f, 12, 30, OWNER).bev(0.1f);                           // banners
        bx(m, 77, 82, 80, 80.6f, 12, 30, OWNER).bev(0.1f);
        bx(m, 12, 38, 88, 94, 0.6f, 5.6f, cWall).t(T_PANEL, 6, cWallD).bev(0.3f);    // stage
        bx(m, 16, 34, 88, 88.6f, 4.6f, 7.6f, OWNER).bev(0.1f);                       // stage banner
    } else {
        bx(m, 8, 88, 40, 80, 0.6f, 31.4f, kBrick).t(T_WINDOWS, 8, kWin).bev(0.5f);   // brick walls with warm windows
        m.profileX({ { 36.0f, 31.4f }, { 60.0f, 44.0f }, { 84.0f, 31.4f } }, 6, 90, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // tiled gable
        bx(m, 6, 90, 58.5f, 61.5f, 43.0f, 44.6f, OWNER).bev(0.1f);                  // team ridge
        bx(m, 34, 62, 80, 81.2f, 25.0f, 28.5f, kWood).t(T_PLANKS, 2, kWoodL).bev(0.3f);   // wooden marquee board
        bx(m, 36, 60, 81.2f, 81.6f, 25.6f, 27.6f, kWarm).m(EMIT).bev(0.1f);          // bulbs on the marquee
        awningX(m, 38, 58, 80, 85, 14, OWNER, kPlaster, 6);                          // entrance canopy
        bx(m, 42, 54, 80, 80.6f, 0.6f, 11, kWood).t(T_PLANKS, 2, kWoodL).bev(0.2f);   // doors
        bx(m, 14, 19, 80, 80.6f, 12, 30, OWNER).bev(0.1f);                           // banners
        bx(m, 77, 82, 80, 80.6f, 12, 30, OWNER).bev(0.1f);
        bx(m, 12, 38, 88, 94, 0.6f, 5.6f, kBrick).t(T_BRICK, 4, kMort).bev(0.3f);    // brick stage
        bx(m, 16, 34, 88, 88.6f, 4.6f, 7.6f, OWNER).bev(0.1f);
    }
    bx(m, 40, 56, 84.5f, 87.0f, 0.6f, 1.8f, cy ? cConc : kStoneD).bev(0.2f);     // steps
    bx(m, 40, 56, 87.0f, 89.5f, 0.6f, 1.2f, cy ? cConc : kStoneD).bev(0.2f);
    lamp(m, 4, 92, 10, cy ? cLight : kWarm); lamp(m, 92, 92, 10, cy ? cLight : kWarm);
    // string lights swagged across the plaza
    m.rod({ 6, 86, 10 }, { 48, 86, 6 }, 0.25f, DARK).m(METAL).bev(0);
    m.rod({ 48, 86, 6 }, { 90, 86, 10 }, 0.25f, DARK).m(METAL).bev(0);
    for (int i = 0; i < 5; i++) {
        const float t = i / 4.0f, x = 6 + 84 * t, z = 10 - 4 * std::sin(3.14159265f * t);
        m.ball(x, 86, z - 0.8f, 0.9f, 0.9f, 0.9f, cy ? cLight : kWarm).m(EMIT);
    }
}

// ================================================================= TOWN SQUARE (3 x 3)
// An open plaza with a fountain in the middle, a gazebo, benches, four trees and lamps. Nothing tall: the team flag is the highest part.
void townSquare(Model& m, bool cy) {
    pad(m, 96, 96, cy ? cPave : kSand, T_PANEL, cy ? cSteel : kStoneD, 16);
    for (int i = 0; i < 4; i++) for (int j = 0; j < 4; j++) if ((i + j) & 1)
        bx(m, 24.0f * i, 24.0f * i + 24, 24.0f * j, 24.0f * j + 24, 0.6f, 0.75f, cy ? cWallD : kStoneD).bev(0);   // checker tiles
    for (float x : { 12.0f, 84.0f }) for (float y : { 12.0f, 84.0f }) tree(m, x, y, 6.0f, 4.6f, cy ? cLeaf : kLeaf);
    bench(m, 28, 72, 12, cy ? cWood : kWood); bench(m, 56, 72, 12, cy ? cWood : kWood); bench(m, 56, 20, 12, cy ? cWood : kWood);
    // the fountain: a rim, a pool, a central column with jets
    m.cyl('z', 48, 48, 1.3f, 18, 0.7f, cy ? cWall : kStone).bev(0.4f);
    m.cyl('z', 48, 48, 2.0f, 17.4f, 0.12f, OWNER).bev(0.05f);                      // team ring on the rim
    m.cyl('z', 48, 48, 2.0f, 14.6f, 0.2f, C(0x5FB8E0)).m(GLASS).bev(0);
    m.cyl('z', 48, 48, 7.5f, 2.4f, 5.6f, cy ? cWall : kStone).bev(0.3f);
    for (int i = 0; i < 4; i++) {
        const float an = i * 1.5708f;
        m.ball(48 + std::cos(an) * 6.0f, 48 + std::sin(an) * 6.0f, 3.4f, 0.9f, 0.9f, 0.9f, cy ? cLight : kWarm).m(EMIT);
    }
    // the gazebo: four posts and a low roof
    m.cyl('z', 78, 40, 0.9f, 9.5f, 0.3f, cy ? cWall : kStone).bev(0.2f);
    for (float x : { 72.0f, 84.0f }) for (float y : { 34.0f, 46.0f }) m.rod({ x, y, 1.2f }, { x, y, 7.0f }, 0.5f, METALC).m(METAL).bev(0);
    if (cy) {
        m.cyl('z', 78, 40, 6.2f, 11.4f, 0.5f, cCob).bev(0.2f);                       // eave
        m.cyl('z', 78, 40, 10.4f, 10.5f, 3.8f, OWNER, 0.0f).bev(0.3f);             // pagoda roof in the team colour
    } else {
        m.cyl('z', 78, 40, 6.2f, 11.4f, 0.5f, OWNER).bev(0.2f);                     // owner ring at the eave
        m.cyl('z', 78, 40, 10.4f, 10.5f, 3.8f, kTile, 0.0f).t(T_TILES, 3, kTileD).bev(0.3f);
    }
    lamp(m, 6, 48, 12, cy ? cLight : kWarm); lamp(m, 90, 48, 12, cy ? cLight : kWarm); lamp(m, 48, 90, 12, cy ? cLight : kWarm);
    flag(m, 50, 26, 18, 14, 8, METALC);                                             // the team flag
}

// ================================================================= TOWN HALL (4 x 3)
// The civic centre: a long hall with a clock tower, a portico of columns with a pediment and team roundel, steps, lawns and two flagpoles.
// Cyber: a pale block with glass clock tower and dome, cobalt trim and solar panels. Clanker: plaster hall under a low hip roof, a copper dome.
void townHall(Model& m, bool cy) {
    pad(m, 128, 96, cy ? cPave : kSand, T_PANEL, cy ? cSteel : kStoneD, 16);
    bx(m, 0, 6, 40, 92, 0.6f, 1.2f, cy ? cLawn : kLawn).m(MATTE).bev(0.2f);       // lawns at the sides
    bx(m, 122, 128, 40, 92, 0.6f, 1.2f, cy ? cLawn : kLawn).m(MATTE).bev(0.2f);
    tree(m, 4, 56, 6, 4, cy ? cLeaf : kLeaf); tree(m, 124, 56, 6, 4, cy ? cLeaf : kLeaf);
    tree(m, 4, 86, 6, 4, cy ? cLeaf : kLeaf); tree(m, 124, 86, 6, 4, cy ? cLeaf : kLeaf);
    const Color stone = cy ? cWall : kStone;
    bx(m, 22, 106, 84, 88, 0.6f, 2.2f, stone).bev(0.2f);                         // steps
    bx(m, 26, 102, 80, 84, 0.6f, 4.0f, stone).bev(0.2f);
    bx(m, 30, 98, 76, 80, 0.6f, 6.0f, stone).bev(0.2f);
    // the hall
    bx(m, 8, 120, 36, 76, 0.6f, 34, cy ? cWall : kPlaster).t(T_PANEL, 8, cy ? cWallD : kStoneD).bev(0.4f);
    bx(m, 6, 122, 36, 78, 34, 35.4f, stone).bev(0.4f);                           // cornice
    if (cy) {
        solar(m, 24, 60, 36.6f, 8, 3.5f, 0); solar(m, 98, 60, 36.6f, 8, 3.5f, 0);   // roof solar
        bx(m, 10, 118, 37, 39, 35.4f, 36.0f, OWNER).bev(0.1f);                       // team strip along the roof
    } else {
        m.taper(64, 56, 35.4f, 56, 20, 8, 44, 6, kTile).t(T_TILES, 3, kTileD).bev(0.4f);   // a low hip roof
        bx(m, 36, 92, 55, 57, 43.4f, 44.0f, OWNER).bev(0.1f);                        // team ridge
    }
    for (float x : { 14.0f, 100.0f }) pane(m, x, x + 12, 76, 10, 22, cy ? cGlassB : kWarm, GLASS);   // windows
    // the portico: six columns on the top step, an entablature and a pediment
    for (int i = 0; i < 6; i++) {
        const float x = 40 + i * 9.2f;
        m.cyl('z', x, 78, 16, 1.9f, 10, cy ? cWall : kPlaster).t(T_NONE).bev(0.2f);
        bx(m, x - 2.4f, x + 2.4f, 76.2f, 79.8f, 6.0f, 7.2f, stone).bev(0.2f);       // base
        bx(m, x - 2.4f, x + 2.4f, 76.2f, 79.8f, 25.0f, 26.4f, stone).bev(0.2f);     // capital
    }
    bx(m, 34, 94, 76.2f, 80.2f, 26.4f, 28.4f, stone).bev(0.3f);                  // entablature
    m.profileY({ { 34.0f, 28.4f }, { 64.0f, 42.0f }, { 94.0f, 28.4f } }, 76.2f, 80.2f, stone).bev(0.3f);   // pediment
    m.cyl('y', 64, 80.4f, 35, 4.4f, 0.5f, OWNER).bev(0.2f);                       // team roundel
    bx(m, 61, 65, 76, 76.6f, 0.6f, 11, cy ? cGlassD : kWood).m(cy ? GLASS : MATTE).bev(0.1f);   // the door between the columns
    // the clock tower
    bx(m, 50, 78, 30, 52, 30, 80, cy ? cWall : kPlaster).t(T_PANEL, 6, cy ? cWallD : kStoneD).bev(0.4f);
    bx(m, 50, 78, 30, 52, 70, 72.5f, OWNER).bev(0.1f);                           // team band round the tower
    m.cyl('y', 64, 52.4f, 56, 9.5f, 0.5f, cy ? C(0xF2F5F8) : C(0xE8E0C8)).bev(0.2f);   // clock face
    m.rod({ 64, 52.9f, 56 }, { 64, 52.9f, 62 }, 0.6f, DARK).bev(0);
    m.rod({ 64, 52.9f, 56 }, { 69, 52.9f, 54 }, 0.6f, DARK).bev(0);
    for (float x : { 56.0f, 68.0f }) pane(m, x, x + 4, 52, 40, 50, cy ? cGlassB : kWarm, GLASS);
    bx(m, 48, 80, 29, 54, 80, 81.2f, stone).bev(0.4f);                           // tower cap
    if (cy) m.dome(64, 41, 81.2f, 9.5f, 9.5f, 9.0f, cGlassB).m(GLASS);
    else m.dome(64, 41, 81.2f, 9.5f, 9.5f, 9.0f, kCopper);
    flag(m, 14, 90, 50, 16, 9, METALC); flag(m, 110, 90, 50, 16, 9, METALC);
}

// ================================================================= GOVERNMENT OFFICE (4 x 3)
// A formal ministry: a tall central block with a dome and a portico, two wings with roofs and antennas, and flags on the forecourt.
// Cyber: a glass front and a glass dome on pale walls. Clanker: brick with a pediment and a copper dome over the centre.
void government(Model& m, bool cy) {
    pad(m, 128, 96, cy ? cPave : kSand, T_PANEL, cy ? cSteel : kStoneD, 16);
    tree(m, 8, 88, 6, 5, cy ? cLeaf : kLeaf); tree(m, 120, 88, 6, 5, cy ? cLeaf : kLeaf);
    const Color stone = cy ? cWall : kStone;
    // wings
    bx(m, 6, 40, 46, 76, 0.6f, 26, cy ? cWall : kBrick).t(T_WINDOWS, 6, cy ? cGlassB : kWin).bev(0.5f);
    bx(m, 88, 122, 46, 76, 0.6f, 26, cy ? cWall : kBrick).t(T_WINDOWS, 6, cy ? cGlassB : kWin).bev(0.5f);
    if (cy) {
        bx(m, 5, 41, 45, 77, 26, 27, cConc).bev(0.3f);
        bx(m, 87, 123, 45, 77, 26, 27, cConc).bev(0.3f);
    } else {
        m.profileX({ { 45.0f, 26.0f }, { 61.0f, 33.0f }, { 77.0f, 26.0f } }, 5, 41, kTile).t(T_TILES, 3, kTileD).bev(0.3f);   // wing roofs
        m.profileX({ { 45.0f, 26.0f }, { 61.0f, 33.0f }, { 77.0f, 26.0f } }, 87, 123, kTile).t(T_TILES, 3, kTileD).bev(0.3f);
    }
    bx(m, 10, 24, 50, 53, 26, 26.6f, OWNER).bev(0.1f);                            // team band on the left wing
    bx(m, 104, 118, 50, 53, 26, 26.6f, OWNER).bev(0.1f);
    if (cy) {
        bx(m, 5, 41, 56, 58, 27, 27.8f, OWNER).bev(0.1f);                          // team strips on the wing roofs
        bx(m, 87, 123, 56, 58, 27, 27.8f, OWNER).bev(0.1f);
    } else {
        bx(m, 5, 41, 60.2f, 61.8f, 32.6f, 33.5f, OWNER).bev(0.1f);                 // team ridges on the wing roofs
        bx(m, 87, 123, 60.2f, 61.8f, 32.6f, 33.5f, OWNER).bev(0.1f);
    }
    m.rod({ 20, 60, 26 }, { 20, 60, 43 }, 0.5f, cy ? cCob : kIron).m(METAL).bev(0);      // antennas
    m.rod({ 108, 60, 26 }, { 108, 60, 43 }, 0.5f, cy ? cCob : kIron).m(METAL).bev(0);
    m.ball(20, 60, 43.6f, 0.7f, 0.7f, 0.7f, cy ? cLight : kWarm).m(EMIT);
    m.ball(108, 60, 43.6f, 0.7f, 0.7f, 0.7f, cy ? cLight : kWarm).m(EMIT);
    // central block
    bx(m, 40, 88, 30, 76, 0.6f, 63, cy ? cWall : kPlaster).t(T_WINDOWS, 6, cy ? cGlassB : kWin).bev(0.5f);
    bx(m, 38, 90, 28, 78, 63, 64.2f, stone).bev(0.4f);                            // central roof
    bx(m, 44, 84, 30, 32, 64.2f, 64.8f, OWNER).bev(0.1f);                          // team band on the central roof
    bx(m, 38, 90, 74, 76, 64.2f, 64.8f, OWNER).bev(0.1f);                          // team band along the front of the roof
    m.cyl('z', 64, 48, 63.9f, 12.6f, 0.6f, OWNER).bev(0.2f);                       // owner ring under the dome
    if (cy) {
        pane(m, 44, 84, 76, 6.0f, 58.0f, cGlassB, GLASS);                          // the glass front
        m.dome(64, 48, 64.2f, 12, 12, 12, cGlassB).m(GLASS);
        bx(m, 50, 78, 76, 84, 22, 23, cGlassB).m(GLASS).bev(0.2f);                  // glass canopy over the entrance
        bx(m, 51, 53, 79, 80, 0.6f, 22, cWall).bev(0.2f);
        bx(m, 75, 77, 79, 80, 0.6f, 22, cWall).bev(0.2f);
    } else {
        m.dome(64, 48, 64.2f, 12, 12, 12, kCopper);
        for (float x : { 52.0f, 60.0f, 68.0f, 76.0f }) m.cyl('z', x, 79, 13, 1.8f, 12.4f, kPlaster).bev(0.2f);   // columns
        bx(m, 46, 82, 76, 80, 24.4f, 26.4f, kStone).bev(0.3f);                       // entablature
        m.profileY({ { 46.0f, 26.4f }, { 64.0f, 36.0f }, { 82.0f, 26.4f } }, 76, 80, kStone).bev(0.3f);   // pediment
    }
    bx(m, 58, 70, 76, 76.6f, 0.6f, 12, cy ? cGlassD : kWood).m(cy ? GLASS : MATTE).bev(0.1f);   // door
    bx(m, 50, 78, 76, 80, 0.6f, 1.8f, stone).bev(0.2f);                           // steps
    bx(m, 46, 82, 80, 84, 0.6f, 1.2f, stone).bev(0.2f);
    flag(m, 22, 90, 50, 16, 9, METALC); flag(m, 106, 90, 50, 16, 9, METALC);
    lamp(m, 44, 90, 12, cy ? cLight : kWarm); lamp(m, 84, 90, 12, cy ? cLight : kWarm);
}

}  // namespace

// ---------------------------------------------------------------- registration
static void cHouse(Model& m, int) { house(m, true); }
static void kHouse(Model& m, int) { house(m, false); }
static void cApartments(Model& m, int) { apartments(m, true); }
static void kApartments(Model& m, int) { apartments(m, false); }
static void cSkyscraper(Model& m, int) { skyscraper(m, true); }
static void kSkyscraper(Model& m, int) { skyscraper(m, false); }
static void cGas(Model& m, int) { gasStation(m, true); }
static void kGas(Model& m, int) { gasStation(m, false); }
static void cGrocery(Model& m, int) { grocery(m, true); }
static void kGrocery(Model& m, int) { grocery(m, false); }
static void cEventHall(Model& m, int) { eventHall(m, true); }
static void kEventHall(Model& m, int) { eventHall(m, false); }
static void cSquare(Model& m, int) { townSquare(m, true); }
static void kSquare(Model& m, int) { townSquare(m, false); }
static void cTownHall(Model& m, int) { townHall(m, true); }
static void kTownHall(Model& m, int) { townHall(m, false); }
static void cGovernment(Model& m, int) { government(m, true); }
static void kGovernment(Model& m, int) { government(m, false); }

M3D_BUILD(B_C_HOUSE, cHouse)
M3D_BUILD(B_C_APARTMENTS, cApartments)
M3D_BUILD(B_C_SKYSCRAPER, cSkyscraper)
M3D_BUILD(B_C_GAS, cGas)
M3D_BUILD(B_C_GROCERY, cGrocery)
M3D_BUILD(B_C_EVENTHALL, cEventHall)
M3D_BUILD(B_C_SQUARE, cSquare)
M3D_BUILD(B_C_TOWNHALL, cTownHall)
M3D_BUILD(B_C_GOVERNMENT, cGovernment)
M3D_BUILD(B_K_HOUSE, kHouse)
M3D_BUILD(B_K_APARTMENTS, kApartments)
M3D_BUILD(B_K_SKYSCRAPER, kSkyscraper)
M3D_BUILD(B_K_GAS, kGas)
M3D_BUILD(B_K_GROCERY, kGrocery)
M3D_BUILD(B_K_EVENTHALL, kEventHall)
M3D_BUILD(B_K_SQUARE, kSquare)
M3D_BUILD(B_K_TOWNHALL, kTownHall)
M3D_BUILD(B_K_GOVERNMENT, kGovernment)
