#include "sim.h"
#include <unordered_set>

namespace {
constexpr int FIRE_INTERVAL = SIM_HZ / 2;
constexpr float FIRE_DT = float(FIRE_INTERVAL) / SIM_HZ;
constexpr float FUEL_SECONDS = Sim::FIRE_FUEL;
constexpr float SPREAD_SECONDS = 4.0f;   // (a tile spreads only while it first flares: about 1.3 neighbours lit on average, so most fires
                                          // burn a patch and gutter out, and a strong wind drives a long tongue through a forest)
constexpr float TURN = 6.28318530718f;
constexpr int DX[] = {1, 0, -1, 0, 1, -1, -1, 1};
constexpr int DY[] = {0, 1, 0, -1, 1, 1, -1, -1};

float exposure(const Sim& sim, int x0, int y0, int x1, int y1) {
    float heat = 0;
    for (int y = y0 - 1; y <= y1 + 1; ++y) for (int x = x0 - 1; x <= x1 + 1; ++x)
        heat = std::max(heat, sim.fireAt(x, y));
    return heat;
}

// The search only queues exposed tiles, so even a trapped unit never scans the map.
bool fireExit(const Sim& sim, const Entity& e, int x, int y, Vec2& dest) {
    if (!inMap(x, y)) return false;
    std::vector<int> open{x + y * MAP_W};
    std::unordered_set<int> seen{open.front()};
    for (size_t i = 0; i < open.size(); ++i) {
        int cx = open[i] % MAP_W, cy = open[i] / MAP_W;
        for (int d = 0; d < 8; ++d) {
            int nx = cx + DX[d], ny = cy + DY[d];
            if (!inMap(nx, ny) || !Sim::walkable(e, nx, ny)) continue;   // (a tile this unit can stand on, by its own way of moving)
            if (d >= 4 && (!Sim::walkable(e, nx, cy) || !Sim::walkable(e, cx, ny))) continue;
            if (!seen.insert(nx + ny * MAP_W).second) continue;
            if (exposure(sim, nx, ny, nx, ny) == 0) { dest = tileCenter(nx, ny); return true; }
            open.push_back(nx + ny * MAP_W);
        }
    }
    return false;
}
}

void Sim::resetFire(u64 seed) {
    fires.clear();
    fires.reserve(FIRE_CAP);
    fireBurnt = fireIgnitions = firePeak = fireDoused = fireTorched = 0;
    fireIndex.assign(MAP_W * MAP_H, 0);
    for (int t : hazardTiles) g_map.hazard[t] = 0;
    hazardTiles.clear();
    // The match seed drives both streams, but wind never consumes a combat roll.
    fireRng = Rng(seed ^ 0xa0761d6478bd642full);
    windAngle = fireRng.f(0, TURN);
    windSpeed = fireRng.f(0.35f, 0.85f);
    // the rain: a quarter of the matches start under it; then dry spells and showers or storms take turns (the drawn sky follows: weather.cpp)
    const bool wet = fireRng.f() < 0.25f;
    rain = rainTo = wet ? fireRng.f(0.45f, 1.0f) : 0.0f;
    nextRainRoll = wet ? fireRng.f(80, 210) : fireRng.f(60, 200);
}

bool Sim::torchTree(Entity& u, int tx, int ty) {
    if (!u.alive || !u.isUnit() || u.isAir() || u.ut().kind != UK_INF || u.carrier.valid() || !inMap(tx, ty)) return false;
    if (g_map.tile(tx, ty) != T_TREE || rain > 0.5f || dist(u.pos, tileCenter(tx, ty)) > TILE * 1.75f) return false;
    if (!igniteFire(tx, ty)) return false;
    fires.back().heat = 0.5f;   // (a torch takes hold at once)
    fireTorched++;
    return true;
}

float Sim::fireAt(int tx, int ty) const {
    if (!inMap(tx, ty) || fireIndex.empty()) return 0;
    u16 slot = fireIndex[ty * MAP_W + tx];
    return slot && g_map.tile(tx, ty) == T_TREE ? fires[slot - 1].heat : 0;
}

bool Sim::igniteFire(int tx, int ty) {
    if (fires.size() >= FIRE_CAP || fireIndex.empty() || !inMap(tx, ty) || g_map.tile(tx, ty) != T_TREE) return false;
    u16& slot = fireIndex[ty * MAP_W + tx];
    if (slot) return false;
    fires.push_back({(u16)tx, (u16)ty, 0.2f, FUEL_SECONDS});
    slot = (u16)fires.size();
    fireIgnitions++; firePeak = std::max(firePeak, (int)fires.size());
    return true;
}

void Sim::igniteBlast(Vec2 at, float radiusTiles, float damage, bool ring) {
    if (fires.size() >= FIRE_CAP || damage <= 0) return;
    const float r = radiusTiles + 1.0f;
    const float strength = clampf(damage / 250.0f + radiusTiles * 0.045f, 0, 1) * (1.0f - 0.8f * rain);   // (wet trees hardly catch)
    const int x0 = std::max(0, tileOf(at.x - r * TILE)), x1 = std::min(MAP_W - 1, tileOf(at.x + r * TILE));
    const int y0 = std::max(0, tileOf(at.y - r * TILE)), y1 = std::min(MAP_H - 1, tileOf(at.y + r * TILE));
    for (int y = y0; y <= y1; ++y) for (int x = x0; x <= x1; ++x) {
        if (g_map.tile(x, y) != T_TREE || fireAt(x, y) != 0) continue;
        const float d = dist(at, tileCenter(x, y)) / TILE;
        if (d > r || (ring && d < radiusTiles * 0.70f)) continue;
        const float chance = ring ? 0.50f * strength : 0.5f * strength * (1.0f - 0.94f * d / r);
        if (fireRng.f() < chance && igniteFire(x, y) && fires.size() == FIRE_CAP) return;
    }
}

void Sim::updateFire() {
    if (tick % FIRE_INTERVAL != 0) return;
    windAngle += fireRng.f(-0.025f, 0.025f);
    windSpeed = clampf(windSpeed + fireRng.f(-0.025f, 0.025f) + (0.60f - windSpeed) * 0.012f, 0, 1);
    if (fireRng.f() < 0.005f) windAngle += fireRng.f(-0.9f, 0.9f);
    if (windAngle < 0) windAngle += TURN;
    if (windAngle >= TURN) windAngle -= TURN;
    if (time >= nextRainRoll) {
        if (rainTo > 0) { rainTo = 0; nextRainRoll = time + fireRng.f(150, 320); }                         // the rain stops: a dry spell
        else if (fireRng.f() < 0.45f) { rainTo = fireRng.f(0.45f, 1.0f); nextRainRoll = time + fireRng.f(80, 210); }   // a shower or a storm
        else nextRainRoll = time + fireRng.f(60, 130);
    }
    rain += clampf(rainTo - rain, -FIRE_DT / 18.0f, FIRE_DT / 18.0f);   // (the sky turns over in about 18 s)
    for (int t : hazardTiles) g_map.hazard[t] = 0;
    hazardTiles.clear();
    if (fires.empty()) return;

    for (size_t i = 0; i < fires.size();) {
        FireCell& f = fires[i];
        f.fuel -= FIRE_DT;
        bool tree = g_map.tile(f.tx, f.ty) == T_TREE;
        const bool doused = tree && f.fuel > 0 && f.heat <= 0.05f && rain > 0.2f;   // (the rain put it out: the tree stands, scorched)
        if (!tree || f.fuel <= 0 || doused) {
            if (tree && !doused) { g_map.burnTree(f.tx, f.ty); fireBurnt++; }
            if (doused) fireDoused++;
            fireIndex[f.ty * MAP_W + f.tx] = 0;
            f = fires.back();
            fires.pop_back();
            if (i < fires.size()) fireIndex[f.ty * MAP_W + f.tx] = (u16)(i + 1);
            continue;
        }
        // rain holds the flames down: at 0.7 and over a fire dies in seconds; a light shower only slows it
        f.heat = std::max(0.0f, std::min({1.0f, f.heat + FIRE_DT * 0.5f * (1.0f - rain), f.fuel / 6.0f}) - FIRE_DT * 0.35f * rain);
        ++i;
    }
    if (fires.empty()) return;
    // the ground next to a fire costs more to path over (Map::moveCost): columns go round a blaze when there is a way round
    for (const FireCell& f : fires) {
        const u8 pen = (u8)(4 + 12 * f.heat);   // 1 to 4 extra tiles per step
        for (int y = f.ty - 1; y <= f.ty + 1; ++y) for (int x = f.tx - 1; x <= f.tx + 1; ++x) {
            if (!inMap(x, y)) continue;
            u8& h = g_map.hazard[y * MAP_W + x];
            if (!h) hazardTiles.push_back(y * MAP_W + x);
            h = std::max(h, pen);
        }
    }

    const Vec2 wind(std::cos(windAngle), std::sin(windAngle));
    float chance[8];
    for (int d = 0; d < 8; ++d) {
        float along = (DX[d] * wind.x + DY[d] * wind.y) * (d < 4 ? 1.0f : 0.70710678f);
        float head = std::max(0.0f, along);
        chance[d] = 0.012f * (1.0f + 0.87f * windSpeed * std::min(0.0f, along))
                    + 0.10f * windSpeed * head * head;
    }
    const float wet = 1.0f - 0.9f * rain;
    for (float& c : chance) c *= wet;
    const size_t existing = fires.size();   // a new ignition cannot spread until the next interval
    for (size_t i = 0; i < existing && fires.size() < FIRE_CAP; ++i) {
        const FireCell f = fires[i];
        if (f.fuel < FUEL_SECONDS - SPREAD_SECONDS) continue;
        for (int d = 0; d < 8 && fires.size() < FIRE_CAP; ++d) {
            int x = f.tx + DX[d], y = f.ty + DY[d];
            if (inMap(x, y) && g_map.tile(x, y) == T_TREE && !fireIndex[y * MAP_W + x] && fireRng.f() < chance[d] * f.heat)
                igniteFire(x, y);
        }
        if (fires.size() < FIRE_CAP && fireRng.f() < 0.012f * windSpeed * f.heat * (1.0f - rain)) {
            int reach = fireRng.range(2, 3);
            igniteFire(f.tx + (int)std::lround(wind.x * reach), f.ty + (int)std::lround(wind.y * reach));
        }
    }

    for (size_t i = 0; i < ents.size(); ++i) {
        Entity& e = ents[i];
        if (!e.alive || e.kind == EK_RESOURCE || e.carrier.valid() || e.fall > 0) continue;
        if (e.isUnit() && (e.isAir() || e.ut().naval)) continue;
        int x = tileOf(e.pos.x), y = tileOf(e.pos.y);
        float heat = e.isBuilding() ? exposure(*this, e.tx, e.ty, e.tx + e.bt().w - 1, e.ty + e.bt().h - 1)
                                    : exposure(*this, x, y, x, y);
        if (heat <= 0) continue;
        float dps = e.isBuilding() ? 2.0f : (e.ut().kind == UK_INF ? 16.0f : (e.armor() == AR_HEAVY ? 0.8f : 3.0f));
        Ref ref = refOf(e);
        applyDamage(e, dps * heat * FIRE_DT, -1, NOREF, nullptr);
        Entity* survivor = get(ref);
        if (!survivor || !survivor->isUnit() || survivor->disabledUntil > time) continue;
        Entity& u = *survivor;
        if (u.order == O_MOVE || u.order == O_ATTACKMOVE) {   // on the way somewhere: a new route, which now goes round the flames where it can
            if (time >= u.evadeUntil && heat > 0.25f) { requestPath(u, u.target); u.evadeUntil = time + 2.0f; }
            continue;
        }
        if (u.order != O_IDLE && heat < 0.25f) continue;   // (busy units shrug off a dying ember)
        Vec2 dest;
        if (!fireExit(*this, u, x, y, dest)) continue;
        if (u.order != O_IDLE && u.order != O_GUARDPOS && u.order != O_REARM) {   // step out of the fire, then pick the work up again
            u.resumeOrder = u.order; u.resumePost = u.postOrder; u.resumeTarget = u.target;
        }
        const Ref keep = u.targetEnt;
        moveAside(u, dest, 1.5f);
        if (u.resumeOrder != O_IDLE) u.targetEnt = keep;
    }
}
