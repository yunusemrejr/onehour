// One Hour - procedural structure art: 2x supersampled, raised 3/4-view blocks with cast shadows, roof props and windows
#pragma once
#include "gfx.h"

static const int BART_K = 2;                              // canvas pixels per design pixel
static const int BART_PAD_L = 10, BART_PAD_T = 26, BART_PAD_R = 18, BART_PAD_B = 18;   // design pixels around the footprint (roofs overhang north, shadows fall south-east)

struct BArtSize { int w, h; float cx, cy; };               // canvas size and the footprint centre in canvas pixels
BArtSize artBuildingSize(int type);

// Paints the structure into 'c' (colour, no ownership) and the white-on-clear team mask into 'mask' (tinted by the owner's colour when drawn).
void artBuilding(Canvas& c, Canvas& mask, int type);
// Construction site: bare plate, stacked materials, perimeter fence and scaffold frame (same canvas size as the building).
void artSite(Canvas& c, int type);
// The warhead on a nuke ramp, same geometry as the structure: drawn live only while the ramp is armed.
void artArmed(Canvas& c, int type);
// Scorched rubble left where a structure fell (footprint w x h tiles).
void artRubble(Canvas& c, int tilesW, int tilesH, u32 seed);
// Small rotating props drawn live over a structure: 0 cyber radar dish, 1 clanker radar dish, 2 pump jack beam, 3 cooling fan.
void artProp(Canvas& c, int which);
// Naval Yards and anti-nuke defences (artb_naval.cpp). Paints the structure and its team mask; false for any other structure type.
bool artNavalBuilding(Canvas& c, Canvas& mask, int type);
// Civilian town buildings and the Underground Nuke silo (artb_civic.cpp). Paints the structure and its team mask; false for any other type.
bool artCivicBuilding(Canvas& c, Canvas& mask, int type);
// The silo's armed overlay: the blast doors slid back and the silver warhead rising from the shaft (drawn live, same geometry as the structure).
void artSiloArmed(Canvas& c, int type);
// The map's special structures (artb_special.cpp): the Island Nuke Launcher in its laser chamber, the Mountain Fortress and the Bastion Wall.
// Paints the structure and its team mask; false for any other type.
bool artSpecialBuilding(Canvas& c, Canvas& mask, int type);
