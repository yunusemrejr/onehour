// One Hour - the commander's ambush (F2_TRAP, Normal and up), laid on the heaviest lane of the kill zones (ai_fort.cpp).
//  * while no wave is out, up to 35% of the idle home army (at least four fighters) hides in two wings, in the cover (trees, rock) 6 to 9 tiles out along
//    the lane and 6 to 8 tiles to either side of it, off the route the enemy walks
//  * the spring: three armed enemy ground fighters (or the kill zone's guns and walls under fire) in the zone between the wings, and both wings attack-move onto
//    the enemy in the same think while the rest of the idle army attack-moves to the gate. Ten seconds with no enemy about and the wings go back to their posts
//  * the lure (Hard and Brutal): when a group of four or more armed enemy fighters stands 12 to 26 tiles beyond the gate, near the lane and outside the zone, two or three of the fastest idle
//    units attack-move at it as bait and, once within one and a half weapon ranges of an enemy (or after 8 seconds), run back through the gate
#include "ai.h"

namespace {

bool aiDebugT() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

const float WING_REFRESH = 30.0f;   // seconds between looks at the wings' size and posts
const float ZONE_REACH = 7.0f;      // tiles: how close to the lane between the wings three enemies must be to spring the trap
const float CALM_DIST = 12.0f;      // tiles: no enemy this near for CALM_FOR seconds and the wings go back to their posts
const float CALM_FOR = 10.0f;
const float LURE_COOLDOWN = 90.0f;
const float BAIT_TURN = 8.0f;       // seconds before the bait turns back whatever it has met

float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

float distToSegment(Vec2 p, Vec2 a, Vec2 b) {
    const Vec2 ab = b - a;
    const float l2 = ab.x * ab.x + ab.y * ab.y;
    const float t = l2 > 1e-3f ? clampf(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2, 0.0f, 1.0f) : 0.0f;
    return dist(p, a + ab * t);
}

// An enemy that can fight on the ground (a passing hauler or dozer does not spring a trap)
bool groundFoe(int player, const Entity& e) {
    return e.alive && e.isUnit() && e.owner >= 0 && e.owner < MAX_PLAYERS && g_sim.enemies(player, e.owner) && !e.isAir() && !e.ut().naval && e.weapon() >= 0;
}

}  // namespace

void AiPlayer::trapOps(const std::vector<Ref>& army, const std::vector<Ref>& idleArmy, const std::vector<Entity*>& enemyUnits, bool attacking) {
    Sim& S = g_sim;
    const Player& pl = S.players[player];
    auto release = [&]() { wing[0].clear(); wing[1].clear(); bait.clear(); baitBack.clear(); wingsPosted = 0; sprung = false; };
    if (attacking || lanes.empty() || dist(lanesHome, pl.basePos) > 3.0f * TILE) { if (wingsPosted > 0 || !bait.empty()) release(); return; }   // (a wave takes the wings with it)
    // whoever died, climbed aboard something or was turned is out of the wings and the bait
    auto gone = [&](Ref r) { const Entity* e = S.get(r); return !e || e->owner != player || e->carrier.valid() || e->order == O_ENTER; };
    for (auto& w : wing) w.erase(std::remove_if(w.begin(), w.end(), gone), w.end());
    bait.erase(std::remove_if(bait.begin(), bait.end(), gone), bait.end());
    baitBack.erase(std::remove_if(baitBack.begin(), baitBack.end(), gone), baitBack.end());
    const Lane& L = lanes[0];
    if (wingsPosted > 0 && dist(trapGate, L.gate) > 3.0f * TILE) { release(); }   // (another lane is the heaviest now)
    for (auto& w : wing) if (w.size() < 2) w.clear();   // (a wing below two fighters is dissolved)

    // ---- the posts: a walkable tile in cover, off the lane's axis
    auto pickPost = [&](float side) {
        const Vec2 raw = L.gate + L.dir * ((7.5f + 0.5f * side) * TILE) + L.perp * (side * 7.0f * TILE);
        const int rx = tileOf(raw.x), ry = tileOf(raw.y);
        Vec2 best = g_map.nearestFree(raw, 6); float bestScore = -1e18f;
        for (int y = ry - 4; y <= ry + 4; y++) for (int x = rx - 4; x <= rx + 4; x++) {
            if (!inMap(x, y) || !g_map.passable(x, y)) continue;
            const Vec2 c = tileCenter(x, y);
            const float fwd = dot(c - L.gate, L.dir) / TILE, lat = std::fabs(dot(c - L.gate, L.perp)) / TILE;
            if (lat < 5.5f || fwd < 4.0f || fwd > 11.0f || !g_map.sameGround(c, rally)) continue;
            int cover = 0;
            for (int j = y - 2; j <= y + 2; j++) for (int i = x - 2; i <= x + 2; i++) if (inMap(i, j) && (g_map.tile(i, j) == T_TREE || g_map.tile(i, j) == T_ROCK)) cover++;
            const float sc = cover * 3.0f - dist(c, raw) / TILE;
            if (sc > bestScore) { bestScore = sc; best = c; }
        }
        return best;
    };
    // fighters of the idle army that may be posted: ground combat units, armed
    auto fit = [&](Ref r) {
        const Entity* e = S.get(r);
        return e && e->ut().role == UR_COMBAT && e->ut().kind != UK_AIR && !e->ut().naval && e->weapon() >= 0 && e->hp > e->maxHp * 0.6f && !inWing(r)
               && std::find(bait.begin(), bait.end(), r) == bait.end() && g_map.sameGround(e->pos, rally);
    };
    std::vector<Ref> pool;
    for (Ref r : idleArmy) if (fit(r)) pool.push_back(r);
    const int posted = (int)(wing[0].size() + wing[1].size());
    const int target = (int)(0.35f * (float)(pool.size() + posted));
    if (target < 4 && posted == 0) { wingsPosted = 0; return; }   // (too small an army to spare four)
    if (S.time - wingsAt >= WING_REFRESH || posted == 0) {
        wingsAt = S.time;
        if (posted == 0 || dist(trapGate, L.gate) > 1.0f * TILE) { wingPost[0] = pickPost(1.0f); wingPost[1] = pickPost(-1.0f); trapGate = L.gate; }
        // the nearest to the gate are posted first
        std::sort(pool.begin(), pool.end(), [&](Ref a, Ref b) { return dist2(S.get(a)->pos, L.gate) < dist2(S.get(b)->pos, L.gate); });
        std::vector<Ref> add[2];
        for (size_t i = 0; i < pool.size() && (int)(posted + add[0].size() + add[1].size()) < target; i++) {
            const int s = wing[0].size() + add[0].size() <= wing[1].size() + add[1].size() ? 0 : 1;
            add[s].push_back(pool[i]);
        }
        for (int s = 0; s < 2; s++) {
            if (!add[s].empty()) { wing[s].insert(wing[s].end(), add[s].begin(), add[s].end()); groupMove(add[s], wingPost[s], false); }
            std::vector<Ref> drift;   // (a fighter that stands idle away from its post is sent there again)
            for (Ref r : wing[s]) { const Entity* e = S.get(r); if (e && !sprung && e->order == O_IDLE && dist(e->pos, wingPost[s]) > 5.0f * TILE && std::find(add[s].begin(), add[s].end(), r) == add[s].end()) drift.push_back(r); }
            if (!drift.empty()) groupMove(drift, wingPost[s], false);
        }
        if (aiDebugT() && (int)(wing[0].size() + wing[1].size()) != posted)
            fprintf(stderr, "[ai%d t=%.0f] AMBUSH wings %d + %d at %d,%d and %d,%d (gate %d,%d)\n", player, S.time, (int)wing[0].size(), (int)wing[1].size(), tileOf(wingPost[0].x), tileOf(wingPost[0].y), tileOf(wingPost[1].x), tileOf(wingPost[1].y), tileOf(L.gate.x), tileOf(L.gate.y));
    }
    wingsPosted = (int)(wing[0].size() + wing[1].size());
    if (wing[0].empty() || wing[1].empty()) return;

    // ---- the zone between the wings, and what stands in it
    const float f0 = dot(wingPost[0] - L.gate, L.dir), f1 = dot(wingPost[1] - L.gate, L.dir);
    const Vec2 segA = L.gate + L.dir * std::min(f0, f1), segB = L.gate + L.dir * std::max(f0, f1);
    std::vector<const Entity*> inZone;
    Vec2 zoneSum; bool foeNear = false;
    for (const Entity* u : enemyUnits) {
        if (!groundFoe(player, *u)) continue;
        const float d = distToSegment(u->pos, segA, segB);
        if (d < CALM_DIST * TILE) foeNear = true;
        if (d < ZONE_REACH * TILE) { inZone.push_back(u); zoneSum += u->pos; }
    }
    if (foeNear) lastFoeNear = S.time;
    // the kill zone's own guns and walls under fire count as an enemy at the gate
    bool struck = false; Vec2 strikeAt;
    S.forEachNear(L.gate, 12.0f * TILE, [&](Entity& e) {
        if (struck || !e.isBuilding() || e.owner != player || S.time - e.lastDamaged > 2.0f) return;
        const BuildRole r = e.bt().role;
        if (!(r == BR_TURRET || r == BR_AATURRET || r == BR_WALL || r == BR_FWALL)) return;
        if (const Entity* a = S.get(e.attacker)) if (groundFoe(player, *a)) { struck = true; strikeAt = a->pos; lastFoeNear = S.time; }
    });
    const bool trigger = inZone.size() >= 3 || struck;
    if (trigger) {
        const Vec2 at = inZone.size() >= 3 ? zoneSum * (1.0f / (float)inZone.size()) : strikeAt;
        if (!sprung || S.time - springAt >= 4.0f) {   // (the first order springs the trap; the next ones follow the enemy)
            if (!sprung) {
                springs++; sprung = true;
                if (aiDebugT()) fprintf(stderr, "[ai%d t=%.0f] SPRING: %d enemy in the zone%s, wings %d + %d onto %d,%d\n", player, S.time, (int)inZone.size(), struck ? " (the kill zone is hit)" : "", (int)wing[0].size(), (int)wing[1].size(), tileOf(at.x), tileOf(at.y));
            }
            springAt = S.time;
            for (auto& w : wing) {
                std::vector<Ref> go;
                for (Ref r : w) { const Entity* e = S.get(r); if (e && !e->engaged.valid()) go.push_back(r); }
                if (!go.empty()) groupMove(go, at, true);
            }
            std::vector<Ref> rest;   // the rest of the idle army: to the gate, to fight beside the guns
            for (Ref r : army) { const Entity* e = S.get(r); if (e && e->order == O_IDLE && !inWing(r) && e->ut().kind != UK_AIR && e->ut().role == UR_COMBAT && g_map.sameGround(e->pos, L.gate)) rest.push_back(r); }
            if (!rest.empty()) groupMove(rest, L.gate, true);
        }
    } else if (sprung && S.time - lastFoeNear >= CALM_FOR) {
        sprung = false;
        for (int s = 0; s < 2; s++) groupMove(wing[s], wingPost[s], false);
        if (aiDebugT()) fprintf(stderr, "[ai%d t=%.0f] the wings go back to their posts\n", player, S.time);
    }

    // ---- the lure (Hard and Brutal): a few fast units show themselves to a group out on the lane and run back through the gate
    if (pl.difficulty < 2) return;
    const Vec2 backTo = g_map.nearestFree(L.gate - L.dir * (4.0f * TILE), 6);
    if (!bait.empty()) {
        for (Ref r : bait) {
            if (std::find(baitBack.begin(), baitBack.end(), r) != baitBack.end()) continue;
            const Entity* e = S.get(r);
            float nearest = 1e18f;
            for (const Entity* u : enemyUnits) if (groundFoe(player, *u)) nearest = std::min(nearest, dist(u->pos, e->pos));
            const float reach = e->weapon() >= 0 ? WEAPONS[e->weapon()].range * TILE * 1.5f : 6.0f * TILE;
            if (nearest <= reach || S.time - lureAt >= BAIT_TURN) { S.cmdMove({ r }, backTo, false); baitBack.push_back(r); }
        }
        bool home = true;
        for (Ref r : baitBack) { const Entity* e = S.get(r); if (e && dist(e->pos, backTo) > 3.0f * TILE) home = false; }
        if ((baitBack.size() == bait.size() && home) || S.time - lureAt > 40.0f) { bait.clear(); baitBack.clear(); }
        return;
    }
    if (S.time < nextLure || sprung) return;
    // the biggest knot of armed enemy fighters standing 12 to 26 tiles beyond the gate (counted from home), within ten tiles of the lane and outside the
    // zone the wings spring on: a group that is not already walking in
    const float g = dist(L.gate, pl.basePos);
    const Vec2 laneEnd = L.gate + L.dir * (40.0f * TILE);
    std::vector<const Entity*> cand;
    for (const Entity* u : enemyUnits) {
        const float db = dist(u->pos, pl.basePos);
        if (groundFoe(player, *u) && db >= g + 12.0f * TILE && db <= g + 26.0f * TILE && distToSegment(u->pos, L.gate, laneEnd) <= 10.0f * TILE && distToSegment(u->pos, segA, segB) > ZONE_REACH * TILE) cand.push_back(u);
    }
    if (cand.size() < 4) return;
    size_t bestN = 0; Vec2 bestC;
    for (const Entity* a : cand) {
        size_t n = 0; Vec2 sum;
        for (const Entity* b : cand) if (dist(a->pos, b->pos) < 6.0f * TILE) { n++; sum += b->pos; }
        if (n > bestN) { bestN = n; bestC = sum * (1.0f / (float)n); }
    }
    if (bestN < 4) return;
    std::vector<std::pair<float, Ref>> fast;
    for (Ref r : idleArmy) if (fit(r)) fast.push_back({ -S.get(r)->ut().speed, r });
    if (fast.size() < 2) return;
    std::sort(fast.begin(), fast.end(), [](const std::pair<float, Ref>& a, const std::pair<float, Ref>& b) { return a.first < b.first; });
    const size_t nb = std::min<size_t>(fast.size(), bestN >= 8 ? 3 : 2);
    for (size_t i = 0; i < nb; i++) bait.push_back(fast[i].second);
    groupMove(bait, bestC, true);
    lureAt = S.time; nextLure = S.time + LURE_COOLDOWN; lures++;
    if (aiDebugT()) fprintf(stderr, "[ai%d t=%.0f] LURE: %d bait at the %d enemy near %d,%d\n", player, S.time, (int)bait.size(), (int)bestN, tileOf(bestC.x), tileOf(bestC.y));
}
