#include "sim.h"

// ------------------------------------------------------------ The Chosen and its Shadow Guard
// The Chosen is one soldier, one per army at a time (a barracks with a tech structure trains it). Nothing finds it but a helicopter (see
// updateDetection): snipers, spy drones, planes, turrets, bunkers and scans all look straight through it, whatever it does. It carries no gun.
// Its "weapon" (W_CONVERT) takes a structure the moment it is in reach - any enemy or neutral one, as an infiltrator would but at once - and turns
// an enemy soldier, vehicle or ship near it to its own army on the spot (never an aircraft, a loan from a support power or another Chosen). Every
// CHOSEN_CALL_COOLDOWN seconds it can call CHOSEN_SQUAD Shadow Guard soldiers to where it stands, as stealthy as itself.

Ref Sim::chosenOf(int player) const {
    for (auto& e : ents) if (e.alive && e.isUnit() && e.owner == player && chosenType(e.type)) return Ref{ (i32)(&e - &ents[0]), e.gen };
    return NOREF;
}

bool Sim::chosenInProduction(int player) const {
    for (auto& e : ents) {
        if (!e.alive || e.owner != player) continue;
        if (e.isUnit() && chosenType(e.type)) return true;
        if (e.isBuilding()) for (int t : e.queue) if (chosenType(t)) return true;
    }
    for (auto& a : airlifts) for (size_t k = a.next; k < a.load.size(); k++) if (a.owner == player && chosenType(a.load[k])) return true;   // (one on its way down from a transport)
    return false;
}

float Sim::shadowWait(int player) const {
    if (!chosenOf(player).valid()) return -1;
    return std::max(0.0f, players[player].shadowReady - time);
}

bool Sim::mayConvert(const Entity& by, const Entity& t) const {
    if (!t.alive || !t.isUnit() || t.isAir() || t.ut().temp || chosenType(t.type) || t.carrier.valid() || t.fall > 0) return false;
    return enemies(by.owner, t.owner);
}

// A unit changes sides: it forgets what it was doing and stands where it is under its new owner. What rides in it changes sides with it; the jets
// in the air of a carrier that changes sides have nowhere to land and leave the map (its deck and the jets parked on it go with the ship).
void Sim::convertUnit(Entity& u, int to, const Entity* by) {
    const int from = u.owner;
    if (from == to || to < 0) return;
    u.owner = to;
    u.order = O_IDLE; u.postOrder = O_IDLE; u.resumeOrder = O_IDLE; u.targetEnt = NOREF; u.engaged = NOREF; u.forceTarget = NOREF; u.attacker = NOREF;
    u.path.clear(); u.pathIdx = 0; u.repathTimer = 0; u.zoneR = 0; u.leashed = false; u.autoTask = false; u.guardPos = u.pos; u.siteQueue.clear();
    u.autoHoldUntil = -1; u.evadeUntil = -1; u.unloading = false; u.riot = -1; u.cooldown = std::max(u.cooldown, 0.6f);
    for (int q = 0; q < MAX_PLAYERS; q++) u.detUntil[q] = 0;
    for (Ref r : u.passengers) if (Entity* p = get(r)) if (p->owner == from) { p->owner = to; p->riot = -1; for (int q = 0; q < MAX_PLAYERS; q++) p->detUntil[q] = 0; }
    const Ref me = refOf(u);
    for (auto& j : ents) if (j.alive && j.isUnit() && j.home == me && j.owner == from && j.isAir()) j.home = NOREF;
    if (from >= 0 && from < MAX_PLAYERS) { players[from].unitsLost++; players[from].convertedLost++; if (from < numPlayers) players[from].noteChosen(u.pos, time); }
    players[to].converted++;
    const Color c = players[to].faction == F_CYBER ? rgb(190, 236, 255) : rgb(255, 214, 150);
    fx.push_back({FX_RING, u.pos, u.pos, 0, 0.7f, c, u.radius() * 2.6f + 10});
    for (int k = 0; k < 5; k++) fx.push_back({FX_SPARK, u.pos + Vec2(rng.f(-u.radius(), u.radius()), rng.f(-u.radius(), u.radius())), Vec2(), 0, rng.f(0.25f, 0.45f), c, 4});
    if (by) fx.push_back({FX_BEAM, by->pos, u.pos, 0, 0.18f, rgb(236, 210, 255), 1.4f});
    char msg[96];
    snprintf(msg, sizeof msg, "The Chosen turned an enemy %s to our side", u.ut().name);
    emit(EV_MSG, to, SND_NONE, u.pos, msg);
    if (from >= 0 && from < numPlayers) {
        snprintf(msg, sizeof msg, "Our %s went over to the enemy: something unseen is among us", u.ut().name);
        emit(EV_UNDER_ATTACK, from, SND_ATTACKED, u.pos, msg);
    }
}

// The Chosen's turn at its target (it is only ever "fired" through tryFire, which has checked the reach): a structure is taken on the spot, a
// unit turned. Each act takes a moment before the next (CHOSEN_CONVERT_BASE plus a little per credit of what it turned).
bool Sim::chosenAct(Entity& e, Entity& t) {
    const float reach = WEAPONS[W_CONVERT].range * TILE;
    if (distToEntity(e.pos, t) > reach) return false;
    e.turret = e.angle = std::atan2(t.pos.y - e.pos.y, t.pos.x - e.pos.x);
    if (e.cooldown > 0) return true;
    if (t.isBuilding()) {
        if (!(t.constructed || t.district >= 0) || !spyMayTake(e.owner, t)) return false;
        std::vector<Ref> garrison = t.passengers;   // (a bunker's or stronghold's soldiers come out under the Chosen's eye: they change sides too)
        int from = t.owner;
        captureBuilding(t, e);
        if (from >= 0 && from < numPlayers) players[from].noteChosen(t.pos, time);
        if (t.owner == e.owner) {
            players[e.owner].chosenTaken++;
            for (Ref r : garrison) if (Entity* g = get(r)) if (g->owner == from && mayConvert(e, *g)) convertUnit(*g, e.owner, &e);
            fx.push_back({FX_BEAM, e.pos, t.pos, 0, 0.22f, rgb(236, 210, 255), 1.8f});
        }
        e.cooldown = CHOSEN_TAKE_GAP;
        return true;
    }
    if (!mayConvert(e, t)) return false;
    const float cost = (float)t.ut().cost;
    convertUnit(t, e.owner, &e);
    e.cooldown = CHOSEN_CONVERT_BASE + CHOSEN_CONVERT_PER_K * cost / 1000.0f;
    return true;
}

// The Shadow Guard pick their weapon by the target: aircraft get the stinger, vehicles, ships and structures a rocket, soldiers the carbine up
// close and the marksman's shot further off (or when the carbine would barely scratch a tough one)
int Sim::shadowWeapon(const Entity& e, const Entity& t, float dTiles) const {
    (void)e;
    if (t.isAir()) return W_SHADOW_AA;
    if (t.isBuilding() || t.armor() != AR_INF) return W_SHADOW_ROCKET;
    if (dTiles > 4.5f || t.hp > 300.0f) return W_SHADOW_SNIPER;
    return W_SHADOW_RIFLE;
}

bool Sim::cmdShadowCall(int player, Ref chosen) {
    if (player < 0 || player >= numPlayers || time < players[player].shadowReady) return false;
    Entity* c = get(chosen);
    if (!c || c->owner != player || !chosenType(c->type)) { Ref r = chosenOf(player); c = get(r); }
    if (!c || c->carrier.valid()) return false;
    const int type = players[player].faction == F_CYBER ? U_C_SHADOW : U_K_SHADOW;
    std::vector<Ref> squad;
    for (int i = 0; i < CHOSEN_SQUAD; i++) {   // in rings round the Chosen, each soldier on free ground near its slot
        int ring = i < 8 ? 0 : (i < 20 ? 1 : 2), slot = i < 8 ? i : (i < 20 ? i - 8 : i - 20), n = ring == 0 ? 8 : (ring == 1 ? 12 : 10);
        float a = slot * 6.2831853f / n + ring * 0.37f, r = (1.1f + ring * 0.9f) * TILE;
        Vec2 at = g_map.nearestFree(c->pos + Vec2(std::cos(a), std::sin(a)) * r, 5);
        Ref s = spawnUnit(type, player, at);
        if (Entity* u = get(s)) { u->angle = u->turret = a; u->guardPos = u->pos; squad.push_back(s); }
    }
    if (squad.empty()) return false;
    players[player].shadowReady = time + CHOSEN_CALL_COOLDOWN;
    players[player].shadowCalls++;
    const Color col = players[player].faction == F_CYBER ? rgb(190, 236, 255) : rgb(255, 214, 150);
    fx.push_back({FX_RING, c->pos, c->pos, 0, 0.9f, col, 3.6f * TILE});
    fx.push_back({FX_RING, c->pos, c->pos, 0, 1.3f, rgb(236, 210, 255), 5.0f * TILE});
    char msg[96]; snprintf(msg, sizeof msg, "The Chosen called its Shadow Guard: %d soldiers", (int)squad.size());
    emit(EV_MSG, player, SND_EMP, c->pos, msg);
    return true;
}
