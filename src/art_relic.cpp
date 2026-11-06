#include "art.h"
#include "paint.h"

using namespace paint;

// The Relic army's special units: old but formidable war machines of a long-gone third army, restored to service in its abandoned,
// jungle-overgrown bases (a Relic factory captured by a player builds them). Same conventions as the units in art.cpp: 64x64 canvas, facing +x,
// centred at (32,32), seen from straight above and lit from the north-west. Weathered warm-grey and ochre armour, bronze fittings gone green
// with verdigris, dim amber lamps and faded stencils; a light touch of moss in the crevices. The owner's colour appears only as roundels.
namespace {

const Color RL_ARM = rgb(150, 140, 118);   // weathered warm-grey plate
const Color RL_OCH = rgb(178, 144, 94);    // ochre paint, sun-faded
const Color RL_DRK = rgb(44, 40, 34);      // recesses, track shoes, bores
const Color RL_BRZ = rgb(164, 120, 66);    // bronze fittings
const Color RL_VER = rgb(104, 172, 144);   // verdigris on the bronze
const Color RL_AMB = rgb(255, 178, 76);    // dim amber lamps
const Color RL_STN = rgb(226, 210, 164);   // faded stencil paint
const Color RL_MOSS = rgb(92, 124, 58);    // moss in the crevices
const Color RL_GLASS = rgb(40, 60, 66);    // armoured canopy glass

// the player's roundel, as art.cpp paints it (its teamDot is file-static there, so it is copied here)
void teamDot(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 0.9f, rgba(rgb(18, 20, 24), 225), 0.6f, 0.15f);
    A.circle(cx, cy, r, shade(A.team, 0.94f), 0.8f, 0.22f);
    A.ring(cx, cy, r * 0.66f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.42f), 210), 0.4f, 0);
    A.dot(cx, cy, r * 0.28f, rgba(shade(A.team, 0.7f), 220));
}

// enhance-style local contrast (an unsharp mask on the opaque pixels), as the other self-painting unit sprites get
void sharpen(Canvas& c, float amount) {
    std::vector<u32> src = c.px;
    auto at = [&](int x, int y) { return Canvas::unpack(src[clampi(y, 0, c.h - 1) * c.w + clampi(x, 0, c.w - 1)]); };
    for (int y = 0; y < c.h; y++) for (int x = 0; x < c.w; x++) {
        Color p = at(x, y);
        if (p.a < 200) continue;
        float r = 0, g = 0, b = 0, w = 0;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) {
            Color q = at(x + dx, y + dy);
            if (q.a < 200) continue;
            float k = (dx == 0 && dy == 0) ? 2.0f : 1.0f;
            r += q.r * k; g += q.g * k; b += q.b * k; w += k;
        }
        r /= w; g /= w; b /= w;
        Color o{(u8)clampf(p.r + amount * (p.r - r), 0, 255), (u8)clampf(p.g + amount * (p.g - g), 0, 255), (u8)clampf(p.b + amount * (p.b - b), 0, 255), p.a};
        c.px[y * c.w + x] = Canvas::pack(o);
    }
}

// a soft verdigris or moss stain on the paint (feathered, so it sits in the metal rather than on top of it)
void stain(Art& A, float cx, float cy, float r, Color col) {
    float X0 = A.X(cx), Y0 = A.Y(cy), R = r * A.k, f = 1.6f * A.k;
    A.soft((int)(X0 - R - f), (int)(Y0 - R - f), (int)(X0 + R + f), (int)(Y0 + R + f), [=](float px, float py) { return hyp(px - X0, py - Y0) - R; }, col, f);
}

// a bronze bolt head: a flat dot with a verdigris speck on every other one, so the fittings look aged rather than polished
void bolt(Art& A, float x, float y, bool aged) {
    A.dot(x, y, 0.65f, RL_BRZ);
    if (aged) A.dot(x + 0.2f, y + 0.2f, 0.3f, rgba(RL_VER, 230));
}

// the track of the Juggernaut: a wide steel belt with cross grousers that slide with the cycle, two staggered rows of road wheels and a bronze
// idler and sprocket at its ends. s is -1 for the northern track, +1 for the southern one (used for the road-wheel stagger)
void juggTrack(Art& A, float cy, int s, float ph) {
    const float cx = 31.8f, hl = 27.0f, hh = 5.2f;
    A.box(cx, cy, hl, hh, 1.6f, RL_DRK, 1.6f, 0.35f);
    for (float x = cx - hl + 2.4f + ph * 4.0f; x < cx + hl - 2.0f; x += 4.0f) A.line(x, cy - hh + 0.7f, x, cy + hh - 0.7f, 0.9f, rgba(shade(RL_DRK, 2.6f), 170));
    float off = s < 0 ? 0 : 2.6f;   // the outer and inner road-wheel rows are staggered half a pitch apart
    for (int i = 0; i < 6; i++) {
        float x = cx - hl + 7.0f + i * 7.6f + off;
        A.circle(x, cy - 2.0f, 1.9f, shade(RL_ARM, 0.72f), 0.9f, 0.5f);
        A.dot(x, cy - 2.0f, 0.55f, RL_DRK);
        if (i < 5) { A.circle(x + 3.8f, cy + 2.0f, 1.9f, shade(RL_ARM, 0.72f), 0.9f, 0.5f); A.dot(x + 3.8f, cy + 2.0f, 0.55f, RL_DRK); }
    }
    float sx = cx - hl + hh * 0.9f, ix = cx + hl - hh * 0.9f;   // sprocket (rear) and idler (front)
    A.circle(sx, cy, hh * 0.85f, shade(RL_ARM, 0.82f), 1.0f, 0.45f);
    A.circle(ix, cy, hh * 0.85f, RL_BRZ, 1.0f, 0.45f);
    for (int i = 0; i < 6; i++) { float a = i * 1.0472f + ph * 1.0472f; A.dot(sx + std::cos(a) * hh * 0.5f, cy + std::sin(a) * hh * 0.5f, 0.5f, rgba(RL_DRK, 220)); }
    A.dot(ix, cy, hh * 0.3f, RL_DRK);
    stain(A, ix, cy, 2.4f, rgba(RL_VER, 90));
}

// a hatch with its periscope rack: a bronze-rimmed ring, a dished centre and a glint, plus a dark periscope block beside it
void hatchPeriscope(Art& A, float cx, float cy, float r, float px) {
    A.circle(cx, cy, r, RL_ARM, 1.6f, 0.55f);
    A.ring(cx, cy, r * 0.92f, 0.5f, RL_BRZ, 0.6f, 0.2f);
    A.circle(cx, cy, r * 0.6f, shade(RL_ARM, 0.7f), 1.2f, 0.5f);
    A.dot(cx - r * 0.2f, cy - r * 0.2f, r * 0.14f, shade(RL_ARM, 1.6f));
    A.box(px, cy, 0.9f, 1.5f, 0.4f, RL_DRK, 0.9f, 0.4f);
    A.dot(px, cy - 0.7f, 0.45f, rgb(120, 150, 150));
}

void juggernaut(Art& A, int frame) {
    float ph = frame * 0.25f;   // the track cycle: 0, a quarter and a half of a pitch
    for (int s = -1; s <= 1; s += 2) juggTrack(A, 32 + s * 16.2f, s, ph);
    // the hull: a low, wide slab with a sloped nose, its fenders reaching out over the tracks
    A.poly({ {6, 20.2f}, {38, 18.2f}, {50, 20.4f}, {57, 25.0f}, {59.2f, 32}, {57, 39.0f}, {50, 43.6f}, {38, 45.8f}, {6, 43.8f} }, RL_ARM, 3.0f, 0.62f);
    // sloped glacis, seamed into plates
    A.poly({ {38, 20.8f}, {49.5f, 23.6f}, {54.8f, 28.4f}, {56.2f, 32}, {54.8f, 35.6f}, {49.5f, 40.4f}, {38, 43.2f} }, shade(RL_ARM, 1.08f), 1.5f, 0.5f);
    A.line(38, 20.8f, 49.5f, 23.6f, 0.6f, rgba(RL_DRK, 170)); A.line(38, 43.2f, 49.5f, 40.4f, 0.6f, rgba(RL_DRK, 170));
    A.line(49.5f, 23.6f, 54.8f, 28.4f, 0.6f, rgba(RL_DRK, 150)); A.line(49.5f, 40.4f, 54.8f, 35.6f, 0.6f, rgba(RL_DRK, 150));
    // side skirts over the inner half of each track, bolted along their length, a faded stencil bar on the north one
    for (int s = -1; s <= 1; s += 2) {
        float sy = 32 + s * 11.6f;
        A.box(30.0f, sy, 20.5f, 2.2f, 0.6f, RL_OCH, 1.4f, 0.5f);
        for (float x = 10; x <= 50; x += 4.4f) bolt(A, x, sy, (int)x % 9 == 0);
    }
    A.line(38.0f, 20.4f, 45.0f, 20.4f, 0.8f, rgba(RL_STN, 150));
    A.line(12.0f, 20.4f, 17.0f, 20.4f, 0.8f, rgba(RL_STN, 120));
    teamDot(A, 24.4f, 20.4f, 1.7f);   // the owner's roundel on the north skirt
    teamDot(A, 24.4f, 43.6f, 1.7f);   // and on the south one
    // engine deck at the rear: louvred grille inside a bronze frame
    A.box(13.0f, 32, 6.4f, 10.6f, 1.2f, shade(RL_ARM, 0.9f), 2.0f, 0.5f);
    A.box(13.0f, 32, 4.6f, 8.6f, 0.8f, RL_DRK, 1.0f, 0.3f);
    for (int i = 0; i < 6; i++) A.line(9.2f + i * 1.9f, 24.0f, 9.2f + i * 1.9f, 40.0f, 0.9f, rgba(shade(RL_ARM, 1.2f), 210));
    for (int s = -1; s <= 1; s += 2) {   // the exhaust stacks at the rear corners: bronze collars and dark mouths
        float sy = 32 + s * 7.8f;
        A.circle(7.4f, sy, 2.3f, RL_BRZ, 1.4f, 0.5f); A.dot(7.4f, sy, 1.1f, RL_DRK);
        stain(A, 7.4f, sy + s * 1.2f, 1.9f, rgba(RL_VER, 120));
    }
    // front deck: the driver's and the loader's hatches, periscopes, and a smoke discharger bank on each front corner
    hatchPeriscope(A, 44.0f, 27.4f, 2.4f, 39.2f);
    hatchPeriscope(A, 44.0f, 36.6f, 2.4f, 39.2f);
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 11.6f;
        A.box(48.4f, y, 2.6f, 1.7f, 0.4f, RL_DRK, 1.0f, 0.4f);
        for (int k = -1; k <= 1; k++) A.dot(50.2f, y + k * 1.0f, 0.5f, rgb(20, 18, 16));
    }
    // the turret ring: a bronze bearing with its bolt circle, showing round the turret's housing
    A.ring(32, 32, 14.2f, 1.2f, shade(RL_DRK, 1.5f), 0.6f, 0.3f);
    for (int i = 0; i < 16; i++) { float a = i * 0.3927f; bolt(A, 32 + std::cos(a) * 14.2f, 32 + std::sin(a) * 14.2f, i % 3 == 0); }
    // the rear and deck lines, a moss trace in the seam, and the amber headlamps on the nose
    A.line(20.0f, 22.4f, 40.0f, 22.4f, 0.5f, rgba(RL_MOSS, 120));
    stain(A, 22.0f, 22.4f, 1.6f, rgba(RL_MOSS, 90));
    A.dot(57.4f, 28.4f, 0.9f, RL_AMB); A.dot(57.4f, 35.6f, 0.9f, RL_AMB);
    A.glow(57.6f, 28.4f, 3.6f, rgba(RL_AMB, 110)); A.glow(57.6f, 35.6f, 3.6f, rgba(RL_AMB, 110));
    A.grain(6, 17, 59, 47, 0.08f, 21);
}

void juggTurret(Art& A) {
    // the turret housing: a long, slab-sided box with a sloped front and roof plates; the gun trunnion is its forward end
    A.shadowRect(15.8f, 21.0f, 31.0f, 22.0f, 1.0f, 1.4f, 1.6f, 80);   // the turret's cast shadow on the hull
    A.poly({ {15.8f, 24}, {31, 20.8f}, {42, 21.4f}, {46.8f, 24.2f}, {48.6f, 28.6f}, {48.6f, 35.4f}, {46.8f, 39.8f}, {42, 42.6f}, {31, 43.2f}, {15.8f, 40} }, RL_OCH, 2.8f, 0.65f);
    A.poly({ {18.0f, 25.4f}, {31, 23.2f}, {41, 23.8f}, {44.8f, 27.4f}, {44.8f, 36.6f}, {41, 40.6f}, {31, 41.6f}, {18.0f, 38.6f} }, shade(RL_OCH, 1.12f), 1.6f, 0.5f);   // the roof plate, lit
    A.line(18.0f, 25.4f, 44.8f, 27.4f, 0.5f, rgba(RL_DRK, 150)); A.line(18.0f, 38.6f, 44.8f, 36.6f, 0.5f, rgba(RL_DRK, 150));
    // the rear bustle: a stowage box with two straps and a fire bottle
    A.box(19.4f, 32, 3.4f, 7.2f, 1.0f, shade(RL_ARM, 0.92f), 1.8f, 0.5f);
    A.line(17.4f, 26.2f, 17.4f, 37.8f, 0.6f, rgba(RL_DRK, 200)); A.line(21.4f, 26.2f, 21.4f, 37.8f, 0.6f, rgba(RL_DRK, 200));
    A.circle(27.6f, 36.2f, 1.2f, RL_BRZ, 0.8f, 0.4f);
    // the commander's cupola, with its hatch ring and a periscope bank
    A.circle(30.4f, 32, 4.4f, RL_ARM, 1.8f, 0.55f);
    A.ring(30.4f, 32, 3.6f, 0.6f, RL_BRZ, 0.6f, 0.2f);
    A.circle(30.4f, 32, 2.4f, shade(RL_ARM, 0.7f), 1.0f, 0.5f);
    for (int s = -1; s <= 1; s += 2) A.box(30.4f, 32 + s * 6.0f, 1.8f, 0.8f, 0.3f, RL_DRK, 0.8f, 0.3f);
    // the mantlet of the gun trunnion, bolted, and the sight box above the barrels
    A.box(47.4f, 32, 2.2f, 6.6f, 1.2f, shade(RL_ARM, 1.05f), 1.8f, 0.5f);
    bolt(A, 47.4f, 27.8f, true); bolt(A, 47.4f, 36.2f, false);
    A.box(42.0f, 32, 2.6f, 2.8f, 0.6f, RL_DRK, 1.1f, 0.4f);
    A.dot(41.4f, 32, 0.8f, RL_AMB); A.glow(41.4f, 32, 2.8f, rgba(RL_AMB, 100));
    // twin long cannons, each with a sleeve at the trunnion and a slotted muzzle brake, at y 29.4 and 34.6
    for (int s = -1; s <= 1; s += 2) {
        float y = 32 + s * 2.6f;
        A.cap(46.0f, y, 62.0f, y, 1.5f, shade(RL_DRK, 2.4f), 1.2f, 0.5f);   // barrel
        A.line(48.6f, y - 1.2f, 61.0f, y - 1.2f, 0.4f, rgba(rgb(220, 222, 214), 90));   // a highlight along the barrel
        A.box(49.6f, y, 1.6f, 2.2f, 0.4f, shade(RL_ARM, 1.1f), 1.0f, 0.4f);   // the sleeve
        A.box(61.2f, y, 1.0f, 2.4f, 0.4f, RL_DRK, 1.1f, 0.5f);   // the muzzle brake
        A.dot(62.0f, y, 0.6f, rgb(10, 10, 12));
    }
    teamDot(A, 22.4f, 39.4f, 1.9f);   // the owner's roundel on the turret's rear flank
    stain(A, 27.0f, 25.0f, 2.8f, rgba(RL_VER, 110));   // verdigris on the roof bolts and hatch rims
    A.grain(14, 19, 50, 45, 0.09f, 37);
}

// one walker leg, hip to knee to foot: a thigh with the hip socket behind it, a bronze knee joint, a shin on its hydraulic piston and a splayed foot
void striderLeg(Art& A, float hx, float hy, float kx, float ky, float fx, float fy) {
    A.circle(hx, hy, 3.2f, RL_BRZ, 1.6f, 0.5f); A.dot(hx, hy, 1.2f, RL_DRK);   // the hip socket
    A.cap(hx, hy, kx, ky, 2.4f, shade(RL_ARM, 0.92f), 1.6f, 0.55f);   // the thigh
    A.line(hx + (kx - hx) * 0.2f, hy + (ky - hy) * 0.2f, hx + (kx - hx) * 0.8f, hy + (ky - hy) * 0.8f, 0.5f, rgba(RL_STN, 90));   // a stencilled bar on the thigh
    A.circle(kx, ky, 2.9f, RL_BRZ, 1.6f, 0.5f); A.dot(kx, ky, 1.0f, RL_DRK);   // the knee
    stain(A, kx, ky, 2.2f, rgba(RL_VER, 110));
    A.cap(kx, ky, fx, fy, 1.9f, shade(RL_ARM, 1.0f), 1.4f, 0.5f);   // the shin
    float p0x = kx + (fx - kx) * 0.25f, p0y = ky + (fy - ky) * 0.25f, p1x = kx + (fx - kx) * 0.7f, p1y = ky + (fy - ky) * 0.7f;
    A.cap(p0x, p0y, p1x, p1y, 1.2f, RL_DRK, 1.0f, 0.4f);   // the hydraulic cylinder
    A.line(p1x, p1y, fx, fy, 0.5f, rgba(shade(RL_BRZ, 1.2f), 230));   // its polished rod
    A.ell(fx, fy, 3.8f, 2.8f, RL_DRK, 1.6f, 0.45f);   // the splayed foot pad
    A.ring(fx, fy, 2.0f, 0.6f, RL_BRZ, 0.6f, 0.2f);
    stain(A, fx + 1.0f, fy - 0.8f, 1.6f, rgba(RL_VER, 120));
}

void strider(Art& A, int frame) {
    float sw = frame == 0 ? 0 : (frame == 1 ? 3.2f : -3.2f);   // the stride: diagonal pairs swing together
    struct Leg { float hx, hy, kx, ky, fx, fy; int pair; };
    // front-left, front-right, rear-left, rear-right; the pairs (front-left with rear-right) swing together
    Leg legs[4] = { { 44, 22, 52, 13, 57, 5, 1 }, { 44, 42, 52, 51, 57, 59, -1 }, { 20, 22, 12, 13, 7, 5, -1 }, { 20, 42, 12, 51, 7, 59, 1 } };
    for (auto& l : legs) striderLeg(A, l.hx, l.hy, l.kx + sw * 0.5f * l.pair, l.ky, l.fx + sw * l.pair, l.fy);
    // the hull: a stout armoured body, sloped at both ends
    A.poly({ {12, 28}, {20, 23.2f}, {44, 23.2f}, {51, 27.5f}, {53, 32}, {51, 36.5f}, {44, 40.8f}, {20, 40.8f}, {12, 36} }, RL_ARM, 2.8f, 0.6f);
    // ochre side plates over the hull's edges, bolted, with a faded stencil on each
    for (int s = -1; s <= 1; s += 2) {
        float py = 32 + s * 7.6f;
        A.box(31.0f, py, 11.0f, 2.0f, 0.6f, RL_OCH, 1.4f, 0.5f);
        for (float x = 22; x <= 40; x += 4.6f) bolt(A, x, py, (int)x % 9 == 0);
        A.line(26.0f, py, 33.0f, py, 0.7f, rgba(RL_STN, 140));
    }
    // a cable and hydraulic run down the spine, and the rear power pod with its louvres and a moss trace
    A.line(14.5f, 32, 46, 32, 0.9f, rgba(RL_DRK, 190));
    A.box(16.6f, 32, 4.0f, 7.8f, 1.2f, shade(RL_DRK, 1.5f), 1.6f, 0.4f);
    for (int i = 0; i < 4; i++) A.line(14.6f, 27.8f + i * 2.6f, 18.6f, 27.8f + i * 2.6f, 0.6f, rgba(shade(RL_DRK, 2.2f), 200));
    stain(A, 13.4f, 27.0f, 2.2f, rgba(RL_MOSS, 110));
    // the sensor head at the nose: a dome with an amber lens
    A.circle(50.6f, 32, 2.9f, shade(RL_ARM, 1.1f), 1.6f, 0.5f);
    A.dot(51.4f, 32, 1.0f, RL_AMB); A.glow(51.4f, 32, 4.4f, rgba(RL_AMB, 110));
    // hatches on the hull top flanking the spine
    for (int s = -1; s <= 1; s += 2) hatchPeriscope(A, 38.0f, 32 + s * 4.6f, 1.9f, 42.0f);
    teamDot(A, 24.0f, 25.2f, 1.6f);   // the owner's roundel on the north side plate
    A.grain(12, 22, 53, 42, 0.08f, 55);
}

void striderRocketBox(Art& A) {
    // the back-mounted rocket-artillery box: a bronze-strapped slab with a bank of eight tube mouths (two rows of four), each collared in bronze
    A.shadowRect(20.0f, 24.0f, 24.0f, 16.0f, 1.0f, 1.2f, 1.6f, 90);
    A.box(32, 32, 12.0f, 8.0f, 1.4f, RL_OCH, 2.2f, 0.6f);
    A.box(32, 32, 10.6f, 6.8f, 0.8f, shade(RL_OCH, 1.12f), 1.4f, 0.5f);   // the lid, lit
    for (float x : {25.4f, 38.6f}) A.line(x, 24.8f, x, 39.2f, 1.0f, rgba(RL_BRZ, 220));   // the straps
    for (int r = 0; r < 2; r++) {
        float y = r == 0 ? 27.6f : 36.4f;
        for (int i = 0; i < 4; i++) {
            float x = 24.0f + i * 5.6f;
            A.circle(x, y, 2.3f, RL_DRK, 1.2f, 0.4f);   // the tube mouth
            A.ring(x, y, 2.1f, 0.6f, RL_BRZ, 0.5f, 0.2f);   // its bronze collar
            A.dot(x + 0.3f, y - 0.3f, 0.5f, rgb(12, 12, 14));
        }
    }
    A.box(20.6f, 32, 1.4f, 4.4f, 0.5f, shade(RL_DRK, 1.4f), 1.2f, 0.4f);   // the rear sight housing with its lamp
    A.dot(19.9f, 32, 0.8f, RL_AMB); A.glow(19.9f, 32, 2.8f, rgba(RL_AMB, 100));
    stain(A, 40.0f, 25.8f, 2.4f, rgba(RL_VER, 110));
    teamDot(A, 21.4f, 36.4f, 1.6f);   // the owner's roundel on the rear end plate
    A.grain(18, 22, 46, 42, 0.08f, 61);
}

// the Thunderhawk: a heavy attack helicopter, top-down. Its fuselage is an armoured pod with a tandem cockpit, stub wings carry rocket pods, and a
// chin turret sits under the nose. No blades: the renderer spins them over the centre.
void thunderhawk(Art& A) {
    // the tail: a boom, a tailplane with its stencil, and the tail-rotor housing with its dark fan and bronze hub
    A.cap(27, 32, 9.5f, 32, 2.5f, shade(RL_ARM, 0.9f), 1.6f, 0.55f);
    A.box(12.5f, 32, 1.5f, 6.5f, 0.6f, shade(RL_OCH, 0.92f), 1.2f, 0.5f);   // tailplane, spanning both sides of the boom
    A.line(14.6f, 30.4f, 19.2f, 30.4f, 0.5f, rgba(RL_STN, 140)); A.line(14.6f, 33.6f, 19.2f, 33.6f, 0.5f, rgba(RL_STN, 140));
    A.circle(6.2f, 32, 4.6f, RL_DRK, 1.6f, 0.5f); A.circle(6.2f, 32, 3.5f, rgb(24, 26, 26), 1.2f, 0.3f);
    A.ring(6.2f, 32, 4.1f, 0.7f, RL_BRZ, 0.6f, 0.2f); A.dot(6.2f, 32, 1.2f, RL_BRZ);
    stain(A, 6.2f, 27.6f, 2.0f, rgba(RL_VER, 120));
    teamDot(A, 18.0f, 32, 1.8f);   // the owner's roundel on the boom
    // stub wings: a root and a tip each side, with two rocket pods outboard (and a pylon from each wing to its pods)
    for (int s = -1; s <= 1; s += 2) {
        A.poly({ {26.4f, 32 + s * 6.8f}, {38.0f, 32 + s * 6.8f}, {36.6f, 32 + s * 17.6f}, {28.0f, 32 + s * 18.4f} }, shade(RL_ARM, 0.95f), 1.6f, 0.55f);
        for (float yy : {12.4f, 17.6f}) {
            float y = 32 + s * yy;
            A.line(32.0f, 32 + s * 9.0f, 32.0f, y, 0.9f, rgba(RL_DRK, 200));   // the pylon
            A.cap(28.0f, y, 41.4f, y, 2.5f, shade(RL_ARM, 0.8f), 1.4f, 0.5f);   // the rocket pod
            A.circle(41.6f, y, 2.1f, RL_DRK, 1.0f, 0.4f);   // its tube nose
            A.dot(42.4f, y - 0.9f, 0.5f, RL_BRZ); A.dot(42.4f, y + 0.9f, 0.5f, RL_BRZ);
        }
    }
    // the fuselage: an armoured pod, its flanks seamed and riveted
    A.poly({ {59, 32}, {55, 27.8f}, {46, 25.6f}, {34, 25.4f}, {24, 27.2f}, {21.5f, 32}, {24, 36.8f}, {34, 38.6f}, {46, 38.4f}, {55, 36.2f} }, RL_ARM, 2.6f, 0.62f);
    A.line(25.0f, 28.4f, 44.0f, 27.6f, 0.6f, rgba(RL_DRK, 170)); A.line(25.0f, 35.6f, 44.0f, 36.4f, 0.6f, rgba(RL_DRK, 170));
    for (float x = 26.0f; x <= 44.0f; x += 3.6f) { A.dot(x, 26.8f, 0.45f, RL_BRZ); A.dot(x, 37.2f, 0.45f, RL_BRZ); }
    // the engine intakes behind the cockpit, grilled, and the two exhausts at the boom root with their dim glow
    for (int s = -1; s <= 1; s += 2) {
        A.ell(33.0f, 32 + s * 5.6f, 3.6f, 1.8f, RL_DRK, 1.2f, 0.4f);
        for (float x = 30.8f; x <= 35.4f; x += 1.5f) A.line(x, 31.4f + s * 5.6f, x, 32.6f + s * 5.6f, 0.4f, rgba(shade(RL_DRK, 2.4f), 200));
        A.circle(26.6f, 32 + s * 3.4f, 1.9f, RL_BRZ, 1.0f, 0.4f); A.dot(26.6f, 32 + s * 3.4f, 0.9f, RL_DRK);
        A.glow(26.6f, 32 + s * 3.4f, 3.6f, rgba(RL_AMB, 70));
    }
    // the armoured cockpit: a bronze-framed canopy for the pilot up front and one for the gunner behind, with an armoured bulkhead between
    A.ell(49.0f, 32, 5.6f, 4.8f, RL_DRK, 1.2f, 0.4f);
    A.ell(49.0f, 32, 4.8f, 4.0f, RL_GLASS, 1.6f, 0.6f);
    A.ell(47.6f, 30.4f, 2.2f, 0.8f, rgba(rgb(214, 228, 222), 190), 1, 0);
    A.ell(40.2f, 32, 4.2f, 3.6f, RL_DRK, 1.2f, 0.4f);
    A.ell(40.2f, 32, 3.4f, 2.8f, RL_GLASS, 1.4f, 0.6f);
    A.ell(39.0f, 30.8f, 1.6f, 0.6f, rgba(rgb(214, 228, 222), 170), 1, 0);
    A.box(44.5f, 32, 0.9f, 3.6f, 0.4f, RL_BRZ, 1.0f, 0.4f);
    // the nose: a chin turret with its gun, the sensor lamps and a nose sensor
    A.cap(52.0f, 32, 62.4f, 32, 1.2f, RL_DRK, 1.0f, 0.5f);
    A.circle(53.6f, 32, 3.0f, shade(RL_ARM, 0.92f), 1.6f, 0.5f);
    A.dot(54.2f, 32, 0.8f, RL_DRK);
    A.dot(56.2f, 30.4f, 0.8f, RL_AMB); A.dot(56.2f, 33.6f, 0.8f, RL_AMB);
    A.glow(56.4f, 30.4f, 3.2f, rgba(RL_AMB, 100)); A.glow(56.4f, 33.6f, 3.2f, rgba(RL_AMB, 100));
    // the rotor head over the centre: a bronze mast housing the renderer's blades turn above
    A.circle(32, 32, 3.8f, shade(RL_ARM, 0.9f), 1.6f, 0.5f);
    A.ring(32, 32, 3.2f, 0.9f, RL_BRZ, 0.6f, 0.2f); A.dot(32, 32, 1.2f, RL_DRK);
    stain(A, 35.2f, 35.4f, 2.2f, rgba(RL_VER, 110));
    A.grain(6, 13, 58, 51, 0.07f, 91);
}

}  // namespace

// ---------------------------------------------------------------- public entry points
void artRelicUnitBody(Canvas& c, int kind, Color team, int frame) {
    Art A(c, team);
    if (kind == 0) juggernaut(A, frame);
    else if (kind == 1) strider(A, frame);
    else thunderhawk(A);
    sharpen(c, 0.7f);
    c.outline(rgb(8, 10, 14, 200));
}

bool artRelicUnitTurret(Canvas& c, int kind, Color team) {
    if (kind == 2) return false;
    Art A(c, team);
    if (kind == 0) juggTurret(A);
    else striderRocketBox(A);
    sharpen(c, 0.7f);
    c.outline(rgb(8, 10, 14, 200));
    return true;
}
