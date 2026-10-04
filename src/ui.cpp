#include "ui.h"
#include "art.h"

Game g_game;

enum { BK_BUILD = 1, BK_TRAIN, BK_SELL, BK_RALLY, BK_POWER, BK_ATTACKMOVE, BK_STOP, BK_CANCEL, BK_AREA, BK_SCAN, BK_RESEARCH, BK_NUKE };

static const char* kindHotkey(int kind, int id) {
    switch (kind) {
    case BK_BUILD: return BUILDS[id].hotkey;
    case BK_TRAIN: return UNITS[id].hotkey;
    case BK_POWER: return "X";
    case BK_SCAN: return "V";
    case BK_RESEARCH: return "R";
    case BK_NUKE: return "K";
    case BK_ATTACKMOVE: return "A";
    case BK_STOP: return "S";
    case BK_AREA: return "G";
    }
    return nullptr;
}
static const float AREA_DEFAULT_R = 6.0f * TILE;   // circle radius for a plain click in area mode

// HUD layout: minimap left, command grid anchored right, the info panel takes whatever is between
static const int MINIMAP_X = 8, MINIMAP_SIZE = 116, BTN_W = 120, BTN_H = 28, BTN_GAP = 3;
static const int INFO_X = 134;
static inline int MINIMAP_Y_() { return SCREEN_H - HUD_H + 8; }
static inline int GRID_X_() { return SCREEN_W - (3 * BTN_W + 2 * BTN_GAP) - 10; }
static inline int GRID_Y_() { return SCREEN_H - HUD_H + 8; }
static inline int INFO_W_() { return GRID_X_() - 8 - INFO_X; }
#define MINIMAP_Y MINIMAP_Y_()
#define GRID_X GRID_X_()
#define GRID_Y GRID_Y_()
#define INFO_W INFO_W_()

static Color hudBase(Faction f) { return f == F_CYBER ? rgb(36, 44, 58) : rgb(62, 60, 46); }
static Color hudAccent(Faction f) { return f == F_CYBER ? rgb(110, 230, 255) : rgb(222, 178, 60); }
static Color hudText() { return rgb(222, 226, 230); }
static Color hudDim() { return rgb(150, 156, 164); }

static int menuOffsetY() { return std::max(0, (SCREEN_H - MIN_SCREEN_H) / 2); }
static const float SPEED_STEPS[Game::SPEED_COUNT] = { 0.5f, 0.75f, 1.0f, 1.5f, 2.0f, 3.0f, 4.0f };
static const char* DIFF_NAME[4] = { "Easy", "Normal", "Hard", "Brutal" };

void Game::addMessage(const char* text, Color c) {
    messages.push_back({text, wallTime, c});
    if (messages.size() > 6) messages.erase(messages.begin());
}

// ------------------------------------------------------------ game setup
void Game::startGame() {
    Faction fac[MAX_PLAYERS]; bool ai[MAX_PLAYERS]; int diff[MAX_PLAYERS]; int team[MAX_PLAYERS];
    Rng r(seed);
    int n = 0;
    fac[n] = menu.playerFaction; ai[n] = false; diff[n] = 1; team[n] = 0; g_colorSlot[n] = 0; n++;
    if (menu.hasAlly()) {   // a computer-controlled ally on your team: green, with the difficulty chosen for it
        fac[n] = menu.allyFaction == 2 ? (Faction)r.range(0, 1) : (Faction)menu.allyFaction;
        ai[n] = true; diff[n] = menu.allyDiff; team[n] = 0; g_colorSlot[n] = 3; n++;
    }
    int e = 0;
    for (int i = 0; i < 3 && n < MAX_PLAYERS; i++) {
        int f = menu.enemyFaction[i];
        if (f == 3) continue;
        fac[n] = f == 2 ? (Faction)r.range(0, 1) : (Faction)f;
        ai[n] = true; diff[n] = menu.enemyDiff[i]; team[n] = menu.enemiesAllied ? 1 : 1 + e; g_colorSlot[n] = 1 + e;
        n++; e++;
    }
    for (int k = n; k < MAX_PLAYERS; k++) g_colorSlot[k] = k;
    g_sim.init(n, fac, ai, diff, team, seed);
    g_ai.init(seed);
    selection.clear();
    for (auto& g : groups) g.clear();
    messages.clear();
    parts.clear(); decals.clear(); nukeFlash = 0; frameDt = 0;   // no smoke, scorch or flash left over from the previous match
    placingType = -1; attackMoveMode = false; powerMode = false; nukeMode = false; rallyMode = false; areaMode = false; areaDrag = false; zoneFlashes.clear(); paused = false; speed = 1.0f; menuOpen = false; menuConfirm = -1;
    accumulator = 0;
    Vec2 b = g_sim.players[0].basePos;
    cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
    state = GS_PLAYING;
    addMessage("Skirmish started. Build a Supply Hub and power first.", hudText());
    if (menu.hasAlly()) addMessage("Your ally (green) fights on your side, shares its vision and defends your base.", rgb(150, 235, 165));
}

// ------------------------------------------------------------ update
void Game::update(float dt) {
    wallTime += dt;
    if (state != GS_PLAYING) return;
    if (!menuOpen) scroll(dt);
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
    frameDt = (paused || menuOpen || g_sim.gameOver) ? 0.0f : dt * speed;
    spawnFromFx();
    updateParticles(frameDt);
    processEvents();
    cleanSelection();
    g_audio.setListener(cam, SCREEN_W, VIEW_H);
    if (g_sim.gameOver && state == GS_PLAYING) {
        state = GS_GAMEOVER; gameOverAt = wallTime;
        if (!g_brainPath.empty()) { g_ai.finish(g_sim.winnerTeam); g_brain.games++; g_brain.save(g_brainPath.c_str()); }   // each commander's doctrine learns whether it won
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
        case EV_MSG: if (mine) addMessage(ev.msg.c_str(), rgb(255, 200, 120)); break;
        case EV_PLAYER_DEAD: {
            bool ally = ev.player != g_sim.humanPlayer && !g_sim.enemies(g_sim.humanPlayer, ev.player);
            char buf[96]; snprintf(buf, sizeof buf, "%s (%s) has been eliminated", ev.player == g_sim.humanPlayer ? "You" : (ally ? "Your ally" : "Enemy"), ev.msg.c_str());
            addMessage(buf, ev.player == g_sim.humanPlayer || ally ? rgb(255, 120, 100) : rgb(150, 240, 150));
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
// how far a structure's raised roof reaches above its footprint (matches the art in artb.cpp): the visible roof is clickable too
static float roofLift(const BuildType& b) {
    switch (b.role) {
    case BR_HQ: return 22; case BR_POWER: return 14; case BR_SUPPLY: return 16; case BR_BARRACKS: return 12; case BR_FACTORY: return 20;
    case BR_AIRFIELD: return 14; case BR_TECH: return 16; case BR_AATURRET: return 6; case BR_INCOME: return b.faction == F_CYBER ? 10 : 8; case BR_NUKE: return 16;
    default: return 0;
    }
}

Entity* Game::pickEntity(Vec2 w, bool ownOnly) {
    Entity* best = nullptr; float bd = 1e9f;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (ownOnly && e.owner != g_sim.humanPlayer) continue;
        if (e.owner != g_sim.humanPlayer && e.kind != EK_RESOURCE && !g_sim.explored(g_sim.humanPlayer, clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1))) continue;
        float d;
        if (e.isBuilding()) {
            if (g_sim.distToEntity(w, e) <= 0) d = dist(w, e.pos) * 0.1f;
            else {   // the lifted roof above the footprint: anything standing there is picked in preference
                float x0 = (float)e.tx * TILE, y0 = (float)e.ty * TILE, lift = roofLift(e.bt());
                d = (e.constructed && w.x >= x0 && w.x < x0 + e.bt().w * TILE && w.y < y0 && w.y >= y0 - lift) ? 400.0f : 1e9f;
            }
        }
        else if (e.kind == EK_RESOURCE) d = dist(w, e.pos) <= 20 ? dist(w, e.pos) : 1e9f;
        else if (e.isAir()) {   // aircraft are drawn lifted off their ground point: pick the craft that is on screen, with a generous hit area
            Vec2 drawn(e.pos.x, e.pos.y - 14.0f * e.alt);
            d = dist(w, drawn) <= e.radius() + 12 ? dist(w, drawn) * 0.5f : 1e9f;
        }
        else d = dist(w, e.pos) <= e.radius() + 6 ? dist(w, e.pos) : 1e9f;
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
        if (i >= 12) return;
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
            const char* need = ok ? "" : (ut.program && !pl.advTech ? "  Requires the Advanced Program (tech structure)" : (ut.requires_ >= 0 ? "  Requires " : ""));
            snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s%s%s", ut.name, ut.cost, ut.hotkey, ut.desc, need, (!ok && !(ut.program && !pl.advTech) && ut.requires_ >= 0) ? BUILDS[ut.requires_].name : "");
            add(BK_TRAIN, u, ok, ut.name, tip);
        }
        if (role == BR_HQ || role == BR_TECH) {
            bool tech = g_sim.hasRole(g_sim.humanPlayer, BR_TECH);
            const PowerType& pw = POWERS[pl.faction];
            snprintf(tip, sizeof tip, "%s  [X]  %s%s", pw.name, pw.desc, tech ? "" : "  Requires tech structure");
            add(BK_POWER, 0, tech && g_sim.time >= pl.powerReady, pw.name, tip);
        }
        if (role == BR_TECH) {
            const ScanType& sc = SCANS[pl.faction];
            snprintf(tip, sizeof tip, "%s  [V]  %s", sc.name, sc.desc);
            add(BK_SCAN, 0, g_sim.time >= pl.scanReady, sc.name, tip);
            const ProgramType& pg = PROGRAMS[pl.faction];
            if (pl.advTech) snprintf(tip, sizeof tip, "%s researched: %s", pg.name, pg.desc);
            else if (pl.researching) snprintf(tip, sizeof tip, "%s in progress (%d%%): %s", pg.name, (int)(pl.researchProgress * 100), pg.desc);
            else snprintf(tip, sizeof tip, "%s  $%d  [R]  %s", pg.name, pg.cost, pg.desc);
            add(BK_RESEARCH, 0, g_sim.programAvailable(g_sim.humanPlayer) && g_sim.canAfford(g_sim.humanPlayer, pg.cost), pg.name, tip);
        }
        if (role == BR_NUKE) {
            int rdy = g_sim.nukesReady(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "Launch Nuke  [K]  Obliterates a %d tile radius and leaves lethal radiation for over a minute. One warhead per ramp every 5 minutes%s", (int)NUKE_RADIUS, pl.lowPower() ? "  (LOW POWER)" : "");
            add(BK_NUKE, 0, rdy > 0, "Launch Nuke", tip);
        }
        if (role == BR_BARRACKS || role == BR_FACTORY || role == BR_AIRFIELD || role == BR_SUPPLY) add(BK_RALLY, 0, true, "Rally", "Set rally point (right-click ground while selected)");
        add(BK_SELL, 0, true, "Sell", "Sell this structure for 50% of its cost");
        return;
    }
    if (selectionHasRole(UR_DOZER)) {
        // the Command Core / Post comes last so the other buttons keep their places
        for (int pass = 0; pass < 2; pass++) for (int t = 0; t < B_COUNT; t++) {
            const BuildType& bt = BUILDS[t];
            if (bt.faction != pl.faction || (bt.role == BR_HQ) != (pass == 1)) continue;
            bool ok = g_sim.buildAvailable(g_sim.humanPlayer, t);
            bool lim = g_sim.atBuildLimit(g_sim.humanPlayer, t);
            snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s%s%s", bt.name, bt.cost, bt.hotkey, bt.desc, ok ? "" : (lim ? "  Limit reached" : "  Requires "), ok || lim ? "" : BUILDS[bt.requires_].name);
            add(BK_BUILD, t, ok, bt.name, tip);
        }
        return;
    }
    bool combat = false;
    for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->weapon() >= 0) combat = true; }
    bool haul = selectionHasRole(UR_HARVESTER);
    if (combat) add(BK_ATTACKMOVE, 0, true, "Attack Move", "Attack-move: engage everything on the way  [A]");
    if (combat || haul) add(BK_AREA, 0, true, combat && haul ? "Guard/Gather" : (haul ? "Gather Area" : "Guard Area"),
                            haul && !combat ? "Gather Area: haulers search a circle you pick for supplies and collect them  [G]"
                                            : "Guard Area: pick a circle (click, or drag to size it); units protect it, aircraft patrol it  [G]");
    add(BK_STOP, 0, true, "Stop", "Stop and hold position  [S]");
}

void Game::cancelModes() {
    placingType = -1; attackMoveMode = false; powerMode = false; nukeMode = false; rallyMode = false; areaMode = false; areaDrag = false;
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
    case BK_NUKE: if (g_sim.nukesReady(g_sim.humanPlayer) > 0) { cancelModes(); nukeMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_SCAN: if (g_sim.cmdScan(g_sim.humanPlayer)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_RESEARCH: if (g_sim.cmdResearch(g_sim.humanPlayer)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_ATTACKMOVE: attackMoveMode = true; areaMode = false; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_AREA: areaMode = true; areaDrag = false; placingType = -1; attackMoveMode = false; powerMode = false; rallyMode = false; g_audio.play(SND_CLICK, Vec2(), true); break;
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
        const char* hk = kindHotkey(b.kind, b.id);
        if (hk && hk[0] == c) { clickButton(b); return; }
    }
    if (c == 'S') { g_sim.cmdStop(selection); return; }
    if (c == 'A' && !selection.empty()) { attackMoveMode = true; return; }
}

bool Game::selectionCanArea() const {
    for (auto r : selection) { const Entity* e = g_sim.get(r); if (e && e->isUnit() && ((e->ut().role == UR_COMBAT && e->weapon() >= 0) || e->ut().role == UR_HARVESTER)) return true; }
    return false;
}

void Game::applyArea(Vec2 center, float radius) {
    areaMode = false; areaDrag = false;
    if (!selectionCanArea()) return;
    bool combat = false;
    for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->ut().role == UR_COMBAT) combat = true; }
    g_sim.cmdArea(selection, center, radius);
    float r = clampf(radius, 2.0f * TILE, 14.0f * TILE);
    zoneFlashes.push_back({center, r, wallTime, !combat});
    if (zoneFlashes.size() > 4) zoneFlashes.erase(zoneFlashes.begin());
    g_audio.play(SND_ORDER, Vec2(), true, 0.7f);
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

// Setup table: your army, an optional ally, up to three enemy armies (each with its own army and difficulty), the enemy alliance, start.
enum { MR_YOU = 0, MR_ALLY, MR_E1, MR_E2, MR_E3, MR_TEAMS, MR_START, MR_ROWS };
static const int MENU_ROW_H = 34;
// the ally and every enemy take one of four seats: an ally leaves three for enemies, so the third enemy needs a free seat
static bool menuRowEnabled(const MenuSettings& m, int row) {
    if (row == MR_E3) return !m.hasAlly();
    if (row == MR_TEAMS) return m.enemyCount() >= 2;
    return true;
}
static bool menuRowHasDiff(const MenuSettings& m, int row) {
    if (row == MR_ALLY) return m.hasAlly();
    if (row >= MR_E1 && row <= MR_E3) return m.enemyFaction[row - MR_E1] != 3 && menuRowEnabled(m, row);
    return false;
}
static void menuChange(MenuSettings& m, int row, int col, int dir) {
    if (col == 1 && menuRowHasDiff(m, row)) {
        int& d = row == MR_ALLY ? m.allyDiff : m.enemyDiff[row - MR_E1];
        d = (d + dir + 4) % 4;
        return;
    }
    switch (row) {
    case MR_YOU: m.playerFaction = (Faction)((m.playerFaction + 1) % 2); break;
    case MR_ALLY:
        m.allyFaction = (m.allyFaction + dir + 4) % 4;
        if (m.hasAlly()) m.enemyFaction[2] = 3;   // four armies at most: with an ally there are two enemies at most
        break;
    case MR_E1: m.enemyFaction[0] = (m.enemyFaction[0] + dir + 3) % 3; break;
    case MR_E2: m.enemyFaction[1] = (m.enemyFaction[1] + dir + 4) % 4; break;
    case MR_E3: if (menuRowEnabled(m, row)) m.enemyFaction[2] = (m.enemyFaction[2] + dir + 4) % 4; break;
    case MR_TEAMS: m.enemiesAllied = !m.enemiesAllied; break;
    }
}
static const int MENU_ARMY_X = -150, MENU_ARMY_W = 220, MENU_DIFF_X = 90, MENU_DIFF_W = 170;   // control columns, relative to the screen centre

void Game::menuEvent(const SDL_Event& e) {
    auto change = [&](int row, int col, int dir) {
        if (row == MR_START) { seed = (u64)SDL_GetTicks() * 2654435761ull + 17; startGame(); }
        else menuChange(menu, row, col, dir);
        g_audio.play(SND_CLICK, Vec2(), true);
    };
    auto fixFocus = [&]() { if (!menuRowHasDiff(menu, menu.cursor)) menu.col = 0; };
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_UP || k == SDLK_DOWN) {
            int d = k == SDLK_UP ? -1 : 1;
            do { menu.cursor = (menu.cursor + d + MR_ROWS) % MR_ROWS; } while (!menuRowEnabled(menu, menu.cursor));
        }
        else if (k == SDLK_TAB) { if (menuRowHasDiff(menu, menu.cursor)) menu.col = 1 - menu.col; }
        else if (k == SDLK_LEFT) change(menu.cursor, menu.col, -1);
        else if (k == SDLK_RIGHT || k == SDLK_SPACE || k == SDLK_RETURN) change(menu.cursor, menu.col, 1);
        else if (k == SDLK_ESCAPE) quitRequested = true;
        fixFocus();
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int x = e.button.x, y = e.button.y;
        int oy = menuOffsetY(), cx = SCREEN_W / 2;
        int rowY0 = 250 + oy;
        int row = (y - rowY0) / MENU_ROW_H;
        if (y >= rowY0 && row >= 0 && row < MR_START && menuRowEnabled(menu, row)) {
            menu.cursor = row;
            int col = -1, dir = 0;
            if (x >= cx + MENU_ARMY_X && x < cx + MENU_ARMY_X + MENU_ARMY_W) { col = 0; dir = x < cx + MENU_ARMY_X + MENU_ARMY_W / 2 ? -1 : 1; }
            else if (x >= cx + MENU_DIFF_X && x < cx + MENU_DIFF_X + MENU_DIFF_W && menuRowHasDiff(menu, row)) { col = 1; dir = x < cx + MENU_DIFF_X + MENU_DIFF_W / 2 ? -1 : 1; }
            if (col >= 0) { menu.col = col; change(row, col, dir); }
            fixFocus();
        }
        if (y >= 520 + oy && y < 560 + oy && x >= cx - 100 && x < cx + 100) { menu.cursor = MR_START; change(MR_START, 0, 1); }
        if (y >= 572 + oy && y < 604 + oy && x >= cx - 50 && x < cx + 50) quitRequested = true;
    }
}

void Game::gameEvent(const SDL_Event& e) {
    if (menuOpen) { pauseMenuEvent(e); return; }
    Vec2 w = screenToWorld(mouseX, mouseY);
    bool overHud = mouseY >= VIEW_H;
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        u16 mod = e.key.keysym.mod;
        if (k == SDLK_ESCAPE) {
            if (placingType >= 0 || attackMoveMode || powerMode || nukeMode || rallyMode || areaMode) cancelModes();
            else if (showHelp) showHelp = false;
            else openPauseMenu();
            return;
        }
        if (k == SDLK_SPACE || k == SDLK_PAUSE) { paused = !paused; return; }
        if (k == SDLK_F1) { showHelp = !showHelp; return; }
        if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) { stepSpeed(1); return; }
        if (k == SDLK_MINUS || k == SDLK_KP_MINUS) { stepSpeed(-1); return; }
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
                if (areaMode) applyArea(wp, AREA_DEFAULT_R);
                else if (attackMoveMode && !selection.empty()) { g_sim.cmdMove(selection, wp, true); attackMoveMode = false; }
                else if (powerMode) { if (g_sim.cmdPower(g_sim.humanPlayer, wp)) powerMode = false; }
                else if (nukeMode) { if (g_sim.cmdNuke(g_sim.humanPlayer, wp)) { nukeMode = false; addMessage("Nuclear missile launched", rgb(255, 120, 90)); } }
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
            if (nukeMode) { if (g_sim.cmdNuke(g_sim.humanPlayer, w)) { nukeMode = false; addMessage("Nuclear missile launched", rgb(255, 120, 90)); } else { nukeMode = false; g_audio.play(SND_CANT, Vec2(), true); } return; }
            if (attackMoveMode) { issueAttackMove(w); attackMoveMode = false; return; }
            if (rallyMode) { Entity* b = selectedBuilding(); if (b) g_sim.cmdSetRally(g_sim.refOf(*b), w); rallyMode = false; return; }
            if (areaMode) { areaDrag = true; areaStart = w; areaRadius = 0; return; }
            dragging = true; dragStart = w; dragNow = w;
            return;
        }
        if (btn == SDL_BUTTON_RIGHT) {
            if (placingType >= 0 || attackMoveMode || powerMode || nukeMode || rallyMode || areaMode) { cancelModes(); return; }
            issueRightClick(w);
            return;
        }
    }
    if (e.type == SDL_MOUSEMOTION) {
        if (midDrag) { cam -= Vec2(e.motion.xrel, e.motion.yrel); cam.x = clampf(cam.x, 0, WORLD_W - SCREEN_W); cam.y = clampf(cam.y, 0, WORLD_H - VIEW_H); }
        if (dragging) dragNow = screenToWorld(mouseX, mouseY);
        if (areaDrag) areaRadius = dist(areaStart, screenToWorld(mouseX, mouseY));
    }
    if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_MIDDLE) midDrag = false;
        if (e.button.button == SDL_BUTTON_LEFT && areaDrag) {
            float r = areaRadius >= 24 ? areaRadius : AREA_DEFAULT_R;
            applyArea(areaStart, r);
        }
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
    case GS_PLAYING: renderWorld(); renderHud(); if (menuOpen) renderPauseMenu(); break;
    case GS_GAMEOVER: renderWorld(); renderHud(); renderGameOver(); break;
    }
}

void Game::renderMenu() {
    Gfx& g = g_gfx;
    g.beginFrame(rgb(18, 22, 28));
    // backdrop: a slice of the baked map
    {
        int sw = std::min(SCREEN_W, WORLD_W - 400), sh = std::min(SCREEN_H, WORLD_H - 640);
        SDL_Rect src = { 400, 640, sw, sh }, dst = { 0, 0, sw, sh };
        SDL_RenderCopy(g.ren, g.worldTerrain.tex, &src, &dst);
    }
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(10, 14, 20, 170));
    Color accent = hudAccent(menu.playerFaction);
    int cx = SCREEN_W / 2, oy = menuOffsetY();
    g.fill(0, 110 + oy, SCREEN_W, 4, accent);
    g.text(cx - g.textW("ONE HOUR", 6) / 2, 40 + oy, "ONE HOUR", rgb(240, 244, 248), 6);
    g.text(cx - g.textW("Skirmish: Cyber Army vs Clanker Army", 2) / 2, 130 + oy, "Skirmish: Cyber Army vs Clanker Army", hudDim(), 2);
    const char* fdesc = menu.playerFaction == F_CYBER
        ? "Lasers, railguns, volt coils, EMP Strike, Orbital Scan, Ion Lancers, Aegis Titans and Specter Jets."
        : "Diesel tanks, gatling guns, Shell Storm, Recon Flight, Grenadiers, Behemoths and Talon Jets.";
    g.text(cx - g.textW(fdesc) / 2, 170 + oy, fdesc, hudText());
    g.text(cx - g.textW("One map: Lakeside, 4 corner bases, contested supply piles in the middle") / 2, 190 + oy, "One map: Lakeside, 4 corner bases, contested supply piles in the middle", hudDim());

    int rowY0 = 250 + oy;
    // seat colours as they will appear in the match: you blue, an ally green, enemies red then yellow (then green when there is no ally)
    auto seatColor = [&](int row) -> Color {
        if (row == MR_YOU) return PLAYER_COLOR[0];
        if (row == MR_ALLY) return PLAYER_COLOR[3];
        int idx = 0; for (int i = 0; i < row - MR_E1; i++) if (menu.enemyFaction[i] != 3) idx++;
        return PLAYER_COLOR[1 + idx];
    };
    static const char* FN[4] = { "Cyber Army", "Clanker Army", "Random", "-" };
    static const char* AN[4] = { "Cyber Army", "Clanker Army", "Random", "None" };
    static const char* EN[4] = { "Cyber Army", "Clanker Army", "Random", "Off" };
    (void)FN;
    // a bracketed control: dim when unfocused, accent arrows and a lit frame when focused
    auto control = [&](int x0, int w, int y, const char* value, bool focused, bool rowCur, Color tint) {
        if (focused) { g.fill(x0 - 2, y - 4, w + 4, MENU_ROW_H - 4, rgb(255, 255, 255, 26)); g.box(x0 - 2, y - 4, w + 4, MENU_ROW_H - 4, Color{accent.r, accent.g, accent.b, 200}); }
        g.text(x0 + 6, y + 4, "<", focused ? accent : hudDim(), 2);
        g.text(x0 + w / 2 - g.textW(value, 2) / 2, y + 4, value, focused ? rgb(255, 255, 255) : (rowCur ? hudText() : tint), 2);
        g.text(x0 + w - 18, y + 4, ">", focused ? accent : hudDim(), 2);
    };
    g.text(cx + MENU_ARMY_X + MENU_ARMY_W / 2 - g.textW("ARMY") / 2, rowY0 - 17, "ARMY", hudDim());
    g.text(cx + MENU_DIFF_X + MENU_DIFF_W / 2 - g.textW("DIFFICULTY") / 2, rowY0 - 17, "DIFFICULTY", hudDim());
    char buf[32];
    for (int i = 0; i < MR_START; i++) {
        int y = rowY0 + i * MENU_ROW_H;
        bool en = menuRowEnabled(menu, i), cur = menu.cursor == i;
        if (cur) g.fill(cx - 260, y - 4, 520, MENU_ROW_H - 4, rgb(255, 255, 255, 14));
        const char* label = i == MR_YOU ? "You" : (i == MR_ALLY ? "Ally" : (i == MR_TEAMS ? "Teams" : nullptr));
        if (!label) { snprintf(buf, sizeof buf, "Enemy %d", i - MR_E1 + 1); label = buf; }
        if (i != MR_TEAMS) { Color sc = seatColor(i); bool seated = i == MR_YOU || (i == MR_ALLY ? menu.hasAlly() : (en && menu.enemyFaction[i - MR_E1] != 3)); g.fillCircle(cx - 246.0f, (float)(y + 10), 5.0f, seated ? sc : rgb(70, 74, 82)); }
        g.text(cx - 236, y + 4, label, en ? hudText() : hudDim(), 2);
        if (!en) { g.text(cx + MENU_ARMY_X + MENU_ARMY_W / 2 - 6, y + 4, "-", hudDim(), 2); continue; }
        if (i == MR_TEAMS) { control(cx + MENU_ARMY_X, MENU_ARMY_W, y, menu.enemiesAllied ? "Enemies allied" : "Free for all", cur, cur, hudText()); continue; }
        const char* army = i == MR_YOU ? FACTION_NAME[menu.playerFaction] : (i == MR_ALLY ? AN[menu.allyFaction] : EN[menu.enemyFaction[i - MR_E1]]);
        control(cx + MENU_ARMY_X, MENU_ARMY_W, y, army, cur && menu.col == 0, cur, hudText());
        if (i == MR_YOU) { g.text(cx + MENU_DIFF_X + MENU_DIFF_W / 2 - g.textW("(you)", 2) / 2, y + 4, "(you)", hudDim(), 2); continue; }
        if (menuRowHasDiff(menu, i)) control(cx + MENU_DIFF_X, MENU_DIFF_W, y, DIFF_NAME[i == MR_ALLY ? menu.allyDiff : menu.enemyDiff[i - MR_E1]], cur && menu.col == 1, cur, hudText());
        else g.text(cx + MENU_DIFF_X + MENU_DIFF_W / 2 - 6, y + 4, "-", hudDim(), 2);
    }
    {
        const char* note = menu.hasAlly() ? "Your ally fights on your side, shares its vision with you, and plays at the difficulty set for it. Four armies at most." : "Add an ally to fight beside you. Each army has its own difficulty.";
        g.text(cx - g.textW(note) / 2, rowY0 + MR_START * MENU_ROW_H - 2, note, hudDim());
    }
    // start / quit
    bool cur = menu.cursor == MR_START;
    g.bevelPanel(cx - 100, 520 + oy, 200, 40, cur ? shade(accent, 0.55f) : rgb(50, 58, 70));
    g.text(cx - g.textW("START", 3) / 2, 528 + oy, "START", rgb(255, 255, 255), 3);
    g.bevelPanel(cx - 50, 572 + oy, 100, 32, rgb(50, 58, 70));
    g.text(cx - g.textW("Quit", 2) / 2, 580 + oy, "Quit", hudText(), 2);
    g.text(cx - g.textW("Up/Down row   Left/Right change   Tab army / difficulty   Enter start   Esc quit") / 2, 616 + oy, "Up/Down row   Left/Right change   Tab army / difficulty   Enter start   Esc quit", hudDim());
}

void Game::drawShroud() {
    Gfx& g = g_gfx;
    if (g_sim.revealed(g_sim.humanPlayer)) {
        // scan running: no shroud, a faint tint and a sweeping scan line instead
        Color ac = hudAccent(g_sim.players[g_sim.humanPlayer].faction);
        float left = g_sim.players[g_sim.humanPlayer].revealUntil - g_sim.time;
        u8 a = (u8)(left < 5.0f ? 10 + (int)(14 * (0.5f + 0.5f * std::sin(wallTime * 12.0f))) : 18);
        g.fill(0, 0, SCREEN_W, VIEW_H, Color{ac.r, ac.g, ac.b, a});
        float sweep = std::fmod(wallTime * 0.45f, 1.0f) * VIEW_H;
        g.fill(0, (int)sweep, SCREEN_W, 2, Color{ac.r, ac.g, ac.b, 70});
        g.fill(0, (int)sweep - 6, SCREEN_W, 6, Color{ac.r, ac.g, ac.b, 18});
        return;
    }
    static u32 lastStamp = 0; static const void* lastPtr = nullptr;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    // refresh the mask when the exploration changed (cheap hash of the array)
    u32 h = 2166136261u; for (size_t i = 0; i < ex.size(); i += 3) h = (h ^ ex[i]) * 16777619u;
    static float nextShroud = 0;   // the cloudy edge costs a few ms to rebuild: refresh at most about 8 times a second while units uncover ground
    if ((h != lastStamp && wallTime >= nextShroud) || lastPtr != ex.data()) { g.updateShroud(ex); lastStamp = h; lastPtr = ex.data(); nextShroud = wallTime + 0.12f; }
    int tx0 = std::max(0, tileOf(cam.x)), ty0 = std::max(0, tileOf(cam.y));
    int tx1 = std::min(MAP_W - 1, tileOf(cam.x + SCREEN_W)), ty1 = std::min(MAP_H - 1, tileOf(cam.y + VIEW_H));
    const int SS = Gfx::SHROUD_SS;
    SDL_Rect src = { tx0 * SS, ty0 * SS, (tx1 - tx0 + 1) * SS, (ty1 - ty0 + 1) * SS };
    SDL_FRect dst = { tx0 * TILE - cam.x, ty0 * TILE - cam.y, (float)(tx1 - tx0 + 1) * TILE, (float)(ty1 - ty0 + 1) * TILE };
    SDL_RenderCopyF(g.ren, g.shroudTex, &src, &dst);
}


// Circle a group is assigned to: translucent disc, marching dashes, and what it is for.
static void zoneCircle(Gfx& g, Vec2 sc, float r, bool gather, float phase, float alpha, const char* label) {
    Color col = gather ? rgb(245, 205, 90) : rgb(110, 255, 170);
    Color f = col; f.a = (u8)(30 * alpha);
    g.discFill(sc.x, sc.y, r, f);
    Color ring = col; ring.a = (u8)(230 * alpha);
    g.dashedCircle(sc.x, sc.y, r, ring, phase);
    g.dashedCircle(sc.x, sc.y, r - 1.2f, Color{ring.r, ring.g, ring.b, (u8)(ring.a / 2)}, phase);
    g.line(sc.x - 5, sc.y, sc.x + 5, sc.y, ring); g.line(sc.x, sc.y - 5, sc.x, sc.y + 5, ring);
    if (label) g.text((int)(sc.x - g.textW(label) / 2), (int)std::max(24.0f, sc.y - r - 12), label, Color{ring.r, ring.g, ring.b, (u8)(255 * alpha)});
}

void Game::drawZones() {
    Gfx& g = g_gfx;
    float phase = wallTime * 22.0f;
    // the assignments of everything selected (one circle per distinct assignment)
    struct Z { Vec2 c; float r; bool gather; };
    std::vector<Z> seen;
    for (auto ref : selection) {
        Entity* e = g_sim.get(ref);
        if (!e || !e->isUnit() || e->zoneR <= 0) continue;
        bool gather = e->ut().role == UR_HARVESTER;
        bool dup = false;
        for (auto& z : seen) if (dist(z.c, e->zone) < 4 && std::abs(z.r - e->zoneR) < 4 && z.gather == gather) dup = true;
        Vec2 sc = worldToScreen(e->zone);
        if (!dup) { seen.push_back({e->zone, e->zoneR, gather}); zoneCircle(g, sc, e->zoneR, gather, phase, 1.0f, gather ? "GATHER" : "GUARD"); }
        // a thin tether from a unit that is still on its way
        Vec2 up = worldToScreen(entPos(*e));
        if (selection.size() <= 8 && dist(e->pos, e->zone) > e->zoneR + 30) g.line(up.x, up.y, sc.x, sc.y, Color{200, 255, 220, 40});
    }
    // confirmation pulse right after giving the order
    for (size_t i = 0; i < zoneFlashes.size();) {
        float age = wallTime - zoneFlashes[i].time;
        if (age > 0.9f) { zoneFlashes.erase(zoneFlashes.begin() + i); continue; }
        Vec2 sc = worldToScreen(zoneFlashes[i].pos);
        float k = age / 0.9f;
        Color col = zoneFlashes[i].gather ? rgb(245, 205, 90) : rgb(110, 255, 170);
        col.a = (u8)(200 * (1 - k));
        g.dashedCircle(sc.x, sc.y, zoneFlashes[i].r * (0.6f + 0.4f * k), col, 0, 40, 0);
        i++;
    }
    // choosing a circle: follows the cursor, or grows while dragging
    if (areaMode && mouseY < VIEW_H) {
        bool haul = selectionHasRole(UR_HARVESTER), combat = false;
        for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->ut().role == UR_COMBAT) combat = true; }
        Vec2 c = areaDrag ? worldToScreen(areaStart) : Vec2(mouseX, mouseY);
        float r = areaDrag && areaRadius >= 24 ? areaRadius : AREA_DEFAULT_R;
        r = clampf(r, 2.0f * TILE, 14.0f * TILE);
        zoneCircle(g, c, r, haul && !combat, phase, 0.9f, nullptr);
        const char* hint = haul && !combat ? "GATHER AREA: click, or drag to size" : "GUARD AREA: click, or drag to size";
        g.text((int)(c.x - g.textW(hint) / 2), (int)(c.y - r - 12), hint, haul && !combat ? rgb(245, 205, 90) : rgb(110, 255, 170));
    }
}

// Range of one weapon around a point: what a defense (or a selected unit) can hit.
void Game::rangeRing(Vec2 sp, float radiusPx, int weapon, const char* label, bool powered, bool enemy, bool faint) {
    Gfx& g = g_gfx;
    const Weapon& w = WEAPONS[weapon];
    Color col = !powered ? rgb(150, 150, 160) : enemy ? rgb(255, 90, 80) : (w.ground && w.air) ? rgb(120, 225, 255) : (w.ground ? rgb(255, 190, 90) : rgb(150, 175, 255));
    float a = faint ? 0.55f : 1.0f;
    Color f = col; f.a = (u8)((faint ? 10 : 24) * (powered ? 1.0f : 0.6f));
    g.discFill(sp.x, sp.y, radiusPx, f);
    Color ring = col; ring.a = (u8)(200 * a);
    if (powered) g.circle(sp.x, sp.y, radiusPx, ring, 96);
    else g.dashedCircle(sp.x, sp.y, radiusPx, ring, wallTime * 10.0f, 6, 6);
    g.circle(sp.x, sp.y, radiusPx - 1.0f, Color{col.r, col.g, col.b, (u8)(ring.a / 3)}, 96);
    if (w.minRange > 0) g.dashedCircle(sp.x, sp.y, w.minRange * TILE, Color{col.r, col.g, col.b, (u8)(150 * a)}, 0, 4, 4);
    if (!faint && label) {
        char buf[96]; snprintf(buf, sizeof buf, "%s  %s  range %.1f", label, powered ? (w.ground && w.air ? "ground + air" : (w.ground ? "ground" : "air only")) : "NO POWER", w.range);
        float ly = sp.y - radiusPx - 12;
        if (ly < 24) ly = std::min(sp.y + radiusPx + 4, (float)VIEW_H - 14);   // ring runs off the top: label below it
        g.text((int)clampf(sp.x - g.textW(buf) / 2, 4, SCREEN_W - g.textW(buf) - 4), (int)ly, buf, col);
    }
}

void Game::drawRangeRings() {
    Player& pl = g_sim.players[g_sim.humanPlayer];
    int fewUnits = 0;
    for (auto ref : selection) { Entity* e = g_sim.get(ref); if (e && e->isUnit() && e->weapon() >= 0) fewUnits++; }
    for (auto ref : selection) {
        Entity* e = g_sim.get(ref);
        if (!e) continue;
        Vec2 sp = worldToScreen(entPos(*e));
        if (e->isBuilding() && e->constructed && e->bt().weapon >= 0) {
            const BuildType& bt = e->bt();
            bool powered = !(pl.lowPower() && bt.power < 0) && e->disabledUntil <= g_sim.time;
            rangeRing(sp, WEAPONS[bt.weapon].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, bt.weapon, bt.name, powered, false, false);
        } else if (e->isUnit() && e->weapon() >= 0 && fewUnits <= 3) {
            rangeRing(sp, WEAPONS[e->weapon()].range * TILE + e->radius(), e->weapon(), nullptr, true, false, true);
        }
    }
    // a defense under the cursor: show what it covers (enemy ones in red)
    if (placingType < 0 && !areaMode && mouseY < VIEW_H) {
        Entity* h = pickEntity(screenToWorld(mouseX, mouseY), false);
        if (h && h->isBuilding() && h->constructed && h->bt().weapon >= 0 && std::find(selection.begin(), selection.end(), g_sim.refOf(*h)) == selection.end()) {
            bool mine = !g_sim.enemies(g_sim.humanPlayer, h->owner);   // yours or an ally's: green; an enemy's: red
            const BuildType& bt = h->bt();
            rangeRing(worldToScreen(h->pos), WEAPONS[bt.weapon].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, bt.weapon, bt.name, !(mine && h->owner >= 0 && g_sim.players[h->owner].lowPower() && bt.power < 0), !mine, false);
        }
    }
}

// draw a sprite scaled to fit a square box, centred on the point its anchor (the structure's footprint centre) would map to
static void drawFitted(Gfx& g, const Sprite& s, float cx, float cy, float box, const Sprite* ref = nullptr, Color mod = rgb(255, 255, 255)) {
    const Sprite& r = ref ? *ref : s;
    float d = r.dscale, w = r.w * d, h = r.h * d, sc = std::min(box / w, box / h);
    g.draw(s, cx + (r.ox - r.w * 0.5f) * d * sc, cy + (r.oy - r.h * 0.5f) * d * sc, 0, sc, mod);
}

// Live details over a finished structure: pulsing reactor cores, turning dishes, a pumping jack, blinking beacons.
void Game::drawBuildingAnim(const Entity& e, Vec2 p, bool disabled) {
    Gfx& g = g_gfx;
    const BuildType& bt = e.bt();
    bool cy = bt.faction == F_CYBER;
    if (disabled || paused) { }
    float t = wallTime + (float)e.gen * 0.37f;
    bool powered = !(g_sim.players[e.owner < 0 ? 0 : e.owner].lowPower() && bt.power < 0);
    float pulse = powered ? 0.55f + 0.45f * std::sin(t * 3.0f) : 0.12f;
    switch (bt.role) {
    case BR_HQ:
        if (cy) g.glowAdd(p.x, p.y - 17, 22, Color{110, 232, 255, (u8)(70 + 80 * pulse)});
        else { g.draw(g.prop[1], p.x - 39, p.y - 13, disabled ? 0.3f : t * 1.7f, 0.95f); }
        break;
    case BR_POWER:
        if (cy) { g.glowAdd(p.x - 22, p.y + 7, 20, Color{110, 232, 255, (u8)(60 + 100 * pulse)}); g.glowAdd(p.x + 22, p.y + 7, 20, Color{110, 232, 255, (u8)(60 + 100 * (1 - pulse))}); }
        else { g.glowAdd(p.x + 28, p.y + 2, 9, Color{255, 120, 40, (u8)(40 + 50 * pulse)}); g.glowAdd(p.x + 39, p.y + 2, 9, Color{255, 120, 40, (u8)(40 + 50 * (1 - pulse))}); }
        break;
    case BR_TECH:
        if (cy) g.draw(g.prop[0], p.x + 22, p.y - 36, disabled ? 0.6f : t * 1.1f, 0.78f);
        break;
    case BR_SUPPLY:
        if (cy) g.glowAdd(p.x - 24, p.y + 36, 14, Color{110, 232, 255, (u8)(40 + 90 * pulse)});
        break;
    case BR_INCOME:
        if (cy) g.glowAdd(p.x, p.y - 22, 26, Color{255, 214, 96, (u8)(40 + 90 * pulse)});
        else g.draw(g.prop[2], p.x, p.y + 6, powered ? std::sin(t * 2.4f) * 0.26f : 0, 1.15f);
        break;
    case BR_NUKE:
        if (g_sim.time >= e.actionTimer && powered && g.armed[e.type].tex) g.draw(g.armed[e.type], p.x, p.y, 0, 1, disabled ? rgb(120, 140, 170) : rgb(255, 255, 255));
        if (cy) { Color rc = Color{255, 80, 70, (u8)((std::sin(t * 5.0f) > 0 && powered) ? 190 : 40)}; g.glowAdd(p.x - 37, p.y - 51, 9, rc); g.glowAdd(p.x + 37, p.y - 51, 9, rc); }
        else g.glowAdd(p.x - 24, p.y + 26, 18, Color{255, 120, 50, (u8)(30 + 50 * (0.5f + 0.5f * std::sin(t * 7.0f)))});
        break;
    default: break;
    }
}

// Soot, flames and a smoke column on a hurt structure.
void Game::drawBuildingDamage(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    const BuildType& bt = e.bt();
    float f = e.hp / e.maxHp, fw = (float)bt.w * TILE, fh = (float)bt.h * TILE;
    Rng r((u64)e.gen * 7919 + (u64)e.type * 131 + 1);
    int soot = 2 + bt.w * bt.h / 3;
    for (int i = 0; i < soot; i++) {
        float x = p.x + r.f(-0.42f, 0.42f) * fw, y = p.y + r.f(-0.5f, 0.3f) * fh;
        g.fillCircle(x, y, r.f(7, 14) * (1.3f - f), rgb(10, 8, 6, (int)(120 * (0.66f - f) / 0.66f + 20)));
    }
    if (f < 0.5f) {
        int n = 1 + (int)((0.5f - f) * 2 * (1 + bt.w * bt.h / 4));
        for (int i = 0; i < n; i++) {
            float x = p.x + r.f(-0.4f, 0.4f) * fw, y = p.y + r.f(-0.45f, 0.3f) * fh;
            float fl = 0.7f + 0.3f * std::sin(wallTime * 13.0f + i * 2.1f + e.gen);
            int fr = ((int)(wallTime * 13) + i * 2 + (int)e.gen) % 6;
            float sc = (0.7f + 0.5f * (0.5f - f) * 2) * (0.8f + 0.2f * fl);
            g.glowAdd(x, y - 6 * sc, 18 * sc, Color{255, 120, 40, (u8)(110 * fl)});
            g.draw(g.fxs.flame[fr], x, y + 4, 0, sc, rgb(255, 255, 255), (u8)(230));
            if (frameDt > 0 && fxRng.f() < (2.2f + (0.5f - f) * 4) * frameDt)
                emitP(e.pos.x + (x - p.x), e.pos.y + (y - p.y) - 6, fxRng.f(-6, 6), fxRng.f(-34, -18), fxRng.f(1.8f, 3.0f), 6 + 8 * sc, 16 + 14 * sc, rgb(44, 42, 40, 230), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), fxRng.f(-0.3f, 0.3f));
        }
    } else if (frameDt > 0 && fxRng.f() < 1.2f * frameDt) {
        emitP(e.pos.x + r.f(-0.35f, 0.35f) * fw, e.pos.y + r.f(-0.4f, 0.2f) * fh, fxRng.f(-4, 4), fxRng.f(-20, -10), fxRng.f(1.5f, 2.4f), 5, 12, rgb(80, 76, 72, 150), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.2f);
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
        const Sprite& s = g.building[e.type];
        float fw = (float)bt.w * TILE, fh = (float)bt.h * TILE;           // footprint
        float fx = p.x - fw * 0.5f, fy = p.y - fh * 0.5f;
        Color tint = disabled ? rgb(120, 140, 170) : rgb(255, 255, 255);
        if (!e.constructed) {
            g.draw(g.site[e.type], p.x, p.y);
            // the finished structure rises out of the site from the ground up as construction progresses, with a work light along the cut
            float prog = clampf(e.progress, 0, 1);
            int rows = (int)(s.h * prog);
            if (rows > 0) {
                float d = s.dscale;
                SDL_Rect src = { 0, s.h - rows, s.w, rows };
                SDL_FRect dst = { p.x - s.ox * d, p.y - s.oy * d + (s.h - rows) * d, s.w * d, rows * d };
                SDL_SetTextureColorMod(s.tex, 255, 255, 255); SDL_SetTextureAlphaMod(s.tex, 255);
                SDL_RenderCopyF(g.ren, s.tex, &src, &dst);
                Color wl = hudAccent(bt.faction);
                g.fill((int)(fx - 2), (int)dst.y, (int)(fw + 4), 1, rgb(wl.r, wl.g, wl.b, 200));
                g.glowAdd(fx + std::fmod(wallTime * 40.0f + e.gen * 17.0f, fw), dst.y, 10, Color{wl.r, wl.g, wl.b, 150});
            }
        } else {
            g.draw(s, p.x, p.y, 0, 1, tint);
            if (g.buildingTeam[e.type].tex) { Color tc = playerColor(owner); if (disabled) tc = mix(tc, rgb(120, 140, 170), 0.5f); g.draw(g.buildingTeam[e.type], p.x, p.y, 0, 1, tc); }
            drawBuildingAnim(e, p, disabled);
            // rotating heads for defenses
            if (bt.role == BR_TURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 0 : 1], p.x, p.y, e.angle, 1, tint);
            else if (bt.role == BR_AATURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 2 : 3], p.x, p.y, e.angle, 1, tint);
            // production activity light
            if (!e.queue.empty() && ((int)(wallTime * 3) & 1)) { Color ac = hudAccent(bt.faction); g.glowAdd(fx + fw - 7, fy + 2, 7, Color{ac.r, ac.g, ac.b, 160}); g.fillCircle(fx + fw - 7, fy + 2, 2.4f, ac); }
        }
        // damage: soot, then flames and smoke columns once the structure is badly hurt (drawn live, cheap)
        if (e.constructed && e.hp < e.maxHp * 0.66f && !disabled) drawBuildingDamage(e, p);
        if (selected) {
            Color sc = rgb(255, 255, 255, 230); float L = 9;
            for (int cx = 0; cx < 2; cx++) for (int cy = 0; cy < 2; cy++) {   // corner brackets
                float x = cx ? fx + fw + 1 : fx - 2, y = cy ? fy + fh + 1 : fy - 2, dx = cx ? -L : L, dy = cy ? -L : L;
                g.fill((int)std::min(x, x + dx), (int)y, (int)L, 1, sc); g.fill((int)x, (int)std::min(y, y + dy), 1, (int)L, sc);
            }
            if (e.hasRally && e.constructed) { Vec2 r = worldToScreen(e.rally); g.line(p.x, p.y, r.x, r.y, rgb(120, 255, 120, 160)); g.circle(r.x, r.y, 5, rgb(120, 255, 120)); }
        }
        if (selected || e.hp < e.maxHp || !e.constructed) hpBar(g, fx, fy - 8, fw, e.constructed ? e.hp / e.maxHp : e.progress, true);
        if (disabled) g.text((int)p.x - 9, (int)p.y - 4, "EMP", rgb(160, 220, 255));
        return;
    }
    const UnitType& ut = e.ut();
    const Sprite& body = g.unitBody[e.type][slotOf(owner)];
    bool air = ut.kind == UK_AIR;
    float alt = air ? e.alt : 0.0f;
    float spd = (e.pos - e.prevPos).len() / SIM_DT;            // px/s over the last tick
    Color mod = disabled ? rgb(120, 140, 170) : rgb(255, 255, 255);
    if (air) {
        // shadow: the aircraft's own silhouette, thrown to the south-east and sliding closer as it settles onto the pad
        g.draw(body, p.x + 3 + 15 * alt, p.y + 4 + 27 * alt, e.angle, 1, rgb(0, 0, 0), (u8)(70 + 40 * (1 - alt)));
        p.y -= 14 * alt;   // altitude
    } else {
        g.draw(ut.kind == UK_INF ? g.shadowSmall : g.shadowLarge, p.x, p.y + 3, 0, ut.kind == UK_INF ? 0.55f : 0.75f);
        if (ut.kind == UK_VEH) g.draw(body, p.x + 2, p.y + 3, e.angle, 1, rgb(0, 0, 0), 55);   // contact shadow in the hull's own shape
    }
    if (selected) {   // a soft ring on the ground under the unit (not under its altitude), squashed a little for the viewing angle
        float rr = (e.radius() + 5) * 2.0f, gy = p.y + (air ? 14 * alt : 0) + 2;
        g.drawSized(g.fxs.ring, p.x, gy, rr, rr * 0.8f, 0, rgb(150, 255, 170), 235);
        g.drawSized(g.fxs.ring, p.x, gy, rr * 1.1f, rr * 0.88f, 0, rgb(150, 255, 170), 70);
    }
    // walk / track cycle: the two extra frames alternate while the unit is actually moving
    const Sprite* bs = &body;
    bool moving = (e.pos - e.prevPos).len2() > 0.01f;
    if (!air && !paused && !disabled && moving) {
        float travelled = ut.kind == UK_INF ? wallTime * 9.0f + e.gen * 0.7f : (e.pos.x + e.pos.y) / 3.0f;
        bs = &g.unitAnim[e.type][slotOf(owner)][((int)std::floor(travelled)) & 1];
    }
    // vehicles kick up dust: a puff behind the tracks now and then, grey on asphalt and tan on bare ground
    if (!air && ut.kind == UK_VEH && moving && frameDt > 0 && fxRng.f() < 11.0f * frameDt) {
        Vec2 back = Vec2(std::cos(e.angle), std::sin(e.angle)) * -(e.radius() * 0.85f);
        bool road = g_map.tile(clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) == T_ROAD;
        float sz = e.radius() * 0.42f;
        emitP(e.pos.x + back.x + fxRng.f(-3, 3), e.pos.y + back.y + fxRng.f(-3, 3), back.norm().x * 14 + fxRng.f(-6, 6), back.norm().y * 14 + fxRng.f(-6, 6), fxRng.f(0.6f, 1.0f), sz, sz * 2.6f,
              road ? rgb(150, 150, 152, 150) : rgb(178, 150, 108, 170), PK_DUST, (u8)fxRng.range(0, 2), 0, 1.4f, fxRng.f(0, 6), fxRng.f(-0.6f, 0.6f));
    }
    // jets: afterburner flame, glow and a vapour trail while they are moving fast
    if (air && ut.jet && spd > 120 && !disabled) {
        float burn = clampf((spd - 120) / 360.0f, 0, 1), th = e.angle;
        Vec2 tail = Vec2(p.x - std::cos(th) * 25.0f * 0.62f * 1.28f, p.y - std::sin(th) * 25.0f * 0.62f * 1.28f);
        int fr = ((int)(wallTime * 24) + (int)e.gen) % 6;
        bool cyj = ut.faction == F_CYBER;
        Color fc = cyj ? rgb(140, 230, 255) : rgb(255, 190, 110);
        g.draw(g.fxs.flame[fr], tail.x, tail.y, th - 1.5708f, 0.55f + 1.0f * burn, fc, (u8)(150 + 105 * burn));
        g.glowAdd(tail.x, tail.y, 9 + 9 * burn, Color{fc.r, fc.g, fc.b, (u8)(120 * burn + 40)});
        if (frameDt > 0 && burn > 0.45f && fxRng.f() < 40.0f * frameDt) {
            float wy = alt > 0.5f ? 1.0f : 0.0f; (void)wy;
            emitP(e.pos.x - std::cos(th) * 18, e.pos.y - std::sin(th) * 18 - 14 * alt, 0, 0, fxRng.f(1.1f, 1.7f), 2.4f, 7.5f, rgb(238, 244, 248, 150), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), 0.1f);
        }
    }
    g.draw(*bs, p.x, p.y, e.angle, 1, mod);
    const Sprite& tur = g.unitTurret[e.type][slotOf(owner)];
    if (tur.tex) {
        // the barrel kicks back for a moment after every shot
        float rec = 0;
        if (ut.weapon >= 0 && !paused) { const Weapon& w = WEAPONS[ut.weapon]; float since = w.cooldown - e.cooldown; if (w.cooldown > 0.6f && since >= 0 && since < 0.15f) rec = 1.0f - since / 0.15f; }
        g.draw(tur, p.x - std::cos(e.turret) * 3.2f * rec, p.y - std::sin(e.turret) * 3.2f * rec, e.turret, 1, mod);
    }
    // a badly hurt vehicle or aircraft trails smoke, and burns when nearly dead
    if (ut.kind != UK_INF && e.hp < e.maxHp * 0.4f && !disabled) {
        float hpf = e.hp / e.maxHp;
        if (frameDt > 0 && fxRng.f() < (7.0f + (0.4f - hpf) * 18.0f) * frameDt)
            emitP(e.pos.x + fxRng.f(-4, 4), e.pos.y - 14 * alt + fxRng.f(-4, 4), fxRng.f(-8, 8), fxRng.f(-26, -12), fxRng.f(0.9f, 1.6f), 3.5f, 9.0f, hpf < 0.2f ? rgb(36, 34, 32, 230) : rgb(110, 106, 100, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.8f, fxRng.f(0, 6), 0.3f);
        if (hpf < 0.2f) {
            int fr = ((int)(wallTime * 13) + (int)e.gen) % 6;
            g.glowAdd(p.x, p.y - 2, 13, Color{255, 120, 40, (u8)(90 + 40 * std::sin(wallTime * 11 + e.gen))});
            g.draw(g.fxs.flame[fr], p.x + 1, p.y + 3, 0, 0.5f, rgb(255, 255, 255), 230);
        }
    }
    if (ut.kind == UK_AIR && ut.faction == F_CLANKER && !ut.jet) {   // main rotor: motion-blur disc plus a spinning blade pair
        float spin = paused || disabled ? 0.4f : wallTime * 38.0f;
        g.draw(g.rotorDisc, p.x - 2, p.y, 0, 1, rgb(255, 255, 255), disabled ? 40 : 255);
        g.draw(g.rotorBlades, p.x - 2, p.y, spin, 1, rgb(255, 255, 255), 150);
    }
    if (e.cargo > 0) { float bx = p.x - std::cos(e.angle) * 2, by = p.y - std::sin(e.angle) * 2; g.fillCircle(bx, by, 3.2f, rgb(240, 205, 90)); g.fillCircle(bx - 0.8f, by - 0.8f, 1.4f, rgb(255, 240, 170)); }
    if (selected || e.hp < e.maxHp) hpBar(g, p.x - 10, p.y - e.radius() - 7, 20, e.hp / e.maxHp);
    if (selected && air && ut.ammo > 0) for (int k = 0; k < ut.ammo; k++) g.fill((int)p.x - 10 + k * 3, (int)p.y - e.radius() - 11, 2, 2, k < e.ammo ? rgb(255, 230, 120) : rgb(80, 80, 80));
}

void Game::renderWorld() {
    Gfx& g = g_gfx;
    g.beginFrame(rgb(6, 8, 12));
    SDL_Rect clip = { 0, 0, SCREEN_W, VIEW_H };
    SDL_RenderSetClipRect(g.ren, &clip);
    // screen shake from big blasts near the view centre (restored at the end of the frame)
    Vec2 camSaved = cam;
    {
        float amp = 0; Vec2 centre = cam + Vec2(SCREEN_W * 0.5f, VIEW_H * 0.5f);
        for (auto& f : g_sim.fx) if (f.type == FX_EXPLODE && f.size >= 26 && f.t < 0.3f) amp = std::max(amp, (f.size / 60.0f) * (1 - f.t / 0.3f) * clampf(1.4f - dist(f.a, centre) / 900.0f, 0, 1));
        if (amp > 0 && !paused) cam += Vec2(std::sin(wallTime * 93.0f), std::cos(wallTime * 77.0f)) * (amp * 5.0f);
    }
    // terrain
    int tx0 = std::max(0, tileOf(cam.x)), ty0 = std::max(0, tileOf(cam.y));
    int tx1 = std::min(MAP_W - 1, tileOf(cam.x + SCREEN_W)), ty1 = std::min(MAP_H - 1, tileOf(cam.y + VIEW_H));
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    // baked terrain: one blit of the visible window
    {
        int sx0 = clampi((int)cam.x, 0, WORLD_W - 1), sy0 = clampi((int)cam.y, 0, WORLD_H - 1);
        int sw = std::min(SCREEN_W + 1, WORLD_W - sx0), sh = std::min(VIEW_H + 1, WORLD_H - sy0);
        SDL_Rect src = { sx0, sy0, sw, sh }, dst = { sx0 - (int)cam.x, sy0 - (int)cam.y, sw, sh };
        SDL_SetTextureColorMod(g.worldTerrain.tex, 255, 255, 255); SDL_SetTextureAlphaMod(g.worldTerrain.tex, 255);
        SDL_RenderCopy(g.ren, g.worldTerrain.tex, &src, &dst);
    }
    // water: seamless animated caustics over open water (one small draw per visible interior tile)
    {
        int fr = ((int)(wallTime * 3.0f)) & 3;
        const Sprite& w = g.waterFx[fr];
        SDL_SetTextureColorMod(w.tex, 255, 255, 255); SDL_SetTextureAlphaMod(w.tex, 255);
        for (int ty = ty0; ty <= ty1; ty++) for (int tx = tx0; tx <= tx1; tx++) {
            if (g_map.tile(tx, ty) != T_WATER || !(rev || ex[ty * MAP_W + tx])) continue;
            if (tx < 1 || ty < 1 || tx > MAP_W - 2 || ty > MAP_H - 2) continue;
            if (g_map.tile(tx - 1, ty) != T_WATER || g_map.tile(tx + 1, ty) != T_WATER || g_map.tile(tx, ty - 1) != T_WATER || g_map.tile(tx, ty + 1) != T_WATER) continue;
            SDL_Rect src = { (tx & 3) * TILE, (ty & 3) * TILE, TILE, TILE };
            SDL_Rect dst = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), TILE, TILE };
            SDL_RenderCopy(g.ren, w.tex, &src, &dst);
        }
    }
    drawGroundFx();
    // placement ghost
    if (placingType >= 0) {
        Vec2 w = screenToWorld(mouseX, mouseY);
        const BuildType& bt = BUILDS[placingType];
        int tx = tileOf(w.x) - bt.w / 2, ty = tileOf(w.y) - bt.h / 2;
        bool ok = g_sim.canPlace(g_sim.humanPlayer, placingType, tx, ty);
        Vec2 c = worldToScreen(g_sim.buildingCenter(placingType, tx, ty));
        if (bt.weapon >= 0) rangeRing(c, WEAPONS[bt.weapon].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, bt.weapon, bt.name, true, false, false);
        { Color gt = ok ? rgb(160, 255, 160) : rgb(255, 120, 120); g.draw(g.building[placingType], c.x, c.y, 0, 1, gt, 150); if (g.buildingTeam[placingType].tex) g.draw(g.buildingTeam[placingType], c.x, c.y, 0, 1, gt, 150); }
        for (int j = 0; j < bt.h; j++) for (int i = 0; i < bt.w; i++) {
            bool t = g_map.buildable(tx + i, ty + j) && g_sim.explored(g_sim.humanPlayer, clampi(tx + i, 0, MAP_W - 1), clampi(ty + j, 0, MAP_H - 1));
            g.fill((int)((tx + i) * TILE - cam.x), (int)((ty + j) * TILE - cam.y), TILE, TILE, t ? rgb(80, 255, 80, 60) : rgb(255, 60, 60, 90));
        }
    }
    if (nukeMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        float pulse = 0.6f + 0.4f * std::sin(wallTime * 8.0f);
        g.discFill(m.x, m.y, NUKE_RADIUS * TILE, rgb(255, 60, 40, (int)(34 * pulse)));
        g.circle(m.x, m.y, NUKE_RADIUS * TILE, rgb(255, 90, 60), 48);
        g.circle(m.x, m.y, NUKE_RADIUS * TILE * 0.35f, rgb(255, 200, 90), 32);
    }
    if (powerMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        g.circle(m.x, m.y, POWERS[g_sim.players[g_sim.humanPlayer].faction].radius * TILE, hudAccent(g_sim.players[g_sim.humanPlayer].faction), 40);
    }
    drawZones();
    drawRangeRings();
    // entities: resources, buildings, then ground units sorted by y, projectiles, fx, aircraft
    std::vector<Entity*> ground, air;
    float vx0 = cam.x - 96, vy0 = cam.y - 96, vx1 = cam.x + SCREEN_W + 96, vy1 = cam.y + VIEW_H + 96;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!rev && e.owner != g_sim.humanPlayer && !ex[clampi(tileOf(e.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(e.pos.x), 0, MAP_W - 1)]) continue;
        if (e.kind == EK_RESOURCE || e.isBuilding()) drawEntity(e);
        else if (e.isAir()) air.push_back(&e);
        else ground.push_back(&e);
    }
    std::sort(ground.begin(), ground.end(), [](Entity* a, Entity* b) { return a->pos.y < b->pos.y; });
    for (auto* e : ground) drawEntity(*e);
    // flags: every finished structure flies its owner's flag from a mast on the roof edge (over units, under aircraft)
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isBuilding() || !e.constructed || e.owner < 0) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!rev && e.owner != g_sim.humanPlayer && !ex[clampi(tileOf(e.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(e.pos.x), 0, MAP_W - 1)]) continue;
        const BuildType& bt = e.bt();
        Vec2 p = worldToScreen(e.pos);
        float hw = bt.w * TILE * 0.5f, hh = bt.h * TILE * 0.5f;
        int area = bt.w * bt.h;
        float sc = area >= 12 ? 1.3f : (area >= 6 ? 1.0f : (area >= 4 ? 0.85f : 0.62f));
        float px = p.x + (bt.w >= 3 ? hw * 0.5f : hw * 0.4f), py = p.y - hh + (area >= 6 ? 12.0f : 7.0f);
        g.drawFlag(px, py, e.owner, sc, wallTime * 4.2f + (float)e.gen * 0.8f + (float)e.owner);
    }
    drawClouds();
    // projectiles: hot streaks for bullets and rockets, a lobbed shell with a ground shadow
    for (auto& p : g_sim.projs) {
        const Weapon& w = WEAPONS[p.weapon];
        Vec2 s = worldToScreen(p.prevPos + (p.pos - p.prevPos) * renderAlpha);
        if (s.x < -40 || s.y < -80 || s.x > SCREEN_W + 40 || s.y > VIEW_H + 40) continue;
        if (w.proj == PJ_SHELL) {
            float t = clampf(p.arcT, 0, 1), h = std::sin(t * 3.14159f) * 60;
            float sc = 1.0f + 0.5f * std::sin(t * 3.14159f);
            g.drawSized(g.shadowSmall, s.x, s.y + 2, 9 * sc, 5 * sc, 0, rgb(255, 255, 255), 150);
            g.fillCircle(s.x, s.y - h, 3.0f, rgb(30, 30, 32));
            g.fillCircle(s.x - 0.8f, s.y - h - 0.8f, 1.3f, rgb(150, 150, 154));
            g.glowAdd(s.x, s.y - h, 6, Color{255, 190, 110, 70});
        } else if (w.proj == PJ_BOMB) {   // a bomb dropping from altitude: tumbling fat body, fins, a glow on the nose and a shadow closing in on the impact point
            float t = clampf(p.arcT, 0, 1), h = (1.0f - t * t) * 78.0f;
            Vec2 gd = (p.dest - p.vel).norm();
            float sc = 0.7f + 0.5f * t;
            Color glow = p.weapon == W_BOMB_CYBER ? Color{130, 225, 255, 130} : Color{255, 170, 90, 130};
            g.drawSized(g.shadowSmall, s.x, s.y + 2, 11 * sc, 6 * sc, 0, rgb(255, 255, 255), (u8)(90 + 90 * t));
            float bx = s.x, by = s.y - h;
            g.drawSized(g.fxs.streak, bx - gd.x * 5, by - 6, 4, 16, 1.5708f, rgb(220, 220, 230), (u8)(120 * (1 - t)));
            g.fillCircle(bx, by, 3.6f, rgb(36, 38, 42));
            g.fillCircle(bx - 0.9f, by - 0.9f, 1.5f, rgb(128, 132, 140));
            g.line(bx - 2.6f, by - 4.6f, bx + 2.6f, by - 4.6f, rgb(70, 74, 82));
            g.glowAdd(bx, by, 7, glow);
        } else if (w.proj == PJ_ROCKET) {
            Vec2 d = p.vel.norm();
            float ang = std::atan2(d.y, d.x);
            Vec2 mid = s - d * 9.0f;
            g.drawSized(g.fxs.streak, mid.x, mid.y, 22, 4.0f, ang, rgb(255, 214, 150), 255);
            g.glowAdd(s.x, s.y, 7, Color{w.color.r, w.color.g, w.color.b, 150});
            g.fillCircle(s.x, s.y, 1.9f, rgb(255, 250, 235));
        } else {
            Vec2 d = p.vel.norm();
            float ang = std::atan2(d.y, d.x);
            Vec2 mid = s - d * 8.0f;
            g.drawSized(g.fxs.streak, mid.x, mid.y, 18, 2.6f, ang, w.color, 255);
            g.glowAdd(s.x, s.y, 4.5f, Color{w.color.r, w.color.g, w.color.b, 110});
        }
    }
    drawParticles(0);
    drawFx();
    drawParticles(1);
    drawNukes();
    for (auto* e : air) drawEntity(*e);
    drawShroud();
    // drag box
    if (dragging) {
        Vec2 a = worldToScreen(dragStart), b = worldToScreen(dragNow);
        g.fill((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 30));
        g.box((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 200));
    }
    // cursor hints
    if (areaMode) {}
    else if (attackMoveMode) g.text(mouseX + 12, mouseY - 4, "ATTACK", rgb(255, 90, 80));
    else if (nukeMode) g.text(mouseX + 12, mouseY - 4, "NUKE", rgb(255, 100, 70));
    else if (powerMode) g.text(mouseX + 12, mouseY - 4, POWERS[g_sim.players[g_sim.humanPlayer].faction].name, hudAccent(g_sim.players[g_sim.humanPlayer].faction));
    else if (rallyMode) g.text(mouseX + 12, mouseY - 4, "RALLY", rgb(120, 255, 120));
    else if (mouseY < VIEW_H) {
        Entity* h = pickEntity(screenToWorld(mouseX, mouseY), false);
        if (h && h->kind != EK_RESOURCE && h->owner != g_sim.humanPlayer) {
            const char* nm = h->isUnit() ? h->ut().name : h->bt().name;
            bool foe = g_sim.enemies(g_sim.humanPlayer, h->owner);
            char buf[96]; snprintf(buf, sizeof buf, "%s (%s%s)", nm, foe || h->owner < 0 ? "" : "ally, ", h->owner >= 0 ? FACTION_NAME[g_sim.players[h->owner].faction] : "");
            g.text(mouseX + 12, mouseY - 4, buf, foe ? rgb(255, 150, 140) : rgb(150, 235, 165));
        } else if (h && h->kind == EK_RESOURCE) { char buf[40]; snprintf(buf, sizeof buf, "Supplies: $%d", h->amount); g.text(mouseX + 12, mouseY - 4, buf, rgb(240, 220, 150)); }
    }
    SDL_RenderSetClipRect(g.ren, nullptr);
    cam = camSaved;
}

void Game::drawMinimap(int x, int y, int size) {
    Gfx& g = g_gfx;
    g.drawRect(g.minimapTerrain, x, y, size, size);
    float sc = size / (float)MAP_W;
    const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
    SDL_SetRenderDrawColor(g.ren, 4, 6, 10, 255);
    // shroud in 2x2 tile blocks (cheap)
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    for (int ty = 0; ty < MAP_H && !rev; ty += 2) for (int tx = 0; tx < MAP_W; tx += 2) {
        if (ex[ty * MAP_W + tx] || ex[ty * MAP_W + tx + 1] || ex[(ty + 1) * MAP_W + tx] || ex[(ty + 1) * MAP_W + tx + 1]) continue;
        SDL_Rect r = { (int)(x + tx * sc), (int)(y + ty * sc), (int)std::ceil(2 * sc), (int)std::ceil(2 * sc) };
        SDL_RenderFillRect(g.ren, &r);
    }
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (e.kind == EK_RESOURCE) { if (rev || ex[ty * MAP_W + tx]) g.fill((int)(x + tx * sc), (int)(y + ty * sc), 2, 2, rgb(230, 200, 90)); continue; }
        if (!rev && e.owner != g_sim.humanPlayer && !ex[ty * MAP_W + tx]) continue;
        Color c = playerColor(e.owner);
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
    snprintf(buf, sizeof buf, "%s%s  speed x%.2g%s", FACTION_NAME[pl.faction], g_audio.isMuted() ? "  [muted]" : "", speed, paused ? "  PAUSED" : "");
    g.text(SCREEN_W - 8 - g.textW(buf), 6, buf, hudDim());
    // alive players
    int ax = SCREEN_W / 2 + 40;
    for (int p = 0; p < g_sim.numPlayers; p++) {
        const Sprite& fl = g.flag[slotOf(p)];
        int fx = ax + p * 22;
        if (g_sim.players[p].alive) { g.draw(fl, (float)fx, 4.0f, 0, 0.75f); g.fill(fx - 1, 3, 1, 14, rgb(200, 204, 212)); }
        else { g.draw(fl, (float)fx, 4.0f, 0, 0.75f, rgb(70, 70, 70), 160); g.fill(fx - 1, 3, 1, 14, rgb(90, 90, 96)); g.line((float)fx, 5, (float)fx + 17, 15, rgb(20, 20, 20)); }
    }
    if (g_sim.revealed(g_sim.humanPlayer)) {
        snprintf(buf, sizeof buf, "%s %ds", SCANS[pl.faction].name, (int)std::ceil(pl.revealUntil - g_sim.time));
        g.text(ax + g_sim.numPlayers * 22 + 12, 6, buf, accent);
    }
    // messages
    for (size_t i = 0; i < messages.size(); i++) {
        float age = wallTime - messages[i].time;
        if (age > 9) continue;
        Color c = messages[i].color; c.a = (u8)(255 * clampf(1.5f - age / 6.0f, 0, 1));
        g.text(8, 26 + (int)i * 11, messages[i].text.c_str(), c);
    }
    if (paused && !menuOpen) g.text(SCREEN_W / 2 - g.textW("PAUSED", 3) / 2, VIEW_H / 2 - 12, "PAUSED", rgb(255, 255, 255), 3);

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
                float psc = (e->ut().jet ? 1.6f : 2.0f) / artScale(e->type);   // the big elite units and the long jets are scaled to sit inside the frame
                g.draw(g.unitBody[e->type][slotOf(e->owner)], INFO_X + 42, hy + 48, 0, psc);
                if (g.unitTurret[e->type][slotOf(e->owner)].tex) g.draw(g.unitTurret[e->type][slotOf(e->owner)], INFO_X + 42, hy + 48, 0, psc);
            } else {
                drawFitted(g, g.building[e->type], INFO_X + 42, hy + 48, 60);
                if (g.buildingTeam[e->type].tex) drawFitted(g, g.buildingTeam[e->type], INFO_X + 42, hy + 48, 60, &g.building[e->type], playerColor(e->owner));
            }
            const char* name = e->isUnit() ? e->ut().name : e->bt().name;
            g.text(INFO_X + 86, hy + 16, name, rgb(255, 255, 255), 2);
            if (e->isBuilding() && !e->constructed) { snprintf(buf, sizeof buf, "Under construction %d%%", (int)(e->progress * 100)); g.text(INFO_X + 86, hy + 36, buf, rgb(240, 220, 130)); }
            else { snprintf(buf, sizeof buf, "HP %d / %d", (int)std::ceil(e->hp), (int)e->maxHp); g.text(INFO_X + 86, hy + 36, buf, hudText()); }
            hpBar(g, INFO_X + 86, hy + 48, 150, e->isBuilding() && !e->constructed ? e->progress : e->hp / e->maxHp, true);
            if (e->isUnit()) {
                const UnitType& ut = e->ut();
                if (ut.bomber) { const Weapon& bw = WEAPONS[ut.faction == F_CYBER ? W_BOMB_CYBER : W_BOMB_CLANKER]; snprintf(buf, sizeof buf, "%s x%d  dmg %d  blast %.1f  (guns vs air)", bw.name, BOMB_STICK, (int)bw.dmg, bw.splash); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                else if (ut.weapon >= 0) { const Weapon& w = WEAPONS[ut.weapon]; snprintf(buf, sizeof buf, "%s  dmg %d  range %.1f%s%s", w.name, (int)w.dmg, w.range, w.air ? "  AA" : "", w.ground ? "" : "  air only"); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.role == UR_HARVESTER) { snprintf(buf, sizeof buf, "Cargo $%d / %d", e->cargo, SUPPLY_PER_TRIP); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.ammo > 0) { snprintf(buf, sizeof buf, "Ammo %d / %d", e->ammo, ut.ammo); g.text(INFO_X + 86, hy + 70, buf, hudDim()); }
                g.text(INFO_X + 86, hy + 84, ut.desc, hudDim());
                const char* st = (e->zoneR > 0 && (e->order == O_GUARDAREA || e->postOrder == O_GUARDAREA)) ? (e->order == O_ATTACK ? "Defending assigned area" : "Guarding assigned area") : (e->zoneR > 0 && (e->order == O_HARVEST || e->order == O_RETURN)) ? (e->order == O_RETURN ? "Returning (assigned area)" : "Gathering in assigned area") : e->order == O_IDLE ? "Idle" : e->order == O_MOVE ? "Moving" : e->order == O_ATTACKMOVE ? "Attack-moving" : e->order == O_ATTACK ? "Attacking" : e->order == O_HARVEST ? "Gathering" : e->order == O_RETURN ? "Returning" : e->order == O_BUILD ? "Constructing" : e->order == O_REARM ? "Rearming" : "Guarding";
                g.text(INFO_X + 86, hy + 98, st, accent);
            } else {
                const BuildType& bt = e->bt();
                if (bt.power != 0) { snprintf(buf, sizeof buf, "Power %+d", bt.power); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                { char d[96]; snprintf(d, sizeof d, "%.*s", e->queue.empty() ? 76 : 38, bt.desc); g.text(INFO_X + 86, hy + 70, d, hudDim()); }
                if (bt.role == BR_TECH && e->constructed) {
                    const ProgramType& pg = PROGRAMS[pl.faction];
                    if (pl.advTech) snprintf(buf, sizeof buf, "%s: researched, special units unlocked", pg.name);
                    else if (pl.researching) snprintf(buf, sizeof buf, "%s: researching %d%%%s", pg.name, (int)(pl.researchProgress * 100), pl.lowPower() ? " (slowed: low power)" : "");
                    else snprintf(buf, sizeof buf, "%s: not researched ($%d)", pg.name, pg.cost);
                    g.text(INFO_X + 86, hy + 84, buf, pl.advTech ? accent : hudDim());
                    if (g_sim.revealed(g_sim.humanPlayer)) { snprintf(buf, sizeof buf, "%s: %ds left", SCANS[pl.faction].name, (int)std::ceil(pl.revealUntil - g_sim.time)); g.text(INFO_X + 86, hy + 98, buf, accent); }
                }
                if (!e->queue.empty()) {
                    g.text(INFO_X + 330, hy + 58, "Queue (click to cancel)", hudDim());
                    int qx = INFO_X + 330, qy = hy + 70;
                    for (int i = 0; i < (int)e->queue.size() && i < 9; i++) {
                        g.bevelPanel(qx + i * 30, qy, 28, 28, shade(base, 0.55f), false);
                        g.draw(g.unitBody[e->queue[i]][slotOf(e->owner)], qx + i * 30 + 14, qy + 14, 0, 0.8f);
                        if (i == 0) hpBar(g, qx, qy + 30, 28, e->queueProgress);
                    }
                    snprintf(buf, sizeof buf, "%s %d%%", UNITS[e->queue[0]].name, (int)(e->queueProgress * 100)); g.text(INFO_X + 330, hy + 104, buf, hudText());
                } else if (e->constructed && (bt.role == BR_BARRACKS || bt.role == BR_FACTORY || bt.role == BR_AIRFIELD || bt.role == BR_HQ || bt.role == BR_SUPPLY)) g.text(INFO_X + 330, hy + 58, "Production idle", hudDim());
                if (pl.lowPower() && bt.power < 0) g.text(INFO_X + 86, hy + (bt.role == BR_TECH ? 110 : 98), "LOW POWER: reduced output", rgb(255, 140, 120));
                if (bt.role == BR_INCOME && e->constructed) {
                    bool half = pl.lowPower() && bt.power < 0;
                    int amt = pl.faction == F_CYBER ? 75 : 60;
                    snprintf(buf, sizeof buf, "Income $%d every %ds%s   total earned $%d", amt, (int)INCOME_INTERVAL, half ? " (low power: half rate)" : "", pl.mined); g.text(INFO_X + 86, hy + 84, buf, rgb(240, 220, 130));
                    snprintf(buf, sizeof buf, "%d of %d income structures", g_sim.countRole(g_sim.humanPlayer, BR_INCOME, false), INCOME_MAX); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_NUKE && e->constructed) {
                    float w = std::max(0.0f, e->actionTimer - g_sim.time);
                    if (w > 0) snprintf(buf, sizeof buf, "Warhead %s: %d:%02d", "loading", (int)w / 60, (int)w % 60);
                    else snprintf(buf, sizeof buf, "WARHEAD READY  (K to launch)");
                    g.text(INFO_X + 86, hy + 84, buf, w > 0 ? rgb(255, 200, 120) : rgb(255, 110, 90));
                    snprintf(buf, sizeof buf, "%d ramp(s) ready of %d", g_sim.nukesReady(g_sim.humanPlayer), g_sim.countRole(g_sim.humanPlayer, BR_NUKE, true)); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_AIRFIELD && e->constructed) {
                    int n = 0; Ref self = g_sim.refOf(*e);
                    for (auto& u : g_sim.ents) if (u.alive && u.isUnit() && u.isAir() && u.home == self) n++;
                    snprintf(buf, sizeof buf, "Aircraft %d / 4%s", n, n >= 4 ? "  (full: production waits)" : ""); g.text(INFO_X + 86, hy + 84, buf, n >= 4 ? rgb(255, 200, 120) : hudDim());
                }
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
            g.draw(g.unitBody[e->type][slotOf(e->owner)], sx + 15, sy + 15, 0, 0.85f);
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
        g.text(b.x + 5, b.y + 4, b.label, tc);
        int cost = b.kind == BK_BUILD ? BUILDS[b.id].cost : (b.kind == BK_TRAIN ? UNITS[b.id].cost : -1);
        if (cost >= 0) { snprintf(buf, sizeof buf, "$%d", cost); g.text(b.x + 5, b.y + 15, buf, pl.money >= cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        if (b.kind == BK_POWER || b.kind == BK_SCAN) {
            float rem = (b.kind == BK_POWER ? pl.powerReady : pl.scanReady) - g_sim.time;
            if (rem > 0) { snprintf(buf, sizeof buf, "%d:%02d", (int)rem / 60, (int)rem % 60); g.text(b.x + 5, b.y + 15, buf, hudDim()); }
            else if (b.enabled) g.text(b.x + 5, b.y + 15, "READY", accent);
        }
        if (b.kind == BK_RESEARCH) {
            const ProgramType& pg = PROGRAMS[pl.faction];
            if (pl.advTech) g.text(b.x + 5, b.y + 15, "DONE", accent);
            else if (pl.researching) { snprintf(buf, sizeof buf, "%d%%", (int)(pl.researchProgress * 100)); g.text(b.x + 5, b.y + 15, buf, accent); g.fill(b.x + 36, b.y + 18, (int)((b.w - 42) * clampf(pl.researchProgress, 0, 1)), 3, accent); }
            else { snprintf(buf, sizeof buf, "$%d", pg.cost); g.text(b.x + 5, b.y + 15, buf, pl.money >= pg.cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        }
        const char* hk = kindHotkey(b.kind, b.id);
        if (hk) g.text(b.x + b.w - 12, b.y + 15, hk, accent);
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
        int hx = SCREEN_W / 2 - 312, hy0 = std::max(30, VIEW_H / 2 - 170);
        g.fill(hx, hy0, 624, 300, rgb(8, 10, 14, 235));
        g.box(hx, hy0, 624, 300, accent);
        g.text(hx + 16, hy0 + 12, "ONE HOUR - controls", rgb(255, 255, 255), 2);
        const char* lines[] = {
            "Left click / drag box      select units (double-click: all of that type on screen)",
            "Right click                move / attack / gather / repair / rally (also on the minimap)",
            "A + click                  attack-move       S: stop        Tab: army on screen",
            "G, then click or drag      guard an area (fighters, drones) / gather in an area (haulers)",
            "Click a turret or battery  shows the area it covers (hover any defense to see its reach)",
            "Ctrl + 0-9 / 0-9           assign / recall control group (Alt+#: jump to it)",
            "Arrows, screen edge, middle-drag, wheel: scroll     Home: your base",
            "Dozer selected             build menu; click the ground to place, Shift for several",
            "Structure selected         train units; right-click ground for rally; Del sells",
            "Tech structure            X strike, V map scan (30s), R Advanced Program: unlocks elite units",
            "Space pause    + / - speed    F2 mute    F11 fullscreen    F12 screenshot    Esc menu",
            "",
            "Supplies: haulers carry $300 per trip to a Supply Hub/Depot.",
            "Power: keep production above consumption or turrets and factories slow down.",
        };
        for (size_t i = 0; i < sizeof(lines) / sizeof(lines[0]); i++) g.text(hx + 16, hy0 + 40 + (int)i * 16, lines[i], hudText());
    }
}

// ------------------------------------------------------------ nukes and pause menu
void Game::drawNukes() {
    Gfx& g = g_gfx;
    for (auto& n : g_sim.nukes) {
        float k = clampf(n.t / Sim::NUKE_FLIGHT, 0, 1);
        Vec2 tgt = worldToScreen(n.pos);
        // warning zone
        float pulse = 0.5f + 0.5f * std::sin(wallTime * 9.0f);
        g.discFill(tgt.x, tgt.y, NUKE_RADIUS * TILE, rgb(255, 60, 40, (int)(20 + 26 * pulse)));
        g.dashedCircle(tgt.x, tgt.y, NUKE_RADIUS * TILE, rgb(255, 90, 60), wallTime * 30.0f);
        char buf[32]; snprintf(buf, sizeof buf, "NUKE %ds", (int)std::ceil(Sim::NUKE_FLIGHT - n.t));
        g.text((int)(tgt.x - g.textW(buf) / 2), (int)(tgt.y - 6), buf, rgb(255, 230, 200));
        // the missile: rises from the ramp on a long arc and falls on the target
        auto world = [&](float u) { Vec2 p = n.from + (n.pos - n.from) * u; p.y -= std::sin(u * 3.14159f) * 260.0f; return p; };
        Vec2 w0 = world(k), w1 = world(std::min(1.0f, k + 0.012f));
        Vec2 m = worldToScreen(w0), dir = (w1 - w0).norm();
        float ang = std::atan2(dir.y, dir.x);
        int fr = ((int)(wallTime * 24)) % 6;
        Vec2 tail = m - dir * 17.0f;
        g.draw(g.fxs.flame[fr], tail.x, tail.y, ang - 1.5708f, 1.5f, rgb(255, 214, 160), 235);
        g.glowAdd(tail.x, tail.y, 30, Color{255, 160, 70, 170});
        g.draw(g.fxs.missile, m.x, m.y, ang, 1.5f);
        if (frameDt > 0 && onScreen(w0.x, w0.y, 200) && fxRng.f() < 55.0f * frameDt)
            emitP(w0.x - dir.x * 20, w0.y - dir.y * 20, fxRng.f(-8, 8), fxRng.f(-8, 8), fxRng.f(1.8f, 2.6f), 5, 17, rgb(232, 228, 222, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.8f, fxRng.f(0, 6), fxRng.f(-0.4f, 0.4f));
    }
    if (nukeFlash > 0) {
        g.fill(0, 0, SCREEN_W, VIEW_H, rgb(255, 246, 228, (int)(215 * clampf(nukeFlash / 0.55f, 0, 1))));
    }
}
void Game::stepSpeed(int dir) {
    int cur = 0; float bd = 1e9f;
    for (int i = 0; i < SPEED_COUNT; i++) if (std::abs(SPEED_STEPS[i] - speed) < bd) { bd = std::abs(SPEED_STEPS[i] - speed); cur = i; }
    speed = SPEED_STEPS[clampi(cur + dir, 0, SPEED_COUNT - 1)];
}

void Game::openPauseMenu() {
    menuOpen = true; menuCursor = 0; menuConfirm = -1;
    pausedBeforeMenu = paused; paused = true;
    cancelModes(); dragging = false; midDrag = false;
    g_audio.play(SND_CLICK, Vec2(), true);
}
void Game::closePauseMenu() {
    menuOpen = false; menuConfirm = -1; paused = pausedBeforeMenu;
    g_audio.play(SND_CLICK, Vec2(), true);
}

static const int PM_ROWS = 8;
static const char* PM_LABEL[PM_ROWS] = { "Resume", "Game speed", "Sound", "Controls help", "Restart match", "Surrender (end game)", "Quit to main menu", "Quit to desktop" };
static int pmY0() { return SCREEN_H / 2 - 178; }
static const int PM_W = 440, PM_RH = 34;

void Game::pauseMenuActivate(int row) {
    // destructive rows ask twice
    bool destructive = row >= 4;
    if (destructive && menuConfirm != row) { menuConfirm = row; g_audio.play(SND_CLICK, Vec2(), true); return; }
    menuConfirm = -1;
    switch (row) {
    case 0: closePauseMenu(); break;
    case 1: stepSpeed(1); break;
    case 2: g_audio.setMuted(!g_audio.isMuted()); break;
    case 3: closePauseMenu(); showHelp = true; break;
    case 4: closePauseMenu(); startGame(); break;
    case 5: {
        closePauseMenu(); paused = false;
        int me = g_sim.humanPlayer, winner = -1;
        for (int p = 0; p < g_sim.numPlayers; p++) if (g_sim.enemies(me, p) && g_sim.players[p].alive) { winner = g_sim.players[p].team; break; }   // an ally does not win the game for you
        g_sim.players[me].alive = false;
        g_sim.gameOver = true; g_sim.winnerTeam = winner;
        break;
    }
    case 6: closePauseMenu(); state = GS_MENU; break;
    case 7: quitRequested = true; break;
    }
}

void Game::pauseMenuEvent(const SDL_Event& e) {
    int x0 = SCREEN_W / 2 - PM_W / 2, y0 = pmY0();
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE) { if (menuConfirm >= 0) menuConfirm = -1; else closePauseMenu(); return; }
        if (k == SDLK_UP) { menuCursor = (menuCursor + PM_ROWS - 1) % PM_ROWS; menuConfirm = -1; }
        else if (k == SDLK_DOWN) { menuCursor = (menuCursor + 1) % PM_ROWS; menuConfirm = -1; }
        else if (k == SDLK_LEFT) { if (menuCursor == 1) stepSpeed(-1); else if (menuCursor == 2) g_audio.setMuted(!g_audio.isMuted()); }
        else if (k == SDLK_RIGHT) { if (menuCursor == 1) stepSpeed(1); else if (menuCursor == 2) g_audio.setMuted(!g_audio.isMuted()); }
        else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) stepSpeed(-1);
        else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) stepSpeed(1);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
            pauseMenuActivate(menuCursor);
        }
        return;
    }
    if (e.type == SDL_MOUSEMOTION) {
        int row = (mouseY - y0) / PM_RH;
        if (mouseX >= x0 && mouseX < x0 + PM_W && mouseY >= y0 && row >= 0 && row < PM_ROWS && row != menuCursor) { menuCursor = row; menuConfirm = -1; }
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int row = (mouseY - y0) / PM_RH;
        if (mouseX < x0 || mouseX >= x0 + PM_W || mouseY < y0 || row < 0 || row >= PM_ROWS) return;
        menuCursor = row;
        if (row == 1) {
            // slider: click a notch to jump there, the arrows step
            int sx = x0 + 190, sw = PM_W - 200;
            if (mouseX >= sx - 14 && mouseX <= sx + sw + 14) {
                int i = clampi((int)std::floor((mouseX - sx) / (float)sw * (SPEED_COUNT - 1) + 0.5f), 0, SPEED_COUNT - 1);
                speed = SPEED_STEPS[i]; g_audio.play(SND_CLICK, Vec2(), true);
            }
        } else pauseMenuActivate(row);
    }
}

void Game::renderPauseMenu() {
    Gfx& g = g_gfx;
    Player& pl = g_sim.players[g_sim.humanPlayer];
    Color accent = hudAccent(pl.faction), base = hudBase(pl.faction);
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(6, 8, 12, 165));
    int x0 = SCREEN_W / 2 - PM_W / 2, y0 = pmY0();
    g.bevelPanel(x0 - 16, y0 - 62, PM_W + 32, PM_ROWS * PM_RH + 112, shade(base, 0.7f), true);
    g.fill(x0 - 16, y0 - 62, PM_W + 32, 3, accent);
    g.text(SCREEN_W / 2 - g.textW("GAME PAUSED", 3) / 2, y0 - 46, "GAME PAUSED", rgb(255, 255, 255), 3);
    int secs = (int)g_sim.time; char buf[96];
    snprintf(buf, sizeof buf, "%s   %02d:%02d   $%d", FACTION_NAME[pl.faction], secs / 60, secs % 60, pl.money);
    g.text(SCREEN_W / 2 - g.textW(buf) / 2, y0 - 16, buf, hudDim());
    for (int i = 0; i < PM_ROWS; i++) {
        int y = y0 + i * PM_RH;
        bool cur = i == menuCursor, conf = menuConfirm == i;
        Color rowc = conf ? rgb(150, 50, 40) : (cur ? shade(base, 1.6f) : shade(base, 1.05f));
        g.bevelPanel(x0, y, PM_W, PM_RH - 4, rowc, true);
        const char* lab = PM_LABEL[i];
        if (conf) lab = i == 5 ? "Surrender? Enter = yes" : (i == 4 ? "Restart? Enter = yes" : (i == 6 ? "Leave the match? Enter = yes" : "Exit game? Enter = yes"));
        g.text(x0 + 12, y + 9, lab, cur ? rgb(255, 255, 255) : hudText(), 1);
        if (i == 0) g.text(x0 + PM_W - 12 - g.textW("Esc"), y + 9, "Esc", hudDim());
        if (i == 2) g.text(x0 + PM_W - 12 - g.textW(g_audio.isMuted() ? "Off" : "On"), y + 9, g_audio.isMuted() ? "Off" : "On", g_audio.isMuted() ? rgb(255, 150, 120) : accent);
        if (i == 3) g.text(x0 + PM_W - 12 - g.textW("F1"), y + 9, "F1", hudDim());
        if (i == 1) {
            int sx = x0 + 190, sw = PM_W - 200, ty = y + 13;
            g.fill(sx, ty, sw, 3, shade(base, 0.5f));
            int cur_i = 0; float bd = 1e9f;
            for (int k = 0; k < SPEED_COUNT; k++) if (std::abs(SPEED_STEPS[k] - speed) < bd) { bd = std::abs(SPEED_STEPS[k] - speed); cur_i = k; }
            for (int k = 0; k < SPEED_COUNT; k++) {
                int nx = sx + (int)(sw * k / (float)(SPEED_COUNT - 1));
                g.fill(nx - 1, ty - 4, 3, 11, k <= cur_i ? accent : hudDim());
            }
            int hx = sx + (int)(sw * cur_i / (float)(SPEED_COUNT - 1));
            g.fill(sx, ty, hx - sx, 3, accent);
            g.bevelPanel(hx - 5, ty - 7, 11, 17, rgb(240, 244, 248), true);
            snprintf(buf, sizeof buf, "x%.2g", speed);
            g.text(x0 + 110, y + 9, buf, accent);
        }
    }
    g.text(SCREEN_W / 2 - g.textW("Up/Down choose   Left/Right change   Enter select   Esc resume") / 2, y0 + PM_ROWS * PM_RH + 6, "Up/Down choose   Left/Right change   Enter select   Esc resume", hudDim());
}

void Game::renderGameOver() {
    Gfx& g = g_gfx;
    bool won = g_sim.winnerTeam == g_sim.players[0].team;
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(0, 0, 0, 150));
    const char* t = won ? "VICTORY" : "DEFEAT";
    g.text(SCREEN_W / 2 - g.textW(t, 6) / 2, SCREEN_H / 2 - 170, t, won ? rgb(140, 255, 150) : rgb(255, 110, 100), 6);
    Player& pl = g_sim.players[0];
    char buf[128];
    int secs = (int)g_sim.time;
    snprintf(buf, sizeof buf, "Time %02d:%02d   Units built %d   Units lost %d   Kills %d   Structures destroyed %d   Supplies gathered $%d",
             secs / 60, secs % 60, pl.unitsBuilt, pl.unitsLost, pl.unitsKilled, pl.structuresKilled, pl.harvested);
    g.text(SCREEN_W / 2 - g.textW(buf) / 2, SCREEN_H / 2 - 90, buf, hudText());
    g.text(SCREEN_W / 2 - g.textW("Press Enter to return to the menu", 2) / 2, SCREEN_H / 2 - 50, "Press Enter to return to the menu", hudDim(), 2);
}

void Game::screenshot(const char* path) {
    Gfx& g = g_gfx;
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, SCREEN_W, SCREEN_H, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s) return;
    SDL_Rect r = { 0, 0, SCREEN_W, SCREEN_H };
    if (SDL_RenderReadPixels(g.ren, &r, SDL_PIXELFORMAT_ARGB8888, s->pixels, s->pitch) == 0) SDL_SaveBMP(s, path);
    SDL_FreeSurface(s);
}
