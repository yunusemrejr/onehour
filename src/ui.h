// One Hour - menu, HUD, input and world rendering
#pragma once
#include "sim.h"
#include "gfx.h"
#include "audio.h"
#include "ai.h"

enum GameState { GS_MENU = 0, GS_PLAYING, GS_GAMEOVER };

struct MenuSettings {
    Faction playerFaction = F_CYBER;
    int enemies = 2;
    int enemyFaction[3] = { 2, 2, 2 };   // 0 cyber, 1 clanker, 2 random
    int difficulty = 1;
    bool enemiesAllied = true;
    int cursor = 0;
};

struct Message { std::string text; float time; Color color; };

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
    bool powerMode = false;
    bool rallyMode = false;
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
    float escArmedUntil = -1;
    struct Ping { Vec2 pos; float time; };
    std::vector<Ping> pings;

    void startGame();
    void update(float dt);          // called each frame; runs fixed sim ticks
    void render();
    void handleEvent(const SDL_Event& e);
    void addMessage(const char* text, Color c);
    void screenshot(const char* path);
private:
    float accumulator = 0;
    void processEvents();
    void menuEvent(const SDL_Event& e);
    void gameEvent(const SDL_Event& e);
    void renderMenu();
    void renderWorld();
    void renderHud();
    void renderGameOver();
    void scroll(float dt);
    Vec2 screenToWorld(int sx, int sy) const { return Vec2(sx + cam.x, sy + cam.y); }
    Vec2 worldToScreen(Vec2 w) const { return Vec2(w.x - cam.x, w.y - cam.y); }
    Vec2 entPos(const Entity& e) const { return e.isUnit() ? e.prevPos + (e.pos - e.prevPos) * renderAlpha : e.pos; }
    Entity* pickEntity(Vec2 world, bool ownOnly);
    void selectSingle(Entity* e, bool add);
    void selectBox(Vec2 a, Vec2 b, bool add);
    void selectSameType(Entity* e);
    void issueRightClick(Vec2 world);
    void issueAttackMove(Vec2 world);
    void cleanSelection();
    struct Button { int x, y, w, h; int kind; int id; bool enabled; const char* label; std::string tip; };
    std::vector<Button> buttons;
    void buildButtons();
    void clickButton(const Button& b);
    bool selectionHasRole(UnitRole r) const;
    Entity* selectedBuilding() const;
    int selectionOwner() const;
    void drawEntity(Entity& e);
    void drawFx();
    void drawShroud();
    void drawMinimap(int x, int y, int size);
    void hotkey(SDL_Keycode k, u16 mod);
    void cmdSelection(int kind, int id);
};
extern Game g_game;
