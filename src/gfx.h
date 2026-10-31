// One Hour - rendering: procedural sprites, tiles, bitmap font, primitives
#pragma once
#include "common.h"
#include "data.h"
#include "map.h"
#include <SDL2/SDL.h>
#include <unordered_map>

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

// dscale: on-screen size relative to the texture (2x supersampled unit sprites use 0.5). On the GPU most sprites share a few big atlas pages
// (see Gfx::atlasPlace): src is the sprite's rectangle in its page (w = 0: the whole texture) and blend its blend mode, set on the page per draw.
struct Sprite {
    SDL_Texture* tex = nullptr; int w = 0, h = 0; float ox = 0, oy = 0; float dscale = 1;
    SDL_Rect src = { 0, 0, 0, 0 }; u8 blend = SDL_BLENDMODE_BLEND;
    u8 radial = 0;   // 1: nothing outside its inscribed circle (a glow, a puff, a disc); 2: a ring with an empty middle too (big ones draw as meshes, see Gfx::drawRadial)
    const SDL_Rect* srcRect() const { return src.w > 0 ? &src : nullptr; }
    SDL_Rect sub(int x, int y, int w2, int h2) const { return SDL_Rect{ src.x + x, src.y + y, w2, h2 }; }   // a part of the sprite, in its texture
};

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
    Sprite guardDrone[F_COUNT][MAX_PLAYERS];   // protector drones of jeeps and ships, by army look and owner colour
    Sprite cargoPlane[MAX_PLAYERS][F_COUNT];   // paradrop transport by owner colour and army look
    Sprite chute[MAX_PLAYERS];                 // parachute canopy by owner colour
    Sprite aidPlane, aidChute, crate, crateOpen;   // Aid Drop: the white relief plane, its sky-blue canopies and the wooden crates
    Sprite turretHead[4];           // laser turret, gun nest, patriot, rocket battery
    Sprite building[B_COUNT];       // one colour sprite per structure type, shared by every owner (2x supersampled, shadow baked in)
    Sprite buildingTeam[B_COUNT];   // white-on-clear owner marks, drawn over the structure with the owner's colour modulated in
    Sprite bunkerUpg[F_COUNT][BU_COUNT];   // per-bunker upgrades painted alone, drawn over a bunker that has them (same anchor as its sprite)
    Sprite site[B_COUNT];
    Sprite armed[B_COUNT];          // the warhead over a nuke ramp (only the nuke ramps have one)
    Sprite rubble[7];               // scorched ruins by footprint class
    Sprite prop[4];                 // live-rotated structure props: cyber dish, clanker radar, pump jack beam, fan
    Sprite pile[3];
    Sprite shadowSmall, shadowLarge, blob, disc, vignette;   // vignette: soft dark frame corners (GPU only, like the cloud shadows)
    Sprite blobAdd;                 // additive radial glow (muzzle light, fire glow, beacons)
    Sprite teamRing;                // a soft white ring (blended, not added): the army-coloured mark on the ground under a unit
    // baked effect sprites: smoke and dust puffs, fireball frames, flame tongues, muzzle stars, sparks, shock rings, beams, debris, scorch decals
    struct FxArt { Sprite smoke[4], dust[3], fire[8], flame[6], flash[3], streak, ring, beam, debris[4], scorch[3], tread[2], missile, missileK, cloud[3], billow[4];
                   Sprite fallout[2][2]; } fxs;   // missile: Cyber warhead, missileK: Clanker warhead; fallout[silver][inner ring]: a radiation zone's haze and rings in one sprite
    Sprite waterFx[4];              // seamless animated caustics laid over open water
    Sprite seaSwell[4];             // seamless animated swell over the open ocean (512x512: a quarter per aligned 8x8-tile block)
    Sprite flag[MAX_PLAYERS];       // each player's own flag (shape and emblem differ, not just the colour)
    static constexpr int FLAG_W = 24, FLAG_H = 15;
    Sprite minimapTerrain;
    // The baked ground (organic, noise-blended transitions), cut into WORLD_CHUNK px square chunks: a 10240 px world is bigger than many GPUs'
    // largest texture. drawWorld blits a world rectangle from however many chunks it spans. Every chunk keeps its pixels in CPU memory as
    // dithered RGB565; only the chunks drawn lately also hold an SDL texture (at most WORLD_RESIDENT, the least recently drawn one is destroyed
    // first), so a low-end machine does not pay for a 400 MB 32-bit copy of the whole world.
    static constexpr int WORLD_CHUNK = 1024;
    static constexpr int WORLD_RESIDENT = 12;   // (a 1080p view spans at most 9 chunks: 12 keep a margin for scrolling, 24 MB of video memory instead of 48)
    struct WorldChunk { std::vector<u16> px; SDL_Texture* tex = nullptr; int w = 0, h = 0; u32 used = 0; };   // used: the drawWorld call that last drew it
    std::vector<WorldChunk> worldChunks;
    int worldCols = 0, worldRows = 0, worldResident = 0;
    u32 worldStamp = 0;
    std::vector<SDL_Texture*> worldRetired;   // textures given up mid-frame: destroyed at the next beginFrame, once the queued draws that may use them are flushed
    void drawWorld(int wx, int wy, int w, int h, int dx, int dy);
    static constexpr int SHROUD_SS = 4;     // shroud mask cells per tile
    SDL_Texture* miniFog = nullptr;     // the minimap's fog (MAP_W/2 x MAP_H/2 cells, see Game::drawMinimap)
    SDL_Texture* shroudTex = nullptr;   // (MAP_W*SS) x (MAP_H*SS) alpha mask, bilinear-scaled; the edge is displaced by noise so it drifts like cloud
    void updateShroud(const std::vector<u8>& fog, bool force = false);   // fog per tile: 0 never seen, 1 remembered (explored, out of sight), 2 in sight; force: rebuild it all
    static constexpr int SHROUD_BLK = 8;   // tiles per side of a shroud block
    std::vector<u8> shroudClear;           // per block: 1 = every texel of it is clear (all in sight), so drawShroud leaves it out
    void rebakeTerrain();           // a new match on a map whose baked ground was edited by the last one: bake it afresh
    void refreshTerrain();          // structures cleared trees / rocks (Map::cleared): repaint and re-upload just that part of the baked world
    bool terrainEdited = false;
    void uploadTerrainPatch(const std::vector<int>& footprint);
    bool softwareRenderer = false;  // no GPU: skip purely atmospheric full-screen blends (cloud shadows)
    // Texture atlas (GPU only). SDL draws a run of copies from one texture with one blend mode as a single GL call, and every change of texture
    // starts a new one: with a texture per sprite a battle frame was 330 draw calls. Sprites, the effects and a white block (fills and thick lines
    // go through it) share skyline-packed 4096 px pages instead, so whole layers draw in a handful of calls. Sprites a caller tiles or cuts with its
    // own source rectangles (water, swell, the shroud, the font, the vignette) keep textures of their own.
    struct AtlasPage { SDL_Texture* tex = nullptr; std::vector<SDL_Point> sky; long used = 0; int h = 0; };   // sky: the skyline (x, top y) segments, left to right; h: the page's height
    std::vector<AtlasPage> atlas;
    int atlasSize = 4096;
    bool useAtlas = false;
    Sprite whiteBlock;              // a 4x4 opaque white block in the first page (fills sample its middle)
    bool atlasPlace(int w, int h, SDL_Texture*& tex, SDL_Rect& at);   // finds room for a w x h rectangle (2 px apart from the rest)
    Sprite fromCanvasSolo(Canvas& c, float ox, float oy, bool smooth, SDL_BlendMode mode = SDL_BLENDMODE_BLEND);   // a texture of its own (tiled / cut by its caller), never the atlas
    long atlasPixels = 0;           // (texels the sprites in the atlas cover: ONEHOUR_ATLASDBG prints how full the pages are)
    // Additive draws (glows, sparks, muzzle flashes) mixed in among ordinary ones flip the blend mode, and every flip starts a new batch: inside a
    // pass that sets deferAdd they are queued instead and drawn together by flushAdd (light adds up the same whatever the order).
    struct AddDraw { SDL_Texture* tex; SDL_Rect src; SDL_FRect dst; float angleDeg; SDL_FPoint c; bool hasC; Color mod; u8 alpha; float inner = -1; };   // inner >= 0: a drawRadial
    std::vector<AddDraw> addQueue;
    bool deferAdd = false;
    void beginAdd() { static const bool off = getenv("ONEHOUR_NODEFER") != nullptr; if (!softwareRenderer && !off) deferAdd = true; }
    void flushAdd();
    void bind(const Sprite& s, Color mod, u8 alpha) {   // colour, opacity and blend mode of the next copy of s
        SDL_SetTextureColorMod(s.tex, mod.r, mod.g, mod.b); SDL_SetTextureAlphaMod(s.tex, alpha); SDL_SetTextureBlendMode(s.tex, (SDL_BlendMode)s.blend);
    }
    double fillPx = 0;   // (profiling: screen pixels covered by sprite draws so far; see ONEHOUR_FILLDBG)
    int userScale = 0;              // 0 = pick the UI scale from the window size
    int refreshHz = 60;             // the display's refresh rate (the main loop never runs faster)
    bool init(int scale, bool software, int reqW = 0, int reqH = 0);
    void onResize();                // window size changed: recompute the logical (UI) size
    void shutdown();
    Sprite fromCanvas(Canvas& c, float ox, float oy);
    void draw(const Sprite& s, float x, float y, float angle = 0, float scale = 1, Color mod = rgb(255, 255, 255), u8 alpha = 255);
    void drawRect(const Sprite& s, int x, int y, int w, int h, u8 alpha = 255);
    void copyScaled(SDL_Texture* t, const SDL_Rect* src, const SDL_FRect& dst);   // SDL_RenderCopyF, cut to the screen first on the CPU renderer
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
    // an aircraft rolling about its long axis: the sprite foreshortened across the wings by |cos roll| and mirrored while it is belly-up
    void drawRolled(const Sprite& s, float x, float y, float angle, float roll, float scale = 1, Color mod = rgb(255, 255, 255), u8 alpha = 255);
    // The CPU renderer turns a sprite by building a scaled, turned copy of the whole quad on every call (off-screen parts included): a
    // battlefield of wrecks and track marks cost a quarter of a second a frame. Small sprites are drawn from copies turned once instead.
    bool rotCached(SDL_Texture* t, float x, float y, float w, float h, float px, float py, float angle, bool flipV, Color mod, u8 alpha);   // small sprites: pre-turned copies
    struct Pix { int w = 0, h = 0; std::vector<u32> px; };
    struct RotEntry { SDL_Texture* tex = nullptr; SDL_Texture* src = nullptr; int bucket = 0, w = 0, h = 0, size = 0; bool flip = false; size_t bytes = 0; u32 used = 0; };
    std::unordered_map<SDL_Texture*, std::vector<Pix>> pixCache;   // (read-back pixels per texture: level 0 full size, 1 half, 2 quarter)
    std::unordered_map<u64, RotEntry> rotCache;                     // (rotCached)
    size_t rotBytes = 0; u32 rotClock = 0;
    const Pix* pixelsOf(SDL_Texture* t, int level);

    void discFill(float cx, float cy, float r, Color c);  // flat translucent disc with a soft edge
    // A round sprite (a glow, a disc, a ring: nothing outside its inscribed circle) stretched to rx x ry, drawn as a fan of triangles - or a band
    // from 'inner' (share of the radius) out, for a ring with an empty middle - instead of its square: the corners (a fifth of the square) and the
    // empty middle are never blended. Big ones only, on the GPU; small ones and the CPU renderer draw the plain square.
    void drawRadial(const Sprite& s, float cx, float cy, float rx, float ry, float inner, Color mod, u8 alpha, float angle = 0);
    void radialNow(SDL_Texture* tex, const SDL_Rect& src, float cx, float cy, float rx, float ry, float inner, Color mod, u8 alpha, float angle);
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
    void makeChunkTexture(WorldChunk& ch, u32 stamp);   // gives a chunk its SDL texture, first freeing the least recently drawn one if the cap is reached
    void uploadChunkRect(WorldChunk& ch, int x, int y, int w, int h);   // copies a rectangle of the chunk's RGB565 pixels into its texture (if it has one)
    void buildEffects();
    Sprite makeSprite(Canvas& c, float ox, float oy, bool smooth, SDL_BlendMode mode);
};
extern Gfx g_gfx;
