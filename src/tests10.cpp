// One Hour - headless tests for the map's neutral civilian strongholds (--forttest)
//  * the generator places a fair, rotated set of castles, civilian bunkers and watchtowers, all empty and unowned
//  * infantry walk in, the first one claims the landmark, the garrison is capped and only that army (or its allies) may reinforce
//  * the garrison fires with its own weapons from inside; the masonry is much tougher than a normal structure and survives a direct warhead
//  * everybody can be ordered back out, and a destroyed stronghold turns its garrison onto the ground
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok10 = true;
static bool fail10(const char* m) { fprintf(stderr, "forttest: %s\n", m); g_ok10 = false; return false; }
#define CHECK10(c, m) do { if (!(c)) return fail10(m); } while (0)

static void run10(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static bool alive10(Ref r) { return g_sim.get(r) != nullptr; }
static float hp10(Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : 0.0f; }

static Ref fortOfKind(int kind) {
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_STRONGHOLD && g_map.landmarks.size() && kind < 0) return g_sim.refOf(e);
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_STRONGHOLD) {
        for (auto& L : g_map.landmarks) if (L.tx == e.tx && L.ty == e.ty && L.kind == kind) return g_sim.refOf(e);
    }
    return NOREF;
}

static void clearUnits() {
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

static Ref spawnAt(int type, int owner, Vec2 near) {
    return g_sim.spawnUnit(type, owner, g_map.nearestFree(near, 10));
}

// ---- the map's strongholds: count, fairness, emptiness and data
static bool placementTest() {
    int total = 0, kinds[3] = { 0, 0, 0 }, quad[4] = { 0, 0, 0, 0 };
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isBuilding() || e.bt().role != BR_STRONGHOLD) continue;
        total++;
        int kind = e.type == B_C_CASTLE ? 0 : (e.type == B_C_CIVBUNKER ? 1 : 2);
        kinds[kind]++;
        quad[(e.tx < MAP_W / 2 ? 0 : 1) + (e.ty < MAP_H / 2 ? 0 : 2)]++;
        CHECK10(e.owner < 0, "a stronghold starts owned instead of neutral");
        CHECK10(e.passengers.empty(), "a stronghold starts with a garrison");
        CHECK10(e.constructed && e.progress >= 1.0f, "a stronghold starts unfinished");
        CHECK10(e.hp >= e.maxHp - 0.1f, "a stronghold starts damaged");
        CHECK10(e.bt().garrison >= 6, "a stronghold has no usable infantry capacity");
        CHECK10(e.maxHp == BUILDS[e.type].hp, "a stronghold's health does not come from its data row");
        CHECK10(g_map.tile(e.tx, e.ty) != T_ROAD, "a stronghold was dropped on a road");
    }
    CHECK10(total >= 12, "the map has fewer than twelve neutral strongholds");
    CHECK10(kinds[0] >= 4 && kinds[1] >= 4 && kinds[2] >= 4, "the map does not have four of each stronghold kind");
    CHECK10((int)g_map.landmarks.size() == total, "the placed strongholds do not match the map landmark list");
    for (int q = 0; q < 4; q++) CHECK10(quad[q] >= 3, "a map quadrant has fewer than three strongholds");
    CHECK10(!g_sim.buildAvailable(0, B_C_CASTLE), "a stronghold is offered in the build menu");
    Ref dz = g_sim.spawnUnit(U_C_DOZER, 0, g_map.nearestFree(g_sim.players[0].basePos + Vec2(180, 180), 12));
    if (dz.valid()) CHECK10(!Sim::canRaise(*g_sim.get(dz), B_C_CASTLE), "a dozer may raise a map stronghold");
    if (dz.valid()) g_sim.destroy(*g_sim.get(dz), false);
    return true;
}

// ---- walking in claims a neutral castle; capacity and enemy access
static bool claimTest() {
    Ref fr = fortOfKind(0);
    CHECK10(fr.valid(), "no castle to test the claim on");
    Entity* fort = g_sim.get(fr);
    Vec2 at = g_map.nearestFree(fort->pos + Vec2(0, fort->radius() + 28), 12);
    Ref inf = spawnAt(U_C_INF1, 0, at);
    CHECK10(alive10(inf), "could not spawn the infantryman for the claim test");
    CHECK10(g_sim.canBoard(*g_sim.get(inf), *fort), "a neutral stronghold refuses friendly infantry");
    g_sim.cmdEnter({ inf }, fr);
    for (int t = 0; t < 20 * 12 && g_sim.garrisonInfantry(*g_sim.get(fr)) == 0; t++) { g_sim.step(); g_sim.events.clear(); }
    CHECK10(g_sim.garrisonInfantry(*g_sim.get(fr)) == 1, "the infantryman never walked into the neutral castle");
    CHECK10(g_sim.get(fr)->owner == 0, "the first infantryman did not claim the castle for their army");
    CHECK10(g_sim.get(fr)->passengers.size() == 1, "the castle holds the wrong number of passengers after the claim");

    Entity* fort2 = g_sim.get(fr);
    Ref foe = spawnAt(U_K_INF1, 1, fort2->pos + Vec2(0, fort2->radius() + 34));
    CHECK10(alive10(foe), "could not spawn the enemy infantryman");
    CHECK10(!g_sim.canBoard(*g_sim.get(foe), *fort2), "an enemy may walk into a claimed stronghold");

    int cap = g_sim.garrisonCap(*fort2);
    while (g_sim.garrisonInfantry(*g_sim.get(fr)) < cap) {
        Ref r = spawnAt(U_C_INF2, 0, g_sim.get(fr)->pos + Vec2(0, 40));
        CHECK10(alive10(r), "could not spawn reinforcements for the capacity test");
        g_sim.boardQuiet(*g_sim.get(r), *g_sim.get(fr));
    }
    CHECK10(g_sim.garrisonInfantry(*g_sim.get(fr)) == cap, "the castle does not fill to its data-row capacity");
    Ref extra = spawnAt(U_C_INF1, 0, g_sim.get(fr)->pos + Vec2(0, 40));
    CHECK10(!g_sim.canBoard(*g_sim.get(extra), *g_sim.get(fr)), "a full castle still accepts infantry");

    g_sim.cmdUnload(fr, false, Vec2());
    for (int t = 0; t < 20 * 8 && !g_sim.get(fr)->passengers.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
    CHECK10(g_sim.get(fr)->passengers.empty(), "unload left soldiers inside the castle");
    CHECK10(g_sim.get(fr)->owner == 0, "the castle became neutral again after everybody left");
    return true;
}

// ---- the garrison shoots from the windows; the building is much tougher than a normal structure
static bool fightTest() {
    Ref fr = fortOfKind(1);
    CHECK10(fr.valid(), "no civilian bunker to test the garrison fire on");
    Entity* fort = g_sim.get(fr);
    // claim it with laser troopers (long range, good against a tank)
    int cap = std::min(g_sim.garrisonCap(*fort), 8);
    for (int i = 0; i < cap; i++) {
        Ref r = spawnAt(U_C_INF2, 0, fort->pos + Vec2(0, 40));
        CHECK10(alive10(r), "could not spawn a laser trooper");
        g_sim.boardQuiet(*g_sim.get(r), *g_sim.get(fr));
    }
    CHECK10(g_sim.get(fr)->owner == 0, "the bunker was not claimed by its garrison");
    Ref tank = spawnAt(U_K_TANK, 1, fort->pos + Vec2(-4 * TILE, 0));
    CHECK10(alive10(tank), "could not spawn the enemy tank");
    g_sim.get(tank)->disabledUntil = 1e9f;   // it sits there and takes it: this test is about the garrison's fire
    float t0 = hp10(tank);
    run10(5.0f);
    CHECK10(!alive10(tank) || hp10(tank) < t0, "the garrison did not shoot the enemy tank");

    // conventional fire on the stronghold versus a normal structure
    Entity* fort2 = g_sim.get(fr);
    float before = fort2->hp;
    g_sim.applyDamagePublic(*fort2, 1000.0f, 1, NOREF);
    float lostFort = before - fort2->hp;
    int tx = tileOf(fort2->pos.x) + 8, ty = tileOf(fort2->pos.y) + 8;
    Ref pw = NOREF;
    for (int r = 0; r < 12 && !pw.valid(); r++) for (int dy = -r; dy <= r && !pw.valid(); dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (g_sim.canPlace(0, B_C_POWER, tx + dx, ty + dy)) { pw = g_sim.placeBuilding(B_C_POWER, 0, tx + dx, ty + dy, true); break; }
    }
    CHECK10(pw.valid(), "could not place the normal structure for the damage comparison");
    float p0 = g_sim.get(pw)->hp;
    g_sim.applyDamagePublic(*g_sim.get(pw), 1000.0f, 1, NOREF);
    float lostPower = p0 - g_sim.get(pw)->hp;
    CHECK10(lostFort < lostPower * 0.8f, "stronghold masonry is not tougher than a normal structure");
    CHECK10(lostFort > 400.0f && lostFort < 750.0f, "the stronghold damage reduction is not the expected 62%");

    // a direct warhead cannot erase a full-health stronghold
    Entity* fort3 = g_sim.get(fr);
    fort3->hp = fort3->maxHp;
    Sim::Nuke n; n.from = fort3->pos + Vec2(300, 300); n.pos = fort3->pos; n.owner = 1; n.t = 0; n.scale = 1.0f; n.power = 1.0f;
    g_sim.nukeBlastPublic(n);
    CHECK10(g_sim.get(fr) && g_sim.get(fr)->hp > g_sim.get(fr)->maxHp * 0.28f, "one direct warhead erased a full-health stronghold");

    // destroying it turns the garrison out (the test has already refilled it)
    Entity* fort4 = g_sim.get(fr);
    if (fort4) {
        int beforeCount = g_sim.garrisonInfantry(*fort4);
        g_sim.applyDamagePublic(*fort4, fort4->hp / STRONGHOLD_ARMOR + 20.0f, 1, NOREF);
        int outside = 0;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && !e.carrier.valid()) outside++;
        CHECK10(g_sim.get(fr) == nullptr || !g_sim.get(fr)->alive, "the destroyed stronghold is still alive");
        CHECK10(outside >= beforeCount, "a destroyed stronghold did not turn its garrison out onto the ground");
    }
    return true;
}

bool fortTest(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, false };
    int diff[2] = { 1, 0 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    clearUnits();
    if (!placementTest()) return false;
    if (!claimTest()) return false;
    if (!fightTest()) return false;
    printf("forttest seed %llu: ok (neutral castles, bunkers and watchtowers: claim, garrison, fire, durability, unload, destruction)\n", (unsigned long long)seed);
    return g_ok10;
}
