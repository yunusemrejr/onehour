// One Hour - procedural unit art: 2x supersampled, lit and anti-aliased (drawn at half size on screen)
#pragma once
#include "gfx.h"

static const int ART_SIZE = 64;   // unit canvases are 64x64 and face +x, centre (32,32)
static const float UNIT_SCALE = 0.65f;   // on-screen size: a little over half the texture, so the extra detail shows

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
// The canvas a unit type is painted on: ART_SIZE, or ART_BIG (twice the detail) for the big ships. The sprite is drawn at UNIT_SCALE * artScale *
// ART_SIZE / artCanvas, so artScale means the same for both.
int artCanvas(int type);
// Paradrop cargo plane (faction look, owner colour) and parachute canopy, both 64x64.
void artCargoPlane(Canvas& c, Color team, bool cyber);
void artChute(Canvas& c, Color team);
// Aid Drop: the white relief plane (64x64, faces +x) and the wooden aid crate, closed (falling) or opened (landed).
void artAidPlane(Canvas& c);
void artCrate(Canvas& c, bool open);
// Relic army special units (art_relic.cpp): 64x64, facing +x, centre (32,32), painted and finished (sharpened, outlined) by the function itself.
// The renderer draws them at UNIT_SCALE * scale. team: the owner's colour (small roundels only).
// kind 0 Juggernaut: a super-heavy tank on double tracks (frame 0/1/2 = track cycle); its turret is separate.
// kind 1 Strider: a four-legged artillery walker (frame 0/1/2 = walk cycle, diagonal leg pairs); its back-mounted rocket box is separate.
// kind 2 Thunderhawk: a heavy attack helicopter; the frame is ignored and the renderer spins the rotor blades over the centre (32,32).
void artRelicUnitBody(Canvas& c, int kind, Color team, int frame);
// The rotating part, 64x64 centred (32,32), pointing +x: kind 0 the Juggernaut's turret with twin long cannons; kind 1 the Strider's rocket box
// (2x4 tubes); kind 2 returns false (no turret).
bool artRelicUnitTurret(Canvas& c, int kind, Color team);
