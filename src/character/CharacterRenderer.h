#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>
#include "Character.h"

// Renders a Character as a hierarchy of scaled boxes.
// Also owns a small FBO used for the panel preview image.
class CharacterRenderer {
public:
    CharacterRenderer();
    ~CharacterRenderer();

    // Rebuild the geometry whenever character data changes.
    // Call once after any scale / clothing change.
    void rebuild(const Character& c);

    // Draw into the panel preview FBO.  Returns the OpenGL texture ID
    // that ImGui::Image() should display.
    GLuint renderPreview(const Character& c, float rotY);

    // Draw the character in world space (for future in-world avatar).
    void renderWorld(const glm::mat4& VP, glm::vec3 pos, float rotY);

    static constexpr int PREVIEW_W = 250;
    static constexpr int PREVIEW_H = 400;

private:
    // ── GPU resources ──────────────────────────────────────────────────
    GLuint vao = 0, vbo = 0, ebo = 0;
    GLuint prog = 0;
    GLuint fbo = 0, colorTex = 0, depthRbo = 0;
    int    indexCount = 0;

    void   initFBO();
    void   initShader();
    void   drawMesh(const glm::mat4& MVP);
};
