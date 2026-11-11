#include "terrain.h"
#include "paint.h"
#include "m3d.h"
#include "m3d_reg.h"
#include <SDL2/SDL.h>
#include <functional>

using namespace paint;

// The ground is baked once. Terrain classes are smoothed into coverage fields (so borders become curves), the fields are displaced by
// low-frequency noise and sharpened (so borders turn ragged and organic), each class is shaded procedurally per pixel, and a coarse
// height field (cliffs, lake beds, tree canopies) lights everything from the north-west and casts soft shadows to the south-east.
// Trees, boulders, grass, flowers, pebbles, reeds and road markings are then painted on top. It all runs on worker threads.
namespace {

enum { K_GRASS = 0, K_GRASS2, K_DIRT, K_SAND, K_ROAD, K_WATER, K_ROCK, K_FOREST, K_N };

// floor without a libm call (the bake calls it a few hundred million times on the generic x86-64 target)
inline int ifloor(float x) { int i = (int)x; return i > x ? i - 1 : i; }

struct Noise {
    float g[256 * 256];
    Noise() { Rng r(0xB16B00B5ull); for (auto& v : g) v = r.f(); }
    inline float at(int x, int y) const { return g[((y & 255) << 8) | (x & 255)]; }
    inline float v(float x, float y) const {
        int ix = ifloor(x), iy = ifloor(y);
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

// Separable box blur, rows and then columns split across the cores (each row or column is independent)
void blurBox(std::vector<float>& a, int w, int h, int r, int iters) {
    std::vector<float> tmp(a.size());
    float inv = 1.0f / (2 * r + 1);
    for (int it = 0; it < iters; it++) {
        parallelRows(h, [&](int y0, int y1) {
            for (int y = y0; y < y1; y++) {
                const float* row = &a[(size_t)y * w]; float* out = &tmp[(size_t)y * w];
                float sum = 0;
                for (int i = -r; i <= r; i++) sum += row[clampi(i, 0, w - 1)];
                for (int x = 0; x < w; x++) { out[x] = sum * inv; sum += row[clampi(x + r + 1, 0, w - 1)] - row[clampi(x - r, 0, w - 1)]; }
            }
        });
        parallelRows(w, [&](int x0, int x1) {
            for (int x = x0; x < x1; x++) {
                float sum = 0;
                for (int i = -r; i <= r; i++) sum += tmp[(size_t)clampi(i, 0, h - 1) * w + x];
                for (int y = 0; y < h; y++) { a[(size_t)y * w + x] = sum * inv; sum += tmp[(size_t)clampi(y + r + 1, 0, h - 1) * w + x] - tmp[(size_t)clampi(y - r, 0, h - 1) * w + x]; }
            }
        });
    }
}

inline float samp(const std::vector<float>& m, int w, int h, float fx, float fy) {
    int x0 = ifloor(fx), y0 = ifloor(fy);
    float tx = fx - x0, ty = fy - y0;
    int x1 = clampi(x0 + 1, 0, w - 1), y1 = clampi(y0 + 1, 0, h - 1);
    x0 = clampi(x0, 0, w - 1); y0 = clampi(y0, 0, h - 1);
    float a = m[(size_t)y0 * w + x0], b = m[(size_t)y0 * w + x1], c = m[(size_t)y1 * w + x0], d = m[(size_t)y1 * w + x1];
    return a + (b - a) * tx + (c - a) * ty + (a - b - c + d) * tx * ty;
}

// ---------------------------------------------------------------- ground colours
// Materials, not tints: each class is built from a small palette that belongs to the thing it is (a meadow's greens run from shaded clover to
// sun-bleached seed heads; earth runs from damp plough to pale chalk; sand from dry dune crest to wet tide line), so the ground reads as a
// material under one light rather than as noise. Values are the MATERIAL's own colour; the sun (and its grade) does the rest.
const F3 GRASS_A = f3(74, 104, 46), GRASS_B = f3(96, 126, 54), GRASS2_A = f3(57, 89, 44), GRASS2_B = f3(84, 112, 48);
const F3 DRY = f3(158, 148, 88);           // sun-cured grass: the pale straw of a hot meadow
const F3 GRASS_LUSH = f3(96, 138, 50), GRASS_DEEP = f3(42, 70, 34);      // knots of longer grass: sunned, and shaded
const F3 DIRT_A = f3(132, 96, 62), DIRT_B = f3(104, 74, 48), DIRT_DARK = f3(72, 48, 32), DIRT_PALE = f3(178, 142, 100);
const F3 SAND_A = f3(226, 202, 146), SAND_B = f3(200, 172, 116), SAND_WET = f3(142, 126, 92);
const F3 ASPHALT = f3(62, 64, 70), GRAVEL = f3(138, 128, 110);
const F3 FOREST_A = f3(48, 62, 34), FOREST_B = f3(84, 66, 42), LEAF_LITTER = f3(146, 96, 44);
const F3 ROCK_A = f3(152, 144, 130), ROCK_B = f3(106, 103, 101), ROCK_OCHRE = f3(150, 116, 78), LICHEN = f3(130, 136, 98);
const F3 MOSS = f3(74, 100, 46), SCREE = f3(160, 148, 126);
const F3 WATER_SHALLOW = f3(84, 186, 172), WATER_MID = f3(26, 124, 162), WATER_DEEP = f3(10, 58, 108), WEED = f3(40, 108, 76);
const F3 OCEAN_ABYSS = f3(8, 34, 74);   // the open sea far from any shore
const float SEA_OPEN = 7.5f;            // tiles from land beyond which water is open sea: no sand bars, weed or caustics, only waves and depth

// The grade every baked pixel goes through: an S-curve (deep, clean shadows; highlights that roll off), fuller colour, and a split tone that
// leans the shadows to teal and the highlights to gold, so the whole map sits in one late-afternoon light.
inline void gradeRGB(float& vr, float& vg, float& vb) {
    float lum0 = vr * 0.299f + vg * 0.587f + vb * 0.114f;
    float k = lum0 < 0.5f ? 0.55f + 0.9f * lum0 : 1.0f;   // (less crush in the shadows: materials keep their colour down there)
    auto curve = [k](float v) { return v * (1.0f - 0.32f * k) + v * v * (3.0f - 2.0f * v) * (0.32f * k); };
    vr = curve(vr); vg = curve(vg); vb = curve(vb);
    float lum = vr * 0.299f + vg * 0.587f + vb * 0.114f;
    vr = lum + (vr - lum) * 1.06f; vg = lum + (vg - lum) * 1.06f; vb = lum + (vb - lum) * 1.06f;
    float t = lum - 0.45f;
    vr += t * 0.070f; vg += t * 0.012f; vb -= t * 0.090f;
}
// Rolling ground: the land is not a table. A slow height wave is lit from the north-west, so meadows swell and dip (a pure function of the
// pixel, shared by the bake and by repainted ground).
inline float rollShade(float x, float y, const Noise& nz) {
    float a = nz.v(x * 0.0125f + 91, y * 0.0125f + 37), b = nz.v((x + 5.0f) * 0.0125f + 91, (y + 6.0f) * 0.0125f + 37);
    float c = nz.v(x * 0.041f + 13, y * 0.041f + 77), d = nz.v((x + 3.0f) * 0.041f + 13, (y + 3.5f) * 0.041f + 77);
    return 1.0f + clampf((a - b) * 3.4f + (c - d) * 2.2f, -0.15f, 0.10f);
}
// Open sea, far from land: what K_WATER below works out to there (depth 1: the bars, weed and caustics are gone), done with a third of the work
F3 openSeaColor(float x, float y, const Noise& nz, float sea) {
    // (the depth waver is fine-grained: a broad one would survive the 565 quantisation as contour lines across the open sea)
    float d = clampf(1.0f + (nz.v(x * 0.10f + 17, y * 0.10f + 28) - 0.5f) * 0.10f + (nz.v(x * 0.35f, y * 0.35f) - 0.5f) * 0.03f, 0, 1);
    F3 col = lerp3(WATER_MID, WATER_DEEP, d);
    col = lerp3(col, OCEAN_ABYSS, sstep(SEA_OPEN, 22.0f, sea) * 0.88f);
    float n2 = nz.v(x * 0.17f + 5, y * 0.17f + 9);
    // the same two crossed swells as the lake, so open water has one sea on it: their west faces are lit, their troughs hold a deeper blue (the
    // cosines are shaded through their own gradient rather than as bands, so there is no contour line to read as a map symbol)
    float sl1 = std::cos(x * 0.030f + 1.7f * nz.fbm(x * 0.008f, y * 0.008f) + y * 0.017f);
    float sl2 = std::cos(x * 0.014f - 2.2f * nz.v(x * 0.012f + 30, y * 0.012f + 8) - y * 0.040f + 2.1f);
    col = mul3(col, 1.0f + 0.045f * sl1 + 0.028f * sl2 + 0.02f * (n2 - 0.5f));
    // a fine chop that survives the half-resolution open sea: two octaves at about 6 and 3 px, tilted against the swell
    float ch = nz.v(x * 0.34f + y * 0.11f + 3, y * 0.29f + 7) * 0.6f + nz.v(x * 0.9f + y * 0.2f, y * 0.8f + 19) * 0.4f;
    col = mul3(col, 0.985f + 0.05f * ch);
    float glit = clampf(1.0f - std::abs((x - 2.0f * y) * 0.4472f - 260.0f * std::sin(x * 0.0007f)) / 440.0f, 0, 1);
    float th = 0.994f - 0.012f * glit;
    if (nz.v(x * 1.3f + 13, y * 1.3f + 7) > th) col = add3(col, mul3(f3(90, 110, 104), 0.35f + 0.65f * glit));
    return col;
}

// The four base octaves every ground class reads at a pixel: evaluated once per pixel, so a border pixel that blends two classes samples them once
struct Base { float n1, n2, n3, n4; };
inline Base baseAt(const Noise& nz, float x, float y) {
    return Base{ nz.v(x * 0.045f, y * 0.045f), nz.v(x * 0.17f + 5, y * 0.17f + 9), nz.v(x * 0.9f + y * 0.3f, y * 1.3f), nz.v(x * 0.55f + 71, y * 0.55f + 13) };
}
// ---------------------------------------------------------------- ground materials
// A material is not a tint: at 1x the eye reads grass by its clumps and the shadow between them, earth by its crust and grit, rock by its
// ledges. So each class is built in three acts: a broad field tone (the thing seen from far away), mid-scale patches of its own family
// (drifts of dry grass, damp soil, sand bars), and then fine structure whose LIGHT and DARK are correlated (a clump is lit on its west and
// shaded on its east), which is what makes it read as three-dimensional ground rather than as noise.
//
// A sun-facing slope, in units of tone: dx is the slope worked out from a height noise. The west light: a face turned to the west is lit.
inline float facing(float hx, float hy) { return clampf(-hx * 0.78f - hy * 0.40f, -1.0f, 1.0f); }
// F3 helpers for material work
inline F3 toned(F3 c, float k) { return mul3(c, k); }
// The materials proper ------------------------------------------------------------------------------------------------
// Meadow grass: a field of clumps. The lit crown of a clump is the class's grass; its far side falls into shade; a few pale seed heads catch
// the sun. Dryness (macro) bleaches the whole field towards straw and thins the clumps out.
F3 grassMaterial(F3 lushA, F3 lushB, float x, float y, const Noise& nz, float macro, float n1, float n2, float n4) {
    // --- field tone: the average of grass and the dark between it
    F3 field = lerp3(lushA, lushB, n1);
    // --- mid: drifts of cure (macro weather) and hollows of new growth
    float drift = nz.v(x * 0.0045f + 21, y * 0.0045f + 6);
    float dry = sstep(0.50f, 0.88f, drift) * (0.55f + 0.45f * macro);
    field = lerp3(field, DRY, dry * 0.62f);
    field = lerp3(field, GRASS_DEEP, sstep(0.42f, 0.10f, drift) * 0.35f);
    // --- clumps: a rounded-cell pattern (~9 px) that gives every square its own tilt
    float cx = x * 0.115f, cy = y * 0.115f;
    float h0 = nz.v(cx, cy);
    float hx = nz.v(cx + 0.6f, cy) - h0, hy = nz.v(cx, cy + 0.6f) - h0;   // (one-sided: two samples, not four)
    float clump = sstep(0.40f, 0.78f, h0) * (1.0f - 0.72f * dry);   // cure thins the clumps out
    float lit = facing(hx, hy);                                     // the clump's west shoulder
    // the clump is a tuft: lit crowns on the tone of the field, a shadowed hollow east of each
    field = mul3(field, 1.0f + 0.26f * lit * clump + 0.16f * clump);
    field = mul3(field, 1.0f - 0.20f * clampf(hy, 0, 1) * clump);
    // --- blades: two-pixel strokes, warm where they face the sun, cool in the shade
    float blade = nz.v(x * 0.62f + 3, y * 0.26f + 8);
    field = mul3(field, 0.90f + 0.20f * blade);
    // --- seed heads: pale ticks on tall stems, sparse, denser where the field is curing
    if (n4 > 0.955f - 0.035f * dry && n4 < 0.985f) field = lerp3(field, f3(196, 184, 120), 0.55f);
    else if (n4 >= 0.985f) field = lerp3(field, f3(220, 214, 170), 0.4f);
    // --- flowers as flecks in the field tone (the painted flower patches add the big ones on top)
    if (n2 > 0.955f) field = lerp3(field, f3(226, 208, 96), 0.5f);
    return field;
}
// Bare earth: damp plough furrows, a dried crust on the crests, gravel and stone showing where the crust has broken.
F3 dirtMaterial(float x, float y, const Noise& nz, float n1, float n2, float n3, float n4) {
    F3 col = lerp3(DIRT_A, DIRT_B, n1);
    // damp hollows (the low ground) and pale dried crests
    float low = sstep(0.55f, 0.80f, nz.v(x * 0.06f + 33, y * 0.06f + 7));
    col = lerp3(col, DIRT_DARK, low * 0.62f);
    col = lerp3(col, DIRT_PALE, sstep(0.66f, 0.86f, n1) * 0.30f);   // (n1 is a 22 px field: the crests)
    // furrows: long slow ridges, so the earth reads as worked ground
    float ridge = std::sin((x * 0.012f + y * 0.004f + n1 * 0.9f) * 24.0f);   // (the ridged plough, wavering with the field tone)
    float ry = nz.v(x * 0.09f, y * 0.09f + 40);
    col = mul3(col, 0.93f + 0.09f * ridge * (0.4f + 0.6f * ry));
    // clods: a pebbled field, lit on the west
    float h0 = nz.v(x * 0.35f + 11, y * 0.35f + 3), hx = nz.v(x * 0.35f + 11.6f, y * 0.35f + 3) - h0, hy = nz.v(x * 0.35f + 11, y * 0.35f + 3.6f) - h0;
    col = mul3(col, 1.0f + 0.30f * facing(hx, hy) * sstep(0.42f, 0.72f, h0));
    col = mul3(col, 0.90f + 0.20f * n2);
    // crust cracks and gravel
    float crack = std::abs(nz.v(x * 0.055f + 90, y * 0.055f + 40) - 0.5f);
    if (crack < 0.008f) col = mul3(col, 0.76f);
    if (n4 > 0.93f) col = lerp3(col, f3(150, 138, 120), 0.55f);            // pale stones
    else if (n3 > 0.93f) col = mul3(col, 0.86f);                            // dark grit
    // a little green in the damp hollows: grass creeping back
    if (low > 0.55f) col = lerp3(col, MOSS, (low - 0.55f) * 0.5f * sstep(0.55f, 0.85f, ry));
    return col;
}
// Sand: dunes and wind ripples, a dry pale crest and a damp darker trough, grit on the sheltered faces.
F3 sandMaterial(float x, float y, const Noise& nz, float n1, float n2, float n3, float n4) {
    float swirl = nz.v(x * 0.02f + 4, y * 0.02f + 11);
    float ripple = std::sin((x * 0.30f + y * 0.14f + 14.0f * swirl) * 0.55f);   // wind ripples (wider and slower than the old ones)
    float swell = n1;                                                            // (the 22 px field is the dunes)
    float h = 0.55f * swell + 0.20f * ripple + 0.10f * n2;
    F3 col = lerp3(SAND_B, SAND_A, clampf(h, 0, 1));
    float lit = clampf(ripple * 0.6f + (swell - 0.5f), -1, 1);   // crest lit, trough shaded
    col = mul3(col, 1.0f + 0.07f * lit);
    col = mul3(col, 0.96f + 0.07f * n3);
    if (n4 > 0.94f) col = mul3(col, 0.90f);                    // grit
    else if (n4 < 0.05f) col = lerp3(col, f3(240, 226, 186), 0.5f);   // bleached grains
    return col;
}
// One ground class's colour at a pixel. Land classes are built from several scales: a large drift or sweep, a mid clump, and fine detail.
F3 classColor(int c, float x, float y, const Noise& nz, const Base& b, float macro, float depth, float sea) {
    float n1 = b.n1, n2 = b.n2, n3 = b.n3, n4 = b.n4;
    switch (c) {
    case K_GRASS: return grassMaterial(GRASS_A, GRASS_B, x, y, nz, macro, n1, n2, n4);
    case K_GRASS2: return grassMaterial(GRASS2_A, GRASS2_B, x, y, nz, macro, n1, n2, n4);
    case K_DIRT: return dirtMaterial(x, y, nz, n1, n2, n3, n4);
    case K_SAND: return sandMaterial(x, y, nz, n1, n2, n3, n4);
    case K_ROAD: {
        // asphalt: coarse aggregate, worn wheel tracks polished pale, patch repairs a shade darker, a tarred seam down the middle
        F3 col = mul3(ASPHALT, 0.93f + 0.14f * n3);
        col = mul3(col, 0.95f + 0.10f * n1);
        float agg = nz.v(x * 0.75f + 21, y * 0.75f + 5);                    // the aggregate itself, not a haze
        col = mul3(col, 0.90f + 0.22f * agg);
        col = lerp3(col, f3(96, 96, 96), sstep(0.60f, 0.82f, nz.v(x * 0.05f + 27, y * 0.05f + 3)) * 0.16f);   // polished, worn patches
        float crack = std::abs(nz.v(x * 0.06f + 12, y * 0.06f + 77) - 0.5f);
        if (crack < 0.005f) col = mul3(col, 0.62f);                                                       // hairline cracks with a lit lip
        else if (crack < 0.009f) col = mul3(col, 1.10f);
        float patch = sstep(0.66f, 0.74f, nz.v(x * 0.03f + 5, y * 0.03f + 61));
        col = lerp3(col, f3(74, 74, 78), patch * 0.26f);
        float tyre = std::abs(nz.v(x * 0.012f + y * 0.004f + 20, y * 0.012f) - 0.5f);   // faint wheel tracks
        if (tyre < 0.02f) col = mul3(col, 0.91f);
        if (n4 > 0.96f) col = add3(col, f3(26, 26, 26));                    // a bright chip
        return col;
    }
    case K_FOREST: {
        // the needle floor under the canopy: deep duff, drifts of fallen leaves, cool moss between the roots, and the odd lit patch of sand
        F3 col = lerp3(FOREST_A, FOREST_B, n2);
        col = mul3(col, 0.84f + 0.24f * n1);
        float litter = nz.v(x * 0.31f + 12, y * 0.31f + 31);                  // fallen leaves in drifts
        col = lerp3(col, LEAF_LITTER, sstep(0.58f, 0.82f, litter) * 0.55f);
        col = lerp3(col, MOSS, sstep(0.62f, 0.84f, nz.v(x * 0.11f + 61, y * 0.11f + 9)) * 0.30f);
        float h0 = nz.v(x * 0.28f + 7, y * 0.28f + 44);
        float hx2 = nz.v(x * 0.28f + 7.6f, y * 0.28f + 44) - h0;                 // (one-sided: two samples)
        col = mul3(col, 1.0f + 0.22f * facing(hx2, 0) * sstep(0.40f, 0.70f, h0));   // needles and twigs in raking light
        col = mul3(col, 0.93f + 0.14f * n3);
        return col;
    }
    case K_ROCK: {
        // stone: bedding planes, iron staining, lichen on the sheltered faces, and a lit west lip on every ledge
        float strata = std::sin((y * 0.85f + x * 0.28f + 9.0f * nz.fbm(x * 0.02f, y * 0.02f)) * 0.55f);
        F3 col = lerp3(ROCK_A, ROCK_B, n1 * 0.8f + 0.2f * (0.5f + 0.5f * strata));
        float h0 = nz.v(x * 0.42f + 3, y * 0.42f + 18);
        float hx2 = nz.v(x * 0.42f + 3.7f, y * 0.42f + 18) - h0, hy2 = nz.v(x * 0.42f + 3, y * 0.42f + 18.7f) - h0;   // (one-sided: three samples)
        col = mul3(col, 1.0f + 0.34f * facing(hx2, hy2) * sstep(0.38f, 0.68f, h0));
        col = lerp3(col, ROCK_OCHRE, sstep(0.62f, 0.8f, nz.v(x * 0.05f + 80, y * 0.05f + 12)) * 0.30f);   // iron stains
        col = lerp3(col, LICHEN, sstep(0.70f, 0.80f, nz.v(x * 0.09f + 14, y * 0.09f + 66)) * 0.40f);      // lichen
        col = mul3(col, 0.88f + 0.18f * n2);
        col = mul3(col, 0.94f + 0.12f * n3);
        float crack = std::abs(nz.v(x * 0.09f + 3, y * 0.09f + 55) - 0.5f);
        if (crack < 0.014f) col = mul3(col, 0.58f);
        return col;
    }
    case K_WATER: {
        // Clear water over a visible bed. The colour is what you see looking down through it: the seabed's own sand and weed where it is shallow,
        // a turquoise shelf, a teal drop-off, then the navy of deep water. Depth (0 at the shore, 1 far out) drives the whole ramp, and the waves
        // only ripple the surface on top of it.
        float d = clampf(depth + (n1 - 0.5f) * 0.10f + (n3 - 0.5f) * 0.03f, 0, 1);   // dithered depth: no contour rings on a big lake bed
        float open = sstep(SEA_OPEN - 2.0f, SEA_OPEN, sea);   // (fades the shallows' detail out toward the open sea, see openSeaColor)
        F3 col;
        // four-stop ramp: pale sand through the water, bright turquoise, lake teal, then the deep
        const float sh = sstep(0.00f, 0.30f, d), tu = sstep(0.22f, 0.60f, d), tp = sstep(0.55f, 0.86f, d);
        col = lerp3(f3(122, 190, 178), f3(58, 170, 176), sh);
        col = lerp3(col, f3(24, 120, 158), tu);
        col = lerp3(col, f3(12, 62, 112), tp);
        // sand bars and weed beds show through the shallows (seen through the water, so softened towards its own colour)
        float bar = sstep(0.62f, 0.8f, nz.v(x * 0.016f + 44, y * 0.016f + 2));
        col = lerp3(col, lerp3(SAND_B, f3(120, 196, 180), 0.45f), bar * (1.0f - d) * 0.5f * (1.0f - open));
        float weed = sstep(0.6f, 0.82f, nz.v(x * 0.03f + 8, y * 0.03f + 51));
        col = lerp3(col, WEED, weed * clampf(1.0f - d * 1.6f, 0, 1) * 0.45f * (1.0f - open));
        // the surface: two crossed swells (a broad one from the south-west, a finer chop), lit on their west faces, so the water has a direction
        float sl1 = std::cos(x * 0.030f + 1.7f * nz.fbm(x * 0.005f, y * 0.005f) + y * 0.017f);
        float sl2 = std::cos(x * 0.014f - 2.2f * nz.v(x * 0.006f + 30, y * 0.006f + 8) - y * 0.040f + 2.1f);
        col = mul3(col, 1.0f + 0.050f * sl1 + 0.032f * sl2 + 0.02f * (n2 - 0.5f));
        // the crests catch the west light, the troughs hold a deeper blue
        col = add3(col, mul3(f3(20, 30, 26), clampf(sl1, 0, 1) * 0.32f * (1.0f - 0.5f * d)));
        float c1 = 1.0f - std::abs(2.0f * nz.v(x * 0.09f + 3, y * 0.09f + 11) - 1.0f), c2 = 1.0f - std::abs(2.0f * nz.v(x * 0.065f + 40 + y * 0.02f, y * 0.065f + 7) - 1.0f);
        float caus = std::pow(c1 * c2, 6.0f);                                        // finer caustic light networks in the shallows
        col = add3(col, mul3(f3(56, 84, 76), (1.0f - d * 0.8f) * clampf(caus * 2.0f, 0, 1) * (1.0f - open)));
        // sun glitter: a broken diagonal path of sparkles running away from the west light, denser in open water
        float glit = clampf(1.0f - std::abs((x - 2.0f * y) * 0.4472f - 260.0f * std::sin(x * 0.0007f)) / 440.0f, 0, 1);
        float th = 0.994f - 0.012f * glit;   // (sparse: a big lake sprinkled evenly read as noise)
        if (nz.v(x * 1.3f + 13, y * 1.3f + 7) > th) col = add3(col, mul3(f3(90, 110, 104), 0.35f + 0.65f * glit));
        return col;
    }
    }
    return f3(128, 128, 128);
}
F3 groundColor(int c, float x, float y, const Noise& nz, float macro, float depth, float sea = 0) { return classColor(c, x, y, nz, baseAt(nz, x, y), macro, depth, sea); }

// ---------------------------------------------------------------- trees and props
inline bool inMapPx(int x, int y) { return x >= 0 && y >= 0 && x < WORLD_W && y < WORLD_H; }
// a tile within two tiles of the ocean (beach and cliff country: no reeds, cattails or lily pads)
inline bool seaShore(int tx, int ty) {
    for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) {
        int x = tx + i, y = ty + j;
        if (inMap(x, y) && g_map.tiles[y * MAP_W + x] == T_WATER && !g_map.freshWater(x, y)) return true;
    }
    return false;
}
// Decor counts are per tile, not fixed: a count tuned for the 176x176 map scales with the map's area (so a bigger map keeps its density)
inline int decorCount(int base) { return (int)(base * ((float)(MAP_W * MAP_H) / (176.0f * 176.0f)) + 0.5f); }
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

// ---------------------------------------------------------------- the 3D wild (m3d_nature.cpp)
// Trees, bushes and boulders are solid models now, rendered once here into ordinary sprites: a trunk that flares at the root, boughs, a crown
// of overlapping leaf clumps lit on the west and shadowed to the east, a soft contact shadow on the ground, all in the same oblique view and
// under the same low western sun as the units and buildings. The model is rendered twice: once to measure it, then once at the scale that fits
// the sprite the game already expects (about 2R across), so every existing size relationship, and the repaint that uses it, still holds.
Canvas modelSprite(m3d::Model& m, float R) {
    m3d::RenderOpt o; o.scale = 1.0f; o.heading = 0.0f; o.ss = 2;   // (the sun stays in the west: the sprite never turns)
    m3d::Rendered probe; m3d::render(m, o, probe);
    int x0 = probe.color.w, x1 = -1, y0 = probe.color.h, y1 = -1;
    for (int y = 0; y < probe.color.h; y++) for (int x = 0; x < probe.color.w; x++)
        if ((probe.color.px[(size_t)y * probe.color.w + x] >> 24) > 8) { x0 = std::min(x0, x); x1 = std::max(x1, x); y0 = std::min(y0, y); y1 = std::max(y1, y); }
    if (x1 < x0) return Canvas(8, 8);
    // fit the whole model (crown and the shadow it casts) into the sprite the old painters used: about 2R across the crown, a little wider for
    // the shadow, so nothing about the forest's size, spacing or the area a repaint must recompose changes under it
    const float want = 2.0f * R + 12.0f;
    o.scale = want / (float)std::max(x1 - x0 + 1, y1 - y0 + 1);
    m3d::Rendered r; m3d::render(m, o, r);
    Canvas out = m3d::composite(r, rgb(255, 255, 255));
    // a sprite centred on the model's origin (the foot of the trunk), which is the centre convention every tree in the game uses
    const int hx = std::max((int)R + 6, (int)std::ceil(std::max(r.ox, out.w - r.ox))) + 1;
    const int hy = std::max((int)R + 6, (int)std::ceil(std::max(r.oy, out.h - r.oy))) + 1;
    Canvas c(hx * 2, hy * 2);
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        int sx = x - hx + (int)r.ox, sy = y - hy + (int)r.oy;
        if (sx < 0 || sy < 0 || sx >= out.w || sy >= out.h) continue;
        c.px[(size_t)y * c.w + x] = out.px[(size_t)sy * out.w + sx];
    }
    return c;
}
Canvas treeSprite(const char* name, int variant, float R, u32 seed) {
    m3d::PropFn fn = m3d::reg().prop(name);
    if (!fn) return Canvas(8, 8);
    m3d::Model m;
    fn(m, variant + (int)(seed % 3));                 // (variant cycles the model's own random shapes)
    return modelSprite(m, R);
}

Canvas makeTree(int kind, float R, u32 seed) {
    switch (kind) {
    case T_CONIFER: return treeSprite("tree.conifer", 0, R, seed);
    case T_AUTUMN: return treeSprite("tree.autumn", 0, R, seed);
    case T_BUSH: return treeSprite("bush", 0, R, seed);
    default: return treeSprite("tree.broad", 0, R, seed);
    }
}

Canvas makeBoulder(float R, u32 seed) {
    // the same 3D path: a weathered block with a shoulder stone, moss on the damp north face, its shadow on the ground to the east
    m3d::PropFn fn = m3d::reg().prop("boulder");
    if (!fn) return Canvas(8, 8);
    m3d::Model m;
    fn(m, (int)(seed % 3));
    return modelSprite(m, R * 1.4f);   // (the rock model is smaller than its footprint: the old sprites were mostly shadow)
}

// Decorative props baked into the ground. kind: 0 fallen log, 1 ruined wall, 2 standing stone, 3 burnt-out hulk, 4 lily pad, 5 broken pillar, 6 stump,
// 7 hay bales, 8 post-and-rail fence, 9 wildflower patch, 10 mossy boulder cluster, 11 camp site, 12 crates and barrels, 13 driftwood,
// 14 rowboat on the sand, 15 signpost, 16 cattails, 17 toadstool ring, 18 bones, 19 wheat strip
Canvas makeProp(int kind, float R, u32 seed) {
    int S = (int)(R * 2 + 14);
    Canvas big(S * 2, S * 2);
    Art a(big, rgb(255, 255, 255), 2.0f);
    Rng r(seed);
    float cx = S * 0.5f, cy = S * 0.5f;
    switch (kind) {
    case 0: {   // fallen log, with a cut end ring and a couple of broken branches
        float an = r.f(-0.5f, 0.5f), L = R * 1.1f, th = R * 0.2f;
        float ax = cx - std::cos(an) * L, ay = cy - std::sin(an) * L, bx = cx + std::cos(an) * L, by = cy + std::sin(an) * L;
        a.shadowCircle(cx, cy, L * 0.55f, 2, 3, 3, 45);
        Color bark = shade(rgb(92, 66, 44), r.f(0.85f, 1.1f));
        a.cap(ax, ay, bx, by, th, bark, th * 0.9f, 0.7f);
        a.line(ax + (bx - ax) * 0.1f, ay + (by - ay) * 0.1f - th * 0.3f, ax + (bx - ax) * 0.9f, ay + (by - ay) * 0.9f - th * 0.3f, 0.6f, rgba(rgb(150, 120, 86), 120));
        a.ell(bx, by, th * 0.55f, th * 0.9f, rgb(190, 158, 112), 1, 0.3f);
        a.ring(bx, by, th * 0.35f, 0.5f, rgba(rgb(120, 90, 60), 200), 0.5f, 0);
        for (int i = 0; i < 2; i++) { float t = r.f(0.25f, 0.75f), px = ax + (bx - ax) * t, py = ay + (by - ay) * t, s = i ? 1.0f : -1.0f; a.cap(px, py, px - std::sin(an) * s * th * 2.2f, py + std::cos(an) * s * th * 2.2f, th * 0.3f, shade(bark, 0.9f), 0.5f, 0.5f); }
        break;
    }
    case 1: {   // a stretch of ruined wall: a few mortared blocks of different height and a collapsed heap
        float an = r.f(-0.35f, 0.35f);
        int n = 4 + (int)r.range(0, 3);
        a.shadowCircle(cx, cy + 2, R * 0.5f, 3, 4, 3, 35);
        for (int i = 0; i < n; i++) {
            float t = (i - (n - 1) * 0.5f) * R * 0.34f, px = cx + std::cos(an) * t, py = cy + std::sin(an) * t;
            float h = r.f(0.8f, 1.6f) * ((i == 0 || i == n - 1) ? 0.6f : 1.0f);
            Color c = shade(rgb(150, 142, 128), r.f(0.82f, 1.1f));
            a.box(px, py - h * R * 0.12f, R * 0.17f, R * 0.12f + h * R * 0.12f, 1.2f, c, 2.0f, 0.65f);
            a.line(px - R * 0.15f, py - h * R * 0.12f, px + R * 0.15f, py - h * R * 0.12f, 0.6f, rgba(shade(c, 1.25f), 200));
        }
        for (int i = 0; i < 6; i++) a.poly({ {cx + r.f(-R * 0.6f, R * 0.6f), cy + R * 0.2f + r.f(0, R * 0.2f)}, {cx + r.f(-R * 0.6f, R * 0.6f) + 3, cy + R * 0.25f}, {cx + r.f(-R * 0.6f, R * 0.6f), cy + R * 0.34f} }, shade(rgb(130, 124, 112), r.f(0.8f, 1.1f)), 1.0f, 0.5f);
        a.ell(cx - R * 0.1f, cy + R * 0.3f, R * 0.4f, R * 0.12f, rgba(rgb(80, 104, 56), 140), 1, 0);   // grass creeping over the rubble
        break;
    }
    case 2: {   // standing stone
        float h = R * r.f(0.7f, 1.05f), w = R * r.f(0.18f, 0.26f);
        a.shadowCircle(cx, cy + R * 0.1f, R * 0.5f, 4, 5, 3, 85);
        Color c = shade(rgb(118, 114, 106), r.f(0.88f, 1.08f));
        a.poly({ {cx - w, cy + R * 0.25f}, {cx - w * 0.8f, cy + R * 0.25f - h}, {cx + w * 0.2f, cy + R * 0.25f - h - 2}, {cx + w, cy + R * 0.25f - h * 0.8f}, {cx + w * 1.1f, cy + R * 0.25f} }, c, w * 0.9f, 0.8f);
        a.line(cx - w * 0.2f, cy + R * 0.1f, cx + w * 0.1f, cy - h * 0.5f, 0.6f, rgba(rgb(46, 44, 40), 110));
        a.ell(cx, cy + R * 0.25f, w * 1.3f, w * 0.4f, rgba(rgb(78, 100, 56), 150), 1, 0);
        break;
    }
    case 3: {   // burnt-out vehicle hull: scorched, no turret, wheels gone
        float an = r.f(-0.8f, 0.8f), ca = std::cos(an), sa = std::sin(an);
        a.shadowCircle(cx, cy, R * 0.6f, 2, 3, 4, 70);
        a.ell(cx, cy, R * 0.95f, R * 0.7f, rgba(rgb(24, 20, 16), 70), 5, 0);
        auto rot = [&](float x, float y) { return P{ cx + x * ca - y * sa, cy + x * sa + y * ca }; };
        float hl = R * 0.8f, hw = R * 0.42f;
        a.poly({ rot(-hl, -hw), rot(hl * 0.85f, -hw), rot(hl, -hw * 0.4f), rot(hl, hw * 0.4f), rot(hl * 0.85f, hw), rot(-hl, hw) }, rgb(60, 56, 52), 3.0f, 0.7f);
        for (int s = -1; s <= 1; s += 2) { P p0 = rot(-hl, s * hw * 1.18f), p1 = rot(hl, s * hw * 1.18f); a.cap(p0.x, p0.y, p1.x, p1.y, hw * 0.22f, rgb(36, 34, 32), 1.2f, 0.5f); }
        P t = rot(-hl * 0.1f, 0); a.ell(t.x, t.y, hw * 0.75f, hw * 0.6f, rgb(40, 38, 36), 2.0f, 0.6f);
        P q = rot(hl * 0.3f, hw * 0.1f); a.ell(q.x, q.y, hw * 0.5f, hw * 0.4f, rgb(22, 20, 18), 1.0f, 0.3f);   // blown-out hatch
        for (int i = 0; i < 3; i++) { P u = rot(r.f(-hl, hl), r.f(-hw, hw)); a.dot(u.x, u.y, r.f(0.8f, 1.8f), rgb(120, 62, 30)); }   // rust
        break;
    }
    case 4: {   // lily pad (R ~ 5), a flower on some
        float rr = R * 0.7f, notch = r.f(0, 6.28f);
        Color g = shade(rgb(64, 130, 62), r.f(0.85f, 1.15f));
        a.shadowCircle(cx, cy, rr, 1, 1.5f, 1.5f, 60);
        std::vector<P> v; v.push_back({cx, cy});
        for (int i = 0; i <= 14; i++) { float t = notch + 0.35f + i * (6.28f - 0.7f) / 14.0f; v.push_back({cx + std::cos(t) * rr, cy + std::sin(t) * rr * 0.8f}); }
        a.poly(v, g, 1.4f, 0.5f);
        a.line(cx, cy, cx + std::cos(notch + 3.14f) * rr * 0.8f, cy + std::sin(notch + 3.14f) * rr * 0.6f, 0.4f, rgba(rgb(34, 84, 40), 160));
        if (r.f() < 0.4f) { a.dot(cx + 1, cy - 1, rr * 0.35f, rgb(246, 232, 236)); a.dot(cx + 1, cy - 1, rr * 0.16f, rgb(244, 200, 80)); }
        break;
    }
    case 5: {   // broken pillar on a square base
        a.shadowCircle(cx, cy + R * 0.1f, R * 0.5f, 4, 5, 3, 85);
        Color c = shade(rgb(166, 158, 142), r.f(0.9f, 1.08f));
        a.box(cx, cy + R * 0.16f, R * 0.3f, R * 0.12f, 1, shade(c, 0.9f), 1.6f, 0.6f);
        float h = R * r.f(0.35f, 0.7f);
        a.box(cx, cy + R * 0.05f - h * 0.5f, R * 0.17f, h * 0.5f + R * 0.06f, 1, c, 2.0f, 0.8f);
        a.poly({ {cx - R * 0.17f, cy + R * 0.05f - h}, {cx - R * 0.02f, cy + R * 0.05f - h - 3}, {cx + R * 0.1f, cy + R * 0.05f - h}, {cx + R * 0.17f, cy + R * 0.05f - h + 2}, {cx, cy + R * 0.05f - h + 3} }, shade(c, 1.1f), 1, 0.4f);
        a.poly({ {cx + R * 0.25f, cy + R * 0.2f}, {cx + R * 0.5f, cy + R * 0.18f}, {cx + R * 0.45f, cy + R * 0.3f} }, shade(c, 0.85f), 1, 0.5f);
        break;
    }
    case 7: {   // hay: two round bales with banded straw and a loose scatter of stalks
        Color straw = shade(rgb(214, 180, 96), r.f(0.9f, 1.08f));
        for (int i = 0; i < 2; i++) {
            float bx = cx + (i ? R * 0.4f : -R * 0.36f), by = cy + (i ? -R * 0.12f : R * 0.16f), br = R * (i ? 0.42f : 0.5f);
            a.shadowCircle(bx, by, br, 1.8f, 2.0f, 1.6f, 90);
            a.circle(bx, by, br, shade(straw, r.f(0.94f, 1.06f)), br * 0.35f, 0.55f);
            a.ring(bx, by, br * 0.62f, 0.45f, rgba(shade(straw, 0.8f), 170), 0.6f, 0.2f);
            a.ring(bx, by, br * 0.28f, 0.4f, rgba(shade(straw, 0.78f), 150), 0.5f, 0.2f);
            a.dot(bx - br * 0.25f, by - br * 0.25f, br * 0.2f, rgba(shade(straw, 1.25f), 210));
        }
        for (int i = 0; i < 9; i++) {
            float px = cx + r.f(-R * 0.8f, R * 0.8f), py = cy + r.f(-R * 0.5f, R * 0.6f), an = r.f(0, 3.14f);
            a.line(px, py, px + std::cos(an) * 2.0f, py + std::sin(an) * 1.0f, 0.5f, rgba(rgb(176, 140, 66), 200));
        }
        break;
    }
    case 8: {   // a short post-and-rail fence, one rail missing from the middle bay
        float an = r.f() < 0.5f ? r.f(-0.2f, 0.2f) : 1.5708f + r.f(-0.2f, 0.2f), ca = std::cos(an), sa = std::sin(an);
        const int n = 5;
        float half = R * 0.95f, step = 2 * half / (n - 1);
        auto at = [&](float t, float off) { return P{ cx + ca * t - sa * off, cy + sa * t + ca * off }; };
        Color wood = shade(rgb(152, 114, 74), r.f(0.9f, 1.08f));
        for (int k = 0; k < 2; k++) {
            float off = k ? 1.1f : -1.5f;
            for (int i = 0; i < n - 1; i++) {
                if (k == 0 && i == 2) continue;
                P p0 = at(-half + i * step + 1.5f, off), p1 = at(-half + (i + 1) * step - 1.5f, off);
                a.cap(p0.x, p0.y, p1.x, p1.y, 0.55f, shade(wood, k ? 1.12f : 0.86f), 0.5f, 0.4f);
            }
        }
        for (int i = 0; i < n; i++) {
            P p = at(-half + i * step, 0);
            a.shadowCircle(p.x, p.y, 1.5f, 1.4f, 1.6f, 1.0f, 90);
            a.box(p.x, p.y, 1.5f, 1.5f, 0.4f, shade(wood, 1.05f), 0.6f, 0.5f);
        }
        break;
    }
    case 9: {   // a wildflower patch: leaves underneath, then the blooms in one of four two-colour mixes
        static const Color SETS[4][2] = { { rgb(244, 214, 70), rgb(246, 244, 230) }, { rgb(176, 130, 220), rgb(240, 150, 170) },
                                          { rgb(214, 66, 58), rgb(246, 226, 150) }, { rgb(150, 190, 246), rgb(244, 214, 70) } };
        const Color* set = SETS[r.range(0, 3)];
        int n = 18 + r.range(0, 8);
        std::vector<P> pts;
        for (int i = 0; i < n; i++) { float an = r.f(0, 6.2832f), d = std::sqrt(r.f()) * R * 0.85f; pts.push_back(P{ cx + std::cos(an) * d, cy + std::sin(an) * d * 0.85f }); }
        a.shadowCircle(cx, cy, R * 0.75f, 1.5f, 2.0f, 3.0f, 40);
        for (auto& p : pts) a.dot(p.x + 0.4f, p.y + 0.6f, 1.5f, rgba(rgb(60, 92, 40), 170));
        for (auto& p : pts) {
            Color c = r.f() < 0.6f ? set[0] : set[1];
            a.dot(p.x, p.y, r.f(1.4f, 2.0f), shade(c, r.f(0.92f, 1.08f)));
            a.dot(p.x, p.y, 0.7f, rgba(rgb(236, 196, 74), 230));
        }
        break;
    }
    case 10: {   // a cluster of stones with moss on the shaded south-east lips and a few lichen flecks
        int n = 3 + r.range(0, 1);
        a.shadowCircle(cx, cy, R * 0.9f, 2.5f, 2.8f, 3.0f, 95);
        for (int i = 0; i < n; i++) {
            float an = r.f(0, 6.2832f), d = i ? R * r.f(0.25f, 0.5f) : 0.0f, rr = i ? R * r.f(0.26f, 0.4f) : R * r.f(0.5f, 0.6f);
            float bx = cx + std::cos(an) * d, by = cy + std::sin(an) * d * 0.8f;
            std::vector<P> v;
            for (int k = 0; k < 7; k++) { float t = k * 6.2832f / 7 + r.f(-0.2f, 0.2f), q = rr * r.f(0.8f, 1.0f); v.push_back(P{ bx + std::cos(t) * q, by + std::sin(t) * q * 0.85f }); }
            a.poly(v, shade(rgb(118, 112, 102), r.f(0.85f, 1.1f)), rr * 0.45f, 0.8f);
            a.ell(bx + rr * 0.35f, by + rr * 0.4f, rr * 0.5f, rr * 0.3f, rgba(rgb(70, 98, 46), 200), 1, 0);
        }
        for (int i = 0; i < 5; i++) a.dot(cx + r.f(-R * 0.5f, R * 0.5f), cy + r.f(-R * 0.4f, R * 0.4f), 0.8f, rgba(rgb(176, 178, 110), 200));
        break;
    }
    case 11: {   // an abandoned camp: a trampled patch, a dead fire ringed with stones, a pitched tent and a log seat
        float an = r.f(0, 6.2832f), ca = std::cos(an), sa = std::sin(an);
        Color tent = shade(rgb(150, 138, 96), r.f(0.9f, 1.08f));
        a.ell(cx, cy, R * 0.8f, R * 0.7f, rgba(rgb(120, 96, 66), 90), 1, 0);
        // the tent: a low canvas roof with its ridge running across the radius from the fire
        float tcx = cx + ca * R * 0.72f, tcy = cy + sa * R * 0.72f, ux = -sa, uy = ca, vx = ca, vy = sa;
        float hl = R * 0.5f, hw = R * 0.3f;
        auto T = [&](float u, float v) { return P{ tcx + ux * u + vx * v, tcy + uy * u + vy * v }; };
        std::vector<P> tp = { T(-hl, -hw), T(hl, -hw), T(hl, hw), T(-hl, hw) }, tsh;
        for (auto& p : tp) tsh.push_back(P{ p.x + 2.0f, p.y + 2.2f });
        a.poly(tsh, rgba(rgb(0, 0, 0), 80), 0.8f, 0.0f);
        a.poly(tp, tent, 0.8f, 0.5f);
        P e0 = T(-hl * 0.9f, 0), e1 = T(hl * 0.9f, 0);
        a.line(e0.x, e0.y, e1.x, e1.y, 0.8f, rgba(shade(tent, 0.7f), 220));
        // dead fire: an ash bed, two charred sticks and a ring of stones
        for (int i = 0; i < 7; i++) {
            float t = i * 6.2832f / 7 + r.f(-0.15f, 0.15f);
            a.circle(cx + std::cos(t) * R * 0.32f, cy + std::sin(t) * R * 0.32f, r.f(1.4f, 1.9f), shade(rgb(120, 116, 108), r.f(0.85f, 1.1f)), 0.8f, 0.6f);
        }
        a.ell(cx, cy, R * 0.22f, R * 0.2f, rgb(88, 84, 80), 1.2f, 0.2f);
        a.cap(cx - R * 0.16f, cy - R * 0.08f, cx + R * 0.14f, cy + R * 0.1f, 0.7f, rgb(46, 34, 26), 0.5f, 0.2f);
        a.cap(cx + R * 0.1f, cy - R * 0.12f, cx - R * 0.12f, cy + R * 0.08f, 0.6f, rgb(40, 30, 24), 0.5f, 0.2f);
        for (int i = 0; i < 6; i++) a.dot(cx + r.f(-R * 0.18f, R * 0.18f), cy + r.f(-R * 0.16f, R * 0.16f), 0.5f, rgba(rgb(150, 146, 138), 200));
        // a log seat on the far side of the fire, facing the tent
        float sx = cx - ca * R * 0.62f, sy = cy - sa * R * 0.62f;
        a.cap(sx - ux * R * 0.22f, sy - uy * R * 0.22f, sx + ux * R * 0.22f, sy + uy * R * 0.22f, R * 0.09f, rgb(120, 88, 56), 0.8f, 0.5f);
        break;
    }
    case 12: {   // a stack of supply crates (one stacked on another) and two barrels
        Color wood = shade(rgb(160, 122, 80), r.f(0.9f, 1.08f));
        auto crate = [&](float x, float y, float h) {
            a.shadowRect(x - h, y - h, h * 2, h * 2, 1.8f, 2.2f, 1.6f, 100);
            a.box(x, y, h, h, 0.5f, shade(wood, r.f(0.93f, 1.07f)), 0.7f, 0.5f);
            a.line(x - h * 0.85f, y - h * 0.85f, x + h * 0.85f, y + h * 0.85f, 0.45f, rgba(rgb(96, 68, 42), 170));
            a.line(x - h * 0.85f, y + h * 0.85f, x + h * 0.85f, y - h * 0.85f, 0.45f, rgba(rgb(96, 68, 42), 170));
        };
        auto barrel = [&](float x, float y, float rad) {
            a.shadowCircle(x, y, rad, 1.5f, 2.0f, 1.4f, 100);
            a.circle(x, y, rad, shade(rgb(124, 84, 54), r.f(0.92f, 1.08f)), rad * 0.4f, 0.6f);
            a.ring(x, y, rad * 0.62f, 0.4f, rgba(rgb(52, 38, 26), 200), 0.5f, 0.2f);
            a.dot(x - rad * 0.3f, y - rad * 0.3f, rad * 0.28f, rgba(rgb(176, 130, 90), 220));
        };
        crate(cx - R * 0.3f, cy + R * 0.15f, R * 0.26f);
        crate(cx + R * 0.38f, cy - R * 0.1f, R * 0.22f);
        crate(cx - R * 0.3f - 1.0f, cy + R * 0.15f - 1.6f, R * 0.2f);   // stacked on the first
        barrel(cx + R * 0.2f, cy + R * 0.6f, R * 0.2f);
        barrel(cx - R * 0.72f, cy - R * 0.42f, R * 0.2f);
        break;
    }
    case 13: {   // sun-bleached driftwood: a long pale trunk, a snapped branch stub and a knot
        float an = r.f(0, 6.2832f), L = R * 1.05f, ca = std::cos(an), sa = std::sin(an), th = R * 0.15f;
        float ax = cx - ca * L, ay = cy - sa * L, bx = cx + ca * L, by = cy + sa * L;
        a.cap(ax + 1.5f, ay + 2.0f, bx + 1.5f, by + 2.0f, th, rgba(rgb(0, 0, 0), 60), 0.5f, 0.0f);
        Color wood = shade(rgb(196, 184, 160), r.f(0.9f, 1.05f));
        a.cap(ax, ay, bx, by, th, wood, th * 0.7f, 0.6f);
        float t = r.f(0.3f, 0.7f), px = ax + (bx - ax) * t, py = ay + (by - ay) * t, side = r.f() < 0.5f ? -1.0f : 1.0f;
        a.cap(px, py, px - sa * side * R * 0.42f + ca * R * 0.18f, py + ca * side * R * 0.42f + sa * R * 0.18f, th * 0.45f, shade(wood, 0.95f), 0.4f, 0.5f);
        float k2 = 1.0f - t;
        a.dot(ax + (bx - ax) * k2, ay + (by - ay) * k2, th * 0.3f, rgba(rgb(122, 104, 82), 220));
        break;
    }
    case 14: {   // a rowboat drawn up on the sand: pointed bow, planked inside, two thwarts and an oar lying alongside
        float an = r.f(0, 6.2832f), L = R * 1.0f, w = R * 0.27f, ca = std::cos(an), sa = std::sin(an);
        auto W2 = [&](float u, float v) { return P{ cx + ca * u - sa * v, cy + sa * u + ca * v }; };   // along the keel (u), across it (v)
        std::vector<P> hull = { W2(-L * 0.92f, -w), W2(L * 0.5f, -w), W2(L, 0), W2(L * 0.5f, w), W2(-L * 0.92f, w) }, hsh;
        for (auto& p : hull) hsh.push_back(P{ p.x + 2.0f, p.y + 2.4f });
        a.poly(hsh, rgba(rgb(0, 0, 0), 70), 0.8f, 0.0f);
        a.poly(hull, shade(rgb(146, 96, 58), r.f(0.92f, 1.08f)), 1.4f, 0.5f);
        a.poly({ W2(-L * 0.75f, -w * 0.6f), W2(L * 0.45f, -w * 0.6f), W2(L * 0.8f, 0), W2(L * 0.45f, w * 0.6f), W2(-L * 0.75f, w * 0.6f) }, rgb(184, 140, 92), 0.6f, 0.3f);
        float thw[2] = { -L * 0.42f, L * 0.12f };
        for (float u : thw) { P p0 = W2(u, -w * 0.6f), p1 = W2(u, w * 0.6f); a.cap(p0.x, p0.y, p1.x, p1.y, 0.45f, rgb(108, 74, 44), 0.4f, 0.3f); }
        P o0 = W2(-L * 0.6f, -w * 1.7f), o1 = W2(L * 0.85f, -w * 1.7f);
        a.cap(o0.x, o0.y, o1.x, o1.y, 0.5f, rgb(120, 92, 62), 0.4f, 0.3f);
        break;
    }
    case 15: {   // a wooden signpost: two arrow boards pointing different ways from one post
        float an = r.f(0, 6.2832f);
        auto board = [&](float ang, float len) {
            float ca2 = std::cos(ang), sa2 = std::sin(ang), hw = R * 0.13f, head = R * 0.22f;
            auto W2 = [&](float u, float v) { return P{ cx + ca2 * u - sa2 * v, cy + sa2 * u + ca2 * v }; };
            std::vector<P> v = { W2(R * 0.15f, -hw), W2(len - head, -hw), W2(len, 0), W2(len - head, hw), W2(R * 0.15f, hw) }, sh;
            for (auto& p : v) sh.push_back(P{ p.x + 1.6f, p.y + 2.0f });
            a.poly(sh, rgba(rgb(0, 0, 0), 80), 0.6f, 0.0f);
            a.poly(v, shade(rgb(184, 140, 90), r.f(0.92f, 1.06f)), 0.6f, 0.4f);
            P s0 = W2(R * 0.3f, 0), s1 = W2(len - head - 1.0f, 0);
            a.line(s0.x, s0.y, s1.x, s1.y, 0.45f, rgba(rgb(96, 64, 38), 190));
        };
        board(an, R * 0.95f);
        board(an + 2.5f + r.f(-0.2f, 0.2f), R * 0.8f);
        a.shadowCircle(cx, cy, 1.6f, 1.2f, 1.8f, 1.0f, 100);
        a.circle(cx, cy, 1.6f, rgb(110, 80, 50), 0.6f, 0.6f);
        break;
    }
    case 16: {   // a clump of cattails: green blades and dark brown heads on tall stalks, leaning a little
        int n = 5 + r.range(0, 3);
        float lean = r.f(-0.35f, 0.35f);
        a.shadowCircle(cx, cy, R * 0.45f, 1.2f, 1.6f, 2.0f, 60);
        for (int i = 0; i < n; i++) {
            float bx = cx + r.f(-R * 0.5f, R * 0.5f), by = cy + r.f(-R * 0.15f, R * 0.25f), len = R * r.f(0.7f, 1.0f);
            float tx = bx + lean * len * 0.3f, ty = by - len;
            a.cap(bx, by, bx + r.f(-0.25f, 0.25f) * len, by - len * 0.55f, 0.35f, rgb(70, 104, 50), 0.3f, 0.3f);   // a blade
            a.cap(bx, by, tx, ty, 0.3f, rgb(84, 122, 58), 0.3f, 0.3f);
            float hl = len * r.f(0.22f, 0.3f);
            a.cap(tx, ty + len * 0.15f, tx, ty + len * 0.15f + hl, 0.7f, rgb(96, 62, 34), 0.5f, 0.4f);   // the seed head
        }
        break;
    }
    case 17: {   // a fairy ring of toadstools on the grass, some red with white spots, some brown
        int n = 7 + r.range(0, 3);
        Color capA = rgb(176, 56, 44), capB = rgb(150, 112, 70);
        a.shadowCircle(cx, cy, R * 0.9f, 1.5f, 2.0f, 4.0f, 30);
        for (int i = 0; i < n; i++) {
            float an = i * 6.2832f / n + r.f(-0.15f, 0.15f), d = R * r.f(0.72f, 0.92f);
            float mx = cx + std::cos(an) * d, my = cy + std::sin(an) * d * 0.9f, mr = r.f(1.8f, 2.6f);
            bool red = r.f() < 0.5f;
            a.cap(mx, my, mx + 0.4f, my + mr * 0.8f, mr * 0.32f, rgb(230, 220, 196), 0.4f, 0.3f);
            a.ell(mx, my - 0.2f, mr, mr * 0.72f, shade(red ? capA : capB, r.f(0.92f, 1.08f)), mr * 0.45f, 0.6f);
            if (red) for (int k = 0; k < 2; k++) a.dot(mx + r.f(-mr * 0.5f, mr * 0.5f), my - 0.2f + r.f(-mr * 0.4f, mr * 0.3f), 0.45f, rgba(rgb(246, 240, 228), 230));
        }
        break;
    }
    case 18: {   // the bones of a large beast, bleached: a long leg bone with knuckle ends, a skull and a rib
        float an = r.f(0, 6.2832f), ca = std::cos(an), sa = std::sin(an);
        Color bone = shade(rgb(222, 212, 186), r.f(0.9f, 1.05f));
        auto rot = [&](float x, float y) { return P{ cx + x * ca - y * sa, cy + x * sa + y * ca }; };
        P b0 = rot(-R * 0.72f, -R * 0.18f), b1 = rot(R * 0.3f, R * 0.22f);
        a.cap(b0.x + 1.5f, b0.y + 2.0f, b1.x + 1.5f, b1.y + 2.0f, R * 0.1f, rgba(rgb(0, 0, 0), 60), 0.5f, 0.0f);
        a.cap(b0.x, b0.y, b1.x, b1.y, R * 0.085f, bone, 0.5f, 0.4f);
        a.circle(b0.x, b0.y, R * 0.17f, bone, 0.6f, 0.4f);
        a.circle(b1.x, b1.y, R * 0.17f, bone, 0.6f, 0.4f);
        P s = rot(R * 0.5f, -R * 0.32f);
        a.ell(s.x, s.y, R * 0.3f, R * 0.25f, bone, 0.8f, 0.5f);
        P e0 = rot(R * 0.58f, -R * 0.42f), e1 = rot(R * 0.58f, -R * 0.22f);
        a.dot(e0.x, e0.y, R * 0.07f, rgb(58, 50, 42));
        a.dot(e1.x, e1.y, R * 0.07f, rgb(58, 50, 42));
        P k0 = rot(-R * 0.22f, -R * 0.36f), k1 = rot(-R * 0.1f, -R * 0.82f), k2 = rot(R * 0.12f, -R * 0.86f);
        a.cap(k0.x, k0.y, k1.x, k1.y, R * 0.06f, bone, 0.4f, 0.4f);
        a.cap(k1.x, k1.y, k2.x, k2.y, R * 0.06f, bone, 0.4f, 0.4f);
        break;
    }
    case 19: {   // a strip of ripe wheat on tilled soil: rows of ears, a green row every third furrow
        float hw = R * 1.0f, hh = R * 0.56f;
        Color soil = shade(rgb(126, 96, 62), r.f(0.92f, 1.06f));
        a.shadowRect(cx - hw, cy - hh, hw * 2, hh * 2, 2.0f, 2.4f, 2.2f, 90);
        a.rect(cx - hw, cy - hh, hw * 2, hh * 2, 0.6f, soil, 1.0f, 0.3f);
        for (int i = 0; i < 7; i++) {
            float y = cy - hh + (i + 0.5f) * (hh * 2 / 7.0f);
            Color ear = i % 3 == 2 ? rgb(116, 146, 66) : rgb(214, 182, 96);
            a.line(cx - hw + 1.2f, y, cx + hw - 1.2f, y, 0.9f, rgba(shade(soil, 0.8f), 150));
            for (float x = cx - hw + 1.6f; x < cx + hw - 1.0f; x += 1.7f) a.dot(x + r.f(-0.3f, 0.3f), y + r.f(-0.4f, 0.4f), 0.6f + r.f(0, 0.35f), shade(ear, r.f(0.88f, 1.12f)));
        }
        break;
    }
    default: {   // stump
        a.shadowCircle(cx, cy, R * 0.5f, 2, 3, 3, 70);
        a.ell(cx, cy + 1, R * 0.4f, R * 0.3f, rgb(84, 60, 40), 2, 0.7f);
        a.ell(cx, cy - 1.2f, R * 0.36f, R * 0.26f, rgb(188, 154, 108), 1, 0.3f);
        a.ring(cx, cy - 1.2f, R * 0.2f, 0.5f, rgba(rgb(120, 90, 58), 180), 0.5f, 0);
        break;
    }
    }
    return downsample2(big);
}

struct Tree { float x, y, r; int kind; u32 seed; int tile = -1; bool gone = false; };   // tile: the map tile it grows on (a structure built there clears it); gone: felled

// What the bake leaves behind for local repaints (see terrainRepaint): the world as it was before the trees were drawn, every tree and bush,
// and the tree sprites. A structure placed over trees then only has to recompose a small rectangle.
struct Kept {
    // RGB565 (half the memory of the 32-bit world), kept in PRE_CHUNK px squares, and of each square only the pixels a repaint can ever change: the
    // ones some tree's or bush's sprite covers, and the feathered ground round small rock that a structure may quarry. Everywhere else the ground
    // under the trees is the ground as it is, so a repaint leaves those pixels alone (it marks them clear: see terrainRepaint). A bit per pixel says
    // whether it is kept; the kept pixels lie packed row by row (the open meadows and the sea keep next to nothing).
    static const int PRE_CHUNK = 256;
    struct Sq { std::vector<u64> bits; std::vector<u32> rowAt; std::vector<u16> px; };   // bits: 4 words a row; rowAt: the index of each row's first kept pixel
    std::vector<Sq> preTree;
    int preCols = 0;
    u16* pre(int x, int y) {
        if (x < 0 || y < 0 || x >= WORLD_W || y >= WORLD_H) return nullptr;
        Sq& q = preTree[(size_t)(y / PRE_CHUNK) * preCols + x / PRE_CHUNK];
        if (q.bits.empty()) return nullptr;
        const int lx = x % PRE_CHUNK, ly = y % PRE_CHUNK, w = lx >> 6, b = lx & 63;
        const u64* row = &q.bits[(size_t)ly * 4];
        if (!(row[w] >> b & 1)) return nullptr;
        u32 k = q.rowAt[ly];
        for (int i = 0; i < w; i++) k += (u32)__builtin_popcountll(row[i]);
        if (b) k += (u32)__builtin_popcountll(row[w] & ((1ull << b) - 1));
        return &q.px[k];
    }
    std::vector<Tree> trees;                // sorted by y (back to front)
    std::vector<int> tileHead, tileNext;    // the trees and bushes growing on each tile: a list per tile through trees
    std::vector<Canvas> sprites[T_KINDS];
    bool ok = false;
};
Kept& kept() { static Kept k; return k; }
// RGB565 with ordered dither: each channel is rounded to its 5 or 6 bit step against a 4x4 Bayer threshold taken from the pixel's world position.
// A smooth gradient then steps as a fine pattern instead of hard bands, and the mean colour is kept (the rounding is unbiased). A pixel's
// threshold depends only on its coordinates, so the pre-tree chunks and the repainted patches dither exactly alike.
// (the steps are scaled as the expansion in from565 reads them back: 31 and 63 steps across 255, so the mean colour survives a round trip)
inline u16 to565(Color c, int x, int y) {
    static const u8 BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    float u = (BAYER[(y & 3) * 4 + (x & 3)] + 0.5f) / 16.0f;   // threshold in (0, 1)
    int r = std::min(31, (int)(c.r * (31.0f / 255.0f) + u)), g = std::min(63, (int)(c.g * (63.0f / 255.0f) + u)), b = std::min(31, (int)(c.b * (31.0f / 255.0f) + u));
    return (u16)(r << 11 | g << 5 | b);
}
inline Color from565(u16 v) { int r = v >> 11, g = (v >> 5) & 63, b = v & 31; return Color{ (u8)(r << 3 | r >> 2), (u8)(g << 2 | g >> 4), (u8)(b << 3 | b >> 2), 255 }; }

float spriteBase(int kind) { return kind == T_BUSH ? 8.5f : (kind == T_CONIFER ? 15.0f : 16.5f); }
// One tree into 'dst', whose pixel (0,0) is world pixel (ox, oy)
void drawTree(Canvas& dst, const Tree& t, int ox, int oy) {
    const Canvas& s = kept().sprites[t.kind][t.seed % kept().sprites[t.kind].size()];
    float sc = t.r / spriteBase(t.kind);
    if (std::abs(sc - 1.0f) < 0.18f) { blitOver(dst, s, (int)(t.x - s.w * 0.5f) - ox, (int)(t.y - s.h * 0.5f) - oy); return; }
    // scale by nearest sampling for the odd size
    int nw = std::max(4, (int)(s.w * sc)), nh = std::max(4, (int)(s.h * sc));
    Canvas sc2(nw, nh);
    for (int y = 0; y < nh; y++) for (int x = 0; x < nw; x++) sc2.px[y * nw + x] = s.px[std::min(s.h - 1, (int)(y / sc)) * s.w + std::min(s.w - 1, (int)(x / sc))];
    blitOver(dst, sc2, (int)(t.x - nw * 0.5f) - ox, (int)(t.y - nh * 0.5f) - oy);
}
// The pixel rectangle a tree's sprite covers (canopy and its shadow), generously
void treeRect(const Tree& t, int& x0, int& y0, int& x1, int& y1) {
    const Canvas& s = kept().sprites[t.kind][t.seed % kept().sprites[t.kind].size()];
    float sc = t.r / spriteBase(t.kind);
    if (std::abs(sc - 1.0f) < 0.18f) sc = 1.0f;
    float hw = s.w * sc * 0.5f + 2, hh = s.h * sc * 0.5f + 2;
    x0 = (int)(t.x - hw) - 1; y0 = (int)(t.y - hh) - 1; x1 = (int)(t.x + hw) + 2; y1 = (int)(t.y + hh) + 2;
}
// The final colour grade of the bake (broad light variation and the split-tone curve), for repainted ground
F3 gradeGround(F3 col, float x, float y, const Noise& nz) {
    col = mul3(col, (0.95f + 0.10f * nz.v(x * 0.006f + 3, y * 0.006f + 4)) * rollShade(x, y, nz));
    float vr = col.r / 255.0f, vg = col.g / 255.0f, vb = col.b / 255.0f;
    gradeRGB(vr, vg, vb);
    return f3((int)clampf(vr * 255.0f, 0, 255), (int)clampf(vg * 255.0f, 0, 255), (int)clampf(vb * 255.0f, 0, 255));
}

}  // namespace

// ================================================================= bake
static std::vector<u8> g_waterCover; static int g_wcW = 0, g_wcH = 0; static u32 g_wcStamp = 0;
const std::vector<u8>& terrainWaterCover(int& w, int& h, u32& stamp) { w = g_wcW; h = g_wcH; stamp = g_wcStamp; return g_waterCover; }

namespace {
// The world is painted one band of rows at a time (TERRAIN_BAND tall, a chunk row of the renderer): a 32-bit copy of the whole world was the
// biggest allocation of the game (420 MB on the 320 map, 600 MB on a bigger one) and lived only for the bake. A band is drawn in world
// coordinates; every write outside its rows is dropped, exactly as the whole-world canvas dropped writes outside the world, so the bands
// come out pixel for pixel what the single canvas did.
struct Band {
    Canvas* c = nullptr; int oy = 0;
    void set(int x, int y, Color col) { if (y >= oy && y < oy + c->h) c->set(x, y - oy, col); }
    u32* at(int x, int y) { return (x >= 0 && x < c->w && y >= oy && y < oy + c->h) ? &c->px[(size_t)(y - oy) * c->w + x] : nullptr; }
    void fillCircle(float cx, float cy, float r, Color col) {   // (Canvas::fillCircle's own loops, in world rows)
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++) {
            if (j < oy || j >= oy + c->h) continue;
            for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
                float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
                if (dx * dx + dy * dy <= r * r) c->set(i, j - oy, col);
            }
        }
    }
    void blit(const Canvas& s, int x0, int y0) { blitOver(*c, s, x0, y0 - oy); }
};
// The same calls doing nothing: the planning pass runs every routine that draws random numbers through it, so each stream advances exactly
// as it does while painting and a band can replay any item from the generator state saved for it.
struct NoBand {
    void set(int, int, Color) {}
    void fillCircle(float, float, float, Color) {}
    void blit(const Canvas&, int, int) {}
};

// A piece of ground clutter planned once for the whole world and painted by every band it reaches (in the planned order: where two pieces
// overlap, the later one is on top, as before). kind: what it is; markers (D_ROADC ...) stand for a whole deterministic pass at that point
// of the order. rs: the generator as it was when the piece started drawing; y0..y1: the world rows it can touch.
enum DKind : u8 { D_TUFT = 0, D_FLOWER, D_PEBBLE, D_REEDS, D_PAD, D_CRATER, D_BLIT, D_MARKERS, D_ROADC = D_MARKERS, D_ROADE, D_APRONS, D_BRIDGES };
struct DItem { u8 kind; int a; float x, y, r; Rng rs; int y0, y1; const Canvas* sp; };

template <class B> void drawTuft(B& b, Rng& r, float x, float y) {
    int blades = r.range(3, 5);
    bool lightTuft = r.f() < 0.5f;
    for (int k = 0; k < blades; k++) {
        float an = -1.5708f + (k - (blades - 1) * 0.5f) * 0.42f + r.f(-0.1f, 0.1f), l = r.f(3, 6);
        Color c = lightTuft ? rgb(150, 178, 82) : rgb(36, 66, 30); c.a = 120;
        int steps = (int)l;
        for (int s = 0; s <= steps; s++) b.set((int)(x + std::cos(an) * s), (int)(y + std::sin(an) * s), c);
    }
}
template <class B> void drawFlowers(B& b, Rng& r, float x, float y, Color c) {
    for (int k = 0; k < 3; k++) { float fx = x + r.f(-5, 5), fy = y + r.f(-5, 5); b.set((int)fx, (int)fy, c); b.set((int)fx + 1, (int)fy, shade(c, 0.85f)); b.set((int)fx, (int)fy + 1, rgb(60, 90, 40, 140)); }
}
template <class B> void drawPebble(B& b, Rng& r, float x, float y, bool sand) {
    float s = r.f(1.2f, 3.0f); Color c = shade(sand ? rgb(170, 154, 120) : rgb(150, 132, 108), r.f(0.8f, 1.15f));
    b.fillCircle(x + 1, y + 1, s, rgb(30, 24, 18, 90));
    b.fillCircle(x, y, s, c);
    b.fillCircle(x - s * 0.3f, y - s * 0.3f, s * 0.45f, shade(c, 1.3f));
}
template <class B> void drawReeds(B& b, Rng& r, int tx, int ty) {
    for (int k = 0; k < 10; k++) {   // reeds: twice as thick
        float x = tx * TILE + r.f(2, TILE - 2), y = ty * TILE + r.f(2, TILE - 2);
        int h = r.range(5, 10);
        for (int s = 0; s < h; s++) b.set((int)(x + s * 0.12f), (int)(y - s), s > h - 3 ? rgb(120, 78, 40) : rgb(70, 108, 52, 220));
    }
}
}  // namespace

void terrainBake(const std::function<void(const Canvas& band, int y0)>& emit) {
    // ONEHOUR_BAKETIME=1: where the bake's time goes (milliseconds per phase, summed over the bands)
    static const bool timing = getenv("ONEHOUR_BAKETIME") != nullptr;
    double tPh[12] = {}; u64 tMark = SDL_GetPerformanceCounter();
    auto phase = [&](int i) { if (!timing) return; u64 t = SDL_GetPerformanceCounter(); tPh[i] += (t - tMark) * 1000.0 / SDL_GetPerformanceFrequency(); tMark = t; };
    g_wcW = WORLD_W / 8; g_wcH = WORLD_H / 8; g_waterCover.assign((size_t)g_wcW * g_wcH, 0); g_wcStamp++;
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
    parallelRows(CH, [&](int y0, int y1) {
        for (int cy = y0; cy < y1; cy++) for (int cx = 0; cx < CW; cx++) mask[classOf(cx / 4, cy / 4)][(size_t)cy * CW + cx] = 1.0f;
    });
    std::vector<float> wide = mask[K_WATER];
    // ground the jungle has taken back (Map::overgrown: the relic bases, the jungle islands), softened like the classes so its edge is a curve
    std::vector<float> ogF((size_t)CW * CH, 0.0f);
    std::vector<u8> ogNear((size_t)MAP_W * MAP_H, 0);
    bool anyOg = false;
    for (int cy = 0; cy < CH; cy++) for (int cx = 0; cx < CW; cx++) if (g_map.overgrownAt(cx / 4, cy / 4)) { ogF[(size_t)cy * CW + cx] = 1.0f; anyOg = true; }
    if (anyOg) {
        blurBox(ogF, CW, CH, 4, 2);
        for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            bool n = false;
            for (int j = -2; j <= 2 && !n; j++) for (int i = -2; i <= 2; i++) if (g_map.overgrownAt(tx + i, ty + j)) { n = true; break; }
            ogNear[(size_t)ty * MAP_W + tx] = n;
        }
    }
    // three box passes (about 1.1 tiles of softness): a tile-stepped edge comes out as one smooth curve, not a staircase of tile steps
    // (roads keep a firmer edge, since their kerb is drawn at the boundary; the water keeps its old coverage so the shoreline stays where the water work expects it)
    for (int c = 0; c < K_N; c++) blurBox(mask[c], CW, CH, 4, c == K_ROAD ? 2 : 3);
    blurBox(wide, CW, CH, 13, 2);
    // tiles whose coverage can reach them (dilated by one tile so the border displacement never samples an unlisted class)
    std::vector<u8> present((size_t)MAP_W * MAP_H, 0), presentRaw((size_t)MAP_W * MAP_H, 0);
    parallelRows(MAP_H, [&](int y0, int y1) {
        for (int ty = y0; ty < y1; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            u8 bits = 0;
            for (int c = 0; c < K_N; c++) {
                bool any = false;
                for (int j = -1; j < 5 && !any; j++) for (int i = -1; i < 5; i++) { int cx = clampi(tx * 4 + i, 0, CW - 1), cy = clampi(ty * 4 + j, 0, CH - 1); if (mask[c][(size_t)cy * CW + cx] > 0.004f) { any = true; break; } }
                if (any) bits |= (u8)(1 << c);
            }
            presentRaw[(size_t)ty * MAP_W + tx] = bits;
        }
    });
    parallelRows(MAP_H, [&](int y0, int y1) {
        for (int ty = y0; ty < y1; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            u8 bits = 0;
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) bits |= presentRaw[(size_t)clampi(ty + j, 0, MAP_H - 1) * MAP_W + clampi(tx + i, 0, MAP_W - 1)];
            present[(size_t)ty * MAP_W + tx] = bits;
        }
    });

    // ---- domain warp on an 8 px lattice
    const int WG = W / CELL + 2;
    std::vector<float> warpX((size_t)WG * WG), warpY((size_t)WG * WG);
    for (int j = 0; j < WG; j++) for (int i = 0; i < WG; i++) {
        float x = (float)(i * CELL), y = (float)(j * CELL);
        // a broad wander (about half a tile either way) and a smaller mid-scale wobble, so no border runs straight for long
        warpX[(size_t)j * WG + i] = (nz.fbm(x * 0.02f, y * 0.02f) - 0.5f) * 36.0f + (nz.v(x * 0.07f + 3, y * 0.07f + 9) - 0.5f) * 5.0f;
        warpY[(size_t)j * WG + i] = (nz.fbm(x * 0.02f + 40, y * 0.02f + 40) - 0.5f) * 36.0f + (nz.v(x * 0.07f + 60, y * 0.07f + 22) - 0.5f) * 5.0f;
    }
    auto warpAt = [&](float x, float y, float& wx, float& wy) {
        float fx = x / CELL, fy = y / CELL; int ix = (int)fx, iy = (int)fy; float tx = fx - ix, ty = fy - iy;
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);   // eased lattice weights: the warp has no facets at the cell edges
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
                t.kind = zone > 0.55f ? (u < 0.8f ? T_CONIFER : T_BROAD) : (u < 0.11f ? T_AUTUMN : T_BROAD);
                t.r = (t.kind == T_CONIFER ? r.f(13, 17) : r.f(14, 19));
                t.seed = r.next();
                t.tile = ty * MAP_W + tx;
                trees.push_back(t);
            }
        }
        // bushes on open ground
        for (int ty = 2; ty < MAP_H - 2; ty++) for (int tx = 2; tx < MAP_W - 2; tx++) {
            int tl = g_map.tile(tx, ty);
            if (tl != T_GRASS && tl != T_GRASS2 && tl != T_DIRT) continue;
            const bool og = g_map.overgrownAt(tx, ty);   // (the jungle's undergrowth is thick)
            if (nz.fbm(tx * 0.21f + 70, ty * 0.21f + 20) < (og ? 0.40f : 0.62f) || r.f() > (og ? 0.75f : 0.42f)) continue;
            bool near = false;
            for (auto& s : g_map.starts) if (std::abs(tx - s.tx) < 8 && std::abs(ty - s.ty) < 8) near = true;
            for (auto& s : g_map.supplies) if (std::abs(tx - s.tx) < 3 && std::abs(ty - s.ty) < 3) near = true;
            if (near) continue;
            Tree t; t.x = tx * TILE + r.f(4, TILE - 4); t.y = ty * TILE + r.f(4, TILE - 4); t.kind = T_BUSH; t.r = r.f(7, 10); t.seed = r.next(); t.tile = ty * MAP_W + tx;
            trees.push_back(t);
        }
        std::sort(trees.begin(), trees.end(), [](const Tree& a, const Tree& b) { return a.y < b.y; });
    }

    // ---- distance from land over the water (tiles, chamfered): the open sea gets darker and cheaper to paint the further out it lies
    std::vector<float> seaD((size_t)MAP_W * MAP_H, 0.0f);
    {
        for (int i = 0; i < MAP_W * MAP_H; i++) seaD[i] = g_map.tiles[i] == T_WATER ? 1e6f : 0.0f;
        const float D1 = 1.0f, D2 = 1.4142f;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            float& v = seaD[(size_t)y * MAP_W + x];
            if (x > 0) v = std::min(v, seaD[(size_t)y * MAP_W + x - 1] + D1);
            if (y > 0) { v = std::min(v, seaD[(size_t)(y - 1) * MAP_W + x] + D1); if (x > 0) v = std::min(v, seaD[(size_t)(y - 1) * MAP_W + x - 1] + D2); if (x < MAP_W - 1) v = std::min(v, seaD[(size_t)(y - 1) * MAP_W + x + 1] + D2); }
        }
        for (int y = MAP_H - 1; y >= 0; y--) for (int x = MAP_W - 1; x >= 0; x--) {
            float& v = seaD[(size_t)y * MAP_W + x];
            if (x < MAP_W - 1) v = std::min(v, seaD[(size_t)y * MAP_W + x + 1] + D1);
            if (y < MAP_H - 1) { v = std::min(v, seaD[(size_t)(y + 1) * MAP_W + x] + D1); if (x < MAP_W - 1) v = std::min(v, seaD[(size_t)(y + 1) * MAP_W + x + 1] + D2); if (x > 0) v = std::min(v, seaD[(size_t)(y + 1) * MAP_W + x - 1] + D2); }
        }
    }

    phase(0);   // coverage fields, warp, trees, sea distance
    // ================= the plan: every random pass is run once over the whole world, in the order it is painted, without painting
    auto tileAt = [&](float x, float y) { return g_map.tile(clampi(tileOf(x), 0, MAP_W - 1), clampi(tileOf(y), 0, MAP_H - 1)); };
    auto openGround = [&](float x, float y) { int t = tileAt(x, y); return t == T_GRASS || t == T_GRASS2 || t == T_DIRT || t == T_SAND; };
    std::vector<DItem> items; items.reserve(1 << 16);
    auto item = [&](u8 kind, int a, float x, float y, float rr, const Rng& rs, int y0, int y1, const Canvas* sp = nullptr) { items.push_back(DItem{ kind, a, x, y, rr, rs, y0, y1, sp }); };
    auto marker = [&](u8 kind) { item(kind, 0, 0, 0, 0, Rng(1), 0, H, nullptr); };
    NoBand nb;
    static const Color FL[5] = { rgb(246, 244, 230), rgb(244, 214, 70), rgb(208, 168, 228), rgb(240, 150, 170), rgb(150, 190, 246) };
    Rng r(4242);
    // grass tufts
    for (int i = 0; i < decorCount(88000); i++) {   // grass tufts: twice the old density
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_GRASS && t != T_GRASS2) continue;
        if (nz.v(x * 0.02f + 9, y * 0.02f + 2) < 0.4f) continue;
        item(D_TUFT, 0, x, y, 0, r, (int)y - 8, (int)y + 2);
        drawTuft(nb, r, x, y);
    }
    // wildflowers in drifts
    for (int i = 0; i < decorCount(30000); i++) {   // wildflowers: twice the old density
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_GRASS && t != T_GRASS2) continue;
        float drift = nz.v(x * 0.011f + 50, y * 0.011f + 17);
        if (drift < 0.62f) continue;
        int fc = (int)(drift * 40) % 5;
        item(D_FLOWER, fc, x, y, 0, r, (int)y - 7, (int)y + 8);
        drawFlowers(nb, r, x, y, FL[fc]);
    }
    // pebbles on dirt and sand, shells on the shore
    for (int i = 0; i < decorCount(20000); i++) {   // pebbles: twice the old density
        float x = r.f(4, W - 4.0f), y = r.f(4, H - 4.0f);
        int t = tileAt(x, y);
        if (t != T_DIRT && t != T_SAND) continue;
        item(D_PEBBLE, t == T_SAND, x, y, 0, r, (int)y - 6, (int)y + 6);
        drawPebble(nb, r, x, y, t == T_SAND);
    }
    // reeds along the water's edge
    for (int ty = 1; ty < MAP_H - 1; ty++) for (int tx = 1; tx < MAP_W - 1; tx++) {
        if (g_map.tile(tx, ty) != T_SAND) continue;
        bool shore = g_map.tile(tx + 1, ty) == T_WATER || g_map.tile(tx - 1, ty) == T_WATER || g_map.tile(tx, ty + 1) == T_WATER || g_map.tile(tx, ty - 1) == T_WATER;
        if (!shore || seaShore(tx, ty)) continue;   // (reeds grow by fresh water, not on the sea's beaches)
        item(D_REEDS, 0, (float)tx, (float)ty, 0, r, ty * TILE - 12, (ty + 1) * TILE + 2);
        drawReeds(nb, r, tx, ty);
    }
    marker(D_ROADC);
    marker(D_ROADE);
    // supply piles stand on a gravel depot pad with churned earth and a ring of tyre tracks
    for (size_t k = 0; k < g_map.supplies.size(); k++) {
        const auto& sp = g_map.supplies[k];
        float cy = sp.ty * TILE + TILE * 0.5f;
        item(D_PAD, (int)k, 0, 0, 0, r, (int)cy - 52, (int)cy + 52);
        for (int i = 0; i < 40; i++) { r.f(); r.f(); r.f(); }   // (the 40 clods below: a bearing, a distance and a size each)
    }
    marker(D_APRONS);
    // craters from some older war: dark scorched bowls with a pale thrown-up rim, clustered along the old front lines
    {
        Rng cr(5150);
        int placedC = 0;
        for (int i = 0; i < 2600 && placedC < 30; i++) {
            float x = cr.f(60, W - 60.0f), y = cr.f(60, H - 60.0f);
            if (!openGround(x, y)) continue;
            float dc = hyp(x - W * 0.5f, y - H * 0.5f) / TILE;
            if (dc < 26 || dc > 46) continue;   // the band armies cross to reach the lake
            bool near = false;
            for (auto& s : g_map.starts) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < 10 * TILE) near = true;
            for (auto& s : g_map.supplies) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < 90) near = true;
            if (near) continue;
            float R = cr.f(13, 30);
            item(D_CRATER, 0, x, y, R, cr, (int)(y - R - 4) - 1, (int)(y + R + 4) + 1);
            placedC++;
        }
    }
    // ---- props (their sprites live until the bake is done: the plan points at them)
    std::vector<Canvas> logs, walls, stones, hulks, pads, pillars, stumps;
    std::vector<Canvas> hay, fence, wildflowers, mossy, camp, crates, driftwood, boats, signs, cattails, rings, bones, wheat;
    std::vector<Canvas> boulder;
    {
        for (int i = 0; i < 5; i++) logs.push_back(makeProp(0, 16.0f + i * 2, 7100u + i));
        for (int i = 0; i < 5; i++) walls.push_back(makeProp(1, 24.0f + i * 2, 7200u + i));
        for (int i = 0; i < 5; i++) stones.push_back(makeProp(2, 18.0f + i * 2, 7300u + i));
        for (int i = 0; i < 4; i++) hulks.push_back(makeProp(3, 20.0f + i * 2, 7400u + i));
        for (int i = 0; i < 6; i++) pads.push_back(makeProp(4, 7.0f + i * 0.8f, 7500u + i));
        for (int i = 0; i < 4; i++) pillars.push_back(makeProp(5, 24.0f + i * 2, 7600u + i));
        for (int i = 0; i < 4; i++) stumps.push_back(makeProp(6, 14.0f + i * 1.5f, 7700u + i));
        auto put = [&](const Canvas& c, float x, float y) { int x0 = (int)(x - c.w * 0.5f), y0 = (int)(y - c.h * 0.5f); item(D_BLIT, x0, x, y, 0, Rng(1), y0, y0 + c.h, &c); };
        auto clearOf = [&](float x, float y, float baseR, float supR) {
            for (auto& s : g_map.starts) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < baseR * TILE) return false;
            for (auto& s : g_map.supplies) if (dist(Vec2(x, y), tileCenter(s.tx, s.ty)) < supR) return false;
            return true;
        };
        auto flat = [&](float x, float y) {   // open ground with no rock, water, road or tree within a tile
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) { int t = tileAt(x + i * TILE, y + j * TILE); if (t != T_GRASS && t != T_GRASS2 && t != T_DIRT) return false; }
            return true;
        };
        Rng pr(9137);
        // lily pads on the river shallows (not the deep lake)
        for (int i = 0; i < decorCount(8400); i++) {
            float x = pr.f(8, W - 8.0f), y = pr.f(8, H - 8.0f);
            if (tileAt(x, y) != T_WATER || tileAt(x + 12, y) != T_WATER || tileAt(x - 12, y) != T_WATER || tileAt(x, y + 12) != T_WATER || tileAt(x, y - 12) != T_WATER) continue;
            {   // not out on the open lake: only its shallows next to the shore (rivers and pools anywhere)
                int ptx = clampi(tileOf(x), 0, MAP_W - 1), pty = clampi(tileOf(y), 0, MAP_H - 1);
                if (!g_map.freshWater(ptx, pty)) continue;   // (salt water: none on the sea)
                if (g_map.waterComp[pty * MAP_W + ptx] == g_map.lakeComp) {   // (the lake and the canal: only by their shores)
                    bool shore = false;
                    for (int j = -2; j <= 2 && !shore; j++) for (int i = -2; i <= 2; i++) if (inMap(ptx + i, pty + j) && g_map.tile(ptx + i, pty + j) != T_WATER) { shore = true; break; }
                    if (!shore) continue;
                }
            }
            if (nz.v(x * 0.012f + 31, y * 0.012f + 5) < 0.45f) continue;
            if (g_map.bridge[clampi(tileOf(y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(x), 0, MAP_W - 1)]) continue;
            put(pads[pr.range(0, 5)], x, y);
        }
        // fallen logs and stumps near the trees, logs on river banks
        int placed = 0;
        for (int i = 0; i < decorCount(13000) && placed < decorCount(280); i++) {
            float x = pr.f(40, W - 40.0f), y = pr.f(40, H - 40.0f);
            if (!flat(x, y) || !clearOf(x, y, 9, 90)) continue;
            bool nearTree = false;
            for (int j = -2; j <= 2 && !nearTree; j++) for (int k = -2; k <= 2; k++) if (tileAt(x + k * TILE, y + j * TILE) == T_TREE) { nearTree = true; break; }
            bool nearWater = false;
            for (int j = -3; j <= 3 && !nearWater; j++) for (int k = -3; k <= 3; k++) if (tileAt(x + k * TILE, y + j * TILE) == T_WATER) { nearWater = true; break; }
            if (!nearTree && !nearWater && pr.f() > 0.08f) continue;
            if (pr.f() < 0.35f) put(stumps[pr.range(0, 3)], x, y); else put(logs[pr.range(0, 4)], x, y);
            placed++;
        }
        // ruins, standing stones, and the husks of an old fight: scattered, mirrored where it counts for fairness
        struct Site { float tx, ty; int what; };
        static const Site SITES[] = {   // one quadrant; mirrored into the other three. what: 0 ruin, 1 stone circle, 2 hulks
            {24, 17, 0}, {30, 8.5f, 1}, {8, 22, 2}, {25, 31, 2}, {19, 47, 0}, {6.5f, 54, 1}
        };
        for (int q = 0; q < 4; q++) for (const Site& st : SITES) {
            float sx = ISLE_X0 + st.tx * (ISLE_W / 80.0f), sy = ISLE_X0 + st.ty * (ISLE_W / 80.0f);   // sites were placed for the old 80x80 map (now the main island)
            float tx = (q & 1) ? (MAP_W - 1) - sx : sx, ty = (q & 2) ? (MAP_H - 1) - sy : sy, cx = tx * TILE, cy = ty * TILE;
            if (!clearOf(cx, cy, 8, 80)) continue;
            Rng sr((u64)(q * 977 + st.tx * 31 + st.ty * 7));
            if (st.what == 0) {   // a ruined courtyard: wall runs round a square with pillar stumps at the corners
                int k = 0;
                for (int sd = 0; sd < 4; sd++) { float ax = sd < 2 ? (sd ? 1 : -1) : 0, ay = sd >= 2 ? (sd == 3 ? 1 : -1) : 0; float px = cx + ax * 52 * (sd < 2 ? 1 : 0) + (sd < 2 ? 0 : (sr.f() - 0.5f) * 30), py = cy + ay * 52 + (sd < 2 ? (sr.f() - 0.5f) * 30 : 0); if (openGround(px, py)) put(walls[k++ % 5], px, py); }
                for (int c4 = 0; c4 < 4; c4++) { float px = cx + ((c4 & 1) ? 56 : -56), py = cy + ((c4 & 2) ? 56 : -56); if (openGround(px, py) && sr.f() < 0.8f) put(pillars[sr.range(0, 3)], px, py); }
            } else if (st.what == 1) {   // stones in a ring
                int n = 7;
                for (int i = 0; i < n; i++) { float an = i * 6.2831853f / n + 0.3f, px = cx + std::cos(an) * 46, py = cy + std::sin(an) * 40; if (flat(px, py) && sr.f() < 0.88f) put(stones[sr.range(0, 4)], px, py); }
            } else {   // wrecks scattered on and beside the road
                for (int i = 0; i < 3; i++) { float px = cx + sr.f(-70, 70), py = cy + sr.f(-50, 50); int t = tileAt(px, py); if (t == T_WATER || t == T_ROCK || t == T_TREE) continue; put(hulks[sr.range(0, 3)], px, py); }
            }
        }
        // ---- more decor: thirteen more kinds, each scattered by density over the ground it belongs on (counts scale with the map's area)
        for (int i = 0; i < 4; i++) hay.push_back(makeProp(7, 11.0f + i * 1.5f, 8100u + i));
        for (int i = 0; i < 4; i++) fence.push_back(makeProp(8, 14.0f + i * 2.0f, 8200u + i));
        for (int i = 0; i < 4; i++) wildflowers.push_back(makeProp(9, 9.0f + i * 1.5f, 8300u + i));
        for (int i = 0; i < 3; i++) mossy.push_back(makeProp(10, 12.0f + i * 2.0f, 8400u + i));
        for (int i = 0; i < 3; i++) camp.push_back(makeProp(11, 16.0f + i * 2.0f, 8500u + i));
        for (int i = 0; i < 3; i++) crates.push_back(makeProp(12, 18.0f + i * 1.5f, 8600u + i));
        for (int i = 0; i < 3; i++) driftwood.push_back(makeProp(13, 12.0f + i * 2.0f, 8700u + i));
        for (int i = 0; i < 3; i++) boats.push_back(makeProp(14, 15.0f + i * 1.5f, 8800u + i));
        for (int i = 0; i < 3; i++) signs.push_back(makeProp(15, 14.0f + i * 1.5f, 8900u + i));
        for (int i = 0; i < 4; i++) cattails.push_back(makeProp(16, 10.0f + i * 1.5f, 9000u + i));
        for (int i = 0; i < 3; i++) rings.push_back(makeProp(17, 10.0f + i * 1.5f, 9100u + i));
        for (int i = 0; i < 3; i++) bones.push_back(makeProp(18, 12.0f + i * 2.0f, 9200u + i));
        for (int i = 0; i < 3; i++) wheat.push_back(makeProp(19, 14.0f + i * 2.0f, 9300u + i));
        Rng mr(6161);
        // a candidate spot is taken if it lies within minD of a spot already taken by the same kind
        auto spaced = [](const std::vector<P>& pts, float x, float y, float minD) { for (const P& p : pts) if (hyp(p.x - x, p.y - y) < minD) return false; return true; };
        auto scatter = [&](int attempts, int maxN, float minD, auto ok, auto place) {
            std::vector<P> got;
            for (int i = 0; i < attempts && (int)got.size() < maxN; i++) {
                float x = mr.f(40, W - 40.0f), y = mr.f(40, H - 40.0f);
                if (!ok(x, y) || !spaced(got, x, y, minD)) continue;
                got.push_back(P{ x, y });
                place(x, y);
            }
        };
        auto dry = [](int t) { return t == T_GRASS || t == T_GRASS2 || t == T_DIRT || t == T_SAND; };
        auto onLand = [&](float x, float y, int rad) {   // every tile within rad is dry ground: no water, road, rock or tree
            for (int j = -rad; j <= rad; j++) for (int i = -rad; i <= rad; i++) if (!dry(tileAt(x + i * TILE, y + j * TILE))) return false;
            return true;
        };
        auto nearTile = [&](float x, float y, int t, int rad) {
            for (int j = -rad; j <= rad; j++) for (int i = -rad; i <= rad; i++) if (tileAt(x + i * TILE, y + j * TILE) == t) return true;
            return false;
        };
        auto grassAt = [&](float x, float y) { int t = tileAt(x, y); return t == T_GRASS || t == T_GRASS2; };
        auto apart = [&](float x, float y) { return clearOf(x, y, 7, 70) && !g_map.overgrownAt(tileOf(x), tileOf(y)); };   // clear of the bases and supply piles (and of the jungle: no hay or fences in it)
        // hay and wheat lie in meadow drifts, flowers in their own drifts, the rest by what they stand on
        scatter(decorCount(9000), decorCount(90), 44, [&](float x, float y) { return grassAt(x, y) && flat(x, y) && apart(x, y) && nz.v(x * 0.008f + 61, y * 0.008f + 44) > 0.5f; },
                [&](float x, float y) { put(hay[mr.range(0, 3)], x, y); });
        scatter(decorCount(16000), decorCount(170), 30, [&](float x, float y) { return grassAt(x, y) && apart(x, y) && nz.v(x * 0.011f + 50, y * 0.011f + 17) > 0.6f; },
                [&](float x, float y) { put(wildflowers[mr.range(0, 3)], x, y); });
        scatter(decorCount(14000), decorCount(12), 130, [&](float x, float y) { return flat(x, y) && apart(x, y); },
                [&](float x, float y) { put(camp[mr.range(0, 2)], x, y); });
        scatter(decorCount(6000), decorCount(60), 60, [&](float x, float y) { return flat(x, y) && apart(x, y); },
                [&](float x, float y) { put(fence[mr.range(0, 3)], x, y); });
        scatter(decorCount(8000), decorCount(60), 48, [&](float x, float y) { return openGround(x, y) && nearTile(x, y, T_ROCK, 1) && apart(x, y); },
                [&](float x, float y) { put(mossy[mr.range(0, 2)], x, y); });
        scatter(decorCount(12000), decorCount(28), 60, [&](float x, float y) { return flat(x, y) && nearTile(x, y, T_ROAD, 2) && apart(x, y); },
                [&](float x, float y) { put(crates[mr.range(0, 2)], x, y); });
        scatter(decorCount(12000), decorCount(22), 90, [&](float x, float y) { return flat(x, y) && nearTile(x, y, T_ROAD, 2) && apart(x, y); },
                [&](float x, float y) { put(signs[mr.range(0, 2)], x, y); });
        scatter(decorCount(12000), decorCount(44), 36, [&](float x, float y) { return onLand(x, y, 1) && nearTile(x, y, T_WATER, 2) && apart(x, y); },
                [&](float x, float y) { put(driftwood[mr.range(0, 2)], x, y); });
        scatter(decorCount(12000), decorCount(10), 150, [&](float x, float y) { return tileAt(x, y) == T_SAND && onLand(x, y, 1) && nearTile(x, y, T_WATER, 2) && apart(x, y); },
                [&](float x, float y) { put(boats[mr.range(0, 2)], x, y); });
        scatter(decorCount(30000), decorCount(240), 18, [&](float x, float y) { return dry(tileAt(x, y)) && nearTile(x, y, T_WATER, 1) && !seaShore(tileOf(x), tileOf(y)) && apart(x, y); },
                [&](float x, float y) { put(cattails[mr.range(0, 3)], x, y); });
        scatter(decorCount(20000), decorCount(60), 60, [&](float x, float y) { return grassAt(x, y) && flat(x, y) && nearTile(x, y, T_TREE, 2) && apart(x, y); },
                [&](float x, float y) { put(rings[mr.range(0, 2)], x, y); });
        scatter(decorCount(12000), decorCount(26), 90, [&](float x, float y) { return flat(x, y) && apart(x, y); },
                [&](float x, float y) { put(bones[mr.range(0, 2)], x, y); });
        scatter(decorCount(16000), decorCount(30), 110, [&](float x, float y) { return grassAt(x, y) && flat(x, y) && apart(x, y) && nz.v(x * 0.006f + 12, y * 0.006f + 71) > 0.55f; },
                [&](float x, float y) { put(wheat[mr.range(0, 2)], x, y); });

        // ---- boulders: scattered where open ground meets rock, plus a few strays
        for (int i = 0; i < 6; i++) boulder.push_back(makeBoulder(5.0f + i * 1.6f, 900u + i));
        for (int i = 0; i < decorCount(2300); i++) {
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
            const Canvas& bc = boulder[v];
            int x0 = (int)(x - bc.w * 0.5f), y0 = (int)(y - bc.h * 0.5f);
            item(D_BLIT, x0, x, y, 0, Rng(1), y0, y0 + bc.h, &bc);
        }
    }

    phase(1);   // the plan
    // ---- what the bake keeps for local repaints: the tree sprites, the tree list, and the ground before the trees (land squares only)
    Kept& K = kept();
    if (K.sprites[0].empty()) for (int k = 0; k < T_KINDS; k++) for (int v = 0; v < 5; v++) K.sprites[k].push_back(makeTree(k, spriteBase(k) + v * 0.6f, 5000u + k * 97u + v * 13u));
    const int PC = Kept::PRE_CHUNK, pcols = (W + PC - 1) / PC, prows = (H + PC - 1) / PC, TPC = PC / TILE;
    K.preCols = pcols;
    K.preTree.assign((size_t)pcols * prows, Kept::Sq());
    static float maxHalf[T_KINDS] = {};   // the widest sprite of each kind, for a cheap "can this tree reach the band" bound
    if (maxHalf[0] <= 0) for (int k = 0; k < T_KINDS; k++) for (auto& sp : K.sprites[k]) maxHalf[k] = std::max(maxHalf[k], std::max(sp.w, sp.h) * 0.5f);
    float maxReach = 0;   // (the farthest any tree's rectangle reaches from its centre)
    for (int k = 0; k < T_KINDS; k++) maxReach = std::max(maxReach, maxHalf[k] * 1.4f + 8.0f);

    // ================= the bands
    const int BAND = TERRAIN_BAND;
    Canvas bandC(W, std::min(BAND, H));
    std::vector<float> hband;
    std::vector<u8> reliefB, rawB;
    for (int oy = 0; oy < H; oy += BAND) {
        const int bh = std::min(BAND, H - oy);
        if (bandC.h != bh) bandC = Canvas(W, bh);
        Band band; band.c = &bandC; band.oy = oy;

        // ---- height field rows of this band (half resolution: rock massifs, lake beds, canopies), with a margin of two tiles above and below
        // (the soft shadows look up to 27 px north-west, and the relief mask spreads two tiles)
        const int hb0 = std::max(0, oy / 2 - 32), hb1 = std::min(HH, (oy + bh) / 2 + 32), hbn = hb1 - hb0;
        hband.assign((size_t)HW * hbn, 0.0f);
        parallelRows(hbn, [&](int y0, int y1) {
            for (int hyl = y0; hyl < y1; hyl++) for (int hx = 0; hx < HW; hx++) {
                const int hy = hb0 + hyl;
                float x = hx * 2.0f, y = hy * 2.0f, wx, wy; warpAt(x, y, wx, wy);
                float fx = (x + wx) / CELL - 0.5f, fy = (y + wy) / CELL - 0.5f;
                int tx = clampi((int)x >> 5, 0, MAP_W - 1), ty = clampi((int)y >> 5, 0, MAP_H - 1);
                u8 bits = present[(size_t)ty * MAP_W + tx];
                float h = 0;
                if (bits & (1 << K_ROCK)) {
                    float mr = samp(mask[K_ROCK], CW, CH, fx, fy);
                    float e = (nz.v(x * 0.06f + 7 * 31.7f, y * 0.06f + 7 * 17.3f) - 0.5f) * 0.28f;   // (a slower wobble: the cliff foot reads as one line, not teeth)
                    float a = sstep(0.38f, 0.74f, mr + e);
                    float ridge = 1.0f - std::abs(2.0f * nz.fbm(x * 0.045f + 11, y * 0.045f + 3) - 1.0f);
                    h += a * (14.0f + 22.0f * ridge * ridge + 4.0f * nz.v(x * 0.05f, y * 0.05f));
                }
                if (bits & (1 << K_WATER)) h -= 3.5f * sstep(0.5f, 0.95f, samp(wide, CW, CH, fx, fy));
                hband[(size_t)hyl * HW + hx] = h;
            }
        });
        for (auto& t : trees) {
            if ((t.y + t.r) / 2 < hb0 - 1 || (t.y - t.r) / 2 > hb1 + 1) continue;
            float hgt = t.kind == T_BUSH ? 7.0f : (t.kind == T_CONIFER ? 24.0f : 20.0f);
            int hx0 = std::max(0, (int)((t.x - t.r) / 2)), hx1 = std::min(HW - 1, (int)((t.x + t.r) / 2));
            int hy0 = std::max(hb0, std::max(0, (int)((t.y - t.r) / 2))), hy1 = std::min(hb1 - 1, std::min(HH - 1, (int)((t.y + t.r) / 2)));
            for (int hy = hy0; hy <= hy1; hy++) for (int hx = hx0; hx <= hx1; hx++) {
                float d = hyp(hx * 2.0f - t.x, hy * 2.0f - t.y) / t.r;
                if (d >= 1) continue;
                float h = hgt * std::sqrt(1 - d * d);
                float& cell = hband[(size_t)(hy - hb0) * HW + hx];
                cell = std::max(cell, h);
            }
        }
        phase(2);   // height field
        // tiles with any relief (dilated) get the lighting pass; flat meadow skips it (worked out for this band's tile rows)
        const int ty0 = oy / TILE, ty1 = std::min(MAP_H, (oy + bh + TILE - 1) / TILE), rt0 = hb0 * 2 / TILE, rt1 = std::min(MAP_H, (hb1 * 2 + TILE - 1) / TILE);
        rawB.assign((size_t)MAP_W * (rt1 - rt0), 0);
        for (int hyl = 0; hyl < hbn; hyl++) for (int hx = 0; hx < HW; hx++) if (std::abs(hband[(size_t)hyl * HW + hx]) > 0.05f) rawB[(size_t)((hb0 + hyl) * 2 / TILE - rt0) * MAP_W + hx * 2 / TILE] = 1;
        reliefB.assign((size_t)MAP_W * (ty1 - ty0), 0);
        for (int ty = ty0; ty < ty1; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            u8 v = 0;
            for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) { int ry = clampi(ty + j, 0, MAP_H - 1); if (ry >= rt0 && ry < rt1) v |= rawB[(size_t)(ry - rt0) * MAP_W + clampi(tx + i, 0, MAP_W - 1)]; }
            reliefB[(size_t)(ty - ty0) * MAP_W + tx] = v;
        }
        auto hsamp = [&](float hx, float hy) { return samp(hband, HW, hbn, hx, hy - hb0); };

        // ---- per-pixel ground
        parallelRows(bh, [&](int y0, int y1) {
            for (int yl = y0; yl < y1; yl++) {
                const int y = oy + yl;
                u32* row = &bandC.px[(size_t)yl * W];
                for (int x = 0; x < W; x++) {
                    int tx = x >> 5, ty = y >> 5;
                    u8 bits = present[(size_t)ty * MAP_W + tx];
                    float sea = bits == (1 << K_WATER) ? samp(seaD, MAP_W, MAP_H, x / (float)TILE - 0.5f, y / (float)TILE - 0.5f) : 0.0f;
                    if (sea >= SEA_OPEN) {   // open sea: nothing else reaches it, no relief, and the shallows' detail has faded out (see groundColor)
                        if ((x & 7) == 4 && (y & 7) == 4) g_waterCover[(size_t)(y >> 3) * g_wcW + (x >> 3)] = 255;
                        F3 col = mul3(openSeaColor((float)x, (float)y, nz, sea), 0.95f + 0.10f * nz.v(x * 0.006f + 3, y * 0.006f + 4));
                        float vr = col.r / 255.0f, vg = col.g / 255.0f, vb = col.b / 255.0f;
                        gradeRGB(vr, vg, vb);
                        row[x] = Canvas::pack(Color{ (u8)clampf(vr * 255.0f, 0, 255), (u8)clampf(vg * 255.0f, 0, 255), (u8)clampf(vb * 255.0f, 0, 255), 255 });
                        continue;
                    }
                    float wox, woy; warpAt((float)x, (float)y, wox, woy);
                    float fx = (x + wox) / CELL - 0.5f, fy = (y + woy) / CELL - 0.5f;
                    float w[K_N]; float sum = 0, bestV = -1; int best = 0;
                    for (int c = 0; c < K_N; c++) {
                        if (!(bits >> c & 1)) { w[c] = 0; continue; }
                        float m = samp(mask[c], CW, CH, fx, fy);
                        if (m > bestV) { bestV = m; best = c; }
                        float mm = m;
                        // the border's wobble: a mid-scale waver (about 16 px) and a finer one, not the 8 px static that made the edges jagged
                        if (m > 0.02f && m < 0.98f) mm += (c == K_ROAD ? 0.35f : 1.0f) * ((nz.v(x * 0.06f + c * 31.7f, y * 0.06f + c * 17.3f) - 0.5f) * 0.30f + (nz.v(x * 0.21f + c * 7.0f, y * 0.21f + c * 3.0f) - 0.5f) * 0.10f);   // (a road's edge stays nearly straight)
                        float s = std::max(mm, 0.0f); s *= s; s *= s;
                        w[c] = s; sum += s;
                    }
                    if (sum < 1e-6f) { w[best] = 1; sum = 1; }
                    float macro = (bits & ((1 << K_GRASS) | (1 << K_GRASS2))) ? nz.fbm(x * 0.012f, y * 0.012f) : 0.0f;   // (only the meadows read it)
                    float depth = 0;
                    if (w[K_WATER] > 0) depth = sstep(0.5f, 0.95f, samp(wide, CW, CH, fx, fy));
                    F3 col{0, 0, 0};
                    float inv = 1.0f / sum;
                    Base base = baseAt(nz, (float)x, (float)y);   // (shared by every class that blends in here)
                    for (int c = 0; c < K_N; c++) if (w[c] > 0.003f) col = add3(col, mul3(classColor(c, (float)x, (float)y, nz, base, macro, depth, sea), w[c] * inv));
                    float wWater = w[K_WATER] * inv, wRoad = w[K_ROAD] * inv, wRock = w[K_ROCK] * inv;
                    if ((x & 7) == 4 && (y & 7) == 4) g_waterCover[(size_t)(y >> 3) * g_wcW + (x >> 3)] = (u8)clampf(wWater * 255.0f + 0.5f, 0, 255);
                    // scree aprons: pale rubble shed at the foot of every cliff, strongest where rock coverage is just giving out
                    if (wRock > 0.03f && wRock < 0.5f && wWater < 0.05f) col = lerp3(col, SCREE, (0.5f - wRock) * 0.6f * (0.65f + 0.35f * nz.v(x * 0.4f + 3, y * 0.4f + 8)));
                    // ---- the beach profile, land side: dry sand, then a darker soaked band at the waterline with a bright rim of dried salt
                    if (wWater > 0.005f && wWater < 0.5f) {
                        // how far the tide has soaked in, as a band whose width wanders: the wet sand is a real band, not a gradient
                        float wob = 0.5f + 0.5f * std::sin(x * 0.045f + y * 0.028f + 5.0f * nz.v(x * 0.01f + 2, y * 0.01f + 7));
                        float wet = sstep(0.30f + 0.10f * wob, 0.02f, wWater);
                        col = lerp3(col, SAND_WET, wet * 0.62f);
                        col = mul3(col, 1.0f - 0.16f * wet);                              // wet sand is darker, not just greyer
                        // a bright crust of dried salt a little back from the water
                        float crust = sstep(0.30f, 0.20f, wWater) * sstep(0.10f, 0.20f, wWater);
                        col = lerp3(col, f3(238, 228, 198), crust * 0.30f);
                    }
                    // ---- the waterline itself: a shore of lighter, aerated water (the surf) with its own broken foam line
                    if (wWater > 0.5f && wWater < 0.995f) {
                        float edge = sstep(0.995f, 0.5f, wWater);
                        // the surf: shallow water still breaking, pale turquoise and whitened
                        col = lerp3(col, lerp3(col, f3(150, 226, 214), 0.55f), edge * 0.75f);
                        float foam = sstep(0.30f, 0.95f, edge) * (0.35f + 0.65f * nz.v(x * 0.30f + 5, y * 0.30f + 9));
                        foam *= 0.55f + 0.45f * std::sin(x * 0.16f + y * 0.10f + 6.0f * nz.v(x * 0.02f, y * 0.02f));
                        col = lerp3(col, f3(232, 242, 240), clampf(foam * 0.85f, 0, 0.78f));
                    }
                    // gravel shoulder where a road meets the ground, a darker kerb just inside it
                    if (wRoad > 0.05f && wRoad < 0.95f) {
                        float edge = 1.0f - std::abs(wRoad - 0.5f) * 2.0f;
                        float band2 = edge * edge * edge;   // a narrow band on the boundary itself
                        if (wRoad < 0.5f) col = lerp3(col, GRAVEL, band2 * 0.42f * (0.6f + 0.4f * nz.v(x * 0.3f, y * 0.3f)));
                        else col = mul3(col, 1.0f - 0.24f * band2);   // the kerb: the road's edge runs darker
                    }
                    // the jungle floor where the jungle has taken the ground back: moss, fallen leaves and deep shade between the roots
                    if (ogNear[(size_t)ty * MAP_W + tx]) {
                        float og = samp(ogF, CW, CH, fx, fy) * (1.0f - wWater) * (1.0f - wRoad * 0.6f);
                        if (og > 0.01f) {
                            float lv = nz.v(x * 0.09f + 41, y * 0.09f + 7), ms = nz.v(x * 0.035f + 13, y * 0.035f + 88);
                            F3 floor = lerp3(lerp3(FOREST_A, MOSS, sstep(0.35f, 0.75f, ms)), LEAF_LITTER, sstep(0.55f, 0.85f, lv) * 0.55f);
                            floor = mul3(floor, 0.82f + 0.3f * nz.v(x * 0.4f + 5, y * 0.4f + 2));
                            col = lerp3(col, floor, clampf(og * 0.62f, 0, 0.62f));
                        }
                    }
                    // broad light variation, and the roll of the land (it fades out over water)
                    col = mul3(col, (0.95f + 0.10f * nz.v(x * 0.006f + 3, y * 0.006f + 4)) * (1.0f + (rollShade((float)x, (float)y, nz) - 1.0f) * (1.0f - clampf(w[K_WATER] / std::max(sum, 1e-4f), 0, 1))));
                    // relief lighting from the height field
                    if (reliefB[(size_t)(ty - ty0) * MAP_W + tx]) {
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
                        k = 1.0f + (k - 1.0f) * 1.6f;   // north-west faces up, south-east faces down: the cliffs read as lit from the NW
                        const float shade = clampf(k, 0.42f, 1.7f) * (1.0f - 0.34f * occ);
                        col = mul3(col, shade);
                        if (shade < 1.0f && wRock > 0.02f) col = add3(col, mul3(f3(30, 44, 66), (1.0f - shade) * wRock));   // the sky lights what the sun does not: a cliff's shaded face is blue slate, not a black hole
                        // moss in the cracks at the foot of cliffs, a pale dry rim on the crests
                        if (w[K_ROCK] > 0.4f * sum) { col = lerp3(col, MOSS, (1.0f - rockish) * 0.3f); col = add3(col, mul3(f3(30, 28, 24), clampf((h0 - 28.0f) / 14.0f, 0, 1))); }
                    }
                    // split-tone grade: a whisper of S-curve, a touch more saturation, warm highlights and cool shadows (one sunlight for the whole map)
                    {
                        float vr = col.r / 255.0f, vg = col.g / 255.0f, vb = col.b / 255.0f;
                        gradeRGB(vr, vg, vb);
                        col = f3((int)clampf(vr * 255.0f, 0, 255), (int)clampf(vg * 255.0f, 0, 255), (int)clampf(vb * 255.0f, 0, 255));
                    }
                    Color o{ (u8)clampf(col.r, 0, 255), (u8)clampf(col.g, 0, 255), (u8)clampf(col.b, 0, 255), 255 };
                    row[x] = Canvas::pack(o);
                }
            }
        });

        phase(3);   // relief and ground
        // ---- the planned clutter that reaches this band, in order (and the deterministic passes where they stood)
        for (const DItem& it : items) {
            if (it.kind < D_MARKERS && (it.y1 < oy || it.y0 >= oy + bh)) continue;
            Rng rr = it.rs;
            switch (it.kind) {
            case D_TUFT: drawTuft(band, rr, it.x, it.y); break;
            case D_FLOWER: drawFlowers(band, rr, it.x, it.y, FL[it.a]); break;
            case D_PEBBLE: drawPebble(band, rr, it.x, it.y, it.a != 0); break;
            case D_REEDS: drawReeds(band, rr, (int)it.x, (int)it.y); break;
            case D_BLIT: band.blit(*it.sp, it.a, it.y0); break;
            case D_PAD: {
                const auto& sp = g_map.supplies[it.a];
                float cx = sp.tx * TILE + TILE * 0.5f, cy = sp.ty * TILE + TILE * 0.5f;
                for (int y = (int)(cy - 26); y < (int)(cy + 26); y++) for (int x = (int)(cx - 26); x < (int)(cx + 26); x++) {
                    u32* d = band.at(x, y);
                    if (!inMapPx(x, y) || !d) continue;
                    float e = std::max(std::abs(x - cx), std::abs(y - cy)) / 26.0f;
                    float k = clampf((1.0f - e) * 3.0f, 0, 1) * 0.5f;
                    if (k <= 0) continue;
                    Color o = Canvas::unpack(*d);
                    Color pad = shade(rgb(150, 142, 124), 0.92f + 0.16f * nz.v(x * 0.5f, y * 0.5f));
                    bool seam = ((x - (int)cx) % 13 == 0) || ((y - (int)cy) % 13 == 0);
                    if (seam) pad = shade(pad, 0.82f);
                    *d = Canvas::pack(Color{(u8)(o.r + (pad.r - o.r) * k), (u8)(o.g + (pad.g - o.g) * k), (u8)(o.b + (pad.b - o.b) * k), 255});
                }
                for (int i = 0; i < 40; i++) { float an = rr.f(0, 6.28f), d = rr.f(10, 44); band.fillCircle(cx + std::cos(an) * d, cy + std::sin(an) * d * 0.8f, rr.f(1.5f, 4), rgb(70, 54, 36, 80)); }
                for (int k = 0; k < 2; k++) for (float a = 0; a < 6.2f; a += 0.05f) { float rr2 = 34 + k * 5; band.set((int)(cx + std::cos(a) * rr2), (int)(cy + std::sin(a) * rr2 * 0.8f), rgb(60, 46, 32, 70)); }
                break;
            }
            case D_CRATER: {
                const float x = it.x, y = it.y, R = it.r;
                for (int yy = (int)(y - R - 4); yy < (int)(y + R + 4); yy++) {
                    if (yy < oy || yy >= oy + bh) continue;
                    for (int xx = (int)(x - R - 4); xx < (int)(x + R + 4); xx++) {
                        if (!inMapPx(xx, yy)) continue;
                        float d = hyp(xx - x, (yy - y) * 1.2f) / R;
                        if (d > 1.25f) continue;
                        u32* px = band.at(xx, yy);
                        Color o = Canvas::unpack(*px);
                        float nse = 0.85f + 0.3f * nz.v(xx * 0.3f + 9, yy * 0.3f);
                        Color c = o;
                        if (d < 0.72f) { float k = (0.72f - d) * 1.1f; c = shade(mix(o, rgb(38, 30, 24), clampf(k, 0, 0.8f)), nse); }        // scorched bowl
                        else if (d < 1.0f) { float k = (d - 0.72f) * 2.2f; c = shade(mix(o, rgb(168, 148, 118), clampf(k, 0, 0.55f)), nse); } // thrown-up rim
                        else { float k = (1.25f - d) * 0.9f; c = mix(o, rgb(120, 102, 80), clampf(k, 0, 0.3f)); }                              // dust apron
                        if (xx - x + (yy - y) > R * 0.55f && d >= 0.72f && d < 1.0f) c = shade(c, 0.82f);                                        // rim shadow to the south-east
                        *px = Canvas::pack(Color{c.r, c.g, c.b, 255});
                    }
                }
                break;
            }
            case D_ROADC:   // road centre lines: dashes along every segment, only where the tile is still asphalt
                for (const auto& rs : g_map.roads) {
                    float x0 = rs.x0 * TILE, y0 = rs.y0 * TILE, x1 = rs.x1 * TILE, y1 = rs.y1 * TILE;
                    if (std::max(y0, y1) + 4 < oy || std::min(y0, y1) - 4 >= oy + bh) continue;
                    float len = hyp(x1 - x0, y1 - y0); if (len < 1) continue;
                    float dx = (x1 - x0) / len, dy = (y1 - y0) / len;
                    for (float s = 20; s < len - 20; s += 34) {
                        for (float u = 0; u < 15; u += 0.7f) {
                            float px = x0 + dx * (s + u), py = y0 + dy * (s + u);
                            if (py < oy - 4 || py >= oy + bh + 4) continue;
                            if (tileAt(px, py) != T_ROAD) continue;
                            // worn paint: a dull cream that greys and breaks up in patches where the traffic has ground it down
                            float wear = nz.v(px * 0.45f + 4, py * 0.45f + 19);
                            if (wear < 0.2f) continue;
                            Color paint = wear > 0.6f ? rgb(206, 194, 150) : rgb(184, 176, 144);
                            for (int wdt = -1; wdt <= 1; wdt++) band.set((int)(px - dy * wdt), (int)(py + dx * wdt), rgba(paint, wdt == 0 ? (wear > 0.55f ? 190 : 150) : 96));
                        }
                    }
                }
                break;
            case D_ROADE:   // road edge lines: a pale worn line just inside each shoulder (skipped where repair patches and dirt have eaten it)
                for (const auto& rs : g_map.roads) {
                    float x0 = rs.x0 * TILE, y0 = rs.y0 * TILE, x1 = rs.x1 * TILE, y1 = rs.y1 * TILE;
                    float off = rs.halfW * TILE * 0.62f;   // inside the asphalt core: coverage-blended tile borders already read as dirt
                    if (std::max(y0, y1) + off + 4 < oy || std::min(y0, y1) - off - 4 >= oy + bh) continue;
                    float len = hyp(x1 - x0, y1 - y0); if (len < 1) continue;
                    float dx = (x1 - x0) / len, dy = (y1 - y0) / len;
                    for (float s = 6; s < len - 6; s += 1.4f) {
                        if (nz.v(s * 0.05f + x0 * 0.01f, y0 * 0.01f) < 0.18f) continue;   // worn away in stretches
                        for (int sd = -1; sd <= 1; sd += 2) {
                            float px = x0 + dx * s - dy * off * sd, py = y0 + dy * s + dx * off * sd;
                            if (py < oy - 2 || py >= oy + bh + 2) continue;
                            if (tileAt(px, py) != T_ROAD || g_map.bridge[clampi(tileOf(py), 0, MAP_H - 1) * MAP_W + clampi(tileOf(px), 0, MAP_W - 1)]) continue;
                            band.set((int)px, (int)py, rgb(206, 204, 192, 110));
                        }
                    }
                }
                break;
            case D_APRONS:   // each base sits on prepared ground: a faint gravel apron under the start position, so a headquarters reads as founded, not parked
                for (const auto& st : g_map.starts) {
                    float cx = st.tx * TILE + TILE * 0.5f, cy = st.ty * TILE + TILE * 0.5f;
                    const float R = 5.2f * TILE;
                    for (int y = (int)(cy - R); y < (int)(cy + R); y++) {
                        if (y < oy || y >= oy + bh) continue;
                        for (int x = (int)(cx - R); x < (int)(cx + R); x++) {
                            if (!inMapPx(x, y)) continue;
                            float d = hyp(x - cx, (y - cy) * 1.15f) / R;
                            if (d >= 1) continue;
                            float k = (1 - d) * (1 - d) * 0.30f;
                            u32* px = band.at(x, y);
                            Color o = Canvas::unpack(*px);
                            Color pad = shade(rgb(144, 138, 122), 0.94f + 0.12f * nz.v(x * 0.35f + 4, y * 0.35f));
                            *px = Canvas::pack(Color{(u8)(o.r + (pad.r - o.r) * k), (u8)(o.g + (pad.g - o.g) * k), (u8)(o.b + (pad.b - o.b) * k), 255});
                        }
                    }
                }
                break;
            case D_BRIDGES: {   // bridges: timber decks over the rivers with rails, posts and a shadow on the water below
                const int tA = std::max(1, ty0 - 1), tB = std::min(MAP_H - 1, ty1 + 1);
                for (int ty = tA; ty < tB; ty++) for (int tx = 1; tx < MAP_W - 1; tx++) {
                    if (!g_map.bridge[ty * MAP_W + tx]) continue;
                    int x0 = tx * TILE, y0 = ty * TILE;
                    bool runsEW = g_map.bridge[ty * MAP_W + tx - 1] || g_map.bridge[ty * MAP_W + tx + 1];   // deck direction, for the wheel-worn runner strips
                    for (int y = 0; y < TILE; y++) for (int x = 0; x < TILE; x++) {   // planks
                        float pl = nz.v((x0 + x) * 0.9f, (y0 + y) * 0.1f), g = 0.86f + 0.22f * pl;
                        bool seamH = (y % 8) == 0, seamV = ((x0 + x + (((y0 + y) / 8) & 1) * 12) % 24) == 0;
                        int lane = runsEW ? y : x;
                        bool runner = (lane >= 7 && lane <= 10) || (lane >= 21 && lane <= 24);   // twin dark tracks where wheels and treads polish the timber
                        Color c = shade(rgb(142, 108, 74), g * (seamH || seamV ? 0.6f : 1.0f) * (runner ? 0.78f : 1.0f));
                        band.set(x0 + x, y0 + y, c);
                    }
                }
                for (int ty = tA; ty < tB; ty++) for (int tx = 1; tx < MAP_W - 1; tx++) {
                    if (!g_map.bridge[ty * MAP_W + tx]) continue;
                    int x0 = tx * TILE, y0 = ty * TILE;
                    auto off = [&](int ox, int oy2) { return !g_map.bridge[(ty + oy2) * MAP_W + tx + ox] && g_map.tile(tx + ox, ty + oy2) == T_WATER; };
                    struct Side { int ox, oy; };
                    static const Side SD[4] = { {0, -1}, {0, 1}, {-1, 0}, {1, 0} };
                    for (int sdI = 0; sdI < 4; sdI++) {
                        if (!off(SD[sdI].ox, SD[sdI].oy)) continue;
                        bool horiz = SD[sdI].oy != 0;
                        for (int u = 0; u < TILE; u++) {   // rail: a dark beam with a lit top edge, drop shadow outwards
                            for (int t = 0; t < 4; t++) {
                                int px = horiz ? x0 + u : x0 + (SD[sdI].ox < 0 ? t : TILE - 1 - t), py = horiz ? y0 + (SD[sdI].oy < 0 ? t : TILE - 1 - t) : y0 + u;
                                band.set(px, py, t == 0 ? rgb(176, 140, 98) : (t == 3 ? rgb(70, 50, 34) : rgb(104, 76, 52)));
                            }
                            for (int t = 4; t < 9; t++) {
                                int px = horiz ? x0 + u : x0 + (SD[sdI].ox < 0 ? -(t - 3) : TILE - 1 + (t - 3)), py = horiz ? y0 + (SD[sdI].oy < 0 ? -(t - 3) : TILE - 1 + (t - 3)) : y0 + u;
                                u32* d = band.at(px, py);
                                if (inMapPx(px, py) && d) { Color p = Canvas::unpack(*d); *d = Canvas::pack(shade(p, 1.0f - 0.35f * (1.0f - (t - 4) / 5.0f))); }
                            }
                            if (u % 10 == 4) for (int dt = 0; dt < 5; dt++) for (int dw = 0; dw < 3; dw++) {   // posts
                                int px = horiz ? x0 + u + dw : x0 + (SD[sdI].ox < 0 ? dt - 1 : TILE - 4 + dt), py = horiz ? y0 + (SD[sdI].oy < 0 ? dt - 1 : TILE - 4 + dt) : y0 + u + dw;
                                band.set(px, py, dw == 0 ? rgb(170, 134, 94) : rgb(86, 62, 42));
                            }
                        }
                    }
                }
                break;
            }
            default: break;
            }
        }

        phase(4);   // clutter
        // ---- the ground before the trees, kept (RGB565) for the land squares of this band: only the pixels under a tree's or a bush's sprite and round
        // the small rock a structure may quarry (see Kept)
        {
            const int sr0 = oy / PC, sr1 = std::min(prows, (oy + bh + PC - 1) / PC);
            parallelRows(sr1 - sr0, [&](int a0, int a1) {
                std::vector<u64> bits;
                for (int cy = sr0 + a0; cy < sr0 + a1; cy++) {
                    const int py0 = cy * PC, py1 = std::min(H, py0 + PC);
                    // the trees that can reach this row of squares (the list is sorted by y)
                    const float reach = maxReach;
                    auto lo = std::lower_bound(trees.begin(), trees.end(), py0 - reach, [](const Tree& t, float v) { return t.y < v; });
                    for (int cx = 0; cx < pcols; cx++) {
                        bool land = false;
                        for (int ty = cy * TPC - 1; ty <= (cy + 1) * TPC && !land; ty++) for (int tx = cx * TPC - 1; tx <= (cx + 1) * TPC; tx++)
                            if (inMap(tx, ty) && g_map.tile(tx, ty) != T_WATER) { land = true; break; }
                        if (!land) continue;
                        const int px0 = cx * PC, px1 = std::min(W, px0 + PC);
                        bits.assign((size_t)PC * 4, 0);
                        auto mark = [&](int x0, int y0, int x1, int y1) {   // (half-open, world px)
                            x0 = std::max(x0, px0); y0 = std::max(y0, py0); x1 = std::min(x1, px1); y1 = std::min(y1, py1);
                            for (int y = y0; y < y1; y++) for (int x = x0; x < x1; x++) bits[(size_t)(y - py0) * 4 + ((x - px0) >> 6)] |= 1ull << ((x - px0) & 63);
                        };
                        bool any = false;
                        for (auto it = lo; it != trees.end() && it->y - reach < py1; ++it) {
                            int a, b, c, d; treeRect(*it, a, b, c, d);
                            if (c < px0 || a >= px1 || d < py0 || b >= py1) continue;
                            mark(a, b, c + 1, d + 1); any = true;
                        }
                        for (int ty = cy * TPC - 1; ty <= (cy + 1) * TPC; ty++) for (int tx = cx * TPC - 1; tx <= (cx + 1) * TPC; tx++) {
                            if (!inMap(tx, ty) || g_map.clearable[ty * MAP_W + tx] != 2) continue;   // small rock: a quarry feathers 14 px round it
                            mark(tx * TILE - 18, ty * TILE - 18, (tx + 1) * TILE + 18, (ty + 1) * TILE + 18); any = true;
                        }
                        if (!any) continue;
                        Kept::Sq& q = K.preTree[(size_t)cy * pcols + cx];
                        q.bits = bits; q.rowAt.assign(PC, 0); q.px.clear();
                        for (int y = 0; y < PC; y++) {
                            q.rowAt[y] = (u32)q.px.size();
                            const int wy = py0 + y;
                            if (wy >= H) continue;
                            for (int x = 0; x < PC && px0 + x < W; x++) if (bits[(size_t)y * 4 + (x >> 6)] >> (x & 63) & 1) q.px.push_back(to565(Canvas::unpack(bandC.px[(size_t)(wy - oy) * W + px0 + x]), px0 + x, wy));
                        }
                        q.px.shrink_to_fit();
                    }
                }
            });
        }
        phase(5);   // pre-tree copy
        // ---- trees, back to front
        for (auto& t : trees) {
            float m = maxHalf[t.kind] * std::max(1.0f, t.r / spriteBase(t.kind)) + 6.0f;
            if (t.y + m < oy || t.y - m >= oy + bh) continue;
            drawTree(bandC, t, 0, oy);
        }
        phase(6);   // trees
        emit(bandC, oy);
        phase(7);   // the caller's grading
    }
    K.trees = trees;
    K.tileHead.assign((size_t)MAP_W * MAP_H, -1); K.tileNext.assign(K.trees.size(), -1);
    for (int i = (int)K.trees.size() - 1; i >= 0; i--) { int t = K.trees[i].tile; if (t >= 0 && t < MAP_W * MAP_H) { K.tileNext[i] = K.tileHead[t]; K.tileHead[t] = i; } }
    K.ok = true;
    if (timing) printf("bake ms: fields %.0f, plan %.0f, height %.0f, ground %.0f, clutter %.0f, pre-tree %.0f, trees %.0f, grade %.0f\n", tPh[0], tPh[1], tPh[2], tPh[3], tPh[4], tPh[5], tPh[6], tPh[7]);
}

bool terrainRepaint(const std::vector<int>& cleared, int& rx, int& ry, int& rw, int& rh, std::vector<u32>& out) {
    Kept& K = kept();
    if (!K.ok || cleared.empty()) return false;
    const Noise& nz = NZ();
    // everything here is local to the footprint: its tiles, the trees growing on them (each tile's list), the rock quarried, and the trees whose
    // sprites reach the rectangle (a range of the y-sorted list) - a full-map pass per footprint was most of the first frame of a match
    int x0 = 1 << 30, y0 = 1 << 30, x1 = -1, y1 = -1;
    auto grow = [&](int a, int b, int c, int d) { x0 = std::min(x0, a); y0 = std::min(y0, b); x1 = std::max(x1, c); y1 = std::max(y1, d); };
    bool felled = false;
    std::vector<int> quarried;
    for (int c : cleared) {
        int k = c & 0xFFFFFF, kind = c >> 24;
        if (k < 0 || k >= MAP_W * MAP_H) continue;
        if (kind == 2) quarried.push_back(k);
        for (int ti = K.tileHead.empty() ? -1 : K.tileHead[k]; ti >= 0; ti = K.tileNext[ti]) {   // the trees and bushes that grew on the footprint go
            Tree& t = K.trees[ti];
            if (t.gone) continue;
            t.gone = true; felled = true;
            int a, b, cc, d; treeRect(t, a, b, cc, d); grow(a, b, cc, d);
        }
    }
    std::sort(quarried.begin(), quarried.end()); quarried.erase(std::unique(quarried.begin(), quarried.end()), quarried.end());
    // quarried rock becomes bare earth, feathered into what is round it
    const int FEATHER = 14;
    bool rock = !quarried.empty();
    if (rock) {
        int qx0 = 1 << 30, qy0 = 1 << 30, qx1 = -1, qy1 = -1;
        for (int i : quarried) { int tx = i % MAP_W, ty = i / MAP_W; qx0 = std::min(qx0, tx); qy0 = std::min(qy0, ty); qx1 = std::max(qx1, tx); qy1 = std::max(qy1, ty); }
        int px0 = std::max(0, qx0 * TILE - FEATHER), py0 = std::max(0, qy0 * TILE - FEATHER);
        int px1 = std::min(WORLD_W - 1, (qx1 + 1) * TILE + FEATHER), py1 = std::min(WORLD_H - 1, (qy1 + 1) * TILE + FEATHER);
        grow(px0, py0, px1 + 1, py1 + 1);
        for (int y = py0; y <= py1; y++) for (int x = px0; x <= px1; x++) {
            float best = 1e9f;   // distance to the nearest quarried tile (px)
            for (int ty = std::max(0, (y - FEATHER) / TILE); ty <= std::min(MAP_H - 1, (y + FEATHER) / TILE); ty++)
                for (int tx = std::max(0, (x - FEATHER) / TILE); tx <= std::min(MAP_W - 1, (x + FEATHER) / TILE); tx++) {
                    if (!std::binary_search(quarried.begin(), quarried.end(), ty * MAP_W + tx)) continue;
                    float dx = std::max(0.0f, std::max(tx * TILE - (float)x, x - (tx + 1.0f) * TILE)), dy = std::max(0.0f, std::max(ty * TILE - (float)y, y - (ty + 1.0f) * TILE));
                    best = std::min(best, std::sqrt(dx * dx + dy * dy));
                }
            if (best > FEATHER) continue;
            u16* dp = K.pre(x, y);
            if (!dp) continue;   // (nothing kept there: no rock to quarry within reach)
            float w = best <= 2.0f ? 1.0f : sstep((float)FEATHER, 2.0f, best);
            float macro = nz.fbm(x * 0.012f, y * 0.012f);
            F3 dirt = groundColor(K_DIRT, (float)x, (float)y, nz, macro, 0);
            dirt = lerp3(dirt, SCREE, 0.25f * nz.v(x * 0.2f + 4, y * 0.2f + 9));   // rubble left by the quarrying
            dirt = gradeGround(dirt, (float)x, (float)y, nz);
            u16& d = *dp;
            Color o = from565(d);
            d = to565(Color{ (u8)clampf(o.r + (dirt.r - o.r) * w, 0, 255), (u8)clampf(o.g + (dirt.g - o.g) * w, 0, 255), (u8)clampf(o.b + (dirt.b - o.b) * w, 0, 255), 255 }, x, y);
        }
    }
    if (!felled && !rock) return false;
    x0 = std::max(0, x0); y0 = std::max(0, y0); x1 = std::min(WORLD_W, x1); y1 = std::min(WORLD_H, y1);
    if (x1 <= x0 || y1 <= y0) return false;
    rx = x0; ry = y0; rw = x1 - x0; rh = y1 - y0;
    // recompose: the ground before the trees where anything was kept (clear elsewhere: those pixels do not change), then every standing tree that
    // reaches into the rectangle, back to front
    Canvas patch(rw, rh);
    for (int y = 0; y < rh; y++) for (int x = 0; x < rw; x++) { const u16* q = K.pre(rx + x, ry + y); patch.px[(size_t)y * rw + x] = q ? Canvas::pack(from565(*q)) : 0; }
    static float maxHalf[T_KINDS] = {};
    if (maxHalf[0] <= 0) for (int k = 0; k < T_KINDS; k++) for (auto& sp : K.sprites[k]) maxHalf[k] = std::max(maxHalf[k], std::max(sp.w, sp.h) * 0.5f);
    float maxAll = 0; for (int k = 0; k < T_KINDS; k++) maxAll = std::max(maxAll, maxHalf[k] * 1.4f + 8.0f);
    auto lo = std::lower_bound(K.trees.begin(), K.trees.end(), ry - maxAll, [](const Tree& t, float v) { return t.y < v; });
    for (auto it = lo; it != K.trees.end() && it->y - maxAll < ry + rh; ++it) {
        const Tree& t = *it;
        if (t.gone) continue;
        float m = maxHalf[t.kind] * std::max(1.0f, t.r / spriteBase(t.kind)) + 6.0f;
        if (t.x + m < rx || t.x - m > rx + rw || t.y + m < ry || t.y - m > ry + rh) continue;
        int a, b, c, d; treeRect(t, a, b, c, d);
        if (c < rx || a > rx + rw || d < ry || b > ry + rh) continue;
        drawTree(patch, t, rx, ry);
    }
    out.swap(patch.px);
    return true;
}

// The overview is gathered band by band: 3x3 samples inside each output pixel's exact footprint, summed in the same order as a single pass over the
// whole world would (rows top to bottom), so the bands give exactly the old picture
void terrainOverviewBegin(TerrainOverview& o, int w, int h) { o.w = w; o.h = h; o.sum.assign((size_t)w * h * 3, 0.0f); }
void terrainOverviewAdd(TerrainOverview& o, const Canvas& band, int oy) {
    if (o.w <= 0) return;
    for (int y = 0; y < o.h; y++) for (int j = 0; j < 3; j++) {
        int sy = std::min(WORLD_H - 1, (int)((y + (j + 0.5f) / 3.0f) * WORLD_H / o.h));
        if (sy < oy || sy >= oy + band.h) continue;
        const u32* row = &band.px[(size_t)(sy - oy) * band.w];
        float* acc = &o.sum[(size_t)y * o.w * 3];
        for (int x = 0; x < o.w; x++) for (int i = 0; i < 3; i++) {
            int sx = std::min(WORLD_W - 1, (int)((x + (i + 0.5f) / 3.0f) * WORLD_W / o.w));
            Color c = Canvas::unpack(row[sx]); acc[x * 3] += c.r; acc[x * 3 + 1] += c.g; acc[x * 3 + 2] += c.b;
        }
    }
}
void terrainOverviewEnd(const TerrainOverview& o, Canvas& out) {
    for (int y = 0; y < out.h && y < o.h; y++) for (int x = 0; x < out.w && x < o.w; x++) {
        const float* a = &o.sum[((size_t)y * o.w + x) * 3];
        out.px[y * out.w + x] = Canvas::pack(Color{(u8)clampf(a[0] / 9.0f, 0, 255), (u8)clampf(a[1] / 9.0f, 0, 255), (u8)clampf(a[2] / 9.0f, 0, 255), 255});
    }
}

// (the bake's row splitter for the rest of the renderer: see terrain.h)
void bakeRows(int rows, const std::function<void(int, int)>& f) { parallelRows(rows, f); }
