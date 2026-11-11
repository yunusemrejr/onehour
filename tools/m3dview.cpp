// One Hour - model viewer: renders registered 3D models (src/m3d*.cpp) to a BMP sheet without building the game.
//   g++ -std=c++17 -O2 -Isrc $(pkg-config --cflags sdl2) tools/m3dview.cpp src/m3d.cpp src/m3d_<your file>.cpp -o /tmp/m3dview
//   /tmp/m3dview out.bmp U_K_TANK U_C_TANK B_K_POWER prop:tree.oak        (names as registered; "list" prints them all)
// Each model gets one row: eight headings at the game's size, then four headings enlarged 4x. Structures and props: one view at 1x and one at 2.5x.
// It also prints, per model, the number of solids, the sprite size in pixels at game size and how much of it is in the owner's colour.
// Team colour cycles blue / red / yellow / green along a row.
#include "../src/m3d_reg.h"
#include <chrono>

float viewScale(int type);   // the size the game draws a unit type at (tools/m3dscale.cpp)

static void blit(Canvas& dst, const Canvas& src, int x0, int y0, int zoom) {
    for (int y = 0; y < src.h; y++) for (int x = 0; x < src.w; x++) {
        Color c = Canvas::unpack(src.px[(size_t)y * src.w + x]);
        if (c.a == 0) continue;
        for (int j = 0; j < zoom; j++) for (int i = 0; i < zoom; i++) dst.set(x0 + x * zoom + i, y0 + y * zoom + j, c);
    }
}
static bool saveBmp(const char* path, const Canvas& c) {
    FILE* f = fopen(path, "wb"); if (!f) return false;
    int row = (c.w * 3 + 3) & ~3, size = 54 + row * c.h;
    u8 hd[54] = { 'B', 'M' };
    auto w32 = [&](int o, u32 v) { hd[o] = v; hd[o + 1] = v >> 8; hd[o + 2] = v >> 16; hd[o + 3] = v >> 24; };
    w32(2, size); w32(10, 54); w32(14, 40); w32(18, c.w); w32(22, c.h); hd[26] = 1; hd[28] = 24; w32(34, row * c.h);
    fwrite(hd, 1, 54, f);
    std::vector<u8> line(row, 0);
    for (int y = c.h - 1; y >= 0; y--) {
        for (int x = 0; x < c.w; x++) { Color p = Canvas::unpack(c.px[(size_t)y * c.w + x]); line[x * 3] = p.b; line[x * 3 + 1] = p.g; line[x * 3 + 2] = p.r; }
        fwrite(line.data(), 1, row, f);
    }
    fclose(f); return true;
}

int main(int argc, char** argv) {
    using namespace m3d;
    Registry& R = reg();
    if (argc < 2 || !strcmp(argv[1], "list")) {
        for (int i = 0; i < U_COUNT; i++) if (R.unit[i]) printf("%s\n", R.unitName[i]);
        for (int i = 0; i < B_COUNT; i++) if (R.build[i]) printf("%s\n", R.buildName[i]);
        for (auto& p : R.props) printf("prop:%s\n", p.name);
        return 0;
    }
    static const Color TEAMS[4] = { rgb(64, 150, 255), rgb(240, 70, 60), rgb(250, 208, 60), rgb(80, 210, 110) };
    const float US = getenv("M3D_SCALE") ? (float)atof(getenv("M3D_SCALE")) : 0.65f;
    const int rowH = getenv("M3D_ROWH") ? atoi(getenv("M3D_ROWH")) : 150;
    Canvas sheet(1280, rowH * (argc - 2) + 8);
    // ground: grass on the left half of each row, sand then sea on the right, so sprites are judged on what they will stand on
    for (int y = 0; y < sheet.h; y++) for (int x = 0; x < sheet.w; x++) {
        u32 h = (u32)(x * 7349) ^ (u32)(y * 9151); h ^= h >> 13; h *= 0x5bd1e995u; h ^= h >> 15;
        float n = ((h & 255) / 255.0f - 0.5f) * 10;
        Color g = x < 760 ? rgb(92 + (int)n, 128 + (int)n, 62 + (int)n) : (x < 1020 ? rgb(196 + (int)n, 176 + (int)n, 128 + (int)n) : rgb(40 + (int)n, 104 + (int)n, 138 + (int)n));
        sheet.px[(size_t)y * sheet.w + x] = Canvas::pack(g);
    }
    double total = 0; int views = 0;
    for (int a = 2; a < argc; a++) {
        const char* name = argv[a];
        const int y0 = (a - 2) * rowH + rowH / 2 + 10;
        int uid = -1, bid = -1; PropFn pf = nullptr; int variant = 0;
        for (int i = 0; i < U_COUNT; i++) if (R.unit[i] && !strcmp(R.unitName[i], name)) uid = i;
        for (int i = 0; i < B_COUNT; i++) if (R.build[i] && !strcmp(R.buildName[i], name)) bid = i;
        if (!strncmp(name, "prop:", 5)) { std::string nm = name + 5; size_t c = nm.find(':'); if (c != std::string::npos) { variant = atoi(nm.c_str() + c + 1); nm = nm.substr(0, c); } pf = R.prop(nm.c_str()); }
        if (uid < 0 && bid < 0 && !pf) { fprintf(stderr, "unknown model %s\n", name); continue; }
        auto t0 = std::chrono::steady_clock::now();
        int nPrim = 0, sw = 0, sh = 0; double teamPx = 0, allPx = 0;
        auto measure = [&](const Rendered& r) {
            sw = r.color.w; sh = r.color.h;
            for (size_t i = 0; i < r.color.px.size(); i++) { if ((r.color.px[i] >> 24) > 200) allPx++; if (r.hasMask && (r.mask.px[i] >> 24) > 100) teamPx++; }
        };
        if (uid >= 0) {
            const float us = getenv("M3D_SCALE") ? US : viewScale(uid);
            for (int k = 0; k < 12; k++) {
                const bool big = k >= 8;
                const float hd = big ? (k - 8) * 1.5708f + 0.7854f : k * 0.7854f;
                Model body, tur; R.unit[uid](body, tur, k == 3 ? 1 : (k == 5 ? 2 : 0));
                nPrim = (int)(body.prims.size() + tur.prims.size());
                const float zoom = big ? std::min(4.0f, 2.6f / us) : 1.0f;
                RenderOpt o; o.scale = us * zoom; o.heading = hd;
                Rendered rb; render(body, o, rb); views++;
                if (k == 0) measure(rb);
                Canvas cb = composite(rb, TEAMS[k & 3]);
                const int cx = big ? 420 + (k - 8) * 215 : 30 + k * 44, cy = y0;
                blit(sheet, cb, cx - (int)rb.ox, cy - (int)rb.oy, 1);
                if (!tur.prims.empty()) {
                    RenderOpt ot = o; ot.heading = hd + (big ? 0.5f : 0.0f);
                    Rendered rt; render(tur, ot, rt); views++;
                    Canvas ct = composite(rt, TEAMS[k & 3]);
                    blit(sheet, ct, cx - (int)rt.ox, cy - (int)rt.oy, 1);
                }
            }
        } else {
            Model m;
            if (bid >= 0) R.build[bid](m, 0); else pf(m, variant);
            nPrim = (int)m.prims.size();
            for (int k = 0; k < 2; k++) {
                RenderOpt o; o.scale = (bid >= 0 ? 1.0f : US) * (k ? 2.5f : 1);
                Rendered r; render(m, o, r); views++;
                if (k == 0) measure(r);
                Canvas c = composite(r, TEAMS[(a + k) & 3]);
                // the model's origin (a structure's footprint corner) is put at a fixed spot of the row
                blit(sheet, c, (k ? 420 : 60) - (int)r.ox, y0 - rowH / 2 + (int)(rowH * 0.45f) - (int)r.oy, 1);
            }
        }
        double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        total += ms;
        printf("%-18s %3d solids, sprite %3d x %3d px, owner colour %4.1f%% of it, %6.1f ms\n", name, nPrim, sw, sh, allPx > 0 ? 100.0 * teamPx / allPx : 0.0, ms);
    }
    printf("%d views, %.1f ms\n", views, total);
    return saveBmp(argv[1], sheet) ? 0 : 1;
}
