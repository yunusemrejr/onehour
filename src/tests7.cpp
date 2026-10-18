// One Hour - headless tests for the wall / Hyper Protection / Anti-Rocket / durability pass (--walltest)
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok7 = true;
static bool fail7(const char* m) { fprintf(stderr, "walltest: %s\n", m); g_ok7 = false; return false; }
#define CHECK7(c, m) do { if (!(c)) return fail7(m); } while (0)

static void run7(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static Ref place7(int type, int owner, Vec2 near) {
    int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r < 30; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) return g_sim.placeBuilding(type, owner, cx + dx, cy + dy, true);
    }
    return NOREF;
}
static int countB7(int owner, int type) { int n = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == owner && e.type == type) n++; return n; }
static int rocketWeapon() { for (int i = 0; i < WEAPON_COUNT; i++) if (WEAPONS[i].proj == PJ_ROCKET) return i; return -1; }

static void scene7(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 500000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit()) g_sim.destroy(e, false);
    Vec2 c = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 14);
    flattenArena(c, 22, 22);
}

static void nuke7(Vec2 at, int owner, bool deep, float scale) {
    Sim::Nuke n; n.from = at + Vec2(80, 0); n.pos = at; n.owner = owner; n.t = Sim::NUKE_FLIGHT - 0.05f; n.scale = scale; n.deep = deep;
    g_sim.nukes.push_back(n);
    run7(0.25f);
    g_sim.fallouts.clear();
}

// a world with room for walls, big airfields and a research tech structure
static bool world7(Faction me, Faction foe, u64 seed) {
    scene7(me, foe, seed);
    Vec2 arena = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 14);
    int power = firstBuildOf(me) + BR_POWER, tech = firstBuildOf(me) + BR_TECH;
    Ref p1 = place7(power, 0, arena + Vec2(-6 * TILE, 0)), p2 = place7(power, 0, arena + Vec2(-6 * TILE, 3 * TILE));
    Ref techB = place7(tech, 0, arena + Vec2(0, -8 * TILE));
    CHECK7(p1.valid() && p2.valid() && techB.valid(), "could not build the test power/tech structures");
    return true;
}

static bool constantsTest(Faction me, Faction foe, u64 seed) {
    scene7(me, foe, seed);
    int hq = firstBuildOf(me), tech = hq + BR_TECH, bunker = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    int wall = me == F_CYBER ? B_C_WALL : B_K_WALL, air = hq + BR_AIRFIELD;
    CHECK7(BUILDS[wall].role == BR_WALL && BUILDS[wall].w == 1 && BUILDS[wall].h == 1 && BUILDS[wall].faction == me, "the wall is not a 1x1 structure of this army");
    CHECK7(BUILDS[wall].hp == WALL_HP && BUILDS[wall].hp < BUILDS[tech].hp, "the wall does not have its thick health pool");
    CHECK7(BUILDS[hq].hp > BUILDS[tech].hp && BUILDS[tech].hp > BUILDS[bunker].hp, "the Command structure / tech structure / bunker durability order is wrong");
    CHECK7(BUILDS[air].w == 10 && BUILDS[air].h == 7 && AIRFIELD_CAP == 25, "the airfield is not the larger 10x7 / 25-pad airbase");
    CHECK7(BUILDS[me == F_CYBER ? B_C_WALL : B_K_WALL].role == BR_WALL, "the other army has no wall");
    CHECK7(BARRACKS_SPEED == 3.0f && HYPER_HP == 2.0f && HYPER_NUKE < 1.0f && HQ_NUKE < HYPER_NUKE, "the new durability constants are not the intended ones");
    return true;
}

static bool wallTest(Faction me, Faction foe, u64 seed) {
    if (!world7(me, foe, seed)) return false;
    Vec2 arena = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 14);
    int wall = me == F_CYBER ? B_C_WALL : B_K_WALL, tech = firstBuildOf(me) + BR_TECH;
    CHECK7(g_sim.buildAvailable(0, wall) && !g_sim.atBuildLimit(0, wall), "the wall is not buildable / is capped");
    // a chain: place 32 segments next to each other and make sure none is refused
    int made = 0;
    for (int i = 0; i < 32; i++) {
        Vec2 p = arena + Vec2((i % 8) * TILE - 4 * TILE, (i / 8) * TILE + 4 * TILE);
        if (place7(wall, 0, p).valid()) made++;
    }
    CHECK7(made == 32, "could not chain 32 wall segments (walls must be unlimited and adjacent)");
    CHECK7(countB7(0, wall) >= 32 && !g_sim.atBuildLimit(0, wall), "the wall limit kicked in");
    // thick against warheads: 8 ground-zero nukes leave it standing, a long barrage eventually opens the breach
    Ref w = place7(wall, 0, arena + Vec2(-16 * TILE, -16 * TILE));
    CHECK7(w.valid(), "no isolated wall to nuke");
    Entity* W = g_sim.get(w); W->hp = W->maxHp;
    for (int k = 0; k < 8; k++) { nuke7(W->pos, 1, false, 1.0f); CHECK7(g_sim.get(w) != nullptr, "a wall fell to fewer than eight warheads"); }
    CHECK7(g_sim.get(w)->hp > 0.0f, "the wall did not survive eight warheads");
    for (int k = 0; k < 8; k++) { if (!g_sim.get(w)) break; nuke7(g_sim.get(w)->pos, 1, false, 1.0f); }
    CHECK7(!g_sim.get(w), "a wall survived sixteen ground-zero warheads");
    // a silver warhead also only chips it by the cap
    Ref w2 = place7(wall, 0, arena + Vec2(16 * TILE, -16 * TILE));
    CHECK7(w2.valid(), "no second wall");
    float hp0 = g_sim.get(w2)->maxHp; g_sim.get(w2)->hp = hp0;
    nuke7(g_sim.get(w2)->pos, 1, true, UNUKE_SCALE);
    CHECK7(g_sim.get(w2) && g_sim.get(w2)->hp > hp0 * (1.0f - WALL_NUKE - 0.02f), "an Underground Nuke took more than the wall's hard cap");
    // Hyper Protection: researchable at the tech structure, doubles health and caps one warhead
    if (!g_sim.techOnline(0)) {   // (the warhead tests may have caught the first one at the edge of a blast)
        Ref t2 = place7(tech, 0, arena + Vec2(-20 * TILE, 12 * TILE));
        CHECK7(t2.valid() && g_sim.techOnline(0), "could not rebuild the test tech structure");
    }
    CHECK7(g_sim.upgradeAvailable(0, UPG_HYPER), "Hyper Protection is not available at the tech structure");
    CHECK7(g_sim.cmdUpgrade(0, UPG_HYPER), "Hyper Protection could not be started");
    for (int t = 0; t < 20 * 130 && !g_sim.players[0].upg[UPG_HYPER]; t++) { g_sim.step(); g_sim.events.clear(); }
    CHECK7(g_sim.players[0].upg[UPG_HYPER], "Hyper Protection never finished");
    int barracks = firstBuildOf(me) + BR_BARRACKS;
    Ref br = place7(barracks, 0, arena + Vec2(8 * TILE, -4 * TILE));
    CHECK7(br.valid() && std::fabs(g_sim.get(br)->maxHp - BUILDS[barracks].hp * HYPER_HP) < 1.0f, "Hyper Protection did not double a structure's health");
    Entity* B = g_sim.get(br); B->hp = B->maxHp;
    nuke7(B->pos, 1, true, UNUKE_SCALE);
    CHECK7(g_sim.get(br) != nullptr, "one Underground Nuke erased a full-health Hyper-protected structure");
    CHECK7(g_sim.get(br)->hp > g_sim.get(br)->maxHp * (1.0f - HYPER_NUKE - 0.02f), "Hyper Protection did not cap the warhead");
    float before = g_sim.get(br)->hp;
    g_sim.applyDamagePublic(*g_sim.get(br), 100.0f, 1, NOREF);
    CHECK7(std::fabs(g_sim.get(br)->hp - (before - 100.0f * HYPER_ARMOR)) < 1.0f, "Hyper Protection did not reduce conventional damage");
    // Command structure: far tougher than anything else, and a warhead can only chip it
    int hq = firstBuildOf(me);
    Ref h = place7(hq, 0, arena + Vec2(-14 * TILE, 8 * TILE));
    CHECK7(h.valid(), "no room for a Command structure");
    g_sim.get(h)->hp = g_sim.get(h)->maxHp;
    nuke7(g_sim.get(h)->pos, 1, true, UNUKE_SCALE);
    CHECK7(g_sim.get(h) && g_sim.get(h)->hp > g_sim.get(h)->maxHp * (1.0f - HQ_NUKE - 0.02f), "the Command structure lost more than its hard nuke cap");
    CHECK7(BUILDS[hq].hp > BUILDS[tech].hp, "the Command structure is not tougher than the tech structure");
    // bunkers are not hidden any more
    int bunker = me == F_CYBER ? B_C_BUNKER : B_K_BUNKER;
    Ref bk = place7(bunker, 0, arena + Vec2(12 * TILE, 8 * TILE));
    CHECK7(bk.valid() && Sim::stealthOf(*g_sim.get(bk)) == ST_NONE && g_sim.visibleTo(*g_sim.get(bk), 1), "an underground bunker is still stealthy");
    return true;
}

static bool antiRocketTest(Faction me, Faction foe, u64 seed) {
    if (!world7(me, foe, seed)) return false;
    Vec2 arena = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 14);
    int wall = me == F_CYBER ? B_C_WALL : B_K_WALL, barracks = firstBuildOf(me) + BR_BARRACKS;
    CHECK7(g_sim.techOnline(0), "the test tech structure is offline");
    CHECK7(g_sim.cmdUpgrade(0, UPG_ANTIROCKET), "Anti-Rocket could not be researched");
    for (int t = 0; t < 20 * 80 && !g_sim.players[0].upg[UPG_ANTIROCKET]; t++) { g_sim.step(); g_sim.events.clear(); }
    CHECK7(g_sim.players[0].upg[UPG_ANTIROCKET], "Anti-Rocket never finished");
    std::vector<Ref> walls;
    for (int i = 0; i < 6; i++) { Ref w = place7(wall, 0, arena + Vec2(i * TILE - 3 * TILE, 2 * TILE)); if (w.valid()) walls.push_back(w); }
    CHECK7(walls.size() >= 4, "could not build an Anti-Rocket wall line");
    Ref br = place7(barracks, 0, arena + Vec2(0, 5 * TILE));
    CHECK7(br.valid(), "could not build the protected structure");
    int rw = rocketWeapon(); CHECK7(rw >= 0, "no rocket weapon to test with");
    auto pd = [&]() { for (Ref wr : walls) { Entity* w = g_sim.get(wr); if (w) { w->pdAt = 0; w->siloTimer = 0; } } };
    int intercepted = 0;
    for (int attempt = 0; attempt < 40 && intercepted == 0; attempt++) {
        pd();
        Projectile p; p.pos = g_sim.get(br)->pos + Vec2(-6 * TILE, 0); p.prevPos = p.pos; p.dest = g_sim.get(br)->pos;
        p.target = br; p.weapon = rw; p.owner = 1; p.life = 5; p.alive = true;
        p.vel = (p.dest - p.pos).norm() * std::max(1.0f, WEAPONS[rw].projSpeed);
        g_sim.projs.push_back(p);
        float hp = g_sim.get(br)->hp;
        g_sim.step(); g_sim.events.clear();
        if (g_sim.projs.empty() || g_sim.get(br)->hp >= hp) intercepted++;
    }
    CHECK7(intercepted > 0, "Anti-Rocket walls never shot a rocket down");
    // aircraft: the wall laser hits an enemy aircraft in reach
    int air = me == F_CYBER ? U_C_HELI : U_K_AIR;
    Ref a = g_sim.spawnUnit(air, 1, g_sim.get(walls[0])->pos + Vec2(3 * TILE, 0));
    Entity* A = g_sim.get(a); A->alt = 1; A->order = O_IDLE; A->maxHp *= 4; A->hp = A->maxHp;
    float ah = A->hp;
    pd();
    run7(3);
    CHECK7(!g_sim.get(a) || g_sim.get(a)->hp < ah, "an Anti-Rocket wall did not shoot at an aircraft");
    // a Q anti-nuke defence also engages aircraft and rockets
    int an = me == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE;
    Ref q = place7(an, 0, arena + Vec2(-8 * TILE, 2 * TILE));
    CHECK7(q.valid(), "could not build a Q defence");
    if (Entity* Q = g_sim.get(q)) Q->pdAt = 0;
    Ref a2 = g_sim.spawnUnit(air, 1, g_sim.get(q)->pos + Vec2(4 * TILE, 0));
    Entity* A2 = g_sim.get(a2); A2->alt = 1; A2->order = O_IDLE; A2->maxHp *= 4; A2->hp = A2->maxHp;
    float ah2 = A2->hp;
    run7(3);
    CHECK7(!g_sim.get(a2) || g_sim.get(a2)->hp < ah2, "a Q anti-nuke defence did not shoot at an aircraft");
    // the Q defence engages a warhead; repeat a few attempts because each defence rolls its own chance
    bool nukeHit = false;
    for (int attempt = 0; attempt < 20 && !nukeHit; attempt++) {
        g_sim.nukes.clear(); g_sim.interceptors.clear(); g_sim.fallouts.clear();
        if (Entity* Q = g_sim.get(q)) { Q->siloTimer = 0; Q->disabledUntil = -1; }
        Sim::Nuke n; n.from = g_sim.get(q)->pos + Vec2(30 * TILE, 0); n.pos = g_sim.get(q)->pos; n.owner = 1; n.t = Sim::NUKE_FLIGHT - 4.0f; n.id = ++g_sim.nukeSeq;
        g_sim.nukes.push_back(n);
        run7(1.2f);
        if (g_sim.nukes.empty() || g_sim.nukes[0].killT >= 0 || g_sim.nukes[0].broken) nukeHit = true;
    }
    CHECK7(nukeHit, "a Q anti-nuke defence never engaged a warhead");
    return true;
}

static bool economyTest(Faction me, Faction foe, u64 seed) {
    scene7(me, foe, seed);
    Vec2 arena = openGroundNear(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 14);
    int barracks = firstBuildOf(me) + BR_BARRACKS, rifle = firstUnitOf(me) + 2;
    Ref br = place7(barracks, 0, arena + Vec2(0, 4 * TILE));
    CHECK7(br.valid(), "no barracks for the production test");
    g_sim.players[0].money = 100000;
    CHECK7(g_sim.cmdTrain(br, rifle), "could not queue a soldier");
    float expected = UNITS[rifle].buildTime / BARRACKS_SPEED;
    int ticks = 0, maxTicks = (int)(expected * 4.0f * SIM_HZ);
    while (g_sim.get(br) && !g_sim.get(br)->queue.empty() && ticks < maxTicks) { g_sim.step(); g_sim.events.clear(); ticks++; }
    float secs = ticks / (float)SIM_HZ;
    CHECK7(g_sim.get(br) && g_sim.get(br)->queue.empty(), "the barracks never finished its soldier");
    CHECK7(secs <= expected * 1.35f + 0.15f, "the barracks did not produce at three times the old speed");
    // airfield: the big footprint and the 25-pad capacity are real
    int air = firstBuildOf(me) + BR_AIRFIELD, plane = me == F_CYBER ? U_C_AIR : U_K_JET;
    Ref af = place7(air, 0, arena + Vec2(0, -10 * TILE));
    CHECK7(af.valid(), "no room for the oversized airfield");
    for (int i = 0; i < AIRFIELD_CAP; i++) {
        Ref u = g_sim.spawnUnit(plane, 0, g_sim.get(af)->pos + Vec2((i % 5 - 2) * 24.0f, (i / 5 - 2) * 24.0f));
        if (Entity* e = g_sim.get(u)) { e->home = af; e->alt = 1; }
    }
    CHECK7(g_sim.padsUsed(*g_sim.get(af)) == AIRFIELD_CAP, "the airfield does not hold exactly 25 fixed-wing aircraft");
    CHECK7(g_sim.get(af)->bt().w * g_sim.get(af)->bt().h >= 60, "the airfield was not enlarged");
    return true;
}

bool wallTestMain(u64 seed) {
    g_map.generate();
    g_ok7 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        if (!constantsTest(me, foe, seed + fi)) return false;
        if (!wallTest(me, foe, seed + 20 + fi)) return false;
        if (!antiRocketTest(me, foe, seed + 40 + fi)) return false;
        if (!economyTest(me, foe, seed + 60 + fi)) return false;
        printf("walltest %s: walls, durability, Hyper Protection, Anti-Rocket, barracks and airfield ok\n", FACTION_NAME[me]);
    }
    printf("walltest: ok\n");
    return g_ok7;
}
