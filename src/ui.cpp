#include "ui.h"
#include "art.h"
#include "art2.h"
#include "icons.h"

Game g_game;

enum { BK_BUILD = 1, BK_TRAIN, BK_SELL, BK_RALLY, BK_POWER, BK_ATTACKMOVE, BK_STOP, BK_CANCEL, BK_AREA, BK_SCAN, BK_RESEARCH, BK_NUKE, BK_DROP, BK_FORCE, BK_UPGRADE, BK_AID, BK_UNLOAD, BK_DROPOFF, BK_MININUKE, BK_AIRSTRIKE, BK_DEEPNUKE,
       BK_ISLANDNUKE,   // an island launcher the army holds: fire its small nuke
       BK_NUKEBOMBER,   // tech structure: call in the Nuclear Bomber
       BK_FLEET,        // tech structure: Fleet Call
       BK_BUNKERUPG,    // a bunker's own upgrade (id: BunkerUpg)
       BK_BUNKERPARA,   // a bunker's Para Backup call
       BK_BUNKERRAIN,   // a bunker's Rain Nukes
       BK_TANKDROP,     // a bunker's Tank Drop
       BK_BUNKERALL };  // Upgrade All Bunkers (id 1: cancel what it queued)

static const char* kindHotkey(int kind, int id) {
    switch (kind) {
    case BK_BUILD: return BUILDS[id].hotkey;
    case BK_TRAIN: return UNITS[id].hotkey;
    case BK_POWER: return "X";
    case BK_SCAN: return "V";
    case BK_RESEARCH: return "R";
    case BK_NUKE: return "K";
    case BK_MININUKE: return "K";
    case BK_DEEPNUKE: return "K";
    case BK_DROP: return "P";
    case BK_AIRSTRIKE: return "I";
    case BK_FLEET: return "L";
    case BK_ISLANDNUKE: return "K";
    case BK_NUKEBOMBER: return "B";
    case BK_AID: return "D";
    case BK_UNLOAD: return "U";
    case BK_DROPOFF: return "D";
    case BK_ATTACKMOVE: return "A";
    case BK_STOP: return "S";
    case BK_AREA: return "G";
    case BK_FORCE: return "F";
    case BK_UPGRADE: return UPGRADES[0][id].hotkey;   // (the same keys for both armies)
    case BK_BUNKERUPG: return BUNKER_UPGS[id].hotkey;
    case BK_BUNKERPARA: return "P";
    case BK_BUNKERRAIN: return "J";
    case BK_TANKDROP: return "D";
    case BK_BUNKERALL: return "A";
    }
    return nullptr;
}
// The icon of a command button; structure and unit buttons draw their own thumbnail instead (ICO_NONE)
static int buttonIcon(int kind, int id, const char* label, Faction f) {
    switch (kind) {
    case BK_NUKE: return ICO_NUKE;
    case BK_MININUKE: return ICO_MININUKE;
    case BK_DEEPNUKE: return ICO_DEEPNUKE;
    case BK_ISLANDNUKE: return ICO_ISLANDNUKE;
    case BK_NUKEBOMBER: return ICO_NUKEBOMBER;
    case BK_POWER: return f == F_CYBER ? ICO_EMP : ICO_SHELLSTORM;
    case BK_SCAN: return f == F_CYBER ? ICO_SCAN_ORBITAL : ICO_SCAN_RECON;
    case BK_DROP: return ICO_PARADROP;
    case BK_AID: return ICO_AID;
    case BK_AIRSTRIKE: return ICO_AIRSTRIKE;
    case BK_FLEET: return ICO_FLEET;
    case BK_SELL: return ICO_SELL;
    case BK_RALLY: return ICO_RALLY;
    case BK_ATTACKMOVE: return ICO_ATTACKMOVE;
    case BK_STOP: return ICO_STOP;
    case BK_CANCEL: return ICO_CANCEL;
    case BK_AREA: return std::strcmp(label, "Gather Area") == 0 ? ICO_GATHER : ICO_GUARD;   // (haulers only: the gather circle)
    case BK_FORCE: return ICO_FORCE;
    case BK_RESEARCH: return ICO_RESEARCH;
    case BK_UNLOAD: return ICO_UNLOAD;
    case BK_DROPOFF: return ICO_DROPOFF;
    case BK_BUNKERUPG: return ICO_BU_SHELL + id;
    case BK_BUNKERPARA: return ICO_PARADROP;
    case BK_BUNKERRAIN: return ICO_RAINNUKE;
    case BK_TANKDROP: return ICO_TANKDROP;
    case BK_BUNKERALL: return id == 1 ? ICO_CANCEL : ICO_BUNKERALL;
    case BK_UPGRADE:
        switch (id) {
        case UPG_RUGGED: return ICO_UP_RUGGED;
        case UPG_GUNS: return ICO_UP_GUNS;
        case UPG_REPAIR: return ICO_UP_REPAIR;
        case UPG_DRONES: return ICO_UP_DRONES;
        case UPG_HYPER: return ICO_UP_HYPER;
        case UPG_ANTIROCKET: return ICO_UP_ANTIROCKET;
        }
        return ICO_NONE;
    }
    return ICO_NONE;
}

static const float AREA_DEFAULT_R = 6.0f * TILE;   // circle radius for a plain click in area mode

// An army as messages name it: the colour it plays in and its faction ("Red Clanker Army")
static std::string armyName(int p) {
    static const char* const COLOUR[MAX_PLAYERS] = { "Blue", "Red", "Yellow", "Green", "Rioting" };
    return std::string(COLOUR[slotOf(p)]) + " " + FACTION_NAME[g_sim.players[p].faction];
}

// HUD layout: minimap left, command grid anchored right, the info panel takes whatever is between
static const int MINIMAP_X = 8, MINIMAP_SIZE = 116, BTN_W = 156, BTN_H = 28, BTN_GAP = 3;   // (wide enough for an icon and "Anti-Radiation Truck")
static const int BTN_MAX = 20, BTN_H_COMPACT = 22, BTN_H_DENSE = 19;   // up to six rows of three; past four rows (12 buttons) every button shrinks to fit, past five (15) a little more;
                                                                       // past six rows (18) the grid becomes five compact rows of four narrower buttons
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
    for (int i = 0; i < 3 && n < MAX_ARMIES; i++) {
        int f = menu.enemyFaction[i];
        if (f == 3) continue;
        fac[n] = f == 2 ? (Faction)r.range(0, 1) : (Faction)f;
        ai[n] = true; diff[n] = menu.enemyDiff[i]; team[n] = menu.enemiesAllied ? 1 : 1 + e; g_colorSlot[n] = 1 + e;
        n++; e++;
    }
    for (int k = n; k < MAX_ARMIES; k++) g_colorSlot[k] = k;
    g_colorSlot[RIOT_PLAYER] = RIOT_PLAYER;
    if (g_gfx.terrainEdited) { g_map.generate(); g_gfx.rebakeTerrain(); }   // (the last match cleared trees off the baked ground: start from a fresh bake)
    g_sim.init(n, fac, ai, diff, team, seed);
    g_ai.init(seed);
    selection.clear();
    for (auto& g : groups) g.clear();
    messages.clear();
    parts.clear(); decals.clear(); treads.clear(); treadHead = 0; nukeFlash = 0; frameDt = 0;   // no smoke, scorch or flash left over from the previous match
    placingType = -1; attackMoveMode = false; forceMode = false; forceLatch = false; powerMode = false; nukeMode = false; nukeMini = false; nukeDeep = false; nukeIsland = false; bomberMode = false; dropMode = false; strikeMode = false; fleetMode = false; aidMode = false; rallyMode = false; areaMode = false; areaDrag = false; bulkDrag = false; paraBunker = NOREF; tankBunker = NOREF; rainBunker = NOREF; zoneFlashes.clear(); paused = false; speed = 1.0f; menuOpen = false; menuConfirm = -1;
    accumulator = 0;
    Vec2 b = g_sim.players[0].basePos;
    cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
    state = GS_PLAYING;
    initWeather();
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
        // Catching up costs at most about 30 ms of a frame: a machine that cannot keep up (a slow frame, a busy tick) runs the battle a little
        // slower instead of stalling the picture for eight ticks at a time and then dropping the time anyway
        const u64 t0 = SDL_GetPerformanceCounter(), budget = SDL_GetPerformanceFrequency() * 30 / 1000;
        const int maxSteps = std::max(2, (int)std::ceil(speed * 2.0f));   // (fast-forward needs a few more ticks a frame)
        int steps = 0;
        while (accumulator >= SIM_DT && steps < maxSteps) {
            g_sim.step();
            g_ai.update();
            accumulator -= SIM_DT;
            steps++;
            if (SDL_GetPerformanceCounter() - t0 > budget) break;
        }
        if (accumulator > SIM_DT * 2) accumulator = SIM_DT * 2;   // can't keep up: drop time rather than spiral
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
        case EV_AID:
            if (mine && ev.other >= 0) {
                char buf[160]; snprintf(buf, sizeof buf, "Aid delivered to the %s (%s): $%d and a Dozer", armyName(ev.other).c_str(), g_sim.enemies(g_sim.humanPlayer, ev.other) ? "enemy" : "ally", AID_MONEY);
                addMessage(buf, playerColor(ev.other));
            }
            break;
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
    selection.erase(std::remove_if(selection.begin(), selection.end(), [&](Ref r) { Entity* e = g_sim.get(r); return !e || e->owner != g_sim.humanPlayer || e->carrier.valid(); }), selection.end());
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
    case BR_AIRFIELD: return 16; case BR_TECH: return 16; case BR_AATURRET: return 6; case BR_INCOME: return b.faction == F_CYBER ? 10 : 8; case BR_NUKE: return 16;
    case BR_NAVAL: return 18; case BR_ANTINUKE: return b.faction == F_CYBER ? 22 : 10; case BR_WALL: case BR_FWALL: return 10;
    case BR_HEALANT: return 18;   // (the mast)
    case BR_STRONGHOLD: return b.w >= 4 ? 26 : (b.w >= 3 ? 12 : 24);   // castle keep / low bunker / watchtower
    default: return 0;
    }
}

Entity* Game::pickEntity(Vec2 w, bool ownOnly) {
    Entity* best = nullptr; float bd = 1e9f;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (ownOnly && e.owner != g_sim.humanPlayer) continue;
        if (e.carrier.valid() || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;   // aboard a carrier, or hidden (stealth)
        if (!g_sim.shownToHuman(e)) continue;   // unexplored ground, or an enemy unit nobody of yours can see right now
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
        else if (e.ut().naval) {   // a ship is picked anywhere along its hull
            Vec2 f(std::cos(e.angle), std::sin(e.angle)), r = w - e.pos;
            float along = r.x * f.x + r.y * f.y, across = -r.x * f.y + r.y * f.x;
            d = std::abs(along) <= e.radius() * 1.55f && std::abs(across) <= e.radius() * 0.6f + 6 ? dist(w, e.pos) * 0.7f : 1e9f;
        }
        else d = dist(w, e.pos) <= e.radius() + 6 ? dist(w, e.pos) : 1e9f;
        if (d < bd) { bd = d; best = &e; }
    }
    return best;
}

void Game::selectSingle(Entity* e, bool add) {
    if (!add) selection.clear();
    if (!e || e->owner != g_sim.humanPlayer || (e->isUnit() && e->ut().temp)) return;   // (a Hotline Strike fighter on loan takes no orders)
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
        if (!e.alive || !e.isUnit() || e.owner != g_sim.humanPlayer || e.carrier.valid() || e.ut().temp) continue;
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
        if (!o.alive || !o.isUnit() || o.owner != e->owner || o.type != e->type || o.carrier.valid() || o.ut().temp) continue;
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

// Ctrl held = "force fire": the click may target your own or an ally's units and structures (and powers/nukes hit friendly ground too).
static bool forceFireKey() { return (SDL_GetModState() & KMOD_CTRL) != 0 || (g_game.forceLatch && g_game.powerMode); }   // (the F toggle only counts while aiming a strike: a nuke hits everything anyway)

void Game::issueRightClick(Vec2 w) {
    if (selection.empty()) return;
    Entity* t = pickEntity(w, false);
    if (t && t->kind != EK_RESOURCE && (t->owner >= 0 || Sim::invulnerable(*t)) && forceFireKey()) {
        std::vector<Ref> sel; Ref tr = g_sim.refOf(*t);
        for (auto r : selection) if (r != tr) sel.push_back(r);
        if (!sel.empty()) { g_sim.cmdAttack(sel, tr, !g_sim.enemies(g_sim.humanPlayer, t->owner)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); }
        return;
    }
    if (t && t->owner != g_sim.humanPlayer && t->kind != EK_RESOURCE && g_sim.enemies(g_sim.humanPlayer, t->owner)) {
        g_sim.cmdAttack(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
        return;
    }
    if (t && Sim::districtBlock(*t) && selectionHasRole(UR_DOZER) && !g_sim.enemies(g_sim.humanPlayer, t->owner) && !selectionHasRole(UR_SPY)) {   // a town round the lake that is not an enemy's: the dozers help raise and patch it
        std::vector<Ref> dz, rest;
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o) (o->isUnit() && o->ut().role == UR_DOZER ? dz : rest).push_back(r); }
        g_sim.cmdAssist(dz, g_sim.refOf(*t));
        if (!rest.empty()) g_sim.cmdMove(rest, t->pos, false);
        g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
        return;
    }
    if (t && t->isBuilding() && (t->constructed || t->district >= 0) && ((Sim::neutralTakeable(*t) && t->bt().role != BR_STRONGHOLD) || g_sim.allyTakeable(g_sim.humanPlayer, *t)) && selectionHasRole(UR_SPY)) {
        // nobody's island launcher or Bastion Wall, or an ally's structure: the infiltrators go and take it (swimming out to an island), everybody else goes along
        std::vector<Ref> spies, rest;
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o) (o->isUnit() && o->ut().role == UR_SPY ? spies : rest).push_back(r); }
        g_sim.cmdCapture(spies, g_sim.refOf(*t));
        if (!rest.empty()) g_sim.cmdMove(rest, t->pos, false);
        g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
        return;
    }
    if (t && t->owner >= 0 && t->owner != g_sim.humanPlayer && t->kind != EK_RESOURCE && !selectedBuilding()) {   // an ally: go to it (Ctrl or Force Fire to attack it)
        g_sim.cmdMove(selection, t->pos, false); g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
        return;
    }
    bool neutralFort = t && t->isBuilding() && t->bt().role == BR_STRONGHOLD && (t->owner < 0 || !g_sim.enemies(g_sim.humanPlayer, t->owner));
    bool boardable = t && ((t->isBuilding() && t->bt().role == BR_BUNKER && t->owner == g_sim.humanPlayer) || neutralFort || (t->isUnit() && t->owner == g_sim.humanPlayer && t->ut().cargoCap > 0));
    if (boardable) {   // climb aboard a cargo chopper / bunker / neutral (or friendly) civilian stronghold
        std::vector<Ref> riders; Ref tr = g_sim.refOf(*t);
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o && o != t && g_sim.canBoard(*o, *t)) riders.push_back(r); }
        if (!riders.empty()) { g_sim.cmdEnter(riders, tr); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
    }
    if (t && t->kind == EK_RESOURCE && selectionHasRole(UR_HARVESTER)) { g_sim.cmdHarvest(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
    if (t && t->isBuilding() && t->owner == g_sim.humanPlayer && selectionHasRole(UR_DOZER)) { g_sim.cmdAssist(selection, g_sim.refOf(*t)); g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
    Entity* b = selectedBuilding();
    if (b) { g_sim.cmdSetRally(g_sim.refOf(*b), w); g_audio.play(SND_CLICK, Vec2(), true); return; }
    g_sim.cmdMove(selection, w, false);
    g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
}

// Force Fire button: attack whatever unit or structure is under the click on purpose, your own and your allies' included
bool Game::issueForceFire(Vec2 w) {
    Entity* t = pickEntity(w, false);
    if (!t || t->kind == EK_RESOURCE || (t->owner < 0 && !Sim::invulnerable(*t))) return false;   // (nobody's island launcher or Bastion Wall may be fired on: it only burns)
    std::vector<Ref> sel; Ref tr = g_sim.refOf(*t);
    for (auto r : selection) if (r != tr) sel.push_back(r);
    if (sel.empty()) return false;
    g_sim.cmdAttack(sel, tr, !g_sim.enemies(g_sim.humanPlayer, t->owner));
    g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
    return true;
}

void Game::issueAttackMove(Vec2 w) {
    Entity* t = pickEntity(w, false);
    if (t && t->kind != EK_RESOURCE && (t->owner >= 0 || Sim::invulnerable(*t)) && forceFireKey()) {
        std::vector<Ref> sel; Ref tr = g_sim.refOf(*t);
        for (auto r : selection) if (r != tr) sel.push_back(r);
        g_sim.cmdAttack(sel, tr, !g_sim.enemies(g_sim.humanPlayer, t->owner));
    }
    else if (t && g_sim.enemies(g_sim.humanPlayer, t->owner)) g_sim.cmdAttack(selection, g_sim.refOf(*t));
    else g_sim.cmdMove(selection, w, true);
    g_audio.play(SND_ORDER, Vec2(), true, 0.6f);
}

// A short "what the next drop carries" line for the tooltip, straight from the same manifest the plane itself loads: so the panel always
// shows the army's real, current roster instead of a fixed blurb that goes stale the moment an upgrade finishes.
static std::string rosterText(const std::vector<int>& load, size_t maxChars) {
    std::vector<int> kinds; int cnt[U_COUNT] = {};
    for (int t : load) { if (!cnt[t]++) kinds.push_back(t); }
    std::string s;
    for (size_t i = 0; i < kinds.size(); i++) {
        std::string part = std::to_string(cnt[kinds[i]]) + "x " + UNITS[kinds[i]].name;
        if (!s.empty()) { if (s.size() + part.size() + 2 > maxChars) { s += ", ..."; break; } s += ", "; }
        s += part;
    }
    return s.empty() ? std::string("nothing yet") : s;
}

// Long tooltips wrap onto a few lines instead of running off the screen: the drop roster can list a dozen unit kinds, and the panel is a
// single unwrapped line otherwise.
static std::vector<std::string> wrapTip(Gfx& g, const std::string& s, int maxW) {
    std::vector<std::string> lines;
    std::string cur;
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && s[i] == ' ') i++;
        size_t j = i; while (j < s.size() && s[j] != ' ') j++;
        if (j <= i) break;
        std::string word = s.substr(i, j - i);
        i = j;
        std::string cand = cur.empty() ? word : cur + " " + word;
        if (!cur.empty() && g.textW(cand.c_str()) > maxW) { lines.push_back(cur); cur = word; }
        else cur = cand;
    }
    if (!cur.empty()) lines.push_back(cur);
    return lines;
}

// The footprints of a shift-dragged line: from the one at (bulkTx, bulkTy) toward the one at (tx1, ty1), stepping a whole footprint at a time
// along the longer axis (and the other axis when the line slants), so they sit edge to edge and never overlap. 1x1 walls follow the line
// tile by tile, diagonals included (a diagonal chain still blocks: nothing squeezes between two segments that touch at a corner).
void Game::bulkSpots(int type, int tx1, int ty1, std::vector<std::pair<int, int>>& out) const {
    out.clear();
    const int w = BUILDS[type].w, h = BUILDS[type].h, MAXN = 80;
    int dx = tx1 - bulkTx, dy = ty1 - bulkTy;
    int nx = (std::abs(dx) + w / 2) / w, ny = (std::abs(dy) + h / 2) / h;   // whole footprints along each axis
    int n = std::min(std::max(nx, ny), MAXN - 1);
    int sx = dx < 0 ? -1 : 1, sy = dy < 0 ? -1 : 1;
    for (int k = 0; k <= n; k++) {
        int ix = n > 0 ? (nx * k * 2 + n) / (2 * n) : 0, iy = n > 0 ? (ny * k * 2 + n) / (2 * n) : 0;   // (rounded: the steps spread evenly)
        out.push_back({ bulkTx + sx * ix * w, bulkTy + sy * iy * h });
    }
}

// ------------------------------------------------------------ command buttons
void Game::buildButtons() {
    buildButtonsRaw();
    // five rows do not fit at full height, and six (a dozer's 18 structures: the Bastion Wall, the Waymaker and the Command structure are the sixth row)
    // only fit as dense buttons with a 2 px gap: the bar is 124 px tall. (Six compact rows ran off the bottom of the screen and hid that last row.)
    if ((int)buttons.size() > 18) {   // a dozer's 19 structures (the Healing Antenna joined them): four columns in the same width, five compact rows
        const int w4 = (3 * BTN_W - BTN_GAP) / 4;
        for (size_t i = 0; i < buttons.size(); i++) { buttons[i].x = GRID_X + (int)(i % 4) * (w4 + BTN_GAP); buttons[i].w = w4; buttons[i].y = GRID_Y + (int)(i / 4) * (BTN_H_COMPACT + BTN_GAP); buttons[i].h = BTN_H_COMPACT; }
    }
    else if ((int)buttons.size() > 15) for (size_t i = 0; i < buttons.size(); i++) { buttons[i].y = GRID_Y + (int)(i / 3) * (BTN_H_DENSE + 2); buttons[i].h = BTN_H_DENSE; }
    else if ((int)buttons.size() > 12) for (size_t i = 0; i < buttons.size(); i++) { buttons[i].y = GRID_Y + (int)(i / 3) * (BTN_H_COMPACT + BTN_GAP); buttons[i].h = BTN_H_COMPACT; }
}

void Game::commandBarCheck(bool& onScreen, bool& fwall, bool& way) {
    buildButtons();
    onScreen = true; fwall = way = false;
    for (auto& b : buttons) {
        if (b.y < SCREEN_H - HUD_H || b.y + b.h > SCREEN_H || b.x < 0 || b.x + b.w > SCREEN_W) onScreen = false;
        if (b.kind == BK_BUILD && BUILDS[b.id].role == BR_FWALL) fwall = true;
        if (b.kind == BK_BUILD && BUILDS[b.id].role == BR_WAYMAKER) way = true;
    }
}

void Game::buildButtonsRaw() {
    buttons.clear();
    Player& pl = g_sim.players[g_sim.humanPlayer];
    auto add = [&](int kind, int id, bool enabled, const char* label, const std::string& tip) {
        int i = (int)buttons.size();
        if (i >= BTN_MAX) return;
        Button b; b.x = GRID_X + (i % 3) * (BTN_W + BTN_GAP); b.y = GRID_Y + (i / 3) * (BTN_H + BTN_GAP); b.w = BTN_W; b.h = BTN_H;
        b.kind = kind; b.id = id; b.enabled = enabled; b.label = label; b.tip = tip;
        buttons.push_back(b);
    };
    if (placingType >= 0) { add(BK_CANCEL, 0, true, "Cancel", "Cancel placement (Esc)"); return; }
    if (selection.empty()) return;
    Entity* b = selectedBuilding();
    char tip[640];
    if (b) {
        if (b->district >= 0) return;   // a block of a town round the lake: held, never sold, nothing to build there
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
            const DropType& dr = DROPS[pl.faction];
            int dtier = g_sim.dropTier(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "%s  [P]  %s  Now loading (%s, tier %d of %d): %s", dr.name, dr.desc, DROP_TIERS[dtier].name, dtier + 1, DTI_COUNT, rosterText(g_sim.dropManifest(g_sim.humanPlayer), 150).c_str());
            add(BK_DROP, 0, g_sim.dropsReady(g_sim.humanPlayer) > 0, dr.name, tip);
            const StrikeType& st = STRIKES[pl.faction];
            std::vector<int> answer = g_sim.strikeManifest(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "%s  [I]  %s  (%s)", st.name, st.desc, rosterText(answer, 120).c_str());
            add(BK_AIRSTRIKE, 0, g_sim.strikesReady(g_sim.humanPlayer) > 0, st.name, tip);
            const FleetType& ft = FLEETS[pl.faction];
            snprintf(tip, sizeof tip, "%s  [L]  %s (%d s now)", ft.name, ft.desc, (int)DROP_TIERS[g_sim.dropTier(g_sim.humanPlayer)].cooldown);
            add(BK_FLEET, 0, g_sim.fleetsReady(g_sim.humanPlayer) > 0, ft.name, tip);
            int bt = pl.faction == F_CYBER ? U_C_NUKEBOMBER : U_K_NUKEBOMBER;
            snprintf(tip, sizeof tip, "Nuclear Bomber  [B]  A %s strategic bomber comes in from the map corner behind your base, flies straight to the spot you pick, drops one mid-sized nuclear bomb (%.1f tile blast, leaves radiation) and flies on off the map. Strong anti-air can shoot it down before the drop. One call per %d s per tech structure%s",
                     UNITS[bt].name, NUKE_RADIUS * std::sqrt(NUKEBOMBER_SCALE), (int)NUKEBOMBER_COOLDOWN, g_sim.time < NUKEBOMBER_OPEN ? "  (available a few minutes into the match)" : "");
            add(BK_NUKEBOMBER, 0, g_sim.bombersReady(g_sim.humanPlayer) > 0, "Nuclear Bomber", tip);
            snprintf(tip, sizeof tip, "Aid Drop  [D]  A white relief plane nobody shoots at drops $%d and a Dozer in crates: the army nearest the spot gets them, ally or enemy, never you (%d s cooldown per tech structure)", AID_MONEY, (int)AID_COOLDOWN);
            add(BK_AID, 0, g_sim.aidsReady(g_sim.humanPlayer) > 0, "Aid Drop", tip);
            for (int u = 0; u < UPG_COUNT; u++) {
                const UpgradeType& ug = UPGRADES[pl.faction][u];
                if (pl.upg[u]) snprintf(tip, sizeof tip, "%s: done. %s", ug.name, ug.desc);
                else if (pl.upgBusy[u]) snprintf(tip, sizeof tip, "%s in progress (%d%%): %s", ug.name, (int)(pl.upgProgress[u] * 100), ug.desc);
                else snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s. Lasts the whole match", ug.name, ug.cost, ug.hotkey, ug.desc);
                add(BK_UPGRADE, u, g_sim.upgradeAvailable(g_sim.humanPlayer, u) && g_sim.canAfford(g_sim.humanPlayer, ug.cost), ug.name, tip);
            }
        }
        if (role == BR_NUKE) {
            int rdy = g_sim.nukesReady(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "Launch Nuke  [K]  Obliterates a %d tile radius and leaves lethal radiation for over a minute. One warhead per ramp every 5 minutes%s", (int)NUKE_RADIUS, pl.lowPower() ? "  (LOW POWER)" : "");
            add(BK_NUKE, 0, rdy > 0, "Launch Nuke", tip);
        }
        if (role == BR_UNUKE) {
            float dw = g_sim.deepNukeWait(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "Launch Underground Nuke  [K]  A silver warhead with three times a Nuke Ramp's blast radius (%d tiles), damage and radiation (%d s). Only Anti-Nuke Lasers can shoot it down. One warhead per silo every 5 minutes%s", (int)(NUKE_RADIUS * std::sqrt(UNUKE_SCALE)), (int)(Sim::FALLOUT_LIFE * UNUKE_FALLOUT), pl.lowPower() ? "  (LOW POWER)" : (dw > 0 ? "  (arming)" : ""));
            add(BK_DEEPNUKE, 0, g_sim.deepNukesReady(g_sim.humanPlayer) > 0, "Underground Nuke", tip);
        }
        if (role == BR_STRONGHOLD) {   // a claimed stronghold is held, not sold: its garrison can be let out
            if (b->owner != g_sim.humanPlayer) return;
            snprintf(tip, sizeof tip, "Unload  [U]  The garrison climbs out (%d of %d soldiers inside)", g_sim.garrisonInfantry(*b), g_sim.garrisonCap(*b));
            add(BK_UNLOAD, 0, !b->passengers.empty(), "Unload", tip);
            return;
        }
        if (role == BR_ISLANDNUKE) {   // a captured island launcher: it only launches (nothing sells, repairs or upgrades it)
            if (b->owner != g_sim.humanPlayer) return;
            float iw = g_sim.islandNukeWait(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "Launch Super Nuke  [K]  The biggest warhead there is: twice an Underground Nuke's blast (%.0f tile radius, leaves radiation), anywhere on the map, landing in %.1f s. Bunkers and interceptor missiles cannot touch it and a laser's chance is a fifth of its chance against an Underground Nuke. It hurts your own army too if it is in the circle. One launch per launcher every %d s. Nothing destroys the launcher: only an enemy infiltrator taking it back stops it%s",
                     NUKE_RADIUS * std::sqrt(ISLAND_NUKE_SCALE), ISLAND_NUKE_FLIGHT, (int)ISLAND_NUKE_COOLDOWN, iw > 0 ? "  (reloading)" : "");
            add(BK_ISLANDNUKE, 0, g_sim.islandNukesReady(g_sim.humanPlayer) > 0, "Super Nuke", tip);
            return;
        }
        if (role == BR_BUNKER) {
            float mw = g_sim.miniNukeWait(g_sim.humanPlayer);
            snprintf(tip, sizeof tip, "Launch Mini Nuke  [K]  A bunker's own missile, a fifth of a full warhead (%.1f tile radius, same strength per spot, leaves radiation). Needs no power or ramp; one per bunker every %d s%s", NUKE_RADIUS * std::sqrt(MINI_NUKE_SCALE), (int)MINI_NUKE_COOLDOWN, mw > 0 ? "  (reloading)" : "");
            add(BK_MININUKE, 0, g_sim.miniNukesReady(g_sim.humanPlayer) > 0, "Mini Nuke", tip);
            snprintf(tip, sizeof tip, "Unload  [U]  Everybody climbs out onto the ground (%d of %d soldiers, %d of %d dozers inside)", g_sim.garrisonInfantry(*b), BUNKER_INF_CAP, g_sim.garrisonDozers(*b), BUNKER_DOZER_CAP);
            add(BK_UNLOAD, 0, !b->passengers.empty(), "Unload", tip);
            add(BK_RALLY, 0, true, "Rally", "Set the point soldiers walk to when they climb out (right-click ground while selected)");
            add(BK_SELL, 0, true, "Sell", "Sell this structure for 50% of its cost");
            if (b->owner != g_sim.humanPlayer) return;
            for (int u = 0; u < BU_COUNT; u++) {   // this bunker's own upgrades (none of them carries over to another bunker); fitted ones leave the bar
                const BunkerUpgType& ug = BUNKER_UPGS[u];
                if (Sim::bunkerHas(*b, u)) continue;
                if (b->bunkerBusy == u) snprintf(tip, sizeof tip, "%s: fitting (%d%%). %s", ug.name, (int)(b->bunkerProgress * 100), ug.desc);
                else snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s. For THIS bunker only (%d s to fit, one fitting at a time)%s", ug.name, ug.cost, ug.hotkey, ug.desc, (int)ug.time,
                              ((b->bunkerQueue >> u) & 1) ? "  (queued by Upgrade All Bunkers)" : (b->bunkerBusy >= 0 ? "  (busy fitting another upgrade)" : ""));
                add(BK_BUNKERUPG, u, g_sim.bunkerUpgAvailable(*b, u) && g_sim.canAfford(g_sim.humanPlayer, ug.cost), ug.name, tip);
            }
            // the bunkers' calls work like the Mini Nuke: any ready bunker of the army answers (this one first), so pressing the key again calls the next
            char ready[64];
            auto readyText = [&](int u) { int fit = 0, n = g_sim.bunkerPowersReady(g_sim.humanPlayer, u, &fit); snprintf(ready, sizeof ready, "  (%d of %d bunker%s ready)", n, fit, fit == 1 ? "" : "s"); return n; };
            int fitP = 0, fitR = 0, fitT = 0;
            g_sim.bunkerPowersReady(g_sim.humanPlayer, BU_PARA, &fitP); g_sim.bunkerPowersReady(g_sim.humanPlayer, BU_RAIN, &fitR); g_sim.bunkerPowersReady(g_sim.humanPlayer, BU_TANKS, &fitT);
            if (fitP > 0) {
                int n = readyText(BU_PARA);
                snprintf(tip, sizeof tip, "Call Para Backup  [P]  A cargo plane drops %d paratroopers on the spot you pick (%s). Once a minute per bunker: any of your bunkers with a radio mast answers, so press P again for the next one%s", BUNKER_PARA_TROOPS, rosterText(g_sim.bunkerParaManifest(g_sim.humanPlayer), 110).c_str(), ready);
                add(BK_BUNKERPARA, 0, n > 0, "Call Paratroops", tip);
            }
            if (fitR > 0) {
                int n = readyText(BU_RAIN);
                snprintf(tip, sizeof tip, "Rain Nukes  [J]  Pick a wide circle (%d tiles): %d small warheads (%.1f tile blast each, leave radiation) come down at random spots in it, %.1f s after launch. Almost nothing can shoot them down (%d%% of a defence's usual chance). They hit your own units in the circle too. Once every %d s per bunker: any of your bunkers with a rain rack answers, so press J again for the next one%s",
                         (int)RAIN_NUKE_AREA, RAIN_NUKES, NUKE_RADIUS * std::sqrt(RAIN_NUKE_SCALE), Sim::NUKE_FLIGHT * RAIN_NUKE_FLIGHT, (int)(RAIN_NUKE_RESIST * 100 + 0.5f), (int)RAIN_NUKE_COOLDOWN, ready);
                add(BK_BUNKERRAIN, 0, n > 0, "Rain Nukes", tip);
            }
            if (fitT > 0) {
                int n = readyText(BU_TANKS);
                snprintf(tip, sizeof tip, "Tank Drop  [D]  A heavy cargo plane drops %d tanks and a medic vehicle on the spot you pick (%s). Tougher than other airlifts, but heavy flak can still bring it down. Once every %d s per bunker: any of your bunkers with a lift pad answers, so press D again for the next one%s",
                         TANK_DROP_TANKS, rosterText(g_sim.bunkerTankManifest(g_sim.humanPlayer), 110).c_str(), (int)TANK_DROP_COOLDOWN, ready);
                add(BK_TANKDROP, 0, n > 0, "Tank Drop", tip);
            }
            {   // Upgrade All Bunkers: every upgrade every bunker of the army still lacks, queued from this one bunker
                int cost = 0, open = g_sim.bunkerUpgradesMissing(g_sim.humanPlayer, &cost, true), allCost = 0, all = g_sim.bunkerUpgradesMissing(g_sim.humanPlayer, &allCost);
                if (open > 0 || all == 0) {
                    snprintf(tip, sizeof tip, "Upgrade All Bunkers  [A]  Update all upgrades for all bunkers: queues every upgrade your bunkers still lack (%d fitting%s, $%d in all). Each bunker fits its queue one upgrade after another and pays as each one starts; a fitting waits while money is short",
                             open, open == 1 ? "" : "s", cost);
                    add(BK_BUNKERALL, 0, open > 0, "Upgrade All Bunkers", all == 0 ? "Upgrade All Bunkers  [A]  Every bunker you have is fully upgraded" : tip);
                } else {
                    snprintf(tip, sizeof tip, "Cancel Queued Upgrades  [A]  Upgrade All Bunkers has queued every missing upgrade ($%d still to pay). Click to drop the queue (fittings under way go on)", allCost);
                    add(BK_BUNKERALL, 1, true, "Cancel Queued Upgrades", tip);
                }
            }
            return;
        }
        if (role == BR_BARRACKS || role == BR_FACTORY || role == BR_AIRFIELD || role == BR_SUPPLY || role == BR_NAVAL) add(BK_RALLY, 0, true, "Rally", role == BR_NAVAL ? "Set the point ships sail to (right-click the lake while the yard is selected)" : "Set rally point (right-click ground while selected)");
        add(BK_SELL, 0, true, "Sell", "Sell this structure for 50% of its cost");
        return;
    }
    if (selectionHasRole(UR_DOZER)) {
        bool civ = true;   // only civilian dozers in the selection: the town; any other dozer: the base
        for (auto r : selection) { Entity* d = g_sim.get(r); if (d && d->isUnit() && d->ut().role == UR_DOZER && !civicDozerType(d->type)) civ = false; }
        // the Command Core / Post comes last so the other buttons keep their places
        for (int pass = 0; pass < 2; pass++) for (int t = 0; t < B_COUNT; t++) {
            const BuildType& bt = BUILDS[t];
            if (bt.faction != pl.faction || neutralRole(bt.role) || (bt.role == BR_CIVIC) != civ || (bt.role == BR_HQ) != (pass == 1)) continue;
            bool ok = g_sim.buildAvailable(g_sim.humanPlayer, t);
            bool lim = g_sim.atBuildLimit(g_sim.humanPlayer, t);
            snprintf(tip, sizeof tip, "%s  $%d  [%s]  %s%s%s", bt.name, bt.cost, bt.hotkey, bt.desc, ok ? "" : (lim ? "  Limit reached" : "  Requires "), ok || lim ? "" : BUILDS[bt.requires_].name);
            add(BK_BUILD, t, ok, bt.name, tip);
        }
        return;
    }
    bool combat = false, lifter = false, jeep = false;
    bool cargoShip = false;
    for (auto r : selection) { Entity* e = g_sim.get(r); if (e && e->isUnit() && e->weapon() >= 0) combat = true; if (e && e->isUnit() && e->ut().cargoCap > 0) (e->ut().kind == UK_AIR ? lifter : (e->ut().naval ? cargoShip : jeep)) = true; }
    if (cargoShip) {
        bool aboard = false; for (auto r : selection) { Entity* e = g_sim.get(r); if (e && !e->passengers.empty()) aboard = true; }
        add(BK_UNLOAD, 0, aboard, "Land Here", "Land Here  [U]  Everybody aboard goes ashore onto the nearest beach (it must lie within a few tiles). Load the ship by bringing it beside a shore and right-clicking it with units selected");
        add(BK_DROPOFF, 0, aboard, "Land At", "Land At  [D]  Pick a beach: the ship sails to the water off it and lands everybody there");
    } else if (lifter) {
        add(BK_UNLOAD, 0, true, "Unload", "Unload  [U]  Set down where it hovers and let everybody out. Load a lifter by right-clicking it with soldiers or vehicles selected");
        add(BK_DROPOFF, 0, true, "Drop Off", "Drop Off  [D]  Pick a spot: the lifter flies there, sets down and unloads");
    } else if (jeep) {
        bool aboard = false; for (auto r : selection) { Entity* e = g_sim.get(r); if (e && !e->passengers.empty()) aboard = true; }
        add(BK_UNLOAD, 0, aboard, "Unload", "Unload  [U]  The jeep stops and everybody climbs out. Load it by right-clicking it with soldiers selected: seven seats, and the soldiers shoot from inside");
    }
    bool haul = selectionHasRole(UR_HARVESTER);
    if (combat) add(BK_ATTACKMOVE, 0, true, "Attack Move", "Attack-move: engage everything on the way  [A]");
    if (combat) add(BK_FORCE, 0, true, "Force Fire", "Force fire: click any unit or structure, your own or an ally's included, to attack it on purpose (or hold Ctrl and right-click). Allies never shoot back  [F]");
    if (combat || haul) add(BK_AREA, 0, true, combat && haul ? "Guard/Gather" : (haul ? "Gather Area" : "Guard Area"),
                            haul && !combat ? "Gather Area: haulers search a circle you pick for supplies and collect them  [G]"
                                            : "Guard Area: pick a circle (click, or drag to size it); units protect it, aircraft patrol it  [G]");
    add(BK_STOP, 0, true, "Stop", "Stop and hold position  [S]");
}

bool Game::launchNuke(Vec2 at) {
    if (rainBunker.valid()) {
        if (!g_sim.cmdBunkerRain(g_sim.readyBunker(g_sim.humanPlayer, BU_RAIN, rainBunker), at, forceFireKey())) return false;   // (another one if that one was knocked offline meanwhile)
        addMessage("Rain Nukes launched", rgb(255, 120, 90));
        return true;
    }
    if (nukeDeep) {
        Ref from = NOREF;
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o && o->isBuilding() && o->bt().role == BR_UNUKE && o->owner == g_sim.humanPlayer && o->constructed && g_sim.time >= o->actionTimer) { from = r; break; } }
        if (!g_sim.cmdDeepNuke(g_sim.humanPlayer, at, forceFireKey(), from)) return false;
        addMessage("Underground nuke launched", rgb(214, 218, 226));
        return true;
    }
    if (nukeIsland) {
        Ref from = NOREF;
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o && o->isBuilding() && o->bt().role == BR_ISLANDNUKE && o->owner == g_sim.humanPlayer && g_sim.time >= o->siloTimer) { from = r; break; } }
        if (!g_sim.cmdIslandNuke(g_sim.humanPlayer, at, forceFireKey(), from)) return false;
        addMessage("Island nuke launched", rgb(255, 130, 100));
        return true;
    }
    if (nukeMini) {
        Ref from = NOREF;
        for (auto r : selection) { Entity* o = g_sim.get(r); if (o && o->isBuilding() && o->bt().role == BR_BUNKER && o->owner == g_sim.humanPlayer && o->constructed && g_sim.time >= o->siloTimer) { from = r; break; } }
        if (!g_sim.cmdMiniNuke(g_sim.humanPlayer, at, forceFireKey(), from)) return false;
        addMessage("Mini nuke launched", rgb(255, 120, 90));
        return true;
    }
    if (!g_sim.cmdNuke(g_sim.humanPlayer, at, forceFireKey())) return false;
    addMessage("Nuclear missile launched", rgb(255, 120, 90));
    return true;
}

void Game::cancelModes() {
    placingType = -1; attackMoveMode = false; forceMode = false; forceLatch = false; powerMode = false; nukeMode = false; nukeMini = false; nukeDeep = false; nukeIsland = false; bomberMode = false; dropMode = false; strikeMode = false; fleetMode = false; aidMode = false; rallyMode = false; areaMode = false; areaDrag = false; dropoffMode = false; bulkDrag = false; paraBunker = NOREF; tankBunker = NOREF; rainBunker = NOREF;
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
    case BK_POWER: if (g_sim.time >= pl.powerReady && g_sim.techOnline(g_sim.humanPlayer)) { powerMode = true; placingType = -1; attackMoveMode = false; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_NUKE: if (g_sim.nukesReady(g_sim.humanPlayer) > 0) { cancelModes(); nukeMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_MININUKE: if (g_sim.miniNukesReady(g_sim.humanPlayer) > 0) { cancelModes(); nukeMode = true; nukeMini = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_DEEPNUKE: if (g_sim.deepNukesReady(g_sim.humanPlayer) > 0) { cancelModes(); nukeMode = true; nukeDeep = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_DROP: if (g_sim.dropsReady(g_sim.humanPlayer) > 0) { cancelModes(); dropMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_AIRSTRIKE: if (g_sim.strikesReady(g_sim.humanPlayer) > 0) { cancelModes(); strikeMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_FLEET: if (g_sim.fleetsReady(g_sim.humanPlayer) > 0) { cancelModes(); fleetMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_ISLANDNUKE: if (g_sim.islandNukesReady(g_sim.humanPlayer) > 0) { cancelModes(); nukeMode = true; nukeIsland = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_NUKEBOMBER: if (g_sim.bombersReady(g_sim.humanPlayer) > 0) { cancelModes(); bomberMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_AID: if (g_sim.aidsReady(g_sim.humanPlayer) > 0) { cancelModes(); aidMode = true; g_audio.play(SND_CLICK, Vec2(), true); } else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_SCAN: if (g_sim.cmdScan(g_sim.humanPlayer)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_RESEARCH: if (g_sim.cmdResearch(g_sim.humanPlayer)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_UPGRADE: if (g_sim.cmdUpgrade(g_sim.humanPlayer, id)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break;
    case BK_BUNKERUPG: { Entity* b = selectedBuilding(); if (b && g_sim.cmdBunkerUpgrade(g_sim.refOf(*b), id)) g_audio.play(SND_CLICK, Vec2(), true); else g_audio.play(SND_CANT, Vec2(), true); break; }
    case BK_BUNKERRAIN: case BK_TANKDROP: case BK_BUNKERPARA: {   // (the selected bunker if it is ready, else any ready one: see Sim::readyBunker)
        Entity* b = selectedBuilding();
        int u = kind == BK_BUNKERRAIN ? BU_RAIN : kind == BK_TANKDROP ? BU_TANKS : BU_PARA;
        Ref r = g_sim.readyBunker(g_sim.humanPlayer, u, b ? g_sim.refOf(*b) : NOREF);
        if (!r.valid()) { g_audio.play(SND_CANT, Vec2(), true); break; }
        cancelModes();
        if (u == BU_RAIN) { nukeMode = true; rainBunker = r; } else { dropMode = true; (u == BU_TANKS ? tankBunker : paraBunker) = r; }
        g_audio.play(SND_CLICK, Vec2(), true);
        break;
    }
    case BK_BUNKERALL:
        if (id == 1) { g_sim.cancelBunkerQueue(g_sim.humanPlayer); g_audio.play(SND_CLICK, Vec2(), true); }
        else if (g_sim.cmdBunkerUpgradeAll(g_sim.humanPlayer) > 0) g_audio.play(SND_CLICK, Vec2(), true);
        else g_audio.play(SND_CANT, Vec2(), true);
        break;
    case BK_ATTACKMOVE: attackMoveMode = true; forceMode = false; areaMode = false; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_FORCE: cancelModes(); forceMode = true; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_AREA: areaMode = true; areaDrag = false; placingType = -1; attackMoveMode = false; powerMode = false; rallyMode = false; g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_STOP: g_sim.cmdStop(selection); g_audio.play(SND_CLICK, Vec2(), true); break;
    case BK_UNLOAD: {
        int n = 0;
        for (auto r : selection) { Entity* e = g_sim.get(r); if (e && !e->passengers.empty()) { g_sim.cmdUnload(r, false, Vec2()); n++; } }
        g_audio.play(n ? SND_CLICK : SND_CANT, Vec2(), true);
        break;
    }
    case BK_DROPOFF: cancelModes(); dropoffMode = true; g_audio.play(SND_CLICK, Vec2(), true); break;
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
    char c = (char)toupper((int)k);
    if (powerMode && c == 'F') { forceLatch = !forceLatch; g_audio.play(SND_CLICK, Vec2(), true); addMessage(forceLatch ? "Friendly fire ON: the blast hits your own and allied units too" : "Friendly fire off", rgb(255, 150, 110)); return; }
    buildButtons();
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
            if (placingType >= 0 || attackMoveMode || forceMode || powerMode || nukeMode || dropMode || strikeMode || fleetMode || bomberMode || aidMode || rallyMode || areaMode || dropoffMode) cancelModes();
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
                if (!o.alive || !o.isUnit() || o.owner != g_sim.humanPlayer || o.ut().role != UR_COMBAT || o.carrier.valid()) continue;
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
                else if (powerMode) { if (g_sim.cmdPower(g_sim.humanPlayer, wp, forceFireKey())) { powerMode = false; forceLatch = false; } }
                else if (nukeMode) { if (launchNuke(wp)) { nukeMode = false; nukeMini = false; nukeDeep = false; nukeIsland = false; rainBunker = NOREF; bomberMode = false; forceLatch = false; } }
                else if (dropoffMode) { for (auto r : selection) { Entity* o = g_sim.get(r); if (o && !o->passengers.empty()) g_sim.cmdUnload(r, true, wp); } dropoffMode = false; }
                else if (dropMode) { if (tankBunker.valid() ? g_sim.cmdTankDrop(g_sim.readyBunker(g_sim.humanPlayer, BU_TANKS, tankBunker), wp) : paraBunker.valid() ? g_sim.cmdBunkerPara(g_sim.readyBunker(g_sim.humanPlayer, BU_PARA, paraBunker), wp) : g_sim.cmdParadrop(g_sim.humanPlayer, wp)) { dropMode = false; paraBunker = NOREF; tankBunker = NOREF; } else g_audio.play(SND_CANT, Vec2(), true); }
                else if (strikeMode) { if (g_sim.cmdPhoneStrike(g_sim.humanPlayer, wp)) strikeMode = false; else g_audio.play(SND_CANT, Vec2(), true); }
                else if (fleetMode) { if (g_sim.cmdFleetCall(g_sim.humanPlayer, wp)) fleetMode = false; else g_audio.play(SND_CANT, Vec2(), true); }
                else if (bomberMode) { if (g_sim.cmdNukeBomber(g_sim.humanPlayer, wp)) bomberMode = false; else g_audio.play(SND_CANT, Vec2(), true); }
                else if (aidMode) { if (g_sim.cmdAidDrop(g_sim.humanPlayer, wp)) aidMode = false; else g_audio.play(SND_CANT, Vec2(), true); }
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
                if (SDL_GetModState() & KMOD_SHIFT) { bulkDrag = true; bulkTx = tx; bulkTy = ty; return; }   // shift: drag out a line of them, placed when the button comes up
                Ref dozer;
                bool way = BUILDS[placingType].role == BR_WAYMAKER;   // (any military dozer places one: it clears its ground by itself)
                for (auto r : selection) { Entity* d = g_sim.get(r); if (d && d->isUnit() && d->ut().role == UR_DOZER && (way ? !civicDozerType(d->type) : Sim::canRaise(*d, placingType))) { dozer = r; break; } }
                if (dozer.valid() && g_sim.cmdBuild(dozer, placingType, tx, ty)) { if (!(SDL_GetModState() & KMOD_SHIFT) && !way) placingType = -1; }   // (Waymakers are laid in chains: the mode stays on until Esc)
                else g_audio.play(SND_CANT, Vec2(), true);
                return;
            }
            if (powerMode) { if (g_sim.cmdPower(g_sim.humanPlayer, w, forceFireKey())) { powerMode = false; forceLatch = false; } else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (nukeMode) { if (!launchNuke(w)) g_audio.play(SND_CANT, Vec2(), true); nukeMode = false; nukeMini = false; nukeDeep = false; nukeIsland = false; rainBunker = NOREF; bomberMode = false; forceLatch = false; return; }
            if (dropMode) { if (tankBunker.valid() ? g_sim.cmdTankDrop(g_sim.readyBunker(g_sim.humanPlayer, BU_TANKS, tankBunker), w) : paraBunker.valid() ? g_sim.cmdBunkerPara(g_sim.readyBunker(g_sim.humanPlayer, BU_PARA, paraBunker), w) : g_sim.cmdParadrop(g_sim.humanPlayer, w)) { dropMode = false; paraBunker = NOREF; tankBunker = NOREF; } else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (strikeMode) { if (g_sim.cmdPhoneStrike(g_sim.humanPlayer, w)) strikeMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (fleetMode) { if (g_sim.cmdFleetCall(g_sim.humanPlayer, w)) fleetMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (bomberMode) { if (g_sim.cmdNukeBomber(g_sim.humanPlayer, w)) bomberMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (aidMode) { if (g_sim.cmdAidDrop(g_sim.humanPlayer, w)) aidMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (dropoffMode) { for (auto r : selection) { Entity* o = g_sim.get(r); if (o && !o->passengers.empty()) g_sim.cmdUnload(r, true, w); } dropoffMode = false; g_audio.play(SND_ORDER, Vec2(), true, 0.6f); return; }
            if (attackMoveMode) { issueAttackMove(w); attackMoveMode = false; return; }
            if (forceMode) { if (issueForceFire(w)) forceMode = false; else g_audio.play(SND_CANT, Vec2(), true); return; }
            if (rallyMode) { Entity* b = selectedBuilding(); if (b) g_sim.cmdSetRally(g_sim.refOf(*b), w); rallyMode = false; return; }
            if (areaMode) { areaDrag = true; areaStart = w; areaRadius = 0; return; }
            dragging = true; dragStart = w; dragNow = w;
            return;
        }
        if (btn == SDL_BUTTON_RIGHT) {
            if (placingType >= 0 || attackMoveMode || forceMode || powerMode || nukeMode || dropMode || strikeMode || fleetMode || bomberMode || aidMode || rallyMode || areaMode || dropoffMode) { cancelModes(); return; }
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
        if (e.button.button == SDL_BUTTON_LEFT && bulkDrag) {
            bulkDrag = false;
            if (placingType >= 0) {
                std::vector<std::pair<int, int>> spots;
                bulkSpots(placingType, tileOf(w.x) - BUILDS[placingType].w / 2, tileOf(w.y) - BUILDS[placingType].h / 2, spots);
                int n = g_sim.cmdBuildLine(selection, placingType, spots);
                if (n <= 0) g_audio.play(SND_CANT, Vec2(), true);
                else if (n > 1) { char m[64]; snprintf(m, sizeof m, "%d x %s placed", n, BUILDS[placingType].name); addMessage(m, hudDim()); }
                if (n > 0 && !(SDL_GetModState() & KMOD_SHIFT) && BUILDS[placingType].role != BR_WAYMAKER) placingType = -1;   // (still holding shift: keep laying lines)
            }
        }
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
        const int mx = ISLE_X0 * TILE + 400, my = ISLE_X0 * TILE + 640;   // (a corner of the main island)
        g.drawWorld(mx, my, std::min(SCREEN_W, WORLD_W - mx), std::min(SCREEN_H, WORLD_H - my), 0, 0);
    }
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(10, 14, 20, 170));
    Color accent = hudAccent(menu.playerFaction);
    int cx = SCREEN_W / 2, oy = menuOffsetY();
    g.fill(0, 110 + oy, SCREEN_W, 4, accent);
    g.text(cx - g.textW("ONE HOUR", 6) / 2, 40 + oy, "ONE HOUR", rgb(240, 244, 248), 6);
    g.text(cx - g.textW("Skirmish: Cyber Army vs Clanker Army", 2) / 2, 130 + oy, "Skirmish: Cyber Army vs Clanker Army", hudDim(), 2);
    const char* fdesc = menu.playerFaction == F_CYBER
        ? "Lasers, railguns, volt coils, EMP Strike, Orbital Scan, Airlift Drop, Ion Lancers, Aegis Titans and Specter Jets."
        : "Diesel tanks, gatling guns, Shell Storm, Recon Flight, Paradrop, Grenadiers, Behemoths and Talon Jets.";
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
        // scan running: no shroud and no tint over the picture (it is lit as if by day, see ambientTint), only a thin sweep line, and a
        // blinking frame for the last ten seconds
        Color ac = hudAccent(g_sim.players[g_sim.humanPlayer].faction);
        float left = g_sim.players[g_sim.humanPlayer].revealUntil - g_sim.time;
        float sweep = std::fmod(wallTime * 0.30f, 1.0f) * VIEW_H;
        g.fill(0, (int)sweep, SCREEN_W, 1, Color{ac.r, ac.g, ac.b, 60});
        if (left < 10.0f && ((int)(wallTime * 3.0f) & 1)) { Color fc{ac.r, ac.g, ac.b, 150}; g.fill(0, 0, SCREEN_W, 2, fc); g.fill(0, VIEW_H - 2, SCREEN_W, 2, fc); g.fill(0, 0, 2, VIEW_H, fc); g.fill(SCREEN_W - 2, 0, 2, VIEW_H, fc); }
        return;
    }
    // the fog of war in three levels per tile: never seen, remembered (explored, nobody there now), in sight. The mask is rebuilt (only where it
    // changed) when the sight was refreshed, at most about 8 times a second: the cloudy edge costs a few ms
    static u32 lastGen = ~0u; static int lastPlayer = -1;
    static float nextShroud = 0, lastSimTime = 1e30f;
    const bool newMatch = g_sim.time < lastSimTime;   // (the clock ran back: another match on the same map)
    lastSimTime = g_sim.time;
    if ((g_sim.sightGen != lastGen && wallTime >= nextShroud) || lastPlayer != g_sim.humanPlayer || newMatch) {
        const std::vector<u8>& ex = g_sim.players[g_sim.humanPlayer].explored;
        static std::vector<u8> fog;
        fog.resize((size_t)MAP_W * MAP_H);
        const bool sight = !g_sim.inSight.empty();
        for (size_t i = 0; i < fog.size(); i++) fog[i] = !ex[i] ? 0 : (sight && g_sim.inSight[i] ? 2 : 1);
        g.updateShroud(fog, newMatch || lastPlayer != g_sim.humanPlayer);
        lastGen = g_sim.sightGen; lastPlayer = g_sim.humanPlayer; nextShroud = wallTime + 0.12f;
    }
    int tx0 = std::max(0, tileOf(cam.x)), ty0 = std::max(0, tileOf(cam.y));
    int tx1 = std::min(MAP_W - 1, tileOf(cam.x + SCREEN_W)), ty1 = std::min(MAP_H - 1, tileOf(cam.y + VIEW_H));
    const int SS = Gfx::SHROUD_SS;
    SDL_Rect src = { tx0 * SS, ty0 * SS, (tx1 - tx0 + 1) * SS, (ty1 - ty0 + 1) * SS };
    SDL_FRect dst = { tx0 * TILE - cam.x, ty0 * TILE - cam.y, (float)(tx1 - tx0 + 1) * TILE, (float)(ty1 - ty0 + 1) * TILE };
    g.copyScaled(g.shroudTex, &src, dst);
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
        } else if (e->isBuilding() && e->constructed && e->bt().role == BR_ANTINUKE) {
            const BuildType& bt = e->bt();
            bool powered = !(pl.lowPower() && bt.power < 0) && e->disabledUntil <= g_sim.time;
            drawCoverRing(sp, (bt.faction == F_CYBER ? ANTINUKE_COVER_C : ANTINUKE_COVER_K) * TILE, hudAccent(bt.faction), powered, powered ? "Anti-nuke cover" : "Anti-nuke cover: NO POWER");
        } else if (e->isBuilding() && e->constructed && e->bt().role == BR_BUNKER) {
            const BuildType& bt = e->bt();
            rangeRing(sp, WEAPONS[W_BUNKER_MINIGUN].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, W_BUNKER_MINIGUN, "Miniguns", e->hatch > 0 && e->disabledUntil <= g_sim.time, false, false);
        } else if (e->isBuilding() && g_sim.roofGun(*e) >= 0) {
            const BuildType& bt = e->bt();
            rangeRing(sp, WEAPONS[g_sim.roofGun(*e)].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, g_sim.roofGun(*e), WEAPONS[g_sim.roofGun(*e)].name, e->disabledUntil <= g_sim.time, false, false);
        } else if (e->isUnit() && e->ut().role == UR_DECON) {
            drawCoverRing(sp, RADTRUCK_RADIUS * TILE, rgb(150, 240, 180), e->disabledUntil <= g_sim.time, "Decontamination field");
        } else if (e->isUnit() && e->weapon() >= 0 && fewUnits <= 3) {
            rangeRing(sp, WEAPONS[e->weapon()].range * TILE + e->radius(), e->weapon(), nullptr, true, false, true);
        }
    }
    // a defense under the cursor: show what it covers (enemy ones in red)
    if (placingType < 0 && !areaMode && mouseY < VIEW_H) {
        Entity* h = pickEntity(screenToWorld(mouseX, mouseY), false);
        if (h && h->isBuilding() && h->constructed && h->bt().role == BR_ANTINUKE && std::find(selection.begin(), selection.end(), g_sim.refOf(*h)) == selection.end()) {
            bool mine = !g_sim.enemies(g_sim.humanPlayer, h->owner);
            drawCoverRing(worldToScreen(h->pos), (h->bt().faction == F_CYBER ? ANTINUKE_COVER_C : ANTINUKE_COVER_K) * TILE, mine ? hudAccent(h->bt().faction) : rgb(255, 90, 80), true, mine ? "Anti-nuke cover" : "Enemy anti-nuke cover");
        }
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
// The townsfolk of a civilian structure: decoration only (they are not units, nobody can shoot them). They stroll in pairs along a walkway round
// the front of the building (on the plaza of a Town Square or an Event Hall), stop now and then to chat (a speech bubble over one, then the other)
// and walk on; while the building is under fire they hurry and nobody stops to talk. Positions come from the clock and a hash, so nothing is stored.
void Game::drawTownsfolk(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    int k = civicKind(e.type), n = g_sim.civicPeople(e);
    if (k < 0 || n <= 0 || p.x < -160 || p.y < -160 || p.x > SCREEN_W + 160 || p.y > VIEW_H + 160) return;
    const BuildType& bt = e.bt();
    float fw = bt.w * TILE, fh = bt.h * TILE, x0 = p.x - fw * 0.5f, y0 = p.y - fh * 0.5f;
    bool plaza = k == CV_SQUARE || k == CV_EVENTHALL;
    bool panic = g_sim.time - e.lastDamaged < 4.0f || (e.district >= 0 && g_sim.time - g_sim.districts[e.district].lastHit < 6.0f);   // (a whole town takes cover when any of it is hit)
    // the walkway: a U round the west side, the front and the east side, just outside the footprint (on a plaza each pair wanders its own loop)
    const float out = 7.0f, side = fh * 0.55f + out, front = fw + 2 * out, L = 2 * side + front;
    auto walk = [&](float u, Vec2& dir) -> Vec2 {   // u: distance along the walkway, 0..L
        if (u < side) { dir = Vec2(0, 1); return Vec2(x0 - out, y0 + fh - side + out + u); }
        if (u < side + front) { dir = Vec2(1, 0); return Vec2(x0 - out + (u - side), y0 + fh + out); }
        dir = Vec2(0, -1); return Vec2(x0 + fw + out, y0 + fh + out - (u - side - front));
    };
    static const Color SHIRT[8] = { rgb(220, 70, 60), rgb(70, 120, 220), rgb(240, 200, 70), rgb(90, 180, 110), rgb(236, 236, 236), rgb(160, 90, 200), rgb(240, 140, 60), rgb(60, 60, 70) };
    static const Color SKIN[4] = { rgb(246, 210, 178), rgb(220, 170, 128), rgb(170, 116, 80), rgb(110, 74, 50) };
    auto person = [&](Vec2 at, int id, float stride, bool moving) {
        u32 h = (u32)(e.gen * 2654435761u) ^ (u32)(id * 40503u + e.type * 977u);
        Color shirt = SHIRT[h % 8], skin = SKIN[(h >> 4) % 4], legs = (h >> 7) & 1 ? rgb(52, 58, 80) : rgb(90, 76, 60);
        float sw = moving ? std::sin(stride) * 2.0f : 0;
        g.fillCircle(at.x + 1.0f, at.y + 0.8f, 2.8f, rgb(0, 0, 0, 80));                             // shadow
        g.line(at.x - 1.0f, at.y - 3.4f, at.x - 1.0f + sw, at.y + 0.4f, legs);                    // legs
        g.line(at.x + 1.0f, at.y - 3.4f, at.x + 1.0f - sw, at.y + 0.4f, legs);
        g.line(at.x - 2.3f, at.y - 6.6f, at.x - 2.6f - sw * 0.6f, at.y - 3.6f, skin);             // arms swing against the legs
        g.line(at.x + 2.3f, at.y - 6.6f, at.x + 2.6f + sw * 0.6f, at.y - 3.6f, skin);
        g.fill((int)std::floor(at.x - 2.0f), (int)std::floor(at.y - 7.6f), 4, 5, shirt);           // body
        g.fillCircle(at.x, at.y - 9.4f, 1.8f, skin);                                                 // head
        if ((h >> 9) & 1) g.fill((int)std::floor(at.x - 1.8f), (int)std::floor(at.y - 11.4f), 4, 2, (h >> 10) & 1 ? rgb(60, 40, 30) : rgb(200, 170, 90));   // hair
    };
    float t = wallTime + e.gen * 1.7f;
    int pairs = n / 2;
    for (int j = 0; j < (n + 1) / 2; j++) {
        bool single = j == pairs;   // an odd one out strolls alone
        u32 hj = (u32)(e.gen * 7919u + j * 104729u + e.type * 31u);
        float speed = (9.0f + (hj % 7)) * (panic ? 2.6f : 1.0f), cyc = 11.0f + (hj >> 3) % 5, walkT = cyc - (single ? 2.0f : 4.0f);
        float tt = t + (hj % 97) * 0.37f, c = std::floor(tt / cyc), in = tt - c * cyc;
        bool stopped = !panic && in > walkT;
        float dist = speed * (c * walkT + std::min(in, walkT)) + (hj % 1000) * 0.13f;
        Vec2 dir, at;
        if (plaza) {   // a lazy figure-of-eight of its own over the plaza (both sines advance with the distance walked, so a stop is a stop)
            float ax = fw * 0.34f, ay = fh * 0.3f, ph = (hj % 628) * 0.01f, k1 = 1.0f / ax, k2 = 1.7f / ax;
            at = Vec2(p.x + ax * std::sin(dist * k1 + ph), p.y + fh * 0.04f + ay * std::sin(dist * k2 + ph * 1.3f));
            dir = Vec2(std::cos(dist * k1 + ph), ay * k2 * std::cos(dist * k2 + ph * 1.3f)).norm();
        } else {
            float u = std::fmod(dist, 2 * L); bool back = u > L; if (back) u = 2 * L - u;   // back and forth along the walkway
            at = walk(u, dir);
            if (back) dir = dir * -1.0f;
        }
        Vec2 perp(-dir.y, dir.x);
        float stride = dist * 0.55f;
        if (single) { person(at, j * 2, stride, !stopped); continue; }
        Vec2 a = stopped ? at - dir * 3.6f : at + perp * 3.2f, b = stopped ? at + dir * 3.6f : at - perp * 3.2f;   // side by side, face to face when they stop
        if (a.y > b.y) std::swap(a, b);   // (the one further north is drawn first)
        person(a, j * 2, stride, !stopped); person(b, j * 2 + 1, stride + 1.6f, !stopped);
        if (stopped) {   // chatting: a bubble over one, then the other
            Vec2 sp = ((int)((in - walkT) / 1.3f) & 1) ? b : a;
            float bx = sp.x + 2, by = sp.y - 21;
            g.fill((int)bx, (int)by, 11, 7, rgb(250, 250, 246, 230));
            g.fill((int)bx + 1, (int)by + 7, 2, 2, rgb(250, 250, 246, 230));
            for (int d = 0; d < 3; d++) g.fill((int)bx + 2 + d * 3, (int)by + 3, 1, 1, ((int)(wallTime * 4) % 3) >= d ? rgb(80, 80, 90) : rgb(180, 180, 186));
        }
    }
}

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
    case BR_AIRFIELD: {   // runway edge lights chase down the strip when powered (two dotted rows, alternating blink)
        if (disabled || !powered) break;
        float fw2 = bt.w * TILE * 0.5f - 8, fh2 = bt.h * TILE * 0.5f - 8;
        Color lc = cy ? Color{110, 232, 255, 200} : Color{255, 190, 90, 200};
        int phase = ((int)(t * 2.2f)) & 1;
        for (int i = 0; i < 4; i++) {
            if (((i + phase) & 1) == 0) continue;
            float lx = p.x - fw2 + (2 * fw2) * (i / 3.0f);
            g.glowAdd(lx, p.y - fh2, 5.5f, lc); g.glowAdd(lx, p.y + fh2, 5.5f, lc);
        }
        break; }
    case BR_INCOME:
        if (cy) g.glowAdd(p.x, p.y - 22, 26, Color{255, 214, 96, (u8)(40 + 90 * pulse)});
        else g.draw(g.prop[2], p.x, p.y + 6, powered ? std::sin(t * 2.4f) * 0.26f : 0, 1.15f);
        break;
    case BR_NUKE:
        if (g_sim.time >= e.actionTimer && powered && g.armed[e.type].tex) g.draw(g.armed[e.type], p.x, p.y, 0, 1, disabled ? rgb(120, 140, 170) : rgb(255, 255, 255));
        if (cy) { Color rc = Color{255, 80, 70, (u8)((std::sin(t * 5.0f) > 0 && powered) ? 190 : 40)}; g.glowAdd(p.x - 37, p.y - 51, 9, rc); g.glowAdd(p.x + 37, p.y - 51, 9, rc); }
        else g.glowAdd(p.x - 24, p.y + 26, 18, Color{255, 120, 50, (u8)(30 + 50 * (0.5f + 0.5f * std::sin(t * 7.0f)))});
        break;
    case BR_UNUKE: {   // the silver warhead shows in the open doors when it is ready; the status lights breathe silver
        bool ready = g_sim.time >= e.actionTimer && powered && !disabled;
        if (ready && g.armed[e.type].tex) g.draw(g.armed[e.type], p.x, p.y, 0, 1);
        Color sc = Color{214, 220, 230, (u8)(ready ? 90 + 110 * pulse : 40)};
        g.glowAdd(p.x - 34, p.y + 30, 8, sc); g.glowAdd(p.x + 34, p.y + 30, 8, sc);
        if (ready) g.glowAdd(p.x, p.y - 6, 30, Color{220, 226, 236, (u8)(40 + 50 * pulse)});
        if (e.hatch <= 0) g.glowAdd(p.x, p.y, 20, Color{255, 120, 60, (u8)(60 + 60 * pulse)});   // doors blown: the shaft burns
        break; }
    case BR_CIVIC: drawTownsfolk(e, p); break;
    case BR_ANTINUKE: {
        bool firing = e.cooldown2 > 0 && powered && !disabled, armed = powered && !disabled && g_sim.time >= e.siloTimer;
        if (cy) {   // the lens on the pylon: dim without power, a slow breath when armed, a blinding focus while a beam is on its way
            float al = firing ? 255.0f : (armed ? 60.0f + 70.0f * pulse : 18.0f);
            g.glowAdd(p.x + 1, p.y - 42, firing ? 34.0f : 22.0f, Color{110, 232, 255, (u8)al});
            if (firing) { g.glowAdd(p.x + 1, p.y - 42, 12.0f, Color{255, 255, 255, 230}); g.fill((int)p.x - 1, (int)p.y - 42, 3, 3, rgb(255, 255, 255)); }
        } else {    // the silo: the hatch glows when it is armed and belches fire and smoke when interceptors leave it
            Vec2 hc(p.x + 2, p.y - 12);
            if (armed) g.glowAdd(hc.x, hc.y, 16.0f, Color{255, 190, 90, (u8)(30 + 40 * pulse)});
            if (firing) {
                g.glowAdd(hc.x, hc.y - 6, 30.0f, Color{255, 170, 80, 190});
                int fr = ((int)(wallTime * 24) + (int)e.gen) % 6;
                g.draw(g.fxs.flame[fr], hc.x, hc.y + 2, 0, 0.9f, rgb(255, 230, 190), 220);
                if (frameDt > 0 && fxRng.f() < 40.0f * frameDt) emitP(hc.x + cam.x + fxRng.f(-8, 8), hc.y + cam.y - 6, fxRng.f(-14, 14), fxRng.f(-46, -18), fxRng.f(0.9f, 1.8f), 6, 20, rgb(222, 220, 214, 200), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.2f);
            }
        }
        break; }
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

// An indestructible structure does not crumble: it burns. Flames and a smoke column grow with its heat and die down as it cools; the island
// launcher's laser chamber hums with live beams between its six emitters and flares white-hot when something hits it.
void Game::drawIndestructible(const Entity& e, Vec2 p) {
    Gfx& g = g_gfx;
    const BuildType& bt = e.bt();
    float fw = (float)bt.w * TILE, fh = (float)bt.h * TILE;
    if (bt.role == BR_ISLANDNUKE) {
        const float cx = 48, cy = 50, PR = 38, PH = 16;   // (the emitter geometry of the launcher's art: artb_special.cpp)
        Vec2 o = p - Vec2(fw * 0.5f, fh * 0.5f);
        Vec2 top[6];
        for (int i = 0; i < 6; i++) { float an = -1.5708f + i * 1.0472f; top[i] = o + Vec2(cx + std::cos(an) * PR, cy + std::sin(an) * PR - PH); }
        float hit = clampf(1.0f - (g_sim.time - e.lastDamaged) / 1.2f, 0, 1);   // flare after a hit
        bool held = e.owner >= 0;
        Color beam = held ? playerColor(e.owner) : rgb(255, 90, 60);
        beam = mix(beam, rgb(255, 250, 240), hit * 0.8f);
        for (int i = 0; i < 6; i++) {
            Vec2 a = top[i], b = top[(i + 1) % 6];
            float fl = 0.55f + 0.45f * std::sin(wallTime * 17.0f + i * 1.7f + e.gen) * std::sin(wallTime * 5.3f + i);
            u8 al = (u8)clampf((110 + 120 * fl) * (0.6f + 0.4f * (held ? 1.0f : 0.6f)) + 100 * hit, 0, 255);
            g.line(a.x, a.y, b.x, b.y, Color{beam.r, beam.g, beam.b, al});
            if (hit > 0.05f) g.thickLine(a.x, a.y, b.x, b.y, Color{255, 240, 220, (u8)(150 * hit)}, 2.0f);
            g.glowAdd(a.x, a.y, 7 + 5 * fl + 10 * hit, Color{beam.r, beam.g, beam.b, (u8)(90 + 60 * fl)});
        }
        if (hit > 0.05f) g.circle(p.x, p.y, PR + 6 + 10 * (1.0f - hit), Color{255, 230, 200, (u8)(200 * hit)}, 32);
        if (held && g_sim.time >= e.siloTimer && ((int)(wallTime * 2.5f) & 1)) g.glowAdd(o.x + cx, o.y + cy - 10, 9, Color{255, 80, 60, 170});   // armed: the warhead's tip glows
    }
    if (e.heat <= 0.02f) return;
    float h = clampf(e.heat / HEAT_MAX, 0, 1);
    Rng r((u64)e.gen * 6151 + (u64)e.type * 37 + 3);
    int n = std::max(1, (int)(h * (2.6f + bt.w * bt.h * 0.6f) + 0.5f));   // (a burning wall segment carries up to three flames)
    for (int i = 0; i < n; i++) {
        float x = p.x + r.f(-0.38f, 0.38f) * fw, y = p.y + r.f(-0.42f, 0.28f) * fh;
        float fl = 0.7f + 0.3f * std::sin(wallTime * 13.0f + i * 2.1f + e.gen);
        int fr = ((int)(wallTime * 13) + i * 2 + (int)e.gen) % 6;
        float sc = (0.5f + 0.8f * h) * (0.8f + 0.2f * fl) * (bt.w * bt.h <= 1 ? 0.95f : 1.0f);
        g.glowAdd(x, y - 6 * sc, 18 * sc, Color{255, 120, 40, (u8)(110 * fl)});
        g.draw(g.fxs.flame[fr], x, y + 4, 0, sc, rgb(255, 255, 255), (u8)(200 + 50 * h));
        if (frameDt > 0 && fxRng.f() < (1.0f + 3.0f * h) * frameDt)
            emitP(e.pos.x + (x - p.x), e.pos.y + (y - p.y) - 6, fxRng.f(-6, 6), fxRng.f(-34, -18), fxRng.f(1.6f, 2.8f), 5 + 7 * sc, 14 + 12 * sc, rgb(46, 42, 40, 220), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), fxRng.f(-0.3f, 0.3f));
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
    if (e.carrier.valid()) return;   // aboard a carrier
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
        if (bt.role == BR_NAVAL) drawDock(e, p);   // the pier reaches into the lake under the structure
        if (bt.role == BR_WAYMAKER) {   // the grinder at work on its patch: it shudders, and a bar fills as the ground is cleared
            float sh = std::sin(wallTime * 60.0f + e.gen) * 0.8f;
            g.draw(s, p.x + sh, p.y, 0, 1, tint);
            if (g.buildingTeam[e.type].tex) g.draw(g.buildingTeam[e.type], p.x + sh, p.y, 0, 1, playerColor(owner));
            float prog = clampf(e.progress, 0, 1);
            hpBar(g, p.x - 24, fy - 8, 48, prog);
            return;
        }
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
                g.copyScaled(s.tex, &src, dst);
                Color wl = hudAccent(bt.faction);
                g.fill((int)(fx - 2), (int)dst.y, (int)(fw + 4), 1, rgb(wl.r, wl.g, wl.b, 200));
                g.glowAdd(fx + std::fmod(wallTime * 40.0f + e.gen * 17.0f, fw), dst.y, 10, Color{wl.r, wl.g, wl.b, 150});
            }
        } else {
            g.draw(s, p.x, p.y, 0, 1, tint);
            if (bt.role == BR_BUNKER) for (int u = 0; u < BU_COUNT; u++) {   // its own upgrades, each a part fitted to the apron (the one being fitted fades in as the work goes on)
                const Sprite& us = g.bunkerUpg[bt.faction][u];
                if (!us.tex) continue;
                if (Sim::bunkerHas(e, u)) g.draw(us, p.x, p.y, 0, 1, tint);
                else if (e.bunkerBusy == u) g.draw(us, p.x, p.y, 0, 1, mix(tint, rgb(255, 226, 150), 0.45f), (u8)clampf(50.0f + 150.0f * e.bunkerProgress + 25.0f * std::sin(wallTime * 6.0f), 0, 255));
            }
            if (bt.role == BR_BUNKER) {   // the surface hatch: two steel leaves that lie flat; blown off, only a scorched hole remains. Its health shows as the rim light
                float hf = e.hatchMax > 0 ? e.hatch / e.hatchMax : 0;
                Vec2 hc = p;
                const float R = BUNKER_HATCH_R * bt.w / 3.0f;
                if (hf <= 0) { g.fillCircle(hc.x, hc.y, R, rgb(22, 20, 18)); g.fillCircle(hc.x, hc.y, R * 0.73f, rgb(8, 8, 8)); g.circle(hc.x, hc.y, R, rgb(90, 76, 60), 24); }
                else {
                    bool firing = e.cooldown2 > 0;   // the garrison is shooting: the leaves stand open
                    float open = firing ? R * 0.2f : 0.0f;
                    g.fillCircle(hc.x, hc.y, R, rgb(46, 50, 56));
                    g.fillCircle(hc.x - open, hc.y, R * 0.87f, rgb(138, 146, 154)); g.fillCircle(hc.x - open - 1, hc.y - 1, R * 0.7f, rgb(176, 184, 192));
                    for (int k = 0; k < 8; k++) { float an = k * 0.7854f + 0.39f; g.fillCircle(hc.x - open + std::cos(an) * R * 0.78f, hc.y + std::sin(an) * R * 0.78f, 1.2f, rgb(96, 102, 110)); }   // the leaves' bolts
                    g.fill((int)(hc.x - R * 0.87f), (int)(hc.y - 1), (int)(R * 1.74f), 2, rgb(40, 44, 48));
                    if (open > 0) { g.fill((int)(hc.x - R * 0.27f), (int)(hc.y - R * 0.8f), (int)(R * 0.54f), (int)(R * 1.6f), rgb(14, 14, 16)); g.glowAdd(hc.x, hc.y, R * 0.8f, Color{255, 180, 80, 70}); }
                    Color rim = hf > 0.6f ? hudAccent(bt.faction) : (hf > 0.3f ? rgb(240, 200, 60) : rgb(230, 70, 60));
                    g.circle(hc.x, hc.y, R, rim, 28);
                }
            }
            if (g.buildingTeam[e.type].tex) { Color tc = e.owner < 0 ? rgb(206, 200, 180) : playerColor(owner); if (disabled) tc = mix(tc, rgb(120, 140, 170), 0.5f); g.draw(g.buildingTeam[e.type], p.x, p.y, 0, 1, tc); }
            drawBuildingAnim(e, p, disabled);
            if (bt.role == BR_BUNKER && e.constructed && e.hatchMax > 0 && e.hatch > 0) {   // the two miniguns on the hatch
                for (int gi = 0; gi < BUNKER_GUNS; gi++) {
                    Vec2 gp = p + (g_sim.bunkerGunPos(e, gi) - e.pos);
                    Vec2 d(std::cos(e.gunAng[gi]), std::sin(e.gunAng[gi]));
                    int mx = (int)gp.x, my = (int)gp.y;   // (solid squares: the soft blob sprites would melt into the steel plate)
                    g.fill(mx - 6, my - 5, 12, 12, rgb(10, 12, 16, 150));
                    g.fill(mx - 5, my - 6, 10, 10, rgb(30, 34, 40));
                    g.fill(mx - 4, my - 5, 8, 8, disabled ? rgb(90, 96, 108) : rgb(120, 130, 142));
                    g.thickLine(gp.x, gp.y - 1, gp.x + d.x * 13, gp.y - 1 + d.y * 13, rgb(18, 20, 24), 3.0f);
                    g.fill(mx - 1, my - 3, 3, 3, hudAccent(bt.faction));
                }
            }
            if (bt.role == BR_BUNKER && e.constructed) {   // live parts of its upgrades: the laser heads turning, the hospital beacon, the tubes' ready lights, the mast's warning light
                const float k = bt.w * TILE / 96.0f;
                if (Sim::bunkerHas(e, BU_LASERS)) {
                    Color lc = WEAPONS[W_BUNKER_SKYLASER].color;
                    bool hot = g_sim.time - e.lzFired < 0.22f && !disabled;
                    for (int i = 0; i < 2; i++) {
                        Vec2 lp = p + (g_sim.bunkerLaserPos(e, i) - e.pos), d(std::cos(e.lzAng[i]), std::sin(e.lzAng[i])), sd(-d.y, d.x);
                        g.fillCircle(lp.x + 1.0f, lp.y + 2.0f, 6.5f, rgb(10, 12, 16, 150));
                        g.fillCircle(lp.x, lp.y, 5.8f, disabled ? rgb(84, 90, 100) : rgb(170, 180, 194));
                        g.fillCircle(lp.x - 1.0f, lp.y - 1.0f, 3.6f, disabled ? rgb(100, 106, 116) : rgb(212, 220, 230));
                        for (int b2 = -1; b2 <= 1; b2 += 2) g.thickLine(lp.x + sd.x * 2.1f * b2, lp.y + sd.y * 2.1f * b2, lp.x + sd.x * 2.1f * b2 + d.x * 12.0f, lp.y + sd.y * 2.1f * b2 + d.y * 12.0f, rgb(40, 46, 58), 2.2f);
                        g.fillCircle(lp.x, lp.y, 1.8f, disabled ? rgb(80, 90, 100) : lc);
                        if (hot) g.glowAdd(lp.x + d.x * 12.0f, lp.y + d.y * 12.0f, 9.0f, Color{lc.r, lc.g, lc.b, 170});
                    }
                }
                if (Sim::bunkerHas(e, BU_MEDIC) && !disabled) {   // a green beacon breathing over the hospital, and a faint pulse running out to its reach now and then
                    float br = 0.5f + 0.5f * std::sin(wallTime * 3.0f + e.gen);
                    Vec2 mp = p + Vec2(0, (BUNKER_MED_Y - 15.0f) * k);   // (the tip of the hospital's mast in artb_bunker.cpp)
                    g.glowAdd(mp.x, mp.y, 5.0f + 4.0f * br, Color{110, 255, 150, (u8)(90 + 90 * br)});
                    float ph = std::fmod(wallTime * 0.45f + e.gen * 0.37f, 1.0f);
                    if (ph < 0.6f) g.circle(p.x, p.y, BUNKER_MED_RADIUS * TILE * ph / 0.6f, Color{120, 255, 160, (u8)(70 * (1.0f - ph / 0.6f))}, 48);
                }
                if (Sim::bunkerHas(e, BU_TRIPLE)) {   // a ready light on each tube lid: green when the launch is ready, amber while it reloads
                    bool rdy = g_sim.time >= e.siloTimer && !disabled;
                    for (int t = -1; t <= 1; t++) { Vec2 q = p + Vec2(t * BUNKER_SILO_X, BUNKER_SILO_Y) * k; g.fillCircle(q.x + 3.5f, q.y - 3.5f, 1.3f, rdy ? rgb(120, 255, 120) : rgb(255, 176, 60)); }
                }
                if (Sim::bunkerHas(e, BU_PARA) && ((int)(wallTime * 1.6f + e.gen) & 1)) {   // the aviation light on the radio mast (and its call is ready: steady white under it)
                    Vec2 q = p + Vec2(BUNKER_MAST_X * k, -33.5f * k);   // (the top of the radio mast)
                    g.glowAdd(q.x, q.y, 6.0f, Color{255, 70, 50, 200});
                }
            }
            if (bt.role == BR_BUNKER && e.constructed && e.lastDamaged > 0 && g_sim.time - e.lastDamaged < BUNKER_RETALIATE) {   // the rocket pod, raised while the bunker is under attack
                Vec2 pp = p + Vec2(0, BUNKER_POD_Y), d(std::cos(e.podAng), std::sin(e.podAng)), sd(-d.y, d.x);
                g.fillCircle(pp.x, pp.y + 2.0f, 8.0f, rgb(10, 12, 16, 150));
                g.fillCircle(pp.x, pp.y, 7.0f, disabled ? rgb(80, 86, 96) : rgb(96, 104, 92));
                for (int k = -1; k <= 1; k += 2) g.thickLine(pp.x + sd.x * 3.5f * k, pp.y + sd.y * 3.5f * k, pp.x + sd.x * 3.5f * k + d.x * 11, pp.y + sd.y * 3.5f * k + d.y * 11, rgb(26, 28, 30), 3.5f);
                g.fillCircle(pp.x, pp.y, 2.0f, e.podCd > WEAPONS[W_BUNKER_ROCKETS].cooldown - 0.3f ? rgb(255, 200, 120) : hudAccent(bt.faction));
            }
            if (bt.role == BR_BUNKER && e.constructed && e.surrounded) {   // surrounded overdrive: pop-up heavy lasers / quad machineguns and radiation warning rim
                Color oc = bt.faction == F_CYBER ? rgb(100, 245, 255) : rgb(255, 210, 90);
                g.circle(p.x, p.y, fw * 0.56f + 1.5f * std::sin(wallTime * 9.0f), Color{120, 255, 95, 170}, 24);
                for (int oi = 0; oi < 2; oi++) {
                    Vec2 op = p + (g_sim.bunkerOverdrivePos(e, oi) - e.pos);
                    Vec2 d(std::cos(e.overdriveAng[oi]), std::sin(e.overdriveAng[oi])), sd(-d.y, d.x);
                    g.fillCircle(op.x, op.y + 1.0f, 6.5f, rgb(14, 18, 22, 180));
                    g.fillCircle(op.x, op.y, 5.5f, disabled ? rgb(75, 82, 92) : rgb(52, 60, 68));
                    for (int b = -1; b <= 1; b += 2)
                        g.thickLine(op.x + sd.x * 2.2f * b, op.y + sd.y * 2.2f * b, op.x + sd.x * 2.2f * b + d.x * 12.0f, op.y + sd.y * 2.2f * b + d.y * 12.0f, oc, 2.2f);
                    g.fillCircle(op.x, op.y, 2.2f, oc);
                }
            }
            // rotating heads for defenses
            if (bt.role == BR_TURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 0 : 1], p.x, p.y, e.angle, 1, tint);
            else if (bt.role == BR_AATURRET) g.draw(g.turretHead[bt.faction == F_CYBER ? 2 : 3], p.x, p.y, e.angle, 1, tint);
            if (g_sim.roofGun(e) >= 0) {   // Defense Guns upgrade: a small laser / machine gun mount on the roof
                Vec2 gp = p + (g_sim.roofGunPos(e) - e.pos);
                Color ac = hudAccent(bt.faction);
                g.fillCircle(gp.x, gp.y + 2.0f, 9.0f, rgb(10, 12, 16, 150));
                g.fillCircle(gp.x, gp.y + 1.0f, 8.0f, disabled ? rgb(70, 76, 90) : shade(ac, 0.55f));
                g.fillCircle(gp.x, gp.y + 1.0f, 6.5f, rgb(30, 34, 40));
                g.draw(g.turretHead[bt.faction == F_CYBER ? 0 : 1], gp.x, gp.y, e.turret2, 0.85f, tint);
            }
            // production activity light
            if (!e.queue.empty() && ((int)(wallTime * 3) & 1)) { Color ac = hudAccent(bt.faction); g.glowAdd(fx + fw - 7, fy + 2, 7, Color{ac.r, ac.g, ac.b, 160}); g.fillCircle(fx + fw - 7, fy + 2, 2.4f, ac); }
        }
        // damage: soot, then flames and smoke columns once the structure is badly hurt (drawn live, cheap)
        if (e.constructed && e.hp < e.maxHp * 0.66f && !disabled) drawBuildingDamage(e, p);
        if (e.district >= 0) drawRebuildCrew(e, p);   // a town block's people raising it again / patching it
        if (e.constructed && Sim::invulnerable(e)) drawIndestructible(e, p);
        if (selected) {
            Color sc = rgb(255, 255, 255, 230); float L = 9;
            for (int cx = 0; cx < 2; cx++) for (int cy = 0; cy < 2; cy++) {   // corner brackets
                float x = cx ? fx + fw + 1 : fx - 2, y = cy ? fy + fh + 1 : fy - 2, dx = cx ? -L : L, dy = cy ? -L : L;
                g.fill((int)std::min(x, x + dx), (int)y, (int)L, 1, sc); g.fill((int)x, (int)std::min(y, y + dy), 1, (int)L, sc);
            }
            if (e.hasRally && e.constructed) { Vec2 r = worldToScreen(e.rally); g.line(p.x, p.y, r.x, r.y, rgb(120, 255, 120, 160)); g.circle(r.x, r.y, 5, rgb(120, 255, 120)); }
        }
        if (selected || e.hp < e.maxHp || !e.constructed) hpBar(g, fx, fy - 8, fw, e.constructed ? e.hp / e.maxHp : e.progress, true);
        if (bt.role == BR_BUNKER && e.constructed && e.owner >= 0 && g_sim.players[e.owner].team != g_sim.players[g_sim.humanPlayer].team) {   // a hidden enemy bunker a spy drone has found
            g.circle(p.x, p.y, fw * 0.62f + 2.0f * std::sin(wallTime * 7.0f), rgb(110, 230, 255), 28);
            g.text((int)p.x - 20, (int)(fy - 20), "BUNKER SPOTTED", rgb(110, 230, 255));
        }
        if ((bt.role == BR_BUNKER || bt.role == BR_STRONGHOLD) && e.constructed && e.owner >= 0 && g_sim.players[e.owner].team == g_sim.players[g_sim.humanPlayer].team && (selected || !e.passengers.empty())) {   // garrison count
            int inf = g_sim.garrisonInfantry(e), cap = g_sim.garrisonCap(e);
            char gb[32]; snprintf(gb, sizeof gb, "%d/%d%s", inf, cap, g_sim.garrisonDozers(e) > 0 ? " +D" : "");
            g.text((int)(p.x - g.textW(gb, 1) / 2), (int)(fy + fh + 4), gb, rgb(240, 240, 240));
        }
        if (bt.role == BR_STRONGHOLD && e.constructed && e.owner < 0 && mouseY < VIEW_H &&
            mouseX >= fx && mouseX <= fx + fw && mouseY >= fy - 18 && mouseY <= fy + fh) {
            char label[80]; snprintf(label, sizeof label, "%s - UNCLAIMED", bt.name);
            const char* lbl = label;
            g.text((int)(p.x - g.textW(lbl, 1) / 2), (int)(fy - 18), lbl, rgb(226, 218, 190));
        }
        if (disabled) g.text((int)p.x - 9, (int)p.y - 4, "EMP", rgb(160, 220, 255));
        return;
    }
    const UnitType& ut = e.ut();
    const Sprite& body = g.unitBody[e.type][slotOf(owner)];
    bool air = ut.kind == UK_AIR;
    float alt = air ? e.alt : 0.0f;
    float spd = (e.pos - e.prevPos).len() / SIM_DT;            // px/s over the last tick
    float fallK = !air && e.fall > 0 ? clampf(e.fall / e.fallTime, 0, 1) : 0.0f;   // parachute descent: 1 at release .. 0 on touchdown
    float lift = fallK > 0 ? 4.0f + 40.0f * fallK : 0.0f;
    if (fallK > 0) {   // the canopy throws a wide soft shadow on the ground
        float cs = ut.kind == UK_VEH ? 1.7f : 1.05f;
        g.draw(g.shadowLarge, p.x + 5, p.y + 5, 0, 0.9f * cs * (0.6f + 0.4f * fallK), rgb(255, 255, 255), 110);
    }
    Color mod = disabled ? rgb(120, 140, 170) : rgb(255, 255, 255);
    if (air || fallK > 0) { Color t = ambientTint(); mod = rgb(mod.r * t.r / 255, mod.g * t.g / 255, mod.b * t.b / 255); }   // (aircraft are drawn after the world's light pass: they take the hour's light themselves)
    const u8 ghost = (u8)((Sim::stealthOf(e) ? 150 : 255) * clampf(e.fade, 0, 1));
    const bool deckJet = e.type == U_C_CARRIERJET || e.type == U_K_CARRIERJET;
    const float usc = deckJet ? CARRIER_JET_DECK_SCALE + (1.0f - CARRIER_JET_DECK_SCALE) * clampf(alt, 0, 1) : 1.0f;   // a carrier jet on deck is small; it grows as it climbs   // your own stealth units show as ghosts (the enemy sees nothing at all unless a detector is near); a departing Air Strike fighter fades out
    if (air) {
        // shadow: the aircraft's own silhouette, thrown to the south-east and sliding closer as it settles onto the pad
        g.drawRolled(body, p.x + 3 + 15 * alt, p.y + 4 + 27 * alt, e.angle, e.roll, usc, rgb(0, 0, 0), (u8)((70 + 40 * (1 - alt)) * clampf(e.fade, 0, 1)));
        p.y -= 14 * alt;   // altitude
    } else {
        if (ut.naval) {   // a hull on the water: the wake round it and a soft dark patch under it
            if (e.fade > 0.35f) drawShipWake(e, p);   // (a Fleet Call ship still in the haze at the border leaves none yet)
            g.draw(body, p.x + 1, p.y + 4, e.angle, 1, rgb(0, 0, 0), (u8)(64 * clampf(e.fade, 0, 1)));
        } else if (Sim::swimmer(e) && g_map.swimTile(tileOf(e.pos.x), tileOf(e.pos.y))) {   // an infiltrator paddling its inflatable raft
            float ca = std::cos(e.angle), sa = std::sin(e.angle);
            if (spd > 5) for (int k = 1; k <= 2; k++) g.drawSized(g.fxs.ring, p.x - ca * 9 * k, p.y - sa * 9 * k, 10.0f + 7 * k, 7.0f + 5 * k, e.angle, rgb(220, 240, 255), (u8)(90 / k));
            g.drawSized(g.disc, p.x + 1, p.y + 2, 22, 13, e.angle, rgb(0, 20, 30), 90);        // its shadow on the water
            g.drawSized(g.disc, p.x, p.y, 21, 12, e.angle, rgb(48, 50, 54));                   // the rubber tube
            g.drawSized(g.disc, p.x + ca * 0.5f, p.y + sa * 0.5f, 15, 7, e.angle, rgb(26, 28, 32));   // the floor
            g.drawSized(g.disc, p.x + ca * 9, p.y + sa * 9, 4, 4, 0, rgb(232, 120, 40));          // the orange bow cap
        } else {
            g.draw(ut.kind == UK_INF ? g.shadowSmall : g.shadowLarge, p.x, p.y + 3, 0, ut.kind == UK_INF ? 0.55f : 0.75f);
            if (ut.kind == UK_VEH) g.draw(body, p.x + 2, p.y + 3, e.angle, 1, rgb(0, 0, 0), 55);   // contact shadow in the hull's own shape
        }
        p.y -= lift;
    }
    if (Sim::stealthOf(e) && e.owner >= 0 && g_sim.players[e.owner].team != g_sim.players[g_sim.humanPlayer].team) {   // a stealthy enemy one of your detectors has found: marked while it is spotted
        float pulse = 0.65f + 0.35f * std::sin(wallTime * 7.0f + (float)e.gen);
        float gy = p.y + (air ? 14 * alt : 0) + 2;
        g.circle(p.x, gy, e.radius() + 6 + 2 * pulse, rgb(110, 230, 255), 18);
        g.text((int)p.x - 12, (int)(p.y - e.radius() - 20), "SPOTTED", rgb(110, 230, 255));
    }
    if (selected && ut.detect > 0 && ut.role == UR_SCOUT) {   // a selected spy drone shows how far it finds hidden snipers and bunkers
        float gy = p.y + (air ? 14 * alt : 0);
        g.dashedCircle(p.x, gy, ut.detect * TILE, rgb(110, 230, 255), wallTime * 18.0f);
    }
    if (selected) {   // a soft ring on the ground under the unit (not under its altitude), squashed a little for the viewing angle
        float rr = (e.radius() + 5) * 2.0f, gy = p.y + (air ? 14 * alt : 0) + lift + 2;
        g.drawSized(g.fxs.ring, p.x, gy, rr, rr * 0.8f, 0, rgb(150, 255, 170), 235);
        g.drawSized(g.fxs.ring, p.x, gy, rr * 1.1f, rr * 0.88f, 0, rgb(150, 255, 170), 70);
    }
    // walk / track cycle: the two extra frames alternate while the unit is actually moving
    const Sprite* bs = &body;
    bool moving = (e.pos - e.prevPos).len2() > 0.01f && fallK <= 0;
    if (!air && !paused && !disabled && moving) {
        float travelled = (e.pos.x + e.pos.y) / (ut.kind == UK_INF ? 2.4f : 3.0f) + e.gen * 0.37f;   // feet and tracks turn with the ground they cover, not the clock
        bs = &g.unitAnim[e.type][slotOf(owner)][((int)std::floor(travelled)) & 1];
    }
    // body language (same draw calls, just a nudge): suspension bob on the move, a soldier's step bounce, a hull's slow
    // roll and heave, a helicopter's hover sway. Everything settles still when the unit does.
    float drawAng = e.angle;
    if (!paused) {
        if (ut.naval) {
            drawAng += std::sin(wallTime * 0.85f + (float)e.gen * 1.7f) * 0.018f;
            p.y += std::sin(wallTime * 0.65f + (float)e.gen * 2.3f) * 0.8f;
        } else if (air) {
            p.y += std::sin(wallTime * (ut.heli ? 2.1f : 3.0f) + (float)e.gen * 2.9f) * (ut.heli ? 1.1f : 0.5f) * (0.25f + 0.75f * alt);
        } else if (moving && !disabled) {
            if (ut.kind == UK_VEH) p.y += std::sin((e.pos.x + e.pos.y) * 0.115f + (float)e.gen) * 0.75f;
            else if (ut.kind == UK_INF) p.y -= std::fabs(std::sin((e.pos.x + e.pos.y) * 0.21f + (float)e.gen)) * 1.1f;
        }
    }
    // vehicles kick up dust: a puff behind the tracks now and then, grey on asphalt and tan on bare ground
    if (!air && ut.kind == UK_VEH && !ut.naval && moving && frameDt > 0 && fxRng.f() < 11.0f * frameDt) {
        Vec2 back = Vec2(std::cos(e.angle), std::sin(e.angle)) * -(e.radius() * 0.85f);
        bool road = g_map.tile(clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1)) == T_ROAD;
        float sz = e.radius() * 0.42f;
        emitP(e.pos.x + back.x + fxRng.f(-3, 3), e.pos.y + back.y + fxRng.f(-3, 3), back.norm().x * 14 + fxRng.f(-6, 6), back.norm().y * 14 + fxRng.f(-6, 6), fxRng.f(0.6f, 1.0f), sz, sz * 2.6f,
              road ? rgb(150, 150, 152, 150) : rgb(178, 150, 108, 170), PK_DUST, (u8)fxRng.range(0, 2), 0, 1.4f, fxRng.f(0, 6), fxRng.f(-0.6f, 0.6f));
    }
    // tracks press into soft ground: a pair of short dark dashes behind a moving vehicle (none on asphalt), fading over a quarter of a minute
    if (!air && ut.kind == UK_VEH && !ut.naval && moving && frameDt > 0 && fxRng.f() < 9.0f * frameDt) {
        int tl = g_map.tile(clampi(tileOf(e.pos.x), 0, MAP_W - 1), clampi(tileOf(e.pos.y), 0, MAP_H - 1));
        if (tl == T_DIRT || tl == T_SAND || tl == T_GRASS || tl == T_GRASS2) {
            Vec2 fw(std::cos(e.angle), std::sin(e.angle)), side(-fw.y, fw.x);
            float off = e.radius() * 0.55f, life = tl == T_SAND ? 9.0f : (tl == T_DIRT ? 16.0f : 8.0f);
            for (int sd = -1; sd <= 1; sd += 2) {
                Tread t{ e.pos.x - fw.x * e.radius() * 0.5f + side.x * off * sd, e.pos.y - fw.y * e.radius() * 0.5f + side.y * off * sd, e.angle, 0, life, (u8)fxRng.range(0, 1) };
                if (treads.size() < 360) treads.push_back(t);
                else { treads[treadHead] = t; treadHead = (treadHead + 1) % treads.size(); }
            }
        }
    }
    // jets: afterburner flame, glow and a vapour trail while they are moving fast
    if (air && ut.jet && spd > 120 && !disabled) {
        float burn = clampf((spd - 120) / 360.0f, 0, 1), th = e.angle;
        const float jsc = artScale(e.type) * usc;
        Vec2 tail = Vec2(p.x - std::cos(th) * 25.0f * 0.62f * jsc, p.y - std::sin(th) * 25.0f * 0.62f * jsc);
        int fr = ((int)(wallTime * 24) + (int)e.gen) % 6;
        bool cyj = ut.faction == F_CYBER;
        Color fc = cyj ? rgb(140, 230, 255) : rgb(255, 190, 110);
        g.draw(g.fxs.flame[fr], tail.x, tail.y, th - 1.5708f, (0.55f + 1.0f * burn) * usc, fc, (u8)(150 + 105 * burn));
        g.glowAdd(tail.x, tail.y, 9 + 9 * burn, Color{fc.r, fc.g, fc.b, (u8)(120 * burn + 40)});
        if (frameDt > 0 && burn > 0.45f && fxRng.f() < 40.0f * frameDt) {
            float wy = alt > 0.5f ? 1.0f : 0.0f; (void)wy;
            emitP(e.pos.x - std::cos(th) * 18, e.pos.y - std::sin(th) * 18 - 14 * alt, 0, 0, fxRng.f(1.1f, 1.7f), 2.4f, 7.5f, rgb(238, 244, 248, 150), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), 0.1f);
        }
        if (ut.temp && !disabled && e.order == O_STRIKERUN && e.fade > 0.3f) {   // an Air Strike fighter: twin afterburner cores, a shock cone of vapour round the airframe, wingtip vortices and a long hot contrail
            Vec2 f(std::cos(th), std::sin(th)), sd(-f.y, f.x);
            float wing = 24.0f * UNIT_SCALE * jsc;
            Color hot = cyj ? rgb(190, 240, 255) : rgb(255, 214, 150);
            for (int k = -1; k <= 1; k += 2) {
                Vec2 nz = tail + sd * (k * 3.4f);
                g.draw(g.fxs.flame[(fr + k + 3) % 6], nz.x, nz.y, th - 1.5708f, (0.5f + 0.9f * burn), hot, (u8)(120 + 135 * burn));
                g.glowAdd(nz.x, nz.y, 6 + 7 * burn, Color{hot.r, hot.g, hot.b, (u8)(110 + 120 * burn)});
            }
            g.glowAdd(tail.x - f.x * 10, tail.y - f.y * 10, 20 + 16 * burn, Color{fc.r, fc.g, fc.b, (u8)(60 + 90 * burn)});
            if (burn > 0.55f) {   // the Prandtl-Glauert cone: a pale ellipse of condensed air hugging the fuselage, trailing back from the wing roots
                Vec2 cc = Vec2(p.x - f.x * 8.0f, p.y - f.y * 8.0f);
                g.drawSized(g.fxs.cloud[1], cc.x, cc.y, 54.0f + 40.0f * burn, 22.0f + 8.0f * burn, th, rgb(244, 250, 255), (u8)(34 + 40 * burn));
            }
            if (frameDt > 0 && burn > 0.3f) {
                for (int k = -1; k <= 1; k += 2) {   // vortices rolling off the wing tips
                    if (fxRng.f() < 55.0f * frameDt) {
                        Vec2 wt(e.pos.x - f.x * 6.0f + sd.x * k * wing, e.pos.y - f.y * 6.0f + sd.y * k * wing - 14 * alt);
                        emitP(wt.x, wt.y, 0, 0, fxRng.f(0.9f, 1.5f), 1.4f, 4.5f, rgb(246, 250, 255, 130), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.2f, fxRng.f(0, 6), 0.05f);
                    }
                }
                if (fxRng.f() < 70.0f * frameDt) emitP(e.pos.x - f.x * 22.0f, e.pos.y - f.y * 22.0f - 14 * alt, 0, 0, fxRng.f(1.6f, 2.6f), 3.0f, 11.0f, rgb(236, 242, 248, 120), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), 0.08f);
            }
            if (e.salvoLeft > 0 && !paused) {   // rocket pods lighting up on the wings
                for (int k = -1; k <= 1; k += 2) {
                    Vec2 pod = Vec2(p.x + f.x * 6.0f + sd.x * k * wing * 0.62f, p.y + f.y * 6.0f + sd.y * k * wing * 0.62f);
                    g.glowAdd(pod.x, pod.y, 11.0f, Color{255, 236, 190, 190});
                    if (frameDt > 0 && fxRng.f() < 30.0f * frameDt) emitP(pod.x + cam.x, pod.y + cam.y, -f.x * 30, -f.y * 30, fxRng.f(0.8f, 1.4f), 3, 9, rgb(236, 232, 224, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.5f, fxRng.f(0, 6), 0.2f);
                }
            }
        }
    }
    if (air && std::abs(e.roll) > 0.02f) g.drawRolled(*bs, p.x, p.y, drawAng, e.roll, usc, mod, ghost);   // banking into a turn, or a barrel roll
    else g.draw(*bs, p.x, p.y, drawAng, usc, mod, ghost);
    if (deckJet && e.deckPhase == 2 && frameDt > 0) {   // on the catapult: steam streams back off the track behind it
        Vec2 f(std::cos(e.angle), std::sin(e.angle));
        for (float q = fxRng.f(); q < 70.0f * frameDt; q += 1.0f)
            emitP(e.pos.x - f.x * fxRng.f(6, 30), e.pos.y - f.y * fxRng.f(6, 30), -f.x * fxRng.f(10, 40) + fxRng.f(-8, 8), -f.y * fxRng.f(10, 40) + fxRng.f(-8, 8),
                  fxRng.f(0.5f, 0.9f), 2.5f, 8.0f, rgb(240, 244, 248, 170), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 1.2f, fxRng.f(0, 6), 0.2f);
    }
    if (ut.naval) drawShipMounts(e, p, mod, ghost);
    if (e.type == U_C_CARRIER || e.type == U_K_CARRIER) drawCarrierDeck(e, p, drawAng, mod, ghost);
    if (ut.naval && e.fade < 1.0f) {   // a called ship coming out of the sea haze at the map border: a bank of mist round the hull thins as it sails clear
        float k = 1.0f - clampf(e.fade, 0, 1), L = e.radius() * 2.6f;
        g.drawSized(g.disc, p.x, p.y, L * 1.6f, L, e.angle, rgb(226, 234, 242), (u8)(40 * k));
        g.glowAdd(p.x, p.y, L * 0.6f, Color{200, 220, 236, (u8)(50 * k)});
        if (frameDt > 0) for (float q = fxRng.f(); q < 26.0f * k * frameDt * (e.radius() / 12.0f); q += 1.0f) {   // banks of sea mist rolling off the hull
            float a = fxRng.f(0, 6.2832f), r = fxRng.f(0.2f, 1.0f) * L * 0.8f;
            emitP(e.pos.x + std::cos(a) * r, e.pos.y + std::sin(a) * r * 0.7f, fxRng.f(-6, 6), fxRng.f(-6, 6), fxRng.f(1.2f, 2.2f), L * 0.35f, L * 0.8f,
                  rgb(232, 238, 244, 130), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.6f, fxRng.f(0, 6), fxRng.f(-0.2f, 0.2f));
        }
    }
    if (ut.kind == UK_VEH && ut.cargoCap > 0 && !ut.naval) drawJeepCrew(e, p);
    const Sprite& tur = g.unitTurret[e.type][slotOf(owner)];
    if (tur.tex && !ut.naval) {
        // the barrel kicks back for a moment after every shot
        float rec = 0;
        if (ut.weapon >= 0 && !paused) { const Weapon& w = WEAPONS[ut.weapon]; float since = w.cooldown - e.cooldown; if (w.cooldown > 0.6f && since >= 0 && since < 0.15f) rec = 1.0f - since / 0.15f; }
        g.draw(tur, p.x - std::cos(e.turret) * 3.2f * rec, p.y - std::sin(e.turret) * 3.2f * rec, e.turret, 1, mod);
    }
    if (fallK > 0) {   // parachute: shroud lines up to the canopy, which opens just after the drop and collapses as the unit touches down
        float cs = ut.kind == UK_VEH ? 1.7f : 1.05f;
        float opened = clampf((e.fallTime - e.fall) / 0.4f, 0.2f, 1.0f), flare = clampf(e.fall / 0.35f, 0.0f, 1.0f);
        float sway = std::sin(wallTime * 3.1f + (float)e.gen * 1.7f) * 2.2f * flare;
        float R = 23.0f * UNIT_SCALE * cs * opened * (0.55f + 0.45f * flare);
        float cx = p.x + sway, cy = p.y - (10.0f + 9.0f * cs) * (0.6f + 0.4f * opened);
        Color ln = rgb(34, 34, 38, 190);
        for (int k = -2; k <= 2; k++) if (k != 0) g.line(cx + k * R * 0.42f, cy + R * 0.38f, p.x, p.y - 2, ln);
        g.draw(g.chute[slotOf(owner)], cx, cy, sway * 0.02f, cs * opened * (0.55f + 0.45f * flare), rgb(255, 255, 255), (u8)(flare < 0.3f ? 120 + 450 * flare : 255));
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
    if (ut.kind == UK_AIR && ut.heli) {   // main rotor: motion-blur disc plus a spinning blade pair over the hub
        float spin = paused || disabled ? 0.4f : wallTime * (ut.faction == F_CYBER ? -42.0f : 38.0f);
        if (ut.cargoCap > 0) {   // a cargo lifter has two big rotors, fore and aft
            Vec2 fw(std::cos(e.angle), std::sin(e.angle));
            for (int k = -1; k <= 1; k += 2) {
                Vec2 hub = p + fw * (k * 21.0f);
                g.draw(g.rotorDisc, hub.x, hub.y, 0, 1.55f, rgb(255, 255, 255), disabled ? 40 : 235);
                g.draw(g.rotorBlades, hub.x, hub.y, spin * (float)k, 1.55f, ut.faction == F_CYBER ? rgb(200, 240, 255) : rgb(255, 255, 255), 150);
            }
        } else if (ut.role == UR_SCOUT) {   // a spy drone: a small, faint rotor
            g.draw(g.rotorDisc, p.x, p.y, 0, 0.6f, rgb(255, 255, 255), (u8)(disabled ? 30 : 150));
            g.draw(g.rotorBlades, p.x, p.y, spin * 1.3f, 0.6f, rgb(200, 240, 255), 110);
        } else {
            Vec2 hub = ut.faction == F_CYBER ? p : Vec2(p.x - 2, p.y);
            g.draw(g.rotorDisc, hub.x, hub.y, 0, 1, rgb(255, 255, 255), disabled ? 40 : 255);
            g.draw(g.rotorBlades, hub.x, hub.y, spin, 1, ut.faction == F_CYBER ? rgb(200, 240, 255) : rgb(255, 255, 255), 150);
        }
    }
    if (e.cargo > 0) { float bx = p.x - std::cos(e.angle) * 2, by = p.y - std::sin(e.angle) * 2; g.fillCircle(bx, by, 3.2f, rgb(240, 205, 90)); g.fillCircle(bx - 0.8f, by - 0.8f, 1.4f, rgb(255, 240, 170)); }
    drawEscorts(e, p);
    if (e.owner == RIOT_PLAYER) {   // the crowd: the petrol bombs' rags burn, and now and then somebody shouts
        u32 h = e.gen * 2654435761u;
        if (e.type == U_FIREBOMBER) {
            float fl = 0.6f + 0.4f * std::sin(wallTime * 17.0f + (h % 50)) * std::sin(wallTime * 7.3f + (h % 13));
            Vec2 hand = p + Vec2(std::cos(e.angle), std::sin(e.angle)) * 9.0f;
            g.glowAdd(hand.x, hand.y - 2, 9.0f + 3.0f * fl, Color{255, 140, 40, (u8)(110 + 70 * fl)});
            g.fillCircle(hand.x, hand.y - 2, 1.6f, rgb(255, 230, 150));
        }
        float ph = std::fmod(wallTime * 0.45f + (h % 997) * 0.013f, 3.0f);
        if (ph < 0.9f && g_sim.time - e.lastDamaged > 2.0f) {   // a shout: a jagged white bubble with an exclamation mark
            float bx = p.x + 4, by = p.y - 20 - ph * 3.0f;
            g.fill((int)bx, (int)by, 7, 8, rgb(250, 248, 240, 220));
            g.fill((int)bx + 1, (int)by + 8, 2, 2, rgb(250, 248, 240, 220));
            g.fill((int)bx + 3, (int)by + 1, 1, 4, rgb(200, 40, 30)); g.fill((int)bx + 3, (int)by + 6, 1, 1, rgb(200, 40, 30));
        }
    }
    if (selected || e.hp < e.maxHp) hpBar(g, p.x - 10 - (ut.naval ? 8.0f : 0.0f), p.y - e.radius() - 7, 20 + (ut.naval ? 16.0f : 0.0f) + (ut.cargoCap > 0 && ut.kind == UK_AIR ? 24.0f : 0.0f), e.hp / e.maxHp);
    if (e.rank > 0) {   // veteran / elite chevrons beside the health bar
        Color cc = e.rank == 2 ? rgb(255, 224, 110) : rgb(200, 236, 255);
        for (int k = 0; k < e.rank; k++) { float cx = p.x + 12 + k * 4, cy = p.y - e.radius() - 5; g.line(cx - 2, cy - 1, cx, cy + 1, cc); g.line(cx, cy + 1, cx + 2, cy - 1, cc); }
    }
    if (ut.role == UR_SPY && e.order == O_CAPTURE && e.actionTimer > 0) {   // capture progress
        const Entity* tb = g_sim.get(e.targetEnt);
        if (tb) hpBar(g, p.x - 14, p.y - e.radius() - 12, 28, e.actionTimer / g_sim.captureTime(*tb));
    }
    if (ut.cargoCap > 0 && ut.naval && (selected || !e.passengers.empty())) {   // how full a cargo ship is: a bar of its places
        float k = g_sim.cargoUsed(e) / (float)ut.cargoCap;
        g.fill((int)p.x - 30, (int)(p.y - e.radius() - 14), 60, 4, rgb(50, 54, 60));
        g.fill((int)p.x - 30, (int)(p.y - e.radius() - 14), (int)(60 * k), 4, rgb(240, 205, 90));
    }
    if (ut.cargoCap > 0 && ut.kind == UK_VEH && !ut.naval && e.owner == g_sim.humanPlayer && (selected || !e.passengers.empty())) {   // a jeep: one pip per seat
        for (int k = 0; k < JEEP_SEATS; k++) g.fill((int)p.x - JEEP_SEATS * 2 + k * 4, (int)(p.y - e.radius() - 12), 3, 3, k < (int)e.passengers.size() ? rgb(240, 205, 90) : rgb(70, 74, 80));
    }
    if (ut.cargoCap > 0 && ut.kind == UK_AIR && (selected || !e.passengers.empty())) {   // how full a lifter is: one pip per five places
        int used = g_sim.cargoUsed(e), pips = ut.cargoCap / 5;
        for (int k = 0; k < pips; k++) g.fill((int)p.x - pips * 2 + k * 4, (int)(p.y - e.radius() - 12), 3, 3, k * 5 < used ? rgb(240, 205, 90) : rgb(70, 74, 80));
    }
    if (selected && air && ut.ammo > 0) for (int k = 0; k < ut.ammo; k++) g.fill((int)p.x - 10 + k * 3, (int)p.y - e.radius() - 11, 2, 2, k < e.ammo ? rgb(255, 230, 120) : rgb(80, 80, 80));
}

// ONEHOUR_RENDERPROF=ms (profiling, with SDL_RENDER_BATCHING=0 so each draw runs when it is issued): print any world frame slower than that,
// section by section
namespace {
struct RenderProf {
    static int limit() { static int v = getenv("ONEHOUR_RENDERPROF") ? atoi(getenv("ONEHOUR_RENDERPROF")) : -1; return v; }
    u64 t0 = 0, last = 0; const char* names[32]; double ms[32]; int n = 0;
    RenderProf() { if (limit() >= 0) t0 = last = SDL_GetPerformanceCounter(); }
    void mark(const char* name) { if (limit() < 0 || n >= 32) return; u64 now = SDL_GetPerformanceCounter(); names[n] = name; ms[n++] = (now - last) * 1000.0 / SDL_GetPerformanceFrequency(); last = now; }
    ~RenderProf() {
        if (limit() < 0) return;
        mark("rest");
        double tot = (SDL_GetPerformanceCounter() - t0) * 1000.0 / SDL_GetPerformanceFrequency();
        if (tot < limit()) return;
        fprintf(stderr, "[renderprof t=%.1f] %.1f ms:", g_sim.time, tot);
        for (int i = 0; i < n; i++) if (ms[i] >= 0.5) fprintf(stderr, " %s %.1f", names[i], ms[i]);
        fprintf(stderr, "\n");
    }
};
}

void Game::renderWorld() {
    RenderProf rp;
    Gfx& g = g_gfx;
    g.refreshTerrain();   // trees and rocks cleared for new structures since the last frame
    updateWeather();
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
        g.drawWorld(sx0, sy0, sw, sh, sx0 - (int)cam.x, sy0 - (int)cam.y);
    } rp.mark("terrain");
    // water: seamless animated caustics over open water. Interior water is pre-grouped by the map into aligned 4x4 blocks
    // (one 128 px draw each) plus the leftover shore-side tiles, so a lake filling the screen costs dozens of draws, not hundreds.
    {
        int fr = ((int)(wallTime * 3.0f)) & 3;
        const Sprite& w = g.waterFx[fr];
        SDL_SetTextureColorMod(w.tex, 255, 255, 255);
        SDL_SetTextureAlphaMod(w.tex, (u8)(176 + 22 * std::sin(wallTime * 0.6f)));   // the light on the water breathes a little
        // the pattern is laid at twice its size (it repeats every 8 tiles, not 4): each 4x4 block shows one quarter of the texture, scaled up
        const u8 causA = (u8)(176 + 22 * std::sin(wallTime * 0.6f));
        SDL_SetTextureAlphaMod(w.tex, causA);
        for (int idx : g_map.waterBlocks) {   // the lake and the rivers
            int tx = idx % MAP_W, ty = idx / MAP_W;
            if (tx + 3 < tx0 || tx > tx1 || ty + 3 < ty0 || ty > ty1) continue;
            if (!rev && !ex[(ty + 1) * MAP_W + tx + 1] && !ex[(ty + 2) * MAP_W + tx + 2]) continue;
            SDL_Rect src = { (tx & 4) * 16, (ty & 4) * 16, 64, 64 };
            SDL_Rect dst = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), 4 * TILE, 4 * TILE };
            SDL_RenderCopy(g.ren, w.tex, &src, &dst);
        }
        // the ocean, all of it, shore to open sea: one slow swell (the 512 px pattern at full size, 16 tiles before it repeats) laid in the
        // biggest aligned pieces that fit, so there is no seam anywhere; then its shallows get caustics tile by tile, fading out with depth
        const Sprite& sw = g.seaSwell[((int)(wallTime * 1.6f)) & 3];
        SDL_SetTextureAlphaMod(sw.tex, (u8)(170 + 26 * std::sin(wallTime * 0.4f)));
        auto swell = [&](int idx, int n) {
            int tx = idx % MAP_W, ty = idx / MAP_W;
            if (tx + n - 1 < tx0 || tx > tx1 || ty + n - 1 < ty0 || ty > ty1) return;
            if (!rev && !ex[(ty + n / 2) * MAP_W + tx + n / 2] && !ex[ty * MAP_W + tx] && !ex[(ty + n - 1) * MAP_W + tx + n - 1]) return;
            SDL_Rect src = { (tx & 15) * 32, (ty & 15) * 32, n * 32, n * 32 }, dst = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), n * TILE, n * TILE };
            SDL_RenderCopy(g.ren, sw.tex, &src, &dst);
        };
        for (int idx : g_map.seaBlocks) swell(idx, 8);
        for (int idx : g_map.seaQuads) swell(idx, 4);
        for (int idx : g_map.seaTiles) swell(idx, 1);
        for (int idx : g_map.seaShallow) {
            int tx = idx % MAP_W, ty = idx / MAP_W;
            if (tx < tx0 || tx > tx1 || ty < ty0 || ty > ty1 || (!rev && !ex[idx])) continue;
            float k = clampf((Map::SEA_CAUSTIC_REACH + 1.0f - g_map.shoreDist[idx]) / (float)Map::SEA_CAUSTIC_REACH, 0.0f, 1.0f);
            SDL_SetTextureAlphaMod(w.tex, (u8)(causA * k * k));
            SDL_Rect src = { (tx & 7) * 16, (ty & 7) * 16, 16, 16 };
            SDL_Rect dst = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), TILE, TILE };
            SDL_RenderCopy(g.ren, w.tex, &src, &dst);
        }
        SDL_SetTextureAlphaMod(w.tex, causA);
        for (int idx : g_map.waterTiles) {
            int tx = idx % MAP_W, ty = idx / MAP_W;
            if (tx < tx0 || tx > tx1 || ty < ty0 || ty > ty1) continue;
            if (!rev && !ex[idx]) continue;
            SDL_Rect src = { (tx & 7) * 16, (ty & 7) * 16, 16, 16 };
            SDL_Rect dst = { (int)(tx * TILE - cam.x), (int)(ty * TILE - cam.y), TILE, TILE };
            SDL_RenderCopy(g.ren, w.tex, &src, &dst);
        }
    }
    drawSeaLife(); rp.mark("water+sealife");
    drawGroundFx(); rp.mark("groundfx");
    // placement ghost
    if (placingType >= 0 && bulkDrag) {   // a shift-dragged line: every footprint of it, green where it goes up, red where it cannot (or the money runs out)
        Vec2 w = screenToWorld(mouseX, mouseY);
        const BuildType& bt = BUILDS[placingType];
        std::vector<std::pair<int, int>> spots;
        bulkSpots(placingType, tileOf(w.x) - bt.w / 2, tileOf(w.y) - bt.h / 2, spots);
        int money = g_sim.players[g_sim.humanPlayer].money, n = 0, cost = 0;
        for (auto& s : spots) {
            bool ok = g_sim.canPlace(g_sim.humanPlayer, placingType, s.first, s.second);
            bool paid = ok && money - cost >= bt.cost;
            if (paid) { n++; cost += bt.cost; }
            Vec2 c = worldToScreen(g_sim.buildingCenter(placingType, s.first, s.second));
            Color gt = paid ? rgb(160, 255, 160) : (ok ? rgb(255, 200, 110) : rgb(255, 120, 120));
            g.draw(g.building[placingType], c.x, c.y, 0, 1, gt, 130);
            int sx = (int)(s.first * TILE - cam.x), sy = (int)(s.second * TILE - cam.y);
            g.fill(sx, sy, bt.w * TILE, bt.h * TILE, paid ? rgb(80, 255, 80, 46) : (ok ? rgb(255, 190, 80, 60) : rgb(255, 60, 60, 70)));
            g.box(sx, sy, bt.w * TILE, bt.h * TILE, paid ? rgb(150, 255, 150, 170) : rgb(255, 140, 120, 170));
        }
        char lb[96];
        snprintf(lb, sizeof lb, "%d x %s   $%d%s", n, bt.name, cost, n < (int)spots.size() ? "   (red: blocked or no money)" : "");
        g.text(mouseX + 14, mouseY + 10, lb, rgb(230, 250, 220));
        g.text(mouseX + 14, mouseY + 22, "release to place   right-click / Esc: cancel", hudDim());
    } else
    if (placingType >= 0) {
        Vec2 w = screenToWorld(mouseX, mouseY);
        const BuildType& bt = BUILDS[placingType];
        int tx = tileOf(w.x) - bt.w / 2, ty = tileOf(w.y) - bt.h / 2;
        bool ok = g_sim.canPlace(g_sim.humanPlayer, placingType, tx, ty);
        Vec2 c = worldToScreen(g_sim.buildingCenter(placingType, tx, ty));
        if (bt.weapon >= 0) rangeRing(c, WEAPONS[bt.weapon].range * TILE + std::max(bt.w, bt.h) * TILE * 0.5f, bt.weapon, bt.name, true, false, false);
        if (bt.role == BR_ANTINUKE) drawCoverRing(c, (bt.faction == F_CYBER ? ANTINUKE_COVER_C : ANTINUKE_COVER_K) * TILE, hudAccent(bt.faction), true, "Anti-nuke cover");
        if (bt.role == BR_NAVAL) {   // a Naval Yard must touch the ocean or the lake: the open water around the footprint is outlined, and a hint says what is wrong
            for (int j = ty - 1; j <= ty + bt.h; j++) for (int i = tx - 1; i <= tx + bt.w; i++) {
                bool inside = i >= tx && i < tx + bt.w && j >= ty && j < ty + bt.h, corner = (i < tx || i >= tx + bt.w) && (j < ty || j >= ty + bt.h);
                if (inside || corner || !g_map.bigWater(i, j) || !g_map.navTile(i, j)) continue;
                g.fill((int)(i * TILE - cam.x), (int)(j * TILE - cam.y), TILE, TILE, rgb(90, 200, 255, 70));
            }
            if (!ok) g.text((int)c.x - 90, (int)c.y - bt.h * TILE / 2 - 16, "Naval Yard: its edge must touch the open sea or the lake", rgb(255, 170, 130));
        }
        { Color gt = ok ? rgb(160, 255, 160) : rgb(255, 120, 120); g.draw(g.building[placingType], c.x, c.y, 0, 1, gt, 150); if (g.buildingTeam[placingType].tex) g.draw(g.buildingTeam[placingType], c.x, c.y, 0, 1, gt, 150); }
        bool tilesOk = true;
        if (bt.role == BR_WAYMAKER) {   // what it will clear (orange), open ground it only grades (green), and what it leaves alone (structures, water, piles: blue)
            int rough = 0, kept = 0;
            for (int j = 0; j < bt.h; j++) for (int i = 0; i < bt.w; i++) {
                int x = tx + i, y = ty + j;
                bool seen = inMap(x, y) && g_sim.explored(g_sim.humanPlayer, x, y);
                bool t = seen && g_map.wayTile(x, y);
                bool r = g_sim.waymakerClears(g_sim.humanPlayer, x, y);
                rough += r; kept += seen && !t;
                int sx = (int)(x * TILE - cam.x), sy = (int)(y * TILE - cam.y);
                g.fill(sx, sy, TILE, TILE, !seen ? rgb(40, 40, 40, 90) : (!t ? rgb(90, 170, 255, 64) : (r ? rgb(255, 176, 70, 92) : rgb(80, 255, 80, 50))));
                if (seen && !t) g.box(sx + 2, sy + 2, TILE - 4, TILE - 4, rgb(150, 210, 255, 150));   // (kept as it is)
            }
            const char* why = rough == 0 ? "Waymaker: nothing to clear here" : (!ok ? "Waymaker: another Waymaker is already clearing this" : nullptr);
            if (why) g.text((int)c.x - g.textW(why) / 2, (int)c.y - bt.h * TILE / 2 - 16, why, rgb(255, 180, 140));
            else {
                const char* hint = kept > 0 ? "Clears only the trees and rock: your structures, water and piles (blue) stay" : "Clears trees and rock into open ground  (shift-drag: a road, Esc to stop)";
                g.text((int)c.x - g.textW(hint) / 2, (int)c.y - bt.h * TILE / 2 - 16, hint, rgb(230, 240, 210));
            }
        } else
        for (int j = 0; j < bt.h; j++) for (int i = 0; i < bt.w; i++) {
            bool t = g_map.buildable(tx + i, ty + j) && g_sim.explored(g_sim.humanPlayer, clampi(tx + i, 0, MAP_W - 1), clampi(ty + j, 0, MAP_H - 1));
            tilesOk = tilesOk && t;
            int sx = (int)((tx + i) * TILE - cam.x), sy = (int)((ty + j) * TILE - cam.y);
            bool veg = t && g_map.vegetation(tx + i, ty + j);   // a tree or a small rock the structure will clear away
            g.fill(sx, sy, TILE, TILE, !t ? rgb(255, 60, 60, 90) : (veg ? rgb(150, 255, 90, 78) : rgb(80, 255, 80, 60)));
            if (veg) { g.line((float)sx + 9, (float)sy + 9, (float)sx + TILE - 9, (float)sy + TILE - 9, rgb(220, 255, 190, 170)); g.line((float)sx + TILE - 9, (float)sy + 9, (float)sx + 9, (float)sy + TILE - 9, rgb(220, 255, 190, 170)); }
        }
        if (!ok && tilesOk && bt.role != BR_NAVAL && bt.role != BR_WAYMAKER) {   // the ground is fine but the site is not: say why, and outline the supply-pile margin it touches
            const char* why = "Would close a passage: leave units a way through";
            if (!wallRole(bt.role)) for (int j = ty - 1; j <= ty + bt.h; j++) for (int i = tx - 1; i <= tx + bt.w; i++) {
                if (!inMap(i, j) || !(g_map.blocked[j * MAP_W + i] & 4)) continue;
                why = "Too close to a supply pile: keep one tile clear round it";
                g.box((int)(i * TILE - cam.x), (int)(j * TILE - cam.y), TILE, TILE, rgb(255, 170, 90));
            }
            if (g_sim.atBuildLimit(g_sim.humanPlayer, placingType)) why = "Limit reached for this structure";
            g.text((int)c.x - g.textW(why) / 2, (int)c.y - bt.h * TILE / 2 - 16, why, rgb(255, 180, 140));
        }
    }
    if (nukeMode && rainBunker.valid()) {   // Rain Nukes: the wide circle the warheads fall in at random, and the size of one blast drifting round in it
        Vec2 m = Vec2(mouseX, mouseY);
        float pulse = 0.6f + 0.4f * std::sin(wallTime * 8.0f), R = RAIN_NUKE_AREA * TILE, nr = NUKE_RADIUS * TILE * std::sqrt(RAIN_NUKE_SCALE);
        g.discFill(m.x, m.y, R, rgb(255, 60, 40, (int)(26 * pulse)));
        g.dashedCircle(m.x, m.y, R, rgb(255, 90, 60), wallTime * 30.0f);
        g.circle(m.x, m.y, R + 2, rgb(255, 160, 110, 120), 96);
        for (int k = 0; k < RAIN_NUKES; k++) {
            float a = wallTime * 0.7f + k * 6.2832f / RAIN_NUKES, r = R * (0.3f + 0.4f * (k & 1)) ;
            Vec2 c = m + Vec2(std::cos(a), std::sin(a)) * r;
            g.discFill(c.x, c.y, nr, rgb(255, 80, 50, (int)(30 * pulse)));
            g.circle(c.x, c.y, nr, rgb(255, 120, 80, 170), 32);
        }
        g.circle(m.x, m.y, 4, rgb(255, 200, 90), 12);
    } else if (nukeMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        float pulse = 0.6f + 0.4f * std::sin(wallTime * 8.0f);
        float nr = NUKE_RADIUS * TILE * (nukeMini ? std::sqrt(MINI_NUKE_SCALE) : (nukeIsland ? std::sqrt(ISLAND_NUKE_SCALE) : (nukeDeep ? std::sqrt(UNUKE_SCALE) : 1.0f)));
        g.discFill(m.x, m.y, nr, nukeDeep ? rgb(200, 206, 216, (int)(30 * pulse)) : rgb(255, 60, 40, (int)(34 * pulse)));
        g.circle(m.x, m.y, nr, nukeDeep ? rgb(214, 218, 226) : rgb(255, 90, 60), 64);
        g.circle(m.x, m.y, nr * 0.35f, rgb(255, 200, 90), 32);
    }
    if (dropMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        Faction fac = g_sim.players[g_sim.humanPlayer].faction;
        float R = DROPS[fac].radius * TILE, pulse = 0.6f + 0.4f * std::sin(wallTime * 7.0f);
        Color ac = hudAccent(fac);
        g.discFill(m.x, m.y, R, rgb(ac.r, ac.g, ac.b, (int)(30 * pulse)));
        g.dashedCircle(m.x, m.y, R, ac, wallTime * 24.0f);
        // the plane's approach: along the line from the home base through the drop zone, in from the map edge
        Vec2 wp = screenToWorld(mouseX, mouseY), base = g_sim.players[g_sim.humanPlayer].basePos;
        Vec2 dir = (wp - base).len() > 6.0f * TILE ? (wp - base).norm() : (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - wp).norm();
        g.line(m.x - dir.x * 220, m.y - dir.y * 220, m.x - dir.x * R, m.y - dir.y * R, rgb(ac.r, ac.g, ac.b, 150));
        g.line(m.x + dir.x * R, m.y + dir.y * R, m.x + dir.x * 120, m.y + dir.y * 120, rgb(ac.r, ac.g, ac.b, 90));
        g.draw(g.cargoPlane[slotOf(g_sim.humanPlayer)][fac], m.x - dir.x * 150, m.y - dir.y * 150, std::atan2(dir.y, dir.x), 1.0f, rgb(255, 255, 255), 150);
    }
    if (aidMode && mouseY < VIEW_H) {
        Vec2 m = Vec2(mouseX, mouseY), wp = screenToWorld(mouseX, mouseY);
        const Color ac = rgb(150, 210, 250);
        float R = AID_RADIUS * TILE, pulse = 0.6f + 0.4f * std::sin(wallTime * 7.0f);
        g.discFill(m.x, m.y, R, rgb(ac.r, ac.g, ac.b, (int)(30 * pulse)));
        g.dashedCircle(m.x, m.y, R, ac, wallTime * 24.0f);
        Vec2 base = g_sim.players[g_sim.humanPlayer].basePos;
        Vec2 dir = (wp - base).len() > 6.0f * TILE ? (wp - base).norm() : (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - wp).norm();
        g.line(m.x - dir.x * 220, m.y - dir.y * 220, m.x - dir.x * R, m.y - dir.y * R, rgb(ac.r, ac.g, ac.b, 150));
        g.draw(g.aidPlane, m.x - dir.x * 150, m.y - dir.y * 150, std::atan2(dir.y, dir.x), 1.2f, rgb(255, 255, 255), 150);
        // who would get it: a line to the nearest unit or structure of that army (only where you have seen the ground)
        int who[MAX_PLAYERS]; int n = g_sim.aidCandidates(g_sim.humanPlayer, wp, who);
        for (int k = 0; k < n; k++) {
            const Entity* best = nullptr; float bd = 1e30f;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == who[k] && e.kind != EK_RESOURCE) { float d = g_sim.distToEntity(wp, e); if (d < bd) { bd = d; best = &e; } }
            if (!best || !g_sim.explored(g_sim.humanPlayer, clampi(tileOf(best->pos.x), 0, MAP_W - 1), clampi(tileOf(best->pos.y), 0, MAP_H - 1))) continue;
            Vec2 t = worldToScreen(best->pos);
            Color pc = playerColor(who[k]);
            g.line(m.x, m.y, t.x, t.y, rgb(pc.r, pc.g, pc.b, 170));
            g.circle(t.x, t.y, best->radius() + 4, pc, 20);
        }
    }
    if (powerMode) {
        Vec2 m = Vec2(mouseX, mouseY);
        g.circle(m.x, m.y, POWERS[g_sim.players[g_sim.humanPlayer].faction].radius * TILE, hudAccent(g_sim.players[g_sim.humanPlayer].faction), 40);
    }
    if (bomberMode) {   // the blast the bomb will make, and the straight line the bomber flies in on from its map corner
        Vec2 m = Vec2(mouseX, mouseY), wp = screenToWorld(mouseX, mouseY);
        float nr = NUKE_RADIUS * TILE * std::sqrt(NUKEBOMBER_SCALE), pulse = 0.6f + 0.4f * std::sin(wallTime * 8.0f);
        g.discFill(m.x, m.y, nr, rgb(255, 60, 40, (int)(30 * pulse)));
        g.circle(m.x, m.y, nr, rgb(255, 90, 60), 64);
        g.circle(m.x, m.y, nr * 0.35f, rgb(255, 200, 90), 32);
        Vec2 from = g_sim.bomberEntry(g_sim.humanPlayer, wp), fs = worldToScreen(from);
        Vec2 dir = (wp - from).norm();
        g.line(fs.x, fs.y, m.x, m.y, rgb(255, 150, 110, 150));
        g.line(m.x, m.y, m.x + dir.x * 260, m.y + dir.y * 260, rgb(255, 150, 110, 70));
        int bt = g_sim.players[g_sim.humanPlayer].faction == F_CYBER ? U_C_NUKEBOMBER : U_K_NUKEBOMBER;
        g.draw(g.unitBody[bt][slotOf(g_sim.humanPlayer)], m.x - dir.x * 170, m.y - dir.y * 170, std::atan2(dir.y, dir.x), 1.0f, rgb(255, 255, 255), 160);
    }
    if (strikeMode) {   // the called spot, the circle the fighters' rockets are scattered over, and the lane the package comes in along (the track a cargo plane would fly)
        Vec2 m = Vec2(mouseX, mouseY), wp = screenToWorld(mouseX, mouseY);
        Faction sf = g_sim.players[g_sim.humanPlayer].faction;
        Color ac = hudAccent(sf);
        float R = 6.0f * TILE, Rr = STRIKE_SPREAD * TILE, pulse = 0.5f + 0.5f * std::sin(wallTime * 7.0f);
        g.discFill(m.x, m.y, Rr, Color{255, 90, 70, (u8)(26 + 22 * pulse)});
        g.circle(m.x, m.y, Rr, rgb(255, 110, 90), 32);
        g.dashedCircle(m.x, m.y, R, ac, wallTime * 24.0f);
        for (int k = 0; k < 4; k++) { float a = k * 1.5708f + 0.7854f; g.line(m.x + std::cos(a) * (Rr * 0.4f), m.y + std::sin(a) * (Rr * 0.4f), m.x + std::cos(a) * (Rr + 8), m.y + std::sin(a) * (Rr + 8), rgb(255, 130, 100)); }
        Vec2 base = g_sim.players[g_sim.humanPlayer].basePos;
        Vec2 dir = (wp - base).len() > 6.0f * TILE ? (wp - base).norm() : (Vec2(WORLD_W * 0.5f, WORLD_H * 0.5f) - wp).norm();
        // the three headings the waves come in on, and the first wave's V on its run-in
        int jt = sf == F_CYBER ? U_C_STRIKEJET : U_K_STRIKEJET;
        float base0 = std::atan2(dir.y, dir.x);
        for (int w = 0; w < 3; w++) {
            float a = base0 + (w == 0 ? 0.0f : (w == 1 ? 1.0f : -1.0f) * STRIKE_TURN);
            Vec2 d(std::cos(a), std::sin(a)), pp(-d.y, d.x);
            Vec2 sp = worldToScreen(wp - d * (R + 300.0f)), ep = worldToScreen(wp + d * (R + 120.0f));
            g.line(sp.x, sp.y, ep.x, ep.y, Color{ac.r, ac.g, ac.b, (u8)(w == 0 ? 150 : 80)});
            if (w != 0) continue;
            for (int i = 0; i < STRIKE_WAVE; i++) {
                int rank = (i + 1) / 2, side = (i & 1) ? 1 : -1;
                Vec2 q = worldToScreen(wp - d * (R + 200.0f + rank * 58.0f) + pp * (side * rank * 46.0f));
                g.draw(g.unitBody[jt][slotOf(g_sim.humanPlayer)], q.x, q.y, a, 0.9f, rgb(255, 255, 255), 150);
            }
        }
    }
    drawZones();
    drawRangeRings();
    drawStrikeMarks();
    drawFleetCall();
    drawDistrictLife();   // the towns' streets: walkers and cars (under the blocks' roofs and shadows)
    rp.mark("ghosts+zones+rings+town");
    // entities: resources, buildings, then ground units sorted by y, projectiles, fx, aircraft
    std::vector<Entity*> ground, air;
    float vx0 = cam.x - 96, vy0 = cam.y - 96, vx1 = cam.x + SCREEN_W + 96, vy1 = cam.y + VIEW_H + 96;
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        if (e.pos.x < vx0 || e.pos.x > vx1 || e.pos.y < vy0 || e.pos.y > vy1) continue;
        if (!g_sim.shownToHuman(e)) continue;   // never seen, or an enemy unit out of sight (remembered ground shows its structures only)
        if (e.carrier.valid() || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;   // aboard a carrier / hidden by stealth
        if (e.kind == EK_RESOURCE || e.isBuilding()) drawEntity(e);
        else if (e.isAir() || e.fall > 0) air.push_back(&e);
        else ground.push_back(&e);
    }
    std::sort(ground.begin(), ground.end(), [](Entity* a, Entity* b) { return a->pos.y < b->pos.y; });
    for (auto* e : ground) drawEntity(*e);
    rp.mark("entities");
    // flags: every finished structure flies its owner's flag from a mast on the roof edge (over units, under aircraft)
    for (auto& e : g_sim.ents) {
        if (!e.alive || !e.isBuilding() || !e.constructed || e.owner < 0 || e.bt().role == BR_BUNKER) continue;   // (a bunker flies no flag: it is underground)
        if (wallRole(e.bt().role) && (e.tx * 7 + e.ty * 3) % 4 != 0) continue;   // (a wall line flies a flag on every fourth segment, not a forest of them)
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
    drawClouds(); rp.mark("flags+clouds");
    drawWeatherTint();   // the light of the hour over the ground world; what follows (fire, tracers, aircraft) is lit on its own
    rp.mark("tint");
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
            const bool nuke = p.weapon == W_NUKE_BOMB;   // the Supersonic Bomber's compact nuclear bomb: much smaller than a stick bomb
            float t = clampf(p.arcT, 0, 1), h = (1.0f - t * t) * (nuke ? 54.0f : 78.0f);
            Vec2 gd = (p.dest - p.vel).norm();
            float sc = (nuke ? 0.5f : 0.7f) + (nuke ? 0.22f : 0.5f) * t;
            Color glow = nuke ? Color{220, 228, 240, 150} : (p.weapon == W_BOMB_CYBER ? Color{130, 225, 255, 130} : Color{255, 170, 90, 130});
            g.drawSized(g.shadowSmall, s.x, s.y + 2, (nuke ? 5.5f : 11.0f) * sc, (nuke ? 3.0f : 6.0f) * sc, 0, rgb(255, 255, 255), (u8)(90 + 90 * t));
            float bx = s.x, by = s.y - h;
            g.drawSized(g.fxs.streak, bx - gd.x * (nuke ? 2.0f : 5.0f), by - (nuke ? 3.0f : 6.0f), nuke ? 2 : 4, nuke ? 7 : 16, 1.5708f, rgb(220, 220, 230), (u8)(120 * (1 - t)));
            g.fillCircle(bx, by, nuke ? 1.7f : 3.6f, rgb(36, 38, 42));
            g.fillCircle(bx - 0.45f, by - 0.45f, nuke ? 0.7f : 1.5f, nuke ? rgb(196, 202, 212) : rgb(128, 132, 140));
            if (nuke) { g.line(bx - 1.2f, by - 2.1f, bx + 1.2f, by - 2.1f, rgb(160, 166, 176)); g.glowAdd(bx, by, 4, glow); }
            else { g.line(bx - 2.6f, by - 4.6f, bx + 2.6f, by - 4.6f, rgb(70, 74, 82)); g.glowAdd(bx, by, 7, glow); }
        } else if (w.proj == PJ_ROCKET) {
            Vec2 d = p.vel.norm();
            float ang = std::atan2(d.y, d.x);
            bool strike = p.weapon == W_STRIKE_C || p.weapon == W_STRIKE_K;   // the strike fighters' rockets: longer, hotter, with a smoke line that hangs in the air
            Vec2 mid = s - d * (strike ? 18.0f : 9.0f);
            g.drawSized(g.fxs.streak, mid.x, mid.y, strike ? 44 : 22, strike ? 6.0f : 4.0f, ang, rgb(255, 214, 150), 255);
            g.glowAdd(s.x, s.y, strike ? 12 : 7, Color{w.color.r, w.color.g, w.color.b, (u8)(strike ? 190 : 150)});
            g.fillCircle(s.x, s.y, strike ? 2.4f : 1.9f, rgb(255, 250, 235));
            if (strike && frameDt > 0 && fxRng.f() < 40.0f * frameDt) emitP(p.pos.x - d.x * 12, p.pos.y - d.y * 12, fxRng.f(-6, 6), fxRng.f(-6, 6), fxRng.f(1.2f, 2.0f), 3, 11, rgb(238, 236, 232, 190), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.4f, fxRng.f(0, 6), 0.2f);
        } else {
            Vec2 d = p.vel.norm();
            float ang = std::atan2(d.y, d.x);
            Vec2 mid = s - d * 8.0f;
            g.drawSized(g.fxs.streak, mid.x, mid.y, 18, 2.6f, ang, w.color, 255);
            g.glowAdd(s.x, s.y, 4.5f, Color{w.color.r, w.color.g, w.color.b, 110});
        }
    }
    drawParticles(0); rp.mark("projs+parts0");
    drawFx(); rp.mark("fx");
    drawParticles(1); rp.mark("parts1");
    drawNukes(); rp.mark("nukes");
    for (auto* e : air) drawEntity(*e);
    rp.mark("air");
    drawAirlifts(); rp.mark("airlifts");
    drawShroud(); rp.mark("shroud");
    drawDistrictLabels();
    drawAidDrops();
    drawRain(); rp.mark("labels+aid+rain");
    if (!g.softwareRenderer && g.vignette.tex) g.drawRect(g.vignette, 0, 0, SCREEN_W, VIEW_H);   // soft dark corners frame the battlefield (one draw, GPU only)
    // drag box
    if (dragging) {
        Vec2 a = worldToScreen(dragStart), b = worldToScreen(dragNow);
        g.fill((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 30));
        g.box((int)std::min(a.x, b.x), (int)std::min(a.y, b.y), (int)std::abs(b.x - a.x), (int)std::abs(b.y - a.y), rgb(120, 255, 120, 200));
    }
    // cursor hints
    if (areaMode) {}
    else if (attackMoveMode) g.text(mouseX + 12, mouseY - 4, "ATTACK", rgb(255, 90, 80));
    else if (forceMode) {
        Entity* h = mouseY < VIEW_H ? pickEntity(screenToWorld(mouseX, mouseY), false) : nullptr;
        char buf[96];
        if (h && h->kind != EK_RESOURCE && h->owner >= 0) snprintf(buf, sizeof buf, "FORCE FIRE: %s%s", h->isUnit() ? h->ut().name : h->bt().name, h->owner == g_sim.humanPlayer ? " (yours)" : g_sim.enemies(g_sim.humanPlayer, h->owner) ? "" : " (ally)");
        else snprintf(buf, sizeof buf, "FORCE FIRE: pick a unit or structure");
        g.text(mouseX + 12, mouseY - 4, buf, rgb(255, 120, 90));
    }
    else if (nukeMode && rainBunker.valid()) {
        char rb[128]; snprintf(rb, sizeof rb, "RAIN NUKES: %d WARHEADS FALL AT RANDOM IN THE BIG CIRCLE (YOU TOO)", RAIN_NUKES);
        const char* rb2 = "Too fast for the defences: they almost never stop one";
        int tx = std::min(mouseX + 12, SCREEN_W - 6 - std::max(g.textW(rb), g.textW(rb2)));   // (kept on the screen)
        g.text(tx, mouseY - 18, rb, rgb(255, 100, 70));
        g.text(tx, mouseY - 6, rb2, rgb(255, 214, 140));
    }
    else if (nukeMode) {
        g.text(mouseX + 12, mouseY - 4, nukeMini ? "MINI NUKE (HITS EVERYONE IN THE CIRCLE, YOU TOO)" : (nukeIsland ? "ISLAND NUKE (HITS EVERYONE IN THE CIRCLE, YOU TOO)" : (nukeDeep ? "UNDERGROUND NUKE (HITS EVERYONE IN THE CIRCLE, YOU TOO)" : "NUKE (HITS EVERYONE IN THE CIRCLE, YOU TOO)")), nukeDeep ? rgb(220, 224, 232) : rgb(255, 100, 70));
        Vec2 wp = screenToWorld(mouseX, mouseY);
        float sc = nukeMini ? MINI_NUKE_SCALE : (nukeIsland ? ISLAND_NUKE_SCALE : (nukeDeep ? UNUKE_SCALE : 1.0f));
        int nd = g_sim.antiNukeCovering(wp, g_sim.humanPlayer, true, sc, nukeDeep);   // (only defences you have seen; only lasers count against a silver warhead)
        if (nd > 0) { char ab[200]; snprintf(ab, sizeof ab, "Anti-nuke cover here: %d defence%s, %d%% the warhead is shot down (30%% of the damage still falls)", nd, nd > 1 ? "s" : "", (int)(100.0f * g_sim.antiNukeChance(wp, g_sim.humanPlayer, true, sc, nukeDeep) + 0.5f)); g.text(mouseX + 12, mouseY + 8, ab, rgb(255, 214, 140)); }
    }
    else if (bomberMode) {
        Vec2 wp = screenToWorld(mouseX, mouseY);
        g.text(mouseX + 12, mouseY - 4, "NUCLEAR BOMBER: PICK THE TARGET (THE BOMB HITS EVERYONE IN THE CIRCLE)", rgb(255, 120, 90));
        int nd = g_sim.antiNukeCovering(wp, g_sim.humanPlayer, true, NUKEBOMBER_SCALE, false);
        if (nd > 0) { char ab[200]; snprintf(ab, sizeof ab, "Anti-nuke cover here: %d defence%s, %d%% the bomb is shot down", nd, nd > 1 ? "s" : "", (int)(100.0f * g_sim.antiNukeChance(wp, g_sim.humanPlayer, true, NUKEBOMBER_SCALE, false) + 0.5f)); g.text(mouseX + 12, mouseY + 8, ab, rgb(255, 214, 140)); }
    }
    else if (strikeMode) { char buf[128]; snprintf(buf, sizeof buf, "%s: %d strike fighters bomb the spot you pick", STRIKES[g_sim.players[g_sim.humanPlayer].faction].name, (int)g_sim.strikeManifest(g_sim.humanPlayer).size()); g.text(mouseX + 12, mouseY - 4, buf, hudAccent(g_sim.players[g_sim.humanPlayer].faction)); }
    else if (dropMode) g.text(mouseX + 12, mouseY - 4, tankBunker.valid() ? "TANK DROP: 10 tanks and a medic" : paraBunker.valid() ? "PARA BACKUP: 35 paratroopers" : DROPS[g_sim.players[g_sim.humanPlayer].faction].name, hudAccent(g_sim.players[g_sim.humanPlayer].faction));
    else if (fleetMode) { Vec2 gl, en, nn; bool ok = g_sim.fleetEntry(screenToWorld(mouseX, mouseY), gl, en, nn); g.text(mouseX + 12, mouseY - 4, ok ? "Fleet Call: the fleet forms up here" : "Fleet Call: pick a spot on the open ocean", ok ? hudAccent(g_sim.players[g_sim.humanPlayer].faction) : rgb(255, 150, 120)); }
    else if (aidMode) {
        int who[MAX_PLAYERS]; int n = g_sim.aidCandidates(g_sim.humanPlayer, screenToWorld(mouseX, mouseY), who);
        char buf[160];
        if (n == 0) { g.text(mouseX + 12, mouseY - 4, "AID DROP: nobody left to receive it", rgb(255, 150, 120)); }
        else if (n == 1) { snprintf(buf, sizeof buf, "AID DROP to the %s (%s)", armyName(who[0]).c_str(), g_sim.enemies(g_sim.humanPlayer, who[0]) ? "enemy" : "ally"); g.text(mouseX + 12, mouseY - 4, buf, playerColor(who[0])); }
        else { snprintf(buf, sizeof buf, "AID DROP: a tie, %d armies just as close (drawn at random)", n); g.text(mouseX + 12, mouseY - 4, buf, rgb(150, 210, 250)); }
    }
    else if (powerMode) { char buf[96]; snprintf(buf, sizeof buf, "%s%s", POWERS[g_sim.players[g_sim.humanPlayer].faction].name, forceFireKey() ? " (FRIENDLY FIRE)" : ""); g.text(mouseX + 12, mouseY - 4, buf, forceFireKey() ? rgb(255, 120, 90) : hudAccent(g_sim.players[g_sim.humanPlayer].faction)); }
    else if (rallyMode) g.text(mouseX + 12, mouseY - 4, "RALLY", rgb(120, 255, 120));
    else if (mouseY < VIEW_H) {
        Entity* h = pickEntity(screenToWorld(mouseX, mouseY), false);
        if (h && h->kind != EK_RESOURCE && h->owner >= 0 && forceFireKey() && !selection.empty()) {
            const char* nm = h->isUnit() ? h->ut().name : h->bt().name;
            char buf[96]; snprintf(buf, sizeof buf, "FORCE FIRE: %s%s", nm, h->owner == g_sim.humanPlayer ? " (yours)" : g_sim.enemies(g_sim.humanPlayer, h->owner) ? "" : " (ally)");
            g.text(mouseX + 12, mouseY - 4, buf, rgb(255, 120, 90));
        } else if (h && h->kind != EK_RESOURCE && h->owner != g_sim.humanPlayer) {
            const char* nm = h->isUnit() ? h->ut().name : h->bt().name;
            bool foe = g_sim.enemies(g_sim.humanPlayer, h->owner);
            char buf[128];
            if (h->district >= 0) { const District& D = g_sim.districts[h->district]; snprintf(buf, sizeof buf, "%s, %s (%s%s)", nm, D.name, h->owner < 0 ? "neutral: an infiltrator takes the whole " : foe ? "enemy-held " : "ally-held ", h->owner < 0 ? (D.city ? "city" : "town") : (D.city ? "city" : "town")); }
            else if (h->owner < 0) snprintf(buf, sizeof buf, "%s (neutral civilian%s)", nm, h->isBuilding() && h->bt().role == BR_STRONGHOLD ? ": send infantry to garrison it" : "");
            else if (h->owner == RIOT_PLAYER) snprintf(buf, sizeof buf, "%s (rioting civilians: they fight every army)", nm);
            else snprintf(buf, sizeof buf, "%s (%s%s)", nm, foe ? "" : "ally, ", FACTION_NAME[g_sim.players[h->owner].faction]);
            g.text(mouseX + 12, mouseY - 4, buf, foe ? rgb(255, 150, 140) : (h->owner < 0 ? rgb(228, 220, 190) : rgb(150, 235, 165)));
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
    // the fog over it: a small texture of 2x2-tile cells (never seen: dark; remembered: dimmed; in sight: clear), refreshed with the sight
    const bool rev = g_sim.revealed(g_sim.humanPlayer);
    if (!rev) {
        const int FW = MAP_W / 2, FH = MAP_H / 2;
        static u32 lastGen = ~0u; static float nextAt = 0;
        if (!g.miniFog) { g.miniFog = SDL_CreateTexture(g.ren, SDL_PIXELFORMAT_ABGR8888, SDL_TEXTUREACCESS_STREAMING, FW, FH); if (g.miniFog) SDL_SetTextureBlendMode(g.miniFog, SDL_BLENDMODE_BLEND); lastGen = ~0u; }
        if (g.miniFog && lastGen != g_sim.sightGen && wallTime >= nextAt) {
            static std::vector<u32> px; px.resize((size_t)FW * FH);
            for (int cy = 0; cy < FH; cy++) for (int cx = 0; cx < FW; cx++) {
                int k = cy * 2 * MAP_W + cx * 2, seen = 0, known = 0;
                for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int q = k + j * MAP_W + i; known |= ex[q]; seen |= g_sim.inSight.empty() ? 0 : g_sim.inSight[q]; }
                px[(size_t)cy * FW + cx] = seen ? 0u : (known ? (110u << 24 | 30u << 16 | 20u << 8 | 12u) : (255u << 24 | 12u << 16 | 8u << 8 | 5u));
            }
            SDL_UpdateTexture(g.miniFog, nullptr, px.data(), FW * 4);
            lastGen = g_sim.sightGen; nextAt = wallTime + 0.25f;
        }
        if (g.miniFog) { SDL_Rect r = { x, y, size, size }; SDL_RenderCopy(g.ren, g.miniFog, nullptr, &r); }
    }
    for (auto& e : g_sim.ents) {
        if (!e.alive) continue;
        int tx = clampi(tileOf(e.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(e.pos.y), 0, MAP_H - 1);
        if (e.kind == EK_RESOURCE) { if (rev || ex[ty * MAP_W + tx]) g.fill((int)(x + tx * sc), (int)(y + ty * sc), 2, 2, rgb(230, 200, 90)); continue; }
        if (!g_sim.shownToHuman(e)) continue;
        if (e.carrier.valid() || !g_sim.visibleTo(e, g_sim.humanPlayer)) continue;
        Color c = e.owner < 0 ? rgb(198, 190, 170) : playerColor(e.owner);
        int s = e.isBuilding() ? std::max(2, (int)(e.bt().w * sc)) : 2;
        g.fill((int)(x + e.pos.x / WORLD_W * size) - s / 2, (int)(y + e.pos.y / WORLD_H * size) - s / 2, s, s, c);
    }
    for (auto& D : g_sim.districts) {   // the towns round the lake: a frame in the colour of whoever holds them
        int tx = clampi(tileOf(D.center.x), 0, MAP_W - 1), ty = clampi(tileOf(D.center.y), 0, MAP_H - 1);
        if (!rev && !ex[ty * MAP_W + tx]) continue;
        int r = std::max(3, (int)(D.radius / WORLD_W * size)), mx = (int)(x + D.center.x / WORLD_W * size), my = (int)(y + D.center.y / WORLD_H * size);
        g.box(mx - r, my - r, 2 * r, 2 * r, D.owner < 0 ? rgb(236, 228, 200, 220) : playerColor(D.owner));
    }
    for (auto& a : g_sim.airlifts) {
        int tx = clampi(tileOf(a.pos.x), 0, MAP_W - 1), ty = clampi(tileOf(a.pos.y), 0, MAP_H - 1);
        if (!rev && a.owner != g_sim.humanPlayer && !ex[ty * MAP_W + tx]) continue;
        int mx = (int)(x + clampf(a.pos.x, 0, WORLD_W) / WORLD_W * size), my = (int)(y + clampf(a.pos.y, 0, WORLD_H) / WORLD_H * size);
        g.fill(mx - 2, my - 2, 5, 5, playerColor(a.owner)); g.box(mx - 3, my - 3, 7, 7, rgb(255, 255, 255, 200));
    }
    for (auto& a : g_sim.aidDrops) {   // your relief plane: white
        if (a.downed || a.pos.x < 0 || a.pos.y < 0 || a.pos.x > WORLD_W || a.pos.y > WORLD_H) continue;
        int mx = (int)(x + a.pos.x / WORLD_W * size), my = (int)(y + a.pos.y / WORLD_H * size);
        g.fill(mx - 2, my - 2, 5, 5, rgb(250, 250, 250)); g.box(mx - 3, my - 3, 7, 7, rgb(150, 210, 250));
    }
    for (auto& n : g_sim.nukes) {   // warheads in flight: the circle they will hit (amber once one has been shot down: only its pieces fall)
        float pr = Sim::nukeRadius(n) / WORLD_W * size;
        g.circle(x + n.pos.x / WORLD_W * size, y + n.pos.y / WORLD_H * size, std::max(3.0f, pr), n.broken ? rgb(255, 200, 110) : rgb(255, 70, 50), 20);
    }
    for (auto& m : g_sim.strikeMarks) {   // a called air strike: a pulsing cross (green for your team's, red for an enemy's you can see)
        bool mine = m.owner >= 0 && g_sim.players[m.owner].team == g_sim.players[g_sim.humanPlayer].team;
        if (!mine && !rev && !ex[clampi(tileOf(m.pos.y), 0, MAP_H - 1) * MAP_W + clampi(tileOf(m.pos.x), 0, MAP_W - 1)]) continue;
        int mx = (int)(x + m.pos.x / WORLD_W * size), my = (int)(y + m.pos.y / WORLD_H * size);
        Color c = mine ? rgb(120, 255, 150) : rgb(255, 86, 70);
        int r = 3 + (((int)(wallTime * 4)) & 1);
        g.line(mx - r, my, mx + r, my, c); g.line(mx, my - r, mx, my + r, c);
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
    {   // ORDER: how firmly the army holds its own people (low order means riots: see Sim::updateRiots)
        float ctl = g_sim.control[g_sim.humanPlayer];
        int ox = px + 196;
        Color oc = ctl > 0.66f ? rgb(110, 220, 120) : (ctl > 0.4f ? rgb(240, 200, 70) : rgb(240, 90, 70));
        g.text(ox, 6, "ORDER", hudDim());
        g.fill(ox + 34, 7, 44, 7, rgb(30, 34, 40));
        g.fill(ox + 34, 7, (int)(44 * clampf(ctl, 0, 1)), 7, oc);
        if (g_sim.time >= RIOT_GRACE && g_sim.unrest[g_sim.humanPlayer] > 0.75f && ((int)(wallTime * 3) & 1)) g.box(ox + 33, 6, 46, 9, rgb(255, 120, 90));   // a crowd is about to rise
    }
    int secs = (int)g_sim.time;
    snprintf(buf, sizeof buf, "%02d:%02d", secs / 60, secs % 60); g.text(SCREEN_W / 2 - 15, 6, buf, hudText());
    { const char* wl = weatherLabel(); g.text(SCREEN_W / 2 - 26 - g.textW(wl), 6, wl, hudDim()); }   // the hour and the weather
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
                float psc = (e->ut().jet ? 1.6f : (e->ut().cargoCap > 0 ? 1.55f : 2.0f)) / artScale(e->type);   // the big elite units and the long jets are scaled to sit inside the frame
                g.draw(g.unitBody[e->type][slotOf(e->owner)], INFO_X + 42, hy + 48, 0, psc);
                if (g.unitTurret[e->type][slotOf(e->owner)].tex) g.draw(g.unitTurret[e->type][slotOf(e->owner)], INFO_X + 42, hy + 48, 0, psc);
            } else {
                drawFitted(g, g.building[e->type], INFO_X + 42, hy + 48, 60);
                if (g.buildingTeam[e->type].tex) drawFitted(g, g.buildingTeam[e->type], INFO_X + 42, hy + 48, 60, &g.building[e->type], playerColor(e->owner));
            }
            const char* name = e->isUnit() ? e->ut().name : e->bt().name;
            g.text(INFO_X + 86, hy + 16, name, rgb(255, 255, 255), 2);
            if (e->isBuilding() && !e->constructed) { snprintf(buf, sizeof buf, e->district >= 0 ? "RAZED: its people are rebuilding it %d%%" : "Under construction %d%%", (int)(e->progress * 100)); g.text(INFO_X + 86, hy + 36, buf, rgb(240, 220, 130)); }
            else { snprintf(buf, sizeof buf, "HP %d / %d", (int)std::ceil(e->hp), (int)e->maxHp); g.text(INFO_X + 86, hy + 36, buf, hudText()); }
            hpBar(g, INFO_X + 86, hy + 48, 150, e->isBuilding() && !e->constructed ? e->progress : e->hp / e->maxHp, true);
            if (e->isUnit()) {
                const UnitType& ut = e->ut();
                if (ut.nukeBomb) snprintf(buf, sizeof buf, "Nuclear Bomb x1: an Underground Nuke's damage in a compact fireball (tail cannon vs air)"), g.text(INFO_X + 86, hy + 58, buf, hudDim());
                else if (ut.bomber) { const Weapon& bw = WEAPONS[ut.faction == F_CYBER ? W_BOMB_CYBER : W_BOMB_CLANKER]; snprintf(buf, sizeof buf, "%s x%d  dmg %d  blast %.1f  (guns vs air)", bw.name, BOMB_STICK, (int)bw.dmg, bw.splash); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                else if (ut.weapon >= 0) { const Weapon& w = WEAPONS[ut.weapon]; snprintf(buf, sizeof buf, "%s  dmg %d  range %.1f%s%s", w.name, (int)w.dmg, w.range, w.air ? "  AA" : "", w.ground ? "" : "  air only"); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.role == UR_HARVESTER) { snprintf(buf, sizeof buf, "Cargo $%d / %d", e->cargo, SUPPLY_PER_TRIP); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (ut.ammo > 0) { snprintf(buf, sizeof buf, "Ammo %d / %d", e->ammo, ut.ammo); g.text(INFO_X + 86, hy + 70, buf, hudDim()); }
                if (ut.role == UR_DECON) {
                    int zones = 0;
                    for (auto& f : g_sim.fallouts) if (dist(e->pos, f.pos) <= f.r + RADTRUCK_RADIUS * TILE) zones++;
                    if (zones) snprintf(buf, sizeof buf, "Decontamination field %d tiles: scrubbing %d fallout zone%s (the truck is taking damage)", (int)RADTRUCK_RADIUS, zones, zones == 1 ? "" : "s");
                    else snprintf(buf, sizeof buf, "Decontamination field %d tiles: drive into the fallout to scrub it, pull out before the truck melts", (int)RADTRUCK_RADIUS);
                    g.text(INFO_X + 86, hy + 70, buf, zones ? rgb(150, 240, 180) : hudDim());
                }
                if (ut.cargoCap > 0) { snprintf(buf, sizeof buf, "Cargo %d / %d places   %d aboard   (right-click it with units selected to load)", g_sim.cargoUsed(*e), ut.cargoCap, (int)e->passengers.size()); g.text(INFO_X + 86, hy + 70, buf, rgb(240, 205, 90)); }
                if (e->type == U_C_CARRIER || e->type == U_K_CARRIER) {   // the wing: ready on deck, rearming, in the air, and a replacement on its way
                    int ready = 0, rearm = 0, up = 0; Ref cr = g_sim.refOf(*e);
                    for (float t : e->deck) (t <= g_sim.time ? ready : rearm)++;
                    for (auto& j : g_sim.ents) if (j.alive && j.isUnit() && j.home == cr && (j.type == U_C_CARRIERJET || j.type == U_K_CARRIERJET)) up++;
                    if (e->deckReplace >= 0) snprintf(buf, sizeof buf, "Wing: %d ready on deck, %d rearming, %d in the air   (a replacement jet in %ds)", ready, rearm, up, (int)std::ceil(e->deckReplace - g_sim.time));
                    else snprintf(buf, sizeof buf, "Wing: %d ready on deck, %d rearming, %d in the air   (they launch at enemies within %d tiles)", ready, rearm, up, (int)CARRIER_RANGE);
                    g.text(INFO_X + 86, hy + 70, buf, rgb(240, 205, 90));
                }
                if (ut.role == UR_SPY) g.text(INFO_X + 86, hy + 70, "Right-click an enemy or neutral structure to take it (swims to islands)", rgb(240, 205, 90));
                if (e->rank > 0) { snprintf(buf, sizeof buf, "%s: %d kills (+%d%% damage)", e->rank == 2 ? "ELITE" : "VETERAN", (int)e->kills, 20 * e->rank); g.text(INFO_X + 86, hy + 70, buf, e->rank == 2 ? rgb(255, 224, 110) : rgb(200, 236, 255)); }
                else if (ut.kind == UK_INF && ut.weapon >= 0) { snprintf(buf, sizeof buf, "Kills %d (veteran at %d, elite at %d)", (int)e->kills, VET_KILLS, ELITE_KILLS); g.text(INFO_X + 86, hy + 70, buf, hudDim()); }
                if (ut.stealth) g.text(INFO_X + 86 + 190, hy + 36, ut.stealth == ST_DRONE ? "STEALTH: only enemy drones see it" : "STEALTH: only enemy spy drones see it", rgb(150, 220, 255));
                g.text(INFO_X + 86, hy + 84, ut.desc, hudDim());
                const char* st = (e->zoneR > 0 && (e->order == O_GUARDAREA || e->postOrder == O_GUARDAREA)) ? (e->order == O_ATTACK ? "Defending assigned area" : "Guarding assigned area") : (e->zoneR > 0 && (e->order == O_HARVEST || e->order == O_RETURN)) ? (e->order == O_RETURN ? "Returning (assigned area)" : "Gathering in assigned area") : e->order == O_IDLE ? "Idle" : e->order == O_MOVE ? "Moving" : e->order == O_ATTACKMOVE ? "Attack-moving" : e->order == O_ATTACK ? "Attacking" : e->order == O_HARVEST ? "Gathering" : e->order == O_RETURN ? "Returning" : e->order == O_BUILD ? "Constructing" : e->order == O_REARM ? "Rearming" : "Guarding";
                g.text(INFO_X + 86, hy + 98, st, accent);
                if (int dn = g_sim.droneCount(*e)) {   // protector drones: how many are ready to fire
                    int rdy = 0; for (int i = 0; i < dn; i++) if (e->droneAt[i] <= g_sim.time) rdy++;
                    snprintf(buf, sizeof buf, "Guard drones %d / %d ready: they shoot down rockets and shells aimed at it", rdy, dn); g.text(INFO_X + 86, hy + 110, buf, rgb(150, 220, 255));
                } else if ((ut.naval || ut.cargoCap > 0) && ut.kind == UK_VEH && e->owner == g_sim.humanPlayer && !g_sim.hasUpgrade(g_sim.humanPlayer, UPG_DRONES) && ut.naval) g.text(INFO_X + 86, hy + 110, "No drones yet: the Guard Drones upgrade at the tech structure gives every ship a screen", hudDim());
            } else if (e->district >= 0) {   // a block of a town round the lake: the town as a whole is what matters
                const District& D = g_sim.districts[e->district];
                bool ours = D.owner == g_sim.humanPlayer, foe = D.owner >= 0 && g_sim.enemies(g_sim.humanPlayer, D.owner);
                std::string holder = D.owner < 0 ? "nobody (neutral)" : ours ? "us" : armyName(D.owner) + (foe ? " (enemy)" : " (ally)");
                snprintf(buf, sizeof buf, "%s %s, held by %s", D.city ? "City of" : "Town of", D.name, holder.c_str());
                g.text(INFO_X + 86, hy + 58, buf, D.owner < 0 ? rgb(236, 228, 200) : playerColor(D.owner));
                bool pays = D.integrity >= DISTRICT_PAY_MIN;
                snprintf(buf, sizeof buf, "Integrity %d%%   %d blocks, %d razed   %d people   pays $%d / %ds", (int)(D.integrity * 100 + 0.5f), (int)D.blocks.size(), D.ruins, D.people, pays ? (int)(D.pay * D.integrity) : 0, (int)DISTRICT_PAY_INTERVAL);
                g.text(INFO_X + 86, hy + 70, buf, pays ? rgb(240, 220, 130) : rgb(255, 150, 120));
                const char* l3 = D.owner < 0 ? "Neutral: an infiltrator of any army takes the WHOLE town at once (right-click a block with an infiltrator)"
                               : ours ? "Ours: it pays us and our enemies will come for it. An enemy infiltrator can take it from us"
                               : foe ? "Enemy-held: send an infiltrator to take it all, or wreck it to stop the pay (it rebuilds itself)"
                                     : "Held by an ally: it pays them; our dozers may help rebuild it";
                g.text(INFO_X + 86, hy + 84, l3, hudDim());
                const char* l4 = !pays && D.owner >= 0 ? "TOO DAMAGED TO PAY until it is rebuilt past half its strength: dozers rebuild it much faster"
                               : "Never wiped out: razed blocks are rebuilt by their people (right-click with dozers to speed it up)";
                g.text(INFO_X + 86, hy + 96, l4, !pays && D.owner >= 0 ? rgb(255, 180, 120) : hudDim());
            } else {
                const BuildType& bt = e->bt();
                if (bt.power != 0) { snprintf(buf, sizeof buf, "Power %+d", bt.power); g.text(INFO_X + 86, hy + 58, buf, hudDim()); }
                if (bt.role == BR_BUNKER && e->constructed) {
                    snprintf(buf, sizeof buf, "Garrison %d / %d soldiers   Dozers %d / %d", g_sim.garrisonInfantry(*e), BUNKER_INF_CAP, g_sim.garrisonDozers(*e), BUNKER_DOZER_CAP); g.text(INFO_X + 86, hy + 58, buf, rgb(240, 205, 90));
                    if (e->hatch > 0) snprintf(buf, sizeof buf, "Hatch %d / %d: the garrison fires from it", (int)e->hatch, (int)e->hatchMax);
                    else snprintf(buf, sizeof buf, "HATCH DOWN: garrison silent until it is refitted");
                    g.text(INFO_X + 86, hy + 70, buf, e->hatch > 0 ? hudDim() : rgb(255, 140, 120));
                    int queued = 0; for (int u = 0; u < BU_COUNT; u++) queued += (e->bunkerQueue >> u) & 1;
                    if (e->bunkerBusy >= 0) snprintf(buf, sizeof buf, queued ? "Fitting %s: %d%%  (%d more queued)" : "Fitting %s: %d%%", BUNKER_UPGS[e->bunkerBusy].name, (int)(e->bunkerProgress * 100), queued);
                    else if (queued) snprintf(buf, sizeof buf, "%d upgrade%s queued: waiting for money", queued, queued > 1 ? "s" : "");
                    else snprintf(buf, sizeof buf, "Nuke-hardened (%d%% a hit), radiation-proof", (int)(BUNKER_NUKE_SHARE * 100 * (Sim::bunkerHas(*e, BU_SHELL) ? BUNKER_SHELL_NUKE : 1.0f) + 0.5f));
                    g.text(INFO_X + 86, hy + 84, buf, e->bunkerBusy >= 0 || queued ? rgb(255, 226, 150) : rgb(150, 220, 255));
                    float mw = std::max(0.0f, e->siloTimer - g_sim.time);
                    const char* tubes = Sim::bunkerHas(*e, BU_TRIPLE) ? "Triple nuke" : "Mini nuke";
                    // the launch and the calls it has: with the newer powers fitted the line is kept short (the keys are on the buttons)
                    const bool many = Sim::bunkerHas(*e, BU_RAIN) || Sim::bunkerHas(*e, BU_TANKS);
                    if (mw > 0) snprintf(buf, sizeof buf, "%s %d:%02d", many ? (Sim::bunkerHas(*e, BU_TRIPLE) ? "Triple" : "Mini") : tubes, (int)mw / 60, (int)mw % 60);
                    else snprintf(buf, sizeof buf, many ? "%s READY" : "%s READY (K)", many ? (Sim::bunkerHas(*e, BU_TRIPLE) ? "Triple" : "Mini") : tubes);
                    auto call = [&](bool has, float w, const char* name, const char* key) {
                        if (!has) return;
                        size_t l = strlen(buf);
                        if (w > 0) snprintf(buf + l, sizeof buf - l, many ? "  %s %ds" : "   %s %ds", name, (int)std::ceil(w));
                        else if (many) snprintf(buf + l, sizeof buf - l, "  %s READY", name);
                        else snprintf(buf + l, sizeof buf - l, "   %s READY (%s)", name, key);
                    };
                    call(Sim::bunkerHas(*e, BU_PARA), g_sim.bunkerParaWait(*e), "Paras", "P");
                    call(Sim::bunkerHas(*e, BU_RAIN), g_sim.bunkerRainWait(*e), "Rain", "J");
                    call(Sim::bunkerHas(*e, BU_TANKS), g_sim.bunkerTankWait(*e), "Tanks", "D");
                    g.text(INFO_X + 86, hy + 96, buf, mw > 0 ? rgb(255, 200, 120) : rgb(255, 110, 90));
                    std::string fit;
                    for (int u = 0; u < BU_COUNT; u++) if (Sim::bunkerHas(*e, u)) { if (!fit.empty()) fit += " "; fit += BUNKER_UPGS[u].tag; }
                    if (e->bunkerUpg == (u8)((1 << BU_COUNT) - 1)) fit = "every upgrade";   // (the whole list would run into the command grid)
                    if (!fit.empty()) { fit = "Fitted: " + fit; g.text(INFO_X + 86, hy + 108, fit.c_str(), accent); }
                    else { snprintf(buf, sizeof buf, "Income $%d / %ds   no upgrades fitted yet", (int)(BUNKER_INCOME * pl.econMul()), (int)BUNKER_INCOME_INTERVAL); g.text(INFO_X + 86, hy + 108, buf, rgb(240, 220, 130)); }
                }
                if (e->owner >= 0 && e->constructed && bt.role != BR_BUNKER) {   // structure upgrades of its army (a bunker's line shows its garrison)
                    const Player& op = g_sim.players[e->owner];
                    std::string ups;
                    for (int u = 0; u < UPG_COUNT; u++) if (op.upg[u]) { if (!ups.empty()) ups += "  "; ups += UPGRADES[op.faction][u].name; }
                    if (!ups.empty()) g.text(INFO_X + 86 + (bt.power != 0 ? 70 : 0), hy + 58, ups.c_str(), accent);
                }
                if (bt.role == BR_ANTINUKE && e->constructed) {
                    bool powered = !(pl.lowPower() && bt.power < 0) && e->disabledUntil <= g_sim.time;
                    float rw = std::max(0.0f, e->siloTimer - g_sim.time);
                    if (!powered) snprintf(buf, sizeof buf, "OFFLINE: %s", e->disabledUntil > g_sim.time ? "switched off by a pulse" : "no power");
                    else if (rw > 0) snprintf(buf, sizeof buf, "Reloading %ds", (int)std::ceil(rw));
                    else if (bt.faction == F_CYBER) { int ln = g_sim.antiNukeLinked(*e); snprintf(buf, sizeof buf, "ARMED: covers %d tiles, %d beams, %d%% vs a warhead (40%% sighting nerf, linked to %d laser%s)", (int)ANTINUKE_COVER_C, 3 + ln, (int)(100.0f * g_sim.antiNukeWarheadShot(*e) + 0.5f), ln, ln == 1 ? "" : "s"); }
                    else snprintf(buf, sizeof buf, "ARMED: covers %d tiles around it", (int)ANTINUKE_COVER_K);
                    g.text(INFO_X + 86, hy + 70, buf, powered && rw <= 0 ? accent : rgb(255, 200, 120));
                    int foe = -1; for (int q = 0; q < g_sim.numPlayers; q++) if (g_sim.enemies(g_sim.humanPlayer, q)) { foe = q; break; }
                    int nd = foe >= 0 ? g_sim.antiNukeCovering(e->pos, foe) : 1;
                    if (nd > 0) snprintf(buf, sizeof buf, "%d defence%s cover this spot: a warhead aimed here is shot down %d%% of the time (30%% of its damage still falls)", nd, nd == 1 ? "" : "s", (int)(100.0f * g_sim.antiNukeChance(e->pos, foe) + 0.5f));
                    else snprintf(buf, sizeof buf, "No working defence covers this spot: a warhead aimed here would not be engaged");
                    g.text(INFO_X + 86, hy + 84, buf, hudDim());
                    snprintf(buf, sizeof buf, "%d of %d anti-nuke defences built", g_sim.countRole(g_sim.humanPlayer, BR_ANTINUKE, false), ANTINUKE_MAX); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                } else if (bt.role == BR_NAVAL && e->constructed) {
                    int ships = 0; for (auto& u : g_sim.ents) if (u.alive && u.isUnit() && u.owner == g_sim.humanPlayer && u.ut().naval) ships++;
                    snprintf(buf, sizeof buf, "%d ship%s afloat   (right-click the lake to set where new ships sail)", ships, ships == 1 ? "" : "s"); g.text(INFO_X + 86, hy + 84, buf, hudDim());
                }
                if (bt.role != BR_BUNKER && bt.role != BR_ANTINUKE) {   // the description, word-wrapped to the room there is (two lines unless the row below is taken)
                    int maxW = (e->queue.empty() ? INFO_W - 96 : (INFO_W - 96) / 2);
                    std::vector<std::string> dl = wrapTip(g, bt.desc, maxW);
                    bool twoLines = !(bt.role == BR_TECH || bt.role == BR_NAVAL) && e->queue.empty();
                    for (size_t li = 0; li < dl.size() && li < (twoLines ? 2u : 1u); li++) {
                        std::string ln = dl[li];
                        if (li + 1 == (twoLines ? 2u : 1u) && dl.size() > li + 1) ln += " ...";
                        g.text(INFO_X + 86, hy + 70 + (int)li * 10, ln.c_str(), hudDim());
                    }
                }
                if (bt.role == BR_TECH && e->constructed) {
                    const ProgramType& pg = PROGRAMS[pl.faction];
                    if (pl.advTech) snprintf(buf, sizeof buf, "%s: researched, special units unlocked", pg.name);
                    else if (pl.researching) snprintf(buf, sizeof buf, "%s: researching %d%%%s", pg.name, (int)(pl.researchProgress * 100), pl.lowPower() ? " (slowed: low power)" : "");
                    else snprintf(buf, sizeof buf, "%s: not researched ($%d)", pg.name, pg.cost);
                    g.text(INFO_X + 86, hy + 84, buf, pl.advTech ? accent : hudDim());
                    {   // structure upgrades under way
                        std::string ups;
                        for (int u = 0; u < UPG_COUNT; u++) if (pl.upgBusy[u]) { char t[64]; snprintf(t, sizeof t, "%s%s %d%%", ups.empty() ? "Upgrading: " : "  ", UPGRADES[pl.faction][u].name, (int)(pl.upgProgress[u] * 100)); ups += t; }
                        if (!ups.empty() && !g_sim.revealed(g_sim.humanPlayer)) g.text(INFO_X + 86, hy + 98, ups.c_str(), accent);
                    }
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
                } else if (e->constructed && (bt.role == BR_BARRACKS || bt.role == BR_FACTORY || bt.role == BR_AIRFIELD || bt.role == BR_HQ || bt.role == BR_SUPPLY || bt.role == BR_NAVAL)) g.text(INFO_X + 330, hy + 58, "Production idle", hudDim());
                if (pl.lowPower() && bt.power < 0 && bt.role != BR_ANTINUKE) g.text(INFO_X + 86, hy + (bt.role == BR_TECH || bt.role == BR_NUKE || bt.role == BR_UNUKE || bt.role == BR_INCOME || bt.role == BR_NAVAL ? 110 : 98), "LOW POWER: reduced output", rgb(255, 140, 120));
                if (bt.role == BR_INCOME && e->constructed) {
                    bool half = pl.lowPower() && bt.power < 0;
                    int amt = pl.faction == F_CYBER ? 75 : 60;
                    snprintf(buf, sizeof buf, "Income $%d every %ds%s   total earned $%d", amt, (int)INCOME_INTERVAL, half ? " (low power: half rate)" : "", pl.mined); g.text(INFO_X + 86, hy + 84, buf, rgb(240, 220, 130));
                    snprintf(buf, sizeof buf, "%d of %d income structures", g_sim.countRole(g_sim.humanPlayer, BR_INCOME, false), INCOME_MAX); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_CIVIC && e->constructed && civicKind(e->type) >= 0) {
                    const CivicType& cv = CIVICS[civicKind(e->type)];
                    float dw = std::max(0.0f, e->dozerAt - g_sim.time);
                    snprintf(buf, sizeof buf, "Pays $%d every %ds   %d townsfolk   town: %d of %d", cv.income, (int)CIVIC_INTERVAL, cv.people, g_sim.countRole(e->owner, BR_CIVIC, false), CIVIC_MAX); g.text(INFO_X + 86, hy + 84, buf, rgb(240, 220, 130));
                    snprintf(buf, sizeof buf, "Next free %s in %d:%02d   (no guns, no plating: guard it with defences)", e->dozerKind == 0 ? UNITS[firstUnitOf(bt.faction)].name : UNITS[bt.faction == F_CYBER ? U_C_CIVDOZER : U_K_CIVDOZER].name, (int)dw / 60, (int)dw % 60); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_UNUKE && e->constructed) {
                    float w = std::max(0.0f, e->actionTimer - g_sim.time);
                    if (w > 0) snprintf(buf, sizeof buf, "Silver warhead loading: %d:%02d   blast doors %d%%", (int)w / 60, (int)w % 60, (int)(100 * e->hatch / std::max(1.0f, e->hatchMax)));
                    else snprintf(buf, sizeof buf, "SILVER WARHEAD READY  (K to launch)   blast doors %d%%", (int)(100 * e->hatch / std::max(1.0f, e->hatchMax)));
                    g.text(INFO_X + 86, hy + 84, buf, w > 0 ? rgb(214, 218, 226) : rgb(255, 255, 255));
                    snprintf(buf, sizeof buf, "%d silo(s) ready of %d   only Anti-Nuke Lasers stop it", g_sim.deepNukesReady(g_sim.humanPlayer), g_sim.countRole(g_sim.humanPlayer, BR_UNUKE, true)); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_NUKE && e->constructed) {
                    float w = std::max(0.0f, e->actionTimer - g_sim.time);
                    if (w > 0) snprintf(buf, sizeof buf, "Warhead %s: %d:%02d", "loading", (int)w / 60, (int)w % 60);
                    else snprintf(buf, sizeof buf, "WARHEAD READY  (K to launch)");
                    g.text(INFO_X + 86, hy + 84, buf, w > 0 ? rgb(255, 200, 120) : rgb(255, 110, 90));
                    snprintf(buf, sizeof buf, "%d ramp(s) ready of %d", g_sim.nukesReady(g_sim.humanPlayer), g_sim.countRole(g_sim.humanPlayer, BR_NUKE, true)); g.text(INFO_X + 86, hy + 96, buf, hudDim());
                }
                if (bt.role == BR_AIRFIELD && e->constructed) {
                    int n = g_sim.padsUsed(*e), hn = 0; Ref self = g_sim.refOf(*e);
                    for (auto& u : g_sim.ents) if (u.alive && u.isUnit() && u.isAir() && u.ut().heli && u.home == self) hn++;
                    snprintf(buf, sizeof buf, "Planes %d / %d%s   Helicopters %d (no limit)", n, AIRFIELD_CAP, n >= AIRFIELD_CAP ? "  (pads full: planes wait)" : "", hn); g.text(INFO_X + 86, hy + 84, buf, n >= AIRFIELD_CAP ? rgb(255, 200, 120) : hudDim());
                }
            }
        }
    } else if (selection.size() > 1) {
        snprintf(buf, sizeof buf, "%d units selected", (int)selection.size());
        g.text(INFO_X + 10, hy + 14, buf, rgb(255, 255, 255), 2);
        // sixteen cells to a row, two rows: the cells shrink to fit the panel (the command grid takes its right-hand side at 1024 px)
        const int pitch = std::min(32, (INFO_W - 10) / 16), cell = pitch - 2;
        int i = 0;
        for (auto r : selection) {
            Entity* e = g_sim.get(r);
            if (!e || !e->isUnit()) continue;
            if (i >= 32) break;
            int sx = INFO_X + 10 + (i % 16) * pitch, sy = hy + 40 + (i / 16) * 40;
            g.bevelPanel(sx, sy, cell, cell, shade(base, 0.55f), false);
            g.draw(g.unitBody[e->type][slotOf(e->owner)], sx + cell * 0.5f, sy + cell * 0.5f, 0, (0.85f / std::max(1.0f, artScale(e->type) * 0.95f)) * cell / 30.0f);   // (ships, jets and lifters are big sprites: they shrink to fit the cell)
            hpBar(g, sx, sy + cell + 2, cell, e->hp / e->maxHp);
            i++;
        }
    } else {
        g.text(INFO_X + 10, hy + 14, "No selection", hudDim(), 2);
        g.text(INFO_X + 10, hy + 40, "Left-click / drag: select    Right-click: move, attack, gather, repair   F / Ctrl+right-click: force fire on own / allies", hudDim());
        g.text(INFO_X + 10, hy + 52, "A: attack-move   S: stop   Tab: select army on screen   Ctrl+#: group", hudDim());
        g.text(INFO_X + 10, hy + 64, "Arrows/edge/middle-drag/wheel: scroll   Home: base   Space: pause   +/-: speed", hudDim());
        g.text(INFO_X + 10, hy + 76, "Select a Dozer to build structures; select a structure to train units", hudDim());
        g.text(INFO_X + 10, hy + 88, "Shift+drag while placing: a whole line   Del: sell   F2: mute   F1: help", hudDim());
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
        // the icon square at the left (a thumbnail for structure and unit buttons); the text starts to its right
        const int is = b.h - 4, tx = b.x + is + 5, bx = tx + 31;
        const float icx = b.x + 2 + is * 0.5f, icy = b.y + 2 + is * 0.5f;
        if (b.kind == BK_BUILD) iconDrawBuilding(g, b.id, icx, icy, is, b.enabled);
        else if (b.kind == BK_TRAIN) iconDrawUnit(g, b.id, slotOf(g_sim.humanPlayer), icx, icy, is, b.enabled);
        else iconDraw(g, buttonIcon(b.kind, b.id, b.label, pl.faction), icx, icy, is, b.enabled);
        Color tc = b.enabled ? rgb(255, 255, 255) : hudDim();
        const int ly = b.h >= BTN_H ? 4 : (b.h > BTN_H_DENSE ? 3 : 1), sy = b.h >= BTN_H ? 15 : (b.h > BTN_H_DENSE ? 12 : 10);   // label and second-line offsets (a compact button is tighter, a dense one tighter still)
        if (g.textW(b.label) > b.x + b.w - 3 - tx) {   // (a narrow four-column button: the label is cut to fit, the tooltip has the full name)
            std::string cut = b.label;
            while (cut.size() > 2 && g.textW((cut + ".").c_str()) > b.x + b.w - 3 - tx) cut.pop_back();
            g.text(tx, b.y + ly, (cut + ".").c_str(), tc);
        } else g.text(tx, b.y + ly, b.label, tc);
        int cost = b.kind == BK_BUILD ? BUILDS[b.id].cost : (b.kind == BK_TRAIN ? UNITS[b.id].cost : -1);
        if (cost >= 0) { snprintf(buf, sizeof buf, "$%d", cost); g.text(tx, b.y + sy, buf, pl.money >= cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        if (b.kind == BK_POWER || b.kind == BK_SCAN || b.kind == BK_DROP || b.kind == BK_AID || b.kind == BK_AIRSTRIKE || b.kind == BK_FLEET || b.kind == BK_NUKEBOMBER || b.kind == BK_ISLANDNUKE) {
            float rem = b.kind == BK_DROP ? g_sim.dropWait(g_sim.humanPlayer) : b.kind == BK_AID ? g_sim.aidWait(g_sim.humanPlayer) : b.kind == BK_AIRSTRIKE ? g_sim.strikeWait(g_sim.humanPlayer) : b.kind == BK_FLEET ? g_sim.fleetWait(g_sim.humanPlayer)
                      : b.kind == BK_NUKEBOMBER ? g_sim.bomberWait(g_sim.humanPlayer) : b.kind == BK_ISLANDNUKE ? g_sim.islandNukeWait(g_sim.humanPlayer) : (b.kind == BK_POWER ? pl.powerReady : pl.scanReady) - g_sim.time;
            if (rem > 0) { snprintf(buf, sizeof buf, "%d:%02d", (int)rem / 60, (int)rem % 60); g.text(tx, b.y + sy, buf, hudDim()); }
            else if (b.enabled) {
                int dn = b.kind == BK_DROP ? g_sim.dropsReady(g_sim.humanPlayer) : b.kind == BK_AID ? g_sim.aidsReady(g_sim.humanPlayer) : b.kind == BK_AIRSTRIKE ? g_sim.strikesReady(g_sim.humanPlayer) : b.kind == BK_FLEET ? g_sim.fleetsReady(g_sim.humanPlayer) : 1;
                if (dn > 1) { snprintf(buf, sizeof buf, "READY x%d", dn); g.text(tx, b.y + sy, buf, accent); } else g.text(tx, b.y + sy, "READY", accent);
            }
        }
        if (b.kind == BK_RESEARCH) {
            const ProgramType& pg = PROGRAMS[pl.faction];
            if (pl.advTech) g.text(tx, b.y + sy, "DONE", accent);
            else if (pl.researching) { snprintf(buf, sizeof buf, "%d%%", (int)(pl.researchProgress * 100)); g.text(tx, b.y + sy, buf, accent); g.fill(bx, b.y + sy + 3, (int)((b.x + b.w - 6 - bx) * clampf(pl.researchProgress, 0, 1)), 3, accent); }
            else { snprintf(buf, sizeof buf, "$%d", pg.cost); g.text(tx, b.y + sy, buf, pl.money >= pg.cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        }
        if (b.kind == BK_UPGRADE) {
            const UpgradeType& ug = UPGRADES[pl.faction][b.id];
            float pr = pl.upgProgress[b.id];
            if (pl.upg[b.id]) g.text(tx, b.y + sy, "DONE", accent);
            else if (pl.upgBusy[b.id]) { snprintf(buf, sizeof buf, "%d%%", (int)(pr * 100)); g.text(tx, b.y + sy, buf, accent); g.fill(bx, b.y + sy + 3, (int)((b.x + b.w - 16 - bx) * clampf(pr, 0, 1)), 3, accent); }
            else { snprintf(buf, sizeof buf, "$%d", ug.cost); g.text(tx, b.y + sy, buf, pl.money >= ug.cost ? rgb(240, 220, 130) : rgb(255, 120, 100)); }
        }
        const char* hk = kindHotkey(b.kind, b.id);
        if (hk) g.text(b.x + b.w - 12, b.y + sy, hk, accent);
        if (b.kind == BK_BUILD && placingType == b.id) g.box(b.x, b.y, b.w, b.h, accent);
    }
    if (hoverButton >= 0) {
        std::vector<std::string> tl = wrapTip(g, buttons[hoverButton].tip, SCREEN_W - 24);
        if (tl.size() > 3) { tl.resize(3); tl[2] += " ..."; }
        int tw = 0; for (auto& l : tl) tw = std::max(tw, g.textW(l.c_str()));
        int th = (int)tl.size() * 16;
        int tx = std::min(SCREEN_W - tw - 12, std::max(4, mouseX - (tw + 12) / 2));
        int ty = VIEW_H - 6 - th;
        g.fill(tx, ty, tw + 12, th, rgb(8, 10, 14, 230));
        for (size_t i = 0; i < tl.size(); i++) g.text(tx + 6, ty + 4 + (int)i * 16, tl[i].c_str(), rgb(255, 255, 255));
    }
    if (showHelp) {
        const char* lines[] = {
            "Left click / drag box      select units (double-click: all of that type on screen)",
            "Right click                move / attack / gather / repair / rally (also on the minimap)",
            "A + click                  attack-move       S: stop        Tab: army on screen",
            "G, then click or drag      guard an area (fighters, drones) / gather in an area (haulers)",
            "Click a turret or battery  shows the area it covers (hover any defense to see its reach)",
            "Ctrl + 0-9 / 0-9           assign / recall control group (Alt+#: jump to it)",
            "Arrows, screen edge, middle-drag, wheel: scroll     Home: your base",
            "Dozer selected             build menu; click the ground to place, Shift+drag for a whole line",
            "Structure selected         train units; right-click ground for rally; Del sells",
            "F + click / Ctrl+right-click  force fire on your own or an ally's unit or structure (they never shoot back)",
            "F or Ctrl while aiming     a strike power hits friendly ground too (a nuke always does)",
            "Tech structure            X strike, V map scan (30s), R Advanced Program, P airdrop, I Air Strike, L Fleet Call, B Nuclear Bomber, D aid drop, U/G/E/N/H/A upgrades",
            "Wall (pasture/defense)    W: unlimited thick wall segments; H: Hyper Protection, A: Anti-Rocket wall lasers (rockets, nukes, aircraft)",
            "Spy selected              right-click an enemy structure, an island launcher or a Bastion Wall to take it (swims across water)",
            "Soldiers + bunker / lifter right-click your bunker (50 + a dozer) or cargo lifter (40 places) to board",
            "Bunker / lifter selected  U unloads; D (lifter) flies to a spot you click and drops everybody off",
            "Stealth                    snipers show only to an enemy spy drone; underground bunkers are plainly visible",
            "Lake                       Y: Naval Yard on the shore (ships sail on water only); Q: anti-nuke defence (laser 18%, missiles 30%; overlapping cover helps)",
            "Jeep                       right-click it with soldiers: seven seats, they shoot from inside; drops carry loaded jeeps; N: Guard Drones upgrade",
            "Space pause    + / - speed    F2 mute    F11 fullscreen    F12 screenshot    Esc menu",
            "",
            "Supplies: haulers carry $300 per trip to a Supply Hub/Depot.",
            "Power: keep production above consumption or turrets and factories slow down.",
        };
        int helpW = std::min(1000, SCREEN_W - 32);
        std::vector<std::string> wrapped;
        for (const char* line : lines) {
            auto row = wrapTip(g, line, helpW - 32);
            if (row.empty()) wrapped.emplace_back();
            else wrapped.insert(wrapped.end(), row.begin(), row.end());
        }
        int lineH = std::min(16, (VIEW_H - 72) / std::max(1, (int)wrapped.size()));
        int helpH = 52 + (int)wrapped.size() * lineH;
        int hx = (SCREEN_W - helpW) / 2, hy0 = std::max(12, (VIEW_H - helpH) / 2);
        g.fill(hx, hy0, helpW, helpH, rgb(8, 10, 14, 235));
        g.box(hx, hy0, helpW, helpH, accent);
        g.text(hx + 16, hy0 + 12, "ONE HOUR - controls", rgb(255, 255, 255), 2);
        for (size_t i = 0; i < wrapped.size(); i++) g.text(hx + 16, hy0 + 40 + (int)i * lineH, wrapped[i].c_str(), hudText());
    }
}

// ------------------------------------------------------------ nukes and pause menu
// The paradrop cargo plane: a big transport crossing the map, high above everything else, with the drop zone marked on the ground for as long as it is still releasing
void Game::drawAirlifts() {
    Gfx& g = g_gfx;
    for (auto& a : g_sim.airlifts) {
        Faction fac = g_sim.players[a.owner].faction;
        Color tc = playerColor(a.owner);
        if (a.next < a.load.size()) {
            Vec2 tgt = worldToScreen(a.target);
            float R = DROPS[fac].radius * TILE, pulse = 0.5f + 0.5f * std::sin(wallTime * 7.0f);
            g.discFill(tgt.x, tgt.y, R, rgb(tc.r, tc.g, tc.b, (int)(14 + 18 * pulse)));
            g.dashedCircle(tgt.x, tgt.y, R, tc, wallTime * 24.0f);
            const char* lb = a.owner == g_sim.humanPlayer || !g_sim.enemies(g_sim.humanPlayer, a.owner) ? "DROP ZONE" : "ENEMY DROP";
            g.text((int)(tgt.x - g.textW(lb) / 2), (int)(tgt.y - 4), lb, rgb(255, 235, 210));
        }
        Vec2 w = a.prevPos + (a.pos - a.prevPos) * renderAlpha;
        if (a.owner != g_sim.humanPlayer && !g_sim.seenNow(clampi(tileOf(w.x), 0, MAP_W - 1), clampi(tileOf(w.y), 0, MAP_H - 1))) continue;
        Vec2 p = worldToScreen(w);
        if (p.x < -140 || p.y < -160 || p.x > SCREEN_W + 140 || p.y > VIEW_H + 160) continue;
        float ang = std::atan2(a.dir.y, a.dir.x);
        const Sprite& spr = g.cargoPlane[slotOf(a.owner)][fac];
        const float SC = 1.9f, ALT = 30.0f;
        g.draw(spr, p.x + 26, p.y + 44, ang, SC, rgb(0, 0, 0), 70);            // shadow thrown far to the south-east: the plane is high
        Vec2 perp(-a.dir.y, a.dir.x);
        if (fac == F_CYBER) for (int k = 0; k < 4; k++) {
            float off = (k < 2 ? 11.0f : 21.0f) * (k & 1 ? 1 : -1) * UNIT_SCALE * SC;
            Vec2 tail = Vec2(p.x, p.y - ALT) - a.dir * (19.0f * UNIT_SCALE * SC) + perp * off;
            g.glowAdd(tail.x, tail.y, 7, Color{110, 230, 255, 150});
        }
        g.draw(spr, p.x, p.y - ALT, ang, SC);
        if (fac == F_CLANKER && !paused) for (int k = 0; k < 4; k++) {   // propeller discs flickering at the engine fronts
            float off = (k < 2 ? 11.0f : 21.0f) * (k & 1 ? 1 : -1) * UNIT_SCALE * SC;
            Vec2 nose = Vec2(p.x, p.y - ALT) + a.dir * ((k < 2 ? 12.0f : 8.0f) * UNIT_SCALE * SC) + perp * off;
            g.drawSized(g.fxs.ring, nose.x, nose.y, 11, 11, wallTime * 30.0f + k, rgb(230, 232, 236), 70);
        }
        if (frameDt > 0 && !paused) {
            for (int k = 0; k < 2; k++) if (fxRng.f() < 22.0f * frameDt) {   // contrails off the outer engines
                float off = (k ? 21.0f : -21.0f) * UNIT_SCALE * SC;
                Vec2 tail = w - a.dir * 26.0f + perp * off;
                emitP(tail.x, tail.y - ALT, 0, 0, fxRng.f(1.2f, 1.9f), 2.6f, 8.0f, rgb(238, 244, 248, 130), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), 0.1f);
            }
            if (a.hp < a.maxHp * 0.5f && fxRng.f() < 18.0f * frameDt)
                emitP(w.x + fxRng.f(-6, 6), w.y - ALT + fxRng.f(-4, 4), fxRng.f(-8, 8), fxRng.f(-20, -8), fxRng.f(1.0f, 1.6f), 4.0f, 11.0f, rgb(40, 38, 36, 220), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.8f, fxRng.f(0, 6), 0.3f);
        }
        if (a.hp < a.maxHp) hpBar(g, p.x - 20, p.y - ALT - 42, 40, clampf(a.hp / a.maxHp, 0, 1));
    }
}

// Your Aid Drop flights: the white relief plane high over everything, the crates swinging down under their canopies, and once they are
// down the opened crates (they fade after a while) with a label saying who got the aid. Always drawn: it is your own flight.
void Game::drawAidDrops() {
    Gfx& g = g_gfx;
    const Color ac = rgb(150, 210, 250);
    const float ALT = 34.0f;   // the plane's height on screen (its shadow is thrown to the south-east)
    for (auto& a : g_sim.aidDrops) {
        if (!a.delivered && !(a.downed && a.crates.empty())) {   // the drop zone, until the last crate is down
            Vec2 tgt = worldToScreen(a.target);
            float R = AID_RADIUS * TILE, pulse = 0.5f + 0.5f * std::sin(wallTime * 6.0f);
            g.discFill(tgt.x, tgt.y, R, rgb(ac.r, ac.g, ac.b, (int)(12 + 16 * pulse)));
            g.dashedCircle(tgt.x, tgt.y, R, ac, wallTime * 24.0f);
            g.text((int)(tgt.x - g.textW("AID DROP") / 2), (int)(tgt.y - R - 12), "AID DROP", rgb(235, 245, 255));
        }
        float fade = a.delivered ? clampf((Sim::AID_LINGER - (g_sim.time - a.deliveredAt)) / 3.0f, 0, 1) : 1.0f;
        for (int pass = 0; pass < 3; pass++) for (size_t i = 0; i < a.crates.size(); i++) {   // shadows, then the crates, then the canopies high above them
            const Sim::AidCrate& c = a.crates[i];
            float k = c.fall > 0 ? clampf((c.fall - renderAlpha * SIM_DT) / c.fallTime, 0, 1) : 0.0f;   // 1 just released .. 0 on the ground
            if (pass == 2 && k <= 0) continue;
            float ease = k * k * (3 - 2 * k);
            Vec2 gp = c.to + (c.from - c.to) * ease;                       // ground point under the crate
            Vec2 sp = worldToScreen(gp);
            if (sp.x < -80 || sp.y < -120 || sp.x > SCREEN_W + 80 || sp.y > VIEW_H + 80) continue;
            float h = ALT * k;                                              // height above the ground on screen
            float sway = k > 0 ? std::sin(wallTime * 2.2f + (float)i * 1.7f) * 0.18f * k : 0.0f;
            float sc = 1.15f + 0.25f * k;                                   // nearer the camera while it is high
            if (pass == 0) g.drawSized(g.fxs.cloud[0], sp.x + 4 + h * 0.6f, sp.y + 3 + h * 0.5f, 26 - 8 * k, 14 - 5 * k, 0, rgb(0, 0, 0), (u8)((90 - 50 * k) * fade));
            else if (pass == 1) g.draw(a.delivered ? g.crateOpen : g.crate, sp.x, sp.y - h, sway + (float)i * 0.35f, sc, rgb(255, 255, 255), (u8)(255 * fade));
            else {
                float open = clampf((1 - k) / 0.12f, 0.25f, 1.0f), flare = clampf(k / 0.08f, 0, 1);   // the canopy blooms after the drop and collapses on touchdown
                Vec2 cp(sp.x - sway * 30.0f, sp.y - h - 22.0f * sc);
                g.line(sp.x - 7, sp.y - h - 6, cp.x - 12 * open, cp.y + 4, rgb(70, 66, 60, 200));
                g.line(sp.x + 7, sp.y - h - 6, cp.x + 12 * open, cp.y + 4, rgb(70, 66, 60, 200));
                g.draw(g.aidChute, cp.x, cp.y, sway * 0.5f, 1.45f * open * (0.6f + 0.4f * flare), rgb(255, 255, 255), (u8)(255 * flare));
            }
        }
        if (a.delivered && a.recipient >= 0 && g_sim.time - a.deliveredAt < 7.0f) {   // who got it
            Vec2 at = worldToScreen(a.crates.empty() ? a.target : a.crates[a.crates.size() / 2].to);
            float rise = (g_sim.time - a.deliveredAt) * 4.0f;
            char buf[96]; snprintf(buf, sizeof buf, "+$%d & Dozer: %s", AID_MONEY, armyName(a.recipient).c_str());
            int tx = (int)(at.x - g.textW(buf) / 2), ty = (int)(at.y - 34 - rise);
            g.fill(tx - 4, ty - 3, g.textW(buf) + 8, 14, rgb(8, 10, 14, 170));
            g.text(tx, ty, buf, playerColor(a.recipient));
        }
        // the plane
        if (a.downed) continue;
        Vec2 w = a.prevPos + (a.pos - a.prevPos) * renderAlpha;
        Vec2 p = worldToScreen(w);
        if (p.x < -160 || p.y < -180 || p.x > SCREEN_W + 160 || p.y > VIEW_H + 180) continue;
        float ang = std::atan2(a.dir.y, a.dir.x);
        const float SC = 2.7f;
        g.draw(g.aidPlane, p.x + 30, p.y + 50, ang, SC, rgb(0, 0, 0), 70);
        g.draw(g.aidPlane, p.x, p.y - ALT, ang, SC);
        Vec2 perp(-a.dir.y, a.dir.x);
        if (!paused) for (int k = 0; k < 4; k++) {   // propeller discs
            float off = (k < 2 ? 11.0f : 20.5f) * (k & 1 ? 1 : -1) * UNIT_SCALE * SC;
            Vec2 nose = Vec2(p.x, p.y - ALT) + a.dir * ((k < 2 ? 12.6f : 8.6f) * UNIT_SCALE * SC) + perp * off;
            g.drawSized(g.fxs.ring, nose.x, nose.y, 12, 12, wallTime * 30.0f + k, rgb(236, 238, 242), 70);
        }
        if (frameDt > 0 && !paused) for (int k = 0; k < 2; k++) if (fxRng.f() < 22.0f * frameDt) {   // contrails off the outer engines
            float off = (k ? 20.5f : -20.5f) * UNIT_SCALE * SC;
            Vec2 tail = w - a.dir * 30.0f + perp * off;
            emitP(tail.x, tail.y - ALT, 0, 0, fxRng.f(1.2f, 1.9f), 2.6f, 8.0f, rgb(240, 246, 250, 130), PK_CONTRAIL, (u8)fxRng.range(0, 3), 0, 0.3f, fxRng.f(0, 6), 0.1f);
        }
    }
}

void Game::drawNukes() {
    Gfx& g = g_gfx;
    for (auto& n : g_sim.nukes) {
        float k = clampf(n.t / Sim::flightOf(n), 0, 1);
        Vec2 tgt = worldToScreen(n.pos);
        // warning zone
        float pulse = 0.5f + 0.5f * std::sin(wallTime * (n.super ? 16.0f : 9.0f));
        float nr = Sim::nukeRadius(n);
        const bool sv = n.deep;   // an Underground Nuke: everything about it is silver
        const bool su = n.super;  // an island Super Nuke: magenta-white, and the whole of its huge circle flashes
        g.discFill(tgt.x, tgt.y, nr, su ? rgb(255, 70, 190, (int)(16 + 26 * pulse)) : sv ? rgb(196, 202, 212, (int)(18 + 22 * pulse)) : rgb(255, 60, 40, (int)(20 + 26 * pulse)));
        g.dashedCircle(tgt.x, tgt.y, nr, su ? rgb(255, 120, 210) : sv ? rgb(220, 224, 232) : rgb(255, 90, 60), wallTime * 30.0f);
        if (su) g.dashedCircle(tgt.x, tgt.y, nr * 0.35f, rgb(255, 200, 240), -wallTime * 40.0f);   // (its lethal core)
        if (su && !n.broken && dist(Vec2(SCREEN_W * 0.5f, VIEW_H * 0.5f), tgt) < nr) {   // the view is inside the huge circle (its rim is off screen): say so plainly
            Color edge = rgb(255, 70, 190, (int)(70 + 90 * pulse));
            g.fill(0, 0, SCREEN_W, 4, edge); g.fill(0, VIEW_H - 4, SCREEN_W, 4, edge); g.fill(0, 0, 4, VIEW_H, edge); g.fill(SCREEN_W - 4, 0, 4, VIEW_H, edge);
            char warn[64]; snprintf(warn, sizeof warn, "INSIDE THE SUPER NUKE'S BLAST  %ds", (int)std::ceil(Sim::flightOf(n) - n.t));
            g.text((int)(SCREEN_W * 0.5f - g.textW(warn, 2) * 0.5f), 34, warn, rgb(255, 214, 240), 2);
        }
        char buf[48]; snprintf(buf, sizeof buf, "%s %ds", n.broken ? "SHOT DOWN: PIECES" : (su ? "SUPER NUKE" : sv ? "UNDERGROUND NUKE" : n.rain ? "RAIN NUKE" : (n.scale < 1.0f ? "MINI NUKE" : "NUKE")), (int)std::ceil(Sim::flightOf(n) - n.t));
        g.text((int)(tgt.x - g.textW(buf) / 2), (int)(tgt.y - 6), buf, n.broken ? rgb(255, 214, 140) : rgb(255, 230, 200));
        if (n.broken) continue;   // (the missile is gone: its pieces are drawn with the anti-nuke shots)
        // the missile: rises from the ramp on a long arc and falls on the target
        auto world = [&](float u) { Vec2 p = n.from + (n.pos - n.from) * u; p.y -= std::sin(u * 3.14159f) * 260.0f; return p; };
        auto ground = [&](float u) { return n.from + (n.pos - n.from) * u; };
        Vec2 w0 = world(k), w1 = world(std::min(1.0f, k + 0.012f));
        Vec2 m = worldToScreen(w0), dir = (w1 - w0).norm();
        float ang = std::atan2(dir.y, dir.x);
        const float S = su ? 2.0f : sv ? 1.7f : 1.3f;
        bool cyberArmy = g_sim.players[n.owner < 0 ? 0 : n.owner].faction == F_CYBER;
        const Sprite& body = cyberArmy ? g.fxs.missile : g.fxs.missileK;
        const Color hot = su ? rgb(255, 236, 250) : sv ? rgb(236, 240, 248) : rgb(255, 244, 214), warm = su ? rgb(255, 120, 210) : sv ? rgb(196, 204, 218) : rgb(255, 190, 120);
        const Color glowA = su ? Color{255, 90, 200, 170} : sv ? Color{200, 210, 228, 150} : Color{255, 150, 60, 150};
        // its shadow slides across the ground below it, smaller and fainter the higher it climbs
        { Vec2 gs = worldToScreen(ground(k)); float lift = std::sin(k * 3.14159f); float sz = 34.0f - 12.0f * lift;
          g.drawSized(g.fxs.cloud[0], gs.x + 6, gs.y + 4, sz * 2.2f, sz, ang, rgb(0, 0, 0), (u8)(120 - 60 * lift)); }
        // exhaust: a long white-hot core fading through orange, with a hard glow at the nozzle
        int fr = ((int)(wallTime * 26)) % 6;
        Vec2 tail = m - dir * (27.0f * S);
        for (int j = 2; j >= 0; j--) {
            Vec2 fp = tail - dir * (j * 15.0f * S);
            g.draw(g.fxs.flame[(fr + j * 2) % 6], fp.x, fp.y, ang - 1.5708f, (1.9f - 0.35f * j) * S, j == 0 ? hot : warm, (u8)(245 - 55 * j));
        }
        g.glowAdd(tail.x, tail.y, 52 * S, glowA);
        g.glowAdd(tail.x - dir.x * 6, tail.y - dir.y * 6, 22 * S, Color{hot.r, hot.g, hot.b, 210});
        g.draw(body, m.x, m.y, ang, S, su ? rgb(255, 196, 236) : sv ? UNUKE_GLOW : rgb(255, 255, 255));
        if (sv) g.glowAdd(m.x, m.y, 30 * S, Color{214, 220, 232, 70});   // the polished casing catches the light
        if (su) g.glowAdd(m.x, m.y, 40 * S, Color{255, 110, 210, (u8)(90 + 60 * pulse)});   // the Super Nuke's casing burns magenta all the way
        if (k > 0.5f) {   // re-entry: the nose cone glows as the warhead dives
            float heat = clampf((k - 0.5f) / 0.4f, 0, 1);
            Vec2 nose = m + dir * (29.0f * S);
            g.glowAdd(nose.x, nose.y, 26 * S, Color{255, 140, 70, (u8)(140 * heat)});
            g.glowAdd(nose.x, nose.y, 11 * S, Color{255, 236, 200, (u8)(190 * heat)});
        }
        if (frameDt > 0 && onScreen(w0.x, w0.y, 260)) {
            // a thick billowing trail that hangs in the air for a few seconds, plus embers
            float rate = 95.0f;
            for (float q = fxRng.f(); q < rate * frameDt; q += 1.0f) {
                Vec2 back = w0 - dir * (30.0f + fxRng.f(0, 10));
                emitP(back.x, back.y, fxRng.f(-12, 12), fxRng.f(-12, 12), fxRng.f(2.8f, 4.6f), 6, fxRng.f(24, 36), rgb(236, 232, 226, 200), PK_SMOKE, (u8)fxRng.range(0, 3), 0, 0.7f, fxRng.f(0, 6), fxRng.f(-0.4f, 0.4f));
            }
            if (fxRng.f() < 40.0f * frameDt) emitP(w0.x - dir.x * 34, w0.y - dir.y * 34, -dir.x * fxRng.f(40, 120) + fxRng.f(-40, 40), -dir.y * fxRng.f(40, 120) + fxRng.f(-40, 40), fxRng.f(0.3f, 0.7f), 1.8f, 0.4f, rgb(255, 214, 140), PK_SPARK, 0, 120, 2.0f);
        }
        if (k < 0.3f && frameDt > 0 && onScreen(n.from.x, n.from.y, 200)) {   // launch plume at the ramp
            float inten = 1.0f - k / 0.3f;
            for (float q = fxRng.f(); q < 70.0f * inten * frameDt; q += 1.0f) {
                float an = fxRng.f(0, 6.283f), sp = fxRng.f(30, 100);
                emitP(n.from.x, n.from.y, std::cos(an) * sp, std::sin(an) * sp * 0.5f - fxRng.f(10, 40), fxRng.f(1.4f, 2.6f), 14, fxRng.f(44, 70), rgb(190, 178, 164, 190), PK_DUST, (u8)fxRng.range(0, 2), 0, 1.6f, fxRng.f(0, 6), fxRng.f(-0.4f, 0.4f));
            }
            Vec2 fs = worldToScreen(n.from);
            g.glowAdd(fs.x, fs.y, 64 * inten, Color{255, 170, 80, (u8)(150 * inten)});
        }
    }
    drawInterceptors();
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

// The pause menu's rows: the fixed ones, and after "Controls help" one row per computer army whose difficulty can be raised (never lowered) mid-match.
enum PmRow { PMR_RESUME = 0, PMR_SPEED, PMR_SOUND, PMR_HELP, PMR_ARMY, PMR_RESTART, PMR_SURRENDER, PMR_MENU, PMR_QUIT };
struct PmItem { PmRow kind; int player; };
static std::vector<PmItem> pmRows() {
    std::vector<PmItem> r = { {PMR_RESUME, -1}, {PMR_SPEED, -1}, {PMR_SOUND, -1}, {PMR_HELP, -1} };
    for (int p = 0; p < g_sim.numPlayers; p++) if (g_sim.players[p].isAI && g_sim.players[p].alive) r.push_back({PMR_ARMY, p});
    for (PmRow k : {PMR_RESTART, PMR_SURRENDER, PMR_MENU, PMR_QUIT}) r.push_back({k, -1});
    return r;
}
static const char* PM_LABEL[PMR_QUIT + 1] = { "Resume", "Game speed", "Sound", "Controls help", "", "Restart match", "Surrender (end game)", "Quit to main menu", "Quit to desktop" };
static int pmY0(int rows) { return std::max(70, SCREEN_H / 2 - 178 - std::max(0, rows - 8) * 17); }
static const int PM_W = 440, PM_RH = 34;

void Game::pauseMenuActivate(int row) {
    std::vector<PmItem> rows = pmRows();
    if (row < 0 || row >= (int)rows.size()) return;
    PmItem it = rows[row];
    // destructive rows ask twice, and so does raising an army's difficulty (it cannot be undone)
    bool confirm = it.kind >= PMR_RESTART || (it.kind == PMR_ARMY && g_sim.players[it.player].difficulty < 3);
    if (it.kind == PMR_ARMY && g_sim.players[it.player].difficulty >= 3) { g_audio.play(SND_CANT, Vec2(), true); return; }
    if (confirm && menuConfirm != row) { menuConfirm = row; g_audio.play(SND_CLICK, Vec2(), true); return; }
    menuConfirm = -1;
    switch (it.kind) {
    case PMR_RESUME: closePauseMenu(); break;
    case PMR_SPEED: stepSpeed(1); break;
    case PMR_SOUND: g_audio.setMuted(!g_audio.isMuted()); break;
    case PMR_HELP: closePauseMenu(); showHelp = true; break;
    case PMR_ARMY:
        if (g_ai.raiseDifficulty(it.player)) {
            char msg[128]; snprintf(msg, sizeof msg, "The %s now plays on %s", armyName(it.player).c_str(), DIFF_NAME[g_sim.players[it.player].difficulty]);
            addMessage(msg, rgb(255, 200, 120));
            g_audio.play(SND_BUILD_DONE, Vec2(), true);
        }
        break;
    case PMR_RESTART: closePauseMenu(); startGame(); break;
    case PMR_SURRENDER: {
        closePauseMenu(); paused = false;
        int me = g_sim.humanPlayer, winner = -1;
        for (int p = 0; p < g_sim.numPlayers; p++) if (g_sim.enemies(me, p) && g_sim.players[p].alive) { winner = g_sim.players[p].team; break; }   // an ally does not win the game for you
        g_sim.players[me].alive = false;
        g_sim.gameOver = true; g_sim.winnerTeam = winner;
        break;
    }
    case PMR_MENU: closePauseMenu(); state = GS_MENU; break;
    case PMR_QUIT: quitRequested = true; break;
    }
}

void Game::pauseMenuEvent(const SDL_Event& e) {
    std::vector<PmItem> rows = pmRows();
    const int N = (int)rows.size();
    int x0 = SCREEN_W / 2 - PM_W / 2, y0 = pmY0(N);
    if (menuCursor >= N) menuCursor = 0;
    if (e.type == SDL_KEYDOWN) {
        SDL_Keycode k = e.key.keysym.sym;
        PmRow kind = rows[menuCursor].kind;
        if (k == SDLK_ESCAPE) { if (menuConfirm >= 0) menuConfirm = -1; else closePauseMenu(); return; }
        if (k == SDLK_UP) { menuCursor = (menuCursor + N - 1) % N; menuConfirm = -1; }
        else if (k == SDLK_DOWN) { menuCursor = (menuCursor + 1) % N; menuConfirm = -1; }
        else if (k == SDLK_LEFT) { if (kind == PMR_SPEED) stepSpeed(-1); else if (kind == PMR_SOUND) g_audio.setMuted(!g_audio.isMuted()); else if (kind == PMR_ARMY) g_audio.play(SND_CANT, Vec2(), true); }   // (a difficulty never goes down)
        else if (k == SDLK_RIGHT) { if (kind == PMR_SPEED) stepSpeed(1); else if (kind == PMR_SOUND) g_audio.setMuted(!g_audio.isMuted()); else if (kind == PMR_ARMY) pauseMenuActivate(menuCursor); }
        else if (k == SDLK_MINUS || k == SDLK_KP_MINUS) stepSpeed(-1);
        else if (k == SDLK_EQUALS || k == SDLK_PLUS || k == SDLK_KP_PLUS) stepSpeed(1);
        else if (k == SDLK_RETURN || k == SDLK_KP_ENTER || k == SDLK_SPACE) {
            pauseMenuActivate(menuCursor);
        }
        return;
    }
    if (e.type == SDL_MOUSEMOTION) {
        int row = (mouseY - y0) / PM_RH;
        if (mouseX >= x0 && mouseX < x0 + PM_W && mouseY >= y0 && row >= 0 && row < N && row != menuCursor) { menuCursor = row; menuConfirm = -1; }
    }
    if (e.type == SDL_MOUSEBUTTONDOWN && e.button.button == SDL_BUTTON_LEFT) {
        int row = (mouseY - y0) / PM_RH;
        if (mouseX < x0 || mouseX >= x0 + PM_W || mouseY < y0 || row < 0 || row >= N) return;
        menuCursor = row;
        if (rows[row].kind == PMR_SPEED) {
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
    std::vector<PmItem> rows = pmRows();
    const int N = (int)rows.size();
    g.fill(0, 0, SCREEN_W, SCREEN_H, rgb(6, 8, 12, 165));
    int x0 = SCREEN_W / 2 - PM_W / 2, y0 = pmY0(N);
    g.bevelPanel(x0 - 16, y0 - 62, PM_W + 32, N * PM_RH + 112, shade(base, 0.7f), true);
    g.fill(x0 - 16, y0 - 62, PM_W + 32, 3, accent);
    g.text(SCREEN_W / 2 - g.textW("GAME PAUSED", 3) / 2, y0 - 46, "GAME PAUSED", rgb(255, 255, 255), 3);
    int secs = (int)g_sim.time; char buf[160];
    snprintf(buf, sizeof buf, "%s   %02d:%02d   $%d", FACTION_NAME[pl.faction], secs / 60, secs % 60, pl.money);
    g.text(SCREEN_W / 2 - g.textW(buf) / 2, y0 - 16, buf, hudDim());
    for (int i = 0; i < N; i++) {
        int y = y0 + i * PM_RH;
        PmRow kind = rows[i].kind;
        bool cur = i == menuCursor, conf = menuConfirm == i;
        Color rowc = conf ? (kind == PMR_ARMY ? rgb(150, 100, 30) : rgb(150, 50, 40)) : (cur ? shade(base, 1.6f) : shade(base, 1.05f));
        g.bevelPanel(x0, y, PM_W, PM_RH - 4, rowc, true);
        std::string lab = PM_LABEL[kind];
        if (kind == PMR_ARMY) {   // an army: its colour flag, name and commander; its difficulty with a [+] while it can still go up
            int p = rows[i].player, d = g_sim.players[p].difficulty;
            const AiPlayer& a = g_ai.ais[p];
            g.drawFlag((float)x0 + 14, (float)y + 26, p, 0.62f, wallTime * 3.0f);
            lab = std::string(g_sim.enemies(g_sim.humanPlayer, p) ? "Enemy: " : "Ally: ") + armyName(p) + (a.commander && *a.commander ? std::string(", ") + a.commander : std::string());
            if (conf) { snprintf(buf, sizeof buf, "Raise to %s? (cannot be lowered) Enter = yes", DIFF_NAME[std::min(3, d + 1)]); lab = buf; }
            g.text(x0 + 38, y + 9, lab.c_str(), cur ? rgb(255, 255, 255) : hudText(), 1);
            if (!conf) {
                snprintf(buf, sizeof buf, d < 3 ? "%s  [+]" : "%s (max)", DIFF_NAME[d]);
                Color dc = d >= 3 ? rgb(255, 120, 90) : (d == 2 ? rgb(255, 190, 90) : (d == 1 ? accent : rgb(150, 220, 150)));
                g.text(x0 + PM_W - 12 - g.textW(buf), y + 9, buf, dc);
            }
            continue;
        }
        if (conf) lab = kind == PMR_SURRENDER ? "Surrender? Enter = yes" : (kind == PMR_RESTART ? "Restart? Enter = yes" : (kind == PMR_MENU ? "Leave the match? Enter = yes" : "Exit game? Enter = yes"));
        g.text(x0 + 12, y + 9, lab.c_str(), cur ? rgb(255, 255, 255) : hudText(), 1);
        if (kind == PMR_RESUME) g.text(x0 + PM_W - 12 - g.textW("Esc"), y + 9, "Esc", hudDim());
        if (kind == PMR_SOUND) g.text(x0 + PM_W - 12 - g.textW(g_audio.isMuted() ? "Off" : "On"), y + 9, g_audio.isMuted() ? "Off" : "On", g_audio.isMuted() ? rgb(255, 150, 120) : accent);
        if (kind == PMR_HELP) g.text(x0 + PM_W - 12 - g.textW("F1"), y + 9, "F1", hudDim());
        if (kind == PMR_SPEED) {
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
    const char* help = "Up/Down choose   Left/Right change   Enter select   Esc resume   (army difficulty only goes up)";
    g.text(SCREEN_W / 2 - g.textW(help) / 2, y0 + N * PM_RH + 6, help, hudDim());
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
