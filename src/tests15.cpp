// One Hour - headless tests for the towns round the lake, the riots and the newer commander (--civiltest)
//  * the map: four civilian areas between the beach and the ring road, every block neutral, reachable, streets round every block
//  * an infiltrator takes a whole town at once; it pays its holder (nothing while too damaged); the holder's enemies target it
//  * a razed block falls into a ruin (never vanishes) and is raised again by its people, several times faster with a dozer helping
//  * a town is not an army: an army left with nothing but a town is out, and its towns are free again
//  * riots: a beaten army sees crowds rise near its base and wreck its structures; a strong one does not; rioters belong to no army and fight every
//    army; none rise before RIOT_GRACE or with riots switched off
//  * the commander: a persona and a name; a computer army takes a neutral town with an infiltrator; a teammate joins a teammate's attack;
//    the human can raise a computer army's difficulty, never lower it
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok15 = true;
static bool fail15(const char* m) { fprintf(stderr, "civiltest: %s\n", m); g_ok15 = false; return false; }
#define CHECK15(c, m) do { if (!(c)) return fail15(m); } while (0)

static void run15(float sec, bool ai = false) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n && !g_sim.gameOver; t++) { g_sim.step(); if (ai) g_ai.update(); g_sim.events.clear(); } }

// players: 0 the human (Cyber), 1 a computer enemy (Clanker)
static void init15(u64 seed, bool riots) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, true };
    int diff[2] = { 1, 1 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_sim.riotsOn = riots;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 100000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    std::vector<Ref> units;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) units.push_back(g_sim.refOf(e));
    for (Ref r : units) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

static Ref spawn15(int type, int owner, Vec2 at) { return g_sim.spawnUnit(type, owner, g_map.nearestFree(at, 8)); }

static int heldBy15(const District& D) {   // the owner every block agrees on (-2: they disagree)
    int o = -3;
    for (Ref r : D.blocks) { const Entity* b = g_sim.get(r); if (!b) return -2; if (o == -3) o = b->owner; else if (o != b->owner) return -2; }
    return o;
}

// ---- the map's towns
static bool mapTest15() {
    init15(3, false);
    CHECK15(g_map.districts.size() == 4 && g_sim.districts.size() == 4, "the map does not have four towns round the lake");
    int cities = 0;
    for (auto& D : g_sim.districts) {
        CHECK15(D.blocks.size() >= 10, "a town has fewer than ten blocks");
        CHECK15(D.owner < 0 && heldBy15(D) == -1, "a town does not start neutral");
        CHECK15(D.pay > 0 && D.people > 0, "a town pays nothing or has nobody living in it");
        if (D.city) cities++;
        bool hall = false;
        for (Ref r : D.blocks) {
            const Entity* b = g_sim.get(r);
            CHECK15(b && b->isBuilding() && b->bt().role == BR_CIVIC && b->district >= 0 && b->constructed, "a town block is not a standing civic building");
            if (civicKind(b->type) == CV_TOWNHALL) hall = true;
            // its street: at least one side of the block opens onto walkable ground
            bool street = false;
            for (int x = b->tx - 1; x <= b->tx + b->bt().w && !street; x++) street = g_map.passable(x, b->ty - 1) || g_map.passable(x, b->ty + b->bt().h);
            CHECK15(street, "a town block has no street in front of it");
        }
        CHECK15(hall, "a town has no Town Hall");
        // a dozer from every corner can drive into the town
        for (auto& st : g_map.starts) {
            std::vector<Vec2> path;
            CHECK15(g_map.findPath(tileCenter(st.tx, st.ty + 4), g_map.nearestFree(D.center, 8), path), "a town cannot be reached from a start position");
        }
        float d = dist(D.center, Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f)) / TILE;
        CHECK15(d > 40 && d < 66, "a town is not between the lake and the ring road");
    }
    CHECK15(cities == 2, "there are not two cities and two towns");
    for (auto& s : g_map.supplies) CHECK15(!(g_map.blocked[s.ty * MAP_W + s.tx] & 2), "a town was built over a supply pile");
    CHECK15(g_sim.countBuildings(0) == 1 && g_sim.countBuildings(1) == 1, "town blocks are counted as an army's structures");
    printf("civiltest: map ok (%d towns, %d blocks each at least, streets, reachable from every start)\n", (int)g_sim.districts.size(), (int)g_sim.districts[0].blocks.size());
    return true;
}

// ---- capture, pay, targeting, ruins and rebuilding
static bool townTest15() {
    init15(5, false);
    District& D = g_sim.districts[0];
    Entity* blk = g_sim.get(D.blocks[0]);
    // an infiltrator takes the whole town through one block
    Ref spy = spawn15(U_C_SPY, 0, blk->pos + Vec2(0, (blk->bt().h * 0.5f + 1.2f) * TILE));
    g_sim.cmdCapture({spy}, D.blocks[0]);
    CHECK15(g_sim.get(spy)->order == O_CAPTURE, "an infiltrator would not go after a neutral town");
    run15(DISTRICT_CAPTURE + 12.0f);
    CHECK15(D.owner == 0 && heldBy15(D) == 0, "the infiltrator did not take the whole town");
    CHECK15(g_sim.get(spy) != nullptr, "the infiltrator did not survive the capture");
    // it pays its holder
    int m0 = g_sim.players[0].money;
    run15(DISTRICT_PAY_INTERVAL * 3 + 0.5f);
    int gain = g_sim.players[0].money - m0;
    CHECK15(gain >= D.pay * 2 && gain <= D.pay * 4, "the town does not pay its holder");
    // an enemy tank takes it for a target; a neutral town it would ignore
    Ref tk = spawn15(U_K_TANK, 1, blk->pos + Vec2(0, (blk->bt().h * 0.5f + 3.0f) * TILE));
    Entity* tke = g_sim.get(tk);
    bool sees = false;
    g_sim.forEachNear(tke->pos, 6.0f * TILE, [&](Entity& t) { if (t.district == 0 && g_sim.canTarget(*tke, t) && g_sim.enemies(1, t.owner)) sees = true; });
    CHECK15(sees, "an enemy does not see a held town as a target");
    const District& N = g_sim.districts[1];
    const Entity* nb = g_sim.get(N.blocks[0]);
    CHECK15(!g_sim.enemies(1, nb->owner), "a neutral town is somebody's enemy");
    g_sim.destroy(*tke, false);
    // razed: a ruin, never gone; nobody shoots rubble
    for (Ref r : D.blocks) if (Entity* b = g_sim.get(r)) g_sim.applyDamagePublic(*b, b->maxHp * 4.0f, 1, NOREF);
    for (Ref r : D.blocks) { const Entity* b = g_sim.get(r); CHECK15(b && !b->constructed && b->progress < 0.01f, "a razed block vanished instead of falling into a ruin"); }
    Ref tk2 = spawn15(U_K_TANK, 1, blk->pos + Vec2(0, (blk->bt().h * 0.5f + 3.0f) * TILE));
    CHECK15(!g_sim.canTarget(*g_sim.get(tk2), *g_sim.get(D.blocks[0])), "an enemy still shoots at a fresh ruin");
    g_sim.destroy(*g_sim.get(tk2), false);
    run15(1.0f);
    CHECK15(D.owner == 0 && D.ruins == (int)D.blocks.size() && D.integrity < DISTRICT_PAY_MIN, "the razed town did not keep its holder or report its ruins");
    // too damaged: no pay
    m0 = g_sim.players[0].money;
    run15(DISTRICT_PAY_INTERVAL * 2 + 0.5f);
    CHECK15(g_sim.players[0].money == m0, "a razed town still pays");
    // its people raise it again by themselves; one block gets a dozer and is done much sooner
    Entity* b0 = g_sim.get(D.blocks[0]);
    Ref dz = spawn15(U_C_DOZER, 0, b0->pos + Vec2(0, (b0->bt().h * 0.5f + 1.0f) * TILE));
    g_sim.cmdAssist({dz}, D.blocks[0]);
    float tDozer = -1, tAlone = -1, t0 = g_sim.time;
    for (int t = 0; t < (int)((DISTRICT_REBUILD + DISTRICT_REBUILD_DELAY + 20) * SIM_HZ); t++) {
        g_sim.step(); g_sim.events.clear();
        if (tDozer < 0 && g_sim.get(D.blocks[0])->constructed) tDozer = g_sim.time - t0;
        if (tAlone < 0 && g_sim.get(D.blocks[1])->constructed) tAlone = g_sim.time - t0;
        if (tDozer >= 0 && tAlone >= 0) break;
    }
    CHECK15(tAlone > 0, "a razed block was never rebuilt by its people");
    CHECK15(tDozer > 0 && tDozer < tAlone * 0.6f, "a dozer did not make the rebuilding much faster");
    run15(30.0f);
    CHECK15(D.ruins == 0 && D.integrity > 0.6f, "the town did not rise again");
    m0 = g_sim.players[0].money;
    run15(DISTRICT_PAY_INTERVAL + 0.5f);
    CHECK15(g_sim.players[0].money > m0, "the rebuilt town does not pay again");
    // the enemy's infiltrator takes it back
    Ref espy = spawn15(U_K_SPY, 1, blk->pos + Vec2(0, (blk->bt().h * 0.5f + 1.2f) * TILE));
    g_sim.cmdCapture({espy}, D.blocks[0]);
    run15(DISTRICT_CAPTURE + 12.0f);
    CHECK15(D.owner == 1 && heldBy15(D) == 1, "an enemy infiltrator could not take the town back");
    // an army left with nothing but a town is out, and its town is free again
    std::vector<Ref> mine;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.district < 0) mine.push_back(g_sim.refOf(e));
    for (Ref r : mine) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    run15(2.0f);
    CHECK15(!g_sim.players[1].alive, "an army with nothing but a town is still in the game");
    CHECK15(D.owner == -1 && heldBy15(D) == -1, "a beaten army's town did not fall back to nobody");
    printf("civiltest: towns ok (whole-town capture, pay $%d in 30 s, targeted when held, ruins never vanish, rebuilt alone in %.0f s and with a dozer in %.0f s, retaken, freed when its army fell)\n", gain, tAlone, tDozer);
    return true;
}

// ---- riots
static bool riotTest15() {
    // a strong army against a beaten one: crowds rise against the beaten one only
    init15(9, true);
    Vec2 b0 = g_sim.players[0].basePos, b1 = g_sim.players[1].basePos;
    int bk1 = firstBuildOf(F_CLANKER);
    int placed = 0;
    for (int k = 0; k < 3; k++) for (int dy = -10; dy <= 10 && placed <= k; dy++) for (int dx = -10; dx <= 10 && placed <= k; dx++) {
        int tx = tileOf(b1.x) + dx, ty = tileOf(b1.y) + dy, type = bk1 + (k == 0 ? BR_SUPPLY : k == 1 ? BR_POWER : BR_BARRACKS);
        if (std::abs(dx) + std::abs(dy) >= 6 && g_sim.canPlace(1, type, tx, ty)) { g_sim.placeBuilding(type, 1, tx, ty, true); placed++; }
    }
    for (int i = 0; i < 16; i++) spawn15(U_C_TANK, 0, b0 + Vec2((i % 4) * 30.0f, (i / 4) * 30.0f + 5 * TILE));
    float hp0 = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1) hp0 += e.hp;
    run15(RIOT_GRACE - 10.0f);
    CHECK15(g_sim.riotSeq == 0, "a crowd rose before the riots may start");
    run15(220.0f);
    CHECK15(g_sim.riotsRaised[1] >= 1, "no crowd rose against an army with no army left");
    CHECK15(g_sim.riotsRaised[0] == 0, "a crowd rose against an army firmly in control");
    CHECK15(g_sim.control[0] > 0.8f && g_sim.control[1] < 0.7f, "the order meters do not follow the armies' strength");
    float hp1 = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1) hp1 += e.hp;
    CHECK15(hp1 < hp0, "the rioters did not damage the beaten army's base");
    CHECK15(g_sim.players[1].alive || g_sim.gameOver, "riots broke the game state");
    // rioters belong to no army and fight every army
    Entity* rioter = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == RIOT_PLAYER) { rioter = &e; break; }
    if (!rioter) {   // (the crowd may have melted away already: raise one to check)
        g_sim.unrest[1] = 5.0f; run15(1.1f);
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == RIOT_PLAYER) { rioter = &e; break; }
    }
    CHECK15(rioter && (rioter->type == U_RIOTER || rioter->type == U_FIREBOMBER), "no rioter to check");
    CHECK15(g_sim.enemies(RIOT_PLAYER, 0) && g_sim.enemies(RIOT_PLAYER, 1) && !g_sim.players[RIOT_PLAYER].active, "rioters are on some army's side");
    CHECK15(rioter->riot > 0, "a rioter does not march with a crowd");
    Ref tk = spawn15(U_C_TANK, 0, rioter->pos + Vec2(3 * TILE, 0));
    Entity* tke = g_sim.get(tk);
    CHECK15(g_sim.canTarget(*tke, *rioter) && g_sim.canTarget(*rioter, *tke), "rioters and an army's tank do not fight each other");
    int alive1 = 0; for (int p = 0; p < g_sim.numPlayers; p++) alive1 += g_sim.players[p].alive;
    CHECK15(alive1 == 2, "rioters changed who is in the game");
    const int crowds = g_sim.riotsRaised[1]; const float ord0 = g_sim.control[0], ord1 = g_sim.control[1];
    // switched off: nothing rises
    init15(9, false);
    g_sim.time = RIOT_GRACE + 1; g_sim.unrest[0] = g_sim.unrest[1] = 5.0f;
    run15(10.0f);
    CHECK15(g_sim.riotSeq == 0 && g_sim.riotersAlive() == 0, "a crowd rose with the riots switched off");
    printf("civiltest: riots ok (%d crowds against the beaten army, none against the strong one; order %.2f vs %.2f; they wreck its base and fight every army)\n", crowds, ord0, ord1);
    return true;
}

// ---- the commander: persona, towns, team play, the difficulty raised mid-match
static bool aiTest15() {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CYBER, F_CLANKER };
    bool ai[4] = { true, true, true, true };
    int diff[4] = { 2, 2, 2, 2 };
    int team[4] = { 0, 1, 0, 1 };
    int townsTaken = 0, joins = 0;
    for (u64 seed = 21; seed < 23; seed++) {
        g_sim.init(4, fac, ai, diff, team, seed);
        for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = false; g_ai.smartEnabled[p] = true; g_ai.proEnabled[p] = true; g_ai.featMask[p] = FEAT_DEFAULT; }
        g_ai.init(seed);
        for (int p = 0; p < 4; p++) {
            CHECK15(g_ai.ais[p].commander && *g_ai.ais[p].commander, "a commander has no name");
            for (int q = 0; q < p; q++) CHECK15(std::string(g_ai.ais[p].commander) != g_ai.ais[q].commander, "two commanders share a name");
        }
        bool held = false;
        for (int t = 0; t < 16 * 60 * SIM_HZ && !g_sim.gameOver; t++) {
            g_sim.step(); g_ai.update(); g_sim.events.clear();
            if (t % SIM_HZ == 0) for (auto& D : g_sim.districts) if (D.owner >= 0) held = true;
        }
        if (held) townsTaken++;
        for (int p = 0; p < 4; p++) joins += g_ai.ais[p].teamJoins;
        CHECK15(g_ai.focus[0] != 0 && g_ai.focus[1] != 1, "a team focuses on itself");
    }
    CHECK15(townsTaken >= 1, "no computer army took a town in sixteen minutes");
    CHECK15(joins >= 1, "no commander ever joined a teammate's attack");
    // the difficulty: up one step at a time, never down, a Brutal commander gets its war chest
    init15(4, false);
    g_ai.init(4);
    CHECK15(!g_ai.raiseDifficulty(0), "the human's own difficulty was changed");
    int m0 = g_sim.players[1].money;
    CHECK15(g_ai.raiseDifficulty(1) && g_sim.players[1].difficulty == 2, "a computer army's difficulty did not go up");
    CHECK15(g_ai.raiseDifficulty(1) && g_sim.players[1].difficulty == 3 && g_sim.players[1].brutal() && g_sim.players[1].money == m0 + 6000, "a computer army raised to Brutal did not get its edge");
    CHECK15(!g_ai.raiseDifficulty(1) && g_sim.players[1].difficulty == 3, "a difficulty went past Brutal");
    printf("civiltest: commander ok (named personas, a town taken in %d of 2 four-army games, %d joint attacks, difficulty raised Normal -> Hard -> Brutal and no further)\n", townsTaken, joins);
    return true;
}

bool civilTest(u64 seed) {
    (void)seed;
    Sim::riotsDefault = true;
    mapTest15();
    if (g_ok15) townTest15();
    if (g_ok15) riotTest15();
    if (g_ok15) aiTest15();
    return g_ok15;
}
