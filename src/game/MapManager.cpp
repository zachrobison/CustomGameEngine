#include "MapManager.h"
#include "../voxel/World.h"
#include "../character/EntityManager.h"
#include "../ui/ScenePanel.h"
#include "../platform/PlayerProfile.h"
#include <filesystem>

static void mkdirp(const std::string& path) {
    std::error_code ec;
    std::filesystem::create_directories(path, ec);   // portable, recursive
}

std::string MapManager::levelDir(const std::string& id) const {
    return PlayerProfile::get().saveDir() + "/maps/" + id;
}

std::vector<MapManager::LevelInfo> MapManager::listLevels() const {
    std::vector<LevelInfo> out;
    std::string mapsPath = PlayerProfile::get().saveDir() + "/maps";
    std::error_code ec;
    for (const auto& e : std::filesystem::directory_iterator(mapsPath, ec)) {
        if (!e.is_directory()) continue;
        std::string name = e.path().filename().string();
        out.push_back({name, name});
    }
    return out;
}

bool MapManager::saveLevel(const std::string& id, const World& world,
                           const EntityManager& entities, const ScenePanel& scene) const {
    mkdirp(PlayerProfile::get().saveDir() + "/maps");
    std::string dir = levelDir(id);
    mkdirp(dir);
    bool ok = world.save(dir + "/terrain.vox");
    ok = entities.save(dir + "/entities.bin") && ok;
    ok = scene.saveProject(dir + "/items.json") && ok;
    return ok;
}

bool MapManager::loadLevel(const std::string& id, World& world,
                           EntityManager& entities, ScenePanel& scene) const {
    std::string dir = levelDir(id);
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return false;

    world.load(dir + "/terrain.vox");
    entities.load(dir + "/entities.bin");
    scene.loadProject(dir + "/items.json");
    return true;
}

bool MapManager::switchLevel(const std::string& id, World& world,
                             EntityManager& entities, ScenePanel& scene) {
    if (id == current) {
        // Replaying the current level: reload pristine state from disk so
        // dead enemies and opened doors come back (nothing is saved).
        world.chunks.clear();
        entities.clear();
        return loadLevel(id, world, entities, scene);
    }

    // Runtime state (kills, opened doors) must never overwrite the authored
    // level files — only editor sessions persist on switch.
    if (persistRuntime) saveLevel(current, world, entities, scene);

    world.chunks.clear();
    entities.clear();

    bool existed = loadLevel(id, world, entities, scene);
    if (!existed) {
        // Brand new level: start from an empty world and the default node set.
        scene.addDefaultNodes({});
    }
    current = id;
    return true;
}
