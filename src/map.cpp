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

// How far a tile lies inland from the main island's coast (tiles; <= 0 is sea). The coast follows the old map's square border a few tiles inside it,
// rounded off at the corners and pushed in and out by noise, so it reads as a coastline with bays and headlands rather than a box.
float mainIslandInland(int x, int y) {
    const u32 SEED = 1979;
    float u = x + 0.5f - ISLE_X0, v = y + 0.5f - ISLE_X0, W = (float)ISLE_W;
    const float RC = 26.0f;   // corner rounding radius
    float cu = std::min(u, W - u), cv = std::min(v, W - v), d;
    if (cu < RC && cv < RC) d = RC - std::sqrt((RC - cu) * (RC - cu) + (RC - cv) * (RC - cv));
    else d = std::min(cu, cv);
    // broad bays and headlands (up to about 11 tiles in or 3 out), smaller coves, and a ragged edge (the bases sit 28 tiles in: always dry)
    float wob = (fbm(x * 0.022f + 300, y * 0.022f + 500, SEED + 401) - 0.5f) * 16.0f + (fbm(x * 0.06f + 40, y * 0.06f + 90, SEED + 403) - 0.5f) * 6.0f
              + (fbm(x * 0.16f, y * 0.16f, SEED + 402) - 0.5f) * 2.5f;
    return d - 4.0f + wob;
}
// The same for the second island: an ellipse with a wandering coast and a harbour bay biting into its east side (roughly in tiles)
float seaIslandInland(int x, int y) {
    const u32 SEED = 1979;
    float dx = (x + 0.5f - SEA_ISLE_X) / SEA_ISLE_RX, dy = (y + 0.5f - MAP_H * 0.5f) / SEA_ISLE_RY;
    float e = std::sqrt(dx * dx + dy * dy);
    float wob = (fbm(x * 0.08f + 11, y * 0.08f + 23, SEED + 420) - 0.5f) * 0.30f;
    float inland = (1.0f + wob - e) * SEA_ISLE_RX;
    float bx = x + 0.5f - (SEA_ISLE_X + SEA_ISLE_RX * 0.8f), by = y + 0.5f - (MAP_H * 0.5f + 7.0f);
    return std::min(inland, std::sqrt(bx * bx + by * by) - 6.0f);   // (the bay)
}

void Map::generate() {
    const u32 SEED = 1979;
    regionDirty = true;
    // The arena's routes scale with the main island (the old map, ISLE_W tiles square, centred in the sea); structures, road widths and unit
    // silhouettes stay at their original readable size. Mirrors and rotations about the map centre are mirrors about the island's centre too.
    const float G = ISLE_W / 104.0f;
    const float S = ISLE_W / 80.0f;
    const float C = MAP_W * 0.5f;   // map centre = the island's centre, tile units
    const int O = ISLE_X0;          // the island's first tile: layout coordinates of the old map are offset by it
    // The lake country (the lake, its beach and islands, the ring road round it, the shore supply piles, the pools and the old strongholds between
    // the lake and the ring road's corners) is scaled up by LAKE_K about the centre on top of the map's own scale: the lake has twice the water of
    // the 146x146 map while the bases, ridges and woods keep their place in the corners.
    const float LAKE_K = 1.17f, GL = G * LAKE_K;
    const float ISL = GL / (146.0f / 104.0f);   // island sizes and offsets were laid out for the 146 map's lake
    auto inner = [&](float v104) { return C + (v104 - 52.0f) * GL; };   // a lake-country coordinate of the 104-tile layout, in tiles
    auto sc = [&](int v) { return O + (int)std::lround(v * S); };
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
        float R = (20.0f + 1.5f * std::cos(4.0f * th + 3.14159f) + 0.6f * std::sin(3.0f * th + 1.1f) + 0.5f * std::sin(8.0f * th + 0.7f) + (fbm(x * 0.21f + 7, y * 0.21f + 3, SEED + 77) - 0.5f) * 2.0f) * GL;
        if (d <= R + 2.2f) tiles[y * MAP_W + x] = T_SAND;
        if (d <= R) tiles[y * MAP_W + x] = T_WATER;
    }
    // Pools inside the ring road's corners, on the diagonals between the lake and the road (decor + partial cover)
    for (int q = 0; q < 4; q++) {
        float px = C + ((q & 1) ? 21.0f : -21.0f) * GL, py = C + ((q & 2) ? 21.0f : -21.0f) * GL;
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
    const int tip = (int)(C - 26 * GL);   // ridge ends stop under the ring road (its discs paint over the last tiles), leaving a shore corridor inside the ring with the supply piles in it
    ridge(sc(38), sc(41), O, tip, true, sc(10), sc(14));   // (from the old border: the coast below cuts them off as cliffs over the sea)
    ridge(sc(38), sc(41), MAP_H - 1 - tip, sc(80) - 1, true, sc(65), sc(69));
    ridge(O, tip, sc(38), sc(41), false, sc(10), sc(14));
    ridge(MAP_W - 1 - tip, sc(80) - 1, sc(38), sc(41), false, sc(65), sc(69));
    // A Mountain Fortress is set into each ridge, in the long stretch of mountain between the map edge and the pass: the ridge rock is cut back
    // round the fortress so each of the two armies on either side of the ridge has a doorstep onto it, but the fortress itself still closes
    // the ridge (it is no second pass).
    std::vector<LandmarkSpot> forts;
    {
        const int FW = 5, FH = 4;   // BUILDS[B_C_FORTRESS] footprint
        int axis = (sc(38) + sc(41)) / 2, segA = O + 2, segB = sc(10) - 1;   // the ridge stretch between the map edge and the pass
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
    {   // (pushed out with the bigger lake: rocky headlands on its shore)
        const float hx = (25.7f - 40.0f) * S * LAKE_K, hy = (32.3f - 40.0f) * S * LAKE_K, hr = 2.4f * S;
        paintDisc(*this, C + hx, C + hy, hr, T_ROCK); paintDisc(*this, C - hx, C + hy, hr, T_ROCK);
        paintDisc(*this, C + hx, C - hy, hr, T_ROCK); paintDisc(*this, C - hx, C - hy, hr, T_ROCK);
    }

    // Roads: diagonals from bases to the ring road around the lake, plus the ring
    {
        int st = sc(11), sf = MAP_W - 1 - st;
        starts = { {st, st}, {sf, st}, {st, sf}, {sf, sf} };
    }
    const float ring = 26.5f * GL;   // the ring road runs around the lake's beach and the supply piles on its shore
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
        for (int i = 0; i < N; i++) NW[i] = Pt{ O + NW0[i].x * S, O + NW0[i].y * S };
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
    // (Both grew with the lake: sizes and offsets are the 146 map's times ISL.)
    const float bigR = 7.0f * std::sqrt(ISL), smallR = 4.4f * std::sqrt(ISL), sx2 = C + 16.5f * ISL, sy2 = C + 2.5f * ISL;
    islands = { { C, C, bigR }, { sx2, sy2, smallR } };
    paintDisc(*this, C, C, bigR, T_SAND); paintDisc(*this, C, C, bigR - 1.1f, T_GRASS);
    paintDisc(*this, C - 2.6f, C + 3.2f, 4.4f, T_SAND, [](u8 t) { return t == T_WATER; }); paintDisc(*this, C - 2.4f, C + 2.6f, 3.4f, T_GRASS);   // a lobe to the south-west
    paintDisc(*this, sx2, sy2, smallR, T_SAND); paintDisc(*this, sx2, sy2, smallR - 0.9f, T_GRASS2);
    {   // a few trees on each island, kept off the launchers' footprints and the landing ground
        static const int TREES[][2] = { {-5, -2}, {-4, -4}, {-5, -3}, {3, -5}, {4, -4}, {-5, 3}, {-4, 5}, {1, 5}, {5, 2}, {6, -1}, {-6, 0} };
        for (auto& t : TREES) { int i = (int)(C + t[1]) * MAP_W + (int)(C + t[0]); if (tiles[i] == T_GRASS) tiles[i] = T_TREE; }
        static const int TREES2[][2] = { {-2, -3}, {2, 1}, {-3, 1}, {3, -2} };
        for (auto& t : TREES2) { int i = (int)(sy2 + t[1]) * MAP_W + (int)(sx2 + t[0]); if (tiles[i] == T_GRASS2) tiles[i] = T_TREE; }
    }
    paintDisc(*this, C - 13.0f * ISL, C - 10.0f * ISL, 1.9f, T_ROCK); paintDisc(*this, C - 9.0f * ISL, C + 15.5f * ISL, 1.9f, T_ROCK);   // rocky islets (nothing can be built on them)
    paintDisc(*this, C + 6.0f * ISL, C - 17.0f * ISL, 1.5f, T_ROCK); paintDisc(*this, C - 19.0f * ISL, C + 4.0f * ISL, 1.4f, T_ROCK);   // and two more out in the wider water

    // ---- the sea. The old map is the main island: its coast wanders a few tiles inside the old border (headlands push out a little), the corners
    // are rounded off, and a beach or a band of cliffs rings it (where the coast is high, and wherever a ridge runs into the sea). The rivers keep
    // flowing out through the beach: their mouths open into the ocean.
    auto baseTile = [&](int x, int y) -> u8 {   // the island's ground before anything else is laid on it (the same noise as the first pass)
        float n = fbm(x * 0.07f, y * 0.07f, SEED);
        return n > 0.62f ? T_DIRT : (n < 0.40f ? T_GRASS2 : T_GRASS);
    };
    auto coastBand = [&](int x, int y, float inland, float beachW, float cliffK) {   // a land tile 'inland' tiles from the water: beach, cliff or as it is
        u8& t = tiles[y * MAP_W + x];
        if (inland >= beachW || t == T_WATER || t == T_ROAD) return;   // (a river runs on out to its mouth; no road reaches the beach)
        bool cliff = t == T_ROCK || fbm(x * 0.06f + 90, y * 0.06f + 40, SEED + 411) > cliffK;
        t = cliff ? T_ROCK : T_SAND;
    };
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float d = mainIslandInland(x, y);
        if (d <= 0) { tiles[y * MAP_W + x] = T_WATER; continue; }
        coastBand(x, y, d, 1.7f + 1.6f * fbm(x * 0.11f + 7, y * 0.11f + 70, SEED + 412), 0.55f);
    }
    // The second island off the east coast: far smaller than the main island but many times the lake's islands. A long green ridge of land with beaches
    // and northern cliffs, a harbour bay biting into its east side, a rocky knoll, woods (the forest pass below), three rich supply piles and three
    // old strongholds. Nothing drives there: transports, cargo ships and lifters bring armies over, infiltrators swim.
    seaIslands.clear();
    {
        const float SX = SEA_ISLE_X, SY = C, RX = SEA_ISLE_RX, RY = SEA_ISLE_RY;
        seaIslands.push_back({ SX, SY, RX, RY });
        for (int y = (int)(SY - RY - 4); y <= (int)(SY + RY + 4); y++) for (int x = (int)(SX - RX - 4); x <= (int)(SX + RX + 4); x++) {
            if (!inMap(x, y)) continue;
            float inland = seaIslandInland(x, y);
            if (inland <= 0) continue;
            tiles[y * MAP_W + x] = baseTile(x, y);
            coastBand(x, y, inland, 1.5f + 1.2f * fbm(x * 0.13f + 3, y * 0.13f + 9, SEED + 413), y < SY - RY * 0.55f ? 0.48f : 0.66f);   // (cliffs mostly on the north end)
        }
        paintDisc(*this, SX - 3.5f, SY - 9.0f, 2.6f, T_ROCK);   // a rocky knoll (small: a structure may still quarry it)
    }
    // Islets scattered round the sea: sand cays with a few trees, bare rock stacks. Kept well off both islands' coasts and the edge of the map.
    {
        Rng ir(SEED + 4401);
        std::vector<Island> got;
        for (int attempt = 0; attempt < 4000 && got.size() < 22; attempt++) {
            float x = ir.f(9.0f, MAP_W - 9.0f), y = ir.f(9.0f, MAP_H - 9.0f), r = ir.f(1.6f, 4.4f);
            if (mainIslandInland((int)x, (int)y) > -9.0f || seaIslandInland((int)x, (int)y) > -9.0f) continue;
            bool near = false;
            for (auto& o2 : got) if (std::hypot(o2.cx - x, o2.cy - y) < 17.0f) near = true;
            if (near) continue;
            got.push_back({ x, y, r });
            if (r < 2.2f || ir.f() < 0.25f) { paintDisc(*this, x, y, r * 0.8f, T_ROCK); continue; }   // a bare stack
            paintDisc(*this, x, y, r, T_SAND);
            paintDisc(*this, x + ir.f(-0.4f, 0.4f), y + ir.f(-0.4f, 0.4f), r - 1.3f, T_GRASS);
            for (int k = 0; k < (int)(r * 1.2f); k++) {
                int tx = (int)(x + ir.f(-r + 1.6f, r - 1.6f)), ty = (int)(y + ir.f(-r + 1.6f, r - 1.6f));
                if (inMap(tx, ty) && tiles[ty * MAP_W + tx] == T_GRASS) tiles[ty * MAP_W + tx] = T_TREE;
            }
        }
        for (auto& g2 : got) seaIslands.push_back({ g2.cx, g2.cy, g2.r, g2.r });
    }
    // Tree clusters from noise, kept off roads, bases, and the shore
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        u8& t = tiles[y * MAP_W + x];
        if (!isGrass(t)) continue;
        float n = fbm(x * 0.13f + 40, y * 0.13f + 40, SEED + 31);
        bool nearBase = false;
        for (auto& s : starts) if (std::abs(x - s.tx) < 15 && std::abs(y - s.ty) < 15) nearBase = true;
        float dc = std::sqrt((x - C) * (x - C) + (y - C) * (y - C));
        if (nearBase || dc < 24 * GL) continue;
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
            float bx = O + b[0] * G, by = O + b[1] * G;
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
    for (size_t k = 0; k < supplies.size(); k++) {
        auto& supply = supplies[k];
        bool shore = k >= 8 && k < 12;   // the four on the lake shore move out with the lake country
        supply.tx = shore ? (int)std::lround(inner(supply.tx + 0.5f) - 0.5f) : O + (int)std::lround((supply.tx + 0.5f) * G - 0.5f);
        supply.ty = shore ? (int)std::lround(inner(supply.ty + 0.5f) - 0.5f) : O + (int)std::lround((supply.ty + 0.5f) * G - 0.5f);
    }
    // three rich piles on the second island (nobody's: whoever ships a hauler and a Supply Hub over gets them)
    supplies.push_back({ (int)SEA_ISLE_X - 3, (int)C - 21, 50000 });
    supplies.push_back({ (int)SEA_ISLE_X + 3, (int)C - 2, 50000 });
    supplies.push_back({ (int)SEA_ISLE_X - 4, (int)C + 19, 50000 });
    // Clear the ground around supply piles and bases so nothing important is walled in
    for (auto& s : supplies) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 3.4f, T_DIRT, [](u8 t) { return t == T_TREE || t == T_ROCK || t == T_WATER; });
    for (auto& s : starts) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 11.5f, T_GRASS, [](u8 t) { return t == T_TREE || t == T_ROCK; });

    // Clearings sealed inside a forest (a few tiles of open ground with trees on every side, no road through them) grow over: on the bigger
    // map the noise woods close more of them, and nothing can ever walk in (a structure or a Waymaker still clears its way to the ground)
    {
        static u8 seenC[MAP_W * MAP_H];
        memset(seenC, 0, sizeof(seenC));
        std::vector<int> comp, stack;
        auto open = [&](int i) { u8 t = tiles[i]; return t != T_WATER && t != T_ROCK && t != T_TREE; };
        for (int i = 0; i < MAP_W * MAP_H; i++) {
            if (seenC[i] || !open(i)) continue;
            comp.clear(); stack.assign(1, i); seenC[i] = 1; bool sealed = true;
            while (!stack.empty()) {
                int c = stack.back(); stack.pop_back(); comp.push_back(c);
                if (tiles[c] == T_ROAD) sealed = false;
                int cx = c % MAP_W, cy = c / MAP_W;
                static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + DX4[k], ny = cy + DY4[k];
                    if (!inMap(nx, ny)) { sealed = false; continue; }
                    int n = ny * MAP_W + nx;
                    if (!open(n)) { if (tiles[n] != T_TREE) sealed = false; continue; }
                    if (!seenC[n]) { seenC[n] = 1; stack.push_back(n); }
                }
                if (comp.size() > 24) sealed = false;
            }
            if (sealed && comp.size() <= 24) for (int c : comp) tiles[c] = T_TREE;
        }
    }
    // Pockets of the main island's ground walled in by the coast's cliffs and the sea (a strip of beach under a cliff, a cliff top) are part of the
    // cliffs: nothing could ever walk in or out. (The lake's islands are cut off by design, and so is everything out at sea.)
    {
        static int labC[MAP_W * MAP_H];
        memset(labC, 0, sizeof(labC));
        auto open = [&](int i) { u8 t = tiles[i]; return t != T_WATER && t != T_ROCK && t != T_TREE; };
        std::vector<int> sizes(1, 0), stack;
        for (int i = 0; i < MAP_W * MAP_H; i++) {
            if (labC[i] || !open(i)) continue;
            int id = (int)sizes.size(); sizes.push_back(0);
            stack.assign(1, i); labC[i] = id;
            while (!stack.empty()) {
                int c = stack.back(); stack.pop_back(); sizes[id]++;
                int cx = c % MAP_W, cy = c / MAP_W;
                static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = cx + DX4[k], ny = cy + DY4[k];
                    if (!inMap(nx, ny)) continue;
                    int n = ny * MAP_W + nx;
                    if (!labC[n] && open(n)) { labC[n] = id; stack.push_back(n); }
                }
            }
        }
        int mainId = labC[starts[0].ty * MAP_W + starts[0].tx];
        for (int i = 0; i < MAP_W * MAP_H; i++) {
            int id = labC[i];
            if (!id || id == mainId || sizes[id] > 160) continue;
            int x = i % MAP_W, y = i / MAP_W;
            if (mainIslandInland(x, y) <= 0) continue;   // (out at sea: the second island and the islets)
            bool lakeIsle = false;
            for (auto& isl : islands) if ((x - isl.cx) * (x - isl.cx) + (y - isl.cy) * (y - isl.cy) <= (isl.r + 3) * (isl.r + 3)) lakeIsle = true;
            if (lakeIsle || mainIslandInland(x, y) > 14.0f) continue;   // (only the coast: pockets inland are the forests' business, see above)
            tiles[i] = T_ROCK;
        }
    }
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
    // landmasses (4-connected land, bridges included): the main island, the second island, each islet. Structures and trees do not split them
    landComp.assign((size_t)MAP_W * MAP_H, 0);
    {
        int id = 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            if (tiles[y * MAP_W + x] == T_WATER || landComp[y * MAP_W + x]) continue;
            id++;
            std::vector<int> stack{ y * MAP_W + x };
            landComp[y * MAP_W + x] = (u16)id;
            while (!stack.empty()) {
                int i = stack.back(); stack.pop_back();
                int ix = i % MAP_W, iy = i / MAP_W;
                static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = ix + DX4[k], ny = iy + DY4[k];
                    if (!inMap(nx, ny) || tiles[ny * MAP_W + nx] == T_WATER || landComp[ny * MAP_W + nx]) continue;
                    landComp[ny * MAP_W + nx] = (u16)id; stack.push_back(ny * MAP_W + nx);
                }
            }
        }
    }
    // the ocean is the biggest body of water; the lake is the body that holds most of the water round the map's centre
    oceanComp = 0;
    for (int i = 1; i < (int)waterSize.size(); i++) if (waterSize[i] > waterSize[oceanComp]) oceanComp = i;
    {
        std::vector<int> near(waterSize.size(), 0);
        const float LR = 22.0f * GL;
        for (int y = (int)(C - LR); y <= (int)(C + LR); y++) for (int x = (int)(C - LR); x <= (int)(C + LR); x++)
            if (inMap(x, y) && (x - C) * (x - C) + (y - C) * (y - C) <= LR * LR) near[waterComp[y * MAP_W + x]]++;
        lakeComp = 0;
        for (int i = 1; i < (int)near.size(); i++) if (i != oceanComp && near[i] > near[lakeComp]) lakeComp = i;
    }
    // how far every tile lies from land (0 on land), chamfered 2-3 (so 2 = one tile straight, 3 = one diagonal step) then halved
    shoreDist.assign((size_t)MAP_W * MAP_H, 0);
    {
        std::vector<int> d((size_t)MAP_W * MAP_H);
        for (int i = 0; i < MAP_W * MAP_H; i++) d[i] = tiles[i] == T_WATER ? 1 << 20 : 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            int& v = d[(size_t)y * MAP_W + x];
            if (x > 0) v = std::min(v, d[(size_t)y * MAP_W + x - 1] + 2);
            if (y > 0) { v = std::min(v, d[(size_t)(y - 1) * MAP_W + x] + 2); if (x > 0) v = std::min(v, d[(size_t)(y - 1) * MAP_W + x - 1] + 3); if (x < MAP_W - 1) v = std::min(v, d[(size_t)(y - 1) * MAP_W + x + 1] + 3); }
        }
        for (int y = MAP_H - 1; y >= 0; y--) for (int x = MAP_W - 1; x >= 0; x--) {
            int& v = d[(size_t)y * MAP_W + x];
            if (x < MAP_W - 1) v = std::min(v, d[(size_t)y * MAP_W + x + 1] + 2);
            if (y < MAP_H - 1) { v = std::min(v, d[(size_t)(y + 1) * MAP_W + x] + 2); if (x < MAP_W - 1) v = std::min(v, d[(size_t)(y + 1) * MAP_W + x + 1] + 3); if (x > 0) v = std::min(v, d[(size_t)(y + 1) * MAP_W + x - 1] + 3); }
        }
        for (int i = 0; i < MAP_W * MAP_H; i++) shoreDist[i] = (u8)std::min(255, d[i] / 2);
        // and the other way round: how far every land tile lies from water (0 on water), for cheap "is this near a shore" tests
        for (int i = 0; i < MAP_W * MAP_H; i++) d[i] = tiles[i] == T_WATER ? 0 : 1 << 20;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            int& v = d[(size_t)y * MAP_W + x];
            if (x > 0) v = std::min(v, d[(size_t)y * MAP_W + x - 1] + 2);
            if (y > 0) { v = std::min(v, d[(size_t)(y - 1) * MAP_W + x] + 2); if (x > 0) v = std::min(v, d[(size_t)(y - 1) * MAP_W + x - 1] + 3); if (x < MAP_W - 1) v = std::min(v, d[(size_t)(y - 1) * MAP_W + x + 1] + 3); }
        }
        for (int y = MAP_H - 1; y >= 0; y--) for (int x = MAP_W - 1; x >= 0; x--) {
            int& v = d[(size_t)y * MAP_W + x];
            if (x < MAP_W - 1) v = std::min(v, d[(size_t)y * MAP_W + x + 1] + 2);
            if (y < MAP_H - 1) { v = std::min(v, d[(size_t)(y + 1) * MAP_W + x] + 2); if (x < MAP_W - 1) v = std::min(v, d[(size_t)(y + 1) * MAP_W + x + 1] + 3); if (x > 0) v = std::min(v, d[(size_t)(y + 1) * MAP_W + x - 1] + 3); }
        }
        waterDist.assign((size_t)MAP_W * MAP_H, 0);
        for (int i = 0; i < MAP_W * MAP_H; i++) waterDist[i] = (u8)std::min(255, d[i] / 2);
    }
    // Interior water grouped for the overlay passes (see map.h): the whole ocean gets the swell in aligned 8x8 blocks, then 4x4 blocks, then
    // single tiles (no seam between the coast and the open sea), and its shallows a list of tiles for the fading caustics; the lake and the
    // rivers get caustics in aligned 4x4 blocks, then whatever interior tiles remain.
    waterBlocks.clear(); waterTiles.clear(); seaBlocks.clear(); seaQuads.clear(); seaTiles.clear(); seaShallow.clear();
    {
        auto interior = [&](int x, int y) {
            if (x < 1 || y < 1 || x > MAP_W - 2 || y > MAP_H - 2) return false;
            return tiles[y * MAP_W + x] == T_WATER && tiles[y * MAP_W + x - 1] == T_WATER && tiles[y * MAP_W + x + 1] == T_WATER
                && tiles[(y - 1) * MAP_W + x] == T_WATER && tiles[(y + 1) * MAP_W + x] == T_WATER;
        };
        auto ocean = [&](int x, int y) { return interior(x, y) && waterComp[y * MAP_W + x] == oceanComp; };
        auto inland = [&](int x, int y) { return interior(x, y) && waterComp[y * MAP_W + x] != oceanComp; };
        static u8 covered[MAP_W * MAP_H];
        memset(covered, 0, sizeof(covered));
        // the ocean's swell: the biggest aligned blocks first, then 4x4 blocks, then single tiles
        for (int by = 0; by + 7 < MAP_H; by += 8) for (int bx = 0; bx + 7 < MAP_W; bx += 8) {
            bool full = true;
            for (int j = 0; j < 8 && full; j++) for (int i = 0; i < 8; i++) if (!ocean(bx + i, by + j)) { full = false; break; }
            if (!full) continue;
            seaBlocks.push_back(by * MAP_W + bx);
            for (int j = 0; j < 8; j++) for (int i = 0; i < 8; i++) covered[(by + j) * MAP_W + bx + i] = 1;
        }
        for (int by = 0; by + 3 < MAP_H; by += 4) for (int bx = 0; bx + 3 < MAP_W; bx += 4) {
            bool full = true;
            for (int j = 0; j < 4 && full; j++) for (int i = 0; i < 4; i++) if (covered[(by + j) * MAP_W + bx + i] || !ocean(bx + i, by + j)) { full = false; break; }
            if (!full) continue;
            seaQuads.push_back(by * MAP_W + bx);
            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) covered[(by + j) * MAP_W + bx + i] = 1;
        }
        for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++) {
            if (!ocean(x, y)) continue;
            if (!covered[y * MAP_W + x]) seaTiles.push_back(y * MAP_W + x);
            if (shoreDist[y * MAP_W + x] <= SEA_CAUSTIC_REACH) seaShallow.push_back(y * MAP_W + x);
        }
        // the lake and the rivers: caustics everywhere
        memset(covered, 0, sizeof(covered));
        for (int by = 4; by + 3 < MAP_H - 1; by += 4) for (int bx = 4; bx + 3 < MAP_W - 1; bx += 4) {
            bool full = true;
            for (int j = 0; j < 4 && full; j++) for (int i = 0; i < 4; i++) if (!inland(bx + i, by + j)) { full = false; break; }
            if (!full) continue;
            waterBlocks.push_back(by * MAP_W + bx);
            for (int j = 0; j < 4; j++) for (int i = 0; i < 4; i++) covered[(by + j) * MAP_W + bx + i] = 1;
        }
        for (int y = 1; y < MAP_H - 1; y++) for (int x = 1; x < MAP_W - 1; x++)
            if (!covered[y * MAP_W + x] && inland(x, y)) waterTiles.push_back(y * MAP_W + x);
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
            int tx = (int)std::lround(inner((float)L.x)), ty = (int)std::lround(inner((float)L.y)), w = L.w, h = L.h;
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
        // The lake islands: two Island Nuke Launchers on each (the big island's side by side on its north side, clear of the landing ground in
        // its middle; the small one's one above the other), and an old watchtower beside the big island's pair. They are not part of the
        // four-corner symmetry.
        static const int PADS[2][2][2] = { { {-2, -4}, {1, -4} }, { {-1, -4}, {-1, 1} } };   // footprint top-left offsets from each island's centre tile
        for (size_t k = 0; k < islands.size() && k < 2; k++) for (int j = 0; j < 2; j++) {
            int tx = (int)islands[k].cx + PADS[k][j][0], ty = (int)islands[k].cy + PADS[k][j][1];
            landmarks.push_back({ tx, ty, 6 });
            setStructure(tx, ty, 3, 3, true);
            paintDisc(*this, tx + 1.5f, ty + 1.5f, 2.6f, T_DIRT, [](u8 t) { return t == T_GRASS || t == T_GRASS2; });
        }
        {
            int tx = (int)C - 4, ty = (int)C + 1;
            if (landmarkFits(tx, ty, 2, 2)) { landmarks.push_back({ tx, ty, 2 }); setStructure(tx, ty, 2, 2, true); }
        }
        // the second island's old strongholds: a castle over the harbour bay, a watchtower on the cliffs at the north end, a civilian bunker in the south
        static const struct { int dx, dy, w, h, kind; } SEA[3] = { { -5, 5, 4, 4, 0 }, { -1, -29, 2, 2, 2 }, { -2, 27, 3, 3, 1 } };
        for (auto& L : SEA) {
            int tx = (int)SEA_ISLE_X + L.dx, ty = (int)C + L.dy, bx = -1, by = -1;
            for (int r = 0; r <= 6 && bx < 0; r++) for (int dy = -r; dy <= r && bx < 0; dy++) for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r || !landmarkFits(tx + dx, ty + dy, L.w, L.h)) continue;
                bx = tx + dx; by = ty + dy; break;
            }
            if (bx < 0) continue;
            landmarks.push_back({ bx, by, L.kind });
            setStructure(bx, by, L.w, L.h, true);
            paintDisc(*this, bx + L.w * 0.5f, by + L.h * 0.5f, std::max(L.w, L.h) * 0.5f + 2.0f, T_DIRT, [](u8 t) { return t == T_GRASS || t == T_GRASS2; });
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
    // The towns and cities round the lake (see DISTRICT_* in data.h). The lake country is open between the beach and the ring road on either side of the
    // north and south shore piles (the rivers leave the lake beside the east and west ones and the old strongholds hold the diagonals), so a civilian
    // area stands in each of those four stretches: every corner base has one nearest to it. Each is a cluster of civic blocks, the Town Hall at its
    // heart, with a paved street round every block. Two are glass-and-steel cities and two old brick towns (the Cyber and Clanker civic designs, the
    // opposite corners alike); they hold the same blocks, so each is worth the same.
    districts.clear();
    {
        static const char* const NAMES[4] = { "Port Halden", "Lakeview", "Marrowgate", "Fernside" };
        static const float ANG[4] = { -104.0f, -76.0f, 76.0f, 104.0f };   // degrees in screen axes (y down): beside the north and the south pile
        static const int KINDS[] = { CV_TOWNHALL, CV_GOVERNMENT, CV_SKYSCRAPER, CV_EVENTHALL, CV_SKYSCRAPER, CV_SQUARE, CV_APARTMENTS, CV_GROCERY, CV_APARTMENTS,
                                     CV_GAS, CV_HOUSE, CV_HOUSE, CV_HOUSE };
        for (int d = 0; d < 4; d++) {
            float th = ANG[d] * 3.14159265f / 180.0f, cs = std::cos(th), sn = std::sin(th);
            float ringD = ring / std::max(std::fabs(cs), std::fabs(sn));   // where the ring road crosses this bearing
            float shore = ringD;
            for (float r = ringD - 2.0f; r > 10.0f; r -= 0.5f) {   // walk in from the road to the water
                int x = (int)(C + r * cs), y = (int)(C + r * sn);
                if (inMap(x, y) && tile(x, y) == T_WATER) { shore = r; break; }
            }
            float ar = (shore + ringD) * 0.5f;
            DistrictSpot spot; spot.name = NAMES[d]; spot.city = d == 0 || d == 2; spot.cx = C + ar * cs; spot.cy = C + ar * sn;
            auto clearAround = [&](int x, int y, int w, int h) {   // a street round the block: no structure, pile, water or ridge rock in the ring
                for (int j = y - 1; j <= y + h; j++) for (int i = x - 1; i <= x + w; i++) {
                    if (i >= x && i < x + w && j >= y && j < y + h) continue;
                    if (!inMap(i, j)) return false;
                    int k = j * MAP_W + i;
                    if ((blocked[k] & ~1) || tiles[k] == T_WATER || bridge[k] || (tiles[k] == T_ROCK && !clearable[k])) return false;
                }
                return true;
            };
            auto fits = [&](int x, int y, int w, int h) {
                for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
                    if (!buildable(i, j) || tile(i, j) == T_ROAD || tile(i, j) == T_SAND) return false;
                    if (blocked[j * MAP_W + i] & 6) return false;
                }
                for (auto& sp : supplies) if (std::abs(sp.tx - (x + w / 2)) <= 2 + w / 2 + 1 && std::abs(sp.ty - (y + h / 2)) <= 2 + h / 2 + 1) return false;   // the shore pile keeps its yard
                return clearAround(x, y, w, h) && !wouldPlug(x, y, w, h);
            };
            struct Cand { int x, y; float d; };
            std::vector<Cand> cand;
            for (int kk : KINDS) {
                const BuildType& bt = BUILDS[B_C_HOUSE + kk];
                int w = bt.w, h = bt.h;
                cand.clear();
                for (int y = (int)spot.cy - 12; y <= (int)spot.cy + 12; y++) for (int x = (int)spot.cx - 12; x <= (int)spot.cx + 12; x++) {
                    float dx = x + w * 0.5f - spot.cx, dy = y + h * 0.5f - spot.cy, dd = dx * dx + dy * dy;
                    if (dd <= 11.5f * 11.5f) cand.push_back({ x, y, dd + hash2(x, y, SEED + 900 + d) * 0.8f });   // (a little jitter: no perfect rings)
                }
                std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
                for (auto& c : cand) {
                    if (!fits(c.x, c.y, w, h)) continue;
                    spot.blocks.push_back({ kk, c.x, c.y });
                    setStructure(c.x, c.y, w, h, true);
                    break;
                }
            }
            if (spot.blocks.empty()) continue;
            // the streets: every open tile round a block is paved (a tree or a small rock there is cleared first)
            for (auto& b : spot.blocks) {
                const BuildType& bt = BUILDS[B_C_HOUSE + b.kind];
                for (int j = b.ty - 1; j <= b.ty + bt.h; j++) for (int i = b.tx - 1; i <= b.tx + bt.w; i++) {
                    if (!inMap(i, j)) continue;
                    int k = j * MAP_W + i;
                    if (blocked[k] & ~1) continue;
                    u8 t = tiles[k];
                    if (t == T_WATER || bridge[k] || (t == T_ROCK && !clearable[k])) continue;
                    if (t == T_TREE || t == T_ROCK) { blocked[k] &= ~1; clearable[k] = 0; }
                    tiles[k] = T_ROAD;
                }
            }
            regionDirty = true;
            districts.push_back(spot);
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
    // The window saw a split, but two pieces of it may still meet beyond the window's edge (round a wood, a ridge, a lake or along the coast): that
    // is not a plug, and refusing it is why a site with plenty of room sometimes would not go down. Confirm on the whole map. Before the footprint
    // the free ground is the cached region labelling (Map::region); with it, a flood from one of the tiles round the footprint, the footprint
    // blocked, has to reach every other tile round it of the same region. It stops as soon as it has (usually a few tiles round the corner), so a
    // false alarm costs little even on the big sea map. Tiles it cannot reach are a plug unless they are a dead-end nook of six tiles or fewer,
    // which may be closed off.
    if (regionDirty) buildRegions();
    static std::vector<u32> seenG; static u32 genG = 0;
    if (seenG.size() != (size_t)MAP_W * MAP_H) { seenG.assign((size_t)MAP_W * MAP_H, 0); genG = 0; }
    static std::vector<int> ring, queueG;
    ring.clear();
    for (int y = ty - 1; y <= ty + h; y++) for (int x = tx - 1; x <= tx + w; x++) {
        if (!inMap(x, y) || (x >= tx && x < tx + w && y >= ty && y < ty + h)) continue;
        if (blocked[y * MAP_W + x] == 0) ring.push_back(y * MAP_W + x);
    }
    auto inFoot = [&](int i) { int x = i % MAP_W, y = i / MAP_W; return x >= tx && x < tx + w && y >= ty && y < ty + h; };
    // Before: one area per region the tiles round the footprint belong to. After: the pieces those tiles fall into with the footprint blocked,
    // not counting nooks of six tiles or fewer. More pieces after than areas before is a plug. Each piece is flooded only until it has more than six
    // tiles and has reached every tile round the footprint of its region that is still unaccounted for (usually a few tiles round the corner).
    static std::vector<u8> assigned;
    assigned.assign(ring.size(), 0);
    int before = 0, after = 0;
    std::vector<u16> regs;
    for (int i : ring) if (region[i] && std::find(regs.begin(), regs.end(), region[i]) == regs.end()) regs.push_back(region[i]);
    before = (int)regs.size();
    for (u16 rg : regs) {
        for (size_t k = 0; k < ring.size(); k++) {
            if (region[ring[k]] != rg || assigned[k]) continue;
            int want = 0;
            for (size_t j = 0; j < ring.size(); j++) if (region[ring[j]] == rg && !assigned[j]) want++;
            if (++genG == 0) { std::fill(seenG.begin(), seenG.end(), 0); genG = 1; }
            queueG.clear(); queueG.push_back(ring[k]); seenG[ring[k]] = genG;
            int reached = 0; bool done = false;
            for (size_t q = 0; q < queueG.size() && !done; q++) {
                int c = queueG[q];
                for (size_t j = 0; j < ring.size(); j++) if (ring[j] == c && !assigned[j]) { assigned[j] = 2; reached++; }
                if (reached >= want && queueG.size() > 6) { done = true; break; }
                int cx = c % MAP_W, cy = c / MAP_W;
                static const int DX[4] = {1, -1, 0, 0}, DY[4] = {0, 0, 1, -1};
                for (int d = 0; d < 4; d++) {
                    int nx = cx + DX[d], ny = cy + DY[d];
                    if (!inMap(nx, ny)) continue;
                    int ni = ny * MAP_W + nx;
                    if (seenG[ni] == genG || blocked[ni] != 0 || inFoot(ni)) continue;
                    seenG[ni] = genG; queueG.push_back(ni);
                }
            }
            for (auto& a2 : assigned) if (a2 == 2) a2 = 1;
            if (queueG.size() > 6) after++;   // (a nook of six tiles or fewer may be closed off)
        }
    }
    return after > before;
}

void Map::clearForStructure(int x, int y, int w, int h) {
    regionDirty = true;
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

int Map::clearWay(int x, int y, int w, int h) {
    int opened = 0;
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) opened += clearWayTile(i, j);
    clearWayDone();
    return opened;
}

bool Map::clearWayTile(int i, int j) {
    if (!wayTile(i, j)) return false;   // (water, a bridge deck, a structure or a supply pile is left as it is)
    regionDirty = true;
    int k = j * MAP_W + i;
    bool was = blocked[k] & 1;
    if (tiles[k] != T_ROAD) tiles[k] = T_DIRT;   // (a road already through it stays a road)
    blocked[k] &= ~1;
    clearable[k] = 0;
    if (cleared.size() < 200000) cleared.push_back(k | (2 << 24));   // the renderer grades it into bare earth and lifts any tree or bush off it
    return was;
}

void Map::setStructure(int x, int y, int w, int h, bool on) {
    regionDirty = true;
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
        if (!inMap(i, j)) continue;
        if (on) blocked[j * MAP_W + i] |= 2; else blocked[j * MAP_W + i] &= ~2;
    }
}
void Map::setResource(int x, int y, bool on) {
    if (!inMap(x, y)) return;
    regionDirty = true;
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

bool Map::lineClear(Vec2 a, Vec2 b, bool swim) const {
    // Sample along the segment at sub-tile spacing
    float len = dist(a, b);
    int steps = (int)(len / (TILE * 0.35f)) + 1;
    for (int i = 0; i <= steps; i++) {
        float t = i / (float)steps;
        float x = a.x + (b.x - a.x) * t, y = a.y + (b.y - a.y) * t;
        // check a small footprint so units don't scrape corners
        for (int k = 0; k < 4; k++) {
            float ox = (k & 1) ? 6.0f : -6.0f, oy = (k & 2) ? 6.0f : -6.0f;
            if (!(swim ? swimPassable(tileOf(x + ox), tileOf(y + oy)) : passable(tileOf(x + ox), tileOf(y + oy)))) return false;
        }
    }
    return true;
}

void Map::buildRegions() const {
    region.assign((size_t)MAP_W * MAP_H, 0);
    static std::vector<int> stack;
    u16 next = 0;
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        if (blocked[i] != 0 || region[i]) continue;
        if (next == 0xFFFF) break;   // (cannot happen on a real map: far fewer regions than that)
        ++next;
        stack.assign(1, i); region[i] = next;
        while (!stack.empty()) {
            int c = stack.back(); stack.pop_back();
            int cx = c % MAP_W, cy = c / MAP_W;
            if (cx > 0 && !blocked[c - 1] && !region[c - 1]) { region[c - 1] = next; stack.push_back(c - 1); }
            if (cx < MAP_W - 1 && !blocked[c + 1] && !region[c + 1]) { region[c + 1] = next; stack.push_back(c + 1); }
            if (cy > 0 && !blocked[c - MAP_W] && !region[c - MAP_W]) { region[c - MAP_W] = next; stack.push_back(c - MAP_W); }
            if (cy < MAP_H - 1 && !blocked[c + MAP_W] && !region[c + MAP_W]) { region[c + MAP_W] = next; stack.push_back(c + MAP_W); }
        }
    }
    regionDirty = false;
    cacheGen = 0;   // (closestInRegion's memo is stale now)
}

bool Map::closestInRegion(u16 rg, int gx, int gy, int& bx, int& by) const {
    // Memoized per (goal tile, region) until the regions are rebuilt: the same unreachable spot is asked for over and over (a squad chasing a ship
    // on the lake, a wave re-planning toward a walled base), and a ring search out of the middle of the lake is not cheap.
    struct Slot { u32 key = 0, gen = 0; int at = -1; };
    static Slot cache[8192];
    static u32 gen = 1;
    static const std::vector<u16>* seen = nullptr;
    if (seen != &region || cacheGen != gen) { gen++; cacheGen = gen; seen = &region; }
    u32 key = ((u32)(gy * MAP_W + gx) << 16 | rg) + 1u, h = (key * 2654435761u) >> 19;
    Slot& sl = cache[h & 8191];
    if (sl.gen == gen && sl.key == key) { if (sl.at < 0) return false; bx = sl.at % MAP_W; by = sl.at / MAP_W; return true; }
    float best = 1e30f; int found = -1;
    for (int r = 0; r <= MAP_W + MAP_H && found < 0; r++) {   // the first ring with any tile of the region: its nearest tile (close enough to the exact nearest)
        for (int dy = -r; dy <= r; dy++) {
            int step = (dy == -r || dy == r) ? 1 : 2 * r;   // whole top and bottom rows, only the two ends of the rows between
            for (int dx = -r; dx <= r; dx += std::max(1, step)) {
                int x = gx + dx, y = gy + dy;
                if (!inMap(x, y) || region[y * MAP_W + x] != rg) continue;
                float d = (float)(std::max(std::abs(dx), std::abs(dy)) + 0.4142f * std::min(std::abs(dx), std::abs(dy)));
                if (d < best) { best = d; found = y * MAP_W + x; }
            }
        }
    }
    sl.key = key; sl.gen = gen; sl.at = found;
    if (found < 0) return false;
    bx = found % MAP_W; by = found / MAP_W;
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

bool Map::findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes, bool swim) const {
    auto open_ = [&](int x, int y) { return swim ? swimPassable(x, y) : passable(x, y); };
    out.clear();
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || !std::isfinite(to.x) || !std::isfinite(to.y) || maxNodes <= 0) return false;
    to = Vec2(clampf(to.x, 0, WORLD_W - 1.0f), clampf(to.y, 0, WORLD_H - 1.0f));
    int sx = clampi(tileOf(from.x), 0, MAP_W - 1), sy = clampi(tileOf(from.y), 0, MAP_H - 1);
    int gx = clampi(tileOf(to.x), 0, MAP_W - 1), gy = clampi(tileOf(to.y), 0, MAP_H - 1);
    if (!open_(sx, sy)) { Vec2 nf = nearestFree(from); sx = tileOf(nf.x); sy = tileOf(nf.y); }
    if (!open_(sx, sy)) return false;
    if (sx == gx && sy == gy) { out.push_back(to); return true; }
    if (!swim) {   // a spot in another region of ground: no flood, the closest point of our own region instead (see Map::region)
        if (regionDirty) buildRegions();
        u16 rs = region[sy * MAP_W + sx];
        if (rs != 0 && region[gy * MAP_W + gx] != rs) {
            int bx = sx, by = sy;
            if (closestInRegion(rs, gx, gy, bx, by) && !(bx == sx && by == sy)) findPath(from, tileCenter(bx, by), out, maxNodes, false);
            return false;
        }
    }
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
            if (!open_(nx, ny)) continue;
            if (d >= 4 && (!open_(x + DX[d], y) || !open_(x, y + DY[d]))) continue; // no corner cutting
            int ni = ny * MAP_W + nx;
            if (closedGen[ni] == curGen) continue;
            float step = (d >= 4 ? 1.4142f : 1.0f) * (swim && tiles[ni] == T_WATER ? SWIM_COST : moveCost(nx, ny));
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
    pullString(from, rev, out, swim);
    return found;
}

// String pulling: skip the waypoints reachable in a straight line, looking at most PULL_AHEAD tiles along the route at a time (testing every
// later waypoint of a long route against the start made the pass quadratic in the path length: an eighth of a crowded tick)
void Map::pullString(Vec2 from, const std::vector<Vec2>& rev, std::vector<Vec2>& out, bool swim) const {
    const size_t PULL_AHEAD = 14;
    Vec2 cur = from;
    size_t i = 0;
    while (i < rev.size()) {
        size_t j = std::min(rev.size() - 1, i + PULL_AHEAD);
        while (j > i && !lineClear(cur, rev[j], swim)) j--;
        out.push_back(rev[j]);
        cur = rev[j];
        i = j + 1;
    }
}

bool Map::findPathToRect(Vec2 from, int rx, int ry, int rw, int rh, std::vector<Vec2>& out, int maxNodes, bool swim) const {
    auto open_ = [&](int x, int y) { return swim ? swimPassable(x, y) : passable(x, y); };
    out.clear();
    if (!std::isfinite(from.x) || !std::isfinite(from.y) || maxNodes <= 0 || rw <= 0 || rh <= 0) return false;
    int sx = clampi(tileOf(from.x), 0, MAP_W - 1), sy = clampi(tileOf(from.y), 0, MAP_H - 1);
    if (!open_(sx, sy)) { Vec2 nf = nearestFree(from); sx = tileOf(nf.x); sy = tileOf(nf.y); }
    if (!open_(sx, sy)) return false;
    // goal: a free tile in the ring round the footprint (its eight neighbours' band)
    auto isGoal = [&](int x, int y) { return passable(x, y) && x >= rx - 1 && x <= rx + rw && y >= ry - 1 && y <= ry + rh && !(x >= rx && x < rx + rw && y >= ry && y < ry + rh); };
    if (isGoal(sx, sy)) { out.push_back(tileCenter(sx, sy)); return true; }
    if (!swim) {   // no side of it in our region of ground: the closest point of ours instead of a flood (see Map::region)
        if (regionDirty) buildRegions();
        u16 rs = region[sy * MAP_W + sx];
        bool any = false;
        for (int y = ry - 1; y <= ry + rh && !any; y++) for (int x = rx - 1; x <= rx + rw; x++) if (inMap(x, y) && isGoal(x, y) && region[y * MAP_W + x] == rs) { any = true; break; }
        if (rs != 0 && !any) {
            int bx = sx, by = sy;
            if (closestInRegion(rs, rx + rw / 2, ry + rh / 2, bx, by) && !(bx == sx && by == sy)) findPath(from, tileCenter(bx, by), out, maxNodes, false);
            return false;
        }
    }
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
            if (!open_(nx, ny)) continue;
            if (d >= 4 && (!open_(x + DX[d], y) || !open_(x, y + DY[d]))) continue; // no corner cutting
            int ni = ny * MAP_W + nx;
            if (closedGen[ni] == curGen) continue;
            float ng = gScore[n.idx] + (d >= 4 ? 1.4142f : 1.0f) * (swim && tiles[ni] == T_WATER ? SWIM_COST : moveCost(nx, ny));
            if (visitGen[ni] != curGen || ng < gScore[ni]) { visitGen[ni] = curGen; gScore[ni] = ng; parentIdx[ni] = n.idx; open.push({ng + h(ni), ni}); }
        }
    }
    std::vector<Vec2> rev;
    for (int i = best; i != -1 && i != start; i = parentIdx[i]) rev.push_back(tileCenter(i % MAP_W, i / MAP_W));
    if (rev.empty()) return false;
    std::reverse(rev.begin(), rev.end());
    pullString(from, rev, out, swim);
    return found;
}

// ---- the lake
Vec2 Map::nearestWater(Vec2 p, int maxR, bool bigOnly, int comp) const {
    int tx = clampi(tileOf(p.x), 0, MAP_W - 1), ty = clampi(tileOf(p.y), 0, MAP_H - 1);
    if (navOpen(tx, ty) && (!bigOnly || bigWater(tx, ty)) && (comp == 0 || waterComp[ty * MAP_W + tx] == comp)) return p;
    for (int pass = 0; pass < 2; pass++)   // open water first; a tile with only a narrow channel is the fallback
        for (int r = 1; r <= maxR; r++) {
            float best = 1e18f; int bx = -1, by = -1;
            for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx += (dy == -r || dy == r) ? 1 : 2 * r) {   // the ring only, in row order (the old full-square scan was O(R^3))
                int x = tx + dx, y = ty + dy;
                if (pass == 0 ? !navOpen(x, y) : !navTile(x, y)) continue;
                if (bigOnly && !bigWater(x, y)) continue;
                if (comp != 0 && waterComp[y * MAP_W + x] != comp) continue;
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
    Vec2 sf = nearestWater(from, 6);
    int comp = compAt(sf);   // (the goal is moved onto the same body of water: the lake and the ocean do not meet)
    Vec2 gt = nearestWater(to, 14, true, comp);
    if (!navTile(tileOf(gt.x), tileOf(gt.y)) || compAt(gt) != comp) gt = nearestWater(to, 60, true, comp);   // a goal far inland: head for the closest point of this sea
    if (!navTile(tileOf(gt.x), tileOf(gt.y)) || compAt(gt) != comp) gt = nearestWater(to, 150, true, comp);
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
