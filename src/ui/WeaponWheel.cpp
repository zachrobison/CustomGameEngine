#include "WeaponWheel.h"
#include "imgui.h"
#include "../game/Weapon.h"
#include <cmath>
#include <cstring>
#include <vector>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

// ── Slice drawing ─────────────────────────────────────────────────────────

void WeaponWheel::drawSlice(ImDrawList* dl,
                             float cx, float cy,
                             float r0, float r1,
                             float a0, float a1,
                             unsigned int fillCol, unsigned int borderCol,
                             const char* label, bool active, bool highlighted) {
    const int SEGS = 18;
    float gap = (a1 - a0) > 0.25f ? 0.04f : 0.01f;
    a0 += gap; a1 -= gap;

    // Build annular sector polygon
    std::vector<ImVec2> pts;
    pts.reserve(SEGS * 2 + 4);

    for (int i = 0; i <= SEGS; i++) {
        float a = a0 + (a1 - a0) * i / SEGS;
        pts.push_back({ cx + r0 * cosf(a), cy + r0 * sinf(a) });
    }
    for (int i = SEGS; i >= 0; i--) {
        float a = a0 + (a1 - a0) * i / SEGS;
        pts.push_back({ cx + r1 * cosf(a), cy + r1 * sinf(a) });
    }

    dl->AddConvexPolyFilled(pts.data(), (int)pts.size(), fillCol);
    // Border
    dl->AddPolyline(pts.data(), (int)pts.size(), borderCol, ImDrawFlags_Closed, 1.5f);

    // Label in the middle of the slice at mid-radius
    float aMid = (a0 + a1) * 0.5f;
    float rMid = (r0 + r1) * 0.5f;
    float lx   = cx + rMid * cosf(aMid);
    float ly   = cy + rMid * sinf(aMid);

    ImVec2 tsz = ImGui::CalcTextSize(label);
    float  tx  = lx - tsz.x * 0.5f;
    float  ty  = ly - tsz.y * 0.5f;

    // Shadow + main text
    dl->AddText({ tx+1, ty+1 }, IM_COL32(0,0,0,180), label);
    unsigned int textCol = highlighted ? IM_COL32(255,255,255,255)
                         : active      ? IM_COL32(220,190,255,255)
                                       : IM_COL32(200,210,230,200);
    dl->AddText({ tx, ty }, textCol, label);
}

// ── Weapon colour ─────────────────────────────────────────────────────────

static unsigned int weaponAccent(Weapon* w, bool highlight, bool active) {
    // Try Gun first
    if (dynamic_cast<Gun*>(w)) {
        if (highlight) return IM_COL32(100,220,200,230);
        if (active)    return IM_COL32( 60,160,150,180);
        return                IM_COL32( 30, 80, 75,160);
    }
    // Sprayer / eraser / other
    if (highlight) return IM_COL32(180,140,230,230);
    if (active)    return IM_COL32(120, 90,180,180);
    return                IM_COL32( 55, 45, 80,160);
}

static unsigned int weaponBorder(Weapon* w, bool highlight) {
    if (dynamic_cast<Gun*>(w))
        return highlight ? IM_COL32(148,226,213,255) : IM_COL32( 80,160,150,200);
    return highlight     ? IM_COL32(203,166,247,255) : IM_COL32(100, 80,150,200);
}

// ── Main render ───────────────────────────────────────────────────────────

int WeaponWheel::render(const std::vector<Weapon*>& weapons, int activeSlot,
                         int winW, int winH, int panelW, bool qHeld) {
    int result = -1;

    // Detect open/close transitions
    bool justOpened  = qHeld && !wasOpen;
    bool justClosed  = !qHeld && wasOpen;
    wasOpen = open;

    if (justOpened) {
        open    = true;
        hovered = activeSlot;  // pre-select current weapon
        vx = vy = 0.f;
    }
    if (justClosed && open) {
        open   = false;
        result = (hovered >= 0 && hovered < (int)weapons.size()) ? hovered : -1;
    }
    if (!qHeld) { open = false; return result; }
    if (weapons.empty()) { open = false; return result; }

    // Accumulate mouse delta (works in cursor-captured mode)
    ImVec2 delta = ImGui::GetIO().MouseDelta;
    vx += delta.x;
    vy += delta.y;

    // Determine hovered slice from virtual cursor angle
    float dist = sqrtf(vx*vx + vy*vy);
    if (dist > 12.f) {
        float angle = atan2f(vy, vx) + (float)M_PI * 0.5f; // 0 at top
        if (angle < 0) angle += 2.f * (float)M_PI;
        float sliceAngle = 2.f * (float)M_PI / (int)weapons.size();
        hovered = (int)(angle / sliceAngle) % (int)weapons.size();
    }

    // ── Draw ──────────────────────────────────────────────────────────────
    ImDrawList* dl = ImGui::GetForegroundDrawList();

    // Centre of the 3D viewport
    float cx = panelW + (winW - panelW) * 0.5f;
    float cy = winH * 0.5f;

    int   N      = (int)weapons.size();
    float outer  = 110.f;
    float inner  =  38.f;
    float slice  = 2.f * (float)M_PI / N;

    // Background dim circle
    dl->AddCircleFilled({ cx, cy }, outer + 18.f, IM_COL32(10,10,20,160), 64);
    dl->AddCircle       ({ cx, cy }, outer + 18.f, IM_COL32(80,70,100,120), 64, 1.5f);

    for (int i = 0; i < N; i++) {
        float a0 = slice * i       - (float)M_PI * 0.5f;
        float a1 = slice * (i + 1) - (float)M_PI * 0.5f;

        bool hl  = (i == hovered);
        bool act = (i == activeSlot);

        drawSlice(dl, cx, cy, inner, outer, a0, a1,
                  weaponAccent(weapons[i], hl, act),
                  weaponBorder(weapons[i], hl),
                  weapons[i]->name.c_str(), act, hl);
    }

    // Centre hub: show active weapon name
    dl->AddCircleFilled({ cx, cy }, inner - 4.f, IM_COL32(20,18,30,220), 32);
    dl->AddCircle       ({ cx, cy }, inner - 4.f, IM_COL32(100,90,130,180), 32, 1.f);

    const char* centreLabel = (hovered >= 0 && hovered < N)
                            ? weapons[hovered]->name.c_str()
                            : (activeSlot < N ? weapons[activeSlot]->name.c_str() : "");
    ImVec2 clsz = ImGui::CalcTextSize(centreLabel);
    // Only fits if short enough; otherwise skip
    if (clsz.x < (inner - 4.f) * 2.f - 6.f) {
        dl->AddText({ cx - clsz.x*0.5f + 1, cy - clsz.y*0.5f + 1 },
                    IM_COL32(0,0,0,160), centreLabel);
        dl->AddText({ cx - clsz.x*0.5f,     cy - clsz.y*0.5f },
                    IM_COL32(210,200,240,240), centreLabel);
    }

    // Small "Q to cancel" hint
    const char* hint = "hold F — release to equip";
    ImVec2 hsz = ImGui::CalcTextSize(hint);
    dl->AddText({ cx - hsz.x*0.5f, cy + outer + 24.f },
                IM_COL32(120,110,150,200), hint);

    return result;
}
