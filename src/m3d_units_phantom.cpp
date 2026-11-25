// One Hour - 3D models: the Phantom Jeep, a hero vehicle for both armies (see m3d.h / m3d_reg.h).
// A long, low, wide armoured jeep in matte near-black: faceted charcoal and graphite plate with lit chamfer edges, big off-road tyres,
// a roll cage and a small roof mount carrying a rotary cannon and a compact missile pod. Cyber: smooth wedge plates. Clanker: boxy,
// bolted plates, a bull bar and the spare wheel on the tail. Both are built from one silhouette so they read as one family.
#include "m3d_parts.h"

using namespace m3d;

namespace {
const Color phRecess = C(0x15171B);   // wells, underbody, tube mouths
const Color phChar = C(0x23262C);     // main armour, a slightly bluish black
const Color phPlate = C(0x2C3038);    // raised armour plates
const Color phPlate2 = C(0x353B45);   // lit top plates
const Color phEdge = C(0x4B535F);     // the lighter bevel trim along plate edges
const Color phGun = C(0x363B45);      // gunmetal: cage, hubs, mounts
const Color phBolt = C(0x6A6556);     // Clanker bolt heads, tow gear
const Color phRubber = C(0x1B1C20);
const Color phRed = C(0xFF3A2C);      // running lights, headlights, tail lamps
}

// a big off-road wheel: tall sidewall, a flat graphite rim, and (Clanker) six lug bolts
static void phantomWheel(Model& m, float x, float y, bool cyber) {
    const float s = y < 0 ? -1.0f : 1.0f;
    m.cyl('y', x, y, 4.4f, 4.4f, 3.2f, phRubber).m(RUBBER).t(T_TREAD, 2.6f, C(0x4A4E56)).bev(0.6f);
    m.cyl('y', x, y + s * 3.15f, 4.4f, 2.6f, 0.25f, phGun).m(PAINT).bev(0.35f);             // rim face
    m.cyl('y', x, y + s * 3.34f, 4.4f, 1.15f, 0.32f, cyber ? phPlate2 : phPlate).m(METAL).bev(0.2f);
    if (!cyber)
        for (int k = 0; k < 6; k++) {
            const float a = k * 1.0472f;
            m.cyl('y', x + std::cos(a) * 1.85f, y + s * 3.44f, 4.4f + std::sin(a) * 1.85f, 0.34f, 0.17f, phBolt).m(METAL).bev(0.1f);
        }
}

// a low armoured seat: cushion on the tub floor with a short backrest
static void phantomSeat(Model& m, float x, float y) {
    m.box(x, y, 8.1f, 2.2f, 2.0f, 0.65f, C(0x2A2D33)).m(MATTE).bev(0.35f);
    m.box(x - 2.2f, y, 9.0f, 0.55f, 2.1f, 1.5f, C(0x22252B)).m(MATTE).bev(0.25f);
}

// the roof mount that turns: pedestal, rotary cannon forward, compact missile pod behind. Both factions share the layout.
static void phantomTurret(Model& t, bool cyber) {
    t.ground = false;
    t.dirt = 0.05f;
    if (cyber) {
        t.taper(0, 0, 13.3f, 4.5f, 4.5f, 2.6f, 3.5f, 3.5f, phPlate).t(T_PANEL, 8, phEdge).bev(1.0f);
        t.box(0, 0, 13.42f, 4.8f, 4.8f, 0.18f, phEdge).bev(0.4f);                                  // lit base ring
        t.cyl('x', 7.0f, 0, 16.3f, 2.3f, 3.4f, C(0x3E4550), 0.88f).m(METAL).bev(0.5f);            // smooth shroud
        for (int k = 0; k < 4; k++) {
            const float a = 0.7854f + k * 1.5708f;
            t.rod({ 9.0f, std::sin(a) * 1.35f, 16.3f + std::cos(a) * 1.35f }, { 20.8f, std::sin(a) * 1.35f, 16.3f + std::cos(a) * 1.35f }, 0.66f, C(0x767E8A)).m(METAL).bev(0);
        }
        t.box(-5.7f, 0, 15.9f, 3.3f, 4.2f, 2.0f, phPlate).t(T_PANEL, 8, phEdge).bev(0.9f);          // pod box, smooth
    } else {
        t.box(0, 0, 14.4f, 4.6f, 4.6f, 1.2f, phPlate).t(T_PANEL, 4, phEdge).bev(0.5f);              // bolted pedestal
        t.box(7.2f, 0, 16.3f, 3.3f, 2.1f, 1.9f, C(0x3E4550)).m(METAL).bev(0.5f);                    // boxy cannon shroud
        for (int k = 0; k < 6; k++) {
            const float a = k * 1.0472f;
            t.rod({ 9.4f, std::sin(a) * 1.5f, 16.3f + std::cos(a) * 1.5f }, { 20.8f, std::sin(a) * 1.5f, 16.3f + std::cos(a) * 1.5f }, 0.58f, C(0x6C7480)).m(METAL).bev(0);
        }
        t.box(-5.7f, 0, 15.9f, 3.4f, 4.3f, 2.05f, phPlate).t(T_PANEL, 4, phEdge).bev(0.45f);        // pod box, riveted
        for (int i = 0; i < 3; i++) for (int j = -1; j <= 1; j += 2)
            t.cyl('x', -9.15f, j * 2.6f, 15.2f + i * 0.8f, 0.3f, 0.14f, phBolt).m(METAL).bev(0.1f);   // pod rivets
    }
    t.box(3.7f, 0, 16.2f, 2.6f, 2.3f, 1.6f, phRecess).bev(0.4f);                                   // gun cradle
    t.cyl('x', 21.2f, 0, 16.3f, 2.1f, 0.5f, C(0x5D6570)).m(METAL).bev(0.3f);                       // muzzle carrier
    t.cyl('x', 21.7f, 0, 16.3f, 1.3f, 0.12f, phRecess).bev(0.1f);                                   // bore
    // the pod's four tube mouths, seen from above
    for (int i = -1; i <= 1; i += 2) for (int j = -1; j <= 1; j += 2)
        t.cyl('z', -5.7f + i * 1.6f, j * 2.0f, 18.12f, 0.95f, 0.28f, phRecess).m(METAL).bev(0.2f);
    t.box(-9.3f, 0, 15.3f, 0.4f, 3.3f, 1.0f, phRecess).t(T_GRILLE, 1.0f).bev(0.2f);               // pod exhaust vent
    t.box(1.5f, -3.3f, 17.2f, 1.1f, 0.9f, 0.8f, phPlate2).bev(0.3f);                               // gunner's sight
    t.box(2.65f, -3.3f, 17.2f, 0.16f, 0.55f, 0.34f, phRed).m(EMIT);                                // its red lens
    t.rod({ -7.6f, 3.9f, 17.2f }, { -9.4f, 5.8f, 22.0f }, 0.16f, phGun).f(F_NOSHADOW);              // whip aerial
}

// the shared body: the same plan, wheelbase, tub, bonnet and cage in both liveries; the plates differ
static void phantom(Model& b, Model& t, bool cyber) {
    b.dirt = 0.42f;
    for (float ax : { -19.0f, 18.5f })
        for (int s = -1; s <= 1; s += 2) phantomWheel(b, ax, s * 13.2f, cyber);

    // chassis and the lower hull: long, wide and cut back at the nose and tail
    b.slab(-1.0f, 0, 26.0f, 8.4f, 4.2f, 3.2f, phRecess).t(T_PANEL, 9).bev(0.3f);
    b.plan({ { 31.2f, -5.6f }, { 29, -8.0f }, { 23, -9.9f }, { 15, -11.1f }, { 3, -11.6f }, { -15, -11.6f }, { -25, -10.9f }, { -29.6f, -9.0f },
             { -29.6f, 9.0f }, { -25, 10.9f }, { -15, 11.6f }, { 3, 11.6f }, { 15, 11.1f }, { 23, 9.9f }, { 29, 8.0f }, { 31.2f, 5.6f } },
           3.6f, 7.4f, phChar).t(T_PANEL, 9, phEdge).bev(1.0f);
    b.slab(-2.0f, 0, 12.4f, 8.6f, 7.4f, 0.45f, C(0x202329)).m(MATTE).t(T_RIBS, 1.8f).bev(0.2f);     // tub floor

    for (int s = -1; s <= 1; s += 2) {
        const float y0 = s < 0 ? -11.8f : 10.3f, y1 = s < 0 ? -10.3f : 11.8f;
        b.profileY({ { -25.4f, 7.2f }, { -25.4f, 10.4f }, { -20.5f, 10.9f }, { 6.0f, 10.9f }, { 10.2f, 10.0f }, { 10.2f, 7.2f } },
                   y0, y1, phPlate).t(T_PANEL, 7, phEdge).bev(0.7f);                                      // faceted side armour
        b.box(-6.5f, s * 11.35f, 10.9f, 16.6f, 0.62f, 0.42f, phEdge).bev(0.3f);                          // lit top rail
        b.box(-10.0f, s * 11.85f, 9.3f, 5.2f, 0.16f, 0.4f, OWNER).bev(0.05f);                            // thin owner stripe on the flank
        b.box(-7.0f, s * 10.9f, 5.6f, 14.0f, 0.45f, 0.8f, phPlate2).bev(0.35f);                          // rocker armour
        b.box(-4.0f, s * 11.2f, 4.5f, 12.0f, 0.35f, 0.45f, phRecess).bev(0.2f);                          // side step
    }
    b.box(-25.6f, 0, 9.0f, 0.9f, 11.2f, 2.4f, phPlate).t(T_PANEL, 6, phEdge).bev(0.6f);                  // rear bulkhead of the tub

    // the bonnet: a long wedge; Cyber smooth, Clanker plated and slatted
    b.profileY({ { 10.5f, 6.9f }, { 10.5f, 10.0f }, { 24.5f, 10.0f }, { 31.2f, 8.3f }, { 31.2f, 6.4f }, { 25.0f, 6.4f } },
               -10.2f, 10.2f, phPlate).t(cyber ? T_NONE : T_PANEL, cyber ? 4.0f : 5.0f, phEdge).bev(cyber ? 1.1f : 0.55f);
    b.box(22.0f, 0, 10.26f, 1.1f, 7.0f, 0.14f, OWNER).bev(0.05f);                                       // the owner's band across the bonnet
    if (cyber) {
        b.box(26.8f, 0, 10.08f, 2.8f, 8.2f, 0.12f, phPlate2).bev(0.4f);                                  // flush raised plate
        for (int s = -1; s <= 1; s += 2) b.box(26.0f, s * 7.6f, 10.12f, 2.6f, 0.5f, 0.06f, phRecess).bev(0.05f);   // louvre slots
        b.box(30.4f, 0, 5.4f, 2.6f, 6.2f, 0.3f, phPlate2).ry(14).bev(0.3f);                              // low splitter blade
    } else {
        b.box(20.0f, 0, 10.05f, 7.0f, 8.6f, 0.14f, phPlate2).bev(0.3f);                                  // bolted bonnet plate
        b.box(16.5f, 0, 10.16f, 3.2f, 6.8f, 0.1f, phRecess).t(T_GRILLE, 1.3f).bev(0.15f);                // vent slots
        for (int i = 0; i < 4; i++) for (int s = -1; s <= 1; s += 2)
            b.cyl('z', 13.8f + i * 4.4f, s * 10.5f, 10.05f, 0.36f, 0.12f, phBolt).m(METAL).bev(0.1f);    // bonnet bolts
    }
    b.box(31.35f, 0, 7.9f, 0.5f, 6.2f, 1.25f, phRecess).t(T_GRILLE, 1.1f).bev(0.2f);                     // grille
    b.box(31.6f, 0, 6.1f, 0.9f, 11.6f, 1.1f, phChar).bev(0.5f);                                          // bumper

    // fender flares over the tyres, with a lit lip and a red marker at each corner
    for (float ax : { -19.0f, 18.5f }) for (int s = -1; s <= 1; s += 2) {
        b.box(ax, s * 13.5f, 9.3f, 6.7f, 3.6f, 0.85f, phPlate).t(T_PANEL, 6, phEdge).bev(0.7f);
        b.box(ax, s * 16.95f, 9.9f, 6.4f, 0.22f, 0.28f, phEdge).bev(0.15f);                              // lit outer lip
        b.box(ax + (ax < 0 ? -5.4f : 5.4f), s * 17.0f, 9.55f, 0.35f, 0.3f, 0.3f, phRed).m(EMIT);         // running light
        b.box(ax, s * 16.9f, 7.4f, 6.0f, 0.18f, 1.1f, phRecess).bev(0.2f);                               // mud flap
    }

    // raked windscreen in a heavy frame, split by a centre bar
    b.box(10.6f, 0, 8.9f, 0.7f, 9.1f, 0.7f, phPlate).bev(0.3f);                                          // dash
    b.box(10.1f, 0, 10.8f, 0.55f, 8.9f, 2.1f, phGun).m(PAINT).ry(-14).bev(0.3f);
    b.box(10.1f, 0, 10.8f, 0.24f, 8.25f, 1.8f, C(0x0F141B)).m(GLASS).ry(-14).bev(0.1f);
    b.box(10.1f, 0, 10.8f, 0.62f, 0.3f, 2.1f, phEdge).ry(-14).bev(0.1f);                                 // centre bar

    // roll cage: two hoops, side rails and a small roof plate carrying the mount
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ 3.2f, s * 9.9f, 7.6f }, { 3.2f, s * 9.9f, 13.4f }, 0.6f, phGun).m(PAINT).bev(0.3f);
        b.rod({ -16.0f, s * 9.9f, 7.6f }, { -16.0f, s * 9.9f, 13.4f }, 0.6f, phGun).m(PAINT).bev(0.3f);
        b.rod({ -16.0f, s * 9.9f, 13.4f }, { 3.2f, s * 9.9f, 13.4f }, 0.55f, phGun).m(PAINT).bev(0.3f);
        b.rod({ -16.0f, s * 9.9f, 11.4f }, { 3.2f, s * 9.9f, 11.4f }, 0.32f, phEdge).m(PAINT).bev(0);     // lit mid rail
    }
    b.rod({ 3.2f, -9.9f, 13.4f }, { 3.2f, 9.9f, 13.4f }, 0.55f, phGun).m(PAINT).bev(0.3f);
    b.rod({ -16.0f, -9.9f, 13.4f }, { -16.0f, 9.9f, 13.4f }, 0.55f, phGun).m(PAINT).bev(0.3f);
    b.box(0, 0, 13.25f, 5.5f, 5.5f, 0.35f, phPlate).t(T_PANEL, 5).bev(0.4f);                             // roof plate
    b.cyl('z', 0, 0, 13.5f, 4.75f, 0.28f, phRecess).m(METAL).bev(0.25f);                                 // turret ring

    // crew: two front seats and a rear pair
    phantomSeat(b, 5.0f, -5.7f);
    phantomSeat(b, 5.0f, 5.7f);
    phantomSeat(b, -8.5f, -5.7f);
    phantomSeat(b, -8.5f, 5.7f);
    b.box(1.0f, 0, 8.0f, 1.2f, 4.2f, 0.7f, C(0x25282E)).m(MATTE).bev(0.2f);                              // centre console

    // the tail: red lamps inboard, a rear bumper and a red strip under the tailgate
    b.box(-29.3f, 0, 8.6f, 0.9f, 11.4f, 2.6f, phPlate).t(T_PANEL, 5, phEdge).bev(0.6f);
    b.box(-30.2f, 0, 5.6f, 0.8f, 11.2f, 0.9f, phChar).bev(0.4f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-30.2f, s * 8.9f, 9.1f, 0.3f, 1.15f, 0.4f, phRed).m(EMIT);                                 // tail lamps
        b.box(-30.25f, s * 4.6f, 7.1f, 0.22f, 2.0f, 0.16f, phRed).m(EMIT);                               // low running strip
    }

    // Cyber: long smooth edge trim, no clutter. Clanker: bolts, bull bar, jerry cans and the spare on the tail.
    if (cyber) {
        for (int s = -1; s <= 1; s += 2) {
            b.box(19.0f, s * 9.6f, 10.15f, 9.0f, 0.18f, 0.14f, phEdge).bev(0.05f);                       // lit bonnet edge
            b.box(-20.0f, s * 9.6f, 10.95f, 4.0f, 0.2f, 0.18f, phEdge).bev(0.05f);
            b.box(17.9f, s * 10.6f, 8.2f, 3.2f, 0.5f, 0.7f, phPlate2).bev(0.4f);                         // flush vent pod
        }
        b.rod({ -28.0f, -8.6f, 9.2f }, { -31.4f, -12.4f, 17.4f }, 0.28f, phGun).f(F_NOSHADOW);           // aerial
        b.ball(-31.4f, -12.4f, 17.8f, 0.45f, 0.45f, 0.45f, phRed).m(EMIT);
    } else {
        for (int s = -1; s <= 1; s += 2) {
            for (int i = 0; i < 5; i++) {
                b.cyl('y', -22.0f + i * 6.6f, s * 11.95f, 9.8f, 0.4f, 0.16f, phBolt).m(METAL).bev(0.1f);  // side plate bolts
                b.cyl('y', -22.0f + i * 6.6f, s * 11.95f, 8.0f, 0.34f, 0.14f, phBolt).m(METAL).bev(0.1f);
            }
            b.box(-28.1f, s * 7.2f, 9.0f, 1.0f, 1.3f, 1.7f, C(0x2E3128)).bev(0.3f);                      // jerry cans
            b.box(33.0f, s * 7.6f, 7.9f, 0.55f, 0.55f, 2.5f, phGun).m(PAINT).bev(0.2f);                  // bull bar uprights
            b.rod({ 31.2f, s * 9.4f, 6.1f }, { 33.0f, s * 7.6f, 5.6f }, 0.5f, phGun).m(PAINT).bev(0);      // braces
            b.box(-29.6f, s * 10.2f, 4.8f, 0.7f, 0.5f, 0.7f, phBolt).m(METAL).bev(0.2f);                 // tow hooks
        }
        b.rod({ 33.0f, -11.2f, 10.3f }, { 33.0f, 11.2f, 10.3f }, 0.55f, phGun).m(PAINT).bev(0.3f);
        b.rod({ 33.0f, -11.2f, 7.2f }, { 33.0f, 11.2f, 7.2f }, 0.5f, phGun).m(PAINT).bev(0.3f);
        b.cyl('x', -31.6f, 0, 8.0f, 3.5f, 1.1f, phRubber).m(RUBBER).t(T_BAND, 2.0f, C(0x4C4A3A)).bev(0.4f);   // spare on the tail
        b.cyl('x', -32.8f, 0, 8.0f, 1.4f, 0.25f, phPlate2).m(METAL).bev(0.2f);
    }

    // the turret's shade on the cage and deck, then the turning turret itself
    b.cyl('z', 0, 0, 16.2f, 4.5f, 2.9f, phPlate).f(F_SHADOWONLY);
    phantomTurret(t, cyber);
}

static void cPhantom(Model& b, Model& t, int) { phantom(b, t, true); }
static void kPhantom(Model& b, Model& t, int) { phantom(b, t, false); }

M3D_UNIT(U_C_PHANTOM, cPhantom)
M3D_UNIT(U_K_PHANTOM, kPhantom)
