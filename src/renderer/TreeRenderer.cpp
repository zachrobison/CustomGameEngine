#include "TreeRenderer.h"
#include "../voxel/Noise.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

static const char* TR_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uVP;
out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vPos    = aPos;
    vNormal = aNormal;
    vColor  = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

static const char* TR_FRAG = R"(
#version 410 core
in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform vec3  uSunDir;
uniform float uFogDensity;
uniform vec3  uCamPos;
void main() {
    vec3  n   = normalize(vNormal);
    float d   = max(dot(n, normalize(uSunDir)), 0.0);
    float lit = 0.35 + 0.65 * d;
    vec3  col = vColor * lit;
    float dist   = length(vPos - uCamPos);
    float fogFac = exp(-uFogDensity * dist * dist);
    vec3  skyCol = vec3(0.62, 0.84, 0.98);
    FragColor = vec4(mix(skyCol, col, clamp(fogFac,0,1)), 1.0);
}
)";

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) throw std::runtime_error("TreeRenderer shader compile failed");
    return s;
}

// Returns world-space terrain height — must match Terrain.cpp exactly.
static float terrainY(float wx, float wz) {
    float ix = wx / 4.f + 128.f;
    float iz = wz / 4.f + 128.f;
    return Noise::fractal(ix * 0.025f, iz * 0.025f, 4) * 28.f + 20.f;
}

// Append a cylinder into verts/idx.  cy=centre Y of bottom circle.
static void addCylinder(std::vector<float>& verts, std::vector<uint32_t>& idx,
                        glm::vec3 base, float radius, float height,
                        int segs, glm::vec3 color) {
    uint32_t base0 = (uint32_t)(verts.size() / 9);
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i      / segs * 6.2831853f;
        float a1 = (float)(i+1)  / segs * 6.2831853f;
        glm::vec3 n0 = {cosf(a0), 0, sinf(a0)};
        glm::vec3 n1 = {cosf(a1), 0, sinf(a1)};
        glm::vec3 v0b = base + n0 * radius;
        glm::vec3 v1b = base + n1 * radius;
        glm::vec3 v0t = v0b + glm::vec3(0, height, 0);
        glm::vec3 v1t = v1b + glm::vec3(0, height, 0);
        glm::vec3 fn  = glm::normalize(n0 + n1);

        auto push = [&](glm::vec3 p, glm::vec3 n) {
            verts.insert(verts.end(), {p.x,p.y,p.z, n.x,n.y,n.z, color.r,color.g,color.b});
        };
        uint32_t i0 = (uint32_t)(verts.size()/9);
        push(v0b, fn); push(v1b, fn); push(v0t, fn); push(v1t, fn);
        idx.insert(idx.end(), {i0,i0+1,i0+2, i0+1,i0+3,i0+2});
    }
}

// Append a cone (foliage) into verts/idx.
static void addCone(std::vector<float>& verts, std::vector<uint32_t>& idx,
                    glm::vec3 base, float radius, float height,
                    int segs, glm::vec3 color) {
    glm::vec3 apex = base + glm::vec3(0, height, 0);
    for (int i = 0; i < segs; i++) {
        float a0 = (float)i     / segs * 6.2831853f;
        float a1 = (float)(i+1) / segs * 6.2831853f;
        glm::vec3 v0 = base + glm::vec3(cosf(a0)*radius, 0, sinf(a0)*radius);
        glm::vec3 v1 = base + glm::vec3(cosf(a1)*radius, 0, sinf(a1)*radius);
        glm::vec3 e0 = glm::normalize(v0 - base);
        glm::vec3 e1 = glm::normalize(v1 - base);
        glm::vec3 fn = glm::normalize(glm::cross(v1-v0, apex-v0));

        auto push = [&](glm::vec3 p, glm::vec3 n) {
            verts.insert(verts.end(), {p.x,p.y,p.z, n.x,n.y,n.z, color.r,color.g,color.b});
        };
        uint32_t i0 = (uint32_t)(verts.size()/9);
        push(v0, fn); push(v1, fn); push(apex, fn);
        idx.insert(idx.end(), {i0, i0+1, i0+2});
    }
}

void TreeRenderer::buildFrom(const std::vector<glm::vec3>& trees, float groundY) {
    if (!prog) initShader();
    std::vector<float>    verts;
    std::vector<uint32_t> idx;
    uint32_t seed = 0x1234abcdu;
    auto lcg = [&]() -> float { seed = seed*1664525u+1013904223u; return (seed>>8)/(float)(1<<24); };
    for (auto& t : trees) {
        float sz = t.y * 0.7f;                 // t.y is the size hint
        float trunkH = sz*2.8f, trunkR = sz*0.18f, foliageR = sz*1.6f, foliageH = sz*3.5f;
        glm::vec3 trunkBase = {t.x, groundY, t.z};
        glm::vec3 trunkCol  = {0.38f+lcg()*0.06f, 0.25f, 0.14f};
        float gr = 0.22f+lcg()*0.12f;
        glm::vec3 leafCol  = {0.15f+lcg()*0.08f, 0.38f+gr, 0.10f};
        glm::vec3 leafCol2 = {0.12f, 0.32f+gr, 0.09f};
        addCylinder(verts, idx, trunkBase, trunkR, trunkH, 7, trunkCol);
        glm::vec3 fb = trunkBase + glm::vec3(0, trunkH*0.55f, 0);
        addCone(verts, idx, fb, foliageR, foliageH, 8, leafCol);
        addCone(verts, idx, fb + glm::vec3(0, foliageH*0.42f, 0), foliageR*0.65f, foliageH*0.7f, 8, leafCol2);
    }
    indexCount = (int)idx.size();
    if (!vao) { glGenVertexArrays(1,&vao); glGenBuffers(1,&vbo); glGenBuffers(1,&ebo); }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size()*sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);
    constexpr int stride = 9*sizeof(float);
    glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,stride,(void*)0); glEnableVertexAttribArray(0);
    glVertexAttribPointer(1,3,GL_FLOAT,GL_FALSE,stride,(void*)(3*sizeof(float))); glEnableVertexAttribArray(1);
    glVertexAttribPointer(2,3,GL_FLOAT,GL_FALSE,stride,(void*)(6*sizeof(float))); glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void TreeRenderer::build() {
    const int   N    = 256;
    const float cell = 4.f;
    // Sample candidate positions across the grid
    const int treeCount = 1200;

    std::vector<float>    verts;
    std::vector<uint32_t> idx;
    verts.reserve(treeCount * 200);
    idx.reserve  (treeCount * 100);

    // Deterministic pseudo-random via simple LCG seed
    uint32_t seed = 0xA3B4C5D6u;
    auto lcg = [&]() -> float {
        seed = seed * 1664525u + 1013904223u;
        return (seed >> 8) / (float)(1 << 24);
    };

    int placed = 0;
    int attempts = 0;
    while (placed < treeCount && attempts < treeCount * 8) {
        attempts++;
        // Random index in [0, N)
        float fi = lcg() * N;
        float fz = lcg() * N;
        float wx = (fi - N/2) * cell;
        float wz = (fz - N/2) * cell;

        float h = terrainY(wx, wz);

        // Only grass zone, avoid steep slopes
        float hl = terrainY(wx-cell, wz);
        float hr = terrainY(wx+cell, wz);
        float hd = terrainY(wx,      wz-cell);
        float hu = terrainY(wx,      wz+cell);
        float slope = std::sqrt(((hr-hl)/(2*cell))*((hr-hl)/(2*cell))
                              + ((hu-hd)/(2*cell))*((hu-hd)/(2*cell)));

        if (h < 19.f || h > 38.f || slope > 0.28f) continue;

        // Vary tree size slightly
        float sz = 0.75f + lcg() * 0.65f;
        float trunkH   = sz * 2.8f;
        float trunkR   = sz * 0.18f;
        float foliageR = sz * 1.6f;
        float foliageH = sz * 3.5f;

        // Slight random offset within cell
        float ox = (lcg() - 0.5f) * cell * 0.6f;
        float oz = (lcg() - 0.5f) * cell * 0.6f;
        glm::vec3 trunkBase = {wx + ox, h, wz + oz};

        // Brown trunk
        glm::vec3 trunkCol  = {0.38f + lcg()*0.06f, 0.25f, 0.14f};
        // Green foliage (two stacked cones for more shape)
        float gr = 0.22f + lcg()*0.12f;
        glm::vec3 leafCol   = {0.15f + lcg()*0.08f, 0.38f + gr, 0.10f};
        glm::vec3 leafCol2  = {0.12f, 0.32f + gr, 0.09f};

        addCylinder(verts, idx, trunkBase,     trunkR, trunkH,  7, trunkCol);
        glm::vec3 foliageBase = trunkBase + glm::vec3(0, trunkH * 0.55f, 0);
        addCone(verts, idx, foliageBase,        foliageR,        foliageH,        8, leafCol);
        addCone(verts, idx, foliageBase + glm::vec3(0, foliageH*0.42f,0),
                            foliageR*0.65f, foliageH*0.7f, 8, leafCol2);
        placed++;
    }

    indexCount = (int)idx.size();
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(idx.size()*sizeof(uint32_t)), idx.data(), GL_STATIC_DRAW);

    constexpr int stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void TreeRenderer::initShader() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   TR_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, TR_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog,v); glAttachShader(prog,f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

TreeRenderer::TreeRenderer()  { initShader(); build(); }
TreeRenderer::~TreeRenderer() {
    glDeleteProgram(prog);
    if (vao) { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo); }
}

void TreeRenderer::render(const glm::mat4& VP, glm::vec3 sunDir,
                           float fogDensity, glm::vec3 cameraPos) {
    if (!indexCount) return;
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"),         1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv      (glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f       (glGetUniformLocation(prog,"uFogDensity"), fogDensity);
    glUniform3fv      (glGetUniformLocation(prog,"uCamPos"),     1, glm::value_ptr(cameraPos));
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}
