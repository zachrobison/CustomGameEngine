#include "Props.h"
#include "../vendor/json.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cmath>
#include <fstream>

static const char* PR_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uVP;
out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vPos = aPos; vNormal = aNormal; vColor = aColor;
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

static const char* PR_FRAG = R"(
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
    float lit = 0.4 + 0.6 * d;
    vec3  col = vColor * lit;
    float dist   = length(vPos - uCamPos);
    float fogFac = exp(-uFogDensity * dist * dist);
    vec3  skyCol = vec3(0.62, 0.84, 0.98);
    FragColor = vec4(mix(skyCol, col, clamp(fogFac,0,1)), 1.0);
}
)";

void Props::initShader() {
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        return s;
    };
    GLuint v = compile(GL_VERTEX_SHADER,   PR_VERT);
    GLuint f = compile(GL_FRAGMENT_SHADER, PR_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

Props::~Props() {
    if (vao)  glDeleteVertexArrays(1, &vao);
    if (vbo)  glDeleteBuffers(1, &vbo);
    if (prog) glDeleteProgram(prog);
}

void Props::addBox(glm::vec3 mn, glm::vec3 mx, glm::vec3 color, bool visible) {
    boxes.push_back({mn, mx, color, visible});
}

bool Props::setGroupActive(const std::string& itemRef, bool on) {
    bool changed = false;
    for (auto& bx : boxes) {
        if (bx.itemRef == itemRef && bx.groupOn != on) {
            bx.groupOn = on;
            changed = true;
        }
    }
    if (changed) buildMesh();
    return changed;
}

void Props::clear() {
    boxes.clear();
    groupNames.clear();
    if (vao)  { glDeleteVertexArrays(1, &vao); vao = 0; }
    if (vbo)  { glDeleteBuffers(1, &vbo);      vbo = 0; }
    if (prog) { glDeleteProgram(prog);         prog = 0; }
    vertCount = 0;
}

bool Props::loadFile(const std::string& path) {
    clear();
    std::ifstream f(path);
    if (f) {
        try {
            nlohmann::json j;
            f >> j;
            for (auto& b : j["boxes"]) {
                auto v3 = [&](const char* k, glm::vec3 dflt) {
                    if (!b.contains(k)) return dflt;
                    return glm::vec3(b[k][0].get<float>(), b[k][1].get<float>(),
                                     b[k][2].get<float>());
                };
                addBox(v3("min", {0,0,0}), v3("max", {1,1,1}),
                       v3("color", {0.7f,0.7f,0.7f}), b.value("visible", true));
                boxes.back().solid   = b.value("solid", true);
                boxes.back().itemRef = b.value("itemRef", std::string());
                if (!boxes.back().itemRef.empty() &&
                    std::find(groupNames.begin(), groupNames.end(),
                              boxes.back().itemRef) == groupNames.end())
                    groupNames.push_back(boxes.back().itemRef);
            }
        } catch (...) {
            boxes.clear();
        }
    }
    buildMesh();       // empty set is fine — renders/collides nothing
    return !boxes.empty();
}

void Props::addRamp(glm::vec3 base, glm::vec3 dir, float width, float length,
                    float height, glm::vec3 color) {
    const float stepH = 0.5f;
    int steps = std::max(1, (int)std::ceil(height / stepH));
    float stepL = length / steps;
    for (int i = 0; i < steps; i++) {
        float h  = stepH * (i + 1);
        glm::vec3 along = dir * (stepL * i);
        glm::vec3 mn = base + along;
        glm::vec3 mx = mn + dir * stepL + glm::vec3(0, h, 0);
        // widen perpendicular to dir
        glm::vec3 perp = glm::vec3(dir.z, 0, dir.x) * (width * 0.5f);
        glm::vec3 lo = glm::min(mn - perp, mx - perp);
        glm::vec3 hi = glm::max(mn + perp, mx + perp);
        boxes.push_back({lo, hi, color});
    }
}

void Props::buildMesh() {
    // 6 faces * 2 tris * 3 verts, interleaved pos3 normal3 color3
    std::vector<float> v;
    v.reserve(boxes.size() * 36 * 9);
    auto quad = [&](glm::vec3 a, glm::vec3 b, glm::vec3 c, glm::vec3 d,
                    glm::vec3 n, glm::vec3 col) {
        // Emit CCW as seen from the outward normal's side; if the given
        // order winds the other way, flip it so back-face culling keeps the
        // exterior and not the interior.
        if (glm::dot(glm::cross(b - a, c - a), n) < 0.f)
            std::swap(b, d);
        const glm::vec3 pts[6] = {a, b, c, a, c, d};
        for (auto& p : pts) {
            v.insert(v.end(), {p.x,p.y,p.z, n.x,n.y,n.z, col.x,col.y,col.z});
        }
    };
    for (auto& bx : boxes) {
        if (!bx.visible || !bx.groupOn) continue;
        glm::vec3 mn = bx.mn, mx = bx.mx, c = bx.color;
        quad({mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},{mx.x,mx.y,mx.z},{mn.x,mx.y,mx.z},{0, 1,0},c); // top
        quad({mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mx.x,mn.y,mn.z},{mn.x,mn.y,mn.z},{0,-1,0},c); // bottom
        quad({mn.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mn.y,mx.z},{0,0, 1},c); // +z
        quad({mx.x,mn.y,mn.z},{mx.x,mx.y,mn.z},{mn.x,mx.y,mn.z},{mn.x,mn.y,mn.z},{0,0,-1},c); // -z
        quad({mx.x,mn.y,mx.z},{mx.x,mx.y,mx.z},{mx.x,mx.y,mn.z},{mx.x,mn.y,mn.z},{ 1,0,0},c); // +x
        quad({mn.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mn.x,mx.y,mx.z},{mn.x,mn.y,mx.z},{-1,0,0},c); // -x
    }
    vertCount = (int)(v.size() / 9);

    initShader();
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(v.size()*sizeof(float)),
                 v.data(), GL_STATIC_DRAW);
    constexpr int stride = 9 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
    for (int k = 0; k < 3; k++) glEnableVertexAttribArray(k);
    glBindVertexArray(0);
}

void Props::render(const glm::mat4& VP, glm::vec3 sunDir,
                   float fogDensity, glm::vec3 camPos) {
    if (!prog || vertCount == 0) return;
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"), 1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv(glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f (glGetUniformLocation(prog,"uFogDensity"), fogDensity);
    glUniform3fv(glGetUniformLocation(prog,"uCamPos"),     1, glm::value_ptr(camPos));
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLES, 0, vertCount);
    glBindVertexArray(0);
}

// Slab-method ray vs AABB; nearest hit across all boxes.
bool Props::raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                    glm::vec3& outPoint) const {
    float bestT = maxDist;
    bool  found = false;
    for (auto& bx : boxes) {
        if (!bx.solid || !bx.groupOn) continue;
        float t0 = 0.f, t1 = bestT;
        bool  miss = false;
        for (int a = 0; a < 3 && !miss; a++) {
            if (std::abs(dir[a]) < 1e-8f) {
                if (origin[a] < bx.mn[a] || origin[a] > bx.mx[a]) miss = true;
            } else {
                float inv = 1.f / dir[a];
                float tA = (bx.mn[a] - origin[a]) * inv;
                float tB = (bx.mx[a] - origin[a]) * inv;
                if (tA > tB) std::swap(tA, tB);
                t0 = std::max(t0, tA);
                t1 = std::min(t1, tB);
                if (t0 > t1) miss = true;
            }
        }
        if (!miss && t0 < bestT && t0 > 0.f) {
            bestT = t0;
            found = true;
        }
    }
    if (found) outPoint = origin + dir * bestT;
    return found;
}

bool Props::overlapsBox(glm::vec3 mn, glm::vec3 mx) const {
    for (auto& bx : boxes) {
        if (!bx.solid || !bx.groupOn) continue;
        if (mn.x < bx.mx.x && mx.x > bx.mn.x &&
            mn.y < bx.mx.y && mx.y > bx.mn.y &&
            mn.z < bx.mx.z && mx.z > bx.mn.z) return true;
    }
    return false;
}

bool Props::overlapsPlayer(glm::vec3 eye) const {
    return overlapsBox(eye + glm::vec3(-0.28f, -1.65f, -0.28f),
                       eye + glm::vec3( 0.28f,  0.15f,  0.28f));
}

float Props::supportHeight(float x, float z, float maxTop) const {
    float best = -1e9f;
    const float r = 0.28f;   // player half-width
    for (auto& bx : boxes) {
        if (!bx.solid || !bx.groupOn) continue;
        if (x < bx.mn.x - r || x > bx.mx.x + r) continue;
        if (z < bx.mn.z - r || z > bx.mx.z + r) continue;
        if (bx.mx.y <= maxTop && bx.mx.y > best) best = bx.mx.y;
    }
    return best;
}
