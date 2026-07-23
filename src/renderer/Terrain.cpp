#include "Terrain.h"
#include "../voxel/Noise.h"
#include "../vendor/stb_image.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <stdexcept>
#include <cmath>

// ── Shaders ───────────────────────────────────────────────────────────────

static const char* T_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uVP;
out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
out vec2 vUV;
void main() {
    vPos    = aPos;
    vNormal = aNormal;
    vColor  = aColor;
    vUV     = aPos.xz * 0.08;   // tiling scale: ~12 tiles per 100 units
    gl_Position = uVP * vec4(aPos, 1.0);
}
)";

static const char* T_FRAG = R"(
#version 410 core
in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
in vec2 vUV;
out vec4 FragColor;

uniform vec3      uSunDir;
uniform float     uFogDensity;
uniform vec3      uCamPos;
uniform sampler2D uGrassTex;

void main() {
    vec3  n    = normalize(vNormal);
    float slope = 1.0 - n.y;                 // 0=flat, 1=vertical

    // Triplanar-ish: fade grass on steep slopes
    vec3 grassCol = texture(uGrassTex, vUV).rgb;

    // Height-based blending: grass → rock → snow
    float h = vPos.y;
    vec3 rockCol  = vec3(0.50, 0.47, 0.43);
    vec3 snowCol  = vec3(0.88, 0.91, 0.96);
    vec3 dirtCol  = vec3(0.45, 0.35, 0.24);  // valley floors, no water

    vec3 baseCol;
    if      (h < 18.0) baseCol = mix(dirtCol,  grassCol, (h-10.0)/8.0);
    else if (h < 36.0) baseCol = grassCol;
    else if (h < 44.0) baseCol = mix(grassCol, rockCol, (h-36.0)/8.0);
    else if (h < 50.0) baseCol = mix(rockCol,  snowCol, (h-44.0)/6.0);
    else               baseCol = snowCol;

    // Steep slopes always show rock regardless of height
    baseCol = mix(baseCol, rockCol, smoothstep(0.35, 0.70, slope));

    // Lighting
    float d   = max(dot(n, normalize(uSunDir)), 0.0);
    float lit = 0.38 + 0.62 * d;
    if (n.y > 0.5)  lit *= 1.08;
    if (n.y < -0.5) lit *= 0.55;
    vec3 col = baseCol * lit;

    // Fog
    float dist   = length(vPos - uCamPos);
    float fogFac = exp(-uFogDensity * dist * dist);
    vec3  skyCol2 = vec3(0.62, 0.84, 0.98);
    FragColor = vec4(mix(skyCol2, col, clamp(fogFac, 0.0, 1.0)), 1.0);
}
)";

// ── Helpers ───────────────────────────────────────────────────────────────

static GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) throw std::runtime_error("Terrain shader compile failed");
    return s;
}

// Maps world height to a terrain colour (matches voxel biome palette).
static glm::vec3 heightColor(float y, float slope) {
    glm::vec3 water  = {0.20f, 0.40f, 0.72f};
    glm::vec3 sand   = {0.87f, 0.82f, 0.65f};
    glm::vec3 grass  = {0.34f, 0.60f, 0.22f};
    glm::vec3 rock   = {0.52f, 0.49f, 0.45f};
    glm::vec3 snow   = {0.88f, 0.91f, 0.96f};

    glm::vec3 base;
    if      (y < 8.f)  base = water;
    else if (y < 10.f) base = mix(sand,  grass, (y-8.f)/2.f);
    else if (y < 14.f) base = grass;
    else if (y < 20.f) base = mix(grass, rock,  (y-14.f)/6.f);
    else if (y < 26.f) base = mix(rock,  snow,  (y-20.f)/6.f);
    else               base = snow;

    // Steep slopes go rocky
    float slopeFac = std::min(slope * 1.8f, 1.f);
    return mix(base, rock, slopeFac * 0.7f);
}

// ── Generate ──────────────────────────────────────────────────────────────

void Terrain::generate(int N, float cell, bool flat) {
    // N = number of cells per side → (N+1)^2 vertices
    int V = N + 1;

    // Build height map
    std::vector<float> hmap(V * V);
    for (int z = 0; z < V; z++)
        for (int x = 0; x < V; x++) {
            float h = flat ? 20.f
                           : Noise::fractal(x * 0.025f, z * 0.025f, 4) * 28.f + 20.f;
            hmap[z*V+x] = h;
        }

    // Build vertex buffer: pos(3) + normal(3) + color(3) = 9 floats
    std::vector<float>    verts;
    std::vector<uint32_t> idx;
    verts.reserve(V * V * 9);
    idx.reserve(N * N * 6);

    // Normals via central differences
    for (int z = 0; z < V; z++) {
        for (int x = 0; x < V; x++) {
            float wx = (x - N/2) * cell;
            float wz = (z - N/2) * cell;
            float h  = hmap[z*V+x];

            float hl = (x > 0)   ? hmap[z*V+(x-1)] : h;
            float hr = (x < V-1) ? hmap[z*V+(x+1)] : h;
            float hd = (z > 0)   ? hmap[(z-1)*V+x] : h;
            float hu = (z < V-1) ? hmap[(z+1)*V+x] : h;

            glm::vec3 nx = glm::normalize(glm::vec3(hl-hr, 2.f*cell, 0));
            glm::vec3 nz = glm::normalize(glm::vec3(0, 2.f*cell, hd-hu));
            glm::vec3 n  = glm::normalize(nx + nz);

            float dhdx = (hr - hl) / (2.f * cell);
            float dhdz = (hu - hd) / (2.f * cell);
            float slope = std::sqrt(dhdx*dhdx + dhdz*dhdz);

            glm::vec3 col;
            if (flat) {
                // Checker-grid floor: alternate two grays every 8 world units
                int gx = (int)std::floor(wx / 8.f);
                int gz = (int)std::floor(wz / 8.f);
                col = ((gx + gz) & 1) ? glm::vec3(0.60f, 0.60f, 0.62f)
                                       : glm::vec3(0.48f, 0.48f, 0.50f);
            } else {
                col = heightColor(h, slope);
            }

            verts.push_back(wx); verts.push_back(h); verts.push_back(wz);
            verts.push_back(n.x); verts.push_back(n.y); verts.push_back(n.z);
            verts.push_back(col.r); verts.push_back(col.g); verts.push_back(col.b);
        }
    }

    // Indices
    for (int z = 0; z < N; z++) {
        for (int x = 0; x < N; x++) {
            uint32_t a = z*V+x, b = z*V+x+1, c = (z+1)*V+x, d = (z+1)*V+x+1;
            // CCW from above so GL_BACK culling keeps the top face
            idx.push_back(a); idx.push_back(b); idx.push_back(c);
            idx.push_back(b); idx.push_back(d); idx.push_back(c);
        }
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

// ── Init / Dtor ───────────────────────────────────────────────────────────

void Terrain::initShader() {
    GLuint v = compile(GL_VERTEX_SHADER,   T_VERT);
    GLuint f = compile(GL_FRAGMENT_SHADER, T_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog,v); glAttachShader(prog,f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

void Terrain::loadTextures() {
    stbi_set_flip_vertically_on_load(true);

    int w, h, ch;
    unsigned char* data = stbi_load("assets/textures/grass.jpg", &w, &h, &ch, 3);

    glGenTextures(1, &grassTex);
    glBindTexture(GL_TEXTURE_2D, grassTex);

    if (data) {
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        stbi_image_free(data);
    } else {
        // Fallback: solid green 1×1 texture
        unsigned char green[3] = { 60, 120, 40 };
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 1, 1, 0, GL_RGB, GL_UNSIGNED_BYTE, green);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // Anisotropic filtering (extension constant; 0x84FE/0x84FF are standard values)
    glTexParameterf(GL_TEXTURE_2D, 0x84FF, 8.f);

    glBindTexture(GL_TEXTURE_2D, 0);
}

Terrain::Terrain()  { initShader(); loadTextures(); generate(); }
Terrain::~Terrain() {
    glDeleteProgram(prog);
    if (vao) { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo); }
    if (grassTex) glDeleteTextures(1, &grassTex);
}

// ── Render ────────────────────────────────────────────────────────────────

void Terrain::render(const glm::mat4& VP, glm::vec3 sunDir,
                     float fogDensity, glm::vec3 cameraPos) {
    if (!indexCount) return;

    // Height field has no side faces — disable culling so hills look solid
    glDisable(GL_CULL_FACE);

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"),         1, GL_FALSE, glm::value_ptr(VP));
    glUniform3fv      (glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f       (glGetUniformLocation(prog,"uFogDensity"), fogDensity);
    glUniform3fv      (glGetUniformLocation(prog,"uCamPos"),     1, glm::value_ptr(cameraPos));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, grassTex);
    glUniform1i(glGetUniformLocation(prog,"uGrassTex"), 0);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
}
