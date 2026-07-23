#include "BossController.h"
#include "Bullets.h"
#include "../character/EntityManager.h"
#include "../renderer/Props.h"
#include "../platform/Audio.h"
#include <cmath>

// Tunables — committed timings are the whole point (readable, punishable).
static constexpr float APPROACH_SPD  = 3.2f;
static constexpr float MELEE_RANGE   = 3.4f;   // swipe when this close
static constexpr float FAR_RANGE     = 18.f;   // jump only beyond this
static constexpr float MELEE_WINDUP  = 0.55f;
static constexpr float MELEE_REACH   = 4.4f;
static constexpr float MELEE_ARC     = 0.45f;  // dot() threshold (~63°)
static constexpr float MELEE_DMG     = 30.f;
static constexpr float JUMP_WINDUP   = 0.75f;  // crouch telegraph before leap
static constexpr float JUMP_TIME     = 0.60f;  // airborne arc
static constexpr float JUMP_HEIGHT   = 3.0f;
static constexpr float SLAM_RADIUS   = 3.6f;
static constexpr float SLAM_DMG      = 38.f;
static constexpr float SPRAY_TIME    = 1.3f;   // burst length (p2: 1.8)
static constexpr float SPRAY_STEP    = 0.16f;  // seconds between rounds
static constexpr float RECOVER_TIME  = 0.85f;  // the punish window

float BossController::telegraph(const std::string& id) const {
    auto it = states.find(id);
    if (it == states.end()) return 0.f;
    const S& s = it->second;
    if (s.phase == Phase::MeleeWindup) return std::min(1.f, s.t / MELEE_WINDUP);
    if (s.phase == Phase::JumpWindup)  return std::min(1.f, s.t / JUMP_WINDUP);
    if (s.phase == Phase::Jump || s.phase == Phase::MeleeStrike) return 1.f;
    return 0.f;
}

bool BossController::update(float dt, EntityManager& entities, Props* props,
                            glm::vec3 playerFeet, float* playerHealth) {
    bool anyBoss = false;

    for (auto& e : entities.all()) {
        if (e.isPlayer || e.dead || e.name != "Boss") continue;
        anyBoss = true;
        S& s = states[e.id];

        bool  phase2 = e.health < e.maxHealth * 0.5f;
        glm::vec2 to = {playerFeet.x - e.position.x, playerFeet.z - e.position.z};
        float dist   = glm::length(to);
        glm::vec2 dir = dist > 0.01f ? to / dist : glm::vec2(0, 1);
        // Track the player except while airborne/committed to a swing
        if (s.phase != Phase::Jump && s.phase != Phase::MeleeStrike)
            e.facingY = std::atan2(dir.x, dir.y);
        s.t += dt;

        switch (s.phase) {
            case Phase::Approach: {
                // Advance with a strafe weave; slide along walls when blocked
                if (dist > MELEE_RANGE * 0.8f) {
                    float weave = std::sin(s.t * 2.2f) * 0.35f;
                    glm::vec2 perp = {-dir.y, dir.x};
                    glm::vec2 mv = glm::normalize(dir + perp * weave);
                    glm::vec3 step = glm::vec3(mv.x, 0, mv.y) * APPROACH_SPD * dt;
                    auto freeAt = [&](glm::vec3 p) {
                        return !props || !props->overlapsPlayer(p + glm::vec3(0,1.65f,0));
                    };
                    glm::vec3 np = e.position + step;
                    if      (freeAt(np)) e.position = np;
                    else if (freeAt(e.position + glm::vec3(step.x,0,0)))
                        e.position += glm::vec3(step.x, 0, 0);
                    else if (freeAt(e.position + glm::vec3(0,0,step.z)))
                        e.position += glm::vec3(0, 0, step.z);
                }
                s.actionCd -= dt;
                if (s.actionCd <= 0.f) {
                    if (dist < MELEE_RANGE) {           // close: swipe
                        s.phase = Phase::MeleeWindup; s.t = 0.f; s.struck = false;
                        Audio::get().play("melee_swing", 0.75f, 0.45f);
                    } else if (dist > FAR_RANGE) {      // really far: leap in
                        s.phase = Phase::JumpWindup; s.t = 0.f;
                        s.baseY = e.position.y;
                        Audio::get().play("melee_swing", 0.7f, 0.35f);
                    } else {                            // mid: spray often
                        s.phase = Phase::Spray; s.t = 0.f; s.sprayCd = 0.f;
                    }
                }
                break;
            }
            case Phase::MeleeWindup: {
                if (s.t >= MELEE_WINDUP) { s.phase = Phase::MeleeStrike; s.t = 0.f; }
                break;
            }
            case Phase::MeleeStrike: {
                // One committed swipe frame window (~0.25s), single hit
                if (!s.struck && dist < MELEE_REACH) {
                    glm::vec2 face = {std::sin(e.facingY), std::cos(e.facingY)};
                    if (glm::dot(dir, face) > MELEE_ARC && playerHealth) {
                        *playerHealth -= MELEE_DMG;
                        Audio::get().play("hurt", 0.8f);
                        s.struck = true;
                    }
                }
                if (s.t >= 0.25f) { s.phase = Phase::Recover; s.t = 0.f; }
                break;
            }
            case Phase::JumpWindup: {
                // Crouch + glow, then commit toward where the player IS now
                if (s.t >= JUMP_WINDUP) {
                    s.phase = Phase::Jump; s.t = 0.f; s.struck = false;
                    s.jumpFrom = e.position;
                    s.jumpTo   = glm::vec3(playerFeet.x, s.baseY, playerFeet.z);
                    Audio::get().play("dash", 0.6f, 0.7f);
                }
                break;
            }
            case Phase::Jump: {
                // Parabolic leap; slam AoE on landing
                float k = std::min(s.t / JUMP_TIME, 1.f);
                glm::vec3 flat = glm::mix(s.jumpFrom, s.jumpTo, k);
                flat.y = s.baseY + std::sin(k * 3.14159265f) * JUMP_HEIGHT;
                e.position = flat;
                if (k >= 1.f && !s.struck) {
                    s.struck = true;
                    e.position.y = s.baseY;
                    Audio::get().play("explosion", 0.6f);
                    Audio::get().play("crash", 0.5f);
                    glm::vec2 land = {playerFeet.x - e.position.x,
                                      playerFeet.z - e.position.z};
                    if (glm::length(land) < SLAM_RADIUS && playerHealth)
                        *playerHealth -= SLAM_DMG;
                    s.phase = Phase::Recover; s.t = 0.f;
                }
                break;
            }
            case Phase::Spray: {
                // Burst of parryable rounds along the animated gun bone —
                // the firing sweep arcs the spray. Falls back to entity aim.
                float len = phase2 ? SPRAY_TIME * 1.4f : SPRAY_TIME;
                s.sprayCd -= dt;
                if (s.sprayCd <= 0.f) {
                    s.sprayCd = phase2 ? SPRAY_STEP * 0.75f : SPRAY_STEP;
                    glm::vec3 origin = e.position + glm::vec3(0, 1.6f, 0);
                    glm::vec3 aim    = {dir.x, 0.f, dir.y};
                    if (muzzle) muzzle(e.id, origin, aim);
                    Bullets::get().fire(origin, origin + aim * 6.f, e.id);
                }
                if (s.t >= len) { s.phase = Phase::Recover; s.t = 0.f; }
                break;
            }
            case Phase::Recover: {
                // The punish window — boss stands vulnerable
                if (s.t >= (phase2 ? RECOVER_TIME * 0.6f : RECOVER_TIME)) {
                    s.phase = Phase::Approach; s.t = 0.f;
                    s.actionCd = phase2 ? 0.45f : 0.8f;   // sprays come often
                }
                break;
            }
        }
    }

    if (!anyBoss) states.clear();
    return anyBoss;
}
