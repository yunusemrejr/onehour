// One Hour - the relic structures: the bases of a third army, abandoned decades ago and taken back by the jungle. Weathered ochre and warm-grey
// concrete, bronze fittings gone green with verdigris, old camouflage faded to dust, dim amber lamps and stencilled numbers half hidden under
// vines, moss, leaf clumps, roots and small trees. Same raised 3/4 view and north-west light as bld.h, and the overgrowth uses the terrain's
// tree recipe (dark underside, mid clusters, lit tops, loose leaves). Also the river bridge, a sprite of its own laid over the river tiles.
#include "bld.h"

namespace {

// ---------------------------------------------------------------- palette
const Color RL_PAD = rgb(152, 140, 112), RL_SEAM = rgb(72, 64, 50);
const Color RL_CONC = rgb(166, 154, 126), RL_CONC_TOP = rgb(186, 174, 146), RL_CONC_W = rgb(150, 138, 112);
const Color RL_CONC_D = rgb(104, 95, 78), RL_CONC_L = rgb(206, 196, 168);
const Color RL_VERD = rgb(88, 150, 130), RL_VERD_L = rgb(152, 206, 178), RL_VERD_D = rgb(46, 90, 82);
const Color RL_RUST = rgb(150, 92, 52), RL_ORANGE = rgb(184, 112, 58), RL_OCHRE = rgb(196, 160, 90);
const Color RL_IRON = rgb(120, 116, 102), RL_IRON_D = rgb(74, 72, 64), RL_IRON_L = rgb(160, 156, 140);
const Color RL_CAM_A = rgb(134, 124, 92), RL_CAM_B = rgb(108, 102, 78), RL_CAM_C = rgb(168, 152, 114);
const Color RL_LEAF_D = rgb(22, 50, 30), RL_LEAF_M = rgb(44, 90, 44), RL_LEAF_L = rgb(82, 124, 60);
const Color RL_MOSS = rgb(74, 104, 52), RL_MOSS_L = rgb(112, 146, 74);
const Color RL_VINE = rgb(34, 60, 28), RL_VINE_L = rgb(78, 112, 50);
const Color RL_TRUNK = rgb(92, 66, 42), RL_ROOT = rgb(84, 60, 40), RL_ROOT_L = rgb(138, 104, 66);
const Color RL_LITTER = rgb(166, 118, 54), RL_LITTER_D = rgb(118, 82, 40);
const Color RL_AMBER = rgb(218, 152, 66), RL_STENCIL = rgb(228, 216, 178);
const Color RL_DARK = rgb(20, 22, 20), RL_GLASS = rgb(48, 64, 58);
const Color RL_DECK = rgb(150, 140, 116), RL_WATER = rgb(42, 98, 116), RL_FOAM = rgb(226, 238, 236);
const Color RL_EARTH = rgb(118, 104, 78);

// ---------------------------------------------------------------- overgrowth and weathering

// one leaf: a pointed diamond from (x, y) along angle ang, len long
void leaf(Art& a, float x, float y, float ang, float len, Color c) {
    float dx = std::cos(ang), dy = std::sin(ang), px = -dy * len * 0.22f, py = dx * len * 0.22f;
    a.poly({{x, y}, {x + dx * len * 0.5f + px, y + dy * len * 0.5f + py}, {x + dx * len, y + dy * len}, {x + dx * len * 0.5f - px, y + dy * len * 0.5f - py}}, c, 0.3f, 0.0f);
}

// a leaf clump (the terrain trees' recipe): a dark underside pushed south-east, mid-green clusters with lit tops and loose leaves on top
void leafBlob(Art& a, float cx, float cy, float R, u32 seed) {
    Rng r(seed);
    a.shadowCircle(cx, cy, R * 0.9f, R * 0.2f, R * 0.25f, 2.0f + R * 0.2f, 95);
    for (int i = 0; i < 7; i++) {
        float an = i * 2.3f + r.f(0, 1), d = R * (0.3f + 0.35f * r.f()), rr = R * r.f(0.32f, 0.46f);
        a.circle(cx + R * 0.1f + std::cos(an) * d, cy + R * 0.12f + std::sin(an) * d, rr, RL_LEAF_D, rr * 0.9f, 0.58f);
    }
    for (int i = 0; i < 6; i++) {
        float an = i * 2.1f + r.f(0, 1), d = R * 0.5f * r.f(0.1f, 1.0f), rr = R * r.f(0.24f, 0.36f);
        float mx = cx - R * 0.08f + std::cos(an) * d, my = cy - R * 0.1f + std::sin(an) * d;
        a.circle(mx, my, rr, RL_LEAF_M, rr * 0.9f, 0.6f);
        a.circle(mx - rr * 0.22f, my - rr * 0.26f, rr * 0.5f, shade(RL_LEAF_M, 1.2f), rr * 0.4f, 0.6f);
    }
    for (int i = 0; i < 3; i++) {
        float rr = R * r.f(0.12f, 0.2f), ox = r.f(-0.18f, 0.18f) * R, oy = r.f(-0.18f, 0.18f) * R;
        a.circle(cx - R * 0.22f + ox, cy - R * 0.26f + oy, rr, RL_LEAF_L, rr * 0.9f, 0.6f);
    }
    int n = (int)(R * 2.6f);
    for (int i = 0; i < n; i++) {
        float an = r.f(0, 6.2832f), d = std::sqrt(r.f()) * R * 0.9f;
        Color cc = r.f() < 0.5f ? shade(RL_LEAF_L, 1.05f) : shade(RL_LEAF_D, 0.8f);
        cc.a = 200;
        a.dot(cx + std::cos(an) * d, cy + std::sin(an) * d, 0.8f, cc);
    }
}

// a small tree rooted at (cx, by): a leaning trunk and a leaf crown of radius R above it
void smallTree(Art& a, float cx, float by, float R, u32 seed) {
    a.line(cx + 2.0f, by + 1.5f, cx + 3.5f, by - R * 0.8f + 1.5f, 1.2f, rgba(BLACK, 60));   // the trunk's shadow
    a.cap(cx, by, cx + R * 0.12f, by - R * 0.85f, 1.1f, RL_TRUNK, 0.8f, 0.4f);
    leafBlob(a, cx, by - R, R, seed);
}

// a vine: a wavering stem from (x0, y0) to (x1, y1) with a lit edge and leaves on both sides
void vine(Art& a, float x0, float y0, float x1, float y1, float w, float sway, u32 seed, int leaves) {
    Rng r(seed);
    const int n = 12;
    P pts[n + 1];
    float ph = r.f(0, 6.28f);
    for (int i = 0; i <= n; i++) {
        float t = i / (float)n;
        pts[i] = P{x0 + (x1 - x0) * t + std::sin(t * 5.0f + ph) * sway, y0 + (y1 - y0) * t};
    }
    for (int i = 0; i < n; i++) a.cap(pts[i].x, pts[i].y, pts[i + 1].x, pts[i + 1].y, w * 0.5f, RL_VINE, w * 0.35f, 0.3f);
    for (int i = 0; i < n; i++) a.line(pts[i].x - w * 0.25f, pts[i].y, pts[i + 1].x - w * 0.25f, pts[i + 1].y, w * 0.25f, rgba(RL_VINE_L, 210));
    for (int k = 0; k < leaves; k++) {
        int i = r.range(0, n - 1);
        float t = r.f();
        float px = pts[i].x + (pts[i + 1].x - pts[i].x) * t, py = pts[i].y + (pts[i + 1].y - pts[i].y) * t;
        float an = r.f(0, 6.28f), len = r.f(3.5f, 5.5f);
        leaf(a, px, py, an, len, r.f() < 0.5f ? shade(RL_LEAF_M, 1.1f) : RL_LEAF_L);
    }
}

// a curtain of vines hanging from an edge: n stems spread over x0..x1, each from y0 down to a varying length toward y1
void vineCurtain(Art& a, float x0, float x1, float y0, float y1, int n, u32 seed) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        float x = x0 + (x1 - x0) * (i + 0.5f) / n + r.f(-2, 2);
        float len = (y1 - y0) * r.f(0.55f, 1.0f), dx = r.f(-2, 2), w = r.f(1.8f, 2.6f), sway = r.f(0.8f, 1.8f);
        vine(a, x, y0, x + dx, y0 + len, w, sway, seed * 13u + (u32)i * 7u, 7);
    }
}

// a root pushing through a slab joint: a tapering dark strand with a lit upper edge
void root(Art& a, float x0, float y0, float x1, float y1, float w, u32 seed) {
    Rng r(seed);
    float mx = (x0 + x1) * 0.5f + r.f(-3, 3), my = (y0 + y1) * 0.5f + r.f(-3, 3);
    a.cap(x0, y0, mx, my, w * 0.5f, RL_ROOT, w * 0.4f, 0.3f);
    a.cap(mx, my, x1, y1, w * 0.4f, RL_ROOT, w * 0.4f, 0.3f);
    a.line(x0, y0 - w * 0.25f, mx, my - w * 0.25f, w * 0.22f, rgba(RL_ROOT_L, 190));
}

// a patch of moss on a surface: a soft green blot with lighter specks
void moss(Art& a, float cx, float cy, float rx, float ry, u32 seed, int alpha = 150) {
    Rng r(seed);
    a.ell(cx, cy, rx, ry, rgba(RL_MOSS, alpha), 1.4f, 0.35f);
    int n = 6 + (int)(rx * ry * 0.1f);
    for (int i = 0; i < n; i++) {
        float an = r.f(0, 6.2832f), d = std::sqrt(r.f()) * 0.85f, sz = 0.6f + r.f(0, 0.5f);
        Color sp = r.f() < 0.5f ? RL_MOSS_L : RL_LEAF_M;
        a.dot(cx + std::cos(an) * rx * d, cy + std::sin(an) * ry * d, sz, rgba(sp, 210));
    }
}

// old camouflage paint faded on concrete: soft blotches in three dusty tones, kept inside (x0..x1, y0..y1)
void camo(Art& a, float x0, float y0, float x1, float y1, int n, u32 seed, int alpha = 160) {
    Rng r(seed);
    const Color cols[3] = {RL_CAM_A, RL_CAM_B, RL_CAM_C};
    for (int i = 0; i < n; i++) {
        float rx = r.f(4.0f, 8.0f), ry = r.f(2.5f, 4.5f);
        float cx = r.f(x0 + rx, x1 - rx), cy = r.f(y0 + ry, y1 - ry);
        a.ell(cx, cy, rx, ry, rgba(cols[i % 3], alpha), 1.5f, 0.2f);
    }
}

// leaf litter drifted in a band (x0..x1, y0..y1): dry leaves in two browns, lying flat
void litter(Art& a, float x0, float y0, float x1, float y1, int n, u32 seed) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        float lx = r.f(x0, x1), ly = r.f(y0, y1), an = r.f(0, 3.14f), len = r.f(2.6f, 4.2f);
        leaf(a, lx, ly, an, len, r.f() < 0.5f ? RL_LITTER : RL_LITTER_D);
    }
}

// stencilled digits: seven-segment strokes of height h in the faded cream of old stencil paint (other characters leave a gap)
void stencil(Art& a, float x, float y, float h, const char* s, Color col) {
    static const char* const segs[10] = {"abcdef", "bc", "abged", "abgcd", "fgbc", "afgcd", "afgedc", "abc", "abcdefg", "abcdfg"};
    struct Seg { char c; float x0, y0, x1, y1; };
    float w = h * 0.6f, t = std::max(0.8f, h * 0.17f), e = t * 0.5f;
    for (const char* p = s; *p; p++) {
        if (*p < '0' || *p > '9') { x += w * 0.7f; continue; }
        const char* m = segs[*p - '0'];
        float L = x + e, R = x + w - e, T = y + e, B = y + h - e, M = y + h * 0.5f;
        const Seg sg[7] = {{'a', L, T, R, T}, {'b', R, T, R, M}, {'c', R, M, R, B}, {'d', L, B, R, B}, {'e', L, M, L, B}, {'f', L, T, L, M}, {'g', L, M, R, M}};
        for (const Seg& q : sg) {
            bool on = false;
            for (const char* z = m; *z; z++) if (*z == q.c) on = true;
            if (on) a.line(q.x0, q.y0, q.x1, q.y1, t, col);
        }
        x += w + t * 1.4f;
    }
}

// a dim amber signal lamp: a soft halo and a small lens with a dull highlight
void dimLamp(Art& a, float x, float y, float r) {
    a.glow(x, y, r * 3.4f, rgba(RL_AMBER, 70));
    a.dot(x, y, r, shade(RL_AMBER, 0.95f));
    a.dot(x - r * 0.3f, y - r * 0.3f, r * 0.35f, rgba(rgb(255, 228, 170), 150));
}

// a broken window in a concrete frame: a dark opening with a dull glow low inside, jagged glass left in the top corners and a crack
void brokenWindow(Art& a, float x, float y, float w, float h, u32 seed) {
    Rng r(seed);
    a.rect(x - 1.0f, y - 1.0f, w + 2.0f, h + 2.0f, 0.4f, RL_CONC_D, 0.7f, 0.3f);
    a.rect(x, y, w, h, 0.3f, RL_DARK, 0.8f, 0.1f);
    a.rect(x + 0.6f, y + h * 0.7f, w - 1.2f, h * 0.3f - 0.6f, 0.2f, rgba(RL_AMBER, 70), 0.6f, 0.0f);
    a.poly({{x + 0.4f, y + 0.4f}, {x + w * (0.25f + 0.2f * r.f()), y + 0.4f}, {x + 0.4f, y + h * 0.4f}}, rgba(RL_GLASS, 230), 0.4f, 0.0f);
    a.poly({{x + w - 0.4f, y + 0.4f}, {x + w * 0.66f, y + 0.4f}, {x + w - 0.4f, y + h * 0.3f}}, rgba(RL_GLASS, 230), 0.4f, 0.0f);
    a.line(x + w * 0.5f, y, x + w * 0.38f, y + h * 0.7f, 0.4f, rgba(RL_CONC_L, 140));
}

// a bronze fitting gone green: a verdigris plate with four rivets, a bloom of lighter green and a rust bleed running down under it
void verdPlate(Art& a, float x, float y, float w, float h, u32 seed) {
    Rng r(seed);
    a.rect(x, y, w, h, 0.6f, RL_VERD, 0.8f, 0.5f);
    a.ell(x + w * r.f(0.3f, 0.7f), y + h * 0.5f, w * 0.3f, h * 0.3f, rgba(RL_VERD_L, 110), 0.8f, 0.2f);
    for (float dx : {1.4f, w - 1.4f}) for (float dy : {1.4f, h - 1.4f}) a.dot(x + dx, y + dy, 0.45f, RL_VERD_D);
    a.rect(x + w * 0.4f, y + h, 1.6f, 4.0f + r.f(0, 3), 0.6f, rgba(RL_RUST, 110), 0.5f, 0.0f);
}

// a rust bleed: a faint orange-brown streak running down a wall from a fitting
void rustBleed(Art& a, float x, float y0, float y1, float w) { a.rect(x - w * 0.5f, y0, w, y1 - y0, w * 0.5f, rgba(RL_RUST, 80), 1.0f, 0.0f); }

// the vertices of a regular n-gon of circumradius r round (cx, cy), the first corner at angle rot
std::vector<P> ngonPts(float cx, float cy, float r, int n, float rot) {
    std::vector<P> v;
    for (int i = 0; i < n; i++) { float t = rot + i * 6.2831853f / n; v.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r}); }
    return v;
}

// the concrete apron of a base, cracked on a 32 px grid: moss and weeds in the joints, roots heaving the slabs, cracks, leaf litter along the foot
void relicPad(Bld& b, u32 seed) {
    Art& a = b.a;
    Rng r(seed);
    float W = (float)b.W, H = (float)b.H;
    a.shadowRect(0, 0, W, H, 3, 3, 4, 85);
    a.rect(0, 0, W, H, 4, RL_PAD, 2.4f, 0.5f);
    for (float x = 32; x < W - 1; x += 32) a.line(x, 1, x, H - 1, 0.8f, rgba(RL_SEAM, 150));
    for (float y = 32; y < H - 1; y += 32) a.line(1, y, W - 1, y, 0.8f, rgba(RL_SEAM, 150));
    for (float x = 32; x < W; x += 32) for (float y = 3; y < H - 2; y += 4) if (r.f() < 0.35f) a.dot(x + r.f(-0.6f, 0.6f), y, 0.7f, rgba(RL_MOSS, 190));
    for (float y = 32; y < H; y += 32) for (float x = 3; x < W - 2; x += 4) if (r.f() < 0.35f) a.dot(x, y + r.f(-0.6f, 0.6f), 0.7f, rgba(RL_MOSS, 190));
    int cracks = (int)(W * H / 2500.0f);
    for (int i = 0; i < cracks; i++) {
        float x = r.f(4, W - 4), y = r.f(4, H - 4), an = r.f(0, 6.2832f), l = r.f(5, 11);
        a.line(x, y, x + std::cos(an) * l, y + std::sin(an) * l, 0.8f, rgba(RL_SEAM, 160));
    }
    root(a, W * 0.2f, 40, W * 0.2f + 14, 52, 2.2f, seed + 1);
    root(a, W * 0.66f, H - 30, W * 0.66f + 12, H - 20, 2.0f, seed + 2);
    litter(a, 4, H - 9, W - 4, H - 3, (int)(W / 4), seed + 3);
    a.grain(0, 0, W, H, 0.12f, seed + 4);
}

// overgrowth on a roof: moss along the north edge and leaf clumps spread over the top
void roofGrowth(Bld& b, const Roof& r, int clumps, float R, u32 seed) {
    Rng q(seed);
    for (float x = r.x + 6; x < r.x + r.w - 4; x += q.f(12, 18)) moss(b.a, x, r.y + 1.6f, q.f(4, 7), 2.0f, seed * 3u + (u32)x, 170);
    for (int i = 0; i < 2 * clumps + 2; i++) {
        float mx = q.f(r.x + 4, r.x + r.w - 4), my = q.f(r.y + 4, r.y + r.h - 4), mrx = q.f(4, 8), mry = q.f(2.5f, 4);
        moss(b.a, mx, my, mrx, mry, seed * 11u + (u32)i * 17u, 150);
    }
    for (int i = 0; i < clumps; i++) {
        float cx = q.f(r.x + R, r.x + r.w - R), cy = q.f(r.y + R * 0.8f, r.y + r.h - R * 0.8f), rr = R * q.f(0.8f, 1.1f);
        leafBlob(b.a, cx, cy, rr, seed * 7u + (u32)i * 131u);
    }
}

// a domed roof: a pale concrete dome with meridian seams, a verdigris band round its foot and a crown vent, lit from the north-west
void relicDome(Bld& b, float cx, float cy, float r) {
    Art& a = b.a;
    a.shadowCircle(cx, cy, r, r * 0.12f, r * 0.14f, 3, 110);
    a.circle(cx, cy, r, RL_CONC_L, r * 0.7f, 0.7f);
    for (int i = 0; i < 8; i++) {
        float an = i * 0.7854f + 0.2f;
        a.line(cx + std::cos(an) * r * 0.2f, cy + std::sin(an) * r * 0.2f, cx + std::cos(an) * r * 0.94f, cy + std::sin(an) * r * 0.94f, 0.6f, rgba(RL_CONC_D, 120));
    }
    a.ring(cx, cy, r * 0.55f, 0.6f, rgba(RL_CONC_D, 110), 0.5f, 0.0f);
    a.ring(cx, cy, r - 1.4f, 2.6f, RL_VERD, 0.8f, 0.4f);   // the verdigris band round the foot
    for (int i = 0; i < 12; i++) {
        float an = i * 0.5236f;
        a.dot(cx + std::cos(an) * (r - 1.4f), cy + std::sin(an) * (r - 1.4f), 0.5f, RL_VERD_L);
    }
    a.circle(cx, cy, r * 0.16f, RL_IRON_D, 1.0f, 0.3f);   // crown vent
}

// a lattice mast standing at (x, by) and rising ht: two leaning legs, cross bracing, a cross arm and a dim lamp at the top
void lattice(Art& a, float x, float by, float ht) {
    float top = by - ht;
    a.line(x + 2.0f, by + 1.0f, x + 2.6f, top + 1.0f, 1.0f, rgba(BLACK, 60));
    a.cap(x - 3.0f, by, x - 0.8f, top, 0.6f, RL_IRON_L, 0.5f, 0.3f);
    a.cap(x + 3.0f, by, x + 0.8f, top, 0.6f, RL_IRON_L, 0.5f, 0.3f);
    for (int k = 0; k < 4; k++) {
        float y0 = by - ht * k / 4.0f, y1 = by - ht * (k + 1) / 4.0f;
        float w0 = 3.0f - 2.2f * k / 4.0f, w1 = 3.0f - 2.2f * (k + 1) / 4.0f;
        a.line(x - w0, y0, x + w1, y1, 0.5f, RL_IRON);
        a.line(x + w0, y0, x - w1, y1, 0.5f, RL_IRON);
        a.line(x - w0, y0, x + w0, y0, 0.6f, RL_IRON_L);
    }
    a.line(x - 1.8f, top + ht * 0.3f, x + 1.8f, top + ht * 0.3f, 0.6f, RL_IRON_L);
    dimLamp(a, x, top, 1.1f);
}

// a vehicle door in a wall whose foot is baseY: a verdigris frame, a ribbed shutter lifted a little so a dark gap shows under it, a
// stencilled number on the leaf and a dull lamp over the lintel
void bigDoor(Bld& b, float x, float baseY, float w, float h, const char* num) {
    Art& a = b.a;
    float top = baseY - h, lift = 5.0f;
    a.rect(x - 2.0f, top - 2.0f, w + 4.0f, h + 2.0f, 0.6f, RL_VERD_D, 1.2f, 0.4f);
    a.rect(x, top, w, h, 0.2f, RL_DARK, 0.8f, 0.1f);
    a.rect(x + 1.0f, top + 1.0f, w - 2.0f, h - 2.0f - lift, 0.3f, RL_CONC_D, 1.0f, 0.4f);
    for (float yy = top + 4.0f; yy < baseY - lift - 2.0f; yy += 3.5f) a.line(x + 1.6f, yy, x + w - 1.6f, yy, 0.5f, rgba(RL_DARK, 120));
    stencil(a, x + w * 0.5f - 3.0f, top + 3.0f, 5.0f, num, rgba(RL_STENCIL, 170));
    a.rect(x + 1.0f, baseY - lift + 0.5f, w - 2.0f, lift - 1.5f, 0.2f, RL_DARK, 0.6f, 0.1f);   // the gap under the shutter
    dimLamp(a, x + 3.0f, top - 4.0f, 1.0f);
}

// ---------------------------------------------------------------- the structures

// 128 x 128: a domed command post. Two low wings flank a hall with a great concrete dome on its roof, comms dishes and two lattice masts.
void cmdPost(Bld& b) {
    Art& a = b.a;
    relicPad(b, 11);
    Roof wl = b.block(6, 92, 40, 30, 10, RL_CONC_TOP, RL_CONC_W, 2);     // roof 82..112, wall 112..122.6
    Roof wr = b.block(82, 92, 40, 30, 10, RL_CONC_TOP, RL_CONC_W, 2);
    camo(a, 9, 85, 43, 109, 3, 21);
    camo(a, 85, 85, 119, 109, 3, 22);
    roofGrowth(b, wl, 2, 6, 23);
    roofGrowth(b, wr, 2, 6, 24);
    smallTree(a, 12, 84, 9, 25);   // a tree growing out of the west wing's roof corner
    brokenWindow(a, 13, 114, 8, 6, 26);
    brokenWindow(a, 27, 114, 8, 6, 27);
    brokenWindow(a, 93, 114, 8, 6, 28);
    brokenWindow(a, 109, 114, 8, 6, 29);
    stencil(a, 84, 115, 4.5f, "14", rgba(RL_STENCIL, 170));
    b.mark(92, 98, 20, 3);   // the owner's band on the east wing's roof
    vine(a, 16, 112, 17, 122, 1.4f, 1.0f, 27, 4);

    // the hall: a long low block under a great dome, its south wall brokenly glazed, a door and a verdigris plate
    Roof hall = b.block(18, 34, 92, 52, 20, RL_CONC_TOP, RL_CONC_W, 3);   // roof 14..66, wall 66..86.6
    camo(a, 22, 18, 40, 34, 3, 31);
    camo(a, 90, 42, 106, 62, 2, 32);
    roofGrowth(b, hall, 3, 6, 33);
    relicDome(b, 64, 40, 22);
    lattice(a, 26, 30, 42);
    lattice(a, 104, 30, 42);
    b.dishTop(28, 56, 8, RL_CONC_D, RL_VERD);
    b.dishTop(100, 56, 8, RL_CONC_D, RL_VERD);
    leafBlob(a, 42, 26, 8, 34);   // leaves creeping over the dome's north-west shoulder
    brokenWindow(a, 26, 72, 9, 8, 35);
    brokenWindow(a, 42, 72, 9, 8, 36);
    brokenWindow(a, 84, 72, 9, 8, 37);
    brokenWindow(a, 98, 72, 9, 8, 38);
    b.door(64, 86, 16, 14, RL_VERD_D, RL_DARK);
    stencil(a, 62, 74, 5, "7", rgba(RL_STENCIL, 170));
    verdPlate(a, 76, 74, 6, 6, 39);
    rustBleed(a, 52, 68, 84, 2.4f);
    rustBleed(a, 92, 68, 82, 2.4f);
    vineCurtain(a, 20, 104, 66, 86, 6, 41);
    vineCurtain(a, 6, 44, 112, 122, 4, 44);
    vineCurtain(a, 84, 118, 112, 122, 4, 45);
}

// 96 x 96: a squat containment dome on a cylindrical drum, a radiator bank of cooling fins beside it and a faint amber glow in its cracked crown
void reactor(Bld& b) {
    Art& a = b.a;
    relicPad(b, 12);
    Roof rad = b.block(74, 52, 20, 38, 10, RL_CONC_TOP, RL_CONC_W, 2);   // roof 42..80, wall 80..90.6
    b.wallSeams(rad, 10, 2.2f, 120);                                       // cooling fins down the face
    for (float y = 46; y < 80; y += 8) a.line(rad.x + 1, y, rad.x + rad.w - 1, y, 0.5f, rgba(RL_CONC_D, 80));
    b.pipe(78, 44, 90, 40, 1.6f, RL_VERD);
    leafBlob(a, 88, 62, 6, 52);
    stencil(a, 80, 83, 5, "07", rgba(RL_STENCIL, 170));
    b.mark(78, 66, 10, 3);

    P top = b.cyl(44, 84, 30, 22, RL_CONC_TOP, RL_CONC_W);   // the drum: its crown at (44, 62)
    for (float x = 18; x < 72; x += 4) a.line(x, 66, x, 82, 0.6f, rgba(RL_CONC_D, 120));   // cooling fins down the drum
    a.rect(28, 78, 14, 3, 0.3f, RL_DARK, 0.5f, 0.1f);                          // vent slots at the foot
    a.line(29, 80.6f, 41, 80.6f, 0.8f, rgba(RL_AMBER, 130));
    a.glow(top.x, top.y, 18, rgba(RL_AMBER, 60));                              // the faint amber core glow
    for (int i = 0; i < 12; i++) {
        float an = i * 0.5236f;
        a.line(top.x + std::cos(an) * 6, top.y + std::sin(an) * 6, top.x + std::cos(an) * 27, top.y + std::sin(an) * 27, 0.7f, rgba(RL_CONC_D, 120));
    }
    a.ring(top.x, top.y, 16, 0.7f, rgba(RL_CONC_D, 120), 0.5f, 0.0f);
    a.circle(top.x, top.y, 6, RL_IRON, 1.0f, 0.3f);                            // crown hatch
    a.dot(top.x + 1, top.y + 1, 2.0f, rgba(RL_AMBER, 170));
    a.poly({{54, 50}, {60, 48}, {62, 54}, {57, 57}}, rgb(44, 40, 34), 0.8f, 0.2f);   // a cracked panel
    a.line(57, 52, 60, 51, 0.6f, rgba(RL_AMBER, 220));
    a.line(58, 46, 63, 54, 0.8f, rgba(RL_DARK, 220));
    moss(a, 30, 36, 8, 3, 61, 170);
    leafBlob(a, 60, 34, 8, 62);
    vineCurtain(a, 16, 40, 74, 92, 3, 63);
    vine(a, 30, 36, 36, 56, 1.8f, 2.0f, 64, 6);
}

// 160 x 128: a vehicle hall with a gantry crane on its roof, three big doors on the south wall and a tree and vines growing out of the gutters
void foundry(Bld& b) {
    Art& a = b.a;
    relicPad(b, 13);
    Roof hall = b.block(6, 20, 148, 84, 26, RL_CONC_TOP, RL_CONC_W, 3);   // roof -6..78, wall 78..104.6
    b.corrugated(hall, 9, RL_CONC_D, 90);
    b.skylights(hall, 3, RL_GLASS, RL_CONC_D);
    camo(a, 24, 52, 60, 68, 4, 71);
    camo(a, 96, 4, 132, 18, 3, 72);
    roofGrowth(b, hall, 5, 7, 73);
    smallTree(a, 96, 70, 8, 88);   // a second tree in the middle of the roof, rooted in the gutter
    b.cyl(40, 60, 4.5f, 6, RL_VERD, RL_VERD_D);   // roof ventilators
    b.cyl(112, 66, 4.5f, 6, RL_VERD, RL_VERD_D);

    // the gantry crane: rails along the roof, two portal legs standing on them, a truss bridge across the hall, a trolley with its hook and a cab
    a.rect(14, 34, 132, 3, 0.6f, RL_IRON_D, 1.0f, 0.4f);
    a.rect(14, 60, 132, 3, 0.6f, RL_IRON_D, 1.0f, 0.4f);
    for (float x : {24.0f, 132.0f}) {
        a.line(x + 1.5f, 37, x + 2.5f, -18, 0.8f, rgba(BLACK, 60));
        a.rect(x - 2.5f, -18, 5, 55, 0.5f, RL_IRON, 1.2f, 0.5f);
        a.rect(x - 4, 34, 8, 4, 0.5f, RL_IRON_D, 1.0f, 0.4f);
    }
    a.rect(21, -21, 117, 6, 0.6f, RL_IRON, 1.2f, 0.5f);
    for (float x = 21; x < 138; x += 6) {
        a.line(x, -21, x + 3, -15, 0.5f, RL_IRON_D);
        a.line(x + 3, -15, x + 6, -21, 0.5f, RL_IRON_D);
    }
    a.rect(70, -21, 14, 6, 0.5f, RL_VERD_D, 1.0f, 0.4f);   // trolley
    a.line(77, -15, 77, 20, 0.7f, rgba(RL_DARK, 230));     // hoist cable
    a.ring(77, 24, 3.0f, 1.2f, RL_IRON_L, 0.5f, 0.3f);     // hook
    a.rect(118, -14, 9, 8, 1.0f, RL_VERD, 1.0f, 0.5f);     // cab
    a.rect(120, -12, 5, 3, 0.4f, RL_DARK, 0.6f, 0.1f);
    dimLamp(a, 122.5f, -16.5f, 1.0f);

    bigDoor(b, 20, 104, 30, 20, "4");
    bigDoor(b, 65, 104, 30, 20, "5");
    bigDoor(b, 110, 104, 30, 20, "6");
    brokenWindow(a, 54, 84, 7, 8, 80);
    brokenWindow(a, 99, 84, 7, 8, 81);
    b.mark(140, 94, 8, 4);
    verdPlate(a, 144, 80, 6, 6, 86);
    rustBleed(a, 30, 84, 100, 2.4f);
    rustBleed(a, 130, 84, 100, 2.4f);
    vineCurtain(a, 20, 52, 78, 92, 3, 82);
    vineCurtain(a, 66, 94, 78, 92, 3, 83);
    vineCurtain(a, 110, 140, 78, 96, 3, 84);
    vine(a, 24, -12, 22, 34, 1.8f, 1.2f, 85, 7);   // climbing the west leg
    smallTree(a, 150, 4, 10, 85);                   // a tree out of the north-east roof corner
    litter(a, 10, 106, 150, 116, 18, 87);
}

// 160 x 128: a VTOL hangar on the north side and a round gunship pad to the south-east, faded markings and cracked concrete round a tower
void airstrip(Bld& b) {
    Art& a = b.a;
    relicPad(b, 91);
    const float px = 124, py = 98, pr = 34;
    a.shadowCircle(px, py, pr, 3, 3, 3, 80);
    a.circle(px, py, pr, RL_CONC_L, 2.4f, 0.35f);                 // the round pad
    a.ring(px, py, pr - 3.5f, 1.4f, rgba(RL_STENCIL, 150), 0.5f, 0.0f);
    a.ring(px, py, 19, 1.4f, rgba(RL_STENCIL, 130), 0.5f, 0.0f);
    for (int i = 0; i < 4; i++) {
        float an = i * 1.5708f + 0.785f;
        a.line(px + std::cos(an) * 20, py + std::sin(an) * 20, px + std::cos(an) * (pr - 4), py + std::sin(an) * (pr - 4), 1.0f, rgba(RL_STENCIL, 90));
    }
    a.line(px - 8, py - 9, px - 8, py + 9, 2.2f, rgba(RL_STENCIL, 150));   // a faded H
    a.line(px + 8, py - 9, px + 8, py + 9, 2.2f, rgba(RL_STENCIL, 150));
    a.line(px - 8, py, px + 8, py, 2.0f, rgba(RL_STENCIL, 150));
    for (int i = 0; i < 6; i++) {
        float an = i * 1.0472f + 0.3f;
        moss(a, px + std::cos(an) * (pr - 2), py + std::sin(an) * (pr - 2), 6, 2.4f, 92u + (u32)i, 170);
    }
    root(a, px - 30, py + 18, px - 20, py + 10, 2.2f, 93);
    smallTree(a, 148, 122, 7, 94);   // a bush on the pad's rim

    // the hangar: a barrel roof ribbed like its arches, a rolled-up door and a VTOL inside the dark bay
    Roof hg = b.block(6, 10, 88, 58, 24, RL_CONC_TOP, RL_CONC_W, 4);   // roof -14..44, wall 44..68.6
    for (float x = hg.x + 11; x < hg.x + hg.w - 4; x += 11) {
        a.line(x, hg.y + 1, x, hg.y + hg.h - 1, 0.8f, rgba(RL_CONC_D, 110));
        a.line(x + 0.9f, hg.y + 1, x + 0.9f, hg.y + hg.h - 1, 0.5f, rgba(RL_CONC_L, 120));
    }
    camo(a, 12, -8, 40, 8, 3, 95);
    roofGrowth(b, hg, 3, 7, 96);
    a.rect(22, 50, 52, 18.6f, 0.5f, RL_DARK, 0.8f, 0.1f);
    a.rect(22, 44.5f, 52, 6, 1.5f, RL_CONC_L, 1.0f, 0.5f);   // the rolled door
    for (float x = 24; x < 74; x += 3) a.line(x, 45.5f, x, 49.5f, 0.5f, rgba(RL_CONC_D, 150));
    a.cap(34, 62, 66, 60, 3.4f, RL_CAM_B, 1.6f, 0.5f);       // the VTOL's fuselage
    a.poly({{46, 60}, {58, 60}, {52, 66}, {42, 66}}, RL_CAM_A, 0.8f, 0.3f);
    a.poly({{60, 58}, {64, 51}, {68, 51}, {66, 59}}, RL_CAM_A, 0.8f, 0.3f);
    a.dot(36, 61, 1.8f, RL_IRON_D);
    stencil(a, 78, 52, 6, "92", rgba(RL_STENCIL, 170));
    dimLamp(a, 10, 47, 1.0f);
    dimLamp(a, 92, 47, 1.0f);
    b.mark(16, 16, 14, 3);
    vineCurtain(a, 8, 92, 46, 64, 6, 97);

    // the control tower to the west: a radar dish on its roof, cracked windows and a windsock on a pole
    b.block(8, 84, 26, 24, 14, RL_CONC_TOP, RL_CONC_W, 2);   // roof 70..94, wall 94..108.6
    b.dishTop(21, 80, 7, RL_CONC_D, RL_VERD);
    b.mark(12, 89, 10, 3);
    brokenWindow(a, 13, 99, 7, 7, 99);
    brokenWindow(a, 25, 99, 7, 7, 100);
    smallTree(a, 30, 72, 8, 101);
    vine(a, 31, 94, 33, 108, 1.6f, 1.4f, 102, 7);
    a.line(44, 121, 44, 87, 0.9f, rgba(BLACK, 40));
    a.cap(44, 120, 44, 86, 0.9f, RL_IRON_L, 0.6f, 0.3f);
    a.poly({{44, 86}, {58, 81}, {58, 90}}, RL_ORANGE, 0.6f, 0.3f);
    litter(a, 4, 116, 30, 124, 8, 103);
}

// 64 x 64: a flak battery: an octagonal pad of cracked concrete, a round armoured plinth with a verdigris turret ring and bolts, and rotten
// ammunition crates by it. The live launcher head is drawn over the centre.
void flakBase(Bld& b) {
    Art& a = b.a;
    a.shadowCircle(32, 36, 30, 2, 2, 3, 90);
    a.poly(ngonPts(32, 32, 30, 8, 0.3927f), RL_CONC, 2.2f, 0.45f);
    for (int i = 0; i < 6; i++) {
        float an = i * 1.0472f + 0.2f;
        moss(a, 32 + std::cos(an) * 29, 32 + std::sin(an) * 29, 5, 2.2f, 170u + (u32)i, 170);
    }
    root(a, 6, 46, 20, 56, 2.2f, 171);
    root(a, 44, 6, 56, 14, 2.0f, 172);
    b.crate(4, 4, 10, 9, rgb(120, 96, 66));
    b.crate(48, 50, 10, 9, rgb(112, 90, 62));
    P t = b.cyl(32, 44, 22, 12, RL_CONC_TOP, RL_CONC_W);   // the plinth: its top at (32, 32)
    a.ring(32, 32, 16.5f, 2.6f, RL_VERD, 1.0f, 0.5f);      // the verdigris turret ring
    for (int i = 0; i < 8; i++) {
        float an = i * 0.785f;
        a.dot(32 + std::cos(an) * 20, 32 + std::sin(an) * 20, 0.9f, RL_VERD_L);
    }
    verdPlate(a, 27, 38, 10, 4, 173);
    dimLamp(a, t.x + 14, t.y + 14, 1.0f);
    vine(a, 52, 34, 56, 48, 1.4f, 1.0f, 174, 5);
    leafBlob(a, 8, 56, 6, 175);
    leafBlob(a, 56, 10, 5, 176);
}

// 64 x 64: a laser tower: a square concrete pad, four cable pylons at its corners with dim lamps and cables sagging to a short pedestal with
// a verdigris turntable (the emitter head is drawn live over the centre).
void laserBase(Bld& b) {
    Art& a = b.a;
    relicPad(b, 181);
    const float pyl[4][2] = {{7, 9}, {57, 9}, {7, 55}, {57, 55}};
    for (auto& q : pyl) a.line(q[0], q[1], 32, 32, 0.6f, rgba(RL_DARK, 170));   // cables to the centre, under the head
    for (auto& q : pyl) {
        P t = b.cyl(q[0], q[1] + 6, 3.4f, 12, RL_CONC_TOP, RL_CONC_W);
        dimLamp(a, t.x, t.y, 1.2f);
    }
    P top = b.cyl(32, 44, 13, 12, RL_CONC_TOP, RL_CONC_W);   // the pedestal: its top at (32, 32)
    a.ring(32, 32, 10, 2.2f, RL_VERD, 1.0f, 0.5f);
    for (int i = 0; i < 6; i++) {
        float an = i * 1.0472f;
        a.dot(32 + std::cos(an) * 11, 32 + std::sin(an) * 11, 0.7f, RL_VERD_L);
    }
    verdPlate(a, 27, 40, 10, 4, 182);
    vine(a, top.x + 8, top.y + 2, top.x + 11, top.y + 12, 1.3f, 1.0f, 183, 4);
    moss(a, 12, 60, 6, 2.5f, 184, 170);
    leafBlob(a, 56, 20, 5, 185);
}

// 96 x 96: a heavy cannon emplacement: a round concrete platform with radial joints and a moss-grown rim, an armoured ring of bolted
// verdigris plates round a turntable, and a small ammunition bunker on the west rim with a stencilled door.
void cannonBase(Bld& b) {
    Art& a = b.a;
    a.shadowCircle(48, 52, 46, 2, 3, 4, 90);
    a.circle(48, 48, 46, RL_CONC, 3.0f, 0.4f);
    for (int i = 0; i < 16; i++) {
        float an = i * 0.3927f;
        a.line(48 + std::cos(an) * 12, 48 + std::sin(an) * 12, 48 + std::cos(an) * 43, 48 + std::sin(an) * 43, 0.7f, rgba(RL_SEAM, 110));
    }
    for (int i = 0; i < 8; i++) {
        float an = i * 0.785f + 0.4f;
        moss(a, 48 + std::cos(an) * 41, 48 + std::sin(an) * 41, 6, 3, 180u + (u32)i, 150);
    }
    Roof bk = b.block(14, 56, 22, 20, 8, RL_CONC_TOP, RL_CONC_W, 2);   // roof 48..68, wall 68..76.6
    stencil(a, 18, 70, 5, "3", rgba(RL_STENCIL, 170));
    rustBleed(a, 30, 68, 74, 2.0f);
    roofGrowth(b, bk, 1, 5, 186);
    a.ring(48, 48, 30, 5, RL_IRON_D, 1.5f, 0.6f);   // the armoured ring
    for (int i = 0; i < 12; i++) {
        float an = i * 0.5236f;
        a.dot(48 + std::cos(an) * 27.5f, 48 + std::sin(an) * 27.5f, 0.9f, RL_VERD_L);
    }
    verdPlate(a, 44, 14, 8, 5, 187);
    verdPlate(a, 44, 77, 8, 5, 188);
    verdPlate(a, 16, 44, 5, 8, 189);
    verdPlate(a, 75, 44, 5, 8, 190);
    a.circle(48, 48, 22, RL_CONC_TOP, 4.0f, 0.6f);   // the turntable
    a.ring(48, 48, 20, 1.0f, rgba(RL_CONC_D, 160), 0.5f, 0.0f);
    a.circle(48, 48, 14, RL_IRON, 2.0f, 0.5f);
    b.crate(72, 68, 10, 9, rgb(120, 96, 66));
    b.markDisc(48, 84, 3);
    vine(a, 80, 40, 84, 60, 1.6f, 1.6f, 191, 7);
    vine(a, 66, 74, 68, 86, 1.3f, 1.0f, 192, 4);
}

// 32 x 32: a thick wall segment, chained into lines: a coping of two cut blocks with joints at 0 and 16 so every tile repeats, a rust
// bleed, a few cracks and vines hanging down the face (kept clear of the tile edges so neighbours join cleanly).
void wallSeg(Bld& b) {
    Art& a = b.a;
    float W = (float)b.W, H = (float)b.H;
    a.shadowRect(0, 0, W, H, 3, 3, 3, 90);
    Roof r = b.block(0, 0, W, H, 10, RL_CONC_TOP, RL_CONC_W, 0.8f);   // roof -10..22, wall 22..32.6
    a.line(0, r.y + r.h * 0.5f, W, r.y + r.h * 0.5f, 0.8f, rgba(RL_CONC_D, 150));   // the coping's joint, running through every tile
    a.line(16, r.y + 1, 16, r.y + r.h * 0.5f, 0.8f, rgba(RL_CONC_D, 150));
    a.line(0, r.y + 1.2f, W, r.y + 1.2f, 0.8f, rgba(RL_CONC_L, 130));                  // lit top edge
    a.line(0.5f, r.y + 2, 0.5f, r.y + r.h - 2, 0.6f, rgba(RL_CONC_D, 110));
    a.line(31.5f, r.y + 2, 31.5f, r.y + r.h - 2, 0.6f, rgba(RL_CONC_D, 110));
    a.line(6, r.y + 3, 10, r.y + 3, 0.5f, rgba(RL_SEAM, 150));                          // a crack on the coping
    rustBleed(a, 26, 22, 30, 1.6f);
    moss(a, 6, r.y + r.h - 2.5f, 4, 1.8f, 301, 170);
    moss(a, 25, r.y + 3.5f, 3, 1.6f, 302, 160);
    vine(a, 9, 22, 11, 32, 1.5f, 1.0f, 303, 4);
    vine(a, 21, 22, 19, 32, 1.2f, 0.8f, 304, 3);
    litter(a, 2, 24, 30, 31, 4, 305);
    a.grain(0, 0, W, H, 0.12f, 306u);
}

// 96 x 96: a missile silo: a raised collar round a shaft with armoured sliding doors (the hatch frame, rails and two leaves), a warning band
// of faded ochre round its rim, a maintenance tower on the north side and a generator hut with its roof under moss.
void siloBase(Bld& b) {
    Art& a = b.a;
    relicPad(b, 14);
    const float cx = b.W * 0.5f, cy = b.H * 0.5f;   // (48, 48)
    // the maintenance tower north-east of the shaft: two legs, lattice bracing, a platform with a rail and a dim lamp, a ladder
    for (float x : {68.0f, 84.0f}) {
        a.line(x + 1, 22, x + 1, -8, 1.0f, rgba(BLACK, 60));
        a.cap(x, 22, x, -8, 0.9f, RL_IRON, 0.8f, 0.4f);
    }
    for (float y = -6; y < 20; y += 8) {
        a.line(68, y + 8, 84, y, 0.6f, RL_IRON_D);
        a.line(84, y + 8, 68, y, 0.6f, RL_IRON_D);
        a.line(68, y, 84, y, 0.6f, RL_IRON_L);
    }
    a.rect(64, -10, 24, 3, 0.4f, RL_IRON, 1.0f, 0.4f);
    a.line(64, -13, 88, -13, 0.6f, RL_IRON_L);
    for (float x = 64; x <= 88; x += 6) a.dot(x, -13, 0.5f, RL_IRON_L);
    a.line(91, 22, 91, -8, 0.8f, RL_IRON_L);
    for (float y = -6; y < 22; y += 4) a.line(89, y, 93, y, 0.5f, RL_IRON_D);
    dimLamp(a, 76, -14.5f, 1.0f);
    // the hut west of the shaft: its roof under moss and a camouflage net, a window, a stencilled number
    b.block(8, 64, 18, 22, 10, RL_CONC_TOP, RL_CONC_W, 2);   // roof 54..76, wall 76..86.6
    camo(a, 10, 56, 24, 74, 2, 111);
    moss(a, 14, 66, 6, 3, 113, 160);
    brokenWindow(a, 12, 79, 6, 5, 112);
    stencil(a, 20, 79.5f, 5, "3", rgba(RL_STENCIL, 170));
    b.mark(10, 58, 12, 3);
    vine(a, 25, 74, 26, 84, 1.2f, 0.6f, 114, 3);
    // the armoured collar: a raised cylinder, its rim a band of faded ochre and dark spokes
    b.cyl(cx, cy + 8, 34, 8, RL_CONC_TOP, RL_CONC_W);   // the collar's top at (48, 48)
    for (int i = 0; i < 24; i++) {
        float an = i * 0.2618f;
        a.line(cx + std::cos(an) * 26, cy + std::sin(an) * 26, cx + std::cos(an) * 32, cy + std::sin(an) * 32, 1.6f, i % 2 ? rgba(RL_DARK, 170) : rgba(RL_OCHRE, 200));
    }
    // the hatch: a verdigris frame, the top and bottom rails and the two closed leaves with stiffener ribs, a seam and bolts
    a.rect(cx - 23, cy - 23, 46, 46, 4, RL_VERD_D, 1.2f, 0.3f);
    a.rect(cx - 30, cy - 25, 60, 2.5f, 0.4f, RL_IRON_D, 1.0f, 0.4f);
    a.rect(cx - 30, cy + 22.5f, 60, 2.5f, 0.4f, RL_IRON_D, 1.0f, 0.4f);
    a.rect(cx - 20, cy - 20, 20, 40, 1.0f, RL_IRON, 1.2f, 0.5f);
    a.rect(cx, cy - 20, 20, 40, 1.0f, RL_IRON, 1.2f, 0.5f);
    for (float yy = cy - 14; yy < cy + 20; yy += 6) {
        a.line(cx - 18, yy, cx - 2, yy, 0.5f, rgba(RL_DARK, 120));
        a.line(cx + 2, yy, cx + 18, yy, 0.5f, rgba(RL_DARK, 120));
    }
    a.line(cx, cy - 20, cx, cy + 20, 0.7f, rgba(RL_DARK, 200));
    for (float x : {cx - 17, cx + 17}) for (float y : {cy - 17, cy + 17}) a.dot(x, y, 0.9f, RL_VERD_L);
    verdPlate(a, 66, 50, 7, 5, 115);
    stencil(a, 40, 50, 5, "31", rgba(RL_STENCIL, 170));
    vine(a, 36, 52, 38, 64, 1.5f, 1.0f, 121, 5);
    vine(a, 60, 53, 62, 63, 1.4f, 1.0f, 122, 4);
    vine(a, 84, -4, 82, 20, 1.2f, 1.2f, 123, 5);   // climbing the tower's leg
    root(a, 4, 46, 16, 60, 2.2f, 126);
    root(a, 80, 80, 92, 70, 2.0f, 127);
    leafBlob(a, 16, 30, 9, 125);
    b.crate(70, 74, 9, 8, rgb(120, 96, 66));
}

// 96 x 96: a secret bunker: a concrete dome sunk into a mound of earth, its south wall pierced by gun slits and a steel hatch, camouflage
// netting over the north-west half, a canopy of leaves over the dome and a radio mast through the canopy.
void bunkerBase(Bld& b) {
    Art& a = b.a;
    float W = (float)b.W, H = (float)b.H;
    a.shadowRect(0, 0, W, H, 3, 3, 4, 85);
    a.rect(0, 0, W, H, 6, RL_EARTH, 2.6f, 0.5f);
    litter(a, 4, 4, 92, 92, 30, 151);
    moss(a, 14, 14, 10, 5, 152, 150);
    moss(a, 84, 84, 8, 4, 153, 150);
    a.ell(48, 58, 42, 36, rgba(rgb(104, 92, 68), 150), 6.0f, 0.4f);   // the earth heaped over the dome
    P top = b.cyl(48, 66, 34, 10, RL_CONC_TOP, RL_CONC_D);            // the dome: its crown at (48, 56)
    for (int i = 0; i < 8; i++) {
        float an = i * 0.785f + 0.2f;
        a.line(top.x + std::cos(an) * 8, top.y + std::sin(an) * 8, top.x + std::cos(an) * 31, top.y + std::sin(an) * 31, 0.7f, rgba(RL_CONC_D, 110));
    }
    a.ring(top.x, top.y, 22, 0.7f, rgba(RL_CONC_D, 90), 0.5f, 0.0f);
    for (float x : {24.0f, 40.0f, 54.0f}) {   // gun slits on the south wall
        a.rect(x, 60.5f, 6, 2.4f, 0.3f, RL_DARK, 0.5f, 0.1f);
        a.line(x + 0.5f, 63.6f, x + 5.5f, 63.6f, 0.6f, rgba(RL_CONC_L, 150));
    }
    a.rect(60, 58, 14, 8, 0.8f, RL_VERD_D, 1.0f, 0.4f);   // steel hatch
    a.rect(61, 59, 12, 6, 0.4f, RL_IRON_D, 1.0f, 0.4f);
    stencil(a, 64, 60, 4, "9", rgba(RL_STENCIL, 170));
    // the canopy's upper layer over the crown, then the camouflage net draped down the west flank (a mottled sheet with a frayed hem),
    // then the canopy's lower clumps and the mast poking up through it
    leafBlob(a, 30, 36, 18, 161);
    leafBlob(a, 66, 26, 15, 162);
    leafBlob(a, 48, 14, 12, 163);
    std::vector<P> net = {{10, 66}, {12, 46}, {26, 40}, {42, 44}, {52, 56}, {44, 68}, {26, 70}};
    a.poly(net, rgba(RL_CAM_A, 225), 1.0f, 0.3f);
    for (int i = 0; i < 9; i++) a.ell(16 + (i % 3) * 12.0f, 46 + (i / 3) * 8.0f, 4.0f, 2.5f, rgba(i & 1 ? RL_CAM_B : RL_CAM_C, 170), 1.0f, 0.2f);
    for (size_t i = 0; i < net.size(); i += 2) {
        P p = net[i], q = net[(i + 3) % net.size()];
        a.line(p.x, p.y, q.x, q.y, 0.5f, rgba(RL_DARK, 90));
    }
    leafBlob(a, 16, 62, 10, 164);
    leafBlob(a, 80, 52, 9, 165);
    b.antenna(70, 50, 30, RL_AMBER);
    vine(a, 14, 62, 18, 80, 1.6f, 1.5f, 166, 6);
    vine(a, 84, 58, 86, 74, 1.5f, 1.2f, 167, 5);
    b.mark(44, 84, 14, 3);
}

// ---------------------------------------------------------------- the bridge

// the river: translucent water across the middle rows of the sprite, faded out at the sides and along the banks so it blends with the map's
// own water, with a few ripples
void bridgeWater(Canvas& c, Art& A) {
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = (x + 0.5f) / BART_K - 8.0f, dy = (y + 0.5f) / BART_K - 8.0f;   // design px
        float fx = std::min(clampf((dx + 8.0f) / 8.0f, 0, 1), clampf((104.0f - dx) / 8.0f, 0, 1));
        float fy = std::min(clampf((dy - 96.0f) / 10.0f, 0, 1), clampf((288.0f - dy) / 10.0f, 0, 1));
        float f = fx * fy;
        if (f <= 0) continue;
        Color wc = shade(RL_WATER, 0.92f + 0.14f * (float)((x / 3 * 7 + y / 5 * 13) % 11) / 10.0f);
        wc.a = (u8)(214 * f);
        put(c, x, y, wc);
    }
    Rng r(7741u);
    for (int i = 0; i < 40; i++) {
        float x = r.f(-4, 86), y = r.f(104, 280), l = r.f(6, 14);
        A.line(x, y, x + l, y, 0.7f, rgba(rgb(150, 204, 210), 70));
    }
}

// a pier standing in the water beside the deck: a foam halo, its shadow, a concrete column and cap, weed on the foot
void pier(Art& A, float px, float py, u32 seed) {
    A.ell(px, py + 1.0f, 8.0f, 10.0f, rgba(RL_FOAM, 120), 1.0f, 0.0f);
    A.shadowRect(px - 4, py - 7, 8, 16, 3, 4, 3, 100);
    A.rect(px - 4, py - 7, 8, 16, 1.5f, RL_CONC, 1.4f, 0.5f);
    A.rect(px - 5.5f, py - 9, 11, 4, 0.8f, RL_CONC_D, 1.2f, 0.5f);
    moss(A, px, py + 6, 3.0f, 2.2f, seed, 150);
}

// one stretch of deck (y0..y1): the concrete surface, expansion joints every 32 px, road edges, a dashed centre line and leaf litter drifts
void deckSeg(Art& A, float y0, float y1) {
    A.rect(0, y0, 96, y1 - y0, 1.0f, RL_DECK, 1.6f, 0.35f);
    for (float y = 32; y < 384; y += 32) if (y > y0 + 1.5f && y < y1 - 1.5f) A.line(4, y, 92, y, 1.0f, rgba(RL_SEAM, 150));
    A.line(15, y0 + 1, 15, y1 - 1, 0.8f, rgba(RL_STENCIL, 110));
    A.line(81, y0 + 1, 81, y1 - 1, 0.8f, rgba(RL_STENCIL, 110));
    for (float y = y0 + 4; y < y1 - 6; y += 14) A.line(48, y, 48, y + 6, 0.9f, rgba(RL_STENCIL, 120));
    Rng g(9100u + (u32)y0);
    for (float y = y0 + 6; y < y1 - 4; y += 32) {   // weeds in the joints and grime streaks down the wheel tracks
        float wx = g.f(10, 86), gx = g.f(20, 76), gy = g.f(4, 12), gw = g.f(8, 14), gh = g.f(3, 6);
        if (g.f() < 0.7f) A.dot(wx, y - 1.0f, 1.2f, rgba(RL_MOSS, 200));
        A.ell(gx, y + gy, gw, gh, rgba(rgb(60, 54, 40), 60), 1.0f, 0.2f);
    }
    for (int i = 0; i < 4; i++) {
        float mx = g.f(6, 90), my = g.f(y0 + 8, y1 - 8), mrx = g.f(3, 6), mry = g.f(2, 3);
        moss(A, mx, my, mrx, mry, 9200u + (u32)(y0 + i * 13.0f), 150);
    }
    litter(A, 17, y0 + 2, 79, y1 - 2, 12, 9100u + (u32)y0);
    A.grain(0, y0, 96, y1, 0.1f, 9001u + (u32)y0);
}

// the steel truss on both edges of a stretch, read as steel from above: heavier chords with a lit edge, a darker diagonal lattice and a
// verdigris bolt at each node
void truss(Art& A, float y0, float y1) {
    for (int side = 0; side < 2; side++) {
        float x0 = side ? 90.0f : 0.0f;
        A.rect(x0, y0, 6, y1 - y0, 0.3f, RL_IRON_D, 0.6f, 0.2f);
        A.line(x0 + 1.4f, y0, x0 + 1.4f, y1, 1.6f, RL_IRON);
        A.line(x0 + 4.6f, y0, x0 + 4.6f, y1, 1.6f, RL_IRON);
        A.line(x0 + 0.8f, y0, x0 + 0.8f, y1, 0.5f, rgba(RL_IRON_L, 170));
        for (float y = y0 + 2; y < y1 - 1; y += 16) {
            A.line(x0 + 1.4f, y, x0 + 4.6f, y + 16, 0.7f, RL_IRON_D);
            A.line(x0 + 4.6f, y, x0 + 1.4f, y + 16, 0.7f, RL_IRON_D);
            A.dot(x0 + 3.0f, y, 0.8f, RL_VERD);
        }
    }
}

// the railings on both walkways, with posts every 12 px
void rails(Art& A, float y0, float y1) {
    for (int side = 0; side < 2; side++) {
        float x = side ? 88.0f : 8.0f;
        A.line(x, y0, x, y1, 0.9f, RL_IRON_L);
        for (float y = y0 + 2; y < y1; y += 12) A.dot(x, y, 0.7f, RL_IRON);
    }
}
// a lamp post on a railing: a foot, a shadow and a dim amber lamp
void lampPost(Art& A, float x, float y) {
    A.line(x + 0.5f, y + 0.5f, x + 2.5f, y + 3.0f, 0.8f, rgba(BLACK, 70));
    A.dot(x, y, 0.8f, RL_IRON_D);
    dimLamp(A, x, y - 1.5f, 1.0f);
}

// the broken end of a span at y: a jagged cut, the slab's underside hanging towards dir (+1 south into the gap, -1 north), the bent steel of the
// truss sticking out of it and the rebar left in the concrete
void brokenEnd(Art& A, float y, float dir, u32 seed) {
    Rng r(seed);
    std::vector<P> top;
    for (int i = 0; i <= 8; i++) top.push_back({i * 12.0f, y + dir * r.f(-1.0f, 4.0f)});
    std::vector<P> poly = top;
    for (int i = 8; i >= 0; i--) poly.push_back({i * 12.0f, top[(size_t)i].y + dir * r.f(10.0f, 14.0f)});
    A.poly(poly, RL_CONC_D, 1.0f, 0.4f);
    for (size_t i = 0; i + 1 < top.size(); i++) A.line(top[i].x, top[i].y, top[i + 1].x, top[i + 1].y, 1.0f, rgba(RL_CONC_L, 200));
    for (float x : {2.0f, 94.0f}) {   // the truss chords, bent over the gap: a kink, then a fall away from the cut
        A.line(x, y, x + 3, y + dir * 8, 1.8f, RL_IRON);
        A.line(x + 3, y + dir * 8, x - 3, y + dir * 17, 1.8f, RL_IRON);
        A.line(x + 3, y + dir * 8, x + 1, y + dir * 16, 0.8f, RL_IRON_L);
    }
    for (float xx : {20.0f, 44.0f, 70.0f}) A.line(xx, y + dir * 4, xx + (xx < 48 ? -4.0f : 4.0f), y + dir * 14, 0.8f, RL_IRON_D);
}

// temporary works bridging the gap: two steel girders running north-south over it, planks laid across them, scaffold guard rails with posts
// and work lights on stands at the edges
void repairDeck(Art& A) {
    for (float x : {10.0f, 82.0f}) A.rect(x - 1.5f, 160, 3, 64, 0.4f, RL_IRON_L, 0.8f, 0.4f);
    for (int i = 0; i < 12; i++) {
        float y = 168 + i * 4.0f;
        A.rect(6, y, 84, 3, 0.2f, (i & 1) ? rgb(150, 112, 72) : rgb(128, 96, 60), 0.6f, 0.3f);
    }
    A.line(4, 166, 4, 218, 0.8f, RL_IRON_L);
    A.line(92, 166, 92, 218, 0.8f, RL_IRON_L);
    A.line(4, 166, 92, 166, 0.8f, RL_IRON_L);
    A.line(4, 218, 92, 218, 0.8f, RL_IRON_L);
    for (float y = 170; y < 218; y += 8) { A.dot(4, y, 0.7f, RL_IRON); A.dot(92, y, 0.7f, RL_IRON); }
    for (float y : {178.0f, 204.0f}) {
        A.line(-2, y, 0, y + 5, 0.8f, RL_IRON_D);
        A.dot(0, y + 5, 1.3f, rgb(255, 242, 200));
        A.glow(0, y + 5, 10, rgba(rgb(255, 236, 176), 110));
    }
}

}  // namespace

// ---------------------------------------------------------------- entry points

bool artRelicBuilding(Canvas& c, Canvas& mask, int kind, int w, int h) {
    if (kind < 0 || kind > 9) return false;
    Bld b(c, mask, w * TILE, h * TILE, false, 6100u + (u32)kind * 37u);
    switch (kind) {
    case 0: cmdPost(b);      break;
    case 1: reactor(b);      break;
    case 2: foundry(b);      break;
    case 3: airstrip(b);     break;
    case 4: flakBase(b);     break;
    case 5: laserBase(b);    break;
    case 6: cannonBase(b);   break;
    case 7: wallSeg(b);      break;
    case 8: siloBase(b);     break;
    case 9: bunkerBase(b);   break;
    }
    c.outline(rgb(8, 10, 14, 150));
    return true;
}

// The silo's armed overlay: the shaft opened (an opaque well over the closed leaves), the two leaves slid back over the collar, the MIRV
// nose and its three warhead bumps standing in the shaft. Transparent everywhere else.
void artRelicSiloArmed(Canvas& c, int w, int h) {
    Canvas dummy(4, 4);
    Bld b(c, dummy, w * TILE, h * TILE, false, 6108u);
    Art& a = b.a;
    const float cx = b.W * 0.5f, cy = b.H * 0.5f;
    a.rect(cx - 20, cy - 20, 40, 40, 1.5f, rgb(30, 32, 30), 1.0f, 0.3f);   // the shaft, opaque
    a.rect(cx - 17, cy - 17, 34, 34, 1.0f, rgb(46, 48, 44), 1.0f, 0.2f);
    // the MIRV: body, nose cone, a faded band and three warhead bumps at the base of the nose
    a.cap(cx, cy + 14, cx, cy - 6, 6.0f, rgb(204, 200, 186), 3.0f, 0.5f);
    a.rect(cx - 6, cy - 7, 12, 3, 0.3f, RL_RUST, 0.8f, 0.4f);
    a.poly({{cx - 6, cy - 5}, {cx + 6, cy - 5}, {cx, cy - 19}}, rgb(222, 218, 204), 2.0f, 0.5f);
    a.dot(cx, cy - 19.4f, 0.9f, rgb(150, 140, 110));
    for (float x : {cx - 4.0f, cx + 4.0f, cx}) {
        float y = x == cx ? cy - 1 : cy - 3;
        a.circle(x, y + 8, 2.6f, rgb(176, 170, 152), 1.4f, 0.5f);
    }
    // the armoured leaves slid back: two ribbed doors over the collar, a lamp on each to show it is ready
    a.rect(cx - 40, cy - 20, 20, 40, 1.0f, RL_IRON, 1.2f, 0.5f);
    a.rect(cx + 20, cy - 20, 20, 40, 1.0f, RL_IRON, 1.2f, 0.5f);
    for (float yy = cy - 14; yy < cy + 20; yy += 6) {
        a.line(cx - 38, yy, cx - 22, yy, 0.5f, rgba(RL_DARK, 120));
        a.line(cx + 22, yy, cx + 38, yy, 0.5f, rgba(RL_DARK, 120));
    }
    dimLamp(a, cx - 30, cy - 23, 1.0f);
    dimLamp(a, cx + 30, cy - 23, 1.0f);
}

// The rotating heads, drawn over the bases at the footprint centre
void artRelicTurretHead(Canvas& c, int which) {
    Art A(c, rgb(255, 255, 255));
    switch (which) {
    case 0:   // flak launcher: a box of eight missile tubes (two per row, four rows) on a verdigris turntable
        A.circle(30, 32, 11, RL_CONC_D, 2.0f, 0.5f);
        A.ring(30, 32, 9, 0.8f, RL_VERD, 1.0f, 0.4f);
        A.box(30, 32, 10, 13, 2.2f, RL_CAM_A, 2.2f, 0.6f);
        for (int r = 0; r < 4; r++) for (int s = -1; s <= 1; s += 2) {
            float y = 26.8f + r * 3.4f + s * 0.95f;
            A.cap(40, y, 56, y, 0.9f, RL_IRON_D, 0.6f, 0.4f);
            A.dot(56, y, 0.6f, RL_DARK);
        }
        break;
    case 1:   // laser emitter: a prism lens on a verdigris yoke over a turntable, its beam dim and amber
        A.circle(30, 32, 10, RL_CONC_D, 2.0f, 0.5f);
        A.ring(30, 32, 8, 0.8f, RL_VERD, 1.0f, 0.4f);
        A.cap(30, 32, 40, 21, 1.6f, RL_VERD_D, 1.0f, 0.4f);
        A.cap(30, 32, 40, 43, 1.6f, RL_VERD_D, 1.0f, 0.4f);
        A.cap(40, 20, 40, 44, 2.2f, RL_VERD, 1.4f, 0.6f);   // the yoke bar
        A.dot(40, 32, 2.4f, RL_VERD_L);
        A.poly({{44, 26}, {44, 38}, {58, 32}}, rgb(168, 204, 184), 1.2f, 0.5f);   // the prism
        A.poly({{46, 29}, {46, 35}, {54, 32}}, rgba(RL_AMBER, 230), 0.6f, 0.0f);  // its core
        A.glow(56, 32, 6, rgba(RL_AMBER, 110));
        A.line(58, 32, 64, 32, 1.2f, rgba(RL_AMBER, 150));
        break;
    case 2:   // heavy cannon: a long barrel with a muzzle brake on a cradle, a recoil cylinder above it
        A.circle(28, 32, 10, RL_CONC, 2.6f, 0.6f);
        A.ring(28, 32, 8, 0.8f, RL_VERD, 1.0f, 0.4f);
        A.box(26, 32, 7, 9, 1.8f, RL_CAM_B, 2.0f, 0.6f);
        A.cap(30, 25, 56, 25, 2.4f, RL_CONC_D, 1.4f, 0.5f);
        A.cap(30, 32, 58, 32, 3.0f, RL_IRON, 1.8f, 0.6f);
        A.box(59, 32, 3.2f, 6.5f, 0.8f, RL_IRON_D, 1.4f, 0.5f);
        A.dot(62, 32, 1.4f, RL_DARK);
        A.dot(38, 28, 0.7f, RL_VERD_L);
        break;
    }
    c.outline(rgb(8, 10, 14, 170));
}

// The river bridge: abutments, piers in the water, the deck with its truss, railings, lamps and overgrowth, and in the broken and repair
// states the gap over the river (y 168..216) with its broken ends, debris and scorch, or the scaffolding laid across it.
void artBridge(Canvas& c, int state) {
    Art A(c, rgb(255, 255, 255), (float)BART_K, 8.0f, 8.0f);   // design px: the 96 x 384 deck with 8 px of margin round it
    bool broken = state != 0, repair = state == 2;
    const float g0 = 168.0f, g1 = 216.0f;
    bridgeWater(c, A);

    // the banks: rocks at the river's edge, shrubs on the land, the concrete abutments with a stencilled number on the north one
    A.ngon(-4, 95, 3.4f, 7, 0.5f, RL_CONC_D, 1.2f, 0.7f);
    A.ngon(100, 98, 3.2f, 7, 1.2f, RL_CONC, 1.2f, 0.7f);
    A.ngon(-3, 290, 3.4f, 7, 2.0f, RL_CONC_D, 1.2f, 0.7f);
    A.ngon(99, 292, 3.2f, 7, 0.2f, RL_CONC, 1.2f, 0.7f);
    leafBlob(A, -4, 30, 6, 230);
    leafBlob(A, 100, 24, 7, 231);
    leafBlob(A, -3, 352, 6, 232);
    leafBlob(A, 101, 364, 7, 233);
    A.rect(-6, -8, 108, 9, 1.0f, RL_CONC, 1.4f, 0.5f);
    for (float x = -4; x < 104; x += 16) A.line(x, -8, x, 1, 0.5f, rgba(BLACK, 60));
    stencil(A, 40, -6, 5, "12", rgba(RL_STENCIL, 170));
    moss(A, 90, -5, 6, 2.5f, 234, 160);
    A.rect(-6, 383, 108, 9, 1.0f, RL_CONC, 1.4f, 0.5f);
    for (float x = -4; x < 104; x += 16) A.line(x, 384, x, 391, 0.5f, rgba(BLACK, 60));

    // the piers stand in the water at the sides of the deck, their caps under the truss
    pier(A, -2, 118, 701);
    pier(A, 98, 118, 702);
    pier(A, -2, 196, 703);
    pier(A, 98, 196, 704);
    pier(A, -2, 262, 705);
    pier(A, 98, 262, 706);

    // the deck's shadow falls south-east onto the water
    A.shadowRect(0, 0, 96, 384, 4, 5, 5, 100);
    if (!broken) {
        deckSeg(A, 0, 384);
        truss(A, 0, 384);
    } else {
        deckSeg(A, 0, g0);
        deckSeg(A, g1, 384);
        truss(A, 0, g0);
        truss(A, g1, 384);
    }
    rails(A, 0, broken ? g0 : 384);
    if (broken) rails(A, g1, 384);
    for (float y : {44.0f, 140.0f, 236.0f, 332.0f}) lampPost(A, 8, y);
    for (float y : {92.0f, 188.0f, 284.0f, 366.0f}) if (!broken || y < g0 || y > g1) lampPost(A, 88, y);

    if (broken && !repair) {
        // the middle span blown out: the two ends with their dangling undersides, twisted girders and rebar, the scorch and the broken concrete in the water
        brokenEnd(A, g0, 1.0f, 711);
        brokenEnd(A, g1, -1.0f, 712);
        A.ell(48, g0 - 2, 46, 6, rgba(rgb(24, 20, 18), 140), 1.0f, 0.2f);
        A.ell(48, g1 + 2, 46, 6, rgba(rgb(24, 20, 18), 140), 1.0f, 0.2f);
        A.ell(30, 152, 9, 4, rgba(rgb(24, 20, 18), 120), 1.0f, 0.2f);
        A.ell(68, 232, 9, 4, rgba(rgb(24, 20, 18), 120), 1.0f, 0.2f);
        for (int i = 0; i < 7; i++) {
            float x = 14 + i * 11.0f + (i & 1) * 4.0f, y = 184.0f + (float)(i * 37 % 25);
            A.poly({{x, y}, {x + 6, y - 1}, {x + 7, y + 4}, {x + 1, y + 5}}, RL_CONC_D, 0.8f, 0.4f);
            A.ell(x + 3, y + 5, 6, 2.5f, rgba(RL_FOAM, 70), 1.0f, 0.0f);
        }
    }
    if (repair) {
        brokenEnd(A, g0, 1.0f, 721);
        brokenEnd(A, g1, -1.0f, 722);
        repairDeck(A);
    }

    // the overgrowth: vines down the truss, moss in the joints, leaves on the edges
    leafBlob(A, 2, 70, 5, 241);   // shrubs hanging over the truss edges
    leafBlob(A, 94, 150, 5, 242);
    leafBlob(A, 2, 300, 5, 243);
    leafBlob(A, 94, 330, 5, 244);
    vineCurtain(A, 0, 6, 36, 150, 4, 221);
    vineCurtain(A, 90, 96, 232, 352, 4, 222);
    vineCurtain(A, 0, 6, 232, 300, 2, 223);
    moss(A, 12, 86, 4, 2.0f, 225, 160);
    moss(A, 84, 306, 4, 2.0f, 226, 160);
    moss(A, 14, 352, 4, 2.0f, 227, 150);
    A.grain(-8, -8, 104, 392, 0.08f, 7751u);
}
