// One Hour - headless tests of dozer priorities: construction before repairs, the hold after a manual order, kept build sites (--dozertest)
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok = true;
static bool failMsg(const char* m) { fprintf(stderr, "dozertest: %s\n", m); g_ok = false; return false; }
#define CHECK(cond, msg) do { if (!(cond)) return failMsg(msg); } while (0)

static void run(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }

// steps the sim until done() holds (looked at before every step) or sec seconds pass; true if it held
template <class F> static bool runUntil(float sec, F done) {
    int n = (int)(sec * SIM_HZ);
    for (int t = 0;; t++) {
        if (done()) return true;
        if (t >= n) return false;
        g_sim.step(); g_sim.events.clear();
    }
}

static float hpOf(Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : -1.0f; }
static int orderOf(Ref r) { Entity* e = g_sim.get(r); return e ? (int)e->order : -1; }
static Ref targetOf(Ref r) { Entity* e = g_sim.get(r); return e ? e->targetEnt : NOREF; }
static bool autoOf(Ref r) { Entity* e = g_sim.get(r); return e && e->autoTask; }
static bool onSite(Ref dz, Ref site) { return orderOf(dz) == (int)O_BUILD && targetOf(dz) == site; }
static Ref queueFront(Ref dz) { Entity* e = g_sim.get(dz); return e && !e->siteQueue.empty() ? e->siteQueue.front() : NOREF; }
static size_t queueLen(Ref dz) { Entity* e = g_sim.get(dz); return e ? e->siteQueue.size() : 0; }
static bool built(Ref b) { Entity* e = g_sim.get(b); return e && e->constructed; }
static bool progressed(Ref b) { Entity* e = g_sim.get(b); return e && e->progress > 0.05f; }

static Ref spawnAt(int type, int owner, Vec2 p) { return g_sim.spawnUnit(type, owner, g_map.nearestFree(p, 30)); }
static Ref dozerAt(Faction f, Vec2 p) { return spawnAt(firstUnitOf(f), 0, p); }

// the first spot near 'near' where a structure of this type can be placed
static bool spotFor(int type, int owner, Vec2 nearP, int& tx, int& ty) {
    int cx = tileOf(nearP.x), cy = tileOf(nearP.y);
    for (int r = 0; r < 20; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) { tx = cx + dx; ty = cy + dy; return true; }
    }
    return false;
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

// a point 'dx', 'dy' tiles away from p
static Vec2 offTile(Vec2 p, float dx, float dy) { return p + Vec2(dx * TILE, dy * TILE); }

// a clean field in the middle of the map: nothing but the two bases far away, everything explored, plenty of money
static void freshScene(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
}

static int barracksOf(Faction f) {
    for (int t = 0; t < B_COUNT; t++) if (BUILDS[t].faction == f && BUILDS[t].role == BR_BARRACKS) return t;
    return -1;
}

// a finished structure of player 0, hurt to 'frac' of its health and not hit lately
static Ref hurtAt(int type, Vec2 nearP, float frac) {
    int tx, ty;
    if (!spotFor(type, 0, nearP, tx, ty)) return NOREF;
    Ref r = g_sim.placeBuilding(type, 0, tx, ty, true);
    if (Entity* b = g_sim.get(r)) { b->hp = b->maxHp * frac; b->lastDamaged = -100; }
    return r;
}

// an unfinished site of 'owner' near nearP (nobody is building it)
static Ref siteAt(int type, Vec2 nearP, int owner = 0) {
    int tx, ty;
    if (!spotFor(type, owner, nearP, tx, ty)) return NOREF;
    return g_sim.placeBuilding(type, owner, tx, ty, false);
}

// player 0's dozer orders a build near nearP the way a player does (cmdBuild); returns the new site
static Ref buildAt(Ref dz, int type, Vec2 nearP) {
    int tx, ty;
    if (!spotFor(type, 0, nearP, tx, ty) || !g_sim.cmdBuild(dz, type, tx, ty)) return NOREF;
    return targetOf(dz);
}

// control: a dozer with no orders mends a damaged structure on its own, within a second or so
static bool idleMends(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bk = barracksOf(me);
    Ref dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    CHECK(dmg.valid(), "no spot for the damaged structure");
    Ref dz = dozerAt(me, mid);
    CHECK(dz.valid(), "no spot for the dozer");
    CHECK(runUntil(1.5f, [&] { return onSite(dz, dmg) && autoOf(dz); }), "an idle dozer with no orders did not pick up a repair within 1.5 s");
    float h0 = hpOf(dmg);
    run(3.0f);
    CHECK(hpOf(dmg) > h0 + 50.0f, "the dozer picked up the repair but did not mend it");
    printf("dozertest %s: idle dozer mends a damaged structure on its own ok\n", FACTION_NAME[me]);
    return true;
}

// point 1: an idle dozer takes unfinished sites of its own army first, then repairs; sites and structures hit in the last 4 s are skipped
static bool priorities(Faction me, Faction foe, u64 seed) {
    int bk;
    // ---- a site beats a nearer repair, and a foreign army's site nearer still is never chosen
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    bk = barracksOf(me);
    Ref dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    Ref site = siteAt(bk, offTile(mid, -6, 0));
    Ref foeSite = siteAt(barracksOf(foe), offTile(mid, 0, -3), 1);
    CHECK(dmg.valid() && site.valid() && foeSite.valid(), "no spot for the structures of the scene");
    Ref dz = dozerAt(me, mid);
    CHECK(dz.valid(), "no spot for the dozer");
    CHECK(runUntil(1.5f, [&] { return onSite(dz, site); }), "an idle dozer went to a repair (or a foreign site) before an unfinished site of its own army");
    CHECK(autoOf(dz), "a dozer that took a site on its own was not marked as acting on its own");
    // ---- a site hit in the last 4 s is skipped: the dozer mends the structure instead
    freshScene(me, foe, seed + 1);
    mid = openField();
    dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    site = siteAt(bk, offTile(mid, -6, 0));
    CHECK(dmg.valid() && site.valid(), "no spot for the structures of the scene");
    CHECK(g_sim.get(site) != nullptr, "the site vanished");
    g_sim.get(site)->lastDamaged = g_sim.time;   // (under fire right now)
    dz = dozerAt(me, mid);
    CHECK(runUntil(1.5f, [&] { return onSite(dz, dmg); }), "an idle dozer went for a site hit in the last 4 s instead of a repair");
    // ---- with everything hit in the last 4 s the dozer waits; once 4 s have passed the site comes first
    freshScene(me, foe, seed + 2);
    mid = openField();
    dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    site = siteAt(bk, offTile(mid, -6, 0));
    CHECK(dmg.valid() && site.valid(), "no spot for the structures of the scene");
    g_sim.get(site)->lastDamaged = g_sim.time;
    g_sim.get(dmg)->lastDamaged = g_sim.time;
    dz = dozerAt(me, mid);
    CHECK(!runUntil(2.5f, [&] { return orderOf(dz) != (int)O_IDLE; }), "an idle dozer went to work on structures hit in the last 4 s");
    CHECK(runUntil(3.0f, [&] { return onSite(dz, site); }), "an idle dozer did not join its site once it had not been hit for 4 s");
    // ---- a damaged structure hit in the last 4 s is skipped too (no site to help with)
    freshScene(me, foe, seed + 3);
    mid = openField();
    dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    CHECK(dmg.valid(), "no spot for the damaged structure");
    g_sim.get(dmg)->lastDamaged = g_sim.time;
    dz = dozerAt(me, mid);
    CHECK(!runUntil(2.5f, [&] { return orderOf(dz) != (int)O_IDLE; }), "an idle dozer mended a structure hit in the last 4 s");
    CHECK(runUntil(3.0f, [&] { return onSite(dz, dmg); }), "an idle dozer did not mend a structure once it had been left alone for 4 s");
    printf("dozertest %s: sites before repairs, hit structures skipped ok\n", FACTION_NAME[me]);
    return true;
}

// point 2: a dozer mending on its own drops the repair for an unfinished site nobody is building; a repair the player ordered is kept
static bool repairDrop(Faction me, Faction foe, u64 seed) {
    int bk = barracksOf(me);
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    Ref dmg = hurtAt(bk, offTile(mid, 4, 0), 0.2f);
    CHECK(dmg.valid(), "no spot for the damaged structure");
    Ref dz = dozerAt(me, mid);
    CHECK(runUntil(1.5f, [&] { return onSite(dz, dmg) && autoOf(dz); }), "an idle dozer did not start mending on its own");
    run(2.0f);
    Entity* d = g_sim.get(dmg);
    CHECK(d && d->hp < d->maxHp, "the repair finished too soon for the test");
    Ref site = siteAt(bk, offTile(mid, -6, 0));
    CHECK(site.valid(), "no spot for the unfinished site");
    CHECK(runUntil(2.5f, [&] { return onSite(dz, site); }), "a dozer mending on its own did not drop the repair for an unfinished site nobody builds");
    CHECK(autoOf(dz), "the dozer lost its own-initiative flag when it switched to the site");
    printf("dozertest %s: a self-started repair gives way to an unbuilt site ok\n", FACTION_NAME[me]);

    // ---- a repair the player ordered (cmdAssist, or cmdAttack on an own building) is not dropped
    for (int via = 0; via < 2; via++) {
        freshScene(me, foe, seed + 10 + via);
        mid = openField();
        dmg = hurtAt(bk, offTile(mid, 4, 0), 0.2f);
        CHECK(dmg.valid(), "no spot for the damaged structure");
        dz = dozerAt(me, mid);
        if (via == 0) g_sim.cmdAssist({dz}, dmg); else g_sim.cmdAttack({dz}, dmg, false);
        CHECK(onSite(dz, dmg) && !autoOf(dz), "a player's repair order did not send the dozer to the structure");
        site = siteAt(bk, offTile(mid, -6, 0));
        CHECK(site.valid(), "no spot for the unfinished site");
        CHECK(!runUntil(4.0f, [&] { return targetOf(dz) != dmg || orderOf(dz) != (int)O_BUILD; }), "a dozer on a repair the player ordered was pulled off it for a site");
        Entity* dd = g_sim.get(dmg);
        CHECK(dd && dd->hp < dd->maxHp, "the ordered repair finished too soon for the test");
    }
    printf("dozertest %s: a player-ordered repair is kept ok\n", FACTION_NAME[me]);
    return true;
}

// point 3: a manual move holds a dozer for DOZER_MANUAL_HOLD after it arrives: no repairs or site-joining on its own; then it resumes
static bool manualHold(Faction me, Faction foe, u64 seed, bool siteJob) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bk = barracksOf(me);
    Ref job = siteJob ? siteAt(bk, offTile(mid, 4, 0)) : hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    CHECK(job.valid(), "no spot for the job");
    float hp0 = hpOf(job);
    Ref dz = dozerAt(me, mid);
    CHECK(dz.valid(), "no spot for the dozer");
    g_sim.cmdMove({dz}, offTile(mid, 0, 5), false);
    CHECK(g_sim.get(dz)->autoHoldUntil >= g_sim.time + DOZER_MANUAL_HOLD - 0.05f, "a move order did not set the dozer's manual hold");
    CHECK(runUntil(15.0f, [&] { return orderOf(dz) == (int)O_IDLE; }), "a dozer never arrived at its move target");
    float arr = g_sim.time;
    CHECK(g_sim.get(dz)->autoHoldUntil >= arr + DOZER_MANUAL_HOLD - 0.1f, "the manual hold does not run from the dozer's arrival");
    CHECK(!runUntil(DOZER_MANUAL_HOLD - 1.0f, [&] { return orderOf(dz) != (int)O_IDLE || targetOf(dz) != NOREF; }), "a dozer on its manual hold went to work on its own");
    CHECK(hpOf(job) == hp0, "a dozer on its manual hold mended a structure");
    CHECK(runUntil(3.0f, [&] { return onSite(dz, job) && autoOf(dz); }), "a dozer did not resume its own work once the manual hold ran out");
    CHECK(g_sim.time >= arr + DOZER_MANUAL_HOLD - 0.1f, "a dozer resumed its own work before the manual hold ran out");
    printf("dozertest %s: manual hold keeps a dozer off its own %s for %.0f s ok\n", FACTION_NAME[me], siteJob ? "site" : "repairs", DOZER_MANUAL_HOLD);
    return true;
}

// point 4: a dozer pulled off an unfinished build (by a move, a new build, or a repair order) goes back to it once it is free
static bool keptSites(Faction me, Faction foe, u64 seed) {
    int bk = barracksOf(me);
    // ---- a move: after arriving it goes back to the site, not held back by the manual hold
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    Ref dz = dozerAt(me, mid);
    Ref site = buildAt(dz, bk, offTile(mid, -4, 0));
    CHECK(site.valid(), "the dozer could not start a build");
    CHECK(runUntil(8.0f, [&] { return progressed(site); }), "the dozer never started its build");
    g_sim.cmdMove({dz}, offTile(mid, 0, 5), false);
    CHECK(queueFront(dz) == site, "a dozer pulled off an unfinished site did not keep it");
    CHECK(runUntil(15.0f, [&] { return orderOf(dz) == (int)O_IDLE; }), "a dozer never arrived at its move target");
    CHECK(runUntil(2.0f, [&] { return onSite(dz, site) && !autoOf(dz); }), "after its move a dozer did not go back to the site it was pulled from");
    CHECK(queueLen(dz) == 0, "the kept site was still queued after the dozer went back to it");
    CHECK(runUntil(40.0f, [&] { return built(site); }), "the resumed site was never finished");
    printf("dozertest %s: a site left for a move is resumed ok\n", FACTION_NAME[me]);

    // ---- a new build: the old site waits in the queue; the dozer returns to it once the new one is done
    freshScene(me, foe, seed + 1);
    mid = openField();
    dz = dozerAt(me, mid);
    Ref s1 = buildAt(dz, bk, offTile(mid, -4, 0));
    CHECK(s1.valid(), "the dozer could not start a build");
    CHECK(runUntil(8.0f, [&] { return progressed(s1); }), "the dozer never started its first build");
    Ref s2 = buildAt(dz, bk, offTile(mid, 4, 0));
    CHECK(s2.valid(), "the dozer could not take a second build order");
    CHECK(queueFront(dz) == s1, "a dozer given a new build did not keep its unfinished site");
    CHECK(onSite(dz, s2), "a dozer given a new build is not building it");
    CHECK(runUntil(40.0f, [&] { return built(s2); }), "the new site was never finished");
    CHECK(runUntil(3.0f, [&] { return onSite(dz, s1) && !autoOf(dz); }), "after finishing its new site a dozer did not return to the one it left");
    CHECK(!built(s1), "the kept site was finished while the dozer was on another one");
    printf("dozertest %s: a site left for a new build is resumed ok\n", FACTION_NAME[me]);

    // ---- a repair order: the site waits; once the structure is mended the dozer goes back to the site, inside the hold
    freshScene(me, foe, seed + 2);
    mid = openField();
    dz = dozerAt(me, mid);
    s1 = buildAt(dz, bk, offTile(mid, -4, 0));
    CHECK(s1.valid(), "the dozer could not start a build");
    CHECK(runUntil(8.0f, [&] { return progressed(s1); }), "the dozer never started its build");
    Ref dmg = hurtAt(bk, offTile(mid, 4, 0), 0.8f);
    CHECK(dmg.valid(), "no spot for the damaged structure");
    float t0 = g_sim.time;
    g_sim.cmdAssist({dz}, dmg);
    CHECK(onSite(dz, dmg), "a repair order did not send the dozer to the structure");
    CHECK(queueFront(dz) == s1, "a repair order did not keep the dozer's unfinished site");
    CHECK(runUntil(15.0f, [&] { return onSite(dz, s1); }), "the dozer did not go back to its unfinished site after the repair");
    Entity* d = g_sim.get(dmg);
    CHECK(d && d->hp >= d->maxHp * 0.99f, "the dozer left the repair before it was done");
    CHECK(g_sim.time < t0 + DOZER_MANUAL_HOLD - 0.5f, "the dozer's return to its site waited out the manual hold");
    printf("dozertest %s: a site left for a repair order is resumed ok\n", FACTION_NAME[me]);
    return true;
}

// point 5: a stop order drops the kept sites and holds the dozer; after the hold it works on its own again (not as a queued order)
static bool stopClears(Faction me, Faction foe, u64 seed) {
    freshScene(me, foe, seed);
    Vec2 mid = openField();
    int bk = barracksOf(me);
    Ref dz = dozerAt(me, mid);
    Ref s1 = buildAt(dz, bk, offTile(mid, -4, 0));
    CHECK(s1.valid(), "the dozer could not start a build");
    CHECK(runUntil(8.0f, [&] { return progressed(s1); }), "the dozer never started its build");
    Ref dmg = hurtAt(bk, offTile(mid, 4, 0), 0.5f);
    CHECK(dmg.valid(), "no spot for the damaged structure");
    g_sim.cmdMove({dz}, offTile(mid, 0, 5), false);
    run(1.0f);
    CHECK(orderOf(dz) == (int)O_MOVE, "the dozer was not on its move when the stop came");
    CHECK(queueFront(dz) == s1, "the site was not kept before the stop");
    float t0 = g_sim.time;
    g_sim.cmdStop({dz});
    CHECK(queueLen(dz) == 0, "a stop order did not clear the dozer's kept sites");
    CHECK(g_sim.get(dz)->autoHoldUntil >= t0 + DOZER_MANUAL_HOLD - 0.05f, "a stop order did not set the dozer's manual hold");
    CHECK(orderOf(dz) == (int)O_IDLE, "a stop order did not leave the dozer idle");
    CHECK(!runUntil(DOZER_MANUAL_HOLD - 1.0f, [&] { return orderOf(dz) != (int)O_IDLE || targetOf(dz) != NOREF; }), "a stopped dozer went back to work during its hold");
    CHECK(runUntil(2.5f, [&] { return onSite(dz, s1); }), "a stopped dozer did not join its unfinished site once the hold ran out");
    CHECK(autoOf(dz), "a stopped dozer went back to its dropped site as a kept order instead of on its own");
    CHECK(g_sim.time >= t0 + DOZER_MANUAL_HOLD - 0.1f, "a stopped dozer resumed before its hold ran out");
    printf("dozertest %s: a stop order drops kept sites and holds the dozer ok\n", FACTION_NAME[me]);
    return true;
}

bool dozerTest(u64 seed) {
    g_map.generate();
    g_ok = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        u64 s = seed + 100 * fi;
        if (!idleMends(me, foe, s)) return false;
        if (!priorities(me, foe, s + 10)) return false;
        if (!repairDrop(me, foe, s + 20)) return false;
        if (!manualHold(me, foe, s + 30, false)) return false;
        if (!manualHold(me, foe, s + 31, true)) return false;
        if (!keptSites(me, foe, s + 40)) return false;
        if (!stopClears(me, foe, s + 50)) return false;
    }
    printf("dozertest: ok\n");
    return g_ok;
}
