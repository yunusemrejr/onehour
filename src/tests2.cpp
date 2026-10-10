// One Hour - headless tests of the stealth / spy / cargo chopper / underground bunker / infantry pass (--stealthtest)
#include "sim.h"
#include "map.h"

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
    // ---- a defended structure: a spy that gets shot never finishes
    Ref spy = spawnAt(spyMe, 0, bp + Vec2(0, 6 * TILE));
    Ref guard = spawnAt(firstUnitOf(foe) + 5, 1, bp + Vec2(2 * TILE, 3 * TILE));
    g_sim.cmdAttack({spy}, b);
    CHECK(g_sim.get(spy)->order == O_CAPTURE, "a right-click on an enemy structure did not give the spy a capture order");
    run(40);
    CHECK(!alive(spy), "a spy walking past an enemy tank survived");
    CHECK(g_sim.get(b)->owner == 1, "a dead spy captured a structure");
    g_sim.destroy(*g_sim.get(guard), false);
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
    CHECK(!g_sim.visibleTo(*B, 1) && g_sim.visibleTo(*B, 0), "a bunker is not hidden from the enemy / not visible to its owner");
    // ---- fifty soldiers and one dozer can climb in, the fifty-first soldier and a second dozer cannot
    std::vector<Ref> inf, ones;
    for (int i = 0; i < 52; i++) inf.push_back(spawnAt(i % 3 == 0 ? rpg : rifle, 0, bp + Vec2(((i % 9) - 4) * 22.0f, 3.5f * TILE + (i / 9) * 22.0f)));
    Ref dz1 = spawnAt(dozerMe, 0, bp + Vec2(-3 * TILE, 3 * TILE)), dz2 = spawnAt(dozerMe, 0, bp + Vec2(3 * TILE, 3 * TILE));
    std::vector<Ref> all = inf; all.push_back(dz1); all.push_back(dz2);
    g_sim.cmdEnter(all, bk);
    if (getenv("ONEHOUR_DBGB")) { for (int sec = 0; sec < 40; sec++) { run(1); int ent = 0, idl = 0, in = 0; for (Ref r : inf) if (alive(r)) { Entity* u = g_sim.get(r); if (u->carrier.valid()) in++; else if (u->order == O_ENTER) ent++; else idl++; } fprintf(stderr, "t=%d in %d entering %d idle %d\n", sec, in, ent, idl); if (sec >= 3 && sec <= 9) for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { Entity* u = g_sim.get(r); Entity* cc = g_sim.get(bk); if (sec >= 3) fprintf(stderr, "  stuck: path0 %.0f,%.0f order %d pos %.0f,%.0f dist %.1f path %zu/%zu actionTimer %.1f canBoard %d unloading %d constructed %d disabled %.1f fall %.1f time %.1f passable %d\n", u->path.empty() ? -1.0f : u->path[0].x, u->path.empty() ? -1.0f : u->path[0].y, (int)u->order, u->pos.x, u->pos.y, g_sim.distToEntity(u->pos, *cc), u->pathIdx, u->path.size(), u->actionTimer, (int)g_sim.canBoard(*u, *cc), (int)cc->unloading, (int)cc->constructed, u->disabledUntil, u->fall, g_sim.time, (int)g_map.passable(tileOf(u->pos.x), tileOf(u->pos.y))); break; } } }
    run(40);
    B = g_sim.get(bk);
    if (g_sim.garrisonInfantry(*B) != BUNKER_INF_CAP) { for (int dy = -6; dy <= 8; dy++) { for (int dx = -8; dx <= 8; dx++) { int x = tileOf(bp.x) + dx, y = tileOf(bp.y) + dy; fprintf(stderr, "%c", !inMap(x, y) ? '?' : (g_map.blocked[y * MAP_W + x] ? (char)('0' + g_map.blocked[y * MAP_W + x]) : '.')); } fprintf(stderr, "\n"); } int idle = 0, enter = 0; for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { if (g_sim.get(r)->order == O_ENTER) enter++; else idle++; } fprintf(stderr, "garrison %d, still entering %d, idle outside %d\n", g_sim.garrisonInfantry(*B), enter, idle); for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) { Entity* u = g_sim.get(r); fprintf(stderr, "  outside unit order %d pos %.0f,%.0f dist to bunker %.1f (bunker %.0f,%.0f) canBoard %d\n", (int)u->order, u->pos.x, u->pos.y, g_sim.distToEntity(u->pos, *B), B->pos.x, B->pos.y, (int)g_sim.canBoard(*u, *B)); break; } }
    CHECK(g_sim.garrisonInfantry(*B) == BUNKER_INF_CAP, "the bunker does not hold exactly 50 soldiers");
    CHECK(g_sim.garrisonDozers(*B) == 1, "the bunker does not hold exactly one dozer");
    int outside = 0; for (Ref r : all) if (alive(r) && !g_sim.get(r)->carrier.valid()) outside++;
    CHECK(outside == 3, "two soldiers and one dozer should be left outside a full bunker");
    // ---- the garrison fires from the hatch at tanks and at aircraft (rocket troopers), the bunker itself stays unseen
    Ref tk = spawnAt(tankFoe, 1, bp + Vec2(5 * TILE, 0));
    Ref air = spawnAt(vultureFoe, 1, bp + Vec2(-4 * TILE, 2 * TILE)); if (Entity* a = g_sim.get(air)) { a->alt = 1; a->order = O_IDLE; }
    float tk0 = hpOf(tk), air0 = hpOf(air);
    run(6);
    CHECK(!alive(tk) || hpOf(tk) < tk0 * 0.8f, "the garrison did not shoot an enemy tank");
    CHECK(!alive(air) || hpOf(air) < air0 * 0.9f, "the rocket troopers in the bunker did not shoot an aircraft");
    CHECK(!g_sim.visibleTo(*g_sim.get(bk), 1), "the bunker became visible by firing");
    // ---- a nuke wrecks the hatch, hurts the bunker, and can never destroy it; the garrison inside is untouched but silent
    Ref tk2 = spawnAt(tankFoe, 1, bp + Vec2(12 * TILE, 0));
    for (int k = 0; k < 6; k++) {
        g_sim.nukes.push_back({g_sim.players[1].basePos, g_sim.get(bk)->pos, 1, Sim::NUKE_FLIGHT - 0.05f, false});
        run(0.2f); g_sim.fallouts.clear();
        CHECK(alive(bk), "a nuke destroyed an underground bunker");
        if (k == 0) {
            B = g_sim.get(bk);
            CHECK(B->hatch <= 0, "a nuke left the surface hatch intact");
            CHECK(B->hp < B->maxHp, "a nuke did not hurt the bunker at all");
            CHECK(g_sim.garrisonInfantry(*B) == BUNKER_INF_CAP, "a nuke killed soldiers sheltering underground");
        }
    }
    B = g_sim.get(bk);
    CHECK(B->hp >= B->maxHp * BUNKER_FLOOR - 1.0f, "repeated nukes took the bunker below its floor");
    CHECK(g_sim.garrisonInfantry(*B) == BUNKER_INF_CAP, "the garrison did not survive the nukes");
    Ref tk3 = spawnAt(tankFoe, 1, bp + Vec2(4 * TILE, 3 * TILE));
    float t30 = hpOf(tk3); run(8);
    CHECK(alive(tk3) && hpOf(tk3) >= t30, "a garrison fired with the hatch blown off");
    // ---- a dozer inside patches the hatch and the garrison is back in action
    run(30);
    B = g_sim.get(bk);
    CHECK(B->hatch > 0, "the dozer inside did not patch the hatch");
    Ref tk4 = spawnAt(tankFoe, 1, g_sim.get(bk)->pos + Vec2(3.5f * TILE, 2 * TILE));   // (the first one backed away from the unseen gunfire)
    float t31 = hpOf(tk4); run(10);
    CHECK(!alive(tk4) || hpOf(tk4) < t31, "the garrison did not resume fire after the hatch was patched");
    (void)tk2;
    // ---- only soldiers and vehicles can destroy it: enemy tanks guided by a spy drone grind it down, and the garrison spills out exposed
    // pre-damage it to the floor with nukes so the grind fits in the test (the nukes also clear the field)
    for (int k = 0; k < 4; k++) { g_sim.nukes.push_back({g_sim.players[1].basePos, g_sim.get(bk)->pos, 1, Sim::NUKE_FLIGHT - 0.05f, false}); run(0.2f); g_sim.fallouts.clear(); }
    run(14);   // (until the pulse that knocked the bunker offline has passed)
    Ref sd = spawnAt(sdroneFoe, 1, bp + Vec2(0, 5 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    std::vector<Ref> tanks;
    for (int i = 0; i < 14; i++) tanks.push_back(spawnAt(tankFoe, 1, bp + Vec2(((i % 7) - 3) * 30.0f, 4 * TILE + (i / 7) * 30.0f)));
    for (Ref t : tanks) if (alive(t)) { g_sim.get(t)->maxHp *= 10; g_sim.get(t)->hp = g_sim.get(t)->maxHp; }   // (the hatch gunfire would kill them: this test is about damage to the bunker)
    run(2);
    g_sim.cmdAttack(tanks, bk);
    CHECK(g_sim.get(tanks[0])->order == O_ATTACK, "enemy tanks could not be ordered to attack a bunker a spy drone had revealed");
    bool dead = false;
    for (int t = 0; t < 20 * 400 && !dead; t++) { g_sim.step(); g_sim.events.clear(); if (t % 100 == 0 && !g_sim.get(sd)) { Ref nsd = spawnAt(sdroneFoe, 1, bp + Vec2(0, 5 * TILE)); sd = nsd; } dead = !alive(bk); }
    CHECK(dead, "tanks could not destroy the bunker");
    int exposed = 0; for (Ref r : inf) if (alive(r) && !g_sim.get(r)->carrier.valid()) exposed++;
    CHECK(exposed >= 40, "the garrison did not spill out onto the ground when the bunker fell");
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
    CHECK(g_sim.get(spy)->order != O_CAPTURE, "a spy was ordered to capture a bunker nobody had found");
    Ref sd = spawnAt(sdroneMe, 0, bp + Vec2(0, 4 * TILE)); if (Entity* a = g_sim.get(sd)) a->alt = 1;
    run(2);
    CHECK(g_sim.visibleTo(*g_sim.get(bk), 0), "my spy drone did not reveal the bunker");
    g_sim.cmdAttack({spy}, bk);
    CHECK(g_sim.get(spy)->order == O_CAPTURE, "a spy could not be ordered to capture a revealed bunker");
    // the hidden garrison shoots the spy before it gets there: knock the hatch off first
    g_sim.get(bk)->hatch = 0;
    run(g_sim.captureTime(*g_sim.get(bk)) + 16);
    CHECK(g_sim.get(bk)->owner == 0, "a spy did not capture a bunker");
    CHECK(g_sim.get(bk)->passengers.empty(), "the old garrison stayed in a captured bunker");
    int out = 0; for (Ref r : garrison) if (alive(r) && !g_sim.get(r)->carrier.valid() && g_sim.get(r)->owner == 1) out++;
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
    for (int i = 0; i < VET_KILLS; i++) { Ref v = spawnAt(foe == F_CYBER ? U_C_SPY : U_K_SPY, 1, mid + Vec2(30, 0)); g_sim.cmdAttack({r}, v); run(6); }
    CHECK(alive(r) && g_sim.get(r)->rank >= 1, "four kills did not make a veteran");
    CHECK(g_sim.get(r)->maxHp > base, "a veteran is no tougher than a recruit");
    printf("stealthtest %s: rocket infantry and promotions ok\n", FACTION_NAME[me]);
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
    }
    printf("stealthtest: ok\n");
    return g_ok;
}
