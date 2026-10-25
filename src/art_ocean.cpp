// One Hour - sea art: the Aircraft Carrier and the military cargo ship (128x128, ART_BIG), and the two carrier-borne jets (64x64): the naval
// multirole fighter and the light grey airstrike fighter. Same conventions as art_ship.cpp and art_jet.cpp: the craft face +x, centred on the
// canvas (64,64 for the big ships, 32,32 for the jets), painted with paint::Art at scale 1 and lit from the north-west, with a 1 px outline.
// Cyber is pale grey composite with cyan deck lights; Clanker is darker steel with a rust-red waterline and yellow markings. The team colour
// only goes on small stripes, pennants and tail flashes. paint::Art polygons are convex, so every hull and wing is built from convex pieces.
#include "art2.h"
#include "paint.h"

using namespace paint;

namespace {

const Color CV_SIDE = rgb(116, 130, 148), CV_DECK = rgb(206, 214, 220), CV_DARK = rgb(34, 42, 56), CV_CYAN = rgb(110, 232, 255), CV_WHITE = rgb(236, 244, 250);
const Color CV_MARK = rgb(60, 74, 98);   // Cyber's deck markings: slate on the pale deck, so they read
const Color CV_ISL = rgb(220, 228, 234), CV_GLASS = rgb(40, 110, 140);
const Color CK_SIDE = rgb(144, 58, 38), CK_DECK = rgb(112, 116, 118), CK_DARK = rgb(38, 40, 36), CK_YEL = rgb(232, 190, 64), CK_ISL = rgb(136, 138, 132), CK_RUST = rgb(170, 84, 48);
const Color NAV_GREY_CY = rgb(140, 148, 156), NAV_GREY_CK = rgb(116, 122, 126);
const Color AIR_GREY = rgb(150, 158, 166), AIR_INK = rgb(24, 28, 36), WHITE = rgb(255, 255, 255);

// a convex upper-half piece and its mirror about the fuselage axis (y = 32): the jets are drawn as one half and reflected
void symPoly(Art& A, std::vector<P> up, Color base, float bevel = 2.2f, float kk = 0.55f) {
    std::vector<P> lo;
    for (const P& p : up) lo.push_back({p.x, 64 - p.y});
    A.poly(up, base, bevel, kk);
    A.poly(lo, base, bevel, kk);
}
// a stroke and its mirror
void lineS(Art& A, float ax, float ay, float bx, float by, float w, Color col) {
    A.line(ax, ay, bx, by, w, col);
    A.line(ax, 64 - ay, bx, 64 - by, w, col);
}
// a capsule and its mirror
void capS(Art& A, float ax, float ay, float bx, float by, float r, Color base, float bevel = 1.6f, float kk = 0.55f) {
    A.cap(ax, ay, bx, by, r, base, bevel, kk);
    A.cap(ax, 64 - ay, bx, 64 - by, r, base, bevel, kk);
}
// a twin exhaust nozzle: a flared bell, petals round the rim and a hot glow ring (the renderer adds the flame)
void nozzle(Art& A, float cx, float cy, float r, Color bell, Color glow) {
    A.circle(cx, cy, r, bell, 1.2f, 0.6f);
    for (int i = 0; i < 6; i++) {
        float t = i * 1.0471976f;
        A.dot(cx + std::cos(t) * r * 0.74f, cy + std::sin(t) * r * 0.74f, r * 0.2f, shade(bell, 1.6f));
    }
    A.ring(cx, cy, r * 0.56f, 0.6f, glow, 0.6f, 0);
    A.dot(cx, cy, r * 0.42f, rgb(8, 10, 16));
    A.glow(cx, cy, r * 2.4f, rgba(glow, 110));
}

// the half-width of the carrier's flight deck at x (the hull outline is the same shape, 2 px wider): symmetric about y = 64
float carrierHalfWidth(float x) {
    if (x <= 8.5f) return 15.5f;
    if (x <= 97.0f) return 15.5f + (x - 8.5f) * (1.9f / 88.5f);
    if (x <= 112.0f) return 17.4f + (x - 97.0f) * (-7.0f / 15.0f);
    return std::max(0.0f, 10.4f - (x - 112.0f) * 1.3f);
}

// a small light: a glow and a bright dot
void lamp(Art& A, float x, float y, float r, Color c) {
    A.glow(x, y, r * 3.0f, rgba(c, 80));
    A.dot(x, y, r, c);
}

// dashes along a line, n of them with gaps (deck centre line, landing lanes, hazard edges)
void dashes(Art& A, float x0, float y0, float x1, float y1, int n, float w, Color col) {
    for (int i = 0; i < n; i++) {
        float t0 = (i + 0.1f) / n, t1 = (i + 0.62f) / n;
        A.line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, w, col);
    }
}

// a gun on a mount seen from above: a dome, a barrel pointing +x and a sensor pip (as art_ship.cpp's ciws)
void ciws(Art& A, float x, float y, bool cy) {
    A.circle(x, y, 1.6f, cy ? CV_WHITE : CK_RUST, 1.0f, 0.5f);
    A.line(x, y, x + 3.2f, y, 0.9f, rgba(cy ? CV_DARK : CK_DARK, 230));
    A.dot(x, y, 0.45f, cy ? CV_CYAN : CK_YEL);
}

// a lift (elevator) flush with the deck: a framed well with a dark centre seam
void elevator(Art& A, float x, float y, float w, float h, Color deck, Color mk) {
    A.rect(x - 0.6f, y - 0.6f, w + 1.2f, h + 1.2f, 0.6f, rgba(mk, 170), 0.8f, 0.2f);
    A.rect(x, y, w, h, 0.4f, shade(deck, 0.68f), 1.0f, 0.3f);
    A.line(x + w * 0.5f, y + 0.9f, x + w * 0.5f, y + h - 0.9f, 0.5f, rgba(CV_DARK, 150));
}

// a fighter parked on deck, nose +x, about 12 px long and 9 px across: a swept wing each side, a tail fin pair and a canopy
void deckJet(Art& A, float x, float y, Color body, Color canopy) {
    for (int s = -1; s <= 1; s += 2) {
        A.poly({ {x + 1.6f, y}, {x - 2.6f, y + s * 4.6f}, {x - 3.4f, y + s * 4.6f}, {x - 1.0f, y} }, body, 0.5f, 0.4f);
        A.poly({ {x - 2.2f, y + s * 0.6f}, {x - 4.8f, y + s * 3.2f}, {x - 3.8f, y + s * 0.6f} }, body, 0.4f, 0.3f);
    }
    A.cap(x - 3.6f, y, x + 4.4f, y, 1.2f, body, 0.7f, 0.5f);
    A.poly({ {x + 4.0f, y - 1.2f}, {x + 6.8f, y}, {x + 4.0f, y + 1.2f} }, body, 0.4f, 0.3f);
    A.dot(x + 1.2f, y, 0.55f, canopy);
}

// ---------------------------------------------------------------- aircraft carrier

void carrierBody(Art& A, bool cy) {
    Color side = cy ? CV_SIDE : CK_SIDE, deck = cy ? CV_DECK : CK_DECK, mk = cy ? CV_MARK : CK_YEL, dk = cy ? CV_DARK : CK_DARK;
    Color isl = cy ? CV_ISL : CK_ISL;
    // the hull: a flared bow and a square stern, its sides a band of waterline colour round the flight deck
    A.poly({ {6, 46}, {98, 44}, {117, 52}, {125.6f, 64}, {117, 76}, {98, 84}, {6, 82} }, side, 1.8f, 0.6f);
    // the flight deck, with a little bevel on its north-west edges
    A.poly({ {8.5f, 48.5f}, {97, 46.6f}, {112, 53.6f}, {120, 64}, {112, 74.4f}, {97, 81.4f}, {8.5f, 79.5f} }, deck, 1.6f, 0.45f);
    if (cy) {
        // cyan deck lights along both edges, and a cyan seam line
        for (float x = 12; x <= 110; x += 5.0f) {
            lamp(A, x, 64 - carrierHalfWidth(x) + 1.5f, 0.45f, CV_CYAN);
            lamp(A, x, 64 + carrierHalfWidth(x) - 1.5f, 0.45f, CV_CYAN);
        }
        A.line(10, 64, 108, 64, 0.5f, rgba(CV_CYAN, 90));
    } else {
        // yellow edge hazard dashes and a rust streak down the sides of the hull
        dashes(A, 10, 50.8f, 96, 47.4f, 16, 0.9f, mk);
        dashes(A, 10, 77.2f, 96, 80.6f, 16, 0.9f, mk);
        for (float x = 14; x < 104; x += 11) A.line(x, 46.6f, x + 1.4f, 47.8f, 0.7f, rgba(CK_RUST, 170));   // streaks on the hull band
    }
    // angled landing deck to port: its edges are painted lines, its landing lane is dashed and it ends in a line at its forward end
    Color lane = cy ? CV_MARK : CK_YEL;
    A.line(14, 52.2f, 58, 49.2f, 0.9f, lane);
    A.line(14, 60.8f, 58, 57.8f, 0.9f, lane);
    A.line(58, 49.2f, 58, 57.8f, 0.9f, lane);
    dashes(A, 18, 56.5f, 54, 53.5f, 7, 0.7f, rgba(lane, 200));
    // the deck centre line, dashed, from the stern to the bow
    dashes(A, 14, 64, 110, 64, 22, 0.9f, rgba(lane, 235));
    // the big hull number "7" on the starboard quarter
    A.line(36.4f, 67.4f, 42.6f, 67.4f, 1.4f, mk); A.line(42.6f, 67.4f, 38.4f, 76.6f, 1.4f, mk);
    // catapult tracks at the bow, one each side of the centre line
    for (float y : { 59.6f, 68.4f }) {
        A.line(100, y, 119, y, 1.1f, rgba(dk, 255));
        A.line(100, y - 0.9f, 119, y - 0.9f, 0.45f, rgba(lane, 200));
        A.box(111.5f, y, 0.7f, 1.4f, 0.2f, shade(dk, 1.6f), 0.5f, 0.3f);
    }
    // elevators: one on the starboard quarter, one forward on the port side
    elevator(A, 18.5f, 66.5f, 9, 10.5f, deck, mk);
    elevator(A, 70, 49.5f, 9, 9, deck, mk);
    // four parked jets: two port forward, two starboard forward (noses +x, clear of the catapults' ends)
    for (float x : { 88.0f, 100.0f }) {
        deckJet(A, x + 1.0f, 53.5f, rgba(dk, 80), rgba(dk, 80));   // ground shadow
        deckJet(A, x, 52.5f, cy ? rgb(146, 154, 162) : rgb(168, 170, 162), dk);
    }
    deckJet(A, 90.0f, 75.6f, rgba(dk, 80), rgba(dk, 80));
    deckJet(A, 89.0f, 74.6f, cy ? rgb(146, 154, 162) : rgb(168, 170, 162), dk);
    // the island on the starboard side (y > 64): a stepped superstructure with its bridge forward, glazed, and a mast and radar aft
    A.shadowRect(50, 70, 18, 9, 2.2f, 2.2f, 1.8f, 170);
    A.rect(50, 70, 18, 9, 1.6f, isl, 1.8f, 0.55f);
    A.rect(50, 76.6f, 18, 2.6f, 0.5f, shade(isl, 0.55f), 0.8f, 0.4f);   // the south face of the island
    A.rect(63.4f, 70.6f, 4.6f, 6.6f, 0.6f, shade(isl, 1.1f), 1.4f, 0.5f);   // the bridge block
    A.rect(64.2f, 71.4f, 2.8f, 1.5f, 0.3f, cy ? CV_GLASS : rgb(230, 170, 70), 0.8f, 0.3f);
    A.rect(52, 70.5f, 9.5f, 1.3f, 0.3f, cy ? CV_GLASS : rgb(80, 60, 34), 0.8f, 0.3f);
    if (cy) {
        A.ngon(55.5f, 74.4f, 2.7f, 6, 0, CV_WHITE, 1.0f, 0.5f);      // faceted radar
        lamp(A, 55.5f, 74.4f, 0.8f, CV_CYAN);
        A.line(60, 72.6f, 61.6f, 78.0f, 0.5f, rgba(CV_DARK, 150));
    } else {
        A.line(52.5f, 72.6f, 57.5f, 76.4f, 0.7f, CK_DARK); A.line(57.5f, 72.6f, 52.5f, 76.4f, 0.7f, CK_DARK);   // lattice mast
        A.circle(55, 74.5f, 2.3f, rgb(184, 186, 180), 1.0f, 0.5f);   // radar dish
        A.dot(55, 74.5f, 0.6f, CK_DARK);
    }
    A.rect(51.4f, 77.4f, 4.2f, 1.1f, 0.2f, A.team, 0.6f, 0.2f);   // team stripe on the island face
    // the team pennant on the stern
    A.poly({ {7.8f, 61.3f}, {11.4f, 62.6f}, {7.8f, 63.9f} }, A.team, 0.6f, 0, 1);
    // gun positions: a CIWS on the bow, one on the stern quarter and one on the port quarter
    ciws(A, 116.0f, 64, cy);
    ciws(A, 11.6f, 71.5f, cy);
    ciws(A, 11.6f, 56.5f, cy);
    // the stern: a deck edge and tail hook
    A.dot(9.0f, 64, 0.9f, dk);
    A.line(9.0f, 64, 5.0f, 64, 0.6f, rgba(dk, 200));
    if (cy) A.grain(6, 44, 120, 84, 0.04f, 151);
    else {   // weathering: grain over the plating and streaks of rust
        A.grain(6, 44, 120, 84, 0.12f, 153);
        for (float x = 24; x < 100; x += 19) A.line(x, 80.6f, x + 1.0f, 81.8f, 0.7f, rgba(CK_RUST, 150));
    }
}

// ---------------------------------------------------------------- military cargo ship

// a tank seen from above, pointing +x: a hull between two tracks, a round turret and a long barrel
void tankTop(Art& A, float x, float y, Color col, Color trk) {
    A.rect(x - 4.6f, y - 2.5f, 9.2f, 5.0f, 0.8f, col, 1.0f, 0.5f);
    A.line(x - 4.6f, y - 2.6f, x + 4.6f, y - 2.6f, 0.9f, trk);
    A.line(x - 4.6f, y + 2.6f, x + 4.6f, y + 2.6f, 0.9f, trk);
    A.line(x + 1.2f, y, x + 6.8f, y, 0.9f, shade(col, 0.55f));
    A.circle(x - 0.4f, y, 1.9f, shade(col, 1.14f), 0.8f, 0.5f);
    A.dot(x - 0.9f, y - 0.6f, 0.45f, rgba(WHITE, 130));
}

// a truck seen from above: a cargo body under a tilt cover with a lighter cab at the front
void truckTop(Art& A, float x, float y, Color col) {
    A.rect(x - 4.0f, y - 2.1f, 6.6f, 4.2f, 0.6f, shade(col, 0.95f), 0.8f, 0.5f);
    A.line(x - 3.2f, y - 1.6f, x - 3.2f, y + 1.6f, 0.4f, rgba(CV_DARK, 120));
    A.rect(x + 2.6f, y - 2.0f, 2.6f, 4.0f, 0.5f, shade(col, 1.15f), 0.7f, 0.5f);   // cab
    A.dot(x + 4.5f, y - 1.2f, 0.4f, rgba(WHITE, 170)); A.dot(x + 4.5f, y + 1.2f, 0.4f, rgba(WHITE, 170));
}

// a shipping container seen from above, ribbed along its length
void boxTop(Art& A, float x, float y, Color col) {
    A.shadowRect(x - 4.6f, y - 2.3f, 9.2f, 4.6f, 1.0f, 1.0f, 1.2f, 80);
    A.rect(x - 4.6f, y - 2.3f, 9.2f, 4.6f, 0.4f, col, 1.0f, 0.5f);
    for (float xx = x - 3.2f; xx < x + 3.8f; xx += 1.9f) A.line(xx, y - 2.0f, xx, y + 2.0f, 0.4f, rgba(CV_DARK, 80));
}

void cargoBody(Art& A, bool cy) {
    Color side = cy ? rgb(108, 122, 142) : CK_SIDE, deck = cy ? rgb(184, 194, 204) : rgb(128, 122, 102), dk = cy ? CV_DARK : CK_DARK;
    Color acc = cy ? rgb(206, 216, 224) : rgb(122, 124, 116), mk = cy ? CV_CYAN : CK_YEL;
    // the hull: a blunt bow and a square stern, a side band of waterline colour round the deck
    A.poly({ {6, 50}, {98, 49}, {111, 53}, {121, 64}, {111, 75}, {98, 79}, {6, 78} }, side, 1.8f, 0.6f);
    A.poly({ {8.5f, 52}, {97, 51}, {108.5f, 55.4f}, {117.6f, 64}, {108.5f, 72.6f}, {97, 77}, {8.5f, 76} }, deck, 1.6f, 0.45f);
    if (cy) {
        for (float x = 12; x <= 106; x += 6.0f) { lamp(A, x, 53.2f, 0.4f, CV_CYAN); lamp(A, x, 74.8f, 0.4f, CV_CYAN); }
    } else {
        dashes(A, 10, 52.4f, 104, 52.4f, 20, 0.8f, mk);
        dashes(A, 10, 75.6f, 104, 75.6f, 20, 0.8f, mk);
    }
    // the bow ramp: a lighter lowered plate with a hinge seam and chevrons
    A.poly({ {108, 57.5f}, {116.4f, 60.5f}, {116.4f, 67.5f}, {108, 70.5f} }, shade(deck, 1.12f), 1.0f, 0.5f);
    A.line(108.2f, 57.6f, 108.2f, 70.4f, 0.8f, rgba(dk, 200));
    for (float y : { 60.0f, 64.0f, 68.0f }) A.line(110, y, 114.6f, y + 1.0f, 0.4f, rgba(dk, 110));
    // the vehicle deck: tanks, trucks and containers in three rows, pointing forward
    Color olive = cy ? rgb(102, 118, 140) : rgb(104, 114, 74), tan = cy ? rgb(146, 158, 176) : rgb(178, 160, 112), cont = cy ? rgb(58, 88, 134) : rgb(150, 92, 52);
    Color trk = cy ? rgb(40, 48, 62) : rgb(46, 46, 38);
    tankTop(A, 37, 56.5f, olive, trk); tankTop(A, 49, 56.5f, olive, trk); truckTop(A, 62, 56.5f, tan); tankTop(A, 73, 56.5f, olive, trk); tankTop(A, 85, 56.5f, olive, trk);
    boxTop(A, 37, 64, cont); boxTop(A, 48, 64, cont); truckTop(A, 62, 64, olive); boxTop(A, 73, 64, shade(cont, 1.2f)); boxTop(A, 84, 64, cont);
    tankTop(A, 39, 71.5f, tan, trk); truckTop(A, 52, 71.5f, olive); tankTop(A, 64, 71.5f, tan, trk); boxTop(A, 76, 71.5f, cont); tankTop(A, 88, 71.5f, olive, trk);
    // portal cranes at the forward end of the deck, their booms swung over the lanes
    for (float y : { 56.0f, 72.0f }) {
        A.shadowCircle(102, y, 2.4f, 1.0f, 1.2f, 1.2f, 80);
        A.circle(102, y, 2.4f, shade(dk, 2.4f), 1.0f, 0.5f);
        A.line(102, y, 94, y + (y < 64 ? 2 : -2), 1.0f, rgba(cy ? CV_WHITE : CK_YEL, 230));
        A.dot(94, y + (y < 64 ? 2 : -2), 0.8f, rgba(dk, 230));
    }
    // accommodation block aft: a flat roof with the bridge at its forward end, glazed on the sea side, and a funnel
    A.shadowRect(9, 54, 19, 20, 2.0f, 2.0f, 1.8f, 120);
    A.rect(9, 54, 19, 20, 1.4f, acc, 1.8f, 0.55f);
    A.rect(9.5f, 72.2f, 18, 1.8f, 0.4f, shade(acc, 0.7f), 0.8f, 0.4f);
    for (float x = 12; x < 27; x += 3.4f) A.dot(x, 72.6f, 0.5f, cy ? CV_CYAN : rgb(255, 196, 92));   // lit windows along the south face
    A.rect(22.6f, 57, 5.2f, 14, 0.8f, shade(acc, 1.1f), 1.2f, 0.5f);   // the bridge block, forward on the accommodation
    A.rect(26.8f, 58.2f, 1.0f, 11.6f, 0.2f, cy ? CV_GLASS : rgb(230, 170, 70), 0.6f, 0.2f);
    A.line(22.6f, 57.5f, 22.6f, 70.5f, 0.6f, rgba(dk, 110));
    // the funnel: a round stack with a band and a dark top, with a mast beside it
    A.circle(15.5f, 64, 3.4f, cy ? rgb(200, 210, 218) : CK_RUST, 1.4f, 0.5f);
    A.ring(15.5f, 64, 2.6f, 0.6f, cy ? CV_CYAN : CK_DARK, 0.6f, 0.2f);
    A.circle(15.5f, 64, 1.5f, dk, 0.8f, 0.3f);
    A.dot(12.4f, 58.8f, 0.7f, rgb(180, 182, 186));
    // team pennant on the accommodation roof, and a team stripe on the bridge
    A.rect(11.2f, 56.2f, 3.4f, 1.6f, 0.2f, A.team, 0.6f, 0.2f);
    A.rect(22.6f, 59.4f, 1.4f, 1.2f, 0.2f, A.team, 0.4f, 0.1f);
    if (cy) A.grain(6, 48, 120, 80, 0.04f, 161);
    else {
        A.grain(6, 48, 120, 80, 0.12f, 162);
        for (float x = 30; x < 110; x += 17) A.line(x, 76.4f, x + 1.2f, 77.6f, 0.7f, rgba(CK_RUST, 160));   // streaks on the hull band
    }
}

// ---------------------------------------------------------------- carrier jet (naval multirole)

// Both carrier jets: a broad LEX-and-wing shape, twin fins with a team stripe on each, a twin-nozzle tail with a glow ring.
void navalJet(Art& A, bool cy) {
    const Color H = cy ? NAV_GREY_CY : NAV_GREY_CK, DK = rgb(20, 24, 32), GLS = rgb(44, 90, 112), GL = cy ? rgb(110, 232, 255) : rgb(255, 150, 50);
    const Color RAM = rgb(28, 32, 40);
    // the swept wings with a leading-edge root extension, flap seams and a dark intake lip at each root
    symPoly(A, {{42, 29.2f}, {24, 14.8f}, {21.4f, 16.0f}, {30, 29.6f}}, H, 2.2f, 0.6f);
    symPoly(A, {{48, 29.4f}, {41, 27.6f}, {33, 29.2f}}, shade(H, 1.1f), 1.1f, 0.5f);   // LEX blend
    lineS(A, 33, 26.4f, 24.4f, 17.6f, 0.45f, rgba(RAM, 120));
    lineS(A, 26.6f, 28.2f, 25.4f, 26.4f, 0.4f, rgba(RAM, 100));
    if (cy) lineS(A, 42, 29.2f, 24, 14.8f, 0.6f, rgba(CV_CYAN, 200));   // cyan leading-edge line
    // twin tails, canted outward, with a team stripe on each fin
    capS(A, 17.2f, 27.4f, 10.2f, 17.8f, 1.25f, shade(H, 0.95f), 1.0f, 0.5f);
    capS(A, 12.6f, 22.2f, 10.2f, 17.8f, 0.85f, A.team, 0.6f, 0.4f);
    symPoly(A, {{22, 29.6f}, {14.6f, 25.6f}, {13.2f, 27.0f}, {19.4f, 31.0f}}, shade(H, 1.06f), 1.1f, 0.5f);   // stabilators
    // a fuselage with a pointed radome, a canopy and a tail hook
    A.cap(11.4f, 32, 49, 32, 3.2f, shade(H, 1.08f), 2.2f, 0.6f);
    symPoly(A, {{46, 29.4f}, {55, 31.0f}, {59.4f, 32}, {46, 32}}, shade(H, 1.22f), 1.4f, 0.5f);
    A.ell(39.5f, 32, 4.6f, 2.3f, GLS, 1.4f, 0.6f);
    A.ell(38.8f, 31.2f, 2.6f, 0.6f, rgba(WHITE, 200), 1, 0);
    A.line(9.5f, 32, 6.0f, 32, 0.5f, rgba(DK, 200));
    // twin nozzles with their glow rings
    nozzle(A, 9.4f, 28.2f, 2.0f, shade(DK, 2.2f), GL);
    nozzle(A, 9.4f, 35.8f, 2.0f, shade(DK, 2.2f), GL);
    // a dark shoulder line along the fuselage and a rivet row on each wing
    lineS(A, 14, 28.6f, 42, 28.6f, 0.4f, rgba(RAM, 120));
    for (int i = 0; i < 4; i++) { A.dot(29 - i * 2.2f, 18.8f + i * 0.4f, 0.3f, rgba(DK, 170)); A.dot(29 - i * 2.2f, 64 - (18.8f + i * 0.4f), 0.3f, rgba(DK, 170)); }
    // Clanker: canards at the nose
    if (!cy) symPoly(A, {{45.5f, 28.6f}, {52.0f, 25.2f}, {50.8f, 28.8f}}, shade(H, 0.95f), 1.0f, 0.5f);
    // wingtip navigation lights
    A.dot(22.6f, 15.6f, 0.5f, rgb(255, 80, 70)); A.dot(22.6f, 64 - 15.6f, 0.5f, rgb(80, 255, 120));
}

// ---------------------------------------------------------------- airstrike jet (light grey)

// A store hung under a wing, seen from above: a body with a seeker or fuse nose, a cross-bar for the tail fins and a soft shadow to the
// south-east so it reads as hanging below the wing.
void store(Art& A, float x0, float x1, float y, float r, Color body, Color tip) {
    A.soft((int)x0 - 3, (int)(y - r - 4), (int)x1 + 4, (int)(y + r + 5), [=](float px, float py) { return sdCap(px, py, x0 + 1.0f, y + 1.2f, x1 - r + 0.9f, y + 1.2f, r); }, rgba(AIR_INK, 120), 1.6f);
    A.box(x0 + r * 1.5f, y, r * 0.5f, r * 2.1f, 0.15f, shade(body, 0.7f), 0.5f, 0.3f);   // tail fins
    A.cap(x0 + r * 1.2f, y, x1 - r, y, r, body, 0.7f, 0.55f);
    A.circle(x1 - r * 0.6f, y, r * 0.72f, tip, 0.6f, 0.4f);
}

// The strike jet: a light grey livery with darker panel lines, twin tails with a small team flash, twin nozzles, and stores on the pylons.
void strikeJet(Art& A, bool cy) {
    const Color G = AIR_GREY, GLS = rgb(40, 84, 104), GL = rgb(255, 170, 80), RAM = AIR_INK;
    // broad trapezoid wings with a leading-edge root extension, panel seams and a dark intake lip under each root
    symPoly(A, {{44, 29.4f}, {25.5f, 12.9f}, {20.8f, 14.6f}, {32, 30.6f}}, G, 2.4f, 0.6f);
    symPoly(A, {{49, 29.3f}, {42, 27.8f}, {36, 29.6f}}, shade(G, 1.06f), 1.1f, 0.5f);
    lineS(A, 40, 28.8f, 25, 15.8f, 0.5f, rgba(RAM, 110));
    lineS(A, 34.5f, 28.2f, 29.4f, 22.6f, 0.45f, rgba(RAM, 95));
    lineS(A, 28, 26.6f, 24.2f, 17.6f, 0.4f, rgba(RAM, 80));
    A.box(36.4f, 29.6f, 2.6f, 0.7f, 0.2f, AIR_INK, 0.9f, 0.3f);
    A.box(36.4f, 34.4f, 2.6f, 0.7f, 0.2f, AIR_INK, 0.9f, 0.3f);
    // the tailplanes and the fins: Cyber's fins splay out wide (F-15E), Clanker's are shorter and steeper (Su-34 like)
    symPoly(A, {{22, 30.6f}, {12.5f, 26.4f}, {11.2f, 28.6f}, {18.6f, 32}}, shade(G, 0.92f), 1.2f, 0.5f);
    if (cy) {
        capS(A, 19.4f, 27.8f, 12.8f, 15.4f, 1.6f, shade(G, 0.94f), 1.2f, 0.5f);
        capS(A, 14.2f, 19.8f, 12.6f, 16.2f, 1.0f, A.team, 0.6f, 0.4f);   // tail flash
    } else {
        capS(A, 19.4f, 27.8f, 14.0f, 19.0f, 1.5f, shade(G, 0.94f), 1.2f, 0.5f);
        capS(A, 15.4f, 22.6f, 13.8f, 19.4f, 1.0f, A.team, 0.6f, 0.4f);
    }
    // the fuselage with a pointed nose, a bulged middle and a canopy with a glint
    A.cap(11, 32, 50, 32, 3.4f, shade(G, 1.06f), 2.4f, 0.6f);
    symPoly(A, {{47, 28.6f}, {58, 30.6f}, {61.5f, 32}, {47, 32}}, shade(G, 1.1f), 1.6f, 0.55f);
    A.ell(40.5f, 32, 5.2f, 2.4f, GLS, 1.5f, 0.6f);
    A.ell(39.6f, 31.1f, 2.6f, 0.6f, rgba(WHITE, 200), 1, 0);
    A.line(11.2f, 32, 50, 32, 0.5f, rgba(RAM, 90));
    // Clanker: canards on the nose shoulders
    if (!cy) symPoly(A, {{46, 28.6f}, {53.2f, 23.8f}, {51.5f, 28.8f}}, shade(G, 0.92f), 1.0f, 0.5f);
    // twin nozzles and their glow rings
    nozzle(A, 9.6f, 28.8f, 2.1f, shade(AIR_INK, 2.2f), GL);
    nozzle(A, 9.6f, 35.2f, 2.1f, shade(AIR_INK, 2.2f), GL);
    // stores on the pylons: two heavy bombs and a missile on each wing, hung below the wing
    store(A, 24.8f, 35.0f, 19.5f, 1.5f, shade(G, 0.62f), rgb(236, 220, 150));
    store(A, 24.8f, 35.0f, 44.5f, 1.5f, shade(G, 0.62f), rgb(236, 220, 150));
    store(A, 28.6f, 39.2f, 24.6f, 1.5f, shade(G, 0.62f), rgb(236, 220, 150));
    store(A, 28.6f, 39.2f, 39.4f, 1.5f, shade(G, 0.62f), rgb(236, 220, 150));
    store(A, 20.8f, 27.8f, 15.6f, 0.9f, rgb(224, 226, 228), AIR_INK);
    store(A, 20.8f, 27.8f, 48.4f, 0.9f, rgb(224, 226, 228), AIR_INK);
    // wingtip navigation lights
    A.dot(21.6f, 14.0f, 0.5f, rgb(255, 80, 70)); A.dot(21.6f, 50.0f, 0.5f, rgb(80, 255, 120));
}

// Re-lays the sprite over a darker copy of its own alpha, offset by (dx, dy) canvas px: a darker underside edge shows on the south-east
// side of every part, the way a belly in shadow reads from above.
void underShadow(Canvas& c, int dx, int dy, Color col) {
    std::vector<u32> src = c.px;
    Canvas base(c.w, c.h);
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        int sx = x - dx, sy = y - dy;
        if (sx < 0 || sy < 0 || sx >= c.w || sy >= c.h) continue;
        Color s = Canvas::unpack(src[sy * c.w + sx]);
        if (s.a > 0) base.set(x, y, rgba(col, s.a));
    }
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        Color p = Canvas::unpack(src[y * c.w + x]);
        if (p.a > 0) base.set(x, y, p);
    }
    c.px = base.px;
}

}  // namespace

void artCarrierBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    carrierBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
}

void artCargoShipBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    cargoBody(A, cyber);
    c.outline(rgb(8, 10, 14, 200));
}

void artCarrierJetBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team, 0.84f, 5.4f, 6.1f);   // the drawing is laid out about 53 px long; scaled to the 40 px carrier jet and centred
    navalJet(A, cyber);
    underShadow(c, 1, 1, shade(cyber ? NAV_GREY_CY : NAV_GREY_CK, 0.55f));
    c.outline(rgb(8, 10, 14, 200));
}

void artAirstrikeJetBody(Canvas& c, Color team, bool cyber) {
    Art A(c, team);
    strikeJet(A, cyber);
    underShadow(c, 1, 2, shade(AIR_GREY, 0.6f));
    c.outline(rgb(8, 10, 14, 200));
}
