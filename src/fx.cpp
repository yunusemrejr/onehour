// One Hour - visual effects: baked effect sprites, the cosmetic particle system and the drawing of simulation effects.
#include "ui.h"
#include "art.h"
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
// The colour of a hot cloud as it cools: white-hot, yellow, orange, red, dark red-brown, then smoke (sv: the silver of an Underground Nuke, cooling to
// grey). h: 1 = as hot as it gets, 0 = cold.
Color heatRamp(float h, bool sv) {
    static const float P[6] = { 1.0f, 0.82f, 0.6f, 0.38f, 0.16f, 0.0f };
    static const Color S[6] = { rgb(255, 250, 236), rgb(255, 214, 122), rgb(255, 128, 46), rgb(184, 52, 24), rgb(96, 34, 26), rgb(58, 50, 48) };
    static const Color V[6] = { rgb(252, 254, 255), rgb(226, 232, 242), rgb(184, 192, 204), rgb(134, 142, 154), rgb(88, 94, 104), rgb(60, 64, 72) };
    const Color* c = sv ? V : S;
    h = clampf(h, 0, 1);
    for (int i = 0; i < 5; i++) if (h >= P[i + 1]) return lerpC(c[i + 1], c[i], (h - P[i + 1]) / (P[i] - P[i + 1]));
    return c[5];
}

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
void fireCanvas(Canvas& c, int f) {   // the fireball's body: a hot orange mass that churns, darkens to red-brown at its ragged edge and cools to smoke
    float R = 14 + f * 2.6f;
    float cool = clampf((f - 3) / 4.0f, 0, 1);
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = x + 0.5f - 32, dy = y + 0.5f - 32, d = std::sqrt(dx * dx + dy * dy) / R;
        float n = fb(x * 0.12f + f * 2.3f, y * 0.12f - f * 1.1f, 311);
        float n2 = fb(x * 0.33f - f * 1.7f, y * 0.33f + f * 0.9f, 317);   // finer churn: the edge breaks up into lumps
        float t = d + (n - 0.5f) * 0.62f + (n2 - 0.5f) * 0.34f;
        if (t >= 1.06f) continue;
        t = std::max(t, 0.0f);
        Color col;
        if (t < 0.3f) col = lerpC(rgb(255, 190, 90), rgb(255, 138, 40), t / 0.3f);
        else if (t < 0.6f) col = lerpC(rgb(255, 138, 40), rgb(206, 70, 24), (t - 0.3f) / 0.3f);
        else if (t < 0.88f) col = lerpC(rgb(206, 70, 24), rgb(108, 32, 22), (t - 0.6f) / 0.28f);
        else col = lerpC(rgb(108, 32, 22), rgb(52, 36, 32), (t - 0.88f) / 0.18f);
        col = lerpC(col, rgb(62, 56, 52), cool * sst(0.05f, 0.8f, t));
        float a = clampf((1.0f - t) * 2.4f, 0, 1) * (0.78f + 0.22f * n2) * (1.0f - 0.18f * cool);
        col.a = (u8)(a * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
// the white-hot heart of a fireball, drawn additively over the body: it shrinks and yellows as the blast climbs
void fireCoreCanvas(Canvas& c, int f) {
    const float Rc = 20.0f - 7.0f * f / 7.0f;
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = x + 0.5f - 32, dy = y + 0.5f - 32, d = std::sqrt(dx * dx + dy * dy) / Rc;
        float n = fb(x * 0.2f + f * 3.1f, y * 0.2f - f * 2.2f, 411);
        float t = d + (n - 0.5f) * 0.7f;
        if (t >= 1) continue;
        t = std::max(t, 0.0f);
        Color col = t < 0.35f ? lerpC(rgb(255, 252, 240), rgb(255, 236, 170), t / 0.35f) : lerpC(rgb(255, 236, 170), rgb(255, 150, 60), clampf((t - 0.35f) / 0.65f, 0, 1));
        col.a = (u8)(clampf(std::pow(1 - t, 1.2f), 0, 1) * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
void flameCanvas(Canvas& c, int f) {
    for (int y = 0; y < c.h; y++) {
        float v = (44.0f - y) / 44.0f;
        if (v < -0.04f) continue;
        v = std::max(v, 0.0f);
        float width = 9.0f * std::pow(1 - v, 0.65f) * (0.8f + 0.4f * std::sin(f * 1.1f + v * 5));
        float sway = std::sin(f * 1.05f + v * 3.5f) * 3.6f * v + (vn(y * 0.42f + f * 1.9f, 0.3f * f, 520) - 0.5f) * 3.4f * v;   // (the licks: the edge wanders row by row)
        for (int x = 0; x < c.w; x++) {
            float u = (x + 0.5f - 16 - sway) / std::max(width, 0.3f);
            if (std::abs(u) >= 1) continue;
            float inner = 1 - std::abs(u);
            float t = v * 0.8f + std::abs(u) * 0.5f;
            Color col = t < 0.3f ? lerpC(rgb(255, 252, 226), rgb(255, 196, 84), t / 0.3f) : t < 0.6f ? lerpC(rgb(255, 196, 84), rgb(255, 116, 36), (t - 0.3f) / 0.3f)
                                                                                                    : lerpC(rgb(255, 116, 36), rgb(196, 44, 18), clampf((t - 0.6f) / 0.4f, 0, 1));
            float a = std::pow(inner, 0.7f) * (1 - std::pow(v, 1.5f));
            col.a = (u8)(clampf(a, 0, 1) * 255);
            c.px[y * c.w + x] = Canvas::pack(col);
        }
    }
}
void flashCanvas(Canvas& c, int v) {   // a muzzle flash or the first light of a blast: a white-hot centre, a yellow shoulder, thin hot rays that taper out
    int spikes = 5 + v;
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = x + 0.5f - 24, dy = y + 0.5f - 24, r = std::sqrt(dx * dx + dy * dy), ang = std::atan2(dy, dx);
        float rays = std::pow(std::abs(std::cos(ang * spikes * 0.5f + v * 0.9f)), 28.0f) * std::exp(-r / 11.0f);
        float I = clampf(std::exp(-r / 2.6f) + std::exp(-r / 6.5f) * 0.85f + rays * 1.1f + std::exp(-r / 15.0f) * 0.22f, 0, 1);
        Color col = lerpC(rgb(255, 128, 36), rgb(255, 222, 132), clampf(std::exp(-r / 8.0f) * 1.2f, 0, 1));
        col = lerpC(col, rgb(255, 255, 250), clampf(std::exp(-r / 2.6f), 0, 1));
        col.a = (u8)(I * 255);
        c.px[y * c.w + x] = Canvas::pack(col);
    }
}
// a low ring of dust rolling along the ground (drawn flattened, 2:1): the front is a lit lip on its top side, the underside falls into shade
void skirtCanvas(Canvas& c, int v) {
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        float dx = (x + 0.5f - 64) / 62.0f, dy = (y + 0.5f - 32) / 30.0f, d = std::sqrt(dx * dx + dy * dy);
        float n = fb(x * 0.11f + v * 13, y * 0.23f + v * 5, 950 + v);
        float r = d + (n - 0.5f) * 0.24f;
        float front = std::exp(-std::pow((r - 0.84f) / 0.12f, 2.0f)) * (0.45f + 0.55f * n);
        float lip = std::exp(-std::pow((r - 0.95f) / 0.035f, 2.0f)) * (dy < 0 ? 0.55f : 0.2f);
        float a = clampf(front + lip, 0, 1);
        if (a <= 0.01f) continue;
        float lum = clampf(176 + 52 * (-dy * 0.6f) + 30 * (n - 0.5f), 60, 240);
        c.px[y * c.w + x] = Canvas::pack(Color{(u8)lum, (u8)(lum * 0.88f), (u8)(lum * 0.7f), (u8)(a * 230)});
    }
}
}  // namespace

void Gfx::buildEffects() {
    { Canvas c(32, 32); c.glow(16, 16, 16, rgb(255, 255, 255, 255)); for (auto& px : c.px) { Color k = Canvas::unpack(px); k.r = k.g = k.b = 255; px = Canvas::pack(k); } blobAdd = fromCanvasAdd(c, 16, 16); blobAdd.radial = 1; }
    for (int v = 0; v < 4; v++) { Canvas c(64, 64); puffCanvas(c, v, false); fxs.smoke[v] = fromCanvasSmooth(c, 32, 32); fxs.smoke[v].radial = 1; }
    for (int v = 0; v < 3; v++) { Canvas c(64, 64); puffCanvas(c, v + 4, true); fxs.dust[v] = fromCanvasSmooth(c, 32, 32); fxs.dust[v].radial = 1; }
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
    for (int f = 0; f < 8; f++) { Canvas c(64, 64); fireCoreCanvas(c, f); fxs.core[f] = fromCanvasAdd(c, 32, 32); }
    for (int v = 0; v < 2; v++) { Canvas c(128, 64); skirtCanvas(c, v); fxs.skirt[v] = fromCanvasSmooth(c, 64, 32); }
    for (int f = 0; f < 6; f++) { Canvas c(32, 48); flameCanvas(c, f); fxs.flame[f] = fromCanvasAdd(c, 16, 44); }
    for (int v = 0; v < 3; v++) { Canvas c(48, 48); flashCanvas(c, v); fxs.flash[v] = fromCanvasAdd(c, 24, 24); }
    { Canvas c(16, 4); for (int y = 0; y < 4; y++) for (int x = 0; x < 16; x++) { float a = std::pow(x / 15.0f, 2.0f) * (1 - std::abs((y + 0.5f - 2) / 2.0f) * 0.7f); c.px[y * 16 + x] = Canvas::pack(Color{255, (u8)(200 + x * 3), (u8)(120 + x * 8), (u8)(clampf(a, 0, 1) * 255)}); } fxs.streak = fromCanvasAdd(c, 15, 2); }
    { Canvas c(128, 128); for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) { float dx = x + 0.5f - 64, dy = y + 0.5f - 64, d = std::sqrt(dx * dx + dy * dy) / 64.0f; float a = std::exp(-std::pow((d - 0.84f) / 0.08f, 2.0f)) * 0.95f + std::exp(-std::pow((d - 0.84f) / 0.32f, 2.0f)) * 0.14f; c.px[y * 128 + x] = Canvas::pack(Color{255, 255, 255, (u8)(clampf(a, 0, 1) * 255)}); } fxs.ring = fromCanvasAdd(c, 64, 64); fxs.ring.radial = 2; }
    for (int sv = 0; sv < 2; sv++) for (int in = 0; in < 2; in++) {
        // A radiation zone in one sprite: the haze glow, the edge ring and (for a zone that is not huge) the inner ring, added up as the three
        // separate additive draws were, with their colours baked in (the drawing scales the whole by the pulse). Three overlapping big quads a
        // zone was most of the screen area blended in a nuclear exchange.
        const Color haze = sv ? Color{ 150, 158, 172, 255 } : Color{ 70, 220, 40, 255 }, edge = sv ? rgb(214, 220, 232) : rgb(120, 255, 80);
        auto ringA = [](float d) { return std::exp(-std::pow((d - 0.84f) / 0.08f, 2.0f)) * 0.95f + std::exp(-std::pow((d - 0.84f) / 0.32f, 2.0f)) * 0.14f; };
        Canvas c(256, 256);
        for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) {
            float d = std::sqrt((x + 0.5f - 128) * (x + 0.5f - 128) + (y + 0.5f - 128) * (y + 0.5f - 128)) / 128.0f;
            if (d > 1.0f) continue;
            float dg = d / 0.95f, kg = dg < 1 ? (1 - dg) * (1 - dg) * 35.0f / 255.0f : 0.0f, kr = ringA(d) * 135.0f / 255.0f, ki = in ? ringA(d / 0.75f) * 40.0f / 255.0f : 0.0f;
            float r = (haze.r * kg + edge.r * (kr + ki)) * 1.3f, gg = (haze.g * kg + edge.g * (kr + ki)) * 1.3f, b = (haze.b * kg + edge.b * (kr + ki)) * 1.3f;
            c.px[y * 256 + x] = Canvas::pack(Color{ (u8)clampf(r, 0, 255), (u8)clampf(gg, 0, 255), (u8)clampf(b, 0, 255), 255 });
        }
        fxs.fallout[sv][in] = fromCanvasAdd(c, 128, 128); fxs.fallout[sv][in].radial = 1;
    }
    { Canvas c(64, 16); for (int y = 0; y < 16; y++) for (int x = 0; x < 64; x++) { float ax = sst(0, 0.09f, x / 64.0f) * sst(0, 0.09f, 1 - x / 64.0f), v = std::abs((y + 0.5f - 8) / 8.0f), ay = std::pow(1 - v, 1.7f); c.px[y * 64 + x] = Canvas::pack(Color{255, 255, 255, (u8)(clampf(ax * ay, 0, 1) * 255)}); } fxs.beam = fromCanvasAdd(c, 32, 8); }
    for (int v = 0; v < 4; v++) {
        Canvas c(16, 16); Art a(c, rgb(255, 255, 255), 2.0f); Rng r(40u + v);
        std::vector<P> pts; int n = 5 + v % 2;
        for (int i = 0; i < n; i++) { float an = i * 6.2831853f / n + r.f(-0.3f, 0.3f), rr = r.f(2.6f, 4.2f); pts.push_back({4.0f + std::cos(an) * rr, 4.0f + std::sin(an) * rr}); }
        a.poly(pts, rgb(150, 150, 150), 2.0f, 0.7f);
        fxs.debris[v] = fromCanvasSmooth(c, 8, 8);
    }
    for (int v = 0; v < 3; v++) {   // a blast's scorch: a charred core, the soot falling off to a ragged fringe with flecks of ash at its rim
        Canvas c(96, 96);
        for (int y = 0; y < 96; y++) for (int x = 0; x < 96; x++) {
            float dx = (x + 0.5f - 48) / 46.0f, dy = (y + 0.5f - 48) / 46.0f, d = std::sqrt(dx * dx + dy * dy);
            float n = fb(x * 0.08f + v * 7, y * 0.08f, 505 + v);
            float a = std::pow(clampf(1.25f - d * 1.15f + (n - 0.5f) * 0.9f, 0, 1), 1.4f);
            float charred = clampf(1.0f - d * 1.5f, 0, 1);
            if (charred > 0) a = std::max(a, charred * charred * (0.85f + 0.15f * n));
            if (a <= 0) continue;
            Color col = d > 0.62f ? Color{50, 38, 28, 0} : Color{14, 12, 10, 0};
            col.a = (u8)(a * (d > 0.62f ? 120 : 200));
            if (d > 0.7f && d < 1.0f && hh(x, y, 660 + v) > 0.9f) { col = Color{104, 94, 82, 0}; col.a = 120; }   // ash flecks
            c.px[y * 96 + x] = Canvas::pack(col);
        }
        fxs.scorch[v] = fromCanvasSmooth(c, 48, 48);
    }
    for (int v = 0; v < 2; v++) {   // tread marks: a short pressed-in dash, the track cleats showing as darker bars (v 1 is a worn, broken one)
        Canvas c(24, 8);
        for (int y = 0; y < 8; y++) for (int x = 0; x < 24; x++) {
            float dx = (x + 0.5f - 12) / 12.0f, dy = (y + 0.5f - 4) / 4.0f;
            float e = std::max(std::fabs(dx) * 0.9f, std::fabs(dy));
            float a = clampf((1.0f - e) / 0.35f, 0, 1);
            float cleat = (x % 4 < 2) ? 1.0f : 0.55f;
            float n = fb(x * 0.3f + v * 9, y * 0.3f, 640 + v);
            if (v == 1 && n < 0.42f) a *= 0.25f;
            a *= cleat;
            if (a <= 0.01f) continue;
            c.px[y * 24 + x] = Canvas::pack(Color{34, 28, 20, (u8)(a * 100)});
        }
        fxs.tread[v] = fromCanvasSmooth(c, 12, 4);
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
    // water caustics: sums of integer-frequency waves so every frame tiles seamlessly, four frames that loop. Two wave
    // sets (broad swell + fine chop) multiplied together give thin bright filaments instead of blobby squiggles.
    for (int f = 0; f < 4; f++) {
        Canvas c(128, 128);
        static const int A[5] = { 1, 3, 2, 4, 5 }, B[5] = { 2, 1, -3, 3, -2 }, M[5] = { 1, -1, 1, -1, 1 };
        static const int A2[4] = { 7, -5, 9, 4 }, B2[4] = { -4, 8, 3, -9 }, M2[4] = { -1, 1, -1, 1 };
        for (int y = 0; y < 128; y++) for (int x = 0; x < 128; x++) {
            float w = 0, w2 = 0;
            for (int k = 0; k < 5; k++) w += std::sin(6.2831853f * (A[k] * x + B[k] * y) / 128.0f + 1.3f * k + f * 1.5707963f * M[k]);
            for (int k = 0; k < 4; k++) w2 += std::sin(6.2831853f * (A2[k] * x + B2[k] * y) / 128.0f + 0.7f * k + f * 1.5707963f * M2[k]);
            float r = clampf(1.0f - std::abs(w) * 0.5f, 0, 1), r2 = clampf(1.0f - std::abs(w2) * 0.62f, 0, 1);
            float a = std::pow(r, 9.0f) * 0.45f + std::pow(r * r2, 5.0f) * 0.8f;   // (fine filaments carry it; the broad swell only hints)
            c.px[y * 128 + x] = Canvas::pack(Color{196, 236, 248, (u8)(clampf(a, 0, 1) * 80)});
        }
        waterFx[f] = fromCanvasSolo(c, 0, 0, true, SDL_BLENDMODE_ADD);
    }
    // the ocean's swell (512 px: 16 tiles before it repeats): wind-driven wavelets - thin, slightly wavy crest lines running across a
    // north-westerly wind, broken into short dashes by a slow mask, with a glint on the brightest. Everything is a sum of integer-frequency
    // waves (the warp too), so the pattern tiles seamlessly; the four frames move the breakup mask and the glints, not the crest lines, so the
    // loop twinkles like light on moving water instead of stepping
    for (int f = 0; f < 4; f++) {
        const int N = 512;
        const float TAU = 6.2831853f, Q = f * 1.5707963f;
        Canvas c(N, N);
        static const int A[6] = { 6, 8, 5, 10, 7, 12 }, B[6] = { 7, 5, 9, 8, 11, 7 };   // wave vectors pointing along the wind (crests run across it)
        static const float WT[6] = { 1.0f, 0.85f, 0.8f, 0.6f, 0.55f, 0.45f };
        for (int y = 0; y < N; y++) for (int x = 0; x < N; x++) {
            float X = x + 7.0f * std::sin(TAU * (2 * x - 3 * y) / N + 0.7f) + 4.0f * std::sin(TAU * (5 * x + 1 * y) / N + 2.1f);   // (the crests wander)
            float Y = y + 7.0f * std::sin(TAU * (3 * x + 2 * y) / N + 1.9f) + 4.0f * std::sin(TAU * (-1 * x + 5 * y) / N + 0.3f);
            float a = 0;
            for (int k = 0; k < 6; k++) {
                float th = TAU * (A[k] * X + B[k] * Y) / N + 1.37f * k;
                float ridge = std::pow(std::max(0.0f, std::cos(th)), 14.0f);   // a thin bright line on each crest
                float m = 0.5f + 0.5f * std::sin(TAU * ((3 + k) * x - (2 + (k & 1)) * y) / N + 0.9f * k + Q * (k & 1 ? 1 : -1))
                                  * std::sin(TAU * ((1 + (k % 3)) * x + (4 - (k % 2)) * y) / N + 0.4f * k - Q);
                float dash = std::min(1.0f, std::max(0.0f, (m - 0.52f) * 3.2f));   // broken into wavelets that come and go
                a = std::max(a, ridge * dash * WT[k]);
            }
            u32 h = (u32)(x * 73856093u) ^ (u32)(y * 19349663u) ^ (u32)(f * 83492791u); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
            if (a > 0.55f && (h & 255) < 6) a = 1.0f;   // glints
            c.px[y * N + x] = Canvas::pack(Color{206, 232, 248, (u8)(clampf(a, 0, 1) * 62)});
        }
        seaSwell[f] = fromCanvasSolo(c, 0, 0, true, SDL_BLENDMODE_ADD);
    }
}

// ================================================================= particles
void Game::noteFrameCost(float ms) {
    frameCost = frameCost <= 0 ? ms : frameCost * 0.9f + ms * 0.1f;
    // down a step soon after frames run slower than ~33 fps, up a step only after a while well inside a 60 fps frame (a vsync wait counts as cost,
    // so a GPU at 60 Hz sits at about 17 ms and stays where it is)
    if (frameCost > 30.0f && detail > 0 && wallTime - detailAt > 1.0f) { detail--; detailAt = wallTime; frameCost = 22.0f; }
    else if (frameCost < 13.0f && detail < 3 && wallTime - detailAt > 5.0f) { detail++; detailAt = wallTime; }
}

void Game::emitP(float x, float y, float vx, float vy, float life, float s0, float s1, Color col, u8 kind, u8 var, float ay, float drag, float rot, float vrot) {
    static const size_t CAP[4] = { 160, 360, 620, 900 };
    static const float KEEP[4] = { 0.3f, 0.5f, 0.75f, 1.0f };   // (lower detail: a share of every burst, so each effect still shows)
    if (parts.size() >= CAP[detail] || (detail < 3 && fxRng.f() > KEEP[detail])) return;
    parts.push_back(Particle{ x, y, vx, vy, ay, drag, 0, life, s0, s1, rot, vrot, col, kind, var });
}

static constexpr float WIND_X = 15.0f;   // the wind that carries smoke (px/s, screen x): smoke puffs relax towards it, so columns lean and drift

void Game::updateParticles(float dt) {
    if (dt <= 0) return;
    nukeFlash = std::max(0.0f, nukeFlash - dt);
    const float dq = (detail + 1) * 0.25f;   // share of the cosmetic emissions at this detail level (all of them at the top)
    for (auto& p : parts) {
        p.age += dt;
        if (p.kind == PK_SMOKE) p.vx += (WIND_X - p.vx) * std::min(1.0f, 0.9f * dt);
        p.x += p.vx * dt; p.y += p.vy * dt; p.vy += p.ay * dt; p.rot += p.vrot * dt;
        if (p.drag > 0) { float k = std::exp(-p.drag * dt); p.vx *= k; p.vy *= k; }
    }
    // a chunk of flying debris trails a little smoke for its first moments (the new puffs are appended, so only the old ones are looked at)
    const size_t n0 = parts.size();
    for (size_t i = 0; i < n0; i++) {
        if (parts[i].kind != PK_DEBRIS || parts[i].age > 0.3f || fxRng.f() >= 2.5f * dq * dt) continue;
        const Particle d = parts[i];
        emitP(d.x, d.y, fxRng.f(-6, 6), fxRng.f(-10, -2), fxRng.f(0.9f, 1.4f), 2.6f, 6.0f, rgb(94, 90, 86, 150), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), fxRng.f(-0.2f, 0.2f));
    }
    for (size_t i = 0; i < parts.size();) { if (parts[i].age >= parts[i].life) { parts[i] = parts.back(); parts.pop_back(); } else i++; }
    // the smoke columns of big blasts: puffs off the spot for a few seconds, thinning out at the end
    for (auto& pl : plumes) {
        pl.t += dt;
        const float fade = clampf((pl.life - pl.t) / 1.5f, 0, 1);
        pl.acc += pl.rate * dq * fade * dt;
        for (; pl.acc >= 1.0f; pl.acc -= 1.0f) {
            const float s = pl.size;
            emitP(pl.at.x + fxRng.f(-s * 0.3f, s * 0.3f), pl.at.y + fxRng.f(-s * 0.15f, s * 0.15f), fxRng.f(-5, 5), fxRng.f(-36, -22), fxRng.f(4.0f, 6.5f), s * 0.45f, s * 1.1f,
                  pl.nuke ? rgb(100, 90, 84, 210) : rgb(88, 84, 80, 185), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), fxRng.f(-0.15f, 0.15f));
        }
    }
    plumes.erase(std::remove_if(plumes.begin(), plumes.end(), [](const Game::Plume& p) { return p.t >= p.life; }), plumes.end());
    for (auto& d : decals) d.age += dt;
    for (auto& t : treads) t.age += dt;
    decals.erase(std::remove_if(decals.begin(), decals.end(), [](const Decal& d) { return d.age >= d.life; }), decals.end());
}

// First sight of a sim effect: add the cosmetic bits that make blasts read as blasts (flying chunks, sparks, a smoke plume, a scorch mark).
void Game::spawnFromFx() {
    const float dq = (detail + 1) * 0.25f;   // share of the cosmetic emissions at this detail level
    for (auto& f : g_sim.fx) {
        if (f.seen || f.t < 0) continue;
        f.seen = true;
        if (!onScreen(f.a.x, f.a.y, 120 + (f.type == FX_EXPLODE ? f.size * 1.5f : 0.0f))) continue;
        Rng& r = fxRng;
        switch (f.type) {
        case FX_EXPLODE: {
            const float sc = f.size;
            const bool light = f.vel.y < -0.5f;   // one bomb of an Air Strike's carpet: a fraction of the smoke and scorch (a hundred of them land on one spot)
            float s = std::min(sc, 520.0f);   // (an Underground Nuke's fireball is wider than a ramp's, its smoke and chunks are not: their overdraw grows with the square)
            int chunks = 2 + (int)(s / 9), sparks = 4 + (int)(s / 3.5f);
            for (int i = 0; i < chunks; i++) { float an = r.f(0, 6.283f), sp = r.f(50, 70 + s * 3.2f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.7f - r.f(60, 150), r.f(0.5f, 1.0f), r.f(0.7f, 1.2f), r.f(0.7f, 1.2f), shade(rgb(96, 90, 82), r.f(0.6f, 1.2f)), PK_DEBRIS, (u8)r.range(0, 3), 420, 0, r.f(0, 6), r.f(-14, 14)); }
            for (int i = 0; i < sparks; i++) { float an = r.f(0, 6.283f), sp = r.f(80, 140 + s * 4.0f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp, r.f(0.25f, 0.6f), r.f(1.0f, 1.8f), 0.4f, rgb(255, 214, 120), PK_SPARK, 0, 120, 3.0f); }
            int puffs = light ? (r.f() < 0.4f ? 1 : 0) : 1 + (int)(s / 12);
            for (int i = 0; i < puffs; i++) emitP(f.a.x + r.f(-s * 0.3f, s * 0.3f), f.a.y + r.f(-s * 0.3f, s * 0.3f), r.f(-10, 10), r.f(-30, -12), r.f(1.8f, 3.2f), s * r.f(0.35f, 0.55f), s * r.f(0.9f, 1.4f), rgb(52, 50, 48), PK_SMOKE, (u8)r.range(0, 3), 0, 0.6f, r.f(0, 6), r.f(-0.4f, 0.4f));
            if (s >= 20) emitP(f.a.x, f.a.y, 0, 0, 0.6f, s * 0.35f, s * 2.2f, rgb(255, 196, 130), PK_RING);
            if (!light && s >= 8 && s < 25) for (int i = 0, nd = (int)(2 * dq + 0.5f); i < nd; i++) {   // a small impact: a puff of dust rolls out along the ground
                float an = r.f(0, 6.283f), sp = r.f(40, 72);
                emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.45f, r.f(0.45f, 0.8f), s * 0.22f, s * 0.6f, rgb(168, 152, 128, 150), PK_DUST, (u8)r.range(0, 2), 0, 2.2f, r.f(0, 6), r.f(-0.5f, 0.5f));
            }
            if (!light && s >= 60) for (int i = 0, ne = (int)((1 + s / 40.0f) * dq + 0.5f); i < ne; i++) {   // embers: hot specks thrown up that hang, drift down and cool
                emitP(f.a.x + r.f(-s * 0.2f, s * 0.2f), f.a.y + r.f(-s * 0.2f, s * 0.2f), r.f(-45, 45), r.f(-130, -55), r.f(1.0f, 1.9f), 4.6f, 1.4f, rgb(255, 170, 86), PK_GLOW, (u8)r.range(0, 3), 26, 1.5f);
            }
            if (!light && s >= 55 && plumes.size() < (s >= 150 ? 12u : 6u)) plumes.push_back(Plume{ f.a, 0.0f, s >= 150 ? 6.0f : 4.5f, s >= 150 ? 3.5f : 3.0f, 0.0f, std::min(s * 0.22f, 40.0f), s >= 150 });   // (puffs no bigger than 1.1x this: a super blast's column stays light to draw)
            if (s >= 55 && s < 150) {   // heavy bomb: a rolling dust ring, a fire column and a plume of smoke climbing from the crater
                for (int i = 0; i < 12; i++) { float an = i * 0.5236f + r.f(-0.15f, 0.15f), sp = r.f(110, 190) * (s / 70.0f + 0.4f); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.5f, r.f(0.7f, 1.2f), s * 0.12f, s * 0.42f, rgb(150, 132, 108, 170), PK_DUST, (u8)r.range(0, 2), 0, 3.4f, r.f(0, 6), r.f(-0.5f, 0.5f)); }
                for (int i = 0; i < 7; i++) emitP(f.a.x + r.f(-s * 0.15f, s * 0.15f), f.a.y - i * s * 0.1f, r.f(-8, 8), -r.f(70, 150), r.f(0.5f, 0.9f), s * r.f(0.2f, 0.34f), s * 0.08f, rgb(255, 170 + r.range(0, 60), 80), PK_FLAME, (u8)r.range(0, 2), -40, 1.2f);
                for (int i = 0; i < (light ? 1 : 5); i++) emitP(f.a.x + r.f(-s * 0.2f, s * 0.2f), f.a.y - r.f(0, s * 0.3f), r.f(-10, 10), -r.f(36, 90), r.f(2.2f, 3.6f), s * r.f(0.28f, 0.4f), s * r.f(0.8f, 1.15f), light ? rgb(110, 104, 96, 110) : rgb(70, 66, 62, 150), PK_SMOKE, (u8)r.range(0, 3), 0, 0.5f, r.f(0, 6), r.f(-0.3f, 0.3f));
                for (int i = 0; i < 16; i++) { float an = r.f(0, 6.283f), sp = r.f(160, 300); emitP(f.a.x, f.a.y, std::cos(an) * sp, std::sin(an) * sp * 0.8f - r.f(50, 140), r.f(0.3f, 0.7f), r.f(1.2f, 2.2f), 0.4f, rgb(255, 224, 150), PK_SPARK, 0, 160, 2.6f); }
            }
            if (s >= 150) {   // tactical nuke: a rising stem, an opening cap, a dust ring, a huge scorch and a flash
                for (int i = 0; i < 16; i++) emitP(f.a.x + r.f(-s * 0.1f, s * 0.1f), f.a.y + r.f(-s * 0.1f, s * 0.1f), r.f(-6, 6), -r.f(40, 100), r.f(5.0f, 7.5f), s * 0.16f, s * 0.38f, rgb(86, 76, 68, 215), PK_SMOKE, (u8)r.range(0, 3), 0, 0.3f, r.f(0, 6), r.f(-0.2f, 0.2f));
                for (int i = 0; i < 14; i++) { float an = i * 0.4488f + r.f(-0.1f, 0.1f), sp = r.f(30, 60); emitP(f.a.x, f.a.y - s * 0.3f, std::cos(an) * sp, std::sin(an) * sp * 0.55f - 22, r.f(5.0f, 7.0f), s * 0.2f, s * 0.46f, rgb(96, 82, 70, 200), PK_SMOKE, (u8)r.range(0, 3), 0, 0.5f, r.f(0, 6), r.f(-0.3f, 0.3f)); }
                emitP(f.a.x, f.a.y, 0, 0, 1.6f, s * 0.2f, s * 1.9f, rgb(255, 214, 150), PK_RING);
                emitP(f.a.x, f.a.y, 0, 0, 2.4f, s * 0.2f, s * 2.6f, rgb(255, 170, 100), PK_RING);
                decals.push_back(Decal{ f.a.x, f.a.y, sc * 1.5f, 0.0f, 0, 90.0f, 1 });   // (not rotated: a rotated blit this big costs a quarter of a second a frame on the software renderer)
                nukeFlash = 0.55f;
            }
            if (s >= 15 && s < 150 && decals.size() < 56 && (!light || r.f() < 0.25f)) decals.push_back(Decal{ f.a.x, f.a.y, s * (light ? r.f(0.4f, 0.55f) : r.f(1.0f, 1.25f)), r.f(0, 6.28f), 0, r.f(32, 46), (u8)r.range(0, 2) });   // (a nuke has its own scorch above; a carpet bomb's is small, or a hundred of them blacken the spot)
            break;
        }
        case FX_WRECK: if (f.vel.y > 0.5f && f.vel.y < 1.5f) for (int i = 0; i < 2; i++) emitP(f.a.x + r.f(-3, 3), f.a.y + r.f(-3, 3), r.f(-10, 10), r.f(-12, 4), r.f(0.5f, 0.8f), 3, 9, rgb(150, 126, 100, 150), PK_DUST, (u8)r.range(0, 2), 0, 1.6f); break;
        case FX_BEAM: case FX_RAIL: {
            int n = f.type == FX_RAIL ? 5 : 2;
            for (int i = 0; i < n; i++) { float an = r.f(0, 6.283f), sp = r.f(40, 130); emitP(f.b.x, f.b.y, std::cos(an) * sp, std::sin(an) * sp, r.f(0.15f, 0.35f), r.f(0.9f, 1.4f), 0.3f, f.color, PK_SPARK, 0, 60, 4.0f); }
            break;
        }
        case FX_FLASH:   // a bigger gun's muzzle flash leaves a wisp of smoke hanging in front of it
            if (f.size >= 6 && r.f() < 0.35f * dq) emitP(f.a.x, f.a.y, r.f(-4, 4), r.f(-14, -6), r.f(0.5f, 0.8f), f.size * 0.5f, f.size * 1.0f, rgb(226, 222, 214, 150), PK_SMOKE, (u8)r.range(0, 3), 0, 1.4f, r.f(0, 6), r.f(-0.2f, 0.2f));
            break;
        default: break;
        }
    }
}

// ================================================================= drawing
void Game::drawGroundFx() {
    Gfx& g = g_gfx;
    const float dq = (detail + 1) * 0.25f;   // share of the cosmetic emissions at this detail level
    int skirts = 0;                          // dust rings drawn this frame: a few at most, a nuclear exchange has dozens of blasts in view
    const int skirtMax = detail >= 3 ? (g.softwareRenderer ? 2 : 6) : (detail == 2 ? (g.softwareRenderer ? 1 : 3) : 0);
    static const int DECALS[4] = { 0, 14, 30, 1 << 20 };   // (lower detail: only the newest scorch marks)
    for (size_t i = decals.size() > (size_t)DECALS[detail] ? decals.size() - DECALS[detail] : 0; i < decals.size(); i++) {
        const Decal& d = decals[i];
        if (!onScreen(d.x, d.y, d.r + 8)) continue;
        float k = d.age / d.life, a = 255 * clampf((1 - k) * 4.0f, 0, 1) * 0.85f;
        Vec2 s = worldToScreen(Vec2(d.x, d.y));
        g.draw(g.fxs.scorch[d.kind % 3], s.x, s.y, g.softwareRenderer ? 0.0f : d.rot, d.r * 2.0f / 96.0f, rgb(255, 255, 255), (u8)a);   // (a round splotch: the CPU renderer lays it upright)
    }
    for (auto& t : treads) {
        if (t.age >= t.life || !onScreen(t.x, t.y, 16)) continue;
        float k = t.age / t.life, a = 255 * clampf((1 - k) * 2.5f, 0, 1) * clampf(t.age * 6.0f + 0.3f, 0, 1);
        Vec2 s = worldToScreen(Vec2(t.x, t.y));
        g.draw(g.fxs.tread[t.kind & 1], s.x, s.y, t.rot, 0.55f, rgb(255, 255, 255), (u8)a);
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
                int n = (3 + w * h / 3) * detail / 3;
                for (int i = 0; i < n; i++) {
                    float x = s.x + r.f(-0.4f, 0.4f) * w * TILE, y = s.y + r.f(-0.4f, 0.4f) * h * TILE;
                    float fl = 0.5f + 0.5f * std::sin(wallTime * (5.0f + i) + i * 2.3f), fade = 1.0f - f.t / 18.0f;
                    g.glowAdd(x, y, 8 + 3 * fl, Color{255, 110, 40, (u8)(110 * fl * fade)});
                }
                if (frameDt > 0 && fxRng.f() < 1.4f * frameDt && f.t < 14.0f) emitP(f.a.x + fxRng.f(-0.35f, 0.35f) * w * TILE, f.a.y + fxRng.f(-0.35f, 0.35f) * h * TILE, fxRng.f(-4, 4), fxRng.f(-22, -12), fxRng.f(1.8f, 3.0f), 6, 16, rgb(70, 66, 62), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.2f);
            }
        } else if (f.type == FX_EXPLODE) {   // a mid-size blast: a low ring of dust rolls out along the ground (a nuke's surge is drawn with its fireball)
            if (f.t < 0 || k > 0.8f || f.size < 50 || f.size >= 160 || f.vel.y < -0.5f || skirts >= skirtMax || !onScreen(f.a.x, f.a.y, f.size * 1.2f)) continue;
            Vec2 a = worldToScreen(f.a);
            const float Rk = f.size * (0.5f + 1.1f * std::sqrt(k));   // races out fast, then slows as the dust settles
            g.drawSized(g.fxs.skirt[(int)std::fabs(f.a.x) & 1], a.x, a.y, Rk * 2.1f, Rk * 1.05f, 0, rgb(255, 255, 255), (u8)(200 * clampf((0.8f - k) * 3.0f, 0, 1)));
            skirts++;
        } else if (f.type == FX_WRECK && f.vel.y > 1.5f) {   // a ship going down: the hull settles, tilts and fades into a spreading oil slick and foam
            if (!onScreen(f.a.x, f.a.y, 90)) continue;
            Vec2 a = worldToScreen(f.a);
            int ut = (int)f.b.x, ow = slotOf((int)f.b.y);
            float sink = clampf(k * 2.2f, 0, 1), al = clampf((1 - k) * 1.8f, 0, 1);
            float L = 38.0f * UNIT_SCALE * artScale(ut);
            g.drawSized(g.shadowLarge, a.x + 3, a.y + 4, L * (1.6f + 1.4f * k), L * (0.9f + 0.9f * k), f.vel.x, rgb(10, 10, 12), (u8)(110 * al));   // the oil
            g.drawSized(g.fxs.ring, a.x, a.y, L * (1.6f + 2.2f * k), L * (0.8f + 1.1f * k), f.vel.x, rgb(236, 246, 252), (u8)(120 * al * (1 - sink * 0.5f)));
            Color hull = shade(rgb(80, 74, 70), 1.0f - 0.45f * sink);
            g.draw(g.unitBody[ut][ow], a.x, a.y + 3 * sink, f.vel.x + 0.12f * sink, 1.0f - 0.07f * sink, hull, (u8)(255 * al));
            if (f.t < 5.0f) {
                float fade = 1 - f.t / 5.0f; int fr = ((int)(wallTime * 12) + (int)f.a.x) % 6;
                g.draw(g.fxs.flame[fr], a.x - 6, a.y, 0, 0.6f, rgb(255, 255, 255), (u8)(200 * fade));
                g.draw(g.fxs.flame[(fr + 2) % 6], a.x + 8, a.y + 3, 0, 0.5f, rgb(255, 255, 255), (u8)(180 * fade));
                g.glowAdd(a.x, a.y, 24 + 6 * std::sin(wallTime * 9 + f.a.x), Color{255, 130, 50, (u8)(90 * fade)});
                if (frameDt > 0 && fxRng.f() < 6.0f * frameDt) emitP(f.a.x + fxRng.f(-14, 14), f.a.y + fxRng.f(-8, 8), fxRng.f(-6, 6), fxRng.f(-26, -12), fxRng.f(1.4f, 2.6f), 6, 18, rgb(52, 50, 48, 210), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.6f, fxRng.f(0, 6), 0.2f);
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
                g.draw(g.fxs.flame[(fr + 3) % 6], a.x + 7, a.y + 1, 0, 0.42f + 0.1f * std::sin(wallTime * 17 + f.a.y), rgb(255, 255, 255), (u8)(170 * fade));   // a second tongue on another beat
                g.glowAdd(a.x, a.y, 14 + 4 * std::sin(wallTime * 9 + f.a.x), Color{255, 120, 40, (u8)(80 * fade)});
            }
            if (f.t < 7.0f && frameDt > 0 && fxRng.f() < 0.9f * dq * frameDt)   // a thin column of smoke while it burns
                emitP(f.a.x + fxRng.f(-5, 5), f.a.y - 4 + fxRng.f(-4, 4), fxRng.f(-4, 4), fxRng.f(-24, -14), fxRng.f(2.0f, 3.2f), 4.5f, 11.0f, rgb(58, 54, 50, 170), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.15f);
        }
    }
}

void Game::drawFx() {
    Gfx& g = g_gfx;
    // this frame's white cores and first-light stars, capped: a nuclear exchange has ninety blasts in view, each would add its own big additive draw
    int cores = 0, flashes = 0, mushrooms = 0;   // (mushrooms: the clouds drawn this frame; past two, the rest draw every second or third billow)
    const int coreMax = detail >= 3 ? 8 : (detail == 2 ? 4 : 0), flashMax = detail >= 3 ? 10 : (detail == 2 ? 5 : 2);
    static const bool fillDbg = getenv("ONEHOUR_FILLDBG") != nullptr;
    static double byType[64]; int lastType = -1; double mark = g.fillPx;
    if (fillDbg) for (auto& v : byType) v = 0;
    for (auto& f : g_sim.fx) {
        if (fillDbg) { if (lastType >= 0) byType[lastType & 63] += g.fillPx - mark; mark = g.fillPx; lastType = f.type; }
        if (f.t < 0 || f.type == FX_RUBBLE || f.type == FX_WRECK) continue;
        float k = f.t / f.life;
        Vec2 a = worldToScreen(f.a), b = worldToScreen(f.b);
        if ((f.type == FX_BEAM || f.type == FX_RAIL || f.type == FX_ARC || f.type == FX_MUSHROOM || f.type == FX_FALLOUT) ? false : (a.x < -80 || a.y < -80 || a.x > SCREEN_W + 80 || a.y > VIEW_H + 80)) continue;
        switch (f.type) {
        case FX_BEAM: {   // textured bolt: a soft halo, a glow, a bright core and a hot white filament; the power crackles and a packet of light runs along it
            float len = hyp(b.x - a.x, b.y - a.y); if (len < 1) break;
            float ang = std::atan2(b.y - a.y, b.x - a.x);
            float crackle = 0.9f + 0.1f * hh((int)(wallTime * 40) + (int)f.a.x, (int)f.a.y, 77);   // the power jumps a little every frame or so
            float al = (1 - k) * (0.86f + 0.14f * std::sin(wallTime * 90.0f + f.a.x * 0.7f)) * crackle;   // mains flicker
            Vec2 mid = (a + b) * 0.5f;
            if (!g.softwareRenderer) g.drawSized(g.fxs.beam, mid.x, mid.y, len, 22, ang, f.color, (u8)(60 * al));   // (its halo: a soft band, a few lines on the CPU renderer, which it skips)
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 12, ang, f.color, (u8)(150 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 5, ang, f.color, (u8)(255 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 2, ang, rgb(255, 255, 255), (u8)(255 * al));
            Vec2 pk = a + (b - a) * std::fmod(wallTime * 2.6f + f.a.x * 0.013f, 1.0f);   // a packet of light running from the gun to the target
            g.glowAdd(pk.x, pk.y, 4.5f, Color{f.color.r, f.color.g, f.color.b, (u8)(150 * al)});
            g.glowAdd(b.x, b.y, 11 * (1 - k * 0.4f), Color{f.color.r, f.color.g, f.color.b, (u8)(170 * al)});
            g.glowAdd(a.x, a.y, 7, Color{f.color.r, f.color.g, f.color.b, (u8)(120 * al)});
            break;
        }
        case FX_RAIL: {
            float len = hyp(b.x - a.x, b.y - a.y); if (len < 1) break;
            float ang = std::atan2(b.y - a.y, b.x - a.x), al = (1 - k) * (0.88f + 0.12f * hh((int)(wallTime * 30) + (int)f.a.x, (int)f.a.y, 83));   // (the rail hums: a little flicker)
            Vec2 mid = (a + b) * 0.5f;
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 26 * (1 - k * 0.5f), ang, rgb(110, 190, 255), (u8)(140 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 9, ang, f.color, (u8)(255 * al));
            g.drawSized(g.fxs.beam, mid.x, mid.y, len, 3, ang, rgb(255, 255, 255), (u8)(255 * al));
            for (int i = 1; i <= 4; i++) { Vec2 q = a + (b - a) * (i / 5.0f); g.drawSized(g.fxs.ring, q.x, q.y, 18 + 26 * k, 18 + 26 * k, 0, rgb(150, 210, 255), (u8)(120 * al)); }
            g.glowAdd(a.x, a.y, 16, Color{200, 230, 255, (u8)(200 * al)});
            break;
        }
        case FX_ARC: {
            Color c = f.color; float al = (1 - k) * (0.8f + 0.2f * hh((int)(f.t * 1000), (int)f.a.x, 91));
            Rng r((u64)(f.t * 1000) + (u64)f.a.x);
            if (!g.softwareRenderer) { Vec2 mid = (a + b) * 0.5f, dv = b - a; float dl = hyp(dv.x, dv.y); if (dl >= 1) g.drawSized(g.fxs.beam, mid.x, mid.y, dl, 14, std::atan2(dv.y, dv.x), c, (u8)(55 * al)); }   // the arc's soft halo (GPU only)
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
            for (int fork = 0; fork < 2; fork++) {   // side forks snapping off the main bolt
                float t0 = 0.3f + 0.25f * fork + r.f(-0.06f, 0.06f);
                Vec2 base = a + (b - a) * t0 + Vec2(r.f(-6, 6), r.f(-6, 6));
                Vec2 dv = b - a; float dl = hyp(dv.x, dv.y); if (dl < 1) break; dv = dv * (1.0f / dl);
                Vec2 side(-dv.y, dv.x); if (r.f() < 0.5f) side = side * -1.0f;
                Vec2 mid = base + side * r.f(6, 13) + dv * r.f(2, 9), tip = mid + side * r.f(4, 10) + dv * r.f(-2, 7);
                Vec2 pts[3] = { base, mid, tip };
                for (int sgm = 0; sgm < 2; sgm++) {
                    Vec2 d2 = pts[sgm + 1] - pts[sgm]; float l2 = hyp(d2.x, d2.y); if (l2 < 0.5f) continue;
                    Vec2 mm = (pts[sgm] + pts[sgm + 1]) * 0.5f; float a2 = std::atan2(d2.y, d2.x);
                    g.drawSized(g.fxs.beam, mm.x, mm.y, l2 + 2, 5.5f, a2, c, (u8)(85 * al));
                    g.drawSized(g.fxs.beam, mm.x, mm.y, l2 + 1, 1.8f, a2, rgb(255, 255, 255), (u8)(190 * al));
                }
                g.glowAdd(tip.x, tip.y, 5.5f, Color{c.r, c.g, c.b, (u8)(110 * al)});
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
            const float rot = std::fmod(f.a.x * 0.37f, 6.28f);
            if (detail >= 2 || rr < 40) g.glowAdd(a.x, a.y, rr * (2.3f + 0.8f * (1 - k)), Color{255, 140, 50, (u8)(150 * (1 - k))});
            g.drawSized(g.fxs.fire[fr], a.x, a.y, rr * 2.5f, rr * 2.5f, rot, rgb(255, 255, 255), (u8)(255 * al));   // the churning, darkening body
            if (k < 0.6f && rr < 150.0f && cores < coreMax) { cores++; g.drawSized(g.fxs.core[fr], a.x, a.y, rr * 1.7f, rr * 1.7f, 0, rgb(255, 255, 255), (u8)(255 * (1 - k / 0.6f))); }   // its white-hot heart
            if (k < 0.2f) g.glowAdd(a.x, a.y, rr * 1.4f, Color{255, 252, 220, (u8)(230 * (1 - k * 5))});
            if (f.t < 0.14f && rr > 10.0f && flashes < flashMax) { flashes++; g.drawSized(g.fxs.flash[(int)(std::fabs(f.a.x * 7 + f.a.y * 3)) % 3], a.x, a.y, rr * 2.4f, rr * 2.4f, rot, rgb(255, 250, 230), (u8)(255 * (1 - f.t / 0.14f))); }   // the first light: a crisp star
            break;
        }
        case FX_SMOKE: {
            int v = (int)(std::fabs(f.a.x * 3.1f + f.a.y * 1.7f)) & 3;
            float rad = f.size * (0.6f + k) * 1.15f, al = f.color.a / 255.0f * 0.9f;
            float fade = std::min(1.0f, f.t * 6.0f) * (1 - k);
            if (fade < (3 - detail) * 0.12f) break;   // (lower detail: the faintest smoke goes first)
            if (255 * 0.55f * fade < std::max(3.0f, smokeCut)) break;   // (past the smoke budget)
            smokeFill += 4.0f * rad * rad;
            g.drawSized(g.fxs.smoke[v], a.x, a.y, rad * 2, rad * 2, f.t * 0.3f + (float)v, Color{f.color.r, f.color.g, f.color.b, 255}, (u8)(255 * clampf(0.55f * fade * (f.color.r < 120 ? 1.15f : 1.0f), 0, 1) * (al > 0 ? 1 : 1)));
            break;
        }
        case FX_SPARK: {   // a hot spot with three quick streaks flung out in their own directions (a hash of the spot: they do not all point one way)
            float al = 1 - k;
            g.glowAdd(a.x, a.y, f.size * 2.6f, Color{f.color.r, f.color.g, f.color.b, (u8)(200 * al)});
            g.glowAdd(a.x, a.y, f.size * 1.0f, Color{255, 255, 255, (u8)(220 * al)});
            const u32 hs = (u32)(int)f.a.x * 73856093u ^ (u32)(int)f.a.y * 19349663u;
            for (int i = 0; i < 2; i++) {
                float an = (float)((hs >> (i * 8)) & 255) * (6.2831853f / 256.0f) + i * 3.1416f, L = f.size * (1.6f + 1.4f * (1 - k));
                g.drawSized(g.fxs.streak, a.x + std::cos(an) * L * 0.25f, a.y + std::sin(an) * L * 0.25f, L, std::max(1.2f, f.size * 0.4f), an, Color{f.color.r, f.color.g, f.color.b}, (u8)(230 * al));
            }
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
            if (a.x + 3.0f * R < 0 || a.x - 3.0f * R > SCREEN_W || a.y - 3.4f * R > VIEW_H || a.y + 1.8f * R < 0) break;   // (wholly off screen: the stem, the cap and the skirt)
            const int mi = mushrooms++;
            float g1 = clampf(t / 9.0f, 0, 1), grow = 1 - (1 - g1) * (1 - g1) * (1 - g1);
            float fade = clampf((1 - k) * 3.5f, 0, 1);
            float heat = clampf(1 - t / 7.5f, 0, 1);   // (the glows fade with this)
            const float hc = clampf(1 - t / 12.0f, 0, 1);   // the colour cools more slowly: white, then orange, then dark red-brown, before the smoke
            float H = R * 2.05f * grow, Rc = R * (0.2f + 0.78f * grow);
            Vec2 top(a.x, a.y - H);
            auto lerpc = [](Color x, Color y, float u) { u = clampf(u, 0, 1); return Color{(u8)(x.r + (y.r - x.r) * u), (u8)(x.g + (y.g - x.g) * u), (u8)(x.b + (y.b - x.b) * u), 255}; };
            const bool sv = f.color.r != 255 || f.color.g != 255 || f.color.b != 255;   // a tinted cloud: the silver of an Underground Nuke
            const float aged = clampf((t - 4.0f) / 9.0f, 0, 1);   // the smoke darkens as it climbs: from ash grey to dark red-brown
            const Color ash = sv ? Color{196, 200, 208, 255} : lerpC(Color{206, 188, 172, 255}, Color{104, 76, 66, 255}, aged * 0.8f);
            const Color soot = sv ? Color{122, 126, 134, 255} : lerpC(Color{118, 108, 102, 255}, Color{70, 52, 48, 255}, aged);
            const Color lit = sv ? Color{246, 248, 252, 255} : Color{255, 246, 232, 255};
            if (heat > 0) g.glowAdd(a.x, a.y, R * 1.25f * heat, sv ? Color{210, 218, 232, (u8)(160 * heat * fade)} : Color{255, 140, 50, (u8)(180 * heat * fade)});
            if (t < 0.35f) { float fl = 1 - t / 0.35f; g.drawSized(g.fxs.flash[0], a.x, a.y - H * 0.1f, R * 1.3f, R * 1.3f, 0, sv ? rgb(236, 242, 252) : rgb(255, 246, 228), (u8)(255 * fl * fl)); }   // the first flash
            // base surge: a low skirt of dust billows rolling outward along the ground, with a dust ring racing out ahead of them
            float sg = clampf(t / 10.0f, 0, 1); sg = 1 - (1 - sg) * (1 - sg);
            if (detail >= 1) { const float Rs = R * (0.3f + 1.05f * sg); g.drawSized(g.fxs.skirt[0], a.x, a.y + 3, Rs * 2.1f, Rs * 1.05f, 0, sv ? rgb(196, 202, 212) : rgb(190, 168, 140), (u8)(190 * fade * (1.0f - 0.5f * sg))); }
            const int st = std::max(detail >= 3 ? 1 : (detail == 2 ? 2 : 3), mi >= 4 ? 3 : (mi >= 2 ? 2 : 1));   // (lower detail, or a third cloud in view at once - a Hydra's five: every second or third billow, each a little bigger)
            const float sb = 1.0f + 0.22f * (st - 1);
            for (int i = 0; i < 16; i += st) {
                float an = i * 0.3927f + std::sin(i * 2.3f) * 0.12f, rr = R * (0.28f + 1.05f * sg) * (0.86f + 0.14f * std::sin(i * 3.1f + t * 0.4f));
                float rad = (R * (0.2f + 0.1f * sg) + 8) * sb;
                g.drawSized(g.fxs.billow[i & 3], a.x + std::cos(an) * rr, a.y + std::sin(an) * rr * 0.42f + 4, rad * 2.4f, rad * 1.6f, 0, lerpc(sv ? Color{160, 164, 172, 255} : Color{168, 146, 120, 255}, ash, sg), (u8)(150 * fade * (1.0f - 0.4f * sg)));
            }
            // stem: turbulent billows, fat at the foot, narrow in the middle, flaring into the cap; the lower part still glows
            const int NS = 30;
            for (int i = 0; i < NS; i += st) {
                float u = i / (float)(NS - 1);
                float prof = 1.45f - 0.95f * std::sin(u * 3.1416f * 0.62f) + 0.55f * u * u * u;   // foot wide, waist narrow, throat widening under the cap
                float wob = std::sin(t * 1.1f + i * 1.9f) * R * 0.05f * (0.5f + u) + std::sin(t * 0.5f + u * 5.0f) * R * 0.035f;
                float rad = (R * 0.15f * prof * (0.65f + 0.35f * grow) + 7) * sb;
                Vec2 p(a.x + wob, a.y - H * u * 0.97f);
                Color c = lerpc(lerpc(ash, soot, u * 0.4f), heatRamp(hc * (1 - u * 0.9f), sv), clampf(hc * (1.6f - u), 0, 1));   // (the foot is hottest, cooling up the stem)
                g.drawSized(g.fxs.billow[i & 3], p.x, p.y, rad * 2.5f, rad * 2.5f, 0, c, (u8)(235 * fade));
            }
            // condensation collar: a pale ring that opens around the stem and fades
            { float cu = clampf((t - 1.5f) / 2.5f, 0, 1), cf = cu * clampf(1 - (t - 4.0f) / 9.0f, 0, 1) * fade;
              if (cf > 0.02f) { Vec2 cp(a.x, a.y - H * 0.5f); float cr = Rc * (0.55f + 0.25f * cu);
                for (int i = 0; i < 14; i += st) { float an = i * 0.4488f + t * 0.08f; g.drawSized(g.fxs.billow[(i + 2) & 3], cp.x + std::cos(an) * cr, cp.y + std::sin(an) * cr * 0.28f, R * 0.5f, R * 0.4f, 0, Color{236, 232, 228, 255}, (u8)(120 * cf)); } } }
            // cap: two rolling tori of billows (an outer rolling lip and a smaller inner one), back half first, then the core, then the front half
            auto torus = [&](bool front) {
                for (int ring = 0; ring < 2; ring++) {
                    int n = ring ? 18 : 32;
                    for (int i = 0; i < n; i += st) {
                        float th = i * 6.2832f / n + t * (ring ? -0.07f : 0.05f) + ring * 0.4f, sn = std::sin(th);
                        if ((sn > 0) != front) continue;
                        float phi = i * 1.9f + t * (ring ? 1.5f : 1.0f);                         // the roll of the vortex ring
                        float rm = Rc * (ring ? 0.22f : 0.3f), rc = Rc * (ring ? 0.42f : 0.8f);
                        float radial = rc + rm * std::cos(phi);
                        Vec2 p(top.x + std::cos(th) * radial, top.y + sn * radial * 0.34f - rm * std::sin(phi) * 0.85f - Rc * (ring ? 0.12f : 0.0f));
                        float up = std::sin(phi) * 0.5f + 0.5f;                                // top of the roll catches the light, the underside glows
                        const float hl = clampf(hc * (0.55f + 0.45f * (1 - up)) * (sn > 0 ? 1.0f : 0.8f), 0, 1);   // the underside of the rolling cap is the hottest part
                        Color c = lerpc(lerpc(ash, lit, up * 0.8f), heatRamp(hl, sv), clampf(hl * 1.5f, 0, 1));
                        if (up > 0.5f) c = lerpc(c, soot, clampf((t - 8.0f) / 10.0f, 0, 0.6f));
                        float rad = (rm * 1.08f + 9) * sb;
                        g.drawSized(g.fxs.billow[(i + ring) & 3], p.x, p.y, rad * 2.5f, rad * 2.5f, 0, c, (u8)(255 * fade));
                    }
                }
            };
            torus(false);
            for (int i = 0; i < 10; i += st) {   // dense core of the cap
                float an = i * 0.628f + t * 0.2f; Vec2 p(top.x + std::cos(an) * Rc * 0.4f, top.y - Rc * 0.14f + std::sin(an) * Rc * 0.16f);
                g.drawSized(g.fxs.billow[i & 3], p.x, p.y, Rc * 1.15f, Rc * 0.9f, 0, lerpc(lerpc(ash, lit, 0.35f), heatRamp(hc * 1.1f, sv), clampf(hc * 1.4f, 0, 1)), (u8)(240 * fade));
            }
            torus(true);
            for (int i = 0; i < 9; i += st) {    // pale smooth hat riding on top of the cap
                float an = i * 0.698f + t * 0.1f; Vec2 p(top.x + std::cos(an) * Rc * 0.38f, top.y - Rc * 0.34f + std::sin(an) * Rc * 0.1f);
                g.drawSized(g.fxs.billow[(i + 3) & 3], p.x, p.y, Rc * 0.7f, Rc * 0.45f, 0, lit, (u8)(190 * fade * grow));
            }
            if (heat > 0) {
                g.glowAdd(top.x, top.y + Rc * 0.18f, Rc * 1.15f, sv ? Color{200, 210, 228, (u8)(180 * heat * fade)} : Color{255, 160, 70, (u8)(200 * heat * fade)});
                g.glowAdd(top.x, top.y + Rc * 0.05f, Rc * 0.62f, sv ? Color{240, 244, 252, (u8)(210 * heat * fade)} : Color{255, 238, 196, (u8)(210 * heat * fade)});
                g.glowAdd(a.x, a.y - H * 0.25f, R * 0.35f, sv ? Color{210, 216, 228, (u8)(120 * heat * fade)} : Color{255, 170, 80, (u8)(120 * heat * fade)});
            }
            break;
        }
        case FX_FALLOUT: {   // sickly green haze, a pulsing boundary and drifting motes
            float R = f.size, fade = f.t < 3.0f ? f.t / 3.0f : clampf((1 - k) * 8.0f, 0, 1);
            float pulse = 0.5f + 0.5f * std::sin(wallTime * 2.4f);
            float cx = a.x, cy = a.y;
            if (cx < -R - 40 || cy < -R - 40 || cx > SCREEN_W + R + 40 || cy > VIEW_H + R + 40) break;
            const bool sv = f.color.r == UNUKE_GLOW.r && f.color.g == UNUKE_GLOW.g && f.color.b == UNUKE_GLOW.b;   // an Underground Nuke's radiation glows silver
            const Color haze = sv ? Color{150, 158, 172, 255} : Color{70, 220, 40, 255}, edge = sv ? rgb(214, 220, 232) : rgb(120, 255, 80), mote = sv ? Color{224, 230, 242, 255} : Color{110, 255, 70, 255};
            if (detail >= 2) g.drawRadial(g.fxs.fallout[sv ? 1 : 0][R < 600 ? 1 : 0], cx, cy, R, R, 0, rgb(255, 255, 255), (u8)(255 * fade * (0.7f + 0.6f * pulse) / 1.3f));   // (haze and rings in one sprite)
            else {
                g.drawSized(g.fxs.ring, cx, cy, R * 2.0f, R * 2.0f, 0, edge, (u8)((90 + 90 * pulse) * fade));
                if (R < 600) g.drawSized(g.fxs.ring, cx, cy, R * 1.5f, R * 1.5f, 0, edge, (u8)(40 * fade));   // (an Underground Nuke's zone is too wide for the inner ring: it would cover the screen twice)
            }
            (void)haze;
            for (int i = 0, motes = detail >= 3 ? 26 : 6 + 5 * detail; i < motes; i++) {
                float h1 = std::fmod(i * 0.6180339f, 1.0f), h2 = std::fmod(i * 0.7548776f + 0.3f, 1.0f);
                float an = h1 * 6.2832f + wallTime * 0.05f * (1 + h2), rr = R * std::sqrt(h2) * 0.95f;
                float fl = 0.5f + 0.5f * std::sin(wallTime * (1.5f + h1 * 3) + i);
                g.glowAdd(cx + std::cos(an) * rr, cy + std::sin(an) * rr * 1.0f - fl * 6, 9 + 5 * fl, Color{mote.r, mote.g, mote.b, (u8)(90 * fl * fade)});
            }
            break;
        }
        default: break;
        }
    }
    if (fillDbg) {
        if (lastType >= 0) byType[lastType & 63] += g.fillPx - mark;
        double tot = 0; for (double v : byType) tot += v;
        if (tot > 4e6) { fprintf(stderr, "[fill] fx %.1f Mpx:", tot / 1e6); for (int i = 0; i < 64; i++) if (byType[i] > 2e5) fprintf(stderr, " type%d %.1f", i, byType[i] / 1e6); fprintf(stderr, "\n"); }
    }
}

void Game::drawParticles(int pass) {   // pass 0: smoke, dust and debris (under the fire); pass 1: sparks, flames, rings and glows (over it)
    Gfx& g = g_gfx;
    const Color amb = ambientTint();
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
                if (255 * al * 0.7f < std::max(3.0f, smokeCut * (sz > 12 ? 1.0f : 0.5f))) break;   // (too faint to see, or past the smoke budget: see smokeCut)
                smokeFill += 4.0f * sz * sz;
                const Sprite& sp = p.kind == PK_DUST ? g.fxs.dust[p.var % 3] : g.fxs.smoke[p.var & 3];
                const float dk = p.kind == PK_SMOKE ? 1.0f - 0.5f * sst(0.1f, 1.0f, k) : 1.0f;   // smoke darkens as it climbs: the grey of a fresh puff goes to charcoal
                g.drawSized(sp, s.x, s.y, sz * 2, sz * 2, p.rot, Color{(u8)(p.col.r * dk * amb.r / 255), (u8)(p.col.g * dk * amb.g / 255), (u8)(p.col.b * dk * amb.b / 255), 255}, (u8)(255 * al * 0.7f));   // (smoke takes the hour's light)
                break;
            }
            case PK_DEBRIS: {
                float al = clampf((1 - k) * 3.0f, 0, 1);
                g.drawSized(g.fxs.debris[p.var & 3], s.x, s.y, p.s0 * 7, p.s0 * 7, p.rot, Color{(u8)(p.col.r * amb.r / 255), (u8)(p.col.g * amb.g / 255), (u8)(p.col.b * amb.b / 255), p.col.a}, (u8)(255 * al));
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
            case PK_GLOW: {   // an ember: it flickers as it cools
                float fl = 0.65f + 0.35f * std::sin(p.age * 29.0f + p.var * 2.1f);
                g.glowAdd(s.x, s.y, lerpf(p.s0, p.s1, k), Color{p.col.r, p.col.g, p.col.b, (u8)(p.col.a * (1 - k) * fl)});
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
    for (int i = 0; i < 11; i++) {
        float bx = 311.0f * (i * 7 % 11) + 190.0f * i, by = 270.0f * (i * 5 % 11) + 130.0f * (i % 3) + (WORLD_H - 2560.0f) * (i % 4) * 0.22f;
        float x = std::fmod(bx + wallTime * (7.0f + (i % 3) * 2.0f), span) - 900.0f, y = by + std::sin(wallTime * 0.04f + i * 1.7f) * 90.0f;
        float w = 760.0f + 160.0f * (i % 4), h = w * (0.62f + 0.06f * (i % 3));
        Vec2 s = worldToScreen(Vec2(x, y));
        if (s.x + w * 0.5f < 0 || s.x - w * 0.5f > SCREEN_W || s.y + h * 0.5f < 0 || s.y - h * 0.5f > VIEW_H) continue;
        g.drawSized(g.fxs.cloud[i % 3], s.x, s.y, w, h, 0, rgb(0, 6, 12), 50);
    }
}
