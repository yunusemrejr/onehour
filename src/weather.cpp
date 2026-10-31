// One Hour - the day, the weather and the lake's wildlife. All of it is cosmetic: the simulation never sees the hour or the rain.
//  * The day runs a 12 minute cycle (dawn, morning, day, sunset, dusk, night) from a random starting point, and its light tints the world: one
//    multiply pass over the battlefield, skipped entirely in plain daylight, so a bright afternoon costs nothing at all.
//  * Rain comes and goes in spells of a few minutes; a downpour darkens the sky and brings lightning. Streaks and splashes are a few hundred
//    lines and rings drawn in screen space.
//  * At night the structures' windows and the vehicles' headlights come on (small additive glows).
//  * Whales, dolphin pods, sharks and fish schools roam the open water of every big body of water and never leave the one they spawned in.
//    The big animals are shaded body pieces bent along a spine with a travelling wave (sideways for sharks, fluke beats and a flex in turns for whales and dolphins),
//    submerged ones blend into the water, and at the surface they brighten and break it. The fish are boids.
#include "ui.h"
#include "paint.h"
#include <ctime>
#include <string>

namespace {

const float DAY_LEN = 720.0f;   // seconds in one full day cycle

// the light of the hour: (phase, colour) keys, linearly blended, wrapping round at 1
struct Key { float p; float r, g, b; };
const Key KEYS[] = {
    { 0.00f, 0.52f, 0.58f, 0.80f },   // the end of the night
    { 0.05f, 0.96f, 0.78f, 0.74f },   // dawn: a pink sky
    { 0.12f, 1.00f, 0.94f, 0.86f },   // morning sun
    { 0.20f, 1.00f, 1.00f, 1.00f },   // day
    { 0.48f, 1.00f, 1.00f, 1.00f },
    { 0.55f, 1.00f, 0.77f, 0.56f },   // sunset
    { 0.60f, 0.74f, 0.62f, 0.74f },   // dusk
    { 0.67f, 0.50f, 0.56f, 0.78f },   // night
    { 0.82f, 0.44f, 0.50f, 0.74f },   // the dead of night
    { 1.00f, 0.52f, 0.58f, 0.80f },
};
const float START_POINTS[5] = { 0.03f, 0.14f, 0.30f, 0.53f, 0.74f };   // dawn, morning, day, sunset, night

float hashf(u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return (a & 0xFFFFFF) / 16777215.0f; }

// ------------------------------------------------------------ the sea: where the animals may swim
using Beast = Game::SeaBeast;
const int SEA_MIN_BODY = 400;                     // a body of water this small (tiles) holds no animals
const int SEA_MAX_POD = 6, SEA_MAX_SHARK = 8, SEA_MAX_SCHOOL = 24;
const Color WATER = rgb(26, 80, 128);             // a submerged animal blends into this

// the 5x5 tiles round (tx, ty) are all water of body `id`: a creature never noses into the beach, an island or another body
bool openTile(int tx, int ty, int id) {
    if (!inMap(tx - 2, ty - 2) || !inMap(tx + 2, ty + 2)) return false;
    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) {
        int i = (ty + dy) * MAP_W + tx + dx;
        if (g_map.tiles[i] != T_WATER || g_map.waterComp[i] != id) return false;
    }
    return true;
}
bool openAt(Vec2 p, int id) { return openTile(tileOf(p.x), tileOf(p.y), id); }
bool wetTile(Vec2 p, int id) {   // fish only need their own tile to be water of the body
    int tx = tileOf(p.x), ty = tileOf(p.y);
    return inMap(tx, ty) && g_map.tiles[ty * MAP_W + tx] == T_WATER && g_map.waterComp[ty * MAP_W + tx] == id;
}
Vec2 tileCentre(int i) { return Vec2((i % MAP_W) * TILE + TILE * 0.5f, (i / MAP_W) * TILE + TILE * 0.5f); }
// a random open spot of the body, at least minDist from `from` when a few tries find one
Vec2 seaGoal(Rng& r, const Game::SeaBody& b, Vec2 from, float minDist) {
    Vec2 p;
    for (int k = 0; k < 24; k++) {
        p = tileCentre(b.open[r.range(0, (int)b.open.size() - 1)]);
        if (dist(p, from) >= minDist) break;
    }
    return p;
}
// how long a whale or dolphin stays in state st: under, rising, at the surface, diving
float seaStateLen(int kind, int st, Rng& r) {
    if (kind == 0) return st == 0 ? r.f(16, 26) : (st == 1 ? 2.6f : (st == 2 ? r.f(5.5f, 7.5f) : 3.4f));
    return st == 0 ? r.f(2.5f, 6.0f) : (st == 1 ? 0.3f : (st == 2 ? 0.45f : 0.35f));
}
Vec2 rotv(Vec2 v, float a) { float c = std::cos(a), s = std::sin(a); return Vec2(v.x * c - v.y * s, v.x * s + v.y * c); }

// ------------------------------------------------------------ drawing helpers
// the spine of a creature: n points from the tail tip (0) to the snout (n-1) along its heading, bent sideways by the swim wave and a turn.
// Its pieces (below) are drawn along it, so the sprites bend with the spine
const int SPINE_MAX = 12;
struct Spine { Vec2 p[SPINE_MAX]; float r[SPINE_MAX]; int n = 0; };
// half width along the body (u: 0 tail tip .. 1 snout, widest at `peak`): a narrow tail stock, a blunt snout
float halfWidth(float u, float peak) {
    if (u < peak) return 0.18f + 0.82f * std::sin(0.5f * 3.14159f * u / peak);
    float q = (u - peak) / (1 - peak);
    return std::max(0.2f, std::sqrt(std::max(0.0f, 1 - q * q)));
}
// the spine on heading ang, centred on c: a travelling wave bends it sideways (wave: amplitude at the tail, wv: its phase), and a turn
// (bend) makes the tail swing out
void makeSpine(Spine& s, Vec2 c, float ang, float len, float wide, float peak, int n, float wave, float wv, float bend) {
    s.n = n;
    Vec2 f(std::cos(ang), std::sin(ang)), sd(-f.y, f.x);
    for (int i = 0; i < n; i++) {
        float u = i / (n - 1.0f), x = (u - 0.5f) * len;
        float lat = wave * (1 - u) * (1 - u) * std::sin(wv + 4.2f * u) - bend * (1 - u) * len * 0.12f;
        s.p[i] = c + f * x + sd * lat;
        s.r[i] = wide * halfWidth(u, peak);
    }
}
// one straight piece of a body, baked from the reference body (length Lref, half width wide * halfWidth, widest at peak) between the spine
// fractions u0 and u1 (0 tail tip, 1 snout). The reference is a union of discs along its spine, so the outline is one smooth shape, shaded
// with the same bevel as the structures (the colour comes in as a modulation when it is drawn). Pieces overlap a little at the joints.
Sprite bakePiece(Gfx& g, float Lref, float wide, float peak, float u0, float u1) {
    const float ov = 1.5f;
    float Lp = (u1 - u0) * Lref + 2 * ov, H = wide * 2 + 3, cx = ((u0 + u1) * 0.5f - 0.5f) * Lref;
    const int NR = 24;
    float xs[NR], rs[NR];
    for (int i = 0; i < NR; i++) { float u = i / (NR - 1.0f); xs[i] = (u - 0.5f) * Lref - cx; rs[i] = wide * halfWidth(u, peak); }
    Canvas c((int)(Lp * 2), (int)(H * 2));
    // the body's distance field in canvas pixels: the whole reference body shades the piece (so the joints do not show), the cut at the
    // piece's ends only limits its coverage
    auto full = [&](float px, float py) {
        float x = px * 0.5f - Lp * 0.5f, y = py * 0.5f - H * 0.5f, d = 1e9f;
        for (int i = 0; i < NR; i++) d = std::min(d, std::hypot(x - xs[i], y) - rs[i]);
        return 2 * d;
    };
    const float bevel = wide * 0.8f;   // lit edge width in canvas pixels, as the structures' shapes use
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float px = x + 0.5f, py = y + 0.5f, df = full(px, py);
        float cut = 2 * (std::abs(px * 0.5f - Lp * 0.5f) - Lp * 0.5f);
        float cov = clampf(0.5f - std::max(df, cut), 0, 1);
        if (cov <= 0) continue;
        float gx = full(px + 0.6f, py) - full(px - 0.6f, py), gy = full(px, py + 0.6f) - full(px, py - 0.6f), gl = paint::hyp(gx, gy);
        float lit = gl > 1e-4f ? (gx * -0.62f - gy * 0.78f) / gl : 0;
        float f = 1.0f + 0.5f * lit * clampf((bevel + df) / bevel, 0, 1);
        Color col = shade(rgb(170, 170, 170), f);
        col.a = (u8)(255 * cov);
        paint::put(c, x, y, col);
    }
    Sprite s = g.fromCanvasSmooth(c, c.w * 0.5f, c.h * 0.5f);
    s.dscale = 0.5f;
    return s;
}
Color tinted(Color c, Color t) { return rgb(c.r * t.r / 255, c.g * t.g / 255, c.b * t.b / 255); }
Color subm(Color c, float surf) { return mix(c, WATER, (1 - surf) * 0.5f); }   // under the water: blended toward it
u8 alphaOf(float surf) { return (u8)(255 * (0.45f + 0.55f * surf)); }            // and more see-through
// the modulation for a body piece (baked at 170 grey, so the lit side reaches the colour and the shade side falls well below it)
Color bodyMod(Color c, Color t) { return tinted(rgb(std::min(255, c.r * 3 / 2), std::min(255, c.g * 3 / 2), std::min(255, c.b * 3 / 2)), t); }
// the pieces of a body along its spine: piece k covers spine points a[k]..b[k], and sits on the chord between them
void drawPieces(Gfx& g, const Sprite* pieces, const int* a, const int* b, int np, const Spine& sp, float scale, Color mod, u8 alpha, Vec2 off = Vec2()) {
    for (int k = 0; k < np; k++) {
        Vec2 p0 = sp.p[a[k]], p1 = sp.p[b[k]], c = (p0 + p1) * 0.5f + off, d = p1 - p0;
        g.draw(pieces[k], c.x, c.y, std::atan2(d.y, d.x), scale, mod, alpha);
    }
}

// ------------------------------------------------------------ the creatures' sprites
// the sprites are painted once: the body pieces, a shark's dorsal fin, the pectoral fins and flukes, the fish in 16 headings and 3 tail beats
// the reference lengths the bodies are baked at (each animal is drawn at its own length by scaling these)
const float REF_WHALE = 130.0f, REF_DOL = 26.0f, REF_SHARK = 40.0f;
struct SeaArt { Sprite fin, whaleFin, whaleFluke, dolFluke, sharkTail, sharkPec, whale[3], dolphin[2], shark[3], fish[2][3][16]; bool ok = false; };
SeaArt& seaArt() {
    static SeaArt a;
    if (a.ok) return a;
    a.ok = true;
    using namespace paint;
    Gfx& g = g_gfx;
    {   // shark fin, 3/4 view: a swept grey sail standing out of the water
        Canvas c(32, 32); Art A(c, rgb(255, 255, 255));
        A.poly({ {9, 25}, {17, 6}, {19.5f, 8}, {24, 25} }, rgb(70, 78, 86), 1.4f, 0.55f);
        A.poly({ {17, 6}, {19.5f, 8}, {24, 25}, {21, 25} }, rgb(44, 50, 58), 0.8f, 0.3f);
        a.fin = g.fromCanvasSmooth(c, 16, 25);
    }
    // the small parts are painted at 2x and drawn at half size (smooth edges); (w, h) in design pixels, (px, py) the pivot they turn about
    auto bake = [&](float w, float h, float px, float py, auto paintIt) {
        Canvas c((int)(w * 2), (int)(h * 2));
        Art A(c, rgb(255, 255, 255), 2, px, py);   // design origin = the pivot
        paintIt(A);
        Sprite s = g.fromCanvasSmooth(c, px * 2, py * 2);
        s.dscale = 0.5f;
        return s;
    };
    // a fluke lobe: a convex wedge from the base (the origin) out to (len, side * span)
    auto lobe = [](Art& A, float len, float span, float side, Color col) {
        A.poly({ {0, 0}, {len * 0.35f, side * span * 0.6f}, {len, side * span}, {len * 0.8f, side * span * 0.1f} }, col, 0.5f, 0.35f);
    };
    // the bodies: straight pieces baked from the reference bodies (see bakePiece), split at the spine fractions drawPieces uses
    const float uw[4] = { 0, 4 / 11.0f, 8 / 11.0f, 1 };
    for (int k = 0; k < 3; k++) a.whale[k] = bakePiece(g, REF_WHALE, REF_WHALE * 0.11f, 0.55f, uw[k], uw[k + 1]);
    const float ud[3] = { 0, 0.5f, 1 };
    for (int k = 0; k < 2; k++) a.dolphin[k] = bakePiece(g, REF_DOL, REF_DOL * 0.12f, 0.45f, ud[k], ud[k + 1]);
    const float us[4] = { 0, 3 / 8.0f, 6 / 8.0f, 1 };
    for (int k = 0; k < 3; k++) a.shark[k] = bakePiece(g, REF_SHARK, REF_SHARK * 0.13f, 0.42f, us[k], us[k + 1]);
    a.whaleFluke = bake(24, 30, 1, 15, [&](Art& A) { lobe(A, 20, 13, -1, rgb(44, 56, 70)); lobe(A, 20, 13, 1, rgb(44, 56, 70)); });
    a.whaleFin = bake(46, 16, 1, 8, [&](Art& A) {   // a long pectoral blade, dark on top with a pale underside
        A.poly({ {0, -4.8f}, {14, -6.4f}, {28, -5.0f}, {40, 0}, {0, 0} }, rgb(40, 52, 66), 0.6f, 0.4f);
        A.poly({ {0, 0}, {40, 0}, {28, 4.6f}, {14, 6.0f}, {0, 4.6f} }, rgb(190, 204, 216), 0.6f, 0.2f);
    });
    a.dolFluke = bake(12, 14, 1, 7, [&](Art& A) { lobe(A, 9, 6, -1, rgb(96, 110, 126)); lobe(A, 9, 6, 1, rgb(96, 110, 126)); });
    a.sharkTail = bake(20, 24, 1, 12, [&](Art& A) { lobe(A, 17, 10, -1, rgb(60, 60, 58)); lobe(A, 10, 5.5f, 1, rgb(60, 60, 58)); });
    a.sharkPec = bake(18, 8, 1, 4, [&](Art& A) { A.poly({ {0, -2.4f}, {8, -3.2f}, {16, 0}, {8, 3.0f}, {0, 2.4f} }, rgb(54, 56, 56), 0.5f, 0.3f); });
    for (int sil = 0; sil < 2; sil++) for (int fr = 0; fr < 3; fr++) for (int h = 0; h < 16; h++) {
        // a fish seen from above: a dark back, silver along the flanks, the tail beating left, straight or right
        float th = h * 6.2831853f / 16, cs = std::cos(th), sn = std::sin(th), flick = (fr - 1) * 1.0f;
        auto Q = [cs, sn](float u, float v) { return P{ u * cs - v * sn, u * sn + v * cs }; };
        a.fish[sil][fr][h] = bake(9, 9, 4.5f, 4.5f, [&](Art& A) {
            Color body = sil ? rgb(214, 226, 234) : rgb(38, 58, 72), edge = sil ? rgb(255, 255, 255) : rgb(180, 198, 210);
            A.poly({ Q(3.6f, 0), Q(2.4f, -1.4f), Q(0, -1.75f), Q(-2.2f, -1.1f), Q(-2.2f, 1.1f), Q(0, 1.75f), Q(2.4f, 1.4f) }, body, 0.4f, 0.3f);
            A.poly({ Q(-2.0f, -0.5f), Q(-3.9f, -2.6f + flick), Q(-2.9f, flick * 0.3f) }, body, 0.3f, 0.2f);
            A.poly({ Q(-2.0f, 0.5f), Q(-2.9f, flick * 0.3f), Q(-3.9f, 2.6f + flick) }, body, 0.3f, 0.2f);
            P p1 = Q(2.2f, -1.0f), p2 = Q(-1.8f, -1.0f), p3 = Q(2.2f, 1.0f), p4 = Q(-1.8f, 1.0f);
            A.line(p1.x, p1.y, p2.x, p2.y, 0.9f, edge);
            A.line(p3.x, p3.y, p4.x, p4.y, 0.9f, edge);
        });
    }
    return a;
}

// a whale, seen from above, c its screen position: a long blue-grey back, pectoral fins and flukes that beat, a wet ridge as it rolls
void drawWhale(Gfx& g, const SeaArt& art, const Beast& b, Vec2 c, Color tint, float wt) {
    const float surf = b.surf, sc = b.len / REF_WHALE;
    static const int A3[3] = { 0, 4, 8 }, B3[3] = { 4, 8, 11 };
    Spine sp; makeSpine(sp, c, b.ang, b.len, b.wide, 0.55f, 12, 1.0f, b.phase, b.bend);
    Vec2 f(std::cos(b.ang), std::sin(b.ang)), sd(-f.y, f.x);
    u8 a = alphaOf(surf);
    Color fm = tinted(subm(rgb(255, 255, 255), surf), tint);   // the fins' and flukes' own colours, blended toward the water when under it
    if (surf < 0.8f) drawPieces(g, art.whale, A3, B3, 3, sp, sc, tinted(rgb(12, 34, 60), tint), (u8)(90 * (1 - surf)), Vec2(2.5f, 3.5f));   // shadow
    int kf = (int)(sp.n * 0.68f);   // the pectoral fins: long, out and back from the flanks, flapping slowly
    for (int side = -1; side <= 1; side += 2) {
        Vec2 root = sp.p[kf] + sd * (side * sp.r[kf] * 0.75f);
        g.draw(art.whaleFin, root.x, root.y, b.ang + 3.14159f - side * 0.8f + 0.12f * std::sin(b.phase * 0.7f + side), 1.0f, fm, a);
    }
    Vec2 tail = sp.p[0], td = (sp.p[0] - sp.p[1]).norm();   // the flukes beat, and rise as it dives
    g.draw(art.whaleFluke, tail.x - td.x * 3, tail.y - td.y * 3, std::atan2(td.y, td.x) + 0.1f * std::sin(b.phase * 0.8f), 1.0f + 0.08f * std::sin(b.phase * 1.6f) + 0.3f * b.fluke, fm, a);
    drawPieces(g, art.whale, A3, B3, 3, sp, sc, bodyMod(subm(rgb(66, 84, 102), surf), tint), a);
    if (surf > 0.3f) {   // the wet ridge of the back, running from the tail toward the snout as it rolls forward
        int k = clampi((int)((0.2f + 0.6f * b.roll) * (sp.n - 1)), 0, sp.n - 1);
        int j0 = std::max(0, k - 1), j1 = std::min(sp.n - 1, k + 1);
        Vec2 o = sd * (-0.3f * sp.r[k]);   // a thin sheen on the lit side of the back
        Color hl = tinted(rgb(230, 240, 250), tint);
        hl.a = (u8)(110 * surf);
        g.thickLine(sp.p[j0].x + o.x, sp.p[j0].y + o.y, sp.p[j1].x + o.x, sp.p[j1].y + o.y, hl, 1.5f);
    }
    if (surf > 0.2f) {   // white water rippling out from the back
        float p = std::fmod(wt * 0.6f + b.phase * 0.1f, 1.0f), rr = b.len * (0.42f + 0.25f * p);
        g.drawSized(g.fxs.ring, c.x, c.y, rr * 2, rr, 0, tinted(rgb(230, 242, 250), tint), (u8)(110 * surf * (1 - p)));
    }
}

// a dolphin, seen from above: a sleek grey torpedo with a dorsal fin and a beak when it breaks the surface, its flukes beating
void drawDolphin(Gfx& g, const SeaArt& art, const Beast& b, Vec2 c, Color tint) {
    const float surf = b.surf, sc = b.len / REF_DOL;
    static const int A2[2] = { 0, 3 }, B2[2] = { 3, 6 };
    Spine sp; makeSpine(sp, c, b.ang, b.len, b.wide, 0.45f, 7, 0.0f, b.phase, b.bend);
    Vec2 f(std::cos(b.ang), std::sin(b.ang));
    u8 a = alphaOf(surf);
    Color fm = tinted(subm(rgb(255, 255, 255), surf), tint);
    Vec2 tail = sp.p[0], td = (sp.p[0] - sp.p[1]).norm();
    g.draw(art.dolFluke, tail.x - td.x * 1.5f, tail.y - td.y * 1.5f, std::atan2(td.y, td.x), 1.0f + 0.12f * std::sin(b.phase), fm, a);
    drawPieces(g, art.dolphin, A2, B2, 2, sp, sc, bodyMod(subm(rgb(96, 112, 130), surf), tint), a);
    if (surf > 0.3f) {
        g.drawSized(art.fin, sp.p[3].x, sp.p[3].y, 7, 7, 0, tinted(rgb(96, 110, 126), tint), (u8)(255 * surf));
        Vec2 snout = sp.p[sp.n - 1] + f * 1.8f;
        g.drawSized(g.disc, snout.x, snout.y, 2.6f, 2.6f, 0, tinted(rgb(170, 182, 192), tint), a);
    }
    if (b.st == 1 || b.st == 3) {   // the splash ring where it breaks the surface or goes back in
        float s = 6 + 14 * b.e;
        g.drawSized(g.fxs.ring, c.x, c.y, s, s * 0.5f, 0, tinted(rgb(236, 246, 252), tint), (u8)(170 * (1 - b.e)));
    }
}

// a shark, seen from above: a dark grey-brown shape with its body swaying side to side, the pectorals out, the crescent tail sweeping,
// and the dorsal fin cutting the surface behind a thin V of wake
void drawShark(Gfx& g, const SeaArt& art, const Beast& b, Vec2 c, Color tint) {
    const float sc = b.len / REF_SHARK;
    static const int A3[3] = { 0, 3, 6 }, B3[3] = { 3, 6, 8 };
    Spine sp; makeSpine(sp, c, b.ang, b.len, b.wide, 0.42f, 9, b.st ? 6.0f : 4.0f, b.phase, b.bend);
    Vec2 f(std::cos(b.ang), std::sin(b.ang)), sd(-f.y, f.x);
    u8 a = alphaOf(0.35f);
    Color fm = tinted(subm(rgb(255, 255, 255), 0.35f), tint);
    drawPieces(g, art.shark, A3, B3, 3, sp, sc, tinted(rgb(10, 34, 60), tint), 70, Vec2(2.5f, 3.5f));   // shadow
    for (int side = -1; side <= 1; side += 2) {
        Vec2 root = sp.p[4] + sd * (side * sp.r[4] * 0.6f);
        g.draw(art.sharkPec, root.x, root.y, b.ang + 3.14159f - side * 0.9f + 0.15f * std::sin(b.phase + side), 1.0f, fm, a);
    }
    Vec2 tail = sp.p[0], td = (sp.p[0] - sp.p[1]).norm();
    g.draw(art.sharkTail, tail.x - td.x * 2, tail.y - td.y * 2, std::atan2(td.y, td.x) + 0.15f * std::sin(b.phase), 1.0f, fm, a);
    drawPieces(g, art.shark, A3, B3, 3, sp, sc, bodyMod(subm(rgb(72, 70, 64), 0.35f), tint), a);
    Vec2 fin = sp.p[5];
    for (int side = -1; side <= 1; side += 2) {   // the V wake behind the fin
        Vec2 e = fin - f * 22.0f + sd * (side * 10.0f);
        g.thickLine(fin.x, fin.y, e.x, e.y, rgb(228, 242, 250, 70), 1.0f);
    }
    g.drawSized(art.fin, fin.x, fin.y, 9, 9, 0, tinted(rgb(70, 74, 78), tint), 240);
}

// the step for one school this frame: the schools in view every frame, the others a quarter of the frames with four times the step
float seaStep(const Game& G, float dt, size_t si, bool near) {
    if (near) return dt;
    return (G.seaTick + (int)si) % 4 == 0 ? dt * 4 : 0.0f;
}

}  // namespace

// the surface moment of a whale (a column of spray from the blowhole) or a dolphin (a few drops)
void Game::seaBreach(const SeaBeast& b) {
    Rng& r = weather.rng;
    Vec2 f(std::cos(b.ang), std::sin(b.ang));
    if (b.kind == 0) {
        Vec2 hole = b.pos + f * (b.len * 0.34f);
        for (int k = 0; k < 16; k++) emitP(hole.x, hole.y - 4, r.f(-7, 7), -r.f(40, 90), r.f(1.4f, 2.6f), 5, 18, rgb(240, 246, 252, 190), PK_SMOKE, (u8)r.range(0, 3), 22, 0.6f, r.f(0, 6), 0.2f);
    } else {
        for (int k = 0; k < 4; k++) emitP(b.pos.x, b.pos.y, r.f(-14, 14), -r.f(18, 40), r.f(0.5f, 0.9f), 3, 6, rgb(230, 240, 248, 200), PK_SMOKE, (u8)r.range(0, 3), 22, 0.2f, r.f(0, 6), 0.2f);
    }
}

// one step of the sea life: the whales and dolphins rise and dive, the sharks cruise and burst, the pods keep formation, and every fish school flocks
void Game::seaUpdate(float dt) {
    Game& G = *this;
    Rng& r = weather.rng;
    G.seaTick++;
    // what the fish scatter from: the sharks, and the ships on the water
    std::vector<Vec2> threats;
    for (const Beast& b : G.sea) if (b.kind == 2) threats.push_back(b.pos);
    for (const Entity& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().naval) threats.push_back(e.pos);
    for (size_t i = 0; i < G.sea.size(); i++) {   // (a pod's leader comes before its members, so their formation follows this frame's leader)
        Beast& b = G.sea[i];
        const Game::SeaBody& body = G.seaBodies[b.bi];
        float turnRate = b.kind == 0 ? 0.32f : (b.kind == 1 ? 1.8f : 0.45f);
        b.t -= dt;
        if (b.kind == 2) {   // a shark cruises, then bursts for a second or so
            if (b.t <= 0) { b.st ^= 1; b.t = b.st ? r.f(1.0f, 1.6f) : r.f(6.0f, 12.0f); }
            b.surf = 0.35f; b.fluke = 0; b.roll = 0;   // its dorsal fin always cuts the surface
        } else {             // a whale or a dolphin: under, rising, at the surface, diving
            while (b.t <= 0) {
                b.st = (b.st + 1) & 3;
                b.dur = seaStateLen(b.kind, b.st, r);
                b.t += b.dur;
                if (b.st == 1) seaBreach(b);
            }
            b.e = clampf(1 - b.t / b.dur, 0, 1);
            float sm = b.e * b.e * (3 - 2 * b.e);
            b.surf = b.st == 0 ? 0 : (b.st == 1 ? sm : (b.st == 2 ? 1 : 1 - sm));
            b.roll = b.st == 2 ? b.e : 0;
            b.fluke = b.st == 3 ? std::sin(3.14159f * clampf(0.1f + 0.8f * b.e, 0, 1)) : (b.kind == 0 ? 0.15f : 0.0f);
        }
        // where it is heading and how fast: a pod member keeps its place beside the leader, the others wander the open water of their body
        Vec2 to;
        float want;
        if (b.lead >= 0) {
            const Beast& L = G.sea[b.lead];
            to = L.pos + rotv(b.off, L.ang) - b.pos;
            want = L.speed * clampf(to.len() / 24.0f, 0.4f, 1.5f);
        } else {
            if (b.kind == 1 && r.f() < dt * 0.25f) {   // a playful quick turn off to one side
                Vec2 q = b.pos + Vec2(std::cos(b.ang + r.f(-2.4f, 2.4f)), std::sin(b.ang + r.f(-2.4f, 2.4f))) * r.f(60, 100);
                if (openAt(q, body.id)) b.goal = q;
            }
            if ((b.goal - b.pos).len() < 2.5f * TILE || !openAt(b.goal, body.id))
                b.goal = seaGoal(r, body, b.pos, (b.kind == 0 ? 6.0f : (b.kind == 1 ? 3.0f : 4.0f)) * TILE);
            to = b.goal - b.pos;
            if (b.kind == 2) want = b.st ? 58.0f : 22.0f;
            else if (b.kind == 0) want = b.st == 0 ? 12.0f : (b.st == 2 ? 4.0f : (b.st == 3 ? 5.0f : 2.0f));
            else want = b.st == 0 ? 34.0f : 26.0f;
        }
        float wa = std::atan2(to.y, to.x), dd = wa - b.ang;
        while (dd > 3.14159f) dd -= 6.28318f;
        while (dd < -3.14159f) dd += 6.28318f;
        float step = clampf(dd, -turnRate * dt, turnRate * dt);
        b.ang += step;
        b.bend += (clampf(step / (turnRate * dt), -1, 1) * 0.35f * (b.kind == 1 ? 1.4f : 1.0f) - b.bend) * std::min(1.0f, dt * 4);
        b.speed += (want - b.speed) * std::min(1.0f, dt * 1.5f);
        b.phase += dt * (b.kind == 0 ? 1.1f : (b.kind == 1 ? 6.0f : (b.st ? 7.0f : 3.4f)));
        Vec2 np = b.pos + Vec2(std::cos(b.ang), std::sin(b.ang)) * (b.speed * dt);
        if (openAt(np, body.id)) b.pos = np;
        else if (b.lead < 0) b.goal = seaGoal(r, body, b.pos, 0.0f);   // a wall of land or another body: a new goal
    }
    // the fish: each school flocks (cohesion, alignment, separation, a pull toward its goal) and scatters from threats; the schools out of view
    // update every fourth tick with a longer step
    for (size_t si = 0; si < G.seaSchools.size(); si++) {
        Game::SeaSchool& S = G.seaSchools[si];
        const Game::SeaBody& body = G.seaBodies[S.bi];
        bool near = S.cen.x > G.cam.x - 200 && S.cen.x < G.cam.x + SCREEN_W + 200 && S.cen.y > G.cam.y - 200 && S.cen.y < G.cam.y + VIEW_H + 200;
        float h = seaStep(G, dt, si, near);
        if (h <= 0) continue;
        Vec2 cen, av;
        for (int k = 0; k < S.n; k++) { cen += G.seaFish[S.first + k].pos; av += G.seaFish[S.first + k].vel; }
        cen = cen * (1.0f / S.n);
        av = av * (1.0f / S.n);
        S.cen = cen;
        bool threatened = false;
        for (Vec2 t : threats) if (dist2(t, cen) < 130.0f * 130.0f) threatened = true;
        float fleeBefore = S.fleeT;
        S.fleeT = threatened ? 2.0f : std::max(0.0f, S.fleeT - h);
        if (threatened && fleeBefore <= 0) S.goal = seaGoal(r, body, cen, 6 * TILE);   // scatter: a new goal well away
        S.t -= h;
        S.glintT -= h;
        if (S.glintT <= 0) { S.glint = 0.001f; S.glintT = r.f(7, 12); S.goal = seaGoal(r, body, cen, 4 * TILE); }   // a synchronised turn, the silver wave runs down the school
        if (S.glint > 0) { S.glint += h; if (S.glint > 1.4f) S.glint = 0; }
        if (S.t <= 0 || (cen - S.goal).len() < 2.5f * TILE || !openAt(S.goal, body.id)) { S.goal = seaGoal(r, body, cen, 4 * TILE); S.t = r.f(6, 14); }
        S.speed += ((S.fleeT > 0 ? 52.0f : 24.0f) - S.speed) * std::min(1.0f, h * 1.2f);
        Vec2 gd = (S.goal - cen).norm();
        for (int k = 0; k < S.n; k++) {
            Game::SeaFish& fk = G.seaFish[S.first + k];
            Vec2 a = (cen - fk.pos) * 0.8f + (av - fk.vel) * 1.2f + gd * 14.0f;
            for (int m = 0; m < S.n; m++) {   // separation
                if (m == k) continue;
                Vec2 d = fk.pos - G.seaFish[S.first + m].pos;
                float dl = d.len();
                if (dl < 8.0f && dl > 0.01f) a += d * ((8.0f - dl) * 30.0f / dl);
            }
            for (Vec2 t : threats) {   // scatter from a shark or a ship
                Vec2 d = fk.pos - t;
                float dl = d.len();
                if (dl < 70.0f && dl > 0.01f) a += d * (400.0f * (70.0f - dl) / 70.0f / dl);
            }
            fk.vel += a * h;
            float sp = fk.vel.len(), hi = S.speed * 1.35f, lo = S.speed * 0.6f;
            if (sp > hi) fk.vel = fk.vel * (hi / sp);
            else if (sp < lo && sp > 0.01f) fk.vel = fk.vel * (lo / sp);
            Vec2 np = fk.pos + fk.vel * h;
            if (wetTile(np, body.id)) fk.pos = np;
            else fk.vel = (cen - fk.pos).norm() * sp;   // the edge of its body: turn back toward the school
            fk.ph += h * (6.0f + 0.15f * S.speed);
        }
    }
}

// ------------------------------------------------------------ the sky
void Game::initWeather() {
    weather = Weather();
    u64 s = seed ^ ((u64)time(nullptr) * 0x9E3779B97F4A7C15ull);   // every match a different sky (cosmetic, so it needs no replay)
    weather.rng = Rng(s ? s : 1);
    Rng& r = weather.rng;
    weather.phase0 = START_POINTS[r.range(0, 4)] * DAY_LEN;
    if (r.f() < 0.25f) { weather.rain = weather.rainTo = r.f(0.45f, 1.0f); weather.nextRoll = r.f(60, 150); }
    else weather.nextRoll = r.f(40, 120);
    weather.nextBolt = r.f(8, 20);
    if (const char* w = getenv("ONEHOUR_WEATHER")) {   // screenshots: dawn | morning | day | sunset | night [,rain | ,storm]
        std::string ws = w;
        static const char* NAMES[5] = { "dawn", "morning", "day", "sunset", "night" };
        for (int i = 0; i < 5; i++) if (ws.rfind(NAMES[i], 0) == 0) weather.phase0 = START_POINTS[i] * DAY_LEN;
        weather.rain = weather.rainTo = ws.find("storm") != std::string::npos ? 1.0f : (ws.find("rain") != std::string::npos ? 0.7f : 0.0f);
        weather.forced = true;
    }
    // the wildlife of the open water, found over the whole map: every body of 400 tiles or more, with the tiles that have water all round.
    // Whales swim in the biggest body only; dolphin pods, sharks and fish schools in each big body, scaled by its open water.
    seaBodies.clear(); sea.clear(); seaFish.clear(); seaSchools.clear();
    std::vector<int> slot(g_map.waterSize.size(), -1);
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        int id = g_map.waterComp[i];
        if (id <= 0 || id >= (int)slot.size() || g_map.waterSize[id] < SEA_MIN_BODY || !openTile(i % MAP_W, i / MAP_W, id)) continue;
        if (slot[id] < 0) { slot[id] = (int)seaBodies.size(); seaBodies.push_back(Game::SeaBody{ id, {} }); }
        seaBodies[slot[id]].open.push_back(i);
    }
    int big = -1;
    for (int k = 0; k < (int)seaBodies.size(); k++) if (big < 0 || seaBodies[k].open.size() > seaBodies[big].open.size()) big = k;
    auto newBeast = [&](int kind, int bi) {
        const Game::SeaBody& body = seaBodies[bi];
        Beast b;
        b.kind = (u8)kind; b.bi = (u16)bi;
        b.pos = seaGoal(r, body, Vec2(), 0.0f);
        b.goal = seaGoal(r, body, b.pos, 4.0f * TILE);
        b.ang = std::atan2(b.goal.y - b.pos.y, b.goal.x - b.pos.x);
        b.len = kind == 0 ? r.f(110, 150) : (kind == 1 ? r.f(24, 30) : r.f(34, 44));
        b.wide = b.len * (kind == 0 ? 0.11f : (kind == 1 ? 0.12f : 0.13f));
        b.speed = kind == 0 ? 12.0f : (kind == 1 ? 30.0f : 22.0f);
        b.t = b.dur = kind == 0 ? r.f(4, 16) : (kind == 1 ? r.f(0.5f, 6.0f) : r.f(2, 10));
        b.phase = r.f(0, 6.2832f);
        b.fluke = kind == 0 ? 0.15f : 0.0f;
        return b;
    };
    if (big >= 0) {   // whales: one per 6000 open tiles of the biggest body, 1 to 4
        int n = clampi((int)(seaBodies[big].open.size() / 6000), 1, 4);
        for (int k = 0; k < n; k++) sea.push_back(newBeast(0, big));
    }
    int pods = 0, sharks = 0, schools = 0;
    for (int bi = 0; bi < (int)seaBodies.size(); bi++) {
        int nOpen = (int)seaBodies[bi].open.size();
        for (int p = 0; p < nOpen / 3000 && pods < SEA_MAX_POD; p++, pods++) {   // dolphin pods: one per 3000 open tiles, 3 to 7 animals
            int lead = (int)sea.size();
            sea.push_back(newBeast(1, bi));
            int n = r.range(3, 7);
            for (int m = 1; m < n; m++) {
                Beast mb = newBeast(1, bi);
                mb.lead = lead;
                mb.off = Vec2(-r.f(6, 34), r.f(-24, 24));
                mb.ang = sea[lead].ang;
                mb.pos = sea[lead].pos + rotv(mb.off, mb.ang);
                mb.goal = mb.pos;
                sea.push_back(mb);
            }
        }
        for (int p = 0; p < nOpen / 2500 && sharks < SEA_MAX_SHARK; p++, sharks++) sea.push_back(newBeast(2, bi));
        for (int p = 0; p < nOpen / 700 && schools < SEA_MAX_SCHOOL; p++, schools++) {   // fish schools: one per 700 open tiles, 12 to 30 fish
            Game::SeaSchool S;
            S.bi = bi;
            S.first = (int)seaFish.size();
            S.n = r.range(12, 30);
            S.cen = seaGoal(r, seaBodies[bi], Vec2(), 0.0f);
            S.goal = seaGoal(r, seaBodies[bi], S.cen, 6 * TILE);
            S.t = r.f(6, 14);
            S.glintT = r.f(3, 9);
            for (int k = 0; k < S.n; k++) {
                Game::SeaFish f;
                f.pos = S.cen + Vec2(r.f(-14, 14), r.f(-14, 14));
                float a = r.f(0, 6.2832f);
                f.vel = Vec2(std::cos(a), std::sin(a)) * S.speed;
                f.ph = r.f(0, 6.2832f);
                seaFish.push_back(f);
            }
            seaSchools.push_back(S);
        }
    }
}

bool Game::seaFree(Vec2 p, int bi) const { return bi >= 0 && bi < (int)seaBodies.size() && openAt(p, seaBodies[bi].id); }

void Game::updateWeather() {
    float now = g_sim.time, dt = weather.lastT < 0 ? 0.0f : clampf(now - weather.lastT, 0.0f, 0.5f);
    weather.lastT = now;
    if (dt <= 0) return;
    Rng& r = weather.rng;
    if (!weather.forced && now >= weather.nextRoll) {
        if (weather.rainTo > 0) { weather.rainTo = 0; weather.nextRoll = now + r.f(150, 320); }        // the rain stops: a dry spell
        else if (r.f() < 0.45f) { weather.rainTo = r.f(0.45f, 1.0f); weather.nextRoll = now + r.f(80, 210); }   // a shower or a storm rolls in
        else weather.nextRoll = now + r.f(60, 130);
    }
    float step = dt / 18.0f;   // the sky turns over about 18 s
    weather.rain += clampf(weather.rainTo - weather.rain, -step, step);
    weather.bolt *= std::exp(-dt * 7.0f);
    if (weather.rain > 0.75f && now >= weather.nextBolt) {
        weather.bolt = 1.0f; weather.nextBolt = now + r.f(10, 32);
        g_audio.play(SND_EXPLODE_L, Vec2(), true, 0.18f);   // the thunder rolls in a moment later: a low distant boom
    }
    seaUpdate(dt);   // the creatures swim on whether anybody looks or not
}

float Game::dayPhase() const { return std::fmod(weather.phase0 + (weather.forced ? 0.0f : std::max(0.0f, g_sim.time)), DAY_LEN) / DAY_LEN; }   // (a forced sky holds its hour)

Color Game::ambientTint() const {
    float p = dayPhase(), cr = 1, cg = 1, cb = 1;
    for (size_t i = 0; i + 1 < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        const Key &a = KEYS[i], &b = KEYS[i + 1];
        if (p < a.p || p > b.p) continue;
        float t = (p - a.p) / std::max(1e-4f, b.p - a.p);
        cr = a.r + (b.r - a.r) * t; cg = a.g + (b.g - a.g) * t; cb = a.b + (b.b - a.b) * t;
        break;
    }
    float w = weather.rain;   // clouds and rain: greyer and dimmer
    cr *= 1 - 0.20f * w; cg *= 1 - 0.16f * w; cb *= 1 - 0.08f * w;
    if (g_sim.numPlayers > 0 && g_sim.revealed(g_sim.humanPlayer)) {   // a scan is running: the satellite / spotter picture is light-amplified, near daylight at any hour
        const Player& hp = g_sim.players[g_sim.humanPlayer];
        float k = 0.85f * clampf((hp.revealUntil - g_sim.time) / 1.5f, 0, 1) * clampf((g_sim.time - (hp.revealUntil - SCANS[hp.faction].duration)) / 1.5f, 0, 1);   // (eased in and out)
        cr += (1 - cr) * k; cg += (1 - cg) * k; cb += (0.98f - cb) * k;
    }
    return rgb((int)clampf(cr * 255, 0, 255), (int)clampf(cg * 255, 0, 255), (int)clampf(cb * 255, 0, 255));
}

float Game::nightLevel() const {
    Color t = ambientTint();
    return clampf((0.9f - (t.r + t.g + t.b) / (3.0f * 255.0f)) / 0.32f, 0.0f, 1.0f);
}

const char* Game::weatherLabel() const {
    float p = dayPhase();
    bool storm = weather.rain > 0.75f, wet = weather.rain > 0.15f;
    static char buf[40];
    const char* hour = p < 0.10f ? "Dawn" : (p < 0.20f ? "Morning" : (p < 0.50f ? "Day" : (p < 0.58f ? "Sunset" : (p < 0.65f ? "Dusk" : "Night"))));
    snprintf(buf, sizeof buf, "%s%s", hour, storm ? ", storm" : (wet ? ", rain" : ""));
    return buf;
}

// ------------------------------------------------------------ drawing
void Game::drawSeaLife() {
    if (sea.empty() && seaSchools.empty()) return;
    Gfx& g = g_gfx;
    SeaArt& art = seaArt();
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    Color tint = ambientTint();
    auto seen = [&](Vec2 p) { return rev || ex[clampi(tileOf(p.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(p.x), 0, MAP_W - 1)]; };
    for (const Beast& b : sea) {
        Vec2 s = worldToScreen(b.pos);
        if (s.x < -120 || s.y < -120 || s.x > SCREEN_W + 120 || s.y > VIEW_H + 120) continue;
        if (!seen(b.pos)) continue;
        if (b.kind == 0) drawWhale(g, art, b, s, tint, wallTime);
        else if (b.kind == 1) drawDolphin(g, art, b, s, tint);
        else drawShark(g, art, b, s, tint);
    }
    for (const Game::SeaSchool& S : seaSchools) {
        Vec2 sc = worldToScreen(S.cen);
        if (sc.x < -60 || sc.y < -60 || sc.x > SCREEN_W + 60 || sc.y > VIEW_H + 60) continue;
        for (int k = 0; k < S.n; k++) {
            const Game::SeaFish& fk = seaFish[S.first + k];
            Vec2 s = worldToScreen(fk.pos);
            if (s.x < -10 || s.y < -10 || s.x > SCREEN_W + 10 || s.y > VIEW_H + 10 || !seen(fk.pos)) continue;
            int hd = ((int)std::lround(std::atan2(fk.vel.y, fk.vel.x) / 6.2831853f * 16.0f) % 16 + 16) % 16;   // the nearest of 16 headings
            float sn = std::sin(fk.ph);
            int fr = sn > 0.4f ? 0 : (sn < -0.4f ? 2 : 1);
            float wave = S.glint - k * 0.5f / S.n;   // the silver wave runs down the school
            bool shine = S.glint > 0 && wave >= 0 && wave < 0.22f;
            g.draw(art.fish[shine ? 1 : 0][fr][hd], s.x, s.y, 0, 1.0f, tint, shine ? 255 : 210);
        }
    }
}

void Game::drawWeatherTint() {
    Gfx& g = g_gfx;
    Color t = ambientTint();
    if (g.vignette.tex && !g.softwareRenderer) {
        // The light of the hour and the soft dark corners in ONE multiply over the view: a small texture holding the tint times the vignette's
        // darkening, stretched bilinearly (both are smooth). It was two full-screen passes, the vignette's a blend over everything at the end.
        static SDL_Texture* lit = nullptr; static Color last{ 0, 0, 0, 0 };
        const int LW = 64, LH = 40;
        if (!lit) {
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
            lit = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, LW, LH);
            SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
            if (lit) SDL_SetTextureBlendMode(lit, SDL_BLENDMODE_MOD);
        }
        if (lit && (t.r != last.r || t.g != last.g || t.b != last.b || last.a == 0)) {
            static u32 px[LW * LH];
            for (int y = 0; y < LH; y++) for (int x = 0; x < LW; x++) {
                float dx = (x + 0.5f) / LW * 2 - 1, dy = (y + 0.5f) / LH * 2 - 1, d = std::sqrt(dx * dx + dy * dy);
                float a = clampf((d - 0.62f) / 0.75f, 0, 1); a = a * a * (3 - 2 * a) * (105.0f / 255.0f);   // (the vignette's own falloff)
                float k = 1.0f - a;
                px[y * LW + x] = Canvas::pack(Color{ (u8)(t.r * k), (u8)(t.g * k), (u8)(t.b * k), 255 });
            }
            SDL_UpdateTexture(lit, nullptr, px, LW * 4);
            last = t; last.a = 255;
        }
        if (lit) { SDL_Rect r = { 0, 0, SCREEN_W, VIEW_H }; SDL_RenderCopy(g.ren, lit, nullptr, &r); }
    } else if (t.r < 252 || t.g < 252 || t.b < 252) {   // one multiply over the battlefield (nothing at all in plain daylight)
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_MOD);
        SDL_SetRenderDrawColor(g.ren, t.r, t.g, t.b, 255);
        SDL_Rect r = { 0, 0, SCREEN_W, VIEW_H };
        SDL_RenderFillRect(g.ren, &r);
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
    }
    float night = nightLevel();
    if (night <= 0.05f) return;
    // the lights of the night: lit windows on every structure, headlights on the vehicles (only what is on screen and seen)
    float vx0 = cam.x - 64, vy0 = cam.y - 64, vx1 = cam.x + SCREEN_W + 64, vy1 = cam.y + VIEW_H + 64;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.carrier.valid()) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!g_sim.shownToHuman(e)) continue;   // (an enemy vehicle out of sight shows no headlights)
        if (!g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        Vec2 p = worldToScreen(entPos(e));
        if (e.isBuilding()) {
            if (!e.constructed || e.bt().role == BR_WALL || e.bt().role == BR_FWALL || e.bt().role == BR_WAYMAKER) continue;
            const BuildType& bt = e.bt();
            if (bt.role == BR_HEALANT) {   // a Healing Antenna has no windows: its green beacon glows at the top of the mast
                float pulse = 0.75f + 0.25f * std::sin(wallTime * 3.0f + e.gen);
                g.glowAdd(p.x, p.y - 20, 8.0f, Color{120, 255, 170, (u8)(150 * night * pulse)});
                g.glowAdd(p.x, p.y, TILE * 0.9f, Color{120, 255, 170, (u8)(20 * night)});
                continue;
            }
            bool cyber = bt.faction == F_CYBER;
            Color lc = e.owner < 0 ? rgb(255, 200, 120) : (cyber ? rgb(170, 220, 255) : rgb(255, 196, 110));
            int n = std::min(4, 1 + bt.w * bt.h / 4);
            for (int k = 0; k < n; k++) {
                float hx = (hashf(e.gen * 31u + k * 7u + 1u) - 0.5f) * bt.w * TILE * 0.7f, hy = (hashf(e.gen * 17u + k * 13u + 5u) - 0.5f) * bt.h * TILE * 0.5f - 6;
                float flick = 0.85f + 0.15f * std::sin(wallTime * 2.3f + k + e.gen);
                g.glowAdd(p.x + hx, p.y + hy, 9.0f + 3.0f * (k & 1), Color{lc.r, lc.g, lc.b, (u8)(110 * night * flick)});
            }
            g.glowAdd(p.x, p.y, std::max(bt.w, bt.h) * TILE * 0.55f, Color{lc.r, lc.g, lc.b, (u8)(26 * night)});   // light spilling onto the yard
        } else if (e.isUnit() && e.ut().kind == UK_VEH) {
            Vec2 f(std::cos(e.angle), std::sin(e.angle));
            float r = e.radius();
            Color hl = rgb(255, 244, 210);
            g.glowAdd(p.x + f.x * (r + 10), p.y + f.y * (r + 10), 11.0f, Color{hl.r, hl.g, hl.b, (u8)(70 * night)});   // the beam on the ground ahead
            for (int s = -1; s <= 1; s += 2) g.glowAdd(p.x + f.x * r * 0.9f - f.y * s * r * 0.4f, p.y + f.y * r * 0.9f + f.x * s * r * 0.4f, 3.5f, Color{hl.r, hl.g, hl.b, (u8)(170 * night)});
        }
    }
}

void Game::drawRain() {
    Gfx& g = g_gfx;
    float w = weather.rain;
    if (w > 0.02f) {
        int n = (int)(280 * w);
        float t = wallTime;
        const float fall = 900.0f, wind = 170.0f;   // px/s down and across the screen
        Color c = rgb(196, 210, 226, (int)(70 + 70 * w));
        for (int i = 0; i < n; i++) {
            float sx = hashf(i * 2654435761u + 11u), sy = hashf(i * 2246822519u + 7u), sp = 0.75f + 0.5f * hashf(i * 3266489917u + 3u);
            float x = std::fmod(sx * (SCREEN_W + 200) + t * wind * sp - cam.x * 0.25f, (float)(SCREEN_W + 200)) - 100;
            float y = std::fmod(sy * (VIEW_H + 120) + t * fall * sp - cam.y * 0.25f, (float)(VIEW_H + 120)) - 60;
            if (x < -100) x += SCREEN_W + 200;
            if (y < -60) y += VIEW_H + 120;
            float len = 12.0f * sp;
            g.line(x, y, x - wind / fall * len, y - len, c);
        }
        // splashes: rings that flash on the ground where drops land, a fresh set every tenth of a second
        int slot = (int)(t * 10.0f), m = (int)(40 * w);
        for (int i = 0; i < m; i++) {
            u32 hsh = (u32)slot * 9973u + (u32)i * 7919u;
            float x = hashf(hsh) * SCREEN_W, y = hashf(hsh + 1u) * VIEW_H, a = 1.0f - std::fmod(t * 10.0f, 1.0f);
            g.drawSized(g.fxs.ring, x, y, 5.0f + 6.0f * (1 - a), 2.5f + 3.0f * (1 - a), 0, rgb(220, 232, 244), (u8)(80 * a * w));   // (a ripple that widens as it fades)
        }
    }
    if (weather.bolt > 0.02f) {   // lightning: the whole field flashes white for an instant
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(g.ren, 200, 210, 255, (u8)(150 * weather.bolt));
        SDL_Rect r = { 0, 0, SCREEN_W, VIEW_H };
        SDL_RenderFillRect(g.ren, &r);
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
    }
}
