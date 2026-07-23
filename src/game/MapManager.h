#pragma once
#include <string>
#include <vector>

class World;
class EntityManager;
class ScenePanel;

// Per-level save directories: <profile>/maps/<id>/{terrain.vox, entities.bin, items.json}.
// Lets a project grow beyond a single level without losing progress in the others —
// switchLevel() persists whatever is currently loaded before swapping in the next id.
class MapManager {
public:
    struct LevelInfo { std::string id, displayName; };

    // Scans <profile>/maps/ for level directories.
    std::vector<LevelInfo> listLevels() const;

    const std::string& currentId() const { return current; }

    // False during play mode: kills/opened doors are runtime state and must
    // not be written back into the authored level files on switch.
    bool persistRuntime = true;

    // Writes World/EntityManager/ScenePanel state into maps/<id>/.
    bool saveLevel(const std::string& id, const World& world,
                   const EntityManager& entities, const ScenePanel& scene) const;

    // Reads maps/<id>/ into World/EntityManager/ScenePanel.
    // Returns false (no changes made) if the level directory doesn't exist yet.
    bool loadLevel(const std::string& id, World& world,
                   EntityManager& entities, ScenePanel& scene) const;

    // Saves the current level, then loads `id` and makes it current.
    // If `id` has never been saved, World/EntityManager are cleared and
    // ScenePanel is reset to its default nodes so the new level starts fresh.
    bool switchLevel(const std::string& id, World& world,
                     EntityManager& entities, ScenePanel& scene);

private:
    std::string current = "default";
    std::string levelDir(const std::string& id) const;
};
