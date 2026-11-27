// The Bunker from Hell: the moment it is finished it is garrisoned and fitted out like no ordinary bunker.
#include "sim.h"
#include <algorithm>
#include <cmath>

// Every Bunker from Hell comes with its garrison already in the shaft (see B5): HELL_GARRISON_INF soldiers (snipers, laser commandos and
// riflemen/rocket troopers for the rest), HELL_GARRISON_DOZERS dozers and HELL_GARRISON_TANKS tanks in the same hold (they unload with the
// familiar Unload command as the real units they are). It is fitted with every BunkerUpg at once, and this happens once: after the garrison
// has been let out it is not refilled.
void Sim::updateHellBunker(Entity& b) {
    if (!b.constructed || b.hellGarrisoned || b.owner < 0 || b.owner >= numPlayers) return;
    b.hellGarrisoned = true;
    b.bunkerUpg = (u8)((1 << BU_COUNT) - 1);
    b.paraReady = b.rainReady = b.tankReady = 0;
    b.nostopmeReadyAt = time + HELL_NOSTOPME_FIRST;   // its own warhead arms HELL_NOSTOPME_FIRST after the garrison is fitted
    anyBunkerLasers = true;   // its BU_LASERS battery burns rockets and shells from the first moment
    const int owner = b.owner;
    const Vec2 pos = b.pos;
    const Ref bunker = refOf(b);
    const bool cy = players[owner].faction == F_CYBER;
    const int rifle = cy ? U_C_INF1 : U_K_INF1, rocket = cy ? U_C_INF2 : U_K_INF2;
    const int sniper = cy ? U_C_SNIPER : U_K_SNIPER, commando = cy ? U_C_LASERCMD : U_K_LASERCMD;
    const int dozer = cy ? U_C_DOZER : U_K_DOZER, tank = cy ? U_C_TANK : U_K_TANK, titan = cy ? U_C_TITAN : U_K_TITAN;
    std::vector<int> load;
    for (int k = 0; k < HELL_GARRISON_COMMANDOS; k++) load.push_back(commando);
    for (int k = 0; k < HELL_GARRISON_SNIPERS; k++) load.push_back(sniper);
    for (int k = HELL_GARRISON_SNIPERS + HELL_GARRISON_COMMANDOS; k < HELL_GARRISON_INF; k++) load.push_back((k & 1) ? rocket : rifle);
    for (int k = 0; k < HELL_GARRISON_DOZERS; k++) load.push_back(dozer);
    const bool heavy = unitAvailable(owner, titan);   // its super-heavy tanks among them once the army has unlocked them, as the Tank Drop does
    for (int k = 0; k < HELL_GARRISON_TANKS; k++) load.push_back(heavy && k % 3 == 1 ? titan : tank);
    for (int t : load) {
        Ref u = spawnUnit(t, owner, pos);   // (spawning may move ents: look both up again)
        Entity* c = get(bunker); Entity* s = get(u);
        if (c && s) boardQuiet(*s, *c);
    }
}

// The two autonomous ballistic launchers (B13): each fires its own long-range rocket at an enemy ground unit or aircraft in reach, then reloads
// for the weapon's cooldown. They reuse the ordinary structure-weapon path (acquireTarget/canTarget/fireWeapon), so a stealthy Ghost jet that
// nothing has sighted is never picked. Only a constructed, powered-down-free (not disabled) Bunker from Hell fires them, from its own plates.
void Sim::updateHellLaunchers(Entity& b) {
    if (!b.constructed || b.disabledUntil > time) return;
    const Weapon& w = WEAPONS[W_HELL_ROCKET];
    for (int i = 0; i < 2; i++) {
        if (b.hellRocketCd[i] > 0) { b.hellRocketCd[i] = std::max(0.0f, b.hellRocketCd[i] - SIM_DT); continue; }
        Entity* t = acquireTarget(b, w.range + 0.2f, W_HELL_ROCKET);
        if (!t) { b.hellRocketCd[i] = 0.5f; continue; }   // nothing in reach: look again in half a second, not every tick
        const Vec2 muzzle = bunkerGunPos(b, i);
        b.turret = std::atan2(t->pos.y - b.pos.y, t->pos.x - b.pos.x);
        fireWeapon(b, *t, w, &muzzle);
        b.hellRocketCd[i] = w.cooldown;
    }
}

// nostopme (B14): one warhead of both Ghost warheads together - the larger of their blast radii and the sum of their damage factors - that
// flies in HELL_NOSTOPME_FLIGHT seconds and that no defence can touch (Nuke::uninterceptable). Armed HELL_NOSTOPME_FIRST after the bunker is
// finished, then every HELL_NOSTOPME_COOLDOWN. Its damage is cut against a side with an enemy Bunker from Hell in the blast (B15, nukeBlast).
bool Sim::cmdNostopme(int player, Ref bunker, Vec2 at) {
    if (player < 0 || player >= numPlayers || !std::isfinite(at.x) || !std::isfinite(at.y)
        || at.x < 0 || at.y < 0 || at.x >= WORLD_W || at.y >= WORLD_H) return false;
    Entity* b = get(bunker);
    if (!b || !b->alive || !b->isBuilding() || !b->constructed || !b->hellGarrisoned || b->owner != player || !hellBunker(b->type) || b->disabledUntil > time) return false;   // (garrisoned = its first deadline is set)
    if (time < b->nostopmeReadyAt) { emit(EV_MSG, player, SND_CANT, b->pos, "nostopme is not ready"); return false; }
    b->nostopmeReadyAt = time + HELL_NOSTOPME_COOLDOWN;
    Nuke n;
    n.from = b->pos; n.pos = at; n.owner = player; n.t = 0;
    n.scale = std::max(SUB_NUKE_SCALE * 4.0f, NUKEBOMBER_SCALE * 9.0f);   // the Ghosts' larger warhead radius
    n.dmgMul = SUB_NUKE_DAMAGE * 2.0f + 5.0f;                             // ... and both their damage factors added (1.0 base * dmgMul)
    n.flight = HELL_NOSTOPME_FLIGHT;
    n.uninterceptable = true;
    n.nostopme = true;
    nukes.push_back(n);
    emit(EV_SOUND, -1, SND_ROCKET, b->pos);
    emit(EV_MSG, player, SND_NONE, at, "nostopme launched");
    for (int p = 0; p < numPlayers; p++) if (enemies(player, p)) {
        emit(EV_MSG, p, SND_ATTACKED, at, "NUCLEAR LAUNCH DETECTED");
        emit(EV_UNDER_ATTACK, p, SND_ATTACKED, at, "Nuclear missile incoming!");
    }
    return true;
}

// What the UI reads for the nostopme button: seconds left, 0 when armed, -1 for anything but a Bunker from Hell.
float Sim::nostopmeCooldownLeft(const Entity& bunker) const {
    if (!bunker.alive || !bunker.isBuilding() || !hellBunker(bunker.type)) return -1.0f;
    if (!bunker.hellGarrisoned) return HELL_NOSTOPME_FIRST;   // (finished this tick: its deadline is set on the next update)
    return std::max(0.0f, bunker.nostopmeReadyAt - time);
}
