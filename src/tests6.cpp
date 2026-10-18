// One Hour - headless tests of the civilian town, the civilian dozers, the Underground Nuke and the bunker's watch over the structures around it (--civictest)
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok6 = true;
static bool fail6(const char* m) { fprintf(stderr, "civictest: %s\n", m); g_ok6 = false; return false; }
#define CHECK6(cond, msg) do { if (!(cond)) return fail6(msg); } while (0)

static void run6(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static Ref spawn6(int type, int owner, Vec2 p) { return g_sim.spawnUnit(type, owner, g_map.nearestFree(p, 30)); }

// two armies, nothing on the field but their structures, everything explored, plenty of money; player 0 is a human (its nukes hurt everybody)
static void scene6(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 400000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
}
// the nearest spot to 'near' where a structure of this type can stand
static bool spot6(int player, int type, Vec2 near, int& tx, int& ty) {
    int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r < 20; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (g_sim.canPlace(player, type, cx + dx, cy + dy)) { tx = cx + dx; ty = cy + dy; return true; }
    }
    return false;
}
static Ref place6(int type, int owner, Vec2 near) {
    int tx, ty;
    if (!spot6(owner, type, near, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, owner, tx, ty, true);
}
static int countType(int owner, int type) { int n = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == owner && e.type == type) n++; return n; }   // (units only)

// ---- civilian dozers build the town and only the town; the town pays, sends out dozers, has no plating and can be captured
static bool townTest(Faction me, Faction foe, u64 seed) {
    scene6(me, foe, seed);
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 9);
    int milD = firstUnitOf(me), civD = me == F_CYBER ? U_C_CIVDOZER : U_K_CIVDOZER;
    int house = me == F_CYBER ? B_C_HOUSE : B_K_HOUSE, townHall = house + CV_TOWNHALL, sky = house + CV_SKYSCRAPER, power = firstBuildOf(me) + BR_POWER;
    CHECK6(UNITS[civD].cost == UNITS[milD].cost && UNITS[civD].builtBy == BR_HQ && UNITS[civD].role == UR_DOZER, "a civilian dozer is not a dozer trained at the Command structure for a dozer's price");
    CHECK6(g_sim.unitAvailable(0, civD), "the Command structure cannot train a civilian dozer");
    for (int k = 0; k < CV_COUNT; k++) CHECK6(BUILDS[house + k].role == BR_CIVIC && BUILDS[house + k].faction == me && BUILDS[house + k].power == 0 && BUILDS[house + k].weapon < 0, "a civilian structure is not an unarmed civilian structure that needs no power");
    Ref civ = spawn6(civD, 0, mid), mil = spawn6(milD, 0, mid + Vec2(30, 0));
    int tx, ty, px, py;
    CHECK6(spot6(0, house, mid + Vec2(0, 3 * TILE), tx, ty) && spot6(0, power, mid + Vec2(0, -4 * TILE), px, py), "no room for the test structures");
    CHECK6(!g_sim.cmdBuild(mil, house, tx, ty), "a military dozer built a civilian house");
    CHECK6(!g_sim.cmdBuild(civ, power, px, py), "a civilian dozer built a power plant");
    int money0 = g_sim.players[0].money;
    CHECK6(g_sim.cmdBuild(civ, house, tx, ty), "a civilian dozer could not build a house");
    CHECK6(g_sim.players[0].money == money0 - BUILDS[house].cost, "a house did not cost its price");
    Ref site = NOREF;
    for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.type == house && !e.constructed) site = g_sim.refOf(e);
    g_sim.cmdAssist({mil}, site);
    CHECK6(!(g_sim.get(mil)->order == O_BUILD && g_sim.get(mil)->targetEnt == site), "a military dozer took up building a civilian site");
    for (int k = 0; k < 40 && !g_sim.get(site)->constructed; k++) run6(1);
    CHECK6(g_sim.get(site) && g_sim.get(site)->constructed, "the civilian dozer did not finish the house");
    Ref hs = site;
    // ---- the house pays every CIVIC_INTERVAL seconds with no power and no supplies
    int mined0 = g_sim.players[0].mined;
    run6(CIVIC_INTERVAL * 3 + 0.5f);
    int got = g_sim.players[0].mined - mined0;
    CHECK6(got >= 3 * CIVICS[CV_HOUSE].income && got <= 4 * CIVICS[CV_HOUSE].income, "a house did not pay its income every ten seconds");
    // ---- free dozers: a military one, then a civilian one, and none once the army has CIVIC_DOZER_CAP of them
    int m0 = countType(0, milD), c0 = countType(0, civD);
    g_sim.get(hs)->dozerAt = g_sim.time + 0.2f; run6(1);
    CHECK6(countType(0, milD) == m0 + 1 && countType(0, civD) == c0, "the town's first free dozer was not a military dozer");
    g_sim.get(hs)->dozerAt = g_sim.time + 0.2f; run6(1);
    CHECK6(countType(0, milD) == m0 + 1 && countType(0, civD) == c0 + 1, "the town's second free dozer was not a civilian dozer");
    while (countType(0, milD) + countType(0, civD) < CIVIC_DOZER_CAP) spawn6(milD, 0, mid + Vec2(-3 * TILE, 0));
    int all = countType(0, milD) + countType(0, civD);
    g_sim.get(hs)->dozerAt = g_sim.time + 0.2f; run6(1);
    CHECK6(countType(0, milD) + countType(0, civD) == all, "the town sent a dozer past the cap");
    // ---- a civilian dozer mends the town but not the base (a military dozer mends both); it never boards a bunker
    {
        Ref pw = place6(power, 0, mid + Vec2(0, -5 * TILE));
        CHECK6(pw.valid(), "no room for the test power plant");
        g_sim.get(pw)->hp *= 0.5f; g_sim.get(pw)->lastDamaged = g_sim.time - 30;
        g_sim.get(hs)->hp *= 0.5f; g_sim.get(hs)->lastDamaged = g_sim.time - 30;
        g_sim.cmdAssist({civ}, pw);
        CHECK6(!(g_sim.get(civ)->order == O_BUILD && g_sim.get(civ)->targetEnt == pw), "a civilian dozer took up mending a power plant");
        Entity* job = g_sim.repairJob(*g_sim.get(civ));
        CHECK6(job && job->type == house, "a civilian dozer's own repair round did not pick the damaged house over the power plant");
        g_sim.cmdAssist({mil}, pw);
        CHECK6(g_sim.get(mil)->order == O_BUILD && g_sim.get(mil)->targetEnt == pw, "a military dozer would not mend a power plant");
        Ref bk = place6(me == F_CYBER ? B_C_BUNKER : B_K_BUNKER, 0, mid + Vec2(-5 * TILE, -5 * TILE));
        CHECK6(bk.valid() && !g_sim.canBoard(*g_sim.get(civ), *g_sim.get(bk)) && g_sim.canBoard(*g_sim.get(mil), *g_sim.get(bk)), "the bunker's dozer slot took a civilian dozer (or refused a military one)");
        g_sim.destroy(*g_sim.get(bk), false); g_sim.destroy(*g_sim.get(pw), false);
        g_sim.get(hs)->hp = g_sim.get(hs)->maxHp; g_sim.cmdStop({civ, mil});
    }
    // ---- no plating, no roof guns: a hit lands in full, even for an army with Rugged plating
    g_sim.players[0].upg[UPG_RUGGED] = true; g_sim.players[0].upg[UPG_GUNS] = true;
    Ref h2 = place6(house + CV_GAS, 0, mid + Vec2(-4 * TILE, 4 * TILE));
    CHECK6(h2.valid() && g_sim.get(h2)->maxHp == BUILDS[house + CV_GAS].hp, "a civilian structure got Rugged plating");
    float hp0 = g_sim.get(h2)->hp;
    g_sim.applyDamagePublic(*g_sim.get(h2), 100, 1, NOREF);
    CHECK6(std::fabs(g_sim.get(h2)->hp - (hp0 - 100)) < 0.5f, "a civilian structure shrugged off part of a hit");
    { Entity* g2 = g_sim.get(h2); g2->engaged2 = NOREF; Ref foeTank = spawn6(firstUnitOf(foe) + 5, 1, g2->pos + Vec2(0, 4 * TILE)); run6(2); CHECK6(!g_sim.get(h2)->engaged2.valid(), "a civilian structure fired a roof gun"); g_sim.destroy(*g_sim.get(foeTank), false); }
    g_sim.players[0].upg[UPG_RUGGED] = false; g_sim.players[0].upg[UPG_GUNS] = false;
    // ---- one Town Hall; the big ones need it
    CHECK6(!g_sim.buildAvailable(0, sky), "a skyscraper was available without a Town Hall");
    Ref th = place6(townHall, 0, mid + Vec2(5 * TILE, 4 * TILE));
    CHECK6(th.valid() && g_sim.buildAvailable(0, sky) && g_sim.buildAvailable(0, house + CV_GOVERNMENT), "a Town Hall did not open the skyscraper and the government office");
    CHECK6(!g_sim.buildAvailable(0, townHall), "a second Town Hall could be built");
    // ---- a spy takes a civilian structure like any other: from then on it pays the spy's army
    int spyFoe = foe == F_CYBER ? U_C_SPY : U_K_SPY;
    Ref spy = spawn6(spyFoe, 1, g_sim.get(hs)->pos + Vec2(0, 3 * TILE));
    g_sim.cmdCapture({spy}, hs);
    for (int k = 0; k < 30 && g_sim.get(hs) && g_sim.get(hs)->owner != 1; k++) run6(1);
    CHECK6(g_sim.get(hs) && g_sim.get(hs)->owner == 1, "a spy could not capture a civilian house");
    int foeMined = g_sim.players[1].mined;
    run6(CIVIC_INTERVAL + 0.5f);
    CHECK6(g_sim.players[1].mined - foeMined >= CIVICS[CV_HOUSE].income, "a captured house did not pay its new owner");
    printf("civictest %s: town ok (civilian dozer builds only the town, house paid $%d in 30 s, free dozers alternate and stop at %d, no plating, one Town Hall, captured by a spy)\n", FACTION_NAME[me], got, CIVIC_DOZER_CAP);
    return true;
}

// ---- the Underground Nuke: price, prerequisite, its warhead three times as wide and as long-lasting (2.7x the damage), and its silo as hard to kill
static bool deepNukeTest(Faction me, Faction foe, u64 seed) {
    scene6(me, foe, seed);
    int bb = firstBuildOf(me), silo = me == F_CYBER ? B_C_UNUKE : B_K_UNUKE;
    CHECK6(BUILDS[silo].cost == 10 * BUILDS[bb + BR_NUKE].cost && BUILDS[silo].role == BR_UNUKE, "an Underground Nuke does not cost ten Nuke Ramps");
    CHECK6(!g_sim.buildAvailable(0, silo), "an Underground Nuke was available without a Nuke Ramp");
    Vec2 home = g_sim.players[0].basePos;
    for (int k = 0; k < 2; k++) place6(bb + BR_POWER, 0, home + Vec2(-6 * TILE, (k * 3 - 2) * TILE));
    place6(bb + BR_NUKE, 0, home + Vec2(6 * TILE, 0));
    CHECK6(g_sim.buildAvailable(0, silo), "a Nuke Ramp did not open the Underground Nuke");
    Ref sr = place6(silo, 0, home + Vec2(0, 7 * TILE));
    CHECK6(sr.valid(), "no room for the silo");
    g_sim.updatePowerPublic();
    Entity* S = g_sim.get(sr);
    CHECK6(S->hatchMax == UNUKE_DOOR_HP && S->hatch == UNUKE_DOOR_HP, "the silo has no blast doors");
    CHECK6(g_sim.deepNukesReady(0) == 0, "a fresh silo was ready before arming");
    S->actionTimer = g_sim.time;
    CHECK6(g_sim.deepNukesReady(0) == 1, "an armed, powered silo was not ready");
    Vec2 far = g_sim.players[1].basePos;
    CHECK6(g_sim.cmdDeepNuke(0, far), "the silo could not launch");
    CHECK6(g_sim.nukes.size() == 1 && g_sim.nukes[0].deep && std::fabs(Sim::nukeRadius(g_sim.nukes[0]) - 3.0f * NUKE_RADIUS * TILE) < 1.0f, "the silver warhead's blast radius is not three times a ramp's");
    CHECK6(g_sim.deepNukesReady(0) == 0 && !g_sim.cmdDeepNuke(0, far), "a silo launched twice without reloading");
    g_sim.nukes.clear();
    // ---- the silo is as hard as a bunker: doors first, plating, and nothing but troops take it below the floor
    float floorHp = S->maxHp * BUNKER_FLOOR;
    g_sim.applyDamagePublic(*S, 1e7f, 1, NOREF);   // (a nuke, an aircraft, a structure: no ground unit behind it)
    S = g_sim.get(sr);
    CHECK6(S && S->hatch == 0 && S->hp >= floorHp - 0.5f, "something other than troops took the silo below its floor");
    Ref tank = spawn6(firstUnitOf(foe) + 5, 1, S->pos + Vec2(0, 5 * TILE));
    g_sim.applyDamagePublic(*S, 20000, 1, tank);
    CHECK6(!g_sim.get(sr) || g_sim.get(sr)->hp < floorHp, "a tank could not take the silo below its floor");
    // ---- three times the damage: a large structure at ground zero survives a ramp's warhead but not the silver one; a tank at 1.6 ramp radii
    // is outside a ramp's blast but inside the silver one; the radiation is three times as strong and lasts three times as long
    Vec2 gz = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 7);
    int facFoe = firstBuildOf(foe) + BR_FACTORY;
    for (int pass = 0; pass < 2; pass++) {
        bool deep = pass == 1;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && (e.isUnit() || e.type == facFoe)) g_sim.destroy(e, false);
        g_sim.fallouts.clear();
        Ref fac = place6(facFoe, 1, gz);
        CHECK6(fac.valid(), "no room for the target factory");
        Vec2 at = g_sim.get(fac)->pos;
        Ref mid = spawn6(firstUnitOf(foe) + 5, 1, at + Vec2(1.6f * NUKE_RADIUS * TILE, 0));
        float midD = dist(g_sim.get(mid)->pos, at);
        Sim::Nuke n; n.from = home; n.pos = at; n.owner = 0; n.t = Sim::NUKE_FLIGHT - 0.05f; n.scale = deep ? UNUKE_SCALE : 1.0f; n.deep = deep;
        g_sim.nukes.push_back(n);
        run6(0.2f);
        bool facDead = !g_sim.get(fac), midDead = !g_sim.get(mid);
        if (!deep) CHECK6(!facDead && !midDead && midD > NUKE_RADIUS * TILE, "a ramp's warhead flattened a large structure or reached past its radius");
        else {
            CHECK6(facDead, "the silver warhead did not flatten a large structure at ground zero (three times the damage)");
            CHECK6(midDead, "the silver warhead did not kill a tank at 1.6 ramp radii (three times the radius)");
            CHECK6(!g_sim.fallouts.empty() && std::fabs(g_sim.fallouts.back().life - Sim::FALLOUT_LIFE * UNUKE_FALLOUT) < 0.01f && g_sim.fallouts.back().power >= UNUKE_FALLOUT - 0.01f, "the silver warhead's radiation is not three times as strong and long");
        }
    }
    printf("civictest %s: underground nuke ok ($%d, needs a ramp, 3x radius and radiation, 2.7x damage, blast doors and deep plating)\n", FACTION_NAME[me], BUILDS[silo].cost);
    return true;
}

// ---- only an Anti-Nuke Laser stops the silver warhead: the Clanker missiles and the bunkers' own guns let it through (they still engage a ramp's)
static bool deepInterceptTest(Faction defender, u64 seed) {
    scene6(defender == F_CYBER ? F_CLANKER : F_CYBER, defender, seed);
    Vec2 gz = g_sim.players[1].basePos;
    int bb = firstBuildOf(defender), an = defender == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE, bk = defender == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    for (int k = 0; k < 3; k++) place6(bb + BR_POWER, 1, gz + Vec2(-7 * TILE, (k * 3 - 3) * TILE));
    Ref a = place6(an, 1, gz + Vec2(5 * TILE, 3 * TILE)), b = place6(bk, 1, gz + Vec2(-3 * TILE, 6 * TILE));
    CHECK6(a.valid() && b.valid(), "no room for the defences");
    g_sim.updatePowerPublic();
    bool laser = defender == F_CYBER;
    float pDeep = g_sim.antiNukeChance(gz, 0, false, UNUKE_SCALE, true), pRamp = g_sim.antiNukeChance(gz, 0, false, 1.0f, false);
    CHECK6(pRamp > 0, "the anti-nuke defence does not cover its own base");
    CHECK6(laser ? pDeep > 0 : pDeep == 0, laser ? "an Anti-Nuke Laser does not count against a silver warhead" : "anti-nuke missiles count against a silver warhead");
    int engaged[2] = { 0, 0 };
    for (int pass = 0; pass < 2; pass++) {
        g_sim.nukes.clear(); g_sim.interceptors.clear(); g_sim.fallouts.clear();
        if (Entity* A = g_sim.get(a)) A->siloTimer = 0;
        if (Entity* B = g_sim.get(b)) B->antiNukeCd = 0;
        Sim::Nuke n; n.from = g_sim.players[0].basePos; n.pos = gz; n.owner = 0; n.t = Sim::NUKE_FLIGHT - 6.8f; n.deep = pass == 1; n.scale = n.deep ? UNUKE_SCALE : 1.0f;
        g_sim.nukes.push_back(n);
        for (int t = 0; t < (int)(6.5f * SIM_HZ) && !g_sim.nukes.empty(); t++) { g_sim.step(); g_sim.events.clear(); if (!g_sim.nukes.empty()) engaged[pass] = std::max(engaged[pass], (int)g_sim.nukes[0].engagedBy.size()); }
    }
    CHECK6(engaged[0] > 0, "the defences did not engage a ramp's warhead (the scene is broken)");
    if (laser) CHECK6(engaged[1] > 0, "an Anti-Nuke Laser did not engage a silver warhead");
    else CHECK6(engaged[1] == 0, "anti-nuke missiles or a bunker engaged a silver warhead");
    printf("civictest %s: silver warhead interception ok (%s)\n", FACTION_NAME[defender], laser ? "the laser engages it" : "missiles and bunker let it through");
    return true;
}

// ---- the bunker watches over its neighbours: it answers a tank shelling a house beyond the reach of its miniguns, and mends what is hurt
static bool bunkerWardTest(Faction me, Faction foe, u64 seed) {
    scene6(me, foe, seed);
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 9);
    int bk = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER, house = me == F_CYBER ? B_C_HOUSE : B_K_HOUSE;
    Ref b = place6(bk, 0, mid + Vec2(-5 * TILE, 0)), h = place6(house, 0, mid + Vec2(-1 * TILE, 0));
    CHECK6(b.valid() && h.valid(), "no room for the bunker and the house");
    Vec2 bp = g_sim.get(b)->pos, hp = g_sim.get(h)->pos;
    Vec2 tp = g_map.nearestFree(bp + Vec2(9.0f * TILE, 0), 6);
    Ref tank = spawn6(firstUnitOf(foe) + 5, 1, tp);
    { Entity* T = g_sim.get(tank); T->maxHp *= 4; T->hp = T->maxHp; }
    float d0 = g_sim.distToEntity(bp, *g_sim.get(tank)) / TILE;
    CHECK6(d0 > WEAPONS[W_BUNKER_MINIGUN].range + 0.3f && d0 < WEAPONS[W_BUNKER_ROCKETS].range - 0.3f, "the tank does not stand between minigun and pod reach");
    g_sim.cmdAttack({tank}, h);
    float t0 = g_sim.get(tank)->hp;
    run6(10);
    CHECK6(g_sim.get(h) && g_sim.get(h)->hp < g_sim.get(h)->maxHp, "the tank never hit the house (the scene is broken)");
    CHECK6(!g_sim.get(tank) || g_sim.get(tank)->hp < t0, "the bunker did not answer a tank shelling the house next to it");
    if (g_sim.get(tank)) g_sim.destroy(*g_sim.get(tank), false);
    // ---- mending: once the shooting has stopped the bunker's crew patches the house
    Entity* H = g_sim.get(h);
    CHECK6(H, "the house fell");
    H->hp = H->maxHp * 0.5f; H->lastDamaged = g_sim.time - 30;
    float before = H->hp;
    run6(5);
    CHECK6(g_sim.get(h)->hp > before + H->maxHp * 0.01f, "the bunker did not mend a damaged structure next to it");
    // ---- seismic sensors: an enemy sniper hiding next to the bunker is found; a spy drone overhead is not
    int sniperFoe = foe == F_CYBER ? U_C_SNIPER : U_K_SNIPER, droneFoe = foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    Ref sn = spawn6(sniperFoe, 1, bp + Vec2(0, 4 * TILE)), dr = spawn6(droneFoe, 1, bp + Vec2(3 * TILE, -2 * TILE));
    for (Ref r : { sn, dr }) if (Entity* u = g_sim.get(r)) { u->maxHp *= 50; u->hp = u->maxHp; }   // (once found, the hatch guns open up on it)
    run6(0.5f);
    CHECK6(g_sim.get(sn) && g_sim.visibleTo(*g_sim.get(sn), 0), "a bunker's sensors did not find a sniper next to it");
    CHECK6(!g_sim.get(dr) || !g_sim.visibleTo(*g_sim.get(dr), 0), "a bunker's sensors found a spy drone in the air");
    (void)hp;
    printf("civictest %s: bunker ward ok (answers attacks on its neighbours, mends them, feels snipers)\n", FACTION_NAME[me]);
    return true;
}

// ---- a computer army raises a town of its own once its base runs
static bool aiTownTest(u64 seed) {
    Faction fac[2] = { F_CYBER, F_CLANKER };
    bool ai[2] = { true, true }; int diff[2] = { 2, 2 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_ai.init(seed);
    int civics[2] = { 0, 0 }, civD[2] = { 0, 0 };
    for (int t = 0; t < (int)(720.0f * SIM_HZ) && !g_sim.gameOver; t++) {
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        if (t % 200 == 0) for (int p = 0; p < 2; p++) {
            civics[p] = std::max(civics[p], g_sim.countRole(p, BR_CIVIC, true));
            civD[p] = std::max(civD[p], countType(p, p == 0 ? U_C_CIVDOZER : U_K_CIVDOZER));
        }
    }
    CHECK6(civD[0] + civD[1] > 0, "no computer army trained a civilian dozer in twelve minutes");
    CHECK6(civics[0] + civics[1] > 0, "no computer army raised a civilian structure in twelve minutes");
    printf("civictest: computer armies built towns ok (Cyber %d civilian structures, Clanker %d)\n", civics[0], civics[1]);
    return true;
}

bool civicTest(u64 seed) {
    g_ok6 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        if (!townTest(me, foe, seed + fi)) return false;
        if (!deepNukeTest(me, foe, seed + 10 + fi)) return false;
        if (!deepInterceptTest(me, seed + 20 + fi)) return false;
        if (!bunkerWardTest(me, foe, seed + 30 + fi)) return false;
    }
    if (!aiTownTest(seed + 40)) return false;
    printf("civictest: all ok\n");
    return g_ok6;
}
