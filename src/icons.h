// One Hour - command-grid icons: procedural pictograms for the command buttons (nukes, powers, scans, drops, upgrades, orders), and the
// thumbnails the grid shows for structure and unit buttons (shrunk copies of the game's own sprites). Everything is baked once by iconsInit.
#pragma once
#include "gfx.h"

enum IconId : int {
    ICO_NONE = 0,
    ICO_NUKE, ICO_MININUKE, ICO_DEEPNUKE, ICO_ISLANDNUKE, ICO_NUKEBOMBER,
    ICO_EMP, ICO_SHELLSTORM, ICO_SCAN_ORBITAL, ICO_SCAN_RECON,
    ICO_PARADROP, ICO_AID, ICO_AIRSTRIKE,
    ICO_SELL, ICO_RALLY, ICO_ATTACKMOVE, ICO_STOP, ICO_CANCEL, ICO_GUARD, ICO_GATHER, ICO_FORCE, ICO_RESEARCH,
    ICO_UP_RUGGED, ICO_UP_GUNS, ICO_UP_REPAIR, ICO_UP_DRONES, ICO_UP_HYPER, ICO_UP_ANTIROCKET,
    ICO_UNLOAD, ICO_DROPOFF,
    ICO_COUNT
};

// Bakes every icon and thumbnail. Call once after Gfx::init (the thumbnails are shrunk from g.building and g.unitBody).
void iconsInit(Gfx& g);

// An icon centred on (cx, cy), size x size px; a disabled button dims it.
void iconDraw(Gfx& g, int icon, float cx, float cy, float size, bool enabled);
// A structure or unit thumbnail fitted into a size x size square, aspect kept. The unit takes its owner colour slot (slotOf(owner)).
void iconDrawBuilding(Gfx& g, int type, float cx, float cy, float size, bool enabled);
void iconDrawUnit(Gfx& g, int type, int slot, float cx, float cy, float size, bool enabled);
