// One Hour - the commander's kill zones (F2_FORTIFY, Normal and up).
//  * every 45 seconds the ground routes from the enemy bases to home are walked (PM_WALK): where a route first lies outside the ring round the base
//    (past the farthest production structure) is the lane's gate, and the route's heading there says which way the enemy comes
//  * the defences for a lane stand about its gate: guns behind it whose fire crosses the gap, two funnel arms of walls that squeeze the enemy into the
//    gap (or, where the ground is a choke, one partial wall across it with a gap on the side of the guns), anti-air behind the guns, a Bastion Wall
//    at the end of each arm for the sensors, and the flank bunkers on the heaviest lane's sides
//  * the commander's totals (guns, walls, bunkers) are unchanged: the kill zones are filled first and only what is left goes to the old spiral
#include "ai.h"

namespace {

bool aiDebugF() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

const float LANE_EVERY = 45.0f;     // seconds between plans
const float LANE_MERGE = 7.0f;      // tiles: gates this close are one lane
const float CHOKE_WIDTH = 6.0f;     // tiles: a lane this narrow or narrower is a choke
const float FAIL_HOLD = 60.0f;      // seconds a spot that took nothing is left alone

// The route as a polyline from our end outward, a point about every tile
void denseRouteFromHome(Vec2 from, const std::vector<Vec2>& path, std::vector<Vec2>& out) {
    std::vector<Vec2> route;
    route.push_back(from);
    route.insert(route.end(), path.begin(), path.end());
    out.clear();
    for (size_t i = route.size(); i-- > 1;) {
        const Vec2 a = route[i], b = route[i - 1];
        const int n = std::max(1, (int)(dist(a, b) / TILE));
        for (int k = 0; k < n; k++) out.push_back(a + (b - a) * (k / (float)n));
    }
    out.push_back(route[0]);
}

// Tiles of open ground (terrain only: structures do not count) each way from c along perp, up to cap
void openRun(Vec2 c, Vec2 perp, int cap, int& lo, int& hi) {
    auto run = [&](float s) {
        int n = 0;
        for (int k = 1; k <= cap; k++) {
            const Vec2 q = c + perp * (s * k * TILE);
            if (!g_map.terrainPassable(clampi(tileOf(q.x), 0, MAP_W - 1), clampi(tileOf(q.y), 0, MAP_H - 1))) break;
            n++;
        }
        return n;
    };
    hi = run(1.0f); lo = run(-1.0f);
}

// The centre of the tile nearest to p
Vec2 tileSnap(Vec2 p) { return tileCenter(clampi(tileOf(p.x), 1, MAP_W - 2), clampi(tileOf(p.y), 1, MAP_H - 2)); }

}  // namespace

void AiPlayer::planLanes() {
    Sim& S = g_sim;
    Player& pl = S.players[player];
    nextLanes = S.time + LANE_EVERY;
    lanesHome = pl.basePos;
    // the ring: past the farthest production structure of the home base, and four tiles more
    float farthest = 0;
    for (auto& e : S.ents) {
        if (!e.alive || e.owner != player || !e.isBuilding() || e.district >= 0 || e.bt().relic) continue;
        const BuildRole r = e.bt().role;
        if (!(r == BR_HQ || r == BR_BARRACKS || r == BR_FACTORY || r == BR_SUPPLY || r == BR_AIRFIELD)) continue;
        if (on2(F2_EXPAND) && xSlotOf(e.pos) >= 0) continue;   // (an expansion is not the home)
        farthest = std::max(farthest, dist(e.pos, pl.basePos));
    }
    const float ring = clampf(farthest / TILE + 4.0f, 9.0f, 18.0f);
    // how strong each enemy looks from here (what is in sight of its combat units), the team's focus counting half again
    float strength[MAX_PLAYERS];
    for (int q = 0; q < MAX_PLAYERS; q++) strength[q] = 1200.0f;
    for (auto& e : S.ents) {
        if (!e.alive || !e.isUnit() || e.owner < 0 || e.owner >= MAX_PLAYERS || !S.enemies(player, e.owner) || e.ut().role != UR_COMBAT) continue;
        if (!(S.explored(player, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) && S.visibleTo(e, player))) continue;
        strength[e.owner] += UNITS[e.type].cost * (0.5f + 0.5f * e.hp / e.maxHp);
    }
    const int focus = on(FEAT_TEAM) ? teamFocus() : -1;
    const Vec2 home = g_map.nearestFree(pl.basePos, 8);
    std::vector<Lane> found;
    std::vector<Vec2> path, route;
    for (int q = 0; q < S.numPlayers; q++) {
        if (q == player || !S.players[q].active || !S.players[q].alive || !S.enemies(player, q)) continue;
        const Vec2 from = g_map.nearestFree(S.players[q].basePos, 8);
        if (!g_map.sameGround(from, home) || !g_map.findPath(from, home, path) || path.empty()) continue;   // (a sea or an island between: nothing walks here)
        denseRouteFromHome(from, path, route);
        size_t gi = route.size();
        for (size_t i = 0; i < route.size(); i++) if (dist(route[i], pl.basePos) >= ring * TILE) { gi = i; break; }
        if (gi >= route.size()) continue;
        const Vec2 gate = route[gi], ahead = route[std::min(gi + 8, route.size() - 1)];
        if (dist(gate, ahead) < 2.0f * TILE) continue;
        Lane L;
        L.enemy = q; L.gate = gate; L.dir = (ahead - gate).norm(); L.perp = Vec2(-L.dir.y, L.dir.x);
        L.weight = strength[q] * (q == focus ? 1.5f : 1.0f) + scarNear(gate, 10.0f);
        found.push_back(L);
    }
    // a lane already held keeps its gate and heading (the route shifts as walls go up round it); a new one measures how open the ground is across it
    for (Lane& L : found) {
        const Lane* old = nullptr;
        for (const Lane& o : lanes) if (dist(o.gate, L.gate) < LANE_MERGE * TILE) { old = &o; break; }
        if (old) { L.gate = old->gate; L.dir = old->dir; L.perp = old->perp; L.width = old->width; L.choke = old->choke; continue; }
        int lo, hi; openRun(L.gate, L.perp, 8, lo, hi);
        L.width = (float)(lo + hi + 1);
        L.choke = L.width <= CHOKE_WIDTH;
    }
    std::sort(found.begin(), found.end(), [](const Lane& a, const Lane& b) { return a.weight > b.weight; });
    std::vector<Lane> kept;
    for (const Lane& L : found) {
        bool merged = false;
        for (Lane& K : kept) if (dist(K.gate, L.gate) < LANE_MERGE * TILE) { K.weight += L.weight; merged = true; break; }   // (one gate for both: the heavier lane's)
        if (!merged && kept.size() < 3) kept.push_back(L);
    }
    std::sort(kept.begin(), kept.end(), [](const Lane& a, const Lane& b) { return a.weight > b.weight; });
    if (aiDebugF() && (kept.size() != lanes.size() || (!kept.empty() && !lanes.empty() && dist(kept[0].gate, lanes[0].gate) > TILE)))
        for (size_t i = 0; i < kept.size(); i++)
            fprintf(stderr, "[ai%d t=%.0f] LANE %d from player %d: gate %d,%d dir %.2f,%.2f weight %.0f %s (width %.0f), ring %.0f tiles\n", player, S.time, (int)i, kept[i].enemy,
                    tileOf(kept[i].gate.x), tileOf(kept[i].gate.y), kept[i].dir.x, kept[i].dir.y, kept[i].weight, kept[i].choke ? "choke" : "open field", kept[i].width, ring);
    lanes.swap(kept);
    lanesFound = (int)lanes.size();
}

bool AiPlayer::slotHeld(BuildRole role, Vec2 at, float tolTiles) const {
    bool held = false;
    g_sim.forEachNear(at, tolTiles * TILE, [&](Entity& e) {
        if (!held && e.isBuilding() && e.owner == player && e.bt().role == role) held = true;
    });
    return held;
}

// The structure on the tile nearest to 'want' (within 'tol' tiles) that the commander's usual rules allow: the footprint can be placed, a one-tile
// corridor stays round it (walls excepted), the dozer can drive there, no enemy bunker is near and a wall never plugs a route (the strict test)
int AiPlayer::fortTry(int buildType, Vec2 want, int tol) {
    Sim& S = g_sim;
    Entity* d = idleDozer();
    if (!d || !S.buildAvailable(player, buildType) || !S.canAfford(player, BUILDS[buildType].cost)) return 0;
    const BuildType& b = BUILDS[buildType];
    const int x0 = (int)std::floor((want.x - b.w * TILE * 0.5f) / TILE + 0.5f), y0 = (int)std::floor((want.y - b.h * TILE * 0.5f) / TILE + 0.5f);
    std::vector<Vec2> enemyBunkers;
    for (auto& o : S.ents) if (o.alive && o.isBuilding() && o.owner != player && o.bt().role == BR_BUNKER && dist(o.pos, want) < 24.0f * TILE) enemyBunkers.push_back(o.pos);
    for (int r = 0; r <= tol; r++) {
        float bestD = 1e18f; int bx = -1, by = -1;
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            const int x = x0 + dx, y = y0 + dy;
            const Vec2 c = S.buildingCenter(buildType, x, y);
            const float dd = dist(c, want);
            if (dd >= bestD || x < 2 || y < 2 || x + b.w > MAP_W - 2 || y + b.h > MAP_H - 2) continue;
            if (!S.canPlace(player, buildType, x, y)) continue;
            if (!wallRole(b.role)) {   // (a one-tile corridor round every structure but a wall)
                bool gapOk = true;
                for (int j = y - 1; j <= y + b.h && gapOk; j++) for (int i = x - 1; i <= x + b.w; i++) if (g_map.blocked[j * MAP_W + i] & 2) { gapOk = false; break; }
                if (!gapOk) continue;
            }
            bool nearBunker = false;
            for (const Vec2& o : enemyBunkers) if (dist(c, o) < 16.0f * TILE) { nearBunker = true; break; }
            if (nearBunker || !g_map.sameGround(d->pos, c)) continue;
            if (wallRole(b.role) && g_map.wouldPlug(x, y, b.w, b.h, true)) continue;
            bestD = dd; bx = x; by = y;
        }
        if (bx >= 0) return S.cmdBuild(S.refOf(*d), buildType, bx, by) ? 1 : 2;
    }
    return 2;
}

namespace {

struct Slot { BuildRole role; Vec2 at; int tol; };

bool failedRecently(const std::vector<AiPlayer::FortFail>& fails, Vec2 at, BuildRole role, float now) {
    const int tx = tileOf(at.x), ty = tileOf(at.y);
    for (const auto& f : fails) if (f.tx == tx && f.ty == ty && f.kind == (int)role && f.until > now) return true;
    return false;
}

}  // namespace

// One missing gun of the lanes' kill zones, the heaviest lane first. The ground guns stand 3 to 4 tiles behind the gate, 3 tiles to either side of the
// axis, so their fire crosses the gap the enemy is squeezed into; the heaviest lane gets more of them on Hard and Brutal. The anti-air goes 5 to 6 tiles
// behind: after the first two ground guns once the enemy flies, after all the ground guns on Hard and up.
bool AiPlayer::fortGunsOps(bool airSeen) {
    Sim& S = g_sim;
    const Player& pl = S.players[player];
    const int base = firstBuildOf(pl.faction);
    for (size_t li = 0; li < lanes.size(); li++) {
        const Lane& L = lanes[li];
        const int nGround = li == 0 ? (pl.difficulty >= 3 ? 4 : (pl.difficulty == 2 ? 3 : 2)) : 2;
        static const float back[4] = { 3.5f, 3.5f, 4.5f, 4.5f }, side[4] = { 3.0f, -3.0f, 5.5f, -5.5f };
        std::vector<Slot> slots;
        const Slot aa = { BR_AATURRET, L.gate - L.dir * (5.5f * TILE) + L.perp * ((li & 1 ? -4.0f : 4.0f) * TILE), 2 };
        for (int k = 0; k < nGround; k++) {
            slots.push_back({ BR_TURRET, L.gate - L.dir * (back[k] * TILE) + L.perp * (side[k] * TILE), 2 });
            if (k == 1 && airSeen) slots.push_back(aa);
        }
        if (!airSeen && pl.difficulty >= 2) slots.push_back(aa);
        for (const Slot& s : slots) {
            if (slotHeld(s.role, s.at, s.tol + 0.5f) || failedRecently(fortFails, s.at, s.role, S.time)) continue;
            const int r = fortTry(base + s.role, s.at, s.tol);
            if (r == 0) return false;   // (no dozer or no money: the next slot would find the same)
            if (r == 1) { fortGuns++; if (aiDebugF()) fprintf(stderr, "[ai%d t=%.0f] KILL ZONE %zu: %s ordered at %d,%d\n", player, S.time, li, s.role == BR_AATURRET ? "anti-air" : "gun", tileOf(s.at.x), tileOf(s.at.y)); return true; }
            fortFails.push_back({ tileOf(s.at.x), tileOf(s.at.y), (int)s.role, S.time + FAIL_HOLD });
        }
    }
    return false;
}

// One missing wall of the kill zones. Open field: two arms starting 2 tiles either side of the axis at the gate and running out at 45 degrees toward the
// enemy (3 segments each, 4 on Hard, 5 on Brutal), so the enemy is squeezed toward the gap between the arms; a Bastion Wall ends each arm when the
// treasury can spare it. A choke: one wall across it a tile beyond the gate with a two-tile gap on the side of the guns. A segment that cannot go up
// (the strict plug test, no room) is skipped, never moved into the gap.
bool AiPlayer::fortWallsOps() {
    Sim& S = g_sim;
    const Player& pl = S.players[player];
    const int wallType = pl.faction == F_CYBER ? B_C_WALL : B_K_WALL, bastionType = pl.faction == F_CYBER ? B_C_FWALL : B_K_FWALL;
    const int arm = pl.difficulty >= 3 ? 5 : (pl.difficulty == 2 ? 4 : 3);
    for (size_t li = 0; li < lanes.size(); li++) {
        const Lane& L = lanes[li];
        std::vector<Slot> slots;
        if (!L.choke) {
            for (int k = 0; k < arm; k++) for (int s = -1; s <= 1; s += 2) slots.push_back({ BR_WALL, tileSnap(L.gate + L.perp * (s * (2.0f + k) * TILE) + L.dir * (k * (float)TILE)), 0 });
            if (S.buildAvailable(player, bastionType) && pl.money > BUILDS[bastionType].cost + 2500)
                for (int s = -1; s <= 1; s += 2) slots.push_back({ BR_FWALL, tileSnap(L.gate + L.perp * (s * (2.0f + arm) * TILE) + L.dir * (arm * (float)TILE)), 0 });
        } else {
            const Vec2 gunPlus = L.gate - L.dir * (3.5f * TILE) + L.perp * (3.0f * TILE), gunMinus = L.gate - L.dir * (3.5f * TILE) - L.perp * (3.0f * TILE);
            const float gapSide = (!slotHeld(BR_TURRET, gunPlus, 2.5f) && slotHeld(BR_TURRET, gunMinus, 2.5f)) ? -1.0f : 1.0f;
            const Vec2 line = L.gate + L.dir * (float)TILE;
            int lo, hi; openRun(line, L.perp, 8, lo, hi);
            const int edge = gapSide > 0 ? hi : lo;
            for (int t = -lo; t <= hi; t++) {   // (from the far side toward the gap)
                if (gapSide * t > edge - 2) continue;   // the two tiles at the edge nearest the guns stay open
                slots.push_back({ BR_WALL, tileSnap(line + L.perp * (t * (float)TILE)), 0 });
            }
            if (gapSide < 0) std::reverse(slots.begin(), slots.end());
        }
        for (const Slot& s : slots) {
            if (slotHeld(s.role, s.at, 0.6f) || failedRecently(fortFails, s.at, s.role, S.time)) continue;
            const int r = fortTry(s.role == BR_FWALL ? bastionType : wallType, s.at, s.tol);
            if (r == 0) return false;
            if (r == 1) {
                if (s.role == BR_FWALL) fortBastions++; else fortWalls++;
                if (aiDebugF()) fprintf(stderr, "[ai%d t=%.0f] KILL ZONE %zu: %s ordered at %d,%d\n", player, S.time, li, s.role == BR_FWALL ? "Bastion Wall" : "wall", tileOf(s.at.x), tileOf(s.at.y));
                return true;
            }
            fortFails.push_back({ tileOf(s.at.x), tileOf(s.at.y), (int)s.role, S.time + FAIL_HOLD });
        }
    }
    return false;
}

// The flank bunkers' anchors: 6 to 9 tiles to either side of the heaviest lane's axis, level with the gate or two tiles beyond it (open field), or
// level with the choke's exit on the home side
void AiPlayer::laneAnchors(std::vector<Vec2>& out) const {
    if (lanes.empty() || dist(lanesHome, g_sim.players[player].basePos) > 3.0f * TILE) return;
    const Lane& L = lanes[0];
    for (int s = -1; s <= 1; s += 2) {
        const Vec2 side = L.perp * (s * 7.5f * TILE);
        if (L.choke) out.push_back(L.gate - L.dir * (2.0f * TILE) + side);
        else { out.push_back(L.gate + side); out.push_back(L.gate + L.dir * (2.0f * TILE) + side); }
    }
}
