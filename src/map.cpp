#include "map.h"
#include <queue>

Map g_map;

// --- small value-noise for terrain variation (deterministic) ---
static float hash2(int x, int y, u32 seed) {
    u32 h = (u32)x * 374761393u + (u32)y * 668265263u + seed * 2246822519u;
    h = (h ^ (h >> 13)) * 1274126177u;
    h ^= h >> 16;
    return (h & 0xFFFF) / 65535.0f;
}
static float vnoise(float x, float y, u32 seed) {
    int ix = (int)std::floor(x), iy = (int)std::floor(y);
    float fx = x - ix, fy = y - iy;
    fx = fx * fx * (3 - 2 * fx); fy = fy * fy * (3 - 2 * fy);
    float a = hash2(ix, iy, seed), b = hash2(ix + 1, iy, seed), c = hash2(ix, iy + 1, seed), d = hash2(ix + 1, iy + 1, seed);
    return lerpf(lerpf(a, b, fx), lerpf(c, d, fx), fy);
}
static float fbm(float x, float y, u32 seed) {
    return vnoise(x, y, seed) * 0.55f + vnoise(x * 2.1f, y * 2.1f, seed + 7) * 0.3f + vnoise(x * 4.3f, y * 4.3f, seed + 13) * 0.15f;
}

static void paintDisc(Map& m, float cx, float cy, float r, u8 t, bool onlyIf(u8) = nullptr) {
    int x0 = clampi((int)(cx - r - 1), 0, MAP_W - 1), x1 = clampi((int)(cx + r + 1), 0, MAP_W - 1);
    int y0 = clampi((int)(cy - r - 1), 0, MAP_H - 1), y1 = clampi((int)(cy + r + 1), 0, MAP_H - 1);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
        if (dx * dx + dy * dy <= r * r) {
            u8& c = m.tiles[y * MAP_W + x];
            if (!onlyIf || onlyIf(c)) c = t;
        }
    }
}
static void paintRoad(Map& m, float x0, float y0, float x1, float y1, float halfW, bool bridge = false) {
    if (!bridge) m.roads.push_back({x0, y0, x1, y1, halfW});
    float len = std::sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0));
    int steps = (int)(len * 2) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = i / (float)steps;
        float cx = x0 + (x1 - x0) * t, cy = y0 + (y1 - y0) * t;
        int xa = clampi((int)(cx - halfW - 1), 0, MAP_W - 1), xb = clampi((int)(cx + halfW + 1), 0, MAP_W - 1);
        int ya = clampi((int)(cy - halfW - 1), 0, MAP_H - 1), yb = clampi((int)(cy + halfW + 1), 0, MAP_H - 1);
        for (int y = ya; y <= yb; y++) for (int x = xa; x <= xb; x++) {
            float dx = x + 0.5f - cx, dy = y + 0.5f - cy;
            if (dx * dx + dy * dy <= halfW * halfW) {
                u8& c = m.tiles[y * MAP_W + x];
                if (c == T_WATER && bridge) m.bridge[y * MAP_W + x] = 1;
                if (c != T_WATER || bridge) c = T_ROAD;
            }
        }
    }
}
static bool isGrass(u8 t) { return t == T_GRASS || t == T_GRASS2; }

void Map::generate() {
    const u32 SEED = 1979;
    // The arena's routes scale with the map; structures, road widths and unit
    // silhouettes stay at their original readable size.
    const float G = MAP_W / 104.0f;
    const float S = MAP_W / 80.0f;
    const float C = MAP_W * 0.5f;   // map centre, tile units
    auto sc = [&](int v) { return (int)std::lround(v * S); };
    roads.clear();
    // Base terrain from noise: grass with dirt patches, sand near center
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = fbm(x * 0.07f, y * 0.07f, SEED);
        u8 t = T_GRASS;
        if (n > 0.62f) t = T_DIRT;
        else if (n < 0.40f) t = T_GRASS2;
        tiles[y * MAP_W + x] = t;
        variant[y * MAP_W + x] = (u8)(hash2(x, y, SEED + 99) * 255);
        blocked[y * MAP_W + x] = 0;
        bridge[y * MAP_W + x] = 0;
    }
    // The central lake: a big organic body of water, the arena of the warships. Its shore bulges into bays toward the four corners and pulls back
    // toward the supply piles on the axes; a sandy beach rings it, two islands and two rocky islets break the open water, and the four
    // rivers leave it (they are cut off by the bridges of the ring road, so only the lake itself is a sea for ships).
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float dx = x + 0.5f - C, dy = y + 0.5f - C, d = std::sqrt(dx * dx + dy * dy), th = std::atan2(dy, dx);
        float R = (20.0f + 1.5f * std::cos(4.0f * th + 3.14159f) + 0.6f * std::sin(3.0f * th + 1.1f) + 0.5f * std::sin(8.0f * th + 0.7f) + (fbm(x * 0.21f + 7, y * 0.21f + 3, SEED + 77) - 0.5f) * 2.0f) * G;
        if (d <= R + 2.2f) tiles[y * MAP_W + x] = T_SAND;
        if (d <= R) tiles[y * MAP_W + x] = T_WATER;
    }
    // Pools inside the ring road's corners, on the diagonals between the lake and the road (decor + partial cover)
    for (int q = 0; q < 4; q++) {
        float px = C + ((q & 1) ? 21.0f : -21.0f) * G, py = C + ((q & 2) ? 21.0f : -21.0f) * G;
        paintDisc(*this, px, py, 3.0f, T_SAND); paintDisc(*this, px, py, 1.9f, T_WATER);
    }

    // Rock ridges dividing quadrants, each with a gap (chokepoint)
    auto ridge = [&](int x0, int x1, int y0, int y1, bool vertical, int gapA, int gapB) {
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            int along = vertical ? y : x;
            if (along >= gapA && along <= gapB) continue;
            // ragged edge
            float n = hash2(x, y, SEED + 5);
            int edgeDist = vertical ? std::min(x - x0, x1 - x) : std::min(y - y0, y1 - y);
            if (edgeDist == 0 && n < 0.45f) continue;
            tiles[y * MAP_W + x] = T_ROCK;
        }
    };
    const int tip = (int)(C - 26 * G);   // ridge ends stop under the ring road (its discs paint over the last tiles), leaving a shore corridor inside the ring with the supply piles in it
    ridge(sc(38), sc(41), 1, tip, true, sc(10), sc(14));
    ridge(sc(38), sc(41), MAP_H - 1 - tip, sc(78), true, sc(65), sc(69));
    ridge(1, tip, sc(38), sc(41), false, sc(10), sc(14));
    ridge(MAP_W - 1 - tip, sc(78), sc(38), sc(41), false, sc(65), sc(69));
    // A Mountain Fortress is set into each ridge, in the long stretch of mountain between the map edge and the pass: the ridge rock is cut back
    // round the fortress so each of the two armies on either side of the ridge has a doorstep onto it, but the fortress itself still closes
    // the ridge (it is no second pass).
    std::vector<LandmarkSpot> forts;
    {
        const int FW = 5, FH = 4;   // BUILDS[B_C_FORTRESS] footprint
        int axis = (sc(38) + sc(41)) / 2, segA = 2, segB = sc(10) - 1;   // the ridge stretch between the map edge and the pass
        int along = (segA + segB) / 2;
        struct F { int x, y; bool vertical; } F4[4] = {
            { axis - FW / 2, along - FH / 2, true }, { axis - FW / 2, (MAP_H - 1) - along - FH / 2, true },
            { along - FW / 2, axis - FH / 2, false }, { (MAP_W - 1) - along - FW / 2, axis - FH / 2, false } };
        for (auto& f : F4) {
            // the footprint and a doorstep two tiles deep on the two open faces become worn ground
            int x0 = f.x - (f.vertical ? 2 : 0), x1 = f.x + FW - 1 + (f.vertical ? 2 : 0);
            int y0 = f.y - (f.vertical ? 0 : 2), y1 = f.y + FH - 1 + (f.vertical ? 0 : 2);
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (inMap(x, y)) tiles[y * MAP_W + x] = T_DIRT;
            forts.push_back({ f.x, f.y, 5 });
        }
    }
    // Rock outcrops guarding the diagonal approaches to the center
    paintDisc(*this, 25.7f * S, 32.3f * S, 2.4f * S, T_ROCK); paintDisc(*this, 54.3f * S, 32.3f * S, 2.4f * S, T_ROCK);   // (pushed out with the bigger lake: rocky headlands on its shore)
    paintDisc(*this, 25.7f * S, 47.7f * S, 2.4f * S, T_ROCK); paintDisc(*this, 54.3f * S, 47.7f * S, 2.4f * S, T_ROCK);
    // Map border rocks
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) tiles[y * MAP_W + x] = T_ROCK;

    // Roads: diagonals from bases to the ring road around the lake, plus the ring
    {
        int st = sc(11), sf = MAP_W - 1 - st;
        starts = { {st, st}, {sf, st}, {st, sf}, {sf, sf} };
    }
    const float ring = 26.5f * G;   // the ring road runs around the lake's beach and the supply piles on its shore
    float rx0 = C - ring, rx1 = C + ring;
    paintRoad(*this, rx0, rx0, rx1, rx0, 1.4f);
    paintRoad(*this, rx1, rx0, rx1, rx1, 1.4f);
    paintRoad(*this, rx1, rx1, rx0, rx1, 1.4f);
    paintRoad(*this, rx0, rx1, rx0, rx0, 1.4f);
    {
        float dn = sc(13), df = (MAP_W - 1) - dn;   // just outside each Command Core
        paintRoad(*this, dn, dn, rx0, rx0, 1.5f);
        paintRoad(*this, df, dn, rx1, rx0, 1.5f);
        paintRoad(*this, dn, df, rx0, rx1, 1.5f);
        paintRoad(*this, df, df, rx1, rx1, 1.5f);
        // Lateral roads through the chokepoint gaps
        float ln = sc(12), lf = (MAP_W - 1) - sc(12) - 1;
        paintRoad(*this, dn, ln, df, ln, 1.3f);
        paintRoad(*this, dn, lf + 1, df, lf + 1, 1.3f);
        paintRoad(*this, ln, dn, ln, df, 1.3f);
        paintRoad(*this, lf + 1, dn, lf + 1, df, 1.3f);
    }

    // Rivers: four meandering streams leave the lake towards the map edge (one per quadrant, mirrored). Roads that cross them become bridges and
    // every river has a sandy ford besides, so nothing is cut off, but armies funnel through the crossings.
    {
        struct Pt { float x, y; };
        static const Pt NW0[] = { {32.5f, 35.3f}, {26.5f, 36.2f}, {22, 36.8f}, {18.5f, 34.5f}, {16, 31.5f}, {12.5f, 29.6f}, {8, 30.4f}, {4.5f, 32.4f}, {1, 31.5f} };
        const int N = (int)(sizeof(NW0) / sizeof(NW0[0]));
        Pt NW[16];
        for (int i = 0; i < N; i++) NW[i] = Pt{ NW0[i].x * S, NW0[i].y * S };
        for (int q = 0; q < 4; q++) {
            auto tf = [&](Pt p) { return Pt{ (q & 1) ? (MAP_W - 1) - p.x : p.x, (q & 2) ? (MAP_H - 1) - p.y : p.y }; };
            std::vector<Pt> pts;   // Catmull-Rom through the control points
            for (int i = 0; i < N - 1; i++) {
                Pt a = NW[std::max(0, i - 1)], b = NW[i], c = NW[i + 1], d = NW[std::min(N - 1, i + 2)];
                for (int k = 0; k < 8; k++) {
                    float t = k / 8.0f, t2 = t * t, t3 = t2 * t;
                    auto cr = [&](float p0, float p1, float p2, float p3) { return 0.5f * ((2 * p1) + (-p0 + p2) * t + (2 * p0 - 5 * p1 + 4 * p2 - p3) * t2 + (-p0 + 3 * p1 - 3 * p2 + p3) * t3); };
                    pts.push_back(tf(Pt{cr(a.x, b.x, c.x, d.x), cr(a.y, b.y, c.y, d.y)}));
                }
            }
            pts.push_back(tf(NW[N - 1]));
            for (size_t i = 0; i < pts.size(); i++) {   // banks first (grass and dirt only), then the water on top
                float w = (1.35f + 0.55f * (1.0f - i / (float)pts.size()) + 0.35f * std::sin(i * 0.7f + q)) * S;
                paintDisc(*this, pts[i].x, pts[i].y, w + 0.9f * S, T_SAND, [](u8 t) { return t == T_GRASS || t == T_GRASS2 || t == T_DIRT; });
            }
            for (size_t i = 0; i < pts.size(); i++) {
                float w = (1.35f + 0.55f * (1.0f - i / (float)pts.size()) + 0.35f * std::sin(i * 0.7f + q)) * S;
                paintDisc(*this, pts[i].x, pts[i].y, w, T_WATER, [](u8 t) { return t != T_ROCK; });
            }
            {   // a wider, marshy pool part-way down each river, where the stream slows and spreads
                Pt mp = pts[pts.size() * 13 / 20];
                paintDisc(*this, mp.x, mp.y, 3.4f * S, T_SAND, [](u8 t) { return t == T_GRASS || t == T_GRASS2 || t == T_DIRT; });
                paintDisc(*this, mp.x, mp.y, 2.7f * S, T_WATER, [](u8 t) { return t != T_ROCK; });
            }
            size_t fi = pts.size() * 9 / 20;   // the ford: a narrow sandy shallows strip laid across the stream
            Pt f = pts[fi], d{ pts[fi + 2].x - pts[fi - 2].x, pts[fi + 2].y - pts[fi - 2].y };
            float dl = std::sqrt(d.x * d.x + d.y * d.y); d.x /= dl; d.y /= dl;
            for (float t = -3.2f * S - 0.8f; t <= 3.2f * S + 0.8f; t += 0.25f) for (float u = -0.9f * S; u <= 0.9f * S; u += 0.3f) {
                int fx = (int)(f.x - d.y * t + d.x * u), fy = (int)(f.y + d.x * t + d.y * u);
                if (inMap(fx, fy) && tiles[fy * MAP_W + fx] == T_WATER) tiles[fy * MAP_W + fx] = T_SAND;
            }
        }
        // bridges: every road is laid over the water again
        for (auto& rs : roads) paintRoad(*this, rs.x0, rs.y0, rs.x1, rs.y1, rs.halfW, true);
        // square off ragged deck ends: a road tile touching the deck both along and across it joins it
        for (int pass = 0; pass < 2; pass++) {
            for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++) {
                int i = y * MAP_W + x;
                if (tiles[i] != T_ROAD || bridge[i]) continue;
                bool h = bridge[i - 1] || bridge[i + 1], v = bridge[i - MAP_W] || bridge[i + MAP_W];
                if (h && v) bridge[i] = 2;
            }
            for (int i = 0; i < MAP_W * MAP_H; i++) if (bridge[i] == 2) bridge[i] = 1;
        }
    }

    // Two islands (painted after the rivers, whose channels start out in the lake). The big one in the middle of the lake: a sandy shore round a grassy top with a stand of trees, an Island Nuke Launcher and an
    // old watchtower. The small one lies off the east shore (well out in the water, not a wade away) with its own launcher and a few trees.
    // Nothing drives to either: infantry and dozers are airlifted in, so they are prizes for whoever holds the water and the sky.
    islands = { { C, C, 7.0f }, { C + 16.5f, C + 2.5f, 4.4f } };
    paintDisc(*this, C, C, 7.0f, T_SAND); paintDisc(*this, C, C, 5.9f, T_GRASS);
    paintDisc(*this, C - 2.6f, C + 3.2f, 4.4f, T_SAND, [](u8 t) { return t == T_WATER; }); paintDisc(*this, C - 2.4f, C + 2.6f, 3.4f, T_GRASS);   // a lobe to the south-west
    paintDisc(*this, C + 16.5f, C + 2.5f, 4.4f, T_SAND); paintDisc(*this, C + 16.5f, C + 2.5f, 3.5f, T_GRASS2);
    {   // a few trees on each island, kept off the launchers' footprints and the landing ground
        static const int TREES[][2] = { {-5, -2}, {-4, -4}, {-5, -3}, {3, -5}, {4, -4}, {-5, 3}, {-4, 5}, {1, 5} };
        for (auto& t : TREES) tiles[(int)(C + t[1]) * MAP_W + (int)(C + t[0])] = T_TREE;
        static const int TREES2[][2] = { {-2, -3}, {2, 1}, {-3, 1} };
        for (auto& t : TREES2) tiles[(int)(C + 2.5f + t[1]) * MAP_W + (int)(C + 16.5f + t[0])] = T_TREE;
    }
    paintDisc(*this, C - 13.0f, C - 10.0f, 1.9f, T_ROCK); paintDisc(*this, C - 9.0f, C + 15.5f, 1.9f, T_ROCK);   // rocky islets (nothing can be built on them)
    // Tree clusters from noise, kept off roads, bases, and the shore
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        u8& t = tiles[y * MAP_W + x];
        if (!isGrass(t)) continue;
        float n = fbm(x * 0.13f + 40, y * 0.13f + 40, SEED + 31);
        bool nearBase = false;
        for (auto& s : starts) if (std::abs(x - s.tx) < 15 && std::abs(y - s.ty) < 15) nearBase = true;
        float dc = std::sqrt((x - C) * (x - C) + (y - C) * (y - C));
        if (nearBase || dc < 24 * G) continue;
        // keep roads' neighbors clear
        bool nearRoad = false;
        for (int dy = -1; dy <= 1 && !nearRoad; dy++) for (int dx = -1; dx <= 1; dx++)
            if (tiles[(y + dy) * MAP_W + x + dx] == T_ROAD) { nearRoad = true; break; }
        if (nearRoad) continue;
        if (n > 0.63f) t = T_TREE;
    }
    // Forest belts: two dense stands in each quadrant between the base and the ring road (cover for infantry, never sealed:
    // the noise gate leaves gaps a column can thread). Placed by rotation so every quadrant gets the same woods.
    {
        static const float BELT[2][2] = { {18, 30}, {36, 22} };   // NW quadrant, tile units
        for (int q = 0; q < 4; q++) for (auto& b : BELT) {
            float bx = b[0] * G, by = b[1] * G;
            for (int k = 0; k < q; k++) { float t = bx; bx = (MAP_W - 1) - by; by = t; }   // rotate 90 degrees q times
            const float BR = 5.5f * G;
            int x0 = clampi((int)(bx - BR - 1), 1, MAP_W - 2), x1 = clampi((int)(bx + BR + 1), 1, MAP_W - 2);
            int y0 = clampi((int)(by - BR - 1), 1, MAP_H - 2), y1 = clampi((int)(by + BR + 1), 1, MAP_H - 2);
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
                float dx = x + 0.5f - bx, dy = y + 0.5f - by, d = std::sqrt(dx * dx + dy * dy) / BR;
                if (d > 1.0f) continue;
                u8& t = tiles[y * MAP_W + x];
                if (!isGrass(t)) continue;
                bool near = false;
                for (auto& s : starts) if (std::abs(x - s.tx) < 14 && std::abs(y - s.ty) < 14) near = true;
                for (int j = -1; j <= 1 && !near; j++) for (int i = -1; i <= 1; i++) if (tiles[(y + j) * MAP_W + x + i] == T_ROAD) near = true;
                if (near) continue;
                if (fbm(x * 0.35f + 9, y * 0.35f + 4, SEED + 47) > 0.38f + 0.25f * d) t = T_TREE;
            }
        }
    }
    // Supply piles: two per base, four on the lake shore, four in the lateral gaps, four contested ones in the quadrant woods
    supplies = {
        {8, 21, 36000}, {21, 8, 36000},
        {95, 21, 36000}, {82, 8, 36000},
        {8, 82, 36000}, {21, 95, 36000},
        {95, 82, 36000}, {82, 95, 36000},
        {52, 29, 50000}, {75, 52, 50000}, {52, 75, 50000}, {29, 52, 50000},
        {46, 8, 36000}, {8, 57, 36000}, {57, 95, 36000}, {95, 46, 36000},
        {18, 33, 30000}, {70, 18, 30000}, {85, 70, 30000}, {33, 85, 30000},
    };
    for (auto& supply : supplies) {
        supply.tx = (int)std::lround((supply.tx + 0.5f) * G - 0.5f);
        supply.ty = (int)std::lround((supply.ty + 0.5f) * G - 0.5f);
    }
    // Clear the ground around supply piles and bases so nothing important is walled in
    for (auto& s : supplies) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 3.4f, T_DIRT, [](u8 t) { return t == T_TREE || t == T_ROCK || t == T_WATER; });
    for (auto& s : starts) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 11.5f, T_GRASS, [](u8 t) { return t == T_TREE || t == T_ROCK; });

    // Blocked flags
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = tiles[i];
        blocked[i] = (t == T_WATER || t == T_ROCK || t == T_TREE) ? 1 : 0;
    }
    for (auto& s : supplies) setResource(s.tx, s.ty, true);
    // What a structure may be built over: every tree, and rock that belongs to a small outcrop or islet (a 4-connected patch of at most
    // SMALL_ROCK tiles away from the map border). The ridges, the rocky headlands' cores and the border cliffs stay hard obstacles.
    cleared.clear();
    memset(clearable, 0, sizeof(clearable));
    {
        const int SMALL_ROCK = 70;
        static u8 seenR[MAP_W * MAP_H];
        memset(seenR, 0, sizeof(seenR));
        std::vector<int> comp, stack;
        for (int i = 0; i < MAP_W * MAP_H; i++) {
            if (tiles[i] == T_TREE) clearable[i] = 1;
            if (tiles[i] != T_ROCK || seenR[i]) continue;
            comp.clear(); stack.assign(1, i); seenR[i] = 1; bool border = false;
            while (!stack.empty()) {
                int c = stack.back(); stack.pop_back(); comp.push_back(c);
                int cx = c % MAP_W, cy = c / MAP_W;
                if (cx <= 1 || cy <= 1 || cx >= MAP_W - 2 || cy >= MAP_H - 2) border = true;
                static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + DX4[k], ny = cy + DY4[k];
                    if (!inMap(nx, ny) || tiles[ny * MAP_W + nx] != T_ROCK || seenR[ny * MAP_W + nx]) continue;
                    seenR[ny * MAP_W + nx] = 1; stack.push_back(ny * MAP_W + nx);
                }
            }
            if (!border && (int)comp.size() <= SMALL_ROCK) for (int c : comp) clearable[c] = 2;
        }
    }
    // bodies of water (4-connected): the lake is the one a Naval Yard may touch
    memset(waterComp, 0, sizeof(waterComp));
    waterSize.assign(1, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        if (tiles[y * MAP_W + x] != T_WATER || waterComp[y * MAP_W + x]) continue;
        int id = (int)waterSize.size(); waterSize.push_back(0);
        std::vector<int> stack{ y * MAP_W + x };
        waterComp[y * MAP_W + x] = (u16)id;
        while (!stack.empty()) {
            int i = stack.back(); stack.pop_back(); waterSize[id]++;
            int ix = i % MAP_W, iy = i / MAP_W;
            static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
            for (int k = 0; k < 4; k++) {
                int nx = ix + DX4[k], ny = iy + DY4[k];
                if (!inMap(nx, ny) || tiles[ny * MAP_W + nx] != T_WATER || waterComp[ny * MAP_W + nx]) continue;
                waterComp[ny * MAP_W + nx] = (u16)id; stack.push_back(ny * MAP_W + nx);
            }
        }
    }
    lakeComp = 0;
    for (int i = 1; i < (int)waterSize.size(); i++) if (waterSize[i] > waterSize[lakeComp]) lakeComp = i;
    // Interior water grouped for the caustics pass (see map.h): aligned 4x4 blocks first, then whatever interior tiles remain.
    waterBlocks.clear(); waterTiles.clear();
    {
        auto interior = [&](int x, int y) {
            return tiles[y * MAP_W + x] == T_WATER && tiles[y * MAP_W + x - 1] == T_WATER && tiles[y * MAP_W + x + 1] == T_WATER
                && tiles[(y - 1) * MAP_W + x] == T_WATER && tiles[(y + 1) * MAP_W + x] == T_WATER;
        };
        static u8 covered[MAP_W * MAP_H];
        memset(covered, 0, sizeof(covered));
        for (int by = 4; by + 3 < MAP_H - 1; by += 4) for (int bx = 4; bx + 3 < MAP_W - 1; bx += 4) {
            bool full = true;
            for (int j = 0; j < 4 && full; j++) for (int i = 0; i < 4; i++) if (!interior(bx + i, by + j)) { full = false; break; }
            if (!full) continue;
            waterBlocks.push_back(by * MAP_W + bx);
            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) covered[(by + j) * MAP_W + bx + i] = 1;
        }
        for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++)
            if (!covered[y * MAP_W + x] && interior(x, y)) waterTiles.push_back(y * MAP_W + x);
    }
    // Neutral civilian strongholds: four rotated copies of a castle, a civilian bunker and a watchtower, so every quadrant gets the same set.
    // They are placed on valid open ground away from roads and, before they are reserved, checked not to seal a corridor. Each gets a worn dirt
    // apron (tiles only: no new obstacle) so the landmark reads as part of the landscape rather than a building dropped on the grass.
    landmarks.clear();
    {
        static const struct { int x, y, w, h, kind; } BASE[5] = {
            { 27, 38, 4, 4, 0 },   // castle
            { 40, 28, 3, 3, 1 },   // civilian bunker
            { 37, 34, 2, 2, 2 },   // watchtower
            { 24, 39, 2, 2, 3 },   // abandoned cottage beside the castle
            { 32, 36, 3, 2, 4 },   // old roadside workshop
        };
        auto landmarkFits = [&](int x, int y, int w, int h) {
            for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
                if (!buildable(i, j) || tile(i, j) == T_ROAD) return false;
                if ((blocked[j * MAP_W + i] & 2) || (blocked[j * MAP_W + i] & 4)) return false;
            }
            return !wouldPlug(x, y, w, h);
        };
        for (int q = 0; q < 4; q++) for (auto& L : BASE) {
            int tx = (int)std::lround(L.x * G), ty = (int)std::lround(L.y * G), w = L.w, h = L.h;
            for (int k = 0; k < q; k++) { int nx = MAP_W - ty - h, ny = tx; tx = nx; ty = ny; }   // rotate the whole set with the quadrant
            auto fits = [&](int x, int y) { return landmarkFits(x, y, w, h); };
            int bx = tx, by = ty;
            if (!fits(bx, by)) {   // a river, rock or a narrow corridor shifted on this seed: take the nearest legal footprint
                float best = 1e18f; int fx = -1, fy = -1;
                for (int r = 1; r <= 10; r++) for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                    if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
                    int x = tx + dx, y = ty + dy;
                    if (!fits(x, y)) continue;
                    float d2 = (float)(dx * dx + dy * dy);
                    if (d2 < best) { best = d2; fx = x; fy = y; }
                }
                if (fx < 0) continue;   // no legal spot: leave this copy out rather than wall a corridor off
                bx = fx; by = fy;
            }
            landmarks.push_back({ bx, by, L.kind });
            setStructure(bx, by, w, h, true);
            paintDisc(*this, bx + w * 0.5f, by + h * 0.5f, std::max(w, h) * 0.5f + 2.3f, T_DIRT,
                      [](u8 t) { return t == T_GRASS || t == T_GRASS2 || t == T_SAND; });
        }
        // The Mountain Fortresses in the ridges (their ground was cut back above, before the trees and the blocked flags)
        for (auto& f : forts) { landmarks.push_back(f); setStructure(f.tx, f.ty, 5, 4, true); }
        // The lake islands: an Island Nuke Launcher in the middle of each, and an old watchtower beside the big island's launcher. They are not
        // part of the four-corner symmetry.
        for (size_t k = 0; k < islands.size(); k++) {
            int tx = (int)islands[k].cx - 1, ty = (int)islands[k].cy - 1;
            landmarks.push_back({ tx, ty, 6 });
            setStructure(tx, ty, 3, 3, true);
            paintDisc(*this, islands[k].cx + 0.5f, islands[k].cy + 0.5f, 2.6f, T_DIRT, [](u8 t) { return t == T_GRASS || t == T_GRASS2; });
        }
        {
            int tx = (int)C - 4, ty = (int)C + 1;
            if (landmarkFits(tx, ty, 2, 2)) { landmarks.push_back({ tx, ty, 2 }); setStructure(tx, ty, 2, 2, true); }
        }
    }
    // Short worn access tracks tie the settlement to its road network. Paint only
    // traversable ground along a real path: no new bridge, obstacle or water fill.
    for (const auto& L : landmarks) {
        if (L.kind >= 5) continue;   // (the fortresses sit in the ridges and the launchers on the islands: no track leads there)
        int w = L.kind == 0 ? 4 : (L.kind == 1 || L.kind == 4 ? 3 : 2);
        int h = L.kind == 0 ? 4 : (L.kind == 1 ? 3 : 2);
        Vec2 entry = nearestFree(tileCenter(L.tx + w / 2, L.ty + h), 4);
        float best = 12 * TILE; Vec2 road = entry;
        for (int y = std::max(1, L.ty - 12); y < std::min(MAP_H - 1, L.ty + 13); y++)
            for (int x = std::max(1, L.tx - 12); x < std::min(MAP_W - 1, L.tx + 13); x++) {
                if (tile(x, y) != T_ROAD || bridge[y * MAP_W + x]) continue;
                float d = dist(tileCenter(x, y), entry);
                if (d < best) { best = d; road = tileCenter(x, y); }
            }
        std::vector<Vec2> path;
        if (best >= 12 * TILE || !findPath(entry, road, path, 800)) continue;
        Vec2 prev = entry;
        for (Vec2 next : path) {
            int steps = (int)(dist(prev, next) / 8) + 1;
            for (int i = 0; i <= steps; i++) {
                Vec2 p = prev + (next - prev) * (i / (float)steps);
                paintDisc(*this, p.x / TILE, p.y / TILE, 0.65f, T_DIRT, [](u8 t) { return t == T_GRASS || t == T_GRASS2; });
            }
            prev = next;
        }
    }

}

bool Map::wouldPlug(int tx, int ty, int w, int h) const {
    // If the one-tile ring is open, every exit from the removed footprint is
    // still connected around it. This is an exact shortcut, not a relaxed rule.
    bool ringOpen = w > 0 && h > 0;
    for (int x = tx - 1; ringOpen && x <= tx + w; x++)
        ringOpen = passable(x, ty - 1) && passable(x, ty + h);
    for (int y = ty; ringOpen && y < ty + h; y++)
        ringOpen = passable(tx - 1, y) && passable(tx + w, y);
    if (ringOpen) return false;
    const int R = 6;
    int x0 = std::max(0, tx - R), y0 = std::max(0, ty - R), x1 = std::min(MAP_W - 1, tx + w - 1 + R), y1 = std::min(MAP_H - 1, ty + h - 1 + R);
    int W = x1 - x0 + 1, H = y1 - y0 + 1;
    auto regions = [&](bool withFootprint) {
        // The search window is at most MAP_W*MAP_H; a fixed (2R+8)^2 buffer was too small for the larger structures (10x7 airfields).
        static u8 seen[MAP_W * MAP_H];
        memset(seen, 0, (size_t)W * H);
        int count = 0;
        static int stack[MAP_W * MAP_H];
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            int li = (y - y0) * W + (x - x0);
            bool inF = x >= tx && x < tx + w && y >= ty && y < ty + h;
            if (seen[li] || blocked[y * MAP_W + x] != 0 || (withFootprint && inF)) continue;
            count++;
            int sp = 0; stack[sp++] = li; seen[li] = 1;
            while (sp > 0) {
                int c = stack[--sp], cx = x0 + c % W, cy = y0 + c / W;
                static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + DX[k], ny = cy + DY[k];
                    if (nx < x0 || ny < y0 || nx > x1 || ny > y1) continue;
                    int nl = (ny - y0) * W + (nx - x0);
                    bool nF = nx >= tx && nx < tx + w && ny >= ty && ny < ty + h;
                    if (seen[nl] || blocked[ny * MAP_W + nx] != 0 || (withFootprint && nF)) continue;
                    seen[nl] = 1; stack[sp++] = nl;
                }
            }
        }
        return count;
    };
    if (regions(true) <= regions(false)) return false;
    // The window saw a split, but two pieces of it may still meet beyond the window's edge (round a wood, a ridge or a lake): that is not a
    // plug, and refusing it is why a site with plenty of room sometimes would not go down. Confirm on the whole map: label the free ground with
    // and without the footprint, and count the separate areas the tiles round the footprint belong to. A plug splits one of them in two. A
    // dead-end nook of a few tiles that gets closed off is harmless and does not count.
    static int labA[MAP_W * MAP_H], labB[MAP_W * MAP_H], sizeB[MAP_W * MAP_H + 1];
    static int stackG[MAP_W * MAP_H];
    auto label = [&](int* lab, int* sizes, bool withFootprint) {
        for (int i = 0; i < MAP_W * MAP_H; i++) lab[i] = 0;
        int n = 0;
        for (int i = 0; i < MAP_W * MAP_H; i++) {
            int x = i % MAP_W, y = i / MAP_W;
            bool inF = withFootprint && x >= tx && x < tx + w && y >= ty && y < ty + h;
            if (lab[i] || blocked[i] != 0 || inF) continue;
            n++; int sp = 0, cnt = 0; stackG[sp++] = i; lab[i] = n;
            while (sp > 0) {
                int c = stackG[--sp], cx = c % MAP_W, cy = c / MAP_W; cnt++;
                static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + DX[k], ny = cy + DY[k];
                    if (!inMap(nx, ny)) continue;
                    int ni = ny * MAP_W + nx;
                    bool nF = withFootprint && nx >= tx && nx < tx + w && ny >= ty && ny < ty + h;
                    if (lab[ni] || blocked[ni] != 0 || nF) continue;
                    lab[ni] = n; stackG[sp++] = ni;
                }
            }
            if (sizes) sizes[n] = cnt;
        }
    };
    label(labA, nullptr, false);
    label(labB, sizeB, true);
    std::vector<int> before, after;
    for (int y = ty - 1; y <= ty + h; y++) for (int x = tx - 1; x <= tx + w; x++) {
        if (!inMap(x, y) || (x >= tx && x < tx + w && y >= ty && y < ty + h)) continue;
        int i = y * MAP_W + x;
        if (!labA[i]) continue;
        before.push_back(labA[i]);
        if (labB[i] && sizeB[labB[i]] > 6) after.push_back(labB[i]);   // (a nook of six tiles or fewer may be closed off)
    }
    std::sort(before.begin(), before.end()); before.erase(std::unique(before.begin(), before.end()), before.end());
    std::sort(after.begin(), after.end()); after.erase(std::unique(after.begin(), after.end()), after.end());
    return after.size() > before.size();
}

void Map::clearForStructure(int x, int y, int w, int h) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
        if (!inMap(i, j)) continue;
        int k = j * MAP_W + i, kind = 1;   // (every footprint tile is logged: the renderer also lifts the decorative bushes off it)
        if (vegetation(i, j)) {
            if (tiles[k] == T_ROCK) kind = 2;
            tiles[k] = tiles[k] == T_TREE ? T_GRASS : T_DIRT;   // (felled trees leave grass, a quarried outcrop bare earth)
            blocked[k] &= ~1;
            clearable[k] = 0;
        }
        if (cleared.size() < 200000) cleared.push_back(k | (kind << 24));
    }
    if (cleared.size() < 200000) cleared.push_back(-1);   // (end of this structure's footprint)
}

void Map::setStructure(int x, int y, int w, int h, bool on) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
        if (!inMap(i, j)) continue;
        if (on) blocked[j * MAP_W + i] |= 2; else blocked[j * MAP_W + i] &= ~2;
    }
}
void Map::setResource(int x, int y, bool on) {
    if (!inMap(x, y)) return;
    if (on) blocked[y * MAP_W + x] |= 4; else blocked[y * MAP_W + x] &= ~4;
}

Vec2 Map::nearestFree(Vec2 p, int maxR) const {
    int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1);
    p = Vec2(clampf(p.x, 0, WORLD_W - 1.0f), clampf(p.y, 0, WORLD_H - 1.0f));
    if (passable(tx, ty)) return p;
    for (int r = 1; r <= maxR; r++) {
        for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::abs(dx) != r && std::abs(dy) != r) continue;
            if (passable(tx + dx, ty + dy)) return tileCenter(tx + dx, ty + dy);
        }
    }
    return p;
}

bool Map::lineClear(Vec2 a, Vec2 b) const {
    // Sample along the segment at sub-tile spacing
    float len = dist(a, b);
    int steps = (int)(len / (TILE * 0.35f)) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = i / (float)steps;
        float x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
        // check a small footprint so units don't scrape corners
        for (int k = 0; k < 4; k++) {
            float ox = (k & 1) ? 6.0f : -6.0f, oy = (k & 2) ? 6.0f : -6.0f;
            if (!passable(tileOf(x + ox), tileOf(y + oy))) return false;
        }
    }
    return true;
}

// ---- A* ----
namespace {
struct Node { float f; int idx; bool operator<(const Node& o) const { return f > o.f; } };
// A* open list over one reused buffer (a fresh priority_queue allocated and grew its storage on every search)
struct OpenList {
    std::vector<Node>& v;
    OpenList() : v(buf()) { v.clear(); }
    static std::vector<Node>& buf() { static std::vector<Node> b; return b; }
    bool empty() const { return v.empty(); }
    const Node& top() const { return v.front(); }
    void push(Node n) { v.push_back(n); std::push_heap(v.begin(), v.end()); }
    void pop() { std::pop_heap(v.begin(), v.end()); v.pop_back(); }
};
static float gScore[MAP_W * MAP_H];
static i32 parentIdx[MAP_W * MAP_H];
static u32 visitGen[MAP_W * MAP_H];
static u32 closedGen[MAP_W * MAP_H];
static u32 curGen = 1;
}

bool Map::findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes) const {
    out.clear();
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y) || maxNodes <= 0) return false;
    to = Vec2(clampf(to.x, 0, WORLD_W - 1.0f), clampf(to.y, 0, WORLD_H - 1.0f));
    int sx = clampi(tileOf(from.x), 0, MAP_W - 1), sy = clampi(tileOf(from.y), 0, MAP_H - 1);
    int gx = clampi(tileOf(to.x), 0, MAP_W - 1), gy = clampi(tileOf(to.y), 0, MAP_H - 1);
    if (!passable(sx, sy)) { Vec2 nf = nearestFree(from); sx = tileOf(nf.x); sy = tileOf(nf.y); }
    if (!passable(sx, sy)) return false;
    if (sx == gx && sy == gy) { out.push_back(to); return true; }
    curGen++;
    if (curGen == 0) { memset(visitGen, 0, sizeof(visitGen)); memset(closedGen, 0, sizeof(closedGen)); curGen = 1; }
    OpenList open;
    int start = sy * MAP_W + sx, goal = gy * MAP_W + gx;
    auto h = [&](int idx) {
        int x = idx % MAP_W, y = idx / MAP_W;
        int dx = std::abs(x - gx), dy = std::abs(y - gy);
        // octile distance at the off-road step cost: it can only overestimate where the way runs along a road (a step there costs 0.8), so the
        // search goes straight for the goal instead of flooding the map, and still takes a road it meets on the way
        return 1.0005f * (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy));
    };
    gScore[start] = 0; parentIdx[start] = -1; visitGen[start] = curGen;
    open.push({h(start), start});
    int best = start; float bestH = h(start);
    int expanded = 0;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    bool found = false;
    while (!open.empty()) {
        Node n = open.top(); open.pop();
        if (closedGen[n.idx] == curGen) continue;
        closedGen[n.idx] = curGen;
        if (n.idx == goal) { best = goal; found = true; break; }
        float hh = h(n.idx);
        if (hh < bestH) { bestH = hh; best = n.idx; }
        if (++expanded > maxNodes) break;
        int x = n.idx % MAP_W, y = n.idx / MAP_W;
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!passable(nx, ny)) continue;
            if (d >= 4 && (!passable(x + DX[d], y) || !passable(x, y + DY[d]))) continue; // no corner cutting
            int ni = ny * MAP_W + nx;
            if (closedGen[ni] == curGen) continue;
            float step = (d >= 4 ? 1.4142f : 1.0f) * moveCost(nx, ny);
            float ng = gScore[n.idx] + step;
            if (visitGen[ni] != curGen || ng < gScore[ni]) {
                visitGen[ni] = curGen; gScore[ni] = ng; parentIdx[ni] = n.idx;
                open.push({ng + h(ni), ni});
            }
        }
    }
    // Reconstruct
    std::vector<Vec2> rev;
    for (int i = best; i != -1 && i != start; i = parentIdx[i]) rev.push_back(tileCenter(i % MAP_W, i / MAP_W));
    if (rev.empty()) return false;
    std::reverse(rev.begin(), rev.end());
    if (found) rev.back() = to;
    pullString(from, rev, out);
    return found;
}

// String pulling: skip the waypoints reachable in a straight line, looking at most PULL_AHEAD tiles along the route at a time (testing every
// later waypoint of a long route against the start made the pass quadratic in the path length: an eighth of a crowded tick)
void Map::pullString(Vec2 from, const std::vector<Vec2>& rev, std::vector<Vec2>& out) const {
    const size_t PULL_AHEAD = 14;
    Vec2 cur = from;
    size_t i = 0;
    while (i < rev.size()) {
        size_t j = std::min(rev.size() - 1, i + PULL_AHEAD);
        while (j > i && !lineClear(cur, rev[j])) j--;
        out.push_back(rev[j]);
        cur = rev[j];
        i = j + 1;
    }
}

bool Map::findPathToRect(Vec2 from, int rx, int ry, int rw, int rh, std::vector<Vec2>& out, int maxNodes) const {
    out.clear();
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || maxNodes <= 0 || rw <= 0 || rh <= 0) return false;
    int sx = clampi(tileOf(from.x), 0, MAP_W - 1), sy = clampi(tileOf(from.y), 0, MAP_H - 1);
    if (!passable(sx, sy)) { Vec2 nf = nearestFree(from); sx = tileOf(nf.x); sy = tileOf(nf.y); }
    if (!passable(sx, sy)) return false;
    // goal: a free tile in the ring round the footprint (its eight neighbours' band)
    auto isGoal = [&](int x, int y) { return x >= rx - 1 && x <= rx + rw && y >= ry - 1 && y <= ry + rh && !(x >= rx && x < rx + rw && y >= ry && y < ry + rh); };
    if (isGoal(sx, sy)) { out.push_back(tileCenter(sx, sy)); return true; }
    curGen++;
    if (curGen == 0) { memset(visitGen, 0, sizeof(visitGen)); memset(closedGen, 0, sizeof(closedGen)); curGen = 1; }
    OpenList open;
    int start = sy * MAP_W + sx;
    auto h = [&](int idx) {   // octile distance to the ring (as findPath: at the off-road step cost)
        int x = idx % MAP_W, y = idx / MAP_W;
        int dx = std::max(0, std::max(rx - 1 - x, x - (rx + rw))), dy = std::max(0, std::max(ry - 1 - y, y - (ry + rh)));
        return 1.0005f * (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy));
    };
    gScore[start] = 0; parentIdx[start] = -1; visitGen[start] = curGen;
    open.push({h(start), start});
    int best = start; float bestH = h(start); int expanded = 0; bool found = false;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty()) {
        Node n = open.top(); open.pop();
        if (closedGen[n.idx] == curGen) continue;
        closedGen[n.idx] = curGen;
        int x = n.idx % MAP_W, y = n.idx / MAP_W;
        if (isGoal(x, y)) { best = n.idx; found = true; break; }
        float hh = h(n.idx);
        if (hh < bestH) { bestH = hh; best = n.idx; }
        if (++expanded > maxNodes) break;
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!passable(nx, ny)) continue;
            if (d >= 4 && (!passable(x + DX[d], y) || !passable(x, y + DY[d]))) continue; // no corner cutting
            int ni = ny * MAP_W + nx;
            if (closedGen[ni] == curGen) continue;
            float ng = gScore[n.idx] + (d >= 4 ? 1.4142f : 1.0f) * moveCost(nx, ny);
            if (visitGen[ni] != curGen || ng < gScore[ni]) { visitGen[ni] = curGen; gScore[ni] = ng; parentIdx[ni] = n.idx; open.push({ng + h(ni), ni}); }
        }
    }
    std::vector<Vec2> rev;
    for (int i = best; i != -1 && i != start; i = parentIdx[i]) rev.push_back(tileCenter(i % MAP_W, i / MAP_W));
    if (rev.empty()) return false;
    std::reverse(rev.begin(), rev.end());
    pullString(from, rev, out);
    return found;
}

// ---- the lake
Vec2 Map::nearestWater(Vec2 p, int maxR, bool bigOnly) const {
    int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1);
    if (navOpen(tx, ty) && (!bigOnly || bigWater(tx, ty))) return p;
    for (int pass = 0; pass < 2; pass++)   // open water first; a tile with only a narrow channel is the fallback
        for (int r = 1; r <= maxR; r++) {
            float best = 1e18f; int bx = -1, by = -1;
            for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx++) {
                if (std::abs(dx) != r && std::abs(dy) != r) continue;
                int x = tx + dx, y = ty + dy;
                if (pass == 0 ? !navOpen(x, y) : !navTile(x, y)) continue;
                if (bigOnly && !bigWater(x, y)) continue;
                float d = dist2(tileCenter(x, y), p);
                if (d < best) { best = d; bx = x; by = y; }
            }
            if (bx >= 0) return tileCenter(bx, by);
        }
    return p;
}

bool Map::navLineClear(Vec2 a, Vec2 b) const {
    float len = dist(a, b);
    int steps = (int)(len / (TILE * 0.35f)) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = i / (float)steps;
        if (!navOpen(tileOf(a.x + (b.x - a.x) * t), tileOf(a.y + (b.y - a.y) * t))) return false;
    }
    return true;
}

namespace {
static float nScore[MAP_W * MAP_H];
static i32 nParent[MAP_W * MAP_H];
static u32 nVisit[MAP_W * MAP_H];
static u32 nClosed[MAP_W * MAP_H];
static u32 nGen = 1;
}

// A* over open water. Ships keep off the shore (a tile next to land costs more), cut no corners, and head for the open water nearest to a goal on
// land (a ship sent to shell a shore target sails to the closest it can get).
bool Map::findNavalPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes) const {
    out.clear();
    Vec2 sf = nearestWater(from, 6), gt = nearestWater(to, 14, true);
    if (!navTile(tileOf(gt.x), tileOf(gt.y))) gt = nearestWater(to, 60, true);   // a goal far inland: head for the closest point of the lake
    int sx = clampi(tileOf(sf.x), 0, MAP_W - 1), sy = clampi(tileOf(sf.y), 0, MAP_H - 1);
    int gx = clampi(tileOf(gt.x), 0, MAP_W - 1), gy = clampi(tileOf(gt.y), 0, MAP_H - 1);
    if (!navTile(sx, sy) || !navTile(gx, gy)) return false;
    if (sx == gx && sy == gy) { if (dist(from, sf) > 3.0f) out.push_back(sf); out.push_back(gt); return true; }
    nGen++;
    if (nGen == 0) { memset(nVisit, 0, sizeof(nVisit)); memset(nClosed, 0, sizeof(nClosed)); nGen = 1; }
    struct Node { float f; int idx; bool operator<(const Node& o) const { return f > o.f; } };
    std::priority_queue<Node> open;
    int start = sy * MAP_W + sx, goal = gy * MAP_W + gx;
    auto h = [&](int idx) {
        int x = idx % MAP_W, y = idx / MAP_W, dx = std::abs(x - gx), dy = std::abs(y - gy);
        return 1.0005f * (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy));
    };
    nScore[start] = 0; nParent[start] = -1; nVisit[start] = nGen;
    open.push({h(start), start});
    int best = start; float bestH = h(start); int expanded = 0; bool found = false;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    while (!open.empty()) {
        Node n = open.top(); open.pop();
        if (nClosed[n.idx] == nGen) continue;
        nClosed[n.idx] = nGen;
        if (n.idx == goal) { best = goal; found = true; break; }
        float hh = h(n.idx);
        if (hh < bestH) { bestH = hh; best = n.idx; }
        if (++expanded > maxNodes) break;
        int x = n.idx % MAP_W, y = n.idx / MAP_W;
        for (int d = 0; d < 8; d++) {
            int nx = x + DX[d], ny = y + DY[d];
            if (!navOpen(nx, ny) && !(nx == gx && ny == gy)) continue;
            if (d >= 4 && (!navTile(x + DX[d], y) || !navTile(x, y + DY[d]))) continue;   // no corner cutting
            int ni = ny * MAP_W + nx;
            if (nClosed[ni] == nGen) continue;
            bool shore = !(navTile(nx + 1, ny) && navTile(nx - 1, ny) && navTile(nx, ny + 1) && navTile(nx, ny - 1) && navTile(nx + 1, ny + 1) && navTile(nx - 1, ny - 1) && navTile(nx + 1, ny - 1) && navTile(nx - 1, ny + 1));
            float step = (d >= 4 ? 1.4142f : 1.0f) * (shore ? 1.6f : 1.0f);
            float ng = nScore[n.idx] + step;
            if (nVisit[ni] != nGen || ng < nScore[ni]) { nVisit[ni] = nGen; nScore[ni] = ng; nParent[ni] = n.idx; open.push({ng + h(ni), ni}); }
        }
    }
    std::vector<Vec2> rev;
    for (int i = best; i != -1 && i != start; i = nParent[i]) rev.push_back(tileCenter(i % MAP_W, i / MAP_W));
    if (rev.empty()) return false;
    std::reverse(rev.begin(), rev.end());
    if (found) rev.back() = gt;
    if (dist(from, sf) > 3.0f) out.push_back(sf);   // a ship hugging the shore first sails to the open water beside it (a straight line from where it is might cross the beach)
    Vec2 cur = sf;   // string pulling over open water
    size_t i = 0;
    while (i < rev.size()) {
        size_t j = rev.size() - 1;
        while (j > i && !navLineClear(cur, rev[j])) j--;
        out.push_back(rev[j]);
        cur = rev[j];
        i = j + 1;
    }
    return found;
}
