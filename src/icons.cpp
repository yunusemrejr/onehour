// One Hour - command-grid icons. Each icon is a short list of pieces (circles, capsules, boxes, convex polygons, rings) painted with the
// paint::Art signed-distance shapes on a 48x48 canvas. Pass one paints every piece's dark outline, pass two the fills, so each silhouette
// keeps a crisp edge on any button colour and still reads at 18-24 px. Coordinates are centred on the icon (0,0 is the middle of the canvas).
#include "icons.h"
#include "paint.h"
#include <cmath>
#include <algorithm>

namespace {

const int CANVAS = 48;                     // icon canvas (design units = canvas pixels)
const float OUTLINE = 2.0f;                // dark outline width (1 px once an icon is shown at half size)
const float BEVEL = 2.0f;                  // lit edge width of the fills
const int THUMB = 48;                      // structure and unit thumbnails are fitted into this many pixels
const Color INK = rgb(12, 14, 20);         // outline and dark details
const Color DIM = rgb(110, 114, 122);      // a disabled button's icon colour (multiplied into the sprite)

// the palette: the game's own HUD colours plus the signal colours of each weapon
const Color RED = rgb(206, 46, 34), HOT = rgb(232, 64, 52), ORANGE = rgb(255, 186, 58), AMBER = rgb(250, 200, 80);
const Color SILVER = rgb(228, 234, 242), SLATE = rgb(62, 72, 90), GREY = rgb(160, 170, 184), STEEL = rgb(196, 206, 218);
const Color CYAN = rgb(110, 230, 255), SHELL = rgb(246, 150, 52), SHELL_NOSE = rgb(232, 84, 40), BRASS = rgb(120, 74, 40);
const Color LIME = rgb(120, 240, 150), GREEN_DK = rgb(30, 78, 54), GREEN = rgb(96, 230, 130), GOLD = rgb(242, 196, 60);
const Color WOOD = rgb(178, 126, 70), WHITE = rgb(240, 244, 248), PALE = rgb(226, 236, 246);

enum Kind : u8 { K_CIRCLE, K_SEG, K_BOX, K_POLY, K_RING, K_GLOW };

struct Piece {
    Kind k = K_CIRCLE;
    float a[5] = { 0, 0, 0, 0, 0 };      // circle: x y r; seg: ax ay bx by r; box: x y hw hh radius; ring: x y r thickness; glow: x y r; poly: a[0] = winding
    std::vector<paint::P> v;             // poly: convex outline in canvas pixels
    Color col = INK;
    float kk = 0.5f;                     // lighting strength of the fill (0 = flat)
    float alpha = 1;                     // coverage
    bool outline = true;                 // decorative pieces (rings, dashes, marks, glints) skip the dark outline
};

// An icon under construction. frame() places the following pieces: local (x, y) maps to canvas (24 + ox + x * s, 24 + oy + y * s).
struct Ico {
    std::vector<Piece> p;
    float ox = 0, oy = 0, s = 1;
    void frame(float x, float y, float sc) { ox = x; oy = y; s = sc; }
    paint::P T(float x, float y) const { return paint::P{ 24 + ox + x * s, 24 + oy + y * s }; }
    Piece& add(Kind k, Color c, float kk) { p.push_back(Piece{}); Piece& q = p.back(); q.k = k; q.col = c; q.kk = kk; return q; }
    Ico& circle(float x, float y, float r, Color c, float kk = 0.5f) {
        paint::P o = T(x, y); Piece& q = add(K_CIRCLE, c, kk); q.a[0] = o.x; q.a[1] = o.y; q.a[2] = r * s; return *this;
    }
    Ico& glow(float x, float y, float r, Color c) {
        paint::P o = T(x, y); Piece& q = add(K_GLOW, c, 0); q.a[0] = o.x; q.a[1] = o.y; q.a[2] = r * s; q.outline = false; return *this;
    }
    Ico& seg(float ax, float ay, float bx, float by, float r, Color c, float kk = 0.5f) {
        paint::P a = T(ax, ay), b = T(bx, by); Piece& q = add(K_SEG, c, kk);
        q.a[0] = a.x; q.a[1] = a.y; q.a[2] = b.x; q.a[3] = b.y; q.a[4] = r * s; return *this;
    }
    Ico& box(float x, float y, float hw, float hh, float rad, Color c, float kk = 0.5f) {
        paint::P o = T(x, y); Piece& q = add(K_BOX, c, kk);
        q.a[0] = o.x; q.a[1] = o.y; q.a[2] = hw * s; q.a[3] = hh * s; q.a[4] = std::min(rad, std::min(hw, hh)) * s; return *this;
    }
    Ico& ring(float x, float y, float r, float thick, Color c, float kk = 0.3f) {
        paint::P o = T(x, y); Piece& q = add(K_RING, c, kk); q.a[0] = o.x; q.a[1] = o.y; q.a[2] = r * s; q.a[3] = thick * s; return *this;
    }
    // convex polygon in icon coordinates
    Ico& poly(const std::vector<paint::P>& v, Color c, float kk = 0.5f) {
        Piece& q = add(K_POLY, c, kk);
        float area = 0;
        for (auto& pt : v) q.v.push_back(T(pt.x, pt.y));
        for (size_t i = 0; i < q.v.size(); i++) { const paint::P& a = q.v[i]; const paint::P& b = q.v[(i + 1) % q.v.size()]; area += a.x * b.y - b.x * a.y; }
        q.a[0] = area > 0 ? 1.0f : -1.0f;
        return *this;
    }
    Ico& deco() { p.back().outline = false; return *this; }
    Ico& alpha(float a) { p.back().alpha = a; return *this; }
};

// points on an ellipse (convex), from angle a0 for n steps around
std::vector<paint::P> ellipse(float cx, float cy, float rx, float ry, int n = 16) {
    std::vector<paint::P> v;
    for (int i = 0; i < n; i++) { float t = i * 6.2831853f / n; v.push_back({ cx + std::cos(t) * rx, cy + std::sin(t) * ry }); }
    return v;
}
// regular polygon around (cx, cy), first vertex at angle rotDeg
std::vector<paint::P> regular(float cx, float cy, float r, int n, float rotDeg) {
    std::vector<paint::P> v;
    for (int i = 0; i < n; i++) { float t = (rotDeg + i * 360.0f / n) * 3.14159265f / 180.0f; v.push_back({ cx + std::cos(t) * r, cy + std::sin(t) * r }); }
    return v;
}
// the radiation trefoil: three 60-degree blades around (cx, cy), from radius r0 to R, plus the centre dot
void trefoil(Ico& I, float cx, float cy, float R, float r0, Color c) {
    for (int k = 0; k < 3; k++) {
        float base = -90.0f + 120.0f * k;
        std::vector<paint::P> v;
        auto pt = [&](float r, float deg) { float t = deg * 3.14159265f / 180.0f; return paint::P{ cx + std::cos(t) * r, cy + std::sin(t) * r }; };
        v.push_back(pt(r0, base - 30));
        for (int j = 0; j <= 4; j++) v.push_back(pt(R, base - 30 + 15.0f * j));
        v.push_back(pt(r0, base + 30));
        I.poly(v, c, 0.25f);
    }
}
// a side-on-top aircraft with its nose along +x, centred on the icon's local origin, scaled by the frame
void plane(Ico& I, Color c, float kk) {
    I.seg(-16, 0, 16, 0, 3.2f, c, kk);
    I.poly({ { -2, -1 }, { 5, -1 }, { -6, -18 }, { -10, -18 } }, c, kk);
    I.poly({ { -2, 1 }, { 5, 1 }, { -6, 18 }, { -10, 18 } }, c, kk);
    I.poly({ { -15, -1 }, { -11, -1 }, { -17, -9 }, { -20, -9 } }, c, kk);
    I.poly({ { -15, 1 }, { -11, 1 }, { -17, 9 }, { -20, 9 } }, c, kk);
}
// a small jet diving nose-first (+y), centred on its local origin
void jet(Ico& I, Color c) {
    I.seg(0, -11, 0, 10, 2.1f, c);
    I.poly({ { 0, -3 }, { 10, 4 }, { 10, 6 }, { 0, 2 } }, c);
    I.poly({ { 0, -3 }, { -10, 4 }, { -10, 6 }, { 0, 2 } }, c);
    I.poly({ { 0, -9 }, { 4.5f, -12.5f }, { 4.5f, -13.5f }, { 0, -11 } }, c);
    I.poly({ { 0, -9 }, { -4.5f, -12.5f }, { -4.5f, -13.5f }, { 0, -11 } }, c);
}
// a dashed circle of gold or green ticks
void dashes(Ico& I, float r, Color c) {
    for (int i = 0; i < 12; i++) {
        float a0 = i * 30.0f * 3.14159265f / 180.0f, a1 = a0 + 18.0f * 3.14159265f / 180.0f;
        I.seg(std::cos(a0) * r, std::sin(a0) * r, std::cos(a1) * r, std::sin(a1) * r, 0.9f, c, 0.2f).deco();
    }
}
// a 155 mm-style shell: body, nose and a brass band, centred on x
void shell(Ico& I, float x) {
    I.box(x, 7, 3.6f, 10, 1.2f, SHELL, 0.45f);
    I.poly({ { x - 3.6f, -3 }, { x + 3.6f, -3 }, { x, -13 } }, SHELL_NOSE, 0.4f);
    I.box(x, 15.5f, 4.2f, 2.4f, 0.8f, BRASS, 0.3f);
}

void buildIcon(int id, Ico& I) {
    I.frame(0, 0, 1);
    switch (id) {
    case ICO_NUKE:
        I.circle(0, 0, 20, RED, 0.45f);
        trefoil(I, 0, 0, 16, 4.5f, ORANGE);
        I.circle(0, 0, 3.2f, ORANGE);
        break;
    case ICO_MININUKE:   // the bunker's warhead: a smaller, orange drum
        I.circle(0, 1, 15, rgb(255, 132, 36), 0.45f);
        trefoil(I, 0, 1, 11, 3.2f, rgb(104, 42, 14));
        I.circle(0, 1, 2.2f, rgb(104, 42, 14));
        break;
    case ICO_DEEPNUKE:   // silver casing of the buried warhead
        I.circle(0, 0, 20, SILVER, 0.5f);
        I.ring(0, 0, 16.5f, 1.2f, GREY).deco();
        trefoil(I, 0, 0, 13.5f, 3.8f, SLATE);
        I.circle(0, 0, 2.6f, SLATE);
        break;
    case ICO_ISLANDNUKE:   // island launcher: a red warhead over a green island in blue water
        I.poly(ellipse(0, 13, 20, 8), rgb(52, 120, 200), 0.3f);
        I.poly(ellipse(0, 12, 9, 3.5f), rgb(110, 176, 96), 0.3f);
        I.circle(0, -5, 11, RED, 0.45f);
        trefoil(I, 0, -5, 7.5f, 2.2f, ORANGE);
        break;
    case ICO_NUKEBOMBER:   // strategic bomber dropping a warhead
        plane(I, STEEL, 0.4f);
        I.circle(0, 9, 6.5f, RED, 0.45f);
        I.poly({ { -5, 12 }, { 5, 12 }, { 0, 20 } }, GREY, 0.3f);
        trefoil(I, 0, 9, 4.2f, 1.3f, ORANGE);
        break;
    case ICO_EMP:   // Cyber's electric strike: a cyan bolt in a glow
        I.glow(0, 0, 22, paint::rgba(CYAN, 120));
        I.poly({ { 10, -21 }, { -15, 4 }, { 6, 1 } }, CYAN, 0.35f);
        I.poly({ { -6, -3 }, { 16, -4 }, { -4, 22 } }, CYAN, 0.35f);
        break;
    case ICO_SHELLSTORM:   // Clanker's barrage: three artillery shells
        shell(I, -12);
        shell(I, 12);
        shell(I, 0);
        break;
    case ICO_SCAN_ORBITAL:   // Orbital Scan: a green radar sweep
        I.circle(0, 0, 20, GREEN_DK, 0.25f);
        I.ring(0, 0, 14, 1.3f, LIME).deco();
        I.ring(0, 0, 7, 1.2f, LIME).deco();
        {
            std::vector<paint::P> fan{ { 0, 0 } };
            for (int i = 0; i <= 4; i++) { float t = (-95.0f + i * 15.0f) * 3.14159265f / 180.0f; fan.push_back({ std::cos(t) * 19, std::sin(t) * 19 }); }
            I.poly(fan, rgb(130, 255, 160), 0.0f).deco().alpha(0.55f);
        }
        I.circle(9, -9, 2.2f, rgb(255, 250, 170)).deco();
        I.circle(0, 0, 2.6f, rgb(214, 255, 222));
        break;
    case ICO_SCAN_RECON:   // Recon Flight: a spotter plane with its sonar ring
        I.ring(0, 2, 17.5f, 1.4f, LIME).deco().alpha(0.7f);
        plane(I, LIME, 0.5f);
        break;
    case ICO_PARADROP: {   // a crate on a parachute
        std::vector<paint::P> canopy;
        for (int i = 0; i <= 8; i++) { float t = (180.0f + i * 22.5f) * 3.14159265f / 180.0f; canopy.push_back({ std::cos(t) * 16, -5 + std::sin(t) * 16 }); }
        I.poly(canopy, rgb(112, 198, 255), 0.4f);
        for (float deg : { 225.0f, 270.0f, 315.0f }) {
            float t = deg * 3.14159265f / 180.0f;
            I.seg(0, -5, std::cos(t) * 16, -5 + std::sin(t) * 16, 0.9f, WHITE, 0.0f).deco();
        }
        I.seg(-14.8f, -11.1f, -7, 9, 0.6f, PALE, 0.0f).deco();
        I.seg(14.8f, -11.1f, 7, 9, 0.6f, PALE, 0.0f).deco();
        I.seg(0, -5, 0, 10, 0.6f, PALE, 0.0f).deco();
        I.box(0, 15, 7, 5, 1.2f, WOOD, 0.4f);
        I.seg(-7, 15, 7, 15, 0.9f, rgb(104, 66, 32), 0.0f).deco();
        break;
    }
    case ICO_AID:   // relief plane dropping a crate of money
        I.frame(0, -9, 0.62f);
        plane(I, WHITE, 0.35f);
        I.frame(0, 0, 1);
        I.seg(0, 3, 0, 9, 0.7f, PALE, 0.0f).deco();
        I.box(0, 15, 7, 5, 1.2f, GOLD, 0.4f);
        I.seg(-7, 15, 7, 15, 0.9f, rgb(150, 100, 20), 0.0f).deco();
        break;
    case ICO_AIRSTRIKE:   // Hotline Strike: jets diving on a reticle
        I.frame(-10, -9, 0.8f); jet(I, STEEL); I.frame(0, 0, 1);
        I.frame(9, -13, 0.8f); jet(I, STEEL); I.frame(0, 0, 1);
        I.ring(5, 7, 10.5f, 2.2f, HOT);
        I.seg(5, -5.5f, 5, -8.5f, 1.2f, HOT).deco();
        I.seg(5, 19.5f, 5, 22.5f, 1.2f, HOT).deco();
        I.seg(-7.5f, 7, -10.5f, 7, 1.2f, HOT).deco();
        I.seg(17.5f, 7, 20.5f, 7, 1.2f, HOT).deco();
        I.circle(5, 7, 2.2f, HOT);
        break;
    case ICO_FLEET: {   // Fleet Call: a warship in profile coming in over the swell, a second one behind it still in the haze
        const Color SEA = rgb(60, 150, 214), HULL = rgb(150, 160, 174);
        I.poly({ { -21, -6 }, { -3, -6 }, { -6, -1 }, { -19, -1 } }, rgb(176, 196, 214), 0.2f).alpha(0.55f).deco();   // the far hull, half haze
        I.box(-13, -9, 3.5f, 2.6f, 0.6f, rgb(176, 196, 214), 0.2f).alpha(0.55f).deco();
        I.poly({ { -15, 3 }, { 21, 3 }, { 15, 12 }, { -12, 12 } }, HULL, 0.45f);    // the near hull: a raked bow on the right
        I.box(0, -1, 6.5f, 4.0f, 1.0f, STEEL, 0.4f);                                // the superstructure
        I.box(-1, -7, 3.0f, 2.2f, 0.6f, STEEL, 0.4f);                               // the bridge
        I.seg(-1, -9, -1, -17, 0.9f, GREY, 0.2f);                                   // the mast
        I.seg(-4, -14, 2, -14, 0.7f, GREY, 0.2f).deco();
        I.seg(9, 1, 17, -2, 1.3f, rgb(88, 96, 108), 0.2f);                           // the forward gun
        I.circle(9, 2, 2.4f, rgb(110, 118, 130), 0.3f);
        I.seg(-10, 7, 12, 7, 0.7f, rgb(96, 106, 120), 0.0f).deco();                 // a line of portholes / the boot top
        for (int k = 0; k < 3; k++) { float y = 15.5f + k * 3.6f, x0 = -20.0f + k * 3.0f; I.seg(x0, y, x0 + 10, y - 1.2f, 1.1f, SEA, 0.1f).deco(); I.seg(x0 + 13, y, x0 + 23, y - 1.2f, 1.1f, SEA, 0.1f).deco(); }   // the swell
        I.seg(21, 12, 25, 9, 1.0f, WHITE, 0.0f).deco();                              // the bow wave
        break;
    }
    case ICO_SELL:   // a gold coin with a dollar sign
        I.circle(0, 0, 20, GOLD, 0.5f);
        I.ring(0, 0, 15.5f, 1.6f, rgb(176, 122, 18)).deco();
        I.seg(6, -6.5f, -5, -6.5f, 1.5f, rgb(120, 78, 12), 0).deco();
        I.seg(-5, -6.5f, -5, 0, 1.5f, rgb(120, 78, 12), 0).deco();
        I.seg(-5, 0, 5, 0, 1.5f, rgb(120, 78, 12), 0).deco();
        I.seg(5, 0, 5, 6.5f, 1.5f, rgb(120, 78, 12), 0).deco();
        I.seg(5, 6.5f, -5, 6.5f, 1.5f, rgb(120, 78, 12), 0).deco();
        I.seg(0, -11, 0, 11, 1.0f, rgb(120, 78, 12), 0).deco();
        break;
    case ICO_RALLY:   // a flag on a pole
        I.seg(-11, -18, -11, 19, 1.6f, GREY, 0.4f);
        I.poly({ { -10, -17 }, { 16, -9 }, { -10, 0 } }, rgb(238, 98, 58), 0.4f);
        I.circle(-11, 20, 2.4f, GREY, 0.4f);
        break;
    case ICO_ATTACKMOVE:   // crossed swords
        I.seg(16, -16, 9, -9, 2.0f, BRASS, 0.2f);                        // grip of the mirrored sword
        I.seg(5.8f, -12.2f, 12.2f, -5.8f, 1.6f, GOLD, 0.3f);              // its guard
        I.seg(9, -9, -16, 16, 2.2f, STEEL, 0.35f);                        // its blade
        I.seg(-16, -16, -9, -9, 2.0f, BRASS, 0.2f);                       // grip
        I.seg(-5.8f, -12.2f, -12.2f, -5.8f, 1.6f, GOLD, 0.3f);            // guard
        I.seg(-9, -9, 16, 16, 2.2f, STEEL, 0.35f);                        // blade
        break;
    case ICO_STOP:   // red octagon with a white bar
        I.poly(regular(0, 0, 20, 8, 22.5f), HOT, 0.4f);
        I.box(0, 0, 11, 3, 1.2f, WHITE, 0.2f);
        break;
    case ICO_CANCEL:   // red cross
        I.seg(-13, -13, 13, 13, 3.6f, HOT, 0.3f);
        I.seg(13, -13, -13, 13, 3.6f, HOT, 0.3f);
        break;
    case ICO_GUARD:   // a shield inside a dashed circle
        dashes(I, 20, GOLD);
        I.box(0, -7, 8.5f, 7, 2.5f, rgb(150, 196, 240), 0.45f);
        I.poly({ { -8.5f, -7 }, { 8.5f, -7 }, { 0, 11 } }, rgb(150, 196, 240), 0.45f);
        I.seg(0, -13, 0, 6, 0.9f, rgb(70, 110, 160), 0).deco();
        break;
    case ICO_GATHER:   // haulers collecting: a supply crate inside a dashed circle
        dashes(I, 20, LIME);
        I.box(0, 0, 9, 8, 1.5f, WOOD, 0.4f);
        I.seg(-9, 0, 9, 0, 1.0f, rgb(110, 74, 36), 0).deco();
        I.seg(0, -8, 0, 8, 1.0f, rgb(110, 74, 36), 0).deco();
        break;
    case ICO_FORCE:   // a red crosshair
        I.ring(0, 0, 13, 3.0f, HOT);
        I.seg(0, -20, 0, -12, 1.6f, HOT, 0.2f);
        I.seg(0, 12, 0, 20, 1.6f, HOT, 0.2f);
        I.seg(-20, 0, -12, 0, 1.6f, HOT, 0.2f);
        I.seg(12, 0, 20, 0, 1.6f, HOT, 0.2f);
        I.circle(0, 0, 3, HOT);
        break;
    case ICO_RESEARCH:   // a lab flask with a green brew
        I.box(0, -12, 3.4f, 9, 1.0f, GREY, 0.4f);
        I.poly({ { -6, -4 }, { 6, -4 }, { 16, 15 }, { -16, 15 } }, PALE, 0.35f);
        I.poly({ { -10.5f, 6 }, { 10.5f, 6 }, { 16, 15 }, { -16, 15 } }, rgb(110, 236, 140), 0.25f);
        I.circle(-4, 11, 2.0f, rgb(230, 255, 235), 0).deco();
        I.circle(4, 9.5f, 1.4f, rgb(230, 255, 235), 0).deco();
        break;
    case ICO_UP_RUGGED:   // a chamfered steel plate with an inset panel and bolts
        I.poly(regular(0, 0, 20, 8, 22.5f), rgb(132, 156, 184), 0.6f);
        I.poly(regular(0, 0, 12.5f, 8, 22.5f), rgb(70, 86, 108), 0.3f);
        for (float x : { -6.0f, 6.0f }) for (float y : { -6.0f, 6.0f }) I.circle(x, y, 1.8f, rgb(226, 236, 246), 0.6f);
        break;
    case ICO_UP_GUNS:   // a turret on the roof
        I.box(0, 13, 15, 4, 1.5f, rgb(110, 122, 96), 0.4f);
        I.circle(-2, 3, 9, rgb(150, 176, 110), 0.5f);
        I.seg(0, 0, 19, -9, 2.6f, rgb(96, 108, 118), 0.3f);
        I.circle(19, -9, 2.0f, INK, 0).deco();
        break;
    case ICO_UP_REPAIR:   // wrench and a green plus
        I.seg(-14, 14, 2, -2, 3.0f, STEEL, 0.35f);
        I.circle(6, -6, 8.5f, STEEL, 0.35f);
        I.box(12, -12, 3.2f, 3.2f, 0.8f, INK, 0).deco();
        I.box(13, 13, 7, 2.2f, 0.8f, GREEN, 0.3f);
        I.box(13, 13, 2.2f, 7, 0.8f, GREEN, 0.3f);
        break;
    case ICO_UP_DRONES:   // a quad drone
        for (float x : { -12.0f, 12.0f }) for (float y : { -12.0f, 12.0f }) I.seg(0, 0, x, y, 1.6f, rgb(96, 104, 118), 0.3f);
        for (float x : { -13.0f, 13.0f }) for (float y : { -13.0f, 13.0f }) I.circle(x, y, 6.5f, rgb(196, 236, 255), 0).alpha(0.8f).deco();
        I.circle(0, 0, 6.5f, rgb(74, 84, 100), 0.5f);
        I.circle(0, 0, 2.6f, CYAN, 0.2f);
        break;
    case ICO_UP_HYPER:   // a glowing hexagon shield
        I.glow(0, 0, 23, paint::rgba(CYAN, 120));
        I.poly(regular(0, 0, 21, 6, -90), rgb(96, 210, 255), 0.4f);
        I.poly(regular(0, 0, 13.5f, 6, -90), rgb(24, 58, 86), 0.2f);
        I.ring(0, 0, 17, 1.6f, rgb(180, 240, 255)).deco();
        I.circle(0, 0, 4.5f, rgb(230, 250, 255), 0.3f);
        break;
    case ICO_UP_ANTIROCKET:   // a laser beam burning a rocket
        I.seg(-21, -2, 1, -1, 1.2f, rgb(255, 90, 76), 0).deco();
        I.circle(-21, -2, 2.6f, rgb(255, 90, 76), 0.2f);
        I.glow(1, -1, 9, paint::rgba(rgb(255, 160, 80), 150));
        I.poly({ { -10, 10 }, { -16, 14 }, { -14, 17 }, { -6, 12 } }, GREY, 0.3f);
        I.seg(-10, 10, 12, -12, 3.0f, rgb(226, 230, 236), 0.35f);
        I.poly({ { 14.1f, -9.9f }, { 9.9f, -14.1f }, { 16.95f, -16.95f } }, rgb(240, 80, 70), 0.35f);
        I.circle(1, -1, 2.6f, rgb(255, 210, 110), 0).deco();
        break;
    case ICO_BU_SHELL:   // the bunker's dome under a thick layered shell
        I.poly(ellipse(0, 4, 21, 15), rgb(108, 116, 126), 0.5f);
        I.poly(ellipse(0, 3, 16, 11), rgb(150, 160, 172), 0.6f);
        I.poly(ellipse(0, 2, 10, 6.5f), rgb(70, 78, 90), 0.3f);
        for (int k = 0; k < 6; k++) { float t = k * 1.0472f + 0.5f; I.circle(std::cos(t) * 18.5f, 4 + std::sin(t) * 13, 1.5f, rgb(226, 232, 240), 0.5f).deco(); }
        break;
    case ICO_BU_LASERS:   // a twin laser turret firing up into the sky
        I.seg(4, -6, 20, -21, 1.3f, CYAN, 0).deco();
        I.glow(20, -21, 7, paint::rgba(CYAN, 170));
        I.box(-2, 13, 15, 5, 1.5f, rgb(96, 104, 116), 0.4f);
        I.circle(-2, 4, 9, rgb(150, 162, 176), 0.55f);
        I.seg(-2, 2, 9, -9, 2.2f, rgb(70, 78, 92), 0.3f);
        I.seg(-6, 0, 5, -11, 2.2f, rgb(70, 78, 92), 0.3f);
        I.circle(-2, 4, 3, CYAN, 0.2f);
        break;
    case ICO_BU_MEDIC:   // a green cross over a hatch
        I.circle(0, 0, 20, rgb(234, 240, 236), 0.5f);
        I.box(0, 0, 13, 4.5f, 1.0f, rgb(40, 190, 96), 0.35f);
        I.box(0, 0, 4.5f, 13, 1.0f, rgb(40, 190, 96), 0.35f);
        I.ring(0, 0, 17, 1.4f, rgb(120, 220, 150)).deco();
        break;
    case ICO_BU_TRIPLE:   // three warheads side by side
        for (int k = -1; k <= 1; k++) {
            float x = k * 13.0f, y = k == 0 ? -3.0f : 3.0f;
            I.seg(x, y + 13, x, y - 7, 5.0f, rgb(246, 236, 220), 0.5f);
            I.poly({ { x - 5, y - 7 }, { x + 5, y - 7 }, { x, y - 17 } }, rgb(232, 84, 40), 0.4f);
            I.box(x, y + 2, 5, 1.6f, 0.4f, rgb(255, 186, 58), 0.2f).deco();
        }
        break;
    case ICO_BU_SWIFT:   // a fast warhead with speed streaks
        for (int k = 0; k < 3; k++) I.seg(-21, -8 + k * 8, -9, -8 + k * 8, 1.2f, rgb(255, 214, 140), 0).deco();
        I.seg(-8, 0, 12, 0, 6.0f, rgb(240, 242, 246), 0.5f);
        I.poly({ { 12, -6 }, { 12, 6 }, { 22, 0 } }, rgb(220, 50, 40), 0.4f);
        I.poly({ { -8, -6 }, { -14, -12 }, { -4, -6 } }, rgb(220, 50, 40), 0.3f);
        I.poly({ { -8, 6 }, { -14, 12 }, { -4, 6 } }, rgb(220, 50, 40), 0.3f);
        I.box(2, 0, 2, 6, 0.4f, rgb(220, 50, 40), 0.2f).deco();
        break;
    case ICO_BU_PARA:   // a radio mast calling in parachutes
        I.seg(-8, 20, -8, -14, 1.6f, rgb(170, 178, 190), 0.3f);
        I.seg(-14, 20, -8, -6, 0.9f, rgb(130, 138, 150), 0).deco();
        I.seg(-2, 20, -8, -6, 0.9f, rgb(130, 138, 150), 0).deco();
        I.circle(-8, -16, 2.6f, rgb(255, 80, 60), 0.2f);
        I.ring(-8, -16, 7, 1.0f, rgb(255, 214, 140)).deco();
        for (float x : { 9.0f, 18.0f }) {
            I.poly(ellipse(x, -4, 6, 4.5f, 8), rgb(112, 198, 255), 0.4f);
            I.seg(x, 0, x, 8, 0.6f, PALE, 0).deco();
            I.box(x, 10, 2.5f, 2.5f, 0.6f, rgb(110, 128, 80), 0.3f);
        }
        break;
    case ICO_BU_RAIN:   // a rack of five small warheads, red noses up
        I.box(0, 10, 19, 2.4f, 0.8f, rgb(96, 104, 116), 0.3f);
        for (int k = -2; k <= 2; k++) {
            float x = k * 8.0f;
            I.seg(x, 9, x, -5, 2.4f, SILVER, 0.5f);
            I.poly({ { x - 2.6f, -5 }, { x + 2.6f, -5 }, { x, -12 } }, HOT, 0.4f);
        }
        break;
    case ICO_BU_TANKS:   // a square lift pad with an amber ring, a white H and a beacon light on a post
        I.box(0, 5, 17, 17, 2.0f, rgb(70, 80, 96), 0.4f);
        I.ring(0, 5, 14.5f, 1.2f, ORANGE, 0.3f).deco();
        I.seg(-6, -3, -6, 13, 2.2f, WHITE, 0.0f).deco();
        I.seg(6, -3, 6, 13, 2.2f, WHITE, 0.0f).deco();
        I.seg(-6, 5, 6, 5, 2.2f, WHITE, 0.0f).deco();
        I.seg(0, -12, 0, -18, 0.9f, GREY, 0.2f).deco();
        I.circle(0, -20, 2.6f, AMBER, 0.2f);
        break;
    case ICO_RAINNUKE:   // five small warheads falling, each with its impact ring below
        for (int k = -2; k <= 2; k++) {
            float x = k * 8.5f, y = (k & 1) ? -6.0f : -14.0f;
            I.seg(x, y - 6, x, y + 4, 2.0f, SILVER, 0.5f);
            I.poly({ { x - 2.2f, y + 4 }, { x + 2.2f, y + 4 }, { x, y + 9 } }, HOT, 0.4f);
            I.ring(x, 15.5f, 3.6f, 1.1f, ORANGE, 0.2f).deco();
        }
        break;
    case ICO_TANKDROP: {   // a tank under an orange parachute, its straps running down to the hull
        std::vector<paint::P> canopy;
        for (int i = 0; i <= 8; i++) { float t = (180.0f + i * 22.5f) * 3.14159265f / 180.0f; canopy.push_back({ std::cos(t) * 17, -10 + std::sin(t) * 13 }); }
        I.poly(canopy, rgb(232, 122, 52), 0.4f);
        for (float deg : { 225.0f, 270.0f, 315.0f }) {
            float t = deg * 3.14159265f / 180.0f;
            I.seg(0, -10, std::cos(t) * 17, -10 + std::sin(t) * 13, 0.9f, WHITE, 0.0f).deco();
        }
        I.seg(-14, -9, -9, 6, 0.6f, PALE, 0.0f).deco();
        I.seg(14, -9, 9, 6, 0.6f, PALE, 0.0f).deco();
        I.box(0, 10, 13, 5.0f, 1.2f, rgb(122, 136, 84), 0.4f);       // the hull, olive
        I.seg(0, 9, 15, 9, 1.8f, rgb(92, 102, 64), 0.3f);            // the gun barrel
        I.seg(-12, 17, 12, 17, 2.4f, rgb(40, 44, 40), 0.2f);         // the tracks
        I.circle(-12, 17, 2.4f, rgb(40, 44, 40), 0.2f);
        I.circle(12, 17, 2.4f, rgb(40, 44, 40), 0.2f);
        break;
    }
    case ICO_BUNKERALL:   // three small bunkers under an upward arrow: the whole base upgraded at once
        for (int k = -1; k <= 1; k++) {
            float x = k * 12.0f;
            I.poly(ellipse(x, 5, 5.4f, 3.8f, 10), rgb(134, 150, 170), 0.4f);   // the dome
            I.box(x, 9.5f, 5.4f, 4.2f, 1.0f, rgb(104, 118, 138), 0.4f);        // its base
            I.box(x, 12.6f, 1.8f, 1.4f, 0.4f, INK, 0.0f).deco();               // the door
        }
        I.poly({ { 0, -21 }, { 9, -10 }, { 3.5f, -10 }, { 3.5f, -1 }, { -3.5f, -1 }, { -3.5f, -10 }, { -9, -10 } }, GOLD, 0.4f);
        break;
    case ICO_UNLOAD:   // everybody out of a door
        I.box(-12, 0, 7, 14, 1.5f, rgb(110, 118, 130), 0.3f);
        I.box(-12, 0, 4, 11, 0.8f, rgb(24, 28, 36), 0).deco();
        I.seg(-5, 0, 8, 0, 2.0f, AMBER, 0.3f);
        I.poly({ { 6, -9 }, { 19, 0 }, { 6, 9 } }, AMBER, 0.3f);
        break;
    case ICO_DROPOFF:   // the lifter sets down on a marked spot
        I.poly(ellipse(0, 15, 15, 4.5f), rgb(110, 236, 150), 0.2f);
        I.seg(0, -19, 0, 4, 2.0f, AMBER, 0.3f);
        I.poly({ { -11, 0 }, { 11, 0 }, { 0, 12 } }, AMBER, 0.3f);
        break;
    default: break;
    }
}

// --- painting ------------------------------------------------------------------------------------------------------------------------

// bounding box of a piece (canvas px), grown by `grow`
void bounds(const Piece& q, float grow, float& x0, float& y0, float& x1, float& y1) {
    float m = grow + 2.0f;
    switch (q.k) {
    case K_CIRCLE: x0 = q.a[0] - q.a[2] - m; x1 = q.a[0] + q.a[2] + m; y0 = q.a[1] - q.a[2] - m; y1 = q.a[1] + q.a[2] + m; break;
    case K_SEG: {
        float r = q.a[4] + m;
        x0 = std::min(q.a[0], q.a[2]) - r; x1 = std::max(q.a[0], q.a[2]) + r;
        y0 = std::min(q.a[1], q.a[3]) - r; y1 = std::max(q.a[1], q.a[3]) + r; break;
    }
    case K_BOX: x0 = q.a[0] - q.a[2] - m; x1 = q.a[0] + q.a[2] + m; y0 = q.a[1] - q.a[3] - m; y1 = q.a[1] + q.a[3] + m; break;
    case K_RING: { float r = q.a[2] + q.a[3] * 0.5f + m; x0 = q.a[0] - r; x1 = q.a[0] + r; y0 = q.a[1] - r; y1 = q.a[1] + r; break; }
    case K_POLY: {
        x0 = y0 = 1e9f; x1 = y1 = -1e9f;
        for (auto& v : q.v) { x0 = std::min(x0, v.x); x1 = std::max(x1, v.x); y0 = std::min(y0, v.y); y1 = std::max(y1, v.y); }
        x0 -= m; y0 -= m; x1 += m; y1 += m; break;
    }
    default: x0 = y0 = x1 = y1 = 0; break;
    }
}

// one piece painted in one pass: grown by `grow` canvas px, filled `col`, lit by `kk`
void paintPiece(paint::Art& A, const Piece& q, float grow, Color col, float kk, float alpha) {
    float fx0, fy0, fx1, fy1;
    bounds(q, grow, fx0, fy0, fx1, fy1);
    int x0 = (int)std::floor(fx0), y0 = (int)std::floor(fy0), x1 = (int)std::ceil(fx1), y1 = (int)std::ceil(fy1);
    switch (q.k) {
    case K_CIRCLE: {
        float cx = q.a[0], cy = q.a[1], r = q.a[2] + grow;
        A.shape(x0, y0, x1, y1, [=](float x, float y) { return paint::hyp(x - cx, y - cy) - r; }, col, BEVEL, kk, alpha);
        break;
    }
    case K_SEG: {
        float ax = q.a[0], ay = q.a[1], bx = q.a[2], by = q.a[3], r = q.a[4] + grow;
        A.shape(x0, y0, x1, y1, [=](float x, float y) { return paint::sdCap(x, y, ax, ay, bx, by, r); }, col, BEVEL, kk, alpha);
        break;
    }
    case K_BOX: {
        float cx = q.a[0], cy = q.a[1], hw = q.a[2] + grow, hh = q.a[3] + grow, rd = q.a[4] + grow;
        A.shape(x0, y0, x1, y1, [=](float x, float y) { return paint::sdBox(x, y, cx, cy, hw, hh, rd); }, col, BEVEL, kk, alpha);
        break;
    }
    case K_RING: {
        float cx = q.a[0], cy = q.a[1], r = q.a[2], half = q.a[3] * 0.5f + grow;
        A.shape(x0, y0, x1, y1, [=](float x, float y) { return std::abs(paint::hyp(x - cx, y - cy) - r) - half; }, col, BEVEL, kk, alpha);
        break;
    }
    case K_POLY: {
        const std::vector<paint::P>& v = q.v;
        float sgn = q.a[0];
        A.shape(x0, y0, x1, y1, [&v, sgn, grow](float x, float y) { return paint::sdPoly(v, x, y, sgn) - grow; }, col, BEVEL, kk, alpha);
        break;
    }
    default: break;
    }
}

void render(Canvas& c, const Ico& I) {
    paint::Art A(c, rgb(255, 255, 255));
    for (const Piece& q : I.p) if (q.outline && q.k != K_GLOW) paintPiece(A, q, OUTLINE, INK, 0, 1);
    for (const Piece& q : I.p) {
        if (q.k == K_GLOW) A.glow(q.a[0], q.a[1], q.a[2], q.col);
        else paintPiece(A, q, 0, q.col, q.kk, q.alpha);
    }
}

// --- baked sprites -------------------------------------------------------------------------------------------------------------

Sprite s_icon[ICO_COUNT];
Sprite s_bld[B_COUNT];
Sprite s_unit[U_COUNT][MAX_PLAYERS];

// The sprite as it is shown (its own dscale applied), fitted inside a maxDim square, as a texture of its own. Each halving on the GPU
// averages 2x2 texels, so a big structure does not alias the way one bilinear step would.
Sprite shrinkSprite(Gfx& g, const Sprite& src, float maxDim) {
    Sprite out;
    if (!src.tex || src.w <= 0 || src.h <= 0) return out;
    float tw = src.w * src.dscale, th = src.h * src.dscale;
    float f = std::min(1.0f, maxDim / std::max(tw, th));
    int W = std::max(1, (int)std::lround(tw * f)), H = std::max(1, (int)std::lround(th * f));
    SDL_Texture* cur = src.tex;
    int cw = src.w, ch = src.h;
    bool own = false;
    if (!g.useAtlas) SDL_SetTextureScaleMode(cur, SDL_ScaleModeLinear);   // (an atlas page is bilinear already)
    g.bind(src, rgb(255, 255, 255), 255);
    do {
        int nw = cw > 2 * W ? std::max(W, (cw + 1) / 2) : W;
        int nh = ch > 2 * H ? std::max(H, (ch + 1) / 2) : H;
        SDL_Texture* t = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_TARGET, nw, nh);
        if (!t) { if (own) SDL_DestroyTexture(cur); return Sprite(); }
        SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
        SDL_SetTextureScaleMode(t, SDL_ScaleModeLinear);
        SDL_SetRenderTarget(g.ren, t);
        SDL_SetRenderDrawColor(g.ren, 0, 0, 0, 0);
        SDL_RenderClear(g.ren);
        SDL_FRect dst = { 0, 0, (float)nw, (float)nh };
        SDL_RenderCopyF(g.ren, cur, own ? nullptr : src.srcRect(), &dst);
        SDL_SetRenderTarget(g.ren, nullptr);
        if (own) SDL_DestroyTexture(cur);
        cur = t; own = true; cw = nw; ch = nh;
    } while (cw != W || ch != H);
    if (g.useAtlas) {   // read the thumbnail back into the atlas: the build menu's pictures then draw in one batch with the rest of the HUD
        Canvas c(W, H);
        SDL_SetRenderTarget(g.ren, cur);
        bool ok = SDL_RenderReadPixels(g.ren, nullptr, SDL_PIXELFORMAT_ABGR8888, c.px.data(), W * 4) == 0;
        SDL_SetRenderTarget(g.ren, nullptr);
        if (ok) { SDL_DestroyTexture(cur); out = g.fromCanvasSmooth(c, W * 0.5f, H * 0.5f); return out; }
    }
    out.tex = cur; out.w = W; out.h = H;
    return out;
}

void drawBaked(Gfx& g, const Sprite& s, float cx, float cy, float size, bool enabled) {
    if (!s.tex || s.w <= 0 || s.h <= 0) return;
    float f = std::min(size / s.w, size / s.h);
    g.drawSized(s, cx, cy, s.w * f, s.h * f, 0, enabled ? rgb(255, 255, 255) : DIM, enabled ? 255 : 150);
}

}  // namespace

void iconsInit(Gfx& g) {
    for (int i = ICO_NONE + 1; i < ICO_COUNT; i++) {
        Canvas c(CANVAS, CANVAS);
        Ico I;
        buildIcon(i, I);
        render(c, I);
        s_icon[i] = g.fromCanvasSmooth(c, CANVAS / 2.0f, CANVAS / 2.0f);
    }
    for (int t = 0; t < B_COUNT; t++) s_bld[t] = shrinkSprite(g, g.building[t], THUMB);
    for (int t = 0; t < U_COUNT; t++) for (int p = 0; p < MAX_PLAYERS; p++) s_unit[t][p] = shrinkSprite(g, g.unitBody[t][p], THUMB);
}

void iconDraw(Gfx& g, int icon, float cx, float cy, float size, bool enabled) {
    if (icon <= ICO_NONE || icon >= ICO_COUNT) return;
    drawBaked(g, s_icon[icon], cx, cy, size, enabled);
}

void iconDrawBuilding(Gfx& g, int type, float cx, float cy, float size, bool enabled) {
    if (type < 0 || type >= B_COUNT) return;
    drawBaked(g, s_bld[type], cx, cy, size, enabled);
}

void iconDrawUnit(Gfx& g, int type, int slot, float cx, float cy, float size, bool enabled) {
    if (type < 0 || type >= U_COUNT || slot < 0 || slot >= MAX_PLAYERS) return;
    drawBaked(g, s_unit[type][slot], cx, cy, size, enabled);
}
