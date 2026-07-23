#include "MapManager.h"
#include "../voxel/World.h"
#include "../character/EntityManager.h"
#include "../ui/ScenePanel.h"
#include "../platform/PlayerProfile.h"
#include <sys/stat.h>
#include <dirent.h>

static void mkdirp(const std::string& path) {
    mkdir(path.c_str(), 0755);
}

std::string MapManager::levelDir(const std::string& id) const {
    return PlayerProfile::get().saveDir() + "/maps/" + id;
}

std::vector<MapManager::LevelInfo> MapManager::listLevels() const {
    std::vector<LevelInfo> out;
    std::string mapsPath = PlayerProfile::get().saveDir() + "/maps";
    DIR* d = opendir(mapsPath.c_str());
    if (!d) return out;
    while (dirent* e = readdir(d)) {
        std::string name = e->d_name;
        if (name == "." || name == "..") continue;
        if (e->d_type != DT_DIR) continue;
        out.push_back({name, name});
    }
    closedir(d);
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
    struct stat st;
    if (stat(dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) return false;

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
