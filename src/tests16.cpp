// One Hour - headless tests for the ocean update (--oceantest)
//  * the map: the old map is now the main island in a sea, with a second island off its east coast and islets round it; the coast is beach and
//    cliffs, every base and its piles are on dry land and reachable, the second island's piles are not (it takes a ship or a lifter)
//  * the navy: a Naval Yard goes down on the ocean coast by a base; its ships sail round the island and never cross onto the lake
//  * the cargo ship: a column boards it from the beach, it sails to the second island and lands them there; if it sinks, everyone aboard is lost
//  * the aircraft carrier: its jets launch on their own at enemies near it, hit them, come back and land; a lost jet is replaced on deck
#include "sim.h"
#include "map.h"
#include "testutil.h"

static bool g_ok16 = true;
static bool fail16(const char* m) { fprintf(stderr, "oceantest: %s\n", m); g_ok16 = false; return false; }
#define CHECK16(c, m) do { if (!(c)) return fail16(m); } while (0)

static float rng16(int k) { return std::sin(k * 12.9898f + 1.7f); }   // a fixed scatter in -1..1 for placing a column (no Sim::rng: it would shift the scene)
static void run16(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n && !g_sim.gameOver; t++) { g_sim.step(); g_sim.events.clear(); } }

static void init16(u64 seed, Faction f0) {
    Faction fac[2] = { f0, (Faction)(1 - f0) };
    bool ai[2] = { false, false };
    int diff[2] = { 1, 1 };
    int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    g_sim.riotsOn = false;
    for (int p = 0; p < 2; p++) { g_sim.players[p].money = 200000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1); }
}

// connected free ground from a start tile (4-connected over passable tiles)
static std::vector<u8> floodFrom16(Vec2 start) {
    std::vector<u8> seen((size_t)MAP_W * MAP_H, 0);
    std::vector<int> q;
    int s = tileOf(start.y) * MAP_W + tileOf(start.x);
    seen[s] = 1; q.push_back(s);
    for (size_t h = 0; h < q.size(); h++) {
        int at = q[h], x = at % MAP_W, y = at / MAP_W;
        static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
        for (int k = 0; k < 4; k++) {
            int nx = x + DX[k], ny = y + DY[k];
            if (!g_map.passable(nx, ny) || seen[ny * MAP_W + nx]) continue;
            seen[ny * MAP_W + nx] = 1; q.push_back(ny * MAP_W + nx);
        }
    }
    return seen;
}
static bool besideReached16(const std::vector<u8>& seen, int tx, int ty) {
    for (int y = ty - 1; y <= ty + 1; y++) for (int x = tx - 1; x <= tx + 1; x++) if (inMap(x, y) && seen[y * MAP_W + x]) return true;
    return false;
}

static bool mapTest16() {
    g_map.generate();
    CHECK16(g_map.oceanComp != 0 && g_map.lakeComp != 0 && g_map.oceanComp != g_map.lakeComp, "the lake and the ocean are not two separate seas");
    CHECK16(g_map.waterSize[g_map.oceanComp] > 4 * g_map.waterSize[g_map.lakeComp], "the ocean is not much bigger than the lake");
    // the map's border is all sea
    for (int i = 0; i < MAP_W; i++) {
        CHECK16(g_map.tile(i, 0) == T_WATER && g_map.tile(i, MAP_H - 1) == T_WATER && g_map.tile(0, i) == T_WATER && g_map.tile(MAP_W - 1, i) == T_WATER, "the map's edge is not open sea");
    }
    // the main island's coast: beach and cliffs, nothing grassy straight into the sea
    int coast = 0, beach = 0, cliff = 0;
    for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++) {
        int k = y * MAP_W + x;
        if (g_map.tiles[k] == T_WATER) continue;
        bool sea = false;
        for (int j = -1; j <= 1 && !sea; j++) for (int i = -1; i <= 1; i++) if (g_map.waterComp[(y + j) * MAP_W + x + i] == g_map.oceanComp) { sea = true; break; }
        if (!sea || mainIslandInland(x, y) < 0.0f) continue;
        coast++;
        if (g_map.tiles[k] == T_SAND) beach++; else if (g_map.tiles[k] == T_ROCK) cliff++;
    }
    CHECK16(coast > 400, "the main island has hardly any coast");
    CHECK16(beach + cliff > coast * 8 / 10, "the main island's coast is not beach and cliffs");
    CHECK16(cliff > coast / 12 && beach > coast / 3, "the coast lacks either beaches or cliffs");
    // the second island: far bigger than the lake's islands, much smaller than the main island, cut off by sea
    CHECK16(!g_map.seaIslands.empty(), "no second island");
    int isle = 0, main = 0;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (g_map.tile(x, y) == T_WATER) continue;
        if (seaIslandInland(x, y) > 0) isle++;
        if (mainIslandInland(x, y) > 0) main++;
    }
    const auto& big = g_map.islands[0];
    float lakeIsle = 3.1416f * big.r * big.r;
    CHECK16(isle > lakeIsle * 4.0f, "the second island is not much bigger than the lake's islands");
    CHECK16(isle * 8 < main, "the second island is not much smaller than the main island");
    int islets = 0;
    for (size_t k = 1; k < g_map.seaIslands.size(); k++) islets++;
    CHECK16(islets >= 12, "too few islets in the sea");
    // the bases and their piles stand on dry land; the main island's piles are all reachable from a base, the second island's from none
    std::vector<u8> reach = floodFrom16(tileCenter(g_map.starts[0].tx, g_map.starts[0].ty + 6));
    for (auto& s : g_map.starts) {
        CHECK16(mainIslandInland(s.tx, s.ty) > 12.0f, "a base sits too close to the sea");
        CHECK16(besideReached16(reach, s.tx, s.ty + 6) || besideReached16(reach, s.tx + 6, s.ty), "the bases cannot reach each other over land");
    }
    int onIsle = 0;
    for (auto& sp : g_map.supplies) {
        bool island = seaIslandInland(sp.tx, sp.ty) > 0;
        if (island) { onIsle++; CHECK16(!besideReached16(reach, sp.tx, sp.ty), "a supply pile on the second island can be driven to"); }
        else CHECK16(besideReached16(reach, sp.tx, sp.ty), "a main island supply pile cannot be reached");
    }
    CHECK16(onIsle == 3, "the second island does not hold its three supply piles");
    int seaForts = 0;
    for (auto& L : g_map.landmarks) if (seaIslandInland(L.tx, L.ty) > 0) seaForts++;
    CHECK16(seaForts >= 2, "the second island has no old strongholds");
    printf("oceantest map: ok (%d ocean / %d lake water tiles, coast %d tiles (%d beach, %d cliff), second island %d tiles (main %d), %d islets, %d piles out there)\n",
           g_map.waterSize[g_map.oceanComp], g_map.waterSize[g_map.lakeComp], coast, beach, cliff, isle, main, islets, onIsle);
    return true;
}

static bool navyTest16(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi;
        init16(seed + fi, me);
        Player& pl = g_sim.players[0];
        int yardT = firstBuildOf(me) == B_C_HQ ? B_C_NAVAL : B_K_NAVAL;
        CHECK16(BUILDS[yardT].w * BUILDS[yardT].h >= 20, "the Naval Yard did not grow for the sea");
        int tx = -1, ty = -1;
        CHECK16(g_sim.shoreSite(0, yardT, pl.basePos, tx, ty), "no Naval Yard site on the coast near the base");
        Vec2 yc = g_sim.buildingCenter(yardT, tx, ty);
        CHECK16(dist(yc, pl.basePos) < 40.0f * TILE, "the nearest Naval Yard site is far from the base");
        Ref yard = g_sim.placeBuilding(yardT, 0, tx, ty, true);
        CHECK16(g_sim.get(yard), "could not place the Naval Yard");
        Vec2 exitW = g_sim.navalExit(*g_sim.get(yard));
        CHECK16(g_map.compAt(exitW) == g_map.oceanComp, "the coastal yard launches its ships onto the lake");
        // a destroyer sails right round the island to the far side, and refuses to sail onto the lake
        int destT = me == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER;
        Ref d = g_sim.spawnUnit(destT, 0, exitW);
        Vec2 far = g_map.nearestWater(g_sim.players[1].basePos, 60, true, g_map.oceanComp);
        CHECK16(g_map.compAt(far) == g_map.oceanComp, "no ocean by the far base");
        std::vector<Vec2> path;
        CHECK16(g_map.findNavalPath(g_sim.get(d)->pos, far, path), "no sea route round the island");
        g_sim.cmdMove({d}, Vec2(WORLD_W * 0.5f + 6 * TILE, WORLD_H * 0.5f), false);   // the middle of the lake
        Entity* de = g_sim.get(d);
        CHECK16(g_map.compAt(de->target) == g_map.oceanComp, "a ship on the ocean was sent onto the lake");
        // the cargo ship: a column of tanks, a dozer and infantry boards it on the beach and it lands them on the second island
        int cargoT = me == F_CYBER ? U_C_CARGOSHIP : U_K_CARGOSHIP;
        CHECK16(UNITS[cargoT].cargoCap >= 100 && UNITS[cargoT].naval && UNITS[cargoT].builtBy == BR_NAVAL, "the cargo ship is not a big naval transport");
        Ref yr = yard;
        CHECK16(g_sim.cmdTrain(yr, cargoT), "the yard would not build a cargo ship");
        run16(UNITS[cargoT].buildTime * 2.2f + 8.0f);   // (no power plants in this scene: the yard builds at the low-power rate)
        Ref ship = NOREF;
        for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == cargoT && e.owner == 0) ship = g_sim.refOf(e);
        CHECK16(ship.valid(), "the cargo ship was never launched");
        Entity* sh = g_sim.get(ship);
        CHECK16(g_map.compAt(sh->pos) == g_map.oceanComp, "the cargo ship is not on the ocean");
        // bring it beside the beach nearest it and load a column there
        Vec2 landing, beach = g_map.nearestFree(sh->pos, 24);
        g_sim.cmdMove({ship}, g_map.nearestWater(beach, 6, true, g_map.oceanComp), false);
        run16(15);
        sh = g_sim.get(ship);
        CHECK16(g_sim.shipLanding(*sh, &landing), "the cargo ship brought to a beach has no land within reach");
        int ub = firstUnitOf(me);
        std::vector<Ref> column;
        for (int k = 0; k < 4; k++) column.push_back(g_sim.spawnUnit(ub + 5, 0, g_map.nearestFree(landing + Vec2(rng16(k) * 40.0f, rng16(k + 9) * 40.0f), 8)));
        column.push_back(g_sim.spawnUnit(ub, 0, g_map.nearestFree(landing, 8)));   // a dozer
        for (int k = 0; k < 8; k++) column.push_back(g_sim.spawnUnit(ub + 2, 0, g_map.nearestFree(landing + Vec2(rng16(k + 3) * 50.0f, rng16(k + 5) * 50.0f), 8)));
        g_sim.cmdEnter(column, ship);
        for (int t = 0; t < 20 * 40 && g_sim.get(ship)->passengers.size() < column.size(); t++) { g_sim.step(); g_sim.events.clear(); }
        sh = g_sim.get(ship);
        CHECK16(sh->passengers.size() == column.size(), "the column did not all board the cargo ship from the beach");
        // to the second island: it sails there and lands them on its beach
        Vec2 isle = Vec2(SEA_ISLE_X * TILE, WORLD_H * 0.5f);
        g_sim.cmdUnload(ship, true, isle);
        for (int t = 0; t < 20 * 180 && !g_sim.get(ship)->passengers.empty(); t++) { g_sim.step(); g_sim.events.clear(); }
        sh = g_sim.get(ship);
        CHECK16(sh && sh->passengers.empty(), "the cargo ship never landed its load");
        int ashore = 0;
        for (Ref r : column) if (Entity* u = g_sim.get(r)) if (!u->carrier.valid() && seaIslandInland(tileOf(u->pos.x), tileOf(u->pos.y)) > 0 && g_map.passable(tileOf(u->pos.x), tileOf(u->pos.y))) ashore++;
        CHECK16(ashore == (int)column.size(), "not everyone landed on the second island");
        // a loaded cargo ship that sinks takes everyone aboard down with it
        std::vector<Ref> few;
        for (int k = 0; k < 3; k++) few.push_back(column[5 + k]);
        for (Ref r : few) { Entity* u = g_sim.get(r); g_sim.boardPublic(*u, *g_sim.get(ship)); }
        int lostBefore = pl.unitsLost;
        g_sim.applyDamagePublic(*g_sim.get(ship), 1e7f, 1, NOREF);
        run16(0.5f);
        int alive = 0; for (Ref r : few) if (g_sim.get(r)) alive++;
        CHECK16(alive == 0 && pl.unitsLost >= lostBefore + 4, "people aboard a sunk cargo ship survived");
        printf("oceantest navy %s: ok (yard %d tiles from the base, a route round the island of %d legs, %d units shipped to the second island)\n", FACTION_NAME[me], (int)(dist(yc, pl.basePos) / TILE), (int)path.size(), (int)column.size());
    }
    return true;
}

static bool carrierTest16(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        init16(seed + 7 + fi, me);
        int carT = me == F_CYBER ? U_C_CARRIER : U_K_CARRIER, jetT = me == F_CYBER ? U_C_CARRIERJET : U_K_CARRIERJET;
        CHECK16(UNITS[carT].requires_ == firstBuildOf(me) + BR_TECH, "the carrier does not need the tech structure");
        CHECK16(UNITS[jetT].temp == 1 && UNITS[jetT].builtBy == BR_NONE, "a carrier jet can be built or commanded");
        // out at sea west of the main island, with an enemy warship and a gunship coming at it
        Vec2 sea = g_map.nearestWater(Vec2((ISLE_X0 - 26) * TILE, WORLD_H * 0.5f), 10, true, g_map.oceanComp);
        Ref car = g_sim.spawnUnit(carT, 0, sea);
        CHECK16(g_sim.get(car) && g_sim.get(car)->deck.size() == CARRIER_WING, "a new carrier does not have its whole wing on deck");
        Ref foeShip = g_sim.spawnUnit(foe == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, 1, g_map.nearestWater(sea + Vec2(0, 14 * TILE), 10, true, g_map.oceanComp));
        Ref foeHeli = g_sim.spawnUnit(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, sea + Vec2(12 * TILE, -8 * TILE));
        for (Ref r : { foeShip, foeHeli }) { Entity* e = g_sim.get(r); e->hp = e->maxHp = e->maxHp * 1.0f; e->disabledUntil = 1e9f; }
        int maxUp = 0; bool landed = false;
        for (int t = 0; t < 20 * 60; t++) {
            g_sim.step(); g_sim.events.clear();
            int up = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) up++;
            maxUp = std::max(maxUp, up);
            if (maxUp >= 2 && up < maxUp && g_sim.get(car) && g_sim.get(car)->deck.size() + up == CARRIER_WING) landed = true;
        }
        CHECK16(maxUp >= 3, "the carrier did not launch its jets at enemies near it");
        CHECK16(!g_sim.get(foeShip) && !g_sim.get(foeHeli), "the carrier's jets did not destroy the enemy warship and gunship");
        CHECK16(landed, "no jet came back to land on the carrier");
        run16(30);
        int up = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) up++;
        Entity* c = g_sim.get(car);
        CHECK16(c && up == 0 && (int)c->deck.size() == CARRIER_WING, "the wing did not all land back on deck once the fight was over");
        // a lost jet is replaced on deck after a while
        c->deck.pop_back();
        run16(CARRIER_REPLACE + 2.0f);
        CHECK16((int)g_sim.get(car)->deck.size() == CARRIER_WING, "a lost jet was never replaced");
        // nobody commands the jets
        Ref foe2 = g_sim.spawnUnit(foe == F_CYBER ? U_C_HELI : U_K_AIR, 1, sea + Vec2(-10 * TILE, 6 * TILE));
        g_sim.get(foe2)->disabledUntil = 1e9f;
        run16(4);
        std::vector<Ref> jets; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) jets.push_back(g_sim.refOf(e));
        CHECK16(!jets.empty(), "no jet went for the second gunship");
        g_sim.cmdMove(jets, g_sim.players[0].basePos, false); g_sim.cmdStop(jets);
        for (Ref r : jets) CHECK16(g_sim.get(r)->order == O_CARRIERJET, "a carrier jet obeyed an order");
        // the carrier sinks: its jets in the air have nowhere to land and fly off the map
        g_sim.applyDamagePublic(*g_sim.get(car), 1e8f, 1, NOREF);
        run16(25);
        up = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) up++;
        CHECK16(up == 0, "the jets of a sunk carrier stayed on the map");
        printf("oceantest carrier %s: ok (%d jets in the air at once, they sank the warship and downed the gunship, landed, a lost jet was replaced)\n", FACTION_NAME[me], maxUp);
    }
    return true;
}

bool oceanTest(u64 seed) {
    g_ok16 = true;
    bool ok = mapTest16() && navyTest16(seed) && carrierTest16(seed);
    if (ok) printf("oceantest: ok\n");
    return ok && g_ok16;
}
