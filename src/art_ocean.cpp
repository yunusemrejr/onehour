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
const Color CV_ISL = rgb(220, 228, 234), CV_GLASS = rgb(30, 52, 70);
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

// ---------------------------------------------------------------- shared hull profiles and deck helpers

// A half-width profile: (x, w) control points along a hull, linear between them, symmetric about y = 64 (the centre line).
struct Prof { float x, w; };
float profileAt(const std::vector<Prof>& t, float x) {
    if (x <= t.front().x) return t.front().w;
    for (size_t i = 1; i < t.size(); i++) if (x <= t[i].x) {
        float u = (x - t[i - 1].x) / (t[i].x - t[i - 1].x);
        return t[i - 1].w + (t[i].w - t[i - 1].w) * u;
    }
    return t.back().w;
}
// the outline of a shape from its profile: the stern edge at the first point, a point at (bowX, 64), every half-width cut back by `inset`
std::vector<P> profileOutline(const std::vector<Prof>& t, float bowX, float inset) {
    std::vector<P> v;
    for (const Prof& p : t) v.push_back({p.x, 64 - std::max(0.0f, p.w - inset)});
    v.push_back({bowX - inset, 64});
    for (size_t i = t.size(); i-- > 0;) v.push_back({t[i].x, 64 + std::max(0.0f, t[i].w - inset)});
    return v;
}
// a lamp: a glow and a bright dot
void lamp(Art& A, float x, float y, float r, Color c) {
    A.glow(x, y, r * 3.0f, rgba(c, 80));
    A.dot(x, y, r, c);
}
// dashes along a line, n of them with gaps (centre lines, lane lines, hazard edges)
void dashes(Art& A, float x0, float y0, float x1, float y1, int n, float w, Color col) {
    for (int i = 0; i < n; i++) {
        float t0 = (i + 0.1f) / n, t1 = (i + 0.62f) / n;
        A.line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1, w, col);
    }
}
// a line along one side of a profile (s = -1 port / north, +1 starboard / south), `off` px in from its edge
void profileEdge(Art& A, const std::vector<Prof>& t, int s, float off, float w, Color col) {
    for (size_t i = 1; i < t.size(); i++) A.line(t[i - 1].x, 64 + s * (t[i - 1].w - off), t[i].x, 64 + s * (t[i].w - off), w, col);
}
// dashes along one side of a profile, following its bends
void profileDashes(Art& A, const std::vector<Prof>& t, int s, float off, float w, Color col) {
    for (size_t i = 1; i < t.size(); i++) {
        float ax = t[i - 1].x, bx = t[i].x, ay = 64 + s * (t[i - 1].w - off), by = 64 + s * (t[i].w - off);
        dashes(A, ax, ay, bx, by, std::max(1, (int)(hyp(bx - ax, by - ay) / 3.6f)), w, col);
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

// ---------------------------------------------------------------- aircraft carrier

// The carrier's numbers are the sim's (data.h), in world px; the canvas is 1.495 world px per px, centred on (64, 64): canvas = 64 + world / 1.495.
// The touchdown point (-62.8, -12.0) is canvas (22, 56); the roll-out stop (-20.9, -15.0) is (50, 54); the catapults run from canvas x 101 to 119
// at y 59.6 and 68.4; the six parking spots are converted in carrierBody. The landing lane's heading is CARRIER_LAND_ANG = -0.071 rad.
const float LANE_SLOPE = -0.071f;
float laneY(float x, float off) { return 56.0f + LANE_SLOPE * (x - 22.0f) + off; }   // the lane's centre line, off px to starboard
// a line across the angled deck, square to the lane, half-length h
void laneCross(Art& A, float x, float h, float w, Color col) {
    float c = laneY(x, 0), n = -LANE_SLOPE;
    A.line(x - n * h, c - h, x + n * h, c + h, w, col);
}

// the flight deck: a long flat deck, widest amidships, its bow squared off with rounded corners (not a pointed warship bow); the hull's flare
// shows past the deck edge and round the bow
const std::vector<Prof> KDECK = { {8.0f, 14.8f}, {30.0f, 15.6f}, {60.0f, 15.8f}, {88.0f, 15.4f}, {104.0f, 14.6f}, {114.0f, 12.6f}, {120.0f, 10.0f}, {123.4f, 6.6f}, {124.8f, 3.6f} };
const std::vector<Prof> KHULL = { {4.5f, 13.6f}, {14.0f, 16.6f}, {44.0f, 18.2f}, {80.0f, 17.8f}, {100.0f, 16.0f}, {114.0f, 13.4f}, {120.0f, 11.2f}, {124.0f, 8.0f}, {126.0f, 4.2f} };

void carrierBody(Art& A, bool cy) {
    const Color deck = cy ? CV_DECK : CK_DECK, mk = cy ? CV_MARK : CK_YEL, dk = cy ? CV_DARK : CK_DARK, isl = cy ? CV_ISL : CK_ISL;
    const Color topside = cy ? rgb(122, 136, 154) : rgb(92, 96, 98), waterline = cy ? rgb(52, 64, 86) : CK_SIDE;
    // the hull: a waterline band all round (navy boot-top, rust-red on Clanker) and the topsides above it, both showing past the deck edge
    A.poly(profileOutline(KHULL, 127.0f, 0), waterline, 1.4f, 0.6f);
    A.poly(profileOutline(KHULL, 127.0f, 1.0f), topside, 1.2f, 0.6f);
    // the flight deck, with its bulwark line and the deck-edge line just inside it (Cyber: cyan lights; Clanker: a thin yellow edge line)
    A.poly(profileOutline(KDECK, 125.2f, 0), deck, 1.6f, 0.45f);
    profileEdge(A, KDECK, -1, 0.5f, 0.6f, rgba(dk, 150));
    profileEdge(A, KDECK, 1, 0.5f, 0.6f, rgba(dk, 150));
    if (cy) {
        for (float x = 12; x <= 110; x += 5.0f) {
            lamp(A, x, 64 - profileAt(KDECK, x) + 1.3f, 0.45f, CV_CYAN);
            lamp(A, x, 64 + profileAt(KDECK, x) - 1.3f, 0.45f, CV_CYAN);
        }
    } else {
        profileEdge(A, KDECK, -1, 1.2f, 0.7f, rgba(mk, 210));
        profileEdge(A, KDECK, 1, 1.2f, 0.7f, rgba(mk, 210));
    }
    dashes(A, 14, 64, 110, 64, 22, 0.9f, rgba(mk, 235));   // the centre line, stern to bow
    A.line(119.0f, 64, 122.5f, 64, 0.6f, rgba(mk, 150));

    // the angled landing deck to port: its lane runs along the sim's landing line through the touchdown point, with the lane's edges, its
    // centre dashes, the landing area's box ends, the aiming bars and four arrestor wires square across it
    const float LH = 5.6f;   // half the lane's width
    A.poly({ {9, laneY(9, -LH)}, {50, laneY(50, -LH)}, {50, laneY(50, LH)}, {9, laneY(9, LH)} }, shade(deck, cy ? 0.92f : 0.86f), 1.0f, 0.4f);
    A.line(9, laneY(9, -LH), 50, laneY(50, -LH), 1.0f, mk);
    A.line(9, laneY(9, LH), 50, laneY(50, LH), 1.0f, mk);
    dashes(A, 12, laneY(12, 0), 46, laneY(46, 0), 8, 0.7f, rgba(mk, 200));
    laneCross(A, 16, LH, 0.7f, mk); laneCross(A, 38, LH, 0.7f, mk);
    laneCross(A, 20.3f, 3.2f, 1.0f, mk); laneCross(A, 23.7f, 3.2f, 1.0f, mk);
    for (int k = 0; k < 4; k++) {
        float x = 26.0f + 3.0f * k;
        laneCross(A, x, 5.0f, 0.5f, rgba(dk, 230));
        A.dot(x - 0.37f, laneY(x, -5.0f), 0.45f, mk); A.dot(x + 0.37f, laneY(x, 5.0f), 0.45f, mk);
    }
    // tyre marks at the touchdown
    A.line(15, laneY(15, -2.0f), 30, laneY(30, -2.0f), 0.8f, rgba(dk, 70));
    A.line(15, laneY(15, 2.0f), 30, laneY(30, 2.0f), 0.8f, rgba(dk, 70));
    // the landing lens at the aft end of the port edge, with its row of lights
    A.rect(9.6f, 49.3f, 2.8f, 1.4f, 0.3f, shade(dk, 1.4f), 0.6f, 0.3f);
    for (int k = 0; k < 3; k++) A.dot(10.4f + k * 1.0f, 50.0f, 0.35f, k == 1 ? rgb(255, 190, 60) : rgb(90, 230, 120));

    // the six parking spots (sim CARRIER_PARK, converted): a box lying along each jet's heading; the jets are drawn on top, live
    static const float PARK[6][3] = { {98.8f, 54.0f, 0.5f}, {86.1f, 54.0f, 0.5f}, {73.4f, 54.2f, 0.5f}, {100.1f, 74.0f, -0.5f}, {87.4f, 74.0f, -0.5f}, {75.4f, 74.4f, -0.5f} };
    for (const auto& s : PARK) {
        float ca = std::cos(s[2]), sa = std::sin(s[2]);
        auto corner = [&](float u, float v) { return P{s[0] + u * ca - v * sa, s[1] + u * sa + v * ca}; };
        P q[4] = { corner(-4.6f, -2.6f), corner(4.6f, -2.6f), corner(4.6f, 2.6f), corner(-4.6f, 2.6f) };
        for (int k = 0; k < 4; k++) A.line(q[k].x, q[k].y, q[(k + 1) % 4].x, q[(k + 1) % 4].y, 0.45f, rgba(mk, 120));
    }
    // the two bow catapults: a track sunk in the deck with a guide line and a shuttle at the launch end; the blast deflector slots behind them
    for (float y : {59.6f, 68.4f}) {
        A.line(101.0f, y, 119.0f, y, 1.1f, rgba(dk, 255));
        A.line(101.0f, y - 0.9f, 119.0f, y - 0.9f, 0.45f, rgba(mk, 200));
        A.box(117.6f, y, 1.0f, 1.5f, 0.2f, shade(dk, 1.6f), 0.5f, 0.3f);
    }
    A.line(93.6f, 56.0f, 93.6f, 63.4f, 0.8f, rgba(dk, 170));
    A.line(93.6f, 64.6f, 93.6f, 72.0f, 0.8f, rgba(dk, 170));
    A.line(99.2f, 56.2f, 102.5f, 59.0f, 0.9f, rgba(dk, 80));   // taxi tracks from the parking to the catapults
    A.line(100.0f, 71.6f, 103.0f, 68.9f, 0.9f, rgba(dk, 80));

    // deck-edge elevators (aft and amidships on the starboard edge, forward on the port side), and the big hull number on the starboard quarter
    elevator(A, 12.0f, 69.6f, 7.4f, 8.8f, deck, mk);
    elevator(A, 29.0f, 71.6f, 7.4f, 7.2f, deck, mk);
    elevator(A, 58.4f, 48.6f, 7.6f, 6.0f, deck, mk);
    A.line(22.4f, 65.6f, 27.6f, 65.6f, 1.5f, mk);
    A.line(27.6f, 65.6f, 24.0f, 74.6f, 1.5f, mk);

    // the island on the starboard side: a stepped superstructure with the flight control and bridge glazed on its forward face, a radar and an
    // exhaust stack aft
    A.shadowRect(40.0f, 69.4f, 21.4f, 9.6f, 2.2f, 2.2f, 1.8f, 170);
    A.rect(40.0f, 69.4f, 21.4f, 9.6f, 1.2f, isl, 1.8f, 0.55f);                                 // the lower island
    A.rect(41.4f, 77.0f, 14.0f, 1.2f, 0.2f, cy ? CV_GLASS : rgb(60, 48, 30), 0.5f, 0.2f);      // its glazed outboard face
    A.rect(56.6f, 77.0f, 3.4f, 1.2f, 0.2f, A.team, 0.6f, 0.2f);                               // the team stripe
    A.rect(45.6f, 70.6f, 10.4f, 6.0f, 0.8f, shade(isl, 1.08f), 1.4f, 0.5f);                    // the second level
    A.rect(52.6f, 70.9f, 6.6f, 5.4f, 0.6f, shade(isl, 1.16f), 1.4f, 0.5f);                     // the pilot house
    A.rect(58.2f, 71.4f, 1.1f, 4.6f, 0.2f, cy ? CV_GLASS : rgb(230, 170, 70), 0.6f, 0.2f);     // the flight control windows, facing the deck
    A.rect(37.4f, 71.6f, 2.8f, 4.8f, 0.5f, shade(isl, 0.78f), 0.8f, 0.4f);                    // the exhaust stack, aft and outboard
    A.rect(38.4f, 72.4f, 1.2f, 3.2f, 0.2f, rgb(40, 40, 42), 0.4f, 0.1f);
    A.line(44.6f, 70.8f, 42.4f, 66.4f, 0.35f, rgba(dk, 210));                                // antennas on the island's aft end
    A.line(54.4f, 70.8f, 55.6f, 66.6f, 0.35f, rgba(dk, 210));
    if (cy) {
        A.rect(46.0f, 71.2f, 4.8f, 4.2f, 0.3f, CV_WHITE, 1.0f, 0.5f);                         // a flat radar panel with its grid
        A.line(46.0f, 73.3f, 50.8f, 73.3f, 0.3f, rgba(CV_MARK, 130));
        A.line(48.4f, 71.2f, 48.4f, 75.4f, 0.3f, rgba(CV_MARK, 130));
        lamp(A, 49.8f, 70.9f, 0.5f, CV_CYAN);
    } else {
        A.line(45.4f, 71.6f, 48.6f, 74.8f, 0.6f, CK_DARK); A.line(48.6f, 71.6f, 45.4f, 74.8f, 0.6f, CK_DARK);   // lattice mast
        A.circle(50.4f, 73.4f, 1.9f, rgb(184, 186, 180), 1.0f, 0.5f);                          // radar dish
        A.dot(50.4f, 73.4f, 0.6f, CK_DARK);
        lamp(A, 49.8f, 70.9f, 0.5f, rgb(255, 80, 60));
    }

    // CIWS domes on the bow corners and missile sponsons on the stern corners, outside the deck edge
    ciws(A, 117.0f, 51.8f, cy);
    ciws(A, 117.0f, 76.2f, cy);
    for (float y : {44.6f, 79.8f}) {
        A.rect(3.2f, y, 4.8f, 3.6f, 0.5f, shade(topside, 1.25f), 1.0f, 0.5f);
        for (int k = 0; k < 3; k++) { A.dot(4.4f + 1.5f * k, y + 1.1f, 0.45f, dk); A.dot(4.4f + 1.5f * k, y + 2.5f, 0.45f, dk); }
    }
    // the team pennant on the stern, and the bow light
    A.poly({ {7.8f, 61.3f}, {11.4f, 62.6f}, {7.8f, 63.9f} }, A.team, 0.6f, 0, 1);
    lamp(A, 123.0f, 64, 0.35f, rgb(255, 255, 255));
    if (cy) A.grain(4, 44, 124, 84, 0.04f, 151);
    else {   // weathering: grain over the plating and streaks of rust along the waterline
        A.grain(4, 44, 124, 84, 0.12f, 153);
        for (float x = 24; x < 100; x += 19) A.line(x, 80.6f, x + 1.0f, 81.8f, 0.7f, rgba(rgb(96, 34, 20), 150));
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

// the military Ro-Ro: a flat vehicle deck, a stern ramp lowered past the transom, the accommodation forward with the bridge on its forward end
const std::vector<Prof> CGO_HULL = { {6.0f, 12.4f}, {30.0f, 13.2f}, {80.0f, 13.2f}, {100.0f, 12.6f}, {112.0f, 10.8f}, {118.0f, 7.8f} };
const std::vector<Prof> CGO_DECK = { {8.0f, 11.4f}, {30.0f, 11.8f}, {80.0f, 11.8f}, {100.0f, 11.2f}, {110.0f, 9.6f}, {116.0f, 6.8f} };

void cargoBody(Art& A, bool cy) {
    const Color topside = cy ? rgb(122, 136, 154) : rgb(92, 96, 98), waterline = cy ? rgb(52, 64, 86) : CK_SIDE;
    const Color deck = cy ? rgb(184, 194, 204) : rgb(128, 122, 102), dk = cy ? CV_DARK : CK_DARK, mk = cy ? CV_CYAN : CK_YEL;
    const Color acc = cy ? rgb(206, 216, 224) : rgb(122, 124, 116), glass = cy ? CV_GLASS : rgb(230, 170, 70);
    const Color olive = cy ? rgb(102, 118, 140) : rgb(104, 114, 74), tan = cy ? rgb(146, 158, 176) : rgb(178, 160, 112), cont = cy ? rgb(58, 88, 134) : rgb(150, 92, 52);
    const Color trk = cy ? rgb(40, 48, 62) : rgb(46, 46, 38);
    // the hull: a waterline band and the topsides above it, a blunt bow and a square stern; the vehicle deck inset within
    A.poly(profileOutline(CGO_HULL, 122.0f, 0), waterline, 1.4f, 0.6f);
    A.poly(profileOutline(CGO_HULL, 122.0f, 1.0f), topside, 1.2f, 0.6f);
    A.poly(profileOutline(CGO_DECK, 120.0f, 0), deck, 1.6f, 0.45f);
    if (cy) {
        for (float x = 12; x <= 110; x += 6.0f) {
            lamp(A, x, 64 - profileAt(CGO_DECK, x) + 1.2f, 0.4f, CV_CYAN);
            lamp(A, x, 64 + profileAt(CGO_DECK, x) - 1.2f, 0.4f, CV_CYAN);
        }
    } else {
        profileDashes(A, CGO_DECK, -1, 1.2f, 0.8f, mk);
        profileDashes(A, CGO_DECK, 1, 1.2f, 0.8f, mk);
    }
    // the stern ramp: the open stern well, and the ramp lowered past the transom with its hinge and grip bars
    A.rect(6.0f, 58.6f, 3.0f, 11.0f, 0.2f, shade(dk, 1.5f), 0.6f, 0.2f);
    A.poly({ {6.6f, 58.4f}, {2.0f, 60.4f}, {2.0f, 67.6f}, {6.6f, 69.6f} }, shade(deck, 1.1f), 1.0f, 0.5f);
    A.line(6.4f, 58.8f, 6.4f, 69.2f, 0.6f, rgba(dk, 200));
    for (float x : {3.0f, 4.4f, 5.7f}) A.line(x, 60.6f, x, 67.4f, 0.35f, rgba(dk, 120));
    // the vehicle deck: tanks, trucks and containers in three rows, all pointing forward
    tankTop(A, 15.6f, 56.2f, olive, trk); tankTop(A, 27.2f, 56.2f, olive, trk); truckTop(A, 38.8f, 56.2f, tan); tankTop(A, 50.4f, 56.2f, tan, trk); tankTop(A, 62.0f, 56.2f, olive, trk); truckTop(A, 73.6f, 56.2f, olive);
    boxTop(A, 15.6f, 64.0f, cont); boxTop(A, 27.2f, 64.0f, shade(cont, 1.2f)); truckTop(A, 38.8f, 64.0f, olive); boxTop(A, 50.4f, 64.0f, cont); boxTop(A, 62.0f, 64.0f, cont); boxTop(A, 73.6f, 64.0f, shade(cont, 1.2f));
    tankTop(A, 15.6f, 71.8f, tan, trk); truckTop(A, 27.2f, 71.8f, olive); tankTop(A, 38.8f, 71.8f, olive, trk); boxTop(A, 50.4f, 71.8f, cont); tankTop(A, 62.0f, 71.8f, tan, trk); tankTop(A, 73.6f, 71.8f, olive, trk);
    // two cranes at the outboard edges of the deck, forward of the vehicles, their booms swung over the lanes
    for (float y : {55.6f, 72.4f}) {
        A.shadowCircle(83.0f, y, 2.2f, 1.0f, 1.2f, 1.2f, 80);
        A.circle(83.0f, y, 2.2f, shade(dk, 2.4f), 1.0f, 0.5f);
        float tip = y < 64 ? y + 4.0f : y - 4.0f;
        A.line(83.0f, y, 76.6f, tip, 1.0f, rgba(cy ? CV_WHITE : CK_YEL, 230));
        A.dot(76.6f, tip, 0.8f, rgba(dk, 230));
    }
    // the accommodation block forward: the bridge on its forward end, glazed, with lit windows on the south face and the funnel aft
    A.shadowRect(88.2f, 56.4f, 17.2f, 15.2f, 2.0f, 2.0f, 1.8f, 120);
    A.rect(88.2f, 56.4f, 17.2f, 15.2f, 1.4f, acc, 1.8f, 0.55f);
    A.rect(88.2f, 70.6f, 17.2f, 1.0f, 0.2f, shade(acc, 0.7f), 0.6f, 0.3f);
    for (float x = 90.0f; x < 104.0f; x += 3.4f) A.dot(x, 71.1f, 0.5f, cy ? CV_CYAN : rgb(255, 196, 92));
    A.rect(100.6f, 57.6f, 5.2f, 12.0f, 1.0f, shade(acc, 1.12f), 1.2f, 0.5f);                  // the bridge
    A.rect(104.6f, 58.4f, 1.0f, 10.4f, 0.2f, glass, 0.6f, 0.2f);                              // its forward windows
    A.circle(96.0f, 60.0f, 1.2f, shade(dk, 2.6f), 0.6f, 0.4f);                                // a radar scanner on the roof
    A.circle(92.4f, 64.0f, 2.6f, cy ? rgb(200, 210, 218) : CK_RUST, 1.2f, 0.5f);              // the funnel
    A.ring(92.4f, 64.0f, 1.9f, 0.5f, cy ? CV_CYAN : CK_DARK, 0.6f, 0.2f);
    A.circle(92.4f, 64.0f, 1.2f, dk, 0.8f, 0.3f);
    A.rect(89.2f, 57.2f, 3.4f, 1.6f, 0.2f, A.team, 0.6f, 0.2f);                               // team pennant on the roof
    if (cy) A.grain(4, 48, 124, 80, 0.04f, 161);
    else {
        A.grain(4, 48, 124, 80, 0.12f, 162);
        for (float x = 30; x < 110; x += 17) A.line(x, 76.4f, x + 1.2f, 77.6f, 0.7f, rgba(rgb(96, 34, 20), 160));   // streaks on the hull band
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
