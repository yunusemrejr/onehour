// One Hour - headless tests for the October update (--updatetest)
//  * the bigger map: a bigger lake, two islands (one in the middle, one off the east shore) with trees and an Island Nuke Launcher each, and a
//    Mountain Fortress set into every ridge that both neighbouring armies can walk up to
//  * the launchers and Bastion Walls are indestructible: hits, even warheads, only make them burn; only an infiltrator takes them
//  * the fortress holds 100 infantry, needs several warheads, and snipers in it find infiltrators
//  * infiltrators are stealthy: only snipers, drones, aircraft (not heavy bombers), jeeps, infiltrators and light turrets see them
//  * trees and small rocks never block a site: they are cleared under it; the ridges still do
//  * dozers walk to the near side of a site; nuclear bombers guard the ground only; fighters guard air and ground
//  * the Nuclear Bomber call-in: from the corner, one mid-sized warhead, gone; strong anti-air brings it down first
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok12 = true;
static bool fail12(const char* m) { fprintf(stderr, "updatetest: %s\n", m); g_ok12 = false; return false; }
#define CHECK12(c, m) do { if (!(c)) return fail12(m); } while (0)

static void run12(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

static void clearUnits12() {
    std::vector<Ref> victims;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) victims.push_back(g_sim.refOf(e));
    for (Ref r : victims) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
}

static void init12(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { false, false };
    int diff[2] = { 1, 1 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 900000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    clearUnits12();
}

static std::vector<Entity*> ofRole(BuildRole r) {
    std::vector<Entity*> out;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == r) out.push_back(&e);
    return out;
}

// ---- the map
static bool mapTest() {
    CHECK12(MAP_W == 146 && MAP_H == 146, "the map is not 146x146 (about 30% more ground than 128x128)");
    CHECK12(g_map.islands.size() == 2, "the lake does not have two islands");
    int lake = g_map.waterSize[g_map.lakeComp];
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "lake: %d water tiles\n", lake);
    CHECK12(lake > 2000, "the central lake did not grow (the 128x128 map had 1561 tiles of it)");
    const auto& big = g_map.islands[0];
    const auto& small = g_map.islands[1];
    CHECK12(big.r > small.r, "the middle island is not the bigger one");
    CHECK12(std::abs(big.cx - MAP_W / 2) < 2 && std::abs(big.cy - MAP_H / 2) < 2, "the big island is not in the middle of the lake");
    // the small one is out in the water: water between it and the nearest shore, several tiles of it
    {
        int sx = (int)small.cx, sy = (int)small.cy, water = 0;
        for (int x = sx + (int)small.r + 1; x < MAP_W; x++) { if (g_map.tile(x, sy) == T_WATER) water++; else break; }
        CHECK12(water >= 4, "the small island is a wade from the shore");
    }
    for (auto& isl : g_map.islands) {
        int trees = 0, cx = (int)isl.cx, cy = (int)isl.cy, R = (int)isl.r + 1;
        for (int y = cy - R; y <= cy + R; y++) for (int x = cx - R; x <= cx + R; x++) if (inMap(x, y) && g_map.tile(x, y) == T_TREE) trees++;
        CHECK12(trees >= 2, "an island has no trees");
    }
    auto launchers = ofRole(BR_ISLANDNUKE);
    CHECK12(launchers.size() == 2, "there are not exactly two Island Nuke Launchers");
    for (auto* l : launchers) {
        CHECK12(l->owner < 0, "an island launcher starts owned");
        bool onIsland = false;
        for (auto& isl : g_map.islands) if (dist(l->pos, Vec2(isl.cx * TILE, isl.cy * TILE)) < isl.r * TILE) onIsland = true;
        CHECK12(onIsland, "an island launcher is not on an island");
    }
    int forts = 0;
    for (auto& e : g_sim.ents) {
        if (!e.alive || e.type != B_C_FORTRESS) continue;
        forts++;
        CHECK12(e.owner < 0 && e.bt().garrison == 100, "a Mountain Fortress starts owned or holds other than 100");
        // rock on its ridge ends, doorsteps on the two open faces, each reachable from its own side of the ridge
        int rock = 0;
        for (int y = e.ty - 3; y < e.ty + e.bt().h + 3; y++) for (int x = e.tx - 3; x < e.tx + e.bt().w + 3; x++) if (inMap(x, y) && g_map.tile(x, y) == T_ROCK) rock++;
        CHECK12(rock >= 10, "a Mountain Fortress is not set into a ridge");
        int sides = 0;
        Vec2 probes[4] = { e.pos + Vec2(-(e.bt().w * 0.5f + 1.2f) * TILE, 0), e.pos + Vec2((e.bt().w * 0.5f + 1.2f) * TILE, 0),
                           e.pos + Vec2(0, -(e.bt().h * 0.5f + 1.2f) * TILE), e.pos + Vec2(0, (e.bt().h * 0.5f + 1.2f) * TILE) };
        for (auto& p : probes) if (g_map.passable(tileOf(p.x), tileOf(p.y))) sides++;
        CHECK12(sides >= 2, "a Mountain Fortress cannot be walked up to from both sides of its ridge");
    }
    CHECK12(forts == 4, "there are not four Mountain Fortresses");
    CHECK12(!g_sim.buildAvailable(0, B_C_ISLANDNUKE) && !g_sim.buildAvailable(0, B_C_FORTRESS), "a map structure is offered in the build menu");
    CHECK12(g_sim.buildAvailable(0, B_C_FWALL) && g_sim.buildAvailable(1, B_K_FWALL), "the Bastion Wall is not in the build menu");
    CHECK12(BUILDS[B_C_FWALL].cost == BUILDS[B_C_WALL].cost, "the Bastion Wall does not cost what a wall costs");
    return true;
}

// ---- indestructible structures, capture, and the island launcher's missile
static bool launcherTest() {
    auto launchers = ofRole(BR_ISLANDNUKE);
    Ref L = g_sim.refOf(*launchers[0]);
    Entity* l = g_sim.get(L);
    float hp0 = l->hp;
    g_sim.applyDamagePublic(*l, 50000, 1, NOREF);
    l = g_sim.get(L);
    CHECK12(l && l->hp == hp0, "an island launcher lost health");
    CHECK12(l->heat > 0, "an island launcher hit hard does not burn");
    Sim::Nuke n; n.from = l->pos; n.pos = l->pos; n.owner = 1; n.scale = UNUKE_SCALE; n.deep = true;
    g_sim.nukeBlastPublic(n);
    l = g_sim.get(L);
    CHECK12(l && l->hp == hp0 && l->heat >= 1.0f, "a warhead on an island launcher did more than set it ablaze");
    g_sim.fallouts.clear(); g_sim.nukes.clear();   // (the scene's warhead left radiation: the spies below would flee it)
    Ref tank = g_sim.spawnUnit(U_K_TANK, 1, g_map.nearestFree(l->pos + Vec2(0, 140), 12));
    CHECK12(!g_sim.canTarget(*g_sim.get(tank), *l), "a tank may shoot at an indestructible launcher");
    // an infiltrator on the island takes it
    Vec2 shore = g_map.nearestFree(l->pos + Vec2(0, (l->bt().h * 0.5f + 1.0f) * TILE), 4);
    Ref spy = g_sim.spawnUnit(U_C_SPY, 0, shore);
    g_sim.cmdCapture({spy}, L);
    run12(g_sim.captureTime(*l) + 4.0f);
    l = g_sim.get(L);
    CHECK12(l && l->owner == 0, "an infiltrator on the island did not take the neutral launcher");
    CHECK12(g_sim.islandNukesReady(0) == 0, "a launcher fires the moment it is taken");
    run12(ISLAND_NUKE_ARM + 1.0f);
    CHECK12(g_sim.islandNukesReady(0) == 1, "a held launcher never arms");
    size_t before = g_sim.nukes.size();
    Vec2 target = g_sim.players[1].basePos + Vec2(0, 300);
    CHECK12(g_sim.cmdIslandNuke(0, target), "a ready island launcher would not fire");
    CHECK12(g_sim.nukes.size() == before + 1 && std::abs(g_sim.nukes.back().scale - 2 * MINI_NUKE_SCALE) < 1e-4f, "the island nuke is not one warhead with two bunker missiles' blast");
    float wait = g_sim.islandNukeWait(0);
    CHECK12(std::abs(wait - MINI_NUKE_COOLDOWN) < 0.2f, "the island launcher's cooldown is not a bunker's");
    // burning changes nothing: it keeps its clock and is not switched off
    l = g_sim.get(L); l->heat = HEAT_MAX; l->disabledUntil = g_sim.time + 100;
    run12(1.0f);
    l = g_sim.get(L);
    CHECK12(l->disabledUntil <= g_sim.time, "a burning launcher is switched off");
    // the enemy takes it back
    Ref spy2 = g_sim.spawnUnit(U_K_SPY, 1, shore + Vec2(10, 0));
    g_sim.cmdCapture({spy2}, L);
    run12(g_sim.captureTime(*l) + 4.0f);
    l = g_sim.get(L);
    CHECK12(l && l->owner == 1, "an enemy infiltrator could not take the launcher back");
    // Bastion Wall: the same rules, and no guns
    Vec2 site = openGroundNear(g_sim.players[0].basePos + Vec2(320, 320), 4);
    Ref w = g_sim.placeBuilding(B_C_FWALL, 0, tileOf(site.x), tileOf(site.y), true);
    Entity* we = g_sim.get(w);
    g_sim.players[0].upg[UPG_GUNS] = true; g_sim.players[0].upg[UPG_ANTIROCKET] = true;
    CHECK12(g_sim.roofGun(*we) < 0 && !g_sim.isAntiRocketWall(*we), "a Bastion Wall mounts a gun or a point-defence laser");
    float whp = we->hp;
    g_sim.applyDamagePublic(*we, 1e6f, 1, NOREF);
    Sim::Nuke n2; n2.from = we->pos; n2.pos = we->pos; n2.owner = 1; n2.scale = 1.0f;
    g_sim.nukeBlastPublic(n2);
    g_sim.fallouts.clear();
    we = g_sim.get(w);
    CHECK12(we && we->hp == whp && we->heat > 0.5f, "a Bastion Wall was damaged instead of burning");
    Ref spy3 = g_sim.spawnUnit(U_K_SPY, 1, g_map.nearestFree(we->pos + Vec2(40, 0), 4));
    g_sim.cmdCapture({spy3}, w);
    run12(g_sim.captureTime(*we) + 6.0f);
    we = g_sim.get(w);
    CHECK12(we && we->owner == 1, "an enemy infiltrator could not take a Bastion Wall");
    g_sim.players[0].upg[UPG_GUNS] = false; g_sim.players[0].upg[UPG_ANTIROCKET] = false;
    clearUnits12();
    return true;
}

// ---- the fortress: 100 infantry, several warheads, and snipers that see infiltrators
static bool fortressTest() {
    Entity* f = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.type == B_C_FORTRESS) { f = &e; break; }
    Ref F = g_sim.refOf(*f);
    // fill it: one sniper first, then riflemen
    Vec2 door = g_map.nearestFree(f->pos + Vec2(-(f->bt().w * 0.5f + 1.0f) * TILE, 0), 3);
    if (!g_map.passable(tileOf(door.x), tileOf(door.y)) || dist(door, f->pos) > 5 * TILE) door = g_map.nearestFree(f->pos + Vec2((f->bt().w * 0.5f + 1.0f) * TILE, 0), 3);
    Ref sn = g_sim.spawnUnit(U_C_SNIPER, 0, door);
    g_sim.boardQuiet(*g_sim.get(sn), *g_sim.get(F));
    f = g_sim.get(F);
    CHECK12(f->owner < 0 || f->owner == 0, "a sniper boarding the fortress handed it to someone else");
    for (int i = 0; i < 120; i++) { Ref r = g_sim.spawnUnit(U_C_INF1, 0, door); Entity* u = g_sim.get(r); f = g_sim.get(F); if (g_sim.canBoard(*u, *f)) g_sim.boardQuiet(*u, *f); else g_sim.destroy(*u, false); }
    f = g_sim.get(F);
    CHECK12(g_sim.garrisonInfantry(*f) == 100, "the fortress does not hold exactly 100 infantry");
    // an enemy infiltrator creeping up is seen by the sniper inside and shot
    Ref infil = g_sim.spawnUnit(U_K_SPY, 1, g_map.nearestFree(f->pos + Vec2(0, 0) + (door - f->pos) * 2.2f, 6));
    run12(0.3f);
    Entity* ie = g_sim.get(infil);
    CHECK12(!ie || g_sim.visibleTo(*ie, 0), "a sniper in the fortress does not see an infiltrator at its door");   // (seen, it is usually shot at once)
    g_sim.cmdCapture({infil}, F);
    run12(8.0f);
    f = g_sim.get(F);
    CHECK12(!g_sim.get(infil) || f->owner == 0, "an infiltrator walked into a sniper-held fortress unharmed");
    // several warheads
    f = g_sim.get(F); float full = f->maxHp;
    for (int k = 0; k < 3; k++) { Sim::Nuke n; n.from = f->pos; n.pos = f->pos; n.owner = 1; n.scale = 1.0f; g_sim.nukeBlastPublic(n); f = g_sim.get(F); CHECK12(f != nullptr, "the fortress fell to three warheads"); }
    CHECK12(f->hp <= full * (1.0f - 3 * FORTRESS_NUKE) + 1.0f + full * 0.01f || f->hp < full, "a warhead did no harm to the fortress");
    clearUnits12();
    return true;
}

// ---- stealth of infiltrators
static bool stealthTest12() {
    Vec2 c = openGroundNear(g_sim.players[0].basePos + Vec2(600, 500), 10);
    Ref infil = g_sim.spawnUnit(U_K_SPY, 1, c);
    { Entity* ie = g_sim.get(infil); ie->maxHp = ie->hp = 1e7f; }   // (whoever sees it shoots it: keep it standing for the next probe)
    auto seenAfter = [&](int type, Vec2 at) {
        Ref d = g_sim.spawnUnit(type, 0, at);
        Entity* de = g_sim.get(d); de->order = O_IDLE;
        run12(0.4f);
        Entity* ie = g_sim.get(infil);
        bool seen = ie && g_sim.visibleTo(*ie, 0);
        if (Entity* e = g_sim.get(d)) g_sim.destroy(*e, false);
        run12(3.0f);   // (the sighting lingers a moment)
        return seen;
    };
    CHECK12(!seenAfter(U_C_TANK, c + Vec2(70, 0)), "a tank spotted an infiltrator");
    CHECK12(!seenAfter(U_C_INF1, c + Vec2(60, 0)), "a rifleman spotted an infiltrator");
    CHECK12(seenAfter(U_C_SNIPER, c + Vec2(150, 0)), "a sniper did not spot an infiltrator");
    CHECK12(seenAfter(U_C_JEEP, c + Vec2(100, 0)), "a jeep did not spot an infiltrator");
    CHECK12(seenAfter(U_C_SPY, c + Vec2(100, 0)), "an infiltrator did not spot another");
    CHECK12(seenAfter(U_C_SDRONE, c + Vec2(150, 0)), "a drone did not spot an infiltrator");
    CHECK12(seenAfter(U_C_FIGHTER, c + Vec2(100, 0)), "a fighter did not spot an infiltrator");
    CHECK12(!seenAfter(U_C_SBOMBER, c + Vec2(60, 0)), "a heavy bomber spotted an infiltrator");
    Ref tur = g_sim.placeBuilding(B_C_LASER, 0, tileOf(c.x) + 4, tileOf(c.y), true);
    run12(0.4f);
    CHECK12(g_sim.visibleTo(*g_sim.get(infil), 0), "a Laser Turret did not spot an infiltrator");
    g_sim.destroy(*g_sim.get(tur), false);
    run12(3.0f);
    Ref pat = g_sim.placeBuilding(B_C_PATRIOT, 0, tileOf(c.x) + 4, tileOf(c.y), true);
    run12(0.4f);
    CHECK12(!g_sim.visibleTo(*g_sim.get(infil), 0), "a Patriot battery spotted an infiltrator");
    g_sim.destroy(*g_sim.get(pat), false);
    clearUnits12();
    return true;
}

// ---- vegetation never blocks a site; ridges do
static bool vegetationTest() {
    int tx = -1, ty = -1;
    for (int y = 4; y < MAP_H - 6 && tx < 0; y++) for (int x = 4; x < MAP_W - 6 && tx < 0; x++) {
        int trees = 0; bool bad = false;
        for (int j = 0; j < 3; j++) for (int i = 0; i < 3; i++) { if (g_map.tile(x + i, y + j) == T_TREE) trees++; else if (!g_map.buildable(x + i, y + j)) bad = true; }
        if (trees >= 4 && !bad && g_sim.canPlace(0, B_C_BARRACKS, x, y)) { tx = x; ty = y; }
    }
    CHECK12(tx >= 0, "no stand of trees anywhere takes a structure");
    Ref b = g_sim.placeBuilding(B_C_BARRACKS, 0, tx, ty, false);
    CHECK12(b.valid(), "a structure could not be placed over trees");
    for (int j = 0; j < BUILDS[B_C_BARRACKS].h; j++) for (int i = 0; i < BUILDS[B_C_BARRACKS].w; i++) CHECK12(g_map.tile(tx + i, ty + j) != T_TREE, "a tree under a new structure was not cleared");
    CHECK12(!g_map.cleared.empty(), "clearing the trees left nothing for the renderer to repaint");
    g_map.cleared.clear();
    // a ridge tile is still a hard obstacle
    int rx = -1, ry = -1;
    for (int y = 2; y < MAP_H - 2 && rx < 0; y++) for (int x = 2; x < MAP_W - 2 && rx < 0; x++) if (g_map.tile(x, y) == T_ROCK && !g_map.clearable[y * MAP_W + x]) { rx = x; ry = y; }
    CHECK12(rx >= 0 && !g_map.buildable(rx, ry), "a ridge tile is buildable");
    return true;
}

// ---- dozers go to the near side of a site
static bool dozerSideTest() {
    Vec2 c = openGroundNear(g_sim.players[0].basePos + Vec2(-200, 520), 9);
    int tx = tileOf(c.x) - 1, ty = tileOf(c.y) - 1;
    CHECK12(g_sim.canPlace(0, B_C_POWER, tx, ty), "no room for the dozer scene");
    Ref dz = g_sim.spawnUnit(U_C_DOZER, 0, tileCenter(tx + 1, ty - 4));   // north of the site
    CHECK12(g_sim.cmdBuild(dz, B_C_POWER, tx, ty), "the dozer would not take the site");
    run12(0.2f);
    Entity* d = g_sim.get(dz);
    CHECK12(d && !d->path.empty(), "the dozer has no route to its site");
    Vec2 end = d->path.back();
    CHECK12(end.y < c.y, "the dozer drives round to the far side of a site that is right in front of it");
    run12(4.0f);
    d = g_sim.get(dz);
    CHECK12(d && d->pos.y < c.y, "the dozer went round the site instead of working its near face");
    clearUnits12();
    return true;
}

// ---- bombers guard the ground, fighters guard both; the Nuclear Bomber call-in
static bool airTest12() {
    Vec2 c = openGroundNear(Vec2(WORLD_W * 0.5f, 22.0f * TILE), 8);
    Ref sb = g_sim.spawnUnit(U_C_SBOMBER, 0, c); Ref fi = g_sim.spawnUnit(U_C_FIGHTER, 0, c + Vec2(30, 0));
    for (Ref r : { sb, fi }) { Entity* e = g_sim.get(r); e->alt = 1; e->airspeed = e->ut().speed; e->ammo = e->ut().ammo; }
    g_sim.cmdGuardArea({sb, fi}, c, 8 * TILE);
    Ref heli = g_sim.spawnUnit(U_K_AIR, 1, c + Vec2(90, 40));
    Ref truck = g_sim.spawnUnit(U_K_HARV, 1, g_map.nearestFree(c + Vec2(-120, 60), 6));
    run12(0.6f);
    Entity* bomber = g_sim.get(sb);
    Entity* fighter = g_sim.get(fi);
    CHECK12(bomber && !(bomber->targetEnt == heli), "a nuclear bomber on guard went after an aircraft");
    CHECK12(fighter && (fighter->targetEnt == heli || fighter->targetEnt == truck || fighter->engaged.valid()), "a fighter on guard engaged nothing");
    (void)truck;
    clearUnits12();
    // the call-in: not before its opening time, then from a corner, one warhead, off the map
    CHECK12(!g_sim.cmdNukeBomber(0, g_sim.players[1].basePos), "the Nuclear Bomber can be called before it opens");
    bool tech = false;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_TECH) tech = true;
    if (!tech) { Vec2 s = openGroundNear(g_sim.players[0].basePos + Vec2(0, 260), 3); g_sim.placeBuilding(B_C_TECH, 0, tileOf(s.x) - 1, tileOf(s.y) - 1, true); }
    while (g_sim.time < NUKEBOMBER_OPEN + 1.0f) run12(10.0f);
    Vec2 target = g_sim.players[1].basePos + Vec2(0, 260);
    g_sim.fallouts.clear();
    size_t before = 0;   // (the bomb's warhead goes off within the tick it lands: watch for its fallout)
    CHECK12(g_sim.bombersReady(0) >= 1 && g_sim.cmdNukeBomber(0, target), "a ready Nuclear Bomber call failed");
    Entity* nb = nullptr;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == U_C_NUKEBOMBER) nb = &e;
    CHECK12(nb && nb->ut().temp, "no strategic bomber came");
    float edge = std::min(std::min(nb->pos.x, WORLD_W - nb->pos.x), std::min(nb->pos.y, WORLD_H - nb->pos.y));
    CHECK12(edge < 3 * TILE, "the bomber did not come in from the map corner");
    bool dropped = false; float tEnd = g_sim.time + NUKEBOMBER_LIFE;
    while (g_sim.time < tEnd) {
        g_sim.step(); g_sim.events.clear();
        if (g_sim.fallouts.size() > before) { dropped = true; }
        bool still = false; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == U_C_NUKEBOMBER) still = true;
        if (dropped && !still) break;
    }
    CHECK12(dropped, "the bomber never dropped its bomb");
    bool gone = true; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == U_C_NUKEBOMBER) gone = false;
    CHECK12(gone, "the bomber did not fly off the map after the drop");
    CHECK12(g_sim.bombersReady(0) == 0, "a second Nuclear Bomber is ready at once");
    // light anti-air does not stop it; strong anti-air on the route brings it down before the drop
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_TECH) e.bomberTimer = 0;
    Vec2 from = g_sim.bomberEntry(0, target);
    Vec2 dir = (target - from).norm();
    for (int k = 0; k < 4; k++) { Vec2 at = openGroundNear(g_sim.players[1].basePos + Vec2(-300 + k * 110.0f, -260), 2); g_sim.placeBuilding(B_K_POWER, 1, tileOf(at.x) - 1, tileOf(at.y), true); }   // (the batteries need power)
    std::vector<Ref> light;
    for (int k = 0; k < 2; k++) { Vec2 at = from + dir * (1100.0f + k * 90.0f); light.push_back(g_sim.placeBuilding(B_K_ROCKET, 1, tileOf(at.x), tileOf(at.y), true)); }
    g_sim.updatePowerPublic();
    g_sim.fallouts.clear();
    CHECK12(g_sim.cmdNukeBomber(0, target), "the light-AA call failed");
    run12(NUKEBOMBER_LIFE);
    CHECK12(!g_sim.fallouts.empty(), "two rocket batteries were enough to stop a Nuclear Bomber");
    for (Ref r : light) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    clearUnits12(); g_sim.fallouts.clear(); g_sim.nukes.clear();
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.bt().role == BR_TECH) e.bomberTimer = 0;
    for (int k = 0; k < 9; k++) {
        Vec2 at = from + dir * (900.0f + k * 70.0f) + Vec2(-dir.y, dir.x) * ((k % 3 - 1) * 70.0f);
        g_sim.placeBuilding(B_K_ROCKET, 1, tileOf(at.x), tileOf(at.y), true);
    }
    g_sim.updatePowerPublic();
    g_sim.fallouts.clear();
    CHECK12(g_sim.cmdNukeBomber(0, target), "the second call failed");
    run12(NUKEBOMBER_LIFE);
    CHECK12(g_sim.fallouts.empty(), "a bomber flew through a wall of rocket batteries and still dropped");
    clearUnits12();
    return true;
}

bool updateTest(u64 seed) {
    init12(seed);
    if (!mapTest()) return false;
    if (!launcherTest()) return false;
    if (!fortressTest()) return false;
    init12(seed);
    if (!stealthTest12()) return false;
    if (!vegetationTest()) return false;
    if (!dozerSideTest()) return false;
    init12(seed);
    if (!airTest12()) return false;
    printf("updatetest seed %llu: ok (map, island launchers, fortresses, bastion walls, infiltrator stealth, vegetation, dozer routes, bombers, fighters, Nuclear Bomber)\n", (unsigned long long)seed);
    return g_ok12;
}
