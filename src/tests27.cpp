// One Hour - headless tests for the kill zones and the ambush (--traptest): a Hard commander (Cyber) against a still army of the other kind on the real map
//  * lanes: a ground route from the enemy base to home is found, at least two guns stand 6 tiles or less behind the heaviest gate, walls stand near it,
//    and a ground route from the enemy base to ours still exists afterwards
//  * wings: at least four fighters hide in two wings, every one at least four tiles off the lane's axis
//  * spring: eight enemy tanks walk in and both wings are ordered onto them within 20 seconds, and the enemy loses tanks
//  * lure (Hard): six enemy tanks stand idle out on the lane, the bait goes out and comes back to the home side of the gate
//  * off: with F2_FORTIFY and F2_TRAP masked out none of that happens and the old turrets still go up
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

static bool g_okT = true;
static bool failT(const char* m) { fprintf(stderr, "traptest: %s\n", m); g_okT = false; return false; }
#define CHECKT(c, m) do { if (!(c)) return failT(m); } while (0)

// The scene: player 0 a Hard computer commander with a full treasury, player 1 a still army (nobody gives it orders) across the map. The commander is
// held at home (no wave leaves), so what it builds and how its idle army stands can be read.
static void sceneT(u64 seed, int mask2) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { true, false };
    int diff[2] = { 2, 2 }, team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
    g_sim.players[0].money = 400000;
    for (int p = 0; p < 2; p++) { g_ai.brainEnabled[p] = false; g_ai.proEnabled[p] = true; g_ai.smartEnabled[p] = true; g_ai.featMask[p] = FEAT_DEFAULT; g_ai.featMask2[p] = mask2; g_ai.brainOverride[p] = nullptr; }
    g_ai.init(seed);
    g_ai.ais[0].regroupUntil = 1e9f;   // (no wave: the army stays home)
}

// The computer razes the still army's base within about six minutes (its air calls and nukes), and a match that is over freezes every unit: the
// still army is given three fresh structures and the match goes on, so the walk-in and the lure can be played out
static void keepMatchOn() {
    if (!g_sim.gameOver) return;
    static const Vec2 away[3] = { Vec2(-34.0f, 0.0f), Vec2(34.0f, 0.0f), Vec2(0.0f, 34.0f) };
    for (const Vec2& d : away) {
        const int cx = tileOf(g_sim.players[1].basePos.x) + (int)d.x, cy = tileOf(g_sim.players[1].basePos.y) + (int)d.y;
        for (int r = 0, done = 0; r < 8 && !done; r++) for (int dy = -r; dy <= r && !done; dy++) for (int dx = -r; dx <= r && !done; dx++)
            if (g_sim.canPlace(1, B_K_POWER, cx + dx, cy + dy)) { g_sim.placeBuilding(B_K_POWER, 1, cx + dx, cy + dy, true); done = 1; }
    }
    g_sim.players[1].alive = true; g_sim.gameOver = false;
}

static void runT(float sec) {
    const int n = (int)(sec * SIM_HZ);
    for (int t = 0; t < n; t++) {
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        keepMatchOn();
        if (t % (4 * SIM_HZ) == 0) g_sim.players[0].money = std::max(g_sim.players[0].money, 150000);
    }
}

// The commander's phone strikes and bomber runs raze a column of tanks within seconds of seeing it. For the scenes that walk an enemy group in, its
// aircraft are taken out and its calls held back for two minutes, so what the wings, the guns and the bait do can be read
static void quietSky() {
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) g_sim.destroy(e, false);
    g_sim.players[0].dropReady = g_sim.time + 120.0f;
}

static float tilesOff(Vec2 p, const AiPlayer::Lane& L) { return std::fabs((p.x - L.gate.x) * L.perp.x + (p.y - L.gate.y) * L.perp.y) / TILE; }
static bool attacking(const Entity* e) { return e && (e->order == O_ATTACKMOVE || e->order == O_ATTACK); }

// The point on the ground route from the enemy base to home that lies 'tiles' tiles out from the gate (along the route)
static Vec2 laneAt(const AiPlayer::Lane& L, float tiles) {
    std::vector<Vec2> route, dense;
    const Vec2 foe = g_map.nearestFree(g_sim.players[1].basePos, 8), mine = g_map.nearestFree(g_sim.players[0].basePos, 8);
    if (!g_map.findPath(foe, mine, route)) return L.gate;
    route.insert(route.begin(), foe);
    for (size_t i = 0; i + 1 < route.size(); i++) {
        const int n = std::max(1, (int)(dist(route[i], route[i + 1]) / TILE));
        for (int k = 0; k < n; k++) dense.push_back(route[i] + (route[i + 1] - route[i]) * (k / (float)n));
    }
    size_t gi = 0; float bd = 1e18f;
    for (size_t i = 0; i < dense.size(); i++) { const float d = dist(dense[i], L.gate); if (d < bd) { bd = d; gi = i; } }
    return dense[gi > (size_t)tiles ? gi - (size_t)tiles : 0];
}

static bool fortAndTrapTest(u64 seed) {
    sceneT(seed, F2_DEFAULT);
    AiPlayer& A = g_ai.ais[0];
    CHECKT((A.feat2 & F2_FORTIFY) && (A.feat2 & F2_TRAP), "F2_FORTIFY and F2_TRAP are not on by default");
    runT(8 * 60);
    CHECKT(!A.lanes.empty(), "no lane found: no ground route from the enemy base to home");
    const AiPlayer::Lane L = A.lanes[0];
    const Vec2 home = g_sim.players[0].basePos;
    fprintf(stderr, "traptest: %d lane(s); the heaviest: gate %d,%d (%.0f tiles from home), %s, width %.0f; %d guns, %d walls, %d Bastion Walls ordered for kill zones\n", (int)A.lanes.size(), tileOf(L.gate.x), tileOf(L.gate.y), dist(L.gate, home) / TILE, L.choke ? "choke" : "open field", L.width, A.fortGuns, A.fortWalls, A.fortBastions);
    int guns = 0, walls = 0;
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isBuilding() || e.owner != 0) continue;
        const float d = dist(e.pos, L.gate) / TILE;
        if (e.bt().role == BR_TURRET && d <= 6.0f && (e.pos.x - L.gate.x) * L.dir.x + (e.pos.y - L.gate.y) * L.dir.y < 0) guns++;
        if ((e.bt().role == BR_WALL || e.bt().role == BR_FWALL) && d <= 10.0f) walls++;
    }
    fprintf(stderr, "traptest: %d guns within 6 tiles behind the gate, %d wall segments within 10 tiles of it\n", guns, walls);
    CHECKT(guns >= 2, "fewer than two guns within 6 tiles behind the heaviest gate");
    CHECKT(walls >= 1, "no wall segment near the gate");
    std::vector<Vec2> route;
    const Vec2 foe = g_map.nearestFree(g_sim.players[1].basePos, 8), mine = g_map.nearestFree(home, 8);
    CHECKT(g_map.sameGround(foe, mine) && g_map.findPath(foe, mine, route) && !route.empty(), "the kill zone sealed the ground route from the enemy base to home");
    CHECKT(dist(route.back(), mine) < 6.0f * TILE, "the route from the enemy base ends far from home: it was sealed");

    // the wings
    CHECKT(A.wingsPosted >= 4, "fewer than four fighters in the ambush wings");
    int off = 0, n = 0;
    for (auto& w : A.wing) for (Ref r : w) if (const Entity* e = g_sim.get(r)) { n++; if (tilesOff(e->pos, L) >= 4.0f) off++; }
    fprintf(stderr, "traptest: %d fighters in the wings (%d + %d), %d of them at least 4 tiles off the axis\n", n, (int)A.wing[0].size(), (int)A.wing[1].size(), off);
    CHECKT(n >= 4 && off == n, "a fighter of the wings stands within 4 tiles of the lane's axis");
    CHECKT(!A.wing[0].empty() && !A.wing[1].empty(), "the ambush has only one wing");

    // the spring: eight tanks walk into the zone between the wings
    const int springs0 = A.springs;
    std::vector<Ref> tanks;
    const Vec2 from = laneAt(L, 11.0f);   // (the middle of the zone between the wings)
    quietSky();
    // (the home army, the gunships and the nukes of a Hard commander with a full treasury level a column of tanks within seconds: these are armoured so
    // the column is still walking in when the wings spring)
    for (int i = 0; i < 8; i++) { tanks.push_back(g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(from + Vec2((i % 4) * 2.0f * TILE, (i / 4) * 2.0f * TILE), 6))); Entity* e = g_sim.get(tanks.back()); e->hp = e->maxHp = 1.0e6f; }
    g_sim.cmdMove(tanks, home, true);
    bool wingOrdered[2] = { false, false };
    for (int t = 0; t < 20 * 2; t++) {
        runT(0.5f);
        for (int s = 0; s < 2; s++) for (Ref r : A.wing[s]) if (attacking(g_sim.get(r))) wingOrdered[s] = true;
    }
    int lost = 0; float hurt = 0; for (Ref r : tanks) { if (const Entity* e = g_sim.get(r)) hurt += e->maxHp - e->hp; else lost++; }
    fprintf(stderr, "traptest: spring: %d spring(s), wing orders %d/%d, %d of 8 enemy tanks lost, %.0f hit points taken off the rest\n", A.springs - springs0, (int)wingOrdered[0], (int)wingOrdered[1], lost, hurt);
    CHECKT(A.springs > springs0, "the trap never sprang on eight tanks in the zone between the wings");
    CHECKT(wingOrdered[0] && wingOrdered[1], "both wings were not ordered onto the enemy within 20 seconds");
    CHECKT(lost >= 1 || hurt > 0.0f, "the enemy lost no hit points");
    return true;
}

static bool lureTest(u64 seed) {
    sceneT(seed, F2_DEFAULT);
    AiPlayer& A = g_ai.ais[0];
    runT(8 * 60);
    CHECKT(!A.lanes.empty() && A.wingsPosted >= 4, "no lane or no wings to lure from");
    const AiPlayer::Lane L = A.lanes[0];
    const Vec2 home = g_sim.players[0].basePos;
    // the enemy group stands 12 to 26 tiles beyond the gate (counted from home), within 10 tiles of the lane and outside the zone the wings spring on
    const float g = dist(L.gate, home) / TILE;
    const Vec2 at = laneAt(L, 20.0f);
    const float away = dist(at, home) / TILE - g, side = tilesOff(at, L);
    const float f0 = ((A.wingPost[0] - L.gate).x * L.dir.x + (A.wingPost[0] - L.gate).y * L.dir.y) / TILE, f1 = ((A.wingPost[1] - L.gate).x * L.dir.x + (A.wingPost[1] - L.gate).y * L.dir.y) / TILE;
    const float along = ((at - L.gate).x * L.dir.x + (at - L.gate).y * L.dir.y) / TILE;
    const float pastZone = std::max(std::max(0.0f, along - std::max(f0, f1)), std::min(f0, f1) - along);
    const float fromZone = std::sqrt(pastZone * pastZone + side * side);
    fprintf(stderr, "traptest: the gate is %.0f tiles from home, the group stands %.1f tiles beyond it, %.1f tiles off the lane line and %.1f tiles from the wings' zone\n", g, away, side, fromZone);
    CHECKT(away >= 12.0f && away <= 26.0f && side <= 10.0f && fromZone > 7.5f, "the lure window (12 to 26 tiles beyond the gate, near the lane, outside the zone) misses the route on this map: nothing to test");
    while (A.sprung) runT(1.0f);
    A.nextLure = 0;
    const int lures0 = A.lures;
    quietSky();
    std::vector<Ref> group;
    for (int i = 0; i < 6; i++) group.push_back(g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(at + Vec2((i % 3) * 2.0f * TILE, (i / 3) * 2.0f * TILE), 6)));
    float farthest = 0; bool back = false;
    for (int t = 0; t < 40 * 2 && !back; t++) {
        runT(0.5f);
        for (Ref r : A.bait) if (const Entity* e = g_sim.get(r)) farthest = std::max(farthest, dist(e->pos, L.gate) / TILE);
        if (A.lures > lures0 && A.bait.empty()) back = true;
    }
    fprintf(stderr, "traptest: lure: %d lure(s), the bait went %.1f tiles from the gate, %s\n", A.lures - lures0, farthest, back ? "and is back" : "and is not back");
    CHECKT(A.lures > lures0, "no bait went out to a group of six enemy tanks out on the lane");
    CHECKT(farthest >= 5.0f, "the bait never went out past the gate");
    CHECKT(back, "the bait did not come back to the home side of the gate");
    return true;
}

static bool offTest(u64 seed) {
    sceneT(seed, F2_DEFAULT & ~(F2_FORTIFY | F2_TRAP));
    AiPlayer& A = g_ai.ais[0];
    runT(8 * 60);
    int guns = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && (e.bt().role == BR_TURRET || e.bt().role == BR_AATURRET)) guns++;
    fprintf(stderr, "traptest: off: %d lane(s), %d wing fighters, %d springs, %d lures, %d kill-zone guns, %d guns in all\n", (int)A.lanes.size(), A.wingsPosted, A.springs, A.lures, A.fortGuns, guns);
    CHECKT(A.lanes.empty() && A.wingsPosted == 0 && A.springs == 0 && A.lures == 0 && A.fortGuns == 0 && A.fortWalls == 0, "a kill zone or an ambush was made with F2_FORTIFY and F2_TRAP masked out");
    CHECKT(guns >= 1, "no turret at all with the kill zones masked out: the old placement did not run");
    return true;
}

bool trapTest(u64 seed) {
    struct { const char* name; bool (*fn)(u64); } cases[] = { { "lanes, wings, spring", fortAndTrapTest }, { "lure", lureTest }, { "off", offTest } };
    for (auto& cs : cases) {
        g_okT = true;
        const bool ok = cs.fn(seed) && g_okT;
        fprintf(stderr, "traptest: %-22s %s\n", cs.name, ok ? "PASS" : "FAIL");
        if (!ok) return false;
    }
    fprintf(stderr, "traptest: all passed\n");
    return true;
}
