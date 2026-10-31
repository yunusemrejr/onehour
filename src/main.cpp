// One Hour - entry point
#include "ui.h"
#include "brain.h"
#include "artb.h"
#include "art.h"
#include "art2.h"
#include "testutil.h"
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
           "  --evalai [N]     current commander vs the previous generation of the AI, N games (uses --d0 as difficulty)\n"
           "  --evaldiff [N]   difficulty ladder check: ONEHOUR_DA vs ONEHOUR_DB (0 easy .. 3 brutal), N games\n"
           "  --seed S         random seed for the game\n"
           "  --scenario, --econtest, --areatest, --hqtest, --jettest, --rulestest, --stealthtest, --dozertest, --recoverytest, --civictest, --walltest, --autotest, --bombtest, --supporttest, --droptest, --striketest, --supersonictest, --radtest, --forttest, --updatetest, --lakeupdatetest, --superupdatetest, --civiltest, --oceantest, --fleettest, --bunkertest, --learnupdatetest, --chosentest, --braintest, --uitest   headless gameplay tests\n"
           "  --bench [N]      time N rendered frames of a busy battle (software renderer)\n"
           "  --latebench [M]  a whole four-army match for M minutes, a rendered frame each game second: per-minute tick and frame costs\n"
           "  --micromatrix    the unit behaviours (targeting, focus, kiting) against plain units in mirrored battles and assaults\n"
           "  --variety [N]    the same match N times, only the commanders' own random choices changed: how much their attacks vary (ONEHOUR_VARYMASK2)\n"
           "  --faction c|k    your faction for --shot/--selftest\n");
}

static bool selfTest(int seconds, u64 seed, int players, int d0, bool swap) {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER };
    if (swap) { fac[0] = F_CLANKER; fac[1] = F_CYBER; }
    if (getenv("ONEHOUR_MIRROR")) { Faction m = (Faction)atoi(getenv("ONEHOUR_MIRROR")); fac[0] = fac[1] = m; }
    bool ai[4] = { true, true, true, true };
    int diff[4] = { d0, 3, 2, 1 };
    int team[4] = { 0, 1, 2, 3 };
    if (const char* tm = getenv("ONEHOUR_TEAMS")) sscanf(tm, "%d,%d,%d,%d", &team[0], &team[1], &team[2], &team[3]);   // e.g. 0,0,1,1 for two against two
    if (const char* dm = getenv("ONEHOUR_DIFFS")) sscanf(dm, "%d,%d,%d,%d", &diff[0], &diff[1], &diff[2], &diff[3]);
    g_sim.init(players, fac, ai, diff, team, seed);
    g_ai.init(seed);
    int ticks = seconds * SIM_HZ;
    auto t0 = std::chrono::steady_clock::now();
    int maxEnts = 0;
    // optional stuck-unit report (ONEHOUR_STUCK=1): a unit that holds a movement-type order without moving for a long while is a pathing or order bug
    const bool stuckCheck = getenv("ONEHOUR_STUCK") != nullptr;
    struct StuckRec { u32 gen = 0; Vec2 pos; int since = 0; bool reported = false; int order = -1; Ref tgt; };
    std::vector<StuckRec> stuckRecs; int stuckReports = 0; int traceIdx = -1, traceUntil = 0; int hoverReports = 0; std::vector<u32> seenGen; std::vector<float> prevAir;
    for (int t = 0; t < ticks && !g_sim.gameOver; t++) {
        g_sim.step();
        g_ai.update();
        g_sim.events.clear();
        if (const char* tr = getenv("ONEHOUR_TRACE")) { int ti = 0, t0 = 0, t1 = 0; sscanf(tr, "%d,%d,%d", &ti, &t0, &t1); if (t >= t0 * SIM_HZ && t < t1 * SIM_HZ) { traceIdx = ti; traceUntil = t + 2; } }
        if (traceIdx >= 0 && t < traceUntil && t % 30 == 0 && getenv("ONEHOUR_TRACENEAR")) { Entity& e = g_sim.ents[traceIdx]; for (auto& o : g_sim.ents) if (o.alive && &o != &e && o.kind != EK_RESOURCE && g_sim.distToEntity(e.pos, o) < 48) fprintf(stderr, "    near: %s P%d %s order %d pos %.1f,%.1f\n", o.isUnit() ? o.ut().name : o.bt().name, o.owner, o.isUnit() ? "unit" : "bld", (int)o.order, o.pos.x, o.pos.y); int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y); for (int dy = -3; dy <= 4; dy++) { fprintf(stderr, "    "); for (int dx = -4; dx <= 4; dx++) { int x = cx + dx, y = cy + dy; fputc(!inMap(x, y) ? ' ' : (dx == 0 && dy == 0) ? '@' : g_map.passable(x, y) ? '.' : (g_map.blocked[y * MAP_W + x] & 2) ? 'B' : (g_map.blocked[y * MAP_W + x] & 4) ? 'R' : '#', stderr); } fputc('\n', stderr); } }
        if (traceIdx >= 0 && t < traceUntil && t % (getenv("ONEHOUR_TRACEALL") ? 1 : 3) == 0) { Entity& e = g_sim.ents[traceIdx]; Entity* tg = g_sim.get(e.targetEnt); if (e.isUnit() && e.ut().naval) { int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y); fprintf(stderr, "  ship: airspeed %.1f angle %.2f nav (rows y-1..y+1, cols x-1..x+1):", e.airspeed, e.angle); for (int dy = -1; dy <= 1; dy++) { fprintf(stderr, " "); for (int dx = -1; dx <= 1; dx++) fputc(g_map.navTile(cx + dx, cy + dy) ? 'w' : (g_map.tiles[(cy + dy) * MAP_W + cx + dx] == T_WATER ? 'S' : '#'), stderr); } fprintf(stderr, "  wp %.0f,%.0f\n", e.pathIdx < e.path.size() ? e.path[e.pathIdx].x : -1.0f, e.pathIdx < e.path.size() ? e.path[e.pathIdx].y : -1.0f); }
            if (e.isUnit() && e.isAir()) { const Entity* hh = e.home.idx >= 0 && e.home.idx < (int)g_sim.ents.size() ? &g_sim.ents[e.home.idx] : nullptr; fprintf(stderr, "  air: order %d zoneR %.0f loiterUntil %.1f alt %.2f airspeed %.0f ammo %d hp %.0f disabled %.1f home %d (idx %d gen %u/%u alive %d %s dist %.0f) ang %.2f\n", (int)e.order, e.zoneR, e.loiterUntil - g_sim.time, e.alt, e.airspeed, e.ammo, e.hp, e.disabledUntil - g_sim.time, g_sim.get(e.home) ? 1 : 0, e.home.idx, e.home.gen, hh ? hh->gen : 0, hh ? (int)hh->alive : -1, hh && hh->kind == EK_BUILDING ? hh->bt().name : "-", hh ? dist(hh->pos, e.pos) : -1.0f, e.angle); } fprintf(stderr, "  trace t=%d order %d post %d pos %.1f,%.1f path %zu/%zu target %s(%d) cooldown %.2f stuck %.2f cargo %d act %.2f\n", t, (int)e.order, (int)e.postOrder, e.pos.x, e.pos.y, e.pathIdx, e.path.size(), tg ? (tg->isBuilding() ? tg->bt().name : tg->kind == EK_RESOURCE ? "pile" : tg->ut().name) : "-", tg ? tg->amount : 0, e.cooldown, e.stuckTimer, e.cargo, e.actionTimer); }
        if (stuckCheck && t % 40 == 0) {
            if (stuckRecs.size() < g_sim.ents.size()) stuckRecs.resize(g_sim.ents.size());
            for (size_t i = 0; i < g_sim.ents.size(); i++) {
                Entity& e = g_sim.ents[i]; StuckRec& r = stuckRecs[i];
                if (!e.alive || !e.isUnit() || r.gen != e.gen || r.order != (int)e.order || r.tgt != e.targetEnt || dist(r.pos, e.pos) > 6.0f) { r.gen = e.gen; r.pos = e.pos; r.since = t; r.reported = false; r.order = (int)e.order; r.tgt = e.targetEnt; continue; }
                if (e.disabledUntil > g_sim.time || r.reported || t - r.since < 30 * SIM_HZ) continue;
                bool bad = false;
                if (e.order == O_MOVE || e.order == O_ATTACKMOVE || e.order == O_RETURN) bad = dist(e.pos, e.target) > 40 && e.order != O_RETURN;
                if (e.order == O_ATTACK && !e.isAir()) { Entity* tg = g_sim.get(e.targetEnt); if (tg && g_sim.distToEntity(e.pos, *tg) / TILE > WEAPONS[e.ut().weapon].range + 0.5f) bad = true; }
                if (e.order == O_BUILD) { Entity* tg = g_sim.get(e.targetEnt); if (tg && g_sim.distToEntity(e.pos, *tg) > 26.0f + 4) bad = true; }
                if (e.order == O_HARVEST) { Entity* tg = g_sim.get(e.targetEnt); if (tg && g_sim.distToEntity(e.pos, *tg) > 34.0f + 4) bad = true; }
                if (e.isAir() && e.order == O_ATTACK) bad = true;
                if (bad) { r.reported = true; if (stuckReports++ < 40) {
                    fprintf(stderr, "STUCK idx %zu t=%ds P%d %s order %d at %.0f,%.0f (target %.0f,%.0f) for %ds, cargo %d path %zu/%zu actionTimer %.1f", i, t / SIM_HZ, e.owner, e.ut().name, (int)e.order, e.pos.x, e.pos.y, e.target.x, e.target.y, (t - r.since) / SIM_HZ, e.cargo, e.pathIdx, e.path.size(), e.actionTimer);
                    if (e.pathIdx < e.path.size()) fprintf(stderr, " wp %.0f,%.0f (tile passable %d)", e.path[e.pathIdx].x, e.path[e.pathIdx].y, (int)g_map.passable(tileOf(e.path[e.pathIdx].x), tileOf(e.path[e.pathIdx].y)));
                    fprintf(stderr, " here passable %d stuckTimer %.1f", (int)g_map.passable(tileOf(e.pos.x), tileOf(e.pos.y)), e.stuckTimer);
                    if (e.order == O_RETURN) { Entity* sb = nullptr; float bd = 1e9f; for (auto& b : g_sim.ents) if (b.alive && b.isBuilding() && b.owner == e.owner && b.constructed && b.bt().role == BR_SUPPLY) { float d = g_sim.distToEntity(e.pos, b); if (d < bd) { bd = d; sb = &b; } } fprintf(stderr, " nearest supply %.0fpx %s", bd, sb ? "yes" : "none"); }
                    fprintf(stderr, "\n");
                    if (stuckReports == 1 && getenv("ONEHOUR_STUCKMAP")) { traceIdx = (int)i; traceUntil = t + 40; }
                    if (stuckReports <= 0 && getenv("ONEHOUR_STUCKMAP")) {
                        int cx = tileOf(e.pos.x), cy = tileOf(e.pos.y);
                        for (int dy = -9; dy <= 9; dy++) { for (int dx = -14; dx <= 14; dx++) { int x = cx + dx, y = cy + dy; char c = !inMap(x, y) ? ' ' : (g_map.passable(x, y) ? '.' : (g_map.terrainPassable(x, y) ? 'B' : '#')); for (auto& o : g_sim.ents) if (o.alive && o.isUnit() && !o.isAir() && tileOf(o.pos.x) == x && tileOf(o.pos.y) == y) c = (&o == &e) ? '@' : 'u'; fputc(c, stderr); } fputc('\n', stderr); }
                    } } }
            }
        }
        int alive = 0;
        for (auto& e : g_sim.ents) {
            if (!e.alive) continue;
            alive++;
            if (!(e.pos.x == e.pos.x) || !(e.pos.y == e.pos.y)) { fprintf(stderr, "NaN position at tick %d\n", t); return false; }
            if (e.pos.x < 0 || e.pos.y < 0 || e.pos.x > WORLD_W || e.pos.y > WORLD_H) { fprintf(stderr, "entity out of world at tick %d\n", t); return false; }
            // (an infiltrator may be out on the water: it swims)
            if (e.isUnit() && !e.isAir() && e.fall <= 0 && !e.carrier.valid() && (e.ut().naval ? !g_map.navTile(tileOf(e.pos.x), tileOf(e.pos.y)) : !(g_map.terrainPassable(tileOf(e.pos.x), tileOf(e.pos.y)) || (Sim::swimmer(e) && g_map.swimTile(tileOf(e.pos.x), tileOf(e.pos.y)))))) { fprintf(stderr, "ground unit on impassable terrain at tick %d (%s idx %d P%d order %d at %.1f,%.1f prev %.1f,%.1f fall %.2f)\n", t, e.ut().name, (int)(&e - &g_sim.ents[0]), e.owner, (int)e.order, e.pos.x, e.pos.y, e.prevPos.x, e.prevPos.y, e.fall); return false; }
            if (e.hp > e.maxHp + 0.01f) { fprintf(stderr, "hp above max at tick %d\n", t); return false; }
            // a fixed-wing aircraft away from its airfield never flies slower than its stall speed (it circles instead of hovering)
            size_t ix = (size_t)(&e - &g_sim.ents[0]);
            if (seenGen.size() < g_sim.ents.size()) seenGen.resize(g_sim.ents.size(), 0);
            bool fresh = seenGen[ix] != e.gen; seenGen[ix] = e.gen;   // (an aircraft released from a cargo plane this tick has not flown yet)
            if (e.isUnit() && e.isAir() && !e.ut().heli && e.alt >= 0.99f && e.disabledUntil <= g_sim.time && t > 0 && !fresh) {
                const Entity* h = g_sim.get(e.home);
                float v = dist(e.pos, e.prevPos) / SIM_DT;
                float stall = e.ut().speed * AIR_STALL;
                if (prevAir.size() < g_sim.ents.size()) prevAir.resize(g_sim.ents.size(), 0.0f);
                bool approach = (e.order == O_IDLE || e.order == O_REARM) && e.airspeed < stall;   // landing, or going round again after losing its airfield
                bool climbing = e.airspeed < stall && e.airspeed > prevAir[ix] + 0.5f;                // taking off (from a pad that was just destroyed, say)
                prevAir[ix] = e.airspeed;
                if (v < stall * 0.8f && !approach && !climbing && (!h || dist(e.pos, h->pos) > 13.0f * TILE)) { if (hoverReports++ < 5) fprintf(stderr, "fixed-wing %s idx %d P%d hanging in the air at tick %d (speed %.0f px/s, order %d, airspeed %.0f, pos %.0f,%.0f zone %.0f,%.0f r %.0f ang %.2f)\n", e.ut().name, (int)(&e - &g_sim.ents[0]), e.owner, t, v, (int)e.order, e.airspeed, e.pos.x, e.pos.y, e.zone.x, e.zone.y, e.zoneR, e.angle); if (hoverReports <= 5) fprintf(stderr, "   home %s at %.0f px\n", h ? "yes" : "none", h ? dist(e.pos, h->pos) : -1.0f); }
            }
        }
        if (hoverReports > 0 && t == ticks - 1) { fprintf(stderr, "%d fixed-wing hovering reports\n", hoverReports); return false; }
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
    {
        int rioters = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == RIOT_PLAYER) rioters++;
        printf("  riots: %d crowds raised, %d rioters on the map now; towns:", g_sim.riotSeq, rioters);
        for (auto& D : g_sim.districts) printf(" %s P%d %d%%%s", D.name, D.owner, (int)(D.integrity * 100), D.ruins ? "*" : "");
        printf("\n");
    }
    for (int p = 0; p < g_sim.numPlayers; p++) {
        Player& pl = g_sim.players[p];
        int ubase = firstUnitOf(pl.faction);
        {   // what is left of the army at the end (an undecided game: what keeps the losers alive)
            int st = 0, civ = 0, hq = 0, bunk = 0, un = 0, dz = 0, cdz = 0, air = 0, ship = 0;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == p) {
                if (e.isBuilding()) { st++; BuildRole r = e.bt().role; if (r == BR_CIVIC) civ++; else if (r == BR_HQ) hq++; else if (r == BR_BUNKER) bunk++; }
                else if (e.isUnit()) { un++; if (e.ut().role == UR_DOZER) (civicDozerType(e.type) ? cdz : dz)++; else if (e.isAir()) air++; else if (e.ut().naval) ship++; }
            }
            printf("  P%d: left at the end: structures %d (town %d, hq %d, bunkers %d), units %d (dozers %d + civil %d, aircraft %d, ships %d), $%d", p, st, civ, hq, bunk, un, dz, cdz, air, ship, pl.money);
            if (st > 0 && st <= 3) for (auto& e : g_sim.ents) if (e.alive && e.owner == p && e.isBuilding()) printf(" [%s%s %.0f%%]", e.bt().name, e.constructed ? "" : " site", 100.0f * e.hp / e.maxHp);
            printf("\n");
        }
        printf("  P%d: team %d diff %d doctrine %s, mined $%d, income structures %d, nuke ramps %d, town %d, deep silos %d, nuke dodges %d, medics built %d, upgrades %d%d%d\n", p, pl.team, pl.difficulty, DOCTRINE_NAME[g_ai.ais[p].doctrine], pl.mined, g_sim.countRole(p, BR_INCOME, false), g_sim.countRole(p, BR_NUKE, false), g_sim.countRole(p, BR_CIVIC, false), g_sim.countRole(p, BR_UNUKE, false), g_ai.ais[p].dodges, g_ai.ais[p].medicsBuilt, (int)pl.upg[UPG_RUGGED], (int)pl.upg[UPG_GUNS], (int)pl.upg[UPG_REPAIR]);
        printf("  P%d: built %d lost %d kills %d structures killed %d harvested %d alive=%d | program %d, elites bought %d/%d, titans %d, drones/gunships %d, jets %d\n", p, pl.unitsBuilt, pl.unitsLost, pl.unitsKilled, pl.structuresKilled, pl.harvested, (int)pl.alive,
               (int)pl.advTech, (int)(pl.spentOn[ubase + 9] / UNITS[ubase + 9].cost), (int)(pl.spentOn[ubase + 9] > 0), (int)(pl.spentOn[ubase + 10] / UNITS[ubase + 10].cost), (int)(pl.spentOn[ubase + 8] / UNITS[ubase + 8].cost), (int)(pl.spentOn[ubase + 11] / UNITS[ubase + 11].cost));
        {   // the October update's strategic assets: launchers taken and fired, Nuclear Bombers called, fighters bought, fortresses held
            int ft = pl.faction == F_CYBER ? U_C_FIGHTER : U_K_FIGHTER, forts = 0, held = 0;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == p && e.isBuilding()) { if (e.type == B_C_FORTRESS) forts++; if (e.bt().role == BR_ISLANDNUKE) held++; }
            printf("  P%d: island launchers taken %d (held now %d), island nukes %d, Nuclear Bombers called %d, fighters bought %d, fortresses held %d\n", p, pl.launchersTaken, held, pl.islandLaunches, pl.bomberCalls, (int)(pl.spentOn[ft] / UNITS[ft].cost), forts);
        }
        {   // the towns round the lake and the riots
            int towns = 0; for (auto& D : g_sim.districts) if (D.owner == p) towns++;
            const AiPlayer& a = g_ai.ais[p];
            printf("  P%d: %s (%s), towns held %d (defences %d, rebuilds %d), joint attacks %d, riots against it %d, order now %.2f (unrest %.2f)\n", p, a.commander, PERSONAS[a.persona].title, towns, a.townDefences, a.townRebuilds, a.teamJoins, g_sim.riotsRaised[p], g_sim.control[p], g_sim.unrest[p]);
        }
        {   // the special forces: spies, spy drones, snipers (ids sit after both armies' blocks)
            int sp = pl.faction == F_CYBER ? U_C_SPY : U_K_SPY, sd = pl.faction == F_CYBER ? U_C_SDRONE : U_K_SDRONE, sn = pl.faction == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
            int caps = pl.structuresKilled;   // (captures count as structures taken)
            printf("      special forces bought: spies %d, spy drones %d, snipers %d (structures taken or destroyed %d)\n", (int)(pl.spentOn[sp] / UNITS[sp].cost), (int)(pl.spentOn[sd] / UNITS[sd].cost), (int)(pl.spentOn[sn] / UNITS[sn].cost), caps);
            {   // The Chosen (the update's own line: what it did, and how the commander fared against an enemy's)
                int ct = pl.faction == F_CYBER ? U_C_CHOSEN : U_K_CHOSEN, ht = pl.faction == F_CYBER ? U_C_HELI : U_K_AIR;
                printf("      The Chosen: trained %d, Shadow Guard calls %d, units turned %d, structures taken %d (sold %d); lost to an enemy's: %d units; helicopters %d, hunts %d, feints %d\n",
                       (int)(pl.spentOn[ct] / UNITS[ct].cost), pl.shadowCalls, pl.converted, pl.chosenTaken, g_ai.ais[p].chosenSold, pl.convertedLost, (int)(pl.spentOn[ht] / UNITS[ht].cost), g_ai.ais[p].heliHunts, g_ai.ais[p].feints);
            }
            int jp = pl.faction == F_CYBER ? U_C_JEEP : U_K_JEEP, ds = pl.faction == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, cr = pl.faction == F_CYBER ? U_C_CRUISER : U_K_CRUISER;
            printf("      navy and jeeps: destroyers %d, cruisers %d (yards %d), jeeps %d, anti-nuke defences %d, guard drones %d\n", (int)(pl.spentOn[ds] / UNITS[ds].cost), (int)(pl.spentOn[cr] / UNITS[cr].cost), g_sim.countRole(p, BR_NAVAL, false), (int)(pl.spentOn[jp] / UNITS[jp].cost), g_sim.countRole(p, BR_ANTINUKE, false), (int)pl.upg[UPG_DRONES]);
        }
        if (getenv("ONEHOUR_DEBUG")) {
            printf("      value dealt per credit:");
            for (int u = 0; u < U_COUNT; u++) if (pl.spentOn[u] > 0) printf(" %s %.2f (spent %d)", UNITS[u].name, pl.valueDealt[u] / pl.spentOn[u], (int)pl.spentOn[u]);
            printf("\n");
        }
    }
    return true;
}


static int g_playMicro[4] = { -1, -1, -1, -1 };   // Player::micro per army for playAiGame (-1: the default)
// Plays one AI-only game headless. Returns the winning team (or -1 when nobody won within maxSecs).
static int playAiGame(u64 seed, int players, const Faction* fac, const bool* brain, int difficulty, int maxSecs, const int* teamIn = nullptr, const bool* smart = nullptr, const int* diffIn = nullptr, const bool* pro = nullptr) {
    bool ai[4] = { true, true, true, true };
    int diff[4] = { difficulty, difficulty, difficulty, difficulty };
    int team[4] = { 0, 1, 2, 3 };
    if (teamIn) for (int i = 0; i < 4; i++) team[i] = teamIn[i];
    if (diffIn) for (int i = 0; i < 4; i++) diff[i] = diffIn[i];
    g_sim.init(players, fac, ai, diff, team, seed);
    for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = brain[p]; g_ai.smartEnabled[p] = smart ? smart[p] : true; g_ai.proEnabled[p] = pro ? pro[p] : true; }
    for (int p = 0; p < 4; p++) if (g_playMicro[p] >= 0) g_sim.players[p].micro = g_playMicro[p];   // (--evalai with ONEHOUR_OLDMICRO: the baseline's units without the micro update)
    g_ai.init(seed);
    for (int t = 0; t < maxSecs * SIM_HZ && !g_sim.gameOver; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
    if (g_sim.gameOver) g_ai.finish(g_sim.winnerTeam);
    else g_ai.left();   // (an unresolved match still teaches the opponent model what it showed)
    return g_sim.gameOver ? g_sim.winnerTeam : -1;
}

// How predictable a commander is: the same match (same map, seed and armies) played N times, only the commanders' own random choices changed
// (g_aiSalt). For army 0 it prints when its first waves went and what they went for, then how much that varied: the distinct objectives of the
// first wave (by kind and place), the distinct orders of the first three, and the spread of the first wave's time. ONEHOUR_VARYMASK2 sets both
// commanders' AiFeat2 bits (default: what ships).
extern int g_aiSalt;
static int varietyTest(int n, u64 seed, int difficulty) {
    const int mask2 = getenv("ONEHOUR_VARYMASK2") ? atoi(getenv("ONEHOUR_VARYMASK2")) : F2_DEFAULT;
    std::vector<std::string> firsts, seqs; std::vector<float> times; int wins = 0;
    for (int k = 0; k < n; k++) {
        g_aiSalt = k + 1;
        Faction fac[4] = { F_CYBER, F_CLANKER, F_CYBER, F_CLANKER }; bool brain[4] = { false, false, false, false };
        for (int p = 0; p < 4; p++) { g_ai.featMask2[p] = mask2; g_ai.brainOverride[p] = nullptr; }
        int w = playAiGame(seed, 2, fac, brain, difficulty, 900);
        const AiPlayer& a = g_ai.ais[0];
        char f[64] = "none", s[160] = "";
        if (a.wavesLogged > 0) snprintf(f, sizeof f, "%d@%d,%d", a.waveRole[0], tileOf(a.waveLog[0].x) / 8, tileOf(a.waveLog[0].y) / 8);
        for (int i = 0; i < std::min(3, a.wavesLogged); i++) { char b[40]; snprintf(b, sizeof b, "%s%d@%d,%d", i ? ">" : "", a.waveRole[i], tileOf(a.waveLog[i].x) / 8, tileOf(a.waveLog[i].y) / 8); strcat(s, b); }
        printf("  run %2d: first wave %4.0f s -> %s | first three %s | %s\n", k + 1, a.firstWaveT, f, s, w == 0 ? "army 0 won" : (w < 0 ? "unresolved" : "army 1 won"));
        firsts.push_back(f); seqs.push_back(s); if (a.firstWaveT >= 0) times.push_back(a.firstWaveT); if (w == 0) wins++;
    }
    auto distinct = [](std::vector<std::string> v) { std::sort(v.begin(), v.end()); return (int)(std::unique(v.begin(), v.end()) - v.begin()); };
    double m = 0, sd = 0; for (float t : times) m += t; m /= std::max<size_t>(1, times.size()); for (float t : times) sd += (t - m) * (t - m); sd = std::sqrt(sd / std::max<size_t>(1, times.size()));
    printf("variety (mask2 %d, %d runs): %d distinct first objectives, %d distinct first-three orders, first wave at %.0f s +- %.0f s, army 0 won %d\n", mask2, n, distinct(firsts), distinct(seqs), m, sd, wins);
    return 0;
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
        int diffs[4]; for (int k = 0; k < 4; k++) diffs[k] = r.range(1, 3);   // every army its own difficulty, like in the menu
        int teams[4] = { 0, 1, 2, 3 };
        if (n == 4 && r.range(0, 1)) { teams[1] = 0; teams[3] = 2; teams[2] = 2; }          // two against two
        else if (n >= 3 && r.range(0, 2) == 0) { teams[1] = 0; }                           // an allied pair against the rest
        int w = playAiGame(seed * 1000 + i, n, fac, brain, 2, 1500, teams, nullptr, diffs);
        g_brain.games++;
        if ((i + 1) % 10 == 0 || i + 1 == games) {
            printf("game %3d players %d winner team %2d | waves learned %d | wave weights", i + 1, n, w, g_brain.waveSamples);
            for (int k = 0; k < WAVE_F; k++) printf(" %.2f", g_brain.ww[k]);
            printf("\n   doctrines:");
            for (int d = 0; d < DOCTRINES; d++) printf(" %s %.2f (%d)", DOCTRINE_NAME[d], g_brain.docQ[d], g_brain.docN[d]);
            for (int c = 0; c < DOC_CTX; c++) { printf("\n   vs %s %s:", c >= 2 ? "human" : "computer", c & 1 ? "Clanker" : "Cyber"); for (int d = 0; d < DOCTRINES; d++) printf(" %s %.2f (%d)", DOCTRINE_NAME[d], g_brain.ctxQ[c][d], g_brain.ctxN[c][d]); }
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


// Evaluation of the commander itself: the current AI (player 0 or 1, alternating) against the previous generation, difficulty and factions rotated.
static int evalAi(int games, u64 seed, int difficulty) {
    std::string path = Brain::defaultPath();
    if (!path.empty() && g_brain.load(path.c_str())) printf("using %s (%d games)\n", path.c_str(), g_brain.games);
    g_brain.learning = false;
    g_brain.profileLearning = getenv("ONEHOUR_EVALPROFILE") != nullptr;   // FEAT_PROFILE's opponent model learns over the series (nothing else does)
    int wins = 0, losses = 0, draws = 0, tieWins = 0, tieLosses = 0, punishes = 0; double winTime = 0, loseTime = 0;
    for (int i = 0; i < games; i++) {
        if (getenv("ONEHOUR_EVALGAME") && i != atoi(getenv("ONEHOUR_EVALGAME"))) continue;   // (diagnostics: replay one game of the series)
        Faction fac[4] = { (Faction)(i & 1), (Faction)((i >> 1) & 1), F_CYBER, F_CLANKER };
        bool flip = (i >> 2) & 1;
        bool brainOn[4] = { true, true, true, true };
        // the new commander (pro) against the previous generation (smart but not pro); ONEHOUR_EVALOLD=legacy plays the oldest one instead
        static const bool legacy = getenv("ONEHOUR_EVALOLD") && getenv("ONEHOUR_EVALOLD")[0] == 'l';
        bool smart[4] = { legacy ? !flip : true, legacy ? flip : true, true, true };
        bool pro[4] = { !flip, flip, true, true };
        static const int baseMask = getenv("ONEHOUR_BASEMASK") ? atoi(getenv("ONEHOUR_BASEMASK")) : FEAT_DEFAULT;   // the baseline side plays with only these pro features
        static const int newMask = getenv("ONEHOUR_NEWMASK") ? atoi(getenv("ONEHOUR_NEWMASK")) : FEAT_DEFAULT;   // and the new side only these
        for (int p = 0; p < 4; p++) g_ai.featMask[p] = (p == (flip ? 0 : 1)) ? baseMask : newMask;
        static const int baseMask2 = getenv("ONEHOUR_BASEMASK2") ? atoi(getenv("ONEHOUR_BASEMASK2")) : F2_DEFAULT, newMask2 = getenv("ONEHOUR_NEWMASK2") ? atoi(getenv("ONEHOUR_NEWMASK2")) : F2_DEFAULT;   // (the second feature set, AiFeat2)
        for (int p = 0; p < 4; p++) g_ai.featMask2[p] = (p == (flip ? 0 : 1)) ? baseMask2 : newMask2;
        // ONEHOUR_BRAIN2=path|prior: the baseline side consults a different brain (a saved one, or the untrained prior), to measure what learning is worth
        static Brain baseBrain; static bool baseLoaded = false;
        if (!baseLoaded) { baseLoaded = true; const char* b2 = getenv("ONEHOUR_BRAIN2"); if (b2 && strcmp(b2, "prior") != 0) baseBrain.load(b2); baseBrain.learning = false; }
        for (int p = 0; p < 4; p++) g_ai.brainOverride[p] = (getenv("ONEHOUR_BRAIN2") && p == (flip ? 0 : 1)) ? &baseBrain : nullptr;
        if (getenv("ONEHOUR_BASEMASK") || getenv("ONEHOUR_NEWMASK") || getenv("ONEHOUR_BASEMASK2") || getenv("ONEHOUR_NEWMASK2") || getenv("ONEHOUR_BRAIN2")) { pro[0] = pro[1] = true; }
        static const int oldDiff = getenv("ONEHOUR_OLDDIFF") ? atoi(getenv("ONEHOUR_OLDDIFF")) : difficulty;   // the baseline may play at another difficulty
        static const bool oldMicro = getenv("ONEHOUR_OLDMICRO") != nullptr;   // the baseline's units without the micro update (Player::micro 0)
        for (int p = 0; p < 4; p++) g_playMicro[p] = (oldMicro && p == (flip ? 0 : 1)) ? 0 : -1;
        int diffs[4] = { flip ? oldDiff : difficulty, flip ? difficulty : oldDiff, difficulty, difficulty };
        static const bool teams2v2 = getenv("ONEHOUR_EVALTEAMS") != nullptr;   // 2 against 2: two new commanders (armies 0 and 2) against two previous ones (1 and 3)
        int w;
        if (teams2v2) {
            Faction fac4[4] = { fac[0], fac[1], fac[1], fac[0] }; bool pro4[4] = { !flip, flip, !flip, flip }; int team4[4] = { 0, 1, 0, 1 };
            for (int p = 0; p < 4; p++) { g_ai.featMask[p] = (pro4[p] ? newMask : baseMask); g_ai.featMask2[p] = (pro4[p] ? newMask2 : baseMask2); g_ai.brainOverride[p] = nullptr; }
            w = playAiGame(seed + i * 31, 4, fac4, brainOn, difficulty, 1500, team4, nullptr, nullptr, pro4);
        } else
        w = playAiGame(seed + i * 31, 2, fac, brainOn, difficulty, 1500, nullptr, smart, diffs, pro);
        int newTeam = flip ? 1 : 0;
        punishes += g_ai.ais[newTeam].punishes;
        const char* res;
        if (w < 0) {   // (unresolved games are most of the series now: the material each side still holds at the limit is printed as a tiebreak, not counted as a win)
            draws++; res = "unresolved";
            double mat[2] = { 0, 0 };
            for (auto& e : g_sim.ents) if (e.alive && e.owner >= 0 && e.owner < 2 && e.kind != EK_RESOURCE) mat[e.owner] += (e.isUnit() ? UNITS[e.type].cost : BUILDS[e.type].cost) * (e.hp / std::max(1.0f, e.maxHp));
            double nm = mat[newTeam], om = mat[1 - newTeam];
            if (nm > om * 1.1) tieWins++; else if (om > nm * 1.1) tieLosses++;
            printf("    at the limit: material new %.0f vs old %.0f\n", nm, om);
        }
        else if (w == newTeam) { wins++; winTime += g_sim.time; res = "new AI wins"; }
        else { losses++; loseTime += g_sim.time; res = "old AI wins"; }
        if (getenv("ONEHOUR_EVALGAME")) for (int p = 0; p < 2; p++) {   // the final position: who has what
            Player& pl = g_sim.players[p]; int inf = 0, veh = 0, air = 0, dz = 0, hv = 0;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == p && e.isUnit()) { if (e.ut().role == UR_DOZER) dz++; else if (e.ut().role == UR_HARVESTER) hv++; else if (e.isAir()) air++; else if (e.ut().kind == UK_INF) inf++; else veh++; }
            printf("    P%d %s: $%d inf %d veh %d air %d dozers %d haulers %d | structures %d (factories %d barracks %d turrets %d) alive %d wave=%d\n", p, p == (flip ? 1 : 0) ? "new" : "old", pl.money, inf, veh, air, dz, hv, g_sim.countBuildings(p, -1, false), g_sim.countRole(p, BR_FACTORY, false), g_sim.countRole(p, BR_BARRACKS, false), g_sim.countRole(p, BR_TURRET, false) + g_sim.countRole(p, BR_AATURRET, false), (int)pl.alive, (int)g_ai.ais[p].attacking);
        }
        printf("  game %2d (%s%s vs %s%s): %s at %.0f s, doctrine %s\n", i + 1, flip ? "old " : "new ", FACTION_NAME[fac[0]], flip ? "new " : "old ", FACTION_NAME[fac[1]], res, g_sim.time, DOCTRINE_NAME[g_ai.ais[flip ? 1 : 0].doctrine]);
        if (getenv("ONEHOUR_EVALCHOSEN")) for (int p = 0; p < 2; p++) {   // (what each side's Chosen did)
            const Player& pl = g_sim.players[p]; const AiPlayer& a = g_ai.ais[p];
            printf("    P%d %s: chosen bought %d first %.0f, calls %d, turned %d, taken %d, sold %d, lost to the enemy's %d, hunts %d, feints %d\n", p, p == newTeam ? "new" : "old", a.chosenBought, a.chosenFirstAt, pl.shadowCalls, pl.converted, pl.chosenTaken, a.chosenSold, pl.convertedLost, a.heliHunts, a.feints);
        }
        fflush(stdout);
    }
    printf("new AI vs previous generation: %d wins, %d losses, %d unresolved (of %d)%s\n", wins, losses, draws, games, "");
    printf("tiebreak at the limit (material held, >10%% apart): %d new, %d old\n", tieWins, tieLosses);
    printf("counterattacks by the new commander: %d\n", punishes);
    if (wins) printf("  mean time of its wins %.0f s", winTime / wins);
    if (losses) printf("  mean time of its losses %.0f s", loseTime / losses);
    printf("\n");
    return 0;
}


// Difficulty ladder check: an army at difficulty ONEHOUR_DA against one at ONEHOUR_DB (sides and factions rotated), both with the current commander.
static int evalDiff(int games, u64 seed) {
    int da = getenv("ONEHOUR_DA") ? atoi(getenv("ONEHOUR_DA")) : 3, db = getenv("ONEHOUR_DB") ? atoi(getenv("ONEHOUR_DB")) : 1;
    g_brain.learning = false;
    int winsA = 0, winsB = 0, draws = 0;
    for (int i = 0; i < games; i++) {
        Faction fac[4] = { (Faction)(i & 1), (Faction)((i >> 1) & 1), F_CYBER, F_CLANKER };
        bool flip = (i >> 2) & 1;
        bool brainOn[4] = { true, true, true, true };
        int diffs[4] = { flip ? db : da, flip ? da : db, 1, 1 };
        int w = playAiGame(seed + i * 31, 2, fac, brainOn, 1, 1500, nullptr, nullptr, diffs);
        int aTeam = flip ? 1 : 0;
        if (w < 0) draws++; else if (w == aTeam) winsA++; else winsB++;
    }
    printf("difficulty %d vs difficulty %d: %d wins, %d losses, %d unresolved (of %d)\n", da, db, winsA, winsB, draws, games);
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
            if (until - g_sim.time < SCANS[fac[0]].duration - 1.0f || until - g_sim.time > SCANS[fac[0]].duration + 1.0f || SCANS[fac[0]].duration < 179.0f) { fprintf(stderr, "scenario: scan should last 3 minutes (%.1f)\n", until - g_sim.time); return false; }
            for (int guard = 0; guard < (int)(SIM_HZ * (SCANS[fac[0]].duration + 1.0f)); guard++) { g_sim.step(); g_sim.events.clear(); }
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
    Vec2 c = openGroundNear(g_sim.players[0].basePos + Vec2(560, 560), 7);   // (open mainland ground: the map's middle is the lake's big island now)
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
    // an enemy far outside the zone that never enters it: not chased (once the guards are all back in the circle after chasing the intruder)
    for (int t = 0; t < 20 * 30; t++) {
        bool home = true;
        for (auto r : mine) { Entity* e = g_sim.get(r); if (e && !e->isAir() && dist(e->pos, c) > R + 40) home = false; }
        if (home) break;
        g_sim.step(); g_sim.events.clear();
    }
    Vec2 far = openGroundNear(c + Vec2(19.0f * TILE, 0), 2);   // (well beyond any guard leash: on the old map this spot fell across the lake)
    Ref idler = g_sim.spawnUnit(U_K_TANK, 1, far);
    float hp0 = g_sim.get(idler)->hp;
    for (int t = 0; t < 20 * 25; t++) {
        if (Entity* x = g_sim.get(idler)) { x->disabledUntil = g_sim.time + 1.0f; x->order = O_IDLE; x->targetEnt = NOREF; x->engaged = NOREF; }   // (it stays put and never fires: it never gives the guards a reason)
        g_sim.step(); g_sim.events.clear();
    }
    Entity* id = g_sim.get(idler);
    if (!id || id->hp < hp0) { if (id) { const Entity* a = g_sim.get(id->attacker); fprintf(stderr, "outsider hp %.0f/%.0f, hit by %s at %.0f px from the zone centre\n", id->hp, hp0, a ? a->ut().name : "?", a ? dist(a->pos, c) : -1.0f); } return fail("guards attacked an enemy outside their zone"); }
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
        auto spot = [&](int type, int skipDy) { for (int dy = skipDy; dy < 24; dy++) for (int dx = -16; dx < 16; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } for (int r = 1; r < 80; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) { if (std::max(std::abs(dx), std::abs(dy)) != r) continue; int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } return Vec2(-1, -1); };
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 6, tileOf(b0.y) - 2, true);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 6, tileOf(b0.y) + 1, true);
        g_sim.placeBuilding(bb + BR_POWER, 0, tileOf(b0.x) + 9, tileOf(b0.y) - 2, true);
        int placed = 0;
        for (int i = 0; i < 5; i++) { Vec2 sp = spot(bb + BR_INCOME, 5); if (sp.x < 0) break; inc[i] = g_sim.placeBuilding(bb + BR_INCOME, 0, (int)sp.x, (int)sp.y, true); placed++; if (!g_sim.buildAvailable(0, bb + BR_INCOME)) break; }
        if (placed != INCOME_MAX) return fail("income structure limit not enforced");
        int m0 = pl.money;
        for (int t = 0; t < 20 * 60; t++) { g_sim.step(); g_sim.events.clear(); }
        int gain = pl.money - m0, per = fac[0] == F_CYBER ? INCOME_CYBER : INCOME_CLANKER;
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
        // Pick real ground beyond the blast; nearestFree can otherwise snap a
        // point in the larger lake back onto the island inside the blast.
        Vec2 farAt; float farScore = 1e18f;
        for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++) {
            Vec2 p = tileCenter(x, y);
            if (!g_map.passable(x, y) || dist(p, tgt) < 14 * TILE) continue;
            float d = dist2(p, tgt + Vec2(0, 16 * TILE));
            if (d < farScore) { farScore = d; farAt = p; }
        }
        Ref far = g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, farAt);
        if (dist(g_sim.get(far)->pos, tgt) < 14 * TILE) return fail("far blast fixture is not outside the radius");
        g_sim.get(far)->disabledUntil = 1e9f;
        g_sim.players[1].isAI = true;   // a human army's units see the warhead coming and get out of the circle (see --autotest): test the blast itself
        if (g_sim.nukesReady(0) != 2) return fail("two ramps should both be ready");
        if (!g_sim.cmdNuke(0, tgt)) return fail("nuke launch refused");
        if (g_sim.nukesReady(0) != 1) return fail("a launch should use exactly one ramp");
        for (int t = 0; t < 20 * 9; t++) { g_sim.step(); g_sim.events.clear(); }
        for (auto v : victims) if (g_sim.get(v)) return fail("nuke left a unit alive in the blast");
        if (!g_sim.get(far)) return fail("nuke hit a unit far outside the blast radius");
        if (!g_sim.cmdNuke(0, tgt)) return fail("second ramp refused");
        if (g_sim.cmdNuke(0, tgt)) return fail("nuke fired with no ramp ready");
        for (int t = 0; t < (int)(20 * (NUKE_COOLDOWN - 15.0f)) && !g_sim.gameOver; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.nukesReady(0) != 0) return fail("ramp reloaded before its cooldown");
        for (int t = 0; t < 20 * 25; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.nukesReady(0) != 2) return fail("ramps did not reload after their cooldown");
        // aircraft rally: a spawned aircraft flies to the airfield's rally point and waits there
        Vec2 sa = spot(bb + BR_AIRFIELD, 4); Ref af = g_sim.placeBuilding(bb + BR_AIRFIELD, 0, (int)sa.x, (int)sa.y, true);
        Vec2 rp = g_map.nearestFree(g_sim.get(af)->pos + Vec2(0, 9 * TILE), 30);
        g_sim.cmdSetRally(af, rp);
        pl.money = 100000;
        if (!g_sim.cmdTrain(af, firstUnitOf(fac[0]) + 8)) return fail("aircraft not trainable");
        Ref air;
        for (int t = 0; t < 20 * 40 && !air.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) air = g_sim.refOf(e); }
        if (!air.valid()) return fail("aircraft never produced");
        float closest = 1e9f;   // (a fixed-wing craft circles the rally point: it must pass close to it)
        for (int t = 0; t < 20 * 15; t++) { g_sim.step(); g_sim.events.clear(); if (t >= 20 * 9) if (Entity* a = g_sim.get(air)) closest = std::min(closest, dist(a->pos, rp)); }
        Entity* a = g_sim.get(air);
        // (a fixed-wing craft circles the rally point; the circle radius grows with the aircraft's pad slot so a crowd of them spreads out)
        if (!a || closest > 12 * TILE) { if (a) fprintf(stderr, "%s at %.0f px from the rally point %.0f,%.0f (pos %.0f,%.0f order %d)\n", a->ut().name, dist(a->pos, rp), rp.x, rp.y, a->pos.x, a->pos.y, (int)a->order); return fail("aircraft did not go to the rally point"); }
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
        // (the enemy plays Easy: against a Hard one whose first wave flattened the half-built post and every dozer, nothing could be rebuilt, though
        // the post was placed 4 s after the loss - this checks the rebuilding, not whether the base survives the next attack)
        bool ai[2] = { true, true }; int diff[2] = { 2, 0 }; int team[2] = { 0, 1 };
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


// Rules added in the friendly-fire / sniper / air-power pass: forced fire on friends (humans only), snipers and who can spot them,
// both aircraft kinds fighting aircraft, and the per-structure paradrop cooldown.
bool stealthTest(u64 seed);   // tests2.cpp: stealth, spies, cargo lifters, underground bunkers, rocket infantry
bool navyTest(u64 seed);      // tests3.cpp: the lake, warships, nukes sinking ships
bool jeepTest(u64 seed);
bool droneTest(u64 seed);
bool strikeJetTest(u64 seed);
bool antiNukeTest(u64 seed);
bool dozerTest(u64 seed);     // tests4.cpp: dozer priorities, manual-order hold, kept build sites
bool recoveryTest(u64 seed);  // tests5.cpp: a computer army that has lost almost everything rebuilds
bool civicTest(u64 seed);     // tests6.cpp: the civilian town, civilian dozers, the Underground Nuke and the bunker's watch over its neighbours
bool wallTestMain(u64 seed); // tests7.cpp: walls, Hyper Protection, Anti-Rocket, durability, barracks speed and the 25-pad airfield
bool supersonicTest(u64 seed); // tests8.cpp: the balance pass (anti-nuke/wall vs nukes, silver damage, bunker health) and the Supersonic Bomber
bool radiationTest(u64 seed); // tests9.cpp: the Anti-Radiation Truck (scrubbing fallout, shielding inside its field, and the damage it absorbs)
bool fortTest(u64 seed);     // tests10.cpp: the map's neutral civilian strongholds (claim, garrison, fire, durability)
bool lakeUpdateTest(u64 seed);   // tests13.cpp: infiltrators swim to the island launchers, Waymakers, the patrol boat, the bigger lake, weather
bool civilTest(u64 seed);        // tests15.cpp: the towns round the lake, the riots, the newer commander, difficulty raised mid-match
bool oceanTest(u64 seed);        // tests16.cpp: the island map in the sea, the navy on the ocean, cargo ships, aircraft carriers
bool fleetTest(u64 seed);        // tests17.cpp: the Fleet Call (a battle fleet from the map border), carrier deck operations
bool bunkerUpdateTest(u64 seed); // tests18.cpp: nuclear-hardened bunkers, 200/5 garrison, per-bunker upgrades, shift-drag lines, the smarter Waymaker
bool duelMatrix(u64 seed);       // tests17.cpp: sharp fire against plain fire in mirrored battles
bool microMatrix(u64 seed);
bool chosenTest(u64 seed);        // tests21.cpp: The Chosen and its Shadow Guard (helicopter-only detection, captures, turning units, the call, the AI)
bool learnUpdateTest(u64 seed);   // tests20.cpp: the learning update (opponent model, counterattack, trained numbers, reachable rally)      // tests19.cpp: the unit micro update (Player::micro) against plain units, field battles and assaults
bool superUpdateTest(u64 seed);  // tests14.cpp: the 208 map's four launchers, Super Nukes, ally takeovers by the human, Healing Antennas
bool updateTest(u64 seed);   // tests12.cpp: islands and launchers, fortresses, bastion walls, infiltrator stealth, vegetation, dozer routes, bombers, the Nuclear Bomber
bool moveTest(u64 seed);
void navyShowcase(int vp, Vec2& camOut);   // tests3.cpp: ONEHOUR_NAVY=1 --shot scene
void airShowcase(int vp, const char* which, Vec2& camOut, Ref& follow);   // tests3.cpp: ONEHOUR_AIR=strike|drop|nuke --shot scenes
static bool rulesTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "rulestest: %s\n", m); return false; };
    auto run = [](int sec) { for (int t = 0; t < 20 * sec; t++) { g_sim.step(); g_sim.events.clear(); } };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[3] = { (Faction)fi, (Faction)fi, (Faction)(1 - fi) };
        bool ai[3] = { false, true, true }; int diff[3] = { 1, 1, 1 }; int team[3] = { 0, 0, 1 };
        g_sim.init(3, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 3; p++) { g_sim.players[p].money = 100000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
        Faction me = fac[0], foe = fac[2];
        int tankMe = me == F_CYBER ? U_C_TANK : U_K_TANK, tankFoe = foe == F_CYBER ? U_C_TANK : U_K_TANK;
        int inf1Foe = firstUnitOf(foe) + 2, sniperMe = me == F_CYBER ? U_C_SNIPER : U_K_SNIPER, sniperFoe = foe == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
        Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f));
        auto hpOf = [](Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : 0.0f; };
        // ---- force fire on friends: humans only
        Ref a = g_sim.spawnUnit(tankMe, 0, mid), own = g_sim.spawnUnit(tankMe, 0, mid + Vec2(90, 0)), ally = g_sim.spawnUnit(tankMe, 1, mid + Vec2(0, 90));
        float own0 = hpOf(own), ally0 = hpOf(ally);
        g_sim.cmdAttack({a}, own); g_sim.cmdAttack({a}, ally); run(6);
        if (hpOf(own) < own0 || hpOf(ally) < ally0) return fail("a plain attack order hurt a friend");
        g_sim.cmdAttack({a}, own, true); run(10);
        if (hpOf(own) >= own0) return fail("force fire did not hurt my own tank");
        g_sim.cmdAttack({a}, ally, true); run(14);
        if (hpOf(ally) >= ally0) return fail("force fire did not hurt the ally's tank");
        {   // the same through the UI: Ctrl + right-click on a friendly selects force fire, a plain right-click only moves
            Ref uiTank = g_sim.spawnUnit(tankMe, 0, mid + Vec2(-120, 300)), uiOwn = g_sim.spawnUnit(tankMe, 0, mid + Vec2(-30, 300));
            float u0 = hpOf(uiOwn);
            g_game.selection.clear(); g_game.selection.push_back(uiTank);
            g_game.cam = Vec2(clampf(mid.x - 120 - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(mid.y + 300 - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            SDL_SetModState(KMOD_NONE); g_game.issueRightClick(g_sim.get(uiOwn)->pos); run(8);
            if (hpOf(uiOwn) < u0) return fail("a plain right-click on my own tank attacked it");
            SDL_SetModState(KMOD_LCTRL); g_game.issueRightClick(g_sim.get(uiOwn)->pos); SDL_SetModState(KMOD_NONE); run(12);
            if (hpOf(uiOwn) >= u0) return fail("Ctrl+right-click did not force fire on my own tank");
            g_game.selection.clear();
        }
        Ref ai1 = g_sim.spawnUnit(tankMe, 1, mid + Vec2(-300, 0)), vict = g_sim.spawnUnit(tankMe, 0, mid + Vec2(-210, 0));
        float v0 = hpOf(vict);
        g_sim.cmdAttack({ai1}, vict, true); run(10);
        if (hpOf(vict) < v0) return fail("a computer army attacked its human ally on purpose");
        // a human's nuke flattens everything under it: the allies' units and the human's own, forced or not
        Vec2 gz = g_map.nearestFree(mid + Vec2(500, 300), 30);
        Ref bystander = g_sim.spawnUnit(tankMe, 0, gz), allyTank = g_sim.spawnUnit(tankMe, 1, g_map.nearestFree(gz + Vec2(60, 0), 30));
        g_sim.nukes.push_back({g_sim.players[0].basePos, gz, 0, Sim::NUKE_FLIGHT - 0.05f, false}); for (int t = 0; t < 2; t++) { g_sim.step(); g_sim.events.clear(); }   // (lingering fallout hurts everyone: look at the blast itself)
        if (hpOf(bystander) > 0) return fail("my own nuke spared my own tank under it");
        if (hpOf(allyTank) > 0) return fail("my nuke spared the ally's tank under it");
        g_sim.fallouts.clear();
        Vec2 gz2 = g_map.nearestFree(gz + Vec2(-600, 100), 30);
        g_sim.nukes.push_back({g_sim.players[1].basePos, gz2, 1, Sim::NUKE_FLIGHT - 0.05f, true});
        Ref safe = g_sim.spawnUnit(tankMe, 0, gz2); for (int t = 0; t < 2; t++) { g_sim.step(); g_sim.events.clear(); }
        if (hpOf(safe) < g_sim.get(safe)->maxHp) return fail("a computer army's forced nuke hurt a friend");
        // ---- snipers
        g_sim.fallouts.clear();
        Vec2 sp = g_map.nearestFree(mid + Vec2(-400, 300), 30);
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type != U_C_DOZER && e.type != U_K_DOZER && e.ut().role == UR_COMBAT) g_sim.destroy(e, false);
        Ref sn = g_sim.spawnUnit(sniperMe, 0, sp), trooper = g_sim.spawnUnit(inf1Foe, 2, sp + Vec2(200, 0)), enemySn = g_sim.spawnUnit(sniperFoe, 2, sp + Vec2(0, 200)), tk = g_sim.spawnUnit(tankFoe, 2, sp + Vec2(-200, 0));
        g_sim.cmdAttack({sn}, trooper); run(8);
        if (g_sim.get(trooper)) return fail("a sniper could not kill an infantryman");
        g_sim.cmdAttack({sn}, tk); run(3);
        if (hpOf(tk) < g_sim.get(tk)->maxHp) return fail("a sniper hurt a vehicle");
        g_sim.cmdAttack({sn}, enemySn); run(6);
        if (hpOf(enemySn) < g_sim.get(enemySn)->maxHp) return fail("a sniper shot another sniper");
        for (int k = 0; k < 4; k++) g_sim.spawnUnit(inf1Foe, 2, sp + Vec2(60 + k * 14, 40));
        g_sim.destroy(*g_sim.get(tk), false); g_sim.destroy(*g_sim.get(enemySn), false);
        float sn0 = hpOf(sn); run(8);
        if (hpOf(sn) < sn0) return fail("infantry spotted and shot a sniper");
        Ref tkNew = g_sim.spawnUnit(tankFoe, 2, sp + Vec2(150, 60)); run(8);
        if (hpOf(sn) < sn0) return fail("a vehicle spotted a sniper without a spy drone");
        g_sim.spawnUnit(foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE, 2, g_sim.get(sn)->pos + Vec2(100, 60)); run(1);
        g_sim.cmdAttack({tkNew}, sn); run(9);
        if (g_sim.get(sn) && hpOf(sn) >= sn0) return fail("a vehicle could not shoot a sniper that an enemy spy drone had found");
        // ---- aircraft: each kind kills aircraft
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().role == UR_COMBAT) g_sim.destroy(e, false);
        for (int kind = 0; kind < 2; kind++) {
            int myAir = firstUnitOf(me) + (kind ? 11 : 8), foeAir = firstUnitOf(foe) + 8;
            Vec2 at = g_map.nearestFree(mid, 30);
            Ref mine = g_sim.spawnUnit(myAir, 0, at), theirs = g_sim.spawnUnit(foeAir, 2, at + Vec2(260, 0));
            if (Entity* m = g_sim.get(mine)) { m->alt = 1; }
            if (Entity* t = g_sim.get(theirs)) { t->alt = 1; }
            g_sim.cmdAttack({mine}, theirs);
            run(40);
            // the cheap drone trades with an equal bomber (it hurts it badly), the expensive jet wins outright
            if (kind == 1 ? g_sim.get(theirs) != nullptr : (g_sim.get(theirs) && hpOf(theirs) > g_sim.get(theirs)->maxHp * 0.7f)) { fprintf(stderr, "kind %d: target hp left %.0f\n", kind, hpOf(theirs)); return fail("aircraft failed to fight aircraft"); }
            if (Entity* m = g_sim.get(mine)) g_sim.destroy(*m, false);
        }
        // ---- paradrop: one launch per tech structure, 2 minutes each
        int bb = firstBuildOf(me);
        Vec2 b0 = g_sim.players[0].basePos; int placed = 0;
        for (int dy = 4; dy < 22 && placed < 2; dy++) for (int dx = -16; dx < 16 && placed < 2; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, bb + BR_TECH, tx, ty)) { g_sim.placeBuilding(bb + BR_TECH, 0, tx, ty, true); placed++; } }
        if (placed < 2) return fail("could not place two tech structures");
        g_sim.players[0].dropReady = 0; g_sim.updatePowerPublic();
        Vec2 tgt = g_map.nearestFree(mid + Vec2(200, 200), 30);
        if (g_sim.dropsReady(0) != 2) return fail("two tech structures should offer two drops");
        if (!g_sim.cmdParadrop(0, tgt) || !g_sim.cmdParadrop(0, tgt)) return fail("two drops refused");
        if (g_sim.cmdParadrop(0, tgt)) return fail("a third drop with two structures");
        run(119); if (g_sim.dropsReady(0) != 0) return fail("drops came back before two minutes");
        run(3);   if (g_sim.dropsReady(0) != 2) return fail("drops did not come back after two minutes");
        printf("rulestest %s: ok\n", FACTION_NAME[me]);
    }
    return true;
}

// Units left on their own, force fire on friends and air combat:
//  * an idle group shot from beyond its reach goes after the shooter and walks back to its post afterwards
//  * a vehicle hit by something it cannot hit back (an aircraft) backs away; one that can answer stays to fight
//  * idle tanks go to help an ally whose structure is under attack beyond their sight, and so does a unit guarding a small zone
//  * a human army's units get out of an incoming nuke's circle (an army nobody commands does not)
//  * force fire (the Force Fire button, F + click) hurts an ally's tank and structure, and no ally or own unit ever shoots back
//  * a fighter jet out-turns and shoots down two bombers, two jets fight each other, and no fixed-wing aircraft hangs still in the air
static bool autoTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "autotest: %s\n", m); return false; };
    auto run = [](float sec) { for (int t = 0; t < (int)(20 * sec); t++) { g_sim.step(); g_sim.events.clear(); } };
    auto hpOf = [](Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : 0.0f; };
    auto clearAll = []() { for (auto& e : g_sim.ents) if (e.alive && e.kind != EK_RESOURCE && !(e.isBuilding() && e.bt().role == BR_HQ)) g_sim.destroy(e, false); g_sim.nukes.clear(); g_sim.fallouts.clear(); g_sim.projs.clear(); };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[3] = { (Faction)fi, (Faction)(1 - fi), (Faction)(1 - fi) };
        bool ai[3] = { false, true, true }; int diff[3] = { 1, 1, 1 }; int team[3] = { 0, 0, 1 };   // you, a computer ally, an enemy (nobody commands the computer armies here)
        g_sim.init(3, fac, ai, diff, team, seed + 17 * fi);
        for (int p = 0; p < 3; p++) { g_sim.players[p].money = 100000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
        Faction me = fac[0], foe = fac[2];
        int ub = firstUnitOf(me), fb = firstUnitOf(foe), ab = firstUnitOf(fac[1]);
        int tankMe = ub + 5, infFoe = fb + 2, artFoe = foe == F_CYBER ? U_C_RAIL : U_K_MLRS, bomberFoe = fb + 8, jetMe = ub + 11, jetFoe = fb + 11;
        Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f));
        auto freeAt = [](Vec2 p) { return g_map.nearestFree(p, 30); };
        clearAll();
        // ---- answering fire from beyond reach: the enemy's artillery out-ranges the tanks; they go and get it, then walk back
        {
            std::vector<Ref> tanks;
            for (int i = 0; i < 3; i++) tanks.push_back(g_sim.spawnUnit(tankMe, 0, freeAt(mid + Vec2(0, i * 30.0f))));
            g_sim.cmdStop(tanks);
            Vec2 post = g_sim.get(tanks[0])->pos;
            Ref art = g_sim.spawnUnit(artFoe, 2, freeAt(mid + Vec2(WEAPONS[UNITS[artFoe].weapon].range * TILE - 8, 30)));
            g_sim.cmdAttack({art}, tanks[1]);
            float a0 = hpOf(art);
            run(25);
            if (g_sim.get(art) && hpOf(art) > a0 * 0.5f) { fprintf(stderr, "artillery hp %.0f of %.0f\n", hpOf(art), a0); return fail("idle tanks shot from beyond their reach did not go after the shooter"); }
            run(25);
            int home = 0; for (auto r : tanks) if (Entity* t = g_sim.get(r)) if (dist(t->pos, post) < 4 * TILE) home++;
            if (home == 0) return fail("the tanks did not walk back to their post after the fight");
            clearAll();
        }
        // ---- something it cannot hit: a tank bombed from the air backs away; a Gatling/Volt that can answer the aircraft stands and fights
        {
            Vec2 p0 = freeAt(mid + Vec2(-6 * TILE, 6 * TILE));
            Ref tank = g_sim.spawnUnit(tankMe, 0, p0); g_sim.cmdStop({tank});
            Ref bomber = g_sim.spawnUnit(bomberFoe, 2, p0 + Vec2(-12 * TILE, 0)); g_sim.get(bomber)->alt = 1;
            g_sim.cmdAttack({bomber}, tank);
            float moved = 0; float t0 = -1;
            for (int t = 0; t < 20 * 20 && g_sim.get(tank); t++) { g_sim.step(); g_sim.events.clear(); Entity* e = g_sim.get(tank); if (e && e->hp < e->maxHp && t0 < 0) t0 = g_sim.time; if (e) moved = std::max(moved, dist(e->pos, p0)); }
            if (t0 < 0) return fail("the bomber never hit the tank (scene)");
            if (moved < 2.5f * TILE) { fprintf(stderr, "moved %.0f px\n", moved); return fail("a tank bombed by an aircraft it cannot hit did not back away"); }
            clearAll();
            Ref aa = g_sim.spawnUnit(ub + 6, 0, p0); g_sim.cmdStop({aa});   // Volt Walker / Gatling Tank: hits aircraft
            Ref bomber2 = g_sim.spawnUnit(bomberFoe, 2, p0 + Vec2(-12 * TILE, 0)); g_sim.get(bomber2)->alt = 1;
            g_sim.cmdAttack({bomber2}, aa);
            float b0 = hpOf(bomber2);
            run(12);
            if (g_sim.get(bomber2) && hpOf(bomber2) >= b0) return fail("an anti-air vehicle did not fight back against a bomber");
            clearAll();
        }
        // ---- helping an ally: the ally's power plant is attacked out of the tanks' sight; they go and help, then come back
        {
            int pw = firstBuildOf(fac[1]) + BR_POWER;
            Ref plant;
            Vec2 c = mid + Vec2(4 * TILE, -10 * TILE);
            for (int dy = 0; dy < 10 && !plant.valid(); dy++) for (int dx = 0; dx < 10 && !plant.valid(); dx++) if (g_sim.canPlace(1, pw, tileOf(c.x) + dx, tileOf(c.y) + dy)) plant = g_sim.placeBuilding(pw, 1, tileOf(c.x) + dx, tileOf(c.y) + dy, true);
            if (!plant.valid()) return fail("no spot for the ally's plant (scene)");
            Vec2 pp = g_sim.get(plant)->pos;
            std::vector<Ref> tanks;
            for (int i = 0; i < 3; i++) tanks.push_back(g_sim.spawnUnit(tankMe, 0, freeAt(pp + Vec2(8.5f * TILE, i * 30.0f))));
            g_sim.cmdStop(tanks);
            std::vector<Ref> raiders; float r0 = 0;
            for (int i = 0; i < 3; i++) { Ref r = g_sim.spawnUnit(infFoe, 2, freeAt(pp + Vec2(-4.5f * TILE, i * 18.0f))); raiders.push_back(r); r0 += hpOf(r); }
            g_sim.cmdAttack(raiders, plant);
            run(25);
            float r1 = 0; for (auto r : raiders) r1 += hpOf(r);
            if (r1 >= r0 * 0.5f) { fprintf(stderr, "raiders hp %.0f of %.0f\n", r1, r0); return fail("idle tanks did not come to help an ally under attack"); }
            clearAll();
            // the same with one unit guarding a small zone next to a friend's structure that is hit from outside the zone
            Ref own = g_sim.spawnUnit(tankMe, 0, freeAt(pp + Vec2(7 * TILE, 0)));
            g_sim.cmdGuardArea({own}, g_sim.get(own)->pos, 2.0f * TILE);
            Vec2 zone = g_sim.get(own)->zone;
            Ref plant2;
            for (int dy = 0; dy < 10 && !plant2.valid(); dy++) for (int dx = 0; dx < 10 && !plant2.valid(); dx++) if (g_sim.canPlace(0, firstBuildOf(me) + BR_POWER, tileOf(pp.x) + dx - 2, tileOf(pp.y) + dy - 2)) plant2 = g_sim.placeBuilding(firstBuildOf(me) + BR_POWER, 0, tileOf(pp.x) + dx - 2, tileOf(pp.y) + dy - 2, true);
            if (!plant2.valid()) return fail("no spot for my plant (scene)");
            Ref raider = g_sim.spawnUnit(infFoe, 2, freeAt(g_sim.get(plant2)->pos + Vec2(-4 * TILE, 0)));
            g_sim.cmdAttack({raider}, plant2);
            float h0 = hpOf(raider);
            run(25);
            if (g_sim.get(raider) && hpOf(raider) >= h0) return fail("a unit guarding a small zone did not help a friend hit from outside it");
            run(30);
            Entity* o = g_sim.get(own);
            if (o && dist(o->pos, zone) > 4.5f * TILE) return fail("the zone guard did not return to its zone");
            clearAll();
        }
        // ---- an incoming nuke: a human army's tanks drive out of the circle, an army nobody commands stays and burns
        {
            Vec2 gz = freeAt(Vec2((ISLE_X0 + 28) * TILE, (ISLE_X0 + 20) * TILE));   // open meadow between the NW base and the lake
            std::vector<Ref> mine, theirs;
            for (int i = 0; i < 4; i++) {
                float a = i * 1.5708f;
                mine.push_back(g_sim.spawnUnit(tankMe, 0, freeAt(gz + Vec2(std::cos(a), std::sin(a)) * (7.5f * TILE))));
                theirs.push_back(g_sim.spawnUnit(fb + 5, 2, freeAt(gz + Vec2(std::cos(a + 0.78f), std::sin(a + 0.78f)) * (7.5f * TILE))));
            }
            g_sim.cmdStop(mine); g_sim.cmdStop(theirs);
            g_sim.nukes.push_back({g_sim.players[1].basePos, gz, 2, 0.0f, false});   // the enemy's warhead
            g_sim.nukes.push_back({g_sim.players[2].basePos, gz, 0, 0.0f, false});   // and one of ours on the same spot (it would hurt our own tanks just the same)
            run(9);
            int alive = 0, foesAlive = 0; for (auto r : mine) if (g_sim.get(r)) alive++; for (auto r : theirs) if (g_sim.get(r)) foesAlive++;
            if (alive < 3) { fprintf(stderr, "%d of 4 survived\n", alive); return fail("a human army's units did not get out of the nuke's circle"); }
            if (foesAlive > 1) return fail("units nobody commands escaped the nuke on their own");
            clearAll();
        }
        // ---- force fire on friends (the Force Fire button and Ctrl), and nobody ever shoots back
        {
            Vec2 c = freeAt(mid + Vec2(-8 * TILE, -6 * TILE));
            Ref h1 = g_sim.spawnUnit(tankMe, 0, c), h2 = g_sim.spawnUnit(tankMe, 0, freeAt(c + Vec2(0, 40)));
            Ref a1 = g_sim.spawnUnit(ab + 5, 1, freeAt(c + Vec2(4 * TILE, 0))), a2 = g_sim.spawnUnit(ab + 5, 1, freeAt(c + Vec2(4 * TILE, 50)));
            g_sim.cmdStop({h1, h2, a1, a2});
            float hh1 = hpOf(h1), hh2 = hpOf(h2), ha1 = hpOf(a1), ha2 = hpOf(a2);
            int kills0 = g_sim.players[0].unitsKilled + g_sim.players[0].structuresKilled;
            // through the real input code: select h1, press F (the Force Fire button), click the ally's tank
            GameState st = g_game.state; g_game.state = GS_PLAYING;
            g_game.selection.clear(); g_game.selection.push_back(h1);
            g_game.cam = Vec2(clampf(c.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(c.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            auto key = [](SDL_Keycode k) { SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = k; g_game.handleEvent(e); };
            auto lclick = [](Vec2 w) { int x = (int)(w.x - g_game.cam.x), y = (int)(w.y - g_game.cam.y); SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y; g_game.handleEvent(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = x; e.button.y = y; g_game.handleEvent(e); e.type = SDL_MOUSEBUTTONUP; g_game.handleEvent(e); };
            SDL_SetModState(KMOD_NONE);
            key(SDLK_f);
            if (!g_game.forceMode) { g_game.state = st; return fail("the F key did not arm Force Fire for a tank"); }
            lclick(g_sim.get(a1)->pos);
            g_game.state = st;
            if (g_game.forceMode) return fail("Force Fire stayed armed after picking a target");
            run(12);
            if (hpOf(a1) >= ha1) return fail("Force Fire did not hurt the ally's tank");
            if (hpOf(h1) < hh1 || hpOf(h2) < hh2) return fail("an ally shot back at force fire");
            (void)ha2;   // (splash from the shells may graze the ally's other tank: that is the shot's blast, not anybody joining in)
            // my own other tank and the ally's other tank stay out of it: h2 never attacks the ally, a2 never attacks me
            Entity* e2 = g_sim.get(h2); Entity* ea2 = g_sim.get(a2);
            if ((e2 && e2->order == O_ATTACK) || (ea2 && ea2->order == O_ATTACK)) return fail("a bystander joined a friendly-fire fight");
            // turning on my own tank, then on the ally's structure, through the sim
            g_sim.cmdAttack({h1}, h2, true); run(10);
            if (hpOf(h2) >= hh2) return fail("force fire did not hurt my own tank");
            if (hpOf(h1) < hh1) return fail("my own tank shot back at force fire");
            int pw = firstBuildOf(fac[1]) + BR_POWER; Ref plant;
            for (int dy = 0; dy < 10 && !plant.valid(); dy++) for (int dx = 0; dx < 10 && !plant.valid(); dx++) if (g_sim.canPlace(1, pw, tileOf(c.x) + dx - 3, tileOf(c.y) + dy + 3)) plant = g_sim.placeBuilding(pw, 1, tileOf(c.x) + dx - 3, tileOf(c.y) + dy + 3, true);
            if (plant.valid()) { float p0 = hpOf(plant); g_sim.cmdAttack({h1}, plant, true); run(10); if (hpOf(plant) >= p0) return fail("force fire did not hurt the ally's structure"); }
            if (g_sim.players[0].unitsKilled + g_sim.players[0].structuresKilled != kills0) return fail("friendly fire was counted as kills");
            clearAll();
        }
        // ---- air combat: a jet out-turns and kills two bombers, and two jets really fight
        {
            Vec2 c = freeAt(mid + Vec2(0, -4 * TILE));
            // a dogfight is chaotic (one lucky burst decides whether the second bomber limps out of sight): the jet gets three starts and must win one
            float tk = -1;
            for (int attempt = 0; attempt < 3 && tk < 0; attempt++) {
                Vec2 c0 = c + Vec2(0, attempt * 3.0f * TILE);
                Ref jet = g_sim.spawnUnit(jetMe, 0, c0); g_sim.get(jet)->alt = 1; g_sim.get(jet)->airspeed = UNITS[jetMe].speed * 0.6f;
                Ref b1 = g_sim.spawnUnit(bomberFoe, 2, c0 + Vec2(10 * TILE, 0)), b2 = g_sim.spawnUnit(bomberFoe, 2, c0 + Vec2(10 * TILE, 50));
                g_sim.get(b1)->alt = 1; g_sim.get(b2)->alt = 1;
                for (int t = 0; t < 20 * 45 && g_sim.get(jet); t++) { g_sim.step(); g_sim.events.clear(); if (!g_sim.get(b1) && !g_sim.get(b2)) { tk = g_sim.time; break; } }
                if (tk < 0) fprintf(stderr, "dogfight start %d: jet %s hp %.0f, bombers hp %.0f / %.0f\n", attempt, g_sim.get(jet) ? "alive" : "dead", hpOf(jet), hpOf(b1), hpOf(b2));
                clearAll();
            }
            if (tk < 0) return fail("a fighter jet could not shoot down two bombers");
            Ref j1 = g_sim.spawnUnit(jetMe, 0, c), j2 = g_sim.spawnUnit(jetFoe, 2, c + Vec2(14 * TILE, 3 * TILE));
            for (Ref r : { j1, j2 }) { g_sim.get(r)->alt = 1; g_sim.get(r)->airspeed = UNITS[g_sim.get(r)->type].speed * 0.7f; }
            float h1 = hpOf(j1), h2 = hpOf(j2);
            run(30);
            bool hit1 = !g_sim.get(j1) || hpOf(j1) < h1, hit2 = !g_sim.get(j2) || hpOf(j2) < h2;
            if (!hit1 || !hit2) return fail("two jets did not fight each other");
            clearAll();
        }
        // ---- no fixed-wing aircraft hangs still in the air: sent somewhere, guarding, idle with nowhere to land, attack-moving
        {
            Vec2 c = freeAt(mid + Vec2(6 * TILE, 8 * TILE));
            std::vector<Ref> planes;
            for (int k = 0; k < 4; k++) { int tp = k % 2 ? jetMe : ub + 8; if (UNITS[tp].heli) tp = jetMe; Ref r = g_sim.spawnUnit(tp, 0, c + Vec2(k * 40.0f, 0)); g_sim.get(r)->alt = 1; planes.push_back(r); }
            g_sim.cmdMove({planes[0]}, c + Vec2(-10 * TILE, 4 * TILE), false);
            g_sim.cmdGuardArea({planes[1]}, c + Vec2(5 * TILE, -6 * TILE), 6 * TILE);
            g_sim.cmdMove({planes[3]}, c + Vec2(12 * TILE, 0), true);
            int slow = 0;
            for (int t = 0; t < 20 * 40; t++) {
                std::vector<Vec2> before; for (auto r : planes) { Entity* e = g_sim.get(r); before.push_back(e ? e->pos : Vec2()); }
                g_sim.step(); g_sim.events.clear();
                if (t < 40) continue;   // (take-off)
                for (size_t i = 0; i < planes.size(); i++) { Entity* e = g_sim.get(planes[i]); if (!e) continue; float v = dist(e->pos, before[i]) / SIM_DT; if (v < UNITS[e->type].speed * AIR_STALL * 0.8f) slow++; }
            }
            if (slow > 0) { fprintf(stderr, "%d plane-ticks below stall speed\n", slow); return fail("a fixed-wing aircraft hung still in the air"); }
            clearAll();
        }
        printf("autotest %s: ok\n", FACTION_NAME[me]);
    }
    return true;
}

// Nukes and aircraft: aircraft are far too fast to be given the chance to dodge a warhead, so every aircraft under one burns,
// whether it hovers over ground zero, flies through or sits parked; a computer army (difficulty Brutal: it notices at once) and a
// human army alike (both get their ground units out of the circle).
static bool nukeTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "nuketest: %s\n", m); return false; };
    for (int fi = 0; fi < 4; fi++) {
        Faction fac[2] = { (Faction)(fi & 1), (Faction)(1 - (fi & 1)) };
        int victim = fi < 2 ? 1 : 0, shooter = 1 - victim;   // first a computer army's aircraft, then a human army's
        bool ai[2] = { false, true }; int diff[2] = { 3, 3 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 31 * fi);
        g_ai.init(seed);
        Vec2 gz = g_map.nearestFree(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 30);
        std::vector<Ref> air;
        int k = 0;
        for (int u = 0; u < U_COUNT; u++) if (UNITS[u].faction == fac[victim] && UNITS[u].kind == UK_AIR && !UNITS[u].temp) {   // (the Hotline Strike fighters leave on their own and are never parked anywhere)
            Ref still = g_sim.spawnUnit(u, victim, gz + Vec2((k % 4) * 40.0f - 60, (k / 4) * 40.0f - 20));
            Ref mover = g_sim.spawnUnit(u, victim, gz + Vec2(-8 * TILE, 3 * TILE + k * 8.0f));
            g_sim.cmdMove({mover}, gz + Vec2(8 * TILE, 3 * TILE), false);   // crossing ground zero as it lands
            if (UNITS[u].sprintSpeed > 0) {   // the Supersonic Bomber's dash would carry it far past the circle before the warhead lands: hold the
                g_sim.get(still)->disabledUntil = 1e9f; g_sim.get(mover)->disabledUntil = 1e9f;   // pair over ground zero so the fireball still has aircraft in it
            }
            air.push_back(still);
            air.push_back(mover);
            k++;
        }
        g_sim.nukes.push_back({g_sim.players[shooter].basePos, gz, shooter, Sim::NUKE_FLIGHT - 1.0f - 2.0f / 3, false});   // (the crossing aircraft reach ground zero together with it)
        int dodgeTicks = 0;
        while (!g_sim.nukes.empty() && dodgeTicks++ < 200) { g_sim.step(); g_ai.ais[1].dodgeNukes(); g_sim.events.clear(); }   // (only the dodging: the commander's plans would send them elsewhere)
        for (auto r : air) if (Entity* e = g_sim.get(r)) {
            fprintf(stderr, "%s (player %d) survived %.0f px from ground zero with %.0f/%.0f hp\n", e->ut().name, victim, dist(e->pos, gz), e->hp, e->maxHp);
            return fail("an aircraft got away from a nuke");
        }
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
        auto spot = [&](int type, int skipDy) { for (int dy = skipDy; dy < 24; dy++) for (int dx = -16; dx < 16; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } for (int r = 1; r < 80; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) { if (std::max(std::abs(dx), std::abs(dy)) != r) continue; int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } return Vec2(-1, -1); };
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
        // behaviour in every order mode: idle auto-acquire, attack-move, guard area and move-then-fight must all end with kills
        for (int mode = 0; mode < 5; mode++) {
            const char* names[5] = { "idle", "attack-move", "guard area", "move", "defended" };
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.isUnit()) g_sim.destroy(e, false);
            Entity* jj = g_sim.get(jet);
            if (!jj) return fail("jet lost between modes");
            g_sim.cmdStop({jet});
            for (int t = 0; t < 20 * 25; t++) { g_sim.step(); g_sim.events.clear(); }   // back on the pad, rearmed
            jj = g_sim.get(jet);
            if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "  after settle: pos %.0f,%.0f pad %.0f,%.0f ammo %d order %d alt %.2f\n", jj->pos.x, jj->pos.y, g_sim.get(af)->pos.x, g_sim.get(af)->pos.y, jj->ammo, (int)jj->order, jj->alt);
            Vec2 base = g_sim.get(af)->pos;
            Vec2 where = g_map.nearestFree(base + Vec2((mode == 0 ? 7.0f : 16.0f) * TILE, 3 * TILE), 40);
            std::vector<Ref> vs;
            for (int i = 0; i < 4; i++) vs.push_back(g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, where + Vec2(i * 22, (i % 2) * 18)));
            float h0 = 0; for (auto v : vs) h0 += g_sim.get(v)->hp;
            if (mode == 4) {   // the column sits under anti-air cover: the jet must still hurt it, and should not simply die
                int aa = fac[1] == F_CYBER ? B_C_PATRIOT : B_K_ROCKET;
                for (int k = 0; k < 2; k++) for (int dy = 0; dy < 8; dy++) { int tx = tileOf(where.x) + 3 + k * 3, ty = tileOf(where.y) + dy; if (g_map.buildable(tx, ty) && g_map.buildable(tx + 1, ty + 1) && g_map.buildable(tx + 1, ty) && g_map.buildable(tx, ty + 1)) { g_sim.placeBuilding(aa, 1, tx, ty, true); break; } }
                g_sim.updatePowerPublic();
            }
            if (mode == 1 || mode == 4) g_sim.cmdMove({jet}, where, true);
            else if (mode == 2) g_sim.cmdGuardArea({jet}, where, 6 * TILE);
            else if (mode == 3) g_sim.cmdMove({jet}, where, false);
            int ammoStart = g_sim.get(jet)->ammo;
            for (int t = 0; t < 20 * 40; t++) { g_sim.step(); g_sim.events.clear(); if (!g_sim.get(jet)) break; }
            float h1 = 0; for (auto v : vs) if (Entity* e = g_sim.get(v)) h1 += e->hp;
            Entity* j2 = g_sim.get(jet);
            fprintf(stderr, "jettest %s %s: damage %.0f of %.0f, ammo %d->%d, order %d, jet hp %.0f\n", FACTION_NAME[fac[0]], names[mode], h0 - h1, h0, ammoStart, j2 ? j2->ammo : -1, j2 ? (int)j2->order : -1, j2 ? j2->hp : 0.0f);
            if (mode != 3 && h1 >= h0) { fprintf(stderr, "mode %s\n", names[mode]); return fail("jet never attacked in this order mode"); }
        }
        printf("jettest %s: ok (top speed %.0f px/s, turn %.1f rad/s, damage %.0f, kills %d)\n", FACTION_NAME[fac[0]], maxSpeed, maxTurn, hp0 - hp1, dead);
    }
    return true;
}


// Paradrop: a cargo plane releases 15 infantry, 7 vehicles and 4 aircraft on parachutes over the chosen spot, the power recharges for 5 minutes, and anti-air can shoot the plane down.
// Aid Drop: the human player's own relief flight. Needs a tech structure, one flight per tech structure per cooldown, never available to a
// computer army; nothing shoots the plane down (not even a nuke); the army nearest to the spot gets $20000 and a dozer, never the sender
// (dropping on your own base still goes to the nearest other army), and an exact tie is drawn at random between the armies in it.
// A computer ally on a human's team moves up to the front where the human's army is fighting. The human's tank column attack-moves to the nearest
// enemy base; the ally starts with a dozen tanks of its own at home. With the cooperation on, they join the push; with it off they stay home.
static bool allyTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "allytest: %s\n", m); return false; };
    int atFront[2] = { 0, 0 }; bool msg[2] = { false, false };
    for (int mode = 0; mode < 2; mode++) {   // 0 = cooperating, 1 = switched off
        Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER };
        bool ai[4] = { false, true, true, true }; int diff[4] = { 2, 2, 1, 1 }; int team[4] = { 0, 0, 1, 1 };
        g_sim.init(4, fac, ai, diff, team, seed);
        for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = false; g_ai.featMask[p] = FEAT_DEFAULT; }
        g_ai.featMask[1] = mode == 0 ? (FEAT_DEFAULT | FEAT_ALLY) : (FEAT_DEFAULT & ~FEAT_ALLY);
        g_ai.init(seed);
        std::fill(g_sim.players[1].explored.begin(), g_sim.players[1].explored.end(), 1);
        int foe = 2; if (dist(g_sim.players[3].basePos, g_sim.players[0].basePos) < dist(g_sim.players[2].basePos, g_sim.players[0].basePos)) foe = 3;
        Vec2 fb = g_sim.players[foe].basePos;
        Vec2 toMe = (g_sim.players[0].basePos - fb).norm();
        std::vector<Ref> column;
        for (int k = 0; k < 12; k++) column.push_back(g_sim.spawnUnit(firstUnitOf(F_CYBER) + 5, 0, g_map.nearestFree(fb + toMe * (20 * TILE) + Vec2((k % 4) * 30.0f, (k / 4) * 30.0f), 6)));
        for (int k = 0; k < 12; k++) g_sim.spawnUnit(firstUnitOf(F_CLANKER) + 5, 1, g_map.nearestFree(g_sim.players[1].basePos + (fb - g_sim.players[1].basePos).norm() * (6 * TILE) + Vec2((k % 4) * 30.0f, (k / 4) * 30.0f), 6));
        g_sim.cmdMove(column, fb, true);
        for (int t = 0; t < 100 * SIM_HZ && !g_sim.gameOver; t++) {
            g_sim.step(); g_ai.update();
            for (auto& ev : g_sim.events) if (ev.type == EV_MSG && ev.player == 0 && ev.msg.find("Ally") == 0) msg[mode] = true;
            g_sim.events.clear();
            if (t % SIM_HZ == 0) {   // the most ally fighters seen at the front (the human's column may well finish the outpost before the end)
                int near = 0;
                for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 1 && !e.isAir() && e.ut().role == UR_COMBAT && dist(e.pos, fb) < 32 * TILE) near++;
                atFront[mode] = std::max(atFront[mode], near);
            }
            if (getenv("ONEHOUR_TESTDBG") && t % (10 * SIM_HZ) == 0) {
                int h = 0, a = 0, aNear = 0; float md = 0;
                for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && !e.isAir() && e.ut().role == UR_COMBAT) { if (e.owner == 0) h++; if (e.owner == 1) { a++; md += dist(e.pos, fb) / TILE; if (dist(e.pos, fb) < 32 * TILE) aNear++; } }
                Entity* at = g_sim.get(g_ai.ais[1].attackTarget); printf("  mode %d t=%3ds: human tanks %d, ally tanks %d (mean distance to the front %.0f tiles, %d within 32) wave=%d target %s at %d,%d front %d,%d\n", mode, t / SIM_HZ, h, a, a ? md / a : 0.0f, aNear, (int)g_ai.ais[1].attacking, at ? (at->isBuilding() ? at->bt().name : at->ut().name) : "-", at ? tileOf(at->pos.x) : -1, at ? tileOf(at->pos.y) : -1, tileOf(fb.x), tileOf(fb.y));
            }
        }
    }
    printf("allytest: most ally fighters at the human's front: %d cooperating, %d switched off\n", atFront[0], atFront[1]);
    if (atFront[0] < 6) return fail("the ally did not move up to the human's front");
    if (atFront[1] > atFront[0] - 4) return fail("the cooperation made no difference");
    if (!msg[0]) return fail("the ally never said it was joining");
    return true;
}

// A hauler at work keeps clear of a raider: an enemy tank sent at its pile finds nothing to shoot (the hauler leaves toward the depot) and the hauler goes
// back to work once the tank is gone; with the behaviour switched off the tank kills it.
static bool haulTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "haultest: %s\n", m); return false; };
    bool survived[2] = { false, false }; int delivered[2] = { 0, 0 };
    for (int mode = 0; mode < 2; mode++) {   // 0 = hauler avoids raiders, 1 = switched off
        Faction fac[2] = { F_CYBER, F_CLANKER };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed);
        g_sim.players[0].haulFlee = mode == 0;
        std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
        Entity* pile = nullptr; float bd = 1e18f;
        for (auto& e : g_sim.ents) if (e.alive && e.kind == EK_RESOURCE) { float d = dist2(e.pos, g_sim.players[0].basePos); if (d < bd) { bd = d; pile = &e; } }
        if (!pile) return fail("no supply pile");
        Vec2 pp = pile->pos; Ref pr = g_sim.refOf(*pile);
        int hubType = firstBuildOf(F_CYBER) + BR_SUPPLY;
        bool placed = false;
        for (int r = 3; r < 14 && !placed; r++) for (int dy = -r; dy <= r && !placed; dy++) for (int dx = -r; dx <= r && !placed; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            int tx = tileOf(pp.x) + dx, ty = tileOf(pp.y) + dy;
            if (g_sim.canPlace(0, hubType, tx, ty)) { g_sim.placeBuilding(hubType, 0, tx, ty, true); placed = true; }
        }
        if (!placed) return fail("could not place a depot");
        Ref h = g_sim.spawnUnit(U_C_HARV, 0, g_map.nearestFree(pp + Vec2(TILE, TILE), 4));
        g_sim.cmdHarvest({h}, pr);
        Vec2 hubPos; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_SUPPLY) hubPos = e.pos;
        Vec2 away = (pp - hubPos).norm();   // the raider sits on the far side of the pile from the depot, in reach of the pile only
        Ref tank = g_sim.spawnUnit(firstUnitOf(F_CLANKER) + 5, 1, g_map.nearestFree(pp + away * (5 * TILE), 6));
        for (int t = 0; t < 70 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            if (getenv("ONEHOUR_TESTDBG") && t % (2 * SIM_HZ) == 0 && t < 70 * SIM_HZ) { Entity* hh = g_sim.get(h); Entity* tk = g_sim.get(tank); printf("  mode %d t=%2ds hauler %s hp %.0f order %d cargo %d pos %.0f,%.0f | tank %s hp %.0f order %d pos %.0f,%.0f\n", mode, t / SIM_HZ, hh ? "alive" : "dead", hh ? hh->hp : 0.0f, hh ? (int)hh->order : -1, hh ? hh->cargo : 0, hh ? hh->pos.x : 0.0f, hh ? hh->pos.y : 0.0f, tk ? "alive" : "dead", tk ? tk->hp : 0.0f, tk ? (int)tk->order : -1, tk ? tk->pos.x : 0.0f, tk ? tk->pos.y : 0.0f); }
            if (t == 25 * SIM_HZ) { Entity* tk = g_sim.get(tank); if (tk) g_sim.destroy(*tk, false); survived[mode] = g_sim.get(h) != nullptr; }
        }
        Entity* hh = g_sim.get(h);
        delivered[mode] = g_sim.players[0].harvested;
        if (mode == 0 && !hh) return fail("the hauler died after the raider was gone");
    }
    printf("haultest: hauler alive when the raider was removed: %d avoiding, %d switched off; credits delivered afterwards: %d / %d\n", (int)survived[0], (int)survived[1], delivered[0], delivered[1]);
    if (!survived[0]) return fail("the hauler did not get away from the raider");
    if (survived[1]) return fail("the raider never caught the hauler with the behaviour off (the test proves nothing)");
    if (delivered[0] < 300) return fail("the hauler did not go back to work after the raider was gone");
    return true;
}

static bool aidTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "aidtest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[4] = { (Faction)fi, (Faction)(1 - fi), F_CLANKER, F_CYBER };
        bool ai[4] = { false, true, true, true }; int diff[4] = { 1, 1, 3, 1 }; int team[4] = { 0, 0, 1, 1 };
        g_sim.init(4, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 4; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        auto techNear = [&](int p, Vec2 at, int skip) -> Ref {   // a finished tech structure of p's army near 'at' (skip: candidate sites passed over)
            int type = firstBuildOf(g_sim.players[p].faction) + BR_TECH;
            for (int r = 3; r < 24; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                int tx = tileOf(at.x) + dx, ty = tileOf(at.y) + dy;
                if (!inMap(tx, ty) || !g_sim.canPlace(p, type, tx, ty)) continue;
                if (skip-- > 0) continue;
                return g_sim.placeBuilding(type, p, tx, ty, true);
            }
            return NOREF;
        };
        auto dozers = [&](int p) { int n = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == p && e.ut().role == UR_DOZER) n++; return n; };
        auto nearestArmies = [&](Vec2 at, std::vector<int>& out) {   // brute force, independent of the game's own code
            float best[4] = { -1, -1, -1, -1 };
            for (auto& e : g_sim.ents) if (e.alive && e.kind != EK_RESOURCE && e.owner > 0) {
                float d;
                if (e.isBuilding()) { float x0 = e.tx * TILE, y0 = e.ty * TILE, x1 = x0 + e.bt().w * TILE, y1 = y0 + e.bt().h * TILE; float dx = std::max({x0 - at.x, 0.0f, at.x - x1}), dy = std::max({y0 - at.y, 0.0f, at.y - y1}); d = std::sqrt(dx * dx + dy * dy); }
                else d = std::max(0.0f, dist(at, e.pos) - e.radius());
                if (best[e.owner] < 0 || d < best[e.owner]) best[e.owner] = d;
            }
            float m = -1; for (int p = 1; p < 4; p++) if (best[p] >= 0 && (m < 0 || best[p] < m)) m = best[p];
            out.clear(); for (int p = 1; p < 4; p++) if (best[p] == m) out.push_back(p);
        };
        // fly every flight in the air until it has delivered; check the money and the dozer landed with whoever was nearest
        auto deliverAll = [&](const char* what, std::vector<int>& got, bool nukeIt = false) -> bool {
            got.clear();
            std::vector<std::vector<int>> expect(g_sim.aidDrops.size());
            bool nuked = false;
            for (int t = 0; t < 20 * 60; t++) {
                int m0[4], d0[4]; for (int p = 0; p < 4; p++) { m0[p] = g_sim.players[p].money; d0[p] = dozers(p); }
                for (size_t i = 0; i < g_sim.aidDrops.size(); i++) if (!g_sim.aidDrops[i].delivered) nearestArmies(g_sim.aidDrops[i].target, expect[i]);
                std::vector<bool> was; for (auto& a : g_sim.aidDrops) was.push_back(a.delivered);
                if (nukeIt && !nuked && !g_sim.aidDrops.empty() && dist(g_sim.aidDrops[0].pos, g_sim.aidDrops[0].target) < 500) {   // a warhead right on the plane
                    g_sim.nukes.push_back({g_sim.players[2].basePos, g_sim.aidDrops[0].pos, 2, Sim::NUKE_FLIGHT - 0.05f, false}); nuked = true;
                }
                g_sim.step(); g_sim.events.clear();
                if (g_sim.aidDrops.size() != was.size()) { fprintf(stderr, "aidtest %s: a flight vanished before delivering\n", what); return false; }
                int delivered = 0, gotMoney[4] = {}, gotDozer[4] = {};
                std::vector<int> sameTick;   // armies an earlier flight of this very tick gave a dozer to: it stands on the spot now, so it may win the next one
                for (size_t i = 0; i < g_sim.aidDrops.size(); i++) {
                    auto& a = g_sim.aidDrops[i];
                    if (!a.delivered || was[i]) continue;
                    delivered++;
                    if (getenv("ONEHOUR_DEBUG")) {
                        fprintf(stderr, "  t=%.2f flight %zu target %.0f,%.0f -> army %d (expected", g_sim.time, i, a.target.x, a.target.y, a.recipient);
                        for (int q : expect[i]) fprintf(stderr, " %d", q);
                        fprintf(stderr, ")");
                        for (int q = 1; q < 4; q++) { const Entity* nb = nullptr; float bd = 1e30f; for (auto& e : g_sim.ents) if (e.alive && e.owner == q && e.kind != EK_RESOURCE) { float d = g_sim.distToEntity(a.target, e); if (d < bd) { bd = d; nb = &e; } } if (nb) fprintf(stderr, "  [%d: %.1f %s]", q, bd, nb->isUnit() ? nb->ut().name : nb->bt().name); }
                        fprintf(stderr, "\n");
                    }
                    if (a.recipient == 0) { fprintf(stderr, "aidtest %s: the sender got its own aid\n", what); return false; }
                    if (std::find(expect[i].begin(), expect[i].end(), a.recipient) == expect[i].end() && std::find(sameTick.begin(), sameTick.end(), a.recipient) == sameTick.end()) { fprintf(stderr, "aidtest %s: army %d got it, the nearest was army %d\n", what, a.recipient, expect[i].empty() ? -1 : expect[i][0]); return false; }
                    gotMoney[a.recipient] += AID_MONEY; gotDozer[a.recipient]++;
                    got.push_back(a.recipient); sameTick.push_back(a.recipient);
                }
                if (delivered) for (int p = 0; p < 4; p++) {
                    if (g_sim.players[p].money - m0[p] != gotMoney[p]) { fprintf(stderr, "aidtest %s: army %d money %+d (want %+d)\n", what, p, g_sim.players[p].money - m0[p], gotMoney[p]); return false; }
                    if (dozers(p) - d0[p] != gotDozer[p]) { fprintf(stderr, "aidtest %s: army %d dozers %+d (want %+d)\n", what, p, dozers(p) - d0[p], gotDozer[p]); return false; }
                }
                bool all = true; for (auto& a : g_sim.aidDrops) if (!a.delivered) all = false;
                if (all) break;
            }
            for (auto& a : g_sim.aidDrops) if (!a.delivered) { fprintf(stderr, "aidtest %s: a flight never delivered\n", what); return false; }
            if (nukeIt && !nuked) { fprintf(stderr, "aidtest %s: the nuke was never fired\n", what); return false; }
            for (int t = 0; t < 20 * 30 * MAP_W / 146 && !g_sim.aidDrops.empty(); t++) { g_sim.step(); g_sim.events.clear(); }   // (the plane leaves over the far edge: longer on a bigger map)
            if (!g_sim.aidDrops.empty()) { fprintf(stderr, "aidtest %s: the plane and crates never cleared away\n", what); return false; }
            return true;
        };
        Vec2 b0 = g_sim.players[0].basePos, b2 = g_sim.players[2].basePos;
        if (g_sim.cmdAidDrop(0, b2) || g_sim.aidsReady(0) != 0) return fail("aid offered without a tech structure");
        if (!techNear(0, b0, 0).valid() || !techNear(1, g_sim.players[1].basePos, 0).valid() || !techNear(2, b2, 0).valid()) return fail("could not place the tech structures");
        // computer armies never have it, an ally's or an enemy's, Brutal or not
        if (g_sim.aidsReady(1) || g_sim.aidsReady(2) || g_sim.cmdAidDrop(1, b2) || g_sim.cmdAidDrop(2, b0) || !g_sim.aidDrops.empty()) return fail("a computer army could send aid");
        if (g_sim.aidsReady(0) != 1) return fail("one tech structure should offer one aid flight");
        // onto the enemy base: that army gets it, even with a nuke going off right on the plane on the way in
        if (!g_sim.cmdAidDrop(0, b2)) return fail("aid refused");
        if (g_sim.cmdAidDrop(0, b2)) return fail("aid ignored its cooldown");
        if (std::abs(g_sim.aidWait(0) - AID_COOLDOWN) > 0.5f) return fail("wrong cooldown");
        std::vector<int> got;
        if (!deliverAll("enemy base", got, true)) return false;
        if (got.size() != 1 || got[0] != 2) return fail("the enemy army under the drop did not get it");
        // your own nuke on the plane before the drop: it goes down like every aircraft in your blast, and the aid with it (nobody gets anything)
        for (int t = 0; t < (int)(AID_COOLDOWN * SIM_HZ) && g_sim.aidsReady(0) == 0; t++) { g_sim.step(); g_sim.events.clear(); }
        {
            int m[4], d[4]; for (int p = 0; p < 4; p++) { m[p] = g_sim.players[p].money; d[p] = dozers(p); }
            if (!g_sim.cmdAidDrop(0, b2)) return fail("aid refused (own nuke)");
            bool nuked = false;
            for (int t = 0; t < 20 * 90 && !g_sim.aidDrops.empty(); t++) {
                if (!nuked && dist(g_sim.aidDrops[0].pos, g_sim.aidDrops[0].target) < 600) { g_sim.nukes.push_back({b0, g_sim.aidDrops[0].pos, 0, Sim::NUKE_FLIGHT - 0.05f, false}); nuked = true; }
                g_sim.step(); g_sim.events.clear();
                if (!g_sim.aidDrops.empty() && g_sim.aidDrops[0].recipient >= 0) return fail("an aid plane downed before the drop still delivered");
            }
            if (!nuked) return fail("the own nuke was never fired");
            if (!g_sim.aidDrops.empty()) return fail("the downed aid flight never cleared away");
            for (int p = 1; p < 4; p++) if (g_sim.players[p].money != m[p] || dozers(p) > d[p]) return fail("aid from a downed plane reached somebody");
            if (g_sim.players[0].money != m[0]) return fail("the sender's money changed");
        }
        // onto your own base: never you, the nearest other army gets it; two more tech structures give two flights back to back
        for (int t = 0; t < (int)(AID_COOLDOWN * SIM_HZ); t++) { g_sim.step(); g_sim.events.clear(); }
        if (!techNear(0, b0, 0).valid() || !techNear(0, b0, 0).valid()) return fail("could not place more tech structures");
        if (g_sim.aidsReady(0) != 3) { fprintf(stderr, "aidtest: %d flights ready with three tech structures\n", g_sim.aidsReady(0)); return false; }
        if (!g_sim.cmdAidDrop(0, b0)) return fail("three tech structures should send three flights");
        for (int t = 0; t < 30; t++) { g_sim.step(); g_sim.events.clear(); }   // (a moment apart, so each one's delivery can be checked on its own)
        if (!g_sim.cmdAidDrop(0, b0 + Vec2(40, 0))) return fail("three tech structures should send three flights");
        for (int t = 0; t < 30; t++) { g_sim.step(); g_sim.events.clear(); }
        if (!g_sim.cmdAidDrop(0, b0 + Vec2(0, 40))) return fail("three tech structures should send three flights");
        if (g_sim.cmdAidDrop(0, b0) || g_sim.aidsReady(0) != 0) return fail("a fourth flight with three tech structures");
        if (!deliverAll("own base", got)) return false;
        if (got.size() != 3) return fail("not every flight over our own base delivered");
        // an exact tie: two armies' identical structures exactly as far from the spot on either side; both win some of the draws
        for (int t = 0; t < (int)(AID_COOLDOWN * SIM_HZ); t++) { g_sim.step(); g_sim.events.clear(); }
        Vec2 tie; bool placed = false;
        for (int r = 0; r < 70 && !placed; r++) for (int dy = -r; dy <= r && !placed; dy++) for (int dx = -r; dx <= r && !placed; dx++) {   // (out past the lake: the middle of the map is water)
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            int cx = MAP_W / 2 + dx, cy = MAP_H / 2 + dy;
            const int type = B_C_POWER; const int w = BUILDS[type].w, h = BUILDS[type].h;
            int txA = cx - 1 - w, txB = cx + 1, ty = cy - h / 2;
            if (!g_sim.canPlace(2, type, txA, ty) || !g_sim.canPlace(3, type, txB, ty)) continue;
            Vec2 c((float)(cx * TILE), (float)(ty * TILE + h * TILE / 2));
            bool clear = true; for (auto& e : g_sim.ents) if (e.alive && e.kind != EK_RESOURCE && g_sim.distToEntity(c, e) < 12 * TILE) clear = false;
            if (!clear) continue;
            g_sim.placeBuilding(type, 2, txA, ty, true); g_sim.placeBuilding(type, 3, txB, ty, true);
            tie = c; placed = true;
        }
        if (!placed) return fail("no room for the tie");
        int who[4]; int n = g_sim.aidCandidates(0, tie, who);
        if (n != 2 || who[0] != 2 || who[1] != 3) { fprintf(stderr, "aidtest: tie spot has %d candidates\n", n); return false; }
        int wins[4] = {};
        for (int round = 0; round < 12; round++) {   // one flight at a time: the dozer out of the crates would win every later draw
            for (int t = 0; t < (int)(AID_COOLDOWN * SIM_HZ) && g_sim.aidsReady(0) == 0; t++) { g_sim.step(); g_sim.events.clear(); }
            if (!g_sim.cmdAidDrop(0, tie)) return fail("tie flight refused");
            if (!deliverAll("tie", got)) return false;
            for (int r : got) wins[r]++;
            for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().role == UR_DOZER && g_sim.distToEntity(tie, e) < 6 * TILE) g_sim.destroy(e, false);
        }
        if (wins[0] || wins[1] || !wins[2] || !wins[3]) { fprintf(stderr, "aidtest: tie draws %d/%d/%d/%d\n", wins[0], wins[1], wins[2], wins[3]); return false; }
        printf("aidtest %s: ok (tie draws: army 2 x%d, army 3 x%d)\n", FACTION_NAME[fac[0]], wins[2], wins[3]);
    }
    return true;
}

static bool dropTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "droptest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        Player& pl = g_sim.players[0]; pl.money = 100000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int bb = firstBuildOf(fac[0]);
        Vec2 target = g_map.nearestFree(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 20);
        if (g_sim.cmdParadrop(0, target)) return fail("paradrop worked without a tech structure");
        Vec2 b0 = pl.basePos;
        for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (!g_sim.hasBuilding(0, bb + BR_TECH) && g_sim.canPlace(0, bb + BR_TECH, tx, ty)) g_sim.placeBuilding(bb + BR_TECH, 0, tx, ty, true); }
        if (!g_sim.hasBuilding(0, bb + BR_TECH)) return fail("could not place the tech structure");
        if (g_sim.cmdParadrop(0, target)) return fail("paradrop worked before its initial delay");
        pl.dropReady = 0;
        int before = g_sim.countUnits(0), beforeAir = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) beforeAir++;
        // What this fresh tech structure should send, straight from the simulator's own manifest: the load is not a fixed list any more.
        std::vector<int> want = g_sim.dropManifest(0);
        if (want.empty()) return fail("a standing tech structure would send nothing");
        if (g_sim.dropTier(0) != 0) return fail("a fresh tech structure is not at the first drop tier");
        for (int t : want) if (UNITS[t].program) return fail("a drop carried a unit the Advanced Program has not unlocked yet");
        int wantInf = 0, wantVeh = 0, wantAir = 0;
        int jeepsIn = 0;
        for (int t : want) { if (UNITS[t].kind == UK_INF) wantInf++; else if (UNITS[t].kind == UK_VEH) { wantVeh++; if (UNITS[t].cargoCap > 0) jeepsIn++; } else wantAir++; }
        if (jeepsIn != g_sim.dropJeeps(0) || jeepsIn < DROP_JEEPS_MIN || jeepsIn > DROP_JEEPS_MAX) return fail("a drop does not carry its jeeps (3 to 10, by what the army has unlocked)");
        wantInf += jeepsIn * JEEP_SEATS;   // every jeep comes down with its seven soldiers aboard
        if (std::count(want.begin(), want.end(), fac[0] == F_CYBER ? U_C_SNIPER : U_K_SNIPER) < 1) { fprintf(stderr, "droptest: a fresh tech structure sent %d snipers (snipers are available to it)\n", (int)std::count(want.begin(), want.end(), fac[0] == F_CYBER ? U_C_SNIPER : U_K_SNIPER)); return false; }
        if (!g_sim.cmdParadrop(0, target)) return fail("paradrop refused");
        if (g_sim.cmdParadrop(0, target)) return fail("paradrop ignored its cooldown");
        if (std::abs(g_sim.dropWait(0) - 120.0f) > 0.5f) return fail("cooldown is not two minutes");
        if (g_sim.airlifts.size() != 1) return fail("no cargo plane");
        bool sawFall = false; int maxFalling = 0;
        for (int t = 0; t < 20 * 45 && !g_sim.airlifts.empty(); t++) {
            g_sim.step(); g_sim.events.clear();
            int falling = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.fall > 0) falling++;
            if (falling) sawFall = true;
            maxFalling = std::max(maxFalling, falling);
        }
        if (!g_sim.airlifts.empty()) return fail("the plane never left the map");
        if (!sawFall) return fail("nobody hung under a parachute");
        for (int t = 0; t < 20 * 6; t++) { g_sim.step(); g_sim.events.clear(); }
        int inf = 0, veh = 0, air = 0;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) {
            if (e.fall > 0) return fail("a unit is still falling");
            if (e.ut().kind == UK_INF) inf++; else if (e.ut().kind == UK_VEH) veh++; else air++;
            if (e.isAir() || e.ut().role == UR_DOZER) continue;
            if (!g_map.passable(tileOf(e.pos.x), tileOf(e.pos.y))) return fail("a paratrooper landed on blocked ground");
            if (dist(e.pos, target) > (DROPS[fac[0]].radius + 6.0f) * TILE && dist(e.pos, g_sim.players[0].basePos) > 10 * TILE) return fail("a paratrooper landed far from the drop zone");
        }
        if (inf != wantInf || veh != wantVeh + 1 /* the starting dozer */ || air - beforeAir != wantAir) { fprintf(stderr, "droptest: landed %d infantry %d vehicles %d aircraft (manifest said %d/%d/%d)\n", inf, veh, air - beforeAir, wantInf, wantVeh, wantAir); return fail("the load that landed is not the manifest the plane loaded"); }
        if (g_sim.countUnits(0) - before != (int)want.size() + jeepsIn * JEEP_SEATS) return fail("wrong unit count");
        {   // the jeeps carry one rifleman, one sniper, one thrower and rocket troopers in the rest; they shoot from inside
            int jeeps = 0, aboard = 0;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().kind == UK_VEH && e.ut().cargoCap > 0) {
                jeeps++; aboard += (int)e.passengers.size();
                if ((int)e.passengers.size() != JEEP_SEATS) return fail("a dropped jeep landed with an empty seat");
                int snipers = 0, rockets = 0;
                for (Ref r : e.passengers) { const Entity* u = g_sim.get(r); if (!u || u->carrier != g_sim.refOf(e)) return fail("a crew member does not know its jeep"); if (u->ut().sniper) snipers++; if (u->type == (fac[0] == F_CYBER ? U_C_INF2 : U_K_INF2)) rockets++; }
                if (snipers != 1 || rockets < 4) return fail("a dropped jeep's crew is not one sniper and the rest mostly rocket troopers");
            }
            if (jeeps != jeepsIn || aboard != jeepsIn * JEEP_SEATS) return fail("the jeeps that landed are not the jeeps the plane carried");
        }
        printf("droptest drop %s: ok (tier 1 '%s': %d infantry, %d vehicles, %d aircraft, up to %d on the silk at once)\n", FACTION_NAME[fac[0]], DROP_TIERS[0].name, inf, veh - 1, air - beforeAir, maxFalling);

        // ---- it is upgradable: the same Data Center / Arms Lab sends more once the army learns things (R3/R4/R5). Snipers are
        // unlocked by the tech structure itself, so they must already be in the first load (R2); the Advanced Program's own units
        // (Ion Lancer, Aegis Titan) and the structure upgrades must deepen it afterwards, for a human army and for a computer one.
        {
            int sniper = fac[0] == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
            int elite = fac[0] == F_CYBER ? U_C_ELITE : U_K_ELITE;
            int titan = fac[0] == F_CYBER ? U_C_TITAN : U_K_TITAN;
            auto has = [](const std::vector<int>& v, int t) { return std::find(v.begin(), v.end(), t) != v.end(); };
            if (!has(want, sniper)) return fail("the first drop did not carry the snipers the army can field");
            if (has(want, elite) || has(want, titan)) return fail("the first drop carried Advanced Program units");
            if (g_sim.dropManifest(0).size() != want.size()) return fail("the manifest changed with nothing unlocked");
            pl.advTech = true;
            if (g_sim.dropTier(0) <= 0) return fail("the Advanced Program did not deepen the drop");
            std::vector<int> mid = g_sim.dropManifest(0);
            if (mid.size() <= want.size()) return fail("a deeper drop is not bigger");
            if (!has(mid, elite)) return fail("the Advanced Program's elite infantry did not join the drop");
            pl.upg[UPG_REPAIR] = pl.upg[UPG_GUNS] = true;
            if (g_sim.dropTier(0) != 2) return fail("the Advanced Program plus two upgrades did not reach the top drop tier");
            std::vector<int> top = g_sim.dropManifest(0);
            if (top.size() <= mid.size()) return fail("the top tier is not bigger than the middle one");
            if (!has(top, titan)) return fail("the top tier did not reach the super-heavy walker");
            if (!has(top, fac[0] == F_CYBER ? U_C_JET : U_K_JET)) return fail("the top tier did not reach the army's jets");
            if (DROP_TIERS[0].cooldown <= DROP_TIERS[2].cooldown) return fail("a deeper drop did not shorten its cooldown");
            // a computer army reads the same manifest: its own unlocks deepen its drops too - and its own structures gate what may appear
            {
                int bbFoe = firstBuildOf(fac[1]) + BR_TECH;
                Vec2 bp1 = g_sim.players[1].basePos;
                for (int dy = 4; dy < 20 && !g_sim.hasBuilding(1, bbFoe); dy++) for (int dx = -14; dx < 14 && !g_sim.hasBuilding(1, bbFoe); dx++) { int tx = tileOf(bp1.x) + dx, ty = tileOf(bp1.y) + dy; if (g_sim.canPlace(1, bbFoe, tx, ty)) g_sim.placeBuilding(bbFoe, 1, tx, ty, true); }
                if (!g_sim.hasBuilding(1, bbFoe)) return fail("could not give the computer army a tech structure");
                std::vector<int> botEarly = g_sim.dropManifest(1);
                if (botEarly.empty()) return fail("a computer army's standing tech structure would send nothing");
                for (int t : botEarly) if (UNITS[t].program) return fail("a computer army's drop carried a unit its own program has not unlocked");
                if (std::count(botEarly.begin(), botEarly.end(), fac[1] == F_CYBER ? U_C_SNIPER : U_K_SNIPER) < 1) return fail("a computer army's drop carried no sniper though its own tech structure unlocks them");
                g_sim.players[1].advTech = true; g_sim.players[1].upg[UPG_REPAIR] = g_sim.players[1].upg[UPG_GUNS] = true;
                std::vector<int> botLate = g_sim.dropManifest(1);
                if (botLate.size() <= botEarly.size()) { fprintf(stderr, "droptest: a computer army's drop went %d -> %d with its own unlocks\n", (int)botEarly.size(), (int)botLate.size()); return false; }
                if (std::find(botLate.begin(), botLate.end(), fac[1] == F_CYBER ? U_C_ELITE : U_K_ELITE) == botLate.end()) return fail("a computer army's program units did not join its drop");
                printf("droptest bot %s: ok (its own tech structure sends %d units with the snipers in them; its own program and upgrades grow that to %d)\n", FACTION_NAME[fac[1]], (int)botEarly.size(), (int)botLate.size());
            }
            if (g_sim.dropTier(1) != 2) return fail("a computer army's unlocks did not deepen its drop");
            // and the same structure really does send that heavier force now: call it again and count what lands
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_TECH) e.dropTimer = 0;
            int before2 = g_sim.countUnits(0);
            if (!g_sim.cmdParadrop(0, target)) return fail("the top-tier drop was refused");
            if (std::abs(g_sim.dropWait(0) - DROP_TIERS[2].cooldown) > 0.5f) return fail("the deeper drop did not use the shorter cooldown");
            for (int t = 0; t < 20 * 60 && !g_sim.airlifts.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
            for (int t = 0; t < 20 * 6; t++) { g_sim.step(); g_sim.events.clear(); }
            if (!g_sim.airlifts.empty()) return fail("the top-tier plane never left the map");
            int got2 = g_sim.countUnits(0) - before2, topJeeps = (int)std::count(top.begin(), top.end(), fac[0] == F_CYBER ? U_C_JEEP : U_K_JEEP);
            if (topJeeps != g_sim.dropJeeps(0) || topJeeps <= jeepsIn) { fprintf(stderr, "droptest: the top tier carried %d jeeps (a fresh drop has %d, the manifest says %d)\n", topJeeps, jeepsIn, g_sim.dropJeeps(0)); return false; }
            { bool keep = pl.upg[UPG_RUGGED]; pl.upg[UPG_RUGGED] = true; int allJeeps = g_sim.dropJeeps(0); pl.upg[UPG_RUGGED] = keep; if (allJeeps != DROP_JEEPS_MAX) return fail("an army with everything unlocked does not drop the most jeeps"); }
            if (got2 != (int)top.size() + topJeeps * JEEP_SEATS) { fprintf(stderr, "droptest: the top-tier drop landed %d of %d\n", got2, (int)top.size() + topJeeps * JEEP_SEATS); return false; }
            if (got2 <= (int)want.size() + jeepsIn * JEEP_SEATS) return fail("the top-tier drop is not bigger than the first one");
            printf("droptest upgrades %s: ok (tier 1 -> tier 3 grew the load from %d to %d units, snipers in it, cooldown %d s -> %d s)\n", FACTION_NAME[fac[0]], (int)want.size(), got2, (int)DROP_TIERS[0].cooldown, (int)DROP_TIERS[2].cooldown);
        }
    }
    // anti-air on the approach brings the plane down with its load
    {
        Faction fac[2] = { F_CLANKER, F_CYBER };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 5);
        Player& pl = g_sim.players[0]; pl.money = 100000; pl.dropReady = 0;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int bb0 = firstBuildOf(F_CLANKER), bb1 = firstBuildOf(F_CYBER);
        Vec2 b0 = pl.basePos;
        for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (!g_sim.hasBuilding(0, bb0 + BR_TECH) && g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
        Vec2 target = openGroundNear(g_sim.players[1].basePos, 12);
        flattenArena(target, 24, 24);   // this fixture tests flak, not the lake's dry-land area
        Vec2 dir = (target - b0).norm(), perp(-dir.y, dir.x);
        int placed = 0;
        for (int k = 0; k < 40 && placed < 8; k++) {
            Vec2 c = target - dir * (11.0f * TILE) + perp * ((k % 5 - 2) * 3.0f * TILE) - dir * ((k / 5) * 2.0f * TILE);
            int tx = tileOf(c.x), ty = tileOf(c.y);
            bool free = true; for (int j = 0; j < BUILDS[bb1 + BR_AATURRET].h; j++) for (int i = 0; i < BUILDS[bb1 + BR_AATURRET].w; i++) if (!g_map.buildable(tx + i, ty + j)) free = false;
            if (free) { g_sim.placeBuilding(bb1 + BR_AATURRET, 1, tx, ty, true); placed++; }   // (out in the open: no base needed)
        }
        g_sim.updatePowerPublic();
        for (int k = 0; k < 40 && g_sim.players[1].lowPower(); k++) {
            Vec2 c = g_sim.players[1].basePos + Vec2((k % 7 - 3) * 3.0f * TILE, (k / 7 + 4) * 3.0f * TILE);
            int tx = tileOf(c.x), ty = tileOf(c.y);
            bool free = true; for (int j = 0; j < BUILDS[bb1 + BR_POWER].h; j++) for (int i = 0; i < BUILDS[bb1 + BR_POWER].w; i++) if (!g_map.buildable(tx + i, ty + j)) free = false;
            if (free) { g_sim.placeBuilding(bb1 + BR_POWER, 1, tx, ty, true); g_sim.updatePowerPublic(); }
        }
        if (placed < 6) return fail("could not set the anti-air screen up");
        if (g_sim.players[1].lowPower()) return fail("the screen has no power");
        int before = g_sim.countUnits(0);
        std::vector<int> want = g_sim.dropManifest(0);
        if (!g_sim.cmdParadrop(0, target)) return fail("paradrop refused (flak test)");
        for (int t = 0; t < 20 * 45 && !g_sim.airlifts.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
        for (int t = 0; t < 20 * 5; t++) { g_sim.step(); g_sim.events.clear(); }
        int got = g_sim.countUnits(0) - before;
        if (got >= (int)want.size()) return fail("the whole load got through a wall of flak");
        printf("droptest flak: ok (the plane was shot down, %d of %d units got out)\n", got, (int)want.size());
    }
    return true;
}

// The Air Strike: the tech structure's other support power. One call sends waves of strike fighters over a spot you pick; each makes one bombing
// pass and leaves the map. Needs a tech structure, one call per cooldown per structure, and the flight grows with the army like a drop does.
static bool strikeTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "striketest: %s\n", m); return false; };
    auto run = [](float sec) { for (int t = 0; t < (int)(20 * sec); t++) { g_sim.step(); g_sim.events.clear(); } };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 3 * fi);
        Player& pl = g_sim.players[0]; pl.money = 100000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int bb = firstBuildOf(fac[0]);
        Vec2 b0 = pl.basePos;
        Vec2 line = g_map.nearestFree(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 24);
        // nothing to call with, and nothing to call before the support powers open up
        if (g_sim.cmdPhoneStrike(0, line) || g_sim.strikesReady(0) != 0 || g_sim.strikeWait(0) >= 0) return fail("a strike was offered without a tech structure");
        for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (!g_sim.hasBuilding(0, bb + BR_TECH) && g_sim.canPlace(0, bb + BR_TECH, tx, ty)) g_sim.placeBuilding(bb + BR_TECH, 0, tx, ty, true); }
        if (!g_sim.hasBuilding(0, bb + BR_TECH)) return fail("could not place the tech structure");
        if (g_sim.cmdPhoneStrike(0, line)) return fail("a strike worked before the support powers open up");
        pl.dropReady = 0;
        if (g_sim.strikesReady(0) != 1) return fail("one tech structure should take one call");
        // the first tier is already a full flight; every one of them is the army's strike fighter, a loan nobody commands
        if (g_sim.dropTier(0) != 0) return fail("a fresh tech structure is not at the first tier");
        std::vector<int> weak = g_sim.strikeManifest(0);
        int jetT = fac[0] == F_CYBER ? U_C_STRIKEJET : U_K_STRIKEJET;
        if ((int)weak.size() != STRIKE_FLIGHT[0]) { fprintf(stderr, "striketest: the first-tier flight is %d fighters\n", (int)weak.size()); return false; }
        pl.advTech = true;
        std::vector<int> swarm = g_sim.strikeManifest(0);
        if (swarm.size() <= weak.size()) { fprintf(stderr, "striketest: %d fighters after the Advanced Program, %d before\n", (int)swarm.size(), (int)weak.size()); return false; }
        for (int t : swarm) if (t != jetT || !UNITS[t].temp || UNITS[t].builtBy != BR_NONE) { fprintf(stderr, "striketest: type %d '%s' answered a %s army's call\n", t, UNITS[t].name, FACTION_NAME[fac[0]]); return false; }
        // an enemy line drawn up right where the call goes
        int foeInf = firstUnitOf(fac[1]) + 2, foeVeh = firstUnitOf(fac[1]) + 5;
        std::vector<Ref> foes;
        for (int k = 0; k < 6; k++) foes.push_back(g_sim.spawnUnit(k % 2 ? foeVeh : foeInf, 1, line + Vec2((k - 3) * 40.0f, (k % 3 - 1) * 30.0f)));
        float foeHp0 = 0; for (auto r : foes) if (Entity* e = g_sim.get(r)) foeHp0 += e->hp;
        if (!g_sim.cmdPhoneStrike(0, line)) return fail("the strike was refused");
        if (g_sim.cmdPhoneStrike(0, line)) return fail("the strike ignored its cooldown");
        if (std::abs(g_sim.strikeWait(0) - STRIKE_COOLDOWN) > 0.5f) return fail("wrong strike cooldown");
        // the flight really does come in, wave after wave, and closes on the called spot
        std::vector<Ref> seen; float closest = 1e9f;
        for (int t = 0; t < 20 * 40; t++) {
            g_sim.step(); g_sim.events.clear();
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.type == jetT) {
                if (std::find(seen.begin(), seen.end(), g_sim.refOf(e)) == seen.end()) seen.push_back(g_sim.refOf(e));
                closest = std::min(closest, dist(e.pos, line));
            }
        }
        if ((int)seen.size() != (int)swarm.size()) { fprintf(stderr, "striketest: %d fighters answered the call, wanted %d\n", (int)seen.size(), (int)swarm.size()); return false; }
        if (closest > 5.0f * TILE) { fprintf(stderr, "striketest: the flight never crossed the called spot (closest %.0f px)\n", closest); return false; }
        int heli = 0, jet = (int)swarm.size(), drone = 0;
        // and it hit what stood there
        run(5);
        float foeHp1 = 0; for (auto r : foes) if (Entity* e = g_sim.get(r)) foeHp1 += e->hp;
        if (foeHp1 >= foeHp0 * 0.6f) { fprintf(stderr, "striketest: the enemy line took only %.0f of %.0f damage\n", foeHp0 - foeHp1, foeHp0); return false; }
        // the top tier sends a bigger swarm still: the same tech structure grows with the army's unlocks
        pl.upg[UPG_REPAIR] = pl.upg[UPG_GUNS] = true;
        if (g_sim.dropTier(0) != 2) return fail("the strike tiers do not follow the drop tiers");
        int big = (int)g_sim.strikeManifest(0).size();
        if (big <= (int)swarm.size()) { fprintf(stderr, "striketest: %d aircraft at the top tier, %d at the middle one\n", big, (int)swarm.size()); return false; }
        // a second tech structure that has not been called on yet can take a call while the first is still cooling down
        for (int dy = 4; dy < 20 && g_sim.countBuildings(0, bb + BR_TECH, false) < 2; dy++)
            for (int dx = -14; dx < 14 && g_sim.countBuildings(0, bb + BR_TECH, false) < 2; dx++) {
                int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy;
                if (g_sim.canPlace(0, bb + BR_TECH, tx, ty)) g_sim.placeBuilding(bb + BR_TECH, 0, tx, ty, true);
            }
        if (g_sim.countBuildings(0, bb + BR_TECH, false) < 2) return fail("could not place a second tech structure");
        if (g_sim.strikesReady(0) != 1) return fail("the second tech structure did not offer a call while the first was cooling down");
        if (!g_sim.cmdPhoneStrike(0, line)) return fail("the second tech structure refused the call");
        if (g_sim.strikesReady(0) != 0) return fail("one call cooled down both tech structures");
        // the cooldown does come back
        for (int t = 0; t < (int)((STRIKE_COOLDOWN + 2.0f) * 20) && g_sim.strikesReady(0) < 2; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.strikesReady(0) != 2) return fail("the calls did not come back after the cooldown");
        printf("striketest %s: ok (a flight of %d strike fighters, grown %d -> %d -> %d with the army's unlocks; the enemy line was left at %.0f%% of its health)\n", FACTION_NAME[fac[0]], (int)swarm.size(), (int)weak.size(), (int)swarm.size(), big, 100.0f * foeHp1 / std::max(1.0f, foeHp0));
        (void)heli; (void)jet; (void)drone;
    }
    return true;
}

// Medics heal everything near them, idle dozers mend damaged structures by themselves, nukes flatten a wide area and leave radiation behind.
static bool supportTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "supporttest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        Player& pl = g_sim.players[0]; pl.money = 100000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        int bb = firstBuildOf(fac[0]), ub = firstUnitOf(fac[0]);
        Vec2 b0 = pl.basePos;
        auto spot = [&](int type, int skipDy) { for (int dy = skipDy; dy < 24; dy++) for (int dx = -16; dx < 16; dx++) { int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } for (int r = 1; r < 80; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) { if (std::max(std::abs(dx), std::abs(dy)) != r) continue; int tx = tileOf(b0.x) + dx, ty = tileOf(b0.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) return Vec2((float)tx, (float)ty); } return Vec2(-1, -1); };
        Vec2 sp = spot(bb + BR_POWER, 5); g_sim.placeBuilding(bb + BR_POWER, 0, (int)sp.x, (int)sp.y, true);
        Vec2 sf = spot(bb + BR_FACTORY, 5); Ref fact = g_sim.placeBuilding(bb + BR_FACTORY, 0, (int)sf.x, (int)sf.y, true);
        g_sim.updatePowerPublic();
        int medic = fac[0] == F_CYBER ? U_C_MEDIC : U_K_MEDIC;
        if (!g_sim.cmdTrain(fact, medic)) return fail("medic not trainable at the factory");
        Ref med;
        for (int t = 0; t < 20 * 40 && !med.valid(); t++) { g_sim.step(); g_sim.events.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.type == medic) med = g_sim.refOf(e); }
        if (!med.valid()) return fail("medic never produced");
        // wounded soldier, tank, aircraft next to the medic; a second wounded tank far away
        Vec2 at = g_sim.get(med)->pos + Vec2(40, 20);
        Ref inf = g_sim.spawnUnit(ub + 2, 0, at), tank = g_sim.spawnUnit(ub + 5, 0, at + Vec2(30, 10)), air = g_sim.spawnUnit(ub + 8, 0, at + Vec2(0, 40));
        Ref far = g_sim.spawnUnit(ub + 5, 0, g_map.nearestFree(at + Vec2(30 * TILE, 0), 30));
        for (Ref r : { inf, tank, air, far }) g_sim.get(r)->hp *= 0.3f;
        g_sim.cmdMove({med}, g_sim.get(med)->pos, false);
        for (int t = 0; t < 20 * 14; t++) { g_sim.step(); g_sim.events.clear(); }
        for (Ref r : { inf, tank, air }) { Entity* e = g_sim.get(r); if (!e || e->hp < e->maxHp * 0.95f) { if (e) fprintf(stderr, "%s hp %.0f/%.0f at %.0f px from the medic, order %d\n", e->ut().name, e->hp, e->maxHp, dist(e->pos, g_sim.get(med)->pos), (int)e->order); return fail("the medic did not heal a nearby friend"); } }
        // damaged structure: the idle dozer repairs it with no order
        Entity* fb = g_sim.get(fact); fb->hp = fb->maxHp * 0.4f; fb->lastDamaged = -100;
        Ref dz; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) dz = g_sim.refOf(e);
        g_sim.cmdStop({dz});
        for (int t = 0; t < 20 * 80; t++) { g_sim.step(); g_sim.events.clear(); }
        fb = g_sim.get(fact);
        if (!fb || fb->hp < fb->maxHp * 0.9f) { fprintf(stderr, "factory hp %.0f / %.0f\n", fb ? fb->hp : 0, fb ? fb->maxHp : 0); return fail("the idle dozer did not repair the damaged structure"); }
        // nuke: lots of enemy units and a structure, 9 tiles from ground zero
        Vec2 gz = g_map.nearestFree(b0 + Vec2(0, 30 * TILE), 60);
        flattenArena(gz, 14, 14);
        Ref e1 = g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, gz), e2 = g_sim.spawnUnit(fac[1] == F_CYBER ? U_C_TANK : U_K_TANK, 1, g_map.nearestFree(gz + Vec2(9 * TILE, 0), 20));
        Vec2 gzE = g_sim.get(e1)->pos;
        g_sim.nukes.push_back({b0, gzE, 0, Sim::NUKE_FLIGHT - 0.05f});
        for (int t = 0; t < 10; t++) { g_sim.step(); g_sim.events.clear(); if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "step %d nukes %zu fallouts %zu\n", t, g_sim.nukes.size(), g_sim.fallouts.size()); }
        if (g_sim.get(e1)) return fail("a tank at ground zero survived the nuke");
        if (g_sim.get(e2)) return fail("a tank at 9 tiles survived the nuke");
        if (g_sim.fallouts.empty()) { fprintf(stderr, "nukes %zu over %d gameOver %d t %.1f\n", g_sim.nukes.size(), (int)g_sim.gameOver, (int)g_sim.gameOver, g_sim.time); return fail("no radiation left behind"); }
        Ref ally = g_sim.spawnUnit(ub + 5, 0, g_map.nearestFree(gzE + Vec2(0, 3 * TILE), 20));   // walks into the crater after the blast (the blast itself would have flattened it)
        g_sim.cmdStop({ally});
        Entity* al = g_sim.get(ally); float hpA = al ? al->hp : 0;
        for (int t = 0; t < 20 * 20; t++) { g_sim.step(); g_sim.events.clear(); }
        al = g_sim.get(ally);
        if (al && al->hp >= hpA && Vec2(al->pos - gzE).len() < NUKE_RADIUS * TILE * 0.9f) return fail("radiation did not hurt a unit standing in it");
        for (int t = 0; t < 20 * 90; t++) { g_sim.step(); g_sim.events.clear(); }
        if (!g_sim.fallouts.empty()) return fail("radiation never faded");
        printf("supporttest %s: ok\n", FACTION_NAME[fac[0]]);
    }
    return true;
}


// The nuke wrecks rather than erases: small structures and every ground unit in the blast collapse, large structures survive heavily damaged,
// aircraft inside the fireball fall. Bombers destroy a structure with bombing runs while the guns stay for aircraft.
static bool bombTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "bombtest: %s\n", m); return false; };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        int b1 = firstBuildOf(fac[1]), u0 = firstUnitOf(fac[0]), u1 = firstUnitOf(fac[1]);
        Vec2 gz = g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f;
        gz = g_map.nearestFree(gz, 40);
        // enemy structures in a ring around ground zero: place each at the first buildable tile beyond the wanted distance
        auto put = [&](int type, float distTiles, float ang) {
            for (float extra = 0; extra < 6; extra += 0.5f) for (float da = 0; da < 1.2f; da += 0.1f) {
                Vec2 c = gz + Vec2(std::cos(ang + da), std::sin(ang + da)) * ((distTiles + extra) * TILE);
                int tx = tileOf(c.x) - BUILDS[type].w / 2, ty = tileOf(c.y) - BUILDS[type].h / 2;
                if (!inMap(tx, ty) || !g_sim.canPlace(1, type, tx, ty)) continue;
                Ref r = g_sim.placeBuilding(type, 1, tx, ty, true);
                return r;
            }
            return NOREF;
        };
        Ref turret = put(b1 + BR_TURRET, 3, 0.2f), power = put(b1 + BR_POWER, 5, 1.6f), barracks = put(b1 + BR_BARRACKS, 3, 3.2f);
        Ref factory = put(b1 + BR_FACTORY, 3, 4.6f), hq = put(b1 + BR_HQ, 8, 0.8f), rim = put(b1 + BR_SUPPLY, 9.5f, 2.4f);
        for (Ref r : { turret, power, barracks, factory, hq, rim }) if (!g_sim.get(r)) return fail("could not set the scene up");
        Ref tank = g_sim.spawnUnit(u1 + 5, 1, g_map.nearestFree(gz + Vec2(0, 50), 20));
        Ref airC = g_sim.spawnUnit(u1 + 8, 1, gz + Vec2(20, 0)), airRing = g_sim.spawnUnit(u1 + 8, 1, gz + Vec2(NUKE_RADIUS * TILE * 0.88f, 0)), airOut = g_sim.spawnUnit(u1 + 8, 1, gz + Vec2(0, NUKE_RADIUS * TILE + 3 * TILE));
        Ref airMine = g_sim.spawnUnit(u0 + 8, 0, gz + Vec2(-20, 10));
        g_sim.nukes.push_back({g_sim.players[0].basePos, gz, 0, Sim::NUKE_FLIGHT - 0.05f});
        for (int t = 0; t < 6; t++) { g_sim.step(); g_sim.events.clear(); }
        if (g_sim.fallouts.empty()) return fail("the nuke never went off");
        if (g_sim.get(tank)) return fail("a tank in the blast survived");
        if (g_sim.get(turret)) return fail("a turret at the edge of the core survived");
        if (g_sim.get(power)) return fail("a power plant in the inner blast survived");
        if (g_sim.get(barracks)) return fail("barracks inside the core survived");
        Entity* f = g_sim.get(factory); Entity* h = g_sim.get(hq); Entity* rm = g_sim.get(rim);
        if (!f) return fail("a large factory inside the core was erased");
        if (!h) return fail("the command structure was erased");
        if (f->hp > f->maxHp * 0.45f) return fail("the factory in the core is barely scratched");
        if (f->hp <= 0 || h->hp <= 0) return fail("large structure dead");
        if (rm && rm->hp >= rm->maxHp) return fail("a structure near the rim took no damage");
        if (g_sim.get(airC)) return fail("an aircraft inside the fireball did not fall");
        if (g_sim.get(airRing)) return fail("an aircraft at the shock ring did not fall");
        Entity* ao = g_sim.get(airOut); if (!ao || ao->hp < ao->maxHp) return fail("the nuke hurt an aircraft outside its radius");
        if (g_sim.get(airMine)) return fail("the launcher's own aircraft survived the blast");
        printf("bombtest nuke %s: ok (factory %.0f%%, hq %.0f%%, rim %.0f%%)\n", FACTION_NAME[fac[0]], 100 * f->hp / f->maxHp, 100 * h->hp / h->maxHp, rm ? 100 * rm->hp / rm->maxHp : 0.0f);
        // radiation never finishes a structure off
        for (int t = 0; t < 20 * 90; t++) { g_sim.step(); g_sim.events.clear(); if (getenv("ONEHOUR_DEBUG") && t % 100 == 0) fprintf(stderr, "t %d factory %.0f hq %.0f\n", t, g_sim.get(factory) ? g_sim.get(factory)->hp : -1, g_sim.get(hq) ? g_sim.get(hq)->hp : -1); }
        if (!g_sim.get(factory) || !g_sim.get(hq)) return fail("radiation killed a large structure");
    }
    // bombing runs
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 10 + fi);
        for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        int b0 = firstBuildOf(fac[0]), b1 = firstBuildOf(fac[1]), u0 = firstUnitOf(fac[0]);
        Vec2 mid = openGroundNear(g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f);
        Ref af, target;
        for (int dy = -8; dy < 8 && !af.valid(); dy++) for (int dx = -8; dx < 8 && !af.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, b0 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(b0 + BR_AIRFIELD, 0, tx, ty, true); }
        for (int dy = 12; dy < 24 && !target.valid(); dy++) for (int dx = -10; dx < 10 && !target.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(1, b1 + BR_FACTORY, tx, ty)) target = g_sim.placeBuilding(b1 + BR_FACTORY, 1, tx, ty, true); }
        if (!af.valid() || !target.valid()) return fail("could not set the bombing scene up");
        Vec2 pad = g_sim.get(af)->pos;
        std::vector<Ref> bombers;
        for (int i = 0; i < 3; i++) { Ref b = g_sim.spawnUnit(u0 + 8, 0, pad + Vec2((i - 1) * 30, 0)); g_sim.get(b)->home = af; bombers.push_back(b); }
        float hp0 = g_sim.get(target)->hp;
        int bombsSeen = 0; float firstHit = -1, killedAt = -1;
        g_sim.cmdAttack(bombers, target);
        for (int t = 0; t < 20 * 40; t++) {
            g_sim.step(); g_sim.events.clear();
            for (auto& p : g_sim.projs) if (WEAPONS[p.weapon].proj == PJ_BOMB && p.arcT < 0.12f) bombsSeen++;
            if (firstHit < 0 && g_sim.get(target) && g_sim.get(target)->hp < hp0 - 100) firstHit = g_sim.time;
            if (!g_sim.get(target)) { killedAt = g_sim.time; break; }
        }
        Entity* tg = g_sim.get(target);
        float dealt = tg ? hp0 - tg->hp : hp0;
        if (bombsSeen < 8) { fprintf(stderr, "bombs released: %d\n", bombsSeen); return fail("the bombers released almost no bombs"); }
        if (killedAt < 0) { fprintf(stderr, "damage dealt %.0f (target %.0f hp)\n", dealt, hp0); return fail("three bombers could not bring down a factory in 40 s"); }
        int alive = 0; for (Ref b : bombers) if (g_sim.get(b)) alive++;
        if (alive != 3) return fail("an undefended bomber was lost");
        // against aircraft the bomber uses its gun, not bombs
        Ref foe = g_sim.spawnUnit(firstUnitOf(fac[1]) + 8, 1, pad + Vec2(5 * TILE, -3 * TILE));
        int dropped = 0;
        float fh0 = g_sim.get(foe)->hp;
        for (Ref b : bombers) { Entity* e = g_sim.get(b); e->ammo = e->ut().ammo; e->order = O_IDLE; e->bombsLeft = 0; }
        g_sim.cmdAttack(bombers, foe);
        for (int t = 0; t < 20 * 12 && g_sim.get(foe); t++) { g_sim.step(); g_sim.events.clear(); for (auto& p : g_sim.projs) if (WEAPONS[p.weapon].proj == PJ_BOMB && p.owner == 0 && p.arcT < 0.12f && g_sim.get(foe)) dropped++; }
        Entity* fe = g_sim.get(foe);
        if (dropped > 0) return fail("a bomber dropped bombs on an aircraft");
        if (fe && fe->hp >= fh0) return fail("bombers did not shoot the enemy aircraft down");
        (void)fh0;
        printf("bombtest bombers %s: ok (%d bombs, a %.0f hp factory destroyed in %.0f s, first damage at %.0f s)\n", FACTION_NAME[fac[0]], bombsSeen, hp0, killedAt - 0, firstHit);
    }
    return true;
}


// A nuke aimed at an AI army: the smart commander walks its units out of the circle in the seven seconds of flight, the previous generation stays put.
// Afterwards the big structures it was aimed at are still there and the commander carries on.
static bool nukeDodgeTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "nukedodgetest: %s\n", m); return false; };
    float survived[2] = { 0, 0 };
    for (int mode = 0; mode < 2; mode++) {   // 0 = smart, 1 = previous generation
        Faction fac[2] = { F_CYBER, F_CLANKER };
        bool ai[2] = { true, true }; int diff[2] = { 2, 2 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed);
        g_ai.smartEnabled[0] = true; g_ai.smartEnabled[1] = mode == 0;
        g_ai.brainEnabled[0] = g_ai.brainEnabled[1] = false;
        g_ai.init(seed);
        g_ai.ais[0].player = -1;            // the launcher is only a name on the warhead; no war goes on while the target builds up
        g_ai.ais[1].regroupUntil = 1e9f;    // and the target keeps its army at home (no attack waves)
        for (int t = 0; t < 20 * 330; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        Vec2 gz = g_sim.players[1].basePos;
        std::vector<Ref> inside;
        float R = NUKE_RADIUS * TILE;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.isUnit() && !e.isAir() && dist(e.pos, gz) < R) inside.push_back(g_sim.refOf(e));
        int structures0 = g_sim.countBuildings(1, -1, true);
        if (getenv("ONEHOUR_DEBUG")) { fprintf(stderr, "  money %d built %d lost %d island launches %d\n", g_sim.players[1].money, g_sim.players[1].unitsBuilt, g_sim.players[1].unitsLost, g_sim.players[1].islandLaunches); for (auto& e : g_sim.ents) if (e.alive && e.owner == 1) fprintf(stderr, "  %s at %.1f tiles, order %d%s\n", e.isUnit() ? e.ut().name : e.bt().name, dist(e.pos, gz) / TILE, e.isUnit() ? (int)e.order : 0, e.isBuilding() && !e.constructed ? " (site)" : ""); }
        if (inside.size() < 6) { fprintf(stderr, "only %zu units in the circle\n", inside.size()); return fail("not enough units around the target to test the evasion"); }
        g_sim.nukes.push_back({g_sim.players[0].basePos, gz, 0, 0.0f});
        for (int t = 0; t < 20 * 9; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        int alive = 0; for (Ref r : inside) if (g_sim.get(r)) alive++;
        survived[mode] = alive / (float)inside.size();
        // the commander recovers: still alive, small structures rebuilt, large ones standing
        for (int t = 0; t < 20 * 200; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        int structures1 = g_sim.countBuildings(1, -1, true);
        if (!g_sim.players[1].alive) return fail("the nuked commander was wiped out");
        if (structures1 < structures0 * 0.6f) { fprintf(stderr, "structures %d -> %d\n", structures0, structures1); return fail("the nuked commander did not rebuild"); }
        printf("nukedodgetest %s: %.0f%% of %zu units survived the warhead, structures %d -> %d, dodges %d\n", mode == 0 ? "smart" : "previous", survived[mode] * 100, inside.size(), structures0, structures1, g_ai.ais[1].dodges);
    }
    if (survived[0] < survived[1] + 0.3f) return fail("the smart commander did not save a clearly larger share of its army");
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
    Brain prior; bool ok = b.games == 5 && b.waveSamples == 0 && std::abs(b.ww[2] - prior.ww[2]) < 1e-4f && std::abs(b.ww[1] - prior.ww[1]) < 1e-4f;   // (wave weights older than v8 mean something else: the prior stays)
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
    if (!again || c.unitSamples[U_K_DOZER] != 12 || c.unitSamples[U_K_TITAN] != 22 || c.unitSamples[U_C_TITAN] != 11) { remove(path.c_str()); return fail("a saved brain did not round-trip"); }
    // a v13 file (47 unit rows, from before the Anti-Radiation Trucks) loads with the trucks on the prior and every older row where it was
    {
        FILE* g13 = fopen(path.c_str(), "w");
        if (!g13) return fail("cannot write the temporary brain file");
        fprintf(g13, "onehour-brain 13 9 13\n"); for (int i = 0; i < WAVE_F; i++) fprintf(g13, "0.5 "); fprintf(g13, "\n");
        for (int r = 0; r < 47; r++) fprintf(g13, "%d %.3f 0.100 0.200 0.300\n", r + 200, 1.7f);
        fprintf(g13, "doctrines"); for (int d13 = 0; d13 < DOCTRINES; d13++) fprintf(g13, " 1 0.40000"); fprintf(g13, "\ncontexts\n");
        for (int c13 = 0; c13 < DOC_CTX; c13++) { for (int d13 = 0; d13 < DOCTRINES; d13++) fprintf(g13, " 1 0.50000"); fprintf(g13, "\n"); }
        fclose(g13);
        Brain b13; bool ok13 = b13.load(path.c_str()); remove(path.c_str());
        if (!ok13 || b13.unitSamples[U_C_SBOMBER] != 245 || b13.unitSamples[U_K_SBOMBER] != 246 || b13.unitSamples[U_K_JET] != 223 || b13.unitSamples[U_C_RADTRUCK] != 0 || b13.wu[U_K_RADTRUCK][0] != 1.0f) return fail("a v13 brain file did not keep its rows and leave the new Anti-Radiation Trucks on the prior");
    }
    // a v9 file (29 unit rows, from before the spies, spy drones and cargo lifters) loads with the newcomers on the prior
    {
        FILE* g9 = fopen(path.c_str(), "w");
        if (!g9) return fail("cannot write the temporary brain file");
        fprintf(g9, "onehour-brain 9 7 11\n"); for (int i = 0; i < WAVE_F; i++) fprintf(g9, "0.5 "); fprintf(g9, "\n");
        for (int r = 0; r < 29; r++) fprintf(g9, "%d %.3f 0.100 0.200 0.300\n", r + 100, 1.5f);
        fprintf(g9, "doctrines"); for (int d9 = 0; d9 < DOCTRINES; d9++) fprintf(g9, " 3 0.60000"); fprintf(g9, "\ncontexts\n");
        for (int c9 = 0; c9 < DOC_CTX; c9++) { for (int d9 = 0; d9 < DOCTRINES; d9++) fprintf(g9, " 1 0.50000"); fprintf(g9, "\n"); }
        fclose(g9);
        Brain b9; bool ok9 = b9.load(path.c_str()); remove(path.c_str());
        if (!ok9 || b9.games != 7 || b9.unitSamples[U_C_HELI] != 128 || b9.unitSamples[U_C_SPY] != 0 || b9.wu[U_K_CARGO][0] != 1.0f) return fail("a v9 brain file did not load with the new units on the prior");
    }
    // doctrines: the bandit prefers what wins, keeps trying the rest, learns a separate favourite per opponent context, and the
    // statistics survive a save and load (older files simply have none of the contexts)
    Brain d;
    Rng rr(5); int picks[DOC_CTX][DOCTRINES] = {};
    for (int k = 0; k < 800; k++) {
        int ctx = k & 1 ? 3 : 0;   // a computer Cyber opponent, or a human Clanker one
        int pk = d.pickDoctrine(rr, ctx); picks[ctx][pk]++;
        int best = ctx == 0 ? DOC_AIR : DOC_RUSH;   // air wins 90% of the time against the first, rush against the second; everything else 30%
        d.learnDoctrine(ctx, pk, pk == best ? 0.9f : 0.3f);
    }
    if (picks[0][DOC_AIR] < 200 || picks[3][DOC_RUSH] < 200) { fprintf(stderr, "picks ctx0: %d %d %d %d %d, ctx3: %d %d %d %d %d\n", picks[0][0], picks[0][1], picks[0][2], picks[0][3], picks[0][4], picks[3][0], picks[3][1], picks[3][2], picks[3][3], picks[3][4]); remove(path.c_str()); return fail("the doctrine bandit did not learn a favourite for each opponent"); }
    for (int c = 0; c < DOC_CTX; c += 3) for (int k = 0; k < DOCTRINES; k++) if (picks[c][k] < 2) { remove(path.c_str()); return fail("the doctrine bandit stopped exploring"); }
    d.save(path.c_str());
    Brain e2; bool back = e2.load(path.c_str());
    remove(path.c_str());
    bool same = back; for (int k = 0; k < DOCTRINES && same; k++) same = e2.docN[k] == d.docN[k] && std::abs(e2.docQ[k] - d.docQ[k]) < 1e-3f;
    for (int c = 0; c < DOC_CTX && same; c++) for (int k = 0; k < DOCTRINES && same; k++) same = e2.ctxN[c][k] == d.ctxN[c][k] && std::abs(e2.ctxQ[c][k] - d.ctxQ[c][k]) < 1e-3f;
    if (!same) return fail("doctrine statistics did not round-trip");
    // a context with no games leans on the overall table: the overall favourite is a sensible first guess
    { Brain f; for (int k = 0; k < 30; k++) f.learnDoctrine(0, DOC_TURTLE, 0.9f); f.learning = false; Rng r2(9); if (f.pickDoctrine(r2, 1) != DOC_TURTLE) return fail("an untried context ignored the overall favourite"); }
    // the wave model calibrates: where launching only pays off at a ratio of 2 or better, the odds at 1.5 must fall below the prior's
    { Brain w; float x[WAVE_F]; Brain::waveFeaturesR(1.5f, 1.5f, 10, 30, x); float before = w.waveProb(x);
      Rng r3(3);
      for (int k = 0; k < 400; k++) { float R = r3.f(0.6f, 3.0f); Brain::waveFeaturesR(R, R, 10, 30, x); w.learnWave(x, R >= 2.0f); }
      Brain::waveFeaturesR(1.5f, 1.5f, 10, 30, x); float mid = w.waveProb(x); Brain::waveFeaturesR(2.6f, 2.6f, 10, 30, x); float hi = w.waveProb(x);
      if (!(mid < before - 0.1f && hi > 0.6f)) { fprintf(stderr, "wave odds at 1.5: %.2f -> %.2f, at 2.6: %.2f\n", before, mid, hi); return fail("the wave model did not calibrate to the outcomes"); } }
    printf("braintest: ok (v2 weights migrated, jets start from the prior, v13 and v9 files still load, current files round-trip with per-opponent doctrine statistics; bandit picks air %d / 400 against computers and rush %d / 400 against people; the wave model calibrates)\n", picks[0][DOC_AIR], picks[3][DOC_RUSH]);
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
    // menu: pick Clanker, add a Cyber ally on Hard, one Brutal Cyber-random enemy plus a second enemy that is switched off again, start
    key(SDLK_RIGHT);                                         // your army -> Clanker
    key(SDLK_DOWN); key(SDLK_RIGHT);                         // ally row: None -> Cyber Army
    key(SDLK_TAB); key(SDLK_RIGHT);                          // ally difficulty: Normal -> Hard
    if (!g_game.menu.hasAlly() || g_game.menu.allyDiff != 2) return fail("ally selection in the menu");
    key(SDLK_DOWN); key(SDLK_RIGHT); key(SDLK_RIGHT);        // enemy 1 (the difficulty column stays focused): Normal -> Hard -> Brutal
    key(SDLK_DOWN); key(SDLK_RIGHT);                         // enemy 2: Normal -> Hard
    key(SDLK_TAB); key(SDLK_RIGHT);                          // enemy 2 army: Random -> Off
    if (g_game.menu.enemyDiff[0] != 3 || g_game.menu.enemyDiff[1] != 2 || g_game.menu.enemyFaction[1] != 3) return fail("per-enemy difficulty in the menu");
    key(SDLK_DOWN);                                          // enemy 3 is skipped while an ally holds the fourth seat, and the teams row while there is a single enemy
    if (g_game.menu.cursor != 6) return fail("menu rows that cannot be changed were not skipped");
    key(SDLK_RETURN);
    if (g_game.state != GS_PLAYING) return fail("game did not start from the menu");
    if (g_sim.players[0].faction != F_CLANKER) return fail("faction selection ignored");
    if (g_sim.numPlayers != 3) return fail("army count ignored");
    if (!g_sim.players[1].isAI || g_sim.players[1].team != g_sim.players[0].team || g_sim.players[1].faction != F_CYBER || g_sim.players[1].difficulty != 2) return fail("the ally was not set up as chosen");
    if (!g_sim.players[2].isAI || g_sim.players[2].team == g_sim.players[0].team || g_sim.players[2].difficulty != 3) return fail("the enemy was not set up as chosen");
    if (g_sim.enemies(0, 1) || !g_sim.enemies(0, 2)) return fail("ally and enemy relations");
    frames(3);
    // click on the dozer
    Entity* dz = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) dz = &e;
    if (!dz) return fail("no dozer");
    Vec2 s = Vec2(dz->pos.x - g_game.cam.x, dz->pos.y - g_game.cam.y);
    click((int)s.x, (int)s.y, SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1) return fail("dozer not selected by click");
    {   // every button of the dozer's menu is on screen, both walls and the Waymaker among them (a sixth row of buttons once ran off the bottom)
        bool onScreen = false, fwall = false, way = false;
        g_game.commandBarCheck(onScreen, fwall, way);
        if (!onScreen) return fail("a dozer menu button is off the screen");
        if (!fwall || !way) return fail("the dozer's menu lacks the Bastion Wall or the Waymaker");
    }
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
    g_game.buildButtonsPublic();   // (the grid follows the button width: click the first button where the layout puts it)
    click(g_game.buttonsPublic()[0].x + 20, g_game.buttonsPublic()[0].y + 10, SDL_BUTTON_LEFT);
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
        g_sim.players[0].dropReady = 0;
        key(SDLK_p);
        if (!g_game.dropMode) return fail("P did not arm the paradrop");
        { Vec2 tgt = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f); g_game.cam = Vec2(clampf(tgt.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(tgt.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); frames(1); click((int)(tgt.x - g_game.cam.x), (int)(tgt.y - g_game.cam.y), SDL_BUTTON_LEFT); }
        if (g_game.dropMode || g_sim.airlifts.size() != 1) return fail("clicking the map did not launch the paradrop");
        if (g_sim.dropWait(0) < 119.0f) return fail("paradrop cooldown not started");
        key(SDLK_p);
        if (g_game.dropMode) return fail("P armed the paradrop during its cooldown");
        key(SDLK_d);
        if (!g_game.aidMode) return fail("D did not arm the aid drop");
        { Vec2 tgt = g_sim.players[1].basePos; g_game.cam = Vec2(clampf(tgt.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(tgt.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); frames(1); click((int)(tgt.x - g_game.cam.x), (int)(tgt.y - g_game.cam.y), SDL_BUTTON_LEFT); }
        if (g_game.aidMode || g_sim.aidDrops.size() != 1) return fail("clicking the map did not send the aid flight");
        key(SDLK_d);
        if (g_game.aidMode) return fail("D armed the aid drop during its cooldown");
        g_sim.players[0].money = 40000;
        key(SDLK_u); key(SDLK_g); key(SDLK_e); key(SDLK_n); key(SDLK_h); key(SDLK_a);
        for (int u = 0; u < UPG_COUNT; u++) if (!g_sim.players[0].upgBusy[u]) return fail("U / G / E / N / H / A did not start the structure upgrades");
        { int spent = 0; for (int u = 0; u < UPG_COUNT; u++) spent += UPGRADES[F_CLANKER][u].cost; if (g_sim.players[0].money != 40000 - spent) return fail("upgrade costs not charged"); }
        for (int i = 0; i < 60 * 14; i++) frames(1);   // the plane crosses the map and the troops land (renders every frame: the plane, parachutes and drop circle)
        g_game.cam = Vec2(clampf(te->pos.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(te->pos.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
        g_game.selection.clear(); g_game.selection.push_back(tech);
        frames(30);   // shroud-free path, HUD scan label, tech panel
        for (int i = 0; i < 60 * 50 && !g_sim.players[0].advTech; i++) frames(1);
        if (!g_sim.players[0].advTech) return fail("research did not complete");
        for (int i = 0; i < 60 * 30 && !g_sim.players[0].upg[UPG_GUNS]; i++) frames(1);   // (renders the roof guns firing at whatever comes by)
        if (!g_sim.players[0].upg[UPG_RUGGED] || !g_sim.players[0].upg[UPG_GUNS] || !g_sim.players[0].upg[UPG_REPAIR]) return fail("structure upgrades did not complete");
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
    {   // the reported bug: an infiltrator right-clicked onto a nobody's island launcher must set out to take it (it used to walk to the shore and stop)
        Entity* L = nullptr;
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE && e.owner < 0) { L = &e; break; }
        if (!L) return fail("no neutral island launcher");
        std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
        Vec2 shore = g_map.nearestFree(L->pos + Vec2(0, -(L->bt().h * 0.5f + 14.0f) * TILE), 24);
        Ref spy = g_sim.spawnUnit(g_sim.players[0].faction == F_CYBER ? U_C_SPY : U_K_SPY, 0, shore);
        g_game.selection.clear(); g_game.selection.push_back(spy);
        g_game.cam = Vec2(clampf(L->pos.x - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W), clampf(L->pos.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H));
        Vec2 ls = L->pos - g_game.cam;
        click((int)ls.x, (int)ls.y, SDL_BUTTON_RIGHT);
        Entity* sp = g_sim.get(spy);
        if (!sp || sp->order != O_CAPTURE || sp->targetEnt != g_sim.refOf(*L)) return fail("right-clicking a neutral island launcher with an infiltrator did not order a capture");
        g_sim.destroy(*sp, false);
        g_game.selection.clear();
    }
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
        // the bunkers' calls work like the Mini Nuke: with one bunker selected, pressing P / J / D again calls in the next ready bunker's
        {
            const int me = g_sim.humanPlayer, bt = g_sim.players[me].faction == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
            std::vector<Ref> bks;
            Vec2 bp = g_sim.players[me].basePos;
            for (int r = 6; r < 40 && bks.size() < 2; r++) for (int k = 0; k < 16 && bks.size() < 2; k++) {
                int tx = tileOf(bp.x) + (int)(r * std::cos(k * 0.3927f)), ty = tileOf(bp.y) + (int)(r * std::sin(k * 0.3927f));
                if (g_sim.canPlace(me, bt, tx, ty)) bks.push_back(g_sim.placeBuilding(bt, me, tx, ty, true));
            }
            if (bks.size() < 2) return fail("no room for two bunkers");
            for (Ref r : bks) { Entity* b = g_sim.get(r); b->bunkerUpg = (u8)((1 << BU_COUNT) - 1); b->paraReady = b->rainReady = b->tankReady = 0; b->disabledUntil = 0; }
            g_game.selection.clear(); g_game.selection.push_back(bks[0]);
            g_game.cam = Vec2(clampf(bp.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(bp.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            struct Call { SDL_Keycode k; int u; const char* what; } calls[3] = { { SDLK_p, BU_PARA, "Para Backup" }, { SDLK_j, BU_RAIN, "Rain Nukes" }, { SDLK_d, BU_TANKS, "Tank Drop" } };
            for (auto& c : calls) {
                for (int n = 0; n < 2; n++) { key(c.k); click(260 + n * 300, VIEW_H / 3, SDL_BUTTON_LEFT); }
                int fired = 0; for (Ref r : bks) if (g_sim.bunkerPowerWait(*g_sim.get(r), c.u) > 0) fired++;
                if (fired != 2) { fprintf(stderr, "uitest: %s: %d of 2 bunkers fired after two presses\n", c.what, fired); return fail("a second press did not call the next ready bunker"); }
                key(c.k);
                if (g_game.nukeMode || g_game.dropMode) return fail("a bunker call opened its targeting with no bunker ready");
            }
            g_sim.nukes.clear(); g_sim.airlifts.clear();
            g_game.selection.clear();
        }
        // a computer army's difficulty: its row follows "Controls help"; Right asks once, Right again raises it a step, Left never lowers it
        int armies = 0, firstAi = -1;
        for (int p = 0; p < g_sim.numPlayers; p++) if (g_sim.players[p].isAI && g_sim.players[p].alive) { armies++; if (firstAi < 0) firstAi = p; }
        if (firstAi >= 0) {
            int d0 = g_sim.players[firstAi].difficulty;
            key(SDLK_ESCAPE);
            for (int i = 0; i < 3; i++) key(SDLK_DOWN);   // the frame rate row: Right and Left step through Balanced / Full / Power saver
            { int m0 = g_game.fpsMode; key(SDLK_RIGHT); if (g_game.fpsMode == m0) return fail("the frame rate row did not change the setting");
              if (g_game.targetFps(144, false) <= 0 || g_game.targetFps(144, true) > 30) return fail("a frame rate setting asked for no frames, or more than 30 in the background");
              key(SDLK_LEFT); if (g_game.fpsMode != m0) return fail("Left did not step the frame rate back"); }
            if (g_game.targetFps(144, false) != 72 || g_game.targetFps(60, false) != 60) return fail("Balanced is not every refresh up to 75 Hz and every second one above");
            for (int i = 0; i < 2; i++) key(SDLK_DOWN);
            key(SDLK_RIGHT);
            if (g_sim.players[firstAi].difficulty != d0) return fail("raising an army's difficulty did not ask for confirmation");
            key(SDLK_RIGHT);
            if (d0 < 3 && g_sim.players[firstAi].difficulty != d0 + 1) return fail("the pause menu did not raise the army's difficulty");
            key(SDLK_LEFT); key(SDLK_LEFT); key(SDLK_RETURN); key(SDLK_ESCAPE);
            if (g_sim.players[firstAi].difficulty < std::min(3, d0 + 1)) return fail("the pause menu lowered an army's difficulty");
            if (g_game.menuOpen) key(SDLK_ESCAPE);
            if (g_game.menuOpen) return fail("Esc did not close the pause menu after the difficulty row");
        }
        // surrender needs a second confirmation and ends the game as a defeat
        key(SDLK_ESCAPE);
        for (int i = 0; i < 6 + armies; i++) key(SDLK_DOWN);   // (Resume, speed, sound, frame rate, help, the armies, restart: then surrender)
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


// Every aircraft in every order mode against every kind of target: each combination must end with damage dealt.
static bool airMatrixTest(u64 seed) {
    int bad = 0, runs = 0;
    const char* modes[5] = { "idle", "attack-move", "guard", "attack", "guard-near" };
    const char* kinds[7] = { "infantry", "tank", "plant", "turret", "aircraft", "hq", "jet" };
    for (int fi = 0; fi < 2; fi++) for (int ac = 0; ac < 3; ac++) for (int kind = 0; kind < 7; kind++) for (int mode = 0; mode < 5; mode++) for (int var = 0; var < 3; var++) {
        if (ac == 2 && fi != F_CYBER) continue;   // ac 2: the Hornet Gunship (the Clanker helicopter is the Vulture, ac 0)
        if (var == 1 && kind > 1) continue;
        if (ac == 0 && kind == 6) continue;   // a lone bomber against fighters is expected to lose
        if (var == 2 && kind > 1 && kind != 4 && kind != 6) continue;   // var 1: the targets drive off; var 2: a flight of four
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 100000; }
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);   // (no dozers mending the test target back up between runs)
        int b0 = firstBuildOf(fac[0]), b1 = firstBuildOf(fac[1]), u0 = firstUnitOf(fac[0]), u1 = firstUnitOf(fac[1]);
        Vec2 mid = g_map.nearestFree(g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f, 40);
        flattenArena(mid, 16, 26);   // (the middle of the map is a lake: the aircraft get an open field)
        Ref af;
        for (int dy = -8; dy < 8 && !af.valid(); dy++) for (int dx = -8; dx < 8 && !af.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, b0 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(b0 + BR_AIRFIELD, 0, tx, ty, true); }
        if (!af.valid()) { fprintf(stderr, "airmatrix: no airfield spot\n"); return false; }
        g_sim.updatePowerPublic();
        Vec2 pad = g_sim.get(af)->pos;
        int type = ac == 2 ? (int)U_C_HELI : u0 + (ac == 0 ? 8 : 11);
        Ref jet = g_sim.spawnUnit(type, 0, pad + Vec2(0, 0)); g_sim.get(jet)->home = af;
        std::vector<Ref> flight{jet};
        if (var == 2) for (int i = 1; i < 4; i++) { Ref r = g_sim.spawnUnit(type, 0, pad + Vec2((i - 1.5f) * 30, 0)); g_sim.get(r)->home = af; flight.push_back(r); }
        Vec2 where = pad + Vec2(0, (mode == 0 ? 7 : 16) * TILE);
        std::vector<Ref> tg;
        // nearest spot to 'where' first (a corner-first search could park the target against the map border, where a jet's turning circle does not fit)
        auto putB = [&](int btype) { for (int r2 = 0; r2 <= 130; r2++) for (int dy = 0; dy < 10; dy++) for (int dx = -6; dx < 6; dx++) { if (dx * dx + dy * dy != r2) continue; int tx = tileOf(where.x) + dx, ty = tileOf(where.y) + dy; if (g_sim.canPlace(1, btype, tx, ty)) return g_sim.placeBuilding(btype, 1, tx, ty, true); } return NOREF; };
        if (kind == 0) for (int i = 0; i < 3; i++) tg.push_back(g_sim.spawnUnit(u1 + 2, 1, g_map.nearestFree(where + Vec2(i * 18, 0), 20)));
        if (kind == 1) for (int i = 0; i < 3; i++) tg.push_back(g_sim.spawnUnit(u1 + 5, 1, g_map.nearestFree(where + Vec2(i * 24, 0), 20)));
        if (kind == 2) tg.push_back(putB(b1 + BR_POWER));
        if (kind == 3) tg.push_back(putB(b1 + BR_TURRET));
        if (kind == 4) for (int i = 0; i < 2; i++) tg.push_back(g_sim.spawnUnit(u1 + 8, 1, where + Vec2(i * 40, 0)));
        if (kind == 5) tg.push_back(putB(b1 + BR_HQ));
        if (kind == 6) tg.push_back(g_sim.spawnUnit(u1 + 11, 1, where));
        g_sim.updatePowerPublic();
        for (Ref r : tg) if (!g_sim.get(r)) { fprintf(stderr, "airmatrix: scene failed\n"); return false; }
        float h0 = 0; for (Ref r : tg) h0 += g_sim.get(r)->hp;
        if (var == 1) for (Ref r : tg) g_sim.cmdMove({r}, g_map.nearestFree(g_sim.get(r)->pos + Vec2(30 * TILE, 0), 60), false);
        // aircraft targets need to be able to fly: park the enemy ones as plain idle craft without a home (they just hover)
        Vec2 tc = g_sim.get(tg[0])->pos;
        if (mode == 1) g_sim.cmdMove(flight, tc, true);
        else if (mode == 2) g_sim.cmdGuardArea(flight, tc, var == 1 ? 14 * TILE : 6 * TILE);
        else if (mode == 3) g_sim.cmdAttack(flight, tg[0]);
        else if (mode == 4) g_sim.cmdGuardArea(flight, tc + Vec2(0, -7 * TILE), 12 * TILE);
        float firstDmg = -1;
        for (int t = 0; t < 20 * 45; t++) {
            g_sim.step(); g_sim.events.clear();
            if (firstDmg < 0 && (t % 4) == 0) { float hh = 0; for (Ref r : tg) if (Entity* e = g_sim.get(r)) hh += e->hp; if (hh < h0 - 1) firstDmg = t * SIM_DT; }
        }
        float h1 = 0; for (Ref r : tg) if (Entity* e = g_sim.get(r)) h1 += e->hp;
        runs++;
        Entity* j = g_sim.get(jet);
        bool ok = h1 < h0 - 1 && firstDmg < 22.0f;
        if (!ok) { bad++; fprintf(stderr, "airmatrix FAIL %s %s vs %s var %d: first damage %.1fs hp %.0f -> %.0f (jet %s order %d ammo %d alt %.1f)\n", FACTION_NAME[fac[0]], modes[mode], kinds[kind], var, firstDmg, h0, h1, j ? UNITS[j->type].name : "dead", j ? (int)j->order : -1, j ? j->ammo : -1, j ? j->alt : 0.f); }
    }
    printf("airmatrix: %d runs, %d without damage\n", runs, bad);
    return bad == 0;
}


// Every ground combat unit in every order mode against every kind of target: each combination that its weapon allows must deal damage.
static bool groundMatrixTest(u64 seed) {
    int bad = 0, runs = 0;
    const char* modes[4] = { "idle", "attack-move", "guard", "attack" };
    const char* kinds[6] = { "infantry", "tank", "plant", "turret", "aircraft", "hq" };
    for (int fi = 0; fi < 2; fi++) for (int ut = 0; ut < 12; ut++) for (int kind = 0; kind < 6; kind++) for (int mode = 0; mode < 4; mode++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        int u0 = firstUnitOf(fac[0]);
        const UnitType& U = UNITS[u0 + ut];
        if (U.kind == UK_AIR || U.role != UR_COMBAT || U.weapon < 0) continue;
        const Weapon& W = WEAPONS[U.weapon];
        if (kind == 4 && !W.air) continue;
        if (mode == 0 && W.minRange > 0) continue;   // artillery standing idle does not engage what is inside its minimum range (by design)
        if (kind != 4 && !W.ground) continue;
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 100000; }
        int b1 = firstBuildOf(fac[1]), u1 = firstUnitOf(fac[1]);
        Vec2 mid = openGroundNear(g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f);
        Vec2 me = g_map.nearestFree(mid, 20);
        Ref unit = g_sim.spawnUnit(u0 + ut, 0, me);
        Vec2 where = g_map.nearestFree(me + Vec2(mode == 0 ? 5.0f * TILE : 13.0f * TILE, 0), 30);
        std::vector<Ref> tg;
        auto putB = [&](int btype) { for (int dy = -4; dy < 10; dy++) for (int dx = -2; dx < 12; dx++) { int tx = tileOf(where.x) + dx, ty = tileOf(where.y) + dy; if (g_sim.canPlace(1, btype, tx, ty)) return g_sim.placeBuilding(btype, 1, tx, ty, true); } return NOREF; };
        if (kind == 0) for (int i = 0; i < 3; i++) tg.push_back(g_sim.spawnUnit(u1 + 2, 1, g_map.nearestFree(where + Vec2(0, i * 18), 20)));
        if (kind == 1) for (int i = 0; i < 3; i++) tg.push_back(g_sim.spawnUnit(u1 + 5, 1, g_map.nearestFree(where + Vec2(0, i * 24), 20)));
        if (kind == 2) tg.push_back(putB(b1 + BR_POWER));
        if (kind == 3) tg.push_back(putB(b1 + BR_TURRET));
        if (kind == 4) for (int i = 0; i < 2; i++) tg.push_back(g_sim.spawnUnit(u1 + 8, 1, where + Vec2(0, i * 40)));
        if (kind == 5) tg.push_back(putB(b1 + BR_HQ));
        g_sim.updatePowerPublic();
        for (Ref r : tg) if (!g_sim.get(r)) { fprintf(stderr, "groundmatrix: scene failed\n"); return false; }
        // enemies that shoot back would muddy the result for weak units: make the targets harmless where they could kill the attacker
        for (Ref r : tg) { Entity* e = g_sim.get(r); if (e->isUnit()) e->disabledUntil = 1e9f; else if (e->weapon() >= 0) e->disabledUntil = 1e9f; }
        float h0 = 0; for (Ref r : tg) h0 += g_sim.get(r)->hp;
        Vec2 tc = g_sim.get(tg[0])->pos;
        if (mode == 1) g_sim.cmdMove({unit}, tc, true);
        else if (mode == 2) g_sim.cmdGuardArea({unit}, tc, 6 * TILE);
        else if (mode == 3) g_sim.cmdAttack({unit}, tg[0]);
        float firstDmg = -1;
        for (int t = 0; t < 20 * 60; t++) {
            g_sim.step(); g_sim.events.clear();
            if (firstDmg < 0 && (t % 4) == 0) { float hh = 0; for (Ref r : tg) if (Entity* e = g_sim.get(r)) hh += e->hp; else hh = -1e9f; if (hh < h0 - 1) firstDmg = t * SIM_DT; }
        }
        runs++;
        if (firstDmg < 0 || firstDmg > 45) { bad++; Entity* j = g_sim.get(unit); fprintf(stderr, "groundmatrix FAIL %s %s %s vs %s: first damage %.1fs (order %d at %.0f,%.0f target %.0f,%.0f)\n", FACTION_NAME[fac[0]], U.name, modes[mode], kinds[kind], firstDmg, j ? (int)j->order : -1, j ? j->pos.x : 0.f, j ? j->pos.y : 0.f, tc.x, tc.y); }
    }
    printf("groundmatrix: %d runs, %d failed\n", runs, bad);
    return bad == 0;
}


// Defensive structures: every turret / anti-air battery must engage what its weapon allows, from every side, and never while unfinished.
static bool turretTest(u64 seed) {
    int bad = 0, runs = 0;
    const char* kinds[5] = { "infantry", "tank", "aircraft", "jet", "bomber" };
    for (int fi = 0; fi < 2; fi++) for (int tb = 0; tb < 2; tb++) for (int kind = 0; kind < 5; kind++) for (int side = 0; side < 4; side++) for (int fresh = 0; fresh < 2; fresh++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 100000; }
        int b0 = firstBuildOf(fac[0]), u1 = firstUnitOf(fac[1]);
        int type = b0 + (tb == 0 ? BR_TURRET : BR_AATURRET);
        const Weapon& W = WEAPONS[BUILDS[type].weapon];
        bool air = kind >= 2;
        if (air && !W.air) continue;
        if (!air && !W.ground) continue;
        Vec2 mid = g_map.nearestFree(g_sim.players[0].basePos + (g_sim.players[1].basePos - g_sim.players[0].basePos) * 0.5f, 40);
        flattenArena(mid, 22, 22);   // (the middle of the map is a lake)
        Ref tur;
        for (int dy = -8; dy < 8 && !tur.valid(); dy++) for (int dx = -8; dx < 8 && !tur.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, type, tx, ty)) tur = g_sim.placeBuilding(type, 0, tx, ty, !fresh); }
        // power plant so the turret is powered
        int pwType = b0 + BR_POWER;
        for (int dy = -8; dy < 8; dy++) for (int dx = 4; dx < 12; dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, pwType, tx, ty)) { g_sim.placeBuilding(pwType, 0, tx, ty, true); dy = 99; break; } }
        g_sim.updatePowerPublic();
        if (!tur.valid()) { fprintf(stderr, "turrettest: no spot\n"); return false; }
        Vec2 tp = g_sim.get(tur)->pos;
        float ang = side * 1.5708f; float dd = (W.range - 1.2f) * TILE;
        Vec2 at = tp + Vec2(std::cos(ang), std::sin(ang)) * dd;
        int ut = kind == 0 ? u1 + 2 : kind == 1 ? u1 + 5 : kind == 2 ? u1 + 8 : u1 + 11;
        if (kind == 4) ut = u1 + 8;
        Ref tg = g_sim.spawnUnit(ut, 1, g_map.nearestFree(at, 20));
        if (!g_sim.get(tg)) return false;
        g_sim.get(tg)->disabledUntil = 1e9f;     // sits still; its own weapons are off
        float h0 = g_sim.get(tg)->hp;
        float tx0 = dist(g_sim.get(tg)->pos, tp) / TILE;
        if (tx0 > W.range + 0.3f) continue;       // the blocking terrain pushed the target out of range: not a fair test
        for (int t = 0; t < 20 * 20; t++) { g_sim.step(); g_sim.events.clear(); }
        Entity* e = g_sim.get(tg);
        bool dealt = !e || e->hp < h0 - 1;
        runs++;
        if (fresh ? dealt : !dealt) { bad++; fprintf(stderr, "turrettest FAIL %s %s vs %s side %d %s: %s\n", FACTION_NAME[fac[0]], BUILDS[type].name, kinds[kind], side, fresh ? "unfinished" : "finished", fresh ? "an unfinished structure fired" : "no damage"); }
    }
    printf("turrettest: %d runs, %d failed\n", runs, bad);
    return bad == 0;
}


// Structure upgrades at the tech structure (Rugged, Defense Guns, Self-Repair) and the dozer repair crews that share out the work.
static bool upgradeTest(u64 seed) {
    auto fail = [](const char* m) { fprintf(stderr, "upgradetest: %s\n", m); return false; };
    auto steps = [](int n) { for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        for (int p = 0; p < 2; p++) { std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); g_sim.players[p].money = 100000; }
        int b0 = firstBuildOf(fac[0]), u0 = firstUnitOf(fac[0]), u1 = firstUnitOf(fac[1]);
        Player& pl = g_sim.players[0];
        // structures of player 0 on free ground near its base, placed at the first spot beyond the wanted distance in a direction
        auto put = [&](int type, float distTiles, float ang, int owner = 0) {
            Vec2 base = g_sim.players[owner].basePos;
            for (float extra = 0; extra < 8; extra += 0.5f) for (float da = 0; da < 6.2f; da += 0.2f) {
                Vec2 c = base + Vec2(std::cos(ang + da), std::sin(ang + da)) * ((distTiles + extra) * TILE);
                int tx = tileOf(c.x) - BUILDS[type].w / 2, ty = tileOf(c.y) - BUILDS[type].h / 2;
                if (!inMap(tx, ty) || !g_sim.canPlace(owner, type, tx, ty)) continue;
                return g_sim.placeBuilding(type, owner, tx, ty, true);
            }
            return NOREF;
        };
        Ref power = put(b0 + BR_POWER, 5, 0.0f), power2 = put(b0 + BR_POWER, 5, 1.2f), tech = put(b0 + BR_TECH, 6, 2.4f), barracks = put(b0 + BR_BARRACKS, 6, 3.6f);
        for (Ref r : { power, power2, tech, barracks }) if (!g_sim.get(r)) return fail("could not set the scene up");
        g_sim.updatePowerPublic();
        // ---- research rules
        if (g_sim.upgradeAvailable(1, UPG_RUGGED) || g_sim.cmdUpgrade(1, UPG_RUGGED)) return fail("an upgrade started without a tech structure");
        for (int u = 0; u < UPG_COUNT; u++) {
            int m = pl.money;
            if (!g_sim.cmdUpgrade(0, u)) return fail("upgrade refused");
            if (pl.money != m - UPGRADES[fac[0]][u].cost) return fail("upgrade cost not charged");
            if (g_sim.cmdUpgrade(0, u)) return fail("the same upgrade started twice");
        }
        float hp0 = g_sim.get(barracks)->maxHp;
        for (int guard = 0; guard < 20 * 120 && !(pl.upg[0] && pl.upg[1] && pl.upg[2]); guard++) steps(1);
        if (!(pl.upg[UPG_RUGGED] && pl.upg[UPG_GUNS] && pl.upg[UPG_REPAIR])) return fail("upgrades never finished");
        if (g_sim.upgradeAvailable(0, UPG_GUNS) || g_sim.cmdUpgrade(0, UPG_GUNS)) return fail("a finished upgrade can be bought again");
        if (std::abs(g_sim.get(barracks)->maxHp - hp0 * RUGGED_HP * (pl.upg[UPG_HYPER] ? HYPER_HP : 1.0f)) > 1) return fail("Rugged/Hyper Protection did not toughen a standing structure");
        // ---- they last: the tech structure falls, a new structure still gets everything
        g_sim.destroy(*g_sim.get(tech), true);
        Ref fresh = put(b0 + BR_BARRACKS, 9, 5.0f);
        if (!g_sim.get(fresh)) return fail("no room for a new structure");
        if (std::abs(g_sim.get(fresh)->maxHp - BUILDS[b0 + BR_BARRACKS].hp * RUGGED_HP * (pl.upg[UPG_HYPER] ? HYPER_HP : 1.0f)) > 1) return fail("a structure built after the tech structure fell is not rugged");
        if (g_sim.roofGun(*g_sim.get(fresh)) < 0) return fail("a new structure has no roof gun");
        // ---- self-repair heals structures, never units
        Entity* br = g_sim.get(barracks);
        br->hp = br->maxHp * 0.4f; br->lastDamaged = g_sim.time - 10;
        Ref tank = g_sim.spawnUnit(u0 + 5, 0, g_map.nearestFree(pl.basePos + Vec2(0, 90), 10));
        g_sim.get(tank)->hp = g_sim.get(tank)->maxHp * 0.5f;
        float bh = br->hp, th = g_sim.get(tank)->hp;
        steps(20 * 5);
        if (!g_sim.get(barracks) || g_sim.get(barracks)->hp < bh + g_sim.get(barracks)->maxHp * SELF_REPAIR_RATE * 4) return fail("self-repair did not mend a structure");
        if (!g_sim.get(tank) || g_sim.get(tank)->hp > th + 0.01f) return fail("self-repair healed a unit");
        g_sim.destroy(*g_sim.get(tank), false);
        // ---- roof guns: a plain structure shoots aircraft, vehicles and infantry
        const char* kinds[3] = { "aircraft", "tank", "infantry" };
        int types[3] = { u1 + 8, u1 + 5, u1 + 2 };
        for (int k = 0; k < 3; k++) {
            Entity* b = g_sim.get(barracks);
            b->hp = b->maxHp;
            Vec2 at = b->pos + Vec2(0, (b->bt().h * 0.5f + 4.0f) * TILE);
            Ref foe = g_sim.spawnUnit(types[k], 1, k == 0 ? at : g_map.nearestFree(at, 12));
            Entity* f = g_sim.get(foe);
            if (dist(f->pos, b->pos) / TILE > WEAPONS[W_DEFENSE_LASER].range + 1.5f) { g_sim.destroy(*f, false); continue; }
            f->disabledUntil = 1e9f;
            float h0 = f->hp;
            steps(20 * 6);
            f = g_sim.get(foe);
            if (f && f->hp > h0 - 1) { fprintf(stderr, "upgradetest %s: the roof gun never hit the %s\n", FACTION_NAME[fac[0]], kinds[k]); return false; }
            if (f) g_sim.destroy(*f, false);
        }
        // ---- Rugged under nukes: one warhead at ground zero leaves a barracks standing; a short barrage flattens it (Hyper Protection adds a cap)
        {
            Entity* b = g_sim.get(barracks); b->hp = b->maxHp;
            Vec2 gz = b->pos;
            g_sim.nukes.push_back({g_sim.players[1].basePos, gz, 1, Sim::NUKE_FLIGHT - 0.05f});
            steps(4);
            if (!g_sim.get(barracks)) return fail("one nuke flattened a rugged barracks");
            if (!pl.upg[UPG_HYPER] && g_sim.get(barracks)->hp > g_sim.get(barracks)->maxHp * 0.6f) return fail("the nuke barely scratched a rugged barracks");
            for (int k = 0; k < 3 && g_sim.get(barracks); k++) { g_sim.nukes.push_back({g_sim.players[1].basePos, gz, 1, Sim::NUKE_FLIGHT - 0.05f}); steps(4); }
            if (g_sim.get(barracks)) return fail("repeated nukes at ground zero did not bring a rugged barracks down");
        }
    }
    // ---- Rugged plating: a small group of tanks does half damage, a massive assault at once swamps it and does full damage
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        float perTank[2] = {};
        for (int big = 0; big < 2; big++) {
            g_sim.init(2, fac, ai, diff, team, seed + 3 + fi);
            g_sim.players[0].upg[UPG_RUGGED] = true;
            int b0 = firstBuildOf(fac[0]), u1 = firstUnitOf(fac[1]);
            Ref bk;
            Vec2 base = g_sim.players[0].basePos;
            for (int ring = 6; ring < 14 && !bk.valid(); ring++) for (int dy = -ring; dy <= ring && !bk.valid(); dy++) for (int dx = -ring; dx <= ring && !bk.valid(); dx++)
                if ((std::abs(dx) == ring || std::abs(dy) == ring) && g_sim.canPlace(0, b0 + BR_BARRACKS, tileOf(base.x) + dx, tileOf(base.y) + dy)) bk = g_sim.placeBuilding(b0 + BR_BARRACKS, 0, tileOf(base.x) + dx, tileOf(base.y) + dy, true);
            if (!g_sim.get(bk)) return fail("no room for the plating test");
            Entity* b = g_sim.get(bk);
            b->maxHp = b->hp = 1e6f;   // (never falls during the measurement, so the load is a share of a fixed health: set it like a 3000 hp structure)
            int n = big ? 16 : 2;
            std::vector<Ref> tanks;
            for (int k = 0; k < n; k++) { float a = k * 6.283f / n; tanks.push_back(g_sim.spawnUnit(u1 + 5, 1, g_map.nearestFree(b->pos + Vec2(std::cos(a), std::sin(a)) * 3.2f * TILE, 6))); }
            g_sim.cmdAttack(tanks, bk);
            b = g_sim.get(bk); b->maxHp = 3000 * RUGGED_HP; b->hp = 1e6f;
            steps(20 * 4);
            b = g_sim.get(bk); float h0 = b->hp;
            steps(20 * 6);
            b = g_sim.get(bk);
            perTank[big] = (h0 - b->hp) / n;
        }
        if (perTank[0] <= 0) return fail("the tanks never hit the rugged structure");
        if (perTank[1] < perTank[0] * 1.5f) { fprintf(stderr, "upgradetest: a massive assault did not swamp the plating (%.0f vs %.0f per tank)\n", perTank[1], perTank[0]); return false; }
    }
    // ---- dozers share out the repairs
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + 7 + fi);
        g_sim.players[0].money = 100000;
        int b0 = firstBuildOf(fac[0]), u0 = firstUnitOf(fac[0]);
        auto put = [&](int type, float distTiles, float ang) {
            Vec2 base = g_sim.players[0].basePos;
            for (float extra = 0; extra < 8; extra += 0.5f) for (float da = 0; da < 6.2f; da += 0.2f) {
                Vec2 c = base + Vec2(std::cos(ang + da), std::sin(ang + da)) * ((distTiles + extra) * TILE);
                int tx = tileOf(c.x) - BUILDS[type].w / 2, ty = tileOf(c.y) - BUILDS[type].h / 2;
                if (!inMap(tx, ty) || !g_sim.canPlace(0, type, tx, ty)) continue;
                return g_sim.placeBuilding(type, 0, tx, ty, true);
            }
            return NOREF;
        };
        std::vector<Ref> blds = { put(b0 + BR_POWER, 6, 0.0f), put(b0 + BR_BARRACKS, 6, 1.5f), put(b0 + BR_POWER, 6, 3.0f), put(b0 + BR_SUPPLY, 7, 4.5f) };
        for (Ref r : blds) if (!g_sim.get(r)) return fail("could not set the repair scene up");
        std::vector<Ref> dz;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0) dz.push_back(g_sim.refOf(e));
        while (dz.size() < 5) dz.push_back(g_sim.spawnUnit(u0, 0, g_map.nearestFree(g_sim.players[0].basePos + Vec2((float)dz.size() * 26.0f - 60.0f , 70), 10)));
        auto hurt = [&](Ref r, float f) { Entity* b = g_sim.get(r); b->hp = b->maxHp * f; b->lastDamaged = -100; };
        auto crews = [&](Ref r) { Entity* b = g_sim.get(r); return b ? g_sim.repairCrew(*b) : 0; };
        // one lightly damaged structure: one dozer goes, the rest stay free
        hurt(blds[1], 0.85f);
        steps(25);
        if (crews(blds[1]) != 1) { fprintf(stderr, "upgradetest: %d dozers went to mend one scratched structure\n", crews(blds[1])); return false; }
        // four damaged structures: the idle dozers spread over them
        for (Ref r : blds) hurt(r, 0.3f);
        steps(25);
        int busy = 0, worst = 0;
        for (Ref r : blds) { int c = crews(r); if (c > 0) busy++; worst = std::max(worst, c); }
        if (busy < 4) { fprintf(stderr, "upgradetest: idle dozers covered only %d of 4 damaged structures (most on one: %d)\n", busy, worst); return false; }
        // the player sends all five to one structure: its crew is capped and the others fan out to the damaged ones around it
        for (Ref r : blds) { hurt(r, 0.3f); }
        for (Ref d : dz) g_sim.cmdStop({d});
        g_sim.cmdAssist(dz, blds[0]);
        int c0 = crews(blds[0]); busy = 0;
        for (Ref r : blds) if (crews(r) > 0) busy++;
        if (c0 > std::max(2, Sim::repairCrewCap(*g_sim.get(blds[0]))) || busy < 3) { fprintf(stderr, "upgradetest: a group repair order piled %d dozers on one structure (%d covered)\n", c0, busy); return false; }
        // and the work gets done
        steps(20 * 40);
        for (Ref r : blds) { Entity* b = g_sim.get(r); if (!b || b->hp < b->maxHp * 0.99f) return fail("the dozers did not finish the repairs"); }
    }
    printf("upgradetest: ok\n");
    return true;
}

// Helicopters at the airfields: both armies build them, as many as they like from one airfield (planes still need a free pad each),
// and they park on helipads around it.
static bool heliTest(u64 seed) {
    auto steps = [](int n) { for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } };
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        g_sim.players[0].money = 200000;
        std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
        flattenArena(g_sim.players[0].basePos, 42, 42);   // room for the enlarged 10x7 airbase
        int b0 = firstBuildOf(fac[0]);
        auto fail = [&](const char* m) { fprintf(stderr, "helitest %s: %s\n", FACTION_NAME[fac[0]], m); return false; };
        auto put = [&](int type, float distTiles, float ang) {
            Vec2 base = g_sim.players[0].basePos;
            for (float extra = 0; extra < 8; extra += 0.5f) for (float da = 0; da < 6.2f; da += 0.2f) {
                Vec2 c = base + Vec2(std::cos(ang + da), std::sin(ang + da)) * ((distTiles + extra) * TILE);
                int tx = tileOf(c.x) - BUILDS[type].w / 2, ty = tileOf(c.y) - BUILDS[type].h / 2;
                if (!inMap(tx, ty) || !g_sim.canPlace(0, type, tx, ty)) continue;
                return g_sim.placeBuilding(type, 0, tx, ty, true);
            }
            return NOREF;
        };
        Ref pw = put(b0 + BR_POWER, 5, 0.0f), pw2 = put(b0 + BR_POWER, 5, 1.5f), fac0 = put(b0 + BR_FACTORY, 6, 3.0f), tech = put(b0 + BR_TECH, 6, 4.5f), air = put(b0 + BR_AIRFIELD, 8, 2.2f);
        if (!g_sim.get(air)) {   // the oversized airbase needs more room than the original 5x3 pad: widen the search before giving up
            for (float extra = 8; extra < 40 && !g_sim.get(air); extra += 1.0f)
                for (float da = 0; da < 6.2f && !g_sim.get(air); da += 0.1f) {
                    Vec2 c = g_sim.players[0].basePos + Vec2(std::cos(2.2f + da), std::sin(2.2f + da)) * ((8 + extra) * TILE);
                    int tx = tileOf(c.x) - BUILDS[b0 + BR_AIRFIELD].w / 2, ty = tileOf(c.y) - BUILDS[b0 + BR_AIRFIELD].h / 2;
                    if (inMap(tx, ty) && g_sim.canPlace(0, b0 + BR_AIRFIELD, tx, ty)) air = g_sim.placeBuilding(b0 + BR_AIRFIELD, 0, tx, ty, true);
                }
        }
        for (Ref r : { pw, pw2, fac0, tech, air }) if (!g_sim.get(r)) return fail("could not set the scene up");
        g_sim.updatePowerPublic();
        int heli = -1, plane = -1;
        for (int u = 0; u < U_COUNT; u++) if (UNITS[u].faction == fac[0] && UNITS[u].builtBy == BR_AIRFIELD) { if (UNITS[u].heli && heli < 0) heli = u; if (!UNITS[u].heli && plane < 0) plane = u; }
        if (heli < 0) return fail("the airfield builds no helicopter");
        if (plane < 0) return fail("the airfield builds no plane");
        auto count = [&](int type) { int n = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.type == type) n++; return n; };
        // two full queues of helicopters from one airfield: far beyond its four pads, and every one of them comes out
        for (int round = 0; round < 2; round++) {
            for (int k = 0; k < 9; k++) if (!g_sim.cmdTrain(air, heli)) return fail("could not queue a helicopter");
            for (int guard = 0; guard < 20 * 260 && !g_sim.get(air)->queue.empty(); guard++) steps(1);
            if (!g_sim.get(air)->queue.empty()) { fprintf(stderr, "helitest %s: production stalled with %d helicopters out\n", FACTION_NAME[fac[0]], count(heli)); return false; }
        }
        if (count(heli) != 18) return fail("not every helicopter was built");
        if (g_sim.padsUsed(*g_sim.get(air)) != 0) return fail("helicopters took up plane pads");
        // they settle on helipads around the airfield: all of them land close by, none on top of each other in a pile
        steps(20 * 25);
        int parked = 0, crowded = 0;
        std::vector<Vec2> spots;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == heli) { if (dist(e.pos, g_sim.get(air)->pos) < 8 * TILE && e.order == O_IDLE) parked++; spots.push_back(e.pos); }
        const float gap = HELI_SPACE - 4.0f;   // hulls never touch: a few pixels of air between neighbouring helicopters
        for (size_t i = 0; i < spots.size(); i++) for (size_t j = i + 1; j < spots.size(); j++) if (dist(spots[i], spots[j]) < gap) crowded++;
        if (parked < 18) { fprintf(stderr, "helitest %s: only %d of 18 helicopters settled at their airfield\n", FACTION_NAME[fac[0]], parked); return false; }
        if (crowded > 0) { fprintf(stderr, "helitest %s: %d helicopter pairs parked on top of each other\n", FACTION_NAME[fac[0]], crowded); return false; }
        std::vector<Ref> helis;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == heli) helis.push_back(g_sim.refOf(e));
        auto pairsTooClose = [&]() { int n = 0; for (size_t i = 0; i < helis.size(); i++) for (size_t j = i + 1; j < helis.size(); j++) { Entity* a = g_sim.get(helis[i]); Entity* b = g_sim.get(helis[j]); if (a && b && dist(a->pos, b->pos) < gap) { n++; if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "  pair %d(%.0f,%.0f o%d v%.0f) %d(%.0f,%.0f o%d v%.0f) d %.1f\n", helis[i].idx, a->pos.x, a->pos.y, (int)a->order, a->airspeed, helis[j].idx, b->pos.x, b->pos.y, (int)b->order, b->airspeed, dist(a->pos, b->pos)); } } return n; };
        // all of them sent to one spot: they stop side by side around it, a small gap apart, not in one pile
        Vec2 meet = g_map.nearestFree(g_sim.get(air)->pos + Vec2(10 * TILE, -6 * TILE), 20);
        g_sim.cmdMove(helis, meet, false);
        steps(20 * 45);
        if (int n = pairsTooClose()) { fprintf(stderr, "helitest %s: %d helicopter pairs piled up on a move order\n", FACTION_NAME[fac[0]], n); return false; }
        for (Ref r : helis) if (dist(g_sim.get(r)->pos, meet) > 5 * TILE) return fail("a helicopter of a group move stopped far from the spot");
        // guarding an area: each one hovers still on a spot of its own (no circling)
        g_sim.cmdGuardArea(helis, meet, 3 * TILE);
        steps(20 * 15);
        std::vector<Vec2> was; for (Ref r : helis) was.push_back(g_sim.get(r)->pos);
        steps(20 * 4);
        for (size_t i = 0; i < helis.size(); i++) {
            Entity* h = g_sim.get(helis[i]);
            if (h->order != O_GUARDAREA) return fail("a helicopter dropped its guard order");
            if (dist(h->pos, was[i]) > 3.0f) { fprintf(stderr, "helitest %s: a guarding helicopter moved %.0f px in 4 s\n", FACTION_NAME[fac[0]], dist(h->pos, was[i])); return false; }
        }
        if (int n = pairsTooClose()) { fprintf(stderr, "helitest %s: %d helicopter pairs share a guard spot\n", FACTION_NAME[fac[0]], n); return false; }
        g_sim.cmdStop(helis);   // (back to the airfield for the rest of the test)
        steps(20 * 25);
        // planes still wait for a free pad: three full queues, 25 come out, the rest wait
        for (int batch = 0; batch < 3; batch++) {
            for (int k = 0; k < 9; k++) if (!g_sim.cmdTrain(air, plane)) return fail("could not queue a plane");
            steps(20 * 450);
        }
        if (count(plane) != AIRFIELD_CAP || g_sim.get(air)->queue.size() != 2) { fprintf(stderr, "helitest %s: %d planes out, %zu waiting (want %d and 2)\n", FACTION_NAME[fac[0]], count(plane), g_sim.get(air)->queue.size(), AIRFIELD_CAP); return false; }
        {   // each plane parks on a pad of its own (never two on one pad)
            std::vector<Vec2> pads; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == plane) pads.push_back(e.pos);
            for (size_t i = 0; i < pads.size(); i++) for (size_t j = i + 1; j < pads.size(); j++) if (dist(pads[i], pads[j]) < 20) { fprintf(stderr, "helitest %s: two planes parked on one pad\n", FACTION_NAME[fac[0]]); return false; }
        }
        // and a helicopter queued behind the waiting planes is not held up by them for ever: cancel the planes, it comes out
        g_sim.cmdCancelTrain(air, 0); g_sim.cmdCancelTrain(air, 0);
        if (!g_sim.cmdTrain(air, heli)) return fail("could not queue a helicopter after the planes");
        steps(20 * 30);
        if (count(heli) != 19) return fail("a helicopter was not built once the pads were full of planes");
        {   // a plane lost and replaced: the newcomer's entity index can sit four apart from a parked plane's; it still lands on a pad of its own
            std::vector<Ref> pls; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == plane) pls.push_back(g_sim.refOf(e));
            if (pls.size() < 2) return fail("planes missing");
            Ref keep = pls[0];
            g_sim.destroy(*g_sim.get(pls[1]), false);
            Vec2 away = g_map.nearestFree(g_sim.players[0].basePos + Vec2(0, 6 * TILE), 20);
            Ref nb;
            for (int k = 0; k < 64 && !nb.valid(); k++) {
                Ref f = g_sim.spawnUnit(firstUnitOf(fac[0]) + 2, 0, away);   // (a trooper as filler)
                if (f.idx != keep.idx && (f.idx - keep.idx) % AIRFIELD_CAP == 0) { g_sim.destroy(*g_sim.get(f), false); nb = g_sim.spawnUnit(plane, 0, g_sim.get(air)->pos + Vec2(0, -6 * TILE)); }
            }
            if (!nb.valid() || (nb.idx - keep.idx) % AIRFIELD_CAP != 0) return fail("could not set up two planes on the same pad slot");
            g_sim.get(nb)->home = air;
            steps(20 * 40);
            std::vector<Vec2> at; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == plane) at.push_back(e.pos);
            for (size_t i = 0; i < at.size(); i++) for (size_t j = i + 1; j < at.size(); j++) if (dist(at[i], at[j]) < 20) { fprintf(stderr, "helitest %s: a replacement plane parked on another plane's pad\n", FACTION_NAME[fac[0]]); return false; }
        }
    }
    printf("helitest: ok\n");
    return true;
}

// Command fuzzer: player 0 issues random commands (every command, random arguments, including nonsense) while the computer plays the others.
// Looks for crashes (run it under the sanitizers) and broken invariants.
static bool fuzzTest(u64 seed, int seconds) {
    Rng rng(seed * 7919 + 1);
    Faction fac[4] = { (Faction)rng.range(0, 1), F_CLANKER, F_CYBER, F_CLANKER };
    bool ai[4] = { false, true, true, true };
    int diff[4] = { 1, 2, 2, 1 }; int team[4] = { 0, 1, 2, 3 };
    g_sim.init(4, fac, ai, diff, team, seed);
    g_ai.init(seed);
    g_sim.players[0].money = 200000;
    int n = seconds * SIM_HZ;
    long cmds = 0;
    auto anyEnt = [&](int owner, int kindMask) -> Ref {   // random live entity (owner -2 = anyone); kindMask bit0 unit, bit1 building, bit2 resource
        for (int tries = 0; tries < 30; tries++) {
            if (g_sim.ents.empty()) break;
            int i = rng.range(0, (int)g_sim.ents.size() - 1);
            Entity& e = g_sim.ents[i];
            if (!e.alive) continue;
            if (owner != -2 && e.owner != owner) continue;
            if (!(kindMask & (1 << (int)e.kind))) continue;
            return g_sim.refOf(i);
        }
        return NOREF;
    };
    auto sel = [&]() { std::vector<Ref> v; int k = rng.range(1, 8); for (int i = 0; i < k; i++) { Ref r = rng.range(0, 9) == 0 ? Ref{rng.range(-3, 5000), (u32)rng.range(0, 3)} : anyEnt(0, 3); v.push_back(r); } return v; };
    auto where = [&]() { return Vec2(rng.f(-200, WORLD_W + 200), rng.f(-200, WORLD_H + 200)); };
    for (int t = 0; t < n && !g_sim.gameOver; t++) {
        if (t % 6 == 0) {
            int c = rng.range(0, 36);
            cmds++;
            switch (c) {
            case 0: g_sim.cmdMove(sel(), where(), rng.range(0, 1)); break;
            case 1: { Ref r = anyEnt(rng.range(-2, 3), 7); g_sim.cmdAttack(sel(), r); break; }
            case 2: g_sim.cmdStop(sel()); break;
            case 3: { Ref r = anyEnt(-1, 4); g_sim.cmdHarvest(sel(), r); break; }
            case 4: g_sim.cmdGuardArea(sel(), where(), rng.f(-50, 700)); break;
            case 5: g_sim.cmdGatherArea(sel(), where(), rng.f(-50, 700)); break;
            case 6: g_sim.cmdArea(sel(), where(), rng.f(-50, 700)); break;
            case 7: { Ref d = anyEnt(0, 1); int bt = rng.range(0, B_COUNT - 1); g_sim.cmdBuild(d, bt, rng.range(-3, MAP_W + 3), rng.range(-3, MAP_H + 3)); break; }
            case 8: { Ref d = sel()[0]; Ref b = anyEnt(rng.range(-1, 1), 2); g_sim.cmdAssist({d}, b); break; }
            case 9: { Ref b = anyEnt(0, 2); g_sim.cmdTrain(b, rng.range(0, U_COUNT - 1)); break; }
            case 10: { Ref b = anyEnt(0, 2); g_sim.cmdCancelTrain(b, rng.range(-1, 6)); break; }
            case 11: { Ref b = anyEnt(0, 2); g_sim.cmdSetRally(b, where()); break; }
            case 12: if (rng.range(0, 5) == 0) { Ref b = anyEnt(0, 2); g_sim.cmdSell(b); } break;
            case 13: g_sim.cmdPower(0, where()); break;
            case 14: g_sim.cmdNuke(0, where()); break;
            case 26: { Ref b2 = anyEnt(rng.range(-1, 2), 3); g_sim.cmdMiniNuke(rng.range(-1, 4), where(), rng.range(0, 1) == 0, b2); break; }   // a bunker's small missile (any player id, any structure, or nothing)
            case 15: g_sim.cmdScan(0); break;
            case 16: g_sim.cmdResearch(0); break;
            case 17: { int bt = rng.range(0, B_COUNT - 1); g_sim.canPlace(0, bt, rng.range(-5, MAP_W + 5), rng.range(-5, MAP_H + 5)); g_sim.unitAvailable(0, rng.range(0, U_COUNT - 1)); g_sim.buildAvailable(0, bt); break; }
            case 19: g_sim.cmdUpgrade(rng.range(-1, 2) == 0 ? 0 : rng.range(-1, 4), rng.range(-1, UPG_COUNT)); break;
            case 18: g_sim.players[0].dropReady = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding()) e.dropTimer = std::min(e.dropTimer, g_sim.time + 3.0f); g_sim.cmdParadrop(0, where()); break;
            case 20: g_sim.cmdAidDrop(rng.range(-1, 4), where()); break;   // (any player id: only the human's may ever fly)
            case 21: { Ref c2 = anyEnt(rng.range(-2, 3), 3); g_sim.cmdEnter(sel(), c2); break; }                                  // board a lifter / bunker (anybody's, or nothing)
            case 22: { Ref c2 = anyEnt(0, 3); g_sim.cmdUnload(c2, rng.range(0, 1) == 0, where()); break; }
            case 23: { Ref b2 = anyEnt(rng.range(-2, 3), 2); g_sim.cmdCapture(sel(), b2); break; }
            case 27: g_sim.cmdPhoneStrike(rng.range(-1, 4), where()); break;                                                       // the swarm and the two special fighters (any player id)
            case 28: { Ref c3 = anyEnt(rng.range(-2, 3), 3); std::vector<Ref> s2 = sel(); g_sim.cmdEnter(s2, c3); if (rng.range(0, 3) == 0) g_sim.cmdUnload(c3, false, Vec2()); break; }   // board / unload jeeps too
            case 29: { Vec2 w2 = where(); g_sim.nukes.push_back({g_sim.players[rng.range(0, 3)].basePos, w2, rng.range(0, 3), Sim::NUKE_FLIGHT - rng.f(0.0f, 5.0f), rng.range(0, 1) == 0, rng.range(0, 1) ? 1.0f : MINI_NUKE_SCALE}); break; }   // a warhead in flight: the anti-nuke defences meet it
            case 24: { int ty = U_C_SPY + rng.range(0, 13); g_sim.spawnUnit(ty, rng.range(0, 3) == 0 ? rng.range(0, 3) : 0, g_sim.players[0].basePos + Vec2(rng.f(-200, 200), rng.f(-200, 200))); break; }   // the new kinds, for whoever
            case 30: { int bt = B_C_HOUSE + rng.range(0, 2 * CV_COUNT + 1); int tx = tileOf(g_sim.players[0].basePos.x) + rng.range(-14, 14), ty = tileOf(g_sim.players[0].basePos.y) + rng.range(-14, 14); if (g_sim.canPlace(0, bt, tx, ty)) g_sim.placeBuilding(bt, rng.range(0, 3) == 0 ? rng.range(0, 3) : 0, tx, ty, rng.range(0, 1) == 0); if (rng.range(0, 2) == 0) g_sim.spawnUnit(rng.range(0, 1) ? U_C_CIVDOZER : U_K_CIVDOZER, rng.range(0, 3), g_sim.players[0].basePos + Vec2(rng.f(-200, 200), rng.f(-200, 200))); break; }   // the civilian town, its dozers and the silos, for whoever
            case 31: { Ref b2 = anyEnt(rng.range(-1, 2), 3); g_sim.cmdDeepNuke(rng.range(-1, 4), where(), rng.range(0, 1) == 0, b2); if (rng.range(0, 3) == 0) { Sim::Nuke n; n.from = g_sim.players[0].basePos; n.pos = where(); n.owner = rng.range(0, 3); n.t = Sim::NUKE_FLIGHT - rng.f(0.0f, 6.5f); n.scale = UNUKE_SCALE; n.deep = true; g_sim.nukes.push_back(n); } break; }   // silver warheads
            case 32: { Ref b2 = anyEnt(rng.range(-2, 3), 3); g_sim.cmdIslandNuke(rng.range(-1, 4), where(), rng.range(0, 1) == 0, b2); if (rng.range(0, 4) == 0) for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE) { e.owner = rng.range(-1, 1); e.siloTimer = 0; } break; }   // island launchers: any owner, fired at anything
            case 33: { if (rng.range(0, 2) == 0) for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_TECH) e.bomberTimer = 0; g_sim.cmdNukeBomber(rng.range(-1, 4), where()); break; }   // the Nuclear Bomber call-in, anywhere
            case 35: {   // the bunkers' Rain Nukes, Tank Drop and Upgrade All (any upgrades, cooldowns, owners and spots)
                Ref b2 = anyEnt(rng.range(-1, 2), 3); Entity* be = g_sim.get(b2);
                if (be && rng.range(0, 1) == 0) { be->bunkerUpg |= (u8)(1 << rng.range(0, BU_COUNT - 1)); be->rainReady = be->tankReady = 0; }
                int k = rng.range(0, 3);
                if (k == 0) g_sim.cmdBunkerRain(b2, where(), rng.range(0, 1) == 0);
                else if (k == 1) g_sim.cmdTankDrop(b2, where(), rng.range(0, 1) == 0);
                else if (k == 2) g_sim.cmdBunkerUpgradeAll(rng.range(-1, 4));
                else g_sim.cancelBunkerQueue(rng.range(0, 3));
                break;
            }
            case 36: {   // The Chosen and its Shadow Guard, for whoever: calls, turnings and captures of anything (aircraft, loans, structures, other Chosen)
                int ow = rng.range(0, 3);
                Ref ch = g_sim.chosenOf(ow);
                if (!ch.valid() || rng.range(0, 3) == 0) ch = g_sim.spawnUnit(rng.range(0, 1) ? U_C_CHOSEN : U_K_CHOSEN, ow, g_map.nearestFree(g_sim.players[rng.range(0, 3)].basePos + Vec2(rng.f(-300, 300), rng.f(-300, 300)), 10));
                if (rng.range(0, 2) == 0) g_sim.players[ow].shadowReady = 0;
                g_sim.cmdShadowCall(rng.range(-1, 4), rng.range(0, 1) ? ch : anyEnt(-2, 3));
                Entity* ce = g_sim.get(ch); Entity* t = g_sim.get(anyEnt(-2, 3));
                if (ce && t) { ce->cooldown = 0; if (rng.range(0, 1)) g_sim.chosenAct(*ce, *t); else g_sim.cmdAttack({ch}, g_sim.refOf(*t)); }
                if (ce && t && t->isUnit() && rng.range(0, 3) == 0) g_sim.convertUnit(*t, rng.range(0, 3), ce);
                break;
            }
            case 34: { int bt = rng.range(0, 1) ? B_C_FWALL : B_K_FWALL; int tx = tileOf(g_sim.players[0].basePos.x) + rng.range(-12, 12), ty = tileOf(g_sim.players[0].basePos.y) + rng.range(-12, 12); if (g_sim.canPlace(0, bt, tx, ty)) g_sim.placeBuilding(bt, rng.range(0, 2) == 0 ? rng.range(-1, 1) : 0, tx, ty, rng.range(0, 1) == 0); break; }   // Bastion Walls, any owner
            case 25: { int bt = B_C_BUNKER + rng.range(0, 5); int tx = tileOf(g_sim.players[0].basePos.x) + rng.range(-12, 12), ty = tileOf(g_sim.players[0].basePos.y) + rng.range(-12, 12); if (g_sim.canPlace(0, bt, tx, ty)) g_sim.placeBuilding(bt, rng.range(0, 3) == 0 ? rng.range(0, 3) : 0, tx, ty, rng.range(0, 1) == 0); break; }
            }
        }
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        for (auto& a : g_sim.aidDrops) if (a.owner < 0 || g_sim.players[a.owner].isAI || a.recipient == a.owner) { fprintf(stderr, "fuzz: an aid flight from a computer army, or back to its sender, at tick %d\n", t); return false; }
        for (auto& e : g_sim.ents) {
            if (!e.alive) continue;
            if (!(e.pos.x == e.pos.x) || !(e.pos.y == e.pos.y)) { fprintf(stderr, "fuzz: NaN position at tick %d (%s)\n", t, e.isUnit() ? e.ut().name : "?"); return false; }
            if (e.pos.x < 0 || e.pos.y < 0 || e.pos.x > WORLD_W || e.pos.y > WORLD_H) { fprintf(stderr, "fuzz: entity out of world at tick %d (%s %.0f,%.0f)\n", t, e.isUnit() ? e.ut().name : "building", e.pos.x, e.pos.y); return false; }
            if (e.isUnit() && !e.isAir() && e.fall <= 0 && !e.carrier.valid() && (e.ut().naval ? !g_map.navTile(tileOf(e.pos.x), tileOf(e.pos.y)) : !(g_map.terrainPassable(tileOf(e.pos.x), tileOf(e.pos.y)) || (Sim::swimmer(e) && g_map.swimTile(tileOf(e.pos.x), tileOf(e.pos.y)))))) { fprintf(stderr, "fuzz: ground unit on impassable terrain at tick %d (%s)\n", t, e.ut().name); return false; }
            if (e.hp > e.maxHp + 0.01f) { fprintf(stderr, "fuzz: hp above max at tick %d\n", t); return false; }
            if (e.isUnit() && e.ammo < 0) { fprintf(stderr, "fuzz: negative ammo\n"); return false; }
            if (e.kind != EK_RESOURCE && g_sim.players[e.owner >= 0 ? e.owner : 0].money < -1) { fprintf(stderr, "fuzz: negative money at tick %d\n", t); return false; }
        }
    }
    printf("fuzztest seed %llu: ok (%ld commands, %.0f s of play)\n", (unsigned long long)seed, cmds, g_sim.time);
    return true;
}

bool crowdTest(u64 seed);
bool frontierMapTest(u64 seed);
bool learningTest(u64 seed);
bool determinismTest(u64 seed, int seconds);
bool placementTest(u64 seed);
bool placementBench(u64 seed);

// ONEHOUR_ARTSHEET=<file.bmp>: headless preview of the sea art (the carrier, the cargo ship and the two carrier jets) for both armies, each in
// its owner's colour (Cyber PLAYER_COLOR[0], Clanker PLAYER_COLOR[1]), at 2x over sea blue. The big ships are on the top row, the jets below.
static void writeArtSheet(const char* path) {
    const int S = 2, GAP = 8, M = 8, bigW = ART_BIG * S, smW = ART_SIZE * S;
    Canvas out(M * 2 + 4 * bigW + 3 * GAP, M * 2 + bigW + GAP + smW);
    for (u32& p : out.px) p = Canvas::pack(rgb(30, 90, 140));
    auto put2x = [&](const Canvas& src, int x0, int y0) {   // nearest-neighbour 2x, alpha-blended onto the sheet
        for (int y = 0; y < src.h * S; y++) for (int x = 0; x < src.w * S; x++) out.set(x0 + x, y0 + y, Canvas::unpack(src.px[(y / S) * src.w + x / S]));
    };
    for (int f = 0; f < 2; f++) {   // f 0: Cyber, f 1: Clanker
        bool cy = f == 0;
        Canvas car(ART_BIG, ART_BIG), crg(ART_BIG, ART_BIG), cj(ART_SIZE, ART_SIZE), aj(ART_SIZE, ART_SIZE);
        artCarrierBody(car, PLAYER_COLOR[f], cy);
        artCargoShipBody(crg, PLAYER_COLOR[f], cy);
        artCarrierJetBody(cj, PLAYER_COLOR[f], cy);
        artAirstrikeJetBody(aj, PLAYER_COLOR[f], cy);
        put2x(car, M + f * (bigW + GAP), M);
        put2x(crg, M + (2 + f) * (bigW + GAP), M);
        put2x(cj, M + f * (bigW + GAP) + (bigW - smW) / 2, M + bigW + GAP);
        put2x(aj, M + (2 + f) * (bigW + GAP) + (bigW - smW) / 2, M + bigW + GAP);
    }
    if (SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(out.px.data(), out.w, out.h, 32, out.w * 4, SDL_PIXELFORMAT_ABGR8888)) { SDL_SaveBMP(sf, path); SDL_FreeSurface(sf); }
    printf("wrote %s\n", path);
}

// ONEHOUR_BUNKERSHEET=<file.bmp>: headless preview of both armies' bunkers (Cyber top row, Clanker bottom row) on grass, at the 2x art scale:
// the bare bunker, then the bunker with each per-bunker upgrade alone (BunkerUpg order), then with all of them. Live parts are not drawn.
static void writeBunkerSheet(const char* path) {
    const int GAP = 6, M = 8, N = BU_COUNT + 2;
    BArtSize sz = artBuildingSize(B_C_BUNKER);
    Canvas out(M * 2 + N * sz.w + (N - 1) * GAP, M * 2 + 2 * sz.h + GAP);
    for (int y = 0; y < out.h; y++) for (int x = 0; x < out.w; x++) out.px[y * out.w + x] = Canvas::pack(((x / 9 + y / 7) & 1) ? rgb(84, 118, 58) : rgb(78, 112, 54));
    auto put = [&](const Canvas& src, int x0, int y0) { for (int y = 0; y < src.h; y++) for (int x = 0; x < src.w; x++) out.set(x0 + x, y0 + y, Canvas::unpack(src.px[y * src.w + x])); };
    for (int f = 0; f < 2; f++) {
        int type = f == 0 ? B_C_BUNKER : B_K_BUNKER;
        Canvas base(sz.w, sz.h), mask(sz.w, sz.h);
        artBuilding(base, mask, type);
        for (int i = 0; i < (int)mask.px.size(); i++) {   // the team mask in the owner's colour, as the game tints it
            Color m = Canvas::unpack(mask.px[i]);
            if (m.a) { Color t = PLAYER_COLOR[f]; Color b0 = Canvas::unpack(base.px[i]); float k = m.a / 255.0f; base.px[i] = Canvas::pack(Color{(u8)(b0.r + (t.r * m.r / 255 - b0.r) * k), (u8)(b0.g + (t.g * m.g / 255 - b0.g) * k), (u8)(b0.b + (t.b * m.b / 255 - b0.b) * k), std::max(b0.a, m.a)}); }
        }
        std::vector<Canvas> ups(BU_COUNT, Canvas(sz.w, sz.h));
        for (int u = 0; u < BU_COUNT; u++) artBunkerUpgrade(ups[u], type, u);
        for (int cell = 0; cell < N; cell++) {
            int x0 = M + cell * (sz.w + GAP), y0 = M + f * (sz.h + GAP);
            put(base, x0, y0);
            for (int u = 0; u < BU_COUNT; u++) if (cell == u + 1 || cell == N - 1) put(ups[u], x0, y0);
        }
    }
    if (SDL_Surface* sf = SDL_CreateRGBSurfaceWithFormatFrom(out.px.data(), out.w, out.h, 32, out.w * 4, SDL_PIXELFORMAT_ABGR8888)) { SDL_SaveBMP(sf, path); SDL_FreeSurface(sf); }
    printf("wrote %s\n", path);
}

int main(int argc, char** argv) {
    if (const char* as = getenv("ONEHOUR_ARTSHEET")) { writeArtSheet(as); return 0; }   // preview only: no window, no game state
    if (const char* bs = getenv("ONEHOUR_BUNKERSHEET")) { writeBunkerSheet(bs); return 0; }
    int scale = 0; int reqW = 0, reqH = 0; bool software = false; bool headless = false;
    int selftestSecs = -1; const char* shot = nullptr; const char* sheetPath = nullptr; int ticks = 0; u64 seed = 12345; Faction faction = F_CYBER; int players = 4; int d0 = 3; bool swap = false; int viewPlayer = 0; bool allAi = false; int trainN = 0, evalN = 0, evalAiN = 0, evalDiffN = 0; bool autostart = false; int benchFrames = 0; int lateMin = 0;
    for (int i = 1; i < argc; i++) {   // the headless scene tests check one mechanic each in scenes that run for minutes: no riots in them (full matches - the game,
        std::string a = argv[i];         // --selftest, --fuzztest, --determinismtest, the AI evaluations - keep them; --civiltest switches them on where it tests them)
        if ((a.find("test") != std::string::npos || a.find("matrix") != std::string::npos || a == "--scenario") && a != "--selftest" && a != "--fuzztest" && a != "--determinismtest") Sim::riotsDefault = false;
    }
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
        else if (a == "--evaldiff") { evalDiffN = 8; if (i + 1 < argc && argv[i + 1][0] != '-') evalDiffN = atoi(argv[++i]); }
        else if (a == "--evalai") { evalAiN = 16; if (i + 1 < argc && argv[i + 1][0] != '-') evalAiN = atoi(argv[++i]); }
        else if (a == "--allai") allAi = true;
        else if (a == "--soundcheck") { g_audio.debugStats(); return 0; }
        else if (a == "--uitest") { headless = true; software = true; shot = nullptr; ticks = -1; }
        else if (a == "--autostart") autostart = true;
        else if (a == "--bench") { benchFrames = 300; if (i + 1 < argc && argv[i + 1][0] != '-') benchFrames = atoi(argv[++i]); headless = true; software = true; }
        else if (a == "--gpubench") {   // the same scene through the real (GPU) renderer in a window: what the GPU path costs (run it on a display, or Xvfb + llvmpipe)
            benchFrames = 300; if (i + 1 < argc && argv[i + 1][0] != '-') benchFrames = atoi(argv[++i]);
            if (reqW <= 0) { reqW = 1920; reqH = 1080; }
            SDL_SetHint(SDL_HINT_RENDER_VSYNC, "0");
        }
        else if (a == "--latebench") { lateMin = 30; if (i + 1 < argc && argv[i + 1][0] != '-') lateMin = atoi(argv[++i]); headless = true; software = true; }
        else if (a == "--scenario") { g_map.generate(); return scenarioTest(seed) ? 0 : 1; }
        else if (a == "--econtest") { g_map.generate(); return econTest(seed) ? 0 : 1; }
        else if (a == "--hqtest") { g_map.generate(); return hqTest(seed) ? 0 : 1; }
        else if (a == "--droptest") { g_map.generate(); return dropTest(seed) ? 0 : 1; }
        else if (a == "--striketest") { g_map.generate(); return strikeTest(seed) ? 0 : 1; }
        else if (a == "--haultest") { g_map.generate(); return haulTest(seed) ? 0 : 1; }
        else if (a == "--allytest") { g_map.generate(); return allyTest(seed) ? 0 : 1; }
        else if (a == "--aidtest") { g_map.generate(); return aidTest(seed) ? 0 : 1; }
        else if (a == "--supporttest") { g_map.generate(); return supportTest(seed) ? 0 : 1; }
        else if (a == "--airmatrix") { g_map.generate(); return airMatrixTest(seed) ? 0 : 1; }
        else if (a == "--groundmatrix") { g_map.generate(); return groundMatrixTest(seed) ? 0 : 1; }
        else if (a == "--turrettest") { g_map.generate(); return turretTest(seed) ? 0 : 1; }
        else if (a == "--upgradetest") { g_map.generate(); return upgradeTest(seed) ? 0 : 1; }
        else if (a == "--helitest") { g_map.generate(); return heliTest(seed) ? 0 : 1; }
        else if (a == "--fuzztest") { g_map.generate(); int secs = 400; if (i + 1 < argc && argv[i + 1][0] != '-') secs = atoi(argv[++i]); bool ok = true; for (int k = 0; k < 6 && ok; k++) ok = fuzzTest(seed + k, secs); return ok ? 0 : 1; }
        else if (a == "--mapdump") {   // ASCII tile map: . grass, : dirt, s sand, = road, B bridge, ~ water, ^ rock, T tree
            g_map.generate();
            // (structures: # a block of a town round the lake, L another landmark, $ a supply pile)
            for (int y = 0; y < MAP_H; y++) { for (int x = 0; x < MAP_W; x++) { int t = g_map.tile(x, y); u8 b = g_map.blocked[y * MAP_W + x]; char c = g_map.bridge[y * MAP_W + x] ? 'B' : ".. :s=~^T"[t == T_GRASS2 ? 0 : (t == T_GRASS ? 0 : (t == T_DIRT ? 3 : (t == T_SAND ? 4 : (t == T_ROAD ? 5 : (t == T_WATER ? 6 : (t == T_ROCK ? 7 : 8))))))];
                if (b & 4) c = '$'; else if (b & 2) { c = 'L'; for (auto& D : g_map.districts) for (auto& k : D.blocks) { const BuildType& bt = BUILDS[B_C_HOUSE + k.kind]; if (x >= k.tx && x < k.tx + bt.w && y >= k.ty && y < k.ty + bt.h) c = '#'; } }
                putchar(c); } putchar('\n'); }
            for (auto& D : g_map.districts) fprintf(stderr, "district %s (%s) centre %.1f,%.1f blocks %d\n", D.name, D.city ? "city" : "town", D.cx, D.cy, (int)D.blocks.size());
            return 0;
        }
        else if (a == "--rulestest") { g_map.generate(); return rulesTest(seed) ? 0 : 1; }
        else if (a == "--stealthtest") return stealthTest(seed) ? 0 : 1;
        else if (a == "--navytest") return navyTest(seed) ? 0 : 1;
        else if (a == "--jeeptest") return jeepTest(seed) ? 0 : 1;
        else if (a == "--dronetest") return droneTest(seed) ? 0 : 1;
        else if (a == "--strikejettest") return strikeJetTest(seed) ? 0 : 1;
        else if (a == "--antinuketest") return antiNukeTest(seed) ? 0 : 1;
        else if (a == "--dozertest") return dozerTest(seed) ? 0 : 1;
        else if (a == "--recoverytest") return recoveryTest(seed) ? 0 : 1;
        else if (a == "--civictest") return civicTest(seed) ? 0 : 1;
        else if (a == "--walltest") return wallTestMain(seed) ? 0 : 1;
        else if (a == "--supersonictest") { g_map.generate(); return supersonicTest(seed) ? 0 : 1; }
        else if (a == "--radtest") { g_map.generate(); return radiationTest(seed) ? 0 : 1; }
        else if (a == "--forttest") { g_map.generate(); return fortTest(seed) ? 0 : 1; }
        else if (a == "--updatetest") { g_map.generate(); return updateTest(seed) ? 0 : 1; }
        else if (a == "--lakeupdatetest") { g_map.generate(); return lakeUpdateTest(seed) ? 0 : 1; }
        else if (a == "--superupdatetest") { g_map.generate(); return superUpdateTest(seed) ? 0 : 1; }
        else if (a == "--civiltest") { g_map.generate(); return civilTest(seed) ? 0 : 1; }
        else if (a == "--oceantest") { g_map.generate(); return oceanTest(seed) ? 0 : 1; }
        else if (a == "--fleettest") { g_map.generate(); return fleetTest(seed) ? 0 : 1; }
        else if (a == "--bunkertest") { g_map.generate(); return bunkerUpdateTest(seed) ? 0 : 1; }
        else if (a == "--duelmatrix") { g_map.generate(); return duelMatrix(seed) ? 0 : 1; }
        else if (a == "--micromatrix") { g_map.generate(); return microMatrix(seed) ? 0 : 1; }
        else if (a == "--learnupdatetest") { g_map.generate(); return learnUpdateTest(seed) ? 0 : 1; }
        else if (a == "--chosentest") { g_map.generate(); return chosenTest(seed) ? 0 : 1; }
        else if (a == "--variety") { int n = 20; if (i + 1 < argc && argv[i + 1][0] != '-') n = atoi(argv[++i]); g_map.generate(); return varietyTest(n, seed, d0 >= 0 && d0 <= 3 ? d0 : 2); }
        else if (a == "--movetest") return moveTest(seed) ? 0 : 1;
        else if (a == "--jettest") { g_map.generate(); return jetTest(seed) ? 0 : 1; }
        else if (a == "--nuketest") { g_map.generate(); return nukeTest(seed) ? 0 : 1; }
        else if (a == "--autotest") { g_map.generate(); return autoTest(seed) ? 0 : 1; }
        else if (a == "--bombtest") { g_map.generate(); return bombTest(seed) && nukeDodgeTest(seed) ? 0 : 1; }
        else if (a == "--braintest") return brainTest() ? 0 : 1;
        else if (a == "--crowdtest") return crowdTest(seed) ? 0 : 1;
        else if (a == "--frontiermaptest") return frontierMapTest(seed) ? 0 : 1;
        else if (a == "--learningtest") return learningTest(seed) ? 0 : 1;
        else if (a == "--determinismtest") { int secs = 300; if (i + 1 < argc && argv[i + 1][0] != '-') secs = clampi(atoi(argv[++i]), 1, 3600); return determinismTest(seed, secs) ? 0 : 1; }
        else if (a == "--placementtest") return placementTest(seed) ? 0 : 1;
        else if (a == "--placementbench") return placementBench(seed) ? 0 : 1;
        else if (a == "--areatest") { g_map.generate(); return areaTest(seed) ? 0 : 1; }
        else if (a == "--view" && i + 1 < argc) viewPlayer = clampi(atoi(argv[++i]), 0, 9);
        else if (a == "--d0" && i + 1 < argc) d0 = clampi(atoi(argv[++i]), 0, 3);
        else if (a == "--faction" && i + 1 < argc) faction = argv[++i][0] == 'k' ? F_CLANKER : F_CYBER;
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); usage(); return 1; }
    }
    g_map.generate();
    if (trainN > 0) return trainBrain(trainN, seed);
    if (evalDiffN > 0) return evalDiff(evalDiffN, seed);
    if (evalAiN > 0) return evalAi(evalAiN, seed, d0 >= 0 && d0 <= 3 ? d0 : 2);
    if (evalN > 0) return evalBrain(evalN, seed, d0 >= 0 && d0 <= 3 ? std::max(1, std::min(d0, 3)) : 2);
    if (selftestSecs >= 0) return selfTest(selftestSecs, seed, players, d0, swap) ? 0 : 1;

    if (headless) SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    // headless runs (screenshots, ui test) keep the classic 1024x640 unless --size is given
    if (headless && reqW <= 0) { reqW = MIN_SCREEN_W; reqH = MIN_SCREEN_H; }
    if (!g_gfx.init(scale, software, reqW, reqH)) return 1;
    if (!headless && g_gfx.softwareRenderer) g_game.detail = 2;   // no GPU: start a step down (it climbs back if the machine keeps up)
    if (!headless) {
        g_audio.init();
        g_brainPath = Brain::defaultPath();
        if (!g_brainPath.empty()) g_brain.load(g_brainPath.c_str());   // the AI keeps what it learned in earlier games
    }

    g_game.seed = seed;
    g_game.menu.playerFaction = faction;
    if (getenv("ONEHOUR_MENUDEMO")) {   // screenshots of the setup table: an ally and two enemies with different difficulties
        MenuSettings& m = g_game.menu;
        m.allyFaction = 1; m.allyDiff = 2; m.enemyFaction[0] = 0; m.enemyDiff[0] = 3; m.enemyFaction[1] = 2; m.enemyDiff[1] = 1; m.enemyFaction[2] = 3;
        m.cursor = atoi(getenv("ONEHOUR_MENUDEMO")) ; m.col = m.cursor >= 1 && m.cursor <= 3 ? 1 : 0;
    }
    if (sheetPath) {
        Gfx& g = g_gfx;
        g.beginFrame(rgb(74, 96, 58));
        if (const char* bl = getenv("ONEHOUR_BLD")) {   // every structure of one faction at 1x (ONEHOUR_BLD=0 cyber, 1 clanker); ONEHOUR_BLDSCALE zooms
            int fac = atoi(bl) & 1; float sc = getenv("ONEHOUR_BLDSCALE") ? (float)atof(getenv("ONEHOUR_BLDSCALE")) : 1.0f;
            int first = fac == 0 ? B_C_HQ : B_K_HQ, only = getenv("ONEHOUR_BLDONLY") ? atoi(getenv("ONEHOUR_BLDONLY")) : -1;
            float x = 10, y = 6, rowH = 0;
            std::vector<int> types;   // the faction's base structures, then its underground bunker, Naval Yard and anti-nuke defence (their ids sit after both blocks);
            if (getenv("ONEHOUR_FORTS")) {
                types = { B_C_CASTLE, B_C_CIVBUNKER, B_C_WATCHTOWER, B_C_COTTAGE, B_C_WORKSHOP, B_C_FORTRESS };
            } else if (getenv("ONEHOUR_CIVIC")) {   // ONEHOUR_CIVIC=1: the civilian town and the Underground Nuke silo instead
                for (int k = 0; k < CV_COUNT; k++) types.push_back((fac == 0 ? (int)B_C_HOUSE : (int)B_K_HOUSE) + k);
                types.push_back(fac == 0 ? (int)B_C_UNUKE : (int)B_K_UNUKE);
            } else {
                for (int i = 0; i < BUILDS_PER_FACTION; i++) types.push_back(first + i);
                types.push_back(fac == 0 ? (int)B_C_BUNKER : (int)B_K_BUNKER); types.push_back(fac == 0 ? (int)B_C_NAVAL : (int)B_K_NAVAL); types.push_back(fac == 0 ? (int)B_C_ANTINUKE : (int)B_K_ANTINUKE);
            }
            for (int i = 0; i < (int)types.size(); i++) {
                if (only >= 0 && i != only) continue;
                int t = types[i]; const BuildType& bt = BUILDS[t];
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
        if (getenv("ONEHOUR_PATROL_SPRITES")) {   // the Patrol Boat art: Cyber then Clanker, each in the first two team colours, at 4x with its mount; a 1x strip of the patrol boats beside the destroyers at in-game scale
            const float PATROL_ART_SCALE = 1.15f;   // the patrol boat's artScale: in game it is drawn at UNIT_SCALE * PATROL_ART_SCALE
            Sprite pBody[2][2], pMount[2][2];        // [0 Cyber, 1 Clanker][team slot 0 or 1]; the sprites keep dscale 1 (the 4x row)
            for (int f = 0; f < 2; f++) for (int p = 0; p < 2; p++) {
                Canvas bc(64, 64), mc(64, 64);
                artPatrolBoatBody(bc, PLAYER_COLOR[p], f == 0);
                artPatrolBoatMount(mc, PLAYER_COLOR[p], f == 0);
                pBody[f][p] = g.fromCanvasSmooth(bc, 32, 32);
                pMount[f][p] = g.fromCanvasSmooth(mc, 32, 32);
            }
            ShipMounts pm = patrolMounts();
            for (int f = 0; f < 2; f++) for (int p = 0; p < 2; p++) {   // 4x: Cyber on the top row, Clanker below; the mount at its offset * 4, turned 0.4 rad
                float x = 260 + p * 500, y = 170 + f * 220;
                g.draw(pBody[f][p], x, y, 0, 4);
                for (int i = 0; i < pm.n; i++) g.draw(pMount[f][p], x + pm.x[i] * 4, y + pm.y[i] * 4, 0.4f, 4);
            }
            for (int f = 0; f < 2; f++) for (int p = 0; p < 2; p++) {   // 1x: the patrol boats at in-game scale, UNIT_SCALE * PATROL_ART_SCALE
                float x = 130 + (f * 2 + p) * 100, y = 560, sc = UNIT_SCALE * PATROL_ART_SCALE;
                Sprite b = pBody[f][p], m = pMount[f][p];
                b.dscale = sc; m.dscale = sc;
                g.draw(b, x, y, 0, 1);
                for (int i = 0; i < pm.n; i++) g.draw(m, x + pm.x[i] * sc, y + pm.y[i] * sc, 0.4f, 1);
            }
            const int dest[2] = { U_C_DESTROYER, U_K_DESTROYER };   // the destroyers at 1x, their in-game scale, for comparison
            for (int f = 0; f < 2; f++) {
                int t = dest[f]; float x = 640 + f * 180, y = 560;
                g.draw(g.unitBody[t][0], x, y, 0, 1);
                ShipMounts dm = shipMounts(false);
                for (int i = 0; i < dm.n; i++) g.draw(g.unitTurret[t][0], x + dm.x[i] * UNIT_SCALE * artScale(t), y + dm.y[i] * UNIT_SCALE * artScale(t), 0.4f, 1);
            }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_NEWUNITS")) {   // jeeps, warships, strike fighters and protector drones, Cyber row then Clanker row, 4x; the jeep seats and ship mounts are marked; a 1x strip below
            const int ids[2][4] = { { U_C_JEEP, U_C_DESTROYER, U_C_CRUISER, U_C_STRIKEJET }, { U_K_JEEP, U_K_DESTROYER, U_K_CRUISER, U_K_STRIKEJET } };
            const float colX[4] = { 120, 360, 610, 880 }, rowY[2] = { 150, 400 };
            for (int row = 0; row < 2; row++) for (int col = 0; col < 4; col++) {
                int t = ids[row][col]; float x = colX[col], y = rowY[row], sc = 4.0f;
                const UnitType& ut = UNITS[t];
                float k = ut.naval ? 1.0f : 1.0f;
                g.draw(g.unitBody[t][row], x, y, 0, sc * k);
                if (ut.naval) {
                    ShipMounts m = shipMounts(t == U_C_CRUISER || t == U_K_CRUISER);
                    for (int i = 0; i < m.n; i++) g.draw(g.unitTurret[t][row], x + m.x[i] * UNIT_SCALE * artScale(t) * sc, y + m.y[i] * UNIT_SCALE * artScale(t) * sc, 0.4f, sc);
                }
                if (t == U_C_JEEP || t == U_K_JEEP) {
                    for (int i = 0; i < 7; i++) g.fillCircle(x + (JEEP_SEAT_X[i] - 32) * UNIT_SCALE * sc, y + (JEEP_SEAT_Y[i] - 32) * UNIT_SCALE * sc, 2.6f * UNIT_SCALE * sc, i < 2 ? rgb(255, 200, 80) : rgb(255, 255, 255, 150));
                    g.draw(g.unitAnim[t][row][0], x - 60, y + 78, 0, 2.0f); g.draw(g.unitAnim[t][row][1], x + 40, y + 78, 0, 2.0f);   // the two wheel-cycle frames, small, below
                }
            }
            for (int row = 0; row < 2; row++) for (int p = 0; p < 4; p++) g.draw(g.guardDrone[row][p], 40 + (row * 4 + p) * 40, 560, 0, 3.0f);
            for (int i = 0; i < 8; i++) {
                int t = ids[i / 4][i % 4]; float x = 460 + i * 66, y = 600;
                g.draw(g.unitBody[t][i / 4], x, y, 0, 1);
                if (UNITS[t].naval) { ShipMounts m = shipMounts(t == U_C_CRUISER || t == U_K_CRUISER); for (int k = 0; k < m.n; k++) g.draw(g.unitTurret[t][i / 4], x + m.x[k] * UNIT_SCALE * artScale(t), y + m.y[k] * UNIT_SCALE * artScale(t), 0.4f, 1); }
            }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_CHOSEN_SPRITES")) {   // the Chosen and Shadow Guard art: Cyber and Clanker in team colours 0 and 1, at 4.5x (row 1 the Chosen, row 2 the Shadow Guard); a 1x strip against the laser commando and a trooper at in-game scale; walk frames 0/1/2 at 2x
            const float CHOSEN_ART_SCALE = 1.12f, SHADOW_ART_SCALE = 1.0f;   // their artScale once wired: the Chosen the commando's footprint, the Shadow Guard a trooper's
            Sprite ch[2][2][3], sh[2][2][3], cmd[2];   // [0 Cyber, 1 Clanker][team slot][walk frame]; the sprites carry their in-game dscale
            for (int f = 0; f < 2; f++) for (int p = 0; p < 2; p++) for (int fr = 0; fr < 3; fr++) {
                Canvas cc(64, 64), sc(64, 64);
                artChosenBody(cc, PLAYER_COLOR[p], f == 0, fr);
                artShadowBody(sc, PLAYER_COLOR[p], f == 0, fr);
                ch[f][p][fr] = g.fromCanvasSmooth(cc, 32, 32); ch[f][p][fr].dscale = UNIT_SCALE * CHOSEN_ART_SCALE;
                sh[f][p][fr] = g.fromCanvasSmooth(sc, 32, 32); sh[f][p][fr].dscale = UNIT_SCALE * SHADOW_ART_SCALE;
            }
            for (int f = 0; f < 2; f++) {
                Canvas kc(64, 64); artCommandoBody(kc, PLAYER_COLOR[0], f == 0, 0);
                cmd[f] = g.fromCanvasSmooth(kc, 32, 32); cmd[f].dscale = UNIT_SCALE * 1.12f;   // the laser commando's artScale
            }
            for (int f = 0; f < 2; f++) for (int p = 0; p < 2; p++) {   // rows 1 and 2 at 4.5x: Cyber then Clanker, team colour 0 then 1
                float x = 150 + (f * 2 + p) * 240;
                g.draw(ch[f][p][0], x, 110, 0, 4.5f);
                g.draw(sh[f][p][0], x, 322, 0, 4.5f);
            }
            for (int f = 0; f < 2; f++) {   // row 3 at 1x (in-game size): Cyber on the left, Clanker on the right; Chosen, Shadow Guard, laser commando, trooper
                float x0 = 80 + f * 500, y = 470;
                g.draw(ch[f][0][0], x0, y, 0, 1.0f);
                g.draw(sh[f][0][0], x0 + 110, y, 0, 1.0f);
                g.draw(cmd[f], x0 + 220, y, 0, 1.0f);
                g.draw(g.unitBody[f == 0 ? U_C_INF1 : U_K_INF1][0], x0 + 330, y, 0, 1.0f);
            }
            for (int u = 0; u < 4; u++) for (int fr = 0; fr < 3; fr++) {   // row 4: walk frames 0/1/2 at 2x (team 0): Chosen Cyber, Chosen Clanker, Shadow Cyber, Shadow Clanker
                const Sprite& s = u == 0 ? ch[0][0][fr] : u == 1 ? ch[1][0][fr] : u == 2 ? sh[0][0][fr] : sh[1][0][fr];
                g.draw(s, 50 + (u * 3 + fr) * 83, 590, 0, 2.0f);
            }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_RADTRUCK_SPRITES")) {   // the Anti-Radiation Truck art: Cyber and Clanker at 4.5x in all four team colours, a 1x strip below
            const int ids[2] = { U_C_RADTRUCK, U_K_RADTRUCK };
            for (int f = 0; f < 2; f++) for (int p = 0; p < 4; p++) {
                float x = 135 + p * 235, y = 125 + f * 285;
                g.draw(g.unitBody[ids[f]][p], x, y, 0, 4.5f);
            }
            for (int i = 0; i < 8; i++) { int t = ids[i / 4]; float x = 100 + i * 110, y = 610; g.draw(g.unitBody[t][i % 4], x, y, 0, 1.4f); }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_SBOMBER_SPRITES")) {   // the Supersonic Bomber art: Cyber and Clanker at 4.5x in all four team colours, a 1x strip below
            const int ids[2] = { U_C_SBOMBER, U_K_SBOMBER };
            for (int f = 0; f < 2; f++) for (int p = 0; p < 4; p++) {
                float x = 135 + p * 235, y = 125 + f * 285;
                g.draw(g.unitBody[ids[f]][p], x, y, 0, 4.5f);
            }
            for (int i = 0; i < 8; i++) { int t = ids[i / 4]; float x = 100 + i * 110, y = 610; g.draw(g.unitBody[t][i % 4], x, y, 0, 1.4f); }
            g_game.screenshot(sheetPath);
            printf("wrote %s\n", sheetPath);
            g_gfx.shutdown(); SDL_Quit(); return 0;
        }
        if (getenv("ONEHOUR_NEW")) {   // the new designs at 5x: spy, spy drone and cargo lifter, Cyber row then Clanker row; a 1x strip of all six below
            const int ids[2][3] = { { U_C_SPY, U_C_SDRONE, U_C_CARGO }, { U_K_SPY, U_K_SDRONE, U_K_CARGO } };
            const float sc = 5.0f, colX[3] = { 150, 400, 730 }, rowY[2] = { 165, 455 };
            for (int row = 0; row < 2; row++) for (int col = 0; col < 3; col++) {
                int t = ids[row][col]; float x = colX[col], y = rowY[row];
                g.draw(g.unitBody[t][0], x, y, 0, sc);
                if (t == U_C_SDRONE || t == U_K_SDRONE) { g.draw(g.rotorDisc, x, y, 0, 0.6f * sc, rgb(255, 255, 255), 150); g.draw(g.rotorBlades, x, y, 0.5f, 0.6f * sc, rgb(200, 240, 255), 110); }
                if (t == U_C_CARGO || t == U_K_CARGO) for (int k = -1; k <= 1; k += 2) {
                    g.draw(g.rotorDisc, x + k * 20.8f * sc, y, 0, 1.55f * sc, rgb(255, 255, 255), 235);
                    g.draw(g.rotorBlades, x + k * 20.8f * sc, y, 0.5f * k, 1.55f * sc, row == 0 ? rgb(200, 240, 255) : rgb(255, 255, 255), 150);
                }
            }
            for (int i = 0; i < 6; i++) {   // the 1x strip: Cyber then Clanker, same three designs
                int t = ids[i / 3][i % 3]; float x = 40 + i * 75, y = 600;
                g.draw(g.unitBody[t][0], x, y, 0, 1);
                if (t == U_C_SDRONE || t == U_K_SDRONE) { g.draw(g.rotorDisc, x, y, 0, 0.6f, rgb(255, 255, 255), 150); g.draw(g.rotorBlades, x, y, 0.5f, 0.6f, rgb(200, 240, 255), 110); }
                if (t == U_C_CARGO || t == U_K_CARGO) for (int k = -1; k <= 1; k += 2) {
                    g.draw(g.rotorDisc, x + k * 20.8f, y, 0, 1.55f, rgb(255, 255, 255), 235);
                    g.draw(g.rotorBlades, x + k * 20.8f, y, 0.5f * k, 1.55f, i < 3 ? rgb(200, 240, 255) : rgb(255, 255, 255), 150);
                }
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
        for (int m = 0; m < 2; m++) { int t = m == 0 ? U_C_SNIPER : U_K_SNIPER; g.draw(g.unitBody[t][0], 860 + m * 110, 150, 0, 3); }   // the snipers
        for (int m = 0; m < 2; m++) { int t = m == 0 ? U_C_MEDIC : U_K_MEDIC; g.draw(g.shadowLarge, 860 + m * 110, 306, 0, 1.6f); g.draw(g.unitBody[t][0], 860 + m * 110, 300, 0, 3); }   // the medics sit after both unit blocks
        for (int i = 0; i < 4; i++) g.draw(g.turretHead[i], 800 + i * 50, 480, 0, 1.2f);
        for (int p = 0; p < MAX_PLAYERS; p++) g.drawFlag(820 + p * 50, 590, p, 1.4f, 1.0f);
        g_game.screenshot(sheetPath);
        printf("wrote %s\n", sheetPath);
        g_gfx.shutdown(); SDL_Quit(); return 0;
    }
    if (ticks == -1) { bool ok = uiTest(); g_gfx.shutdown(); SDL_Quit(); return ok ? 0 : 1; }
    if (lateMin > 0) {
        // a whole match as a player would see it: four armies (the viewed one under the computer too), one rendered frame per game second (the
        // camera alternates between the viewed base and the latest fighting); per game minute the cost of a tick and of a frame, and what has piled up
        g_game.menu.enemyFaction[0] = 1; g_game.menu.enemyFaction[1] = 1; g_game.menu.enemyFaction[2] = 0; g_game.menu.enemiesAllied = false;   // everyone against everyone: the longest matches
        g_game.menu.enemyDiff[0] = 3; g_game.menu.enemyDiff[1] = 2; g_game.menu.enemyDiff[2] = 2;
        g_game.startGame(); g_sim.players[0].isAI = true; g_sim.players[0].difficulty = 3; g_ai.init(seed);
        const double f0 = (double)SDL_GetPerformanceFrequency();
        double simT = 0, simW = 0, renT = 0, renW = 0, aiT = 0, aiW = 0; int nT = 0, nR = 0;
        const int lateFrom = getenv("ONEHOUR_LATEFROM") ? atoi(getenv("ONEHOUR_LATEFROM")) : 0, lateFps = getenv("ONEHOUR_LATEFPS") ? atoi(getenv("ONEHOUR_LATEFPS")) : 3;   // (profiling: render only from that minute, that many frames a second)
        auto rssMb = []() { long pages = 0, rss = 0; FILE* f = fopen("/proc/self/statm", "r"); if (f) { if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = 0; fclose(f); } return rss * 4096.0 / 1048576.0; };
        for (int t = 1; t <= lateMin * 60 * SIM_HZ; t++) {
            u64 s0 = SDL_GetPerformanceCounter();
            g_sim.step();
            u64 s1 = SDL_GetPerformanceCounter();
            g_ai.update();
            double sms = (SDL_GetPerformanceCounter() - s0) * 1000.0 / f0, ams = (SDL_GetPerformanceCounter() - s1) * 1000.0 / f0; simT += sms; simW = std::max(simW, sms); nT++;
            aiT += ams; aiW = std::max(aiW, ams);
            if (getenv("ONEHOUR_BENCHDBG") && sms > 25) printf("  t=%.1fs tick %.1f ms (AI %.1f ms) nukes %zu fx %zu projs %zu\n", g_sim.time, sms, ams, g_sim.nukes.size(), g_sim.fx.size(), g_sim.projs.size());
            if (t % SIM_HZ == 0 && t >= lateFrom * 60 * SIM_HZ) {
                Vec2 b = g_sim.players[0].basePos;
                if ((t / SIM_HZ) % 2) { float best = -1; for (auto& e : g_sim.ents) if (e.alive && e.lastDamaged > best) { best = e.lastDamaged; b = e.pos; } }
                g_game.cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int k = 0; k < lateFps; k++) {   // three frames a second by default: enough for the particles and the fog to follow the camera
                    u64 r0 = SDL_GetPerformanceCounter();
                    g_game.renderAlpha = 1; g_game.wallTime += 1.0f / 60; g_game.frameDt = 1.0f / 60;
                    g_game.update(1.0f / 60);
                    u64 r1 = SDL_GetPerformanceCounter();
                    g_gfx.fillPx = 0;
                    g_game.render();
                    u64 r2 = SDL_GetPerformanceCounter();
                    g_gfx.present();
                    double rms = (SDL_GetPerformanceCounter() - r0) * 1000.0 / f0; renT += rms; renW = std::max(renW, rms); nR++;
                    if (!getenv("ONEHOUR_FULLDETAIL")) g_game.noteFrameCost((float)((SDL_GetPerformanceCounter() - r1) * 1000.0 / f0));   // (as the game does: effects detail follows the frame cost)
                    if (getenv("ONEHOUR_BENCHDBG") && rms > 150) printf("  t=%.0fs frame %.0f ms: update %.0f render %.0f present %.0f fill %.1f Mpx | parts %zu fx %zu nukes %zu fallouts %zu decals %zu cam %.0f,%.0f\n", g_sim.time, rms,
                        (r1 - r0) * 1000.0 / f0, (r2 - r1) * 1000.0 / f0, (SDL_GetPerformanceCounter() - r2) * 1000.0 / f0, g_gfx.fillPx / 1e6, g_game.parts.size(), g_sim.fx.size(), g_sim.nukes.size(), g_sim.fallouts.size(), g_game.decals.size(), g_game.cam.x, g_game.cam.y);
                }
            }
            g_sim.events.clear();
            if (t % (60 * SIM_HZ) == 0) {
                int alive = 0, units = 0; for (auto& e : g_sim.ents) if (e.alive) { alive++; if (e.isUnit()) units++; }
                char alive4[8] = "----"; for (int p = 0; p < std::min(4, g_sim.numPlayers); p++) alive4[p] = g_sim.players[p].alive ? '0' + p : 'x';
                printf("min %3d %s | tick avg %.2f worst %.1f ms (AI avg %.2f worst %.1f) | frame avg %.2f worst %.1f ms | ents %zu alive %d units %d | fx %zu projs %zu parts %zu decals %zu treads %zu | rss %.0f MB%s\n",
                       t / (60 * SIM_HZ), alive4, simT / std::max(1, nT), simW, aiT / std::max(1, nT), aiW, renT / std::max(1, nR), renW, g_sim.ents.size(), alive, units, g_sim.fx.size(), g_sim.projs.size(),
                       g_game.parts.size(), g_game.decals.size(), g_game.treads.size(), rssMb(), g_sim.gameOver ? " (over)" : "");
                fflush(stdout);
                simT = simW = renT = renW = aiT = aiW = 0; nT = nR = 0;
                if (g_sim.gameOver) break;
            }
        }
        g_gfx.shutdown(); SDL_Quit(); return 0;
    }
    if (benchFrames > 0) {
        // a busy scene: four AI armies eight minutes in, the camera sweeping across the battle, every frame fully rendered (software renderer: no GPU help)
        const bool onIsland = !g_gfx.softwareRenderer || getenv("ONEHOUR_BENCHISLAND");   // (--gpubench: four armies, everyone against everyone, the camera on the fighting)
        if (onIsland) {
            g_game.menu.enemyFaction[0] = 1; g_game.menu.enemyFaction[1] = 1; g_game.menu.enemyFaction[2] = 0; g_game.menu.enemiesAllied = false;
            g_game.menu.enemyDiff[0] = 3; g_game.menu.enemyDiff[1] = 2; g_game.menu.enemyDiff[2] = 2;
        }
        g_game.startGame(); g_sim.players[0].isAI = true; g_ai.init(seed);
        int crowd = getenv("ONEHOUR_BENCHCROWD") ? atoi(getenv("ONEHOUR_BENCHCROWD")) : 0;   // a massed clash instead: N infantry and tanks a side, on screen
        Vec2 crowdAt;
        if (crowd > 0) {
            for (int t = 0; t < 200; t++) { g_sim.step(); g_sim.events.clear(); }
            crowdAt = Vec2(WORLD_W * 0.5f, std::round(12.0f * MAP_W / 80.0f) * TILE);   // the open ground north of the lake, between two bases: the middle of the northern pass (22 tiles down on the 146 map) on any map size
            for (int side = 0; side < 2; side++) {
                int p = side, base = firstUnitOf(g_sim.players[p].faction);
                std::vector<Ref> army;
                for (int i = 0; i < crowd; i++) {
                    int type = base + (i % 5 == 4 ? 5 : (i % 5 == 3 ? 6 : 2 + i % 3));   // three infantry kinds, a tank and a light vehicle in every five
                    Vec2 at = crowdAt + Vec2((side ? 1.0f : -1.0f) * (9.0f * TILE + (i / 20) * 26.0f), ((i % 20) - 9.5f) * 22.0f);
                    army.push_back(g_sim.spawnUnit(type, p, g_map.nearestFree(at, 8)));
                }
                g_sim.cmdMove(army, crowdAt + Vec2(side ? -6.0f * TILE : 6.0f * TILE, 0), true);
            }
        } else for (int t = 0, warm = getenv("ONEHOUR_BENCHMIN") ? atoi(getenv("ONEHOUR_BENCHMIN")) * 60 * SIM_HZ : 9600; t < warm; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
        Vec2 fight = g_sim.players[0].basePos, camAt = fight;   // (--gpubench: the camera glides to the latest fighting, picked again every 2 s)
        u64 f0 = SDL_GetPerformanceFrequency(); double worst = 0, total = 0, updTotal = 0, updWorst = 0;
        double simTotal = 0, simWorst = 0;
        for (int fr = 0; fr < benchFrames; fr++) {
            u64 s0 = SDL_GetPerformanceCounter();
            g_sim.step(); g_ai.update(); g_sim.events.clear();
            double sms = (SDL_GetPerformanceCounter() - s0) * 1000.0 / SDL_GetPerformanceFrequency(); simTotal += sms; simWorst = std::max(simWorst, sms);
            if (getenv("ONEHOUR_BENCHDBG") && sms > 12) printf("  tick %d: %.1f ms\n", fr, sms);
            if (crowd > 0 && fr % 60 == 30) {   // the armies are re-ordered across the field now and then: a mass move order plans every path in one tick
                for (int side = 0; side < 2; side++) {
                    std::vector<Ref> army;
                    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == side && !e.isAir()) army.push_back(g_sim.refOf(e));
                    u64 m0 = SDL_GetPerformanceCounter();
                    const float k = MAP_W / 146.0f;   // (the same places on any map size: the arena's features scale with it)
                    g_sim.cmdMove(army, crowdAt + Vec2((side ? -1.0f : 1.0f) * std::round(30.0f * k) * TILE, ((fr / 60) % 2 ? std::round(14.0f * k) : -std::round(10.0f * k)) * TILE), true);
                    double mms = (SDL_GetPerformanceCounter() - m0) * 1000.0 / SDL_GetPerformanceFrequency(); simTotal += mms; simWorst = std::max(simWorst, mms);
                    if (getenv("ONEHOUR_BENCHDBG")) printf("  mass move of %zu units: %.1f ms\n", army.size(), mms);
                }
            }
            float u = fr / (float)benchFrames;
            g_game.cam = Vec2(clampf(200 + u * 1500, 0, WORLD_W - SCREEN_W), clampf(300 + std::sin(u * 6.0f) * 700 + 700, 0, WORLD_H - VIEW_H));
            if (crowd > 0) g_game.cam = Vec2(clampf(crowdAt.x - SCREEN_W * 0.5f + std::sin(u * 3.0f) * 300, 0, WORLD_W - SCREEN_W), clampf(crowdAt.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H));
            else if (onIsland) {
                if (fr % 40 == 0) { float best = -1; for (auto& e : g_sim.ents) if (e.alive && e.lastDamaged > best) { best = e.lastDamaged; fight = e.pos; } }
                camAt = camAt + (fight - camAt) * 0.08f;
                g_game.cam = Vec2(clampf(camAt.x - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W), clampf(camAt.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H));
            }
            g_game.renderAlpha = 1; g_game.wallTime += 1.0f / 60; g_game.frameDt = 1.0f / 60;
            u64 t0 = SDL_GetPerformanceCounter();
            g_game.update(1.0f / 60);
            u64 t1 = SDL_GetPerformanceCounter();
            g_game.render();
            if (fr == benchFrames - 1 && getenv("ONEHOUR_GPUSHOT")) g_game.screenshot(getenv("ONEHOUR_GPUSHOT"));   // (the last frame as the GPU path drew it)
            g_gfx.present();   // SDL queues draw calls until present: the frame only costs what it costs once flushed
            double ms = (SDL_GetPerformanceCounter() - t0) * 1000.0 / f0, ums = (t1 - t0) * 1000.0 / f0;
            total += ms; worst = std::max(worst, ms); updTotal += ums; updWorst = std::max(updWorst, ums);
            if (getenv("ONEHOUR_BENCHDBG") && ms > 30) printf("  frame %d: %.1f ms (cam %.0f,%.0f, particles %zu, fx %zu, nukes %zu, fallouts %zu)\n", fr, ms, g_game.cam.x, g_game.cam.y, g_game.parts.size(), g_sim.fx.size(), g_sim.nukes.size(), g_sim.fallouts.size());
        }
        printf("bench: %d frames, avg %.2f ms, worst %.2f ms (software renderer; of which update/sim avg %.2f ms, worst %.2f ms), particles %zu, fx %zu, entities %zu\n", benchFrames, total / benchFrames, worst, updTotal / benchFrames, updWorst, g_game.parts.size(), g_sim.fx.size(), g_sim.ents.size());
        printf("bench: simulation tick + AI avg %.2f ms, worst %.2f ms (mass move orders included)\n", simTotal / benchFrames, simWorst);
        { long pages = 0, rss = 0; FILE* f = fopen("/proc/self/statm", "r"); if (f) { if (fscanf(f, "%ld %ld", &pages, &rss) != 2) rss = 0; fclose(f); } printf("bench: resident memory now %.0f MB\n", rss * 4096.0 / 1048576.0); }
        g_gfx.shutdown(); SDL_Quit(); return 0;
    }
    if (shot) {
        if (ticks > 0) {
            g_game.startGame();
            if (allAi) { g_sim.players[0].isAI = true; g_ai.init(seed); }
            const char* upgShow = getenv("ONEHOUR_UPG");   // ONEHOUR_UPG=1: every army starts with all structure upgrades; =raid: enemy aircraft and tanks then hit the viewed base
            if (upgShow) for (int p = 0; p < g_sim.numPlayers; p++) for (int u = 0; u < UPG_COUNT; u++) g_sim.players[p].upg[u] = true;
            for (int t = 0; t < ticks; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
            Vec2 b = g_sim.players[std::min(viewPlayer, 3)].basePos;
            Vec2 stealthCam; bool haveStealthCam = false;
            if (getenv("ONEHOUR_FORTS")) {   // showcase: the three neutral civilian strongholds on the north-west of the map
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                Vec2 focus; int n = 0;
                for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_STRONGHOLD && e.type != B_C_WATCHTOWER) { focus += e.pos; n++; if (n >= 2) break; }
                if (n == 0) for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_STRONGHOLD) { focus += e.pos; n++; if (n >= 3) break; }
                if (n > 0) { focus = focus * (1.0f / n); g_game.cam = Vec2(clampf(focus.x - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W), clampf(focus.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H)); }
                for (int p = 0; p < g_sim.numPlayers; p++) for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_STRONGHOLD) { int tx = e.tx + e.bt().w + 1, ty = e.ty + e.bt().h + 1; (void)tx; (void)ty; }
            }
            if (getenv("ONEHOUR_STEALTH")) {   // ONEHOUR_STEALTH=1: an underground bunker whose garrison fires at an enemy tank, a loaded cargo lifter, a spy drone and a spy at work, near the viewed base
                int vp = std::min(viewPlayer, g_sim.numPlayers - 1), foe = vp == 0 ? 1 : 0; Faction f = g_sim.players[vp].faction, ff = g_sim.players[foe].faction;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto spot = [&](Vec2 at) { return g_map.nearestFree(at, 12); };
                Vec2 c = spot(b + Vec2(0, 330));
                int tx = tileOf(c.x) - 1, ty = tileOf(c.y) - 1;
                for (int r = 0; r < 12 && !g_sim.canPlace(vp, f == F_CYBER ? B_C_BUNKER : B_K_BUNKER, tx, ty); r++) { tx += (r % 2) ? 1 : -1; ty += (r % 3) ? 0 : 1; }
                Ref bk = g_sim.placeBuilding(f == F_CYBER ? B_C_BUNKER : B_K_BUNKER, vp, tx, ty, true);
                Vec2 bp = g_sim.get(bk)->pos;
                std::vector<Ref> gar;
                for (int i = 0; i < 24; i++) gar.push_back(g_sim.spawnUnit(firstUnitOf(f) + (i % 3 == 0 ? 3 : 2), vp, spot(bp + Vec2((i % 8 - 4) * 20.0f, 60.0f + (i / 8) * 20.0f))));
                g_sim.cmdEnter(gar, bk);
                for (int t = 0; t < 20 * 8; t++) { g_sim.step(); g_sim.events.clear(); }
                Ref tk = g_sim.spawnUnit(firstUnitOf(ff) + 5, foe, spot(bp + Vec2(150, 40)));
                Ref tk2 = g_sim.spawnUnit(firstUnitOf(ff) + 5, foe, spot(bp + Vec2(170, 110)));
                if (std::string(getenv("ONEHOUR_STEALTH")) == "pod") {   // ONEHOUR_STEALTH=pod: the tanks have found the bunker and go for it (its rocket pod answers)
                    g_sim.get(bk)->detUntil[foe] = g_sim.time + 1000;
                    g_sim.cmdAttack({tk, tk2}, bk);
                }
                Ref ch = g_sim.spawnUnit(f == F_CYBER ? U_C_CARGO : U_K_CARGO, vp, bp + Vec2(-230, -30)); g_sim.get(ch)->alt = 1; g_sim.get(ch)->angle = 0.4f;
                std::vector<Ref> load; for (int i = 0; i < 12; i++) load.push_back(g_sim.spawnUnit(firstUnitOf(f) + 2, vp, spot(bp + Vec2(-230 + (i % 6) * 14.0f, 20.0f + (i / 6) * 14.0f))));
                load.push_back(g_sim.spawnUnit(firstUnitOf(f) + 5, vp, spot(bp + Vec2(-300, 30))));
                g_sim.cmdEnter(load, ch);
                Ref sd = g_sim.spawnUnit(f == F_CYBER ? U_C_SDRONE : U_K_SDRONE, vp, bp + Vec2(-90, -120)); g_sim.get(sd)->alt = 1;
                g_sim.spawnUnit(f == F_CYBER ? U_C_SPY : U_K_SPY, vp, spot(bp + Vec2(-60, 80)));
                g_sim.spawnUnit(f == F_CYBER ? U_C_SNIPER : U_K_SNIPER, vp, spot(bp + Vec2(60, -90)));
                for (int t = 0; t < 20 * 6; t++) { g_sim.step(); g_sim.events.clear(); }
                stealthCam = Vec2(clampf(bp.x - SCREEN_W / 2 + 40, 0, WORLD_W - SCREEN_W), clampf(bp.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); haveStealthCam = true;
                g_game.selection.clear(); g_game.selection.push_back(bk); g_game.selection.push_back(ch);
            }
            if (const char* tw = getenv("ONEHOUR_TOWN")) {   // ONEHOUR_TOWN=1: a civilian town (every kind), an armed Underground Nuke silo and a bunker by the viewed base;
                                                              // =deep: its silver warhead on the way to the enemy; =boom: a silver detonation, ONEHOUR_TOWNBOOM ticks after impact
                int vp = std::min(viewPlayer, g_sim.numPlayers - 1), foe = vp == 0 ? 1 : 0; Faction f = g_sim.players[vp].faction;
                std::string mode = tw;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto put = [&](int type, int owner, Vec2 near) -> Ref {
                    int cx = tileOf(near.x), cy = tileOf(near.y);
                    for (int r = 0; r < 16; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++)
                        if (std::max(std::abs(dx), std::abs(dy)) == r && g_sim.canPlace(owner, type, cx + dx, cy + dy)) return g_sim.placeBuilding(type, owner, cx + dx, cy + dy, true);
                    return NOREF;
                };
                Vec2 c0 = g_map.nearestFree(b + (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - b).norm() * 420.0f, 12);
                int hb = f == F_CYBER ? B_C_HOUSE : B_K_HOUSE;
                static const float OX[CV_COUNT] = { -330, -200, -60, 80, 220, -300, -120, 60, 240 }, OY[CV_COUNT] = { -120, -120, -130, -120, -120, 40, 40, 40, 40 };
                Ref first;
                for (int k = 0; k < CV_COUNT; k++) { Ref r = put(hb + k, vp, c0 + Vec2(OX[k], OY[k])); if (!first.valid()) first = r; }
                for (int k = 0; k < 2; k++) put(firstBuildOf(f) + BR_POWER, vp, c0 + Vec2(-420.0f, 160.0f + k * 70));
                Ref silo = put(f == F_CYBER ? B_C_UNUKE : B_K_UNUKE, vp, c0 + Vec2(380, -60));
                put(f == F_CYBER ? B_C_BUNKER : B_K_BUNKER, vp, c0 + Vec2(380, 120));
                g_sim.updatePowerPublic();
                if (Entity* S = g_sim.get(silo)) S->actionTimer = g_sim.time;
                for (int k = 0; k < 2; k++) g_sim.spawnUnit(f == F_CYBER ? U_C_CIVDOZER : U_K_CIVDOZER, vp, g_map.nearestFree(c0 + Vec2(-40.0f + k * 50, 200), 8));
                for (int t = 0; t < 20 * 3; t++) { g_sim.step(); g_sim.events.clear(); }
                stealthCam = Vec2(clampf(c0.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(c0.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); haveStealthCam = true;
                if (mode == "deep" && silo.valid()) {
                    g_sim.cmdDeepNuke(vp, g_sim.players[foe].basePos);
                    for (int t = 0; t < 20 * 2; t++) { g_sim.step(); g_sim.events.clear(); }
                    if (!g_sim.nukes.empty()) { Vec2 w = Sim::nukeAirPos(g_sim.nukes[0], g_sim.nukes[0].t); stealthCam = Vec2(clampf(w.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(w.y - VIEW_H / 2 + 60, 0, WORLD_H - VIEW_H)); }
                    g_game.selection.clear(); g_game.selection.push_back(silo);
                } else if (mode == "boom") {
                    Vec2 gz = g_map.nearestFree(g_sim.players[foe].basePos + (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - g_sim.players[foe].basePos) * 0.35f, 10);
                    Sim::Nuke n; n.from = g_sim.players[vp].basePos; n.pos = gz; n.owner = vp; n.t = Sim::NUKE_FLIGHT - 0.05f; n.deep = !getenv("ONEHOUR_TOWNRAMP"); n.scale = n.deep ? UNUKE_SCALE : 1.0f;   // (ONEHOUR_TOWNRAMP=1: a ramp's warhead, to compare)
                    g_sim.nukes.push_back(n);
                    g_game.cam = Vec2(clampf(gz.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(gz.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                    int after = getenv("ONEHOUR_TOWNBOOM") ? atoi(getenv("ONEHOUR_TOWNBOOM")) : 60;
                    double ms = 0; u64 f0 = SDL_GetPerformanceFrequency();
                    for (int t = 0; t < after; t++) {   // every tick rendered (the cosmetic particles need the frames), timed like --bench
                        g_sim.step(); g_sim.events.clear();
                        u64 c0 = SDL_GetPerformanceCounter();
                        g_game.wallTime += SIM_DT; g_game.frameDt = SIM_DT; g_game.update(SIM_DT); g_game.render(); g_gfx.present();
                        ms += (SDL_GetPerformanceCounter() - c0) * 1000.0 / f0;
                    }
                    if (after > 0) printf("town boom: %d frames after the %s, avg %.2f ms per frame (software renderer)\n", after, n.deep ? "silver warhead" : "ramp's warhead", ms / after);
                    stealthCam = g_game.cam;
                } else if (first.valid()) { g_game.selection.clear(); g_game.selection.push_back(first); }
            }
            if (const char* cs = getenv("ONEHOUR_CHOSEN")) {   // ONEHOUR_CHOSEN=1: the viewed army's Chosen with its Shadow Guard at an enemy outpost; =spotted: an enemy helicopter has found it
                const bool spotted = std::string(cs) == "spotted";   // (then the Chosen is the enemy's, and one of your helicopters hovers over it)
                int vp = std::min(viewPlayer, g_sim.numPlayers - 1), foe = vp == 0 ? 1 : 0;
                if (spotted) std::swap(vp, foe);
                Faction f = g_sim.players[vp].faction, ff = g_sim.players[foe].faction;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                Vec2 c = g_map.nearestFree(b + (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - b).norm() * 520.0f, 12);
                Ref ch = g_sim.spawnUnit(f == F_CYBER ? U_C_CHOSEN : U_K_CHOSEN, vp, c);
                if (!spotted) g_sim.cmdShadowCall(vp, ch);   // (its guard would shoot the helicopter down: the spotted scene shows the Chosen alone)
                g_sim.cmdStop({ch});
                std::vector<Ref> foes;
                for (int k = 0; k < 4; k++) foes.push_back(g_sim.spawnUnit(firstUnitOf(ff) + (k == 0 ? 5 : 2), foe, g_map.nearestFree(c + Vec2(260.0f + k * 26.0f, -60.0f + k * 40.0f), 8)));
                if (spotted) { Ref h = g_sim.spawnUnit(ff == F_CYBER ? U_C_HELI : U_K_AIR, foe, c + Vec2(70, -60)); g_sim.get(h)->alt = 1; g_sim.cmdStop({h}); for (Ref r : foes) g_sim.cmdStop({r}); }
                for (int t = 0; t < 20 * 3; t++) { g_sim.step(); g_sim.events.clear(); }
                stealthCam = Vec2(clampf(c.x - SCREEN_W / 2 + 120, 0, WORLD_W - SCREEN_W), clampf(c.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); haveStealthCam = true;
                g_game.selection.clear(); if (!spotted) g_game.selection.push_back(ch);
            }
            if (const char* hs = getenv("ONEHOUR_HELI")) {   // ONEHOUR_HELI=N: N helicopters of the viewed army hover over its base, a few more parked at an airfield
                int vp = std::min(viewPlayer, g_sim.numPlayers - 1), ht = g_sim.players[vp].faction == F_CYBER ? (int)U_C_HELI : (int)U_K_AIR, n = std::max(1, atoi(hs));
                std::vector<Ref> hs2;
                for (int k = 0; k < n; k++) { Ref r = g_sim.spawnUnit(ht, vp, b + Vec2(-200 + (k % 4) * 120, -120 + (k / 4) * 110)); g_sim.get(r)->alt = 1; g_sim.get(r)->angle = 0.3f * k; hs2.push_back(r); }
                if (getenv("ONEHOUR_HELIGUARD")) { g_sim.cmdGuardArea(hs2, b, 3.0f * TILE); for (int t = 0; t < 20 * 15; t++) { g_sim.step(); g_sim.events.clear(); } }   // (they guard a small circle: each hovers on a spot of its own)
                for (int t = 0; t < 30; t++) { g_sim.step(); g_sim.events.clear(); }
            }
            if (upgShow && std::string(upgShow) == "raid") {
                int vp = std::min(viewPlayer, g_sim.numPlayers - 1), foe = vp == 0 ? 1 : 0, uf = firstUnitOf(g_sim.players[foe].faction);
                g_game.cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                std::vector<Ref> raid;
                for (int k = 0; k < 4; k++) raid.push_back(g_sim.spawnUnit(uf + 8, foe, b + Vec2(-260 + k * 150, -150 + (k & 1) * 260)));
                for (int k = 0; k < 4; k++) raid.push_back(g_sim.spawnUnit(uf + 5, foe, g_map.nearestFree(b + Vec2(-300 + k * 200, 200), 10)));
                g_sim.cmdMove(raid, b, true);
                for (int t = 0; t < 40; t++) { g_sim.step(); g_sim.events.clear(); }
            }
            if (viewPlayer == 9) {
                // centre on the action: the unit hit most recently
                float best = -1; for (auto& e : g_sim.ents) if (e.alive && e.lastDamaged > best) { best = e.lastDamaged; b = e.pos; }
                if (!g_sim.projs.empty()) b = g_sim.projs[0].pos;
                printf("action at tile %d,%d (projectiles %zu, fx %zu, lastDamaged %.1f at t=%.1f)\n", tileOf(b.x), tileOf(b.y), g_sim.projs.size(), g_sim.fx.size(), best, g_sim.time);
            }
            g_game.cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            if (getenv("ONEHOUR_NAVY")) {   // ONEHOUR_NAVY=ticks: the lake with yards, fleets under drone screens, jeeps with crews and anti-nuke defences; that many ticks of fighting first
                navyShowcase(std::min(viewPlayer, g_sim.numPlayers - 1), stealthCam); haveStealthCam = true;
                for (int t = 0; t < atoi(getenv("ONEHOUR_NAVY")); t++) { g_sim.step(); g_sim.events.clear(); }
                if (const char* ns = getenv("ONEHOUR_NAVYSEL")) {   // ship | yard | jeep | an | dozer: what is selected (the HUD shows its panel and buttons)
                    int vp2 = std::min(viewPlayer, g_sim.numPlayers - 1); std::string w = ns; g_game.selection.clear();
                    for (auto& e : g_sim.ents) {
                        if (!e.alive || e.owner != vp2) continue;
                        bool hit = (w == "ship" && e.isUnit() && e.ut().naval) || (w == "jeep" && e.isUnit() && e.ut().kind == UK_VEH && e.ut().cargoCap > 0) || (w == "yard" && e.isBuilding() && e.bt().role == BR_NAVAL) || (w == "an" && e.isBuilding() && e.bt().role == BR_ANTINUKE) || (w == "dozer" && e.isUnit() && e.ut().role == UR_DOZER);
                        if (hit) { g_game.selection.push_back(g_sim.refOf(e)); if (w != "ship" || g_game.selection.size() >= 3) break; }
                    }
                }
            }
            if (const char* air = getenv("ONEHOUR_AIR")) {   // ONEHOUR_AIR=strike|drop|nuke, ONEHOUR_AIRTICKS=n: the strike fighters over a cluster, a jeep drop on the silk, or an anti-nuke ring meeting a warhead
                Ref follow; airShowcase(std::min(viewPlayer, g_sim.numPlayers - 1), air, stealthCam, follow); haveStealthCam = true;
                int n = getenv("ONEHOUR_AIRTICKS") ? atoi(getenv("ONEHOUR_AIRTICKS")) : 40;
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); }
                if (Entity* fe = g_sim.get(follow)) stealthCam = Vec2(clampf(fe->pos.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(fe->pos.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            }
            if (haveStealthCam) g_game.cam = stealthCam;
            g_game.renderAlpha = 1;
            if (viewPlayer != 0 || getenv("ONEHOUR_REVEAL")) { std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1); }
            if (getenv("ONEHOUR_SCAN")) { Player& sp = g_sim.players[g_sim.humanPlayer]; sp.revealUntil = g_sim.time + SCANS[sp.faction].duration - 20.0f; std::fill(sp.explored.begin(), sp.explored.end(), 1); }   // a satellite scan 20 s in (combine with ONEHOUR_WEATHER=night)
            if (const char* cm = getenv("ONEHOUR_CAM")) {   // ONEHOUR_CAM=tx,ty: centre the view on a map tile
                float tx = 40, ty = 40; if (sscanf(cm, "%f,%f", &tx, &ty) == 2) g_game.cam = Vec2(clampf(tx * TILE - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(ty * TILE - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            }
            // ONEHOUR_SEL=turret|army|haul|enemy: showcase selections for screenshots
            if (const char* sel = getenv("ONEHOUR_SEL")) {
                std::string m = sel;
                Vec2 focus = b;
                if (m == "dozer") {   // the dozer's build menu: both walls and the Waymaker included (every structure unlocked with money to spare)
                    g_sim.players[0].money = 90000;
                    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER && !civicDozerType(e.type)) { g_game.selection.push_back(g_sim.refOf(e)); focus = e.pos; break; }
                } else if (m == "tech") {
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
            if (const char* ts = getenv("ONEHOUR_TREADS")) {   // showcase: a column of tanks driving across open ground, rendered frame by frame so their track marks and dust build up (ONEHOUR_TREADS=frames)
                Faction f0 = g_sim.players[0].faction; Vec2 bp = g_sim.players[0].basePos;
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
                std::vector<Ref> col; Vec2 st = g_map.nearestFree(bp + Vec2(300, 160), 20);
                for (int i = 0; i < 4; i++) col.push_back(g_sim.spawnUnit(firstUnitOf(f0) + 5, 0, g_map.nearestFree(st + Vec2(0, i * 34), 10)));
                g_sim.cmdMove(col, g_map.nearestFree(st + Vec2(260, 60), 20), true);
                g_game.cam = Vec2(clampf(st.x + 120 - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(st.y + 40 - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                int n = std::max(10, atoi(ts));
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; g_game.render(); g_game.updateParticles(SIM_DT); }
            }
            if (getenv("ONEHOUR_BOMBS")) {   // showcase: three bombers working over an enemy outpost (ONEHOUR_BOMBS=N ticks into the raid)
                Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction; int bb0 = firstBuildOf(f0), bb1 = firstBuildOf(f1);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1); std::fill(g_sim.players[1].explored.begin(), g_sim.players[1].explored.end(), 1);
                Vec2 mid = openGroundNear(bp + (ep - bp) * 0.5f);
                Ref af;
                for (int dy = -8; dy < 8 && !af.valid(); dy++) for (int dx = -8; dx < 8 && !af.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, bb0 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(bb0 + BR_AIRFIELD, 0, tx, ty, true); }
                Vec2 pad = g_sim.get(af)->pos; Vec2 tgc = pad + Vec2(10 * TILE, 4 * TILE); Ref first;
                int types[4] = { bb1 + BR_FACTORY, bb1 + BR_BARRACKS, bb1 + BR_POWER, bb1 + BR_SUPPLY }; int placed = 0;
                for (int k = 0; k < 4; k++) for (int ring = 0; ring < 12; ring++) { bool ok = false; for (int a = 0; a < 16 && !ok; a++) { int tx = tileOf(tgc.x) + (int)(std::cos(a * 0.39f + k) * (ring + k * 3)), ty = tileOf(tgc.y) + (int)(std::sin(a * 0.39f + k) * (ring + k * 3)); if (g_sim.canPlace(1, types[k], tx, ty)) { Ref r = g_sim.placeBuilding(types[k], 1, tx, ty, true); if (!first.valid()) first = r; placed++; ok = true; } } if (ok) break; }
                for (int i = 0; i < 6; i++) g_sim.spawnUnit(f1 == F_CYBER ? U_C_INF1 : U_K_INF1, 1, g_map.nearestFree(tgc + Vec2(i * 14 - 40, 40), 10));
                std::vector<Ref> bm;
                for (int i = 0; i < 3; i++) { Ref b = g_sim.spawnUnit(firstUnitOf(f0) + 8, 0, pad + Vec2((i - 1) * 30, 0)); g_sim.get(b)->home = af; bm.push_back(b); }
                g_sim.cmdAttack(bm, first);
                int n = atoi(getenv("ONEHOUR_BOMBS")); if (n < 2) n = 70;
                g_game.cam = Vec2(clampf(tgc.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(tgc.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));   // cosmetic particles only spawn for effects on screen
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                Vec2 c = g_sim.get(first) ? g_sim.get(first)->pos : tgc;
                g_game.cam = Vec2(clampf(c.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(c.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                g_game.selection.clear(); g_game.selection.push_back(bm[0]);
                printf("bombs showcase: %d structures placed, %zu projectiles, %zu fx, %zu particles\n", placed, g_sim.projs.size(), g_sim.fx.size(), g_game.parts.size());
            }
            if (getenv("ONEHOUR_SBOMBER")) {   // showcase: a Supersonic Bomber's nuclear strike on an enemy outpost (ONEHOUR_SBOMBER=N ticks into the run; =park to show it parked and loaded)
                Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction; int bb0 = firstBuildOf(f0), bb1 = firstBuildOf(f1);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Vec2 mid = openGroundNear(bp + (ep - bp) * 0.5f);
                Ref af, tech;
                for (int dy = -10; dy < 10 && !af.valid(); dy++) for (int dx = -10; dx < 10 && !af.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, bb0 + BR_AIRFIELD, tx, ty)) af = g_sim.placeBuilding(bb0 + BR_AIRFIELD, 0, tx, ty, true); }
                for (int dy = -14; dy < 14 && !tech.valid(); dy++) for (int dx = -14; dx < 14 && !tech.valid(); dx++) { int tx = tileOf(mid.x) + dx, ty = tileOf(mid.y) + dy; if (g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) tech = g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
                g_sim.updatePowerPublic();
                if (!g_sim.get(af)) { fprintf(stderr, "supersonic showcase: no airfield spot\n"); }
                else {
                    Vec2 pad = g_sim.get(af)->pos; Vec2 tgc = pad + Vec2(11 * TILE, 4 * TILE); Ref first;
                    bool park = getenv("ONEHOUR_SBOMBER")[0] == 'p';
                    int types[4] = { bb1 + BR_FACTORY, bb1 + BR_BARRACKS, bb1 + BR_POWER, bb1 + BR_SUPPLY };
                    if (!park) {   // (park: no enemy outpost, so the bomber settles on its pad instead of picking a fight)
                        for (int k = 0; k < 4; k++) for (int ring = 0; ring < 12 && !first.valid(); ring++) for (int a = 0; a < 16 && !first.valid(); a++) { int tx = tileOf(tgc.x) + (int)(std::cos(a * 0.39f + k) * (ring + k * 3)), ty = tileOf(tgc.y) + (int)(std::sin(a * 0.39f + k) * (ring + k * 3)); if (g_sim.canPlace(1, types[k], tx, ty)) { Ref r = g_sim.placeBuilding(types[k], 1, tx, ty, true); if (!first.valid()) first = r; } }
                        for (int i = 0; i < 6; i++) g_sim.spawnUnit(f1 == F_CYBER ? U_C_INF1 : U_K_INF1, 1, g_map.nearestFree(tgc + Vec2(i * 14 - 40, 40), 10));
                    }
                    int sbt = f0 == F_CYBER ? U_C_SBOMBER : U_K_SBOMBER;
                    Ref b = g_sim.spawnUnit(sbt, 0, pad + Vec2(0, 30)); g_sim.get(b)->home = af;
                    if (!park && g_sim.get(first)) g_sim.cmdAttack({ b }, first);
                    int n = park ? 100 : atoi(getenv("ONEHOUR_SBOMBER")); if (n < 2) n = 80;
                    g_game.cam = Vec2(clampf(tgc.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(tgc.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                    for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                    if (Entity* q = g_sim.get(b)) {
                        Vec2 look = park ? g_sim.get(af)->pos : q->pos;   // (parked: frame the airfield, the plane settles on one of its pads)
                        if (!park) if (Entity* t = g_sim.get(q->targetEnt)) look = t->pos;
                        g_game.cam = Vec2(clampf(look.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(look.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                    }
                    g_game.selection.clear(); g_game.selection.push_back(b);
                    printf("supersonic showcase: bomber %s, %zu projectiles, %zu fx, %zu nukes, %zu particles\n", g_sim.get(b) ? "in the air" : "lost", g_sim.projs.size(), g_sim.fx.size(), g_sim.nukes.size(), g_game.parts.size());
                }
            }
            if (getenv("ONEHOUR_RADTRUCK")) {   // showcase: two Anti-Radiation Trucks scrubbing a fresh nuke crater (ONEHOUR_RADTRUCK=N ticks)
                Faction f0 = g_sim.players[0].faction;
                Vec2 bp = g_sim.players[0].basePos, dir = (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - bp).norm();
                for (int p = 0; p < g_sim.numPlayers; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Vec2 mid = openGroundNear(bp + dir * (18 * TILE), 12);
                flattenArena(mid, 16, 14);
                Sim::Nuke n; n.from = bp; n.pos = mid; n.owner = 0; n.t = Sim::NUKE_FLIGHT - 0.05f;   // a fresh tactical nuke: leaves the crater behind
                g_sim.nukes.push_back(n);
                for (int t = 0; t < 10; t++) { g_sim.step(); g_sim.events.clear(); g_sim.nukes.clear(); }
                int rt = f0 == F_CYBER ? U_C_RADTRUCK : U_K_RADTRUCK;
                std::vector<Ref> trucks;
                for (int k = 0; k < 2; k++) trucks.push_back(g_sim.spawnUnit(rt, 0, mid + Vec2((k * 2 - 1) * 5 * TILE, 0)));
                for (int k = 0; k < 4; k++) g_sim.spawnUnit(firstUnitOf(f0) + 2, 0, mid + Vec2((k % 2 ? 1 : -1) * 2 * TILE, (k / 2 ? 1 : -1) * 2 * TILE));
                int n2 = atoi(getenv("ONEHOUR_RADTRUCK")); if (n2 < 2) n2 = 120;
                g_game.cam = Vec2(clampf(mid.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(mid.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int t = 0; t < n2; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                g_game.selection = trucks;
                printf("radiation showcase: %zu fallout zone(s) left, %zu trucks, first truck hp %.0f, %zu particles\n", g_sim.fallouts.size(), trucks.size(), g_sim.get(trucks[0]) ? g_sim.get(trucks[0])->hp : 0.0f, g_game.parts.size());
            }
            if (getenv("ONEHOUR_DROP")) {   // showcase: a paradrop over the middle of the map (ONEHOUR_DROP=N ticks after the call; ONEHOUR_CAMBACK=px shifts the camera against the plane's heading)
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Faction f0 = g_sim.players[0].faction; int bb0 = firstBuildOf(f0);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (!g_sim.hasBuilding(0, bb0 + BR_TECH) && g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
                g_sim.players[0].dropReady = 0;
                if (getenv("ONEHOUR_DROPUPG")) {   // showcase the same Data Center later in the match: its program and its structure upgrades are already in
                    g_sim.players[0].advTech = true; g_sim.players[0].upg[UPG_REPAIR] = g_sim.players[0].upg[UPG_GUNS] = true;
                }
                Vec2 tgt = g_map.nearestFree(bp + (ep - bp) * 0.45f, 40);
                if (!g_sim.cmdParadrop(0, tgt)) fprintf(stderr, "drop showcase: the call was refused\n");
                int n = atoi(getenv("ONEHOUR_DROP")); if (n < 1) n = 100;
                float back = getenv("ONEHOUR_CAMBACK") ? (float)atof(getenv("ONEHOUR_CAMBACK")) : 0.0f;
                Vec2 dir = g_sim.airlifts.empty() ? Vec2(1, 0) : g_sim.airlifts[0].dir;
                Vec2 look = tgt - dir * back;
                g_game.cam = Vec2(clampf(look.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(look.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                printf("drop showcase: %zu planes in the air, %d units, %zu in the manifest (tier %d of %d)\n", g_sim.airlifts.size(), g_sim.countUnits(0), g_sim.dropManifest(0).size(), g_sim.dropTier(0) + 1, DTI_COUNT);
            }
            if (getenv("ONEHOUR_OCEAN")) {   // showcase: a carrier group off the main island's west coast with a loaded cargo ship, an enemy warship and gunships coming at it (ONEHOUR_OCEAN=N ticks)
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction;
                bool cy = f0 == F_CYBER, cy1 = f1 == F_CYBER;
                Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 14) * TILE, WORLD_H * 0.5f), 12, true, g_map.oceanComp);
                Ref car = g_sim.spawnUnit(cy ? U_C_CARRIER : U_K_CARRIER, 0, sea);
                g_sim.get(car)->angle = -1.2f;
                Ref cargo = g_sim.spawnUnit(cy ? U_C_CARGOSHIP : U_K_CARGOSHIP, 0, g_map.nearestWater(sea + Vec2(6 * TILE, 5 * TILE), 8, true, g_map.oceanComp));
                for (int k = 0; k < 2; k++) g_sim.spawnUnit(cy ? U_C_DESTROYER : U_K_DESTROYER, 0, g_map.nearestWater(sea + Vec2((k ? -5 : 4) * TILE, -6 * TILE), 8, true, g_map.oceanComp));
                for (int k = 0; k < 8; k++) { Ref u = g_sim.spawnUnit(firstUnitOf(f0) + (k < 4 ? 5 : 2), 0, g_sim.get(cargo)->pos); g_sim.boardPublic(*g_sim.get(u), *g_sim.get(cargo)); }
                g_sim.spawnUnit(cy1 ? U_C_DESTROYER : U_K_DESTROYER, 1, g_map.nearestWater(sea + Vec2(-4 * TILE, 16 * TILE), 10, true, g_map.oceanComp));
                for (int k = 0; k < 3; k++) g_sim.spawnUnit(cy1 ? U_C_HELI : U_K_AIR, 1, sea + Vec2((k - 1) * 3 * TILE, -15 * TILE));
                int n = atoi(getenv("ONEHOUR_OCEAN")); if (n < 1) n = 60;
                g_game.cam = Vec2(clampf(sea.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(sea.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                if (getenv("ONEHOUR_OCEANSEL")) g_game.selection = { std::string(getenv("ONEHOUR_OCEANSEL")) == "cargo" ? cargo : car };
            }
            if (getenv("ONEHOUR_FLEET")) {   // showcase: a Fleet Call off the main island's west coast, N ticks after the call (ONEHOUR_FLEETCAM=entry|goal, ONEHOUR_FLEETPICK=1: the targeting overlay)
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                int bb0 = firstBuildOf(g_sim.players[0].faction);
                Vec2 bp = g_sim.players[0].basePos;
                for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (!g_sim.hasBuilding(0, bb0 + BR_TECH) && g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
                g_sim.players[0].dropReady = 0;
                Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 18) * TILE, WORLD_H * 0.42f), 12, true, g_map.oceanComp);
                Vec2 goal, entry, n;
                g_sim.fleetEntry(sea, goal, entry, n);
                std::string camAt = getenv("ONEHOUR_FLEETCAM") ? getenv("ONEHOUR_FLEETCAM") : "entry";
                Vec2 focus = camAt == "goal" ? goal : entry + n * (6.0f * TILE);
                g_game.cam = Vec2(clampf(focus.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(focus.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                if (getenv("ONEHOUR_FLEETPICK")) {   // the targeting overlay: the mouse over the spot, the mode on
                    g_game.cam = Vec2(clampf(goal.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(goal.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                    g_game.mouseX = (int)(sea.x - g_game.cam.x); g_game.mouseY = (int)(sea.y - g_game.cam.y); g_game.fleetMode = true;
                } else {
                    g_sim.cmdFleetCall(0, sea);
                    int nt = atoi(getenv("ONEHOUR_FLEET")); if (nt < 1) nt = 60;
                    for (int t = 0; t < nt; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                }
            }
            if (getenv("ONEHOUR_STRIKE")) {   // showcase: the Air Strike's waves over an enemy line (ONEHOUR_STRIKE=N ticks after the call; ONEHOUR_CAMBACK=px shifts the camera up the approach)
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction;
                int bb0 = firstBuildOf(f0);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (!g_sim.hasBuilding(0, bb0 + BR_TECH) && g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
                g_sim.players[0].advTech = true;   // the showcase calls in the full package, as it looks at the top tier
                g_sim.players[0].upg[UPG_REPAIR] = g_sim.players[0].upg[UPG_GUNS] = true;
                g_sim.players[0].dropReady = 0;
                Vec2 tgt = g_map.nearestFree(bp + (ep - bp) * 0.45f, 40);
                for (int k = 0; k < 8; k++) {   // an enemy line drawn up right where the call goes
                    int t = (k % 2) ? firstUnitOf(f1) + 5 : firstUnitOf(f1) + 2;
                    g_sim.spawnUnit(t, 1, g_map.nearestFree(tgt + Vec2((k - 4) * 34.0f, (k % 3 - 1) * 30.0f), 12));
                }
                std::vector<int> pack = g_sim.strikeManifest(0);
                if (!g_sim.cmdPhoneStrike(0, tgt)) fprintf(stderr, "strike showcase: the call was refused\n");
                int n = atoi(getenv("ONEHOUR_STRIKE")); if (n < 1) n = 100;
                float back = getenv("ONEHOUR_CAMBACK") ? (float)atof(getenv("ONEHOUR_CAMBACK")) : 0.0f;
                Vec2 look = tgt - Vec2(0, back);
                g_game.cam = Vec2(clampf(look.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(look.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                int ours = 0, oursAir = 0, foes = 0;
                for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) { if (e.owner == 0) { ours++; if (e.isAir()) oursAir++; } else if (e.owner == 1) foes++; }
                printf("strike showcase: %d aircraft in the package (%s), %d of our aircraft in the air, enemy line left with %d units\n", (int)pack.size(), FACTION_NAME[f0], oursAir, foes);
            }
            if (getenv("ONEHOUR_AID")) {   // showcase: an Aid Drop near the enemy base (ONEHOUR_AID=N ticks after the call; ONEHOUR_CAMBACK=px shifts the camera against the heading)
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                g_sim.players[0].isAI = false;   // (only a human army has the power)
                Faction f0 = g_sim.players[0].faction; int bb0 = firstBuildOf(f0);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                for (int dy = 4; dy < 20; dy++) for (int dx = -14; dx < 14; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (!g_sim.hasBuilding(0, bb0 + BR_TECH) && g_sim.canPlace(0, bb0 + BR_TECH, tx, ty)) g_sim.placeBuilding(bb0 + BR_TECH, 0, tx, ty, true); }
                Vec2 tgt = g_map.nearestFree(bp + (ep - bp) * 0.8f, 40);
                if (!g_sim.cmdAidDrop(0, tgt)) fprintf(stderr, "aid showcase: the call was refused\n");
                int n = atoi(getenv("ONEHOUR_AID")); if (n < 1) n = 100;
                float back = getenv("ONEHOUR_CAMBACK") ? (float)atof(getenv("ONEHOUR_CAMBACK")) : 0.0f;
                Vec2 dir = g_sim.aidDrops.empty() ? Vec2(1, 0) : g_sim.aidDrops[0].dir;
                Vec2 look = tgt - dir * back;
                g_game.cam = Vec2(clampf(look.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(look.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                for (int t = 0; t < n; t++) { g_sim.step(); g_game.update(0); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                if (getenv("ONEHOUR_CAMPLANE") && !g_sim.aidDrops.empty()) { Vec2 pp = g_sim.aidDrops[0].pos; g_game.cam = Vec2(clampf(pp.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(pp.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); }   // (centre on the plane)
                printf("aid showcase: %zu flights, delivered %d to army %d\n", g_sim.aidDrops.size(), g_sim.aidDrops.empty() ? 0 : (int)g_sim.aidDrops[0].delivered, g_sim.aidDrops.empty() ? -2 : g_sim.aidDrops[0].recipient);
            }
            if (getenv("ONEHOUR_NUKEDMG")) {   // showcase: an enemy base under a nuke (ONEHOUR_NUKEDMG=N ticks after the blast)
                Faction f1 = g_sim.players[1].faction; int bb1 = firstBuildOf(f1);
                Vec2 bp = g_sim.players[0].basePos, ep = g_sim.players[1].basePos;
                for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
                Vec2 gz = g_map.nearestFree(bp + (ep - bp) * 0.5f, 40);
                int types[9] = { bb1 + BR_FACTORY, bb1 + BR_BARRACKS, bb1 + BR_POWER, bb1 + BR_TURRET, bb1 + BR_SUPPLY, bb1 + BR_TECH, bb1 + BR_AATURRET, bb1 + BR_HQ, bb1 + BR_INCOME };
                float dists[9] = { 2.5f, 4.5f, 6.0f, 3.0f, 7.5f, 8.5f, 5.0f, 9.5f, 6.5f };
                for (int k = 0; k < 9; k++) for (int tries = 0; tries < 80; tries++) {
                    float ang = k * 0.7f + tries * 0.31f, dd = dists[k] + (tries / 16) * 0.6f;
                    Vec2 c = gz + Vec2(std::cos(ang), std::sin(ang)) * (dd * TILE);
                    int tx = tileOf(c.x) - BUILDS[types[k]].w / 2, ty = tileOf(c.y) - BUILDS[types[k]].h / 2;
                    if (inMap(tx, ty) && g_sim.canPlace(1, types[k], tx, ty)) { g_sim.placeBuilding(types[k], 1, tx, ty, true); break; }
                }
                for (int i = 0; i < 6; i++) g_sim.spawnUnit(f1 == F_CYBER ? U_C_TANK : U_K_TANK, 1, g_map.nearestFree(gz + Vec2(i * 30 - 80, 60 + (i % 2) * 40), 20));
                for (int i = 0; i < 3; i++) g_sim.spawnUnit(firstUnitOf(f1) + 8, 1, gz + Vec2(i * 40 - 40, -30));
                g_sim.nukes.push_back({bp, gz, 0, Sim::NUKE_FLIGHT - 0.05f});
                g_game.cam = Vec2(clampf(gz.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(gz.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                int n = atoi(getenv("ONEHOUR_NUKEDMG")); if (n < 2) n = 120;
                for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                int left = 0, small = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1) { left++; printf("  %s %.0f%%\n", e.bt().name, 100 * e.hp / e.maxHp); }
                (void)small; printf("nuke showcase: %d enemy structures left\n", left);
            }
            if (const char* bk = getenv("ONEHOUR_BUNKER")) {   // showcase of the bunker update: base | line | way | rain | rainfall | tanks | commando (combine with ONEHOUR_WEATHER=...)
                std::string m = bk;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto camOn = [&](Vec2 p) { g_game.cam = Vec2(clampf(p.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(p.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); };
                auto frames = [&](int n, bool sim) { for (int k = 0; k < n; k++) { if (sim) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); } g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; g_game.render(); g_game.updateParticles(SIM_DT); } };
                Vec2 c = openGroundNear(g_sim.players[0].basePos + Vec2(15.0f * TILE, 4.0f * TILE), 9);
                flattenArena(c, 9, 6);
                int cx = tileOf(c.x), cy = tileOf(c.y);
                Ref dz = NOREF; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) { dz = g_sim.refOf(e); break; }
                const bool cy0 = g_sim.players[0].faction == F_CYBER;
                if (m == "base") {   // two bunkers: one bare, one with every upgrade (the laser battery tracking a gunship, the hospital at work, a soldier crowd inside)
                    int bt = cy0 ? B_C_BUNKER : B_K_BUNKER;
                    Ref a = g_sim.placeBuilding(bt, 0, cx - 6, cy - 1, true), b = g_sim.placeBuilding(bt, 0, cx + 3, cy - 1, true);
                    for (int u = 0; u < BU_COUNT; u++) { g_sim.get(b)->bunkerUpg |= (u8)(1 << u); }
                    g_sim.get(b)->maxHp *= BUNKER_SHELL_HP; g_sim.get(b)->hp = g_sim.get(b)->maxHp; g_sim.anyBunkerLasers = true;
                    g_sim.get(a)->bunkerBusy = BU_LASERS; g_sim.get(a)->bunkerProgress = 0.6f;
                    for (int k = 0; k < 30; k++) { Ref r = g_sim.spawnUnit(cy0 ? U_C_INF1 : U_K_INF1, 0, c + Vec2(0, 3.0f * TILE)); g_sim.boardQuiet(*g_sim.get(r), *g_sim.get(b)); }
                    Ref t = g_sim.spawnUnit(cy0 ? U_C_TANK : U_K_TANK, 0, c + Vec2(6.0f * TILE, 3.5f * TILE)); g_sim.get(t)->hp *= 0.4f;
                    g_sim.spawnUnit(cy0 ? U_K_AIR : U_C_AIR, 1, c + Vec2(9.0f * TILE, -5.0f * TILE));
                    camOn(c + Vec2(0, TILE));
                    frames(getenv("ONEHOUR_BUNKERTICKS") ? atoi(getenv("ONEHOUR_BUNKERTICKS")) : 18, true);
                    g_game.selection.clear(); g_game.selection.push_back(b);
                } else if (m == "line") {   // a dozer selected, the player shift-dragging a wall line (the preview), half of an earlier line already going up
                    int wt = cy0 ? B_C_WALL : B_K_WALL;
                    std::vector<std::pair<int, int>> row; for (int k = 0; k < 10; k++) row.push_back({cx - 7 + k, cy + 3});
                    g_sim.cmdBuildLine({dz}, wt, row);
                    camOn(c);
                    frames(200, true);
                    g_game.selection.clear(); g_game.selection.push_back(dz);
                    g_game.placingType = wt; g_game.bulkDrag = true; g_game.bulkTx = cx - 7; g_game.bulkTy = cy - 2;
                    Vec2 sc = tileCenter(cx + 6, cy - 5) - g_game.cam; g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
                } else if (m == "rain" || m == "rainfall" || m == "tanks" || m == "commando") {
                    // rain: a fully upgraded bunker selected, aiming its Rain Nukes (the command bar with the powers and Upgrade All; the big circle)
                    // rainfall: the five warheads coming down on an enemy camp (ONEHOUR_BUNKERTICKS ticks after the call); tanks: a Tank Drop on the silk
                    // commando: laser commandos lancing an enemy squad and a tank (toxic clouds)
                    int bt = cy0 ? B_C_BUNKER : B_K_BUNKER;
                    Ref b = g_sim.placeBuilding(bt, 0, cx - 6, cy - 1, true);
                    Entity* be = g_sim.get(b);
                    be->bunkerUpg = (u8)((1 << BU_COUNT) - 1); be->maxHp *= BUNKER_SHELL_HP; be->hp = be->maxHp; g_sim.anyBunkerLasers = true;
                    int ticks = getenv("ONEHOUR_BUNKERTICKS") ? atoi(getenv("ONEHOUR_BUNKERTICKS")) : 18;
                    Vec2 tgt = c + Vec2(8.0f * TILE, 0);
                    if (m == "rain") {
                        camOn(c + Vec2(3.0f * TILE, 0));
                        frames(6, true);
                        g_game.selection.clear(); g_game.selection.push_back(b);
                        g_game.nukeMode = true; g_game.rainBunker = b;
                        Vec2 sc = tgt - g_game.cam; g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
                    } else if (m == "rainfall") {
                        for (int k = 0; k < 14; k++) g_sim.spawnUnit(cy0 ? U_K_INF1 : U_C_INF1, 1, tgt + Vec2(g_sim.rng.f(-6, 6) * TILE, g_sim.rng.f(-4, 4) * TILE));
                        for (int k = 0; k < 4; k++) g_sim.spawnUnit(cy0 ? U_K_TANK : U_C_TANK, 1, tgt + Vec2(g_sim.rng.f(-5, 5) * TILE, g_sim.rng.f(-4, 4) * TILE));
                        g_sim.cmdBunkerRain(b, tgt);
                        camOn(c + Vec2(4.0f * TILE, 0));
                        frames(ticks, true);
                    } else if (m == "tanks") {
                        Vec2 z = c + Vec2(6.0f * TILE, 2.0f * TILE);
                        g_sim.cmdTankDrop(b, z);
                        camOn(z);
                        frames(ticks, true);
                    } else {
                        for (int k = 0; k < 3; k++) g_sim.spawnUnit(cy0 ? U_C_LASERCMD : U_K_LASERCMD, 0, c + Vec2(-1.0f * TILE, (k - 1) * 1.2f * TILE));
                        for (int k = 0; k < 10; k++) g_sim.spawnUnit(cy0 ? U_K_INF1 : U_C_INF1, 1, c + Vec2((5.5f + (k % 3) * 0.6f) * TILE, ((k / 3) - 1.5f) * 0.9f * TILE));
                        Ref tk = g_sim.spawnUnit(cy0 ? U_K_TANK : U_C_TANK, 1, c + Vec2(6.0f * TILE, 3.0f * TILE)); g_sim.get(tk)->hp = g_sim.get(tk)->maxHp;
                        camOn(c + Vec2(2.5f * TILE, 0));
                        frames(ticks, true);
                    }
                } else if (m == "way") {   // a Waymaker over the corner of a structure and the wood beside it: the ghost marks what it clears and what it keeps
                    int pw = cy0 ? B_C_POWER : B_K_POWER, wt = cy0 ? B_C_WAYMAKER : B_K_WAYMAKER;
                    g_sim.placeBuilding(pw, 0, cx - 1, cy - 1, true);
                    for (int j = -3; j <= 3; j++) for (int i = 1; i <= 5; i++) { int q = (cy + j) * MAP_W + cx + i; g_map.tiles[q] = T_TREE; g_map.blocked[q] |= 1; }
                    g_map.touch(); g_gfx.rebakeTerrain();
                    camOn(c);
                    frames(4, false);
                    g_game.selection.clear(); g_game.selection.push_back(dz);
                    g_game.placingType = wt;
                    Vec2 sc = tileCenter(cx + 1, cy) - g_game.cam; g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
                }
            }
            if (const char* lk = getenv("ONEHOUR_LAKE")) {   // showcase of the lake update: sea | swim | boats | way (combine with ONEHOUR_WEATHER=night,rain ...)
                std::string m = lk;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto camOn = [&](Vec2 p) { g_game.cam = Vec2(clampf(p.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(p.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); };
                auto frames = [&](int n, bool sim) { for (int k = 0; k < n; k++) { if (sim) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); } g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; g_game.render(); g_game.updateParticles(SIM_DT); } };
                if (m == "sea") {   // the lake's wildlife gathered in one view: the big animals in a grid, every other one at the surface (the rest under it), a school below them
                    Vec2 c = g_game.sea.empty() ? Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) : g_game.sea[0].pos;
                    auto nearOpen = [&](Vec2 p, int bi) {   // the nearest spot of body bi with water all round, searching outwards from p
                        for (int rr = 0; rr <= 24; rr++) for (int dy = -rr; dy <= rr; dy++) for (int dx = -rr; dx <= rr; dx++) {
                            if (std::max(std::abs(dx), std::abs(dy)) != rr) continue;
                            Vec2 q = p + Vec2((float)(dx * TILE), (float)(dy * TILE));
                            if (g_game.seaFree(q, bi)) return q;
                        }
                        return p;
                    };
                    for (size_t i = 0; i < g_game.sea.size(); i++) {
                        auto& b = g_game.sea[i];
                        if (b.lead >= 0) continue;   // a pod member follows its leader below
                        Vec2 want = c + Vec2((float)((int)(i % 4) - 1.5f) * 150.0f, (float)((int)(i / 4) - 0.5f) * 170.0f);
                        b.pos = nearOpen(want, b.bi);
                        bool up = i % 2 == 0;
                        b.st = up ? 2 : 0; b.surf = up ? 1.0f : 0.0f; b.roll = up ? 0.5f : 0.0f; b.fluke = up ? 0.0f : 0.15f;
                    }
                    for (size_t i = 0; i < g_game.sea.size(); i++) {   // the pod members keep their places beside their leaders
                        auto& b = g_game.sea[i];
                        if (b.lead < 0) continue;
                        const auto& L = g_game.sea[b.lead];
                        b.pos = L.pos + Vec2(b.off.x * std::cos(L.ang) - b.off.y * std::sin(L.ang), b.off.x * std::sin(L.ang) + b.off.y * std::cos(L.ang));
                        b.st = 2; b.surf = 1.0f; b.roll = 0.5f; b.fluke = 0.0f;
                    }
                    for (size_t k = 0; k < g_game.seaSchools.size() && k < 2; k++) {   // two schools below the animals, their fish gathered round a spot
                        auto& S = g_game.seaSchools[k];
                        Vec2 spot = nearOpen(c + Vec2(k ? 240.0f : -240.0f, 210.0f), S.bi);
                        S.cen = spot;
                        for (int j = 0; j < S.n; j++) g_game.seaFish[S.first + j].pos = spot + Vec2(g_game.weather.rng.f(-16, 16), g_game.weather.rng.f(-16, 16));
                    }
                    camOn(c);
                    frames(24, false);
                } else if (m == "swim") {   // an infiltrator paddling out to the small island's launcher
                    Entity* l = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE && dist(e.pos, Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f)) > 8 * TILE) l = &e;
                    if (l) {
                        Vec2 shore = g_map.nearestFree(l->pos + Vec2(14 * TILE, -2 * TILE), 20);
                        Ref spy = g_sim.spawnUnit(firstUnitOf(g_sim.players[0].faction) == U_C_DOZER ? U_C_SPY : U_K_SPY, 0, shore);
                        g_sim.cmdAttack({spy}, g_sim.refOf(*l));
                        for (int t = 0; t < 20 * 14; t++) { g_sim.step(); g_sim.events.clear(); }
                        if (Entity* s = g_sim.get(spy)) { camOn(s->pos); g_game.selection.clear(); g_game.selection.push_back(spy); }
                        frames(20, true);
                    }
                } else if (m == "boats") {   // a pack of patrol boats from each side meeting on the lake, a gunship overhead
                    Vec2 c(WORLD_W * 0.5f, WORLD_H * 0.5f - 22 * TILE);
                    std::vector<Ref> a, b;
                    for (int i = 0; i < 5; i++) a.push_back(g_sim.spawnUnit(g_sim.players[0].faction == F_CYBER ? U_C_PATROL : U_K_PATROL, 0, c + Vec2(-7 * TILE, (i - 2) * 40.0f)));
                    for (int i = 0; i < 5; i++) b.push_back(g_sim.spawnUnit(g_sim.players[1].faction == F_CYBER ? U_C_PATROL : U_K_PATROL, 1, c + Vec2(7 * TILE, (i - 2) * 40.0f)));
                    Ref h = g_sim.spawnUnit(firstUnitOf(g_sim.players[1].faction) + 8, 1, c + Vec2(2 * TILE, -3 * TILE)); g_sim.get(h)->alt = 1;
                    g_sim.cmdMove(a, c + Vec2(2 * TILE, 0), true); g_sim.cmdMove(b, c - Vec2(2 * TILE, 0), true);
                    camOn(c);
                    frames(150, true);
                } else if (m == "walls") {   // the two walls side by side: a line of ordinary walls and a line of Bastion Walls, some of them burning
                    int vp = 0; Faction f = g_sim.players[vp].faction;
                    Vec2 at = openGroundNear(g_sim.players[vp].basePos + Vec2(8 * TILE, 10 * TILE), 5);
                    int tx = tileOf(at.x) - 4, ty = tileOf(at.y) - 2;
                    std::vector<Ref> bastion;
                    for (int i = 0; i < 9; i++) {
                        g_sim.placeBuilding(f == F_CYBER ? B_C_WALL : B_K_WALL, vp, tx + i, ty, true);
                        bastion.push_back(g_sim.placeBuilding(g_sim.players[1].faction == F_CYBER ? B_C_FWALL : B_K_FWALL, 1, tx + i, ty + 3, true));   // (an enemy's Bastion Wall)
                    }
                    // our tanks pour fire on the enemy's Bastion Wall: it burns, it never breaks
                    std::vector<Ref> tanks;
                    for (int i = 0; i < 4; i++) tanks.push_back(g_sim.spawnUnit(f == F_CYBER ? U_C_TANK : U_K_TANK, vp, g_map.nearestFree(tileCenter(tx + 2 + i * 2, ty + 7), 4)));
                    g_sim.cmdAttack({ tanks[0], tanks[1] }, bastion[6]); g_sim.cmdAttack({ tanks[2], tanks[3] }, bastion[7]);
                    for (int t = 0; t < 20 * 8; t++) { g_sim.step(); g_sim.events.clear(); }
                    camOn(tileCenter(tx + 4, ty + 3));
                    frames(30, true);
                } else if (m == "way") {   // a dozer cutting a road into the ridge with a chain of Waymakers (two cleared, one at work, one placed)
                    int wt = g_sim.players[0].faction == F_CYBER ? B_C_WAYMAKER : B_K_WAYMAKER;
                    Vec2 bp = g_sim.players[0].basePos; int bx = -1, by = -1; float bd = 1e30f;
                    for (int y = 3; y < MAP_H - 4; y++) for (int x = 3; x < MAP_W - 12; x++) {
                        bool ok = g_map.passable(x - 1, y + 1);
                        for (int k = 0; k < 9 && ok; k++) for (int j = 0; j < 3 && ok; j++) if (g_map.tile(x + k, y + j) != T_ROCK || !g_map.wayTile(x + k, y + j)) ok = false;
                        float d = dist(tileCenter(x, y), bp);
                        if (ok && d < bd) { bd = d; bx = x; by = y; }
                    }
                    if (bx >= 0) {
                        Ref dz = NOREF; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) { dz = g_sim.refOf(e); break; }
                        if (Entity* d = g_sim.get(dz)) d->pos = g_map.nearestFree(tileCenter(bx - 2, by + 1), 4);
                        g_sim.cmdBuild(dz, wt, bx, by); for (int t = 0; t < (int)(WAYMAKER_TIME * SIM_HZ) + 4; t++) { g_sim.step(); g_sim.events.clear(); }
                        g_sim.cmdBuild(dz, wt, bx + 3, by); for (int t = 0; t < (int)(WAYMAKER_TIME * SIM_HZ) + 4; t++) { g_sim.step(); g_sim.events.clear(); }
                        g_sim.cmdBuild(dz, wt, bx + 6, by);
                        camOn(tileCenter(bx + 5, by + 1));
                        g_game.selection.clear(); g_game.selection.push_back(dz);
                        frames(25, true);
                        g_game.placingType = wt; Vec2 sc = tileCenter(bx + 10, by + 1) - g_game.cam; g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
                    }
                }
            }
            if (const char* up = getenv("ONEHOUR_UPDATE")) {   // showcase of the October update: launcher | bomber | trees | fortress
                std::string m = up;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto camOn = [&](Vec2 p) { g_game.cam = Vec2(clampf(p.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(p.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); };
                if (m == "launcher") {   // the big island's launcher taken by us and burning after a hit, its laser cage flaring; the small one still neutral
                    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE) {
                        if (dist(e.pos, Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f)) < 4 * TILE) { e.owner = 0; e.heat = 1.3f; e.lastDamaged = g_sim.time - 0.2f; e.siloTimer = 0; g_game.selection.clear(); g_game.selection.push_back(g_sim.refOf(e)); camOn(e.pos + Vec2(200, 0)); }
                    }
                    for (int k = 0; k < 40; k++) { g_game.frameDt = 1.0f / 30; g_game.wallTime += 1.0f / 30; g_game.render(); g_game.updateParticles(1.0f / 30); }
                } else if (m == "bomber") {   // a Nuclear Bomber call on the way in
                    Vec2 target = g_sim.players[1].basePos + Vec2(0, 200);
                    g_sim.time = std::max(g_sim.time, NUKEBOMBER_OPEN + 1.0f);
                    bool tech = false; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_TECH) tech = true;
                    if (!tech) { Vec2 sp = g_map.nearestFree(g_sim.players[0].basePos + Vec2(0, 300), 10); g_sim.placeBuilding(firstBuildOf(g_sim.players[0].faction) + BR_TECH, 0, tileOf(sp.x), tileOf(sp.y), true); }
                    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_TECH) e.bomberTimer = 0;
                    g_sim.cmdNukeBomber(0, target);
                    int steps = getenv("ONEHOUR_UPDATET") ? atoi(getenv("ONEHOUR_UPDATET")) : 60;
                    for (int t = 0; t < steps; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; g_game.render(); g_game.updateParticles(SIM_DT); }
                    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().temp) camOn(e.pos);
                } else if (m == "trees") {   // a dozer placing a barracks over a stand of trees: the ghost shows the trees it will clear
                    Vec2 bp = g_sim.players[0].basePos; int bt = firstBuildOf(g_sim.players[0].faction) + BR_BARRACKS; int fx = -1, fy = -1; float bd = 1e30f;
                    for (int y = 4; y < MAP_H - 6; y++) for (int x = 4; x < MAP_W - 6; x++) {
                        int trees = 0; for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) if (g_map.tile(x + i, y + j) == T_TREE) trees++;
                        float d = dist(tileCenter(x, y), bp);
                        if (trees >= 3 && d < bd && g_sim.canPlace(0, bt, x, y)) { bd = d; fx = x; fy = y; }
                    }
                    if (fx >= 0) {
                        Vec2 c = g_sim.buildingCenter(bt, fx, fy); camOn(c);
                        g_game.selection.clear(); for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) { g_game.selection.push_back(g_sim.refOf(e)); break; }
                        g_game.placingType = bt; Vec2 sc = c - g_game.cam; g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
                        if (getenv("ONEHOUR_UPDATEPLACE")) { g_sim.placeBuilding(bt, 0, fx, fy, true); g_game.placingType = -1; g_game.render(); }   // placed: the trees on the footprint are gone
                    }
                } else if (m == "fortress") {   // a Mountain Fortress garrisoned by our infantry, an enemy column marching on it
                    for (auto& e : g_sim.ents) if (e.alive && e.type == B_C_FORTRESS) {
                        Vec2 door = g_map.nearestFree(e.pos + Vec2(-(e.bt().w * 0.5f + 1.0f) * TILE, 0), 3);
                        for (int i = 0; i < 60; i++) { Ref r = g_sim.spawnUnit(firstUnitOf(g_sim.players[0].faction) + 2 + i % 3, 0, door); Entity* u = g_sim.get(r); Entity* f = &e; if (g_sim.canBoard(*u, *f)) g_sim.boardQuiet(*u, *f); }
                        Vec2 foe = g_map.nearestFree(e.pos + Vec2((e.bt().w * 0.5f + 7.0f) * TILE, 0), 6);
                        std::vector<Ref> col; for (int i = 0; i < 10; i++) col.push_back(g_sim.spawnUnit(firstUnitOf(g_sim.players[1].faction) + (i % 2 ? 5 : 2), 1, foe + Vec2((i % 5) * 24.0f, (i / 5) * 30.0f)));
                        g_sim.cmdMove(col, e.pos, true);
                        g_game.selection.clear(); g_game.selection.push_back(g_sim.refOf(e)); camOn(e.pos + Vec2(80, 40));
                        Ref fr = g_sim.refOf(e);
                        for (int t = 0; t < 70; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; g_game.render(); g_game.updateParticles(SIM_DT); }
                        (void)fr;
                        break;
                    }
                }
            }
            if (const char* su = getenv("ONEHOUR_SUPER")) {   // showcase of the super update: island | flight | boom | antenna
                std::string m = su;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto camOn = [&](Vec2 p) { g_game.cam = Vec2(clampf(p.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(p.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); };
                auto play = [&](int ticks) {   // (prints the frames' cost: render() + present, the software renderer's real price of the effects)
                    double worst = 0, sum = 0;
                    for (int t = 0; t < ticks; t++) {
                        g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT;
                        u64 a0 = SDL_GetPerformanceCounter(); g_game.render(); SDL_RenderPresent(g_gfx.ren); double ms = (SDL_GetPerformanceCounter() - a0) * 1000.0 / SDL_GetPerformanceFrequency();
                        sum += ms; worst = std::max(worst, ms); g_game.updateParticles(SIM_DT);
                    }
                    fprintf(stderr, "super showcase: %d frames, avg %.1f ms, worst %.1f ms\n", ticks, ticks ? sum / ticks : 0.0, worst);
                };
                Entity* L = nullptr;   // the big island's first launcher, ours and armed
                for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ISLANDNUKE && dist(e.pos, Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f)) < 6 * TILE) { L = &e; break; }
                if (L) { L->owner = 0; L->siloTimer = 0; }
                if (m == "island") {   // the big island with its two launchers (one ours, selected), the small island's pair beyond
                    if (L) { g_game.selection.clear(); g_game.selection.push_back(g_sim.refOf(*L)); camOn(Vec2(WORLD_W * 0.5f + 5 * TILE, WORLD_H * 0.5f)); }
                    play(20);
                } else if ((m == "flight" || m == "boom") && L) {   // a Super Nuke on its way to the enemy base (its huge warning circle), or a moment after it lands
                    Vec2 target = g_sim.players[1].basePos;
                    if (getenv("ONEHOUR_SUPERDEEP")) { Sim::Nuke dn; dn.from = L->pos; dn.pos = target; dn.owner = 0; dn.t = Sim::NUKE_FLIGHT - ISLAND_NUKE_FLIGHT; dn.scale = UNUKE_SCALE; dn.deep = true; g_sim.nukes.push_back(dn); }   // (an Underground Nuke in the same scene, to compare)
                    else g_sim.cmdIslandNuke(0, target, false, g_sim.refOf(*L));
                    camOn(m == "flight" ? target + (L->pos - target) * 0.35f : target + Vec2(0, -4 * TILE));
                    int t1 = getenv("ONEHOUR_SUPERT") ? atoi(getenv("ONEHOUR_SUPERT")) : (m == "flight" ? (int)(ISLAND_NUKE_FLIGHT * SIM_HZ * 0.6f) : (int)(ISLAND_NUKE_FLIGHT * SIM_HZ) + 12);
                    play(t1);
                } else if (m == "antenna") {   // a cluster of Healing Antennas mending a battered column and scrubbing a fallout zone
                    Vec2 bp = g_sim.players[0].basePos + Vec2(0, 9 * TILE);
                    int at = g_sim.players[0].faction == F_CYBER ? B_C_HEALANT : B_K_HEALANT, placed = 0;
                    for (int r = 0; r < 8 && placed < 4; r++) for (int dy = -r; dy <= r && placed < 4; dy++) for (int dx = -r; dx <= r && placed < 4; dx++) {
                        int tx = tileOf(bp.x) + dx * 3, ty = tileOf(bp.y) + dy * 3;
                        if (std::max(std::abs(dx), std::abs(dy)) == r && g_sim.canPlace(0, at, tx, ty)) { g_sim.placeBuilding(at, 0, tx, ty, true); placed++; }
                    }
                    for (int i = 0; i < 6; i++) { Ref r = g_sim.spawnUnit(firstUnitOf(g_sim.players[0].faction) + (i % 2 ? 5 : 2), 0, g_map.nearestFree(bp + Vec2((i % 3) * 40.0f - 40, (i / 3) * 40.0f - 20), 6)); if (Entity* u = g_sim.get(r)) u->hp = u->maxHp * 0.35f; }
                    g_sim.fallouts.push_back({bp + Vec2(4 * TILE, 0), 3.0f * TILE, 0.0f, 0.0f, 0.6f, -1, false, 200.0f, 0});
                    g_game.selection.clear(); for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_HEALANT) g_game.selection.push_back(g_sim.refOf(e));
                    camOn(bp);
                    play(36);
                }
            }
            if (const char* cv = getenv("ONEHOUR_CIVIL")) {   // showcase of the towns and the riots: riot | ruin | town (prints the frames' cost)
                std::string m = cv;
                for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
                auto camOn = [&](Vec2 p) { g_game.cam = Vec2(clampf(p.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(p.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); };
                auto play = [&](int ticks) {
                    double worst = 0, sum = 0;
                    for (int t = 0; t < ticks; t++) {
                        g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT;
                        u64 a0 = SDL_GetPerformanceCounter(); g_game.render(); SDL_RenderPresent(g_gfx.ren); double ms = (SDL_GetPerformanceCounter() - a0) * 1000.0 / SDL_GetPerformanceFrequency();
                        sum += ms; worst = std::max(worst, ms); g_game.updateParticles(SIM_DT);
                    }
                    printf("civil showcase: %d frames, avg %.1f ms, worst %.1f ms\n", ticks, sum / std::max(1, ticks), worst);
                };
                if (m == "riot") {   // a crowd rising against the viewed army near its base
                    g_sim.riotsOn = true;
                    if (g_sim.time < RIOT_GRACE) g_sim.time = RIOT_GRACE + 1;
                    int v = viewPlayer; g_sim.unrest[v] = 1.5f; g_sim.control[v] = 0.1f;
                    for (int t = 0; t < 30 && g_sim.riots.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
                    if (!g_sim.riots.empty()) { camOn(g_sim.riots[0].origin); play(getenv("ONEHOUR_CIVILT") ? atoi(getenv("ONEHOUR_CIVILT")) : 160); Vec2 c; int n = 0; for (Ref r : g_sim.riots[0].members) if (Entity* e = g_sim.get(r)) { c += e->pos; n++; } if (n) camOn(c * (1.0f / n)); }
                } else {   // a town held by the viewed army: 'ruin' razes a few blocks and lets the townsfolk start rebuilding, 'town' shows it whole
                    District& D = g_sim.districts[getenv("ONEHOUR_TOWNIDX") ? atoi(getenv("ONEHOUR_TOWNIDX")) % (int)g_sim.districts.size() : 0];
                    g_sim.captureDistrict((int)(&D - &g_sim.districts[0]), viewPlayer);
                    if (m == "ruin") { int k = 0; for (Ref r : D.blocks) if (Entity* b = g_sim.get(r)) { if (k++ % 3 == 0) g_sim.applyDamagePublic(*b, b->maxHp * 4.0f, -1, NOREF); else if (k % 3 == 2) b->hp = b->maxHp * 0.55f; } }
                    camOn(D.center);
                    play(m == "ruin" ? (int)((DISTRICT_REBUILD_DELAY + 12.0f) * SIM_HZ) : 60);
                    g_game.selection.clear(); g_game.selection.push_back(D.blocks[0]);
                }
            }
            if (getenv("ONEHOUR_PAUSEMENU")) g_game.openPauseMenu();
            if (getenv("ONEHOUR_NEWB")) {   // showcase: income structure, nuke ramp, a nuke in flight
                int bb2 = firstBuildOf(g_sim.players[0].faction); Vec2 bp = g_sim.players[0].basePos; Ref last;
                int types[2] = { bb2 + BR_INCOME, bb2 + BR_NUKE };
                for (int k = 0; k < 2; k++) for (int dy = 5; dy < 16; dy++) { bool ok = false; for (int dx = -12; dx < 12 && !ok; dx++) { int tx = tileOf(bp.x) + dx, ty = tileOf(bp.y) + dy; if (g_sim.canPlace(0, types[k], tx, ty)) { last = g_sim.placeBuilding(types[k], 0, tx, ty, true); ok = true; } } if (ok) break; }
                g_sim.get(last)->actionTimer = 0; g_game.selection.clear(); g_game.selection.push_back(last);
                std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1);
                bool boom = getenv("ONEHOUR_NEWB")[0] == 'b';   // ONEHOUR_NEWB=boom: freeze a moment after detonation instead of the flight
                g_sim.nukes.push_back({g_sim.get(last)->pos, g_sim.get(last)->pos + Vec2(300, boom ? -300 : 40), 0, boom ? Sim::NUKE_FLIGHT - 0.05f : 4.0f});
                if (boom) g_game.cam = Vec2(clampf(g_sim.get(last)->pos.x - SCREEN_W / 2 + 300, 0, WORLD_W - SCREEN_W), clampf(g_sim.get(last)->pos.y - 300 - VIEW_H * 0.8f, 0, WORLD_H - VIEW_H));
                if (boom) for (int t = 0, tn = getenv("ONEHOUR_BOOMT") ? atoi(getenv("ONEHOUR_BOOMT")) : 22; t < tn; t++) { g_sim.step(); g_sim.events.clear(); g_game.spawnFromFx(); g_game.updateParticles(SIM_DT); g_game.frameDt = SIM_DT; g_game.wallTime += SIM_DT; }
                g_game.cam = Vec2(clampf(g_sim.get(last)->pos.x - SCREEN_W / 2 + (boom ? 300 : 100), 0, WORLD_W - SCREEN_W), clampf(g_sim.get(last)->pos.y + (boom ? -300 : 0) - VIEW_H * (boom ? 0.8f : 0.5f), 0, WORLD_H - VIEW_H));   // (the cloud climbs above ground zero: frame it low)
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
    const bool fpsLog = getenv("ONEHOUR_FPSLOG") != nullptr;
    bool running = true;
    void leaveMatchLearning();   // (ui.cpp: the window closed mid-match - the opponent models keep what it showed)
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
        // a minimised (or hidden) window draws nothing at all: the match runs on, the GPU rests
        const u32 wf = SDL_GetWindowFlags(g_gfx.win);
        const bool hidden = (wf & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0;
        const bool idle = g_game.state != GS_PLAYING || g_game.menuOpen || g_game.paused || !(wf & SDL_WINDOW_INPUT_FOCUS);
        if (hidden) { SDL_Delay(30); continue; }
        const u64 r0 = SDL_GetPerformanceCounter();
        g_game.render();
        g_gfx.present();
        g_game.noteFrameCost((float)((SDL_GetPerformanceCounter() - r0) * 1000.0 / freq));
        if (fpsLog) {   // ONEHOUR_FPSLOG: frames a second, the frame cost and the effects detail every five seconds (a real window, GPU or not)
            static int frames = 0; static double worst = 0; static u64 since = now;
            frames++; worst = std::max(worst, (SDL_GetPerformanceCounter() - now) * 1000.0 / freq);
            if (now - since > freq * 5) { printf("fps %.1f | frame cost %.1f ms (worst whole frame %.1f ms) | detail %d | sim %.0f s, %s renderer\n", frames * (double)freq / (now - since), g_game.frameCost, worst, g_game.detail, g_sim.time, g_gfx.softwareRenderer ? "software" : "GPU"); fflush(stdout); frames = 0; worst = 0; since = now; }
        }
        // Never spin faster than the display: without vsync (the software renderer, a GPU fallback, a driver or compositor that ignores it) the
        // loop would otherwise draw hundreds of frames a second, and a laptop that heats up throttles its CPU into stutter
        // and never faster than the frame rate setting asks (see Game::targetFps): with vsync the present then waits for the next refresh
        const double spent = (SDL_GetPerformanceCounter() - now) / (double)freq, frame = 1.0 / std::max(20, g_game.targetFps(g_gfx.refreshHz, idle));
        if (spent < frame - 0.0015) SDL_Delay((Uint32)((frame - spent) * 1000.0 - 1.0));
    }
    leaveMatchLearning();
    g_audio.shutdown();
    g_gfx.shutdown();
    SDL_Quit();
    return 0;
}
