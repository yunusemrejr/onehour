// One Hour - entry point
#include "ui.h"
#include "brain.h"
#include "artb.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>
static const int INFO_X_TEST = 134;

static void usage() {
    printf("One Hour - compact Zero Hour style skirmish RTS\n"
           "  --scale N        UI scale factor (default: automatic); the window is resizable, F11 toggles fullscreen\n"
           "  --size WxH       initial window size in pixels (default: most of the desktop)\n"
           "  --software       use the software renderer\n"
           "  --selftest [T]   run an AI-only game headless for T seconds of sim time and report\n"
           "  --shot FILE      render a frame to FILE (BMP) after --ticks and exit (headless)\n"
           "  --ticks N        sim ticks to run before --shot (default 0 = main menu)\n"
           "  --train [N]      self-play N games to train the AI brain (saved to ~/.local/share/onehour/brain.txt)\n"
           "  --eval [N]       learned AI vs plain heuristic AI, N games (uses --d0 as difficulty)\n"
           "  --seed S         random seed for the game\n"
           "  --scenario, --econtest, --areatest, --hqtest, --jettest, --braintest, --uitest   headless gameplay tests\n"
           "  --bench [N]      time N rendered frames of a busy battle (software renderer)\n"
           "  --faction c|k    your faction for --shot/--selftest\n");
}

static bool selfTest(int seconds, u64 seed, int players, int d0, bool swap) {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER };
    if (swap) { fac[0] = F_CLANKER; fac[1] = F_CYBER; }
    if (getenv("ONEHOUR_MIRROR")) { Faction m = (Faction)atoi(getenv("ONEHOUR_MIRROR")); fac[0] = fac[1] = m; }
    bool ai[4] = { true, true, true, true };
    int diff[4] = { d0, 3, 2, 1 };
    int team[4] = { 0, 1, 2, 3 };
    g_sim.init(players, fac, ai, diff, team, seed);
    g_ai.init(seed);
    int ticks = seconds * SIM_HZ;
    auto t0 = std::chrono::steady_clock::now();
    int maxEnts = 0;
    for (int t = 0; t < ticks && !g_sim.gameOver; t++) {
        g_sim.step();
        g_ai.update();
        g_sim.events.clear();
        int alive = 0;
        for (auto& e : g_sim.ents) {
            if (!e.alive) continue;
            alive++;
            if (!(e.pos.x == e.pos.x) || !(e.pos.y == e.pos.y)) { fprintf(stderr, "NaN position at tick %d\n", t); return false; }
            if (e.pos.x < 0 || e.pos.y < 0 || e.pos.x > WORLD_W || e.pos.y > WORLD_H) { fprintf(stderr, "entity out of world at tick %d\n", t); return false; }
            if (e.isUnit() && !e.isAir() && !g_map.terrainPassable(tileOf(e.pos.x), tileOf(e.pos.y))) { fprintf(stderr, "ground unit on impassable terrain at tick %d (%s)\n", t, e.ut().name); return false; }
            if (e.hp > e.maxHp + 0.01f) { fprintf(stderr, "hp above max at tick %d\n", t); return false; }
        }
        maxEnts = std::max(maxEnts, alive);
        if (t % (60 * SIM_HZ) == 0) {
            printf("t=%4ds", t / SIM_HZ);
            for (int p = 0; p < g_sim.numPlayers; p++) {
                Player& pl = g_sim.players[p];
                printf(" | P%d %s $%5d u%3d b%2d pw%2d/%2d%s", p, pl.faction == F_CYBER ? "CYB" : "CLK", pl.money, g_sim.countUnits(p), g_sim.countBuildings(p, -1, false), pl.powerUsed, pl.powerMade, pl.alive ? "" : " DEAD");
            }
            printf("\n");
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("selftest: %d ticks in %.0f ms (%.3f ms/tick), peak entities %d, gameOver=%d winnerTeam=%d\n", ticks, ms, ms / std::max(1, ticks), maxEnts, (int)g_sim.gameOver, g_sim.winnerTeam);
    for (int p = 0; p < g_sim.numPlayers; p++) {
        Player& pl = g_sim.players[p];
        int ubase = firstUnitOf(pl.faction);
        printf("  P%d: mined $%d, income structures %d, nuke ramps %d\n", p, pl.mined, g_sim.countRole(p, BR_INCOME, false), g_sim.countRole(p, BR_NUKE, false));
        printf("  P%d: built %d lost %d kills %d structures killed %d harvested %d alive=%d | program %d, elites bought %d/%d, titans %d, drones/gunships %d, jets %d\n", p, pl.unitsBuilt, pl.unitsLost, pl.unitsKilled, pl.structuresKilled, pl.harvested, (int)pl.alive,
               (int)pl.advTech, (int)(pl.spentOn[ubase + 9] / UNITS[ubase + 9].cost), (int)(pl.spentOn[ubase + 9] > 0), (int)(pl.spentOn[ubase + 10] / UNITS[ubase + 10].cost), (int)(pl.spentOn[ubase + 8] / UNITS[ubase + 8].cost), (int)(pl.spentOn[ubase + 11] / UNITS[ubase + 11].cost));
        if (getenv("ONEHOUR_DEBUG")) {
            printf("      value dealt per credit:");
            for (int u = 0; u < U_COUNT; u++) if (pl.spentOn[u] > 0) printf(" %s %.2f (spent %d)", UNITS[u].name, pl.valueDealt[u] / pl.spentOn[u], (int)pl.spentOn[u]);
            printf("\n");
        }
    }
    return true;
}


// Plays one AI-only game headless. Returns the winning team (or -1 when nobody won within maxSecs).
static int playAiGame(u64 seed, int players, const Faction* fac, const bool* brain, int difficulty, int maxSecs, const int* teamIn = nullptr) {
    bool ai[4] = { true, true, true, true };
    int diff[4] = { difficulty, difficulty, difficulty, difficulty };
    int team[4] = { 0, 1, 2, 3 };
    if (teamIn) for (int i = 0; i < 4; i++) team[i] = teamIn[i];
    g_sim.init(players, fac, ai, diff, team, seed);
    for (int p = 0; p < 4; p++) g_ai.brainEnabled[p] = brain[p];
    g_ai.init(seed);
    for (int t = 0; t < maxSecs * SIM_HZ && !g_sim.gameOver; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
    return g_sim.gameOver ? g_sim.winnerTeam : -1;
}

// Self-play training: every AI shares and updates the brain; the result is saved for later games.
static int trainBrain(int games, u64 seed) {
    std::string path = Brain::defaultPath();
    if (!getenv("ONEHOUR_FRESH") && !path.empty() && g_brain.load(path.c_str())) printf("continuing from %s (%d games, %d waves)\n", path.c_str(), g_brain.games, g_brain.waveSamples);
    g_brain.learning = true;
    Rng r(seed);
    for (int i = 0; i < games; i++) {
        int n = r.range(2, 4);
        Faction fac[4]; for (int k = 0; k < 4; k++) fac[k] = (Faction)r.range(0, 1);
        bool brain[4] = { true, true, true, true };
        int w = playAiGame(seed * 1000 + i, n, fac, brain, r.range(1, 3), 1500);
        g_brain.games++;
        if ((i + 1) % 10 == 0 || i + 1 == games) {
            printf("game %3d players %d winner team %2d | waves learned %d | wave weights", i + 1, n, w, g_brain.waveSamples);
            for (int k = 0; k < WAVE_F; k++) printf(" %.2f", g_brain.ww[k]);
            printf("\n");
            if (!path.empty()) g_brain.save(path.c_str());
        }
    }
    if (!path.empty()) printf("brain saved to %s\n", path.c_str());
    return 0;
}

// Evaluation: the learned AI (player 0) against the plain heuristic AI (player 1), sides and factions rotated.
static int evalBrain(int games, u64 seed, int difficulty) {
    std::string path = Brain::defaultPath();
    if (!path.empty() && g_brain.load(path.c_str())) printf("using %s (%d games, %d waves)\n", path.c_str(), g_brain.games, g_brain.waveSamples);
    else printf("no saved brain: evaluating the built-in prior\n");
    g_brain.learning = false;
    int wins = 0, losses = 0, draws = 0;
    for (int i = 0; i < games; i++) {
        Faction fac[4] = { (Faction)(i & 1), (Faction)((i >> 1) & 1), F_CYBER, F_CLANKER };
        bool brainOn[4]; bool flip = (i >> 2) & 1;     // swap which start corner the learned AI gets
        int teamB = flip ? 1 : 0;
        brainOn[0] = !flip; brainOn[1] = flip; brainOn[2] = brainOn[3] = false;
        (void)teamB;
        int w = playAiGame(seed + i * 31, 2, fac, brainOn, difficulty, 1500);
        int brainTeam = flip ? 1 : 0;
        if (w < 0) {
            // no winner: compare remaining army + structure value
            float v[2] = {0, 0};
            for (auto& e : g_sim.ents) if (e.alive && e.owner >= 0 && e.owner < 2 && e.kind != EK_RESOURCE) v[e.owner] += e.isUnit() ? UNITS[e.type].cost : BUILDS[e.type].cost;
            draws++; (void)v;
        } else if (w == brainTeam) wins++; else losses++;
    }
    printf("brain vs baseline: %d wins, %d losses, %d unresolved (of %d)\n", wins, losses, draws, games);
    return 0;
}

// Drives the player-facing commands for both factions: every structure, every unit, sell, powers, orders.
static bool scenarioTest(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        g_ai.init(seed);
        Player& pl = g_sim.players[0];
        pl.money = 200000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        Ref dozer;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) dozer = g_sim.refOf(e);
        int bbase = firstBuildOf(fac[0]), ubase = firstUnitOf(fac[0]);
        // place every structure in a ring around the base, in prerequisite order, and let one dozer build them
        int order[10] = { BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_TURRET, BR_AATURRET, BR_TECH, BR_AIRFIELD, BR_INCOME, BR_NUKE };
        std::vector<Ref> sites;
        for (int k = 0; k < 10; k++) {
            int type = bbase + order[k];
            bool placed = false;
            for (int ring = (k >= 8 ? 6 : 3); ring < 24 && !placed; ring++) for (int dy = -ring; dy <= ring && !placed; dy++) for (int dx = -ring; dx <= ring && !placed; dx++) {
                if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
                int tx = tileOf(pl.basePos.x) + dx, ty = tileOf(pl.basePos.y) + dy;
                if (!g_sim.canPlace(0, type, tx, ty)) continue;
                // must be buildable regardless of prerequisites at this point: run the sim until prereqs exist
                for (int guard = 0; guard < 20 * 120 && !g_sim.buildAvailable(0, type); guard++) { g_sim.step(); g_sim.events.clear(); }
                if (!g_sim.buildAvailable(0, type)) { fprintf(stderr, "scenario: %s never became available\n", BUILDS[type].name); return false; }
                if (!g_sim.canPlace(0, type, tx, ty)) continue;
                if (Entity* dd = g_sim.get(dozer)) { dd->pos = dd->prevPos = g_map.nearestFree(g_sim.buildingCenter(type, tx, ty) + Vec2(0, BUILDS[type].h * TILE * 0.5f + 20), 12); dd->path.clear(); }   // earlier ring structures may have boxed the dozer in
                if (!g_sim.cmdBuild(dozer, type, tx, ty)) { fprintf(stderr, "scenario: cmdBuild failed for %s\n", BUILDS[type].name); return false; }
                placed = true;
                // wait for construction to finish
                for (int guard = 0; guard < 20 * 200; guard++) {
                    g_sim.step(); g_sim.events.clear();
                    Entity* d = g_sim.get(dozer);
                    if (!d) { fprintf(stderr, "scenario: dozer died\n"); return false; }
                    if (d->order == O_IDLE) break;
                }
                if (!g_sim.hasBuilding(0, type)) { Entity* dd = g_sim.get(dozer); fprintf(stderr, "scenario: %s not completed (dozer order %d, sites:", BUILDS[type].name, dd ? (int)dd->order : -1); for (auto& q : g_sim.ents) if (q.alive && q.owner == 0 && q.isBuilding() && !q.constructed) fprintf(stderr, " %s %.2f", q.bt().name, q.progress); fprintf(stderr, ") dozer %.0f,%.0f  site tile %d,%d\n", dd ? dd->pos.x : 0, dd ? dd->pos.y : 0, tx, ty); if (getenv("ONEHOUR_DEBUG")) for (int yy = 0; yy < 24; yy++) { for (int xx = 0; xx < 24; xx++) fputc(!g_map.passable(xx, yy) ? '#' : (dd && tileOf(dd->pos.x) == xx && tileOf(dd->pos.y) == yy ? 'D' : '.'), stderr); fputc('\n', stderr); } return false; }
            }
            if (!placed) { fprintf(stderr, "scenario: no spot for %s\n", BUILDS[type].name); return false; }
        }
        // tech structure powers: the special units are locked until the Advanced Program is researched, the scan lifts the shroud
        {
            int elite = ubase + 9;
            Ref barracks; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.constructed && e.bt().role == BR_BARRACKS) barracks = g_sim.refOf(e);
            if (g_sim.cmdTrain(barracks, elite)) { fprintf(stderr, "scenario: special unit trainable before the program\n"); return false; }
            int m = pl.money;
            if (!g_sim.cmdResearch(0)) { fprintf(stderr, "scenario: research refused\n"); return false; }
            if (pl.money != m - PROGRAMS[fac[0]].cost || g_sim.cmdResearch(0)) { fprintf(stderr, "scenario: research cost / double start wrong\n"); return false; }
            for (int guard = 0; guard < 20 * 120 && !pl.advTech; guard++) { g_sim.step(); g_sim.events.clear(); }
            if (!pl.advTech) { fprintf(stderr, "scenario: research never finished\n"); return false; }
            pl.scanReady = 0;
            if (!g_sim.cmdScan(0) || !g_sim.revealed(0)) { fprintf(stderr, "scenario: scan failed\n"); return false; }
            if (g_sim.cmdScan(0)) { fprintf(stderr, "scenario: scan ignored its cooldown\n"); return false; }
            float until = pl.revealUntil;
            if (until - g_sim.time < 29.0f || until - g_sim.time > 31.0f) { fprintf(stderr, "scenario: scan should last 30s (%.1f)\n", until - g_sim.time); return false; }
            for (int guard = 0; guard < 20 * 31; guard++) { g_sim.step(); g_sim.events.clear(); }
            if (g_sim.revealed(0)) { fprintf(stderr, "scenario: scan did not expire\n"); return false; }
        }
        // train every unit from its producer
        for (int u = 0; u < UNITS_PER_FACTION; u++) {
            int type = ubase + u;
            Ref prod;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.constructed && e.bt().role == UNITS[type].builtBy) { prod = g_sim.refOf(e); break; }
            if (!prod.valid()) { fprintf(stderr, "scenario: no producer for %s\n", UNITS[type].name); return false; }
            if (!g_sim.cmdTrain(prod, type)) { fprintf(stderr, "scenario: cmdTrain failed for %s\n", UNITS[type].name); return false; }
        }
        int before = g_sim.countUnits(0);
        for (int guard = 0; guard < 20 * 300; guard++) { g_sim.step(); g_sim.events.clear(); if (g_sim.countUnits(0) >= before + UNITS_PER_FACTION) break; }
        if (g_sim.countUnits(0) < before + UNITS_PER_FACTION) { fprintf(stderr, "scenario: not all units produced (%d/%d)\n", g_sim.countUnits(0) - before, UNITS_PER_FACTION); return false; }
        // orders: attack-move the army at the enemy base, harvest, power, sell
        std::vector<Ref> army, harv;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) { if (e.ut().role == UR_COMBAT) army.push_back(g_sim.refOf(e)); if (e.ut().role == UR_HARVESTER) harv.push_back(g_sim.refOf(e)); }
        g_sim.cmdMove(army, g_sim.players[1].basePos, true);
        Ref pile; for (auto& e : g_sim.ents) if (e.alive && e.kind == EK_RESOURCE) { pile = g_sim.refOf(e); break; }
        g_sim.cmdHarvest(harv, pile);
        g_sim.players[0].powerReady = 0;
        if (!g_sim.cmdPower(0, g_sim.players[1].basePos)) { fprintf(stderr, "scenario: power failed\n"); return false; }
        for (int guard = 0; guard < 20 * 240; guard++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        g_sim.cmdStop(army);
        Ref sellMe; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_BARRACKS) sellMe = g_sim.refOf(e);
        if (sellMe.valid()) {
            int m = g_sim.players[0].money; g_sim.cmdSell(sellMe);
            if (g_sim.players[0].money <= m) { fprintf(stderr, "scenario: sell gave no refund\n"); return false; }
        } else printf("scenario: barracks already lost to the enemy before selling\n");
        for (int guard = 0; guard < 20 * 60; guard++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        printf("scenario %s: ok (units %d, structures %d, enemy structures %d, time %.0fs)\n", FACTION_NAME[fac[0]], g_sim.countUnits(0), g_sim.countBuildings(0, -1, false), g_sim.countBuildings(1, -1, false), g_sim.time);
    }
    return true;
}


// Area orders: fighters guard a circle, drones loiter over it, haulers work the supplies inside a circle.
static bool areaTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "areatest: %s\n", m); return false; };
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
    std::fill(g_sim.players[1].explored.begin(), g_sim.players[1].explored.end(), 1);
    Vec2 c = g_map.nearestFree(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 40);
    float R = 5 * TILE;
    std::vector<Ref> mine;
    mine.push_back(g_sim.spawnUnit(U_C_INF1, 0, c + Vec2(-90, 40)));
    mine.push_back(g_sim.spawnUnit(U_C_TANK, 0, c + Vec2(-120, 60)));
    mine.push_back(g_sim.spawnUnit(U_C_TANK, 0, c + Vec2(-120, 100)));
    Ref drone = g_sim.spawnUnit(U_C_AIR, 0, c + Vec2(-200, 0));
    mine.push_back(drone);
    Entity* dv = g_sim.get(drone); dv->ammo = dv->ut().ammo;
    { Vec2 b = g_sim.players[0].basePos; g_sim.placeBuilding(B_C_AIRFIELD, 0, tileOf(b.x) + 6, tileOf(b.y) - 2, true); }
    g_sim.cmdGuardArea(mine, c, R);
    for (auto r : mine) if (g_sim.get(r)->order != O_GUARDAREA) return fail("guard order not applied");
    for (int t = 0; t < 20 * 12; t++) { g_sim.step(); g_sim.events.clear(); }
    for (auto r : mine) { Entity* e = g_sim.get(r); if (!e) return fail("unit lost"); if (dist(e->pos, c) > R + 40) { fprintf(stderr, "%s at %.0f from centre\n", e->ut().name, dist(e->pos, c)); return fail("unit did not settle in the zone"); } }
    // an enemy walks into the zone: it must die, and the guards must be back in the zone afterwards
    Ref intruder = g_sim.spawnUnit(U_K_TANK, 1, c + Vec2(R + 60, 0));
    g_sim.cmdMove({intruder}, c + Vec2(-20, 20), false);
    for (int t = 0; t < 20 * 40 && g_sim.get(intruder); t++) { g_sim.step(); g_sim.events.clear(); }
    if (g_sim.get(intruder)) return fail("intruder was not destroyed by the guards");
    for (int t = 0; t < 20 * 10; t++) { g_sim.step(); g_sim.events.clear(); }
    for (auto r : mine) { Entity* e = g_sim.get(r); if (!e) return fail("guard lost"); if (e->order != O_GUARDAREA) { fprintf(stderr, "%s order %d post %d zoneR %.0f hp %.0f ammo %d\n", e->ut().name, (int)e->order, (int)e->postOrder, e->zoneR, e->hp, e->ammo); return fail("guard did not resume guarding"); } }
    // an enemy far outside the zone that never enters it: not chased
    Vec2 far = g_map.nearestFree(c + Vec2(0, -14 * TILE), 40);
    Ref idler = g_sim.spawnUnit(U_K_TANK, 1, far);
    float hp0 = g_sim.get(idler)->hp;
    for (int t = 0; t < 20 * 25; t++) { g_sim.step(); g_sim.events.clear(); }
    Entity* id = g_sim.get(idler);
    if (!id || id->hp < hp0) { if (id) { fprintf(stderr, "outsider hp %.0f/%.0f\n", id->hp, hp0); } return fail("guards attacked an enemy outside their zone"); }
    for (auto r : mine) { Entity* e = g_sim.get(r); if (e && !e->isAir() && dist(e->pos, c) > R + 40) return fail("guard wandered out of its zone"); }
    // a manual move cancels the assignment
    g_sim.cmdMove({mine[0]}, c, false);
    if (g_sim.get(mine[0])->zoneR != 0) return fail("move did not clear the zone");
    g_sim.destroy(*g_sim.get(idler), false);

    // haulers: a zone with a pile is worked and pays out; an empty zone is given up
    Entity* pile = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.kind == EK_RESOURCE) { pile = &e; break; }
    if (!pile) return fail("no supply pile on the map");
    Entity* hub = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_HQ) hub = &e;
    Vec2 hubPos = hub->pos;
    g_sim.placeBuilding(B_C_SUPPLY, 0, tileOf(pile->pos.x) - 5, tileOf(pile->pos.y) + 3, true);
    Ref hv = g_sim.spawnUnit(U_C_HARV, 0, pile->pos + Vec2(60, 60));
    int before = g_sim.players[0].harvested;
    g_sim.cmdGatherArea({hv}, pile->pos, 4 * TILE);
    if (g_sim.get(hv)->zoneR <= 0) return fail("gather zone not applied");
    for (int t = 0; t < 20 * 90; t++) { g_sim.step(); g_sim.events.clear(); }
    if (g_sim.players[0].harvested <= before) return fail("hauler collected nothing from the zone");
    Ref hv2 = g_sim.spawnUnit(U_C_HARV, 0, hubPos + Vec2(0, 120));
    Vec2 empty = g_map.nearestFree(Vec2(hubPos.x + 6 * TILE, hubPos.y + 10 * TILE), 20);
    bool anyPile = false; for (auto& e : g_sim.ents) if (e.alive && e.kind == EK_RESOURCE && dist(e.pos, empty) < 5 * TILE) anyPile = true;
    if (!anyPile) {
        g_sim.cmdGatherArea({hv2}, empty, 3 * TILE);
        for (int t = 0; t < 20 * 60 && g_sim.get(hv2)->zoneR > 0; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.get(hv2)->zoneR > 0) return fail("hauler never gave up an empty zone");
    }
    printf("areatest: ok\n");
    return true;
}

// Income structures, nuke ramps (cooldown, blast, several ramps) and aircraft rally points.
static bool econTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "econtest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        Player& pl = g_sim.players[0]; pl.money = 100000;
        int bb = firstBuildOf(fac[0]);
        Vec2 b0 = pl.basePos; Ref inc[5];
        auto spot = [&](int type, int skipDy) { for (int dy = skipDy; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } return Vec2(-1, -1); };
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 6, tileOf(b0.y) - 2, true);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 6, tileOf(b0.y) + 1, true);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 9, tileOf(b0.y) - 2, true);
        int placed = 0;
        for (int i = 0; i < 5; i++) { Vec2 sp = spot(bb + BR_INCOME, 5); if (sp.x < 0) break; inc[i] = g_sim.placeBuilding(bb + BR_INCOME, 0, (int)sp.x, (int)sp.y, true); placed++; if (!g_sim.buildAvailable(0, bb + BR_INCOME)) break; }
        if (placed != INCOME_MAX) return fail("income structure limit not enforced");
        int m0 = pl.money;
        for (int t = 0; t < 20 * 60; t++) { g_sim.step(); g_sim.events.clear(); }
        int gain = pl.money - m0, per = fac[0] == F_CYBER ? 75 : 60;
        int expect = INCOME_MAX * per * 12;
        if (gain < expect - 2 * per * INCOME_MAX || gain > expect + 2 * per * INCOME_MAX) { fprintf(stderr, "gained %d expected about %d\n", gain, expect); return fail("income structures pay the wrong amount"); }
        // nukes: two ramps, an enemy cluster
        Vec2 s1 = spot(bb + BR_NUKE, 8); Ref r1 = g_sim.placeBuilding(bb + BR_NUKE, 0, (int)s1.x, (int)s1.y, true);
        Vec2 s2 = spot(bb + BR_NUKE, 8); Ref r2 = g_sim.placeBuilding(bb + BR_NUKE, 0, (int)s2.x, (int)s2.y, true);
        g_sim.get(r1)->actionTimer = 0; g_sim.get(r2)->actionTimer = 0;
        pl.money = 100000;
        g_sim.updatePowerPublic();
        for (int i = 0; i < 3; i++) g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) - 8 - i * 4, tileOf(b0.y) + 14, true);
        g_sim.updatePowerPublic();
        Vec2 tgt = g_map.nearestFree(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 30);
        std::vector<Ref> victims;
        for (int i = 0; i < 6; i++) victims.push_back(g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, tgt + Vec2(i * 20 - 50, (i % 2) * 20)));
        Ref far = g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, g_map.nearestFree(tgt + Vec2(0, 12 * TILE), 30));
        if (g_sim.nukesReady(0) != 2) return fail("two ramps should both be ready");
        if (!g_sim.cmdNuke(0, tgt)) return fail("nuke launch refused");
        if (g_sim.nukesReady(0) != 1) return fail("a launch should use exactly one ramp");
        for (int t = 0; t < 20 * 9; t++) { g_sim.step(); g_sim.events.clear(); }
        for (auto v : victims) if (g_sim.get(v)) return fail("nuke left a unit alive in the blast");
        if (!g_sim.get(far)) return fail("nuke hit a unit far outside the blast radius");
        if (!g_sim.cmdNuke(0, tgt)) return fail("second ramp refused");
        if (g_sim.cmdNuke(0, tgt)) return fail("nuke fired with no ramp ready");
        for (int t = 0; t < 20 * 285 && !g_sim.gameOver; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.nukesReady(0) != 0) return fail("ramp reloaded before five minutes");
        for (int t = 0; t < 20 * 25; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.nukesReady(0) != 2) return fail("ramps did not reload after five minutes");
        // aircraft rally: a spawned aircraft flies to the airfield's rally point and waits there
        Vec2 sa = spot(bb + BR_AIRFIELD, 4); Ref af = g_sim.placeBuilding(bb + BR_AIRFIELD, 0, (int)sa.x, (int)sa.y, true);
        Vec2 rp = g_map.nearestFree(g_sim.get(af)->pos + Vec2(0, 9 * TILE), 30);
        g_sim.cmdSetRally(af, rp);
        pl.money = 100000;
        if (!g_sim.cmdTrain(af, firstUnitOf(fac[0]) + 8)) return fail("aircraft not trainable");
        Ref air;
        for (int t = 0; t < 20 * 40 && !air.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) air = g_sim.refOf(e); }
        if (!air.valid()) return fail("aircraft never produced");
        for (int t = 0; t < 20 * 15; t++) { g_sim.step(); g_sim.events.clear(); }
        Entity* a = g_sim.get(air);
        if (!a || dist(a->pos, rp) > 4 * TILE) return fail("aircraft did not go to the rally point");
        printf("econtest %s: ok\n", FACTION_NAME[fac[0]]);
    }
    return true;
}


// A dozer can found a new Command Core / Post when the old one falls: the base is rebuilt, it trains dozers again, three at most.
static bool hqTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "hqtest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        Player& pl = g_sim.players[0];
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int hqType = firstBuildOf(fac[0]) + BR_HQ, dozerType = firstUnitOf(fac[0]);
        Ref old, dozer;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0) { if (e.isBuilding() && e.type == hqType) old = g_sim.refOf(e); if (e.isUnit() && e.type == dozerType) dozer = g_sim.refOf(e); }
        if (!old.valid() || !dozer.valid()) return fail("no starting Command Core / dozer");
        if (g_sim.buildAvailable(0, hqType)) { /* a second one is allowed as an expansion, within the limit */ }
        Vec2 oldPos = g_sim.get(old)->pos;
        g_sim.destroy(*g_sim.get(old), true);                       // the base falls
        if (!g_sim.buildAvailable(0, hqType)) return fail("Command Core not offered to the dozer after the old one fell");
        if (g_sim.cmdTrain(old, dozerType)) return fail("trained a dozer from a destroyed Command Core");
        // pick a spot a few tiles from where it stood
        int tx = -1, ty = -1;
        for (int ring = 2; ring < 16 && tx < 0; ring++) for (int dy = -ring; dy <= ring && tx < 0; dy++) for (int dx = -ring; dx <= ring; dx++) {
            if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
            int x = tileOf(oldPos.x) - 2 + dx, y = tileOf(oldPos.y) - 2 + dy;
            if (g_sim.canPlace(0, hqType, x, y)) { tx = x; ty = y; break; }
        }
        if (tx < 0) return fail("no spot for the new Command Core");
        int cash = pl.money = 9000;
        if (!g_sim.cmdBuild(dozer, hqType, tx, ty)) return fail("dozer could not start a Command Core");
        if (pl.money != cash - BUILDS[hqType].cost) return fail("Command Core price not charged");
        for (int t = 0; t < 20 * 120 && !g_sim.hasRole(0, BR_HQ); t++) { g_sim.step(); g_sim.events.clear(); }
        if (!g_sim.hasRole(0, BR_HQ)) return fail("new Command Core never finished");
        Ref fresh; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.type == hqType && e.constructed) fresh = g_sim.refOf(e);
        if (dist(pl.basePos, g_sim.get(fresh)->pos) > 1.0f) return fail("home base did not move to the rebuilt Command Core");
        if (!g_sim.cmdTrain(fresh, dozerType)) return fail("rebuilt Command Core cannot train dozers");
        // the limit: HQ_MAX standing or rising at once
        pl.money = 100000;
        int built = g_sim.countRole(0, BR_HQ, false);
        for (int ring = 6; ring < 24 && built < HQ_MAX + 2; ring++) for (int dy = -ring; dy <= ring && built < HQ_MAX + 2; dy++) for (int dx = -ring; dx <= ring && built < HQ_MAX + 2; dx++) {
            if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
            int x = tileOf(pl.basePos.x) + dx, y = tileOf(pl.basePos.y) + dy;
            if (!g_sim.canPlace(0, hqType, x, y) || !g_sim.buildAvailable(0, hqType)) continue;
            if (!g_sim.cmdBuild(dozer, hqType, x, y)) continue;
            built = g_sim.countRole(0, BR_HQ, false);
        }
        if (g_sim.countRole(0, BR_HQ, false) != HQ_MAX) { fprintf(stderr, "%d Command Cores\n", g_sim.countRole(0, BR_HQ, false)); return fail("Command Core limit not enforced"); }
        if (g_sim.buildAvailable(0, hqType) || !g_sim.atBuildLimit(0, hqType)) return fail("limit should hide the Command Core from the build menu");
        printf("hqtest %s: ok\n", FACTION_NAME[fac[0]]);
    }
    // the computer opponent rebuilds a lost Command Core by itself: it banks the price and sends its dozer
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { true, true }; int diff[2] = { 2, 2 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 10 + fi);
        g_ai.init(seed);
        int hqType = firstBuildOf(fac[0]) + BR_HQ;
        for (int t = 0; t < 20 * 150; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        Ref old; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.type == hqType) old = g_sim.refOf(e);
        if (!old.valid()) return fail("AI has no Command Core to lose");
        g_sim.destroy(*g_sim.get(old), true);
        g_sim.players[0].money = std::max(g_sim.players[0].money, 4000);
        bool rebuilt = false;
        for (int t = 0; t < 20 * 240 && !rebuilt; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); rebuilt = g_sim.hasRole(0, BR_HQ); if (!g_sim.players[0].alive) break; }
        if (!rebuilt) return fail("the AI never rebuilt its lost Command Core");
        printf("hqtest AI %s: rebuilt its Command Core after %.0fs\n", FACTION_NAME[fac[0]], g_sim.time - 150.0f);
    }
    return true;
}

// Supersonic jets: built at the airfield, they fly fixed-wing strafing passes (finite turn rate, very fast), kill things, rearm and land.
static bool jetTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "jettest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        Player& pl = g_sim.players[0]; pl.money = 100000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int bb = firstBuildOf(fac[0]), jetType = firstUnitOf(fac[0]) + 11;
        if (!UNITS[jetType].jet || UNITS[jetType].kind != UK_AIR) return fail("unit table: the last unit of each army must be the jet");
        Vec2 b0 = pl.basePos;
        auto spot = [&](int type, int skipDy) { for (int dy = skipDy; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } return Vec2(-1, -1); };
        Vec2 sp = spot(bb + BR_POWER, 5); g_sim.placeBuilding(bb + BR_POWER, 0, (int)sp.x, (int)sp.y, true);
        Vec2 sf = spot(bb + BR_FACTORY, 5); g_sim.placeBuilding(bb + BR_FACTORY, 0, (int)sf.x, (int)sf.y, true);
        Vec2 st = spot(bb + BR_TECH, 5); Ref tech = g_sim.placeBuilding(bb + BR_TECH, 0, (int)st.x, (int)st.y, true); (void)tech;
        Vec2 sa = spot(bb + BR_AIRFIELD, 5); Ref af = g_sim.placeBuilding(bb + BR_AIRFIELD, 0, (int)sa.x, (int)sa.y, true);
        g_sim.updatePowerPublic();
        if (!g_sim.cmdTrain(af, jetType)) return fail("jet not trainable at the airfield with a tech structure standing");
        Ref jet;
        for (int t = 0; t < 20 * 40 && !jet.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.type == jetType) jet = g_sim.refOf(e); }
        if (!jet.valid()) return fail("jet never produced");
        for (int t = 0; t < 20 * 3; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.get(jet)->alt > 0.05f) return fail("an idle jet should sit on the pad");
        // targets: a column of enemy vehicles 20 tiles away
        Vec2 far = g_map.nearestFree(g_sim.get(af)->pos + Vec2(20 * TILE, 4 * TILE), 40);
        std::vector<Ref> vics;
        for (int i = 0; i < 4; i++) vics.push_back(g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, far + Vec2(i * 24, (i % 2) * 20)));
        float hp0 = 0; for (auto v : vics) hp0 += g_sim.get(v)->hp;
        g_sim.cmdAttack({jet}, vics[0]);
        float maxSpeed = 0, maxTurn = 0, prevAngle = g_sim.get(jet)->angle; Vec2 prev = g_sim.get(jet)->pos; int ammo0 = g_sim.get(jet)->ammo, minAmmo = ammo0; bool leftPad = false;
        for (int t = 0; t < 20 * 40; t++) {
            g_sim.step(); g_sim.events.clear();
            Entity* j = g_sim.get(jet); if (!j) return fail("the jet was lost with nothing shooting at it");
            float sp2 = dist(j->pos, prev) / SIM_DT; maxSpeed = std::max(maxSpeed, sp2); prev = j->pos;
            float da = std::abs(j->angle - prevAngle); if (da > 3.14159f) da = 6.2831853f - da; maxTurn = std::max(maxTurn, da / SIM_DT); prevAngle = j->angle;
            minAmmo = std::min(minAmmo, j->ammo);
            if (j->alt > 0.9f) leftPad = true;
            if (j->pos.x < 0 || j->pos.y < 0 || j->pos.x > WORLD_W || j->pos.y > WORLD_H) return fail("the jet left the map");
            for (auto v : vics) if (!g_sim.get(v)) { /* a kill */ }
        }
        float hp1 = 0; int dead = 0; for (auto v : vics) { if (Entity* e = g_sim.get(v)) hp1 += e->hp; else dead++; }
        if (maxSpeed < 450) { fprintf(stderr, "top speed %.0f px/s\n", maxSpeed); return fail("a supersonic jet should top 450 px/s"); }
        if (maxTurn > JET_TURN * 2.2f + 0.2f) { fprintf(stderr, "turn rate %.2f rad/s\n", maxTurn); return fail("jet snapped its heading instead of banking"); }
        if (!leftPad) return fail("jet never took off");
        if (minAmmo >= ammo0) return fail("jet never fired");
        if (hp1 >= hp0 && dead == 0) return fail("jet strafing did no damage");
        // it flies back to the pad, rearms and lands
        g_sim.cmdStop({jet});
        for (int t = 0; t < 20 * 30; t++) { g_sim.step(); g_sim.events.clear(); }
        Entity* j = g_sim.get(jet);
        if (j && j->ammo < UNITS[jetType].ammo && j->order != O_ATTACK) return fail("jet did not rearm");
        printf("jettest %s: ok (top speed %.0f px/s, turn %.1f rad/s, damage %.0f, kills %d)\n", FACTION_NAME[fac[0]], maxSpeed, maxTurn, hp0 - hp1, dead);
    }
    return true;
}


// The AI's learned weights from before the jets (v2: 22 unit rows) must load into the new unit table without shifting any army's rows.
static bool brainTest() {
    auto fail = [](const char* m) { fprintf(stderr, "braintest: %s\n", m); return false; };
    std::string path = std::string(getenv("TMPDIR") ? getenv("TMPDIR") : "/tmp") + "/onehour_braintest.txt";
    FILE* f = fopen(path.c_str(), "w");
    if (!f) return fail("cannot write the temporary brain file");
    fprintf(f, "onehour-brain 2 5 9\n0.1 0.2 0.3 0.4 0.5 \n");
    for (int r = 0; r < 22; r++) fprintf(f, "%d %.3f 0.100 0.200 0.300\n", r + 1, 1.0f + r * 0.01f);
    fclose(f);
    Brain b;
    if (!b.load(path.c_str())) { remove(path.c_str()); return fail("a v2 brain file was refused"); }
    bool ok = b.games == 5 && b.waveSamples == 9 && std::abs(b.ww[2] - 0.3f) < 1e-4f;
    for (int r = 0; r < 22 && ok; r++) {
        int t = r < 11 ? r : r + 1;   // the Clanker block moved up one place for the Cyber jet
        ok = b.unitSamples[t] == r + 1 && std::abs(b.wu[t][0] - (1.0f + r * 0.01f)) < 1e-3f && std::abs(b.wu[t][3] - 0.3f) < 1e-3f;
    }
    if (!ok) { remove(path.c_str()); return fail("v2 rows landed on the wrong unit types"); }
    if (b.unitSamples[U_C_JET] != 0 || b.unitSamples[U_K_JET] != 0 || b.wu[U_C_JET][0] != 1.0f || b.wu[U_K_JET][0] != 1.0f) { remove(path.c_str()); return fail("the jets should start from the prior"); }
    if (!b.save(path.c_str())) { remove(path.c_str()); return fail("save failed"); }
    Brain c;
    bool again = c.load(path.c_str());
    remove(path.c_str());
    if (!again || c.unitSamples[U_K_DOZER] != 12 || c.unitSamples[U_K_TITAN] != 22 || c.unitSamples[U_C_TITAN] != 11) return fail("a saved v3 brain did not round-trip");
    printf("braintest: ok (v2 weights migrated, jets start from the prior, v3 round-trips)\n");
    return true;
}

// Feeds synthetic SDL events through the real input handling (headless) and checks the outcomes.
static bool uiTest() {
    auto key = [](SDL_Keycode k, u16 mod = 0) { SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.keysym.mod = mod; g_game.handleEvent(e); };
    auto motion = [](int x, int y) { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y; g_game.handleEvent(e); };
    auto click = [&](int x, int y, int btn) {
        motion(x, y);
        SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = btn; e.button.x = x; e.button.y = y; g_game.handleEvent(e);
        e.type = SDL_MOUSEBUTTONUP; g_game.handleEvent(e);
    };
    auto frames = [](int n) { for (int i = 0; i < n; i++) { g_game.update(1.0f / 60); g_game.render(); } };
    auto fail = [](const char* m) { fprintf(stderr, "uitest: %s\n", m); return false; };
    // menu: pick Clanker, 1 enemy, start
    key(SDLK_RIGHT); // faction -> clanker
    key(SDLK_DOWN); key(SDLK_LEFT); // enemies 2 -> 1
    for (int i = 0; i < 8 && g_game.state != GS_PLAYING; i++) { key(SDLK_DOWN); if (g_game.menu.cursor == 7) key(SDLK_RETURN); }
    if (g_game.state != GS_PLAYING) return fail("game did not start from the menu");
    if (g_sim.players[0].faction != F_CLANKER) return fail("faction selection ignored");
    if (g_sim.numPlayers != 2) return fail("enemy count ignored");
    frames(3);
    // click on the dozer
    Entity* dz = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) dz = &e;
    if (!dz) return fail("no dozer");
    Vec2 s = Vec2(dz->pos.x - g_game.cam.x, dz->pos.y - g_game.cam.y);
    click((int)s.x, (int)s.y, SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1) return fail("dozer not selected by click");
    // hotkey P = power plant, place it below the base
    key(SDLK_p);
    if (g_game.placingType < 0) return fail("build hotkey did not enter placement");
    Vec2 base = g_sim.players[0].basePos;
    int placed = 0;
    for (int dy = 4; dy < 12 && !placed; dy++) for (int dx = -8; dx < 8 && !placed; dx++) {
        int tx = tileOf(base.x) + dx, ty = tileOf(base.y) + dy;
        if (!g_sim.canPlace(0, g_game.placingType, tx, ty)) continue;
        Vec2 c = g_sim.buildingCenter(g_game.placingType, tx, ty);
        click((int)(c.x - g_game.cam.x), (int)(c.y - g_game.cam.y), SDL_BUTTON_LEFT);
        placed = 1;
    }
    if (!placed || g_sim.countRole(0, BR_POWER, false) != 1) return fail("power plant not placed by click");
    if (g_game.placingType >= 0) return fail("placement mode should end after placing");
    // right-click move the dozer somewhere: it should abandon construction (order becomes MOVE)
    click(400, 200, SDL_BUTTON_RIGHT);
    if (dz->order != O_MOVE) return fail("right-click move ignored");
    // reassign: right-click the site to continue construction
    Entity* site = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && !e.constructed) site = &e;
    if (!site) return fail("site missing");
    click((int)(site->pos.x - g_game.cam.x), (int)(site->pos.y - g_game.cam.y), SDL_BUTTON_RIGHT);
    if (dz->order != O_BUILD) return fail("right-click on own site did not resume construction");
    // let it build, then select the HQ and queue a dozer with the D hotkey, then cancel it
    for (int i = 0; i < 60 * 40 && !g_sim.hasRole(0, BR_POWER); i++) frames(1);
    if (!g_sim.hasRole(0, BR_POWER)) return fail("power plant never finished");
    Entity* hq = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_HQ) hq = &e;
    click((int)(hq->pos.x - g_game.cam.x), (int)(hq->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1 || g_sim.get(g_game.selection[0]) != hq) return fail("HQ not selected");
    int money = g_sim.players[0].money;
    key(SDLK_d);
    if (hq->queue.size() != 1) return fail("train hotkey did not queue");
    // cancel via the queue slot click in the HUD
    click(INFO_X_TEST + 330 + 14, SCREEN_H - HUD_H + 70 + 14, SDL_BUTTON_LEFT);
    if (!hq->queue.empty() || g_sim.players[0].money != money) return fail("queue cancel did not refund");
    // command grid button click: first button (Dozer) trains
    click(SCREEN_W - 348 + 20, SCREEN_H - HUD_H + 8 + 10, SDL_BUTTON_LEFT);
    if (hq->queue.size() != 1) return fail("command button click did not queue");
    // rally point by right-click on ground with HQ selected
    click(300, 300, SDL_BUTTON_RIGHT);
    if (!hq->hasRally) return fail("rally point not set");
    // control groups: select dozer, Ctrl+1, clear, press 1
    click((int)(dz->pos.x - g_game.cam.x), (int)(dz->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1) return fail("dozer reselect failed");
    key(SDLK_1, KMOD_CTRL);
    g_game.selection.clear();
    key(SDLK_1);
    if (g_game.selection.size() != 1) return fail("control group recall failed");
    // drag box select over the dozer
    Vec2 d2 = Vec2(dz->pos.x - g_game.cam.x, dz->pos.y - g_game.cam.y);
    motion((int)d2.x - 30, (int)d2.y - 30);
    { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)d2.x - 30; e.button.y = (int)d2.y - 30; g_game.handleEvent(e); }
    motion((int)d2.x + 30, (int)d2.y + 30);
    { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)d2.x + 30; e.button.y = (int)d2.y + 30; g_game.handleEvent(e); }
    if (g_game.selection.size() != 1) return fail("drag box select failed");
    // minimap click moves the camera
    Vec2 camBefore = g_game.cam;
    click(8 + 58, SCREEN_H - HUD_H + 8 + 58, SDL_BUTTON_LEFT);   // map centre: never equals a corner base view
    if (dist(camBefore, g_game.cam) < 100) return fail("minimap click did not move camera");
    key(SDLK_HOME);
    // pause toggles, speed keys, help, mute, screenshot key path
    key(SDLK_SPACE); if (!g_game.paused) return fail("pause failed"); key(SDLK_SPACE);
    key(SDLK_EQUALS); if (g_game.speed <= 1.0f) return fail("speed up failed"); key(SDLK_MINUS);
    key(SDLK_F1); if (!g_game.showHelp) return fail("help failed"); key(SDLK_F1);
    // area orders through the real input path: G, then click or drag on the ground
    {
        Vec2 b0 = g_sim.players[0].basePos + Vec2(0, 150);
        Ref t1 = g_sim.spawnUnit(U_K_INF1, 0, b0), t2 = g_sim.spawnUnit(U_K_TANK, 0, b0 + Vec2(40, 0));
        Ref hv = g_sim.spawnUnit(U_K_HARV, 0, b0 + Vec2(-40, 0));
        g_game.cam = Vec2(clampf(b0.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b0.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
        g_game.selection = { t1, t2 };
        key(SDLK_g);
        if (!g_game.areaMode) return fail("G did not start area mode");
        frames(2);   // the preview overlay must render
        Vec2 a = Vec2(300, 200);
        motion((int)a.x, (int)a.y);
        { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)a.x; e.button.y = (int)a.y; g_game.handleEvent(e); }
        motion((int)a.x + 100, (int)a.y);
        frames(2);
        { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)a.x + 100; e.button.y = (int)a.y; g_game.handleEvent(e); }
        Entity* u1 = g_sim.get(t1);
        if (g_game.areaMode || !u1 || u1->order != O_GUARDAREA) return fail("drag did not assign a guard area");
        if (std::abs(u1->zoneR - 100.0f) > 3) return fail("guard area radius does not follow the drag");
        frames(2);
        key(SDLK_g); key(SDLK_ESCAPE);
        if (g_game.areaMode) return fail("Esc did not cancel area mode");
        g_game.selection = { hv };
        key(SDLK_g); click(400, 250, SDL_BUTTON_LEFT);
        Entity* h = g_sim.get(hv);
        if (!h || h->zoneR <= 0) return fail("click did not assign a gather area to the hauler");
        g_game.selection = { t1 };
        key(SDLK_s);
        if (g_sim.get(t1)->zoneR != 0) return fail("stop did not clear the area");
    }
    // tech structure: research and scan through the real input path, flags and shroud-free rendering while the scan runs
    {
        Vec2 b0 = g_sim.players[0].basePos;
        Ref tech; g_game.selection.clear();
        for (int dy = 6; dy < 14 && !tech.valid(); dy++) for (int dx = -10; dx < 10 && !tech.valid(); dx++) {
            int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy;
            if (g_sim.canPlace(0, B_K_TECH, tx, ty)) tech = g_sim.placeBuilding(B_K_TECH, 0, tx, ty, true);
        }
        if (!tech.valid()) return fail("no room for a tech structure");
        Entity* te = g_sim.get(tech);
        g_sim.players[0].money = 9000; g_sim.players[0].scanReady = 0;
        g_game.cam = Vec2(clampf(te->pos.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(te->pos.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
        frames(2);
        click((int)(te->pos.x - g_game.cam.x), (int)(te->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
        if (g_game.selection.size() != 1 || g_sim.get(g_game.selection[0]) != te) return fail("tech structure not selected");
        key(SDLK_r);
        if (!g_sim.players[0].researching) return fail("R did not start the research");
        if (g_sim.players[0].money != 9000 - PROGRAMS[F_CLANKER].cost) return fail("research cost not charged");
        key(SDLK_v);
        if (!g_sim.revealed(0)) return fail("V did not start the scan");
        frames(30);   // shroud-free path, HUD scan label, tech panel
        for (int i = 0; i < 60 * 50 && !g_sim.players[0].advTech; i++) frames(1);
        if (!g_sim.players[0].advTech) return fail("research did not complete");
        // barracks now offers the Grenadier
        Ref bk; for (int dy = 6; dy < 14 && !bk.valid(); dy++) for (int dx = -12; dx < 12 && !bk.valid(); dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, B_K_BARRACKS, tx, ty)) bk = g_sim.placeBuilding(B_K_BARRACKS, 0, tx, ty, true); }
        Entity* be = g_sim.get(bk);
        click((int)(be->pos.x - g_game.cam.x), (int)(be->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
        key(SDLK_b);
        if (be->queue.size() != 1 || be->queue[0] != U_K_ELITE) return fail("B did not queue the Grenadier after the program");
    }
    // the Command Post falls and a dozer founds a new one through the real input path (C hotkey, click to place), and the jet shows up in the airstrip's menu
    {
        Entity* oldHq = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_HQ) oldHq = &e;
        if (!oldHq) return fail("no Command Post to lose");
        Vec2 spot = oldHq->pos;
        g_sim.destroy(*oldHq, true);
        Ref dzr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) dzr = g_sim.refOf(e);
        if (!dzr.valid()) { dzr = g_sim.spawnUnit(U_K_DOZER, 0, spot + Vec2(0, 90)); }
        g_sim.players[0].money = 9000;
        g_game.selection = { dzr };
        g_game.cam = Vec2(clampf(spot.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(spot.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
        frames(2);
        key(SDLK_c);
        if (g_game.placingType != B_K_HQ) return fail("C did not offer the Command Post to the dozer");
        bool put = false;
        for (int ring = 2; ring < 14 && !put; ring++) for (int dy = -ring; dy <= ring && !put; dy++) for (int dx = -ring; dx <= ring && !put; dx++) {
            if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
            int tx = tileOf(spot.x) - 2 + dx, ty = tileOf(spot.y) - 2 + dy;
            if (!g_sim.canPlace(0, B_K_HQ, tx, ty)) continue;
            Vec2 c = g_sim.buildingCenter(B_K_HQ, tx, ty);
            click((int)(c.x - g_game.cam.x), (int)(c.y - g_game.cam.y), SDL_BUTTON_LEFT);
            put = true;
        }
        if (!put || g_sim.countRole(0, BR_HQ, false) != 1) return fail("Command Post not placed by click");
        frames(2);   // the foundation renders
        for (int i = 0; i < 60 * 70 && !g_sim.hasRole(0, BR_HQ); i++) frames(1);
        if (!g_sim.hasRole(0, BR_HQ)) return fail("new Command Post never finished");
        // the build menu lists it last, after the other structures
        g_game.selection = { dzr }; g_game.buildButtonsPublic();
        int lastHq = -1, lastIdx = -1;
        for (size_t i = 0; i < g_game.buttonsPublic().size(); i++) { const auto& b = g_game.buttonsPublic()[i]; if (b.kind == 1) { lastIdx = (int)i; if (BUILDS[b.id].role == BR_HQ) lastHq = (int)i; } }
        if (lastHq < 0 || lastHq != lastIdx) return fail("Command Post should be the last entry of the dozer's build menu");
    }
    // run a while with rendering to shake out draw paths
    frames(120);
    // pause menu: Esc opens it and freezes the sim, speed is adjustable in it, Esc resumes
    {
        float speed0 = g_game.speed; u32 tick0 = g_sim.tick;
        key(SDLK_ESCAPE);
        if (!g_game.menuOpen) return fail("Esc did not open the pause menu");
        frames(20);
        if (g_sim.tick != tick0) return fail("sim advanced while the pause menu was open");
        key(SDLK_DOWN); key(SDLK_RIGHT); key(SDLK_RIGHT);
        if (g_game.speed <= speed0) return fail("pause menu speed control did not raise the speed");
        key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_LEFT);
        if (g_game.speed >= speed0) return fail("pause menu speed control did not lower the speed");
        key(SDLK_ESCAPE);
        if (g_game.menuOpen) return fail("Esc did not close the pause menu");
        frames(10);
        if (g_sim.tick == tick0) return fail("sim did not resume after the pause menu");
        // surrender needs a second confirmation and ends the game as a defeat
        key(SDLK_ESCAPE);
        for (int i = 0; i < 5; i++) key(SDLK_DOWN);
        key(SDLK_RETURN);
        if (g_game.state != GS_PLAYING) return fail("surrender did not ask for confirmation");
        key(SDLK_RETURN);
        frames(3);
        if (g_game.state != GS_GAMEOVER || !g_sim.gameOver || g_sim.winnerTeam == g_sim.players[0].team) return fail("surrender did not end the game as a defeat");
        key(SDLK_RETURN);
        if (g_game.state != GS_MENU) return fail("game over screen did not return to the menu");
    }
    printf("uitest: ok\n");
    return true;
}

int main(int argc, char** argv) {
    int scale = 0; int reqW = 0, reqH = 0; bool software = false; bool headless = false;
    int selftestSecs = -1; const char* shot = nullptr; const char* sheetPath = nullptr; int ticks = 0; u64 seed = 12345; Faction faction = F_CYBER; int players = 4; int d0 = 3; bool swap = false; int viewPlayer = 0; bool allAi = false; int trainN = 0, evalN = 0; bool autostart = false; int benchFrames = 0;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--scale" && i + 1 < argc) scale = clampi(atoi(argv[++i]), 1, 4);
        else if (a == "--size" && i + 1 < argc) { if (sscanf(argv[++i], "%dx%d", &reqW, &reqH) != 2) { reqW = reqH = 0; } }
        else if (a == "--software") software = true;
        else if (a == "--selftest") { selftestSecs = 600; if (i + 1 < argc && argv[i + 1][0] != '-') selftestSecs = atoi(argv[++i]); }
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; headless = true; software = true; }
        else if (a == "--sheet" && i + 1 < argc) { sheetPath = argv[++i]; headless = true; software = true; }
        else if (a == "--ticks" && i + 1 < argc) ticks = atoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = (u64)atoll(argv[++i]);
        else if (a == "--players" && i + 1 < argc) players = clampi(atoi(argv[++i]), 2, 4);
        else if (a == "--swap") swap = true;
        else if (a == "--train") { trainN = 40; if (i + 1 < argc && argv[i + 1][0] != '-') trainN = atoi(argv[++i]); }
        else if (a == "--eval") { evalN = 16; if (i + 1 < argc && argv[i + 1][0] != '-') evalN = atoi(argv[++i]); }
        else if (a == "--allai") allAi = true;
        else if (a == "--soundcheck") { g_audio.debugStats(); return 0; }
        else if (a == "--uitest") { headless = true; software = true; shot = nullptr; ticks = -1; }
        else if (a == "--autostart") autostart = true;
        else if (a == "--bench") { benchFrames = 300; if (i + 1 < argc && argv[i + 1][0] != '-') benchFrames = atoi(argv[++i]); headless = true; software = true; }
        else if (a == "--scenario") { g_map.generate(); return scenarioTest(seed) ? 0 : 1; }
        else if (a == "--econtest") { g_map.generate(); return econTest(seed) ? 0 : 1; }
        else if (a == "--hqtest") { g_map.generate(); return hqTest(seed) ? 0 : 1; }
        else if (a == "--jettest") { g_map.generate(); return jetTest(seed) ? 0 : 1; }
        else if (a == "--braintest") return brainTest() ? 0 : 1;
        else if (a == "--areatest") { g_map.generate(); return areaTest(seed) ? 0 : 1; }
        else if (a == "--view" && i + 1 < argc) viewPlayer = clampi(atoi(argv[++i]), 0, 9);
        else if (a == "--d0" && i + 1 < argc) d0 = clampi(atoi(argv[++i]), 0, 3);
        else if (a == "--faction" && i + 1 < argc) faction = argv[++i][0] == 'k' ? F_CLANKER : F_CYBER;
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); usage(); return 1; }
    }
    g_map.generate();
    if (trainN > 0) return trainBrain(trainN, seed);
    if (evalN > 0) return evalBrain(evalN, seed, d0 >= 0 && d0 <= 3 ? std::max(1, std::min(d0, 3)) : 2);
    if (selftestSecs >= 0) return selfTest(selftestSecs, seed, players, d0, swap) ? 0 : 1;

    if (headless) SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    // headless runs (screenshots, ui test) keep the classic 1024x640 unless --size is given
    if (headless && reqW <= 0) { reqW = MIN_SCREEN_W; reqH = MIN_SCREEN_H; }
    if (!g_gfx.init(scale, software, reqW, reqH)) return 1;
    if (!headless) {
        g_audio.init();
        g_brainPath = Brain::defaultPath();
        if (!g_brainPath.empty()) g_brain.load(g_brainPath.c_str());   // the AI keeps what it learned in earlier games
    }

    g_game.seed = seed;
    g_game.menu.playerFaction = faction;
    if (sheetPath) {
        Gfx& g = g_gfx;
        g.beginFrame(rgb(74, 96, 58));
        if (const char* bl = getenv("ONEHOUR_BLD")) {   // every structure of one faction at 1x (ONEHOUR_BLD=0 cyber, 1 clanker); ONEHOUR_BLDSCALE zooms
            int fac = atoi(bl) & 1; float sc = getenv("ONEHOUR_BLDSCALE") ? (float)atof(getenv("ONEHOUR_BLDSCALE")) : 1.0f;
            int first = fac == 0 ? B_C_HQ : B_K_HQ, only = getenv("ONEHOUR_BLDONLY") ? atoi(getenv("ONEHOUR_BLDONLY")) : -1;
            float x = 10, y = 6, rowH = 0;
            for (int i = 0; i < BUILDS_PER_FACTION; i++) {
                if (only >= 0 && i != only) continue;
                int t = first + i; const BuildType& bt = BUILDS[t];
                float w = (bt.w * TILE + 20) * sc, h = (bt.h * TILE + 44) * sc;
                if (x + w > SCREEN_W - 4) { x = 10; y += rowH + 4; rowH = 0; }
                float cx = x + (BART_PAD_L + bt.w * TILE * 0.5f) * sc - 6 * sc, cy = y + (BART_PAD_T + bt.h * TILE * 0.5f) * sc - 6 * sc;
                g.draw(g.building[t], cx, cy, 0, sc);
                if (g.buildingTeam[t].tex) g.draw(g.buildingTeam[t], cx, cy, 0, sc, PLAYER_COLOR[i & 3]);
                if (bt.role == BR_TURRET) g.draw(g.turretHead[fac == 0 ? 0 : 1], cx, cy, 0.6f, sc);
                if (bt.role == BR_AATURRET) g.draw(g.turretHead[fac == 0 ? 2 : 3], cx, cy, 0.6f, sc);
                x += w; rowH = std::max(rowH, h);
            }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_BIG")) {   // 6x close-up of one faction: ONEHOUR_BIG=0 cyber, 1 clanker
            int fac = atoi(getenv("ONEHOUR_BIG")) & 1;
            for (int u = 0; u < UNITS_PER_FACTION; u++) {
                int t = (fac == 0 ? U_C_DOZER : U_K_DOZER) + u;
                float x = 100 + (u % 4) * 230, y = 100 + (u / 4) * 200;
                g.draw(g.unitBody[t][0], x, y, 0, 5.5f);
                if (g.unitTurret[t][0].tex) g.draw(g.unitTurret[t][0], x, y, 0, 5.5f);
                if (t == U_K_AIR) { g.draw(g.rotorDisc, x - 10, y, 0, 5.5f); g.draw(g.rotorBlades, x - 10, y, 0.5f, 5.5f, rgb(255, 255, 255), 150); }
            }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        for (int row = 0; row < 4; row++) for (int u = 0; u < UNITS_PER_FACTION; u++) {
            int fac = row / 2, owner = row % 2, t = (fac == 0 ? U_C_DOZER : U_K_DOZER) + u;
            float x = 44 + u * 82, y = 60 + row * 100;
            if (UNITS[t].kind != UK_AIR) g.draw(g.shadowLarge, x, y + 6, 0, 1.6f);
            g.draw(g.unitBody[t][owner], x, y, 0, 3);
            if (g.unitTurret[t][owner].tex) g.draw(g.unitTurret[t][owner], x, y, 0, 3);
            if (t == U_K_AIR) { g.draw(g.rotorDisc, x - 6, y, 0, 3); g.draw(g.rotorBlades, x - 6, y, 0.5f, 3, rgb(255, 255, 255), 150); }
            // 1x row with the walk frames
            float x1 = 30 + u * 32 + (row % 2) * 0;
            float y1 = 470 + row * 34;
            g.draw(g.unitBody[t][owner], x1, y1, 0, 1); if (g.unitTurret[t][owner].tex) g.draw(g.unitTurret[t][owner], x1, y1, 0, 1);
            if (UNITS[t].kind != UK_AIR) { g.draw(g.unitAnim[t][owner][0], 400 + u * 32, y1, 0, 1); if (g.unitTurret[t][owner].tex) g.draw(g.unitTurret[t][owner], 400 + u * 32, y1, 0, 1); }
        }
        for (int i = 0; i < 4; i++) g.draw(g.turretHead[i], 800 + i * 50, 480, 0, 1.2f);
        for (int p = 0; p < MAX_PLAYERS; p++) g.drawFlag(820 + p * 50, 590, p, 1.4f, 1.0f);
        g_game.screenshot(sheetPath);
        printf("wrote %s\n", sheetPath);
        g_gfx.shutdown(); SDL_Quit(); return 0;
    }
    if (ticks == -1) { bool ok = uiTest(); g_gfx.shutdown(); SDL_Quit(); return ok ? 0 : 1; }
    if (benchFrames > 0) {
        // a busy scene: four AI armies eight minutes in, the camera sweeping across the battle, every frame fully rendered (software renderer: no GPU help)
        g_game.startGame(); g_sim.players[0].isAI = true; g_ai.init(seed);
        for (int t = 0; t < 9600; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
        u64 f0 = SDL_GetPerformanceFrequency(); double worst = 0, total = 0;
        for (int fr = 0; fr < benchFrames; fr++) {
            g_sim.step(); g_ai.update(); g_sim.events.clear();
            float u = fr / (float)benchFrames;
            g_game.cam = Vec2(clampf(200 + u * 1500, 0, WORLD_W - SCREEN_W), clampf(300 + std::sin(u * 6.0f) * 700 + 700, 0, WORLD_H - VIEW_H));
            g_game.renderAlpha = 1; g_game.wallTime += 1.0f / 60; g_game.frameDt = 1.0f / 60;
            u64 t0 = SDL_GetPerformanceCounter();
            g_game.update(1.0f / 60);
            g_game.render();
            g_gfx.present();   // SDL queues draw calls until present: the frame only costs what it costs once flushed
            double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / f0;
            total += ms; worst = std::max(worst, ms);
        }
        printf("bench: %d frames, avg %.2f ms, worst %.2f ms (software renderer), particles %zu, fx %zu, entities %zu\n", benchFrames, total / benchFrames, worst, g_game.parts.size(), g_sim.fx.size(), g_sim.ents.size());
        g_gfx.shutdown(); SDL_Quit(); return 0;
    }
    if (shot) {
        if (ticks > 0) {
            g_game.startGame();
            if (allAi) { g_sim.players[0].isAI = true; g_ai.init(seed); }
            for (int t = 0; t < ticks; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
            Vec2 b = g_sim.players[std::min(viewPlayer, 3)].basePos;
            if (viewPlayer == 9) {
                // centre on the action: the unit hit most recently
                float best = -1; for (auto& e : g_sim.ents) if (e.alive && e.lastDamaged > best) { best = e.lastDamaged; b = e.pos; }
                if (!g_sim.projs.empty()) b = g_sim.projs[0].pos;
                printf("action at tile %d,%d (projectiles %zu, fx %zu, lastDamaged %.1f at t=%.1f)\n", tileOf(b.x), tileOf(b.y), g_sim.projs.size(), g_sim.fx.size(), best, g_sim.time);
            }
            g_game.cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            g_game.renderAlpha = 1;
            if (viewPlayer != 0 || getenv("ONEHOUR_REVEAL")) { std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1); }
            if (const char* cm = getenv("ONEHOUR_CAM")) {   // ONEHOUR_CAM=tx,ty: centre the view on a map tile
                float tx = 40, ty = 40; if (sscanf(cm, "%f,%f", &tx, &ty) == 2) g_game.cam = Vec2(clampf(tx * TILE - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(ty * TILE - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            }
            // ONEHOUR_SEL=turret|army|haul|enemy: showcase selections for screenshots
            if (const char* sel = getenv("ONEHOUR_SEL")) {
                std::string m = sel;
                Vec2 focus = b;
                if (m == "tech") {
                    // showcase the tech structure panel: research running, scan active, an elite unit on the field
                    Faction f0 = g_sim.players[0].faction;
                    int tt = firstBuildOf(f0) + BR_TECH; Ref tr;
                    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.type == tt && e.constructed) tr = g_sim.refOf(e);
                    Vec2 bp = g_sim.players[0].basePos;
                    for (int dy = 4; dy < 16 && !tr.valid(); dy++) for (int dx = -14; dx < 14 && !tr.valid(); dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (g_sim.canPlace(0, tt, tx, ty)) tr = g_sim.placeBuilding(tt, 0, tx, ty, true); }
                    g_sim.players[0].money = 9000; g_sim.players[0].scanReady = 0; g_sim.updatePowerPublic();
                    g_sim.cmdResearch(0); g_sim.cmdScan(0);
                    for (int t = 0; t < 60; t++) g_sim.step();
                    if (Entity* te = g_sim.get(tr)) { g_game.selection.push_back(tr); focus = te->pos; }
                    Vec2 at = focus + Vec2(-90, 90);
                    g_sim.spawnUnit(firstUnitOf(f0) + 10, 0, at); g_sim.spawnUnit(firstUnitOf(f0) + 9, 0, at + Vec2(60, 10));
                    g_sim.spawnUnit(firstUnitOf(f0) + 5, 0, at + Vec2(-40, 30)); g_sim.spawnUnit(firstUnitOf(f0) + 2, 0, at + Vec2(20, 50));
                } else if (m == "turret") {
                    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.constructed && e.bt().weapon >= 0) { g_game.selection.push_back(g_sim.refOf(e)); focus = e.pos; break; }
                } else if (m == "army" || m == "haul") {
                    UnitRole want = m == "army" ? UR_COMBAT : UR_HARVESTER;
                    Vec2 c; int n = 0;
                    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == want) { g_game.selection.push_back(g_sim.refOf(e)); c += e.pos; n++; }
                    if (n) { c = c * (1.0f / n); focus = c; g_sim.cmdArea(g_game.selection, c + Vec2(0, 80), 6 * TILE); for (int t = 0; t < 40; t++) g_sim.step(); }
                }
                g_game.cam = Vec2(clampf(focus.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(focus.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            }
            if (getenv("ONEHOUR_SITES")) {   // showcase: unfinished structures at 15 / 50 / 85 percent, and the placement ghost
                Faction f0 = g_sim.players[0].faction; int bb2 = firstBuildOf(f0); Vec2 bp = g_sim.players[0].basePos;
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
                int types[3] = { bb2 + BR_FACTORY, bb2 + BR_TECH, bb2 + BR_BARRACKS }; float prog[3] = { 0.15f, 0.5f, 0.85f };
                int ox = -14;
                for (int k = 0; k < 3; k++) for (int dy = 6; dy < 20; dy++) { bool ok = false; for (int dx = ox; dx < 16 && !ok; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (g_sim.canPlace(0, types[k], tx, ty)) { Ref r = g_sim.placeBuilding(types[k], 0, tx, ty, false); g_sim.get(r)->progress = prog[k]; g_sim.get(r)->hp = BUILDS[types[k]].hp * (0.1f + 0.9f * prog[k]); ox = tx - tileOf(bp.x) + BUILDS[types[k]].w + 1; ok = true; } } if (ok) break; }
                g_game.cam = Vec2(clampf(bp.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(bp.y + 150 - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                g_game.placingType = bb2 + BR_AIRFIELD; g_game.mouseX = 760; g_game.mouseY = 330;
            }
            if (getenv("ONEHOUR_JETS")) {   // showcase: an airfield, two jets strafing a column and a third parked on its pad
                Faction f0 = g_sim.players[0].faction; int bb2 = firstBuildOf(f0); Vec2 bp = g_sim.players[0].basePos; Ref af;
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
                for (int dy = 4; dy < 16 && !af.valid(); dy++) for (int dx = -12; dx < 12 && !af.valid(); dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (g_sim.canPlace(0, bb2 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(bb2 + BR_AIRFIELD, 0, tx, ty, true); }
                Entity* a = g_sim.get(af); Vec2 pad = a->pos;
                int jt = firstUnitOf(f0) + 11;
                std::vector<Ref> jets;
                for (int i = 0; i < 3; i++) { Ref j = g_sim.spawnUnit(jt, 0, pad + Vec2((i - 1.5f) * 30, 0)); g_sim.get(j)->home = af; jets.push_back(j); }
                Vec2 tg = g_map.nearestFree(pad + Vec2(300, 150), 30);
                bool park = getenv("ONEHOUR_JETS")[0] == 'p';   // ONEHOUR_JETS=park: no targets, the jets sit on their pads
                if (!park) for (int i = 0; i < 5; i++) g_sim.spawnUnit(g_sim.players[1].faction == F_CYBER ? U_C_TANK : U_K_TANK, 1, tg + Vec2(i * 26 - 50, (i % 2) * 22));
                std::vector<Ref> two = { jets[0], jets[1] };
                if (!park) g_sim.cmdAttack(two, g_sim.refOf(*[&]() { for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.isUnit()) return &e; return (Entity*)nullptr; }()));
                int n = getenv("ONEHOUR_JETS")[0] == '1' ? 30 : (park ? 60 : atoi(getenv("ONEHOUR_JETS")));
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                Vec2 mid = (tg + pad) * 0.5f;
                g_game.cam = Vec2(clampf(mid.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(mid.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                g_game.selection.clear(); g_game.selection.push_back(jets[0]);
            }
            if (getenv("ONEHOUR_PAUSEMENU")) g_game.openPauseMenu();
            if (getenv("ONEHOUR_NEWB")) {   // showcase: income structure, nuke ramp, a nuke in flight
                int bb2 = firstBuildOf(g_sim.players[0].faction); Vec2 bp = g_sim.players[0].basePos; Ref last;
                int types[2] = { bb2 + BR_INCOME, bb2 + BR_NUKE };
                for (int k = 0; k < 2; k++) for (int dy = 5; dy < 16; dy++) { bool ok = false; for (int dx = -12; dx < 12 && !ok; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (g_sim.canPlace(0, types[k], tx, ty)) { last = g_sim.placeBuilding(types[k], 0, tx, ty, true); ok = true; } } if (ok) break; }
                g_sim.get(last)->actionTimer = 0; g_game.selection.clear(); g_game.selection.push_back(last);
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
                bool boom = getenv("ONEHOUR_NEWB")[0] == 'b';   // ONEHOUR_NEWB=boom: freeze a moment after detonation instead of the flight
                g_sim.nukes.push_back({g_sim.get(last)->pos, g_sim.get(last)->pos + Vec2(300, 40), 0, boom ? Sim::NUKE_FLIGHT - 0.05f : 4.0f});
                if (boom) for (int t = 0; t < 22; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                g_game.cam = Vec2(clampf(g_sim.get(last)->pos.x - SCREEN_W / 2 + 100, 0, WORLD_W - SCREEN_W), clampf(g_sim.get(last)->pos.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            }
            // select something for the HUD
            if (g_game.selection.empty()) for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && !e.queue.empty()) { g_game.selection.push_back(g_sim.refOf(e)); break; }
            if (g_game.selection.empty()) for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) { g_game.selection.push_back(g_sim.refOf(e)); break; }
        }
        g_game.render();
        g_game.screenshot(shot);
        printf("wrote %s\n", shot);
        g_gfx.shutdown(); SDL_Quit();
        return 0;
    }

    if (autostart) { g_game.startGame(); if (allAi) { g_sim.players[0].isAI = true; g_ai.init(seed); } g_game.speed = 3.0f; }
    u64 freq = SDL_GetPerformanceFrequency();
    u64 last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else {
                if (e.type == SDL_WINDOWEVENT && (e.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || e.window.event == SDL_WINDOWEVENT_RESIZED)) g_gfx.onResize();
                g_game.handleEvent(e);
            }
        }
        if (g_game.quitRequested) running = false;
        u64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / (double)freq);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        g_game.update(dt);
        g_game.render();
        g_gfx.present();
        if (software) SDL_Delay(12);   // no vsync on the software path: cap around 60 fps
    }
    g_audio.shutdown();
    g_gfx.shutdown();
    SDL_Quit();
    return 0;
}
