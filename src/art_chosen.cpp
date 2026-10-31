#include "art2.h"
#include "paint.h"

using namespace paint;

// The Chosen (one elite operative per army) and the Shadow Guard (the Chosen's stealth squads, 30 at a time). Same conventions as the troopers in
// art.cpp and the laser commando in art_commando.cpp: 64x64 canvas, facing +x, body centred at (32,32), seen from straight above and lit from the
// north-west, with the walk cycle in the boots. The Chosen is a soldier with a round hood, broad shoulders, a short cape draping back from them and a
// slim long rifle held forward in both hands; the Shadow Guard is a slim matte stealth suit with a short cowl and a compact multi-role rifle (scope on
// top, a fat launcher tube under the barrel).
namespace {

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

// the Chosen's cape: a short cloak that drapes back from the shoulders about 10 px, a little wider at the hem than where it leaves the shoulders,
// with a lining showing round its rim (silver-blue for the Cyber, crimson for the Clanker) and three folds
void cape(Art& A, bool cy) {
    Color lining = cy ? rgb(150, 166, 190) : rgb(158, 26, 38);
    Color outer = cy ? rgb(206, 218, 232) : rgb(34, 32, 38);
    Color seam = cy ? rgb(110, 232, 255) : rgb(204, 156, 70);       // cyan light seams, or brass trim
    Color fold = cy ? rgb(132, 150, 176) : rgb(64, 60, 68);         // the shaded folds between them
    // a rounded cape: the hem (the back edge) is its widest part, and its front is hidden under the shoulders and torso
    A.ell(26.4f, 32, 7.2f, 12.0f, lining, 1.6f, 0.4f);
    A.ell(26.6f, 32, 6.2f, 11.0f, outer, 2.6f, 0.55f);
    A.line(29.0f, 27.2f, 22.6f, 25.8f, 0.6f, rgba(seam, 200));       // two bright seams, the outer folds
    A.line(29.0f, 36.8f, 22.6f, 38.2f, 0.6f, rgba(seam, 200));
    A.line(29.5f, 32.0f, 21.6f, 32.0f, 0.6f, rgba(fold, 150));       // and a shaded centre fold
    for (int i = 0; i < 8; i++) {                                     // the hem trim, along the back of the ellipse
        float a0 = 1.9f + i * 0.31f, a1 = a0 + 0.31f;
        A.line(26.6f + 6.2f * std::cos(a0), 32 + 11.0f * std::sin(a0), 26.6f + 6.2f * std::cos(a1), 32 + 11.0f * std::sin(a1), 0.6f, rgba(seam, 210));
    }
}

void chosen(Art& A, bool cy, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 5.6f : -5.6f);     // the stride of the boots, as the laser commando's
    Color halo = cy ? rgb(110, 232, 255) : rgb(255, 160, 60);
    Color torso = cy ? rgb(118, 136, 162) : rgb(62, 58, 66);       // the armour under the cape: blue-grey, or charcoal (so the crimson lining reads)
    Color pauldron = cy ? rgb(150, 166, 190) : rgb(74, 70, 80);    // a shade off the cape, so the shoulders read against it
    Color sleeve = cy ? rgb(168, 182, 202) : rgb(44, 42, 48);
    Color dark = cy ? rgb(40, 48, 66) : rgb(64, 56, 60);           // (the Clanker's boots are a shade up from its black cape, so the stride reads)
    Color hood = cy ? rgb(226, 234, 244) : rgb(30, 28, 34);
    Color hoodRim = cy ? rgb(150, 166, 190) : rgb(158, 26, 38);
    Color gun = cy ? rgb(60, 70, 90) : rgb(44, 42, 46), gunHi = cy ? rgb(168, 180, 198) : rgb(102, 98, 102);
    Color glint = cy ? rgb(110, 232, 255) : rgb(255, 190, 70);     // the visor glint and the rifle's light: cyan or amber
    Color brass = rgb(204, 156, 70);
    // a faint faction halo behind the figure, so it reads as unique at a glance
    A.glow(32, 32, 14, rgba(halo, 60));
    // the cape, then boots that step out beyond its front edge beside the hips
    cape(A, cy);
    A.ell(32 + step, 27.8f, 3.4f, 2.5f, shade(dark, 1.25f), 1.4f, 0.4f);
    A.ell(32 - step, 36.2f, 3.4f, 2.5f, shade(dark, 1.25f), 1.4f, 0.4f);
    // the torso: a broad cuirass with a light strip (Cyber) or a brass chest trim (Clanker)
    A.ell(32.4f, 32, 6.2f, 9.4f, torso, 3.0f, 0.6f);
    A.line(35.6f, 28.4f, 35.6f, 35.6f, 0.7f, rgba(cy ? glint : brass, 210));
    // broad shoulders: a pauldron each, about 10 px either side; the team roundel on the near (north) one
    for (int s = -1; s <= 1; s += 2) A.ell(31.4f, 32 + s * 8.2f, 3.8f, 2.5f, pauldron, 1.6f, 0.55f);
    teamDot(A, 31.4f, 32 - 8.2f, 1.7f);
    // the slim long rifle, held forward along +x: the stock at the shoulder, a receiver, a barrel with a light coil and a muzzle emitter
    A.cap(29.0f, 32, 37.0f, 32, 1.3f, gun, 1.0f, 0.5f);
    A.box(41.0f, 32, 3.4f, 1.4f, 0.5f, gun, 1.0f, 0.5f);
    A.cap(44.0f, 32, 61.0f, 32, 0.8f, gunHi, 0.9f, 0.5f);
    for (float x : {48.4f, 51.0f, 53.6f, 56.2f}) A.box(x, 32, 0.6f, 1.5f, 0.2f, shade(glint, 0.9f), 0.7f, 0.3f);
    A.line(44.5f, 31.1f, 58.0f, 31.1f, 0.6f, rgba(glint, 200));
    A.circle(61.6f, 32, 1.7f, glint, 1.0f, 0.3f); A.glow(62.4f, 32, 5.5f, rgba(glint, 170));
    // both arms over the torso and the rifle: the near arm to the grip at x=40, the far arm to the fore-grip at x=45
    A.cap(33.0f, 32 - 8.2f, 40.0f, 31.2f, 1.9f, sleeve, 1.3f, 0.5f);
    A.cap(33.0f, 32 + 8.2f, 45.0f, 32.8f, 1.9f, sleeve, 1.3f, 0.5f);
    A.dot(40.0f, 31.2f, 1.1f, shade(dark, 1.2f));
    A.dot(45.0f, 32.8f, 1.0f, shade(dark, 1.2f));
    // the round hood over the head at x=33: a darker rim, a dark visor across its front and a visor glint
    A.circle(33.0f, 32, 5.1f, hoodRim, 1.6f, 0.5f);
    A.circle(33.0f, 32, 4.5f, hood, 2.2f, 0.6f);
    A.box(36.6f, 32, 0.9f, 2.0f, 0.4f, rgb(22, 26, 34), 0.8f, 0.3f);
    A.dot(37.4f, 32, 0.5f, glint);
    A.glow(37.6f, 32, 3.2f, rgba(glint, 110));
}

void shadow(Art& A, bool cy, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 5.6f : -5.6f);
    Color suit = cy ? rgb(54, 62, 80) : rgb(60, 62, 46);            // matte stealth suit: blue-charcoal or olive-charcoal
    Color sleeve = shade(suit, 0.84f);
    Color plates = cy ? rgb(30, 34, 44) : rgb(32, 33, 24);          // darker armour plates
    Color cowl = shade(suit, 1.12f);                                // a shade lighter than the suit, so the cowl lifts off the shoulders
    Color accent = cy ? rgb(110, 232, 255) : rgb(255, 190, 70);     // the thin faction strips
    Color gun = cy ? rgb(38, 42, 50) : rgb(40, 40, 34), gunHi = cy ? rgb(120, 132, 150) : rgb(98, 98, 84);
    // boots, alternating in the walk cycle
    A.ell(32 + step, 27.8f, 3.0f, 2.2f, shade(plates, 1.4f), 1.2f, 0.4f);
    A.ell(32 - step, 36.2f, 3.0f, 2.2f, shade(plates, 1.4f), 1.2f, 0.4f);
    // a slim pack
    A.box(25.8f, 32, 3.0f, 5.0f, 1.2f, plates, 1.6f, 0.5f);
    // the slim torso and a chest plate with its accent strip
    A.ell(32.4f, 32, 5.2f, 7.4f, suit, 2.6f, 0.6f);
    A.box(34.6f, 32, 1.6f, 4.4f, 0.7f, plates, 1.2f, 0.5f);
    A.line(36.0f, 28.8f, 36.0f, 35.2f, 0.5f, rgba(accent, 210));
    // shoulder plates (dark), with a team roundel on the north one
    for (int s = -1; s <= 1; s += 2) A.ell(30.6f, 32 + s * 6.0f, 3.0f, 2.0f, plates, 1.3f, 0.5f);
    teamDot(A, 30.6f, 26.0f, 1.4f);
    A.line(34.0f, 26.2f, 40.0f, 29.6f, 0.45f, rgba(accent, 200));   // accent strip along each sleeve
    A.line(34.0f, 37.8f, 40.0f, 34.4f, 0.45f, rgba(accent, 200));
    // the rifle: a stock under the cowl, a receiver, a slim barrel, a scope on top, and a fat launcher tube under the barrel
    A.cap(30.5f, 32, 37.5f, 32, 1.4f, plates, 1.1f, 0.5f);
    A.box(41.2f, 32, 3.8f, 1.4f, 0.5f, gun, 1.0f, 0.5f);
    A.cap(44.5f, 32, 55.0f, 32, 0.9f, gunHi, 0.9f, 0.5f);
    A.box(44.6f, 32, 2.6f, 1.5f, 0.6f, shade(gun, 1.1f), 1.3f, 0.5f);        // scope
    A.dot(46.6f, 32, 0.55f, accent); A.glow(46.8f, 32, 2.6f, rgba(accent, 90));
    A.cap(39.5f, 33.6f, 49.5f, 33.6f, 1.5f, gun, 1.3f, 0.5f);               // launcher tube under the barrel
    A.dot(49.5f, 33.6f, 0.9f, rgb(20, 22, 28));
    A.dot(55.6f, 32, 0.9f, accent);                                         // muzzle light
    // the arms, in the stealth sleeves, reaching to the grip
    A.cap(33.5f, 32 - 6.0f, 40.5f, 30.4f, 1.6f, sleeve, 1.3f, 0.5f);
    A.cap(33.5f, 32 + 6.0f, 42.8f, 33.6f, 1.6f, sleeve, 1.3f, 0.5f);
    // the head under a short cowl that runs back over the shoulders, with a dark visor and an accent glint
    A.ell(31.2f, 32, 4.4f, 4.0f, cowl, 1.6f, 0.6f);
    A.box(35.8f, 32, 0.8f, 2.2f, 0.4f, rgb(18, 22, 28), 0.8f, 0.3f);
    A.dot(36.6f, 32, 0.4f, accent);
}

}  // namespace

// The Chosen sprite for either army, painted onto c and finished like the other self-painting unit sprites: it sharpens and outlines itself.
void artChosenBody(Canvas& c, Color team, bool cyber, int frame) {
    Art A(c, team);
    chosen(A, cyber, frame);
    sharpen(c, 0.7f);
    c.outline(rgb(8, 10, 14, 200));
}

// The Shadow Guard sprite for either army, finished like the Chosen's.
void artShadowBody(Canvas& c, Color team, bool cyber, int frame) {
    Art A(c, team);
    shadow(A, cyber, frame);
    sharpen(c, 0.7f);
    c.outline(rgb(8, 10, 14, 200));
}
