#include "art2.h"
#include "paint.h"

using namespace paint;

// Civilian rioters: the street crowd that is no army's soldier. Same conventions as the infantry in art.cpp: 64x64 canvas, facing +x, body
// centred at (32,32), seen from straight above and lit from the north-west, with the walk cycle in the boots. Nothing here is armour, a helmet
// or an emitter: they wear hoodies, jackets, jeans and trainers, and carry whatever they had to hand.
// Kind 0 "Rioter": a dull red-brown hoodie with faded denim sleeves, bare dark hair and a dark bandana across the face, a small rucksack, and an
// old hunting rifle with a battered wooden stock held forward.
// Kind 1 "Firebomber": a black hoodie, a canvas shoulder bag with bottle necks poking out at the back, and one arm raised forward holding a
// bottle with a burning rag. No firearm.
namespace {

// The player's roundel, as art.cpp paints it (its teamDot is file-static there, so the four lines are copied here).
void riotDot(Art& A, float cx, float cy, float r) {
    A.circle(cx, cy, r + 0.9f, rgba(rgb(18, 20, 24), 225), 0.6f, 0.15f);
    A.circle(cx, cy, r, shade(A.team, 0.94f), 0.8f, 0.22f);
    A.ring(cx, cy, r * 0.66f, r * 0.24f, rgba(mix(A.team, rgb(255, 255, 255), 0.42f), 210), 0.4f, 0);
    A.dot(cx, cy, r * 0.28f, rgba(shade(A.team, 0.7f), 220));
}

void rioter(Art& A, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 3.0f : -3.0f);
    float sw = 9.0f;                                  // shoulder half-width
    Color hoodie = rgb(124, 62, 50), hood = shade(hoodie, 0.78f);   // dull red-brown
    Color denim = rgb(66, 88, 118);                   // faded jeans-blue sleeves
    Color shoe = rgb(46, 40, 36), pack = rgb(52, 60, 68), hair = rgb(34, 28, 26);
    Color wood = rgb(128, 84, 46), steel = rgb(54, 56, 60), band = rgb(28, 30, 36);
    // trainers, alternating in the walk cycle
    A.ell(32 + step, 28, 3.6f, 2.4f, shade(shoe, 1.2f), 1.4f, 0.4f);
    A.ell(32 - step, 36, 3.6f, 2.4f, shade(shoe, 1.2f), 1.4f, 0.4f);
    // small rucksack on the back, with its top strap
    A.box(25.4f, 32, 3.2f, 5.2f, 1.6f, pack, 1.8f, 0.5f);
    A.line(25.4f, 27.6f, 25.4f, 36.4f, 0.8f, rgba(shade(pack, 1.7f), 150));
    // hoodie torso, the hood folded down at the back of the neck, a front zip
    A.ell(32.5f, 32, 6.0f, sw, hoodie, 3.0f, 0.55f);
    A.ell(28.6f, 32, 2.0f, 4.4f, hood, 1.4f, 0.4f);
    A.line(31.2f, 25.5f, 31.2f, 38.5f, 0.9f, rgba(shade(hoodie, 0.55f), 170));
    A.box(36.0f, 32, 1.6f, 3.2f, 0.5f, shade(hoodie, 0.86f), 1.0f, 0.3f);   // front pouch pocket
    // denim sleeves forward to the hands on the rifle's fore-end and grip
    Color sleeve = denim;
    A.cap(33.5f, 32 - sw + 2.4f, 41, 29.8f, 2.1f, sleeve, 1.4f, 0.5f);
    A.cap(33.5f, 32 + sw - 2.4f, 44, 33.2f, 2.1f, sleeve, 1.4f, 0.5f);
    // old hunting rifle: wooden stock and fore-end with a dent, a long dark barrel, a receiver block, and a muzzle
    A.cap(36, 32, 42, 32, 1.5f, wood, 1.0f, 0.5f);
    A.dot(40.2f, 31.1f, 0.35f, shade(wood, 0.55f));
    A.cap(38, 32, 51, 32, 0.9f, steel, 1.0f, 0.4f);
    A.box(43.2f, 32, 1.1f, 1.5f, 0.5f, shade(steel, 1.3f), 1.0f, 0.3f);
    A.dot(51.3f, 32, 0.9f, rgb(30, 30, 34));
    // head: bare dark hair with a cowlick, and a dark bandana across the face side
    A.circle(33.5f, 32, 4.6f, hair, 3.0f, 0.65f);
    A.dot(32.4f, 30.6f, 1.1f, rgba(rgb(92, 78, 70), 170));
    A.box(36.8f, 32, 1.1f, 3.6f, 0.5f, band, 0.8f, 0.3f);
    A.dot(36.8f, 29.2f, 0.35f, rgba(rgb(120, 110, 104), 200)); A.dot(36.8f, 34.8f, 0.35f, rgba(rgb(120, 110, 104), 200));
    riotDot(A, 31, 25, 1.9f);   // the roundel on the shoulder
}

void firebomber(Art& A, int frame) {
    float step = frame == 0 ? 0 : (frame == 1 ? 3.0f : -3.0f);
    float sw = 9.0f;
    Color hoodie = rgb(42, 42, 48), hood = shade(hoodie, 0.8f), sleeve = rgb(60, 60, 68);
    Color shoe = rgb(36, 34, 34), canvas = rgb(150, 120, 78), glove = rgb(28, 28, 32);
    Color glass = rgb(74, 128, 84), glassD = rgb(40, 74, 50);
    // trainers, alternating in the walk cycle
    A.ell(32 + step, 28, 3.6f, 2.4f, shade(shoe, 1.3f), 1.4f, 0.4f);
    A.ell(32 - step, 36, 3.6f, 2.4f, shade(shoe, 1.3f), 1.4f, 0.4f);
    // canvas shoulder bag on the back with three bottle necks poking out of its top
    A.box(25.8f, 32, 2.6f, 5.4f, 1.2f, canvas, 1.6f, 0.5f);
    A.line(25.8f, 27.0f, 25.8f, 37.0f, 0.7f, rgba(shade(canvas, 0.6f), 170));
    A.cap(24.0f, 29.4f, 22.2f, 29.0f, 0.9f, glass, 0.7f, 0.3f); A.dot(21.8f, 29.0f, 0.9f, glassD);
    A.cap(24.0f, 32.0f, 21.8f, 32.0f, 0.9f, glass, 0.7f, 0.3f); A.dot(21.6f, 32.0f, 0.9f, glassD);
    A.cap(24.0f, 34.6f, 22.2f, 35.0f, 0.9f, glass, 0.7f, 0.3f); A.dot(21.8f, 35.0f, 0.9f, glassD);
    // black hoodie torso with the hood at the neck
    A.ell(32.5f, 32, 6.0f, sw, hoodie, 3.0f, 0.55f);
    A.ell(28.6f, 32, 2.0f, 4.4f, hood, 1.4f, 0.4f);
    A.line(31.2f, 25.5f, 31.2f, 38.5f, 0.9f, rgba(rgb(18, 18, 22), 170));
    // left arm down and bent at the chest, gloved hand
    A.cap(33.5f, 32 - sw + 2.4f, 38.4f, 28.6f, 2.0f, sleeve, 1.4f, 0.5f);
    A.dot(38.6f, 28.6f, 1.2f, glove);
    // right arm raised forward, the hand holding the bottle
    A.cap(33.5f, 32 + sw - 2.4f, 41.4f, 34.0f, 2.0f, sleeve, 1.4f, 0.5f);
    A.dot(41.6f, 34.0f, 1.3f, glove);
    // the bottle: a short glass body, a neck, and a cream rag stuffed in the mouth, with the flame at its tip
    A.cap(41.4f, 34.0f, 44.0f, 34.0f, 1.5f, glass, 1.0f, 0.4f);
    A.cap(44.0f, 34.0f, 46.2f, 34.0f, 0.7f, rgb(96, 150, 104), 0.6f, 0.3f);
    A.dot(46.4f, 34.0f, 0.8f, rgb(228, 214, 176));
    A.glow(47.4f, 33.6f, 5.0f, rgba(rgb(255, 128, 36), 150));
    A.dot(47.8f, 33.4f, 1.6f, rgb(255, 178, 52));
    A.dot(48.2f, 33.0f, 0.9f, rgb(255, 246, 196));
    // head: a dark knit cap with a ribbed cuff, and a black bandana across the face side
    A.circle(33.5f, 32, 4.6f, rgb(54, 58, 70), 3.0f, 0.65f);
    A.ring(33.5f, 32, 3.9f, 1.0f, rgb(84, 90, 104), 0.8f, 0.2f);
    A.box(36.8f, 32, 1.1f, 3.6f, 0.5f, rgb(30, 30, 36), 0.8f, 0.3f);
    riotDot(A, 31, 39, 1.9f);   // the roundel on the shoulder
}

}  // namespace

// Civilian rioter sprite for kind 0 (Rioter) or 1 (Firebomber), painted onto c with the team roundel. Finishes with its own outline (the unit
// painter's enhance pass is not applied to these).
void artRioterBody(Canvas& c, Color team, int kind, int frame) {
    Art A(c, team);
    if (kind == 1) firebomber(A, frame);
    else rioter(A, frame);
    c.outline(rgb(8, 10, 14, 200));
}
