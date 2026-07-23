#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class EntityManager;
class LineRenderer;
class GltfModel;

// The Robot Souls delayed bullet (doc: telegraph → hold → release). The round
// crawls out of the muzzle, HANGS frozen mid-air (the parry window — slice it
// with a melee swing to send it back at the shooter), then rips forward.
class Bullets {
public:
    static Bullets& get();

    float slowSpeed  = 5.f,  slowTime = 0.4f;
    float hangTime   = 0.9f;
    float fastSpeed  = 26.f;
    float damage     = 22.f;

    void fire(glm::vec3 from, glm::vec3 target, const std::string& shooterId);

    // Melee swing hook: any HANGing round in reach flips friendly and flies
    // back along its path. Returns true if something was parried.
    bool trySlice(glm::vec3 swingerPos, glm::vec3 dir, float reach);

    void update(float dt, EntityManager& entities,
                glm::vec3 playerEye, float* playerHealth);
    void render(LineRenderer& lines, const glm::mat4& VP);
    // 3D bullet model at each round, oriented along flight, tinted by phase.
    void renderModels(GltfModel& model, const glm::mat4& VP, glm::vec3 sunDir,
                      float fog, glm::vec3 camPos);
    void clear() { shots.clear(); }

private:
    Bullets() = default;
    enum class Phase { Slow, Hang, Fast };
    struct Shot {
        glm::vec3   pos, dir;
        Phase       phase = Phase::Slow;
        float       t = 0.f, life = 0.f;
        bool        friendly = false;
        std::string shooterId;
    };
    std::vector<Shot> shots;
};
