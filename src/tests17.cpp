// One Hour - headless tests for the Fleet Call (--fleettest)
//  * the fleet: ten patrol boats, three destroyers, three cruisers and a carrier (never a cargo ship), the same at any stage of the match
//  * it is called on the ocean only (never inland or on the lake), one per tech structure per paradrop cooldown, more structures more fleets
//  * nothing appears at once or in the middle of the sea: every ship comes out of the haze within a few tiles of the map border, fading in (and
//    cannot be targeted while it is mostly haze), then sails in and forms up round the called spot; there it is an ordinary ship that takes orders
//  * computer armies call it too
#include "sim.h"
#include "map.h"
#include "ai.h"

static bool g_ok17 = true;
static bool fail17(const char* m) { fprintf(stderr, "fleettest: %s\n", m); g_ok17 = false; return false; }
#define CHECK17(c, m) do { if (!(c)) return fail17(m); } while (0)

static void init17(u64 seed, Faction f0, bool ai1) {
    Faction fac[2] = { f0, (Faction)(1 - f0) };
    bool ai[2] = { false, ai1 };
    int diff[2] = { 1, 2 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
}
static void run17(float sec, bool ai = false) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n && !g_sim.gameOver; t++) { g_sim.step(); if (ai) g_ai.update(); g_sim.events.clear(); } }

// a finished tech structure of the player's army on free ground near its base
static Ref techNear17(int p, int ring0) {
    int type = firstBuildOf(g_sim.players[p].faction) + BR_TECH;
    Vec2 b = g_sim.players[p].basePos;
    int bx = tileOf(b.x), by = tileOf(b.y);
    for (int r = ring0; r < 30; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(p, type, bx + dx, by + dy)) return g_sim.placeBuilding(type, p, bx + dx, by + dy, true);
    }
    return NOREF;
}
static int countShips17(int p, int* byType = nullptr) {
    int n = 0;
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == p && e.ut().naval) { n++; if (byType) byType[e.type]++; }
    return n;
}
static float borderDist17(Vec2 q) { return std::min(std::min(q.x, WORLD_W - q.x), std::min(q.y, WORLD_H - q.y)); }

static bool fleetTest17(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi;
        init17(seed + fi, me, false);
        Player& pl = g_sim.players[0];
        // the manifest: always the same, never a cargo ship
        int counts[U_COUNT] = {};
        for (int t : g_sim.fleetManifest(0)) counts[t]++;
        const bool cy = me == F_CYBER;
        CHECK17(counts[cy ? U_C_PATROL : U_K_PATROL] == FLEET_PATROLS && FLEET_PATROLS == 10, "the fleet does not bring ten patrol boats");
        CHECK17(counts[cy ? U_C_DESTROYER : U_K_DESTROYER] == 3 && counts[cy ? U_C_CRUISER : U_K_CRUISER] == 3, "the fleet does not bring three of each warship");
        CHECK17(counts[cy ? U_C_CARRIER : U_K_CARRIER] == 1, "the fleet does not bring one carrier");
        CHECK17(counts[U_C_CARGOSHIP] + counts[U_K_CARGOSHIP] == 0, "a cargo ship came with the fleet");
        CHECK17(g_sim.fleetManifest(0).size() == 17, "the fleet is not seventeen ships");
        // no tech structure, or too early: no fleet
        Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 30) * TILE, WORLD_H * 0.45f), 10, true, g_map.oceanComp);
        CHECK17(!g_sim.cmdFleetCall(0, sea) && g_sim.fleetsReady(0) == 0, "a fleet came without a tech structure");
        Ref tech = techNear17(0, 6);
        CHECK17(g_sim.get(tech), "no room for a tech structure");
        CHECK17(g_sim.fleetsReady(0) == 0 && !g_sim.cmdFleetCall(0, sea), "a fleet came before the paradrops' opening delay");
        pl.dropReady = 0;
        CHECK17(g_sim.fleetsReady(0) == 1, "a tech structure does not offer one fleet");
        // only on the ocean: not inland, not on the lake
        CHECK17(!g_sim.cmdFleetCall(0, pl.basePos), "a fleet was called onto dry land");
        Vec2 lake = g_map.nearestWater(Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f), 30, true, g_map.lakeComp);
        CHECK17(g_map.compAt(lake) == g_map.lakeComp, "no lake water found for the test");
        CHECK17(!g_sim.cmdFleetCall(0, lake), "a fleet was called onto the lake");
        CHECK17(g_sim.fleetsReady(0) == 1, "a refused call used up the tech structure");
        // the call on the open sea
        Vec2 goal, entry, n;
        CHECK17(g_sim.fleetEntry(sea, goal, entry, n) && borderDist17(entry) < 3.0f * TILE, "the fleet's entry is not on the map border");
        CHECK17(g_sim.cmdFleetCall(0, sea), "the fleet call on the ocean was refused");
        CHECK17(!g_sim.cmdFleetCall(0, sea) && g_sim.fleetsReady(0) == 0, "one tech structure called two fleets at once");
        CHECK17(std::abs(g_sim.fleetWait(0) - DROP_TIERS[g_sim.dropTier(0)].cooldown) < 0.5f, "the fleet's cooldown is not the paradrop's");
        CHECK17(countShips17(0) == 0, "ships appeared the moment the fleet was called");
        // they come out of the haze at the border, one after another
        std::vector<u32> seen(1, 0); int appeared = 0; float firstFade = 1; bool farSpawn = false, gradual = false;
        Ref firstRef = NOREF;
        for (int t = 0; t < 12 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            if (seen.size() < g_sim.ents.size()) seen.resize(g_sim.ents.size(), 0);
            int now = 0;
            for (size_t i = 0; i < g_sim.ents.size(); i++) {
                Entity& e = g_sim.ents[i];
                if (!e.alive || !e.isUnit() || e.owner != 0 || !e.ut().naval) continue;
                now++;
                if (seen[i] == e.gen) continue;
                seen[i] = e.gen; appeared++;
                if (borderDist17(e.pos) > 4.0f * TILE) farSpawn = true;
                firstFade = std::min(firstFade, e.fade);
                if (!firstRef.valid()) firstRef = g_sim.refOf(e);
            }
            if (now > 0 && now < 17) gradual = true;
        }
        CHECK17(appeared == 17 && countShips17(0) == 17, "the whole fleet did not come in");
        CHECK17(!farSpawn, "a ship appeared away from the map border");
        CHECK17(firstFade < 0.2f, "the ships did not come out of the haze (no fade-in)");
        CHECK17(gradual, "the fleet appeared all at once instead of ship after ship");
        // a ship still mostly haze cannot be targeted; once clear it is an ordinary target
        {
            Ref r = g_sim.spawnUnit(cy ? U_C_PATROL : U_K_PATROL, 0, sea + Vec2(0, 6 * TILE));
            Entity* s = g_sim.get(r); s->fade = 0.1f;
            Ref foe = g_sim.spawnUnit(cy ? U_K_DESTROYER : U_C_DESTROYER, 1, sea + Vec2(0, 9 * TILE));
            CHECK17(!g_sim.canTarget(*g_sim.get(foe), *g_sim.get(r)), "a ship still in the haze could be targeted");
            g_sim.get(r)->fade = 1.0f;
            CHECK17(g_sim.canTarget(*g_sim.get(foe), *g_sim.get(r)), "a ship out of the haze could not be targeted");
            g_sim.destroy(*g_sim.get(r), false); g_sim.destroy(*g_sim.get(foe), false);
        }
        run17(FLEET_FADE + 1.0f);
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().naval) CHECK17(e.fade >= 1.0f, "a ship stayed faded");
        // they sail in and form up round the called spot
        run17(75);
        int near = 0, total = 0;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().naval) { total++; if (dist(e.pos, goal) < 15.0f * TILE) near++; }
        CHECK17(total == 17 && near >= 15, "the fleet did not form up round the called spot");
        // ordinary ships: they take orders
        std::vector<Ref> all; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.owner == 0 && e.ut().naval) all.push_back(g_sim.refOf(e));
        Vec2 dest = g_map.nearestWater(goal + Vec2(0, 18 * TILE), 12, true, g_map.oceanComp);
        g_sim.cmdMove(all, dest, false);
        for (Ref r : all) CHECK17(g_sim.get(r)->order == O_MOVE, "a fleet ship ignored a move order");
        run17(40);
        int moved = 0; for (Ref r : all) if (Entity* e = g_sim.get(r)) if (dist(e->pos, dest) < 16.0f * TILE) moved++;
        CHECK17(moved >= 14, "the fleet did not sail where it was sent");
        // two tech structures: two fleets at once (after the cooldown the first one is ready again)
        for (int t = 0; t < 200 * SIM_HZ && g_sim.fleetsReady(0) == 0; t++) { g_sim.step(); g_sim.events.clear(); }
        CHECK17(g_sim.fleetsReady(0) == 1, "the fleet never came off its cooldown");
        Ref tech2 = techNear17(0, 10);
        CHECK17(g_sim.get(tech2) && g_sim.fleetsReady(0) == 2, "a second tech structure does not offer its own fleet");
        CHECK17(g_sim.cmdFleetCall(0, sea + Vec2(0, -20 * TILE)) && g_sim.cmdFleetCall(0, sea + Vec2(0, 20 * TILE)), "two tech structures did not call two fleets");
        CHECK17(!g_sim.cmdFleetCall(0, sea) && g_sim.fleetsReady(0) == 0, "two tech structures called a third fleet");
        run17(12);
        CHECK17(countShips17(0) >= 17 * 3, "the two fleets did not both come in");
        printf("fleettest %s: ok (entry %.0f px from the border, %d ships formed up, then took orders)\n", FACTION_NAME[me], borderDist17(entry), near);
    }
    // a computer army calls its fleet too
    for (int fi = 0; fi < 2; fi++) {
        init17(seed + 11 + fi, (Faction)fi, true);
        g_ai.init(seed);
        Ref tech = techNear17(1, 6);
        CHECK17(g_sim.get(tech), "no room for the computer's tech structure");
        g_sim.players[1].dropReady = 0;
        int before = countShips17(1);
        run17(30, true);
        CHECK17(countShips17(1) >= before + 15, "the computer army did not call its fleet");
        printf("fleettest AI %s: ok (%d ships came in)\n", FACTION_NAME[g_sim.players[1].faction], countShips17(1) - before);
    }
    return true;
}

// The carrier's flight deck: a jet that launches starts on the deck (not in the air), taxis to a catapult, is shot off the bow and climbs away; coming
// home it flies the glide path down onto the deck, rolls out, taxis to its spot and goes back into the deck list
static bool deckTest17(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        init17(seed + 21 + fi, me, false);
        int carT = me == F_CYBER ? U_C_CARRIER : U_K_CARRIER, jetT = me == F_CYBER ? U_C_CARRIERJET : U_K_CARRIERJET;
        Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 26) * TILE, WORLD_H * 0.55f), 10, true, g_map.oceanComp);
        Ref car = g_sim.spawnUnit(carT, 0, sea);
        Ref heli = g_sim.spawnUnit(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, sea + Vec2(10 * TILE, -10 * TILE));
        g_sim.get(heli)->disabledUntil = 1e9f;
        bool sawDeckStart = false, sawCatapult = false, sawClimb = false, sawFinal = false, sawRollout = false, startInAir = false;
        std::vector<u32> seen;
        for (int t = 0; t < 60 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            Entity* c = g_sim.get(car);
            CHECK17(c, "the carrier was lost in the deck test");
            if (seen.size() < g_sim.ents.size()) seen.resize(g_sim.ents.size(), 0);
            for (size_t i = 0; i < g_sim.ents.size(); i++) {
                Entity& j = g_sim.ents[i];
                if (!j.alive || !j.isUnit() || j.type != jetT) continue;
                if (seen[i] != j.gen) {   // a new launch: on the deck, on the ground, taxiing
                    seen[i] = j.gen;
                    if (j.alt > 0.05f || j.deckPhase != 1 || dist(j.pos, c->pos) > 70.0f) startInAir = true; else sawDeckStart = true;
                }
                if (j.deckPhase == 2) { sawCatapult = true; CHECK17(j.alt <= 0.01f, "a jet on the catapult was off the deck"); }
                if (j.deckPhase == 3) sawClimb = true;
                if (j.deckPhase == 5) sawFinal = true;
                if (j.deckPhase == 6 || j.deckPhase == 7) { sawRollout = true; CHECK17(j.alt <= 0.01f && dist(j.pos, c->pos) < 80.0f, "a landed jet was not on the deck"); }
            }
            if (sawRollout && t > 30 * SIM_HZ) break;
        }
        CHECK17(!startInAir, "a carrier jet appeared in the air instead of on the deck");
        CHECK17(sawDeckStart && sawCatapult && sawClimb, "the launch did not go deck - catapult - climb");
        CHECK17(sawFinal && sawRollout, "no jet came down the glide path onto the deck");
        run17(40);
        int up = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) up++;
        CHECK17(up == 0 && g_sim.get(car) && (int)g_sim.get(car)->deck.size() == CARRIER_WING, "the wing did not end up back on deck");
        printf("fleettest deck %s: ok (taxi, catapult, climb-out, glide path, roll-out, parked)\n", FACTION_NAME[me]);
    }
    return true;
}

bool fleetTest(u64 seed) {
    g_ok17 = true;
    bool ok = fleetTest17(seed) && deckTest17(seed);
    if (ok) printf("fleettest: ok\n");
    return ok && g_ok17;
}

// --duelmatrix: does sharp fire (Player::sharpFire) win fights? Two equal armies meet on open ground; one side picks its targets the sharp way, the
// other the plain way, then the sides swap. Several compositions, both armies, several seeds. Prints wins and the value each side has left.
#include "testutil.h"
bool duelMatrix(u64 seed) {
    struct Comp { const char* name; int off[6]; int n[6]; };
    static const Comp COMPS[] = {
        { "infantry", { 2, 3, 4, -1 }, { 10, 6, 6 } },
        { "armour",   { 5, 6, 7, 3, -1 }, { 6, 4, 3, 6 } },
        { "combined", { 2, 3, 5, 6, 8, -1 }, { 8, 5, 4, 3, 3 } },
    };
    int sharpWins = 0, plainWins = 0, draws = 0; double sharpLeft = 0, plainLeft = 0;
    for (u64 sd = 0; sd < 6; sd++) for (const Comp& c : COMPS) for (int fp = 0; fp < 4; fp++) for (int swap = 0; swap < 2; swap++) {
        Faction fa = (Faction)(fp & 1), fb = (Faction)((fp >> 1) & 1);
        Faction fac[2] = { fa, fb }; bool ai[2] = { false, false }; int diff[2] = { 1, 1 }, team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + sd * 31);
        g_sim.riotsOn = false;
        for (int p = 0; p < 2; p++) std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        const int sharpSide = swap;   // which player fires sharp this round
        g_sim.players[0].sharpFire = sharpSide == 0; g_sim.players[1].sharpFire = sharpSide == 1;
        Vec2 mid = openGroundNear(Vec2(WORLD_W * 0.5f, ISLE_X0 * TILE + 40.0f * TILE), 14);
        flattenArena(mid, 22, 16);
        std::vector<Ref> side[2];
        for (int s = 0; s < 2; s++) {
            int base = firstUnitOf(g_sim.players[s].faction), k = 0;
            for (int i = 0; c.off[i] >= 0 && i < 6; i++) for (int j = 0; j < c.n[i]; j++, k++) {
                Vec2 at = mid + Vec2((s ? 1.0f : -1.0f) * (11.0f * TILE + (k / 8) * 30.0f), ((k % 8) - 3.5f) * 34.0f);
                Ref r = g_sim.spawnUnit(base + c.off[i], s, at);
                g_sim.get(r)->angle = s ? 3.14159f : 0.0f;
                side[s].push_back(r);
            }
        }
        for (int s = 0; s < 2; s++) g_sim.cmdMove(side[s], mid + Vec2((s ? -1.0f : 1.0f) * 8.0f * TILE, 0), true);
        auto value = [&](int s) { double v = 0; for (Ref r : side[s]) if (Entity* e = g_sim.get(r)) v += UNITS[e->type].cost * e->hp / e->maxHp; return v; };
        for (int t = 0; t < 120 * SIM_HZ; t++) {
            g_sim.step(); g_sim.events.clear();
            if (t % 20 == 0 && (value(0) <= 0 || value(1) <= 0)) break;
            if (t % (20 * SIM_HZ) == 0 && t > 0) for (int s = 0; s < 2; s++) { std::vector<Ref> idle; for (Ref r : side[s]) if (Entity* e = g_sim.get(r)) if (e->order == O_IDLE) idle.push_back(r); if (!idle.empty()) g_sim.cmdMove(idle, mid + Vec2((s ? -1.0f : 1.0f) * 10.0f * TILE, 0), true); }
        }
        double vs = value(sharpSide), vp = value(1 - sharpSide);
        sharpLeft += vs; plainLeft += vp;
        if (vs > 0 && vp <= 0) sharpWins++; else if (vp > 0 && vs <= 0) plainWins++; else draws++;
    }
    printf("duelmatrix: sharp fire won %d, plain fire won %d, unresolved %d; value left: sharp %.0f, plain %.0f\n", sharpWins, plainWins, draws, sharpLeft, plainLeft);
    return true;
}
