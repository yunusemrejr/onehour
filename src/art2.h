// One Hour - unit art for the jeeps, the warships of the lake, the Hotline Strike fighters and the protector drones.
// Same conventions as art.cpp: 64x64 canvas, the craft faces +x, centre (32,32), painted with paint::Art (paint.h) at scale 1 over the
// canvas, lit from the north-west. The renderer shows these at UNIT_SCALE * artScale(type) of the canvas size.
#pragma once
#include "gfx.h"

// Jeep (both armies share the hull and stats, only the look differs): an open-top 4x4 seen from above with seven seats. frame 0 stands, 1 and 2 are
// the two phases of the wheel / track cycle. Draw the seven seats EXACTLY at JEEP_SEAT_X/Y (canvas px): the renderer puts a helmet on every
// seat that has a soldier in it.
// (the hull is about 50 canvas px long and 28 wide: bonnet and windscreen at the front, +x; a helmet is drawn about 2.8 px across)
static const float JEEP_SEAT_X[7] = { 40.0f, 40.0f, 29.0f, 29.0f, 29.0f, 18.0f, 18.0f };
static const float JEEP_SEAT_Y[7] = { 26.0f, 38.0f, 24.5f, 32.0f, 39.5f, 27.0f, 37.0f };
void artJeepBody(Canvas& c, Color team, bool cyber, int frame);

// Protector drone (jeep and ship screens): a small quad drone seen from above, centred at (32,32), about 22 canvas px across, no shadow.
// The rotors are NOT drawn (the renderer spins a blur over them); draw the four rotor housings.
void artGuardDrone(Canvas& c, Color team, bool cyber);

// Warships. The hull lies along x (bow at +x); the hull + superstructure sprite carries no moving parts. The turret / launcher mounts are one
// separate sprite, centred at (32,32) and rotated about its own centre, that the renderer draws once at every ShipMounts position
// (offsets from the hull centre in canvas px, +x towards the bow). The hull must show an empty ring/pedestal at each mount position.
struct ShipMounts { int n; float x[3], y[3]; };
ShipMounts shipMounts(bool cruiser);
void artShipBody(Canvas& c, Color team, bool cyber, bool cruiser);
bool artShipMount(Canvas& c, Color team, bool cyber, bool cruiser);

// The special fighter of a Hotline Strike (Valkyrie / Warhawk): long, slender, swept, twin-tailed, weapon pylons under the wings. 64x64 canvas
// with a wingspan up to about 52 px and a length up to about 62 px. Afterburner flame and contrails are added live by the renderer.
void artStrikeJetBody(Canvas& c, Color team, bool cyber);

// The Supersonic Bomber (Specter / Ironwing): a small, very fast fixed-wing nuclear bomber, 64x64 canvas, nose +x, about 58 px long,
// carrying ONE compact silver bomb (with the silver warhead's gold band, much smaller than a silo's) under an inboard wing; the bomb
// is painted into the sprite. Afterburner flame and contrails are added live by the renderer.
void artSupersonicBomberBody(Canvas& c, Color team, bool cyber);
