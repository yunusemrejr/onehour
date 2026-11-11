// One Hour - 3D models: the twenty foot soldiers of both armies (see m3d.h / m3d_parts.h).
// Every one of them is the shared parametric trooper, dressed with its own colours, bulk, headgear, weapon and one signature prop that tells it
// apart at a glance: a coiled laser rifle, a rocket tube, an arc fork, a gunner's bipod and ammo box, an ion lance, a grenade drum, a medical
// cross, a ghillie cloak, a brimmed hat and briefcase, a commando power pack, the Chosen's cloak and standard, the Shadow Guard's launcher tube,
// a rioter's armband and a petrol bomb. Cyber: white ceramic plate over graphite, cyan light. Clanker: khaki, olive, steel and rust, amber light.
#include "m3d_parts.h"

using namespace m3d;

namespace {

// ---------------------------------------------------------------- shared extras
// (trooper units, before the figure is enlarged: torso top z 12, head centre 13.6, shoulders y +-3.5 * bulk, barrel line y 1.6, z 10.6)

// a cloak that leaves the shoulders and falls back to the knees: because the view looks down, the slab leans back with height so the drape
// shows as a widening shape behind the figure (the old art draws the same thing from above). It stays close to the pack so it hangs, not floats.
void cloak(Model& m, float b, float zTop, float zBot, float wTop, float wBot, Color outer, Color lining) {
    const float xTop = -2.0f * b, xBot = -4.0f * b, h = zTop - zBot;
    // three strips of slightly different depth: the middle one stands proud, so the drape reads as folds rather than a flat plate
    const float sh[3] = { 0.36f, 0.28f, 0.36f };            // each strip's share of the hem (they sum to 1)
    const float back[3] = { 0.22f, -0.30f, 0.22f };         // how far each hangs back
    float y0 = -wBot;
    for (int i = 0; i < 3; i++) {
        const float y1 = y0 + 2 * wBot * sh[i];
        const float ty0 = y0 * wTop / wBot, ty1 = y1 * wTop / wBot;
        m.taper(xBot + back[i] * b, (y0 + y1) * 0.5f, zBot, 0.72f * b, (y1 - y0) * 0.5f, h, 0.62f * b, (ty1 - ty0) * 0.5f, outer,
                xTop - xBot - back[i] * b, (ty0 + ty1) * 0.5f - (y0 + y1) * 0.5f).m(MATTE).bev(0.4f);
        y0 = y1;
    }
    // the lining sits just behind the cloth and a little wider, so its colour rims the cape instead of hiding behind it
    m.taper(xBot - 1.15f * b, 0, zBot + 0.25f, 0.26f * b, wBot + 0.25f, h - 0.7f, 0.2f * b, wTop + 0.2f, lining, xTop - xBot, 0).m(MATTE).bev(0.3f);
    m.box(xBot - 1.5f * b, 0, zBot + 0.7f, 0.5f * b, wBot - 0.35f, 0.5f, outer).m(MATTE).bev(0.35f);   // the hem folds back on itself
}

// a plus of two bars on a face ('x' front/back, 'y' a flank, 'z' the top): the medics' cross
void crossMark(Model& m, char face, float x, float y, float z, float arm, float bar, float depth, Color c) {
    if (face == 'x') { m.box(x, y, z, depth, arm, bar, c).m(PAINT); m.box(x, y, z, depth, bar, arm, c).m(PAINT); }
    else if (face == 'y') { m.box(x, y, z, arm, depth, bar, c).m(PAINT); m.box(x, y, z, bar, depth, arm, c).m(PAINT); }
    else { m.box(x, y, z, arm, bar, depth, c).m(PAINT); m.box(x, y, z, bar, arm, depth, c).m(PAINT); }
}

// glowing rings down a barrel that runs from (xa, y, za) to (xb, y, zb), set in a dark shroud: a laser rifle's coil
void coil(Model& m, float xa, float za, float xb, float zb, int n, float y, float r, Color c) {
    const float ux = (xb - xa) / n;
    m.rod({ xa - 0.3f, y, za - 0.3f }, { xb + 0.3f, y, zb + 0.3f }, r * 0.66f, C(0x23262C)).m(METAL).bev(0);   // the shroud the rings sit on
    for (int i = 0; i < n; i++) {
        const float t = (i + 0.5f) / n;
        const float rr = r * (1.0f - 0.22f * t);
        m.cyl('x', xa + (xb - xa) * t, y, za + (zb - za) * t, rr, ux * 0.16f, c).m(EMIT).bev(0.05f);
    }
    m.cyl('x', xa - 0.5f, y, za - 0.35f, r * 0.75f, 0.3f, C(0x30343C)).m(METAL).bev(0.1f);   // its rear collar
}

// a thin two-legged bipod under the muzzle, its feet a hand's width apart
void bipod(Model& m, float x, float y, float z, Color c) {
    for (int s = -1; s <= 1; s += 2) m.rod({ x, y + s * 0.5f, z }, { x + 2.9f, y + s * 2.5f, z - 2.9f }, 0.28f, c).m(METAL).bev(0);
}

// a scope on top of a barrel that runs through (x, y, z)
void scope(Model& m, float x, float y, float z, float len, Color c, Color lens) {
    m.cyl('x', x, y, z + 1.15f, 0.55f, len, c).m(METAL).bev(0.15f);
    m.box(x - len * 0.4f, y, z + 0.65f, 0.3f, 0.35f, 0.5f, c).m(METAL).bev(0);
    m.box(x + len * 0.4f, y, z + 0.65f, 0.3f, 0.35f, 0.5f, c).m(METAL).bev(0);
    if (lens.a) m.cyl('x', x + len + 0.1f, y, z + 1.15f, 0.42f, 0.12f, lens).m(EMIT);
}

// loose ghillie strands over a cloak's shoulders and hem
void ghillie(Model& m, float b, int n, Color a, Color c) {
    for (int i = 0; i < n; i++) {
        const float y = -4.6f + i * (9.2f / (n - 1));
        const float z = 5.2f + ((i * 7) % 5) * 1.15f;
        m.ball(-4.6f * b - (i % 2) * 0.6f, y, z, 1.05f, 0.85f, 0.7f, (i % 2) ? a : c).m(FOLIAGE);
    }
}

// a stowed rocket: a tube with a warhead at the front
void rocket(Model& m, V3 a, V3 d, float r, Color tube, Color head) {
    const V3 b2 = a + d;
    m.rod(a, b2, r, tube).m(PAINT).bev(0.15f);
    m.rod(b2, b2 + norm(d) * 1.1f, r * 0.85f, head, 0.25f).m(PAINT).bev(0.1f);
}

// ---------------------------------------------------------------- the Cyber line: INF2, INF3, ELITE
static void cInf2(Model& b, Model&, int f) {   // Laser Trooper: pale plate, cyan visor, a coiled laser rifle
    Trooper p; p.suit = pal::cDark; p.armour = C(0xCFD8E2); p.helmet = C(0xE2E8EF); p.pack = C(0x465062); p.skin = C(0xC79A72);
    p.head = 1; p.light = pal::cGlow; p.gun = C(0xBCC8D6); p.weapon = 2;
    coil(b, 4.6f, 10.70f, 11.4f, 10.87f, 4, 1.6f, 0.82f, pal::cGlow);                   // the capacitor coil down the barrel
    b.cyl('x', 12.2f, 1.6f, 10.89f, 1.1f, 0.7f, C(0x515C6B)).m(METAL).bev(0.15f);       // muzzle shroud
    b.box(2.4f, 1.6f, 11.6f, 1.6f, 0.4f, 0.32f, C(0x3A4552)).m(METAL).bev(0.1f);       // sight rail
    b.box(-1.9f, 2.6f, 8.9f, 0.5f, 0.9f, 0.7f, pal::cGlow).m(EMIT).f(F_NOSHADOW);       // a cell at the hip, faintly lit
    trooper(b, p, f);
}
M3D_UNIT(U_C_INF2, cInf2)

static void cInf3(Model& b, Model&, int f) {   // Shock Trooper: an arc caster at the hip with a forked emitter, a capacitor hump
    Trooper p; p.suit = C(0x262B34); p.armour = C(0xAEB9C6); p.helmet = C(0xC4CDD8); p.pack = C(0x3E4756); p.bulk = 1.15f;
    p.head = 1; p.light = pal::cGlow; p.gun = C(0x7C8896); p.weapon = 4;
    b.box(2.0f, 2.6f, 8.6f, 2.6f, 1.35f, 1.5f, C(0x4A5563)).m(METAL).bev(0.3f);          // the caster's body round the muzzle
    for (int s = -1; s <= 1; s += 2) {                                                  // the fork: two solid prongs off it
        b.rod({ 4.4f, 2.6f + s * 0.9f, 8.6f }, { 9.4f, 2.6f + s * 2.4f, 8.7f }, 0.48f, C(0x6B7887)).m(METAL).bev(0);
        b.cyl('x', 10.1f, 2.6f + s * 2.55f, 8.72f, 0.62f, 0.7f, C(0x8E9AA8), 0.7f).m(METAL).bev(0.1f);
        b.ball(10.9f, 2.6f + s * 2.6f, 8.74f, 0.34f, 0.34f, 0.34f, pal::cGlow).m(EMIT).bev(0);
    }
    b.box(-4.4f, 0, 11.6f, 1.4f, 2.6f, 1.7f, C(0x36404F)).t(T_GRILLE, 1.2f).bev(0.4f);   // capacitor hump over the pack
    b.box(-5.0f, 0, 12.9f, 0.7f, 1.5f, 0.28f, pal::cGlow).m(EMIT).f(F_NOSHADOW);
    trooper(b, p, f);
}
M3D_UNIT(U_C_INF3, cInf3)

static void cElite(Model& b, Model&, int f) {   // Ion Lancer: tall enough to read as elite, a crest fin, a lance that out-reaches a rifle
    Trooper p; p.suit = C(0x232833); p.armour = C(0xE7EDF3); p.helmet = C(0xF3F7FA); p.pack = C(0x4E5B6E); p.bulk = 1.2f;
    p.head = 1; p.light = pal::cGlow; p.gun = C(0xD6E0EA); p.weapon = 2;
    b.box(0.1f, 0, 16.5f, 1.25f, 0.34f, 1.5f, pal::cTrim).m(PAINT).bev(0.15f);          // crest fin on the helmet
    b.box(0.1f, 0, 16.5f, 1.15f, 0.1f, 0.55f, pal::cGlow).m(EMIT).f(F_NOSHADOW);
    for (int s = -1; s <= 1; s += 2) b.box(0, s * 4.5f, 12.4f, 1.2f, 1.0f, 0.35f, pal::cTrim).bev(0.2f);   // cobalt pauldron caps
    b.cyl('x', 12.6f, 1.6f, 10.88f, 0.95f, 0.75f, C(0x6E7C8E)).m(METAL).bev(0.15f);     // lance throat
    for (int i = 0; i < 3; i++) b.box(10.2f + i * 1.1f, 1.6f, 10.86f, 0.14f, 1.15f, 0.28f, C(0x535F6E)).m(METAL).bev(0);
    b.rod({ 14.3f, 1.6f, 10.96f }, { 17.6f, 1.6f, 11.0f }, 0.3f, C(0xB9C6D4)).m(METAL).bev(0);   // the needle
    b.cyl('x', 18.0f, 1.6f, 11.0f, 0.5f, 0.5f, C(0x8E9AA8), 0.15f).m(METAL).bev(0.1f);
    trooper(b, p, f);
}
M3D_UNIT(U_C_ELITE, cElite)

// ---------------------------------------------------------------- the Clanker line: INF2, INF3, ELITE
static void kInf2(Model& b, Model&, int f) {   // RPG Trooper: a rocket tube over the shoulder, a quiver of reloads on the pack
    Trooper p; p.suit = C(0x6E6A48); p.armour = C(0x575C3A); p.helmet = C(0x4A5034); p.pack = C(0x6B5836);
    p.gun = C(0x4C4A42); p.weapon = 1;
    for (int s = -1; s <= 1; s += 2) rocket(b, { -3.9f, s * 1.6f, 7.4f }, { -1.5f, s * 0.5f, 4.8f }, 0.7f, C(0x6E5C38), C(0x9A4430));   // spare rockets
    b.cyl('x', 8.1f, 3.0f, 13.85f, 1.45f, 1.6f, C(0x8E4030), 0.3f).m(PAINT).bev(0.2f);    // the loaded warhead
    b.box(-3.6f, -3.0f, 12.2f, 1.0f, 1.6f, 0.9f, C(0x5C4C30)).m(MATTE).bev(0.2f);        // a satchel of spare rounds
    trooper(b, p, f);
}
M3D_UNIT(U_K_INF2, kInf2)

static void kInf3(Model& b, Model&, int f) {   // Gunner: gas mask, a heavy machine gun at the hip on its bipod, an ammo box
    Trooper p; p.suit = C(0x5A5D3A); p.armour = C(0x474D30); p.helmet = C(0x3E4429); p.pack = C(0x51462E); p.bulk = 1.15f;
    p.head = 3; p.light = pal::kGlow; p.gun = C(0x32322C); p.weapon = 4;
    b.cyl('x', 5.4f, 2.6f, 8.72f, 1.55f, 3.2f, C(0x4A4A42)).m(METAL).bev(0.2f);          // jacket round the receiver
    b.cyl('x', 10.4f, 2.6f, 8.9f, 1.3f, 0.7f, C(0x2C2C28)).m(METAL).bev(0.15f);           // flash hider
    bipod(b, 9.2f, 2.6f, 8.88f, C(0x2E2E2A));
    b.box(2.4f, 5.4f, 7.9f, 1.9f, 1.2f, 1.1f, C(0x6B5B33)).m(PAINT).bev(0.2f);           // ammo box on the belt side
    b.rod({ 3.6f, 5.0f, 8.6f }, { 4.4f, 3.4f, 8.5f }, 0.35f, C(0xB9953F)).m(METAL).bev(0);   // the belt
    b.box(-3.2f, 0, 12.4f, 1.5f, 2.2f, 0.6f, C(0x3E4429)).m(MATTE).bev(0.3f);            // bedroll over the pack
    trooper(b, p, f);
}
M3D_UNIT(U_K_INF3, kInf3)

static void kElite(Model& b, Model&, int f) {   // Grenadier: a fat drum-fed launcher, hazard trim, a bandolier of grenades
    Trooper p; p.suit = C(0x76714A); p.armour = C(0x8B8459); p.helmet = C(0x5E6339); p.pack = C(0x6B5836); p.bulk = 1.2f;
    p.head = 0; p.light = pal::kGlow; p.gun = C(0x3E3E34); p.weapon = 4;
    b.cyl('x', 5.6f, 2.6f, 8.72f, 1.6f, 3.4f, C(0x5A5644)).t(T_HAZARD, 1.6f, pal::kHaz).bev(0.2f);   // the launcher's sleeve
    b.cyl('x', 10.5f, 2.6f, 8.9f, 1.5f, 0.7f, C(0x2F2F29)).m(METAL).bev(0.2f);            // muzzle
    b.cyl('y', 2.6f, 5.0f, 7.6f, 1.85f, 1.0f, C(0x4E4A38)).m(METAL).bev(0.25f);           // the revolving drum
    b.cyl('y', 2.6f, 5.0f, 7.6f, 0.7f, 1.15f, C(0x2A2A24)).m(METAL).bev(0.1f);
    for (int i = 0; i < 3; i++) b.ball(1.9f, -2.2f + i * 1.9f, 10.6f - i * 0.35f, 0.62f, 0.62f, 0.8f, C(0x4A5230)).m(PAINT);   // grenades on the chest
    b.box(-3.4f, 0, 12.2f, 1.4f, 2.4f, 0.55f, C(0x6B5B33)).m(MATTE).bev(0.3f);
    trooper(b, p, f);
}
M3D_UNIT(U_K_ELITE, kElite)

// ---------------------------------------------------------------- medics (unarmed, a cross on the pack, a case or a satchel)
static void cMedic(Model& b, Model&, int f) {   // Cyber field medic: white plate, teal cross on the pack, a case in the hand, a beacon
    Trooper p; p.suit = C(0x39424E); p.armour = C(0xE6EEF3); p.helmet = C(0xF4F8FB); p.pack = C(0x24303C); p.weapon = 3;
    p.boot = C(0x2B2F38); p.gun = C(0x30363F);
    b.box(-2.6f, 0, 11.9f, 1.3f, 2.3f, 0.1f, C(0x2A7A6A)).m(PAINT);                      // a cross-shaped plate on the dark pack
    crossMark(b, 'z', -2.6f, 0, 12.08f, 1.95f, 0.68f, 0.12f, C(0x40F0C4));
    b.cyl('z', -1.4f, 1.9f, 12.4f, 0.5f, 0.7f, C(0x596577)).m(METAL).bev(0.1f);
    b.ball(-1.4f, 1.9f, 13.2f, 0.52f, 0.52f, 0.42f, C(0x50F0C0)).m(EMIT).f(F_NOSHADOW);   // beacon on the pack
    b.box(1.5f, -4.2f, 6.1f, 1.5f, 1.5f, 1.3f, C(0xF2F7FA)).m(PAINT).bev(0.35f);         // the case in the hand
    b.box(1.5f, -4.2f, 7.42f, 1.2f, 1.2f, 0.08f, C(0x2A7A6A)).m(PAINT);
    crossMark(b, 'z', 1.5f, -4.2f, 7.55f, 1.0f, 0.34f, 0.1f, C(0x40F0C4));
    b.box(1.5f, -3.4f, 5.6f, 1.1f, 0.3f, 0.5f, C(0x7C8B9C)).m(METAL).bev(0.1f);          // case latch
    trooper(b, p, f);
}
M3D_UNIT(U_C_MEDIC, cMedic)

static void kMedic(Model& b, Model&, int f) {   // Clanker medic: cream apron over khaki, a red cross, a canvas satchel
    Trooper p; p.suit = C(0x6E6A48); p.armour = C(0xE2DCBC); p.helmet = C(0xCFCAA4); p.pack = C(0x9A9268); p.weapon = 3;
    p.boot = C(0x3A3428); p.gun = C(0x3A2C22);
    b.box(-4.5f, 0, 11.5f, 2.1f, 2.2f, 0.9f, C(0xCFCAA4)).m(MATTE).bev(0.3f);           // the bedroll over the pack
    crossMark(b, 'z', -4.5f, 0, 12.45f, 1.85f, 0.62f, 0.2f, C(0xD22A26));                // the cross on it
    b.box(0.4f, 4.9f, 7.2f, 1.9f, 1.3f, 1.7f, C(0xA89A6A)).m(MATTE).bev(0.3f);           // satchel on the hip
    crossMark(b, 'y', 0.4f, 6.25f, 7.2f, 1.1f, 0.3f, 0.18f, C(0xD22A26));
    b.rod({ 0.4f, 4.8f, 9.0f }, { 0.4f, 1.8f, 12.2f }, 0.28f, C(0x5C5232)).m(MATTE).bev(0);   // its strap over the shoulder
    b.ball(1.9f, -3.9f, 7.6f, 0.72f, 0.72f, 0.9f, C(0x3A3428)).m(GLASS).bev(0);          // a bottle in the fist
    trooper(b, p, f);
}
M3D_UNIT(U_K_MEDIC, kMedic)

// ---------------------------------------------------------------- snipers (a cloak, a scope, a bipod)
static void cSniper(Model& b, Model&, int f) {   // Ghost Sniper: teal cloak, hood, a long needle rifle
    Trooper p; p.suit = C(0x243038); p.armour = C(0x35545C); p.helmet = C(0x27424A); p.pack = C(0x2C3E44); p.bulk = 0.95f;
    p.head = 2; p.light = pal::cGlow; p.gun = C(0xB9C6D4); p.weapon = 2; p.boot = C(0x1B2226);
    cloak(b, 0.95f, 12.0f, 3.6f, 3.7f, 5.2f, C(0x27454E), C(0x40808A));
    b.box(-1.4f, 0, 12.4f, 0.75f, 2.3f, 0.85f, C(0x2E4C54)).m(MATTE).bev(0.3f);          // high collar behind the hood
    b.box(-6.4f, 0, 6.5f, 0.55f, 4.7f, 1.6f, C(0x22383E)).m(MATTE).bev(0.3f);            // the cloak's hem at the back
    scope(b, 7.6f, 1.6f, 10.75f, 1.6f, C(0x4A5866), pal::cGlow);
    bipod(b, 11.6f, 1.6f, 10.87f, C(0x46586A));
    b.cyl('x', 13.6f, 1.6f, 10.92f, 0.75f, 0.6f, C(0x5E6C80)).m(METAL).bev(0.1f);        // muzzle brake
    b.ball(14.5f, 1.6f, 10.92f, 0.3f, 0.3f, 0.3f, pal::cGlow).m(EMIT);
    trooper(b, p, f);
}
M3D_UNIT(U_C_SNIPER, cSniper)

static void kSniper(Model& b, Model&, int f) {   // Sniper: an olive ghillie cloak with leaf strands, a scoped rifle, a bipod
    Trooper p; p.suit = C(0x5E6440); p.armour = C(0x4E5434); p.helmet = C(0x565C3A); p.pack = C(0x5B4A32); p.bulk = 0.95f;
    p.head = 2; p.gun = C(0x3A3A32); p.weapon = 2; p.boot = C(0x2C2A20);
    cloak(b, 0.95f, 12.0f, 3.4f, 3.8f, 5.4f, C(0x4E5434), C(0x707646));
    ghillie(b, 0.95f, 8, C(0x707848), C(0x565E32));
    b.box(-1.5f, 0, 12.4f, 0.75f, 2.4f, 0.9f, C(0x424A2C)).m(MATTE).bev(0.3f);           // collar of the cloak
    b.box(-6.6f, 0, 6.3f, 0.55f, 4.9f, 1.7f, C(0x3E4429)).m(MATTE).bev(0.3f);             // its hem at the back
    scope(b, 7.6f, 1.6f, 10.75f, 1.6f, C(0x3E4232), pal::kGlow);
    bipod(b, 11.6f, 1.6f, 10.87f, C(0x2E2E28));
    b.cyl('x', 13.6f, 1.6f, 10.92f, 0.72f, 0.6f, C(0x2A2A24)).m(METAL).bev(0.1f);
    b.ball(-2.5f, -2.6f, 11.4f, 0.85f, 0.95f, 0.8f, C(0x6B7444)).m(FOLIAGE);            // leaves over the shoulders
    b.ball(-2.0f, 2.7f, 11.0f, 0.8f, 0.9f, 0.75f, C(0x5A6236)).m(FOLIAGE);
    b.ball(-3.4f, 1.2f, 9.0f, 0.8f, 0.85f, 0.7f, C(0x66703E)).m(FOLIAGE);
    trooper(b, p, f);
}
M3D_UNIT(U_K_SNIPER, kSniper)

// ---------------------------------------------------------------- spies (unarmed, a coat or a bodysuit)
static void cSpy(Model& b, Model&, int f) {   // Infiltrator: black bodysuit, a glowing visor, a hacking slab
    Trooper p; p.suit = C(0x2A2D36); p.armour = C(0x22252C); p.helmet = C(0x1E2128); p.pack = C(0x2A2D36); p.bulk = 0.9f;
    p.head = 1; p.light = pal::cGlow; p.weapon = 3; p.backpack = false; p.boot = C(0x14161A); p.gun = C(0x30363F);
    b.box(2.3f, 3.7f, 7.4f, 1.5f, 1.9f, 0.4f, C(0x181A20)).m(PAINT).bev(0.15f);         // the hacking slab
    b.box(2.55f, 3.7f, 7.85f, 1.15f, 1.5f, 0.1f, pal::cGlow).m(EMIT).f(F_NOSHADOW);      // its screen
    b.rod({ -1.2f, 0, 11.9f }, { -1.6f, 0, 6.9f }, 0.35f, C(0x39404C)).m(PAINT).bev(0);   // a slim equipment back
    b.box(-1.8f, 0, 9.4f, 0.4f, 1.2f, 1.0f, C(0x39404C)).m(PAINT).bev(0.15f);
    trooper(b, p, f);
}
M3D_UNIT(U_C_SPY, cSpy)

static void kSpy(Model& b, Model&, int f) {   // Spy: a long trench coat, a brimmed hat, a briefcase
    Trooper p; p.suit = C(0x8A7A56); p.armour = C(0xB29868); p.helmet = C(0x3A2C1E); p.pack = C(0x8A7A56); p.bulk = 0.9f;
    p.head = 2; p.weapon = 3; p.boot = C(0x2C2418); p.gun = C(0x30363F);
    cloak(b, 0.9f, 11.4f, 2.2f, 3.4f, 4.4f, C(0xB29868), C(0x6E5C3A));
    b.box(-1.5f, 0, 12.3f, 0.8f, 2.5f, 0.95f, C(0x8A7450)).m(MATTE).bev(0.25f);          // turned-up collar
    b.cyl('z', 0.1f, 0, 14.9f, 3.35f, 0.3f, C(0x3A2C1E)).m(MATTE).bev(0.3f);             // hat brim
    b.cyl('z', 0.1f, 0, 15.7f, 1.9f, 0.9f, C(0x46341F)).m(MATTE).bev(0.3f);              // crown
    b.cyl('z', 0.1f, 0, 15.35f, 2.0f, 0.16f, C(0x241A10)).m(MATTE).bev(0.1f);            // its hatband
    b.box(1.9f, -3.9f, 5.6f, 1.6f, 1.9f, 0.85f, C(0x6B4A2E)).m(PAINT).bev(0.3f);         // briefcase
    b.box(2.6f, -3.9f, 6.4f, 0.1f, 0.5f, 0.2f, pal::kBrass).m(METAL);                    // its clasp
    trooper(b, p, f);
}
M3D_UNIT(U_K_SPY, kSpy)

// ---------------------------------------------------------------- laser commandos (bulky, a power pack, a lance that glows toxic green)
static void cCommando(Model& b, Model&, int f) {   // Prism Commando: white plate, cyan visor, a heavy power pack, a toxic lance
    Trooper p; p.suit = C(0x1E232C); p.armour = C(0xB8C6D6); p.helmet = C(0xE7EDF3); p.pack = C(0x34405A); p.bulk = 1.3f;
    p.head = 0; p.light = C(0x96FF50); p.gun = C(0xCED8E4); p.weapon = 2; p.boot = C(0x1A1D24);
    b.box(-4.2f, 0, 9.6f, 1.5f, 3.3f, 2.7f, C(0x34405A)).t(T_GRILLE, 1.3f).bev(0.5f);    // the power pack
    for (int s = -1; s <= 1; s += 2) {
        b.box(-5.6f, s * 1.9f, 10.6f, 0.45f, 0.55f, 1.5f, pal::cGlow).m(EMIT).f(F_NOSHADOW);   // its vent lights
        b.box(0, s * 5.9f, 11.6f, 1.7f, 0.85f, 1.5f, C(0xD4DEE8)).m(PAINT).bev(0.4f);    // heavy pauldron
        b.box(0.6f, s * 2.6f, 11.95f, 1.1f, 1.0f, 0.45f, C(0x3C4658)).m(PAINT).bev(0.2f);   // forearm bracer
    }
    b.box(1.85f, 0, 13.7f, 0.4f, 1.45f, 0.42f, pal::cGlow).m(EMIT).f(F_NOSHADOW);        // the cyan visor bar
    coil(b, 5.2f, 10.72f, 11.6f, 10.87f, 5, 1.6f, 0.95f, C(0x96FF50));                   // the lance's coil, glowing toxic green
    b.cyl('x', 12.4f, 1.6f, 10.89f, 1.35f, 0.85f, C(0x5E6C80)).m(METAL).bev(0.2f);       // emitter housing
    b.box(2.0f, -1.6f, 9.4f, 0.6f, 0.9f, 1.4f, C(0x3A4454)).m(METAL).bev(0.15f);         // chest rig
    trooper(b, p, f);
}
M3D_UNIT(U_C_LASERCMD, cCommando)

static void kCommando(Model& b, Model&, int f) {   // Venom Commando: rust pauldrons over olive plate, a gas mask, two canister packs
    Trooper p; p.suit = C(0x3E4229); p.armour = C(0x686C46); p.helmet = C(0x4E5434); p.pack = C(0x4A4630); p.bulk = 1.3f;
    p.head = 3; p.light = C(0x96FF50); p.gun = C(0x3A3C34); p.weapon = 2; p.boot = C(0x222318);
    b.box(-4.2f, 0, 9.4f, 1.5f, 3.2f, 2.6f, C(0x4A4630)).m(PAINT).bev(0.5f);             // the power pack
    for (int s = -1; s <= 1; s += 2) {
        b.cyl('z', -4.6f, s * 2.1f, 11.9f, 1.0f, 0.9f, C(0x8A4A2B)).m(METAL).bev(0.25f);   // rust canisters
        b.box(0, s * 5.9f, 11.4f, 1.8f, 0.9f, 1.5f, C(0x8A4A2B)).m(PAINT).t(T_HAZARD, 1.4f, pal::kHaz).bev(0.4f);
        b.box(0.6f, s * 2.6f, 11.95f, 1.1f, 1.0f, 0.45f, C(0x3A3428)).m(PAINT).bev(0.2f);
    }
    coil(b, 5.2f, 10.72f, 11.6f, 10.87f, 5, 1.6f, 0.95f, C(0x96FF50));
    b.cyl('x', 12.4f, 1.6f, 10.89f, 1.35f, 0.85f, C(0x2E2E28)).m(METAL).bev(0.2f);
    b.box(2.0f, -1.6f, 9.4f, 0.6f, 0.9f, 1.4f, C(0x3A3428)).m(PAINT).bev(0.15f);
    b.rod({ 1.2f, 0.9f, 12.8f }, { 1.1f, 2.4f, 10.4f }, 0.4f, C(0x24261E)).m(RUBBER).bev(0);   // the mask's hose
    trooper(b, p, f);
}
M3D_UNIT(U_K_LASERCMD, kCommando)

// ---------------------------------------------------------------- The Chosen (one hero, a cloak and a standard) and its Shadow Guard
static void cChosen(Model& b, Model&, int f) {
    Trooper p; p.suit = C(0x2E3644); p.armour = C(0x9FB0C4); p.helmet = C(0xE8EEF4); p.pack = C(0x596577); p.bulk = 1.3f;
    p.head = 2; p.light = pal::cGlow; p.gun = C(0xA8B8CA); p.weapon = 2; p.boot = C(0x1E222A);
    cloak(b, 1.3f, 12.2f, 2.6f, 4.0f, 5.6f, C(0xCBD8E6), C(0x7C90AC));
    b.box(-1.6f, 0, 12.6f, 0.9f, 2.6f, 1.1f, C(0xB6C4D4)).m(MATTE).bev(0.3f);            // the collar stands up behind the hood
    b.rod({ -3.4f, 2.6f, 8.4f }, { -3.9f, 3.2f, 18.4f }, 0.4f, C(0x8E9AA8)).m(METAL).bev(0);   // the staff of its standard
    b.rod({ -3.9f, 3.2f, 18.4f }, { -4.1f, 3.3f, 20.2f }, 0.17f, pal::cGlow).m(EMIT).f(F_NOSHADOW);
    b.box(-3.6f, 3.25f, 16.6f, 0.16f, 1.7f, 1.5f, C(0x2C5AA0)).m(MATTE).bev(0.1f);        // its banner in the army's cobalt
    b.box(-3.78f, 3.25f, 16.6f, 0.08f, 1.25f, 0.3f, pal::cGlow).m(EMIT).f(F_NOSHADOW);    // and the cyan sigil on it
    b.box(1.85f, 0, 13.7f, 0.4f, 1.45f, 0.4f, pal::cGlow).m(EMIT).f(F_NOSHADOW);           // the visor under the hood
    b.box(0.8f, 0, 15.6f, 1.0f, 1.3f, 0.35f, pal::cTrim).bev(0.2f);                        // a circlet over the hood
    coil(b, 5.2f, 10.72f, 12.4f, 10.9f, 4, 1.6f, 0.9f, pal::cGlow);                         // the staff-blade's coil
    b.rod({ 13.6f, 1.6f, 10.98f }, { 17.4f, 1.6f, 11.02f }, 0.5f, C(0xCFE8F4), 0.15f).m(EMIT);   // the glowing point
    b.cyl('x', 12.4f, 1.6f, 10.9f, 1.3f, 0.8f, C(0x8A98A8)).m(METAL).bev(0.2f);
    trooper(b, p, f);
}
M3D_UNIT(U_C_CHOSEN, cChosen)

static void kChosen(Model& b, Model&, int f) {
    Trooper p; p.suit = C(0x3A363E); p.armour = C(0x4A444C); p.helmet = C(0x2A262E); p.pack = C(0x3A363E); p.bulk = 1.3f;
    p.head = 2; p.light = C(0xFFBE46); p.gun = C(0x2E2C2E); p.weapon = 2; p.boot = C(0x24201E);
    cloak(b, 1.3f, 12.2f, 2.4f, 4.0f, 5.8f, C(0x3A343C), C(0xA81E2C));
    b.box(-1.6f, 0, 12.6f, 0.9f, 2.6f, 1.1f, C(0x33303A)).m(MATTE).bev(0.3f);
    for (int s = -1; s <= 1; s += 2) b.box(0, s * 5.4f, 11.6f, 1.6f, 0.9f, 1.3f, C(0x6E6872)).m(PAINT).bev(0.4f);   // brass-lit pauldrons
    b.rod({ -3.4f, 2.6f, 8.4f }, { -3.9f, 3.2f, 18.4f }, 0.4f, C(0x5A4A2A)).m(METAL).bev(0);   // the staff
    b.rod({ -3.9f, 3.2f, 18.4f }, { -4.1f, 3.3f, 20.2f }, 0.17f, C(0xFFBE46)).m(EMIT).f(F_NOSHADOW);
    b.box(-3.6f, 3.25f, 16.6f, 0.16f, 1.7f, 1.5f, C(0x9A2230)).m(MATTE).bev(0.1f);         // crimson banner
    b.box(-3.78f, 3.25f, 16.6f, 0.08f, 1.25f, 0.3f, C(0xFFBE46)).m(EMIT).f(F_NOSHADOW);
    b.box(1.85f, 0, 13.7f, 0.4f, 1.45f, 0.4f, C(0xFFBE46)).m(EMIT).f(F_NOSHADOW);
    b.box(0.8f, 0, 15.6f, 1.0f, 1.3f, 0.35f, C(0x8A6E32)).m(METAL).bev(0.2f);
    coil(b, 5.2f, 10.72f, 12.4f, 10.9f, 4, 1.6f, 0.9f, C(0xFFBE46));
    b.rod({ 13.6f, 1.6f, 10.98f }, { 17.4f, 1.6f, 11.02f }, 0.5f, C(0xFFE0A0), 0.15f).m(EMIT);
    for (int i = 0; i < 3; i++) b.ball(-5.4f, -1.4f + i * 1.4f, 12.6f - i * 0.5f, 0.5f, 0.5f, 0.45f, pal::kBrass).m(METAL);
    b.cyl('x', 12.4f, 1.6f, 10.9f, 1.3f, 0.8f, C(0x2E2C2E)).m(METAL).bev(0.2f);
    trooper(b, p, f);
}
M3D_UNIT(U_K_CHOSEN, kChosen)

static void cShadow(Model& b, Model&, int f) {   // Shadow Guard: a black stealth suit, a short cowl, a four-role weapon
    Trooper p; p.suit = C(0x363E50); p.armour = C(0x1E222C); p.helmet = C(0x1A1E26); p.pack = C(0x2A2F3A); p.bulk = 0.95f;
    p.head = 2; p.light = pal::cGlow; p.gun = C(0x262A32); p.weapon = 0; p.boot = C(0x14161A);
    b.box(-2.1f, 0, 11.2f, 1.2f, 2.1f, 1.0f, C(0x2E3440)).m(MATTE).bev(0.35f);           // short cowl over the shoulders
    b.box(1.85f, 0, 13.7f, 0.35f, 1.3f, 0.35f, pal::cGlow).m(EMIT).f(F_NOSHADOW);
    b.cyl('x', 7.0f, 1.6f, 9.15f, 1.0f, 3.4f, C(0x22262E)).m(METAL).bev(0.2f);           // the fat launcher tube under the barrel
    b.cyl('x', 10.9f, 1.6f, 9.15f, 1.25f, 0.6f, C(0x2E343C)).m(METAL).bev(0.15f);        // its muzzle collar
    b.ball(11.7f, 1.6f, 9.15f, 0.45f, 0.45f, 0.45f, C(0x14161C)).m(RUBBER);
    scope(b, 5.0f, 1.6f, 10.7f, 0.9f, C(0x30353E), pal::cGlow);
    b.box(0.8f, -3.2f, 9.2f, 0.9f, 0.5f, 0.9f, C(0x262A32)).m(PAINT).bev(0.15f);         // stinger pod on the flank
    b.ball(1.3f, -5.0f, 8.6f, 0.55f, 0.55f, 0.45f, pal::cGlow).m(EMIT).f(F_NOSHADOW);
    trooper(b, p, f);
}
M3D_UNIT(U_C_SHADOW, cShadow)

static void kShadow(Model& b, Model&, int f) {
    Trooper p; p.suit = C(0x3C3E30); p.armour = C(0x22241A); p.helmet = C(0x1E2018); p.pack = C(0x2C2B28); p.bulk = 0.95f;
    p.head = 2; p.light = pal::kGlow; p.gun = C(0x28281E); p.weapon = 0; p.boot = C(0x16170F);
    b.box(-2.1f, 0, 11.2f, 1.2f, 2.1f, 1.0f, C(0x34362A)).m(MATTE).bev(0.35f);
    b.box(1.85f, 0, 13.7f, 0.35f, 1.3f, 0.35f, pal::kGlow).m(EMIT).f(F_NOSHADOW);
    b.cyl('x', 7.0f, 1.6f, 9.15f, 1.0f, 3.4f, C(0x2A2A20)).m(METAL).bev(0.2f);
    b.cyl('x', 10.9f, 1.6f, 9.15f, 1.25f, 0.6f, C(0x36362A)).m(METAL).bev(0.15f);
    b.ball(11.7f, 1.6f, 9.15f, 0.45f, 0.45f, 0.45f, C(0x16170F)).m(RUBBER);
    scope(b, 5.0f, 1.6f, 10.7f, 0.9f, C(0x33332A), pal::kGlow);
    b.box(0.8f, -3.2f, 9.2f, 0.9f, 0.5f, 0.9f, C(0x2A2A20)).m(PAINT).bev(0.15f);
    b.ball(1.3f, -5.0f, 8.6f, 0.55f, 0.55f, 0.45f, pal::kGlow).m(EMIT).f(F_NOSHADOW);
    b.rod({ -2.6f, 2.8f, 8.4f }, { -3.0f, 3.1f, 15.4f }, 0.16f, C(0x2A2A22)).m(METAL).f(F_NOSHADOW);   // a whip antenna
    trooper(b, p, f);
}
M3D_UNIT(U_K_SHADOW, kShadow)

}  // namespace

// ---------------------------------------------------------------- civilian rioters (no armour, no helmet, the army's colour on an armband)
// the two civilians wear no helmet and no army's armour: their clothing colours go where a soldier's plate would, and the army's colour is a
// band round one upper arm instead. bandX/bandY/bandZ: the band's centre on that arm; bandLen: how far along the arm it runs.
static void rioterBase(Model& b, Trooper& p, Color band, float bandX, float bandY, float bandZ, float bandLen, float bandR) {
    p.accent = band;                     // no owner-colour shoulder plates: the clothing colour instead
    p.armour = p.accent; p.helmet = p.accent;
    p.head = 2;
    for (int s = -1; s <= 1; s += 2) b.ball(0, s * 3.5f, 11.5f, 1.5f, 1.4f, 1.3f, band).m(MATTE);   // cloth shoulders
    b.cyl('y', bandX, bandY, bandZ, bandR, bandLen, Color{ 255, 255, 255, 254 }).m(TEAM).bev(0.45f);   // the army's armband
}

static void rioter(Model& b, Model&, int f) {   // Rioter: a red-brown hoodie, denim sleeves, a dark bandana, an old hunting rifle
    Trooper p; p.suit = C(0x42403C); p.accent = C(0x7C3E32); p.skin = C(0xBB9070); p.boot = C(0x2E2A26);
    p.pack = C(0x343C44); p.gun = C(0x80704A); p.weapon = 0; p.backpack = true;
    rioterBase(b, p, C(0x7C3E32), 1.9f, -3.9f, 10.2f, 1.15f, 1.5f);
    b.box(-3.5f, 0, 9.7f, 1.6f, 2.2f, 1.5f, C(0x343C44)).m(MATTE).bev(0.4f);             // rucksack
    b.rod({ -3.0f, 0, 8.3f }, { -3.0f, 0, 10.0f }, 0.55f, C(0x4A5058)).m(MATTE).bev(0);   // its rolled top
    b.box(1.6f, 0, 14.4f, 0.6f, 1.9f, 0.55f, C(0x2A2624)).m(MATTE).bev(0.2f);            // dark hair
    b.box(1.85f, 0, 13.1f, 0.4f, 1.65f, 0.5f, C(0x1C1E24)).m(MATTE).bev(0.15f);          // bandana across the face
    b.rod({ 3.6f, 1.5f, 10.55f }, { 10.4f, 1.5f, 10.6f }, 0.42f, C(0x32343A)).m(METAL).bev(0);   // the long barrel
    b.box(4.0f, 1.5f, 10.3f, 1.6f, 0.55f, 0.55f, C(0x6B5A3E)).m(MATTE).bev(0.2f);        // its wooden fore-end
    b.box(1.4f, 4.6f, 7.4f, 1.0f, 0.7f, 1.3f, C(0x46503A)).m(MATTE).bev(0.25f);          // a bottle in the hip pocket
    b.ball(1.4f, 4.6f, 8.9f, 0.4f, 0.4f, 0.5f, C(0x5E7A52)).m(GLASS);
    trooper(b, p, f);
}
M3D_UNIT(U_RIOTER, rioter)

static void firebomber(Model& b, Model&, int f) {   // Firebomber: a black hoodie, a canvas bag of bottles, a burning bottle in the hand
    Trooper p; p.suit = C(0x2E3038); p.accent = C(0x2A2A30); p.skin = C(0xB08668); p.boot = C(0x262424);
    p.pack = C(0x968355); p.gun = C(0x30323A); p.weapon = 3; p.backpack = true;
    rioterBase(b, p, C(0x2A2A30), 1.7f, -3.9f, 10.3f, 1.1f, 1.45f);
    b.box(-3.5f, 0, 9.6f, 1.6f, 2.3f, 1.5f, C(0x968355)).m(MATTE).bev(0.4f);             // canvas shoulder bag
    for (int i = 0; i < 3; i++) {                                                        // bottle necks poking out of it
        b.cyl('x', -5.5f, -1.5f + i * 1.5f, 10.9f, 0.45f, 0.8f, C(0x4A8054)).m(GLASS).bev(0.1f);
        b.ball(-6.4f, -1.5f + i * 1.5f, 10.9f, 0.45f, 0.45f, 0.45f, C(0x284A32)).m(GLASS);
    }
    b.box(1.6f, 0, 14.3f, 0.6f, 1.85f, 0.6f, C(0x36404E)).m(MATTE).bev(0.2f);            // knit cap
    b.cyl('z', 0.1f, 0, 14.75f, 1.45f, 0.22f, C(0x545A68)).m(MATTE).bev(0.15f);          // its ribbed cuff
    b.box(1.85f, 0, 13.1f, 0.4f, 1.65f, 0.5f, C(0x1E1E22)).m(MATTE).bev(0.15f);          // bandana
    // the near arm comes up across the body to hold the bottle out at shoulder height; the far one is drawn in at the chest
    b.rod({ 0.4f, 2.9f, 11.4f }, { 3.2f, 2.9f, 11.5f }, 0.85f, C(0x2E3038)).m(MATTE).bev(0);   // upper arm
    b.rod({ 3.2f, 2.9f, 11.5f }, { 5.4f, 2.9f, 11.7f }, 0.8f, C(0x2E3038)).m(MATTE).bev(0);    // forearm
    b.ball(5.5f, 2.9f, 11.75f, 0.9f, 0.9f, 0.85f, C(0x1C1C20)).m(MATTE);                 // the gloved fist
    b.rod({ 0.4f, -2.9f, 11.4f }, { 2.6f, -2.4f, 9.6f }, 0.85f, C(0x2E3038)).m(MATTE).bev(0);  // the other arm at the chest
    // the bottle: a fat glass body held point-up in the fist, a neck, the rag, and the flame standing off the end
    b.ball(5.7f, 2.9f, 12.4f, 0.95f, 0.95f, 1.15f, C(0x4A8054)).m(GLASS);
    b.cyl('z', 5.7f, 2.9f, 13.9f, 0.42f, 0.55f, C(0x609A68)).m(GLASS).bev(0.1f);
    b.ball(5.7f, 2.9f, 14.5f, 0.45f, 0.45f, 0.4f, C(0xE4D6B0)).m(MATTE);                // the rag stuffed in its mouth
    b.ball(5.7f, 2.9f, 15.2f, 0.75f, 0.75f, 0.95f, C(0xFFA22E)).m(EMIT).f(F_NOSHADOW);   // the flame
    b.ball(5.7f, 2.9f, 16.0f, 0.42f, 0.42f, 0.6f, C(0xFFF2C0)).m(EMIT).f(F_NOSHADOW);
    trooper(b, p, f);
}
M3D_UNIT(U_FIREBOMBER, firebomber)