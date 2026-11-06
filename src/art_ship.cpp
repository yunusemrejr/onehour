// One Hour - warship art: the Cyber "Pulse Destroyer", "Lancer Cruiser", "Pulse Gunboat", "Battleship" and "Nuclear Submarine", and the Clanker
// "Iron Destroyer", "Rocket Cruiser", "Gunboat", "Battleship" and "Submarine". Same conventions as art.cpp: the hulls lie along x with the bow at
// +x and the centre on the canvas (32,32 for the 64 px ships, 64,64 for the 128 px battleship and submarine), painted with paint::Art and lit from
// the north-west. Cyber ships are graphite and cobalt composites with pale ceramic plating and cyan lights; Clanker ships are olive and steel,
// riveted, rust-streaked, with hazard-yellow trim. The rotating mounts are separate sprites drawn live at each mount offset, so every hull carries
// an empty pedestal under each of them. Paint is deterministic: the weathering uses fixed seeds.
#include "art2.h"
#include "paint.h"

using namespace paint;

namespace {

const Color CY_HULL = rgb(46, 62, 92), CY_DECK = rgb(36, 48, 72), CY_PALE = rgb(166, 182, 202), CY_DARK = rgb(22, 30, 44), CY_GLOW = rgb(110, 232, 255);
const Color CY_FOAM = rgb(236, 246, 252), CY_WHITE = rgb(230, 238, 246);
const Color CK_HULL = rgb(88, 96, 64), CK_IRON = rgb(120, 122, 116), CK_DARK = rgb(38, 40, 32), CK_STEEL = rgb(134, 136, 132);
const Color CK_RUST = rgb(156, 90, 48), CK_HAZ = rgb(226, 182, 62), CK_FOAM = rgb(232, 238, 236), CK_RAFT = rgb(204, 128, 70);
// patrol boat: graphite-blue composite hull with a cobalt deck (Cyber); grey-olive hull with a light grey deck (Clanker)
const Color PB_CY_HULL = rgb(66, 78, 96), PB_CY_DECK = rgb(58, 90, 142), PB_CK_HULL = rgb(104, 106, 90), PB_CK_DECK = rgb(150, 150, 136);
const Color PB_GLASS = rgb(26, 58, 80);

// The player's mark on a ship: a jewel-like dot (as teamDot in art.cpp)
void teamDot(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 1.2f, rgba(shade(CY_DARK, 0.6f), 235), 1.0f, 0.2f);
    A.circle(cx, cy, r, A.team, r, 0.75f);
    A.dot(cx - r * 0.32f, cy - r * 0.36f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.75f), 235));
}
void rivets(Art& A, float x0, float y0, float x1, float y1, int n, Color c) {
    for (int i = 0; i < n; i++) { float t = n > 1 ? i / (float)(n - 1) : 0.5f; A.dot(x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, 0.7f, c); }
}
void hatch(Art& A, float cx, float cy, float r, Color base) {
    A.circle(cx, cy, r, base, 1.4f, 0.55f);
    A.circle(cx, cy, r * 0.6f, shade(base, 0.7f), 1.0f, 0.5f);
    A.dot(cx - r * 0.2f, cy - r * 0.2f, r * 0.14f, shade(base, 1.6f));
}
// the empty pedestal under a mount: a dark recess with a lit lip (the mount sprite sits on it)
void pedestal(Art& A, float cx, float cy, float r, Color base) {
    A.circle(cx, cy, r, shade(base, 0.38f), 1.2f, 0.4f);
    A.ring(cx, cy, r - 0.9f, 1.1f, shade(base, 1.9f), 0.7f, 0.3f);
}
// alternating stripes along a line (hazard trim)
void dashes(Art& A, float x0, float y0, float x1, float y1, int n, float w, Color a, Color b) {
    for (int i = 0; i < n; i++) {
        float t0 = i / (float)n, t1 = (i + 0.5f) / (float)n;
        A.line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, w, i % 2 ? a : b);
    }
}
// rust run-downs from the rails
void streaks(Art& A, float x0, float y0, float x1, float y1, int n, u32 seed) {
    Rng r(seed);
    for (int i = 0; i < n; i++) {
        float x = r.f(x0, x1), y = r.f(y0, y1), l = r.f(1.5f, 4.0f);
        A.line(x, y, x + r.f(-0.3f, 0.3f), y + l, 0.9f, rgba(CK_RUST, 150));
    }
}
void polyline(Art& A, const std::vector<P>& v, float w, Color col) {
    for (size_t i = 0; i + 1 < v.size(); i++) A.line(v[i].x, v[i].y, v[i + 1].x, v[i + 1].y, w, col);
}
// a close-in gun seen from above: a dome, a barrel pointing at the bow, a sensor pip
void ciws(Art& A, float x, float y, bool cy) {
    A.circle(x, y, 1.7f, cy ? CY_PALE : CK_STEEL, 1.0f, 0.5f);
    A.line(x, y, x + 3.4f, y, 0.9f, rgba(cy ? CY_DARK : CK_DARK, 230));
    A.dot(x, y, 0.5f, cy ? CY_GLOW : CK_RUST);
}
// a small lamp with its glow
void lamp(Art& A, float x, float y, float r, Color c) {
    A.glow(x, y, r * 3.0f, rgba(c, 80));
    A.dot(x, y, r, c);
}
// a helicopter pad: a ringed disc with an H
void heliPad(Art& A, float x, float y, float r, bool cy) {
    Color pad = cy ? CY_PALE : CK_HAZ, hg = cy ? CY_GLOW : rgb(240, 236, 220);
    A.circle(x, y, r, pad, 1.0f, 0.3f);
    A.circle(x, y, r * 0.8f, shade(cy ? CY_DECK : CK_IRON, 0.9f), 1.0f, 0.3f);
    float h = r * 0.46f;
    A.line(x - h * 0.7f, y - h, x - h * 0.7f, y + h, 0.9f, hg); A.line(x + h * 0.7f, y - h, x + h * 0.7f, y + h, 0.9f, hg); A.line(x - h * 0.7f, y, x + h * 0.7f, y, 0.9f, hg);
}
// a vertical-launch block seen from above: a dark frame with cols x rows lids, each lid with a seal light (Cyber) or a rust seal (Clanker)
void vlsBlock(Art& A, float x0, float y0, float pitchX, float pitchY, int cols, int rows, bool cy) {
    A.rect(x0 - 0.8f, y0 - 0.8f, cols * pitchX + 1.6f, rows * pitchY + 1.6f, 0.5f, shade(cy ? CY_DARK : CK_DARK, 1.5f), 0.8f, 0.3f);
    for (int j = 0; j < rows; j++) for (int i = 0; i < cols; i++) {
        float cx = x0 + pitchX * (i + 0.5f), cyy = y0 + pitchY * (j + 0.5f);
        A.rect(cx - pitchX * 0.36f, cyy - pitchY * 0.36f, pitchX * 0.72f, pitchY * 0.72f, 0.25f, cy ? CY_PALE : CK_STEEL, 0.5f, 0.35f);
        A.dot(cx, cyy, 0.42f, cy ? CY_GLOW : CK_RUST);
    }
}
// mirror a list of points about the horizontal axis y = axis
std::vector<P> mirY(const std::vector<P>& v, float axis) {
    std::vector<P> o;
    for (const P& p : v) o.push_back({p.x, 2 * axis - p.y});
    return o;
}
// a symmetric convex outline from its upper chain: the chain runs stern to bow and its last point (the bow tip) lies on the axis
std::vector<P> symHull(const std::vector<P>& top, float axis) {
    std::vector<P> v = top;
    for (int i = (int)top.size() - 2; i >= 0; i--) v.push_back({top[i].x, 2 * axis - top[i].y});
    return v;
}

// ---------------------------------------------------------------- destroyer: about 60 px long (x 2..63), 17 wide

void destroyerBody(Art& A, bool cy) {
    Color H = cy ? CY_HULL : CK_HULL, Dk = cy ? CY_DARK : CK_DARK;
    // the bow wave and the foam painted under the hull
    A.poly(symHull({ {1.6f, 24.2f}, {14, 22.6f}, {38, 22.4f}, {50, 23.2f}, {61.2f, 28.4f}, {63.8f, 32} }, 32), cy ? CY_FOAM : CK_FOAM, 1, 0, 0.7f);
    // the hull: flared sides, a raking clipper bow, a flat transom
    A.poly(symHull({ {3, 25.2f}, {14, 23.9f}, {38, 23.6f}, {50, 24.4f}, {58.4f, 27.2f}, {62.4f, 32} }, 32), H, 2.4f, 0.6f);
    // the main deck, and the raised forecastle forward of the mount
    A.poly(symHull({ {5.2f, 26.6f}, {14, 25.5f}, {38, 25.3f}, {49, 26.2f}, {55.4f, 29.2f}, {57.6f, 32} }, 32), shade(H, 1.16f), 1.6f, 0.45f);
    A.poly(symHull({ {46, 26.2f}, {52, 26.6f}, {56.2f, 29.4f}, {57.6f, 32} }, 32), shade(H, 1.3f), 2.0f, 0.6f);
    polyline(A, { {4.5f, 25.8f}, {14, 24.9f}, {38, 24.9f}, {49.5f, 26.2f}, {58.8f, 31.2f} }, 0.7f, rgba(Dk, 200));   // rails
    polyline(A, { {4.5f, 38.2f}, {14, 39.1f}, {38, 39.1f}, {49.5f, 37.8f}, {58.8f, 32.8f} }, 0.7f, rgba(Dk, 200));
    if (cy) {
        // angular stealth facets along the deck, cyan seams on the deck edges and the forecastle
        A.line(8, 26.6f, 14, 32, 0.7f, rgba(Dk, 190)); A.line(14, 32, 8, 37.4f, 0.7f, rgba(Dk, 190));
        A.line(20, 26.2f, 24, 32, 0.7f, rgba(Dk, 170)); A.line(24, 32, 20, 37.8f, 0.7f, rgba(Dk, 170));
        A.line(4, 26.2f, 26, 25.8f, 0.8f, rgba(CY_GLOW, 210)); A.line(4, 37.8f, 26, 38.2f, 0.8f, rgba(CY_GLOW, 210));
        A.line(46, 26.6f, 56, 30.8f, 0.8f, rgba(CY_GLOW, 200)); A.line(46, 37.4f, 56, 33.2f, 0.8f, rgba(CY_GLOW, 200));
        lamp(A, 13, 26, 0.45f, CY_GLOW); lamp(A, 13, 38, 0.45f, CY_GLOW);
    } else {
        // iron plates: transverse seams, riveted rows and hazard-trimmed bow shoulders
        A.line(22, 26, 22, 38, 0.6f, rgba(Dk, 170)); A.line(38.2f, 26.2f, 38.2f, 37.8f, 0.6f, rgba(Dk, 170));
        rivets(A, 5, 27.4f, 37, 27.2f, 8, shade(H, 1.35f)); rivets(A, 5, 36.6f, 37, 36.8f, 8, shade(H, 1.35f));
        dashes(A, 47, 26.4f, 57, 30.6f, 6, 1.2f, CK_HAZ, Dk);
        dashes(A, 47, 37.6f, 57, 33.4f, 6, 1.2f, CK_HAZ, Dk);
    }
    // the helicopter pad on the stern
    heliPad(A, 8.4f, 32, 3.6f, cy);
    // vertical-launch cells fore and aft of the bridge (hatch grids)
    vlsBlock(A, 25.6f, 27.6f, 2.2f, 1.9f, 2, 5, cy);
    vlsBlock(A, 38.6f, 27.6f, 2.2f, 1.9f, 2, 5, cy);
    // the bridge: Cyber a faceted pale tower with a cyan window band; Clanker a box with a stack and a mast
    if (cy) {
        A.poly({ {31.4f, 28.6f}, {35, 26.6f}, {37.6f, 28.2f}, {37.6f, 35.8f}, {35, 37.4f}, {31.4f, 35.4f} }, CY_PALE, 1.8f, 0.55f);
        A.line(37.0f, 29.6f, 37.0f, 34.4f, 0.9f, rgba(CY_GLOW, 230));
        A.ngon(33.8f, 32, 2.1f, 6, 0, CY_DECK, 0.7f, 0.4f);
        A.dot(33.8f, 32, 0.6f, CY_GLOW);
        teamDot(A, 35.2f, 28.6f, 1.2f);
    } else {
        A.rect(31.2f, 26.4f, 6.4f, 11.2f, 1.0f, shade(CK_IRON, 0.95f), 1.8f, 0.5f);
        rivets(A, 31.8f, 28.0f, 31.8f, 36.0f, 4, shade(CK_IRON, 1.35f));
        A.circle(34.6f, 32, 2.2f, CK_STEEL, 1.4f, 0.5f);      // the stack, banded in hazard yellow
        A.ring(34.6f, 32, 1.8f, 0.5f, CK_HAZ, 0.6f, 0.2f);
        A.dot(34.6f, 32, 0.5f, CK_DARK);
        A.line(36.6f, 28.0f, 36.6f, 36.0f, 0.5f, rgba(CK_DARK, 220));   // a lattice mast along the bridge roof
        teamDot(A, 36.8f, 28.4f, 1.2f);
    }
    // the anchor at the stem, and bow cleats
    A.dot(56.0f, 25.4f, 0.8f, Dk); A.line(56.0f, 25.4f, 59.5f, 26.4f, 0.6f, rgba(Dk, 210));
    A.dot(59.0f, 32, 0.5f, Dk);
    // the team pennant at the stern
    A.poly({ {7.5f, 30.5f}, {10.8f, 31.6f}, {7.5f, 32.7f} }, A.team, 0.6f, 0, 1);
    // the empty pedestals of the two mounts, drawn last so the rims show
    ShipMounts m = shipMounts(false);
    for (int i = 0; i < m.n; i++) pedestal(A, 32 + m.x[i], 32 + m.y[i], 6.0f, H);
    if (cy) {
        A.grain(2, 23, 62, 42, 0.05f, 43);
    } else {   // weathering: grain over the plating and rust run-downs from both rails
        A.grain(2, 23, 62, 42, 0.14f, 41);
        streaks(A, 8, 25.2f, 36, 26.4f, 5, 91);
        streaks(A, 8, 37.6f, 36, 38.8f, 5, 92);
    }
}

// ---------------------------------------------------------------- cruiser: about 62 px long (x 1.5..63), 21 wide

void cruiserBody(Art& A, bool cy) {
    Color H = cy ? CY_HULL : CK_HULL, Dk = cy ? CY_DARK : CK_DARK;
    A.poly(symHull({ {0.2f, 22.2f}, {16, 20.2f}, {44, 20.2f}, {56.2f, 21.8f}, {63.4f, 32} }, 32), cy ? CY_FOAM : CK_FOAM, 1, 0, 0.75f);
    A.poly(symHull({ {1.5f, 23}, {16, 21.5f}, {44, 21.5f}, {56, 23}, {63, 32} }, 32), H, 2.8f, 0.6f);
    // the long deck, rails along both edges
    A.poly(symHull({ {5, 25.2f}, {44, 24.6f}, {54, 27.8f}, {58, 32} }, 32), shade(H, 1.16f), 1.8f, 0.45f);
    polyline(A, { {4, 23.8f}, {16, 22.9f}, {44, 22.9f}, {55, 24.4f}, {60.5f, 32} }, 0.7f, rgba(Dk, 200));
    polyline(A, { {4, 40.2f}, {16, 41.1f}, {44, 41.1f}, {55, 39.6f}, {60.5f, 32} }, 0.7f, rgba(Dk, 200));
    if (cy) {
        A.line(5, 25.4f, 53, 25.2f, 0.8f, rgba(CY_GLOW, 200)); A.line(5, 38.6f, 53, 38.8f, 0.8f, rgba(CY_GLOW, 200));
        A.line(4, 32, 53, 32, 0.6f, rgba(CY_DARK, 130));
        lamp(A, 14, 25.4f, 0.4f, CY_GLOW); lamp(A, 14, 38.6f, 0.4f, CY_GLOW);
    } else {
        rivets(A, 5, 27, 43, 26.8f, 10, shade(H, 1.35f)); rivets(A, 5, 37, 43, 37.2f, 10, shade(H, 1.35f));
        dashes(A, 48, 22.8f, 60, 28.9f, 9, 1.2f, CK_HAZ, Dk);
        dashes(A, 48, 41.2f, 60, 35.1f, 9, 1.2f, CK_HAZ, Dk);
    }
    // the missile hatch panels along the deck, two rows outboard of the centre line
    for (int i = 0; i < 4; i++) {
        float x = 36.4f + i * 3.4f;
        for (int s = -1; s <= 1; s += 2) {
            float y = s < 0 ? 22.8f : 37.2f;
            A.rect(x, y, 2.6f, 2.4f, 0.4f, shade(H, 0.8f), 0.8f, 0.5f);
            if (cy) A.line(x + 0.5f, y + 1.2f, x + 2.1f, y + 1.2f, 0.5f, rgba(CY_GLOW, 220));
            else A.dot(x + 1.3f, y + 1.2f, 0.5f, rgba(CK_RUST, 230));
        }
    }
    // superstructure: a broad lower tier and a big radar on the mid tier (Cyber); a box with twin funnels (Clanker)
    if (cy) {
        A.poly({ {26, 25.4f}, {31, 23.6f}, {37, 24.4f}, {38, 32}, {37, 39.6f}, {31, 40.4f}, {26, 38.6f} }, CY_PALE, 2.4f, 0.55f);
        A.poly({ {28, 28.2f}, {33, 26.8f}, {36.2f, 28.4f}, {36.6f, 32}, {36.2f, 35.6f}, {33, 37.2f}, {28, 35.8f} }, CY_DECK, 2.0f, 0.5f);
        A.line(33, 26.8f, 36.2f, 28.4f, 0.7f, rgba(CY_GLOW, 210)); A.line(33, 37.2f, 36.2f, 35.6f, 0.7f, rgba(CY_GLOW, 210));
        A.circle(29.6f, 32, 3.0f, CY_PALE, 1.2f, 0.5f);
        A.ngon(29.6f, 32, 1.9f, 6, 0, CY_DECK, 0.6f, 0.3f);
        A.dot(29.6f, 32, 0.7f, CY_GLOW);
        teamDot(A, 35.4f, 32, 1.6f);
    } else {
        A.rect(25.8f, 23.6f, 12.2f, 16.8f, 1.2f, shade(CK_IRON, 0.95f), 2.0f, 0.5f);
        rivets(A, 26.8f, 25.6f, 26.8f, 38.4f, 6, shade(CK_IRON, 1.35f));
        for (int s = -1; s <= 1; s += 2) {   // twin funnels with rust bands and soot tops
            float fy = 32 + s * 4.6f;
            A.circle(29.6f, fy, 2.7f, CK_STEEL, 1.4f, 0.5f);
            A.ring(29.6f, fy, 2.2f, 0.5f, CK_RUST, 0.6f, 0.2f);
            A.circle(29.6f, fy, 1.5f, CK_DARK, 0.8f, 0.3f);
        }
        A.ring(30.6f, 32, 1.6f, 0.6f, CK_STEEL, 0.6f, 0.3f);   // lattice mast between the stacks
        A.dot(30.6f, 32, 0.6f, CK_DARK);
        A.box(35.8f, 32, 2.2f, 3.6f, 0.7f, shade(CK_IRON, 1.12f), 1.4f, 0.5f);
        teamDot(A, 35.8f, 32, 1.4f);
    }
    // close-in guns fore and aft, and the anchor at the hawse
    ciws(A, 58.0f, 27.8f, cy); ciws(A, 58.0f, 36.2f, cy);
    ciws(A, 5.4f, 26.4f, cy); ciws(A, 5.4f, 37.6f, cy);
    A.dot(52.2f, 23.4f, 0.9f, Dk); A.line(52.2f, 23.4f, 55.4f, 23.8f, 0.7f, rgba(Dk, 210));
    // the empty pedestals of the two launchers, drawn last
    ShipMounts m = shipMounts(true);
    for (int i = 0; i < m.n; i++) pedestal(A, 32 + m.x[i], 32 + m.y[i], 9.4f, H);
    if (cy) {
        A.grain(1, 21, 62, 43, 0.05f, 63);
    } else {
        A.grain(1, 21, 62, 43, 0.14f, 61);
        streaks(A, 8, 25.0f, 36, 26.2f, 6, 93);
        streaks(A, 8, 37.8f, 36, 39.0f, 6, 94);
    }
}

// ---------------------------------------------------------------- patrol boat: about 39 px long (x 13.6..52.6), 12 wide

void patrolBody(Art& A, bool cy) {
    Color H = cy ? PB_CY_HULL : PB_CK_HULL, D = cy ? PB_CY_DECK : PB_CK_DECK, Dk = cy ? CY_DARK : CK_DARK;
    Color fender = cy ? CY_DARK : CK_HAZ;
    // bow wave and a thin wash at the transom, painted under the hull
    A.poly(symHull({ {12.5f, 25.0f}, {17, 24.8f}, {40, 24.8f}, {44.4f, 25.2f}, {48.2f, 26.9f}, {51.6f, 29.6f}, {54, 32} }, 32), cy ? CY_FOAM : CK_FOAM, 1, 0, 0.5f);
    // planing hull: a slim, flat-transomed shape with a raked bow
    A.poly(symHull({ {14, 26.4f}, {17, 25.9f}, {40, 25.9f}, {44, 26.3f}, {47.2f, 27.5f}, {50.4f, 29.9f}, {52.6f, 32} }, 32), H, 2.4f, 0.6f);
    // light non-slip deck with a stipple of grip dots
    A.poly(symHull({ {15.6f, 27.4f}, {18, 27.0f}, {40, 27.0f}, {43.5f, 27.6f}, {46.2f, 28.8f}, {49, 30.6f}, {50.8f, 32} }, 32), shade(D, 1.12f), 1.6f, 0.45f);
    for (float x = 17.0f; x <= 44.0f; x += 2.2f)
        for (float y = 28.0f; y <= 36.0f; y += 2.2f) A.dot(x + (int)(y / 2.2f) % 2 * 1.1f, y, 0.28f, rgba(shade(D, 0.6f), 90));
    polyline(A, { {15.6f, 26.9f}, {40, 26.9f}, {44.6f, 27.8f}, {48.2f, 29.6f}, {50.4f, 32} }, 0.7f, rgba(Dk, 200));   // rails
    polyline(A, { {15.6f, 37.1f}, {40, 37.1f}, {44.6f, 36.2f}, {48.2f, 34.4f}, {50.4f, 32} }, 0.7f, rgba(Dk, 200));
    if (cy) {
        A.line(18, 27.8f, 42, 27.8f, 0.6f, rgba(CY_GLOW, 210)); A.line(18, 36.2f, 42, 36.2f, 0.6f, rgba(CY_GLOW, 210));   // cyan accent seams
    } else {
        rivets(A, 18, 28.4f, 40, 28.4f, 6, shade(H, 1.35f)); rivets(A, 18, 35.6f, 40, 35.6f, 6, shade(H, 1.35f));
        dashes(A, 42.5f, 27.4f, 50.2f, 30.6f, 5, 1.0f, CK_HAZ, Dk);   // hazard-trimmed bow shoulders
        dashes(A, 42.5f, 36.6f, 50.2f, 33.4f, 5, 1.0f, CK_HAZ, Dk);
    }
    // rubber fenders bulging out past the sheer, every 9 px along both sides
    for (float x = 22.0f; x <= 40.0f; x += 9.0f) {
        A.cap(x - 1.2f, 25.4f, x + 1.2f, 25.4f, 0.8f, fender, 0.6f, 0.5f);
        A.cap(x - 1.2f, 38.6f, x + 1.2f, 38.6f, 0.8f, fender, 0.6f, 0.5f);
    }
    // twin outboard engines on the transom, with pale cowl tops
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 3.1f;
        A.box(12.2f, y, 2.2f, 1.6f, 0.7f, cy ? rgb(30, 36, 48) : rgb(54, 56, 50), 1.4f, 0.5f);
        A.dot(11.2f, y - s * 0.2f, 0.45f, cy ? CY_PALE : rgb(120, 122, 110));
    }
    // aft deck: a life-raft canister with its strap, a bitt on each side and a small hatch
    A.rect(16.2f, 35.0f, 3.6f, 2.0f, 0.6f, cy ? CY_PALE : CK_RAFT, 1.2f, 0.5f);
    A.line(18.0f, 35.0f, 18.0f, 37.0f, 0.4f, rgba(Dk, 170));
    A.dot(16.0f, 28.4f, 0.5f, Dk); A.dot(16.0f, 35.6f, 0.5f, Dk);
    hatch(A, 19.4f, 30.6f, 1.2f, shade(D, 1.2f));
    // low angular wheelhouse a little aft of the middle, glazed across the forward face, with the team band across its roof
    Color roof = cy ? shade(CY_PALE, 0.9f) : shade(CK_IRON, 0.72f);
    A.poly({ {21.8f, 28.2f}, {28.6f, 28.2f}, {30.6f, 30.0f}, {30.6f, 34.0f}, {28.6f, 35.8f}, {21.8f, 35.8f} }, roof, 2.2f, 0.55f);
    A.poly({ {28.4f, 29.4f}, {29.8f, 30.2f}, {30.2f, 32}, {29.8f, 33.8f}, {28.4f, 34.6f} }, PB_GLASS, 0.8f, 0.3f);
    A.line(28.8f, 30.4f, 29.6f, 31.2f, 0.4f, rgba(rgb(255, 255, 255), 120));
    A.rect(24.4f, 28.2f, 1.8f, 7.6f, 0.2f, A.team, 1.0f, 0.3f);
    // a short mast with a radar bar across the beam, on the roof's aft half
    A.dot(23.0f, 32, 0.9f, Dk);
    A.line(23.0f, 29.0f, 23.0f, 35.0f, 0.9f, cy ? CY_DARK : CK_DARK);
    A.dot(23.0f, 29.0f, 0.55f, cy ? CY_PALE : CK_STEEL); A.dot(23.0f, 35.0f, 0.55f, cy ? CY_PALE : CK_STEEL);
    // the empty pedestal of the mount on the foredeck, drawn last so the rim shows
    ShipMounts m = patrolMounts();
    for (int i = 0; i < m.n; i++) pedestal(A, 32 + m.x[i], 32 + m.y[i], 4.9f, H);
    if (cy) {
        A.grain(14, 25, 52, 39, 0.05f, 71);
    } else {   // weathering: grain over the plating and rust run-downs from the rails
        A.grain(14, 25, 52, 39, 0.14f, 72);
        streaks(A, 18, 27.0f, 40, 28.0f, 5, 95);
        streaks(A, 18, 36.0f, 40, 37.0f, 5, 96);
    }
}

// ---------------------------------------------------------------- mounts (one sprite, centred at (32,32), pointing +x)

// Pulse Destroyer: a faceted shield with twin pulse-cannon barrels and cyan lens tips at the muzzles
void gunCyber(Art& A) {
    A.poly({ {38.2f, 32}, {34.4f, 26.4f}, {27.8f, 26.8f}, {25.8f, 32}, {27.8f, 37.2f}, {34.4f, 37.6f} }, CY_PALE, 1.4f, 0.6f);
    A.line(27.8f, 26.8f, 34.4f, 32, 0.5f, rgba(CY_DARK, 120)); A.line(27.8f, 37.2f, 34.4f, 32, 0.5f, rgba(CY_DARK, 120));
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 2.8f;
        A.cap(33.5f, y, 42.2f, y, 1.2f, CY_DARK, 0.8f, 0.4f);
        A.circle(42.6f, y, 1.5f, CY_DARK, 0.8f, 0.3f);
        A.dot(42.6f, y, 1.0f, CY_GLOW);
        A.glow(42.6f, y, 3.0f, rgba(CY_GLOW, 120));
    }
    A.circle(31.4f, 32, 2.2f, CY_DECK, 1.0f, 0.4f); A.dot(31.4f, 32, 0.8f, CY_GLOW);
}
// Iron Destroyer: a rounded armoured turret with a hatch on its roof and twin long naval guns with muzzle brakes
void gunClanker(Art& A) {
    A.circle(32, 32, 6.0f, shade(CK_HULL, 1.2f), 2.4f, 0.6f);
    A.ring(32, 32, 4.4f, 0.7f, shade(CK_HULL, 0.8f), 0.8f, 0.3f);
    hatch(A, 28.4f, 32, 2.0f, shade(CK_HULL, 1.4f));
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 2.6f;
        A.cap(35.6f, y, 45.0f, y, 1.3f, shade(CK_IRON, 0.8f), 1.0f, 0.5f);
        A.box(44.8f, y, 1.0f, 1.8f, 0.3f, CK_DARK, 0.6f, 0.4f);
    }
    A.box(36.0f, 32, 1.8f, 4.6f, 0.8f, shade(CK_HULL, 0.9f), 1.4f, 0.5f);
}
// Lancer Cruiser: a deep angled VLS box, four columns of two cells, each lid with a cyan seal rim
void launcherCyber(Art& A) {
    A.poly({ {18.2f, 27.6f}, {44.8f, 26.2f}, {46.2f, 32}, {44.8f, 37.8f}, {18.2f, 36.4f} }, CY_PALE, 1.6f, 0.6f);
    for (int i = 0; i < 4; i++) {
        float x = 22.6f + i * 5.6f;
        for (int j = 0; j < 2; j++) {
            float y = 29.6f + j * 4.8f;
            A.rect(x - 2.1f, y - 1.9f, 4.2f, 3.8f, 0.5f, shade(CY_DECK, 1.3f), 0.6f, 0.5f);
            A.line(x - 2.0f, y - 1.9f, x + 2.0f, y - 1.9f, 0.5f, rgba(CY_GLOW, 230));
            A.dot(x, y + 0.2f, 0.45f, CY_DARK);
        }
    }
    A.line(19.4f, 32, 43.8f, 32, 0.5f, rgba(CY_DARK, 170));   // the centre seam between the two rows
}
// Rocket Cruiser: a rack of three rows of long rocket tubes on an iron frame, rust-red warheads at the front
void rackClanker(Art& A) {
    A.box(32, 32, 14.0f, 8.0f, 1.2f, shade(CK_IRON, 0.8f), 1.4f, 0.5f);
    for (int r = 0; r < 3; r++) {
        float y = 27.6f + r * 4.4f;
        A.cap(20.6f, y, 42.2f, y, 1.7f, CK_STEEL, 1.0f, 0.5f);
        A.circle(43.8f, y, 1.8f, CK_RUST, 1.0f, 0.6f);
        A.dot(43.8f, y, 0.6f, rgb(90, 44, 24));
    }
    A.line(20.0f, 32, 44.0f, 32, 0.4f, rgba(CK_DARK, 170));
}
// Patrol boat: a compact turret with a twin-barrel autocannon pointing +x (barrels to about +10 px) and a pod of two rocket tubes on the
// port side, their mouths at the front; the dome sits on a dark hatch ring
void patrolGun(Art& A, bool cy) {
    Color T = cy ? rgb(92, 106, 128) : shade(CK_HULL, 1.15f), Bar = cy ? CY_DARK : CK_DARK, Tube = cy ? CY_PALE : CK_STEEL, Warhead = cy ? CY_DARK : CK_RUST;
    A.box(33.2f, 26.6f, 4.6f, 1.9f, 0.6f, shade(T, 0.85f), 1.4f, 0.5f);   // rocket pod on the port side
    for (float y : { 25.6f, 27.6f }) {
        A.circle(37.6f, y, 0.8f, Tube, 0.7f, 0.4f);
        A.dot(37.6f, y, 0.4f, Warhead);
    }
    A.circle(32, 32, 4.2f, T, 1.8f, 0.6f);                 // turret dome
    A.ring(32, 32, 3.1f, 0.6f, shade(T, 0.8f), 0.7f, 0.3f);
    hatch(A, 29.4f, 32, 1.1f, shade(T, 1.25f));            // gunner's hatch
    A.box(35.0f, 32, 1.8f, 2.4f, 0.6f, shade(T, 0.9f), 1.2f, 0.5f);   // breech housing
    for (int s = -1; s <= 1; s += 2) {                     // twin barrels with muzzle tips
        float y = 32 + s * 1.6f;
        A.line(35.0f, y, 41.6f, y, 1.2f, Bar);
        A.box(41.8f, y, 0.4f, 0.7f, 0.2f, cy ? shade(Bar, 1.4f) : CK_HAZ, 0.5f, 0.4f);
    }
    if (cy) A.line(36.0f, 32.0f, 40.5f, 32.0f, 0.3f, rgba(CY_GLOW, 170));   // thin cyan seam between the barrels
}

}  // namespace

// The mount offsets from the hull centre (canvas px, +x towards the bow): the destroyer has a forward gun on the forecastle and one
// aft, the cruiser a launcher forward and one aft. Both sit on the centre line and on the pedestals the hull paints.
ShipMounts shipMounts(bool cruiser) {
    if (cruiser) return ShipMounts{ 2, { 16.0f, -16.0f, 0 }, { 0, 0, 0 } };
    return ShipMounts{ 2, { 17.0f, -14.0f, 0 }, { 0, 0, 0 } };
}

void artShipBody(Canvas& c, Color team, bool cyber, bool cruiser) {
    Art A(c, team);
    if (cruiser) cruiserBody(A, cyber);
    else destroyerBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
}

bool artShipMount(Canvas& c, Color team, bool cyber, bool cruiser) {
    Art A(c, team);
    if (cruiser) { if (cyber) launcherCyber(A); else rackClanker(A); }
    else { if (cyber) gunCyber(A); else gunClanker(A); }
    c.outline(rgb(8, 10, 14, 200));
    return true;
}

// The patrol boat's one mount sits on the foredeck, 7 px forward of the hull centre (its pedestal is painted there).
ShipMounts patrolMounts() { return ShipMounts{ 1, { 7.0f, 0, 0 }, { 0, 0, 0 } }; }

void artPatrolBoatBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    patrolBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
}

bool artPatrolBoatMount(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    patrolGun(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
    return true;
}

// ---------------------------------------------------------------- battleship: 128 px canvas, hull x 6..123.6 (about 118 px), 26 wide

// The battleship's three mounts: two forward (superfiring: the forward turret on the bow, the second one a turret's length aft of it) and one aft
// on the centre line. Canvas px from the centre (64,64), +x towards the bow. The hull paints an empty barbette under each.
ShipMounts battleshipMounts() { return ShipMounts{ 3, { 41.0f, 27.0f, -38.0f }, { 0, 0, 0 } }; }

namespace {

void battleshipBody(Art& A, bool cy) {
    Color deck = cy ? rgb(190, 202, 214) : rgb(132, 134, 128), side = cy ? rgb(40, 50, 70) : rgb(86, 88, 82), Dk = cy ? CY_DARK : CK_DARK;
    // the bow wave and the wash painted under the hull
    A.poly(symHull({ {3.6f, 54.6f}, {10, 51.2f}, {26, 49.6f}, {56, 48.8f}, {92, 49.2f}, {108, 52.4f}, {119, 58}, {127.2f, 64} }, 64), cy ? CY_FOAM : CK_FOAM, 1, 0, 0.6f);
    // the hull: flared, armoured sides, a raking clipper bow, a flat transom
    A.poly(symHull({ {6, 55.2f}, {10, 53.2f}, {26, 51.6f}, {56, 50.8f}, {92, 51.2f}, {108, 54.2f}, {118, 59}, {123.6f, 64} }, 64), side, 2.2f, 0.6f);
    // the flight deck and main deck, the raised forecastle at the bow
    A.poly(symHull({ {9, 57.2f}, {14, 56}, {30, 55.4f}, {56, 55.2f}, {92, 55.6f}, {106, 58.6f}, {114, 62}, {118, 64} }, 64), deck, 1.8f, 0.45f);
    A.poly(symHull({ {92, 56.4f}, {104, 57.8f}, {112, 60.6f}, {116, 64} }, 64), shade(deck, 1.08f), 1.6f, 0.5f);
    // armour: transverse deck seams along the citadel and the rails along both edges
    for (float x : { 22.0f, 34.0f, 46.0f, 62.0f, 78.0f, 94.0f }) A.line(x, 56.2f, x, 71.8f, 0.5f, rgba(Dk, 150));
    polyline(A, { {9.5f, 57.6f}, {30, 56.4f}, {56, 56.2f}, {92, 56.6f}, {106, 59.4f}, {114, 62.6f} }, 0.6f, rgba(cy ? CY_GLOW : Dk, cy ? 200 : 170));
    polyline(A, { {9.5f, 70.4f}, {30, 71.6f}, {56, 71.8f}, {92, 71.4f}, {106, 68.6f}, {114, 65.4f} }, 0.6f, rgba(cy ? CY_GLOW : Dk, cy ? 200 : 170));
    // the sponsons: two secondary gun turrets each side, two CIWS on the quarters, a life raft each side
    for (int s = -1; s <= 1; s += 2) {
        float y = 64 + s * 9.2f;
        for (float x : { 40.0f, 84.0f }) {
            A.circle(x, y, 1.8f, cy ? CY_PALE : CK_STEEL, 1.0f, 0.5f);
            A.line(x, y, x + 3.4f, y - s * 0.4f, 0.8f, rgba(Dk, 230));
        }
        ciws(A, 18.0f, 64 + s * 8.4f, cy);
        A.box(28.0f, 64 + s * 9.6f, 2.4f, 1.0f, 0.5f, cy ? CY_PALE : CK_RAFT, 0.8f, 0.4f);
    }
    // the helicopter deck aft: a pad with its H
    heliPad(A, 12.0f, 64, 5.0f, cy);
    // the vertical-launch blocks, two 4x4 hatch grids: one aft (under the citadel), one fore of the bridge
    vlsBlock(A, 36.8f, 57.8f, 3.4f, 3.4f, 4, 4, cy);
    vlsBlock(A, 68.6f, 57.8f, 3.4f, 3.4f, 4, 4, cy);
    // the tall pagoda bridge: three stepped tiers, each with a lit window band; a radar on top of the tower
    A.shadowRect(53.6f, 57.6f, 12.8f, 12.8f, 1.6f, 1.6f, 1.4f, 150);
    Color t0 = cy ? rgb(200, 210, 222) : rgb(118, 120, 114), t1 = cy ? shade(CY_PALE, 0.95f) : shade(CK_IRON, 1.05f), t2 = cy ? CY_DECK : shade(CK_IRON, 0.85f);
    A.rect(53.6f, 57.6f, 12.8f, 12.8f, 1.0f, t0, 2.0f, 0.55f);
    A.rect(55.6f, 59.6f, 8.8f, 8.8f, 0.8f, t1, 1.6f, 0.5f);
    A.rect(57.6f, 61.6f, 4.8f, 4.8f, 0.6f, t2, 1.2f, 0.5f);
    A.line(54.4f, 60.6f, 65.6f, 60.6f, 0.6f, rgba(cy ? CY_GLOW : Dk, cy ? 220 : 170));
    A.line(54.4f, 67.4f, 65.6f, 67.4f, 0.6f, rgba(cy ? CY_GLOW : Dk, cy ? 220 : 170));
    if (cy) {
        A.ngon(60.0f, 64.0f, 2.0f, 6, 0, CY_WHITE, 0.8f, 0.4f);   // a faceted radar on the tower's top tier
        lamp(A, 60.0f, 64.0f, 0.7f, CY_GLOW);
    } else {
        A.line(57.2f, 62.4f, 62.8f, 65.6f, 0.6f, CK_DARK); A.line(62.8f, 62.4f, 57.2f, 65.6f, 0.6f, CK_DARK);   // lattice mast
        A.circle(60.0f, 64.0f, 1.6f, rgb(184, 186, 180), 0.8f, 0.5f);
        A.dot(60.0f, 64.0f, 0.4f, CK_DARK);
    }
    teamDot(A, 66.2f, 64, 1.2f);
    // the anchor at the hawse, the bollards, the team pennant on the transom
    A.dot(116.8f, 60.4f, 0.9f, Dk); A.line(116.8f, 60.4f, 121.4f, 61.2f, 0.7f, rgba(Dk, 210));
    for (float x : { 104.0f, 22.0f }) { A.dot(x, 56.6f, 0.6f, Dk); A.dot(x, 71.4f, 0.6f, Dk); }
    A.dot(7.2f, 64, 0.8f, Dk);
    A.poly({ {7.2f, 62.8f}, {2.4f, 64}, {7.2f, 65.2f} }, A.team, 0.6f, 0, 1);
    // the team stripe along the bow shoulders (Cyber) or the hazard shoulders and the rust waterline (Clanker)
    if (cy) {
        A.line(96, 52.6f, 110, 55.2f, 0.9f, rgba(A.team, 230)); A.line(96, 75.4f, 110, 72.8f, 0.9f, rgba(A.team, 230));
    } else {
        dashes(A, 96, 56.0f, 113, 62.0f, 6, 1.2f, CK_HAZ, Dk);
        dashes(A, 96, 72.0f, 113, 66.0f, 6, 1.2f, CK_HAZ, Dk);
        polyline(A, { {6, 55.2f}, {10, 53.2f}, {26, 51.6f}, {56, 50.8f}, {92, 51.2f}, {108, 54.2f}, {118, 59} }, 1.1f, rgba(CK_RUST, 200));
        streaks(A, 30, 52.6f, 70, 53.6f, 6, 111);
        streaks(A, 30, 74.4f, 70, 75.4f, 6, 112);
    }
    // the empty barbettes of the three turrets, drawn last so the rims show
    ShipMounts m = battleshipMounts();
    for (int i = 0; i < m.n; i++) pedestal(A, 64 + m.x[i], 64 + m.y[i], 6.4f, deck);
    if (cy) {
        A.grain(6, 51, 123, 77, 0.04f, 143);
    } else {
        A.grain(6, 51, 123, 77, 0.14f, 141);
        for (float x = 14; x < 110; x += 17) A.line(x, 76.6f, x + 1.2f, 78.0f, 0.7f, rgba(CK_RUST, 150));
    }
}

// the heavy gun turret: a rounded housing with three barrels out of the front, muzzle brakes and a roof hatch
void battleshipMountBody(Art& A, bool cy) {
    Color T = cy ? rgb(150, 162, 178) : rgb(104, 106, 100), Bar = cy ? CY_DARK : CK_DARK;
    A.box(32, 32, 12.0f, 10.0f, 3.6f, shade(T, 0.92f), 2.0f, 0.6f);   // the turret housing (24 x 20 px)
    A.line(30.6f, 22.8f, 30.6f, 41.2f, 0.5f, rgba(Bar, 110));          // a seam across the roof
    A.ring(25.0f, 32, 3.6f, 0.8f, shade(T, 0.75f), 0.8f, 0.3f);          // the roof hatch
    A.circle(25.0f, 32, 2.6f, shade(T, 1.25f), 0.8f, 0.5f);
    A.dot(25.0f, 32, 0.6f, Bar);
    A.circle(21.2f, 32, 1.8f, shade(T, 1.4f), 0.8f, 0.5f);               // the rangefinder dome at the back
    for (float s : { -4.2f, 0.0f, 4.2f }) {                                 // three barrels with muzzle brakes
        float y = 32 + s;
        A.box(42.0f, y, 1.8f, 2.5f, 0.6f, shade(T, 0.8f), 1.0f, 0.5f);    // breech
        A.cap(40.0f, y, 58.6f, y, 1.5f, Bar, 0.8f, 0.5f);
        A.box(59.2f, y, 1.2f, 2.0f, 0.4f, cy ? shade(Bar, 1.6f) : CK_HAZ, 0.6f, 0.4f);
    }
    if (cy) for (float s : { -4.2f, 0.0f, 4.2f }) A.dot(60.2f, 32 + s, 0.5f, CY_GLOW);
}

}  // namespace

void artBattleshipBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    battleshipBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
}

bool artBattleshipMount(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    battleshipMountBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
    return true;
}

// ---------------------------------------------------------------- nuclear submarine: 128 px canvas, hull x 9..119 (about 110 px), 14 wide

namespace {

// the hull's upper chain (stern to bow, the bow tip on the axis), the casing inside it and the sail
std::vector<P> subHull() { return { {10, 61.2f}, {16, 59.4f}, {30, 57.6f}, {60, 57.0f}, {92, 57.0f}, {106, 58.0f}, {114, 60.2f}, {119, 64} }; }
std::vector<P> subCasing() { return { {12, 62.8f}, {16, 61.2f}, {30, 59.6f}, {60, 59.2f}, {92, 59.2f}, {104, 60.0f}, {112, 61.8f}, {116, 64} }; }
std::vector<P> subSail() { return { {72.8f, 64}, {74, 61.0f}, {80.4f, 59.9f}, {89.6f, 60.9f}, {92.6f, 64} }; }
std::vector<P> subSailTop() { return { {76.4f, 64}, {78, 62.4f}, {84.6f, 61.7f}, {89.2f, 62.6f}, {91, 64} }; }
std::vector<P> subPlane() { return { {84.4f, 60.6f}, {89.8f, 60.4f}, {88.6f, 55.6f}, {85.4f, 55.4f} }; }

// the surfaced boat in full detail; hatchOpen paints the launch moment (one missile hatch open and glowing)
void subSurfaced(Art& A, bool cy, bool hatchOpen) {
    Color H = cy ? rgb(44, 52, 66) : rgb(58, 60, 56), casing = cy ? rgb(64, 74, 92) : rgb(76, 78, 72), sail = cy ? rgb(150, 162, 178) : rgb(78, 78, 72);
    Color Dk = cy ? CY_DARK : CK_DARK, hot = cy ? CY_GLOW : rgb(255, 170, 60);
    // the wash of the bow wave and the stern, painted round the hull
    A.poly(symHull({ {9, 60.2f}, {16, 58.2f}, {30, 56.4f}, {60, 55.8f}, {92, 55.8f}, {106, 56.8f}, {115, 59.2f}, {120.4f, 64} }, 64), cy ? CY_FOAM : CK_FOAM, 1, 0, 0.42f);
    // the wet dark hull and its casing
    A.poly(symHull(subHull(), 64), shade(H, 0.9f), 2.0f, 0.6f);
    A.poly(symHull(subCasing(), 64), shade(casing, 0.95f), 1.4f, 0.5f);
    // the anechoic tiles: a staggered grid of seams across the casing
    for (int r = 0; r < 5; r++) {
        float y = 59.6f + r * 2.2f;
        for (float x = 17 + (r % 2) * 1.6f; x < 104; x += 3.2f) A.line(x, y, x, y + 2.2f, 0.35f, rgba(Dk, 80));
        A.line(17, y + 2.2f, 104, y + 2.2f, 0.3f, rgba(Dk, 50));
    }
    // the waterline washing over the casing: a pale edge of foam along the casing
    polyline(A, subCasing(), 0.6f, rgba(CY_FOAM, cy ? 110 : 90));
    // the bow: the sonar dome seam; the stern planes of the X tail, with the rudder fin between them
    A.line(114.4f, 60.4f, 114.4f, 67.6f, 0.4f, rgba(Dk, 150));
    A.poly({ {10.6f, 61.0f}, {17.6f, 59.6f}, {13.8f, 52.6f}, {9.4f, 53.8f} }, shade(H, 1.08f), 1.0f, 0.5f);
    A.poly(mirY({ {10.6f, 61.0f}, {17.6f, 59.6f}, {13.8f, 52.6f}, {9.4f, 53.8f} }, 64), shade(H, 1.08f), 1.0f, 0.5f);
    A.line(9.6f, 64, 17.0f, 64, 1.0f, rgba(Dk, 200));
    // the row of ballistic-missile hatches behind the sail, on the centre line
    for (int i = 0; i < 5; i++) {
        float x = 46 + i * 6;
        if (hatchOpen && i == 2) {
            A.circle(x, 64, 1.9f, rgb(10, 12, 16), 0.6f, 0);
            A.dot(x, 64, 1.2f, hot);
            A.glow(x, 64, 6.5f, rgba(hot, 130));
            A.dot(x, 64, 0.5f, rgb(255, 255, 255));
        } else {
            A.circle(x, 64, 1.9f, shade(casing, 1.12f), 0.6f, 0.5f);
            A.ring(x, 64, 1.1f, 0.4f, shade(casing, 0.6f), 0.4f, 0.2f);
            A.dot(x, 64, 0.35f, Dk);
        }
    }
    // the sail (the conning tower) about a third back from the bow, its fairing, its sail planes and the periscope
    A.poly(symHull(subSail(), 64), sail, 1.8f, 0.6f);
    A.poly(symHull(subSailTop(), 64), shade(sail, 1.25f), 1.2f, 0.5f);
    A.poly(subPlane(), shade(sail, 1.05f), 0.8f, 0.4f);
    A.poly(mirY(subPlane(), 64), shade(sail, 1.05f), 0.8f, 0.4f);
    A.circle(80.6f, 64, 0.9f, shade(sail, 1.5f), 0.6f, 0.3f);
    A.dot(80.6f, 64, 0.35f, Dk);
    if (cy) {
        A.line(75.6f, 60.6f, 89.4f, 60.4f, 0.5f, rgba(CY_GLOW, 220));      // a cyan light strip along each side of the sail
        A.line(75.6f, 67.4f, 89.4f, 67.6f, 0.5f, rgba(CY_GLOW, 220));
    } else {
        A.line(74.8f, 60.8f, 89.2f, 60.8f, 0.45f, rgba(CK_HAZ, 230));     // hazard markings on the sail
        A.line(74.8f, 67.2f, 89.2f, 67.2f, 0.45f, rgba(CK_HAZ, 230));
        polyline(A, subHull(), 1.0f, rgba(CK_RUST, 200));                  // rust-red waterline on the hull's upper edge
    }
}

// the boat as its own side sees it: submerged, a dark translucent blue-grey silhouette with only the sail top and the periscope breaking the surface
void subSubmerged(Art& A, bool cy) {
    Color ink = cy ? rgb(26, 38, 54) : rgb(34, 42, 46), top = cy ? rgb(176, 196, 214) : rgb(170, 180, 176);
    A.poly(symHull(subHull(), 64), ink, 0, 0, 0.5f);                                                      // the hull, dark and translucent
    A.poly({ {10.6f, 61.0f}, {17.6f, 59.6f}, {13.8f, 52.6f}, {9.4f, 53.8f} }, ink, 0, 0, 0.5f);          // the stern planes
    A.poly(mirY({ {10.6f, 61.0f}, {17.6f, 59.6f}, {13.8f, 52.6f}, {9.4f, 53.8f} }, 64), ink, 0, 0, 0.5f);
    A.poly(symHull(subSail(), 64), ink, 0, 0, 0.55f);
    polyline(A, subHull(), 0.5f, rgba(rgb(120, 150, 170), 80));                                            // a faint rim where the light catches the hull
    A.poly(symHull(subSailTop(), 64), top, 0, 0, 0.85f);                                                  // the sail top breaking the surface
    A.dot(80.6f, 64, 0.5f, rgba(top, 220));                                                               // the periscope
}

}  // namespace

// state 0: surfaced, full detail; state 1: submerged, as its own side sees it (a translucent silhouette, alpha ~90-140); state 2: surfaced
// with one missile hatch open and glowing (the launch moment)
void artSubBody(Canvas& c, Color team, bool cyber, int state) {
    Art A(c, team);
    if (state == 1) {
        subSubmerged(A, cyber);
        return;   // (no outline: the silhouette is a tint on the water)
    }
    subSurfaced(A, cyber, state == 2);
    c.outline(rgb(8, 10, 14, 200));
}
