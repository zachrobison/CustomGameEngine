#pragma once
#include <glm/glm.hpp>

// ── Body part bones ────────────────────────────────────────────────────────
enum class BodyPart : int {
    Head = 0,
    Torso,
    LeftArm, RightArm,
    LeftLeg, RightLeg,
    COUNT
};

// ── Clothing section slots (Minecraft-style swap areas) ───────────────────
enum class ClothingSlot : int {
    Head  = 0,   // hat / helmet / hair
    Torso = 1,   // shirt / armor / jacket
    Arms  = 2,   // sleeves / gloves / bracers
    Legs  = 3,   // pants / skirt / armor
    Feet  = 4,   // shoes / boots / sandals
    COUNT = 5
};

// ── Per-bone scale ─────────────────────────────────────────────────────────
struct BoneScale {
    glm::vec3 v = { 1.f, 1.f, 1.f };
};

// ── One clothing section ───────────────────────────────────────────────────
struct ClothingConfig {
    int       preset = 0;                    // 0 = none, 1+ = a preset style
    glm::vec3 color  = { 0.2f, 0.4f, 0.8f };
    bool      on()  const { return preset > 0; }
};

// ── The full character definition ─────────────────────────────────────────
struct Character {
    BoneScale      scales[(int)BodyPart::COUNT];
    ClothingConfig clothing[(int)ClothingSlot::COUNT];
    glm::vec3      skinColor = { 0.89f, 0.73f, 0.58f };

    Character(); // sets default proportions
};
