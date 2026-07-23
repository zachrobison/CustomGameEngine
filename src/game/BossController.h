#pragma once
#include <glm/glm.hpp>
#include <functional>
#include <map>
#include <string>

class EntityManager;
class Props;
class World;

// Souls boss ("The Don"): three committed, readable behaviours by range —
//   close (< MELEE_RANGE): windup → melee swipe → punishable recover
//   mid   (≤ FAR_RANGE):   frequent spray bursts of parryable rounds fired
//                          along the animated gun bone (arcs with the sweep)
//   far   (> FAR_RANGE):   telegraphed jump-slam to close the gap
// Phase 2 under 50% HP: faster recovers, longer sprays.
class BossController {
public:
    // Muzzle provider (set from main): world-space gun position + aim
    // direction for an entity id, sampled from the animated gun bone.
    using MuzzleFn = std::function<bool(const std::string& entId,
                                        glm::vec3& origin, glm::vec3& dir)>;
    void setMuzzle(MuzzleFn fn) { muzzle = std::move(fn); }

    // Drives every entity named "Boss". Returns true while a boss is alive.
    bool update(float dt, EntityManager& entities, Props* props,
                glm::vec3 playerFeet, float* playerHealth);

    void reset() { states.clear(); }

    // Render hook: 0..1 windup progress for telegraphs (gold flash/crouch).
    float telegraph(const std::string& id) const;

private:
    enum class Phase { Approach, MeleeWindup, MeleeStrike,
                       JumpWindup, Jump, Spray, Recover };
    struct S {
        Phase phase = Phase::Approach;
        float t = 0.f;               // time in current phase
        float actionCd = 1.0f;       // gap before next committed action
        glm::vec3 jumpFrom{0.f}, jumpTo{0.f};
        float baseY   = 0.f;
        float sprayCd = 0.f;         // per-round timer inside a spray burst
        bool  struck  = false;
    };
    std::map<std::string, S> states;
    MuzzleFn muzzle;
};
