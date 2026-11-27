// One Hour - the commander and forest fires (F2_FIRE, Normal and up).
//  * firebreaks: a fire burning toward one of its structures (the wind blowing from the fire at it, the structure within 8 tiles) and the
//    commander drops Waymakers on the trees 2 to 6 tiles ahead of the flames, across the fire's path, so the blaze runs out of forest before
//    it arrives. Its units need no orders for this: everything steps out of the flames by itself (sim_fire.cpp)
//  * the torch: in a dry wind, a forest whose trees run unbroken downwind into an enemy's structures or infantry is a weapon. Two cheap
//    infantry from the idle army walk to its upwind edge and set it alight; the wind does the rest. Never upwind of its own base
#include "ai.h"

namespace {

bool aiDebugF() { static int v = -1; if (v < 0) v = getenv("ONEHOUR_DEBUG") ? 1 : 0; return v == 1; }

const float BREAK_LOOK = 4.0f;      // seconds between looks for a fire coming at the base
const float BREAK_REACH = 8.0f;     // tiles: a fire this near a structure worth more than $500, the wind blowing at it, is a threat
const float TORCH_LOOK = 25.0f;     // seconds between looks for a forest to set alight
const float TORCH_GIVEUP = 75.0f;   // seconds the torchers have to get there
const float TORCH_REACH = 70.0f;    // tiles from home: farther targets are a raid, not a torch job (infantry would not get there)
const int TORCH_TARGETS = 64;       // enemy things weighed per look (spread over the list)

float dot(Vec2 a, Vec2 b) { return a.x * b.x + a.y * b.y; }

bool treeAt(int x, int y) { return inMap(x, y) && g_map.tile(x, y) == T_TREE; }

}  // namespace

void AiPlayer::fireOps(const std::vector<Ref>& army, const std::vector<Entity*>& enemyUnits) {
    Sim& S = g_sim;
    const Player& pl = S.players[player];
    const Vec2 wind(std::cos(S.windAngle), std::sin(S.windAngle));

    // ---- the torch already out: whoever gets there first sets the tree alight
    if (torchX >= 0) {
        bool done = !treeAt(torchX, torchY) || S.fireAt(torchX, torchY) > 0 || S.time > torchUntil;
        for (Ref r : torchers) {
            Entity* u = S.get(r);
            if (!done && u && (u->order == O_IDLE || u->order == O_GUARDPOS) && dist2(u->pos, torchStand) > 2.0f * TILE * TILE)
                S.cmdMove({ r }, torchStand, false);   // (stopped short, or put on guard: on to the forest)
            if (!done && u && S.torchTree(*u, torchX, torchY)) { done = true; torches++; if (aiDebugF()) printf("[ai %d] t=%.0f TORCH (%d,%d)\n", player, S.time, torchX, torchY); }
        }
        if (done && aiDebugF()) {
            float best = 1e9f; int alive = 0, ord = -1;
            for (Ref r : torchers) if (Entity* u = S.get(r)) { alive++; float d = dist(u->pos, tileCenter(torchX, torchY)) / TILE; if (d < best) { best = d; ord = u->order; } }
            printf("[ai %d] t=%.0f torch end: alive %d, nearest %.1f tiles (order %d), tree %d, burning %d\n", player, S.time, alive, best, ord, (int)treeAt(torchX, torchY), (int)(S.fireAt(torchX, torchY) > 0));
        }
        if (done) {
            std::vector<Ref> back;
            for (Ref r : torchers) if (S.get(r)) back.push_back(r);
            if (!back.empty()) S.cmdMove(back, pl.basePos, false);   // (home, out of the smoke)
            torchers.clear(); torchX = torchY = -1;
        }
    }

    // ---- firebreaks against a fire coming at the base
    if (S.time >= breakWindowEnd) { breakWindowEnd = S.time + 60.0f; breakWindow = 0; }
    if (!S.fires.empty() && S.time >= nextBreak && breakWindow < 3) {   // (at most three Waymakers a minute: a firebreak, not a clearing)
        nextBreak = S.time + BREAK_LOOK;
        const int way = pl.faction == F_CYBER ? B_C_WAYMAKER : B_K_WAYMAKER;
        Entity* dz = idleDozer(false);
        if (dz && S.canAfford(player, BUILDS[way].cost * 2)) {
            // the nearest fire, among the hot ones, that the wind drives at a structure of ours
            const FireCell* best = nullptr; Vec2 aim; float bestD = 1e9f;
            static std::vector<Vec2> mine; mine.clear();
            for (auto& e : S.ents) if (e.alive && e.owner == player && e.isBuilding() && BUILDS[e.type].role != BR_WAYMAKER && BUILDS[e.type].cost > 500) mine.push_back(e.pos);
            for (size_t i = 0; i < S.fires.size(); i += 2) {   // (every other cell: a front is many cells wide)
                const FireCell& f = S.fires[i];
                if (f.heat < 0.3f) continue;
                const Vec2 fp = tileCenter(f.tx, f.ty);
                for (const Vec2& m : mine) {
                    const Vec2 to = m - fp; const float d = to.len();
                    if (d > BREAK_REACH * TILE || d < 1 || dot(to * (1.0f / d), wind) < 0.5f || d >= bestD) continue;
                    bestD = d; best = &f; aim = to * (1.0f / d);
                }
            }
            if (best) {
                // down the fire's path, the first spot where a Waymaker clears trees; then one each side of it across the path
                const Vec2 side(-aim.y, aim.x);
                int placed = 0;
                for (int k = 2; k <= 6 && !placed; k++) {
                    const Vec2 c = tileCenter(best->tx, best->ty) + aim * (k * TILE);
                    for (int s : { 0, -3, 3 }) {
                        if (breakWindow >= 3) break;
                        const Vec2 p = c + side * (s * TILE);
                        const int tx = tileOf(p.x) - 1, ty = tileOf(p.y) - 1;
                        if (!S.waymakerFits(player, tx, ty, 3, 3) || S.fireAt(tx + 1, ty + 1) > 0) continue;
                        if (!S.canAfford(player, BUILDS[way].cost) || !S.cmdBuild(S.refOf(*dz), way, tx, ty)) continue;
                        placed++; firebreaks++; breakWindow++;
                    }
                }
                if (placed && aiDebugF()) printf("[ai %d] t=%.0f FIREBREAK %d Waymakers ahead of the fire at (%d,%d)\n", player, S.time, placed, best->tx, best->ty);
            }
        }
    }

    // ---- the torch: a forest that runs downwind into the enemy
    if (torchX >= 0 || S.time < nextTorch) return;
    nextTorch = S.time + TORCH_LOOK;
    if (S.windSpeed < 0.35f || S.rain > 0.2f) { if (aiDebugF()) printf("[ai %d] t=%.0f torch: wind %.2f rain %.2f\n", player, S.time, S.windSpeed, S.rain); return; }
    // the torchers: the two cheapest infantry at home, standing or on guard (not hidden in an ambush wing, not on a wave)
    std::vector<Entity*> inf;
    for (Ref r : army) {
        Entity* u = S.get(r);
        if (!u || u->ut().kind != UK_INF || u->isAir() || inWing(r) || u->carrier.valid()) continue;
        if (u->order != O_IDLE && u->order != O_GUARDPOS && u->order != O_GUARDAREA) continue;
        if (dist2(u->pos, pl.basePos) > 30.0f * 30.0f * TILE * TILE) continue;
        inf.push_back(u);
    }
    if (inf.empty()) { if (aiDebugF()) printf("[ai %d] t=%.0f torch: no free infantry (%zu in the army)\n", player, S.time, army.size()); return; }
    std::sort(inf.begin(), inf.end(), [](const Entity* a, const Entity* b) { return a->ut().cost != b->ut().cost ? a->ut().cost < b->ut().cost : a < b; });   // (ties by slot: deterministic)
    // what the fire would reach: enemy structures (by what they cost) and infantry
    struct Mark { Vec2 at; float value; };
    static std::vector<Mark> marks; marks.clear();
    auto mark = [&](Vec2 at, float value) { Mark m; m.at = at; m.value = value; marks.push_back(m); };
    const float reach2 = TORCH_REACH * TORCH_REACH * TILE * TILE;
    for (auto& e : S.ents)
        if (e.alive && e.isBuilding() && dist2(e.pos, pl.basePos) < reach2 && e.owner >= 0 && e.owner < MAX_PLAYERS && S.enemies(player, e.owner) && e.constructed && BUILDS[e.type].role != BR_WAYMAKER)
            mark(e.pos, BUILDS[e.type].cost / 400.0f);
    for (Entity* e : enemyUnits) if (e->alive && e->isUnit() && e->ut().kind == UK_INF && dist2(e->pos, pl.basePos) < reach2) mark(e->pos, 1.5f);
    if (marks.empty()) return;
    const Vec2 home = pl.basePos;
    const size_t stride = std::max<size_t>(1, marks.size() / TORCH_TARGETS);
    const Vec2 across(-wind.y, wind.x);
    float bestScore = 1.2f; int bx = -1, by = -1; Vec2 stand;   // (two cheap infantry are a small stake: a structure or a squad in reach is worth it)
    for (size_t i = 0; i < marks.size(); i += stride) {
        for (int lat = -2; lat <= 2; lat += 2) for (int k = 3; k <= 14; k++) {
            const Vec2 t = marks[i].at + across * (lat * TILE);
            const Vec2 q = t - wind * (k * TILE);
            const int qx = tileOf(q.x), qy = tileOf(q.y);
            if (!treeAt(qx, qy) || S.fireAt(qx, qy) > 0) continue;
            // the trees must run on toward the mark (a base's yard is bare: the last three tiles need not be forest)
            int trees = 0;
            for (int j = 1; j <= k - 3; j++) trees += treeAt(tileOf(q.x + wind.x * j * TILE), tileOf(q.y + wind.y * j * TILE));
            if (trees < (k - 3) * 0.5f) continue;
            // never with our own base downwind of it
            const Vec2 toHome = home - q;
            if (toHome.len() < 26 * TILE && dot(toHome, wind) > 0) continue;
            float score = 0;
            for (const Mark& m : marks) if (dist2(m.at, t) < 9.0f * TILE * TILE) score += m.value;
            score *= (trees + 3.0f) / k;
            if (score <= bestScore) continue;
            // a spot to stand on, on the upwind side, that our infantry can walk to
            Vec2 sp; float up = 1e9f;
            for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
                const int nx = qx + dx, ny = qy + dy;
                if ((!dx && !dy) || !g_map.passable(nx, ny) || !g_map.sameGround(tileCenter(nx, ny), inf[0]->pos)) continue;
                const float u = dx * wind.x + dy * wind.y;
                if (u < up) { up = u; sp = tileCenter(nx, ny); }
            }
            if (up > 1e8f) continue;
            bool covered = false;   // (an armed enemy near the spot would cut the torch-bearers down before they got there)
            for (Entity* e : enemyUnits) if (e->alive && e->weapon() >= 0 && dist2(e->pos, sp) < 64.0f * TILE * TILE) { covered = true; break; }
            if (covered) continue;
            bestScore = score; bx = qx; by = qy; stand = sp;
            break;   // (the nearest forest edge upwind of this mark: farther ones along the same line are no better)
        }
    }
    if (aiDebugF()) printf("[ai %d] t=%.0f torch look: %zu infantry, %zu marks, best %.1f at (%d,%d)\n", player, S.time, inf.size(), marks.size(), bestScore, bx, by);
    if (bx < 0) return;
    torchers.clear();
    for (size_t i = 0; i < inf.size() && i < 2; i++) torchers.push_back(S.refOf(*inf[i]));
    S.cmdMove(torchers, stand, false);
    torchX = bx; torchY = by; torchStand = stand; torchUntil = S.time + TORCH_GIVEUP;
    if (aiDebugF()) printf("[ai %d] t=%.0f TORCH OUT to (%d,%d), score %.1f, wind %.2f\n", player, S.time, bx, by, bestScore, S.windSpeed);
}
