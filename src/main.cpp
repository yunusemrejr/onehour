// One Hour - entry point
#include "ui.h"
#include <SDL2/SDL.h>
#include <chrono>
#include <thread>
static const int INFO_X_TEST = 134;

static void usage() {
    printf("One Hour - compact Zero Hour style skirmish RTS\n"
           "  --scale N        window scale factor (default 1, logical 1024x640)\n"
           "  --software       use the software renderer\n"
           "  --selftest [T]   run an AI-only game headless for T seconds of sim time and report\n"
           "  --shot FILE      render a frame to FILE (BMP) after --ticks and exit (headless)\n"
           "  --ticks N        sim ticks to run before --shot (default 0 = main menu)\n"
           "  --seed S         random seed for the game\n"
           "  --faction c|k    your faction for --shot/--selftest\n");
}

static bool selfTest(int seconds, u64 seed, int players, int d0, bool swap) {
    Faction fac[4] = { F_CYBER, F_CLANKER, F_CLANKER, F_CYBER };
    if (swap) { fac[0] = F_CLANKER; fac[1] = F_CYBER; }
    if (getenv("ONEHOUR_MIRROR")) { Faction m = (Faction)atoi(getenv("ONEHOUR_MIRROR")); fac[0] = fac[1] = m; }
    bool ai[4] = { true, true, true, true };
    int diff[4] = { d0, 3, 2, 1 };
    int team[4] = { 0, 1, 2, 3 };
    g_sim.init(players, fac, ai, diff, team, seed);
    g_ai.init(seed);
    int ticks = seconds * SIM_HZ;
    auto t0 = std::chrono::steady_clock::now();
    int maxEnts = 0;
    for (int t = 0; t < ticks && !g_sim.gameOver; t++) {
        g_sim.step();
        g_ai.update();
        g_sim.events.clear();
        int alive = 0;
        for (auto& e : g_sim.ents) {
            if (!e.alive) continue;
            alive++;
            if (!(e.pos.x == e.pos.x) || !(e.pos.y == e.pos.y)) { fprintf(stderr, "NaN position at tick %d\n", t); return false; }
            if (e.pos.x < 0 || e.pos.y < 0 || e.pos.x > WORLD_W || e.pos.y > WORLD_H) { fprintf(stderr, "entity out of world at tick %d\n", t); return false; }
            if (e.isUnit() && !e.isAir() && !g_map.terrainPassable(tileOf(e.pos.x), tileOf(e.pos.y))) { fprintf(stderr, "ground unit on impassable terrain at tick %d (%s)\n", t, e.ut().name); return false; }
            if (e.hp > e.maxHp + 0.01f) { fprintf(stderr, "hp above max at tick %d\n", t); return false; }
        }
        maxEnts = std::max(maxEnts, alive);
        if (t % (60 * SIM_HZ) == 0) {
            printf("t=%4ds", t / SIM_HZ);
            for (int p = 0; p < g_sim.numPlayers; p++) {
                Player& pl = g_sim.players[p];
                printf(" | P%d %s $%5d u%3d b%2d pw%2d/%2d%s", p, pl.faction == F_CYBER ? "CYB" : "CLK", pl.money, g_sim.countUnits(p), g_sim.countBuildings(p, -1, false), pl.powerUsed, pl.powerMade, pl.alive ? "" : " DEAD");
            }
            printf("\n");
        }
    }
    auto t1 = std::chrono::steady_clock::now();
    double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
    printf("selftest: %d ticks in %.0f ms (%.3f ms/tick), peak entities %d, gameOver=%d winnerTeam=%d\n", ticks, ms, ms / std::max(1, ticks), maxEnts, (int)g_sim.gameOver, g_sim.winnerTeam);
    for (int p = 0; p < g_sim.numPlayers; p++) {
        Player& pl = g_sim.players[p];
        printf("  P%d: built %d lost %d kills %d structures killed %d harvested %d alive=%d\n", p, pl.unitsBuilt, pl.unitsLost, pl.unitsKilled, pl.structuresKilled, pl.harvested, (int)pl.alive);
        if (getenv("ONEHOUR_DEBUG")) {
            printf("      value dealt per credit:");
            for (int u = 0; u < U_COUNT; u++) if (pl.spentOn[u] > 0) printf(" %s %.2f (spent %d)", UNITS[u].name, pl.valueDealt[u] / pl.spentOn[u], (int)pl.spentOn[u]);
            printf("\n");
        }
    }
    return true;
}

// Drives the player-facing commands for both factions: every structure, every unit, sell, powers, orders.
static bool scenarioTest(u64 seed) {
    for (int fi = 0; fi < 2; fi++) {
        Faction fac[2] = { (Faction)fi, (Faction)(1 - fi) };
        bool ai[2] = { false, true }; int diff[2] = { 1, 1 }; int team[2] = { 0, 1 };
        g_sim.init(2, fac, ai, diff, team, seed + fi);
        g_ai.init(seed);
        Player& pl = g_sim.players[0];
        pl.money = 200000;
        std::fill(pl.explored.begin(), pl.explored.end(), 1);
        Ref dozer;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) dozer = g_sim.refOf(e);
        int bbase = firstBuildOf(fac[0]), ubase = firstUnitOf(fac[0]);
        // place every structure in a ring around the base, in prerequisite order, and let one dozer build them
        int order[8] = { BR_POWER, BR_SUPPLY, BR_BARRACKS, BR_FACTORY, BR_TURRET, BR_AATURRET, BR_TECH, BR_AIRFIELD };
        std::vector<Ref> sites;
        for (int k = 0; k < 8; k++) {
            int type = bbase + order[k];
            bool placed = false;
            for (int ring = 3; ring < 20 && !placed; ring++) for (int dy = -ring; dy <= ring && !placed; dy++) for (int dx = -ring; dx <= ring && !placed; dx++) {
                if (std::abs(dx) != ring && std::abs(dy) != ring) continue;
                int tx = tileOf(pl.basePos.x) + dx, ty = tileOf(pl.basePos.y) + dy;
                if (!g_sim.canPlace(0, type, tx, ty)) continue;
                // must be buildable regardless of prerequisites at this point: run the sim until prereqs exist
                for (int guard = 0; guard < 20 * 120 && !g_sim.buildAvailable(0, type); guard++) { g_sim.step(); g_sim.events.clear(); }
                if (!g_sim.buildAvailable(0, type)) { fprintf(stderr, "scenario: %s never became available\n", BUILDS[type].name); return false; }
                if (!g_sim.canPlace(0, type, tx, ty)) continue;
                if (!g_sim.cmdBuild(dozer, type, tx, ty)) { fprintf(stderr, "scenario: cmdBuild failed for %s\n", BUILDS[type].name); return false; }
                placed = true;
                // wait for construction to finish
                for (int guard = 0; guard < 20 * 200; guard++) {
                    g_sim.step(); g_sim.events.clear();
                    Entity* d = g_sim.get(dozer);
                    if (!d) { fprintf(stderr, "scenario: dozer died\n"); return false; }
                    if (d->order == O_IDLE) break;
                }
                if (!g_sim.hasBuilding(0, type)) { fprintf(stderr, "scenario: %s not completed\n", BUILDS[type].name); return false; }
            }
            if (!placed) { fprintf(stderr, "scenario: no spot for %s\n", BUILDS[type].name); return false; }
        }
        // train every unit from its producer
        for (int u = 0; u < UNITS_PER_FACTION; u++) {
            int type = ubase + u;
            Ref prod;
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.constructed && e.bt().role == UNITS[type].builtBy) { prod = g_sim.refOf(e); break; }
            if (!prod.valid()) { fprintf(stderr, "scenario: no producer for %s\n", UNITS[type].name); return false; }
            if (!g_sim.cmdTrain(prod, type)) { fprintf(stderr, "scenario: cmdTrain failed for %s\n", UNITS[type].name); return false; }
        }
        int before = g_sim.countUnits(0);
        for (int guard = 0; guard < 20 * 120; guard++) { g_sim.step(); g_sim.events.clear(); if (g_sim.countUnits(0) >= before + UNITS_PER_FACTION) break; }
        if (g_sim.countUnits(0) < before + UNITS_PER_FACTION) { fprintf(stderr, "scenario: not all units produced (%d/%d)\n", g_sim.countUnits(0) - before, UNITS_PER_FACTION); return false; }
        // orders: attack-move the army at the enemy base, harvest, power, sell
        std::vector<Ref> army, harv;
        for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) { if (e.ut().role == UR_COMBAT) army.push_back(g_sim.refOf(e)); if (e.ut().role == UR_HARVESTER) harv.push_back(g_sim.refOf(e)); }
        g_sim.cmdMove(army, g_sim.players[1].basePos, true);
        Ref pile; for (auto& e : g_sim.ents) if (e.alive && e.kind == EK_RESOURCE) { pile = g_sim.refOf(e); break; }
        g_sim.cmdHarvest(harv, pile);
        g_sim.players[0].powerReady = 0;
        if (!g_sim.cmdPower(0, g_sim.players[1].basePos)) { fprintf(stderr, "scenario: power failed\n"); return false; }
        for (int guard = 0; guard < 20 * 240; guard++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        g_sim.cmdStop(army);
        Ref sellMe; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_BARRACKS) sellMe = g_sim.refOf(e);
        if (sellMe.valid()) {
            int m = g_sim.players[0].money; g_sim.cmdSell(sellMe);
            if (g_sim.players[0].money <= m) { fprintf(stderr, "scenario: sell gave no refund\n"); return false; }
        } else printf("scenario: barracks already lost to the enemy before selling\n");
        for (int guard = 0; guard < 20 * 60; guard++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
        printf("scenario %s: ok (units %d, structures %d, enemy structures %d, time %.0fs)\n", FACTION_NAME[fac[0]], g_sim.countUnits(0), g_sim.countBuildings(0, -1, false), g_sim.countBuildings(1, -1, false), g_sim.time);
    }
    return true;
}

// Feeds synthetic SDL events through the real input handling (headless) and checks the outcomes.
static bool uiTest() {
    auto key = [](SDL_Keycode k, u16 mod = 0) { SDL_Event e; SDL_zero(e); e.type = SDL_KEYDOWN; e.key.keysym.sym = k; e.key.keysym.mod = mod; g_game.handleEvent(e); };
    auto motion = [](int x, int y) { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEMOTION; e.motion.x = x; e.motion.y = y; g_game.handleEvent(e); };
    auto click = [&](int x, int y, int btn) {
        motion(x, y);
        SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = btn; e.button.x = x; e.button.y = y; g_game.handleEvent(e);
        e.type = SDL_MOUSEBUTTONUP; g_game.handleEvent(e);
    };
    auto frames = [](int n) { for (int i = 0; i < n; i++) { g_game.update(1.0f / 60); g_game.render(); } };
    auto fail = [](const char* m) { fprintf(stderr, "uitest: %s\n", m); return false; };
    // menu: pick Clanker, 1 enemy, start
    key(SDLK_RIGHT); // faction -> clanker
    key(SDLK_DOWN); key(SDLK_LEFT); // enemies 2 -> 1
    for (int i = 0; i < 8 && g_game.state != GS_PLAYING; i++) { key(SDLK_DOWN); if (g_game.menu.cursor == 7) key(SDLK_RETURN); }
    if (g_game.state != GS_PLAYING) return fail("game did not start from the menu");
    if (g_sim.players[0].faction != F_CLANKER) return fail("faction selection ignored");
    if (g_sim.numPlayers != 2) return fail("enemy count ignored");
    frames(3);
    // click on the dozer
    Entity* dz = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit()) dz = &e;
    if (!dz) return fail("no dozer");
    Vec2 s = Vec2(dz->pos.x - g_game.cam.x, dz->pos.y - g_game.cam.y);
    click((int)s.x, (int)s.y, SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1) return fail("dozer not selected by click");
    // hotkey P = power plant, place it below the base
    key(SDLK_p);
    if (g_game.placingType < 0) return fail("build hotkey did not enter placement");
    Vec2 base = g_sim.players[0].basePos;
    int placed = 0;
    for (int dy = 4; dy < 12 && !placed; dy++) for (int dx = -8; dx < 8 && !placed; dx++) {
        int tx = tileOf(base.x) + dx, ty = tileOf(base.y) + dy;
        if (!g_sim.canPlace(0, g_game.placingType, tx, ty)) continue;
        Vec2 c = g_sim.buildingCenter(g_game.placingType, tx, ty);
        click((int)(c.x - g_game.cam.x), (int)(c.y - g_game.cam.y), SDL_BUTTON_LEFT);
        placed = 1;
    }
    if (!placed || g_sim.countRole(0, BR_POWER, false) != 1) return fail("power plant not placed by click");
    if (g_game.placingType >= 0) return fail("placement mode should end after placing");
    // right-click move the dozer somewhere: it should abandon construction (order becomes MOVE)
    click(400, 200, SDL_BUTTON_RIGHT);
    if (dz->order != O_MOVE) return fail("right-click move ignored");
    // reassign: right-click the site to continue construction
    Entity* site = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && !e.constructed) site = &e;
    if (!site) return fail("site missing");
    click((int)(site->pos.x - g_game.cam.x), (int)(site->pos.y - g_game.cam.y), SDL_BUTTON_RIGHT);
    if (dz->order != O_BUILD) return fail("right-click on own site did not resume construction");
    // let it build, then select the HQ and queue a dozer with the D hotkey, then cancel it
    for (int i = 0; i < 60 * 40 && !g_sim.hasRole(0, BR_POWER); i++) frames(1);
    if (!g_sim.hasRole(0, BR_POWER)) return fail("power plant never finished");
    Entity* hq = nullptr; for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && e.bt().role == BR_HQ) hq = &e;
    click((int)(hq->pos.x - g_game.cam.x), (int)(hq->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1 || g_sim.get(g_game.selection[0]) != hq) return fail("HQ not selected");
    int money = g_sim.players[0].money;
    key(SDLK_d);
    if (hq->queue.size() != 1) return fail("train hotkey did not queue");
    // cancel via the queue slot click in the HUD
    click(INFO_X_TEST + 330 + 14, SCREEN_H - HUD_H + 70 + 14, SDL_BUTTON_LEFT);
    if (!hq->queue.empty() || g_sim.players[0].money != money) return fail("queue cancel did not refund");
    // command grid button click: first button (Dozer) trains
    click(676 + 20, SCREEN_H - HUD_H + 8 + 10, SDL_BUTTON_LEFT);
    if (hq->queue.size() != 1) return fail("command button click did not queue");
    // rally point by right-click on ground with HQ selected
    click(300, 300, SDL_BUTTON_RIGHT);
    if (!hq->hasRally) return fail("rally point not set");
    // control groups: select dozer, Ctrl+1, clear, press 1
    click((int)(dz->pos.x - g_game.cam.x), (int)(dz->pos.y - g_game.cam.y), SDL_BUTTON_LEFT);
    if (g_game.selection.size() != 1) return fail("dozer reselect failed");
    key(SDLK_1, KMOD_CTRL);
    key(SDLK_ESCAPE);
    if (!g_game.selection.empty()) return fail("Esc did not clear selection");
    key(SDLK_1);
    if (g_game.selection.size() != 1) return fail("control group recall failed");
    // drag box select over the dozer
    Vec2 d2 = Vec2(dz->pos.x - g_game.cam.x, dz->pos.y - g_game.cam.y);
    motion((int)d2.x - 30, (int)d2.y - 30);
    { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONDOWN; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)d2.x - 30; e.button.y = (int)d2.y - 30; g_game.handleEvent(e); }
    motion((int)d2.x + 30, (int)d2.y + 30);
    { SDL_Event e; SDL_zero(e); e.type = SDL_MOUSEBUTTONUP; e.button.button = SDL_BUTTON_LEFT; e.button.x = (int)d2.x + 30; e.button.y = (int)d2.y + 30; g_game.handleEvent(e); }
    if (g_game.selection.size() != 1) return fail("drag box select failed");
    // minimap click moves the camera
    Vec2 camBefore = g_game.cam;
    click(8 + 100, SCREEN_H - HUD_H + 8 + 100, SDL_BUTTON_LEFT);
    if (dist(camBefore, g_game.cam) < 100) return fail("minimap click did not move camera");
    key(SDLK_HOME);
    // pause toggles, speed keys, help, mute, screenshot key path
    key(SDLK_SPACE); if (!g_game.paused) return fail("pause failed"); key(SDLK_SPACE);
    key(SDLK_EQUALS); if (g_game.speed <= 1.0f) return fail("speed up failed"); key(SDLK_MINUS);
    key(SDLK_F1); if (!g_game.showHelp) return fail("help failed"); key(SDLK_F1);
    // run a while with rendering to shake out draw paths, then Esc twice to the menu
    frames(120);
    key(SDLK_ESCAPE); key(SDLK_ESCAPE); key(SDLK_ESCAPE);
    if (g_game.state != GS_MENU) return fail("double Esc did not return to the menu");
    printf("uitest: ok\n");
    return true;
}

int main(int argc, char** argv) {
    int scale = 1; bool software = false; bool headless = false;
    int selftestSecs = -1; const char* shot = nullptr; int ticks = 0; u64 seed = 12345; Faction faction = F_CYBER; int players = 4; int d0 = 3; bool swap = false; int viewPlayer = 0; bool allAi = false; bool autostart = false;
    for (int i = 1; i < argc; i++) {
        std::string a = argv[i];
        if (a == "--scale" && i + 1 < argc) scale = clampi(atoi(argv[++i]), 1, 4);
        else if (a == "--software") software = true;
        else if (a == "--selftest") { selftestSecs = 600; if (i + 1 < argc && argv[i + 1][0] != '-') selftestSecs = atoi(argv[++i]); }
        else if (a == "--shot" && i + 1 < argc) { shot = argv[++i]; headless = true; software = true; }
        else if (a == "--ticks" && i + 1 < argc) ticks = atoi(argv[++i]);
        else if (a == "--seed" && i + 1 < argc) seed = (u64)atoll(argv[++i]);
        else if (a == "--players" && i + 1 < argc) players = clampi(atoi(argv[++i]), 2, 4);
        else if (a == "--swap") swap = true;
        else if (a == "--allai") allAi = true;
        else if (a == "--soundcheck") { g_audio.debugStats(); return 0; }
        else if (a == "--uitest") { headless = true; software = true; shot = nullptr; ticks = -1; }
        else if (a == "--autostart") autostart = true;
        else if (a == "--scenario") { g_map.generate(); return scenarioTest(seed) ? 0 : 1; }
        else if (a == "--view" && i + 1 < argc) viewPlayer = clampi(atoi(argv[++i]), 0, 9);
        else if (a == "--d0" && i + 1 < argc) d0 = clampi(atoi(argv[++i]), 0, 3);
        else if (a == "--faction" && i + 1 < argc) faction = argv[++i][0] == 'k' ? F_CLANKER : F_CYBER;
        else if (a == "--help" || a == "-h") { usage(); return 0; }
        else { fprintf(stderr, "unknown option %s\n", argv[i]); usage(); return 1; }
    }
    g_map.generate();
    if (selftestSecs >= 0) return selfTest(selftestSecs, seed, players, d0, swap) ? 0 : 1;

    if (headless) SDL_SetHint(SDL_HINT_VIDEODRIVER, "dummy");
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_EVENTS) != 0) { fprintf(stderr, "SDL: %s\n", SDL_GetError()); return 1; }
    if (!g_gfx.init(scale, software)) return 1;
    if (!headless) g_audio.init();

    g_game.seed = seed;
    g_game.menu.playerFaction = faction;
    if (ticks == -1) { bool ok = uiTest(); g_gfx.shutdown(); SDL_Quit(); return ok ? 0 : 1; }
    if (shot) {
        if (ticks > 0) {
            g_game.startGame();
            if (allAi) { g_sim.players[0].isAI = true; g_ai.init(seed); }
            for (int t = 0; t < ticks; t++) { g_sim.step(); g_ai.update(); g_sim.events.clear(); }
            Vec2 b = g_sim.players[std::min(viewPlayer, 3)].basePos;
            if (viewPlayer == 9) {
                // centre on the action: the unit hit most recently
                float best = -1; for (auto& e : g_sim.ents) if (e.alive && e.lastDamaged > best) { best = e.lastDamaged; b = e.pos; }
                if (!g_sim.projs.empty()) b = g_sim.projs[0].pos;
                printf("action at tile %d,%d (projectiles %zu, fx %zu, lastDamaged %.1f at t=%.1f)\n", tileOf(b.x), tileOf(b.y), g_sim.projs.size(), g_sim.fx.size(), best, g_sim.time);
            }
            g_game.cam = Vec2(clampf(b.x - SCREEN_W / 2, 0, WORLD_W - SCREEN_W), clampf(b.y - VIEW_H / 2, 0, WORLD_H - VIEW_H));
            g_game.renderAlpha = 1;
            if (viewPlayer != 0) { std::fill(g_sim.players[0].explored.begin(), g_sim.players[0].explored.end(), 1); }
            // select something for the HUD
            for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isBuilding() && !e.queue.empty()) { g_game.selection.push_back(g_sim.refOf(e)); break; }
            if (g_game.selection.empty()) for (auto& e : g_sim.ents) if (e.alive && e.owner == 0 && e.isUnit() && e.ut().role == UR_DOZER) { g_game.selection.push_back(g_sim.refOf(e)); break; }
        }
        g_game.render();
        g_game.screenshot(shot);
        printf("wrote %s\n", shot);
        g_gfx.shutdown(); SDL_Quit();
        return 0;
    }

    if (autostart) { g_game.startGame(); if (allAi) { g_sim.players[0].isAI = true; g_ai.init(seed); } g_game.speed = 3.0f; }
    u64 freq = SDL_GetPerformanceFrequency();
    u64 last = SDL_GetPerformanceCounter();
    bool running = true;
    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) running = false;
            else g_game.handleEvent(e);
        }
        if (g_game.quitRequested) running = false;
        u64 now = SDL_GetPerformanceCounter();
        float dt = (float)((now - last) / (double)freq);
        last = now;
        if (dt > 0.1f) dt = 0.1f;
        g_game.update(dt);
        g_game.render();
        g_gfx.present();
        if (software) SDL_Delay(12);   // no vsync on the software path: cap around 60 fps
    }
    g_audio.shutdown();
    g_gfx.shutdown();
    SDL_Quit();
    return 0;
}
