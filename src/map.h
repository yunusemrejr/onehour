// One Hour - the single map: terrain, occupancy, pathfinding
#pragma once
#include "common.h"
#include "data.h"

enum Tile : u8 { T_GRASS = 0, T_GRASS2, T_DIRT, T_SAND, T_ROAD, T_WATER, T_ROCK, T_TREE, T_COUNT };

struct StartSpot { int tx, ty; };
struct SupplySpot { int tx, ty; int amount; };
// Places on the map where the old civilian strongholds stand. kind: 0 castle, 1 civilian bunker, 2 watchtower, 3 cottage, 4 workshop,
// 5 Mountain Fortress (set into a ridge), 6 Island Nuke Launcher (one on each lake island) (see Sim::spawnMapLandmarks).
// They are filled in by generate() in four rotated copies, so every corner of the map gets the same set.
struct LandmarkSpot { int tx, ty; int kind; };
struct RoadSeg { float x0, y0, x1, y1, halfW; };
// The towns and cities round the lake (see DISTRICT_* in data.h and Sim::districts): a cluster of civic blocks (CivicKind, top-left tile) with a
// paved street round every block. 'city' areas use the glass-and-steel (Cyber) civic designs, the others the old brick (Clanker) ones.
struct DistrictBlock { int kind, tx, ty; };
struct DistrictSpot { const char* name; bool city; float cx, cy; std::vector<DistrictBlock> blocks; };   // tile units; kept so the baked terrain can paint centre lines

// The second island off the main island's east coast (Map::seaIslands[0]): an ellipse RX x RY tiles round (SEA_ISLE_X, the map's middle row)
static const float SEA_ISLE_X = ISLE_X0 + ISLE_W + 23.0f, SEA_ISLE_RX = 15.0f, SEA_ISLE_RY = 38.0f;
float mainIslandInland(int x, int y);   // tiles inland from the main island's coast (<= 0: the sea)
float seaIslandInland(int x, int y);    // ... from the second island's coast

struct Map {
    u8 tiles[MAP_W * MAP_H];
    u8 blocked[MAP_W * MAP_H];      // bit0 terrain, bit1 structure, bit2 resource pile
    u8 variant[MAP_W * MAP_H];      // visual variation
    u8 bridge[MAP_W * MAP_H];       // road laid over a river (drawn as a timber deck)
    u16 waterComp[MAP_W * MAP_H];   // connected body of water a water tile belongs to (0 = not water), see waterSize
    u8 clearable[MAP_W * MAP_H];    // vegetation a structure may be built over (it is cleared away): 1 a tree, 2 a small rock outcrop (not a ridge)
    std::vector<int> cleared;       // footprint tiles of structures placed since the renderer last repainted the baked ground: tile | kind << 24 (1 footprint, 2 quarried rock), each footprint closed by -1
    std::vector<int> waterSize;     // tiles of each body of water, indexed by waterComp (the central lake is by far the biggest)
    int lakeComp = 0;               // waterComp id of the central lake on the main island
    std::vector<StartSpot> starts;   // 4 corner start positions (HQ center tile)
    std::vector<SupplySpot> supplies;
    std::vector<LandmarkSpot> landmarks;   // neutral civilian castles / bunkers / watchtowers, 4 rotated copies of each kind, the ridge fortresses and island launchers
    std::vector<DistrictSpot> districts;   // the civilian areas round the lake (four: either side of the north and south shore piles)
    struct Island { float cx, cy, r; };    // the lake's islands (tile units): [0] the big one in the middle, [1] the small one off the east shore
    std::vector<Island> islands;
    struct SeaIsland { float cx, cy, rx, ry; };   // out in the ocean: [0] the second island, then the islets (tile units)
    std::vector<SeaIsland> seaIslands;
    int oceanComp = 0;              // waterComp id of the ocean round the islands (the largest body of water)
    std::vector<RoadSeg> roads;
    // Interior water (water with water on all four sides), pre-grouped for the animated overlays. The lake and the rivers get caustics everywhere:
    // origins of aligned 4x4 all-interior blocks (one 128 px draw each) and every interior tile no block covers. Filled by generate().
    std::vector<int> waterBlocks;
    std::vector<int> waterTiles;
    // The whole ocean, shore to open sea, gets one slow swell (caustics there would read as a repeating grid): aligned 8x8 blocks (seaBlocks),
    // 4x4 blocks (seaQuads) and single tiles (seaTiles), so it covers every interior tile with no seam between "near shore" and "open sea";
    // its shallows (seaShallow: shoreDist <= SEA_CAUSTIC_REACH) also get caustics tile by tile, fading out as the bottom drops away.
    // shoreDist: tiles from the nearest land (chamfered, capped at 255).
    std::vector<int> seaBlocks, seaQuads, seaTiles, seaShallow;
    static constexpr int SEA_CAUSTIC_REACH = 6;
    std::vector<u8> shoreDist;
    std::vector<u8> waterDist;      // land tiles: tiles to the nearest water of any kind (chamfered, capped at 255; 0 on water)

    void generate();
    u8 tile(int x, int y) const { return tiles[y * MAP_W + x]; }
    bool passable(int x, int y) const { return inMap(x, y) && blocked[y * MAP_W + x] == 0; }
    bool terrainPassable(int x, int y) const { return inMap(x, y) && (blocked[y * MAP_W + x] & 1) == 0; }
    // Trees, bushes and small rocks never stop a structure: they are cleared away when it is placed (clearForStructure). Water, ridges, other
    // structures, supply piles and bridge decks do.
    bool buildable(int x, int y) const {
        if (!inMap(x, y)) return false;
        int i = y * MAP_W + x;
        u8 t = tiles[i];
        if (bridge[i] != 0 || (blocked[i] & ~1) != 0 || t == T_WATER) return false;   // (nothing is built on a bridge deck: it would plug the crossing)
        if (t == T_TREE || t == T_ROCK) return clearable[i] != 0;
        return blocked[i] == 0;
    }
    bool vegetation(int x, int y) const { return inMap(x, y) && clearable[y * MAP_W + x] != 0 && (tiles[y * MAP_W + x] == T_TREE || tiles[y * MAP_W + x] == T_ROCK); }   // would be cleared by a structure
    void clearForStructure(int x, int y, int w, int h);   // trees / small rocks under a new structure's footprint turn into open ground
    // Waymaker ground: any land tile away from the map's border cliffs with no water, bridge deck, structure or supply pile on it. Trees, rock and
    // the ridges are all fair game: clearWay turns the whole footprint into a graded earth track (walkable, buildable) and logs it for the renderer.
    bool wayTile(int x, int y) const {
        if (x < 1 || y < 1 || x > MAP_W - 2 || y > MAP_H - 2) return false;
        int i = y * MAP_W + x;
        return tiles[i] != T_WATER && bridge[i] == 0 && (blocked[i] & ~1) == 0;
    }
    int clearWay(int x, int y, int w, int h);   // returns how many tiles were blocked before
    bool clearWayTile(int x, int y);           // one tile of it (true if it was blocked); end the footprint with clearWayDone()
    void clearWayDone() { if (cleared.size() < 200000) cleared.push_back(-1); }
    // Would a w x h structure at (x, y) cut the ground around it in two (a corridor between rock and water, a gap between two other structures)? Counts the
    // 4-connected regions of free ground in the window of a few tiles round the footprint before and after: more of them after means it would plug something.
    bool wouldPlug(int x, int y, int w, int h) const;
    void setStructure(int x, int y, int w, int h, bool on);
    void setResource(int x, int y, bool on);
    // Connected ground: every passable tile carries the id of the 4-connected region it belongs to (0 = blocked). A* moves diagonally only between two
    // open orthogonal neighbours, so these are exactly the places a ground route can reach from each other. A search for a spot in another region is
    // bound to fail after flooding everything it can reach (most failed searches in a battle: units re-planning toward a target behind walls), so
    // findPath goes straight for the closest point of its own region instead. Rebuilt lazily after the map's own setters change 'blocked';
    // code that writes 'blocked' directly (tests) must call touch().
    mutable std::vector<u16> region;
    mutable bool regionDirty = true;
    mutable u32 cacheGen = 0;           // generation of closestInRegion's memo (0 after a rebuild: it starts afresh)
    void touch() { regionDirty = true; }
    void buildRegions() const;
    bool closestInRegion(u16 rg, int gx, int gy, int& bx, int& by) const;   // the tile of region rg nearest to (gx, gy) (octile distance)
    float moveCost(int x, int y) const { return tiles[y * MAP_W + x] == T_ROAD ? 0.8f : 1.0f; }

    // Pathfinding: fills out with tile centers (excluding start tile), returns false if unreachable.
    // If the goal is blocked, the nearest reachable tile is used.
    // swim: an infiltrator may also cross open water (see swimTile); it costs more than walking, so land is still preferred where it goes.
    bool findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes = MAP_W * MAP_H, bool swim = false) const;
    // Path to the nearest reachable tile beside a w x h footprint at (x, y) (any of its sides, by real path length): dozers and spies walk up to
    // the face of a structure that is on their side instead of a fixed doorstep that may be round the back. False if no side can be reached.
    bool findPathToRect(Vec2 from, int x, int y, int w, int h, std::vector<Vec2>& out, int maxNodes = MAP_W * MAP_H, bool swim = false) const;
    bool lineClear(Vec2 a, Vec2 b, bool swim = false) const;   // straight ground line free of blocked tiles (swim: water is free too)
    void pullString(Vec2 from, const std::vector<Vec2>& tiles, std::vector<Vec2>& out, bool swim = false) const;   // a tile route into straight-line waypoints
    // Water an infiltrator can swim through: any water tile (lake, river, pond) with nothing standing in it
    bool swimTile(int x, int y) const { return inMap(x, y) && tiles[y * MAP_W + x] == T_WATER && (blocked[y * MAP_W + x] & ~1) == 0; }
    bool swimPassable(int x, int y) const { return passable(x, y) || swimTile(x, y); }
    Vec2 nearestFree(Vec2 p, int maxR = 6) const;

    // ---- the lake: ships sail on water only. A tile is sailable if it is water with no structure on it (bridges are road, so they close a
    // river to ships); a tile is "open" if it and its four neighbours are sailable, which leaves room for a hull between the shores.
    bool navTile(int x, int y) const { return inMap(x, y) && tiles[y * MAP_W + x] == T_WATER && (blocked[y * MAP_W + x] & ~1) == 0; }
    bool navOpen(int x, int y) const { return navTile(x, y) && navTile(x + 1, y) && navTile(x - 1, y) && navTile(x, y + 1) && navTile(x, y - 1); }
    // the lake or the ocean (the two seas ships sail), not a pond or a stretch of river cut off by a bridge
    bool seaComp(int c) const { return c != 0 && (c == lakeComp || c == oceanComp) && waterSize[c] >= NAVAL_WATER_MIN; }
    bool bigWater(int x, int y) const { return inMap(x, y) && seaComp(waterComp[y * MAP_W + x]); }
    // centre of the nearest open water tile (p itself when it already is one); bigOnly: on the lake or the ocean, not in a pond or a cut-off river;
    // comp: only in that body of water (a ship on the ocean cannot be sent onto the lake)
    Vec2 nearestWater(Vec2 p, int maxR = 12, bool bigOnly = false, int comp = 0) const;
    std::vector<u16> landComp;      // per tile: the landmass it belongs to (0 = water); built with the map
    // the landmass at p, or the one within 'r' tiles of it (a structure on the shore, a ship lying off it); 0 if there is none
    int landNear(Vec2 p, int r = 0) const {
        int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1);
        if (landComp.empty()) return 0;
        for (int k = 0; k <= r; k++) for (int dy = -k; dy <= k; dy++) for (int dx = -k; dx <= k; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != k || !inMap(tx + dx, ty + dy)) continue;
            int c = landComp[(ty + dy) * MAP_W + tx + dx];
            if (c) return c;
        }
        return 0;
    }
    int compAt(Vec2 p) const { int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1); return waterComp[ty * MAP_W + tx]; }
    // like findPath for ships; the goal is moved to the nearest open water of the body the ship is on (the ocean's routes round the islands are long)
    bool findNavalPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes = 40000) const;
    bool navLineClear(Vec2 a, Vec2 b) const;                                     // straight line over open water
};

extern Map g_map;
