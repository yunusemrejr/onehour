// One Hour - synthesized sound effects, ambience beds and a small mixer (no asset files)
#pragma once
#include "common.h"
#include "data.h"

// Freeverb-style room: per channel 4 damped feedback combs in parallel, then 2 allpass diffusers in series. Mono in, stereo out.
struct Reverb {
    struct Comb { std::vector<float> buf; int pos = 0; float store = 0; };
    struct Allpass { std::vector<float> buf; int pos = 0; };
    Comb comb[2][4];
    Allpass ap[2][2];
    float feedback = 0.86f, damp = 0.2f;
    void init();
    void process(float in, float& outL, float& outR);
};

struct Audio {
    bool init();
    void shutdown();
    // pos is a world position (panned, dimmed and dulled with distance from the view); isUi = true plays centred, dry and without pitch change
    void play(Sound s, Vec2 pos, bool isUi = false, float gain = 1.0f);
    // a blast `size` px across: small (< 20), medium (< 45) or large clip; louder and more reverberant the bigger it is
    void explosion(Vec2 pos, float size);
    // thunder: distance01 0 = a near, loud crack; 1 = a far rumble only (low-passed, quieter, delayed by up to ~3 s). Not positional.
    void thunder(float distance01);
    void nuke(Vec2 pos);
    // ambience beds, each 0..1 (the mixer glides to them over ~0.4 s); sea = surf, rain = drizzle and downpour, storm = gusts with heavy rain,
    // battle = a far rumble with thuds. The wind bed always runs; storm also gusts it.
    void setAmbience(float sea, float rain, float storm, float battle);
    void setListener(Vec2 camTopLeft, int viewW, int viewH) { camX = camTopLeft.x; camY = camTopLeft.y; vw = viewW; vh = viewH; }
    void setMuted(bool m) { muted = m; }
    bool isMuted() const { return muted; }
    float masterVolume = 0.8f;
    bool ok = false;
    // --soundcheck: prints every clip and bed (duration, peak, rms, clipped samples), the synth time and the mixer's CPU cost; with
    // ONEHOUR_SOUNDDUMP=<dir> it also writes each clip and bed as a 16-bit stereo WAV into that directory
    void debugStats();
private:
    static constexpr int AUDIO_VARIANTS = 3;   // synthesized variants per clip id (one is picked at random per play)
    static constexpr int MAX_VOICES = 32;
    static constexpr int PER_SOUND = 4;        // most voices of one sound id at once; the quietest is stolen
    static constexpr int BEDS = 5;             // wind, sea, rain, storm, battle
    struct Clip { std::vector<float> var[AUDIO_VARIANTS]; int nv = 0; };
    struct Voice {
        int clip = -1, var = 0;
        float pos = 0, rate = 1;               // fractional read position (linear interpolation) and its speed (pitch wobble)
        float gainL = 0, gainR = 0, send = 0;  // constant-power pan gains; reverb send
        float lp = 0, lpA = 1;                 // one-pole low-pass state and coefficient (distant sounds are duller)
        float env = 0, startTime = 0;          // loudness follower (steal the quietest) and start time (steal the oldest on a tie)
        int delay = 0;                         // frames to wait before the sound starts (thunder)
    };
    struct Bed { std::vector<float> d; size_t pos = 0; float level = 0, target = 0; };
    Clip clips[SND_COUNT];
    Voice voices[MAX_VOICES];
    Bed beds[BEDS];
    float lastPlay[SND_COUNT] = {};
    float camX = 0, camY = 0; int vw = SCREEN_W, vh = SCREEN_H;
    u32 dev = 0;
    bool muted = false;
    float clock = 0;
    float limGain = 1;                         // the limiter's gain (smoothed back up after a peak)
    Rng mixRng{ 0xA0D10 };                     // picks variants and pitch wobble (the game's own random streams are not touched)
    Reverb reverb;
    std::vector<float> bus;                    // reverb send of the block being mixed (mono)
    double synthMs = 0;
    void synthAll();
    void playAt(Sound s, Vec2 pos, bool isUi, float gain, float wet, float delaySec = 0, float fcOverride = 0);
    static void callback(void* ud, u8* stream, int len);
    void mixInto(float* out, int frames);
    double benchMix(float seconds);            // CPU fraction of one core spent mixing `seconds` of audio with every voice busy
};
extern Audio g_audio;
static const int AUDIO_RATE = 44100;
