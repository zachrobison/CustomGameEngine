#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

class Camera;

class Skybox {
public:
    Skybox();
    ~Skybox();

    // Must be rendered before the voxel world (writes to background at max depth)
    void render(const Camera& camera, float aspect, glm::vec3 sunDir);

private:
    GLuint vao = 0, vbo = 0, ebo = 0, prog = 0;
};
