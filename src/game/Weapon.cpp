#include "Weapon.h"
#include "../voxel/World.h"
#include "../character/EntityManager.h"
#include "../platform/Audio.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <cmath>

static float randF() { return (float)rand() / (float)RAND_MAX; }

// Apply damage to an entity; at 0 HP it's marked dead and removed.
// Returns true if the entity died.
static bool damageEntity(EntityManager& em, CharacterEntity& e, float dmg) {
    e.health -= dmg;
    e.hitFlash = 0.16f;
    if (e.health <= 0.f) {
        e.dead = true;
        em.remove(e.id);
        Audio::get().play("enemy_die", 0.7f);
        Audio::get().play("kill", 0.35f);       // score chime
        return true;
    }
    Audio::get().play("enemy_hurt", 0.5f);
    return false;
}

// Nearest non-player entity hit by a ray (entities approximated as a
// 2u-tall, 0.9u-radius capsule around their position). Returns null if none.
static CharacterEntity* rayHitEntity(EntityManager& em, glm::vec3 origin,
                                     glm::vec3 dir, float maxDist) {
    CharacterEntity* best = nullptr;
    float bestT = maxDist;
    for (auto& e : em.all()) {
        if (e.isPlayer || e.dead) continue;
        glm::vec3 center = e.position + glm::vec3(0, 1.f, 0);
        float t = glm::dot(center - origin, dir);
        if (t < 0.f || t > bestT) continue;
        glm::vec3 closest = origin + dir * t;
        glm::vec3 d       = center - closest;
        d.y *= 0.5f;                       // stretch tolerance vertically
        if (glm::length(d) < 0.9f) { best = &e; bestT = t; }
    }
    return best;
}

static void fireRays(const Camera& cam, World& world,
                     int count, float spreadDeg, float range, uint8_t placetype) {
    glm::vec3 fwd = cam.forward();
    glm::vec3 r   = cam.right();
    glm::vec3 u   = cam.up();
    float     rad = glm::radians(spreadDeg);

    for (int i = 0; i < count; i++) {
        float angle  = randF() * 6.2831853f;
        // squared radius biases grains toward the cone centre — material
        // clumps where you aim instead of scattering stragglers at the edges
        float spread = randF() * randF() * rad;
        glm::vec3 dir = glm::normalize(
            fwd + (r * std::cos(angle) + u * std::sin(angle)) * std::tan(spread));

        glm::ivec3 hit, norm;
        if (world.raycast(cam.position, dir, range, hit, norm)) {
            glm::ivec3 t = (placetype == 0) ? hit : hit + norm;
            if (placetype == 0) {
                world.setVoxelWorld(t.x, t.y, t.z, placetype);
                continue;
            }
            // Light sand settle: grains fall to rest, then roll at most a
            // couple of steps (coin-flip each) — softens spires into steep
            // piles without the material wandering away from your aim.
            static const int RX[4] = {1,-1,0,0}, RZ[4] = {0,0,1,-1};
            int fallBudget = 30;
            while (fallBudget > 0 && t.y > 1 &&
                   !world.getVoxelWorld(t.x, t.y - 1, t.z)) {
                t.y--; fallBudget--;
            }
            for (int roll = 0; roll < 2; roll++) {
                if (rand() % 2) break;                 // 50% stop per step
                int  s = rand() % 4;
                bool moved = false;
                for (int k = 0; k < 4; k++) {
                    int j  = (s + k) % 4;
                    int nx = t.x + RX[j], nz = t.z + RZ[j];
                    if (!world.getVoxelWorld(nx, t.y, nz) &&
                        !world.getVoxelWorld(nx, t.y - 1, nz)) {
                        t.x = nx; t.z = nz; t.y--;     // roll down one step
                        moved = true;
                        break;
                    }
                }
                if (!moved) break;
                // keep falling after a roll if there's a drop
                while (fallBudget > 0 && t.y > 1 &&
                       !world.getVoxelWorld(t.x, t.y - 1, t.z)) {
                    t.y--; fallBudget--;
                }
            }
            if (fallBudget > 0 && !world.getVoxelWorld(t.x, t.y, t.z))
                world.setVoxelWorld(t.x, t.y, t.z, placetype);
        }
    }
}

// ── VoxelSprayer ─────────────────────────────────────────────────────────────

void VoxelSprayer::onFire(const Camera& cam, World& world, EntityManager*) {
    Audio::get().play("spray", 0.35f);
    fireRays(cam, world, sprayCount, spreadAngle, sprayRange, voxelType);
}

void VoxelSprayer::renderUI() {
    // Color swatches for type
    static const char* names[] = {
        "Air","Dirt","Grass","Stone","Water",
        "Sand","Wood","Leaves","Snow","Gravel","Red Clay"
    };
    static const float cols[11][3] = {
        {0,0,0},{.45f,.35f,.25f},{.3f,.6f,.2f},{.55f,.55f,.55f},
        {.2f,.4f,.8f},{.9f,.85f,.7f},{.4f,.25f,.15f},{.15f,.5f,.1f},
        {.85f,.88f,.95f},{.5f,.47f,.45f},{.65f,.3f,.2f}
    };

    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(250/255.f,179/255.f,135/255.f,1.f));
    ImGui::SliderInt  ("Spray Count",  &sprayCount,  1,    60);
    ImGui::SliderFloat("Spread",       &spreadAngle, 0.f,  60.f, "%.0f deg");
    ImGui::SliderFloat("Range",        &sprayRange,  1.f, 100.f, "%.0f");
    ImGui::SliderFloat("Fire Rate",    &fireRate,    0.02f, 1.f,  "%.2f s");
    ImGui::PopStyleColor();
    ImGui::Spacing();
    ImGui::Text("Voxel Type:");
    ImGui::SameLine();
    int t = (int)voxelType;
    ImGui::ColorButton("##sw", {cols[t][0],cols[t][1],cols[t][2],1.f},
        ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoPicker, {14,14});
    ImGui::SameLine();
    if (ImGui::BeginCombo("##vtype", names[t])) {
        for (int i = 1; i <= 10; i++) {
            ImGui::ColorButton("##c", {cols[i][0],cols[i][1],cols[i][2],1.f},
                ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoPicker, {12,12});
            ImGui::SameLine();
            bool sel = (voxelType == (uint8_t)i);
            if (ImGui::Selectable(names[i], sel)) voxelType = (uint8_t)i;
            if (sel) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
    }
}

// ── VoxelEraser ──────────────────────────────────────────────────────────────

void VoxelEraser::onFire(const Camera& cam, World& world, EntityManager*) {
    Audio::get().play("erase", 0.35f);
    fireRays(cam, world, eraseCount, spreadAngle, eraseRange, 0 /* air = erase */);
}

void VoxelEraser::renderUI() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(243/255.f,139/255.f,168/255.f,1.f));
    ImGui::SliderInt  ("Erase Count",  &eraseCount,  1,  40);
    ImGui::SliderFloat("Spread",       &spreadAngle, 0.f, 60.f, "%.0f deg");
    ImGui::SliderFloat("Range",        &eraseRange,  1.f, 100.f,"%.0f");
    ImGui::SliderFloat("Fire Rate",    &fireRate,    0.02f, 1.f, "%.2f s");
    ImGui::PopStyleColor();
}

// ── Gun ───────────────────────────────────────────────────────────────────────

void Gun::onFire(const Camera& cam, World& world, EntityManager* entities) {
    if (ammo <= 0) {
        Audio::get().play("switch", 0.4f, 0.6f);   // dry click
        return;
    }
    ammo--;
    Audio::get().play(rand() % 2 ? "laser" : "laser2", 0.5f,
                      0.95f + randF() * 0.1f);

    glm::vec3 fwd = cam.forward();
    glm::vec3 r   = cam.right();
    glm::vec3 u   = cam.up();
    float     rad = glm::radians(spread);

    float angle  = randF() * 6.2831853f;
    float s      = randF() * rad;
    glm::vec3 dir = glm::normalize(
        fwd + (r * std::cos(angle) + u * std::sin(angle)) * std::tan(s));

    // Entities checked before voxels: a body blocks the shot
    float voxelDist = range;
    glm::ivec3 hit, norm;
    bool voxelHit = world.raycast(cam.position, dir, range, hit, norm);
    if (voxelHit)
        voxelDist = glm::length((glm::vec3(hit) + glm::vec3(0.5f)) * VOXEL_SIZE
                                - cam.position);

    if (entities) {
        if (CharacterEntity* e = rayHitEntity(*entities, cam.position, dir, voxelDist)) {
            damageEntity(*entities, *e, damage);
            hitFlash = 0.18f;
            return;
        }
    }

    if (voxelHit && breakVoxels)
        world.setVoxelWorld(hit.x, hit.y, hit.z, 0);
}

void Gun::renderUI() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(148/255.f,226/255.f,213/255.f,1.f));
    ImGui::SliderFloat("Damage",    &damage,   1.f,  200.f, "%.0f");
    ImGui::SliderFloat("Range",     &range,    5.f,  200.f, "%.0f m");
    ImGui::SliderFloat("Spread",    &spread,   0.f,   20.f, "%.1f deg");
    ImGui::SliderFloat("Fire Rate", &fireRate, 0.02f,  2.f, "%.2f s");
    ImGui::SliderInt  ("Max Ammo",  &maxAmmo,  1,     200);
    ImGui::Checkbox   ("Auto Fire", &autoFire);
    ImGui::Checkbox   ("Break Voxels", &breakVoxels);
    ImGui::PopStyleColor();
    ImGui::Spacing();
    // Ammo display + reload
    ImGui::PushStyleColor(ImGuiCol_Text,
        ammo > maxAmmo/4 ? ImVec4(166/255.f,218/255.f,149/255.f,1.f)
                         : ImVec4(243/255.f,139/255.f,168/255.f,1.f));
    ImGui::Text("Ammo: %d / %d", ammo, maxAmmo);
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::SmallButton("Reload")) reload();
}

// ── Turret Placer ─────────────────────────────────────────────────────────

#include "Turrets.h"
#include "Bullets.h"
#include "../voxel/Chunk.h"

void TurretPlacer::onFire(const Camera& cam, World& world, EntityManager*) {
    glm::ivec3 hit, norm;
    if (!world.raycast(cam.position, cam.forward(), placeRange, hit, norm)) {
        Audio::get().play("switch", 0.4f, 0.5f);
        return;
    }
    // Stand the turret on top of the aimed voxel column
    glm::vec3 top = {(hit.x + 0.5f) * VOXEL_SIZE,
                     (hit.y + 1.0f) * VOXEL_SIZE,
                     (hit.z + 0.5f) * VOXEL_SIZE};
    Turrets::get().place(top);
}

void TurretPlacer::renderUI() {
    Turrets& t = Turrets::get();
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(137/255.f,180/255.f,250/255.f,1.f));
    ImGui::Text("Placed: %d / %d", t.count(), t.maxTurrets);
    ImGui::SliderInt  ("Max Turrets", &t.maxTurrets, 1, 12);
    ImGui::SliderFloat("Range",       &t.range,      4.f, 30.f, "%.0f");
    ImGui::SliderFloat("Damage",      &t.damage,     2.f, 60.f, "%.0f");
    ImGui::SliderFloat("Fire Rate",   &t.fireRate,   0.1f, 3.f, "%.2f s");
    if (ImGui::SmallButton("Clear all turrets")) t.clear();
    ImGui::PopStyleColor();
}

// ── Melee ─────────────────────────────────────────────────────────────────

void MeleeWeapon::onFire(const Camera& cam, World&, EntityManager* entities) {
    Audio::get().play("melee_swing", 0.55f, 0.9f + randF() * 0.2f);
    swingT = 0.25f;
    glm::vec3 fwd    = aimOverrideOn ? aimOverride : cam.forward();
    // Parry: hanging delayed-bullets in reach get sliced back at the shooter
    if (Bullets::get().trySlice(cam.position - glm::vec3(0, 0.5f, 0), fwd, range + 0.8f))
        hitFlash = 0.18f;
    if (!entities) return;
    float     cosArc = std::cos(glm::radians(arcDeg * 0.5f));

    // Collect ids first: damageEntity may remove from the vector mid-loop
    std::vector<std::string> hitIds;
    for (auto& e : entities->all()) {
        if (e.isPlayer || e.dead) continue;
        glm::vec3 to  = (e.position + glm::vec3(0, 1.f, 0)) - cam.position;
        to.y *= 0.4f;                       // forgive height differences
        float len = glm::length(to);
        if (len > range || len < 0.001f) continue;
        glm::vec3 fwdFlat = glm::normalize(glm::vec3(fwd.x, fwd.y * 0.4f, fwd.z));
        if (glm::dot(to / len, fwdFlat) >= cosArc) hitIds.push_back(e.id);
    }
    for (auto& id : hitIds)
        if (CharacterEntity* e = entities->find(id))
            damageEntity(*entities, *e, damage);
    if (!hitIds.empty()) {
        hitFlash = 0.18f;
        Audio::get().play("melee_hit", 0.6f);
    }
}

void MeleeWeapon::renderUI() {
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(203/255.f,166/255.f,247/255.f,1.f));
    ImGui::SliderFloat("Damage",    &damage,   1.f,  200.f, "%.0f");
    ImGui::SliderFloat("Range",     &range,    1.f,   6.f,  "%.1f m");
    ImGui::SliderFloat("Arc",       &arcDeg,  20.f, 180.f,  "%.0f deg");
    ImGui::SliderFloat("Swing Rate",&fireRate, 0.1f,  2.f,  "%.2f s");
    ImGui::PopStyleColor();
}
