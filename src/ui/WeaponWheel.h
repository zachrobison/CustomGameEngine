#pragma once
#include "../game/Weapon.h"
#include <vector>

// Radial weapon wheel drawn as an ImGui overlay.
// Hold Q to open, move mouse to select a slice, release Q to equip.
class WeaponWheel {
public:
    bool isOpen() const { return open; }

    // Call once per frame inside an active ImGui frame (after NewFrame, before Render).
    // qHeld: glfwGetKey(GLFW_KEY_Q) == GLFW_PRESS
    // Returns new active slot if the player released Q over a slot, else -1.
    int render(const std::vector<Weapon*>& weapons, int activeSlot,
               int winW, int winH, int panelW, bool qHeld);

private:
    bool  open     = false;
    bool  wasOpen  = false;
    int   hovered  = -1;
    float vx = 0, vy = 0;   // virtual cursor from accumulated mouse delta

    void drawSlice(struct ImDrawList* dl, float cx, float cy,
                   float r0, float r1, float a0, float a1,
                   unsigned int fillCol, unsigned int borderCol,
                   const char* label, bool active, bool highlighted);
};
