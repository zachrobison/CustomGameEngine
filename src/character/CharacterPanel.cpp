#include "CharacterPanel.h"
#include "imgui.h"
#include <GLFW/glfw3.h>
#include <cmath>
#include <cstring>

// Forward declarations from CharacterRenderer.cpp
int         characterPresetCount(ClothingSlot s);
const char* characterPresetName(ClothingSlot s, int i);

CharacterPanel::CharacterPanel() {
    lastTime = glfwGetTime();
}

// ── Helpers ───────────────────────────────────────────────────────────────

static ImVec4 toImVec4(glm::vec3 v) { return {v.r, v.g, v.b, 1.f}; }

static bool vec3ColorPicker(const char* id, glm::vec3& col) {
    float f[3] = {col.r, col.g, col.b};
    bool changed = ImGui::ColorEdit3(id, f,
        ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_PickerHueWheel);
    if (changed) col = {f[0], f[1], f[2]};
    return changed;
}

// ── Bone scaling section ──────────────────────────────────────────────────

static const char* PART_LABELS[(int)BodyPart::COUNT] = {
    "Head", "Torso", "L Arm", "R Arm", "L Leg", "R Leg"
};

bool CharacterPanel::renderBoneSection(Character& c) {
    bool changed = false;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(137/255.f,180/255.f,250/255.f,1.f));
    ImGui::Text("  Body Proportions");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    for (int i = 0; i < (int)BodyPart::COUNT; i++) {
        ImGui::PushID(i);
        ImGui::Text("%s", PART_LABELS[i]);

        float* s = &c.scales[i].v.x;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##sx", &s[0], 0.4f, 2.5f, "W %.2f"))
            changed = true;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##sy", &s[1], 0.4f, 2.5f, "H %.2f"))
            changed = true;
        ImGui::SetNextItemWidth(-1);
        if (ImGui::SliderFloat("##sz", &s[2], 0.4f, 2.5f, "D %.2f"))
            changed = true;

        // Quick "uniform" button
        ImGui::SameLine();
        if (ImGui::SmallButton("=")) {
            s[1] = s[2] = s[0];
            changed = true;
        }
        if (ImGui::IsItemHovered())
            ImGui::SetTooltip("Lock W/H/D to same value");

        ImGui::Spacing();
        ImGui::PopID();
    }
    return changed;
}

// ── Clothing section ──────────────────────────────────────────────────────

static const char* SLOT_LABELS[(int)ClothingSlot::COUNT] = {
    "Head", "Torso", "Arms", "Legs", "Feet"
};
static const ImVec4 SLOT_ACCENT[(int)ClothingSlot::COUNT] = {
    {203/255.f,166/255.f,247/255.f,1.f},   // purple - head
    {250/255.f,179/255.f,135/255.f,1.f},   // peach  - torso
    {137/255.f,180/255.f,250/255.f,1.f},   // blue   - arms
    {166/255.f,218/255.f,149/255.f,1.f},   // green  - legs
    {243/255.f,139/255.f,168/255.f,1.f},   // red    - feet
};

bool CharacterPanel::renderClothingSection(Character& c) {
    bool changed = false;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(203/255.f,166/255.f,247/255.f,1.f));
    ImGui::Text("  Clothing Slots");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    for (int s = 0; s < (int)ClothingSlot::COUNT; s++) {
        ClothingConfig& cfg = c.clothing[s];
        ClothingSlot   slot = (ClothingSlot)s;

        ImGui::PushID(s);
        ImGui::PushStyleColor(ImGuiCol_Text, SLOT_ACCENT[s]);
        ImGui::Text("%s", SLOT_LABELS[s]);
        ImGui::PopStyleColor();

        // Preset dropdown
        const char* current = characterPresetName(slot, cfg.preset);
        ImGui::SetNextItemWidth(-50.f);
        if (ImGui::BeginCombo("##preset", current)) {
            int count = characterPresetCount(slot);
            for (int p = 0; p < count; p++) {
                bool sel = (cfg.preset == p);
                if (ImGui::Selectable(characterPresetName(slot, p), sel)) {
                    cfg.preset = p;
                    changed = true;
                }
                if (sel) ImGui::SetItemDefaultFocus();
            }
            ImGui::EndCombo();
        }

        // Inline color swatch (only useful when clothing is on)
        if (cfg.on()) {
            ImGui::SameLine();
            ImGui::ColorButton("##sw", toImVec4(cfg.color),
                ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {18,18});
            if (ImGui::IsItemClicked()) ImGui::OpenPopup("##cpick");
        }

        // Color picker popup
        if (ImGui::BeginPopup("##cpick")) {
            if (vec3ColorPicker("##col", cfg.color)) changed = true;
            ImGui::EndPopup();
        }

        ImGui::Spacing();
        ImGui::PopID();
    }
    return changed;
}

// ── Appearance section (skin color, spin) ────────────────────────────────

bool CharacterPanel::renderAppearanceSection(Character& c) {
    bool changed = false;
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(148/255.f,226/255.f,213/255.f,1.f));
    ImGui::Text("  Appearance");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Skin Color");
    ImGui::SameLine();
    ImGui::ColorButton("##skinsw", toImVec4(c.skinColor),
        ImGuiColorEditFlags_NoTooltip | ImGuiColorEditFlags_NoPicker, {18,18});
    if (ImGui::IsItemClicked()) ImGui::OpenPopup("##skinpick");
    if (ImGui::BeginPopup("##skinpick")) {
        if (vec3ColorPicker("##sc", c.skinColor)) changed = true;
        ImGui::EndPopup();
    }

    ImGui::Spacing();
    ImGui::Checkbox("Auto-spin preview", &autoSpin);
    if (!autoSpin) {
        ImGui::SetNextItemWidth(-1);
        ImGui::SliderFloat("##rot", &rotY, 0.f, 6.2831f, "Rotation %.2f");
    }
    ImGui::SetNextItemWidth(-1);
    ImGui::SliderFloat("##sp", &spinSpeed, 0.1f, 3.f, "Spin speed %.1f");

    return changed;
}

// ── Animation section ─────────────────────────────────────────────────────

void CharacterPanel::renderAnimationSection(Character& /*c*/, CharacterRenderer& /*renderer*/) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(243/255.f,139/255.f,168/255.f,1.f));
    ImGui::Text("  Animation");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    // Built-in clip selector
    static const char* BUILTIN_NAMES[] = { "idle", "walk", "run", "jump" };
    static const AnimationClip* BUILTIN_CLIPS[] = {
        &BuiltinAnims::idle(), &BuiltinAnims::walk(),
        &BuiltinAnims::run(),  &BuiltinAnims::jump()
    };
    constexpr int BUILTIN_COUNT = 4;

    // Build combined list: builtins + loaded
    std::vector<const char*>          names;
    std::vector<const AnimationClip*> clips;
    for (int i = 0; i < BUILTIN_COUNT; i++) { names.push_back(BUILTIN_NAMES[i]); clips.push_back(BUILTIN_CLIPS[i]); }
    for (auto& lc : loadedClips)             { names.push_back(lc.name.c_str());  clips.push_back(&lc); }

    const char* cur = (selectedClip < (int)names.size()) ? names[selectedClip] : "none";
    ImGui::SetNextItemWidth(-1);
    if (ImGui::BeginCombo("##anim", cur)) {
        for (int i = 0; i < (int)names.size(); i++) {
            bool sel = (i == selectedClip);
            if (ImGui::Selectable(names[i], sel)) {
                selectedClip = i;
                previewAnim.play(clips[i]);
            }
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }

    // Play / Stop
    ImGui::SameLine();
    if (previewAnim.playing) {
        if (ImGui::SmallButton("||")) previewAnim.stop();
    } else {
        if (ImGui::SmallButton("|>")) {
            if (selectedClip < (int)clips.size())
                previewAnim.play(clips[selectedClip]);
        }
    }

    ImGui::Spacing();

    // BVH download
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(108/255.f,112/255.f,134/255.f,1.f));
    ImGui::Text("BVH URL:");
    ImGui::PopStyleColor();
    char urlBuf[512];
    strncpy(urlBuf, bvhUrl.c_str(), sizeof(urlBuf)-1); urlBuf[sizeof(urlBuf)-1] = '\0';
    ImGui::SetNextItemWidth(-60.f);
    if (ImGui::InputText("##url", urlBuf, sizeof(urlBuf)))
        bvhUrl = urlBuf;
    ImGui::SameLine();
    if (ImGui::SmallButton("Load")) {
        loadedClips.emplace_back();
        if (BVHLoader::downloadAndLoad(bvhUrl, loadedClips.back(), bvhStatus)) {
            selectedClip = (int)(names.size()); // will be the new last entry next frame
            previewAnim.play(&loadedClips.back());
        } else {
            loadedClips.pop_back();
        }
    }
    if (!bvhStatus.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(148/255.f,226/255.f,213/255.f,1.f));
        ImGui::TextWrapped("  %s", bvhStatus.c_str());
        ImGui::PopStyleColor();
    }
}

// ── Entity spawning section ───────────────────────────────────────────────

void CharacterPanel::renderEntitySection(Character& c, EntityManager& entities) {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(166/255.f,218/255.f,149/255.f,1.f));
    ImGui::Text("  Spawn / Entities");
    ImGui::PopStyleColor();
    ImGui::Separator();
    ImGui::Spacing();

    ImGui::Text("Name");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##spname", spawnName, sizeof(spawnName));

    ImGui::Text("Pos");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputFloat3("##sppos", spawnPos, "%.1f");

    if (ImGui::Button("Spawn with current look", {-1, 0})) {
        // Generate a simple sequential id
        static int nextId = 1;
        std::string id = "npc_" + std::to_string(nextId++);
        CharacterEntity* e = entities.spawn(id, spawnName,
                                            {spawnPos[0], spawnPos[1], spawnPos[2]});
        if (e) e->character = c;
        entities.markDirty(id);
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(108/255.f,112/255.f,134/255.f,1.f));
    ImGui::Text("Active entities: %d", (int)entities.all().size());
    ImGui::PopStyleColor();

    // List with remove buttons
    for (auto& e : entities.all()) {
        ImGui::PushID(e.id.c_str());
        ImGui::Text("  %s (%s)", e.name.c_str(), e.id.c_str());
        ImGui::SameLine();
        if (ImGui::SmallButton("X")) {
            entities.remove(e.id);
            ImGui::PopID();
            break; // iterator invalidated
        }
        ImGui::PopID();
    }
}

// ── Main render entry ─────────────────────────────────────────────────────

bool CharacterPanel::render(Character& c, CharacterRenderer& renderer, EntityManager& entities) {
    // Update auto-spin
    double now = glfwGetTime();
    float  dt  = (float)(now - lastTime);
    lastTime   = now;
    if (autoSpin) rotY += spinSpeed * dt;

    bool changed = false;

    if (ImGui::CollapsingHeader("   Character Creator")) {
        // ── 3D preview ─────────────────────────────────────────────────
        ImGui::Spacing();
        GLuint tex = renderer.renderPreview(c, rotY);

        // Centre the preview image in the panel
        float panelW  = ImGui::GetContentRegionAvail().x;
        float imgW    = (float)CharacterRenderer::PREVIEW_W;
        float imgH    = (float)CharacterRenderer::PREVIEW_H;
        float imgDisplayW = panelW - 10.f;
        float imgDisplayH = imgH * (imgDisplayW / imgW);
        float offsetX = (panelW - imgDisplayW) * 0.5f;
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
        ImGui::Image((ImTextureID)(intptr_t)tex, {imgDisplayW, imgDisplayH},
                     {0,1},{1,0}); // flip Y (OpenGL origin is bottom-left)

        // Drag to manually rotate
        if (ImGui::IsItemHovered() && ImGui::IsMouseDragging(0)) {
            rotY += ImGui::GetIO().MouseDelta.x * 0.01f;
            autoSpin = false;
        }
        ImGui::Spacing();

        // Advance preview animation
        double now2 = glfwGetTime();
        previewAnim.update((float)(now2 - lastTime)); // lastTime already updated above

        // ── Sub-sections ───────────────────────────────────────────────
        if (ImGui::CollapsingHeader("  Bones", ImGuiTreeNodeFlags_DefaultOpen))
            if (renderBoneSection(c)) changed = true;
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("  Clothing", ImGuiTreeNodeFlags_DefaultOpen))
            if (renderClothingSection(c)) changed = true;
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("  Appearance"))
            if (renderAppearanceSection(c)) changed = true;
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("  Animation"))
            renderAnimationSection(c, renderer);
        ImGui::Spacing();

        if (ImGui::CollapsingHeader("  Entities"))
            renderEntitySection(c, entities);
    }

    return changed;
}
