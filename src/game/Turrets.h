#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class EntityManager;
class Props;

// Placeable auto-firing turrets (Horde Defense). Placed via the Turret
// Placer weapon; each turret hitscans the nearest hostile entity in range.
// Visuals/collision ride on an internal Props instance.
class Turrets {
public:
    static Turrets& get();

    int   maxTurrets = 4;
    float range      = 12.f;
    float damage     = 12.f;
    float fireRate   = 0.7f;   // seconds between shots per turret

    // Returns false when the cap is reached. pos = turret base (on ground).
    bool place(glm::vec3 pos);
    void clear();
    int  count() const { return (int)turrets.size(); }

    void update(float dt, EntityManager& entities);
    void render(const glm::mat4& VP, glm::vec3 sunDir,
                float fogDensity, glm::vec3 camPos);

private:
    Turrets() = default;
    struct Turret { glm::vec3 pos; float cooldown = 0.f; };
    std::vector<Turret> turrets;
    Props* visuals = nullptr;   // lazily created (needs GL context)
    void rebuildVisuals();
};
