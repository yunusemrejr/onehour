#include "audio.h"
#include <SDL2/SDL.h>
#include <chrono>

Audio g_audio;

namespace {
const float TAU = 6.2831853f;
const float BED_LOOP = 7.5f;   // seconds in one ambience loop: every LFO in a bed runs whole cycles over it, so the loop repeats cleanly
const float BED_FADE = 0.5f;   // the wrap is a crossfade of this length (a bed is synthesized 8 s long and cut to BED_LOOP)
enum { BED_WIND, BED_SEA, BED_RAIN, BED_STORM, BED_BATTLE };

// RBJ biquad (type 0 low-pass, 1 high-pass, 2 band-pass with 0 dB peak). Swept filters re-tune it every few samples, which is cheap enough.
struct Bq {
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, x1 = 0, x2 = 0, y1 = 0, y2 = 0;
    void set(int type, float fc, float q) {
        fc = clampf(fc, 20.0f, AUDIO_RATE * 0.45f);
        float w = TAU * fc / AUDIO_RATE, c = std::cos(w), al = std::sin(w) / (2 * q), a0 = 1 + al;
        float n0 = type == 0 ? (1 - c) / 2 : (type == 1 ? (1 + c) / 2 : al);
        float n1 = type == 0 ? 1 - c : (type == 1 ? -(1 + c) : 0.0f);
        float n2 = type == 2 ? -al : n0;
        b0 = n0 / a0; b1 = n1 / a0; b2 = n2 / a0; a1 = -2 * c / a0; a2 = (1 - al) / a0;
    }
    float operator()(float x) {
        float y = b0 * x + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
        x2 = x1; x1 = x; y2 = y1; y1 = y;
        return y;
    }
};

// ------------------------------------------------------------ synthesis helpers
struct Synth {
    std::vector<float> buf;
    Rng rng;
    explicit Synth(float seconds, u64 seed = 0xC0FFEE) : rng(seed) { buf.assign((size_t)(seconds * AUDIO_RATE) + 1, 0.0f); }
    int n() const { return (int)buf.size(); }
    float total() const { return buf.size() / (float)AUDIO_RATE; }
    // attack ramps up linearly, then the level falls off exponentially (time constant decay) and stops at total
    static float env(float t, float attack, float decay, float total) {
        if (t < attack) return t / attack;
        return std::exp(-(t - attack) / std::max(decay, 1e-4f)) * (t < total ? 1.0f : 0.0f);
    }
    static float ad(float t, float attack, float decay) {
        return t < attack ? t / attack : std::exp(-(t - attack) / std::max(decay, 1e-4f));
    }
    // adds f(tt, k) over [start, start + dur): tt = seconds since start, k = tt / dur (0..1 across the event)
    template <class F> void add(float start, float dur, F f) {
        int base = (int)(start * AUDIO_RATE);
        int s0 = std::max(0, base), s1 = std::min(n(), (int)((start + dur) * AUDIO_RATE));
        for (int i = s0; i < s1; i++) { float tt = (i - base) / (float)AUDIO_RATE; buf[i] += f(tt, tt / dur); }
    }
    // sine (optionally with 2nd/3rd harmonics) sweeping f0 -> f1 exponentially
    void sweep(float f0, float f1, float amp, float attack, float decay, float harm = 0, float start = 0, float dur = -1) {
        if (dur < 0) dur = total() - start;
        float ph = 0, lr = std::log(f1 / f0);
        add(start, dur, [&](float tt, float k) {
            ph += TAU * f0 * std::exp(lr * k) / AUDIO_RATE;
            float v = std::sin(ph);
            if (harm > 0) v = v * (1 - harm) + harm * (std::sin(ph * 2) * 0.5f + std::sin(ph * 3) * 0.33f);
            return v * amp * env(tt, attack, decay, dur);
        });
    }
    // FM tone: a carrier gliding fc0 -> fc1 with a modulator at ratio x its pitch; the index (metallic bite) falls from i0 to i1
    void fm(float fc0, float fc1, float ratio, float i0, float i1, float amp, float attack, float decay, float start, float dur) {
        float pc = 0, pm = 0, lr = std::log(fc1 / fc0);
        add(start, dur, [&](float tt, float k) {
            float fc = fc0 * std::exp(lr * k);
            pc += TAU * fc / AUDIO_RATE; pm += TAU * fc * ratio / AUDIO_RATE;
            return std::sin(pc + (i0 + (i1 - i0) * k) * std::sin(pm)) * amp * env(tt, attack, decay, dur);
        });
    }
    // a square with a softening sine (buzzes without the harshness of a pure square)
    void square(float f, float amp, float attack, float decay, float start, float dur) {
        float ph = 0;
        add(start, dur, [&](float tt, float) {
            ph += f / AUDIO_RATE; if (ph > 1) ph -= 1;
            float v = (ph < 0.5f ? 1.0f : -1.0f) * 0.6f + std::sin(ph * TAU) * 0.4f;
            return v * amp * env(tt, attack, decay, dur);
        });
    }
    // filtered noise burst; lp in [0,1] is a one-pole low-pass coefficient swept lp0 -> lp1 (1 = raw noise)
    void noise(float amp, float attack, float decay, float lp0, float lp1, float start = 0, float dur = -1) {
        if (dur < 0) dur = total() - start;
        float y = 0;
        add(start, dur, [&](float tt, float k) {
            y += (rng.f(-1, 1) - y) * (lp0 + (lp1 - lp0) * k);
            return y * amp * env(tt, attack, decay, dur);
        });
    }
    // band-passed noise, centre swept fc0 -> fc1 (a click, a body, a whoosh)
    void bpNoise(float fc0, float fc1, float q, float amp, float attack, float decay, float start = 0, float dur = -1) {
        if (dur < 0) dur = total() - start;
        Bq b; int c = 0;
        add(start, dur, [&](float tt, float k) {
            if ((c++ & 15) == 0) b.set(2, fc0 * std::pow(fc1 / fc0, k), q);
            return b(rng.f(-1, 1)) * amp * ad(tt, attack, decay);
        });
    }
    // brown noise (a leaky integrator: all rumble, no hiss) under an envelope e(tt, k)
    template <class E> void brown(float amp, float start, float dur, E e) {
        float y = 0;
        add(start, dur, [&](float tt, float k) {
            y = y * 0.9992f + rng.f(-1, 1) * 0.02f;
            return y * amp * e(tt, k);
        });
    }
    void brownAD(float amp, float attack, float decay, float start, float dur) {
        brown(amp, start, dur, [&](float tt, float) { return ad(tt, attack, decay); });
    }
    // random clicks (debris, arcing, grit): about `rate` a second, each a band-passed snap dying in `decay` seconds, thinning out by `taper` over dur
    void crackle(float start, float dur, float rate, float amp, float decay, float fc, float taper = 0) {
        Bq b; b.set(2, fc, 1.2f);
        float e = 0, dk = std::exp(-1 / (decay * AUDIO_RATE)), p = rate / AUDIO_RATE;
        add(start, dur, [&](float, float k) {
            if (rng.f() < p) e = amp * rng.f(0.4f, 1.0f) * (1 - taper * k);
            e *= dk;
            return b(rng.f(-1, 1)) * e;
        });
    }
    // rain: droplet ticks, each a short sine blip at a random pitch in [fLo, fHi]
    void drops(float start, float dur, float rate, float amp, float fLo, float fHi) {
        float e = 0, ph = 0, w = 0, dk = std::exp(-1 / (0.0018f * AUDIO_RATE)), p = rate / AUDIO_RATE;
        add(start, dur, [&](float, float) {
            if (rng.f() < p) { e = amp * rng.f(0.2f, 1.0f); w = TAU * rng.f(fLo, fHi) / AUDIO_RATE; ph = 0; }
            float v = std::sin(ph) * e;
            ph += w; e *= dk;
            return v;
        });
    }
    // a soft bell for the UI: a sine with a quiet octave and twelfth, fast attack, and the upper partials dying first
    void bell(float f, float amp, float start, float dur, float decay) {
        add(start, dur, [&](float tt, float) {
            float e = ad(tt, 0.003f, decay);
            return amp * e * (std::sin(TAU * f * tt) + 0.22f * std::exp(-tt / (decay * 0.5f)) * std::sin(TAU * 2 * f * tt)
                              + 0.07f * std::exp(-tt / (decay * 0.25f)) * std::sin(TAU * 3 * f * tt));
        });
    }
    void tone(float f, float amp, float start, float dur, float attack = 0.005f, float decay = 0.08f) {
        sweep(f, f, amp, attack, decay, 0.25f, start, dur);
    }
    // a bed as a seamless loop: the last `fadeSec` are equal-power blended over the first, then cut off (the seam is then two adjacent samples)
    void loop(float fadeSec) {
        int fade = (int)(fadeSec * AUDIO_RATE), m = n() - fade;
        for (int i = 0; i < fade; i++) {
            float k = i / (float)fade * 1.5707963f;
            buf[i] = buf[i] * std::sin(k) + buf[m + i] * std::cos(k);
        }
        buf.resize(m);
    }
    void normalize(float peak) {
        float m = 0; for (auto v : buf) m = std::max(m, std::abs(v));
        if (m > 1e-5f) for (auto& v : buf) v *= peak / m;
    }
    // scale to an RMS level (beds are judged by their average loudness), but never above peakMax
    void normRms(float rms, float peakMax) {
        // a leaky integrator (brown noise) carries a small offset: remove it, so a bed has no DC to thump the speakers
        double mean = 0;
        for (float v : buf) mean += v;
        mean /= buf.size();
        for (auto& v : buf) v -= (float)mean;
        double sq = 0; float pk = 0;
        for (float v : buf) { sq += v * v; pk = std::max(pk, std::abs(v)); }
        if (sq <= 0 || pk <= 0) return;
        float g = rms / std::sqrt((float)(sq / buf.size()));
        if (pk * g > peakMax) g = peakMax / pk;
        for (auto& v : buf) v *= g;
    }
};

// sends (fraction of a voice that goes to the room): blasts and cannon are wet, small arms barely, UI not at all
float sendOf(Sound s) {
    switch (s) {
    case SND_RIFLE: return 0.08f;
    case SND_MG: return 0.04f;
    case SND_LASER: case SND_LASER_HEAVY: case SND_ARC: return 0.12f;
    case SND_CANNON: return 0.3f;
    case SND_ROCKET: case SND_AIR: case SND_JET: case SND_SPLASH: case SND_SHELL_IMPACT: case SND_EMP: return 0.22f;
    case SND_RAIL: return 0.25f;
    case SND_EXPLODE_S: case SND_SHIPHORN: case SND_COLLAPSE: return 0.3f;
    case SND_EXPLODE_M: case SND_EXPLODE_L: return 0.4f;
    case SND_THUNDER: return 0.5f;
    case SND_NUKE: return 0.6f;
    default: return 0.0f;
    }
}

// soft knee above 0.75: linear below it, then a tanh roll-off that never reaches 1 (the limiter keeps the peaks near 0.9 beforehand)
float knee(float x) {
    const float K = 0.75f;
    float a = std::abs(x);
    if (a <= K) return x;
    float y = K + (1 - K) * std::tanh((a - K) / (1 - K));
    return x < 0 ? -y : y;
}

// the WAV writer for --soundcheck: 16-bit stereo (mono sources are written to both channels)
void writeWav(const std::string& path, const std::vector<float>& d) {
    FILE* f = fopen(path.c_str(), "wb");
    if (!f) { printf("  cannot write %s\n", path.c_str()); return; }
    const uint32_t rate = AUDIO_RATE, dataBytes = (uint32_t)d.size() * 4;
    unsigned char h[44];
    auto put32 = [&](int at, uint32_t v) { for (int i = 0; i < 4; i++) h[at + i] = (unsigned char)(v >> (8 * i)); };
    auto put16 = [&](int at, uint32_t v) { h[at] = (unsigned char)v; h[at + 1] = (unsigned char)(v >> 8); };
    memcpy(h, "RIFF", 4); put32(4, 36 + dataBytes); memcpy(h + 8, "WAVEfmt ", 8); put32(16, 16); put16(20, 1); put16(22, 2);
    put32(24, rate); put32(28, rate * 4); put16(32, 4); put16(34, 16); memcpy(h + 36, "data", 4); put32(40, dataBytes);
    fwrite(h, 1, 44, f);
    std::vector<unsigned char> pcm(dataBytes);
    for (size_t i = 0; i < d.size(); i++) {
        int16_t v = (int16_t)std::lrintf(clampf(d[i], -1, 1) * 32767.0f);
        pcm[i * 4] = (unsigned char)v; pcm[i * 4 + 1] = (unsigned char)((uint16_t)v >> 8);
        pcm[i * 4 + 2] = pcm[i * 4]; pcm[i * 4 + 3] = pcm[i * 4 + 1];
    }
    fwrite(pcm.data(), 1, pcm.size(), f);
    fclose(f);
}
}

// ------------------------------------------------------------ reverb
void Reverb::init() {
    // the right channel's delays are a little longer than the left's, so the room is not mono
    static const int CL[2][4] = { { 1116, 1277, 1422, 1557 }, { 1139, 1300, 1445, 1580 } };
    static const int AL[2][2] = { { 556, 341 }, { 579, 364 } };
    for (int ch = 0; ch < 2; ch++) {
        for (int c = 0; c < 4; c++) { comb[ch][c].buf.assign(CL[ch][c], 0.0f); comb[ch][c].pos = 0; comb[ch][c].store = 0; }
        for (int a = 0; a < 2; a++) { ap[ch][a].buf.assign(AL[ch][a], 0.0f); ap[ch][a].pos = 0; }
    }
}

void Reverb::process(float in, float& outL, float& outR) {
    float o[2];
    for (int ch = 0; ch < 2; ch++) {
        float acc = 0;
        for (auto& c : comb[ch]) {
            float y = c.buf[c.pos];
            c.store = y * (1 - damp) + c.store * damp;   // the damping is a low-pass inside the loop: the tail gets darker
            c.buf[c.pos] = in * 0.1f + c.store * feedback;
            if (++c.pos >= (int)c.buf.size()) c.pos = 0;
            acc += y;
        }
        for (auto& a : ap[ch]) {
            float bo = a.buf[a.pos];
            float y = bo - acc;
            a.buf[a.pos] = acc + bo * 0.5f;
            if (++a.pos >= (int)a.buf.size()) a.pos = 0;
            acc = y;
        }
        o[ch] = acc;
    }
    outL = o[0]; outR = o[1];
}

// ------------------------------------------------------------ synthesis
void Audio::synthAll() {
    auto t0 = std::chrono::steady_clock::now();
    reverb.init();
    bus.assign(8192, 0.0f);   // the reverb send of one callback block (pre-sized: the audio thread never allocates)
    // each clip is peak-normalized to its own level (UI quiet, blasts loud). Guns and blasts get AUDIO_VARIANTS takes (other seeds and a
    // few percent of pitch), so repeated shots do not sound identical.
    auto make = [&](Sound id, float sec, int nv, float peak, auto gen) {
        clips[id].nv = nv;
        for (int v = 0; v < nv; v++) {
            Synth s(sec, 0xC0FFEEull + 7919ull * (v + 1) + 104729ull * (u64)id);
            gen(s, 1 + 0.06f * (v - 1));
            s.normalize(peak);
            clips[id].var[v] = std::move(s.buf);
        }
    };
    make(SND_RIFLE, 0.16f, 3, 0.45f, [](Synth& s, float vf) {
        s.bpNoise(3600 * vf, 3600 * vf, 0.9f, 1.0f, 0.0004f, 0.0022f, 0, 0.012f);   // the click of the shot
        s.bpNoise(950 * vf, 700 * vf, 1.4f, 0.8f, 0.001f, 0.016f, 0, 0.09f);        // the body
        s.sweep(180 * vf, 70 * vf, 0.6f, 0.0008f, 0.02f, 0, 0, 0.12f);              // a low thump
        s.noise(0.25f, 0.002f, 0.05f, 0.25f, 0.05f, 0, 0.16f);                       // the tail
    });
    make(SND_MG, 0.07f, 3, 0.35f, [](Synth& s, float vf) {
        s.bpNoise(4200 * vf, 4200 * vf, 0.9f, 1.0f, 0.0003f, 0.0016f, 0, 0.008f);
        s.bpNoise(1500 * vf, 1100 * vf, 1.2f, 0.7f, 0.0008f, 0.011f, 0, 0.05f);
        s.sweep(260 * vf, 110 * vf, 0.45f, 0.0005f, 0.012f, 0, 0, 0.05f);
        s.noise(0.15f, 0.001f, 0.02f, 0.3f, 0.1f, 0, 0.07f);
    });
    make(SND_LASER, 0.22f, 3, 0.42f, [](Synth& s, float vf) {
        s.fm(1900 * vf, 420, 1.5f, 2.6f, 0.5f, 0.8f, 0.002f, 0.07f, 0, 0.22f);     // FM zap: the carrier glides down as the modulator bites
        s.bpNoise(4500 * vf, 3000 * vf, 2.0f, 0.12f, 0.001f, 0.01f, 0, 0.03f);     // sizzle
    });
    make(SND_LASER_HEAVY, 0.42f, 3, 0.55f, [](Synth& s, float vf) {
        s.fm(900 * vf, 130, 2.0f, 3.0f, 0.6f, 0.9f, 0.004f, 0.12f, 0, 0.42f);       // the charged zap
        s.sweep(90 * vf, 40, 0.7f, 0.01f, 0.2f, 0, 0, 0.42f);                        // a sub drop under it
        s.bpNoise(2200 * vf, 900, 0.7f, 0.12f, 0.002f, 0.05f, 0, 0.12f);             // hiss
    });
    make(SND_ARC, 0.3f, 3, 0.5f, [](Synth& s, float vf) {
        for (int k = 0; k < 9; k++) {                                                 // electric buzz: snaps over a 60 Hz hum
            float st = k * 0.03f + s.rng.f(0, 0.01f), a = s.rng.f(0.5f, 1.0f);
            s.bpNoise(3200 * vf, 3200 * vf, 3.0f, a, 0.0005f, 0.005f, st, 0.02f);
            s.square(60 + s.rng.f(-15, 15), 0.35f * a, 0.001f, 0.02f, st, 0.03f);
        }
        s.sweep(3000 * vf, 900, 0.3f, 0.002f, 0.05f, 0.5f, 0, 0.3f);
    });
    make(SND_CANNON, 0.5f, 3, 0.75f, [](Synth& s, float vf) {
        s.sweep(70 * vf, 42, 1.0f, 0.003f, 0.14f, 0.15f, 0, 0.3f);                   // deep thump: the pitch drops
        s.bpNoise(1800 * vf, 900, 0.8f, 0.6f, 0.0008f, 0.012f, 0, 0.05f);            // crack
        s.brownAD(0.5f, 0.01f, 0.25f, 0, 0.5f);                                       // rumble tail
    });
    make(SND_ROCKET, 0.5f, 3, 0.45f, [](Synth& s, float vf) {
        s.noise(0.9f, 0.0005f, 0.012f, 0.9f, 0.5f, 0, 0.03f);                        // ignition pop
        s.bpNoise(500 * vf, 2200, 1.2f, 0.5f, 0.02f, 0.25f, 0, 0.5f);                // whoosh: the band climbs
        s.sweep(200 * vf, 500, 0.25f, 0.02f, 0.15f, 0.3f, 0, 0.5f);
    });
    make(SND_RAIL, 0.5f, 3, 0.7f, [](Synth& s, float vf) {
        s.sweep(260 * vf, 2600, 0.4f, 0.15f, 0.25f, 0.2f, 0, 0.3f);                  // the charge whine
        s.bpNoise(5000 * vf, 2500, 0.9f, 1.0f, 0.0004f, 0.012f, 0.3f, 0.05f);        // bright crack
        s.sweep(160 * vf, 40, 0.9f, 0.004f, 0.18f, 0.1f, 0.3f, 0.2f);                // sub
        s.noise(0.2f, 0.01f, 0.1f, 0.4f, 0.05f, 0.3f, 0.2f);                         // tail
    });
    make(SND_EXPLODE_S, 0.6f, 3, 0.65f, [](Synth& s, float vf) {
        s.bpNoise(2800 * vf, 2800 * vf, 0.8f, 1.0f, 0.0003f, 0.004f, 0, 0.02f);      // transient crack
        s.sweep(95 * vf, 30, 0.9f, 0.003f, 0.15f, 0, 0, 0.5f);                       // low boom
        s.brownAD(0.6f, 0.004f, 0.24f, 0, 0.6f);                                      // rumble tail
        s.crackle(0.02f, 0.5f, 35, 0.22f, 0.004f, 2500);                              // debris
    });
    make(SND_EXPLODE_M, 0.9f, 3, 0.75f, [](Synth& s, float vf) {
        s.bpNoise(2600 * vf, 2600 * vf, 0.8f, 1.0f, 0.0004f, 0.006f, 0, 0.03f);
        s.sweep(85 * vf, 28, 0.9f, 0.004f, 0.3f, 0, 0, 0.8f);
        s.brownAD(0.7f, 0.006f, 0.35f, 0, 0.9f);
        s.crackle(0.03f, 0.7f, 45, 0.2f, 0.005f, 2000);
    });
    make(SND_EXPLODE_L, 1.4f, 3, 0.85f, [](Synth& s, float vf) {
        s.bpNoise(2200 * vf, 1000, 0.7f, 1.0f, 0.0005f, 0.008f, 0, 0.04f);           // crack
        s.sweep(75 * vf, 24, 1.0f, 0.008f, 0.42f, 0, 0, 1.2f);                       // boom
        s.sweep(160 * vf, 50, 0.35f, 0.002f, 0.08f, 0, 0, 0.3f);                     // body
        s.brownAD(0.9f, 0.01f, 0.5f, 0, 1.4f);                                        // long rumble
        s.crackle(0.05f, 1.2f, 60, 0.18f, 0.005f, 2000, 0.5f);                       // debris, thinning out
        s.noise(0.3f, 0.005f, 0.15f, 0.3f, 0.05f, 0, 0.5f);                          // whomp
    });
    make(SND_NUKE, 7.5f, 1, 0.92f, [](Synth& s, float) {
        s.bpNoise(5000, 2500, 0.6f, 1.0f, 0.0002f, 0.012f, 0, 0.08f);               // blinding crack
        s.sweep(2600, 900, 0.5f, 0.0005f, 0.06f, 0.2f, 0, 0.25f);                   // white-hot flash tone
        s.sweep(38, 24, 1.0f, 0.4f, 3.5f, 0, 0, 7.0f);                              // enormous sub, slowly falling
        float y = 0;                                                                  // the rolling roar: swells for 1.2 s, then fades
        s.add(0, 7.5f, [&](float tt, float) {
            y = y * 0.9992f + s.rng.f(-1, 1) * 0.02f;
            float e = tt < 1.2f ? std::pow(tt / 1.2f, 1.5f) : std::exp(-(tt - 1.2f) / 1.8f);
            return y * 0.9f * e;
        });
        s.sweep(70, 26, 0.9f, 0.01f, 0.6f, 0, 1.4f, 3.0f);                          // the shock wave's second boom
        s.crackle(0.8f, 5.0f, 40, 0.12f, 0.01f, 1500, 0.4f);                         // debris after
    });
    make(SND_THUNDER, 5.0f, 1, 0.9f, [](Synth& s, float) {
        s.bpNoise(4500, 2500, 0.7f, 1.0f, 0.0002f, 0.02f, 0, 0.12f);               // the sharp crack
        s.sweep(120, 60, 0.8f, 0.001f, 0.25f, 0, 0, 0.8f);                           // a thump under it
        float y = 0;                                                                  // four seconds of rolling rumble
        s.add(0.05f, 4.95f, [&](float tt, float) {
            y = y * 0.9992f + s.rng.f(-1, 1) * 0.02f;
            float roll = 0.5f + 0.5f * std::sin(TAU * 0.7f * tt);
            return y * 0.8f * roll * std::exp(-tt / 2.2f) * std::min(1.0f, tt / 0.2f);
        });
        s.sweep(45, 30, 0.7f, 0.2f, 2.5f, 0, 0.1f, 4.5f);                            // sub
    });
    make(SND_SPLASH, 0.9f, 3, 0.5f, [](Synth& s, float vf) {
        s.bpNoise(1600 * vf, 500, 1.0f, 1.0f, 0.002f, 0.14f, 0, 0.5f);              // the plunge
        s.bpNoise(6000, 3500, 0.6f, 0.35f, 0.001f, 0.12f, 0, 0.35f);                // the hiss of the spray
        for (int k = 0; k < 9; k++) {                                                 // bubbles rising to the surface
            float st = 0.12f + k * 0.07f * s.rng.f(0.6f, 1.4f);
            s.sweep(500 * vf + s.rng.f(0, 300), 1500 * vf + s.rng.f(0, 700), 0.22f * s.rng.f(0.4f, 1), 0.002f, 0.02f, 0, st, 0.07f);
        }
    });
    make(SND_SHIPHORN, 2.6f, 1, 0.5f, [](Synth& s, float) {
        s.add(0, 2.6f, [&](float tt, float) {                                         // two tones a fifth apart (110 and 165 Hz), a little vibrato
            float e = tt < 0.25f ? tt / 0.25f : (tt < 2.0f ? 1.0f : std::exp(-(tt - 2.0f) / 0.25f));
            float vib = 1 + 0.004f * std::sin(TAU * 5 * tt), v = 0;
            for (int h = 1; h <= 5; h++) v += (std::sin(TAU * 110 * h * tt * vib) + 0.7f * std::sin(TAU * 165 * h * tt * vib)) / h;
            return v * 0.2f * e;
        });
        s.bpNoise(400, 300, 1.0f, 0.1f, 0.25f, 0.3f, 0, 2.6f);                        // breath
    });
    make(SND_COLLAPSE, 2.5f, 3, 0.7f, [](Synth& s, float vf) {
        s.brown(0.9f, 0, 2.5f, [](float tt, float k) { return std::min(1.0f, tt / 0.05f) * (1 - 0.7f * k); });   // the rumble, falling away
        s.sweep(60 * vf, 35, 0.5f, 0.1f, 1.0f, 0.1f, 0, 2.5f);                       // a groan
        s.crackle(0, 2.5f, 150, 0.22f, 0.012f, 3000, 0.6f);                          // debris clatter, thinning out
    });
    make(SND_SHELL_IMPACT, 0.6f, 3, 0.6f, [](Synth& s, float vf) {
        s.sweep(95 * vf, 45, 1.0f, 0.002f, 0.07f, 0, 0, 0.3f);                        // the thud
        s.bpNoise(800 * vf, 600, 1.0f, 0.7f, 0.001f, 0.05f, 0, 0.25f);                // dirt
        s.brownAD(0.4f, 0.003f, 0.12f, 0, 0.4f);
        s.crackle(0.02f, 0.3f, 80, 0.12f, 0.003f, 3000);
    });
    make(SND_HIT, 0.05f, 1, 0.25f, [](Synth& s, float) { s.bpNoise(2500, 1800, 1.0f, 1.0f, 0.0005f, 0.008f, 0, 0.05f); });
    // ---- UI: soft bells and clean sines, never a square
    make(SND_SELECT, 0.12f, 1, 0.3f, [](Synth& s, float) { s.bell(740, 0.8f, 0, 0.12f, 0.05f); s.bell(1110, 0.5f, 0.03f, 0.1f, 0.04f); });
    make(SND_ORDER, 0.09f, 1, 0.3f, [](Synth& s, float) { s.sweep(700, 1050, 1, 0.003f, 0.05f, 0.2f); });
    make(SND_PLACE, 0.35f, 1, 0.45f, [](Synth& s, float) {
        s.sweep(130, 80, 0.8f, 0.002f, 0.05f, 0, 0, 0.2f);                            // a soft thud
        s.bell(330, 0.45f, 0, 0.3f, 0.1f); s.bell(495, 0.22f, 0.02f, 0.25f, 0.08f);
    });
    make(SND_BUILD_DONE, 0.7f, 1, 0.4f, [](Synth& s, float) {
        s.bell(523, 1, 0, 0.3f, 0.1f); s.bell(659, 1, 0.16f, 0.3f, 0.1f); s.bell(784, 1, 0.32f, 0.38f, 0.18f);
    });
    make(SND_UNIT_READY, 0.35f, 1, 0.35f, [](Synth& s, float) { s.bell(587, 1, 0, 0.25f, 0.08f); s.bell(880, 1, 0.12f, 0.3f, 0.12f); });
    make(SND_NOFUNDS, 0.3f, 1, 0.35f, [](Synth& s, float) { s.bell(330, 0.7f, 0, 0.2f, 0.06f); s.bell(262, 0.7f, 0.15f, 0.2f, 0.08f); });
    make(SND_LOWPOWER, 0.8f, 1, 0.3f, [](Synth& s, float) { s.bell(440, 0.6f, 0, 0.5f, 0.22f); s.bell(370, 0.6f, 0.38f, 0.5f, 0.25f); });
    make(SND_ATTACKED, 0.9f, 1, 0.35f, [](Synth& s, float) {
        s.sweep(900, 500, 1, 0.02f, 0.3f, 0.3f, 0, 0.4f); s.sweep(900, 500, 1, 0.02f, 0.3f, 0.3f, 0.45f, 0.4f);
    });
    make(SND_VICTORY, 1.8f, 1, 0.5f, [](Synth& s, float) {
        float notes[5] = { 523, 659, 784, 1046, 1318 };
        for (int i = 0; i < 5; i++) s.bell(notes[i], 0.9f, i * 0.18f, 0.9f, 0.25f);
        s.tone(261, 0.6f, 0, 1.6f, 0.02f, 0.9f);
    });
    make(SND_DEFEAT, 1.8f, 1, 0.5f, [](Synth& s, float) {
        float notes[3] = { 440, 349, 261 };
        for (int i = 0; i < 3; i++) s.bell(notes[i], 0.9f, i * 0.35f, 0.8f, 0.4f);
        s.sweep(110, 55, 0.8f, 0.1f, 0.8f, 0.1f, 0.7f, 1.0f);
    });
    make(SND_CLICK, 0.03f, 1, 0.2f, [](Synth& s, float) { s.sweep(1500, 1200, 1, 0.0008f, 0.006f, 0, 0, 0.03f); });
    make(SND_SUPPLY, 0.18f, 1, 0.25f, [](Synth& s, float) { s.bell(1200, 0.6f, 0, 0.08f, 0.03f); s.bell(1600, 0.6f, 0.07f, 0.11f, 0.04f); });
    make(SND_CANT, 0.14f, 1, 0.3f, [](Synth& s, float) { s.bell(196, 0.8f, 0, 0.13f, 0.05f); });
    // ---- the air: an EMP is a deep wobbling sweep
    make(SND_EMP, 1.1f, 1, 0.6f, [](Synth& s, float) {
        float ph = 0;
        s.add(0, 1.1f, [&](float tt, float k) {
            float f = 220 * std::exp(-k * 1.8f) + 40;
            ph += TAU * f / AUDIO_RATE;
            float wob = 0.5f + 0.5f * std::sin(TAU * (6 + 10 * k) * tt);
            return std::sin(ph) * wob * Synth::ad(tt, 0.01f, 0.6f) * 0.9f;
        });
        s.bpNoise(1400, 300, 2.0f, 0.4f, 0.01f, 0.4f, 0, 1.1f);                    // sizzle
        s.bpNoise(6000, 4000, 1.0f, 0.6f, 0.001f, 0.02f, 0, 0.05f);                // zap
    });
    make(SND_AIR, 0.6f, 1, 0.35f, [](Synth& s, float) {
        s.sweep(180, 420, 1, 0.05f, 0.3f, 0.5f);
        s.bpNoise(900, 400, 1.2f, 0.5f, 0.05f, 0.3f, 0, 0.6f);
    });
    // supersonic flyby: the engine's pitch falls as it passes (doppler), it swells as it comes level, then a crack and boom
    make(SND_JET, 1.5f, 1, 0.55f, [](Synth& s, float) {
        auto dop = [](float k) { return 1 - 0.25f * std::tanh(5 * (k - 0.62f)); };   // high on approach, low on the way out
        auto swell = [](float k) { return 0.1f + 0.9f * std::exp(-std::pow((k - 0.62f) / 0.3f, 2.0f)); };
        Bq b; int c = 0;
        s.add(0, 1.5f, [&](float tt, float k) {                                       // the roar
            if ((c++ & 15) == 0) b.set(2, 450 * dop(k), 1.5f);
            return b(s.rng.f(-1, 1)) * swell(k) * 1.4f;
        });
        float ph = 0;
        s.add(0, 1.5f, [&](float tt, float k) {                                       // the turbine whine
            ph += TAU * 620 * dop(k) / AUDIO_RATE;
            return std::sin(ph) * 0.25f * swell(k);
        });
        s.bpNoise(3000, 1400, 0.9f, 1.0f, 0.0005f, 0.02f, 0.9f, 0.12f);              // the crack
        s.sweep(90, 30, 0.9f, 0.003f, 0.12f, 0.1f, 0.9f, 0.6f);                      // the boom
    });
    // ---- ambience beds: 8 s synthesized, looped at 7.5 s with a 0.5 s crossfade (see Synth::loop); RMS-levelled
    auto bed = [&](int b, u64 seed, float rms, auto gen) {
        Synth s(8.0f, seed);
        gen(s);
        s.loop(BED_FADE);
        s.normRms(rms, 0.9f);
        beds[b].d = std::move(s.buf);
    };
    bed(BED_WIND, 11, 0.05f, [](Synth& s) {   // a broad moan that gusts: two swells per loop, with a whistle on the strongest gusts
        float y1 = 0, y2 = 0;
        Bq wb; wb.set(2, 650, 3.0f);
        s.add(0, 8, [&](float tt, float) {
            float w = s.rng.f(-1, 1);
            y1 += (w - y1) * 0.08f; y2 += (y1 - y2) * 0.3f;
            float gust = 0.55f + 0.3f * std::sin(TAU * tt / BED_LOOP) + 0.15f * std::sin(TAU * 3 * tt / BED_LOOP + 1.3f);
            return y2 * gust + wb(w) * 0.25f * gust * gust;
        });
    });
    bed(BED_SEA, 12, 0.08f, [](Synth& s) {   // swells of filtered noise (two per loop) and two soft crashes near their peaks
        Bq b; b.set(0, 900, 0.7f);
        s.add(0, 8, [&](float tt, float) {
            float sw = 0.5f - 0.5f * std::cos(TAU * 2 * tt / BED_LOOP);
            return b(s.rng.f(-1, 1)) * (0.15f + 0.85f * std::pow(sw, 1.4f));
        });
        for (int c = 0; c < 2; c++) {
            float t0 = 1.6f + c * 3.75f + s.rng.f(-0.2f, 0.2f);
            Bq cb; cb.set(2, 1800, 0.7f);
            s.add(t0, 2.2f, [&](float tt, float) { return cb(s.rng.f(-1, 1)) * 0.6f * Synth::ad(tt, 0.08f, 0.6f); });
        }
    });
    bed(BED_RAIN, 13, 0.06f, [](Synth& s) {   // a dense high-passed drizzle with droplet ticks
        Bq h; h.set(1, 2200, 0.6f);
        s.add(0, 8, [&](float tt, float) { return h(s.rng.f(-1, 1)) * (0.8f + 0.2f * std::sin(TAU * tt / BED_LOOP)); });
        s.drops(0, 8, 260, 0.35f, 2800, 6500);
    });
    bed(BED_STORM, 14, 0.1f, [](Synth& s) {   // strong gusty wind under heavy rain
        float y1 = 0, y2 = 0;
        Bq h; h.set(1, 1500, 0.6f);
        s.add(0, 8, [&](float tt, float) {
            float w = s.rng.f(-1, 1);
            y1 += (w - y1) * 0.08f; y2 += (y1 - y2) * 0.3f;
            float g = 0.35f + 0.65f * std::pow(0.5f + 0.5f * std::sin(TAU * 3 * tt / BED_LOOP), 2.0f) + 0.2f * std::sin(TAU * tt / BED_LOOP);
            return y2 * g * 1.5f + h(w) * 0.7f;
        });
        s.drops(0, 8, 700, 0.45f, 2500, 7000);
    });
    bed(BED_BATTLE, 15, 0.05f, [](Synth& s) {   // a low rumble that swells and eases, with far thuds and small arms
        s.brown(1.0f, 0, 8, [](float tt, float) { return 0.75f + 0.25f * std::sin(TAU * 2 * tt / BED_LOOP); });
        for (int c = 0; c < 5; c++) {
            float t0 = c * 1.5f + s.rng.f(0, 1.2f), a = s.rng.f(0.5f, 1.0f);
            s.sweep(70, 35, a, 0.01f, 0.25f, 0, t0, 1.0f);
            Bq tb; tb.set(0, 500, 0.7f);
            s.add(t0, 0.5f, [&](float tt, float) { return tb(s.rng.f(-1, 1)) * 0.5f * a * Synth::ad(tt, 0.005f, 0.1f); });
        }
        s.crackle(0, 8, 5, 0.05f, 0.01f, 1400);
    });
    beds[BED_WIND].target = 0.35f;   // the wind runs from the start
    synthMs = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
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
    if ((int)bus.size() < frames) bus.resize(frames);
    std::fill(bus.begin(), bus.begin() + frames, 0.0f);
    // ambience beds: each glides to its target over ~0.4 s and loops on its own clock
    float glide = 1 - std::exp(-frames / (0.4f * AUDIO_RATE));
    for (auto& b : beds) {
        b.level += (b.target - b.level) * glide;
        if (b.d.empty()) continue;
        float g = b.level * master;
        size_t n = b.d.size();
        for (int i = 0; i < frames; i++) {
            float v = b.d[b.pos] * g;
            out[i * 2] += v; out[i * 2 + 1] += v;
            if (++b.pos >= n) b.pos = 0;
        }
    }
    // voices: fractional read with linear interpolation, one-pole low-pass, constant-power pan, and a send to the room
    for (auto& v : voices) {
        if (v.clip < 0) continue;
        const std::vector<float>& d = clips[v.clip].var[v.var];
        int size = (int)d.size();
        float gl = v.gainL * master, gr = v.gainR * master, send = v.send * master;
        for (int i = 0; i < frames; i++) {
            if (v.delay > 0) { v.delay--; continue; }
            int idx = (int)v.pos;
            if (idx >= size - 1) { v.clip = -1; break; }
            float s = d[idx] + (d[idx + 1] - d[idx]) * (v.pos - idx);
            v.pos += v.rate;
            v.lp += (s - v.lp) * v.lpA;
            v.env = std::max(std::abs(v.lp), v.env * 0.99993f);
            out[i * 2] += v.lp * gl; out[i * 2 + 1] += v.lp * gr;
            bus[i] += v.lp * send;
        }
    }
    // room, then a limiter: the gain drops at once when the peak passes 0.9 and climbs back over ~0.15 s; the knee keeps every sample under 1
    const float lim = 0.9f, relK = 1 - std::exp(-1 / (0.15f * AUDIO_RATE));
    for (int i = 0; i < frames; i++) {
        float wl, wr;
        reverb.process(bus[i], wl, wr);
        float L = out[i * 2] + wl * 0.6f, R = out[i * 2 + 1] + wr * 0.6f;
        float peak = std::max(std::abs(L), std::abs(R));
        float target = peak > lim ? lim / peak : 1.0f;
        if (target < limGain) limGain = target; else limGain += (target - limGain) * relK;
        out[i * 2] = knee(L * limGain); out[i * 2 + 1] = knee(R * limGain);
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

// ------------------------------------------------------------ playing
void Audio::playAt(Sound s, Vec2 pos, bool isUi, float gain, float wet, float delaySec, float fcOverride) {
    if (!ok || s <= SND_NONE || s >= SND_COUNT || clips[s].nv == 0) return;
    float gl = gain * 0.7f, gr = gain * 0.7f, fc = 16000.0f;
    if (!isUi) {
        // attenuate by distance outside the view; the farther, the quieter and the duller (a one-pole low-pass)
        float dx = 0, dy = 0;
        if (pos.x < camX) dx = camX - pos.x; else if (pos.x > camX + vw) dx = pos.x - camX - vw;
        if (pos.y < camY) dy = camY - pos.y; else if (pos.y > camY + vh) dy = pos.y - camY - vh;
        float d = std::sqrt(dx * dx + dy * dy);
        float dist = clampf(d / 700.0f, 0.0f, 1.0f);
        float att = 1.0f - dist;
        if (att <= 0.02f) return;
        att *= att;
        float pan = clampf((pos.x - camX - vw * 0.5f) / (vw * 0.7f), -1, 1);
        float th = (pan + 1) * 0.25f * 3.14159265f;   // constant power: cos^2 + sin^2 = 1 across the stereo field
        gl = gain * att * 0.7f * std::cos(th);
        gr = gain * att * 0.7f * std::sin(th);
        fc = 16000.0f * std::pow(1400.0f / 16000.0f, dist);
    }
    if (fcOverride > 0) fc = fcOverride;
    if (dev) SDL_LockAudioDevice(dev);
    // throttle: the same sound at most every 30 ms (a gun firing on every tick is one sound, not a buzz)
    if (delaySec <= 0 && clock - lastPlay[s] < 0.03f) { if (dev) SDL_UnlockAudioDevice(dev); return; }
    lastPlay[s] = clock;
    // a voice: a free one, unless this sound already has its share; otherwise steal the quietest (then the oldest) of this sound, or of all
    int same = 0;
    for (auto& v : voices) if (v.clip == s) same++;
    int slot = -1;
    if (same < PER_SOUND) for (int i = 0; i < MAX_VOICES; i++) if (voices[i].clip < 0) { slot = i; break; }
    if (slot < 0) {
        float best = 1e18f, bestT = 1e18f;
        for (int i = 0; i < MAX_VOICES; i++) {
            const Voice& v = voices[i];
            if (same >= PER_SOUND && v.clip != s) continue;
            if (v.env < best || (v.env == best && v.startTime < bestT)) { best = v.env; bestT = v.startTime; slot = i; }
        }
    }
    Voice& v = voices[slot];
    v.clip = s;
    v.var = mixRng.range(0, clips[s].nv - 1);
    v.pos = 0;
    v.rate = isUi ? 1.0f : 1.0f + mixRng.f(-0.05f, 0.05f);
    v.gainL = gl; v.gainR = gr;
    v.send = isUi && wet <= 0 ? 0.0f : wet;
    v.lp = 0;
    v.lpA = 1 - std::exp(-TAU * fc / AUDIO_RATE);
    v.env = gain * 0.3f;   // a prior until the mixer has measured it
    v.startTime = clock;
    v.delay = (int)(delaySec * AUDIO_RATE);
    if (dev) SDL_UnlockAudioDevice(dev);
}

void Audio::play(Sound s, Vec2 pos, bool isUi, float gain) {
    playAt(s, pos, isUi, gain, isUi ? 0.0f : sendOf(s));
}

void Audio::explosion(Vec2 pos, float size) {
    Sound s = size < 20 ? SND_EXPLODE_S : (size < 45 ? SND_EXPLODE_M : SND_EXPLODE_L);
    float g = clampf(0.45f + size / 120.0f, 0.45f, 1.0f);
    float wet = clampf(0.15f + size / 150.0f, 0.15f, 0.6f);
    playAt(s, pos, false, g, wet);
}

void Audio::thunder(float d) {
    d = clampf(d, 0, 1);
    // a far strike loses its crack (the low-pass takes the top off) and arrives later: the sound travels about 3 s a km
    playAt(SND_THUNDER, Vec2(), true, 1.0f - 0.4f * d, 0.5f, 0.2f + 2.5f * d, 16000.0f * std::pow(200.0f / 16000.0f, d));
}

void Audio::nuke(Vec2 pos) {
    playAt(SND_NUKE, pos, false, 1.0f, sendOf(SND_NUKE));
}

void Audio::setAmbience(float sea, float rain, float storm, float battle) {
    if (dev) SDL_LockAudioDevice(dev);
    beds[BED_WIND].target = 0.35f + 0.5f * clampf(storm, 0, 1);
    beds[BED_SEA].target = clampf(sea, 0, 1);
    beds[BED_RAIN].target = clampf(rain, 0, 1);
    beds[BED_STORM].target = clampf(storm, 0, 1);
    beds[BED_BATTLE].target = clampf(battle, 0, 1);
    if (dev) SDL_UnlockAudioDevice(dev);
}

// ------------------------------------------------------------ checks
double Audio::benchMix(float seconds) {
    // a busy battle: every voice holds a long clip, panned, with the room send; the beds run at full level
    static const Sound busy[8] = { SND_EXPLODE_L, SND_CANNON, SND_LASER, SND_MG, SND_RIFLE, SND_ROCKET, SND_RAIL, SND_ARC };
    for (auto& b : beds) { b.target = 1; b.level = 1; }
    for (auto& v : voices) v.clip = -1;
    const int frames = 512;
    std::vector<float> out(frames * 2);
    int blocks = (int)(seconds * AUDIO_RATE / frames);
    double spent = 0;
    for (int b = 0; b < blocks; b++) {
        for (int i = 0; i < MAX_VOICES; i++) {
            Voice& v = voices[i];
            if (v.clip >= 0) continue;
            v.clip = busy[i % 8]; v.var = i % clips[v.clip].nv; v.pos = 0; v.rate = 1.01f;
            v.gainL = v.gainR = 0.4f; v.send = 0.3f; v.lpA = 0.3f; v.lp = 0; v.env = 0.3f; v.startTime = clock;
        }
        memset(out.data(), 0, out.size() * sizeof(float));
        auto t0 = std::chrono::steady_clock::now();
        mixInto(out.data(), frames);
        spent += std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
    }
    for (auto& v : voices) v.clip = -1;
    return spent / seconds;
}

void Audio::debugStats() {
    if (clips[SND_RIFLE].nv == 0) synthAll();
    static const char* NAMES[SND_COUNT] = { "none", "rifle", "mg", "laser", "laser_heavy", "arc", "cannon", "rocket", "rail", "explode_s", "explode_l", "hit", "select", "order", "place", "build_done", "unit_ready", "nofunds", "lowpower", "attacked", "victory", "defeat", "click", "emp", "supply", "air", "cant", "jet", "explode_m", "nuke", "thunder", "splash", "shiphorn", "collapse", "shell_impact" };
    static const char* BED_NAMES[BEDS] = { "wind", "sea", "rain", "storm", "battle" };
    const char* dump = getenv("ONEHOUR_SOUNDDUMP");
    // one line per id: length of the first take, how many takes, and peak / RMS / clipped samples over all takes
    auto line = [](const std::string& name, const std::vector<float>* takes, int nt) {
        float peak = 0, sq = 0; size_t cnt = 0; int clipped = 0;
        for (int t = 0; t < nt; t++)
            for (float v : takes[t]) { peak = std::max(peak, std::abs(v)); sq += v * v; cnt++; if (std::abs(v) >= 0.999f) clipped++; }
        printf("  %-14s %5.2fs x%d peak %.2f rms %.3f clipped %d\n", name.c_str(), takes[0].size() / (float)AUDIO_RATE, nt,
               peak, std::sqrt(sq / std::max<size_t>(1, cnt)), clipped);
    };
    int clipCount = 0, bedCount = 0;
    for (int i = 1; i < SND_COUNT; i++) {
        if (clips[i].nv == 0) continue;
        line(NAMES[i], clips[i].var, clips[i].nv);
        clipCount++;
        if (dump) for (int v = 0; v < clips[i].nv; v++) writeWav(std::string(dump) + "/" + NAMES[i] + "_" + std::to_string(v) + ".wav", clips[i].var[v]);
    }
    for (int b = 0; b < BEDS; b++) {
        if (beds[b].d.empty()) continue;
        line(std::string("bed ") + BED_NAMES[b], &beds[b].d, 1);
        bedCount++;
        if (dump) writeWav(std::string(dump) + "/bed_" + BED_NAMES[b] + ".wav", beds[b].d);
    }
    printf("synth %.0f ms for %d clips and %d ambience beds\n", synthMs, clipCount, bedCount);
    double load = benchMix(1.0f);
    printf("mixer: %.2f%% of one core for 1 s of audio with %d busy voices (reverb and beds on)\n", load * 100, MAX_VOICES);
    if (dump) printf("sound dump written to %s\n", dump);
}
