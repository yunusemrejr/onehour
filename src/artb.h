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
// The Healing Antenna of either army (artb_antenna.cpp): a 1x1 mast with a heal glyph and a green beacon. Paints the structure and its team mask.
bool artHealAntenna(Canvas& c, Canvas& mask, bool cyber, u32 seed);
// The underground bunker of either army (artb_bunker.cpp): its 3x3 surface apron. Paints the structure and its team mask; false for any other type.
// Live parts (the hatch leaves, miniguns, rocket pod, overdrive and laser heads) are drawn over it at the BUNKER_* spots in data.h.
bool artBunker(Canvas& c, Canvas& mask, int type);
// What one per-bunker upgrade (BunkerUpg) adds to that apron, painted alone on a clear canvas of the bunker's size (drawn over the bunker sprite).
void artBunkerUpgrade(Canvas& c, int type, int upg);
// The finishing pass (art_finish.cpp): cavity shading, occlusion, sky light and rim, weathering, emitter bloom and a contact shadow, applied
// to a painted structure canvas before the realism grade. 'overlay' is the lighter variant for a bunker upgrade drawn over the apron (no
// contact shadow, no weathering: the bunker underneath already has both). Changes colour inside the shape and adds alpha only outside it.
void finishBuilding(Canvas& c, int type, u32 seed, bool overlay = false);

// Relic (abandoned, overgrown) structures of the third army (artb_relic.cpp). kind: 0 Command (4x4 tiles), 1 Reactor (3x3), 2 Foundry/war
// factory (5x4), 3 Airstrip: VTOL hangar + round gunship pad (5x4), 4 Flak battery base (2x2), 5 Laser tower base (2x2), 6 Heavy cannon
// emplacement base (3x3), 7 Wall segment (1x1), 8 Missile silo (3x3), 9 Secret bunker (3x3). w, h: the footprint in tiles, and the canvas is
// the size artBuildingSize would give for it. Paints the colour into c and the white-on-clear team mask into mask.
bool artRelicBuilding(Canvas& c, Canvas& mask, int kind, int w, int h);
// The silo's armed overlay (same canvas as kind 8): armoured doors slid back, the MIRV nose visible in the shaft. Drawn live when ready.
void artRelicSiloArmed(Canvas& c, int w, int h);
// Rotating heads drawn live over the 2x2/3x3 relic bases (centred on the footprint centre): 0 flak launcher, 1 laser emitter, 2 heavy cannon.
// 64x64 canvas centred (32,32), pointing toward +x, like artTurretHead.
void artRelicTurretHead(Canvas& c, int which);
// The river bridge: a 3 x 12 tile deck running north-south, canvas 224 x 800 (BART_K = 2, 8 design px of margin). state 0 intact, 1 broken
// (the middle span blown out), 2 under repair (the gap bridged by scaffolding and temporary girders).
void artBridge(Canvas& c, int state);
