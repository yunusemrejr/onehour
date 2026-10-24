// One Hour - the day, the weather and the lake's wildlife. All of it is cosmetic: the simulation never sees the hour or the rain.
//  * The day runs a 12 minute cycle (dawn, morning, day, sunset, dusk, night) from a random starting point, and its light tints the world: one
//    multiply pass over the battlefield, skipped entirely in plain daylight, so a bright afternoon costs nothing at all.
//  * Rain comes and goes in spells of a few minutes; a downpour darkens the sky and brings lightning. Streaks and splashes are a few hundred
//    lines and rings drawn in screen space.
//  * At night the structures' windows and the vehicles' headlights come on (small additive glows).
//  * Whales, dolphins and sharks roam the open lake: a whale's back and spout, a pod of dolphins leaping, a shark's fin cutting a wake.
#include "ui.h"
#include "paint.h"
#include <ctime>
#include <string>

namespace {

const float DAY_LEN = 720.0f;   // seconds in one full day cycle

// the light of the hour: (phase, colour) keys, linearly blended, wrapping round at 1
struct Key { float p; float r, g, b; };
const Key KEYS[] = {
    { 0.00f, 0.52f, 0.58f, 0.80f },   // the end of the night
    { 0.05f, 0.96f, 0.78f, 0.74f },   // dawn: a pink sky
    { 0.12f, 1.00f, 0.94f, 0.86f },   // morning sun
    { 0.20f, 1.00f, 1.00f, 1.00f },   // day
    { 0.48f, 1.00f, 1.00f, 1.00f },
    { 0.55f, 1.00f, 0.77f, 0.56f },   // sunset
    { 0.60f, 0.74f, 0.62f, 0.74f },   // dusk
    { 0.67f, 0.50f, 0.56f, 0.78f },   // night
    { 0.82f, 0.44f, 0.50f, 0.74f },   // the dead of night
    { 1.00f, 0.52f, 0.58f, 0.80f },
};
const float START_POINTS[5] = { 0.03f, 0.14f, 0.30f, 0.53f, 0.74f };   // dawn, morning, day, sunset, night

float hashf(u32 a) { a ^= a >> 16; a *= 0x7feb352du; a ^= a >> 15; a *= 0x846ca68bu; a ^= a >> 16; return (a & 0xFFFFFF) / 16777215.0f; }

// open lake water with room round it: a creature never noses into the beach or an island
bool deepWater(Vec2 p) {
    int tx = tileOf(p.x), ty = tileOf(p.y);
    for (int dy = -2; dy <= 2; dy++) for (int dx = -2; dx <= 2; dx++) if (!g_map.bigWater(tx + dx, ty + dy) || g_map.tile(tx + dx, ty + dy) != T_WATER) return false;
    return true;
}
Vec2 randomDeep(Rng& r) {
    for (int k = 0; k < 400; k++) {
        float an = r.f(0, 6.2832f), rad = r.f(0.0f, 0.5f) * MAP_W * TILE * 0.5f;
        Vec2 p(WORLD_W * 0.5f + std::cos(an) * rad, WORLD_H * 0.5f + std::sin(an) * rad);
        if (deepWater(p)) return p;
    }
    return Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f - 18.0f * TILE);
}

// the creatures' sprites, painted once on first use: a shark's dorsal fin, a dolphin leaping (facing right and left), a whale's back, its fluke
struct SeaArt { Sprite fin, dolR, dolL, back, fluke; bool ok = false; };
SeaArt& seaArt() {
    static SeaArt a;
    if (a.ok) return a;
    a.ok = true;
    using namespace paint;
    Gfx& g = g_gfx;
    {   // shark fin, 3/4 view: a swept grey sail standing out of the water
        Canvas c(32, 32); Art A(c, rgb(255, 255, 255));
        A.poly({ {9, 25}, {17, 6}, {19.5f, 8}, {24, 25} }, rgb(70, 78, 86), 1.4f, 0.55f);
        A.poly({ {17, 6}, {19.5f, 8}, {24, 25}, {21, 25} }, rgb(44, 50, 58), 0.8f, 0.3f);
        a.fin = g.fromCanvasSmooth(c, 16, 25);
    }
    for (int side = 0; side < 2; side++) {   // a dolphin in mid-leap, side profile: a slender grey arc with a beak, a fin and flukes
        Canvas c(48, 24); Art A(c, rgb(255, 255, 255));
        auto X = [&](float x) { return side == 0 ? x : 48 - x; };
        Color body = rgb(112, 128, 146), belly = rgb(200, 210, 218);
        A.poly({ {X(6), 15}, {X(14), 10}, {X(26), 8}, {X(36), 9.5f}, {X(43), 12}, {X(36), 14}, {X(24), 15}, {X(12), 16} }, body, 1.6f, 0.55f);
        A.poly({ {X(14), 14.5f}, {X(26), 13.5f}, {X(36), 12.8f}, {X(26), 15.4f} }, belly, 0.8f, 0.3f);
        A.poly({ {X(22), 8.5f}, {X(25), 3}, {X(28), 8.4f} }, shade(body, 0.8f), 0.8f, 0.4f);   // dorsal fin
        A.poly({ {X(6), 15}, {X(2), 11}, {X(4), 15}, {X(2), 19} }, shade(body, 0.85f), 0.8f, 0.4f);   // flukes
        A.dot(X(38.5f), 10.8f, 0.8f, rgb(20, 22, 26));
        (side == 0 ? a.dolR : a.dolL) = g.fromCanvasSmooth(c, 24, 12);
    }
    {   // a whale's back breaking the surface, seen from above: a long dark rounded hump with a blowhole and a wet shine
        Canvas c(96, 40); Art A(c, rgb(255, 255, 255));
        A.ell(48, 20, 42, 14, rgb(52, 62, 74), 3.0f, 0.6f);
        A.ell(44, 17, 30, 7, rgb(78, 90, 104), 2.0f, 0.4f, 0.55f);
        A.dot(70, 20, 1.4f, rgb(24, 28, 34));
        A.ell(22, 20, 6, 3.2f, rgb(40, 48, 58), 1.2f, 0.3f);   // the small dorsal knob near the tail
        a.back = g.fromCanvasSmooth(c, 48, 20);
    }
    {   // the fluke raised as it dives, 3/4 view
        Canvas c(48, 32); Art A(c, rgb(255, 255, 255));
        A.poly({ {24, 30}, {21, 18}, {6, 6}, {14, 6}, {24, 13}, {34, 6}, {42, 6}, {27, 18} }, rgb(48, 58, 70), 1.6f, 0.55f);
        A.poly({ {24, 13}, {34, 6}, {42, 6}, {27, 18} }, rgb(34, 40, 50), 0.8f, 0.3f);
        a.fluke = g.fromCanvasSmooth(c, 24, 30);
    }
    return a;
}

}  // namespace

// ------------------------------------------------------------ the sky
void Game::initWeather() {
    weather = Weather();
    u64 s = seed ^ ((u64)time(nullptr) * 0x9E3779B97F4A7C15ull);   // every match a different sky (cosmetic, so it needs no replay)
    weather.rng = Rng(s ? s : 1);
    Rng& r = weather.rng;
    weather.phase0 = START_POINTS[r.range(0, 4)] * DAY_LEN;
    if (r.f() < 0.25f) { weather.rain = weather.rainTo = r.f(0.45f, 1.0f); weather.nextRoll = r.f(60, 150); }
    else weather.nextRoll = r.f(40, 120);
    weather.nextBolt = r.f(8, 20);
    if (const char* w = getenv("ONEHOUR_WEATHER")) {   // screenshots: dawn | morning | day | sunset | night [,rain | ,storm]
        std::string ws = w;
        static const char* NAMES[5] = { "dawn", "morning", "day", "sunset", "night" };
        for (int i = 0; i < 5; i++) if (ws.rfind(NAMES[i], 0) == 0) weather.phase0 = START_POINTS[i] * DAY_LEN;
        weather.rain = weather.rainTo = ws.find("storm") != std::string::npos ? 1.0f : (ws.find("rain") != std::string::npos ? 0.7f : 0.0f);
        weather.forced = true;
    }
    // the wildlife: two whales, a pod of three dolphins, two sharks, all well out in the lake
    sea.clear();
    for (int i = 0; i < 7; i++) {
        SeaBeast b;
        b.kind = i < 2 ? 0 : (i < 5 ? 1 : 2);
        b.pos = randomDeep(r); b.goal = randomDeep(r);
        b.ang = r.f(0, 6.2832f);
        b.speed = b.kind == 0 ? 14.0f : (b.kind == 1 ? 46.0f : 30.0f);
        b.up = r.f() < 0.4f; b.upFor = b.kind == 0 ? 9.0f : (b.kind == 1 ? 6.0f : 12.0f);
        b.timer = r.f(2, 14);
        sea.push_back(b);
    }
    for (int i = 3; i < 5; i++) { sea[i].pos = sea[2].pos + Vec2(r.f(-40, 40), r.f(-40, 40)); sea[i].goal = sea[2].goal; sea[i].up = sea[2].up; sea[i].timer = sea[2].timer + r.f(0, 0.6f); }   // (the pod swims together)
}

void Game::updateWeather() {
    float now = g_sim.time, dt = weather.lastT < 0 ? 0.0f : clampf(now - weather.lastT, 0.0f, 0.5f);
    weather.lastT = now;
    if (dt <= 0) return;
    Rng& r = weather.rng;
    if (!weather.forced && now >= weather.nextRoll) {
        if (weather.rainTo > 0) { weather.rainTo = 0; weather.nextRoll = now + r.f(150, 320); }        // the rain stops: a dry spell
        else if (r.f() < 0.45f) { weather.rainTo = r.f(0.45f, 1.0f); weather.nextRoll = now + r.f(80, 210); }   // a shower or a storm rolls in
        else weather.nextRoll = now + r.f(60, 130);
    }
    float step = dt / 18.0f;   // the sky turns over about 18 s
    weather.rain += clampf(weather.rainTo - weather.rain, -step, step);
    weather.bolt *= std::exp(-dt * 7.0f);
    if (weather.rain > 0.75f && now >= weather.nextBolt) {
        weather.bolt = 1.0f; weather.nextBolt = now + r.f(10, 32);
        g_audio.play(SND_EXPLODE_L, Vec2(), true, 0.18f);   // the thunder rolls in a moment later: a low distant boom
    }
    // the creatures swim on whether anybody looks or not
    for (auto& b : sea) {
        b.timer -= dt;
        if (b.timer <= 0) {
            b.up = !b.up;
            b.timer = b.up ? b.upFor * r.f(0.7f, 1.3f) : r.f(b.kind == 0 ? 18.0f : 10.0f, b.kind == 0 ? 40.0f : 30.0f);
            if (b.up && b.kind == 0) for (int k = 0; k < 16; k++)   // a whale surfaces with a blow: a column of spray
                emitP(b.pos.x + std::cos(b.ang) * 26, b.pos.y + std::sin(b.ang) * 26 - 4, r.f(-7, 7), -r.f(40, 90), r.f(1.4f, 2.6f), 5, 18, rgb(240, 246, 252, 190), PK_SMOKE, (u8)r.range(0, 3), 22, 0.6f, r.f(0, 6), 0.2f);
        }
        Vec2 to = b.goal - b.pos;
        if (to.len() < 3.0f * TILE) b.goal = randomDeep(r);
        float want = std::atan2(to.y, to.x), d = want - b.ang;
        while (d > 3.14159f) d -= 6.28318f;
        while (d < -3.14159f) d += 6.28318f;
        b.ang += clampf(d, -0.6f * dt, 0.6f * dt);
        Vec2 np = b.pos + Vec2(std::cos(b.ang), std::sin(b.ang)) * (b.speed * dt);
        if (deepWater(np)) b.pos = np; else { b.goal = randomDeep(r); b.ang += 1.2f * dt; }
    }
}

float Game::dayPhase() const { return std::fmod(weather.phase0 + (weather.forced ? 0.0f : std::max(0.0f, g_sim.time)), DAY_LEN) / DAY_LEN; }   // (a forced sky holds its hour)

Color Game::ambientTint() const {
    float p = dayPhase(), cr = 1, cg = 1, cb = 1;
    for (size_t i = 0; i + 1 < sizeof(KEYS) / sizeof(KEYS[0]); i++) {
        const Key &a = KEYS[i], &b = KEYS[i + 1];
        if (p < a.p || p > b.p) continue;
        float t = (p - a.p) / std::max(1e-4f, b.p - a.p);
        cr = a.r + (b.r - a.r) * t; cg = a.g + (b.g - a.g) * t; cb = a.b + (b.b - a.b) * t;
        break;
    }
    float w = weather.rain;   // clouds and rain: greyer and dimmer
    cr *= 1 - 0.20f * w; cg *= 1 - 0.16f * w; cb *= 1 - 0.08f * w;
    return rgb((int)clampf(cr * 255, 0, 255), (int)clampf(cg * 255, 0, 255), (int)clampf(cb * 255, 0, 255));
}

float Game::nightLevel() const {
    Color t = ambientTint();
    return clampf((0.9f - (t.r + t.g + t.b) / (3.0f * 255.0f)) / 0.32f, 0.0f, 1.0f);
}

const char* Game::weatherLabel() const {
    float p = dayPhase();
    bool storm = weather.rain > 0.75f, wet = weather.rain > 0.15f;
    static char buf[40];
    const char* hour = p < 0.10f ? "Dawn" : (p < 0.20f ? "Morning" : (p < 0.50f ? "Day" : (p < 0.58f ? "Sunset" : (p < 0.65f ? "Dusk" : "Night"))));
    snprintf(buf, sizeof buf, "%s%s", hour, storm ? ", storm" : (wet ? ", rain" : ""));
    return buf;
}

// ------------------------------------------------------------ drawing
void Game::drawSeaLife() {
    if (sea.empty()) return;
    Gfx& g = g_gfx;
    SeaArt& art = seaArt();
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    Color tint = ambientTint();
    auto tinted = [&](Color c) { return rgb(c.r * tint.r / 255, c.g * tint.g / 255, c.b * tint.b / 255); };
    for (size_t i = 0; i < sea.size(); i++) {
        const SeaBeast& b = sea[i];
        if (!b.up) continue;
        Vec2 s = worldToScreen(b.pos);
        if (s.x < -80 || s.y < -80 || s.x > SCREEN_W + 80 || s.y > VIEW_H + 80) continue;
        int tx = clampi(tileOf(b.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(b.pos.y), 0, MAP_H - 1);
        if (!rev && !ex[ty * MAP_W + tx]) continue;
        // fade in as it rises and out as it sinks
        float life = b.upFor, k = clampf(std::min(b.timer, life - b.timer) / 1.2f, 0.0f, 1.0f);
        if (k <= 0) continue;
        Vec2 f(std::cos(b.ang), std::sin(b.ang)), sd(-f.y, f.x);
        float spd = b.speed;
        if (b.kind == 2) {   // shark: the fin and the V it cuts in the water
            for (int side = -1; side <= 1; side += 2) {
                Vec2 a = s + f * 2.0f, e = s - f * 26.0f + sd * (side * 11.0f);
                g.thickLine(a.x, a.y + 2, e.x, e.y + 2, rgb(230, 244, 250, (int)(110 * k)), 1.3f);
            }
            g.drawSized(g.disc, s.x - f.x * 12, s.y - f.y * 12 + 3, 26, 13, 0, rgb(8, 24, 40), (u8)(70 * k));   // its body, a dark shape under the surface
            g.drawSized(g.fxs.ring, s.x, s.y + 3, 22, 9, 0, rgb(225, 240, 250), (u8)(110 * k));
            float bob = std::sin(wallTime * 3.0f + i) * 0.8f;
            g.drawSized(art.fin, s.x, s.y + bob, 20, 20, 0, tinted(rgb(255, 255, 255)), (u8)(255 * k));
        } else if (b.kind == 1) {   // dolphin: leaps in a row, a splash where it leaves the water and where it goes back in
            float cyc = std::fmod(wallTime * 0.8f + i * 0.37f, 1.0f);   // one leap every 1.25 s
            if (cyc < 0.55f) {
                float t = cyc / 0.55f, h = std::sin(t * 3.14159f) * 14.0f;
                Vec2 q = s + f * ((t - 0.5f) * 30.0f);
                float tilt = std::atan2(-std::cos(t * 3.14159f) * 14.0f, 30.0f * (f.x >= 0 ? 1.0f : -1.0f)) * (f.x >= 0 ? 1.0f : -1.0f);
                g.drawSized(g.disc, q.x, q.y + 3, 22, 7, 0, rgb(8, 24, 40), (u8)(60 * k));   // its shadow on the water
                g.drawSized(f.x >= 0 ? art.dolR : art.dolL, q.x, q.y - h, 34, 17, f.x >= 0 ? tilt : -tilt, tinted(rgb(255, 255, 255)), (u8)(255 * k));
                if (t < 0.18f || t > 0.82f) { Vec2 sp = s + f * ((t < 0.5f ? -0.5f : 0.5f) * 30.0f); g.drawSized(g.fxs.ring, sp.x, sp.y + 2, 20, 8, 0, rgb(236, 246, 252), (u8)(170 * k)); }
            } else {
                g.drawSized(g.fxs.ring, s.x + f.x * 15, s.y + f.y * 15 + 2, 10.0f + 18.0f * (cyc - 0.55f), 4.0f + 8.0f * (cyc - 0.55f), 0, rgb(236, 246, 252), (u8)(100 * k * (1 - (cyc - 0.55f) / 0.45f)));
            }
        } else {   // whale: the long back rolls slowly through the surface; near the end of its time up the fluke rises and it dives
            float diving = clampf((life * 0.75f - (life - b.timer)) <= 0 ? ((life - b.timer) - life * 0.75f) / (life * 0.25f) : 0.0f, 0.0f, 1.0f);
            // (whole-sprite rotated blits: the CPU renderer's shortcut draws long thin rotated sprites as a plain line)
            g.draw(art.back, s.x + 2, s.y + 5, b.ang, 1.25f, rgb(0, 0, 0), (u8)(70 * k));   // the whole animal, a long shadow under the surface
            g.drawSized(g.fxs.ring, s.x, s.y + 3, 70 + 6 * std::sin(wallTime * 1.3f + i), 40, 0, rgb(220, 238, 248), (u8)(55 * k));
            if (diving < 0.4f) g.draw(art.back, s.x, s.y, b.ang, 0.95f, tinted(rgb(255, 255, 255)), (u8)(245 * k * (1 - diving * 2.0f)));
            if (diving > 0.15f) {
                float fk = std::sin(clampf((diving - 0.15f) / 0.85f, 0, 1) * 3.14159f);
                Vec2 q = s - f * 22.0f;
                g.drawSized(art.fluke, q.x, q.y - 8 * fk, 48, 30 * fk + 2, 0, tinted(rgb(255, 255, 255)), (u8)(255 * fk));
                g.drawSized(g.fxs.ring, q.x, q.y + 2, 42, 14, 0, rgb(236, 246, 252), (u8)(160 * fk));
            }
        }
        (void)spd;
    }
}

void Game::drawWeatherTint() {
    Gfx& g = g_gfx;
    Color t = ambientTint();
    if (t.r < 252 || t.g < 252 || t.b < 252) {   // one multiply over the battlefield (nothing at all in plain daylight)
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_MOD);
        SDL_SetRenderDrawColor(g.ren, t.r, t.g, t.b, 255);
        SDL_Rect r = { 0, 0, SCREEN_W, VIEW_H };
        SDL_RenderFillRect(g.ren, &r);
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
    }
    float night = nightLevel();
    if (night <= 0.05f) return;
    // the lights of the night: lit windows on every structure, headlights on the vehicles (only what is on screen and seen)
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    float vx0 = cam.x - 64, vy0 = cam.y - 64, vx1 = cam.x + SCREEN_W + 64, vy1 = cam.y + VIEW_H + 64;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.kind == EK_RESOURCE || e.carrier.valid()) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!rev && e.owner != g_sim.humanPlayer && !ex[clampi(tileOf(e.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(e.pos.x), 0, MAP_W - 1)]) continue;
        if (!g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        Vec2 p = worldToScreen(entPos(e));
        if (e.isBuilding()) {
            if (!e.constructed || e.bt().role == BR_WALL || e.bt().role == BR_FWALL || e.bt().role == BR_WAYMAKER) continue;
            const BuildType& bt = e.bt();
            if (bt.role == BR_HEALANT) {   // a Healing Antenna has no windows: its green beacon glows at the top of the mast
                float pulse = 0.75f + 0.25f * std::sin(wallTime * 3.0f + e.gen);
                g.glowAdd(p.x, p.y - 20, 8.0f, Color{120, 255, 170, (u8)(150 * night * pulse)});
                g.glowAdd(p.x, p.y, TILE * 0.9f, Color{120, 255, 170, (u8)(20 * night)});
                continue;
            }
            bool cyber = bt.faction == F_CYBER;
            Color lc = e.owner < 0 ? rgb(255, 200, 120) : (cyber ? rgb(170, 220, 255) : rgb(255, 196, 110));
            int n = std::min(4, 1 + bt.w * bt.h / 4);
            for (int k = 0; k < n; k++) {
                float hx = (hashf(e.gen * 31u + k * 7u + 1u) - 0.5f) * bt.w * TILE * 0.7f, hy = (hashf(e.gen * 17u + k * 13u + 5u) - 0.5f) * bt.h * TILE * 0.5f - 6;
                float flick = 0.85f + 0.15f * std::sin(wallTime * 2.3f + k + e.gen);
                g.glowAdd(p.x + hx, p.y + hy, 9.0f + 3.0f * (k & 1), Color{lc.r, lc.g, lc.b, (u8)(110 * night * flick)});
            }
            g.glowAdd(p.x, p.y, std::max(bt.w, bt.h) * TILE * 0.55f, Color{lc.r, lc.g, lc.b, (u8)(26 * night)});   // light spilling onto the yard
        } else if (e.isUnit() && e.ut().kind == UK_VEH) {
            Vec2 f(std::cos(e.angle), std::sin(e.angle));
            float r = e.radius();
            Color hl = rgb(255, 244, 210);
            g.glowAdd(p.x + f.x * (r + 10), p.y + f.y * (r + 10), 11.0f, Color{hl.r, hl.g, hl.b, (u8)(70 * night)});   // the beam on the ground ahead
            for (int s = -1; s <= 1; s += 2) g.glowAdd(p.x + f.x * r * 0.9f - f.y * s * r * 0.4f, p.y + f.y * r * 0.9f + f.x * s * r * 0.4f, 3.5f, Color{hl.r, hl.g, hl.b, (u8)(170 * night)});
        }
    }
}

void Game::drawRain() {
    Gfx& g = g_gfx;
    float w = weather.rain;
    if (w > 0.02f) {
        int n = (int)(280 * w);
        float t = wallTime;
        const float fall = 900.0f, wind = 170.0f;   // px/s down and across the screen
        Color c = rgb(196, 210, 226, (int)(70 + 70 * w));
        for (int i = 0; i < n; i++) {
            float sx = hashf(i * 2654435761u + 11u), sy = hashf(i * 2246822519u + 7u), sp = 0.75f + 0.5f * hashf(i * 3266489917u + 3u);
            float x = std::fmod(sx * (SCREEN_W + 200) + t * wind * sp - cam.x * 0.25f, (float)(SCREEN_W + 200)) - 100;
            float y = std::fmod(sy * (VIEW_H + 120) + t * fall * sp - cam.y * 0.25f, (float)(VIEW_H + 120)) - 60;
            if (x < -100) x += SCREEN_W + 200;
            if (y < -60) y += VIEW_H + 120;
            float len = 12.0f * sp;
            g.line(x, y, x - wind / fall * len, y - len, c);
        }
        // splashes: rings that flash on the ground where drops land, a fresh set every tenth of a second
        int slot = (int)(t * 10.0f), m = (int)(40 * w);
        for (int i = 0; i < m; i++) {
            u32 hsh = (u32)slot * 9973u + (u32)i * 7919u;
            float x = hashf(hsh) * SCREEN_W, y = hashf(hsh + 1u) * VIEW_H, a = 1.0f - std::fmod(t * 10.0f, 1.0f);
            g.drawSized(g.fxs.ring, x, y, 5.0f + 6.0f * (1 - a), 2.5f + 3.0f * (1 - a), 0, rgb(220, 232, 244), (u8)(80 * a * w));   // (a ripple that widens as it fades)
        }
    }
    if (weather.bolt > 0.02f) {   // lightning: the whole field flashes white for an instant
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_ADD);
        SDL_SetRenderDrawColor(g.ren, 200, 210, 255, (u8)(150 * weather.bolt));
        SDL_Rect r = { 0, 0, SCREEN_W, VIEW_H };
        SDL_RenderFillRect(g.ren, &r);
        SDL_SetRenderDrawBlendMode(g.ren, SDL_BLENDMODE_BLEND);
    }
}
