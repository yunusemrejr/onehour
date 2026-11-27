#include "sim.h"
#include "testutil.h"
#include <chrono>

namespace {
constexpr int X = MAP_W / 2, Y = MAP_H / 2;

void setup(u64 seed) {
    Faction factions[] = {F_CYBER, F_CLANKER};
    bool ai[] = {false, false};
    int difficulty[] = {1, 1}, teams[] = {0, 1};
    g_sim.init(2, factions, ai, difficulty, teams, seed);
    flattenArena(tileCenter(X, Y), 32, 24);
    g_map.cleared.clear();
    g_sim.rain = g_sim.rainTo = 0; g_sim.nextRainRoll = 1e9f;   // (a dry sky unless a check brings the rain)
}

// every tile along the path's segments, sampled at half-tile steps, is free of fire hazard
bool clearOfFire(Vec2 from, const std::vector<Vec2>& path) {
    Vec2 a = from;
    for (const Vec2& b : path) {
        const float l = dist(a, b);
        for (float t = 0; t <= l; t += TILE * 0.5f) {
            const Vec2 p = a + (b - a) * (l > 0 ? t / l : 0.0f);
            if (g_map.hazard[tileOf(p.y) * MAP_W + tileOf(p.x)]) return false;
        }
        a = b;
    }
    return true;
}

void tree(int x, int y) {
    int i = y * MAP_W + x;
    g_map.tiles[i] = T_TREE;
    g_map.blocked[i] |= 1;
    g_map.clearable[i] = 1;
}

void advance(int ticks) {
    for (int i = 0; i < ticks; ++i) { g_sim.step(); g_sim.events.clear(); }
}

std::vector<int> ash() {
    std::vector<int> tiles;
    for (int c : g_map.cleared) if (c >= 0 && (c >> 24) == 3) tiles.push_back(c & 0xFFFFFF);
    std::sort(tiles.begin(), tiles.end());
    return tiles;
}

void bomb(int x, int y) {
    Projectile p{};
    p.pos = p.vel = p.dest = tileCenter(x, y);
    p.target = p.shooter = NOREF;
    p.weapon = W_BOMB_CYBER; p.owner = 0; p.life = 1; p.arcLen = SIM_DT;
    g_sim.projs.push_back(p);
    advance(1);
}
}

bool fireTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("firetest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };

    setup(seed);
    for (int y = Y - 5; y <= Y + 5; ++y) for (int x = X - 5; x <= X + 5; ++x) tree(x, y);
    g_map.touch();
    bomb(X, Y);
    check(!g_sim.fires.empty(), "a bomb impact ignites trees");

    auto spread = [&]() {
        setup(seed);
        for (int y = Y - 14; y <= Y + 14; ++y) for (int x = X - 24; x <= X + 24; ++x) tree(x, y);
        g_map.touch();
        g_sim.windAngle = 0; g_sim.windSpeed = 1;
        for (int y = Y - 5; y <= Y + 5; y += 2) g_sim.igniteFire(X, y);
        advance(22 * SIM_HZ);
        int east = 0, west = 0;
        for (const FireCell& f : g_sim.fires) { east = std::max(east, f.tx - X); west = std::max(west, X - f.tx); }
        advance(31 * SIM_HZ);
        return std::make_pair(std::make_pair(east, west), ash());
    };
    auto first = spread(), second = spread();
    printf("firetest front after 22 s: downwind %d tiles, upwind %d; %zu burnt by 53 s\n", first.first.first, first.first.second, first.second.size());
    check(first.first.first > first.first.second && first.first.first >= 3, "strong wind carries fire farther downwind");
    check(!first.second.empty() && first == second, "the same seed burns exactly the same tiles across fresh matches");

    setup(seed);
    for (int y = Y - 1; y <= Y + 1; ++y) for (int x = X - 4; x <= X + 4; ++x) {
        if (y == Y && x != X - 4 && x != X + 4) continue;
        int i = y * MAP_W + x; g_map.tiles[i] = T_ROCK; g_map.blocked[i] |= 1;
    }
    tree(X, Y); g_map.touch();
    Vec2 left = tileCenter(X - 3, Y), right = tileCenter(X + 3, Y);
    std::vector<Vec2> path;
    bool before = !g_map.sameGround(left, right) && !g_map.findPath(left, right, path);
    g_sim.igniteFire(X, Y);
    advance(31 * SIM_HZ);
    auto burnt = ash();
    bool after = g_map.passable(X, Y) && !g_map.clearable[Y * MAP_W + X] && g_sim.fireAt(X, Y) == 0
                 && std::binary_search(burnt.begin(), burnt.end(), Y * MAP_W + X)
                 && g_map.sameGround(left, right) && g_map.findPath(left, right, path);
    Ref walker = g_sim.spawnUnit(U_C_INF1, 0, left);
    g_sim.cmdMove({walker}, right, false);
    advance(6 * SIM_HZ);
    Entity* arrived = g_sim.get(walker);
    check(before && after && arrived && tileOf(arrived->pos.x) >= X + 2, "burnout logs ash, opens the cached corridor and lets infantry walk through");

    setup(seed);
    tree(X, Y); g_map.touch();
    Ref idle = g_sim.spawnUnit(U_C_INF1, 0, tileCenter(X + 1, Y));
    float hp = g_sim.get(idle)->hp;
    g_sim.igniteFire(X, Y);
    advance(SIM_HZ / 2);
    const Entity* hurt = g_sim.get(idle);
    check(hurt && hurt->hp < hp, "infantry next to fire take damage");
    advance(3 * SIM_HZ);
    const Entity* safe = g_sim.get(idle);
    check(safe && (std::abs(tileOf(safe->pos.x) - X) > 1 || std::abs(tileOf(safe->pos.y) - Y) > 1), "idle infantry walk to safety");

    // rain puts fires out: the trees stand, the ground beside them is safe again
    setup(seed);
    for (int y = Y - 5; y <= Y + 5; ++y) for (int x = X - 5; x <= X + 5; ++x) tree(x, y);
    g_map.touch();
    g_sim.windSpeed = 0.5f;
    for (int y = Y - 2; y <= Y + 2; y += 2) g_sim.igniteFire(X, y);
    advance(3 * SIM_HZ);
    const bool burning = !g_sim.fires.empty() && g_map.hazard[Y * MAP_W + X + 1] > 0;
    g_sim.rain = g_sim.rainTo = 1.0f;
    advance(12 * SIM_HZ);
    printf("firetest rain: %d fires put out, %zu still burning after 12 s of downpour\n", g_sim.fireDoused, g_sim.fires.size());
    check(burning && g_sim.fires.empty() && g_sim.fireDoused > 0 && g_map.tile(X, Y) == T_TREE && g_map.hazard[Y * MAP_W + X + 1] == 0,
          "a downpour puts the fire out, leaves the trees standing and clears the danger round them");

    // a route goes round a blaze: a copse with a way past it on either side; its north edge burns, so the path takes the south
    setup(seed);
    for (int y = Y - 1; y <= Y + 1; ++y) for (int x = X - 2; x <= X + 2; ++x) tree(x, y);
    g_map.touch();
    for (int x = X - 2; x <= X + 2; ++x) g_sim.igniteFire(x, Y - 1);
    advance(SIM_HZ);
    std::vector<Vec2> around;
    const Vec2 from = tileCenter(X - 7, Y - 2), to = tileCenter(X + 7, Y - 2);
    const bool found = g_map.findPath(from, to, around);
    check(found && clearOfFire(from, around), "a route goes round the ground next to a fire when there is a way round");

    // a unit busy fighting steps out of the flames, then takes its fight up again
    setup(seed);
    tree(X, Y); g_map.touch();
    Ref fighter = g_sim.spawnUnit(U_C_INF1, 0, tileCenter(X + 1, Y));
    Ref dummy = g_sim.spawnUnit(U_K_INF1, 1, tileCenter(X + 4, Y));
    { Entity* d = g_sim.get(dummy); d->hp = d->maxHp = 1e6f; d->disabledUntil = 1e9f; }
    g_sim.cmdAttack({fighter}, dummy);
    advance(SIM_HZ / 2);
    g_sim.igniteFire(X, Y);
    g_sim.fires.back().heat = 1.0f;
    advance(3 * SIM_HZ);
    const Entity* fe = g_sim.get(fighter);
    if (fe) printf("firetest fighter: tile (%d,%d) order %d resume %d\n", tileOf(fe->pos.x), tileOf(fe->pos.y), (int)fe->order, (int)fe->resumeOrder);
    check(fe && (std::abs(tileOf(fe->pos.x) - X) > 1 || std::abs(tileOf(fe->pos.y) - Y) > 1) && (fe->order == O_ATTACK || fe->resumeOrder == O_ATTACK),
          "a unit fighting next to a fire steps out of it and keeps its target");

    // the torch: infantry beside a tree set it alight; a tank cannot, nor can anyone from afar or in a downpour
    setup(seed);
    tree(X, Y); tree(X + 6, Y); g_map.touch();
    Ref nearInf = g_sim.spawnUnit(U_C_INF1, 0, tileCenter(X + 1, Y)), farInf = g_sim.spawnUnit(U_C_INF1, 0, tileCenter(X + 4, Y));
    Ref tank = g_sim.spawnUnit(U_C_TANK, 0, tileCenter(X + 7, Y));
    const bool tankNo = !g_sim.torchTree(*g_sim.get(tank), X + 6, Y), farNo = !g_sim.torchTree(*g_sim.get(farInf), X, Y);
    g_sim.rain = 0.8f;
    const bool wetNo = !g_sim.torchTree(*g_sim.get(nearInf), X, Y);
    g_sim.rain = 0;
    const bool lit = g_sim.torchTree(*g_sim.get(nearInf), X, Y) && g_sim.fireAt(X, Y) > 0;
    check(tankNo && farNo && wetNo && lit, "infantry beside a tree can set it alight; not a tank, not from afar, not in a downpour");

    setup(seed);
    bool capOk = true;
    for (int y = Y - 13; y <= Y + 13; ++y) for (int x = X - 13; x <= X + 13; ++x) {
        tree(x, y);
        bool space = g_sim.fires.size() < Sim::FIRE_CAP;
        capOk &= g_sim.igniteFire(x, y) == space && g_sim.fires.size() <= Sim::FIRE_CAP;
    }
    g_map.touch();
    auto total = std::chrono::steady_clock::duration::zero(), peak = total;
    constexpr int samples = 40;
    for (int i = 0; i < samples; ++i) {
        g_sim.tick += SIM_HZ / 2;
        auto begin = std::chrono::steady_clock::now();
        g_sim.updateFire();
        auto elapsed = std::chrono::steady_clock::now() - begin;
        total += elapsed; peak = std::max(peak, elapsed);
        capOk &= g_sim.fires.size() == Sim::FIRE_CAP;
    }
    using Micros = std::chrono::duration<double, std::micro>;
    printf("firetest update at %d active: %.2f us mean, %.2f us peak (%d half-second updates, %zu entity slots)\n",
           Sim::FIRE_CAP, Micros(total).count() / samples, Micros(peak).count(), samples, g_sim.ents.size());
    check(capOk, "the active list holds at most 600 fires and refuses more ignitions");
    return ok;
}
