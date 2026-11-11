// One Hour - 3D models: the Cyber ground vehicles (Fabricator dozer, E-Hauler, Volt Walker, Railgun Tank, Aegis Titan,
// jeep, Anti-Radiation Truck and the civilian dozer). Cyber look: pale ceramic plates over graphite, deep cobalt trim,
// glass and small cyan lights. Coordinates are the old 64x64 canvas pixels minus 32 (see GUIDE.md).
#include "m3d_parts.h"

using namespace m3d;

// a few extra tones in the same spirit as pal::
static const Color graphite = C(0x39424F);    // the graphite under the ceramic plates
static const Color steelC = C(0x8E9AA8);      // bare structural steel
static const Color amber = C(0xFFAA3C);       // work / warning lamps
static const Color tail = C(0xFF5A48);        // tail lamps
static const Color ice = C(0xEAF6FB);         // white light, pale ceramic highlight
static const Color civicWhite = C(0xEDEBE3);  // civilian livery
static const Color safeOrange = C(0xEE7418);  // safety orange

// ---------------------------------------------------------------- Fabricator (builder vehicle) and its civilian twin
// civic = false: pale ceramic over graphite, cyan work lights. civic = true: the same machine in civilian white with
// safety orange on the blade and ripper and an amber/white beacon bar over the cab.
static void dozerModel(Model& b, bool civic) {
    using namespace pal;
    const Color plate = civic ? civicWhite : cHull;      // the ceramic body
    const Color body2 = civic ? C(0xD2CFC6) : cHull2;    // secondary panels
    const Color acc = civic ? safeOrange : cTrim;        // the strong accent: orange, or deep cobalt
    const Color dark = civic ? C(0x4A4E56) : graphite;
    const Color work = civic ? amber : cGlow;            // work lights: amber beacon / cyan strip
    const Color blade = civic ? C(0xF2F0EA) : C(0xC6D1DC);   // the moldboard: pale steel-faced plate
    const Color edge = civic ? C(0x9A9C9E) : C(0x8E9AA8);    // its cutting edge
    b.dirt = civic ? 0.35f : 0.25f;

    // crawler track runs: the old ones sit centred 2 px behind the origin, 34 long and 32 across the outside
    b.frame(-2, 0, 0);
    tracks(b, 34, 5.6f, 10.4f, 7.0f, body2, false);
    b.frame();
    for (int s = -1; s <= 1; s += 2) {   // faired track guards over the inner half of each run
        b.profileY({ { -18.5f, 3.4f }, { -18, 8.6f }, { 12, 8.6f }, { 15.5f, 4.0f } }, s * 13.6f - 1.0f, s * 13.6f + 1.0f, plate).t(T_PANEL, 7).bev(0.5f);
        b.box(-2, s * 15.9f, 6.4f, 15.0f, 0.5f, 1.2f, acc).t(T_PANEL, 5).bev(0.2f);   // accent band along the guard's edge
    }
    // chassis between the tracks: a raised rear, a sloped nose
    b.profileY({ { -16.5f, 3.2f }, { -17.0f, 9.8f }, { 5.5f, 10.4f }, { 12.5f, 6.2f }, { 11.0f, 3.0f }, { -15.0f, 3.0f } }, -9.6f, 9.6f, plate).t(T_PANEL, 7).bev(1.0f);
    // rear hydraulic and fuel block with louvres; the owner's roundel sits on its deck
    b.slab(-10.5f, 0, 5.4f, 7.6f, 9.8f, 2.3f, body2).t(T_RIBS, 1.6f).bev(0.5f);
    b.box(-15.6f, 0, 11.0f, 0.5f, 6.6f, 1.2f, dark).t(T_GRILLE, 1.4f).bev(0.2f);
    b.cyl('z', -10.5f, 0, 12.15f, 3.0f, 0.25f, OWNER).bev(0.2f);
    // engine hood at the front: radiator grille on the nose, an exhaust stack on the rear corner
    b.slab(12.4f, 0, 3.2f, 3.8f, 6.2f, 3.6f, body2).t(T_PANEL, 4).bev(0.6f);
    b.box(15.5f, 0, 8.0f, 0.5f, 3.4f, 1.5f, dark).t(T_GRILLE, 1.2f).bev(0.2f);
    b.cyl('z', 9.6f, -2.6f, 11.4f, 0.8f, 1.4f, steelC).m(METAL).bev(0.3f);
    b.cyl('z', 9.6f, -2.6f, 12.9f, 0.95f, 0.4f, dark).bev(0.2f);
    // operator's cab: armoured glass windscreen, a composite roof in the owner's colour, a roll-over cage behind it
    b.taper(2.0f, 0, 10.2f, 6.6f, 7.2f, 6.4f, 5.4f, 6.4f, plate, 0.6f, 0).t(T_PANEL, 6).bev(0.9f);
    b.box(6.9f, 0, 13.6f, 0.9f, 5.6f, 1.9f, cGlass).m(GLASS).rz(-16).bev(0.3f);
    b.box(2.0f, 0, 16.65f, 4.6f, 5.0f, 0.25f, OWNER).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ -4.4f, s * 6.3f, 10.4f }, { -4.4f, s * 6.3f, 17.4f }, 0.55f, dark).m(METAL).bev(0.2f);   // cage posts
        b.box(7.2f, s * 6.4f, 11.0f, 0.7f, 1.2f, 0.5f, work).m(EMIT);                                    // work lamps, front corners
        b.box(1.0f, s * 7.0f, 13.4f, 2.6f, 0.4f, 1.5f, civic ? body2 : OWNER).bev(0.2f);                 // cab door panels
    }
    b.rod({ -4.4f, -6.3f, 17.4f }, { -4.4f, 6.3f, 17.4f }, 0.55f, dark).m(METAL).bev(0.2f);              // cage crossbar
    b.box(-3.6f, 0, 12.0f, 0.5f, 6.6f, 0.9f, body2).t(T_PANEL, 3).bev(0.2f);
    if (civic) {   // the civilian beacon bar across the cab roof: amber at one end, white at the other
        b.slab(2.0f, 0, 3.2f, 5.4f, 16.9f, 1.3f, dark).bev(0.4f);
        b.box(2.0f, -2.6f, 18.4f, 1.1f, 1.4f, 0.5f, amber).m(EMIT);
        b.box(2.0f, 2.6f, 18.4f, 1.1f, 1.4f, 0.5f, ice).m(EMIT);
    } else {
        for (int s = -1; s <= 1; s += 2) b.box(-3.9f, s * 6.9f, 15.2f, 0.5f, 0.6f, 1.6f, cGlow).m(EMIT);  // cyan cab strips
    }
    // ripper at the tail: a crossbeam on two braces, with one broad steel tooth hanging behind the tracks
    b.box(-16.4f, 0, 9.4f, 1.3f, 10.5f, 1.2f, body2).m(METAL).t(T_PANEL, 4).bev(0.35f);
    for (int s = -1; s <= 1; s += 2) b.rod({ -15.4f, s * 6.4f, 10.0f }, { -17.6f, s * 4.0f, 8.6f }, 0.9f, acc).m(PAINT).bev(0.25f);
    b.taper(-18.4f, 0, 1.2f, 0.5f, 0.9f, 7.8f, 1.6f, 2.4f, C(0xA9B4C0), 0.7f, 0).m(METAL).bev(0.3f);
    // the blade: a mouldboard in three sections (the two wings angled back for the bow), each with a steel cutting edge
    for (int k = -1; k <= 1; k++) {
        const float ad = k * 20.0f, cy = k * 8.6f, hy = k ? 5.6f : 5.4f, cx = 21.6f - std::abs(k) * 1.2f, tall = k ? 5.0f : 6.0f;
        b.taper(cx, cy, 1.4f, 1.8f, hy, tall, 1.8f, hy, blade, -2.6f, 0).t(T_PANEL, 5).rz(ad).bev(0.5f);
        b.box(cx + 1.9f, cy, 1.4f + tall * 0.4f, 0.9f, hy - 0.4f, tall * 0.75f, edge).m(METAL).rz(ad).bev(0.25f);
        b.box(cx - 2.6f, cy, 1.4f + tall - 0.2f, 1.0f, hy - 0.8f, 0.6f, body2).rz(ad).bev(0.2f);   // the wear crown along the top
    }
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ 8.5f, s * 9.0f, 6.4f }, { 16.2f, s * 6.8f, 6.0f }, 1.3f, body2).m(METAL).bev(0.3f);          // push arms
        b.rod({ 11.0f, s * 5.4f, 8.6f }, { 17.2f, s * 4.6f, 6.6f }, 0.8f, steelC).m(METAL).bev(0.2f);        // lift rams
        b.box(-2.0f, s * 6.6f, 3.6f, 1.2f, 1.0f, 0.7f, civic ? safeOrange : tail).m(civic ? PAINT : EMIT).bev(0.2f);
    }
    b.rod({ 15.0f, 0, 5.4f }, { 19.1f, 0, 5.4f }, 1.0f, steelC).m(METAL).bev(0.2f);                          // blade mount bracket
}
static void cDozer(Model& b, Model&, int) { dozerModel(b, false); }
M3D_UNIT(U_C_DOZER, cDozer)

static void civDozer(Model& b, Model&, int) { dozerModel(b, true); }
M3D_UNIT(U_C_CIVDOZER, civDozer)

// ---------------------------------------------------------------- E-Hauler (harvester): a six-wheel supply truck
static void cHarv(Model& b, Model&, int) {
    using namespace pal;
    b.dirt = 0.3f;
    const Color deck = C(0x9FADBC);
    const float axle[3] = { -13.0f, 1.5f, 16.5f };
    // six wheels: the tyres stand out past the deck, their outer edges 15.8 from the centre line
    for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2) {
        b.cyl('y', axle[i], s * 13.6f, 3.4f, 3.4f, 2.2f, pal::rubber).m(RUBBER).t(T_TREAD, 2.0f, C(0x505A68)).bev(0.5f);
        b.cyl('y', axle[i], s * 13.6f, 3.4f, 1.5f, 2.5f, cHull2).m(METAL).bev(0.3f);
        b.box(axle[i], s * 11.0f, 6.4f, 2.0f, 2.6f, 0.7f, cHull).t(T_PANEL, 5).bev(0.3f);   // mudguard over the inner half
    }
    // chassis and flatbed deck
    b.slab(-7.5f, 0, 18.0f, 8.6f, 4.6f, 1.6f, graphite).t(T_PANEL, 8).bev(0.4f);
    b.slab(-7.5f, 0, 17.5f, 11.0f, 6.2f, 1.5f, deck).t(T_PANEL, 9).bev(0.6f);
    for (int s = -1; s <= 1; s += 2) b.box(-7.5f, s * 11.2f, 7.6f, 17.0f, 0.5f, 1.1f, cHull2).t(T_RIBS, 2.2f).bev(0.2f);   // side rails
    // two sealed supply containers on the deck, ribbed roofs with a rear door each
    for (int s = -1; s <= 1; s += 2) {
        const float cy = s * 5.6f;
        b.taper(-5.9f, cy, 7.7f, 13.5f, 4.2f, 4.4f, 13.0f, 3.8f, C(0xB4C0CD), -0.4f, 0).t(T_RIBS, 2.4f).bev(0.7f);
        b.box(-18.6f, cy, 9.8f, 0.5f, 3.6f, 1.9f, graphite).t(T_PANEL, 2).bev(0.2f);
        b.box(-5.9f, cy, 12.15f, 7.0f, 1.8f, 0.22f, OWNER).bev(0.2f);
    }
    // folding crane: a pedestal between the containers, a boom laid to the rear, a hook block
    b.cyl('z', 3.6f, 0, 7.7f, 2.0f, 1.0f, cMid).m(METAL).bev(0.3f);
    b.rod({ 3.6f, 0, 9.6f }, { -17.0f, 0, 12.6f }, 0.9f, cHull2).m(METAL).bev(0.25f);
    b.cyl('z', -17.0f, 0, 12.0f, 1.1f, 0.5f, graphite).bev(0.2f);
    b.box(3.6f, 0, 10.6f, 1.4f, 1.4f, 0.8f, cGlow).m(EMIT).bev(0.2f);
    // spare wheel lying flat on the deck ahead of the cab
    b.cyl('z', 9.2f, 0, 7.7f, 3.2f, 0.8f, pal::rubber).m(RUBBER).t(T_TREAD, 2.0f, C(0x505A68)).bev(0.3f);
    b.cyl('z', 9.2f, 0, 8.6f, 1.3f, 0.5f, cHull2).m(METAL).bev(0.2f);
    // cab: composite body, armoured windscreen, an owner-coloured roof, a roof hatch, mirrors and lamps
    b.taper(18.2f, 0, 7.6f, 6.8f, 9.2f, 6.6f, 5.6f, 8.4f, cHull, 0.8f, 0).t(T_PANEL, 6).bev(1.0f);
    b.box(22.6f, 0, 12.0f, 1.1f, 6.8f, 1.5f, cGlass).m(GLASS).rz(-14).bev(0.3f);
    b.box(18.2f, 0, 14.25f, 2.8f, 4.2f, 0.25f, OWNER).bev(0.2f);
    b.cyl('z', 14.8f, 0, 14.3f, 2.0f, 0.5f, cHull2).t(T_PANEL, 3).bev(0.3f);
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ 20.6f, s * 8.6f, 12.4f }, { 23.0f, s * 10.6f, 11.6f }, 0.4f, cMid).m(METAL).bev(0.1f);
        b.box(23.2f, s * 11.2f, 11.4f, 0.5f, 1.0f, 1.4f, graphite).bev(0.2f);
        b.box(24.2f, s * 6.8f, 9.4f, 0.5f, 1.0f, 0.5f, amber).m(EMIT);
        b.box(18.2f, s * 9.4f, 8.6f, 5.0f, 0.4f, 0.9f, cGlow).m(EMIT);
    }
    b.box(24.9f, 0, 8.4f, 0.6f, 7.4f, 1.6f, graphite).t(T_GRILLE, 1.3f).bev(0.2f);
    b.box(-24.6f, 0, 7.0f, 0.6f, 8.6f, 1.2f, cHull2).t(T_PANEL, 4).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) b.box(-25.1f, s * 6.6f, 5.6f, 0.5f, 1.2f, 0.8f, tail).m(EMIT);
}
M3D_UNIT(U_C_HARV, cHarv)

// ---------------------------------------------------------------- Volt Walker: a legged chassis with a volt coil
// The old body is a 27 px disc on four articulated legs; frame 1 / 2 swing the diagonal leg pairs fore and aft.
static void cVolt(Model& b, Model& t, int frame) {
    using namespace pal;
    b.dirt = 0.2f;
    const float sw = frame == 1 ? 3.2f : (frame == 2 ? -3.2f : 0.0f);
    const float DECK = 18.5f;                       // the top of the body, where the coil's ring sits
    // hips (front pair +x, rear pair -x), knees and feet: the feet slide by the stride
    const float hipx[4] = { 6, 6, -6, -6 }, hipy[4] = { -7, 7, -7, 7 };
    const float dsw[4] = { 1, -1, -1, 1 };
    for (int i = 0; i < 4; i++) {
        const float s = hipy[i] < 0 ? -1.0f : 1.0f;
        const V3 hip = { hipx[i], hipy[i], DECK - 2.5f };
        const V3 knee = { hipx[i] + 6.0f * (hipx[i] > 0 ? 1.0f : -1.0f), s * 15.0f, 9.5f };
        const V3 foot = { knee.x + dsw[i] * sw, s * 21.0f, 1.4f };
        b.rod(hip, knee, 2.7f, cMid).m(PAINT).t(T_PANEL, 5).bev(0.5f);                       // thigh
        b.rod(knee, foot, 2.0f, graphite).m(METAL).bev(0.4f);                                 // shin
        b.ball(knee.x, knee.y, knee.z, 2.9f, 2.9f, 2.9f, cHull2).t(T_PANEL, 4).bev(0.5f);    // knee joint
        b.cyl('z', foot.x, foot.y, 1.2f, 3.0f, 1.3f, graphite).m(RUBBER).bev(0.5f);          // foot pad
        b.cyl('z', foot.x, foot.y, 2.6f, 1.5f, 0.5f, cHull2).m(METAL).bev(0.2f);
        b.rod({ hip.x, hip.y, hip.z - 1.5f }, { foot.x, foot.y, foot.z + 2.0f }, 0.45f, cGlow).m(EMIT).f(F_NOSHADOW);
    }
    // the body: a shallow disc with a raised hub, six radial intakes and a ring of cyan emitter studs
    b.cyl('z', 0, 0, DECK - 4.2f, 13.5f, 4.2f, cHull, 0.90f).t(T_PANEL, 7).bev(1.0f);
    b.cyl('z', 0, 0, DECK - 0.4f, 10.0f, 1.6f, cHull2, 0.92f).t(T_RIBS, 2.0f).bev(0.5f);
    b.cyl('z', 0, 0, DECK + 1.4f, 6.0f, 1.2f, graphite, 0.85f).bev(0.5f);
    for (int i = 0; i < 6; i++) {
        const float a = i * 1.0472f + 0.5236f;
        b.box(std::cos(a) * 10.0f, std::sin(a) * 10.0f, DECK - 2.2f, 1.7f, 1.7f, 1.7f, graphite).rz(a * 57.3f).t(T_GRILLE, 1.1f).bev(0.25f);
    }
    for (int i = 0; i < 8; i++) {
        const float a = i * 0.7854f;
        b.cyl('z', std::cos(a) * 12.6f, std::sin(a) * 12.6f, DECK + 0.1f, 0.85f, 0.45f, cGlow).m(EMIT).bev(0.15f);
    }
    b.box(-9.8f, 0, DECK + 0.35f, 2.6f, 5.2f, 0.3f, OWNER).bev(0.2f);                        // the owner's panel on the rim deck
    b.dome(-6.0f, -5.0f, DECK + 0.6f, 2.4f, 2.4f, 2.0f, cGlass).m(GLASS);                     // sensor dome
    b.rod({ -8.0f, 5.0f, DECK + 1.0f }, { -13.0f, 8.0f, DECK + 9.0f }, 0.35f, graphite).f(F_NOSHADOW);
    b.cyl('z', 0, 0, DECK + 2.4f, 8.6f, 0.6f, graphite).bev(0.3f);                            // the ring the coil turns on
    b.cyl('z', 0, 0, DECK + 5.5f, 9.0f, 3.6f, cHull).f(F_SHADOWONLY);                          // (the coil's shade on the deck)
    // turret: the volt coil - a round base with an energy hub, twin conduits and four coil rings
    t.ground = false; t.dirt = 0.05f;
    t.cyl('z', 0, 0, DECK + 2.9f, 8.6f, 1.5f, cHull2, 0.86f).t(T_PANEL, 5).bev(0.5f);
    t.cyl('z', 0, 0, DECK + 5.6f, 5.4f, 1.4f, cMid, 0.8f).m(METAL).bev(0.4f);
    t.ball(0, 0, DECK + 7.4f, 3.2f, 3.2f, 2.6f, ice).m(GLASS);
    for (int s = -1; s <= 1; s += 2) t.rod({ 4.0f, s * 3.0f, DECK + 4.6f }, { 23.0f, s * 3.0f, DECK + 4.6f }, 1.5f, cHull2).m(METAL).bev(0.3f);
    for (int i = 0; i < 4; i++) {
        const float x = 8.0f + i * 4.2f;
        t.box(x, 0, DECK + 4.6f, 0.9f, 5.6f, 0.45f, i % 2 ? cGlow : ice).m(i % 2 ? EMIT : PAINT).bev(0.2f);
    }
    t.rod({ 4.0f, 0, DECK + 4.6f }, { 24.0f, 0, DECK + 4.6f }, 1.1f, graphite).m(METAL).bev(0.2f);
    t.box(24.6f, 0, DECK + 4.6f, 1.2f, 2.6f, 2.0f, cMid).m(METAL).bev(0.35f);
    t.ball(26.2f, 0, DECK + 4.6f, 1.6f, 1.6f, 1.6f, cGlow).m(EMIT);
    t.box(-6.0f, 0, DECK + 5.0f, 2.4f, 5.0f, 1.4f, graphite).t(T_GRILLE, 1.3f).bev(0.2f);     // the power bus behind the hub
    t.box(0, 0, DECK + 7.1f, 4.2f, 4.2f, 0.25f, OWNER).bev(0.2f);                             // the owner's colour on the coil's shoulder
    t.cyl('z', -3.0f, 4.4f, DECK + 6.4f, 1.9f, 0.6f, cHull2).bev(0.3f);
}
M3D_UNIT(U_C_VOLT, cVolt)

// ---------------------------------------------------------------- Railgun Tank: a long, low tank destroyer on wide tracks
static void cRail(Model& b, Model& t, int) {
    using namespace pal;
    const Color comp = C(0x7C8AA0);                       // the blue-grey composite of the hull and turret
    b.dirt = 0.3f;
    b.frame(-2, 0, 0);                 // the old track runs sit centred 2 px behind the origin
    tracks(b, 48, 10.5f, 10.0f, 7.0f, comp, true);
    b.frame();
    // hull: a long low tub with a tapering nose
    b.plan({ { -25, -11.5f }, { 10, -11.5f }, { 23, -5.5f }, { 23, 5.5f }, { 10, 11.5f }, { -25, 11.5f } }, 3.0f, 10.2f, comp).t(T_PANEL, 8).bev(1.0f);
    b.profileY({ { 10, 10.2f }, { 23, 5.4f }, { 22, 3.0f }, { 9, 3.0f } }, -5.5f, 5.5f, C(0x93A0B2)).t(T_PANEL, 4).bev(0.6f);   // glacis
    for (int s = -1; s <= 1; s += 2) {
        b.box(-4.0f, s * 19.0f, 8.6f, 12.0f, 0.6f, 1.4f, OWNER).bev(0.15f);                   // the owner's flank stripes
        b.box(21.5f, s * 4.0f, 7.4f, 0.8f, 1.4f, 0.5f, cGlow).m(EMIT);
        b.rod({ 12.0f, s * 5.5f, 10.4f }, { 22.5f, s * 3.2f, 6.6f }, 0.4f, graphite).f(F_NOSHADOW);   // glacis seams
    }
    b.slab(-20.0f, 0, 5.0f, 9.0f, 10.2f, 1.6f, graphite).t(T_GRILLE, 1.5f).bev(0.3f);           // rear engine deck
    b.box(-24.6f, 0, 6.0f, 0.6f, 7.0f, 1.2f, C(0x6B7480)).m(METAL).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) b.box(-24.9f, s * 5.0f, 5.2f, 0.5f, 1.2f, 0.7f, tail).m(EMIT);
    b.cyl('z', -1, 0, 11.6f, 10.6f, 0.7f, graphite).bev(0.3f);                                  // turret ring
    b.cyl('z', -1, 0, 14.5f, 12.0f, 3.6f, comp).f(F_SHADOWONLY);                                // (the turret's shade on the deck)
    // turret: a low composite shell with the long rail gun
    t.ground = false; t.dirt = 0.1f;
    t.plan({ { -18, -8.0f }, { 6, -8.0f }, { 11, -4.5f }, { 11, 4.5f }, { 6, 8.0f }, { -18, 8.0f }, { -19.5f, 4.0f }, { -19.5f, -4.0f } }, 11.4f, 15.6f, comp).t(T_PANEL, 7).bev(0.9f);
    t.box(-4.0f, 0, 15.8f, 8.0f, 5.6f, 0.3f, C(0x93A0B2)).t(T_PANEL, 6).bev(0.3f);              // roof plate
    t.box(-4.0f, 0, 16.1f, 4.6f, 3.0f, 0.25f, OWNER).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        t.cyl('z', -9.5f, s * 4.4f, 16.0f, s < 0 ? 2.2f : 1.8f, 0.6f, C(0x93A0B2)).t(T_PANEL, 3).bev(0.3f);
        t.box(2.0f, s * 8.2f, 13.4f, 4.0f, 0.5f, 1.4f, graphite).bev(0.2f);                      // side stowage
    }
    t.box(3.8f, -4.4f, 16.4f, 1.6f, 1.2f, 0.6f, graphite).bev(0.2f);
    t.box(3.9f, -4.4f, 16.6f, 0.4f, 0.5f, 0.4f, cGlow).m(EMIT);                                  // gunner's sight
    t.rod({ -15.5f, -7.0f, 15.8f }, { -19.0f, -13.0f, 24.0f }, 0.35f, graphite).f(F_NOSHADOW);   // antenna
    t.slab(-16.4f, 0, 2.4f, 5.8f, 15.6f, 1.6f, cMid).m(METAL).t(T_RIBS, 1.4f).bev(0.3f);         // power bus at the rear
    t.box(9.6f, 0, 13.6f, 2.0f, 4.8f, 1.6f, C(0x59667A)).bev(0.4f);                              // breech housing
    for (int s = -1; s <= 1; s += 2)                                                             // the two conductor rails
        t.rod({ 9.0f, s * 3.3f, 13.6f }, { 29.5f, s * 3.3f, 13.6f }, 1.2f, C(0xC4CEDA)).m(METAL).bev(0.25f);
    t.rod({ 9.0f, 0, 13.6f }, { 28.0f, 0, 13.6f }, 1.0f, graphite).m(METAL).bev(0.2f);           // insulated spine
    for (int i = 0; i < 4; i++) {
        const float x = 13.5f + i * 4.5f;
        t.box(x, 0, 13.6f, 0.9f, 4.5f, 0.5f, C(0xA9B4C0)).m(METAL).bev(0.2f);                    // coil rings across the rails
        if (i == 2) for (int s = -1; s <= 1; s += 2) t.box(x, s * 4.9f, 13.6f, 0.4f, 0.4f, 0.4f, cGlow).m(EMIT);
    }
    t.box(30.0f, 0, 13.6f, 0.9f, 2.5f, 0.5f, cMid).m(METAL).bev(0.2f);                           // slim muzzle
    t.ball(31.1f, 0, 13.6f, 0.5f, 0.5f, 0.5f, cGlow).m(EMIT);
}
M3D_UNIT(U_C_RAIL, cRail)

// ---------------------------------------------------------------- Aegis Titan: the super-heavy on four track pods
static void cTitan(Model& b, Model& t, int) {
    using namespace pal;
    b.dirt = 0.35f;
    b.frame(-2, 0, 0);                    // the old track runs sit centred 2 px behind the origin
    tracks(b, 50, 15.3f, 6.4f, 8.0f, cMid, true);
    b.frame();
    // the hull: a broad slab with a chamfered nose
    b.plan({ { -27, -13 }, { 12, -13 }, { 25, -6.5f }, { 25, 6.5f }, { 12, 13 }, { -27, 13 } }, 3.2f, 11.5f, cHull).t(T_PANEL, 9).bev(1.1f);
    b.profileY({ { 12, 11.5f }, { 25, 6.4f }, { 24, 3.2f }, { 11, 3.2f } }, -6.5f, 6.5f, cHull2).t(T_PANEL, 4).bev(0.6f);
    for (int s = -1; s <= 1; s += 2) {
        b.slab(-6.0f, s * 11.4f, 21.0f, 2.4f, 11.5f, 3.2f, cHull2).t(T_PANEL, 6).bev(0.6f);   // armoured shoulder plates
        b.box(-6.0f, s * 13.9f, 13.4f, 16.0f, 0.5f, 1.2f, cGlow).m(EMIT);                     // the emitter strip along each
        b.box(22.5f, s * 4.4f, 8.6f, 0.8f, 1.6f, 0.6f, ice).m(EMIT);                          // headlamps
        b.rod({ 12.0f, s * 6.2f, 11.5f }, { 24.5f, s * 3.4f, 7.0f }, 0.4f, graphite).f(F_NOSHADOW);
    }
    // reactor deck at the rear: a glowing core ring, three louvres and the intake stack
    b.slab(-18.0f, 0, 8.5f, 10.5f, 11.5f, 2.6f, cMid).t(T_PANEL, 5).bev(0.6f);
    b.cyl('z', -19.0f, 0, 14.1f, 5.2f, 0.9f, graphite).bev(0.3f);
    b.cyl('z', -19.0f, 0, 14.9f, 3.4f, 0.6f, cGlow).m(EMIT).bev(0.2f);
    for (int i = 0; i < 3; i++) b.box(-12.5f + i * 2.4f, 0, 13.6f, 0.7f, 6.5f, 0.5f, graphite).t(T_GRILLE, 1.1f).bev(0.15f);
    b.box(-26.6f, 0, 8.0f, 0.7f, 9.0f, 1.6f, C(0x6B7480)).m(METAL).bev(0.2f);                 // rear shield
    for (int s = -1; s <= 1; s += 2) b.box(-14.0f, s * 12.9f, 14.85f, 6.0f, 0.9f, 0.3f, OWNER).bev(0.15f);  // the owner's bands along both shoulders
    b.box(17.5f, 0, 11.65f, 4.0f, 4.2f, 0.3f, OWNER).bev(0.15f);                                            // and one on the glacis
    b.cyl('z', -1, 0, 14.0f, 12.5f, 0.8f, graphite).bev(0.35f);                                // turret ring
    b.cyl('z', -1, 0, 18.0f, 13.5f, 4.6f, cHull).f(F_SHADOWONLY);                              // (the turret's shade on the deck)
    // turret: a big faceted shell with twin ion cannons
    t.ground = false; t.dirt = 0.1f;
    t.plan({ { -19.5f, -10 }, { 5, -10.5f }, { 14, -4.5f }, { 14, 4.5f }, { 5, 10.5f }, { -19.5f, 10 }, { -21, 6.0f }, { -21, -6.0f } }, 14.0f, 19.6f, cHull2).t(T_PANEL, 8).bev(1.0f);
    t.box(-6.0f, 0, 19.9f, 8.0f, 6.0f, 0.3f, cHull).t(T_PANEL, 6).bev(0.3f);                   // roof plate
    t.box(-6.0f, 0, 20.2f, 5.0f, 3.4f, 0.25f, OWNER).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) t.box(2.0f, s * 10.2f, 17.0f, 6.0f, 0.6f, 1.6f, cGlow).m(EMIT).bev(0.2f);   // flank emitters
    t.cyl('z', -2.5f, 0, 20.0f, 3.7f, 1.1f, cMid).m(METAL).t(T_PANEL, 3).bev(0.4f);            // commander's cupola
    t.cyl('z', -2.5f, 0, 21.6f, 2.2f, 0.5f, cGlass).m(GLASS).bev(0.2f);
    t.ball(3.5f, 0, 20.4f, 2.6f, 2.6f, 1.6f, cGlow).m(EMIT);                                   // the reactor eye
    t.rod({ -17.0f, -8.0f, 19.6f }, { -23.0f, -17.0f, 30.0f }, 0.45f, graphite).f(F_NOSHADOW);  // antenna
    t.slab(-15.0f, 0, 3.0f, 7.0f, 19.6f, 1.8f, graphite).t(T_GRILLE, 1.4f).bev(0.25f);          // rear bus
    for (int s = -1; s <= 1; s += 2) {                                                          // the twin ion cannons
        const float y = s * 3.8f, z = 17.6f;
        t.box(12.0f, y, z, 2.6f, 3.2f, 2.4f, cMid).m(METAL).bev(0.4f);                          // gun mount
        t.rod({ 13.0f, y, z }, { 31.0f, y, z }, 2.1f, C(0xB9C6D4)).m(METAL).t(T_PANEL, 6).bev(0.3f);
        for (int i = 0; i < 4; i++) t.box(14.0f + i * 4.4f, y, z, 0.9f, 3.2f, 0.5f, cHull).bev(0.2f);
        t.cyl('z', 31.5f, y, z, 2.1f, 0.9f, ice).m(EMIT).bev(0.2f);
    }
}
M3D_UNIT(U_C_TITAN, cTitan)

// ---------------------------------------------------------------- Jeep: open-top fast transport, seven seats
// The seats are left empty and open: the game draws a helmet over each one that has a soldier in it.
static void cJeep(Model& b, Model&, int) {
    using namespace pal;
    const Color shell = C(0x6E8098);      // the composite of the old jeep hull
    const Color shellD = C(0x414C60);
    b.dirt = 0.3f;
    const float ax[2] = { -15.0f, 14.0f };
    for (int i = 0; i < 2; i++) for (int s = -1; s <= 1; s += 2) {
        b.cyl('y', ax[i], s * 10.5f, 3.2f, 3.2f, 2.4f, pal::rubber).m(RUBBER).t(T_TREAD, 2.0f, C(0x505A68)).bev(0.5f);
        b.cyl('y', ax[i], s * 10.5f, 3.2f, 1.4f, 2.6f, cHull2).m(METAL).bev(0.3f);
        b.box(ax[i], s * 8.6f, 5.6f, 6.0f, 2.4f, 1.2f, shell).t(T_PANEL, 5).bev(0.6f);   // fender pad over the inner half
    }
    // the hull: a narrow bonnet, a wider crew tub. The tub is cut out of the body, so the seats stand free in it.
    b.plan({ { 25, -5.2f }, { 21, -7.2f }, { 15, -8.0f }, { 11, -9.4f }, { -2, -9.8f }, { -12, -9.8f }, { -18, -9.4f }, { -22.4f, -8.2f },
             { -22.4f, 8.2f }, { -18, 9.4f }, { -12, 9.8f }, { 2, 9.8f }, { 11, 9.4f }, { 15, 8.0f }, { 21, 7.2f }, { 25, 5.2f } }, 3.0f, 6.8f, shell).t(T_PANEL, 7).bev(0.9f);
    b.slab(-3.6f, 0, 13.4f, 8.4f, 6.8f, 0.7f, C(0x39435A)).m(MATTE).t(T_PANEL, 6).bev(0.3f);   // the tub floor
    b.plan({ { 24, -4.4f }, { 21, -5.8f }, { 17, -6.7f }, { 12.5f, -7.4f }, { 12.5f, 7.4f }, { 17, 6.7f }, { 21, 5.8f }, { 24, 4.4f } }, 6.8f, 8.0f, cHull2).t(T_PANEL, 4).bev(0.4f);   // bonnet deck
    b.box(19.0f, 0, 8.2f, 5.0f, 0.9f, 0.4f, cHull).bev(0.2f);                              // centre ridge
    b.box(18.8f, 0, 8.45f, 1.3f, 5.2f, 0.25f, OWNER).bev(0.15f);                            // the owner's band across the bonnet
    b.box(23.6f, 0, 7.4f, 0.9f, 3.6f, 1.2f, shellD).t(T_GRILLE, 1.2f).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.ball(18.8f, s * 8.4f, 8.4f, 0.8f, 0.8f, 0.8f, ice).m(EMIT);                        // round lamps in the wings
        b.box(24.4f, s * 4.5f, 7.8f, 0.5f, 1.0f, 0.4f, cGlow).m(EMIT);                       // corner running lights
        b.rod({ -17.0f, s * 9.4f, 6.8f }, { -17.0f, s * 9.4f, 9.6f }, 0.5f, shellD).bev(0.2f);
    }
    // windscreen across the body, then the raised sill along both sides of the tub
    b.box(10.7f, 0, 9.6f, 0.6f, 8.0f, 1.8f, steelC).m(METAL).rz(-10).bev(0.3f);
    b.box(10.9f, 0, 9.9f, 0.3f, 7.4f, 1.3f, cGlass).m(GLASS).rz(-10).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-3.6f, s * 8.9f, 8.3f, 13.4f, 0.95f, 1.0f, cHull2).t(T_PANEL, 4).bev(0.3f);     // the sill
        b.box(-3.6f, s * 9.6f, 7.8f, 12.0f, 0.4f, 1.0f, OWNER).bev(0.15f);                   // the owner's colour along both sills
        b.box(1.0f, s * 9.9f, 6.4f, 3.0f, 0.4f, 1.6f, shellD).bev(0.2f);                     // armoured door panels
    }
    b.box(-17.4f, 0, 8.4f, 0.8f, 8.0f, 2.2f, shellD).t(T_PANEL, 4).bev(0.3f);                 // low rear bulkhead
    // seven empty seats: a cushion with a short backrest behind it (the game draws a helmet on the ones it fills)
    const float sx[7] = { 8, 8, -3, -3, -3, -14, -14 }, sy[7] = { -6, 6, -7.5f, 0, 7.5f, -5, 5 };
    for (int i = 0; i < 7; i++) {
        b.box(sx[i], sy[i], 8.0f, 1.6f, 1.45f, 0.65f, C(0x8593A8)).m(MATTE).bev(0.5f);
        b.box(sx[i] - 2.0f, sy[i], 8.3f, 0.5f, 1.75f, 1.3f, C(0x5C6A82)).m(MATTE).bev(0.3f);
    }
    // the rear: a spare on the tail, ammo boxes and the pintle ring
    b.cyl('x', -23.4f, 0, 6.4f, 3.0f, 4.6f, pal::rubber).m(RUBBER).t(T_TREAD, 2.0f, C(0x505A68)).bev(0.5f);
    b.cyl('x', -23.9f, 0, 6.4f, 1.2f, 0.6f, OWNER).m(PAINT).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-21.0f, s * 6.8f, 7.8f, 1.2f, 1.3f, 1.2f, graphite).t(T_PANEL, 2).bev(0.25f);   // ammo boxes
        b.box(-22.0f, s * 9.6f, 5.4f, 0.6f, 1.0f, 0.7f, tail).m(EMIT);
    }
    b.cyl('z', -20.0f, 0, 9.0f, 1.6f, 0.7f, steelC).m(METAL).bev(0.25f);                      // pintle mount ring
    b.cyl('z', -20.0f, 0, 10.0f, 0.7f, 0.6f, graphite).bev(0.2f);
    b.rod({ -20.0f, -8.4f, 9.0f }, { -23.2f, -12.8f, 17.6f }, 0.3f, graphite).f(F_NOSHADOW);   // antenna whip
    b.ball(-23.4f, -13.2f, 18.0f, 0.5f, 0.5f, 0.5f, cGlow).m(EMIT);
}
M3D_UNIT(U_C_JEEP, cJeep)

// ---------------------------------------------------------------- Anti-Radiation Truck: a six-wheel decontamination truck
static void cRadTruck(Model& b, Model&, int) {
    using namespace pal;
    const Color hull = C(0xD6DEE8), hullD = C(0x8E9AAC), deck = C(0xECEFF4), steel = C(0x78889C);
    const Color deco = C(0x7EFFB0);                       // decontamination green
    b.dirt = 0.25f;
    const float ax[3] = { -19.0f, -9.5f, 14.5f };
    for (int i = 0; i < 3; i++) for (int s = -1; s <= 1; s += 2) {
        b.cyl('y', ax[i], s * 9.6f, 4.4f, 4.4f, 2.4f, C(0x262C38)).m(RUBBER).t(T_TREAD, 2.4f, C(0x8090A4)).bev(0.5f);
        b.cyl('y', ax[i], s * 9.6f, 4.4f, 2.0f, 2.7f, hullD).m(METAL).bev(0.3f);
    }
    b.slab(-1.0f, 0, 25.0f, 7.8f, 6.0f, 1.8f, graphite).t(T_PANEL, 8).bev(0.4f);      // chassis rails
    // the scrubber tank: a clean white oval barrel with pale bands, end caps and a roof walkway
    b.profileX({ { -6.4f, 7.6f }, { 6.4f, 7.6f }, { 7.4f, 9.0f }, { 6.0f, 15.6f }, { 0, 16.8f }, { -6.0f, 15.6f }, { -7.4f, 9.0f } }, -20.5f, 5.5f, hull).t(T_PANEL, 8).bev(0.9f);
    b.ball(-20.5f, 0, 12.0f, 2.6f, 7.0f, 4.6f, hullD);
    b.ball(5.5f, 0, 12.0f, 2.4f, 7.0f, 4.6f, hull);
    for (int i = 0; i < 3; i++) b.box(-13.5f + i * 6.5f, 0, 15.4f, 0.8f, 7.2f, 0.7f, deck).bev(0.25f);
    b.slab(-8.0f, 0, 10.0f, 2.6f, 16.3f, 0.5f, deck).t(T_RIBS, 1.6f).bev(0.2f);        // roof walkway
    // bolted radiation-shield skirts along both flanks, with the cyan emitter strips above them
    for (int s = -1; s <= 1; s += 2) {
        b.box(-5.5f, s * 8.1f, 9.6f, 13.0f, 1.2f, 2.0f, steel).m(METAL).t(T_PANEL, 5).bev(0.3f);
        b.box(-5.5f, s * 6.5f, 12.6f, 13.0f, 0.5f, 0.7f, cGlow).m(EMIT).bev(0.15f);
    }
    // the trefoil marking on the tank roof: a pale field, three dark blades and the hub
    b.cyl('z', 0.5f, 0, 16.7f, 3.9f, 0.3f, C(0xB0F6C6)).m(MATTE).bev(0.2f);
    for (int i = 0; i < 3; i++) {
        const float a = i * 2.0944f - 1.5708f;
        b.box(0.5f + std::cos(a) * 2.1f, std::sin(a) * 2.1f, 17.0f, 1.3f, 1.0f, 0.35f, C(0x1C2420)).m(MATTE).rz(a * 57.3f).bev(0.1f);
    }
    // roof sprayer boom across the tank with its nozzles
    b.rod({ -9.0f, -6.8f, 17.6f }, { -9.0f, 6.8f, 17.6f }, 0.85f, steel).m(METAL).bev(0.2f);
    for (int i = -1; i <= 1; i++) b.box(-9.0f, i * 5.8f, 16.6f, 0.9f, 0.9f, 0.9f, deco).m(EMIT).bev(0.2f);
    // rear manifold and the side sprayer arms reaching out behind the wheels
    b.slab(-23.2f, 0, 1.3f, 6.6f, 6.2f, 4.2f, steel).m(METAL).t(T_PANEL, 3).bev(0.3f);
    for (int i = 0; i < 3; i++) b.box(-24.6f, -4.6f + i * 4.6f, 8.0f, 0.8f, 0.8f, 0.9f, deco).m(EMIT).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ -18.0f, s * 8.0f, 8.6f }, { -22.4f, s * 11.8f, 7.4f }, 0.9f, steel).m(METAL).bev(0.2f);
        b.ball(-22.9f, s * 12.2f, 7.2f, 0.9f, 0.9f, 0.9f, deco).m(EMIT);
    }
    // hose reel on the tank's rear shoulder
    b.cyl('z', -17.0f, 0, 16.9f, 2.6f, 0.7f, steel).m(METAL).bev(0.3f);
    b.cyl('z', -17.0f, 0, 17.6f, 1.5f, 0.5f, graphite).bev(0.2f);
    // cab: white composite over a steel roof, dark windshield, sensor dish, twin stacks and the decon beacon
    b.taper(17.0f, 0, 7.6f, 7.0f, 8.0f, 8.2f, 6.0f, 7.4f, hull, 0.5f, 0).t(T_PANEL, 6).bev(0.9f);
    b.box(16.6f, 0, 15.9f, 5.6f, 6.4f, 0.5f, steel).m(METAL).t(T_PANEL, 4).bev(0.3f);
    b.box(16.6f, 0, 16.35f, 3.4f, 4.6f, 0.25f, OWNER).bev(0.2f);                        // the owner's stripe on the roof
    b.box(23.4f, 0, 13.4f, 1.0f, 6.4f, 2.4f, cGlass).m(GLASS).rz(-12).bev(0.3f);
    for (int s = -1; s <= 1; s += 2) b.box(15.0f, s * 8.1f, 11.0f, 3.0f, 0.4f, 1.8f, OWNER).bev(0.15f);   // the owner's door bands
    b.box(25.6f, 0, 9.4f, 1.8f, 6.6f, 1.8f, hull).t(T_PANEL, 3).bev(0.4f);              // bonnet
    b.box(27.4f, 0, 8.8f, 0.7f, 6.4f, 1.6f, graphite).t(T_GRILLE, 1.3f).bev(0.2f);
    b.box(26.8f, 0, 6.4f, 0.9f, 8.8f, 0.9f, steel).m(METAL).bev(0.25f);                 // bumper
    for (int s = -1; s <= 1; s += 2) {
        b.ball(26.4f, s * 5.2f, 8.6f, 0.85f, 0.85f, 0.85f, ice).m(EMIT);
        b.rod({ 21.0f, s * 7.4f, 13.0f }, { 22.6f, s * 9.2f, 12.4f }, 0.35f, graphite).f(F_NOSHADOW);
        b.box(22.9f, s * 9.6f, 12.2f, 0.5f, 1.0f, 1.4f, graphite).bev(0.2f);             // mirror
        b.cyl('z', 8.0f, s * 7.4f, 13.0f, 1.3f, 3.2f, steel).m(METAL).bev(0.3f);         // exhaust stack
        b.cyl('z', 8.0f, s * 7.4f, 16.4f, 1.4f, 0.4f, graphite).bev(0.2f);
    }
    b.cyl('z', 14.4f, 0, 17.0f, 2.9f, 0.7f, steel, 0.35f).m(METAL).bev(0.3f);            // sensor dish
    b.ball(14.4f, 0, 18.0f, 0.7f, 0.7f, 0.7f, deco).m(EMIT);
    b.cyl('z', 6.0f, 0, 16.8f, 1.6f, 0.9f, steel).m(METAL).bev(0.3f);                    // decon beacon housing
    b.ball(6.0f, 0, 18.0f, 1.1f, 1.1f, 1.1f, deco).m(EMIT);
    b.box(-25.2f, 0, 9.0f, 0.7f, 8.0f, 1.4f, hullD).t(T_PANEL, 4).bev(0.25f);            // rear plate
    for (int s = -1; s <= 1; s += 2) b.box(-25.7f, s * 6.4f, 6.6f, 0.5f, 1.2f, 0.8f, tail).m(EMIT);
}
M3D_UNIT(U_C_RADTRUCK, cRadTruck)
