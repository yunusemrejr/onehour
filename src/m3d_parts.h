// One Hour - shared model parts (see m3d.h): the two armies' palettes, track units and the parametric soldier.
#pragma once
#include "m3d_reg.h"

namespace m3d {

namespace pal {
// Cyber
const Color cHull = C(0xD5DCE4), cHull2 = C(0xAEB9C6), cDark = C(0x2A2F3A), cMid = C(0x5E6C80), cGlow = C(0x58ECFF), cGlass = C(0x123A4C);
// Clanker
const Color kHull = C(0x6C7144), kHull2 = C(0x474D30), kDark = C(0x2C2B28), kSteel = C(0x55585A), kRust = C(0x8A4A2B), kGlow = C(0xFFB23E), kGlass = C(0x2A3A34), kBrass = C(0xB9953F);
const Color track = C(0x26262A), rubber = C(0x1D1D20);
// structures. Cyber: pale ceramic and cool concrete, deep cobalt trim, glass, cyan light. Clanker: olive plate, corrugated khaki roofs, brick, rust, hazard paint, warm light.
const Color cPave = C(0x414C5A), cWall = C(0xC6D0DB), cWall2 = C(0x98A7B8), cRoof = C(0xE4EAF0), cTrim = C(0x2C5AA0), cSteel = C(0x8E9AA8), cWin = C(0x1B4E66);
const Color kPave = C(0x4E4838), kWall = C(0x768040), kWall2 = C(0x545C30), kRoof = C(0xA58E5C), kBrick = C(0x8F5A41), kConc = C(0x8B877C), kHaz = C(0xE6BA3C), kWin = C(0x3A3424), kLit = C(0xFFD678);
}

// a pair of track units: long sloped-end pods with link texture
inline void tracks(Model& m, float len, float halfGap, float width, float height, Color skirt, bool skirts) {
    for (int s = -1; s <= 1; s += 2) {
        const float y = s * (halfGap + width * 0.5f), l = len * 0.5f;
        m.profileY({ { -l, height * 0.45f }, { -l + height * 0.5f, height }, { l - height * 0.5f, height }, { l, height * 0.45f }, { l - height * 0.55f, 0 }, { -l + height * 0.55f, 0 } },
                   y - width * 0.5f, y + width * 0.5f, pal::track).m(RUBBER).t(T_TREAD, 2.2f).bev(0.5f);
        // road wheels showing under the skirt
        for (float x = -l + height * 0.9f; x <= l - height * 0.8f; x += height * 0.95f)
            m.cyl('y', x, y + s * width * 0.32f, height * 0.42f, height * 0.36f, width * 0.22f, C(0x3A3B3E)).m(METAL).bev(0.3f);
        if (skirts) m.box(0, y + s * width * 0.2f, height * 0.86f, l * 0.86f, width * 0.36f, height * 0.2f, skirt).t(T_PANEL, 6).bev(0.4f);
    }
}

// ---------------------------------------------------------------- infantry
// One parametric soldier serves every foot unit: build, colours, headgear, pack and weapon. frame 1 / 2: the two phases of the stride.
static const float TROOPER_SIZE = 1.7f;   // foot soldiers are drawn larger than life next to vehicles, as in every game of this kind, or they would be specks
struct Trooper {
    Color suit, armour, skin = C(0xC79A72), helmet, boot = C(0x24231F), pack, gun = C(0x1E2024);
    float bulk = 1.0f;        // 1 = line infantry, 1.25 = heavy armour
    int head = 0;             // 0 helmet, 1 visor helmet (a glowing band), 2 hood / cap, 3 gas mask
    int weapon = 0;           // 0 rifle, 1 launcher tube on the shoulder, 2 long rifle, 3 none (medic / spy), 4 heavy gun at the hip
    Color light = C(0, 0);    // visor / weapon light (alpha 0: none)
    bool backpack = true;
    Color accent = OWNER;     // shoulder plates and helmet band
};
inline void trooper(Model& m, const Trooper& p, int frame) {
    const float b = p.bulk;
    const float st = frame == 1 ? 1.0f : (frame == 2 ? -1.0f : 0.0f);   // stride
    // legs (a stride swings them fore and aft)
    for (int s = -1; s <= 1; s += 2) {
        float fx = st * s * 2.3f;
        m.rod({ 0, s * 1.5f * b, 7.2f }, { fx, s * 1.6f * b, 1.2f }, 1.25f * b, p.suit).m(MATTE).bev(0);
        m.box(fx + 0.7f, s * 1.6f * b, 0.8f, 1.7f, 1.1f * b, 0.8f, p.boot).m(MATTE).bev(0.2f);
    }
    // torso: armour vest over the suit
    m.taper(0, 0, 6.6f, 1.9f * b, 2.9f * b, 5.4f, 2.2f * b, 3.4f * b, p.armour).m(PAINT).bev(0.6f);
    m.box(0, 0, 6.9f, 1.8f * b, 2.7f * b, 0.6f, p.boot).m(MATTE).bev(0.2f);                 // belt
    for (int s = -1; s <= 1; s += 2) m.ball(0, s * 3.5f * b, 11.5f, 1.5f * b, 1.4f * b, 1.3f * b, p.accent);   // shoulder plates: the owner's colour
    if (p.backpack) m.box(-2.6f * b, 0, 9.6f, 1.2f * b, 2.3f * b, 2.4f, p.pack).m(MATTE).bev(0.5f);
    // head
    m.ball(0.2f, 0, 13.6f, 1.6f, 1.6f, 1.7f, p.skin).m(MATTE);
    if (p.head == 0 || p.head == 1 || p.head == 3) {
        m.dome(0, 0, 13.9f, 2.15f, 2.15f, 2.0f, p.helmet).m(PAINT);
        if (p.head == 1 && p.light.a) m.box(1.75f, 0, 13.7f, 0.5f, 1.3f, 0.45f, p.light).m(EMIT);
        if (p.head == 3) { m.ball(1.7f, 0, 13.2f, 0.9f, 1.1f, 1.0f, C(0x2B2E2A)).m(RUBBER); m.cyl('x', 2.4f, 0.8f, 12.6f, 0.55f, 0.6f, C(0x5A5D52)).m(METAL); }
    } else {
        m.ball(-0.2f, 0, 14.0f, 2.0f, 1.95f, 1.9f, p.helmet).m(MATTE);                       // hood
    }
    // arms and weapon
    const float ay = 3.5f * b;
    if (p.weapon == 0 || p.weapon == 2 || p.weapon == 4) {
        const float len = p.weapon == 2 ? 12.5f : (p.weapon == 4 ? 9.5f : 8.5f), gz = p.weapon == 4 ? 8.6f : 10.6f, gy = p.weapon == 4 ? 2.6f : 1.6f;
        m.rod({ 0, ay, 11.2f }, { 3.0f, gy + 0.4f, gz }, 0.95f * b, p.suit).m(MATTE).bev(0);       // right arm to the grip
        m.rod({ 0, -ay, 11.2f }, { 5.2f, gy - 0.6f, gz + 0.1f }, 0.95f * b, p.suit).m(MATTE).bev(0);   // left arm to the fore-end
        m.rod({ 0.5f, gy, gz }, { 0.5f + len, gy, gz + 0.3f }, p.weapon == 4 ? 1.0f : 0.6f, p.gun).m(METAL).bev(0);
        m.box(1.6f, gy, gz - 0.2f, 1.6f, 0.6f, 0.9f, p.gun).m(MATTE).bev(0.2f);
        if (p.light.a && p.weapon != 0) m.ball(0.9f + len, gy, gz + 0.3f, 0.75f, 0.75f, 0.75f, p.light).m(EMIT);
    } else if (p.weapon == 1) {
        m.rod({ 0, ay, 11.2f }, { 2.6f, 3.2f, 12.6f }, 0.95f * b, p.suit).m(MATTE).bev(0);
        m.rod({ 0, -ay, 11.2f }, { 3.0f, -1.0f, 11.0f }, 0.95f * b, p.suit).m(MATTE).bev(0);
        m.rod({ -5.5f, 3.0f, 13.4f }, { 6.5f, 3.0f, 13.8f }, 1.25f, p.gun).m(PAINT).bev(0.2f);       // the tube on the shoulder
        m.cyl('x', 6.6f, 3.0f, 13.8f, 1.6f, 0.6f, C(0x3C4033)).bev(0.2f);
    } else {
        for (int s = -1; s <= 1; s += 2) m.rod({ 0, s * ay, 11.2f }, { 0.8f + st * s * -1.4f, s * (ay + 0.3f), 6.6f }, 0.95f * b, p.suit).m(MATTE).bev(0);
    }
    m.scale(TROOPER_SIZE);
}


}  // namespace m3d
