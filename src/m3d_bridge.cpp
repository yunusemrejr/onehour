// One Hour - 3D model of the canal bridge: a 3 x 12 tile concrete deck over the canal, its piers and banks, and its two other states (the
// middle span snapped and lying in the water, and the dozers' scaffolding over the gap). It replaces the painted artBridge (artb_relic.cpp):
// same canvas, same anchor at the centre, and the deck covers the same tiles. Model units are the design pixels of that art (x across the
// deck, y along it, z up, the water at z = 0), rendered by the kit at BART_K canvas pixels per unit, so the water shows through the sprite.
#include "m3d_parts.h"
#include "artb.h"
#include <cmath>
#include <algorithm>

using namespace m3d;

namespace {

const Color cDeck = C(0x968C74), cRoad = C(0x8A8170), cRoadD = C(0x5E5646), cKerb = C(0xCEC4A8), cSeam = C(0x3E362C), cStencil = C(0xE4D8B2),
            cConc = C(0xA69A7E), cConcD = C(0x685F4E), cEarth = C(0x76684E), cRock = C(0x6E6A5E),
            cIron = C(0x78746A), cTimber = C(0x857055), cTimberD = C(0x5E4B33), cLamp = C(0xFFD27A), cPole = C(0x5A5850);

const float DT = 9.0f, DB = 6.0f;          // the deck's top and underside (z): 3 units thick, about 1.8 m over the water at the tanks' scale
const float RAIL = 4.5f;                   // railing height above the deck
const float PN = 112, PM = 192, PS = 272;  // the piers in the water beside the deck (y); PM stands in the snapped gap
const float G0 = 136, G1 = 216;            // the gap: the two cut ends of the snapped span
const float PI_F = 3.14159265f;

// a box from x0..x1, y0..y1, z0..z1
Prim& bx(Model& m, float x0, float x1, float y0, float y1, float z0, float z1, Color c) {
    return m.box((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f, (x1 - x0) * 0.5f, (y1 - y0) * 0.5f, (z1 - z0) * 0.5f, c);
}
// a round post standing at (x, y) from z0 to z1
Prim& post(Model& m, float x, float y, float z0, float z1, float r, Color c) { return m.cyl('z', x, y, (z0 + z1) * 0.5f, r, (z1 - z0) * 0.5f, c); }
// a bar from one point to another
Prim& bar(Model& m, float x0, float y0, float z0, float x1, float y1, float z1, float r, Color c) { return m.rod({ x0, y0, z0 }, { x1, y1, z1 }, r, c); }

// the deck's top over the water at plan y: flat, except where the snapped span hangs from its two hinges (state 1)
float deckZ(int state, float y) {
    if (state == 1) {
        if (y > PN && y < G0) return DT - (DT - 0.5f) * (y - PN) / (G0 - PN);
        if (y > G1 && y < PS) return 0.5f + (DT - 0.5f) * (y - G1) / (PS - G1);
    }
    return DT;
}

// a flat stretch of deck from y0 to y1: concrete walkways, the worn road between kerbs, a dashed centre line
void flatDeck(Model& m, float y0, float y1) {
    bx(m, 0, 96, y0, y1, DB, DT, cDeck).m(MATTE).t(T_PANEL, 12, cConcD).bev(0.4f);
    bx(m, 13, 83, y0, y1, DT, DT + 0.5f, cRoad).m(MATTE).bev(0.2f);
    for (float x : { 20.0f, 70.0f }) bx(m, x, x + 6, y0, y1, DT + 0.5f, DT + 0.56f, cRoadD).m(MATTE).bev(0);   // the wheel tracks, worn darker
    for (float x : { 11.0f, 83.0f }) bx(m, x, x + 2, y0, y1, DT, DT + 1.2f, cKerb).m(MATTE).bev(0.3f);
    for (float y = y0 + 4; y + 6 < y1; y += 14) bx(m, 47.2f, 48.8f, y, y + 6, DT + 0.5f, DT + 0.6f, cStencil).m(MATTE).bev(0);
}

// the expansion joint across the deck at y: a dark seam in the road and on the walkways
void joint(Model& m, float y) { bx(m, 0, 96, y - 0.8f, y + 0.8f, DT + 0.5f, DT + 0.7f, cSeam).m(MATTE).bev(0); }

// a snapped span: a deck piece hung from its hinge (y0, z0) and tilted down to its broken end (y1, z1); its top runs between the two points
void snapped(Model& m, float y0, float z0, float y1, float z1) {
    const float t = DT - DB, dy = y1 - y0, dz = z1 - z0, L = std::sqrt(dy * dy + dz * dz), th = std::atan2(dz, dy);
    const float cy = (y0 + y1) * 0.5f + std::sin(th) * t * 0.5f, cz = (z0 + z1) * 0.5f - std::cos(th) * t * 0.5f;
    m.box(48, cy, cz, 48, L * 0.5f, t * 0.5f, cDeck).m(MATTE).t(T_PANEL, 12, cConcD).bev(0.4f).rx(th * 180.0f / PI_F);
}

// a pier in the water beside the deck (its column from the water up, its cap reaching under the deck's edge), x0..x1 and centred on y
void pier(Model& m, float x0, float x1, float y, float top) {
    bx(m, x0, x1, y - 4, y + 4, 0, top - 1.5f, cConc).m(MATTE).bev(0.6f);
    bx(m, x0 - 1, x1 + 1, y - 5, y + 5, top - 1.5f, top, cConcD).m(MATTE).bev(0.5f);
}

// a boulder half sunk at the water's edge
void rock(Model& m, float x, float y, float r) { m.ball(x, y, r * 0.3f, r, r, r * 0.8f, cRock).m(MATTE).bev(0.5f); }

// a railing along the deck's edge at x: a post about every 12 units, the top and middle rails, all following the deck's height
void railing(Model& m, int state, float x, float y0, float y1) {
    const int n = std::max(1, (int)std::ceil((y1 - y0) / 12.0f));
    std::vector<float> ys;
    for (int i = 0; i <= n; i++) ys.push_back(y0 + (y1 - y0) * i / n);
    for (float b : { PN, PS }) if (b > y0 && b < y1) ys.push_back(b);
    std::sort(ys.begin(), ys.end());
    for (float y : ys) { const float z = deckZ(state, y); post(m, x, y, z, z + RAIL, 0.7f, cIron).m(METAL).bev(0.2f); }
    for (size_t i = 0; i + 1 < ys.size(); i++) {
        const float za = deckZ(state, ys[i]), zb = deckZ(state, ys[i + 1]);
        bar(m, x, ys[i], za + RAIL, x, ys[i + 1], zb + RAIL, 0.5f, cIron).m(METAL).bev(0);
        bar(m, x, ys[i], za + RAIL * 0.45f, x, ys[i + 1], zb + RAIL * 0.45f, 0.4f, cIron).m(METAL).bev(0);
    }
}

// a lamp on a pole at each side of the walkway, standing on the deck at its height
void lamps(Model& m, int state) {
    for (float y : { 60.0f, 150.0f, 244.0f, 336.0f }) {
        if (state != 0 && y > G0 - 6 && y < G1 + 6) continue;   // (no deck in the gap)
        for (float x : { 6.0f, 90.0f }) {
            const float z = deckZ(state, y);
            post(m, x, y, z, z + 10.0f, 0.6f, cPole).m(METAL).bev(0.2f);
            m.ball(x, y, z + 10.8f, 1.2f, 1.2f, 1.2f, cLamp).m(EMIT).f(F_NOSHADOW);
        }
    }
}

// a piece of the broken span lying in the water: a slab of concrete turned about x (rx) and then about z (rz), resting just above the water
void wreck(Model& m, float cx, float cy, float hx, float hy, float hz, float rzDeg, float rxDeg, float zBase, Color c) {
    const float a = rxDeg * PI_F / 180.0f;
    const float zc = zBase + std::fabs(hy * std::sin(a)) + hz * std::cos(a);
    m.box(cx, cy, zc, hx, hy, hz, c).m(MATTE).bev(0.5f).rz(rzDeg).rx(rxDeg);
}

// the snapped span's debris and the twisted rail that drops into the water with it
void debris(Model& m) {
    wreck(m, 34, 150, 14, 9, 1.6f, 20, -14, 0.8f, cDeck);
    wreck(m, 72, 176, 12, 10, 1.6f, -30, 9, 0.4f, cConc);
    wreck(m, 26, 198, 9, 7, 1.3f, 8, -6, 0.6f, cDeck);
    wreck(m, 60, 146, 6, 4, 1.2f, 55, 18, 0.3f, cConcD);
    wreck(m, 48, 190, 3.5f, 3, 1.0f, 30, 0, 0.2f, cConcD);
    for (float x : { 2.0f, 94.0f }) {
        const float s = x < 50 ? -1.0f : 1.0f;
        bar(m, x, G0 - 1, 0.5f + RAIL, x + s * 3, G0 + 4, 3.5f, 0.5f, cIron).m(METAL).bev(0);
        bar(m, x + s * 3, G0 + 4, 3.5f, x + s * 8, G0 + 12, 0.8f, 0.5f, cIron).m(METAL).bev(0);
        bar(m, x, G1 + 1, 0.5f + RAIL, x + s * 3, G1 - 4, 3.5f, 0.5f, cIron).m(METAL).bev(0);
        bar(m, x + s * 3, G1 - 4, 3.5f, x + s * 8, G1 - 12, 0.8f, 0.5f, cIron).m(METAL).bev(0);
    }
}

// the dozers' works over the gap: two steel girders on three trestle frames standing in the water, timber planks laid over them as far
// as y 200 and the scaffold guard rails and work lights along the planked part
void repairWorks(Model& m) {
    for (float yt : { 148.0f, 180.0f, 212.0f }) {
        for (float x : { 8.0f, 84.0f }) for (float s : { -2.5f, 2.5f }) post(m, x, yt + s, 0, DT, 0.6f, cIron).m(METAL).bev(0.1f);
        for (float z : { 4.5f, 8.2f }) for (float s : { -2.5f, 2.5f }) bar(m, 8, yt + s, z, 84, yt + s, z, 0.45f, cIron).m(METAL).bev(0);
        bar(m, 8, yt - 2.5f, 4.5f, 8, yt + 2.5f, 8.2f, 0.4f, cTimberD).m(MATTE).bev(0);   // timber bracing
        bar(m, 84, yt - 2.5f, 4.5f, 84, yt + 2.5f, 8.2f, 0.4f, cTimberD).m(MATTE).bev(0);
    }
    for (float x : { 10.0f, 82.0f }) m.box(x, (G0 + G1) * 0.5f, DT + 0.9f, 2.0f, (G1 - G0) * 0.5f, 0.9f, cIron).m(METAL).bev(0.2f);
    for (float y = G0 + 1; y < 200; y += 3.4f) bx(m, 0, 96, y, y + 2.6f, DT + 1.8f, DT + 2.6f, ((int)(y / 3.4f) & 1) ? cTimber : cTimberD).m(MATTE).t(T_PLANKS, 2.2f, cTimberD).bev(0.3f);
    for (float y = G0 + 2; y < 200; y += 8.0f) for (float x : { 2.0f, 94.0f }) {
        post(m, x, y, DT + 2.6f, DT + 2.6f + RAIL, 0.6f, cIron).m(METAL).bev(0.2f);
    }
    for (float x : { 2.0f, 94.0f }) for (float z : { DT + 2.6f + RAIL, DT + 2.6f + RAIL * 0.5f })
        bar(m, x, G0 + 2, z, x, 200, z, 0.45f, cIron).m(METAL).bev(0);
    for (float y : { 142.0f, 196.0f }) {
        post(m, 88, y, DT + 2.6f, DT + 11.0f, 0.5f, cPole).m(METAL).bev(0.1f);
        m.ball(88, y, DT + 11.8f, 1.3f, 1.3f, 1.3f, cLamp).m(EMIT).f(F_NOSHADOW);
    }
}

void bridgeModel(Model& m, int state) {
    m.dirt = 0.2f;
    // the banks: the abutment walls at the water's edge that the deck rests on, the earth behind them, boulders at the foot
    bx(m, -8, 104, 0, 22, 0, DB, cEarth).m(MATTE).bev(0.5f);
    bx(m, -8, 104, 22, 33, 0, DB, cConc).m(MATTE).t(T_BRICK, 5, cConcD).bev(0.4f);
    bx(m, -8, 104, 352, 362, 0, DB, cConc).m(MATTE).t(T_BRICK, 5, cConcD).bev(0.4f);
    bx(m, -8, 104, 362, 384, 0, DB, cEarth).m(MATTE).bev(0.5f);
    rock(m, -12, 38, 2.6f); rock(m, -3, 37, 2.2f); rock(m, 100, 37, 2.4f); rock(m, 108, 40, 3.0f);
    rock(m, -10, 350, 2.8f); rock(m, 104, 352, 2.4f);

    // the piers stand in the water beside the deck; the one in the snapped gap is only a stump left standing
    for (float y : { PN, PS }) { pier(m, -9, -1, y, DB); pier(m, 97, 105, y, DB); }
    const float top = state == 0 ? DB : 3.0f;
    pier(m, -9, -1, PM, top); pier(m, 97, 105, PM, top);

    // the deck
    if (state == 0) {
        flatDeck(m, 0, 384);
        for (float y : { PN, PM, PS }) joint(m, y);
        railing(m, 0, 2.0f, 2.0f, 382.0f); railing(m, 0, 94.0f, 2.0f, 382.0f);
    } else if (state == 1) {
        flatDeck(m, 0, PN); snapped(m, PN, DT, G0, 0.5f);
        snapped(m, G1, 0.5f, PS, DT); flatDeck(m, PS, 384);
        joint(m, PN); joint(m, PS);
        for (float x : { 2.0f, 94.0f }) { railing(m, 1, x, 2.0f, G0); railing(m, 1, x, G1, 382.0f); }
        debris(m);
    } else {
        flatDeck(m, 0, G0); flatDeck(m, G1, 384);
        joint(m, PN); joint(m, PS);
        for (float x : { 2.0f, 94.0f }) { railing(m, 2, x, 2.0f, G0); railing(m, 2, x, G1, 382.0f); }
        debris(m);
        repairWorks(m);
    }
    lamps(m, state);
}

}  // namespace

void m3dBridge(Canvas& c, int state) {
    Model m;
    bridgeModel(m, state < 0 ? 0 : (state > 2 ? 2 : state));
    RenderOpt o;
    o.scale = (float)BART_K;   // canvas pixels per design unit, as the painted art
    o.ss = 2;
    std::vector<Shot> shots{ Shot{ &m, o, {} } };
    bake(shots);   // rendered with the kit's lighting and print grade, like every other model sprite
    const Baked& b = shots[0].out;
    // the model's origin (the deck's north-west corner, design (0, 0)) sits at the art's margin: 8 design px in from the canvas edge
    const int ox = BART_K * 8 - (int)std::lround(b.ox), oy = BART_K * 8 - (int)std::lround(b.oy);
    for (int y = 0; y < b.color.h; y++) for (int x = 0; x < b.color.w; x++) {
        const u32 p = b.color.px[(size_t)y * b.color.w + x];
        if ((p >> 24) == 0) continue;
        const int dx = x + ox, dy = y + oy;
        if (dx < 0 || dy < 0 || dx >= c.w || dy >= c.h) continue;
        u32& d = c.px[(size_t)dy * c.w + dx];
        if ((d >> 24) == 0) d = p; else c.set(dx, dy, Canvas::unpack(p));
    }
}
