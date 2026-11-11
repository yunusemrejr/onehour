// One Hour - the studio pipeline's lighting pass for painted sprites (see paint::Relief). Painted once at startup: no cost per frame.
#pragma once
#include "paint.h"

struct LightRig {
    float lx = -0.40f, ly = -0.48f, lz = 0.78f;   // toward the key light (the sun), normalised by relight
    float amb = 0.40f;     // sky fill on a flat face
    float key = 0.60f;     // key light on a flat face (amb + key = 1: a flat, open face keeps its albedo)
    float spec = 0.38f, shine = 26.0f;   // specular sheen (bare metal gets all of it, paint a little)
    float ao = 1.0f;       // ambient occlusion in creases and under raised parts
    float shadow = 0.72f;  // how dark the key light's cast self-shadow is
    float reach = 14.0f;   // how far (canvas px) a raised part casts its shadow
    float wear = 1.0f;     // worn, lighter paint on the sharp convex edges; grime in the concave ones
    float zScale = 1.0f;   // heights times this
};
// units turn on the map, so their key light stands high (the shading stays plausible at any heading); structures never turn: a lower sun
LightRig rigUnit();
LightRig rigBuilding();
// Lights the sprite painted on c from its height field r. mask: a team-colour mask painted over the same geometry (white on clear): it gets the
// same diffuse light (no sheen), so the owner's colour is shaded like the paint under it.
void relight(Canvas& c, const paint::Relief& r, const LightRig& L, Canvas* mask = nullptr);

// The studio pipeline around a painter: paintIt() runs with a height field attached to c, then the sprite is lit from it (mask: see relight).
// ONEHOUR_OLDART=1 keeps the old baked-bevel look (for comparisons).
inline bool studioOn() { static const bool on = !getenv("ONEHOUR_OLDART"); return on; }
template <class F> void studioPaint(Canvas& c, const LightRig& rig, Canvas* mask, F paintIt) {
    if (!studioOn()) { paintIt(); return; }
    paint::Relief rf(c);
    paint::Relief* prev = paint::g_relief;
    float prevK = Canvas::outlineK;
    paint::g_relief = &rf; Canvas::outlineK = 0.42f;
    paintIt();
    paint::g_relief = prev; Canvas::outlineK = prevK;
    relight(c, rf, rig, mask);
}
