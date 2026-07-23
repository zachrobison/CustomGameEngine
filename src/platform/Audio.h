#pragma once
#include <string>

// Engine-wide audio: one-shots and named loop channels, backed by miniaudio.
// Files live in assets/sounds/<name>.ogg (or .wav). All calls are safe when
// init failed or a file is missing — they just do nothing.
class Audio {
public:
    static Audio& get();

    void init();
    void update();   // call once per frame: reclaims finished one-shots

    // Fire-and-forget. pitch 1 = normal.
    void play(const std::string& name, float volume = 1.f, float pitch = 1.f);

    // Named persistent loop channel ("engine", "jetpack"). Starts/stops the
    // loop and retunes volume/pitch every call, so callers can just invoke
    // it each frame with the current state.
    void loop(const std::string& key, const std::string& name, bool on,
              float volume = 1.f, float pitch = 1.f);

    void stopAllLoops();

    struct Impl;   // public so the backend's file-scope helpers can use it

private:
    Audio() = default;
    Impl* impl = nullptr;
};
