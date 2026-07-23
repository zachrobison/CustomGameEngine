#pragma once
#include <string>
#include <vector>

// A "game" is a data project the engine loads: which level to boot, which
// engine features are enabled, which weapons the player carries. Lives in
// <profile>/games/<id>/game.json — games configure the engine, they don't
// fork it.
struct GameConfig {
    std::string id, name = "Untitled Game", description;
    std::string bootLevel = "default";

    // Feature switches
    bool vehicle        = true;
    bool rocket         = true;
    bool grapple        = true;
    bool dash           = true;
    bool jetpack        = true;
    bool dimensionShift = true;
    bool thirdPerson    = true;
    bool isoCamera      = false;  // locked isometric view (RTS/terminal games)
    bool rts            = false;  // Iron Command: top-down factory-RTS mode
    bool factory        = false;  // Iron Command: first-person build/factory

    // Weapon loadout by name: "sprayer", "eraser", "gun", "melee"
    std::vector<std::string> weapons = {"sprayer", "eraser", "gun", "melee"};

    static GameConfig load(const std::string& dir);          // dir has game.json
    static std::vector<GameConfig> scan(const std::string& gamesRoot);
};
