#include "audio.h"
#include <SDL2/SDL.h>

Audio g_audio;

// ------------------------------------------------------------ synthesis helpers
namespace {
struct Synth {
    std::vector<float> buf;
    Rng rng{ 0xC0FFEE };
    float t = 0;
    explicit Synth(float seconds) { buf.assign((size_t)(seconds * AUDIO_RATE) + 1, 0.0f); }
    int n() const { return (int)buf.size(); }
    static float env(float t, float attack, float decay, float total) {
        if (t < attack) return t / attack;
        float d = (t - attack) / std::max(decay, 1e-4f);
        return std::exp(-d) * (t < total ? 1.0f : 0.0f);
    }
    // sine sweep from f0 to f1 over the buffer with exponential decay
    void sweep(float f0, float f1, float amp, float attack, float decay, float harmonics = 0, float start = 0, float dur = -1) {
        if (dur < 0) dur = n() / (float)AUDIO_RATE - start;
        float phase = 0;
        int s0 = (int)(start * AUDIO_RATE), s1 = std::min(n(), (int)((start + dur) * AUDIO_RATE));
        for (int i = s0; i < s1; i++) {
            float tt = (i - s0) / (float)AUDIO_RATE;
            float k = tt / dur;
            float f = f0 * std::pow(f1 / f0, k);
            phase += 6.2831853f * f / AUDIO_RATE;
            float v = std::sin(phase);
            if (harmonics > 0) v = v * (1 - harmonics) + harmonics * (std::sin(phase * 2) * 0.5f + std::sin(phase * 3) * 0.33f);
            buf[i] += v * amp * env(tt, attack, decay, dur);
        }
    }
    void square(float f, float amp, float attack, float decay, float start, float dur) {
        float phase = 0;
        int s0 = (int)(start * AUDIO_RATE), s1 = std::min(n(), (int)((start + dur) * AUDIO_RATE));
        for (int i = s0; i < s1; i++) {
            float tt = (i - s0) / (float)AUDIO_RATE;
            phase += f / AUDIO_RATE; if (phase > 1) phase -= 1;
            float v = phase < 0.5f ? 1.0f : -1.0f;
            v = v * 0.6f + std::sin(phase * 6.2831853f) * 0.4f;
            buf[i] += v * amp * env(tt, attack, decay, dur);
        }
    }
    // filtered noise burst; lp in [0,1] controls low-pass (1 = raw noise)
    void noise(float amp, float attack, float decay, float lp0, float lp1, float start = 0, float dur = -1) {
        if (dur < 0) dur = n() / (float)AUDIO_RATE - start;
        int s0 = (int)(start * AUDIO_RATE), s1 = std::min(n(), (int)((start + dur) * AUDIO_RATE));
        float y = 0;
        for (int i = s0; i < s1; i++) {
            float tt = (i - s0) / (float)AUDIO_RATE;
            float k = tt / dur;
            float lp = lp0 + (lp1 - lp0) * k;
            float w = rng.f(-1, 1);
            y += (w - y) * lp;
            buf[i] += y * amp * env(tt, attack, decay, dur);
        }
    }
    void tone(float f, float amp, float start, float dur, float attack = 0.005f, float decay = 0.08f) {
        sweep(f, f, amp, attack, decay, 0.25f, start, dur);
    }
    void clip() { for (auto& v : buf) v = clampf(v, -1, 1); }
    void normalize(float peak) {
        float m = 0; for (auto v : buf) m = std::max(m, std::abs(v));
        if (m > 1e-5f) for (auto& v : buf) v *= peak / m;
    }
};
}

void Audio::synthAll() {
    auto set = [&](Sound s, Synth& sy, float peak) { sy.normalize(peak); clips[s].data = sy.buf; };
    { Synth s(0.12f); s.noise(1, 0.002f, 0.03f, 0.9f, 0.4f); s.sweep(400, 120, 0.5f, 0.001f, 0.02f); set(SND_RIFLE, s, 0.45f); }
    { Synth s(0.07f); s.noise(1, 0.001f, 0.018f, 0.95f, 0.5f); s.sweep(300, 90, 0.5f, 0.001f, 0.015f); set(SND_MG, s, 0.35f); }
    { Synth s(0.22f); s.sweep(2200, 380, 1, 0.004f, 0.07f, 0.35f); s.noise(0.25f, 0.002f, 0.03f, 0.6f, 0.2f); set(SND_LASER, s, 0.42f); }
    { Synth s(0.42f); s.sweep(1100, 140, 1, 0.006f, 0.14f, 0.45f); s.sweep(80, 45, 0.7f, 0.01f, 0.15f); s.noise(0.2f, 0.002f, 0.05f, 0.5f, 0.1f); set(SND_LASER_HEAVY, s, 0.55f); }
    { Synth s(0.3f);
      for (int k = 0; k < 9; k++) { float st = k * 0.03f + s.rng.f(0, 0.01f); s.noise(0.9f, 0.001f, 0.012f, 1.0f, 0.7f, st, 0.03f); s.square(60 + s.rng.f(-15, 15), 0.5f, 0.001f, 0.02f, st, 0.03f); }
      s.sweep(3000, 900, 0.3f, 0.002f, 0.05f, 0.5f); set(SND_ARC, s, 0.5f); }
    { Synth s(0.45f); s.sweep(110, 38, 1, 0.004f, 0.12f, 0.2f); s.noise(0.9f, 0.001f, 0.06f, 0.5f, 0.08f); set(SND_CANNON, s, 0.75f); }
    { Synth s(0.5f); s.noise(1, 0.02f, 0.18f, 0.08f, 0.6f); s.sweep(700, 220, 0.25f, 0.02f, 0.15f, 0.3f); set(SND_ROCKET, s, 0.45f); }
    { Synth s(0.5f); s.noise(1, 0.001f, 0.02f, 1.0f, 0.9f, 0, 0.05f); s.sweep(3200, 1800, 0.8f, 0.001f, 0.05f, 0.4f); s.sweep(150, 40, 0.9f, 0.005f, 0.2f, 0.1f, 0.02f); s.noise(0.5f, 0.01f, 0.15f, 0.3f, 0.05f, 0.03f); set(SND_RAIL, s, 0.7f); }
    { Synth s(0.6f); s.noise(1, 0.004f, 0.16f, 0.35f, 0.05f); s.sweep(90, 30, 0.8f, 0.005f, 0.18f); set(SND_EXPLODE_S, s, 0.65f); }
    { Synth s(1.4f); s.noise(1, 0.006f, 0.4f, 0.3f, 0.03f); s.sweep(70, 22, 1.0f, 0.01f, 0.45f); s.noise(0.5f, 0.05f, 0.5f, 0.1f, 0.02f, 0.1f); set(SND_EXPLODE_L, s, 0.85f); }
    { Synth s(0.05f); s.noise(1, 0.001f, 0.012f, 0.8f, 0.4f); set(SND_HIT, s, 0.25f); }
    { Synth s(0.12f); s.tone(660, 1, 0, 0.05f); s.tone(990, 1, 0.05f, 0.06f); set(SND_SELECT, s, 0.3f); }
    { Synth s(0.09f); s.sweep(700, 1050, 1, 0.003f, 0.05f, 0.2f); set(SND_ORDER, s, 0.3f); }
    { Synth s(0.35f); s.noise(0.7f, 0.002f, 0.06f, 0.3f, 0.05f); s.tone(220, 0.8f, 0.02f, 0.25f, 0.01f, 0.12f); s.tone(330, 0.5f, 0.02f, 0.25f, 0.01f, 0.12f); set(SND_PLACE, s, 0.45f); }
    { Synth s(0.7f); s.tone(523, 1, 0, 0.16f, 0.01f, 0.1f); s.tone(659, 1, 0.16f, 0.16f, 0.01f, 0.1f); s.tone(784, 1, 0.32f, 0.3f, 0.01f, 0.18f); set(SND_BUILD_DONE, s, 0.4f); }
    { Synth s(0.35f); s.tone(587, 1, 0, 0.12f, 0.01f, 0.08f); s.tone(880, 1, 0.12f, 0.2f, 0.01f, 0.12f); set(SND_UNIT_READY, s, 0.35f); }
    { Synth s(0.3f); s.square(180, 1, 0.005f, 0.1f, 0, 0.14f); s.square(160, 1, 0.005f, 0.1f, 0.15f, 0.14f); set(SND_NOFUNDS, s, 0.35f); }
    { Synth s(0.8f); s.square(440, 1, 0.01f, 0.25f, 0, 0.3f); s.square(330, 1, 0.01f, 0.25f, 0.35f, 0.3f); set(SND_LOWPOWER, s, 0.3f); }
    { Synth s(0.9f); s.sweep(900, 500, 1, 0.02f, 0.3f, 0.3f, 0, 0.4f); s.sweep(900, 500, 1, 0.02f, 0.3f, 0.3f, 0.45f, 0.4f); set(SND_ATTACKED, s, 0.35f); }
    { Synth s(1.8f); float notes[5] = {523, 659, 784, 1046, 1318}; for (int i = 0; i < 5; i++) s.tone(notes[i], 1, i * 0.18f, 0.5f, 0.01f, 0.25f); s.tone(261, 0.6f, 0, 1.6f, 0.02f, 0.9f); set(SND_VICTORY, s, 0.5f); }
    { Synth s(1.8f); float notes[3] = {440, 349, 261}; for (int i = 0; i < 3; i++) s.tone(notes[i], 1, i * 0.35f, 0.8f, 0.02f, 0.4f); s.sweep(110, 55, 0.8f, 0.1f, 0.8f, 0.1f, 0.7f, 1.0f); set(SND_DEFEAT, s, 0.5f); }
    { Synth s(0.03f); s.noise(1, 0.001f, 0.006f, 0.9f, 0.9f); set(SND_CLICK, s, 0.2f); }
    { Synth s(1.1f); s.sweep(200, 2400, 1, 0.02f, 0.4f, 0.3f, 0, 0.45f); s.sweep(2400, 40, 1, 0.005f, 0.35f, 0.2f, 0.45f, 0.6f); s.noise(0.4f, 0.01f, 0.2f, 0.4f, 0.1f, 0.45f); set(SND_EMP, s, 0.6f); }
    { Synth s(0.18f); s.tone(1200, 1, 0, 0.06f, 0.003f, 0.04f); s.tone(1600, 1, 0.07f, 0.1f, 0.003f, 0.06f); set(SND_SUPPLY, s, 0.25f); }
    { Synth s(0.6f); s.sweep(180, 420, 1, 0.05f, 0.3f, 0.5f); s.noise(0.4f, 0.05f, 0.3f, 0.15f, 0.3f); set(SND_AIR, s, 0.35f); }
    { Synth s(0.14f); s.square(140, 1, 0.003f, 0.08f, 0, 0.13f); set(SND_CANT, s, 0.3f); }
    // supersonic flyby: a rising turbine whine, a roar that swells and fades, and a sharp crack as the jet breaks the sound barrier
    { Synth s(1.5f); s.sweep(380, 1500, 0.35f, 0.25f, 0.5f, 0.5f, 0, 0.6f); s.noise(1, 0.3f, 0.45f, 0.06f, 0.5f, 0, 1.3f); s.sweep(140, 55, 0.7f, 0.2f, 0.5f, 0.3f);
      s.noise(1.0f, 0.002f, 0.045f, 0.95f, 0.4f, 0.38f, 0.14f); s.sweep(90, 30, 0.9f, 0.003f, 0.12f, 0.1f, 0.38f, 0.5f); set(SND_JET, s, 0.55f); }
    // ambient wind loop (4 seconds, seamless-ish via crossfade)
    { Synth s(4.0f); s.noise(1, 0.5f, 100.0f, 0.02f, 0.03f);
      int n = s.n();
      for (int i = 0; i < n; i++) { float k = i / (float)n; s.buf[i] *= 0.7f + 0.3f * std::sin(k * 6.2831853f * 2 + 1.0f); }
      int fade = AUDIO_RATE / 4;
      for (int i = 0; i < fade; i++) { float k = i / (float)fade; s.buf[i] = s.buf[i] * k + s.buf[n - fade + i] * (1 - k); }
      s.normalize(0.11f);
      ambientBuf.assign(s.buf.begin(), s.buf.begin() + (n - fade)); }   // (the loop ends where the blended head began, so the wrap-around has no step)
}

// ------------------------------------------------------------ device
void Audio::callback(void* ud, u8* stream, int len) {
    Audio* a = (Audio*)ud;
    float* out = (float*)stream;
    int frames = len / (int)(sizeof(float) * 2);
    memset(out, 0, len);
    a->mixInto(out, frames);
}

void Audio::mixInto(float* out, int frames) {
    float master = muted ? 0.0f : masterVolume;
    // ambient
    if (!ambientBuf.empty()) {
        for (int i = 0; i < frames; i++) {
            float v = ambientBuf[ambient.pos] * master;
            ambient.pos = (ambient.pos + 1) % ambientBuf.size();
            out[i * 2] += v; out[i * 2 + 1] += v;
        }
    }
    for (auto& v : voices) {
        if (v.clip < 0) continue;
        const std::vector<float>& d = clips[v.clip].data;
        for (int i = 0; i < frames; i++) {
            if (v.pos >= d.size()) { v.clip = -1; break; }
            float s = d[v.pos++] * master;
            out[i * 2] += s * v.gainL; out[i * 2 + 1] += s * v.gainR;
        }
    }
    for (int i = 0; i < frames * 2; i++) {
        float x = out[i];
        out[i] = x < -1 ? -1 : (x > 1 ? 1 : x);   // soft-ish limiter
    }
    clock += frames / (float)AUDIO_RATE;
}

bool Audio::init() {
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) { fprintf(stderr, "audio: %s\n", SDL_GetError()); return false; }
    synthAll();
    SDL_AudioSpec want, have;
    SDL_zero(want);
    want.freq = AUDIO_RATE; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 512;
    want.callback = callback; want.userdata = this;
    dev = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!dev) { fprintf(stderr, "audio: open failed: %s (continuing without sound)\n", SDL_GetError()); return false; }
    SDL_PauseAudioDevice(dev, 0);
    ok = true;
    return true;
}

void Audio::shutdown() {
    if (dev) { SDL_CloseAudioDevice(dev); dev = 0; }
}

void Audio::play(Sound s, Vec2 pos, bool isUi, float gain) {
    if (!ok || s <= SND_NONE || s >= SND_COUNT || clips[s].data.empty()) return;
    float gl = gain, gr = gain;
    if (!isUi) {
        // attenuate by distance outside the view
        float dx = 0, dy = 0;
        if (pos.x < camX) dx = camX - pos.x; else if (pos.x > camX + vw) dx = pos.x - camX - vw;
        if (pos.y < camY) dy = camY - pos.y; else if (pos.y > camY + vh) dy = pos.y - camY - vh;
        float d = std::sqrt(dx * dx + dy * dy);
        float att = clampf(1.0f - d / 700.0f, 0.0f, 1.0f);
        if (att <= 0.02f) return;
        att *= att;
        float pan = clampf((pos.x - camX - vw * 0.5f) / (vw * 0.7f), -1, 1);
        gl = gain * att * (1 - pan * 0.5f) * 0.5f;
        gr = gain * att * (1 + pan * 0.5f) * 0.5f;
    }
    SDL_LockAudioDevice(dev);
    // throttle: the same sound at most every 40ms (the oldest voice is stolen when all are busy)
    if (clock - lastPlay[s] < 0.04f) { SDL_UnlockAudioDevice(dev); return; }
    lastPlay[s] = clock;
    int slot = -1; float oldest = 1e18f;
    for (int i = 0; i < MAX_VOICES; i++) {
        if (voices[i].clip < 0) { slot = i; break; }
        if (voices[i].startTime < oldest) { oldest = voices[i].startTime; slot = i; }
    }
    Voice& v = voices[slot];
    v.clip = s; v.pos = 0; v.gainL = gl; v.gainR = gr; v.startTime = clock;
    SDL_UnlockAudioDevice(dev);
}

void Audio::debugStats() {
    if (clips[SND_RIFLE].data.empty()) synthAll();
    static const char* NAMES[SND_COUNT] = { "none", "rifle", "mg", "laser", "laser_heavy", "arc", "cannon", "rocket", "rail", "explode_s", "explode_l", "hit", "select", "order", "place", "build_done", "unit_ready", "nofunds", "lowpower", "attacked", "victory", "defeat", "click", "emp", "supply", "air", "cant", "jet" };
    for (int i = 1; i < SND_COUNT; i++) {
        const auto& d = clips[i].data;
        float peak = 0, sq = 0; int clipped = 0;
        for (float v : d) { peak = std::max(peak, std::abs(v)); sq += v * v; if (std::abs(v) >= 0.999f) clipped++; }
        printf("  %-12s %5.2fs peak %.2f rms %.3f clipped %d\n", NAMES[i], d.size() / (float)AUDIO_RATE, peak, std::sqrt(sq / std::max<size_t>(1, d.size())), clipped);
    }
    printf("  ambient loop %.2fs\n", ambientBuf.size() / (float)AUDIO_RATE);
}
