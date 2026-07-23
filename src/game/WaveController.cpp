#include "WaveController.h"
#include "Turrets.h"
#include "../character/EntityManager.h"
#include "../voxel/World.h"
#include "../voxel/Chunk.h"
#include "../platform/Audio.h"
#include "../vendor/json.hpp"
#include <fstream>
#include <cstdio>
#include <queue>

using json = nlohmann::json;

// Horde entities are tagged by id prefix so other systems can tell them
// apart from authored NPCs (main's ambient chase AI skips them).
static const char* HORDE_PREFIX = "horde_";

static bool isHorde(const std::string& id) {
    return id.rfind(HORDE_PREFIX, 0) == 0;
}

bool WaveController::load(const std::string& levelDir) {
    waves.clear(); pads.clear();
    isActive = false;
    phase    = Phase::Idle;
    waveNumber = 0; enemiesLeft = 0;
    victory = defeat = false;

    std::ifstream f(levelDir + "/waves.json");
    if (!f) return false;
    try {
        json j; f >> j;
        auto bp = j.value("basePos", std::vector<float>{0, 21, 0});
        basePos       = {bp[0], bp[1], bp[2]};
        baseMaxHealth = j.value("baseHealth", 200.f);
        baseHealth    = baseMaxHealth;
        baseRadius    = j.value("baseRadius", 4.f);
        breather      = j.value("breather", 6.f);
        startTurrets  = j.value("startTurrets", 1);
        for (auto& p : j["spawnPads"])
            pads.push_back({p[0].get<float>(), p[1].get<float>(), p[2].get<float>()});
        endless = j.value("endless", false);
        for (auto& w : j["waves"]) {
            WaveDef d;
            d.count  = w.value("count", 4);
            d.hp     = w.value("hp", 60.f);
            d.speed  = w.value("speed", 3.f);
            d.damage = w.value("damage", 8.f);
            waves.push_back(d);
        }
    } catch (...) {
        fprintf(stderr, "WaveController: bad waves.json in %s\n", levelDir.c_str());
        return false;
    }
    if (waves.empty() || pads.empty()) return false;
    waveCount = (int)waves.size();
    isActive  = true;
    return true;
}

void WaveController::clearHorde(EntityManager& entities) {
    std::vector<std::string> ids;
    for (auto& e : entities.all())
        if (isHorde(e.id)) ids.push_back(e.id);
    for (auto& id : ids) entities.remove(id);
}

void WaveController::reset(EntityManager& entities) {
    if (!isActive) return;
    clearHorde(entities);
    chewTimers.clear();
    waveNumber  = 0;
    baseHealth  = baseMaxHealth;
    victory     = defeat = false;
    phase       = Phase::Breather;
    countdown   = 3.f;              // short lead-in before wave 1
    enemiesLeft   = 0;
    pendingSpawns = 0;
    beams.clear();
    Turrets::get().maxTurrets = startTurrets;
}

void WaveController::spawnWave(EntityManager& entities) {
    // Authored waves, then endless scaling: count grows ~28%/wave (hits the
    // hundreds around wave 15), hp/speed/damage creep more gently.
    if (waveNumber <= (int)waves.size()) {
        curWave = waves[waveNumber - 1];
    } else {
        float over = (float)(waveNumber - (int)waves.size());
        curWave = waves.back();
        curWave.count  = std::min((int)(curWave.count * std::pow(1.28f, over)), 400);
        curWave.hp    *= std::pow(1.08f, over);
        curWave.speed  = std::min(curWave.speed * std::pow(1.02f, over), 7.f);
        curWave.damage*= std::pow(1.05f, over);
    }
    // Trickle the spawns in (waves of 100s can't all appear in one frame)
    pendingSpawns = curWave.count;
    spawnTick     = 0.f;
    enemiesLeft   = curWave.count;
    chewTimers.clear();
    fprintf(stderr, "WaveController: wave %d — %d enemies incoming\n",
            waveNumber, curWave.count);
    Audio::get().play("wave", 0.7f);
}

void WaveController::buildFlowField(World& world) {
    const int N = GRID_N;
    flowDist.assign(N * N, 1e9f);

    // Cell traversal cost scales with how much material must be chewed:
    // thin walls are cheap shortcuts, thick/tall ones are worth walking
    // around. Counts solid voxels in the body column at the cell centre.
    auto cellCost = [&](int cx, int cz) {
        float wx = gridOrigin + (cx + 0.5f) * CELL;
        float wz = gridOrigin + (cz + 0.5f) * CELL;
        int vx = (int)std::floor(wx / VOXEL_SIZE);
        int vz = (int)std::floor(wz / VOXEL_SIZE);
        int vy0 = (int)std::floor(20.f / VOXEL_SIZE);
        int bodyVox = (int)(1.8f / VOXEL_SIZE);
        int solid = 0;
        for (int dy = 3; dy < bodyVox; dy++)   // ignore ankle-height bumps
            if (world.getVoxelWorld(vx, vy0 + dy, vz)) solid++;
        return 1.f + (float)solid * 4.f;   // full-height wall ≈ cost 21
    };

    int bx = (int)((basePos.x - gridOrigin) / CELL);
    int bz = (int)((basePos.z - gridOrigin) / CELL);
    if (bx < 0 || bx >= N || bz < 0 || bz >= N) return;

    using QE = std::pair<float, int>;   // (dist, index)
    std::priority_queue<QE, std::vector<QE>, std::greater<QE>> q;
    flowDist[bz * N + bx] = 0.f;
    q.push({0.f, bz * N + bx});

    static const int DX[8] = {1,-1,0,0, 1,1,-1,-1};
    static const int DZ[8] = {0,0,1,-1, 1,-1,1,-1};
    static const float DW[8] = {1,1,1,1, 1.41421f,1.41421f,1.41421f,1.41421f};

    while (!q.empty()) {
        auto [d, idx] = q.top(); q.pop();
        if (d > flowDist[idx]) continue;
        int cx = idx % N, cz = idx / N;
        for (int k = 0; k < 8; k++) {
            int nx = cx + DX[k], nz = cz + DZ[k];
            if (nx < 0 || nx >= N || nz < 0 || nz >= N) continue;
            float nd = d + DW[k] * cellCost(nx, nz);
            int   ni = nz * N + nx;
            if (nd < flowDist[ni]) {
                flowDist[ni] = nd;
                q.push({nd, ni});
            }
        }
    }
}

bool WaveController::flowDirAt(glm::vec3 pos, glm::vec2& outDir) const {
    if (flowDist.empty()) return false;
    const int N = GRID_N;
    int cx = (int)((pos.x - gridOrigin) / CELL);
    int cz = (int)((pos.z - gridOrigin) / CELL);
    if (cx < 1 || cx >= N - 1 || cz < 1 || cz >= N - 1) return false;
    if (flowDist[cz * N + cx] >= 1e9f) return false;

    // Step toward the lowest-distance neighbor cell
    float best = flowDist[cz * N + cx];
    int   bi = -1;
    static const int DX[8] = {1,-1,0,0, 1,1,-1,-1};
    static const int DZ[8] = {0,0,1,-1, 1,-1,1,-1};
    for (int k = 0; k < 8; k++) {
        float d = flowDist[(cz + DZ[k]) * N + (cx + DX[k])];
        if (d < best) { best = d; bi = k; }
    }
    if (bi < 0) return false;
    float tx = gridOrigin + (cx + DX[bi] + 0.5f) * CELL;
    float tz = gridOrigin + (cz + DZ[bi] + 0.5f) * CELL;
    glm::vec2 dir = {tx - pos.x, tz - pos.z};
    float len = glm::length(dir);
    if (len < 0.001f) return false;
    outDir = dir / len;
    return true;
}

void WaveController::update(float dt, EntityManager& entities, World& world,
                            bool voxelMode, glm::vec3 playerFeet, float* playerHealth) {
    if (!isActive || phase == Phase::Idle || phase == Phase::Over) return;

    const WaveDef& w = curWave;

    // ── Trickle spawner: batches every 0.2 s until the wave quota is out ──
    if (phase == Phase::Fighting && pendingSpawns > 0) {
        spawnTick -= dt;
        if (spawnTick <= 0.f) {
            int batch = std::min(pendingSpawns, 6);
            for (int i = 0; i < batch; i++) {
                int n = spawnedCtr++;
                glm::vec3 pad = pads[n % (int)pads.size()];
                float ang = (float)(n * 137 % 360) * 3.14159f / 180.f;
                float rad = 0.5f + (float)(n * 29 % 30) / 10.f;   // ring 0.5–3.5
                glm::vec3 pos = pad + glm::vec3(std::cos(ang) * rad, 0,
                                                std::sin(ang) * rad);
                bool shooter = (n % shooterEvery) == shooterEvery - 1;
                char id[48];
                snprintf(id, sizeof(id), "%s%s_%d", HORDE_PREFIX,
                         shooter ? "shooter" : "grunt", n);
                CharacterEntity* e = entities.spawn(id, shooter ? "Shooter" : "Horde", pos);
                if (e) e->health = e->maxHealth = w.hp;
                pendingSpawns--;
            }
            spawnTick = 0.2f;
        }
    }

    // ── Shooter tracers fade ──────────────────────────────────────────────
    for (int i = (int)beams.size() - 1; i >= 0; i--) {
        beams[i].ttl -= dt;
        if (beams[i].ttl <= 0.f) beams.erase(beams.begin() + i);
    }

    // Entity solidity check (same AABB the player uses, eye 1.65 above feet)
    auto solidAt = [&](glm::vec3 feet) {
        return voxelMode && world.overlapsVoxel(feet + glm::vec3(0, 1.65f, 0));
    };

    // Refresh the path field periodically (world changes as walls go up)
    if (voxelMode) {
        flowRebuild -= dt;
        if (flowRebuild <= 0.f) {
            buildFlowField(world);
            flowRebuild = 0.7f;
        }
    }

    // ── Horde AI: head for the nearer of player/base, contact damage ──────
    int alive = 0;
    for (auto& e : entities.all()) {
        if (!isHorde(e.id) || e.dead) continue;
        alive++;

        glm::vec2 toP = {playerFeet.x - e.position.x, playerFeet.z - e.position.z};
        glm::vec2 toB = {basePos.x - e.position.x,    basePos.z - e.position.z};
        float dP = glm::length(toP), dB = glm::length(toB);

        // Shooters: keep distance and fire visible tracers at the target
        bool isShooter = e.name == "Shooter";
        if (isShooter && (dP < 11.f || dB < 11.f)) {
            bool atPlayer = dP < dB;
            float& t = chewTimers[e.id];       // reuse map as fire cadence
            t -= dt;
            glm::vec3 tgt = atPlayer ? playerFeet + glm::vec3(0, 1.4f, 0)
                                     : basePos + glm::vec3(0, 4.f, 0);
            e.facingY = std::atan2(tgt.x - e.position.x, tgt.z - e.position.z);
            if (t <= 0.f) {
                beams.push_back({e.position + glm::vec3(0, 1.5f, 0), tgt, 0.12f});
                if (atPlayer) { if (playerHealth) *playerHealth -= w.damage * 0.6f; }
                else          baseHealth -= w.damage * 0.6f;
                Audio::get().play("laser", 0.3f, 0.7f);
                t = 1.6f;
            }
            continue;   // shooters don't advance while in range
        }

        // Player is only worth chasing when close; otherwise siege the base
        bool chasePlayer = dP < 10.f && dP < dB;
        glm::vec2 to   = chasePlayer ? toP : toB;
        float     dist = chasePlayer ? dP : dB;
        float     stop = chasePlayer ? 1.2f : baseRadius;

        if (dist > stop) {
            glm::vec2 dir = to / dist;
            // Base-seekers follow the flow field (routes around walls);
            // player-chasers stay direct.
            glm::vec2 fdir;
            if (!chasePlayer && voxelMode && flowDirAt(e.position, fdir))
                dir = fdir;
            glm::vec3 np  = e.position + glm::vec3(dir.x, 0, dir.y) * w.speed * dt;

            bool stepped = false;
            if (!solidAt(np)) {
                e.position = np;
                stepped = true;
            } else {
                // Step up over low bumps: up to 3 voxels (~knee height) so
                // spray residue and pile edges don't confuse them
                for (int k = 1; k <= 3 && !stepped; k++) {
                    glm::vec3 up = np + glm::vec3(0, k * VOXEL_SIZE, 0);
                    if (!solidAt(up)) {
                        e.position = up;
                        stepped = true;
                    }
                }
            }
            if (!stepped) {
                // Wall: chew through the blocking voxels on a timer
                float& t = chewTimers[e.id];
                t -= dt;
                if (t <= 0.f) {
                    glm::vec3 ahead = (e.position + glm::vec3(dir.x, 0, dir.y) * 0.8f) / VOXEL_SIZE;
                    int bx = (int)std::floor(ahead.x);
                    int bz = (int)std::floor(ahead.z);
                    int by = (int)std::floor(e.position.y / VOXEL_SIZE);
                    // Bite a body-sized chunk out of the wall (voxels are
                    // small — a bite is ~0.75 wide x 2 tall x 0.75 deep)
                    int bodyVox = (int)(1.9f / VOXEL_SIZE);
                    for (int dx = -1; dx <= 1; dx++)
                        for (int dz = -1; dz <= 1; dz++)
                            for (int dy = 0; dy < bodyVox; dy++)
                                if (world.getVoxelWorld(bx+dx, by+dy, bz+dz))
                                    world.setVoxelWorld(bx+dx, by+dy, bz+dz, 0);
                    Audio::get().play("crash", 0.4f, 0.8f);
                    t = 0.35f;                             // seconds per bite
                }
            }
            e.facingY = std::atan2(dir.x, dir.y);

            // Gravity: settle onto the surface below (fall into erased pits)
            if (voxelMode) {
                while (solidAt(e.position))                       // popped into a block
                    e.position.y += 1.f;
                glm::vec3 below = e.position - glm::vec3(0, 0.1f, 0);
                if (!solidAt(below) && e.position.y > 1.f)
                    e.position.y -= std::min(8.f * dt, 1.f);
            }
        } else if (chasePlayer) {
            if (playerHealth) *playerHealth -= w.damage * dt;
        } else {
            baseHealth -= w.damage * dt;
        }
    }
    if (phase == Phase::Fighting) enemiesLeft = alive + pendingSpawns;

    // ── Base destroyed ────────────────────────────────────────────────────
    if (baseHealth <= 0.f && !defeat) {
        baseHealth = 0.f;
        defeat     = true;
        phase      = Phase::Over;
        clearHorde(entities);
        Audio::get().play("death", 0.9f);
        return;
    }

    // ── Wave state machine ────────────────────────────────────────────────
    if (phase == Phase::Breather) {
        countdown -= dt;
        if (countdown <= 0.f) {
            waveNumber++;
            phase = Phase::Fighting;
            spawnWave(entities);
        }
    } else if (phase == Phase::Fighting && alive == 0 && pendingSpawns == 0) {
        if (!endless && waveNumber >= waveCount) {
            victory = true;
            phase   = Phase::Over;
            Audio::get().play("win", 0.9f);
        } else {
            phase     = Phase::Breather;
            countdown = breather;
            Turrets::get().maxTurrets++;   // wave cleared: +1 turret slot
            Audio::get().play("kill", 0.5f);
            Audio::get().play("switch", 0.6f, 1.5f);
        }
    }
}
