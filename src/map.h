// One Hour - the single map: terrain, occupancy, pathfinding
#pragma once
#include "common.h"

enum Tile : u8 { T_GRASS = 0, T_GRASS2, T_DIRT, T_SAND, T_ROAD, T_WATER, T_ROCK, T_TREE, T_COUNT };

struct StartSpot { int tx, ty; };
struct SupplySpot { int tx, ty; int amount; };

struct Map {
    u8 tiles[MAP_W * MAP_H];
    u8 blocked[MAP_W * MAP_H];      // bit0 terrain, bit1 structure, bit2 resource pile
    u8 variant[MAP_W * MAP_H];      // visual variation
    std::vector<StartSpot> starts;   // 4 corner start positions (HQ center tile)
    std::vector<SupplySpot> supplies;

    void generate();
    u8 tile(int x, int y) const { return tiles[y * MAP_W + x]; }
    bool passable(int x, int y) const { return inMap(x, y) && blocked[y * MAP_W + x] == 0; }
    bool terrainPassable(int x, int y) const { return inMap(x, y) && (blocked[y * MAP_W + x] & 1) == 0; }
    bool buildable(int x, int y) const {
        if (!inMap(x, y)) return false;
        u8 t = tiles[y * MAP_W + x];
        return blocked[y * MAP_W + x] == 0 && t != T_WATER && t != T_ROCK && t != T_TREE;
    }
    void setStructure(int x, int y, int w, int h, bool on);
    void setResource(int x, int y, bool on);
    float moveCost(int x, int y) const { return tiles[y * MAP_W + x] == T_ROAD ? 0.8f : 1.0f; }

    // Pathfinding: fills out with tile centers (excluding start tile), returns false if unreachable.
    // If the goal is blocked, the nearest reachable tile is used.
    bool findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes = 6000) const;
    bool lineClear(Vec2 a, Vec2 b) const;   // straight ground line free of blocked tiles
    Vec2 nearestFree(Vec2 p, int maxR = 6) const;
};

extern Map g_map;
