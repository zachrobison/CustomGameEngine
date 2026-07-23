#include "Audio.h"

// miniaudio with Ogg Vorbis via stb_vorbis (both vendored, public domain).
// This is the documented integration recipe: vorbis header first, then the
// miniaudio implementation, then the vorbis implementation.
#define STB_VORBIS_HEADER_ONLY
#include "../vendor/stb_vorbis.c"

#define MINIAUDIO_IMPLEMENTATION
#include "../vendor/miniaudio.h"

#undef STB_VORBIS_HEADER_ONLY
#include "../vendor/stb_vorbis.c"

#include <cstdio>
#include <map>
#include <sys/stat.h>
#include <vector>

struct Audio::Impl {
    ma_engine engine;
    bool      ok = false;

    struct Shot { ma_sound sound; bool inUse = false; };
    std::vector<Shot*> shots;                    // one-shot pool
    std::map<std::string, ma_sound*> loops;      // key → looping sound
    std::map<std::string, std::string> pathCache;
};

Audio& Audio::get() {
    static Audio inst;
    return inst;
}

static bool fileExists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

// name → assets/sounds/<name>.ogg|.wav (cached; empty string = missing)
static std::string resolve(Audio::Impl* impl, const std::string& name) {
    auto it = impl->pathCache.find(name);
    if (it != impl->pathCache.end()) return it->second;
    std::string base = "assets/sounds/" + name;
    std::string found;
    for (const char* ext : {".ogg", ".wav", ".mp3"}) {
        if (fileExists(base + ext)) { found = base + ext; break; }
    }
    if (found.empty())
        fprintf(stderr, "Audio: no file for '%s'\n", name.c_str());
    impl->pathCache[name] = found;
    return found;
}

void Audio::init() {
    if (impl) return;
    impl = new Impl();
    if (ma_engine_init(nullptr, &impl->engine) != MA_SUCCESS) {
        fprintf(stderr, "Audio: engine init failed — sound disabled\n");
        impl->ok = false;
        return;
    }
    impl->ok = true;
}

void Audio::update() {
    if (!impl || !impl->ok) return;
    for (auto* s : impl->shots) {
        if (s->inUse && !ma_sound_is_playing(&s->sound)) {
            ma_sound_uninit(&s->sound);
            s->inUse = false;
        }
    }
}

void Audio::play(const std::string& name, float volume, float pitch) {
    if (!impl || !impl->ok) return;
    std::string path = resolve(impl, name);
    if (path.empty()) return;

    Audio::Impl::Shot* slot = nullptr;
    for (auto* s : impl->shots)
        if (!s->inUse) { slot = s; break; }
    if (!slot) {
        if (impl->shots.size() >= 64) return;   // cap concurrent one-shots
        slot = new Impl::Shot();
        impl->shots.push_back(slot);
    }
    if (ma_sound_init_from_file(&impl->engine, path.c_str(), 0,
                                nullptr, nullptr, &slot->sound) != MA_SUCCESS)
        return;
    ma_sound_set_volume(&slot->sound, volume);
    ma_sound_set_pitch(&slot->sound, pitch);
    ma_sound_start(&slot->sound);
    slot->inUse = true;
}

void Audio::loop(const std::string& key, const std::string& name, bool on,
                 float volume, float pitch) {
    if (!impl || !impl->ok) return;

    auto it = impl->loops.find(key);
    if (!on) {
        if (it != impl->loops.end() && ma_sound_is_playing(it->second))
            ma_sound_stop(it->second);
        return;
    }

    ma_sound* s;
    if (it == impl->loops.end()) {
        std::string path = resolve(impl, name);
        if (path.empty()) return;
        s = new ma_sound();
        if (ma_sound_init_from_file(&impl->engine, path.c_str(), 0,
                                    nullptr, nullptr, s) != MA_SUCCESS) {
            delete s;
            return;
        }
        ma_sound_set_looping(s, MA_TRUE);
        impl->loops[key] = s;
    } else {
        s = it->second;
    }
    ma_sound_set_volume(s, volume);
    ma_sound_set_pitch(s, pitch);
    if (!ma_sound_is_playing(s)) ma_sound_start(s);
}

void Audio::stopAllLoops() {
    if (!impl || !impl->ok) return;
    for (auto& [k, s] : impl->loops)
        if (ma_sound_is_playing(s)) ma_sound_stop(s);
}
