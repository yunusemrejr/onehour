#include "ui.h"

Game g_game;

enum { BK_BUILD = 1, BK_TRAIN, BK_SELL, BK_RALLY, BK_POWER, BK_ATTACKMOVE, BK_STOP, BK_CANCEL };

static const int MINIMAP_X = 8, MINIMAP_Y = SCREEN_H - HUD_H + 8, MINIMAP_SIZE = 116;
static const int GRID_X = 676, GRID_Y = SCREEN_H - HUD_H + 8, BTN_W = 110, BTN_H = 37, BTN_GAP = 4;
static const int INFO_X = 134, INFO_W = 534;

static Color hudBase(Faction f) { return f == F_CYBER ? rgb(36, 44, 58) : rgb(62, 60, 46); }
static Color hudAccent(Faction f) { return f == F_CYBER ? rgb(110, 230, 255) : rgb(222, 178, 60); }
static Color hudText() { return rgb(222, 226, 230); }
static Color hudDim() { return rgb(150, 156, 164); }

static const char* DIFF_NAME[4] = { "Easy", "Normal", "Hard", "Brutal" };

void Game::addMessage(const char* text, Color c) {
    messages.push_back({text, wallTime, c});
    if (messages.size() > 6) messages.erase(messages.begin());
}

// ------------------------------------------------------------ game setup
void Game::startGame() {
    Faction fac[MAX_PLAYERS]; bool ai[MAX_PLAYERS]; int diff[MAX_PLAYERS]; int team[MAX_PLAYERS];
    int n = 1 + menu.enemies;
    Rng r(seed);
    fac[0] = menu.playerFaction; ai[0] = false; diff[0] = 1; team[0] = 0;
    for (int i = 0; i < menu.enemies; i++) {
        int f = menu.enemyFaction[i];
        fac[i + 1] = f == 2 ? (Faction)(r.range(0, 1)) : (Faction)f;
        ai[i + 1] = true; diff[i + 1] = menu.difficulty; team[i + 1] = menu.enemiesAllied ? 1 : 1 + i;
    }
    g_sim.init(n, fac, ai, diff, team, seed);
    g_ai.init(seed);
    selection.clear();
    for (auto& g : groups) g.clear();
    messages.clear();
    placingType = -1; attackMoveMode = false; powerMode = false; rallyMode = false; paused = false; speed = 1.0f;
    accumulator = 0;
    Vec2 b = g_sim.players[0].basePos;
    cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
    state = GS_PLAYING;
    addMessage("Skirmish started. Build a Supply Hub and power first.", hudText());
}

// ------------------------------------------------------------ update
void Game::update(float dt) {
    wallTime += dt;
    if (state != GS_PLAYING) return;
    scroll(dt);
    if (!paused && !g_sim.gameOver) {
        accumulator += dt * speed;
        int steps = 0;
        while (accumulator >= SIM_DT && steps < 8) {
            g_sim.step();
            g_ai.update();
            accumulator -= SIM_DT;
            steps++;
        }
        if (steps == 8) accumulator = 0;   // can't keep up: drop time rather than spiral
    }
    renderAlpha = paused ? 1.0f : clampf(accumulator / SIM_DT, 0, 1);
    processEvents();
    cleanSelection();
    g_audio.setListener(cam, SCREEN_W, VIEW_H);
    if (g_sim.gameOver && state == GS_PLAYING) {
        state = GS_GAMEOVER; gameOverAt = wallTime;
        bool won = g_sim.winnerTeam == g_sim.players[0].team;
        g_audio.play(won ? SND_VICTORY : SND_DEFEAT, Vec2(), true);
    }
}

void Game::processEvents() {
    for (auto& ev : g_sim.events) {
        bool mine = ev.player == g_sim.humanPlayer;
        switch (ev.type) {
        case EV_SOUND: g_audio.play(ev.sound, ev.pos); break;
        case EV_BUILD_DONE: if (mine) { g_audio.play(SND_BUILD_DONE, Vec2(), true); addMessage((ev.msg + " complete").c_str(), hudText()); } break;
        case EV_UNIT_READY: if (mine) { g_audio.play(SND_UNIT_READY, Vec2(), true, 0.7f); } break;
        case EV_UNDER_ATTACK: if (mine) { g_audio.play(SND_ATTACKED, Vec2(), true); addMessage(ev.msg.c_str(), rgb(255, 120, 100)); pings.push_back({ev.pos, wallTime}); if (pings.size() > 6) pings.erase(pings.begin()); } break;
        case EV_NOFUNDS: if (mine) { g_audio.play(SND_NOFUNDS, Vec2(), true); addMessage("Insufficient funds", rgb(255, 170, 90)); } break;
        case EV_LOWPOWER: if (mine) { g_audio.play(SND_LOWPOWER, Vec2(), true); addMessage("Low power: build another power plant", rgb(255, 170, 90)); } break;
        case EV_SUPPLY_EMPTY: if (mine) addMessage("Supply pile depleted", hudDim()); break;
        case EV_PLAYER_DEAD: {
            char buf[96]; snprintf(buf, sizeof buf, "%s (%s) has been eliminated", ev.player == 0 ? "You" : "Enemy", ev.msg.c_str());
            addMessage(buf, ev.player == 0 ? rgb(255, 120, 100) : rgb(150, 240, 150));
            break;
        }
        default: break;
        }
    }
    g_sim.events.clear();
}

void Game::cleanSelection() {
    selection.erase(std::remove_if(selection.begin(), selection.end(), [&](Ref r) { Entity* e = g_sim.get(r); return !e || e->owner != g_sim.humanPlayer; }), selection.end());
    for (auto& g : groups) g.erase(std::remove_if(g.begin(), g.end(), [&](Ref r) { return !g_sim.get(r); }), g.end());
}

void Game::scroll(float dt) {
    const u8* k = SDL_GetKeyboardState(nullptr);
    float sp = 720 * dt;
    Vec2 d;
    if (k[SDL_SCANCODE_LEFT]) d.x -= 1;
    if (k[SDL_SCANCODE_RIGHT]) d.x += 1;
    if (k[SDL_SCANCODE_UP]) d.y -= 1;
    if (k[SDL_SCANCODE_DOWN]) d.y += 1;
    if (mouseInWindow && !midDrag && !dragging) {
        const int edge = 6;
        if (mouseX <= edge) d.x -= 1;
        if (mouseX >= SCREEN_W - 1 - edge) d.x += 1;
        if (mouseY <= edge) d.y -= 1;
        if (mouseY >= SCREEN_H - 1 - edge && mouseX >= INFO_X && mouseX < GRID_X) d.y += 1;
    }
    cam += d * sp;
    cam.x = clampf(cam.x, 0, WORLD_W - SCREEN_W);
    cam.y = clampf(cam.y, 0, WORLD_H - VIEW_H);
}

// ------------------------------------------------------------ selection helpers
Entity* Game::pickEntity(Vec2 w, bool ownOnly) {
    Entity* best = nullptr; float bd = 1e9f;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (ownOnly && e.owner != g_sim.humanPlayer) continue;
        if (e.owner != g_sim.humanPlayer && e.kind != EK_RESOURCE && !g_sim.explored(g_sim.humanPlayer, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
        float d;
        if (e.isBuilding()) d = g_sim.distToEntity(w, e) > 0 ? 1e9f : dist(w, e.pos) * 0.1f;
        else if (e.kind == EK_RESOURCE) d = dist(w, e.pos) <= 20 ? dist(w, e.pos) : 1e9f;
        else d = dist(w, e.pos) <= e.radius() + 6 ? dist(w, e.pos) : 1e9f;
        if (e.isAir()) d *= 0.5f;
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

void Game::selectSingle(Entity* e, bool add) {
    if (!add) selection.clear();
    if (!e || e->owner != g_sim.humanPlayer) return;
    Ref r = g_sim.refOf(*e);
    auto it = std::find(selection.begin(), selection.end(), r);
    if (it != selection.end()) { if (add) selection.erase(it); return; }
    if (e->isBuilding()) { selection.clear(); selection.push_back(r); }
    else {
        selection.erase(std::remove_if(selection.begin(), selection.end(), [&](Ref x) { Entity* o = g_sim.get(x); return o && o->isBuilding(); }), selection.end());
        selection.push_back(r);
    }
    g_audio.play(SND_SELECT, Vec2(), true, 0.6f);
}

void Game::selectBox(Vec2 a, Vec2 b, bool add) {
    if (!add) selection.clear();
    float x0 = std::min(a.x, b.x), x1 = std::max(a.x, b.x), y0 = std::min(a.y, b.y), y1 = std::max(a.y, b.y);
    bool any = false;
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isUnit() || e.owner != g_sim.humanPlayer) continue;
        if (e.pos.x < x0 || e.pos.x > x1 || e.pos.y < y0 || e.pos.y > y1) continue;
        Ref r = g_sim.refOf(e);
        if (std::find(selection.begin(), selection.end(), r) == selection.end()) { selection.push_back(r); any = true; }
    }
    // box selection drops buildings and, if combat units are present, non-combat units
    bool combat = false;
    for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->ut().role == UR_COMBAT) combat = true; }
    if (combat) selection.erase(std::remove_if(selection.begin(), selection.end(), [&](Ref r) { Entity* e = g_sim.get(r); return e && e->isUnit() && e->ut().role != UR_COMBAT; }), selection.end());
    if (any) g_audio.play(SND_SELECT, Vec2(), true, 0.6f);
}

void Game::selectSameType(Entity* e) {
    if (!e || !e->isUnit() || e->owner != g_sim.humanPlayer) return;
    selection.clear();
    for (auto& o : g_sim.ents) {
        if (!o.alive || !o.isUnit() || o.owner != e->owner || o.type != e->type) continue;
        Vec2 s = worldToScreen(o.pos);
        if (s.x < -20 || s.y < -20 || s.x > SCREEN_W + 20 || s.y > VIEW_H + 20) continue;
        selection.push_back(g_sim.refOf(o));
    }
}

bool Game::selectionHasRole(UnitRole r) const {
    for (auto ref : selection) { const Entity* e = g_sim.get(ref); if (e && e->isUnit() && e->ut().role == r) return true; }
    return false;
}
Entity* Game::selectedBuilding() const {
    if (selection.size() != 1) return nullptr;
    Entity* e = const_cast<Entity*>(g_sim.get(selection[0]));
    return (e && e->isBuilding()) ? e : nullptr;
}
int Game::selectionOwner() const { return g_sim.humanPlayer; }

void Game::issueRightClick(Vec2 w) {
    if (selection.empty()) return;
    Entity* t = pickEntity(w, false);
    if (t && t->owner != g_sim.humanPlayer && t->kind != EK_RESOURCE) {
        if (g_sim.enemies(g_sim.humanPlayer, t->owner)) { g_sim.cmdAttack(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); }
        return;
    }
    if (t && t->kind == EK_RESOURCE && selectionHasRole(UR_HARVESTER)) { g_sim.cmdHarvest(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
    if (t && t->isBuilding() && t->owner == g_sim.humanPlayer && selectionHasRole(UR_DOZER)) { g_sim.cmdAssist(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
    Entity* b = selectedBuilding();
    if (b) { g_sim.cmdSetRally(g_sim.refOf(*b), w); g_audio.play(SND_CLICK, Vec2(), true); return; }
    g_sim.cmdMove(selection, w, false);
    g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
}

void Game::issueAttackMove(Vec2 w) {
    Entity* t = pickEntity(w, false);
    if (t && g_sim.enemies(g_sim.humanPlayer, t->owner)) g_sim.cmdAttack(selection, g_sim.refOf(*t));
    else g_sim.cmdMove(selection, w, true);
    g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
}

// ------------------------------------------------------------ command buttons
void Game::buildButtons() {
    buttons.clear();
    Player& pl = g_sim.players[g_sim.humanPlayer];
    auto add = [&](int kind, int id, bool enabled, const char* label, const std::string& tip) {
        int i = (int)buttons.size();
        if (i >= 9) return;
        Button b; b.x = GRID_X + (i % 3) * (BTN_W + BTN_GAP); b.y = GRID_Y + (i / 3) * (BTN_H + BTN_GAP); b.w = BTN_W; b.h = BTN_H;
        b.kind = kind; b.id = id; b.enabled = enabled; b.label = label; b.tip = tip;
        buttons.push_back(b);
    };
    if (placingType >= 0) { add(BK_CANCEL, 0, true, "Cancel", "Cancel placement (Esc)"); return; }
    if (selection.empty()) return;
    Entity* b = selectedBuilding();
    char tip[160];
    if (b) {
        if (!b->constructed) { add(BK_SELL, 0, true, "Sell", "Sell this structure for a partial refund"); return; }
        BuildRole role = b->bt().role;
        for (int u = 0; u < U_COUNT; u++) {
            const UnitType& ut = UNITS[u];
            if (ut.faction != pl.faction || ut.builtBy != role) continue;
            bool ok = g_sim.unitAvailable(g_sim.humanPlayer, u);
            snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s%s%s", ut.name, ut.cost, ut.hotkey, ut.desc, ok ? "" : "  Requires ", ok ? "" : BUILDS[ut.requires_].name);
            add(BK_TRAIN, u, ok, ut.name, tip);
        }
        if (role == BR_HQ) {
            bool tech = g_sim.hasRole(g_sim.humanPlayer, BR_TECH);
            const PowerType& pw = POWERS[pl.faction];
            snprintf(tip, sizeof tip, "%s  [X]  %s%s", pw.name, pw.desc, tech ? "" : "  Requires tech structure");
            add(BK_POWER, 0, tech && g_sim.time >= pl.powerReady, pw.name, tip);
        }
        if (role == BR_BARRACKS || role == BR_FACTORY || role == BR_AIRFIELD || role == BR_SUPPLY) add(BK_RALLY, 0, true, "Rally", "Set rally point (right-click ground while selected)");
        add(BK_SELL, 0, true, "Sell", "Sell this structure for 50% of its cost");
        return;
    }
    if (selectionHasRole(UR_DOZER)) {
        for (int t = 0; t < B_COUNT; t++) {
            const BuildType& bt = BUILDS[t];
            if (bt.faction != pl.faction || bt.role == BR_HQ) continue;
            bool ok = g_sim.buildAvailable(g_sim.humanPlayer, t);
            snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s%s%s", bt.name, bt.cost, bt.hotkey, bt.desc, ok ? "" : "  Requires ", ok ? "" : BUILDS[bt.requires_].name);
            add(BK_BUILD, t, ok, bt.name, tip);
        }
        return;
    }
    bool combat = false;
    for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->weapon() >= 0) combat = true; }
    if (combat) add(BK_ATTACKMOVE, 0, true, "Attack Move", "Attack-move: engage everything on the way  [A]");
    add(BK_STOP, 0, true, "Stop", "Stop and hold position  [S]");
}

void Game::clickButton(const Button& b) {
    if (!b.enabled) { g_audio.play(SND_CANT, Vec2(), true); return; }
    cmdSelection(b.kind, b.id);
}

void Game::cmdSelection(int kind, int id) {
    Player& pl = g_sim.players[g_sim.humanPlayer];
    switch (kind) {
    case BK_BUILD:
        if (!g_sim.canAfford(g_sim.humanPlayer, BUILDS[id].cost)) { g_audio.play(SND_NOFUNDS, Vec2(), true); addMessage("Insufficient funds", rgb(255, 170, 90)); return; }
        placingType = id; attackMoveMode = false; powerMode = false; g_audio.play(SND_CLICK, Vec2(), true);
        break;
    case BK_TRAIN: {
        Entity* b = selectedBuilding();
        if (b && g_sim.cmdTrain(g_sim.refOf(*b), id)) g_audio.play(SND_CLICK, Vec2(), true);
        break;
    }
    case BK_SELL: { Entity* b = selectedBuilding(); if (b) { g_sim.cmdSell(g_sim.refOf(*b)); selection.clear(); } break; }
    case BK_RALLY: rallyMode = true; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_POWER: if (g_sim.time >= pl.powerReady && g_sim.hasRole(g_sim.humanPlayer, BR_TECH)) { powerMode = true; placingType = -1; attackMoveMode = false; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_ATTACKMOVE: attackMoveMode = true; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_STOP: g_sim.cmdStop(selection); g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_CANCEL: placingType = -1; break;
    }
}

void Game::hotkey(SDL_Keycode k, u16 mod) {
    if (k >= SDLK_0 && k <= SDLK_9) {
        int g = k - SDLK_0;
        if (mod & KMOD_CTRL) { groups[g] = selection; addMessage("Group assigned", hudDim()); }
        else if (!groups[g].empty()) {
            selection = groups[g]; cleanSelection();
            if (mod & KMOD_ALT && !selection.empty()) { Entity* e = g_sim.get(selection[0]); if (e) { cam = Vec2(clampf(e->pos.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(e->pos.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); } }
        }
        return;
    }
    // command hotkeys matched against the current button set
    buildButtons();
    char c = (char)toupper((int)k);
    for (auto& b : buttons) {
        const char* hk = b.kind == BK_BUILD ? BUILDS[b.id].hotkey : (b.kind == BK_TRAIN ? UNITS[b.id].hotkey : (b.kind == BK_POWER ? "X" : (b.kind == BK_ATTACKMOVE ? "A" : (b.kind == BK_STOP ? "S" : nullptr))));
        if (hk && hk[0] == c) { clickButton(b); return; }
    }
    if (c == 'S') { g_sim.cmdStop(selection); return; }
    if (c == 'A' && !selection.empty()) { attackMoveMode = true; return; }
}

// ------------------------------------------------------------ events
void Game::handleEvent(const SDL_Event& e) {
    if (e.type == SDL_MOUSEMOTION) { mouseX = e.motion.x; mouseY = e.motion.y; }
    if (e.type == SDL_WINDOWEVENT) { if (e.window.event == SDL_WINDOWEVENT_LEAVE) mouseInWindow = false; if (e.window.event == SDL_WINDOWEVENT_ENTER) mouseInWindow = true; }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F12) { screenshot("onehour_screenshot.bmp"); return; }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F2) { g_audio.setMuted(!g_audio.isMuted()); }
    if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_F11) {
        u32 fl = SDL_GetWindowFlags(g_gfx.win);
        SDL_SetWindowFullscreen(g_gfx.win, (fl & SDL_WINDOW_FULLSCREEN_DESKTOP) ? 0 : SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
    switch (state) {
    case GS_MENU: menuEvent(e); break;
    case GS_PLAYING: gameEvent(e); break;
    case GS_GAMEOVER:
        if (e.type == SDL_KEYDOWN && (e.key.keysym.sym == SDLK_RETURN || e.key.keysym.sym == SDLK_ESCAPE)) state = GS_MENU;
        else if (e.type == SDL_MOUSEBUTTONDOWN && wallTime - gameOverAt > 1.0f) state = GS_MENU;
        break;
    }
}

void Game::menuEvent(const SDL_Event& e) {
    const int ROWS = 8;   // faction, enemies, e1, e2, e3, difficulty, teams, start
    auto change = [&](int row, int dir) {
        switch (row) {
        case 0: menu.playerFaction = (Faction)((menu.playerFaction + 1) % 2); break;
        case 1: menu.enemies = clampi(menu.enemies + dir, 1, 3); break;
        case 2: case 3: case 4: menu.enemyFaction[row - 2] = (menu.enemyFaction[row - 2] + dir + 3) % 3; break;
        case 5: menu.difficulty = (menu.difficulty + dir + 4) % 4; break;
        case 6: menu.enemiesAllied = !menu.enemiesAllied; break;
        case 7: seed = (u64)SDL_GetTicks() * 2654435761ull + 17; startGame(); break;
        }
        g_audio.play(SND_CLICK, Vec2(), true);
    };
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_UP) menu.cursor = (menu.cursor + ROWS - 1) % ROWS;
        else if (k == SDLK_DOWN) menu.cursor = (menu.cursor + 1) % ROWS;
        else if (k == SDLK_LEFT) change(menu.cursor, -1);
        else if (k == SDLK_RIGHT || k == SDLK_SPACE) change(menu.cursor, 1);
        else if (k == SDLK_RETURN) { if (menu.cursor == 7) change(7, 1); else change(menu.cursor, 1); }
        else if (k == SDLK_ESCAPE) quitRequested = true;
        // skip disabled enemy rows
        while (menu.cursor >= 2 && menu.cursor <= 4 && menu.cursor - 2 >= menu.enemies) menu.cursor = (k == SDLK_UP) ? menu.cursor - 1 : menu.cursor + 1;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int x = e.button.x, y = e.button.y;
        int rowY0 = 250, rowH = 34;
        int row = (y - rowY0) / rowH;
        if (y >= rowY0 && row >= 0 && row < 7) {
            if (row >= 2 && row <= 4 && row - 2 >= menu.enemies) return;
            menu.cursor = row;
            if (x >= 560 && x < 600) change(row, -1);
            else if (x >= 800 && x < 840) change(row, 1);
            else if (x >= 600 && x < 800) change(row, 1);
        }
        if (y >= 520 && y < 560 && x >= 412 && x < 612) { menu.cursor = 7; change(7, 1); }
        if (y >= 572 && y < 604 && x >= 462 && x < 562) quitRequested = true;
    }
}

void Game::gameEvent(const SDL_Event& e) {
    Vec2 w = screenToWorld(mouseX, mouseY);
    bool overHud = mouseY >= VIEW_H;
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        u16 mod = e.key.keysym.mod;
        if (k == SDLK_ESCAPE) {
            if (placingType >= 0 || attackMoveMode || powerMode || rallyMode) { placingType = -1; attackMoveMode = false; powerMode = false; rallyMode = false; }
            else if (!selection.empty()) selection.clear();
            else if (showHelp) showHelp = false;
            else if (wallTime < escArmedUntil) { state = GS_MENU; }
            else { escArmedUntil = wallTime + 2.5f; addMessage("Press Esc again to leave the game", rgb(255, 200, 120)); }
            return;
        }
        if (k == SDLK_SPACE || k == SDLK_PAUSE) { paused = !paused; return; }
        if (k == SDLK_F1) { showHelp = !showHelp; return; }
        if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { speed = std::min(3.0f, speed + 0.5f); return; }
        if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { speed = std::max(0.5f, speed - 0.5f); return; }
        if (k == SDLK_BACKSPACE || k == SDLK_HOME) { Vec2 b = g_sim.players[0].basePos; cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H)); return; }
        if (k == SDLK_TAB) {
            // select all combat units on screen
            selection.clear();
            for (auto& o : g_sim.ents) {
                if (!o.alive || !o.isUnit() || o.owner != g_sim.humanPlayer || o.ut().role != UR_COMBAT) continue;
                Vec2 s = worldToScreen(o.pos);
                if (s.x < 0 || s.y < 0 || s.x > SCREEN_W || s.y > VIEW_H) continue;
                selection.push_back(g_sim.refOf(o));
            }
            return;
        }
        if (k == SDLK_DELETE) { Entity* b = selectedBuilding(); if (b) { g_sim.cmdSell(g_sim.refOf(*b)); selection.clear(); } return; }
        if ((k >= SDLK_a && k <= SDLK_z) || (k >= SDLK_0 && k <= SDLK_9)) { hotkey(k, mod); return; }
        return;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        int btn = e.button.button;
        if (btn == SDL_BUTTON_MIDDLE) { midDrag = true; midStart = Vec2(mouseX, mouseY); return; }
        if (overHud) {
            bool onMini = mouseX >= MINIMAP_X && mouseX < MINIMAP_X + MINIMAP_SIZE && mouseY >= MINIMAP_Y && mouseY < MINIMAP_Y + MINIMAP_SIZE;
            if (btn == SDL_BUTTON_RIGHT && onMini && !selection.empty()) {
                Vec2 wp((mouseX - MINIMAP_X) * (float)WORLD_W / MINIMAP_SIZE, (mouseY - MINIMAP_Y) * (float)WORLD_H / MINIMAP_SIZE);
                if (selectedBuilding()) g_sim.cmdSetRally(g_sim.refOf(*selectedBuilding()), wp); else g_sim.cmdMove(selection, wp, false);
                g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
                return;
            }
            if (btn != SDL_BUTTON_LEFT) return;
            // minimap
            if (onMini) {
                Vec2 wp((mouseX - MINIMAP_X) * (float)WORLD_W / MINIMAP_SIZE, (mouseY - MINIMAP_Y) * (float)WORLD_H / MINIMAP_SIZE);
                if (attackMoveMode && !selection.empty()) { g_sim.cmdMove(selection, wp, true); attackMoveMode = false; }
                else if (powerMode) { if (g_sim.cmdPower(g_sim.humanPlayer, wp)) powerMode = false; }
                else cam = Vec2(clampf(wp.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(wp.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
                return;
            }
            buildButtons();
            for (auto& b : buttons) if (mouseX >= b.x && mouseX < b.x + b.w && mouseY >= b.y && mouseY < b.y + b.h) { clickButton(b); return; }
            // queue cancel (click on queue slots in the info panel)
            Entity* b = selectedBuilding();
            if (b && !b->queue.empty()) {
                int qx = INFO_X + 330, qy = SCREEN_H - HUD_H + 70;
                for (int i = 0; i < (int)b->queue.size() && i < 9; i++) {
                    int sx = qx + i * 30;
                    if (mouseX >= sx && mouseX < sx + 28 && mouseY >= qy && mouseY < qy + 28) { g_sim.cmdCancelTrain(g_sim.refOf(*b), i); g_audio.play(SND_CLICK, Vec2(), true); return; }
                }
            }
            return;
        }
        if (btn == SDL_BUTTON_LEFT) {
            if (placingType >= 0) {
                int tx = tileOf(w.x) - BUILDS[placingType].w / 2, ty = tileOf(w.y) - BUILDS[placingType].h / 2;
                Ref dozer;
                for (auto r : selection) { Entity* d = g_sim.get(r); if (d && d->isUnit() && d->ut().role == UR_DOZER) { dozer = r; break; } }
                if (dozer.valid() && g_sim.cmdBuild(dozer, placingType, tx, ty)) { if (!(SDL_GetModState() & KMOD_SHIFT)) placingType = -1; }
                else g_audio.play(SND_CANT, Vec2(), true);
                return;
            }
            if (powerMode) { if (g_sim.cmdPower(g_sim.humanPlayer, w)) powerMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (attackMoveMode) { issueAttackMove(w); attackMoveMode = false; return; }
            if (rallyMode) { Entity* b = selectedBuilding(); if (b) g_sim.cmdSetRally(g_sim.refOf(*b), w); rallyMode = false; return; }
            dragging = true; dragStart = w; dragNow = w;
            return;
        }
        if (btn == SDL_BUTTON_RIGHT) {
            if (placingType >= 0 || attackMoveMode || powerMode || rallyMode) { placingType = -1; attackMoveMode = false; powerMode = false; rallyMode = false; return; }
            issueRightClick(w);
            return;
        }
    }
    if (e.type == SDL_MOUSEMOTION) {
        if (midDrag) { cam -= Vec2(e.motion.xrel, e.motion.yrel); cam.x = clampf(cam.x, 0, WORLD_W - SCREEN_W); cam.y = clampf(cam.y, 0, WORLD_H - VIEW_H); }
        if (dragging) dragNow = screenToWorld(mouseX, mouseY);
    }
    if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_MIDDLE) midDrag = false;
        if (e.button.button == SDL_BUTTON_LEFT && dragging) {
            dragging = false;
            bool shift = SDL_GetModState() & KMOD_SHIFT;
            if (dist(dragStart, dragNow) > 6) selectBox(dragStart, dragNow, shift);
            else {
                Entity* t = pickEntity(dragStart, false);
                Ref tr = t ? g_sim.refOf(*t) : NOREF;
                if (t && t->owner == g_sim.humanPlayer && wallTime - lastClickTime < 0.35f && tr == lastClickEnt) selectSameType(t);
                else selectSingle(t, shift);
                lastClickTime = wallTime; lastClickEnt = tr;
            }
        }
    }
    if (e.type == SDL_MOUSEWHEEL) {
        // wheel scrolls vertically, shift+wheel horizontally
        if (SDL_GetModState() & KMOD_SHIFT) cam.x -= e.wheel.y * 64; else cam.y -= e.wheel.y * 64;
        cam.x = clampf(cam.x, 0, WORLD_W - SCREEN_W); cam.y = clampf(cam.y, 0, WORLD_H - VIEW_H);
    }
}

// ------------------------------------------------------------ rendering
void Game::render() {
    switch (state) {
    case GS_MENU: renderMenu(); break;
    case GS_PLAYING: renderWorld(); renderHud(); break;
    case GS_GAMEOVER: renderWorld(); renderHud(); renderGameOver(); break;
    }
}

void Game::renderMenu() {
    Gfx& g = g_gfx;
    g.beginFrame(rgb(18, 22, 28));
    // backdrop: a slice of the map with a shroud vignette
    for (int ty = 0; ty < SCREEN_H / TILE + 1; ty++) for (int tx = 0; tx < SCREEN_W / TILE + 1; tx++) {
        int mx = tx + 12, my = ty + 20;
        if (!inMap(mx, my)) continue;
        u8 t = g_map.tile(mx, my);
        g.drawRect(g.tiles[t][g_map.variant[my * MAP_W + mx] % 4], tx * TILE, ty * TILE, TILE, TILE, 255);
    }
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(10, 14, 20, 170));
    Color accent = hudAccent(menu.playerFaction);
    g.fill(0, 110, SCREEN_W, 4, accent);
    g.text(SCREEN_W / 2 - g.textW("ONE HOUR", 6) / 2, 40, "ONE HOUR", rgb(240, 244, 248), 6);
    g.text(SCREEN_W / 2 - g.textW("Skirmish: Cyber Army vs Clanker Army", 2) / 2, 130, "Skirmish: Cyber Army vs Clanker Army", hudDim(), 2);
    const char* fdesc = menu.playerFaction == F_CYBER
        ? "Lasers, railguns, volt coils, Patriot batteries, EMP Strike. Fast electric armor."
        : "Diesel tanks, gatling guns, rocket artillery, gun nests, Shell Storm. Heavy and cheap.";
    g.text(SCREEN_W / 2 - g.textW(fdesc) / 2, 170, fdesc, hudText());
    g.text(SCREEN_W / 2 - g.textW("One map: Lakeside, 4 corner bases, contested supply piles in the middle") / 2, 190, "One map: Lakeside, 4 corner bases, contested supply piles in the middle", hudDim());

    int rowY0 = 250, rowH = 34;
    auto row = [&](int i, const char* label, const char* value, bool enabled) {
        int y = rowY0 + i * rowH;
        bool cur = menu.cursor == i;
        if (cur) g.fill(300, y - 4, 560, rowH - 4, rgb(255, 255, 255, 22));
        g.text(320, y + 4, label, enabled ? hudText() : hudDim(), 2);
        if (!enabled) { g.text(620, y + 4, "-", hudDim(), 2); return; }
        g.text(570, y + 4, "<", cur ? accent : hudDim(), 2);
        g.text(700 - g.textW(value, 2) / 2, y + 4, value, cur ? rgb(255, 255, 255) : hudText(), 2);
        g.text(810, y + 4, ">", cur ? accent : hudDim(), 2);
    };
    static const char* FN[3] = { "Cyber Army", "Clanker Army", "Random" };
    char buf[32];
    row(0, "Your army", FACTION_NAME[menu.playerFaction], true);
    snprintf(buf, sizeof buf, "%d", menu.enemies); row(1, "Enemies", buf, true);
    for (int i = 0; i < 3; i++) { snprintf(buf, sizeof buf, "Enemy %d", i + 1); row(2 + i, buf, FN[menu.enemyFaction[i]], i < menu.enemies); }
    row(5, "Difficulty", DIFF_NAME[menu.difficulty], true);
    row(6, "Enemy teams", menu.enemiesAllied ? "Allied vs you" : "Free for all", true);
    // start / quit
    bool cur = menu.cursor == 7;
    g.bevelPanel(412, 520, 200, 40, cur ? shade(accent, 0.55f) : rgb(50, 58, 70));
    g.text(512 - g.textW("START", 3) / 2, 528, "START", rgb(255, 255, 255), 3);
    g.bevelPanel(462, 572, 100, 32, rgb(50, 58, 70));
    g.text(512 - g.textW("Quit", 2) / 2, 580, "Quit", hudText(), 2);
    g.text(SCREEN_W / 2 - g.textW("Arrows to change, Enter to start, Esc to quit") / 2, 616, "Arrows to change, Enter to start, Esc to quit", hudDim());
}

void Game::drawShroud() {
    Gfx& g = g_gfx;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    int tx0 = std::max(0, tileOf(cam.x)), ty0 = std::max(0, tileOf(cam.y));
    int tx1 = std::min(MAP_W - 1, tileOf(cam.x + SCREEN_W)), ty1 = std::min(MAP_H - 1, tileOf(cam.y + VIEW_H));
    SDL_SetRenderDrawColor(g.ren, 4, 6, 10, 255);
    for (int ty = ty0; ty <= ty1; ty++) {
        int runStart = -1;
        for (int tx = tx0; tx <= tx1 + 1; tx++) {
            bool dark = tx <= tx1 && !ex[ty * MAP_W + tx];
            if (dark && runStart < 0) runStart = tx;
            if (!dark && runStart >= 0) {
                SDL_Rect r = { (int)(runStart * TILE - cam.x), (int)(ty * TILE - cam.y), (tx - runStart) * TILE, TILE };
                SDL_RenderFillRect(g.ren, &r);
                runStart = -1;
            }
        }
    }
    // soft edge: darken explored tiles adjacent to shroud
    SDL_SetRenderDrawColor(g.ren, 4, 6, 10, 110);
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!ex[ty * MAP_W + tx]) continue;
        bool edge = false;
        for (int d = 0; d < 4 && !edge; d++) { int nx = tx + (d == 0) - (d == 1), ny = ty + (d == 2) - (d == 3); if (inMap(nx, ny) && !ex[ny * MAP_W + nx]) edge = true; }
        if (edge) { SDL_Rect r = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), TILE, TILE }; SDL_RenderFillRect(g.ren, &r); }
    }
}

static void hpBar(Gfx& g, float x, float y, float w, float frac, bool big = false) {
    int h = big ? 4 : 3;
    g.fill((int)x, (int)y, (int)w, h, rgb(0, 0, 0, 200));
    Color c = frac > 0.6f ? rgb(80, 220, 80) : (frac > 0.3f ? rgb(240, 200, 60) : rgb(230, 70, 60));
    g.fill((int)x + 1, (int)y + 1, (int)((w - 2) * clampf(frac, 0, 1)), h - 2, c);
}

void Game::drawEntity(Entity& e) {
    Gfx& g = g_gfx;
    Vec2 p = worldToScreen(entPos(e));
    bool selected = std::find(selection.begin(), selection.end(), g_sim.refOf(e)) != selection.end();
    int owner = e.owner < 0 ? 0 : e.owner;
    bool disabled = e.disabledUntil > g_sim.time;
    if (e.kind == EK_RESOURCE) {
        int v = e.amount > 20000 ? 0 : (e.amount > 8000 ? 1 : 2);
        g.draw(g.pile[v], p.x, p.y);
        return;
    }
    if (e.isBuilding()) {
        const BuildType& bt = e.bt();
        const Sprite& s = g.building[e.type][owner];
        g.fill((int)(p.x - s.ox) + 5, (int)(p.y - s.oy) + 6, s.w, s.h, rgb(0, 0, 0, 70));   // drop shadow
        if (!e.constructed) {
            g.draw(g.site[e.type], p.x, p.y);
            // finished structure rises from the bottom as construction progresses
            int rows = (int)(s.h * clampf(e.progress, 0, 1));
            if (rows > 0) {
                SDL_Rect src = { 0, s.h - rows, s.w, rows };
                SDL_Rect dst = { (int)(p.x - s.ox), (int)(p.y - s.oy) + s.h - rows, s.w, rows };
                SDL_SetTextureColorMod(s.tex, 255, 255, 255); SDL_SetTextureAlphaMod(s.tex, 255);
                SDL_RenderCopy(g.ren, s.tex, &src, &dst);
            }
        } else {
            g.draw(s, p.x, p.y, 0, 1, disabled ? rgb(120, 140, 170) : rgb(255, 255, 255));
            // rotating heads for defenses
            if (bt.role == BR_TURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 0 : 1], p.x, p.y, e.angle);
            else if (bt.role == BR_AATURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 2 : 3], p.x, p.y, e.angle);
            // production activity light
            if (!e.queue.empty() && ((int)(wallTime * 3) & 1)) g.fillCircle(p.x + s.w * 0.5f - 6, p.y - s.h * 0.5f + 6, 2.5f, hudAccent(bt.faction));
        }
        // damage smoke
        if (e.hp < e.maxHp * 0.5f && e.constructed && ((g_sim.tick + e.gen) % 7 == 0) && !paused)
            g_sim.fx.push_back({FX_SMOKE, e.pos + Vec2(g_sim.rng.f(-bt.w * 10.0f, bt.w * 10.0f), g_sim.rng.f(-bt.h * 10.0f, bt.h * 10.0f)), Vec2(), 0, 1.6f, rgb(40, 40, 40), 6, Vec2(0, -18)});
        if (selected) {
            g.box((int)(p.x - s.ox) - 2, (int)(p.y - s.oy) - 2, s.w + 4, s.h + 4, rgb(255, 255, 255, 200));
            if (e.hasRally && e.constructed) { Vec2 r = worldToScreen(e.rally); g.line(p.x, p.y, r.x, r.y, rgb(120, 255, 120, 160)); g.circle(r.x, r.y, 5, rgb(120, 255, 120)); }
        }
        if (selected || e.hp < e.maxHp || !e.constructed) hpBar(g, p.x - s.w * 0.5f, p.y - s.h * 0.5f - 6, s.w, e.constructed ? e.hp / e.maxHp : e.progress, true);
        if (disabled) g.text((int)p.x - 9, (int)p.y - 4, "EMP", rgb(160, 220, 255));
        return;
    }
    const UnitType& ut = e.ut();
    const Sprite& body = g.unitBody[e.type][owner];
    bool air = ut.kind == UK_AIR;
    if (air) {
        g.draw(g.shadowLarge, p.x + 14, p.y + 26, 0, 0.8f, rgb(255, 255, 255), 120);
        p.y -= 14;   // altitude
    } else {
        g.draw(ut.kind == UK_INF ? g.shadowSmall : g.shadowLarge, p.x, p.y + 3, 0, ut.kind == UK_INF ? 0.55f : 0.75f);
    }
    if (selected) g.circle(p.x, p.y + (air ? 14 : 0), e.radius() + 4, rgb(255, 255, 255, 230), 18);
    Color mod = disabled ? rgb(120, 140, 170) : rgb(255, 255, 255);
    g.draw(body, p.x, p.y, e.angle, 1, mod);
    const Sprite& tur = g.unitTurret[e.type];
    if (tur.tex) g.draw(tur, p.x, p.y, e.turret, 1, mod);
    if (ut.kind == UK_AIR && ut.faction == F_CLANKER) g.circle(p.x + 2, p.y, 12, rgb(90, 90, 90, 90 + 60 * ((g_sim.tick & 1))), 12); // rotor blur
    if (e.cargo > 0) g.fillCircle(p.x, p.y, 3, rgb(230, 200, 90));
    if (selected || e.hp < e.maxHp) hpBar(g, p.x - 10, p.y - e.radius() - 7, 20, e.hp / e.maxHp);
    if (selected && air && ut.ammo > 0) for (int k = 0; k < ut.ammo; k++) g.fill((int)p.x - 10 + k * 3, (int)p.y - e.radius() - 11, 2, 2, k < e.ammo ? rgb(255, 230, 120) : rgb(80, 80, 80));
}

void Game::drawFx() {
    Gfx& g = g_gfx;
    for (auto& f : g_sim.fx) {
        float k = f.t / f.life;
        Vec2 a = worldToScreen(f.a), b = worldToScreen(f.b);
        switch (f.type) {
        case FX_BEAM: { Color c = f.color; c.a = (u8)(255 * (1 - k)); g.thickLine(a.x, a.y, b.x, b.y, c, 2); c.a /= 3; g.thickLine(a.x, a.y, b.x, b.y, c, 5); break; }
        case FX_RAIL: { Color c = f.color; c.a = (u8)(255 * (1 - k)); g.thickLine(a.x, a.y, b.x, b.y, c, 3); Color h = rgb(120, 200, 255, (u8)(120 * (1 - k))); g.thickLine(a.x, a.y, b.x, b.y, h, 7); break; }
        case FX_ARC: {
            Color c = f.color; c.a = (u8)(255 * (1 - k));
            Rng r((u64)(f.t * 1000) + (u64)f.a.x);
            Vec2 prev = a; int segs = 6;
            for (int i = 1; i <= segs; i++) { float t = i / (float)segs; Vec2 q = a + (b - a) * t; if (i < segs) q += Vec2(r.f(-7, 7), r.f(-7, 7)); g.thickLine(prev.x, prev.y, q.x, q.y, c, 2); prev = q; }
            break;
        }
        case FX_FLASH: g.fillCircle(a.x, a.y, f.size * (1 - k * 0.5f), rgb(255, 250, 200, 220)); break;
        case FX_EXPLODE: {
            float rr = f.size * (0.3f + 0.7f * k);
            Color c = mix(rgb(255, 240, 160), rgb(200, 60, 20), k); c.a = (u8)(230 * (1 - k * k));
            g.fillCircle(a.x, a.y, rr, c);
            g.fillCircle(a.x, a.y, rr * 0.5f, rgb(255, 255, 220, (u8)(200 * (1 - k))));
            break;
        }
        case FX_SMOKE: { Color c = f.color; c.a = (u8)(140 * (1 - k)); g.fillCircle(a.x, a.y, f.size * (0.6f + k), c); break; }
        case FX_SPARK: { Color c = f.color; c.a = (u8)(255 * (1 - k)); g.fillCircle(a.x, a.y, f.size, c); break; }
        case FX_DEBRIS: g.fill((int)a.x, (int)a.y, (int)f.size, (int)f.size, f.color); break;
        case FX_RING: g.circle(a.x, a.y, f.size * k, f.color); break;
        case FX_EMP: { Color c = f.color; c.a = (u8)(255 * (1 - k)); g.circle(a.x, a.y, f.size * k, c, 40); c.a /= 2; g.circle(a.x, a.y, f.size * k * 0.7f, c, 40); break; }
        }
    }
}

void Game::renderWorld() {
    Gfx& g = g_gfx;
    g.beginFrame(rgb(6, 8, 12));
    SDL_Rect clip = { 0, 0, SCREEN_W, VIEW_H };
    SDL_RenderSetClipRect(g.ren, &clip);
    // terrain
    int tx0 = std::max(0, tileOf(cam.x)), ty0 = std::max(0, tileOf(cam.y));
    int tx1 = std::min(MAP_W - 1, tileOf(cam.x + SCREEN_W)), ty1 = std::min(MAP_H - 1, tileOf(cam.y + VIEW_H));
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    static const int PRIO[T_COUNT] = { 3, 3, 2, 1, 4, 0, 5, 3 };
    static const int DX[4] = { 0, 1, 0, -1 }, DY[4] = { -1, 0, 1, 0 };
    for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
        if (!ex[ty * MAP_W + tx]) continue;
        u8 t = g_map.tile(tx, ty);
        int v = (g_map.variant[ty * MAP_W + tx] + (t == T_WATER ? (int)(wallTime * 2.5f) : 0)) % 4;
        int sx = (int)(tx * TILE - cam.x), sy = (int)(ty * TILE - cam.y);
        g.drawRect(g.tiles[t][v], sx, sy, TILE, TILE);
        // blend higher-priority neighbours over this tile's edges
        for (int d = 0; d < 4; d++) {
            int nx = tx + DX[d], ny = ty + DY[d];
            if (!inMap(nx, ny)) continue;
            u8 n = g_map.tile(nx, ny);
            if (PRIO[n] <= PRIO[t] || n == T_TREE) continue;
            g.draw(g.edgeTile[n], sx + 16, sy + 16, d * 1.5707963f);
        }
    }
    // placement ghost
    if (placingType >= 0) {
        Vec2 w = screenToWorld(mouseX, mouseY);
        const BuildType& bt = BUILDS[placingType];
        int tx = tileOf(w.x) - bt.w / 2, ty = tileOf(w.y) - bt.h / 2;
        bool ok = g_sim.canPlace(g_sim.humanPlayer, placingType, tx, ty);
        Vec2 c = worldToScreen(g_sim.buildingCenter(placingType, tx, ty));
        g.draw(g.building[placingType][g_sim.humanPlayer], c.x, c.y, 0, 1, ok ? rgb(160, 255, 160) : rgb(255, 120, 120), 150);
        for (int j = 0; j < bt.h; j++) for (int i = 0; i < bt.w; i++) {
            bool t = g_map.buildable(tx + i, ty + j) && g_sim.explored(g_sim.humanPlayer, clampi(tx + i, 0, MAP_W - 1), clampi(ty + j, 0, MAP_H - 1));
            g.fill((int)((tx + i) * TILE - cam.x), (int)((ty + j) * TILE - cam.y), TILE, TILE, t ? rgb(80, 255, 80, 60) : rgb(255, 60, 60, 90));
        }
    }
    if (powerMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        g.circle(m.x, m.y, POWERS[g_sim.players[0].faction].radius * TILE, hudAccent(g_sim.players[0].faction), 40);
    }
    // entities: resources, buildings, then ground units sorted by y, projectiles, fx, aircraft
    std::vector<Entity*> ground, air;
    float vx0 = cam.x - 96, vy0 = cam.y - 96, vx1 = cam.x + SCREEN_W + 96, vy1 = cam.y + VIEW_H + 96;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (e.owner != g_sim.humanPlayer && !ex[clampi(tileOf(e.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(e.pos.x), 0, MAP_W - 1)]) continue;
        if (e.kind == EK_RESOURCE || e.isBuilding()) drawEntity(e);
        else if (e.isAir()) air.push_back(&e);
        else ground.push_back(&e);
    }
    std::sort(ground.begin(), ground.end(), [](Entity* a, Entity* b) { return a->pos.y < b->pos.y; });
    for (auto* e : ground) drawEntity(*e);
    // projectiles
    for (auto& p : g_sim.projs) {
        const Weapon& w = WEAPONS[p.weapon];
        Vec2 s = worldToScreen(p.prevPos + (p.pos - p.prevPos) * renderAlpha);
        if (w.proj == PJ_SHELL) {
            float h = std::sin(clampf(p.arcT, 0, 1) * 3.14159f) * 60;
            g.fillCircle(s.x, s.y + 2, 3, rgb(0, 0, 0, 90));
            g.fillCircle(s.x, s.y - h, 2.5f, rgb(40, 40, 40));
        } else if (w.proj == PJ_ROCKET) {
            Vec2 d = p.vel.norm();
            g.thickLine(s.x - d.x * 6, s.y - d.y * 6, s.x, s.y, rgb(255, 230, 160), 2);
            g.fillCircle(s.x, s.y, 2, w.color);
        } else {
            Vec2 d = p.vel.norm();
            g.thickLine(s.x - d.x * 8, s.y - d.y * 8, s.x, s.y, w.color, 1);
        }
    }
    drawFx();
    for (auto* e : air) drawEntity(*e);
    drawShroud();
    // drag box
    if (dragging) {
        Vec2 a = worldToScreen(dragStart), b = worldToScreen(dragNow);
        g.fill((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 30));
        g.box((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 200));
    }
    // cursor hints
    if (attackMoveMode) g.text(mouseX + 12, mouseY - 4, "ATTACK", rgb(255, 90, 80));
    else if (powerMode) g.text(mouseX + 12, mouseY - 4, POWERS[g_sim.players[0].faction].name, hudAccent(g_sim.players[0].faction));
    else if (rallyMode) g.text(mouseX + 12, mouseY - 4, "RALLY", rgb(120, 255, 120));
    else if (mouseY < VIEW_H) {
        Entity* h = pickEntity(screenToWorld(mouseX, mouseY), false);
        if (h && h->kind != EK_RESOURCE && h->owner != g_sim.humanPlayer) {
            const char* nm = h->isUnit() ? h->ut().name : h->bt().name;
            char buf[80]; snprintf(buf, sizeof buf, "%s (%s)", nm, h->owner >= 0 ? FACTION_NAME[g_sim.players[h->owner].faction] : "");
            g.text(mouseX + 12, mouseY - 4, buf, rgb(255, 150, 140));
        } else if (h && h->kind == EK_RESOURCE) { char buf[40]; snprintf(buf, sizeof buf, "Supplies: $%d", h->amount); g.text(mouseX + 12, mouseY - 4, buf, rgb(240, 220, 150)); }
    }
    SDL_RenderSetClipRect(g.ren, nullptr);
}

void Game::drawMinimap(int x, int y, int size) {
    Gfx& g = g_gfx;
    g.drawRect(g.minimapTerrain, x, y, size, size);
    float sc = size / (float)MAP_W;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    SDL_SetRenderDrawColor(g.ren, 4, 6, 10, 255);
    // shroud in 2x2 tile blocks (cheap)
    for (int ty = 0; ty < MAP_H; ty += 2) for (int tx = 0; tx < MAP_W; tx += 2) {
        if (ex[ty * MAP_W + tx] || ex[ty * MAP_W + tx + 1] || ex[(ty + 1) * MAP_W + tx] || ex[(ty + 1) * MAP_W + tx + 1]) continue;
        SDL_Rect r = { (int)(x + tx * sc), (int)(y + ty * sc), (int)std::ceil(2 * sc), (int)std::ceil(2 * sc) };
        SDL_RenderFillRect(g.ren, &r);
    }
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (e.kind == EK_RESOURCE) { if (ex[ty * MAP_W + tx]) g.fill((int)(x + tx * sc), (int)(y + ty * sc), 2, 2, rgb(230, 200, 90)); continue; }
        if (e.owner != g_sim.humanPlayer && !ex[ty * MAP_W + tx]) continue;
        Color c = PLAYER_COLOR[e.owner];
        int s = e.isBuilding() ? std::max(2, (int)(e.bt().w * sc)) : 2;
        g.fill((int)(x + e.pos.x / WORLD_W * size) - s / 2, (int)(y + e.pos.y / WORLD_H * size) - s / 2, s, s, c);
    }
    // recent attack pings: a blinking ring that fades over 8 seconds
    for (auto& p : pings) {
        float age = wallTime - p.time;
        if (age > 8 || ((int)(age * 4) & 1)) continue;
        g.circle(x + p.pos.x / WORLD_W * size, y + p.pos.y / WORLD_H * size, 4 + age * 0.5f, rgb(255, 80, 60), 12);
    }
    g.box((int)(x + cam.x / WORLD_W * size), (int)(y + cam.y / WORLD_H * size), (int)(SCREEN_W / (float)WORLD_W * size), (int)(VIEW_H / (float)WORLD_H * size), rgb(255, 255, 255, 220));
}

void Game::renderHud() {
    Gfx& g = g_gfx;
    Player& pl = g_sim.players[g_sim.humanPlayer];
    Color base = hudBase(pl.faction), accent = hudAccent(pl.faction);
    int hy = SCREEN_H - HUD_H;
    // ---- top bar
    g.fill(0, 0, SCREEN_W, 20, rgb(8, 10, 14, 190));
    char buf[128];
    snprintf(buf, sizeof buf, "$%d", pl.money); g.text(8, 6, buf, rgb(240, 220, 130), 1);
    // power meter
    int px = 110;
    g.text(px, 6, "POWER", hudDim());
    int made = pl.powerMade, used = pl.powerUsed;
    int barW = 100; int mx = std::max(1, std::max(made, used));
    g.fill(px + 44, 7, barW, 7, rgb(30, 34, 40));
    g.fill(px + 44, 7, (int)(barW * std::min(1.0f, made / (float)mx)), 7, pl.lowPower() ? rgb(230, 70, 60) : rgb(80, 220, 90));
    g.fill(px + 44 + (int)(barW * std::min(1.0f, used / (float)mx)) - 1, 5, 2, 11, rgb(255, 255, 255));
    snprintf(buf, sizeof buf, "%d/%d", used, made); g.text(px + 150, 6, buf, pl.lowPower() ? rgb(255, 140, 120) : hudText());
    int secs = (int)g_sim.time;
    snprintf(buf, sizeof buf, "%02d:%02d", secs / 60, secs % 60); g.text(SCREEN_W / 2 - 15, 6, buf, hudText());
    snprintf(buf, sizeof buf, "%s%s  speed x%.1f%s", FACTION_NAME[pl.faction], g_audio.isMuted() ? "  [muted]" : "", speed, paused ? "  PAUSED" : "");
    g.text(SCREEN_W - 8 - g.textW(buf), 6, buf, hudDim());
    // alive players
    int ax = SCREEN_W / 2 + 40;
    for (int p = 0; p < g_sim.numPlayers; p++) { g.fill(ax + p * 14, 7, 9, 7, g_sim.players[p].alive ? PLAYER_COLOR[p] : rgb(60, 60, 60)); }
    // messages
    for (size_t i = 0; i < messages.size(); i++) {
        float age = wallTime - messages[i].time;
        if (age > 9) continue;
        Color c = messages[i].color; c.a = (u8)(255 * clampf(1.5f - age / 6.0f, 0, 1));
        g.text(8, 26 + (int)i * 11, messages[i].text.c_str(), c);
    }
    if (paused) g.text(SCREEN_W / 2 - g.textW("PAUSED", 3) / 2, VIEW_H / 2 - 12, "PAUSED", rgb(255, 255, 255), 3);

    // ---- command bar
    g.fill(0, hy, SCREEN_W, HUD_H, base);
    g.fill(0, hy, SCREEN_W, 2, accent);
    g.fill(0, hy + 2, SCREEN_W, 1, shade(base, 1.5f));
    // minimap frame
    g.bevelPanel(MINIMAP_X - 3, MINIMAP_Y - 3, MINIMAP_SIZE + 6, MINIMAP_SIZE + 6, shade(base, 0.7f), false);
    drawMinimap(MINIMAP_X, MINIMAP_Y, MINIMAP_SIZE);
    // info panel
    g.bevelPanel(INFO_X, hy + 6, INFO_W, HUD_H - 12, shade(base, 0.8f), false);
    if (selection.size() == 1) {
        Entity* e = g_sim.get(selection[0]);
        if (e) {
            // portrait
            g.bevelPanel(INFO_X + 8, hy + 14, 68, 68, shade(base, 0.55f), false);
            if (e->isUnit()) {
                g.draw(g.unitBody[e->type][e->owner], INFO_X + 42, hy + 48, 0, 2);
                if (g.unitTurret[e->type].tex) g.draw(g.unitTurret[e->type], INFO_X + 42, hy + 48, 0, 2);
            } else {
                const Sprite& s = g.building[e->type][e->owner];
                float sc = std::min(60.0f / s.w, 60.0f / s.h);
                g.draw(s, INFO_X + 42, hy + 48, 0, sc);
            }
            const char* name = e->isUnit() ? e->ut().name : e->bt().name;
            g.text(INFO_X + 86, hy + 16, name, rgb(255, 255, 255), 2);
            if (e->isBuilding() && !e->constructed) { snprintf(buf, sizeof buf, "Under construction %d%%", (int)(e->progress * 100)); g.text(INFO_X + 86, hy + 36, buf, rgb(240, 220, 130)); }
            else { snprintf(buf, sizeof buf, "HP %d / %d", (int)std::ceil(e->hp), (int)e->maxHp); g.text(INFO_X + 86, hy + 36, buf, hudText()); }
            hpBar(g, INFO_X + 86, hy + 48, 150, e->isBuilding() && !e->constructed ? e->progress : e->hp / e->maxHp, true);
            if (e->isUnit()) {
                const UnitType& ut = e->ut();
                if (ut.weapon >= 0) { const Weapon& w = WEAPONS[ut.weapon]; snprintf(buf, sizeof buf, "%s  dmg %d  range %.1f%s%s", w.name, (int)w.dmg, w.range, w.air ? "  AA" : "", w.ground ? "" : "  air only"); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.role == UR_HARVESTER) { snprintf(buf, sizeof buf, "Cargo $%d / %d", e->cargo, SUPPLY_PER_TRIP); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.ammo > 0) { snprintf(buf, sizeof buf, "Ammo %d / %d", e->ammo, ut.ammo); g.text(INFO_X + 86, hy + 70, buf, hudDim()); }
                g.text(INFO_X + 86, hy + 84, ut.desc, hudDim());
                const char* st = e->order == O_IDLE ? "Idle" : e->order == O_MOVE ? "Moving" : e->order == O_ATTACKMOVE ? "Attack-moving" : e->order == O_ATTACK ? "Attacking" : e->order == O_HARVEST ? "Gathering" : e->order == O_RETURN ? "Returning" : e->order == O_BUILD ? "Constructing" : e->order == O_REARM ? "Rearming" : "Guarding";
                g.text(INFO_X + 86, hy + 98, st, accent);
            } else {
                const BuildType& bt = e->bt();
                if (bt.power != 0) { snprintf(buf, sizeof buf, "Power %+d", bt.power); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                { char d[40]; snprintf(d, sizeof d, "%.38s", bt.desc); g.text(INFO_X + 86, hy + 70, d, hudDim()); }
                if (!e->queue.empty()) {
                    g.text(INFO_X + 330, hy + 58, "Queue (click to cancel)", hudDim());
                    int qx = INFO_X + 330, qy = hy + 70;
                    for (int i = 0; i < (int)e->queue.size() && i < 9; i++) {
                        g.bevelPanel(qx + i * 30, qy, 28, 28, shade(base, 0.55f), false);
                        g.draw(g.unitBody[e->queue[i]][e->owner], qx + i * 30 + 14, qy + 14, 0, 0.8f);
                        if (i == 0) hpBar(g, qx, qy + 30, 28, e->queueProgress);
                    }
                    snprintf(buf, sizeof buf, "%s %d%%", UNITS[e->queue[0]].name, (int)(e->queueProgress * 100)); g.text(INFO_X + 330, hy + 104, buf, hudText());
                } else if (e->constructed && (bt.role == BR_BARRACKS || bt.role == BR_FACTORY || bt.role == BR_AIRFIELD || bt.role == BR_HQ || bt.role == BR_SUPPLY)) g.text(INFO_X + 330, hy + 58, "Production idle", hudDim());
                if (pl.lowPower() && bt.power < 0) g.text(INFO_X + 86, hy + 98, "LOW POWER: reduced output", rgb(255, 140, 120));
            }
        }
    } else if (selection.size() > 1) {
        snprintf(buf, sizeof buf, "%d units selected", (int)selection.size());
        g.text(INFO_X + 10, hy + 14, buf, rgb(255, 255, 255), 2);
        int i = 0;
        for (auto r : selection) {
            Entity* e = g_sim.get(r);
            if (!e || !e->isUnit()) continue;
            if (i >= 32) break;
            int sx = INFO_X + 10 + (i % 16) * 32, sy = hy + 40 + (i / 16) * 40;
            g.bevelPanel(sx, sy, 30, 30, shade(base, 0.55f), false);
            g.draw(g.unitBody[e->type][e->owner], sx + 15, sy + 15, 0, 0.85f);
            hpBar(g, sx, sy + 32, 30, e->hp / e->maxHp);
            i++;
        }
    } else {
        g.text(INFO_X + 10, hy + 14, "No selection", hudDim(), 2);
        g.text(INFO_X + 10, hy + 40, "Left-click / drag: select    Right-click: move, attack, gather, repair", hudDim());
        g.text(INFO_X + 10, hy + 52, "A: attack-move   S: stop   Tab: select army on screen   Ctrl+#: group", hudDim());
        g.text(INFO_X + 10, hy + 64, "Arrows/edge/middle-drag/wheel: scroll   Home: base   Space: pause   +/-: speed", hudDim());
        g.text(INFO_X + 10, hy + 76, "Select a Dozer to build structures; select a structure to train units", hudDim());
        g.text(INFO_X + 10, hy + 88, "Shift+click while placing: place several   Del: sell   F2: mute   F1: help", hudDim());
        g.text(INFO_X + 10, hy + 100, "Win by destroying every enemy unit and structure.", hudDim());
    }
    // command grid
    buildButtons();
    hoverButton = -1;
    for (size_t i = 0; i < buttons.size(); i++) {
        Button& b = buttons[i];
        bool hover = mouseX >= b.x && mouseX < b.x + b.w && mouseY >= b.y && mouseY < b.y + b.h;
        if (hover) hoverButton = (int)i;
        Color bc = b.enabled ? (hover ? shade(base, 1.5f) : shade(base, 1.15f)) : shade(base, 0.7f);
        g.bevelPanel(b.x, b.y, b.w, b.h, bc, true);
        Color tc = b.enabled ? rgb(255, 255, 255) : hudDim();
        g.text(b.x + 5, b.y + 5, b.label, tc);
        int cost = b.kind == BK_BUILD ? BUILDS[b.id].cost : (b.kind == BK_TRAIN ? UNITS[b.id].cost : -1);
        if (cost >= 0) { snprintf(buf, sizeof buf, "$%d", cost); g.text(b.x + 5, b.y + 20, buf, pl.money >= cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        if (b.kind == BK_POWER) {
            float rem = pl.powerReady - g_sim.time;
            if (rem > 0) { snprintf(buf, sizeof buf, "%d:%02d", (int)rem / 60, (int)rem % 60); g.text(b.x + 5, b.y + 20, buf, hudDim()); }
            else if (b.enabled) g.text(b.x + 5, b.y + 20, "READY", accent);
        }
        const char* hk = b.kind == BK_BUILD ? BUILDS[b.id].hotkey : (b.kind == BK_TRAIN ? UNITS[b.id].hotkey : (b.kind == BK_POWER ? "X" : (b.kind == BK_ATTACKMOVE ? "A" : (b.kind == BK_STOP ? "S" : nullptr))));
        if (hk) g.text(b.x + b.w - 12, b.y + 20, hk, accent);
        if (b.kind == BK_BUILD && placingType == b.id) g.box(b.x, b.y, b.w, b.h, accent);
    }
    if (hoverButton >= 0) {
        const std::string& tip = buttons[hoverButton].tip;
        int tw = g.textW(tip.c_str()) + 12;
        int tx = std::min(SCREEN_W - tw - 4, std::max(4, mouseX - tw / 2));
        g.fill(tx, VIEW_H - 22, tw, 16, rgb(8, 10, 14, 230));
        g.text(tx + 6, VIEW_H - 18, tip.c_str(), rgb(255, 255, 255));
    }
    if (showHelp) {
        g.fill(200, 60, 624, 300, rgb(8, 10, 14, 235));
        g.box(200, 60, 624, 300, accent);
        g.text(216, 72, "ONE HOUR - controls", rgb(255, 255, 255), 2);
        const char* lines[] = {
            "Left click / drag box      select units (double-click: all of that type on screen)",
            "Right click                move / attack / gather / repair / rally (also on the minimap)",
            "A + click                  attack-move       S: stop        Tab: army on screen",
            "Ctrl + 0-9 / 0-9           assign / recall control group (Alt+#: jump to it)",
            "Arrows, screen edge, middle-drag, wheel: scroll     Home: your base",
            "Dozer selected             build menu; click the ground to place, Shift for several",
            "Structure selected         train units; right-click ground for rally; Del sells",
            "Command Core/Post          special power (X) once your tech structure is built",
            "Space pause    + / - speed    F2 mute    F11 fullscreen    F12 screenshot    Esc menu",
            "",
            "Supplies: haulers carry $300 per trip to a Supply Hub/Depot.",
            "Power: keep production above consumption or turrets and factories slow down.",
        };
        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) g.text(216, 100 + (int)i * 16, lines[i], hudText());
    }
}

void Game::renderGameOver() {
    Gfx& g = g_gfx;
    bool won = g_sim.winnerTeam == g_sim.players[0].team;
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(0, 0, 0, 150));
    const char* t = won ? "VICTORY" : "DEFEAT";
    g.text(SCREEN_W / 2 - g.textW(t, 6) / 2, 150, t, won ? rgb(140, 255, 150) : rgb(255, 110, 100), 6);
    Player& pl = g_sim.players[0];
    char buf[128];
    int secs = (int)g_sim.time;
    snprintf(buf, sizeof buf, "Time %02d:%02d   Units built %d   Units lost %d   Kills %d   Structures destroyed %d   Supplies gathered $%d",
             secs / 60, secs % 60, pl.unitsBuilt, pl.unitsLost, pl.unitsKilled, pl.structuresKilled, pl.harvested);
    g.text(SCREEN_W / 2 - g.textW(buf) / 2, 230, buf, hudText());
    g.text(SCREEN_W / 2 - g.textW("Press Enter to return to the menu", 2) / 2, 270, "Press Enter to return to the menu", hudDim(), 2);
}

void Game::screenshot(const char* path) {
    Gfx& g = g_gfx;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    SDL_Rect r = { 0, 0, SCREEN_W, SCREEN_H };
    if (SDL_RenderReadPixels(g.ren, &r, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}
