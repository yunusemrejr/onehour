// One Hour - 3D models: the rotorcraft and drones (see m3d.h / m3d_reg.h).
// Cyber: pale ceramic plate over graphite, cobalt trim, cyan light. Clanker: olive drab, earth, rust, gunmetal, amber light.
// Relic: weathered warm grey, sun-faded ochre, bronze gone green with verdigris.
// Main rotor blades are NOT modelled: the game spins them over the masts, and body.hub is the height of the hub top.
#include "m3d_parts.h"

using namespace m3d;

// ---------------------------------------------------------------- Cyber Wraith Drone: a stealth flying wing, twin pusher engines
static void cWraith(Model& b, Model&, int) {
    using namespace pal;
    b.ground = false; b.dirt = 0.0f;
    // the wing: a dark graphite belly under pale ceramic plates, a raised spine with the sensor canopy
    b.plan({ { 20, 0 }, { 8, -7 }, { -13, -6 }, { -15, 0 }, { -13, 6 }, { 8, 7 } }, 1.2f, 2.4f, cDark).t(T_PANEL, 6).bev(0.4f);
    b.plan({ { 26, 0 }, { 12, -9 }, { -16, -8 }, { -18, 0 }, { -16, 8 }, { 12, 9 } }, 2.0f, 4.8f, cHull).t(T_PANEL, 7).bev(0.8f);
    for (int s = -1; s <= 1; s += 2) {
        b.plan({ { 10, s * 9.0f }, { -19, s * 24.0f }, { -24, s * 21.0f }, { -14, s * 6.0f } }, 2.4f, 4.2f, cHull2).t(T_PANEL, 6).bev(0.4f);
        b.plan({ { -17, s * 20.5f }, { -22.5f, s * 22.0f }, { -23.5f, s * 19.5f }, { -18, s * 18.5f } }, 4.2f, 4.5f, OWNER).bev(0.12f);
        // weapon pods along the leading edge, with their emitters
        b.rod({ -4, s * 13.5f, 3.4f }, { 8, s * 10.0f, 3.4f }, 1.5f, cMid).m(PAINT).bev(0.3f);
        b.ball(8.2f, s * 9.8f, 3.4f, 1.2f, 1.2f, 1.2f, cDark);
        b.ball(9.6f, s * 9.5f, 3.4f, 0.45f, 0.45f, 0.45f, cGlow).m(EMIT);
        // engine nacelle: intake grille in front, glowing exhaust behind
        b.taper(-16, s * 5.0f, 3.6f, 3.4f, 1.8f, 2.6f, 2.6f, 1.5f, cHull2).t(T_PANEL, 4).bev(0.4f);
        b.box(-12.4f, s * 5.0f, 4.9f, 0.9f, 1.5f, 1.0f, cDark).t(T_GRILLE, 0.9f).bev(0.2f);
        b.cyl('x', -19.4f, s * 5.0f, 4.6f, 1.5f, 1.0f, cDark, 0.85f).bev(0.2f);
        b.ball(-20.3f, s * 5.0f, 4.6f, 0.6f, 1.1f, 1.1f, cGlow).m(EMIT);
        // canted tail fins, the owner's colour on their tips
        b.box(-17.5f, s * 6.2f, 6.3f, 3.2f, 0.4f, 1.8f, cHull2).bev(0.3f).rx(s * -26);
        b.box(-17.5f, s * 7.2f, 7.6f, 2.4f, 0.42f, 0.5f, OWNER).rx(s * -26);
        b.ball(-22.5f, s * 20.0f, 4.4f, 0.5f, 0.5f, 0.5f, s < 0 ? C(0xFF5A48) : C(0x5AFF8C)).m(EMIT);   // nav lights
    }
    // the spine, the owner's band and the glass sensor canopy
    b.plan({ { 20, 0 }, { 9, -6 }, { -14, -6 }, { -16, 0 }, { -14, 6 }, { 9, 6 } }, 4.4f, 6.0f, cHull).t(T_PANEL, 5).bev(0.5f);
    b.box(-6, 0, 6.15f, 5.0f, 2.4f, 0.3f, OWNER).bev(0.12f);
    b.cyl('z', 6, 0, 6.1f, 4.2f, 0.3f, cDark).bev(0.2f);
    b.dome(6, 0, 6.2f, 4.2f, 2.7f, 1.9f, cGlass).m(GLASS);
    // nose sensor block and its lit lens
    b.box(24.4f, 0, 3.6f, 1.8f, 2.2f, 1.1f, cDark).bev(0.3f);
    b.ball(26.2f, 0, 3.6f, 0.7f, 1.4f, 0.9f, cGlow).m(EMIT);
    b.rod({ -12, 0, 6.6f }, { -18.5f, 0, 9.4f }, 0.17f, cDark).bev(0).f(F_NOSHADOW);   // aerial
}
M3D_UNIT(U_C_AIR, cWraith)

// ---------------------------------------------------------------- Clanker Vulture Gunship: an armoured pod, tail boom, stub wings with rocket pods
static void kVulture(Model& b, Model&, int) {
    using namespace pal;
    b.ground = false; b.dirt = 0.0f;
    // the fuselage: a rounded pod in three stages, a glazed nose, a chin gun
    b.taper(1, 0, 2.2f, 16.0f, 7.6f, 2.8f, 15.0f, 7.2f, kHull).t(T_CAMO, 7, kHull2).bev(0.8f);
    b.taper(0.5f, 0, 5.0f, 15.0f, 7.2f, 3.4f, 12.4f, 5.6f, kHull).t(T_CAMO, 6, kHull2).bev(0.8f);
    b.taper(-1, 0, 8.4f, 12.0f, 5.4f, 1.6f, 9.0f, 4.0f, kHull2).t(T_PANEL, 5).bev(0.5f);
    b.taper(17.5f, 0, 2.6f, 5.6f, 5.4f, 5.6f, 3.0f, 2.8f, kHull, 1.6f, 0).t(T_CAMO, 5, kHull2).bev(0.7f);
    b.box(-13, 0, 8.9f, 4.6f, 4.6f, 0.4f, kHull2).t(T_GRILLE, 1.6f).bev(0.2f);            // engine deck louvres
    b.dome(11, 0, 6.6f, 7.4f, 4.6f, 2.8f, kGlass).m(GLASS);                               // canopy
    b.cyl('z', 11, 0, 6.5f, 7.0f, 0.3f, kDark).bev(0.2f);
    b.box(19.0f, 0, 2.6f, 2.2f, 1.7f, 1.4f, kSteel).m(METAL).bev(0.4f);                   // chin gun mount
    b.rod({ 20.0f, 0, 2.3f }, { 26.6f, 0, 1.9f }, 0.75f, kSteel).m(METAL).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) {
        // stub wings on pylons, a rocket pod under each tip
        b.box(-2, s * 11.0f, 8.6f, 4.4f, 9.2f, 0.75f, kHull).t(T_CAMO, 6, kHull2).bev(0.4f);
        b.box(-2, s * 19.6f, 8.6f, 4.0f, 1.3f, 0.55f, OWNER).bev(0.2f);                    // the owner's colour on the wing tips
        b.box(1, s * 14.4f, 8.2f, 1.4f, 0.7f, 0.8f, kHull2).bev(0.2f);                      // pylon
        b.cyl('x', 2, s * 14.4f, 6.6f, 2.3f, 6.6f, kSteel, 0.92f).m(METAL).t(T_PANEL, 4).bev(0.4f);
        b.cyl('x', 9.0f, s * 14.4f, 6.6f, 2.3f, 0.7f, kDark).bev(0.2f);                     // pod face
        for (int k = -1; k <= 1; k++) b.cyl('x', 9.7f, s * 14.4f + k * 1.35f, 6.6f, 0.52f, 0.7f, kRust).m(METAL).bev(0);   // rocket tubes
        // exhausts at the boom root, and the nav lights
        b.cyl('x', -7.6f, s * 4.4f, 8.8f, 1.3f, 1.6f, kRust, 0.9f).m(METAL).bev(0.2f);
        b.ball(-9.4f, s * 4.4f, 8.8f, 0.4f, 0.9f, 0.9f, kGlow).m(EMIT);
        b.box(14.6f, s * 6.4f, 4.2f, 0.7f, 1.1f, 0.5f, kGlow).m(EMIT);                      // landing lamps
        // skids and their struts
        b.rod({ -7, s * 6.4f, 1.5f }, { 11, s * 6.4f, 1.5f }, 0.5f, kSteel).m(METAL).bev(0);
        b.rod({ 6, s * 6.4f, 1.6f }, { 7.5f, s * 4.2f, 3.4f }, 0.4f, kSteel).m(METAL).bev(0);
        b.rod({ -4, s * 6.4f, 1.6f }, { -3, s * 4.2f, 3.4f }, 0.4f, kSteel).m(METAL).bev(0);
    }
    // the tail boom, its fin, the tail rotor and the tailplane
    b.cyl('x', -16.5f, 0, 6.6f, 2.6f, 11.4f, kHull, 0.66f).t(T_PANEL, 5).bev(0.5f);
    b.box(-10.5f, 0, 6.6f, 1.6f, 3.2f, 1.6f, OWNER).bev(0.25f);                             // the owner's band on the boom
    b.box(-6.0f, 0, 10.05f, 5.0f, 2.6f, 0.3f, OWNER).bev(0.12f);                             // and the owner's panel on the engine deck
    b.box(-27.4f, 0, 8.2f, 1.5f, 0.5f, 3.0f, kHull2).bev(0.3f);                              // fin
    b.box(-27.4f, 0, 10.4f, 1.2f, 0.55f, 1.2f, OWNER).bev(0.15f);
    b.box(-26.0f, 0, 6.4f, 4.2f, 7.4f, 0.42f, kHull2).bev(0.2f);                            // tailplane
    b.cyl('y', -27.6f, 1.5f, 7.0f, 0.8f, 0.9f, kSteel).m(METAL).bev(0.2f);                   // tail rotor hub
    b.rod({ -30.2f, 1.5f, 7.0f }, { -25.0f, 1.5f, 7.0f }, 0.45f, kDark).bev(0);              // its two blades
    b.rod({ -27.6f, 1.5f, 3.3f }, { -27.6f, 1.5f, 10.7f }, 0.45f, kDark).bev(0);
    // rotor mast and hub (the game spins the blades over them)
    b.cyl('z', -2, 0, 10.6f, 1.1f, 1.6f, kSteel).m(METAL).bev(0.2f);
    b.cyl('z', -2, 0, 12.4f, 2.4f, 0.45f, kSteel).m(METAL).bev(0.3f);
    b.dome(-2, 0, 12.85f, 2.0f, 2.0f, 0.75f, kDark).bev(0);
    b.hub = 13.6f;
}
M3D_UNIT(U_K_AIR, kVulture)

// ---------------------------------------------------------------- Cyber Hornet: a slim electric attack helicopter, ducted tail fan, laser pods
static void cHornet(Model& b, Model&, int) {
    using namespace pal;
    b.ground = false; b.dirt = 0.0f;
    // the fuselage: a narrow armoured pod, a stepped tandem canopy, a sensor nose
    b.plan({ { 22, 0 }, { 17, -5.2f }, { 4, -6.4f }, { -8, -4.8f }, { -11, 0 }, { -8, 4.8f }, { 4, 6.4f }, { 17, 5.2f } }, 2.4f, 7.4f, cHull).t(T_PANEL, 6).bev(0.9f);
    b.plan({ { 20, 0 }, { 15, -4.0f }, { 2, -5.0f }, { -8, -3.6f }, { -10, 0 }, { -8, 3.6f }, { 2, 5.0f }, { 15, 4.0f } }, 7.2f, 8.0f, cHull2).t(T_PANEL, 5).bev(0.4f);
    b.box(-9.5f, 0, 7.9f, 1.8f, 3.4f, 0.35f, cTrim).bev(0.2f);                              // cobalt trim across the cabin back
    b.dome(12.5f, 0, 7.4f, 5.4f, 2.9f, 1.9f, cGlass).m(GLASS);                              // pilot's canopy
    b.dome(5.5f, 0, 7.6f, 3.9f, 2.5f, 1.5f, cGlass).m(GLASS);                               // gunner's canopy
    b.ball(23.0f, 0, 5.4f, 2.2f, 2.2f, 1.9f, cDark);                                        // sensor ball
    b.ball(24.4f, 0, 5.0f, 0.8f, 1.3f, 0.9f, cGlow).m(EMIT);
    b.box(19.5f, 0, 3.4f, 2.4f, 2.6f, 0.9f, cMid).bev(0.3f);                                // nose chin plate
    for (int s = -1; s <= 1; s += 2) {
        // stub wings with the twin laser pods
        b.box(-1, s * 10.0f, 6.8f, 3.4f, 8.6f, 0.7f, cHull).t(T_PANEL, 5).bev(0.4f);
        b.box(-1, s * 17.2f, 6.8f, 3.2f, 1.2f, 0.5f, OWNER).bev(0.15f);                     // the owner's colour on the tips
        b.box(-1, s * 9.4f, 7.52f, 2.6f, 2.4f, 0.2f, OWNER).bev(0.1f);                      // and on the wing uppers
        b.cyl('x', 2, s * 15.0f, 6.4f, 1.85f, 5.9f, cMid).m(PAINT).t(T_PANEL, 4).bev(0.5f);
        b.cyl('x', 7.6f, s * 15.0f, 6.4f, 1.5f, 0.55f, cDark).bev(0.2f);
        b.ball(8.3f, s * 15.0f, 6.4f, 0.5f, 1.0f, 1.0f, cGlow).m(EMIT);
        b.cyl('x', -3.4f, s * 15.0f, 6.4f, 1.6f, 0.5f, cHull2).bev(0.2f);
        // light bars and the landing light
        b.box(16.0f, s * 4.6f, 5.0f, 1.2f, 0.6f, 0.4f, cGlow).m(EMIT);
        // skids
        b.rod({ -4, s * 3.6f, 1.5f }, { 12, s * 3.6f, 1.5f }, 0.45f, cMid).m(METAL).bev(0);
        b.rod({ 9, s * 3.6f, 1.6f }, { 10, s * 2.4f, 3.0f }, 0.35f, cMid).m(METAL).bev(0);
        b.rod({ -1, s * 3.6f, 1.6f }, { 0, s * 2.4f, 3.0f }, 0.35f, cMid).m(METAL).bev(0);
    }
    // tail boom, tailplane and the ducted fan at the end of the boom
    b.cyl('x', -14.0f, 0, 6.4f, 2.2f, 10.6f, cHull2, 0.82f).t(T_PANEL, 5).bev(0.5f);
    b.box(-21.0f, 0, 7.2f, 1.2f, 6.0f, 0.45f, cHull).bev(0.25f);
    for (int s = -1; s <= 1; s += 2) b.box(-21.0f, s * 5.6f, 7.2f, 1.0f, 0.7f, 0.4f, OWNER).bev(0.1f);
    b.cyl('z', -25.2f, 0, 8.2f, 4.5f, 1.3f, cMid).m(PAINT).bev(0.6f);                       // the fan duct
    b.cyl('z', -25.2f, 0, 9.35f, 3.5f, 0.25f, C(0x0E1E2C)).bev(0.2f);                       // its dark face
    b.cyl('z', -25.2f, 0, 9.6f, 0.9f, 0.35f, cMid).m(METAL).bev(0.2f);                       // fan hub
    b.cyl('z', -25.2f, 0, 9.05f, 3.9f, 0.22f, cGlow).m(EMIT);                                // glow under the fan ring
    // rotor mast and hub
    b.cyl('z', 0, 0, 9.2f, 1.0f, 1.7f, cMid).m(METAL).bev(0.2f);
    b.cyl('z', 0, 0, 11.0f, 2.2f, 0.4f, cMid).m(METAL).bev(0.3f);
    b.dome(0, 0, 11.4f, 1.8f, 1.8f, 0.7f, cDark);
    b.ball(0, 0, 12.2f, 0.4f, 0.4f, 0.35f, cGlow).m(EMIT);
    b.hub = 12.55f;
}
M3D_UNIT(U_C_HELI, cHornet)

// ---------------------------------------------------------------- the heavy cargo lifters (the biggest airframes on the field)
// One function, a faction flag: the old painters are cargoBody(A, cyber) in art.cpp, on the same 64 px plan.
// Cyber: a pale twin-boom lifter, H-tail, a glazed nose, glowing emitter strips. Clanker: an olive tandem-rotor workhorse with a
// corrugated spine, rust ramp, hazard trim, exposed exhausts.
static void cargoLifter(Model& b, Model&, int cyber) {
    using namespace pal;
    const Color hull = cyber ? C(0xC4CED8) : C(0x60682E), hullD = cyber ? C(0x67707A) : C(0x3C4224);
    const Color glass = cyber ? C(0x123A4A) : C(0x2A3A34), trim = cyber ? cGlow : kHaz;
    const Color hubC = cyber ? C(0x464E5C) : C(0x46483A);
    b.ground = false; b.dirt = 0.0f;
    // the fuselage capsule: a low wide body, a raised deck step, a lifting tail cone
    b.taper(2, 0, 2.6f, 21.0f, 8.4f, 7.6f, 18.0f, 7.4f, hull).t(cyber ? T_PANEL : T_CAMO, 9, hullD).bev(1.1f);
    b.taper(1.0f, 0, 10.2f, 19.0f, 7.6f, 3.0f, 15.0f, 5.6f, hull).t(cyber ? T_PANEL : T_CAMO, 8, hullD).bev(0.9f);
    b.taper(-16, 0, 8.4f, 11.0f, 5.4f, 3.2f, 8.0f, 3.2f, hullD, -8.0f, 0).t(T_PANEL, 6).bev(0.6f);   // tail cone, lifted
    // the spine: a corrugated deck (Clanker) or a smooth deck with the owner's band (Cyber)
    if (cyber) {
        b.box(-3, 0, 13.4f, 14.0f, 5.2f, 0.35f, cHull).t(T_PANEL, 8).bev(0.3f);
        b.box(-2, 0, 13.8f, 8.0f, 2.4f, 0.3f, OWNER).bev(0.12f);
        b.box(2, 0, 13.5f, 1.2f, 5.4f, 0.4f, cMid).bev(0.2f);
    } else {
        b.box(-3, 0, 13.5f, 14.0f, 5.2f, 0.4f, kHull2).t(T_RIBS, 1.8f).bev(0.3f);
        b.box(-2, 0, 13.85f, 8.4f, 2.2f, 0.28f, OWNER).bev(0.12f);
        for (int s = -1; s <= 1; s += 2) b.box(-3, s * 5.0f, 14.1f, 10.0f, 0.35f, 0.35f, kSteel).m(METAL).bev(0.1f);
    }
    // the glazed cockpit at the nose, its brow and the nose bumper
    b.taper(24.0f, 0, 7.4f, 4.4f, 5.6f, 4.2f, 2.6f, 3.6f, glass, 1.6f, 0).m(GLASS).bev(0.6f);
    b.box(26.8f, 0, 9.6f, 1.6f, 5.4f, 0.45f, hullD).bev(0.2f);
    b.box(23.6f, 0, 11.55f, 3.8f, 4.2f, 0.28f, hull).bev(0.2f);
    b.box(28.6f, 0, 4.6f, 1.2f, 3.6f, 1.6f, hullD).bev(0.4f);
    for (int s = -1; s <= 1; s += 2) b.box(27.2f, s * 4.0f, 5.0f, 0.7f, 1.0f, 0.6f, kGlow).m(EMIT);
    // side sponsons (hazard-trimmed) and the rear loading ramp, let down with its tread
    for (int s = -1; s <= 1; s += 2) {
        b.box(1, s * 9.2f, 5.0f, 13.0f, 1.6f, 2.4f, hullD).bev(0.5f);
        b.box(1, s * 10.6f, 6.4f, 12.0f, 0.4f, 0.7f, trim).t(T_HAZARD, 2.4f, hullD).bev(0.12f);
        b.box(1, s * 10.6f, 3.4f, 8.0f, 0.35f, 0.5f, cyber ? trim : kSteel).bev(0.1f);
        if (cyber) b.box(1, s * 9.1f, 7.6f, 11.0f, 0.35f, 0.35f, trim).m(EMIT);              // emitter strip
    }
    b.profileY({ { -29.0f, 2.6f }, { -19.0f, 2.6f }, { -19.0f, 8.0f }, { -29.0f, 6.4f } }, -4.6f, 4.6f, cyber ? C(0x5C6470) : kRust).m(cyber ? PAINT : MATTE).t(T_RIBS, 1.6f).bev(0.4f);
    for (int i = 0; i < 3; i++) b.box(-23.0f - i * 2.4f, 0, 4.4f + i * 0.35f, 0.35f, 4.2f, 0.25f, kDark).bev(0.05f);
    // engine pods on the rear pylon, one either side of the aft hub
    for (int s = -1; s <= 1; s += 2) {
        b.cyl('x', -10.0f, s * 9.6f, 12.2f, 2.8f, 5.4f, hullD, 0.86f).m(PAINT).t(T_PANEL, 4).bev(0.6f);
        b.cyl('x', -4.4f, s * 9.6f, 12.2f, 2.4f, 0.5f, kDark).t(T_GRILLE, 1.1f).bev(0.2f);
        b.cyl('x', -15.6f, s * 9.6f, 12.2f, 2.2f, 0.7f, cyber ? kSteel : kRust).m(METAL).bev(0.2f);
        b.ball(-16.5f, s * 9.6f, 12.2f, 0.4f, 1.6f, 1.6f, cyber ? trim : kGlow).m(EMIT);
        b.box(-10.0f, s * 7.2f, 11.0f, 3.4f, 0.8f, 1.4f, hull).bev(0.3f);                    // its pylon
    }
    // the two rotor masts and hubs: a steel collar under a bright mast cap (no blades: the game spins them)
    for (int s = -1; s <= 1; s += 2) {
        const float hx = s * 16.8f;
        b.cyl('z', hx, 0, 14.4f, 4.6f, 1.0f, hubC).m(PAINT).bev(0.5f);                        // hub fairing
        b.cyl('z', hx, 0, 15.9f, 1.6f, 1.2f, hullD).m(METAL).bev(0.2f);                       // mast
        b.cyl('z', hx, 0, 17.3f, 2.6f, 0.45f, hubC).m(METAL).bev(0.3f);                       // collar
        b.ball(hx, 0, 18.1f, 1.0f, 1.0f, 0.7f, trim).m(EMIT);
        if (cyber) for (int k = -1; k <= 1; k += 2) b.box(hx, k * 2.0f, 14.8f, 1.6f, 0.3f, 0.5f, OWNER).bev(0.1f);
    }
    b.hub = 18.8f;
    if (cyber) {
        // twin tail booms with an H-tail and the owner's colour on the fins
        for (int s = -1; s <= 1; s += 2) {
            b.cyl('x', -24.6f, s * 5.2f, 8.6f, 1.9f, 6.2f, hull, 0.7f).t(T_PANEL, 5).bev(0.4f);
            b.box(-29.8f, s * 5.2f, 10.4f, 1.4f, 0.55f, 2.8f, hullD).bev(0.25f);              // fin
            b.box(-29.8f, s * 5.2f, 12.8f, 1.3f, 0.6f, 1.4f, OWNER).bev(0.15f);
            b.cyl('y', -30.2f, s * 5.2f, 8.6f, 0.6f, 0.4f, cMid).m(METAL).bev(0.15f);         // tail rotor hub
            b.rod({ -32.0f, s * 5.2f, 8.6f }, { -28.4f, s * 5.2f, 8.6f }, 0.35f, cDark).bev(0);
            b.rod({ -30.2f, s * 5.2f, 6.6f }, { -30.2f, s * 5.2f, 10.6f }, 0.35f, cDark).bev(0);
            b.box(-25.0f, s * 3.6f, 7.4f, 8.0f, 0.5f, 0.7f, hullD).bev(0.2f);                  // boom brace into the body
        }
        b.box(-29.4f, 0, 13.6f, 1.0f, 5.6f, 0.5f, hullD).bev(0.2f);                            // the tailplane joining the fins
        b.box(-29.4f, 0, 13.9f, 0.7f, 2.0f, 0.35f, OWNER).bev(0.1f);
        // landing wheels in their fairings
        for (int s = -1; s <= 1; s += 2) for (int k = -1; k <= 1; k += 2) {
            b.box(-6.0f + s * 12.0f, k * 6.4f, 3.35f, 2.4f, 1.2f, 1.0f, hullD).bev(0.3f);
            b.cyl('y', -6.0f + s * 12.0f, k * 6.6f, 2.55f, 1.4f, 0.7f, pal::rubber).m(RUBBER).bev(0.2f);
        }
    } else {
        // the tailplane and fin of the workhorse, with a tail rotor, and its belly wheels
        b.box(-26.0f, 0, 8.8f, 1.6f, 0.55f, 3.0f, hullD).bev(0.3f);
        b.box(-26.0f, 0, 11.2f, 1.3f, 0.6f, 1.4f, OWNER).bev(0.15f);
        b.box(-28.4f, 0, 7.4f, 3.2f, 6.4f, 0.5f, hull).bev(0.25f);
        for (int s = -1; s <= 1; s += 2) b.box(-28.4f, s * 6.1f, 7.4f, 2.8f, 0.55f, 0.45f, OWNER).bev(0.1f);
        b.cyl('y', -27.0f, 1.6f, 8.0f, 0.75f, 0.8f, kSteel).m(METAL).bev(0.2f);
        b.rod({ -30.6f, 1.6f, 8.0f }, { -23.4f, 1.6f, 8.0f }, 0.42f, kDark).bev(0);
        b.rod({ -27.0f, 1.6f, 4.4f }, { -27.0f, 1.6f, 11.6f }, 0.42f, kDark).bev(0);
        for (int s = -1; s <= 1; s += 2) for (int k = -1; k <= 1; k += 2) {
            b.rod({ -6.0f + s * 9.0f, k * 4.6f, 3.9f }, { -6.0f + s * 9.0f, k * 6.0f, 2.2f }, 0.4f, kSteel).m(METAL).bev(0);
            b.cyl('y', -6.0f + s * 9.0f, k * 6.2f, 2.5f, 1.35f, 0.75f, pal::rubber).m(RUBBER).bev(0.2f);
            b.cyl('y', -6.0f + s * 9.0f, k * 6.2f, 2.5f, 0.6f, 0.8f, kSteel).m(METAL).bev(0.15f);
        }
        // exposed exhaust stacks and the hoist cradle under the belly
        for (int s = -1; s <= 1; s += 2) {
            b.cyl('x', -16.0f, s * 3.2f, 10.4f, 1.2f, 1.8f, kRust, 0.9f).m(METAL).bev(0.2f);
            b.ball(-17.9f, s * 3.2f, 10.4f, 0.35f, 0.8f, 0.8f, kGlow).m(EMIT);
        }
        b.box(0, 0, 2.0f, 6.0f, 3.4f, 0.7f, kSteel).m(METAL).bev(0.3f);
    }
}
static void cCargo(Model& b, Model& t, int) { cargoLifter(b, t, 1); }
static void kCargo(Model& b, Model& t, int) { cargoLifter(b, t, 0); }
M3D_UNIT(U_C_CARGO, cCargo)
M3D_UNIT(U_K_CARGO, kCargo)

// ---------------------------------------------------------------- the small stealth spy drones: a pod on four stub arms with ducted fan pods
static void spyDrone(Model& b, Model&, int cyber) {
    using namespace pal;
    const Color body = cyber ? C(0x1C1E26) : C(0x404436), arm = cyber ? C(0x2C2E38) : C(0x565A46), duct = cyber ? C(0x0A0B10) : C(0x282A20);
    const Color lens = cyber ? cGlow : C(0xFFB03C);
    b.ground = false; b.dirt = 0.0f;
    // the pod, its top deck and the sensor dome with its lens
    b.ball(0, 0, 5.0f, 8.0f, 6.0f, 2.8f, body).t(T_PANEL, 5).bev(0.4f);
    b.taper(0, 0, 6.0f, 5.2f, 4.4f, 2.0f, 3.6f, 3.0f, cyber ? cHull2 : arm).t(T_PANEL, 4).bev(0.4f);
    b.ball(6.5f, 0, 6.4f, 2.4f, 2.4f, 1.6f, duct);
    b.ball(6.9f, 0, 6.6f, 0.9f, 1.1f, 0.8f, lens).m(EMIT);
    // the owner's band across the deck, and a small hatch
    b.box(-1.4f, 0, 7.85f, 1.6f, 2.6f, 0.25f, OWNER).bev(0.1f);
    b.cyl('z', -3.4f, 0, 8.0f, 1.4f, 0.25f, duct).bev(0.15f);
    // the tail boom, its fin with the owner's colour, and the antenna
    b.rod({ -6.5f, 0, 5.6f }, { -17.5f, 0, 6.0f }, 1.15f, arm).m(PAINT).bev(0.2f);
    b.box(-18.6f, 0, 7.1f, 2.6f, 0.45f, 1.5f, arm).bev(0.25f);
    b.box(-18.6f, 0, 8.3f, 2.2f, 0.5f, 0.45f, OWNER).bev(0.1f);
    b.rod({ -8.0f, 0, 6.6f }, { -15.5f, 0, 10.6f }, 0.15f, lens).bev(0).f(F_NOSHADOW);
    // four stub arms, each to a ducted fan pod
    for (int sx = -1; sx <= 1; sx += 2) for (int sy = -1; sy <= 1; sy += 2) {
        const float ex = sx * 13.5f, ey = sy * 13.5f;
        b.rod({ sx * 5.0f, sy * 4.0f, 4.6f }, { ex, ey, 4.6f }, 1.15f, arm).m(PAINT).bev(0.2f);
        b.cyl('z', ex, ey, 4.6f, 3.5f, 0.7f, arm).m(PAINT).bev(0.4f);                          // the duct ring
        b.cyl('z', ex, ey, 5.0f, 2.4f, 0.45f, duct).bev(0.2f);                                  // its dark face
        b.cyl('z', ex, ey, 5.35f, 0.8f, 0.4f, arm).m(METAL).bev(0.15f);                          // fan hub
    }
    // a slim camera ball under the nose
    b.ball(5.0f, 0, 2.8f, 1.8f, 1.8f, 1.2f, duct);
    b.hub = 8.0f;
}
static void cSDrone(Model& b, Model& t, int) { spyDrone(b, t, 1); }
static void kSDrone(Model& b, Model& t, int) { spyDrone(b, t, 0); }
M3D_UNIT(U_C_SDRONE, cSDrone)
M3D_UNIT(U_K_SDRONE, kSDrone)

// ---------------------------------------------------------------- Relic Thunderhawk: a heavy attack helicopter, weathered bronze and rust
static void rThunderhawk(Model& b, Model&, int) {
    using namespace pal;
    const Color rArm = C(0x968C76), rOch = C(0xB2905E), rDrk = C(0x2C2822), rBrz = C(0xA47842), rVer = C(0x68AC90);
    const Color rAmber = C(0xFFB24C), rStn = C(0xE2D2A4), rGlass = C(0x283C42);
    b.ground = false; b.dirt = 0.0f;
    // the armoured pod: a wide belly, a stepped flank, a seamed back
    b.plan({ { 27, 0 }, { 23, -4.2f }, { 14, -6.6f }, { -2, -6.6f }, { -10.5f, -3.2f }, { -10.5f, 3.2f }, { -2, 6.6f }, { 14, 6.6f }, { 23, 4.2f } }, 2.4f, 8.6f, rArm).t(T_NOISE, 6, rOch).bev(1.0f);
    b.plan({ { 25, 0 }, { 20, -3.4f }, { 12, -5.4f }, { -1, -5.4f }, { -9, -2.6f }, { -9, 2.6f }, { -1, 5.4f }, { 12, 5.4f }, { 20, 3.4f } }, 8.4f, 10.4f, rArm).t(T_NOISE, 7, rOch).bev(0.7f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(6, s * 6.6f, 5.6f, 12.0f, 0.35f, 0.8f, rBrz).m(METAL).bev(0.15f);              // bronze seam down each flank
        // stub wings: a root and a tip, two rocket pods outboard on their pylons, the owner's colour on the tips
        b.plan({ { -5.6f, s * 6.8f }, { 6.0f, s * 6.8f }, { 4.6f, s * 17.6f }, { -4.0f, s * 18.4f } }, 7.6f, 9.2f, rArm).t(T_NOISE, 5, rOch).bev(0.5f);
        b.box(3.6f, s * 17.6f, 8.6f, 2.6f, 1.1f, 0.5f, OWNER).bev(0.15f);
        for (float yy : { s * 12.4f, s * 17.6f }) {
            b.box(0, yy, 6.6f, 1.0f, 0.55f, 1.2f, rDrk).bev(0.2f);                              // pylon
            b.cyl('x', 2.7f, yy, 4.6f, 2.5f, 6.7f, rDrk, 0.9f).m(PAINT).t(T_NOISE, 4, rOch).bev(0.5f);
            b.cyl('x', 9.0f, yy, 4.6f, 2.1f, 0.5f, rDrk).bev(0.2f);                             // tube face
            b.cyl('x', 9.5f, yy, 4.6f, 0.7f, 0.4f, rBrz).m(METAL).bev(0.1f);                     // bronze nose cap
        }
        // engine intakes behind the cockpit, exhausts at the boom root with their dim glow
        b.box(-1.0f, s * 5.6f, 10.2f, 3.4f, 2.0f, 0.6f, rDrk).t(T_GRILLE, 1.1f).bev(0.2f);
        b.cyl('x', -9.6f, s * 3.4f, 8.8f, 1.7f, 1.4f, rBrz, 0.9f).m(METAL).bev(0.2f);
        b.ball(-11.1f, s * 3.4f, 8.8f, 0.35f, 1.0f, 1.0f, rAmber).m(EMIT);
        // the bronze-framed canopy frames, the chin turret's lamps, skids
        b.ball(20.0f, s * 1.5f, 4.4f, 0.6f, 0.6f, 0.6f, rAmber).m(EMIT);
        b.rod({ 2, s * 3.4f, 1.5f }, { 16, s * 3.4f, 1.5f }, 0.5f, rBrz).m(METAL).bev(0);
        b.rod({ 13, s * 3.4f, 1.6f }, { 14.2f, s * 2.4f, 3.2f }, 0.4f, rBrz).m(METAL).bev(0);
        b.rod({ 5, s * 3.4f, 1.6f }, { 6, s * 2.4f, 3.2f }, 0.4f, rBrz).m(METAL).bev(0);
    }
    // the armoured cockpit: two glass canopies in dark bronze frames, a bronze bulkhead between them
    b.taper(17.4f, 0, 8.4f, 3.2f, 4.2f, 2.6f, 2.4f, 3.2f, rDrk, 0.6f, 0).bev(0.4f);
    b.dome(17.4f, 0, 10.0f, 3.0f, 3.9f, 1.5f, rGlass).m(GLASS);
    b.box(13.6f, 0, 10.6f, 0.7f, 4.6f, 1.4f, rBrz).m(METAL).bev(0.2f);                          // the bulkhead
    b.taper(9.6f, 0, 8.6f, 3.0f, 3.6f, 2.2f, 2.2f, 2.6f, rDrk, 0.4f, 0).bev(0.4f);
    b.dome(9.6f, 0, 9.8f, 2.8f, 3.3f, 1.3f, rGlass).m(GLASS);
    // the chin turret under the nose: a bronze-shod gun
    b.cyl('x', 24.0f, 0, 3.4f, 1.6f, 2.4f, rArm, 0.85f).m(PAINT).t(T_NOISE, 4, rOch).bev(0.3f);
    b.rod({ 26.0f, 0, 3.2f }, { 32.4f, 0, 3.0f }, 0.7f, rDrk).m(METAL).bev(0);
    b.cyl('x', 32.6f, 0, 3.0f, 0.95f, 0.6f, rBrz).m(METAL).bev(0.15f);
    b.ball(27.8f, 0, 5.6f, 0.7f, 1.5f, 0.8f, rBrz).m(METAL);                                     // nose sensor
    // the tail: a boom, a tailplane, the tail-rotor housing with its dark fan and bronze hub
    b.cyl('x', -17.5f, 0, 7.2f, 2.5f, 11.0f, rArm, 0.7f).t(T_NOISE, 5, rOch).bev(0.5f);
    b.box(-12.0f, 0, 7.6f, 1.6f, 3.0f, 1.6f, OWNER).bev(0.25f);                                   // the owner's roundel band
    b.box(-24.0f, 0, 7.95f, 1.6f, 4.4f, 0.25f, OWNER).bev(0.12f);                                 // and the owner's panel on the tailplane
    b.box(-18.5f, 0, 7.4f, 6.5f, 6.6f, 0.5f, rOch).t(T_NOISE, 5, rArm).bev(0.25f);                // tailplane
    for (int s = -1; s <= 1; s += 2) b.box(-18.5f, s * 6.2f, 7.4f, 5.6f, 0.5f, 0.45f, rStn).bev(0.1f);
    b.cyl('z', -28.0f, 0, 8.2f, 4.4f, 0.9f, rArm, 0.94f).m(PAINT).t(T_NOISE, 4, rOch).bev(0.5f);   // fan housing
    b.cyl('z', -28.0f, 0, 9.05f, 4.0f, 0.22f, rBrz, 0.9f).m(METAL).bev(0.15f);                     // bronze rim round it
    b.cyl('z', -28.0f, 0, 9.05f, 3.3f, 0.3f, rDrk).bev(0.2f);                                      // the dark fan face
    b.cyl('z', -28.0f, 0, 9.35f, 0.9f, 0.3f, rBrz).m(METAL).bev(0.15f);                            // and its hub
    // the rotor head: a bronze collar over a mast, the blades turning above it
    b.cyl('z', -1.5f, 0, 11.4f, 1.2f, 1.2f, rBrz).m(METAL).bev(0.2f);
    b.cyl('z', -1.5f, 0, 12.8f, 3.6f, 0.9f, rArm).m(PAINT).t(T_NOISE, 4, rOch).bev(0.5f);
    b.cyl('z', -1.5f, 0, 13.9f, 3.1f, 0.3f, rBrz).m(METAL).bev(0.2f);                                 // bronze ring
    b.dome(-1.5f, 0, 14.2f, 2.0f, 2.0f, 0.9f, rDrk).bev(0);
    b.hub = 15.1f;
    // verdigris where the bronze has stood out in the rain: patches on the boom and the fin, a stain over the engine deck
    b.box(-22.0f, 0, 8.55f, 2.6f, 1.6f, 0.3f, rVer).m(MATTE).bev(0.3f);
    b.box(-4.0f, 0, 11.15f, 3.0f, 1.8f, 0.2f, rVer).m(MATTE).bev(0.3f);
}
M3D_UNIT(U_R_THUNDERHAWK, rThunderhawk)
