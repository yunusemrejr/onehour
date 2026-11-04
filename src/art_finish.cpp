// One Hour - the finishing pass for structure sprites (declared in artb.h). It runs once on each painted structure canvas, after the painter
// and before the realism grade, and adds the depth, light and wear the flat painters leave out: cavity shading, occlusion under taller
// parts, a gentle sky light, rim light on the outer edges, weathering (rain drips, dirt at the base, rust and soot on Clanker, a cool sheen
// on Cyber), emitter bloom and a tight contact shadow. It only changes colour inside the painted shape and only adds alpha outside it, so the
// edges, footprints and the team mask (painted separately) stay where they were. Everything is seeded: a rebuild gives the same pixels.
//
// The analysis (cavity, occlusion, drips, soot, bloom) runs on a half-resolution grid, one cell per design pixel (2x2 canvas pixels), padded
// by two empty cells so the neighbour reads need no bounds checks; the last step applies the cell values back to every canvas pixel.
#include "artb.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {

typedef std::vector<float> Plane;

// deterministic value in [0,1) for a cell and a seed: no state, so the same cell always gets the same speckle
inline float hash01(int x, int y, u32 seed) {
    u32 h = ((u32)x * 0x27d4eb2du) + ((u32)y * 0x165667b1u) + (seed * 0x9e3779b9u);
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return (h & 0xffffff) / 16777216.0f;
}
inline float clamp01(float v) { return v < 0 ? 0 : (v > 1 ? 1 : v); }
inline u32 toByte(float v) { return v <= 0 ? 0u : (v >= 255 ? 255u : (u32)(v + 0.5f)); }
// saturation of a colour: (max - min) / max, 0 for black
inline float satOf(float r, float g, float b) {
    float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b));
    return mx > 0 ? (mx - mn) / mx : 0.0f;
}

// running-sum box blur of one line of n samples spaced `step` apart, zero beyond both ends (out must not alias in)
void boxLine(const float* in, float* out, int n, int step, int r) {
    float sum = 0, inv = 1.0f / (2 * r + 1);
    for (int i = 0; i <= std::min(n - 1, r); i++) sum += in[i * step];
    for (int i = 0; i < n; i++) {
        out[i * step] = sum * inv;
        if (i + 1 + r < n) sum += in[(i + 1 + r) * step];
        if (i - r >= 0) sum -= in[(i - r) * step];
    }
}
// separable box blur of a w x h plane; each pass is a horizontal then a vertical box, so two passes approximate a gaussian
void blur(Plane& p, int w, int h, int r, int passes, Plane& tmp) {
    tmp.resize(p.size());
    for (int k = 0; k < passes; k++) {
        for (int y = 0; y < h; y++) boxLine(&p[(size_t)y * w], &tmp[(size_t)y * w], w, 1, r);
        for (int x = 0; x < w; x++) boxLine(&tmp[x], &p[x], h, w, r);
    }
}

// a colour wash (dust, rust, soot, oil) pulls the painted colour toward t by a. Every wash is affine, so a cell's whole stack of washes
// folds into one multiply and one offset: the painted colour c0 becomes c0 * P + K, which the pixel loop applies with no per-wash work.
inline void wash(float& P, float& Kr, float& Kg, float& Kb, Color t, float a) {
    a = clamp01(a);
    P *= 1 - a; Kr = Kr * (1 - a) + t.r * a; Kg = Kg * (1 - a) + t.g * a; Kb = Kb * (1 - a) + t.b * a;
}

}  // namespace

void finishBuilding(Canvas& c, int type, u32 seed, bool overlay) {
    // 1. the painted bounds, widened by a margin for the shadow and bloom that spill past them, on an even grid so the half-res cells line up
    int bx0 = c.w, by0 = c.h, bx1 = -1, by1 = -1;
    for (int y = 0; y < c.h; y++) {
        const u32* row = &c.px[(size_t)y * c.w];
        for (int x = 0; x < c.w; x++) {
            if (row[x] >> 24 == 0) continue;
            bx0 = std::min(bx0, x); by0 = std::min(by0, y); bx1 = std::max(bx1, x); by1 = std::max(by1, y);
        }
    }
    if (bx1 < 0) return;
    const int M = 6;   // (three cells: the contact shadow and the bloom reach two)
    const int x0 = std::max(0, bx0 - M) & ~1, y0 = std::max(0, by0 - M) & ~1;
    const int x1 = std::min(c.w - 1, bx1 + M) | 1, y1 = std::min(c.h - 1, by1 + M) | 1;
    const int W = x1 - x0 + 1, H = y1 - y0 + 1, hw = W / 2, hh = H / 2;   // the interior: hw x hh cells
    const int PAD = 2, pw = hw + 2 * PAD, ph = hh + 2 * PAD, m = pw * ph;  // planes carry PAD empty cells on every side
    const bool cy = BUILDS[type].faction == F_CYBER;
    auto hk = [pw, PAD](int x, int y) { return (size_t)(y + PAD) * pw + (x + PAD); };
    auto inside = [hw, hh](int x, int y) { return x >= 0 && y >= 0 && x < hw && y < hh; };
    const int rx = x0 >> 1, ry = y0 >> 1;   // global design-cell origin: the seeded speckle is the same wherever the region sits
    // sky light: north-west brighter, south-east darker (+8.5% at the canvas's north-west corner, -8.5% at its south-east corner). The canvas
    // is the reference, not the painted bounds, so an upgrade overlay is lit exactly like the bunker it sits on.
    std::vector<float> skyX(hw), skyY(hh);
    for (int x = 0; x < hw; x++) skyX[x] = (x0 + 2 * x) / (float)c.w;
    for (int y = 0; y < hh; y++) skyY[y] = (y0 + 2 * y) / (float)c.h;

    // 2. the cells: alpha, the colour of the painted part (the four canvas pixels, weighted by their alpha), and luminance
    Plane A(m, 0.0f), L(m, 0.0f), Cr(m, 0.0f), Cg(m, 0.0f), Cb(m, 0.0f);
    for (int y = 0; y < hh; y++) {
        const u32* top = &c.px[(size_t)(y0 + 2 * y) * c.w + x0];
        for (int x = 0; x < hw; x++) {
            const u32* p = top + 2 * x;
            const u32 v[4] = { p[0], p[1], p[c.w], p[c.w + 1] };
            float sa = 0, sr = 0, sg = 0, sb = 0;
            for (int i = 0; i < 4; i++) {
                const float a8 = (float)(v[i] >> 24);
                sa += a8; sr += (v[i] & 255) * a8; sg += ((v[i] >> 8) & 255) * a8; sb += ((v[i] >> 16) & 255) * a8;
            }
            const size_t k = hk(x, y);
            A[k] = sa / (4 * 255.0f);
            if (sa > 0) { Cr[k] = sr / sa; Cg[k] = sg / sa; Cb[k] = sb / sa; }
            L[k] = 0.299f * Cr[k] + 0.587f * Cg[k] + 0.114f * Cb[k];
        }
    }

    // 3. cavity: how far each painted cell sits from its own neighbourhood (alpha-weighted, so the sprite edge is not dragged dark)
    Plane num(m), den(m), cav(m, 0.0f), tmp;
    for (size_t k = 0; k < (size_t)m; k++) { num[k] = L[k] * A[k]; den[k] = A[k]; }
    blur(num, pw, ph, 2, 1, tmp); blur(den, pw, ph, 2, 1, tmp);
    for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
        size_t k = hk(x, y);
        if (A[k] > 0.5f && den[k] > 0.1f) cav[k] = L[k] - num[k] / den[k];
    }

    // 4. the lighting factor: cavity, occlusion from taller, brighter parts to the north-west (their shade falls south-east), and the rim:
    // the outer edge facing north-west catches the sky and the edge facing south-east falls into shade (the same light as paint.h's bevel)
    Plane fac(m, 1.0f);
    static const int D[3][2] = { { -1, -1 }, { 0, -1 }, { -1, 0 } };   // occluders to the NW, N and W
    for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
        const size_t k = hk(x, y);
        fac[k] = 1.0f + 0.085f * (1.0f - (skyX[x] + skyY[y]));
        if (A[k] < 0.5f) continue;
        float ao = 0;
        const size_t o1[3] = { hk(x - 1, y - 1), hk(x, y - 1), hk(x - 1, y) };   // the occluders one cell away, NW / N / W
        bool nwOut = false;
        for (int d = 0; d < 3; d++) {
            if (A[o1[d]] < 0.5f) { nwOut = true; continue; }
            float step = L[o1[d]] - L[k];
            if (step > 0) ao = std::max(ao, clamp01(step / 90.0f));
        }
        for (int d = 0; d < 3; d++) {   // two cells away (weaker)
            const size_t o2 = hk(x + 2 * D[d][0], y + 2 * D[d][1]);
            if (A[o2] < 0.5f) continue;
            float step = L[o2] - L[k];
            if (step > 0) ao = std::max(ao, clamp01(step / 90.0f) * 0.85f);
        }
        const bool seOut = A[hk(x + 1, y + 1)] < 0.5f || A[hk(x + 1, y)] < 0.5f || A[hk(x, y + 1)] < 0.5f;
        float fc = 1.0f + clamp01(cav[k] / 140.0f) * (cav[k] > 0 ? 0.12f : 0.28f) * (cy ? 1.15f : 1.0f);
        fac[k] = fc * (1.0f - (cy ? 0.34f : 0.30f) * ao) * fac[k] * (nwOut ? 1.15f : 1.0f) * (seOut ? 0.82f : 1.0f);
    }

    // 5. emitters: bright, saturated cells (lights, screens, LEDs). They keep their colour, are never shaded, and feed the bloom
    Plane em(m, 0.0f), bR(m, 0.0f), bG(m, 0.0f), bB(m, 0.0f);
    bool anyEmitter = false;
    for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
        const size_t k = hk(x, y);
        if (A[k] < 0.3f || L[k] <= 185.0f) continue;   // (most cells are far darker than an emitter: the saturation test is for the few that pass)
        float e = clamp01((L[k] - 185.0f) / 55.0f) * clamp01((satOf(Cr[k], Cg[k], Cb[k]) - 0.3f) / 0.25f);
        if (e <= 0) continue;
        anyEmitter = true;
        em[k] = e;
        bR[k] = Cr[k] * e; bG[k] = Cg[k] * e; bB[k] = Cb[k] * e;
        if (e > 0.5f) fac[k] = 1.0f;
    }

    // 6. weathering: rain runs down from horizontal edges (a top edge, or a ledge a little above a drop in brightness); each streak fades
    // over 6-19 cells and darkens the paint (Cyber) or carries orange-brown run-off (Clanker). Dirt gathers toward each column's foot.
    Plane drip(m, 0.0f), dirt(m, 0.0f), soot(m, 0.0f), speck(m, 0.0f), oil(m, 0.0f), shadow(m, 0.0f);
    if (!overlay) {
        for (int x = 0; x < hw; x++) {
            float d = 0, decay = 1;
            for (int y = 0; y < hh; y++) {
                const size_t k = hk(x, y);
                if (A[k] < 0.5f) { d = 0; continue; }
                const bool top = A[hk(x, y - 1)] < 0.5f;
                const bool ledge = A[hk(x, y - 2)] > 0.8f && L[hk(x, y - 2)] - L[k] > 40.0f;
                if (d < 0.02f && (top || ledge) && hash01(rx + x, ry + y, seed + 7) < (cy ? 0.12f : 0.16f)) {
                    d = 1.0f; decay = 1.0f - 1.0f / (6.0f + 13.0f * hash01(rx + x, ry + y, seed + 8));
                }
                drip[k] = d;
                d *= decay;
            }
        }
        for (int x = 0; x < hw; x++) {
            int yb = -1;
            for (int y = hh - 1; y >= 0; y--) if (A[hk(x, y)] >= 0.5f) { yb = y; break; }
            for (int y = 0; y <= yb; y++) { float t = 1.0f - clamp01((yb - y) / 22.0f); dirt[hk(x, y)] = t * t; }
        }
        if (!cy) {
            // soot: a dark, vent-like cavity puts a dirty halo on the metal round it
            for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
                const size_t k = hk(x, y);
                if (A[k] > 0.5f) soot[k] = clamp01((-cav[k] - 22.0f) / 40.0f);
            }
            blur(soot, pw, ph, 2, 1, tmp);
            // rust specks across the metal, and a few dark oil pools on the pads near the base
            for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
                if (A[hk(x, y)] < 0.5f || hash01(rx + x, ry + y, seed + 21) > 0.06f) continue;
                speck[hk(x, y)] = 0.7f + 0.3f * hash01(rx + x, ry + y, seed + 22);
            }
            for (int s = 0; s < 2; s++) {
                float cx0 = hw * (0.2f + 0.6f * hash01(0, 0, seed + 31 + s)), cy0 = hh * (0.62f + 0.3f * hash01(0, 0, seed + 33 + s));
                float rr = 2.5f + 2.0f * hash01(0, 0, seed + 35 + s);
                for (int y = (int)(cy0 - rr * 1.2f); y <= (int)(cy0 + rr * 1.2f); y++) for (int x = (int)(cx0 - rr * 1.6f); x <= (int)(cx0 + rr * 1.6f); x++) {
                    if (!inside(x, y) || A[hk(x, y)] < 0.5f) continue;
                    float dd = std::hypot((x + 0.5f - cx0) / 1.6f, (y + 0.5f - cy0) / 1.2f) / rr;
                    if (dd < 1) oil[hk(x, y)] = std::max(oil[hk(x, y)], 0.45f * (1.0f - dd * dd));
                }
            }
        }
    }

    // 7. contact shadow: a tight, feathered dark band under the south-east edge (the wider pad shadow is already in the painter), composited
    // beneath the painted pixels. Only the padding gains alpha, and the shape's own colour is unchanged.
    if (!overlay) {
        for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) shadow[hk(x, y)] = A[hk(x - 1, y - 1)];   // the NW edge casts it south-east
        blur(shadow, pw, ph, 1, 1, tmp);
        for (int y = 0; y < hh; y++) for (int x = 0; x < hw; x++) {
            const size_t k = hk(x, y);
            shadow[k] = A[k] > 0.999f ? 0.0f : std::min(1.0f, shadow[k] * 1.6f) * 0.36f;   // (solid cells are never under the shadow)
        }
    }
    // 8. bloom: the emitters' energy spread over ~2-3 px, for every structure including the bunker overlays
    if (anyEmitter) { blur(bR, pw, ph, 1, 1, tmp); blur(bG, pw, ph, 1, 1, tmp); blur(bB, pw, ph, 1, 1, tmp); }

    // 9. the sheen across Cyber's bright panels: about one and a half soft diagonal bands across the sprite, from the NW corner
    const float sheenU = 0.30f + 0.35f * hash01(0, 0, seed + 3);
    std::vector<float> sheen(cy ? W + H : 0, 0.0f);
    for (int d = 0; d < (int)sheen.size(); d++) sheen[d] = std::pow(0.5f + 0.5f * std::cos(6.2831853f * (1.5f * d / (float)(W + H) - sheenU)), 6.0f);

    // 10. apply to every canvas pixel, one cell (2x2 pixels) at a time: the cell's washes fold into c0 * P + K, then the lighting factor and
    // the Cyber sheen, then the contact shadow and the bloom. Emitters and the padding keep their own colour.
    const Color rustC = rgb(150, 84, 44), sootC = rgb(44, 40, 36), dustC = rgb(88, 78, 60), dustCy = rgb(74, 80, 90), oilC = rgb(24, 22, 30);
    for (int cyI = 0; cyI < hh; cyI++) for (int cxI = 0; cxI < hw; cxI++) {
        const size_t k = hk(cxI, cyI);
        const bool emit = em[k] > 0.5f;
        float P = 1, Kr = 0, Kg = 0, Kb = 0, fk = emit ? 1.0f : fac[k], sheenW = 0;
        if (!emit && !overlay) {
            fk *= 1.0f - (cy ? 0.05f : 0.09f) * dirt[k];   // dirt toward the base
            fk *= 1.0f - (cy ? 0.14f : 0.08f) * drip[k];   // rain streaks run darker
            if (!cy) {
                const float lit = clamp01((L[k] - 35.0f) / 50.0f);   // rust shows on lit metal, not in the black voids of the blast holes
                wash(P, Kr, Kg, Kb, dustC, 0.14f * dirt[k]);
                wash(P, Kr, Kg, Kb, rustC, 0.38f * drip[k] * lit);   // orange-brown run-off
                wash(P, Kr, Kg, Kb, sootC, std::min(0.55f, soot[k] * 1.4f));
                wash(P, Kr, Kg, Kb, rustC, 0.42f * speck[k] * lit);
                wash(P, Kr, Kg, Kb, oilC, oil[k]);
            } else {
                wash(P, Kr, Kg, Kb, dustCy, 0.06f * dirt[k]);
                sheenW = clamp01((L[k] - 130.0f) / 70.0f) * (1.0f - clamp01(satOf(Cr[k], Cg[k], Cb[k]) * 2.0f));   // unsaturated, bright panels only
            }
        }
        if (!emit) fk = clampf(fk, 0.62f, 1.3f);
        float br = bR[k], bg = bG[k], bb = bB[k];
        if (cy) { br *= 0.55f; bg *= 0.85f; bb *= 1.15f; }   // Cyber's light bleeds blue
        const float bmax = std::max(br, std::max(bg, bb)), sh = shadow[k];
        // the painted colour becomes c0 * mul + off (+ the sheen, which is per pixel): the cell's washes and lighting in a few numbers
        const float mul = P * fk, offR = Kr * fk, offG = Kg * fk, offB = Kb * fk;
        const float shR = 38.0f * sheenW * fk, shG = 44.0f * sheenW * fk, shB = 54.0f * sheenW * fk;
        for (int dy = 0; dy < 2; dy++) for (int dx = 0; dx < 2; dx++) {
            const int px = 2 * cxI + dx, py = 2 * cyI + dy;
            const size_t idx = (size_t)(y0 + py) * c.w + x0 + px;
            const u32 v = c.px[idx];
            const u32 a8 = v >> 24;
            if (a8 < 3 && sh <= 0 && bmax < 1.0f) { c.px[idx] = 0; continue; }   // (below 1%: the padding, which stays empty)
            const float a = a8 / 255.0f;
            float r = v & 255, g = (v >> 8) & 255, b = (v >> 16) & 255, al = a;
            if (a8 >= 3) {
                const float sv = sheenW > 0 ? sheen[px + py] : 0.0f;
                r = clampf(r * mul + offR + sv * shR, 0, 255);
                g = clampf(g * mul + offG + sv * shG, 0, 255);
                b = clampf(b * mul + offB + sv * shB, 0, 255);
            }
            if (sh <= 0 && bmax < 1.0f) {   // (the common case: nothing to composite, the alpha stays)
                c.px[idx] = (a8 << 24) | (toByte(b) << 16) | (toByte(g) << 8) | toByte(r);
                continue;
            }
            // contact shadow beneath the painted pixel (under-composite: the colour is unchanged where the pixel is solid)
            if (sh > 0 && al < 1.0f) {
                float oa = al + sh * (1.0f - al);
                r *= al / oa; g *= al / oa; b *= al / oa;
                al = oa;
            }
            // bloom: added onto painted pixels, over-composited onto the padding
            if (bmax >= 1.0f) {
                if (al >= 0.5f) {
                    const float kb = cy ? 0.55f : 0.7f;
                    r = clampf(r + br * kb, 0, 255); g = clampf(g + bg * kb, 0, 255); b = clampf(b + bb * kb, 0, 255);
                } else {
                    float ab = std::min(0.45f, clamp01(bmax / 255.0f * 0.9f) * 0.5f), sc = 255.0f / bmax;
                    float na = ab + al * (1.0f - ab);
                    if (na > 0) {
                        r = (br * sc * ab + r * al * (1.0f - ab)) / na; g = (bg * sc * ab + g * al * (1.0f - ab)) / na; b = (bb * sc * ab + b * al * (1.0f - ab)) / na;
                        al = na;
                    }
                }
            }
            const u32 ai = toByte(al * 255.0f);
            c.px[idx] = ai == 0 ? 0 : (ai << 24) | (toByte(b) << 16) | (toByte(g) << 8) | toByte(r);
        }
    }
}
