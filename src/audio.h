// One Hour - synthesized sound effects and a small mixer (no asset files)
#pragma once
#include "common.h"
#include "data.h"

struct Audio {
    bool init();
    void shutdown();
    // pos is a world position; isUi = true plays at full volume centered
    void play(Sound s, Vec2 pos, bool isUi = false, float gain = 1.0f);
    void setListener(Vec2 camTopLeft, int viewW, int viewH) { camX = camTopLeft.x; camY = camTopLeft.y; vw = viewW; vh = viewH; }
    void setMuted(bool m) { muted = m; }
    bool isMuted() const { return muted; }
    float masterVolume = 0.8f;
    bool ok = false;
    void debugStats();   // prints peak/RMS/duration per clip
private:
    struct Clip { std::vector<float> data; };
    struct Voice { int clip = -1; size_t pos = 0; float gainL = 0, gainR = 0; bool loop = false; float startTime = 0; };
    static const int MAX_VOICES = 24;
    Clip clips[SND_COUNT];
    Voice voices[MAX_VOICES];
    Voice ambient;
    float lastPlay[SND_COUNT] = {};
    int playsThisFrame[SND_COUNT] = {};
    float camX = 0, camY = 0; int vw = SCREEN_W, vh = SCREEN_H;
    u32 dev = 0;
    bool muted = false;
    float clock = 0;
    std::vector<float> ambientBuf;
    void synthAll();
    static void callback(void* ud, u8* stream, int len);
    void mixInto(float* out, int frames);
};
extern Audio g_audio;
static const int AUDIO_RATE = 22050;
