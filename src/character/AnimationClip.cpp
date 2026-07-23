#include "AnimationClip.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <unistd.h>

// ── Interpolation helpers ─────────────────────────────────────────────────

static float lerpF(float a, float b, float t) { return a + (b - a) * t; }

static BonePose lerpPose(const BonePose& a, const BonePose& b, float t) {
    return { lerpF(a.rx,b.rx,t), lerpF(a.ry,b.ry,t), lerpF(a.rz,b.rz,t) };
}

void AnimationClip::sample(float t, BonePose out[(int)BodyPart::COUNT]) const {
    if (frames.empty()) {
        for (int i = 0; i < (int)BodyPart::COUNT; i++) out[i] = {};
        return;
    }
    if (frames.size() == 1) {
        for (int i = 0; i < (int)BodyPart::COUNT; i++) out[i] = frames[0].bones[i];
        return;
    }

    // Find surrounding keyframes
    int hi = 0;
    while (hi < (int)frames.size() && frames[hi].time <= t) hi++;
    if (hi == 0) {
        for (int i = 0; i < (int)BodyPart::COUNT; i++) out[i] = frames[0].bones[i];
        return;
    }
    if (hi >= (int)frames.size()) {
        for (int i = 0; i < (int)BodyPart::COUNT; i++) out[i] = frames.back().bones[i];
        return;
    }

    int lo = hi - 1;
    float span = frames[hi].time - frames[lo].time;
    float blend = (span > 0.f) ? (t - frames[lo].time) / span : 0.f;
    for (int i = 0; i < (int)BodyPart::COUNT; i++)
        out[i] = lerpPose(frames[lo].bones[i], frames[hi].bones[i], blend);
}

// ── Built-in animation factory ────────────────────────────────────────────

static AnimationClip makeIdle() {
    AnimationClip c;
    c.name = "idle"; c.duration = 2.f; c.loop = true;

    auto kf = [&](float t, float headTilt, float torsoSway, float lArmSwing, float rArmSwing) {
        Keyframe k; k.time = t;
        k.bones[(int)BodyPart::Head].rz     = headTilt;
        k.bones[(int)BodyPart::Torso].rz    = torsoSway;
        k.bones[(int)BodyPart::LeftArm].rx  = lArmSwing;
        k.bones[(int)BodyPart::RightArm].rx = rArmSwing;
        c.frames.push_back(k);
    };

    kf(0.f,  0.f,  0.f,  3.f, -3.f);
    kf(0.5f, 1.5f, 1.f,  2.f, -2.f);
    kf(1.0f, 0.f,  0.f, -3.f,  3.f);
    kf(1.5f,-1.5f,-1.f, -2.f,  2.f);
    kf(2.0f, 0.f,  0.f,  3.f, -3.f);
    return c;
}

static AnimationClip makeWalk() {
    AnimationClip c;
    c.name = "walk"; c.duration = 0.8f; c.loop = true;

    auto kf = [&](float t, float lLeg, float rLeg, float lArm, float rArm, float torso) {
        Keyframe k; k.time = t;
        k.bones[(int)BodyPart::LeftLeg].rx  = lLeg;
        k.bones[(int)BodyPart::RightLeg].rx = rLeg;
        k.bones[(int)BodyPart::LeftArm].rx  = lArm;
        k.bones[(int)BodyPart::RightArm].rx = rArm;
        k.bones[(int)BodyPart::Torso].ry    = torso;
        c.frames.push_back(k);
    };

    kf(0.0f,  30.f, -30.f, -20.f,  20.f,  3.f);
    kf(0.2f,   0.f,   0.f,   0.f,   0.f,  0.f);
    kf(0.4f, -30.f,  30.f,  20.f, -20.f, -3.f);
    kf(0.6f,   0.f,   0.f,   0.f,   0.f,  0.f);
    kf(0.8f,  30.f, -30.f, -20.f,  20.f,  3.f);
    return c;
}

static AnimationClip makeRun() {
    AnimationClip c;
    c.name = "run"; c.duration = 0.5f; c.loop = true;

    auto kf = [&](float t, float lLeg, float rLeg, float lArm, float rArm, float lean) {
        Keyframe k; k.time = t;
        k.bones[(int)BodyPart::LeftLeg].rx  = lLeg;
        k.bones[(int)BodyPart::RightLeg].rx = rLeg;
        k.bones[(int)BodyPart::LeftArm].rx  = lArm;
        k.bones[(int)BodyPart::RightArm].rx = rArm;
        k.bones[(int)BodyPart::Torso].rx    = lean;
        c.frames.push_back(k);
    };

    kf(0.00f,  55.f, -55.f, -45.f,  45.f, -15.f);
    kf(0.125f,  0.f,   0.f,   0.f,   0.f, -12.f);
    kf(0.25f, -55.f,  55.f,  45.f, -45.f, -15.f);
    kf(0.375f,  0.f,   0.f,   0.f,   0.f, -12.f);
    kf(0.5f,   55.f, -55.f, -45.f,  45.f, -15.f);
    return c;
}

static AnimationClip makeJump() {
    AnimationClip c;
    c.name = "jump"; c.duration = 0.7f; c.loop = false;

    auto kf = [&](float t, float lLeg, float rLeg, float lArm, float rArm) {
        Keyframe k; k.time = t;
        k.bones[(int)BodyPart::LeftLeg].rx  = lLeg;
        k.bones[(int)BodyPart::RightLeg].rx = rLeg;
        k.bones[(int)BodyPart::LeftArm].rx  = lArm;
        k.bones[(int)BodyPart::RightArm].rx = rArm;
        c.frames.push_back(k);
    };

    kf(0.0f,  20.f,  20.f, -40.f, -40.f);  // crouch / prep
    kf(0.15f,-35.f, -35.f, -90.f, -90.f);  // airborne
    kf(0.45f,-35.f, -35.f, -90.f, -90.f);  // hold peak
    kf(0.60f, 20.f,  20.f, -20.f, -20.f);  // landing
    kf(0.70f,  0.f,   0.f,   0.f,   0.f);  // settle
    return c;
}

namespace BuiltinAnims {
    const AnimationClip& idle() { static AnimationClip c = makeIdle(); return c; }
    const AnimationClip& walk() { static AnimationClip c = makeWalk(); return c; }
    const AnimationClip& run()  { static AnimationClip c = makeRun();  return c; }
    const AnimationClip& jump() { static AnimationClip c = makeJump(); return c; }
}

// ── BVH Parser ────────────────────────────────────────────────────────────
// Maps BVH joint names to BodyPart indices.  Joints not in the map are ignored.

static int bvhJointToBodyPart(const std::string& name) {
    // Case-insensitive prefix matching against common BVH conventions
    auto ci = name;
    for (auto& ch : ci) ch = (char)tolower((unsigned char)ch);

    if (ci.find("head")  != std::string::npos) return (int)BodyPart::Head;
    if (ci.find("spine") != std::string::npos ||
        ci.find("chest") != std::string::npos ||
        ci.find("torso") != std::string::npos ||
        ci.find("abdomen")!= std::string::npos) return (int)BodyPart::Torso;
    if (ci.find("lshldr")!= std::string::npos ||
        ci.find("leftshoulder")!=std::string::npos ||
        ci.find("l_upperarm")  !=std::string::npos ||
        (ci.find("left")!=std::string::npos  && ci.find("arm")!=std::string::npos))
        return (int)BodyPart::LeftArm;
    if (ci.find("rshldr")!= std::string::npos ||
        ci.find("rightshoulder")!=std::string::npos ||
        ci.find("r_upperarm")   !=std::string::npos ||
        (ci.find("right")!=std::string::npos && ci.find("arm")!=std::string::npos))
        return (int)BodyPart::RightArm;
    if ((ci.find("left")!=std::string::npos || ci.find("lhip")!=std::string::npos ||
         ci.find("l_thigh")!=std::string::npos) && ci.find("leg")!=std::string::npos)
        return (int)BodyPart::LeftLeg;
    if ((ci.find("right")!=std::string::npos || ci.find("rhip")!=std::string::npos ||
         ci.find("r_thigh")!=std::string::npos) && ci.find("leg")!=std::string::npos)
        return (int)BodyPart::RightLeg;
    return -1;
}

struct BVHJoint {
    std::string name;
    int         bodyPart = -1;
    int         channels = 0;      // total channel count for this joint
    // which channel indices (within this joint's block) carry Xrot, Yrot, Zrot
    int         xRotIdx = -1, yRotIdx = -1, zRotIdx = -1;
};

namespace BVHLoader {

bool load(const std::string& path, AnimationClip& out) {
    std::ifstream f(path);
    if (!f.is_open()) return false;

    std::vector<BVHJoint> joints;
    bool   inMotion    = false;
    int    frameCount  = 0;
    float  frameTime   = 1.f / 30.f;
    int    totalCh     = 0; // total channels across all joints

    std::string line;
    while (std::getline(f, line)) {
        std::istringstream ss(line);
        std::string tok;
        ss >> tok;

        if (!inMotion) {
            if (tok == "ROOT" || tok == "JOINT") {
                BVHJoint j;
                ss >> j.name;
                j.bodyPart = bvhJointToBodyPart(j.name);
                joints.push_back(j);
            } else if (tok == "CHANNELS") {
                int n; ss >> n;
                BVHJoint& j = joints.back();
                j.channels = n;
                for (int i = 0; i < n; i++) {
                    std::string ch; ss >> ch;
                    if (ch == "Xrotation") j.xRotIdx = i;
                    else if (ch == "Yrotation") j.yRotIdx = i;
                    else if (ch == "Zrotation") j.zRotIdx = i;
                }
                totalCh += n;
            } else if (tok == "MOTION") {
                inMotion = true;
            }
        } else {
            if (tok == "Frames:") { ss >> frameCount; continue; }
            if (tok == "Frame") {
                std::string timeWord; ss >> timeWord; // "Time:"
                ss >> frameTime;
                continue;
            }

            // Data line: one value per channel across all joints
            std::vector<float> vals;
            vals.reserve(totalCh);
            float v;
            std::istringstream ds(line);
            while (ds >> v) vals.push_back(v);
            if ((int)vals.size() < totalCh) continue;

            Keyframe kf;
            kf.time = (float)out.frames.size() * frameTime;

            int offset = 0;
            for (auto& j : joints) {
                if (j.bodyPart >= 0) {
                    BonePose& bp = kf.bones[j.bodyPart];
                    if (j.xRotIdx >= 0) bp.rx = vals[offset + j.xRotIdx];
                    if (j.yRotIdx >= 0) bp.ry = vals[offset + j.yRotIdx];
                    if (j.zRotIdx >= 0) bp.rz = vals[offset + j.zRotIdx];
                }
                offset += j.channels;
            }
            out.frames.push_back(kf);
        }
    }

    if (out.frames.empty()) return false;
    out.duration = out.frames.back().time + frameTime;
    out.loop     = true;
    if (out.name.empty()) out.name = path;
    return true;
}

bool downloadAndLoad(const std::string& url, AnimationClip& out, std::string& statusOut) {
    // Write to a temp file
    char tmpPath[] = "/tmp/voxengine_bvh_XXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd < 0) { statusOut = "Failed to create temp file"; return false; }
    close(fd);

    // Use curl to download
    std::string cmd = "curl -s -L --max-time 15 -o '";
    cmd += tmpPath;
    cmd += "' '";
    cmd += url;
    cmd += "'";

    int ret = system(cmd.c_str());
    if (ret != 0) {
        statusOut = "curl failed (exit " + std::to_string(ret) + ")";
        unlink(tmpPath);
        return false;
    }

    bool ok = load(tmpPath, out);
    unlink(tmpPath);

    if (ok) {
        statusOut = "Loaded: " + out.name + " (" + std::to_string(out.frames.size()) + " frames)";
    } else {
        statusOut = "BVH parse failed";
    }
    return ok;
}

} // namespace BVHLoader
