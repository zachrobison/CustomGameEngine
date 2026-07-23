#include "EntityManager.h"
#include <glm/gtc/matrix_transform.hpp>
#include <fstream>
#include <cstring>
#include <algorithm>

CharacterEntity* EntityManager::spawn(const std::string& id,
                                      const std::string& name,
                                      glm::vec3 pos, bool isPlayer) {
    // Don't duplicate
    if (find(id)) return find(id);

    CharacterEntity e;
    e.id       = id;
    e.name     = name;
    e.position = pos;
    e.isPlayer = isPlayer;
    e.anim.play(&BuiltinAnims::idle());
    entities.push_back(std::move(e));
    return &entities.back();
}

void EntityManager::remove(const std::string& id) {
    for (int i = 0; i < (int)entities.size(); i++) {
        if (entities[i].id == id) {
            entities.erase(entities.begin() + i);
            return;
        }
    }
}

CharacterEntity* EntityManager::find(const std::string& id) {
    for (auto& e : entities)
        if (e.id == id) return &e;
    return nullptr;
}

const CharacterEntity* EntityManager::find(const std::string& id) const {
    for (auto& e : entities)
        if (e.id == id) return &e;
    return nullptr;
}

void EntityManager::clear() {
    entities.clear();
}

// ── Update / Render ───────────────────────────────────────────────────────

void EntityManager::update(float dt) {
    for (auto& e : entities) {
        e.anim.update(dt);
        if (e.hitFlash > 0.f) e.hitFlash = std::max(0.f, e.hitFlash - dt);
    }
}

// ── Save / Load ───────────────────────────────────────────────────────────

static void writeStr(std::ofstream& f, const std::string& s) {
    uint32_t len = (uint32_t)s.size();
    f.write((const char*)&len, 4);
    f.write(s.data(), len);
}

static std::string readStr(std::ifstream& f) {
    uint32_t len = 0;
    f.read((char*)&len, 4);
    std::string s(len, '\0');
    f.read(s.data(), len);
    return s;
}

static void writeVec3(std::ofstream& f, glm::vec3 v) {
    f.write((const char*)&v, 12);
}
static glm::vec3 readVec3(std::ifstream& f) {
    glm::vec3 v;
    f.read((char*)&v, 12);
    return v;
}

bool EntityManager::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t count = (uint32_t)entities.size();
    f.write((const char*)&count, 4);

    for (auto& e : entities) {
        writeStr(f, e.id);
        writeStr(f, e.name);
        uint8_t isPlayer = e.isPlayer ? 1 : 0;
        f.write((const char*)&isPlayer, 1);
        writeVec3(f, e.position);
        f.write((const char*)&e.facingY, 4);

        // Character scales
        for (int i = 0; i < (int)BodyPart::COUNT; i++)
            writeVec3(f, e.character.scales[i].v);

        // Clothing
        for (int i = 0; i < (int)ClothingSlot::COUNT; i++) {
            const ClothingConfig& c = e.character.clothing[i];
            f.write((const char*)&c.preset, 4);
            writeVec3(f, c.color);
        }
        writeVec3(f, e.character.skinColor);
    }
    return true;
}

bool EntityManager::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    uint32_t count = 0;
    f.read((char*)&count, 4);

    entities.clear();

    for (uint32_t n = 0; n < count; n++) {
        CharacterEntity e;
        e.id       = readStr(f);
        e.name     = readStr(f);
        uint8_t ip = 0; f.read((char*)&ip, 1); e.isPlayer = (ip != 0);
        e.position = readVec3(f);
        f.read((char*)&e.facingY, 4);

        for (int i = 0; i < (int)BodyPart::COUNT; i++)
            e.character.scales[i].v = readVec3(f);
        for (int i = 0; i < (int)ClothingSlot::COUNT; i++) {
            f.read((char*)&e.character.clothing[i].preset, 4);
            e.character.clothing[i].color = readVec3(f);
        }
        e.character.skinColor = readVec3(f);
        e.anim.play(&BuiltinAnims::idle());
        entities.push_back(std::move(e));
    }

    return !f.fail();
}
