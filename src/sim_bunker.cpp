#include "sim.h"

// ------------------------------------------------------------ per-bunker upgrades
// Each bunker is fitted with its own upgrades (BunkerUpg): select it, pick one. It is paid for when the fitting starts and takes
// BUNKER_UPGS[].time; one fitting at a time per bunker. What a bunker has goes with it if a spy takes it.
bool Sim::bunkerUpgAvailable(const Entity& b, int u) const {
    return u >= 0 && u < BU_COUNT && b.alive && b.isBuilding() && b.constructed && b.bt().role == BR_BUNKER && b.owner >= 0 && !bunkerHas(b, u) && b.bunkerBusy < 0;
}

bool Sim::cmdBunkerUpgrade(Ref bunker, int u) {
    Entity* b = get(bunker);
    if (!b || !bunkerUpgAvailable(*b, u)) return false;
    int p = b->owner;
    const BunkerUpgType& ug = BUNKER_UPGS[u];
    if (!canAfford(p, ug.cost)) { emit(EV_NOFUNDS, p, SND_NOFUNDS, b->pos, "Insufficient funds"); return false; }
    players[p].money -= ug.cost;
    b->bunkerBusy = u; b->bunkerProgress = 0;
    emit(EV_MSG, p, SND_CLICK, b->pos, (std::string(ug.name) + ": fitting this bunker").c_str());
    return true;
}

float Sim::bunkerParaWait(const Entity& b) const {
    if (!bunkerHas(b, BU_PARA)) return -1;
    return std::max(0.0f, b.paraReady - time);
}

// What one Para Backup carries: BUNKER_PARA_TROOPS soldiers. BUNKER_PARA_COMMANDOS laser commandos (no barracks trains them: they only come this way),
// then riflemen, anti-air troopers and snipers above all and the heavier kinds, each one only if the army has unlocked it (a kind it has not falls
// back to the plain rifleman). Never an infiltrator: they do not jump with the line troops.
std::vector<int> Sim::bunkerParaManifest(int player) const {
    std::vector<int> load;
    if (player < 0 || player >= numPlayers) return load;
    bool cy = players[player].faction == F_CYBER;
    const int rifle = cy ? U_C_INF1 : U_K_INF1, aa = cy ? U_C_INF2 : U_K_INF2, heavy = cy ? U_C_INF3 : U_K_INF3;
    const int elite = cy ? U_C_ELITE : U_K_ELITE, sniper = cy ? U_C_SNIPER : U_K_SNIPER, commando = cy ? U_C_LASERCMD : U_K_LASERCMD;
    // the stick in the order the soldiers jump (mixed, so every part of the drop zone gets some of each); 35 places
    static const int PATTERN[7] = { 4, 0, 1, 2, 0, 1, 3 };   // 0 rifle, 1 anti-air, 2 sniper, 3 heavy / elite in turn, 4 laser commando
    int extra = 0, commandos = 0;
    for (int k = 0; k < BUNKER_PARA_TROOPS; k++) {
        int slot = PATTERN[k % 7], t = rifle;
        if (slot == 4) t = commandos++ < BUNKER_PARA_COMMANDOS ? commando : rifle;
        else if (slot == 1) t = unitAvailable(player, aa) ? aa : rifle;
        else if (slot == 2) t = unitAvailable(player, sniper) ? sniper : (unitAvailable(player, aa) && (k & 1) ? aa : rifle);
        else if (slot == 3) { int want = (extra++ & 1) ? elite : heavy; t = unitAvailable(player, want) ? want : (unitAvailable(player, heavy) ? heavy : aa); if (!unitAvailable(player, t)) t = rifle; }
        load.push_back(t);
    }
    return load;
}

float Sim::bunkerRainWait(const Entity& b) const {
    if (!bunkerHas(b, BU_RAIN)) return -1;
    return std::max(0.0f, b.rainReady - time);
}
float Sim::bunkerTankWait(const Entity& b) const {
    if (!bunkerHas(b, BU_TANKS)) return -1;
    return std::max(0.0f, b.tankReady - time);
}

// What one Tank Drop carries: TANK_DROP_TANKS of the army's main battle tank (TANK_DROP_HEAVY of them its super-heavy once it has unlocked it), in
// the order they leave the plane, and the medic vehicle last (it lands in the middle of the stick).
std::vector<int> Sim::bunkerTankManifest(int player) const {
    std::vector<int> load;
    if (player < 0 || player >= numPlayers) return load;
    bool cy = players[player].faction == F_CYBER;
    const int tank = cy ? U_C_TANK : U_K_TANK, titan = cy ? U_C_TITAN : U_K_TITAN, medic = cy ? U_C_MEDIC : U_K_MEDIC;
    const bool heavy = unitAvailable(player, titan);
    for (int k = 0; k < TANK_DROP_TANKS; k++) {
        load.push_back(heavy && k % 3 == 1 && k / 3 < TANK_DROP_HEAVY ? titan : tank);
        if (k == TANK_DROP_TANKS / 2 - 1) load.push_back(medic);
    }
    return load;
}

// ------------------------------------------------------------ Upgrade All Bunkers
// One button on any bunker queues every upgrade the army's finished bunkers still lack. Each bunker then fits its queue one upgrade after another
// (the order below: protection first), paying for each fitting as it starts; one that cannot be paid waits until the money is there.
static const int BUNKER_FIT_ORDER[BU_COUNT] = { BU_SHELL, BU_LASERS, BU_MEDIC, BU_SWIFT, BU_TRIPLE, BU_PARA, BU_RAIN, BU_TANKS };

int Sim::bunkerUpgradesMissing(int player, int* cost, bool unqueued) const {
    int n = 0, c = 0;
    for (auto& b : ents) {
        if (!b.alive || !b.isBuilding() || b.owner != player || !b.constructed || b.bt().role != BR_BUNKER) continue;
        for (int u = 0; u < BU_COUNT; u++) {
            if (bunkerHas(b, u) || b.bunkerBusy == u || (unqueued && ((b.bunkerQueue >> u) & 1))) continue;
            n++; c += BUNKER_UPGS[u].cost;
        }
    }
    if (cost) *cost = c;
    return n;
}

int Sim::cmdBunkerUpgradeAll(int player) {
    if (player < 0 || player >= numPlayers) return 0;
    int n = 0, bunkers = 0, cost = 0;
    for (auto& b : ents) {
        if (!b.alive || !b.isBuilding() || b.owner != player || !b.constructed || b.bt().role != BR_BUNKER) continue;
        int before = n;
        for (int u = 0; u < BU_COUNT; u++) {
            if (bunkerHas(b, u) || b.bunkerBusy == u || ((b.bunkerQueue >> u) & 1)) continue;
            b.bunkerQueue |= (u8)(1 << u); n++; cost += BUNKER_UPGS[u].cost;
        }
        if (n > before) bunkers++;
        startQueuedFitting(b);   // (an idle bunker starts at once if the money is there)
    }
    if (n > 0) {
        char msg[128]; snprintf(msg, sizeof msg, "Upgrade All Bunkers: %d fitting%s queued on %d bunker%s ($%d in all, paid as each one starts)", n, n > 1 ? "s" : "", bunkers, bunkers > 1 ? "s" : "", cost);
        emit(EV_MSG, player, SND_CLICK, players[player].basePos, msg);
    }
    return n;
}

void Sim::cancelBunkerQueue(int player) {
    if (player < 0 || player >= numPlayers) return;
    bool any = false;
    for (auto& b : ents) if (b.alive && b.isBuilding() && b.owner == player && b.bunkerQueue) { b.bunkerQueue = 0; any = true; }
    if (any) emit(EV_MSG, player, SND_CLICK, players[player].basePos, "Upgrade All Bunkers: queue cancelled (fittings under way go on)");
}

// The next queued upgrade of an idle bunker, if its army can pay for it now (fitted ones leave the queue). True if a fitting started.
bool Sim::startQueuedFitting(Entity& b) {
    if (!b.bunkerQueue || b.bunkerBusy >= 0 || b.owner < 0 || !b.constructed) return false;
    for (int u : BUNKER_FIT_ORDER) {
        if (!((b.bunkerQueue >> u) & 1)) continue;
        if (bunkerHas(b, u)) { b.bunkerQueue &= (u8)~(1 << u); continue; }
        if (!canAfford(b.owner, BUNKER_UPGS[u].cost)) return false;   // (waits for the money: the order stays as it is)
        players[b.owner].money -= BUNKER_UPGS[u].cost;
        b.bunkerQueue &= (u8)~(1 << u);
        b.bunkerBusy = u; b.bunkerProgress = 0;
        return true;
    }
    return false;
}

// ------------------------------------------------------------ toxic clouds
// A Toxic Lance hit leaves a small cloud that poisons the enemy's ground units in it (see updateFallout: Fallout.toxic). A hit near one of the same
// army's clouds renews that one instead of stacking another, and there are never more than TOXIC_MAX on the map.
void Sim::toxicCloud(Vec2 at, int owner) {
    int count = 0;
    for (auto& f : fallouts) {
        if (!f.toxic) continue;
        count++;
        if (f.owner == owner && dist(f.pos, at) < TOXIC_RADIUS * TILE * 0.75f) {
            f.t = 0; f.power = TOXIC_POWER; f.r = TOXIC_RADIUS * TILE;
            if (f.visTag) for (auto& vf : fx) if (vf.type == FX_FALLOUT && vf.tag == f.visTag) { vf.t = 0; vf.size = f.r; break; }
            return;
        }
    }
    if (count >= TOXIC_MAX) return;
    const float R = TOXIC_RADIUS * TILE;
    fallouts.push_back({at, R, 0.0f, 0.0f, TOXIC_POWER, owner, true, TOXIC_LIFE});
    fallouts.back().toxic = true;
    fx.push_back({FX_FALLOUT, at, at, 0, TOXIC_LIFE, rgb(150, 255, 80), R});
    fx.back().tag = ++fxTagSeq;
    fallouts.back().visTag = fxTagSeq;
}

// A blast at strength k (1 = a direct hit by an ordinary warhead; already scaled for a big warhead, the distance and a shot-down warhead's
// pieces): the hatch takes BUNKER_NUKE_HATCH of its plate and the buried body BUNKER_NUKE_SHARE of its health (see applyDamage: a warhead on a
// bunker is taken as it is, with no plating, armour or floor in between).
void Sim::bunkerNuked(Entity& b, float k, int attackerOwner) {
    if (b.hatch > 0) {
        b.hatch -= b.hatchMax * BUNKER_NUKE_HATCH * k * (bunkerHas(b, BU_SHELL) ? BUNKER_SHELL_NUKE : 1.0f);
        if (b.hatch <= 0) { b.hatch = 0; hatchBlown(b); }
    }
    float share = BUNKER_NUKE_SHARE * k * (bunkerHas(b, BU_SHELL) ? BUNKER_SHELL_NUKE : 1.0f);
    applyDamage(b, b.maxHp * share, attackerOwner, NOREF, nullptr, -1.0f, true);
}

// The bunker mends itself without any upgrade, fast once it is left alone and a little even under fire (the army's Repair Crews only make it
// faster). A blown hatch is refitted after BUNKER_HATCH_REFIT seconds of quiet. It goes on while a pulse has knocked the bunker offline.
void Sim::bunkerMend(Entity& b) {
    const bool calm = time - b.lastDamaged >= BUNKER_REGEN_DELAY;
    const float up = hasUpgrade(b.owner, UPG_REPAIR) ? BUNKER_REGEN_UPG : 1.0f, k = (calm ? 1.0f : BUNKER_HATCH_HOT) * up;
    if (b.hp < b.maxHp) {
        b.hp = std::min(b.maxHp, b.hp + (calm ? b.maxHp * BUNKER_REGEN : BUNKER_REGEN_HOT) * up * SIM_DT);
        if ((tick + b.gen) % 24 == 0) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-34, 34), rng.f(-30, 30)), Vec2(), 0, 0.35f, players[b.owner].faction == F_CYBER ? rgb(120, 236, 255) : rgb(255, 214, 120), 3});
    }
    if (b.hatchMax > 0 && b.hatch < b.hatchMax) {
        if (b.hatch > 0) b.hatch = std::min(b.hatchMax, b.hatch + b.hatchMax * BUNKER_HATCH_REGEN * k * SIM_DT);
        else if (time - b.lastDamaged >= BUNKER_HATCH_REFIT) b.hatch = b.hatchMax * 0.25f;   // refitted: a new plate goes on, then it mends like the rest
    }
}

// Fitting the upgrade under way, then what the fitted ones do every tick: the laser battery's two roof lasers and the field hospital.
void Sim::updateBunkerUpgrades(Entity& b) {
    if (b.bunkerQueue && b.bunkerBusy < 0 && (tick + b.gen) % 15 == 0) startQueuedFitting(b);
    if (b.bunkerBusy >= 0) {
        int u = b.bunkerBusy;
        b.bunkerProgress += SIM_DT / BUNKER_UPGS[u].time;
        if ((tick + b.gen) % 10 == 0) fx.push_back({FX_SPARK, b.pos + Vec2(rng.f(-40, 40), rng.f(-38, 38)), Vec2(), 0, 0.3f, rgb(255, 226, 150), 3});
        if (b.bunkerProgress >= 1.0f) {
            b.bunkerUpg |= (u8)(1 << u); b.bunkerBusy = -1; b.bunkerProgress = 0;
            if (u == BU_SHELL) {   // the shell is poured round the old one: more health (the same share of it) and a heavier hatch
                float f = b.hp / b.maxHp;
                b.maxHp *= BUNKER_SHELL_HP; b.hp = b.maxHp * f;
                b.hatchMax *= BUNKER_SHELL_HATCH; b.hatch *= BUNKER_SHELL_HATCH;
            }
            if (u == BU_PARA) b.paraReady = time;
            if (u == BU_RAIN) b.rainReady = time;
            if (u == BU_TANKS) b.tankReady = time;
            if (u == BU_LASERS) anyBunkerLasers = true;
            fx.push_back({FX_RING, b.pos, b.pos, 0, 0.9f, rgb(255, 236, 170), 70.0f});
            emit(EV_MSG, b.owner, SND_BUILD_DONE, b.pos, (std::string(BUNKER_UPGS[u].name) + ": this bunker is upgraded").c_str());
        }
    }
    if (bunkerHas(b, BU_LASERS)) {   // two laser turrets: aircraft first, then whatever is closest in reach (the hatch can be gone: they stand on the roof)
        const Weapon& w = WEAPONS[W_BUNKER_SKYLASER];
        const float reach = (w.range + 0.25f) * TILE;
        for (int i = 0; i < 2; i++) {
            if (b.lzCd[i] > 0) b.lzCd[i] -= SIM_DT;
            Entity* t = get(b.lzTgt[i]);
            if (!t || !enemies(b.owner, t->owner) || !canTarget(b, *t, W_BUNKER_SKYLASER) || distToEntity(b.pos, *t) > reach) {
                t = nullptr; b.lzTgt[i] = NOREF;
                if ((tick + b.gen + i) % 3 == 0) {
                    Entity* best = nullptr; float bs = 1e9f;
                    Ref other = b.lzTgt[i ^ 1];
                    forEachNear(b.pos, reach, [&](Entity& c) {
                        if (!c.isUnit() || !enemies(b.owner, c.owner) || !canTarget(b, c, W_BUNKER_SKYLASER)) return;
                        float d = distToEntity(b.pos, c) / TILE;
                        if (d > w.range + 0.2f) return;
                        float sc = d - (c.isAir() ? 30.0f : 0.0f) - 2.0f * w.mult[c.armor()] + (refOf(c) == other ? 2.5f : 0.0f);   // (the two split their fire)
                        if (sc < bs) { bs = sc; best = &c; }
                    });
                    if (best) { t = best; b.lzTgt[i] = refOf(*best); }
                }
            }
            if (!t) continue;
            Vec2 m = bunkerLaserPos(b, i);
            b.lzAng[i] = std::atan2(t->pos.y - m.y, t->pos.x - m.x);
            if (b.lzCd[i] > 0) continue;
            fireWeapon(b, *t, w, &m);
            b.lzCd[i] = w.cooldown; b.lzFired = time;
        }
    }
    if (bunkerHas(b, BU_MEDIC)) {   // the field hospital: every half second, everything of its army and its allies in reach gets some health back, fighting or not
        const u32 PULSE = (u32)(0.5f * SIM_HZ);
        if ((tick + b.gen) % PULSE == 0) {
            const float R = BUNKER_MED_RADIUS * TILE, dt = PULSE * SIM_DT;
            forEachNear(b.pos, R + 2 * TILE, [&](Entity& t) {
                if (&t == &b || t.kind == EK_RESOURCE || t.owner < 0 || t.hp >= t.maxHp || t.carrier.valid() || (t.owner != b.owner && enemies(b.owner, t.owner))) return;
                if (t.isBuilding() && (!t.constructed || invulnerable(t))) return;
                if (dist(t.pos, b.pos) > R + t.radius() * 0.5f) return;
                t.hp = std::min(t.maxHp, t.hp + t.maxHp * (t.isBuilding() ? BUNKER_MED_STRUCT : BUNKER_MED_UNIT) * dt);
                if (((tick / PULSE) + (u32)(&t - &ents[0])) % 3 == 0) fx.push_back({FX_SPARK, t.pos + Vec2(rng.f(-6, 6), rng.f(-8, 2)), Vec2(), 0, 0.5f, rgb(120, 255, 170), 4});
            });
            if (((tick / PULSE) + b.gen) % 6 == 0) fx.push_back({FX_RING, b.pos, b.pos, 0, 1.3f, rgb(120, 255, 170), R * 0.9f});
        }
    }
}

// The laser battery's point defence: a rocket, shell or bomb of an enemy coming down within BUNKER_LASER_CIWS_RANGE of the bunker on anything of
// its side is burned out of the air (one shot per BUNKER_LASER_CIWS_GAP per bunker). True if it was shot down.
bool Sim::bunkerLaserIntercept(Projectile& p) {
    if (!anyBunkerLasers) return false;
    Entity* tgt = get(p.target);
    bool hit = false, tried = false;
    forEachNear(p.pos, (BUNKER_LASER_CIWS_RANGE + 2.0f) * TILE, [&](Entity& b) {
        if (tried || !b.isBuilding() || !b.constructed || b.bt().role != BR_BUNKER || !bunkerHas(b, BU_LASERS) || b.disabledUntil > time || time < b.ciwsAt) return;
        if (!enemies(b.owner, p.owner) || (tgt && enemies(b.owner, tgt->owner)) || dist(p.pos, b.pos) > BUNKER_LASER_CIWS_RANGE * TILE + b.radius()) return;
        tried = true;
        b.ciwsAt = time + BUNKER_LASER_CIWS_GAP;
        int i = rng.range(0, 1);
        Vec2 m = bunkerLaserPos(b, i);
        b.lzAng[i] = std::atan2(p.pos.y - m.y, p.pos.x - m.x); b.lzFired = time;
        Color col = WEAPONS[W_BUNKER_SKYLASER].color;
        fx.push_back({FX_BEAM, m, p.pos, 0, 0.12f, col, 2.2f});
        fx.push_back({FX_FLASH, m, m, 0, 0.10f, col, 8.0f});
        if (rng.f() < BUNKER_LASER_CIWS_CHANCE) {
            fx.push_back({FX_EXPLODE, p.pos, p.pos, 0, 0.4f, rgb(255, 224, 150), 20.0f});
            fx.push_back({FX_RING, p.pos, p.pos, 0, 0.35f, col, 26.0f});
            emit(EV_SOUND, b.owner, SND_EXPLODE_S, p.pos);
            hit = true;
        }
    });
    return hit;
}
