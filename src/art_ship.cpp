#include "art2.h"
#include "paint.h"
using namespace paint;

// PLACEHOLDER - replaced by the warship art pass.
ShipMounts shipMounts(bool cruiser) {
    if (cruiser) return ShipMounts{ 2, { 12.0f, -13.0f, 0 }, { 0, 0, 0 } };
    return ShipMounts{ 2, { 15.0f, -15.0f, 0 }, { 0, 0, 0 } };
}

void artShipBody(Canvas& c, Color team, bool cyber, bool cruiser) {
    Art A(c, team);
    Color hull = cyber ? rgb(70, 92, 130) : rgb(98, 108, 64);
    float hw = cruiser ? 9.5f : 8.0f;
    A.poly({ {2, 32 - hw}, {44, 32 - hw}, {62, 32}, {44, 32 + hw}, {2, 32 + hw} }, hull, 2.4f, 0.5f);
    A.box(30, 32, 8, 4, 1, shade(hull, 1.3f), 1.4f, 0.4f);
    ShipMounts m = shipMounts(cruiser);
    for (int i = 0; i < m.n; i++) A.circle(32 + m.x[i], 32 + m.y[i], 5, shade(hull, 0.6f), 1.4f, 0.4f);
    A.dot(26, 32, 2, team);
    c.outline(rgb(8, 10, 14, 200));
}

bool artShipMount(Canvas& c, Color team, bool cyber, bool cruiser) {
    Art A(c, team);
    Color m = cyber ? rgb(150, 168, 190) : rgb(124, 126, 124);
    A.circle(32, 32, 5, m, 1.8f, 0.5f);
    A.cap(32, 32, cruiser ? 46 : 50, 32, 1.6f, shade(m, 0.6f), 1, 0.4f);
    c.outline(rgb(8, 10, 14, 200));
    return true;
}
