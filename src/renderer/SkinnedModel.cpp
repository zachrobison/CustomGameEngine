#include "SkinnedModel.h"
#include "../vendor/tiny_gltf.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cstdio>
#include <cstring>

static const char* SK_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
layout(location=2) in vec4 aJoints;
layout(location=3) in vec4 aWeights;
layout(location=4) in vec3 aColor;    // COLOR_0 vertex paint (white if none)
uniform mat4 uVP;
uniform mat4 uModel;
uniform mat4 uJoints[96];
out vec3 vPos;
out vec3 vNormal;
out vec3 vColor;
void main() {
    mat4 skin = aWeights.x * uJoints[int(aJoints.x)]
              + aWeights.y * uJoints[int(aJoints.y)]
              + aWeights.z * uJoints[int(aJoints.z)]
              + aWeights.w * uJoints[int(aJoints.w)];
    vec4 wp = uModel * skin * vec4(aPos, 1.0);
    vPos     = wp.xyz;
    vNormal  = normalize(mat3(uModel) * mat3(skin) * aNormal);
    vColor   = aColor;
    gl_Position = uVP * wp;
}
)";

static const char* SK_FRAG = R"(
#version 410 core
in vec3 vPos;
in vec3 vNormal;
in vec3 vColor;
out vec4 FragColor;
uniform vec3  uColor;
uniform vec3  uSunDir;
uniform float uFogDensity;
uniform vec3  uCamPos;
void main() {
    vec3  n   = normalize(vNormal);
    float d   = max(dot(n, normalize(uSunDir)), 0.0);
    float lit = 0.35 + 0.65 * d;
    vec3  col = uColor * vColor * lit;   // material tint × vertex paint
    float dist   = length(vPos - uCamPos);
    float fogFac = exp(-uFogDensity * dist * dist);
    vec3  skyCol = vec3(0.62, 0.84, 0.98);
    FragColor = vec4(mix(skyCol, col, clamp(fogFac,0,1)), 1.0);
}
)";

void SkinnedModel::initShader() {
    auto compile = [](GLenum type, const char* src) {
        GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0; glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[1024]; glGetShaderInfoLog(s, sizeof(log), nullptr, log);
            fprintf(stderr, "SkinnedModel shader error: %s\n", log);
        }
        return s;
    };
    GLuint v = compile(GL_VERTEX_SHADER,   SK_VERT);
    GLuint f = compile(GL_FRAGMENT_SHADER, SK_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

void SkinnedModel::cleanup() {
    for (auto& p : prims) {
        if (p.vao) glDeleteVertexArrays(1, &p.vao);
        if (p.vbo) glDeleteBuffers(1, &p.vbo);
        if (p.ebo) glDeleteBuffers(1, &p.ebo);
    }
    prims.clear();
    if (prog) { glDeleteProgram(prog); prog = 0; }
    rest.clear(); pose.clear(); jointNodes.clear();
    inverseBind.clear(); jointMats.clear(); anims.clear();
    curAnim = -1; animT = 0.f;
}

SkinnedModel::~SkinnedModel() { cleanup(); }

// Raw pointer to an accessor's data (assumes tightly packed or strided).
static const unsigned char* accessorData(const tinygltf::Model& m,
                                         const tinygltf::Accessor& acc,
                                         int* strideOut = nullptr) {
    const tinygltf::BufferView& bv = m.bufferViews[acc.bufferView];
    if (strideOut) {
        int def = tinygltf::GetComponentSizeInBytes(acc.componentType)
                * tinygltf::GetNumComponentsInType(acc.type);
        *strideOut = bv.byteStride ? (int)bv.byteStride : def;
    }
    return m.buffers[bv.buffer].data.data() + bv.byteOffset + acc.byteOffset;
}

bool SkinnedModel::load(const std::string& path) {
    cleanup();
    initShader();

    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;
    if (!loader.LoadBinaryFromFile(&model, &err, &warn, path)) {
        fprintf(stderr, "SkinnedModel: parse failed '%s': %s\n", path.c_str(), err.c_str());
        return false;
    }
    if (model.skins.empty()) {
        fprintf(stderr, "SkinnedModel: '%s' has no skin\n", path.c_str());
        return false;
    }

    // ── Node hierarchy (rest pose) ────────────────────────────────────────
    rest.resize(model.nodes.size());
    nodeNames.resize(model.nodes.size());
    for (int i = 0; i < (int)model.nodes.size(); i++) {
        const tinygltf::Node& n = model.nodes[i];
        nodeNames[i] = n.name;
        Node& out = rest[i];
        if (n.translation.size() == 3)
            out.t = {(float)n.translation[0], (float)n.translation[1], (float)n.translation[2]};
        if (n.rotation.size() == 4)
            out.r = glm::quat((float)n.rotation[3], (float)n.rotation[0],
                              (float)n.rotation[1], (float)n.rotation[2]);
        if (n.scale.size() == 3)
            out.s = {(float)n.scale[0], (float)n.scale[1], (float)n.scale[2]};
        for (int c : n.children) rest[c].parent = i;
    }
    // Re-run to fix parents set before child node was initialised
    for (int i = 0; i < (int)model.nodes.size(); i++)
        for (int c : model.nodes[i].children) rest[c].parent = i;
    pose = rest;

    // ── Skin ──────────────────────────────────────────────────────────────
    const tinygltf::Skin& skin = model.skins[0];
    jointNodes.assign(skin.joints.begin(), skin.joints.end());
    if ((int)jointNodes.size() > MAX_JOINTS) {
        fprintf(stderr, "SkinnedModel: %d joints exceeds max %d\n",
                (int)jointNodes.size(), MAX_JOINTS);
        return false;
    }
    inverseBind.resize(jointNodes.size(), glm::mat4(1.f));
    if (skin.inverseBindMatrices >= 0) {
        const tinygltf::Accessor& acc = model.accessors[skin.inverseBindMatrices];
        const float* d = (const float*)accessorData(model, acc);
        for (int i = 0; i < (int)jointNodes.size(); i++)
            memcpy(glm::value_ptr(inverseBind[i]), d + i*16, 16*sizeof(float));
    }
    jointMats.resize(jointNodes.size(), glm::mat4(1.f));

    // ── Mesh primitives ───────────────────────────────────────────────────
    for (int nodeIdx = 0; nodeIdx < (int)model.nodes.size(); nodeIdx++) {
        const tinygltf::Node& n = model.nodes[nodeIdx];
        if (n.mesh < 0) continue;
        // Bone-parented / static mesh nodes have no skin — bind them to a
        // virtual joint set to the node's own global transform so they render
        // and follow the animation (e.g. the tommy gun in the pig's hands).
        int virtualJoint = -1;
        for (const tinygltf::Primitive& prim : model.meshes[n.mesh].primitives) {
            auto need = [&](const char* attr) -> int {
                auto it = prim.attributes.find(attr);
                return it == prim.attributes.end() ? -1 : it->second;
            };
            int aPos = need("POSITION"), aNorm = need("NORMAL");
            int aJnt = need("JOINTS_0"), aWgt = need("WEIGHTS_0");
            int aCol = need("COLOR_0");
            if (aPos < 0) continue;
            bool skinned = (aJnt >= 0 && aWgt >= 0 && n.skin >= 0);
            if (!skinned && virtualJoint < 0) {
                virtualJoint = (int)jointNodes.size();
                jointNodes.push_back(nodeIdx);
                inverseBind.push_back(glm::mat4(1.f));
                jointMats.push_back(glm::mat4(1.f));
            }

            const tinygltf::Accessor& pAcc = model.accessors[aPos];
            int vCount = (int)pAcc.count;
            int pStride; const unsigned char* pD = accessorData(model, pAcc, &pStride);

            int nStride = 12; const unsigned char* nD = nullptr;
            if (aNorm >= 0) nD = accessorData(model, model.accessors[aNorm], &nStride);

            int jStride = 0, wStride = 0, jCompType = 0;
            const unsigned char* jD = nullptr; const unsigned char* wD = nullptr;
            if (skinned) {
                const tinygltf::Accessor& jAcc = model.accessors[aJnt];
                jD = accessorData(model, jAcc, &jStride); jCompType = jAcc.componentType;
                const tinygltf::Accessor& wAcc = model.accessors[aWgt];
                wD = accessorData(model, wAcc, &wStride);
            }

            // Optional vertex color (COLOR_0): vec3/vec4 float or normalized u8/u16
            int cStride = 0, cComps = 0, cCompType = 0;
            const unsigned char* cD = nullptr;
            if (aCol >= 0) {
                const tinygltf::Accessor& cAcc = model.accessors[aCol];
                cD = accessorData(model, cAcc, &cStride);
                cComps    = tinygltf::GetNumComponentsInType(cAcc.type);
                cCompType = cAcc.componentType;
            }

            // Interleave: pos3 norm3 joints4 weights4 color3 = 17 floats
            std::vector<float> verts((size_t)vCount * 17);
            for (int i = 0; i < vCount; i++) {
                float* v = verts.data() + (size_t)i*17;
                const float* p = (const float*)(pD + i*pStride);
                v[0]=p[0]; v[1]=p[1]; v[2]=p[2];
                if (nD) { const float* nn=(const float*)(nD+i*nStride); v[3]=nn[0]; v[4]=nn[1]; v[5]=nn[2]; }
                else    { v[3]=0; v[4]=1; v[5]=0; }
                if (skinned) {
                    for (int k = 0; k < 4; k++) {
                        int j;
                        if (jCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                            j = ((const uint8_t*)(jD + i*jStride))[k];
                        else
                            j = ((const uint16_t*)(jD + i*jStride))[k];
                        v[6+k] = (float)j;
                    }
                    const float* w = (const float*)(wD + i*wStride);
                    float sum = w[0]+w[1]+w[2]+w[3];
                    if (sum <= 0.f) sum = 1.f;
                    for (int k = 0; k < 4; k++) v[10+k] = w[k] / sum;
                } else {
                    // rigid bind to the virtual joint (node's global transform)
                    v[6]=(float)virtualJoint; v[7]=v[8]=v[9]=0.f;
                    v[10]=1.f; v[11]=v[12]=v[13]=0.f;
                }
                // color (default white if absent)
                v[14]=v[15]=v[16]=1.f;
                if (cD) {
                    const unsigned char* cp = cD + i*cStride;
                    for (int k = 0; k < 3 && k < cComps; k++) {
                        if (cCompType == TINYGLTF_COMPONENT_TYPE_FLOAT)
                            v[14+k] = ((const float*)cp)[k];
                        else if (cCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                            v[14+k] = ((const uint8_t*)cp)[k] / 255.f;
                        else if (cCompType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                            v[14+k] = ((const uint16_t*)cp)[k] / 65535.f;
                    }
                }
            }

            std::vector<uint32_t> indices;
            if (prim.indices >= 0) {
                const tinygltf::Accessor& iAcc = model.accessors[prim.indices];
                const unsigned char* iD = accessorData(model, iAcc);
                indices.resize(iAcc.count);
                for (size_t i = 0; i < iAcc.count; i++) {
                    if (iAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        indices[i] = ((const uint16_t*)iD)[i];
                    else if (iAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        indices[i] = iD[i];
                    else
                        indices[i] = ((const uint32_t*)iD)[i];
                }
            } else {
                indices.resize(vCount);
                for (int i = 0; i < vCount; i++) indices[i] = (uint32_t)i;
            }

            Prim gp;
            gp.indexCount = (int)indices.size();
            if (prim.material >= 0) {
                auto& pbr = model.materials[prim.material].pbrMetallicRoughness;
                if (pbr.baseColorFactor.size() >= 3)
                    gp.color = {(float)pbr.baseColorFactor[0],
                                (float)pbr.baseColorFactor[1],
                                (float)pbr.baseColorFactor[2]};
            }
            glGenVertexArrays(1, &gp.vao);
            glGenBuffers(1, &gp.vbo);
            glGenBuffers(1, &gp.ebo);
            glBindVertexArray(gp.vao);
            glBindBuffer(GL_ARRAY_BUFFER, gp.vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)),
                         verts.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gp.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         (GLsizeiptr)(indices.size()*sizeof(uint32_t)),
                         indices.data(), GL_STATIC_DRAW);
            constexpr int stride = 17 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
            glVertexAttribPointer(2, 4, GL_FLOAT, GL_FALSE, stride, (void*)(6*sizeof(float)));
            glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, stride, (void*)(10*sizeof(float)));
            glVertexAttribPointer(4, 3, GL_FLOAT, GL_FALSE, stride, (void*)(14*sizeof(float)));
            for (int k = 0; k < 5; k++) glEnableVertexAttribArray(k);
            glBindVertexArray(0);
            prims.push_back(gp);
        }
    }

    // ── Animations ────────────────────────────────────────────────────────
    for (const tinygltf::Animation& a : model.animations) {
        Anim out;
        out.name = a.name;
        for (const tinygltf::AnimationChannel& ch : a.channels) {
            const tinygltf::AnimationSampler& smp = a.samplers[ch.sampler];
            Channel c;
            c.node = ch.target_node;
            if      (ch.target_path == "translation") c.path = 0;
            else if (ch.target_path == "rotation")    c.path = 1;
            else if (ch.target_path == "scale")       c.path = 2;
            else continue;

            const tinygltf::Accessor& tAcc = model.accessors[smp.input];
            const float* tD = (const float*)accessorData(model, tAcc);
            c.times.assign(tD, tD + tAcc.count);
            if (!c.times.empty())
                out.duration = std::max(out.duration, c.times.back());

            const tinygltf::Accessor& vAcc = model.accessors[smp.output];
            const float* vD = (const float*)accessorData(model, vAcc);
            int comps = tinygltf::GetNumComponentsInType(vAcc.type);
            c.values.resize(vAcc.count, glm::vec4(0.f));
            for (size_t i = 0; i < vAcc.count; i++)
                for (int k = 0; k < comps && k < 4; k++)
                    c.values[i][k] = vD[i*comps + k];
            out.channels.push_back(std::move(c));
        }
        anims.push_back(std::move(out));
    }

    fprintf(stderr, "SkinnedModel: '%s' → %d prim(s), %d joint(s), %d anim(s):",
            path.c_str(), (int)prims.size(), (int)jointNodes.size(), (int)anims.size());
    for (auto& a : anims) fprintf(stderr, " %s", a.name.c_str());
    fprintf(stderr, "\n");

    if (!anims.empty()) curAnim = 0;
    evalPose();
    return !prims.empty();
}

bool SkinnedModel::nodeWorld(const std::string& substr, glm::mat4& out) const {
    auto lower = [](std::string s) {
        for (auto& c : s) c = (char)tolower(c);
        return s;
    };
    std::string want = lower(substr);
    for (int i = 0; i < (int)nodeNames.size(); i++) {
        if (lower(nodeNames[i]).find(want) == std::string::npos) continue;
        if (i < (int)pose.size()) { out = pose[i].global; return true; }
    }
    return false;
}

void SkinnedModel::play(const std::string& animName) {
    for (int i = 0; i < (int)anims.size(); i++) {
        if (anims[i].name == animName) {
            if (curAnim != i) { curAnim = i; animT = 0.f; }
            return;
        }
    }
}

float SkinnedModel::duration() const {
    float dur = 0.f;
    if (combineAll) for (auto& a : anims) dur = std::max(dur, a.duration);
    else if (curAnim >= 0)                dur = anims[curAnim].duration;
    return dur;
}
void SkinnedModel::playSegment(float t0, float t1, bool loop) {
    segActive = true; holding = false; segLoop = loop; segDone = false;
    segStart = t0; segEnd = t1; animT = t0;
}
void SkinnedModel::holdAt(float t) { segActive = false; holding = true; animT = t; }

void SkinnedModel::update(float dt) {
    if (anims.empty()) return;
    if (segActive) {
        animT += dt;
        if (animT >= segEnd) {
            float span = segEnd - segStart;
            if (segLoop && span > 1e-4f) animT = segStart + std::fmod(animT - segStart, span);
            else { animT = segEnd; segDone = true; segActive = false; }
        }
        evalPose(); return;
    }
    if (holding) { evalPose(); return; }   // frozen on one pose
    float dur = duration();
    if (dur > 0.f) { animT += dt; while (animT > dur) animT -= dur; }
    evalPose();
}

void SkinnedModel::evalPose() {
    pose = rest;
    // Stop-motion: quantise the sample clock to snapFps poses per second
    float sampleT = animT;
    if (snapFps > 0.f) sampleT = std::floor(animT * snapFps) / snapFps;
    // Apply one clip's channels at the current time.
    auto applyAnim = [&](const Anim& a) {
        for (const Channel& c : a.channels) {
            if (c.node < 0 || c.node >= (int)pose.size() || c.times.empty()) continue;
            size_t hi = 0;
            while (hi < c.times.size() && c.times[hi] < sampleT) hi++;
            size_t lo = hi > 0 ? hi - 1 : 0;
            if (hi >= c.times.size()) hi = c.times.size() - 1;
            float span = c.times[hi] - c.times[lo];
            float f    = span > 0.f ? (sampleT - c.times[lo]) / span : 0.f;

            if (c.path == 0) {
                glm::vec4 v = glm::mix(c.values[lo], c.values[hi], f);
                pose[c.node].t = {v.x, v.y, v.z};
            } else if (c.path == 1) {
                glm::quat qa(c.values[lo].w, c.values[lo].x, c.values[lo].y, c.values[lo].z);
                glm::quat qb(c.values[hi].w, c.values[hi].x, c.values[hi].y, c.values[hi].z);
                pose[c.node].r = glm::slerp(qa, qb, f);
            } else {
                glm::vec4 v = glm::mix(c.values[lo], c.values[hi], f);
                pose[c.node].s = {v.x, v.y, v.z};
            }
        }
    };
    if (combineAll)        for (auto& a : anims) applyAnim(a);   // all layers
    else if (curAnim >= 0) applyAnim(anims[curAnim]);

    // Global transforms: nodes are stored parent-before-child by the
    // exporter, but don't rely on it — resolve through parents iteratively.
    std::vector<char> done(pose.size(), 0);
    for (size_t pass = 0; pass < pose.size(); pass++) {
        bool progress = false, remaining = false;
        for (size_t i = 0; i < pose.size(); i++) {
            if (done[i]) continue;
            int p = pose[i].parent;
            if (p >= 0 && !done[p]) { remaining = true; continue; }
            glm::mat4 local = glm::translate(glm::mat4(1.f), pose[i].t)
                            * glm::mat4_cast(pose[i].r)
                            * glm::scale(glm::mat4(1.f), pose[i].s);
            pose[i].global = (p >= 0 ? pose[p].global : glm::mat4(1.f)) * local;
            done[i] = 1; progress = true;
        }
        if (!remaining || !progress) break;
    }

    for (int i = 0; i < (int)jointNodes.size(); i++)
        jointMats[i] = pose[jointNodes[i]].global * inverseBind[i];
}

void SkinnedModel::render(const glm::mat4& VP, glm::vec3 worldPos, float yaw, float scale,
                          glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                          glm::vec3 tint) {
    glm::mat4 modelM = glm::translate(glm::mat4(1.f), worldPos)
                     * glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0,1,0))
                     * glm::scale(glm::mat4(1.f), glm::vec3(scale));
    renderMatrix(VP, modelM, sunDir, fogDensity, camPos, tint);
}

void SkinnedModel::renderMatrix(const glm::mat4& VP, const glm::mat4& modelM,
                                glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                                glm::vec3 tint) {
    if (!prog || prims.empty()) return;
    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uVP"),    1, GL_FALSE, glm::value_ptr(VP));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"), 1, GL_FALSE, glm::value_ptr(modelM));
    glUniformMatrix4fv(glGetUniformLocation(prog,"uJoints"),
                       (GLsizei)jointMats.size(), GL_FALSE,
                       glm::value_ptr(jointMats[0]));
    glUniform3fv(glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f (glGetUniformLocation(prog,"uFogDensity"), fogDensity);
    glUniform3fv(glGetUniformLocation(prog,"uCamPos"),     1, glm::value_ptr(camPos));

    for (auto& p : prims) {
        glm::vec3 c = p.color * tint;
        glUniform3fv(glGetUniformLocation(prog,"uColor"), 1, glm::value_ptr(c));
        glBindVertexArray(p.vao);
        glDrawElements(GL_TRIANGLES, p.indexCount, GL_UNSIGNED_INT, nullptr);
    }
    glBindVertexArray(0);
}
