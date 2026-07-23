#pragma once
#include "Character.h"
#include <string>
#include <vector>

// ── Per-bone rotation (Euler degrees, applied X→Y→Z) ──────────────────────
struct BonePose {
    float rx = 0, ry = 0, rz = 0;
};

// ── One frame in a clip ───────────────────────────────────────────────────
struct Keyframe {
    float    time = 0.f;
    BonePose bones[(int)BodyPart::COUNT] = {};
};

// ── A named keyframe animation ────────────────────────────────────────────
struct AnimationClip {
    std::string          name;
    float                duration = 1.f;
    bool                 loop     = true;
    std::vector<Keyframe> frames;

    // Interpolated pose at time t
    void sample(float t, BonePose out[(int)BodyPart::COUNT]) const;
};

// ── Playback state ────────────────────────────────────────────────────────
struct AnimationState {
    const AnimationClip* clip    = nullptr;
    float                time    = 0.f;
    bool                 playing = false;

    void play(const AnimationClip* c) { clip = c; time = 0.f; playing = (c != nullptr); }
    void stop()                        { playing = false; }

    void update(float dt) {
        if (!playing || !clip) return;
        time += dt;
        if (clip->loop) { while (time >= clip->duration) time -= clip->duration; }
        else            { if (time > clip->duration) { time = clip->duration; playing = false; } }
    }

    void getBonePoses(BonePose out[(int)BodyPart::COUNT]) const {
        if (clip) clip->sample(time, out);
        else for (int i = 0; i < (int)BodyPart::COUNT; i++) out[i] = {};
    }
};

// ── Built-in clips (defined in AnimationClip.cpp) ─────────────────────────
namespace BuiltinAnims {
    const AnimationClip& idle();
    const AnimationClip& walk();
    const AnimationClip& run();
    const AnimationClip& jump();
}

// ── BVH loader ────────────────────────────────────────────────────────────
namespace BVHLoader {
    // Load a .bvh file from disk into a clip.
    bool load(const std::string& path, AnimationClip& out);

    // Download a BVH from a URL (uses curl) then load it.
    // statusOut receives a human-readable result message.
    bool downloadAndLoad(const std::string& url, AnimationClip& out,
                         std::string& statusOut);
}
