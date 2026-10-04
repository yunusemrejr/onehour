// One Hour - procedural unit art: 2x supersampled, lit and anti-aliased (drawn at half size on screen)
#pragma once
#include "gfx.h"

static const int ART_SIZE = 64;   // unit canvases are 64x64 and face +x, centre (32,32)
static const float UNIT_SCALE = 0.62f;   // on-screen size: a little over half the texture, so the extra detail shows

// Body sprite. frame: 0 = standing, 1/2 = the two phases of the walk / track cycle.
void artUnitBody(Canvas& c, int type, Color team, int frame);
// Rotating part of a vehicle (turret). Returns false when the unit has none.
bool artUnitTurret(Canvas& c, int type, Color team);
// Rotor blades of a helicopter (one blade pair, spun by the renderer) and the translucent disc.
void artRotor(Canvas& c, bool blades);
// Rotating heads of the defensive structures: 0 laser turret, 1 gun nest, 2 patriot launcher, 3 rocket battery.
void artTurretHead(Canvas& c, int idx);
// On-screen size multiplier of a unit type relative to UNIT_SCALE (the elite vehicles are visibly larger than the line units).
float artScale(int type);
// Paradrop cargo plane (faction look, owner colour) and parachute canopy, both 64x64.
void artCargoPlane(Canvas& c, Color team, bool cyber);
void artChute(Canvas& c, Color team);
