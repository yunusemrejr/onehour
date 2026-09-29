#include "art.h"

// Everything is authored in 64x64 canvas pixels, facing +x. Shapes are signed distance fields: each pixel gets
// anti-aliased coverage and a bevel lit from the north-west, which is what gives the hulls their metal look.
namespace {

// Palette in use for the unit being drawn. Every unit type has its own tone (setTone), so a faction's army is a family of
// related materials rather than one repainted hull: Cyber is graphite, cobalt, ceramic and teal lit by an emitter colour;
// Clanker is olive, sand, brick and iron with rust, hazard yellow and steel trim.
Color CYH, CYP, CYD, CYG, CYW, CYM;
Color CKH, CKP, CKD, CKR, CKY, CKS, CKT;
const Color CYH0 = rgb(66, 86, 120), CYP0 = rgb(108, 140, 182), CYD0 = rgb(24, 32, 48), CYG0 = rgb(110, 232, 255), CYW0 = rgb(216, 238, 248), CYM0 = rgb(42, 54, 78);
const Color CKH0 = rgb(94, 104, 64), CKP0 = rgb(142, 134, 98), CKD0 = rgb(38, 40, 32), CKR0 = rgb(156, 90, 48), CKY0 = rgb(226, 182, 62), CKS0 = rgb(124, 126, 124), CKT0 = rgb(160, 144, 102);

struct CyTone { Color h, p, m, d, g; };
struct CkTone { Color h, p, d, r, y, s, t; };
const CyTone CY_TONES[11] = {
    { rgb(104, 114, 128), rgb(152, 164, 178), rgb(64, 72, 86),  rgb(30, 34, 42), rgb(255, 190, 92)  },   // Fabricator: brushed titanium, amber work lights
    { rgb(34, 104, 112),  rgb(82, 154, 160),  rgb(26, 62, 70),  rgb(16, 38, 44), rgb(120, 255, 214) },   // E-Hauler: deep teal
    { rgb(70, 92, 130),   rgb(112, 146, 192), rgb(44, 56, 82),  rgb(24, 32, 48), rgb(110, 232, 255) },   // Trooper: slate blue
    { rgb(150, 168, 190), rgb(200, 216, 232), rgb(88, 104, 126),rgb(40, 50, 66), rgb(96, 226, 255)  },   // Laser Trooper: pale ceramic
    { rgb(52, 56, 74),    rgb(92, 98, 124),   rgb(32, 34, 50),  rgb(16, 18, 28), rgb(206, 232, 255) },   // Shock Trooper: charcoal, white arcs
    { rgb(38, 82, 156),   rgb(88, 134, 208),  rgb(26, 50, 98),  rgb(14, 26, 54), rgb(110, 232, 255) },   // Photon Tank: cobalt
    { rgb(28, 76, 80),    rgb(60, 126, 126),  rgb(18, 44, 50),  rgb(10, 26, 30), rgb(140, 255, 202) },   // Volt Walker: dark teal
    { rgb(46, 50, 64),    rgb(90, 96, 114),   rgb(30, 32, 44),  rgb(14, 16, 22), rgb(232, 250, 255) },   // Railgun Tank: graphite
    { rgb(32, 38, 54),    rgb(68, 76, 98),    rgb(22, 26, 40),  rgb(10, 12, 20), rgb(110, 232, 255) },   // Wraith Drone: matte stealth
    { rgb(178, 192, 206), rgb(224, 234, 244), rgb(102, 116, 134),rgb(44, 52, 66), rgb(150, 255, 255) },  // Ion Lancer: polar white
    { rgb(82, 90, 106),   rgb(134, 144, 160), rgb(52, 58, 74),  rgb(22, 26, 36), rgb(255, 214, 124) },   // Aegis Titan: titanium, gold emitters
};
const CkTone CK_TONES[11] = {
    { rgb(96, 104, 68),  rgb(142, 134, 98),  rgb(38, 40, 32), rgb(156, 90, 48),  rgb(232, 186, 58),  rgb(124, 126, 124), rgb(160, 144, 102) },   // Dozer: yellow machine
    { rgb(112, 88, 60),  rgb(158, 130, 92),  rgb(40, 34, 28), rgb(164, 84, 44),  rgb(226, 182, 62),  rgb(124, 122, 116), rgb(168, 140, 96)  },   // Supply Truck: earth brown
    { rgb(92, 106, 62),  rgb(138, 142, 96),  rgb(36, 40, 30), rgb(150, 92, 50),  rgb(226, 182, 62),  rgb(124, 126, 124), rgb(150, 138, 96)  },   // Rifleman: olive drab
    { rgb(162, 142, 96), rgb(196, 176, 128), rgb(50, 44, 34), rgb(170, 84, 44),  rgb(234, 190, 70),  rgb(128, 126, 120), rgb(190, 166, 116) },   // RPG Trooper: desert khaki
    { rgb(58, 68, 54),   rgb(98, 108, 90),   rgb(26, 30, 26), rgb(140, 84, 48),  rgb(220, 178, 62),  rgb(112, 116, 112), rgb(120, 122, 96)  },   // Gunner: charcoal green
    { rgb(82, 98, 54),   rgb(126, 138, 86),  rgb(34, 40, 28), rgb(154, 90, 46),  rgb(226, 182, 62),  rgb(122, 126, 120), rgb(152, 142, 96)  },   // Brute Tank: heavy olive
    { rgb(172, 150, 98), rgb(204, 184, 132), rgb(54, 46, 34), rgb(168, 88, 46),  rgb(236, 192, 72),  rgb(128, 126, 118), rgb(196, 172, 118) },   // Gatling Tank: sand
    { rgb(120, 78, 54),  rgb(166, 118, 88),  rgb(44, 32, 26), rgb(174, 96, 50),  rgb(230, 184, 60),  rgb(128, 120, 112), rgb(176, 138, 100) },   // Rocket Launcher: brick
    { rgb(74, 88, 96),   rgb(116, 132, 140), rgb(28, 34, 38), rgb(150, 92, 52),  rgb(226, 182, 62),  rgb(132, 136, 138), rgb(150, 150, 130) },   // Vulture: slate blue-grey
    { rgb(108, 62, 56),  rgb(150, 100, 90),  rgb(40, 26, 24), rgb(180, 96, 50),  rgb(236, 190, 66),  rgb(126, 118, 112), rgb(170, 128, 104) },   // Grenadier: maroon
    { rgb(74, 76, 72),   rgb(118, 118, 108), rgb(30, 30, 28), rgb(162, 92, 48),  rgb(238, 190, 60),  rgb(140, 142, 140), rgb(150, 146, 120) },   // Behemoth: cast iron, hazard trim
};

// type < 0 selects the neutral default palette (structures' turret heads)
void setTone(int type) {
    CYH = CYH0; CYP = CYP0; CYD = CYD0; CYG = CYG0; CYW = CYW0; CYM = CYM0;
    CKH = CKH0; CKP = CKP0; CKD = CKD0; CKR = CKR0; CKY = CKY0; CKS = CKS0; CKT = CKT0;
    if (type < 0) return;
    int local = type - firstUnitOf(UNITS[type].faction);
    if (local < 0 || local > 10) return;
    if (UNITS[type].faction == F_CYBER) {
        const CyTone& t = CY_TONES[local];
        CYH = t.h; CYP = t.p; CYM = t.m; CYD = t.d; CYG = t.g;
        CYW = mix(t.g, rgb(255, 255, 255), 0.72f);
    } else {
        const CkTone& t = CK_TONES[local];
        CKH = t.h; CKP = t.p; CKD = t.d; CKR = t.r; CKY = t.y; CKS = t.s; CKT = t.t;
    }
}

struct P { float x, y; };
inline float hyp(float x, float y) { return std::sqrt(x * x + y * y); }

float sdBox(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = std::abs(px - cx) - (hw - r), qy = std::abs(py - cy) - (hh - r);
    return hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}
float sdCap(float px, float py, float ax, float ay, float bx, float by, float r) {
    float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    float h = clampf((pax * bax + pay * bay) / std::max(1e-6f, bax * bax + bay * bay), 0, 1);
    return hyp(pax - bax * h, pay - bay * h) - r;
}
float sdPoly(const std::vector<P>& v, float px, float py, float sgn) {
    float d = -1e9f; size_t n = v.size();
    for (size_t i = 0; i < n; i++) {
        const P& a = v[i]; const P& b = v[(i + 1) % n];
        float ex = b.x - a.x, ey = b.y - a.y, l = hyp(ex, ey);
        float nx = sgn * ey / l, ny = -sgn * ex / l;
        d = std::max(d, nx * (px - a.x) + ny * (py - a.y));
    }
    return d;
}

// straight-alpha "over" so anti-aliased edges keep the right colour on a transparent canvas
void put(Canvas& c, int x, int y, Color s) {
    if (x < 0 || y < 0 || x >= c.w || y >= c.h || s.a == 0) return;
    u32& dst = c.px[y * c.w + x];
    Color d = Canvas::unpack(dst);
    if (d.a == 0 || s.a == 255) { dst = Canvas::pack(s); return; }
    float sa = s.a / 255.0f, da = d.a / 255.0f, oa = sa + da * (1 - sa);
    Color o{(u8)((s.r * sa + d.r * da * (1 - sa)) / oa), (u8)((s.g * sa + d.g * da * (1 - sa)) / oa), (u8)((s.b * sa + d.b * da * (1 - sa)) / oa), (u8)(oa * 255)};
    dst = Canvas::pack(o);
}

struct Art {
    Canvas& c; Color team;
    Art(Canvas& cv, Color t) : c(cv), team(t) {}

    template <class F> void shape(int x0, int y0, int x1, int y1, F sdf, Color base, float bevel, float k, float alpha) {
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(c.w - 1, x1); y1 = std::min(c.h - 1, y1);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f, py = y + 0.5f, d = sdf(px, py);
            if (d > 0.75f) continue;
            float cov = clampf(0.5f - d, 0, 1) * alpha;
            if (cov <= 0) continue;
            float f = 1.0f;
            if (k != 0) {
                float gx = sdf(px + 0.6f, py) - sdf(px - 0.6f, py), gy = sdf(px, py + 0.6f) - sdf(px, py - 0.6f), gl = hyp(gx, gy);
                float lit = gl > 1e-4f ? (gx * -0.62f - gy * 0.78f) / gl : 0;
                float w = clampf((bevel + d) / bevel, 0, 1);
                f = 1.0f + k * lit * w;
            }
            Color col = shade(base, f); col.a = (u8)(base.a * cov);
            put(c, x, y, col);
        }
    }
    void circle(float cx, float cy, float r, Color base, float bevel = 2.4f, float k = 0.5f, float a = 1) {
        shape((int)(cx - r - 2), (int)(cy - r - 2), (int)(cx + r + 2), (int)(cy + r + 2), [=](float x, float y) { return hyp(x - cx, y - cy) - r; }, base, bevel, k, a);
    }
    void ell(float cx, float cy, float rx, float ry, Color base, float bevel = 2.4f, float k = 0.5f, float a = 1) {
        float m = std::min(rx, ry);
        shape((int)(cx - rx - 2), (int)(cy - ry - 2), (int)(cx + rx + 2), (int)(cy + ry + 2), [=](float x, float y) { return (hyp((x - cx) / rx, (y - cy) / ry) - 1) * m; }, base, bevel, k, a);
    }
    void box(float cx, float cy, float hw, float hh, float r, Color base, float bevel = 2.2f, float k = 0.5f, float a = 1) {
        r = std::min(r, std::min(hw, hh));
        shape((int)(cx - hw - 2), (int)(cy - hh - 2), (int)(cx + hw + 2), (int)(cy + hh + 2), [=](float x, float y) { return sdBox(x, y, cx, cy, hw, hh, r); }, base, bevel, k, a);
    }
    void cap(float ax, float ay, float bx, float by, float r, Color base, float bevel = 1.6f, float k = 0.5f, float a = 1) {
        shape((int)(std::min(ax, bx) - r - 2), (int)(std::min(ay, by) - r - 2), (int)(std::max(ax, bx) + r + 2), (int)(std::max(ay, by) + r + 2), [=](float x, float y) { return sdCap(x, y, ax, ay, bx, by, r); }, base, bevel, k, a);
    }
    void poly(std::vector<P> v, Color base, float bevel = 2.4f, float k = 0.5f, float a = 1) {
        float area = 0, mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (size_t i = 0; i < v.size(); i++) { const P& p = v[i]; const P& q = v[(i + 1) % v.size()]; area += p.x * q.y - q.x * p.y; mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
        float sgn = area > 0 ? 1.0f : -1.0f;
        shape((int)mnx - 2, (int)mny - 2, (int)mxx + 2, (int)mxy + 2, [&v, sgn](float x, float y) { return sdPoly(v, x, y, sgn); }, base, bevel, k, a);
    }
    // flat anti-aliased strokes and dots (no lighting)
    void line(float ax, float ay, float bx, float by, float w, Color col) { cap(ax, ay, bx, by, w * 0.5f, col, 1, 0, 1); }
    void dot(float cx, float cy, float r, Color col) { circle(cx, cy, r, col, 1, 0, 1); }
    void glow(float cx, float cy, float r, Color col) {
        for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++) for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            float d = hyp(x + 0.5f - cx, y + 0.5f - cy) / r;
            if (d >= 1) continue;
            Color k = col; k.a = (u8)(col.a * (1 - d) * (1 - d));
            put(c, x, y, k);
        }
    }
    // dirt, wear and rust: modulate what is already painted
    void grain(int x0, int y0, int x1, int y1, float amount, u32 seed) {
        Rng r(seed);
        for (int y = std::max(0, y0); y <= std::min(c.h - 1, y1); y++) for (int x = std::max(0, x0); x <= std::min(c.w - 1, x1); x++) {
            Color p = Canvas::unpack(c.px[y * c.w + x]);
            if (p.a < 200) continue;
            float f = 1.0f + (r.f() - 0.5f) * amount;
            c.px[y * c.w + x] = Canvas::pack(shade(p, f));
        }
    }
    void darken(int x0, int y0, int x1, int y1, float f) {
        for (int y = std::max(0, y0); y <= std::min(c.h - 1, y1); y++) for (int x = std::max(0, x0); x <= std::min(c.w - 1, x1); x++) {
            Color p = Canvas::unpack(c.px[y * c.w + x]);
            if (p.a < 200) continue;
            c.px[y * c.w + x] = Canvas::pack(shade(p, f));
        }
    }
    Color tm(float f = 1.0f) const { return shade(team, f); }
};

Color rgba(Color c, int a) { c.a = (u8)a; return c; }

// The player's mark on a unit: a small jewel-like dot. Dark seat ring for contrast on any hull tone, the team colour lit from
// the north-west like every other surface, a pinpoint highlight, and a faint halo so it still reads at half size.
void teamDot(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 1.3f, rgba(shade(CYD0, 0.6f), 235), 1.0f, 0.2f);
    A.circle(cx, cy, r, A.team, r, 0.75f);
    A.dot(cx - r * 0.32f, cy - r * 0.36f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.75f), 235));
}

// crawler track run: rubber band with grousers, idler wheels at both ends. phase (0..1) slides the grousers.
void trackRun(Art& A, float cx, float cy, float hl, float hh, float phase, Color dark) {
    A.box(cx, cy, hl, hh, hh * 0.6f, dark, 1.6f, 0.3f);
    float pitch = 3.4f;
    for (float x = cx - hl + 1.5f + phase * pitch; x < cx + hl - 1.5f; x += pitch) A.line(x, cy - hh + 0.9f, x, cy + hh - 0.9f, 1.0f, rgba(shade(dark, 2.3f), 150));
    A.circle(cx - hl + hh * 0.75f, cy, hh * 0.78f, shade(dark, 1.5f), 1.6f, 0.45f);
    A.circle(cx + hl - hh * 0.75f, cy, hh * 0.78f, shade(dark, 1.5f), 1.6f, 0.45f);
    A.dot(cx - hl + hh * 0.75f, cy, hh * 0.28f, shade(dark, 0.7f));
    A.dot(cx + hl - hh * 0.75f, cy, hh * 0.28f, shade(dark, 0.7f));
}

void hatch(Art& A, float cx, float cy, float r, Color base) {
    A.circle(cx, cy, r, base, 1.8f, 0.55f);
    A.circle(cx, cy, r * 0.62f, shade(base, 0.72f), 1.2f, 0.5f);
    A.dot(cx - r * 0.2f, cy - r * 0.2f, r * 0.14f, shade(base, 1.6f));
}

void rivets(Art& A, float x0, float y0, float x1, float y1, int n, Color c) {
    for (int i = 0; i < n; i++) { float t = n > 1 ? i / (float)(n - 1) : 0.5f; A.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.75f, c); }
}

void weatherClanker(Art& A, u32 seed, int x0, int y0, int x1, int y1) {
    A.grain(x0, y0, x1, y1, 0.16f, seed);
}

// ---------------------------------------------------------------- infantry
struct Trooper { Color armor, dark, helmet, gun; };

void soldier(Art& A, bool cy, int local, int frame) {
    Canvas& c = A.c; (void)c;
    Color armor = cy ? shade(CYH, 1.05f) : shade(CKH, 0.95f);
    Color dark = cy ? CYD : CKD;
    Color helmet = cy ? CYP : shade(CKH, 0.85f);
    float step = frame == 0 ? 0 : (frame == 1 ? 3.0f : -3.0f);
    bool heavy = local == 4 || (!cy && local == 9);
    float sw = heavy ? 10.5f : 9.5f;   // shoulder half-width
    // boots, alternating in the walk cycle
    A.ell(32 + step, 32 - 4.0f, 3.6f, 2.4f, shade(dark, 1.2f), 1.4f, 0.4f);
    A.ell(32 - step, 32 + 4.0f, 3.6f, 2.4f, shade(dark, 1.2f), 1.4f, 0.4f);
    // pack
    if (cy) {
        A.box(26.5f, 32, 4.4f, heavy ? 7.2f : 5.8f, 1.6f, shade(CYM, 1.2f), 1.8f, 0.5f);
        A.line(24.6f, 32 - 3.2f, 24.6f, 32 + 3.2f, 1.1f, rgba(CYG, 210));
        if (heavy) { A.glow(25, 32, 6, rgba(CYG, 110)); }
        if (local == 9) { A.cap(25, 29, 19, 24.5f, 0.9f, CYW, 1.0f, 0.4f); A.dot(18.6f, 24.2f, 1.2f, CYG); A.glow(18.6f, 24.2f, 4, rgba(CYG, 130)); }   // sniper mast
    } else {
        A.box(26.5f, 32, 4.6f, heavy ? 7.6f : 6.2f, 1.8f, shade(CKT, 0.85f), 1.8f, 0.5f);
        A.line(27, 32 - 5.0f, 27, 32 + 5.0f, 1.0f, rgba(CKD, 150));
        if (local == 3) A.cap(24, 32 - 5, 24, 32 + 5, 1.5f, CKR, 1.2f, 0.4f);   // rocket tubes
        if (local == 9) for (int i = 0; i < 4; i++) A.cap(24.5f, 32 - 4.5f + i * 3.0f, 27.5f, 32 - 4.5f + i * 3.0f, 1.2f, i % 2 ? CKY : CKR, 1.0f, 0.4f);   // grenade bandolier
    }
    // torso
    A.ell(32.5f, 32, heavy ? 6.6f : 6.0f, sw, armor, 3.4f, 0.55f);
    if (cy) { A.line(31, 27, 31, 37, 1.0f, rgba(CYG, 170)); if (heavy) { A.ell(32.5f, 32 - sw + 1.2f, 3.6f, 3.0f, CYP, 1.6f, 0.5f); A.ell(32.5f, 32 + sw - 1.2f, 3.6f, 3.0f, CYP, 1.6f, 0.5f); } }
    else { A.line(30, 28, 30, 36, 1.0f, rgba(CKD, 140)); A.box(33.5f, 32 + 3.2f, 1.8f, 1.4f, 0.4f, shade(CKT, 0.9f), 1, 0.3f); A.box(33.5f, 32 - 3.2f, 1.8f, 1.4f, 0.4f, shade(CKT, 0.9f), 1, 0.3f); }
    // arms and weapon
    Color sleeve = shade(armor, 0.82f);
    float gx = 41;   // hands
    A.cap(33.5f, 32 - sw + 2.4f, gx, 32 - 2.2f, 2.1f, sleeve, 1.4f, 0.5f);
    A.cap(33.5f, 32 + sw - 2.4f, gx + 3, 32 + 1.2f, 2.1f, sleeve, 1.4f, 0.5f);
    if (cy) {
        if (local == 2) {        // pulse rifle
            A.cap(35, 32, 50, 32, 1.5f, shade(CYM, 1.3f), 1.2f, 0.5f);
            A.box(43, 32 + 2.8f, 1.6f, 2.0f, 0.5f, CYD, 1, 0.4f);
            A.dot(50.5f, 32, 1.3f, CYG); A.glow(51, 32, 4, rgba(CYG, 130));
        } else if (local == 3) { // laser rifle: long barrel, lens, scope
            A.cap(34, 32, 52, 32, 1.3f, CYW, 1.2f, 0.55f);
            A.box(42, 32 - 2.4f, 3, 1.1f, 0.5f, CYD, 1, 0.4f);
            A.circle(52.5f, 32, 1.9f, CYG, 1.2f, 0.4f); A.glow(53, 32, 5, rgba(CYG, 150));
            A.line(37, 31.2f, 47, 31.2f, 0.7f, rgba(CYG, 200));
        } else if (local == 9) {  // ion lance: very long emitter rail with a scope and a bright muzzle
            A.cap(33, 32, 58, 32, 1.35f, CYW, 1.2f, 0.55f);
            A.cap(36, 32, 46, 32, 2.1f, shade(CYM, 1.3f), 1.4f, 0.5f);
            A.box(41, 32 - 3.1f, 4.2f, 1.2f, 0.5f, CYD, 1, 0.4f); A.dot(45.5f, 32 - 3.1f, 1.1f, CYG);
            for (int i = 0; i < 4; i++) A.box(49 + i * 2.4f, 32, 0.5f, 2.3f, 0.2f, rgba(CYG, 220), 1, 0);
            A.circle(58.4f, 32, 1.7f, CYG, 1.2f, 0.4f); A.glow(59, 32, 6, rgba(CYG, 170));
        } else {                 // arc caster: forked emitter
            A.cap(35, 32, 47, 32, 2.2f, shade(CYM, 1.4f), 1.4f, 0.5f);
            A.cap(46, 32 - 2.6f, 54, 32 - 3.4f, 1.0f, CYW, 1, 0.4f); A.cap(46, 32 + 2.6f, 54, 32 + 3.4f, 1.0f, CYW, 1, 0.4f);
            A.glow(52, 32, 7, rgba(CYG, 170)); A.line(50, 32 - 3.0f, 53, 32, 0.9f, rgba(CYW, 230)); A.line(50, 32 + 3.0f, 53, 32, 0.9f, rgba(CYW, 230));
        }
    } else {
        if (local == 2) {        // assault rifle
            A.cap(34, 32, 40, 32, 2.0f, shade(CKR, 0.75f), 1.2f, 0.4f);           // stock/receiver
            A.cap(39, 32, 51, 32, 1.1f, CKS, 1.0f, 0.5f);
            A.box(43.5f, 32 + 3.0f, 1.3f, 2.4f, 0.5f, CKD, 1, 0.4f);              // magazine
            A.dot(51.5f, 32, 1.0f, CKD);
        } else if (local == 3) { // RPG on the shoulder
            A.cap(28, 31, 46, 31, 2.7f, shade(CKH, 1.05f), 1.8f, 0.55f);
            A.cap(46, 31, 52, 31, 3.0f, CKR, 1.4f, 0.5f);                          // warhead
            A.dot(52.5f, 31, 1.5f, shade(CKY, 0.9f));
            A.line(27, 31, 27, 31, 1, rgba(CKD, 255)); A.circle(27.5f, 31, 2.6f, CKD, 1.2f, 0.4f);   // rear cone
            A.line(38, 28.6f, 38, 33.4f, 1.0f, rgba(CKD, 160));
        } else if (local == 9) { // grenade launcher: fat short barrel, revolving drum and a stubby sight
            A.cap(33, 32, 47, 32, 3.2f, shade(CKD, 1.5f), 1.6f, 0.55f);
            A.cap(45, 32, 51, 32, 3.7f, CKS, 1.4f, 0.5f);
            A.circle(51.4f, 32, 2.0f, CKD, 1.2f, 0.4f);
            A.circle(38.5f, 32 + 4.2f, 3.6f, shade(CKS, 0.9f), 1.8f, 0.55f);
            for (int i = 0; i < 6; i++) { float a = i * 1.0472f; A.dot(38.5f + std::cos(a) * 2.0f, 32 + 4.2f + std::sin(a) * 2.0f, 0.65f, CKD); }
            A.box(40, 32 - 3.6f, 2.4f, 1.0f, 0.4f, CKD, 1, 0.3f);
        } else {                 // heavy machine gun with bipod and belt
            A.cap(35, 32, 53, 32, 1.6f, CKD, 1.2f, 0.5f);
            A.box(40, 32, 4.5f, 2.6f, 0.8f, shade(CKS, 0.9f), 1.4f, 0.5f);
            A.cap(50, 32, 53.5f, 32, 2.2f, CKD, 1.0f, 0.3f);                       // flash hider
            A.line(48, 32, 52, 27.5f, 1.0f, CKD); A.line(48, 32, 52, 36.5f, 1.0f, CKD);   // bipod
            A.box(40, 32 + 4.6f, 2.6f, 2.2f, 0.6f, shade(CKT, 0.8f), 1.2f, 0.4f);   // ammo box
            A.cap(41, 32 + 3.0f, 38, 32 + 5.5f, 0.7f, CKY, 1, 0.3f);
        }
    }
    // head
    A.circle(33.5f, 32, heavy ? 5.2f : 4.7f, helmet, 3.0f, 0.65f);
    if (cy) { A.box(36.8f, 32, 1.6f, 2.8f, 0.8f, CYG, 1, 0.2f); A.glow(37.5f, 32, 4, rgba(CYG, 90)); }
    else { A.line(34.5f, 32 - 3.2f, 34.5f, 32 + 3.2f, 1.1f, rgba(CKD, 200)); A.dot(31.5f, 32, 1.0f, rgba(CKD, 140)); }
    teamDot(A, 32.0f, 32, heavy ? 2.3f : 2.1f);            // helmet crest
}

// ---------------------------------------------------------------- vehicles
void dozerCyber(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 21, 17, 5.0f, ph, CYD); trackRun(A, 30, 43, 17, 5.0f, ph, CYD);
    A.box(30, 32, 16, 10.5f, 3, CYH, 2.6f, 0.55f);
    A.box(21, 32, 7, 7.5f, 2, CYM, 2, 0.5f);                                    // rear power unit
    for (int i = 0; i < 4; i++) A.line(17.5f + i * 2.6f, 27.5f, 17.5f + i * 2.6f, 36.5f, 0.9f, rgba(CYD, 190));
    A.box(34, 32, 6.5f, 7.0f, 2, shade(CYP, 0.9f), 2.2f, 0.55f);                   // cab roof
    A.box(37.2f, 32, 2.6f, 5.4f, 1.2f, rgba(shade(CYG, 0.5f), 255), 1.4f, 0.5f);      // windshield glass
    A.line(35.5f, 28.2f, 38.5f, 28.8f, 0.8f, rgba(CYW, 200));
    A.dot(32.5f, 32, 1.6f, rgb(255, 170, 60)); A.glow(32.5f, 32, 4, rgba(rgb(255, 170, 60), 120));    // beacon
    // blade on articulated arms with emitter edge
    A.cap(42, 26, 48, 24, 1.8f, CYM, 1.2f, 0.5f); A.cap(42, 38, 48, 40, 1.8f, CYM, 1.2f, 0.5f);
    A.box(51.5f, 32, 3.4f, 14.5f, 1.4f, shade(CYP, 1.05f), 2, 0.55f);
    A.line(54.6f, 19, 54.6f, 45, 1.0f, rgba(CYG, 230)); A.glow(54.5f, 32, 10, rgba(CYG, 70));
    teamDot(A, 19.5f, 32, 3.0f);
}
void dozerClanker(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 29, 20.5f, 18, 5.5f, ph, CKD); trackRun(A, 29, 43.5f, 18, 5.5f, ph, CKD);
    A.box(29, 32, 17, 11, 2, CKY, 2.4f, 0.5f);
    A.box(21, 32, 8, 8, 1.5f, shade(CKY, 0.82f), 2, 0.45f);
    for (int i = 0; i < 3; i++) A.line(16.5f + i * 3.2f, 27, 16.5f + i * 3.2f, 37, 1.0f, rgba(CKD, 150));
    A.circle(19, 40.5f, 2.2f, CKS, 1.4f, 0.5f); A.dot(19, 40.5f, 1.0f, CKD);        // exhaust stack
    A.box(35, 32, 6, 7.2f, 1.6f, shade(CKH, 1.0f), 2, 0.5f);                           // cab
    A.box(38, 32, 2.6f, 5.6f, 1, rgb(72, 96, 108), 1.2f, 0.5f);
    for (int i = 0; i < 3; i++) A.line(30.5f + i * 2.4f, 25.6f, 30.5f + i * 2.4f, 38.4f, 0.7f, rgba(CKD, 120));   // roll cage
    A.dot(33, 28.5f, 1.3f, rgb(255, 150, 40));
    A.cap(42, 24.5f, 48, 22.5f, 2.0f, CKS, 1.2f, 0.5f); A.cap(42, 39.5f, 48, 41.5f, 2.0f, CKS, 1.2f, 0.5f);
    A.box(51.5f, 32, 3.8f, 15, 1.2f, CKS, 2.2f, 0.6f);
    for (int i = 0; i < 6; i++) A.box(51.5f, 20.5f + i * 4.6f, 3.6f, 1.6f, 0.5f, i % 2 ? CKY : CKD, 1, 0.2f);   // hazard blade teeth
    weatherClanker(A, 91, 10, 12, 58, 52);
    teamDot(A, 17.5f, 32, 3.0f);
}

void harvCyber(Art& A, int frame) {
    float ph = frame == 2 ? 0.5f : 0;
    for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) A.box(19 + i * 9.5f, 32 + s * 13.2f, 3.6f, 2.6f, 1.2f, shade(CYD, 1.1f), 1.4f, 0.4f);   // wheel pods
    (void)ph;
    A.box(25, 32, 19, 11.5f, 2.6f, shade(CYH, 0.95f), 2.4f, 0.5f);                  // cargo container
    for (int i = 0; i < 6; i++) A.line(11 + i * 5.6f, 23, 11 + i * 5.6f, 41, 0.9f, rgba(CYD, 170));
    A.box(25, 32, 15, 5.2f, 1.6f, shade(CYM, 1.3f), 1.6f, 0.45f);                    // fill hatch
    A.line(12, 32, 38, 32, 1.4f, rgba(CYG, 180)); A.glow(25, 32, 14, rgba(CYG, 40));
    A.box(48, 32, 9, 9, 2.4f, shade(CYP, 0.85f), 2.4f, 0.55f);                        // cab
    A.box(51.5f, 32, 3.4f, 7, 1.2f, shade(CYG, 0.45f), 1.4f, 0.5f);
    A.line(49, 27, 52.5f, 27.4f, 0.8f, rgba(CYW, 190));
    teamDot(A, 10.5f, 32, 2.8f);
}
void harvClanker(Art& A, int frame) {
    for (int s = -1; s <= 1; s += 2) for (int i = 0; i < 3; i++) A.box(19 + i * 9.5f, 32 + s * 13.6f, 3.9f, 2.8f, 1.2f, CKD, 1.4f, 0.4f);
    A.box(25, 32, 19, 12, 1.8f, shade(CKH, 0.9f), 2.2f, 0.5f);                          // bed
    // load of crates under a strapped tarp edge
    for (int j = 0; j < 2; j++) for (int i = 0; i < 4; i++) {
        float x = 12 + i * 8.6f, y = 26.4f + j * 8.4f;
        A.box(x, y, 3.9f, 3.7f, 0.6f, shade(rgb(154, 122, 72), 0.9f + ((i + j * 3) % 3) * 0.08f), 1.6f, 0.6f);
        A.line(x - 3, y, x + 3, y, 0.6f, rgba(CKD, 110));
    }
    A.line(9, 32, 40, 32, 1.0f, rgba(CKD, 110));
    A.box(48.5f, 32, 8.5f, 9.6f, 2, CKR, 2.4f, 0.55f);                                 // cab
    A.box(52, 32, 3.2f, 7.4f, 1, rgb(74, 98, 110), 1.2f, 0.5f);
    A.box(46, 32, 1.5f, 8, 0.6f, shade(CKR, 0.75f), 1, 0.3f);
    A.dot(55.5f, 27, 1.0f, rgb(255, 240, 190)); A.dot(55.5f, 37, 1.0f, rgb(255, 240, 190));
    weatherClanker(A, 17, 8, 12, 60, 52);
    teamDot(A, 10.0f, 32, 2.8f);
}

void tankCyber(Art& A, int frame) {   // Photon Tank body
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 17, 21, 5.2f, ph, CYD); trackRun(A, 30, 47, 21, 5.2f, ph, CYD);
    A.poly({ {10, 21}, {38, 21}, {52, 27.5f}, {52, 36.5f}, {38, 43}, {10, 43} }, CYH, 2.8f, 0.6f);
    A.box(21, 32, 7.5f, 9, 1.5f, shade(CYM, 1.15f), 2, 0.5f);                              // engine deck
    for (int i = 0; i < 5; i++) A.line(15.8f + i * 2.5f, 25.5f, 15.8f + i * 2.5f, 38.5f, 0.8f, rgba(CYD, 200));
    A.line(36, 22.5f, 48, 28.2f, 0.9f, rgba(CYD, 170)); A.line(36, 41.5f, 48, 35.8f, 0.9f, rgba(CYD, 170));    // glacis seams
    A.line(11, 22.2f, 37, 22.2f, 1.2f, rgba(CYG, 210)); A.line(11, 41.8f, 37, 41.8f, 1.2f, rgba(CYG, 210));   // side light strips
    A.glow(24, 22.2f, 7, rgba(CYG, 60)); A.glow(24, 41.8f, 7, rgba(CYG, 60));
    A.dot(50, 29.8f, 1.1f, CYW); A.dot(50, 34.2f, 1.1f, CYW);                                // headlamps
    A.grain(9, 18, 54, 46, 0.06f, 5);
}
void tankClanker(Art& A, int frame) {   // Brute Tank body
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 16.5f, 22, 5.6f, ph, CKD); trackRun(A, 30, 47.5f, 22, 5.6f, ph, CKD);
    A.poly({ {9, 20.5f}, {40, 20.5f}, {53, 26}, {53, 38}, {40, 43.5f}, {9, 43.5f} }, CKH, 2.8f, 0.6f);
    A.box(30, 21.6f, 20, 2.6f, 0.8f, shade(CKH, 0.86f), 1.4f, 0.5f); A.box(30, 42.4f, 20, 2.6f, 0.8f, shade(CKH, 0.86f), 1.4f, 0.5f);   // skirts
    rivets(A, 12, 22.2f, 46, 22.2f, 11, shade(CKH, 1.3f)); rivets(A, 12, 41.8f, 46, 41.8f, 11, shade(CKH, 1.3f));
    A.box(18, 32, 8, 9.4f, 1.4f, shade(CKD, 1.25f), 2, 0.5f);                                // engine grille
    for (int i = 0; i < 5; i++) A.line(12.5f + i * 2.6f, 24.4f, 12.5f + i * 2.6f, 39.6f, 0.9f, rgba(CKD, 210));
    A.circle(12.5f, 26.5f, 2.6f, CKR, 1.6f, 0.6f); A.circle(12.5f, 37.5f, 2.6f, CKR, 1.6f, 0.6f);                // fuel drums
    A.line(10.4f, 26.5f, 14.6f, 26.5f, 0.7f, rgba(CKD, 140)); A.line(10.4f, 37.5f, 14.6f, 37.5f, 0.7f, rgba(CKD, 140));
    A.line(42, 22, 50, 27, 0.9f, rgba(CKD, 160)); A.line(42, 42, 50, 37, 0.9f, rgba(CKD, 160));
    A.dot(51, 28.5f, 1.1f, rgb(255, 240, 190)); A.dot(51, 35.5f, 1.1f, rgb(255, 240, 190));
    weatherClanker(A, 33, 8, 14, 56, 50);
}

void walkerCyber(Art& A, int frame) {   // Volt Walker: four articulated legs
    float sw = frame == 0 ? 0 : (frame == 1 ? 3.2f : -3.2f);
    struct Leg { float hx, hy, kx, ky, fx, fy; };
    Leg legs[4] = {
        { 38, 25, 44 + sw, 17, 47 + sw, 11 }, { 38, 39, 44 - sw, 47, 47 - sw, 53 },
        { 26, 25, 21 - sw, 16, 17 - sw, 12 }, { 26, 39, 21 + sw, 48, 17 + sw, 52 } };
    for (auto& l : legs) {
        A.cap(l.hx, l.hy, l.kx, l.ky, 2.7f, shade(CYM, 1.05f), 1.8f, 0.5f);
        A.cap(l.kx, l.ky, l.fx, l.fy, 2.1f, shade(CYD, 1.3f), 1.6f, 0.5f);
        A.circle(l.kx, l.ky, 2.9f, shade(CYP, 0.85f), 1.8f, 0.55f);
        A.circle(l.fx, l.fy, 3.0f, shade(CYD, 1.5f), 1.6f, 0.5f);
    }
    A.circle(32, 32, 13.5f, CYH, 3.4f, 0.6f);
    A.circle(32, 32, 10, shade(CYH, 0.82f), 2.4f, 0.5f);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f; A.line(32 + std::cos(a) * 8.2f, 32 + std::sin(a) * 8.2f, 32 + std::cos(a) * 11.6f, 32 + std::sin(a) * 11.6f, 1.1f, rgba(CYD, 200)); }
    for (int i = 0; i < 16; i++) { float a = i * 0.3927f; A.dot(32 + std::cos(a) * 12.6f, 32 + std::sin(a) * 12.6f, 0.8f, rgba(CYG, 190)); }
}
void tankCyberRail(Art& A, int frame) {   // Railgun Tank body: long, heavy
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 16.5f, 24, 5.4f, ph, CYD); trackRun(A, 30, 47.5f, 24, 5.4f, ph, CYD);
    A.poly({ {7, 20.5f}, {42, 20.5f}, {55, 26.5f}, {55, 37.5f}, {42, 43.5f}, {7, 43.5f} }, shade(CYH, 1.05f), 2.8f, 0.6f);
    A.line(8, 21.8f, 41, 21.8f, 1.2f, rgba(CYG, 220)); A.line(8, 42.2f, 41, 42.2f, 1.2f, rgba(CYG, 220));
    A.glow(24, 21.8f, 8, rgba(CYG, 55)); A.glow(24, 42.2f, 8, rgba(CYG, 55));
    for (int i = 0; i < 3; i++) {                                                             // capacitor bank
        float y = 25.5f + i * 6.5f;
        A.circle(13.5f, y, 2.6f, shade(CYM, 1.3f), 1.6f, 0.6f); A.circle(13.5f, y, 1.4f, CYG, 1, 0.2f); A.glow(13.5f, y, 5, rgba(CYG, 90));
    }
    A.box(24, 32, 1.4f, 9, 0.5f, rgba(CYD, 255), 1, 0.1f);
    A.line(44, 23, 52, 27, 0.9f, rgba(CYD, 170)); A.line(44, 41, 52, 37, 0.9f, rgba(CYD, 170));
}
void gatlingBody(Art& A, int frame) {   // Gatling Tank body: lighter, wider tracks
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 31, 17, 20, 5.0f, ph, CKD); trackRun(A, 31, 47, 20, 5.0f, ph, CKD);
    A.poly({ {11, 21.5f}, {40, 21.5f}, {51, 27}, {51, 37}, {40, 42.5f}, {11, 42.5f} }, shade(CKH, 1.05f), 2.6f, 0.6f);
    A.box(17.5f, 32, 6.5f, 8.4f, 1.2f, shade(CKD, 1.3f), 2, 0.5f);
    for (int i = 0; i < 4; i++) A.line(13.5f + i * 2.6f, 25, 13.5f + i * 2.6f, 39, 0.8f, rgba(CKD, 210));
    A.box(43, 32, 4, 8, 1, CKT, 1.6f, 0.5f);
    A.dot(49, 29, 1.0f, rgb(255, 240, 190)); A.dot(49, 35, 1.0f, rgb(255, 240, 190));
    weatherClanker(A, 71, 9, 14, 56, 50);
}
void launcherBody(Art& A, int frame) {   // Rocket Launcher body: truck-style carrier
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 29, 17, 23, 5.2f, ph, CKD); trackRun(A, 29, 47, 23, 5.2f, ph, CKD);
    A.box(27, 32, 21, 10.6f, 2.2f, shade(CKH, 1.0f), 2.6f, 0.55f);
    A.box(51, 32, 7, 9.6f, 2, CKT, 2.4f, 0.55f);                                      // cab
    A.box(54, 32, 2.4f, 7, 0.8f, rgb(74, 98, 110), 1.2f, 0.5f);
    A.box(27, 32, 16, 8, 1.2f, shade(CKD, 1.4f), 2, 0.4f);                              // turntable deck
    for (int i = -1; i <= 1; i += 2) { A.box(12, 32 + i * 10.6f, 3.6f, 1.6f, 0.5f, CKS, 1.4f, 0.5f); }   // outrigger pads
    A.dot(57, 28.6f, 1.0f, rgb(255, 240, 190)); A.dot(57, 35.4f, 1.0f, rgb(255, 240, 190));
    weatherClanker(A, 5, 6, 12, 60, 52);
}

void titanCyber(Art& A, int frame) {   // Aegis Titan body: broad tracked hull, armoured shoulders, gold emitter trim
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 30, 13.5f, 25, 6.4f, ph, CYD); trackRun(A, 30, 50.5f, 25, 6.4f, ph, CYD);
    A.poly({ {5, 19}, {40, 19}, {57, 25.5f}, {57, 38.5f}, {40, 45}, {5, 45} }, CYH, 3.0f, 0.62f);
    A.box(30, 20.6f, 22, 2.4f, 0.8f, shade(CYP, 1.0f), 1.4f, 0.5f); A.box(30, 43.4f, 22, 2.4f, 0.8f, shade(CYP, 1.0f), 1.4f, 0.5f);   // side armour plates
    A.line(8, 22.6f, 40, 22.6f, 1.2f, rgba(CYG, 235)); A.line(8, 41.4f, 40, 41.4f, 1.2f, rgba(CYG, 235));
    A.glow(24, 22.6f, 8, rgba(CYG, 60)); A.glow(24, 41.4f, 8, rgba(CYG, 60));
    A.box(14, 32, 8.5f, 10.5f, 1.6f, shade(CYM, 1.15f), 2.2f, 0.5f);                          // reactor deck
    A.circle(13, 32, 5.2f, CYD, 1.8f, 0.5f); A.circle(13, 32, 3.4f, CYG, 1.4f, 0.3f); A.glow(13, 32, 10, rgba(CYG, 120));
    for (int i = 0; i < 3; i++) A.line(19.5f + i * 2.4f, 25.5f, 19.5f + i * 2.4f, 38.5f, 0.8f, rgba(CYD, 200));
    A.line(42, 21, 53, 26.6f, 0.9f, rgba(CYD, 170)); A.line(42, 43, 53, 37.4f, 0.9f, rgba(CYD, 170));
    A.dot(54.5f, 28.5f, 1.2f, CYW); A.dot(54.5f, 35.5f, 1.2f, CYW);
    A.grain(6, 15, 58, 49, 0.06f, 9);
}
void behemothBody(Art& A, int frame) {   // Behemoth body: cast-iron slab hull on four heavy track pods, hazard rear
    float ph = frame == 2 ? 0.5f : 0;
    trackRun(A, 29, 13.5f, 25, 6.4f, ph, CKD); trackRun(A, 29, 50.5f, 25, 6.4f, ph, CKD);
    A.poly({ {4, 19}, {42, 19}, {57, 25}, {57, 39}, {42, 45}, {4, 45} }, CKH, 3.0f, 0.62f);
    A.box(30, 20.8f, 24, 2.8f, 0.8f, shade(CKH, 0.84f), 1.4f, 0.5f); A.box(30, 43.2f, 24, 2.8f, 0.8f, shade(CKH, 0.84f), 1.4f, 0.5f);
    rivets(A, 8, 22.4f, 50, 22.4f, 13, shade(CKH, 1.4f)); rivets(A, 8, 41.6f, 50, 41.6f, 13, shade(CKH, 1.4f));
    A.box(14.5f, 32, 8, 10.4f, 1.4f, shade(CKD, 1.3f), 2, 0.5f);
    for (int i = 0; i < 5; i++) A.line(9.8f + i * 2.4f, 25, 9.8f + i * 2.4f, 39, 0.9f, rgba(CKD, 220));
    A.circle(7.5f, 26, 2.8f, CKS, 1.6f, 0.55f); A.dot(7.5f, 26, 1.1f, CKD); A.circle(7.5f, 38, 2.8f, CKS, 1.6f, 0.55f); A.dot(7.5f, 38, 1.1f, CKD);   // exhaust stacks
    for (int i = 0; i < 5; i++) A.box(4.6f + 0, 22.5f + i * 4.7f, 1.6f, 1.9f, 0.4f, i % 2 ? CKY : CKD, 1, 0.2f);                              // hazard rear
    A.line(44, 21.5f, 53, 26.4f, 0.9f, rgba(CKD, 170)); A.line(44, 42.5f, 53, 37.6f, 0.9f, rgba(CKD, 170));
    A.dot(55, 28.6f, 1.2f, rgb(255, 240, 190)); A.dot(55, 35.4f, 1.2f, rgb(255, 240, 190));
    weatherClanker(A, 81, 6, 15, 60, 49);
}

// ---------------------------------------------------------------- aircraft
void droneBody(Art& A) {   // Wraith Drone: stealth flying wing
    A.poly({ {58, 32}, {40, 24}, {13, 8}, {8, 12}, {19, 26}, {15, 32}, {19, 38}, {8, 52}, {13, 56}, {40, 40} }, CYH, 2.8f, 0.6f);
    // centre spine and canopy sensor
    A.poly({ {54, 32}, {40, 28}, {20, 29}, {17, 32}, {20, 35}, {40, 36} }, shade(CYP, 0.9f), 2.6f, 0.6f);
    A.ell(38, 32, 5, 2.6f, rgb(20, 60, 84), 1.6f, 0.5f); A.line(34, 31, 42, 31, 0.8f, rgba(CYG, 210));
    // wing panel lines and weapon pods
    A.line(20, 26, 40, 27, 0.8f, rgba(CYD, 170)); A.line(20, 38, 40, 37, 0.8f, rgba(CYD, 170));
    A.cap(28, 18.5f, 40, 22, 1.6f, CYM, 1.2f, 0.5f); A.cap(28, 45.5f, 40, 42, 1.6f, CYM, 1.2f, 0.5f);
    A.dot(41, 22.2f, 1.1f, CYG); A.dot(41, 41.8f, 1.1f, CYG);
    // twin engines with glowing exhaust
    for (int s = -1; s <= 1; s += 2) {
        A.box(15, 32 + s * 5, 4, 2.6f, 1.2f, shade(CYD, 1.3f), 1.4f, 0.5f);
        A.dot(11.2f, 32 + s * 5, 2.4f, CYG); A.glow(10, 32 + s * 5, 8, rgba(CYG, 190));
    }
    teamDot(A, 25.0f, 32, 2.9f);
    A.dot(9.5f, 12, 0.9f, rgb(255, 80, 70)); A.dot(9.5f, 52, 0.9f, rgb(80, 255, 120));   // nav lights
}
void gunshipBody(Art& A) {   // Vulture Gunship: fuselage, tail boom, stub wings with rocket pods
    A.cap(28, 32, 4, 32, 2.6f, shade(CKH, 0.9f), 1.8f, 0.5f);                          // tail boom
    A.box(6, 32, 1.4f, 7.5f, 0.6f, shade(CKH, 0.8f), 1.2f, 0.4f);                       // tail stabiliser
    A.circle(4.5f, 32, 4.2f, rgba(CKD, 120), 1, 0, 0.5f);                                // tail rotor disc
    A.cap(4.5f, 27.6f, 4.5f, 36.4f, 0.9f, CKD, 1, 0.3f);
    // stub wings + pods
    A.box(30, 32, 4.4f, 20, 1.6f, shade(CKH, 0.95f), 2, 0.5f);
    for (int s = -1; s <= 1; s += 2) {
        A.box(34, 32 + s * 14.6f, 7, 3.0f, 1.2f, CKS, 1.6f, 0.55f);
        for (int k = 0; k < 3; k++) A.dot(40.6f, 32 + s * (12.8f + k * 1.8f), 0.9f, CKR);
        A.dot(41.5f, 32 + s * 14.6f, 1.2f, rgb(60, 40, 30));
    }
    // fuselage
    A.ell(34, 32, 19, 8.6f, CKH, 3.2f, 0.6f);
    A.ell(30, 32, 10, 6.2f, shade(CKH, 0.85f), 2, 0.5f);
    A.line(20, 27.8f, 39, 27.2f, 0.7f, rgba(CKD, 150)); A.line(20, 36.2f, 39, 36.8f, 0.7f, rgba(CKD, 150));
    A.ell(43.5f, 32, 8.2f, 5.6f, rgb(70, 108, 128), 3.0f, 0.75f);                        // canopy glass
    A.ell(42, 30, 3.6f, 1.8f, rgba(rgb(200, 230, 245), 190), 1, 0);                         // glint
    A.cap(51, 32, 56, 32, 1.2f, CKD, 1.2f, 0.5f);                                         // chin gun
    A.circle(28, 32, 3.0f, CKS, 1.8f, 0.55f);                                              // rotor hub
    A.grain(14, 22, 56, 42, 0.14f, 44);
    teamDot(A, 21.5f, 32, 2.7f);
}

}  // namespace

// ---------------------------------------------------------------- public entry points
void artUnitBody(Canvas& c, int type, Color team, int frame) {
    setTone(type);
    Art A(c, team);
    bool cy = UNITS[type].faction == F_CYBER;
    int local = type - firstUnitOf(UNITS[type].faction);
    switch (local) {
    case 0: if (cy) dozerCyber(A, frame); else dozerClanker(A, frame); break;
    case 1: if (cy) harvCyber(A, frame); else harvClanker(A, frame); break;
    case 2: case 3: case 4: soldier(A, cy, local, frame); break;
    case 5: if (cy) tankCyber(A, frame); else tankClanker(A, frame); break;
    case 6: if (cy) walkerCyber(A, frame); else gatlingBody(A, frame); break;
    case 7: if (cy) tankCyberRail(A, frame); else launcherBody(A, frame); break;
    case 8: if (cy) droneBody(A); else gunshipBody(A); break;
    case 9: soldier(A, cy, local, frame); break;
    case 10: if (cy) titanCyber(A, frame); else behemothBody(A, frame); break;
    }
    if (local == 8 && !cy) c.outline(rgb(10, 12, 14, 150)); else c.outline(rgb(8, 10, 14, 200));
}

bool artUnitTurret(Canvas& c, int type, Color team) {
    setTone(type);
    Art A(c, team);
    bool cy = UNITS[type].faction == F_CYBER;
    int local = type - firstUnitOf(UNITS[type].faction);
    switch (local) {
    case 5:
        if (cy) {   // photon cannon turret
            A.circle(32, 32, 10.5f, rgba(CYD, 110), 1, 0);   // ring shadow
            A.cap(42, 29.6f, 63, 29.6f, 1.7f, shade(CYM, 1.4f), 1.4f, 0.55f); A.cap(42, 34.4f, 63, 34.4f, 1.7f, shade(CYM, 1.4f), 1.4f, 0.55f);
            A.line(44, 32, 62, 32, 1.5f, rgba(CYG, 230)); A.glow(60, 32, 7, rgba(CYG, 120));
            A.box(60.5f, 32, 2.2f, 6, 0.8f, shade(CYP, 0.9f), 1.4f, 0.5f);
            A.poly({ {21, 23.5f}, {37, 23}, {46, 28.5f}, {46, 35.5f}, {37, 41}, {21, 40.5f}, {17.5f, 36}, {17.5f, 28} }, CYP, 2.8f, 0.65f);
            A.poly({ {23, 26}, {35, 25.5f}, {41, 29.5f}, {41, 34.5f}, {35, 38.5f}, {23, 38}, {21, 35}, {21, 29} }, shade(CYP, 1.12f), 2, 0.5f);
            hatch(A, 27.5f, 32, 3.6f, shade(CYH, 1.1f));
            A.box(41.5f, 32, 1.6f, 2.6f, 0.6f, CYD, 1, 0.3f); A.dot(42.5f, 32, 1.1f, CYG);
            teamDot(A, 34.5f, 32, 2.5f);
            A.line(18, 40, 12, 47, 0.8f, rgba(CYW, 220));       // antenna
        } else {    // 120mm turret
            A.circle(32, 32, 11, rgba(CKD, 110), 1, 0);
            A.cap(42, 32, 63, 32, 1.9f, shade(CKD, 1.5f), 1.6f, 0.6f);                     // barrel
            A.cap(48, 32, 52, 32, 2.6f, shade(CKD, 1.7f), 1.4f, 0.5f);                     // bore evacuator
            A.box(61.5f, 32, 2.2f, 3.2f, 0.7f, shade(CKD, 1.3f), 1.2f, 0.5f);              // muzzle brake
            A.poly({ {19, 23}, {36, 22.5f}, {46, 27.5f}, {46, 36.5f}, {36, 41.5f}, {19, 41}, {15.5f, 36.5f}, {15.5f, 27.5f} }, shade(CKH, 1.08f), 3.0f, 0.65f);
            A.box(43.5f, 32, 3.6f, 6.6f, 1.4f, shade(CKH, 0.8f), 1.6f, 0.5f);               // mantlet
            rivets(A, 20, 24.6f, 34, 24.6f, 6, shade(CKH, 1.4f)); rivets(A, 20, 39.4f, 34, 39.4f, 6, shade(CKH, 1.4f));
            hatch(A, 25, 27.5f, 3.2f, shade(CKH, 1.05f));                                  // commander cupola
            A.cap(25, 27.5f, 30, 27.5f, 0.9f, CKD, 1, 0.3f);                               // pintle MG
            A.box(19, 35.5f, 3.4f, 3.0f, 0.6f, shade(CKT, 0.85f), 1.4f, 0.5f);              // stowage
            teamDot(A, 31.5f, 33.5f, 2.6f);
            A.grain(14, 20, 48, 44, 0.16f, 12);
            A.line(17, 24, 11, 15, 0.8f, rgba(CKD, 230));
        }
        break;
    case 6:
        if (cy) {   // volt coil
            A.glow(32, 32, 15, rgba(CYG, 120));
            A.cap(36, 29, 55, 29, 1.5f, shade(CYM, 1.5f), 1.4f, 0.55f); A.cap(36, 35, 55, 35, 1.5f, shade(CYM, 1.5f), 1.4f, 0.55f);
            for (int i = 0; i < 4; i++) { float x = 40 + i * 4.2f; A.box(x, 32, 1.2f, 6.6f, 0.5f, i % 2 ? CYG : CYW, 1, 0.3f); }
            A.glow(56, 32, 6, rgba(CYW, 160)); A.dot(56, 32, 1.6f, CYW);
            A.circle(32, 32, 8.6f, CYP, 2.6f, 0.65f);
            A.circle(32, 32, 5.4f, shade(CYM, 1.2f), 2, 0.5f);
            A.circle(32, 32, 3.2f, CYW, 1.6f, 0.55f); A.glow(32, 32, 7, rgba(CYG, 160));
            for (int i = 0; i < 8; i++) { float a = i * 0.7854f; A.dot(32 + std::cos(a) * 7.0f, 32 + std::sin(a) * 7.0f, 0.9f, CYG); }
            teamDot(A, 25.5f, 32, 2.3f);
        } else {    // gatling
            A.circle(32, 32, 10, rgba(CKD, 110), 1, 0);
            for (int i = -1; i <= 1; i++) A.cap(40, 32 + i * 2.8f, 58, 32 + i * 2.8f, 1.35f, i == 0 ? CKS : shade(CKD, 1.7f), 1.2f, 0.55f);
            for (int i = 0; i < 3; i++) A.box(46 + i * 4.6f, 32, 0.9f, 5.0f, 0.4f, shade(CKS, 0.8f), 1, 0.3f);   // barrel clamps
            A.box(58, 32, 1.2f, 5.6f, 0.4f, shade(CKD, 1.4f), 1, 0.3f);
            A.poly({ {20, 24}, {38, 24}, {42, 28.5f}, {42, 35.5f}, {38, 40}, {20, 40}, {16.5f, 36}, {16.5f, 28} }, shade(CKH, 1.1f), 2.8f, 0.65f);
            A.box(27, 43.4f, 4.6f, 3.6f, 1, shade(CKD, 1.3f), 1.6f, 0.5f);                     // ammo drum
            A.cap(30, 41, 37, 37, 0.8f, CKY, 1, 0.3f);                                        // feed belt
            hatch(A, 25, 32, 3.4f, shade(CKH, 1.05f));
            teamDot(A, 31.5f, 32, 2.6f);
            A.grain(16, 22, 44, 42, 0.14f, 21);
        }
        break;
    case 7:
        if (cy) {   // railgun
            A.circle(32, 32, 11, rgba(CYD, 110), 1, 0);
            A.cap(38, 28.6f, 63.5f, 28.6f, 1.6f, CYW, 1.4f, 0.6f); A.cap(38, 35.4f, 63.5f, 35.4f, 1.6f, CYW, 1.4f, 0.6f);
            A.line(40, 32, 63, 32, 1.4f, rgba(CYG, 220)); A.glow(50, 32, 16, rgba(CYG, 70));
            for (int i = 0; i < 6; i++) A.box(42 + i * 3.8f, 32, 0.9f, 5.6f, 0.3f, shade(CYP, 1.1f), 1, 0.2f);   // accelerator coils
            A.dot(63, 32, 2.2f, CYW); A.glow(63, 32, 6, rgba(CYW, 140));
            A.poly({ {17, 24}, {40, 25}, {43, 29}, {43, 35}, {40, 39}, {17, 40}, {14, 36}, {14, 28} }, shade(CYP, 1.0f), 2.8f, 0.65f);
            A.box(24, 32, 6.5f, 5.6f, 1.4f, shade(CYM, 1.3f), 2, 0.5f); A.circle(23.5f, 32, 2.6f, CYG, 1.4f, 0.3f); A.glow(23.5f, 32, 8, rgba(CYG, 100));
            A.box(37, 32, 2, 5, 0.7f, CYD, 1.2f, 0.3f);
            teamDot(A, 32.5f, 32, 2.3f);
        } else {    // rocket pod (raised, 6 tubes)
            A.box(31, 32, 20, 13, 1.6f, rgba(CKD, 90), 1, 0);       // pod shadow
            A.box(30, 32, 19.5f, 11.6f, 1.8f, shade(CKS, 0.95f), 2.4f, 0.6f);
            for (int j = 0; j < 4; j++) {
                float y = 25.3f + j * 4.5f;
                A.cap(14, y, 46, y, 1.9f, shade(CKD, 1.15f), 1.4f, 0.5f);
                A.dot(46.6f, y, 1.6f, CKR); A.dot(46.6f, y, 0.7f, CKY);       // warhead in the muzzle
            }
            A.box(30, 32, 0.9f, 12.5f, 0.3f, rgba(CKD, 190), 1, 0); A.box(20, 32, 0.9f, 12.5f, 0.3f, rgba(CKD, 190), 1, 0); A.box(40, 32, 0.9f, 12.5f, 0.3f, rgba(CKD, 190), 1, 0);
            A.box(13, 32, 3, 8, 1, shade(CKS, 0.8f), 1.6f, 0.5f);
            teamDot(A, 31.0f, 32, 2.6f);
            A.grain(11, 20, 50, 44, 0.16f, 60);
        }
        break;
    case 10:
        if (cy) {   // twin ion cannons
            A.circle(32, 32, 12.5f, rgba(CYD, 110), 1, 0);
            for (int s = -1; s <= 1; s += 2) {
                float y = 32 + s * 3.8f;
                A.cap(41, y, 63, y, 2.1f, shade(CYM, 1.4f), 1.4f, 0.55f);
                A.line(43, y, 62, y, 1.2f, rgba(CYG, 235));
                for (int i = 0; i < 4; i++) A.box(46 + i * 4.4f, y, 0.9f, 3.2f, 0.3f, shade(CYP, 1.1f), 1, 0.2f);
                A.circle(63.5f, y, 2.1f, CYW, 1.2f, 0.4f); A.glow(64, y, 6, rgba(CYG, 150));
            }
            A.poly({ {16, 22}, {37, 21.5f}, {46, 27.5f}, {46, 36.5f}, {37, 42.5f}, {16, 42}, {12.5f, 36}, {12.5f, 28} }, CYP, 3.0f, 0.65f);
            A.poly({ {19, 25}, {35, 24.5f}, {41, 29}, {41, 35}, {35, 39.5f}, {19, 39}, {17, 35}, {17, 29} }, shade(CYP, 1.14f), 2.2f, 0.5f);
            A.line(18, 25.4f, 34, 25, 1.0f, rgba(CYG, 200)); A.line(18, 38.6f, 34, 39, 1.0f, rgba(CYG, 200));
            hatch(A, 23.5f, 32, 3.7f, shade(CYH, 1.1f));
            A.circle(29.5f, 32, 3.0f, shade(CYM, 1.2f), 1.6f, 0.5f); A.dot(29.5f, 32, 1.3f, CYG); A.glow(29.5f, 32, 6, rgba(CYG, 130));
            teamDot(A, 36.0f, 32, 2.5f);
            A.line(15, 24, 9, 15, 0.8f, rgba(CYW, 220));
        } else {    // Behemoth: twin 150mm barrels
            A.circle(32, 32, 12.5f, rgba(CKD, 110), 1, 0);
            for (int s = -1; s <= 1; s += 2) {
                float y = 32 + s * 3.9f;
                A.cap(42, y, 64, y, 2.1f, shade(CKD, 1.5f), 1.6f, 0.6f);
                A.cap(49, y, 54, y, 2.9f, shade(CKD, 1.7f), 1.4f, 0.5f);
                A.box(62.5f, y, 2.1f, 3.0f, 0.7f, shade(CKD, 1.3f), 1.2f, 0.5f);
            }
            A.poly({ {14, 21.5f}, {38, 21}, {48, 27}, {48, 37}, {38, 43}, {14, 42.5f}, {10.5f, 37}, {10.5f, 27} }, shade(CKH, 1.1f), 3.2f, 0.65f);
            A.box(45.5f, 32, 3.6f, 8.2f, 1.4f, shade(CKH, 0.78f), 1.6f, 0.5f);                    // mantlet
            rivets(A, 15, 23.4f, 36, 23.4f, 7, shade(CKH, 1.4f)); rivets(A, 15, 40.6f, 36, 40.6f, 7, shade(CKH, 1.4f));
            hatch(A, 21.5f, 26.5f, 3.2f, shade(CKH, 1.05f)); hatch(A, 21.5f, 37.5f, 3.0f, shade(CKH, 1.0f));
            A.cap(22, 26.5f, 28, 26.5f, 0.9f, CKD, 1, 0.3f);
            A.box(15.5f, 32, 3.2f, 3.4f, 0.6f, shade(CKT, 0.85f), 1.4f, 0.5f);                    // stowage
            A.box(31, 32, 0.8f, 9.6f, 0.3f, rgba(CKD, 150), 1, 0);
            teamDot(A, 35.0f, 32, 2.7f);
            A.grain(11, 20, 50, 44, 0.16f, 14);
            A.line(13, 23, 7, 14, 0.8f, rgba(CKD, 230));
        }
        break;
    default: return false;
    }
    c.outline(rgb(8, 10, 14, 170));
    return true;
}

void artRotor(Canvas& c, bool blades) {
    setTone(-1);
    Art A(c, rgb(255, 255, 255));
    if (!blades) {
        for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
            float d = hyp(x + 0.5f - 32, y + 0.5f - 32) / 30.0f;
            if (d < 1) { float a = 34.0f * (0.35f + 0.65f * d * d); put(c, x, y, Color{90, 92, 96, (u8)a}); }
        }
        return;
    }
    A.cap(3, 32, 61, 32, 1.2f, rgb(36, 36, 38), 1, 0.3f);
    A.cap(32, 3, 32, 61, 1.2f, rgb(36, 36, 38), 1, 0.3f);
    A.dot(32, 32, 2.6f, rgb(150, 152, 156));
}

void artTurretHead(Canvas& c, int idx) {
    setTone(-1);
    Art A(c, rgb(255, 255, 255));
    switch (idx) {
    case 0:   // laser turret
        A.cap(38, 32, 63, 32, 2.4f, CYM, 1.6f, 0.6f);
        A.line(40, 32, 62, 32, 1.4f, rgba(CYG, 230)); A.circle(62, 32, 3.2f, CYW, 1.6f, 0.5f); A.glow(62, 32, 8, rgba(CYG, 170));
        A.circle(32, 32, 10.5f, CYP, 3, 0.65f); A.circle(32, 32, 6.5f, shade(CYM, 1.3f), 2, 0.5f); A.dot(32, 32, 2.4f, CYG); A.glow(32, 32, 9, rgba(CYG, 110));
        A.box(37, 32, 2.6f, 6, 0.8f, CYD, 1.4f, 0.4f);
        break;
    case 1:   // gun nest: twin MGs on a shielded mount
        for (int s = -1; s <= 1; s += 2) { A.cap(38, 32 + s * 3.4f, 60, 32 + s * 3.4f, 1.6f, shade(CKD, 1.7f), 1.4f, 0.55f); A.dot(60.5f, 32 + s * 3.4f, 1.8f, CKD); }
        A.box(42, 32, 2, 8.5f, 0.8f, shade(CKS, 0.9f), 1.6f, 0.55f);   // gun shield
        A.circle(30, 32, 10, CKS, 3, 0.65f); A.circle(30, 32, 6, shade(CKD, 1.6f), 2, 0.5f);
        A.box(26, 32 + 8, 4, 3, 0.8f, shade(CKT, 0.85f), 1.4f, 0.5f);
        rivets(A, 22, 25, 38, 25, 5, shade(CKS, 1.5f));
        break;
    case 2:   // patriot launcher box, angled up: pods pointing +x
        A.box(30, 32, 20, 13, 2.4f, rgb(206, 210, 214), 2.6f, 0.6f);
        for (int j = 0; j < 4; j++) { float y = 24.5f + j * 5; A.cap(14, y, 48, y, 2.2f, rgb(64, 66, 74), 1.6f, 0.5f); A.dot(48.6f, y, 1.8f, rgb(232, 232, 236)); A.dot(48.6f, y, 0.8f, rgb(190, 40, 40)); }
        A.box(11.5f, 32, 3, 10, 1, rgb(150, 152, 160), 1.6f, 0.5f);
        A.box(30, 32, 0.9f, 13, 0.3f, rgba(rgb(30, 30, 36), 190), 1, 0);
        break;
    case 3:   // rocket battery pod
        A.box(30, 32, 20, 13, 2.4f, CKS, 2.6f, 0.6f);
        for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) { A.circle(21 + i * 9.5f, 25 + j * 14, 3.6f, shade(CKD, 1.3f), 1.8f, 0.5f); A.dot(21 + i * 9.5f, 25 + j * 14, 1.7f, CKR); }
        A.box(52, 32, 3.4f, 12, 1.2f, shade(CKR, 0.9f), 1.8f, 0.55f);
        rivets(A, 12, 20, 12, 44, 5, shade(CKS, 1.5f));
        A.grain(10, 19, 50, 45, 0.14f, 3);
        break;
    }
    c.outline(rgb(8, 10, 14, 170));
}

float artScale(int type) {
    int local = type - firstUnitOf(UNITS[type].faction);
    if (local == 10) return 1.26f;   // Aegis Titan / Behemoth
    if (local == 9) return 1.06f;    // elite infantry
    return 1.0f;
}
