#include "World.h"
#include "Noise.h"
#include "../renderer/Shader.h"
#include "../renderer/Camera.h"
#include "../Settings.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <cstdint>
#include <cstring>

int World::floorDiv(int a, int b) {
    return a / b - (a % b != 0 && (a ^ b) < 0);
}
glm::ivec3 World::toChunkPos(int wx, int wy, int wz) {
    return { floorDiv(wx,CHUNK_SIZE), floorDiv(wy,CHUNK_SIZE), floorDiv(wz,CHUNK_SIZE) };
}
glm::ivec3 World::toLocalPos(int wx, int wy, int wz) {
    glm::ivec3 cp = toChunkPos(wx,wy,wz);
    return { wx - cp.x*CHUNK_SIZE, wy - cp.y*CHUNK_SIZE, wz - cp.z*CHUNK_SIZE };
}

Chunk& World::getOrCreate(glm::ivec3 cp) {
    auto it = chunks.find(cp);
    if (it != chunks.end()) return *it->second;
    auto [ins,ok] = chunks.emplace(cp, std::make_unique<Chunk>(cp));
    return *ins->second;
}

int World::getTerrainHeight(int wx, int wz) const {
    float n = Noise::fractal(wx * 0.025f, wz * 0.025f, 4); // -1..1
    return (int)(n * 12.f) + 10;                            // range ~-2..22
}

void World::generateChunk(glm::ivec3 cp) {
    Chunk& c = getOrCreate(cp);
    int baseY = cp.y * CHUNK_SIZE;

    for (int x = 0; x < CHUNK_SIZE; x++) {
        for (int z = 0; z < CHUNK_SIZE; z++) {
            int wx  = cp.x * CHUNK_SIZE + x;
            int wz2 = cp.z * CHUNK_SIZE + z;
            int h   = getTerrainHeight(wx, wz2);

            for (int y = 0; y < CHUNK_SIZE; y++) {
                int wy = baseY + y;
                if (wy > h) { c.voxels[x][y][z] = 0; continue; }
                if      (wy == h)     c.voxels[x][y][z] = 2; // grass
                else if (wy >= h - 2) c.voxels[x][y][z] = 1; // dirt
                else                  c.voxels[x][y][z] = 3; // stone
            }
        }
    }
    c.dirty = true;
}

void World::streamChunks(glm::vec3 playerPos, int renderDist, bool procedural) {
    glm::vec3 vp = playerPos / VOXEL_SIZE;   // world → voxel space
    glm::ivec3 pc = toChunkPos((int)vp.x, (int)vp.y, (int)vp.z);
    int rd = renderDist;
    for (int cx = pc.x - rd; cx <= pc.x + rd; cx++) {
        for (int cz = pc.z - rd; cz <= pc.z + rd; cz++) {
            // Vertical: from 2 chunks below sea level to 2 chunks above max height
            for (int cy = -2; cy <= 2; cy++) {
                glm::ivec3 cp2 = {cx, cy, cz};
                if (chunks.find(cp2) == chunks.end()) {
                    if (procedural) generateChunk(cp2);
                    else            getOrCreate(cp2);   // empty: authored maps end in void
                }
            }
        }
    }
}

uint8_t World::getVoxelWorld(int wx, int wy, int wz) const {
    auto it = chunks.find(toChunkPos(wx,wy,wz));
    if (it == chunks.end()) return 0;
    glm::ivec3 lp = toLocalPos(wx,wy,wz);
    return it->second->get(lp.x, lp.y, lp.z);
}

void World::setVoxelWorld(int wx, int wy, int wz, uint8_t type) {
    glm::ivec3 cp = toChunkPos(wx,wy,wz);
    Chunk& c  = getOrCreate(cp);
    glm::ivec3 lp = toLocalPos(wx,wy,wz);
    c.set(lp.x, lp.y, lp.z, type);

    for (int axis = 0; axis < 3; axis++) {
        for (int s : {-1, 1}) {
            if ((s == -1 && lp[axis] == 0) || (s == 1 && lp[axis] == CHUNK_SIZE-1)) {
                glm::ivec3 ncp = cp; ncp[axis] += s;
                auto it = chunks.find(ncp);
                if (it != chunks.end()) it->second->dirty = true;
            }
        }
    }
}

void World::update() {
    for (auto& [pos, chunk] : chunks)
        if (chunk->dirty) chunk->buildMesh(*this);
}

void World::render(Shader& shader, const Camera& camera, const WorldSettings& ws) {
    glm::vec3 cv = camera.position / VOXEL_SIZE;
    glm::ivec3 cc = toChunkPos((int)cv.x, (int)cv.y, (int)cv.z);
    for (auto& [pos, chunk] : chunks) {
        if (chunk->indexCount == 0) continue;
        glm::ivec3 d = pos - cc;
        if (std::max({std::abs(d.x),std::abs(d.y),std::abs(d.z)}) > ws.render_dist) continue;
        chunk->render();
    }
}

// Player AABB: eye is camera.position, 0.28 half-width, 1.65 down to feet, 0.15 up to crown
bool World::overlapsVoxel(glm::vec3 eye) const {
    glm::vec3 mn = (eye + glm::vec3(-0.28f, -1.65f, -0.28f)) / VOXEL_SIZE;
    glm::vec3 mx = (eye + glm::vec3( 0.28f,  0.15f,  0.28f)) / VOXEL_SIZE;
    for (int x = (int)std::floor(mn.x); x <= (int)std::floor(mx.x); x++)
    for (int y = (int)std::floor(mn.y); y <= (int)std::floor(mx.y); y++)
    for (int z = (int)std::floor(mn.z); z <= (int)std::floor(mx.z); z++)
        if (getVoxelWorld(x,y,z)) return true;
    return false;
}

bool World::raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                    glm::ivec3& outHit, glm::ivec3& outNorm) const {
    origin  /= VOXEL_SIZE;                   // trace in voxel space
    maxDist /= VOXEL_SIZE;
    glm::vec3 d = glm::normalize(dir);
    glm::ivec3 pos  = glm::ivec3(glm::floor(origin));
    glm::ivec3 step = { d.x>0?1:-1, d.y>0?1:-1, d.z>0?1:-1 };
    glm::vec3 tDelta = glm::abs(1.f / d);
    glm::vec3 tMax;
    for (int i = 0; i < 3; i++) {
        if (std::abs(d[i]) < 1e-9f) { tMax[i]=1e30f; continue; }
        tMax[i] = d[i]>0 ? ((float)(pos[i]+1)-origin[i])/d[i]
                          : (origin[i]-(float)pos[i])/(-d[i]);
    }
    glm::ivec3 norm = {0,0,0};
    while (true) {
        if (getVoxelWorld(pos.x,pos.y,pos.z)) { outHit=pos; outNorm=norm; return true; }
        int ax = 0;
        if (tMax.y<tMax.x && tMax.y<tMax.z) ax=1;
        else if (tMax.z<tMax.x)             ax=2;
        if (tMax[ax] > maxDist) break;
        norm={0,0,0}; norm[ax]=-step[ax];
        pos[ax]+=step[ax]; tMax[ax]+=tDelta[ax];
    }
    return false;
}

// ── Save / Load ───────────────────────────────────────────────────────────────
// Format: magic(4) + version(1) + chunk_count(4) + per chunk [cx,cy,cz(3×4) + 4096 bytes]

static const char MAGIC[4] = {'V','X','W','L'};
static const uint8_t VERSION = 1;

bool World::save(const std::string& path) const {
    std::ofstream f(path, std::ios::binary);
    if (!f) return false;

    f.write(MAGIC, 4);
    f.write((char*)&VERSION, 1);

    uint32_t count = (uint32_t)chunks.size();
    f.write((char*)&count, 4);

    for (auto& [pos, chunk] : chunks) {
        int32_t cx = pos.x, cy = pos.y, cz = pos.z;
        f.write((char*)&cx, 4);
        f.write((char*)&cy, 4);
        f.write((char*)&cz, 4);
        f.write((char*)chunk->voxels, CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE);
    }
    return f.good();
}

bool World::load(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;

    char magic[4] = {};
    f.read(magic, 4);
    if (memcmp(magic, MAGIC, 4) != 0) return false;

    uint8_t ver = 0;
    f.read((char*)&ver, 1);
    if (ver != VERSION) return false;

    uint32_t count = 0;
    f.read((char*)&count, 4);

    chunks.clear();
    for (uint32_t i = 0; i < count; i++) {
        int32_t cx,cy,cz;
        f.read((char*)&cx,4); f.read((char*)&cy,4); f.read((char*)&cz,4);
        Chunk& c = getOrCreate({cx,cy,cz});
        f.read((char*)c.voxels, CHUNK_SIZE*CHUNK_SIZE*CHUNK_SIZE);
        c.dirty = true;
    }
    return f.good();
}
