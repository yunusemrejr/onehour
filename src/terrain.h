// One Hour - baked world terrain: the whole map painted once into a single texture at startup
#pragma once
#include "gfx.h"

// Paints the WORLD_W x WORLD_H ground: noise-shaded grass, dirt, sand and asphalt blended along organic borders, a height field that lights
// the cliffs and casts soft shadows from rocks and trees, lakes with depth, foam and wet sand, painted roads, individual trees, boulders and clutter.
void terrainBake(Canvas& world);
// Box-filtered overview of the baked world for the minimap (size x size).
void terrainOverview(const Canvas& world, Canvas& out);
