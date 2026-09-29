#include "gfx.h"
#include "art.h"

Gfx g_gfx;

const Color PLAYER_COLOR[MAX_PLAYERS] = { rgb(70, 170, 255), rgb(235, 70, 60), rgb(240, 205, 60), rgb(90, 215, 95) };

// Faction palettes: Cyber is cold slate + cyan light; Clanker is olive steel + rust + hazard yellow
static const Color CY_HULL = rgb(50, 64, 90), CY_PANEL = rgb(96, 128, 168), CY_DARK = rgb(28, 36, 52), CY_GLOW = rgb(110, 230, 255), CY_WHITE = rgb(210, 235, 245);
static const Color CK_HULL = rgb(98, 102, 68), CK_PANEL = rgb(140, 132, 100), CK_DARK = rgb(46, 46, 36), CK_RUST = rgb(150, 86, 46), CK_YEL = rgb(222, 178, 60), CK_STEEL = rgb(112, 112, 108);

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
    onResize();
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    { Canvas c(2, 2); c.fillRect(0, 0, 2, 2, rgb(255, 255, 255)); white = fromCanvas(c, 0, 0).tex; }
    buildFont(); buildTiles(); bakeTerrain(); buildUnits(); buildBuildings(); buildMisc(); buildMinimap();
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
    if (!blob.tex) return;
    SDL_SetTextureBlendMode(blob.tex, SDL_BLENDMODE_ADD);
    draw(blob, cx, cy, 0, r / 16.0f, c, c.a);
    SDL_SetTextureBlendMode(blob.tex, SDL_BLENDMODE_BLEND);
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

// ------------------------------------------------------------ tiles
static void speckle(Canvas& c, Rng& r, Color base, Color a, Color b, int n) {
    c.fillRect(0, 0, c.w, c.h, base);
    for (int i = 0; i < n; i++) c.set(r.range(0, c.w - 1), r.range(0, c.h - 1), r.next() & 1 ? a : b);
}
static void paintTile(Canvas& c, int type, int v, Rng& r) {
    int W = c.w, H = c.h;
    switch (type) {
    case T_GRASS:
        speckle(c, r, rgb(74, 108, 50), rgb(86, 122, 58), rgb(62, 94, 44), W * H / 10 + v * 10);
        for (int k = 0; k < W * H / 170; k++) { int x = r.range(1, W - 2), y = r.range(1, H - 1); c.set(x, y, rgb(96, 134, 62)); c.set(x, y - 1, rgb(96, 134, 62)); }
        break;
    case T_GRASS2:
        speckle(c, r, rgb(90, 116, 52), rgb(104, 130, 60), rgb(76, 100, 46), W * H / 10);
        for (int k = 0; k < W * H / 200; k++) { int x = r.range(1, W - 2), y = r.range(1, H - 2); c.set(x, y, rgb(122, 140, 70)); }
        break;
    case T_DIRT:
        speckle(c, r, rgb(124, 100, 68), rgb(138, 112, 76), rgb(106, 84, 56), W * H / 9);
        for (int k = 0; k < W * H / 256; k++) c.fillCircle(r.range(3, W - 4), r.range(3, H - 4), r.f(1.5f, 3), rgb(112, 90, 60));
        break;
    case T_SAND:
        speckle(c, r, rgb(190, 172, 124), rgb(204, 186, 138), rgb(174, 156, 110), W * H / 8);
        break;
    case T_ROAD:
        speckle(c, r, rgb(84, 82, 78), rgb(92, 90, 86), rgb(74, 72, 68), W * H / 12);
        if (v == 0) for (int k = 0; k < 3; k++) c.set(r.range(0, W - 1), r.range(0, H - 1), rgb(120, 116, 110));
        break;
    case T_WATER:
        speckle(c, r, rgb(40, 88, 132), rgb(46, 98, 144), rgb(36, 80, 122), W * H / 17);
        for (int k = 0; k < 3; k++) { int y = r.range(2, H - 3), x = r.range(0, W - 12); c.line(x, y, x + r.range(4, 10), y, rgb(90, 150, 190)); }
        break;
    case T_ROCK: {
        speckle(c, r, rgb(92, 88, 80), rgb(100, 96, 88), rgb(80, 76, 70), W * H / 14);
        // angular slabs: a quad with a lit top edge and a dark base
        for (int k = 0; k < 5; k++) {
            int x = r.range(-4, W - 10), y = r.range(-4, H - 10), w = r.range(9, 18), h = r.range(7, 14);
            int sk = r.range(-4, 4);
            Color top = rgb(124, 120, 112), face = rgb(106, 102, 94), sideC = rgb(70, 66, 60);
            for (int j = 0; j < h; j++) { int off = sk * j / std::max(1, h); int ww = w - std::abs(off); for (int i = 0; i < ww; i++) c.set(x + i + std::max(0, off), y + j, j < 2 ? top : (j > h - 3 ? sideC : face)); }
            c.line(x, y + h, x + w, y + h + 1, rgb(48, 46, 42));
        }
        for (int k = 0; k < 3; k++) { int x = r.range(2, W - 3), y = r.range(2, H - 3); c.line(x, y, x + r.range(-6, 6), y + r.range(2, 8), rgb(54, 52, 48)); }
        break;
    }
    case T_TREE: {
        speckle(c, r, rgb(62, 92, 44), rgb(72, 104, 50), rgb(52, 80, 38), W * H / 12);
        if (H < 20) break;
        int n = r.range(3, 4);
        float cx[4], cy[4], cr[4];
        for (int k = 0; k < n; k++) { cx[k] = r.f(9, 23); cy[k] = r.f(9, 23); cr[k] = r.f(6, 9); }
        for (int k = 0; k < n; k++) c.fillCircle(cx[k] + 3, cy[k] + 4, cr[k], rgb(26, 44, 26, 190));
        for (int k = 0; k < n; k++) { c.fillCircle(cx[k], cy[k], cr[k], rgb(34, 82, 40)); c.fillCircle(cx[k] - cr[k] * 0.25f, cy[k] - cr[k] * 0.3f, cr[k] * 0.6f, rgb(50, 106, 50)); c.fillCircle(cx[k] - cr[k] * 0.4f, cy[k] - cr[k] * 0.45f, cr[k] * 0.28f, rgb(80, 138, 66)); }
        for (int k = 0; k < 14; k++) c.set(r.range(1, 30), r.range(1, 30), rgb(40, 92, 44));
        break;
    }
    }
}

void Gfx::buildTiles() {
    Rng r(4242);
    for (int v = 0; v < 4; v++) for (int t = 0; t < T_COUNT; t++) {
        Canvas c(TILE, TILE);
        paintTile(c, t, v, r);
        tilePx[t][v] = c.px;
        tiles[t][v] = fromCanvas(c, 0, 0);
    }
    // edge strips: the tile's own texture fading out, drawn over lower-priority neighbours
    for (int t = 0; t < T_COUNT; t++) {
        Canvas c(TILE, 12);
        paintTile(c, t, 0, r);
        for (int y = 0; y < c.h; y++) {
            float a = 1.0f - (y + 0.5f) / c.h; a = a * a;
            for (int x = 0; x < c.w; x++) { Color k = Canvas::unpack(c.px[y * c.w + x]); k.a = (u8)(k.a * a * (0.55f + 0.45f * r.f())); c.px[y * c.w + x] = Canvas::pack(k); }
        }
        edgeTile[t] = fromCanvas(c, 16, 16);
    }
}


// ------------------------------------------------------------ baked world terrain
static float nhash(int x, int y, u32 seed) {
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u; h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}
static float nnoise(float x, float y, u32 seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy; fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    return lerpf(lerpf(nhash(ix, iy, seed), nhash(ix + 1, iy, seed), fx), lerpf(nhash(ix, iy + 1, seed), nhash(ix + 1, iy + 1, seed), fx), fy);
}
static void paintTreeCanopy(Canvas& c, Rng& r) {
    int n = r.range(3, 4);
    float cx[4], cy[4], cr[4];
    for (int k = 0; k < n; k++) { cx[k] = r.f(9, 23); cy[k] = r.f(9, 23); cr[k] = r.f(6, 9); }
    for (int k = 0; k < n; k++) c.fillCircle(cx[k] + 3, cy[k] + 4, cr[k], rgb(14, 30, 16, 120));
    for (int k = 0; k < n; k++) { c.fillCircle(cx[k], cy[k], cr[k], rgb(34, 82, 40)); c.fillCircle(cx[k] - cr[k] * 0.25f, cy[k] - cr[k] * 0.3f, cr[k] * 0.6f, rgb(50, 106, 50)); c.fillCircle(cx[k] - cr[k] * 0.4f, cy[k] - cr[k] * 0.45f, cr[k] * 0.28f, rgb(80, 138, 66)); }
    for (int k = 0; k < 14; k++) { int x = r.range(1, 30), y = r.range(1, 30); if (Canvas::unpack(c.px[y * c.w + x]).a > 250) c.set(x, y, rgb(40, 92, 44)); }
}

void Gfx::bakeTerrain() {
    const int W = WORLD_W, H = WORLD_H;
    Canvas c(W, H);
    auto typeAt = [&](float fx, float fy) -> u8 {
        int tx = clampi((int)std::floor(fx / TILE), 0, MAP_W - 1), ty = clampi((int)std::floor(fy / TILE), 0, MAP_H - 1);
        u8 t = g_map.tile(tx, ty);
        return t == T_TREE ? (u8)T_GRASS : t;
    };
    auto sample = [&](float fx, float fy, int px, int py) -> Color {
        int tx = clampi((int)std::floor(fx / TILE), 0, MAP_W - 1), ty = clampi((int)std::floor(fy / TILE), 0, MAP_H - 1);
        u8 t = typeAt(fx, fy);
        int v = g_map.variant[ty * MAP_W + tx] % 4;
        return Canvas::unpack(tilePx[t][v][(py & (TILE - 1)) * TILE + (px & (TILE - 1))]);
    };
    static const float JX[4] = { -3.5f, 3.5f, -2.0f, 2.5f }, JY[4] = { -2.5f, -3.0f, 3.5f, 2.0f };
    for (int py = 0; py < H; py++) for (int px = 0; px < W; px++) {
        // domain warp so tile borders become organic curves, then average four jittered lookups for soft blends
        float wx = px + (nnoise(px * 0.045f, py * 0.045f, 11) - 0.5f) * 20.0f + (nnoise(px * 0.15f, py * 0.15f, 12) - 0.5f) * 6.0f;
        float wy = py + (nnoise(px * 0.045f + 50, py * 0.045f + 50, 13) - 0.5f) * 20.0f + (nnoise(px * 0.15f, py * 0.15f + 30, 14) - 0.5f) * 6.0f;
        float r = 0, g = 0, b = 0;
        int rockN = 0, waterN = 0;
        for (int k = 0; k < 4; k++) {
            u8 t = typeAt(wx + JX[k], wy + JY[k]);
            Color s = sample(wx + JX[k], wy + JY[k], px, py);
            r += s.r; g += s.g; b += s.b;
            if (t == T_ROCK) rockN++;
            if (t == T_WATER) waterN++;
        }
        r *= 0.25f; g *= 0.25f; b *= 0.25f;
        // low-frequency tint breaks up tiling, plus a faint light gradient (light from the north-west)
        float tint = 0.90f + 0.20f * nnoise(px * 0.012f, py * 0.012f, 21) + 0.06f * (nnoise(px * 0.05f, py * 0.05f, 22) - 0.5f);
        // shoreline: pixels that are land but touch water get a wet dark rim, water touching land a light foam
        if (waterN > 0 && waterN < 4) { float k = waterN / 4.0f; r = lerpf(r, 170, 0.35f * (1 - std::abs(k - 0.5f) * 2)); g = lerpf(g, 205, 0.35f * (1 - std::abs(k - 0.5f) * 2)); b = lerpf(b, 215, 0.35f * (1 - std::abs(k - 0.5f) * 2)); }
        // cliffs: rock edges lit from the north-west, shadowed on the south-east
        if (rockN > 0 && rockN < 4) { float sh = (rockN <= 2) ? 0.78f : 1.0f; r *= sh; g *= sh; b *= sh; }
        c.px[py * W + px] = Canvas::pack(Color{(u8)clampf(r * tint, 0, 255), (u8)clampf(g * tint, 0, 255), (u8)clampf(b * tint, 0, 255), 255});
    }
    // soft drop shadows from rock cliffs onto the ground to their south-east
    {
        std::vector<u32> src = c.px;
        for (int ty = 1; ty < MAP_H - 1; ty++) for (int tx = 1; tx < MAP_W - 1; tx++) {
            if (g_map.tile(tx, ty) == T_ROCK || g_map.tile(tx, ty) == T_WATER) continue;
            bool rockNW = g_map.tile(tx - 1, ty) == T_ROCK || g_map.tile(tx, ty - 1) == T_ROCK;
            if (!rockNW) continue;
            for (int j = 0; j < 8; j++) for (int i = 0; i < TILE; i++) {
                if (g_map.tile(tx, ty - 1) != T_ROCK) break;
                Color k = Canvas::unpack(c.px[(ty * TILE + j) * W + tx * TILE + i]);
                float f = 1.0f - 0.28f * (1.0f - j / 8.0f);
                c.px[(ty * TILE + j) * W + tx * TILE + i] = Canvas::pack(Color{(u8)(k.r * f), (u8)(k.g * f), (u8)(k.b * f), 255});
            }
        }
    }
    // trees: canopies composited over the grass, four variants
    Rng tr(777);
    Canvas canopy[4] = { Canvas(TILE, TILE), Canvas(TILE, TILE), Canvas(TILE, TILE), Canvas(TILE, TILE) };
    for (int v = 0; v < 4; v++) paintTreeCanopy(canopy[v], tr);
    for (int ty = 0; ty < MAP_H; ty++) for (int tx = 0; tx < MAP_W; tx++) {
        if (g_map.tile(tx, ty) != T_TREE) continue;
        Canvas& cv = canopy[g_map.variant[ty * MAP_W + tx] % 4];
        for (int j = 0; j < TILE; j++) for (int i = 0; i < TILE; i++) {
            Color k = Canvas::unpack(cv.px[j * TILE + i]);
            if (k.a == 0) continue;
            int X = tx * TILE + i, Y = ty * TILE + j;
            Color o = Canvas::unpack(c.px[Y * W + X]);
            float a = k.a / 255.0f;
            c.px[Y * W + X] = Canvas::pack(Color{(u8)lerpf(o.r, k.r, a), (u8)lerpf(o.g, k.g, a), (u8)lerpf(o.b, k.b, a), 255});
        }
    }
    worldTerrain = fromCanvas(c, 0, 0);
    // fog mask, filtered bilinearly at creation time (hint is read when the texture is created)
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    shroudTex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, MAP_W, MAP_H);
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    if (shroudTex) SDL_SetTextureBlendMode(shroudTex, SDL_BLENDMODE_BLEND);
}

void Gfx::updateShroud(const std::vector<u8>& ex) {
    if (!shroudTex) return;
    // 0 = explored, 255 = never seen; explored tiles bordering the shroud get a partial veil so the edge reads as a gradient
    static std::vector<u32> buf;
    buf.assign((size_t)MAP_W * MAP_H, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        u8 a = ex[y * MAP_W + x] ? 0 : 245;
        buf[y * MAP_W + x] = (u32)a << 24 | 10u << 16 | 6u << 8 | 4u;
    }
    SDL_UpdateTexture(shroudTex, nullptr, buf.data(), MAP_W * 4);
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
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, false); rotorDisc = fromCanvasSmooth(c, 32, 32); rotorDisc.dscale = UNIT_SCALE; }
    { Canvas c(ART_SIZE, ART_SIZE); artRotor(c, true); rotorBlades = fromCanvasSmooth(c, 32, 32); rotorBlades.dscale = UNIT_SCALE; }
}

// ------------------------------------------------------------ buildings
static void hazardStripe(Canvas& c, int x, int y, int w, int h) {
    for (int i = 0; i < w; i++) for (int j = 0; j < h; j++) c.set(x + i, y + j, ((i + j) / 3) % 2 ? CK_YEL : CK_DARK);
}
static void lightStrip(Canvas& c, int x, int y, int w, int h, Color glow) {
    c.fillRect(x, y, w, h, glow);
    Color g = glow; g.a = 90;
    c.fillRect(x - 1, y - 1, w + 2, h + 2, g);
}

void Gfx::buildBuildings() {
    Rng r(777);
    for (int p = 0; p < MAX_PLAYERS; p++) {
        Color team = PLAYER_COLOR[p];
        for (int t = 0; t < B_COUNT; t++) {
            const BuildType& b = BUILDS[t];
            bool cy = b.faction == F_CYBER;
            int W = b.w * TILE, H = b.h * TILE;
            Canvas c(W, H);
            // every structure role has its own material tone inside the faction's family
            static const Color CY_BASE[11] = { rgb(70, 90, 124), rgb(36, 90, 100), rgb(84, 90, 102), rgb(60, 82, 122), rgb(54, 62, 80), rgb(46, 56, 74), rgb(46, 50, 76), rgb(72, 92, 116), rgb(64, 78, 104) };
            static const Color CY_ROOF[11] = { rgb(128, 160, 204), rgb(70, 130, 136), rgb(124, 134, 150), rgb(104, 134, 182), rgb(94, 106, 130), rgb(70, 82, 104), rgb(92, 100, 140), rgb(110, 132, 160), rgb(96, 116, 150), rgb(60, 70, 96), rgb(96, 100, 112) };
            static const Color CY_ACC[11] = { rgb(110, 230, 255), rgb(120, 255, 214), rgb(255, 190, 92), rgb(110, 230, 255), rgb(110, 230, 255), rgb(214, 240, 255), rgb(206, 232, 255), rgb(110, 230, 255), rgb(140, 255, 255), rgb(255, 214, 90), rgb(255, 96, 80) };
            static const Color CK_BASE[11] = { rgb(122, 112, 84), rgb(94, 90, 82), rgb(112, 92, 64), rgb(92, 104, 68), rgb(88, 92, 86), rgb(100, 98, 90), rgb(106, 76, 58), rgb(102, 102, 88), rgb(90, 98, 82), rgb(66, 62, 58), rgb(96, 100, 88) };
            static const Color CK_ROOF[11] = { rgb(160, 148, 112), rgb(128, 124, 112), rgb(152, 130, 92), rgb(130, 140, 94), rgb(126, 122, 106), rgb(120, 118, 108), rgb(152, 114, 90), rgb(126, 126, 108), rgb(116, 124, 104), rgb(90, 84, 78), rgb(132, 136, 120) };
            Color base = cy ? CY_BASE[b.role] : CK_BASE[b.role];
            Color plate = shade(base, 0.74f);
            Color roof = cy ? CY_ROOF[b.role] : CK_ROOF[b.role];
            Color dark = cy ? CY_DARK : CK_DARK;
            Color accent = cy ? CY_ACC[b.role] : CK_YEL;
            // foundation plate with a 2px margin
            c.panel(1, 1, W - 2, H - 2, plate, 1);
            for (int i = 0; i < W * H / 40; i++) c.set(r.range(2, W - 3), r.range(2, H - 3), shade(plate, 0.9f));
            switch (b.role) {
            case BR_HQ:
                c.panel(8, 8, W - 16, H - 16, base, 2);
                c.panel(W / 2 - 22, H / 2 - 22, 44, 44, roof, 2);
                if (cy) { c.ring(W / 2, H / 2, 14, 3, accent); c.glow(W / 2, H / 2, 20, rgb(110, 230, 255, 120)); c.fillCircle(W / 2, H / 2, 6, CY_WHITE);
                          lightStrip(c, 12, 12, W - 24, 2, accent); lightStrip(c, 12, H - 14, W - 24, 2, accent); }
                else { c.fillRect(W / 2 - 8, H / 2 - 8, 16, 16, CK_STEEL); c.line(W / 2, H / 2 - 20, W / 2, H / 2 - 40, CK_STEEL, 2); c.fillCircle(W / 2, H / 2 - 40, 3, rgb(230, 60, 50));
                       hazardStripe(c, 8, H - 14, W - 16, 4); c.fillRect(10, 10, 20, 12, CK_RUST); c.fillRect(W - 30, 10, 20, 12, CK_RUST); }
                c.fillRect(10, H - 26, 24, 12, dark); // door
                break;
            case BR_POWER:
                c.panel(6, 6, W - 12, H - 12, base, 2);
                if (cy) { c.fillCircle(W / 2, H / 2, 18, dark); c.ring(W / 2, H / 2, 16, 4, accent); c.ring(W / 2, H / 2, 9, 2, CY_WHITE); c.glow(W / 2, H / 2, 22, rgb(110, 230, 255, 140));
                          lightStrip(c, 10, H - 10, W - 20, 2, accent); }
                else { for (int k = 0; k < 3; k++) { int x = 16 + k * 26; c.fillCircle(x, 20, 8, CK_STEEL); c.fillCircle(x, 20, 5, dark); c.fillCircle(x - 2, 18, 2, rgb(150, 150, 150)); }
                       c.fillRect(10, 36, W - 20, 14, CK_RUST); hazardStripe(c, 10, H - 12, W - 20, 4); c.fillRect(W - 26, 34, 10, 18, dark); }
                break;
            case BR_SUPPLY:
                c.panel(6, 6, W - 12, H - 12, base, 2);
                c.panel(12, 12, W - 24, H - 40, roof, 2);          // warehouse roof
                for (int k = 16; k < W - 16; k += 8) c.fillRect(k, 14, 1, H - 44, shade(roof, 0.8f));
                c.fillRect(14, H - 26, W - 28, 16, dark);          // loading bay
                if (cy) { lightStrip(c, 14, H - 12, W - 28, 2, accent); c.fillRect(W - 24, 14, 8, 8, accent); }
                else { hazardStripe(c, 14, H - 28, W - 28, 3); c.fillCircle(W - 18, 22, 5, CK_RUST); c.fillCircle(W - 30, 22, 5, CK_RUST); }
                break;
            case BR_BARRACKS:
                c.panel(5, 5, W - 10, H - 10, base, 2);
                c.panel(10, 9, W - 20, H - 30, roof, 1);
                c.fillRect(W / 2 - 7, H - 20, 14, 10, dark);      // door
                if (cy) { lightStrip(c, 12, 11, W - 24, 2, accent); c.fillRect(14, H - 18, 8, 6, accent); c.fillRect(W - 22, H - 18, 8, 6, accent); }
                else { c.fillRect(12, 11, 12, 8, CK_RUST); c.fillRect(W - 24, 11, 12, 8, CK_RUST); hazardStripe(c, 10, H - 9, W - 20, 3); c.fillRect(W - 14, H - 20, 6, 10, rgb(60, 110, 60)); }
                break;
            case BR_FACTORY:
                c.panel(5, 5, W - 10, H - 10, base, 2);
                c.panel(8, 8, W - 16, H - 40, roof, 2);
                for (int k = 12; k < W - 12; k += 12) c.fillRect(k, 10, 8, H - 44, shade(roof, cy ? 1.15f : 0.85f));  // roof ribs
                c.fillRect(W / 2 - 18, H - 30, 36, 22, dark);      // bay door
                for (int k = 0; k < 4; k++) c.fillRect(W / 2 - 18, H - 28 + k * 5, 36, 1, shade(dark, 1.6f));
                if (cy) { lightStrip(c, 10, H - 8, W - 20, 2, accent); c.fillCircle(W - 20, 22, 6, dark); c.ring(W - 20, 22, 5, 2, accent); }
                else { c.fillRect(12, 12, 10, 26, CK_STEEL); c.fillCircle(17, 10, 4, dark); hazardStripe(c, W / 2 - 22, H - 32, 44, 3); c.fillRect(W - 24, 12, 12, 14, CK_RUST); }
                break;
            case BR_AIRFIELD:
                c.fillRect(4, 4, W - 8, H - 8, cy ? rgb(50, 58, 72) : rgb(92, 90, 84));
                for (int k = 0; k < 4; k++) { int x = W / 2 + (int)((k - 1.5f) * 30) - 12; c.rect(x, H / 2 - 12, 24, 24, cy ? accent : CK_YEL); c.fillCircle(x + 12, H / 2, 3, cy ? accent : CK_YEL); }
                c.panel(6, H - 24, 40, 18, base, 1); c.panel(W - 46, H - 24, 40, 18, base, 1);
                if (cy) { lightStrip(c, 6, 6, W - 12, 2, accent); c.fillRect(W / 2 - 3, H - 22, 6, 14, CY_WHITE); }
                else { hazardStripe(c, 6, 6, W - 12, 3); c.fillRect(10, H - 20, 6, 10, CK_RUST); c.fillCircle(W - 26, H - 15, 4, dark); }
                break;
            case BR_TECH:
                c.panel(6, 6, W - 12, H - 12, base, 2);
                if (cy) { c.panel(14, 14, W - 28, H - 28, dark, 1); for (int k = 0; k < 5; k++) c.fillRect(18 + k * 12, 18, 8, H - 36, k % 2 ? shade(CY_PANEL, 0.9f) : CY_PANEL);
                          for (int k = 0; k < 5; k++) for (int j = 0; j < 4; j++) c.set(21 + k * 12, 22 + j * 12, (k + j) % 3 ? accent : rgb(255, 120, 90));
                          c.glow(W / 2, H / 2, 24, rgb(110, 230, 255, 70)); }
                else { c.fillCircle(W / 2, H / 2, 20, CK_STEEL); c.fillCircle(W / 2 - 4, H / 2 - 4, 12, rgb(140, 140, 136)); c.line(W / 2, H / 2, W - 10, 10, CK_DARK, 2); c.fillCircle(W - 10, 10, 3, rgb(230, 60, 50));
                       hazardStripe(c, 8, H - 12, W - 16, 3); c.fillRect(8, 8, 16, 10, CK_RUST); }
                break;
            case BR_TURRET:
                c.fillCircle(16, 16, 13, base); c.ring(16, 16, 13, 2, shade(base, 0.7f));
                if (cy) { c.fillCircle(16, 16, 7, dark); c.ring(16, 16, 6, 2, accent); c.glow(16, 16, 9, rgb(110, 230, 255, 120)); }
                else { c.fillCircle(16, 16, 8, CK_STEEL); c.fillCircle(16, 16, 4, dark); for (int a = 0; a < 8; a++) c.fillCircle(16 + std::cos(a * 0.785f) * 11, 16 + std::sin(a * 0.785f) * 11, 1.5f, CK_DARK); }
                break;
            case BR_AATURRET:
                c.panel(4, 4, W - 8, H - 8, base, 2);
                if (cy) { c.panel(10, 10, W - 20, H - 20, roof, 1); c.fillRect(14, 14, 2, H - 28, accent); c.fillRect(W - 16, 14, 2, H - 28, accent); }
                else { c.panel(10, 10, W - 20, H - 20, CK_STEEL, 1); hazardStripe(c, 6, H - 8, W - 12, 2); }
                break;
            case BR_INCOME:
                if (cy) {   // bitcoin datacenter: rack rows with gold-lit servers and a coin mark
                    c.panel(4, 4, W - 8, H - 8, base, 2);
                    for (int k = 0; k < 4; k++) { int x = 10 + k * 20; c.panel(x, 9, 16, H - 26, dark, 1);
                        for (int j = 0; j < 5; j++) c.set(x + 3, 13 + j * 6, (j + k) % 2 ? accent : rgb(90, 255, 160)), c.fillRect(x + 6, 13 + j * 6, 6, 1, shade(CY_PANEL, 1.2f)); }
                    c.fillCircle(W - 14, H - 11, 6, accent); c.ring(W - 14, H - 11, 5, 1, dark); c.fillRect(W - 15, H - 15, 2, 8, dark);
                    c.glow(W / 2, H / 2, 30, rgb(255, 214, 90, 60)); lightStrip(c, 8, H - 8, W - 16, 2, accent);
                } else {    // oil well: derrick tower over a dark pit with a pump beam
                    c.fillCircle(W / 2, H / 2, 27, rgb(48, 44, 40)); c.ring(W / 2, H / 2, 27, 3, CK_STEEL);
                    c.fillCircle(W / 2, H / 2, 15, rgb(20, 18, 18)); c.glow(W / 2, H / 2, 16, rgb(120, 100, 60, 90));
                    c.line(W / 2 - 22, H / 2 + 20, W / 2, H / 2 - 24, CK_STEEL, 2); c.line(W / 2 + 22, H / 2 + 20, W / 2, H / 2 - 24, CK_STEEL, 2);
                    c.line(W / 2 - 12, H / 2 + 2, W / 2 + 12, H / 2 + 2, CK_STEEL, 1); c.fillCircle(W / 2, H / 2 - 24, 3, rgb(230, 60, 50));
                    hazardStripe(c, 6, H - 10, W - 12, 3);
                }
                break;
            case BR_NUKE:
                c.panel(4, 4, W - 8, H - 8, base, 2);
                c.fillRect(W / 2 - 22, 10, 44, H - 20, dark);                       // launch bay
                c.fillRect(W / 2 - 14, 12, 28, H - 24, shade(dark, 1.5f));
                c.fillRect(W / 2 - 7, 16, 14, H - 30, rgb(198, 200, 200));           // missile body
                c.fillRect(W / 2 - 7, 30, 14, 6, rgb(230, 190, 40));                 // warhead band
                c.fillCircle(W / 2, 20, 7, rgb(214, 70, 60));                       // nose
                c.fillRect(W / 2 - 12, H - 24, 24, 4, rgb(150, 150, 150));
                hazardStripe(c, 8, H - 12, W - 16, 4); hazardStripe(c, 8, 8, W - 16, 3);
                c.fillCircle(14, 16, 4, accent); c.fillCircle(W - 14, 16, 4, accent);
                break;
            }
            (void)team;   // ownership is shown by the player's flag (Gfx::drawFlag), drawn live above the roof
            c.outline(rgb(10, 12, 14, 220));
            building[t][p] = fromCanvas(c, W * 0.5f, H * 0.5f);
            if (p == 0) {
                // construction site: plate + scaffold grid
                Canvas s(W, H);
                s.panel(1, 1, W - 2, H - 2, rgb(70, 66, 58), 1);
                for (int x = 4; x < W - 4; x += 8) s.fillRect(x, 3, 1, H - 6, rgb(120, 112, 90));
                for (int y = 4; y < H - 4; y += 8) s.fillRect(3, y, W - 6, 1, rgb(120, 112, 90));
                s.rect(2, 2, W - 4, H - 4, rgb(180, 150, 60));
                site[t] = fromCanvas(s, W * 0.5f, H * 0.5f);
            }
        }
    }
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
    if (player < 0 || player >= MAX_PLAYERS || !flag[player].tex) return;
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

void Gfx::buildMinimap() {
    Canvas c(MAP_W, MAP_H);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        Color k;
        switch (g_map.tile(x, y)) {
        case T_GRASS: k = rgb(70, 104, 48); break; case T_GRASS2: k = rgb(86, 112, 50); break;
        case T_DIRT: k = rgb(120, 96, 64); break; case T_SAND: k = rgb(186, 168, 120); break;
        case T_ROAD: k = rgb(84, 82, 78); break; case T_WATER: k = rgb(40, 88, 132); break;
        case T_ROCK: k = rgb(108, 104, 96); break; default: k = rgb(38, 78, 40); break;
        }
        c.set(x, y, k);
    }
    minimapTerrain = fromCanvas(c, 0, 0);
}
