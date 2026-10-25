#include "gfx.h"
#include "map.h"
#include "art.h"
#include "art2.h"
#include "artb.h"
#include "terrain.h"
#include "icons.h"

Gfx g_gfx;

const Color PLAYER_COLOR[MAX_PLAYERS] = { rgb(70, 170, 255), rgb(235, 70, 60), rgb(240, 205, 60), rgb(90, 215, 95), rgb(186, 92, 214) };   // (the last: the riot crowd's violet armbands)
int g_colorSlot[MAX_PLAYERS] = { 0, 1, 2, 3, 4 };

// 5x7 bitmap font, column-major, LSB = top row (ASCII 32..126)
static const u8 FONT5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},
    {0x14,0x08,0x3E,0x08,0x14},{0x08,0x08,0x3E,0x08,0x08},{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x01,0x01},{0x3E,0x41,0x41,0x51,0x32},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x04,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x7F,0x20,0x18,0x20,0x7F},{0x63,0x14,0x08,0x14,0x63},{0x03,0x04,0x78,0x04,0x03},{0x61,0x51,0x49,0x45,0x43},{0x00,0x00,0x7F,0x41,0x41},
    {0x02,0x04,0x08,0x10,0x20},{0x41,0x41,0x7F,0x00,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},
    {0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},{0x08,0x14,0x54,0x54,0x3C},{0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x00,0x7F,0x10,0x28,0x44},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x08,0x08,0x2A,0x1C,0x08},
};

// ------------------------------------------------------------ init
// Logical size = window size / UI scale. The scale grows in big windows so the HUD stays readable
// and the visible area never exceeds the map.
void Gfx::onResize() {
    if (!win || !ren) return;
    int w = 0, h = 0;
    SDL_GetWindowSize(win, &w, &h);
    if (w <= 0 || h <= 0) return;
    int s = userScale > 0 ? userScale : 1;
    while (s < 8 && (w / s > 2200 || h / s > 1400 || (userScale <= 0 && w >= 2560 && h >= 1500 && s < 2))) s++;
    while (s > 1 && (w / s < MIN_SCREEN_W || h / s < MIN_SCREEN_H)) s--;
    setScreenSize(std::max(MIN_SCREEN_W, w / s), std::max(MIN_SCREEN_H, h / s));
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
}

bool Gfx::init(int scale, bool software, int reqW, int reqH) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "video: %s\n", SDL_GetError()); return false; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    userScale = scale;
    // default window: most of the desktop, capped so the map stays the focus
    int ww = reqW, wh = reqH;
    if (ww <= 0 || wh <= 0) {
        SDL_Rect ub = { 0, 0, 0, 0 };
        if (SDL_GetDisplayUsableBounds(0, &ub) != 0 || ub.w < 640) { ub.w = 1280; ub.h = 800; }
        int s = std::max(1, scale);
        ww = clampi((int)(ub.w * 0.92f), MIN_SCREEN_W * s, 1800 * s);
        wh = clampi((int)(ub.h * 0.92f), MIN_SCREEN_H * s, 1100 * s);
    }
    win = SDL_CreateWindow("One Hour", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, ww, wh, SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return false; }
    SDL_SetWindowMinimumSize(win, MIN_SCREEN_W, MIN_SCREEN_H);
    u32 flags = software ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ren = SDL_CreateRenderer(win, -1, flags);
    if (!ren && !software) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return false; }
    { SDL_RendererInfo ri; if (SDL_GetRendererInfo(ren, &ri) == 0) softwareRenderer = (ri.flags & SDL_RENDERER_SOFTWARE) != 0; }
    onResize();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    { Canvas c(2, 2); c.fillRect(0, 0, 2, 2, rgb(255, 255, 255)); white = fromCanvas(c, 0, 0).tex; }
    buildFont(); bakeTerrain(); buildUnits(); buildBuildings(); buildMisc(); buildEffects(); buildMinimap();
    iconsInit(*this);
    return true;
}

void Gfx::shutdown() {
    if (ren) SDL_DestroyRenderer(ren);
    if (win) SDL_DestroyWindow(win);
}

Sprite Gfx::fromCanvas(Canvas& c, float ox, float oy) {
    Sprite s;
    s.tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, c.w, c.h);
    SDL_UpdateTexture(s.tex, nullptr, c.px.data(), c.w * 4);
    SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_BLEND);
    s.w = c.w; s.h = c.h; s.ox = ox; s.oy = oy;
    return s;
}

Sprite Gfx::fromCanvasAdd(Canvas& c, float ox, float oy) {
    Sprite s = fromCanvasSmooth(c, ox, oy);
    SDL_SetTextureBlendMode(s.tex, SDL_BLENDMODE_ADD);
    return s;
}

Sprite Gfx::fromCanvasSmooth(Canvas& c, float ox, float oy) {
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    Sprite s = fromCanvas(c, ox, oy);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    return s;
}

// ------------------------------------------------------------ primitives
void Gfx::draw(const Sprite& s, float x, float y, float angle, float scale, Color mod, u8 alpha) {
    if (!s.tex) return;
    scale *= s.dscale;
    SDL_Rect dst = { (int)std::lround(x - s.ox * scale), (int)std::lround(y - s.oy * scale), (int)std::lround(s.w * scale), (int)std::lround(s.h * scale) };
    SDL_Point c = { (int)std::lround(s.ox * scale), (int)std::lround(s.oy * scale) };
    fillPx += std::max(0, std::min(dst.x + dst.w, SCREEN_W) - std::max(dst.x, 0)) * (double)std::max(0, std::min(dst.y + dst.h, SCREEN_H) - std::max(dst.y, 0));
    SDL_SetTextureColorMod(s.tex, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    if (angle == 0) SDL_RenderCopy(ren, s.tex, nullptr, &dst);
    else {
        static const bool slowDbg = getenv("ONEHOUR_SLOWDRAW") != nullptr;
        u64 t0 = slowDbg ? SDL_GetPerformanceCounter() : 0;
        SDL_RenderCopyEx(ren, s.tex, nullptr, &dst, angle * 57.29578f, &c, SDL_FLIP_NONE);
        if (slowDbg) { double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency(); if (ms > 20) fprintf(stderr, "[slowdraw] draw rot %.0f ms: sprite %dx%d dst %d,%d %dx%d\n", ms, s.w, s.h, dst.x, dst.y, dst.w, dst.h); }
    }
}
void Gfx::drawRolled(const Sprite& s, float x, float y, float angle, float roll, float scale, Color mod, u8 alpha) {
    if (!s.tex) return;
    float c = std::cos(roll), k = std::max(0.22f, std::abs(c));
    scale *= s.dscale;
    SDL_FRect dst = { x - s.ox * scale, y - s.oy * scale * k, s.w * scale, s.h * scale * k };
    SDL_FPoint pc = { s.ox * scale, s.oy * scale * k };
    float shade = c < 0 ? 0.72f : 1.0f;   // (the belly is in its own shadow)
    SDL_SetTextureColorMod(s.tex, (u8)(mod.r * shade), (u8)(mod.g * shade), (u8)(mod.b * shade));
    SDL_SetTextureAlphaMod(s.tex, alpha);
    SDL_RenderCopyExF(ren, s.tex, nullptr, &dst, angle * 57.29578f, &pc, c < 0 ? SDL_FLIP_VERTICAL : SDL_FLIP_NONE);
}
void Gfx::drawRect(const Sprite& s, int x, int y, int w, int h, u8 alpha) {
    SDL_Rect dst = { x, y, w, h };
    SDL_SetTextureColorMod(s.tex, 255, 255, 255);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    SDL_RenderCopy(ren, s.tex, nullptr, &dst);
}
void Gfx::fill(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_Rect r = { x, y, w, h };
    SDL_RenderFillRect(ren, &r);
}
void Gfx::box(int x, int y, int w, int h, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_Rect r = { x, y, w, h };
    SDL_RenderDrawRect(ren, &r);
}
void Gfx::line(float x0, float y0, float x1, float y1, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_RenderDrawLineF(ren, x0, y0, x1, y1);
}
void Gfx::thickLine(float x0, float y0, float x1, float y1, Color c, float thick) {
    Vec2 d = Vec2(x1 - x0, y1 - y0).norm();
    Vec2 n(-d.y, d.x);
    int k = std::max(1, (int)thick);
    for (int i = 0; i < k; i++) {
        float off = (i - (k - 1) * 0.5f);
        line(x0 + n.x * off, y0 + n.y * off, x1 + n.x * off, y1 + n.y * off, c);
    }
}
void Gfx::circle(float cx, float cy, float r, Color c, int segs) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    SDL_FPoint pts[64];
    segs = std::min(segs, 63);
    for (int i = 0; i <= segs; i++) { float a = i * 6.2831853f / segs; pts[i] = { cx + std::cos(a) * r, cy + std::sin(a) * r }; }
    SDL_RenderDrawLinesF(ren, pts, segs + 1);
}
void Gfx::fillCircle(float cx, float cy, float r, Color c) {
    // draw as a scaled blob sprite (cheap)
    draw(blob, cx, cy, 0, r / 16.0f, c, c.a);
}
void Gfx::glowAdd(float cx, float cy, float r, Color c) {
    if (!blobAdd.tex) return;
    draw(blobAdd, cx, cy, 0, r / 16.0f, c, c.a);
}
void Gfx::drawSized(const Sprite& s, float cx, float cy, float w, float h, float angle, Color mod, u8 alpha) {
    if (!s.tex) return;
    if (angle != 0 && w > 600 && w >= 4 * h) {
        // a long beam (a laser to a warhead high over the map, a rail shot across the screen): only the part over the view is drawn. Rotating the
        // whole of a 3000 px quad cost the CPU renderer 20-30 ms a beam - seconds a frame during a nuclear exchange - and a GPU nothing it shows
        Vec2 d(std::cos(angle), std::sin(angle));
        float t0 = -w * 0.5f, t1 = w * 0.5f;
        const float m = h + 8.0f, lo[2] = { -m, -m }, hi[2] = { SCREEN_W + m, SCREEN_H + m }, p0[2] = { cx, cy }, dv[2] = { d.x, d.y };
        for (int k = 0; k < 2; k++) {   // Liang-Barsky against the view, one axis at a time
            if (std::fabs(dv[k]) < 1e-6f) { if (p0[k] < lo[k] || p0[k] > hi[k]) return; continue; }
            float a = (lo[k] - p0[k]) / dv[k], b = (hi[k] - p0[k]) / dv[k];
            if (a > b) std::swap(a, b);
            t0 = std::max(t0, a); t1 = std::min(t1, b);
            if (t0 >= t1) return;
        }
        cx += d.x * (t0 + t1) * 0.5f; cy += d.y * (t0 + t1) * 0.5f; w = t1 - t0;
    }
    SDL_FRect dst = { cx - w * 0.5f, cy - h * 0.5f, w, h };
    fillPx += std::max(0.0f, std::min(dst.x + dst.w, (float)SCREEN_W) - std::max(dst.x, 0.0f)) * (double)std::max(0.0f, std::min(dst.y + dst.h, (float)SCREEN_H) - std::max(dst.y, 0.0f));
    SDL_SetTextureColorMod(s.tex, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    if (angle == 0) { SDL_RenderCopyF(ren, s.tex, nullptr, &dst); return; }
    if (softwareRenderer && (w < 2.2f * h || w <= 96)) {
        // The CPU renderer rotates a texture by building a rotated copy for every call: with a battle's smoke, sparks and tracers that was most of
        // the frame. Small effects do not need it: a thin streak becomes a line along its heading, anything rounder (smoke, dust, debris) is drawn
        // upright, which reads the same at that size.
        if (w >= 2.2f * h) {
            Vec2 d(std::cos(angle) * w * 0.5f, std::sin(angle) * w * 0.5f);
            Color c{ mod.r, mod.g, mod.b, alpha };
            line(cx - d.x, cy - d.y, cx + d.x, cy + d.y, c);
            if (h >= 3.0f) { Vec2 n(-std::sin(angle), std::cos(angle)); line(cx - d.x * 0.8f + n.x, cy - d.y * 0.8f + n.y, cx + d.x * 0.8f + n.x, cy + d.y * 0.8f + n.y, Color{ mod.r, mod.g, mod.b, (u8)(alpha / 2) }); }
        } else SDL_RenderCopyF(ren, s.tex, nullptr, &dst);
        return;
    }
    static const bool slowDbg = getenv("ONEHOUR_SLOWDRAW") != nullptr;
    u64 t0 = slowDbg ? SDL_GetPerformanceCounter() : 0;
    SDL_RenderCopyExF(ren, s.tex, nullptr, &dst, angle * 57.29578f, nullptr, SDL_FLIP_NONE);
    if (slowDbg) { double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency(); if (ms > 20) fprintf(stderr, "[slowdraw] drawSized rot %.0f ms: sprite %dx%d dst %.0f,%.0f %.0fx%.0f\n", ms, s.w, s.h, dst.x, dst.y, dst.w, dst.h); }
}
void Gfx::discFill(float cx, float cy, float r, Color c) {
    if (!disc.tex || r < 1) return;
    SDL_FRect dst = { cx - r, cy - r, r * 2, r * 2 };
    SDL_SetTextureColorMod(disc.tex, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(disc.tex, c.a);
    SDL_RenderCopyF(ren, disc.tex, nullptr, &dst);
}
void Gfx::dashedCircle(float cx, float cy, float r, Color c, float phase, float dash, float gap) {
    if (r < 2) return;
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, c.a);
    float circ = 6.2831853f * r;
    int n = clampi((int)(circ / 3.0f), 24, 900);
    float step = circ / n, period = dash + gap;
    SDL_FPoint pts[3];
    for (int i = 0; i < n; i++) {
        float s = i * step + phase;
        if (std::fmod(s < 0 ? s + period * 1000 : s, period) > dash) continue;
        float a0 = i * 6.2831853f / n, a1 = (i + 1) * 6.2831853f / n;
        pts[0] = { cx + std::cos(a0) * r, cy + std::sin(a0) * r };
        pts[1] = { cx + std::cos(a1) * r, cy + std::sin(a1) * r };
        SDL_RenderDrawLinesF(ren, pts, 2);
    }
}
void Gfx::bevelPanel(int x, int y, int w, int h, Color base, bool raised) {
    fill(x, y, w, h, base);
    Color hi = shade(base, raised ? 1.35f : 0.65f), lo = shade(base, raised ? 0.6f : 1.3f);
    fill(x, y, w, 1, hi); fill(x, y, 1, h, hi);
    fill(x, y + h - 1, w, 1, lo); fill(x + w - 1, y, 1, h, lo);
}
void Gfx::text(int x, int y, const char* s, Color c, int scale, bool shadow) {
    if (!s) return;
    for (int pass = shadow ? 0 : 1; pass < 2; pass++) {
        int cx = x + (pass == 0 ? scale : 0), cy = y + (pass == 0 ? scale : 0);
        if (pass == 0) SDL_SetTextureColorMod(font, 0, 0, 0); else SDL_SetTextureColorMod(font, c.r, c.g, c.b);
        SDL_SetTextureAlphaMod(font, pass == 0 ? (u8)(c.a * 0.7f) : c.a);
        for (const char* p = s; *p; p++) {
            int ch = (u8)*p;
            if (ch < 32 || ch > 126) ch = '?';
            SDL_Rect src = { (ch - 32) * 6, 0, 6, 8 };
            SDL_Rect dst = { cx, cy, 6 * scale, 8 * scale };
            SDL_RenderCopy(ren, font, &src, &dst);
            cx += 6 * scale;
        }
    }
}
void Gfx::beginFrame(Color clear) {
    SDL_SetRenderDrawColor(ren, clear.r, clear.g, clear.b, 255);
    SDL_RenderClear(ren);
}
void Gfx::present() { SDL_RenderPresent(ren); }

// ------------------------------------------------------------ font
void Gfx::buildFont() {
    Canvas c(95 * 6, 8);
    for (int g = 0; g < 95; g++) for (int col = 0; col < 5; col++) for (int row = 0; row < 7; row++)
        if (FONT5x7[g][col] >> row & 1) c.set(g * 6 + col, row, rgb(255, 255, 255));
    font = fromCanvas(c, 0, 0).tex;
}

// ------------------------------------------------------------ baked world terrain (see terrain.cpp)
void Gfx::bakeTerrain() {
    Canvas c(WORLD_W, WORLD_H);
    terrainBake(c);
    // On a GPU the world is RGB565 (half the memory and sampling bandwidth, dithered so nothing bands). The software
    // renderer has no fast 565-to-display blit (measured +11 ms a frame), so there it stays 32-bit and memcpy-fast.
    // On a GPU every chunk is RGB565 (half the memory and sampling bandwidth, dithered on world coordinates so nothing bands or seams). The
    // software renderer has no fast 565-to-display blit (measured +11 ms a frame), so there they stay 32-bit and memcpy-fast.
    worldCols = (WORLD_W + WORLD_CHUNK - 1) / WORLD_CHUNK; worldRows = (WORLD_H + WORLD_CHUNK - 1) / WORLD_CHUNK;
    worldChunks.assign((size_t)worldCols * worldRows, nullptr);
    {
        static const int BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
        std::vector<u16> row;
        for (int cy = 0; cy < worldRows; cy++) for (int cx = 0; cx < worldCols; cx++) {
            int x0 = cx * WORLD_CHUNK, y0 = cy * WORLD_CHUNK, w = std::min(WORLD_CHUNK, WORLD_W - x0), h = std::min(WORLD_CHUNK, WORLD_H - y0);
            SDL_Texture* t = softwareRenderer ? nullptr : SDL_CreateTexture(ren, SDL_PIXELFORMAT_RGB565, SDL_TEXTUREACCESS_STATIC, w, h);
            if (t) {
                row.resize((size_t)w);
                for (int y = 0; y < h; y++) {
                    for (int x = 0; x < w; x++) {
                        Color col = Canvas::unpack(c.px[(size_t)(y0 + y) * c.w + x0 + x]);
                        int dth = BAYER[((y0 + y) & 3) * 4 + ((x0 + x) & 3)] - 8;
                        int r = clampi((int)col.r + dth / 4, 0, 255) >> 3, gg = clampi((int)col.g + dth / 8, 0, 255) >> 2, b = clampi((int)col.b + dth / 4, 0, 255) >> 3;
                        row[x] = (u16)((r << 11) | (gg << 5) | b);
                    }
                    SDL_Rect rr = { 0, y, w, 1 };
                    SDL_UpdateTexture(t, &rr, row.data(), w * 2);
                }
            } else {
                t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
                if (t) SDL_UpdateTexture(t, nullptr, &c.px[(size_t)y0 * c.w + x0], c.w * 4);
            }
            if (t) SDL_SetTextureBlendMode(t, SDL_BLENDMODE_NONE);
            worldChunks[(size_t)cy * worldCols + cx] = t;
        }
    }
    if (const char* wp = getenv("ONEHOUR_WORLDBMP")) {   // debugging: dump the baked ground (ONEHOUR_WORLDBMP=file.bmp)
        if (SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(c.px.data(), c.w, c.h, 32, c.w * 4, SDL_PIXELFORMAT_ABGR8888)) { SDL_SaveBMP(sf, wp); SDL_FreeSurface(sf); }
    }
    if (const char* op = getenv("ONEHOUR_OVERVIEW")) {   // debugging: the whole baked map shrunk to 960x960 (ONEHOUR_OVERVIEW=file.bmp)
        Canvas big(960, 960);
        terrainOverview(c, big);
        if (SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(big.px.data(), big.w, big.h, 32, big.w * 4, SDL_PIXELFORMAT_ABGR8888)) { SDL_SaveBMP(sf, op); SDL_FreeSurface(sf); }
    }
    Canvas mini(232, 232);
    terrainOverview(c, mini);
    minimapTerrain = fromCanvasSmooth(mini, 0, 0);
    // fog mask, filtered bilinearly at creation time (hint is read when the texture is created)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    shroudTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MAP_W * SHROUD_SS, MAP_H * SHROUD_SS);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (shroudTex) SDL_SetTextureBlendMode(shroudTex, SDL_BLENDMODE_BLEND);
}

void Gfx::rebakeTerrain() {
    for (auto* t : worldChunks) if (t) SDL_DestroyTexture(t);
    worldChunks.clear();
    if (minimapTerrain.tex) SDL_DestroyTexture(minimapTerrain.tex);
    if (shroudTex) SDL_DestroyTexture(shroudTex);
    minimapTerrain = Sprite(); shroudTex = nullptr;
    bakeTerrain();
    terrainEdited = false;
}

void Gfx::refreshTerrain() {
    if (g_map.cleared.empty()) return;
    // one structure's footprint at a time (Map::clearForStructure ends each with -1): structures placed far apart must not merge into one huge rectangle
    std::vector<int> group;
    for (size_t i = 0; i <= g_map.cleared.size(); i++) {
        if (i < g_map.cleared.size() && g_map.cleared[i] >= 0) { group.push_back(g_map.cleared[i]); continue; }
        if (!group.empty()) uploadTerrainPatch(group);
        group.clear();
    }
    g_map.cleared.clear();
}

void Gfx::uploadTerrainPatch(const std::vector<int>& group) {
    int x = 0, y = 0, w = 0, h = 0; std::vector<u32> px;
    bool ok = terrainRepaint(group, x, y, w, h, px);
    if (!ok || worldChunks.empty()) return;
    terrainEdited = true;
    static const int BAYER[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };   // (the same dither as the bake, on world coordinates)
    std::vector<u16> out;
    for (int cy = y / WORLD_CHUNK; cy <= (y + h - 1) / WORLD_CHUNK && cy < worldRows; cy++) for (int cx = x / WORLD_CHUNK; cx <= (x + w - 1) / WORLD_CHUNK && cx < worldCols; cx++) {
        SDL_Texture* t = worldChunks[(size_t)cy * worldCols + cx];
        if (!t) continue;
        int ax = std::max(x, cx * WORLD_CHUNK), ay = std::max(y, cy * WORLD_CHUNK);   // the part of the patch in this chunk (world px)
        int bx = std::min(x + w, (cx + 1) * WORLD_CHUNK), by = std::min(y + h, (cy + 1) * WORLD_CHUNK);
        if (bx <= ax || by <= ay) continue;
        SDL_Rect r = { ax - cx * WORLD_CHUNK, ay - cy * WORLD_CHUNK, bx - ax, by - ay };
        Uint32 fmt = 0; SDL_QueryTexture(t, &fmt, nullptr, nullptr, nullptr);
        if (fmt != SDL_PIXELFORMAT_RGB565) { SDL_UpdateTexture(t, &r, &px[(size_t)(ay - y) * w + (ax - x)], w * 4); continue; }
        out.resize((size_t)r.w * r.h);
        for (int j = 0; j < r.h; j++) for (int i = 0; i < r.w; i++) {
            Color col = Canvas::unpack(px[(size_t)(ay - y + j) * w + (ax - x + i)]);
            int dth = BAYER[((ay + j) & 3) * 4 + ((ax + i) & 3)] - 8;
            int rr = clampi((int)col.r + dth / 4, 0, 255) >> 3, gg = clampi((int)col.g + dth / 8, 0, 255) >> 2, bb = clampi((int)col.b + dth / 4, 0, 255) >> 3;
            out[(size_t)j * r.w + i] = (u16)((rr << 11) | (gg << 5) | bb);
        }
        SDL_UpdateTexture(t, &r, out.data(), r.w * 2);
    }
}

void Gfx::drawWorld(int wx, int wy, int w, int h, int dx, int dy) {
    if (worldChunks.empty() || w <= 0 || h <= 0) return;
    int x1 = std::min(WORLD_W, wx + w), y1 = std::min(WORLD_H, wy + h);
    dx += std::max(0, wx) - wx; dy += std::max(0, wy) - wy;
    wx = std::max(0, wx); wy = std::max(0, wy);
    for (int cy = wy / WORLD_CHUNK; cy < worldRows && cy * WORLD_CHUNK < y1; cy++) for (int cx = wx / WORLD_CHUNK; cx < worldCols && cx * WORLD_CHUNK < x1; cx++) {
        SDL_Texture* t = worldChunks[(size_t)cy * worldCols + cx];
        if (!t) continue;
        int ax = std::max(wx, cx * WORLD_CHUNK), ay = std::max(wy, cy * WORLD_CHUNK), bx = std::min(x1, (cx + 1) * WORLD_CHUNK), by = std::min(y1, (cy + 1) * WORLD_CHUNK);
        SDL_Rect src = { ax - cx * WORLD_CHUNK, ay - cy * WORLD_CHUNK, bx - ax, by - ay }, dst = { dx + (ax - wx), dy + (ay - wy), bx - ax, by - ay };
        SDL_RenderCopy(ren, t, &src, &dst);
    }
}

void Gfx::updateShroud(const std::vector<u8>& fog, bool force) {
    if (!shroudTex) return;
    // Three levels: ground never seen is covered completely (a mottled near-black, fully opaque: nothing shows through it), remembered ground lies
    // under a cool grey veil (its terrain and structures show, its people do not), ground in sight is clear. The tile levels are upsampled with
    // bilinear weights and the boundaries pushed around by value noise, so they are ragged, cloudy fringes instead of a staircase of tiles.
    const int SS = SHROUD_SS, W2 = MAP_W * SS, H2 = MAP_H * SS;
    static std::vector<u32> buf; static std::vector<float> noise;
    static std::vector<u8> mottle; static std::vector<float> edgeN;   // the mottling and the fringe displacement are fixed per pixel: baked once, looked up per refresh
    if (noise.empty()) { Rng r(99); noise.resize(64 * 64); for (auto& v : noise) v = r.f(); }
    auto vn = [&](float x, float y) {
        int ix = (int)std::floor(x), iy = (int)std::floor(y); float fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
        auto at = [&](int a, int b) { return noise[(b & 63) * 64 + (a & 63)]; };
        return lerpf(lerpf(at(ix, iy), at(ix + 1, iy), fx), lerpf(at(ix, iy + 1), at(ix + 1, iy + 1), fx), fy);
    };
    if ((int)mottle.size() != W2 * H2) {
        mottle.assign((size_t)W2 * H2, 0); edgeN.assign((size_t)W2 * H2, 0);
        for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++) {
            mottle[(size_t)y * W2 + x] = (u8)(vn(x * 0.35f, y * 0.35f) * 10);
            edgeN[(size_t)y * W2 + x] = vn(x * 0.55f + 11, y * 0.55f + 5) * 0.65f + vn(x * 1.4f, y * 1.4f) * 0.35f;
        }
    }
    // Only the tiles whose fog changed, and the ring of tiles round them (the bilinear fringe of a tile reaches one tile into its neighbours), are
    // rebuilt, and the texture is updated band by band (BLK tile rows at a time, the span of what changed in it). The old scheme rebuilt whole
    // 8 x 8-tile blocks and all eight neighbours of each for every tile that changed: in a late battle that was most of the frame's CPU time.
    // A new texture (a rebaked map) or a new match starts from nothing: the whole mask is rebuilt. (Patching only what changed since the last
    // refresh left the previous match's explored ground clear in the new texture, so the map showed through the fog.)
    static std::vector<u8> last;
    static SDL_Texture* lastTex = nullptr;
    static std::vector<u8> dirty;   // per tile
    const int BLK = 8, BH = (MAP_H + BLK - 1) / BLK;
    bool full = force || shroudTex != lastTex || (int)buf.size() != W2 * H2 || last.size() != fog.size();
    lastTex = shroudTex;
    if (full) { buf.assign((size_t)W2 * H2, 0); dirty.assign((size_t)MAP_W * MAP_H, 1); }
    else {
        bool any = false;
        for (int i = 0; i < MAP_W * MAP_H; i++) if (fog[i] != last[i]) {
            if (!any) { dirty.assign((size_t)MAP_W * MAP_H, 0); any = true; }
            int x = i % MAP_W, y = i / MAP_W;
            for (int j = std::max(0, y - 1); j <= std::min(MAP_H - 1, y + 1); j++) for (int k = std::max(0, x - 1); k <= std::min(MAP_W - 1, x + 1); k++) dirty[(size_t)j * MAP_W + k] = 1;
        }
        if (!any) return;
    }
    last = fog;
    static const float LV[3] = { 0.0f, 0.5f, 1.0f };
    const float DIM = 118.0f;   // the remembered veil's opacity
    auto E = [&](int a, int b) { return LV[fog[clampi(b, 0, MAP_H - 1) * MAP_W + clampi(a, 0, MAP_W - 1)]]; };
    auto pack = [](float a, int r, int g, int b) { return (u32)clampi((int)a, 0, 255) << 24 | (u32)b << 16 | (u32)g << 8 | (u32)r; };
    auto F = [&](int a, int b) { return fog[clampi(b, 0, MAP_H - 1) * MAP_W + clampi(a, 0, MAP_W - 1)]; };
    for (int band = 0; band < BH; band++) {
        int bx0 = MAP_W, bx1 = -1;
        const int ty0 = band * BLK, ty1 = std::min(MAP_H, ty0 + BLK);
        for (int ty = ty0; ty < ty1; ty++) for (int tx = 0; tx < MAP_W; tx++) {
            if (!dirty[(size_t)ty * MAP_W + tx]) continue;
            bx0 = std::min(bx0, tx); bx1 = std::max(bx1, tx);
            const int px0 = tx * SS, py0 = ty * SS;
            // one level all round (the tile and its eight neighbours): no fringe, a plain fill
            const u8 l0 = F(tx, ty);
            bool flat = true;
            for (int j = -1; j <= 1 && flat; j++) for (int k = -1; k <= 1; k++) if (F(tx + k, ty + j) != l0) { flat = false; break; }
            if (flat) {
                for (int y = py0; y < py0 + SS; y++) for (int x = px0; x < px0 + SS; x++) {
                    u32& o = buf[(size_t)y * W2 + x];
                    if (l0 == 2) o = 0;
                    else if (l0 == 1) o = pack(DIM, 14, 20, 30);
                    else { int m = mottle[(size_t)y * W2 + x]; o = pack(255, 5 + m / 3, 8 + m / 3, 12 + m / 2); }
                }
                continue;
            }
            for (int y = py0; y < py0 + SS; y++) for (int x = px0; x < px0 + SS; x++) {
                float fx = (x + 0.5f) / SS - 0.5f, fy = (y + 0.5f) / SS - 0.5f;
                int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy); float tx2 = fx - x0, ty2 = fy - y0;
                float a00 = E(x0, y0), a10 = E(x0 + 1, y0), a01 = E(x0, y0 + 1), a11 = E(x0 + 1, y0 + 1);
                float v = lerpf(lerpf(a00, a10, tx2), lerpf(a01, a11, tx2), ty2);
                u32& o = buf[(size_t)y * W2 + x];
                int m = mottle[(size_t)y * W2 + x];
                bool pflat = a00 == a10 && a00 == a01 && a00 == a11;   // inside one level: no fringe to displace
                if (!pflat) v = clampf(v + (edgeN[(size_t)y * W2 + x] - 0.5f) * 0.34f, 0.0f, 1.0f);
                if (v <= 0.0f) { o = pack(255, 5 + m / 3, 8 + m / 3, 12 + m / 2); continue; }   // never seen: opaque, faintly mottled
                if (v >= 1.0f) { o = 0; continue; }                                               // in sight: clear
                if (v < 0.5f) {   // the edge of the unknown: from opaque to the veil
                    float k = v / 0.5f; k = k * k * (3 - 2 * k);
                    o = pack(lerpf(255.0f, DIM, k), (int)lerpf(5, 14, k), (int)lerpf(8, 20, k), (int)lerpf(12, 30, k));
                } else {          // the edge of sight: from the veil to clear
                    float k = (v - 0.5f) / 0.5f; k = k * k * (3 - 2 * k);
                    o = pack(lerpf(DIM, 0.0f, k), 14, 20, 30);
                }
            }
        }
        if (!full && bx1 >= bx0) { SDL_Rect r = { bx0 * SS, ty0 * SS, (bx1 - bx0 + 1) * SS, (ty1 - ty0) * SS }; SDL_UpdateTexture(shroudTex, &r, &buf[(size_t)r.y * W2 + r.x], W2 * 4); }
    }
    if (full) SDL_UpdateTexture(shroudTex, nullptr, buf.data(), W2 * 4);
}

static Sprite cropToSprite(Gfx& g, Canvas& c, float cx, float cy, float dscale);   // defined with the buildings below; unit sprites are cropped to their painted area too

// ------------------------------------------------------------ units
// Unit art lives in art.cpp: 2x supersampled sprites (drawn at 0.5 scale, bilinear filtered), with two extra
// animation frames for anything that walks or rolls, turrets per player, and a spinning rotor for helicopters.
// The realism grade every unit and structure sprite gets once it is painted: colour pulled a little toward grey (field paint, not toy plastic) and a
// fine grime of darker and lighter flecks (dust, wear, chipped paint) that follows the sprite's own pixels, so it costs nothing after startup.
// The pattern is seeded by the art, not the animation frame, so walk / track frames never flicker. Bright emitter pixels keep their colour.
static void gradeArt(Canvas& c, u32 seed, float desat, float grime) {
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        u32& px = c.px[y * c.w + x];
        Color o = Canvas::unpack(px);
        if (o.a == 0) continue;
        float r = o.r, g = o.g, b = o.b, lum = 0.299f * r + 0.587f * g + 0.114f * b;
        float mx = std::max(r, std::max(g, b)), mn = std::min(r, std::min(g, b)), sat = mx > 0 ? (mx - mn) / mx : 0;
        float d = desat * (mx > 225 && sat > 0.35f ? 0.25f : 1.0f);   // (lights and emitters stay vivid)
        r += (lum - r) * d; g += (lum - g) * d; b += (lum - b) * d;
        u32 h = (u32)((x >> 1) * 73856093u) ^ (u32)((y >> 1) * 19349663u) ^ (seed * 83492791u);
        h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        float n = ((h & 1023) / 1023.0f - 0.5f) * 2.0f, k = 1.0f + grime * n * (0.6f + 0.4f * (lum / 255.0f));
        o.r = (u8)clampf(r * k, 0, 255); o.g = (u8)clampf(g * k, 0, 255); o.b = (u8)clampf(b * k, 0, 255);
        px = Canvas::pack(o);
    }
}

void Gfx::buildUnits() {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        Color team = PLAYER_COLOR[p];
        for (int t = 0; t < U_COUNT; t++) {
            const UnitType& u = UNITS[t];
            for (int f = 0; f < 3; f++) {
                if (f > 0 && u.kind == UK_AIR) break;   // aircraft have a single frame
                const int cs = artCanvas(t);   // (the big ships are painted at twice the detail on a bigger canvas)
                Canvas c(cs, cs);
                artUnitBody(c, t, team, f);
                gradeArt(c, 977u + (u32)t, 0.10f, 0.07f);
                Sprite s = cropToSprite(*this, c, cs / 2, cs / 2, UNIT_SCALE * artScale(t) * ART_SIZE / (float)cs);
                if (f == 0) unitBody[t][p] = s; else unitAnim[t][p][f - 1] = s;
            }
            if (u.kind == UK_AIR) { unitAnim[t][p][0] = unitBody[t][p]; unitAnim[t][p][1] = unitBody[t][p]; }
            Canvas tc(ART_SIZE, ART_SIZE);
            if (artUnitTurret(tc, t, team)) { gradeArt(tc, 1913u + (u32)t, 0.10f, 0.07f); unitTurret[t][p] = cropToSprite(*this, tc, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE * artScale(t)); }
        }
    }
    for (int p = 0; p < MAX_PLAYERS; p++) {
        for (int f = 0; f < F_COUNT; f++) { Canvas c(ART_SIZE, ART_SIZE); artCargoPlane(c, PLAYER_COLOR[p], f == F_CYBER); cargoPlane[p][f] = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE); }
        Canvas c(ART_SIZE, ART_SIZE); artChute(c, PLAYER_COLOR[p]); chute[p] = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE);
    }
    { Canvas c(ART_SIZE, ART_SIZE); artAidPlane(c); aidPlane = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE); }
    { Canvas c(ART_SIZE, ART_SIZE); artChute(c, rgb(110, 180, 240)); aidChute = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE); }
    { Canvas c(ART_SIZE, ART_SIZE); artCrate(c, false); crate = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE); }
    { Canvas c(ART_SIZE, ART_SIZE); artCrate(c, true); crateOpen = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE); }
    for (int f = 0; f < F_COUNT; f++) for (int p = 0; p < MAX_PLAYERS; p++) {
        Canvas c(ART_SIZE, ART_SIZE); artGuardDrone(c, PLAYER_COLOR[p], f == F_CYBER);
        guardDrone[f][p] = cropToSprite(*this, c, ART_SIZE / 2, ART_SIZE / 2, UNIT_SCALE);
    }
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, false); rotorDisc = fromCanvasSmooth(c, 32, 32); rotorDisc.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, true); rotorBlades = fromCanvasSmooth(c, 32, 32); rotorBlades.dscale = UNIT_SCALE; }
}

// ------------------------------------------------------------ buildings
// Structure art lives in artb.cpp. Each type is painted once (no owner baked in); the owner's colour comes from a white team mask
// tinted at draw time, and the owner's flag on the roof. Sprites are cropped to their used area to keep texture memory small.
static Sprite cropToSprite(Gfx& g, Canvas& c, float cx, float cy, float dscale) {
    int x0 = c.w, y0 = c.h, x1 = -1, y1 = -1;
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) if (Canvas::unpack(c.px[y * c.w + x]).a > 0) { x0 = std::min(x0, x); y0 = std::min(y0, y); x1 = std::max(x1, x); y1 = std::max(y1, y); }
    if (x1 < 0) return Sprite();
    Canvas sub(x1 - x0 + 1, y1 - y0 + 1);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) sub.px[(y - y0) * sub.w + (x - x0)] = c.px[y * c.w + x];
    Sprite s = g.fromCanvasSmooth(sub, cx - x0, cy - y0);
    s.dscale = dscale;
    return s;
}

void Gfx::buildBuildings() {
    for (int t = 0; t < B_COUNT; t++) {
        BArtSize sz = artBuildingSize(t);
        Canvas c(sz.w, sz.h), m(sz.w, sz.h);
        artBuilding(c, m, t);
        gradeArt(c, 3301u + (u32)t, BUILDS[t].role == BR_CIVIC ? 0.04f : 0.08f, 0.05f);
        // cropped to the painted area like the team masks: the padding round a footprint is mostly empty sky, and the
        // saved texture memory (a third or more of every structure sprite) pays for the bigger world texture
        building[t] = cropToSprite(*this, c, sz.cx, sz.cy, 1.0f / BART_K);
        buildingTeam[t] = cropToSprite(*this, m, sz.cx, sz.cy, 1.0f / BART_K);
        Canvas s(sz.w, sz.h);
        artSite(s, t);
        site[t] = cropToSprite(*this, s, sz.cx, sz.cy, 1.0f / BART_K);
        if (BUILDS[t].role == BR_NUKE || BUILDS[t].role == BR_UNUKE) { Canvas a(sz.w, sz.h); artArmed(a, t); armed[t] = cropToSprite(*this, a, sz.cx, sz.cy, 1.0f / BART_K); }
    }
    static const int RW[7] = { 1, 2, 3, 3, 4, 4, 5 }, RH[7] = { 1, 2, 2, 3, 3, 4, 3 };
    for (int i = 0; i < 7; i++) {
        int dw = RW[i] * TILE + BART_PAD_L + BART_PAD_R, dh = RH[i] * TILE + BART_PAD_T + BART_PAD_B;
        Canvas c(dw * BART_K, dh * BART_K);
        artRubble(c, RW[i], RH[i], 3000u + i);
        rubble[i] = fromCanvasSmooth(c, (BART_PAD_L + RW[i] * TILE * 0.5f) * BART_K, (BART_PAD_T + RH[i] * TILE * 0.5f) * BART_K);
        rubble[i].dscale = 1.0f / BART_K;
    }
    for (int i = 0; i < 4; i++) { Canvas c(64, 64); artProp(c, i); prop[i] = fromCanvasSmooth(c, 32, 32); prop[i].dscale = 0.5f; }
    // rotating heads of the defensive structures
    for (int i = 0; i < 4; i++) { Canvas tc(ART_SIZE, ART_SIZE); artTurretHead(tc, i); turretHead[i] = fromCanvasSmooth(tc, 32, 32); turretHead[i].dscale = 0.5f; }
}

// ------------------------------------------------------------ misc
// ------------------------------------------------------------ flags
// One flag per player, unique by silhouette and emblem as well as by colour: 0 rectangle with a chevron, 1 swallowtail
// with a roundel, 2 pennant with a diagonal bar, 3 burgee with a cross.
static void paintFlag(Canvas& c, int player, Color team) {
    const int W = Gfx::FLAG_W, H = Gfx::FLAG_H;
    Color edge = shade(team, 0.55f), hi = shade(team, 1.22f);
    Color mark = player == 2 ? rgb(38, 40, 44) : rgb(246, 248, 250);
    auto inside = [&](int x, int y) -> bool {
        float fx = (x + 0.5f) / W, fy = (y + 0.5f) / H;
        switch (player) {
        case 0: return true;                                                   // rectangle
        case 1: return !(fx > 0.72f && std::abs(fy - 0.5f) < (fx - 0.72f) * 1.7f);   // swallowtail
        case 2: return std::abs(fy - 0.5f) < 0.5f * (1.0f - fx);               // pennant
        default: return std::abs(fy - 0.5f) < 0.5f - 0.14f * fx;               // burgee: narrows toward the fly
        }
    };
    for (int y = 0; y < H; y++) for (int x = 0; x < W; x++) {
        if (!inside(x, y)) continue;
        Color k = team;
        if (y < 2) k = hi;                    // lit top edge
        else if (y >= H - 2) k = shade(team, 0.78f);
        if (x == 0) k = edge;                 // hoist
        c.set(x, y, k);
    }
    switch (player) {
    case 0: for (int i = 0; i < 6; i++) { c.set(6 + i, 3 + i, mark); c.set(7 + i, 3 + i, mark); c.set(6 + i, H - 4 - i, mark); c.set(7 + i, H - 4 - i, mark); } break;   // chevron >
    case 1: c.ring(11.5f, H / 2.0f, 4.6f, 1.6f, mark); c.fillCircle(11.5f, H / 2.0f, 1.4f, mark); break;
    case 2: for (int x = 3; x < 15; x++) { int y = H / 2 + (x - 9) / 2; c.set(x, y - 1, mark); c.set(x, y, mark); } break;
    default: c.fillRect(8, 5, 8, 2, mark); c.fillRect(11, 2, 2, 11, mark); break;   // cross
    }
    // outline only around the cloth, so the mast gap stays clean
    c.outline(rgb(10, 12, 14, 200));
}

void Gfx::drawFlag(float px, float py, int player, float scale, float phase) {
    if (player < 0 || player >= MAX_PLAYERS) return;
    player = slotOf(player);
    if (!flag[player].tex) return;
    float mastH = 27 * scale;
    // mast: dark shaft with a lit edge, a small base plate and a finial
    fill((int)std::lround(px) - 1, (int)std::lround(py - mastH), 2, (int)std::lround(mastH) + 1, rgb(28, 30, 34));
    fill((int)std::lround(px), (int)std::lround(py - mastH), 1, (int)std::lround(mastH), rgb(150, 156, 166));
    fill((int)std::lround(px) - 3, (int)std::lround(py) - 1, 6, 3, rgb(40, 42, 48));
    fillCircle(px, py - mastH - 1, 1.9f * std::max(scale, 0.8f), rgb(226, 200, 110));
    // cloth in 2px slices, each displaced by a travelling wave; the free end swings more than the hoist
    const Sprite& s = flag[player];
    float cw = s.w * scale, ch = s.h * scale;
    float x0 = px + 1.0f, y0 = py - mastH + 1.0f;
    SDL_SetTextureColorMod(s.tex, 255, 255, 255); SDL_SetTextureAlphaMod(s.tex, 255);
    const int step = 2;
    for (int sx = 0; sx < s.w; sx += step) {
        float k = (float)sx / s.w;
        float dy = std::sin(phase - sx * 0.42f) * 1.7f * k * scale;
        float dxs = -std::sin(phase - sx * 0.42f + 0.6f) * 0.6f * k * scale;
        SDL_Rect src = { sx, 0, std::min(step, s.w - sx), s.h };
        SDL_FRect dst = { x0 + sx * scale + dxs, y0 + dy, src.w * scale + 0.6f, ch };
        SDL_RenderCopyF(ren, s.tex, &src, &dst);
    }
    (void)cw;
}

void Gfx::buildMisc() {
    Rng r(31337);
    for (int p = 0; p < MAX_PLAYERS; p++) { Canvas c(FLAG_W, FLAG_H); paintFlag(c, p, PLAYER_COLOR[p]); flag[p] = fromCanvas(c, 0, 0); }
    for (int v = 0; v < 3; v++) {
        Canvas c(40, 40);
        // supply crates stacked
        int n = 6 - v;
        for (int k = 0; k < n; k++) {
            int x = 6 + (k % 3) * 10 + r.range(-1, 1), y = 8 + (k / 3) * 12 + r.range(-1, 1);
            c.fillRect(x + 2, y + 2, 10, 10, rgb(20, 20, 20, 90));
            c.panel(x, y, 10, 10, rgb(150, 120, 70), 1);
            c.fillRect(x + 4, y + 1, 2, 8, rgb(90, 140, 60));
        }
        c.outline(rgb(10, 12, 14, 180));
        pile[v] = fromCanvas(c, 20, 20);
    }
    { Canvas c(256, 256); for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) { float d = std::sqrt((x + 0.5f - 128) * (x + 0.5f - 128) + (y + 0.5f - 128) * (y + 0.5f - 128)); float a = clampf(128 - d, 0, 1); if (a > 0) c.px[y * 256 + x] = Canvas::pack(Color{255, 255, 255, (u8)(a * 255)}); } disc = fromCanvasSmooth(c, 128, 128); }
    { Canvas c(256, 256); for (int y = 0; y < 256; y++) for (int x = 0; x < 256; x++) { float dx = (x + 0.5f - 128) / 128.0f, dy = (y + 0.5f - 128) / 128.0f; float d = std::sqrt(dx * dx + dy * dy); float a = clampf((d - 0.62f) / 0.75f, 0, 1); a = a * a * (3 - 2 * a); if (a > 0) c.px[y * 256 + x] = Canvas::pack(Color{4, 6, 10, (u8)(a * 105)}); } vignette = fromCanvasSmooth(c, 0, 0); }
    { Canvas c(32, 32); c.glow(16, 16, 16, rgb(255, 255, 255, 255)); for (auto& px : c.px) { Color k = Canvas::unpack(px); k.r = k.g = k.b = 255; px = Canvas::pack(k); } blob = fromCanvas(c, 16, 16); }
    { Canvas c(24, 16); for (int y = 0; y < 16; y++) for (int x = 0; x < 24; x++) { float dx = (x - 12) / 12.0f, dy = (y - 8) / 8.0f; float d = dx * dx + dy * dy; if (d < 1) c.set(x, y, rgb(0, 0, 0, (u8)(110 * (1 - d)))); } shadowSmall = fromCanvas(c, 12, 8); }
    { Canvas c(40, 26); for (int y = 0; y < 26; y++) for (int x = 0; x < 40; x++) { float dx = (x - 20) / 20.0f, dy = (y - 13) / 13.0f; float d = dx * dx + dy * dy; if (d < 1) c.set(x, y, rgb(0, 0, 0, (u8)(120 * (1 - d)))); } shadowLarge = fromCanvas(c, 20, 13); }
}

void Gfx::buildMinimap() {}   // the overview is cut from the baked terrain in bakeTerrain()
