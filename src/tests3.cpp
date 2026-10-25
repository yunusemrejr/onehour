// One Hour - headless tests of the lake and its warships, jeeps with shooting crews, protector drones, the Hotline Strike fighters and the anti-nuke
// defences (--navytest, --jeeptest, --dronetest, --strikejettest, --antinuketest)
#include "sim.h"
#include "map.h"
#include "ui.h"
#include "ai.h"
#include "testutil.h"

static bool g_ok3 = true;
static const char* g_name3 = "test";
static bool failMsg3(const char* m) { fprintf(stderr, "%s: %s\n", g_name3, m); g_ok3 = false; return false; }
#define CHECK3(cond, msg) do { if (!(cond)) return failMsg3(msg); } while (0)

static void run3(float sec) { int n = (int)(sec * SIM_HZ); for (int t = 0; t < n; t++) { g_sim.step(); g_sim.events.clear(); } }
static bool alive3(Ref r) { return g_sim.get(r) != nullptr; }
static float hp3(Ref r) { Entity* e = g_sim.get(r); return e ? e->hp : -1.0f; }

// A scene with two armies (everything explored, plenty of money, enough power) and nothing but the two Command Cores and the lake in the middle.
static void lakeScene(Faction f0, Faction f1, u64 seed) {
    Faction fac[2] = { f0, f1 };
    bool ai[2] = { false, false }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
    g_sim.init(2, fac, ai, diff, team, seed);
    for (int p = 0; p < 2; p++) {
        g_sim.players[p].money = 400000; std::fill(g_sim.players[p].explored.begin(), g_sim.players[p].explored.end(), 1);
        g_sim.players[p].dropReady = 0;
    }
    for (auto& e : g_sim.ents) if (e.alive && (e.isUnit() || (e.isBuilding() && e.owner < 0))) g_sim.destroy(e, false);
}

static Ref placeNear3(int type, int owner, Vec2 nearP) {
    int cx = tileOf(nearP.x), cy = tileOf(nearP.y);
    for (int r = 0; r < 24; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
        if (std::abs(dx) != r && std::abs(dy) != r) continue;
        if (g_sim.canPlace(owner, type, cx + dx, cy + dy)) return g_sim.placeBuilding(type, owner, cx + dx, cy + dy, true);
    }
    return NOREF;
}
static void givePower3(int owner) {
    int pt = firstBuildOf(g_sim.players[owner].faction) + BR_POWER;
    Vec2 in = (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - g_sim.players[owner].basePos).norm();   // (toward the middle of the map: the bases sit in the corners)
    for (int i = 0; i < 5; i++) placeNear3(pt, owner, g_sim.players[owner].basePos + in * ((6 + i * 2) * (float)TILE));
    g_sim.updatePowerPublic();
}
static Vec2 lakePoint(float dx, float dy) { return Vec2(WORLD_W * 0.5f + dx * TILE, WORLD_H * 0.5f + dy * TILE); }
static Ref ship3(int type, int owner, Vec2 at) { return g_sim.spawnUnit(type, owner, g_map.nearestWater(at, 12)); }
static bool onWater(const Entity& e) { return g_map.navTile(tileOf(e.pos.x), tileOf(e.pos.y)); }

// ---------------------------------------------------------------------------------------------------------------- the lake and the fleet
static bool lakeAndShips(Faction me, Faction foe, u64 seed) {
    lakeScene(me, foe, seed);
    // the map: 104x104 tiles (30% wider than the old 80x80), a big central lake with a buildable island in the middle of it,
    // one body of water for the ships, still crossable on land
    CHECK3(MAP_W >= 104 && MAP_H >= 104, "the map is not the enlarged 104x104 one");
    int biggest = 0; for (int s : g_map.waterSize) biggest = std::max(biggest, s);
    CHECK3(biggest >= 850, "the central lake is not big (850 tiles or more)");
    CHECK3(g_map.bigWater(MAP_W / 2 + 10, MAP_H / 2) && g_map.navOpen(MAP_W / 2 + 10, MAP_H / 2), "the water east of the island is not open lake");
    CHECK3(g_map.buildable(MAP_W / 2 + 2, MAP_H / 2), "the island has no buildable ground beside its watchtower");
    {   // ships can sail all the way round the island
        std::vector<Vec2> ring;
        Vec2 w = g_map.nearestWater(lakePoint(-10, 0), 6), e = g_map.nearestWater(lakePoint(10, 0), 6);
        CHECK3(g_map.findNavalPath(w, e, ring), "no naval path round the island");
    }
    {   // a land route between the bases that is not the lake (from beside one Command Core to beside the other)
        std::vector<Vec2> path;
        Vec2 s0 = g_map.nearestFree(g_sim.players[0].basePos + Vec2(0, 5 * TILE), 10), s1 = g_map.nearestFree(g_sim.players[1].basePos + Vec2(0, 5 * TILE), 10);
        CHECK3(g_map.findPath(s0, s1, path, 20000), "no land route between the bases");
        for (Vec2 q : path) CHECK3(g_map.passable(tileOf(q.x), tileOf(q.y)), "the land route crosses water");
    }
    // ships sail on open water only, along a path planned over it
    std::vector<Vec2> np;
    Vec2 a = g_map.nearestWater(lakePoint(-9, 0), 6), b = g_map.nearestWater(lakePoint(9, 1), 6);
    CHECK3(g_map.findNavalPath(a, b, np), "no naval path across the lake");
    for (Vec2 q : np) CHECK3(g_map.navTile(tileOf(q.x), tileOf(q.y)), "a naval waypoint is not water");
    // a Naval Yard must touch the open lake
    int yardT = me == F_CYBER ? B_C_NAVAL : B_K_NAVAL, facT = firstBuildOf(me) + BR_FACTORY, techT = firstBuildOf(me) + BR_TECH;
    const int YW = BUILDS[yardT].w, YH = BUILDS[yardT].h;   // (the yard grew to 5x4 for the sea)
    CHECK3(!g_sim.canPlace(0, yardT, ISLE_X0 + 8, ISLE_X0 + 8), "a Naval Yard may stand on dry land away from the lake");
    {   // a buildable footprint whose edge touches river water but not the lake: still refused (found by scanning, so this
        // holds wherever the rivers run)
        bool found = false, refused = false;
        for (int ty = 1; ty < MAP_H - 4 && !found; ty++) for (int tx = 1; tx < MAP_W - 4 && !found; tx++) {
            bool foot = true;
            for (int j = 0; j < YH && foot; j++) for (int i = 0; i < YW; i++) if (!g_map.buildable(tx + i, ty + j)) foot = false;
            if (!foot) continue;
            bool river = false, lake = false;
            for (int j = ty - 1; j <= ty + YH; j++) for (int i = tx - 1; i <= tx + YW; i++) {
                bool inside = i >= tx && i < tx + YW && j >= ty && j < ty + YH, corner = (i < tx || i >= tx + YW) && (j < ty || j >= ty + YH);
                if (inside || corner || !inMap(i, j) || g_map.tile(i, j) != T_WATER) continue;
                if (g_map.bigWater(i, j)) lake = true; else river = true;
            }
            if (river && !lake) { found = true; refused = !g_sim.canPlace(0, yardT, tx, ty); }
        }
        CHECK3(found, "no buildable riverbank footprint found to test the Naval Yard rule on");
        CHECK3(refused, "a Naval Yard may stand on a bridge-cut river");
    }
    int tx = 0, ty = 0;
    CHECK3(g_sim.shoreSite(0, yardT, g_sim.players[0].basePos, tx, ty), "no shore site for a Naval Yard");
    {   // the site touches the lake along one of its edges
        bool touch = false;
        for (int j = ty - 1; j <= ty + YH; j++) for (int i = tx - 1; i <= tx + YW; i++) { bool inside = i >= tx && i < tx + YW && j >= ty && j < ty + YH, corner = (i < tx || i >= tx + YW) && (j < ty || j >= ty + YH); if (!inside && !corner && g_map.bigWater(i, j)) touch = true; }
        CHECK3(touch, "the shore site does not touch the lake");
    }
    // it needs the Assembly Plant / War Factory first
    CHECK3(!g_sim.buildAvailable(0, yardT), "a Naval Yard is buildable without a factory");
    placeNear3(facT, 0, g_sim.players[0].basePos + Vec2(0, 6 * TILE));
    placeNear3(techT, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));
    givePower3(0);
    CHECK3(g_sim.buildAvailable(0, yardT), "a Naval Yard is not buildable with a factory");
    Ref yard = g_sim.placeBuilding(yardT, 0, tx, ty, true);
    g_sim.updatePowerPublic();
    int destroyer = me == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, cruiser = me == F_CYBER ? U_C_CRUISER : U_K_CRUISER;
    CHECK3(g_sim.unitAvailable(0, destroyer) && g_sim.unitAvailable(0, cruiser), "the ships are not available at a yard with a tech structure");
    CHECK3(g_sim.cmdTrain(yard, destroyer) && g_sim.cmdTrain(yard, cruiser), "the yard cannot train ships");
    int before = g_sim.countUnits(0, destroyer) + g_sim.countUnits(0, cruiser);
    run3(70);
    CHECK3(g_sim.countUnits(0, destroyer) + g_sim.countUnits(0, cruiser) == before + 2, "the yard did not build both ships");
    for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().naval) {
        CHECK3(onWater(e), "a ship came out of the yard onto land");
        CHECK3(g_map.compAt(e.pos) == g_map.compAt(g_sim.navalExit(*g_sim.get(yard))), "a new ship is not on its yard's sea");
        CHECK3(dist(e.pos, g_sim.get(yard)->pos) > 3.5f * TILE, "a new ship did not head out from the slipway");
    }
    // and it sails where it is sent, over water the whole way, turning like a ship
    Ref sh = ship3(destroyer, 0, lakePoint(-8, 0));
    Vec2 goal = g_map.nearestWater(lakePoint(9, 2), 6);
    g_sim.cmdMove({sh}, goal, false);
    float lastAngle = g_sim.get(sh)->angle; float maxTurn = 0; bool arrived = false;
    for (int t = 0; t < 20 * 60 && !arrived; t++) {
        g_sim.step(); g_sim.events.clear();
        Entity* e = g_sim.get(sh); CHECK3(e, "the ship vanished");
        CHECK3(onWater(*e), "the ship left the water");
        float da = std::abs(std::atan2(std::sin(e->angle - lastAngle), std::cos(e->angle - lastAngle))); maxTurn = std::max(maxTurn, da); lastAngle = e->angle;
        if (dist(e->pos, goal) < 40) arrived = true;
    }
    CHECK3(arrived, "the ship never crossed the lake");
    CHECK3(maxTurn < 1.1f * SIM_DT + 0.001f, "the ship turned faster than its turn rate");
    // a destination on land: it sails to the nearest open water
    g_sim.cmdMove({sh}, g_sim.players[0].basePos, false);
    run3(40);
    CHECK3(g_sim.get(sh) && onWater(*g_sim.get(sh)), "a ship sent to a land target left the water");
    printf("lake %s: ok (lake %d tiles, yard at %d,%d builds ships, a destroyer crosses the lake in a turn-limited sail)\n", FACTION_NAME[me], biggest, tx, ty);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------- ships fight
static bool shipsFight(Faction me, Faction foe, u64 seed) {
    lakeScene(me, foe, seed);
    int dM = me == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, dF = foe == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER;
    int cM = me == F_CYBER ? U_C_CRUISER : U_K_CRUISER;
    int tankF = firstUnitOf(foe) + 5;
    // two destroyers per side fight it out in the middle of the lake
    std::vector<Ref> mine, theirs;
    for (int i = 0; i < 2; i++) { mine.push_back(ship3(dM, 0, lakePoint(-7, -2 + i * 3))); theirs.push_back(ship3(dF, 1, lakePoint(7, -2 + i * 3))); }
    g_sim.cmdMove(mine, lakePoint(0, 0), true); g_sim.cmdMove(theirs, lakePoint(0, 0), true);
    run3(60);
    float hm = 0, ht = 0; int am = 0, at = 0;
    for (Ref r : mine) if (alive3(r)) { hm += hp3(r); am++; }
    for (Ref r : theirs) if (alive3(r)) { ht += hp3(r); at++; }
    CHECK3(am + at < 4 || hm + ht < 4 * UNITS[dM].hp * 0.95f, "two fleets met on the lake and nothing was damaged");
    // a ship dies as a ship: wreck, no corpse of a vehicle on land
    for (Ref r : mine) if (Entity* e = g_sim.get(r)) CHECK3(onWater(*e), "a fighting ship is on land");
    // cruiser rockets hit a tank on the shore, a long way off, and nothing closer than the minimum range
    lakeScene(me, foe, seed + 1);
    Vec2 shore = g_map.nearestFree(lakePoint(0, -29), 10);   // (the north shore, beyond the beach of the bigger lake)
    Ref tank = g_sim.spawnUnit(tankF, 1, shore);
    Entity* tk = g_sim.get(tank); tk->disabledUntil = 1e9f;   // it sits still and takes it
    Vec2 wpos = g_map.nearestWater(Vec2(tk->pos.x, tk->pos.y + 11.0f * TILE), 12);
    float range = dist(wpos, tk->pos) / TILE;
    CHECK3(range < WEAPONS[UNITS[cM].weapon].range, "the test scene put the cruiser out of its own range");
    Ref cr = ship3(cM, 0, wpos);
    float h0 = g_sim.get(tank)->hp;
    g_sim.cmdAttack({cr}, tank);
    run3(40);
    CHECK3(!alive3(tank) || hp3(tank) < h0 * 0.8f, "a cruiser did not hurt a tank on the shore");
    // a ground tank at the shore shoots a ship that comes close
    lakeScene(me, foe, seed + 2);
    Vec2 sh2 = g_map.nearestFree(lakePoint(0, -13.5f), 10);
    Ref gt = g_sim.spawnUnit(firstUnitOf(me) + 5, 0, sh2);
    Ref enemyShip = ship3(dF, 1, Vec2(sh2.x, sh2.y + 6 * TILE));
    float e0 = hp3(enemyShip);
    g_sim.get(enemyShip)->order = O_IDLE;
    g_sim.cmdAttack({gt}, enemyShip);
    run3(30);
    CHECK3(!alive3(enemyShip) || hp3(enemyShip) < e0, "a tank on the shore could not hurt a ship in range");
    printf("ships fight %s vs %s: ok\n", FACTION_NAME[me], FACTION_NAME[foe]);
    return true;
}

// ---------------------------------------------------------------------------------------------------------------- nukes sink ships
static bool nukesSinkShips(Faction me, Faction foe, u64 seed) {
    lakeScene(me, foe, seed);
    int dF = foe == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, cF = foe == F_CYBER ? U_C_CRUISER : U_K_CRUISER;
    Vec2 c = lakePoint(-11, -4);   // open water well clear of the island and the rocky islets
    std::vector<Ref> fleet;
    for (int i = 0; i < 3; i++) fleet.push_back(ship3(i % 2 ? cF : dF, 1, c + Vec2((i - 1) * 3.0f * TILE, 0)));
    Ref faraway = ship3(dF, 1, lakePoint(9, 7));
    Vec2 gz = g_sim.get(fleet[1])->pos;
    // a bunker's small missile has a fifth of the blast area: every ship inside its circle goes down, whatever its health
    Sim::Nuke mini{ g_sim.players[0].basePos, gz, 0, Sim::NUKE_FLIGHT - 0.05f, false, MINI_NUKE_SCALE };
    float R = Sim::nukeRadius(mini);
    int inside = 0; for (Ref r : fleet) if (dist(g_sim.get(r)->pos, gz) <= R) inside++;
    CHECK3(inside == 3, "the test fleet is not inside the small blast");
    CHECK3(dist(g_sim.get(faraway)->pos, gz) > R + 2 * TILE, "the far ship is inside the small blast");
    g_sim.nukes.push_back(mini);
    run3(0.5f);
    int sunk = 0; for (Ref r : fleet) if (!alive3(r)) sunk++;
    CHECK3(sunk == 3, "a small nuke did not sink every ship in its circle");
    CHECK3(alive3(faraway), "a small nuke sank a ship far outside its circle");
    // a full warhead sinks a ship near the rim of its circle too
    lakeScene(me, foe, seed + 1);
    Ref rim = ship3(dF, 1, lakePoint(-10, -6));
    Vec2 rp = g_sim.get(rim)->pos;
    Vec2 gz2 = g_map.nearestWater(rp + Vec2(NUKE_RADIUS * TILE * 0.85f, 0), 20);
    CHECK3(dist(gz2, rp) < NUKE_RADIUS * TILE, "the test ship is outside the full blast");
    g_sim.nukes.push_back({g_sim.players[0].basePos, gz2, 0, Sim::NUKE_FLIGHT - 0.05f, false});
    run3(0.5f);
    CHECK3(!alive3(rim), "a full warhead did not sink a ship near the rim of its circle");
    printf("nukes sink ships %s: ok (all %d ships in a bunker missile's circle sunk, the one outside left alone)\n", FACTION_NAME[foe], sunk);
    return true;
}

// The map after the lake grew: every base, every supply pile and every road is on one connected stretch of land (ships cannot cut anything off)
static bool mapConnectivity() {
    g_map.generate();
    static std::vector<int> comp; comp.assign(MAP_W * MAP_H, 0);
    int ncomp = 0; std::vector<int> size(1, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (!g_map.terrainPassable(x, y) || comp[y * MAP_W + x]) continue;
        int id = ++ncomp; size.push_back(0);
        std::vector<int> st{ y * MAP_W + x }; comp[y * MAP_W + x] = id;
        while (!st.empty()) {
            int i = st.back(); st.pop_back(); size[id]++;
            static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) { int nx = i % MAP_W + DX[k], ny = i / MAP_W + DY[k]; if (g_map.terrainPassable(nx, ny) && !comp[ny * MAP_W + nx]) { comp[ny * MAP_W + nx] = id; st.push_back(ny * MAP_W + nx); } }
        }
    }
    int main = 1; for (int i = 1; i <= ncomp; i++) if (size[i] > size[main]) main = i;
    for (auto& st : g_map.starts) CHECK3(comp[st.ty * MAP_W + st.tx] == main, "a base start is cut off from the rest of the land");
    for (auto& sp : g_map.supplies) if (mainIslandInland(sp.tx, sp.ty) > 0) CHECK3(comp[sp.ty * MAP_W + sp.tx] == main, "a supply pile is cut off from the rest of the land");   // (the second island's piles are out at sea by design)
    int island = comp[(MAP_H / 2) * MAP_W + MAP_W / 2];   // the lake's island is cut off by design (reached by airlift, not by road)
    CHECK3(island != 0 && island != main, "the island in the middle of the lake is not an island");
    std::vector<int> islandComps{ island };   // (both lake islands are cut off by design)
    for (auto& isl : g_map.islands) islandComps.push_back(comp[(int)isl.cy * MAP_W + (int)isl.cx]);
    for (int t = 0; t < MAP_W * MAP_H; t++) if (comp[t] && mainIslandInland(t % MAP_W, t / MAP_W) <= 0 && std::find(islandComps.begin(), islandComps.end(), comp[t]) == islandComps.end()) islandComps.push_back(comp[t]);   // (and the second island and the islets out at sea)
    int stray = 0; for (int i = 1; i <= ncomp; i++) if (i != main && std::find(islandComps.begin(), islandComps.end(), i) == islandComps.end()) stray += size[i];
    if (getenv("ONEHOUR_DEBUG")) fprintf(stderr, "map: main land %d tiles, island %d, %d tiles in small pockets\n", size[main], size[island], stray);
    if (getenv("ONEHOUR_DEBUG")) for (int i = 1; i <= ncomp; i++) if (i != main && std::find(islandComps.begin(), islandComps.end(), i) == islandComps.end())
        for (int t = 0; t < MAP_W * MAP_H; t++) if (comp[t] == i) {
            fprintf(stderr, "  pocket of %d tiles at %d,%d\n", size[i], t % MAP_W, t / MAP_W);
            if (size[i] >= 8) for (int y = t / MAP_W - 5; y <= t / MAP_W + 7; y++) {   // (what walls it in: ~ water, # rock, T tree, o the pocket, . open land)
                fprintf(stderr, "    ");
                for (int x = t % MAP_W - 8; x <= t % MAP_W + 10; x++) { if (!inMap(x, y)) { fputc(' ', stderr); continue; } u8 c = g_map.tile(x, y); fputc(comp[y * MAP_W + x] == i ? 'o' : c == T_WATER ? '~' : c == T_ROCK ? '#' : c == T_TREE ? 'T' : '.', stderr); }
                fputc('\n', stderr);
            }
            break;
        }
    CHECK3(stray <= 40, "the lake and rivers cut off pockets of land");
    return true;
}

bool navyTest(u64 seed) {
    g_name3 = "navytest"; g_ok3 = true;
    if (!mapConnectivity()) return false;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        if (!lakeAndShips(me, foe, seed + fi)) return false;
        if (!shipsFight(me, foe, seed + 3 + fi)) return false;
        if (!nukesSinkShips(me, foe, seed + 7 + fi)) return false;
    }
    printf("navytest: ok\n");
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- jeeps
static Ref jeepWithCrew(int owner, Vec2 at) {
    Faction f = g_sim.players[owner].faction;
    Ref jp = g_sim.spawnUnit(f == F_CYBER ? U_C_JEEP : U_K_JEEP, owner, at);
    for (int t : g_sim.jeepCrew(owner)) { Ref c = g_sim.spawnUnit(t, owner, at); Entity* cu = g_sim.get(c); Entity* je = g_sim.get(jp); if (cu && je) g_sim.boardQuiet(*cu, *je); }
    return jp;
}

static bool jeepScenes(Faction me, Faction foe, u64 seed) {
    lakeScene(me, foe, seed);
    flattenArena(lakePoint(0, 0), 24, 24);
    Vec2 mid = lakePoint(0, 0);
    int jeepT = me == F_CYBER ? U_C_JEEP : U_K_JEEP, rifle = firstUnitOf(me) + 2, tankF = firstUnitOf(foe) + 5, rifleF = firstUnitOf(foe) + 2;
    // the same hull, weapon and price in both armies (only the look differs)
    CHECK3(UNITS[U_C_JEEP].hp == UNITS[U_K_JEEP].hp && UNITS[U_C_JEEP].speed == UNITS[U_K_JEEP].speed && UNITS[U_C_JEEP].cost == UNITS[U_K_JEEP].cost && UNITS[U_C_JEEP].weapon == UNITS[U_K_JEEP].weapon && UNITS[U_C_JEEP].cargoCap == JEEP_SEATS && UNITS[U_K_JEEP].cargoCap == JEEP_SEATS, "the two armies' jeeps are not the same vehicle");
    // seven seats, soldiers only, and only while it stands still
    Ref jp = g_sim.spawnUnit(jeepT, 0, mid);
    std::vector<Ref> men;
    for (int i = 0; i < 9; i++) men.push_back(g_sim.spawnUnit(rifle, 0, mid + Vec2((i % 3 - 1) * 20.0f, 40.0f + (i / 3) * 18.0f)));
    Ref tank = g_sim.spawnUnit(firstUnitOf(me) + 5, 0, mid + Vec2(60, 0));
    CHECK3(!g_sim.canBoard(*g_sim.get(tank), *g_sim.get(jp)), "a tank was allowed into a jeep");
    CHECK3(g_sim.canBoard(*g_sim.get(men[0]), *g_sim.get(jp)), "a soldier cannot board an empty jeep");
    g_sim.cmdEnter(men, jp);
    run3(15);
    CHECK3((int)g_sim.get(jp)->passengers.size() == JEEP_SEATS, "a jeep did not fill its seven seats");
    int outside = 0; for (Ref r : men) if (alive3(r) && !g_sim.get(r)->carrier.valid()) outside++;
    CHECK3(outside == 2, "the two soldiers that did not fit are not standing outside");
    CHECK3(!g_sim.canBoard(*g_sim.get(men[0]), *g_sim.get(jp)) || g_sim.get(men[0])->carrier.valid(), "a full jeep took an eighth soldier");
    // the crew rides along and unloads on command
    g_sim.cmdMove({jp}, mid + Vec2(0, -200), false);
    run3(4);
    for (Ref r : g_sim.get(jp)->passengers) CHECK3(g_sim.get(r) && dist(g_sim.get(r)->pos, g_sim.get(jp)->pos) < 2.0f, "a passenger is not where the jeep is");
    g_sim.cmdUnload(jp, false, Vec2());
    run3(3);
    CHECK3(g_sim.get(jp)->passengers.empty(), "the crew did not climb out");
    for (Ref r : men) if (alive3(r)) CHECK3(!g_sim.get(r)->carrier.valid() && g_map.passable(tileOf(g_sim.get(r)->pos.x), tileOf(g_sim.get(r)->pos.y)), "a soldier climbed out onto blocked ground");
    // from inside: a rocket trooper hits a tank beyond the jeep's own gun, a sniper picks off infantry far beyond anything else aboard
    lakeScene(me, foe, seed + 1);
    flattenArena(lakePoint(0, 0), 24, 24);
    mid = lakePoint(0, 0);
    placeNear3(firstBuildOf(me) + BR_TECH, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));   // (the sniper and the thrower need the tech structure)
    Ref j2 = jeepWithCrew(0, mid);
    CHECK3((int)g_sim.get(j2)->passengers.size() == JEEP_SEATS, "the test crew is not seven");
    int rocketN = 0, sniperN = 0; for (Ref r : g_sim.get(j2)->passengers) { Entity* u = g_sim.get(r); if (u->ut().sniper) sniperN++; if (u->type == (me == F_CYBER ? U_C_INF2 : U_K_INF2)) rocketN++; }   // (the anti-armour trooper: Laser Trooper / RPG Trooper)
    Ref t2 = g_sim.spawnUnit(tankF, 1, mid + Vec2(6.35f * TILE, 0));
    g_sim.get(t2)->disabledUntil = 1e9f;
    {   // the tank sits beyond the jeep's own gun and inside the reach of the rocket / laser troopers aboard (ranges are measured to its edge)
        float edge = g_sim.distToEntity(g_sim.get(j2)->pos, *g_sim.get(t2)) / TILE, rpg = WEAPONS[UNITS[me == F_CYBER ? U_C_INF2 : U_K_INF2].weapon].range;
        CHECK3(edge > WEAPONS[W_JEEP_MG].range + 0.2f && edge < rpg, "the test geometry does not put the tank between the jeep gun's and the rocket's range");
    }
    g_sim.cmdStop({j2});
    float h0 = hp3(t2);
    run3(20);
    CHECK3(!alive3(t2) || hp3(t2) < h0 * 0.8f, "the soldiers of a jeep did not shoot a tank from inside");
    Ref inf2 = g_sim.spawnUnit(rifleF, 1, mid + Vec2(10.5f * TILE, 0));
    g_sim.get(inf2)->disabledUntil = 1e9f;
    run3(15);
    CHECK3(!alive3(inf2), "a sniper in a jeep did not shoot infantry 10 tiles away");
    CHECK3(rocketN >= 4 && sniperN == 1, "the dropped crew is not mostly rocket troopers with one sniper");
    // moving, they keep firing; blown up, some of the crew die and the rest climb out hurt
    Ref t3 = g_sim.spawnUnit(tankF, 1, mid + Vec2(30 * TILE, 0));
    g_sim.get(t3)->disabledUntil = 1e9f;
    g_sim.cmdMove({j2}, mid + Vec2(25 * TILE, 0), false);
    float h3 = hp3(t3);
    run3(14);
    CHECK3(!alive3(t3) || hp3(t3) < h3, "a moving jeep's crew did not fire");
    int crewBefore = (int)g_sim.get(j2)->passengers.size();
    Entity* je = g_sim.get(j2);
    std::vector<Ref> crew = je->passengers;
    g_sim.applyDamagePublic(*je, 1e6f, 1, NOREF);
    int dead = 0, hurt = 0, fine = 0;
    for (Ref r : crew) { Entity* u = g_sim.get(r); if (!u) dead++; else if (u->hp < u->maxHp * 0.9f) hurt++; else fine++; }
    CHECK3(crewBefore == JEEP_SEATS && dead + hurt + fine == JEEP_SEATS && hurt + fine > 0 && fine == 0, "the crew of a wrecked jeep is not thrown out hurt or killed");
    // protector drones: one for a jeep, two with the upgrade
    lakeScene(me, foe, seed + 2);
    Ref j3 = g_sim.spawnUnit(jeepT, 0, lakePoint(0, -14));
    CHECK3(g_sim.droneCount(*g_sim.get(j3)) == JEEP_DRONES, "a jeep does not fly with a mini drone");
    g_sim.players[0].upg[UPG_DRONES] = true;
    CHECK3(g_sim.droneCount(*g_sim.get(j3)) == JEEP_DRONES + 1, "the Guard Drones upgrade did not give the jeep a second drone");
    printf("jeeps %s: ok (7 seats, soldiers shoot from inside incl. a sniper at 10 tiles, casualties %d dead %d hurt when it blows up)\n", FACTION_NAME[me], dead, hurt);
    return true;
}

bool jeepTest(u64 seed) {
    g_name3 = "jeeptest"; g_ok3 = true;
    for (int fi = 0; fi < 2; fi++) if (!jeepScenes((Faction)fi, (Faction)(1 - fi), seed + fi)) return false;
    printf("jeeptest: ok\n");
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- protector drones
// A ship is attacked by a missile cruiser, by shell-firing or by laser-firing destroyers for a while: the same scene with and without the Guard
// Drones upgrade (lasers are too quick for the drones; rockets and shells are not)
enum { ATK_MISSILES = 0, ATK_LASERS, ATK_SHELLS };
static float shellingDamage(Faction foe, u64 seed, bool drones, int kind) {
    lakeScene(F_CYBER, foe, seed);
    g_sim.players[1].upg[UPG_DRONES] = drones;
    int dT = foe == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER;
    int shooterT = kind == ATK_MISSILES ? U_C_CRUISER : (kind == ATK_LASERS ? U_C_DESTROYER : U_K_DESTROYER);   // (Cyber guns are lasers, Clanker guns fire shells)
    int shooterOwner = 0;
    Ref tgt = ship3(dT, 1, lakePoint(8, 4));
    g_sim.get(tgt)->maxHp = g_sim.get(tgt)->hp = 20000.0f;   // (so that the damage done adds up instead of sinking it)
    std::vector<Ref> shooters;
    int n = kind == ATK_LASERS ? 3 : 1;   // (one ship: a screen of three drones stops its volleys; a whole fleet's massed fire would saturate it)
    for (int i = 0; i < n; i++) { Ref r = ship3(shooterT, shooterOwner, lakePoint(8.0f + i * 2.0f, -4.0f)); shooters.push_back(r); }
    g_sim.get(tgt)->order = O_IDLE;
    g_sim.cmdAttack(shooters, tgt);
    for (int t = 0; t < 20 * 14 && alive3(tgt); t++) {   // the target never moves or shoots back (it would just sail out of range)
        g_sim.step(); g_sim.events.clear();
        if (Entity* e = g_sim.get(tgt)) { e->order = O_IDLE; e->cooldown = 1e3f; }
    }
    Entity* e = g_sim.get(tgt);
    return e ? 20000.0f - e->hp : 20000.0f;
}

bool droneTest(u64 seed) {
    g_name3 = "dronetest"; g_ok3 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction foe = (Faction)fi;
        float bare = shellingDamage(foe, seed + fi, false, ATK_MISSILES), screened = shellingDamage(foe, seed + fi, true, ATK_MISSILES);
        CHECK3(bare > 800.0f, "the missile ship did not hurt a bare destroyer");
        if (!(screened < bare * 0.5f)) { fprintf(stderr, "dronetest: rockets did %.0f without the screen and %.0f with it\n", bare, screened); return failMsg3("the drones did not blunt a rocket attack"); }
        float sb = shellingDamage(foe, seed + 3 + fi, false, ATK_SHELLS), ss = shellingDamage(foe, seed + 3 + fi, true, ATK_SHELLS);
        CHECK3(sb > 400.0f, "the shell-firing destroyer did not hurt a bare destroyer");
        if (!(ss < sb * 0.6f)) { fprintf(stderr, "dronetest: shells did %.0f without the screen and %.0f with it\n", sb, ss); return failMsg3("the drones did not blunt a shell attack"); }
        float lb = shellingDamage(foe, seed + 5 + fi, false, ATK_LASERS), ls = shellingDamage(foe, seed + 5 + fi, true, ATK_LASERS);
        CHECK3(lb > 400.0f && std::abs(lb - ls) < lb * 0.06f, "the drones stopped lasers, which are too quick for them");
        printf("drones on a %s destroyer: ok (rockets %.0f -> %.0f, shells %.0f -> %.0f, lasers %.0f -> %.0f)\n", FACTION_NAME[foe], bare, screened, sb, ss, lb, ls);
    }
    // count, EMP
    lakeScene(F_CYBER, F_CLANKER, seed + 9);
    Ref sh = ship3(U_C_DESTROYER, 0, lakePoint(0, 0));
    CHECK3(g_sim.droneCount(*g_sim.get(sh)) == 0, "a ship has drones without the upgrade");
    g_sim.players[0].upg[UPG_DRONES] = true;
    CHECK3(g_sim.droneCount(*g_sim.get(sh)) == SHIP_DRONES, "the upgrade did not give a ship three drones");
    g_sim.get(sh)->disabledUntil = g_sim.time + 5;
    CHECK3(g_sim.droneCount(*g_sim.get(sh)) == 0, "a switched-off ship still has its drones");
    printf("dronetest: ok\n");
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- Air Strike fighters
bool strikeJetTest(u64 seed) {
    g_name3 = "strikejettest"; g_ok3 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        lakeScene(me, foe, seed + fi);
        int techT = firstBuildOf(me) + BR_TECH, jetT = me == F_CYBER ? U_C_STRIKEJET : U_K_STRIKEJET, airfT = firstBuildOf(me) + BR_AIRFIELD;
        placeNear3(techT, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));
        Ref af = placeNear3(airfT, 0, g_sim.players[0].basePos + Vec2(-8 * TILE, 10 * TILE));
        givePower3(0);
        CHECK3(!g_sim.cmdTrain(af, jetT), "a strike fighter could be built at an airfield");
        CHECK3(UNITS[jetT].builtBy == BR_NONE && UNITS[jetT].temp == 1, "the strike fighter is not marked as a loan");
        // a target worth the trip: a cluster of enemy tanks that sit still
        Vec2 spot = g_map.nearestFree(lakePoint(0, -17), 10);
        flattenArena(spot, 10, 10);
        std::vector<Ref> victims;
        for (int i = 0; i < 6; i++) { Ref r = g_sim.spawnUnit(firstUnitOf(foe) + 5, 1, spot + Vec2((i % 3 - 1) * 40.0f, (i / 3 - 0.5f) * 40.0f)); g_sim.get(r)->disabledUntil = 1e9f; victims.push_back(r); }
        float h0 = 0; for (Ref r : victims) h0 += hp3(r);
        int airBefore = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.isAir() && e.owner == 0) airBefore++;
        int flight = (int)g_sim.strikeManifest(0).size();
        CHECK3(flight >= 10, "an Air Strike sends fewer than ten fighters");
        CHECK3(g_sim.cmdPhoneStrike(0, spot), "the strike call was refused");
        // the waves come in one after another; every fighter takes no orders, makes one pass and leaves
        std::vector<Ref> seen; std::vector<int> dropped; float fastest = 0, minFade = 1.0f; int rockets = 0, bombs = 0, maxUp = 0; bool obeyed = false, offMap = false;
        for (int t = 0; t < 20 * 40; t++) {
            g_sim.step(); g_sim.events.clear();
            int up = 0;
            for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT) {
                Ref r = g_sim.refOf(e); up++;
                if (std::find(seen.begin(), seen.end(), r) == seen.end()) {
                    seen.push_back(r);
                    if (e.order != O_STRIKERUN) obeyed = true;
                    if (e.pos.x <= 0 || e.pos.y <= 0 || e.pos.x >= WORLD_W || e.pos.y >= WORLD_H) offMap = true;
                    g_sim.cmdMove({r}, g_sim.players[0].basePos, false); g_sim.cmdStop({r}); g_sim.cmdAttack({r}, victims[0]);
                    if (e.order != O_STRIKERUN) obeyed = true;
                }
                fastest = std::max(fastest, e.airspeed); minFade = std::min(minFade, e.fade);
                size_t k = std::find(seen.begin(), seen.end(), r) - seen.begin();
                if (k >= dropped.size()) dropped.resize(k + 1, 0);
                dropped[k] = std::max(dropped[k], STRIKE_BOMBS - (int)e.bombsLeft);
            }
            maxUp = std::max(maxUp, up);
            for (auto& p : g_sim.projs) if (p.weapon == W_STRIKE_C || p.weapon == W_STRIKE_K) rockets++;
        }
        for (int d : dropped) bombs += d;
        float h1 = 0; for (Ref r : victims) h1 += std::max(0.0f, hp3(r));
        CHECK3((int)seen.size() == flight, "the strike did not bring the whole flight");
        CHECK3(maxUp > STRIKE_WAVE && maxUp <= flight, "the waves did not overlap in the air");
        CHECK3(!obeyed, "a strike fighter obeyed an order");
        CHECK3(!offMap, "a fighter spawned off the map");
        CHECK3(fastest > 600.0f, "the strike fighters are not fast");
        CHECK3(rockets > 0, "the strike fighters fired no rockets");
        CHECK3(bombs >= flight * STRIKE_BOMBS / 2, "the strike fighters dropped few bombs");
        CHECK3(h1 < h0 * 0.35f, "the strike did not do big damage to what stood at the spot");
        CHECK3(minFade < 0.5f, "the fighters did not fade out over the sea");
        run3(30);
        CHECK3(g_sim.countUnits(0, jetT) == 0, "the strike fighters never left");
        int airAfter = 0; for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.isAir() && e.owner == 0) airAfter++;
        CHECK3(airAfter == airBefore, "the strike left aircraft behind with the army");
        CHECK3(g_sim.players[0].alive, "the army was counted out");
        // hard to shoot down: two Patriot / Rocket batteries at the target take few of them
        lakeScene(me, foe, seed + 4 + fi);
        placeNear3(techT, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));
        givePower3(1);
        spot = g_map.nearestFree(lakePoint(0, -17), 10);
        flattenArena(spot, 12, 12);
        Ref aa1 = g_sim.placeBuilding(firstBuildOf(foe) + BR_AATURRET, 1, tileOf(spot.x) + 4, tileOf(spot.y) - 2, true);
        Ref aa2 = g_sim.placeBuilding(firstBuildOf(foe) + BR_AATURRET, 1, tileOf(spot.x) - 5, tileOf(spot.y) + 2, true);
        g_sim.updatePowerPublic();
        CHECK3(g_sim.get(aa1) && g_sim.get(aa2), "no anti-air for the test");
        CHECK3(g_sim.cmdPhoneStrike(0, spot), "the second strike call was refused");
        int shotDown = 0; std::vector<Ref> up2;
        for (int t = 0; t < 20 * 40; t++) {
            g_sim.step();
            for (auto& ev : g_sim.events) (void)ev;
            g_sim.events.clear();
            for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.type == jetT && std::find(up2.begin(), up2.end(), g_sim.refOf(e)) == up2.end()) up2.push_back(g_sim.refOf(e));
        }
        for (Ref r : up2) { Entity* e = g_sim.get(r); (void)e; }
        shotDown = g_sim.players[0].unitsLost;
        CHECK3(shotDown * 3 < (int)up2.size(), "two anti-air batteries shot down a third of the flight");
        printf("strike fighters %s: ok (%d on the call in waves of %d, %.0f px/s, %d rockets and %d bombs, the target cluster %.0f%% destroyed, they faded out over the sea; %d of %d lost to two batteries)\n", FACTION_NAME[me], flight, STRIKE_WAVE, fastest, rockets, bombs, 100.0f * (1.0f - h1 / h0), shotDown, (int)up2.size());
    }
    printf("strikejettest: ok\n");
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- anti-nuke defences
// Fires warheads at the same spot over and over (the defences reload between trials) and counts how many were shot down
static int trialsShotDown(Vec2 gz, int trials, float scale, int* fragmentsOut) {
    int down = 0, frag = 0;
    for (int k = 0; k < trials; k++) {
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ANTINUKE) e.siloTimer = 0;
        g_sim.fallouts.clear(); g_sim.interceptors.clear();
        Sim::Nuke n{ g_sim.players[1].basePos, gz, 1, Sim::NUKE_FLIGHT - 4.2f, false, scale };
        g_sim.nukes.push_back(n);
        bool broken = false;
        for (int t = 0; t < 20 * 6 && !g_sim.nukes.empty(); t++) {
            g_sim.step(); g_sim.events.clear();
            if (getenv("ONEHOUR_DEBUG") && k == 0 && !g_sim.nukes.empty()) { const Sim::Nuke& q = g_sim.nukes[0]; fprintf(stderr, "t=%d nuke t=%.2f killT=%.2f engaged=%zu interceptors=%zu id=%d broken=%d power=%.2f\n", t, q.t, q.killT, q.engagedBy.size(), g_sim.interceptors.size(), q.id, (int)q.broken, q.power); }
            if (!g_sim.nukes.empty() && g_sim.nukes[0].broken) broken = true;
        }
        if (broken) { down++; if (!g_sim.fallouts.empty() && g_sim.fallouts[0].power < 0.31f) frag++; }
    }
    if (fragmentsOut) *fragmentsOut = frag;
    return down;
}

// The Anti-Nuke Laser (Cyber only) reaches further and sooner: it engages a warhead whose target is inside its cover plus the blast radius, opens
// fire ANTINUKE_LASER_WINDOW seconds out and recharges straight after its beams, so it can take a second warhead a salvo behind the first; lasers
// standing close to each other are linked: each link adds a beam, shortens the gap between beams and adds to the hit chance
static void laserScene(Faction me, u64 seed) {
    lakeScene(me, me == F_CYBER ? F_CLANKER : F_CYBER, seed);
    placeNear3(firstBuildOf(me) + BR_TECH, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));
    givePower3(0);
}
static Ref defenceAt(int type, Vec2 at) {
    flattenArena(at, 10, 10);
    Ref d = placeNear3(type, 0, at);
    g_sim.updatePowerPublic();
    return d;
}
// a warhead of player 1 aimed at 'at', already 'age' seconds into its flight (it lands NUKE_FLIGHT seconds after launch)
static void launchAt(Vec2 at, float age) {
    Sim::Nuke n{ g_sim.players[1].basePos, at, 1, age, false, 1.0f };
    g_sim.nukes.push_back(n);
}
// whether the warhead in slot k has been engaged by the defence with entity index 'who'
static bool engagedWith(size_t k, int who) {
    if (k >= g_sim.nukes.size()) return false;
    const std::vector<int>& by = g_sim.nukes[k].engagedBy;
    return std::find(by.begin(), by.end(), who) != by.end();
}
static bool laserChecks(u64 seed) {
    const Vec2 L = lakePoint(0, -16.5f);
    const float inR = (ANTINUKE_COVER_C + 0.6f * NUKE_RADIUS) * TILE, outR = (ANTINUKE_COVER_C + NUKE_RADIUS + 3.0f) * TILE;
    // (a) no blind ring: a warhead aimed inside the laser's widened cover is engaged before it lands, one aimed beyond it is not
    laserScene(F_CYBER, seed);
    Ref lz = defenceAt(B_C_ANTINUKE, L);
    CHECK3(lz.valid(), "no spot for an anti-nuke laser");
    Vec2 lp = g_sim.get(lz)->pos;
    CHECK3(g_sim.antiNukeLinked(*g_sim.get(lz)) == 0, "a lone laser counts linked neighbours");
    launchAt(lp + Vec2(inR, 0), 0.0f);
    run3(1.0f);
    CHECK3(engagedWith(0, lz.idx), "a laser did not engage a warhead aimed 0.6 blast radii inside its cover");
    g_sim.nukes.clear(); g_sim.get(lz)->siloTimer = 0;
    launchAt(lp + Vec2(0, outR), 0.0f);
    run3(1.0f);
    CHECK3(!engagedWith(0, lz.idx), "a laser engaged a warhead aimed beyond its widened cover");
    // (b) a salvo: a second warhead launched 1.2 s after the first, at the same spot, is engaged too (the laser recharges right after its beams)
    g_sim.nukes.clear(); g_sim.get(lz)->siloTimer = 0;
    launchAt(lp + Vec2(inR, 0), 0.0f);
    run3(1.2f);
    launchAt(lp + Vec2(inR, 0), 0.0f);
    run3(3.0f);
    CHECK3(engagedWith(0, lz.idx) && engagedWith(1, lz.idx), "a laser did not engage both warheads of a salvo");
    // the Clanker missile silo keeps its 12 s reload: it engages the first of the two and not the second
    laserScene(F_CLANKER, seed + 1);
    Ref sl = defenceAt(B_K_ANTINUKE, L);
    CHECK3(sl.valid(), "no spot for an anti-nuke missile silo");
    Vec2 sp = g_sim.get(sl)->pos + Vec2(10 * TILE, 0);   // (inside its 19 tile cover)
    launchAt(sp, 0.0f);
    run3(1.2f);
    launchAt(sp, 0.0f);
    run3(4.8f);
    CHECK3(engagedWith(0, sl.idx), "a missile silo did not engage a warhead inside its cover");
    CHECK3(!engagedWith(1, sl.idx), "a missile silo engaged a second warhead within its reload time");
    // (c) stacking: four lasers five tiles apart are linked to the other three: each adds three beams and 3 x 0.09 to its chance
    // (the warhead chance it rolls is the linked chance after the 40% Anti-Nuke Laser sighting nerf)
    laserScene(F_CYBER, seed + 2);
    const int offs[4][2] = { { 0, 0 }, { 4, 0 }, { 0, 4 }, { 4, 4 } };
    std::vector<Ref> stack;
    for (auto& o : offs) {
        Ref r = defenceAt(B_C_ANTINUKE, L + Vec2(o[0] * TILE, o[1] * TILE));
        CHECK3(r.valid(), "no spot for a stacked laser");
        stack.push_back(r);
    }
    const float shotRaw = ANTINUKE_CHANCE + 3 * ANTINUKE_LINK_BONUS;
    const float shot3 = shotRaw * ANTINUKE_LASER_NUKE_MUL;
    for (Ref r : stack) {
        const Entity* e = g_sim.get(r);
        CHECK3(g_sim.antiNukeLinked(*e) == 3, "a laser with three neighbours within 9 tiles is not linked to all three");
        CHECK3(std::abs(g_sim.antiNukeShot(*e) - shotRaw) < 1e-5f, "a linked laser does not get 0.09 more chance per link");
        CHECK3(std::abs(g_sim.antiNukeWarheadShot(*e) - shot3) < 1e-5f, "a linked laser's warhead chance is not the nerfed one");
    }
    Vec2 spot = L + Vec2(20 * TILE, 0);   // inside the cover of every one of them, yet outside the blast (11 tiles) of a warhead that lands: the trials leave them standing
    launchAt(spot, 0.0f);
    for (int k = 0; k < 40 && g_sim.interceptors.empty(); k++) { g_sim.step(); g_sim.events.clear(); }
    // every laser that engages fires three beams plus one per linked laser (a warhead that an earlier laser's beam already dooms is left alone)
    const int beamsEach = 3 + ANTINUKE_LINK_MAX;
    int beams = 0, engaged = 0; bool allEach = true;
    for (Ref r : stack) {
        int mine = 0;   // (a beam leaves its laser's muzzle, 22 px above its centre)
        for (auto& it : g_sim.interceptors) if (dist(it.from, g_sim.get(r)->pos) < TILE) mine++;
        bool went = g_sim.nukes.size() == 1 && engagedWith(0, r.idx);
        allEach = allEach && mine == (went ? beamsEach : 0);
        beams += mine; engaged += went;
    }
    CHECK3(engaged >= 1 && allEach && beams == engaged * beamsEach && g_sim.nukes.size() == 1, "a linked laser did not fire 3 + linked beams at the warhead");
    g_sim.nukes.clear(); g_sim.interceptors.clear();
    float wantS = g_sim.antiNukeChance(spot, 1);
    CHECK3(std::abs(wantS - (1.0f - std::pow(1.0f - shot3, 4.0f))) < 1e-4f, "four linked lasers do not stop a warhead with 1 - (1 - p)^4");
    int trials = 240, downS = trialsShotDown(spot, trials, 1.0f, nullptr);
    float rateS = downS / (float)trials, sigmaS = std::sqrt(wantS * (1 - wantS) / trials);
    if (std::abs(rateS - wantS) > 4.0f * sigmaS + 0.02f) { fprintf(stderr, "antinuketest: four linked lasers shot down %.1f%% of %d warheads, expected %.1f%%\n", rateS * 100, trials, wantS * 100); return false; }
    printf("anti-nuke %s: lasers ok (aimed %.1f tiles off a laser engaged, %.1f tiles off not; a second warhead 1.2 s behind engaged too, a missile silo only the first; four linked lasers (%d beams on the first warhead) stop %.0f%% of warheads, expected %.0f%%)\n",
           FACTION_NAME[F_CYBER], inR / TILE, outR / TILE, beams, rateS * 100, wantS * 100);
    return true;
}

bool antiNukeTest(u64 seed) {
    g_name3 = "antinuketest"; g_ok3 = true;
    for (int fi = 0; fi < 2; fi++) {
        Faction me = (Faction)fi, foe = (Faction)(1 - fi);
        lakeScene(me, foe, seed + fi);
        int techT = firstBuildOf(me) + BR_TECH, anT = me == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE;
        CHECK3(!g_sim.buildAvailable(0, anT), "an anti-nuke defence is buildable without a tech structure");
        placeNear3(techT, 0, g_sim.players[0].basePos + Vec2(8 * TILE, 8 * TILE));
        givePower3(0);
        CHECK3(g_sim.buildAvailable(0, anT), "an anti-nuke defence is not buildable with a tech structure");
        CHECK3(BUILDS[anT].w * BUILDS[anT].h <= 4, "the anti-nuke defence is not a small structure");
        Vec2 gz = lakePoint(0, -16.5f);   // (north of the lake, far from both bases: the test's own warheads must not blow up the power plants)
        flattenArena(gz, 20, 20);
        std::vector<Ref> ds;
        // none: nothing is ever shot down
        CHECK3(trialsShotDown(gz, 20, 1.0f, nullptr) == 0, "a warhead was shot down with no defences");
        // one defence: about a third; three: about two thirds; six: nearly always
        for (int n = 1; n <= 6; n++) {
            // the defences ring the target at 13.5 tiles: inside everybody's cover and outside the blast of the test warheads
            float ang = n * 1.0472f + 0.3f;
            Ref d = placeNear3(anT, 0, gz + Vec2(std::cos(ang), std::sin(ang)) * (13.5f * TILE));
            CHECK3(d.valid(), "no spot for a defence");
            ds.push_back(d);
            g_sim.updatePowerPublic();
            if (n == 1 || n == 3 || n == 6) {
                if (g_sim.antiNukeCovering(gz, 1) != n) { fprintf(stderr, "antinuketest: %d covering, expected %d (power %d/%d disabled? ", g_sim.antiNukeCovering(gz, 1), n, g_sim.players[0].powerMade, g_sim.players[0].powerUsed); for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.owner == 0) fprintf(stderr, "{%s p%d c%d} ", e.bt().name, e.bt().power, (int)e.constructed); for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ANTINUKE) fprintf(stderr, "[%.0f,%.0f dist %.0f cover %.0f constructed %d disabled %.1f] ", e.pos.x, e.pos.y, dist(e.pos, gz), g_sim.antiNukeCover(e, 1), (int)e.constructed, e.disabledUntil); fprintf(stderr, ")\n"); return failMsg3("the number of defences covering the target is wrong"); }
                float want = g_sim.antiNukeChance(gz, 1);   // (the ring is 13.5 tiles across: no two of them are linked)
                float pOne = ANTINUKE_CHANCE * (me == F_CYBER ? ANTINUKE_LASER_NUKE_MUL : 1.0f);   // the laser rolled its nerfed warhead chance, the missile silo its own
                CHECK3(std::abs(want - (1.0f - std::pow(1.0f - pOne, (float)n))) < 1e-4f, "the stated chance is not 1 - (1 - p)^n");
                int frag = 0, trials = 240;
                int down = trialsShotDown(gz, trials, 1.0f, &frag);
                float rate = down / (float)trials, sigma = std::sqrt(want * (1 - want) / trials);
                if (std::abs(rate - want) > 4.0f * sigma + 0.02f) { fprintf(stderr, "antinuketest: %d defences shot down %.0f%% of %d warheads, expected %.0f%%\n", n, rate * 100, trials, want * 100); return failMsg3("the interception rate does not follow the number of defences"); }
                CHECK3(frag == down, "a shot-down warhead did not leave weak radiation");
                printf("anti-nuke %s: %d defence%s stop %.0f%% of warheads (expected %.0f%%)\n", FACTION_NAME[me], n, n == 1 ? "" : "s", rate * 100, want * 100);
            }
        }
        // mini nukes are covered the same way (whatever the six defences' current combined chance is)
        { float wantMini = g_sim.antiNukeChance(gz, 1, false, MINI_NUKE_SCALE);
          int d1 = trialsShotDown(gz, 120, MINI_NUKE_SCALE, nullptr);
          CHECK3(d1 > 120 * (wantMini - 0.10f), "six defences let too many bunker missiles through"); }
        // out of cover: untouched
        float coverD = g_sim.antiNukeCover(*g_sim.get(ds.back()), 1);   // (a laser's cover: its missiles' cover plus the blast radius)
        Vec2 far = gz + Vec2(0, coverD * 2.2f);   // (further from every defence than its cover reaches: they ring the target at 13.5 tiles)
        CHECK3(g_sim.antiNukeCovering(far, 1) == 0, "a spot beyond the cover counts as covered");
        // no power, or switched off: blind
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ANTINUKE) e.disabledUntil = g_sim.time + 1000;
        CHECK3(trialsShotDown(gz, 30, 1.0f, nullptr) == 0, "a switched-off defence shot a warhead down");
        for (auto& e : g_sim.ents) if (e.alive && e.isBuilding() && e.bt().role == BR_ANTINUKE) e.disabledUntil = -1;
        // the pieces do 30% of the damage: a barracks and a tank at ground zero of a shot-down warhead vs a whole one
        lakeScene(me, foe, seed + 7 + fi);
        flattenArena(gz, 10, 10);
        auto damageAt = [&](float power, bool broken) {
            Ref b = g_sim.placeBuilding(firstBuildOf(foe) + BR_BARRACKS, 1, tileOf(gz.x) - 1, tileOf(gz.y) - 1, true);
            Ref u = g_sim.spawnUnit(firstUnitOf(foe) + 5, 1, gz + Vec2(0, 3 * TILE));
            float bh = hp3(b), uh = hp3(u);
            Sim::Nuke n{ g_sim.players[0].basePos, g_sim.get(b)->pos, 0, Sim::NUKE_FLIGHT - 0.05f, false, 1.0f, power };
            n.broken = broken; n.killT = broken ? 1.0f : -1.0f;
            g_sim.nukes.push_back(n);
            run3(0.4f);
            float lostB = alive3(b) ? bh - hp3(b) : bh, lostU = alive3(u) ? uh - hp3(u) : uh;
            g_sim.fallouts.clear();
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 1 && e.kind != EK_RESOURCE && !(e.isBuilding() && e.bt().role == BR_HQ)) g_sim.destroy(e, false);
            return Vec2(lostB / bh, lostU / uh);
        };
        Vec2 whole = damageAt(1.0f, false), pieces = damageAt(ANTINUKE_FALLOFF, true);
        CHECK3(whole.x > 0.95f && whole.y > 0.95f, "a whole warhead did not destroy a barracks and a tank at ground zero");
        CHECK3(std::abs(pieces.x - ANTINUKE_FALLOFF) < 0.06f && std::abs(pieces.y - ANTINUKE_FALLOFF) < 0.06f, "the pieces of a shot-down warhead do not do 30% of the damage");
        printf("anti-nuke %s: ok (pieces take %.0f%% of a barracks and %.0f%% of a tank at ground zero where a whole warhead takes %.0f%%/%.0f%%)\n", FACTION_NAME[me], pieces.x * 100, pieces.y * 100, whole.x * 100, whole.y * 100);
        if (me == F_CYBER && !laserChecks(seed + 20)) return false;
    }
    printf("antinuketest: ok\n");
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- vehicle dynamics
// Land vehicles have weight: they build up speed over about a third of a second, ease in over the last stretch and slow while the hull swings round
bool moveTest(u64 seed) {
    g_name3 = "movetest"; g_ok3 = true;
    lakeScene(F_CYBER, F_CLANKER, seed);
    Vec2 start = g_map.nearestFree(lakePoint(-8, -17), 10);
    flattenArena(start, 18, 8);
    int tankT = U_C_TANK;
    float vmax = UNITS[tankT].speed;
    Ref t = g_sim.spawnUnit(tankT, 0, start);
    Entity* e = g_sim.get(t); e->angle = 0;   // facing the way it will go
    Vec2 goal = start + Vec2(12.0f * TILE, 0);
    g_sim.cmdMove({t}, goal, false);
    std::vector<float> speeds; float arrivedAt = -1;
    for (int k = 0; k < 20 * 20; k++) {
        Vec2 before = g_sim.get(t)->pos;
        g_sim.step(); g_sim.events.clear();
        float v = dist(g_sim.get(t)->pos, before) / SIM_DT;
        speeds.push_back(v);
        if (arrivedAt < 0 && dist(g_sim.get(t)->pos, goal) < 4.0f) arrivedAt = k * SIM_DT;
    }
    CHECK3(speeds[1] < vmax * 0.6f, "a tank leaps to full speed from a standstill");
    CHECK3(speeds[12] > vmax * 0.93f, "a tank does not reach full speed within a second");
    CHECK3(arrivedAt > 0 && arrivedAt < 12.0f * TILE / vmax + 2.0f, "a tank is slow to cover twelve tiles");
    float lastV = 0; for (int k = (int)(arrivedAt * SIM_HZ) - 3; k >= 0 && k < (int)speeds.size() && k < (int)(arrivedAt * SIM_HZ); k++) lastV = std::max(lastV, speeds[k]);
    CHECK3(lastV < vmax * 0.6f, "a tank arrives at full speed instead of easing in");
    CHECK3(dist(g_sim.get(t)->pos, goal) < 6.0f, "a tank overshot its destination");
    run3(1.0f);
    CHECK3(g_sim.get(t)->airspeed < 1.0f, "a stopped tank keeps its speed");
    // facing away from where it is sent: it slows while the hull swings round
    Ref t2 = g_sim.spawnUnit(tankT, 0, start + Vec2(0, 3.0f * TILE));
    g_sim.get(t2)->angle = 3.14159f;
    g_sim.cmdMove({t2}, start + Vec2(12.0f * TILE, 3.0f * TILE), false);
    float swingMax = 0; for (int k = 0; k < 4; k++) { Vec2 b = g_sim.get(t2)->pos; g_sim.step(); g_sim.events.clear(); swingMax = std::max(swingMax, dist(g_sim.get(t2)->pos, b) / SIM_DT); }
    CHECK3(swingMax < vmax * 0.5f, "a tank facing the wrong way drives off at speed before its hull has turned");
    // infantry are light on their feet: no ramp
    Ref in = g_sim.spawnUnit(U_C_INF1, 0, start + Vec2(0, -3.0f * TILE));
    g_sim.cmdMove({in}, start + Vec2(10.0f * TILE, -3.0f * TILE), false);
    Vec2 b0 = g_sim.get(in)->pos; g_sim.step(); g_sim.events.clear();
    CHECK3(dist(g_sim.get(in)->pos, b0) / SIM_DT > UNITS[U_C_INF1].speed * 0.9f, "infantry are slowed by the vehicle dynamics");
    printf("movetest: ok (a tank is at %.0f%% speed after a tick, %.0f%% after a second, eases in to %.0f%% over the last tile and arrives in %.1f s)\n", 100 * speeds[1] / vmax, 100 * speeds[12] / vmax, 100 * lastV / vmax, arrivedAt);
    return g_ok3;
}

// ---------------------------------------------------------------------------------------------------------------- screenshot scene
// ONEHOUR_NAVY=1 --shot: the lake with both armies' yards, destroyers and cruisers under their drone screens trading fire, jeeps with crews on the shore,
// two anti-nuke defences and a strike marker (setup only: the caller then simulates a few ticks and renders)
void navyShowcase(int vp, Vec2& camOut) {
    int foe = vp == 0 ? 1 : 0;
    Faction f = g_sim.players[vp].faction, ff = g_sim.players[foe].faction;
    for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
    for (int p : { vp, foe }) { g_sim.players[p].money = 400000; g_sim.players[p].upg[UPG_DRONES] = true; }
    auto yardNear = [&](int owner, Faction fac, Vec2 near) {
        int tx, ty;
        placeNear3(firstBuildOf(fac) + BR_POWER, owner, g_sim.players[owner].basePos + Vec2(0, 6 * TILE));
        placeNear3(firstBuildOf(fac) + BR_FACTORY, owner, g_sim.players[owner].basePos + Vec2(0, 9 * TILE));
        placeNear3(firstBuildOf(fac) + BR_TECH, owner, g_sim.players[owner].basePos + Vec2(8 * TILE, 8 * TILE));
        if (g_sim.shoreSite(owner, fac == F_CYBER ? B_C_NAVAL : B_K_NAVAL, near, tx, ty)) return g_sim.placeBuilding(fac == F_CYBER ? B_C_NAVAL : B_K_NAVAL, owner, tx, ty, true);
        return NOREF;
    };
    Vec2 lake = lakePoint(0, 0);
    Ref y0 = yardNear(vp, f, lake + (g_sim.players[vp].basePos - lake) * 0.5f);
    Ref y1 = yardNear(foe, ff, lake + (g_sim.players[foe].basePos - lake) * 0.5f);
    g_sim.updatePowerPublic();
    int dM = f == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, cM = f == F_CYBER ? U_C_CRUISER : U_K_CRUISER, dF = ff == F_CYBER ? U_C_DESTROYER : U_K_DESTROYER, cF = ff == F_CYBER ? U_C_CRUISER : U_K_CRUISER;
    Vec2 mc = g_sim.get(y0) ? g_sim.get(y0)->pos : lake, fc = g_sim.get(y1) ? g_sim.get(y1)->pos : lake;
    Vec2 toF = (fc - mc).norm(), side(-toF.y, toF.x), mid = (mc + fc) * 0.5f;
    std::vector<Ref> mine, theirs;
    for (int i = 0; i < 3; i++) { mine.push_back(ship3(i == 1 ? cM : dM, vp, mid - toF * (4.0f * TILE) + side * ((i - 1) * 3.2f * TILE))); theirs.push_back(ship3(i == 1 ? cF : dF, foe, mid + toF * (4.0f * TILE) + side * ((i - 1) * 3.2f * TILE))); }
    g_sim.cmdMove(mine, mid + toF * 2.0f * TILE, true); g_sim.cmdMove(theirs, mid - toF * 2.0f * TILE, true);
    // jeeps with their crews on the shore, and two anti-nuke defences
    Vec2 shoreSpot = g_map.nearestFree(mc + (lake - mc).norm() * (3.0f * TILE) + side * (5.0f * TILE), 10);
    for (int j = 0; j < 2; j++) {
        Ref jp = g_sim.spawnUnit(f == F_CYBER ? U_C_JEEP : U_K_JEEP, vp, shoreSpot + Vec2(j * 46.0f, 30.0f));
        for (int t : g_sim.jeepCrew(vp)) { Ref c = g_sim.spawnUnit(t, vp, shoreSpot); if (Entity* cu = g_sim.get(c)) if (Entity* je = g_sim.get(jp)) g_sim.boardQuiet(*cu, *je); }
    }
    placeNear3(firstBuildOf(f) + BR_TECH, vp, shoreSpot + Vec2(0, 5 * TILE));
    placeNear3(f == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE, vp, shoreSpot + Vec2(4 * TILE, 3 * TILE));
    camOut = Vec2(clampf(mid.x - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W), clampf(mid.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H));
}

// ONEHOUR_AIR=strike|drop|nuke --shot (ONEHOUR_AIRTICKS=n ticks after the call): the Hotline Strike with its two fighters over an enemy cluster, a jeep drop on its
// parachutes, or an anti-nuke defence ring meeting a warhead. Sets the scene up and returns the camera centre; the caller steps and renders.
void airShowcase(int vp, const char* which, Vec2& camOut, Ref& follow) {
    int foe = vp == 0 ? 1 : 0;
    Faction f = g_sim.players[vp].faction, ff = g_sim.players[foe].faction;
    for (auto& pl : g_sim.players) std::fill(pl.explored.begin(), pl.explored.end(), 1);
    g_sim.players[vp].money = 400000; g_sim.players[vp].dropReady = 0;
    for (int u = 0; u < UPG_COUNT; u++) g_sim.players[vp].upg[u] = true;
    g_sim.players[vp].advTech = true;
    Vec2 spot = g_map.nearestFree(lakePoint(0, -17), 10);
    flattenArena(spot, 14, 12);
    std::string w = which;
    placeNear3(firstBuildOf(f) + BR_TECH, vp, g_sim.players[vp].basePos + Vec2(8 * TILE, 8 * TILE));
    placeNear3(firstBuildOf(f) + BR_FACTORY, vp, g_sim.players[vp].basePos + Vec2(0, 9 * TILE));
    givePower3(vp);
    if (w == "strike") {
        for (int i = 0; i < 12; i++) { Ref r = g_sim.spawnUnit(firstUnitOf(ff) + (i % 3 == 0 ? 5 : 2), foe, spot + Vec2((i % 4 - 1.5f) * 34.0f, (i / 4 - 1) * 34.0f)); g_sim.get(r)->disabledUntil = 1e9f; }
        for (int i = 0; i < 2; i++) placeNear3(firstBuildOf(ff) + BR_POWER, foe, spot + Vec2((i * 2 - 1) * 150.0f, 100.0f));
        g_sim.cmdPhoneStrike(vp, spot);
    } else if (w == "drop") {
        g_sim.cmdParadrop(vp, spot, false);
    } else {   // nuke: a defence ring and a warhead on its way to a cluster in the middle of it
        int anT = ff == F_CYBER ? B_C_ANTINUKE : B_K_ANTINUKE;
        for (int k = 0; k < 4; k++) { float a = k * 1.5708f + 0.3f; placeNear3(anT, foe, spot + Vec2(std::cos(a), std::sin(a)) * (7.5f * TILE)); }
        givePower3(foe);
        for (int i = 0; i < 8; i++) g_sim.spawnUnit(firstUnitOf(ff) + 5, foe, spot + Vec2((i % 4 - 1.5f) * 36.0f, (i / 4 - 0.5f) * 36.0f));
        g_sim.nukes.push_back({g_sim.players[vp].basePos, spot, vp, Sim::NUKE_FLIGHT - 5.2f, false});
    }
    camOut = Vec2(clampf(spot.x - SCREEN_W * 0.5f, 0, WORLD_W - SCREEN_W), clampf(spot.y - VIEW_H * 0.5f, 0, WORLD_H - VIEW_H));
    follow = NOREF;
    if (w == "strike") for (auto& e : g_sim.ents) if (e.alive && e.isUnit() && e.ut().temp && e.owner == vp) { follow = g_sim.refOf(e); break; }
}
