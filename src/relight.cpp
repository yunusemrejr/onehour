// One Hour - the studio pipeline's lighting pass (relight.h). A painted sprite comes in as albedo plus a height field (paint::Relief) and
// goes out lit the way an offline renderer lights a model seen from above:
//   normals from the height field's slopes, a warm key light (Lambert) with a cast self-shadow traced over the height field (a turret
//   shades the hull on its far side, a mast lays a thin shadow across the deck), a cool sky fill that dims on faces turned away from the
//   sky, ambient occlusion from how far a pixel sits below its surroundings (creases, the foot of every raised part), a Blinn-Phong sheen
//   that bare metal takes fully and paint only a little, worn lighter paint along the sharp convex edges and grime in the concave ones.
//   Emitters (glows, lit glass, lamps) keep their own colour.
// All of it runs once per sprite at startup on canvases of at most a few hundred pixels a side.
#include "relight.h"
#include <cmath>
#include <vector>

using paint::Relief;

LightRig rigUnit() { LightRig L; L.lx = -0.34f; L.ly = -0.44f; L.lz = 0.83f; L.reach = 11; L.shadow = 0.74f; L.amb = 0.36f; L.key = 0.64f; L.spec = 0.50f; L.ao = 1.25f; return L; }
LightRig rigBuilding() { LightRig L; L.lx = -0.54f; L.ly = -0.60f; L.lz = 0.59f; L.reach = 26; L.shadow = 0.80f; L.amb = 0.37f; L.key = 0.63f; L.spec = 0.48f; L.ao = 1.3f; return L; }

namespace {

// running-sum box blur of n samples spaced `step` apart, edge samples repeated
void boxLine(const float* in, float* out, int n, int step, int r) {
    if (n <= 0) return;
    float sum = 0, inv = 1.0f / (2 * r + 1);
    for (int i = -r; i <= r; i++) sum += in[clampi(i, 0, n - 1) * step];
    for (int i = 0; i < n; i++) {
        out[i * step] = sum * inv;
        sum += in[clampi(i + r + 1, 0, n - 1) * step] - in[clampi(i - r, 0, n - 1) * step];
    }
}
void boxBlur(std::vector<float>& p, int w, int h, int r, int passes) {
    std::vector<float> t(p.size());
    for (int k = 0; k < passes; k++) {
        for (int y = 0; y < h; y++) boxLine(&p[(size_t)y * w], &t[(size_t)y * w], w, 1, r);
        for (int x = 0; x < w; x++) boxLine(&t[x], &p[x], h, w, r);
    }
}
inline float zAt(const std::vector<float>& z, int w, int h, float x, float y) {   // bilinear, zero outside
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    auto g = [&](int a, int b) { return a < 0 || b < 0 || a >= w || b >= h ? 0.0f : z[(size_t)b * w + a]; };
    return (g(ix, iy) * (1 - fx) + g(ix + 1, iy) * fx) * (1 - fy) + (g(ix, iy + 1) * (1 - fx) + g(ix + 1, iy + 1) * fx) * fy;
}

}  // namespace

void relight(Canvas& c, const Relief& r, const LightRig& L0, Canvas* mask) {
    const int W = c.w, H = c.h;
    const size_t N = (size_t)W * H;
    if (r.w != W || r.h != H) return;
    LightRig L = L0;
    { float n = std::sqrt(L.lx * L.lx + L.ly * L.ly + L.lz * L.lz); L.lx /= n; L.ly /= n; L.lz /= n; }
    std::vector<float> z(N), b2, b6;
    bool any = false;
    for (size_t i = 0; i < N; i++) { z[i] = (c.px[i] >> 24) ? r.z[i] * L.zScale : 0.0f; any = any || z[i] != 0; }
    {   // a light 1-2-1 smoothing: the profile steps between stacked shapes become slopes a normal can read
        std::vector<float> t = z;
        for (int y = 0; y < H; y++) for (int x = 1; x < W - 1; x++) t[(size_t)y * W + x] = (z[(size_t)y * W + x - 1] + 2 * z[(size_t)y * W + x] + z[(size_t)y * W + x + 1]) * 0.25f;
        for (int y = 1; y < H - 1; y++) for (int x = 0; x < W; x++) z[(size_t)y * W + x] = (t[(size_t)(y - 1) * W + x] + 2 * t[(size_t)y * W + x] + t[(size_t)(y + 1) * W + x]) * 0.25f;
        for (size_t i = 0; i < N; i++) if (!(c.px[i] >> 24)) z[i] = 0;
    }
    float zMax = 0;
    for (size_t i = 0; i < N; i++) zMax = std::max(zMax, z[i]);
    if (any) { b2 = z; boxBlur(b2, W, H, 2, 2); b6 = z; boxBlur(b6, W, H, 6, 2); }
    const float hl = std::sqrt(L.lx * L.lx + L.ly * L.ly);
    const float dx = hl > 1e-4f ? L.lx / hl : 0, dy = hl > 1e-4f ? L.ly / hl : 0, slope = hl > 1e-4f ? L.lz / hl : 1e6f;
    float hx = L.lx, hy = L.ly, hz = L.lz + 1.0f;   // the half vector toward a viewer straight above
    { float n = std::sqrt(hx * hx + hy * hy + hz * hz); hx /= n; hy /= n; hz /= n; }
    const float AR = 0.74f, AG = 0.90f, AB = 1.20f;   // sky fill: the blue of open sky, so every shaded face turns to cool steel
    const float KR = 1.15f, KG = 1.05f, KB = 0.84f;   // key: a low golden sun
    std::vector<u32> out = c.px;
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        const size_t i = (size_t)y * W + x;
        Color p = Canvas::unpack(c.px[i]);
        if (p.a == 0) continue;
        float nx = 0, ny = 0, nz = 1, ao = 1, sh = 0, lap = 0;
        if (any) {
            auto Z = [&](int a, int b) { a = clampi(a, 0, W - 1); b = clampi(b, 0, H - 1); return z[(size_t)b * W + a]; };
            float z0 = z[i];
            float gx = (Z(x + 1, y) - Z(x - 1, y)) * 0.5f, gy = (Z(x, y + 1) - Z(x, y - 1)) * 0.5f;
            float nl = std::sqrt(gx * gx + gy * gy + 1.0f);
            nx = -gx / nl; ny = -gy / nl; nz = 1.0f / nl;
            float occ = std::max(0.0f, b2[i] - z0) * 0.30f + std::max(0.0f, b6[i] - z0) * 0.16f;
            ao = 1.0f - clampf(occ * L.ao, 0.0f, 0.62f);
            const float tEnd = std::min(L.reach, (zMax - z0) / slope);   // (past this nothing on the sprite stands tall enough to shade the pixel)
            for (float t = 1.0f; t <= tEnd; t += 1.0f) {   // the key light's cast shadow, traced toward the light over the height field
                float zs = zAt(z, W, H, x + 0.5f + dx * t - 0.5f, y + 0.5f + dy * t - 0.5f);
                float hgt = zs - z0 - t * slope;
                if (hgt > 0) sh = std::max(sh, clampf(hgt * 0.7f, 0, 1) * (1.0f - 0.45f * t / L.reach));
            }
            lap = Z(x - 1, y) + Z(x + 1, y) + Z(x, y - 1) + Z(x, y + 1) - 4 * z0;
        }
        float R = p.r, G = p.g, B = p.b;
        float lum = 0.299f * R + 0.587f * G + 0.114f * B, mx = std::max(R, std::max(G, B)), mn = std::min(R, std::min(G, B));
        float sat = mx > 0 ? (mx - mn) / mx : 0;
        float emit = r.emit[i] / 255.0f;
        if (lum > 205 && sat > 0.42f) emit = std::max(emit, 0.8f);   // lit glass, lamps, emitter strips painted as plain bright colour
        float metal = clampf((0.24f - sat) / 0.14f, 0, 1) * clampf((lum - 45) / 70.0f, 0, 1);
        {   // bare greys become materials: gunmetal in the darks, blue naval steel through the mids, and no slab left paper-white
            float neutral = clampf((0.20f - sat) / 0.14f, 0, 1) * (1.0f - emit);
            float l = lum / 255.0f;
            float dim = 1.0f - 0.30f * clampf((l - 0.42f) / 0.4f, 0, 1);          // the palest panels come down a fifth: lamps and markings stand out
            float cool = 1.0f - 0.5f * clampf((l - 0.55f) / 0.4f, 0, 1);          // (the darks take the most blue)
            R += (R * dim * (1.0f - 0.13f * cool) - R) * neutral;
            G += (G * dim * (1.0f - 0.03f * cool) - G) * neutral;
            B += (B * dim * (1.0f + 0.12f * cool) - B) * neutral;
        }
        float ndl = std::max(0.0f, nx * L.lx + ny * L.ly + nz * L.lz);
        float lit = 1.0f - sh * L.shadow;
        float key = L.key * ndl / L.lz * lit;   // (a flat face: L.key)
        float sky = L.amb * (0.55f + 0.45f * nz) / 1.0f * ao;
        float ndh = std::max(0.0f, nx * hx + ny * hy + nz * hz);
        float sp = std::pow(ndh, L.shine) * L.spec * (0.12f + 0.88f * metal) * lit;
        sp += std::pow(ndh, L.shine * 0.25f) * 0.05f * metal * lit;   // (a broad soft lobe under the tight one: brushed metal)
        float wear = 1.0f;
        if (lap < -0.25f) wear += L.wear * 0.16f * clampf(-lap * 0.45f, 0, 1);   // the sharp edge of a plate catches the light and loses its paint
        else if (lap > 0.25f) wear -= L.wear * 0.14f * clampf(lap * 0.45f, 0, 1);   // dirt settles in the crease
        float fr = R * (AR * sky + KR * key) * wear + sp * 255 * KR * 0.95f;
        float fg = G * (AG * sky + KG * key) * wear + sp * 255 * KG * 0.95f;
        float fb = B * (AB * sky + KB * key) * wear + sp * 255 * KB * 0.95f;
        {   // the finishing grade: a contrast curve and fuller colour, so paint reads as paint and steel as steel (pale washed-out greys are gone)
            auto fin = [](float v) { v = clampf(v / 255.0f, 0.0f, 1.0f); return (v * 0.56f + v * v * (3.0f - 2.0f * v) * 0.44f) * 255.0f; };
            fr = fin(fr * 0.96f); fg = fin(fg * 0.96f); fb = fin(fb * 0.96f);
            float l2 = 0.299f * fr + 0.587f * fg + 0.114f * fb;
            const float S = 1.22f;
            fr = l2 + (fr - l2) * S; fg = l2 + (fg - l2) * S; fb = l2 + (fb - l2) * S;
        }
        fr += (R - fr) * emit; fg += (G - fg) * emit; fb += (B - fb) * emit;
        out[i] = Canvas::pack(Color{ (u8)clampf(fr, 0, 255), (u8)clampf(fg, 0, 255), (u8)clampf(fb, 0, 255), p.a });
        if (mask) {
            Color m = Canvas::unpack(mask->px[i]);
            if (m.a) {
                float f = clampf((sky + key) * wear * (1 - emit) + emit + sp * 0.5f, 0, 1.25f);
                mask->px[i] = Canvas::pack(Color{ (u8)clampf(m.r * f, 0, 255), (u8)clampf(m.g * f, 0, 255), (u8)clampf(m.b * f, 0, 255), m.a });
            }
        }
    }
    c.px.swap(out);
}
