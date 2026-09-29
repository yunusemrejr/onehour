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
static void paintRoad(Map& m, float x0, float y0, float x1, float y1, float halfW) {
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
                if (c != T_WATER) c = T_ROAD;
            }
        }
    }
}
static bool isGrass(u8 t) { return t == T_GRASS || t == T_GRASS2; }

void Map::generate() {
    const u32 SEED = 1979;
    // Base terrain from noise: grass with dirt patches, sand near center
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        float n = fbm(x * 0.07f, y * 0.07f, SEED);
        u8 t = T_GRASS;
        if (n > 0.62f) t = T_DIRT;
        else if (n < 0.40f) t = T_GRASS2;
        tiles[y * MAP_W + x] = t;
        variant[y * MAP_W + x] = (u8)(hash2(x, y, SEED + 99) * 255);
        blocked[y * MAP_W + x] = 0;
    }
    // Central lake with sandy shore
    paintDisc(*this, 40, 40, 10.5f, T_SAND);
    paintDisc(*this, 40, 40, 7.0f, T_WATER);
    paintDisc(*this, 35.5f, 43.5f, 3.0f, T_WATER);
    // Small ponds near the lateral gaps (decor + partial cover)
    paintDisc(*this, 40, 21, 2.2f, T_SAND); paintDisc(*this, 40, 21, 1.3f, T_WATER);
    paintDisc(*this, 40, 59, 2.2f, T_SAND); paintDisc(*this, 40, 59, 1.3f, T_WATER);

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
    ridge(38, 41, 1, 24, true, 10, 14);
    ridge(38, 41, 55, 78, true, 65, 69);
    ridge(1, 24, 38, 41, false, 10, 14);
    ridge(55, 78, 38, 41, false, 65, 69);
    // Rock outcrops guarding the diagonal approaches to the center
    paintDisc(*this, 27, 33, 2.4f, T_ROCK); paintDisc(*this, 53, 33, 2.4f, T_ROCK);
    paintDisc(*this, 27, 47, 2.4f, T_ROCK); paintDisc(*this, 53, 47, 2.4f, T_ROCK);
    // Map border rocks
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if (x == 0 || y == 0 || x == MAP_W - 1 || y == MAP_H - 1) tiles[y * MAP_W + x] = T_ROCK;

    // Roads: diagonals from bases to the ring road around the lake, plus the ring
    starts = { {11, 11}, {68, 11}, {11, 68}, {68, 68} };
    const float ring = 13.5f;
    float rx0 = 40 - ring, rx1 = 40 + ring;
    paintRoad(*this, rx0, rx0, rx1, rx0, 1.1f);
    paintRoad(*this, rx1, rx0, rx1, rx1, 1.1f);
    paintRoad(*this, rx1, rx1, rx0, rx1, 1.1f);
    paintRoad(*this, rx0, rx1, rx0, rx0, 1.1f);
    paintRoad(*this, 13, 13, rx0, rx0, 1.2f);
    paintRoad(*this, 67, 13, rx1, rx0, 1.2f);
    paintRoad(*this, 13, 67, rx0, rx1, 1.2f);
    paintRoad(*this, 67, 67, rx1, rx1, 1.2f);
    // Lateral roads through the chokepoint gaps
    paintRoad(*this, 13, 12, 67, 12, 1.0f);
    paintRoad(*this, 13, 67, 67, 67, 1.0f);
    paintRoad(*this, 12, 13, 12, 67, 1.0f);
    paintRoad(*this, 67, 13, 67, 67, 1.0f);

    // Tree clusters from noise, kept off roads, bases, and the shore
    for (int y = 2; y < MAP_H - 2; y++) for (int x = 2; x < MAP_W - 2; x++) {
        u8& t = tiles[y * MAP_W + x];
        if (!isGrass(t)) continue;
        float n = fbm(x * 0.13f + 40, y * 0.13f + 40, SEED + 31);
        bool nearBase = false;
        for (auto& s : starts) if (std::abs(x - s.tx) < 11 && std::abs(y - s.ty) < 11) nearBase = true;
        float dc = std::sqrt((x - 40.0f) * (x - 40.0f) + (y - 40.0f) * (y - 40.0f));
        if (nearBase || dc < 16) continue;
        // keep roads' neighbors clear
        bool nearRoad = false;
        for (int dy = -1; dy <= 1 && !nearRoad; dy++) for (int dx = -1; dx <= 1; dx++)
            if (tiles[(y + dy) * MAP_W + x + dx] == T_ROAD) { nearRoad = true; break; }
        if (nearRoad) continue;
        if (n > 0.66f) t = T_TREE;
    }
    // Scenic forest belts in the corners between bases (partial cover, never sealed)
    // Supply piles: two per base, four around the lake, four in the lateral gaps
    supplies = {
        {6, 16, 36000}, {16, 6, 36000},
        {73, 16, 36000}, {63, 6, 36000},
        {6, 63, 36000}, {16, 73, 36000},
        {73, 63, 36000}, {63, 73, 36000},
        {40, 27, 50000}, {53, 40, 50000}, {40, 53, 50000}, {27, 40, 50000},
        {35, 6, 36000}, {6, 45, 36000}, {45, 73, 36000}, {73, 35, 36000},
    };
    // Clear the ground around supply piles and bases so nothing important is walled in
    for (auto& s : supplies) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 2.6f, T_DIRT, [](u8 t) { return t == T_TREE || t == T_ROCK || t == T_WATER; });
    for (auto& s : starts) paintDisc(*this, s.tx + 0.5f, s.ty + 0.5f, 9.0f, T_GRASS, [](u8 t) { return t == T_TREE || t == T_ROCK; });

    // Blocked flags
    for (int i = 0; i < MAP_W * MAP_H; i++) {
        u8 t = tiles[i];
        blocked[i] = (t == T_WATER || t == T_ROCK || t == T_TREE) ? 1 : 0;
    }
    for (auto& s : supplies) setResource(s.tx, s.ty, true);
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
static float gScore[MAP_W * MAP_H];
static i32 parentIdx[MAP_W * MAP_H];
static u32 visitGen[MAP_W * MAP_H];
static u32 closedGen[MAP_W * MAP_H];
static u32 curGen = 1;
}

bool Map::findPath(Vec2 from, Vec2 to, std::vector<Vec2>& out, int maxNodes) const {
    out.clear();
    int sx = clampi(tileOf(from.x), 0, MAP_W - 1), sy = clampi(tileOf(from.y), 0, MAP_H - 1);
    int gx = clampi(tileOf(to.x), 0, MAP_W - 1), gy = clampi(tileOf(to.y), 0, MAP_H - 1);
    if (!passable(sx, sy)) { Vec2 nf = nearestFree(from); sx = tileOf(nf.x); sy = tileOf(nf.y); }
    if (sx == gx && sy == gy) { out.push_back(to); return true; }
    curGen++;
    if (curGen == 0) { memset(visitGen, 0, sizeof(visitGen)); memset(closedGen, 0, sizeof(closedGen)); curGen = 1; }
    std::priority_queue<Node> open;
    int start = sy * MAP_W + sx, goal = gy * MAP_W + gx;
    auto h = [&](int idx) {
        int x = idx % MAP_W, y = idx / MAP_W;
        int dx = std::abs(x - gx), dy = std::abs(y - gy);
        return 0.8f * 1.0005f * (float)(std::max(dx, dy) + 0.4142f * std::min(dx, dy));   // octile distance scaled by the cheapest step (road) => admissible; epsilon breaks ties toward the goal
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
    // String pulling: skip waypoints reachable in a straight line
    Vec2 cur = from;
    size_t i = 0;
    while (i < rev.size()) {
        size_t j = rev.size() - 1;
        while (j > i && !lineClear(cur, rev[j])) j--;
        out.push_back(rev[j]);
        cur = rev[j];
        i = j + 1;
    }
    return found;
}
