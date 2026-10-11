#include "art2.h"
#include "paint.h"
using namespace paint;

// PLACEHOLDER - replaced by the strike fighter art pass.
void artStrikeJetBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    Color hull = cyber ? rgb(44, 54, 76) : rgb(112, 120, 92);
    A.poly({ {3, 31}, {44, 29}, {63, 32}, {44, 35}, {3, 33} }, hull, 2, 0.5f);
    A.poly({ {26, 32}, {16, 6}, {20, 6}, {34, 29} }, shade(hull, 1.2f), 1.6f, 0.5f);
    A.poly({ {26, 32}, {16, 58}, {20, 58}, {34, 35} }, shade(hull, 1.2f), 1.6f, 0.5f);
    A.dot(26, 32, 2, team);
    c.outline(rgb(8, 10, 14, 200));
}
