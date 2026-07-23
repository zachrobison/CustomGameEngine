#pragma once
#include "../Settings.h"
#include "ScenePanel.h"
#include "WeaponWheel.h"
#include <string>
#include <vector>
#include <functional>

struct GLFWwindow;
struct GameConfig;

class UI {
public:
    static constexpr int PANEL_W = 300;

    std::string statusMsg;
    bool        playMode     = false;
    bool        requestMenu  = false;   // "Games" button → back to main menu
    bool        nearVehicle     = false;
    bool        inVehicle       = false;
    float       vehicleSpeed    = 0.f;
    float       vehicleMaxSpeed = 42.f;  // scales the speedometer sweep
    float       vehicleHeading  = 0.f;   // degrees (kept for future use)
    // View offset from car-forward (degrees): dashboard is locked to the car,
    // so it slides opposite to where you look.
    float       vehicleLookYaw   = 0.f;
    float       vehicleLookPitch = 0.f;
    bool        vehicleDrifting = false;
    bool        vehicleNitrous  = false;

    // Player HUD (set from main each frame)
    float playerHealth = 100.f, playerMaxHealth = 100.f;
    float jetFuel      = 100.f, jetMaxFuel      = 100.f;
    float rocketCharge = -1.f;  // >=0 while charging (0..2s)
    float hitMarker    = 0.f;   // >0 briefly after a shot lands (crosshair X)

    // Horde Defense HUD (set from main each frame)
    bool  hordeActive = false, hordeVictory = false, hordeDefeat = false;
    bool  hordePrestart = false;   // horde level loaded but not in play mode
    int   hordeWave = 0, hordeWaves = 0, hordeLeft = 0;
    float hordeBase = 0.f, hordeBaseMax = 1.f, hordeCountdown = 0.f;
    float levelWin = 0.f;   // >0 = "level cleared" banner countdown
    std::function<void()> extraHud;   // game overlay drawn before ImGui::Render
    bool  bossActive = false;
    float bossHealthFrac = 1.f;

    // Rebindable keys (owned by main; edited in the panel, saved on change)
    class KeyBinds* keyBinds = nullptr;
    std::string     keyBindsPath;

    // Per-dimension screen tint (Interdimensional Shooter): hardcoded colors
    // keyed by the current level id, drawn as a viewport overlay when on.
    bool dimensionTints = false;

    void applyObsidianStyle();

    ScenePanel  scenePanel;
    WeaponWheel weaponWheel;

    // fHeld: glfwGetKey(GLFW_KEY_F) — drives the weapon wheel.
    // Returns new activeSlot if wheel changed it, else -1.
    int render(WorldSettings& ws, PlayerSettings& ps, bool& flyMode,
               std::vector<Weapon*>& weapons, int& activeSlot, World& world,
               Character& character, CharacterRenderer& charRenderer,
               EntityManager& entities,
               const std::string& savePath,
               GLFWwindow* window, bool& mouseCaptured, bool fHeld);

    void drawCrosshair(int winW, int winH, int xOffset) const;

    // Fullscreen game-picker (main menu). Owns the ImGui frame for the
    // iteration. Returns the picked game index, or -1.
    int renderMainMenu(const std::vector<GameConfig>& games, GLFWwindow* window);
};
