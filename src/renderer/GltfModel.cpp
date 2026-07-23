#include "GltfModel.h"
#include "../vendor/tiny_gltf.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <glm/gtc/quaternion.hpp>
#include <stdexcept>
#include <cstring>
#include <fstream>
#include <sstream>
#include <algorithm>

static const char* GM_VERT = R"(
#version 410 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNormal;
uniform mat4 uMVP;
uniform mat4 uModel;
out vec3 vPos;
out vec3 vNormal;
void main() {
    vec4 wp   = uModel * vec4(aPos, 1.0);
    vPos      = wp.xyz;
    vNormal   = normalize(mat3(uModel) * aNormal);
    gl_Position = uMVP * vec4(aPos, 1.0);
}
)";

static const char* GM_FRAG = R"(
#version 410 core
in vec3 vPos;
in vec3 vNormal;
out vec4 FragColor;
uniform vec3  uColor;
uniform vec3  uSunDir;
uniform float uFogDensity;
uniform vec3  uCamPos;
void main() {
    vec3  n   = normalize(vNormal);
    float d   = max(dot(n, normalize(uSunDir)), 0.0);
    float lit = 0.35 + 0.65 * d;
    vec3  col = uColor * lit;
    float dist   = length(vPos - uCamPos);
    float fogFac = exp(-uFogDensity * dist * dist);
    vec3  skyCol = vec3(0.62, 0.84, 0.98);
    FragColor = vec4(mix(skyCol, col, clamp(fogFac,0,1)), 1.0);
}
)";

static GLuint compileGM(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    return s;
}

void GltfModel::initShader() {
    GLuint v = compileGM(GL_VERTEX_SHADER,   GM_VERT);
    GLuint f = compileGM(GL_FRAGMENT_SHADER, GM_FRAG);
    prog = glCreateProgram();
    glAttachShader(prog, v); glAttachShader(prog, f);
    glLinkProgram(prog);
    glDeleteShader(v); glDeleteShader(f);
}

void GltfModel::cleanup() {
    for (auto& m : meshes) {
        if (m.vao) glDeleteVertexArrays(1, &m.vao);
        if (m.vbo) glDeleteBuffers(1, &m.vbo);
        if (m.ebo) glDeleteBuffers(1, &m.ebo);
    }
    meshes.clear();
    if (prog) { glDeleteProgram(prog); prog = 0; }
}

GltfModel::~GltfModel() { cleanup(); }

GltfModel::GltfModel(GltfModel&& o) noexcept
    : meshes(std::move(o.meshes)), prog(o.prog) {
    o.meshes.clear();
    o.prog = 0;
}

GltfModel& GltfModel::operator=(GltfModel&& o) noexcept {
    if (this != &o) {
        cleanup();
        meshes = std::move(o.meshes);
        prog   = o.prog;
        o.meshes.clear();
        o.prog = 0;
    }
    return *this;
}

// Build a node's local matrix from TRS or matrix fields.
static glm::mat4 nodeLocalMatrix(const tinygltf::Node& node) {
    if (node.matrix.size() == 16) {
        glm::mat4 m;
        for (int c = 0; c < 4; c++)
            for (int r = 0; r < 4; r++)
                m[c][r] = (float)node.matrix[c*4+r];
        return m;
    }
    glm::mat4 T(1.f), R(1.f), S(1.f);
    if (node.translation.size() == 3)
        T = glm::translate(glm::mat4(1.f), {(float)node.translation[0],
                                            (float)node.translation[1],
                                            (float)node.translation[2]});
    if (node.rotation.size() == 4) {
        glm::quat q((float)node.rotation[3], (float)node.rotation[0],
                    (float)node.rotation[1], (float)node.rotation[2]);
        R = glm::mat4_cast(q);
    }
    if (node.scale.size() == 3)
        S = glm::scale(glm::mat4(1.f), {(float)node.scale[0],
                                        (float)node.scale[1],
                                        (float)node.scale[2]});
    return T * R * S;
}

// Recursively traverse nodes. Vertex positions are kept in local mesh space;
// the accumulated node transform is stored in GpuMesh::localXform and applied
// per draw call in render() so the model is always centred at worldPos.
static void processNode(const tinygltf::Model& model,
                        int nodeIdx,
                        const glm::mat4& parentXform,
                        std::vector<GltfModel::GpuMesh>& out)
{
    const tinygltf::Node& node  = model.nodes[nodeIdx];
    glm::mat4             xform = parentXform * nodeLocalMatrix(node);

    if (node.mesh >= 0) {
        const tinygltf::Mesh& mesh = model.meshes[node.mesh];
        for (auto& prim : mesh.primitives) {
            int mode = (prim.mode < 0) ? TINYGLTF_MODE_TRIANGLES : prim.mode;
            if (mode != TINYGLTF_MODE_TRIANGLES) continue;

            auto posIt = prim.attributes.find("POSITION");
            if (posIt == prim.attributes.end()) continue;

            // Positions (raw, not transformed — applied via uniform in render)
            const tinygltf::Accessor&   posAcc = model.accessors[posIt->second];
            const tinygltf::BufferView& posBV  = model.bufferViews[posAcc.bufferView];
            const unsigned char* posData =
                model.buffers[posBV.buffer].data.data() + posBV.byteOffset + posAcc.byteOffset;
            int posStride = posBV.byteStride ? (int)posBV.byteStride : 12;

            // Normals (optional)
            const unsigned char* normData = nullptr; int normStride = 12;
            auto normIt = prim.attributes.find("NORMAL");
            if (normIt != prim.attributes.end()) {
                const tinygltf::Accessor&   nAcc = model.accessors[normIt->second];
                const tinygltf::BufferView& nBV  = model.bufferViews[nAcc.bufferView];
                normData   = model.buffers[nBV.buffer].data.data() + nBV.byteOffset + nAcc.byteOffset;
                normStride = nBV.byteStride ? (int)nBV.byteStride : 12;
            }

            int vCount = (int)posAcc.count;
            std::vector<float> verts(vCount * 6);
            for (int i = 0; i < vCount; i++) {
                const float* p = (const float*)(posData + i * posStride);
                verts[i*6+0] = p[0]; verts[i*6+1] = p[1]; verts[i*6+2] = p[2];
                if (normData) {
                    const float* n = (const float*)(normData + i * normStride);
                    verts[i*6+3] = n[0]; verts[i*6+4] = n[1]; verts[i*6+5] = n[2];
                } else {
                    verts[i*6+3] = 0; verts[i*6+4] = 1; verts[i*6+5] = 0;
                }
            }

            // Indices — generate sequential if absent
            std::vector<uint32_t> indices;
            if (prim.indices >= 0) {
                const tinygltf::Accessor&   idxAcc = model.accessors[prim.indices];
                const tinygltf::BufferView& idxBV  = model.bufferViews[idxAcc.bufferView];
                const unsigned char* idxData =
                    model.buffers[idxBV.buffer].data.data() + idxBV.byteOffset + idxAcc.byteOffset;
                int iCount = (int)idxAcc.count;
                indices.resize(iCount);
                for (int i = 0; i < iCount; i++) {
                    if      (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT)
                        indices[i] = ((const uint16_t*)idxData)[i];
                    else if (idxAcc.componentType == TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE)
                        indices[i] = idxData[i];
                    else
                        indices[i] = ((const uint32_t*)idxData)[i];
                }
            } else {
                indices.resize(vCount);
                for (int i = 0; i < vCount; i++) indices[i] = (uint32_t)i;
            }

            glm::vec3 color = {0.75f, 0.72f, 0.68f};
            if (prim.material >= 0) {
                auto& pbr = model.materials[prim.material].pbrMetallicRoughness;
                if (pbr.baseColorFactor.size() >= 3)
                    color = { (float)pbr.baseColorFactor[0],
                              (float)pbr.baseColorFactor[1],
                              (float)pbr.baseColorFactor[2] };
            }

            GltfModel::GpuMesh gm;
            gm.color      = color;
            gm.localXform = xform;   // stored, applied per draw call
            gm.indexCount = (int)indices.size();
            gm.indexType  = GL_UNSIGNED_INT;

            glGenVertexArrays(1, &gm.vao);
            glGenBuffers(1, &gm.vbo);
            glGenBuffers(1, &gm.ebo);
            glBindVertexArray(gm.vao);
            glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
            glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)),
                         verts.data(), GL_STATIC_DRAW);
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                         (GLsizeiptr)(indices.size()*sizeof(uint32_t)),
                         indices.data(), GL_STATIC_DRAW);
            constexpr int stride = 6 * sizeof(float);
            glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
            glEnableVertexAttribArray(0);
            glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
            glEnableVertexAttribArray(1);
            glBindVertexArray(0);
            out.push_back(gm);
        }
    }

    for (int child : node.children)
        processNode(model, child, xform, out);
}

static bool loadObj(const std::string& path, std::vector<GltfModel::GpuMesh>& out) {
    std::ifstream f(path);
    if (!f) { fprintf(stderr, "GltfModel OBJ: cannot open '%s'\n", path.c_str()); return false; }

    std::vector<glm::vec3> positions, normals;
    // face indices: position index, normal index (-1 = absent)
    struct FaceVert { int p, n; };
    std::vector<FaceVert> faceVerts;

    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string tok; ss >> tok;
        if (tok == "v") {
            glm::vec3 v; ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (tok == "vn") {
            glm::vec3 n; ss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (tok == "f") {
            std::vector<FaceVert> poly;
            std::string vstr;
            while (ss >> vstr) {
                FaceVert fv{0, -1};
                // formats: v   v/vt   v//vn   v/vt/vn
                size_t s1 = vstr.find('/');
                fv.p = std::stoi(vstr.substr(0, s1)) - 1;
                if (s1 != std::string::npos) {
                    size_t s2 = vstr.find('/', s1 + 1);
                    if (s2 != std::string::npos && s2 > s1 + 1)
                        fv.n = std::stoi(vstr.substr(s2 + 1)) - 1;
                    else if (s2 == std::string::npos && s1 + 1 < vstr.size())
                        ; // vt only, ignore
                }
                poly.push_back(fv);
            }
            // fan-triangulate
            for (int i = 1; i + 1 < (int)poly.size(); i++) {
                faceVerts.push_back(poly[0]);
                faceVerts.push_back(poly[i]);
                faceVerts.push_back(poly[i+1]);
            }
        }
    }

    if (positions.empty() || faceVerts.empty()) {
        fprintf(stderr, "GltfModel OBJ: no geometry in '%s'\n", path.c_str());
        return false;
    }

    // Build flat interleaved buffer (no index sharing — simpler, fine for static meshes)
    int vCount = (int)faceVerts.size();
    std::vector<float> verts(vCount * 6);
    std::vector<uint32_t> indices(vCount);
    for (int i = 0; i < vCount; i++) {
        auto& fv = faceVerts[i];
        glm::vec3 p = positions[fv.p];
        glm::vec3 n = (fv.n >= 0 && fv.n < (int)normals.size()) ? normals[fv.n] : glm::vec3(0,1,0);
        verts[i*6+0]=p.x; verts[i*6+1]=p.y; verts[i*6+2]=p.z;
        verts[i*6+3]=n.x; verts[i*6+4]=n.y; verts[i*6+5]=n.z;
        indices[i] = (uint32_t)i;
    }

    GltfModel::GpuMesh gm;
    gm.color      = {0.75f, 0.72f, 0.68f};
    gm.localXform = glm::mat4(1.f);
    gm.indexCount = vCount;
    gm.indexType  = GL_UNSIGNED_INT;

    glGenVertexArrays(1, &gm.vao);
    glGenBuffers(1, &gm.vbo);
    glGenBuffers(1, &gm.ebo);
    glBindVertexArray(gm.vao);
    glBindBuffer(GL_ARRAY_BUFFER, gm.vbo);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(verts.size()*sizeof(float)), verts.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gm.ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(indices.size()*sizeof(uint32_t)), indices.data(), GL_STATIC_DRAW);
    constexpr int stride = 6 * sizeof(float);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, stride, (void*)(3*sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    out.push_back(gm);
    fprintf(stderr, "GltfModel OBJ: '%s' → %d tris\n", path.c_str(), vCount/3);
    return true;
}

bool GltfModel::load(const std::string& path) {
    cleanup();
    initShader();

    // OBJ fast-path: no scene graph, no transforms
    std::string ext = path.size() > 4 ? path.substr(path.size() - 4) : "";
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    if (ext == ".obj") return loadObj(path, meshes);

    tinygltf::Model    model;
    tinygltf::TinyGLTF loader;
    std::string err, warn;

    // Try binary first, fall back to ASCII for .gltf files
    bool ok = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    if (!ok) ok = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    if (!ok) {
        fprintf(stderr, "GltfModel: parse failed '%s': %s\n", path.c_str(), err.c_str());
        return false;
    }
    if (!warn.empty()) fprintf(stderr, "GltfModel warn '%s': %s\n", path.c_str(), warn.c_str());

    // Walk scene graph to collect all mesh primitives with baked transforms
    int sceneIdx = model.defaultScene >= 0 ? model.defaultScene : 0;
    if (sceneIdx < (int)model.scenes.size()) {
        for (int rootNode : model.scenes[sceneIdx].nodes)
            processNode(model, rootNode, glm::mat4(1.f), meshes);
    } else {
        // No scene — just dump all meshes at identity
        for (int i = 0; i < (int)model.nodes.size(); i++)
            processNode(model, i, glm::mat4(1.f), meshes);
    }

    fprintf(stderr, "GltfModel: '%s' → %d GPU mesh(es)\n", path.c_str(), (int)meshes.size());
    return !meshes.empty();
}

void GltfModel::render(const glm::mat4& VP, glm::vec3 worldPos, float yaw, float scale,
                        glm::vec3 sunDir, float fogDensity, glm::vec3 camPos) {
    glm::mat4 model = glm::translate(glm::mat4(1.f), worldPos)
                    * glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0,1,0))
                    * glm::scale(glm::mat4(1.f), glm::vec3(scale));
    renderMatrix(VP, model, sunDir, fogDensity, camPos);
}

void GltfModel::renderMatrix(const glm::mat4& VP, const glm::mat4& model,
                              glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                              glm::vec3 tint) {
    if (!prog || meshes.empty()) return;

    glUseProgram(prog);
    glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"), 1, GL_FALSE, glm::value_ptr(model));
    glUniform3fv      (glGetUniformLocation(prog,"uSunDir"),     1, glm::value_ptr(sunDir));
    glUniform1f       (glGetUniformLocation(prog,"uFogDensity"), fogDensity);
    glUniform3fv      (glGetUniformLocation(prog,"uCamPos"),     1, glm::value_ptr(camPos));

    glm::mat4 mvp = VP * model;

    for (auto& m : meshes) {
        glm::mat4 meshModel = model * m.localXform;
        glm::mat4 meshMvp   = VP * meshModel;
        glUniformMatrix4fv(glGetUniformLocation(prog,"uMVP"),   1, GL_FALSE, glm::value_ptr(meshMvp));
        glUniformMatrix4fv(glGetUniformLocation(prog,"uModel"), 1, GL_FALSE, glm::value_ptr(meshModel));
        glm::vec3 c = m.color * tint;
        glUniform3fv      (glGetUniformLocation(prog,"uColor"), 1, glm::value_ptr(c));
        glBindVertexArray(m.vao);
        glDrawElements(GL_TRIANGLES, m.indexCount, m.indexType, nullptr);
    }
    glBindVertexArray(0);
}
