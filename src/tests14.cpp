// One Hour - headless tests for the super update (--superupdatetest)
//  * the map: 208x208 (40% more ground than 176x176), two Island Nuke Launchers on each island, every footprint on dry land
//  * the Super Nuke an island launcher fires: twice an Underground Nuke's blast radius, much faster, a 75 s reload; bunkers and interceptor
//    missiles never touch it, and a laser's chance is a fifth of its chance against an Underground Nuke
//  * a human's infiltrator takes over an ally's structure; a computer army's infiltrator never does
//  * the Healing Antenna: cheap, unlimited, fragile; it mends friendly units, aircraft and structures round it (antennas do not stack), never an
//    enemy's, and scrubs radiation more slowly than an Anti-Radiation Truck
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok14 = true;
static bool fail14(const char* m) { fprintf(stderr, "superupdatetest: %s\n", m); g_ok14 = false; return false; }
#define CHECK14(c, m) do { if (!(c)) return fail14(m); } while (0)

static void run14(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

static void clearUnits14() {
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

// players: 0 the human, 1 a computer army (an ally of the human when 'ally', else its enemy), 2 a computer enemy
static void init14(u64 seed, int n = 2, bool ally = false) {
    Faction fac[3] = { F_CYBER, F_CLANKER, F_CLANKER };
    bool ai[3] = { false, true, true };
    int diff[3] = { 1, 1, 1 };
    int team[3] = { 0, ally ? 0 : 1, 1 };
    g_sim.init(n, fac, ai, diff, team, seed);
    for (int p = 0; p < n; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    clearUnits14();
    g_sim.fallouts.clear(); g_sim.nukes.clear();
}

static std::vector<Entity*> ofRole14(BuildRole r) {
    std::vector<Entity*> out;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == r) out.push_back(&e);
    return out;
}

// a free site for 'type' near 'at' (a growing square search)
static bool siteNear14(int player, int type, Vec2 at, int& tx, int& ty) {
    int cx = tileOf(at.x), cy = tileOf(at.y);
    for (int r = 0; r < 20; r++) for (int y = cy - r; y <= cy + r; y++) for (int x = cx - r; x <= cx + r; x++) {
        if (std::max(std::abs(x - cx), std::abs(y - cy)) != r) continue;
        if (g_sim.canPlace(player, type, x, y)) { tx = x; ty = y; return true; }
    }
    return false;
}
static Ref placeNear14(int player, int type, Vec2 at) {
    int tx, ty;
    if (!siteNear14(player, type, at, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, player, tx, ty, true);
}

// ---- the map and its launchers
static bool mapTest14() {
    CHECK14(ISLE_W == 208 && MAP_W == 320 && MAP_H == 320, "the main island is not 208 tiles in a 320x320 sea");
    CHECK14(g_map.islands.size() == 2, "the lake does not have two islands");
    auto launchers = ofRole14(BR_ISLANDNUKE);
    CHECK14(launchers.size() == 4, "there are not four Island Nuke Launchers");
    int per[2] = { 0, 0 };
    for (auto* l : launchers) {
        CHECK14(l->owner < 0, "an island launcher starts owned");
        int k = dist(l->pos, Vec2(g_map.islands[0].cx * TILE, g_map.islands[0].cy * TILE)) < dist(l->pos, Vec2(g_map.islands[1].cx * TILE, g_map.islands[1].cy * TILE)) ? 0 : 1;
        CHECK14(dist(l->pos, Vec2(g_map.islands[k].cx * TILE, g_map.islands[k].cy * TILE)) < g_map.islands[k].r * TILE, "an island launcher is not on its island");
        per[k]++;
        for (int y = l->ty; y < l->ty + l->bt().h; y++) for (int x = l->tx; x < l->tx + l->bt().w; x++) CHECK14(g_map.tile(x, y) != T_WATER, "an island launcher stands in the water");
    }
    CHECK14(per[0] == 2 && per[1] == 2, "the islands do not hold two launchers each");
    return true;
}

// ---- the Super Nuke
static Entity* takeLauncher14(int player) {
    auto launchers = ofRole14(BR_ISLANDNUKE);
    Entity* l = launchers[0];
    l->owner = player; l->siloTimer = 0;
    return l;
}

static bool superNukeTest() {
    Entity* l = takeLauncher14(0);
    Ref L = g_sim.refOf(*l);
    CHECK14(g_sim.islandNukesReady(0) == 1, "a held, armed launcher is not ready");
    Vec2 target = g_sim.players[1].basePos;
    CHECK14(g_sim.cmdIslandNuke(0, target), "a ready island launcher would not fire");
    CHECK14(!g_sim.nukes.empty() && g_sim.nukes.back().super, "the island launcher did not fire a Super Nuke");
    const Sim::Nuke& n = g_sim.nukes.back();
    Sim::Nuke deep; deep.scale = UNUKE_SCALE; deep.deep = true;
    CHECK14(std::abs(Sim::nukeRadius(n) - 2.0f * Sim::nukeRadius(deep)) < 1.0f, "a Super Nuke's blast radius is not twice an Underground Nuke's");
    CHECK14(Sim::flightOf(n) < Sim::NUKE_FLIGHT * 0.5f, "a Super Nuke is not much faster than other warheads");
    CHECK14(std::abs(g_sim.islandNukeWait(0) - ISLAND_NUKE_COOLDOWN) < 0.2f, "an island launcher does not reload in its cooldown");
    l = g_sim.get(L);
    CHECK14(l, "the launcher vanished");
    run14(Sim::flightOf(n) + 0.2f);
    CHECK14(g_sim.nukes.empty(), "a Super Nuke was still in the air after its flight time");
    g_sim.fallouts.clear();
    run14(ISLAND_NUKE_COOLDOWN);
    CHECK14(g_sim.islandNukesReady(0) == 1, "an island launcher was not ready again a cooldown after a launch");
    return true;
}

// a tank inside an Underground Nuke's reach-and-a-half dies, one past the Super Nuke's rim lives
static bool superBlastTest() {
    clearUnits14();
    Vec2 gz(WORLD_W * 0.5f, WORLD_H * 0.5f);
    Vec2 dir(0.6f, 0.8f);
    Ref nearT = g_sim.spawnUnit(U_K_TANK, 1, gz + dir * (NUKE_RADIUS * 4.5f * TILE));    // past an Underground Nuke's rim (3x), inside the Super Nuke's
    Ref farT = g_sim.spawnUnit(U_K_TANK, 1, gz - dir * (NUKE_RADIUS * 6.6f * TILE));     // past the Super Nuke's rim (6x)
    CHECK14(g_sim.get(nearT) && g_sim.get(farT), "could not set up the blast scene");
    for (Ref r : { nearT, farT }) g_sim.get(r)->disabledUntil = g_sim.time + 100;
    run14(0.1f);   // (the spatial grid the blast searches is rebuilt by a step)
    Sim::Nuke n; n.from = gz; n.pos = gz; n.owner = 0; n.scale = ISLAND_NUKE_SCALE; n.super = true;
    g_sim.nukeBlastPublic(n);
    g_sim.fallouts.clear();
    CHECK14(!g_sim.get(nearT), "a tank four and a half ramp radii out survived a Super Nuke");
    CHECK14(g_sim.get(farT) && g_sim.get(farT)->hp == g_sim.get(farT)->maxHp, "a Super Nuke hurt a tank past its rim");
    clearUnits14();
    return true;
}

// interception: only lasers, at a fifth of their chance against an Underground Nuke; bunkers and interceptor missiles never engage
static bool interceptTest() {
    // the human (Cyber) defends with a laser; a computer enemy fires
    Vec2 spot = g_sim.players[0].basePos;
    for (int k = 0; k < 2; k++) placeNear14(0, B_C_POWER, spot + Vec2(-6 * TILE, 4 * TILE));   // (a defence without power is blind)
    Ref laser = placeNear14(0, B_C_ANTINUKE, spot + Vec2(4 * TILE, 0));
    CHECK14(laser.valid(), "could not place an Anti-Nuke Laser");
    run14(12.0f);   // (finished and charged)
    float deep = g_sim.antiNukeChance(spot, 1, false, UNUKE_SCALE, true);
    float sup = g_sim.antiNukeChance(spot, 1, false, ISLAND_NUKE_SCALE, true, true);
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "laser chance: underground %.3f super %.3f\n", deep, sup);
    CHECK14(deep > 0.05f && sup > 0.0f, "the laser does not cover its own base");
    CHECK14(std::abs(sup * 5.0f - deep) < 0.01f, "a laser's chance against a Super Nuke is not a fifth of its chance against an Underground Nuke");
    // a Clanker army's missiles and bunker never engage it
    init14(1);
    Vec2 k = g_sim.players[1].basePos;
    for (int q = 0; q < 2; q++) placeNear14(1, B_K_POWER, k + Vec2(-6 * TILE, 4 * TILE));
    Ref miss = placeNear14(1, B_K_ANTINUKE, k + Vec2(4 * TILE, 0));
    Ref bunk = placeNear14(1, B_K_BUNKER, k + Vec2(-4 * TILE, 0));
    CHECK14(miss.valid() && bunk.valid(), "could not place the Clanker defences");
    run14(12.0f);
    CHECK14(g_sim.antiNukeChance(k, 0, false, ISLAND_NUKE_SCALE, true, true) == 0.0f, "interceptor missiles count against a Super Nuke");
    Entity* l = takeLauncher14(0);
    (void)l;
    CHECK14(g_sim.cmdIslandNuke(0, k), "the launcher would not fire at the Clanker base");
    int id = 0;
    for (int t = 0; t < (int)(ISLAND_NUKE_FLIGHT * SIM_HZ) - 2; t++) {
        g_sim.step(); g_sim.events.clear();
        if (!g_sim.nukes.empty()) { id = (int)g_sim.nukes.back().engagedBy.size(); CHECK14(g_sim.nukes.back().killT < 0, "a Clanker defence shot a Super Nuke down"); }
    }
    CHECK14(id == 0, "a bunker or interceptor missile engaged a Super Nuke");
    run14(1.0f);
    g_sim.fallouts.clear();
    return true;
}

// ---- infiltrators and an ally's structures
static bool allyCaptureTest(u64 seed) {
    init14(seed, 3, true);   // 0 human + 1 computer ally against 2
    CHECK14(!g_sim.enemies(0, 1) && g_sim.enemies(0, 2), "the teams are not as set up");
    Vec2 ab = g_sim.players[1].basePos;
    Ref bar = placeNear14(1, B_K_BARRACKS, ab + Vec2(6 * TILE, 4 * TILE));
    CHECK14(bar.valid(), "could not place the ally's barracks");
    Entity* b = g_sim.get(bar);
    CHECK14(g_sim.spyMayTake(0, *b), "a human's infiltrator may not take an ally's structure");
    Ref spy = g_sim.spawnUnit(U_C_SPY, 0, g_map.nearestFree(b->pos + Vec2(0, (b->bt().h * 0.5f + 1.0f) * TILE), 6));
    g_sim.cmdAttack({spy}, bar);
    Entity* s = g_sim.get(spy);
    CHECK14(s && s->order == O_CAPTURE, "ordering a human's infiltrator onto an ally's structure is not a capture");
    run14(g_sim.captureTime(*b) + 6.0f);
    b = g_sim.get(bar);
    CHECK14(b && b->owner == 0, "a human's infiltrator did not take over the ally's barracks");
    // a computer army's infiltrator never takes an ally's structure (the human's, or another computer ally's)
    Ref hb = placeNear14(0, B_C_BARRACKS, g_sim.players[0].basePos + Vec2(6 * TILE, 4 * TILE));
    CHECK14(hb.valid(), "could not place the human's barracks");
    Entity* h = g_sim.get(hb);
    CHECK14(!g_sim.spyMayTake(1, *h), "a computer army's infiltrator may take its human ally's structure");
    Ref aspy = g_sim.spawnUnit(U_K_SPY, 1, g_map.nearestFree(h->pos + Vec2(0, (h->bt().h * 0.5f + 1.0f) * TILE), 6));
    g_sim.cmdCapture({aspy}, hb);
    g_sim.cmdAttack({aspy}, hb);
    Entity* as = g_sim.get(aspy);
    CHECK14(as && as->order != O_CAPTURE, "a computer army's infiltrator was given a capture order on its ally's structure");
    run14(g_sim.captureTime(*h) + 4.0f);
    h = g_sim.get(hb);
    CHECK14(h && h->owner == 0, "a computer army's infiltrator took its human ally's structure");
    clearUnits14();
    return true;
}

// ---- the Healing Antenna
static bool antennaTest(u64 seed) {
    init14(seed, 3, true);
    for (int t : { B_C_HEALANT, B_K_HEALANT }) {
        const BuildType& bt = BUILDS[t];
        CHECK14(bt.role == BR_HEALANT && bt.w == 1 && bt.h == 1, "the Healing Antenna is not a 1x1 structure");
        CHECK14(bt.cost <= 300, "the Healing Antenna is not cheap");
        CHECK14(bt.hp <= 1000, "the Healing Antenna is not fragile");
    }
    CHECK14(g_sim.buildAvailable(0, B_C_HEALANT) && g_sim.buildAvailable(1, B_K_HEALANT), "the Healing Antenna is not in both armies' build menus");
    Vec2 c = g_sim.players[0].basePos + Vec2(0, 8 * TILE);
    Ref mast = placeNear14(0, B_C_HEALANT, c);
    CHECK14(mast.valid(), "could not place a Healing Antenna");
    // no limit: many more of them still leave it available
    for (int k = 0; k < 24; k++) placeNear14(0, B_C_HEALANT, g_sim.players[0].basePos + Vec2(-10 * TILE, -6 * TILE));
    CHECK14((int)ofRole14(BR_HEALANT).size() >= 20 && g_sim.buildAvailable(0, B_C_HEALANT), "the Healing Antenna has a limit");
    Entity* m = g_sim.get(mast);
    Vec2 at = m->pos;
    // a damaged friendly tank, an ally's soldier-carrying jeep, a helicopter overhead, a structure: all mended; an enemy tank is not
    Ref tank = g_sim.spawnUnit(U_C_TANK, 0, g_map.nearestFree(at + Vec2(2 * TILE, 0), 4));
    Ref ally = g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(at + Vec2(-2 * TILE, 0), 4));
    Ref foe = g_sim.spawnUnit(U_K_TANK, 2, g_map.nearestFree(at + Vec2(0, 2 * TILE), 4));
    Ref heli = g_sim.spawnUnit(U_C_HELI, 0, at + Vec2(0, -TILE));
    Ref bld = placeNear14(0, B_C_POWER, at + Vec2(0, -3 * TILE));
    CHECK14(tank.valid() && ally.valid() && foe.valid() && heli.valid() && bld.valid(), "could not set up the healing scene");
    auto setHalf = [](Ref r) { Entity* e = g_sim.get(r); e->hp = e->maxHp * 0.5f; e->disabledUntil = g_sim.time + 100; };   // (stunned: nobody drives or flies off, nobody shoots)
    for (Ref r : { tank, ally, foe, heli, bld }) setHalf(r);
    g_sim.get(bld)->disabledUntil = 0;
    run14(4.0f);
    auto frac = [](Ref r) { Entity* e = g_sim.get(r); return e ? e->hp / e->maxHp : -1.0f; };
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "after 4 s: tank %.3f ally %.3f foe %.3f heli %.3f structure %.3f\n", frac(tank), frac(ally), frac(foe), frac(heli), frac(bld));
    CHECK14(frac(tank) > 0.58f && frac(ally) > 0.58f && frac(heli) > 0.58f, "the antenna did not mend friendly and allied units and aircraft");
    CHECK14(frac(bld) > 0.52f, "the antenna did not mend a structure");
    CHECK14(std::abs(frac(foe) - 0.5f) < 0.001f, "the antenna mended an enemy");
    // antennas do not stack: a second one next to the first changes nothing
    float before = frac(tank);
    Ref mast2 = placeNear14(0, B_C_HEALANT, at + Vec2(2 * TILE, TILE));
    CHECK14(mast2.valid(), "could not place a second antenna");
    run14(4.0f);
    float gain2 = frac(tank) - before;
    CHECK14(gain2 > 0.0f && gain2 < HEALANT_RATE * 4.0f * 1.2f, "two antennas over one tank mended it faster than one");
    // fragile: a tank's fire brings one down quickly
    Ref shooter = g_sim.spawnUnit(U_K_TANK, 2, g_map.nearestFree(at + Vec2(-4 * TILE, 3 * TILE), 6));
    Entity* m2 = g_sim.get(mast2);
    g_sim.applyDamagePublic(*m2, m2->maxHp * 0.6f, 2, shooter);
    g_sim.applyDamagePublic(*m2, m2->maxHp * 0.6f, 2, shooter);
    CHECK14(!g_sim.get(mast2), "a Healing Antenna did not fall to ordinary damage");
    clearUnits14();
    // radiation: a zone it reaches shrinks, but more slowly than under one Anti-Radiation Truck (even with a crowd of antennas)
    std::vector<Ref> masts;
    for (auto* e : ofRole14(BR_HEALANT)) masts.push_back(g_sim.refOf(*e));
    Vec2 z = g_sim.get(mast)->pos;
    g_sim.fallouts.clear();
    g_sim.fallouts.push_back({z, 6.0f * TILE, 0.0f, 0.0f, 1.0f, -1, false, 300.0f, 0});
    run14(10.0f);
    CHECK14(!g_sim.fallouts.empty(), "the antennas wiped a fallout zone out at once");
    float antPower = g_sim.fallouts[0].power;
    CHECK14(antPower < 0.85f, "Healing Antennas did not scrub radiation");
    for (Ref r : masts) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    g_sim.fallouts.clear();
    g_sim.fallouts.push_back({z, 6.0f * TILE, 0.0f, 0.0f, 1.0f, -1, false, 300.0f, 0});
    Ref truck = g_sim.spawnUnit(U_C_RADTRUCK, 0, g_map.nearestFree(z, 4));
    CHECK14(truck.valid(), "could not spawn an Anti-Radiation Truck");
    run14(10.0f);
    float truckPower = g_sim.fallouts.empty() ? 0.0f : g_sim.fallouts[0].power;
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "fallout power after 10 s: %zu antennas %.3f, one truck %.3f\n", masts.size(), antPower, truckPower);
    CHECK14(truckPower < antPower, "a crowd of Healing Antennas scrubbed radiation faster than one Anti-Radiation Truck");
    g_sim.fallouts.clear();
    clearUnits14();
    return true;
}

bool superUpdateTest(u64 seed) {
    init14(seed);
    if (!mapTest14()) return false;
    if (!superNukeTest()) return false;
    init14(seed);
    if (!superBlastTest()) return false;
    init14(seed);
    if (!interceptTest()) return false;
    if (!allyCaptureTest(seed)) return false;
    if (!antennaTest(seed)) return false;
    printf("superupdatetest seed %llu: ok (208 map, four launchers, Super Nukes, ally takeovers by the human only, Healing Antennas)\n", (unsigned long long)seed);
    return g_ok14;
}
