#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <cstdint>
#include <vector>

constexpr int CHUNK_SIZE = 16;

// World-space edge length of one voxel. Voxel coordinates stay integers;
// this scales them into world units at every world<->voxel boundary
// (meshing, raycast, collision, chunk streaming).
constexpr float VOXEL_SIZE = 0.2f;

class World;

class Chunk {
public:
    uint8_t voxels[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE] = {};
    glm::ivec3 chunkPos;
    bool dirty = true;

    GLuint vao = 0, vbo = 0, ebo = 0;
    int indexCount = 0;

    explicit Chunk(glm::ivec3 pos);
    ~Chunk();

    uint8_t get(int x, int y, int z) const;
    void    set(int x, int y, int z, uint8_t type);

    void buildMesh(World& world);
    void render() const;

private:
    uint8_t neighborGet(int lx, int ly, int lz, World& world) const;
};
