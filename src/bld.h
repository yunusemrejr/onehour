// One Hour - the structure painter shared by the building art files (artb.cpp: the base structures; artb_naval.cpp: the naval yards and anti-nuke defences).
// Structures are drawn in a raised 3/4 view: each volume has a ground rectangle, a roof lifted by its height and a south wall in
// between, with soft shadows falling south-east (light from the north-west). Cyber is ceramic and cobalt with emitter light; Clanker is
// olive steel, corrugated roofs, sandbags, rust and hazard paint.
#pragma once
#include "artb.h"
#include "paint.h"

using namespace paint;

namespace {


// ---------------------------------------------------------------- palettes
const Color C_PAD = rgb(64, 74, 92), C_PAD2 = rgb(52, 60, 78), C_SEAM = rgb(28, 34, 46);
const Color C_HULL = rgb(58, 84, 136), C_HULL_D = rgb(36, 54, 94), C_ROOF = rgb(150, 170, 198), C_ROOF2 = rgb(106, 130, 170), C_STEEL = rgb(36, 42, 56);
const Color C_GLASS = rgb(48, 132, 164), C_CYAN = rgb(110, 232, 255), C_WHITE = rgb(228, 243, 252), C_AMBER = rgb(255, 190, 92), C_GOLD = rgb(255, 212, 96), C_RED = rgb(255, 92, 80);
const Color K_PAD = rgb(112, 106, 92), K_PAD2 = rgb(94, 90, 78), K_SEAM = rgb(52, 48, 40);
const Color K_OLIVE = rgb(98, 108, 68), K_OLIVE_D = rgb(64, 72, 44), K_KHAKI = rgb(166, 152, 108), K_RUST = rgb(160, 94, 54), K_RUST_D = rgb(108, 64, 38);
const Color K_STEEL = rgb(130, 132, 130), K_DARK = rgb(38, 38, 32), K_YEL = rgb(232, 188, 62), K_ROOF = rgb(138, 130, 106), K_TARP = rgb(88, 104, 70);
const Color K_SAND = rgb(194, 176, 128), K_BRICK = rgb(144, 88, 64), K_RED = rgb(216, 62, 50);
const Color BLACK = rgb(0, 0, 0), WHITE = rgb(255, 255, 255);

struct Roof { float x, y, w, h; };   // the top face of a block, in design coordinates (already lifted by the block's height)

struct Bld {
    Art a, t;           // colour painter and team-mask painter (same canvas geometry)
    int W, H; bool cy; Rng rng;
    Bld(Canvas& c, Canvas& m, int w, int h, bool cyber, u32 seed)
        : a(c, WHITE, (float)BART_K, (float)BART_PAD_L, (float)BART_PAD_T), t(m, WHITE, (float)BART_K, (float)BART_PAD_L, (float)BART_PAD_T), W(w), H(h), cy(cyber), rng(seed) {}

    // ---- ground
    void pad(Color base, Color seam, float wear = 0.10f, float r = 4) {
        a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
        a.rect(0, 0, (float)W, (float)H, r, base, 2.6f, 0.5f);
        for (int x = 32; x < W; x += 32) a.line((float)x, 2, (float)x, H - 2.0f, 0.7f, rgba(seam, 120));
        for (int y = 32; y < H; y += 32) a.line(2, (float)y, W - 2.0f, (float)y, 0.7f, rgba(seam, 120));
        a.grain(0, 0, (float)W, (float)H, wear, rng.next());
    }
    void oilStain(float cx, float cy2, float rx, float ry) { a.ell(cx, cy2, rx, ry, rgba(rgb(18, 18, 20), 70), 1, 0); }

    // ---- volumes
    Roof block(float x, float y, float w, float h, float ht, Color roof, Color wall, float r = 1.5f) {
        a.shadowRect(x, y, w, h, ht * 0.55f + 1, ht * 0.4f + 1, 1.5f + ht * 0.2f, 100);
        a.rect(x, y + h - ht, w, ht + 0.6f, 0.8f, wall, 1.4f, 0.4f);
        a.ramp(x, y + h - ht, x + w, y + h + 0.6f, 1.08f, 0.66f);
        a.line(x + 0.5f, y + h, x + w - 0.5f, y + h, 1.0f, rgba(BLACK, 110));
        a.rect(x, y - ht, w, h, r, roof, 2.3f, 0.58f);
        return {x, y - ht, w, h};
    }
    void windows(const Roof& r, float ht, int n, Color glow, float frac = 0.55f) {
        float wy = r.y + r.h + ht * 0.26f, hh = std::max(1.6f, ht * 0.34f), pitch = r.w / n;
        for (int i = 0; i < n; i++) {
            float cx = r.x + pitch * (i + 0.5f), ww = std::min(pitch * frac, 7.5f);
            a.rect(cx - ww * 0.5f - 0.5f, wy - 0.5f, ww + 1, hh + 1, 0.6f, rgba(BLACK, 150), 0.6f, 0);
            a.rect(cx - ww * 0.5f, wy, ww, hh, 0.5f, glow, 0.8f, 0.25f);
        }
    }
    void door(float cx, float baseY, float w, float h, Color frame, Color inner) {
        a.rect(cx - w * 0.5f - 1, baseY - h - 1, w + 2, h + 1, 0.8f, frame, 1, 0.4f);
        a.rect(cx - w * 0.5f, baseY - h, w, h, 0.6f, inner, 1, 0.1f);
    }
    void wallSeams(const Roof& r, float ht, float pitch, int alpha = 110) {
        for (float x = r.x + pitch; x < r.x + r.w - 1; x += pitch) a.line(x, r.y + r.h + 0.5f, x, r.y + r.h + ht, 0.6f, rgba(BLACK, alpha));
    }
    // ---- roof furniture
    void vent(float x, float y, float w, float h, Color slat) {
        a.rect(x, y, w, h, 0.6f, rgb(20, 22, 26), 1.0f, 0.3f);
        for (float yy = y + 1.1f; yy < y + h - 0.4f; yy += 1.5f) a.line(x + 0.7f, yy, x + w - 0.7f, yy, 0.6f, slat);
    }
    void fan(float cx, float cy2, float r, Color rim, Color blade) {
        a.circle(cx, cy2, r, rim, 1.6f, 0.55f);
        a.circle(cx, cy2, r * 0.8f, rgb(18, 20, 24), 1.0f, 0.2f);
        for (int i = 0; i < 4; i++) { float an = i * 1.5708f + 0.4f; a.cap(cx, cy2, cx + std::cos(an) * r * 0.72f, cy2 + std::sin(an) * r * 0.72f, r * 0.17f, blade, 0.8f, 0.4f); }
        a.dot(cx, cy2, r * 0.2f, rim);
    }
    void light(float x, float y, float r, Color c, int glowA = 120) { a.glow(x, y, r * 3.6f, rgba(c, glowA)); a.dot(x, y, r, c); a.dot(x - r * 0.25f, y - r * 0.3f, r * 0.35f, rgba(WHITE, 200)); }
    void antenna(float x, float y, float ht, Color tip) {
        a.line(x, y, x + ht * 0.45f, y + ht * 0.28f, 0.9f, rgba(BLACK, 55));   // its shadow on the roof
        a.cap(x, y, x, y - ht, 0.55f, rgb(150, 156, 166), 0.8f, 0.4f);
        a.line(x - 1.6f, y - ht * 0.55f, x + 1.6f, y - ht * 0.55f, 0.6f, rgb(120, 126, 136));
        light(x, y - ht, 0.9f, tip, 150);
    }
    // vertical cylinder standing on (cx, by): returns the centre of its top face
    P cyl(float cx, float by, float r, float ht, Color top, Color side) {
        a.shadowCircle(cx, by - r * 0.2f, r, ht * 0.5f, ht * 0.38f, 2.0f + ht * 0.15f, 95);
        a.cap(cx, by - r * 0.1f, cx, by - ht, r, side, r * 0.9f, 0.6f);
        a.ramp(cx - r, by - ht, cx + r, by + r * 0.4f, 1.1f, 0.7f);
        a.circle(cx, by - ht, r, top, 1.8f, 0.55f);
        return {cx, by - ht};
    }
    void hazard(float x, float y, float w, float h, float period = 5, Color ca = rgb(232, 188, 62), Color cb = rgb(36, 36, 32)) {
        Canvas& c = a.c;
        int x0 = std::max(0, (int)a.X(x)), y0 = std::max(0, (int)a.Y(y)), x1 = std::min(c.w - 1, (int)a.X(x + w)), y1 = std::min(c.h - 1, (int)a.Y(y + h));
        float per = period * BART_K;
        for (int py = y0; py < y1; py++) for (int px = x0; px < x1; px++) {
            float k = std::fmod((float)(px + py) / per, 1.0f);
            Color col = k < 0.5f ? ca : cb;
            float edge = std::min(std::min(px - x0, x1 - px), std::min(py - y0, y1 - py)) / 1.5f;
            col = shade(col, py - y0 < (y1 - y0) * 0.4f ? 1.1f : 0.86f);
            if (edge < 1) col = shade(col, 0.6f + 0.4f * edge);
            c.px[py * c.w + px] = Canvas::pack(col);
        }
    }
    void sandbags(float x0, float y0, float x1, float y1, int rows = 1) {
        float len = hyp(x1 - x0, y1 - y0); int n = std::max(2, (int)(len / 5.2f));
        for (int row = 0; row < rows; row++) for (int i = 0; i < n; i++) {
            float k = (i + 0.5f * (row & 1)) / n;
            float px = x0 + (x1 - x0) * k, py = y0 + (y1 - y0) * k - row * 2.6f;
            float sh = 0.9f + rng.f(0, 0.16f);
            a.ell(px, py, 3.5f, 2.5f, shade(K_SAND, sh), 1.4f, 0.6f);
            a.line(px - 1.6f, py, px + 1.6f, py, 0.5f, rgba(K_DARK, 70));
        }
    }
    void crate(float x, float y, float w, float h, Color c) {
        a.shadowRect(x, y, w, h, 1.5f, 1.2f, 1.5f, 80);
        a.rect(x, y, w, h, 0.5f, c, 1.4f, 0.6f);
        a.line(x + 0.6f, y + 0.6f, x + w - 0.6f, y + h - 0.6f, 0.5f, rgba(BLACK, 70)); a.line(x + w - 0.6f, y + 0.6f, x + 0.6f, y + h - 0.6f, 0.5f, rgba(BLACK, 70));
    }
    void barrel(float cx, float cy2, float r, Color c) {
        a.shadowCircle(cx, cy2, r, 1.5f, 1.2f, 1.5f, 85);
        a.circle(cx, cy2, r, c, 1.4f, 0.6f);
        a.ring(cx, cy2, r * 0.62f, 0.6f, shade(c, 0.7f), 0.5f, 0.2f);
        a.dot(cx - r * 0.2f, cy2 - r * 0.25f, r * 0.2f, shade(c, 1.5f));
    }
    void container(float x, float y, float w, float h, Color c) {   // shipping container seen from above, ribbed along its length
        a.shadowRect(x, y, w, h, 2, 1.6f, 1.8f, 90);
        a.rect(x, y, w, h, 0.6f, c, 1.4f, 0.55f);
        for (float xx = x + 2.2f; xx < x + w - 1.5f; xx += 2.4f) a.line(xx, y + 0.8f, xx, y + h - 0.8f, 0.55f, rgba(BLACK, 70));
        a.rect(x + w - 2.6f, y, 2.6f, h, 0.4f, shade(c, 0.78f), 0.8f, 0.3f);
    }
    void pipe(float ax, float ay, float bx, float by, float r, Color c) {
        a.line(ax + 1, ay + 1.4f, bx + 1, by + 1.4f, r * 2, rgba(BLACK, 60));
        a.cap(ax, ay, bx, by, r, c, r * 0.9f, 0.6f);
    }
    // team marks: a dark seat in the colour painter, the white mask in the team painter (tinted live by the owner's colour)
    void mark(float x, float y, float w, float h) {
        a.rect(x - 0.8f, y - 0.8f, w + 1.6f, h + 1.6f, 0.7f, rgb(20, 22, 28), 0.8f, 0.2f);
        t.rect(x, y, w, h, 0.6f, WHITE, 1.2f, 0.45f);
    }
    void markDisc(float cx, float cy2, float r) {
        a.circle(cx, cy2, r + 0.9f, rgb(20, 22, 28), 0.8f, 0.2f);
        t.circle(cx, cy2, r, WHITE, r * 0.9f, 0.55f);
    }
    void stripe(float x0, float y0, float x1, float y1, float w, Color c, int glowA = 90) {
        if (glowA) a.line(x0, y0, x1, y1, w * 3.0f, rgba(c, glowA * 0.4f));
        a.line(x0, y0, x1, y1, w, c);
    }
    // sawtooth skylights: parallel glass strips over a roof
    void skylights(const Roof& r, int n, Color glass, Color frame) {
        float pitch = (r.w - 6) / n;
        for (int i = 0; i < n; i++) {
            float x = r.x + 3 + i * pitch;
            a.rect(x, r.y + 3, pitch * 0.55f, r.h - 6, 0.5f, frame, 0.9f, 0.4f);
            a.rect(x + 0.7f, r.y + 3.8f, pitch * 0.55f - 1.4f, r.h - 7.6f, 0.4f, glass, 0.8f, 0.3f);
            a.line(x + pitch * 0.55f * 0.35f, r.y + 4, x + pitch * 0.55f * 0.35f, r.y + r.h - 4, 0.5f, rgba(WHITE, 90));
        }
    }
    void corrugated(const Roof& r, float pitch, Color dark, int alpha = 90) {   // roof sheets
        for (float x = r.x + pitch; x < r.x + r.w - 0.5f; x += pitch) {
            a.line(x, r.y + 1, x, r.y + r.h - 1, 0.6f, rgba(dark, alpha));
            a.line(x + 0.6f, r.y + 1, x + 0.6f, r.y + r.h - 1, 0.4f, rgba(WHITE, 28));
        }
    }
    void rust(float x0, float y0, float x1, float y1, int n, int alpha = 90) {
        for (int i = 0; i < n; i++) { float px = rng.f(x0, x1), py = rng.f(y0, y1); a.ell(px, py, rng.f(1.5f, 4.5f), rng.f(1, 3), rgba(rgb(130, 70, 36), alpha), 1, 0); }
    }
    void dishTop(float cx, float cy2, float r, Color rim, Color inner) {   // fixed satellite dish seen from above, tilted toward the south-east
        a.shadowCircle(cx, cy2, r, 2.5f, 2, 2, 85);
        a.circle(cx, cy2, r, rim, 1.6f, 0.6f);
        a.circle(cx + r * 0.1f, cy2 + r * 0.1f, r * 0.82f, inner, 2.0f, 0.4f);
        a.line(cx, cy2, cx + r * 0.9f, cy2 + r * 0.9f, 0.8f, rim);
        a.dot(cx + r * 0.9f, cy2 + r * 0.9f, r * 0.14f, rim);
    }
    void domeTop(float cx, float cy2, float r, Color c) {   // radome: a pale sphere with panel seams
        a.shadowCircle(cx, cy2, r, 3, 2.4f, 2.4f, 95);
        a.circle(cx, cy2, r, c, r * 0.95f, 0.7f);
        for (int i = 0; i < 3; i++) a.ring(cx, cy2, r * (0.35f + i * 0.28f), 0.45f, rgba(BLACK, 40), 0.5f, 0);
        a.line(cx - r, cy2, cx + r, cy2, 0.45f, rgba(BLACK, 35)); a.line(cx, cy2 - r, cx, cy2 + r, 0.45f, rgba(BLACK, 35));
        a.dot(cx - r * 0.35f, cy2 - r * 0.4f, r * 0.2f, rgba(WHITE, 190));
    }
};


}  // namespace
