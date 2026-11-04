// One Hour - the living water over the baked sea and lake: surf that rolls in and breaks along every shore, the wash at the waterline, a sea
// that darkens and breaks into whitecaps in a storm, sun glitter on open water, the swell drifting with the wind, and forked lightning.
//
// The baked world holds the water's colour; what moves is drawn here. The surf and the storm sea are worked out on the CPU every frame at a
// quarter of the screen's resolution (one streaming texture, stretched with bilinear filtering over the view, so it costs one draw) from a
// field baked once per match: for every 8 px of the world, how far it lies out from the shore. The field is read off the baked ground itself
// (blue-dominant pixels are water), so the foam lines up with the painted shoreline, warps and all. Glitter and lightning are a handful of
// additive draws.
#include "ui.h"
#include "terrain.h"
#include <SDL2/SDL.h>

namespace {
constexpr int FQ = 8;                       // world px per field texel
constexpr float SURF_REACH = 170.0f;        // px out from the shore the rollers show
constexpr float FIELD_OFF = 40.0f;          // the field stores px + this (land down to -40 px, water out to 215 px)
struct Field {
    int w = 0, h = 0;
    std::vector<u8> d;                      // signed distance from the painted shore, px + FIELD_OFF (below FIELD_OFF: land)
    u32 stamp = 0;
} F;

float hsh(u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return (a & 0xFFFFFF) / 16777215.0f; }

// a small tileable value-noise table, sampled bilinearly (64 cells repeat)
struct NoiseTab {
    float v[64 * 64];
    NoiseTab() { for (int i = 0; i < 64 * 64; i++) v[i] = hsh((u32)i * 2654435761u + 977u); }
    float at(float x, float y) const {
        float fx = std::floor(x), fy = std::floor(y); int ix = (int)fx, iy = (int)fy; float tx = x - fx, ty = y - fy;
        tx = tx * tx * (3 - 2 * tx); ty = ty * ty * (3 - 2 * ty);
        auto g = [&](int a, int b) { return v[((b & 63) << 6) | (a & 63)]; };
        float a = g(ix, iy) + (g(ix + 1, iy) - g(ix, iy)) * tx, b = g(ix, iy + 1) + (g(ix + 1, iy + 1) - g(ix, iy + 1)) * tx;
        return a + (b - a) * ty;
    }
};
const NoiseTab& NT() { static NoiseTab t; return t; }

void buildField(const std::vector<u8>& cover, int cw, int ch) {
    F.w = cw; F.h = ch;
    // chamfer distances (texels) on both sides of the painted shoreline: out over the water to the nearest dry texel, and in over the land to the
    // nearest wet one, so the field is a signed distance whose zero runs between texels exactly where the paint changes
    auto chamfer = [&](bool water) {
        std::vector<float> dist((size_t)F.w * F.h);
        for (size_t i = 0; i < dist.size(); i++) dist[i] = ((cover[i] >= 128) == water) ? 1e6f : 0.0f;
        const float D1 = 1.0f, D2 = 1.4142f;
        for (int y = 0; y < F.h; y++) for (int x = 0; x < F.w; x++) {
            float& v = dist[(size_t)y * F.w + x];
            if (v == 0) continue;
            if (x > 0) v = std::min(v, dist[(size_t)y * F.w + x - 1] + D1);
            if (y > 0) { v = std::min(v, dist[(size_t)(y - 1) * F.w + x] + D1); if (x > 0) v = std::min(v, dist[(size_t)(y - 1) * F.w + x - 1] + D2); if (x < F.w - 1) v = std::min(v, dist[(size_t)(y - 1) * F.w + x + 1] + D2); }
        }
        for (int y = F.h - 1; y >= 0; y--) for (int x = F.w - 1; x >= 0; x--) {
            float& v = dist[(size_t)y * F.w + x];
            if (v == 0) continue;
            if (x < F.w - 1) v = std::min(v, dist[(size_t)y * F.w + x + 1] + D1);
            if (y < F.h - 1) { v = std::min(v, dist[(size_t)(y + 1) * F.w + x] + D1); if (x < F.w - 1) v = std::min(v, dist[(size_t)(y + 1) * F.w + x + 1] + D2); if (x > 0) v = std::min(v, dist[(size_t)(y + 1) * F.w + x - 1] + D2); }
        }
        return dist;
    };
    std::vector<float> dw = chamfer(true), dl = chamfer(false);
    F.d.assign((size_t)F.w * F.h, 0);
    for (size_t i = 0; i < F.d.size(); i++) {
        float px = dw[i] > 0 ? (dw[i] - 0.5f) * FQ : -(dl[i] - 0.5f) * FQ;
        if ((dw[i] > 0 && dw[i] < 1.5f) || (dl[i] > 0 && dl[i] < 1.5f)) px = (cover[i] - 127.5f) / 127.5f * FQ * 0.75f;   // right at the shore: from the coverage itself
        F.d[i] = (u8)clampf(px + FIELD_OFF + 0.5f, 0, 255);
    }
}

// distance out from the shore at a world point, px (0 on land); bilinear between field texels
inline float shoreDist(float wx, float wy) {
    float fx = wx / FQ - 0.5f, fy = wy / FQ - 0.5f;
    int ix = (int)std::floor(fx), iy = (int)std::floor(fy); float tx = fx - ix, ty = fy - iy;
    auto g = [&](int x, int y) -> float { if (x < 0 || y < 0 || x >= F.w || y >= F.h) return 255.0f; return F.d[(size_t)y * F.w + x]; };
    float a = g(ix, iy), b = g(ix + 1, iy), c = g(ix, iy + 1), d = g(ix + 1, iy + 1);
    float v = (a + (b - a) * tx) + ((c + (d - c) * tx) - (a + (b - a) * tx)) * ty;
    return std::max(0.0f, v - FIELD_OFF);
}
}

// the world's water got a new bake (a new match on another map, or the first frame): the field follows it
void Game::waterFieldUpdate() {
    int cw, ch; u32 stamp;
    const std::vector<u8>& cover = terrainWaterCover(cw, ch, stamp);
    if (cover.empty() || (!F.d.empty() && stamp == F.stamp)) return;
    buildField(cover, cw, ch);
    F.stamp = stamp;
    if (const char* dbg = getenv("ONEHOUR_WATERDBG")) {   // (debug: the field as a greyscale image)
        FILE* f = fopen(dbg, "wb");
        if (f) { fprintf(f, "P5 %d %d 255\n", F.w, F.h); fwrite(F.d.data(), 1, F.d.size(), f); fclose(f); }
    }
}

float Game::stormLevel() const { return clampf((weather.rain - 0.55f) / 0.4f, 0.0f, 1.0f); }

// The surf, the wash at the waterline and the storm sea, as one quarter-resolution overlay over the view
void Game::drawWaterOverlay() {
    Gfx& g = g_gfx;
    waterFieldUpdate();
    if (F.d.empty()) return;
    static SDL_Texture* tex = nullptr; static int tw = 0, th = 0;
    static std::vector<u32> px;
    // screen px per overlay texel: a GPU stretches it smoothly (bilinear), so a quarter resolution is plenty; the software renderer stretches
    // nearest-neighbour, so it gets half resolution to keep the foam from looking blocky
    const int OV = g.softwareRenderer ? 2 : 4;
    const int W = SCREEN_W / OV + 2, H = VIEW_H / OV + 2;
    if (!tex || tw != W || th != H) {
        if (tex) SDL_DestroyTexture(tex);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
        tex = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, W, H);
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
        if (!tex) return;
        SDL_SetTextureBlendMode(tex, SDL_BLENDMODE_BLEND);
        tw = W; th = H; px.assign((size_t)W * H, 0);
    }
    // the water moves slowly: worked out at most 30 times a second (15 on a struggling machine) while the view stands still; a scroll redoes it
    static float lastT = -1; static Vec2 lastCam(-1, -1); static bool lastAny = false;
    const float every = detail >= 2 ? 1.0f / 30.0f : 1.0f / 15.0f;
    const int gx0 = (int)std::floor(cam.x / OV), gy0 = (int)std::floor(cam.y / OV);   // texels on a world-aligned grid: nothing shimmers as the view scrolls
    if (lastT >= 0 && wallTime >= lastT && wallTime - lastT < every && (int)std::floor(lastCam.x / OV) == gx0 && (int)std::floor(lastCam.y / OV) == gy0) {
        if (lastAny) { SDL_FRect dst = { gx0 * (float)OV - cam.x, gy0 * (float)OV - cam.y, (float)W * OV, (float)H * OV }; SDL_RenderCopyF(g.ren, tex, nullptr, &dst); }
        return;
    }
    lastT = wallTime; lastCam = cam; lastAny = false;
    const NoiseTab& N = NT();
    const u64 tStart = SDL_GetPerformanceCounter();
    const float t = wallTime, storm = stormLevel(), rough = 0.35f + 0.65f * storm;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    bool any = false;
    const float DR = 20, DG = 30, DB = 40;   // the storm sea's slate grey
    const float fr = 238 - 34 * storm, fg = 246 - 30 * storm, fb = 250 - 24 * storm;   // foam: white, a touch grey under a storm sky
    const float k = 0.052f - 0.012f * storm, v = 1.0f + 0.55f * storm;   // roller spacing (rad per px) and speed
    // Work in blocks of 8 x 8 texels: a block whose corners are all land, or all calm open water, is skipped in one go (most of any view)
    const int BS = 8;
    for (int by = 0; by < H; by += BS) for (int bx = 0; bx < W; bx += BS) {
        const int ex1 = std::min(W, bx + BS), ey1 = std::min(H, by + BS);
        float dmin = 1e9f, dmax = -1;
        for (int cy = 0; cy <= 2; cy++) for (int cxx = 0; cxx <= 2; cxx++) {
            float d = shoreDist((gx0 + bx + (ex1 - bx) * cxx * 0.5f) * OV, (gy0 + by + (ey1 - by) * cy * 0.5f) * OV);
            dmin = std::min(dmin, d); dmax = std::max(dmax, d);
        }
        const bool allLand = dmax <= 0 && dmin <= 0, calmOpen = dmin > SURF_REACH + 24 && storm <= 0.01f;
        if (allLand || calmOpen) { for (int y = by; y < ey1; y++) std::fill(&px[(size_t)y * W + bx], &px[(size_t)y * W + ex1], 0u); continue; }
        for (int y = by; y < ey1; y++) {
            const float wy = (gy0 + y + 0.5f) * OV;
            u32* row = &px[(size_t)y * W];
            for (int x = bx; x < ex1; x++) {
                const float wx = (gx0 + x + 0.5f) * OV;
                float d = shoreDist(wx, wy);
                if (d <= 0 || (d > SURF_REACH && storm <= 0.01f)) { row[x] = 0; continue; }
                if (!rev) { int tx = clampi((int)wx >> 5, 0, MAP_W - 1), ty = clampi((int)wy >> 5, 0, MAP_H - 1); if (!ex[(size_t)ty * MAP_W + tx]) { row[x] = 0; continue; } }
                float foam = 0, dark = 0;
                if (d < SURF_REACH) {
                    const float n1 = N.at(wx * 0.009f, wy * 0.009f), n2 = N.at(wx * 0.022f + 19.0f, wy * 0.022f + 7.0f);
                    // rollers: a breaking line with a sharp front on the shore side and a foam trail fading out behind it (seaward); they run in
                    // toward the beach, break into long segments along their length and fade as they reach the wash
                    float ph = (d * k + t * v + n1 * 3.0f) * 0.15915494f;
                    float u = ph - std::floor(ph);   // 0 at a crest, growing seaward behind it
                    float prof = u < 0.035f ? u / 0.035f : std::exp(-(u - 0.035f) * 11.0f);
                    float zone = clampf((SURF_REACH - d) / 90.0f, 0, 1) * clampf((d - 6.0f) / 30.0f, 0, 1);
                    float seg = clampf((n2 - 0.34f + 0.22f * storm) * 3.0f, 0, 1);
                    foam += prof * zone * seg * (0.62f + 0.33f * rough);
                    // the wash: foam runs up the sand and slides back; a bright lacy fringe right at the waterline
                    float run = 7.0f + 6.0f * std::sin(t * 0.8f + n1 * 7.0f) + 7.0f * storm;
                    float lace = N.at(wx * 0.09f + t * 0.4f, wy * 0.09f) * N.at(wx * 0.21f - t * 0.3f, wy * 0.21f + 3.0f);
                    float edge = std::exp(-std::abs(d - run) * 0.45f);                       // the bright lip of the swash
                    float inside = d < run ? clampf(lace * 2.6f - 0.35f, 0, 1) * 0.55f : 0.0f;   // lacy foam left on the wet sand behind it
                    foam += edge * (0.55f + 0.35f * n2) + inside;
                    foam += clampf((28.0f - d) / 28.0f, 0, 1) * clampf(lace * 3.0f - 0.9f, 0, 1) * 0.45f;   // flecks in the breakers
                }
                if (storm > 0.01f) {   // the storm sea: darker and greyer, choppier further out
                    float open = clampf(d / 110.0f, 0, 1);
                    float chop = N.at(wx * 0.010f + t * 0.12f, wy * 0.010f - t * 0.08f);
                    dark = storm * (0.16f + 0.18f * open) * (0.7f + 0.6f * chop);
                }
                foam = clampf(foam, 0, 0.9f); dark = clampf(dark, 0, 0.55f);
                if (foam < 0.004f && dark < 0.004f) { row[x] = 0; continue; }
                float A = 1.0f - (1.0f - dark) * (1.0f - foam);   // dark under foam, as one colour and coverage
                float r = (DR * dark * (1 - foam) + fr * foam) / A, gg = (DG * dark * (1 - foam) + fg * foam) / A, b = (DB * dark * (1 - foam) + fb * foam) / A;
                row[x] = Canvas::pack(Color{ (u8)clampf(r, 0, 255), (u8)clampf(gg, 0, 255), (u8)clampf(b, 0, 255), (u8)(A * 255) });
                any = true;
            }
        }
    }
    if (getenv("ONEHOUR_WATERDBG")) fprintf(stderr, "water overlay cpu %.3f ms\n", (SDL_GetPerformanceCounter() - tStart) * 1000.0 / SDL_GetPerformanceFrequency());
    if (getenv("ONEHOUR_WATERDBG")) { int lit = 0, strong = 0; for (u32 q : px) { if (q >> 24) lit++; if ((q >> 24) > 100) strong++; } fprintf(stderr, "water overlay: %d lit texels, %d strong (of %d), storm %.2f, t %.1f\n", lit, strong, W * H, storm, t); }
    lastAny = any;
    if (!any) return;
    SDL_UpdateTexture(tex, nullptr, px.data(), W * 4);
    SDL_FRect dst = { gx0 * (float)OV - cam.x, gy0 * (float)OV - cam.y, (float)W * OV, (float)H * OV };
    SDL_RenderCopyF(g.ren, tex, nullptr, &dst);
}

// Sun glitter: points of light that wink on open water by day, strongest on a broad band across the view (toward the low sun at dawn and dusk)
void Game::drawWaterGlitter() {
    Gfx& g = g_gfx;
    if (F.d.empty() || detail < 1) return;
    float night = nightLevel(), sky = (1.0f - night) * (1.0f - 0.85f * clampf(weather.rain * 1.4f, 0, 1));
    if (sky < 0.05f) return;
    const float CELL = 40.0f;
    const int cx0 = (int)std::floor(cam.x / CELL), cy0 = (int)std::floor(cam.y / CELL), cx1 = (int)((cam.x + SCREEN_W) / CELL), cy1 = (int)((cam.y + VIEW_H) / CELL);
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    float p = dayPhase(), low = clampf(1.0f - std::abs(p - 0.30f) / 0.25f, 0, 1);   // (midday: glitter everywhere, a little; low sun: a bright path)
    g.beginAdd();
    for (int cy = cy0; cy <= cy1; cy++) for (int cx = cx0; cx <= cx1; cx++) {
        u32 h = (u32)cx * 73856093u ^ (u32)cy * 19349663u;
        float wx = (cx + hsh(h)) * CELL, wy = (cy + hsh(h + 1u)) * CELL;
        float d = shoreDist(wx, wy);
        if (d < 30.0f) continue;
        if (!rev) { int tx = clampi((int)wx >> 5, 0, MAP_W - 1), ty = clampi((int)wy >> 5, 0, MAP_H - 1); if (!ex[(size_t)ty * MAP_W + tx]) continue; }
        float per = 1.6f + 1.4f * hsh(h + 2u), ph = std::fmod(wallTime / per + hsh(h + 3u), 1.0f);
        float wink = ph < 0.18f ? std::sin(ph / 0.18f * 3.14159f) : 0.0f;
        if (wink <= 0.02f) continue;
        Vec2 s = worldToScreen(Vec2(wx, wy));
        float band = 0.35f + 0.65f * clampf(1.0f - std::abs((s.x - s.y * 0.6f) - SCREEN_W * 0.45f) / (SCREEN_W * (0.25f + 0.3f * (1 - low))), 0, 1);
        u8 a = (u8)clampf(200.0f * wink * band * sky, 0, 255);
        if (a < 8) continue;
        g.glowAdd(s.x, s.y, 2.2f + 1.6f * wink, Color{ 255, 250, 230, a });
        if (a > 110 && detail >= 2) { g.glowAdd(s.x - 4, s.y, 1.2f, Color{ 255, 250, 230, (u8)(a / 2) }); g.glowAdd(s.x + 4, s.y, 1.2f, Color{ 255, 250, 230, (u8)(a / 2) }); }
    }
    g.flushAdd();
}

// Whitecaps in a blow: short white breaking crests across the wind that flare up and fade, scattered over open water
void Game::drawWhitecaps() {
    Gfx& g = g_gfx;
    const float storm = stormLevel(), breeze = clampf(weather.rain * 0.6f, 0, 0.3f);
    const float level = std::max(storm, breeze);
    if (F.d.empty() || level < 0.05f || detail < 1) return;
    const float CELL = detail >= 2 ? 40.0f : 60.0f;
    const int cx0 = (int)std::floor(cam.x / CELL), cy0 = (int)std::floor(cam.y / CELL), cx1 = (int)((cam.x + SCREEN_W) / CELL), cy1 = (int)((cam.y + VIEW_H) / CELL);
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    const float wa = weather.wind + 1.5708f;   // crests run across the wind
    const Vec2 along(std::cos(wa), std::sin(wa)), down(std::cos(weather.wind), std::sin(weather.wind));
    for (int cy = cy0; cy <= cy1; cy++) for (int cx = cx0; cx <= cx1; cx++) {
        u32 h = (u32)cx * 83492791u ^ (u32)cy * 2654435761u;
        float per = 2.2f + 1.8f * hsh(h + 5u), ph = std::fmod(wallTime / per + hsh(h + 6u), 1.0f);
        if (hsh(h + 7u + (u32)(wallTime / per + hsh(h + 6u))) > level * 0.85f) continue;   // (each flare-up rolls its own dice)
        float life = ph < 0.25f ? ph / 0.25f : 1.0f - (ph - 0.25f) / 0.75f;
        if (life <= 0.03f) continue;
        float wx = (cx + hsh(h)) * CELL + down.x * ph * 14.0f, wy = (cy + hsh(h + 1u)) * CELL + down.y * ph * 14.0f;
        float d = shoreDist(wx, wy);
        if (d < 50.0f) continue;
        if (!rev) { int tx = clampi((int)wx >> 5, 0, MAP_W - 1), ty = clampi((int)wy >> 5, 0, MAP_H - 1); if (!ex[(size_t)ty * MAP_W + tx]) continue; }
        Vec2 s = worldToScreen(Vec2(wx, wy));
        float len = (7.0f + 11.0f * hsh(h + 3u)) * (0.6f + 0.4f * life);
        u8 a = (u8)clampf(300.0f * life * (0.55f + 0.45f * level), 0, 255);
        // a soft white patch stretched along the crest, and a fainter spill of foam just downwind of it
        g.drawSized(g.fxs.dust[h % 3], s.x, s.y, len * 2.6f, 6.0f + 4.0f * life, wa, rgb(244, 248, 252), a);
        if (detail >= 2) g.drawSized(g.fxs.dust[(h + 1) % 3], s.x + down.x * 5, s.y + down.y * 5, len * 1.6f, 7.0f, wa, rgb(232, 240, 246), (u8)(a / 3));
    }
}

// ------------------------------------------------------------ lightning
// A strike: a forked bolt from the top of the sky to a spot in or near the view, a flash, and its thunder after a delay that grows with distance
void Game::lightningStrike() {
    Rng& r = weather.rng;
    Vec2 at(cam.x + r.f(-0.2f, 1.2f) * SCREEN_W, cam.y + r.f(-0.1f, 1.1f) * VIEW_H);
    auto& B = weather.boltPath; B.clear();
    Vec2 p(at.x + r.f(-260, 260), at.y - VIEW_H * 1.1f);
    const int steps = 16;
    for (int i = 0; i <= steps; i++) {
        float k = i / (float)steps;
        Vec2 q = p + (at - p) * k + Vec2(r.f(-38, 38), r.f(-12, 12)) * (i == 0 || i == steps ? 0.0f : 1.0f);
        B.push_back(q);
    }
    weather.boltBranches.clear();
    for (int b = 0; b < 3; b++) {   // side forks off the main channel
        int from = r.range(3, steps - 4);
        Vec2 q = B[from], dir = Vec2(r.f(-1, 1), r.f(0.4f, 1.0f)).norm();
        std::vector<Vec2> br{ q };
        for (int i = 0; i < 5; i++) { q = q + dir * r.f(26, 52) + Vec2(r.f(-18, 18), r.f(-8, 8)); br.push_back(q); }
        weather.boltBranches.push_back(br);
    }
    weather.boltAt = at; weather.boltT = wallTime;
    Vec2 mid(cam.x + SCREEN_W * 0.5f, cam.y + VIEW_H * 0.5f);
    float dfar = clampf(dist(at, mid) / 1400.0f, 0, 1);
    g_audio.thunder(dfar);
}

void Game::drawLightning() {
    Gfx& g = g_gfx;
    if (weather.boltT < 0 || weather.boltPath.empty()) return;
    float k = wallTime - weather.boltT;   // seconds since the strike
    if (k > 0.45f) return;
    float flick = k < 0.08f ? 1.0f : (k < 0.14f ? 0.35f : (k < 0.22f ? 0.9f : clampf(1.0f - (k - 0.22f) / 0.23f, 0, 1)));   // the return strokes
    auto stroke = [&](const std::vector<Vec2>& P, float wide, float a) {
        for (size_t i = 0; i + 1 < P.size(); i++) {
            Vec2 s0 = worldToScreen(P[i]), s1 = worldToScreen(P[i + 1]);
            g.thickLine(s0.x, s0.y, s1.x, s1.y, Color{ 150, 170, 255, (u8)clampf(70 * a, 0, 255) }, wide * 3.2f);
            g.thickLine(s0.x, s0.y, s1.x, s1.y, Color{ 235, 240, 255, (u8)clampf(235 * a, 0, 255) }, wide);
        }
    };
    SDL_BlendMode old; SDL_GetRenderDrawBlendMode(g.ren, &old);
    stroke(weather.boltPath, 2.2f, flick);
    for (auto& b : weather.boltBranches) stroke(b, 1.2f, flick * 0.7f);
    Vec2 s = worldToScreen(weather.boltAt);
    g.beginAdd();
    g.glowAdd(s.x, s.y, 150.0f, Color{ 170, 190, 255, (u8)(110 * flick) });
    g.glowAdd(s.x, s.y, 40.0f, Color{ 240, 244, 255, (u8)(200 * flick) });
    g.flushAdd();
    SDL_SetRenderDrawBlendMode(g.ren, old);
}
