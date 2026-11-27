// The Ghosts: a submarine and its jet are trained as one pair, move as one, live and die together, and are found only by a Ghost2 sighting.
#include "sim.h"
#include "testutil.h"
#include <algorithm>
#include <cstdio>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
void setup(u64 seed) {
    Faction factions[] = {F_CYBER, F_CLANKER};
    bool ai[] = {false, false};
    int difficulty[] = {1, 1}, teams[] = {0, 1};
    g_sim.init(2, factions, ai, difficulty, teams, seed);
    g_sim.rain = g_sim.rainTo = 0; g_sim.nextRainRoll = 1e9f;   // (a dry sky: no stray fire or rain in these scenes)
    for (int p = 0; p < 2; p++) {   // (a known map, as the other naval scenes use: canPlace/exploredRaw would refuse every unexplored coast)
        g_sim.players[p].money = 999999;
        std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
    }
}

void advance(int ticks) {
    for (int i = 0; i < ticks; ++i) { g_sim.step(); g_sim.events.clear(); }
}

int buildFor(Faction faction, BuildRole role) {
    for (int i = 0; i < B_COUNT; ++i) if (BUILDS[i].faction == faction && BUILDS[i].role == role) return i;
    return -1;
}

int ghostSubFor(Faction faction) { return faction == F_CYBER ? U_C_GHOSTSUB : U_K_GHOSTSUB; }
int ghostJetFor(Faction faction) { return faction == F_CYBER ? U_C_GHOSTJET : U_K_GHOSTJET; }
int ghost2For(Faction faction) { return faction == F_CYBER ? U_C_GHOST2 : U_K_GHOST2; }

// A finished structure of 'type' on the nearest legal spot to 'near' (ring by ring).
Ref placeNear(int player, int type, Vec2 near, int maxR = 40) {
    if (type < 0) return NOREF;
    const int cx = tileOf(near.x), cy = tileOf(near.y);
    for (int r = 0; r <= maxR; ++r)
        for (int dy = -r; dy <= r; ++dy) for (int dx = -r; dx <= r; ++dx) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            const int tx = cx + dx, ty = cy + dy;
            if (g_sim.canPlace(player, type, tx, ty)) return g_sim.placeBuilding(type, player, tx, ty, true);
        }
    return NOREF;
}

struct Pair { Ref airfield, sub, jet; bool ok = false; };

// A normal two-army match, an Airfield and a coastal Naval Yard for player 0, and the Ghosts pair trained from it.
Pair makePair(u64 seed) {
    setup(seed);
    Pair p;
    const Faction f = g_sim.players[0].faction;
    const int air = buildFor(f, BR_AIRFIELD), naval = buildFor(f, BR_NAVAL);
    const int subType = ghostSubFor(f);
    if (air < 0 || naval < 0) return p;
    p.airfield = placeNear(0, air, g_sim.players[0].basePos);
    int tx = -1, ty = -1;
    if (!g_sim.get(p.airfield) || !g_sim.shoreSite(0, naval, g_sim.players[0].basePos, tx, ty)) return p;
    if (!g_sim.get(g_sim.placeBuilding(naval, 0, tx, ty, true))) return p;
    if (!g_sim.cmdTrain(p.airfield, subType)) return p;
    const int limit = (int)((UNITS[subType].buildTime * 2.5f + 10.0f) * SIM_HZ);   // (low power can halve the rate)
    for (int i = 0; i < limit; ++i) {
        g_sim.step(); g_sim.events.clear();
        if (g_sim.ghosts[0].sub.valid() && g_sim.ghosts[0].jet.valid()) break;
    }
    p.sub = g_sim.ghosts[0].sub;
    p.jet = g_sim.ghosts[0].jet;
    p.ok = g_sim.get(p.sub) && g_sim.get(p.jet);
    return p;
}

// The nearest open-water tile at least 'minTiles' away, of the same body of water as 'from'.
Vec2 farWater(Vec2 from, float minTiles) {
    const int comp = g_map.compAt(from);
    const float minD = minTiles * TILE;
    float best = 1e30f; Vec2 out;
    bool found = false;
    for (int y = 1; y < MAP_H - 1; ++y) for (int x = 1; x < MAP_W - 1; ++x) {
        if (!g_map.navOpen(x, y)) continue;
        const Vec2 c = tileCenter(x, y);
        if (comp && g_map.compAt(c) != comp) continue;
        const float d = dist(c, from);
        if (d < minD || d >= best) continue;
        best = d; out = c; found = true;
    }
    return found ? out : from;
}

bool isWater(Vec2 p) { return g_map.navOpen(tileOf(p.x), tileOf(p.y)); }

bool distFromBases(Vec2 pos, float tiles) {
    for (int p = 0; p < g_sim.numPlayers; ++p) if (dist(pos, g_sim.players[p].basePos) < tiles * TILE) return false;
    return true;
}
}

// The Bunker from Hell: its build limit, the garrison it comes with, its upgrades, warhead hardening, its own mending and its anti-nuke battery.
static bool hellTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("ghosttest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };

    // 1. at most two per army, a construction site counting
    {
        setup(seed);
        const Faction f = g_sim.players[0].faction;
        const int hell = f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER;
        const int tech = f == F_CYBER ? B_C_TECH : B_K_TECH, barracks = f == F_CYBER ? B_C_BARRACKS : B_K_BARRACKS;
        check(g_sim.get(placeNear(0, tech, g_sim.players[0].basePos)) && g_sim.get(placeNear(0, barracks, g_sim.players[0].basePos)),
              "a tech structure and a barracks were placed for the limit check");
        check(!g_sim.atBuildLimit(0, hell) && g_sim.buildAvailable(0, hell), "with its prerequisites a Bunker from Hell is available");
        check(g_sim.get(placeNear(0, hell, g_sim.players[0].basePos)) != nullptr, "the first Bunker from Hell was placed");
        check(g_sim.buildAvailable(0, hell), "one standing Bunker from Hell still leaves room for another");
        Ref site = NOREF;
        const int cx = tileOf(g_sim.players[0].basePos.x), cy = tileOf(g_sim.players[0].basePos.y);
        for (int r = 1; r <= 40 && !g_sim.get(site); ++r)
            for (int dy = -r; dy <= r && !g_sim.get(site); ++dy) for (int dx = -r; dx <= r; ++dx) {
                if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                if (g_sim.canPlace(0, hell, cx + dx, cy + dy)) { site = g_sim.placeBuilding(hell, 0, cx + dx, cy + dy, false); break; }
            }
        check(g_sim.get(site) && !g_sim.get(site)->constructed, "a second Bunker from Hell is under construction");
        check(g_sim.atBuildLimit(0, hell) && !g_sim.buildAvailable(0, hell),
              "a site counts: with two the third Bunker from Hell is refused");
    }

    // 2. the garrison at completion, every upgrade fitted, and no refill after it is let out
    {
        setup(seed);
        const Faction f = g_sim.players[0].faction;
        const int hell = f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER;
        const int sniper = f == F_CYBER ? U_C_SNIPER : U_K_SNIPER, commando = f == F_CYBER ? U_C_LASERCMD : U_K_LASERCMD;
        Ref hr = placeNear(0, hell, g_sim.players[0].basePos);
        check(g_sim.get(hr) != nullptr, "a finished Bunker from Hell was placed");
        advance(2);
        Entity* h = g_sim.get(hr);
        check(h && h->constructed && h->bunkerUpg == (u8)((1 << BU_COUNT) - 1), "it is finished with every bunker upgrade fitted");
        check(!g_sim.cmdBunkerUpgrade(hr, BU_SHELL), "an upgrade command is refused for it");
        int inf = 0, snipers = 0, commandos = 0, dozers = 0, tanks = 0;
        if (h) for (Ref r : h->passengers) {
            Entity* u = g_sim.get(r);
            if (!u || u->carrier != hr) continue;
            if (u->ut().kind == UK_INF) { inf++; if (u->type == sniper) snipers++; if (u->type == commando) commandos++; }
            else if (u->ut().role == UR_DOZER) dozers++;
            else if (u->ut().kind == UK_VEH) tanks++;
        }
        check(inf == HELL_GARRISON_INF && snipers == HELL_GARRISON_SNIPERS && commandos == HELL_GARRISON_COMMANDOS
              && dozers == HELL_GARRISON_DOZERS && tanks == HELL_GARRISON_TANKS,
              "it comes garrisoned: 24 infantry (6 snipers, 3 laser commandos), 2 dozers and 6 tanks");
        std::vector<Ref> aboard;
        if (h) aboard = h->passengers;
        g_sim.cmdUnload(hr, false, Vec2());
        advance(10 * SIM_HZ);
        int out = 0;
        for (Ref r : aboard) { Entity* u = g_sim.get(r); if (u && u->alive && u->isUnit() && !u->carrier.valid()) out++; }
        check((int)aboard.size() == HELL_GARRISON_INF + HELL_GARRISON_DOZERS + HELL_GARRISON_TANKS && out == (int)aboard.size(),
              "the existing Unload command lets the whole garrison out as real, alive units");
        h = g_sim.get(hr);
        check(h && h->passengers.empty(), "the bunker is left empty");
        g_sim.cmdUnload(hr, false, Vec2());
        advance(2 * SIM_HZ);
        h = g_sim.get(hr);
        check(h && h->passengers.empty(), "a second Unload does not refill the garrison");

        // reload: an unloaded tank climbs back into the Bunker from Hell, a Deep Bunker stays closed to it
        Ref tank = NOREF, jet = NOREF;
        for (Ref r : aboard) { Entity* u = g_sim.get(r); if (u && u->alive && u->ut().kind == UK_VEH && u->ut().role != UR_DOZER) { tank = r; break; } }
        const Faction ef = g_sim.players[0].faction;
        check(h && g_sim.get(tank) && g_sim.canBoard(*g_sim.get(tank), *h), "an unloaded tank may board the Bunker from Hell again");
        std::vector<Ref> riders = {tank};
        g_sim.cmdEnter(riders, hr);
        advance(30 * SIM_HZ);
        Entity* t = g_sim.get(tank);
        check(t && t->carrier == hr, "the tank drives in and is carried by the bunker");
        int bay = 0;
        for (Ref r : aboard) { Entity* u = g_sim.get(r); if (u && u->alive && u->ut().kind == UK_VEH && u->ut().role != UR_DOZER) { riders.assign(1, r); g_sim.cmdEnter(riders, hr); } }
        advance(60 * SIM_HZ);
        h = g_sim.get(hr);
        if (h) for (Ref r : h->passengers) { Entity* u = g_sim.get(r); if (u && u->ut().kind == UK_VEH && u->ut().role != UR_DOZER) bay++; }
        check(bay == HELL_GARRISON_TANKS, "the whole tank bay can be loaded again, and no more");
        jet = g_sim.spawnUnit(ef == F_CYBER ? U_C_GHOST2 : U_K_GHOST2, 0, g_sim.players[0].basePos);
        h = g_sim.get(hr);
        check(h && g_sim.get(jet) && !g_sim.canBoard(*g_sim.get(jet), *h), "an aircraft cannot board it");
        Ref deep = placeNear(0, buildFor(ef, BR_BUNKER), g_sim.players[0].basePos);
        Ref extra = g_sim.spawnUnit(ef == F_CYBER ? U_C_TANK : U_K_TANK, 0, g_sim.players[0].basePos);
        check(g_sim.get(deep) && g_sim.get(extra) && !g_sim.canBoard(*g_sim.get(extra), *g_sim.get(deep)), "a tank cannot board a Deep Bunker");
        h = g_sim.get(hr);
        check(g_sim.get(extra) && h && !g_sim.canBoard(*g_sim.get(extra), *h), "a full Bunker from Hell takes no seventh tank");

        g_sim.cmdUnload(hr, false, Vec2());
        advance(10 * SIM_HZ);
        h = g_sim.get(hr);
        check(h && h->passengers.empty(), "the tank bay starts empty for simultaneous boarding");
        riders.clear();
        for (Ref r : aboard) {
            const Entity* u = g_sim.get(r);
            if (u && u->ut().kind == UK_VEH && u->ut().role != UR_DOZER) riders.push_back(r);
        }
        riders.push_back(extra);
        std::vector<float> hp;
        for (Ref r : riders) { const Entity* u = g_sim.get(r); hp.push_back(u ? u->hp : 0.0f); }
        g_sim.cmdEnter(riders, hr);
        int ordered = 0;
        for (Ref r : riders) { const Entity* u = g_sim.get(r); if (u && u->order == O_ENTER && u->targetEnt == hr) ordered++; }
        check(ordered == HELL_GARRISON_TANKS + 1, "all seven tanks receive enter orders before any board");
        advance(60 * SIM_HZ);
        h = g_sim.get(hr);
        int boarded = 0, outside = 0, unharmed = 0;
        for (size_t i = 0; i < riders.size(); i++) {
            const Entity* u = g_sim.get(riders[i]);
            if (!u) continue;
            if (u->hp == hp[i]) unharmed++;
            if (u->carrier == hr) boarded++;
            else if (!u->carrier.valid() && u->order != O_ENTER && h && g_sim.distToEntity(u->pos, *h) > 0) outside++;
        }
        check(h && h->passengers.size() == HELL_GARRISON_TANKS && boarded == HELL_GARRISON_TANKS,
              "simultaneous arrivals load exactly six tanks");
        check(outside == 1 && unharmed == HELL_GARRISON_TANKS + 1, "the seventh tank stays outside and all seven survive unharmed");
    }

    // 3. ten times a Deep Bunker's health, a warhead does 0.08x to it, and its own mending rule
    {
        setup(seed);
        const Faction f = g_sim.players[0].faction;
        const int hell = f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER, deep = buildFor(f, BR_BUNKER);
        Ref hr = placeNear(0, hell, g_sim.players[0].basePos);
        Ref dr = placeNear(0, deep, g_sim.players[0].basePos + Vec2(40.0f * TILE, 0));
        check(g_sim.get(hr) && g_sim.get(dr), "a Bunker from Hell and a Deep Bunker were placed for the warhead check");
        advance(2);
        Entity* h = g_sim.get(hr);
        Entity* d = g_sim.get(dr);
        check(h && d && std::fabs(h->maxHp - 10.0f * d->maxHp) < 1.0f, "it has ten times a Deep Bunker's health");
        if (h && d) {
            d->bunkerUpg = h->bunkerUpg;   // the same shell, so only the Hell hardening differs
            const float hh = h->hp, dh = d->hp;
            Sim::Nuke nh{}; nh.owner = 1; nh.pos = h->pos; nh.scale = 1.0f;
            g_sim.nukeBlastPublic(nh);
            Sim::Nuke nd{}; nd.owner = 1; nd.pos = d->pos; nd.scale = 1.0f;
            g_sim.nukeBlastPublic(nd);
            h = g_sim.get(hr); d = g_sim.get(dr);
            const float hrLoss = h ? (hh - h->hp) / h->maxHp : 0.0f;
            const float drLoss = d ? (dh - d->hp) / d->maxHp : 0.0f;
            check(drLoss > 0.05f && std::fabs(hrLoss / drLoss - HELL_NUKE_DAMAGE) < 0.01f,
                  "a plain warhead takes 0.08x of what it takes from a Deep Bunker");
            // let the garrison out so no dozer inside mends it, then measure its own rule alone
            g_sim.cmdUnload(hr, false, Vec2());
            advance(10 * SIM_HZ);
            h = g_sim.get(hr);
            if (h) { h->hp = h->maxHp * 0.5f; h->lastDamaged = g_sim.time; }
            const float hp0 = h ? h->hp : 0.0f;
            advance(2 * SIM_HZ);
            h = g_sim.get(hr);
            check(h && std::fabs(h->hp - hp0) < 1.0f, "it mends nothing during the first 3 s after a hit");
            advance(2 * SIM_HZ);
            h = g_sim.get(hr);
            const float gain = h ? (h->hp - hp0) / h->maxHp : 0.0f;
            check(gain > 0.8f * HELL_REGEN && gain < 1.8f * HELL_REGEN, "it then mends 1.5% of its health per second");
        } else {
            check(false, "a plain warhead takes 0.08x of what it takes from a Deep Bunker");
            check(false, "it mends nothing during the first 3 s after a hit");
            check(false, "it then mends 1.5% of its health per second");
        }
    }

    // 4. its anti-nuke battery: 30 tiles, six shots, 90%, big warheads engaged, an uninterceptable one never
    {
        setup(seed);
        const Faction f = g_sim.players[0].faction;
        const int hell = f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER;
        check(HELL_ANTINUKE_COVER == 30.0f && HELL_ANTINUKE_SHOTS == 6 && std::fabs(HELL_ANTINUKE_CHANCE - 0.90f) < 1e-4f,
              "the battery's settings are 30 tiles, six shots and 90%");
        Ref hr = placeNear(0, hell, g_sim.players[0].basePos);
        advance(2);
        Entity* h = g_sim.get(hr);
        check(h != nullptr, "a finished Bunker from Hell was placed for the anti-nuke check");
        auto launch = [&](int id, Vec2 at, bool deep, bool uninterceptable, float dmgMul) {
            Sim::Nuke n{}; n.id = id; n.owner = 1; n.pos = at; n.from = at + Vec2(0, -400);
            n.scale = 1.0f; n.deep = deep; n.sub = deep; n.uninterceptable = uninterceptable; n.dmgMul = dmgMul;
            n.t = Sim::flightOf(n) - 4.0f; g_sim.nukes.push_back(n);
        };
        auto engaged = [&](int id) { for (auto& it : g_sim.interceptors) if (it.nukeId == id) return true; return false; };
        auto shotsAt = [&](int id) { int n = 0; for (auto& it : g_sim.interceptors) if (it.nukeId == id) n++; return n; };
        h = g_sim.get(hr);
        if (h) {
            const Vec2 near = h->pos + Vec2(20.0f * TILE, 0);
            h->antiNukeCd = 0;
            launch(9101, near, true, false, 1.0f);    // a Ghost submarine warhead: deep and sub
            advance(1);
            check(shotsAt(9101) == HELL_ANTINUKE_SHOTS, "a warhead 20 tiles out draws six interceptors");
            g_sim.nukes.clear(); g_sim.interceptors.clear();
            h = g_sim.get(hr); if (h) h->antiNukeCd = 0;
            launch(9102, near, false, false, 5.0f);   // a Ghost jet warhead: plain, five times the damage
            advance(1);
            check(engaged(9102), "a Ghost warhead aimed inside 30 tiles is engaged");
            g_sim.nukes.clear(); g_sim.interceptors.clear();
            h = g_sim.get(hr); if (h) h->antiNukeCd = 0;
            launch(9103, near, false, true, 1.0f);    // the later nostopme warhead
            advance(1);
            check(!engaged(9103), "an uninterceptable warhead is left alone");
            g_sim.nukes.clear(); g_sim.interceptors.clear();
            h = g_sim.get(hr); if (h) h->antiNukeCd = 0;
            launch(9104, h->pos + Vec2(80.0f * TILE, 0), false, false, 1.0f);
            advance(1);
            check(!engaged(9104), "a warhead aimed beyond 30 tiles is not engaged");
            g_sim.nukes.clear(); g_sim.interceptors.clear();
        } else {
            check(false, "a warhead 20 tiles out draws six interceptors");
            check(false, "a Ghost warhead aimed inside 30 tiles is engaged");
            check(false, "an uninterceptable warhead is left alone");
            check(false, "a warhead aimed beyond 30 tiles is not engaged");
        }
    }
    return ok;
}

// nostopme (B14/B15): the Bunker from Hell's one uninterceptable warhead - its clock, its composition, its immunity and the shield of a
// Bunker from Hell on the target's side.
static bool nostopmeTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("ghosttest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };
    auto hellFor = [](Faction f) { return f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER; };

    // 1. armed 300 s after completion, then once every 600 s; what it fires
    {
        setup(seed);
        const Faction f = g_sim.players[0].faction;
        Ref hr = placeNear(0, hellFor(f), g_sim.players[0].basePos);
        check(g_sim.get(hr) != nullptr, "a finished Bunker from Hell was placed for the nostopme check");
        advance(2);
        Entity* h = g_sim.get(hr);
        check(h && std::fabs(g_sim.nostopmeCooldownLeft(*h) - HELL_NOSTOPME_FIRST) < 2.0f,
              "nostopme is armed 300 s after the bunker is finished");
        const Vec2 at = g_sim.players[1].basePos;
        check(!g_sim.cmdNostopme(0, hr, at) && g_sim.nukes.empty(), "nostopme is refused right after completion");
        g_sim.time = h->nostopmeReadyAt - 100.0f;
        check(!g_sim.cmdNostopme(0, hr, at) && g_sim.nukes.empty(), "nostopme is refused before 300 s");
        g_sim.time = h->nostopmeReadyAt;
        check(g_sim.cmdNostopme(0, hr, at), "nostopme is accepted at 300 s");
        check(g_sim.nukes.size() == 1, "nostopme adds exactly one warhead");
        if (g_sim.nukes.size() == 1) {
            const Sim::Nuke& n = g_sim.nukes[0];
            h = g_sim.get(hr);
            check(h && n.owner == 0 && n.uninterceptable && n.nostopme && dist(n.from, h->pos) < 1.0f,
                  "it is the firer's warhead, cannot be intercepted, and leaves the bunker itself");
            check(std::fabs(Sim::flightOf(n) - HELL_NOSTOPME_FLIGHT) < 1e-4f && HELL_NOSTOPME_FLIGHT == 1.5f,
                  "it flies 1.5 s instead of NUKE_FLIGHT");
            check(std::fabs(n.scale - std::max(SUB_NUKE_SCALE * 4.0f, NUKEBOMBER_SCALE * 9.0f)) < 1e-4f,
                  "its radius is the larger of the two Ghost warheads' (the submarine's)");
            check(std::fabs(Sim::nukeDamageFactor(n) - (SUB_NUKE_DAMAGE * 2.0f + 5.0f)) < 1e-3f,
                  "its damage factor is the sum of both Ghost warheads'");
        }
        check(!g_sim.cmdNostopme(0, hr, at), "a second nostopme is refused at once");
        g_sim.time = h->nostopmeReadyAt - 1.0f;
        check(!g_sim.cmdNostopme(0, hr, at), "nostopme stays refused for 600 s");
        g_sim.time += 1.0f;
        check(g_sim.cmdNostopme(0, hr, at) && g_sim.nukes.size() == 2, "nostopme is accepted again 600 s later");
        h = g_sim.get(hr);
        check(h && std::fabs(g_sim.nostopmeCooldownLeft(*h) - HELL_NOSTOPME_COOLDOWN) < 1.0f,
              "the UI reads the full cooldown back after a launch");
    }

    // 2. nothing intercepts it, while an ordinary warhead at the same spot is engaged and the nostopme still lands
    {
        setup(seed);
        const Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction;
        Ref hr = placeNear(0, hellFor(f0), g_sim.players[0].basePos);
        check(g_sim.get(hr) != nullptr, "a Bunker from Hell was placed to launch the nostopme");
        advance(2);
        Ref hq = NOREF;
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && e.bt().role == BR_HQ) { hq = g_sim.refOf(e); break; }
        check(g_sim.get(hq) != nullptr, "the target army's Command Core stands for the launch");
        const Vec2 at = g_sim.get(hq) ? g_sim.get(hq)->pos : g_sim.players[1].basePos;
        const int hell1 = hellFor(f1), anuke = buildFor(f1, BR_ANTINUKE);
        check(g_sim.get(placeNear(1, hell1, at)) != nullptr && g_sim.get(placeNear(1, anuke, at)) != nullptr,
              "an enemy Bunker from Hell and an Anti-Nuke Laser guard the target");
        Entity* h = g_sim.get(hr);
        g_sim.time = h->nostopmeReadyAt;
        check(g_sim.cmdNostopme(0, hr, at), "nostopme launches at the guarded target");
        Sim::Nuke ord{}; ord.id = 9001; ord.owner = 0; ord.pos = at; ord.from = g_sim.players[0].basePos;
        ord.scale = 1.0f; ord.t = Sim::NUKE_FLIGHT - 4.0f;
        g_sim.nukes.push_back(ord);
        g_sim.step(); g_sim.events.clear();
        int nid = 0;
        for (auto& n : g_sim.nukes) if (n.nostopme) nid = n.id;
        bool ordEngaged = false;
        for (auto& it : g_sim.interceptors) if (it.nukeId == 9001) ordEngaged = true;
        check(nid != 0 && ordEngaged, "an ordinary warhead at the same spot is engaged");
        bool interfered = false, landed = false;
        for (int i = 0; i < 2 * SIM_HZ && !landed; i++) {
            g_sim.step(); g_sim.events.clear();
            for (auto& it : g_sim.interceptors) if (it.nukeId == nid) interfered = true;
            landed = true;
            for (auto& n : g_sim.nukes) if (n.nostopme) landed = false;
        }
        check(!interfered, "no defence fires at the nostopme warhead");
        check(landed, "the nostopme warhead still lands 1.5 s after launch");
    }

    // 3. B15: an enemy Bunker from Hell in the blast cuts the warhead to a fifth; an ally's (here the firer's own) does not
    auto lossOnEnemyCore = [&](bool enemyHell, bool ownHell) -> float {
        setup(seed);
        Ref hq = NOREF;
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 1 && e.bt().role == BR_HQ) { hq = g_sim.refOf(e); break; }
        if (!g_sim.get(hq)) return -1.0f;
        const Vec2 at = g_sim.get(hq)->pos;
        if (enemyHell && !g_sim.get(placeNear(1, hellFor(g_sim.players[1].faction), at))) return -1.0f;
        if (ownHell && !g_sim.get(placeNear(0, hellFor(g_sim.players[0].faction), at))) return -1.0f;
        Entity* h = g_sim.get(hq);
        const float hp0 = h->hp;
        Sim::Nuke n{}; n.owner = 0; n.pos = h->pos; n.nostopme = true; n.uninterceptable = true;
        n.scale = std::max(SUB_NUKE_SCALE * 4.0f, NUKEBOMBER_SCALE * 9.0f); n.dmgMul = SUB_NUKE_DAMAGE * 2.0f + 5.0f;
        n.flight = HELL_NOSTOPME_FLIGHT;
        g_sim.nukeBlastPublic(n);
        h = g_sim.get(hq);
        return h ? hp0 - h->hp : hp0;
    };
    const float l0 = lossOnEnemyCore(false, false), l1 = lossOnEnemyCore(true, false), l2 = lossOnEnemyCore(false, true);
    check(l0 > 0 && l1 > 0 && std::fabs(l1 / l0 - HELL_NOSTOPME_CUT) < 0.02f,
          "an enemy Bunker from Hell in the blast cuts the damage to 20%");
    check(l0 > 0 && l2 > 0 && std::fabs(l2 / l0 - 1.0f) < 0.02f,
          "the firer's own Bunker from Hell in the blast leaves the damage at 100%");
    return ok;
}

// B13: the Bunker from Hell's two autonomous ballistic launchers - what they hit, what they leave alone, and their cadence.
static bool launcherTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("ghosttest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };
    auto hellFor = [](Faction f) { return f == F_CYBER ? B_C_HELLBUNKER : B_K_HELLBUNKER; };
    auto hostile = [&](int type, Vec2 at) {
        Ref r = g_sim.spawnUnit(type, 1, at);
        Entity* u = g_sim.get(r);
        if (u) { u->disabledUntil = 1e9f; u->order = O_IDLE; }
        return r;
    };
    auto liveRockets = [&]() { int n = 0; for (auto& p : g_sim.projs) if (p.alive && p.weapon == W_HELL_ROCKET) n++; return n; };

    // 1. a tank and an aircraft within 12 tiles are destroyed; a unit beyond 16 tiles and an unsighted Ghost jet are untouched
    {
        setup(seed);
        const Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction;
        Ref hr = placeNear(0, hellFor(f0), g_sim.players[0].basePos);
        check(g_sim.get(hr) != nullptr, "a finished Bunker from Hell was placed for the launcher check");
        advance(2);
        Entity* h = g_sim.get(hr);
        if (!h) {
            check(false, "a tank and an aircraft within 12 tiles are destroyed within 15 s");
            check(false, "a unit beyond 16 tiles is not hit");
            check(false, "an unsighted Ghost jet within range is not damaged");
        } else {
            h->hatch = 0; h->bunkerUpg = 0;   // (only its launchers fire here: no hatch guns and no laser battery)
            const Vec2 c = h->pos;
            const int tankType = f1 == F_CYBER ? U_C_TANK : U_K_TANK;
            const int airType = f1 == F_CYBER ? U_C_HELI : U_K_AIR;
            const int jetType = f1 == F_CYBER ? U_C_GHOSTJET : U_K_GHOSTJET;
            Ref tank = hostile(tankType, c + Vec2(10.0f * TILE, 0));
            Ref air = hostile(airType, c + Vec2(-10.0f * TILE, 0));
            Ref far = hostile(tankType, c + Vec2(0, 18.0f * TILE));
            Ref ghost = hostile(jetType, c + Vec2(6.0f * TILE, 6.0f * TILE));
            Entity* farU = g_sim.get(far); Entity* ghostU = g_sim.get(ghost);
            const float farHp = farU ? farU->hp : 0.0f, ghostHp = ghostU ? ghostU->hp : 0.0f;
            advance(15 * SIM_HZ);
            Entity* t = g_sim.get(tank); Entity* a = g_sim.get(air);
            farU = g_sim.get(far); ghostU = g_sim.get(ghost);
            check((!t || t->hp < t->maxHp * 0.5f) && (!a || a->hp < a->maxHp * 0.5f),
                  "a tank and an aircraft within 12 tiles are destroyed within 15 s");
            check(farU && std::fabs(farU->hp - farHp) < 1.0f, "a unit beyond 16 tiles is not hit");
            check(ghostU && std::fabs(ghostU->hp - ghostHp) < 1.0f, "an unsighted Ghost jet within range is not damaged");
        }
    }

    // 2. the cadence: two launchers, about 2 s each, so about ten rockets in ten seconds
    {
        setup(seed);
        const Faction f0 = g_sim.players[0].faction, f1 = g_sim.players[1].faction;
        Ref hr = placeNear(0, hellFor(f0), g_sim.players[0].basePos);
        advance(2);
        Entity* h = g_sim.get(hr);
        check(h != nullptr, "a second Bunker from Hell was placed for the cadence check");
        if (h) {
            h->hatch = 0; h->bunkerUpg = 0;
            Ref dummy = hostile(f1 == F_CYBER ? U_C_TANK : U_K_TANK, h->pos + Vec2(11.0f * TILE, 0));
            Entity* d = g_sim.get(dummy);
            if (d) { d->maxHp = d->hp = 1e9f; }   // (a target that cannot die, so both launchers keep firing for the whole window)
            int prev = 0, fired = 0;
            for (int i = 0; i < 10 * SIM_HZ; i++) {
                g_sim.step(); g_sim.events.clear();
                const int cur = liveRockets();
                if (cur > prev) fired += cur - prev;
                prev = cur;
            }
            check(d != nullptr && fired >= 8 && fired <= 12, "two launchers fire about ten rockets in ten seconds (8..12)");
        } else {
            check(false, "two launchers fire about ten rockets in ten seconds (8..12)");
        }
    }
    return ok;
}

bool ghostTest(u64 seed) {
    bool ok = true;
    auto check = [&](bool pass, const char* text) { printf("ghosttest %s: %s\n", pass ? "PASS" : "FAIL", text); ok &= pass; };
    const Faction f = F_CYBER;

    // 1. one pair per army, and only when a finished Airfield and a finished Naval Yard both stand
    {
        setup(seed);
        const int air = buildFor(f, BR_AIRFIELD), naval = buildFor(f, BR_NAVAL), subType = ghostSubFor(f);
        check(air >= 0 && naval >= 0, "the default map offers an Airfield and a Naval Yard build");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.unitAvailable(0, subType),
              "without an Airfield the Ghosts are blocked and unavailable");
        Ref af = placeNear(0, air, g_sim.players[0].basePos);
        check(g_sim.get(af) != nullptr, "a finished Airfield was placed for the scene");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.cmdTrain(af, subType),
              "the Airfield refuses the Ghosts without a finished Naval Yard");
        int tx = -1, ty = -1;
        const bool shore = g_sim.shoreSite(0, naval, g_sim.players[0].basePos, tx, ty);
        Ref yard = shore ? g_sim.placeBuilding(naval, 0, tx, ty, true) : NOREF;
        check(g_sim.get(yard) != nullptr, "a finished Naval Yard was placed on the shore");
        check(g_sim.ghostTrainBlockReason(0) == nullptr && g_sim.unitAvailable(0, subType),
              "with both structures finished the Ghosts are available");
        check(g_sim.cmdTrain(af, subType), "the Airfield accepts the Ghosts pair");
        check(!g_sim.cmdTrain(af, subType) && g_sim.ghostTrainBlockReason(0) != nullptr,
              "a second pair is refused while the first is queued");
        advance((int)((UNITS[subType].buildTime * 2.5f + 10.0f) * SIM_HZ));
        check(g_sim.get(g_sim.ghosts[0].sub) && g_sim.get(g_sim.ghosts[0].jet),
              "the queued pair is trained after its build time");
        check(g_sim.ghostTrainBlockReason(0) != nullptr && !g_sim.cmdTrain(af, subType),
              "a second pair is refused while the first is alive");
    }

    // 2. where and how the pair is born
    {
        Pair p = makePair(seed);
        check(p.ok, "the Ghosts pair exists after training");
        Entity* s = g_sim.get(p.sub);
        Entity* j = g_sim.get(p.jet);
        check(s && j && s->owner == 0 && j->owner == 0, "both halves belong to the training army");
        check(s && j && s->partner == p.jet && j->partner == p.sub, "the halves reference each other both ways");
        check(s && j && s->type == ghostSubFor(F_CYBER) && j->type == ghostJetFor(F_CYBER), "the pair has the right sub and jet types");
        check(s && j && distFromBases(s->pos, 60.0f) && distFromBases(j->pos, 60.0f),
              "the pair spawns at least 60 tiles from every base");
        check(s && isWater(s->pos) && g_map.openSea(tileOf(s->pos.x), tileOf(s->pos.y)), "the sub spawns in open sea");
    }

    // 3. the jet follows the sub; a land order becomes a move onto water
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the move check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 from = s->pos;
            const Vec2 dest = farWater(from, 15.0f);
            check(isWater(dest) && dist(dest, from) >= 15.0f * TILE, "a water destination 15+ tiles away was found");
            g_sim.cmdMove({p.sub}, dest, false);
            Entity* s2 = g_sim.get(p.sub);
            check(s2 && !s2->path.empty() && isWater(s2->target), "the sub's path leads toward open water");
            bool together = true;
            int ticks = 0;
            for (; ticks < 40 * SIM_HZ; ++ticks) {
                g_sim.step(); g_sim.events.clear();
                Entity* a = g_sim.get(p.sub); Entity* b = g_sim.get(p.jet);
                if (!a || !b) { together = false; break; }
                together &= dist(a->pos, b->pos) < 0.01f;
                if (dist(a->pos, a->target) < 2.0f * TILE) break;
            }
            check(together, "the jet sits on the sub on every tick of the move");
            Entity* a = g_sim.get(p.sub);
            check(a && isWater(a->pos) && dist(a->pos, dest) < 4.0f * TILE, "the pair arrives on the destination water");
            g_sim.cmdMove({p.sub}, g_sim.players[1].basePos, false);
            Entity* b = g_sim.get(p.sub);
            check(b && isWater(b->target), "an order onto land snaps the sub's goal to water");
            advance(2 * SIM_HZ);
            b = g_sim.get(p.sub);
            check(b && isWater(b->pos), "the sub stays on water after a land order");
            Ref building = placeNear(1, buildFor(F_CLANKER, BR_BARRACKS), g_sim.players[1].basePos);
            check(g_sim.get(building) != nullptr, "a land structure was placed for the attack order");
            if (g_sim.get(building)) {
                g_sim.cmdAttack({p.jet}, building);
                Entity* c = g_sim.get(p.sub);
                check(c && isWater(c->target), "an attack on a land structure ordered on the jet becomes a move to water");
                advance(2 * SIM_HZ);
                c = g_sim.get(p.sub);
                check(c && isWater(c->pos), "the pair stays on water after the attack order");
            } else {
                check(false, "an attack on a land structure ordered on the jet becomes a move to water");
            }
        }
    }

    // 4. the submarine stays dived through a move and a strike
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the dive check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 dest = farWater(s->pos, 15.0f);
            g_sim.cmdMove({p.sub}, dest, false);
            advance(3 * SIM_HZ);
            s = g_sim.get(p.sub);
            check(s && s->subState == 2, "the sub is dived after moving");
            g_sim.time = g_sim.ghosts[0].strikeReadyAt;
            const bool struck = g_sim.cmdGhostStrike(0, g_sim.players[1].basePos);
            advance(1);
            s = g_sim.get(p.sub);
            check(struck && s && s->subState == 2, "the sub is still dived after a strike");
        }
    }

    // 5. snipers, spy drones, AA and bunkers never find the pair; only a Ghost2 can
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the detection check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            Ref sniper = g_sim.spawnUnit(U_K_SNIPER, 1, s->pos + Vec2(3.0f * TILE, 0));
            Ref drone = g_sim.spawnUnit(U_K_SDRONE, 1, s->pos + Vec2(-3.0f * TILE, 0));
            // the pair sits in open sea, where a structure has no legal ground: fall back to standing the two enemy structures on the water
            // beside it (this check is about detection, not placement)
            const int aaType = buildFor(F_CLANKER, BR_AATURRET), bunkerType = buildFor(F_CLANKER, BR_BUNKER);
            const int sx = tileOf(s->pos.x), sy = tileOf(s->pos.y);
            const int fx = clampi(sx + 4, 1, MAP_W - 4), fy = clampi(sy + 4, 1, MAP_H - 4);
            Ref aa = placeNear(1, aaType, s->pos, 24);
            if (!g_sim.get(aa)) aa = g_sim.placeBuilding(aaType, 1, fx, sy, true);
            Ref bunker = placeNear(1, bunkerType, s->pos, 24);
            if (!g_sim.get(bunker)) bunker = g_sim.placeBuilding(bunkerType, 1, sx, fy, true);
            Ref rpg = g_sim.spawnUnit(U_K_INF2, 1, s->pos + Vec2(0, 4.0f * TILE));
            check(g_sim.get(sniper) && g_sim.get(drone), "an enemy sniper and spy drone stand beside the pair");
            check(g_sim.get(aa) && g_sim.get(bunker), "an enemy AA turret and bunker stand near the pair");
            advance(3 * SIM_HZ);
            Entity* sub = g_sim.get(p.sub);
            Entity* jet = g_sim.get(p.jet);
            Entity* r = g_sim.get(rpg);
            check(sub && jet && !g_sim.visibleTo(*sub, 1) && !g_sim.visibleTo(*jet, 1),
                  "enemy snipers, drones, AA and bunkers do not reveal the pair");
            check(r && sub && jet && !g_sim.canTarget(*r, *sub, -1) && !g_sim.canTarget(*r, *jet, -1),
                  "an enemy soldier cannot target either half without a Ghost2 sighting");
        }
    }

    // 6. a Ghost2 sighting reveals the pair, then it slips away and cannot be re-sighted while cloaked
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the sighting check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            Ref g2 = g_sim.spawnUnit(ghost2For(F_CLANKER), 1, s->pos + Vec2(3.0f * TILE, 0));
            advance(2);
            check(g_sim.get(g2) != nullptr && g_sim.ghosts[0].sighting.until > g_sim.time,
                  "a Ghost2 within 9 tiles creates a sighting");
            Entity* sub = g_sim.get(p.sub);
            Entity* jet = g_sim.get(p.jet);
            check(sub && jet && g_sim.visibleTo(*sub, 1) && g_sim.visibleTo(*jet, 1),
                  "the sighting reveals both halves");
            Ref rpg = g_sim.spawnUnit(U_K_INF2, 1, s->pos + Vec2(5.0f * TILE, 0));
            Entity* r = g_sim.get(rpg);
            if (r) r->disabledUntil = 1e9f;   // it may aim (canTarget) but must not shoot the pair apart during the 10 s sighting
            check(r && sub && jet && g_sim.canTarget(*r, *sub, -1) && g_sim.canTarget(*r, *jet, -1),
                  "an enemy armed unit may target both halves during the sighting");
            Entity* j = g_sim.get(p.jet);
            if (j) {
                const float hp0 = j->hp;
                g_sim.applyDamagePublic(*j, 100.0f, 1, NOREF);
                const float loss = hp0 - j->hp;
                check(std::fabs(loss - 30.0f) < 8.0f, "the jet takes about 0.3x of an ordinary hit");
            } else {
                check(false, "the jet takes about 0.3x of an ordinary hit");
            }
            Entity* before = g_sim.get(p.sub);
            const Vec2 from = before ? before->pos : Vec2();
            int ticks = 0;
            while (g_sim.ghosts[0].sighting.until > g_sim.time && ticks < 15 * SIM_HZ) { g_sim.step(); g_sim.events.clear(); ++ticks; }
            advance(2);
            Entity* after = g_sim.get(p.sub);
            check(after && dist(after->pos, from) >= 40.0f * TILE, "after the sighting the pair slips 40+ tiles");
            check(after && !g_sim.visibleTo(*after, 1) && g_sim.get(p.jet) && !g_sim.visibleTo(*g_sim.get(p.jet), 1),
                  "the pair is hidden again once the sighting ends");
            const float cloak = g_sim.ghosts[0].cloakUntil - g_sim.time;
            check(cloak >= 9.0f && cloak <= 60.0f, "the cloak then lasts between 10 and 60 s");
            Entity* d = g_sim.get(g2);
            if (d && after) d->pos = after->pos + Vec2(3.0f * TILE, 0);
            advance(2);
            Entity* end = g_sim.get(p.sub);
            check(end && g_sim.ghosts[0].sighting.until <= g_sim.time && !g_sim.visibleTo(*end, 1),
                  "a Ghost2 beside the pair cannot re-sight it during the cloak");
        }
    }

    // 7. the joint strike: two warheads, no earlier than 120 s after the pair appeared, then a 900 s cooldown
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the strike check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            const Vec2 at = g_sim.players[1].basePos;
            const float ready = g_sim.ghosts[0].strikeReadyAt;
            g_sim.time = ready - 1.0f;
            const size_t n0 = g_sim.nukes.size();
            check(!g_sim.cmdGhostStrike(0, at), "a strike is refused before GHOST_FIRST_STRIKE");
            check(g_sim.nukes.size() == n0, "a refused strike adds no warheads");
            g_sim.time = ready;
            check(g_sim.cmdGhostStrike(0, at), "a strike is accepted once the pair has waited 120 s");
            check(g_sim.nukes.size() == n0 + 2, "a strike adds exactly two warheads");
            if (g_sim.nukes.size() >= n0 + 2) {
                const Sim::Nuke& a = g_sim.nukes[n0];
                const Sim::Nuke& b = g_sim.nukes[n0 + 1];
                check(a.scale == SUB_NUKE_SCALE * 4 && a.dmgMul == 2 && a.sub && a.deep,
                      "the submarine warhead is four sub scales, double damage, sub and deep");
                check(b.scale == NUKEBOMBER_SCALE * 9 && b.dmgMul == 5 && !b.sub && !b.deep,
                      "the jet warhead is nine bomber scales, five times damage");
                check(std::fabs(Sim::nukeDamageFactor(a) - SUB_NUKE_DAMAGE * 2.0f) < 1e-4f
                      && std::fabs(Sim::nukeDamageFactor(b) - 5.0f) < 1e-4f,
                      "the two warheads' damage factors are SUB_NUKE_DAMAGE*2 and 5");
            }
            check(!g_sim.cmdGhostStrike(0, at), "a second strike is refused during the 900 s cooldown");
            g_sim.time = g_sim.ghosts[0].strikeReadyAt;
            check(g_sim.cmdGhostStrike(0, at), "a strike is accepted after GHOST_STRIKE_COOLDOWN");
            Sim::Nuke plain{}, subN{}, deepN{}, superN{};
            subN.sub = true; deepN.deep = true; superN.super = true;
            auto oldFactor = [](const Sim::Nuke& n) {
                return n.super ? ISLAND_NUKE_DAMAGE : (n.sub ? SUB_NUKE_DAMAGE : (n.deep ? UNUKE_DAMAGE : 1.0f));
            };
            check(std::fabs(Sim::nukeDamageFactor(plain) - oldFactor(plain)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(subN) - oldFactor(subN)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(deepN) - oldFactor(deepN)) < 1e-4f
                  && std::fabs(Sim::nukeDamageFactor(superN) - oldFactor(superN)) < 1e-4f,
                  "nukeDamageFactor still matches the old formula for plain, sub, deep and super warheads");
        }
    }

    // 8. the pair lives and dies together, and a new pair can then be trained
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the death check");
        const int subType = ghostSubFor(F_CYBER);
        Entity* s = g_sim.get(p.sub);
        if (s) g_sim.destroy(*s, true);
        advance(3);
        check(!g_sim.get(p.jet), "destroying the sub takes the jet with it");
        check(!g_sim.ghosts[0].sub.valid() && !g_sim.ghosts[0].jet.valid(), "the lost pair's record is cleared");
        check(!g_sim.ghostsInProduction(0), "no Ghosts are left in production");
        check(g_sim.ghostTrainBlockReason(0) == nullptr && g_sim.cmdTrain(p.airfield, subType),
              "a new pair may be trained after the loss");

        Pair q = makePair(seed);
        check(q.ok, "the pair exists for the jet-first death check");
        Entity* j = g_sim.get(q.jet);
        if (j) g_sim.destroy(*j, true);
        advance(3);
        check(!g_sim.get(q.sub), "destroying the jet takes the sub with it");
        check(!g_sim.ghosts[0].sub.valid() && !g_sim.ghosts[0].jet.valid(), "the jet-first loss also clears the pair");
        check(!g_sim.ghostsInProduction(0), "no Ghosts remain after the jet-first loss");
    }

    // 9. a sighting that ends with no sea 20 tiles away leaves the pair where it is, uncloaked
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the short-hop check");
        Entity* s = g_sim.get(p.sub);
        if (p.ok && s) {
            std::vector<u8> tiles0(g_map.tiles, g_map.tiles + MAP_W * MAP_H);
            std::vector<u8> blocked0(g_map.blocked, g_map.blocked + MAP_W * MAP_H);
            const Vec2 at = s->pos;
            for (int y = 1; y < MAP_H - 1; ++y) for (int x = 1; x < MAP_W - 1; ++x) {
                if (!g_map.openSea(x, y)) continue;
                if (dist(tileCenter(x, y), at) < 20.0f * TILE) continue;
                g_map.tiles[y * MAP_W + x] = T_ROCK; g_map.blocked[y * MAP_W + x] |= 1;
            }
            g_map.touch();
            g_sim.ghosts[0].sighting = {1, g_sim.time, at};
            advance(2);
            Entity* after = g_sim.get(p.sub);
            check(after && dist(after->pos, at) < 1.0f, "with no sea 20 tiles away the pair stays put");
            check(g_sim.ghosts[0].cloakUntil <= g_sim.time, "and is not cloaked");
            std::memcpy(g_map.tiles, tiles0.data(), sizeof(u8) * MAP_W * MAP_H);
            std::memcpy(g_map.blocked, blocked0.data(), sizeof(u8) * MAP_W * MAP_H);
            g_map.touch();
        }
    }

    // 10. a Ghost2 hunts on its own when its hold ends, but a player's order wins until it arrives and the hold ends
    {
        setup(seed);
        const Vec2 water = g_map.nearestWater(tileCenter(MAP_W / 2, MAP_H / 2), 200, true);
        const Ref g2 = g_sim.spawnUnit(ghost2For(F_CLANKER), 1, water);
        Entity* d = g_sim.get(g2);
        check(d != nullptr, "a Ghost2 was spawned for the hunt check");
        if (d) {
            g_sim.time = d->autoHoldUntil + 0.1f;
            advance(2);
            d = g_sim.get(g2);
            check(d && d->order != O_IDLE && isWater(d->target), "an idle Ghost2 past its hold picks a patrol on its own");
            const Vec2 dest = d ? g_map.nearestWater(d->pos, 8) : water;   // (the drone's own water: it arrives within the hold)
            g_sim.cmdMove({g2}, dest, false);
            d = g_sim.get(g2);
            check(d && d->order == O_MOVE && dist(d->target, dest) < 2.0f * TILE, "a player's move order is taken");
            advance(SIM_HZ);
            d = g_sim.get(g2);
            check(d && g_sim.time < d->autoHoldUntil && (d->order == O_IDLE || dist(d->target, dest) < 3.0f * TILE),
                  "the player's order is not overridden while its hold lasts");
            if (d) g_sim.time = d->autoHoldUntil + 0.1f;
            advance(2);
            d = g_sim.get(g2);
            check(d && d->order != O_IDLE && dist(d->target, dest) > 5.0f * TILE,
                  "once the hold ends the Ghost2 hunts again on its own");
        }
    }

    // 11. the Ghosts alone do not keep an army in the match: checkVictory and the surrender rule both discount the pair
    {
        Pair p = makePair(seed);
        check(p.ok, "the pair exists for the victory check");
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && !(e.isUnit() && ghostType(e.type))) g_sim.destroy(e, false);
        check(g_sim.get(p.sub) && g_sim.get(p.jet), "nothing of the army is left but the Ghost pair");
        check(g_sim.armyBeaten(0), "the surrender rule does not count the Ghost pair as an army");
        advance(25);
        check(!g_sim.players[0].alive, "an army left with only its Ghost pair is beaten");
        check(!g_sim.get(p.sub) && !g_sim.get(p.jet), "the orphaned pair goes with the defeated army");
        check(g_sim.gameOver && g_sim.winnerTeam == g_sim.players[1].team, "the match ends with the other army the winner");
    }

    ok &= hellTest(seed);   // the Bunker from Hell's rules live here too (piece B)
    ok &= nostopmeTest(seed);
    ok &= launcherTest(seed);
    return ok;
}
