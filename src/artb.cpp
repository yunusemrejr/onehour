#include "artb.h"
#include "paint.h"

using namespace paint;

// Structures are drawn in a raised 3/4 view: each volume has a ground rectangle, a roof lifted by its height and a south wall in
// between, with soft shadows falling south-east (light from the north-west, like every other sprite). Cyber is ceramic and cobalt with
// emitter light; Clanker is olive steel, corrugated roofs, sandbags, rust and hazard paint.
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

// ================================================================= CYBER
void hqCyber(Bld& b) {
    int W = b.W, H = b.H; (void)H;
    b.pad(C_PAD, C_SEAM);
    // chamfered inner plinth with an emitter line round it
    b.a.rect(5, 5, W - 10.0f, H - 10.0f, 6, C_PAD2, 2.2f, 0.45f);
    b.stripe(8, 8, W - 8.0f, 8, 0.9f, C_CYAN); b.stripe(8, H - 8.0f, W - 8.0f, H - 8.0f, 0.9f, C_CYAN);
    b.stripe(8, 8, 8, H - 8.0f, 0.9f, C_CYAN); b.stripe(W - 8.0f, 8, W - 8.0f, H - 8.0f, 0.9f, C_CYAN);
    b.a.ngon(64, 112, 9, 6, 0.5236f, rgba(C_CYAN, 160), 1, 0); b.a.ngon(64, 112, 6.5f, 6, 0.5236f, C_PAD2, 1, 0.2f);   // landing hex in the forecourt
    // rear spine
    Roof sp = b.block(14, 16, 100, 26, 12, C_ROOF2, C_HULL_D);
    b.skylights(sp, 6, C_GLASS, C_STEEL);
    b.windows(sp, 12, 9, C_CYAN);
    // side wings
    Roof wl = b.block(10, 52, 34, 52, 10, C_ROOF, C_HULL);
    Roof wr = b.block(84, 52, 34, 52, 10, C_ROOF, C_HULL);
    for (Roof* r : { &wl, &wr }) {
        for (int i = 0; i < 3; i++) b.vent(r->x + 4 + i * 10, r->y + 6, 7, 5, rgba(C_CYAN, 160));
        b.fan(r->x + r->w * 0.5f, r->y + r->h - 14, 7, rgb(112, 126, 152), C_CYAN);
        b.stripe(r->x + 3, r->y + r->h - 4, r->x + r->w - 3, r->y + r->h - 4, 0.7f, C_CYAN, 70);
    }
    b.windows(wl, 10, 4, C_CYAN); b.windows(wr, 10, 4, C_CYAN);
    // command tower
    Roof tw = b.block(44, 46, 40, 52, 22, C_ROOF, C_HULL);
    b.wallSeams(tw, 22, 8);
    b.windows(tw, 22, 5, C_CYAN, 0.6f);
    b.door(64, tw.y + tw.h + 22, 12, 12, C_STEEL, rgb(14, 26, 36));
    b.stripe(58, tw.y + tw.h + 22 - 13.5f, 70, tw.y + tw.h + 22 - 13.5f, 0.8f, C_CYAN, 120);
    float cx = tw.x + tw.w * 0.5f, cy = tw.y + tw.h * 0.44f;
    b.a.circle(cx, cy, 15.5f, C_ROOF2, 2.4f, 0.55f);
    b.a.ring(cx, cy, 12.5f, 2.2f, C_CYAN, 1, 0.3f); b.a.glow(cx, cy, 24, rgba(C_CYAN, 100));
    b.a.circle(cx, cy, 9.5f, C_GLASS, 3.0f, 0.75f);
    b.a.circle(cx, cy, 4.6f, C_WHITE, 2.4f, 0.6f); b.a.glow(cx, cy, 10, rgba(C_WHITE, 130));
    for (int i = 0; i < 8; i++) { float an = i * 0.7854f; b.a.dot(cx + std::cos(an) * 14, cy + std::sin(an) * 14, 0.7f, C_CYAN); }
    b.mark(tw.x + 3, tw.y + 2.5f, tw.w - 6, 4.2f);          // owner's colour: a broad band across the tower roof
    b.mark(sp.x + 4, sp.y + sp.h - 5.2f, 18, 3.4f); b.mark(sp.x + sp.w - 22, sp.y + sp.h - 5.2f, 18, 3.4f);
    b.antenna(tw.x + 4, tw.y + tw.h - 5, 18, C_RED); b.antenna(tw.x + tw.w - 4, tw.y + tw.h - 5, 12, C_CYAN);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 7);
}

void powerCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    Roof ctl = b.block(6, 6, W - 12.0f, 14, 9, C_ROOF2, C_HULL_D);
    b.windows(ctl, 9, 8, C_CYAN); b.skylights(ctl, 5, C_GLASS, C_STEEL);
    b.mark(ctl.x + ctl.w - 20, ctl.y + 2, 16, 3.2f);
    for (int i = 0; i < 2; i++) {
        float cx = 26 + i * 44;
        b.pipe(cx, 30, cx, 50, 3.6f, rgb(60, 78, 110));
        P top = b.cyl(cx, 54, 15, 15, C_ROOF2, C_HULL);
        b.a.ring(top.x, top.y, 11.5f, 1.8f, C_CYAN, 1, 0.3f); b.a.glow(top.x, top.y, 22, rgba(C_CYAN, 140));
        b.a.circle(top.x, top.y, 7.8f, C_STEEL, 1.6f, 0.5f);
        b.a.circle(top.x, top.y, 4.6f, C_WHITE, 2.4f, 0.65f); b.a.glow(top.x, top.y, 9, rgba(C_WHITE, 160));
        for (int k = 0; k < 6; k++) { float an = k * 1.0472f; b.a.dot(top.x + std::cos(an) * 13.6f, top.y + std::sin(an) * 13.6f, 0.8f, C_CYAN); }
    }
    b.pipe(26, 33, 70, 33, 2.4f, C_ROOF2);
    for (int i = 0; i < 5; i++) { b.a.rect(8 + i * 5.2f, 56, 3.6f, 5, 0.5f, C_STEEL, 0.8f, 0.4f); b.light(9.8f + i * 5.2f, 58.4f, 0.8f, i % 2 ? C_CYAN : rgb(120, 255, 190), 80); }
    b.mark(W - 22.0f, H - 10.0f, 14, 3.4f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 11);
}

void supplyCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    Roof wh = b.block(8, 8, W - 16.0f, 42, 16, C_ROOF2, C_HULL);
    for (int i = 1; i < 6; i++) b.a.line(wh.x + i * (wh.w / 6), wh.y + 1, wh.x + i * (wh.w / 6), wh.y + wh.h - 1, 0.7f, rgba(C_STEEL, 150));   // roof ribs
    b.skylights(wh, 3, C_GLASS, C_STEEL);
    b.wallSeams(wh, 16, 10);
    b.door(30, wh.y + wh.h + 16, 26, 13, C_STEEL, rgb(10, 20, 30));
    b.stripe(18, wh.y + wh.h + 16 - 14.5f, 42, wh.y + wh.h + 16 - 14.5f, 0.8f, C_CYAN, 100);
    b.door(64, wh.y + wh.h + 16, 18, 11, C_STEEL, rgb(10, 20, 30));
    b.mark(wh.x + wh.w - 26, wh.y + 3, 22, 4);
    b.vent(wh.x + 6, wh.y + 8, 9, 6, rgba(C_CYAN, 150));
    // containers in the yard
    Color cs[4] = { rgb(46, 112, 120), rgb(70, 92, 130), rgb(60, 70, 90), rgb(40, 100, 96) };
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) b.container(52 + i * 17.0f, 62 + j * 11.0f, 15.5f, 9, cs[(i + j * 2) & 3]);
    // gantry crane over the bay forecourt
    b.a.rect(8, 62, 3, 3, 0.5f, C_ROOF2, 1, 0.5f); b.a.rect(8, 84, 3, 3, 0.5f, C_ROOF2, 1, 0.5f);
    b.pipe(9.5f, 63.5f, 9.5f, 85.5f, 1.5f, C_ROOF);
    b.pipe(9.5f, 74, 44, 74, 1.5f, C_ROOF);
    b.a.rect(26, 71.5f, 6, 5, 0.6f, C_STEEL, 1, 0.4f); b.light(29, 74, 1.0f, C_AMBER, 100);
    b.hazard(14, 52, 36, 3.2f);
    // hologram beacon marking the dropoff
    b.a.ring(24, 84, 6, 1.1f, rgba(C_CYAN, 210), 0.8f, 0); b.a.glow(24, 84, 14, rgba(C_CYAN, 90)); b.a.dot(24, 84, 1.6f, C_WHITE);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 13);
}

void barracksCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    Roof d1 = b.block(6, 7, 38, 34, 10, C_ROOF, C_HULL);
    Roof d2 = b.block(52, 7, 38, 34, 10, C_ROOF, C_HULL);
    Roof cor = b.block(40, 18, 16, 14, 7, C_ROOF2, C_HULL_D);
    for (Roof* r : { &d1, &d2 }) {
        for (int i = 0; i < 4; i++) b.a.rect(r->x + 4 + i * 8.5f, r->y + 5, 5.5f, 11, 0.8f, C_ROOF2, 1.2f, 0.5f);   // sleeping pods on the roof
        b.vent(r->x + 4, r->y + r->h - 9, 12, 5, rgba(C_CYAN, 150));
        b.a.dot(r->x + r->w - 7, r->y + r->h - 7, 2.5f, C_STEEL); b.light(r->x + r->w - 7, r->y + r->h - 7, 1.1f, C_CYAN, 100);
        b.stripe(r->x + 2, r->y + r->h - 2.2f, r->x + r->w - 2, r->y + r->h - 2.2f, 0.7f, C_CYAN, 70);
    }
    b.windows(d1, 10, 5, C_CYAN); b.windows(d2, 10, 5, C_CYAN);
    b.door(48, cor.y + cor.h + 7, 9, 8, C_STEEL, rgb(12, 24, 34));
    b.mark(d1.x + d1.w - 12, d1.y + 1.8f, 9, 3); b.mark(d2.x + 3, d2.y + 1.8f, 9, 3);
    b.a.rect(6, 48, W - 12.0f, 10, 3, C_PAD2, 1.4f, 0.4f);   // parade strip
    for (int i = 0; i < 8; i++) b.a.line(10 + i * 10.0f, 53, 14 + i * 10.0f, 53, 0.9f, rgba(C_CYAN, 110));
    b.antenna(cor.x + 8, cor.y + 5, 14, C_RED);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 17);
}

void factoryCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    Roof hall = b.block(8, 6, W - 16.0f, 52, 18, C_ROOF2, C_HULL);
    b.skylights(hall, 7, C_GLASS, C_STEEL);
    b.wallSeams(hall, 18, 12);
    b.door(64, hall.y + hall.h + 18, 36, 17, C_STEEL, rgb(8, 16, 24));
    for (int i = 0; i < 4; i++) b.a.line(46, hall.y + hall.h + 18 - 4 * (i + 0.5f) - 0.2f, 82, hall.y + hall.h + 18 - 4 * (i + 0.5f) - 0.2f, 0.5f, rgba(C_CYAN, 120));
    b.windows(hall, 18, 3, C_CYAN, 0.5f);
    b.mark(hall.x + hall.w - 30, hall.y + 2.2f, 26, 3.6f);
    Roof an = b.block(8, 66, 32, 24, 9, C_ROOF, C_HULL_D);
    b.fan(an.x + 9, an.y + 9, 6, rgb(112, 126, 152), C_CYAN); b.vent(an.x + 18, an.y + 5, 10, 7, rgba(C_CYAN, 150));
    b.windows(an, 9, 4, C_CYAN);
    P cool = b.cyl(106, 84, 11, 13, C_ROOF2, C_HULL);
    b.a.ring(cool.x, cool.y, 7.5f, 1.4f, C_CYAN, 1, 0.3f); b.a.glow(cool.x, cool.y, 14, rgba(C_CYAN, 90));
    // assembly arm and a chassis on the forecourt
    b.pipe(46, 66, 46, 90, 1.4f, C_ROOF); b.pipe(46, 69, 82, 69, 1.4f, C_ROOF); b.a.rect(70, 69, 5, 6, 0.6f, C_STEEL, 1, 0.4f); b.light(72.5f, 71, 1.0f, C_AMBER, 100);
    b.a.rect(52, 76, 28, 11, 3, C_STEEL, 1.6f, 0.5f); b.a.rect(54, 78, 24, 7, 2, C_HULL_D, 1.4f, 0.5f); b.a.dot(58, 81.5f, 1.2f, C_CYAN);
    b.hazard(86, 94 - 4.0f, 30, 3.2f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 19);
}

void airfieldCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)W, (float)H, 4, rgb(46, 54, 70), 2.6f, 0.5f);
    for (int x = 32; x < W; x += 32) b.a.line((float)x, 2, (float)x, H - 2.0f, 0.7f, rgba(C_SEAM, 110));
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 23);
    // landing pads exactly where the aircraft park: centres at W/2 + (slot - 1.5) * 30, y = H/2
    for (int i = 0; i < 4; i++) {
        float cx = W * 0.5f + (i - 1.5f) * 30, cy = H * 0.5f;
        b.a.circle(cx, cy, 12.5f, rgb(36, 44, 58), 1.8f, 0.4f);
        b.a.ngon(cx, cy, 12, 6, 0.5236f, rgba(C_CYAN, 70), 1, 0);
        b.a.ring(cx, cy, 10.5f, 0.9f, rgba(C_CYAN, 200), 0.8f, 0);
        b.a.line(cx - 4, cy - 4.5f, cx - 4, cy + 4.5f, 1.2f, rgba(C_WHITE, 200)); b.a.line(cx + 4, cy - 4.5f, cx + 4, cy + 4.5f, 1.2f, rgba(C_WHITE, 200)); b.a.line(cx - 4, cy, cx + 4, cy, 1.2f, rgba(C_WHITE, 200));
    }
    for (int i = 0; i < 10; i++) { float x = 8 + i * (W - 16.0f) / 9; b.light(x, 4.5f, 0.8f, C_CYAN, 80); b.light(x, H - 4.5f, 0.8f, C_CYAN, 80); }
    // control tower with a sensor mast
    Roof tw = b.block(6, 6, 26, 20, 22, C_ROOF, C_HULL);
    b.windows(tw, 22, 3, C_CYAN, 0.7f); b.wallSeams(tw, 22, 9);
    b.a.rect(tw.x + 3, tw.y + 3, tw.w - 6, tw.h - 6, 2, C_GLASS, 2, 0.6f); b.a.line(tw.x + 5, tw.y + 5, tw.x + tw.w - 7, tw.y + 5, 0.8f, rgba(C_WHITE, 150));
    b.mark(tw.x + 3, tw.y + tw.h - 5, tw.w - 6, 2.8f);
    b.dishTop(tw.x + 6, tw.y - 2, 5, C_ROOF2, rgb(30, 40, 56)); b.antenna(tw.x + tw.w - 4, tw.y + 4, 14, C_RED);
    // hangar
    Roof hg = b.block(108, 6, 46, 26, 14, C_ROOF2, C_HULL_D);
    for (int i = 0; i < 6; i++) b.a.line(hg.x + 3 + i * 7.4f, hg.y + 1, hg.x + 3 + i * 7.4f, hg.y + hg.h - 1, 0.7f, rgba(C_STEEL, 150));
    b.door(hg.x + hg.w * 0.5f, hg.y + hg.h + 14, 30, 12, C_STEEL, rgb(8, 16, 24));
    b.stripe(hg.x + 8, hg.y + hg.h + 14 - 13.4f, hg.x + hg.w - 8, hg.y + hg.h + 14 - 13.4f, 0.8f, C_CYAN, 100);
    b.mark(hg.x + hg.w - 14, hg.y + 2, 10, 3);
    // charging bay
    Roof sv = b.block(8, 70, 38, 18, 8, C_ROOF, C_HULL_D);
    for (int i = 0; i < 4; i++) { b.a.rect(sv.x + 4 + i * 8.5f, sv.y + 4, 5, 9, 0.6f, C_STEEL, 1, 0.4f); b.light(sv.x + 6.5f + i * 8.5f, sv.y + 6, 0.9f, rgb(120, 255, 190), 80); }
    b.windows(sv, 8, 3, C_CYAN);
    b.hazard(112, 88.0f, 40, 3.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 29);
}

void techCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    Roof hall = b.block(10, 16, 76, 52, 18, C_ROOF2, C_HULL);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 6; i++) { float x = hall.x + 4 + i * 11.8f, y = hall.y + 4 + j * 15; b.a.rect(x, y, 9.5f, 12, 0.7f, C_STEEL, 1.1f, 0.45f); for (int k = 0; k < 4; k++) b.a.line(x + 1.2f, y + 2.4f + k * 2.4f, x + 8.3f, y + 2.4f + k * 2.4f, 0.55f, rgba(((i + j + k) % 3) ? C_CYAN : rgb(255, 130, 90), 190)); }
    b.fan(hall.x + hall.w - 11, hall.y + hall.h - 9, 6.5f, rgb(112, 126, 152), C_CYAN);
    b.wallSeams(hall, 18, 11); b.windows(hall, 18, 4, C_CYAN, 0.5f);
    b.door(hall.x + 16, hall.y + hall.h + 18, 12, 12, C_STEEL, rgb(10, 20, 30));
    b.mark(hall.x + 3, hall.y + hall.h - 5.2f, 20, 3.4f);
    b.a.glow(hall.x + hall.w * 0.5f, hall.y + hall.h * 0.5f, 34, rgba(C_CYAN, 38));
    // cooling coils with a cyan wash
    for (int i = 0; i < 4; i++) { float x = 14 + i * 19; b.a.rect(x, 76, 15, 10, 1.4f, C_ROOF, 1.4f, 0.5f); b.a.rect(x + 1.5f, 77.5f, 12, 7, 1, C_GLASS, 1, 0.3f); b.a.line(x + 2, 81, x + 13, 81, 0.8f, rgba(C_WHITE, 150)); }
    // fixed mount for the live-rotating dish and a lightning mast
    b.a.circle(70, hall.y + 14, 9.2f, C_STEEL, 1.6f, 0.5f); b.a.circle(70, hall.y + 14, 6, C_ROOF2, 1.6f, 0.5f);
    b.antenna(14, hall.y + 4, 28, C_RED); b.antenna(hall.x + hall.w - 4, hall.y + hall.h - 3, 16, C_CYAN);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 31);
}

void laserBase(Bld& b) {   // 1x1 turret pedestal: armoured ring on a hex pad
    b.a.shadowCircle(16, 16, 15, 2.5f, 2.5f, 3, 95);
    b.a.ngon(16, 16, 15.4f, 8, 0.3927f, C_PAD, 2.2f, 0.5f);
    b.a.circle(16, 16, 12.2f, C_STEEL, 2.4f, 0.6f);
    b.a.ring(16, 16, 10.2f, 1.0f, rgba(C_CYAN, 210), 0.8f, 0); b.a.glow(16, 16, 15, rgba(C_CYAN, 70));
    b.a.circle(16, 16, 9, C_ROOF2, 2.4f, 0.55f);
    for (int i = 0; i < 4; i++) { float an = i * 1.5708f + 0.785f; b.a.dot(16 + std::cos(an) * 13.2f, 16 + std::sin(an) * 13.2f, 1.2f, C_ROOF); }
    b.markDisc(16, 27.5f, 1.6f);
}

void patriotCyber(Bld& b) {   // 2x2: armoured plinth with generator and radar mast; the launcher head rotates over the middle
    int W = b.W;
    b.pad(C_PAD, C_SEAM, 0.1f, 5);
    Roof pl = b.block(10, 18, 44, 36, 7, C_ROOF2, C_HULL_D);
    b.a.rect(pl.x + 3, pl.y + 3, pl.w - 6, pl.h - 6, 2, C_STEEL, 1.4f, 0.4f);
    b.stripe(pl.x + 5, pl.y + 5, pl.x + pl.w - 5, pl.y + 5, 0.7f, C_CYAN, 70);
    b.windows(pl, 7, 5, C_CYAN, 0.5f);
    Roof gen = b.block(4, 6, 14, 10, 6, C_ROOF, C_HULL_D); b.vent(gen.x + 2, gen.y + 2, 10, 6, rgba(C_CYAN, 150));
    b.antenna(W - 8.0f, 14, 22, C_RED); b.dishTop(W - 12.0f, 8, 4.4f, C_ROOF2, rgb(30, 40, 56));
    for (int i = 0; i < 4; i++) { float x = i < 2 ? 4.0f : W - 8.0f, y = (i & 1) ? 54.0f : 24.0f; b.a.rect(x, y, 4, 6, 0.8f, C_ROOF, 1.2f, 0.5f); }
    b.mark(W - 22.0f, 54, 14, 3.2f);
}

void minerCyber(Bld& b) {   // bitcoin datacenter: three server containers, gold coin on the roof, heat exchangers
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    for (int i = 0; i < 3; i++) {
        float x = 4 + i * 30;
        Roof r = b.block(x, 8, 28, 36, 10, C_ROOF2, C_HULL_D);
        for (int j = 0; j < 3; j++) b.fan(r.x + 8 + (j % 2) * 12, r.y + 8 + j * 9.5f, 4.6f, rgb(112, 126, 152), C_GOLD);
        b.a.rect(r.x + 2, r.y + r.h - 5, r.w - 4, 3, 0.6f, C_STEEL, 0.9f, 0.3f);
        for (int k = 0; k < 8; k++) b.light(r.x + 4 + k * 3.0f, r.y + r.h - 3.5f, 0.55f, (k + i) % 3 ? rgb(120, 255, 190) : C_GOLD, 60);
        b.windows(r, 10, 3, rgb(255, 214, 120), 0.45f);
        b.stripe(r.x + 2, r.y + 1.8f, r.x + r.w - 2, r.y + 1.8f, 0.7f, C_GOLD, 80);
    }
    // the coin: a gold disc with the double-barred B-stroke, on the middle container
    float cx = 4 + 30 + 14, cy = 8 - 10 + 12;
    b.a.glow(cx, cy, 20, rgba(C_GOLD, 120));
    b.a.circle(cx, cy, 8.4f, rgb(212, 150, 40), 2.2f, 0.6f); b.a.circle(cx, cy, 7, C_GOLD, 2.2f, 0.6f);
    b.a.line(cx - 2, cy - 4.2f, cx - 2, cy + 4.2f, 1.3f, rgb(120, 78, 20)); b.a.line(cx - 2, cy - 3.2f, cx + 2.2f, cy - 3.2f, 1.2f, rgb(120, 78, 20)); b.a.line(cx - 2, cy, cx + 2.6f, cy, 1.2f, rgb(120, 78, 20)); b.a.line(cx - 2, cy + 3.2f, cx + 2.4f, cy + 3.2f, 1.2f, rgb(120, 78, 20));
    b.a.line(cx - 0.4f, cy - 5.6f, cx - 0.4f, cy - 4, 0.8f, rgb(120, 78, 20)); b.a.line(cx + 1.2f, cy - 5.6f, cx + 1.2f, cy - 4, 0.8f, rgb(120, 78, 20));
    // coolant lines and exchangers along the south edge
    b.pipe(6, 54, W - 6.0f, 54, 1.6f, rgb(80, 100, 140)); b.pipe(6, 58, W - 6.0f, 58, 1.1f, C_GOLD);
    for (int i = 0; i < 3; i++) { b.a.rect(10 + i * 30.0f, 47, 12, 5, 0.8f, C_ROOF, 1.2f, 0.5f); }
    b.mark(W - 22.0f, H - 8.0f, 16, 3.2f);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 37);
}

void nukeCyber(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(C_PAD, C_SEAM);
    b.hazard(4, 4, W - 8.0f, 3.4f); b.hazard(4, H - 7.4f, W - 8.0f, 3.4f);
    // silo pit with two sliding doors drawn partly open, the warhead showing in the shaft
    float cx = 48, cy = 50;
    b.a.circle(cx, cy, 29, C_PAD2, 2.6f, 0.5f);
    b.a.circle(cx, cy, 24, rgb(10, 12, 16), 1.6f, 0.3f);
    for (int i = 0; i < 3; i++) b.a.ring(cx, cy, 8.0f + i * 5.5f, 0.8f, rgba(C_CYAN, 60), 0.6f, 0);   // the empty shaft: concentric guide rings
    for (int i = 0; i < 12; i++) { float an = i * 0.5236f; b.a.dot(cx + std::cos(an) * 22.0f, cy + std::sin(an) * 22.0f, 0.9f, i % 3 ? rgba(C_CYAN, 160) : rgba(C_RED, 200)); }
    for (int s = -1; s <= 1; s += 2) {   // blast doors, slid aside to either hand
        b.a.rect(cx + s * 21 - 12, cy - 22, 24, 44, 3, C_ROOF2, 2.2f, 0.55f);
        b.a.rect(cx + s * 21 - 10, cy - 20, 20, 40, 2, C_STEEL, 1.4f, 0.4f);
        b.a.line(cx + s * 21, cy - 19, cx + s * 21, cy + 19, 0.7f, rgba(C_CYAN, 130));
    }
    b.a.ring(cx, cy, 27, 1.2f, rgba(C_RED, 200), 0.8f, 0);
    // gantry towers and fuel tanks
    Roof g1 = b.block(5, 12, 12, 56, 18, C_ROOF, C_HULL_D); Roof g2 = b.block(W - 17.0f, 12, 12, 56, 18, C_ROOF, C_HULL_D);
    for (Roof* r : { &g1, &g2 }) { for (int i = 0; i < 5; i++) b.a.line(r->x + 1, r->y + 4 + i * 10.0f, r->x + r->w - 1, r->y + 4 + i * 10.0f, 0.7f, rgba(C_STEEL, 160)); b.light(r->x + r->w * 0.5f, r->y + 3, 1.2f, C_RED, 130); b.windows(*r, 18, 1, C_CYAN, 0.6f); }
    b.cyl(34, H - 8.0f, 6, 7, C_ROOF, C_HULL); b.cyl(62, H - 8.0f, 6, 7, C_ROOF, C_HULL);
    b.mark(g1.x + 1.5f, g1.y + g1.h - 6, 9, 3); b.mark(g2.x + 1.5f, g2.y + g2.h - 6, 9, 3);
    b.a.grain(0, 0, (float)W, (float)H, 0.05f, 41);
}

// ================================================================= CLANKER
void hqClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.rust(4, 4, W - 4.0f, H - 4.0f, 14, 50);
    // rear fuel tanks
    for (int i = 0; i < 3; i++) { P tp = b.cyl(34 + i * 30.0f, 22, 8, 8, shade(K_STEEL, 1.1f), K_RUST); b.a.ring(tp.x, tp.y, 5, 0.8f, K_DARK, 0.6f, 0.2f); b.a.dot(tp.x, tp.y, 1.3f, K_YEL); }
    // radio shack, generator shed and the main bunker
    Roof rs = b.block(8, 44, 24, 28, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(rs, 3.2f, K_DARK); b.windows(rs, 10, 2, rgb(255, 214, 120), 0.5f);
    Roof gs = b.block(96, 48, 24, 32, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(gs, 3.2f, K_DARK); b.a.circle(gs.x + 8, gs.y + 8, 4, K_STEEL, 1.6f, 0.55f); b.a.dot(gs.x + 8, gs.y + 8, 1.6f, K_DARK);
    b.pipe(gs.x + 8, gs.y + 8, gs.x + 8, gs.y - 12, 2.2f, K_STEEL);
    Roof mb = b.block(34, 36, 60, 56, 16, K_KHAKI, K_OLIVE);
    b.a.rect(mb.x + 3, mb.y + 3, mb.w - 6, mb.h - 6, 2, K_ROOF, 1.8f, 0.5f);
    b.corrugated(Roof{mb.x + 3, mb.y + 3, mb.w - 6, mb.h - 6}, 4.0f, K_DARK);
    b.a.rect(mb.x + 6, mb.y + 6, 12, 9, 1, K_STEEL, 1.4f, 0.5f); b.a.line(mb.x + 12, mb.y + 6, mb.x + 12, mb.y + 15, 0.6f, rgba(K_DARK, 160));   // roof hatch
    b.a.circle(mb.x + mb.w - 12, mb.y + 12, 5, K_STEEL, 1.6f, 0.55f); b.a.dot(mb.x + mb.w - 12, mb.y + 12, 2, K_DARK);                              // vent stack
    b.a.rect(mb.x + 6, mb.y + mb.h - 14, 22, 8, 1, K_OLIVE_D, 1.4f, 0.4f);
    b.mark(mb.x + mb.w - 26, mb.y + mb.h - 13, 20, 6);
    for (int i = 0; i < 4; i++) b.a.rect(mb.x + 6 + i * 13.0f, mb.y + mb.h + 16 * 0.25f, 8, 2.6f, 0.5f, rgb(255, 214, 120), 0.8f, 0.2f);   // firing slits on the south face
    b.door(64, mb.y + mb.h + 16, 14, 12, K_DARK, rgb(20, 20, 16));
    b.hazard(55, mb.y + mb.h + 16 + 1.5f, 18, 3);
    // radio mast with guy wires and a dish
    b.antenna(rs.x + 6, rs.y + 6, 34, K_RED);
    b.a.line(rs.x + 6, rs.y + 6 - 28, rs.x - 3, rs.y + 14, 0.5f, rgba(K_DARK, 190)); b.a.line(rs.x + 6, rs.y + 6 - 28, rs.x + 20, rs.y + 14, 0.5f, rgba(K_DARK, 190));
    b.a.circle(rs.x + 17, rs.y + 17, 7.2f, K_STEEL, 1.6f, 0.5f); b.a.circle(rs.x + 17, rs.y + 17, 4.8f, shade(K_STEEL, 0.7f), 1.4f, 0.4f);   // turntable for the live radar bar
    for (int i = 0; i < 5; i++) b.barrel(8 + (i % 2) * 6.0f, 82 + (i / 2) * 5.0f, 2.8f, i % 2 ? K_RUST : K_OLIVE_D);
    // sandbag perimeter with the gate left open
    b.sandbags(4, H - 5.0f, 44, H - 5.0f); b.sandbags(84, H - 5.0f, W - 4.0f, H - 5.0f); b.sandbags(4, 8, 4, H - 6.0f); b.sandbags(W - 4.0f, 8, W - 4.0f, H - 6.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 43);
}

void powerClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.oilStain(30, 54, 14, 4);
    Roof hall = b.block(5, 12, 62, 38, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(hall, 3.2f, K_DARK); b.rust(hall.x, hall.y, hall.x + hall.w, hall.y + hall.h, 7, 80);
    b.vent(hall.x + 5, hall.y + 6, 16, 8, rgba(WHITE, 90));
    b.windows(hall, 14, 4, rgb(255, 214, 120), 0.5f); b.wallSeams(hall, 14, 14);
    b.door(36, hall.y + hall.h + 14, 20, 12, K_DARK, rgb(24, 22, 18)); b.hazard(26, hall.y + hall.h + 14 + 1.2f, 20, 3);
    b.mark(hall.x + hall.w - 22, hall.y + 3, 18, 3.6f);
    // exhaust stacks and the day tank
    for (int i = 0; i < 2; i++) { P tp = b.cyl(76 + i * 11.0f, 34, 4.8f, 26 - i * 4, rgb(50, 46, 42), K_RUST); b.a.ring(tp.x, tp.y, 3.4f, 0.8f, K_DARK, 0.5f, 0.2f); b.a.glow(tp.x, tp.y, 7, rgba(rgb(255, 120, 40), 60)); }
    P tank = b.cyl(78, 52, 8, 11, K_STEEL, K_OLIVE); b.a.dot(tank.x, tank.y, 2.2f, K_YEL);
    b.pipe(66, 40, 74, 48, 1.8f, K_STEEL); b.pipe(hall.x + hall.w, hall.y + 10, 76, 30, 1.6f, K_RUST);
    for (int i = 0; i < 3; i++) b.barrel(8 + i * 5.0f, 58, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 47);
}

void supplyClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    b.rust(4, 50, W - 4.0f, H - 4.0f, 10, 45);
    Roof wh = b.block(8, 8, W - 16.0f, 42, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(wh, 3.4f, K_DARK); b.a.line(wh.x + 1, wh.y + wh.h * 0.5f, wh.x + wh.w - 1, wh.y + wh.h * 0.5f, 1.0f, rgba(WHITE, 40));
    b.rust(wh.x, wh.y, wh.x + wh.w, wh.y + wh.h, 8, 80);
    b.wallSeams(wh, 14, 10);
    b.door(28, wh.y + wh.h + 14, 24, 12, K_DARK, rgb(22, 20, 16)); b.door(64, wh.y + wh.h + 14, 16, 11, K_DARK, rgb(22, 20, 16));
    b.hazard(16, wh.y + wh.h + 14 + 1.2f, 24, 3);
    b.mark(wh.x + wh.w - 24, wh.y + 3, 20, 3.6f);
    b.vent(wh.x + 8, wh.y + 8, 10, 6, rgba(WHITE, 80));
    // stacked pallets and a tarp-covered heap in the yard
    for (int j = 0; j < 2; j++) for (int i = 0; i < 4; i++) b.crate(10 + i * 11.0f, 64 + j * 11.0f, 9, 9, shade(rgb(158, 124, 74), 0.9f + ((i + j) % 3) * 0.08f));
    b.a.ell(70, 78, 15, 9, K_TARP, 3, 0.6f); b.a.line(58, 77, 82, 80, 0.8f, rgba(K_DARK, 110)); b.a.line(64, 71, 76, 85, 0.8f, rgba(K_DARK, 90));
    b.a.rect(56, 64, 3, 3, 0.4f, K_STEEL, 1, 0.5f); b.pipe(57.5f, 65.5f, 88, 65.5f, 1.0f, K_STEEL);   // small derrick arm
    b.sandbags(8, H - 5.0f, 40, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 53);
}

void barracksClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    Roof hut = b.block(6, 8, W - 12.0f, 36, 12, K_ROOF, K_OLIVE_D);
    // pitched roof: the north slope catches the light, the south slope sits in shade, a ridge between
    b.a.rect(hut.x, hut.y + hut.h * 0.5f, hut.w, hut.h * 0.5f, 1.2f, shade(K_ROOF, 0.78f), 1.4f, 0.4f);
    b.a.line(hut.x + 1, hut.y + hut.h * 0.5f, hut.x + hut.w - 1, hut.y + hut.h * 0.5f, 1.4f, rgba(WHITE, 60));
    b.corrugated(hut, 3.2f, K_DARK, 70); b.rust(hut.x, hut.y, hut.x + hut.w, hut.y + hut.h, 6, 70);
    b.wallSeams(hut, 12, 12); b.windows(hut, 12, 6, rgb(255, 214, 120), 0.4f);
    b.door(W * 0.5f, hut.y + hut.h + 12, 12, 10, K_DARK, rgb(22, 20, 16));
    P ch = b.cyl(hut.x + 12, hut.y + 12, 3.4f, 9, rgb(50, 46, 42), K_BRICK); (void)ch;
    b.mark(hut.x + hut.w - 22, hut.y + 3, 18, 3.4f);
    b.antenna(hut.x + hut.w - 6, hut.y + 10, 14, K_RED);
    b.sandbags(W * 0.5f - 18, hut.y + hut.h + 17, W * 0.5f - 7, hut.y + hut.h + 17, 2); b.sandbags(W * 0.5f + 7, hut.y + hut.h + 17, W * 0.5f + 18, hut.y + hut.h + 17, 2);
    for (int i = 0; i < 3; i++) b.crate(6 + i * 9.0f, 52, 7, 7, shade(rgb(158, 124, 74), 0.88f + i * 0.08f));
    b.barrel(W - 10.0f, 56, 3, K_RUST); b.barrel(W - 16.0f, 57, 3, K_OLIVE_D);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 59);
}

void factoryClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.18f, 3);
    b.oilStain(70, 80, 22, 5); b.rust(4, 60, W - 4.0f, H - 4.0f, 12, 45);
    Roof shed = b.block(8, 8, W - 16.0f, 48, 20, K_ROOF, K_OLIVE_D);
    // sawtooth roof: alternating bands, each lit on its north slope
    for (int i = 0; i < 6; i++) { float x = shed.x + 3 + i * (shed.w - 6) / 6, w = (shed.w - 6) / 6; b.a.rect(x, shed.y + 3, w * 0.6f, shed.h - 6, 0.5f, i % 2 ? shade(K_ROOF, 0.82f) : shade(K_RUST, 1.0f), 1.1f, 0.5f); b.a.rect(x + w * 0.6f, shed.y + 3, w * 0.4f, shed.h - 6, 0.5f, shade(K_ROOF, 0.62f), 0.8f, 0.3f); }
    b.rust(shed.x, shed.y, shed.x + shed.w, shed.y + shed.h, 8, 75);
    b.wallSeams(shed, 20, 12);
    b.door(64, shed.y + shed.h + 20, 38, 18, K_DARK, rgb(24, 22, 18));
    for (int i = 0; i < 4; i++) b.a.line(45.5f, shed.y + shed.h + 20 - 4 * (i + 0.5f), 82.5f, shed.y + shed.h + 20 - 4 * (i + 0.5f), 0.55f, rgba(K_STEEL, 150));
    b.hazard(44, shed.y + shed.h + 20 + 1.4f, 40, 3.2f);
    b.windows(shed, 20, 3, rgb(255, 214, 120), 0.4f);
    b.mark(shed.x + shed.w - 26, shed.y + 3, 22, 3.6f);
    for (int i = 0; i < 2; i++) { P tp = b.cyl(22 + i * 14.0f, 18, 5.2f, 26 - i * 4, rgb(52, 48, 44), K_BRICK); b.a.ring(tp.x, tp.y, 3.6f, 0.8f, K_DARK, 0.5f, 0.2f); }
    // gantry crane across the forecourt, scrap hulls and a tyre stack
    b.pipe(10, 66, 10, 90, 1.5f, K_STEEL); b.pipe(10, 69, 40, 69, 1.5f, K_STEEL); b.a.rect(24, 67, 6, 5, 0.6f, K_RUST, 1, 0.5f);
    b.a.rect(14, 76, 26, 11, 3, K_OLIVE_D, 1.6f, 0.5f); b.a.rect(16, 78, 22, 7, 2, K_DARK, 1.2f, 0.4f);
    for (int i = 0; i < 3; i++) b.barrel(W - 14.0f, 68 + i * 7.0f, 3.2f, i == 1 ? K_RUST : shade(K_DARK, 1.6f));
    b.sandbags(W - 40.0f, H - 5.0f, W - 6.0f, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 61);
}

void airfieldClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.a.shadowRect(0, 0, (float)W, (float)H, 3, 3, 4, 85);
    b.a.rect(0, 0, (float)W, (float)H, 3, rgb(96, 94, 86), 2.6f, 0.5f);
    for (int i = 0; i < 40; i++) { float x = b.rng.f(4, W - 4.0f), y = b.rng.f(4, H - 4.0f); b.a.ell(x, y, b.rng.f(3, 9), b.rng.f(1.5f, 4), rgba(rgb(70, 66, 58), 60), 1, 0); }   // tyre scuffs
    for (int x = 32; x < W; x += 32) b.a.line((float)x, 2, (float)x, H - 2.0f, 0.7f, rgba(K_SEAM, 100));
    b.a.grain(0, 0, (float)W, (float)H, 0.14f, 67);
    // runway centre dashes and the four parking circles
    for (int i = 0; i < 13; i++) b.a.rect(6 + i * 12.0f, H * 0.5f - 0.8f, 7, 1.6f, 0.3f, rgba(WHITE, 130), 0.5f, 0);
    for (int i = 0; i < 4; i++) {
        float cx = W * 0.5f + (i - 1.5f) * 30, cy = H * 0.5f;
        b.a.ring(cx, cy, 11.5f, 1.4f, rgba(K_YEL, 225), 0.8f, 0); b.a.ring(cx, cy, 11.5f, 0.5f, rgba(K_DARK, 60), 0.5f, 0);
        b.a.line(cx - 3.5f, cy - 4, cx - 3.5f, cy + 4, 1.4f, rgba(K_YEL, 220)); b.a.line(cx + 3.5f, cy - 4, cx + 3.5f, cy + 4, 1.4f, rgba(K_YEL, 220)); b.a.line(cx - 3.5f, cy, cx + 3.5f, cy, 1.4f, rgba(K_YEL, 220));
    }
    for (int i = 0; i < 9; i++) { float x = 8 + i * (W - 16.0f) / 8; b.a.rect(x - 0.7f, 3, 1.4f, 3, 0.3f, K_YEL, 0.5f, 0.2f); b.a.rect(x - 0.7f, H - 6.0f, 1.4f, 3, 0.3f, K_YEL, 0.5f, 0.2f); }
    // control tower: a tall steel cab on a concrete leg
    Roof tw = b.block(8, 6, 16, 14, 26, K_KHAKI, K_OLIVE);
    b.wallSeams(tw, 26, 5); b.windows(tw, 26, 2, rgb(255, 214, 120), 0.6f);
    b.a.rect(tw.x + 2, tw.y + 2, tw.w - 4, tw.h - 4, 1.5f, rgb(74, 98, 110), 2, 0.6f); b.a.line(tw.x + 3, tw.y + 3.4f, tw.x + tw.w - 4, tw.y + 3.4f, 0.8f, rgba(WHITE, 130));
    b.antenna(tw.x + tw.w - 2, tw.y + 2, 12, K_RED);
    b.mark(tw.x + 2, tw.y + tw.h - 4, tw.w - 4, 2.6f);
    // Quonset hangar
    Roof hg = b.block(108, 6, 46, 26, 14, K_ROOF, K_OLIVE_D, 5);
    for (int i = 0; i < 7; i++) b.a.line(hg.x + 4 + i * 6.2f, hg.y + 1.5f, hg.x + 4 + i * 6.2f, hg.y + hg.h - 1.5f, 0.7f, rgba(K_DARK, 120));
    b.a.line(hg.x + 2, hg.y + hg.h * 0.5f, hg.x + hg.w - 2, hg.y + hg.h * 0.5f, 1.2f, rgba(WHITE, 50));
    b.door(hg.x + hg.w * 0.5f, hg.y + hg.h + 14, 28, 12, K_DARK, rgb(22, 20, 16)); b.hazard(hg.x + 8, hg.y + hg.h + 14 + 1.2f, 30, 3);
    b.mark(hg.x + hg.w - 14, hg.y + 2, 10, 3);
    // fuel dump and a windsock
    b.cyl(14, 84, 7, 8, K_STEEL, K_OLIVE); b.cyl(30, 84, 7, 8, K_STEEL, K_RUST); b.pipe(14, 78, 30, 78, 1.2f, K_STEEL);
    for (int i = 0; i < 3; i++) b.barrel(46 + i * 5.5f, 82, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.a.line(W - 12.0f, 86, W - 12.0f, 74, 0.9f, K_STEEL);
    b.a.poly({ {W - 12.0f, 74}, {W - 3.0f, 76}, {W - 3.0f, 79}, {W - 12.0f, 78} }, rgb(224, 110, 46), 1.4f, 0.5f);
    b.a.rect(W - 8.0f, 75.4f, 1.8f, 3.4f, 0.2f, WHITE, 0.6f, 0.2f);
    b.sandbags(60, H - 5.0f, 100, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.08f, 71);
}

void techClanker(Bld& b) {
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 3);
    Roof lab = b.block(8, 30, 54, 44, 14, K_ROOF, K_OLIVE_D);
    b.corrugated(lab, 3.4f, K_DARK); b.rust(lab.x, lab.y, lab.x + lab.w, lab.y + lab.h, 6, 70);
    b.wallSeams(lab, 14, 11); b.windows(lab, 14, 4, rgb(255, 214, 120), 0.5f);
    b.door(lab.x + 14, lab.y + lab.h + 14, 12, 11, K_DARK, rgb(22, 20, 16)); b.hazard(lab.x + 8, lab.y + lab.h + 14 + 1.2f, 12, 3);
    b.mark(lab.x + 3, lab.y + lab.h - 5.4f, 20, 3.4f);
    b.vent(lab.x + lab.w - 16, lab.y + 4, 12, 7, rgba(WHITE, 80));
    // observatory radome on a drum
    P dr = b.cyl(56, 48, 15, 10, K_ROOF, K_STEEL); (void)dr;
    b.domeTop(56, 38, 12, rgb(214, 216, 214));
    b.a.rect(53.5f, 37, 5, 2.4f, 0.3f, rgba(K_DARK, 90), 0.5f, 0);
    // ordnance test barrel poking out to the east, with an ammunition rack
    b.pipe(68, 62, 92, 56, 2.6f, shade(K_DARK, 1.7f)); b.pipe(88, 57, 93, 56, 3.4f, shade(K_DARK, 2.0f)); b.a.rect(62, 58, 8, 10, 1, K_STEEL, 1.4f, 0.5f);
    for (int i = 0; i < 4; i++) b.a.cap(70, 72 + i * 3.4f, 86, 72 + i * 3.4f, 1.3f, i % 2 ? K_BRICK : K_YEL, 0.8f, 0.5f);
    // mast array
    b.antenna(14, 28, 26, K_RED); b.antenna(22, 22, 20, K_RED); b.antenna(30, 26, 16, K_YEL);
    b.pipe(8, 78, 40, 78, 1.3f, K_RUST);
    for (int i = 0; i < 3; i++) b.barrel(46 + i * 5.0f, 82, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.sandbags(8, H - 5.0f, 38, H - 5.0f);
    b.a.grain(0, 0, (float)W, (float)H, 0.10f, 73);
}

void gunBase(Bld& b) {   // 1x1 sandbag ring round a pit, the MG head turns over it
    b.a.shadowCircle(16, 16, 14.5f, 2, 2, 3, 85);
    b.a.circle(16, 16, 14.8f, K_PAD2, 2, 0.5f);
    b.a.circle(16, 16, 11, rgb(54, 46, 36), 2.2f, 0.5f);
    for (int i = 0; i < 16; i++) { float an = i * 0.3927f; float px = 16 + std::cos(an) * 12.6f, py = 16 + std::sin(an) * 12.6f; b.a.ell(px, py, 3.4f, 2.4f, shade(K_SAND, 0.86f + b.rng.f(0, 0.16f)), 1.4f, 0.6f); }
    for (int i = 0; i < 3; i++) b.a.line(7 + i * 4.0f, 18, 7 + i * 4.0f, 25, 0.9f, rgba(rgb(120, 92, 56), 160));   // duckboards
    b.markDisc(16, 27, 1.5f);
}

void rocketBase(Bld& b) {   // 2x2: concrete slab, ammunition stacks and sandbags; the rocket pod turns over the middle
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.16f, 4);
    b.a.rect(8, 10, W - 16.0f, H - 20.0f, 3, K_PAD2, 1.8f, 0.45f);
    b.hazard(10, H - 11.0f, W - 20.0f, 2.6f);
    for (int i = 0; i < 3; i++) b.crate(6 + i * 8.0f, 4, 7, 6, shade(K_OLIVE, 0.9f + i * 0.08f));
    for (int i = 0; i < 4; i++) b.a.cap(W - 22.0f, 5 + i * 2.6f, W - 6.0f, 5 + i * 2.6f, 1.1f, i % 2 ? K_BRICK : K_YEL, 0.8f, 0.5f);
    b.sandbags(4, H - 4.0f, W - 4.0f, H - 4.0f, 2); b.sandbags(3, 12, 3, H - 8.0f); b.sandbags(W - 3.0f, 12, W - 3.0f, H - 8.0f);
    b.a.ring(W - 12.0f, H - 15.0f, 3.4f, 1.2f, K_STEEL, 0.8f, 0.3f);   // cable reel
    b.markDisc(8, 13, 1.7f);
}

void oilwell(Bld& b) {   // 2x2: oil-stained pad, pit, derrick, storage tank; the pump-jack beam is drawn live
    int W = b.W, H = b.H;
    b.pad(rgb(84, 78, 66), K_SEAM, 0.18f, 4);
    b.oilStain(32, 40, 22, 14); b.oilStain(14, 18, 9, 5); b.oilStain(52, 52, 8, 4);
    b.a.ell(32, 44, 13, 9, rgb(14, 12, 12), 2.4f, 0.5f);
    b.a.ell(30, 42, 8, 4.4f, rgba(rgb(70, 62, 90), 140), 1, 0);                         // oil sheen
    b.a.ell(28, 41, 3.4f, 1.4f, rgba(WHITE, 90), 1, 0);
    // derrick: a lattice A-frame with a crown block
    for (int s = -1; s <= 1; s += 2) b.a.line(32 + s * 11.0f, 50, 32 + s * 2.0f, 8, 1.6f, K_STEEL);
    for (int i = 0; i < 6; i++) { float y = 46 - i * 6.2f, hw = 10 - i * 1.5f; b.a.line(32 - hw, y, 32 + hw, y, 0.8f, shade(K_STEEL, 0.85f)); b.a.line(32 - hw, y, 32 + hw - 1.6f, y - 6.2f, 0.6f, shade(K_STEEL, 0.7f)); }
    b.a.rect(29, 5, 6, 5, 0.8f, K_RUST, 1.4f, 0.5f); b.light(32, 4, 1.2f, K_RED, 130);
    b.a.line(33, 10, 33, 30, 0.6f, rgba(BLACK, 120));
    b.a.line(32, 50, 45, 56, 0.9f, rgba(BLACK, 50));
    P tk = b.cyl(52, 24, 7, 10, K_STEEL, K_RUST); b.a.dot(tk.x, tk.y, 1.8f, K_DARK); b.pipe(46, 44, 52, 28, 1.3f, K_STEEL);
    for (int i = 0; i < 3; i++) b.barrel(8 + i * 5.0f, 54, 2.6f, i == 1 ? K_RUST : K_OLIVE_D);
    b.hazard(4, H - 7.0f, 28, 2.8f);
    b.markDisc(W - 8.0f, H - 8.0f, 2.2f);
}

void nukeClanker(Bld& b) {   // launch ramp with a missile on its rail, exhaust pit, fuel tanks and bunkers
    int W = b.W, H = b.H;
    b.pad(K_PAD, K_SEAM, 0.18f, 3);
    b.rust(4, 4, W - 4.0f, H - 4.0f, 14, 50);
    b.hazard(4, 4, W - 8.0f, 3.2f); b.hazard(4, H - 7.2f, W - 8.0f, 3.2f);
    b.a.ell(24, 74, 15, 9, rgb(20, 18, 16), 2.4f, 0.5f); b.a.glow(24, 74, 16, rgba(rgb(255, 120, 50), 60));   // exhaust pit
    // launch rails (a long diagonal truss) and the warhead riding them
    float x0 = 22, y0 = 76, x1 = 70, y1 = 22;
    b.a.line(x0 + 4.4f, y0 + 4.4f, x1 + 4.4f, y1 + 4.4f, 5, rgba(BLACK, 70));
    b.pipe(x0 - 4, y0 + 2, x1 - 4, y1 + 2, 1.6f, K_STEEL); b.pipe(x0 + 4, y0 - 2, x1 + 4, y1 - 2, 1.6f, K_STEEL);
    for (int i = 0; i < 9; i++) { float k = i / 8.0f, px = x0 + (x1 - x0) * k, py = y0 + (y1 - y0) * k; b.a.line(px - 5, py + 2.4f, px + 5, py - 2.4f, 1.0f, shade(K_RUST, 0.9f)); }
    // fuel tanks, a blockhouse and sandbag revetments
    b.cyl(74, 74, 7, 9, K_STEEL, K_OLIVE); b.cyl(58, 80, 6, 8, K_STEEL, K_RUST); b.pipe(74, 67, 64, 30, 1.0f, K_STEEL);
    Roof bk = b.block(6, 10, 22, 18, 10, K_ROOF, K_OLIVE_D);
    b.corrugated(bk, 3.2f, K_DARK); b.windows(bk, 10, 2, rgb(255, 214, 120), 0.5f);
    b.mark(bk.x + 3, bk.y + bk.h - 5, 16, 3);
    b.sandbags(4, H - 10.0f, 30, H - 10.0f); b.sandbags(66, 8, W - 4.0f, 8);
    b.antenna(bk.x + bk.w - 4, bk.y + 4, 16, K_RED);
    b.a.grain(0, 0, (float)W, (float)H, 0.12f, 79);
}

}  // namespace

// ================================================================= entry points
BArtSize artBuildingSize(int type) {
    const BuildType& bt = BUILDS[type];
    int dw = bt.w * TILE + BART_PAD_L + BART_PAD_R, dh = bt.h * TILE + BART_PAD_T + BART_PAD_B;
    return BArtSize{ dw * BART_K, dh * BART_K, (BART_PAD_L + bt.w * TILE * 0.5f) * BART_K, (BART_PAD_T + bt.h * TILE * 0.5f) * BART_K };
}

void artBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    bool cy = bt.faction == F_CYBER;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, cy, 1000u + (u32)type * 31u);
    switch (bt.role) {
    case BR_HQ:       cy ? hqCyber(b)       : hqClanker(b);       break;
    case BR_POWER:    cy ? powerCyber(b)    : powerClanker(b);    break;
    case BR_SUPPLY:   cy ? supplyCyber(b)   : supplyClanker(b);   break;
    case BR_BARRACKS: cy ? barracksCyber(b) : barracksClanker(b); break;
    case BR_FACTORY:  cy ? factoryCyber(b)  : factoryClanker(b);  break;
    case BR_AIRFIELD: cy ? airfieldCyber(b) : airfieldClanker(b); break;
    case BR_TECH:     cy ? techCyber(b)     : techClanker(b);     break;
    case BR_TURRET:   cy ? laserBase(b)     : gunBase(b);         break;
    case BR_AATURRET: cy ? patriotCyber(b)  : rocketBase(b);      break;
    case BR_INCOME:   cy ? minerCyber(b)    : oilwell(b);         break;
    case BR_NUKE:     cy ? nukeCyber(b)     : nukeClanker(b);     break;
    }
    c.outline(rgb(8, 10, 14, 150));
}

void artSite(Canvas& c, int type) {
    const BuildType& bt = BUILDS[type];
    Canvas dummy(4, 4);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 2000u + (u32)type * 17u);
    int W = b.W, H = b.H;
    b.pad(rgb(96, 84, 64), rgb(60, 52, 40), 0.22f, 3);
    for (int i = 0; i < 24; i++) b.a.ell(b.rng.f(4, W - 4.0f), b.rng.f(4, H - 4.0f), b.rng.f(3, 10), b.rng.f(2, 5), rgba(rgb(70, 58, 42), 80), 1, 0);   // churned earth
    // concrete footing outline and re-bar grid
    b.a.rect(6, 6, W - 12.0f, H - 12.0f, 2, rgb(128, 124, 114), 1.6f, 0.5f);
    for (int x = 14; x < W - 8; x += 8) b.a.line((float)x, 8, (float)x, H - 8.0f, 0.6f, rgba(rgb(70, 66, 60), 150));
    for (int y = 14; y < H - 8; y += 8) b.a.line(8, (float)y, W - 8.0f, (float)y, 0.6f, rgba(rgb(70, 66, 60), 150));
    // stacked materials
    for (int i = 0; i < std::max(2, bt.w * bt.h / 3); i++) {
        float x = b.rng.f(10, W - 24.0f), y = b.rng.f(10, H - 18.0f);
        if (i & 1) { for (int k = 0; k < 3; k++) b.a.rect(x, y + k * 2.6f, 14, 2.2f, 0.4f, shade(rgb(176, 138, 84), 0.9f + 0.1f * k), 0.8f, 0.5f); }
        else b.crate(x, y, 8, 7, rgb(120, 112, 90));
    }
    // fence posts with hazard tape and a scaffold frame in the middle
    for (int x = 6; x <= W - 6; x += 12) { b.a.dot((float)x, 5, 1.2f, rgb(60, 60, 64)); b.a.dot((float)x, H - 5.0f, 1.2f, rgb(60, 60, 64)); }
    b.a.line(6, 5, W - 6.0f, 5, 0.8f, rgb(232, 188, 62)); b.a.line(6, H - 5.0f, W - 6.0f, H - 5.0f, 0.8f, rgb(232, 188, 62));
    for (int y = 6; y <= H - 6; y += 12) { b.a.dot(5, (float)y, 1.2f, rgb(60, 60, 64)); b.a.dot(W - 5.0f, (float)y, 1.2f, rgb(60, 60, 64)); }
    for (int i = 0; i <= 4; i++) { float x = W * (0.2f + 0.15f * i); b.pipe(x, H * 0.3f, x, H * 0.7f, 0.9f, rgb(150, 152, 156)); }
    b.pipe(W * 0.2f, H * 0.45f, W * 0.8f, H * 0.45f, 0.8f, rgb(150, 152, 156)); b.pipe(W * 0.2f, H * 0.62f, W * 0.8f, H * 0.62f, 0.8f, rgb(150, 152, 156));
    c.outline(rgb(8, 10, 14, 140));
}

void artRubble(Canvas& c, int tw, int th, u32 seed) {
    Canvas dummy(4, 4);
    Bld b(c, dummy, tw * TILE, th * TILE, true, seed);
    int W = b.W, H = b.H;
    b.a.soft(0, 0, c.w, c.h, [&](float x, float y) {
        float cx = (x / BART_K - BART_PAD_L) - W * 0.5f, cy = (y / BART_K - BART_PAD_T) - H * 0.5f;
        float d = (hyp(cx / (W * 0.52f), cy / (H * 0.52f)) - 1) * std::min(W, H) * 0.5f;
        return d;
    }, rgba(rgb(14, 12, 10), 200), 14.0f * BART_K);
    for (int i = 0; i < W * H / 90; i++) {
        float x = b.rng.f(6, W - 6.0f), y = b.rng.f(6, H - 6.0f), s = b.rng.f(2, 7);
        Color base = b.rng.f() < 0.35f ? rgb(60, 56, 52) : (b.rng.f() < 0.5f ? rgb(92, 88, 82) : rgb(46, 44, 42));
        b.a.poly({ {x, y}, {x + s, y + b.rng.f(-1, 1)}, {x + s * b.rng.f(0.5f, 1.0f), y + s * 0.8f}, {x - s * 0.2f, y + s * 0.6f} }, base, 1.4f, 0.6f);
    }
    for (int i = 0; i < 6 + W / 16; i++) { float x = b.rng.f(8, W - 8.0f), y = b.rng.f(8, H - 8.0f), an = b.rng.f(0, 6.28f), l = b.rng.f(4, 12); b.a.cap(x, y, x + std::cos(an) * l, y + std::sin(an) * l, 0.9f, rgb(70, 52, 40), 0.8f, 0.5f); }   // bent beams
    b.a.grain(0, 0, (float)W, (float)H, 0.18f, seed + 5);
}

// The warhead itself, drawn live over a nuke ramp only while the ramp is armed (same canvas geometry as the structure).
void artArmed(Canvas& c, int type) {
    const BuildType& bt = BUILDS[type];
    Canvas dummy(4, 4);
    Bld b(c, dummy, bt.w * TILE, bt.h * TILE, bt.faction == F_CYBER, 4000u + (u32)type);
    if (bt.faction == F_CYBER) {
        float cx = 48, cy = 50;
        b.a.glow(cx, cy, 24, rgba(rgb(255, 120, 60), 90));
        b.a.cap(cx, cy + 14, cx, cy - 16, 6, C_WHITE, 5, 0.65f);                 // missile body
        b.a.circle(cx, cy - 16, 6, rgb(214, 70, 60), 3, 0.65f);                  // nose cone
        b.a.rect(cx - 6, cy - 3, 12, 4, 0.5f, rgb(240, 196, 60), 0.9f, 0.4f);    // warhead band
        for (int s = -1; s <= 1; s += 2) b.a.poly({ {cx + s * 5.0f, cy + 12}, {cx + s * 11.0f, cy + 17}, {cx + s * 5.0f, cy + 6} }, C_ROOF2, 1, 0.4f);   // fins
    } else {
        float x0 = 22, y0 = 76, x1 = 70, y1 = 22;
        float mx0 = x0 + (x1 - x0) * 0.14f, my0 = y0 + (y1 - y0) * 0.14f, mx1 = x0 + (x1 - x0) * 0.86f, my1 = y0 + (y1 - y0) * 0.86f;
        b.a.cap(mx0, my0, mx1, my1, 4.6f, rgb(210, 212, 210), 4, 0.65f);
        b.a.rect(mx0 + (mx1 - mx0) * 0.45f - 1.6f, my0 + (my1 - my0) * 0.45f - 4, 3.2f, 8, 0.4f, rgb(236, 196, 60), 0.8f, 0.3f);
        b.a.circle(mx1, my1, 4.6f, rgb(216, 70, 56), 3, 0.65f);
        b.a.poly({ {mx0, my0}, {mx0 - 8, my0 - 1}, {mx0 - 2, my0 - 7} }, K_STEEL, 1, 0.4f); b.a.poly({ {mx0, my0}, {mx0 + 2, my0 + 8}, {mx0 + 7, my0 + 2} }, K_STEEL, 1, 0.4f);
    }
    c.outline(rgb(8, 10, 14, 150));
}

void artProp(Canvas& c, int which) {
    Canvas dummy(4, 4);
    Art a(c, WHITE, 2.0f, 0, 0);   // design space 32x32
    switch (which) {
    case 0: {   // cyber optical dish: a pale bowl on a swivel, emitter feed in the middle
        a.shadowCircle(16, 16, 11, 2, 2, 2, 70);
        a.circle(16, 16, 11, rgb(150, 170, 198), 2.8f, 0.6f);
        a.circle(16, 16, 8.4f, rgb(86, 108, 148), 2.4f, 0.4f);
        a.ring(16, 16, 6, 0.8f, rgba(C_CYAN, 200), 0.6f, 0);
        a.cap(16, 16, 27, 5, 0.8f, rgb(190, 204, 224), 0.8f, 0.5f);
        a.dot(27, 5, 1.5f, C_CYAN); a.glow(27, 5, 6, rgba(C_CYAN, 140));
        a.dot(16, 16, 2, C_WHITE);
        break; }
    case 1: {   // clanker radar: a lattice antenna bar turning on a drum
        a.shadowRect(2, 11, 28, 10, 2, 2, 2, 60);
        a.rect(2, 11, 28, 6, 0.8f, rgb(150, 150, 140), 1.6f, 0.55f);
        for (int i = 0; i < 7; i++) a.line(4.4f + i * 3.8f, 11.4f, 4.4f + i * 3.8f, 16.6f, 0.6f, rgba(K_DARK, 150));
        a.circle(16, 14, 3.4f, K_STEEL, 1.6f, 0.55f); a.dot(16, 14, 1.2f, K_DARK);
        a.dot(3, 14, 1, K_RED);
        break; }
    case 2: {   // pump-jack walking beam: the horsehead and the counterweight
        a.shadowRect(2, 13, 28, 6, 2, 2, 2, 60);
        a.cap(5, 16, 27, 16, 1.7f, K_RUST, 1.4f, 0.6f);
        a.rect(22, 11, 8, 10, 1.4f, K_STEEL, 1.6f, 0.55f);               // horsehead
        a.rect(2, 12, 7, 8, 1, K_STEEL, 1.4f, 0.5f);                     // counterweight
        a.circle(14, 16, 3, K_YEL, 1.6f, 0.55f); a.dot(14, 16, 1.2f, K_DARK);
        break; }
    case 3: {   // cooling fan (blades only, spun by the renderer)
        a.circle(16, 16, 12, rgba(rgb(20, 22, 26), 120), 1, 0);
        for (int i = 0; i < 4; i++) { float an = i * 1.5708f; a.cap(16, 16, 16 + std::cos(an) * 10, 16 + std::sin(an) * 10, 2.3f, rgb(150, 160, 176), 1.4f, 0.5f); }
        a.dot(16, 16, 2.2f, rgb(210, 220, 232));
        break; }
    }
    c.outline(rgb(8, 10, 14, 120));
}
