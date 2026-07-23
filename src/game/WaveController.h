#pragma once
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

class EntityManager;
class World;

// Horde Defense mode: activates when the current level dir has a waves.json.
// Spawns escalating waves of enemies from spawn pads; they attack whichever
// is closer — the player or the base tower. Base at 0 HP = defeat; clearing
// the last wave = victory.
//
// waves.json: { basePos, baseHealth, baseRadius, breather, spawnPads:[[x,y,z]],
//               waves:[{count,hp,speed,damage}] }
class WaveController {
public:
    // Loads maps/<dir>/waves.json. Returns false (mode inactive) if missing.
    bool load(const std::string& levelDir);
    bool active() const { return isActive; }

    // Call when play mode starts: wave 1, full base, clears old horde units.
    void reset(EntityManager& entities);

    // Player skips the breather (interact key): next wave spawns immediately.
    void startNextWaveNow() { if (phase == Phase::Breather) countdown = 0.f; }

    // Per-frame in play mode. Moves horde enemies (main's ambient AI skips
    // them), applies contact damage to player/base, advances wave state.
    // Voxel-aware AI: falls with gravity, climbs 1-block steps, and chews
    // through blocking voxels (player-built walls buy time, not immunity).
    void update(float dt, EntityManager& entities, World& world, bool voxelMode,
                glm::vec3 playerFeet, float* playerHealth);

    // HUD state
    int         waveNumber   = 0;      // 1-based; 0 = not started
    int         waveCount    = 0;      // authored waves; endless goes beyond
    int         enemiesLeft  = 0;
    float       baseHealth   = 0.f, baseMaxHealth = 0.f;
    float       countdown    = 0.f;    // >0 during breather
    bool        victory      = false, defeat = false;
    bool        endless      = false;
    glm::vec3   basePos{0.f};

    // Shooter tracers for main to draw: {from, to, ttl}
    struct Beam { glm::vec3 a, b; float ttl; };
    std::vector<Beam> beams;

private:
    struct WaveDef { int count = 4; float hp = 60.f, speed = 3.f, damage = 8.f; };
    std::vector<WaveDef>   waves;
    WaveDef  curWave;          // authored or endless-scaled
    int      pendingSpawns = 0;  // trickled in over time (100s of enemies)
    float    spawnTick     = 0.f;
    int      shooterEvery  = 4;  // every Nth spawn is a ranged shooter
    std::vector<glm::vec3> pads;
    float baseRadius = 4.f, breather = 6.f;
    int   startTurrets = 1;   // +1 slot unlocks after each cleared wave
    bool  isActive = false;
    int   spawnedCtr = 0;

    enum class Phase { Idle, Breather, Fighting, Over } phase = Phase::Idle;

    std::map<std::string, float> chewTimers;   // per-enemy wall-eating cadence

    // ── Flow-field pathfinding toward the base ────────────────────────────
    // Weighted Dijkstra from the base cell; built voxels cost ~15x open
    // ground, so enemies route around walls but chew through when boxed in.
    static constexpr int   GRID_N    = 128;
    static constexpr float CELL      = 0.5f;
    float                  gridOrigin = -32.f;      // grid covers ±32 world
    std::vector<float>     flowDist;                // GRID_N*GRID_N
    float                  flowRebuild = 0.f;

    void buildFlowField(World& world);
    // Direction to walk from `pos` toward the base; false = off-grid/no path.
    bool flowDirAt(glm::vec3 pos, glm::vec2& outDir) const;

    void spawnWave(EntityManager& entities);
    void clearHorde(EntityManager& entities);
};
