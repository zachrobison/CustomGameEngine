#include "CharacterRenderer.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <vector>
#include <stdexcept>
#include <cstring>

// ── Inline shaders ────────────────────────────────────────────────────────

static const char* CHAR_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec3 aColor;
uniform mat4 uMVP;
out vec3 vNormal;
out vec3 vColor;
void main() {
    vNormal = aNormal;
    vColor  = aColor;
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* CHAR_FRAG = R"(
#version 410 core
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
void main() {
    vec3 sun = normalize(vec3(0.6, 1.0, 0.5));
    float diff    = max(dot(vNormal, sun), 0.0);
    float ambient = 0.38;
    // face-direction tint
    float faceMult = 1.0;
    if (vNormal.y >  0.5) faceMult = 1.10;
    if (vNormal.y < -0.5) faceMult = 0.55;
    float light = (ambient + (1.0 - ambient) * diff) * faceMult;
    FragColor = vec4(vColor * light, 1.0);
}
)";

// ── Clothing presets per slot ─────────────────────────────────────────────

struct ClothingPreset {
    const char* name;
    float padX, padY, padZ;   // extra size on each axis beyond the body part
    float extraTop;            // additional height above the part (hat brim etc.)
    float extraBot;            // additional height below (boots etc.)
};

static const ClothingPreset HEAD_PRESETS[] = {
    {"None",      0,     0,    0,    0,    0   },
    {"Basic Hat", 0.07f, 0.05f,0.07f,0.40f,0   },
    {"Helmet",    0.09f, 0.09f,0.09f,0,    0   },
    {"Crown",     0.05f, 0.30f,0.05f,0.55f,0   },
    {"Hood",      0.11f, 0.07f,0.11f,0.10f,0.20f},
    {"Beanie",    0.05f, 0.18f,0.05f,0.35f,0   },
};
static const ClothingPreset TORSO_PRESETS[] = {
    {"None",         0,    0,    0,    0, 0   },
    {"T-Shirt",      0.06f,0.06f,0.06f,0, 0   },
    {"Hoodie",       0.10f,0.10f,0.10f,0, 0   },
    {"Chest Armor",  0.12f,0.12f,0.12f,0, 0   },
    {"Jacket",       0.10f,0.08f,0.10f,0, 0.15f},
    {"Coat",         0.08f,0.06f,0.08f,0, 0.50f},
};
static const ClothingPreset ARMS_PRESETS[] = {
    {"None",       0,    0,    0,    0,    0    },
    {"Short Sleeves",0.06f,0.04f,0.06f,0,  0    },
    {"Long Sleeves", 0.07f,0.07f,0.07f,0,  0    },
    {"Bracers",      0.10f,0.10f,0.10f,0,  0    },
    {"Gloves",       0.05f,0.05f,0.05f,0,  0.05f},
};
static const ClothingPreset LEGS_PRESETS[] = {
    {"None",       0,    0,    0, 0, 0    },
    {"Pants",      0.06f,0.06f,0.06f,0,0  },
    {"Shorts",     0.07f,0.07f,0.07f,0,0  },
    {"Skirt",      0.15f,0.06f,0.15f,0,0  },
    {"Leg Armor",  0.12f,0.10f,0.12f,0,0  },
};
static const ClothingPreset FEET_PRESETS[] = {
    {"None",       0,    0,    0, 0,    0    },
    {"Shoes",      0.07f,0.05f,0.09f,0,0.10f},
    {"Boots",      0.07f,0.05f,0.09f,0,0.30f},
    {"Sandals",    0.04f,0.02f,0.07f,0,0.08f},
    {"Armor Boots",0.12f,0.08f,0.12f,0,0.35f},
};

static const ClothingPreset* SLOT_PRESETS[(int)ClothingSlot::COUNT] = {
    HEAD_PRESETS, TORSO_PRESETS, ARMS_PRESETS, LEGS_PRESETS, FEET_PRESETS
};
static const int SLOT_PRESET_COUNTS[(int)ClothingSlot::COUNT] = {
    6, 6, 5, 5, 5
};

// Public accessor for the panel UI
int  characterPresetCount(ClothingSlot s)          { return SLOT_PRESET_COUNTS[(int)s]; }
const char* characterPresetName(ClothingSlot s, int i) { return SLOT_PRESETS[(int)s][i].name; }

// ── Box mesh builder ──────────────────────────────────────────────────────

// Vertex: pos(3) + normal(3) + color(3) = 9 floats
static void addBox(std::vector<float>& V, std::vector<uint32_t>& I,
                   glm::vec3 center, glm::vec3 size, glm::vec3 color) {
    glm::vec3 h = size * 0.5f;

    // 6 faces: +X,-X,+Y,-Y,+Z,-Z
    static const glm::vec3 normals[6] = {
        {1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}
    };
    // 4 corners per face (in (+X right, +Y up) orientation for each face)
    // face 0 (+X): right=+Z, up=+Y, pos.x=+h.x
    // face 1 (-X): right=-Z, up=+Y, pos.x=-h.x
    // etc.
    static const float FACE_CORNERS[6][4][3] = {
        {{ h.x, -h.y, -h.z},{ h.x, -h.y,  h.z},{ h.x,  h.y,  h.z},{ h.x,  h.y, -h.z}},  // +X
        {{-h.x, -h.y,  h.z},{-h.x, -h.y, -h.z},{-h.x,  h.y, -h.z},{-h.x,  h.y,  h.z}},  // -X
        {{-h.x,  h.y, -h.z},{ h.x,  h.y, -h.z},{ h.x,  h.y,  h.z},{-h.x,  h.y,  h.z}},  // +Y
        {{-h.x, -h.y,  h.z},{ h.x, -h.y,  h.z},{ h.x, -h.y, -h.z},{-h.x, -h.y, -h.z}},  // -Y
        {{ h.x, -h.y,  h.z},{-h.x, -h.y,  h.z},{-h.x,  h.y,  h.z},{ h.x,  h.y,  h.z}},  // +Z
        {{-h.x, -h.y, -h.z},{ h.x, -h.y, -h.z},{ h.x,  h.y, -h.z},{-h.x,  h.y, -h.z}},  // -Z
    };

    for (int f = 0; f < 6; f++) {
        uint32_t base = (uint32_t)(V.size() / 9);
        for (int c = 0; c < 4; c++) {
            V.push_back(center.x + FACE_CORNERS[f][c][0]);
            V.push_back(center.y + FACE_CORNERS[f][c][1]);
            V.push_back(center.z + FACE_CORNERS[f][c][2]);
            V.push_back(normals[f].x);
            V.push_back(normals[f].y);
            V.push_back(normals[f].z);
            V.push_back(color.r);
            V.push_back(color.g);
            V.push_back(color.b);
        }
        I.insert(I.end(), {base,base+1,base+2, base,base+2,base+3});
    }
}

// ── Pose computation ──────────────────────────────────────────────────────

struct Pose {
    glm::vec3 pos[(int)BodyPart::COUNT];
    glm::vec3 sz [(int)BodyPart::COUNT]; // actual rendered size after scale
};

// Default base sizes (unscaled, in world feet)
static const glm::vec3 BASE_SIZE[(int)BodyPart::COUNT] = {
    {1.10f, 1.10f, 1.10f},  // Head
    {1.50f, 2.00f, 0.90f},  // Torso
    {0.65f, 1.85f, 0.65f},  // LeftArm
    {0.65f, 1.85f, 0.65f},  // RightArm
    {0.72f, 2.20f, 0.72f},  // LeftLeg
    {0.72f, 2.20f, 0.72f},  // RightLeg
};

static Pose computePose(const Character& c) {
    Pose p;
    for (int i = 0; i < (int)BodyPart::COUNT; i++)
        p.sz[i] = BASE_SIZE[i] * c.scales[i].v;

    glm::vec3& ts = p.sz[(int)BodyPart::Torso];
    glm::vec3& hs = p.sz[(int)BodyPart::Head];
    glm::vec3& ls = p.sz[(int)BodyPart::LeftLeg];
    glm::vec3& as = p.sz[(int)BodyPart::LeftArm];

    float legsTop = ls.y;                              // legs start at y=0
    float torsoBot = legsTop;
    float torsoTop = torsoBot + ts.y;

    p.pos[(int)BodyPart::LeftLeg]  = { -ts.x * 0.25f, ls.y * 0.5f,      0 };
    p.pos[(int)BodyPart::RightLeg] = {  ts.x * 0.25f, ls.y * 0.5f,      0 };
    p.pos[(int)BodyPart::Torso]    = { 0,              torsoBot + ts.y * 0.5f, 0 };
    p.pos[(int)BodyPart::Head]     = { 0,              torsoTop + hs.y * 0.5f, 0 };

    // Arms hang from the shoulder (top quarter of torso)
    float shoulderY = torsoBot + ts.y * 0.80f;
    float armOffX   = ts.x * 0.5f + as.x * 0.5f;
    p.pos[(int)BodyPart::LeftArm]  = { -armOffX, shoulderY - as.y * 0.5f, 0 };
    p.pos[(int)BodyPart::RightArm] = {  armOffX, shoulderY - as.y * 0.5f, 0 };

    return p;
}

// ── Mesh rebuild ──────────────────────────────────────────────────────────

void CharacterRenderer::rebuild(const Character& c) {
    std::vector<float>    V;
    std::vector<uint32_t> I;
    V.reserve(4096);
    I.reserve(6144);

    Pose pose = computePose(c);

    auto part = [&](BodyPart bp, glm::vec3 color) {
        addBox(V, I, pose.pos[(int)bp], pose.sz[(int)bp], color);
    };

    auto clothed = [&](BodyPart bp, ClothingSlot slot, glm::vec3 skinCol) {
        // Inner skin layer
        part(bp, skinCol);

        // Outer clothing layer (slightly larger box)
        const ClothingConfig& cfg = c.clothing[(int)slot];
        if (!cfg.on()) return;
        const ClothingPreset& pr = SLOT_PRESETS[(int)slot][cfg.preset];

        glm::vec3 innerSz = pose.sz[(int)bp];
        glm::vec3 outerSz = {
            innerSz.x + pr.padX * 2.f,
            innerSz.y + pr.padY * 2.f + pr.extraTop + pr.extraBot,
            innerSz.z + pr.padZ * 2.f
        };
        glm::vec3 outerPos = pose.pos[(int)bp];
        outerPos.y += (pr.extraTop - pr.extraBot) * 0.5f;

        addBox(V, I, outerPos, outerSz, cfg.color);
    };

    // Build mesh: skin-colored parts, then clothing overlays
    glm::vec3 skin   = c.skinColor;
    glm::vec3 hair   = skin * 0.45f;  // darker for hair patch

    clothed(BodyPart::Head,     ClothingSlot::Head,  skin);
    clothed(BodyPart::Torso,    ClothingSlot::Torso, skin);
    clothed(BodyPart::LeftArm,  ClothingSlot::Arms,  skin);
    clothed(BodyPart::RightArm, ClothingSlot::Arms,  skin);
    clothed(BodyPart::LeftLeg,  ClothingSlot::Legs,  skin * 0.85f);
    clothed(BodyPart::RightLeg, ClothingSlot::Legs,  skin * 0.85f);

    // Feet (bottom of legs)
    const ClothingConfig& feetCfg = c.clothing[(int)ClothingSlot::Feet];
    if (feetCfg.on()) {
        const ClothingPreset& pr = SLOT_PRESETS[(int)ClothingSlot::Feet][feetCfg.preset];
        for (BodyPart leg : { BodyPart::LeftLeg, BodyPart::RightLeg }) {
            glm::vec3 legSz  = pose.sz[(int)leg];
            glm::vec3 legPos = pose.pos[(int)leg];
            glm::vec3 footSz = { legSz.x + pr.padX*2, pr.extraBot + pr.padY, legSz.z + pr.padZ*2 };
            glm::vec3 footPos = { legPos.x, pr.extraBot * 0.5f, legPos.z };
            addBox(V, I, footPos, footSz, feetCfg.color);
        }
    }

    // Upload
    if (!vao) {
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        glGenBuffers(1, &ebo);
    }
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(V.size() * sizeof(float)), V.data(), GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(I.size() * sizeof(uint32_t)), I.data(), GL_DYNAMIC_DRAW);

    // pos(3) + normal(3) + color(3) = 9 floats per vertex, stride = 36 bytes
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9*sizeof(float), (void*)(6*sizeof(float)));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
    indexCount = (int)I.size();
}

// ── Setup ─────────────────────────────────────────────────────────────────

static GLuint compileShader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) throw std::runtime_error("CharacterRenderer shader failed");
    return s;
}

void CharacterRenderer::initShader() {
    GLuint v = compileShader(GL_VERTEX_SHADER,   CHAR_VERT);
    GLuint f = compileShader(GL_FRAGMENT_SHADER, CHAR_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog,v); glAttachShader(prog,f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

void CharacterRenderer::initFBO() {
    glGenFramebuffers(1, &fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo);

    glGenTextures(1, &colorTex);
    glBindTexture(GL_TEXTURE_2D, colorTex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, PREVIEW_W, PREVIEW_H, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                           GL_TEXTURE_2D, colorTex, 0);

    glGenRenderbuffers(1, &depthRbo);
    glBindRenderbuffer(GL_RENDERBUFFER, depthRbo);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, PREVIEW_W, PREVIEW_H);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depthRbo);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

CharacterRenderer::CharacterRenderer() {
    initShader();
    initFBO();
}

CharacterRenderer::~CharacterRenderer() {
    glDeleteProgram(prog);
    if (vao) { glDeleteVertexArrays(1,&vao); glDeleteBuffers(1,&vbo); glDeleteBuffers(1,&ebo); }
    glDeleteFramebuffers(1,&fbo);
    glDeleteTextures(1,&colorTex);
    glDeleteRenderbuffers(1,&depthRbo);
}

// ── Drawing ───────────────────────────────────────────────────────────────

void CharacterRenderer::drawMesh(const glm::mat4& MVP) {
    if (!indexCount) return;
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"), 1, GL_FALSE, glm::value_ptr(MVP));
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, indexCount, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

GLuint CharacterRenderer::renderPreview(const Character& c, float rotY) {
    // Save GL state
    GLint prevFBO = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    GLint prevVP[4];
    glGetIntegerv(GL_VIEWPORT, prevVP);

    glBindFramebuffer(GL_FRAMEBUFFER, fbo);
    glViewport(0, 0, PREVIEW_W, PREVIEW_H);
    glClearColor(0.13f, 0.13f, 0.18f, 1.f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);

    // Camera looks at chest-height centre of the character
    Pose pose = computePose(c);
    float charH = pose.pos[(int)BodyPart::Head].y + pose.sz[(int)BodyPart::Head].y * 0.5f;
    float lookAtY = charH * 0.55f;
    float camDist  = charH * 1.35f;

    glm::mat4 proj = glm::perspective(glm::radians(42.f),
                                       (float)PREVIEW_W / (float)PREVIEW_H,
                                       0.1f, 50.f);
    glm::mat4 view = glm::lookAt(
        glm::vec3(0, lookAtY, -camDist),   // camera in front (facing +Z side)
        glm::vec3(0, lookAtY, 0),
        glm::vec3(0, 1, 0));
    glm::mat4 model = glm::rotate(glm::mat4(1.f), rotY, glm::vec3(0,1,0));

    drawMesh(proj * view * model);

    // Restore GL state
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevVP[0], prevVP[1], prevVP[2], prevVP[3]);

    return colorTex;
}

void CharacterRenderer::renderWorld(const glm::mat4& VP, glm::vec3 pos, float rotY) {
    glm::mat4 model = glm::translate(glm::mat4(1.f), pos)
                    * glm::rotate(glm::mat4(1.f), rotY, glm::vec3(0,1,0));
    drawMesh(VP * model);
}
