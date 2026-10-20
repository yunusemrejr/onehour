#include "bld.h"

// Structure art for the civilian town and the Underground Nuke silo (the sister file of artb_naval.cpp). The town is lawns, paving, trees,
// parked cars and warm window light: a house with a pitched roof and a garden, an apartment block, a skyscraper on a stepped podium, a gas
// station and a grocery under canopies and awnings, an event hall, a town square with a fountain, a classical town hall and a ministry.
// Cyber is white ceramic, blue glass and cyan trim; Clanker is brick, cream concrete and terracotta. Each carries a team mark (a roof band,
// a sign or a flag). The silo is a steel-lipped pit with silver blast doors; its armed overlay slides them back over a warhead.
namespace {

const Color SILVER = rgb(196, 200, 206), SILVER_D = rgb(160, 164, 170), SILVER_L = rgb(228, 230, 234);

// one army's civilian palette
struct CivPal {
    Color wall, wallD, roof, roofD, glass, glassD, pave, seam, lawn, lawnD, tree, treeL, warm, trim, stone, stoneD, fence, car1, car2, sign, asph, dome, wood;
};
CivPal civPal(bool cy) {
    if (cy) return CivPal{
        rgb(224, 231, 238), rgb(170, 184, 200), rgb(74, 112, 164), rgb(50, 84, 132),    // wall, wall shade, roof, roof shade
        rgb(62, 140, 186), rgb(30, 80, 120), rgb(186, 196, 206), rgb(134, 148, 164),    // glass, dark glass, paving, joints
        rgb(96, 158, 124), rgb(70, 124, 96), rgb(72, 150, 120), rgb(120, 198, 160),     // lawn, lawn shade, tree, tree light
        rgb(255, 214, 140), C_CYAN, rgb(210, 218, 226), rgb(160, 172, 186),             // warm light, team trim, stone, stone shade
        rgb(236, 240, 244), rgb(236, 240, 244), rgb(70, 130, 200), rgb(34, 64, 104),    // fence, car, car, sign board
        rgb(70, 78, 92), rgb(214, 226, 236), rgb(150, 120, 90) };                        // asphalt, dome, wood
    return CivPal{
        rgb(156, 96, 68), rgb(118, 70, 50), rgb(170, 100, 62), rgb(128, 72, 44),        // brick, brick shade, terracotta, terracotta shade
        rgb(104, 118, 108), rgb(54, 60, 54), rgb(182, 166, 134), rgb(132, 118, 92),     // dull glass, dark glass, sand paving, joints
        rgb(118, 136, 72), rgb(88, 102, 54), rgb(92, 112, 56), rgb(138, 156, 82),       // olive lawn, lawn shade, tree, tree light
        rgb(255, 206, 100), rgb(232, 188, 62), rgb(206, 196, 170), rgb(164, 154, 128),  // warm light, yellow trim, cream concrete, shade
        rgb(214, 206, 186), rgb(190, 84, 60), rgb(120, 126, 120), rgb(96, 52, 36),      // fence, rust car, grey car, sign board
        rgb(96, 92, 84), rgb(110, 150, 128), rgb(120, 92, 62) };                         // asphalt, patina dome, wood
}

// ---------------------------------------------------------------- helpers
// the ground of a lot: a slab with a soft drop shadow and a worn surface
void ground(Bld& b, Color base, float wear = 0.10f, float r = 4) {
    b.a.shadowRect(0, 0, (float)b.W, (float)b.H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)b.W, (float)b.H, r, base, 2.6f, 0.5f);
    b.a.grain(0, 0, (float)b.W, (float)b.H, wear, b.rng.next());
}
// a tree seen from above: a shadowed crown with a lit side
void tree(Bld& b, const CivPal& p, float x, float y, float r) {
    b.a.shadowCircle(x, y, r, r * 0.5f, r * 0.6f, 2.0f + r * 0.2f, 100);
    b.a.circle(x, y, r, p.tree, r * 0.45f, 0.6f);
    b.a.dot(x - r * 0.3f, y - r * 0.35f, r * 0.25f, rgba(p.treeL, 200));
}
// a parked car, long axis east-west with its headlights to the east
void car(Bld& b, float x, float y, float w, float h, Color body) {
    b.a.shadowRect(x, y, w, h, 1.2f, 1.2f, 1.4f, 90);
    b.a.rect(x, y, w, h, 2.0f, body, 1.2f, 0.6f);
    b.a.rect(x + w * 0.42f, y + 1.0f, w * 0.28f, h - 2.0f, 1.0f, rgb(36, 50, 66), 0.8f, 0.3f);
    b.a.dot(x + w - 1.2f, y + 1.4f, 0.6f, WHITE); b.a.dot(x + w - 1.2f, y + h - 1.4f, 0.6f, WHITE);
}
// a street lamp standing on (x, y)
void lamp(Bld& b, float x, float y, float ht, Color lit) {
    b.a.line(x + 1.0f, y + 1.0f, x + 1.0f, y - ht + 1.0f, 0.9f, rgba(BLACK, 60));
    b.a.cap(x, y, x, y - ht, 0.55f, rgb(64, 68, 76), 0.6f, 0.4f);
    b.light(x, y - ht, 1.0f, lit, 130);
}
// a park bench: a wooden slat with its shadow
void bench(Bld& b, float x, float y, float w, Color wood) {
    b.a.shadowRect(x, y, w, 3.2f, 1.0f, 1.2f, 1.2f, 80);
    b.a.rect(x, y, w, 3.2f, 0.6f, wood, 0.9f, 0.5f);
}
// a low rail fence with posts along the line
void fence(Bld& b, float x0, float y0, float x1, float y1, Color c) {
    b.a.line(x0, y0, x1, y1, 0.7f, c);
    int n = std::max(2, (int)(hyp(x1 - x0, y1 - y0) / 3.5f));
    for (int i = 0; i <= n; i++) { float k = (float)i / n; b.a.dot(x0 + (x1 - x0) * k, y0 + (y1 - y0) * k, 0.7f, c); }
}
// a flagpole with a team flag: the cloth is a dark seat in the colour painter and white in the team mask
void flag(Bld& b, float x, float y, float ht, float fw, float fh) {
    b.a.line(x + 0.9f, y + 0.9f, x + 0.9f, y - ht + 0.9f, 0.9f, rgba(BLACK, 60));
    b.a.cap(x, y, x, y - ht, 0.6f, rgb(206, 210, 216), 0.6f, 0.4f);
    float ty = y - ht + 1.5f;
    std::vector<P> cloth = { {x, ty}, {x + fw, ty + 1.0f}, {x + fw, ty + fh + 1.0f}, {x, ty + fh} };
    b.a.poly(cloth, rgb(22, 24, 30), 0.8f, 0.2f);
    b.t.poly(cloth, WHITE, 1.2f, 0.45f);
}
// a striped awning across x0..x1 (the stripes alternate between two colours)
void awning(Bld& b, float x0, float y0, float x1, float h, Color c1, Color c2, int n) {
    float pw = (x1 - x0) / n;
    for (int i = 0; i < n; i++) b.a.rect(x0 + i * pw, y0, pw, h, 0.4f, i & 1 ? c2 : c1, 1.0f, 0.3f);
    b.a.line(x0, y0 + h, x1, y0 + h, 0.6f, rgba(BLACK, 90));
}
// a glass face from a roof's bottom edge down to the base: mullions, floor lines, a sheen and a few lit rooms
void glassFace(Bld& b, const CivPal& p, const Roof& r, float base, float pitch) {
    float top = r.y + r.h;
    for (float y = top + 1.5f; y + 3.0f < base; y += 5.0f)
        for (float x = r.x; x + pitch <= r.x + r.w + 0.1f; x += pitch) if (b.rng.f() < 0.28f) b.a.rect(x + 1.5f, y, pitch - 3.0f, 2.6f, 0.3f, p.warm, 0.4f, 0.2f);
    for (float x = r.x + pitch; x < r.x + r.w - 1; x += pitch) b.a.line(x, top, x, base, 0.6f, rgba(BLACK, 80));
    for (float y = top + 5; y < base - 1; y += 5) b.a.line(r.x, y, r.x + r.w, y, 0.4f, rgba(BLACK, 50));
    b.a.line(r.x + 4, top + 2, r.x + 10, base - 2, 2.0f, rgba(WHITE, 40));
}
// the radiation sign: a yellow disc with three black blades round a black centre
void trefoil(Bld& b, float cx, float cy, float r) {
    b.a.circle(cx, cy, r, rgb(236, 196, 52), 1.0f, 0.2f);
    for (int i = 0; i < 3; i++) { float an = -1.5708f + i * 2.0944f; b.a.circle(cx + std::cos(an) * r * 0.5f, cy + std::sin(an) * r * 0.5f, r * 0.42f, rgb(20, 20, 18), 0.6f, 0.1f); }
    b.a.dot(cx, cy, r * 0.2f, rgb(20, 20, 18));
}

// ================================================================= HOUSE (2x2)
// A two-storey family home: a pitched roof with a chimney, a lawn and a path to the gate, a driveway with a car, a tree and flowers.
void house(Bld& b, const CivPal& p) {
    ground(b, p.lawn, 0.12f);
    b.a.rect(29, 50, 6, 14, 0.4f, p.pave, 1.0f, 0.3f);                  // path from the door to the gate
    b.a.rect(37, 52, 21, 11, 0.5f, p.pave, 0.8f, 0.3f);                 // driveway
    for (float x : { 22.0f, 25.0f, 18.0f }) b.a.dot(x, 58, 1.2f, rgb(236, 92, 110));
    fence(b, 3, 62, 27, 62, p.fence); fence(b, 37, 62, 61, 62, p.fence);
    fence(b, 3, 40, 3, 62, p.fence); fence(b, 61, 40, 61, 62, p.fence);
    tree(b, p, 13, 57, 5);
    Roof r = b.block(6, 22, 52, 28, 14, p.roof, p.wall);                // walls 36..50, the roof lifted to 8
    b.a.rect(6, 22, 52, 14, 1.0f, p.roofD, 2.3f, 0.58f);                // the south slope; the ridge is its lit north edge
    for (int row = 0; row < 6; row++) {
        float yy = 10 + row * 4;
        b.a.line(7, yy, 57, yy, 0.45f, rgba(p.roofD, 120));
        for (float xx = 9 + (row & 1 ? 3 : 0); xx < 56; xx += 7)
            b.a.line(xx, yy, xx, yy + 3.8f, 0.4f, rgba(p.roofD, 115));
    }
    b.a.line(6, 22, 58, 22, 0.8f, rgba(BLACK, 90));
    b.a.shadowRect(41, 4, 5, 12, 2, 2, 1.5f, 90);
    b.a.rect(41, 4, 5, 12, 0.5f, p.wallD, 0.9f, 0.5f);                  // chimney
    b.windows(r, 14, 2, p.warm, 0.55f);
    b.a.rect(25, 50, 14, 3, 0.5f, p.stone, 0.8f, 0.4f);
    b.a.rect(23, 53, 18, 2, 0.5f, p.pave, 0.6f, 0.4f);
    b.door(32, 50, 9, 11, p.sign, p.glassD);
    b.mark(12, 28, 14, 3);                                              // team roof band
    car(b, 40, 54, 16, 6, p.car2);
}

// ================================================================= APARTMENTS (3x2)
// A five-storey block with rows of lit windows and balcony rails, a water tank and air conditioners on the flat roof, cars in the lot.
void apartments(Bld& b, const CivPal& p) {
    ground(b, p.asph, 0.10f);
    for (float x = 4; x < 92; x += 12) b.a.line(x, 57, x, 63, 0.5f, rgba(WHITE, 150));   // parking lines
    car(b, 6, 57, 13, 6, p.car1); car(b, 30, 57, 13, 6, p.car2); car(b, 72, 57, 13, 6, p.car1);
    b.block(6, 22, 84, 34, 30, p.roof, p.wall);                         // walls 26..56, the flat roof lifted to -8
    for (float yy = 28; yy + 3.6f < 45; yy += 6) {
        for (float xx = 9; xx + 6 <= 88; xx += 8.4f) b.a.rect(xx, yy, 6, 3.6f, 0.4f, b.rng.f() < 0.45f ? p.warm : p.glassD, 0.6f, 0.2f);
        b.a.line(8, yy + 4.6f, 88, yy + 4.6f, 0.7f, rgba(p.stone, 230));                 // balcony rail
    }
    b.door(48, 56, 16, 10, p.sign, p.glassD);                           // the lobby
    b.cyl(18, 12, 5, 9, p.stone, p.wallD);                              // water tank on the roof
    b.block(44, 14, 12, 7, 3, p.stone, p.wallD); b.vent(46, 12.5f, 8, 3, shade(p.stone, 1.3f));
    b.block(62, 14, 12, 7, 3, p.stone, p.wallD); b.vent(64, 12.5f, 8, 3, shade(p.stone, 1.3f));
    // Roof service walkway and framed solar collectors distinguish the modern block.
    if (b.cy) for (int i = 0; i < 3; i++) {
        float x = 32 + i * 16;
        b.a.rect(x, -4, 13, 9, 0.4f, p.stone, 0.6f, 0.3f);
        b.a.rect(x + 1, -3, 11, 7, 0.3f, rgb(35, 73, 104), 0.5f, 0.3f);
        b.a.line(x + 6, -3, x + 6, 4, 0.4f, rgba(WHITE, 120));
    }
    else { b.a.line(10, 26, 88, 26, 1.2f, p.stone); b.a.line(10, 44, 88, 44, 1.2f, p.stone); }
    b.mark(8, 19, 30, 3);                                               // team band along the roof's front
}

// ================================================================= SKYSCRAPER (3x3)
// A glass tower on a stepped podium: the three blocks share one base line, so the steps read as setbacks. The crown has a helipad and a
// team band across its glass, and the plaza at its foot has trees, benches and a lamp.
void skyscraper(Bld& b, const CivPal& p) {
    ground(b, p.stone, 0.05f);
    for (float x = 16; x < 96; x += 16) b.a.line(x, 84, x, 96, 0.5f, rgba(p.seam, 110));
    tree(b, p, 8, 90, 5); tree(b, p, 88, 90, 5);
    bench(b, 26, 90, 12, p.wood); bench(b, 58, 90, 12, p.wood);
    b.block(6, 66, 84, 16, 26, p.roof, p.wall);                         // podium: roof 40..56
    b.mark(7, 46, 10, 3); b.mark(79, 46, 10, 3);                        // team marks on the podium roof's strips
    Roof mid = b.block(18, 66, 60, 16, 52, p.roofD, p.glass);           // middle: roof 14..30
    glassFace(b, p, mid, 82, 7.5f);
    Roof top = b.block(28, 66, 40, 16, 90, p.roof, p.glass);            // crown: roof -24..-8
    glassFace(b, p, top, 82, 8);
    b.mark(31, 40, 34, 3);                                              // team band across the crown
    b.a.circle(48, -16, 7, p.roofD, 1.2f, 0.4f);                        // helipad
    b.a.ring(48, -16, 5.4f, 0.7f, rgba(p.trim, 230), 0.5f, 0);
    b.a.line(45.4f, -19.5f, 45.4f, -12.5f, 1.0f, WHITE); b.a.line(50.6f, -19.5f, 50.6f, -12.5f, 1.0f, WHITE); b.a.line(45.4f, -16, 50.6f, -16, 1.0f, WHITE);
    lamp(b, 26, 94, 12, p.warm);
}

// ================================================================= GAS STATION (3x2)
// Three pump islands under a flat canopy with a team-colour fascia, a corner shop with a glass front and a tall price sign on a pole.
void gasStation(Bld& b, const CivPal& p) {
    ground(b, p.asph, 0.14f);
    for (float x = 6; x < 90; x += 14) b.a.line(x, 57, x + 6, 57, 0.8f, rgba(WHITE, 160));   // lane dashes
    b.block(72, 4, 22, 24, 12, p.roof, p.wall);                         // shop: roof -8..16, walls 16..28
    b.a.rect(73, 18, 7, 7, 0.4f, p.glass, 0.6f, 0.3f); b.a.rect(88, 18, 5, 7, 0.4f, p.glass, 0.6f, 0.3f);
    b.door(84, 28, 6, 10, p.sign, p.glassD);
    b.mark(74, 9, 18, 3);                                               // team sign on the shop roof
    b.a.shadowRect(4, -4, 66, 14, 8, 12, 4, 90);
    b.a.rect(4, -4, 66, 14, 1.5f, p.stone, 2.2f, 0.5f);                 // canopy slab
    b.mark(6, 10, 62, 3.5f);                                            // team stripe on the canopy edge
    for (float x : { 6.0f, 65.0f }) b.a.rect(x, 13.5f, 3, 15, 0.6f, p.stoneD, 1.0f, 0.4f);   // pillars
    for (float x : { 10.0f, 36.0f, 62.0f }) {
        b.a.rect(x, 40, 14, 6, 1.0f, p.stone, 1.4f, 0.5f);              // island
        b.a.rect(x + 4, 31, 6, 9, 1.0f, p.wall, 1.2f, 0.5f);            // pump
        b.a.rect(x + 5.2f, 33, 3.6f, 2.4f, 0.3f, rgb(90, 255, 160), 0.4f, 0.2f);   // display
    }
    car(b, 12, 50, 13, 6, p.car1); car(b, 38, 50, 13, 6, p.car2);
    b.a.cap(87, 62, 87, 54, 0.8f, p.stoneD, 0.6f, 0.4f);                // price sign on a pole
    b.a.rect(80, 36, 14, 18, 1.2f, p.sign, 1.4f, 0.4f);
    b.a.rect(83, 39, 8, 3.2f, 0.3f, rgb(255, 88, 76), 0.6f, 0.2f);
    for (float y : { 45.0f, 49.0f }) b.a.rect(83, y, 8, 1.6f, 0.2f, WHITE, 0.4f, 0.2f);
}

// ================================================================= GROCERY STORE (3x2)
// A flat-roofed store with a big glass front under a striped awning, a sign board with a team badge, parking bays and shopping carts.
void grocery(Bld& b, const CivPal& p) {
    ground(b, p.asph, 0.10f);
    for (float x = 4; x < 92; x += 12) b.a.line(x, 56, x, 63, 0.6f, rgba(WHITE, 150));
    car(b, 8, 57, 13, 6, p.car2); car(b, 58, 57, 13, 6, p.car1);
    b.block(6, 16, 84, 34, 20, p.roof, p.wall);                         // flat roof -4..30, walls 30..50
    b.a.rect(10, 22, 76, 7, 1.0f, p.sign, 1.2f, 0.4f);                  // sign board on the roof's edge
    b.mark(12, 23, 14, 5);                                              // team badge
    for (float x = 30; x < 84; x += 6) b.a.line(x, 25.5f, x + 3.5f, 25.5f, 1.4f, rgba(WHITE, 220));
    b.a.rect(8, 33, 80, 14, 0.5f, p.glass, 1.0f, 0.3f);                 // glass front
    for (float x = 20; x < 88; x += 12) b.a.line(x, 33, x, 47, 0.7f, rgba(BLACK, 110));
    for (float y : { 37.0f, 42.0f }) b.a.line(10, y, 86, y, 0.6f, rgba(p.warm, 170));   // shelves lit from inside
    b.door(48, 47, 8, 14, p.sign, p.glassD);
    awning(b, 6, 50, 90, 5, p.trim, rgb(240, 240, 236), 12);
    for (float x : { 82.0f, 88.0f }) { b.a.rect(x, 58, 4.5f, 4, 0.4f, rgb(176, 182, 190), 0.8f, 0.5f); b.a.line(x, 60, x + 4.5f, 60, 0.4f, rgba(BLACK, 90)); }   // carts
}

// ================================================================= EVENT HALL (3x3)
// A long arched hall with a ribbed barrel roof, a glowing marquee over the entrance, team banners on the wall, and a front plaza with
// string lights and a little stage.
void eventHall(Bld& b, const CivPal& p) {
    ground(b, p.pave, 0.06f);
    for (float x = 16; x < 96; x += 16) b.a.line(x, 80, x, 96, 0.5f, rgba(p.seam, 110));
    tree(b, p, 92, 62, 4);
    b.block(8, 40, 80, 40, 22, p.roof, p.wall, 20.0f);                  // arched roof (rounded ends): roof 18..58, walls 58..80
    for (float x = 28; x < 68; x += 6) { b.a.line(x, 20, x, 56, 0.6f, rgba(BLACK, 70)); b.a.line(x + 0.8f, 20, x + 0.8f, 56, 0.4f, rgba(WHITE, 40)); }
    b.a.line(30, 38, 66, 38, 0.8f, rgba(WHITE, 90));                    // the ridge
    b.a.rect(12, 62, 72, 9, 0.5f, p.glass, 1.0f, 0.3f);                 // glass band
    for (float x = 20; x < 84; x += 9) b.a.line(x, 62, x, 71, 0.6f, rgba(BLACK, 110));
    b.a.rect(34, 58.5f, 28, 4, 0.5f, p.sign, 0.8f, 0.3f);               // marquee
    for (float x = 36; x < 62; x += 4) b.a.dot(x, 60.5f, 0.6f, p.warm);
    b.mark(16, 60, 5, 14); b.mark(75, 60, 5, 14);                       // team banners
    b.door(48, 80, 16, 14, p.sign, p.glassD);
    b.block(12, 88, 26, 6, 4, p.stone, p.wallD);                        // little stage on the plaza
    b.mark(16, 85, 18, 2.6f);                                           // team banner on the stage
    for (int i = 0; i <= 28; i++) {                                     // string lights swagged over the plaza
        float t = i / 28.0f, x = 6 + 84 * t, y = 82 + 40 * t * (1 - t);
        Color lc = i % 4 == 0 ? p.warm : i % 4 == 1 ? rgb(255, 96, 80) : i % 4 == 2 ? C_CYAN : rgb(120, 230, 130);
        b.a.glow(x, y, 3.2f, rgba(lc, 90)); b.a.dot(x, y, 0.9f, lc);
    }
}

// ================================================================= TOWN SQUARE (3x3)
// An open checkered plaza: a fountain in the middle, a gazebo, benches, four corner trees, lamps and a team flag.
void townSquare(Bld& b, const CivPal& p) {
    ground(b, p.stone, 0.05f);
    for (int i = 0; i < 8; i++) for (int j = 0; j < 8; j++) if ((i + j) & 1) b.a.rect(i * 12.0f, j * 12.0f, 12, 12, 0, rgba(p.stoneD, 130), 0.7f, 0.2f);
    for (float x : { 12.0f, 84.0f }) for (float y : { 12.0f, 84.0f }) tree(b, p, x, y, 7);
    bench(b, 28, 72, 12, p.wood); bench(b, 56, 72, 12, p.wood); bench(b, 56, 20, 12, p.wood);
    b.a.circle(48, 48, 18, p.stone, 2.4f, 0.5f);                        // fountain basin
    b.a.circle(48, 48, 14.6f, rgb(70, 160, 210), 1.6f, 0.4f);           // water
    b.a.ring(48, 48, 10.5f, 0.6f, rgba(WHITE, 110), 0.5f, 0);
    P top = b.cyl(48, 52, 3.2f, 10, p.stone, p.stoneD);                 // central pillar
    b.a.glow(top.x, top.y, 8, rgba(WHITE, 110));
    for (int i = 0; i < 6; i++) { float an = i * 1.0472f; b.a.dot(top.x + std::cos(an) * 7.0f, top.y + std::sin(an) * 3.5f, 0.6f, rgba(WHITE, 200)); }
    b.a.circle(78, 40, 9.5f, p.stoneD, 1.5f, 0.3f);                     // gazebo floor
    b.a.shadowCircle(78, 28, 10.5f, 4, 7, 3, 80);
    for (float dx : { -6.0f, 6.0f }) for (float dy : { -6.0f, 6.0f }) b.a.line(78 + dx, 40 + dy, 78 + dx, 28 + dy, 1.2f, p.fence);
    b.a.ngon(78, 28, 10.5f, 8, 0.3927f, p.roof, 2.2f, 0.6f);            // pointed octagonal roof
    b.a.dot(78, 28, 1.4f, p.trim);
    lamp(b, 6, 48, 14, p.warm); lamp(b, 90, 48, 14, p.warm); lamp(b, 48, 90, 14, p.warm);
    flag(b, 50, 26, 36, 10, 6);
}

// ================================================================= TOWN HALL (4x3)
// Classical: lawns at the sides, wide steps, a portico of six columns under a pediment with a team roundel, a clock tower with a dome, and
// two flagpoles with team flags on the plaza.
void townHall(Bld& b, const CivPal& p) {
    ground(b, p.stone, 0.05f);
    b.a.rect(0, 40, 6, 52, 1.0f, p.lawn, 1.2f, 0.4f); b.a.rect(122, 40, 6, 52, 1.0f, p.lawn, 1.2f, 0.4f);
    tree(b, p, 4, 56, 4); tree(b, p, 124, 56, 4); tree(b, p, 4, 86, 4); tree(b, p, 124, 86, 4);
    b.a.rect(30, 76, 68, 4, 0.5f, p.stone, 1.4f, 0.5f); b.a.rect(26, 80, 76, 4, 0.5f, p.stone, 1.4f, 0.5f); b.a.rect(22, 84, 84, 4, 0.5f, p.stone, 1.4f, 0.5f);   // steps
    b.block(8, 36, 112, 40, 24, p.roof, p.wall);                        // main hall: roof 12..52, walls 52..76
    for (float x : { 14.0f, 26.0f, 100.0f, 112.0f }) { b.a.rect(x - 1, 56, 8, 16, 0.6f, p.sign, 0.8f, 0.3f); b.a.rect(x, 57, 6, 14, 0.5f, p.warm, 0.8f, 0.3f); }
    b.a.rect(34, 58, 60, 4, 0.6f, p.stone, 1.4f, 0.5f);                 // entablature
    b.a.poly({ {32, 58.5f}, {96, 58.5f}, {64, 42} }, p.stone, 1.6f, 0.5f);   // pediment
    b.markDisc(64, 51, 3.6f);                                           // team roundel in the pediment
    for (int i = 0; i < 6; i++) {
        float x = 40 + i * 9.2f;
        b.a.rect(x - 2.0f, 62, 4.0f, 14, 0.6f, p.stone, 1.6f, 0.55f);  // column shaft
        b.a.rect(x - 2.6f, 61, 5.2f, 1.6f, 0.4f, p.stoneD, 0.8f, 0.3f);
        b.a.rect(x - 2.6f, 74.6f, 5.2f, 1.6f, 0.4f, p.stoneD, 0.8f, 0.3f);
    }
    b.block(50, 32, 28, 20, 56, p.roof, p.wall);                        // clock tower: roof -24..-4, walls to 52
    b.a.circle(64, 14, 7.5f, rgb(240, 240, 232), 1.2f, 0.3f); b.a.ring(64, 14, 7.2f, 0.8f, rgba(BLACK, 160), 0.5f, 0);
    b.a.line(64, 14, 64, 9, 0.8f, BLACK); b.a.line(64, 14, 68, 15.5f, 0.8f, BLACK);   // clock hands
    b.a.rect(56, 24, 5, 7, 0.6f, p.warm, 0.8f, 0.3f); b.a.rect(67, 24, 5, 7, 0.6f, p.warm, 0.8f, 0.3f);
    b.a.circle(64, -14, 10.5f, p.dome, 2.4f, 0.7f);                     // dome
    flag(b, 14, 90, 46, 12, 7); flag(b, 114, 90, 46, 12, 7);
}

// ================================================================= GOVERNMENT OFFICE (4x3)
// A symmetric ministry: a tall central block with a dome and a team band, two lower wings with antennas, rows of windows and flags on the forecourt.
void government(Bld& b, const CivPal& p) {
    ground(b, p.pave, 0.05f);
    for (float x = 16; x < 128; x += 16) b.a.line(x, 76, x, 96, 0.5f, rgba(p.seam, 110));
    tree(b, p, 8, 88, 5); tree(b, p, 120, 88, 5);
    Roof wl = b.block(6, 46, 34, 30, 18, p.roof, p.wall);               // left wing: roof 28..58, walls 58..76
    Roof wr = b.block(88, 46, 34, 30, 18, p.roof, p.wall);              // right wing
    b.windows(wl, 18, 4, p.warm, 0.5f); b.windows(wr, 18, 4, p.warm, 0.5f);
    b.mark(10, 36, 14, 3); b.mark(104, 36, 14, 3);
    b.antenna(20, 36, 12, p.trim); b.antenna(108, 36, 12, p.trim);
    b.block(40, 30, 48, 46, 44, p.roof, p.glass);                       // central block: roof -14..32, walls 32..76
    glassFace(b, p, Roof{40, -14, 48, 46}, 76, 8);
    b.mark(46, 26, 36, 3);                                              // team band on the central roof
    b.domeTop(64, 4, 12, p.dome);
    b.a.rect(50, 76, 28, 3, 0.5f, p.stone, 1.2f, 0.5f); b.a.rect(46, 79, 36, 3, 0.5f, p.stone, 1.2f, 0.5f);   // entrance steps
    b.door(64, 76, 14, 12, p.sign, p.glassD);
    flag(b, 22, 90, 44, 12, 7); flag(b, 106, 90, 44, 12, 7);
    lamp(b, 44, 90, 12, p.trim); lamp(b, 84, 90, 12, p.trim);
}

// ================================================================= UNDERGROUND NUKE (3x3)
// A buried silo: a steel lip round a sunk well, two silver blast doors closed on a seam with a locking wheel and bolts, a radiation sign on
// the left leaf and a hazard frame round the pad. Cyber: cyan status lights at the corners; Clanker: rust on the lip.
void undergroundSilo(Bld& b) {
    int W = b.W, H = b.H;
    bool cy = b.cy;
    b.pad(cy ? rgb(128, 138, 152) : rgb(156, 146, 124), cy ? rgb(84, 94, 108) : rgb(116, 106, 84), 0.10f, 4);
    b.hazard(2, 2, W - 4.0f, 3.0f); b.hazard(2, H - 5.0f, W - 4.0f, 3.0f); b.hazard(2, 2, 3.0f, H - 4.0f); b.hazard(W - 5.0f, 2, 3.0f, H - 4.0f);
    b.a.rect(9, 9, 78, 78, 3, cy ? rgb(84, 96, 114) : rgb(110, 104, 92), 2.4f, 0.5f);   // steel lip round the pit
    b.a.rect(12, 12, 72, 72, 2, rgb(16, 18, 22), 1.6f, 0.3f);                             // the sunk well
    for (int s = 0; s < 2; s++) {                                                         // the closed blast doors
        float x = s ? 48.0f : 15.0f;
        b.a.rect(x, 15, 33, 66, 1.0f, SILVER, 1.8f, 0.5f);
        b.a.line(x + 3, 28, x + 30, 28, 0.7f, rgba(BLACK, 70)); b.a.line(x + 3, 68, x + 30, 68, 0.7f, rgba(BLACK, 70));
        for (float yy : { 19.0f, 77.0f }) for (float xx : { x + 3, x + 30 }) b.a.dot(xx, yy, 1.0f, SILVER_D);
    }
    b.a.line(48, 15, 48, 81, 1.4f, rgba(BLACK, 170));                                    // the seam
    b.a.circle(48, 48, 6.0f, SILVER_D, 1.4f, 0.7f); b.a.ring(48, 48, 4.2f, 0.6f, rgba(BLACK, 130), 0.6f, 0); b.a.dot(48, 48, 1.4f, SILVER_L);   // locking wheel
    trefoil(b, 31, 46, 5.5f);
    if (cy) for (float x : { 7.0f, W - 7.0f }) for (float y : { 7.0f, H - 7.0f }) b.light(x, y, 0.9f, C_CYAN, 110);   // status lights
    else b.rust(10, 10, 86, 86, 7, 40);                                                   // weathered lip
    b.mark(30, 84.2f, 36, 2.2f);                                                          // team mark on the lip
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, cy ? 113u : 117u);
}

// ================================================================= NEUTRAL CIVILIAN STRONGHOLDS (map landmarks)
// Weathered stone and old concrete rather than either army's clean materials: the castle, the civilian shelter and the watchtower were here
// before the war. They carry a plain flag (the team mask) so whoever garrisons one sees it change colour, but no other army markings.
const Color N_STONE = rgb(166, 160, 142), N_STONE_D = rgb(108, 104, 92), N_STONE_L = rgb(204, 198, 178);
const Color N_MOSS = rgb(92, 112, 62), N_MOSS_D = rgb(62, 80, 42), N_SLATE = rgb(104, 102, 96);
const Color N_WOOD = rgb(118, 90, 62), N_WARM = rgb(255, 210, 126), N_GLASS = rgb(38, 52, 48);

// Ashlar courses follow the visible wall rather than a grid laid over the roof.
void stoneCourses(Bld& b, float x, float y, float w, float h, float pitch = 9) {
    for (int row = 0; row * 5 < h; row++) {
        float yy = y + row * 5;
        b.a.line(x, yy, x + w, yy, 0.45f, rgba(N_STONE_D, 130));
        for (float xx = x + (row & 1 ? pitch * 0.5f : pitch); xx < x + w; xx += pitch)
            b.a.line(xx, yy, xx, std::min(yy + 5, y + h), 0.45f, rgba(N_STONE_D, 105));
    }
}

void castle(Bld& b) {
    ground(b, rgb(105, 117, 79), 0.12f);
    b.a.rect(12, 17, 104, 100, 4, rgb(148, 140, 119), 2.3f, 0.5f);
    // A recessed bailey, with the gate-to-keep path left clear.
    for (int row = 0; row < 11; row++) for (int col = 0; col < 10; col++) {
        float x = 15 + col * 10 + (row & 1 ? 4 : 0), y = 22 + row * 8;
        b.a.rect(x, y, 8.7f, 6.7f, 0.4f, shade(N_STONE, b.rng.f(0.82f, 1.03f)), 0.55f, 0.25f);
    }
    auto curtain = [&](float x, float y, float w, float h, float ht) {
        Roof r = b.block(x, y, w, h, ht, N_STONE_L, N_STONE);
        stoneCourses(b, x, r.y + r.h, w, ht);
        for (float xx = x + 2; xx < x + w - 3; xx += 8) b.a.rect(xx, r.y - 2, 4.8f, 5, 0.5f, N_STONE_L, 1.0f, 0.5f);
    };
    curtain(18, 20, 92, 15, 18);
    b.block(16, 25, 15, 77, 15, N_STONE_L, N_STONE);
    b.block(97, 25, 15, 77, 15, N_STONE_L, N_STONE);
    auto tower = [&](float cx, float bottom, bool banner) {
        float r = 11, ht = 31;
        P top = b.cyl(cx, bottom, r, ht, N_STONE_L, N_STONE);
        b.a.ell(top.x, top.y, 8.3f, 5.8f, N_STONE_D, 0.8f, 0.2f);
        b.a.ell(top.x, top.y, 5.9f, 3.8f, rgb(127, 124, 109), 0.4f, 0.1f);
        for (int k = 0; k < 8; k++) {
            float an = k * 0.7854f;
            b.a.rect(cx + std::cos(an) * r * 0.82f - 2, top.y + std::sin(an) * r * 0.56f - 2, 4, 4, 0.4f, N_STONE_L, 0.8f, 0.4f);
        }
        for (float yy = bottom - ht + 8; yy < bottom - 3; yy += 8) b.a.rect(cx - 1.1f, yy, 2.2f, 4.8f, 0.4f, N_GLASS, 0.6f, 0.15f);
        b.a.line(cx + 6, bottom - 18, cx + 6, bottom - 4, 0.8f, rgba(N_MOSS, 130));
        if (banner) flag(b, cx + 4, top.y + 1, 16, 9, 5);
    };
    tower(17, 29, false); tower(111, 29, false);
    // A slate-roofed keep makes a distinct silhouette above the curtain wall.
    Roof keep = b.block(39, 36, 50, 34, 27, N_STONE_L, N_STONE);
    stoneCourses(b, 39, keep.y + keep.h, 50, 27, 10);
    b.a.poly({{36, 12}, {64, -5}, {92, 12}, {92, 25}, {36, 25}}, rgb(70, 91, 100), 2.0f, 0.6f);
    for (float yy = 9; yy < 25; yy += 4) b.a.line(38, yy, 90, yy, 0.6f, rgba(rgb(160, 171, 167), 85));
    b.a.line(37, 12, 64, -5, 1.3f, rgb(153, 164, 161));
    for (float x : {47.0f, 63.0f, 79.0f}) {
        b.a.circle(x, 49, 3.2f, N_STONE_D, 0.6f, 0.2f);
        b.a.rect(x - 3.2f, 49, 6.4f, 9, 0.4f, N_GLASS, 0.6f, 0.1f);
        b.a.line(x, 48, x, 57, 0.6f, N_STONE_L);
    }
    b.block(43, 61, 6, 12, 15, N_STONE_L, N_STONE_D);
    b.block(79, 61, 6, 12, 15, N_STONE_L, N_STONE_D);
    flag(b, 72, 8, 26, 13, 7);
    b.mark(54, 62, 20, 4);
    // Foreground curtain, two round towers and an arched, barred entrance.
    curtain(20, 105, 31, 12, 12); curtain(77, 105, 31, 12, 12);
    b.block(49, 98, 30, 19, 24, N_STONE_L, N_STONE);
    stoneCourses(b, 49, 93, 30, 24, 8);
    b.a.circle(64, 101, 8, N_STONE_D, 0.8f, 0.2f);
    b.a.rect(56, 101, 16, 16, 0.5f, N_WOOD, 1, 0.4f);
    for (float x = 58; x < 72; x += 3) b.a.line(x, 98, x, 116, 0.7f, rgb(53, 53, 45));
    b.a.line(56, 107, 72, 107, 0.9f, N_STONE_D);
    for (float x = 51; x < 77; x += 7) b.a.rect(x, 73, 4.5f, 5, 0.5f, N_STONE_L, 0.9f, 0.4f);
    tower(17, 110, true); tower(111, 110, true);
    // A low well and one surviving tree read as a lived-in courtyard.
    b.a.ell(37, 83, 7, 5, N_STONE_D, 1, 0.3f);
    b.a.ell(37, 82, 4.5f, 3, rgb(39, 53, 53), 0.7f, 0.2f);
    b.a.cap(31, 82, 31, 72, 0.8f, N_WOOD); b.a.cap(43, 82, 43, 72, 0.8f, N_WOOD);
    b.a.line(30, 72, 44, 72, 1.0f, N_WOOD);
    tree(b, civPal(false), 91, 85, 6);
    b.a.grain(0, 0, (float)b.W, (float)b.H, 0.065f, 911u);
}

// Small, usable map buildings share the old settlement's stone/brick palette.
void cottage(Bld& b) {
    CivPal p = civPal(false); p.wall = rgb(183, 169, 141); p.wallD = N_STONE_D;
    p.roof = rgb(108, 111, 103); p.roofD = rgb(70, 78, 76); p.lawn = rgb(112, 126, 80);
    house(b, p);
    b.a.line(8, 26, 56, 26, 0.6f, rgba(N_MOSS, 130));
}
void workshop(Bld& b) {
    ground(b, rgb(134, 126, 105), 0.11f);
    b.block(7, 21, 82, 32, 17, rgb(109, 112, 104), rgb(150, 109, 83));
    for (float x = 8; x < 86; x += 26) {
        b.a.poly({{x, 6}, {x + 20, -1}, {x + 24, 6}, {x + 24, 28}, {x, 28}}, rgb(92, 103, 101), 1.2f, 0.5f);
        b.a.rect(x + 3, 6, 15, 5, 0.4f, N_GLASS, 0.6f, 0.2f);
        for (float y = 15; y < 28; y += 4) b.a.line(x + 1, y, x + 23, y, 0.5f, rgba(N_STONE_L, 75));
    }
    b.door(55, 53, 24, 17, N_STONE_D, rgb(61, 69, 66));
    for (float y = 39; y < 53; y += 3) b.a.line(44, y, 66, y, 0.5f, rgba(N_STONE_L, 110));
    b.a.rect(15, 38, 17, 7, 0.5f, N_GLASS, 0.7f, 0.2f);
    b.a.line(23, 38, 23, 45, 0.7f, N_STONE_L);
    b.mark(13, 48, 17, 3); b.crate(76, 55, 10, 7, N_WOOD);
    b.a.rect(45, 56, 24, 4, 0.5f, N_STONE_L, 0.8f, 0.4f);
    flag(b, 83, 23, 17, 9, 5);
}

void civilianBunker(Bld& b) {
    ground(b, rgb(102, 106, 76), 0.14f);
    b.a.ell(48, 42, 46, 29, rgb(96, 112, 72), 3.5f, 0.4f);
    for (float x : {14.0f, 31.0f, 65.0f, 82.0f})
        b.a.rect(x - 3, 43, 6, 32, 0.8f, rgb(138, 136, 118), 1.0f, 0.4f);
    b.a.rect(8, 22, 80, 62, 6, rgb(104, 100, 88), 2.2f, 0.45f);   // concrete apron
    // low, thick shelter with a sloped blast roof: two blocks sharing a base line
    Roof low = b.block(12, 36, 72, 38, 22, rgb(154, 150, 134), rgb(126, 122, 108));
    Roof top = b.block(22, 34, 52, 28, 38, rgb(172, 166, 148), rgb(142, 138, 120));
    b.a.poly({{18, 2}, {26, -5}, {72, -5}, {78, 2}, {78, 24}, {18, 24}}, rgb(158, 157, 140), 1.8f, 0.5f);
    b.a.line(26, -5, 72, -5, 1.1f, N_STONE_L);
    for (float x = top.x + 4; x < top.x + top.w - 4; x += 11) b.a.rect(x, top.y + 2, 6, 4, 0.5f, rgb(120, 116, 104), 0.9f, 0.4f);   // roof ribs
    b.a.circle(70, top.y + 8, 7, rgb(142, 138, 124), 1.4f, 0.45f);   // ventilation cap
    b.a.circle(70, top.y + 8, 4.5f, rgb(30, 34, 32), 1.0f, 0.3f);
    b.vent(34, top.y + 7, 9, 5, rgb(78, 80, 74));
    for (float x = 20; x < 78; x += 11) { b.a.rect(x, low.y + low.h + 3, 7, 3, 0.5f, N_GLASS, 0.8f, 0.2f); }   // firing ports
    b.a.rect(42, low.y + low.h + 1, 14, 11, 0.6f, rgb(62, 66, 62), 1.2f, 0.4f);   // blast door vestibule
    b.door(49, low.y + low.h + 1, 10, 9, rgb(86, 90, 84), rgb(30, 34, 32));
    b.hazard(12, 74, 72, 3, 6, rgb(196, 176, 78), rgb(52, 52, 46));
    // sandbag ring, crates, a rusted drum and a radio mast
    b.sandbags(6, 84, 28, 84, 2); b.sandbags(68, 84, 90, 84, 2);
    b.crate(80, 58, 8, 7, rgb(126, 118, 96)); b.barrel(82, 70, 4, rgb(112, 92, 64));
    b.antenna(24, top.y + 10, 18, N_WARM);
    flag(b, 30, 32, 28, 12, 7);
    b.a.rect(10, 30, 4, 30, 0, rgba(N_MOSS, 50), 0, 0);
    b.rust(14, 40, 82, 76, 6, 30);
    b.a.grain(0, 0, (float)b.W, (float)b.H, 0.08f, 933u);
}

void watchtower(Bld& b) {
    ground(b, rgb(112, 122, 80), 0.14f);
    // stone shaft, timber gallery and a steep slate cap
    b.a.rect(20, 26, 24, 36, 3, rgb(154, 148, 130), 2.0f, 0.45f);   // foundation
    Roof shaft = b.block(22, 32, 20, 28, 20, N_STONE_L, N_STONE);
    b.wallSeams(shaft, 20, 7);
    b.a.rect(30, 42, 2.4f, 8, 0.5f, N_GLASS, 0.7f, 0.2f);
    b.a.rect(24, 50, 16, 3, 0.5f, N_STONE_D, 0.9f, 0.4f);   // string course
    b.block(17, 30, 30, 24, 44, N_WOOD, N_STONE);
    b.a.rect(20, 10, 24, 5, 0.5f, N_GLASS, 0.9f, 0.2f);   // gallery windows
    for (float x = 19; x < 47; x += 6) b.a.line(x, 14, x, 28, 0.55f, rgba(N_WOOD, 150));
    b.a.poly({{12, -8}, {32, -24}, {32, 5}}, rgb(111, 128, 133), 1.3f, 0.5f);
    b.a.poly({{32, -24}, {52, -8}, {32, 5}}, rgb(63, 81, 90), 1.3f, 0.5f);
    b.a.line(32, -24, 32, 5, 1.1f, rgb(164, 173, 169));   // pyramidal roof
    b.a.dot(32, -12, 2.4f, N_STONE_D);
    flag(b, 46, 8, 34, 12, 7);
    b.mark(22, 16, 10, 3);
    // ladder, sandbags and a little camp at the foot
    for (float y = 58; y < 84; y += 4) b.a.line(24, y, 32, y, 0.5f, rgba(N_WOOD, 200));
    b.a.line(24, 58, 24, 82, 0.6f, N_WOOD); b.a.line(32, 58, 32, 82, 0.6f, N_WOOD);
    b.sandbags(12, 82, 36, 82, 2); b.sandbags(36, 86, 52, 86, 1);
    b.crate(42, 70, 8, 7, rgb(126, 118, 96));
    tree(b, civPal(true), 8, 72, 5);
    b.a.ell(18, 34, 5, 10, rgba(N_MOSS, 50), 1, 0);
    b.a.grain(0, 0, (float)b.W, (float)b.H, 0.09f, 947u);
}

}  // namespace

bool artCivicBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role != BR_CIVIC && bt.role != BR_UNUKE && bt.role != BR_STRONGHOLD) return false;
    bool cy = bt.faction == F_CYBER;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, cy, 8000u + (u32)type * 31u);
    if (bt.role == BR_UNUKE) undergroundSilo(b);
    else if (bt.role == BR_STRONGHOLD) {
        if (type == B_C_CASTLE) castle(b);
        else if (type == B_C_CIVBUNKER) civilianBunker(b);
        else if (type == B_C_COTTAGE) cottage(b);
        else if (type == B_C_WORKSHOP) workshop(b);
        else watchtower(b);
    }
    else {
        CivPal p = civPal(cy);
        switch (civicKind(type)) {
        case CV_HOUSE:      house(b, p);       break;
        case CV_APARTMENTS: apartments(b, p);  break;
        case CV_SKYSCRAPER: skyscraper(b, p);  break;
        case CV_GAS:        gasStation(b, p);  break;
        case CV_GROCERY:    grocery(b, p);     break;
        case CV_EVENTHALL:  eventHall(b, p);   break;
        case CV_SQUARE:     townSquare(b, p);  break;
        case CV_TOWNHALL:   townHall(b, p);    break;
        case CV_GOVERNMENT: government(b, p);  break;
        default: break;
        }
    }
    c.outline(rgb(8, 10, 14, 150));
    return true;
}

// The warhead of an Underground Nuke, drawn live over the silo only while it is armed: the blast doors slid back into the lip, the dark shaft
// between them and a silver warhead rising out of it, with a faint glow (same canvas geometry as the structure).
void artSiloArmed(Canvas& c, int type) {
    const BuildType& bt = BUILDS[type];
    Canvas dummy(4, 4);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 4500u + (u32)type * 13u);
    b.a.rect(30, 15, 36, 66, 0.8f, rgb(6, 8, 10), 1.2f, 0.2f);                            // the shaft
    for (int s = 0; s < 2; s++) {                                                           // the leaves, slid back to the lip
        float x = s ? 66.0f : 15.0f;
        b.a.rect(x, 15, 15, 66, 1.0f, SILVER, 1.8f, 0.5f);
        b.a.line(x + 2, 28, x + 13, 28, 0.7f, rgba(BLACK, 70)); b.a.line(x + 2, 68, x + 13, 68, 0.7f, rgba(BLACK, 70));
    }
    b.a.glow(48, 64, 22, rgba(rgb(255, 228, 186), 70));                                    // faint warm glow from the shaft
    b.a.cap(48, 74, 48, 30, 5.6f, SILVER_L, 2.0f, 0.5f);                                    // warhead body
    b.a.rect(42.4f, 48, 11.2f, 2.6f, 0.4f, SILVER_D, 0.8f, 0.3f);                           // band
    b.a.poly({ {42.4f, 30.0f}, {53.6f, 30.0f}, {48.0f, 9.0f} }, SILVER, 1.8f, 0.5f);         // nose cone, above the rim
    for (float s : { -1.0f, 1.0f }) b.a.poly({ {48 + s * 4.0f, 66.0f}, {48 + s * 11.0f, 76.0f}, {48 + s * 4.0f, 76.0f} }, SILVER_D, 1.0f, 0.4f);   // tail fins
    c.outline(rgb(8, 10, 14, 150));
}
