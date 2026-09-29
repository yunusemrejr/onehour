#include "gfx.h"

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
bool Gfx::init(int scale, bool software) {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "video: %s\n", SDL_GetError()); return false; }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
    SDL_SetHint(SDL_HINT_RENDER_BATCHING, "1");
    win = SDL_CreateWindow("One Hour", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, SCREEN_W * scale, SCREEN_H * scale, SDL_WINDOW_SHOWN);
    if (!win) { fprintf(stderr, "window: %s\n", SDL_GetError()); return false; }
    u32 flags = software ? SDL_RENDERER_SOFTWARE : (SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    ren = SDL_CreateRenderer(win, -1, flags);
    if (!ren && !software) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) { fprintf(stderr, "renderer: %s\n", SDL_GetError()); return false; }
    SDL_RenderSetLogicalSize(ren, SCREEN_W, SCREEN_H);
    SDL_SetRenderDrawBlendMode(ren, SDL_BLENDMODE_BLEND);
    { Canvas c(2, 2); c.fillRect(0, 0, 2, 2, rgb(255, 255, 255)); white = fromCanvas(c, 0, 0).tex; }
    buildFont(); buildTiles(); buildUnits(); buildBuildings(); buildMisc(); buildMinimap();
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

// ------------------------------------------------------------ primitives
void Gfx::draw(const Sprite& s, float x, float y, float angle, float scale, Color mod, u8 alpha) {
    if (!s.tex) return;
    SDL_Rect dst = { (int)std::lround(x - s.ox * scale), (int)std::lround(y - s.oy * scale), (int)(s.w * scale), (int)(s.h * scale) };
    SDL_Point c = { (int)(s.ox * scale), (int)(s.oy * scale) };
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

// ------------------------------------------------------------ units
static void tracks(Canvas& c, int x, int y, int w, int h, Color dark) {
    c.fillRect(x, y, w, h, dark);
    for (int i = x; i < x + w; i += 3) c.fillRect(i, y, 1, h, shade(dark, 1.5f));
}
static void teamMark(Canvas& c, int x, int y, int w, int h, Color team) { c.fillRect(x, y, w, h, team); }

void Gfx::buildUnits() {
    for (int p = 0; p < MAX_PLAYERS; p++) {
        Color team = PLAYER_COLOR[p];
        for (int t = 0; t < U_COUNT; t++) {
            const UnitType& u = UNITS[t];
            bool cy = u.faction == F_CYBER;
            Color hull = cy ? CY_HULL : CK_HULL, panel = cy ? CY_PANEL : CK_PANEL, dark = cy ? CY_DARK : CK_DARK, accent = cy ? CY_GLOW : CK_YEL;
            int local = t - firstUnitOf(u.faction);
            Canvas c(32, 32);
            // all unit sprites face +x (right); center at (16,16)
            switch (local) {
            case 0: // dozer / fabricator
                tracks(c, 8, 7, 14, 4, dark); tracks(c, 8, 21, 14, 4, dark);
                c.panel(8, 10, 13, 12, hull, 1);
                c.fillRect(11, 12, 6, 8, panel);
                if (cy) { c.fillRect(20, 9, 4, 14, shade(panel, 1.1f)); c.fillRect(24, 11, 2, 10, accent); c.glow(25, 16, 5, rgb(110, 230, 255, 120)); }
                else { c.fillRect(21, 8, 3, 16, CK_STEEL); c.fillRect(24, 7, 2, 18, CK_YEL); for (int k = 8; k < 24; k += 4) c.fillRect(24, k, 2, 2, dark); }
                teamMark(c, 9, 11, 3, 3, team);
                break;
            case 1: // harvester
                tracks(c, 6, 8, 20, 3, dark); tracks(c, 6, 21, 20, 3, dark);
                c.panel(5, 10, 13, 12, shade(hull, 0.9f), 1);   // cargo bed
                c.panel(18, 10, 8, 12, panel, 1);               // cab
                c.fillRect(24, 12, 2, 8, dark);
                if (cy) { c.fillRect(7, 12, 9, 8, shade(hull, 1.2f)); c.fillRect(8, 15, 7, 2, accent); }
                else { c.fillCircle(9, 16, 3, CK_RUST); c.fillCircle(14, 16, 3, CK_RUST); c.fillRect(6, 11, 1, 10, CK_YEL); }
                teamMark(c, 19, 11, 3, 3, team);
                break;
            case 2: case 3: case 4: { // infantry
                c.fillCircle(16, 16, 5.5f, shade(hull, 0.85f));   // shoulders
                c.fillCircle(16, 16, 3.2f, cy ? panel : rgb(120, 110, 80)); // head/helmet
                c.fillCircle(15.2f, 15.2f, 1.2f, shade(cy ? panel : rgb(120, 110, 80), 1.4f));
                Color gun = local == 3 ? accent : (local == 4 ? (cy ? CY_WHITE : CK_STEEL) : dark);
                int gl = local == 3 ? 9 : (local == 4 ? 8 : 7);
                c.fillRect(18, 15, gl, 2, gun);
                if (local == 4 && !cy) c.fillRect(18, 17, 4, 1, dark);
                if (local == 3 && !cy) { c.fillRect(20, 14, 6, 4, CK_RUST); }
                if (local == 4 && cy) c.glow(24, 16, 3, rgb(160, 220, 255, 150));
                teamMark(c, 12, 14, 2, 4, team);
                break;
            }
            case 5: // main tank body
                tracks(c, 5, 6, 22, 5, dark); tracks(c, 5, 21, 22, 5, dark);
                c.panel(6, 9, 20, 14, hull, 1);
                c.fillRect(8, 11, 16, 10, shade(hull, 1.1f));
                if (cy) { c.fillRect(7, 11, 1, 10, accent); c.fillRect(24, 11, 1, 10, accent); }
                else { c.fillRect(6, 9, 3, 14, CK_RUST); c.fillRect(23, 10, 2, 12, dark); }
                teamMark(c, 9, 12, 3, 3, team);
                break;
            case 6: // volt walker / gatling tank
                if (cy) {
                    c.line(8, 8, 16, 16, dark, 3); c.line(8, 24, 16, 16, dark, 3); c.line(26, 16, 16, 16, dark, 3);
                    c.fillCircle(8, 8, 2.5f, panel); c.fillCircle(8, 24, 2.5f, panel); c.fillCircle(26, 16, 2.5f, panel);
                    c.fillCircle(16, 16, 6.5f, hull); c.fillCircle(16, 16, 4.5f, panel);
                } else {
                    tracks(c, 6, 7, 20, 4, dark); tracks(c, 6, 21, 20, 4, dark);
                    c.panel(7, 10, 18, 12, hull, 1); c.fillRect(9, 12, 14, 8, shade(hull, 1.1f));
                    c.fillRect(7, 10, 2, 12, CK_YEL);
                }
                teamMark(c, 10, 12, 3, 3, team);
                break;
            case 7: // railgun tank / rocket launcher
                tracks(c, 4, 6, 24, 5, dark); tracks(c, 4, 21, 24, 5, dark);
                c.panel(5, 9, 22, 14, cy ? shade(hull, 1.05f) : shade(hull, 0.95f), 1);
                if (cy) { c.fillRect(7, 11, 18, 10, shade(hull, 1.15f)); c.fillRect(6, 15, 20, 2, accent); }
                else { c.fillRect(7, 11, 18, 10, CK_PANEL); c.fillRect(5, 9, 2, 14, CK_RUST); }
                teamMark(c, 8, 12, 3, 3, team);
                break;
            case 8: // aircraft
                if (cy) {
                    // delta drone
                    for (int y = 4; y < 29; y++) { int half = std::abs(y - 16); int len = 24 - half * 2; if (len > 0) c.fillRect(5 + half, y, len, 1, y == 16 ? panel : hull); }
                    c.fillRect(20, 14, 8, 5, panel);
                    c.glow(9, 16, 4, rgb(110, 230, 255, 200)); c.fillRect(6, 15, 4, 3, accent);
                    teamMark(c, 14, 15, 3, 3, team);
                } else {
                    // gunship: fuselage + tail + rotor disc
                    c.fillRect(6, 15, 8, 3, dark); c.fillRect(6, 13, 2, 7, dark);
                    c.panel(12, 11, 14, 11, hull, 1); c.fillRect(22, 13, 5, 7, panel);
                    c.fillRect(13, 22, 10, 2, CK_RUST); c.fillRect(13, 9, 10, 2, CK_RUST);
                    c.ring(18, 16, 12, 1.2f, rgb(60, 60, 60, 110)); c.line(6, 16, 30, 16, rgb(70, 70, 70, 140)); c.line(18, 4, 18, 28, rgb(70, 70, 70, 140));
                    teamMark(c, 14, 13, 3, 3, team);
                }
                break;
            }
            if (local != 8) c.outline(rgb(10, 12, 14, 200));
            unitBody[t][p] = fromCanvas(c, 16, 16);
            // turrets (shared across players)
            if (p == 0) {
                Canvas tc(32, 32);
                bool has = false;
                switch (local) {
                case 5: has = true;
                    tc.fillCircle(16, 16, 6, shade(hull, 1.25f)); tc.fillCircle(15, 15, 3.5f, panel);
                    if (cy) { tc.fillRect(20, 15, 11, 3, dark); tc.fillRect(26, 14, 5, 5, accent); }
                    else { tc.fillRect(20, 15, 12, 2, dark); tc.fillRect(29, 14, 3, 4, CK_STEEL); }
                    break;
                case 6: has = true;
                    if (cy) { tc.fillCircle(16, 16, 4, CY_WHITE); tc.glow(16, 16, 8, rgb(150, 230, 255, 170)); tc.ring(16, 16, 5.5f, 1.2f, accent); }
                    else { tc.fillCircle(16, 16, 5, shade(hull, 1.25f)); tc.fillRect(20, 13, 10, 2, dark); tc.fillRect(20, 16, 10, 2, dark); tc.fillRect(20, 15, 9, 1, CK_STEEL); }
                    break;
                case 7: has = true;
                    if (cy) { tc.fillCircle(16, 16, 5.5f, shade(hull, 1.3f)); tc.fillRect(19, 15, 13, 2, CY_WHITE); tc.fillRect(19, 14, 4, 4, panel); tc.glow(31, 16, 3, rgb(200, 240, 255, 200)); }
                    else { tc.panel(10, 11, 16, 10, CK_STEEL, 1); for (int k = 0; k < 4; k++) tc.fillRect(12 + k * 3, 13, 2, 6, dark); tc.fillRect(24, 12, 4, 8, CK_RUST); }
                    break;
                }
                if (has) { tc.outline(rgb(10, 12, 14, 160)); unitTurret[t] = fromCanvas(tc, 16, 16); }
            }
        }
    }
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
            Color base = cy ? rgb(58, 70, 92) : rgb(108, 106, 90);
            Color plate = cy ? rgb(44, 54, 74) : rgb(88, 88, 76);
            Color roof = cy ? CY_PANEL : CK_PANEL;
            Color dark = cy ? CY_DARK : CK_DARK;
            Color accent = cy ? CY_GLOW : CK_YEL;
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
            }
            // team stripe at a corner of the plate
            c.fillRect(2, 2, std::min(14, W / 4), 3, team);
            c.fillRect(2, 2, 3, std::min(14, H / 4), team);
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
    // turret heads (the rotating part) are drawn as unit turrets: laser turret uses a dedicated sprite
    {
        Canvas tc(32, 32);
        tc.fillCircle(16, 16, 5, CY_PANEL); tc.fillRect(19, 14, 12, 4, CY_DARK); tc.fillRect(26, 13, 5, 6, CY_GLOW); tc.glow(29, 16, 4, rgb(150, 230, 255, 180));
        tc.outline(rgb(10, 12, 14, 160));
        turretHead[0] = fromCanvas(tc, 16, 16);  // laser turret head
    }
    {
        Canvas tc(32, 32);
        tc.fillCircle(16, 16, 5, CK_STEEL); tc.fillRect(19, 13, 10, 2, CK_DARK); tc.fillRect(19, 17, 10, 2, CK_DARK); tc.fillRect(19, 15, 9, 2, rgb(70, 70, 70));
        tc.outline(rgb(10, 12, 14, 160));
        turretHead[1] = fromCanvas(tc, 16, 16);  // gun nest head
    }
    {
        Canvas tc(32, 32);
        tc.panel(6, 9, 20, 14, rgb(200, 205, 210), 1); for (int k = 0; k < 4; k++) tc.fillRect(9 + k * 4, 11, 3, 10, rgb(60, 62, 70)); tc.fillRect(24, 10, 3, 12, rgb(170, 170, 175));
        tc.outline(rgb(10, 12, 14, 160));
        turretHead[2] = fromCanvas(tc, 16, 16);   // patriot launcher box
    }
    {
        Canvas tc(32, 32);
        tc.panel(6, 8, 20, 16, CK_STEEL, 1); for (int j = 0; j < 2; j++) for (int k = 0; k < 3; k++) tc.fillCircle(11 + k * 5, 12 + j * 8, 2, CK_DARK); tc.fillRect(24, 9, 3, 14, CK_RUST);
        tc.outline(rgb(10, 12, 14, 160));
        turretHead[3] = fromCanvas(tc, 16, 16);   // rocket battery pod
    }
}

// ------------------------------------------------------------ misc
void Gfx::buildMisc() {
    Rng r(31337);
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
