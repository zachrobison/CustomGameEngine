#pragma once
#include "../renderer/Camera.h"
#include "../Settings.h"
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <vector>

class World;
class Weapon;
class EntityManager;
class Props;
class KeyBinds;

class Player {
public:
    Camera    camera;
    glm::vec3 velocity    = { 0.f, 0.f, 0.f };
    bool      onGround    = false;
    bool      flyMode     = false;  // debug no-clip; toggle with [G]

    // Health — damaged by ModifyHealth action effects; death/respawn in main
    float health = 100.f, maxHealth = 100.f;

    // Cockpit free-look: view offset from car-forward, clamped each side.
    float lookYaw = 0.f, lookPitch = 0.f;

    bool suppressFire = false;   // true while the factory build tool owns LMB

    // Per-game feature switches (set from GameConfig when a game loads)
    bool allowDash = true, allowGrapple = true,
         allowRocket = true, allowJetpack = true;

    // ── Traversal abilities (walk mode) ────────────────────────────────────
    // Dash [Q]: horizontal burst in look direction with a short cooldown.
    float     dashTimer = 0.f, dashCooldown = 0.f;
    glm::vec3 dashDir   = {0,0,0};
    // Jetpack: hold Space while airborne. Fuel drains in flight, refills on
    // the ground. Cancels downward momentum (doc: "hoverpack").
    float jetFuel = 100.f, jetMaxFuel = 100.f;
    // Grapple [C, hold]: hooks a voxel up to 20u away and reels you in.
    bool      grappling    = false;
    glm::vec3 grapplePoint = {0,0,0};
    // Rocket [X, hold to charge]: releases a charged launch in look direction.
    float rocketCharge = -1.f;  // <0 = idle, otherwise seconds held (max 2)
    float rocketCooldown = 0.f, noControlTimer = 0.f;

    // Aim raycast result (updated every frame, used for highlight)
    glm::ivec3 aimHit  = { 0, 0, 0 };
    glm::ivec3 aimNorm = { 0, 1, 0 };
    bool       aimValid = false;

    // Weapons — pointers are non-owning; weapons are owned in main
    std::vector<Weapon*> weapons;
    int activeSlot = 0;

    // Vehicle state
    bool      inVehicle        = false;
    float     vehicleHeading   = 0.f;    // body yaw, degrees
    glm::vec2 vehicleVel       = {0,0};  // world XZ velocity (m/s)
    float     vehicleYawRate   = 0.f;    // deg/s
    float     vehicleSpeed     = 0.f;    // signed forward speed (for speedometer)
    bool      vehicleDrifting  = false;
    float     vehicleMaxSpeed  = 42.f;   // ~150 km/h default
    float     vehicleTurnRate  = 120.f;  // deg/s

    Weapon* currentWeapon() const;

    void update(GLFWwindow* window, World& world, EntityManager& entities,
                const WorldSettings& ws, const PlayerSettings& ps,
                const KeyBinds& binds,
                float dt, bool mouseCaptured, const Props* props = nullptr);
};
