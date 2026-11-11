// One Hour - baking the 3D models (m3d.h) into sprites: the views of a model are rendered on every core, and all of them are kept in one cache
// file between runs (keyed by a hash of the model and the view), so a start-up renders only what changed since the last one.
#include "m3d.h"
#include <thread>
#include <atomic>
#include <unordered_map>

namespace m3d {

namespace {
const u32 CACHE_MAGIC = 0x3144334Du;   // "M3D1"
const u32 RENDER_VERSION = 5;          // (bump when the renderer's output changes: every cached view is then rendered again)
struct Entry { size_t off; u32 size; };
std::vector<u8> oldBytes;              // the cache as read
std::unordered_map<u64, Entry> oldIdx;
std::vector<u8> newBytes;              // the cache of this run: the entries used, in the order they were asked for
std::string cachePath;
bool isOpen = false, dirty = false;
u32 nEntries = 0;
size_t usedOld = 0;

u64 fnv(u64 h, const void* p, size_t n) { const u8* b = (const u8*)p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 1099511628211ull; } return h; }
template <class T> void put(std::vector<u8>& o, const T& v) { const u8* b = (const u8*)&v; o.insert(o.end(), b, b + sizeof(T)); }
template <class T> bool get(const u8*& p, const u8* end, T& v) { if ((size_t)(end - p) < sizeof(T)) return false; std::memcpy(&v, p, sizeof(T)); p += sizeof(T); return true; }

// pixels as runs: (clear pixels, then literal pixels) pairs
void packPx(std::vector<u8>& o, const Canvas& c) {
    const size_t n = c.px.size();
    size_t i = 0;
    while (i < n) {
        u16 z = 0, l = 0;
        while (i < n && z < 65535 && (c.px[i] >> 24) == 0) { z++; i++; }
        size_t s = i;
        while (i < n && l < 65535 && (c.px[i] >> 24) != 0) { l++; i++; }
        put(o, z); put(o, l);
        const u8* b = (const u8*)&c.px[s < n ? s : 0];
        if (l) o.insert(o.end(), b, b + (size_t)l * 4);
    }
}
bool unpackPx(const u8*& p, const u8* end, Canvas& c) {
    const size_t n = c.px.size();
    size_t i = 0;
    while (i < n) {
        u16 z, l;
        if (!get(p, end, z) || !get(p, end, l)) return false;
        if (i + z + l > n || (size_t)(end - p) < (size_t)l * 4) return false;
        i += z;   // (a new canvas is clear already)
        if (l) std::memcpy(&c.px[i], p, (size_t)l * 4);
        p += (size_t)l * 4; i += l;
    }
    return true;
}
void pack(std::vector<u8>& o, const Baked& b) {
    put(o, (u16)b.color.w); put(o, (u16)b.color.h); put(o, b.ox); put(o, b.oy);
    put(o, (u16)(b.hasMask ? b.mask.w : 0)); put(o, (u16)(b.hasMask ? b.mask.h : 0)); put(o, b.mox); put(o, b.moy);
    packPx(o, b.color);
    if (b.hasMask) packPx(o, b.mask);
}
bool unpack(const u8* p, const u8* end, Baked& b) {
    u16 w, h, mw, mh;
    if (!get(p, end, w) || !get(p, end, h) || !get(p, end, b.ox) || !get(p, end, b.oy) || !get(p, end, mw) || !get(p, end, mh) || !get(p, end, b.mox) || !get(p, end, b.moy)) return false;
    if (w == 0 || h == 0 || w > 4096 || h > 4096 || mw > 4096 || mh > 4096) return false;
    b.color = Canvas(w, h);
    if (!unpackPx(p, end, b.color)) return false;
    b.hasMask = mw > 0 && mh > 0;
    if (b.hasMask) { b.mask = Canvas(mw, mh); if (!unpackPx(p, end, b.mask)) return false; }
    return true;
}
// the part of a canvas that shows anything, and where the origin lands in it
bool crop(const Canvas& c, float ox, float oy, Canvas& out, float& nox, float& noy) {
    int x0 = c.w, y0 = c.h, x1 = -1, y1 = -1;
    for (int y = 0; y < c.h; y++) {
        const u32* row = &c.px[(size_t)y * c.w];
        for (int x = 0; x < c.w; x++) if (row[x] >> 24) { if (x < x0) x0 = x; if (x > x1) x1 = x; if (y < y0) y0 = y; y1 = y; }
    }
    if (x1 < 0) return false;
    out = Canvas(x1 - x0 + 1, y1 - y0 + 1);
    for (int y = y0; y <= y1; y++) std::memcpy(&out.px[(size_t)(y - y0) * out.w], &c.px[(size_t)y * c.w + x0], (size_t)out.w * 4);
    nox = ox - x0; noy = oy - y0;
    return true;
}
// The print: the game lays the hour's light and the weather over the world, which takes a quarter off every sprite's brightness, so the views are
// printed lighter and a little richer than the bare render (highlights roll off instead of clipping; black shadow stays black).
void grade(Canvas& c, float gain, float sat) {
    // a toe under the mid tone (shade goes deeper, so forms read at a glance) and a shoulder above it
    auto curve = [](float x) { return x < 0.46f ? 0.46f * std::pow(x / 0.46f, 1.24f) : (x <= 0.70f ? x : 0.70f + 0.30f * (1.0f - std::exp(-(x - 0.70f) / 0.30f))); };
    for (u32& px : c.px) {
        Color o = Canvas::unpack(px);
        if (o.a == 0 || o.r + o.g + o.b == 0) continue;
        float r = o.r / 255.0f, g = o.g / 255.0f, b = o.b / 255.0f, lum = 0.299f * r + 0.587f * g + 0.114f * b;
        r = lum + (r - lum) * sat; g = lum + (g - lum) * sat; b = lum + (b - lum) * sat;
        o.r = (u8)(clampf(curve(std::max(0.0f, r) * gain), 0, 1) * 255.0f + 0.5f); o.g = (u8)(clampf(curve(std::max(0.0f, g) * gain), 0, 1) * 255.0f + 0.5f); o.b = (u8)(clampf(curve(std::max(0.0f, b) * gain), 0, 1) * 255.0f + 0.5f);
        px = Canvas::pack(o);
    }
}
void shoot(Shot& s) {
    Rendered r;
    render(*s.model, s.opt, r);
    grade(r.color, 1.30f, 1.30f);
    if (r.hasMask) grade(r.mask, 1.15f, 1.0f);
    Baked& b = s.out;
    if (!crop(r.color, r.ox, r.oy, b.color, b.ox, b.oy)) { b.color = Canvas(1, 1); b.ox = b.oy = 0; }
    b.hasMask = r.hasMask && crop(r.mask, r.ox, r.oy, b.mask, b.mox, b.moy);
}
}  // namespace

u64 modelKey(const Model& m) {
    static_assert(sizeof(Prim) == 96 && sizeof(Plane) == 16, "hashed as raw bytes: no padding allowed");
    u64 h = 1469598103934665603ull;
    if (!m.prims.empty()) h = fnv(h, m.prims.data(), m.prims.size() * sizeof(Prim));
    if (!m.planes.empty()) h = fnv(h, m.planes.data(), m.planes.size() * sizeof(Plane));
    const u8 g = m.ground ? 1 : 0;
    h = fnv(h, &g, 1); h = fnv(h, &m.dirt, sizeof(float));
    return h;
}
static u64 shotKey(const Shot& s) {
    u64 h = modelKey(*s.model);
    const float f[3] = { s.opt.scale, s.opt.heading, s.opt.shadow };
    const int i[3] = { s.opt.ss, s.opt.outline ? 1 : 0, (int)RENDER_VERSION };
    h = fnv(h, f, sizeof f); h = fnv(h, i, sizeof i);
    return h;
}

void bakeOpen(const std::string& file) {
    oldBytes.clear(); oldIdx.clear(); newBytes.clear(); nEntries = 0; usedOld = 0; dirty = false;
    cachePath = file; isOpen = true;
    if (file.empty()) return;
    FILE* f = fopen(file.c_str(), "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    if (n > 12 && n < (400L << 20)) {
        oldBytes.resize((size_t)n);
        if (fread(oldBytes.data(), 1, (size_t)n, f) != (size_t)n) oldBytes.clear();
    }
    fclose(f);
    const u8* p = oldBytes.data(); const u8* end = p + oldBytes.size();
    u32 magic = 0, ver = 0, count = 0;
    if (!get(p, end, magic) || !get(p, end, ver) || !get(p, end, count) || magic != CACHE_MAGIC || ver != RENDER_VERSION) { oldBytes.clear(); return; }
    for (u32 i = 0; i < count; i++) {
        u64 key; u32 size;
        if (!get(p, end, key) || !get(p, end, size) || (size_t)(end - p) < size) { oldIdx.clear(); oldBytes.clear(); return; }   // (a cut-off file: render it all again)
        oldIdx[key] = Entry{ (size_t)(p - oldBytes.data()), size };
        p += size;
    }
}

void bake(std::vector<Shot>& shots) {
    const int n = (int)shots.size();
    std::vector<u64> keys(n);
    std::vector<int> todo;
    for (int i = 0; i < n; i++) {
        keys[i] = shotKey(shots[i]);
        auto it = isOpen ? oldIdx.find(keys[i]) : oldIdx.end();
        if (it != oldIdx.end() && unpack(oldBytes.data() + it->second.off, oldBytes.data() + it->second.off + it->second.size, shots[i].out)) {
            put(newBytes, keys[i]); put(newBytes, it->second.size);
            newBytes.insert(newBytes.end(), oldBytes.begin() + (long)it->second.off, oldBytes.begin() + (long)(it->second.off + it->second.size));
            nEntries++; usedOld++;
        } else todo.push_back(i);
    }
    if (todo.empty()) return;
    const int hc = (int)std::thread::hardware_concurrency();
    const int nt = std::max(1, std::min(std::min(hc > 2 ? hc - 2 : 1, 10), (int)todo.size()));
    std::atomic<int> next{ 0 };
    auto work = [&] { for (;;) { int k = next.fetch_add(1); if (k >= (int)todo.size()) return; shoot(shots[todo[k]]); } };
    if (nt == 1) work();
    else { std::vector<std::thread> th; for (int t = 0; t < nt; t++) th.emplace_back(work); for (auto& t : th) t.join(); }
    if (!isOpen) return;
    std::vector<u8> blob;
    for (int i : todo) {
        blob.clear(); pack(blob, shots[i].out);
        put(newBytes, keys[i]); put(newBytes, (u32)blob.size());
        newBytes.insert(newBytes.end(), blob.begin(), blob.end());
        nEntries++; dirty = true;
    }
}

void bakeClose() {
    if (!isOpen) return;
    if ((dirty || usedOld != oldIdx.size()) && nEntries > 0 && !cachePath.empty()) {   // (something new, or entries nothing asked for any more: write the cache of this run; a run that drew no model at all, ONEHOUR_OLDART, leaves the cache alone)
        const std::string tmp = cachePath + ".tmp";
        if (FILE* f = fopen(tmp.c_str(), "wb")) {
            const u32 hd[3] = { CACHE_MAGIC, RENDER_VERSION, nEntries };
            bool ok = fwrite(hd, 1, sizeof hd, f) == sizeof hd && fwrite(newBytes.data(), 1, newBytes.size(), f) == newBytes.size();
            ok = fclose(f) == 0 && ok;
            if (ok) rename(tmp.c_str(), cachePath.c_str()); else remove(tmp.c_str());
        }
    }
    std::vector<u8>().swap(oldBytes); std::vector<u8>().swap(newBytes); oldIdx.clear();
    isOpen = false;
}

}  // namespace m3d
