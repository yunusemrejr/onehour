// One Hour - headless tests for the bunker update (--bunkertest)
//  * nuclear hardening: ten ordinary warheads landing on a bunker together leave it standing (some 4% each), thirty more destroy it; it mends
//    itself fast once left alone, refits a blown hatch, and fallout never hurts it (its scrubbers wash the zone out)
//  * 200 soldiers and 5 dozers fit inside
//  * per-bunker upgrades: bought for one bunker, fitted in time, never carried over to another; one fitting at a time; the shell's health,
//    the triple launch (three swift missiles at once), the Para Backup (35 paratroopers, no infiltrators, once a minute), the laser battery
//    against aircraft and the field hospital mending a tank
//  * Upgrade All Bunkers, Rain Nukes (five fast warheads spread over a wide circle), Tank Drop (ten tanks and a medic) and the Para Backup's laser
//    commandos (ten Laser Troopers in one; their toxic clouds poison only the enemy)
//  * a shift-dragged line of structures (Sim::cmdBuildLine, Game::bulkSpots): every spot placed and paid, footprints edge to edge, one dozer queue
//  * a Waymaker laid over a structure and the trees beside it clears the trees and leaves the structure
#include "sim.h"
#include "map.h"
#include "ai.h"
#include "ui.h"
#include "testutil.h"

static bool g_ok18 = true;
static bool fail18(const char* m) { fprintf(stderr, "bunkertest: %s\n", m); g_ok18 = false; return false; }
#define CHECK18(c, m) do { if (!(c)) return fail18(m); } while (0)

static void init18(u64 seed, Faction f0, bool ai1 = false) {
    Faction fac[2] = { f0, (Faction)(1 - f0) };
    bool ai[2] = { false, ai1 };
    int diff[2] = { 1, 2 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
}
static void run18(float sec, bool ai = false) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n && !g_sim.gameOver; t++) { g_sim.step(); if (ai) g_ai.update(); g_sim.events.clear(); } }
static int bunkerType18(int p) { return g_sim.players[p].faction == F_CYBER ? B_C_BUNKER : B_K_BUNKER; }
// a finished bunker of player p on flattened open ground near 'near'
static Ref bunkerAt18(int p, Vec2 near) {
    Vec2 c = openGroundNear(near, 5);
    flattenArena(c, 5, 5);
    return g_sim.placeBuilding(bunkerType18(p), p, tileOf(c.x) - 1, tileOf(c.y) - 1, true);
}
static float frac18(Ref r) { Entity* e = g_sim.get(r); return e ? e->hp / e->maxHp : -1.0f; }

static bool hardeningTest18(u64 seed) {
    init18(seed, F_CYBER);
    Ref br = bunkerAt18(0, g_sim.players[0].basePos + Vec2(14.0f * TILE, 6.0f * TILE));
    run18(0.2f);
    Vec2 at = g_sim.get(br)->pos, from = g_sim.players[1].basePos;
    for (int k = 0; k < 10; k++) g_sim.nukes.push_back({from, at, 1, Sim::NUKE_FLIGHT - 0.05f});
    run18(0.3f);
    CHECK18(g_sim.get(br), "ten warheads at once destroyed a bunker");
    float f = frac18(br);
    if (f < 0.5f || f > 0.7f) { fprintf(stderr, "bunkertest: after ten direct hits the bunker has %.2f of its health (want about 0.6)\n", f); return fail18("ten warheads did not take about 4% each"); }
    CHECK18(g_sim.get(br)->hatch <= 0, "ten direct hits left the hatch standing");
    // left alone it mends itself fast, and the blown hatch is refitted
    run18(BUNKER_REGEN_DELAY + 10.0f);
    float f2 = frac18(br);
    if (f2 < f + 0.12f) { fprintf(stderr, "bunkertest: mended %.3f -> %.3f in %.0f s\n", f, f2, BUNKER_REGEN_DELAY + 10.0f); return fail18("the bunker does not mend itself fast"); }
    CHECK18(g_sim.get(br)->hatch > 0, "the blown hatch was never refitted");
    // radiation never hurts it, and its scrubbers wash the zone out
    {
        Entity* b = g_sim.get(br);
        b->hp = b->maxHp * 0.5f; b->lastDamaged = g_sim.time;
        g_sim.fallouts.push_back({b->pos, 5.0f * TILE, 0.0f, 0.0f, 2.0f, -1, false});
        float hp0 = b->hp;
        run18(3.0f);
        b = g_sim.get(br);
        CHECK18(b && b->hp >= hp0, "fallout hurt a bunker");
        bool scrubbed = g_sim.fallouts.empty() || g_sim.fallouts[0].power < 2.0f * 0.85f;
        CHECK18(scrubbed, "a bunker did not scrub the fallout over it");
        g_sim.fallouts.clear();
    }
    // a big barrage does destroy it
    for (int k = 0; k < 30; k++) g_sim.nukes.push_back({from, at, 1, Sim::NUKE_FLIGHT - 0.05f});
    run18(0.3f);
    CHECK18(!g_sim.get(br), "thirty more warheads at once did not destroy the bunker");
    return true;
}

static bool garrisonTest18(u64 seed) {
    init18(seed, F_CLANKER);
    Ref br = bunkerAt18(0, g_sim.players[0].basePos + Vec2(14.0f * TILE, 6.0f * TILE));
    Vec2 at = g_sim.get(br)->pos + Vec2(0, 3.0f * TILE);
    int in = 0, dz = 0;
    for (int k = 0; k < BUNKER_INF_CAP + 5; k++) {
        Ref r = g_sim.spawnUnit(U_K_INF1, 0, at);
        Entity* u = g_sim.get(r); Entity* b = g_sim.get(br);
        if (u && b && g_sim.canBoard(*u, *b)) { g_sim.boardQuiet(*u, *b); in++; }
    }
    for (int k = 0; k < BUNKER_DOZER_CAP + 1; k++) {
        Ref r = g_sim.spawnUnit(U_K_DOZER, 0, at);
        Entity* u = g_sim.get(r); Entity* b = g_sim.get(br);
        if (u && b && g_sim.canBoard(*u, *b)) { g_sim.boardQuiet(*u, *b); dz++; }
    }
    CHECK18(BUNKER_INF_CAP == 200 && BUNKER_DOZER_CAP == 5, "the bunker does not hold 200 soldiers and 5 dozers");
    CHECK18(in == 200 && g_sim.garrisonInfantry(*g_sim.get(br)) == 200, "200 soldiers did not fit in a bunker (or more did)");
    CHECK18(dz == 5 && g_sim.garrisonDozers(*g_sim.get(br)) == 5, "5 dozers did not fit in a bunker (or more did)");
    run18(2.0f);
    CHECK18(g_sim.garrisonInfantry(*g_sim.get(br)) == 200, "the garrison of 200 did not stay inside");
    return true;
}

static bool upgradeTest18(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        init18(seed + fi, (Faction)fi);
        Player& pl = g_sim.players[0];
        Ref A = bunkerAt18(0, pl.basePos + Vec2(14.0f * TILE, 6.0f * TILE));
        Ref B = bunkerAt18(0, pl.basePos + Vec2(14.0f * TILE, -10.0f * TILE));
        float hp0 = g_sim.get(A)->maxHp;
        int m0 = pl.money;
        CHECK18(g_sim.cmdBunkerUpgrade(A, BU_SHELL), "a bunker upgrade was refused");
        CHECK18(pl.money == m0 - BUNKER_UPGS[BU_SHELL].cost, "a bunker upgrade did not cost its price");
        CHECK18(!g_sim.cmdBunkerUpgrade(A, BU_LASERS), "a bunker took a second upgrade while fitting one");
        CHECK18(g_sim.cmdBunkerUpgrade(B, BU_LASERS), "another bunker could not be upgraded while the first was busy");
        run18(BUNKER_UPGS[BU_LASERS].time + 1.0f);
        Entity* a = g_sim.get(A); Entity* b = g_sim.get(B);
        CHECK18(Sim::bunkerHas(*a, BU_SHELL) && !Sim::bunkerHas(*a, BU_LASERS), "the shell was not fitted (or the lasers went to the wrong bunker)");
        CHECK18(Sim::bunkerHas(*b, BU_LASERS) && !Sim::bunkerHas(*b, BU_SHELL), "an upgrade carried over to another bunker");
        CHECK18(std::fabs(a->maxHp - hp0 * BUNKER_SHELL_HP) < 1.0f && std::fabs(b->maxHp - hp0) < 1.0f, "the shell did not raise only its own bunker's health");
        CHECK18(!g_sim.cmdBunkerUpgrade(A, BU_SHELL), "a bunker was fitted with the same upgrade twice");
        // triple launch + swift warheads: three swift missiles at once
        for (int u : { BU_TRIPLE, BU_SWIFT, BU_PARA, BU_MEDIC }) { CHECK18(g_sim.cmdBunkerUpgrade(A, u), "a bunker upgrade was refused"); run18(BUNKER_UPGS[u].time + 0.5f); }
        a = g_sim.get(A);
        CHECK18(a && Sim::bunkerHas(*a, BU_TRIPLE) && Sim::bunkerHas(*a, BU_SWIFT) && Sim::bunkerHas(*a, BU_PARA) && Sim::bunkerHas(*a, BU_MEDIC), "the upgrades were not all fitted");
        a->siloTimer = 0; g_sim.get(B)->siloTimer = 0;
        Vec2 tgt = g_sim.players[1].basePos;
        size_t n0 = g_sim.nukes.size();
        CHECK18(g_sim.cmdMiniNuke(0, tgt, false, A), "a ready bunker refused to launch");
        CHECK18(g_sim.nukes.size() == n0 + 3, "triple launch did not fire three missiles at once");
        for (size_t k = n0; k < g_sim.nukes.size(); k++) CHECK18(g_sim.nukes[k].swift && Sim::flightOf(g_sim.nukes[k]) < Sim::NUKE_FLIGHT * 0.6f, "the missiles of a bunker with swift warheads are not swift");
        n0 = g_sim.nukes.size();
        CHECK18(g_sim.cmdMiniNuke(0, tgt, false, B), "the other bunker refused to launch");
        CHECK18(g_sim.nukes.size() == n0 + 1 && !g_sim.nukes.back().swift, "a bunker without the upgrades fired more than its pair's first missile (or a swift one)");
        // Para Backup: 35 paratroopers, no infiltrators, once a minute
        Vec2 zone = openGroundNear(pl.basePos + Vec2(-12.0f * TILE, 10.0f * TILE), 5);
        std::vector<int> load = g_sim.bunkerParaManifest(0);
        CHECK18((int)load.size() == BUNKER_PARA_TROOPS && BUNKER_PARA_TROOPS == 35, "a Para Backup does not carry 35 soldiers");
        for (int t : load) CHECK18(UNITS[t].kind == UK_INF && UNITS[t].role != UR_SPY, "a Para Backup carries something other than line infantry");
        int before = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().kind == UK_INF && !e.carrier.valid()) before++;
        CHECK18(g_sim.cmdBunkerPara(A, zone), "a ready Para Backup was refused");
        CHECK18(!g_sim.cmdBunkerPara(A, zone), "a Para Backup ignored its cooldown");
        CHECK18(!g_sim.cmdBunkerPara(B, zone), "a bunker without the radio mast called a Para Backup");
        run18(40.0f);
        int after = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().kind == UK_INF && !e.carrier.valid() && dist(e.pos, zone) < 9.0f * TILE) after++;
        if (after < 30) { fprintf(stderr, "bunkertest: %d paratroopers on the ground at the zone (%d infantry before)\n", after, before); return fail18("the Para Backup did not land its troops"); }
        CHECK18(g_sim.cmdBunkerPara(A, zone) == false || g_sim.time >= g_sim.get(A)->paraReady - BUNKER_PARA_COOLDOWN, "Para Backup cooldown");
        // field hospital: a damaged tank nearby is mended, even while under fire
        int tank = fi == 0 ? U_C_TANK : U_K_TANK;
        Ref tr = g_sim.spawnUnit(tank, 0, g_sim.get(A)->pos + Vec2(4.0f * TILE, 0));
        Entity* te = g_sim.get(tr); te->hp = te->maxHp * 0.3f; te->lastDamaged = g_sim.time;
        float th0 = te->hp;
        run18(4.0f);
        te = g_sim.get(tr);
        CHECK18(te && te->hp >= th0 + te->maxHp * BUNKER_MED_UNIT * 3.0f, "the field hospital did not mend a tank beside it");
        // laser battery: an enemy gunship over bunker B is burned down
        Ref gr = g_sim.spawnUnit(fi == 0 ? U_K_AIR : U_C_AIR, 1, g_sim.get(B)->pos + Vec2(5.0f * TILE, -3.0f * TILE));
        g_sim.get(B)->lzFired = -10;
        run18(5.0f);
        CHECK18(g_sim.get(B)->lzFired > g_sim.time - 5.5f, "the laser battery never fired at an enemy aircraft");
        CHECK18(!g_sim.get(gr), "the laser battery and the hatch guns did not bring an enemy aircraft down in 5 s");
    }
    return true;
}

// The October bunker powers: Upgrade All Bunkers (queues, pays as it goes, waits for money, can be cancelled), Rain Nukes (five fast, nearly
// unstoppable small warheads spread over a wide circle), Tank Drop (ten tanks and a medic on the silk) and the Para Backup's laser commandos
// (ten Laser Troopers in one, toxic clouds that poison only the enemy's units)
static bool powersTest18(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        init18(seed + 7 + fi, (Faction)fi);
        Player& pl = g_sim.players[0];
        const bool cy = fi == 0;
        Ref A = bunkerAt18(0, pl.basePos + Vec2(14.0f * TILE, 6.0f * TILE));
        Ref B = bunkerAt18(0, pl.basePos + Vec2(14.0f * TILE, -10.0f * TILE));
        run18(0.2f);
        // ---- Upgrade All Bunkers
        int cost = 0;
        CHECK18(g_sim.bunkerUpgradesMissing(0, &cost) == 2 * BU_COUNT, "a bare bunker does not lack every upgrade");
        int total = 0; for (int u = 0; u < BU_COUNT; u++) total += BUNKER_UPGS[u].cost;
        CHECK18(cost == 2 * total, "Upgrade All's price is not every missing upgrade's");
        CHECK18(!g_sim.cmdBunkerRain(A, pl.basePos) && !g_sim.cmdTankDrop(A, pl.basePos), "a bunker without the upgrades called Rain Nukes or a Tank Drop");
        pl.money = BUNKER_UPGS[BU_SHELL].cost + 10;   // enough for one fitting: the other bunker waits for the money
        CHECK18(g_sim.cmdBunkerUpgradeAll(0) == 2 * BU_COUNT, "Upgrade All did not queue every missing upgrade of both bunkers");
        CHECK18(g_sim.cmdBunkerUpgradeAll(0) == 0, "Upgrade All queued the same upgrades twice");
        Entity* a = g_sim.get(A); Entity* b = g_sim.get(B);
        CHECK18((a->bunkerBusy >= 0) != (b->bunkerBusy >= 0), "with money for one fitting, not exactly one bunker started");
        CHECK18(pl.money == 10, "the first queued fitting did not cost its price");
        run18(5.0f);
        CHECK18(g_sim.get(A)->bunkerBusy < 0 || g_sim.get(B)->bunkerBusy < 0, "a bunker started a fitting it could not pay for");
        g_sim.cancelBunkerQueue(0);
        CHECK18(g_sim.get(A)->bunkerQueue == 0 && g_sim.get(B)->bunkerQueue == 0, "cancel left a queue");
        pl.money = 200000;
        CHECK18(g_sim.cmdBunkerUpgradeAll(0) > 0, "Upgrade All refused after a cancel");
        float fitTime = 0; for (int u = 0; u < BU_COUNT; u++) fitTime += BUNKER_UPGS[u].time + 0.6f;
        run18(fitTime + 2.0f);
        a = g_sim.get(A); b = g_sim.get(B);
        CHECK18(a && b && a->bunkerUpg == (u8)((1 << BU_COUNT) - 1) && b->bunkerUpg == (u8)((1 << BU_COUNT) - 1), "Upgrade All did not fit every upgrade on every bunker");
        CHECK18(g_sim.bunkerUpgradesMissing(0) == 0 && a->bunkerQueue == 0, "a fully upgraded army still lacks something");
        // ---- Rain Nukes
        Vec2 tgt = g_sim.players[1].basePos + Vec2(4.0f * TILE, 0);
        size_t n0 = g_sim.nukes.size();
        CHECK18(g_sim.bunkerRainWait(*a) == 0 && g_sim.cmdBunkerRain(A, tgt), "a ready Rain Nukes call was refused");
        CHECK18(!g_sim.cmdBunkerRain(A, tgt), "Rain Nukes ignored the cooldown");
        CHECK18(g_sim.nukes.size() == n0 + RAIN_NUKES && RAIN_NUKES == 5, "Rain Nukes did not send five warheads");
        float minGap = 1e9f;
        for (size_t k = n0; k < g_sim.nukes.size(); k++) {
            const Sim::Nuke& n = g_sim.nukes[k];
            CHECK18(n.rain && dist(n.pos, tgt) <= RAIN_NUKE_AREA * TILE + 1.0f, "a rain warhead fell outside its circle");
            CHECK18(Sim::flightOf(n) <= 1.5f && Sim::nukeResist(n) <= 0.07f, "a rain warhead is not fast and hard to stop");
            for (size_t j = n0; j < k; j++) minGap = std::min(minGap, dist(n.pos, g_sim.nukes[j].pos));
        }
        CHECK18(minGap >= 1.5f * TILE, "the rain warheads were bunched on one spot");
        size_t f0 = g_sim.fallouts.size();
        run18(2.6f);
        CHECK18(g_sim.nukes.size() == n0 && g_sim.fallouts.size() >= f0 + RAIN_NUKES, "the rain warheads did not all come down within 2.6 s");
        g_sim.fallouts.clear();
        // ---- Tank Drop
        std::vector<int> tl = g_sim.bunkerTankManifest(0);
        int tanks = 0, medics = 0;
        for (int t : tl) { if (UNITS[t].role == UR_HEALER && UNITS[t].kind == UK_VEH) medics++; else if (UNITS[t].kind == UK_VEH && UNITS[t].armor == AR_HEAVY && UNITS[t].weapon >= 0) tanks++; }
        CHECK18((int)tl.size() == TANK_DROP_TANKS + 1 && tanks == TANK_DROP_TANKS && medics == 1, "a Tank Drop does not carry ten tanks and a medic");
        Vec2 zone = openGroundNear(pl.basePos + Vec2(-12.0f * TILE, 10.0f * TILE), 6);
        int vb = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().kind == UK_VEH) vb++;
        CHECK18(g_sim.cmdTankDrop(A, zone), "a ready Tank Drop was refused");
        CHECK18(!g_sim.cmdTankDrop(A, zone), "a Tank Drop ignored its cooldown");
        CHECK18(g_sim.airlifts.back().maxHp >= Sim::AIRLIFT_HP * 2.0f, "the Tank Drop plane is no tougher than an airlift");
        run18(40.0f);
        int landed = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().kind == UK_VEH && e.fall <= 0 && dist(e.pos, zone) < 10.0f * TILE) landed++;
        if (landed < TANK_DROP_TANKS + 1) { fprintf(stderr, "bunkertest: %d vehicles landed at the zone (%d before)\n", landed, vb); return fail18("the Tank Drop did not land its tanks and medic"); }
        // ---- the Para Backup's laser commandos: never trained, ten times a Laser Trooper, toxic clouds
        const int cmdT = cy ? U_C_LASERCMD : U_K_LASERCMD;
        const UnitType& ct = UNITS[cmdT]; const UnitType& lt = UNITS[U_C_INF2];
        const Weapon& cw = WEAPONS[ct.weapon]; const Weapon& lw = WEAPONS[lt.weapon];
        CHECK18(ct.builtBy == BR_NONE && !ct.temp, "a laser commando can be trained (or cannot be commanded)");
        CHECK18(ct.hp >= lt.hp * 10.0f - 0.5f && cw.dmg >= lw.dmg * 10.0f - 0.5f && cw.air && cw.ground, "a laser commando is not ten Laser Troopers");
        for (int k = 0; k < AR_COUNT; k++) CHECK18(cw.dmg * cw.mult[k] >= lw.dmg * lw.mult[k] * 10.0f - 0.5f, "the Toxic Lance is weaker than ten laser rifles against some armour");
        std::vector<int> pload = g_sim.bunkerParaManifest(0);
        int nc = 0; for (int t : pload) if (t == cmdT) nc++;
        CHECK18(nc == BUNKER_PARA_COMMANDOS && nc >= 1, "a Para Backup does not carry its laser commandos");
        // a toxic cloud poisons the enemy's soldiers and vehicles in it, never its own side's or a structure
        const int foeInf = cy ? U_K_INF1 : U_C_INF1, ownInf = cy ? U_C_INF1 : U_K_INF1;
        {
            Vec2 c1 = openGroundNear(g_sim.players[0].basePos + Vec2(-6.0f * TILE, -20.0f * TILE), 3);
            Ref mine = g_sim.spawnUnit(ownInf, 0, c1), foe = g_sim.spawnUnit(foeInf, 1, c1 + Vec2(6.0f, 0));
            g_sim.cmdStop({mine}); g_sim.cmdStop({foe});
            g_sim.get(mine)->disabledUntil = g_sim.get(foe)->disabledUntil = g_sim.time + 3.0f;   // (neither shoots the other: only the cloud acts)
            g_sim.toxicCloud(c1, 0);
            CHECK18(!g_sim.fallouts.empty() && g_sim.fallouts.back().toxic && g_sim.fallouts.back().enemyOnly, "toxicCloud made no enemy-only toxic zone");
            size_t nf = g_sim.fallouts.size();
            g_sim.toxicCloud(c1 + Vec2(8.0f, 0), 0);
            CHECK18(g_sim.fallouts.size() == nf, "a second hit beside a cloud stacked another instead of renewing it");
            run18(2.0f);
            Entity* me = g_sim.get(mine); Entity* fo = g_sim.get(foe);
            CHECK18(me && me->hp >= me->maxHp - 0.5f, "a toxic cloud poisoned its own side");
            CHECK18(!fo || fo->hp < fo->maxHp * 0.9f, "a toxic cloud did not poison an enemy soldier in it");
            run18(TOXIC_LIFE);
            bool left = false; for (auto& f : g_sim.fallouts) if (f.toxic) left = true;
            CHECK18(!left, "a toxic cloud outlived TOXIC_LIFE");
        }
        // a commando against a squad of enemy soldiers
        Vec2 arena = openGroundNear(g_sim.players[0].basePos + Vec2(-16.0f * TILE, -14.0f * TILE), 8);
        flattenArena(arena, 8, 8);
        Ref cr = g_sim.spawnUnit(cmdT, 0, arena);
        std::vector<Ref> foes;
        for (int k = 0; k < 6; k++) foes.push_back(g_sim.spawnUnit(foeInf, 1, arena + Vec2(5.0f * TILE + (k % 3) * 14.0f, (k / 3) * 14.0f - 7.0f)));
        size_t fall0 = g_sim.fallouts.size();
        bool toxic = false;
        for (int s2 = 0; s2 < 40 && !toxic; s2++) { run18(0.1f); for (auto& f : g_sim.fallouts) if (f.toxic && f.owner == 0 && f.enemyOnly) toxic = true; }
        CHECK18(toxic && g_sim.fallouts.size() > fall0, "the Toxic Lance left no toxic cloud");
        run18(8.0f);
        int alive = 0; for (Ref r : foes) if (g_sim.get(r)) alive++;
        CHECK18(alive == 0, "one laser commando did not wipe out six enemy soldiers in 12 s");
        CHECK18(g_sim.get(cr) != nullptr, "the laser commando fell to six riflemen");
        g_sim.fallouts.clear();
        // and an enemy gunship goes down fast
        int gunship = cy ? U_K_AIR : U_C_AIR;
        Ref gs = g_sim.spawnUnit(gunship, 1, g_sim.get(cr)->pos + Vec2(5.0f * TILE, 0));
        g_sim.cmdAttack({gs}, cr);
        run18(4.0f);
        CHECK18(!g_sim.get(gs), "a laser commando did not bring an enemy gunship down in 4 s");
    }
    return true;
}

static bool bulkTest18(u64 seed) {
    init18(seed, F_CYBER);
    Player& pl = g_sim.players[0];
    Vec2 c = openGroundNear(pl.basePos + Vec2(16.0f * TILE, 0), 9);
    flattenArena(c, 9, 9);
    int cx = tileOf(c.x), cy = tileOf(c.y);
    Ref dz = g_sim.spawnUnit(U_C_DOZER, 0, c + Vec2(0, 4.0f * TILE));
    Ref dz2 = g_sim.spawnUnit(U_C_DOZER, 0, c + Vec2(TILE, 4.0f * TILE));
    // the line of footprints (the UI's shift-drag)
    std::vector<std::pair<int, int>> spots;
    g_game.bulkTx = cx - 6; g_game.bulkTy = cy;
    g_game.bulkSpots(B_C_WALL, cx + 5, cy, spots);
    CHECK18(spots.size() == 12, "a 12-tile drag did not give 12 wall spots");
    for (size_t k = 0; k < spots.size(); k++) CHECK18(spots[k].first == cx - 6 + (int)k && spots[k].second == cy, "wall spots are not one tile apart along the line");
    std::vector<std::pair<int, int>> diag;
    g_game.bulkSpots(B_C_WALL, cx + 1, cy + 7, diag);
    CHECK18(diag.size() == 8 && diag.back().first == cx + 1 && diag.back().second == cy + 7, "a diagonal drag did not follow the line");
    std::vector<std::pair<int, int>> big;
    int rx = B_C_POWER, w = BUILDS[rx].w, h = BUILDS[rx].h;
    g_game.bulkSpots(rx, cx - 6 + 4 * w, cy, big);
    CHECK18(big.size() == 5, "a drag over five footprints did not give five");
    for (size_t k = 1; k < big.size(); k++) CHECK18(big[k].first - big[k - 1].first == w && big[k].second == big[0].second, "bigger structures are not laid edge to edge");
    g_game.bulkSpots(rx, cx - 6 + 4 * w, cy - 3 * h, big);   // a slanted drag: a staircase along it, still never two footprints on one tile
    for (size_t k = 1; k < big.size(); k++) CHECK18(std::abs(big[k].first - big[k - 1].first) >= w || std::abs(big[k].second - big[k - 1].second) >= h, "a slanted line put two footprints over each other");
    // placing the line
    int m0 = pl.money;
    int n = g_sim.cmdBuildLine({dz, dz2}, B_C_WALL, spots);
    CHECK18(n == 12, "not every wall of the line was placed");
    CHECK18(pl.money == m0 - 12 * BUILDS[B_C_WALL].cost, "the line did not cost one wall per segment");
    Entity* d1 = g_sim.get(dz); Entity* d2 = g_sim.get(dz2);
    CHECK18(d1->order == O_BUILD && d2->order == O_BUILD && d1->siteQueue.size() + d2->siteQueue.size() == 10, "the sites were not shared out between the two dozers");
    run18(150.0f);
    if (getenv("ONEHOUR_T18DBG")) fprintf(stderr, "bulk scene at %.1f s, game over %d\n", g_sim.time, (int)g_sim.gameOver);
    int built = 0; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0 && e.type == B_C_WALL && e.constructed) built++;
    if (built != 12) { fprintf(stderr, "bunkertest: %d of 12 walls finished\n", built); return fail18("the dozers did not finish the whole line"); }
    // money runs out partway: the rest of the line is not placed
    pl.money = BUILDS[B_C_WALL].cost * 3 + 10;
    std::vector<std::pair<int, int>> row2; for (int k = 0; k < 6; k++) row2.push_back({cx - 6 + k, cy + 3});
    CHECK18(g_sim.cmdBuildLine({dz}, B_C_WALL, row2) == 3, "a line went on past the money there was");
    return true;
}

static bool waymakerTest18(u64 seed) {
    init18(seed, F_CLANKER);
    Player& pl = g_sim.players[0];
    Vec2 c = openGroundNear(pl.basePos + Vec2(16.0f * TILE, 0), 7);
    flattenArena(c, 7, 7);
    int cx = tileOf(c.x), cy = tileOf(c.y);
    Ref dz = g_sim.spawnUnit(U_K_DOZER, 0, c + Vec2(0, 5.0f * TILE));
    // a wall segment on one corner of the patch, trees on four tiles beside it, open grass on the rest
    Ref wall = g_sim.placeBuilding(B_K_WALL, 0, cx - 1, cy - 1, true);
    int trees[4][2] = { {cx, cy - 1}, {cx + 1, cy - 1}, {cx - 1, cy}, {cx, cy + 1} };
    for (auto& t : trees) { int i = t[1] * MAP_W + t[0]; g_map.tiles[i] = T_TREE; g_map.blocked[i] |= 1; }
    g_map.touch();
    int way = B_K_WAYMAKER;
    CHECK18(g_sim.canPlace(0, way, cx - 1, cy - 1), "a Waymaker over a structure and the trees beside it was refused");
    CHECK18(g_sim.cmdBuild(dz, way, cx - 1, cy - 1), "the dozer could not place that Waymaker");
    CHECK18(!g_sim.canPlace(0, way, cx - 1, cy - 1), "a second Waymaker could be laid over trees the first is already clearing");
    run18(WAYMAKER_TIME + 1.0f);
    CHECK18(g_sim.get(wall) && g_sim.get(wall)->alive, "the Waymaker took the structure under it");
    CHECK18((g_map.blocked[(cy - 1) * MAP_W + cx - 1] & 2) != 0, "the structure's tile lost its mark");
    for (auto& t : trees) CHECK18(g_map.passable(t[0], t[1]) && g_map.tiles[t[1] * MAP_W + t[0]] != T_TREE, "a tree under the Waymaker was not cleared");
    bool gone = true; for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.type == way) gone = false;
    CHECK18(gone, "the Waymaker did not go once its work was done");
    // nothing left to clear: refused
    CHECK18(!g_sim.canPlace(0, way, cx - 1, cy - 1), "a Waymaker was accepted where only a structure and grass are left");
    return true;
}

// a computer commander with money and a bunker fits upgrades to it
static bool aiTest18(u64 seed) {
    init18(seed, F_CYBER, true);
    g_ai.init(seed);
    Ref br = bunkerAt18(1, g_sim.players[1].basePos + Vec2(-12.0f * TILE, 8.0f * TILE));
    g_sim.players[1].money = 60000;
    g_sim.time = 16.0f * 60.0f;
    run18(40.0f, true);
    Entity* b = g_sim.get(br);
    CHECK18(b, "the computer's bunker was lost in a quiet scene");
    CHECK18(b->bunkerUpg != 0 || b->bunkerBusy >= 0, "a rich computer commander never upgraded its bunker");
    return true;
}

bool bunkerUpdateTest(u64 seed) {
    g_ok18 = true;
    bool ok = hardeningTest18(seed) && garrisonTest18(seed) && upgradeTest18(seed) && powersTest18(seed) && bulkTest18(seed) && waymakerTest18(seed) && aiTest18(seed);
    if (ok) printf("bunkertest: ok (last scene ran to %.0f s)\n", g_sim.time);
    return ok;
}
