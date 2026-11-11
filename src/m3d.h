// One Hour - the model kit. Every unit, structure, tree and prop is a small 3D model built from a handful of solids (boxes with real chamfers,
// tapered hulls, extruded profiles, cylinders, cones, domes) with materials, and is rendered ONCE, when the game starts, into ordinary sprites:
// a planometric oblique view (the ground plan stays undistorted, heights rise up the screen by OBL_K), one sun, soft shadows, ambient occlusion,
// a sky reflection on metal and glass, edge wear and grime. Things that turn get one sprite per heading, so a hull's faces light and shade as it
// turns. Nothing here runs per frame: the game still only blits sprites.
#pragma once
#include "gfx.h"

namespace m3d {

static const float OBL_K = 0.70f;        // screen pixels a point rises per unit of height
static const float SUN[3] = { -0.60f, 0.25f, 0.76f };   // toward the sun (world: x east, y south, z up): low in the west, a little in front

enum Mat : u8 {
    PAINT = 0,   // painted steel: a soft sheen, wear on the edges
    METAL,       // bare metal: mirrors the sky, strong highlights
    RUBBER,      // tyres, tracks, hoses: matte, dark
    GLASS,       // canopies, windows, lenses: mirrors the sky, a hard glint
    TEAM,        // the owner's colour (drawn through the sprite's mask, so one render serves every army)
    EMIT,        // a light of its own: never shaded, with a small halo
    MATTE,       // concrete, cloth, plaster, earth, wood: no sheen
    FOLIAGE      // leaves: soft, light passes through the edges
};
enum Tex : u8 {
    T_NONE = 0,
    T_PANEL,     // plate seams every texS units
    T_TREAD,     // track links along the local x axis
    T_HAZARD,    // diagonal stripes of col and col2
    T_WINDOWS,   // rows of windows (col2) on the upright faces
    T_RIBS,      // corrugation along local x (roofs, containers, radiators)
    T_BRICK,     // courses of brick / block on upright faces
    T_PLANKS,    // boards along local x
    T_CAMO,      // blotches of col and col2
    T_TILES,     // roof tiles
    T_GRILLE,    // dark slots (vents, intakes)
    T_STRIPE,    // one band of col2 along the solid at local y = 0 (width texS)
    T_BAND,      // one band of col2 across the solid at local x = 0 (width texS)
    T_NOISE      // mottled col / col2 (rock, bark, canopy)
};
enum Flag : u8 { F_NOSHADOW = 1,   // casts no shadow (thin aerials, lights)
                 F_SHADOWONLY = 2  // not drawn: only shades what is under it (a stand-in for a part that is a separate sprite, like a turret)
};

struct V3 { float x, y, z; };
inline V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
inline float dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline V3 norm(V3 a) { float l = std::sqrt(dot(a, a)); return l > 1e-9f ? a * (1 / l) : V3{ 0, 0, 1 }; }
inline Color C(u32 hex, int a = 255) { return Color{ (u8)(hex >> 16), (u8)(hex >> 8), (u8)hex, (u8)a }; }
static const Color OWNER = Color{ 255, 255, 255, 254 };   // "the owner's colour": use it as a solid's colour or as a pattern's second colour

struct Plane { float nx, ny, nz, d; };   // local space: inside where n.p <= d

struct Prim {
    u8 kind = 0;            // 0 convex solid (planes), 1 frustum about local z, 2 ellipsoid
    u8 mat = PAINT, tex = T_NONE, flags = 0;
    Color col{ 200, 200, 200, 255 }, col2{ 40, 40, 40, 255 };
    float texS = 4, bevel = 0.7f;
    V3 c{ 0, 0, 0 };        // centre, model space
    float R[9] = { 1, 0, 0, 0, 1, 0, 0, 0, 1 };   // local axes as rows: local = R * (p - c)
    V3 h{ 1, 1, 1 };        // half sizes along the local axes (frustum: base radii and half height; ellipsoid: radii)
    float top = 1;          // frustum: radius at the top / radius at the base (0 = a cone)
    int p0 = 0, pn = 0;     // convex: its planes in Model::planes
    float rad = 1;          // bounding radius
    // modifiers (chain them on the solid just made)
    Prim& m(Mat mm) { mat = mm; return *this; }
    Prim& t(Tex tt, float s = 4, Color c2 = Color{ 40, 40, 40, 255 }) { tex = tt; texS = s; col2 = c2; return *this; }
    Prim& bev(float b) { bevel = b; return *this; }
    Prim& f(u8 fl) { flags |= fl; return *this; }
    Prim& rz(float deg);    // turn about its own centre: about the model's z (heading), y (pitch) or x (roll)
    Prim& ry(float deg);
    Prim& rx(float deg);
};

struct P2 { float a, b; };

struct Model {
    std::vector<Prim> prims;
    std::vector<Plane> planes;
    bool ground = true;     // stands on the ground: throws its shadow on it (aircraft and turrets do not)
    float dirt = 0.0f;      // dust and mud on what is near the ground (vehicles)
    float hub = 0.0f;       // helicopters: the height of the rotor hub (the game spins the blades there)
    // --- the frame new solids are placed in (an assembly: a turret, a wing, a tower on a roof) ---
    V3 org{ 0, 0, 0 }; float fz = 0;   // origin and turn about z, degrees
    void frame(float x, float y, float z, float turnDeg = 0) { org = { x, y, z }; fz = turnDeg; }
    void frame() { org = { 0, 0, 0 }; fz = 0; }

    // --- solids (all coordinates in model units: x forward / east, y right / south, z up; the ground is z = 0) ---
    // box by centre and half sizes
    Prim& box(float cx, float cy, float cz, float hx, float hy, float hz, Color col);
    // box standing on z0, by its plan rectangle centre and half sizes, with a height
    Prim& slab(float cx, float cy, float hx, float hy, float z0, float height, Color col) { return box(cx, cy, z0 + height * 0.5f, hx, hy, height * 0.5f, col); }
    // tapered box: base rectangle (half sizes hx, hy) at z0, top rectangle (tx, ty) at z0 + height, the top shifted by (sx, sy): hulls, turrets, roofs
    Prim& taper(float cx, float cy, float z0, float hx, float hy, float height, float tx, float ty, Color col, float sx = 0, float sy = 0);
    // a convex side profile in (x, z) extruded across y from y0 to y1: sloped hulls, wedges, gables, ramps
    Prim& profileY(const std::vector<P2>& xz, float y0, float y1, Color col);
    // a convex front profile in (y, z) extruded along x
    Prim& profileX(const std::vector<P2>& yz, float x0, float x1, Color col);
    // a convex plan outline in (x, y) extruded up from z0 to z1: wings, decks, bows, pads
    Prim& plan(const std::vector<P2>& xy, float z0, float z1, Color col);
    // cylinder / cone along an axis ('x', 'y' or 'z') by centre, radius and half length; top = far-end radius / radius
    Prim& cyl(char axis, float cx, float cy, float cz, float r, float halfLen, Color col, float top = 1);
    // a rod from a to b (barrels, struts, masts, pipes)
    Prim& rod(V3 a, V3 b, float r, Color col, float top = 1);
    // ellipsoid by centre and radii
    Prim& ball(float cx, float cy, float cz, float rx, float ry, float rz, Color col);
    // dome: the upper half of an ellipsoid standing on z0
    Prim& dome(float cx, float cy, float z0, float rx, float ry, float rz, Color col);
    // enlarge everything built so far about the origin (a whole figure made bigger or smaller)
    void scale(float k);
private:
    Prim& add(Prim& p);
    Prim& convex(const std::vector<Plane>& pl, V3 centre, float rad, Color col);
};

struct RenderOpt {
    float scale = 0.65f;    // screen pixels per model unit
    float heading = 0;      // radians, turns the model about z (screen clockwise, as Entity::angle)
    int ss = 3;             // samples per pixel along each axis
    bool outline = true;    // a dark contact line round the silhouette
    float shadow = 0.46f;   // darkness of the shadow on the ground
};
struct Rendered {
    Canvas color{ 1, 1 };   // the sprite (team-colour areas painted a neutral shade)
    Canvas mask{ 1, 1 };    // white-shaded team-colour areas on clear: draw over 'color' with the owner's colour modulated in
    bool hasMask = false;
    float ox = 0, oy = 0;   // where the model's origin (0, 0, 0) lies in the canvases
};
void render(const Model& m, const RenderOpt& o, Rendered& out);
// color with the mask laid over it in an owner's colour (previews, thumbnails, sprites that are baked per owner)
Canvas composite(const Rendered& r, Color team);

// --- baking (m3d_bake.cpp): many views rendered at once on every core, and kept in a cache file between runs, so only a model that changed is
// ever rendered again. Each result is cropped to what it shows.
struct Baked {
    Canvas color{ 1, 1 }, mask{ 1, 1 };
    bool hasMask = false;
    float ox = 0, oy = 0;     // the model's origin in 'color'
    float mox = 0, moy = 0;   // ... and in 'mask' (cropped on its own: it is usually much smaller)
};
struct Shot { const Model* model; RenderOpt opt; Baked out; };
u64 modelKey(const Model& m);              // a hash of everything that decides how the model renders
void bakeOpen(const std::string& file);    // reads the cache (no such file, or an older renderer's: everything is rendered afresh)
void bake(std::vector<Shot>& shots);       // fills every shot's 'out'
void bakeClose();                          // writes the cache back if anything was rendered, and frees it

}  // namespace m3d
