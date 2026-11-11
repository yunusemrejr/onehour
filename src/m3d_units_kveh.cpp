// One Hour - 3D models: the Clanker ground vehicles and the Relic war machines (see m3d.h / m3d_reg.h).
// Clanker: olive and sand paint, hazard yellow, rust, gunmetal, amber lamps. Relic: weathered plate, sun-faded ochre, bronze gone green,
// dim amber lamps. Civilian: white and safety orange. The old 2D painters in art.cpp / art_jeep.cpp / art_relic.cpp set each plan.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// ---------------------------------------------------------------- shared tones
const Color kSand = C(0xAA966A), kOlive = C(0x5C6440), kOliveD = C(0x2C3020);     // the sand / olive units (gatling tank, launcher)
const Color kCast = C(0x4A4C48), kCastD = C(0x1E1E1C), kSteel2 = C(0x8C8E8C);     // Behemoth: cast iron and bare steel
const Color kWood = C(0x9A7646), kTarp = C(0x686C56), kDrum = C(0x465C6E), kLamp = C(0xFFF0BE), kTail = C(0xD6362C);
const Color civWhite = C(0xE2DED2), civOrange = C(0xEC8028), civSteel = C(0x969896), civDark = C(0x34322E);
const Color rlArm = C(0x968C76), rlOch = C(0xB2905E), rlDrk = C(0x2C2822), rlBrz = C(0xA47842);   // Relic
const Color rlVer = C(0x68AC90), rlAmb = C(0xFFB24C), rlGlass = C(0x283C42), rlStn = C(0xD8C8A0);

// One road wheel: a rubber tyre with a steel hub standing proud of its outer face.
void wheel(Model& m, float x, float y, float r, float hw, Color hub) {
    m.cyl('y', x, y, r, r, hw, pal::rubber).m(RUBBER).t(T_BAND, 1.7f, C(0x3C3B32)).bev(0.5f);
    const float s = y < 0 ? -1.0f : 1.0f;
    m.cyl('y', x, y + s * (hw + 0.1f), r, r * 0.44f, 0.45f, hub).m(METAL).bev(0.2f);
}

// a low seat: a cushion on the floor with a short backrest behind it (jeeps, open vehicles)
void seat(Model& m, float x, float y, Color cush, Color back) {
    m.box(x, y, 5.6f, 1.9f, 1.7f, 0.45f, cush).m(MATTE).bev(0.3f);
    m.box(x - 2.0f, y, 6.6f, 0.45f, 1.8f, 1.2f, back).m(MATTE).bev(0.2f);
}

// ---------------------------------------------------------------- dozer and civil dozer
// A crawler dozer: two track pods, a yellow chassis, a rear engine block, an enclosed cab behind a short hood, a rear ripper and the big
// bowed moldboard on two push arms. The civilian machine is the same body in white and safety orange, with a roof light bar.
static void dozerRig(Model& b, bool civil) {
    using namespace pal;
    const Color chassis = civil ? civOrange : kHaz, cabCol = civil ? civWhite : kHull;
    const Color steel = civil ? civSteel : C(0x7C7E7C), dark = civil ? civDark : kDark;
    b.dirt = 0.5f;
    tracks(b, 36, 6.0f, 11.0f, 7.8f, chassis, true);
    b.box(-3.5f, 0, 6.4f, 13.5f, 9.4f, 3.4f, chassis).t(T_PANEL, 5).bev(0.8f);                  // chassis between the tracks
    b.box(-11.5f, 0, 10.3f, 5.4f, 7.6f, 1.3f, chassis).bev(0.6f);                                // rear engine block
    b.box(-11.5f, 0, 11.8f, 4.2f, 6.2f, 0.35f, dark).t(T_GRILLE, 1.4f).bev(0.1f);                // its louvres
    b.box(-15.8f, 0, 11.9f, 0.9f, 3.4f, 0.3f, OWNER).bev(0.1f);                                  // the owner's panel on the tail
    b.cyl('z', -6.2f, -6.8f, 12.6f, 1.3f, 2.4f, dark).m(METAL).bev(0.3f);                        // exhaust stack
    // operator's cab: an olive steel box with a big windscreen and a roof hatch
    b.taper(1.0f, 0, 9.4f, 6.2f, 7.2f, 6.4f, 5.2f, 6.0f, cabCol).t(T_PANEL, 4).bev(0.7f);
    b.box(6.9f, 0, 11.9f, 0.4f, 5.6f, 2.2f, kGlass).m(GLASS).bev(0.2f);                          // windscreen
    for (int s = -1; s <= 1; s += 2) {
        b.box(2.6f, s * 6.9f, 12.8f, 3.0f, 0.35f, 1.7f, kGlass).m(GLASS).bev(0.1f);               // side glass
        b.box(6.4f, s * 5.4f, 15.0f, 0.7f, 0.8f, 0.5f, kGlow).m(EMIT);                            // amber corner lamps
    }
    b.cyl('z', 1.6f, -3.0f, 15.9f, 2.1f, 0.45f, chassis).bev(0.3f);                              // roof hatch
    // hood: a short deck forward of the cab, radiator grille on the nose, a lamp each side
    b.box(11.2f, 0, 8.9f, 4.4f, 3.9f, 1.6f, chassis).t(T_PANEL, 4).bev(0.6f);
    b.box(15.4f, 0, 8.8f, 0.3f, 3.2f, 1.2f, dark).t(T_GRILLE, 1.1f).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) b.box(15.2f, s * 4.5f, 8.4f, 0.5f, 0.7f, 0.5f, kLamp).m(EMIT);
    if (civil) {   // the light bar across the cab roof: amber and white
        b.box(1.0f, 0, 16.0f, 1.5f, 6.2f, 0.5f, dark).bev(0.2f);
        for (int i = 0; i < 4; i++) b.box(1.0f, -4.5f + i * 3.0f, 16.7f, 1.0f, 1.0f, 0.35f, i < 2 ? C(0xFFAA28) : C(0xFFFFFF)).m(EMIT);
    }
    // the moldboard: a bowed plate across the front, a dark cutting edge under it, the owner's band along its top
    std::vector<P2> bow;
    for (int i = 0; i <= 4; i++) { const float a = -0.62f + i * 0.31f; bow.push_back({ -1.0f + 24.4f * std::cos(a), 24.4f * std::sin(a) }); }
    bow.push_back({ -1.0f + 19.4f * std::cos(0.62f), 19.4f * std::sin(0.62f) });
    bow.push_back({ -1.0f + 19.4f * std::cos(-0.62f), -19.4f * std::sin(0.62f) });
    b.plan(bow, 0.6f, 9.2f, steel).m(METAL).bev(0.7f);
    std::vector<P2> edge;
    for (int i = 0; i <= 4; i++) { const float a = -0.62f + i * 0.31f; edge.push_back({ -1.0f + 25.0f * std::cos(a), 25.0f * std::sin(a) }); }
    edge.push_back({ -1.0f + 22.8f * std::cos(0.62f), 22.8f * std::sin(0.62f) });
    edge.push_back({ -1.0f + 22.8f * std::cos(-0.62f), -22.8f * std::sin(0.62f) });
    b.plan(edge, 0.0f, 1.5f, dark).bev(0.3f);                                                    // cutting edge
    b.box(22.6f, 0, 9.4f, 1.1f, 7.4f, 0.35f, OWNER).bev(0.1f);                                   // the owner's band on the blade
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ 2.0f, s * 9.0f, 4.2f }, { 16.0f, s * 6.6f, 3.6f }, 1.2f, dark).m(METAL).bev(0.2f);   // push arms
        b.rod({ 6.4f, s * 4.6f, 6.6f }, { 17.4f, s * 3.2f, 5.4f }, 0.6f, steel).m(METAL).bev(0);      // lift rams
    }
    // the ripper behind the tail: a crossbeam, two links and one steel tooth
    b.rod({ -17.6f, -10.4f, 4.4f }, { -17.6f, 10.4f, 4.4f }, 1.0f, steel).m(METAL).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) b.rod({ -17.6f, s * 5.0f, 4.4f }, { -20.4f, s * 2.2f, 3.0f }, 0.7f, steel).m(METAL).bev(0);
    b.taper(-20.6f, 0, 0.2f, 1.7f, 1.6f, 4.0f, 1.0f, 1.0f, steel, -0.8f, 0).m(METAL).bev(0.3f);
    b.box(-3.5f, 0, 9.9f, 4.0f, 4.6f, 0.3f, OWNER).bev(0.1f);                                    // the owner's panel on the deck
}
static void kDozer(Model& b, Model&, int) { dozerRig(b, false); }
static void kCivDozer(Model& b, Model&, int) { dozerRig(b, true); }
M3D_UNIT(U_K_DOZER, kDozer)
M3D_UNIT(U_K_CIVDOZER, kCivDozer)

// ---------------------------------------------------------------- supply truck
// A six-wheel flatbed hauler: crates, a lashed tarp and drums on a planked deck, and a rust-brown cab with a glass band.
static void kHarv(Model& b, Model&, int) {
    using namespace pal;
    b.dirt = 0.45f;
    for (float x : { -13.0f, 1.5f, 16.5f }) {
        for (int s = -1; s <= 1; s += 2) wheel(b, x, s * 14.0f, 3.5f, 2.3f, kSteel);
        for (int s = -1; s <= 1; s += 2) b.box(x, s * 12.2f, 5.3f, 3.9f, 1.6f, 0.6f, kHull2).bev(0.3f);   // mudguards
    }
    b.box(-5.0f, 0, 4.3f, 20.0f, 9.6f, 1.6f, kDark).bev(0.4f);                                   // chassis rails under the deck
    b.box(-7.5f, 0, 5.4f, 17.4f, 10.2f, 0.6f, kWood).t(T_PLANKS, 5.0f).bev(0.3f);                 // flatbed in earth brown
    for (int s = -1; s <= 1; s += 2) {
        b.box(-7.5f, s * 10.4f, 7.2f, 17.4f, 0.4f, 1.8f, kHull).t(T_PANEL, 6).bev(0.3f);          // sideboards
        b.box(-25.6f, s * 7.0f, 5.6f, 0.4f, 2.6f, 0.9f, kTail).m(EMIT);                           // rear lamps
        b.box(-7.5f, s * 10.4f, 9.2f, 15.0f, 0.42f, 0.28f, OWNER).bev(0.1f);                        // the owner's stripe along each sideboard
    }
    for (int j = 0; j < 2; j++) for (int i = 0; i < 3; i++) {
        const float x = -16.4f + i * 5.4f, y = -5.9f + j * 11.8f;
        b.box(x, y, 7.1f, 2.5f, 1.9f, 1.1f, kWood).t(T_PLANKS, 2.4f).bev(0.3f);                   // wooden crates
        b.box(x, y, 8.3f, 2.6f, 2.0f, 0.25f, C(0x7C5D38)).t(T_PLANKS, 2.4f).bev(0.1f);            // their lids
    }
    b.taper(1.0f, 0, 6.0f, 3.2f, 9.6f, 3.2f, 2.2f, 9.0f, kTarp).m(MATTE).bev(0.9f);                // tarp lashed over the middle load
    for (int i = 0; i < 2; i++) b.box(1.0f, -9.8f + i * 19.6f, 7.6f, 0.35f, 0.3f, 0.9f, kDark).f(F_NOSHADOW);
    for (int i = 0; i < 3; i++) b.cyl('z', 5.6f, -7.0f + i * 7.0f, 7.4f, 1.6f, 1.6f, kDrum).t(T_BAND, 1.2f, kDark).bev(0.3f);   // oil drums
    // cab: rust-brown shell, dark glass band, roof hatch, mirrors, headlamps
    b.taper(18.4f, 0, 4.4f, 6.6f, 9.2f, 8.4f, 5.6f, 8.4f, kRust, 0.4f, 0).t(T_CAMO, 6, C(0x6E3A22)).bev(0.7f);
    b.box(19.6f, 0, 10.9f, 4.8f, 8.0f, 0.6f, kHull2).t(T_PANEL, 4).bev(0.4f);                      // cab roof
    b.box(24.6f, 0, 8.6f, 0.35f, 7.6f, 2.2f, C(0x48606C)).m(GLASS).bev(0.2f);                      // windscreen
    b.cyl('z', 18.0f, 0, 11.8f, 2.0f, 0.4f, kHull).bev(0.3f);                                      // roof hatch
    for (int s = -1; s <= 1; s += 2) {
        b.box(24.4f, s * 4.4f, 6.6f, 0.3f, 1.2f, 0.7f, kLamp).m(EMIT);                             // headlamps
        b.rod({ 21.0f, s * 8.8f, 9.6f }, { 22.6f, s * 11.8f, 9.6f }, 0.3f, kDark).f(F_NOSHADOW);    // mirror arms
        b.box(22.8f, s * 12.2f, 9.4f, 0.3f, 0.9f, 1.3f, kDark).bev(0.2f);                          // mirrors
    }
    b.box(-25.0f, 0, 7.2f, 0.9f, 7.4f, 1.0f, OWNER).bev(0.2f);                                     // owner's board on the tail
    b.box(10.2f, 0, 6.2f, 2.2f, 9.6f, 0.3f, OWNER).bev(0.1f);                                      // owner's panel at the front of the deck
    b.box(19.6f, 0, 11.65f, 4.4f, 7.6f, 0.22f, OWNER).bev(0.1f);                                   // owner's panel on the cab roof
}
M3D_UNIT(U_K_HARV, kHarv)
// ---------------------------------------------------------------- gatling tank (ZSU-23-4 family)
// Olive tracked chassis with side skirts and a slatted engine deck; a boxy turret with a radar dish on the rear roof and a six-barrel
// rotary cannon in a shroud.
static void kGatling(Model& b, Model& t, int) {
    using namespace pal;
    const Color skirt = C(0x4A5034);
    b.dirt = 0.5f;
    tracks(b, 40, 10.1f, 9.8f, 7.2f, skirt, true);
    b.profileY({ { -20, 3.0f }, { -20.6f, 8.2f }, { 6, 9.0f }, { 17, 6.6f }, { 19.2f, 4.6f }, { 17, 2.4f }, { -18, 2.4f } },
               -10.5f, 10.5f, kOlive).t(T_CAMO, 7, kOliveD).bev(0.9f);
    b.box(-16, 0, 9.6f, 5.4f, 6.6f, 0.6f, kOliveD).t(T_GRILLE, 1.6f).bev(0.2f);                 // engine deck louvres
    b.box(16.2f, 0, 6.4f, 3.0f, 6.0f, 0.3f, kOliveD).t(T_PANEL, 3).bev(0.1f);                    // glacis plate seam
    for (int s = -1; s <= 1; s += 2) {
        b.box(-6, s * 11.0f, 9.2f, 7.0f, 1.4f, 0.9f, skirt).t(T_PANEL, 4).bev(0.3f);             // stowage on the fenders
        b.box(17.4f, s * 6.0f, 6.0f, 0.6f, 1.0f, 0.6f, kGlow).m(EMIT);                           // headlamps
        b.cyl('x', -21.4f, s * 4.0f, 7.4f, 1.0f, 1.6f, kRust).m(METAL).bev(0.2f);                // exhausts
    }
    b.box(-19.8f, 0, 7.4f, 0.8f, 5.0f, 1.1f, OWNER).bev(0.2f);                                   // the owner's band across the tail
    b.box(-15.6f, 0, 10.2f, 3.6f, 4.6f, 0.3f, OWNER).bev(0.1f);                                  // and a panel on the engine deck
    b.cyl('z', 0, 0, 9.2f, 7.8f, 0.45f, kOliveD).bev(0.3f);                                      // turret ring
    b.cyl('z', -0.5f, 0, 12.4f, 9.0f, 3.0f, kOlive).f(F_SHADOWONLY);                            // (the turret's shade on the deck)
    t.ground = false; t.dirt = 0.1f;
    t.taper(-1.2f, 0, 9.2f, 9.4f, 7.6f, 5.4f, 8.6f, 6.8f, kOlive, -0.6f, 0).t(T_CAMO, 6, kOliveD).bev(1.0f);
    t.box(-1.4f, 0, 14.9f, 6.6f, 5.8f, 0.3f, C(0x74805A)).bev(0.2f);                             // roof plate
    t.box(-4.0f, 0, 15.2f, 5.0f, 4.6f, 0.35f, OWNER).bev(0.1f);                                   // the owner's roof panel
    t.box(-12.6f, 0, 15.0f, 2.2f, 5.0f, 1.1f, kOliveD).bev(0.4f);                                 // rear radar deck
    t.cyl('z', -12.6f, 0, 16.5f, 0.6f, 1.3f, kSteel).m(METAL).bev(0);                            // radar mast
    t.cyl('z', -12.6f, 0, 18.4f, 3.8f, 0.5f, pal::kHull2, 0.62f).bev(0.4f).rx(-16);              // radar dish, tipped back
    t.cyl('z', -11.4f, 0, 18.9f, 0.45f, 0.9f, kSteel).m(METAL).bev(0).rx(-16);                   // feed horn
    t.box(9.6f, 0, 12.2f, 4.6f, 1.4f, 1.4f, kOliveD).bev(0.4f);                                   // gun cradle
    t.cyl('x', 13.2f, 0, 12.8f, 2.8f, 4.6f, C(0x4A4E3C), 0.9f).m(METAL).bev(0.4f);              // rotary cannon shroud
    for (int k = 0; k < 6; k++) {   // six barrels, visible past the shroud
        const float a = k * 1.0472f + 0.5f;
        t.rod({ 8.0f, std::sin(a) * 1.55f, 12.8f + std::cos(a) * 1.55f }, { 25.0f, std::sin(a) * 1.55f, 12.8f + std::cos(a) * 1.55f }, 0.62f, kSteel2).m(METAL).bev(0);
    }
    t.cyl('x', 21.0f, 0, 12.8f, 2.3f, 0.7f, kSteel2).m(METAL).bev(0.3f);                         // muzzle carrier
    t.cyl('x', 24.4f, 0, 12.8f, 2.1f, 0.45f, kOliveD).bev(0.2f);
    for (int s = -1; s <= 1; s += 2) {   // hatches and the gunner's sight
        t.cyl('z', -6.4f, s * 3.4f, 15.3f, 2.0f, 0.4f, kHull).bev(0.3f);
        t.box(-3.0f, s * 7.6f, 15.4f, 0.4f, 0.8f, 0.5f, kGlow).m(EMIT);
    }
    t.box(4.2f, -5.0f, 15.6f, 1.2f, 0.9f, 0.5f, kOliveD).bev(0.2f);                               // sight box
    t.box(-2.6f, 8.8f, 12.4f, 3.4f, 1.2f, 1.0f, kOliveD).bev(0.3f);                               // ammunition box on the flank
    t.rod({ 0.4f, 9.4f, 12.6f }, { 6.6f, 5.6f, 12.6f }, 0.55f, pal::kBrass).m(METAL).bev(0);      // brass feed belt
}
M3D_UNIT(U_K_GATLING, kGatling)

// ---------------------------------------------------------------- rocket launcher (M270 family)
// Olive chassis with an armoured cab forward and a hazard bumper; the turntable carries the sand pod with its 4 x 5 grid of tube mouths.
static void kLauncher(Model& b, Model& t, int) {
    using namespace pal;
    const Color skirt = C(0x545C3A);
    b.dirt = 0.5f;
    tracks(b, 46, 9.8f, 10.4f, 7.6f, skirt, true);
    b.profileY({ { -24, 3.0f }, { -24.6f, 8.4f }, { 15, 8.4f }, { 16.4f, 6.6f }, { 14, 2.6f }, { -22, 2.6f } },
               -10.6f, 10.6f, kOlive).t(T_CAMO, 7, kOliveD).bev(0.8f);
    for (int i = 0; i < 4; i++) b.box(-19.0f + i * 2.6f, 0, 8.6f, 0.6f, 6.0f, 0.3f, kOliveD).f(F_NOSHADOW);   // rear deck louvres
    // the cab, forward of the pod
    b.taper(17.4f, 0, 8.2f, 8.6f, 8.0f, 6.6f, 7.8f, 7.0f, kOlive, 0.2f, 0).t(T_PANEL, 5).bev(0.7f);
    b.box(16.6f, 0, 15.2f, 6.4f, 7.2f, 0.5f, kSand).t(T_PANEL, 4).bev(0.3f);                      // sand cab roof
    b.box(16.6f, 0, 15.65f, 5.4f, 6.2f, 0.22f, OWNER).bev(0.1f);                                  // the owner's panel on it
    b.box(25.6f, 0, 11.6f, 0.35f, 6.6f, 2.0f, C(0x2C3E44)).m(GLASS).bev(0.2f);                    // windscreen
    b.box(26.2f, 0, 7.4f, 0.7f, 9.4f, 0.8f, kHaz).t(T_HAZARD, 2.0f, kDark).bev(0.2f);             // hazard-yellow bumper
    b.cyl('z', 14.0f, 0, 15.8f, 1.8f, 0.35f, kSand).bev(0.3f);                                     // commander's hatch
    for (int s = -1; s <= 1; s += 2) {
        b.box(26.0f, s * 5.0f, 7.6f, 0.4f, 1.0f, 0.6f, kLamp).m(EMIT);                             // headlamps
        b.rod({ 22.0f, s * 8.4f, 13.4f }, { 23.8f, s * 10.8f, 13.4f }, 0.3f, kDark).f(F_NOSHADOW);
        b.box(24.0f, s * 11.2f, 13.2f, 0.3f, 0.9f, 1.2f, kDark).bev(0.2f);                         // mirrors
    }
    b.box(-24.2f, 0, 8.8f, 0.7f, 6.0f, 0.9f, OWNER).bev(0.2f);                                     // owner's panel on the tail
    b.box(-22.4f, 0, 3.4f, 0.6f, 1.0f, 0.6f, kHaz).m(EMIT);                                        // tow lamp
    b.cyl('z', -3.0f, 0, 8.6f, 9.4f, 0.5f, kOliveD).bev(0.3f);                                     // turntable ring
    b.cyl('z', -3.0f, 0, 11.8f, 11.0f, 3.0f, kSand).f(F_SHADOWONLY);                                // (the pod's shade on the deck)
    t.ground = false; t.dirt = 0.15f;
    t.cyl('z', -3.0f, 0, 8.9f, 8.6f, 0.8f, kOliveD).bev(0.4f);                                     // the turntable itself
    t.box(-3.0f, 0, 11.6f, 12.9f, 8.2f, 1.8f, C(0x4E5A38)).bev(0.5f);                             // dark pod shell
    t.box(-3.0f, 0, 13.4f, 12.2f, 7.6f, 1.7f, kSand).t(T_NOISE, 5.0f, kOliveD).bev(0.5f);         // sand pod frame
    t.box(-3.0f, 0, 15.1f, 11.6f, 7.0f, 0.25f, kSand).bev(0.2f);                                   // its lid
    t.box(-3.0f, 0, 15.3f, 22.6f, 7.4f, 0.12f, C(0x3A4030)).bev(0.1f);                              // the pod's dark mouth plate
    for (int r = 0; r < 4; r++) for (int c = 0; c < 5; c++)   // the twenty tube mouths, open to the sky
        t.cyl('z', -3.4f + (c - 2) * 4.8f, -8.0f + r * 3.8f, 15.42f, 1.75f, 0.16f, C(0x8A8C86)).m(METAL).bev(0.15f);
    for (int s = -1; s <= 1; s += 2) t.box(-3.0f, s * 7.9f, 13.6f, 11.4f, 0.4f, 0.9f, kHaz).t(T_HAZARD, 2.2f, kDark).bev(0.1f);
    t.box(-15.1f, 0, 13.6f, 1.0f, 5.4f, 2.0f, OWNER).bev(0.3f);                                    // the owner's panel on the pod's tail
    t.box(-13.4f, 0, 15.35f, 1.1f, 6.6f, 0.25f, OWNER).bev(0.1f);                                  // and the rear strip of its lid
    for (int s = -1; s <= 1; s += 2) t.box(-3.0f, s * 8.2f, 11.9f, 11.6f, 0.25f, 0.6f, OWNER).bev(0.1f);   // sealing strips along the pod sides
    t.cyl('z', -13.6f, 0, 16.2f, 2.2f, 0.4f, kOliveD).bev(0.3f);                                   // rear sight hatch
}
M3D_UNIT(U_K_MLRS, kLauncher)

// ---------------------------------------------------------------- Behemoth super-heavy tank
// A cast-iron slab hull on two very wide track pods, hazard trim along the tail, and a turret with twin 150 mm barrels.
static void kTitan(Model& b, Model& t, int) {
    using namespace pal;
    b.dirt = 0.55f;
    tracks(b, 50, 12.1f, 12.8f, 8.6f, C(0x5A5C46), true);
    b.profileY({ { -26, 3.2f }, { -26.6f, 9.4f }, { 10, 10.6f }, { 21, 7.0f }, { 24, 4.6f }, { 22, 2.6f }, { -24, 2.6f } },
               -13.0f, 13.0f, kCast).t(T_NOISE, 9.0f, C(0x3C3E3A)).bev(1.2f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-2, s * 12.6f, 10.2f, 12.4f, 1.6f, 0.9f, kCastD).t(T_PANEL, 5).bev(0.4f);          // side armour plates
        b.box(-2, s * 12.6f, 11.15f, 11.6f, 1.35f, 0.25f, OWNER).bev(0.1f);                        // the owner's stripe along each of them
        b.cyl('z', -24.2f, s * 6.0f, 11.4f, 2.0f, 2.4f, kSteel2, 0.9f).m(METAL).bev(0.3f);        // exhaust stacks
        b.box(20.6f, s * 6.4f, 6.6f, 0.7f, 1.1f, 0.7f, kLamp).m(EMIT);                             // headlamps
    }
    b.box(-15.0f, 0, 11.0f, 9.6f, 6.0f, 0.4f, kCastD).t(T_GRILLE, 1.9f).bev(0.2f);                // engine deck grille
    for (int i = 0; i < 5; i++) b.box(-28.2f, -8.0f + i * 4.0f, 6.0f, 0.5f, 1.6f, 1.6f, i % 2 ? kHaz : kCastD).bev(0.2f);
    b.box(23.4f, 0, 6.6f, 0.6f, 8.0f, 0.8f, OWNER).bev(0.2f);                                     // owner's band on the nose
    b.cyl('z', -1, 0, 10.8f, 10.2f, 0.5f, kCastD).bev(0.3f);                                       // turret ring
    b.cyl('z', -1, 0, 13.8f, 11.4f, 3.2f, kCast).f(F_SHADOWONLY);                                 // (the turret's shade on the deck)
    t.ground = false; t.dirt = 0.12f;
    t.taper(-1.0f, 0, 10.8f, 11.2f, 10.6f, 6.2f, 9.8f, 8.8f, kCast, -0.8f, 0).t(T_NOISE, 7.0f, C(0x3C3E3A)).bev(1.3f);
    t.box(-2.0f, 0, 17.15f, 7.4f, 5.8f, 0.3f, OWNER).bev(0.2f);                                    // the owner's roof panel
    t.box(12.0f, 0, 13.4f, 3.4f, 6.4f, 2.6f, kCastD).m(METAL).bev(0.7f);                           // mantlet
    t.box(-10.6f, 0, 13.6f, 1.4f, 7.6f, 2.2f, C(0x8B7A5C)).t(T_PLANKS, 1.6f).bev(0.4f);           // bustle stowage
    for (int s = -1; s <= 1; s += 2) {
        t.rod({ 14.0f, s * 3.9f, 13.6f }, { 31.0f, s * 3.9f, 13.6f }, 1.5f, kSteel2).m(METAL).bev(0.2f);   // 150 mm barrels
        t.cyl('x', 21.0f, s * 3.9f, 13.6f, 2.4f, 2.6f, kCastD).m(METAL).bev(0.3f);                        // thermal sleeves
        t.cyl('x', 31.4f, s * 3.9f, 13.6f, 2.2f, 1.2f, kCastD).bev(0.3f);                                 // muzzle brakes
        t.cyl('z', -6.0f, s * 5.2f, 17.4f, 2.6f, 0.5f, kCast).bev(0.4f);                                  // commander's and loader's hatches
        t.box(2.0f, s * 9.2f, 15.0f, 1.6f, 0.5f, 1.6f, kCastD).bev(0.3f);                                 // reactive blocks on the flanks
    }
    t.rod({ -3.0f, -5.2f, 17.8f }, { 3.0f, -5.2f, 18.4f }, 0.5f, kCastD).f(F_NOSHADOW);            // pintle machine gun
    t.box(3.0f, 4.4f, 17.4f, 1.4f, 1.0f, 0.6f, kCastD).bev(0.2f);                                 // gunner's sight
    t.rod({ -8.0f, 6.6f, 17.0f }, { -11.0f, 7.4f, 27.0f }, 0.25f, kCastD).f(F_NOSHADOW);            // aerial
}
M3D_UNIT(U_K_TITAN, kTitan)
// ---------------------------------------------------------------- the open-top jeep
// The crew tub stands open with seven empty seats (the game rides the soldiers in it): a bonnet with a team band, a leaning windscreen, a
// roll bar, a rolled canvas top, jerry cans and a spare wheel on the tail. Tyres stand proud of the hull on four corner axles.
static void kJeep(Model& b, Model&, int) {
    using namespace pal;
    const Color shell = C(0x626A40), shellD = C(0x383A28), deck = C(0x7A8054);
    const Color cush = C(0x4E4C38), back = C(0x343222), steel = C(0x969896);
    b.dirt = 0.45f;
    const std::vector<P2> plan = {
        { 25.0f, -6.0f }, { 23.0f, -7.3f }, { 19.0f, -8.4f }, { 13.0f, -9.4f }, { 5.0f, -9.9f }, { -6.0f, -10.0f }, { -14.0f, -9.8f }, { -19.0f, -9.2f }, { -22.4f, -8.4f },
        { -22.4f, 8.4f }, { -19.0f, 9.2f }, { -14.0f, 9.8f }, { -6.0f, 10.0f }, { 5.0f, 9.9f }, { 13.0f, 9.4f }, { 19.0f, 8.4f }, { 23.0f, 7.3f }, { 25.0f, 6.0f }
    };
    for (float ax : { -15.0f, 14.0f }) {
        for (int s = -1; s <= 1; s += 2) wheel(b, ax, s * 10.5f, 3.3f, 2.5f, steel);
        for (int s = -1; s <= 1; s += 2) {
            b.box(ax, s * 8.5f, 7.2f, 6.0f, 2.3f, 0.9f, shell).bev(0.4f);                               // fender pads over the tyres
            b.box(ax, s * 8.5f, 7.7f, 5.0f, 1.9f, 0.25f, OWNER).bev(0.1f);                              // the owner's panel on each fender
        }
    }
    b.plan(plan, 2.2f, 5.0f, shell).t(T_CAMO, 6, shellD).bev(0.7f);                                 // lower body: floor level of the tub
    b.box(19.0f, 0, 6.2f, 6.0f, 7.0f, 1.4f, shell).t(T_CAMO, 6, shellD).bev(0.6f);                  // engine bay under the bonnet: solid
    b.box(-3.6f, 0, 4.95f, 13.4f, 7.6f, 0.25f, C(0x6A6E4C)).t(T_RIBS, 1.6f).bev(0.1f);              // ribbed tub floor
    for (int s = -1; s <= 1; s += 2) {
        b.box(-3.6f, s * 9.15f, 5.9f, 13.6f, 0.95f, 1.8f, shell).t(T_PANEL, 5).bev(0.4f);           // raised sills
        b.box(-3.6f, s * 9.15f, 6.9f, 13.2f, 1.15f, 0.35f, OWNER).bev(0.1f);                         // the owner's stripe along each sill top
        b.box(-22.3f, s * 9.15f, 5.9f, 0.9f, 0.95f, 1.8f, shell).bev(0.3f);                          // rear panel corners
    }
    b.box(-22.6f, 0, 5.9f, 0.8f, 10.0f, 1.8f, shell).t(T_PANEL, 4).bev(0.3f);                       // the tub's rear panel
    b.box(18.6f, 0, 7.6f, 6.2f, 7.2f, 0.9f, deck).bev(0.5f);                                        // bonnet deck
    b.box(18.4f, 0, 8.25f, 2.6f, 6.6f, 0.35f, OWNER).bev(0.1f);                                     // the owner's band across the bonnet
    b.box(24.4f, 0, 7.2f, 1.0f, 5.4f, 0.9f, shellD).t(T_GRILLE, 1.2f).bev(0.2f);                    // grille face
    b.rod({ 25.6f, -8.4f, 5.4f }, { 25.6f, 8.4f, 5.4f }, 0.5f, steel).m(METAL).bev(0);               // brush guard
    for (int s = -1; s <= 1; s += 2) {
        b.rod({ 25.6f, s * 8.4f, 5.4f }, { 24.0f, s * 8.4f, 4.0f }, 0.45f, steel).m(METAL).bev(0);
        b.box(18.0f, s * 8.6f, 7.0f, 0.7f, 1.0f, 0.7f, kLamp).m(EMIT);                              // round lamps in the wings
    }
    // windscreen: a steel frame leaning back, glass in it, standing on the dash
    b.box(9.8f, 0, 6.4f, 0.7f, 7.6f, 0.5f, shellD).bev(0.2f);                                       // dash
    b.box(10.4f, 0, 8.6f, 0.9f, 7.6f, 2.4f, steel).m(METAL).bev(0.3f).ry(-13);
    b.box(10.4f, 0, 8.7f, 0.35f, 7.4f, 2.4f, C(0x8AA2B4)).m(GLASS).bev(0.1f).ry(-13);
    // rear: a roll bar, the rolled canvas top and jerry cans
    b.rod({ -17.1f, -8.2f, 6.4f }, { -17.1f, -8.2f, 10.2f }, 0.5f, steel).m(METAL).bev(0);
    b.rod({ -17.1f, -8.2f, 10.2f }, { -17.1f, 8.2f, 10.2f }, 0.5f, steel).m(METAL).bev(0);
    b.rod({ -17.1f, 8.2f, 10.2f }, { -17.1f, 8.2f, 6.4f }, 0.5f, steel).m(METAL).bev(0);
    b.cyl('y', -19.6f, 0, 7.4f, 1.4f, 8.0f, C(0x686654)).m(MATTE).bev(0.4f);                       // folded canvas top
    for (int s = -1; s <= 1; s += 2) {
        b.box(-21.2f, s * 6.6f, 6.8f, 1.0f, 1.2f, 1.4f, C(0x545E3E)).bev(0.3f);                    // jerry cans
        b.box(-22.9f, s * 8.8f, 5.2f, 0.4f, 1.0f, 0.5f, kTail).m(EMIT);                             // tail lamps
        b.box(-23.2f, s * 3.6f, 6.6f, 0.35f, 1.2f, 1.0f, C(0x545E3E)).bev(0.2f);                    // stowage satchels on the tail
    }
    b.cyl('y', -24.4f, 0, 4.6f, 3.6f, 1.0f, pal::rubber).m(RUBBER).t(T_BAND, 1.6f, C(0x3C3B32)).bev(0.4f);   // spare on the tail
    b.cyl('y', -24.4f, 0, 4.6f, 1.6f, 1.25f, OWNER).bev(0.2f);
    static const float SX[7] = { 8.0f, 8.0f, -3.0f, -3.0f, -3.0f, -14.0f, -14.0f };   // the seats the riders sit in: canvas x - 32
    static const float SY[7] = { -6.0f, 6.0f, -7.5f, 0.0f, 7.5f, -5.0f, 5.0f };
    for (int i = 0; i < 7; i++) seat(b, SX[i], SY[i], cush, back);                                  // seven empty seats
}
M3D_UNIT(U_K_JEEP, kJeep)

// ---------------------------------------------------------------- anti-radiation (decontamination) truck
// A six-wheel lorry: a riveted scrubber tank amidships with a roof sprayer boom and the trefoil mark, bolted radiation skirts over the
// tyres, side sprayer arms and rear nozzles, and a sand-roofed cab behind a grille and a brush guard.
static void kRadTruck(Model& b, Model&, int) {
    using namespace pal;
    const Color hull = C(0x666E4E), hullD = C(0x4A5238), deck = C(0x968E64), steel = C(0x92948E);
    const Color dark = C(0x22221C), glass = C(0x4C6460), deco = C(0xA8EC6E), sand = C(0xB4A274);
    b.dirt = 0.5f;
    for (float wx : { -19.0f, -9.5f, 14.5f }) {
        for (int s = -1; s <= 1; s += 2) wheel(b, wx, s * 10.8f, 3.4f, 2.6f, steel);
        for (int s = -1; s <= 1; s += 2) b.box(wx, s * 9.2f, 3.7f, 3.9f, 1.5f, 0.5f, hullD).bev(0.2f);   // mudguards
    }
    b.box(1.0f, 0, 4.2f, 25.0f, 8.0f, 1.8f, hullD).bev(0.4f);                                        // chassis
    // the scrubber tank: a riveted box with a raised deck, a hazard band at the front and a decon band at the back
    b.taper(-8.0f, 0, 3.4f, 16.0f, 8.8f, 8.4f, 15.4f, 8.2f, hull).t(T_PANEL, 6).bev(0.7f);
    b.box(-8.0f, 0, 12.1f, 6.4f, 6.4f, 0.4f, deck).bev(0.3f);                                        // raised deck
    b.box(-15.0f, 0, 12.55f, 1.1f, 6.0f, 0.25f, OWNER).bev(0.1f);                                    // the owner's panel at its rear
    b.box(0.0f, 0, 8.4f, 0.7f, 9.0f, 0.5f, kHaz).t(T_HAZARD, 2.0f, dark).bev(0.1f);                  // hazard band
    b.box(-16.0f, 0, 8.0f, 0.7f, 9.0f, 0.5f, deco).bev(0.1f);                                         // decon band
    b.box(-23.6f, 0, 6.4f, 0.6f, 7.8f, 0.9f, hullD).bev(0.2f);                                        // bolted rear plate
    for (int i = 0; i < 5; i++) b.box(-24.2f, -5.2f + i * 2.6f, 6.2f, 0.25f, 0.9f, 0.6f, i % 2 ? deco : dark).bev(0.1f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-7.0f, s * 9.0f, 7.2f, 12.0f, 0.5f, 2.2f, C(0x76786E)).bev(0.3f);                     // radiation-shield skirts
        b.box(-7.0f, s * 9.0f, 8.55f, 12.0f, 1.1f, 0.35f, OWNER).bev(0.05f);                          // the owner's panel on top of each
    }
    // the trefoil on the tank roof: a yellow disc under three dark blades
    b.cyl('z', -8.5f, 0, 12.7f, 4.0f, 0.2f, C(0xE8C64A)).bev(0.2f);
    for (int i = 0; i < 3; i++) {
        const float a = -1.5708f + i * 2.0944f;
        b.plan({ { -8.5f, 0 }, { -8.5f + std::cos(a - 0.46f) * 3.4f, std::sin(a - 0.46f) * 3.4f }, { -8.5f + std::cos(a + 0.46f) * 3.4f, std::sin(a + 0.46f) * 3.4f } },
               12.85f, 13.05f, dark).bev(0.1f);
    }
    // sprayer boom across the tank roof, its decon lamps, the rear manifold nozzles
    b.rod({ -12.4f, -7.4f, 13.3f }, { -12.4f, 7.4f, 13.3f }, 0.6f, steel).m(METAL).bev(0);
    for (int i = 0; i < 3; i++) b.ball(-12.4f, -6.0f + i * 6.0f, 13.9f, 0.7f, 0.7f, 0.7f, deco).m(EMIT);
    b.box(-23.0f, 0, 4.8f, 0.9f, 6.6f, 0.5f, steel).m(METAL).bev(0.2f);
    for (int i = 0; i < 4; i++) b.ball(-24.0f, -4.8f + i * 3.2f, 4.8f, 0.6f, 0.6f, 0.6f, deco).m(EMIT);
    for (int s = -1; s <= 1; s += 2) {   // side sprayer arms reaching out behind the wheels
        b.rod({ -18.0f, s * 8.6f, 6.4f }, { -22.4f, s * 12.4f, 4.4f }, 0.6f, steel).m(METAL).bev(0);
        b.ball(-23.0f, s * 13.0f, 4.2f, 0.7f, 0.7f, 0.7f, deco).m(EMIT);
    }
    // the cab: olive shell, sand roof, a dark glass band, a grille and a brush guard
    b.taper(17.8f, 0, 3.2f, 8.8f, 8.4f, 9.0f, 8.0f, 7.6f, C(0x626A4A)).t(T_PANEL, 5).bev(0.7f);
    b.box(16.6f, 0, 12.6f, 6.2f, 7.6f, 0.5f, sand).t(T_PANEL, 4).bev(0.3f);                          // sand cab roof
    b.box(22.9f, 0, 9.2f, 0.5f, 6.6f, 2.4f, glass).m(GLASS).bev(0.2f);                               // windscreen band
    b.box(25.4f, 0, 6.6f, 1.2f, 6.4f, 1.8f, hullD).t(T_GRILLE, 1.1f).bev(0.2f);                      // grille
    b.rod({ 26.7f, -7.4f, 3.4f }, { 26.7f, 7.4f, 3.4f }, 0.55f, steel).m(METAL).bev(0);
    b.box(16.4f, 0, 13.15f, 3.4f, 5.0f, 0.35f, OWNER).bev(0.1f);                                     // the owner's panel on the cab roof
    for (int s = -1; s <= 1; s += 2) {
        b.box(25.6f, s * 5.2f, 4.6f, 0.4f, 1.1f, 0.7f, kLamp).m(EMIT);                               // headlamps
        b.rod({ 19.0f, s * 8.2f, 11.4f }, { 20.6f, s * 10.6f, 11.4f }, 0.3f, dark).f(F_NOSHADOW);     // mirror arms
        b.box(20.8f, s * 11.0f, 11.2f, 0.3f, 0.9f, 1.3f, dark).bev(0.2f);                             // mirrors
    }
    // the big sooty exhaust stack beside the cab, and the decon beacon on the tank's front shoulder
    b.cyl('z', 11.4f, -9.8f, 9.4f, 1.5f, 6.2f, steel, 0.92f).m(METAL).bev(0.3f);
    b.cyl('z', 11.4f, -9.8f, 15.9f, 1.6f, 0.4f, C(0x1A1A16)).bev(0.2f);
    b.cyl('z', 9.8f, 0, 13.4f, 0.8f, 0.8f, deco).m(EMIT).f(F_NOSHADOW);
}
M3D_UNIT(U_K_RADTRUCK, kRadTruck)

// ---------------------------------------------------------------- Relic Juggernaut
// A long, low slab hull on two very wide tracks, fenders reaching out over them in ochre skirt plates, a bronze-rimmed engine grille at the
// tail, periscope hatches on the front deck; the turret is an ochre box with twin long cannons and a slotted muzzle brake on each.
static void rJuggernaut(Model& b, Model& t, int) {
    b.dirt = 0.5f;
    tracks(b, 54, 11.0f, 10.4f, 10.4f, rlOch, false);
    b.profileY({ { -26, 3.4f }, { -27, 9.0f }, { 10, 9.6f }, { 20, 7.2f }, { 25.0f, 5.0f }, { 23, 2.8f }, { -24, 2.8f } },
               -11.0f, 11.0f, rlArm).t(T_PANEL, 8).bev(1.1f);
    b.box(10.0f, 0, 9.7f, 11.0f, 9.0f, 0.4f, shade(rlArm, 1.08f)).bev(0.3f);                       // sloped glacis deck
    for (int s = -1; s <= 1; s += 2) {
        b.box(-2.0f, s * 12.4f, 10.7f, 20.5f, 1.4f, 0.6f, rlOch).t(T_PANEL, 6).bev(0.4f);         // bolted fender plates reaching over the tracks
        b.box(-2.0f, s * 12.4f, 11.35f, 15.0f, 1.1f, 0.3f, OWNER).bev(0.1f);                         // the owner's marking on top of each
        b.box(-2.0f, s * 11.6f, 7.2f, 19.0f, 0.5f, 2.2f, rlOch).t(T_PANEL, 6).bev(0.3f);            // skirt plate along the hull side
        b.cyl('z', -24.4f, s * 7.8f, 11.4f, 2.3f, 2.6f, rlBrz, 0.88f).m(METAL).bev(0.3f);           // bronze exhaust stacks
        b.box(24.4f, s * 6.4f, 5.6f, 0.7f, 1.0f, 0.7f, rlAmb).m(EMIT);                               // amber headlamps
        b.box(-9.0f, s * 6.4f, 10.2f, 2.6f, 1.7f, 0.5f, rlDrk).bev(0.3f);                            // smoke dischargers
    }
    b.box(-13.0f, 0, 9.9f, 6.4f, 10.6f, 0.5f, rlOch).t(T_PANEL, 5).bev(0.4f);                       // engine deck frame
    b.box(-13.0f, 0, 10.3f, 4.6f, 8.6f, 0.3f, rlDrk).t(T_GRILLE, 1.9f).bev(0.2f);                  // louvred grille
    for (int s = -1; s <= 1; s += 2) b.cyl('z', 7.0f, s * 4.6f, 10.1f, 2.1f, 0.4f, rlArm).bev(0.4f);   // driver's / loader's hatches
    b.cyl('z', -1, 0, 10.0f, 14.2f, 0.5f, rlDrk).bev(0.4f);                                         // bronze turret ring
    b.cyl('z', -1, 0, 12.8f, 15.0f, 2.8f, rlOch).f(F_SHADOWONLY);                                   // (the turret's shade on the hull)
    t.ground = false; t.dirt = 0.12f;
    t.taper(-2.0f, 0, 10.0f, 15.2f, 10.4f, 6.4f, 14.2f, 9.4f, rlOch, -1.0f, 0).t(T_PANEL, 7).bev(1.2f);
    t.box(-2.6f, 0, 16.6f, 10.4f, 7.4f, 0.3f, shade(rlOch, 1.12f)).bev(0.2f);                      // the lit roof plate
    t.box(-5.4f, 0, 16.9f, 6.6f, 5.4f, 0.35f, OWNER).bev(0.1f);                                    // the owner's roof panel
    t.cyl('z', -1.0f, 0, 17.0f, 3.4f, 0.6f, rlArm).bev(0.4f);                                      // commander's cupola
    t.cyl('z', -1.0f, 0, 17.2f, 2.3f, 0.4f, rlBrz).bev(0.2f);                                      // its bronze hatch rim
    t.box(-13.4f, 0, 13.6f, 3.4f, 7.6f, 2.2f, shade(rlArm, 0.92f)).t(T_PLANKS, 1.6f).bev(0.4f);     // rear bustle stowage
    for (int s = -1; s <= 1; s += 2) t.box(-6.8f, s * 5.6f, 16.9f, 0.7f, 1.0f, 0.4f, rlDrk).bev(0.2f);   // periscopes
    t.box(10.6f, 0, 12.4f, 4.6f, 6.6f, 2.4f, shade(rlArm, 1.05f)).bev(0.6f);                        // bolted mantlet
    t.box(4.6f, 0, 16.4f, 1.8f, 1.2f, 0.5f, rlDrk).bev(0.2f);                                       // sight box
    t.box(3.6f, 0, 16.6f, 0.5f, 0.6f, 0.3f, rlAmb).m(EMIT);                                         // its lamp
    for (int s = -1; s <= 1; s += 2) {
        t.rod({ 13.4f, s * 2.6f, 13.2f }, { 30.6f, s * 2.6f, 13.2f }, 1.1f, shade(rlDrk, 2.4f)).m(METAL).bev(0.2f);   // long cannons
        t.cyl('x', 17.6f, s * 2.6f, 13.2f, 1.8f, 1.6f, shade(rlArm, 1.1f)).bev(0.3f);                // sleeves at the trunnion
        t.box(29.4f, s * 2.6f, 13.2f, 2.0f, 1.5f, 1.1f, rlDrk).bev(0.3f);                            // slotted muzzle brakes
    }
}
M3D_UNIT(U_R_JUGGERNAUT, rJuggernaut)

// ---------------------------------------------------------------- Relic Strider
// A four-legged artillery walker: hips on outriggers and knees that reach wide, so the chassis rides clear of the ground; a stout hull with
// ochre side plates and a sensor dome at the nose; the back-mounted rocket box (2 x 4 tubes) is the part that aims. Frame 1 and 2 swap the
// diagonal leg pairs.
static void rStrider(Model& b, Model& t, int frame) {
    const float sw = frame == 1 ? 1.0f : (frame == 2 ? -1.0f : 0.0f);   // diagonal pairs swing together
    struct Leg { float hx, hy, kx, ky, fx, fy; float pair; };
    // front-left, front-right, rear-left, rear-right; front-left steps with rear-right
    const Leg legs[4] = { { 12, -10, 20, -19, 25, -27, 1 }, { 12, 10, 20, 19, 25, 27, -1 }, { -12, -10, -20, -19, -25, -27, -1 }, { -12, 10, -20, 19, -25, 27, 1 } };
    b.dirt = 0.5f;
    for (const Leg& l : legs) {
        const float sx = sw * l.pair * 2.6f, fx = l.fx + sx, kx = l.kx + sx * 0.55f;
        b.ball(l.hx, l.hy, 11.0f, 2.8f, 2.6f, 2.6f, rlBrz).m(METAL);                                // hip socket
        b.rod({ l.hx, l.hy, 11.0f }, { kx, l.ky, 8.6f }, 2.2f, rlArm).m(PAINT).t(T_PANEL, 5).bev(0.5f);   // thigh
        b.ball(kx, l.ky, 8.6f, 2.4f, 2.2f, 2.2f, rlBrz).m(METAL);                                    // knee joint
        b.rod({ kx, l.ky, 8.6f }, { fx, l.fy, 2.4f }, 1.7f, rlArm).m(PAINT).bev(0.4f);               // shin
        b.rod({ kx + (fx - kx) * 0.2f, l.ky + (l.fy - l.ky) * 0.2f, 7.6f }, { kx + (fx - kx) * 0.75f, l.ky + (l.fy - l.ky) * 0.75f, 4.2f },
              0.8f, rlDrk).m(METAL).bev(0);                                                          // hydraulic cylinder
        b.ball(fx, l.fy, 1.5f, 2.9f, 2.4f, 1.5f, rlDrk).m(MATTE).bev(0.3f);                         // splayed foot pad
        b.ball(fx, l.fy, 1.4f, 1.6f, 1.4f, 1.5f, rlBrz).m(METAL);                                   // its bronze boss
    }
    // the hull: a stout armoured body, sloped at both ends, lifted well clear of the ground
    b.profileY({ { -16, 10.0f }, { -10, 15.2f }, { 10, 15.6f }, { 18, 12.4f }, { 21.0f, 9.0f }, { 10, 6.4f }, { -10, 6.2f }, { -16, 7.2f } },
               -8.0f, 8.0f, rlArm).t(T_PANEL, 6).bev(0.9f);
    for (int s = -1; s <= 1; s += 2) {
        b.box(-1.0f, s * 7.6f, 11.4f, 11.0f, 2.0f, 2.0f, rlOch).t(T_PANEL, 5).bev(0.5f);          // ochre side plates
        b.box(-1.0f, s * 7.6f, 13.4f, 10.4f, 2.4f, 0.4f, OWNER).bev(0.1f);                           // the owner's marking on top of each plate
        b.box(-9.0f, s * 3.0f, 16.0f, 2.0f, 1.4f, 0.4f, rlArm).bev(0.3f);                            // hatches flanking the spine
    }
    b.box(-12.4f, 0, 15.8f, 4.6f, 5.2f, 0.5f, shade(rlDrk, 1.5f)).t(T_GRILLE, 1.5f).bev(0.3f);     // rear power pod with louvres
    b.rod({ -12.0f, 0, 15.4f }, { 12.0f, 0, 16.0f }, 0.8f, rlDrk).m(METAL).bev(0);                  // hydraulic run down the spine
    b.ball(19.0f, 0, 10.6f, 2.8f, 2.6f, 2.2f, shade(rlArm, 1.1f)).bev(0.4f);                        // sensor dome at the nose
    b.ball(20.6f, 0, 10.6f, 1.0f, 1.0f, 1.0f, rlAmb).m(EMIT);                                       // amber lens
    b.cyl('z', -1.0f, 0, 15.9f, 7.6f, 0.4f, rlBrz).bev(0.3f);                                       // bronze turntable for the rocket box
    b.cyl('z', -1.0f, 0, 17.6f, 8.4f, 2.0f, rlOch).f(F_SHADOWONLY);                                 // (the box's shade on the hull)
    t.ground = false; t.dirt = 0.1f;
    // the back-mounted rocket box: a bronze-strapped slab with a bank of eight tube mouths, two rows of four
    t.box(-1.0f, 0, 17.6f, 12.0f, 8.0f, 2.6f, rlOch).t(T_PANEL, 6).bev(0.5f);
    t.box(-1.0f, 0, 20.0f, 10.6f, 6.8f, 0.35f, shade(rlOch, 1.12f)).bev(0.2f);                     // the lid, lit
    t.box(-12.4f, 0, 20.3f, 3.4f, 6.6f, 0.35f, OWNER).bev(0.1f);                                   // the owner's panel at the lid's rear
    for (int s = -1; s <= 1; s += 2) t.box(-6.6f + s * 13.2f, 0, 17.6f, 0.5f, 7.6f, 2.7f, rlBrz).m(METAL).bev(0.2f);   // bronze straps
    t.box(-1.0f, 0, 20.3f, 11.0f, 7.2f, 0.12f, rlDrk).bev(0.1f);                                  // dark mouth plate of the box
    for (int r = 0; r < 2; r++) for (int i = 0; i < 4; i++)   // the eight tube mouths open through the lid: two rows of four
        t.cyl('z', -8.0f + i * 5.4f, r == 0 ? -2.6f : 2.6f, 20.4f, 1.9f, 0.16f, rlBrz).m(METAL).bev(0.2f);
    t.box(-13.4f, 0, 18.4f, 1.4f, 4.6f, 1.4f, shade(rlDrk, 1.4f)).bev(0.3f);                       // rear sight housing
    t.box(-14.6f, 0, 19.0f, 0.4f, 0.8f, 0.5f, rlAmb).m(EMIT);                                       // its lamp
    t.cyl('z', 4.0f, 0, 20.6f, 1.8f, 0.5f, rlArm).bev(0.3f);                                        // commander's hatch
    t.rod({ -9.0f, 6.4f, 20.0f }, { -12.0f, 7.2f, 29.0f }, 0.25f, rlDrk).f(F_NOSHADOW);              // aerial
}
M3D_UNIT(U_R_STRIDER, rStrider)

}  // namespace
