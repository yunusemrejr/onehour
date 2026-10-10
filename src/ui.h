// One Hour - menu, HUD, input and world rendering
#pragma once
#include "sim.h"
#include "gfx.h"
#include "audio.h"
#include "ai.h"

enum GameState { GS_MENU = 0, GS_PLAYING, GS_GAMEOVER };

struct MenuSettings {
    Faction playerFaction = F_CYBER;
    int enemyFaction[3] = { 2, 2, 3 };   // per enemy army: 0 cyber, 1 clanker, 2 random, 3 off (the first enemy is always on)
    int enemyDiff[3] = { 1, 1, 1 };      // per enemy army: 0 easy, 1 normal, 2 hard, 3 brutal
    int allyFaction = 3;                 // your computer-controlled ally: 0 cyber, 1 clanker, 2 random, 3 none
    int allyDiff = 1;
    bool enemiesAllied = true;           // with two or more enemies: they fight together against your team, or each against everyone
    int cursor = 0;                      // row of the setup table
    int col = 0;                         // focused control within the row: 0 army, 1 difficulty
    bool hasAlly() const { return allyFaction != 3; }
    int enemyCount() const { int n = 0; for (int i = 0; i < 3; i++) if (enemyFaction[i] != 3) n++; return n; }
};

struct Message { std::string text; float time; Color color; };

// Cosmetic, render-side effects: they never touch the simulation, pause with the game and are capped, so they stay cheap.
enum PKind : u8 { PK_SMOKE = 0, PK_DUST, PK_DEBRIS, PK_CONTRAIL, PK_SPARK, PK_FLAME, PK_RING, PK_GLOW };
struct Particle { float x, y, vx, vy, ay, drag, age, life, s0, s1, rot, vrot; Color col; u8 kind, var; };
struct Decal { float x, y, r, rot, age, life; u8 kind; };       // ground scorch marks
struct Tread { float x, y, rot, age, life; u8 kind; };          // a short dark track-pressed dash left by a vehicle on soft ground

struct Game {
    GameState state = GS_MENU;
    MenuSettings menu;
    Vec2 cam;                       // top-left world px
    std::vector<Ref> selection;
    std::vector<Ref> groups[10];
    bool dragging = false; Vec2 dragStart, dragNow;
    bool midDrag = false; Vec2 midStart;
    int placingType = -1;           // build type being placed
    bool attackMoveMode = false;
    bool forceMode = false;         // Force Fire button: the next click attacks whatever is under it, your own or an ally's units and structures included
    bool forceLatch = false;        // F while aiming a strike power: the blast hits friendly ground too (same as holding Ctrl)
    bool powerMode = false;
    bool nukeMode = false;          // picking a nuke target
    bool dropMode = false;          // picking a paradrop zone
    bool aidMode = false;           // picking the spot for an Aid Drop
    bool rallyMode = false;
    bool areaMode = false;          // picking the circle a selection should guard / gather in
    bool areaDrag = false; Vec2 areaStart; float areaRadius = 0;
    struct ZoneFlash { Vec2 pos; float r; float time; bool gather; };
    std::vector<ZoneFlash> zoneFlashes;
    int mouseX = 0, mouseY = 0;
    bool mouseInWindow = true;
    float lastClickTime = -1; Ref lastClickEnt;
    std::vector<Message> messages;
    float speed = 1.0f;
    bool paused = false;
    bool showHelp = false;
    float renderAlpha = 0;
    float wallTime = 0;
    float gameOverAt = 0;
    int hoverButton = -1;
    u64 seed = 1;
    bool quitRequested = false;
    // pause menu (Esc): game speed, sound, help, restart, surrender, quit
    bool menuOpen = false;
    int menuCursor = 0;
    int menuConfirm = -1;           // row waiting for a second confirmation, -1 none
    bool pausedBeforeMenu = false;
    void openPauseMenu();
    void closePauseMenu();
    void stepSpeed(int dir);        // move to the next/previous entry of SPEED_STEPS
    static const int SPEED_COUNT = 7;
    struct Ping { Vec2 pos; float time; };
    std::vector<Ping> pings;
    std::vector<Particle> parts;
    std::vector<Decal> decals;
    std::vector<Tread> treads;      // ring of fading tread marks (capped)
    size_t treadHead = 0;
    float nukeFlash = 0;            // seconds of white-out left after a tactical nuke goes off on screen
    float frameDt = 0;              // wall time this frame, zero while paused (drives particle emission and ageing)
    Rng fxRng{ 0xFA11 };

    void startGame();
    void update(float dt);          // called each frame; runs fixed sim ticks
    void render();
    void handleEvent(const SDL_Event& e);
    void addMessage(const char* text, Color c);
    void screenshot(const char* path);
    void spawnFromFx();
    void updateParticles(float dt);
private:
    float accumulator = 0;
    void processEvents();
    void menuEvent(const SDL_Event& e);
    void gameEvent(const SDL_Event& e);
    void renderMenu();
    void renderWorld();
    void renderHud();
    void renderGameOver();
    void renderPauseMenu();
    void pauseMenuEvent(const SDL_Event& e);
    void pauseMenuActivate(int row);
    void cancelModes();
    void drawNukes();
    void drawAirlifts();
    void drawAidDrops();
    void scroll(float dt);
    Vec2 screenToWorld(int sx, int sy) const { return Vec2(sx + cam.x, sy + cam.y); }
    Vec2 worldToScreen(Vec2 w) const { return Vec2(w.x - cam.x, w.y - cam.y); }
    Vec2 entPos(const Entity& e) const { return e.isUnit() ? e.prevPos + (e.pos - e.prevPos) * renderAlpha : e.pos; }
    Entity* pickEntity(Vec2 world, bool ownOnly);
    void selectSingle(Entity* e, bool add);
    void selectBox(Vec2 a, Vec2 b, bool add);
    void selectSameType(Entity* e);
    void issueAttackMove(Vec2 world);
    bool issueForceFire(Vec2 world);   // Force Fire button: attack the unit or structure under the click on purpose (friends included)
    void cleanSelection();
    struct Button { int x, y, w, h; int kind; int id; bool enabled; const char* label; std::string tip; };
    std::vector<Button> buttons;
    void buildButtons();
public:
    void issueRightClick(Vec2 world);
    void buildButtonsPublic() { buildButtons(); }                 // test hooks
    const std::vector<Button>& buttonsPublic() const { return buttons; }
private:
    void clickButton(const Button& b);
    bool selectionHasRole(UnitRole r) const;
    Entity* selectedBuilding() const;
    int selectionOwner() const;
    void drawEntity(Entity& e);
    void drawBuildingAnim(const Entity& e, Vec2 p, bool disabled);
    void drawBuildingDamage(const Entity& e, Vec2 p);
    void drawFx();
    void drawClouds();
    void drawGroundFx();            // scorch marks, ruins and wrecks: on the ground, under everything that stands or drives
    void drawParticles(int pass);
    // spawnFromFx(): cosmetic debris, sparks, smoke and scorch for sim effects the first frame they show up
    void emitP(float x, float y, float vx, float vy, float life, float s0, float s1, Color col, u8 kind, u8 var = 0, float ay = 0, float drag = 0, float rot = 0, float vrot = 0);
    bool onScreen(float x, float y, float pad = 48) const { return x > cam.x - pad && y > cam.y - pad && x < cam.x + SCREEN_W + pad && y < cam.y + VIEW_H + pad; }
    void drawShroud();
    void drawZones();
    void drawRangeRings();
    void rangeRing(Vec2 screenPos, float radiusPx, int weapon, const char* label, bool powered, bool enemy, bool faint);
    void applyArea(Vec2 center, float radius);
    bool selectionCanArea() const;
    void drawMinimap(int x, int y, int size);
    void hotkey(SDL_Keycode k, u16 mod);
    void cmdSelection(int kind, int id);
};
extern Game g_game;
