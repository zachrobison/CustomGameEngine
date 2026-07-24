#include "UI.h"
#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include "WeaponWheel.h"
#include "../voxel/World.h"
#include "../platform/PlayerProfile.h"
#include "../character/Character.h"
#include "../character/CharacterRenderer.h"
#include "../character/EntityManager.h"
#include "../platform/KeyBinds.h"
#include "../game/GameConfig.h"
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <GLFW/glfw3.h>
#include "../vendor/stb_image.h"
#include <cstdio>
#include <cmath>
#include <cstring>

static ImVec4 col(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return { r/255.f, g/255.f, b/255.f, a/255.f };
}

void UI::applyObsidianStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding = 6.f; s.FrameRounding = 4.f;
    s.ScrollbarRounding = 4.f; s.GrabRounding = 4.f;
    s.ItemSpacing = {8,5}; s.FramePadding = {6,4};
    s.WindowPadding = {10,10}; s.IndentSpacing = 14.f;
    s.ScrollbarSize = 10.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]              = col(30,30,46);
    c[ImGuiCol_ChildBg]               = col(24,24,37);
    c[ImGuiCol_PopupBg]               = col(30,30,46);
    c[ImGuiCol_Border]                = col(69,71,90,140);
    c[ImGuiCol_FrameBg]               = col(49,50,68);
    c[ImGuiCol_FrameBgHovered]        = col(69,71,90);
    c[ImGuiCol_FrameBgActive]         = col(88,91,112);
    c[ImGuiCol_TitleBg]               = col(17,17,27);
    c[ImGuiCol_TitleBgActive]         = col(17,17,27);
    c[ImGuiCol_ScrollbarBg]           = col(24,24,37);
    c[ImGuiCol_ScrollbarGrab]         = col(88,91,112);
    c[ImGuiCol_ScrollbarGrabHovered]  = col(108,112,134);
    c[ImGuiCol_ScrollbarGrabActive]   = col(203,166,247);
    c[ImGuiCol_CheckMark]             = col(203,166,247);
    c[ImGuiCol_SliderGrab]            = col(203,166,247);
    c[ImGuiCol_SliderGrabActive]      = col(220,190,255);
    c[ImGuiCol_Button]                = col(49,50,68);
    c[ImGuiCol_ButtonHovered]         = col(88,91,112);
    c[ImGuiCol_ButtonActive]          = col(203,166,247);
    c[ImGuiCol_Header]                = col(49,50,68,200);
    c[ImGuiCol_HeaderHovered]         = col(88,91,112);
    c[ImGuiCol_HeaderActive]          = col(203,166,247);
    c[ImGuiCol_Separator]             = col(69,71,90,120);
    c[ImGuiCol_Text]                  = col(205,214,244);
    c[ImGuiCol_TextDisabled]          = col(108,112,134);
}

int UI::render(WorldSettings& ws, PlayerSettings& ps, bool& flyMode,
               std::vector<Weapon*>& weapons, int& activeSlot, World& world,
               Character& character, CharacterRenderer& charRenderer,
               EntityManager& entities,
               const std::string& savePath,
               GLFWwindow* window, bool& mouseCaptured, bool fHeld) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);

    if (!playMode) {
        ImGui::SetNextWindowPos({0,0}, ImGuiCond_Always);
        ImGui::SetNextWindowSize({(float)PANEL_W, (float)winH}, ImGuiCond_Always);
        ImGui::SetNextWindowBgAlpha(1.f);

        ImGuiWindowFlags flags = ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoBringToFrontOnFocus
            | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar;
        ImGui::Begin("##panel", nullptr, flags);

        // Player info strip
        auto& prof = PlayerProfile::get();
        ImGui::PushStyleColor(ImGuiCol_Text, col(108,112,134));
        ImGui::Text("  %s  %s", prof.name().c_str(),
            prof.hasSteam() ? "[Steam]" : "[Local]");
        ImGui::PopStyleColor();
        ImGui::Spacing();

        // Play button + back to game picker
        ImGui::PushStyleColor(ImGuiCol_Button,        col(39,100,55));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col(55,140,75));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col(100,200,120));
        ImGui::PushStyleColor(ImGuiCol_Text,          col(166,218,149));
        if (ImGui::Button("  >  PLAY  ", {-70, 26}))
            playMode = true;
        ImGui::PopStyleColor(4);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
        if (ImGui::Button("Games", {-1, 26}))
            requestMenu = true;
        ImGui::PopStyleColor();
        ImGui::Spacing();

        ImGui::BeginChild("##scroll", {-1, (float)winH - 90.f}, false,
                           ImGuiWindowFlags_HorizontalScrollbar);

        if (scenePanel.render(ws, ps, flyMode, weapons, activeSlot,
                              character, charRenderer, entities,
                              statusMsg, world, savePath))
            charRenderer.rebuild(character);

        // ── Keybinds & per-project toggles ────────────────────────────────
        if (keyBinds && ImGui::CollapsingHeader("  Keybinds & Toggles")) {
            ImGui::Indent();
            ImGui::Checkbox("Dimension tints", &dimensionTints);
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, col(108,112,134));
            ImGui::TextWrapped("Type a key name (Q, SPACE, LSHIFT...) to rebind. Saved per project.");
            ImGui::PopStyleColor();
            for (auto& [action, code] : keyBinds->all()) {
                char buf[16];
                snprintf(buf, sizeof(buf), "%s", KeyBinds::keyName(code));
                ImGui::SetNextItemWidth(80);
                char id[80]; snprintf(id, sizeof(id), "##kb_%s", action.c_str());
                if (ImGui::InputText(id, buf, sizeof(buf),
                                     ImGuiInputTextFlags_EnterReturnsTrue |
                                     ImGuiInputTextFlags_CharsUppercase)) {
                    int newCode = KeyBinds::keyCode(buf);
                    if (newCode >= 0) {
                        keyBinds->set(action, newCode);
                        keyBinds->save(keyBindsPath);
                        statusMsg = "Rebound " + action + " to " + KeyBinds::keyName(newCode);
                    }
                }
                ImGui::SameLine();
                ImGui::Text("%s", action.c_str());
            }
            ImGui::Unindent();
        }

        ImGui::EndChild();
        ImGui::End();
    }

    // Weapon wheel overlay
    int newSlot = -1;
    if (fHeld || weaponWheel.isOpen()) {
        int wheelPanelW = playMode ? 0 : PANEL_W;
        newSlot = weaponWheel.render(weapons, activeSlot, winW, winH, wheelPanelW, fHeld);
    }

    // Crosshair: always visible in play mode, hidden while wheel is open otherwise
    int xOff = playMode ? 0 : PANEL_W;

    // Boss health bar (top-centre during a boss fight)
    if (bossActive && (mouseCaptured || playMode) && levelWin <= 0.f) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float bcx = xOff + (winW - xOff) * 0.5f;
        const char* nm = "THE DON";
        ImVec2 ns = ImGui::CalcTextSize(nm);
        dl->AddText({bcx - ns.x*0.5f, 12}, IM_COL32(230,150,170,255), nm);
        float bw = 340.f, bh = 14.f, bx = bcx - bw*0.5f, by = 14 + ns.y;
        float f = bossHealthFrac < 0 ? 0 : (bossHealthFrac > 1 ? 1 : bossHealthFrac);
        dl->AddRectFilled({bx-2, by-2}, {bx+bw+2, by+bh+2}, IM_COL32(0,0,0,170), 3.f);
        dl->AddRectFilled({bx, by}, {bx+bw*f, by+bh}, IM_COL32(200,50,60,240), 3.f);
        dl->AddRect({bx-2, by-2}, {bx+bw+2, by+bh+2}, IM_COL32(140,140,150,200), 3.f);
    }

    // Level-clear banner (boss defeated)
    if (levelWin > 0.f) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float lcx = xOff + (winW - xOff) * 0.5f;
        const char* msg = "LEVEL CLEARED";
        ImFont* font = ImGui::GetFont();
        ImVec2 ms = font->CalcTextSizeA(46.f, FLT_MAX, 0.f, msg);
        float ly = winH * 0.34f;
        dl->AddRectFilled({lcx - ms.x*0.5f - 20, ly - 10}, {lcx + ms.x*0.5f + 20, ly + ms.y + 10},
                          IM_COL32(0,0,0,170), 6.f);
        dl->AddText(font, 46.f, {lcx - ms.x*0.5f, ly}, IM_COL32(150,255,170,255), msg);
        const char* sub = "returning to menu...";
        ImVec2 ss = ImGui::CalcTextSize(sub);
        dl->AddText({lcx - ss.x*0.5f, ly + ms.y + 14}, IM_COL32(210,215,235,220), sub);
    }

    // ── Dimension tint overlay (hardcoded per level id) ──────────────────
    if (dimensionTints) {
        const std::string& lvl = scenePanel.currentLevelId();
        ImU32 tint = 0;
        if      (lvl == "dim2") tint = IM_COL32(150,  70, 255, 46);  // purple
        else if (lvl == "dim3") tint = IM_COL32( 60, 220, 120, 42);  // green
        else if (lvl == "dim4") tint = IM_COL32(255, 120,  60, 42);  // orange
        else if (lvl != "default" && lvl != "dim1") {
            // any other dimension: stable hashed palette pick
            unsigned h = 2166136261u;
            for (char ch : lvl) h = (h ^ (unsigned)ch) * 16777619u;
            static const ImU32 pal[] = {
                IM_COL32(255,  80, 160, 42), IM_COL32( 80, 180, 255, 42),
                IM_COL32(255, 220,  80, 38), IM_COL32(160, 255, 120, 38),
            };
            tint = pal[h % 4];
        }
        if (tint) {
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddRectFilled({(float)xOff, 0.f}, {(float)winW, (float)winH}, tint);
        }
    }
    if (!weaponWheel.isOpen() && (mouseCaptured || playMode))
        drawCrosshair(winW, winH, xOff);

    // Horde level in edit mode: tell the player waves need PLAY
    if (hordePrestart) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float hcx = xOff + (winW - xOff) * 0.5f;
        const char* msg = "HORDE DEFENSE — press PLAY to start the waves";
        ImVec2 ms = ImGui::CalcTextSize(msg);
        dl->AddRectFilled({hcx - ms.x*0.5f - 10, 10}, {hcx + ms.x*0.5f + 10, 18 + ms.y},
                          IM_COL32(0,0,0,140), 4.f);
        dl->AddText({hcx - ms.x*0.5f, 14}, IM_COL32(255,230,120,230), msg);
    }

    // ── Horde Defense banner: wave, enemies, base health (top-centre) ────
    if (hordeActive && (mouseCaptured || playMode)) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float hcx = xOff + (winW - xOff) * 0.5f;

        char line[64];
        ImU32 col = IM_COL32(255,255,255,230);
        if      (hordeVictory) { snprintf(line, sizeof(line), "VICTORY — fort held!");
                                 col = IM_COL32(140,255,160,255); }
        else if (hordeDefeat)  { snprintf(line, sizeof(line), "DEFEAT — the fort has fallen");
                                 col = IM_COL32(255,90,90,255); }
        else if (hordeCountdown > 0.f)
            snprintf(line, sizeof(line), "WAVE %d IN %.0f",
                     hordeWave + 1, hordeCountdown + 0.99f);
        else if (hordeWaves == 0)   // endless
            snprintf(line, sizeof(line), "WAVE %d   —   %d left",
                     hordeWave, hordeLeft);
        else
            snprintf(line, sizeof(line), "WAVE %d / %d   —   %d left",
                     hordeWave, hordeWaves, hordeLeft);

        // Breather gets the big treatment + skip hint; combat stays compact
        float fontSz = (hordeCountdown > 0.f && !hordeVictory && !hordeDefeat)
                     ? 34.f : ImGui::GetFontSize();
        ImFont* font = ImGui::GetFont();
        ImVec2 ls = font->CalcTextSizeA(fontSz, FLT_MAX, 0.f, line);
        dl->AddRectFilled({hcx - ls.x*0.5f - 12, 10}, {hcx + ls.x*0.5f + 12, 16 + ls.y + 8},
                          IM_COL32(0,0,0,140), 4.f);
        dl->AddText(font, fontSz, {hcx - ls.x*0.5f, 14}, col, line);
        if (hordeCountdown > 0.f && !hordeVictory && !hordeDefeat) {
            const char* hint = "[E]  launch wave now";
            ImVec2 hsz = ImGui::CalcTextSize(hint);
            dl->AddText({hcx - hsz.x*0.5f, 20 + ls.y + 8},
                        IM_COL32(255,230,120,220), hint);
        }

        // Base (tower) health bar under the banner
        float bw2 = 220.f, bh2 = 10.f, bx2 = hcx - bw2*0.5f, by2 = 20.f + ls.y + 8.f;
        float frac = hordeBaseMax > 0 ? hordeBase / hordeBaseMax : 0.f;
        frac = frac < 0.f ? 0.f : (frac > 1.f ? 1.f : frac);
        dl->AddRectFilled({bx2-1, by2-1}, {bx2+bw2+1, by2+bh2+1}, IM_COL32(0,0,0,150), 3.f);
        dl->AddRectFilled({bx2, by2}, {bx2 + bw2*frac, by2+bh2},
                          frac > 0.35f ? IM_COL32(250,205,80,230)
                                       : IM_COL32(255,110,50,240), 3.f);
        const char* lbl = "TOWER";
        dl->AddText({bx2 + bw2 + 8, by2 - 3}, IM_COL32(210,215,235,200), lbl);
    }

    // ── Player HUD: health + jetpack fuel bars (top-left of viewport) ────
    if ((mouseCaptured || playMode) && !weaponWheel.isOpen() && !inVehicle) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float bx = xOff + 14.f, by = 14.f, bw = 180.f, bh = 12.f;

        // Health
        float hFrac = playerMaxHealth > 0 ? playerHealth / playerMaxHealth : 0.f;
        hFrac = hFrac < 0.f ? 0.f : (hFrac > 1.f ? 1.f : hFrac);
        dl->AddRectFilled({bx-1, by-1}, {bx+bw+1, by+bh+1}, IM_COL32(0,0,0,150), 3.f);
        dl->AddRectFilled({bx, by}, {bx+bw*hFrac, by+bh},
                          hFrac > 0.35f ? IM_COL32(220,60,70,220)
                                        : IM_COL32(255,120,40,240), 3.f);
        char hbuf[24]; snprintf(hbuf, sizeof(hbuf), "%.0f", playerHealth);
        dl->AddText({bx+bw+8, by-1}, IM_COL32(255,255,255,200), hbuf);

        // Jetpack fuel
        float fy = by + bh + 6.f;
        float fFrac = jetMaxFuel > 0 ? jetFuel / jetMaxFuel : 0.f;
        dl->AddRectFilled({bx-1, fy-1}, {bx+bw+1, fy+8+1}, IM_COL32(0,0,0,150), 3.f);
        dl->AddRectFilled({bx, fy}, {bx+bw*fFrac, fy+8}, IM_COL32(80,170,255,220), 3.f);

        // Rocket charge (only while held)
        if (rocketCharge >= 0.f) {
            float ry = fy + 8 + 6.f;
            float rFrac = rocketCharge / 2.f;
            dl->AddRectFilled({bx-1, ry-1}, {bx+bw+1, ry+8+1}, IM_COL32(0,0,0,150), 3.f);
            dl->AddRectFilled({bx, ry}, {bx+bw*rFrac, ry+8},
                              rFrac >= 1.f ? IM_COL32(255,230,80,255)
                                           : IM_COL32(255,160,60,220), 3.f);
        }
    }

    // ── Vehicle HUD ───────────────────────────────────────────────────────
    if ((mouseCaptured || playMode) && !weaponWheel.isOpen()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        float cx = xOff + (winW - xOff) * 0.5f;

        if (inVehicle) {
            // Minimal HUD only — the 3D interior mesh is the dashboard now.
            float kph = std::abs(vehicleSpeed) * 3.6f;
            char  buf[24]; snprintf(buf, sizeof(buf), "%.0f", kph);
            ImU32 spdCol = vehicleNitrous  ? IM_COL32(80,200,255,255) :
                           vehicleDrifting ? IM_COL32(255,180,40,255)  :
                                            IM_COL32(255,255,255,235);
            // Big speed number, bottom-right
            float sx = winW - 150.f, sy = winH - 70.f;
            ImGui::PushFont(nullptr);
            ImVec2 ns = ImGui::GetFont()->CalcTextSizeA(40.f, FLT_MAX, 0.f, buf);
            dl->AddText(ImGui::GetFont(), 40.f, {sx - ns.x, sy}, spdCol, buf);
            dl->AddText({sx + 6, sy + 18}, IM_COL32(180,200,230,200), "km/h");
            ImGui::PopFont();
            if (vehicleDrifting) dl->AddText({sx - 40, sy - 20}, IM_COL32(255,150,40,230), "DRIFT");
            if (vehicleNitrous)  dl->AddText({sx + 40, sy - 20}, IM_COL32(80,200,255,230), "NOS");
            const char* hint = "[E] Exit   [WASD] Drive   [Space] Handbrake   [Shift] Nitrous   [V] View";
            dl->AddText({(float)xOff + 12.f, winH - 22.f}, IM_COL32(160,190,220,180), hint);

        } else if (nearVehicle) {
            // Entry prompt — below crosshair
            const char* prompt = "[E]  Enter Vehicle";
            ImVec2 ps2 = ImGui::CalcTextSize(prompt);
            float px = cx - ps2.x * 0.5f;
            float py = winH * 0.5f + 30.f;
            dl->AddRectFilled({px-10, py-5}, {px+ps2.x+10, py+ps2.y+5},
                              IM_COL32(0,0,0,140), 4.f);
            dl->AddText({px, py}, IM_COL32(255,230,100,240), prompt);
        }
    }

    if (extraHud) extraHud();      // game-specific overlay (e.g. factory HUD)

    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return newSlot;
}

int UI::renderMainMenu(const std::vector<GameConfig>& games, GLFWwindow* window) {
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    int winW, winH;
    glfwGetWindowSize(window, &winW, &winH);
    ImGui::SetNextWindowPos({winW * 0.5f, winH * 0.5f}, ImGuiCond_Always, {0.5f, 0.5f});
    ImGui::SetNextWindowSize({540.f, 0.f});
    ImGui::Begin("##mainmenu", nullptr,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

    ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
    ImGui::SetWindowFontScale(1.6f);
    ImGui::Text("  VOXEL ENGINE");
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, col(108,112,134));
    ImGui::Text("  pick a game project");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    int picked = -1;
    static const ImVec4 accents[] = {
        col(166,218,149), col(137,180,250), col(249,226,175), col(245,194,231)
    };
    for (int i = 0; i < (int)games.size(); i++) {
        const GameConfig& g = games[i];
        ImGui::PushStyleColor(ImGuiCol_Text, accents[i % 4]);
        char lbl[128];
        snprintf(lbl, sizeof(lbl), "  %s##game%d", g.name.c_str(), i);
        if (ImGui::Button(lbl, {-1, 34})) picked = i;
        ImGui::PopStyleColor();
        if (!g.description.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, col(108,112,134));
            ImGui::TextWrapped("   %s", g.description.c_str());
            ImGui::PopStyleColor();
        }
        ImGui::Spacing();
    }
    if (games.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, col(243,139,168));
        ImGui::TextWrapped("No games found. Create <save>/games/<id>/game.json.");
        ImGui::PopStyleColor();
    }

    ImGui::End();
    ImGui::Render();
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    return picked;
}

void UI::drawCrosshair(int winW, int winH, int xOffset) const {
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    float cx = xOffset + (winW - xOffset) * 0.5f;
    float cy = winH * 0.5f;
    float sz = 9.f, gap = 3.f;
    ImU32 shadow = IM_COL32(0,0,0,100);
    ImU32 white  = IM_COL32(255,255,255,200);

    // Hit marker: red diagonal X that pops out and fades after a hit lands
    if (hitMarker > 0.f) {
        float t = hitMarker / 0.18f;                 // 1 → fresh hit
        float d0 = gap + 3.f, d1 = sz + 5.f + 6.f * t;
        ImU32 hc = IM_COL32(255, 70, 70, (int)(90 + 165 * t));
        for (int sx = -1; sx <= 1; sx += 2)
            for (int sy = -1; sy <= 1; sy += 2)
                dl->AddLine({cx + sx*d0, cy + sy*d0},
                            {cx + sx*d1, cy + sy*d1}, hc, 2.5f);
    }
    dl->AddLine({cx-sz-1,cy+1},{cx-gap-1,cy+1}, shadow,2.f);
    dl->AddLine({cx+gap+1,cy+1},{cx+sz+1,cy+1}, shadow,2.f);
    dl->AddLine({cx+1,cy-sz-1},{cx+1,cy-gap-1}, shadow,2.f);
    dl->AddLine({cx+1,cy+gap+1},{cx+1,cy+sz+1}, shadow,2.f);
    dl->AddLine({cx-sz,cy},{cx-gap,cy}, white,2.f);
    dl->AddLine({cx+gap,cy},{cx+sz,cy}, white,2.f);
    dl->AddLine({cx,cy-sz},{cx,cy-gap}, white,2.f);
    dl->AddLine({cx,cy+gap},{cx,cy+sz}, white,2.f);
}
