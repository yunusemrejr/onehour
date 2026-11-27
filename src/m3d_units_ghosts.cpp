// One Hour - 3D models: The Ghosts (a stealth submarine and a stealth jet, both armies alike) and the Ghost2 hunter drone.
// The Ghosts are faceted pitch-black stealth shapes: flat planes meeting at hard creases, no masts, no stores, no clutter, a single dull-red slit
// of light, and the owner's colour only on the tips. The black is METAL, so the facets catch the sky in sharp highlights.
//  * Ghost Submarine: 92 long, a low wide wedge hull (a stealth ship's slab, not a cigar), a faceted spine where a sail would be, an X tail.
//  * Ghost Jet: 40 long, 36 across, a flat-faceted arrowhead with a 50 degree sweep and a V tail.
//  * Ghost2 Drone: 30 across, pale grey: a small blended flying wing with a pale sensor eye under its nose, so it reads apart from the Ghosts.
#include "m3d_parts.h"
#include <cmath>

using namespace m3d;

namespace {

const Color gBlack = C(0x08090B), gFacet = C(0x111216), gCrease = C(0x1C1E24), gSeam = C(0x030303), gEmber = C(0xC0161E);

void ghostSub(Model& b, Model&, int) {
    b.dirt = 0.0f;
    // the hull: a low wedge, wide amidships, faceted to a point at the bow and a blunt stern
    b.plan({ { -40, -11 }, { 18, -14 }, { 46, 0 }, { 18, 14 }, { -40, 11 }, { -46, 0 } }, 0.0f, 2.2f, gBlack).m(METAL).t(T_PANEL, 10, gSeam).bev(0.4f);
    b.taper(-4, 0, 2.2f, 34.0f, 11.0f, 2.2f, 26.0f, 5.0f, gFacet, 2.0f, 0).m(METAL).bev(0.2f);          // the deck's sloped facets
    b.plan({ { 30, -8.5f }, { 46, 0 }, { 30, 8.5f } }, 2.2f, 3.6f, gFacet).m(METAL).bev(0.15f);         // the bow facet
    // the spine where a sail would be: a long faceted ridge, no tower
    b.taper(4, 0, 4.4f, 15.0f, 3.6f, 1.8f, 9.0f, 0.8f, gCrease, 1.0f, 0).m(METAL).bev(0.15f);
    // the X tail
    for (int s = -1; s <= 1; s += 2)
        b.plan({ { -40, s * 8.0f }, { -34, s * 9.0f }, { -40, s * 16.0f }, { -45, s * 15.0f } }, 1.2f, 1.7f, gFacet).m(METAL).bev(0.2f);
    // a dull-red slit along the spine, and the owner's colour on the tips of the tail
    b.box(6, 0, 6.25f, 7.0f, 0.25f, 0.08f, gEmber).m(EMIT).f(F_NOSHADOW);
    for (int s = -1; s <= 1; s += 2) b.plan({ { -40, s * 14.0f }, { -37.6f, s * 14.3f }, { -40, s * 16.0f }, { -42.4f, s * 15.6f } }, 1.7f, 1.85f, OWNER).bev(0.05f);   // (the owner's colour on the tail tips)
}

void ghostJet(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.0f;
    // the arrowhead: the wing planform, then the faceted body on it (a low pyramid ridge), all flat planes
    b.plan({ { 21, 0 }, { -11, 5 }, { -17, 0 }, { -11, -5 } }, 3.4f, 4.4f, gBlack).m(METAL).t(T_PANEL, 7, gSeam).bev(0.2f);
    for (int s = -1; s <= 1; s += 2)   // the swept wings, each one convex panel
        b.plan({ { 17, s * 1.0f }, { -9, s * 18.0f }, { -14, s * 16.0f }, { -11, s * 3.0f } }, 3.4f, 4.2f, gBlack).m(METAL).t(T_PANEL, 7, gSeam).bev(0.2f);
    b.taper(1, 0, 4.4f, 15.0f, 5.0f, 2.6f, 8.0f, 1.2f, gFacet, 3.0f, 0).m(METAL).bev(0.15f);
    b.plan({ { 21, 0 }, { 12, 3.2f }, { 12, -3.2f } }, 4.4f, 5.6f, gFacet).m(METAL).bev(0.1f);            // the nose facet
    b.taper(9, 0, 7.0f, 3.2f, 1.6f, 0.9f, 1.6f, 0.6f, gCrease, 0.5f, 0).m(GLASS).bev(0.1f);             // the faceted canopy
    // the V tail: two canted fins
    for (int s = -1; s <= 1; s += 2)
        b.plan({ { -16, s * 3.0f }, { -8, s * 3.0f }, { -12, s * 3.6f }, { -17, s * 3.6f } }, 4.4f, 10.0f, gFacet).m(METAL).bev(0.15f).rx(s * 35.0f);
    // the exhaust slit (a dull-red glow at the trailing edge) and the owner's wingtips
    b.box(-16.2f, 0, 4.4f, 0.5f, 3.6f, 0.25f, gEmber).m(EMIT).f(F_NOSHADOW);
    for (int s = -1; s <= 1; s += 2) b.plan({ { -8.2f, s * 16.2f }, { -9, s * 18.0f }, { -14, s * 16.0f }, { -12.6f, s * 15.0f } }, 4.2f, 4.35f, OWNER).bev(0.05f);   // (the owner's colour on the wingtips)
}

void ghost2Drone(Model& b, Model&, int) {
    b.ground = false; b.dirt = 0.0f;
    const Color pale = C(0xC9CED6), mid = C(0x8D96A3), dark = C(0x2B3038);
    for (int s = -1; s <= 1; s += 2)
        b.plan({ { 6, s * 0.5f }, { -2, s * 9.0f }, { -6, s * 8.0f }, { -4, s * 0.5f } }, 3.0f, 3.6f, pale).m(PAINT).t(T_PANEL, 5, mid).bev(0.2f);
    b.ball(0, 0, 3.9f, 4.4f, 2.0f, 1.3f, pale).m(PAINT);                                                  // the blended body
    b.ball(5.2f, 0, 3.0f, 1.3f, 1.3f, 1.3f, dark).m(GLASS);                                              // the sensor turret
    b.ball(6.1f, 0, 2.9f, 0.6f, 0.6f, 0.6f, C(0xE8FBFF)).m(EMIT).f(F_NOSHADOW);                         // its pale eye
    b.box(-5.0f, 0, 3.9f, 0.6f, 1.6f, 0.4f, dark).m(PAINT).bev(0.1f);                                    // the pusher housing
    for (int s = -1; s <= 1; s += 2) b.box(-3.6f, s * 8.0f, 3.7f, 1.0f, 0.8f, 0.12f, OWNER).bev(0.05f);   // the owner's wingtips
    b.scale(1.7f);   // (18 across would vanish on screen: 30)
}

}  // namespace

M3D_UNIT(U_C_GHOSTSUB, ghostSub)
M3D_UNIT(U_K_GHOSTSUB, ghostSub)
M3D_UNIT(U_C_GHOSTJET, ghostJet)
M3D_UNIT(U_K_GHOSTJET, ghostJet)
M3D_UNIT(U_C_GHOST2, ghost2Drone)
M3D_UNIT(U_K_GHOST2, ghost2Drone)
