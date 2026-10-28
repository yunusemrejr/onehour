#include "art2.h"
#include "paint.h"

using namespace paint;

// Laser commandos (Prism Commando for Cyber, Venom Commando for Clanker): elite infantry that never come out of a barracks, they only drop
// by parachute from a bunker's Para Backup. Same conventions as the troopers in art.cpp: 64x64 canvas, facing +x, body centred at (32,32),
// seen from straight above and lit from the north-west, with the walk cycle in the boots. They are bulkier than a trooper (broad shoulders,
// heavy pauldrons, a power pack), and carry a long "Toxic Lance" rifle whose coil and emitter glow toxic green.
// Cyber: pale ceramic plates and a white helmet with a cyan visor. Clanker: olive armour with rust plates and a gas-mask face.
namespace {

const Color TOXIC = rgb(150, 255, 80);   // the Toxic Lance's coil and emitter

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

void commando(Art& A, bool cy, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 5.6f : -5.6f);     // the stride of the boots (a trooper's is 3: under this bulk it shows a little more)
    float sw = 10.6f;                                              // shoulder half-width: a trooper's is 9.5
    Color armor = cy ? rgb(184, 198, 214) : rgb(104, 112, 72);     // Cyber: blue-grey ceramic under the white plates; Clanker: olive
    Color sleeve = shade(armor, 0.78f);
    Color plate = cy ? rgb(238, 244, 250) : rgb(146, 88, 48);       // pauldrons and chest plates: white, or rust
    Color dark = cy ? rgb(40, 48, 66) : rgb(36, 38, 30);
    Color pack = cy ? rgb(52, 62, 84) : rgb(74, 70, 46);
    Color helmet = cy ? rgb(138, 158, 186) : rgb(78, 84, 52);      // a helmet a shade off the armour, so the head reads from above
    Color gunC = cy ? rgb(204, 214, 226) : rgb(58, 60, 52), gunD = cy ? rgb(56, 66, 86) : rgb(36, 38, 32);
    Color stock = cy ? rgb(82, 94, 116) : rgb(122, 80, 46);
    Color accent = cy ? rgb(110, 232, 255) : rgb(206, 180, 70);      // cyan strips, or the hazard yellow of the Clanker's trim
    // boots, heavier than a trooper's and alternating in the walk cycle
    A.ell(32 + step, 27.8f, 4.0f, 2.8f, shade(dark, 1.25f), 1.5f, 0.4f);
    A.ell(32 - step, 36.2f, 4.0f, 2.8f, shade(dark, 1.25f), 1.5f, 0.4f);
    // heavy power pack on the back
    A.box(25.4f, 32, 4.8f, 7.8f, 1.8f, pack, 2.0f, 0.5f);
    if (cy) {
        A.line(23.0f, 27.4f, 23.0f, 36.6f, 1.1f, rgba(accent, 210));                      // a cyan vent strip down the pack
        A.dot(22.6f, 32, 1.1f, TOXIC); A.glow(22.6f, 32, 4.5f, rgba(TOXIC, 110));        // and its power light
    } else {
        A.circle(22.8f, 28.4f, 1.9f, rgb(150, 88, 48), 1.4f, 0.5f);                       // two rust canisters
        A.circle(22.8f, 35.6f, 1.9f, rgb(150, 88, 48), 1.4f, 0.5f);
    }
    // torso: a broad armoured cuirass
    A.ell(32.5f, 32, 6.8f, sw, armor, 3.6f, 0.6f);
    // chest plate with its rivets (Clanker) or a cyan light strip (Cyber)
    A.box(35.2f, 32, 2.6f, 6.6f, 1.4f, plate, 1.6f, 0.55f);
    if (cy) A.line(35.6f, 27.6f, 35.6f, 36.4f, 0.8f, rgba(accent, 190));
    else for (float yy : {27.6f, 36.4f}) A.dot(35.2f, yy, 0.55f, rgba(dark, 200));
    // hip pouches and a chest rig with its straps
    for (int s = -1; s <= 1; s += 2) {
        A.box(29.4f, 32 + s * 7.6f, 2.0f, 1.7f, 0.5f, shade(dark, 1.3f), 1.0f, 0.5f);
        A.line(30.4f, 32 + s * (sw - 2.6f), 36.4f, 32 - s * (sw - 2.6f), 0.9f, rgba(dark, 150));
    }
    // heavy pauldrons on both shoulders, each with a roundel: the team colour
    for (int s = -1; s <= 1; s += 2) {
        float py = 32 + s * (sw - 1.0f);
        A.ell(30.6f, py, 4.6f, 3.6f, plate, 2.2f, 0.6f);
        teamDot(A, 30.4f, py, 1.9f);
    }
    // arms and the Toxic Lance: the forearms reach forward to the grip, as the troopers' do
    float gx = 42;
    A.cap(33.5f, 32 - sw + 2.6f, gx, 32 - 2.4f, 2.4f, sleeve, 1.5f, 0.5f);
    A.cap(33.5f, 32 + sw - 2.6f, gx + 3, 32 + 1.4f, 2.4f, sleeve, 1.5f, 0.5f);
    A.dot(gx, 32 - 2.4f, 1.5f, shade(dark, 1.2f));
    // the rifle: a heavy stock and receiver, a long barrel with a scope and magazine, a glowing coil along the barrel and a toxic emitter at the muzzle
    A.cap(33, 32, 40, 32, 2.2f, stock, 1.3f, 0.5f);
    A.cap(39, 32, 46, 32, 1.9f, gunC, 1.2f, 0.5f);
    A.cap(45, 32, 60, 32, 1.1f, gunC, 1.0f, 0.5f);
    A.box(41.6f, 32 + 3.6f, 1.6f, 2.6f, 0.5f, gunD, 1.0f, 0.4f);                       // magazine
    A.box(44.0f, 32 - 2.7f, 3.4f, 1.2f, 0.5f, gunD, 1.0f, 0.5f);                       // scope
    A.dot(47.6f, 32 - 2.7f, 0.9f, cy ? rgb(110, 232, 255) : rgb(255, 214, 120));
    for (float x : {49.2f, 51.8f, 54.4f, 57.0f}) A.box(x, 32, 0.7f, 2.6f, 0.3f, shade(TOXIC, 0.85f), 0.8f, 0.3f);   // the coil
    A.line(46, 31.1f, 58, 31.1f, 0.7f, rgba(TOXIC, 200));
    A.circle(61.0f, 32, 2.3f, TOXIC, 1.2f, 0.3f); A.glow(61.8f, 32, 7.0f, rgba(TOXIC, 170));   // emitter
    // head: a heavy helmet; Cyber has a cyan visor, Clanker a gas mask with two eyepieces and a hose down to the chest
    A.circle(33.6f, 32, 5.4f, helmet, 3.0f, 0.7f);
    if (cy) {
        A.box(37.2f, 32, 1.7f, 3.0f, 0.8f, rgb(110, 232, 255), 1, 0.2f);
        A.glow(38.0f, 32, 4.0f, rgba(rgb(110, 232, 255), 90));
    } else {
        A.box(37.8f, 32, 2.2f, 3.6f, 1.1f, rgb(40, 42, 34), 1.4f, 0.5f);                   // the mask's snout
        A.dot(37.4f, 29.4f, 1.0f, rgb(120, 140, 110)); A.dot(37.4f, 34.6f, 1.0f, rgb(120, 140, 110));   // eyepieces
        A.line(38.4f, 35.6f, 35.2f, 38.6f, 0.9f, rgba(rgb(30, 32, 26), 230));             // the breathing hose
        A.grain(8, 14, 56, 50, 0.14f, 33);
    }
    teamDot(A, 31.4f, 32, 2.0f);   // a roundel on the helmet crest, as the troopers have
}

}  // namespace

// Laser commando sprite for either army, painted onto c and finished like the other self-painting unit sprites: it sharpens and outlines itself
// (the generic unit layers are off it).
void artCommandoBody(Canvas& c, Color team, bool cyber, int frame) {
    Art A(c, team);
    commando(A, cyber, frame);
    sharpen(c, 0.7f);
    c.outline(rgb(8, 10, 14, 200));
}
