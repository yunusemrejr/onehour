// One Hour - 3D model: the Bunker from Hell (both armies build the same one). A faceted stealth mound in glossy pitch-black armour: a chamfered
// plinth, a glacis of two crossed sloped blocks (eight facets with hard creases, like a stealth hull), the deck with the hatch collar and the two
// minigun bays at the normal bunker's spots (the live parts are drawn over them, see m3d_build_bunker.cpp), two ballistic rocket batteries on
// the north and south shoulders, and thin dull-red light in the seams: vent slits round the plinth, the hatch ring, the battery muzzles.
// The black is METAL, so the faces mirror the sky: near-black planes with sharp highlights on the creases.
#include "m3d_parts.h"
#include <cmath>

using namespace m3d;

namespace {

const float ZD = 8.6f;   // the deck top, as the normal bunker's
inline float gy(float sy, float zt) { return sy + OBL_K * zt; }
Prim& deckBox(Model& m, float sx, float sy, float hx, float hy, float zt, float h, Color c) { return m.box(sx, gy(sy, zt), zt - h * 0.5f, hx, hy, h * 0.5f, c); }

void hellBunker(Model& m, int) {
    m.dirt = 0.05f;
    const Color black = C(0x08090B), facet = C(0x101114), crease = C(0x17181C), seam = C(0x030303);
    const Color ember = C(0xC0161E), emberHot = C(0xFF3A2A), basalt = C(0x24252A);

    // the footing: dark basalt slabs set into the ground
    m.slab(48, 54, 47, 47, 0, 0.9f, basalt).m(MATTE).t(T_PAD, 16).bev(0.4f).seed(66.0f);

    // the plinth: an eight-pointed star in plan (a chamfered block and a diamond through it), plated, with red vent slits along the faces
    m.plan({ { 10, 8 }, { 86, 8 }, { 94, 16 }, { 94, 92 }, { 86, 100 }, { 10, 100 }, { 2, 92 }, { 2, 16 } }, 0.9f, 3.4f, black).m(METAL).t(T_PANEL, 8, seam).bev(0.5f);
    m.plan({ { 48, -8.0f }, { 110.0f, 54 }, { 48, 116.0f }, { -14.0f, 54 } }, 0.9f, 3.2f, facet).m(METAL).t(T_PANEL, 8, seam).bev(0.5f);
    for (int i = 0; i < 6; i++) {
        const float t = 18 + i * 12.0f;
        m.box(t, 7.85f, 2.1f, 2.6f, 0.2f, 0.35f, ember).m(EMIT).f(F_NOSHADOW);    // north face
        m.box(t, 100.15f, 2.1f, 2.6f, 0.2f, 0.35f, ember).m(EMIT).f(F_NOSHADOW);  // south face
        m.box(1.85f, t + 6, 2.1f, 0.2f, 2.6f, 0.35f, ember).m(EMIT).f(F_NOSHADOW);  // west face
        m.box(94.15f, t + 6, 2.1f, 0.2f, 2.6f, 0.35f, ember).m(EMIT).f(F_NOSHADOW); // east face
    }

    // the glacis: a square sloped block and the same turned 45 degrees, so the slope is eight facets meeting at hard creases
    m.taper(48, 54, 3.4f, 40.0f, 40.0f, 3.6f, 35.0f, 35.0f, facet).m(METAL).t(T_PANEL, 9, seam).bev(0.25f);
    m.taper(48, 54, 3.2f, 42.0f, 42.0f, 4.4f, 30.0f, 30.0f, black).m(METAL).bev(0.25f).rz(45);
    // the revetment up to the deck
    m.taper(48, 54, 7.0f, 33.0f, 33.0f, 1.6f, 30.0f, 30.0f, C(0x030304)).m(PAINT).t(T_PANEL, 10, seam).bev(0.3f);

    // the deck: the hatch collar with a red ring inside it (the hatch leaves are drawn live over it)
    m.cyl('z', 48, gy(48, 9.8f), 9.2f, 27.5f, 0.6f, crease).m(METAL).bev(0.3f);   // (the collar: the one bright steel ring on the deck)
    m.cyl('z', 48, gy(48, 9.8f), 9.85f, 23.0f, 0.08f, ember).m(EMIT);
    m.cyl('z', 48, gy(48, 9.8f), 9.95f, 21.5f, 0.1f, seam).bev(0.1f);
    // the minigun bays: a black frame with a pocket, as on the normal bunker
    for (float sx : { 20.0f, 76.0f }) {
        deckBox(m, sx, 48, 8.2f, 8.2f, ZD + 0.4f, 0.4f, crease).m(METAL).bev(0.3f);
        deckBox(m, sx, 48, 6.4f, 6.4f, ZD + 0.8f, 0.4f, seam).bev(0.2f);
    }

    // the two ballistic rocket batteries on the north and south shoulders: a sloped launcher box, four tubes with red-lit muzzles, raised to the sky
    for (int k = 0; k < 2; k++) {
        const float bx = k ? 30.0f : 66.0f, by = k ? 88.0f : 18.0f;
        m.taper(bx, by, 3.4f, 8.5f, 6.5f, 2.4f, 7.0f, 5.0f, crease).m(METAL).bev(0.3f);        // the mount
        m.box(bx - 1.0f, by, 9.2f, 7.0f, 4.6f, 2.6f, facet).m(METAL).t(T_PANEL, 3.5f, seam).bev(0.35f).ry(-28);   // the launcher box
        for (int t = 0; t < 4; t++) {
            const float ty = by - 2.4f + (t & 1) * 4.8f, tz = 9.2f + (t < 2 ? 1.1f : -1.1f);
            m.ball(bx + 5.2f, ty, tz + 3.0f, 0.8f, 1.15f, 1.15f, t == 1 ? emberHot : ember).m(EMIT).f(F_NOSHADOW);
        }
    }

    // the mast: a black faceted spike at the north-west with a red light on top, and the owner's colour as a thin band round it
    m.taper(14, 22, 3.4f, 3.2f, 3.2f, 20.0f, 0.6f, 0.6f, facet).m(METAL).bev(0.15f).rz(45);
    m.box(14, 22, 12.0f, 2.3f, 2.3f, 0.5f, OWNER).bev(0.1f).rz(45);
    m.ball(14, 22, 23.8f, 0.9f, 0.9f, 0.9f, emberHot).m(EMIT);
    // red light along the deck's front rim, under the hatch collar
    m.box(48, gy(82.6f, ZD), ZD + 0.05f, 22.0f, 0.3f, 0.12f, ember).m(EMIT).f(F_NOSHADOW);
}

}  // namespace

M3D_BUILD(B_C_HELLBUNKER, hellBunker)
M3D_BUILD(B_K_HELLBUNKER, hellBunker)
