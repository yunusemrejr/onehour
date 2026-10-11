#include "art2.h"
#include "paint.h"
using namespace paint;

// PLACEHOLDER - replaced by the jeep art pass.
void artJeepBody(Canvas& c, Color team, bool cyber, int frame) {
    Art A(c, team);
    Color hull = cyber ? rgb(70, 92, 130) : rgb(98, 108, 64);
    A.box(32, 32, 19, 11, 3, hull, 2.2f, 0.5f);
    for (int i = 0; i < 7; i++) A.circle(JEEP_SEAT_X[i], JEEP_SEAT_Y[i], 2.4f, shade(hull, 0.6f), 1, 0.3f);
    A.circle(24 + frame, 32, 1.5f, team);
    c.outline(rgb(8, 10, 14, 200));
}

void artGuardDrone(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    Color body = cyber ? rgb(40, 52, 78) : rgb(90, 96, 70);
    A.circle(32, 32, 4, body, 2, 0.5f);
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) A.circle(32 + sx * 8.0f, 32 + sy * 8.0f, 3.2f, shade(body, 1.3f), 1.2f, 0.4f);
    A.dot(32, 32, 1.4f, team);
    c.outline(rgb(8, 10, 14, 200));
}
