// One Hour - warship art: the Cyber "Pulse Destroyer" and "Lancer Cruiser", the Clanker "Iron Destroyer" and "Rocket Cruiser".
// Same conventions as art.cpp: 64x64 canvases, the hull lies along x with the bow at +x and centre (32,32), painted with paint::Art at
// scale 1 and lit from the north-west. Cyber ships are clean graphite and cobalt with pale ceramic plating and cyan seams; Clanker ships
// are olive and iron, riveted, rust-streaked, with hazard-yellow trim. The rotating mounts are one separate sprite drawn live at each
// shipMounts() offset, so every hull carries an empty pedestal under each of them.
#include "art2.h"
#include "paint.h"

using namespace paint;

namespace {

const Color CY_HULL = rgb(46, 62, 92), CY_DECK = rgb(36, 48, 72), CY_PALE = rgb(166, 182, 202), CY_DARK = rgb(22, 30, 44), CY_GLOW = rgb(110, 232, 255);
const Color CY_FOAM = rgb(236, 246, 252);
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

// ---------------------------------------------------------------- destroyer: about 60 px long (x 2..62), 17 wide
void destroyerBody(Art& A, bool cy) {
    Color H = cy ? CY_HULL : CK_HULL, D = cy ? CY_DECK : CK_IRON, Dk = cy ? CY_DARK : CK_DARK;
    // foam rim just outside the hull (painted under it) and the bow wave crest
    A.poly({ {0.8f, 23.2f}, {14, 22.1f}, {34, 22.1f}, {50.6f, 23.8f}, {63.3f, 32}, {50.6f, 40.2f}, {34, 41.9f}, {14, 41.9f}, {0.8f, 40.8f} }, cy ? CY_FOAM : CK_FOAM, 1, 0, 0.75f);
    A.poly({ {2, 24.5f}, {14, 23.5f}, {34, 23.5f}, {50, 25}, {62, 32}, {50, 39}, {34, 40.5f}, {14, 40.5f}, {2, 39.5f} }, H, 2.6f, 0.6f);
    // main deck panel, the raised forecastle forward
    A.poly({ {4, 26.2f}, {30, 25.6f}, {46, 26.8f}, {58, 31.5f}, {58, 32.5f}, {46, 37.2f}, {30, 38.4f}, {4, 37.8f} }, shade(H, 1.16f), 1.6f, 0.45f);
    A.poly({ {44, 26.6f}, {50, 25.8f}, {55, 28.4f}, {57, 32}, {55, 35.6f}, {50, 38.2f}, {44, 37.4f} }, shade(H, 1.32f), 2.0f, 0.6f);
    polyline(A, { {4.5f, 25.8f}, {14, 24.9f}, {34, 24.9f}, {49.5f, 26.2f}, {60.2f, 32} }, 0.7f, rgba(Dk, 200));   // rails
    polyline(A, { {4.5f, 38.2f}, {14, 39.1f}, {34, 39.1f}, {49.5f, 37.8f}, {60.2f, 32} }, 0.7f, rgba(Dk, 200));
    if (cy) {
        // angular stealth facets, cyan seams along the deck edges
        A.line(8, 26.6f, 14, 32, 0.7f, rgba(Dk, 190)); A.line(14, 32, 8, 37.4f, 0.7f, rgba(Dk, 190));
        A.line(19, 26.2f, 24, 32, 0.7f, rgba(Dk, 170)); A.line(24, 32, 19, 37.8f, 0.7f, rgba(Dk, 170));
        A.line(4, 26.2f, 26, 25.8f, 0.8f, rgba(CY_GLOW, 210)); A.line(4, 37.8f, 26, 38.2f, 0.8f, rgba(CY_GLOW, 210));
        A.line(46, 26.8f, 57, 31.6f, 0.8f, rgba(CY_GLOW, 200)); A.line(46, 37.2f, 57, 32.4f, 0.8f, rgba(CY_GLOW, 200));
        A.glow(15, 26, 6, rgba(CY_GLOW, 40)); A.glow(15, 38, 6, rgba(CY_GLOW, 40));
    } else {
        // iron plates: seams, riveted rows and the hazard-trimmed bow shoulders
        A.line(16, 26, 16, 38, 0.6f, rgba(Dk, 170)); A.line(44, 26.4f, 44, 37.6f, 0.6f, rgba(Dk, 170));
        rivets(A, 5, 27.4f, 44, 27.2f, 9, shade(H, 1.35f)); rivets(A, 5, 36.6f, 44, 36.8f, 9, shade(H, 1.35f));
        dashes(A, 48, 25.6f, 60, 31.2f, 8, 1.2f, CK_HAZ, Dk);
        dashes(A, 48, 38.4f, 60, 32.8f, 8, 1.2f, CK_HAZ, Dk);
    }
    // helipad marking aft: a ringed disc with an H
    Color pad = cy ? CY_PALE : CK_HAZ, hg = cy ? CY_GLOW : rgb(240, 236, 220);
    A.circle(6.2f, 32, 4.0f, pad, 1.0f, 0.3f);
    A.circle(6.2f, 32, 3.2f, shade(D, 0.9f), 1.0f, 0.3f);
    A.line(5.0f, 30.2f, 5.0f, 33.8f, 0.9f, hg); A.line(7.4f, 30.2f, 7.4f, 33.8f, 0.9f, hg); A.line(5.0f, 32, 7.4f, 32, 0.9f, hg);
    // superstructure amidships
    if (cy) {
        // lower block of pale ceramic, a cobalt bridge on it with a cyan window edge, a faceted radar and the team dot
        A.poly({ {26, 28.6f}, {31, 26.4f}, {39, 27.2f}, {40.5f, 32}, {39, 36.8f}, {31, 37.6f}, {26, 35.4f} }, CY_PALE, 2.2f, 0.55f);
        A.poly({ {28.4f, 29.6f}, {33, 28.2f}, {37.2f, 29.8f}, {38.2f, 32}, {37.2f, 34.2f}, {33, 35.8f}, {28.4f, 34.4f} }, CY_DECK, 1.6f, 0.5f);
        A.line(37.6f, 30.4f, 37.6f, 33.6f, 0.9f, rgba(CY_GLOW, 230));
        A.poly({ {29.2f, 29.8f}, {31.2f, 32}, {29.2f, 34.2f}, {27.2f, 32} }, shade(CY_PALE, 0.7f), 0.8f, 0.4f);
        A.glow(29.2f, 32, 3.6f, rgba(CY_GLOW, 55)); A.dot(29.2f, 32, 0.8f, CY_GLOW);
        teamDot(A, 33.6f, 32, 2.0f);
    } else {
        // iron block with a yellow-banded funnel, the bridge box aft of it and the team dot on the bridge roof
        A.rect(25.6f, 25.6f, 14.4f, 12.8f, 1.2f, shade(CK_IRON, 0.95f), 2.0f, 0.5f);
        rivets(A, 27, 27.4f, 27, 36.6f, 4, shade(CK_IRON, 1.35f));
        A.circle(29.8f, 32, 3.6f, CK_STEEL, 1.6f, 0.5f);
        A.ring(29.8f, 32, 3.15f, 0.7f, CK_HAZ, 0.6f, 0.2f);
        A.circle(29.8f, 32, 2.5f, CK_DARK, 1.0f, 0.3f);
        A.box(36.9f, 32, 2.9f, 3.8f, 0.8f, shade(CK_IRON, 1.12f), 1.6f, 0.5f);
        A.line(34.6f, 29.5f, 39.2f, 29.5f, 0.8f, rgba(CK_DARK, 220)); A.line(34.6f, 34.5f, 39.2f, 34.5f, 0.8f, rgba(CK_DARK, 220));
        teamDot(A, 36.9f, 32, 1.6f);
    }
    // deck fittings: bow hatches, life rafts on the rails, an anchor at the stem, the team pennant at the masthead
    hatch(A, 53.5f, 27.6f, 1.5f, shade(H, 1.22f));
    hatch(A, 53.5f, 36.4f, 1.5f, shade(H, 1.22f));
    A.box(15, 25.0f, 2.6f, 1.1f, 0.5f, cy ? CY_PALE : CK_RAFT, 0.8f, 0.4f);
    A.box(15, 39.0f, 2.6f, 1.1f, 0.5f, cy ? CY_PALE : CK_RAFT, 0.8f, 0.4f);
    A.dot(50.5f, 25.2f, 0.9f, Dk); A.line(50.5f, 25.2f, 56, 25.6f, 0.7f, rgba(Dk, 210));
    A.dot(58.4f, 32, 0.6f, Dk);
    A.poly({ {58.4f, 30.9f}, {61.8f, 32}, {58.4f, 33.1f} }, A.team, 0.8f, 0, 1);
    // the empty pedestals of the two mounts, drawn last so the rims show
    ShipMounts m = shipMounts(false);
    for (int i = 0; i < m.n; i++) pedestal(A, 32 + m.x[i], 32 + m.y[i], 7.6f, H);
    if (cy) {
        A.grain(2, 23, 62, 42, 0.05f, 43);
    } else {   // weathering: grain over the plating and rust run-downs from both rails
        A.grain(2, 23, 62, 42, 0.14f, 41);
        streaks(A, 8, 25.2f, 40, 26.4f, 6, 91);
        streaks(A, 8, 37.6f, 40, 38.8f, 6, 92);
    }
}

// ---------------------------------------------------------------- cruiser: about 62 px long (x 1.5..63), 21 wide
void cruiserBody(Art& A, bool cy) {
    Color H = cy ? CY_HULL : CK_HULL, Dk = cy ? CY_DARK : CK_DARK;
    A.poly({ {0.2f, 22.2f}, {16, 20.2f}, {44, 20.2f}, {56.2f, 21.8f}, {63.4f, 32}, {56.2f, 42.2f}, {44, 43.8f}, {16, 43.8f}, {0.2f, 41.8f} }, cy ? CY_FOAM : CK_FOAM, 1, 0, 0.75f);
    A.poly({ {1.5f, 23}, {16, 21.5f}, {44, 21.5f}, {56, 23}, {63, 32}, {56, 41}, {44, 42.5f}, {16, 42.5f}, {1.5f, 41} }, H, 2.8f, 0.6f);
    // the long deck, rails along both edges
    A.poly({ {5, 25.2f}, {44, 24.6f}, {54, 27.8f}, {58, 32}, {54, 36.2f}, {44, 39.4f}, {5, 38.8f} }, shade(H, 1.16f), 1.8f, 0.45f);
    polyline(A, { {4, 23.8f}, {16, 22.9f}, {44, 22.9f}, {55, 24.4f}, {60.5f, 32} }, 0.7f, rgba(Dk, 200));
    polyline(A, { {4, 40.2f}, {16, 41.1f}, {44, 41.1f}, {55, 39.6f}, {60.5f, 32} }, 0.7f, rgba(Dk, 200));
    if (cy) {
        A.line(5, 25.4f, 53, 25.2f, 0.8f, rgba(CY_GLOW, 200)); A.line(5, 38.6f, 53, 38.8f, 0.8f, rgba(CY_GLOW, 200));
        A.line(4, 32, 53, 32, 0.6f, rgba(CY_DARK, 130));
        A.glow(14, 25.4f, 6, rgba(CY_GLOW, 40)); A.glow(14, 38.6f, 6, rgba(CY_GLOW, 40));
    } else {
        rivets(A, 5, 27, 43, 26.8f, 10, shade(H, 1.35f)); rivets(A, 5, 37, 43, 37.2f, 10, shade(H, 1.35f));
        dashes(A, 48, 22.8f, 60, 28.9f, 9, 1.2f, CK_HAZ, Dk);
        dashes(A, 48, 41.2f, 60, 35.1f, 9, 1.2f, CK_HAZ, Dk);
    }
    // missile hatch panels along the foredeck, two rows outboard of the centre line
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
        A.dot(29.6f, 32, 0.7f, CY_GLOW); A.glow(29.6f, 32, 4.6f, rgba(CY_GLOW, 45));
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
        streaks(A, 8, 25.0f, 40, 26.2f, 7, 93);
        streaks(A, 8, 37.8f, 40, 39.0f, 7, 94);
    }
}

// ---------------------------------------------------------------- patrol boat: about 39 px long (x 13.6..52.6), 12 wide
void patrolBody(Art& A, bool cy) {
    Color H = cy ? PB_CY_HULL : PB_CK_HULL, D = cy ? PB_CY_DECK : PB_CK_DECK, Dk = cy ? CY_DARK : CK_DARK;
    Color fender = cy ? CY_DARK : CK_HAZ;
    // bow wave and a thin wash at the transom, painted under the hull
    A.poly({ {12.5f, 25.0f}, {17, 24.8f}, {40, 24.8f}, {44.4f, 25.2f}, {48.2f, 26.9f}, {51.6f, 29.6f}, {54, 32}, {51.6f, 34.4f}, {48.2f, 37.1f}, {44.4f, 38.8f}, {40, 39.2f}, {17, 39.2f}, {12.5f, 39.0f} }, cy ? CY_FOAM : CK_FOAM, 1, 0, 0.5f);
    // planing hull: a slim, flat-transomed shape with a sharp bow
    A.poly({ {14, 26.4f}, {17, 25.9f}, {40, 25.9f}, {44, 26.3f}, {47.2f, 27.5f}, {50.4f, 29.9f}, {52.6f, 32}, {50.4f, 34.1f}, {47.2f, 36.5f}, {44, 37.7f}, {40, 38.1f}, {17, 38.1f}, {14, 37.6f} }, H, 2.4f, 0.6f);
    // light non-slip deck with a stipple of grip dots
    A.poly({ {15.6f, 27.4f}, {18, 27.0f}, {40, 27.0f}, {43.5f, 27.6f}, {46.2f, 28.8f}, {49, 30.6f}, {50.8f, 32}, {49, 33.4f}, {46.2f, 35.2f}, {43.5f, 36.4f}, {40, 37.0f}, {18, 37.0f}, {15.6f, 36.6f} }, shade(D, 1.12f), 1.6f, 0.45f);
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
// Lancer Cruiser: a sleek angled missile canister with four tube mouths and cyan seals
void launcherCyber(Art& A) {
    A.poly({ {24, 28.2f}, {27.2f, 26.6f}, {38.6f, 26.6f}, {40, 28.4f}, {40, 35.6f}, {38.6f, 37.4f}, {27.2f, 37.4f}, {24, 35.8f} }, CY_PALE, 1.6f, 0.6f);
    A.line(25.2f, 32, 38.8f, 32, 0.6f, rgba(CY_DARK, 170));
    for (int i = 0; i < 4; i++) {
        float x = 29.4f + (i % 2) * 6.0f, y = 29.6f + (i / 2) * 4.8f;
        A.circle(x, y, 1.4f, CY_DARK, 0.8f, 0.4f);
        A.ring(x, y, 2.2f, 0.5f, CY_GLOW, 0.6f, 0);
    }
}
// Rocket Cruiser: a rack of three rows of rocket tubes with rust-red warheads at the front
void rackClanker(Art& A) {
    A.box(32, 32, 8, 5.6f, 1.2f, shade(CK_IRON, 0.8f), 1.4f, 0.5f);
    for (int r = 0; r < 3; r++) {
        float y = 28.2f + r * 3.8f;
        A.cap(25.2f, y, 37.2f, y, 1.5f, CK_STEEL, 1.0f, 0.5f);
        A.circle(38.6f, y, 1.6f, CK_RUST, 1.0f, 0.6f);
    }
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
    if (cruiser) return ShipMounts{ 2, { 15.0f, -16.0f, 0 }, { 0, 0, 0 } };
    return ShipMounts{ 2, { 15.0f, -15.0f, 0 }, { 0, 0, 0 } };
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
