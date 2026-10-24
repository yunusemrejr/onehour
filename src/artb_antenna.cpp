#include "bld.h"

// Structure art for the Healing Antenna: a cheap, fragile 1x1 mast that heals the units and buildings around it and slowly scrubs radiation.
// Cyber is a slim white composite mast on a hex plinth: a cyan emitter dish at the top, a glowing green cross on a dark glass panel and
// thin whips. Clanker is a rugged olive lattice mast on a riveted plate with a battery box, a red-cross medical board, rust and
// a green lamp on top. Both carry the owner's colour on the base.

namespace {

const Color H_GRN = rgb(120, 255, 168), H_TEAL = rgb(64, 226, 214);
const Color H_WHITE = rgb(222, 230, 238), H_GREY = rgb(150, 162, 178);

// a cable with a slack sag between two points (the sag is a dip at mid-span)
void sagCable(Art& a, float x0, float y0, float x1, float y1, float sag, float w, Color col) {
    const int n = 6;
    for (int i = 0; i < n; i++) {
        float t0 = i / (float)n, t1 = (i + 1) / (float)n;
        float s0 = sag * 4 * t0 * (1 - t0), s1 = sag * 4 * t1 * (1 - t1);
        a.line(x0 + (x1 - x0) * t0, y0 + (y1 - y0) * t0 + s0, x0 + (x1 - x0) * t1, y0 + (y1 - y0) * t1 + s1, w, col);
    }
}

void antennaCyber(Bld& b) {
    Art& a = b.a;
    const Color plinth = rgb(96, 108, 126), top = rgb(176, 186, 198), glass = rgb(26, 36, 50);
    // heal field on the ground round the plinth, and the plinth's soft shadow
    a.glow(16, 22, 15, rgba(H_GRN, 40));
    a.ring(16, 24, 13.5f, 0.9f, rgba(H_GRN, 120), 0.6f, 0);
    a.shadowCircle(16, 22, 10.5f, 3, 3, 3, 90);
    // hex plinth: a skirt band under a lit top face with an inset ring
    a.ngon(16, 23.5f, 10, 6, 0, plinth, 1.6f, 0.5f);
    a.ngon(16, 19, 10, 6, 0, top, 2.0f, 0.6f);
    a.ngon(16, 19, 8.0f, 6, 0, rgb(60, 70, 86), 1.0f, 0.2f);
    a.ngon(16, 19, 7.2f, 6, 0, rgb(124, 138, 156), 1.0f, 0.3f);
    b.light(10.8f, 19.0f, 0.8f, H_GRN, 120);
    b.light(21.2f, 19.0f, 0.8f, H_TEAL, 110);
    b.mark(11.5f, 28.4f, 9, 2.2f);   // owner's colour on the skirt
    // the slim composite mast, with a shadow falling south-east across the plinth
    a.line(17.4f, 19, 19.2f, 2, 2.6f, rgba(BLACK, 60));
    a.cap(16, 19, 16, 2, 1.9f, H_WHITE, 1.0f, 0.6f);
    a.cap(16, 2, 16, -6, 1.2f, H_WHITE, 0.8f, 0.6f);
    a.line(14.2f, 10.0f, 17.8f, 10.0f, 0.8f, rgba(rgb(60, 70, 86), 200));   // collars
    a.line(14.2f, 15.0f, 17.8f, 15.0f, 0.8f, rgba(rgb(60, 70, 86), 200));
    // heal panel: a dark glass board with a glowing green cross
    a.glow(16, 6.5f, 9, rgba(H_GRN, 60));
    a.rect(11.0f, 1.5f, 10, 10, 1.4f, H_GREY, 1.0f, 0.4f);
    a.rect(11.9f, 2.4f, 8.2f, 8.2f, 0.8f, glass, 1.0f, 0.3f);
    a.glow(16, 6.5f, 4.5f, rgba(H_GRN, 50));
    a.rect(15.0f, 3.2f, 2.2f, 6.6f, 0.4f, H_GRN, 0.6f, 0.2f);
    a.rect(12.7f, 5.2f, 6.6f, 2.6f, 0.4f, H_GRN, 0.6f, 0.2f);
    b.light(13.5f, 10.0f, 0.5f, H_TEAL, 90);
    b.light(18.5f, 10.0f, 0.5f, H_TEAL, 90);
    // the emitter dish on top: a white rim round a dark teal well that glows
    a.glow(16, -6, 9, rgba(H_TEAL, 70));
    a.ell(16, -6, 7.5f, 3.4f, top, 1.4f, 0.6f);
    a.ell(16, -6, 6.2f, 2.5f, rgb(20, 60, 74), 0.8f, 0.3f);
    a.ell(16, -6, 4.6f, 1.8f, rgb(40, 150, 150), 0.8f, 0.3f);
    a.dot(16, -6, 1.3f, rgb(214, 255, 250));
    // whips: thin rods off the dish, with tiny tip lights
    a.cap(16, -6, 16, -17, 0.45f, H_WHITE, 0.3f, 0.4f);
    a.cap(9.8f, -6, 8.0f, -16, 0.4f, H_GREY, 0.3f, 0.4f);
    a.cap(22.2f, -6, 24.0f, -15, 0.4f, H_GREY, 0.3f, 0.4f);
    b.light(16, -17.5f, 0.8f, H_GRN, 150);
    b.light(8.0f, -16.5f, 0.6f, H_TEAL, 110);
    b.light(24.0f, -15.5f, 0.6f, H_TEAL, 110);
}

void antennaClanker(Bld& b) {
    Art& a = b.a;
    const Color cream = rgb(226, 220, 200), redc = rgb(206, 52, 42), batt = rgb(44, 46, 42), rivet = rgb(74, 78, 66);
    // ground shadow and the riveted base plate
    a.shadowRect(3, 15, 26, 14, 3, 3, 3, 100);
    a.rect(3, 15, 26, 14, 1.2f, K_OLIVE, 2.0f, 0.5f);
    a.rect(3.8f, 15.8f, 24.4f, 12.4f, 0.8f, shade(K_OLIVE, 1.08f), 1.0f, 0.2f);
    for (float x : { 5.5f, 26.5f }) for (float y : { 18.0f, 27.0f }) { a.dot(x, y, 0.9f, rivet); a.dot(x - 0.3f, y - 0.3f, 0.35f, rgba(WHITE, 110)); }
    a.dot(16.0f, 17.2f, 0.7f, rivet); a.dot(16.0f, 27.5f, 0.7f, rivet);
    b.rust(3, 15, 29, 29, 7, 90);
    // battery box on the west side, its terminals on top and a hazard label
    a.shadowRect(3.8f, 19.4f, 7.0f, 7.0f, 1.5f, 1.2f, 1.5f, 80);
    a.rect(4.0f, 19.5f, 7.0f, 7.0f, 0.8f, batt, 1.2f, 0.4f);
    a.rect(4.6f, 22.4f, 5.8f, 1.6f, 0.3f, K_YEL, 0.6f, 0.2f);
    a.dot(6.0f, 19.6f, 0.8f, K_RED);
    a.dot(9.4f, 19.6f, 0.8f, rgb(196, 196, 190));
    // cables from the battery to the mast foot, sagging
    sagCable(a, 8.0f, 19.2f, 13.6f, 17.0f, 1.8f, 0.9f, rgb(24, 24, 22));
    sagCable(a, 11.0f, 24.0f, 17.0f, 24.5f, 0.6f, 0.9f, rgb(24, 24, 22));
    // lattice mast: two rust-olive legs tapering to the top, with a rung at every bay and a dark X in each bay
    const Color leg = rgb(134, 124, 90), brace = rgb(64, 62, 46), steelD = rgb(84, 86, 84);
    a.line(16.6f, 17.0f, 16.8f, -4.0f, 1.8f, rgba(BLACK, 50));
    a.cap(12.6f, 17.0f, 14.4f, -12.0f, 0.95f, leg, 0.8f, 0.6f);
    a.cap(19.4f, 17.0f, 17.6f, -12.0f, 0.95f, leg, 0.8f, 0.6f);
    auto hw = [](float y) { return 1.6f + 1.8f * (y + 12.0f) / 29.0f; };   // half-width of the mast at height y
    const float bays[6] = { 17.0f, 11.0f, 5.0f, -1.0f, -7.0f, -12.0f };
    for (float y : bays) a.cap(16 - hw(y), y, 16 + hw(y), y, 0.45f, brace, 0.4f, 0.3f);
    for (int i = 0; i < 5; i++) {
        float y0 = bays[i], y1 = bays[i + 1];
        a.cap(16 - hw(y0), y0, 16 + hw(y1), y1, 0.35f, brace, 0.3f, 0.3f);
        a.cap(16 + hw(y0), y0, 16 - hw(y1), y1, 0.35f, brace, 0.3f, 0.3f);
    }
    // medical board bolted across the legs: cream panel, red cross, dark frame
    a.rect(10.2f, 2.4f, 11.6f, 11.6f, 1.0f, rgb(60, 58, 50), 1.0f, 0.3f);
    a.rect(10.9f, 3.1f, 10.2f, 10.2f, 0.8f, cream, 1.2f, 0.3f);
    a.rect(14.9f, 4.8f, 2.2f, 6.8f, 0.3f, redc, 0.6f, 0.2f);
    a.rect(12.9f, 7.1f, 6.2f, 2.2f, 0.3f, redc, 0.6f, 0.2f);
    for (float x : { 11.8f, 20.2f }) for (float y : { 4.2f, 12.2f }) a.dot(x, y, 0.5f, steelD);   // board bolts
    // emitter horn and a green lamp on the top
    a.shadowRect(11.5f, -15.5f, 9.0f, 4.0f, 1.2f, 1.0f, 1.2f, 80);
    a.rect(11.5f, -15.5f, 9.0f, 4.0f, 0.8f, K_OLIVE_D, 1.2f, 0.5f);
    a.rect(12.6f, -14.6f, 6.8f, 1.6f, 0.3f, rgba(BLACK, 90), 0.4f, 0);
    b.light(16.0f, -18.5f, 1.2f, H_GRN, 150);
    b.mark(21.6f, 17.2f, 5.6f, 2.2f);   // owner's colour on the plate
}

}  // namespace

// The Healing Antenna's 1x1 structure, painted into 'c' and its team mask into 'mask'. Always true.
bool artHealAntenna(Canvas& c, Canvas& mask, bool cyber, u32 seed) {
    Bld b(c, mask, TILE, TILE, cyber, seed);
    if (cyber) antennaCyber(b);
    else antennaClanker(b);
    b.a.grain(0, 0, (float)TILE, (float)TILE, 0.08f, seed ^ 0x5bd1e995u);
    c.outline(rgb(8, 10, 14, 150));
    return true;
}
