// One Hour - the single map: terrain, occupancy, pathfinding
#pragma once
#include "common.h"
#include "data.h"

enum Tile : u8 { T_GRASS = 0, T_GRASS2, T_DIRT, T_SAND, T_ROAD, T_WATER, T_ROCK, T_TREE, T_COUNT };

struct StartSpot { int tx, ty; };
struct SupplySpot { int tx, ty; int amount; };
// Places on the map where the old civilian strongholds stand. kind: 0 castle, 1 civilian bunker, 2 watchtower, 3 cottage, 4 workshop (see Sim::spawnMapLandmarks).
// They are filled in by generate() in four rotated copies, so every corner of the map gets the same set.
struct LandmarkSpot { int tx, ty; int kind; };
struct RoadSeg { float x0, y0, x1, y1, halfW; };   // tile units; kept so the baked terrain can paint centre lines

struct Map {
    u8 tiles[MAP_W * MAP_H];
    u8 blocked[MAP_W * MAP_H];      // bit0 terrain, bit1 structure, bit2 resource pile
    u8 variant[MAP_W * MAP_H];      // visual variation
    u8 bridge[MAP_W * MAP_H];       // road laid over a river (drawn as a timber deck)
    u16 waterComp[MAP_W * MAP_H];   // connected body of water a water tile belongs to (0 = not water), see waterSize
    std::vector<int> waterSize;     // tiles of each body of water, indexed by waterComp (the central lake is by far the biggest)
    int lakeComp = 0;               // waterComp id of the central lake (the largest body of water)
    std::vector<StartSpot> starts;   // 4 corner start positions (HQ center tile)
    std::vector<SupplySpot> supplies;
    std::vector<LandmarkSpot> landmarks;   // neutral civilian castles / bunkers / watchtowers, 4 rotated copies of each kind
    std::vector<RoadSeg> roads;
    // Interior water (water with water on all four sides), pre-grouped for the animated-caustics pass: origins of aligned 4x4
    // all-interior blocks (one 128 px draw each) and every interior tile no block covers. Filled by generate().
    std::vector<int> waterBlocks;
    std::vector<int> waterTiles;

    void generate();
    u8 tile(int x, int y) const { return tiles[y * MAP_W + x]; }
    bool passable(int x, int y) const { return inMap(x, y) && blocked[y * MAP_W + x] == 0; }
    bool terrainPassable(int x, int y) const { return inMap(x, y) && (blocked[y * MAP_W + x] & 1) == 0; }
    bool buildable(int x, int y) const {
        if (!inMap(x, y)) return false;
        u8 t = tiles[y * MAP_W + x];
        return blocked[y * MAP_W + x] == 0 && t != T_WATER && t != T_ROCK && t != T_TREE && bridge[y * MAP_W + x] == 0;   // (nothing is built on a bridge deck: it would plug the crossing)
    }
    // Would a w x h structure at (x, y) cut the ground around it in two (a corridor between rock and water, a gap between two other structures)? Counts the
    // 4-connected regions of free ground in the window of a few tiles round the footprint before and after: more of them after means it would plug something.
    bool wouldPlug(int x, int y, int w, int h) const;
    void setStructure(int x, int y, int w, int h, bool on);
    void setResource(int x, int y, bool on);
    float moveCost(int x, int y) const { return tiles[y * MAP_W + x] == T_ROAD ? 0.8f : 1.0f; }

    // Pathfinding: fills out with tile centers (excluding start tile), returns false if unreachable.
    // If the goal is blocked, the nearest reachable tile is used.
    bool findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes = MAP_W * MAP_H) const;
    bool lineClear(Vec2 a, Vec2 b) const;   // straight ground line free of blocked tiles
    Vec2 nearestFree(Vec2 p, int maxR = 6) const;

    // ---- the lake: ships sail on water only. A tile is sailable if it is water with no structure on it (bridges are road, so they close a
    // river to ships); a tile is "open" if it and its four neighbours are sailable, which leaves room for a hull between the shores.
    bool navTile(int x, int y) const { return inMap(x, y) && tiles[y * MAP_W + x] == T_WATER && (blocked[y * MAP_W + x] & ~1) == 0; }
    bool navOpen(int x, int y) const { return navTile(x, y) && navTile(x + 1, y) && navTile(x - 1, y) && navTile(x, y + 1) && navTile(x, y - 1); }
    bool bigWater(int x, int y) const { return inMap(x, y) && waterComp[y * MAP_W + x] == lakeComp && lakeComp != 0 && waterSize[lakeComp] >= NAVAL_WATER_MIN; }   // the lake itself, not a wide stretch of river cut off by a bridge
    Vec2 nearestWater(Vec2 p, int maxR = 12, bool bigOnly = false) const;       // centre of the nearest open water tile (p itself when it already is one); bigOnly: in the lake, not in a pond or a cut-off river
    bool findNavalPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes = 4000) const;   // like findPath for ships; the goal is moved to the nearest open water
    bool navLineClear(Vec2 a, Vec2 b) const;                                     // straight line over open water
};

extern Map g_map;
