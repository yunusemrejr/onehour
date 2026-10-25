// One Hour - shared definitions
#pragma once
#include <cstdint>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <string>
#include <algorithm>

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int32_t i32;
typedef uint64_t u64;

// World geometry
static const int TILE = 32;          // pixels per tile
// The battlefield is an island in the ocean: the 208x208 main island (the old map: four bases round a lake with its own islands) sits in the middle
// of a 320x320 sea with a second, smaller island off its east coast and islets scattered round it.
static const int MAP_W = 320;        // tiles
static const int MAP_H = 320;
static const int ISLE_W = 208;       // the main island's square (the land the old 208 map covered), centred in the sea
static const int ISLE_X0 = (MAP_W - ISLE_W) / 2;   // its first tile on both axes
static const int WORLD_W = MAP_W * TILE;
static const int WORLD_H = MAP_H * TILE;

// Screen: logical size is dynamic (window is resizable); these are updated by Gfx on every resize
static const int MIN_SCREEN_W = 1024, MIN_SCREEN_H = 640;
static const int HUD_H = 132;        // command bar height at bottom
inline int SCREEN_W = MIN_SCREEN_W;
inline int SCREEN_H = MIN_SCREEN_H;
inline int VIEW_H = MIN_SCREEN_H - HUD_H;
inline void setScreenSize(int w, int h) { SCREEN_W = w; SCREEN_H = h; VIEW_H = h - HUD_H; }

// Simulation
static const int SIM_HZ = 20;
static const float SIM_DT = 1.0f / SIM_HZ;

static const int MAX_ARMIES = 4;      // armies in a match (four start spots)
static const int RIOT_PLAYER = 4;     // the slot after the armies: civilian rioters, who belong to nobody and fight every army (see Sim::updateRiots)
static const int MAX_PLAYERS = 5;     // armies + the riot crowd (per-player arrays are sized for both; numPlayers counts only the armies)

struct Vec2 {
    float x = 0, y = 0;
    Vec2() {}
    Vec2(float _x, float _y) : x(_x), y(_y) {}
    Vec2 operator+(const Vec2& o) const { return Vec2(x + o.x, y + o.y); }
    Vec2 operator-(const Vec2& o) const { return Vec2(x - o.x, y - o.y); }
    Vec2 operator*(float s) const { return Vec2(x * s, y * s); }
    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    float len() const { return std::sqrt(x * x + y * y); }
    float len2() const { return x * x + y * y; }
    Vec2 norm() const { float l = len(); return l > 1e-6f ? Vec2(x / l, y / l) : Vec2(0, 0); }
};
static inline float dist(const Vec2& a, const Vec2& b) { return (a - b).len(); }
static inline float dist2(const Vec2& a, const Vec2& b) { return (a - b).len2(); }
static inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline int clampi(int v, int lo, int hi) { return v < lo ? lo : (v > hi ? hi : v); }
static inline float lerpf(float a, float b, float t) { return a + (b - a) * t; }
static inline int tileOf(float px) { return (int)std::floor(px / TILE); }
static inline Vec2 tileCenter(int tx, int ty) { return Vec2(tx * TILE + TILE * 0.5f, ty * TILE + TILE * 0.5f); }
static inline bool inMap(int tx, int ty) { return tx >= 0 && ty >= 0 && tx < MAP_W && ty < MAP_H; }

// Deterministic RNG (xorshift)
struct Rng {
    u64 s;
    explicit Rng(u64 seed = 0x9E3779B97F4A7C15ull) : s(seed ? seed : 1) {}
    u32 next() { s ^= s << 13; s ^= s >> 7; s ^= s << 17; return (u32)(s >> 11); }
    int range(int lo, int hi) { return lo + (int)(next() % (u32)(hi - lo + 1)); } // inclusive
    float f() { return (next() & 0xFFFFFF) / 16777216.0f; }
    float f(float lo, float hi) { return lo + (hi - lo) * f(); }
};

struct Color { u8 r, g, b, a; };
static inline Color rgb(int r, int g, int b, int a = 255) { return Color{(u8)r, (u8)g, (u8)b, (u8)a}; }
static inline Color mix(Color a, Color b, float t) {
    return Color{(u8)(a.r + (b.r - a.r) * t), (u8)(a.g + (b.g - a.g) * t), (u8)(a.b + (b.b - a.b) * t), (u8)(a.a + (b.a - a.a) * t)};
}
static inline Color shade(Color c, float f) {
    return Color{(u8)clampf(c.r * f, 0, 255), (u8)clampf(c.g * f, 0, 255), (u8)clampf(c.b * f, 0, 255), c.a};
}
