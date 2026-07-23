#include "LineRenderer.h"
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

static const char* VERT_SRC = R"(
#version 410 core
layout(location=0) in vec3 aPos;
uniform mat4 uVP;
uniform vec3 uMin;
uniform vec3 uMax;
void main() {
    gl_Position = uVP * vec4(mix(uMin, uMax, aPos), 1.0);
}
)";

static const char* FRAG_SRC = R"(
#version 410 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";

// Unit cube: 8 corners, coords 0 or 1 per axis
static const float VERTS[24] = {
    0,0,0, 1,0,0, 1,1,0, 0,1,0,
    0,0,1, 1,0,1, 1,1,1, 0,1,1
};
// 12 edges → 24 indices
static const uint8_t EDGES[24] = {
    0,1, 1,2, 2,3, 3,0,
    4,5, 5,6, 6,7, 7,4,
    0,4, 1,5, 2,6, 3,7
};

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) throw std::runtime_error("LineRenderer shader compile failed");
    return s;
}

LineRenderer::LineRenderer() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   VERT_SRC);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, FRAG_SRC);
    prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);

    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(VERTS), VERTS, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(EDGES), EDGES, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);

    // line EBO: corner (0,0,0) → uMin, corner (1,1,1) → uMax
    static const uint8_t LINE_IDX[2] = {0, 6};
    glGenBuffers(1, &eboLine);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboLine);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(LINE_IDX), LINE_IDX, GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
}

LineRenderer::~LineRenderer() {
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &eboLine);
}

void LineRenderer::drawLine(glm::vec3 a, glm::vec3 b,
                             const glm::mat4& VP, glm::vec4 color) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"), 1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv(glGetUniformLocation(prog,"uMin"), 1, glm::value_ptr(a));
    glUniform3fv(glGetUniformLocation(prog,"uMax"), 1, glm::value_ptr(b));
    glUniform4fv(glGetUniformLocation(prog,"uColor"), 1, glm::value_ptr(color));
    glBindVertexArray(vao);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, eboLine);
    glDrawElements(GL_LINES, 2, GL_UNSIGNED_BYTE, nullptr);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBindVertexArray(0);
}

void LineRenderer::drawBox(glm::vec3 bmin, glm::vec3 bmax,
                            const glm::mat4& VP, glm::vec4 color) {
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"), 1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv(glGetUniformLocation(prog,"uMin"), 1, glm::value_ptr(bmin));
    glUniform3fv(glGetUniformLocation(prog,"uMax"), 1, glm::value_ptr(bmax));
    glUniform4fv(glGetUniformLocation(prog,"uColor"), 1, glm::value_ptr(color));

    glDepthFunc(GL_LEQUAL);
    glBindVertexArray(vao);
    glDrawElements(GL_LINES, 24, GL_UNSIGNED_BYTE, nullptr);
    glBindVertexArray(0);
    glDepthFunc(GL_LESS);
}
