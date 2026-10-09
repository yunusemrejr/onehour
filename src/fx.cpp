// One Hour - visual effects: baked effect sprites, the cosmetic particle system and the drawing of simulation effects.
#include "ui.h"
#include "paint.h"
#include "artb.h"

using namespace paint;

namespace {
float hh(int x, int y, u32 s) { u32 h = (u32)x * 374761393u + (u32)y * 668265263u + s * 2246822519u; h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16; return (h & 0xFFFF) / 65535.0f; }
float vn(float x, float y, u32 s) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y); float fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(hh(ix, iy, s), hh(ix + 1, iy, s), fx), lerpf(hh(ix, iy + 1, s), hh(ix + 1, iy + 1, s), fx), fy);
}
float fb(float x, float y, u32 s) { return vn(x, y, s) * 0.55f + vn(x * 2.1f, y * 2.1f, s + 7) * 0.3f + vn(x * 4.3f, y * 4.3f, s + 13) * 0.15f; }
float sst(float a, float b, float x) { float t = clampf((x - a) / (b - a), 0, 1); return t * t * (3 - 2 * t); }
Color lerpC(Color a, Color b, float t) { return Color{(u8)lerpf(a.r, b.r, t), (u8)lerpf(a.g, b.g, t), (u8)lerpf(a.b, b.b, t), 255}; }

void puffCanvas(Canvas& c, int v, bool dust) {
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = (x + 0.5f - 32) / 30.0f, dy = (y + 0.5f - 32) / 30.0f, d = std::sqrt(dx * dx + dy * dy);
        if (d >= 1) continue;
        float n = fb(x * 0.085f + v * 17, y * 0.085f + v * 5, 91 + v);
        float a = clampf(std::pow(1 - d, 1.25f) * (0.30f + 1.25f * n), 0, 1) * 0.9f;
        float lum = clampf(150 + 85 * (-dx * 0.55f - dy * 0.65f) + 46 * (n - 0.5f), 40, 250);
        Color col = dust ? Color{(u8)lum, (u8)(lum * 0.88f), (u8)(lum * 0.68f), 255} : Color{(u8)lum, (u8)lum, (u8)(lum * 0.98f), 255};
        col.a = (u8)(a * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
void fireCanvas(Canvas& c, int f) {
    float R = 14 + f * 2.6f;
    float cool = clampf((f - 3) / 4.0f, 0, 1);
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = x + 0.5f - 32, dy = y + 0.5f - 32, d = std::sqrt(dx * dx + dy * dy) / R;
        float n = fb(x * 0.12f + f * 2.3f, y * 0.12f - f * 1.1f, 311);
        float t = d + (n - 0.5f) * 0.62f;
        if (t >= 1) continue;
        t = std::max(t, 0.0f);
        Color col;
        if (t < 0.22f) col = lerpC(rgb(255, 252, 214), rgb(255, 238, 160), t / 0.22f);
        else if (t < 0.5f) col = lerpC(rgb(255, 238, 160), rgb(255, 160, 44), (t - 0.22f) / 0.28f);
        else if (t < 0.8f) col = lerpC(rgb(255, 160, 44), rgb(196, 58, 20), (t - 0.5f) / 0.3f);
        else col = lerpC(rgb(196, 58, 20), rgb(66, 34, 24), (t - 0.8f) / 0.2f);
        col = lerpC(col, rgb(62, 56, 52), cool * sst(0.05f, 0.8f, t));
        float a = clampf(1 - std::pow(t, 2.6f), 0, 1) * (1.0f - 0.18f * cool);
        col.a = (u8)(a * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
void flameCanvas(Canvas& c, int f) {
    for (int y = 0; y < c.h; y++) {
        float v = (44.0f - y) / 44.0f;
        if (v < -0.04f) continue;
        v = std::max(v, 0.0f);
        float width = 9.0f * std::pow(1 - v, 0.65f) * (0.8f + 0.4f * std::sin(f * 1.1f + v * 5));
        float sway = std::sin(f * 1.05f + v * 3.5f) * 3.6f * v;
        for (int x = 0; x < c.w; x++) {
            float u = (x + 0.5f - 16 - sway) / std::max(width, 0.3f);
            if (std::abs(u) >= 1) continue;
            float inner = 1 - std::abs(u);
            float t = v * 0.8f + std::abs(u) * 0.5f;
            Color col = t < 0.4f ? lerpC(rgb(255, 246, 196), rgb(255, 170, 50), t / 0.4f) : lerpC(rgb(255, 170, 50), rgb(210, 50, 20), clampf((t - 0.4f) / 0.5f, 0, 1));
            float a = std::pow(inner, 0.7f) * (1 - std::pow(v, 1.5f));
            col.a = (u8)(clampf(a, 0, 1) * 255);
            c.px[y * c.w + x] = Canvas::pack(col);
        }
    }
}
void flashCanvas(Canvas& c, int v) {
    int spikes = 4 + v;
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = x + 0.5f - 24, dy = y + 0.5f - 24, r = std::sqrt(dx * dx + dy * dy), ang = std::atan2(dy, dx);
        float spike = std::pow(std::abs(std::cos(ang * spikes * 0.5f + v * 0.7f)), 14.0f);
        float a = std::exp(-r / (2.5f + 17.0f * spike)) + std::exp(-r / 4.5f) * 0.9f;
        Color col = lerpC(rgb(255, 214, 120), rgb(255, 252, 232), clampf(std::exp(-r / 6.0f), 0, 1));
        col.a = (u8)(clampf(a, 0, 1) * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
}  // namespace

void Gfx::buildEffects() {
    { Canvas c(32, 32); c.glow(16, 16, 16, rgb(255, 255, 255, 255)); for (auto& px : c.px) { Color k = Canvas::unpack(px); k.r = k.g = k.b = 255; px = Canvas::pack(k); } blobAdd = fromCanvasAdd(c, 16, 16); }
    for (int v = 0; v < 4; v++) { Canvas c(64, 64); puffCanvas(c, v, false); fxs.smoke[v] = fromCanvasSmooth(c, 32, 32); }
    for (int v = 0; v < 3; v++) { Canvas c(64, 64); puffCanvas(c, v + 4, true); fxs.dust[v] = fromCanvasSmooth(c, 32, 32); }
    for (int v = 0; v < 4; v++) {   // dense, lit billows for the mushroom cloud: a sphere lit from the top-left with a turbulent edge and a puckered surface
        Canvas c(96, 96);
        for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) {
            float dx = (x + 0.5f - 48) / 46.0f, dy = (y + 0.5f - 48) / 46.0f, d = std::sqrt(dx * dx + dy * dy);
            float n = fb(x * 0.07f + v * 5.0f, y * 0.07f + v * 2.0f, 900 + v), n2 = fb(x * 0.17f + v * 3.0f, y * 0.17f, 930 + v);
            float e = d + (n - 0.5f) * 0.5f;
            float a = clampf((0.98f - e) / 0.3f, 0, 1); a = a * a * (3 - 2 * a);
            if (a <= 0.01f) continue;
            float nz = std::sqrt(std::max(0.0f, 1 - d * d)), nx = dx + (n2 - 0.5f) * 0.5f, ny = dy + (n2 - 0.5f) * 0.4f;
            float light = nx * -0.5f + ny * -0.6f + nz * 0.62f;
            float lum = clampf(0.52f + 0.62f * light, 0.22f, 1.12f) * (0.84f + 0.32f * n2);
            u8 L = (u8)clampf(lum * 255, 0, 255);
            c.px[y * 96 + x] = Canvas::pack(Color{L, L, L, (u8)(a * 240)});
        }
        fxs.billow[v] = fromCanvasSmooth(c, 48, 48);
    }
    for (int f = 0; f < 8; f++) { Canvas c(64, 64); fireCanvas(c, f); fxs.fire[f] = fromCanvasSmooth(c, 32, 32); }
    for (int f = 0; f < 6; f++) { Canvas c(32, 48); flameCanvas(c, f); fxs.flame[f] = fromCanvasAdd(c, 16, 44); }
    for (int v = 0; v < 3; v++) { Canvas c(48, 48); flashCanvas(c, v); fxs.flash[v] = fromCanvasAdd(c, 24, 24); }
    { Canvas c(16, 4); for (int y = 0; y < 4; y++) for (int x = 0; x < 16; x++) { float a = std::pow(x / 15.0f, 2.0f) * (1 - std::abs((y + 0.5f - 2) / 2.0f) * 0.7f); c.px[y * 16 + x] = Canvas::pack(Color{255, (u8)(200 + x * 3), (u8)(120 + x * 8), (u8)(clampf(a, 0, 1) * 255)}); } fxs.streak = fromCanvasAdd(c, 15, 2); }
    { Canvas c(128, 128); for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) { float dx = x + 0.5f - 64, dy = y + 0.5f - 64, d = std::sqrt(dx * dx + dy * dy) / 64.0f; float a = std::exp(-std::pow((d - 0.84f) / 0.08f, 2.0f)) * 0.95f + std::exp(-std::pow((d - 0.84f) / 0.32f, 2.0f)) * 0.14f; c.px[y * 128 + x] = Canvas::pack(Color{255, 255, 255, (u8)(clampf(a, 0, 1) * 255)}); } fxs.ring = fromCanvasAdd(c, 64, 64); }
    { Canvas c(64, 16); for (int y = 0; y < 16; y++) for (int x = 0; x < 64; x++) { float ax = sst(0, 0.09f, x / 64.0f) * sst(0, 0.09f, 1 - x / 64.0f), v = std::abs((y + 0.5f - 8) / 8.0f), ay = std::pow(1 - v, 1.7f); c.px[y * 64 + x] = Canvas::pack(Color{255, 255, 255, (u8)(clampf(ax * ay, 0, 1) * 255)}); } fxs.beam = fromCanvasAdd(c, 32, 8); }
    for (int v = 0; v < 4; v++) {
        Canvas c(16, 16); Art a(c, rgb(255, 255, 255), 2.0f); Rng r(40u + v);
        std::vector<P> pts; int n = 5 + v % 2;
        for (int i = 0; i < n; i++) { float an = i * 6.2831853f / n + r.f(-0.3f, 0.3f), rr = r.f(2.6f, 4.2f); pts.push_back({4.0f + std::cos(an) * rr, 4.0f + std::sin(an) * rr}); }
        a.poly(pts, rgb(150, 150, 150), 2.0f, 0.7f);
        fxs.debris[v] = fromCanvasSmooth(c, 8, 8);
    }
    for (int v = 0; v < 3; v++) {
        Canvas c(96, 96);
        for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) {
            float dx = (x + 0.5f - 48) / 46.0f, dy = (y + 0.5f - 48) / 46.0f, d = std::sqrt(dx * dx + dy * dy);
            float n = fb(x * 0.08f + v * 7, y * 0.08f, 505 + v);
            float a = std::pow(clampf(1.25f - d * 1.15f + (n - 0.5f) * 0.9f, 0, 1), 1.4f);
            if (a <= 0) continue;
            Color col = d > 0.62f ? Color{50, 38, 28, 0} : Color{14, 12, 10, 0};
            col.a = (u8)(a * (d > 0.62f ? 120 : 200));
            c.px[y * 96 + x] = Canvas::pack(col);
        }
        fxs.scorch[v] = fromCanvasSmooth(c, 48, 48);
    }
    for (int v = 0; v < 3; v++) {   // cloud shadows: big soft irregular blobs
        Canvas c(128, 128);
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            float dx = (x + 0.5f - 64) / 62.0f, dy = (y + 0.5f - 64) / 62.0f, d = std::sqrt(dx * dx + dy * dy);
            float n = fb(x * 0.05f + v * 9, y * 0.05f + v * 3, 700 + v);
            float a = std::pow(clampf((1 - d) * 1.5f + (n - 0.5f) * 1.5f, 0, 1), 1.4f);
            c.px[y * 128 + x] = Canvas::pack(Color{0, 0, 0, (u8)(a * 255)});
        }
        fxs.cloud[v] = fromCanvasSmooth(c, 64, 64);
    }
    for (int army = 0; army < 2; army++) {   // tactical warhead on its way down (faces +x, origin at the middle of the body): two liveries, ogive nose, hazard bands, swept fins, nozzle
        bool cy = army == 0;
        Canvas c(136, 44); Art a(c, rgb(255, 255, 255), 2.0f);
        Color body = cy ? rgb(228, 234, 240) : rgb(122, 128, 88), band = cy ? rgb(96, 214, 238) : rgb(238, 196, 62), nose = cy ? rgb(54, 62, 78) : rgb(206, 64, 52), fin = cy ? rgb(92, 102, 120) : rgb(70, 74, 62);
        for (int s = -1; s <= 1; s += 2) {
            float Y = 11 + s * 0.0f;
            a.poly({ {13, Y}, {5.5f, Y + s * 10.0f}, {2.5f, Y + s * 10.0f}, {7.0f, Y} }, fin, 1.6f, 0.55f);            // swept main fins
            a.poly({ {40, Y + s * 3.2f}, {35, Y + s * 6.8f}, {33, Y + s * 6.8f}, {35.5f, Y + s * 3.2f} }, fin, 1.2f, 0.5f);   // canards
        }
        a.rect(2.4f, 7.6f, 5.2f, 6.8f, 0.8f, rgb(46, 48, 52), 1.8f, 0.6f);                                             // nozzle bell
        a.dot(3.2f, 11, 2.3f, rgb(255, 170, 70)); a.glow(3.2f, 11, 6, rgba(rgb(255, 170, 70), 160));
        a.cap(8, 11, 44, 11, 4.4f, body, 3.4f, 0.7f);                                                                  // body
        a.poly({ {44, 6.7f}, {50, 8.0f}, {56, 10.0f}, {59, 11}, {56, 12.0f}, {50, 14.0f}, {44, 15.3f} }, nose, 3.0f, 0.7f);   // ogive nose
        if (cy) a.dot(57.4f, 11, 1.1f, rgb(150, 245, 255));
        a.rect(18, 6.4f, 4.4f, 9.2f, 0.3f, band, 0.9f, 0.45f);                                                         // warhead band
        for (int i = 0; i < 4; i++) a.poly({ {18.4f + i * 1.1f, 6.6f}, {19.4f + i * 1.1f, 6.6f}, {18.4f + i * 1.1f + 1.6f, 15.4f}, {17.4f + i * 1.1f + 1.6f, 15.4f} }, rgba(rgb(30, 30, 30), 170), 0.2f, 0.2f);   // hazard hatching
        a.rect(30, 6.7f, 1.6f, 8.6f, 0.2f, shade(body, 0.62f), 0.6f, 0.4f); a.rect(38, 6.9f, 1.2f, 8.2f, 0.2f, shade(body, 0.62f), 0.6f, 0.4f);   // panel seams
        a.circle(26, 11, 1.7f, rgb(240, 214, 70), 0.8f, 0.4f);                                                          // radiation roundel
        a.line(10, 7.9f, 43, 7.9f, 0.7f, rgba(rgb(255, 255, 255), 130));
        a.dot(53, 8.7f, 0.9f, rgba(rgb(255, 255, 255), 190));
        c.outline(rgb(8, 10, 14, 170));
        (cy ? fxs.missile : fxs.missileK) = fromCanvasSmooth(c, 62, 22); (cy ? fxs.missile : fxs.missileK).dscale = 0.5f;
    }
    // water caustics: sums of integer-frequency waves so every frame tiles seamlessly, four frames that loop
    for (int f = 0; f < 4; f++) {
        Canvas c(128, 128);
        static const int A[5] = { 1, 3, 2, 4, 5 }, B[5] = { 2, 1, -3, 3, -2 }, M[5] = { 1, -1, 1, -1, 1 };
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            float w = 0;
            for (int k = 0; k < 5; k++) w += std::sin(6.2831853f * (A[k] * x + B[k] * y) / 128.0f + 1.3f * k + f * 1.5707963f * M[k]);
            float r = clampf(1.0f - std::abs(w) * 0.5f, 0, 1);
            float a = std::pow(r, 7.0f);
            c.px[y * 128 + x] = Canvas::pack(Color{190, 232, 250, (u8)(clampf(a, 0, 1) * 110)});
        }
        waterFx[f] = fromCanvasAdd(c, 0, 0);
    }
}

// ================================================================= particles
void Game::emitP(float x, float y, float vx, float vy, float life, float s0, float s1, Color col, u8 kind, u8 var, float ay, float drag, float rot, float vrot) {
    if (parts.size() >= 900) return;
    parts.push_back(Particle{ x, y, vx, vy, ay, drag, 0, life, s0, s1, rot, vrot, col, kind, var });
}

void Game::updateParticles(float dt) {
    if (dt <= 0) return;
    nukeFlash = std::max(0.0f, nukeFlash - dt);
    for (auto& p : parts) {
        p.age += dt; p.x += p.vx * dt; p.y += p.vy * dt; p.vy += p.ay * dt; p.rot += p.vrot * dt;
        if (p.drag > 0) { float k = std::exp(-p.drag * dt); p.vx *= k; p.vy *= k; }
    }
    for (size_t i = 0; i < parts.size();) { if (parts[i].age >= parts[i].life) { parts[i] = parts.back(); parts.pop_back(); } else i++; }
    for (auto& d : decals) d.age += dt;
    decals.erase(std::remove_if(decals.begin(), decals.end(), [](const Decal& d) { return d.age >= d.life; }), decals.end());
}

// First sight of a sim effect: add the cosmetic bits that make blasts read as blasts (flying chunks, sparks, a smoke plume, a scorch mark).
void Game::spawnFromFx() {
    for (auto& f : g_sim.fx) {
        if (f.seen || f.t < 0) continue;
        f.seen = true;
        if (!onScreen(f.a.x, f.a.y, 120 + (f.type == FX_EXPLODE ? f.size * 1.5f : 0.0f))) continue;
        Rng& r = fxRng;
        switch (f.type) {
        case FX_EXPLODE: {
            float s = f.size;
            int chunks = 2 + (int)(s / 9), sparks = 4 + (int)(s / 3.5f);
            for (int i = 0; i < chunks; i++) { float an = r.f(0, 6.283f), sp = r.f(50, 70 + s * 3.2f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.7f - r.f(60, 150), r.f(0.5f, 1.0f), r.f(0.7f, 1.2f), r.f(0.7f, 1.2f), shade(rgb(96, 90, 82), r.f(0.6f, 1.2f)), PK_DEBRIS, (u8)r.range(0, 3), 420, 0, r.f(0, 6), r.f(-14, 14)); }
            for (int i = 0; i < sparks; i++) { float an = r.f(0, 6.283f), sp = r.f(80, 140 + s * 4.0f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp, r.f(0.25f, 0.6f), r.f(1.0f, 1.8f), 0.4f, rgb(255, 214, 120), PK_SPARK, 0, 120, 3.0f); }
            int puffs = 1 + (int)(s / 12);
            for (int i = 0; i < puffs; i++) emitP(f.a.x + r.f(-s * 0.3f, s * 0.3f), f.a.y + r.f(-s * 0.3f, s * 0.3f), r.f(-10, 10), r.f(-30, -12), r.f(1.8f, 3.2f), s * r.f(0.35f, 0.55f), s * r.f(0.9f, 1.4f), rgb(52, 50, 48), PK_SMOKE, (u8)r.range(0, 3), 0, 0.6f, r.f(0, 6), r.f(-0.4f, 0.4f));
            if (s >= 20) emitP(f.a.x, f.a.y, 0, 0, 0.6f, s * 0.35f, s * 2.2f, rgb(255, 196, 130), PK_RING);
            if (s >= 55 && s < 150) {   // heavy bomb: a rolling dust ring, a fire column and a plume of smoke climbing from the crater
                for (int i = 0; i < 12; i++) { float an = i * 0.5236f + r.f(-0.15f, 0.15f), sp = r.f(110, 190) * (s / 70.0f + 0.4f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.5f, r.f(0.7f, 1.2f), s * 0.12f, s * 0.42f, rgb(150, 132, 108, 170), PK_DUST, (u8)r.range(0, 2), 0, 3.4f, r.f(0, 6), r.f(-0.5f, 0.5f)); }
                for (int i = 0; i < 7; i++) emitP(f.a.x + r.f(-s * 0.15f, s * 0.15f), f.a.y - i * s * 0.1f, r.f(-8, 8), -r.f(70, 150), r.f(0.5f, 0.9f), s * r.f(0.2f, 0.34f), s * 0.08f, rgb(255, 170 + r.range(0, 60), 80), PK_FLAME, (u8)r.range(0, 2), -40, 1.2f);
                for (int i = 0; i < 5; i++) emitP(f.a.x + r.f(-s * 0.2f, s * 0.2f), f.a.y - r.f(0, s * 0.3f), r.f(-10, 10), -r.f(36, 90), r.f(2.2f, 3.6f), s * r.f(0.28f, 0.4f), s * r.f(0.8f, 1.15f), rgb(70, 66, 62, 150), PK_SMOKE, (u8)r.range(0, 3), 0, 0.5f, r.f(0, 6), r.f(-0.3f, 0.3f));
                for (int i = 0; i < 16; i++) { float an = r.f(0, 6.283f), sp = r.f(160, 300); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.8f - r.f(50, 140), r.f(0.3f, 0.7f), r.f(1.2f, 2.2f), 0.4f, rgb(255, 224, 150), PK_SPARK, 0, 160, 2.6f); }
            }
            if (s >= 150) {   // tactical nuke: a rising stem, an opening cap, a dust ring, a huge scorch and a flash
                for (int i = 0; i < 16; i++) emitP(f.a.x + r.f(-s * 0.1f, s * 0.1f), f.a.y + r.f(-s * 0.1f, s * 0.1f), r.f(-6, 6), -r.f(40, 100), r.f(5.0f, 7.5f), s * 0.16f, s * 0.38f, rgb(86, 76, 68, 215), PK_SMOKE, (u8)r.range(0, 3), 0, 0.3f, r.f(0, 6), r.f(-0.2f, 0.2f));
                for (int i = 0; i < 14; i++) { float an = i * 0.4488f + r.f(-0.1f, 0.1f), sp = r.f(30, 60); emitP(f.a.x, f.a.y - s * 0.3f, std::cos(an) * sp, std::sin(an) * sp * 0.55f - 22, r.f(5.0f, 7.0f), s * 0.2f, s * 0.46f, rgb(96, 82, 70, 200), PK_SMOKE, (u8)r.range(0, 3), 0, 0.5f, r.f(0, 6), r.f(-0.3f, 0.3f)); }
                emitP(f.a.x, f.a.y, 0, 0, 1.6f, s * 0.2f, s * 1.9f, rgb(255, 214, 150), PK_RING);
                emitP(f.a.x, f.a.y, 0, 0, 2.4f, s * 0.2f, s * 2.6f, rgb(255, 170, 100), PK_RING);
                decals.push_back(Decal{ f.a.x, f.a.y, s * 1.5f, r.f(0, 6.28f), 0, 90.0f, 1 });
                nukeFlash = 0.55f;
            }
            if (s >= 15 && decals.size() < 56) decals.push_back(Decal{ f.a.x, f.a.y, s * r.f(1.0f, 1.25f), r.f(0, 6.28f), 0, r.f(32, 46), (u8)r.range(0, 2) });
            break;
        }
        case FX_WRECK: if (f.vel.y > 0.5f) for (int i = 0; i < 2; i++) emitP(f.a.x + r.f(-3, 3), f.a.y + r.f(-3, 3), r.f(-10, 10), r.f(-12, 4), r.f(0.5f, 0.8f), 3, 9, rgb(150, 126, 100, 150), PK_DUST, (u8)r.range(0, 2), 0, 1.6f); break;
        case FX_BEAM: case FX_RAIL: {
            int n = f.type == FX_RAIL ? 5 : 2;
            for (int i = 0; i < n; i++) { float an = r.f(0, 6.283f), sp = r.f(40, 130); emitP(f.b.x, f.b.y, std::cos(an) * sp, std::sin(an) * sp, r.f(0.15f, 0.35f), r.f(0.9f, 1.4f), 0.3f, f.color, PK_SPARK, 0, 60, 4.0f); }
            break;
        }
        default: break;
        }
    }
}

// ================================================================= drawing
void Game::drawGroundFx() {
    Gfx& g = g_gfx;
    for (auto& d : decals) {
        if (!onScreen(d.x, d.y, d.r + 8)) continue;
        float k = d.age / d.life, a = 255 * clampf((1 - k) * 4.0f, 0, 1) * 0.85f;
        Vec2 s = worldToScreen(Vec2(d.x, d.y));
        g.draw(g.fxs.scorch[d.kind % 3], s.x, s.y, d.rot, d.r * 2.0f / 96.0f, rgb(255, 255, 255), (u8)a);
    }
    for (auto& f : g_sim.fx) {
        float k = f.t / f.life;
        if (f.type == FX_RUBBLE) {
            if (!onScreen(f.a.x, f.a.y, 140)) continue;
            int w = (int)f.b.x, h = (int)f.b.y;
            int idx = w == 1 ? 0 : (w == 2 ? 1 : (w == 3 ? (h == 2 ? 2 : 3) : (w == 4 ? (h == 4 ? 5 : 4) : 6)));
            Vec2 s = worldToScreen(f.a);
            u8 al = (u8)(255 * clampf((1 - k) * 8.0f, 0, 1));
            g.draw(g.rubble[idx], s.x, s.y, 0, 1, rgb(255, 255, 255), al);
            if (f.t < 18.0f) {   // embers still glowing in the wreckage, and the odd thread of smoke
                Rng r((u64)f.size * 31 + 7);
                int n = 3 + w * h / 3;
                for (int i = 0; i < n; i++) {
                    float x = s.x + r.f(-0.4f, 0.4f) * w * TILE, y = s.y + r.f(-0.4f, 0.4f) * h * TILE;
                    float fl = 0.5f + 0.5f * std::sin(wallTime * (5.0f + i) + i * 2.3f), fade = 1.0f - f.t / 18.0f;
                    g.glowAdd(x, y, 8 + 3 * fl, Color{255, 110, 40, (u8)(110 * fl * fade)});
                }
                if (frameDt > 0 && fxRng.f() < 1.4f * frameDt && f.t < 14.0f) emitP(f.a.x + fxRng.f(-0.35f, 0.35f) * w * TILE, f.a.y + fxRng.f(-0.35f, 0.35f) * h * TILE, fxRng.f(-4, 4), fxRng.f(-22, -12), fxRng.f(1.8f, 3.0f), 6, 16, rgb(70, 66, 62), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.2f);
            }
        } else if (f.type == FX_WRECK && f.vel.y > 0.5f) {   // fallen soldier: dark and flat, fading out
            if (!onScreen(f.a.x, f.a.y, 24)) continue;
            Vec2 a = worldToScreen(f.a);
            int ut = (int)f.b.x, ow = slotOf((int)f.b.y);
            u8 al = (u8)(235 * clampf((1 - k) * 3.0f, 0, 1));
            g.draw(g.unitBody[ut][ow], a.x, a.y, f.vel.x, 0.92f, rgb(92, 84, 80), al);
        } else if (f.type == FX_WRECK) {
            if (!onScreen(f.a.x, f.a.y, 40)) continue;
            Vec2 a = worldToScreen(f.a);
            int ut = (int)f.b.x, ow = slotOf((int)f.b.y);
            u8 al = (u8)(255 * clampf((1 - k) * 5.0f, 0, 1));
            Color cinder = rgb(58, 54, 50);
            g.draw(g.unitBody[ut][ow], a.x, a.y, f.vel.x, 1, cinder, al);
            if (g.unitTurret[ut][ow].tex) g.draw(g.unitTurret[ut][ow], a.x + 2, a.y + 1, f.size + 0.5f, 1, cinder, al);
            if (f.t < 6.0f) {
                float fade = 1 - f.t / 6.0f; int fr = ((int)(wallTime * 12) + (int)f.a.x) % 6;
                g.draw(g.fxs.flame[fr], a.x - 2, a.y + 4, 0, 0.55f, rgb(255, 255, 255), (u8)(210 * fade));
                g.glowAdd(a.x, a.y, 14 + 4 * std::sin(wallTime * 9 + f.a.x), Color{255, 120, 40, (u8)(80 * fade)});
            }
        }
    }
}

void Game::drawFx() {
    Gfx& g = g_gfx;
    for (auto& f : g_sim.fx) {
        if (f.t < 0 || f.type == FX_RUBBLE || f.type == FX_WRECK) continue;
        float k = f.t / f.life;
        Vec2 a = worldToScreen(f.a), b = worldToScreen(f.b);
        if ((f.type == FX_BEAM || f.type == FX_RAIL || f.type == FX_ARC || f.type == FX_MUSHROOM || f.type == FX_FALLOUT) ? false : (a.x < -80 || a.y < -80 || a.x > SCREEN_W + 80 || a.y > VIEW_H + 80)) continue;
        switch (f.type) {
        case FX_BEAM: {   // textured bolt: wide soft glow, bright core, hot white filament
            float len = hyp(b.x - a.x, b.y - a.y); if (len < 1) break;
            float ang = std::atan2(b.y - a.y, b.x - a.x), al = 1 - k;
            Vec2 mid = (a + b) * 0.5f;
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 12, ang, f.color, (u8)(150 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 5, ang, f.color, (u8)(255 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 2, ang, rgb(255, 255, 255), (u8)(255 * al));
            g.glowAdd(b.x, b.y, 11 * (1 - k * 0.4f), Color{f.color.r, f.color.g, f.color.b, (u8)(170 * al)});
            g.glowAdd(a.x, a.y, 7, Color{f.color.r, f.color.g, f.color.b, (u8)(120 * al)});
            break;
        }
        case FX_RAIL: {
            float len = hyp(b.x - a.x, b.y - a.y); if (len < 1) break;
            float ang = std::atan2(b.y - a.y, b.x - a.x), al = 1 - k;
            Vec2 mid = (a + b) * 0.5f;
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 26 * (1 - k * 0.5f), ang, rgb(110, 190, 255), (u8)(140 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 9, ang, f.color, (u8)(255 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 3, ang, rgb(255, 255, 255), (u8)(255 * al));
            for (int i = 1; i <= 4; i++) { Vec2 q = a + (b - a) * (i / 5.0f); g.drawSized(g.fxs.ring, q.x, q.y, 18 + 26 * k, 18 + 26 * k, 0, rgb(150, 210, 255), (u8)(120 * al)); }
            g.glowAdd(a.x, a.y, 16, Color{200, 230, 255, (u8)(200 * al)});
            break;
        }
        case FX_ARC: {
            Color c = f.color; float al = 1 - k;
            Rng r((u64)(f.t * 1000) + (u64)f.a.x);
            Vec2 prev = a; int segs = 6;
            for (int i = 1; i <= segs; i++) {
                float t = i / (float)segs; Vec2 q = a + (b - a) * t;
                if (i < segs) q += Vec2(r.f(-7, 7), r.f(-7, 7));
                float len = hyp(q.x - prev.x, q.y - prev.y);
                if (len > 0.5f) { Vec2 mid = (prev + q) * 0.5f, dv = q - prev; float ang = std::atan2(dv.y, dv.x);
                    g.drawSized(g.fxs.beam, mid.x, mid.y, len + 3, 9, ang, c, (u8)(120 * al));
                    g.drawSized(g.fxs.beam, mid.x, mid.y, len + 1, 3, ang, rgb(255, 255, 255), (u8)(255 * al)); }
                prev = q;
            }
            g.glowAdd(b.x, b.y, 12, Color{c.r, c.g, c.b, (u8)(150 * al)});
            break;
        }
        case FX_FLASH: {
            float al = 1 - k, sz = f.size * 5.2f * (1 - k * 0.3f);
            int v = (int)(std::fabs(f.a.x * 7 + f.a.y * 3)) % 3;
            g.drawSized(g.fxs.flash[v], a.x, a.y, sz * 1.6f, sz * 1.6f, (float)((int)(f.a.x * 13 + f.a.y * 5) % 100) * 0.0628f, f.color, (u8)(255 * al));
            g.glowAdd(a.x, a.y, f.size * 3.6f, Color{f.color.r, f.color.g, f.color.b, (u8)(150 * al)});
            break;
        }
        case FX_EXPLODE: {
            int fr = std::min(7, (int)(k * 8.0f));
            float rr = f.size * (0.35f + 0.65f * std::sqrt(k)), al = 1 - k * k * k;
            g.glowAdd(a.x, a.y, rr * (2.3f + 0.8f * (1 - k)), Color{255, 140, 50, (u8)(150 * (1 - k))});
            g.drawSized(g.fxs.fire[fr], a.x, a.y, rr * 2.5f, rr * 2.5f, std::fmod(f.a.x * 0.37f, 6.28f), rgb(255, 255, 255), (u8)(255 * al));
            if (k < 0.2f) g.glowAdd(a.x, a.y, rr * 1.4f, Color{255, 252, 220, (u8)(230 * (1 - k * 5))});
            break;
        }
        case FX_SMOKE: {
            int v = (int)(std::fabs(f.a.x * 3.1f + f.a.y * 1.7f)) & 3;
            float rad = f.size * (0.6f + k) * 1.15f, al = f.color.a / 255.0f * 0.9f;
            float fade = std::min(1.0f, f.t * 6.0f) * (1 - k);
            g.drawSized(g.fxs.smoke[v], a.x, a.y, rad * 2, rad * 2, f.t * 0.3f + (float)v, Color{f.color.r, f.color.g, f.color.b, 255}, (u8)(255 * clampf(0.55f * fade * (f.color.r < 120 ? 1.15f : 1.0f), 0, 1) * (al > 0 ? 1 : 1)));
            break;
        }
        case FX_SPARK: {
            float al = 1 - k;
            g.glowAdd(a.x, a.y, f.size * 2.6f, Color{f.color.r, f.color.g, f.color.b, (u8)(200 * al)});
            g.glowAdd(a.x, a.y, f.size * 1.0f, Color{255, 255, 255, (u8)(220 * al)});
            break;
        }
        case FX_DEBRIS: {
            int v = (int)(std::fabs(f.a.x * 1.3f)) & 3;
            g.drawSized(g.fxs.debris[v], a.x, a.y, f.size * 2.2f, f.size * 2.2f, f.t * 9.0f + (float)v, shade(f.color, 1.3f), (u8)(255 * clampf((1 - k) * 3, 0, 1)));
            break;
        }
        case FX_RING: {
            float d = f.size * 2.0f * (0.15f + 0.85f * k);
            g.drawSized(g.fxs.ring, a.x, a.y, d, d, 0, f.color, (u8)(220 * (1 - k)));
            break;
        }
        case FX_EMP: {
            float d = f.size * 2.0f * k;
            g.drawSized(g.fxs.ring, a.x, a.y, d, d, 0, f.color, (u8)(255 * (1 - k)));
            g.drawSized(g.fxs.ring, a.x, a.y, d * 0.7f, d * 0.7f, 0, f.color, (u8)(150 * (1 - k)));
            g.drawSized(g.fxs.ring, a.x, a.y, d * 0.4f, d * 0.4f, 0, f.color, (u8)(90 * (1 - k)));
            break;
        }
        case FX_MUSHROOM: {   // a rolling toroidal cap on a turbulent stem, a condensation collar, a base surge and a fire-lit underside
            float R = f.size * 0.52f, t = f.t;
            float g1 = clampf(t / 9.0f, 0, 1), grow = 1 - (1 - g1) * (1 - g1) * (1 - g1);
            float fade = clampf((1 - k) * 3.5f, 0, 1);
            float heat = clampf(1 - t / 7.5f, 0, 1);
            float H = R * 2.05f * grow, Rc = R * (0.2f + 0.78f * grow);
            Vec2 top(a.x, a.y - H);
            auto lerpc = [](Color x, Color y, float u) { u = clampf(u, 0, 1); return Color{(u8)(x.r + (y.r - x.r) * u), (u8)(x.g + (y.g - x.g) * u), (u8)(x.b + (y.b - x.b) * u), 255}; };
            const Color ash{206, 188, 172, 255}, soot{118, 108, 102, 255}, ember{255, 150, 66, 255}, lit{255, 246, 232, 255};
            if (heat > 0) g.glowAdd(a.x, a.y, R * 1.25f * heat, Color{255, 140, 50, (u8)(180 * heat * fade)});
            // base surge: a low skirt of dust billows rolling outward along the ground
            float sg = clampf(t / 10.0f, 0, 1); sg = 1 - (1 - sg) * (1 - sg);
            for (int i = 0; i < 16; i++) {
                float an = i * 0.3927f + std::sin(i * 2.3f) * 0.12f, rr = R * (0.28f + 1.05f * sg) * (0.86f + 0.14f * std::sin(i * 3.1f + t * 0.4f));
                float rad = R * (0.2f + 0.1f * sg) + 8;
                g.drawSized(g.fxs.billow[i & 3], a.x + std::cos(an) * rr, a.y + std::sin(an) * rr * 0.42f + 4, rad * 2.4f, rad * 1.6f, 0, lerpc(Color{168, 146, 120, 255}, ash, sg), (u8)(150 * fade * (1.0f - 0.4f * sg)));
            }
            // stem: turbulent billows, fat at the foot, narrow in the middle, flaring into the cap; the lower part still glows
            const int NS = 30;
            for (int i = 0; i < NS; i++) {
                float u = i / (float)(NS - 1);
                float prof = 1.45f - 0.95f * std::sin(u * 3.1416f * 0.62f) + 0.55f * u * u * u;   // foot wide, waist narrow, throat widening under the cap
                float wob = std::sin(t * 1.1f + i * 1.9f) * R * 0.05f * (0.5f + u) + std::sin(t * 0.5f + u * 5.0f) * R * 0.035f;
                float rad = R * 0.15f * prof * (0.65f + 0.35f * grow) + 7;
                Vec2 p(a.x + wob, a.y - H * u * 0.97f);
                Color c = lerpc(lerpc(ash, soot, u * 0.4f), ember, clampf(heat * (1 - u * 1.3f), 0, 1) * 0.85f);
                g.drawSized(g.fxs.billow[i & 3], p.x, p.y, rad * 2.5f, rad * 2.5f, 0, c, (u8)(235 * fade));
            }
            // condensation collar: a pale ring that opens around the stem and fades
            { float cu = clampf((t - 1.5f) / 2.5f, 0, 1), cf = cu * clampf(1 - (t - 4.0f) / 9.0f, 0, 1) * fade;
              if (cf > 0.02f) { Vec2 cp(a.x, a.y - H * 0.5f); float cr = Rc * (0.55f + 0.25f * cu);
                for (int i = 0; i < 14; i++) { float an = i * 0.4488f + t * 0.08f; g.drawSized(g.fxs.billow[(i + 2) & 3], cp.x + std::cos(an) * cr, cp.y + std::sin(an) * cr * 0.28f, R * 0.5f, R * 0.4f, 0, Color{236, 232, 228, 255}, (u8)(120 * cf)); } } }
            // cap: two rolling tori of billows (an outer rolling lip and a smaller inner one), back half first, then the core, then the front half
            auto torus = [&](bool front) {
                for (int ring = 0; ring < 2; ring++) {
                    int n = ring ? 18 : 32;
                    for (int i = 0; i < n; i++) {
                        float th = i * 6.2832f / n + t * (ring ? -0.07f : 0.05f) + ring * 0.4f, sn = std::sin(th);
                        if ((sn > 0) != front) continue;
                        float phi = i * 1.9f + t * (ring ? 1.5f : 1.0f);                         // the roll of the vortex ring
                        float rm = Rc * (ring ? 0.22f : 0.3f), rc = Rc * (ring ? 0.42f : 0.8f);
                        float radial = rc + rm * std::cos(phi);
                        Vec2 p(top.x + std::cos(th) * radial, top.y + sn * radial * 0.34f - rm * std::sin(phi) * 0.85f - Rc * (ring ? 0.12f : 0.0f));
                        float up = std::sin(phi) * 0.5f + 0.5f;                                // top of the roll catches the light, the underside glows
                        Color c = lerpc(lerpc(ash, lit, up * 0.8f), ember, clampf(heat * (1 - up) * (sn > 0 ? 1.0f : 0.6f), 0, 1) * 0.9f);
                        if (up > 0.5f) c = lerpc(c, soot, clampf((t - 8.0f) / 10.0f, 0, 0.6f));
                        float rad = rm * 1.08f + 9;
                        g.drawSized(g.fxs.billow[(i + ring) & 3], p.x, p.y, rad * 2.5f, rad * 2.5f, 0, c, (u8)(255 * fade));
                    }
                }
            };
            torus(false);
            for (int i = 0; i < 10; i++) {   // dense core of the cap
                float an = i * 0.628f + t * 0.2f; Vec2 p(top.x + std::cos(an) * Rc * 0.4f, top.y - Rc * 0.14f + std::sin(an) * Rc * 0.16f);
                g.drawSized(g.fxs.billow[i & 3], p.x, p.y, Rc * 1.15f, Rc * 0.9f, 0, lerpc(lerpc(ash, lit, 0.35f), ember, heat * 0.5f), (u8)(240 * fade));
            }
            torus(true);
            for (int i = 0; i < 9; i++) {    // pale smooth hat riding on top of the cap
                float an = i * 0.698f + t * 0.1f; Vec2 p(top.x + std::cos(an) * Rc * 0.38f, top.y - Rc * 0.34f + std::sin(an) * Rc * 0.1f);
                g.drawSized(g.fxs.billow[(i + 3) & 3], p.x, p.y, Rc * 0.7f, Rc * 0.45f, 0, lit, (u8)(190 * fade * grow));
            }
            if (heat > 0) {
                g.glowAdd(top.x, top.y + Rc * 0.18f, Rc * 1.15f, Color{255, 160, 70, (u8)(200 * heat * fade)});
                g.glowAdd(top.x, top.y + Rc * 0.05f, Rc * 0.62f, Color{255, 238, 196, (u8)(210 * heat * fade)});
                g.glowAdd(a.x, a.y - H * 0.25f, R * 0.35f, Color{255, 170, 80, (u8)(120 * heat * fade)});
            }
            break;
        }
        case FX_FALLOUT: {   // sickly green haze, a pulsing boundary and drifting motes
            float R = f.size, fade = f.t < 3.0f ? f.t / 3.0f : clampf((1 - k) * 8.0f, 0, 1);
            float pulse = 0.5f + 0.5f * std::sin(wallTime * 2.4f);
            float cx = a.x, cy = a.y;
            if (cx < -R - 40 || cy < -R - 40 || cx > SCREEN_W + R + 40 || cy > VIEW_H + R + 40) break;
            g.glowAdd(cx, cy, R * 0.95f, Color{70, 220, 40, (u8)((26 + 18 * pulse) * fade)});
            g.drawSized(g.fxs.ring, cx, cy, R * 2.0f, R * 2.0f, 0, rgb(120, 255, 80), (u8)((90 + 90 * pulse) * fade));
            g.drawSized(g.fxs.ring, cx, cy, R * 1.5f, R * 1.5f, 0, rgb(120, 255, 80), (u8)(40 * fade));
            for (int i = 0; i < 26; i++) {
                float h1 = std::fmod(i * 0.6180339f, 1.0f), h2 = std::fmod(i * 0.7548776f + 0.3f, 1.0f);
                float an = h1 * 6.2832f + wallTime * 0.05f * (1 + h2), rr = R * std::sqrt(h2) * 0.95f;
                float fl = 0.5f + 0.5f * std::sin(wallTime * (1.5f + h1 * 3) + i);
                g.glowAdd(cx + std::cos(an) * rr, cy + std::sin(an) * rr * 1.0f - fl * 6, 9 + 5 * fl, Color{110, 255, 70, (u8)(90 * fl * fade)});
            }
            break;
        }
        default: break;
        }
    }
}

void Game::drawParticles(int pass) {   // pass 0: smoke, dust and debris (under the fire); pass 1: sparks, flames, rings and glows (over it)
    Gfx& g = g_gfx;
    {
        for (auto& p : parts) {
            bool additive = p.kind == PK_SPARK || p.kind == PK_FLAME || p.kind == PK_RING || p.kind == PK_GLOW;
            if (additive != (pass == 1)) continue;
            Vec2 s = worldToScreen(Vec2(p.x, p.y));
            if (s.x < -40 || s.y < -40 || s.x > SCREEN_W + 40 || s.y > VIEW_H + 40) continue;
            float k = p.age / p.life;
            switch (p.kind) {
            case PK_SMOKE: case PK_DUST: case PK_CONTRAIL: {
                float sz = lerpf(p.s0, p.s1, std::sqrt(k));
                float al = clampf(p.age * (p.kind == PK_SMOKE ? 3.2f : 7.0f), 0, 1) * std::pow(1 - k, p.kind == PK_CONTRAIL ? 1.0f : 1.3f) * (p.col.a / 255.0f);
                const Sprite& sp = p.kind == PK_DUST ? g.fxs.dust[p.var % 3] : g.fxs.smoke[p.var & 3];
                g.drawSized(sp, s.x, s.y, sz * 2, sz * 2, p.rot, Color{p.col.r, p.col.g, p.col.b, 255}, (u8)(255 * al * 0.7f));
                break;
            }
            case PK_DEBRIS: {
                float al = clampf((1 - k) * 3.0f, 0, 1);
                g.drawSized(g.fxs.debris[p.var & 3], s.x, s.y, p.s0 * 7, p.s0 * 7, p.rot, p.col, (u8)(255 * al));
                break;
            }
            case PK_SPARK: {
                float sp = hyp(p.vx, p.vy), al = 1 - k;
                float len = p.s0 * (3.0f + sp * 0.07f);
                g.drawSized(g.fxs.streak, s.x, s.y, len, p.s0 * 1.6f, std::atan2(p.vy, p.vx), p.col, (u8)(255 * al));
                break;
            }
            case PK_FLAME: {
                int fr = ((int)(p.age * 14) + p.var) % 6;
                float sz = lerpf(p.s0, p.s1, k), al = 1 - k * k;
                g.drawSized(g.fxs.flame[fr], s.x, s.y + sz * 0.45f, sz * 0.7f, sz * 1.1f, 0, p.col, (u8)(255 * al));
                break;
            }
            case PK_RING: {
                float d = lerpf(p.s0, p.s1, std::sqrt(k)) * 2;
                g.drawSized(g.fxs.ring, s.x, s.y, d, d, 0, p.col, (u8)(85 * (1 - k) * (1 - k)));
                break;
            }
            case PK_GLOW: {
                g.glowAdd(s.x, s.y, lerpf(p.s0, p.s1, k), Color{p.col.r, p.col.g, p.col.b, (u8)(p.col.a * (1 - k))});
                break;
            }
            }
        }
    }
}

// Slow cloud shadows drifting across the ground: very faint, a handful of draws, and they give the static map a sense of weather.
void Game::drawClouds() {
    Gfx& g = g_gfx;
    if (g.softwareRenderer) return;   // big alpha blends are the one thing a CPU-only renderer pays dearly for
    const float span = WORLD_W + 1800.0f;
    for (int i = 0; i < 9; i++) {
        float bx = 311.0f * (i * 7 % 9) + 190.0f * i, by = 270.0f * (i * 5 % 9) + 130.0f * (i % 3);
        float x = std::fmod(bx + wallTime * (7.0f + (i % 3) * 2.0f), span) - 900.0f, y = by + std::sin(wallTime * 0.04f + i * 1.7f) * 90.0f;
        float w = 760.0f + 160.0f * (i % 4), h = w * (0.62f + 0.06f * (i % 3));
        Vec2 s = worldToScreen(Vec2(x, y));
        if (s.x + w * 0.5f < 0 || s.x - w * 0.5f > SCREEN_W || s.y + h * 0.5f < 0 || s.y - h * 0.5f > VIEW_H) continue;
        g.drawSized(g.fxs.cloud[i % 3], s.x, s.y, w, h, 0, rgb(0, 6, 12), 40);
    }
}
