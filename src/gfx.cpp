#include "gfx.h"
#include "art.h"
#include "artb.h"
#include "terrain.h"

Gfx g_gfx;

const Color PLAYER_COLOR[MAX_PLAYERS] = { rgb(70, 170, 255), rgb(235, 70, 60), rgb(240, 205, 60), rgb(90, 215, 95) };
int g_colorSlot[MAX_PLAYERS] = { 0, 1, 2, 3 };

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
    SDL_SetTextureColorMod(s.tex, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    if (angle == 0) SDL_RenderCopy(ren, s.tex, nullptr, &dst);
    else SDL_RenderCopyEx(ren, s.tex, nullptr, &dst, angle * 57.29578f, &c, SDL_FLIP_NONE);
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
    SDL_FRect dst = { cx - w * 0.5f, cy - h * 0.5f, w, h };
    SDL_SetTextureColorMod(s.tex, mod.r, mod.g, mod.b);
    SDL_SetTextureAlphaMod(s.tex, alpha);
    if (angle == 0) SDL_RenderCopyF(ren, s.tex, nullptr, &dst);
    else SDL_RenderCopyExF(ren, s.tex, nullptr, &dst, angle * 57.29578f, nullptr, SDL_FLIP_NONE);
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
    worldTerrain = fromCanvas(c, 0, 0);
    Canvas mini(232, 232);
    terrainOverview(c, mini);
    minimapTerrain = fromCanvasSmooth(mini, 0, 0);
    // fog mask, filtered bilinearly at creation time (hint is read when the texture is created)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    shroudTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MAP_W * SHROUD_SS, MAP_H * SHROUD_SS);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (shroudTex) SDL_SetTextureBlendMode(shroudTex, SDL_BLENDMODE_BLEND);
}

void Gfx::updateShroud(const std::vector<u8>& ex) {
    if (!shroudTex) return;
    // 0 = explored, 255 = never seen. The binary map is upsampled with bilinear weights and the edge is pushed around by value noise, so
    // the boundary of what you have seen is a ragged, cloudy fringe instead of a staircase of tiles; the unseen area carries faint mottling.
    const int SS = SHROUD_SS, W2 = MAP_W * SS, H2 = MAP_H * SS;
    static std::vector<u32> buf; static std::vector<float> noise;
    if (noise.empty()) { Rng r(99); noise.resize(64 * 64); for (auto& v : noise) v = r.f(); }
    auto vn = [&](float x, float y) {
        int ix = (int)std::floor(x), iy = (int)std::floor(y); float fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
        auto at = [&](int a, int b) { return noise[(b & 63) * 64 + (a & 63)]; };
        return lerpf(lerpf(at(ix, iy), at(ix + 1, iy), fx), lerpf(at(ix, iy + 1), at(ix + 1, iy + 1), fx), fy);
    };
    buf.assign((size_t)W2 * H2, 0);
    for (int y = 0; y < H2; y++) for (int x = 0; x < W2; x++) {
        float fx = (x + 0.5f) / SS - 0.5f, fy = (y + 0.5f) / SS - 0.5f;
        int x0 = (int)std::floor(fx), y0 = (int)std::floor(fy); float tx = fx - x0, ty = fy - y0;
        auto E = [&](int a, int b) { return ex[clampi(b, 0, MAP_H - 1) * MAP_W + clampi(a, 0, MAP_W - 1)] ? 1.0f : 0.0f; };
        float v = lerpf(lerpf(E(x0, y0), E(x0 + 1, y0), tx), lerpf(E(x0, y0 + 1), E(x0 + 1, y0 + 1), tx), ty);
        if (v <= 0.0f) { buf[(size_t)y * W2 + x] = (u32)(238 + (int)(vn(x * 0.35f, y * 0.35f) * 14)) << 24 | 12u << 16 | 8u << 8 | 5u; continue; }   // unseen: mottled dark
        if (v >= 1.0f) continue;                                                                                                                   // seen: clear
        float n = vn(x * 0.55f + 11, y * 0.55f + 5) * 0.65f + vn(x * 1.4f, y * 1.4f) * 0.35f;
        float s = clampf((v - 0.5f) * 1.6f + (n - 0.5f) * 0.9f + 0.5f, 0, 1);
        s = s * s * (3 - 2 * s);
        buf[(size_t)y * W2 + x] = (u32)((1.0f - s) * 242) << 24 | 12u << 16 | 8u << 8 | 5u;
    }
    SDL_UpdateTexture(shroudTex, nullptr, buf.data(), W2 * 4);
}

// ------------------------------------------------------------ units
// Unit art lives in art.cpp: 2x supersampled sprites (drawn at 0.5 scale, bilinear filtered), with two extra
// animation frames for anything that walks or rolls, turrets per player, and a spinning rotor for helicopters.
void Gfx::buildUnits() {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        Color team = PLAYER_COLOR[p];
        for (int t = 0; t < U_COUNT; t++) {
            const UnitType& u = UNITS[t];
            for (int f = 0; f < 3; f++) {
                if (f > 0 && u.kind == UK_AIR) break;   // aircraft have a single frame
                Canvas c(ART_SIZE, ART_SIZE);
                artUnitBody(c, t, team, f);
                Sprite s = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2);
                s.dscale = UNIT_SCALE * artScale(t);
                if (f == 0) unitBody[t][p] = s; else unitAnim[t][p][f - 1] = s;
            }
            if (u.kind == UK_AIR) { unitAnim[t][p][0] = unitBody[t][p]; unitAnim[t][p][1] = unitBody[t][p]; }
            Canvas tc(ART_SIZE, ART_SIZE);
            if (artUnitTurret(tc, t, team)) { unitTurret[t][p] = fromCanvasSmooth(tc, ART_SIZE / 2, ART_SIZE / 2); unitTurret[t][p].dscale = UNIT_SCALE * artScale(t); }
        }
    }
    for (int p = 0; p < MAX_PLAYERS; p++) {
        for (int f = 0; f < F_COUNT; f++) { Canvas c(ART_SIZE, ART_SIZE); artCargoPlane(c, PLAYER_COLOR[p], f == F_CYBER); cargoPlane[p][f] = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); cargoPlane[p][f].dscale = UNIT_SCALE; }
        Canvas c(ART_SIZE, ART_SIZE); artChute(c, PLAYER_COLOR[p]); chute[p] = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); chute[p].dscale = UNIT_SCALE;
    }
    { Canvas c(ART_SIZE, ART_SIZE); artAidPlane(c); aidPlane = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); aidPlane.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artChute(c, rgb(110, 180, 240)); aidChute = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); aidChute.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artCrate(c, false); crate = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); crate.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artCrate(c, true); crateOpen = fromCanvasSmooth(c, ART_SIZE / 2, ART_SIZE / 2); crateOpen.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, false); rotorDisc = fromCanvasSmooth(c, 32, 32); rotorDisc.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, true); rotorBlades = fromCanvasSmooth(c, 32, 32); rotorBlades.dscale = UNIT_SCALE; }
}

// ------------------------------------------------------------ buildings
// Structure art lives in artb.cpp. Each type is painted once (no owner baked in); the owner's colour comes from a white team mask
// tinted at draw time, and the owner's flag on the roof. The mask is cropped to its used area to keep texture memory small.
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
        building[t] = fromCanvasSmooth(c, sz.cx, sz.cy); building[t].dscale = 1.0f / BART_K;
        buildingTeam[t] = cropToSprite(*this, m, sz.cx, sz.cy, 1.0f / BART_K);
        Canvas s(sz.w, sz.h);
        artSite(s, t);
        site[t] = fromCanvasSmooth(s, sz.cx, sz.cy); site[t].dscale = 1.0f / BART_K;
        if (BUILDS[t].role == BR_NUKE) { Canvas a(sz.w, sz.h); artArmed(a, t); armed[t] = fromCanvasSmooth(a, sz.cx, sz.cy); armed[t].dscale = 1.0f / BART_K; }
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
    { Canvas c(32, 32); c.glow(16, 16, 16, rgb(255, 255, 255, 255)); for (auto& px : c.px) { Color k = Canvas::unpack(px); k.r = k.g = k.b = 255; px = Canvas::pack(k); } blob = fromCanvas(c, 16, 16); }
    { Canvas c(24, 16); for (int y = 0; y < 16; y++) for (int x = 0; x < 24; x++) { float dx = (x - 12) / 12.0f, dy = (y - 8) / 8.0f; float d = dx * dx + dy * dy; if (d < 1) c.set(x, y, rgb(0, 0, 0, (u8)(110 * (1 - d)))); } shadowSmall = fromCanvas(c, 12, 8); }
    { Canvas c(40, 26); for (int y = 0; y < 26; y++) for (int x = 0; x < 40; x++) { float dx = (x - 20) / 20.0f, dy = (y - 13) / 13.0f; float d = dx * dx + dy * dy; if (d < 1) c.set(x, y, rgb(0, 0, 0, (u8)(120 * (1 - d)))); } shadowLarge = fromCanvas(c, 20, 13); }
}

void Gfx::buildMinimap() {}   // the overview is cut from the baked terrain in bakeTerrain()
