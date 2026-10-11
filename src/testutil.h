// One Hour - helpers shared by the headless tests
#pragma once
#include "map.h"

// Open ground for a test scene: the spot nearest to 'p' that has a (2*half+1)-tile square around it nothing blocks (no lake, rock, tree, supply pile
// or structure). The middle of the map is a lake, so scenes that used to be built there are built here instead.
inline Vec2 openGroundNear(Vec2 p, int half = 8) {
    Vec2 best = p; float bd = 1e18f;
    for (int ty = half + 1; ty < MAP_H - half - 1; ty++) for (int tx = half + 1; tx < MAP_W - half - 1; tx++) {
        Vec2 c = tileCenter(tx, ty); float d = dist(c, p);
        if (d >= bd) continue;
        bool free = true;
        for (int dy = -half; dy <= half && free; dy++) for (int dx = -half; dx <= half && free; dx++) if (!g_map.passable(tx + dx, ty + dy)) free = false;
        if (free) { bd = d; best = c; }
    }
    return best;
}

// A flat arena for a scene that needs room (an aircraft test flies tens of tiles): every tile within hx / hy tiles of c becomes plain grass, open to units
// (rock, trees and water go; structures and supply piles stay). Call it after Sim::init, which regenerates the map.
inline void flattenArena(Vec2 c, int hx, int hy) {
    int cx = tileOf(c.x), cy = tileOf(c.y);
    for (int y = std::max(1, cy - hy); y <= std::min(MAP_H - 2, cy + hy); y++) for (int x = std::max(1, cx - hx); x <= std::min(MAP_W - 2, cx + hx); x++) {
        int i = y * MAP_W + x;
        g_map.tiles[i] = T_GRASS; g_map.blocked[i] &= ~1; g_map.bridge[i] = 0;
    }
}
