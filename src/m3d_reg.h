// One Hour - the model registry: which function builds the 3D model of each unit type, structure type and named prop (see m3d.h).
// A model file registers its builders with the macros at the bottom; the renderer side (gfx.cpp) and the model viewer (tools/m3dview.cpp) look them up.
#pragma once
#include "m3d.h"
#include "data.h"

namespace m3d {

// A unit: 'body' is the hull (or the whole unit); 'turret' is the part that turns on its own (left empty when there is none), modelled round the
// model origin at its real height, facing +x. frame 0 stands; 1 and 2 are the two phases of a walk cycle (vehicles ignore it).
// Units are modelled in design pixels: about 40 long for a battle tank, a soldier about 15 tall. The unit faces +x; the ground is z = 0.
typedef void (*UnitFn)(Model& body, Model& turret, int frame);
// A structure: modelled in screen pixels with its footprint's top-left corner at (0, 0) and BPX pixels to a map tile, so a w x h tile
// footprint spans x 0..w*BPX, y 0..h*BPX; z up (a point at height z is drawn OBL_K * z pixels higher). state 0 = finished.
static const float BPX = 32.0f;
typedef void (*BuildFn)(Model& m, int state);
typedef void (*PropFn)(Model& m, int variant);

struct Registry {
    UnitFn unit[U_COUNT] = {};
    const char* unitName[U_COUNT] = {};
    BuildFn build[B_COUNT] = {};
    const char* buildName[B_COUNT] = {};
    struct Prop { const char* name; PropFn fn; };
    std::vector<Prop> props;
    PropFn prop(const char* name) const { for (const Prop& p : props) if (!strcmp(p.name, name)) return p.fn; return nullptr; }
};
inline Registry& reg() { static Registry r; return r; }
struct RegUnit { RegUnit(int id, const char* name, UnitFn f) { reg().unit[id] = f; reg().unitName[id] = name; } };
struct RegBuild { RegBuild(int id, const char* name, BuildFn f) { reg().build[id] = f; reg().buildName[id] = name; } };
struct RegProp { RegProp(const char* name, PropFn f) { reg().props.push_back({ name, f }); } };

}  // namespace m3d

#define M3D_CAT2(a, b) a##b
#define M3D_CAT(a, b) M3D_CAT2(a, b)
#define M3D_UNIT(id, fn) static m3d::RegUnit M3D_CAT(regU_, __LINE__)(id, #id, fn);
#define M3D_BUILD(id, fn) static m3d::RegBuild M3D_CAT(regB_, __LINE__)(id, #id, fn);
#define M3D_PROP(name, fn) static m3d::RegProp M3D_CAT(regP_, __LINE__)(name, fn);
