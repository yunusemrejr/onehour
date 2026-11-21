// One Hour - headless tests for the calls update (--callstest)
//  * Bastion Walls: no computer army may take one (enemy's, nobody's or its own ally's); a human's infiltrator still may take an enemy's
//  * a bunker's Satellite Scan: the whole map visible, one pass per bunker per BUNKER_SCAN_COOLDOWN, any ready bunker answers
//  * Gunship Call: HELI_CALL_COUNT of the army's gunships fly in from the map edge and gather round the spot; one call per tech structure per cooldown
//  * Find and Destroy: a human army's units hunt down every enemy unit and structure, the hidden ones too, and the mode ends when none is left
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_okC = true;
static bool failC(const char* m) { fprintf(stderr, "callstest: %s\n", m); g_okC = false; return false; }
#define CHECKC(c, m) do { if (!(c)) return failC(m); } while (0)

static void runC(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

static void initC(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, true };
    int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

static Ref placeNearC(int player, int type, Vec2 at) {
    int cx = tileOf(at.x), cy = tileOf(at.y);
    for (int r = 0; r < 24; r++) for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
        if (std::max(std::abs(x - cx), std::abs(y - cy)) != r) continue;
        if (g_sim.canPlace(player, type, x, y)) return g_sim.placeBuilding(type, player, x, y, true);
    }
    return NOREF;
}

static bool wallTest(u64 seed) {
    initC(seed);
    Entity* mine = g_sim.get(placeNearC(0, B_C_FWALL, g_sim.players[0].basePos + Vec2(6 * TILE, 0)));
    Entity* theirs = g_sim.get(placeNearC(1, B_K_FWALL, g_sim.players[1].basePos + Vec2(6 * TILE, 0)));
    CHECKC(mine && theirs, "could not place the Bastion Walls");
    CHECKC(!g_sim.spyMayTake(1, *mine), "a computer army may take a human's Bastion Wall");
    CHECKC(!g_sim.spyMayTake(0, *theirs), "a human's infiltrator may take an enemy's Bastion Wall");
    theirs->owner = -1;
    CHECKC(!g_sim.spyMayTake(1, *theirs), "a computer army may take a Bastion Wall nobody holds");
    CHECKC(!g_sim.spyMayTake(0, *theirs), "a human's infiltrator may take a Bastion Wall nobody holds");
    // a computer army's infiltrator next to the human's wall does not take it
    Ref spy = g_sim.spawnUnit(U_K_SPY, 1, mine->pos + Vec2(2 * TILE, 0));
    g_sim.cmdCapture({spy}, g_sim.refOf(*mine));
    runC(40);
    CHECKC(mine->alive && mine->owner == 0, "a computer army's infiltrator took the human's Bastion Wall");
    CHECKC(Sim::invulnerable(*mine), "the Bastion Wall is no longer indestructible");
    return true;
}

static bool scanTest(u64 seed) {
    initC(seed);
    Ref b1 = placeNearC(0, B_C_BUNKER, g_sim.players[0].basePos + Vec2(8 * TILE, 6 * TILE));
    Ref b2 = placeNearC(0, B_C_BUNKER, g_sim.players[0].basePos + Vec2(-8 * TILE, 6 * TILE));
    CHECKC(g_sim.get(b1) && g_sim.get(b2), "could not place two bunkers");
    CHECKC(g_sim.bunkerScansReady(0) == 2, "two finished bunkers do not both offer a Satellite Scan");
    Player& pl = g_sim.players[0];
    std::fill(pl.explored.begin(), pl.explored.end(), 0);
    CHECKC(g_sim.cmdBunkerScan(0, b2), "the Satellite Scan was refused");
    CHECKC(pl.revealUntil > g_sim.time + 10.0f, "the Satellite Scan did not reveal the map");
    CHECKC(std::count(pl.explored.begin(), pl.explored.end(), 0) == 0, "the Satellite Scan did not chart the whole map");
    CHECKC(g_sim.get(b2)->scanTimer > g_sim.time && g_sim.get(b1)->scanTimer <= g_sim.time, "the chosen bunker did not take the call");
    CHECKC(g_sim.cmdBunkerScan(0, b2), "the second bunker did not answer");
    CHECKC(!g_sim.cmdBunkerScan(0), "a third Satellite Scan was allowed with both bunkers cooling down");
    CHECKC(std::fabs(g_sim.bunkerScanWait(0) - BUNKER_SCAN_COOLDOWN) < 1.0f, "the Satellite Scan cooldown is wrong");
    return true;
}

static bool heliTest(u64 seed) {
    initC(seed);
    Player& pl = g_sim.players[0];
    CHECKC(g_sim.get(placeNearC(0, B_C_TECH, pl.basePos + Vec2(0, 9 * TILE))), "could not place a Data Center");
    pl.dropReady = 0;
    CHECKC(g_sim.heliCallsReady(0) == 1, "the Data Center does not offer a Gunship Call");
    const Vec2 spot = pl.basePos + Vec2(14 * TILE, -10 * TILE);
    CHECKC(g_sim.cmdHeliCall(0, spot), "the Gunship Call was refused");
    CHECKC(!g_sim.cmdHeliCall(0, spot), "a second Gunship Call was allowed from one Data Center");
    runC(12);
    int n = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.type == U_C_HELI) { n++; if (!e.isAir()) return failC("a called gunship is not in the air"); }
    CHECKC(n == HELI_CALL_COUNT, "the Gunship Call did not bring exactly HELI_CALL_COUNT gunships");
    runC(120);
    int near = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.type == U_C_HELI && dist(e.pos, spot) < 12.0f * TILE) near++;
    if (near < HELI_CALL_COUNT * 4 / 5) { fprintf(stderr, "callstest: %d of %d gunships reached the spot\n", near, HELI_CALL_COUNT); return failC("the gunships did not gather at the called spot"); }
    return true;
}

static bool huntTest(u64 seed) {
    initC(seed);
    // the enemy keeps one structure, a few soldiers, a stealthy sniper and a tank; the human gets a tank group and gunships
    Vec2 eb = g_sim.players[1].basePos;
    Entity* keep = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && e.bt().role == BR_HQ) keep = &e;
    CHECKC(keep, "the computer army has no Command Center");
    std::vector<Ref> gone;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && &e != keep) gone.push_back(g_sim.refOf(e));
    for (Ref r : gone) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    for (int k = 0; k < 4; k++) g_sim.spawnUnit(U_K_INF1, 1, g_map.nearestFree(eb + Vec2((k - 2) * TILE, 6 * TILE), 6));
    g_sim.spawnUnit(U_K_SNIPER, 1, g_map.nearestFree(eb + Vec2(-7 * TILE, -5 * TILE), 6));
    g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(eb + Vec2(7 * TILE, -5 * TILE), 6));
    Vec2 hb = g_sim.players[0].basePos;
    for (int k = 0; k < 8; k++) g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(hb + Vec2((k % 4) * TILE, (k / 4 + 4) * TILE), 6));
    for (int k = 0; k < 4; k++) g_sim.spawnUnit(U_C_HELI, 0, hb + Vec2(k * TILE, -3 * TILE));
    // ... and a dived submarine out at sea, with two destroyers of the human's on the same sea
    Vec2 subAt = g_map.nearestWater(eb, 60, true, Map::OPEN_SEA), seaH = g_map.nearestWater(hb, 60, true, Map::OPEN_SEA);
    CHECKC(g_map.openSeaAt(subAt) && g_map.openSeaAt(seaH), "no open sea near the bases");
    Entity* sub = g_sim.get(g_sim.spawnUnit(U_K_SUB, 1, subAt));
    CHECKC(sub, "could not spawn the submarine");
    sub->subState = 2;
    for (int k = 0; k < 2; k++) g_sim.spawnUnit(U_C_DESTROYER, 0, g_map.nearestWater(seaH + Vec2(k * 2 * TILE, 0), 8, true, Map::OPEN_SEA));
    g_sim.cmdFindDestroy(0, true);
    CHECKC(g_sim.players[0].hunting, "Find and Destroy did not switch on");
    g_sim.cmdFindDestroy(1, true);
    CHECKC(!g_sim.players[1].hunting, "a computer army may use Find and Destroy");
    float t0 = g_sim.time;
    while (g_sim.players[0].hunting && g_sim.time - t0 < 600.0f) runC(5);
    int left = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE && !Sim::invulnerable(e)) left++;
    if (left > 0 || g_sim.players[0].hunting) {
        fprintf(stderr, "callstest: after %.0f s, %d enemy things left (hunting %d)\n", g_sim.time - t0, left, (int)g_sim.players[0].hunting);
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE) fprintf(stderr, "  left: %s at %.0f,%.0f\n", e.isUnit() ? e.ut().name : e.bt().name, e.pos.x / TILE, e.pos.y / TILE);
        return failC("Find and Destroy did not finish off the enemy");
    }
    fprintf(stderr, "callstest: Find and Destroy cleared the enemy in %.0f s\n", g_sim.time - t0);
    return true;
}

// a finished bunker comes garrisoned (3 snipers, 2 riflemen); its Para Backup and Tank Drop each bring 2 gunships
static bool bunkerCrewTest(u64 seed) {
    initC(seed);
    Ref B = placeNearC(0, B_C_BUNKER, g_sim.players[0].basePos + Vec2(8 * TILE, 6 * TILE));
    Entity* b = g_sim.get(B);
    CHECKC(b, "could not place a bunker");
    b->constructed = false; g_sim.finishBuildingPublic(*b); b = g_sim.get(B);
    CHECKC(b->passengers.size() == (size_t)(BUNKER_START_SNIPERS + BUNKER_START_RIFLES), "a finished bunker does not hold its 5-man garrison");
    CHECKC(g_sim.bunkerSnipers(*b) == BUNKER_START_SNIPERS, "a finished bunker's garrison does not hold 3 snipers");
    b->bunkerUpg |= (u8)((1 << BU_PARA) | (1 << BU_TANKS)); b->paraReady = b->tankReady = 0;
    auto escorts = [] { int n = 0; for (auto& f : g_sim.fleetArrivals) if (f.escort && f.owner == 0 && f.air) n++; return n; };
    const Vec2 spot = g_sim.players[0].basePos + Vec2(0, 14 * TILE);
    CHECKC(g_sim.cmdBunkerPara(B, spot), "the Para Backup was refused");
    CHECKC(escorts() == DROP_ESCORT_HELIS, "a Para Backup brought no gunship escort");
    CHECKC(g_sim.cmdTankDrop(B, spot), "the Tank Drop was refused");
    CHECKC(escorts() == 2 * DROP_ESCORT_HELIS, "a Tank Drop brought no gunship escort");
    runC(5);
    int helis = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.type == U_C_HELI) helis++;
    CHECKC(helis == 2 * DROP_ESCORT_HELIS, "the drop escorts did not fly in");
    printf("callstest: bunker garrison %zu (%d snipers), %d escort gunships\n", b->passengers.size(), g_sim.bunkerSnipers(*b), helis);
    return true;
}

// Find and Destroy: a submarine with its missile ready launches it at the enemy, never where the blast would reach the army's own structures
static bool huntSubTest(u64 seed) {
    initC(seed);
    Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 34) * TILE, WORLD_H * 0.5f), 14, true, Map::OPEN_SEA);
    Ref S = g_sim.spawnUnit(U_C_SUB, 0, sea);
    Entity* sub = g_sim.get(S);
    CHECKC(sub, "could not launch a submarine");
    sub->siloTimer = 0.01f;   // (armed: 0 would mean a new boat still arming)
    g_sim.cmdFindDestroy(0, true);
    runC(2);
    sub = g_sim.get(S);
    CHECKC(sub && (sub->nukeSalvo != 0 || !g_sim.nukes.empty()), "a hunting submarine did not launch its missile");
    Vec2 at = sub->nukeSalvo != 0 ? sub->nukeSalvoPos : g_sim.nukes.back().pos;
    const float R = NUKE_RADIUS * TILE * std::sqrt(SUB_NUKE_SCALE);
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0) CHECKC(dist(e.pos, at) > R, "a hunting submarine aimed where its blast reaches the army's own structures");
    int hit = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && dist(e.pos, at) <= R) hit++;
    CHECKC(hit > 0, "a hunting submarine aimed where no enemy structure stands");
    printf("callstest: hunting submarine aimed at %d enemy structures\n", hit);
    g_sim.cmdFindDestroy(0, false);
    return true;
}

// Protect Us: idle units patrol round the posts, a few scout and come back, an intruder near a post is driven off and nobody chases it home
static bool protectTest(u64 seed) {
    initC(seed);
    const Vec2 base = g_sim.players[0].basePos;
    std::vector<Ref> tanks;
    for (int k = 0; k < 16; k++) tanks.push_back(g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(base + Vec2((k % 4 - 1.5f) * 2 * TILE, 10 * TILE + (k / 4) * 2 * TILE), 6)));
    g_sim.cmdProtect(0, true);
    CHECKC(g_sim.players[0].protecting, "Protect Us did not switch on");
    runC(4);
    int patrol = 0, scout = 0;
    for (Ref r : tanks) if (Entity* e = g_sim.get(r)) { if (e->protectJob == 1) patrol++; if (e->protectJob == 2) scout++; }
    CHECKC(patrol >= 10 && scout >= 1, "Protect Us did not send units on patrol and scouting");
    Entity* hq = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_HQ) hq = &e;
    CHECKC(hq, "the human army has no Command Center");
    Ref foe = g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(hq->pos + Vec2(0, -12 * TILE), 6));
    g_sim.get(foe)->detUntil[0] = g_sim.time + 60;
    runC(1.6f);
    int defend = 0;
    for (Ref r : tanks) if (Entity* e = g_sim.get(r)) if (e->protectJob == 3) defend++;
    CHECKC(defend >= 3, "Protect Us did not send defenders at an intruder near the Command Center");
    runC(10);
    CHECKC(!g_sim.get(foe), "the defenders did not destroy the intruder");
    runC(3);
    for (Ref r : tanks) if (Entity* e = g_sim.get(r)) CHECKC(!(e->protectJob == 3 && e->order == O_ATTACK), "a defender did not return to its post after the intruder was gone");
    g_sim.cmdFindDestroy(0, true);
    CHECKC(!g_sim.players[0].protecting, "Find and Destroy did not end Protect Us");
    g_sim.cmdProtect(0, true);
    CHECKC(!g_sim.players[0].hunting, "Protect Us did not end Find and Destroy");
    g_sim.cmdProtect(0, false);
    printf("callstest: Protect Us %d patrolling, %d scouting, %d drove off the intruder\n", patrol, scout, defend);
    return true;
}

bool callsTest(u64 seed) {
    g_okC = true;
    wallTest(seed);
    scanTest(seed);
    heliTest(seed);
    huntTest(seed);
    bunkerCrewTest(seed);
    huntSubTest(seed);
    protectTest(seed);
    printf("callstest: %s\n", g_okC ? "PASS" : "FAIL");
    return g_okC;
}
