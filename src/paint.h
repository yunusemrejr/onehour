// One Hour - procedural painter shared by the unit, structure and terrain art.
// Shapes are signed distance fields: each pixel gets anti-aliased coverage and a bevel lit from the north-west, which is what gives
// every hull, roof and rock its machined, volumetric look. An Art works in "design pixels" and renders them at k canvas pixels each,
// so the same drawing code serves the 2x supersampled sprites and anything bigger.
#pragma once
#include "gfx.h"

namespace paint {

struct P { float x, y; };
inline float hyp(float x, float y) { return std::sqrt(x * x + y * y); }
inline Color rgba(Color c, int a) { c.a = (u8)a; return c; }

inline float sdBox(float px, float py, float cx, float cy, float hw, float hh, float r) {
    float qx = std::abs(px - cx) - (hw - r), qy = std::abs(py - cy) - (hh - r);
    return hyp(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - r;
}
inline float sdCap(float px, float py, float ax, float ay, float bx, float by, float r) {
    float pax = px - ax, pay = py - ay, bax = bx - ax, bay = by - ay;
    float h = clampf((pax * bax + pay * bay) / std::max(1e-6f, bax * bax + bay * bay), 0, 1);
    return hyp(pax - bax * h, pay - bay * h) - r;
}
inline float sdPoly(const std::vector<P>& v, float px, float py, float sgn) {
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
inline void put(Canvas& c, int x, int y, Color s) {
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
    float k = 1;            // canvas pixels per design pixel
    float ox = 0, oy = 0;   // design-space origin of the drawing (structures: padding around the footprint)
    Art(Canvas& cv, Color t, float scale = 1, float offX = 0, float offY = 0) : c(cv), team(t), k(scale), ox(offX), oy(offY) {}
    float X(float x) const { return (x + ox) * k; }
    float Y(float y) const { return (y + oy) * k; }

    template <class F> void shape(int x0, int y0, int x1, int y1, F sdf, Color base, float bevel, float kk, float alpha) {
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(c.w - 1, x1); y1 = std::min(c.h - 1, y1);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float px = x + 0.5f, py = y + 0.5f, d = sdf(px, py);
            if (d > 0.75f) continue;
            float cov = clampf(0.5f - d, 0, 1) * alpha;
            if (cov <= 0) continue;
            float f = 1.0f;
            if (kk != 0) {
                float gx = sdf(px + 0.6f, py) - sdf(px - 0.6f, py), gy = sdf(px, py + 0.6f) - sdf(px, py - 0.6f), gl = hyp(gx, gy);
                float lit = gl > 1e-4f ? (gx * -0.62f - gy * 0.78f) / gl : 0;
                float w = clampf((bevel + d) / bevel, 0, 1);
                f = 1.0f + kk * lit * w;
            }
            Color col = shade(base, f); col.a = (u8)(base.a * cov);
            put(c, x, y, col);
        }
    }
    // feathered shape (shadows, glows, soft stains): coverage ramps over `feather` canvas pixels instead of one
    template <class F> void soft(int x0, int y0, int x1, int y1, F sdf, Color col, float feather) {
        x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(c.w - 1, x1); y1 = std::min(c.h - 1, y1);
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float d = sdf(x + 0.5f, y + 0.5f);
            if (d >= feather * 0.5f) continue;
            float cov = clampf(0.5f - d / feather, 0, 1);
            cov = cov * cov * (3 - 2 * cov);
            Color s = col; s.a = (u8)(col.a * cov);
            put(c, x, y, s);
        }
    }

    void circle(float cx, float cy, float r, Color base, float bevel = 2.4f, float kk = 0.5f, float a = 1) {
        cx = X(cx); cy = Y(cy); r *= k; bevel *= k;
        shape((int)(cx - r - 2), (int)(cy - r - 2), (int)(cx + r + 2), (int)(cy + r + 2), [=](float x, float y) { return hyp(x - cx, y - cy) - r; }, base, bevel, kk, a);
    }
    void ell(float cx, float cy, float rx, float ry, Color base, float bevel = 2.4f, float kk = 0.5f, float a = 1) {
        cx = X(cx); cy = Y(cy); rx *= k; ry *= k; bevel *= k;
        float m = std::min(rx, ry);
        shape((int)(cx - rx - 2), (int)(cy - ry - 2), (int)(cx + rx + 2), (int)(cy + ry + 2), [=](float x, float y) { return (hyp((x - cx) / rx, (y - cy) / ry) - 1) * m; }, base, bevel, kk, a);
    }
    void box(float cx, float cy, float hw, float hh, float r, Color base, float bevel = 2.2f, float kk = 0.5f, float a = 1) {
        r = std::min(r, std::min(hw, hh));
        cx = X(cx); cy = Y(cy); hw *= k; hh *= k; r *= k; bevel *= k;
        shape((int)(cx - hw - 2), (int)(cy - hh - 2), (int)(cx + hw + 2), (int)(cy + hh + 2), [=](float x, float y) { return sdBox(x, y, cx, cy, hw, hh, r); }, base, bevel, kk, a);
    }
    // box from its top-left corner and size (structures think in rectangles)
    void rect(float x, float y, float w, float h, float r, Color base, float bevel = 2.0f, float kk = 0.5f, float a = 1) {
        box(x + w * 0.5f, y + h * 0.5f, w * 0.5f, h * 0.5f, r, base, bevel, kk, a);
    }
    void cap(float ax, float ay, float bx, float by, float r, Color base, float bevel = 1.6f, float kk = 0.5f, float a = 1) {
        ax = X(ax); ay = Y(ay); bx = X(bx); by = Y(by); r *= k; bevel *= k;
        shape((int)(std::min(ax, bx) - r - 2), (int)(std::min(ay, by) - r - 2), (int)(std::max(ax, bx) + r + 2), (int)(std::max(ay, by) + r + 2), [=](float x, float y) { return sdCap(x, y, ax, ay, bx, by, r); }, base, bevel, kk, a);
    }
    void poly(std::vector<P> v, Color base, float bevel = 2.4f, float kk = 0.5f, float a = 1) {
        bevel *= k;
        float area = 0, mnx = 1e9f, mny = 1e9f, mxx = -1e9f, mxy = -1e9f;
        for (auto& p : v) { p.x = X(p.x); p.y = Y(p.y); }
        for (size_t i = 0; i < v.size(); i++) { const P& p = v[i]; const P& q = v[(i + 1) % v.size()]; area += p.x * q.y - q.x * p.y; mnx = std::min(mnx, p.x); mny = std::min(mny, p.y); mxx = std::max(mxx, p.x); mxy = std::max(mxy, p.y); }
        float sgn = area > 0 ? 1.0f : -1.0f;
        shape((int)mnx - 2, (int)mny - 2, (int)mxx + 2, (int)mxy + 2, [&v, sgn](float x, float y) { return sdPoly(v, x, y, sgn); }, base, bevel, kk, a);
    }
    // regular polygon (hex bolts, hex pads, octagonal roofs)
    void ngon(float cx, float cy, float r, int n, float rot, Color base, float bevel = 2.0f, float kk = 0.5f, float a = 1) {
        std::vector<P> v;
        for (int i = 0; i < n; i++) { float t = rot + i * 6.2831853f / n; v.push_back({cx + std::cos(t) * r, cy + std::sin(t) * r}); }
        poly(v, base, bevel, kk, a);
    }
    // annulus: pipes, rings, hatches, rotor housings
    void ring(float cx, float cy, float r, float thick, Color base, float bevel = 1.2f, float kk = 0.4f, float a = 1) {
        cx = X(cx); cy = Y(cy); r *= k; thick *= k * 0.5f; bevel *= k;
        shape((int)(cx - r - thick - 2), (int)(cy - r - thick - 2), (int)(cx + r + thick + 2), (int)(cy + r + thick + 2), [=](float x, float y) { return std::abs(hyp(x - cx, y - cy) - r) - thick; }, base, bevel, kk, a);
    }
    // flat anti-aliased strokes and dots (no lighting)
    void line(float ax, float ay, float bx, float by, float w, Color col) { cap(ax, ay, bx, by, w * 0.5f, col, 1, 0, 1); }
    void dot(float cx, float cy, float r, Color col) { circle(cx, cy, r, col, 1, 0, 1); }
    void glow(float cx, float cy, float r, Color col) {
        cx = X(cx); cy = Y(cy); r *= k;
        for (int y = (int)(cy - r - 1); y <= (int)(cy + r + 1); y++) for (int x = (int)(cx - r - 1); x <= (int)(cx + r + 1); x++) {
            float d = hyp(x + 0.5f - cx, y + 0.5f - cy) / r;
            if (d >= 1) continue;
            Color q = col; q.a = (u8)(col.a * (1 - d) * (1 - d));
            put(c, x, y, q);
        }
    }
    // soft drop shadow of a rectangle (design pixels), offset toward the south-east (light from the north-west)
    void shadowRect(float x, float y, float w, float h, float dx, float dy, float feather, int alpha = 90) {
        float cx = X(x + w * 0.5f + dx), cy = Y(y + h * 0.5f + dy), hw = w * 0.5f * k, hh = h * 0.5f * k, f = feather * k;
        soft((int)(cx - hw - f), (int)(cy - hh - f), (int)(cx + hw + f), (int)(cy + hh + f), [=](float px, float py) { return sdBox(px, py, cx, cy, hw, hh, std::min(hw, hh) * 0.2f); }, Color{0, 0, 0, (u8)alpha}, f);
    }
    void shadowCircle(float cx, float cy, float r, float dx, float dy, float feather, int alpha = 90) {
        float X0 = X(cx + dx), Y0 = Y(cy + dy), rr = r * k, f = feather * k;
        soft((int)(X0 - rr - f), (int)(Y0 - rr - f), (int)(X0 + rr + f), (int)(Y0 + rr + f), [=](float px, float py) { return hyp(px - X0, py - Y0) - rr; }, Color{0, 0, 0, (u8)alpha}, f);
    }
    // dirt, wear and rust: modulate what is already painted (canvas pixel rectangle, scaled from design space)
    void grain(float x0, float y0, float x1, float y1, float amount, u32 seed) {
        Rng r(seed);
        int ax = std::max(0, (int)X(x0)), ay = std::max(0, (int)Y(y0)), bx = std::min(c.w - 1, (int)X(x1)), by = std::min(c.h - 1, (int)Y(y1));
        for (int y = ay; y <= by; y++) for (int x = ax; x <= bx; x++) {
            Color p = Canvas::unpack(c.px[y * c.w + x]);
            if (p.a < 200) continue;
            float f = 1.0f + (r.f() - 0.5f) * amount;
            c.px[y * c.w + x] = Canvas::pack(shade(p, f));
        }
    }
    void darken(float x0, float y0, float x1, float y1, float f) {
        int ax = std::max(0, (int)X(x0)), ay = std::max(0, (int)Y(y0)), bx = std::min(c.w - 1, (int)X(x1)), by = std::min(c.h - 1, (int)Y(y1));
        for (int y = ay; y <= by; y++) for (int x = ax; x <= bx; x++) {
            Color p = Canvas::unpack(c.px[y * c.w + x]);
            if (p.a < 200) continue;
            c.px[y * c.w + x] = Canvas::pack(shade(p, f));
        }
    }
    // vertical light ramp over what is already painted: f0 at the top edge, f1 at the bottom edge (walls fade into shade toward the ground)
    void ramp(float x0, float y0, float x1, float y1, float f0, float f1) {
        int ax = std::max(0, (int)X(x0)), ay = std::max(0, (int)Y(y0)), bx = std::min(c.w - 1, (int)X(x1)), by = std::min(c.h - 1, (int)Y(y1));
        for (int y = ay; y <= by; y++) {
            float t = by > ay ? (float)(y - ay) / (by - ay) : 0, f = f0 + (f1 - f0) * t;
            for (int x = ax; x <= bx; x++) {
                Color p = Canvas::unpack(c.px[y * c.w + x]);
                if (p.a < 200) continue;
                c.px[y * c.w + x] = Canvas::pack(shade(p, f));
            }
        }
    }
    Color tm(float f = 1.0f) const { return shade(team, f); }
};

}  // namespace paint
