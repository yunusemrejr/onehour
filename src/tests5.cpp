// One Hour - headless test of a computer army that has lost almost everything and gets back on its feet (--recoverytest)
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok5 = true;
static bool fail5(const char* m) { fprintf(stderr, "recoverytest: %s\n", m); g_ok5 = false; return false; }
#define CHECK5(cond, msg) do { if (!(cond)) return fail5(msg); } while (0)

static void runAi(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); } }

// player 0 is a human who does nothing; player 1 a computer army at 'diff', the clock already well into the match
static void recoveryScene(Faction me, int diff, u64 seed) {
    Faction fac[2] = { F_CYBER, me };
    bool ai[2] = { false, true }; int dif[2] = { 1, diff }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, dif, team, seed);
    for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
    g_ai.featMask[1] = getenv("ONEHOUR_NORECOVER") ? (FEAT_DEFAULT & ~FEAT_RECOVER) : FEAT_DEFAULT;   // (to see what the scenario does without recovery mode)
    g_ai.init(seed);
    g_ai.featMask[1] = FEAT_DEFAULT;
    g_sim.time = 9 * 60.0f;   // (late enough for every timed gate of the commander)
}

// what is left of an army: a structure kind is counted only once finished
struct Census { int hq = 0, hubs = 0, power = 0, barracks = 0, factories = 0, dozers = 0, haulers = 0, army = 0, sites = 0; };
static Census census(int p) {
    Census c;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.owner != p) continue;
        if (e.isUnit()) {
            if (e.ut().role == UR_DOZER) c.dozers++;
            else if (e.ut().role == UR_HARVESTER) c.haulers++;
            else c.army++;
        } else if (e.isBuilding()) {
            if (!e.constructed) { c.sites++; continue; }
            switch (e.bt().role) {
            case BR_HQ: c.hq++; break; case BR_SUPPLY: c.hubs++; break; case BR_POWER: c.power++; break;
            case BR_BARRACKS: c.barracks++; break; case BR_FACTORY: c.factories++; break; default: break;
            }
        }
    }
    return c;
}
static void printCensus(const char* tag, const Census& c) {
    printf("  %-34s hq %d hubs %d power %d barracks %d factories %d | dozers %d haulers %d army %d | sites %d | $%d\n", tag, c.hq, c.hubs, c.power, c.barracks, c.factories, c.dozers, c.haulers, c.army, c.sites, g_sim.players[1].money);
}

// strip player 1 down: keep the structures for which keep() says so, a few soldiers, nothing else; money as given
template <class F> static void strip(F keep, int soldiers, int money) {
    int kept = 0;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.owner != 1) continue;
        if (e.isBuilding() && keep(e)) continue;
        g_sim.destroy(e, false);
    }
    int inf = firstUnitOf(g_sim.players[1].faction) + 2;
    for (int k = 0; k < soldiers; k++) if (g_sim.spawnUnit(inf, 1, g_map.nearestFree(g_sim.players[1].basePos + Vec2(60.0f + 14.0f * k, 90.0f), 20)).valid()) kept++;
    g_sim.players[1].money = money;
}

// a relief drop lands next to what is left: a dozer and the money (what an Aid Drop delivers)
static void reliefDrop(int money) {
    int dz = firstUnitOf(g_sim.players[1].faction) + 0;
    g_sim.spawnUnit(dz, 1, g_map.nearestFree(g_sim.players[1].basePos + Vec2(0, 120.0f), 20));
    g_sim.players[1].money += money;
}

bool recoveryTest(u64 seed) {
    g_ok5 = true;
    bool verbose = getenv("ONEHOUR_DEBUG") != nullptr;
    for (int fi = 0; fi < 2; fi++) for (int diff = 1; diff <= 2; diff++) {
        Faction me = (Faction)fi;
        char tag[96];
        // ---- 1. nothing left but a few soldiers; then a relief drop (a dozer and $20000): the commander rebuilds its base from scratch
        recoveryScene(me, diff, seed + fi * 10 + diff);
        strip([](const Entity&) { return false; }, 3, 0);
        runAi(20);
        Census c0 = census(1);
        CHECK5(c0.hq == 0 && c0.dozers == 0, "the stripped army still has a Command structure or a dozer");
        reliefDrop(AID_MONEY);
        Census c = census(1);
        for (int k = 0; k < 8; k++) {
            runAi(30);
            c = census(1);
            if (verbose) { snprintf(tag, sizeof tag, "%s d%d relief +%ds", FACTION_NAME[me], diff, 30 * (k + 1)); printCensus(tag, c); }
        }
        snprintf(tag, sizeof tag, "%s d%d after relief, 4 min", FACTION_NAME[me], diff); printCensus(tag, c);
        CHECK5(c.hq >= 1, "a computer army given a dozer and money did not rebuild its Command structure");
        CHECK5(c.dozers >= 2, "a recovering computer army did not train more dozers");
        CHECK5(c.hubs >= 1 && c.haulers >= 1, "a recovering computer army did not restore its supply line");
        CHECK5(c.power >= 1 && c.barracks >= 1, "a recovering computer army did not restore power and a barracks");
        CHECK5(c.factories >= 1, "a recovering computer army did not restore a factory");

        // ---- 2. only the Command structure stands, some money, no dozer: dozers first, then the economy
        recoveryScene(me, diff, seed + 100 + fi * 10 + diff);
        strip([](const Entity& e) { return e.bt().role == BR_HQ; }, 2, 6000);
        for (int k = 0; k < 6; k++) {
            runAi(30);
            c = census(1);
            if (verbose) { snprintf(tag, sizeof tag, "%s d%d hq-only +%ds", FACTION_NAME[me], diff, 30 * (k + 1)); printCensus(tag, c); }
        }
        snprintf(tag, sizeof tag, "%s d%d HQ only, 3 min", FACTION_NAME[me], diff); printCensus(tag, c);
        CHECK5(c.dozers >= 2, "a computer army left with its Command structure did not train dozers");
        CHECK5(c.hubs >= 1 && c.haulers >= 1, "a computer army left with its Command structure did not restore its supply line");
        CHECK5(c.power >= 1 && c.barracks >= 1, "a computer army left with its Command structure did not restore power and a barracks");

        // ---- 3. a bunker with a dozer inside is all that is left, plus money: the dozer comes out and rebuilds
        recoveryScene(me, diff, seed + 200 + fi * 10 + diff);
        strip([](const Entity&) { return false; }, 0, 15000);
        {
            int bt = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
            Vec2 at = g_sim.players[1].basePos;
            Ref bk = NOREF;
            for (int r = 0; r < 14 && !bk.valid(); r++) for (int dy = -r; dy <= r && !bk.valid(); dy++) for (int dx = -r; dx <= r && !bk.valid(); dx++)
                if ((std::abs(dx) == r || std::abs(dy) == r) && g_sim.canPlace(1, bt, tileOf(at.x) + dx, tileOf(at.y) + dy)) bk = g_sim.placeBuilding(bt, 1, tileOf(at.x) + dx, tileOf(at.y) + dy, true);
            CHECK5(bk.valid(), "no spot for the bunker");
            Ref dz = g_sim.spawnUnit(firstUnitOf(me) + 0, 1, g_map.nearestFree(g_sim.get(bk)->pos + Vec2(0, 60), 10));
            g_sim.boardQuiet(*g_sim.get(dz), *g_sim.get(bk));
            CHECK5(g_sim.get(dz)->carrier == bk, "the dozer did not go aboard the bunker");
        }
        for (int k = 0; k < 6; k++) {
            runAi(30);
            c = census(1);
            if (verbose) { snprintf(tag, sizeof tag, "%s d%d bunker +%ds", FACTION_NAME[me], diff, 30 * (k + 1)); printCensus(tag, c); }
        }
        snprintf(tag, sizeof tag, "%s d%d bunker + dozer, 3 min", FACTION_NAME[me], diff); printCensus(tag, c);
        CHECK5(c.hq >= 1, "a computer army whose last dozer sat in a bunker did not rebuild its Command structure");
        CHECK5(c.hubs >= 1, "a computer army whose last dozer sat in a bunker did not restore its supply line");
    }
    if (g_ok5) printf("recoverytest: ok\n");
    return g_ok5;
}
