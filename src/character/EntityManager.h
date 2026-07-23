#pragma once
#include "CharacterEntity.h"
#include "CharacterRenderer.h"
#include <vector>
#include <string>
#include <glm/glm.hpp>

class EntityManager {
public:
    // Add an entity; returns pointer to it (owned by this manager).
    CharacterEntity* spawn(const std::string& id, const std::string& name,
                           glm::vec3 pos, bool isPlayer = false);

    // Remove entity by id.
    void remove(const std::string& id);

    CharacterEntity*       find(const std::string& id);
    const CharacterEntity* find(const std::string& id) const;

    std::vector<CharacterEntity>& all() { return entities; }

    // Advance all animation states.
    void update(float dt);

    // Persist / restore the entity list alongside character data.
    // File format: binary, each record is id+name+Character+pos+facingY.
    bool save(const std::string& path) const;
    bool load(const std::string& path);

    // Kept for panel call sites; entities render via SkinnedModel now, so
    // there is no per-entity mesh to rebuild.
    void markDirty(const std::string&) {}

    // Remove all entities.
    void clear();

private:
    std::vector<CharacterEntity> entities;
};
