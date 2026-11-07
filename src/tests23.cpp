// One Hour - headless tests 23 (--relictest): the 384 map's canal, its bridges (down under fire, raised again by any army's dozer), the abandoned
// relic bases (infantry claim them one structure at a time, the Relic Command pays and unlocks relic building, the Hydra Silo's five warheads, no
// selling them), the destructible island launchers, the nuclear submarines (dived: unseen and untouchable; up to launch: exposed), the warships'
// drone swarms and the surrender of a computer army that has nothing left
#include "ai.h"
#include "testutil.h"

#define VERIFY23(c, m) do { if (!(c)) { fprintf(stderr, "relictest: %s\n", m); return false; } } while (0)

static void run23(float sec, bool ai = false) { for (int t = 0, n = (int)(sec * SIM_HZ); t < n && !g_sim.gameOver; t++) { g_sim.step(); if (ai) g_ai.update(); g_sim.events.clear(); } }
static bool alive23(Ref r) { return g_sim.get(r) != nullptr; }
static Ref place23(int type, int owner, Vec2 nearP) {
    int cx = tileOf(nearP.x), cy = tileOf(nearP.y);
    for (int r = 0; r < 24; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) return g_sim.placeBuilding(type, owner, cx + dx, cy + dy, true);
    }
    return NOREF;
}
static void scene23(u64 seed, bool commander = false) {
    Faction fac[2] = { F_CYBER, F_CLANKER }; bool ai[2] = { false, commander }; int diff[2] = { 1, 2 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_ai.init(seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 100000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
}
static std::vector<Entity*> ofType23(int type) {
    std::vector<Entity*> out;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.type == type) out.push_back(&e);
    return out;
}

// ---- the map: the canal joins the lake to the sea; bridges and relic structures stand where the map says, nobody's
static bool mapTest23() {
    VERIFY23(MAP_W == 384 && MAP_H == 384 && ISLE_W == 240, "the map is not a 240-tile island in a 384 sea");
    VERIFY23(g_map.lakeComp == g_map.oceanComp, "the canal does not join the lake to the ocean");
    int fresh = 0, sea = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) { if (g_map.freshWater(x, y)) fresh++; else if (g_map.openSea(x, y)) sea++; }
    VERIFY23(fresh > 3000 && sea > 6 * fresh, "the lake and canal are not a small fresh-water part of a big sea");
    // the canal is open water from coast to coast: a ship can sail from the west sea to the east sea through it
    for (int x = ISLE_X0; x < ISLE_X0 + ISLE_W; x++) {
        bool open = false;
        for (int y = CANAL_Y0 - 18; y <= CANAL_Y1 + 18 && !open; y++) if (g_map.navTile(x, y)) open = true;
        VERIFY23(open, "the canal is blocked somewhere between the two coasts");
    }
    auto bridges = ofType23(B_BRIDGE);
    VERIFY23(bridges.size() == g_map.bridgeSpots.size() && bridges.size() >= 6, "the bridges over the canal are not all there");
    for (auto* b : bridges) {
        VERIFY23(b->owner < 0 && b->constructed, "a bridge starts owned or down");
        int deck = 0;
        for (int j = 0; j < BRIDGE_L; j++) if (g_map.tile(b->tx + 1, b->ty + j) == T_WATER) { deck++; VERIFY23(g_map.passable(b->tx + 1, b->ty + j), "a standing bridge's deck is not passable"); }
        VERIFY23(deck >= 4, "a bridge does not span water");
    }
    int relics = 0, secret = 0, commands = 0, silos = 0;
    for (auto& e : g_sim.ents) if (e.alive && Sim::relicEnt(e)) {
        relics++; if (e.secret) secret++;
        if (e.type == B_R_COMMAND) commands++;
        if (e.type == B_R_SILO) silos++;
        VERIFY23(e.owner < 0 && e.constructed, "a relic structure starts owned or unfinished");
        for (int y = e.ty; y < e.ty + e.bt().h; y++) for (int x = e.tx; x < e.tx + e.bt().w; x++) VERIFY23(g_map.tile(x, y) != T_WATER, "a relic structure stands in the water");
    }
    VERIFY23(relics == (int)g_map.relics.size() && relics >= 30, "the relic bases are not all there");
    VERIFY23(commands >= 4 && silos >= 2 && secret >= 2, "the relic bases lack Command structures, silos or the secret islands' structures");
    // ground units cross the canal by its bridges
    Vec2 n = g_map.nearestFree(tileCenter(ISLE_X0 + 40, CANAL_Y0 - 14), 10), s = g_map.nearestFree(tileCenter(ISLE_X0 + 40, CANAL_Y1 + 14), 10);
    std::vector<Vec2> path;
    VERIFY23(g_map.findPath(n, s, path) && !path.empty(), "no ground route across the canal");
    printf("relictest: map ok (%d fresh-water tiles, %d of open sea, %d bridges, %d relic structures, %d secret)\n", fresh, sea, (int)bridges.size(), relics, secret);
    return true;
}

// ---- a bridge comes down under fire (whoever stands on it is lost) and a dozer of either army raises it again
static bool bridgeTest23(u64 seed) {
    scene23(seed);
    auto bridges = ofType23(B_BRIDGE);
    VERIFY23(!bridges.empty(), "no bridge");
    Entity* B = bridges[0];
    Ref br = g_sim.refOf(*B);
    int dx = B->tx + 1, dy = B->ty + BRIDGE_L / 2;
    VERIFY23(g_map.tile(dx, dy) == T_WATER && g_map.passable(dx, dy), "the deck's middle is not open");
    Ref tank = g_sim.spawnUnit(U_K_TANK, 1, tileCenter(dx, dy));
    VERIFY23(alive23(tank), "no tank on the deck");
    // nobody's bridge: a unit's fire does not wander onto it unordered
    Ref shooter = g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(tileCenter(B->tx + 1, B->ty - 4), 8));
    VERIFY23(!g_sim.canTarget(*g_sim.get(shooter), *B), "a bridge is fair game without an order");
    for (int k = 0; k < 6 && g_sim.get(br)->constructed; k++) { g_sim.applyDamagePublic(*g_sim.get(br), g_sim.get(br)->maxHp * 0.6f, 0, shooter); run23(0.1f); }
    B = g_sim.get(br);
    VERIFY23(B && !B->constructed, "a bridge did not come down under fire");
    VERIFY23(!g_map.passable(dx, dy), "a broken bridge's deck is still passable");
    VERIFY23(!alive23(tank), "a tank on the deck did not go down with it");
    float hp = B->hp; g_sim.applyDamagePublic(*B, 5000, 0, shooter);
    VERIFY23(g_sim.get(br) && g_sim.get(br)->hp == hp, "a wrecked bridge still takes damage (it should wait for a dozer)");
    if (Entity* sh = g_sim.get(shooter)) g_sim.destroy(*sh, false);   // (it would shoot the other army's dozer)
    // the other army's dozer raises it: a bridge is nobody's
    Ref dz = g_sim.spawnUnit(firstUnitOf(F_CLANKER), 1, g_map.nearestFree(tileCenter(B->tx + 1, B->ty - 2), 8));
    VERIFY23(alive23(dz) && g_sim.get(dz)->ut().role == UR_DOZER, "no dozer");
    run23(2.0f);   // (it is lastDamaged just now)
    g_sim.cmdAssist({dz}, br);
    bool up = false;
    for (int t = 0; t < 90 * SIM_HZ && !up; t++) { g_sim.step(); g_sim.events.clear(); up = g_sim.get(br) && g_sim.get(br)->constructed;
        if (getenv("ONEHOUR_DEBUG") && t % 100 == 0) { Entity* d = g_sim.get(dz); Entity* b = g_sim.get(br); fprintf(stderr, "t=%d dozer %s order %d tgt %d pos %.0f,%.0f dist %.0f bridge prog %.2f hp %.0f\n", t, d ? "ok" : "dead", d ? d->order : -1, d ? (d->targetEnt == br) : -1, d ? d->pos.x / TILE : 0, d ? d->pos.y / TILE : 0, d && b ? g_sim.distToEntity(d->pos, *b) / TILE : -1, b ? b->progress : -1, b ? b->hp : -1); } }
    VERIFY23(up, "a dozer did not raise the broken bridge again within 90 s");
    VERIFY23(g_map.passable(dx, dy), "a raised bridge's deck is not passable");
    VERIFY23(g_sim.get(br)->owner < 0, "a raised bridge became the dozer's army's");
    printf("relictest: bridge ok (down under fire, the tank on it lost, raised again by a dozer)\n");
    return true;
}

// ---- relic structures: claimed one at a time by infantry, then the army's: armour, the Command's stipend and relic building, no selling, the Hydra
static bool relicTest23(u64 seed) {
    scene23(seed);
    Entity* cmd = nullptr;
    for (auto* e : ofType23(B_R_COMMAND)) if (!e->secret && g_map.landComp[e->ty * MAP_W + e->tx] == g_map.landComp[tileOf(g_sim.players[0].basePos.y) * MAP_W + tileOf(g_sim.players[0].basePos.x)]) { cmd = e; break; }
    VERIFY23(cmd, "no relic Command on the main island's landmass");
    Ref cr = g_sim.refOf(*cmd);
    VERIFY23(!g_sim.relicBuildable(0) && !g_sim.buildAvailable(0, B_R_FLAK), "relic building is open before any Relic Command is held");
    // damage: far tougher than an army's own structures
    float h0 = cmd->hp; g_sim.applyDamagePublic(*cmd, 1000, 0, NOREF);
    float took = h0 - g_sim.get(cr)->hp;
    VERIFY23(took > 0 && took < 1000 * 0.6f, "a relic structure is not armoured");
    g_sim.get(cr)->hp = h0;
    // a neighbouring relic structure stays nobody's when the Command is claimed: one by one
    Entity* other = nullptr; float od = 1e30f;
    for (auto& e : g_sim.ents) if (e.alive && Sim::relicEnt(e) && &e != cmd) { float d = dist(e.pos, cmd->pos); if (d < od) { od = d; other = &e; } }
    VERIFY23(other && od < 30.0f * TILE, "the relic Command has no relic neighbours");
    Ref orf = g_sim.refOf(*other);
    std::vector<Ref> squad;
    for (int k = 0; k < 3; k++) squad.push_back(g_sim.spawnUnit(U_C_INF1, 0, g_map.nearestFree(cmd->pos + Vec2(0, (cmd->bt().h * 0.5f + 2.0f) * TILE + k * 20.0f), 12)));
    VERIFY23(g_sim.relicClaimable(*g_sim.get(squad[0]), *cmd), "a rifleman may not claim a relic structure nobody holds");
    g_sim.cmdCapture(squad, cr);
    bool ours = false;
    for (int t = 0; t < 120 * SIM_HZ && !ours; t++) { g_sim.step(); g_sim.events.clear(); ours = g_sim.get(cr) && g_sim.get(cr)->owner == 0; }
    VERIFY23(ours, "infantry did not claim the relic Command within 120 s");
    VERIFY23(g_sim.get(orf) && g_sim.get(orf)->owner < 0, "claiming one relic structure handed over its neighbour too (they go one by one)");
    VERIFY23(g_sim.relicBuildable(0) && g_sim.buildAvailable(0, B_R_FLAK), "holding a Relic Command does not let the army's dozers raise relic structures");
    // its stipend
    int m0 = g_sim.players[0].money; run23(RELIC_INCOME_INTERVAL * 2 + 0.5f);
    VERIFY23(g_sim.players[0].money >= m0 + RELIC_COMMAND_INCOME, "the Relic Command pays nothing");
    // it cannot be sold
    g_sim.cmdSell(cr); run23(3.0f);
    VERIFY23(g_sim.get(cr) && g_sim.get(cr)->owner == 0, "a relic structure was sold");
    // the Hydra: one silo, five warheads
    Entity* silo = ofType23(B_R_SILO).empty() ? nullptr : ofType23(B_R_SILO)[0];
    VERIFY23(silo, "no Hydra Silo");
    silo->owner = 0; silo->siloTimer = 0;
    Vec2 aim = g_map.nearestWater(Vec2(WORLD_W * 0.5f, 12.0f * TILE), 10, true, Map::OPEN_SEA);   // (out at sea: nothing to hit)
    size_t n0 = g_sim.nukes.size();
    VERIFY23(g_sim.relicNukesReady(0) >= 1 && g_sim.cmdRelicNuke(0, aim), "a held Hydra Silo did not fire");
    int hydra = 0; for (size_t k = n0; k < g_sim.nukes.size(); k++) if (g_sim.nukes[k].hydra) hydra++;
    VERIFY23(hydra == RELIC_SILO_WARHEADS, "the Hydra did not split into its warheads");
    VERIFY23(g_sim.relicNukesReady(0) == 0 && !g_sim.cmdRelicNuke(0, aim), "a Hydra Silo fired again without reloading");
    printf("relictest: relic base ok (armoured, claimed one by one, stipend, relic building, no selling, %d Hydra warheads)\n", hydra);
    return true;
}

// ---- the island launchers are heavily armoured now, but a long enough pounding destroys one for good
static bool launcherTest23(u64 seed) {
    scene23(seed);
    std::vector<Entity*> L;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE) L.push_back(&e);
    VERIFY23(!L.empty(), "no island launcher");
    Ref lr = g_sim.refOf(*L[0]);
    float h0 = L[0]->hp; g_sim.applyDamagePublic(*L[0], 1000, 0, NOREF);
    VERIFY23(g_sim.get(lr) && h0 - g_sim.get(lr)->hp > 0 && h0 - g_sim.get(lr)->hp < 600, "an island launcher is not armoured, or takes no damage at all");
    int hits = 0;
    while (alive23(lr) && hits < 60) { g_sim.applyDamagePublic(*g_sim.get(lr), 4000, 0, NOREF); run23(0.1f); hits++; }
    VERIFY23(!alive23(lr), "an island launcher could not be destroyed");
    printf("relictest: island launcher ok (destroyed after %d heavy hits)\n", hits);
    return true;
}

// ---- submarines: dived, nothing finds or hurts them (not even another submarine); a launch brings one up and leaves it exposed; at most SUB_MAX
static bool subTest23(u64 seed) {
    scene23(seed);
    Ref tech = place23(B_C_TECH, 0, g_sim.players[0].basePos + Vec2(0, 5 * TILE));
    VERIFY23(alive23(tech), "no room for a tech structure");
    Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 30) * TILE, WORLD_H * 0.30f), 12, true, Map::OPEN_SEA);
    VERIFY23(g_map.openSeaAt(sea), "no open sea for the scene");
    Ref sub = g_sim.spawnUnit(U_C_SUB, 0, sea);
    Ref foe = g_sim.spawnUnit(U_K_DESTROYER, 1, g_map.nearestWater(sea + Vec2(4 * TILE, 0), 6, true));
    Ref foeSub = g_sim.spawnUnit(U_K_SUB, 1, g_map.nearestWater(sea + Vec2(-4 * TILE, 2 * TILE), 6, true));
    VERIFY23(alive23(sub) && alive23(foe) && alive23(foeSub), "the scene's ships did not spawn");
    run23(1.0f);
    VERIFY23(g_sim.visibleTo(*g_sim.get(sub), 1), "a surfaced submarine next to an enemy destroyer is unseen");
    g_sim.cmdDive({sub}, true);
    run23(SUB_DIVE_TIME + 1.0f);
    Entity* S = g_sim.get(sub);
    VERIFY23(S && Sim::submerged(*S), "the submarine did not dive");
    VERIFY23(!g_sim.visibleTo(*S, 1), "an enemy saw a dived submarine");
    VERIFY23(!g_sim.canTarget(*g_sim.get(foe), *S) && !g_sim.canTarget(*g_sim.get(foeSub), *S), "a dived submarine can be targeted (by a destroyer or another submarine)");
    float hp = S->hp; g_sim.applyDamagePublic(*S, 3000, 1, foe);
    VERIFY23(g_sim.get(sub)->hp == hp, "a dived submarine took damage");
    run23(20.0f);
    VERIFY23(alive23(sub) && g_sim.get(sub)->hp == hp && Sim::submerged(*g_sim.get(sub)), "a dived submarine was hurt or surfaced by itself");
    // it may stay down as long as it likes; a launch brings it up, and up it is out in the open
    g_sim.get(sub)->siloTimer = g_sim.time;
    Vec2 aim = g_map.nearestWater(Vec2(WORLD_W * 0.5f, WORLD_H - 12.0f * TILE), 10, true, Map::OPEN_SEA);
    size_t n0 = g_sim.nukes.size();
    VERIFY23(g_sim.subNukesReady(0) == 1 && g_sim.cmdSubNuke(0, aim, false, sub), "the submarine refused a launch order");
    VERIFY23(Sim::submerged(*g_sim.get(sub)) || g_sim.get(sub)->subState == 3, "the submarine did not start to surface for the launch");
    run23(SUB_SURFACE_TIME + 1.0f);
    S = g_sim.get(sub);
    VERIFY23(S && S->subState == 0 && g_sim.nukes.size() == n0 + 1, "the submarine did not surface and launch");
    VERIFY23(g_sim.visibleTo(*S, 1) && g_sim.canTarget(*g_sim.get(foe), *S), "a submarine that just launched is not exposed");
    g_sim.cmdDive({sub}, true);
    run23(1.0f);
    VERIFY23(g_sim.get(sub) && g_sim.get(sub)->subState == 0, "a submarine dived straight after its launch (it must stay up while exposed)");
    run23(SUB_EXPOSED + SUB_DIVE_TIME + 1.0f);
    VERIFY23(g_sim.get(sub) && Sim::submerged(*g_sim.get(sub)), "the submarine did not go down again after its launch");
    // the cap: SUB_MAX per army, the yards' queues included
    VERIFY23(g_sim.unitAvailable(0, U_C_SUB), "a submarine is not available with a tech structure");
    int have = g_sim.subsOf(0);
    for (int k = have; k < SUB_MAX; k++) g_sim.spawnUnit(U_C_SUB, 0, g_map.nearestWater(sea + Vec2((k % 6) * 3.0f * TILE, (k / 6) * 3.0f * TILE - 20 * TILE), 10, true));
    VERIFY23(g_sim.subsOf(0) == SUB_MAX && !g_sim.unitAvailable(0, U_C_SUB), "an army can have more than SUB_MAX submarines");
    printf("relictest: submarines ok (dived: unseen and untouchable; launch: up, exposed, down again; cap %d)\n", SUB_MAX);
    return true;
}

// ---- warships carry their own drone swarms: a battleship's is the biggest, a submarine has none, the Guard Drones upgrade grows them
static bool swarmTest23(u64 seed) {
    scene23(seed);
    Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 + ISLE_W + 30) * TILE, WORLD_H * 0.25f), 12, true, Map::OPEN_SEA);
    Ref bs = g_sim.spawnUnit(U_C_BATTLESHIP, 0, sea), ds = g_sim.spawnUnit(U_C_DESTROYER, 0, g_map.nearestWater(sea + Vec2(0, 8 * TILE), 8, true));
    Ref sb = g_sim.spawnUnit(U_C_SUB, 0, g_map.nearestWater(sea + Vec2(0, -8 * TILE), 8, true));
    run23(0.5f);
    int b0 = g_sim.droneCount(*g_sim.get(bs)), d0 = g_sim.droneCount(*g_sim.get(ds)), s0 = g_sim.droneCount(*g_sim.get(sb));
    VERIFY23(b0 > d0 && d0 > 0 && s0 == 0, "the swarms are not sized by ship (battleship > destroyer > 0, submarine none)");
    g_sim.players[0].upg[UPG_DRONES] = true;
    int b1 = g_sim.droneCount(*g_sim.get(bs));
    VERIFY23(b1 > b0 && b1 <= SWARM_MAX, "the Guard Drones upgrade did not grow a battleship's swarm");
    // the swarm stings an enemy aircraft that comes close
    Ref heli = g_sim.spawnUnit(U_K_AIR, 1, g_sim.get(bs)->pos + Vec2(2 * TILE, 0));
    VERIFY23(alive23(heli), "no enemy aircraft for the swarm");
    g_sim.get(heli)->maxHp = g_sim.get(heli)->hp = 1e6f; g_sim.get(heli)->disabledUntil = 1e9f;
    run23(3.0f);
    VERIFY23(g_sim.get(heli) && g_sim.get(heli)->hp < 1e6f, "a battleship's swarm did not sting an enemy aircraft beside it");
    printf("relictest: swarms ok (battleship %d drones, %d with the upgrade; destroyer %d; submarine %d)\n", b0, b1, d0, s0);
    return true;
}

// ---- surrender: a computer army with nothing left lays down its arms; one that still has a dozer fights on
static bool surrenderTest23(u64 seed) {
    for (int keepDozer = 0; keepDozer < 2; keepDozer++) {
        scene23(seed + keepDozer, true);
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE) g_sim.destroy(e, false);
        Vec2 at = g_map.nearestFree(g_sim.players[1].basePos, 10);
        std::vector<Ref> left;
        for (int k = 0; k < 3; k++) left.push_back(g_sim.spawnUnit(U_K_INF1, 1, at + Vec2(k * 24.0f, 0)));
        if (keepDozer) left.push_back(g_sim.spawnUnit(firstUnitOf(F_CLANKER), 1, at + Vec2(0, 30.0f)));
        g_sim.players[1].money = 0;
        run23(SURRENDER_GRACE * 0.5f, true);
        VERIFY23(g_sim.players[1].alive, "an army surrendered before its grace ran out");
        run23(SURRENDER_GRACE * 0.5f + 8.0f, true);
        int still = 0; for (Ref r : left) if (alive23(r)) still++;
        if (keepDozer) VERIFY23(g_sim.players[1].alive && still > 0, "an army that still had a dozer surrendered");
        else VERIFY23(!g_sim.players[1].alive && still == 0, "a computer army with nothing left did not surrender");
    }
    printf("relictest: surrender ok (nothing left: down in %.0f s; a dozer left: fights on)\n", SURRENDER_GRACE);
    return true;
}

bool relicTest(u64 seed) {
    scene23(seed);
    if (!mapTest23()) return false;
    if (!bridgeTest23(seed)) return false;
    if (!relicTest23(seed + 1)) return false;
    if (!launcherTest23(seed + 2)) return false;
    if (!subTest23(seed + 3)) return false;
    if (!swarmTest23(seed + 4)) return false;
    if (!surrenderTest23(seed + 5)) return false;
    printf("relictest: ok\n");
    return true;
}
