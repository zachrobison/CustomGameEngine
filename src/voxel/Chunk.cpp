#include "Chunk.h"
#include "World.h"
#include <cstring>

Chunk::Chunk(glm::ivec3 pos) : chunkPos(pos) {
    std::memset(voxels, 0, sizeof(voxels));
}

Chunk::~Chunk() {
    if (vao) glDeleteVertexArrays(1, &vao);
    if (vbo) glDeleteBuffers(1, &vbo);
    if (ebo) glDeleteBuffers(1, &ebo);
}

uint8_t Chunk::get(int x, int y, int z) const {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return 0;
    return voxels[x][y][z];
}

void Chunk::set(int x, int y, int z, uint8_t type) {
    if (x < 0 || x >= CHUNK_SIZE || y < 0 || y >= CHUNK_SIZE || z < 0 || z >= CHUNK_SIZE)
        return;
    voxels[x][y][z] = type;
    dirty = true;
}

uint8_t Chunk::neighborGet(int lx, int ly, int lz, World& world) const {
    if (lx >= 0 && lx < CHUNK_SIZE &&
        ly >= 0 && ly < CHUNK_SIZE &&
        lz >= 0 && lz < CHUNK_SIZE)
        return voxels[lx][ly][lz];

    int wx = chunkPos.x * CHUNK_SIZE + lx;
    int wy = chunkPos.y * CHUNK_SIZE + ly;
    int wz = chunkPos.z * CHUNK_SIZE + lz;
    return world.getVoxelWorld(wx, wy, wz);
}

// face index → axis normal (d=axis, sign encoded by face%2==0 → +1)
static const int FACE_NORM[6][3] = {
    { 1, 0, 0}, {-1, 0, 0},
    { 0, 1, 0}, { 0,-1, 0},
    { 0, 0, 1}, { 0, 0,-1}
};

void Chunk::buildMesh(World& world) {
    std::vector<float>    verts;
    std::vector<uint32_t> idxs;
    verts.reserve(4096);
    idxs.reserve(6144);

    uint32_t base = 0;

    for (int face = 0; face < 6; face++) {
        int d    = face / 2;
        int sign = (face % 2 == 0) ? 1 : -1;
        int ua   = (d + 1) % 3;
        int va   = (d + 2) % 3;

        float nx = (float)FACE_NORM[face][0];
        float ny = (float)FACE_NORM[face][1];
        float nz = (float)FACE_NORM[face][2];

        for (int slice = 0; slice < CHUNK_SIZE; slice++) {
            uint8_t mask[CHUNK_SIZE * CHUNK_SIZE] = {};

            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; i++) {
                    int c[3];
                    c[d] = slice; c[ua] = i; c[va] = j;
                    uint8_t here = get(c[0], c[1], c[2]);
                    if (!here) { mask[j * CHUNK_SIZE + i] = 0; continue; }

                    int nc[3] = { c[0] + FACE_NORM[face][0],
                                  c[1] + FACE_NORM[face][1],
                                  c[2] + FACE_NORM[face][2] };
                    uint8_t nbr = neighborGet(nc[0], nc[1], nc[2], world);
                    mask[j * CHUNK_SIZE + i] = nbr ? 0 : here;
                }
            }

            // greedy merge
            for (int j = 0; j < CHUNK_SIZE; j++) {
                for (int i = 0; i < CHUNK_SIZE; ) {
                    uint8_t type = mask[j * CHUNK_SIZE + i];
                    if (!type) { i++; continue; }

                    int w = 1;
                    while (i + w < CHUNK_SIZE && mask[j * CHUNK_SIZE + i + w] == type)
                        w++;

                    int h = 1;
                    bool canGrow = true;
                    while (canGrow && j + h < CHUNK_SIZE) {
                        for (int k = 0; k < w; k++) {
                            if (mask[(j + h) * CHUNK_SIZE + i + k] != type) {
                                canGrow = false;
                                break;
                            }
                        }
                        if (canGrow) h++;
                    }

                    // quad position on the axis face plane
                    float fpos = (float)(slice + (sign > 0 ? 1 : 0));

                    // 4 corners: ua iterates 0..w, va iterates 0..h
                    for (int corner = 0; corner < 4; corner++) {
                        int uOff = (corner == 1 || corner == 2) ? w : 0;
                        int vOff = (corner >= 2)                ? h : 0;

                        float wp[3];
                        wp[d]  = fpos  + (float)(chunkPos[d]  * CHUNK_SIZE);
                        wp[ua] = (float)(i + uOff) + (float)(chunkPos[ua] * CHUNK_SIZE);
                        wp[va] = (float)(j + vOff) + (float)(chunkPos[va] * CHUNK_SIZE);

                        verts.insert(verts.end(), {
                            wp[0] * VOXEL_SIZE, wp[1] * VOXEL_SIZE, wp[2] * VOXEL_SIZE,
                            nx, ny, nz,
                            (float)type
                        });
                    }

                    // winding: flip for negative-sign faces so normals face outward
                    if (sign > 0) {
                        idxs.insert(idxs.end(), {base,base+1,base+2, base,base+2,base+3});
                    } else {
                        idxs.insert(idxs.end(), {base,base+2,base+1, base,base+3,base+2});
                    }
                    base += 4;

                    // clear merged region
                    for (int hh = 0; hh < h; hh++)
                        for (int ww = 0; ww < w; ww++)
                            mask[(j + hh) * CHUNK_SIZE + i + ww] = 0;

                    i += w;
                }
            }
        }
    }

    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }

    glBindVertexArray(vao);

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size() * sizeof(float)), verts.data(), GL_DYNAMIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idxs.size() * sizeof(uint32_t)), idxs.data(), GL_DYNAMIC_DRAW);

    // stride: pos(3) + normal(3) + type(1) = 7 floats
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, 7 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);

    indexCount = (int)idxs.size();
    dirty = false;
}

void Chunk::render() const {
    if (!vao || indexCount == 0) return;
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
