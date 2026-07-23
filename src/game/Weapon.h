#pragma once
#include "../renderer/Camera.h"
#include <string>
#include <cstdint>

class World;
class EntityManager;

class Weapon {
public:
    std::string name;
    float fireRate   = 0.12f;  // seconds between shots
    float cooldown   = 0.f;

    virtual ~Weapon() = default;

    // entities may be null (e.g. tool-style weapons fired without a scene)
    virtual void onFire(const Camera& cam, World& world, EntityManager* entities) = 0;

    // Each weapon draws its own settings section inside the ImGui panel
    virtual void renderUI() = 0;

    void  tick(float dt)     { if (cooldown > 0) cooldown -= dt; }
    bool  canFire()    const { return cooldown <= 0.f; }
    void  startCooldown()    { cooldown = fireRate; }
};

// ── Voxel Sprayer ──────────────────────────────────────────────────────────
class VoxelSprayer : public Weapon {
public:
    int     sprayCount   = 48;    // dense grains, centre-weighted (clumpy)
    float   spreadAngle  = 4.f;
    float   sprayRange   = 25.f;
    uint8_t voxelType    = 5;     // sand

    VoxelSprayer()  { name = "Voxel Sprayer"; fireRate = 0.06f; }
    void onFire(const Camera& cam, World& world, EntityManager* entities) override;
    void renderUI()                                                       override;
};

// ── Voxel Eraser ──────────────────────────────────────────────────────────
class VoxelEraser : public Weapon {
public:
    int   eraseCount   = 8;
    float spreadAngle  = 20.f;
    float eraseRange   = 20.f;

    VoxelEraser()   { name = "Voxel Eraser";  fireRate = 0.10f; }
    void onFire(const Camera& cam, World& world, EntityManager* entities) override;
    void renderUI()                                                       override;
};

// ── Gun (hitscan) ─────────────────────────────────────────────────────────
class Gun : public Weapon {
public:
    float damage     = 25.f;
    float range      = 80.f;
    float spread     = 1.5f;   // degrees
    int   ammo       = 30;
    int   maxAmmo    = 30;
    bool  autoFire   = false;
    bool  breakVoxels = false;  // if true, destroys hit voxels

    // Set to a short duration when a shot lands on an entity; main drains
    // it and drives the crosshair hit marker.
    float hitFlash = 0.f;

    Gun()           { name = "Gun"; fireRate = 0.15f; }
    void onFire(const Camera& cam, World& world, EntityManager* entities) override;
    void renderUI()                                                       override;
    void reload()   { ammo = maxAmmo; }
};

// ── Turret Placer ─────────────────────────────────────────────────────────
// Aims at voxel ground and drops an auto-firing turret on top (see Turrets).
class TurretPlacer : public Weapon {
public:
    float placeRange = 18.f;

    TurretPlacer()  { name = "Turret Placer"; fireRate = 1.0f; }
    void onFire(const Camera& cam, World& world, EntityManager* entities) override;
    void renderUI()                                                       override;
};

// ── Melee (hitbox cone) ───────────────────────────────────────────────────
// Swings a short-range damage cone in front of the camera. Entities inside
// range whose direction is within the arc take damage; at 0 HP they die.
class MeleeWeapon : public Weapon {
public:
    float damage    = 35.f;
    float range     = 2.6f;
    float arcDeg    = 80.f;   // total cone angle
    float hitFlash  = 0.f;    // see Gun::hitFlash
    float swingT    = 0.f;    // >0 while the swing visual plays (main drains)

    // Iso mode: swings aim where the ground cursor points, not the camera.
    bool      aimOverrideOn = false;
    glm::vec3 aimOverride{0.f, 0.f, 1.f};

    MeleeWeapon()   { name = "Melee"; fireRate = 0.5f; }
    void onFire(const Camera& cam, World& world, EntityManager* entities) override;
    void renderUI()                                                       override;
};
