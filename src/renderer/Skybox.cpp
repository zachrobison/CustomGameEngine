#include "Skybox.h"
#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <stdexcept>

// ── Inline shaders ────────────────────────────────────────────────────────────

static const char* SKY_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
uniform mat4 uProjection;
uniform mat4 uView;        // translation stripped before upload
out vec3 vDir;
void main() {
    vDir = aPos;
    // xyww trick: forces depth to 1.0 (behind everything)
    vec4 pos = uProjection * uView * vec4(aPos, 1.0);
    gl_Position = pos.xyww;
}
)";

static const char* SKY_FRAG = R"(
#version 410 core
in  vec3 vDir;
out vec4 FragColor;

uniform vec3  uSunDir;
uniform float uSunHeight; // sin of elevation angle, -1..1

// ── Simple hash-based noise for clouds ───────────────────────────────────
float hash(vec2 p) {
    p = fract(p * vec2(127.1, 311.7));
    p += dot(p, p + 19.19);
    return fract(p.x * p.y);
}
float noise(vec2 p) {
    vec2 i = floor(p), f = fract(p);
    f = f*f*(3.0-2.0*f);
    return mix(mix(hash(i),          hash(i+vec2(1,0)), f.x),
               mix(hash(i+vec2(0,1)),hash(i+vec2(1,1)), f.x), f.y);
}
float fbm(vec2 p) {
    float v = 0.0, a = 0.5;
    for (int i = 0; i < 5; i++) { v += a*noise(p); p *= 2.1; a *= 0.5; }
    return v;
}

void main() {
    vec3 dir = normalize(vDir);

    // ── Base sky gradient ─────────────────────────────────────────────────
    float up = clamp(dir.y, -1.0, 1.0);

    vec3 zenithDay  = vec3(0.08, 0.35, 0.82);
    vec3 zenithDusk = vec3(0.06, 0.07, 0.20);
    vec3 zenith     = mix(zenithDusk, zenithDay, clamp(uSunHeight + 0.3, 0.0, 1.0));

    vec3 horizDay   = vec3(0.62, 0.84, 0.98);
    vec3 horizDusk  = vec3(0.80, 0.38, 0.12);
    vec3 horiz      = mix(horizDusk, horizDay, clamp(uSunHeight + 0.2, 0.0, 1.0));

    vec3 underground = vec3(0.18, 0.16, 0.20);

    vec3 sky;
    if (up >= 0.0)
        sky = mix(horiz, zenith, pow(up, 0.45));
    else
        sky = mix(horiz, underground, clamp(-up * 2.5, 0.0, 1.0));

    // Night darkening
    float night = clamp(-uSunHeight * 1.5, 0.0, 1.0);
    sky = mix(sky, vec3(0.01, 0.02, 0.08), night);

    // ── Clouds ───────────────────────────────────────────────────────────
    if (dir.y > 0.05) {
        // Project onto a cloud plane at height 1
        vec2 uv = dir.xz / dir.y * 0.35;
        float cloud = fbm(uv + vec2(0.3, 0.7));
        cloud = smoothstep(0.42, 0.72, cloud);
        // Clouds thin out near horizon and at night
        float horizFade = clamp((dir.y - 0.05) * 6.0, 0.0, 1.0);
        float dayFade   = clamp(uSunHeight * 3.0, 0.0, 1.0);
        cloud *= horizFade * dayFade;

        vec3 cloudCol = mix(vec3(0.75, 0.78, 0.82),  // shadow side
                            vec3(0.97, 0.97, 0.99),   // lit side
                            clamp(uSunHeight + 0.4, 0.0, 1.0));
        sky = mix(sky, cloudCol, cloud * 0.88);
    }

    // ── Sun disk + glow ───────────────────────────────────────────────────
    float sun  = max(dot(dir, normalize(uSunDir)), 0.0);
    float disk = pow(sun, 3500.0);
    float glow = pow(sun,   12.0) * 0.35;
    float halo = pow(sun,    4.0) * 0.08;

    vec3 sunCol = mix(vec3(1.0, 0.95, 0.85), vec3(1.0, 0.55, 0.15),
                      clamp(1.0 - uSunHeight, 0.0, 1.0));
    sky += sunCol * disk;
    sky += sunCol * glow;
    sky += vec3(1.0, 0.65, 0.25) * halo;

    // ── Horizon scatter ───────────────────────────────────────────────────
    vec3  sunFlat = normalize(vec3(uSunDir.x, 0.0, uSunDir.z));
    vec3  dirFlat = normalize(vec3(dir.x,     0.0, dir.z));
    float scatter   = pow(max(dot(dirFlat, sunFlat), 0.0), 5.0);
    float nearHoriz = clamp(1.0 - abs(dir.y) * 2.5, 0.0, 1.0);
    sky += vec3(0.45, 0.18, 0.04) * scatter * nearHoriz
         * clamp(1.0 - uSunHeight, 0.0, 1.0);

    FragColor = vec4(sky, 1.0);
}
)";

// ── Cube geometry ─────────────────────────────────────────────────────────────

static const float VERTS[24] = {
    -1,-1,-1,  1,-1,-1,  1, 1,-1, -1, 1,-1,
    -1,-1, 1,  1,-1, 1,  1, 1, 1, -1, 1, 1
};

// CCW from outside (GL_BACK culling disabled during skybox render)
static const uint8_t IDX[36] = {
    0,1,2, 0,2,3,   // -Z
    5,4,7, 5,7,6,   // +Z
    4,0,3, 4,3,7,   // -X
    1,5,6, 1,6,2,   // +X
    4,5,1, 4,1,0,   // -Y
    3,2,6, 3,6,7    // +Y
};

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) throw std::runtime_error("Skybox shader compile failed");
    return s;
}

Skybox::Skybox() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   SKY_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, SKY_FRAG);
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
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(IDX), IDX, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

Skybox::~Skybox() {
    glDeleteProgram(prog);
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

void Skybox::render(const Camera& camera, float aspect, glm::vec3 sunDir) {
    // Strip translation so sky follows camera
    glm::mat4 viewRot = glm::mat4(glm::mat3(camera.getView()));
    glm::mat4 proj    = camera.getProjection(aspect);

    float sunHeight = glm::normalize(sunDir).y;

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uProjection"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uView"),       1, GL_FALSE, glm::value_ptr(viewRot));
    glUniform3fv      (glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f       (glGetUniformLocation(prog,"uSunHeight"),  sunHeight);

    glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);  // xyww writes depth=1.0; needs LEQUAL to pass
    glDisable(GL_CULL_FACE);

    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_BYTE, nullptr);
    glBindVertexArray(0);

    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
}
