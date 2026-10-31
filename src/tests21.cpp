// One Hour - headless tests 21: The Chosen and its Shadow Guard (only helicopters find them; instant captures; units turned; the 30-man call;
// four weapons in one; one Chosen at a time) and the computer commanders using it and hunting it
#include "ai.h"
#include "testutil.h"

#define VERIFY21(c, m) do { if (!(c)) { fprintf(stderr, "chosentest: %s\n", m); return false; } } while (0)

static void run21(float sec, bool ai = false) { for (int t = 0, n = (int)(sec * SIM_HZ); t < n && !g_sim.gameOver; t++) { g_sim.step(); if (ai) g_ai.update(); g_sim.events.clear(); } }
static Ref at21(int type, int owner, Vec2 p) { return g_sim.spawnUnit(type, owner, g_map.nearestFree(p, 30)); }
static Ref place21(int type, int owner, Vec2 nearP) {
    int cx = tileOf(nearP.x), cy = tileOf(nearP.y);
    for (int r = 0; r < 20; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) return g_sim.placeBuilding(type, owner, cx + dx, cy + dy, true);
    }
    return NOREF;
}
// two armies, nobody playing them, the units of the start removed, everything explored
static void scene21(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 }; bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
}
static int chosenT(Faction f) { return f == F_CYBER ? U_C_CHOSEN : U_K_CHOSEN; }
static int shadowT(Faction f) { return f == F_CYBER ? U_C_SHADOW : U_K_SHADOW; }

static bool chosenScene(Faction me, Faction foe, u64 seed) {
    scene21(me, foe, seed);
    Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.42f), 9);
    flattenArena(mid, 14, 14);
    const int uf = firstUnitOf(foe);
    // ---- nothing but a helicopter finds it: a sniper, a spy drone, a jet overhead, a jeep, a Laser Turret / Gun Nest and a bunker all look through it
    Ref ch = at21(chosenT(me), 0, mid);
    Entity* C = g_sim.get(ch);
    VERIFY21(C && C->ut().stealth == ST_CHOSEN && C->weapon() == W_CONVERT, "The Chosen is not a stealthy converter");
    Ref sn = at21(foe == F_CYBER ? U_C_SNIPER : U_K_SNIPER, 1, mid + Vec2(6 * TILE, 0));
    Ref sd = at21(foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE, 1, mid + Vec2(2 * TILE, -2 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    Ref jet = at21(foe == F_CYBER ? U_C_FIGHTER : U_K_FIGHTER, 1, mid + Vec2(1 * TILE, 0)); if (Entity* a = g_sim.get(jet)) a->alt = 1;
    Ref jp = at21(foe == F_CYBER ? U_C_JEEP : U_K_JEEP, 1, mid + Vec2(-8 * TILE, 0));
    Ref tur = place21(firstBuildOf(foe) + BR_TURRET, 1, mid + Vec2(0, 7 * TILE));
    g_sim.updatePowerPublic(); for (auto& p : g_sim.players) p.powerMade = 200;
    g_sim.cmdStop({ch});
    run21(3);
    VERIFY21(g_sim.get(ch) && !g_sim.visibleTo(*g_sim.get(ch), 1), "something other than a helicopter found The Chosen");
    // a helicopter within CHOSEN_DETECT finds it, and once found the enemy can shoot it; it is lost again moments after the helicopter leaves
    Ref heli = at21(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, mid + Vec2(4 * TILE, 0)); if (Entity* a = g_sim.get(heli)) { a->alt = 1; a->order = O_IDLE; }
    run21(0.5f);
    VERIFY21(g_sim.get(ch) && g_sim.visibleTo(*g_sim.get(ch), 1), "a helicopter 4 tiles away did not find The Chosen");
    g_sim.destroy(*g_sim.get(heli), false);
    run21(CHOSEN_LINGER + 1.0f);
    VERIFY21(g_sim.get(ch) && !g_sim.visibleTo(*g_sim.get(ch), 1), "The Chosen stayed found long after the helicopter was gone");
    for (Ref r : { sn, sd, jet, jp }) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    if (Entity* t = g_sim.get(tur)) g_sim.destroy(*t, false);
    // ---- it turns soldiers and vehicles at once, never aircraft (a gunship hovering over it stays the enemy's)
    Ref tank = at21(uf + 5, 1, mid + Vec2(3 * TILE, 0));
    Ref inf = at21(uf + 2, 1, mid + Vec2(-3 * TILE, 1 * TILE));
    run21(4);
    if (getenv("ONEHOUR_TESTDBG")) { Entity* I = g_sim.get(inf); Entity* T = g_sim.get(tank); printf("  tank %s owner %d | soldier %s owner %d hp %.0f | converted %d\n", T ? "alive" : "dead", T ? T->owner : -1, I ? "alive" : "dead", I ? I->owner : -1, I ? I->hp : 0.0f, g_sim.players[0].converted); }
    VERIFY21(g_sim.get(tank) && g_sim.get(tank)->owner == 0, "The Chosen did not turn an enemy tank 3 tiles away");
    VERIFY21(g_sim.get(inf) && g_sim.get(inf)->owner == 0, "The Chosen did not turn an enemy soldier 3 tiles away");
    Ref gun = at21(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, g_sim.get(ch)->pos + Vec2(2 * TILE, 0)); if (Entity* a = g_sim.get(gun)) { a->alt = 1; g_sim.cmdStop({gun}); }
    run21(3);
    VERIFY21(!g_sim.get(gun) || g_sim.get(gun)->owner == 1, "The Chosen turned an aircraft");
    VERIFY21(g_sim.players[0].converted >= 2 && g_sim.players[1].convertedLost >= 2, "the turnings were not counted");
    if (Entity* g = g_sim.get(gun)) g_sim.destroy(*g, false);
    // ---- it takes a structure the moment it reaches it (an infiltrator needs seconds): sent at an enemy barracks, idle beside a factory
    Ref bar = place21(firstBuildOf(foe) + BR_BARRACKS, 1, mid + Vec2(10 * TILE, -6 * TILE));
    VERIFY21(bar.valid(), "no spot for an enemy barracks");
    if (Entity* c = g_sim.get(ch)) c->pos = g_map.nearestFree(g_sim.get(bar)->pos + Vec2(-(g_sim.get(bar)->radius() + 20), 0), 4);
    g_sim.cmdAttack({ch}, bar);
    int ticks = 0;
    for (; ticks < 6 * SIM_HZ && g_sim.get(bar) && g_sim.get(bar)->owner != 0; ticks++) { g_sim.step(); g_sim.events.clear(); }
    VERIFY21(g_sim.get(bar) && g_sim.get(bar)->owner == 0, "The Chosen sent at a barracks did not take it");
    VERIFY21(ticks < (int)(1.0f * SIM_HZ) + 10, "The Chosen took a structure next to it no faster than an infiltrator would");
    Ref fac = place21(firstBuildOf(foe) + BR_FACTORY, 1, g_sim.get(ch)->pos + Vec2(0, 7 * TILE));
    VERIFY21(fac.valid(), "no spot for an enemy factory");
    if (Entity* c = g_sim.get(ch)) { c->pos = g_map.nearestFree(g_sim.get(fac)->pos + Vec2(0, -(g_sim.get(fac)->radius() + 24)), 4); g_sim.cmdStop({ch}); }
    run21(3);
    VERIFY21(g_sim.get(fac) && g_sim.get(fac)->owner == 0, "an idle Chosen beside an enemy factory did not take it");
    // ---- the Shadow Guard: 30 at a call, round the Chosen, a 2 minute wait between calls
    float ready0 = g_sim.players[0].shadowReady;
    VERIFY21(g_sim.time >= ready0 && g_sim.shadowWait(0) == 0.0f, "the call was not ready for a new Chosen");
    VERIFY21(g_sim.cmdShadowCall(0, ch), "The Chosen could not call its Shadow Guard");
    int n = 0; float far = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.type == shadowT(me)) { n++; far = std::max(far, dist(e.pos, g_sim.get(ch)->pos)); }
    VERIFY21(n == CHOSEN_SQUAD, "a call did not bring 30 Shadow Guard");
    VERIFY21(far < 5.0f * TILE, "the Shadow Guard did not appear round The Chosen");
    VERIFY21(!g_sim.cmdShadowCall(0, ch), "a second call went through at once");
    VERIFY21(std::abs(g_sim.shadowWait(0) - CHOSEN_CALL_COOLDOWN) < 0.2f, "the call did not start a 2 minute wait");
    // ---- four weapons in one: the right one for each target
    {
        Entity* s = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.type == shadowT(me)) { s = &e; break; }
        VERIFY21(s, "no Shadow Guard");
        Ref a1 = at21(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, mid + Vec2(0, 30 * TILE)); if (Entity* a = g_sim.get(a1)) a->alt = 1;
        Ref t1 = at21(uf + 5, 1, mid + Vec2(0, 32 * TILE)), i1 = at21(uf + 2, 1, mid + Vec2(2 * TILE, 32 * TILE));
        VERIFY21(g_sim.shadowWeapon(*s, *g_sim.get(a1), 5) == W_SHADOW_AA, "a Shadow Guard did not pick the stinger for an aircraft");
        VERIFY21(g_sim.shadowWeapon(*s, *g_sim.get(t1), 5) == W_SHADOW_ROCKET, "a Shadow Guard did not pick a rocket for a tank");
        VERIFY21(g_sim.shadowWeapon(*s, *g_sim.get(i1), 6.5f) == W_SHADOW_SNIPER && g_sim.shadowWeapon(*s, *g_sim.get(i1), 2) == W_SHADOW_RIFLE, "a Shadow Guard did not pick marksman / carbine by range");
        for (Ref r : { a1, t1, i1 }) if (Entity* e = g_sim.get(r)) g_sim.destroy(*e, false);
    }
    // ---- they fight unseen: a squad shoots an enemy tank and soldiers to pieces, and no enemy ever sees one of them (no helicopter near)
    Vec2 fight = g_sim.get(ch)->pos + Vec2(0, 5 * TILE);
    Ref et = at21(uf + 5, 1, fight + Vec2(4 * TILE, 0));
    std::vector<Ref> rifles; for (int k = 0; k < 4; k++) rifles.push_back(at21(uf + 2, 1, fight + Vec2(4 * TILE, (k - 1.5f) * TILE)));
    if (Entity* c = g_sim.get(ch)) { c->pos = g_map.nearestFree(fight + Vec2(-9 * TILE, 0), 6); g_sim.cmdStop({ch}); }   // (out of reach: the squad must do it)
    bool seen = false; float squadHp0 = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.type == shadowT(me)) squadHp0 += e.hp;
    for (int t = 0; t < 12 * SIM_HZ; t++) {
        g_sim.step(); g_sim.events.clear();
        for (auto& e : g_sim.ents) if (e.alive && e.type == shadowT(me) && g_sim.visibleTo(e, 1)) seen = true;
    }
    int foesLeft = (g_sim.get(et) && g_sim.get(et)->owner == 1) ? 1 : 0; for (Ref r : rifles) if (g_sim.get(r) && g_sim.get(r)->owner == 1) foesLeft++;
    float squadHp1 = 0; for (auto& e : g_sim.ents) if (e.alive && e.type == shadowT(me)) squadHp1 += e.hp;
    VERIFY21(foesLeft == 0, "a Shadow Guard squad did not finish a tank and four soldiers in 12 s");
    VERIFY21(!seen, "the enemy saw a Shadow Guard while it fired (no helicopter near)");
    VERIFY21(squadHp1 >= squadHp0 - 1.0f, "the blind enemy still hurt the Shadow Guard");
    // ---- one Chosen at a time: the barracks refuses a second while it lives, trains one again once it is gone
    if (g_sim.time < CHOSEN_OPEN) {   // (none in the opening)
        Ref b0 = place21(firstBuildOf(me) + BR_BARRACKS, 0, g_sim.players[0].basePos + Vec2(-8 * TILE, 0));
        place21(firstBuildOf(me) + BR_TECH, 0, g_sim.players[0].basePos + Vec2(6 * TILE, 6 * TILE));
        VERIFY21(b0.valid() && !g_sim.unitAvailable(0, chosenT(me)), "The Chosen could be trained in the opening");
        g_sim.time = CHOSEN_OPEN + 1.0f;
    }
    Ref myBar = place21(firstBuildOf(me) + BR_BARRACKS, 0, g_sim.players[0].basePos);
    place21(firstBuildOf(me) + BR_TECH, 0, g_sim.players[0].basePos + Vec2(6 * TILE, 6 * TILE));
    VERIFY21(myBar.valid(), "no spot for my barracks");
    VERIFY21(!g_sim.cmdTrain(myBar, chosenT(me)), "a second Chosen was queued while the first lives");
    g_sim.destroy(*g_sim.get(ch), true);
    VERIFY21(g_sim.cmdTrain(myBar, chosenT(me)), "no new Chosen could be trained after the first fell");
    VERIFY21(!g_sim.cmdTrain(myBar, chosenT(me)), "two Chosen could be queued at once");
    printf("chosentest %s vs %s: only helicopters find it, units turned, structures taken at once (%d ticks), 30-man call with a 2 min wait, four weapons, unseen while firing, one at a time\n", FACTION_NAME[me], FACTION_NAME[foe], ticks);
    return true;
}

// A ship near the shore changes sides too
static bool chosenShip(Faction me, Faction foe, u64 seed) {
    scene21(me, foe, seed);
    // a beach: a land tile next to open water
    Vec2 land, water; bool found = false;
    for (int ty = ISLE_X0 + 10; ty < ISLE_X0 + ISLE_W - 10 && !found; ty += 3) for (int tx = ISLE_X0 + 10; tx < ISLE_X0 + ISLE_W - 10 && !found; tx += 3) {
        if (!g_map.passable(tx, ty)) continue;
        for (int d = 2; d <= 3 && !found; d++) if (g_map.navTile(tx + d, ty) && g_map.navTile(tx + d + 1, ty) && g_map.bigWater(tx + d, ty)) { land = tileCenter(tx, ty); water = tileCenter(tx + d + 1, ty); found = true; }
    }
    VERIFY21(found, "no beach for the ship scene");
    Ref ship = g_sim.spawnUnit(foe == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, 1, water);
    Ref ch = g_sim.spawnUnit(chosenT(me), 0, land);
    g_sim.cmdStop({ch});
    run21(4);
    VERIFY21(g_sim.get(ship) && g_sim.get(ship)->owner == 0, "The Chosen on the beach did not turn an enemy destroyer off it");
    printf("chosentest %s: a destroyer turned from the beach\n", FACTION_NAME[me]);
    return true;
}

// The computer commanders train The Chosen, use it, and hunt the enemy's with helicopters
static bool chosenAi(u64 seed) {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER }; bool ai[4] = { true, true, true, true }; int diff[4] = { 3, 3, 2, 2 }; int team[4] = { 0, 1, 2, 3 };
    g_sim.init(4, fac, ai, diff, team, seed);
    for (int p = 0; p < 4; p++) { g_ai.brainEnabled[p] = false; g_ai.proEnabled[p] = true; g_ai.smartEnabled[p] = true; g_ai.featMask[p] = FEAT_DEFAULT; g_ai.featMask2[p] = F2_DEFAULT; g_ai.brainOverride[p] = nullptr; }
    g_ai.init(seed);
    int trained = 0, calls = 0; float firstChosen = -1;
    for (int t = 0; t < 24 * 60 * SIM_HZ && !g_sim.gameOver; t++) {
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        if (t % SIM_HZ) continue;
        for (int p = 0; p < 4; p++) if (g_sim.chosenOf(p).valid() && firstChosen < 0) firstChosen = g_sim.time;
    }
    int heliHunts = 0;
    for (int p = 0; p < 4; p++) { calls += g_sim.players[p].shadowCalls; trained += g_sim.players[p].converted + g_sim.players[p].chosenTaken; heliHunts += g_ai.ais[p].heliHunts; }
    printf("chosentest AI: helicopter hunts %d, feints %d %d %d %d, sold %d %d %d %d\n", heliHunts, g_ai.ais[0].feints, g_ai.ais[1].feints, g_ai.ais[2].feints, g_ai.ais[3].feints, g_ai.ais[0].chosenSold, g_ai.ais[1].chosenSold, g_ai.ais[2].chosenSold, g_ai.ais[3].chosenSold);
    printf("chosentest AI: match at %.0f s%s, ", g_sim.time, g_sim.gameOver ? " (over)" : "");
    printf("first Chosen at %.0f s; per army: calls / units turned / structures taken:", firstChosen);
    for (int p = 0; p < 4; p++) printf("  P%d %d/%d/%d", p, g_sim.players[p].shadowCalls, g_sim.players[p].converted, g_sim.players[p].chosenTaken);
    printf("\n");
    VERIFY21(firstChosen > 0, "no computer commander trained The Chosen in 24 minutes");
    VERIFY21(calls > 0, "no computer commander called a Shadow Guard");
    VERIFY21(trained > 0, "no computer commander's Chosen turned a unit or took a structure");
    return true;
}

bool chosenTest(u64 seed) {
    if (!chosenScene(F_CYBER, F_CLANKER, seed)) return false;
    if (!chosenScene(F_CLANKER, F_CYBER, seed + 1)) return false;
    if (!chosenShip(F_CYBER, F_CLANKER, seed)) return false;
    if (!getenv("ONEHOUR_CHOSEN_NOAI") && !chosenAi(seed)) return false;
    printf("chosentest: all passed\n");
    return true;
}
