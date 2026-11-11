// One Hour - 3D models: the navy. Destroyers, missile cruisers, patrol boats, battleships, nuclear submarines, aircraft carriers and cargo ships.
// Old 2D painters: art_ship.cpp (destroyer / cruiser / patrol boat / battleship / submarine) and art_ocean.cpp (carrier / cargo ship).
// Ships ride the waterline: z = 0 is the water, nothing goes below it, and the hull is modelled only above the waterline. Coordinates are the old
// canvas pixels minus 32 (minus 64 for the 128 px ships), so every ship keeps the old hull length and beam.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// ---------------------------------------------------------------- ship palettes (in the spirit of pal::)
struct ShipPal {
    Color hull, deck, pale, dark, mid, glow, trim, glass, steel, rust, haz, foam, wood;
};
// Cyber: pale ceramic plate over graphite, deep cobalt trim, cyan lights, faceted stealth shapes.
const ShipPal CY = {
    C(0xAEB9C6),   // hull side: pale ceramic plate
    C(0xC8D2DC),   // deck: brighter ceramic
    C(0xD5DCE4),   // pale superstructure
    C(0x2A2F3A),   // graphite recesses, barrels, dark trim
    C(0x5E6C80),   // mid cool grey
    C(0x58ECFF),   // cyan light
    C(0x2C5AA0),   // cobalt trim band
    C(0x123A4C),   // dark glass
    C(0x8E9AA8),   // steel
    C(0x6B7588),   // (no rust: a weathered grey)
    C(0x3F6FB5),   // deck paint seam (cobalt light)
    C(0xE8F0F6),   // foam / white work
    C(0x9AA6B4)    // wood / dunnage
};
// Clanker: dark grey-olive steel, rust streaks, boxy superstructure, funnels, amber lights, hazard yellow.
const ShipPal CK = {
    C(0x5A6148),   // hull side: grey-olive steel
    C(0x6E7458),   // deck
    C(0x8B8E84),   // superstructure plate
    C(0x2C2B28),   // dark steel, barrels
    C(0x55585A),   // mid steel
    C(0xFFB23E),   // amber light
    C(0x8A4A2B),   // rust band
    C(0x2A3A34),   // dark glass
    C(0x9AA0A0),   // bright steel
    C(0x8A4A2B),   // rust
    C(0xE6BA3C),   // hazard yellow
    C(0xD8DCD6),   // foam / white work
    C(0x8B7A52)    // wood deck / dunnage
};

// ---------------------------------------------------------------- shared ship parts
// a plan outline with a raked bow: half-beam stations (x, hw) stern -> bow, chamfered transom corners and a cut-back bow tip.
// Always convex for -x1 < x1 and a monotonic half-beam, so it makes one clean solid.
struct ShipShape { float xs, xe, ys, ye, x1, h1, xb, hb, tip; };
inline std::vector<P2> bowPlan(const ShipShape& S) {
    return { {S.xs, -S.ys}, {S.x1, -S.h1}, {S.xe, -S.ye}, {S.xe, S.ye}, {S.x1, S.h1}, {S.xs, S.ys} };
}
inline std::vector<P2> tipPlan(const ShipShape& S) {
    return { {S.x1, -S.h1}, {S.xb, -S.hb}, {S.tip, 0.0f}, {S.xb, S.hb}, {S.x1, S.h1} };
}
// one hull stage: the pointed-bow plan outline extruded straight up (two convex solids)
inline void hullStage(Model& m, const ShipShape& S, float z0, float z1, Color col, Tex tx, float texS, Color col2, float bev) {
    m.plan(bowPlan(S), z0, z1, col).m(PAINT).t(tx, texS, col2).bev(bev);
    m.plan(tipPlan(S), z0, z1, col).m(PAINT).t(tx, texS, col2).bev(bev);
}
// the whole hull with a flare: a wide lower stage and the topsides above it cut back by `in`
inline void hullSides(Model& m, const ShipShape& S, float z0, float height, float in, Color col, Tex tx, float texS, Color col2) {
    const float zm = z0 + height * 0.48f;
    ShipShape U = S;
    U.xs += in * 0.6f; U.xe -= in * 0.5f; U.ys = std::max(0.4f, S.ys - in); U.ye = std::max(0.3f, S.ye - in);
    U.x1 -= in * 0.4f; U.h1 = std::max(0.6f, S.h1 - in); U.xb -= in * 0.6f; U.hb = std::max(0.25f, S.hb - in); U.tip -= in * 0.9f;
    hullStage(m, S, z0, zm, col, tx, texS, col2, 0.5f);
    hullStage(m, U, zm, z0 + height, col, tx, texS, col2, 0.6f);
}
// the boot-top at the waterline: the same outline a hair larger in plan, so it wraps the hull's lower stage without z-fighting
inline void bootBand(Model& m, const ShipShape& S, float z1, Color col) {
    ShipShape B = S;
    const float k = 0.3f;
    B.xs -= k; B.xe += k * 0.5f; B.ys += k; B.ye += k; B.h1 += k; B.xb += k; B.hb += k * 0.6f; B.tip += k * 0.6f;
    hullStage(m, B, 0.0f, z1, col, T_NONE, 4.0f, col, 0.3f);
}
// the deck: the same shape inset by `in`, sitting on top of the hull sides
inline void deckPlan(Model& m, const ShipShape& S, float in, float z0, float thickness, Color col) {
    ShipShape D = S;
    D.xs += in; D.xe -= in; D.ys = std::max(0.3f, S.ys - in); D.ye = std::max(0.2f, S.ye - in);
    D.x1 -= in * 0.7f; D.h1 = std::max(0.5f, S.h1 - in); D.xb -= in; D.hb = std::max(0.2f, S.hb - in);
    D.tip -= in * 1.15f;
    m.plan(bowPlan(D), z0, z0 + thickness, col).m(PAINT).bev(0.45f);
    m.plan(tipPlan(D), z0, z0 + thickness, col).m(PAINT).bev(0.45f);
}
// a hull from a chain of half-beam stations (x, hw), stern -> bow: one convex quad between each pair of stations, and a triangle
// where the chain ends in a point (hw = 0 at the last station). This is how the long 128 px ships keep their exact old outline.
struct WS { float x, hw; };
inline void hullChain(Model& m, const std::vector<WS>& st, float z0, float z1, Color col, Tex tx, float texS, Color col2, float bev) {
    for (size_t i = 1; i < st.size(); i++) {
        const WS a = st[i - 1], b = st[i];
        std::vector<P2> q;
        q.push_back({ a.x, -a.hw });
        if (b.hw > 0.05f) q.push_back({ b.x, -b.hw });
        q.push_back({ b.x, b.hw });
        q.push_back({ a.x, a.hw });
        m.plan(q, z0, z1, col).m(PAINT).t(tx, texS, col2).bev(bev);
    }
}
// the same chain, inset: the ends pulled back by `in`, every half-beam cut by `in` (a bow tip stays a tip)
inline std::vector<WS> insetChain(const std::vector<WS>& st, float in) {
    std::vector<WS> o;
    for (size_t i = 0; i < st.size(); i++) {
        const WS s = st[i];
        const float x = s.x + (i == 0 ? in : 0.0f) - (i + 1 == st.size() ? in * 1.2f : 0.0f);
        o.push_back({ x, std::max(0.0f, s.hw - in) });
    }
    return o;
}
inline void deckChain(Model& m, const std::vector<WS>& st, float in, float z0, float thickness, Color col) {
    hullChain(m, insetChain(st, in), z0, z0 + thickness, col, T_NONE, 4.0f, col, 0.45f);
}
// an empty mount: a low ring the game's turret model stands on, with a dark recess
inline void barbette(Model& m, float x, float y, float deck, float r, Color col, Color dk) {
    m.cyl('z', x, y, deck + 0.4f, r, 0.4f, col).m(PAINT).bev(0.3f);
    m.cyl('z', x, y, deck + 0.65f, r * 0.68f, 0.22f, dk).m(METAL).bev(0.2f);
}
// a close-in gun: a small mount and a stub barrel pointing at the bow
inline void ciws(Model& m, float x, float y, float z, const ShipPal& P, bool cy) {
    m.cyl('z', x, y, z + 0.8f, 1.2f, 0.8f, cy ? P.pale : P.steel).m(PAINT).bev(0.35f);
    m.rod({ x + 0.6f, y, z + 1.4f }, { x + 4.2f, y, z + 1.8f }, 0.38f, P.dark).m(METAL).bev(0.15f);
}
// bulwark railings along a ship's edge: one thin rail per straight run plus a few stanchions.
// A run of two rails that share a y at a corner is not convex, so each part is its own solid and the raked part is turned.
struct Rail { float x, y; };
inline void railRun(Model& m, const std::vector<Rail>& pts, float z, Color col, Color dk, float gap) {
    for (size_t i = 1; i < pts.size(); i++) {
        const float ax = pts[i - 1].x, ay = pts[i - 1].y, bx = pts[i].x, by = pts[i].y;
        const float dx = bx - ax, dy = by - ay, L = std::hypot(dx, dy);
        if (L < 1e-3f) continue;
        const float ang = std::atan2(dy, dx) * 57.29578f;
        m.box(ax + dx * 0.5f, ay + dy * 0.5f, z + 1.15f, L * 0.5f, 0.16f, 0.16f, col).m(METAL).rz(ang).bev(0.15f);
    }
    float len = 0;
    for (size_t i = 1; i < pts.size(); i++) len += std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
    const int n = (int)(len / gap);
    for (int k = 0; k <= n; k++) {
        float t = n > 0 ? k / (float)n : 0.0f, acc = t * len, x = pts[0].x, y = pts[0].y;
        for (size_t i = 1; i < pts.size(); i++) {
            const float sl = std::hypot(pts[i].x - pts[i - 1].x, pts[i].y - pts[i - 1].y);
            if (acc <= sl || i + 1 == pts.size()) { const float u = sl > 1e-4f ? acc / sl : 0.0f; x = pts[i - 1].x + (pts[i].x - pts[i - 1].x) * u; y = pts[i - 1].y + (pts[i].y - pts[i - 1].y) * u; break; }
            acc -= sl;
        }
        m.box(x, y, z + 0.6f, 0.24f, 0.24f, 0.6f, dk).m(METAL).bev(0.12f);
    }
}
inline void bothRails(Model& m, const std::vector<Rail>& pts, float z, const ShipPal& P, float gap) {
    railRun(m, pts, z, P.mid, P.dark, gap);
    std::vector<Rail> q;
    for (const Rail& r : pts) q.push_back({ r.x, -r.y });
    railRun(m, q, z, P.mid, P.dark, gap);
}
// a vertical-launch block: a dark frame with cols x rows cell lids on the deck
inline void vls(Model& m, float x0, float y0, float pitchX, float pitchY, int cols, int rows, float z, const ShipPal& P, bool cy) {
    const float w = cols * pitchX, d = rows * pitchY;
    m.box(x0 + w * 0.5f, y0 + d * 0.5f, z + 0.5f, w * 0.5f + 0.4f, d * 0.5f + 0.4f, 0.5f, P.dark).m(PAINT).t(T_PANEL, 2.4f).bev(0.25f);
    for (int j = 0; j < rows; j++) for (int i = 0; i < cols; i++)
        m.box(x0 + pitchX * (i + 0.5f), y0 + pitchY * (j + 0.5f), z + 1.1f, pitchX * 0.36f, pitchY * 0.36f, 0.25f,
              cy ? P.pale : P.steel).m(PAINT).bev(0.2f);
}
// a deck marking: an outline square frame of very thin bars (a parking box, a helipad ring bar)
inline void deckFrame(Model& m, float cx, float cy, float w, float d, float z, Color col, float thick, float ang) {
    m.box(cx, cy - d * 0.5f, z, w * 0.5f, thick * 0.5f, 0.1f, col).m(PAINT).rz(ang).bev(0);
    m.box(cx, cy + d * 0.5f, z, w * 0.5f, thick * 0.5f, 0.1f, col).m(PAINT).rz(ang).bev(0);
    m.box(cx - w * 0.5f, cy, z, thick * 0.5f, d * 0.5f, 0.1f, col).m(PAINT).bev(0);
    m.box(cx + w * 0.5f, cy, z, thick * 0.5f, d * 0.5f, 0.1f, col).m(PAINT).bev(0);
}
// an inflatable raft / lifeboat canister on a bracket
inline void raftCan(Model& m, float x, float y, float z, Color col) {
    m.box(x, y, z + 0.25f, 1.5f, 1.0f, 0.25f, col).m(MATTE).bev(0.3f);
    m.box(x, y, z + 0.7f, 0.4f, 1.25f, 0.25f, col).m(MATTE).bev(0.2f);
}
// a propeller shaft and blade hub at the transom (a small blade disc, as the old art shows the screws)
inline void screw(Model& m, float x, float y, float z, Color dk, Color steel) {
    m.rod({ x, y, z }, { x - 2.0f, y, z }, 0.3f, steel).m(METAL).bev(0);
    m.cyl('x', x - 2.4f, y, z, 1.5f, 0.22f, dk).m(METAL).bev(0.2f);
}

// ================================================================ DESTROYER (about 60 x 17 px hull, old canvas x 3..62.4)
// Deck 4.6 along the centre line; the game draws its one gun mount at model x = +12 (the old forecastle gun) and x = -15 (the old stern gun).

void destroyerHull(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const ShipShape H = { -29.0f, 26.4f, 7.5f, 7.2f, 18.0f, 5.6f, 23.0f, 3.2f, 30.4f };
    hullSides(b, H, 0.0f, 4.6f, 1.0f, P.hull, T_PANEL, 6.0f, P.mid);
    deckPlan(b, H, 1.6f, 4.6f, 0.45f, P.deck);
    bootBand(b, H, 1.05f, P.dark);                                              // the boot-top at the waterline
    b.plan({ {24.0f, 2.6f}, {30.0f, 1.4f}, {33.0f, 0.0f}, {30.0f, -1.4f}, {24.0f, -2.6f} }, 0.0f, 0.3f, P.foam).m(MATTE).bev(0.2f);   // the bow wave
    // the owner's colour: a stripe along both deck edges and a mark on the bridge roof
    for (int s = -1; s <= 1; s += 2) b.box(-6.0f, s * 6.1f, 5.2f, 10.0f, 0.5f, 0.2f, OWNER).m(PAINT).bev(0.15f);
    b.box(0.0f, 0.0f, cy ? 12.75f : 12.0f, 2.2f, 2.6f, 0.2f, OWNER).m(PAINT).bev(0.1f);   // the bridge roof panel
    // hawse, anchor and bow cleats, engine louvres and deck vents aft
    b.box(24.6f, 3.0f, 4.3f, 0.7f, 0.7f, 0.4f, P.dark).m(METAL).bev(0.2f);
    b.cyl('z', 20.0f, 4.2f, 5.05f, 0.6f, 0.25f, P.dark).m(METAL).bev(0.2f);
    b.cyl('z', 20.0f, -4.2f, 5.05f, 0.6f, 0.25f, P.dark).m(METAL).bev(0.2f);
    b.box(-23.0f, 0.0f, 5.15f, 2.8f, 4.2f, 0.3f, P.mid).m(MATTE).t(T_GRILLE, 1.5f).bev(0.2f);
    b.box(-19.5f, 3.4f, 5.05f, 1.4f, 1.0f, 0.4f, P.mid).m(METAL).bev(0.25f);
    b.box(-19.5f, -3.4f, 5.05f, 1.4f, 1.0f, 0.4f, P.mid).m(METAL).bev(0.25f);
    // the helicopter deck aft: a ring and its H
    b.cyl('z', -21.0f, 0.0f, 5.06f, 4.2f, 0.08f, cy ? P.pale : P.haz).m(PAINT).bev(0);
    for (int s = -1; s <= 1; s += 2) b.box(-21.0f + s * 1.4f, 0.0f, 5.2f, 0.4f, 1.9f, 0.1f, P.dark).m(PAINT).bev(0);
    b.box(-21.0f, 0.0f, 5.2f, 1.4f, 0.4f, 0.1f, P.dark).m(PAINT).bev(0);
    // vertical-launch fields fore and aft of the bridge (old canvas x 25.6 and 38.6)
    vls(b, 5.6f, -3.3f, 2.2f, 3.0f, 3, 1, 5.05f, P, cy);
    vls(b, -8.6f, -3.3f, 2.2f, 3.0f, 3, 1, 5.05f, P, cy);
    // the bridge: Cyber a faceted stealth tower with a glass band; Clanker a boxy stack with funnels and a lattice mast
    if (cy) {
        b.taper(0.0f, 0.0f, 5.05f, 5.4f, 6.3f, 4.6f, 4.0f, 4.7f, P.pale, 0.6f, 0.0f).m(PAINT).t(T_PANEL, 4.0f).bev(0.7f);
        b.taper(0.0f, 0.0f, 9.6f, 3.6f, 4.4f, 3.0f, 2.4f, 3.0f, P.pale, 0.5f, 0.0f).m(PAINT).t(T_PANEL, 3.4f).bev(0.6f);
        b.box(2.6f, 0.0f, 8.0f, 1.1f, 4.4f, 0.9f, P.glass).m(GLASS).bev(0.3f);
        b.box(2.2f, 0.0f, 11.3f, 0.9f, 2.8f, 0.8f, P.glass).m(GLASS).bev(0.3f);
        b.cyl('z', -1.0f, 0.0f, 12.9f, 1.9f, 0.6f, P.mid).m(PAINT).bev(0.3f);            // sensor drum
        b.dome(-1.0f, 0.0f, 13.4f, 1.5f, 1.5f, 1.2f, P.glass).m(GLASS);
        b.rod({ -4.0f, 0.0f, 12.6f }, { -9.5f, 0.0f, 17.5f }, 0.16f, P.dark).m(METAL).bev(0);
        for (int s = -1; s <= 1; s += 2) { b.box(5.6f, s * 3.4f, 6.8f, 0.5f, 1.2f, 0.5f, P.glow).m(EMIT); ciws(b, -16.0f, s * 5.2f, 5.05f, P, cy); }
    } else {
        b.box(-0.5f, 0.0f, 7.4f, 5.2f, 5.9f, 2.35f, P.pale).m(PAINT).t(T_PANEL, 5.0f).bev(0.6f);
        b.box(0.5f, 0.0f, 10.4f, 3.8f, 4.4f, 1.5f, P.pale).m(PAINT).t(T_PANEL, 4.0f).bev(0.5f);
        b.box(3.9f, 0.0f, 7.9f, 0.9f, 4.6f, 1.0f, P.glass).m(GLASS).bev(0.3f);
        b.box(3.2f, 0.0f, 10.6f, 0.7f, 3.2f, 0.8f, P.glass).m(GLASS).bev(0.3f);
        for (int s = -1; s <= 1; s += 2) {                                                // twin funnels, rust-banded
            b.cyl('z', -3.4f, s * 3.2f, 9.0f, 1.5f, 2.2f, P.mid).m(PAINT).bev(0.35f);
            b.cyl('z', -3.4f, s * 3.2f, 11.35f, 1.55f, 0.25f, P.rust).m(METAL).bev(0.2f);
        }
        b.rod({ -4.0f, 0.0f, 11.9f }, { -4.0f, 0.0f, 16.4f }, 0.28f, P.mid).m(METAL).bev(0);
        b.box(-4.0f, 0.0f, 16.6f, 0.5f, 2.6f, 0.35f, P.steel).m(METAL).bev(0.2f);         // radar bar
        for (int s = -1; s <= 1; s += 2) ciws(b, -16.0f, s * 5.2f, 5.05f, P, cy);
        b.box(-11.0f, 0.0f, 6.4f, 2.2f, 5.0f, 1.0f, P.mid).m(MATTE).t(T_RIBS, 1.4f).bev(0.3f);   // stowage locker
    }
    // lifeboats on their davits amidships
    for (int s = -1; s <= 1; s += 2) {
        b.box(-11.5f, s * 6.6f, 5.3f, 2.2f, 0.85f, 0.55f, P.foam).m(PAINT).bev(0.4f);
        b.box(-11.5f, s * 6.6f, 6.0f, 0.7f, 0.7f, 0.3f, P.mid).m(METAL).bev(0.2f);
    }
    // the empty mount pedestals fore and aft, the bulwark rails and the invisible turret stand-ins
    barbette(b, 12.0f, 0.0f, 5.05f, 5.0f, P.mid, P.dark);
    barbette(b, -15.0f, 0.0f, 5.05f, 5.0f, P.mid, P.dark);
    bothRails(b, { {-3.0f, 6.3f}, {17.0f, 6.3f}, {27.0f, 2.4f} }, 5.05f, P, 7.0f);
    b.cyl('z', 12.0f, 0.0f, 7.4f, 5.0f, 2.8f, P.deck).f(F_SHADOWONLY);
    b.cyl('z', -15.0f, 0.0f, 7.4f, 5.0f, 2.8f, P.deck).f(F_SHADOWONLY);
}

// one gun mount, standing on the deck at z = 5.05 and pointing +x
void destroyerGun(Model& t, const ShipPal& P, bool cy) {
    t.ground = false; t.dirt = 0.0f;
    if (cy) {
        // a faceted stealth shield with twin pulse barrels and cyan lens tips
        t.taper(0.0f, 0.0f, 5.05f, 5.0f, 4.8f, 3.4f, 3.2f, 2.9f, P.pale, 0.7f, 0.0f).m(PAINT).t(T_PANEL, 3.4f).bev(0.8f);
        t.plan({ {-4.6f, -4.4f}, {2.6f, -2.6f}, {5.2f, 0.0f}, {2.6f, 2.6f}, {-4.6f, 4.4f} }, 7.6f, 9.0f, P.pale).m(PAINT).bev(0.6f);
        t.box(2.0f, 0.0f, 6.6f, 1.4f, 3.4f, 1.2f, P.mid).m(PAINT).bev(0.4f);              // mantlet
        for (int s = -1; s <= 1; s += 2) {
            t.rod({ 2.6f, s * 1.9f, 7.0f }, { 11.0f, s * 1.9f, 7.0f }, 0.55f, P.dark).m(METAL).bev(0.1f);
            t.cyl('x', 11.4f, s * 1.9f, 7.0f, 0.75f, 0.5f, P.dark).m(METAL).bev(0.2f);
            t.box(11.9f, s * 1.9f, 7.0f, 0.4f, 0.6f, 0.5f, P.glow).m(EMIT);               // the muzzle lens
        }
        t.cyl('z', -2.4f, 0.0f, 9.35f, 1.5f, 0.5f, P.mid).m(PAINT).bev(0.3f);             // sensor drum
        t.dome(-2.4f, 0.0f, 9.6f, 1.2f, 1.2f, 0.9f, P.glass).m(GLASS);
        t.box(-4.2f, 0.0f, 6.4f, 0.7f, 3.0f, 1.0f, P.dark).m(MATTE).t(T_GRILLE, 1.2f).bev(0.2f);
    } else {
        // a rounded armoured barbette with twin long naval guns and muzzle brakes
        t.cyl('z', 0.0f, 0.0f, 5.35f, 4.7f, 0.5f, P.mid).m(PAINT).bev(0.3f);
        t.taper(0.0f, 0.0f, 5.05f, 4.9f, 4.6f, 3.4f, 4.0f, 3.6f, P.pale, -0.2f, 0.0f).m(PAINT).t(T_PANEL, 3.6f).bev(1.1f);
        t.taper(0.0f, 0.0f, 8.0f, 4.0f, 3.6f, 1.4f, 3.0f, 2.6f, P.pale, -0.2f, 0.0f).m(PAINT).bev(0.9f);
        t.box(1.8f, 0.0f, 6.9f, 1.6f, 3.2f, 1.4f, P.mid).m(METAL).bev(0.5f);              // mantlet
        for (int s = -1; s <= 1; s += 2) {
            t.rod({ 3.0f, s * 1.7f, 7.1f }, { 12.6f, s * 1.7f, 7.1f }, 0.6f, P.dark).m(METAL).bev(0.1f);
            t.cyl('x', 9.4f, s * 1.7f, 7.1f, 0.9f, 1.1f, P.mid).m(METAL).bev(0.2f);       // bore evacuator
            t.box(13.0f, s * 1.7f, 7.1f, 0.7f, 0.9f, 0.6f, P.haz).m(PAINT).bev(0.2f);     // muzzle brake
        }
        t.cyl('z', -2.4f, 0.0f, 9.55f, 1.6f, 0.35f, P.pale).m(PAINT).bev(0.3f);           // gunner's hatch
        t.box(-4.4f, 0.0f, 6.6f, 0.8f, 2.8f, 1.0f, P.rust).m(MATTE).t(T_GRILLE, 1.2f).bev(0.2f);
        t.rod({ -3.6f, 2.6f, 9.6f }, { 1.0f, 2.6f, 9.9f }, 0.22f, P.dark).m(METAL).f(F_NOSHADOW);   // roof machine gun
    }
}

static void kDestroyer(Model& b, Model& t, int) { destroyerHull(b, CK, false); destroyerGun(t, CK, false); }
M3D_UNIT(U_K_DESTROYER, kDestroyer)
static void cDestroyer(Model& b, Model& t, int) { destroyerHull(b, CY, true); destroyerGun(t, CY, true); }
M3D_UNIT(U_C_DESTROYER, cDestroyer)

// ================================================================ CRUISER (about 62 x 21 px hull, old canvas x 1.5..63)
// The deck is 5.0 on the centre line; the game draws its one missile launcher at model x = +16 and x = -16 (the old pedestals).

void cruiserHull(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const ShipShape H = { -30.5f, 24.0f, 10.5f, 9.5f, 22.0f, 8.0f, 27.0f, 4.5f, 31.0f };
    hullSides(b, H, 0.0f, 5.0f, 1.1f, P.hull, T_PANEL, 6.5f, P.mid);
    deckPlan(b, H, 1.8f, 5.0f, 0.45f, P.deck);
    bootBand(b, H, 1.1f, P.dark);
    b.plan({ {25.0f, 3.2f}, {31.0f, 1.6f}, {34.0f, 0.0f}, {31.0f, -1.6f}, {25.0f, -3.2f} }, 0.0f, 0.3f, P.foam).m(MATTE).bev(0.2f);
    // the owner's colour: a wide deck stripe down the centre line and a mark on the launcher deck
    b.box(-6.0f, 0.0f, 5.6f, 12.0f, 0.9f, 0.2f, OWNER).m(PAINT).bev(0.15f);
    b.box(20.0f, 0.0f, 5.6f, 3.0f, 2.6f, 0.2f, OWNER).m(PAINT).bev(0.15f);
    // missile hatch panels along the deck, outboard of the centre line (old canvas x 36.4..47.8)
    for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2)
        b.box(4.4f + i * 3.4f, s * 6.4f, 5.15f, 1.3f, 1.2f, 0.35f, cy ? P.mid : P.steel).m(PAINT).t(T_PANEL, 2.0f).bev(0.2f);
    // the hawse and anchor, deck vents and the aft louvres
    b.box(22.6f, 4.2f, 4.7f, 0.7f, 0.7f, 0.4f, P.dark).m(METAL).bev(0.2f);
    b.box(-25.5f, 0.0f, 5.4f, 3.4f, 6.0f, 0.35f, P.dark).m(MATTE).t(T_GRILLE, 1.4f).bev(0.2f);
    b.box(-19.0f, 5.0f, 5.45f, 1.4f, 1.1f, 0.4f, P.mid).m(METAL).bev(0.25f);
    b.box(-19.0f, -5.0f, 5.45f, 1.4f, 1.1f, 0.4f, P.mid).m(METAL).bev(0.25f);
    // close-in guns at the bow corners and the stern quarters
    ciws(b, 19.0f, 4.6f, 5.45f, P, cy); ciws(b, 19.0f, -4.6f, 5.45f, P, cy);
    ciws(b, -22.0f, 6.0f, 5.45f, P, cy); ciws(b, -22.0f, -6.0f, 5.45f, P, cy);
    // the superstructure: Cyber a broad faceted tier under a big flat radar; Clanker a box with twin funnels and a lattice mast
    if (cy) {
        b.taper(-1.0f, 0.0f, 5.45f, 7.0f, 8.6f, 4.2f, 5.4f, 6.6f, P.pale, 0.8f, 0.0f).m(PAINT).t(T_PANEL, 4.4f).bev(0.8f);
        b.taper(-1.0f, 0.0f, 9.6f, 5.0f, 5.8f, 3.4f, 3.6f, 4.0f, P.pale, 0.6f, 0.0f).m(PAINT).t(T_PANEL, 3.6f).bev(0.6f);
        b.box(3.4f, 0.0f, 7.6f, 1.0f, 6.0f, 0.9f, P.glass).m(GLASS).bev(0.3f);
        b.box(2.8f, 0.0f, 11.2f, 0.8f, 3.4f, 0.8f, P.glass).m(GLASS).bev(0.3f);
        b.box(-6.4f, 0.0f, 8.4f, 0.7f, 5.6f, 1.6f, OWNER).m(PAINT).t(T_PANEL, 3.0f).bev(0.3f);   // the owner's cobalt band
        b.frame(-3.0f, 0.0f, 13.4f, 20.0f);                                          // the big phased-array radar
        b.box(0.0f, 0.0f, 1.1f, 3.4f, 0.5f, 1.1f, P.pale).m(PAINT).bev(0.4f);
        b.box(0.0f, 0.0f, 2.4f, 0.5f, 2.6f, 0.2f, P.mid).m(METAL).bev(0.2f);
        b.frame();
        b.rod({ -6.0f, 0.0f, 13.0f }, { -12.0f, 0.0f, 18.5f }, 0.16f, P.dark).m(METAL).bev(0);
        for (int s = -1; s <= 1; s += 2) b.box(4.6f, s * 4.6f, 6.6f, 0.5f, 1.3f, 0.5f, P.glow).m(EMIT);
    } else {
        b.box(-1.0f, 0.0f, 7.8f, 6.6f, 8.2f, 2.3f, P.pale).m(PAINT).t(T_PANEL, 5.5f).bev(0.6f);
        b.box(0.0f, 0.0f, 11.0f, 4.6f, 5.4f, 1.4f, P.pale).m(PAINT).t(T_PANEL, 4.2f).bev(0.5f);
        b.box(5.2f, 0.0f, 8.3f, 0.9f, 6.4f, 1.0f, P.glass).m(GLASS).bev(0.3f);
        b.box(4.2f, 0.0f, 11.1f, 0.7f, 3.6f, 0.8f, P.glass).m(GLASS).bev(0.3f);
        for (int s = -1; s <= 1; s += 2) {                                            // twin funnels
            b.taper(-4.0f, s * 4.6f, 9.4f, 2.4f, 2.0f, 3.6f, 1.9f, 1.6f, P.mid, -0.3f, 0.0f).m(PAINT).bev(0.5f);
            b.cyl('z', -4.0f, s * 4.6f, 13.15f, 2.0f, 0.25f, P.rust).m(METAL).bev(0.2f);
        }
        b.rod({ -1.0f, 0.0f, 12.4f }, { -1.0f, 0.0f, 17.6f }, 0.3f, P.mid).m(METAL).bev(0);
        b.box(-1.0f, 0.0f, 17.8f, 0.5f, 3.0f, 0.4f, P.steel).m(METAL).bev(0.2f);
        b.rod({ -1.8f, 2.4f, 12.6f }, { -0.2f, -2.4f, 16.4f }, 0.14f, P.dark).m(METAL).bev(0);
        b.rod({ -0.2f, 2.4f, 12.6f }, { -1.8f, -2.4f, 16.4f }, 0.14f, P.dark).m(METAL).bev(0);
        b.box(-9.0f, 0.0f, 6.6f, 2.6f, 6.4f, 1.2f, P.mid).m(MATTE).t(T_RIBS, 1.4f).bev(0.3f);
        b.box(-1.0f, 0.0f, 12.45f, 3.0f, 3.6f, 0.2f, OWNER).m(PAINT).bev(0.15f);                  // the owner's mark on the bridge roof
    }
    // lifeboats and rafts amidships, the anchor detail at the stem
    for (int s = -1; s <= 1; s += 2) {
        b.box(-13.0f, s * 7.6f, 5.6f, 2.3f, 0.9f, 0.6f, P.foam).m(PAINT).bev(0.4f);
        raftCan(b, 9.0f, s * 7.4f, 5.45f, cy ? P.pale : P.rust);
    }
    // the empty launcher pedestals, rails and the turret stand-ins
    barbette(b, 16.0f, 0.0f, 5.45f, 7.0f, P.mid, P.dark);
    barbette(b, -16.0f, 0.0f, 5.45f, 7.0f, P.mid, P.dark);
    bothRails(b, { {-24.0f, 8.2f}, {6.0f, 8.2f}, {20.0f, 6.4f}, {28.0f, 2.6f} }, 5.45f, P, 13.0f);
    b.cyl('z', 16.0f, 0.0f, 8.6f, 7.2f, 3.2f, P.deck).f(F_SHADOWONLY);
    b.cyl('z', -16.0f, 0.0f, 8.6f, 7.2f, 3.2f, P.deck).f(F_SHADOWONLY);
}

// one missile launcher, standing on the deck at z = 5.45 and pointing +x
void cruiserLauncher(Model& t, const ShipPal& P, bool cy) {
    t.ground = false; t.dirt = 0.0f;
    if (cy) {
        // a deep angled VLS box, four columns of two cells, each lid with a cyan seal
        t.taper(0.0f, 0.0f, 5.45f, 8.0f, 6.2f, 3.0f, 7.0f, 5.4f, P.pale, -0.6f, 0.0f).m(PAINT).t(T_PANEL, 3.6f).bev(0.6f);
        t.frame(0.0f, 0.0f, 8.0f);
        for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2) {
            t.box(3.4f - i * 2.6f, s * 2.6f, 0.75f, 1.05f, 1.15f, 0.75f, P.mid).m(PAINT).ry(-14.0f).bev(0.25f);
            t.box(4.4f - i * 2.6f, s * 2.6f, 1.35f, 0.35f, 1.0f, 0.15f, P.glow).m(EMIT).ry(-14.0f);
        }
        t.frame();
        t.box(-7.4f, 0.0f, 6.6f, 0.7f, 4.6f, 1.2f, P.dark).m(MATTE).t(T_GRILLE, 1.3f).bev(0.2f);
        t.box(0.0f, 0.0f, 8.35f, 6.0f, 0.5f, 0.2f, P.trim).m(PAINT).bev(0.1f);
    } else {
        // a rack of three rows of long rocket tubes on an iron frame, rust warheads at the front
        t.box(-1.0f, 0.0f, 6.0f, 6.4f, 5.6f, 0.7f, P.mid).m(METAL).t(T_RIBS, 1.6f).bev(0.3f);
        t.taper(-2.0f, 0.0f, 5.45f, 6.0f, 5.8f, 1.6f, 5.0f, 5.0f, P.pale, -0.4f, 0.0f).m(PAINT).t(T_PANEL, 3.4f).bev(0.5f);
        for (int r = 0; r < 3; r++) {
            const float y = -3.4f + r * 3.4f;
            t.rod({ -4.6f, y, 8.0f }, { 8.6f, y, 9.4f }, 0.95f, P.steel).m(METAL).bev(0.2f);
            t.cyl('x', 9.4f, y, 9.5f, 1.15f, 0.5f, P.rust).m(PAINT).bev(0.25f);
            t.box(-5.4f, y, 7.8f, 0.6f, 1.0f, 0.6f, P.dark).m(METAL).bev(0.2f);
        }
        for (int s = -1; s <= 1; s += 2) t.box(-1.0f, s * 5.2f, 7.4f, 4.0f, 0.5f, 0.5f, P.dark).m(METAL).bev(0.2f);
        t.box(-6.6f, 0.0f, 7.0f, 0.7f, 4.4f, 1.4f, P.haz).m(PAINT).t(T_HAZARD, 2.2f, P.dark).bev(0.2f);
    }
}

static void kCruiser(Model& b, Model& t, int) { cruiserHull(b, CK, false); cruiserLauncher(t, CK, false); }
M3D_UNIT(U_K_CRUISER, kCruiser)
static void cCruiser(Model& b, Model& t, int) { cruiserHull(b, CY, true); cruiserLauncher(t, CY, true); }
M3D_UNIT(U_C_CRUISER, cCruiser)

// ================================================================ PATROL BOAT (about 39 x 12 px hull, old canvas x 14..52.6)
// The deck is 4.4; the game draws its one gun-and-rocket mount at model x = +7 (the old foredeck pedestal).

void patrolHull(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const ShipShape H = { -18.0f, 17.0f, 6.1f, 5.9f, 12.0f, 5.4f, 15.0f, 3.4f, 20.6f };
    hullStage(b, H, 0.0f, 4.0f, P.hull, T_PANEL, 5.0f, P.mid, 0.5f);            // a planing hull: nearly straight topsides
    bootBand(b, H, 0.95f, P.dark);                                              // the boot-top at the waterline
    deckPlan(b, H, 1.1f, 4.0f, 0.4f, P.deck);
    b.plan({ {15.0f, 2.2f}, {20.0f, 1.0f}, {22.5f, 0.0f}, {20.0f, -1.0f}, {15.0f, -2.2f} }, 0.0f, 0.28f, P.foam).m(MATTE).bev(0.2f);
    // the owner's colour: a band across the wheelhouse roof and a stripe on the transom
    b.box(-7.6f, 0.0f, 9.55f, 1.5f, 3.6f, 0.2f, OWNER).m(PAINT).bev(0.1f);
    b.box(-17.4f, 0.0f, 3.85f, 0.8f, 3.4f, 0.35f, OWNER).m(PAINT).bev(0.15f);
    // twin outboard engines on the transom, with pale cowl tops
    for (int s = -1; s <= 1; s += 2) {
        b.box(-19.2f, s * 3.1f, 4.4f, 1.6f, 1.2f, 1.0f, P.dark).m(PAINT).bev(0.4f);
        b.box(-19.2f, s * 3.1f, 5.5f, 1.1f, 0.8f, 0.35f, P.pale).m(PAINT).bev(0.3f);
    }
    // an aft deck hatch, a life-raft canister and a bitt each side
    b.cyl('z', -16.0f, 3.6f, 4.6f, 0.55f, 0.3f, P.dark).m(METAL).bev(0.2f);
    b.cyl('z', -16.0f, -3.6f, 4.6f, 0.55f, 0.3f, P.dark).m(METAL).bev(0.2f);
    b.box(-12.6f, -1.4f, 4.55f, 1.2f, 1.2f, 0.25f, P.mid).m(PAINT).bev(0.25f);
    raftCan(b, -14.0f, 4.0f, 4.4f, cy ? P.pale : P.rust);                       // the life-raft canister and its strap
    // the low angular wheelhouse a little aft of the middle, glazed across its forward face
    b.taper(-6.0f, 0.0f, 4.4f, 4.4f, 3.9f, 4.6f, 3.4f, 3.2f, P.pale, -0.6f, 0.0f).m(PAINT).t(T_PANEL, 3.2f).bev(0.7f);
    b.plan({ {-1.4f, -3.4f}, {0.6f, -2.2f}, {0.6f, 2.2f}, {-1.4f, 3.4f} }, 4.4f, 8.6f, P.glass).m(GLASS).bev(0.4f);   // the windscreen
    b.box(-6.0f, 0.0f, 9.3f, 3.0f, 2.4f, 0.25f, P.mid).m(PAINT).bev(0.2f);       // the roof plate
    // a short mast with a radar bar across the beam, on the roof's aft half
    b.rod({ -9.0f, 0.0f, 9.4f }, { -9.0f, 0.0f, 13.4f }, 0.28f, P.mid).m(METAL).bev(0);
    b.box(-9.0f, 0.0f, 13.6f, 0.45f, 2.8f, 0.3f, P.steel).m(METAL).bev(0.2f);
    b.box(-9.4f, 0.0f, 12.2f, 0.5f, 1.4f, 0.4f, P.glow).m(EMIT);
    // the empty mount pedestal on the foredeck, the sheer rails and the invisible turret stand-in
    barbette(b, 7.0f, 0.0f, 4.4f, 4.0f, P.mid, P.dark);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-4.0f, s * 5.4f, 5.0f, 12.0f, 0.16f, 0.16f, P.mid).m(METAL).bev(0.12f);
        b.box(11.0f, s * 4.2f, 5.0f, 4.0f, 0.16f, 0.16f, P.mid).m(METAL).rz(s * -31.0f).bev(0.12f);
    }
    b.cyl('z', 7.0f, 0.0f, 6.6f, 4.0f, 2.2f, P.deck).f(F_SHADOWONLY);
}

// one gun-and-rocket mount, standing on the deck at z = 4.4 and pointing +x
void patrolGun(Model& t, const ShipPal& P, bool cy) {
    t.ground = false; t.dirt = 0.0f;
    t.taper(0.0f, 0.0f, 4.4f, 4.0f, 3.6f, 2.6f, 3.0f, 2.6f, P.pale, -0.3f, 0.0f).m(PAINT).t(T_PANEL, 3.0f).bev(0.9f);
    t.box(2.2f, 0.0f, 5.9f, 1.4f, 2.2f, 0.9f, P.mid).m(PAINT).bev(0.4f);             // the breech housing
    for (int s = -1; s <= 1; s += 2) {
        t.rod({ 2.6f, s * 1.1f, 6.2f }, { 9.6f, s * 1.1f, 6.2f }, 0.42f, P.dark).m(METAL).bev(0.1f);
        t.box(9.9f, s * 1.1f, 6.2f, 0.5f, 0.55f, 0.4f, cy ? P.mid : P.haz).m(PAINT).bev(0.15f);
    }
    // the rocket pod on the port side: two tubes with their mouths forward
    t.box(-0.4f, -4.6f, 6.4f, 2.4f, 1.2f, 1.0f, P.mid).m(PAINT).bev(0.4f);
    for (int i = 0; i < 2; i++) t.cyl('x', 2.2f, -4.6f, 5.9f + i * 1.1f, 0.55f, 1.7f, P.dark).m(METAL).bev(0.15f);
    t.box(-2.4f, 0.0f, 7.1f, 1.3f, 1.3f, 0.4f, P.pale).m(PAINT).bev(0.3f);           // the gunner's hatch
}

static void kPatrol(Model& b, Model& t, int) { patrolHull(b, CK, false); patrolGun(t, CK, false); }
M3D_UNIT(U_K_PATROL, kPatrol)
static void cPatrol(Model& b, Model& t, int) { patrolHull(b, CY, true); patrolGun(t, CY, true); }
M3D_UNIT(U_C_PATROL, cPatrol)

// ================================================================ BATTLESHIP (128 px canvas: model = px - 64; hull x -58..59.6, 26 wide)
// The deck top is 5.05 on the centre line everywhere the mounts sit; the game draws its one triple turret at model x = +41, +27 and -38.

void battleshipHull(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const std::vector<WS> HULL = { {-55.0f, 9.8f}, {-30.0f, 12.6f}, {-2.0f, 13.2f}, {28.0f, 12.8f}, {46.0f, 9.0f}, {59.6f, 0.0f} };
    const std::vector<WS> DECK = { {-52.0f, 7.8f}, {-20.0f, 8.8f}, {16.0f, 8.6f}, {40.0f, 6.0f}, {52.0f, 0.0f} };
    const std::vector<WS> FORE = { {28.0f, 8.0f}, {44.0f, 5.2f}, {53.0f, 0.0f} };
    hullChain(b, HULL, 0.0f, 1.3f, P.dark, T_NONE, 4.0f, P.dark, 0.4f);           // the boot-top at the waterline
    hullChain(b, HULL, 1.3f, 4.6f, P.hull, T_PANEL, 8.0f, P.mid, 0.6f);           // the armoured sides
    deckChain(b, DECK, 0.0f, 4.6f, 0.45f, P.deck);                                // the main deck
    deckChain(b, FORE, 0.4f, 4.6f, 0.45f, cy ? P.pale : P.deck);                  // the forecastle plate
    b.plan({ {44.0f, 3.4f}, {56.0f, 1.6f}, {62.0f, 0.0f}, {56.0f, -1.6f}, {44.0f, -3.4f} }, 0.0f, 0.35f, P.foam).m(MATTE).bev(0.2f);   // the bow wave
    // the owner's colour: a stripe along the deck each side and a mark on the forecastle
    for (int s = -1; s <= 1; s += 2) b.box(-16.0f, s * 7.6f, 5.2f, 14.0f, 0.6f, 0.2f, OWNER).m(PAINT).bev(0.15f);
    b.box(34.0f, 0.0f, 5.2f, 5.0f, 3.0f, 0.2f, OWNER).m(PAINT).bev(0.1f);
    // armour seams across the citadel, the hawse and the anchor
    for (int i = 0; i < 2; i++) b.box(-28.0f + i * 30.0f, 0.0f, 4.8f, 0.5f, 8.4f, 0.12f, P.mid).m(PAINT).bev(0);
    b.box(50.0f, 4.6f, 4.9f, 0.9f, 0.9f, 0.5f, P.dark).m(METAL).bev(0.2f);
    b.cyl('z', 40.0f, -6.4f, 5.35f, 0.7f, 0.3f, P.dark).m(METAL).bev(0.2f);
    // the vertical-launch blocks: one grid aft of the bridge, one forward (old canvas 36.8 and 68.6)
    for (float x : { -27.2f, 4.6f }) {
        b.box(x + 6.8f, 0.0f, 5.35f, 7.2f, 7.2f, 0.35f, P.dark).m(PAINT).t(T_PANEL, 3.0f).bev(0.2f);
        for (int j = 0; j < 2; j++) b.box(x + 2.4f + j * 8.8f, 0.0f, 5.85f, 2.6f, 6.4f, 0.25f, cy ? P.pale : P.steel).m(PAINT).t(T_RIBS, 2.0f).bev(0.2f);
    }
    // the secondary gun sponsons outboard and the close-in guns on the quarters
    for (int s = -1; s <= 1; s += 2) {
        ciws(b, -24.0f, s * 9.2f, 5.05f, P, cy);
        ciws(b, 20.0f, s * 9.2f, 5.05f, P, cy);
        b.box(-36.0f, s * 9.6f, 5.65f, 2.2f, 0.9f, 0.6f, cy ? P.pale : P.rust).m(PAINT).bev(0.4f);   // life rafts
    }
    // the helicopter deck aft: a ring and its H
    b.cyl('z', -46.0f, 0.0f, 5.06f, 4.8f, 0.08f, cy ? P.pale : P.haz).m(PAINT).bev(0);
    for (int s = -1; s <= 1; s += 2) b.box(-46.0f + s * 1.6f, 0.0f, 5.2f, 0.45f, 2.1f, 0.1f, P.dark).m(PAINT).bev(0);
    b.box(-46.0f, 0.0f, 5.2f, 1.6f, 0.45f, 0.1f, P.dark).m(PAINT).bev(0);
    // the tall pagoda bridge: three stepped tiers with a lit window band, and the radar on top
    b.taper(-8.0f, 0.0f, 5.05f, 6.4f, 6.4f, 4.6f, 5.6f, 5.6f, P.pale, 0.0f, 0.0f).m(PAINT).t(T_PANEL, 5.0f).bev(0.7f);
    b.taper(-8.0f, 0.0f, 9.65f, 4.4f, 4.4f, 3.6f, 3.6f, 3.6f, P.pale, 0.0f, 0.0f).m(PAINT).t(T_PANEL, 4.0f).bev(0.6f);
    b.taper(-8.0f, 0.0f, 13.25f, 2.6f, 2.6f, 2.8f, 2.0f, 2.0f, cy ? P.mid : P.steel, 0.0f, 0.0f).m(PAINT).t(T_PANEL, 3.0f).bev(0.5f);
    b.box(-1.8f, 0.0f, 7.3f, 0.6f, 4.2f, 0.9f, P.glass).m(GLASS).bev(0.25f);
    b.box(-4.4f, 0.0f, 11.3f, 0.6f, 3.2f, 0.8f, P.glass).m(GLASS).bev(0.25f);
    if (cy) {
        b.box(-8.0f, 0.0f, 16.6f, 2.6f, 2.6f, 0.5f, P.pale).m(PAINT).bev(0.4f);            // the flat phased-array panel
        b.box(-8.0f, 0.0f, 17.5f, 1.5f, 1.5f, 0.4f, P.glow).m(EMIT);
        b.rod({ -8.0f, 0.0f, 16.1f }, { -8.0f, 0.0f, 22.0f }, 0.16f, P.dark).m(METAL).bev(0);
        b.box(-2.6f, 5.0f, 7.4f, 0.6f, 1.4f, 0.6f, P.glow).m(EMIT);
        b.box(-2.6f, -5.0f, 7.4f, 0.6f, 1.4f, 0.6f, P.trim).m(PAINT).bev(0.2f);            // the cobalt band on the tower
    } else {
        b.rod({ -8.0f, 0.0f, 16.0f }, { -8.0f, 0.0f, 21.4f }, 0.3f, P.mid).m(METAL).bev(0);
        b.cyl('z', -8.0f, 0.0f, 21.7f, 1.7f, 0.4f, P.steel).m(METAL).bev(0.25f);            // radar scanner
        b.rod({ -10.0f, 2.0f, 16.2f }, { -6.0f, -2.0f, 20.2f }, 0.14f, P.dark).m(METAL).bev(0);
        b.box(-8.0f, 0.0f, 16.3f, 2.2f, 2.2f, 0.2f, OWNER).m(PAINT).bev(0.1f);
        for (int s = -1; s <= 1; s += 2) {
            b.taper(-17.0f, s * 5.8f, 5.05f, 2.6f, 2.2f, 6.4f, 2.0f, 1.7f, P.mid, -0.6f, 0.0f).m(PAINT).t(T_RIBS, 1.6f).bev(0.5f);   // the twin funnels
            b.cyl('z', -17.0f, s * 5.8f, 11.6f, 2.1f, 0.25f, P.rust).m(METAL).bev(0.2f);
        }
    }
    // bulwark rails, the three empty barbettes and the invisible turret stand-ins
    for (int s = -1; s <= 1; s += 2) {
        b.box(-34.0f, s * 10.2f, 5.95f, 20.0f, 0.16f, 0.16f, P.mid).m(METAL).bev(0.12f);
        b.box(4.0f, s * 10.0f, 5.95f, 18.0f, 0.16f, 0.16f, P.mid).m(METAL).bev(0.12f);
        b.box(34.0f, s * 7.4f, 5.95f, 10.0f, 0.16f, 0.16f, P.mid).m(METAL).rz(s * -20.0f).bev(0.12f);
    }
    barbette(b, 41.0f, 0.0f, 5.05f, 6.4f, P.mid, P.dark);
    barbette(b, 27.0f, 0.0f, 5.05f, 6.4f, P.mid, P.dark);
    barbette(b, -38.0f, 0.0f, 5.05f, 6.4f, P.mid, P.dark);
    b.cyl('z', 41.0f, 0.0f, 9.0f, 6.4f, 3.4f, P.deck).f(F_SHADOWONLY);
    b.cyl('z', 27.0f, 0.0f, 9.0f, 6.4f, 3.4f, P.deck).f(F_SHADOWONLY);
    b.cyl('z', -38.0f, 0.0f, 9.0f, 6.4f, 3.4f, P.deck).f(F_SHADOWONLY);
}

// one heavy triple-gun turret, standing on the deck at z = 5.05 / 5.5 and pointing +x
void battleshipTurret(Model& t, const ShipPal& P, bool cy) {
    t.ground = false; t.dirt = 0.0f;
    t.taper(-1.0f, 0.0f, 5.05f, 12.0f, 10.0f, 4.2f, 11.0f, 9.0f, P.pale, -1.0f, 0.0f).m(PAINT).t(T_PANEL, 5.0f).bev(1.1f);
    t.taper(-1.0f, 0.0f, 9.25f, 10.0f, 8.4f, 2.4f, 8.0f, 6.4f, P.pale, -0.8f, 0.0f).m(PAINT).t(T_PANEL, 4.0f).bev(0.9f);
    t.box(1.0f, 0.0f, 12.0f, 5.0f, 5.0f, 0.35f, OWNER).m(PAINT).bev(0.15f);            // the owner's roof panel
    t.box(9.0f, 0.0f, 8.6f, 2.2f, 7.6f, 2.2f, P.mid).m(METAL).bev(0.5f);               // the mantlet
    for (float s : { -4.2f, 0.0f, 4.2f }) {
        t.rod({ 9.0f, s, 9.6f }, { 26.0f, s, 9.6f }, 1.15f, P.dark).m(METAL).bev(0.15f);
        t.cyl('x', 17.0f, s, 9.6f, 1.5f, 1.8f, P.mid).m(METAL).bev(0.2f);              // the bore evacuator
        t.box(26.6f, s, 9.6f, 0.9f, 1.1f, 0.8f, cy ? P.glow : P.haz).m(cy ? EMIT : PAINT).bev(0.2f);
    }
    t.cyl('z', -6.0f, 0.0f, 11.9f, 3.0f, 0.5f, P.pale).m(PAINT).bev(0.4f);             // the roof hatch
    t.dome(-11.0f, 0.0f, 11.6f, 2.4f, 2.4f, 1.8f, cy ? P.glass : P.steel).m(cy ? GLASS : METAL);   // the rangefinder hood
    t.box(-12.6f, 0.0f, 7.4f, 0.9f, 6.0f, 1.4f, P.dark).m(MATTE).t(T_GRILLE, 1.3f).bev(0.2f);
}

static void kBattleship(Model& b, Model& t, int) { battleshipHull(b, CK, false); battleshipTurret(t, CK, false); }
M3D_UNIT(U_K_BATTLESHIP, kBattleship)
static void cBattleship(Model& b, Model& t, int) { battleshipHull(b, CY, true); battleshipTurret(t, CY, true); }
M3D_UNIT(U_C_BATTLESHIP, cBattleship)

// ================================================================ NUCLEAR SUBMARINE (128 px canvas: model = px - 64; hull x -54..55, 14 wide)
// Surfaced look, no turret: the pressure hull shows only a few units above the water, with the casing, the sail and the missile hatches on top.

void subBody(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const std::vector<WS> HULL = { {-54.0f, 2.8f}, {-40.0f, 5.6f}, {-4.0f, 7.0f}, {28.0f, 7.0f}, {46.0f, 4.8f}, {55.0f, 0.0f} };
    const std::vector<WS> CASE = { {-52.0f, 1.4f}, {-34.0f, 4.6f}, {-4.0f, 5.0f}, {28.0f, 5.0f}, {44.0f, 3.2f}, {52.0f, 0.0f} };
    hullChain(b, HULL, 0.0f, 2.5f, P.dark, T_PANEL, 7.0f, P.mid, 0.5f);           // the wetted hull, only its crown showing
    hullChain(b, CASE, 2.5f, 3.9f, P.hull, T_NOISE, 3.4f, P.mid, 0.45f);          // the casing, with its anechoic tiles
    b.plan({ {46.0f, 3.0f}, {56.0f, 1.4f}, {60.0f, 0.0f}, {56.0f, -1.4f}, {46.0f, -3.0f} }, 0.0f, 0.3f, P.foam).m(MATTE).bev(0.2f);   // the wash at the bow
    b.plan({ {-58.0f, 2.8f}, {-53.0f, 2.8f}, {-53.0f, -2.8f}, {-58.0f, -2.8f} }, 0.0f, 0.28f, P.foam).m(MATTE).bev(0.2f);               // the stern wash
    // the sail (the conning tower), its fairing, its planes and the masts
    b.taper(18.7f, 0.0f, 4.0f, 9.9f, 3.4f, 5.4f, 7.4f, 2.6f, P.pale, -0.8f, 0.0f).m(PAINT).t(T_PANEL, 4.0f).bev(0.8f);
    b.taper(18.7f, 0.0f, 9.4f, 7.0f, 2.4f, 1.2f, 5.4f, 1.8f, P.pale, -0.6f, 0.0f).m(PAINT).bev(0.6f);
    b.box(25.2f, 0.0f, 6.7f, 0.9f, 2.6f, 1.5f, P.glass).m(GLASS).bev(0.3f);        // the bridge windows facing forward
    b.rod({ 16.6f, 0.0f, 10.6f }, { 15.4f, 0.0f, 15.8f }, 0.32f, P.steel).m(METAL).bev(0);   // the periscope / attack mast
    b.rod({ 21.0f, 0.0f, 10.6f }, { 22.2f, 0.0f, 14.4f }, 0.26f, P.steel).m(METAL).bev(0);   // the radar mast
    for (int s = -1; s <= 1; s += 2)   // the sail planes, raked aft
        b.plan({ {-3.0f, s * 1.0f}, {4.0f, s * 1.0f}, {6.0f, s * 5.4f}, {-1.0f, s * 5.0f} }, 7.4f, 7.9f, P.pale).m(PAINT).bev(0.3f);
    // the row of ballistic-missile hatches behind the sail, on the centre line
    for (int i = 0; i < 5; i++) b.cyl('z', -18.0f + i * 6.0f, 0.0f, 4.2f, 2.0f, 0.35f, i == 2 ? P.dark : P.mid).m(PAINT).bev(0.3f);
    // the X tail: two stern planes and the rudder fin between them
    for (int s = -1; s <= 1; s += 2)
        b.plan({ {-53.4f, s * 3.0f}, {-46.4f, s * 4.4f}, {-50.2f, s * 11.4f}, {-54.6f, s * 10.2f} }, 1.4f, 2.1f, P.hull).m(PAINT).bev(0.3f);
    b.plan({ {-54.0f, -0.9f}, {-46.0f, -0.9f}, {-46.0f, 0.9f}, {-54.0f, 0.9f} }, 2.0f, 7.2f, P.hull).m(PAINT).bev(0.4f);   // the rudder fin
    // the markings: a band along the sail, the owner's stripe on the casing and its mark on the fin
    for (int s = -1; s <= 1; s += 2) b.box(18.7f, s * 3.0f, 6.5f, 6.0f, 0.35f, 0.35f, cy ? P.glow : P.haz).m(cy ? EMIT : PAINT).bev(0.1f);
    b.box(-8.0f, 0.0f, 4.05f, 12.0f, 0.35f, 0.2f, OWNER).m(PAINT).bev(0.1f);
    b.box(18.7f, 0.0f, 10.7f, 3.0f, 1.6f, 0.2f, OWNER).m(PAINT).bev(0.1f);
    b.box(48.0f, 0.0f, 2.3f, 0.5f, 3.4f, 0.2f, P.mid).m(PAINT).bev(0);                  // the sonar dome seam at the bow
    b.box(27.6f, 0.0f, 10.7f, 0.5f, 1.2f, 0.4f, P.glow).m(EMIT);         // the masthead light
    if (!cy) {
        b.box(-8.0f, 4.7f, 2.4f, 20.0f, 0.4f, 0.5f, P.rust).m(PAINT).bev(0.2f);          // the rust waterline
        b.box(-8.0f, -4.7f, 2.4f, 20.0f, 0.4f, 0.5f, P.rust).m(PAINT).bev(0.2f);
    }
}

static void kSub(Model& b, Model&, int) { subBody(b, CK, false); }
M3D_UNIT(U_K_SUB, kSub)
static void cSub(Model& b, Model&, int) { subBody(b, CY, true); }
M3D_UNIT(U_C_SUB, cSub)

// ================================================================ AIRCRAFT CARRIER (128 px canvas: model = px - 64; hull x -59.5..63, 36 wide)
// No turret. The flight deck sits at z = 7.0 with its markings; the island stands on the starboard (+y) side. The game draws the jets.

void carrierBody(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const std::vector<WS> KHULL = { {-59.5f, 13.6f}, {-50.0f, 16.6f}, {-20.0f, 18.2f}, {16.0f, 17.8f}, {36.0f, 16.0f}, {50.0f, 13.4f}, {56.0f, 11.2f}, {60.0f, 8.0f}, {62.0f, 4.2f}, {63.0f, 0.0f} };
    const std::vector<WS> KDECK = { {-56.0f, 14.8f}, {-34.0f, 15.6f}, {-4.0f, 15.8f}, {24.0f, 15.4f}, {40.0f, 14.6f}, {50.0f, 12.6f}, {56.0f, 10.0f}, {59.4f, 6.6f}, {61.2f, 0.0f} };
    hullChain(b, KHULL, 0.0f, 1.7f, P.dark, T_NONE, 4.0f, P.dark, 0.4f);            // the boot-top
    hullChain(b, KHULL, 1.7f, 6.6f, P.hull, T_PANEL, 9.0f, P.mid, 0.6f);            // the topsides, showing past the deck edge
    deckChain(b, KDECK, 0.0f, 6.6f, 0.5f, P.deck);                                  // the flight deck
    b.plan({ {50.0f, 6.0f}, {62.0f, 2.0f}, {66.0f, 0.0f}, {62.0f, -2.0f}, {50.0f, -6.0f} }, 0.0f, 0.35f, P.foam).m(MATTE).bev(0.2f);   // the bow wave
    // the deck edge lights / marking, and the centre line running stern to bow
    for (int s = -1; s <= 1; s += 2) {
        b.box(-4.0f, s * 14.6f, 7.2f, 40.0f, 0.5f, 0.2f, cy ? P.glow : P.haz).m(cy ? EMIT : PAINT).bev(0.1f);
        for (int i = 0; i < 4; i++) b.box(-46.0f + i * 30.0f, s * 14.6f, 7.35f, 1.4f, 0.4f, 0.25f, P.dark).m(MATTE).bev(0);   // the deck-edge lift wells
    }
    for (int i = 0; i < 5; i++) b.box(-42.0f + i * 20.0f, 0.0f, 7.15f, 6.0f, 0.55f, 0.15f, cy ? P.mid : P.haz).m(PAINT).bev(0);        // the centre line dashes
    // the angled landing deck to port: its lane plate, the edges and four arrestor wires
    b.plan({ {-55.0f, -12.7f}, {-14.0f, -15.6f}, {-14.0f, -4.4f}, {-55.0f, -1.5f} }, 7.12f, 7.2f, cy ? P.mid : P.deck).m(PAINT).bev(0.1f);
    for (int i = 0; i < 4; i++) b.box(-38.0f + i * 3.0f, -9.0f - i * 1.7f, 7.3f, 0.5f, 5.6f, 0.15f, P.dark).m(METAL).rz(-4.1f).bev(0);
    b.box(-34.5f, -14.4f, 7.3f, 20.0f, 0.45f, 0.15f, cy ? P.pale : P.haz).m(PAINT).rz(-4.1f).bev(0);      // the lane's port edge
    b.box(-34.5f, -3.2f, 7.3f, 20.0f, 0.45f, 0.15f, cy ? P.pale : P.haz).m(PAINT).rz(-4.1f).bev(0);      // its starboard edge
    // the two bow catapults: a track sunk in the deck and a shuttle at the launch end
    for (int s = -1; s <= 1; s += 2) {
        b.box(46.0f, s * 4.4f, 7.15f, 9.0f, 0.9f, 0.15f, P.dark).m(METAL).t(T_RIBS, 2.0f).bev(0);
        b.box(53.0f, s * 4.4f, 7.4f, 1.0f, 1.4f, 0.35f, P.mid).m(METAL).bev(0.2f);
    }
    // the parking spots the game fills with its own jets: just the painted boxes
    for (int i = 0; i < 3; i++) b.box(34.8f - i * 12.7f, -10.0f, 7.2f, 9.2f, 5.2f, 0.12f, cy ? P.mid : P.haz).m(PAINT).rz(28.6f).bev(0);
    for (int i = 0; i < 3; i++) b.box(36.1f - i * 12.7f, 10.0f, 7.2f, 9.2f, 5.2f, 0.12f, cy ? P.mid : P.haz).m(PAINT).rz(-28.6f).bev(0);
    // the deck-edge elevators: aft and amidships on the starboard edge, forward on the port side
    b.box(-52.0f, 5.6f, 7.15f, 3.7f, 4.4f, 0.2f, P.mid).m(PAINT).t(T_PANEL, 3.0f).bev(0.2f);
    b.box(-35.0f, 7.6f, 7.15f, 3.7f, 3.6f, 0.2f, P.mid).m(PAINT).t(T_PANEL, 3.0f).bev(0.2f);
    b.box(-5.6f, -12.4f, 7.15f, 3.8f, 3.0f, 0.2f, P.mid).m(PAINT).t(T_PANEL, 3.0f).bev(0.2f);
    // the island on the starboard side: a stepped superstructure, glazed forward, with the radar and the stack
    b.box(-13.3f, 10.2f, 10.0f, 10.7f, 4.8f, 3.0f, P.pale).m(PAINT).t(T_PANEL, 5.0f).bev(0.6f);
    b.taper(-9.0f, 10.2f, 13.0f, 5.2f, 3.0f, 3.4f, 4.4f, 2.4f, P.pale, 0.6f, 0.0f).m(PAINT).t(T_PANEL, 4.0f).bev(0.5f);
    b.box(-5.0f, 10.2f, 17.9f, 3.4f, 2.4f, 1.5f, cy ? P.mid : P.steel).m(PAINT).t(T_PANEL, 3.0f).bev(0.4f);      // the pilot house
    b.box(-1.2f, 10.2f, 10.6f, 0.7f, 4.2f, 1.4f, P.glass).m(GLASS).bev(0.3f);                                    // its glazed forward face
    b.box(-4.6f, 7.9f, 17.9f, 2.6f, 0.5f, 1.1f, P.glass).m(GLASS).bev(0.25f);                                    // the flight-control windows, facing the deck
    b.box(-13.0f, 10.2f, 20.0f, 3.0f, 2.0f, 0.6f, OWNER).m(PAINT).bev(0.2f);                                     // the owner's mark on the island roof
    b.box(-9.0f, 13.6f, 9.6f, 1.7f, 1.4f, 0.6f, OWNER).m(PAINT).bev(0.2f);                                       // the owner's team stripe
    if (cy) {
        b.box(-11.0f, 10.2f, 17.4f, 2.4f, 0.5f, 2.1f, P.pale).m(PAINT).bev(0.3f);                                // the flat radar panel
        b.box(-11.0f, 10.2f, 17.4f, 1.2f, 0.6f, 0.9f, P.glow).m(EMIT);
        b.rod({ -16.0f, 10.2f, 13.0f }, { -16.0f, 10.2f, 21.0f }, 0.18f, P.dark).m(METAL).bev(0);
        b.box(-18.6f, 10.2f, 11.0f, 1.4f, 2.4f, 1.0f, P.trim).m(PAINT).t(T_PANEL, 2.0f).bev(0.3f);              // the cobalt stack
    } else {
        b.cyl('z', -11.0f, 10.2f, 17.6f, 2.0f, 0.5f, P.steel).m(METAL).bev(0.3f);                                // the radar dish
        b.rod({ -13.0f, 8.4f, 13.0f }, { -9.0f, 12.0f, 19.0f }, 0.16f, P.dark).m(METAL).bev(0);
        b.rod({ -9.0f, 8.4f, 13.0f }, { -13.0f, 12.0f, 19.0f }, 0.16f, P.dark).m(METAL).bev(0);
        b.taper(-18.6f, 10.2f, 13.0f, 1.8f, 2.6f, 4.0f, 1.5f, 2.2f, P.mid, -0.4f, 0.0f).m(PAINT).t(T_RIBS, 1.6f).bev(0.4f);   // the exhaust stack
        b.cyl('z', -19.6f, 10.2f, 17.1f, 1.6f, 0.25f, P.rust).m(METAL).bev(0.2f);
    }
    // close-in guns on the bow corners, missile sponsons on the stern corners, the bow light
    ciws(b, 53.0f, -12.2f, 7.1f, P, cy);
    ciws(b, 53.0f, 12.2f, 7.1f, P, cy);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-58.4f, s * 17.6f, 8.0f, 2.4f, 1.8f, 1.0f, P.hull).m(PAINT).t(T_RIBS, 1.6f).bev(0.3f);
        b.box(-58.4f, s * 17.6f, 9.1f, 1.6f, 1.2f, 0.2f, P.dark).m(MATTE).t(T_GRILLE, 1.2f).bev(0.1f);
    }
    b.box(59.0f, 0.0f, 7.6f, 0.6f, 1.0f, 0.5f, P.foam).m(EMIT);                                                  // the bow light
    // the landing lens at the port edge of the lane, with its row of lights
    b.box(-54.4f, -13.4f, 7.5f, 1.4f, 0.7f, 0.5f, P.dark).m(PAINT).bev(0.2f);
    b.box(-54.4f, -13.4f, 8.1f, 0.9f, 0.5f, 0.2f, P.glow).m(EMIT);
}

static void kCarrier(Model& b, Model&, int) { carrierBody(b, CK, false); }
M3D_UNIT(U_K_CARRIER, kCarrier)
static void cCarrier(Model& b, Model&, int) { carrierBody(b, CY, true); }
M3D_UNIT(U_C_CARRIER, cCarrier)

// ================================================================ MILITARY CARGO / LANDING SHIP (128 px canvas: model = px - 64; hull x -58..58, 26 wide)
// No turret. A flat vehicle deck loaded with containers and vehicles, the accommodation forward and a stern ramp lowered into the water.

void cargoBody(Model& b, const ShipPal& P, bool cy) {
    b.dirt = 0.0f;
    const std::vector<WS> CGO_HULL = { {-58.0f, 12.4f}, {-34.0f, 13.2f}, {16.0f, 13.2f}, {36.0f, 12.6f}, {48.0f, 10.8f}, {54.0f, 7.8f}, {58.0f, 0.0f} };
    const std::vector<WS> CGO_DECK = { {-56.0f, 11.4f}, {-34.0f, 11.8f}, {16.0f, 11.8f}, {36.0f, 11.2f}, {46.0f, 9.6f}, {52.0f, 6.8f}, {56.0f, 0.0f} };
    hullChain(b, CGO_HULL, 0.0f, 1.6f, P.dark, T_NONE, 4.0f, P.dark, 0.4f);          // the boot-top: rust-red on Clanker
    hullChain(b, CGO_HULL, 1.6f, 5.0f, P.hull, T_PANEL, 8.0f, P.mid, 0.55f);         // the topsides
    deckChain(b, CGO_DECK, 0.0f, 5.0f, 0.5f, P.deck);                                // the free vehicle deck
    if (!cy) { hullChain(b, CGO_HULL, 0.0f, 1.9f, P.rust, T_NONE, 4.0f, P.rust, 0.35f); }
    b.plan({ {46.0f, 4.0f}, {56.0f, 1.8f}, {60.0f, 0.0f}, {56.0f, -1.8f}, {46.0f, -4.0f} }, 0.0f, 0.32f, P.foam).m(MATTE).bev(0.2f);   // the bow wave
    // the stern well and the ramp lowered past the transom
    b.box(-54.0f, 0.0f, 5.15f, 3.0f, 5.5f, 0.2f, P.dark).m(MATTE).bev(0.1f);
    b.taper(-60.0f, 0.0f, 0.4f, 6.0f, 5.0f, 4.6f, 6.0f, 5.0f, P.deck, 0.0f, 0.0f).m(PAINT).t(T_GRILLE, 1.8f).rz(-42.0f).bev(0.2f);
    b.box(-56.6f, 0.0f, 5.0f, 0.6f, 5.2f, 0.5f, P.mid).m(METAL).bev(0.2f);           // the ramp hinge
    // deck markings: the owner's stripe down the centre line and the lane edges
    b.box(-14.0f, 0.0f, 5.4f, 22.0f, 0.8f, 0.15f, OWNER).m(PAINT).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-12.0f, s * 9.4f, 5.35f, 20.0f, 0.5f, 0.15f, cy ? P.glow : P.haz).m(cy ? EMIT : PAINT).bev(0.1f);
        b.box(30.0f, s * 8.0f, 5.35f, 8.0f, 0.5f, 0.15f, cy ? P.glow : P.haz).m(cy ? EMIT : PAINT).rz(s * -14.0f).bev(0.1f);
    }
    // the load on the deck: three rows of containers, trucks and vehicles, all pointing forward
    for (int i = 0; i < 4; i++) {
        b.box(-44.0f + i * 14.0f, -5.6f, 6.9f, 4.6f, 2.3f, 1.9f, i % 2 ? P.trim : P.mid).m(PAINT).t(T_RIBS, 1.9f).bev(0.3f);
        b.box(-44.0f + i * 14.0f, 0.0f, 6.6f, 4.6f, 2.3f, 1.6f, i % 2 ? P.mid : P.wood).m(PAINT).t(T_RIBS, 1.9f).bev(0.3f);
        b.box(-44.0f + i * 14.0f, 5.6f, 6.9f, 4.6f, 2.3f, 1.9f, i % 2 ? P.wood : P.trim).m(PAINT).t(T_RIBS, 1.9f).bev(0.3f);
    }
    b.box(14.0f, -5.6f, 6.5f, 4.0f, 2.0f, 1.5f, P.dark).m(MATTE).bev(0.4f);           // a vehicle at the head of the load
    b.box(14.0f, 5.6f, 6.5f, 4.0f, 2.0f, 1.5f, P.dark).m(MATTE).bev(0.4f);
    // two cranes on the deck edges, booms swung in over the lanes
    for (int s = -1; s <= 1; s += 2) {
        b.cyl('z', 19.0f, s * 9.4f, 6.6f, 2.0f, 1.2f, P.mid).m(METAL).t(T_PANEL, 3.0f).bev(0.4f);
        b.rod({ 19.0f, s * 9.4f, 8.0f }, { 12.0f, s * 5.4f, 10.4f }, 0.5f, cy ? P.pale : P.haz).m(PAINT).bev(0.2f);
    }
    // the accommodation block forward: the bridge on its forward end, glazed, and the funnel aft of it
    b.box(32.8f, 0.0f, 9.2f, 8.6f, 7.6f, 3.7f, P.pale).m(PAINT).t(T_PANEL, 5.5f).bev(0.6f);
    b.box(37.8f, 0.0f, 13.6f, 3.4f, 6.2f, 2.2f, P.pale).m(PAINT).t(T_PANEL, 4.0f).bev(0.5f);          // the bridge
    b.box(40.9f, 0.0f, 13.9f, 0.6f, 5.4f, 1.3f, P.glass).m(GLASS).bev(0.3f);                          // its forward windows
    b.box(32.8f, 0.0f, 12.9f, 5.0f, 3.4f, 0.2f, OWNER).m(PAINT).bev(0.1f);                            // the owner's pennant board on the roof
    b.cyl('z', 28.4f, 0.0f, 10.4f, 2.6f, 3.0f, P.mid).m(PAINT).bev(0.4f);                             // the funnel
    b.cyl('z', 28.4f, 0.0f, 13.5f, 2.2f, 0.25f, cy ? P.trim : P.rust).m(METAL).bev(0.2f);
    b.cyl('z', 28.4f, 0.0f, 13.7f, 1.3f, 0.2f, P.dark).m(METAL).bev(0.15f);
    if (cy) {
        b.box(31.0f, 0.0f, 15.4f, 2.4f, 2.4f, 0.4f, P.pale).m(PAINT).bev(0.3f);                        // the flat radar
        b.box(31.0f, 0.0f, 15.9f, 1.2f, 1.2f, 0.4f, P.glow).m(EMIT);
        b.box(36.0f, 0.0f, 16.2f, 0.4f, 4.0f, 0.4f, P.glow).m(EMIT);
    } else {
        b.cyl('z', 31.0f, 0.0f, 15.6f, 1.6f, 0.4f, P.steel).m(METAL).bev(0.25f);                        // the radar scanner
        b.rod({ 30.0f, 0.0f, 15.6f }, { 30.0f, 0.0f, 19.4f }, 0.16f, P.dark).m(METAL).bev(0);
        b.box(36.0f, 0.0f, 16.2f, 0.4f, 4.0f, 0.4f, P.haz).m(PAINT).bev(0.2f);
    }
    // the bow: the mooring deck, bitts, the anchor and the bulwark rails
    b.box(48.0f, 0.0f, 5.35f, 3.0f, 4.0f, 0.2f, P.mid).m(MATTE).t(T_RIBS, 1.6f).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) b.cyl('z', 44.0f, s * 5.0f, 5.9f, 0.7f, 0.5f, P.dark).m(METAL).bev(0.2f);
    b.box(52.0f, 3.0f, 5.2f, 0.8f, 0.8f, 0.5f, P.dark).m(METAL).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-10.0f, s * 10.6f, 6.0f, 22.0f, 0.18f, 0.18f, P.mid).m(METAL).bev(0.12f);
        b.box(36.0f, s * 8.6f, 6.0f, 10.0f, 0.18f, 0.18f, P.mid).m(METAL).rz(s * -22.0f).bev(0.12f);
    }
}

static void kCargo(Model& b, Model&, int) { cargoBody(b, CK, false); }
M3D_UNIT(U_K_CARGOSHIP, kCargo)
static void cCargo(Model& b, Model&, int) { cargoBody(b, CY, true); }
M3D_UNIT(U_C_CARGOSHIP, cCargo)

}  // namespace
