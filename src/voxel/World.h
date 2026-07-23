#pragma once
#include "Chunk.h"
#include <glm/glm.hpp>
#include <unordered_map>
#include <memory>
#include <string>

struct IVec3Hash {
    size_t operator()(const glm::ivec3& v) const {
        size_t h = 17;
        h = h * 31 + std::hash<int>()(v.x);
        h = h * 31 + std::hash<int>()(v.y);
        h = h * 31 + std::hash<int>()(v.z);
        return h;
    }
};

class Shader;
class Camera;
struct WorldSettings;

class World {
public:
    std::unordered_map<glm::ivec3, std::unique_ptr<Chunk>, IVec3Hash> chunks;

    // Call each frame with the player position to load nearby chunks on
    // demand. procedural=false creates empty chunks instead of noise terrain
    // (authored maps that should end in void).
    void streamChunks(glm::vec3 playerPos, int renderDist, bool procedural = true);

    void update();
    void render(Shader& shader, const Camera& camera, const WorldSettings& ws);

    uint8_t getVoxelWorld(int wx, int wy, int wz) const;
    void    setVoxelWorld(int wx, int wy, int wz, uint8_t type);

    bool raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                 glm::ivec3& outHit, glm::ivec3& outNorm) const;

    bool overlapsVoxel(glm::vec3 eyePos) const; // player AABB vs voxels

    int  getTerrainHeight(int wx, int wz) const;

    bool save(const std::string& path) const;
    bool load(const std::string& path);

private:
    Chunk& getOrCreate(glm::ivec3 cp);
    void   generateChunk(glm::ivec3 cp);

    static int       floorDiv(int a, int b);
    static glm::ivec3 toChunkPos(int wx, int wy, int wz);
    static glm::ivec3 toLocalPos (int wx, int wy, int wz);
};
