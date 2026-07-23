#pragma once
#include "Character.h"
#include "AnimationClip.h"
#include <string>
#include <glm/glm.hpp>

struct CharacterEntity {
    std::string  id;       // UUID or player ID
    std::string  name;
    Character    character;
    glm::vec3    position  = {0,0,0};
    float        facingY   = 0.f;  // world-space Y rotation (radians)
    AnimationState anim;
    bool         isPlayer  = false;
    float        health    = 100.f;
    float        maxHealth = 100.f;
    bool         dead      = false;
    float        hitFlash  = 0.f;   // >0 briefly after taking damage (white flash)
};
