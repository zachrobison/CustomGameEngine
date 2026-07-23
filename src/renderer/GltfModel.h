#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <string>
#include <vector>

// Loads and renders a GLB file as a static mesh (no skinning).
// Suitable for displaying characters and props.
class GltfModel {
public:
    GltfModel() = default;
    ~GltfModel();

    // Owns raw GL handles: copying would let one instance's destructor delete
    // the other's VAOs/buffers/program. Move-only.
    GltfModel(const GltfModel&)            = delete;
    GltfModel& operator=(const GltfModel&) = delete;
    GltfModel(GltfModel&& o) noexcept;
    GltfModel& operator=(GltfModel&& o) noexcept;

    bool load(const std::string& path); // returns false on failure
    bool loaded() const { return !meshes.empty(); }

    // Render at worldPos, facing yaw (radians), scaled uniformly.
    void render(const glm::mat4& VP, glm::vec3 worldPos, float yaw, float scale,
                glm::vec3 sunDir, float fogDensity, glm::vec3 camPos);

    // Render with an arbitrary model matrix (e.g. first-person viewmodels).
    // tint multiplies each material colour (>1 to glow, e.g. parry bullets).
    void renderMatrix(const glm::mat4& VP, const glm::mat4& model,
                      glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                      glm::vec3 tint = {1.f, 1.f, 1.f});

    struct GpuMesh {
        GLuint    vao = 0, vbo = 0, ebo = 0;
        int       indexCount = 0;
        GLenum    indexType  = GL_UNSIGNED_INT;
        glm::vec3 color      = {0.8f, 0.8f, 0.8f};
        glm::mat4 localXform = glm::mat4(1.f); // node transform applied per draw call
    };

private:
    std::vector<GpuMesh> meshes;
    GLuint prog = 0;

    void initShader();
    void cleanup();
};
