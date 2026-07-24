#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>
#include <vector>

// Procedural low-poly trees scattered across the mesh terrain.
// Parameters must match Terrain::generate() exactly.
class TreeRenderer {
public:
    TreeRenderer();
    ~TreeRenderer();

    // Rebuild the tree mesh at explicit positions (x,z; y = size hint),
    // all sitting on ground plane `groundY`. Used by Iron Command.
    void buildFrom(const std::vector<glm::vec3>& trees, float groundY);

    void render(const glm::mat4& VP, glm::vec3 sunDir,
                float fogDensity, glm::vec3 cameraPos);

private:
    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0;
    int    indexCount = 0;

    void build();
    void initShader();
};
