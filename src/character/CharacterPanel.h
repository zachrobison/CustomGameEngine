#pragma once
#include "Character.h"
#include "CharacterRenderer.h"
#include "AnimationClip.h"
#include "EntityManager.h"
#include <string>

// Self-contained ImGui panel section for the character creator.
// Call render() once per frame inside an ImGui window.
class CharacterPanel {
public:
    CharacterPanel();

    // Returns true if any character data changed (rebuild needed).
    bool render(Character& c, CharacterRenderer& renderer, EntityManager& entities);

private:
    float   rotY      = 0.2f;
    bool    autoSpin  = true;
    float   spinSpeed = 0.6f;
    double  lastTime  = 0.0;

    // Animation UI state
    int         selectedClip  = 0; // index into clip list
    std::string bvhUrl;
    std::string bvhStatus;
    std::vector<AnimationClip> loadedClips;
    AnimationState             previewAnim;

    // Entity spawning UI state
    char        spawnName[64] = "NPC";
    float       spawnPos[3]   = {5.f, 0.f, 0.f};

    bool renderBoneSection      (Character& c);
    bool renderClothingSection  (Character& c);
    bool renderAppearanceSection(Character& c);
    void renderAnimationSection (Character& c, CharacterRenderer& renderer);
    void renderEntitySection    (Character& c, EntityManager& entities);
};
