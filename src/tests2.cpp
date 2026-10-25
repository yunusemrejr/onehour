// One Hour - headless tests of the stealth / spy / cargo chopper / underground bunker / infantry pass (--stealthtest)
#include "sim.h"
#include "map.h"
#include "ui.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok = true;
static bool failMsg(const char* m) { fprintf(stderr, "stealthtest: %s\n", m); g_ok = false; return false; }
#define CHECK(cond, msg) do { if (!(cond)) return failMsg(msg); } while (0)

static void run(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static float hpOf(Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : -1.0f; }
static bool alive(Ref r) { return g_sim.get(r) != nullptr; }

static Ref spawnAt(int type, int owner, Vec2 p) { return g_sim.spawnUnit(type, owner, g_map.nearestFree(p, 30)); }

// the first spot near 'near' where a structure of this type can be placed
static bool spotFor(int type, int owner, Vec2 nearP, int& tx, int& ty) {
    int cx = tileOf(nearP.x), cy = tileOf(nearP.y);
    for (int r = 0; r < 20; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) { tx = cx + dx; ty = cy + dy; return true; }
    }
    return false;
}
static Ref placeAt(int type, int owner, Vec2 nearP) {
    int tx, ty;
    if (!spotFor(type, owner, nearP, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, owner, tx, ty, true);
}

// the open ground nearest the middle of the map: a 15x15 tile square nothing blocks (no lake, rock, tree or supply pile)
static Vec2 openField() {
    Vec2 best = Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f); float bd = 1e18f;
    for (int ty = 8; ty < MAP_H - 8; ty++) for (int tx = 8; tx < MAP_W - 8; tx++) {
        bool free = true;
        for (int dy = -7; dy <= 7 && free; dy++) for (int dx = -7; dx <= 7 && free; dx++) if (!g_map.passable(tx + dx, ty + dy)) free = false;
        if (!free) continue;
        Vec2 c = tileCenter(tx, ty); float d = dist(c, Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f));
        if (d < bd) { bd = d; best = c; }
    }
    return best;
}

// a clean field in the middle of the map: nothing but the two bases far away, everything explored, plenty of money
static void freshScene(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
}

static bool stealthSniperAndDrone(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int sniperMe = me == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
    int tankFoe = firstUnitOf(foe) + 5, wraithFoe = firstUnitOf(foe) + 8;
    int sdroneMe = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE, sdroneFoe = foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    // ---- a sniper is invisible: a tank next to it never finds it, an aircraft overhead does not either
    Ref sn = spawnAt(sniperMe, 0, mid);
    spawnAt(tankFoe, 1, mid + Vec2(5 * TILE, 0));
    Ref air = spawnAt(wraithFoe, 1, mid + Vec2(2 * TILE, 0)); if (Entity* a = g_sim.get(air)) a->alt = 1;
    float sn0 = hpOf(sn);
    run(8);
    CHECK(alive(sn) && hpOf(sn) >= sn0, "a tank or an aircraft found and hit a sniper without a spy drone");
    CHECK(!g_sim.visibleTo(*g_sim.get(sn), 1), "an enemy sees a sniper with no spy drone near");
    CHECK(g_sim.visibleTo(*g_sim.get(sn), 0), "the owner cannot see its own sniper");
    // ---- the sniper itself works from the shadows: it kills infantry and the enemy tank next to it stays blind
    Ref trooper = spawnAt(firstUnitOf(foe) + 2, 1, mid + Vec2(0, 7 * TILE));
    g_sim.cmdAttack({sn}, trooper); run(8);
    CHECK(!alive(trooper), "a stealth sniper could not kill an infantryman");
    CHECK(alive(sn) && hpOf(sn) >= sn0, "the sniper was hit after firing though no spy drone was near");
    // ---- a spy drone nearby: now the sniper can be seen and the tank kills it
    Ref sd = spawnAt(sdroneFoe, 1, mid + Vec2(0, 4 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    run(1);
    CHECK(g_sim.visibleTo(*g_sim.get(sn), 1), "an enemy spy drone close by did not reveal the sniper");
    g_sim.destroy(*g_sim.get(air), false);
    run(10);
    CHECK(!alive(sn) || hpOf(sn) < sn0, "a tank could not shoot a sniper a spy drone had revealed");
    // ---- once the drone is gone the sniper (a fresh one) is hidden again within moments
    Ref sn2 = spawnAt(sniperMe, 0, mid + Vec2(-10 * TILE, 0));
    Ref sd2 = spawnAt(sdroneFoe, 1, g_sim.get(sn2)->pos + Vec2(3 * TILE, 0));
    run(1); CHECK(g_sim.visibleTo(*g_sim.get(sn2), 1), "the second sniper was not revealed");
    g_sim.destroy(*g_sim.get(sd2), false); run(4);
    CHECK(!g_sim.visibleTo(*g_sim.get(sn2), 1), "a sniper stayed revealed long after the spy drone left");
    // ---- the spy drone is stealthy itself: ground units and structures never find it, only drones do
    g_sim.destroy(*g_sim.get(sd), false);
    Ref mySd = spawnAt(sdroneMe, 0, mid + Vec2(0, -8 * TILE)); if (Entity* a = g_sim.get(mySd)) a->alt = 1;
    run(2);
    CHECK(!g_sim.visibleTo(*g_sim.get(mySd), 1), "a spy drone is visible to an enemy with nothing near it");
    Ref aa = placeAt(firstBuildOf(foe) + BR_AATURRET, 1, g_sim.get(mySd)->pos + Vec2(2 * TILE, 3 * TILE));
    CHECK(aa.valid(), "no spot for an anti-air battery");
    g_sim.updatePowerPublic(); for (auto& p : g_sim.players) p.powerMade = 100;
    Ref tank2 = spawnAt(tankFoe, 1, g_sim.get(mySd)->pos + Vec2(3 * TILE, 3 * TILE));
    float sd0 = hpOf(mySd);
    run(10);
    CHECK(alive(mySd) && hpOf(mySd) >= sd0, "an anti-air battery or a tank shot a spy drone nobody had spotted");
    // an enemy spy drone within its range spots it, a bomber drone only when practically on top of it
    Ref foeSd = spawnAt(sdroneFoe, 1, g_sim.get(mySd)->pos + Vec2(7 * TILE, 0)); if (Entity* a = g_sim.get(foeSd)) a->alt = 1;
    run(1);
    CHECK(g_sim.visibleTo(*g_sim.get(mySd), 1), "an enemy spy drone 7 tiles away did not spot my spy drone");
    g_sim.destroy(*g_sim.get(foeSd), false); run(4);
    CHECK(!g_sim.visibleTo(*g_sim.get(mySd), 1), "the spy drone stayed visible after the enemy drone left");
    Ref wr = spawnAt(wraithFoe, 1, g_sim.get(mySd)->pos + Vec2(2.5f * TILE, 0)); if (Entity* a = g_sim.get(wr)) a->alt = 1;
    run(1); CHECK(g_sim.visibleTo(*g_sim.get(mySd), 1), "a bomber drone within 3 tiles did not spot a spy drone");
    g_sim.destroy(*g_sim.get(wr), false); run(4);
    Ref jet = spawnAt(firstUnitOf(foe) + 11, 1, g_sim.get(mySd)->pos + Vec2(1.0f * TILE, 0)); if (Entity* a = g_sim.get(jet)) a->alt = 1;
    run(1); CHECK(!g_sim.visibleTo(*g_sim.get(mySd), 1), "a jet spotted a spy drone (jets cannot)");
    (void)tank2; (void)aa;
    printf("stealthtest %s: stealth snipers and spy drones ok\n", FACTION_NAME[me]);
    return true;
}

static bool spyCapture(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int spyMe = me == F_CYBER ? U_C_SPY : U_K_SPY;
    int bar = firstBuildOf(foe) + BR_BARRACKS;
    Ref b = placeAt(bar, 1, mid);
    CHECK(b.valid(), "no spot for the barracks");
    Vec2 bp = g_sim.get(b)->pos;
    // ---- a structure guarded by a sniper: the sniper finds the stealthy infiltrator and shoots it, so it never finishes. (A tank, which cannot
    // see an infiltrator, would not have stopped it.)
    Ref spy = spawnAt(spyMe, 0, bp + Vec2(0, 6 * TILE));
    Ref guard = spawnAt(foe == F_CYBER ? U_C_SNIPER : U_K_SNIPER, 1, bp + Vec2(2 * TILE, 3 * TILE));
    Ref tank = spawnAt(firstUnitOf(foe) + 5, 1, bp + Vec2(-2 * TILE, 3 * TILE));
    g_sim.cmdAttack({spy}, b);
    CHECK(g_sim.get(spy)->order == O_CAPTURE, "a right-click on an enemy structure did not give the spy a capture order");
    run(40);
    CHECK(!alive(spy), "a spy walking past an enemy sniper survived");
    CHECK(g_sim.get(b)->owner == 1, "a dead spy captured a structure");
    g_sim.destroy(*g_sim.get(guard), false);
    {   // the tank alone: it never sees the infiltrator, which walks past it and takes the structure (then the scene is put back)
        Ref spyT = spawnAt(spyMe, 0, bp + Vec2(0, 6 * TILE));
        g_sim.cmdAttack({spyT}, b);
        run(g_sim.captureTime(*g_sim.get(b)) + 12);
        CHECK(alive(spyT), "a tank shot an infiltrator it cannot see");
        CHECK(g_sim.get(b)->owner == 0, "an infiltrator could not take a structure guarded only by a tank");
        g_sim.get(b)->owner = 1;
        if (Entity* x = g_sim.get(spyT)) g_sim.destroy(*x, false);
    }
    if (Entity* t = g_sim.get(tank)) g_sim.destroy(*t, false);
    g_sim.cmdTrain(b, firstUnitOf(foe) + 2);   // the enemy has a trooper in the queue (frozen at the start): capturing refunds it
    g_sim.get(b)->queueProgress = -100.0f;
    int enemyMoney = g_sim.players[1].money;
    // ---- an undefended one: it changes hands, the queue is refunded, the spy lives and the structure works for its new owner
    Ref spy2 = spawnAt(spyMe, 0, bp + Vec2(0, 6 * TILE));
    g_sim.cmdAttack({spy2}, b);
    run(g_sim.captureTime(*g_sim.get(b)) - 1.0f);
    CHECK(g_sim.get(b)->owner == 1, "the structure was captured before the spy had finished");
    run(14);
    CHECK(g_sim.get(b)->owner == 0, "a spy did not capture an undefended structure");
    CHECK(alive(spy2), "the spy did not survive the capture");
    CHECK(g_sim.get(b)->queue.empty(), "the old owner's production queue stayed in a captured structure");
    CHECK(g_sim.players[1].money >= enemyMoney + UNITS[firstUnitOf(foe) + 2].cost - 1, "the queued unit was not refunded to the old owner");
    int myTrooper = firstUnitOf(me) + 2;
    CHECK(g_sim.cmdTrain(b, myTrooper), "a captured barracks cannot train the new owner's units");
    CHECK(!g_sim.cmdTrain(b, firstUnitOf(foe) + 2), "a captured barracks trains the old owner's units");
    // the spy goes on to take a second one, and cannot take its own
    Ref b2 = placeAt(firstBuildOf(foe) + BR_POWER, 1, bp + Vec2(0, 10 * TILE));
    CHECK(b2.valid(), "no spot for a power plant");
    g_sim.cmdAttack({spy2}, b2); run(g_sim.captureTime(*g_sim.get(b2)) + 14);
    CHECK(g_sim.get(b2)->owner == 0, "a spy could not take a second structure");
    g_sim.cmdAttack({spy2}, b2);
    CHECK(g_sim.get(spy2)->order != O_CAPTURE, "a spy was ordered to capture its own structure");
    printf("stealthtest %s: spy capture ok\n", FACTION_NAME[me]);
    return true;
}

static bool bunkerTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bunkerType = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    int rifle = firstUnitOf(me) + 2, rpg = me == F_CYBER ? firstUnitOf(me) + 3 : U_K_INF2, dozerMe = firstUnitOf(me);
    int tankFoe = firstUnitOf(foe) + 5, vultureFoe = U_K_AIR, sdroneFoe = foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    if (foe == F_CYBER) vultureFoe = firstUnitOf(foe) + 8;
    Ref bk = placeAt(bunkerType, 0, mid);
    CHECK(bk.valid(), "no spot for the bunker");
    Entity* B = g_sim.get(bk);
    CHECK(B->hatch > 0 && B->hatch == B->hatchMax, "a finished bunker has no hatch");
    Vec2 bp = B->pos;
    CHECK(g_sim.visibleTo(*B, 1) && g_sim.visibleTo(*B, 0), "an underground bunker should be plainly visible to both sides on the surface");
    // ---- a crowd of soldiers and two dozers walk in (the bunker holds 200 and five: everyone gets in)
    std::vector<Ref> inf, ones;
    for (int i = 0; i < 52; i++) inf.push_back(spawnAt(i % 3 == 0 ? rpg : rifle, 0, bp + Vec2(((i % 9) - 4) * 22.0f, 3.5f * TILE + (i / 9) * 22.0f)));
    Ref dz1 = spawnAt(dozerMe, 0, bp + Vec2(-3 * TILE, 3 * TILE)), dz2 = spawnAt(dozerMe, 0, bp + Vec2(3 * TILE, 3 * TILE));
    std::vector<Ref> all = inf; all.push_back(dz1); all.push_back(dz2);
    g_sim.cmdEnter(all, bk);
    if (getenv("ONEHOUR_DBGB")) { for (int sec = 0; sec < 40; sec++) { run(1); int ent = 0, idl = 0, in = 0; for (Ref r : inf) if (alive(r)) { Entity* u = g_sim.get(r); if (u->carrier.valid()) in++; else if (u->order == O_ENTER) ent++; else idl++; } fprintf(stderr, "t=%d in %d entering %d idle %d\n", sec, in, ent, idl); if (sec >= 3 && sec <= 9) for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { Entity* u = g_sim.get(r); Entity* cc = g_sim.get(bk); if (sec >= 3) fprintf(stderr, "  stuck: path0 %.0f,%.0f order %d pos %.0f,%.0f dist %.1f path %zu/%zu actionTimer %.1f canBoard %d unloading %d constructed %d disabled %.1f fall %.1f time %.1f passable %d\n", u->path.empty() ? -1.0f : u->path[0].x, u->path.empty() ? -1.0f : u->path[0].y, (int)u->order, u->pos.x, u->pos.y, g_sim.distToEntity(u->pos, *cc), u->pathIdx, u->path.size(), u->actionTimer, (int)g_sim.canBoard(*u, *cc), (int)cc->unloading, (int)cc->constructed, u->disabledUntil, u->fall, g_sim.time, (int)g_map.passable(tileOf(u->pos.x), tileOf(u->pos.y))); break; } } }
    run(40);
    B = g_sim.get(bk);
    if (g_sim.garrisonInfantry(*B) != 52) { for (int dy = -6; dy <= 8; dy++) { for (int dx = -8; dx <= 8; dx++) { int x = tileOf(bp.x) + dx, y = tileOf(bp.y) + dy; fprintf(stderr, "%c", !inMap(x, y) ? '?' : (g_map.blocked[y * MAP_W + x] ? (char)('0' + g_map.blocked[y * MAP_W + x]) : '.')); } fprintf(stderr, "\n"); } int idle = 0, enter = 0; for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { if (g_sim.get(r)->order == O_ENTER) enter++; else idle++; } fprintf(stderr, "garrison %d, still entering %d, idle outside %d\n", g_sim.garrisonInfantry(*B), enter, idle); for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { Entity* u = g_sim.get(r); fprintf(stderr, "  outside unit order %d pos %.0f,%.0f dist to bunker %.1f (bunker %.0f,%.0f) canBoard %d\n", (int)u->order, u->pos.x, u->pos.y, g_sim.distToEntity(u->pos, *B), B->pos.x, B->pos.y, (int)g_sim.canBoard(*u, *B)); break; } }
    CHECK(g_sim.garrisonInfantry(*B) == 52, "52 soldiers walking in did not all get into the bunker");
    CHECK(g_sim.garrisonDozers(*B) == 2, "two dozers walking in did not both get into the bunker");
    int outside = 0; for (Ref r : all) if (alive(r) && !g_sim.get(r)->carrier.valid()) outside++;
    CHECK(outside == 0, "somebody was left outside a bunker with room to spare");
    {   // filled to the brim (put aboard directly): 200 soldiers and 5 dozers, and not one more
        for (int i = 52; i < BUNKER_INF_CAP; i++) { Ref r = spawnAt(rifle, 0, bp + Vec2(0, 4.0f * TILE)); Entity* u = g_sim.get(r); if (u && g_sim.canBoard(*u, *g_sim.get(bk))) g_sim.boardQuiet(*u, *g_sim.get(bk)); }
        for (int i = 2; i < BUNKER_DOZER_CAP; i++) { Ref r = spawnAt(dozerMe, 0, bp + Vec2(0, 4.0f * TILE)); Entity* u = g_sim.get(r); if (u && g_sim.canBoard(*u, *g_sim.get(bk))) g_sim.boardQuiet(*u, *g_sim.get(bk)); }
        Ref xs = spawnAt(rifle, 0, bp + Vec2(0, 4.0f * TILE)), xd = spawnAt(dozerMe, 0, bp + Vec2(TILE, 4.0f * TILE));
        B = g_sim.get(bk);
        CHECK(g_sim.garrisonInfantry(*B) == BUNKER_INF_CAP && BUNKER_INF_CAP == 200, "the bunker does not hold 200 soldiers");
        CHECK(g_sim.garrisonDozers(*B) == BUNKER_DOZER_CAP && BUNKER_DOZER_CAP == 5, "the bunker does not hold 5 dozers");
        CHECK(!g_sim.canBoard(*g_sim.get(xs), *B) && !g_sim.canBoard(*g_sim.get(xd), *B), "a full bunker took one more");
        g_sim.destroy(*g_sim.get(xs), false); g_sim.destroy(*g_sim.get(xd), false);
    }
    // ---- the garrison fires from the hatch at tanks and at aircraft (rocket troopers); the surface bunker stays visible
    Ref tk = spawnAt(tankFoe, 1, bp + Vec2(5 * TILE, 0));
    Ref air = spawnAt(vultureFoe, 1, bp + Vec2(-4 * TILE, 2 * TILE)); if (Entity* a = g_sim.get(air)) { a->alt = 1; a->order = O_IDLE; }
    float tk0 = hpOf(tk), air0 = hpOf(air);
    run(6);
    CHECK(!alive(tk) || hpOf(tk) < tk0 * 0.8f, "the garrison did not shoot an enemy tank");
    CHECK(!alive(air) || hpOf(air) < air0 * 0.9f, "the rocket troopers in the bunker did not shoot an aircraft");
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 1), "a bunker firing its guns should stay plainly visible");
    // ---- three direct nukes wreck the hatch and scrape the bunker, but the garrison inside is untouched (the hatch guns fall silent)
    Ref tk2 = spawnAt(tankFoe, 1, bp + Vec2(12 * TILE, 0));
    for (int k = 0; k < 3; k++) g_sim.nukes.push_back({g_sim.players[1].basePos, g_sim.get(bk)->pos, 1, Sim::NUKE_FLIGHT - 0.05f, false});
    run(0.2f); g_sim.fallouts.clear();
    CHECK(alive(bk), "three nukes destroyed an underground bunker outright");
    B = g_sim.get(bk);
    CHECK(B->hatch <= 0, "three direct nukes left the surface hatch intact");
    CHECK(B->hp < B->maxHp && B->hp > B->maxHp * 0.8f, "three nukes did not just scrape the bunker");
    CHECK(g_sim.garrisonInfantry(*B) == BUNKER_INF_CAP, "a nuke killed soldiers sheltering underground");
    Ref tk3 = spawnAt(tankFoe, 1, bp + Vec2(4 * TILE, 3 * TILE));
    {   // (the pop-up rocket pod still answers the tank: only the garrison and the hatch guns are silenced, so watch the hatch's own fire)
        bool fired = false;
        for (int t = 0; t < 8 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            Entity* bb = g_sim.get(bk);
            if (bb && bb->hatch <= 0 && (bb->cooldown2 > 0.55f || bb->gunCd[0] > WEAPONS[W_BUNKER_MINIGUN].cooldown * 0.5f)) fired = true;
        }
        CHECK(!fired, "a garrison fired with the hatch blown off");
    }
    (void)tk3;
    // ---- a dozer inside patches the hatch and the garrison is back in action
    run(30);
    B = g_sim.get(bk);
    CHECK(B->hatch > 0, "the dozer inside did not patch the hatch");
    Ref tk4 = spawnAt(tankFoe, 1, g_sim.get(bk)->pos + Vec2(3.5f * TILE, 2 * TILE));   // (the first one backed away from the unseen gunfire)
    float t31 = hpOf(tk4); run(10);
    CHECK(!alive(tk4) || hpOf(tk4) < t31, "the garrison did not resume fire after the hatch was patched");
    (void)tk2;
    // ---- a barrage of some 20-30 warheads destroys a bunker (nukes ignore the floor) and the garrison spills out
    {
        int shells = 0;
        while (alive(bk) && shells < 40) { g_sim.nukes.push_back({g_sim.players[1].basePos, g_sim.get(bk)->pos, 1, Sim::NUKE_FLIGHT - 0.05f, false}); run(0.2f); g_sim.fallouts.clear(); shells++; }
        CHECK(!alive(bk), "a long barrage of nukes could not destroy the bunker");
        if (shells < 18 || shells > 32) fprintf(stderr, "stealthtest: the bunker fell to warhead %d\n", shells);
        CHECK(shells >= 18 && shells <= 32, "a bunker did not take some 20-30 warheads to destroy");
    }
    int exposed = 0; for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) exposed++;
    CHECK(exposed >= 40, "the garrison did not spill out onto the ground when nukes destroyed the bunker");
    // ---- conventional ground troops can still grind a bunker down: a fresh bunker, tanks, and the garrison spills out exposed
    Ref bk2 = placeAt(bunkerType, 0, bp + Vec2(-22 * TILE, 0));
    CHECK(bk2.valid(), "no second spot for the tank-grind bunker");
    Entity* B2 = g_sim.get(bk2);
    B2->hp = B2->maxHp * BUNKER_FLOOR + 200.0f; B2->hatch = 0; B2->lastDamaged = g_sim.time;   // (pre-damaged near the floor so the grind fits the test)
    Ref sd = spawnAt(sdroneFoe, 1, bp + Vec2(-22 * TILE, 5 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    std::vector<Ref> tanks;
    for (int i = 0; i < 14; i++) tanks.push_back(spawnAt(tankFoe, 1, bp + Vec2(-22 * TILE + ((i % 7) - 3) * 30.0f, 4 * TILE + (i / 7) * 30.0f)));
    for (Ref t : tanks) if (alive(t)) { g_sim.get(t)->maxHp *= 10; g_sim.get(t)->hp = g_sim.get(t)->maxHp; }   // (the hatch gunfire would kill them: this test is about damage to the bunker)
    run(2);
    g_sim.cmdAttack(tanks, bk2);
    CHECK(g_sim.get(tanks[0])->order == O_ATTACK, "enemy tanks could not be ordered to attack a bunker a spy drone had revealed");
    bool dead = false;
    for (int t = 0; t < 20 * 400 && !dead; t++) { g_sim.step(); g_sim.events.clear(); if (t % 100 == 0 && !g_sim.get(sd)) { Ref nsd = spawnAt(sdroneFoe, 1, bp + Vec2(-22 * TILE, 5 * TILE)); sd = nsd; } dead = !alive(bk2); }
    CHECK(dead, "tanks could not destroy the bunker");
    printf("stealthtest %s: bunker ok (%d soldiers back on the ground)\n", FACTION_NAME[me], exposed);
    return true;
}

static bool bunkerCapture(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bunkerType = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref bk = placeAt(bunkerType, 1, mid);
    CHECK(bk.valid(), "no spot for the enemy bunker");
    Vec2 bp = g_sim.get(bk)->pos;
    std::vector<Ref> garrison;
    for (int i = 0; i < 12; i++) garrison.push_back(spawnAt(firstUnitOf(foe) + 2, 1, bp + Vec2((i - 6) * 12.0f, 3 * TILE)));
    g_sim.cmdEnter(garrison, bk); run(15);
    CHECK(g_sim.garrisonInfantry(*g_sim.get(bk)) == 12, "the enemy garrison did not climb in");
    int spyMe = me == F_CYBER ? U_C_SPY : U_K_SPY, sdroneMe = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    Ref spy = spawnAt(spyMe, 0, bp + Vec2(0, 8 * TILE));
    g_sim.cmdAttack({spy}, bk);
    CHECK(g_sim.get(spy)->order == O_CAPTURE, "a spy could not be ordered to capture a plainly visible bunker");
    Ref sd = spawnAt(sdroneMe, 0, bp + Vec2(0, 4 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 0), "the bunker is no longer visible to its attacker");
    // the garrison shoots the spy before it gets there: knock the hatch off first
    g_sim.get(bk)->hatch = 0; g_sim.get(bk)->lastDamaged = g_sim.time;
    run(g_sim.captureTime(*g_sim.get(bk)) + 16);
    CHECK(g_sim.get(bk)->owner == 0, "a spy did not capture a bunker");
    CHECK(g_sim.get(bk)->passengers.empty(), "the old garrison stayed in a captured bunker");
    int out = 0; for (Ref r : garrison) if (!alive(r) || (!g_sim.get(r)->carrier.valid() && g_sim.get(r)->owner == 1)) out++;   // (the captured bunker's miniguns may already have cut some of them down)
    CHECK(out == 12, "the old garrison did not spill out of a captured bunker");
    printf("stealthtest %s: bunker capture ok\n", FACTION_NAME[me]);
    return true;
}

static bool lifterTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int cargo = me == F_CYBER ? U_C_CARGO : U_K_CARGO;
    int rifle = firstUnitOf(me) + 2, tank = firstUnitOf(me) + 5, light = firstUnitOf(me) + 6, titan = firstUnitOf(me) + 10;
    Ref ch = spawnAt(cargo, 0, mid); if (Entity* c = g_sim.get(ch)) { c->alt = 1; }
    CHECK(UNITS[cargo].cargoCap == 40, "the lifter's capacity changed");
    CHECK(cargoSize(UNITS[rifle]) == 1 && cargoSize(UNITS[tank]) == 6 && cargoSize(UNITS[light]) == 4 && cargoSize(UNITS[titan]) == 10, "cargo sizes changed");
    Vec2 cp = g_sim.get(ch)->pos;
    std::vector<Ref> load;
    for (int i = 0; i < 30; i++) load.push_back(spawnAt(rifle, 0, cp + Vec2(((i % 6) - 3) * 22.0f, 2.5f * TILE + (i / 6) * 22.0f)));
    Ref tk = spawnAt(tank, 0, cp + Vec2(-4 * TILE, 3 * TILE)), lt = spawnAt(light, 0, cp + Vec2(4 * TILE, 3 * TILE));
    load.push_back(tk); load.push_back(lt);
    Ref extra = spawnAt(rifle, 0, cp + Vec2(0, 8 * TILE)), extraTank = spawnAt(tank, 0, cp + Vec2(5 * TILE, 8 * TILE));
    CHECK(g_sim.canBoard(*g_sim.get(tk), *g_sim.get(ch)), "a tank cannot board an empty lifter");
    std::vector<Ref> everyone = load; everyone.push_back(extra); everyone.push_back(extraTank);
    // the human way: select them and right-click the lifter
    g_sim.cmdEnter(everyone, ch);
    run(40);
    Entity* C = g_sim.get(ch);
    CHECK(g_sim.cargoUsed(*C) == 40, "the lifter should be exactly full: 30 soldiers + a tank + a light vehicle");
    CHECK(C->passengers.size() == 32, "the lifter carries the wrong number of passengers");
    int left = 0; for (Ref r : {extra, extraTank}) if (alive(r) && !g_sim.get(r)->carrier.valid()) left++;
    CHECK(left == 2, "a full lifter took more passengers than it has room for");
    for (Ref r : load) CHECK(!g_sim.get(r)->carrier.valid() ? false : true, "a passenger is not marked as aboard");
    // ---- fly them across the map and drop them off
    Vec2 dest = g_map.nearestFree(mid + Vec2(14 * TILE, -10 * TILE), 30);
    g_sim.cmdUnload(ch, true, dest);
    run(25);
    C = g_sim.get(ch);
    CHECK(alive(ch) && dist(C->pos, dest) < 40, "the lifter did not fly to the drop-off spot");
    run(25);
    C = g_sim.get(ch);
    CHECK(C->passengers.empty(), "the lifter did not unload everybody");
    for (Ref r : load) CHECK(alive(r) && !g_sim.get(r)->carrier.valid() && dist(g_sim.get(r)->pos, dest) < 9 * TILE, "a passenger did not come out at the drop-off spot");
    // ---- shot down in the air: everyone aboard is lost; shot down on the ground: they climb out
    std::vector<Ref> few; for (int i = 0; i < 5; i++) few.push_back(spawnAt(rifle, 0, C->pos + Vec2((i - 2) * 20.0f, 60)));
    g_sim.cmdEnter(few, ch); run(15);
    CHECK(g_sim.get(ch)->passengers.size() == 5, "five soldiers did not board again");
    g_sim.get(ch)->alt = 1.0f;
    g_sim.destroy(*g_sim.get(ch), true);
    int survivors = 0; for (Ref r : few) if (alive(r)) survivors++;
    CHECK(survivors == 0, "soldiers survived the crash of an airborne lifter");
    Ref ch2 = spawnAt(cargo, 0, mid + Vec2(-10 * TILE, 8 * TILE)); g_sim.get(ch2)->alt = 1;
    std::vector<Ref> few2; for (int i = 0; i < 5; i++) few2.push_back(spawnAt(rifle, 0, g_sim.get(ch2)->pos + Vec2((i - 2) * 20.0f, 60)));
    g_sim.cmdEnter(few2, ch2); run(15);
    g_sim.get(ch2)->alt = 0.0f;
    g_sim.destroy(*g_sim.get(ch2), true);
    survivors = 0; for (Ref r : few2) if (alive(r) && !g_sim.get(r)->carrier.valid()) survivors++;
    CHECK(survivors == 5, "soldiers aboard a lifter that went down on the ground did not climb out");
    // ---- unarmed enemy units cannot board
    Ref ch3 = spawnAt(cargo, 0, mid); Ref foeInf = spawnAt(firstUnitOf(foe) + 2, 1, g_sim.get(ch3)->pos);
    CHECK(!g_sim.canBoard(*g_sim.get(foeInf), *g_sim.get(ch3)), "an enemy soldier can board my lifter");
    printf("stealthtest %s: cargo lifter ok\n", FACTION_NAME[me]);
    return true;
}

static bool infantryTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int rpg = me == F_CLANKER ? U_K_INF2 : firstUnitOf(me) + 3;     // the rocket / laser anti-armour infantryman
    int tankFoe = firstUnitOf(foe) + 5;
    int heli = foe == F_CYBER ? (int)U_C_HELI : (int)U_K_AIR, wraith = firstUnitOf(foe) + 8, jet = firstUnitOf(foe) + 11;
    for (int kind = 0; kind < 4; kind++) {
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
        std::vector<Ref> troops;
        for (int i = 0; i < 4; i++) troops.push_back(spawnAt(rpg, 0, mid + Vec2(i * 18.0f, 0)));
        Vec2 at = mid + Vec2(3.5f * TILE, 0);
        Ref t = kind == 0 ? spawnAt(tankFoe, 1, at) : spawnAt(kind == 1 ? heli : (kind == 2 ? wraith : jet), 1, at);
        if (kind > 0) if (Entity* a = g_sim.get(t)) { a->alt = 1; a->pos = at; }
        float h0 = hpOf(t);
        run(12);
        const char* names[4] = { "tank", "helicopter", "bomber drone", "jet" };
        if (alive(t) && hpOf(t) >= h0) { fprintf(stderr, "stealthtest: four %s did not hurt a %s\n", UNITS[rpg].name, names[kind]); g_ok = false; return false; }
    }
    // ---- promotions: a soldier that kills four enemies becomes a veteran, and hits harder
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
    int rifle = firstUnitOf(me) + 2;
    Ref r = spawnAt(rifle, 0, mid);
    float base = g_sim.get(r)->maxHp;
    for (int i = 0; i < VET_KILLS; i++) {   // (a stunned enemy rifleman: it cannot fire back; the spies the scene used to use are stealthy now)
        Ref v = spawnAt(firstUnitOf(foe) + 2, 1, mid + Vec2(30, 0));
        if (Entity* ve = g_sim.get(v)) ve->disabledUntil = 1e9f;
        g_sim.cmdAttack({r}, v); run(12);
    }
    CHECK(alive(r) && g_sim.get(r)->rank >= 1, "four kills did not make a veteran");
    CHECK(g_sim.get(r)->maxHp > base, "a veteran is no tougher than a recruit");
    printf("stealthtest %s: rocket infantry and promotions ok\n", FACTION_NAME[me]);
    return true;
}

// The same things through the real input code: selecting, right-clicking, the command buttons and their hotkeys
bool uiNewTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    g_game.cancelModes(); g_game.selection.clear();
    g_game.cam = Vec2(clampf(mid.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(mid.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
    int bunkerType = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    auto labels = [&]() { g_game.buildButtons(); std::string l; for (auto& b : g_game.buttons) { l += b.label; l += "|"; } return l; };
    // ---- the dozer's build menu has the bunker, on its own hotkey
    Ref dz = spawnAt(firstUnitOf(me), 0, mid + Vec2(0, 5 * TILE));
    placeAt(firstBuildOf(me) + BR_BARRACKS, 0, mid + Vec2(0, 12 * TILE));   // (the bunker needs a Barracks)
    g_game.selection.push_back(dz);
    CHECK(labels().find(BUILDS[bunkerType].name) != std::string::npos, "the dozer's build menu has no bunker");
    g_game.hotkey(SDLK_u, 0);
    CHECK(g_game.placingType == bunkerType, "U did not pick the bunker from the dozer's build menu");
    g_game.cancelModes();
    // ---- right-click the bunker with soldiers selected: they climb in; a hidden enemy bunker cannot be picked
    Ref bk = placeAt(bunkerType, 0, mid);
    std::vector<Ref> men;
    for (int i = 0; i < 6; i++) men.push_back(spawnAt(firstUnitOf(me) + 2, 0, g_sim.get(bk)->pos + Vec2((i - 3) * 20.0f, 3 * TILE)));
    g_game.selection = men;
    g_game.issueRightClick(g_sim.get(bk)->pos);
    CHECK(g_sim.get(men[0])->order == O_ENTER, "a right-click on my bunker did not send the soldiers in");
    run(20);
    CHECK(g_sim.garrisonInfantry(*g_sim.get(bk)) == 6, "the soldiers did not climb into the bunker");
    g_game.cleanSelection();
    CHECK(g_game.selection.empty(), "soldiers inside a bunker stayed selected");
    g_game.selection.clear(); g_game.selection.push_back(bk);
    std::string l = labels();
    CHECK(l.find("Unload") != std::string::npos && l.find("Rally") != std::string::npos, "a selected bunker has no Unload button");
    g_game.hotkey(SDLK_u, 0);
    run(4);
    CHECK(g_sim.garrisonInfantry(*g_sim.get(bk)) == 0, "the Unload button did not let the garrison out");
    int bt2 = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref enemyBk = placeAt(bt2, 1, mid + Vec2(10 * TILE, 0));
    CHECK(enemyBk.valid(), "no spot for the enemy bunker");
    CHECK(g_game.pickEntity(g_sim.get(enemyBk)->pos, false) == g_sim.get(enemyBk), "a plainly visible enemy bunker cannot be clicked");
    int sdroneMe = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    Ref sd = spawnAt(sdroneMe, 0, g_sim.get(enemyBk)->pos + Vec2(0, 3 * TILE)); g_sim.get(sd)->alt = 1;
    run(1);
    CHECK(g_game.pickEntity(g_sim.get(enemyBk)->pos, false) != nullptr, "a bunker a spy drone found cannot be clicked");
    // ---- spies: right-click on an enemy structure = capture
    int spyMe = me == F_CYBER ? U_C_SPY : U_K_SPY;
    Ref spy = spawnAt(spyMe, 0, g_sim.get(enemyBk)->pos + Vec2(0, 5 * TILE));
    g_game.selection.clear(); g_game.selection.push_back(spy);
    g_game.issueRightClick(g_sim.get(enemyBk)->pos);
    CHECK(g_sim.get(spy)->order == O_CAPTURE, "a right-click on an enemy structure did not send the spy to capture it");
    // ---- a cargo lifter: soldiers board by right-click, the buttons unload and drop off
    int cargo = me == F_CYBER ? U_C_CARGO : U_K_CARGO;
    Ref ch = spawnAt(cargo, 0, mid + Vec2(-10 * TILE, 0)); g_sim.get(ch)->alt = 1;
    std::vector<Ref> crew;
    for (int i = 0; i < 4; i++) crew.push_back(spawnAt(firstUnitOf(me) + 2, 0, g_sim.get(ch)->pos + Vec2((i - 2) * 20.0f, 2 * TILE)));
    g_game.selection = crew;
    g_game.issueRightClick(g_sim.get(ch)->pos);
    run(15);
    CHECK(g_sim.get(ch)->passengers.size() == 4, "soldiers did not board the lifter after a right-click");
    g_game.selection.clear(); g_game.selection.push_back(ch);
    l = labels();
    CHECK(l.find("Unload") != std::string::npos && l.find("Drop Off") != std::string::npos, "a selected lifter has no Unload / Drop Off buttons");
    g_game.hotkey(SDLK_d, 0);
    CHECK(g_game.dropoffMode, "D did not start picking a drop-off spot");
    Vec2 spot = g_map.nearestFree(mid + Vec2(-10 * TILE, -6 * TILE), 20);
    Vec2 sc = g_game.worldToScreen(spot);
    g_game.mouseX = (int)sc.x; g_game.mouseY = (int)sc.y;
    SDL_Event ev; memset(&ev, 0, sizeof ev); ev.type = SDL_MOUSEBUTTONDOWN; ev.button.button = SDL_BUTTON_LEFT;
    g_game.gameEvent(ev);
    CHECK(!g_game.dropoffMode && g_sim.get(ch)->order == O_UNLOAD, "clicking the ground did not send the lifter to drop its troops");
    run(30);
    CHECK(g_sim.get(ch)->passengers.empty(), "the lifter did not unload at the chosen spot");
    g_game.selection.clear(); g_game.cancelModes();
    // ---- the tech structure panel: the airdrop names the load it will actually carry (with its tier) and the phone strike is there, ready
    Ref tech = placeAt(firstBuildOf(me) + BR_TECH, 0, mid + Vec2(0, -12 * TILE));
    CHECK(tech.valid(), "no spot for the tech structure");
    g_sim.players[0].dropReady = 0; g_sim.updatePowerPublic();
    g_game.selection.clear(); g_game.selection.push_back(tech); g_game.cancelModes();
    g_game.buildButtons();
    int sniperMe2 = me == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
    int heliT = me == F_CYBER ? U_C_HELI : U_K_AIR, jetT = me == F_CYBER ? U_C_JET : U_K_JET, droneT = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    bool sawDrop = false, sawStrike = false;
    for (auto& b : g_game.buttons) {
        if (std::string(DROPS[me].name) == b.label) {
            sawDrop = true;
            CHECK(b.enabled, "the airdrop button is not ready at a fresh tech structure");
            CHECK(b.tip.find("tier") != std::string::npos, "the airdrop tooltip does not name the drop tier");
            CHECK(b.tip.find(UNITS[sniperMe2].name) != std::string::npos, "the airdrop tooltip does not list the snipers it carries");
        }
        if (std::string(STRIKES[me].name) == b.label) {
            sawStrike = true;
            CHECK(b.enabled, "the phone strike button is not ready at a fresh tech structure");
            int strikeT = me == F_CYBER ? U_C_STRIKEJET : U_K_STRIKEJET;
            CHECK(b.tip.find(UNITS[strikeT].name) != std::string::npos, "the strike tooltip does not name its strike fighters");
            (void)heliT; (void)jetT; (void)droneT;
        }
    }
    CHECK(sawDrop && sawStrike, "the tech structure panel has no airdrop / phone strike button");
    // the I hotkey picks the strike and a click on the ground really calls the swarm in
    g_game.hotkey(SDLK_i, 0);
    CHECK(g_game.strikeMode, "I did not start picking a spot for the phone strike");
    int air0 = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) air0++;
    Vec2 callAt = g_map.nearestFree(mid + Vec2(6 * TILE, 0), 20);
    Vec2 cs = g_game.worldToScreen(callAt);
    g_game.mouseX = (int)cs.x; g_game.mouseY = (int)cs.y;
    SDL_Event ev2; memset(&ev2, 0, sizeof ev2); ev2.type = SDL_MOUSEBUTTONDOWN; ev2.button.button = SDL_BUTTON_LEFT;
    g_game.gameEvent(ev2);
    CHECK(!g_game.strikeMode, "clicking the ground did not call the phone strike in");
    for (int t = 0; t < 4; t++) { g_sim.step(); g_sim.events.clear(); }   // (the first wave comes in over the edge on the next tick)
    int air1 = 0; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.isAir()) air1++;
    CHECK(air1 > air0, "the phone strike put no aircraft in the air");
    g_game.selection.clear();
    printf("stealthtest %s: controls ok\n", FACTION_NAME[me]);
    return true;
}

// an army whose last things are hidden (snipers, a spy drone, a bunker and its garrison) is still in the game: the enemy has to find them. When the
// bunker falls its garrison comes out onto the ground and counts too; the army is out only when the last of it is dead
static bool lastHiddenTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE) g_sim.destroy(e, false);   // (the enemy's base is gone)
    Vec2 mid = openField();
    int sniperFoe = foe == F_CYBER ? U_C_SNIPER : U_K_SNIPER, sdroneFoe = foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE, bunkerFoe = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref tank = spawnAt(firstUnitOf(foe) + 5, 1, mid + Vec2(-200, 0));   // a visible unit keeps the army in the game
    Ref sn = spawnAt(sniperFoe, 1, mid), sd = spawnAt(sdroneFoe, 1, mid + Vec2(200, 0));
    Ref bk = placeAt(bunkerFoe, 1, mid + Vec2(0, 10 * TILE));
    CHECK(bk.valid(), "no spot for the bunker");
    std::vector<Ref> garrison; for (int i = 0; i < 4; i++) garrison.push_back(spawnAt(firstUnitOf(foe) + 2, 1, g_sim.get(bk)->pos + Vec2((i - 2) * 20.0f, 3 * TILE)));
    g_sim.cmdEnter(garrison, bk); run(12);
    CHECK(g_sim.garrisonInfantry(*g_sim.get(bk)) == 4, "the garrison did not get in");
    run(2); CHECK(g_sim.players[1].alive, "an army with a visible tank was eliminated");
    g_sim.destroy(*g_sim.get(tank), false);
    run(60);
    CHECK(g_sim.players[1].alive && !g_sim.gameOver, "a sniper, a spy drone and a bunker nobody had found ended the game");
    // the bunker falls: the garrison is out in the open, and the army still stands
    g_sim.destroy(*g_sim.get(bk), true);
    run(2);
    CHECK(g_sim.players[1].alive, "an army was eliminated while its garrison, sniper and drone still stood");
    int out = 0; for (Ref r : garrison) if (alive(r) && g_sim.get(r)->owner == 1 && !g_sim.get(r)->carrier.valid()) out++;
    CHECK(out == 4, "the garrison did not come out of a fallen bunker");
    for (Ref r : garrison) if (alive(r)) { g_sim.destroy(*g_sim.get(r), false); run(1); }
    run(2); CHECK(g_sim.players[1].alive, "an army with a sniper and a spy drone left was eliminated");
    g_sim.destroy(*g_sim.get(sn), false); run(1);
    CHECK(g_sim.players[1].alive, "an army with a spy drone left was eliminated");
    g_sim.destroy(*g_sim.get(sd), false);
    run(2);
    CHECK(!g_sim.players[1].alive && g_sim.gameOver, "an army with nothing left was not eliminated");
    printf("stealthtest %s: hidden last units ok\n", FACTION_NAME[me]);
    return true;
}

// aircraft cannot finish a bunker: once its hatch is gone and its body is at the floor they stop attacking it (they would hover over it doing nothing);
// while the hatch stands they still shoot it, and ground units can grind it below the floor
static bool bunkerFloorTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bunkerFoe = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER, sdroneMe = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    int gunship = me == F_CYBER ? U_C_HELI : U_K_AIR, tankMe = firstUnitOf(me) + 5;
    Ref bk = placeAt(bunkerFoe, 1, mid);
    CHECK(bk.valid(), "no spot for the bunker");
    Vec2 bp = g_sim.get(bk)->pos;
    Ref sd = spawnAt(sdroneMe, 0, bp + Vec2(0, 5 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    run(2);
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 0), "a spy drone did not reveal the bunker");
    // ---- a gunship ordered onto a bunker far outside its sight flies there (it must not turn back for its pad: helicopters never run dry)
    Ref far = spawnAt(gunship, 0, bp + Vec2(0, -30 * TILE)); if (Entity* a = g_sim.get(far)) { a->alt = 4; a->maxHp *= 20; a->hp = a->maxHp; }   // (the bunker's miniguns would shoot it down: this test is about its course)
    float far0 = dist(g_sim.get(far)->pos, bp);
    g_sim.cmdAttack({far}, bk);
    run(8);
    CHECK(alive(far) && dist(g_sim.get(far)->pos, bp) < far0 - 8 * TILE, "a gunship ordered onto a far bunker did not fly toward it");
    g_sim.destroy(*g_sim.get(far), false);
    // ---- the hatch still stands: the gunship keeps shooting it
    Ref heli = spawnAt(gunship, 0, bp + Vec2(0, -4 * TILE)); if (Entity* a = g_sim.get(heli)) { a->alt = 4; a->maxHp *= 20; a->hp = a->maxHp; }
    g_sim.cmdAttack({heli}, bk);
    CHECK(g_sim.get(heli)->order == O_ATTACK, "a gunship could not be ordered to attack a revealed bunker");
    Entity* B = g_sim.get(bk);
    B->hatch = 600; B->hp = B->maxHp * BUNKER_FLOOR;
    run(4);
    CHECK(g_sim.get(bk)->hatch < 600, "a gunship did not shoot the hatch of a bunker at its floor");
    // ---- the hatch is gone: the gunship gives up instead of hovering over the bunker
    B = g_sim.get(bk); B->hatch = 0; B->lastDamaged = g_sim.time; B->hp = B->maxHp * BUNKER_FLOOR;
    run(4);
    // (the contract is that the bunker stops being its target; with the field this close to a corner base the gunship may
    // legitimately pick a fight with the enemy HQ in its sight instead — that is not it hovering over the bunker)
    CHECK(alive(heli) && g_sim.get(heli)->targetEnt != bk, "a gunship kept attacking a bunker it can no longer hurt");
    B = g_sim.get(bk); B->hp = B->maxHp * BUNKER_FLOOR; B->lastDamaged = g_sim.time;   // (it mends a little: back at its floor, just hit)
    float floorHp = g_sim.get(bk)->hp;
    g_sim.cmdAttack({heli}, bk);
    run(2);
    CHECK(g_sim.get(heli)->targetEnt != bk, "a gunship was ordered onto a bunker it cannot hurt and stayed on it");
    CHECK(g_sim.get(bk)->hp >= floorHp, "a gunship damaged a bunker below its floor");
    // ---- tanks on the ground still grind the bunker below the floor (outpacing the trickle it mends under fire)
    std::vector<Ref> tanks;
    for (int k = 0; k < 4; k++) tanks.push_back(spawnAt(tankMe, 0, bp + Vec2(3 * TILE, (k - 1.5f) * TILE)));
    g_sim.cmdAttack(tanks, bk);
    run(12);
    CHECK(!alive(bk) || g_sim.get(bk)->hp < floorHp, "a tank could not hurt a bunker at its floor");
    printf("stealthtest %s: bunker floor ok\n", FACTION_NAME[me]);
    return true;
}

// a bunker stands alone: no power, no other structure, no supplies, nobody inside. It earns money, mends itself, shoots with its two miniguns
// (ground and air) and launches a small nuke (a fifth of a full warhead's area)
static bool bunkerSelfTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bunkerType = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref bk = placeAt(bunkerType, 0, mid);
    CHECK(bk.valid(), "no spot for the bunker");
    CHECK(BUILDS[bunkerType].cost == 7500, "a bunker does not cost 7500");
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.kind != EK_RESOURCE && g_sim.refOf(e) != bk) g_sim.destroy(e, false);   // (nothing else of the army stands: no power, no ramp, no supply)
    Entity* B = g_sim.get(bk);
    Vec2 bp = B->pos;
    CHECK(g_sim.countRole(0, BR_NUKE, false) == 0 && g_sim.players[0].powerMade == 0, "the scene still has a power plant or a nuke ramp");
    // ---- income: a small payout every few seconds, with no power at all
    g_sim.players[0].money = 0; g_sim.players[0].mined = 0;
    run(60);
    int earned = g_sim.players[0].money;
    CHECK(earned >= BUNKER_INCOME * 11 && earned <= BUNKER_INCOME * 13, "a lone bunker did not earn about one payout every five seconds");
    CHECK(g_sim.players[0].mined == earned, "a bunker's income is not counted as mined money");
    CHECK(BUNKER_INCOME < INCOME_CLANKER / 4, "a bunker should earn only a little");
    // ---- it mends itself: a little while it is being hit (BUNKER_REGEN_HOT), then fast once left alone
    B->hp = B->maxHp * 0.5f; B->lastDamaged = g_sim.time;
    run(BUNKER_REGEN_DELAY - 1.0f);
    float hot = (B->hp - B->maxHp * 0.5f) / B->maxHp;
    CHECK(hot > 0 && hot * B->maxHp < BUNKER_REGEN_HOT * (BUNKER_REGEN_DELAY - 1.0f) * 1.5f, "a bunker under fire did not mend at its reduced rate");
    run(21);
    float gain = (B->hp - B->maxHp * 0.5f) / B->maxHp;
    CHECK(gain > BUNKER_REGEN * 16 && gain < BUNKER_REGEN * 22, "a bunker did not mend by itself at its own rate");
    B->hatch = 1000; run(10);
    CHECK(B->hatch > 1000 + B->hatchMax * BUNKER_HATCH_REGEN * 8 && B->hatch < 1000 + B->hatchMax * BUNKER_HATCH_REGEN * 12, "a damaged hatch did not mend by itself");
    // ---- a blown hatch silences the miniguns; the bunker refits it after a while without any help
    B->hatch = 0; B->lastDamaged = g_sim.time;
    Ref tkQuiet = spawnAt(firstUnitOf(foe) + 5, 1, bp + Vec2(4 * TILE, 0));
    if (Entity* tq = g_sim.get(tkQuiet)) tq->disabledUntil = 1e9f;   // (the bunker is visible now: keep the idle tank from starting a fight, this check is about the silent hatch)
    float q0 = hpOf(tkQuiet);
    run(BUNKER_HATCH_REFIT - 4.0f);
    CHECK(B->hatch == 0 && hpOf(tkQuiet) == q0, "a bunker with its hatch blown off fired or refitted at once");
    g_sim.destroy(*g_sim.get(tkQuiet), false);
    run(8);
    CHECK(B->hatch > 0, "a bunker never refitted its blown hatch");
    // ---- two miniguns: ground (tank, infantry) and air, each gun on its own target, and the bunker is plainly visible
    B->hatch = B->hatchMax;
    int tankFoe = firstUnitOf(foe) + 5, rifleFoe = firstUnitOf(foe) + 2, airFoe = foe == F_CYBER ? firstUnitOf(foe) + 8 : (int)U_K_AIR;
    Ref tk = spawnAt(tankFoe, 1, bp + Vec2(5 * TILE, 0)), tk2 = spawnAt(tankFoe, 1, bp + Vec2(-5 * TILE, 0));
    Ref far = spawnAt(tankFoe, 1, bp + Vec2(0, 11 * TILE));
    Ref air = spawnAt(airFoe, 1, bp + Vec2(0, -4 * TILE)); if (Entity* a = g_sim.get(air)) { a->alt = 1; a->order = O_IDLE; }
    Ref inf = spawnAt(rifleFoe, 1, bp + Vec2(3 * TILE, 3 * TILE));
    for (Ref r : {tk, tk2, far}) if (Entity* a = g_sim.get(r)) { a->maxHp *= 10; a->hp = a->maxHp; }
    if (Entity* a = g_sim.get(far)) a->disabledUntil = g_sim.time + 30;   // (held where it stands: left alone it drives in on the bunker, into the guns' range)
    float tk0 = hpOf(tk), tk20 = hpOf(tk2), far0 = hpOf(far), air0 = hpOf(air);
    run(0.5f);
    CHECK(B->gunTgt[0].valid() && B->gunTgt[1].valid(), "the two miniguns did not both pick a target");
    run(4);
    CHECK(!alive(inf), "the miniguns did not kill an infantryman");
    CHECK(hpOf(tk) < tk0 || hpOf(tk2) < tk20, "the miniguns did not shoot a tank");
    CHECK(!alive(air) || hpOf(air) < air0 * 0.9f, "the miniguns did not shoot an aircraft");
    if (getenv("ONEHOUR_DEBUG")) { Entity* f = g_sim.get(far); fprintf(stderr, "far tank: hp %.1f of %.1f, %.1f tiles from the bunker, attacker %d\n", hpOf(far), far0, f ? dist(f->pos, bp) / TILE : -1.0f, f && g_sim.get(f->attacker) ? g_sim.get(f->attacker)->type : -1); }
    CHECK(hpOf(far) == far0, "the miniguns shot a tank beyond their range");
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 1), "the bunker should remain visible while its miniguns fire");
    for (Ref r : {tk, tk2, far, air, inf}) if (alive(r)) g_sim.destroy(*g_sim.get(r), false);
    g_sim.fx.clear();
    // ---- the mini nuke: arming time, then a blast of a fifth of the area, a cooldown, and the launch gives the bunker away for a few seconds
    B = g_sim.get(bk);
    B->siloTimer = g_sim.time + MINI_NUKE_ARM;   // (as a bunker that has just been finished)
    CHECK(g_sim.miniNukesReady(0) == 0 && !g_sim.cmdMiniNuke(0, bp + Vec2(20 * TILE, 0)), "a bunker launched before its missile was armed");
    run(MINI_NUKE_ARM + 0.5f);
    CHECK(g_sim.miniNukesReady(0) == 1 && g_sim.miniNukeWait(0) == 0, "the bunker's missile never became ready");
    Vec2 gz = bp + Vec2(18 * TILE, 0);
    {   // open ground for the scene: ground zero and the three tanks east of it each stand where they were put (the ground near the bunker may be rough)
        auto open = [](Vec2 p) { return inMap(tileOf(p.x), tileOf(p.y)) && dist(g_map.nearestFree(p, 30), p) < TILE; };
        float xs[4] = { 0.0f, 0.7f * NUKE_RADIUS * TILE * std::sqrt(MINI_NUKE_SCALE), 1.25f * NUKE_RADIUS * TILE * std::sqrt(MINI_NUKE_SCALE) + 40.0f, 0.7f * NUKE_RADIUS * TILE };
        for (int k = 0; k < 16; k++) {
            float a = k * 0.3927f; Vec2 c = bp + Vec2(std::cos(a), std::sin(a)) * (18.0f * TILE);
            bool ok = true; for (float x : xs) if (!open(c + Vec2(x, 0))) ok = false;
            if (ok) { gz = c; break; }
        }
    }
    Ref centre = spawnAt(tankFoe, 1, gz), rim = spawnAt(tankFoe, 1, gz + Vec2(0.7f * NUKE_RADIUS * TILE * std::sqrt(MINI_NUKE_SCALE), 0));
    Ref outside = spawnAt(tankFoe, 1, gz + Vec2(1.25f * NUKE_RADIUS * TILE * std::sqrt(MINI_NUKE_SCALE) + 40.0f, 0));
    Ref wide = spawnAt(tankFoe, 1, gz + Vec2(0.7f * NUKE_RADIUS * TILE, 0));   // inside a full warhead's circle, far outside the small one
    for (Ref r : {centre, rim, outside, wide}) if (Entity* a = g_sim.get(r)) a->disabledUntil = g_sim.time + 30;   // (held where they stand: this is about the blast's reach, not about running from it)
    CHECK(!g_sim.cmdMiniNuke(1, gz), "an army without a bunker launched a bunker missile");
    CHECK(g_sim.cmdMiniNuke(0, gz), "a lone bunker could not launch its missile");
    CHECK(g_sim.nukes.size() == 1 && g_sim.nukes[0].scale == MINI_NUKE_SCALE && Sim::nukeRadius(g_sim.nukes[0]) < NUKE_RADIUS * TILE * 0.5f, "the bunker's missile is not a small one");
    CHECK(g_sim.miniNukesReady(0) == 0 && g_sim.miniNukeWait(0) > MINI_NUKE_COOLDOWN - 1.0f && !g_sim.cmdMiniNuke(0, gz), "a bunker launched twice with no cooldown");
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 1), "a launch did not give the bunker away");
    // the salvo: the same missile again, one gap later, at the same spot and scale
    run(MINI_NUKE_SALVO_GAP + 0.1f);
    CHECK(g_sim.nukes.size() == 2 && dist(g_sim.nukes[1].pos, g_sim.nukes[0].pos) < 1.0f && g_sim.nukes[1].scale == MINI_NUKE_SCALE, "a bunker's salvo did not follow with a second missile at the same spot");
    run(Sim::NUKE_FLIGHT + 1.0f + MINI_NUKE_SALVO_GAP);   // (both missiles land)
    CHECK(!alive(centre), "a mini nuke did not destroy a tank at ground zero");
    if (getenv("ONEHOUR_DEBUG") && alive(rim)) fprintf(stderr, "rim tank: hp %.0f of %.0f, %.1f tiles from ground zero (circle %.1f)\n", hpOf(rim), g_sim.get(rim)->maxHp, dist(g_sim.get(rim)->pos, gz) / TILE, NUKE_RADIUS * std::sqrt(MINI_NUKE_SCALE));
    CHECK(!alive(rim) || hpOf(rim) < g_sim.get(rim)->maxHp * 0.5f, "a mini nuke did not reach the inside of its circle");
    CHECK(alive(outside) && alive(wide), "a mini nuke blasted outside its small circle");
    CHECK(hpOf(wide) == g_sim.get(wide)->maxHp, "a mini nuke hurt a tank outside its small circle");
    g_sim.fallouts.clear();
    run(MINI_NUKE_REVEAL);
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 1), "the bunker must stay visible: underground bunkers are no longer stealthy");
    CHECK(alive(bk) && g_sim.miniNukesReady(0) == 0, "the bunker is gone or reloaded too early");
    run(MINI_NUKE_COOLDOWN);
    CHECK(g_sim.miniNukesReady(0) == 1, "the bunker's missile did not reload");
    // ---- a full-size warhead covers what the small one did not
    g_sim.nukes.push_back({bp, gz, 0, Sim::NUKE_FLIGHT - 0.05f, false});   // (a computer army's nuke spares its own side: use the human's)
    Ref wide2 = spawnAt(tankFoe, 1, gz + Vec2(0.7f * NUKE_RADIUS * TILE, 0));
    run(0.3f);
    CHECK(!alive(wide2) || hpOf(wide2) < g_sim.get(wide2)->maxHp * 0.9f, "a full nuke did not reach 7.7 tiles from ground zero");
    printf("stealthtest %s: bunker income, self-repair, miniguns and mini nuke ok\n", FACTION_NAME[me]);
    return true;
}

// a lone bunker of player 0 in a clean field that the enemy can see
static Ref retaliationScene(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Ref bk = placeAt(me == F_CYBER ? B_C_BUNKER : B_K_BUNKER, 0, openField());
    if (!bk.valid()) return NOREF;
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.kind != EK_RESOURCE && g_sim.refOf(e) != bk) g_sim.destroy(e, false);
    g_sim.get(bk)->detUntil[1] = g_sim.time + 1000;
    return bk;
}

// a bunker that is being worn down answers: a rocket pod pops up (with its hatch blown off too, and beyond its own sight) and fires at whoever is
// attacking it, never at one that is not; while the hatch stands its guns swat the rockets aimed at it (BUNKER_CIWS_CHANCE)
static bool bunkerRetaliateTest(Faction me, Faction foe, u64 seed) {
    int tankFoe = firstUnitOf(foe) + 5, longFoe = firstUnitOf(foe) + 7;   // a tank, and the ground unit with the longest reach (rocket launcher or railgun)
    float longReach = std::min(9.5f, WEAPONS[UNITS[longFoe].weapon].range - 0.3f);   // tiles: 9.5 for a rocket launcher, 8.2 for a railgun (beyond the bunker's 8 tile sight)
    // ---- (a) a tank attacking the bunker from 6 tiles, its hatch blown off: the pod shoots back
    Ref bk = retaliationScene(me, foe, seed);
    CHECK(bk.valid(), "no spot for the bunker");
    Vec2 bp = g_sim.get(bk)->pos;
    g_sim.get(bk)->hatch = 0; g_sim.get(bk)->lastDamaged = g_sim.time;
    Ref tk = spawnAt(tankFoe, 1, bp + Vec2(6 * TILE, 0));
    CHECK(tk.valid(), "no spot for the attacking tank");
    g_sim.get(tk)->maxHp *= 10; g_sim.get(tk)->hp = g_sim.get(tk)->maxHp;
    float tk0 = hpOf(tk);
    g_sim.cmdAttack({tk}, bk);
    bool shot = false;
    for (int t = 0; t < 60 && !shot; t++) { run(0.1f); shot = !alive(tk) || hpOf(tk) < tk0; }
    CHECK(shot, "the pod did not shoot a tank attacking the bunker with its hatch blown off");
    CHECK(g_sim.get(bk) && g_sim.get(bk)->hatch == 0, "the hatch came back during the fight");
    // ---- (b) an idle tank at the same distance, not attacking: the pod leaves it alone while the bunker is under fire from somewhere else.
    // (The bunker is kept hidden from the tank: a tank that can see a bunker 6 tiles away picks a fight with it on its own.)
    bk = retaliationScene(me, foe, seed + 1);
    CHECK(bk.valid(), "no spot for the bunker");
    bp = g_sim.get(bk)->pos;
    g_sim.get(bk)->hatch = 0; g_sim.get(bk)->detUntil[1] = 0;
    Ref idle = spawnAt(tankFoe, 1, bp + Vec2(6 * TILE, 0));
    CHECK(idle.valid(), "no spot for the idle tank");
    g_sim.get(idle)->maxHp *= 10; g_sim.get(idle)->hp = g_sim.get(idle)->maxHp;
    if (Entity* it = g_sim.get(idle)) it->disabledUntil = 1e9f;   // (the bunker is visible now: keep the idle tank from picking a fight on its own)
    float idle0 = hpOf(idle);
    for (int t = 0; t < 60; t++) {
        if (Entity* it = g_sim.get(idle)) { it->targetEnt = NOREF; it->engaged = NOREF; it->forceTarget = NOREF; it->disabledUntil = 1e9f; }
        if (Entity* bb = g_sim.get(bk)) bb->attacker = NOREF;
        g_sim.get(bk)->lastDamaged = g_sim.time; run(0.1f);
    }
    if (Entity* it = g_sim.get(idle)) { it->targetEnt = NOREF; it->engaged = NOREF; }
    CHECK(alive(idle) && hpOf(idle) == idle0, "the pod shot a tank that was not attacking the bunker");
    CHECK(g_sim.get(idle)->targetEnt != bk && g_sim.get(idle)->engaged != bk, "an idle tank went for the bunker on its own");
    // ---- (c) a long-range attacker just beyond the bunker's sight: the pod reaches it
    bk = retaliationScene(me, foe, seed + 2);
    CHECK(bk.valid(), "no spot for the bunker");
    bp = g_sim.get(bk)->pos;
    flattenArena(bp, 13, 13);   // the fixture measures retaliation range, not terrain relocation
    g_sim.get(bk)->hatch = 0; g_sim.get(bk)->lastDamaged = g_sim.time;
    Vec2 lrAt = bp + Vec2(longReach * TILE, 0);   // (the first free spot on the ring at exactly that reach: a landmark or a pile may stand east of the bunker)
    for (int k = 0; k < 16; k++) { Vec2 q = bp + Vec2(std::cos(k * 0.3927f), std::sin(k * 0.3927f)) * (longReach * TILE); if (g_map.passable(tileOf(q.x), tileOf(q.y))) { lrAt = q; break; } }
    Ref lr = spawnAt(longFoe, 1, lrAt);
    CHECK(lr.valid(), "no spot for the long-range attacker");
    g_sim.get(lr)->maxHp *= 10; g_sim.get(lr)->hp = g_sim.get(lr)->maxHp;
    float lr0 = hpOf(lr), lrDist = dist(g_sim.get(lr)->pos, bp) / TILE;
    CHECK(lrDist > 8.0f && lrDist < 10.5f, "the long-range attacker did not stand between the bunker's sight and the pod's reach");
    g_sim.cmdAttack({lr}, bk);
    bool lrShot = false;
    for (int t = 0; t < 80 && !lrShot; t++) { run(0.1f); g_sim.get(bk)->lastDamaged = g_sim.time; lrShot = !alive(lr) || hpOf(lr) < lr0; }
    CHECK(lrShot, "the pod did not shoot a long-range attacker beyond the bunker's sight");
    // ---- (d) the hatch guns swat rockets aimed at the bunker: about BUNKER_CIWS_CHANCE of them, one roll per rocket
    bk = retaliationScene(me, foe, seed + 3);
    CHECK(bk.valid(), "no spot for the bunker");
    bp = g_sim.get(bk)->pos;
    int swatted = 0, struck = 0, volleys = 40;
    for (int k = 0; k < volleys; k++) {
        Entity* B = g_sim.get(bk);
        B->hatch = B->hatchMax; B->lastDamaged = g_sim.time;
        float h0 = B->hatch;
        Projectile p;   // a rocket of the Clanker RPG, three tiles out and coming straight in
        p.pos = bp + Vec2(3.0f * TILE, 0); p.prevPos = p.pos; p.vel = Vec2(-WEAPONS[10].projSpeed, 0); p.dest = bp;
        p.target = bk; p.weapon = 10; p.owner = 1; p.shooter = NOREF; p.life = 3.0f;
        g_sim.projs.push_back(p);
        run(1.0f);
        if (g_sim.get(bk)->hatch < h0) struck++; else swatted++;
    }
    float share = swatted / (float)volleys;
    CHECK(struck + swatted == volleys, "a rocket aimed at the bunker neither hit it nor was swatted");
    CHECK(share >= 0.3f && share <= 0.8f, "the hatch guns did not swat about BUNKER_CIWS_CHANCE of the rockets aimed at the bunker");
    printf("stealthtest %s: bunker retaliation ok (a tank attacking from 6 tiles was shot at, an idle one left alone, a %.1f tile attacker beyond sight too, %d of %d rockets swatted)\n",
           FACTION_NAME[me], lrDist, swatted, volleys);
    return true;
}

// the computer commander hunts: the human has nothing left but a hidden bunker, so the computer army sweeps the ground around its base until its
// spy drone finds the bunker, then destroys it and the human is out
static bool huntTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);   // player 0 is the human, player 1 the computer
    g_ai.init(seed);
    for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.kind != EK_RESOURCE) g_sim.destroy(e, false);   // (the human's base is gone)
    int bunkerMe = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref bk = placeAt(bunkerMe, 0, g_sim.players[0].basePos);
    CHECK(bk.valid(), "no spot for the human's bunker");
    Entity* B = g_sim.get(bk);
    B->hatch = 0; B->lastDamaged = g_sim.time; B->hp = B->maxHp * BUNKER_FLOOR + 200.0f;   // (nearly down already: this test is about the search, not the siege)
    int tankFoe = firstUnitOf(foe) + 5, sdroneFoe = foe == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    for (int i = 0; i < 4; i++) spawnAt(tankFoe, 1, g_sim.players[1].basePos + Vec2(i * 30.0f, 4 * TILE));
    Ref sd = spawnAt(sdroneFoe, 1, g_sim.players[1].basePos + Vec2(0, 6 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    bool revealed = false, dead = false;
    for (int t = 0; t < 20 * 900 && !dead; t++) {
        g_sim.step(); g_ai.update(); g_sim.events.clear();
        if (!alive(bk) || g_sim.get(bk)->owner != 0) { dead = true; break; }   // destroyed, or taken by an infiltrator (the town infiltrators are free once the towns are held)
        if (g_sim.visibleTo(*g_sim.get(bk), 1)) revealed = true;
    }
    CHECK(revealed, "the computer's army never found the human's hidden bunker");
    CHECK(dead, "the computer never destroyed or took the hidden bunker it had found");
    run(2);
    CHECK(!g_sim.players[0].alive && g_sim.gameOver, "the human was not out once the hidden bunker fell");
    printf("stealthtest %s: computer hunt ok\n", FACTION_NAME[me]);
    return true;
}

// surrounded bunker special abilities: automatic radiation spreading when hit hard, overdrive lasers/heavy MGs shooting
// aircraft, tanks, and infantry, and intercepting incoming nuclear warheads in flight
static bool bunkerOverdriveAndRadTest(Faction me, Faction foe, u64 seed) {
    Ref bk = retaliationScene(me, foe, seed);
    CHECK(bk.valid(), "no spot for the bunker");
    Vec2 bp = g_sim.get(bk)->pos;
    int infFoe = firstUnitOf(foe), tankFoe = firstUnitOf(foe) + 5, heliFoe = foe == F_CYBER ? U_C_HELI : U_K_AIR;
    int infMe = me == F_CYBER ? U_C_SNIPER : U_K_SNIPER;
    Ref ally = spawnAt(infMe, 0, bp + Vec2(0, 3.0f * TILE));
    Ref f1 = spawnAt(tankFoe, 1, bp + Vec2(4.0f * TILE, 0));
    Ref f2 = spawnAt(tankFoe, 1, bp + Vec2(-4.0f * TILE, 0));
    Ref f3 = spawnAt(infFoe, 1, bp + Vec2(0, -4.0f * TILE));
    Ref air = spawnAt(heliFoe, 1, bp + Vec2(3.5f * TILE, 3.5f * TILE));
    if (Entity* a = g_sim.get(air)) a->alt = 1.0f;
    for (Ref r : {f1, f2, f3, air}) if (Entity* e = g_sim.get(r)) { e->maxHp *= 6; e->hp = e->maxHp; }
    float airHp0 = hpOf(air), f1Hp0 = hpOf(f1), allyHp0 = hpOf(ally);
    g_sim.cmdAttack({f1, f2, f3}, bk);
    // Deal heavy hits while surrounded so automatic radiation triggers
    g_sim.applyDamagePublic(*g_sim.get(bk), 1800.0f, 1, f1);
    run(2.5f);
    Entity* B = g_sim.get(bk);
    CHECK(B && B->surrounded, "bunker surrounded state did not engage with 4 enemies around it");
    bool hasBunkerRad = false;
    for (auto& f : g_sim.fallouts) if (f.enemyOnly && f.owner == 0 && dist(f.pos, bp) < TILE) hasBunkerRad = true;
    CHECK(hasBunkerRad, "surrounded bunker under heavy fire did not spread automatic defensive radiation");
    CHECK(hpOf(ally) >= allyHp0 - 1.0f, "bunker defensive radiation hurt a friendly unit");
    CHECK(hpOf(air) < airHp0 && hpOf(f1) < f1Hp0, "surrounded bunker overdrive weapons did not hit both aircraft and ground forces");
    // Launch an enemy nuclear warhead at the surrounded bunker and verify it is intercepted in flight
    B->antiNukeCd = 0;
    Sim::Nuke nk; nk.from = bp + Vec2(30 * TILE, 0); nk.pos = bp; nk.owner = 1; nk.t = Sim::NUKE_FLIGHT - 3.5f; nk.id = ++g_sim.nukeSeq;
    g_sim.nukes.push_back(nk);
    run(1.5f);
    bool intercepted = g_sim.nukes.empty() || g_sim.nukes[0].killT >= 0 || g_sim.nukes[0].broken;
    CHECK(intercepted, "surrounded bunker did not shoot down the incoming nuclear warhead");
    printf("stealthtest %s: surrounded bunker radiation, overdrive & anti-nuke ok\n", FACTION_NAME[me]);
    return true;
}

// creative AI bunker placement: never next to the player's bunker, picks flanking/cover positions
static bool creativeAiBunkerTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    g_ai.init(seed);
    int bkMe = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    int bkFoe = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    int turFoe = foe == F_CYBER ? B_C_LASER : B_K_MG;
    Vec2 mid(WORLD_W * 0.5f, WORLD_H * 0.5f);
    Ref playerBk = placeAt(bkMe, 0, g_sim.players[1].basePos + (mid - g_sim.players[1].basePos) * 0.35f);
    CHECK(playerBk.valid(), "no spot for player bunker");
    Vec2 pbp = g_sim.get(playerBk)->pos;
    int tx = -1, ty = -1;
    CHECK(g_ai.ais[1].findBunkerSpot(bkFoe, tx, ty), "AI could not find a creative bunker spot");
    Vec2 aiBkPos = g_sim.buildingCenter(bkFoe, tx, ty);
    CHECK(dist(aiBkPos, pbp) >= 22.0f * TILE, "AI placed its bunker too close to the player's bunker");
    int ttx = -1, tty = -1;
    if (g_ai.ais[1].findSpot(turFoe, pbp, 14, ttx, tty)) {
        Vec2 turPos = g_sim.buildingCenter(turFoe, ttx, tty);
        CHECK(dist(turPos, pbp) >= 16.0f * TILE, "AI placed a turret next to the player's bunker");
    }
    printf("stealthtest %s: creative AI bunker placement ok (%.1f tiles from player bunker)\n", FACTION_NAME[me], dist(aiBkPos, pbp) / TILE);
    return true;
}

// autonomous Ghost/Recon Drone swarm intelligence for human and AI players
static bool scoutSwarmAutonomousTest(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    int sdroneMe = me == F_CYBER ? U_C_SDRONE : U_K_SDRONE;
    int bkFoe = foe == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Vec2 start = g_sim.players[0].basePos;
    Vec2 toMid = (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - start).norm();
    Vec2 side(-toMid.y, toMid.x);
    Ref d1 = spawnAt(sdroneMe, 0, start);
    Ref d2 = spawnAt(sdroneMe, 0, start + side * 24.0f);
    Ref d3 = spawnAt(sdroneMe, 0, start - side * 24.0f);
    for (Ref r : {d1, d2, d3}) if (Entity* d = g_sim.get(r)) { d->alt = 1.0f; d->autoHoldUntil = g_sim.time + 20.0f; }
    // Call d1 manually to a spot and verify it goes there and waits for SCOUT_MANUAL_HOLD
    Vec2 callSpot = start + toMid * (7.0f * TILE) + side * (2.0f * TILE);
    g_sim.cmdMove({d1}, callSpot, false);
    run(3.0f);
    CHECK(dist(g_sim.get(d1)->pos, callSpot) < 2.5f * TILE, "Ghost Drone did not fly to the spot the player called it to");
    CHECK(g_sim.get(d1)->autoHoldUntil > g_sim.time, "Ghost Drone did not hold at the player's called spot");
    // Place a hidden enemy bunker within swarm search range and expire hold timers so the swarm hunts autonomously
    Ref enemyBk = placeAt(bkFoe, 1, start + toMid * (18.0f * TILE));
    CHECK(enemyBk.valid(), "no spot for hidden enemy bunker");
    for (Ref r : {d1, d2, d3}) if (Entity* d = g_sim.get(r)) { d->autoHoldUntil = 0; d->order = O_IDLE; d->autoTask = false; }
    bool foundHidden = false;
    for (int t = 0; t < 160 && !foundHidden; t++) {
        run(0.25f);
        if (g_sim.visibleTo(*g_sim.get(enemyBk), 0)) foundHidden = true;
    }
    CHECK(foundHidden, "autonomous player Ghost/Recon Drone swarm did not hunt down and reveal the hidden enemy bunker");
    float spread = dist(g_sim.get(d1)->pos, g_sim.get(d2)->pos) + dist(g_sim.get(d2)->pos, g_sim.get(d3)->pos);
    CHECK(spread > 8.0f * TILE, "Ghost/Recon Drone swarm clumped together instead of coordinating sectors");
    printf("stealthtest %s: autonomous Ghost/Recon Drone swarm ok (spread %.1f tiles)\n", FACTION_NAME[me], spread / TILE);
    return true;
}

bool stealthTest(u64 seed) {
    g_map.generate();
    g_ok = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        if (!stealthSniperAndDrone(me, foe, seed + fi)) return false;
        if (!spyCapture(me, foe, seed + 10 + fi)) return false;
        if (!bunkerTest(me, foe, seed + 20 + fi)) return false;
        if (!bunkerCapture(me, foe, seed + 30 + fi)) return false;
        if (!lifterTest(me, foe, seed + 40 + fi)) return false;
        if (!infantryTest(me, foe, seed + 50 + fi)) return false;
        if (!uiNewTest(me, foe, seed + 60 + fi)) return false;
        if (!lastHiddenTest(me, foe, seed + 70 + fi)) return false;
        if (!bunkerSelfTest(me, foe, seed + 100 + fi)) return false;
        if (!bunkerRetaliateTest(me, foe, seed + 110 + fi)) return false;
        if (!bunkerFloorTest(me, foe, seed + 80 + fi)) return false;
        if (!huntTest(me, foe, seed + 90 + fi)) return false;
        if (!bunkerOverdriveAndRadTest(me, foe, seed + 120 + fi)) return false;
        if (!creativeAiBunkerTest(me, foe, seed + 130 + fi)) return false;
        if (!scoutSwarmAutonomousTest(me, foe, seed + 140 + fi)) return false;
    }
    printf("stealthtest: ok\n");
    return g_ok;
}
