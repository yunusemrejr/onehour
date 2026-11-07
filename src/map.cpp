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
    // the canal's mouths: a broad headland of firm ground either side of each estuary, so the delta and its banks always stand clear of the beach
    float mouth = std::max(0.0f, 1.0f - std::fabs(v - W * 0.5f) / 44.0f);
    if (cu < 44.0f) { float k = mouth * std::max(0.0f, 1.0f - cu / 44.0f); wob = wob * (1.0f - k) + 9.0f * k; }
    return d - 4.0f + wob;
}
// The same for the two jungle islands: squarish ovals (superellipses, so the flat top has room to build on) with a gently wandering coast (roughly
// in tiles); the nearer island answers
float seaIslandInland(int x, int y) {
    const u32 SEED = 1979;
    float cx = (x + 0.5f) > MAP_W * 0.5f ? SEA_ISLE_X : SEA_ISLE_XW;
    float dx = std::fabs(x + 0.5f - cx) / SEA_ISLE_RX, dy = std::fabs(y + 0.5f - MAP_H * 0.5f) / SEA_ISLE_RY;
    float e = std::cbrt(dx * dx * dx + dy * dy * dy);
    float wob = (fbm(x * 0.08f + 11, y * 0.08f + 23, SEED + 420) - 0.5f) * 0.18f + (fbm(x * 0.21f + 5, y * 0.21f + 77, SEED + 421) - 0.5f) * 0.05f;
    return (1.0f + wob - e) * SEA_ISLE_RX;
}
// ... and for the two secret islands
static float secretIslandInland(int x, int y) {
    const u32 SEED = 1979;
    float cy = (y + 0.5f) > MAP_H * 0.5f ? MAP_H - SECRET_ISLE_Y : SECRET_ISLE_Y;
    float dx = std::fabs(x + 0.5f - MAP_W * 0.5f) / SECRET_ISLE_RX, dy = std::fabs(y + 0.5f - cy) / SECRET_ISLE_RY;
    float e = std::sqrt(dx * dx + dy * dy);
    float wob = (fbm(x * 0.13f + 51, y * 0.13f + 17, SEED + 430) - 0.5f) * 0.28f;
    return (1.0f + wob - e) * SECRET_ISLE_RY;
}

void Map::generate() {
    const u32 SEED = 1979;
    regionDirty = true; navGen++; navAllDirty = true; navStaticGen++;
    // The arena's routes scale with the main island (the old map, ISLE_W tiles square, centred in the sea); structures, road widths and unit
    // silhouettes stay at their original readable size. Mirrors and rotations about the map centre are mirrors about the island's centre too.
    const float G = ISLE_W / 104.0f;
    const float S = ISLE_W / 80.0f;
    const float C = MAP_W * 0.5f;   // map centre = the island's centre, tile units
    const int O = ISLE_X0;          // the island's first tile: layout coordinates of the old map are offset by it
    // The lake country (the lake, its beach and islands, the ring road round it, the shore supply piles, the pools and the old strongholds between
    // the lake and the ring road's corners) keeps its size on the bigger island (LAKE_K scales it about the centre on top of the map's own scale):
    // the room the island gained lies between the bases and the ring road.
    const float LAKE_K = 1.0f, GL = G * LAKE_K;
    const float ISL = GL / (146.0f / 104.0f);   // island sizes and offsets were laid out for the 146 map's lake
    auto inner = [&](float v104) { return C + (v104 - 52.0f) * GL; };   // a lake-country coordinate of the 104-tile layout, in tiles
    auto sc = [&](int v) { return O + (int)std::lround(v * S); };
    roads.clear(); relics.clear(); bridgeSpots.clear();
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
    // toward the supply piles on the axes; a sandy beach rings it and two islands and some rocky islets break the open water. The canal runs into it
    // from the west and leaves it to the east: it is one sea with the ocean, and ships sail in under the bridges.
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

    // Rock ridges between the two bases of each half of the island (north and south), each with a gap (chokepoint); the canal divides the halves
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
    // A Mountain Fortress is set into each ridge, in the long stretch of mountain between the map edge and the pass: the ridge rock is cut back
    // round the fortress so each of the two armies on either side of the ridge has a doorstep onto it, but the fortress itself still closes
    // the ridge (it is no second pass).
    std::vector<LandmarkSpot> forts;
    {
        const int FW = 5, FH = 4;   // BUILDS[B_C_FORTRESS] footprint
        int axis = (sc(38) + sc(41)) / 2, segA = O + 2, segB = sc(10) - 1;   // the ridge stretch between the map edge and the pass
        int along = (segA + segB) / 2;
        struct F { int x, y; bool vertical; } F2[2] = { { axis - FW / 2, along - FH / 2, true }, { axis - FW / 2, (MAP_H - 1) - along - FH / 2, true } };
        for (auto& f : F2) {
            // the footprint and a doorstep two tiles deep on the two open faces become worn ground
            int x0 = f.x - 2, x1 = f.x + FW - 1 + 2, y0 = f.y, y1 = f.y + FH - 1;
            for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (inMap(x, y)) tiles[y * MAP_W + x] = T_DIRT;
            forts.push_back({ f.x, f.y, 5 });
        }
    }
    // Rock outcrops guarding the diagonal approaches to the center
    {   // (rocky headlands on the lake's shore)
        const float hx = (25.7f - 40.0f) * S * LAKE_K * (208.0f / ISLE_W) * 1.17f, hy = (32.3f - 40.0f) * S * LAKE_K * (208.0f / ISLE_W) * 1.17f, hr = 2.4f * (208.0f / 80.0f);
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
    const float ln = sc(12), lf = (MAP_W - 1) - sc(12) - 1;
    {
        float dn = sc(13), df = (MAP_W - 1) - dn;   // just outside each Command Core
        paintRoad(*this, dn, dn, rx0, rx0, 1.5f);
        paintRoad(*this, df, dn, rx1, rx0, 1.5f);
        paintRoad(*this, dn, df, rx0, rx1, 1.5f);
        paintRoad(*this, df, df, rx1, rx1, 1.5f);
        // Lateral roads: through the ridge passes (north and south), and down each side of the island over the canal's bridges (west and east)
        paintRoad(*this, dn, ln, df, ln, 1.3f);
        paintRoad(*this, dn, lf + 1, df, lf + 1, 1.3f);
        paintRoad(*this, ln, dn, ln, df, 1.3f);
        paintRoad(*this, lf + 1, dn, lf + 1, df, 1.3f);
    }

    // ---- The canal. Ten tiles of water (CANAL_Y0..CANAL_Y1) run from the lake to the west coast and from the lake to the east coast, so the island is
    // two halves, joined only by the bridges, and the lake is open to the ocean at both ends. Near each mouth the canal splits round a delta island
    // (an abandoned relic outpost stands on it and on both banks beside it); seawards of the island it opens into a broad estuary. Bridges: the
    // ring road's, the lateral road's, and one from each bank onto the delta island - all north-south decks BRIDGE_W x BRIDGE_L.
    {
        const int DI0 = O + 8, DI1 = O + 26;                     // the delta island's west and east columns (on the west side; mirrored for the east)
        const int NCH0 = (int)C - 17, NCH1 = (int)C - 8;         // the north channel's rows round the island
        const int SCH0 = (int)C + 7, SCH1 = (int)C + 16;         // the south channel's
        const int ISL0 = (int)C - 7, ISL1 = (int)C + 6;          // the island's rows
        const int TAPER = 7;                                     // columns over which the two channels merge again upstream of the island
        const int bridgeX[4] = { O + 16, O + 16, (int)std::lround(ln) - 1, (int)std::lround(rx0) - 1 };   // west side: delta north, delta south, lateral road, ring road
        auto nearBridge = [&](int xw) { for (int b = 2; b < 4; b++) if (std::abs(xw - (bridgeX[b] + 1)) <= 4) return true; return std::abs(xw - (bridgeX[0] + 1)) <= 4; };
        for (int side = 0; side < 2; side++) {
            auto X = [&](int xw) { return side ? (MAP_W - 1) - xw : xw; };   // a west-side column, on this side
            for (int xw = 0; xw <= (int)C; xw++) {
                int x = X(xw);
                if (xw > DI1 + TAPER && tiles[(int)C * MAP_W + x] == T_WATER) break;   // (the lake: the canal has reached it)
                int n0, n1, s0, s1;   // water rows: [n0, n1] and [s0, s1] (one span when they meet)
                if (xw < DI0) { n0 = NCH0 - std::max(0, (DI0 - xw)) / 2; n1 = SCH1 + std::max(0, (DI0 - xw)) / 2; s0 = s1 = -1; }   // the estuary widens seawards
                else if (xw <= DI1) { n0 = NCH0; n1 = NCH1; s0 = SCH0; s1 = SCH1; }
                else if (xw <= DI1 + TAPER) {   // the channels close in on the canal's own rows; the island's tip is rounded off
                    float k = (xw - DI1) / (float)TAPER;
                    n0 = (int)std::lround(NCH0 + (CANAL_Y0 - NCH0) * k); s1 = (int)std::lround(SCH1 + (CANAL_Y1 - SCH1) * k);
                    if (xw <= DI1 + 2) { n1 = NCH1 + (xw - DI1); s0 = SCH0 - (xw - DI1); }   // (the tip)
                    else { n1 = s1; s0 = s1 = -1; }
                } else { n0 = CANAL_Y0; n1 = CANAL_Y1; s0 = s1 = -1; }
                // the outer banks wander a little (never at a bridge: its deck spans exactly ten tiles of water)
                if (!nearBridge(xw) && xw > DI1 + TAPER) {
                    float wn = fbm(x * 0.11f + 13, 3.0f, SEED + 441), ws = fbm(x * 0.11f + 71, 9.0f, SEED + 442);
                    n0 -= (int)std::lround((wn - 0.5f) * 3.0f + 0.4f); n1 += (int)std::lround((ws - 0.5f) * 3.0f + 0.4f);
                }
                auto water = [&](int a, int b) { for (int y = a; y <= b; y++) if (inMap(x, y)) tiles[y * MAP_W + x] = T_WATER; };
                auto bank = [&](int y) { if (inMap(x, y)) { u8& t = tiles[y * MAP_W + x]; if (t == T_GRASS || t == T_GRASS2 || t == T_DIRT || t == T_TREE) t = T_SAND; } };
                bank(n0 - 1); if (hash2(x, n0, SEED + 443) < 0.6f) bank(n0 - 2);
                bank((s1 >= 0 ? s1 : n1) + 1); if (hash2(x, n1, SEED + 444) < 0.6f) bank((s1 >= 0 ? s1 : n1) + 2);
                water(n0, n1);
                if (s0 >= 0) water(s0, s1);
            }
            // the delta island itself: grassland with a sandy rim (the outpost's ground is cleared later)
            for (int y = ISL0; y <= ISL1; y++) for (int xw = DI0; xw <= DI1 + 1; xw++) {
                int x = X(xw);
                bool rim = y == ISL0 || y == ISL1 || xw == DI0 || xw == DI1 + 1;
                tiles[y * MAP_W + x] = rim ? T_SAND : (hash2(x, y, SEED + 445) < 0.55f ? T_GRASS2 : T_DIRT);
            }
            // the bridges of this side (decks over the water; their ends rest on a bank tile)
            const int bys[4] = { NCH0 - 1, SCH0 - 1, CANAL_Y0 - 1, CANAL_Y0 - 1 };
            for (int b = 0; b < 4; b++) {
                int bx = side ? MAP_W - bridgeX[b] - BRIDGE_W : bridgeX[b], by = bys[b];
                bridgeSpots.push_back({ bx, by });
                for (int j = 0; j < BRIDGE_L; j++) for (int i = 0; i < BRIDGE_W; i++) {
                    int x = bx + i, y = by + j, k = y * MAP_W + x;
                    if (!inMap(x, y)) continue;
                    if (j == 0 || j == BRIDGE_L - 1) { if (tiles[k] == T_WATER || tiles[k] == T_SAND) tiles[k] = T_ROAD; }   // (the abutments: a road tile on each bank)
                    else tiles[k] = T_WATER;   // (the deck's span: exactly ten tiles of water)
                    bridge[k] = 1;
                }
            }
        }
    }

    // Two islands in the lake (painted after the canal). The big one in the middle: a sandy shore round a grassy top with a stand of trees, two
    // Island Nuke Launchers and an old watchtower. The small one lies off the east shore (well out in the water, not a wade away) with its own launchers.
    // Nothing drives to either: infantry and dozers are airlifted or shipped in, so they are prizes for whoever holds the water and the sky.
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
    paintDisc(*this, C + 6.0f * ISL, C - 17.0f * ISL, 1.5f, T_ROCK);

    // ---- the sea. The old map is the main island: its coast wanders a few tiles inside the old border (headlands push out a little), the corners
    // are rounded off, and a beach or a band of cliffs rings it (where the coast is high, and wherever a ridge runs into the sea). The canal keeps
    // flowing out through the beach: its mouths open into the ocean.
    auto coastBand = [&](int x, int y, float inland, float beachW, float cliffK) {   // a land tile 'inland' tiles from the water: beach, cliff or as it is
        u8& t = tiles[y * MAP_W + x];
        if (inland >= beachW || t == T_WATER || t == T_ROAD) return;   // (the canal runs on out to its mouth; no road reaches the beach)
        bool cliff = t == T_ROCK || fbm(x * 0.06f + 90, y * 0.06f + 40, SEED + 411) > cliffK;
        t = cliff ? T_ROCK : T_SAND;
    };
    fresh.assign((size_t)MAP_W * MAP_H, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float d = mainIslandInland(x, y);
        fresh[(size_t)y * MAP_W + x] = d > 0.5f;   // (water inside the coast is the lake's and the canal's: fresh water)
        if (d <= 0) { if (!bridge[y * MAP_W + x]) tiles[y * MAP_W + x] = T_WATER; continue; }
        if (bridge[y * MAP_W + x]) continue;
        bool canalMouth = std::fabs(y + 0.5f - C) < 24.0f && (x < O + 30 || x > MAP_W - O - 30);   // (the estuary banks stay sand, never cliffs: boats land there)
        coastBand(x, y, d, 1.7f + 1.6f * fbm(x * 0.11f + 7, y * 0.11f + 70, SEED + 412), canalMouth ? 2.0f : 0.55f);
    }
    // The two jungle islands out in the sea, west and east, midway between the main island and the map's edge: a beach all round (rocky at a few
    // headlands), thick jungle, and in the middle a clearing where an abandoned relic base stands, overgrown, with a trail to the north and south
    // beaches where cargo ships can land. Then the two secret islands north and south: a ring of beach and a dome of jungle round a hidden bunker.
    seaIslands.clear();
    std::vector<std::pair<int, int>> trailTiles;   // (the islands' trails: no trees grow on them)
    for (int side = 0; side < 2; side++) {
        const float SX = side ? SEA_ISLE_XW : SEA_ISLE_X, SY = C, RX = SEA_ISLE_RX, RY = SEA_ISLE_RY;
        seaIslands.push_back({ SX, SY, RX, RY });
        const int x0 = std::max(0, (int)(SX - RX - 4)), x1 = std::min(MAP_W - 1, (int)(SX + RX + 4));
        const int y0 = std::max(0, (int)(SY - RY - 4)), y1 = std::min(MAP_H - 1, (int)(SY + RY + 4));
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            float d = seaIslandInland(x, y);
            if (d <= 0) continue;
            u8 t;
            float beachW = 1.4f + 1.4f * fbm(x * 0.17f + 3, y * 0.17f + 9, SEED + 450);
            if (d < beachW) t = fbm(x * 0.09f + 31, y * 0.09f + 13, SEED + 451) > 0.66f ? T_ROCK : T_SAND;   // beach, and a rocky headland here and there
            else {
                float n = fbm(x * 0.09f + 70, y * 0.09f + 40, SEED + 452);
                t = n > 0.58f ? T_DIRT : (n < 0.42f ? T_GRASS2 : T_GRASS);
                // jungle: thick everywhere but the clearing round the base, the trail and a few glades
                float glade = fbm(x * 0.12f + 9, y * 0.12f + 90, SEED + 453);
                bool clearing = std::fabs(x + 0.5f - SX) < 15.5f && std::fabs(y + 0.5f - SY) < 16.5f;
                if (!clearing && d > beachW + 1.0f && glade < 0.62f) t = T_TREE;
                if (clearing && hash2(x, y, SEED + 454) < 0.05f && (std::fabs(x + 0.5f - SX) > 13.0f || std::fabs(y + 0.5f - SY) > 14.0f)) t = T_TREE;   // (the jungle creeps back in at the clearing's edge)
                // a rocky hill at each end of the island
                float hill = std::hypot((x + 0.5f - SX) / 5.0f, (std::fabs(y + 0.5f - SY) - RY * 0.62f) / 4.0f);
                if (hill < 1.0f && fbm(x * 0.3f, y * 0.3f, SEED + 455) > 0.38f) t = T_ROCK;
            }
            tiles[y * MAP_W + x] = t;
        }
        // the trail: from the clearing north and south to the beaches (worn earth, one tile wide, wandering a little)
        for (int dir = -1; dir <= 1; dir += 2) for (float s = 15.0f; s < RY + 2.0f; s += 0.5f) {
            float tx = SX + 2.2f * std::sin(s * 0.21f + side + dir), ty = SY + dir * s;
            int ix = (int)tx, iy = (int)ty;
            if (!inMap(ix, iy) || tiles[iy * MAP_W + ix] == T_WATER) break;
            for (int k = 0; k <= 1; k++) if (inMap(ix + k, iy) && tiles[iy * MAP_W + ix + k] != T_WATER && tiles[iy * MAP_W + ix + k] != T_SAND) { tiles[iy * MAP_W + ix + k] = T_DIRT; trailTiles.push_back({ ix + k, iy }); }
        }
    }
    for (int side = 0; side < 2; side++) {
        const float SY = side ? MAP_H - SECRET_ISLE_Y : SECRET_ISLE_Y;
        seaIslands.push_back({ C, SY, SECRET_ISLE_RX, SECRET_ISLE_RY });
        for (int y = (int)(SY - SECRET_ISLE_RY - 3); y <= (int)(SY + SECRET_ISLE_RY + 3); y++) for (int x = (int)(C - SECRET_ISLE_RX - 3); x <= (int)(C + SECRET_ISLE_RX + 3); x++) {
            if (!inMap(x, y)) continue;
            float d = secretIslandInland(x, y);
            if (d <= 0) continue;
            u8 t = d < 1.3f + 0.8f * fbm(x * 0.2f, y * 0.2f, SEED + 460) ? T_SAND : T_GRASS2;
            bool clearing = std::fabs(x + 0.5f - C) < 6.5f && std::fabs(y + 0.5f - SY) < 3.6f;
            bool trail = std::fabs(x + 0.5f - C) < 1.1f && (side ? y + 0.5f < SY : y + 0.5f > SY);   // (a track from the clearing down to the beach facing the main island)
            if (t != T_SAND && !clearing && !trail && hash2(x, y, SEED + 461) < 0.82f) t = T_TREE;
            if (trail && t != T_SAND) t = T_DIRT;
            tiles[y * MAP_W + x] = t;
            if (trail) trailTiles.push_back({ x, y });
        }
    }
    // Islets scattered round the sea: sand cays with a few trees, bare rock stacks. Kept well off the islands' coasts and the edge of the map, and few
    // (the open sea far from any land is baked at half resolution: every islet costs a little memory)
    {
        Rng ir(SEED + 4401);
        std::vector<Island> got;
        for (int attempt = 0; attempt < 4000 && got.size() < 12; attempt++) {
            float x = ir.f(14.0f, MAP_W - 14.0f), y = ir.f(14.0f, MAP_H - 14.0f), r = ir.f(1.6f, 3.8f);
            if (mainIslandInland((int)x, (int)y) > -12.0f || seaIslandInland((int)x, (int)y) > -12.0f || secretIslandInland((int)x, (int)y) > -12.0f) continue;
            bool near = false;
            for (auto& o2 : got) if (std::hypot(o2.cx - x, o2.cy - y) < 40.0f) near = true;
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
    // ground the jungle has taken back: the jungle islands and the secret islands all over, the relic outposts on the main island (filled in below)
    overgrown.assign((size_t)MAP_W * MAP_H, 0);
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) if (tiles[y * MAP_W + x] != T_WATER && (seaIslandInland(x, y) > 0 || secretIslandInland(x, y) > 0)) overgrown[(size_t)y * MAP_W + x] = 1;
    // The relic outposts' ground on the main island: the delta islands are cleared already; the banks beside them get worn clearings the outposts stand in
    auto relicClearing = [&](int x0, int y0, int x1, int y1) {
        for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
            if (!inMap(x, y)) continue;
            u8& t = tiles[y * MAP_W + x];
            if (t == T_WATER || t == T_ROAD || bridge[y * MAP_W + x]) continue;
            if (t == T_TREE || t == T_ROCK || t == T_GRASS) t = hash2(x, y, SEED + 470) < 0.5f ? T_GRASS2 : T_DIRT;
            overgrown[(size_t)y * MAP_W + x] = 1;
        }
    };
    for (int side = 0; side < 2; side++) for (int y = (int)C - 7; y <= (int)C + 6; y++) for (int xw = O + 8; xw <= O + 27; xw++) { int x = side ? MAP_W - 1 - xw : xw; if (tiles[y * MAP_W + x] != T_WATER) overgrown[(size_t)y * MAP_W + x] = 1; }   // (the delta islands)
    for (int side = 0; side < 2; side++) for (int bankS = 0; bankS < 2; bankS++) {
        int xa = O + 5, xb = O + 32, ya = bankS ? (int)C + 18 : (int)C - 36, yb = bankS ? (int)C + 35 : (int)C - 19;
        if (side) { int t0 = MAP_W - 1 - xb; xb = MAP_W - 1 - xa; xa = t0; }
        relicClearing(xa, ya, xb, yb);
    }
    // Tree clusters from noise, kept off roads, bases, the shore and the relic outposts
    auto relicGround = [&](int x, int y) {   // inside one of the main island's relic outposts (the deltas and their banks)
        bool w = x < O + 34, e = x > MAP_W - O - 35;
        return (w || e) && std::fabs(y + 0.5f - C) < 37.0f && mainIslandInland(x, y) > 0;
    };
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        u8& t = tiles[y * MAP_W + x];
        if (!isGrass(t)) continue;
        if (mainIslandInland(x, y) <= 0) continue;   // (the islands out at sea have their own jungle)
        float n = fbm(x * 0.13f + 40, y * 0.13f + 40, SEED + 31);
        bool nearBase = false;
        for (auto& s : starts) if (std::abs(x - s.tx) < 15 && std::abs(y - s.ty) < 15) nearBase = true;
        float dc = std::sqrt((x - C) * (x - C) + (y - C) * (y - C));
        if (nearBase || dc < 24 * GL || relicGround(x, y)) continue;
        // keep roads' neighbors clear
        bool nearRoad = false;
        for (int dy = -1; dy <= 1 && !nearRoad; dy++) for (int dx = -1; dx <= 1; dx++)
            if (tiles[(y + dy) * MAP_W + x + dx] == T_ROAD) { nearRoad = true; break; }
        if (nearRoad) continue;
        if (n > 0.63f) t = T_TREE;
    }
    // ... but the jungle grows right up to the relic outposts' walls: a fringe of trees round each clearing
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        u8& t = tiles[y * MAP_W + x];
        if (!isGrass(t) && t != T_DIRT) continue;
        if (!relicGround(x, y)) continue;
        bool edge = !relicGround(x - 3, y) || !relicGround(x + 3, y) || !relicGround(x, y - 3) || !relicGround(x, y + 3);
        if (edge && hash2(x, y, SEED + 471) < 0.45f) t = T_TREE;
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
                if (near || std::abs(y - (int)C) < 9) continue;   // (and never on the canal's banks)
                if (fbm(x * 0.35f + 9, y * 0.35f + 4, SEED + 47) > 0.38f + 0.25f * d) t = T_TREE;
            }
        }
    }
    // Supply piles: two per base, the lake shore's, the lateral gaps', and contested ones in the quadrant woods (the canal's banks have two each
    // side where the old west and east piles stood)
    supplies = {
        {8, 21, 36000}, {21, 8, 36000},
        {95, 21, 36000}, {82, 8, 36000},
        {8, 82, 36000}, {21, 95, 36000},
        {95, 82, 36000}, {82, 95, 36000},
        {52, 29, 50000}, {52, 75, 50000},
        {46, 8, 36000}, {57, 95, 36000},
        {18, 33, 30000}, {70, 18, 30000}, {85, 70, 30000}, {33, 85, 30000},
        {57, 8, 36000}, {46, 95, 36000},   // (the old west and east lateral piles stood where the deltas are now: these, across the north and south
                                           // ridges from the piles above, give the north-east and south-west bases theirs back)
    };
    for (size_t k = 0; k < supplies.size(); k++) {
        auto& supply = supplies[k];
        bool shore = k >= 8 && k < 10;   // the two on the lake shore move out with the lake country
        supply.tx = shore ? (MAP_W - 1) / 2 : O + (int)std::lround((supply.tx + 0.5f) * G - 0.5f);   // (the shore ones right on the middle line: as near the western bases as the eastern)
        supply.ty = shore ? (int)std::lround(inner(supply.ty + 0.5f) - 0.5f) : O + (int)std::lround((supply.ty + 0.5f) * G - 0.5f);
    }
    {   // the lake's west and east mouths: a pile on each bank, between the lateral road's bridge and the ring road's
        int px = (int)std::lround((ln + rx0) * 0.5f);
        supplies.push_back({ px, CANAL_Y0 - 8, 34000 }); supplies.push_back({ px, CANAL_Y1 + 8, 34000 });
        supplies.push_back({ MAP_W - 1 - px, CANAL_Y0 - 8, 34000 }); supplies.push_back({ MAP_W - 1 - px, CANAL_Y1 + 8, 34000 });
    }
    // two rich piles on each jungle island, north and south of the relic base (nobody's: whoever ships a hauler and a Supply Hub over gets them)
    for (int side = 0; side < 2; side++) {
        int sx = (int)(side ? SEA_ISLE_XW : SEA_ISLE_X);
        supplies.push_back({ sx - 3, (int)C - 24, 50000 });
        supplies.push_back({ sx + 2, (int)C + 23, 50000 });
    }
    // Clear the ground around supply piles and bases so nothing important is walled in
    for (auto& s : supplies) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 3.4f, T_DIRT, [](u8 t) { return t == T_TREE || t == T_ROCK || t == T_WATER; });
    for (auto& s : starts) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 11.5f, T_GRASS, [](u8 t) { return t == T_TREE || t == T_ROCK; });
    for (auto& t : trailTiles) if (tiles[t.second * MAP_W + t.first] == T_TREE) tiles[t.second * MAP_W + t.first] = T_DIRT;

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
    // cliffs: nothing could ever walk in or out. (The lake's islands are cut off by design, and so is everything out at sea and on the deltas.)
    {
        static int labC[MAP_W * MAP_H];
        memset(labC, 0, sizeof(labC));
        auto open = [&](int i) { u8 t = tiles[i]; return (t != T_WATER || bridge[i]) && t != T_ROCK && t != T_TREE; };
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
            if (mainIslandInland(x, y) <= 0) continue;   // (out at sea: the jungle islands, the secret islands and the islets)
            bool lakeIsle = false;
            for (auto& isl : islands) if ((x - isl.cx) * (x - isl.cx) + (y - isl.cy) * (y - isl.cy) <= (isl.r + 3) * (isl.r + 3)) lakeIsle = true;
            if (lakeIsle || mainIslandInland(x, y) > 14.0f || relicGround(x, y)) continue;   // (only the coast: pockets inland are the forests' business, see above)
            tiles[i] = T_ROCK;
        }
    }
    // Blocked flags (a bridge's deck is water with a road over it: passable while the bridge stands)
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = tiles[i];
        blocked[i] = ((t == T_WATER && !bridge[i]) || t == T_ROCK || t == T_TREE) ? 1 : 0;
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
    // bodies of water (4-connected; a bridge's deck is water too: the canal runs on under it)
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
    // landmasses (4-connected land, bridge decks included: the two halves of the main island are one landmass while the bridges stand, and stay one
    // in this map even while they are broken - they are raised again): the main island, each sea island, each islet, the deltas
    landComp.assign((size_t)MAP_W * MAP_H, 0);
    {
        auto land = [&](int i) { return tiles[i] != T_WATER || bridge[i]; };
        int id = 0;
        for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
            if (!land(y * MAP_W + x) || landComp[y * MAP_W + x]) continue;
            id++;
            std::vector<int> stack{ y * MAP_W + x };
            landComp[y * MAP_W + x] = (u16)id;
            while (!stack.empty()) {
                int i = stack.back(); stack.pop_back();
                int ix = i % MAP_W, iy = i / MAP_W;
                static const int DX4[4] = {1, -1, 0, 0}, DY4[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; k++) {
                    int nx = ix + DX4[k], ny = iy + DY4[k];
                    if (!inMap(nx, ny) || !land(ny * MAP_W + nx) || landComp[ny * MAP_W + nx]) continue;
                    landComp[ny * MAP_W + nx] = (u16)id; stack.push_back(ny * MAP_W + nx);
                }
            }
        }
    }
    // the ocean is the biggest body of water; the lake is the water at the map's centre (the canal joins it to the ocean: then they are one sea)
    oceanComp = 0;
    for (int i = 1; i < (int)waterSize.size(); i++) if (waterSize[i] > waterSize[oceanComp]) oceanComp = i;
    {
        std::vector<int> near(waterSize.size(), 0);
        const float LR = 22.0f * GL;
        for (int y = (int)(C - LR); y <= (int)(C + LR); y++) for (int x = (int)(C - LR); x <= (int)(C + LR); x++)
            if (inMap(x, y) && (x - C) * (x - C) + (y - C) * (y - C) <= LR * LR) near[waterComp[y * MAP_W + x]]++;
        lakeComp = 0;
        for (int i = 1; i < (int)near.size(); i++) if (near[i] > near[lakeComp]) lakeComp = i;
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
    // Interior water grouped for the overlay passes (see map.h): the ocean gets the swell in aligned 8x8 blocks, then 4x4 blocks, then single tiles
    // (no seam between the coast and the open sea), and its shallows a list of tiles for the fading caustics; the lake and the canal (the water
    // inside the main island's coast: fresh water) get caustics in aligned 4x4 blocks, then whatever interior tiles remain.
    waterBlocks.clear(); waterTiles.clear(); seaBlocks.clear(); seaQuads.clear(); seaTiles.clear(); seaShallow.clear();
    {
        auto interior = [&](int x, int y) {
            if (x < 1 || y < 1 || x > MAP_W - 2 || y > MAP_H - 2) return false;
            return tiles[y * MAP_W + x] == T_WATER && tiles[y * MAP_W + x - 1] == T_WATER && tiles[y * MAP_W + x + 1] == T_WATER
                && tiles[(y - 1) * MAP_W + x] == T_WATER && tiles[(y + 1) * MAP_W + x] == T_WATER;
        };
        auto fresh = [&](int x, int y) { return freshWater(x, y); };
        auto ocean = [&](int x, int y) { return interior(x, y) && !fresh(x, y); };
        auto inland = [&](int x, int y) { return interior(x, y) && fresh(x, y); };
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
        // the lake and the canal: caustics everywhere
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
    auto landmarkFits = [&](int x, int y, int w, int h) {
        for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
            if (!buildable(i, j) || tile(i, j) == T_ROAD) return false;
            if ((blocked[j * MAP_W + i] & 2) || (blocked[j * MAP_W + i] & 4)) return false;
        }
        return !wouldPlug(x, y, w, h);
    };
    {
        static const struct { int x, y, w, h, kind; } BASE[5] = {
            { 27, 38, 4, 4, 0 },   // castle
            { 40, 28, 3, 3, 1 },   // civilian bunker
            { 37, 34, 2, 2, 2 },   // watchtower
            { 24, 39, 2, 2, 3 },   // abandoned cottage beside the castle
            { 32, 36, 3, 2, 4 },   // old roadside workshop
        };
        for (int q = 0; q < 4; q++) for (auto& L : BASE) {
            int tx = (int)std::lround(inner((float)L.x)), ty = (int)std::lround(inner((float)L.y)), w = L.w, h = L.h;
            for (int k = 0; k < q; k++) { int nx = MAP_W - ty - h, ny = tx; tx = nx; ty = ny; }   // rotate the whole set with the quadrant
            auto fits = [&](int x, int y) { return landmarkFits(x, y, w, h); };
            int bx = tx, by = ty;
            if (!fits(bx, by)) {   // the canal, rock or a narrow corridor shifted on this seed: take the nearest legal footprint
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
    }
    // ---- The abandoned relic bases. Each layout is given for the west side (the delta and its banks, the jungle island) or the north (the secret
    // island), in tiles from an anchor, and mirrored for the other; a footprint that does not fit on this map's ground moves to the nearest legal spot
    // within a few tiles, or is left out (it must never wall off a corridor). Walls (Relic Ramparts) are left out where a tile is taken.
    {
        auto relicFits = [&](int x, int y, int w, int h) {
            for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
                if (!inMap(i, j) || bridge[j * MAP_W + i] || tile(i, j) == T_WATER || tile(i, j) == T_ROAD) return false;
                if (blocked[j * MAP_W + i] & 6) return false;
                if ((tile(i, j) == T_ROCK || tile(i, j) == T_TREE) && !clearable[j * MAP_W + i]) return false;
            }
            // a tile of open ground round it (walls excepted) so nothing of the base seals a lane
            if (w > 1 || h > 1) for (int j = y - 1; j <= y + h; j++) for (int i = x - 1; i <= x + w; i++) {
                if (!inMap(i, j) || (i >= x && i < x + w && j >= y && j < y + h)) continue;
                if (blocked[j * MAP_W + i] & 6) return false;
            }
            return !wouldPlug(x, y, w, h);
        };
        auto put = [&](int type, int tx, int ty, bool secret, int reach) {
            const BuildType& b = BUILDS[type];
            int bx = -1, by = -1;
            for (int r = 0; r <= reach && bx < 0; r++) for (int dy = -r; dy <= r && bx < 0; dy++) for (int dx = -r; dx <= r; dx++) {
                if (std::max(std::abs(dx), std::abs(dy)) != r || !relicFits(tx + dx, ty + dy, b.w, b.h)) continue;
                bx = tx + dx; by = ty + dy; break;
            }
            if (bx < 0) return;
            clearForStructure(bx, by, b.w, b.h);
            setStructure(bx, by, b.w, b.h, true);
            relics.push_back({ type, bx, by, secret });
        };
        struct L { int type, dx, dy; };
        // the delta island (anchor: its west column and the map's middle row) and its two banks
        static const L DELTA[] = {
            { B_R_COMMAND, 3, -3 }, { B_R_FLAK, 12, -5 }, { B_R_FLAK, 12, 3 }, { B_R_LASER, 15, -1 },
            // north bank (rows above the north channel)
            { B_R_FOUNDRY, -1, -28 }, { B_R_REACTOR, 13, -28 }, { B_R_CANNON, 18, -23 }, { B_R_LASER, 0, -22 },
            // south bank
            { B_R_AIRSTRIP, -1, 24 }, { B_R_REACTOR, 13, 25 }, { B_R_CANNON, 18, 20 }, { B_R_FLAK, 0, 20 },
        };
        const int DI0 = O + 8;
        for (int side = 0; side < 2; side++) for (const L& l : DELTA) {
            const BuildType& b = BUILDS[l.type];
            int tx = DI0 + l.dx, ty = (int)C + l.dy;
            if (side) tx = MAP_W - tx - b.w;
            put(l.type, tx, ty, false, 4);
        }
        // the delta island's ramparts: a broken line along its seaward tip (one in three fallen)
        for (int side = 0; side < 2; side++) for (int y = (int)C - 5; y <= (int)C + 4; y++) {
            if (hash2(y, side, SEED + 480) < 0.34f) continue;
            int tx = DI0 + 1; if (side) tx = MAP_W - tx - 1;
            put(B_R_WALL, tx, y, false, 0);
        }
        // the jungle islands' bases (anchor: the island's centre tile)
        static const L ISLE[] = {
            { B_R_COMMAND, -2, -2 }, { B_R_SILO, -1, 8 }, { B_R_REACTOR, -9, -8 }, { B_R_FOUNDRY, -10, -1 }, { B_R_AIRSTRIP, 4, -8 },
            { B_R_CANNON, 6, -1 }, { B_R_FLAK, -6, 8 }, { B_R_FLAK, 6, 7 }, { B_R_LASER, -1, -9 }, { B_R_LASER, -1, 13 }, { B_R_REACTOR, 5, 11 },
        };
        for (int side = 0; side < 2; side++) {
            int cx = (int)(side ? SEA_ISLE_XW : SEA_ISLE_X), cy = (int)C;
            for (const L& l : ISLE) {
                const BuildType& b = BUILDS[l.type];
                int tx = cx + l.dx, ty = cy + l.dy;
                if (side) tx = 2 * cx - tx - b.w;
                put(l.type, tx, ty, false, 3);
            }
            // its ramparts: the old perimeter, a rectangle with gates on the trail and gaps where the wall has fallen
            for (int k = -12; k <= 12; k++) for (int e = 0; e < 4; e++) {
                int tx = e < 2 ? cx + k : cx + (e == 2 ? -13 : 13), ty = e < 2 ? cy + (e == 0 ? -15 : 16) : cy + k;
                if ((e < 2 && std::abs(k) <= 2) || (e >= 2 && std::abs(k) <= 1)) continue;   // the gates
                if (hash2(tx, ty, SEED + 481) < 0.38f) continue;   // fallen
                put(B_R_WALL, tx, ty, false, 0);
            }
        }
        // the secret islands: a Sentinel Bunker, a Hydra Silo and a flak battery under the canopy
        for (int side = 0; side < 2; side++) {
            int cy = (int)(side ? MAP_H - SECRET_ISLE_Y : SECRET_ISLE_Y), cx = (int)C;
            static const L SECRET[] = { { B_R_BUNKER, -5, -1 }, { B_R_SILO, 2, -1 }, { B_R_FLAK, -1, 1 } };
            for (const L& l : SECRET) {
                const BuildType& b = BUILDS[l.type];
                int tx = cx + l.dx, ty = cy + l.dy;
                if (side) ty = 2 * cy - ty - b.h;
                put(l.type, tx, ty, true, 2);
            }
        }
    }
    // The jungle among the ruins: single young trees on the overgrown ground between the relic structures (never touching another tree, a structure or
    // a pile, so no lane closes; a structure raised there later clears them like any tree)
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        int i = y * MAP_W + x;
        if (!overgrown[i] || blocked[i] || bridge[i] || (tiles[i] != T_GRASS && tiles[i] != T_GRASS2 && tiles[i] != T_DIRT)) continue;
        if (hash2(x, y, SEED + 490) > 0.085f) continue;
        bool crowded = false;
        for (int j = -1; j <= 1 && !crowded; j++) for (int k = -1; k <= 1; k++) { int n = (y + j) * MAP_W + x + k; if (n != i && (blocked[n] || tiles[n] == T_TREE || tiles[n] == T_ROAD)) { crowded = true; break; } }
        if (crowded || wouldPlug(x, y, 1, 1)) continue;
        tiles[i] = T_TREE; blocked[i] = 1; clearable[i] = 1;
    }
    regionDirty = true;
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
    regionDirty = true; noteNavChange(x, y, x + w - 1, y + h - 1);
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) {
        if (!inMap(i, j)) continue;
        if (on) blocked[j * MAP_W + i] |= 2; else blocked[j * MAP_W + i] &= ~2;
    }
}
void Map::setResource(int x, int y, bool on) {
    if (!inMap(x, y)) return;
    regionDirty = true; noteNavChange(x, y, x, y);
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
    // Across the canal: the straight line runs into the water and the search would flood the whole bank looking for the way round. The way round is a
    // bridge (a delta's two bridges, with the island between them): go to the middle of the best crossing that stands, then on from there - two short
    // searches that each head straight for their goal.
    if (!swim && !bridgeSpots.empty()) {
        auto side = [](int y) { return y < CANAL_Y0 - 1 ? -1 : (y > CANAL_Y1 + 1 ? 1 : 0); };
        int ss = side(sy), gs = side(gy);
        if (ss != 0 && gs != 0 && ss != gs && landComp.size() == (size_t)MAP_W * MAP_H && landComp[sy * MAP_W + sx] == landComp[gy * MAP_W + gx]) {
            auto octile = [](int ax, int ay, int bx, int by) { int dx = std::abs(ax - bx), dy = std::abs(ay - by); return (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy)); };
            auto standing = [&](const BridgeSpot& b) { int k = (b.ty + BRIDGE_L / 2) * MAP_W + b.tx + 1; return inMap(b.tx + 1, b.ty + BRIDGE_L / 2) && blocked[k] == 0; };
            int bestX = -1, bestY = -1; float bestD = 1e30f;
            for (size_t i = 0; i < bridgeSpots.size(); i++) {
                const BridgeSpot& b = bridgeSpots[i];
                if (!standing(b)) continue;
                int mx = b.tx + 1, my = b.ty + BRIDGE_L / 2;
                // a delta's bridge only leads to the island: the crossing is its pair (the other bridge at the same column), and the island between them
                bool delta = b.ty + BRIDGE_L - 1 < CANAL_Y0 - 1 || b.ty > CANAL_Y1 + 1;
                if (delta) {
                    if (b.ty > CANAL_Y1) continue;   // (taken with its northern partner)
                    const BridgeSpot* pair = nullptr;
                    for (const BridgeSpot& o : bridgeSpots) if (o.tx == b.tx && o.ty > CANAL_Y1) pair = &o;
                    if (!pair || !standing(*pair)) continue;
                    my = (b.ty + BRIDGE_L + pair->ty) / 2;   // (the island)
                    if (blocked[my * MAP_W + mx]) { int nx = mx, ny = my; for (int r = 1; r <= 3 && blocked[ny * MAP_W + nx]; r++) { nx = mx + r; if (!blocked[my * MAP_W + nx]) { ny = my; break; } nx = mx - r; if (!blocked[my * MAP_W + nx]) { ny = my; break; } nx = mx; } mx = nx; my = ny; }
                }
                float d = octile(sx, sy, mx, my) + octile(mx, my, gx, gy);
                if (d < bestD) { bestD = d; bestX = mx; bestY = my; }
            }
            if (bestX >= 0 && passable(bestX, bestY) && !(bestX == sx && bestY == sy)) {
                std::vector<Vec2> leg;
                Vec2 wp = tileCenter(bestX, bestY);
                if (findPath(from, wp, out, maxNodes, false) && !out.empty() && findPath(wp, to, leg, maxNodes, false)) {
                    out.insert(out.end(), leg.begin(), leg.end());
                    return true;
                }
                out.clear();   // (no luck that way: the plain search below)
            }
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
    auto inComp = [&](int x, int y) { return comp == 0 || (comp == OPEN_SEA ? openSea(x, y) : waterComp[y * MAP_W + x] == comp); };
    if (navOpen(tx, ty) && (!bigOnly || bigWater(tx, ty)) && inComp(tx, ty)) return p;
    for (int pass = 0; pass < 2; pass++)   // open water first; a tile with only a narrow channel is the fallback
        for (int r = 1; r <= maxR; r++) {
            float best = 1e18f; int bx = -1, by = -1;
            for (int dy = -r; dy <= r; dy++) for (int dx = -r; dx <= r; dx += (dy == -r || dy == r) ? 1 : 2 * r) {   // the ring only, in row order (the old full-square scan was O(R^3))
                int x = tx + dx, y = ty + dy;
                if (pass == 0 ? !navOpen(x, y) : !navTile(x, y)) continue;
                if (bigOnly && !bigWater(x, y)) continue;
                if (!inComp(x, y)) continue;
                float d = dist2(tileCenter(x, y), p);
                if (d < best) { best = d; bx = x; by = y; }
            }
            if (bx >= 0) return tileCenter(bx, by);
        }
    return p;
}

bool Map::buildNavFlags() const {
    // only the tiles in the changed box (and their neighbours) need their flags again; the first build covers the whole map. Returns whether any flag
    // changed (a structure raised or lost on land changes nothing a ship cares about: the stretches of open water stay as they were)
    const bool all = navAllDirty;
    int x0 = navAllDirty ? 0 : std::max(0, navBx0), y0 = navAllDirty ? 0 : std::max(0, navBy0);
    int x1 = navAllDirty ? MAP_W - 1 : std::min(MAP_W - 1, navBx1), y1 = navAllDirty ? MAP_H - 1 : std::min(MAP_H - 1, navBy1);
    if (x1 < x0 || y1 < y0) return false;
    static std::vector<u8> before; before.clear();
    int ex0 = std::max(0, x0 - 1), ey0 = std::max(0, y0 - 1), ex1 = std::min(MAP_W - 1, x1 + 1), ey1 = std::min(MAP_H - 1, y1 + 1);
    if (!all) for (int y = ey0; y <= ey1; y++) for (int x = ex0; x <= ex1; x++) before.push_back(navFlag[y * MAP_W + x]);
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        int i = y * MAP_W + x;
        navFlag[i] = (tiles[i] == T_WATER && (blocked[i] & ~1) == 0) ? NF_WET : 0;
    }
    for (int y = ey0; y <= ey1; y++) for (int x = ex0; x <= ex1; x++) {
        int i = y * MAP_W + x;
        auto wet = [&](int nx, int ny) { return inMap(nx, ny) && (navFlag[ny * MAP_W + nx] & NF_WET); };   // (only the wet bits are read: the flags above them are still being set)
        bool open = (navFlag[i] & NF_WET) && wet(x + 1, y) && wet(x - 1, y) && wet(x, y + 1) && wet(x, y - 1);
        bool dry = false;
        for (int dy = -1; dy <= 1; dy++) for (int dx = -1; dx <= 1; dx++) if ((dx || dy) && !wet(x + dx, y + dy)) dry = true;
        navFlag[i] = (u8)((navFlag[i] & NF_WET) | (open ? NF_OPEN : 0) | (dry ? NF_SHORE : 0));
    }
    navAllDirty = false; navBx0 = MAP_W; navBy0 = MAP_H; navBx1 = -1; navBy1 = -1;
    if (all) return true;
    size_t k = 0; bool changed = false;
    for (int y = ey0; y <= ey1 && !changed; y++) for (int x = ex0; x <= ex1; x++) if (navFlag[y * MAP_W + x] != before[k++]) { changed = true; break; }
    return changed;
}

// The stretches of open water: a flood over open tiles, stepping the way the search steps (no corner cutting), so two tiles share a label exactly
// when a ship can get from one to the other. Only a search that runs long needs them (see findNavalPath), so they are built on demand.
void Map::buildNavLabels() const {
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    static std::vector<int> stack;
    for (int i = 0; i < MAP_W * MAP_H; i++) navLabel[i] = 0;
    u32 lab = 0;
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        if (!(navFlag[i] & NF_OPEN) || navLabel[i]) continue;
        navLabel[i] = ++lab;
        stack.assign(1, i);
        while (!stack.empty()) {
            int c = stack.back(); stack.pop_back();
            int x = c % MAP_W, y = c / MAP_W;
            for (int d = 0; d < 8; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!inMap(nx, ny)) continue;
                int ni = ny * MAP_W + nx;
                if (!(navFlag[ni] & NF_OPEN) || navLabel[ni]) continue;
                if (d >= 4 && (!(navFlagAt(x + DX[d], y) & NF_WET) || !(navFlagAt(x, y + DY[d]) & NF_WET))) continue;
                navLabel[ni] = lab; stack.push_back(ni);
            }
        }
    }
    navLabelGen = navFlagGen;
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
// The open list of the naval search: a min-heap on f, and on a tie the deeper node goes first (fewer nodes on a plateau of equal estimates)
struct NavOpen { float f, g; int idx; };
static std::vector<NavOpen> navHeap;   // the frontier: one entry per open tile, kept in heap order by NavNode::slot
static bool navBetter(const NavOpen& a, const NavOpen& b) { return a.f < b.f || (a.f == b.f && a.g > b.g); }
// One record per tile (score, parent and the search stamps together, so a visit touches one cache line instead of four arrays)
struct NavNode { float g; i32 parent; u32 visit, closed; i32 slot; };   // (slot: where the tile sits in navHeap while it is open)
static NavNode nNode[MAP_W * MAP_H];
static u32 nGen = 1;
// heap moves on navHeap (a decrease-key moves a tile up in place, so the heap holds each open tile once)
void navSiftUp(size_t i) {
    NavOpen x = navHeap[i];
    while (i > 0) { size_t p = (i - 1) / 2; if (!navBetter(x, navHeap[p])) break; navHeap[i] = navHeap[p]; nNode[navHeap[i].idx].slot = (int)i; i = p; }
    navHeap[i] = x; nNode[x.idx].slot = (int)i;
}
void navSiftDown(size_t i) {
    NavOpen x = navHeap[i]; size_t n = navHeap.size();
    for (;;) {
        size_t c = 2 * i + 1;
        if (c >= n) break;
        if (c + 1 < n && navBetter(navHeap[c + 1], navHeap[c])) c++;
        if (!navBetter(navHeap[c], x)) break;
        navHeap[i] = navHeap[c]; nNode[navHeap[i].idx].slot = (int)i; i = c;
    }
    navHeap[i] = x; nNode[x.idx].slot = (int)i;
}
const int NAV_LABEL_CHECK = 4000;   // expansions a search runs before it asks whether its goal is on another stretch of open water
}

// A* over open water. Ships keep off the shore (a tile next to land costs more), cut no corners, and head for the open water nearest to a goal on
// land (a ship sent to shell a shore target sails to the closest it can get).
// The naval search's heuristic. Open water hardly ever changes in a match (nothing stands on it), so from each of NAV_LM spots out at sea the
// distance over open water to every tile is worked out once per map, at the plain step cost (no shore surcharge: the heuristic must not overestimate).
// Then |d(L, goal) - d(L, n)| is a lower bound of the way from n to the goal for every landmark L - and unlike the straight line it knows that the main
// island stands in the way: a ship's search round it no longer floods half the sea first (ALT: A*, landmarks and the triangle inequality).
static const int NAV_LM = 8;
static std::vector<u16> g_navLM[NAV_LM];   // distance * 16 (0xFFFF: unreachable)
static u32 g_navLMStamp = 0;
static void buildNavLandmarks(const Map& m) {
    static const int LX[NAV_LM] = { 12, MAP_W / 2, MAP_W - 13, 12, MAP_W - 13, 12, MAP_W / 2, MAP_W - 13 };
    static const int LY[NAV_LM] = { 12, 10, 12, MAP_H / 2, MAP_H / 2, MAP_H - 13, MAP_H - 11, MAP_H - 13 };
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    std::vector<float> d((size_t)MAP_W * MAP_H);
    struct Q { float d; int i; bool operator<(const Q& o) const { return d > o.d; } };
    std::vector<Q> heap; heap.reserve(1 << 16);
    for (int L = 0; L < NAV_LM; L++) {
        g_navLM[L].assign((size_t)MAP_W * MAP_H, 0xFFFF);
        // the landmark: the open water tile nearest its spot
        int sx = -1, sy = -1;
        for (int r = 0; r < 40 && sx < 0; r++) for (int dy = -r; dy <= r && sx < 0; dy++) for (int dx = -r; dx <= r; dx++) {
            if (std::max(std::abs(dx), std::abs(dy)) != r) continue;
            if (m.navFlagAt(LX[L] + dx, LY[L] + dy) & Map::NF_OPEN) { sx = LX[L] + dx; sy = LY[L] + dy; break; }
        }
        if (sx < 0) continue;
        std::fill(d.begin(), d.end(), 1e30f);
        heap.clear();
        d[sy * MAP_W + sx] = 0; heap.push_back({0, sy * MAP_W + sx});
        while (!heap.empty()) {
            std::pop_heap(heap.begin(), heap.end()); Q q = heap.back(); heap.pop_back();
            if (q.d > d[q.i]) continue;
            int x = q.i % MAP_W, y = q.i / MAP_W;
            for (int k = 0; k < 8; k++) {
                int nx = x + DX[k], ny = y + DY[k];
                if (!(m.navFlagAt(nx, ny) & Map::NF_OPEN)) continue;
                if (k >= 4 && (!(m.navFlagAt(x + DX[k], y) & Map::NF_WET) || !(m.navFlagAt(x, y + DY[k]) & Map::NF_WET))) continue;   // (as the search steps: no corner cutting)
                float nd = q.d + (k >= 4 ? 1.4142f : 1.0f);
                int ni = ny * MAP_W + nx;
                if (nd < d[ni]) { d[ni] = nd; heap.push_back({nd, ni}); std::push_heap(heap.begin(), heap.end()); }
            }
        }
        for (int i = 0; i < MAP_W * MAP_H; i++) if (d[i] < 1e29f) g_navLM[L][i] = (u16)std::min(65534.0f, d[i] * 16.0f);
    }
}

bool Map::findNavalPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes) const {
    out.clear();
    if (navFlagGen != navGen) {
        bool changed = buildNavFlags();
        if (!changed && navLabelGen == navFlagGen) navLabelGen = navGen;   // (nothing a ship cares about changed: the stretches still hold)
        navFlagGen = navGen;
    }
    if (g_navLMStamp != navStaticGen) { buildNavLandmarks(*this); g_navLMStamp = navStaticGen; }
    Vec2 sf = nearestWater(from, 6);
    int comp = compAt(sf);   // (the goal is moved onto the same body of water: the lake and the ocean do not meet)
    Vec2 gt = nearestWater(to, 14, true, comp);
    if (!navTile(tileOf(gt.x), tileOf(gt.y)) || compAt(gt) != comp) gt = nearestWater(to, 60, true, comp);   // a goal far inland: head for the closest point of this sea
    if (!navTile(tileOf(gt.x), tileOf(gt.y)) || compAt(gt) != comp) gt = nearestWater(to, 150, true, comp);
    int sx = clampi(tileOf(sf.x), 0, MAP_W - 1), sy = clampi(tileOf(sf.y), 0, MAP_H - 1);
    int gx = clampi(tileOf(gt.x), 0, MAP_W - 1), gy = clampi(tileOf(gt.y), 0, MAP_H - 1);
    if (!navTile(sx, sy) || !navTile(gx, gy)) return false;
    if (sx == gx && sy == gy) { if (dist(from, sf) > 3.0f) out.push_back(sf); out.push_back(gt); return true; }
    const int start = sy * MAP_W + sx;
    bool stretchChecked = false;
    static const int DX[8] = {1, -1, 0, 0, 1, 1, -1, -1};
    static const int DY[8] = {0, 0, 1, -1, 1, -1, 1, -1};
    // one search toward (gx, gy); returns the expanded tile nearest the goal. restart: the goal was moved to another stretch and the search must begin again
    auto search = [&](bool& found, bool& restart, int& expanded) {
        nGen++;
        if (nGen == 0) { memset(nNode, 0, sizeof(nNode)); nGen = 1; }
        const int goal = gy * MAP_W + gx;
        u16 lg[NAV_LM];
        for (int L = 0; L < NAV_LM; L++) lg[L] = g_navLM[L].empty() ? 0xFFFF : g_navLM[L][goal];
        auto h = [&](int idx) {
            int x = idx % MAP_W, y = idx / MAP_W, dx = std::abs(x - gx), dy = std::abs(y - gy);
            float oct = 1.0005f * (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy));
            int alt = 0;   // the landmarks' bound (see buildNavLandmarks)
            for (int L = 0; L < NAV_LM; L++) {
                if (lg[L] == 0xFFFF) continue;
                u16 a = g_navLM[L][idx];
                if (a == 0xFFFF) continue;
                int v = a > lg[L] ? a - lg[L] : lg[L] - a;
                if (v > alt) alt = v;
            }
            return std::max(oct, alt * (1.0f / 16.0f));
        };
        navHeap.clear();
        nNode[start].g = 0; nNode[start].parent = -1; nNode[start].visit = nGen;
        navHeap.push_back({h(start), 0, start}); navSiftUp(0);
        int best = start; float bestH = h(start);
        expanded = 0; found = false; restart = false;
        while (!navHeap.empty()) {
            NavOpen n = navHeap[0];
            navHeap[0] = navHeap.back(); navHeap.pop_back();
            if (!navHeap.empty()) navSiftDown(0);
            NavNode& cur = nNode[n.idx];
            cur.slot = -1;
            if (cur.closed == nGen) continue;
            cur.closed = nGen;
            if (n.idx == goal) { best = goal; found = true; break; }
            float hh = h(n.idx);
            if (hh < bestH) { bestH = hh; best = n.idx; }
            if (++expanded > maxNodes) break;
            // a search that has run long without reaching its goal looks at the stretches: a goal on another stretch cannot be reached, and the search
            // would flood the whole of its own stretch before giving up. Head for the open water of that stretch closest to the goal instead (what
            // the failed search ended on, found without the flood).
            if (expanded == NAV_LABEL_CHECK && !stretchChecked && (navFlagAt(sx, sy) & NF_OPEN)) {
                stretchChecked = true;
                if (navLabelGen != navFlagGen) buildNavLabels();
                u32 sl = navLabel[start], gl = (navFlagAt(gx, gy) & NF_OPEN) ? navLabel[goal] : 0;
                if (sl && gl && gl != sl) {
                    int bx = sx, by = sy; float bd = 1e18f;
                    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
                        if (navLabel[y * MAP_W + x] != sl) continue;
                        float d = 1.0005f * (float)(std::max(std::abs(x - gx), std::abs(y - gy)) + 0.4142f * std::min(std::abs(x - gx), std::abs(y - gy)));
                        if (d < bd) { bd = d; bx = x; by = y; }
                    }
                    gx = bx; gy = by; gt = tileCenter(bx, by);
                    restart = true; break;
                }
            }
            int x = n.idx % MAP_W, y = n.idx / MAP_W;
            for (int d = 0; d < 8; d++) {
                int nx = x + DX[d], ny = y + DY[d];
                if (!(navFlagAt(nx, ny) & NF_OPEN) && !(nx == gx && ny == gy)) continue;
                if (d >= 4 && (!(navFlagAt(x + DX[d], y) & NF_WET) || !(navFlagAt(x, y + DY[d]) & NF_WET))) continue;   // no corner cutting
                int ni = ny * MAP_W + nx;
                NavNode& q = nNode[ni];
                if (q.closed == nGen) continue;
                bool shore = navFlag[ni] & NF_SHORE;
                float step = (d >= 4 ? 1.4142f : 1.0f) * (shore ? 1.6f : 1.0f);
                float ng = cur.g + step;
                if (q.visit != nGen) {
                    q.visit = nGen; q.g = ng; q.parent = n.idx;
                    navHeap.push_back({ng + h(ni), ng, ni}); navSiftUp(navHeap.size() - 1);
                } else if (ng < q.g) {
                    q.g = ng; q.parent = n.idx;
                    navHeap[q.slot] = {ng + h(ni), ng, ni}; navSiftUp(q.slot);
                }
            }
        }
        return best;
    };
    bool found = false, restart = false; int expanded = 0;
    int best = search(found, restart, expanded);
    while (restart) best = search(found, restart, expanded);
    std::vector<Vec2> rev;
    for (int i = best; i != -1 && i != start; i = nNode[i].parent) rev.push_back(tileCenter(i % MAP_W, i / MAP_W));
    if (rev.empty()) {   // (already at the goal: the open water nearest it, which may be where it lies)
        if (!found) return false;
        if (dist(from, sf) > 3.0f) out.push_back(sf);
        out.push_back(gt);
        return true;
    }
    std::reverse(rev.begin(), rev.end());
    if (found) rev.back() = gt;
    if (dist(from, sf) > 3.0f) out.push_back(sf);   // a ship hugging the shore first sails to the open water beside it (a straight line from where it is might cross the beach)
    const size_t NAVAL_PULL_AHEAD = 24;
    Vec2 cur = sf;   // string pulling over open water, looking a bounded distance ahead (the whole route was quadratic, as the land pass was)
    size_t i = 0;
    while (i < rev.size()) {
        size_t j = std::min(rev.size() - 1, i + NAVAL_PULL_AHEAD);
        while (j > i && !navLineClear(cur, rev[j])) j--;
        out.push_back(rev[j]);
        cur = rev[j];
        i = j + 1;
    }
    return found;
}
