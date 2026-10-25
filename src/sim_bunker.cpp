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

// What one Para Backup carries: BUNKER_PARA_TROOPS soldiers, riflemen, anti-air troopers and snipers above all, then the heavier kinds, each one only
// if the army has unlocked it (a kind it has not falls back to the plain rifleman). Never an infiltrator: they do not jump with the line troops.
std::vector<int> Sim::bunkerParaManifest(int player) const {
    std::vector<int> load;
    if (player < 0 || player >= numPlayers) return load;
    bool cy = players[player].faction == F_CYBER;
    const int rifle = cy ? U_C_INF1 : U_K_INF1, aa = cy ? U_C_INF2 : U_K_INF2, heavy = cy ? U_C_INF3 : U_K_INF3;
    const int elite = cy ? U_C_ELITE : U_K_ELITE, sniper = cy ? U_C_SNIPER : U_K_SNIPER;
    // the stick in the order the soldiers jump (mixed, so every part of the drop zone gets some of each); 35 places
    static const int PATTERN[7] = { 0, 1, 2, 0, 1, 2, 3 };   // 0 rifle, 1 anti-air, 2 sniper, 3 heavy / elite in turn
    int extra = 0;
    for (int k = 0; k < BUNKER_PARA_TROOPS; k++) {
        int slot = PATTERN[k % 7], t = rifle;
        if (slot == 1) t = unitAvailable(player, aa) ? aa : rifle;
        else if (slot == 2) t = unitAvailable(player, sniper) ? sniper : (unitAvailable(player, aa) && (k & 1) ? aa : rifle);
        else if (slot == 3) { int want = (extra++ & 1) ? elite : heavy; t = unitAvailable(player, want) ? want : (unitAvailable(player, heavy) ? heavy : aa); if (!unitAvailable(player, t)) t = rifle; }
        load.push_back(t);
    }
    return load;
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
