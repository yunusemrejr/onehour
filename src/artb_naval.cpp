#include "bld.h"

// PLACEHOLDER - replaced by the naval yard / anti-nuke structure art pass.
// Paints the Naval Yards and the anti-nuke defences. Returns false for any other structure.
bool artNavalBuilding(Canvas& c, Canvas& mask, int type) {
    const BuildType& bt = BUILDS[type];
    if (bt.role != BR_NAVAL && bt.role != BR_ANTINUKE) return false;
    bool cy = bt.faction == F_CYBER;
    Bld b(c, mask, bt.w * TILE, bt.h * TILE, cy, 7000u + (u32)type * 31u);
    b.pad(cy ? C_PAD : K_PAD, cy ? C_SEAM : K_SEAM);
    Roof r = b.block(10, 12, b.W - 20.0f, b.H - 24.0f, 10, cy ? C_ROOF : K_ROOF, cy ? C_HULL : K_OLIVE_D);
    b.mark(r.x + 3, r.y + r.h - 5, 12, 3);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}
