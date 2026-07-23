#include "Turrets.h"
#include "../character/EntityManager.h"
#include "../renderer/Props.h"
#include "../platform/Audio.h"

Turrets& Turrets::get() {
    static Turrets inst;
    return inst;
}

void Turrets::rebuildVisuals() {
    if (!visuals) visuals = new Props();
    visuals->clear();
    for (auto& t : turrets) {
        glm::vec3 p = t.pos;
        visuals->addBox(p + glm::vec3(-0.22f, 0.f,  -0.22f),
                        p + glm::vec3( 0.22f, 1.0f,  0.22f), {0.30f, 0.32f, 0.42f});
        visuals->addBox(p + glm::vec3(-0.32f, 1.0f, -0.32f),
                        p + glm::vec3( 0.32f, 1.45f, 0.32f), {0.35f, 0.65f, 0.95f});
        visuals->addBox(p + glm::vec3(-0.08f, 1.1f, -0.45f),
                        p + glm::vec3( 0.08f, 1.3f,  0.45f), {0.8f, 0.85f, 0.95f}); // barrel
    }
    visuals->buildMesh();
}

bool Turrets::place(glm::vec3 pos) {
    if ((int)turrets.size() >= maxTurrets) {
        Audio::get().play("switch", 0.4f, 0.5f);   // denied click
        return false;
    }
    turrets.push_back({pos, 0.f});
    rebuildVisuals();
    Audio::get().play("switch", 0.6f, 1.3f);
    return true;
}

void Turrets::clear() {
    turrets.clear();
    if (visuals) rebuildVisuals();
}

void Turrets::update(float dt, EntityManager& entities) {
    for (auto& t : turrets) {
        t.cooldown -= dt;
        if (t.cooldown > 0.f) continue;

        // Nearest hostile in range (head height for line of sight feel)
        CharacterEntity* best = nullptr;
        float bestD = range;
        for (auto& e : entities.all()) {
            if (e.isPlayer || e.dead) continue;
            float d = glm::length(e.position - t.pos);
            if (d < bestD) { best = &e; bestD = d; }
        }
        if (!best) continue;

        best->health -= damage;
        Audio::get().play("laser2", 0.35f, 1.2f);
        if (best->health <= 0.f) {
            best->dead = true;
            std::string id = best->id;
            entities.remove(id);
            Audio::get().play("enemy_die", 0.6f);
            Audio::get().play("kill", 0.3f);
        }
        t.cooldown = fireRate;
    }
}

void Turrets::render(const glm::mat4& VP, glm::vec3 sunDir,
                     float fogDensity, glm::vec3 camPos) {
    if (visuals) visuals->render(VP, sunDir, fogDensity, camPos);
}
