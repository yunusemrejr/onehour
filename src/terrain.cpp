#include "terrain.h"
#include "paint.h"
#include <SDL2/SDL.h>
#include <functional>

using namespace paint;

// The ground is baked once. Terrain classes are smoothed into coverage fields (so borders become curves), the fields are displaced by
// low-frequency noise and sharpened (so borders turn ragged and organic), each class is shaded procedurally per pixel, and a coarse
// height field (cliffs, lake beds, tree canopies) lights everything from the north-west and casts soft shadows to the south-east.
// Trees, boulders, grass, flowers, pebbles, reeds and road markings are then painted on top. It all runs on worker threads.
namespace {

enum { K_GRASS = 0, K_GRASS2, K_DIRT, K_SAND, K_ROAD, K_WATER, K_ROCK, K_FOREST, K_N };

struct Noise {
    float g[256 * 256];
    Noise() { Rng r(0xB16B00B5ull); for (auto& v : g) v = r.f(); }
    inline float at(int x, int y) const { return g[((y & 255) << 8) | (x & 255)]; }
    inline float v(float x, float y) const {
        int ix = (int)std::floor(x), iy = (int)std::floor(y);
        float fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
        float a = at(ix, iy), b = at(ix + 1, iy), c = at(ix, iy + 1), d = at(ix + 1, iy + 1);
        return a + (b - a) * fx + (c - a) * fy + (a - b - c + d) * fx * fy;
    }
    inline float fbm(float x, float y) const {
        return (v(x, y) * 0.5f + v(x * 2.03f + 17, y * 2.03f + 31) * 0.25f + v(x * 4.1f + 53, y * 4.1f + 7) * 0.125f + v(x * 8.3f + 11, y * 8.3f + 91) * 0.0625f) / 0.9375f;
    }
};
const Noise& NZ() { static Noise n; return n; }

struct F3 { float r, g, b; };
inline F3 f3(int r, int g, int b) { return F3{(float)r, (float)g, (float)b}; }
inline F3 lerp3(F3 a, F3 b, float t) { return F3{a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }
inline F3 mul3(F3 a, float k) { return F3{a.r * k, a.g * k, a.b * k}; }
inline F3 add3(F3 a, F3 b) { return F3{a.r + b.r, a.g + b.g, a.b + b.b}; }
inline float sstep(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }

void parallelRows(int rows, const std::function<void(int, int)>& f) {
    int n = clampi(SDL_GetCPUCount(), 1, 16);
    struct Ctx { const std::function<void(int, int)>* f; int y0, y1; };
    std::vector<Ctx> ctx(n);
    std::vector<SDL_Thread*> th(n, nullptr);
    for (int i = 0; i < n; i++) {
        ctx[i] = Ctx{ &f, rows * i / n, rows * (i + 1) / n };
        if (i == n - 1) { f(ctx[i].y0, ctx[i].y1); continue; }
        th[i] = SDL_CreateThread([](void* p) -> int { Ctx* c = (Ctx*)p; (*c->f)(c->y0, c->y1); return 0; }, "terrain", &ctx[i]);
        if (!th[i]) f(ctx[i].y0, ctx[i].y1);
    }
    for (auto t : th) if (t) SDL_WaitThread(t, nullptr);
}

void blurBox(std::vector<float>& a, int w, int h, int r, int iters) {
    std::vector<float> tmp(a.size());
    float inv = 1.0f / (2 * r + 1);
    for (int it = 0; it < iters; it++) {
        for (int y = 0; y < h; y++) {
            const float* row = &a[(size_t)y * w]; float* out = &tmp[(size_t)y * w];
            float sum = 0;
            for (int i = -r; i <= r; i++) sum += row[clampi(i, 0, w - 1)];
            for (int x = 0; x < w; x++) { out[x] = sum * inv; sum += row[clampi(x + r + 1, 0, w - 1)] - row[clampi(x - r, 0, w - 1)]; }
        }
        for (int x = 0; x < w; x++) {
            float sum = 0;
            for (int i = -r; i <= r; i++) sum += tmp[(size_t)clampi(i, 0, h - 1) * w + x];
            for (int y = 0; y < h; y++) { a[(size_t)y * w + x] = sum * inv; sum += tmp[(size_t)clampi(y + r + 1, 0, h - 1) * w + x] - tmp[(size_t)clampi(y - r, 0, h - 1) * w + x]; }
        }
    }
}

inline float samp(const std::vector<float>& m, int w, int h, float fx, float fy) {
    int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy);
    float tx = fx - x0, ty = fy - y0;
    int x1 = clampi(x0 + 1, 0, w - 1), y1 = clampi(y0 + 1, 0, h - 1);
    x0 = clampi(x0, 0, w - 1); y0 = clampi(y0, 0, h - 1);
    float a = m[(size_t)y0 * w + x0], b = m[(size_t)y0 * w + x1], c = m[(size_t)y1 * w + x0], d = m[(size_t)y1 * w + x1];
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
}

// ---------------------------------------------------------------- ground colours
const F3 GRASS_A = f3(76, 110, 50), GRASS_B = f3(98, 130, 60), GRASS2_A = f3(62, 96, 46), GRASS2_B = f3(84, 114, 54), DRY = f3(132, 134, 70);
const F3 DIRT_A = f3(132, 106, 72), DIRT_B = f3(110, 88, 60), DIRT_DARK = f3(88, 68, 46);
const F3 SAND_A = f3(204, 188, 142), SAND_B = f3(184, 166, 120), SAND_WET = f3(150, 134, 100);
const F3 ASPHALT = f3(72, 72, 74), GRAVEL = f3(132, 124, 108);
const F3 FOREST_A = f3(52, 78, 40), FOREST_B = f3(80, 66, 42);
const F3 ROCK_A = f3(118, 110, 98), ROCK_B = f3(92, 86, 78), MOSS = f3(78, 98, 54);
const F3 WATER_SHALLOW = f3(78, 158, 168), WATER_DEEP = f3(18, 58, 104);

F3 groundColor(int c, float x, float y, const Noise& nz, float macro, float depth) {
    float n1 = nz.v(x * 0.045f, y * 0.045f), n2 = nz.v(x * 0.17f + 5, y * 0.17f + 9), n3 = nz.v(x * 0.9f + y * 0.3f, y * 1.3f);
    float n4 = nz.v(x * 0.55f + 71, y * 0.55f + 13);
    switch (c) {
    case K_GRASS: case K_GRASS2: {
        bool two = c == K_GRASS2;
        F3 col = lerp3(two ? GRASS2_A : GRASS_A, two ? GRASS2_B : GRASS_B, n1);
        col = mul3(col, 0.90f + 0.20f * n2);
        col = mul3(col, 0.94f + 0.12f * n3);
        float dry = sstep(0.60f, 0.85f, macro);
        col = lerp3(col, DRY, dry * 0.5f * n2);
        float blade = nz.v(x * 1.6f, y * 0.55f);          // fine upright strokes
        col = mul3(col, 0.95f + 0.10f * blade);
        if (n4 > 0.94f) col = mul3(col, 1.12f);           // sunlit tips
        return col;
    }
    case K_DIRT: {
        F3 col = lerp3(DIRT_A, DIRT_B, n1);
        col = lerp3(col, DIRT_DARK, sstep(0.55f, 0.8f, nz.v(x * 0.08f + 33, y * 0.08f + 7)) * 0.55f);
        col = mul3(col, 0.88f + 0.24f * n2);
        col = mul3(col, 0.93f + 0.14f * n3);
        float crack = std::abs(nz.v(x * 0.05f + 90, y * 0.05f + 40) - 0.5f);
        if (crack < 0.007f) col = mul3(col, 0.84f);
        if (n4 > 0.92f) col = mul3(col, 0.9f);            // pebbled specks
        return col;
    }
    case K_SAND: {
        float ripple = std::sin((x * 0.55f + y * 0.22f + 7.0f * nz.fbm(x * 0.012f, y * 0.012f)) * 0.5f);
        F3 col = lerp3(SAND_A, SAND_B, 0.5f + 0.35f * ripple + 0.25f * (n2 - 0.5f));
        col = mul3(col, 0.95f + 0.08f * n3);
        if (n4 > 0.93f) col = mul3(col, 0.9f);
        return col;
    }
    case K_ROAD: {
        F3 col = mul3(ASPHALT, 0.92f + 0.16f * n3);
        col = mul3(col, 0.94f + 0.12f * n1);
        float crack = std::abs(nz.v(x * 0.06f + 12, y * 0.06f + 77) - 0.5f);
        if (crack < 0.007f) col = mul3(col, 0.58f);
        float patch = sstep(0.66f, 0.74f, nz.v(x * 0.03f + 5, y * 0.03f + 61));
        col = lerp3(col, f3(80, 80, 82), patch * 0.22f);
        float tyre = std::abs(nz.v(x * 0.012f + y * 0.004f + 20, y * 0.012f) - 0.5f);   // faint wheel tracks
        if (tyre < 0.02f) col = mul3(col, 0.93f);
        if (n4 > 0.95f) col = add3(col, f3(18, 18, 18));
        return col;
    }
    case K_FOREST: {
        F3 col = lerp3(FOREST_A, FOREST_B, n2);
        col = mul3(col, 0.86f + 0.2f * n1);
        col = mul3(col, 0.92f + 0.14f * n3);
        return col;
    }
    case K_ROCK: {
        float strata = std::sin((y * 0.85f + x * 0.28f + 9.0f * nz.fbm(x * 0.02f, y * 0.02f)) * 0.55f);
        F3 col = lerp3(ROCK_A, ROCK_B, n1 * 0.8f + 0.2f * (0.5f + 0.5f * strata));
        col = mul3(col, 0.86f + 0.2f * n2);
        col = mul3(col, 0.93f + 0.14f * n3);
        float crack = std::abs(nz.v(x * 0.09f + 3, y * 0.09f + 55) - 0.5f);
        if (crack < 0.014f) col = mul3(col, 0.62f);
        return col;
    }
    case K_WATER: {
        float d = depth;
        F3 col = lerp3(WATER_SHALLOW, WATER_DEEP, d);
        float wave = std::sin((x * 0.09f + 5.0f * nz.fbm(x * 0.01f, y * 0.01f)) + y * 0.05f);
        col = mul3(col, 0.95f + 0.06f * wave + 0.06f * (n2 - 0.5f));
        float c1 = 1.0f - std::abs(2.0f * nz.v(x * 0.07f + 3, y * 0.07f + 11) - 1.0f), c2 = 1.0f - std::abs(2.0f * nz.v(x * 0.05f + 40 + y * 0.02f, y * 0.05f + 7) - 1.0f);
        float caus = std::pow(c1 * c2, 5.0f);                                        // caustic-like light networks in the shallows
        col = add3(col, mul3(f3(70, 96, 84), (1.0f - d * 0.8f) * clampf(caus * 2.2f, 0, 1)));
        if (nz.v(x * 0.9f + 13, y * 0.9f + 7) > 0.975f) col = add3(col, f3(40, 48, 44));   // sparkle
        return col;
    }
    }
    return f3(128, 128, 128);
}

// ---------------------------------------------------------------- trees and props
Canvas downsample2(const Canvas& big) {
    Canvas out(big.w / 2, big.h / 2);
    for (int y = 0; y < out.h; y++) for (int x = 0; x < out.w; x++) {
        float r = 0, g = 0, b = 0, a = 0;
        for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) {
            Color c = Canvas::unpack(big.px[(2 * y + j) * big.w + 2 * x + i]);
            float al = c.a / 255.0f; r += c.r * al; g += c.g * al; b += c.b * al; a += al;
        }
        if (a > 0) out.px[y * out.w + x] = Canvas::pack(Color{(u8)(r / a), (u8)(g / a), (u8)(b / a), (u8)(a * 0.25f * 255)});
    }
    return out;
}

void blitOver(Canvas& dst, const Canvas& src, int x0, int y0) {
    for (int y = 0; y < src.h; y++) {
        int dy = y0 + y; if (dy < 0 || dy >= dst.h) continue;
        for (int x = 0; x < src.w; x++) {
            int dx = x0 + x; if (dx < 0 || dx >= dst.w) continue;
            Color s = Canvas::unpack(src.px[y * src.w + x]);
            if (s.a == 0) continue;
            u32& d = dst.px[dy * dst.w + dx];
            if (s.a == 255) { d = Canvas::pack(s); continue; }
            Color o = Canvas::unpack(d); float a = s.a / 255.0f;
            d = Canvas::pack(Color{(u8)(o.r + (s.r - o.r) * a), (u8)(o.g + (s.g - o.g) * a), (u8)(o.b + (s.b - o.b) * a), 255});
        }
    }
}

enum { T_BROAD = 0, T_CONIFER, T_AUTUMN, T_BUSH, T_KINDS };

Canvas makeTree(int kind, float R, u32 seed) {
    int S = (int)(R * 2 + 12);
    Canvas big(S * 2, S * 2);
    Art a(big, rgb(255, 255, 255), 2.0f);
    Rng r(seed);
    float cx = S * 0.5f, cy = S * 0.5f;
    Color deep, mid, hi;
    switch (kind) {
    case T_CONIFER: deep = rgb(22, 58, 44); mid = rgb(34, 82, 58); hi = rgb(70, 120, 84); break;
    case T_AUTUMN:  deep = rgb(112, 62, 26); mid = rgb(172, 104, 36); hi = rgb(224, 164, 62); break;
    case T_BUSH:    deep = rgb(46, 86, 40); mid = rgb(74, 120, 54); hi = rgb(118, 160, 80); break;
    default:        deep = rgb(28, 70, 36); mid = rgb(48, 100, 48); hi = rgb(96, 152, 66); break;
    }
    a.shadowCircle(cx, cy, R * 0.85f, R * 0.22f, R * 0.28f, R * 0.4f, 90);
    if (kind == T_CONIFER) {
        for (int layer = 0; layer < 3; layer++) {
            float rad = R * (1.0f - 0.24f * layer), off = -0.07f * R * layer;
            std::vector<P> v;
            int n = 12;
            for (int i = 0; i < n * 2; i++) { float an = i * 3.14159265f / n + layer * 0.26f + r.f(-0.05f, 0.05f); float rr = (i & 1) ? rad * 0.6f : rad * r.f(0.92f, 1.0f); v.push_back({cx + off + std::cos(an) * rr, cy + off + std::sin(an) * rr}); }
            a.poly(v, layer == 0 ? deep : (layer == 1 ? mid : hi), rad * 0.45f, 0.62f);
        }
        a.dot(cx - R * 0.14f, cy - R * 0.16f, R * 0.1f, shade(hi, 1.25f));
        for (int i = 0; i < (int)(R * 1.5f); i++) { float an = r.f(0, 6.28f), d = r.f(0.1f, 0.9f) * R; a.line(cx + std::cos(an) * d * 0.4f, cy + std::sin(an) * d * 0.4f, cx + std::cos(an) * d, cy + std::sin(an) * d, 0.5f, rgba(shade(deep, 0.7f), 120)); }
    } else {
        int n = kind == T_BUSH ? 5 : 9;
        for (int i = 0; i < n; i++) {                       // shaded under-canopy ring
            float an = i * 2.4f + r.f(0, 1), dist = R * (0.38f + 0.24f * (float)((i * 5) % 7) / 7.0f), rr = R * r.f(0.40f, 0.52f);
            a.circle(cx + std::cos(an) * dist, cy + std::sin(an) * dist, rr, deep, rr * 0.9f, 0.58f);
        }
        for (int i = 0; i < (kind == T_BUSH ? 4 : 9); i++) {   // main leaf clusters, nudged toward the light
            float an = i * 2.1f + r.f(0, 1), dist = R * 0.46f * r.f(0.1f, 1.0f), rr = R * r.f(0.28f, 0.4f);
            a.circle(cx - R * 0.09f + std::cos(an) * dist, cy - R * 0.11f + std::sin(an) * dist, rr, mid, rr * 0.9f, 0.6f);
        }
        for (int i = 0; i < (kind == T_BUSH ? 3 : 6); i++) {   // sunlit tops
            float rr = R * r.f(0.16f, 0.26f);
            a.circle(cx - R * 0.24f + r.f(-0.22f, 0.22f) * R, cy - R * 0.28f + r.f(-0.22f, 0.22f) * R, rr, hi, rr * 0.9f, 0.62f);
        }
        for (int i = 0; i < (int)(R * 3.2f); i++) {
            float an = r.f(0, 6.28f), d = std::sqrt(r.f()) * R * 0.86f;
            Color cc = r.f() < 0.5f ? shade(hi, 1.1f) : shade(deep, 0.8f); cc.a = 190;
            a.dot(cx + std::cos(an) * d, cy + std::sin(an) * d, 0.75f, cc);
        }
        if (kind == T_BUSH) for (int i = 0; i < 4; i++) a.dot(cx + r.f(-0.6f, 0.6f) * R, cy + r.f(-0.6f, 0.6f) * R, 0.9f, rgb(200, 60, 56));
    }
    return downsample2(big);
}

Canvas makeBoulder(float R, u32 seed) {
    int S = (int)(R * 2 + 10);
    Canvas big(S * 2, S * 2);
    Art a(big, rgb(255, 255, 255), 2.0f);
    Rng r(seed);
    float cx = S * 0.5f, cy = S * 0.5f;
    a.shadowCircle(cx, cy, R * 0.9f, R * 0.35f, R * 0.4f, R * 0.4f, 100);
    std::vector<P> v;
    int n = 8;
    for (int i = 0; i < n; i++) { float an = i * 6.2831853f / n + r.f(-0.2f, 0.2f), rr = R * r.f(0.78f, 1.0f); v.push_back({cx + std::cos(an) * rr, cy + std::sin(an) * rr * 0.86f}); }
    Color base = shade(rgb(112, 106, 96), r.f(0.85f, 1.1f));
    a.poly(v, base, R * 0.8f, 0.7f);
    std::vector<P> top;
    for (auto& p : v) top.push_back({cx + (p.x - cx) * 0.62f - R * 0.1f, cy + (p.y - cy) * 0.62f - R * 0.12f});
    a.poly(top, shade(base, 1.14f), R * 0.5f, 0.55f);
    a.line(cx - R * 0.2f, cy + R * 0.1f, cx + R * 0.3f, cy + R * 0.5f, 0.5f, rgba(rgb(40, 38, 34), 120));
    if (r.f() < 0.5f) a.ell(cx + R * 0.3f, cy + R * 0.35f, R * 0.35f, R * 0.18f, rgba(rgb(78, 100, 56), 130), 1, 0);   // moss on the shaded side
    return downsample2(big);
}

struct Tree { float x, y, r; int kind; u32 seed; };

}  // namespace

// ================================================================= bake
void terrainBake(Canvas& world) {
    const int W = WORLD_W, H = WORLD_H;
    const Noise& nz = NZ();
    const int CW = MAP_W * 4, CH = MAP_H * 4, CELL = TILE / 4;   // coverage cells of 8 px
    const int HW = W / 2, HH = H / 2;                             // height field at half resolution

    // ---- class coverage fields
    std::vector<float> mask[K_N];
    for (auto& m : mask) m.assign((size_t)CW * CH, 0.0f);
    auto classOf = [&](int tx, int ty) -> int {
        switch (g_map.tile(clampi(tx, 0, MAP_W - 1), clampi(ty, 0, MAP_H - 1))) {
        case T_GRASS: return K_GRASS; case T_GRASS2: return K_GRASS2; case T_DIRT: return K_DIRT; case T_SAND: return K_SAND;
        case T_ROAD: return K_ROAD; case T_WATER: return K_WATER; case T_ROCK: return K_ROCK; default: return K_FOREST;
        }
    };
    for (int cy = 0; cy < CH; cy++) for (int cx = 0; cx < CW; cx++) mask[classOf(cx / 4, cy / 4)][(size_t)cy * CW + cx] = 1.0f;
    std::vector<float> wide = mask[K_WATER];
    for (auto& m : mask) blurBox(m, CW, CH, 4, 2);
    blurBox(wide, CW, CH, 13, 2);
    // tiles whose coverage can reach them (dilated by one tile so the border displacement never samples an unlisted class)
    std::vector<u8> present((size_t)MAP_W * MAP_H, 0), presentRaw((size_t)MAP_W * MAP_H, 0);
    for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
        u8 bits = 0;
        for (int c = 0; c < K_N; c++) {
            bool any = false;
            for (int j = -1; j < 5 && !any; j++) for (int i = -1; i < 5; i++) { int cx = clampi(tx * 4 + i, 0, CW - 1), cy = clampi(ty * 4 + j, 0, CH - 1); if (mask[c][(size_t)cy * CW + cx] > 0.004f) { any = true; break; } }
            if (any) bits |= (u8)(1 << c);
        }
        presentRaw[(size_t)ty * MAP_W + tx] = bits;
    }
    for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
        u8 bits = 0;
        for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) bits |= presentRaw[(size_t)clampi(ty + j, 0, MAP_H - 1) * MAP_W + clampi(tx + i, 0, MAP_W - 1)];
        present[(size_t)ty * MAP_W + tx] = bits;
    }

    // ---- domain warp on an 8 px lattice
    const int WG = W / CELL + 2;
    std::vector<float> warpX((size_t)WG * WG), warpY((size_t)WG * WG);
    for (int j = 0; j < WG; j++) for (int i = 0; i < WG; i++) {
        float x = (float)(i * CELL), y = (float)(j * CELL);
        warpX[(size_t)j * WG + i] = (nz.fbm(x * 0.02f, y * 0.02f) - 0.5f) * 38.0f;
        warpY[(size_t)j * WG + i] = (nz.fbm(x * 0.02f + 40, y * 0.02f + 40) - 0.5f) * 38.0f;
    }
    auto warpAt = [&](float x, float y, float& wx, float& wy) {
        float fx = x / CELL, fy = y / CELL; int ix = (int)fx, iy = (int)fy; float tx = fx - ix, ty = fy - iy;
        const float* A = &warpX[(size_t)iy * WG + ix]; const float* B = &warpY[(size_t)iy * WG + ix];
        wx = A[0] + (A[1] - A[0]) * tx + (A[WG] - A[0]) * ty + (A[0] - A[1] - A[WG] + A[WG + 1]) * tx * ty;
        wy = B[0] + (B[1] - B[0]) * tx + (B[WG] - B[0]) * ty + (B[0] - B[1] - B[WG] + B[WG + 1]) * tx * ty;
    };

    // ---- trees (placed first: their canopies join the height field and so shade the ground around them)
    std::vector<Tree> trees;
    {
        Rng r(777);
        for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            if (g_map.tile(tx, ty) != T_TREE) continue;
            int cnt = 1 + (r.next() % 3 == 0 ? 1 : 0);
            for (int k = 0; k < cnt; k++) {
                Tree t;
                t.x = tx * TILE + r.f(5, TILE - 5); t.y = ty * TILE + r.f(5, TILE - 5);
                float zone = nz.fbm(tx * 0.09f + 3, ty * 0.09f + 8);
                float u = r.f();
                t.kind = zone > 0.55f ? (u < 0.8f ? T_CONIFER : T_BROAD) : (u < 0.06f ? T_AUTUMN : T_BROAD);
                t.r = (t.kind == T_CONIFER ? r.f(13, 17) : r.f(14, 19));
                t.seed = r.next();
                trees.push_back(t);
            }
        }
        // bushes on open ground
        for (int ty = 2; ty < MAP_H - 2; ty++) for (int tx = 2; tx < MAP_W - 2; tx++) {
            int tl = g_map.tile(tx, ty);
            if (tl != T_GRASS && tl != T_GRASS2 && tl != T_DIRT) continue;
            if (nz.fbm(tx * 0.21f + 70, ty * 0.21f + 20) < 0.62f || r.f() > 0.42f) continue;
            bool near = false;
            for (auto& s : g_map.starts) if (std::abs(tx - s.tx) < 8 && std::abs(ty - s.ty) < 8) near = true;
            for (auto& s : g_map.supplies) if (std::abs(tx - s.tx) < 3 && std::abs(ty - s.ty) < 3) near = true;
            if (near) continue;
            Tree t; t.x = tx * TILE + r.f(4, TILE - 4); t.y = ty * TILE + r.f(4, TILE - 4); t.kind = T_BUSH; t.r = r.f(7, 10); t.seed = r.next();
            trees.push_back(t);
        }
        std::sort(trees.begin(), trees.end(), [](const Tree& a, const Tree& b) { return a.y < b.y; });
    }

    // ---- height field (half resolution): rock massifs, lake beds, canopies
    std::vector<float> hmap((size_t)HW * HH, 0.0f);
    parallelRows(HH, [&](int y0, int y1) {
        for (int hy = y0; hy < y1; hy++) for (int hx = 0; hx < HW; hx++) {
            float x = hx * 2.0f, y = hy * 2.0f, wx, wy; warpAt(x, y, wx, wy);
            float fx = (x + wx) / CELL - 0.5f, fy = (y + wy) / CELL - 0.5f;
            int tx = clampi((int)x >> 5, 0, MAP_W - 1), ty = clampi((int)y >> 5, 0, MAP_H - 1);
            u8 bits = present[(size_t)ty * MAP_W + tx];
            float h = 0;
            if (bits & (1 << K_ROCK)) {
                float mr = samp(mask[K_ROCK], CW, CH, fx, fy);
                float e = (nz.v(x * 0.13f + 7 * 31.7f, y * 0.13f + 7 * 17.3f) - 0.5f) * 0.28f;
                float a = sstep(0.38f, 0.74f, mr + e);
                float ridge = 1.0f - std::abs(2.0f * nz.fbm(x * 0.045f + 11, y * 0.045f + 3) - 1.0f);
                h += a * (14.0f + 22.0f * ridge * ridge + 7.0f * nz.v(x * 0.11f, y * 0.11f));
            }
            if (bits & (1 << K_WATER)) h -= 3.5f * sstep(0.5f, 0.95f, samp(wide, CW, CH, fx, fy));
            hmap[(size_t)hy * HW + hx] = h;
        }
    });
    for (auto& t : trees) {
        float hgt = t.kind == T_BUSH ? 7.0f : (t.kind == T_CONIFER ? 24.0f : 20.0f);
        int hx0 = std::max(0, (int)((t.x - t.r) / 2)), hx1 = std::min(HW - 1, (int)((t.x + t.r) / 2));
        int hy0 = std::max(0, (int)((t.y - t.r) / 2)), hy1 = std::min(HH - 1, (int)((t.y + t.r) / 2));
        for (int hy = hy0; hy <= hy1; hy++) for (int hx = hx0; hx <= hx1; hx++) {
            float d = hyp(hx * 2.0f - t.x, hy * 2.0f - t.y) / t.r;
            if (d >= 1) continue;
            float h = hgt * std::sqrt(1 - d * d);
            float& cell = hmap[(size_t)hy * HW + hx];
            cell = std::max(cell, h);
        }
    }
    // tiles with any relief (dilated) get the lighting pass; flat meadow skips it
    std::vector<u8> relief((size_t)MAP_W * MAP_H, 0);
    {
        std::vector<u8> raw((size_t)MAP_W * MAP_H, 0);
        for (int hy = 0; hy < HH; hy++) for (int hx = 0; hx < HW; hx++) if (std::abs(hmap[(size_t)hy * HW + hx]) > 0.05f) raw[(size_t)(hy * 2 / TILE) * MAP_W + hx * 2 / TILE] = 1;
        for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            u8 v = 0;
            for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) v |= raw[(size_t)clampi(ty + j, 0, MAP_H - 1) * MAP_W + clampi(tx + i, 0, MAP_W - 1)];
            relief[(size_t)ty * MAP_W + tx] = v;
        }
    }
    auto hsamp = [&](float hx, float hy) { return samp(hmap, HW, HH, hx, hy); };

    // ---- per-pixel ground
    parallelRows(H, [&](int y0, int y1) {
        for (int y = y0; y < y1; y++) for (int x = 0; x < W; x++) {
            int tx = x >> 5, ty = y >> 5;
            u8 bits = present[(size_t)ty * MAP_W + tx];
            float wox, woy; warpAt((float)x, (float)y, wox, woy);
            float fx = (x + wox) / CELL - 0.5f, fy = (y + woy) / CELL - 0.5f;
            float w[K_N]; float sum = 0, bestV = -1; int best = 0;
            for (int c = 0; c < K_N; c++) {
                if (!(bits >> c & 1)) { w[c] = 0; continue; }
                float m = samp(mask[c], CW, CH, fx, fy);
                if (m > bestV) { bestV = m; best = c; }
                float mm = m;
                if (m > 0.02f && m < 0.98f) mm += (nz.v(x * 0.13f + c * 31.7f, y * 0.13f + c * 17.3f) - 0.5f) * 0.28f + (nz.v(x * 0.4f + c * 7.0f, y * 0.4f + c * 3.0f) - 0.5f) * 0.12f;
                float s = std::max(mm, 0.0f); s *= s; s *= s;
                w[c] = s; sum += s;
            }
            if (sum < 1e-6f) { w[best] = 1; sum = 1; }
            float macro = nz.fbm(x * 0.012f, y * 0.012f);
            float depth = 0;
            if (w[K_WATER] > 0) depth = sstep(0.5f, 0.95f, samp(wide, CW, CH, fx, fy));
            F3 col{0, 0, 0};
            float inv = 1.0f / sum;
            for (int c = 0; c < K_N; c++) if (w[c] > 0.003f) col = add3(col, mul3(groundColor(c, (float)x, (float)y, nz, macro, depth), w[c] * inv));
            float wWater = w[K_WATER] * inv, wRoad = w[K_ROAD] * inv, wSand = w[K_SAND] * inv;
            // shorelines: foam on the water side, wet dark sand on the land side
            if (wWater > 0.02f && wWater < 0.98f) {
                float edge = 1.0f - std::abs(wWater - 0.5f) * 2.0f;
                if (wWater > 0.4f) {
                    float foam = edge * edge * (0.45f + 0.55f * nz.v(x * 0.28f + 5, y * 0.28f + 9));
                    foam *= 0.5f + 0.5f * std::sin(x * 0.17f + y * 0.11f + 6.0f * nz.v(x * 0.02f, y * 0.02f));
                    col = lerp3(col, f3(226, 238, 236), clampf(foam * 0.8f, 0, 0.7f));
                } else col = mul3(col, 1.0f - 0.12f * edge);
            }
            if (wSand > 0.05f && wWater > 0.0f) col = mul3(col, 1.0f - 0.18f * clampf(wWater * 3.0f, 0, 1));
            // gravel shoulder where a road meets the ground, a darker kerb just inside it
            if (wRoad > 0.05f && wRoad < 0.95f) {
                float edge = 1.0f - std::abs(wRoad - 0.5f) * 2.0f;
                if (wRoad < 0.5f) col = lerp3(col, GRAVEL, edge * edge * 0.55f * (0.6f + 0.4f * nz.v(x * 0.3f, y * 0.3f)));
                else col = mul3(col, 1.0f - 0.16f * edge * edge);
            }
            // broad light variation
            col = mul3(col, 0.95f + 0.10f * nz.v(x * 0.006f + 3, y * 0.006f + 4));
            // relief lighting from the height field
            if (relief[(size_t)ty * MAP_W + tx]) {
                float hx = x * 0.5f, hy = y * 0.5f;
                float h0 = hsamp(hx, hy);
                float gx = (hsamp(hx + 1, hy) - hsamp(hx - 1, hy)) * 0.25f, gy = (hsamp(hx, hy + 1) - hsamp(hx, hy - 1)) * 0.25f;
                float nl = 1.0f / std::sqrt(gx * gx + gy * gy + 1.0f);
                float lam = (-gx * nl * -0.50f) + (-gy * nl * -0.60f) + nl * 0.62f;       // light from the NW and above
                float k = lam / 0.622f;
                float occ = 0;
                for (int s = 1; s <= 5; s++) {
                    float d = s * 7.0f;
                    float hh = hsamp(hx - 0.62f * d * 0.5f, hy - 0.78f * d * 0.5f);
                    occ = std::max(occ, clampf((hh - (h0 + d * 0.62f)) / 5.0f, 0, 1));
                }
                float rockish = clampf(h0 / 14.0f, 0, 1);
                k = 1.0f + (k - 1.0f) * 1.25f;
                col = mul3(col, clampf(k, 0.45f, 1.6f) * (1.0f - 0.40f * occ));
                // moss in the cracks at the foot of cliffs, a pale dry rim on the crests
                if (w[K_ROCK] > 0.4f * sum) { col = lerp3(col, MOSS, (1.0f - rockish) * 0.3f); col = add3(col, mul3(f3(30, 28, 24), clampf((h0 - 28.0f) / 14.0f, 0, 1))); }
            }
            Color o{ (u8)clampf(col.r, 0, 255), (u8)clampf(col.g, 0, 255), (u8)clampf(col.b, 0, 255), 255 };
            world.px[(size_t)y * W + x] = Canvas::pack(o);
        }
    });

    // ---- decor: ground clutter first (under the trees)
    Rng r(4242);
    auto tileAt = [&](float x, float y) { return g_map.tile(clampi(tileOf(x), 0, MAP_W - 1), clampi(tileOf(y), 0, MAP_H - 1)); };
    auto openGround = [&](float x, float y) { int t = tileAt(x, y); return t == T_GRASS || t == T_GRASS2 || t == T_DIRT || t == T_SAND; };
    // grass tufts
    for (int i = 0; i < 26000; i++) {
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_GRASS && t != T_GRASS2) continue;
        if (nz.v(x * 0.02f + 9, y * 0.02f + 2) < 0.4f) continue;
        int blades = r.range(3, 5);
        bool lightTuft = r.f() < 0.5f;
        for (int b = 0; b < blades; b++) {
            float an = -1.5708f + (b - (blades - 1) * 0.5f) * 0.42f + r.f(-0.1f, 0.1f), l = r.f(3, 6);
            Color c = lightTuft ? rgb(150, 178, 82) : rgb(36, 66, 30); c.a = 120;
            int steps = (int)l;
            for (int s = 0; s <= steps; s++) world.set((int)(x + std::cos(an) * s), (int)(y + std::sin(an) * s), c);
        }
    }
    // wildflowers in drifts
    for (int i = 0; i < 9000; i++) {
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_GRASS && t != T_GRASS2) continue;
        float drift = nz.v(x * 0.011f + 50, y * 0.011f + 17);
        if (drift < 0.62f) continue;
        static const Color FL[5] = { rgb(246, 244, 230), rgb(244, 214, 70), rgb(208, 168, 228), rgb(240, 150, 170), rgb(150, 190, 246) };
        Color c = FL[(int)(drift * 40) % 5];
        for (int k = 0; k < 3; k++) { float fx = x + r.f(-5, 5), fy = y + r.f(-5, 5); world.set((int)fx, (int)fy, c); world.set((int)fx + 1, (int)fy, shade(c, 0.85f)); world.set((int)fx, (int)fy + 1, rgb(60, 90, 40, 140)); }
    }
    // pebbles on dirt and sand, shells on the shore
    for (int i = 0; i < 6000; i++) {
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_DIRT && t != T_SAND) continue;
        float s = r.f(1.2f, 3.0f); Color c = shade(t == T_SAND ? rgb(170, 154, 120) : rgb(150, 132, 108), r.f(0.8f, 1.15f));
        world.fillCircle(x + 1, y + 1, s, rgb(30, 24, 18, 90));
        world.fillCircle(x, y, s, c);
        world.fillCircle(x - s * 0.3f, y - s * 0.3f, s * 0.45f, shade(c, 1.3f));
    }
    // reeds along the water's edge
    for (int ty = 1; ty < MAP_H - 1; ty++) for (int tx = 1; tx < MAP_W - 1; tx++) {
        if (g_map.tile(tx, ty) != T_SAND) continue;
        bool shore = g_map.tile(tx + 1, ty) == T_WATER || g_map.tile(tx - 1, ty) == T_WATER || g_map.tile(tx, ty + 1) == T_WATER || g_map.tile(tx, ty - 1) == T_WATER;
        if (!shore) continue;
        for (int k = 0; k < 5; k++) {
            float x = tx * TILE + r.f(2, TILE - 2), y = ty * TILE + r.f(2, TILE - 2);
            int h = r.range(5, 10);
            for (int s = 0; s < h; s++) world.set((int)(x + s * 0.12f), (int)(y - s), s > h - 3 ? rgb(120, 78, 40) : rgb(70, 108, 52, 220));
        }
    }
    // road centre lines: dashes along every segment, only where the tile is still asphalt
    for (const auto& rs : g_map.roads) {
        float x0 = rs.x0 * TILE, y0 = rs.y0 * TILE, x1 = rs.x1 * TILE, y1 = rs.y1 * TILE;
        float len = hyp(x1 - x0, y1 - y0); if (len < 1) continue;
        float dx = (x1 - x0) / len, dy = (y1 - y0) / len;
        for (float s = 20; s < len - 20; s += 34) {
            for (float u = 0; u < 15; u += 0.7f) {
                float px = x0 + dx * (s + u), py = y0 + dy * (s + u);
                if (tileAt(px, py) != T_ROAD) continue;
                for (int wdt = -1; wdt <= 1; wdt++) world.set((int)(px - dy * wdt), (int)(py + dx * wdt), rgb(214, 196, 126, wdt == 0 ? 200 : 110));
            }
        }
    }
    // supply piles stand on churned earth with a ring of tyre tracks
    for (const auto& sp : g_map.supplies) {
        float cx = sp.tx * TILE + TILE * 0.5f, cy = sp.ty * TILE + TILE * 0.5f;
        for (int i = 0; i < 40; i++) { float an = r.f(0, 6.28f), d = r.f(10, 44); world.fillCircle(cx + std::cos(an) * d, cy + std::sin(an) * d * 0.8f, r.f(1.5f, 4), rgb(70, 54, 36, 80)); }
        for (int k = 0; k < 2; k++) for (float a = 0; a < 6.2f; a += 0.05f) { float rr = 34 + k * 5; world.set((int)(cx + std::cos(a) * rr), (int)(cy + std::sin(a) * rr * 0.8f), rgb(60, 46, 32, 70)); }
    }

    // ---- boulders: scattered where open ground meets rock, plus a few strays
    {
        std::vector<Canvas> boulder; float br[6];
        for (int i = 0; i < 6; i++) { br[i] = 5.0f + i * 1.6f; boulder.push_back(makeBoulder(br[i], 900u + i)); }
        for (int i = 0; i < 700; i++) {
            float x = r.f(8, W - 8.0f), y = r.f(8, H - 8.0f);
            if (!openGround(x, y)) continue;
            bool nearRock = false;
            for (int j = -1; j <= 1 && !nearRock; j++) for (int k = -1; k <= 1; k++) if (tileAt(x + k * TILE, y + j * TILE) == T_ROCK) { nearRock = true; break; }
            if (!nearRock && r.f() > 0.06f) continue;
            bool near = false;
            for (auto& s : g_map.supplies) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < 70) near = true;
            for (auto& s : g_map.starts) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < 9 * TILE) near = true;
            if (near) continue;
            int v = r.range(0, 5);
            blitOver(world, boulder[v], (int)(x - boulder[v].w * 0.5f), (int)(y - boulder[v].h * 0.5f));
        }
    }

    // ---- trees, back to front
    {
        std::vector<Canvas> sprites[T_KINDS];
        for (int k = 0; k < T_KINDS; k++) for (int v = 0; v < 5; v++) { float R = k == T_BUSH ? 8.5f : (k == T_CONIFER ? 15.0f : 16.5f); sprites[k].push_back(makeTree(k, R + v * 0.6f, 5000u + k * 97u + v * 13u)); }
        for (auto& t : trees) {
            const Canvas& s = sprites[t.kind][t.seed % sprites[t.kind].size()];
            float sc = t.r / (t.kind == T_BUSH ? 8.5f : (t.kind == T_CONIFER ? 15.0f : 16.5f));
            if (std::abs(sc - 1.0f) < 0.18f) { blitOver(world, s, (int)(t.x - s.w * 0.5f), (int)(t.y - s.h * 0.5f)); continue; }
            // scale by nearest sampling for the odd size
            int nw = std::max(4, (int)(s.w * sc)), nh = std::max(4, (int)(s.h * sc));
            Canvas sc2(nw, nh);
            for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) sc2.px[y * nw + x] = s.px[std::min(s.h - 1, (int)(y / sc)) * s.w + std::min(s.w - 1, (int)(x / sc))];
            blitOver(world, sc2, (int)(t.x - nw * 0.5f), (int)(t.y - nh * 0.5f));
        }
    }
}

void terrainOverview(const Canvas& world, Canvas& out) {
    int n = out.w, f = world.w / n;
    for (int y = 0; y < out.h; y++) for (int x = 0; x < out.w; x++) {
        float r = 0, g = 0, b = 0;
        for (int j = 0; j < f; j += 2) for (int i = 0; i < f; i += 2) { Color c = Canvas::unpack(world.px[(size_t)(y * f + j) * world.w + x * f + i]); r += c.r; g += c.g; b += c.b; }
        float k = 1.0f / (float)(((f + 1) / 2) * ((f + 1) / 2));   // (the loops above take (f + 1) / 2 samples a side)
        out.px[y * out.w + x] = Canvas::pack(Color{(u8)clampf(r * k, 0, 255), (u8)clampf(g * k, 0, 255), (u8)clampf(b * k, 0, 255), 255});
    }
}
