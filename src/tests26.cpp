// One Hour - headless tests for the Phantom Jeep (--phantomtest)
//  * stealth: only an aircraft within PHANTOM_DETECT tiles and a sniper's scope find it - never a tank, a turret or a bunker; no sniper may target it
//  * a warhead at ground zero leaves it at PHANTOM_NUKE_FLOOR of its health and fallout never takes it lower
//  * ground fire does PHANTOM_ARMOR of its damage; aircraft hit harder the more distinct aircraft are on it, and it can die to them
//  * it mends itself, slowly under fire and faster after PHANTOM_CALM seconds of calm
//  * TNT: cmdPlant drives it to an enemy structure, plants (PHANTOM_TNT_PLANT) and blows (PHANTOM_TNT_FUSE); one charge is spent and comes back
//  * climb: findPath over rock, water and an ordinary wall succeeds; a ring of Bastion Walls cannot be crossed
//  * one at a time per army; a rich computer army with F2_PHANTOM buys one
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static bool g_okP = true;
static bool failP(const char* m) { fprintf(stderr, "phantomtest: %s\n", m); g_okP = false; return false; }
#define CHECKP(c, m) do { if (!(c)) return failP(m); } while (0)

static void runP(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

// Two human armies (no computer play) on a clear, flat arena away from the bases; every starting unit removed
static Vec2 initP(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, false };
    int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    const Vec2 c = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.45f), 9);
    flattenArena(c, 14, 14);
    return c;
}

// the nearest free spot for a structure of 'type' round 'at' (like tests_calls.cpp's placeNearC)
static Ref placeNearP(int player, int type, Vec2 at) {
    const int cx = tileOf(at.x), cy = tileOf(at.y);
    for (int r = 0; r < 16; r++) for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
        if (std::max(std::abs(x - cx), std::abs(y - cy)) != r) continue;
        if (g_sim.canPlace(player, type, x, y)) return g_sim.placeBuilding(type, player, x, y, true);
    }
    return NOREF;
}

static bool seenP(Ref r, int by) { Entity* e = g_sim.get(r); return e && g_sim.visibleTo(*e, by); }

static bool stealthTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    Entity* p = g_sim.get(ph);
    CHECKP(p, "the Phantom Jeep did not spawn");
    CHECKP(p->ut().stealth == ST_PHANTOM, "the Phantom Jeep is not stealthy");
    p->disabledUntil = g_sim.time + 3600.0f;   // (its own guns stay out of this: it would otherwise shoot the detectors)
    // a tank, a turret and a bunker right next to it find nothing
    g_sim.spawnUnit(U_K_TANK, 1, c + Vec2(2 * TILE, 0));
    Ref turret = placeNearP(1, B_K_MG, c + Vec2(4 * TILE, 0));
    Ref bunker = placeNearP(1, B_K_BUNKER, c + Vec2(-6 * TILE, 0));
    CHECKP(g_sim.get(turret) && g_sim.get(bunker), "could not place the turret and the bunker");
    runP(4);
    CHECKP(!seenP(ph, 1), "a tank, a turret or a bunker finds the Phantom Jeep");
    CHECKP(seenP(ph, 0), "its own army cannot see it");
    // a sniper's scope finds it inside its 7 tiles, not outside
    Ref sn = g_sim.spawnUnit(U_K_SNIPER, 1, c + Vec2(5 * TILE, 3 * TILE));
    CHECKP(g_sim.get(sn), "could not spawn the sniper");
    runP(1.2f);
    CHECKP(seenP(ph, 1), "a sniper within 7 tiles does not find the Phantom Jeep");
    g_sim.get(sn)->pos = c + Vec2(12 * TILE, 0);
    runP(4);
    CHECKP(!seenP(ph, 1), "a sniper beyond 7 tiles still finds the Phantom Jeep");
    // aircraft: a helicopter, a spy drone and a jet find it inside PHANTOM_DETECT and lose it beyond
    const int air[3] = { U_K_AIR, U_K_SDRONE, U_K_JET };
    for (int k = 0; k < 3; k++) {
        Ref a = g_sim.spawnUnit(air[k], 1, c + Vec2(0.6f * TILE, 0));   // (over its head, so it cannot leave the range inside the sweep)
        CHECKP(g_sim.get(a), "could not spawn an aircraft");
        g_sim.get(a)->alt = 1;
        runP(0.4f);
        CHECKP(seenP(ph, 1), "an aircraft within 7 tiles does not find the Phantom Jeep");
        if (Entity* ae = g_sim.get(a)) g_sim.destroy(*ae, false);
        runP(PHANTOM_LINGER + 1.5f);
        CHECKP(!seenP(ph, 1), "the Phantom Jeep stays found after the aircraft is gone");
        Ref far = g_sim.spawnUnit(air[k], 1, c + Vec2(20 * TILE, 0));
        CHECKP(g_sim.get(far), "could not spawn the far aircraft");
        g_sim.get(far)->alt = 1;
        runP(0.4f);
        CHECKP(!seenP(ph, 1), "an aircraft beyond 7 tiles finds the Phantom Jeep");
        if (Entity* ae = g_sim.get(far)) g_sim.destroy(*ae, false);
    }
    return true;
}

static bool sniperTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    Entity* p = g_sim.get(ph);
    CHECKP(p, "the Phantom Jeep did not spawn");
    p->disabledUntil = g_sim.time + 3600.0f;
    Ref sn = g_sim.spawnUnit(U_K_SNIPER, 1, c + Vec2(4 * TILE, 0));
    CHECKP(g_sim.get(sn), "could not spawn the sniper");
    runP(1.0f);
    CHECKP(seenP(ph, 1), "the sniper's scope did not find the Phantom Jeep");
    CHECKP(!g_sim.canTarget(*g_sim.get(sn), *g_sim.get(ph)), "a sniper can target the Phantom Jeep");
    return true;
}

static bool nukeTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    CHECKP(g_sim.get(ph), "the Phantom Jeep did not spawn");
    runP(0.1f);   // (into the sim grid the blast searches)
    Sim::Nuke n;
    n.from = c + Vec2(300, 300); n.pos = c; n.owner = 1; n.t = 0; n.scale = 1.0f; n.power = 1.0f;
    g_sim.nukeBlastPublic(n);
    Entity* p = g_sim.get(ph);
    CHECKP(p && p->alive, "a warhead at ground zero killed the Phantom Jeep");
    const float floorHp = p->maxHp * PHANTOM_NUKE_FLOOR;
    fprintf(stderr, "phantomtest: hp after a ground-zero warhead %.0f of %.0f\n", p->hp, p->maxHp);
    CHECKP(p->hp >= floorHp - 1 && p->hp <= floorHp + 1, "a warhead at ground zero does not leave it at PHANTOM_NUKE_FLOOR of its health");
    // fallout and further warhead damage never take it under the floor
    for (int k = 0; k < 200; k++) {
        g_sim.applyDamagePublic(*p, 5000, 1, NOREF);
        if (g_sim.get(ph) == nullptr) break;
    }
    CHECKP(g_sim.get(ph) != nullptr, "warhead or fallout damage killed the Phantom Jeep");
    CHECKP(g_sim.get(ph)->hp >= floorHp - 1, "warhead or fallout damage took it under PHANTOM_NUKE_FLOOR");
    return true;
}

static bool damageTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    Entity* p = g_sim.get(ph);
    CHECKP(p, "the Phantom Jeep did not spawn");
    const float dmgMul = g_sim.players[1].damageMul();
    // ground fire: PHANTOM_ARMOR of the raw damage (the public test hook applies no weapon table)
    Ref tk = g_sim.spawnUnit(U_K_TANK, 1, c + Vec2(3 * TILE, 0));
    CHECKP(g_sim.get(tk), "could not spawn the tank");
    p->hp = p->maxHp;
    g_sim.applyDamagePublic(*p, 1000, 1, tk);
    const float ground = (p->maxHp - p->hp) / (1000.0f * dmgMul);
    CHECKP(std::fabs(ground - PHANTOM_ARMOR) < 0.02f, "ground fire does not do PHANTOM_ARMOR of its damage");
    // aircraft: one aircraft does the base multiplier, a second and a third raise it, the same aircraft again does not
    const int heli = U_K_AIR;
    const Vec2 farAir = c + Vec2(40 * TILE, 40 * TILE);
    Ref a1 = g_sim.spawnUnit(heli, 1, farAir), a2 = g_sim.spawnUnit(heli, 1, farAir + Vec2(0, TILE)), a3 = g_sim.spawnUnit(heli, 1, farAir + Vec2(0, 2 * TILE));
    CHECKP(g_sim.get(a1) && g_sim.get(a2) && g_sim.get(a3), "could not spawn the aircraft");
    for (Ref r : { a1, a2, a3 }) g_sim.get(r)->alt = 1;
    auto hit = [&](Ref a) {
        p->hp = p->maxHp;
        g_sim.applyDamagePublic(*p, 1000, 1, a);
        return (p->maxHp - p->hp) / (1000.0f * dmgMul);
    };
    const float m1 = hit(a1), m1b = hit(a1), m2 = hit(a2), m3 = hit(a3);
    CHECKP(std::fabs(m1 - PHANTOM_AIR_BASE) < 0.02f && std::fabs(m1b - PHANTOM_AIR_BASE) < 0.02f, "one aircraft does not do the base multiplier (or the same aircraft counts twice)");
    CHECKP(std::fabs(m2 - (PHANTOM_AIR_BASE + PHANTOM_AIR_STEP)) < 0.02f, "a second distinct aircraft does not raise the multiplier");
    CHECKP(std::fabs(m3 - std::min(PHANTOM_AIR_CAP, PHANTOM_AIR_BASE + 2 * PHANTOM_AIR_STEP)) < 0.02f, "a third distinct aircraft does not raise the multiplier again");
    runP(PHANTOM_AIR_WINDOW + 0.5f);
    CHECKP(std::fabs(hit(a1) - PHANTOM_AIR_BASE) < 0.02f, "the aircraft count does not fall back after PHANTOM_AIR_WINDOW");
    // aircraft can kill it
    p->hp = 100;
    g_sim.applyDamagePublic(*p, 100000, 1, a1);
    CHECKP(g_sim.get(ph) == nullptr || g_sim.get(ph)->hp <= 0, "aircraft cannot kill the Phantom Jeep");
    return true;
}

static bool regenTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    Entity* p = g_sim.get(ph);
    CHECKP(p, "the Phantom Jeep did not spawn");
    p->hp = p->maxHp * 0.4f;
    p->lastDamaged = g_sim.time;
    runP(2);
    const float hot = p->hp - p->maxHp * 0.4f;
    CHECKP(hot > 0, "the Phantom Jeep does not mend at all");
    runP(PHANTOM_CALM + 1);
    const float h1 = p->hp;
    runP(2);
    const float calm = p->hp - h1;
    CHECKP(calm > hot * 2.0f, "the Phantom's self-repair is not faster after PHANTOM_CALM seconds without damage");
    runP(120);
    CHECKP(std::fabs(p->hp - p->maxHp) < 1, "the Phantom Jeep does not mend to full health");
    return true;
}

static bool tntTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, c);
    Entity* p = g_sim.get(ph);
    CHECKP(p, "the Phantom Jeep did not spawn");
    Ref site = placeNearP(1, B_K_POWER, c + Vec2(9 * TILE, 0));
    Entity* b = g_sim.get(site);
    CHECKP(b, "could not place the enemy structure");
    CHECKP(p->tntLeft == PHANTOM_TNT_MAX, "the Phantom Jeep does not start with its charges");
    const float hp0 = b->hp;
    CHECKP(g_sim.cmdPlant({ ph }, site), "cmdPlant refused a charge on an enemy structure next to it");
    CHECKP(g_sim.get(ph)->order == O_PLANT, "cmdPlant did not send it to plant TNT");
    bool planted = false;
    for (int t = 0; t < (int)(20 * SIM_HZ) && !planted; t++) { g_sim.step(); g_sim.events.clear(); planted = g_sim.get(ph) && g_sim.get(ph)->tntLeft == PHANTOM_TNT_MAX - 1; }
    CHECKP(planted, "the Phantom Jeep did not plant a charge");
    CHECKP(!g_sim.tnts.empty(), "no charge sits on the structure after planting");
    for (int t = 0; t < (int)((PHANTOM_TNT_FUSE + 2) * SIM_HZ) && !g_sim.tnts.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
    CHECKP(g_sim.tnts.empty(), "the charge outlived its fuse");
    b = g_sim.get(site);
    CHECKP(!b || b->hp <= hp0 - PHANTOM_TNT_DAMAGE + 1, "the charge did not take PHANTOM_TNT_DAMAGE or more off the structure");
    CHECKP(g_sim.get(ph) && g_sim.get(ph)->tntLeft == PHANTOM_TNT_MAX - 1, "the planted charge was not spent");
    // the charges recharge, one per PHANTOM_TNT_RECHARGE seconds
    runP(PHANTOM_TNT_RECHARGE + 2);
    CHECKP(g_sim.get(ph) && g_sim.get(ph)->tntLeft == PHANTOM_TNT_MAX, "a spent charge did not come back");
    // a Bastion Wall is never a TNT target
    Ref fw = placeNearP(1, B_K_FWALL, c + Vec2(0, 8 * TILE));
    CHECKP(g_sim.get(fw), "could not place a Bastion Wall");
    CHECKP(!g_sim.plantable(0, *g_sim.get(fw)), "a Bastion Wall is a TNT target");
    return true;
}

static bool climbTest(u64 seed) {
    const Vec2 c = initP(seed);
    const int tx = tileOf(c.x), ty = tileOf(c.y);
    // a band of rock, a strip of water and an ordinary wall across the way east
    for (int y = ty - 3; y <= ty + 3; y++) {
        const int ir = (y) * MAP_W + tx + 2, iw = (y) * MAP_W + tx + 4;
        g_map.tiles[ir] = T_ROCK;  g_map.blocked[ir] |= 1;
        g_map.tiles[iw] = T_WATER; g_map.blocked[iw] |= 1;
    }
    g_map.touch();
    CHECKP(g_sim.canPlace(1, B_K_WALL, tx + 6, ty), "the arena has no free spot for the ordinary wall");
    Ref wall = g_sim.placeBuilding(B_K_WALL, 1, tx + 6, ty, true);
    CHECKP(g_sim.get(wall), "could not place the ordinary wall");
    g_map.touch();
    std::vector<Vec2> path;
    const Vec2 from = tileCenter(tx, ty), to = tileCenter(tx + 8, ty);
    CHECKP(g_map.findPath(from, to, path, MAP_W * MAP_H, PM_CLIMB), "the climb route does not cross rock, water and an ordinary wall");
    CHECKP(g_map.lineClear(from, to, PM_CLIMB), "the straight line over rock, water and an ordinary wall is not clear for it");
    CHECKP(!g_map.lineClear(from, to, PM_WALK), "a ground unit's straight line over rock and water is clear");
    // it drives there over all of it
    Ref ph = g_sim.spawnUnit(U_C_PHANTOM, 0, from);
    CHECKP(g_sim.get(ph), "the Phantom Jeep did not spawn");
    g_sim.cmdMove({ ph }, to, false);
    runP(14);
    Entity* p = g_sim.get(ph);
    CHECKP(p && dist(p->pos, to) < 2 * TILE, "the Phantom Jeep did not drive over rock, water and the wall to its spot");
    // a closed ring of Bastion Walls round a goal cannot be crossed: the route fails or stops outside it
    const int gx = tx - 6, gy = ty;
    for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
        if (dx == 0 && dy == 0) continue;
        CHECKP(g_sim.canPlace(1, B_C_FWALL, gx + dx, gy + dy), "the arena has no free spot for the Bastion ring");
        CHECKP(g_sim.get(g_sim.placeBuilding(B_C_FWALL, 1, gx + dx, gy + dy, true)), "could not place a Bastion Wall");
    }
    g_map.touch();
    const Vec2 west = tileCenter(gx, gy);
    std::vector<Vec2> path2;
    const bool ok = g_map.findPath(from, west, path2, MAP_W * MAP_H, PM_CLIMB);
    bool inside = false;
    for (const Vec2& w : path2) if (std::abs(tileOf(w.x) - gx) <= 1 && std::abs(tileOf(w.y) - gy) <= 1) inside = true;
    CHECKP(!ok || !inside, "a climb route runs through a closed ring of Bastion Walls");
    CHECKP(!g_map.lineClear(from, west, PM_CLIMB), "the straight line through a Bastion Wall is clear for the Phantom Jeep");
    return true;
}

static bool oneTest(u64 seed) {
    const Vec2 c = initP(seed);
    Ref fac = placeNearP(0, B_C_FACTORY, c + Vec2(-8 * TILE, 0));
    Ref tech = placeNearP(0, B_C_TECH, c + Vec2(8 * TILE, 0));
    CHECKP(g_sim.get(fac) && g_sim.get(tech), "could not place a factory and a tech structure");
    for (int k = 0; k < 3; k++) placeNearP(0, B_C_POWER, c + Vec2(0, (6 + 4 * k) * TILE));   // (full power: a starved factory builds slowly)
    CHECKP(g_sim.unitAvailable(0, U_C_PHANTOM), "the Phantom Jeep is not available with a factory and a tech structure");
    CHECKP(g_sim.cmdTrain(fac, U_C_PHANTOM), "the first Phantom Jeep cannot be queued");
    CHECKP(!g_sim.cmdTrain(fac, U_C_PHANTOM), "a second Phantom Jeep was queued");
    CHECKP(g_sim.phantomInProduction(0), "a queued Phantom Jeep does not count");
    runP(UNITS[U_C_PHANTOM].buildTime + 15);
    CHECKP(g_sim.phantomOf(0).valid(), "the Phantom Jeep was never finished");
    CHECKP(!g_sim.cmdTrain(fac, U_C_PHANTOM), "a second Phantom Jeep was queued with one in the field");
    return true;
}

static bool aiTest(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { true, true };
    int diff[2] = { 3, 3 }, team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 400000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (int p = 0; p < 2; p++) { g_ai.brainEnabled[p] = false; g_ai.proEnabled[p] = true; g_ai.smartEnabled[p] = true; g_ai.featMask[p] = FEAT_DEFAULT; g_ai.featMask2[p] = F2_DEFAULT; g_ai.brainOverride[p] = nullptr; }
    g_ai.init(seed);
    CHECKP((g_ai.ais[0].feat2 & F2_PHANTOM) != 0, "F2_PHANTOM is off by default");
    bool bought = false;
    for (int t = 0; t < 12 * 60 * SIM_HZ && !g_sim.gameOver && !bought; t++) {
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        if (t % SIM_HZ) continue;
        for (int p = 0; p < 2; p++) if (g_sim.phantomOf(p).valid() || g_sim.phantomInProduction(p)) { bought = true; break; }
    }
    CHECKP(bought, "a rich computer army with F2_PHANTOM never bought a Phantom Jeep in 12 minutes");
    return true;
}

bool phantomTest(u64 seed) {
    struct { const char* name; bool (*fn)(u64); } cases[] = {
        { "stealth", stealthTest }, { "sniper", sniperTest }, { "nuke", nukeTest }, { "damage", damageTest },
        { "regen", regenTest }, { "tnt", tntTest }, { "climb", climbTest }, { "one at a time", oneTest }, { "ai", aiTest } };
    for (auto& cs : cases) {
        g_okP = true;
        const bool ok = cs.fn(seed) && g_okP;
        fprintf(stderr, "phantomtest: %-14s %s\n", cs.name, ok ? "PASS" : "FAIL");
        if (!ok) return false;
    }
    fprintf(stderr, "phantomtest: all passed\n");
    return true;
}
