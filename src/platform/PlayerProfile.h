#pragma once
#include <string>
#include <cstdint>

// Identifies the current player.
// Without Steamworks: generates a local UUID stored in ~/.voxelengine/profile.
// With Steamworks: call initSteam() first; id() returns the 64-bit Steam ID as string.
//
// To enable Steam:
//   1. Add the Steamworks SDK to vendor/steamworks/
//   2. Add `add_compile_definitions(HAVE_STEAMWORKS)` to CMakeLists.txt
//   3. Link against steam_api (see CMakeLists.txt comments)

class PlayerProfile {
public:
    static PlayerProfile& get();

    void        init();          // call once at startup
    std::string id()        const { return m_id;   }
    std::string name()      const { return m_name; }
    bool        hasSteam()  const { return m_hasSteam; }

    std::string saveDir()   const; // returns per-player save directory path

    // Point the profile/save/games root at an explicit folder (the games shipped
    // next to the executable). Call before init(). When unset, falls back to
    // ~/.voxelengine. This is what makes a downloaded build read its bundled
    // games straight from the folder it was unzipped into — no copy, no reliance
    // on HOME/USERPROFILE resolution.
    static void setDataRoot(const std::string& root);

private:
    PlayerProfile() = default;
    void loadOrCreateLocal();

    std::string m_id;
    std::string m_name;
    bool        m_hasSteam = false;
};
