#include "Bullets.h"
#include "../character/EntityManager.h"
#include "../renderer/LineRenderer.h"
#include "../renderer/GltfModel.h"
#include "../platform/Audio.h"
#include <glm/gtc/matrix_transform.hpp>

Bullets& Bullets::get() {
    static Bullets inst;
    return inst;
}

void Bullets::fire(glm::vec3 from, glm::vec3 target, const std::string& shooterId) {
    Shot s;
    s.pos       = from;
    s.dir       = glm::normalize(target - from);
    s.shooterId = shooterId;
    shots.push_back(s);
    Audio::get().play("laser", 0.35f, 0.55f);   // low chug: the lineup
}

bool Bullets::trySlice(glm::vec3 swingerPos, glm::vec3 dir, float reach) {
    bool any = false;
    for (auto& s : shots) {
        if (s.phase != Phase::Hang || s.friendly) continue;
        glm::vec3 to = s.pos - swingerPos;
        if (glm::length(to) > reach) continue;
        if (glm::dot(glm::normalize(to), dir) < 0.1f) continue;
        // Sliced: send it back where it came from
        s.dir      = -s.dir;
        s.friendly = true;
        s.phase    = Phase::Fast;
        s.t        = 0.f;
        any = true;
    }
    if (any) Audio::get().play("melee_hit", 0.8f, 1.5f);   // the slice
    return any;
}

void Bullets::update(float dt, EntityManager& entities,
                     glm::vec3 playerEye, float* playerHealth) {
    for (int i = (int)shots.size() - 1; i >= 0; i--) {
        Shot& s = shots[i];
        s.t += dt; s.life += dt;

        float speed = 0.f;
        switch (s.phase) {
            case Phase::Slow:
                speed = slowSpeed;
                if (s.t >= slowTime) { s.phase = Phase::Hang; s.t = 0.f; }
                break;
            case Phase::Hang:
                if (s.t >= hangTime) {
                    s.phase = Phase::Fast; s.t = 0.f;
                    Audio::get().play("laser2", 0.4f, 0.8f);   // lets rip
                }
                break;
            case Phase::Fast:
                speed = fastSpeed;
                break;
        }
        s.pos += s.dir * speed * dt;

        bool dead = s.life > 6.f;
        if (!dead && s.friendly) {
            // Redirected rounds hurt whoever fired them (and any pig in the way)
            for (auto& e : entities.all()) {
                if (e.isPlayer || e.dead) continue;
                if (glm::length(e.position + glm::vec3(0, 1.2f, 0) - s.pos) < 0.9f) {
                    e.health -= damage * 2.5f;   // returned to sender, with interest
                    Audio::get().play("enemy_hurt", 0.6f);
                    if (e.health <= 0.f) {
                        e.dead = true;
                        std::string id = e.id;
                        entities.remove(id);
                        Audio::get().play("enemy_die", 0.7f);
                        Audio::get().play("kill", 0.35f);
                    }
                    dead = true;
                    break;
                }
            }
        } else if (!dead) {
            if (playerHealth && glm::length(playerEye - glm::vec3(0, 0.4f, 0) - s.pos) < 0.7f) {
                *playerHealth -= damage;
                Audio::get().play("hurt", 0.7f);
                dead = true;
            }
        }
        if (dead) shots.erase(shots.begin() + i);
    }
}

void Bullets::render(LineRenderer& lines, const glm::mat4& VP) {
    // Motion trail only (the round itself is the 3D model). Skip the box so
    // it doesn't clip through the mesh.
    for (auto& s : shots) {
        glm::vec3 col = s.friendly              ? glm::vec3(0.4f, 1.f, 0.6f)
                      : s.phase == Phase::Hang  ? glm::vec3(1.f, 0.95f, 0.4f)
                                                : glm::vec3(1.f, 0.45f, 0.2f);
        float len = s.phase == Phase::Fast ? 0.9f : 0.4f;
        lines.drawLine(s.pos, s.pos - s.dir * len, VP, {col.r, col.g, col.b, 0.4f});
    }
}

void Bullets::renderModels(GltfModel& model, const glm::mat4& VP, glm::vec3 sunDir,
                           float fog, glm::vec3 camPos) {
    if (!model.loaded()) return;
    const float SCALE = 0.16f;   // model is ~5.7u long → ~0.9u bullet
    for (auto& s : shots) {
        // Orient the model's long axis (+Y, nose) along the flight direction.
        glm::vec3 f = glm::length(s.dir) > 1e-4f ? glm::normalize(s.dir)
                                                 : glm::vec3(0, 0, 1);
        glm::vec3 up  = std::abs(f.y) < 0.98f ? glm::vec3(0, 1, 0) : glm::vec3(1, 0, 0);
        glm::vec3 rgt = glm::normalize(glm::cross(up, f));
        glm::vec3 nup = glm::cross(f, rgt);
        glm::mat4 R(1.f);
        R[0] = glm::vec4(rgt, 0.f);   // model +X
        R[1] = glm::vec4(f,   0.f);   // model +Y (nose) → flight dir
        R[2] = glm::vec4(nup, 0.f);   // model +Z
        // Spin the round about its axis so it reads as tumbling
        float spin = s.life * (s.phase == Phase::Fast ? 22.f : 3.f);
        glm::mat4 M = glm::translate(glm::mat4(1.f), s.pos) * R
                    * glm::rotate(glm::mat4(1.f), spin, glm::vec3(0, 1, 0))
                    * glm::scale(glm::mat4(1.f), glm::vec3(SCALE));
        // Hang = glow gold (parry window), fast = hot, friendly = green
        glm::vec3 tint = s.friendly             ? glm::vec3(0.5f, 1.8f, 0.9f)
                       : s.phase == Phase::Hang ? glm::vec3(2.0f, 1.7f, 0.6f)
                       : s.phase == Phase::Fast ? glm::vec3(1.6f, 1.1f, 0.9f)
                                                : glm::vec3(1.2f);
        model.renderMatrix(VP, M, sunDir, fog, camPos, tint);
    }
}
