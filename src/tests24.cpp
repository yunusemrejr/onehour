// One Hour - headless tests 24 (--subtest): the submarines look after themselves (an idle boat dives; one in danger crash-dives, even inside its
// post-launch exposure when a warhead comes for it, and runs for safer sea; ordered at a ship while dived it closes in under water, surfaces in range
// and goes down again after), their missile is the heaviest single warhead an army fires, they cost what a small army does; and snipers in a bunker
// make it many times harder for an infiltrator to take
#include "ai.h"
#include "testutil.h"

#define VERIFY24(c, m) do { if (!(c)) { fprintf(stderr, "subtest: %s\n", m); return false; } } while (0)

static void run24(float sec) { for (int t = 0, n = (int)(sec * SIM_HZ); t < n && !g_sim.gameOver; t++) { g_sim.step(); g_sim.events.clear(); } }
static bool alive24(Ref r) { return g_sim.get(r) != nullptr; }
static void scene24(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER }; bool ai[2] = { false, false }; int diff[2] = { 1, 2 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_ai.init(seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 100000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
}
static Vec2 sea24(float fy) { return g_map.nearestWater(Vec2((ISLE_X0 - 34) * TILE, WORLD_H * fy), 14, true, Map::OPEN_SEA); }
static Sim::Nuke enemyNuke24(int owner, Vec2 at, float scale = 1.0f) {
    Sim::Nuke n; n.from = at + Vec2(0, -40 * TILE); n.pos = at; n.owner = owner; n.t = 0; n.scale = scale; n.id = ++g_sim.nukeSeq;
    return n;
}

// ---- price and power: the most expensive unit in the game, and a missile that outreaches and outhits an Underground Nuke
static bool priceTest24() {
    for (int t = 0; t < U_COUNT; t++) if (!subType(t) && UNITS[t].cost > 0) VERIFY24(UNITS[U_C_SUB].cost > UNITS[t].cost, "a submarine is not the most expensive unit");
    VERIFY24(UNITS[U_C_SUB].cost >= 40000 && UNITS[U_K_SUB].cost == UNITS[U_C_SUB].cost, "a submarine is not much more expensive than before");
    Sim::Nuke s; s.scale = SUB_NUKE_SCALE; s.deep = true; s.sub = true;
    Sim::Nuke u; u.scale = UNUKE_SCALE; u.deep = true;
    VERIFY24(Sim::nukeRadius(s) > Sim::nukeRadius(u) && SUB_NUKE_DAMAGE > UNUKE_DAMAGE, "the submarine's warhead is not wider and harder than an Underground Nuke's");
    printf("subtest: price ok ($%d), warhead %.0f tiles x%.1f (deep silo %.0f tiles x%.1f)\n", UNITS[U_C_SUB].cost, Sim::nukeRadius(s) / TILE, SUB_NUKE_DAMAGE, Sim::nukeRadius(u) / TILE, UNUKE_DAMAGE);
    return true;
}

// ---- the warhead itself: a factory far outside a Nuke Ramp's blast is wrecked by a submarine's
static bool blastTest24(u64 seed) {
    scene24(seed);
    Vec2 c = openGroundNear(g_sim.players[1].basePos + Vec2(-30 * TILE, 0), 3);
    int tx = tileOf(c.x), ty = tileOf(c.y);
    Ref f = NOREF;
    for (int r = 0; r < 12 && !f.valid(); r++) for (int dy = -r; dy <= r && !f.valid(); dy++) for (int dx = -r; dx <= r && !f.valid(); dx++)
        if (g_sim.canPlace(1, B_K_FACTORY, tx + dx, ty + dy)) f = g_sim.placeBuilding(B_K_FACTORY, 1, tx + dx, ty + dy, true);
    VERIFY24(alive24(f), "no room for the target factory");
    Entity* F = g_sim.get(f);
    float hp0 = F->hp;
    Vec2 gz = F->pos + Vec2(NUKE_RADIUS * 1.6f * TILE, 0);   // well outside a ramp's 11-tile blast, well inside the submarine's
    Sim::Nuke n = enemyNuke24(0, gz, SUB_NUKE_SCALE); n.deep = true; n.sub = true; n.t = Sim::NUKE_FLIGHT - 0.05f;
    g_sim.nukes.push_back(n);
    run24(0.5f);
    F = g_sim.get(f);
    VERIFY24(!F || F->hp < hp0 * 0.75f, "a factory 18 tiles from a submarine's warhead was barely touched");
    printf("subtest: blast ok (factory 18 tiles out: %.0f%% of its health left)\n", F ? 100.0f * F->hp / hp0 : 0.0f);
    return true;
}

// ---- an idle surfaced boat dives on its own; a warhead coming down on a surfaced one sends it under before impact, even inside its exposure
static bool dodgeTest24(u64 seed) {
    scene24(seed);
    Vec2 sea = sea24(0.30f);
    Ref sub = g_sim.spawnUnit(U_C_SUB, 0, sea);
    VERIFY24(alive24(sub), "the submarine did not spawn");
    VERIFY24(g_sim.get(sub)->subState == 0, "a new submarine is not on the surface");
    run24(SUB_IDLE_DIVE + SUB_DIVE_TIME + 1.5f);
    VERIFY24(Sim::submerged(*g_sim.get(sub)), "an idle surfaced submarine did not dive by itself");
    // a player brings it up on purpose: it stays up while nothing threatens it
    g_sim.cmdDive({sub}, false);
    run24(SUB_SURFACE_TIME + 6.0f);
    VERIFY24(g_sim.get(sub)->subState == 0, "a submarine its player surfaced dived again with nothing threatening it");
    // a warhead comes down on it: down before impact, and not a scratch
    float hp = g_sim.get(sub)->hp;
    g_sim.nukes.push_back(enemyNuke24(1, g_sim.get(sub)->pos));
    run24(Sim::NUKE_FLIGHT + 1.0f);
    VERIFY24(alive24(sub) && g_sim.get(sub)->hp == hp, "a surfaced submarine sat out a warhead aimed at it");
    VERIFY24(g_sim.get(sub)->subState == 2 || g_sim.get(sub)->subState == 1, "the submarine did not crash-dive under the warhead");
    // just after a launch it must stay up - unless a warhead comes for it
    g_sim.get(sub)->siloTimer = g_sim.time;
    Vec2 aim = g_sim.players[1].basePos;
    VERIFY24(g_sim.cmdSubNuke(0, aim, false, sub), "the submarine refused a launch order");
    for (int k = 0; k < (int)((SUB_SURFACE_TIME + 1.0f) * SIM_HZ) && g_sim.get(sub)->nukeSalvo > 0; k++) { g_sim.step(); g_sim.events.clear(); }
    Entity* S = g_sim.get(sub);
    VERIFY24(S && S->subState == 0 && S->nukeSalvo == 0 && g_sim.time < S->exposedUntil, "the submarine did not surface and launch");
    bool silver = false;
    for (auto& n : g_sim.nukes) if (n.owner == 0 && n.sub && n.deep && std::fabs(n.scale - SUB_NUKE_SCALE) < 0.01f) silver = true;
    VERIFY24(silver, "the submarine's missile is not its silver heavy warhead");
    hp = S->hp;
    g_sim.nukes.push_back(enemyNuke24(1, S->pos));
    run24(Sim::NUKE_FLIGHT + 1.0f);
    S = g_sim.get(sub);
    VERIFY24(S && S->hp == hp && S->subState != 0, "a submarine inside its exposure did not crash-dive away from a warhead aimed at it");
    printf("subtest: dodge ok (idle boat dives; a warhead sends it under, exposure or not)\n");
    return true;
}

// ---- exposed after a launch with an enemy warship close by: it runs, then dives the moment it may
static bool evadeTest24(u64 seed) {
    scene24(seed);
    Vec2 sea = sea24(0.62f);
    Ref sub = g_sim.spawnUnit(U_C_SUB, 0, sea);
    VERIFY24(alive24(sub), "the submarine did not spawn");
    g_sim.get(sub)->siloTimer = g_sim.time;
    VERIFY24(g_sim.cmdSubNuke(0, g_sim.players[1].basePos, false, sub), "the submarine refused a launch order");
    run24(0.2f);
    Ref foe = g_sim.spawnUnit(U_K_DESTROYER, 1, g_map.nearestWater(g_sim.get(sub)->pos + Vec2(5 * TILE, 0), 6, true));
    VERIFY24(alive24(foe), "the destroyer did not spawn");
    g_sim.get(foe)->order = O_IDLE;
    float d0 = dist(g_sim.get(sub)->pos, g_sim.get(foe)->pos);
    run24(6.0f);
    VERIFY24(alive24(sub), "the submarine was sunk while it ran");
    float d1 = dist(g_sim.get(sub)->pos, g_sim.get(foe)->pos);
    VERIFY24(d1 > d0 + 3.0f * TILE, "an exposed submarine did not run from the enemy destroyer");
    run24(SUB_EXPOSED + 4.0f);
    VERIFY24(alive24(sub) && (Sim::submerged(*g_sim.get(sub)) || g_sim.get(sub)->subState == 1), "the submarine did not dive once its exposure ended");
    printf("subtest: evade ok (ran %.1f tiles from the destroyer, dived when it could; %.0f%% hull left)\n", (d1 - d0) / TILE, 100.0f * g_sim.get(sub)->hp / g_sim.get(sub)->maxHp);
    return true;
}

// ---- ordered at a ship while dived: it closes in under water, comes up in range, sinks it and goes down again
static bool huntTest24(u64 seed) {
    scene24(seed);
    Vec2 sea = sea24(0.40f);
    Ref sub = g_sim.spawnUnit(U_C_SUB, 0, sea);
    g_sim.cmdDive({sub}, true);
    run24(SUB_DIVE_TIME + 0.5f);
    VERIFY24(alive24(sub) && Sim::submerged(*g_sim.get(sub)), "the submarine did not dive");
    Ref boat = g_sim.spawnUnit(U_K_DESTROYER, 1, g_map.nearestWater(sea + Vec2(0, 18 * TILE), 8, true));
    VERIFY24(alive24(boat), "the target did not spawn");
    // (its player sees the target: a spotter of ours rides close by in a test)
    g_sim.get(boat)->detUntil[0] = 1e9f;
    g_sim.cmdAttack({sub}, boat);
    bool upInRange = false; float closest = 1e9f;
    for (int k = 0; k < (int)(70 * SIM_HZ) && alive24(boat); k++) {
        g_sim.step(); g_sim.events.clear();
        Entity* S = g_sim.get(sub); Entity* B = g_sim.get(boat);
        if (!S) break;
        if (B) { float d = g_sim.distToEntity(S->pos, *B); closest = std::min(closest, d); if (S->subState == 0 && d <= WEAPONS[S->weapon()].range * TILE + TILE) upInRange = true; }
    }
    VERIFY24(alive24(sub), "the hunting submarine was sunk");
    VERIFY24(upInRange, "the submarine never surfaced within torpedo range of its target");
    VERIFY24(!alive24(boat) || g_sim.get(boat)->hp < g_sim.get(boat)->maxHp * 0.6f, "the submarine's torpedoes did not hurt the target");
    run24(SUB_IDLE_DIVE + SUB_DIVE_TIME + 2.0f);
    if (!alive24(boat)) VERIFY24(alive24(sub) && Sim::submerged(*g_sim.get(sub)), "the submarine did not go down again after the hunt");
    printf("subtest: hunt ok (closed in to %.1f tiles, target %s)\n", closest / TILE, alive24(boat) ? "crippled" : "sunk");
    return true;
}

// ---- snipers in a bunker: each multiplies the time an infiltrator needs; an infiltrator does not take a sniper-held bunker in the time an empty one falls
static bool bunkerTest24(u64 seed) {
    scene24(seed);
    Vec2 c = openGroundNear(g_sim.players[1].basePos + Vec2(-26 * TILE, 10 * TILE), 6);
    Ref bk[2] = { NOREF, NOREF };
    for (int i = 0; i < 2; i++) {
        int tx = tileOf(c.x) + i * 14, ty = tileOf(c.y);
        for (int r = 0; r < 14 && !bk[i].valid(); r++) for (int dy = -r; dy <= r && !bk[i].valid(); dy++) for (int dx = -r; dx <= r && !bk[i].valid(); dx++)
            if (g_sim.canPlace(1, B_K_BUNKER, tx + dx, ty + dy)) bk[i] = g_sim.placeBuilding(B_K_BUNKER, 1, tx + dx, ty + dy, true);
        VERIFY24(alive24(bk[i]), "no room for the bunkers");
    }
    Ref spy0 = g_sim.spawnUnit(U_C_SPY, 0, g_sim.get(bk[0])->pos + Vec2(0, 3 * TILE));
    Ref spy1 = g_sim.spawnUnit(U_C_SPY, 0, g_sim.get(bk[1])->pos + Vec2(0, 3 * TILE));
    const float base = g_sim.captureTime(*g_sim.get(bk[0]), *g_sim.get(spy0));
    for (int k = 0; k < 3; k++) { Ref s = g_sim.spawnUnit(U_K_SNIPER, 1, g_sim.get(bk[1])->pos + Vec2(0, -3 * TILE)); g_sim.boardPublic(*g_sim.get(s), *g_sim.get(bk[1])); }
    VERIFY24(g_sim.bunkerSnipers(*g_sim.get(bk[1])) == 3, "the snipers are not in the bunker");
    const float held = g_sim.captureTime(*g_sim.get(bk[1]), *g_sim.get(spy1));
    VERIFY24(held >= base * (1.0f + 3.0f * BUNKER_SNIPER_CAPTURE) - 0.01f, "three snipers do not make a bunker many times harder to take");
    { Ref s = g_sim.spawnUnit(U_K_SNIPER, 1, g_sim.get(bk[1])->pos + Vec2(0, -3 * TILE)); g_sim.boardPublic(*g_sim.get(s), *g_sim.get(bk[1])); }
    VERIFY24(g_sim.captureTime(*g_sim.get(bk[1]), *g_sim.get(spy1)) > held, "a fourth sniper does not make it harder still");
    g_sim.cmdCapture({spy0}, bk[0]);
    g_sim.cmdCapture({spy1}, bk[1]);
    run24(base * 2.0f + 6.0f);
    VERIFY24(g_sim.get(bk[0]) && g_sim.get(bk[0])->owner == 0, "an infiltrator did not take an empty bunker");
    VERIFY24(g_sim.get(bk[1]) && g_sim.get(bk[1])->owner == 1, "an infiltrator took a sniper-held bunker as fast as an empty one");
    printf("subtest: bunker snipers ok (empty %.0f s; three snipers %.0f s; the infiltrator %s)\n", base, held, alive24(spy1) ? "still at it" : "was shot");
    return true;
}

// ---- base layout: a factory whose exit pocket is sealed still lets its units out onto open ground; a unit sealed into a one-tile gap between two
// structures is moved out; a dozer does not take on a repair it cannot reach (an island launcher out in the lake) and drops one it is sent to
static bool layoutTest24(u64 seed) {
    scene24(seed);
    Vec2 c = openGroundNear(g_sim.players[0].basePos + Vec2(30 * TILE, 30 * TILE), 9);
    flattenArena(c, 10, 10);
    int cx = tileOf(c.x), cy = tileOf(c.y);
    // a War Factory with a second structure two rows under it and walls closing the gap's ends: its usual exit is a sealed pocket
    Ref fac = g_sim.placeBuilding(B_C_FACTORY, 0, cx - 2, cy - 4, true);
    Ref low = g_sim.placeBuilding(B_C_FACTORY, 0, cx - 2, cy + 1, true);
    VERIFY24(alive24(fac) && alive24(low), "no room for the factories");
    const BuildType& fb = BUILDS[B_C_FACTORY];
    for (int y = cy - 4 + fb.h; y < cy + 1; y++) { g_sim.placeBuilding(B_C_WALL, 0, cx - 3, y, true); g_sim.placeBuilding(B_C_WALL, 0, cx - 2 + fb.w, y, true); }
    g_map.buildRegions();
    Vec2 out = g_sim.unitExit(*g_sim.get(fac));
    int ox = tileOf(out.x), oy = tileOf(out.y);
    VERIFY24(g_map.passable(ox, oy), "the factory's exit is not on free ground");
    u16 rg = g_map.region[oy * MAP_W + ox];
    VERIFY24(rg && (g_map.regionBox[rg * 4 + 2] - g_map.regionBox[rg * 4]) >= 12, "a factory with its usual exit sealed still puts its units into the pocket");
    // a jeep in a one-tile gap between two structures, closed at both ends by walls: it is moved out
    Vec2 c2 = openGroundNear(g_sim.players[0].basePos + Vec2(-30 * TILE, 30 * TILE), 9);
    flattenArena(c2, 10, 10);
    int x2 = tileOf(c2.x), y2 = tileOf(c2.y);
    g_sim.placeBuilding(B_C_BARRACKS, 0, x2 - BUILDS[B_C_BARRACKS].w, y2 - 1, true);
    g_sim.placeBuilding(B_C_BARRACKS, 0, x2 + 1, y2 - 1, true);
    Ref jeep = g_sim.spawnUnit(U_C_JEEP, 0, tileCenter(x2, y2));
    run24(0.05f);   // (into the spatial grid, as every unit of a running game is)
    g_sim.get(jeep)->pos = tileCenter(x2, y2);
    g_sim.placeBuilding(B_C_WALL, 0, x2, y2 - 2, true);
    g_sim.placeBuilding(B_C_WALL, 0, x2, y2 + BUILDS[B_C_BARRACKS].h - 1, true);
    Entity* J = g_sim.get(jeep);
    VERIFY24(J && !(tileOf(J->pos.x) == x2 && tileOf(J->pos.y) >= y2 - 1 && tileOf(J->pos.y) < y2 - 1 + BUILDS[B_C_BARRACKS].h), "a jeep sealed into a one-tile gap was left there");
    // the commander's walls never close such a gap at all
    VERIFY24(g_map.wouldPlug(x2, y2 + 6, 1, 1, true) == g_map.wouldPlug(x2, y2 + 6, 1, 1, false), "strict plug test disagrees on open ground");
    // an island launcher of ours, hurt, out in the lake: a dozer on the shore does not take the job, and drops it if it is ordered to
    Entity* L = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE && !e.secret) { L = &e; break; }
    VERIFY24(L, "no island launcher");
    L->owner = 0; L->hp = L->maxHp * 0.5f; L->lastDamaged = -100;
    Vec2 shore = g_map.nearestFree(L->pos, 60);
    for (int r = 4; r < 80 && g_map.sameGround(shore, L->pos + Vec2(0, (L->bt().h * 0.5f + 1) * TILE)); r += 4) shore = g_map.nearestFree(L->pos + Vec2(0, r * TILE * 2.0f), 30);
    Ref dz = g_sim.spawnUnit(U_C_DOZER, 0, shore);
    VERIFY24(alive24(dz), "the dozer did not spawn");
    VERIFY24(!g_sim.canReachSite(*g_sim.get(dz), *L), "a dozer on the shore can reach a lake island");
    VERIFY24(g_sim.repairJob(*g_sim.get(dz), 200.0f * TILE) != L, "a dozer on the shore takes a repair out on a lake island");
    g_sim.cmdAssist({dz}, g_sim.refOf(*L));
    run24(3.0f);
    VERIFY24(alive24(dz) && !(g_sim.get(dz)->order == O_BUILD && g_sim.get(dz)->targetEnt == g_sim.refOf(*L)), "a dozer kept a repair it cannot reach");
    // a tank coming down under its parachute: the spot it aims for is covered while it falls, and a warhead's shockwave goes off below it; it
    // lands on free ground beside the spot, not in the structure (or in the water of a bridge that fell)
    {
        Vec2 c3 = openGroundNear(g_sim.players[0].basePos + Vec2(0, 34 * TILE), 6);
        flattenArena(c3, 7, 7);
        int x3 = tileOf(c3.x), y3 = tileOf(c3.y);
        Ref tk = g_sim.spawnUnit(U_C_TANK, 0, c3);
        Entity* T = g_sim.get(tk);
        VERIFY24(T, "the tank did not spawn");
        T->dropFrom = c3 + Vec2(0, -200); T->dropTo = tileCenter(x3, y3); T->fall = T->fallTime = 3.4f; T->pos = T->dropFrom;
        run24(1.0f);
        g_sim.placeBuilding(B_C_WALL, 0, x3, y3, true);
        Sim::Nuke n = enemyNuke24(1, c3 + Vec2(6 * TILE, 0), 0.1f); n.t = Sim::NUKE_FLIGHT - 0.05f;
        g_sim.nukes.push_back(n);
        for (int k = 0; k < 4 * SIM_HZ && g_sim.get(tk) && g_sim.get(tk)->fall > 0; k++) { g_sim.step(); g_sim.events.clear(); }   // (checked on the tick it lands)
        T = g_sim.get(tk);
        if (T) VERIFY24(T->fall <= 0 && g_map.passable(tileOf(T->pos.x), tileOf(T->pos.y)), "a tank that came down by parachute landed on a spot that was covered while it fell");
    }
    printf("subtest: layout ok (sealed factory exit avoided, sealed jeep moved out, unreachable repair dropped, a parachute load lands on free ground)\n");
    return true;
}

bool subTest(u64 seed) {
    if (!priceTest24()) return false;
    if (!blastTest24(seed)) return false;
    if (!dodgeTest24(seed + 1)) return false;
    if (!evadeTest24(seed + 2)) return false;
    if (!huntTest24(seed + 3)) return false;
    if (!bunkerTest24(seed + 4)) return false;
    if (!layoutTest24(seed + 5)) return false;
    printf("subtest: ok\n");
    return true;
}
