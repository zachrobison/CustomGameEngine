#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

// Smooth heightmap terrain mesh rendered when voxelWorld is off.
// Matches the voxel world's noise parameters so switching looks consistent.
class Terrain {
public:
    Terrain();
    ~Terrain();

    // Generate or regenerate the mesh.
    // flat=true produces a checker-grid test floor at constant height.
    void generate(int gridSize = 256, float cellSize = 4.f, bool flat = false);

    void render(const glm::mat4& VP, glm::vec3 sunDir, float fogDensity,
                glm::vec3 cameraPos);

private:
    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0;
    GLuint grassTex = 0;
    int    indexCount = 0;

    void initShader();
    void loadTextures();
};
