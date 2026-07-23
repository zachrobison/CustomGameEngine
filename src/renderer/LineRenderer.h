#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>

// Draws wireframe boxes — used for the voxel aim highlight.
class LineRenderer {
public:
    LineRenderer();
    ~LineRenderer();

    void drawBox(glm::vec3 bmin, glm::vec3 bmax,
                 const glm::mat4& VP,
                 glm::vec4 color = {1.f, 1.f, 1.f, 0.85f});

    // Single world-space segment (tracers, beams).
    void drawLine(glm::vec3 a, glm::vec3 b,
                  const glm::mat4& VP,
                  glm::vec4 color = {1.f, 0.3f, 0.3f, 0.9f});

private:
    GLuint vao = 0, vbo = 0, ebo = 0, eboLine = 0, prog = 0;
};
