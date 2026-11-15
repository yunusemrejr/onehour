// Science Break: every ten minutes of a match the game stops on a chalkboard with one lesson of mathematics or physics (facts.cpp).
// The promise button wakes after SCIENCE_LOCK seconds and must be clicked SCIENCE_CLICKS times; each click chalks one tally stroke.
// Lessons are dealt from a shuffled deck, so none repeats in one session until all 200 have been shown.
#include "ui.h"
#include "facts.h"
#include <algorithm>
#include <ctime>
#include <random>

static const Color BOARD = rgb(28, 56, 46), BOARD_DEEP = rgb(20, 42, 35), FRAME = rgb(118, 80, 46);
static const Color CHALK = rgb(236, 238, 226), CHALK_DIM = rgb(150, 178, 162), CHALK_YELLOW = rgb(250, 220, 112);
static const char* PROMISE = "Yes, I read it and I promise I learned it";

static std::vector<int> g_deck;   // lessons not yet shown this session, in the order they will come
static int g_shown = 0;           // lessons shown this session

static int dealLesson() {
    if (g_deck.empty()) {   // a fresh order of all of them (after a whole round in one very long session)
        for (int i = 0; i < SCIENCE_FACT_COUNT; i++) g_deck.push_back(i);
        std::mt19937 rng((u32)std::time(nullptr) ^ (u32)SDL_GetPerformanceCounter());
        std::shuffle(g_deck.begin(), g_deck.end(), rng);
    }
    int f = g_deck.back();
    g_deck.pop_back();
    g_shown++;
    return f;
}

void Game::checkScience() {
    if (!scienceBreaks || scienceOpen || menuOpen || g_sim.gameOver || g_sim.players[g_sim.humanPlayer].isAI) return;
    if (g_sim.time >= scienceNextAt) openScience();
}

void Game::openScience(int fact) {
    scienceFact = fact >= 0 ? fact % SCIENCE_FACT_COUNT : dealLesson();
    if (fact >= 0) g_shown = std::max(g_shown, 1);
    scienceClicks = 0;
    scienceOpenedAt = wallTime;
    pausedBeforeScience = paused;
    paused = true;
    scienceOpen = true;
    cancelModes(); dragging = false; midDrag = false;
    g_audio.play(SND_CLICK, Vec2(), true);
}

// The panel and its promise button, laid out the same way for drawing and for clicks
struct SciLayout { int x0, y0, w, h, pad; std::vector<std::string> lines; int bx, by, bw, bh; };
static SciLayout sciLayout(Gfx& g, int fact) {
    SciLayout L;
    L.pad = 40;
    L.w = std::min(880, SCREEN_W - 40);
    L.lines = wrapTip(g, SCIENCE_FACTS[fact].text, (L.w - 2 * L.pad) / 2);   // (the lesson is drawn at scale 2)
    L.h = 64 + 30 + (int)L.lines.size() * 24 + 40 + 44 + 36;
    L.x0 = SCREEN_W / 2 - L.w / 2;
    L.y0 = std::max(16, SCREEN_H / 2 - L.h / 2 - 10);
    L.bw = g.textW(PROMISE, 2) + 36;
    L.bh = 44;
    L.bx = L.x0 + L.pad;
    L.by = L.y0 + L.h - 36 - L.bh;
    return L;
}
bool Game::scienceButton(int& x, int& y, int& w, int& h) {
    SciLayout L = sciLayout(g_gfx, scienceFact);
    x = L.bx; y = L.by; w = L.bw; h = L.bh;
    return wallTime - scienceOpenedAt >= SCIENCE_LOCK;
}

void Game::scienceEvent(const SDL_Event& e) {
    if (e.type != SDL_MOUSEBUTTONDOWN || e.button.button != SDL_BUTTON_LEFT) return;   // (keys do nothing here: no Esc out of a lesson)
    int x, y, w, h;
    bool awake = scienceButton(x, y, w, h);
    if (mouseX < x || mouseX >= x + w || mouseY < y || mouseY >= y + h) return;
    if (!awake) { g_audio.play(SND_CANT, Vec2(), true); return; }
    g_audio.play(SND_CLICK, Vec2(), true);
    if (++scienceClicks < SCIENCE_CLICKS) return;
    scienceOpen = false;
    paused = pausedBeforeScience;
    while (scienceNextAt <= g_sim.time) scienceNextAt += SCIENCE_EVERY;
}

void Game::renderScience() {
    Gfx& g = g_gfx;
    const ScienceFact& f = SCIENCE_FACTS[scienceFact];
    SciLayout L = sciLayout(g, scienceFact);
    const float waited = wallTime - scienceOpenedAt;
    const bool awake = waited >= SCIENCE_LOCK;
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(4, 10, 8, 185));
    // a blackboard in a wooden frame
    g.fill(L.x0 - 10, L.y0 - 10, L.w + 20, L.h + 20, FRAME);
    g.fill(L.x0 - 10, L.y0 - 10, L.w + 20, 2, shade(FRAME, 1.3f));
    g.fill(L.x0 - 10, L.y0 + L.h + 8, L.w + 20, 2, shade(FRAME, 0.6f));
    g.fill(L.x0, L.y0, L.w, L.h, BOARD);
    char buf[64];
    g.text(L.x0 + L.pad, L.y0 + 22, "SCIENCE BREAK", CHALK_DIM, 1, false);
    snprintf(buf, sizeof buf, "%d of %d", std::min(g_shown, SCIENCE_FACT_COUNT), SCIENCE_FACT_COUNT);
    g.text(L.x0 + L.w - L.pad - g.textW(buf), L.y0 + 22, buf, CHALK_DIM, 1, false);
    g.text(L.x0 + L.pad, L.y0 + 46, f.title, CHALK_YELLOW, 3, false);
    int ty = L.y0 + 46 + 24 + 22;
    for (const std::string& line : L.lines) { g.text(L.x0 + L.pad, ty, line.c_str(), CHALK, 2, false); ty += 24; }
    // the promise: asleep (filling as the seconds pass), then awake in chalk yellow
    const bool hover = mouseX >= L.bx && mouseX < L.bx + L.bw && mouseY >= L.by && mouseY < L.by + L.bh;
    if (awake) {
        g.fill(L.bx, L.by, L.bw, L.bh, hover ? rgb(255, 232, 140) : CHALK_YELLOW);
        g.fill(L.bx, L.by + L.bh - 3, L.bw, 3, rgb(196, 160, 60));
        g.text(L.bx + 18, L.by + 15, PROMISE, BOARD_DEEP, 2, false);
    } else {
        g.fill(L.bx, L.by, L.bw, L.bh, BOARD_DEEP);
        g.fill(L.bx, L.by, (int)(L.bw * waited / SCIENCE_LOCK), L.bh, rgb(255, 255, 255, 16));
        g.text(L.bx + 18, L.by + 15, PROMISE, CHALK_DIM, 2, false);
    }
    // a tally of the clicks: four strokes and a fifth across them
    const int tx = L.bx + L.bw + 34, top = L.by + 4, bot = L.by + L.bh - 4;
    for (int i = 0; i < SCIENCE_CLICKS; i++) {
        Color c = i < scienceClicks ? CHALK : rgb(255, 255, 255, 34);
        if (i < 4) g.thickLine((float)tx + i * 13, (float)top, (float)tx + i * 13 + 2, (float)bot, c, 3.0f);
        else g.thickLine((float)tx - 8, (float)bot - 6, (float)tx + 3 * 13 + 10, (float)top + 6, c, 3.0f);
    }
    if (!awake) {
        snprintf(buf, sizeof buf, "%d s", (int)std::ceil(SCIENCE_LOCK - waited));
        g.text(tx + 4 * 13 + 26, L.by + 15, buf, CHALK_DIM, 2, false);
    }
}
