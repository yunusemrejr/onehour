// One Hour - the light of the battlefield.
//
// Every frame the light that falls on the ground world is composed in one small texture (half the view's size) and multiplied over it: the
// hour's ambient light and the dark of the corners, the shadows of the clouds drifting over, then every lamp added in - lit windows, yard
// floods, street lamps, headlight cones, muzzle flashes, blasts, burning structures, forest fires, rockets in flight, lightning. Where a lamp
// burns the ground and the units on it show their true colour; away from one the night is dark. Lamps cannot make a pixel brighter than
// its own colour (a multiply), so the hot cores of fires and flashes are still drawn additively over the result by the effects passes.
//
// The old look (a flat multiply plus additive glows) stays behind ONEHOUR_OLDLIGHT=1 for comparisons.
#include "ui.h"

namespace {

struct Lamp { float x, y, r; float cr, cg, cb; float ang; u8 cone; };   // screen px (full resolution), radius, colour * strength (0..1+)

struct LightState {
    SDL_Texture* buf = nullptr;    // the light buffer (render target)
    SDL_Texture* amb = nullptr;    // the ambient light times the corners' darkening, at the buffer's size (rebuilt when the hour's colour moves)
    SDL_Texture* blob = nullptr;   // a soft round pool (white, falloff in alpha)
    SDL_Texture* cone = nullptr;   // a beam: apex at the left middle, pointing right
    int w = 0, h = 0, sw = 0, sh = 0;
    int ar = -1, ag = -1, ab = -1;
    bool cpu = false;              // the CPU renderer: the buffer is composed here in memory and uploaded (SDL's software blits of scaled,
                                   // blended sprites cost several ms a frame)
    std::vector<u32> ambPx, px;    // ambient * corners, and the frame's buffer (ABGR8888, W x H)
    std::vector<u32> qAmb, qPx;    // the CPU renderer composes at a quarter of the view's size (qw x qh), then doubles it smoothly into px
    int qw = 0, qh = 0;
    std::vector<Lamp> lamps;
};
LightState L;

bool oldLight() { static const bool v = getenv("ONEHOUR_OLDLIGHT") != nullptr; return v; }

float lh(u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return (a & 0xFFFFFF) / 16777215.0f; }
float sst01(float x) { x = clampf(x, 0, 1); return x * x * (3 - 2 * x); }

SDL_Texture* makeLightTex(SDL_Renderer* ren, int w, int h, float (*shape)(float, float)) {
    std::vector<u32> px((size_t)w * h);
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        float a = clampf(shape((x + 0.5f) / w, (y + 0.5f) / h), 0, 1);
        px[(size_t)y * w + x] = Canvas::pack(Color{ 255, 255, 255, (u8)(a * 255.0f + 0.5f) });
    }
    SDL_Texture* t = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) return nullptr;
    SDL_UpdateTexture(t, nullptr, px.data(), w * 4);
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_ADD);
    SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
    return t;
}

// a pool of light: bright in the middle, a long soft shoulder (an inverse-square look without the infinite tail)
float blobShape(float u, float v) {
    float dx = u * 2 - 1, dy = v * 2 - 1, d = std::sqrt(dx * dx + dy * dy);
    if (d >= 1) return 0;
    float f = 1 - d * d;
    return f * f * (0.55f + 0.45f * (1 - d));
}
// a headlight beam: 26 degrees either side of the axis, brightest a little ahead of the lamp, fading with distance and toward its edges
float coneShape(float u, float v) {
    float dx = u, dy = (v * 2 - 1) * 0.5f, d = std::sqrt(dx * dx + dy * dy);
    if (d >= 1 || dx <= 0) return 0;
    float ang = std::fabs(std::atan2(dy, dx)) / 0.46f;
    if (ang >= 1) return 0;
    float side = 1 - ang * ang, along = (1 - d) * (1 - d) * sst01(d / 0.12f);
    return side * along * 1.15f;
}

// The buffer composed on the CPU: the ambient copied in, then every lamp added with saturation (a falloff table for the pools, the beam's
// shape worked out per pixel for the cones). At half the view's size a frame of lamps is a few hundred thousand cheap pixel adds.
void composeCpu(int W, int H, float sx, float sy) {   // into L.qPx (W x H: the quarter-size grid)
    static float blobLut[257];
    static bool lut = false;
    if (!lut) { for (int i = 0; i <= 256; i++) { float d = std::sqrt(i / 256.0f); blobLut[i] = blobShape(0.5f + d * 0.5f, 0.5f); } lut = true; }
    L.qPx = L.qAmb;
    u32* px = L.qPx.data();
    for (const Lamp& l : L.lamps) {
        const float cx = l.x * sx, cy = l.y * sy, r = l.r * sx;
        if (r < 1.0f) continue;
        const int R = (int)(clampf(l.cr, 0, 2) * 255), G = (int)(clampf(l.cg, 0, 2) * 255), B = (int)(clampf(l.cb, 0, 2) * 255);
        const int x0 = std::max(0, (int)(cx - r)), x1 = std::min(W - 1, (int)(cx + r)), y0 = std::max(0, (int)(cy - r)), y1 = std::min(H - 1, (int)(cy + r));
        const float inv = 1.0f / (r * r);
        const float ca = std::cos(l.ang), sa = std::sin(l.ang), ir = 1.0f / r;
        for (int y = y0; y <= y1; y++) {
            const float dy = y + 0.5f - cy;
            u32* row = px + (size_t)y * W;
            for (int x = x0; x <= x1; x++) {
                const float dx = x + 0.5f - cx;
                float f;
                if (!l.cone) {
                    const float d2 = (dx * dx + dy * dy) * inv;
                    if (d2 >= 1.0f) continue;
                    f = blobLut[(int)(d2 * 256.0f)];
                } else {
                    const float u = (dx * ca + dy * sa) * ir, v = (-dx * sa + dy * ca) * ir;
                    if (u <= 0.0f || u >= 1.0f) continue;
                    const float t = std::fabs(v) / (u * 0.496f);
                    if (t >= 1.0f) continue;
                    const float d = std::sqrt(u * u + v * v);
                    if (d >= 1.0f) continue;
                    f = (1 - t * t) * (1 - d) * (1 - d) * sst01(d / 0.12f) * 1.15f;
                }
                if (f <= 0.004f) continue;
                const u32 p = row[x];
                const int pr = (int)(p & 255) + (int)(R * f), pg = (int)(p >> 8 & 255) + (int)(G * f), pb = (int)(p >> 16 & 255) + (int)(B * f);
                row[x] = 0xFF000000u | (u32)std::min(255, pb) << 16 | (u32)std::min(255, pg) << 8 | (u32)std::min(255, pr);
            }
        }
    }
}

// Double the quarter-size grid into the half-size buffer with bilinear weights (each output pixel is 9:3:3:1 of the four nearest), two
// channels at a time in the halves of a 32-bit word.
void expandCpu(int QW, int QH, int W, int H) {
    const u32* q = L.qPx.data();
    u32* out = L.px.data();
    for (int y = 0; y < H; y++) {
        const int qy = y >> 1, oy = (y & 1) ? std::min(QH - 1, qy + 1) : std::max(0, qy - 1);
        const u32* r0 = q + (size_t)std::min(qy, QH - 1) * QW, *r1 = q + (size_t)oy * QW;
        u32* o = out + (size_t)y * W;
        for (int x = 0; x < W; x++) {
            const int qx = std::min(x >> 1, QW - 1), ox = (x & 1) ? std::min(QW - 1, qx + 1) : std::max(0, qx - 1);
            const u32 a = r0[qx], b = r0[ox], c = r1[qx], d = r1[ox];
            const u32 lo = ((a & 0x00FF00FFu) * 9 + (b & 0x00FF00FFu) * 3 + (c & 0x00FF00FFu) * 3 + (d & 0x00FF00FFu)) >> 4 & 0x00FF00FFu;
            const u32 hi = (((a >> 8) & 0x00FF00FFu) * 9 + ((b >> 8) & 0x00FF00FFu) * 3 + ((c >> 8) & 0x00FF00FFu) * 3 + ((d >> 8) & 0x00FF00FFu)) >> 4 & 0x00FF00FFu;
            o[x] = lo | hi << 8;
        }
    }
}

}   // namespace

bool Game::lightBufferOn() const { return !oldLight(); }

// The ambient light the buffer starts from: the hour's colour, the night made much darker and bluer than the old flat tint (lamps carry it
// now), storms dimmer still. A scan's light-amplified picture lifts it toward daylight as before (ambientTint does that part).
Color Game::lightAmbient() const {
    Color t = ambientTint();
    if (oldLight()) return t;
    const float night = nightLevel(), st = stormLevel();
    const float k = 1.0f - 0.44f * night - 0.16f * st;
    float r = t.r * k * (1.0f - 0.16f * night), gg = t.g * k * (1.0f - 0.07f * night), b = t.b * k;
    return rgb((int)clampf(r, 0, 255), (int)clampf(gg, 0, 255), (int)clampf(b, 0, 255));
}

// What the passes drawn after the light buffer (smoke, aircraft) tint themselves with: the same light as the ground below them.
Color Game::postLightTint() const { return lightBufferOn() ? lightAmbient() : ambientTint(); }

void Game::collectLamps(float night) {
    std::vector<Lamp>& out = L.lamps;
    out.clear();
    Gfx& g = g_gfx;
    const size_t cap = g.softwareRenderer ? 90 : 240;
    auto lamp = [&](float sx, float sy, float r, Color c, float k) {
        if (k < 0.03f || out.size() >= cap) return;
        if (sx < -r || sy < -r || sx > SCREEN_W + r || sy > VIEW_H + r) return;
        out.push_back(Lamp{ sx, sy, r, c.r / 255.0f * k, c.g / 255.0f * k, c.b / 255.0f * k, 0, 0 });
    };
    auto beam = [&](float sx, float sy, float len, float ang, Color c, float k) {
        if (k < 0.03f || out.size() >= cap) return;
        if (sx < -len || sy < -len || sx > SCREEN_W + len || sy > VIEW_H + len) return;
        out.push_back(Lamp{ sx, sy, len, c.r / 255.0f * k, c.g / 255.0f * k, c.b / 255.0f * k, ang, 1 });
    };
    // 1. the fighting: blasts, muzzle flashes, beams, the nuclear fireball (these light the ground at any hour the ambient leaves room for)
    const float dark = clampf(0.25f + night, 0.0f, 1.0f);
    for (const Fx& f : g_sim.fx) {
        if (f.t < 0) continue;
        const float k = f.t / std::max(0.01f, f.life);
        const Vec2 s = worldToScreen(f.a);
        switch (f.type) {
        case FX_EXPLODE: lamp(s.x, s.y, f.size * 6.5f * (0.6f + 0.4f * std::sqrt(k)), Color{255, 140, 60, 255}, 1.05f * (1 - k) * (1 - k) * dark); break;
        case FX_FLASH: lamp(s.x, s.y, f.size * 9.0f, f.color, 0.95f * (1 - k) * dark); break;
        case FX_BEAM: case FX_RAIL: { const Vec2 b = worldToScreen(f.b); lamp(b.x, b.y, 46.0f, f.color, 0.8f * (1 - k) * dark); lamp(s.x, s.y, 30.0f, f.color, 0.5f * (1 - k) * dark); break; }
        case FX_ARC: { const Vec2 b = worldToScreen(f.b); lamp(b.x, b.y, 36.0f, f.color, 0.6f * (1 - k) * dark); break; }
        case FX_MUSHROOM: lamp(s.x, s.y, 520.0f * (0.5f + 0.5f * std::sqrt(k)), Color{255, 190, 120, 255}, 1.6f * (1 - k) * (1 - k)); break;
        default: break;
        }
    }
    for (const Projectile& p : g_sim.projs) {   // rockets and missiles carry their motor's light; shells and bombs a dull glow
        const Weapon& w = WEAPONS[p.weapon];
        if (w.proj != PJ_ROCKET && w.proj != PJ_SHELL && w.proj != PJ_LASER) continue;
        const Vec2 s = worldToScreen(p.prevPos + (p.pos - p.prevPos) * renderAlpha);
        if (w.proj == PJ_ROCKET) lamp(s.x, s.y, 34.0f, Color{255, 176, 96, 255}, 0.55f * dark);
        else if (w.proj == PJ_LASER) lamp(s.x, s.y, 26.0f, Color{255, 90, 90, 255}, 0.45f * dark);
        else lamp(s.x, s.y, 18.0f, Color{255, 170, 110, 255}, 0.25f * dark);
    }
    // 2. forest fires (sim_fire.cpp): every burning tile throws a flickering orange pool; neighbouring tiles share one so a big blaze stays cheap
    for (size_t i = 0; i < g_sim.fires.size(); i++) {
        const FireCell& c = g_sim.fires[i];
        if (((c.tx + c.ty) & 1) && g_sim.fires.size() > 40) continue;   // (in a big fire every other tile carries the light for both)
        if (!g_sim.explored(g_sim.humanPlayer, c.tx, c.ty)) continue;   // (a fire nobody has mapped gives nothing away)
        const Vec2 s = worldToScreen(Vec2((c.tx + 0.5f) * TILE, (c.ty + 0.5f) * TILE));
        const float fl = 0.80f + 0.12f * std::sin(wallTime * 9.0f + c.tx * 1.3f) + 0.08f * std::sin(wallTime * 21.0f + c.ty * 2.1f);
        lamp(s.x, s.y, TILE * 3.4f, Color{255, 132, 52, 255}, c.heat * fl * (0.35f + 0.85f * dark));
    }
    // 3. burning structures
    for (const Entity& e : g_sim.ents) {
        if (!e.alive || !e.isBuilding() || !e.constructed || e.hp >= e.maxHp * 0.5f) continue;
        if (e.pos.x < cam.x - 200 || e.pos.x > cam.x + SCREEN_W + 200 || e.pos.y < cam.y - 200 || e.pos.y > cam.y + VIEW_H + 200) continue;
        if (!g_sim.shownToHuman(e) || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        const BuildType& bt = e.bt();
        const float f = e.hp / e.maxHp, fl = 0.78f + 0.14f * std::sin(wallTime * 11.0f + (float)e.gen) + 0.08f * std::sin(wallTime * 23.0f + (float)e.gen * 1.7f);
        const Vec2 s = worldToScreen(entPos(e));
        lamp(s.x, s.y, std::max(bt.w, bt.h) * TILE * 1.3f, Color{255, 122, 44, 255}, (0.5f - f) * 2.2f * fl * (0.3f + 0.9f * dark));
    }
    if (night < 0.05f) return;
    // 4. the lights of the night: lit windows and yard floods on every structure, headlight beams on the vehicles, the towns' street lamps
    const float vx0 = cam.x - 160, vy0 = cam.y - 160, vx1 = cam.x + SCREEN_W + 160, vy1 = cam.y + VIEW_H + 160;
    for (const Entity& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.carrier.valid()) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!g_sim.shownToHuman(e) || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        const Vec2 p = worldToScreen(entPos(e));
        if (e.isBuilding()) {
            if (!e.constructed) continue;
            const BuildType& bt = e.bt();
            if (bt.role == BR_WALL || bt.role == BR_FWALL || bt.role == BR_WAYMAKER) {
                if (bt.role != BR_WAYMAKER && (e.tx * 7 + e.ty * 3) % 5 == 0) lamp(p.x, p.y - 4, TILE * 1.6f, rgb(255, 214, 150), 0.35f * night);   // (a lamp on every few wall segments)
                continue;
            }
            if (bt.role == BR_HEALANT) { lamp(p.x, p.y - 10, TILE * 2.2f, rgb(120, 255, 170), 0.45f * night); continue; }
            const bool cyber = bt.faction == F_CYBER;
            const Color lc = e.owner < 0 ? rgb(255, 190, 110) : (cyber ? rgb(150, 200, 255) : rgb(255, 176, 96));
            lamp(p.x, p.y + bt.h * TILE * 0.25f, std::max(bt.w, bt.h) * TILE * 1.15f, lc, 0.30f * night);   // the yard round it, lit by its floods
            const int n = std::min(4, 1 + bt.w * bt.h / 4);
            for (int k = 0; k < n; k++) {
                float hx = (lh(e.gen * 31u + k * 7u + 1u) - 0.5f) * bt.w * TILE * 0.7f, hy = (lh(e.gen * 17u + k * 13u + 5u) - 0.5f) * bt.h * TILE * 0.5f - 6;
                lamp(p.x + hx, p.y + hy, 26.0f, lc, 0.40f * night);   // light from a window on the walls and roof round it
            }
        } else if (e.isUnit() && e.ut().kind == UK_VEH && !e.ut().naval) {
            const float r = e.radius();
            const Vec2 f(std::cos(e.angle), std::sin(e.angle));
            beam(p.x + f.x * r * 0.8f, p.y + f.y * r * 0.8f, 74.0f + r * 2.0f, e.angle, rgb(255, 240, 205), 0.95f * night);
            lamp(p.x, p.y, r * 2.6f, rgb(255, 236, 200), 0.22f * night);   // (the spill round the vehicle itself, so it reads in the dark)
        } else if (e.isUnit() && e.ut().naval) {
            lamp(p.x, p.y, e.radius() * 2.8f, rgb(255, 226, 170), 0.35f * night);   // deck lights
        } else if (e.isUnit() && e.owner == g_sim.humanPlayer) {
            lamp(p.x, p.y, 20.0f, rgb(255, 226, 180), 0.22f * night);   // (your own soldiers carry a torch: an army you can read at night)
        }
    }
    for (const District& D : g_sim.districts) {   // street lamps at the corners of every block, sodium orange
        const Vec2 c = worldToScreen(D.center);
        const float R = D.radius + 160;
        if (c.x < -R || c.y < -R || c.x > SCREEN_W + R || c.y > VIEW_H + R) continue;
        for (const Ref& br : D.blocks) {
            const Entity* b = g_sim.get(br);
            if (!b || !g_sim.shownToHuman(*b)) continue;
            const BuildType& bt = b->bt();
            const float x0 = b->tx * TILE - cam.x - 10, y0 = b->ty * TILE - cam.y - 10, x1 = x0 + bt.w * TILE + 20, y1 = y0 + bt.h * TILE + 20;
            const float on = b->constructed ? 1.0f : 0.0f;   // (a razed block's lamps are out)
            lamp(x0, y0, 46.0f, rgb(255, 170, 84), 0.6f * night * on); lamp(x1, y1, 46.0f, rgb(255, 170, 84), 0.6f * night * on);
            lamp(x1, y0, 46.0f, rgb(255, 170, 84), 0.5f * night * on); lamp(x0, y1, 46.0f, rgb(255, 170, 84), 0.5f * night * on);
        }
    }
}

void Game::drawLightBuffer() {
    Gfx& g = g_gfx;
    SDL_Renderer* ren = g.ren;
    const float night = nightLevel();
    Color a = lightAmbient();
    if (weather.bolt > 0.02f) {   // lightning: for an instant the whole field stands in a cold white light
        const float k = clampf(weather.bolt, 0, 1) * 0.85f;
        a = rgb((int)(a.r + (215 - a.r) * k), (int)(a.g + (222 - a.g) * k), (int)(a.b + (255 - a.b) * k));
    }
    const bool cpu = g.softwareRenderer;
    if (cpu && a.r >= 250 && a.g >= 250 && a.b >= 250) return;   // (full daylight on the CPU renderer: no clouds, nothing to light)
    const int W = std::max(64, SCREEN_W / 2), H = std::max(32, VIEW_H / 2);
    if (!L.buf || L.w != W || L.h != H || L.cpu != cpu) {
        if (L.buf) SDL_DestroyTexture(L.buf);
        if (L.amb) SDL_DestroyTexture(L.amb);
        L.buf = SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, cpu ? SDL_TEXTUREACCESS_STREAMING : SDL_TEXTUREACCESS_TARGET, W, H);
        L.amb = cpu ? nullptr : SDL_CreateTexture(ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STATIC, W, H);
        L.w = W; L.h = H; L.ar = -1; L.cpu = cpu;
        L.ambPx.assign((size_t)W * H, 0xFFFFFFFFu);
        L.qw = (W + 1) / 2; L.qh = (H + 1) / 2;
        L.qAmb.assign((size_t)L.qw * L.qh, 0xFFFFFFFFu);
        L.px.assign((size_t)W * H, 0xFFFFFFFFu);
        // (the CPU renderer stretches nearest: a linear stretch of the whole view costs it more than the rest of the pass)
        if (L.buf) { SDL_SetTextureBlendMode(L.buf, SDL_BLENDMODE_MOD); SDL_SetTextureScaleMode(L.buf, cpu ? SDL_ScaleModeNearest : SDL_ScaleModeLinear); }
        if (L.amb) SDL_SetTextureBlendMode(L.amb, SDL_BLENDMODE_NONE);
    }
    if (!cpu && !L.blob) L.blob = makeLightTex(ren, 128, 128, blobShape);
    if (!cpu && !L.cone) L.cone = makeLightTex(ren, 128, 128, coneShape);
    if (!L.buf || (!cpu && (!L.amb || !L.blob || !L.cone))) { drawWeatherTint(); return; }   // (no render targets: the old flat light)
    if (a.r != L.ar || a.g != L.ag || a.b != L.ab) {   // the ambient light times the soft dark of the corners
        const float deep = 1.0f + 0.5f * night;   // (corners darker at night)
        const int AW = cpu ? L.qw : W, AH = cpu ? L.qh : H;
        std::vector<u32>& dst = cpu ? L.qAmb : L.ambPx;
        for (int y = 0; y < AH; y++) for (int x = 0; x < AW; x++) {
            float dx = (x + 0.5f) / AW * 2 - 1, dy = (y + 0.5f) / AH * 2 - 1, d = std::sqrt(dx * dx + dy * dy);
            float v = sst01((d - 0.62f) / 0.75f) * (105.0f / 255.0f) * deep, k = 1.0f - std::min(0.8f, v);
            dst[(size_t)y * AW + x] = Canvas::pack(Color{ (u8)(a.r * k), (u8)(a.g * k), (u8)(a.b * k), 255 });
        }
        if (L.amb) SDL_UpdateTexture(L.amb, nullptr, L.ambPx.data(), W * 4);
        L.ar = a.r; L.ag = a.g; L.ab = a.b;
    }
    collectLamps(night);
    const float sx = (float)W / SCREEN_W, sy = (float)H / VIEW_H;
    if (cpu) {
        composeCpu(L.qw, L.qh, sx * 0.5f, sy * 0.5f);
        expandCpu(L.qw, L.qh, W, H);
        SDL_UpdateTexture(L.buf, nullptr, L.px.data(), W * 4);
        SDL_Rect view = { 0, 0, SCREEN_W, VIEW_H };
        SDL_RenderCopy(ren, L.buf, nullptr, &view);
        return;
    }

    SDL_Texture* prev = SDL_GetRenderTarget(ren);
    if (SDL_SetRenderTarget(ren, L.buf) != 0) { drawWeatherTint(); return; }   // (the target refused: the ambient copy would land on the world)
    SDL_RenderCopy(ren, L.amb, nullptr, nullptr);
    // the shadows of the clouds: big soft patches drifting with the wind (by day the only thing in the buffer)
    {
        const float span = WORLD_W + 1800.0f, wind = 7.0f + 9.0f * g_sim.windSpeed;
        const float wx = std::cos(g_sim.windAngle), wy = std::sin(g_sim.windAngle);
        const u8 shade = (u8)(150 - 50 * night);
        for (int i = 0; i < 11; i++) {
            float bx = 311.0f * (i * 7 % 11) + 190.0f * i, by = 270.0f * (i * 5 % 11) + 130.0f * (i % 3) + (WORLD_H - 2560.0f) * (i % 4) * 0.22f;
            float drift = wallTime * (wind + (i % 3) * 2.0f);
            float x = std::fmod(bx + drift * wx + span * 4, span) - 900.0f, y = std::fmod(by + drift * wy + std::sin(wallTime * 0.04f + i * 1.7f) * 90.0f + (WORLD_H + 1800.0f) * 4, WORLD_H + 1800.0f) - 900.0f;
            float w = 760.0f + 160.0f * (i % 4), h = w * (0.62f + 0.06f * (i % 3));
            Vec2 s = worldToScreen(Vec2(x, y));
            if (s.x + w * 0.5f < 0 || s.x - w * 0.5f > SCREEN_W || s.y + h * 0.5f < 0 || s.y - h * 0.5f > VIEW_H) continue;
            const Sprite& c = g.fxs.cloud[i % 3];
            if (!c.tex) continue;
            SDL_FRect d = { (s.x - w * 0.5f) * sx, (s.y - h * 0.5f) * sy, w * sx, h * sy };
            SDL_SetTextureBlendMode(c.tex, SDL_BLENDMODE_BLEND);
            SDL_SetTextureColorMod(c.tex, (u8)(shade * a.r / 255), (u8)(shade * a.g / 255), (u8)(shade * a.b / 255));
            SDL_SetTextureAlphaMod(c.tex, 120);
            SDL_RenderCopyF(ren, c.tex, c.srcRect(), &d);
        }
    }
    for (const Lamp& l : L.lamps) {
        SDL_Texture* t = l.cone ? L.cone : L.blob;
        float k = std::max(l.cr, std::max(l.cg, l.cb));
        int passes = k > 1.0f ? 2 : 1;   // (a very bright lamp: twice at half strength, the ADD saturates where it should)
        float m = 255.0f / passes;
        SDL_SetTextureColorMod(t, (u8)clampf(l.cr * m, 0, 255), (u8)clampf(l.cg * m, 0, 255), (u8)clampf(l.cb * m, 0, 255));
        if (l.cone) {
            SDL_FRect d = { l.x * sx, (l.y - l.r * 0.5f) * sy, l.r * sx, l.r * sy };
            SDL_FPoint c = { 0, l.r * sy * 0.5f };
            for (int p = 0; p < passes; p++) SDL_RenderCopyExF(ren, t, nullptr, &d, l.ang * 57.29578f, &c, SDL_FLIP_NONE);
        } else {
            SDL_FRect d = { (l.x - l.r) * sx, (l.y - l.r) * sy, l.r * 2 * sx, l.r * 2 * sy };
            for (int p = 0; p < passes; p++) SDL_RenderCopyF(ren, t, nullptr, &d);
        }
    }
    SDL_SetRenderTarget(ren, prev);
    SDL_Rect view = { 0, 0, SCREEN_W, VIEW_H };
    SDL_RenderCopy(ren, L.buf, nullptr, &view);
}

// The bright points that are light sources themselves (panes, headlamps, beacons), added over the lit world after the multiply.
void Game::drawLightEmitters() {
    const float night = nightLevel();
    if (night <= 0.05f) return;
    Gfx& g = g_gfx;
    const float vx0 = cam.x - 64, vy0 = cam.y - 64, vx1 = cam.x + SCREEN_W + 64, vy1 = cam.y + VIEW_H + 64;
    for (const Entity& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.carrier.valid()) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!g_sim.shownToHuman(e) || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        const Vec2 p = worldToScreen(entPos(e));
        if (e.isBuilding()) {
            if (!e.constructed) continue;
            const BuildType& bt = e.bt();
            if (bt.role == BR_WALL || bt.role == BR_FWALL || bt.role == BR_WAYMAKER) continue;
            if (bt.role == BR_HEALANT) {
                float pulse = 0.75f + 0.25f * std::sin(wallTime * 3.0f + e.gen);
                g.glowAdd(p.x, p.y - 20, 7.0f, Color{120, 255, 170, (u8)(170 * night * pulse)});
                continue;
            }
            const bool cyber = bt.faction == F_CYBER;
            const Color lc = e.owner < 0 ? rgb(255, 210, 140) : (cyber ? rgb(190, 228, 255) : rgb(255, 206, 130));
            const int n = std::min(4, 1 + bt.w * bt.h / 4);
            for (int k = 0; k < n; k++) {
                float hx = (lh(e.gen * 31u + k * 7u + 1u) - 0.5f) * bt.w * TILE * 0.7f, hy = (lh(e.gen * 17u + k * 13u + 5u) - 0.5f) * bt.h * TILE * 0.5f - 6;
                float flick = 0.85f + 0.15f * std::sin(wallTime * 2.3f + k + e.gen);
                g.glowAdd(p.x + hx, p.y + hy, 6.0f, Color{lc.r, lc.g, lc.b, (u8)(110 * night * flick)});
                g.glowAdd(p.x + hx, p.y + hy, 2.6f, Color{255, 250, 235, (u8)(200 * night * flick)});   // the lit pane itself
            }
        } else if (e.isUnit() && e.ut().kind == UK_VEH && !e.ut().naval) {
            const Vec2 f(std::cos(e.angle), std::sin(e.angle));
            const float r = e.radius();
            for (int s = -1; s <= 1; s += 2) g.glowAdd(p.x + f.x * r * 0.9f - f.y * s * r * 0.4f, p.y + f.y * r * 0.9f + f.x * s * r * 0.4f, 3.2f, Color{255, 246, 220, (u8)(210 * night)});
            for (int s = -1; s <= 1; s += 2) g.glowAdd(p.x - f.x * r * 0.9f - f.y * s * r * 0.35f, p.y - f.y * r * 0.9f + f.x * s * r * 0.35f, 2.4f, Color{255, 40, 30, (u8)(170 * night)});   // tail lights
        }
    }
}

// The light pass over the ground world (see the top of this file); ONEHOUR_OLDLIGHT=1 draws the old flat multiply and additive pools.
void Game::drawLighting() {
    Gfx& g = g_gfx;
    if (oldLight()) { drawWeatherTint(); g.beginAdd(); drawGroundLights(); g.flushAdd(); return; }
    drawLightBuffer();
    g.beginAdd(); drawLightEmitters(); g.flushAdd();
}
