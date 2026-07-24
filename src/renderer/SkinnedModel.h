#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <vector>

// Loads and renders a GLB with a skeleton: skinned vertices (JOINTS_0 /
// WEIGHTS_0), inverse bind matrices, and looping keyframe animations.
// Complements GltfModel, which handles static props.
class SkinnedModel {
public:
    SkinnedModel() = default;
    ~SkinnedModel();
    SkinnedModel(const SkinnedModel&)            = delete;
    SkinnedModel& operator=(const SkinnedModel&) = delete;

    bool load(const std::string& path);
    bool loaded() const { return !prims.empty(); }

    // Switch the looping clip by name ("Idle", "Run"). No-op if already
    // playing or unknown.
    void play(const std::string& animName);
    void update(float dt);

    // For models whose motion is split across several clips (e.g. a body rig
    // + a bone-parented prop rig exported separately): play them all together
    // on one shared clock instead of a single active clip.
    void setCombineAll(bool on) { combineAll = on; }

    // Stop-motion look: pose evaluation snaps to this many poses per second
    // while time advances smoothly (the skeleton holds each pose). 0 = off.
    void setSnapFps(float fps) { snapFps = fps; }

    // Play a time window [t0,t1] (seconds). loop=false plays once and holds at
    // t1 (segmentDone() → true); loop=true repeats the window. Used for the
    // build-hand's open / idle-loop / close segments.
    void playSegment(float t0, float t1, bool loop);
    void holdAt(float t);          // freeze on a single pose
    bool segmentDone() const { return segDone; }
    float duration() const;        // longest clip length (seconds)

    // Current global (model-space) transform of the first node whose name
    // contains `substr` (case-insensitive) — e.g. the gun, for muzzle
    // tracking. Valid after update(). Returns false if no such node.
    bool nodeWorld(const std::string& substr, glm::mat4& out) const;

    void render(const glm::mat4& VP, glm::vec3 worldPos, float yaw, float scale,
                glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                glm::vec3 tint = {1.f, 1.f, 1.f});   // multiplies material color
    // Arbitrary model matrix (first-person viewmodels).
    void renderMatrix(const glm::mat4& VP, const glm::mat4& modelM,
                      glm::vec3 sunDir, float fogDensity, glm::vec3 camPos,
                      glm::vec3 tint = {1.f, 1.f, 1.f});

    static constexpr int MAX_JOINTS = 96;

private:
    struct Node {
        int        parent = -1;
        glm::vec3  t{0.f};
        glm::quat  r{1.f, 0.f, 0.f, 0.f};
        glm::vec3  s{1.f};
        glm::mat4  global{1.f};
    };
    struct Channel {
        int  node = -1;
        int  path = 0;                    // 0=translation 1=rotation 2=scale
        std::vector<float>     times;
        std::vector<glm::vec4> values;    // quat in xyzw for rotation
    };
    struct Anim {
        std::string name;
        float       duration = 0.f;
        std::vector<Channel> channels;
    };
    struct Prim {
        GLuint vao = 0, vbo = 0, ebo = 0;
        int    indexCount = 0;
        glm::vec3 color{0.8f};
    };

    std::vector<Node>      rest;          // bind-pose local TRS
    std::vector<std::string> nodeNames;   // per glTF node, for nodeWorld()
    std::vector<Node>      pose;          // evaluated per frame
    std::vector<int>       jointNodes;    // node index per skin joint
    std::vector<glm::mat4> inverseBind;
    std::vector<glm::mat4> jointMats;
    std::vector<Anim>      anims;
    std::vector<Prim>      prims;
    GLuint prog    = 0;
    int    curAnim = -1;
    float  animT   = 0.f;
    bool   combineAll = false;   // play every clip together (multi-rig models)
    float  snapFps    = 0.f;     // >0: stop-motion pose quantisation
    // Segment playback (build-hand): active window, loop flag, done latch.
    bool   segActive  = false, segLoop = false, segDone = false, holding = false;
    float  segStart = 0.f, segEnd = 0.f;

    void evalPose();
    void initShader();
    void cleanup();
};
