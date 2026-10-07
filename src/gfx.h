// One Hour - rendering: procedural sprites, tiles, bitmap font, primitives
#pragma once
#include "common.h"
#include "data.h"
#include "map.h"
#include <SDL2/SDL.h>

struct Canvas {
    int w, h;
    std::vector<u32> px;
    Canvas(int _w, int _h) : w(_w), h(_h), px((size_t)_w * _h, 0) {}
    void set(int x, int y, Color c) {
        if (x < 0 || y < 0 || x >= w || y >= h || c.a == 0) return;
        u32& d = px[y * w + x];
        if (c.a == 255) { d = pack(c); return; }
        Color o = unpack(d);
        float a = c.a / 255.0f;
        Color r{(u8)(o.r + (c.r - o.r) * a), (u8)(o.g + (c.g - o.g) * a), (u8)(o.b + (c.b - o.b) * a), (u8)std::max((int)o.a, (int)c.a)};
        d = pack(r);
    }
    static u32 pack(Color c) { return (u32)c.a << 24 | (u32)c.b << 16 | (u32)c.g << 8 | c.r; }
    static Color unpack(u32 v) { return Color{(u8)(v & 255), (u8)(v >> 8 & 255), (u8)(v >> 16 & 255), (u8)(v >> 24)}; }
    void fillRect(int x, int y, int rw, int rh, Color c) { for (int j = y; j < y + rh; j++) for (int i = x; i < x + rw; i++) set(i, j, c); }
    void rect(int x, int y, int rw, int rh, Color c) { for (int i = x; i < x + rw; i++) { set(i, y, c); set(i, y + rh - 1, c); } for (int j = y; j < y + rh; j++) { set(x, j, c); set(x + rw - 1, j, c); } }
    void fillCircle(float cx, float cy, float r, Color c) {
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++) for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
            float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
            if (dx * dx + dy * dy <= r * r) set(i, j, c);
        }
    }
    void ring(float cx, float cy, float r, float thick, Color c) {
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++) for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
            float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy);
            if (d <= r && d >= r - thick) set(i, j, c);
        }
    }
    void line(int x0, int y0, int x1, int y1, Color c, int thick = 1) {
        int dx = std::abs(x1 - x0), dy = -std::abs(y1 - y0), sx = x0 < x1 ? 1 : -1, sy = y0 < y1 ? 1 : -1, err = dx + dy;
        for (;;) {
            if (thick <= 1) set(x0, y0, c); else fillRect(x0 - thick / 2, y0 - thick / 2, thick, thick, c);
            if (x0 == x1 && y0 == y1) break;
            int e2 = 2 * err;
            if (e2 >= dy) { err += dy; x0 += sx; }
            if (e2 <= dx) { err += dx; y0 += sy; }
        }
    }
    // soft lit rounded box: base color, lighter top-left, darker bottom-right
    void panel(int x, int y, int rw, int rh, Color c, int bevel = 1) {
        fillRect(x, y, rw, rh, c);
        for (int b = 0; b < bevel; b++) {
            for (int i = x + b; i < x + rw - b; i++) { set(i, y + b, shade(c, 1.35f)); set(i, y + rh - 1 - b, shade(c, 0.6f)); }
            for (int j = y + b; j < y + rh - b; j++) { set(x + b, j, shade(c, 1.2f)); set(x + rw - 1 - b, j, shade(c, 0.7f)); }
        }
    }
    void glow(float cx, float cy, float r, Color c) {
        for (int j = (int)(cy - r - 1); j <= (int)(cy + r + 1); j++) for (int i = (int)(cx - r - 1); i <= (int)(cx + r + 1); i++) {
            float dx = i + 0.5f - cx, dy = j + 0.5f - cy;
            float d = std::sqrt(dx * dx + dy * dy) / r;
            if (d < 1) { Color k = c; k.a = (u8)(c.a * (1 - d) * (1 - d)); set(i, j, k); }
        }
    }
    void outline(Color c) {   // 1px outline around opaque pixels
        std::vector<u32> src = px;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            if (unpack(src[y * w + x]).a > 40) continue;
            bool near = false;
            for (int dy = -1; dy <= 1 && !near; dy++) for (int dx = -1; dx <= 1; dx++) {
                int nx = x + dx, ny = y + dy;
                if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                if (unpack(src[ny * w + nx]).a > 40) { near = true; break; }
            }
            if (near) px[y * w + x] = pack(c);
        }
    }
};

struct Sprite { SDL_Texture* tex = nullptr; int w = 0, h = 0; float ox = 0, oy = 0; float dscale = 1; };   // dscale: on-screen size relative to the texture (2x supersampled unit sprites use 0.5)

extern const Color PLAYER_COLOR[MAX_PLAYERS];   // palette slots: blue, red, yellow, green
extern int g_colorSlot[MAX_PLAYERS];            // which slot each player of the current match uses (an ally takes the friendly green, enemies the warm colours)
inline int slotOf(int owner) { return g_colorSlot[owner < 0 ? 0 : (owner >= MAX_PLAYERS ? MAX_PLAYERS - 1 : owner)]; }
inline Color playerColor(int owner) { return PLAYER_COLOR[slotOf(owner)]; }

struct Gfx {
    SDL_Window* win = nullptr;
    SDL_Renderer* ren = nullptr;
    SDL_Texture* font = nullptr;
    SDL_Texture* white = nullptr;
    Sprite unitBody[U_COUNT][MAX_PLAYERS];
    Sprite unitAnim[U_COUNT][MAX_PLAYERS][2];   // walk / track cycle frames
    Sprite unitTurret[U_COUNT][MAX_PLAYERS];
    Sprite rotorDisc, rotorBlades;
    Sprite cargoPlane[MAX_PLAYERS][F_COUNT];   // paradrop transport by owner colour and army look
    Sprite chute[MAX_PLAYERS];                 // parachute canopy by owner colour
    Sprite aidPlane, aidChute, crate, crateOpen;   // Aid Drop: the white relief plane, its sky-blue canopies and the wooden crates
    Sprite turretHead[4];           // laser turret, gun nest, patriot, rocket battery
    Sprite building[B_COUNT];       // one colour sprite per structure type, shared by every owner (2x supersampled, shadow baked in)
    Sprite buildingTeam[B_COUNT];   // white-on-clear owner marks, drawn over the structure with the owner's colour modulated in
    Sprite site[B_COUNT];
    Sprite armed[B_COUNT];          // the warhead over a nuke ramp (only the nuke ramps have one)
    Sprite rubble[6];               // scorched ruins by footprint class
    Sprite prop[4];                 // live-rotated structure props: cyber dish, clanker radar, pump jack beam, fan
    Sprite pile[3];
    Sprite shadowSmall, shadowLarge, blob, disc;
    Sprite blobAdd;                 // additive radial glow (muzzle light, fire glow, beacons)
    // baked effect sprites: smoke and dust puffs, fireball frames, flame tongues, muzzle stars, sparks, shock rings, beams, debris, scorch decals
    struct FxArt { Sprite smoke[4], dust[3], fire[8], flame[6], flash[3], streak, ring, beam, debris[4], scorch[3], missile, missileK, cloud[3], billow[4]; } fxs;   // missile: Cyber warhead, missileK: Clanker warhead
    Sprite waterFx[4];              // seamless animated caustics laid over open water
    Sprite flag[MAX_PLAYERS];       // each player's own flag (shape and emblem differ, not just the colour)
    static const int FLAG_W = 24, FLAG_H = 15;
    Sprite minimapTerrain;
    Sprite worldTerrain;            // whole map baked into one texture (organic, noise-blended transitions)
    static const int SHROUD_SS = 4;     // shroud mask cells per tile
    SDL_Texture* shroudTex = nullptr;   // (MAP_W*SS) x (MAP_H*SS) alpha mask, bilinear-scaled; the edge is displaced by noise so it drifts like cloud
    void updateShroud(const std::vector<u8>& explored);
    bool softwareRenderer = false;  // no GPU: skip purely atmospheric full-screen blends (cloud shadows)
    int userScale = 0;              // 0 = pick the UI scale from the window size
    bool init(int scale, bool software, int reqW = 0, int reqH = 0);
    void onResize();                // window size changed: recompute the logical (UI) size
    void shutdown();
    Sprite fromCanvas(Canvas& c, float ox, float oy);
    void draw(const Sprite& s, float x, float y, float angle = 0, float scale = 1, Color mod = rgb(255, 255, 255), u8 alpha = 255);
    void drawRect(const Sprite& s, int x, int y, int w, int h, u8 alpha = 255);
    void text(int x, int y, const char* s, Color c, int scale = 1, bool shadow = true);
    int textW(const char* s, int scale = 1) { return (int)strlen(s) * 6 * scale; }
    void fill(int x, int y, int w, int h, Color c);
    void box(int x, int y, int w, int h, Color c);
    void line(float x0, float y0, float x1, float y1, Color c);
    void thickLine(float x0, float y0, float x1, float y1, Color c, float thick);
    void circle(float cx, float cy, float r, Color c, int segs = 20);
    void fillCircle(float cx, float cy, float r, Color c);
    void glowAdd(float cx, float cy, float r, Color c);   // additive light blob
    void drawSized(const Sprite& s, float cx, float cy, float w, float h, float angle, Color mod = rgb(255, 255, 255), u8 alpha = 255);   // centred, any aspect, rotated about its centre
    void discFill(float cx, float cy, float r, Color c);  // flat translucent disc with a soft edge
    void dashedCircle(float cx, float cy, float r, Color c, float phase, float dash = 9, float gap = 6);
    Sprite fromCanvasSmooth(Canvas& c, float ox, float oy);   // bilinear-filtered texture (supersampled sprites)
    Sprite fromCanvasAdd(Canvas& c, float ox, float oy);      // bilinear + additive blending
    void bevelPanel(int x, int y, int w, int h, Color base, bool raised = true);
    void drawFlag(float poleX, float poleY, int player, float scale, float phase);   // mast standing at (poleX, poleY), flag waving
    void present();
    void beginFrame(Color clear);
private:
    void buildFont();
    void buildUnits();
    void buildBuildings();
    void buildMisc();
    void buildMinimap();
    void bakeTerrain();
    void buildEffects();
};
extern Gfx g_gfx;
