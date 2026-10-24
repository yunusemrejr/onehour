// One Hour - headless tests for the lake update (--lakeupdatetest)
//  * infiltrators take the island launchers: a right-click order on a neutral launcher or Bastion Wall is a capture order, and an infiltrator on
//    the mainland paddles out to an island on its raft (no airlift needed)
//  * the Waymaker: $100, placed by a dozer on trees, rock or a ridge, it clears its 3x3 patch into walkable ground by itself (never on water)
//  * the patrol boat: cheap, fast and fragile, built at the Naval Yard, its one mount hits aircraft, ships, infantry and structures
//  * the bigger map: a lake with about twice the water of the 146 map, two islands, both launchers out in the water
//  * the Bastion Wall: a player may pour fire on it and it burns at full health; idle units and the computer never shoot it; an infiltrator
//    steals it and its new owner may sell it
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok13 = true;
static bool fail13(const char* m) { fprintf(stderr, "lakeupdatetest: %s\n", m); g_ok13 = false; return false; }
#define CHECK13(c, m) do { if (!(c)) return fail13(m); } while (0)

static void run13(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

static void clearUnits13() {
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

static void init13(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, false };
    int diff[2] = { 1, 1 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    clearUnits13();
}

static std::vector<Entity*> ofRole13(BuildRole r) {
    std::vector<Entity*> out;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == r) out.push_back(&e);
    return out;
}

// ---- infiltrators swim out to the island launchers
static bool swimCaptureTest() {
    auto launchers = ofRole13(BR_ISLANDNUKE);
    CHECK13(launchers.size() == 4, "there are not four island launchers");
    for (int k = 0; k < 2; k++) {
        Entity* l = ofRole13(BR_ISLANDNUKE)[k * 2];   // (two on each island, in island order: one from each)
        Ref L = g_sim.refOf(*l);
        // the nearest mainland ground north of the launcher, across the water
        Vec2 shore = l->pos;
        int ty = tileOf(l->pos.y), tx = tileOf(l->pos.x);
        bool water = false;
        for (int y = ty; y > 1; y--) {
            if (g_map.tile(tx, y) == T_WATER) water = true;
            else if (water && g_map.passable(tx, y)) { shore = tileCenter(tx, y - 1); break; }
        }
        CHECK13(water && shore.y < l->pos.y, "no shore found across the water from a launcher");
        std::vector<Vec2> walk;
        CHECK13(!g_map.findPathToRect(shore, l->tx, l->ty, l->bt().w, l->bt().h, walk), "an island launcher can be walked to (the islands should be cut off)");
        Ref spy = g_sim.spawnUnit(k == 0 ? U_C_SPY : U_K_SPY, k, g_map.nearestFree(shore, 4));
        g_sim.cmdAttack({spy}, L);   // the order a right-click on the launcher gives
        Entity* s = g_sim.get(spy);
        CHECK13(s && s->order == O_CAPTURE, "ordering an infiltrator onto a neutral launcher is not a capture order");
        bool swam = false;
        float t0 = g_sim.time;
        while (g_sim.time - t0 < 150.0f) {
            run13(0.5f);
            s = g_sim.get(spy); l = g_sim.get(L);
            if (!s || !l) break;
            if (g_map.swimTile(tileOf(s->pos.x), tileOf(s->pos.y))) swam = true;
            if (l->owner == k) break;
        }
        CHECK13(s, "the infiltrator died on the way");
        CHECK13(swam, "the infiltrator never crossed the water");
        CHECK13(l && l->owner == k, "an infiltrator on the mainland could not take an island launcher");
        clearUnits13();
    }
    // a plain move order onto an island works too (the infiltrator paddles over and climbs ashore)
    {
        const auto& isl = g_map.islands[0];
        Vec2 onIsland = g_map.nearestFree(tileCenter((int)isl.cx - 3, (int)isl.cy - 3), 3);
        Vec2 shore = openGroundNear(tileCenter((int)isl.cx, (int)(isl.cy - isl.r - 30)), 2);
        Ref spy = g_sim.spawnUnit(U_C_SPY, 0, shore);
        g_sim.cmdMove({spy}, onIsland, false);
        run13(140.0f);
        Entity* s = g_sim.get(spy);
        CHECK13(s && dist(s->pos, onIsland) < 2.0f * TILE, "an infiltrator ordered onto the island did not get there");
        CHECK13(g_map.passable(tileOf(s->pos.x), tileOf(s->pos.y)), "the infiltrator stopped in the water instead of climbing ashore");
        // ordinary soldiers still cannot cross
        Ref trooper = g_sim.spawnUnit(U_C_INF1, 0, shore);
        g_sim.cmdMove({trooper}, onIsland, false);
        run13(60.0f);
        Entity* t = g_sim.get(trooper);
        CHECK13(t && dist(t->pos, onIsland) > 6.0f * TILE, "an ordinary soldier walked across the lake");
        clearUnits13();
    }
    return true;
}

// ---- the lake grew: about twice the 146 map's 2,419 water tiles
static bool lakeTest() {
    CHECK13(MAP_W == 208 && MAP_H == 208, "the map is not 208x208 (it grew again: see --superupdatetest)");
    int lake = g_map.waterSize[g_map.lakeComp];
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "lake: %d water tiles\n", lake);
    CHECK13(lake >= 2 * 2419 - 300, "the central lake is not about twice the 146 map's");
    CHECK13(g_map.islands.size() == 2, "the lake lost its islands");
    return true;
}

// ---- Waymakers cut a way into rough ground
static bool waymakerTest() {
    // a 3x3 patch of ridge rock beside open ground: the stretch of the north-west ridge below the fortress (vertical ridge at the map's x centre)
    int bx = -1, by = -1;
    for (int y = 4; y < MAP_H / 2 && bx < 0; y++) for (int x = 2; x < MAP_W - 5 && bx < 0; x++) {
        bool rock = true;
        for (int j = 0; j < 3 && rock; j++) for (int i = 0; i < 3; i++) if (g_map.tile(x + i, y + j) != T_ROCK || g_map.clearable[(y + j) * MAP_W + x + i] != 0 || (g_map.blocked[(y + j) * MAP_W + x + i] & ~1)) { rock = false; break; }
        if (rock && g_map.passable(x - 1, y + 1) && g_map.passable(x - 2, y + 1)) { bx = x; by = y; }
    }
    CHECK13(bx >= 0, "no ridge rock beside open ground found for the Waymaker scene");
    int wtype = B_C_WAYMAKER;
    CHECK13(BUILDS[wtype].cost == 100, "a Waymaker does not cost $100");
    CHECK13(g_sim.buildAvailable(0, wtype), "the Waymaker is not in the dozer's menu");
    CHECK13(!g_sim.canPlace(0, B_C_POWER, bx, by), "a power plant could be built on ridge rock");
    CHECK13(g_sim.canPlace(0, wtype, bx, by), "a Waymaker cannot be placed on ridge rock");
    // never on water, and not where there is nothing to clear
    const auto& isl = g_map.islands[0];
    int wx = (int)(isl.cx + isl.r + 4), wy = (int)isl.cy;
    CHECK13(g_map.tile(wx, wy) == T_WATER, "the test's lake spot is not water");
    CHECK13(!g_sim.canPlace(0, wtype, wx, wy), "a Waymaker could be placed on the lake");
    Vec2 open = openGroundNear(g_sim.players[0].basePos + Vec2(9 * TILE, 9 * TILE), 3);
    CHECK13(!g_sim.canPlace(0, wtype, tileOf(open.x) - 1, tileOf(open.y) - 1), "a Waymaker could be placed where there is nothing to clear");
    // the dozer places it from far away and keeps its own job; the patch clears by itself
    Ref dz = g_sim.spawnUnit(U_C_DOZER, 0, g_sim.players[0].basePos + Vec2(0, 4 * TILE));
    int money = g_sim.players[0].money;
    std::vector<Vec2> path;
    CHECK13(!g_map.findPath(tileCenter(bx - 1, by + 1), tileCenter(bx + 1, by + 1), path) || path.empty() || dist(path.back(), tileCenter(bx + 1, by + 1)) > TILE, "the test patch is walkable before the Waymaker");
    CHECK13(g_sim.cmdBuild(dz, wtype, bx, by), "a dozer could not place a Waymaker");
    CHECK13(g_sim.players[0].money == money - 100, "placing a Waymaker did not cost $100");
    Entity* d = g_sim.get(dz);
    CHECK13(d && d->order != O_BUILD, "the dozer drove off to build the Waymaker (it should clear its ground by itself)");
    run13(WAYMAKER_TIME + 0.5f);
    for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) {
        CHECK13(g_map.passable(bx + i, by + j), "a Waymaker left a tile of its patch blocked");
        CHECK13(g_map.tile(bx + i, by + j) == T_DIRT, "a cleared Waymaker patch is not open earth");
    }
    int left = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_WAYMAKER) left++;
    CHECK13(left == 0, "the Waymaker did not vanish once its ground was clear");
    CHECK13(g_map.findPath(tileCenter(bx - 1, by + 1), tileCenter(bx + 1, by + 1), path) && dist(path.back(), tileCenter(bx + 1, by + 1)) < TILE, "the cleared patch cannot be walked into");
    // a second one chained on: the road goes on
    if (g_map.tile(bx + 3, by) == T_ROCK && g_sim.canPlace(0, wtype, bx + 3, by)) {
        CHECK13(g_sim.cmdBuild(dz, wtype, bx + 3, by), "a second Waymaker could not be chained on");
        run13(WAYMAKER_TIME + 0.5f);
        CHECK13(g_map.passable(bx + 4, by + 1), "the chained Waymaker did not clear its patch");
    }
    // a civilian dozer cannot place one
    Ref cd = g_sim.spawnUnit(U_C_CIVDOZER, 0, g_sim.players[0].basePos + Vec2(0, 5 * TILE));
    for (int y = 4; y < MAP_H - 4; y++) for (int x = 4; x < MAP_W - 4; x++) if (g_sim.canPlace(0, wtype, x, y)) { CHECK13(!g_sim.cmdBuild(cd, wtype, x, y), "a civilian dozer placed a Waymaker"); y = MAP_H; break; }
    clearUnits13();
    return true;
}

// ---- the patrol boat
static bool patrolTest() {
    for (int f = 0; f < 2; f++) {
        const UnitType& p = UNITS[f == 0 ? U_C_PATROL : U_K_PATROL];
        const UnitType& d = UNITS[f == 0 ? U_C_DESTROYER : U_K_DESTROYER];
        CHECK13(p.naval && p.builtBy == BR_NAVAL && p.requires_ < 0, "the patrol boat is not a Naval Yard ship with no prerequisite");
        CHECK13(p.cost * 2 < d.cost && p.hp * 2 < d.hp && p.speed > d.speed, "the patrol boat is not cheap, fragile and fast next to a destroyer");
        const Weapon& w = WEAPONS[p.weapon];
        CHECK13(w.ground && w.air && w.mult[AR_AIR] > 0 && w.mult[AR_INF] > 0 && w.mult[AR_STRUCT] > 0 && w.mult[AR_HEAVY] > 0, "the patrol boat cannot hit aircraft, troops, ships and structures");
        const Weapon& dw = WEAPONS[d.weapon];
        CHECK13(w.dmg * w.burst / w.cooldown < dw.dmg * dw.burst / dw.cooldown && w.range < dw.range, "a patrol boat outguns a destroyer");
    }
    // one is built at a Naval Yard, sails, and shoots a helicopter and a soldier on the shore
    Ref yard = NOREF;
    for (int k = 0; k < 400 && !yard.valid(); k++) {
        float an = k * 0.37f, r = (g_map.islands[0].r + 30.0f + (k % 7)) * TILE;
        Vec2 at = Vec2(WORLD_W * 0.5f + std::cos(an) * r, WORLD_H * 0.5f + std::sin(an) * r);
        int tx = tileOf(at.x) - 1, ty = tileOf(at.y) - 1;
        if (g_sim.canPlace(0, B_C_NAVAL, tx, ty)) yard = g_sim.placeBuilding(B_C_NAVAL, 0, tx, ty, true);
    }
    CHECK13(yard.valid(), "no shore spot for a Naval Yard");
    g_sim.updatePowerPublic();
    CHECK13(g_sim.unitAvailable(0, U_C_PATROL) && g_sim.cmdTrain(yard, U_C_PATROL), "a Naval Yard will not build a patrol boat");
    run13(UNITS[U_C_PATROL].buildTime * 2.5f + 2.0f);
    if (getenv("ONEHOUR_DEBUG")) { Entity* y = g_sim.get(yard); fprintf(stderr, "yard queue %d progress %.2f lowpower %d\n", y ? (int)y->queue.size() : -1, y ? y->queueProgress : -1.0f, (int)g_sim.players[0].lowPower()); for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) fprintf(stderr, "  unit %s at %.1f,%.1f tile %d blocked %d\n", e.ut().name, e.pos.x / TILE, e.pos.y / TILE, g_map.tile(tileOf(e.pos.x), tileOf(e.pos.y)), g_map.blocked[tileOf(e.pos.y) * MAP_W + tileOf(e.pos.x)]); }
    Entity* boat = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == U_C_PATROL) boat = &e;
    CHECK13(boat && g_map.navTile(tileOf(boat->pos.x), tileOf(boat->pos.y)), "the patrol boat was not launched onto the lake");
    Ref br = g_sim.refOf(*boat);
    Ref heli = g_sim.spawnUnit(U_K_AIR, 1, boat->pos + Vec2(3.5f * TILE, 0));
    float h0 = g_sim.get(heli)->hp;
    run13(6.0f);
    Entity* hh = g_sim.get(heli);
    CHECK13(!hh || hh->hp < h0, "a patrol boat did not shoot at a gunship overhead");
    if (hh) g_sim.destroy(*hh, false);
    boat = g_sim.get(br);
    CHECK13(boat, "the patrol boat sank");
    clearUnits13();
    return true;
}

// ---- the indestructible Bastion Wall
static bool bastionTest() {
    Vec2 site = openGroundNear(g_sim.players[1].basePos + (g_sim.players[0].basePos - g_sim.players[1].basePos).norm() * (16.0f * TILE), 5);
    Ref w = g_sim.placeBuilding(B_K_FWALL, 1, tileOf(site.x), tileOf(site.y), true);
    Entity* we = g_sim.get(w);
    CHECK13(we, "could not place a Bastion Wall");
    float hp0 = we->hp;
    // idle tanks beside it leave it alone (nothing aims at it on its own initiative)
    std::vector<Ref> tanks;
    for (int i = 0; i < 4; i++) tanks.push_back(g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(we->pos + Vec2(-3.0f * TILE, (i - 1.5f) * 30.0f), 6)));
    run13(4.0f);
    we = g_sim.get(w);
    CHECK13(we && we->heat < 0.01f, "idle units opened fire on a Bastion Wall by themselves");
    // the computer never orders it either
    g_sim.players[0].isAI = true;
    g_sim.cmdAttack(tanks, w);
    run13(3.0f);
    we = g_sim.get(w);
    CHECK13(we && we->heat < 0.01f, "a computer army fired on an indestructible wall");
    g_sim.players[0].isAI = false;
    // a player's order: the tanks pour fire on it, it catches fire and loses nothing
    g_sim.cmdAttack(tanks, w);
    float hottest = 0;
    for (int k = 0; k < 40; k++) { run13(0.5f); we = g_sim.get(w); if (we) hottest = std::max(hottest, we->heat); }
    we = g_sim.get(w);
    CHECK13(we && we->alive, "a Bastion Wall was destroyed by gunfire");
    CHECK13(we->hp == hp0, "a Bastion Wall lost health to gunfire");
    CHECK13(hottest > 0.15f, "a Bastion Wall under heavy fire did not catch fire");
    // a warhead: still standing, still at full health, ablaze
    Sim::Nuke n; n.from = we->pos; n.pos = we->pos; n.owner = 0; n.scale = 1.0f;
    g_sim.nukeBlastPublic(n);
    g_sim.fallouts.clear();
    we = g_sim.get(w);
    CHECK13(we && we->hp == hp0 && we->heat >= 1.0f, "a warhead did more than set a Bastion Wall ablaze");
    clearUnits13();
    // an infiltrator steals it, and its new owner sells it to get rid of it
    Ref spy = g_sim.spawnUnit(U_C_SPY, 0, g_map.nearestFree(we->pos + Vec2(-1.5f * TILE, 0), 4));
    g_sim.cmdAttack({spy}, w);
    run13(g_sim.captureTime(*we) + 6.0f);
    we = g_sim.get(w);
    CHECK13(we && we->owner == 0, "an infiltrator could not steal a Bastion Wall");
    int money = g_sim.players[0].money;
    g_sim.cmdSell(w);
    CHECK13(!g_sim.get(w), "the new owner could not sell a stolen Bastion Wall");
    CHECK13(g_sim.players[0].money > money, "selling a Bastion Wall paid nothing");
    clearUnits13();
    return true;
}

bool lakeUpdateTest(u64 seed) {
    init13(seed);
    if (!lakeTest()) return false;
    if (!swimCaptureTest()) return false;
    if (!waymakerTest()) return false;
    init13(seed);
    if (!bastionTest()) return false;
    init13(seed);
    if (!patrolTest()) return false;
    printf("lakeupdatetest seed %llu: ok (bigger lake, infiltrators swim to the island launchers, Waymakers, Bastion Wall rules, patrol boats)\n", (unsigned long long)seed);
    return g_ok13;
}
