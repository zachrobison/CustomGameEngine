#include "ScenePanel.h"
#include "imgui.h"
#include <glm/glm.hpp>
#include "../voxel/World.h"
#include "../voxel/Noise.h"
#include "../platform/PlayerProfile.h"
#include "../platform/FileDialog.h"
#include "../platform/Audio.h"
#include "../game/Weapon.h"
#include "../vendor/json.hpp"
#include <cstring>
#include <cstdio>
#include <algorithm>
#if defined(_WIN32)
  #define strcasecmp _stricmp   // POSIX name → MSVC's equivalent
#endif
#include <cmath>
#include <fstream>

using json = nlohmann::json;

// Returns a Y position safely above the terrain at (wx, wz).
// Matches the noise formula used by Terrain and Player collision.
static float safeSpawnY(float wx, float wz) {
    float ix = wx / 4.f + 128.f;
    float iz = wz / 4.f + 128.f;
    float groundY = Noise::fractal(ix * 0.025f, iz * 0.025f, 4) * 28.f + 20.f;
    return groundY + 3.f;
}

// ── Colour helpers ────────────────────────────────────────────────────────

static ImVec4 col(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return { r/255.f, g/255.f, b/255.f, a/255.f };
}

static const ImVec4 ITEM_TYPE_COLORS[] = {
    col(137,180,250),  // Player       – blue
    col(243,139,168),  // Enemy        – red
    col(203,166,247),  // Weapon       – purple
    col(250,179,135),  // Projectile   – orange
    col(166,218,149),  // Level        – green
    col(250,200,135),  // NPC          – peach
    col(249,226,175),  // Pickup       – yellow
    col(148,226,213),  // Vehicle      – teal
    col(180,190,254),  // Camera       – lavender
    col(166,173,200),  // HUD          – overlay
    col(245,194,231),  // Audio        – pink
    col(242,143,143),  // Effect       – flamingo
    col(161,168,195),  // Construction – subtext
    col(116,199,236),  // Trigger Zone – sapphire
};

static ImVec4 actionCol()  { return col(137,180,250); }
static ImVec4 itemCol()    { return col(166,218,149); }
static ImVec4 varCol()     { return col(249,226,175); }
static ImVec4 dimCol()     { return col(108,112,134); }
static ImVec4 warnCol()    { return col(243,139,168); }

// Draw a selectable card row, text drawn via DrawList so it sits over the selectable bg.
static bool cardRow(int idx, bool selected, const ImVec4& tc,
                    const char* badge, const char* label,
                    bool orphan = false, bool brokenLink = false) {
    char lid[32]; snprintf(lid, sizeof(lid), "##card%d", idx);
    float rowW = ImGui::GetContentRegionAvail().x - 30.f;
    bool clicked = ImGui::Selectable(lid, selected, 0, {rowW, 34});

    ImVec2 rmin = ImGui::GetItemRectMin();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImU32 tc32 = ImGui::ColorConvertFloat4ToU32(tc);

    dl->AddRectFilled({rmin.x, rmin.y+4}, {rmin.x+3, rmin.y+30}, tc32);
    dl->AddText({rmin.x+8, rmin.y+4},  tc32,               badge);
    float bw = ImGui::CalcTextSize(badge).x;
    dl->AddText({rmin.x+8+bw+6, rmin.y+10}, IM_COL32(205,214,244,255), label);

    if (orphan)     dl->AddText({rmin.x+8+bw+6+ImGui::CalcTextSize(label).x+6,
                                 rmin.y+10}, IM_COL32(249,226,175,255), " (no triggers)");
    if (brokenLink) dl->AddText({rmin.x+8+bw+6+ImGui::CalcTextSize(label).x+6,
                                 rmin.y+10}, IM_COL32(243,139,168,255), " ⚠ broken ref");
    return clicked;
}

// ── ScenePanel ────────────────────────────────────────────────────────────

ScenePanel::ScenePanel() {}

void ScenePanel::addDefaultNodes(const std::vector<Weapon*>& weapons) {
    items.clear();
    actions.clear();
    globalVars.clear();
    {   GameItem it; it.id = nextId(); it.name = "Main Level";
        it.type = ItemType::Level; items.push_back(it); }
    {   GameItem it; it.id = nextId(); it.name = "Player";
        it.type = ItemType::Player; items.push_back(it); }
    for (int i = 0; i < (int)weapons.size(); i++) {
        GameItem it; it.id = nextId();
        it.name      = weapons[i]->name;
        it.type      = ItemType::Weapon;
        it.weaponIdx = i;
        items.push_back(it);
    }
    selectedItemId = items[0].id;
}

// ── Project persistence (JSON) ───────────────────────────────────────────
// This is the shared authoring surface: hand-write/generate entries here
// externally, or let the in-app GUI create/edit items and save back to the
// same file. Enums are stored as their display names so the file stays
// readable and stable across enum reordering.

static int nameIndex(const char* const* names, int count, const std::string& s, int fallback = 0) {
    for (int i = 0; i < count; i++) if (s == names[i]) return i;
    return fallback;
}

static json worldSettingsToJson(const WorldSettings& ws) {
    return {
        {"voxelWorld",    ws.voxelWorld},
        {"flatTerrain",   ws.flatTerrain},
        {"hideTerrain",   ws.hideTerrain},
        {"proceduralTerrain", ws.proceduralTerrain},
        {"customMeshPath",std::string(ws.customMeshPath)},
        {"gravity",       ws.gravity},
        {"wind_x",        ws.wind_x},
        {"wind_z",        ws.wind_z},
        {"render_dist",   ws.render_dist},
        {"fog_density",   ws.fog_density},
        {"sun_angle",     ws.sun_angle},
    };
}
static void worldSettingsFromJson(const json& j, WorldSettings& ws) {
    ws.voxelWorld  = j.value("voxelWorld", ws.voxelWorld);
    ws.flatTerrain = j.value("flatTerrain", ws.flatTerrain);
    ws.hideTerrain = j.value("hideTerrain", ws.hideTerrain);
    ws.proceduralTerrain = j.value("proceduralTerrain", ws.proceduralTerrain);
    std::string cmp = j.value("customMeshPath", std::string(ws.customMeshPath));
    snprintf(ws.customMeshPath, sizeof(ws.customMeshPath), "%s", cmp.c_str());
    ws.gravity     = j.value("gravity", ws.gravity);
    ws.wind_x      = j.value("wind_x", ws.wind_x);
    ws.wind_z      = j.value("wind_z", ws.wind_z);
    ws.render_dist = j.value("render_dist", ws.render_dist);
    ws.fog_density = j.value("fog_density", ws.fog_density);
    ws.sun_angle   = j.value("sun_angle", ws.sun_angle);
}

static json playerSettingsToJson(const PlayerSettings& ps) {
    return {
        {"move_speed",        ps.move_speed},
        {"fly_speed",         ps.fly_speed},
        {"mouse_sensitivity", ps.mouse_sensitivity},
        {"sprint_multiplier", ps.sprint_multiplier},
        {"jump_force",        ps.jump_force},
    };
}
static void playerSettingsFromJson(const json& j, PlayerSettings& ps) {
    ps.move_speed        = j.value("move_speed", ps.move_speed);
    ps.fly_speed          = j.value("fly_speed", ps.fly_speed);
    ps.mouse_sensitivity = j.value("mouse_sensitivity", ps.mouse_sensitivity);
    ps.sprint_multiplier = j.value("sprint_multiplier", ps.sprint_multiplier);
    ps.jump_force         = j.value("jump_force", ps.jump_force);
}

static json itemToJson(const GameItem& it) {
    json j;
    j["id"]          = it.id;
    j["name"]        = it.name;
    j["type"]        = ITEM_TYPE_NAMES[(int)it.type];
    j["description"] = it.description;
    j["tags"]        = it.tags;
    j["weaponIdx"]   = it.weaponIdx;
    j["entityId"]    = it.entityId;
    j["meshPath"]    = it.meshPath;
    j["meshScale"]   = it.meshScale;
    j["health"]       = it.health;
    j["speed"]        = it.speed;
    j["jumpForce"]    = it.jumpForce;
    j["gravity"]      = it.gravity;
    j["damage"]       = it.damage;
    j["range"]        = it.range;
    j["fireRate"]     = it.fireRate;
    j["ammo"]         = it.ammo;
    j["reloadTime"]   = it.reloadTime;
    j["detectRadius"] = it.detectRadius;
    j["boss"]         = it.boss;
    j["patrolling"]   = it.patrolling;
    j["autoFire"]     = it.autoFire;
    j["stackSize"]    = it.stackSize;
    j["weight"]       = it.weight;
    j["topSpeed"]     = it.topSpeed;
    j["turnRadius"]   = it.turnRadius;
    j["fov"]          = it.fov;
    j["destructible"] = it.destructible;
    j["hp"]           = it.hp;
    j["active"]       = it.active;
    j["levelSettings"]  = worldSettingsToJson(it.levelSettings);
    j["playerSettings"] = playerSettingsToJson(it.playerSettings);
    return j;
}

static GameItem itemFromJson(const json& j) {
    GameItem it;
    it.id          = j.value("id", std::string());
    it.name        = j.value("name", std::string("Untitled"));
    it.type        = (ItemType)nameIndex(ITEM_TYPE_NAMES, ITEM_TYPE_COUNT,
                                          j.value("type", std::string("Player")));
    it.description = j.value("description", std::string());
    if (j.contains("tags")) it.tags = j["tags"].get<std::vector<std::string>>();
    it.weaponIdx   = j.value("weaponIdx", -1);
    it.entityId    = j.value("entityId", std::string());
    it.meshPath    = j.value("meshPath", std::string());
    it.meshScale   = j.value("meshScale", 1.f);
    it.health       = j.value("health", it.health);
    it.speed        = j.value("speed", it.speed);
    it.jumpForce    = j.value("jumpForce", it.jumpForce);
    it.gravity      = j.value("gravity", it.gravity);
    it.damage       = j.value("damage", it.damage);
    it.range        = j.value("range", it.range);
    it.fireRate     = j.value("fireRate", it.fireRate);
    it.ammo         = j.value("ammo", it.ammo);
    it.reloadTime   = j.value("reloadTime", it.reloadTime);
    it.detectRadius = j.value("detectRadius", it.detectRadius);
    it.boss         = j.value("boss", it.boss);
    it.patrolling   = j.value("patrolling", it.patrolling);
    it.autoFire     = j.value("autoFire", it.autoFire);
    it.stackSize    = j.value("stackSize", it.stackSize);
    it.weight       = j.value("weight", it.weight);
    it.topSpeed     = j.value("topSpeed", it.topSpeed);
    it.turnRadius   = j.value("turnRadius", it.turnRadius);
    it.fov          = j.value("fov", it.fov);
    it.destructible = j.value("destructible", it.destructible);
    it.hp           = j.value("hp", it.hp);
    it.active       = j.value("active", it.active);
    if (j.contains("levelSettings"))  worldSettingsFromJson(j["levelSettings"], it.levelSettings);
    if (j.contains("playerSettings")) playerSettingsFromJson(j["playerSettings"], it.playerSettings);
    return it;
}

static json actionToJson(const GameAction& a) {
    json triggers = json::array();
    for (auto& t : a.triggers) {
        triggers.push_back({
            {"type",       TRIGGER_TYPE_NAMES[(int)t.type]},
            {"itemRef",    t.itemRef},
            {"threshold",  t.threshold},
            {"comparison", t.comparison},
            {"zoneX", t.zoneX}, {"zoneY", t.zoneY}, {"zoneZ", t.zoneZ},
            {"zoneRadius", t.zoneRadius},
            {"anyEntity", t.anyEntity},
        });
    }
    json effects = json::array();
    for (auto& e : a.effects) {
        effects.push_back({
            {"type",      EFFECT_TYPE_NAMES[(int)e.type]},
            {"delay",     e.delay},
            {"targetRef", e.targetRef},
            {"varName",   std::string(e.varName)},
            {"varOp",     e.varOp},
            {"value",     e.value},
            {"stateStr",  std::string(e.stateStr)},
            {"posX", e.posX}, {"posY", e.posY}, {"posZ", e.posZ},
        });
    }
    return {
        {"id", a.id}, {"name", a.name},
        {"subtype", ACTION_SUBTYPE_NAMES[(int)a.subtype]},
        {"fireOnce", a.fireOnce},
        {"holdMode", a.holdMode},
        {"triggerCountRequired", a.triggerCountRequired},
        {"description", a.description},
        {"levelScope", a.levelScope},
        {"tags", a.tags},
        {"triggers", triggers},
        {"effects", effects},
    };
}

static GameAction actionFromJson(const json& j) {
    GameAction a;
    a.id      = j.value("id", std::string());
    a.name    = j.value("name", std::string("NewAction"));
    a.subtype = (ActionSubtype)nameIndex(ACTION_SUBTYPE_NAMES, ACTION_SUBTYPE_COUNT,
                                          j.value("subtype", std::string("Door")));
    a.fireOnce = j.value("fireOnce", true);
    a.holdMode = j.value("holdMode", false);
    a.triggerCountRequired = j.value("triggerCountRequired", 1);
    a.description = j.value("description", std::string());
    a.levelScope  = j.value("levelScope", std::string("global"));
    if (j.contains("tags")) a.tags = j["tags"].get<std::vector<std::string>>();
    if (j.contains("triggers")) {
        for (auto& tj : j["triggers"]) {
            ActionTrigger t;
            t.type       = (TriggerType)nameIndex(TRIGGER_TYPE_NAMES, TRIGGER_TYPE_COUNT,
                                                   tj.value("type", std::string("Item Interaction")));
            t.itemRef    = tj.value("itemRef", std::string());
            t.threshold  = tj.value("threshold", 0.f);
            t.comparison = tj.value("comparison", 0);
            t.zoneX      = tj.value("zoneX", 0.f);
            t.zoneY      = tj.value("zoneY", 0.f);
            t.zoneZ      = tj.value("zoneZ", 0.f);
            t.zoneRadius = tj.value("zoneRadius", 5.f);
            t.anyEntity  = tj.value("anyEntity", false);
            a.triggers.push_back(t);
        }
    }
    if (j.contains("effects")) {
        for (auto& ej : j["effects"]) {
            ActionEffect e;
            e.type  = (EffectType)nameIndex(EFFECT_TYPE_NAMES, EFFECT_TYPE_COUNT,
                                             ej.value("type", std::string("Change State")));
            e.delay = ej.value("delay", 0.f);
            e.targetRef = ej.value("targetRef", std::string());
            std::string vn = ej.value("varName", std::string("variable"));
            snprintf(e.varName, sizeof(e.varName), "%s", vn.c_str());
            e.varOp = ej.value("varOp", 0);
            e.value = ej.value("value", 0.f);
            std::string ss = ej.value("stateStr", std::string("active"));
            snprintf(e.stateStr, sizeof(e.stateStr), "%s", ss.c_str());
            e.posX = ej.value("posX", 0.f);
            e.posY = ej.value("posY", 0.f);
            e.posZ = ej.value("posZ", 0.f);
            a.effects.push_back(e);
        }
    }
    return a;
}

bool ScenePanel::saveProject(const std::string& path) const {
    json j;
    j["items"]    = json::array();
    for (auto& it : items)   j["items"].push_back(itemToJson(it));
    j["actions"]  = json::array();
    for (auto& a : actions)  j["actions"].push_back(actionToJson(a));
    j["globalVars"] = json::array();
    for (auto& v : globalVars)
        j["globalVars"].push_back({{"name", std::string(v.name)}, {"value", v.value}});
    j["idCtr"]  = idCtr;
    j["npcCtr"] = npcCtr;

    std::ofstream f(path);
    if (!f) return false;
    f << j.dump(2);
    return true;
}

bool ScenePanel::loadProject(const std::string& path) {
    std::ifstream f(path);
    if (!f) return false;
    json j;
    try { f >> j; } catch (...) { return false; }

    items.clear();
    actions.clear();
    globalVars.clear();

    if (j.contains("items"))
        for (auto& ij : j["items"]) items.push_back(itemFromJson(ij));
    if (j.contains("actions"))
        for (auto& aj : j["actions"]) actions.push_back(actionFromJson(aj));
    if (j.contains("globalVars")) {
        for (auto& vj : j["globalVars"]) {
            GlobalVar v;
            std::string n = vj.value("name", std::string("variable"));
            snprintf(v.name, sizeof(v.name), "%s", n.c_str());
            v.value = vj.value("value", 0.f);
            globalVars.push_back(v);
        }
    }
    idCtr  = j.value("idCtr", idCtr);
    npcCtr = j.value("npcCtr", npcCtr);

    if (!items.empty() && findItem(selectedItemId) == nullptr)
        selectedItemId = items[0].id;
    return true;
}

// ── Play-mode action executor ─────────────────────────────────────────────
// Evaluates triggers and applies effects while the game is in play mode.
// Semantics per game_engine_spec.md: N-of-M trigger slider, fire once vs
// continuously, per-effect delays. PlaySound is a status-message stub until
// an audio backend exists.

static bool compareOp(float v, int op, float threshold) {
    switch (op) {
        case 0: return v >= threshold;
        case 1: return v >  threshold;
        case 2: return v <= threshold;
        case 3: return v <  threshold;
        case 4: return v == threshold;
        case 5: return v != threshold;
    }
    return false;
}

// Words that mean "inactive" for a ChangeState effect; anything else = active.
static bool stateWordOff(const char* s) {
    // "off" = the item's barrier/collision is absent. For door-glue an
    // open/raised door is off (passable); closed/locked keep it on (solid).
    static const char* offWords[] = {"inactive","off","false","0","dead",
                                     "hidden","open","raised","gone","disabled"};
    for (const char* w : offWords)
        if (strcasecmp(s, w) == 0) return true;
    return false;
}

GlobalVar* ScenePanel::findVar(const char* name, bool createIfMissing) {
    for (auto& v : globalVars)
        if (strcmp(v.name, name) == 0) return &v;
    if (!createIfMissing) return nullptr;
    GlobalVar v;
    snprintf(v.name, sizeof(v.name), "%s", name);
    globalVars.push_back(v);
    return &globalVars.back();
}

void ScenePanel::resetActionRuntime() {
    playClock = 0.f;
    pendingFx.clear();
    pendingLevelSwitch.clear();
    for (auto& a : actions) { a.fired = false; a.wasSat = false; }
}

bool ScenePanel::evalTrigger(const ActionTrigger& t, const PlayCtx& ctx) {
    switch (t.type) {
        case TriggerType::ItemInteraction: {
            // Player pressed E within reach of the item's entity — or, when
            // the item has no entity, within the trigger's own zone (lets
            // consoles/levers be pure data + a decor model).
            if (!ctx.interactPressed) return false;
            GameItem* item = findItem(t.itemRef);
            if (!item) return false;
            if (!item->entityId.empty() && ctx.entities) {
                CharacterEntity* e = ctx.entities->find(item->entityId);
                if (!e) return false;
                return glm::length(ctx.playerPos - (e->position + glm::vec3(0,1.f,0))) < 3.f;
            }
            glm::vec3 zone = {t.zoneX, t.zoneY, t.zoneZ};
            float r = t.zoneRadius > 0.f ? t.zoneRadius : 2.f;
            return glm::length(ctx.playerPos - zone) <= r;
        }
        case TriggerType::VariableThreshold: {
            // itemRef holds the global variable name.
            GlobalVar* v = findVar(t.itemRef.c_str(), false);
            return v && compareOp(v->value, t.comparison, t.threshold);
        }
        case TriggerType::ItemDestroyed: {
            GameItem* item = findItem(t.itemRef);
            if (!item) return false;
            if (!item->entityId.empty() && ctx.entities) {
                CharacterEntity* e = ctx.entities->find(item->entityId);
                return !e || e->dead;
            }
            return item->hp <= 0.f;
        }
        case TriggerType::TimerElapsed:
            return playClock >= t.threshold;
        case TriggerType::GameStateChange: {
            // Referenced item's active flag (1/0) compared vs threshold.
            GameItem* item = findItem(t.itemRef);
            if (!item) return false;
            return compareOp(item->active ? 1.f : 0.f, t.comparison, t.threshold);
        }
        case TriggerType::LocationEntered: {
            glm::vec3 zone = {t.zoneX, t.zoneY, t.zoneZ};
            if (glm::length(ctx.playerPos - zone) <= t.zoneRadius) return true;
            if (t.anyEntity && ctx.entities) {
                for (auto& e : ctx.entities->all()) {
                    if (e.isPlayer || e.dead) continue;
                    if (glm::length(e.position + glm::vec3(0, 1.f, 0) - zone)
                        <= t.zoneRadius) return true;
                }
            }
            return false;
        }
        case TriggerType::PulseTimer: {
            // threshold = period, zoneX = phase offset, zoneY = on-duration
            float period = std::max(t.threshold, 0.2f);
            float duty   = t.zoneY > 0.f ? t.zoneY : period * 0.5f;
            return std::fmod(playClock + t.zoneX, period) < duty;
        }
    }
    return false;
}

void ScenePanel::applyEffect(const ActionEffect& e, PlayCtx& ctx) {
    GameItem* item = findItem(e.targetRef);
    switch (e.type) {
        case EffectType::ChangeState:
            if (item) item->active = !stateWordOff(e.stateStr);
            break;
        case EffectType::SetVariable: {
            GlobalVar* v = findVar(e.varName, true);
            switch (e.varOp) {
                case 0: v->value  = e.value; break;
                case 1: v->value += e.value; break;
                case 2: v->value -= e.value; break;
                case 3: v->value *= e.value; break;
            }
            break;
        }
        case EffectType::SpawnItem: {
            if (!ctx.entities) break;
            std::string eid = nextId();
            CharacterEntity* ent = ctx.entities->spawn(
                eid, item ? item->name : "Spawned", {e.posX, e.posY, e.posZ});
            if (ent && item) {
                ent->health = ent->maxHealth = item->health;
                if (item->entityId.empty()) item->entityId = eid;
            }
            break;
        }
        case EffectType::DespawnItem:
            if (item) {
                if (!item->entityId.empty() && ctx.entities)
                    ctx.entities->remove(item->entityId);
                item->active = false;
            }
            break;
        case EffectType::Teleport: {
            glm::vec3 pos = {e.posX, e.posY, e.posZ};
            // No target (or the Player item) teleports the player.
            if (!item || item->type == ItemType::Player) {
                ctx.teleported = true;
                ctx.teleportTo = pos;
            } else if (!item->entityId.empty() && ctx.entities) {
                if (CharacterEntity* ent = ctx.entities->find(item->entityId))
                    ent->position = pos;
            }
            break;
        }
        case EffectType::ModifyHealth: {
            if ((!item || item->type == ItemType::Player) && ctx.playerHealth) {
                *ctx.playerHealth = std::min(*ctx.playerHealth + e.value,
                                             ctx.playerMaxHealth);
            } else if (item && !item->entityId.empty() && ctx.entities) {
                if (CharacterEntity* ent = ctx.entities->find(item->entityId)) {
                    ent->health = std::min(ent->health + e.value, ent->maxHealth);
                    if (ent->health <= 0.f) {
                        ent->dead = true;
                        ctx.entities->remove(ent->id);
                        item->active = false;
                    }
                }
            }
            break;
        }
        case EffectType::GiveItem:
            if (item) {
                item->active = true;
                ctx.statusMsg = "Received: " + item->name;
            }
            break;
        case EffectType::PlaySound:
            // varName = file name in assets/sounds (without extension)
            Audio::get().play(e.varName, 1.f);
            break;
        case EffectType::SwitchLevel:
            // Deferred: switching clears items/actions, so it can't run
            // while updateActions is iterating them.
            pendingLevelSwitch = e.targetRef;
            break;
        case EffectType::SequenceStep: {
            // Ordered console puzzles: value = this step's index (1-based).
            // Correct next step advances the sequence variable; any wrong
            // step resets it to 0 with a buzz.
            GlobalVar* v = findVar(e.varName, true);
            if ((int)v->value == (int)e.value - 1) {
                v->value = e.value;
                Audio::get().play("kill", 0.45f, 1.2f);
                ctx.statusMsg = std::string(e.varName) + " " +
                                std::to_string((int)e.value);
            } else if ((int)v->value < (int)e.value) {
                v->value = 0.f;
                Audio::get().play("switch", 0.55f, 0.45f);
                ctx.statusMsg = std::string(e.varName) + " reset";
            }
            break;
        }
    }
}

void ScenePanel::updateActions(float dt, PlayCtx& ctx) {
    playClock += dt;

    // Delayed effects whose timers expired this frame
    for (int i = (int)pendingFx.size() - 1; i >= 0; i--) {
        pendingFx[i].timeLeft -= dt;
        if (pendingFx[i].timeLeft <= 0.f) {
            ActionEffect fx = pendingFx[i].fx;
            pendingFx.erase(pendingFx.begin() + i);
            applyEffect(fx, ctx);
        }
    }

    for (auto& a : actions) {
        if (a.triggers.empty()) continue;      // orphan

        int satisfied = 0;
        for (auto& t : a.triggers)
            if (evalTrigger(t, ctx)) satisfied++;
        int  need = std::max(1, std::min(a.triggerCountRequired, (int)a.triggers.size()));
        bool sat  = satisfied >= need;

        if (a.holdMode) {
            // Pressure-plate semantics: effects on the rising edge,
            // ChangeState effects invert on the falling edge.
            if (sat && !a.wasSat) {
                for (auto& e : a.effects) {
                    if (e.delay > 0.f) pendingFx.push_back({e, e.delay});
                    else               applyEffect(e, ctx);
                }
            } else if (!sat && a.wasSat) {
                for (auto& e : a.effects) {
                    if (e.type != EffectType::ChangeState) continue;
                    if (GameItem* item = findItem(e.targetRef))
                        item->active = stateWordOff(e.stateStr);   // inverted
                }
            }
            a.wasSat = sat;
            continue;
        }

        if (a.fireOnce && a.fired) continue;
        if (!sat) continue;

        a.fired = true;
        for (auto& e : a.effects) {
            if (e.delay > 0.f) pendingFx.push_back({e, e.delay});
            else               applyEffect(e, ctx);
        }
    }

    if (!pendingLevelSwitch.empty() && ctx.world && ctx.entities) {
        std::string id = pendingLevelSwitch;
        pendingLevelSwitch.clear();
        mapManager.switchLevel(id, *ctx.world, *ctx.entities, *this);
        resetActionRuntime();
        ctx.levelSwitched = true;
        ctx.statusMsg = "Switched to level '" + id + "'.";
    }
}

bool ScenePanel::getFirstLevelSettings(WorldSettings& out) const {
    for (auto& it : items)
        if (it.type == ItemType::Level) { out = it.levelSettings; return true; }
    return false;
}

// ── Helpers ───────────────────────────────────────────────────────────────

GameItem* ScenePanel::findItem(const std::string& id) {
    for (auto& it : items) if (it.id == id) return &it;
    return nullptr;
}
GameAction* ScenePanel::findAction(const std::string& id) {
    for (auto& a : actions) if (a.id == id) return &a;
    return nullptr;
}
bool ScenePanel::isOrphan(const GameAction& a) const {
    return a.triggers.empty();
}

ScenePanel::VehicleItemInfo ScenePanel::getFirstVehicleInfo() const {
    for (auto& item : items)
        if (item.type == ItemType::Vehicle)
            return { item.meshPath, item.topSpeed,
                     item.turnRadius > 0.f ? (360.f / item.turnRadius) : 80.f,
                     item.meshScale };
    return {};
}

std::vector<glm::vec3> ScenePanel::getSpawnMarkers() const {
    std::vector<glm::vec3> out;
    for (auto& act : actions)
        for (auto& eff : act.effects)
            if (eff.type == EffectType::SpawnItem)
                out.push_back({eff.posX, eff.posY, eff.posZ});
    return out;
}
bool ScenePanel::hasBrokenRef(const GameAction& a) const {
    for (auto& t : a.triggers) {
        if (!t.itemRef.empty()) {
            bool found = false;
            for (auto& it : items) if (it.id == t.itemRef) { found=true; break; }
            if (!found) return true;
        }
    }
    return false;
}

void ScenePanel::goBack() {
    switch (view) {
        case PanelView::ItemInspector:  view = PanelView::ItemList;        break;
        case PanelView::ItemList:       view = PanelView::ItemTypeFolders; break;
        case PanelView::ItemTypeFolders:view = PanelView::Home;            break;
        case PanelView::ActionInspector:view = PanelView::ActionList;      break;
        case PanelView::ActionList:     view = PanelView::Home;            break;
        case PanelView::VarList:        view = PanelView::Home;            break;
        default: break;
    }
}

// ── Breadcrumb ────────────────────────────────────────────────────────────

void ScenePanel::renderBreadcrumb() {
    if (view == PanelView::Home) {
        ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
        ImGui::SetWindowFontScale(1.05f);
        ImGui::Text("  VOXEL ENGINE");
        ImGui::SetWindowFontScale(1.f);
        ImGui::PopStyleColor();
        ImGui::PushStyleColor(ImGuiCol_Separator, col(203,166,247,80));
        ImGui::Separator();
        ImGui::PopStyleColor();
        ImGui::Spacing();
        return;
    }

    // Back button + path
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    if (ImGui::SmallButton("< Back")) goBack();
    ImGui::SameLine();

    // Breadcrumb path text
    std::string path;
    switch (view) {
        case PanelView::ItemTypeFolders: path = "Items"; break;
        case PanelView::ItemList:
            path = std::string("Items / ") + ITEM_TYPE_PLURAL[(int)typeFolder]; break;
        case PanelView::ItemInspector: {
            auto* it = findItem(selectedItemId);
            path = std::string("Items / ") + ITEM_TYPE_PLURAL[(int)typeFolder]
                 + " / " + (it ? it->name : "?");
            break;
        }
        case PanelView::ActionList:     path = "Actions"; break;
        case PanelView::ActionInspector: {
            auto* a = findAction(selectedActionId);
            path = std::string("Actions / ") + (a ? a->name : "?");
            break;
        }
        case PanelView::VarList: path = "Global Variables"; break;
        default: break;
    }
    ImGui::Text("%s", path.c_str());
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Separator, col(69,71,90,120));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();
}

// ── Home ──────────────────────────────────────────────────────────────────

void ScenePanel::renderHome() {
    auto bigBtn = [&](const char* label, ImVec4 tc, PanelView target) {
        ImGui::PushStyleColor(ImGuiCol_Text,          tc);
        ImGui::PushStyleColor(ImGuiCol_Button,        col(36,39,58));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col(54,58,79));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  col(69,71,90));
        if (ImGui::Button(label, {-1, 40})) view = target;
        ImGui::PopStyleColor(4);
        ImGui::Spacing();
    };

    bigBtn("  ITEMS",            itemCol(), PanelView::ItemTypeFolders);
    bigBtn("  ACTIONS",          actionCol(), PanelView::ActionList);
    bigBtn("  GLOBAL VARIABLES", varCol(),  PanelView::VarList);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    int nItems   = (int)items.size();
    int nActions = (int)actions.size();
    int orphans  = 0;
    for (auto& a : actions) if (isOrphan(a)) orphans++;
    ImGui::Text("  %d items  |  %d actions", nItems, nActions);
    if (orphans > 0) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, warnCol());
        ImGui::Text("  (%d orphan actions)", orphans);
        ImGui::PopStyleColor();
    }
    ImGui::PopStyleColor();
}

// ── Item Type Folders ─────────────────────────────────────────────────────

void ScenePanel::renderItemTypeFolders() {
    if (creating) {
        ImGui::PushStyleColor(ImGuiCol_Text, itemCol());
        ImGui::Text("New Item");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("Type##ct", &createTypeIdx, ITEM_TYPE_NAMES, ITEM_TYPE_COUNT);
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Name##cn", createName, sizeof(createName));
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        if (ImGui::Button("Create##ci", {-1, 0})) {
            GameItem it;
            it.id   = nextId();
            it.name = (createName[0] != '\0') ? createName
                                               : ITEM_TYPE_NAMES[createTypeIdx];
            it.type = (ItemType)createTypeIdx;
            items.push_back(it);
            selectedItemId = it.id;
            typeFolder     = it.type;
            view           = PanelView::ItemInspector;
            creating       = false;
            createName[0]  = '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##ci")) creating = false;
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    // Count items per type
    int counts[ITEM_TYPE_COUNT] = {};
    for (auto& it : items) counts[(int)it.type]++;

    for (int t = 0; t < ITEM_TYPE_COUNT; t++) {
        const ImVec4& tc = ITEM_TYPE_COLORS[t];
        char badge[24]; snprintf(badge, sizeof(badge), "[%s]", ITEM_TYPE_NAMES[t]);
        char label[64]; snprintf(label, sizeof(label), "%s  (%d)",
                                  ITEM_TYPE_PLURAL[t], counts[t]);
        if (cardRow(t, false, tc, badge, label)) {
            typeFolder = (ItemType)t;
            view = PanelView::ItemList;
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text(" >");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, itemCol());
    if (ImGui::Button("+ New Item", {-1, 0})) { creating = true; createTypeIdx = 0; }
    ImGui::PopStyleColor();
}

// ── Item List ─────────────────────────────────────────────────────────────

void ScenePanel::renderItemList(std::vector<Weapon*>& weapons, EntityManager& entities) {
    ImVec4 tc = ITEM_TYPE_COLORS[(int)typeFolder];

    if (creating) {
        ImGui::PushStyleColor(ImGuiCol_Text, tc);
        ImGui::Text("New %s", ITEM_TYPE_NAMES[(int)typeFolder]);
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Name##iln", createName, sizeof(createName));
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        if (ImGui::Button("Create##ilc", {-1, 0})) {
            GameItem it;
            it.id   = nextId();
            it.name = (createName[0] != '\0') ? createName
                                               : ITEM_TYPE_NAMES[(int)typeFolder];
            it.type = typeFolder;
            items.push_back(it);
            selectedItemId = it.id;
            view = PanelView::ItemInspector;
            creating      = false;
            createName[0] = '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##ilx")) creating = false;
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();
    }

    int idx = 0;
    bool erased = false;
    for (int i = 0; i < (int)items.size() && !erased; i++) {
        if (items[i].type != typeFolder) continue;
        char badge[24]; snprintf(badge, sizeof(badge), "[%s]", ITEM_TYPE_NAMES[(int)typeFolder]);
        bool sel = (items[i].id == selectedItemId);
        if (cardRow(idx++, sel, tc, badge, items[i].name.c_str())) {
            selectedItemId = items[i].id;
            view = PanelView::ItemInspector;
        }
        ImGui::SameLine();
        char xid[24]; snprintf(xid, sizeof(xid), "x##il%d", i);
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        if (ImGui::SmallButton(xid)) {
            if (items[i].type == ItemType::NPC || items[i].type == ItemType::Enemy)
                if (!items[i].entityId.empty()) entities.remove(items[i].entityId);
            if (selectedItemId == items[i].id) selectedItemId.clear();
            items.erase(items.begin() + i);
            erased = true;
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (idx == 0 && !creating) {
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::TextWrapped("  No %s yet. Create one below.", ITEM_TYPE_PLURAL[(int)typeFolder]);
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, tc);
    char lbl[64]; snprintf(lbl, sizeof(lbl), "+ New %s", ITEM_TYPE_NAMES[(int)typeFolder]);
    if (ImGui::Button(lbl, {-1, 0})) { creating = true; createName[0] = '\0'; }
    ImGui::PopStyleColor();
}

// ── Item Inspector ────────────────────────────────────────────────────────

bool ScenePanel::renderItemInspector(WorldSettings& ws, PlayerSettings& ps,
                                      bool& flyMode, std::vector<Weapon*>& weapons,
                                      int& activeSlot, Character& c, CharacterRenderer& cr,
                                      EntityManager& entities, std::string& statusMsg,
                                      World& world, const std::string& savePath) {
    GameItem* item = findItem(selectedItemId);
    if (!item) { view = PanelView::ItemList; return false; }

    bool changed = false;

    // Name edit
    ImVec4 tc = ITEM_TYPE_COLORS[(int)item->type];
    ImGui::PushStyleColor(ImGuiCol_Text, tc);
    ImGui::SetWindowFontScale(1.02f);
    char nameBuf[64]; strncpy(nameBuf, item->name.c_str(), 63); nameBuf[63]='\0';
    if (ImGui::InputText("##iname", nameBuf, sizeof(nameBuf)))
        item->name = nameBuf;
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleColor();

    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::Text("  type: %s", ITEM_TYPE_NAMES[(int)item->type]);
    ImGui::PopStyleColor();

    // Description
    ImGui::SetNextItemWidth(-1);
    char descBuf[256]; strncpy(descBuf, item->description.c_str(), 255); descBuf[255]='\0';
    if (ImGui::InputText("##idesc", descBuf, sizeof(descBuf),
                          ImGuiInputTextFlags_AutoSelectAll))
        item->description = descBuf;

    // Tags
    renderTagEditor(item->tags, item->id.c_str());

    // Related actions
    bool hasRelated = false;
    for (auto& a : actions)
        for (auto& t : a.triggers)
            if (t.itemRef == item->id) hasRelated = true;
    if (hasRelated) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("  Used by actions:");
        for (auto& a : actions)
            for (auto& t : a.triggers)
                if (t.itemRef == item->id) {
                    ImGui::Text("    • %s", a.name.c_str());
                }
        ImGui::PopStyleColor();
    }

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, col(69,71,90,120));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // Type-specific inspector
    switch (item->type) {
        case ItemType::Level:
            if (renderLevelInspector(*item, statusMsg, world, savePath, entities))
                ws = item->levelSettings;
            break;
        case ItemType::Player:
            if (renderPlayerInspector(*item, ps, flyMode, c, cr, entities))
                changed = true;
            break;
        case ItemType::Weapon:
            if (renderWeaponInspector(*item, weapons, activeSlot)) changed = true;
            break;
        case ItemType::Enemy:
            renderEnemyInspector(*item, entities, c);
            break;
        case ItemType::NPC:
            renderNPCInspector(*item, entities, c);
            break;
        default:
            renderGenericInspector(*item);
            break;
    }
    return changed;
}

// ── Level inspector ───────────────────────────────────────────────────────

bool ScenePanel::renderLevelInspector(GameItem& item, std::string& statusMsg,
                                       World& world, const std::string& savePath,
                                       EntityManager& entities) {
    WorldSettings& ls = item.levelSettings;

    ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
    std::string projectPath = PlayerProfile::get().saveDir() + "/project.json";
    if (ImGui::Button("Save  [F5]", {120,0})) {
        bool worldOk   = world.save(savePath);
        bool projectOk = saveProject(projectPath);
        statusMsg = (worldOk && projectOk) ? "World saved." : "Save failed!";
    }
    ImGui::SameLine();
    if (ImGui::Button("Load  [F6]", {120,0})) {
        bool worldOk   = world.load(savePath);
        bool projectOk = loadProject(projectPath);
        statusMsg = (worldOk && projectOk) ? "World loaded." : "Load failed!";
    }
    ImGui::PopStyleColor();
    if (!statusMsg.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
        ImGui::TextWrapped("  %s", statusMsg.c_str());
        ImGui::PopStyleColor();
    }
    ImGui::Spacing();

    if (ImGui::CollapsingHeader("  Terrain", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();

        // ── Terrain type ──────────────────────────────────────────────────
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("Type:");
        ImGui::PopStyleColor();

        // Voxel
        bool isVoxel  = ls.voxelWorld;
        bool isFlat   = !ls.voxelWorld && ls.flatTerrain && item.meshPath.empty();
        bool isNoise  = !ls.voxelWorld && !ls.flatTerrain && item.meshPath.empty();
        bool isMesh   = !ls.voxelWorld && !item.meshPath.empty();

        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        if (ImGui::RadioButton("Voxel", isVoxel)) {
            ls.voxelWorld  = true;
            ls.flatTerrain = false;
            item.meshPath.clear();
            ls.customMeshPath[0] = '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
        if (ImGui::RadioButton("Noise mesh", isNoise)) {
            ls.voxelWorld  = false;
            ls.flatTerrain = false;
            item.meshPath.clear();
            ls.customMeshPath[0] = '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(249,226,175));
        if (ImGui::RadioButton("Flat", isFlat)) {
            ls.voxelWorld  = false;
            ls.flatTerrain = true;
            item.meshPath.clear();
            ls.customMeshPath[0] = '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
        if (ImGui::RadioButton("Custom mesh", isMesh)) {
            ls.voxelWorld  = false;
            ls.flatTerrain = false;
            // meshPath stays as-is if already set
        }
        ImGui::PopStyleColor();

        // Custom mesh file picker
        if (!ls.voxelWorld) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
            ImGui::Text("Map Mesh (.glb):");
            ImGui::PopStyleColor();

            if (item.meshPath.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
                ImGui::TextWrapped("  No mesh loaded — using %s.",
                    ls.flatTerrain ? "flat floor" : "procedural noise");
                ImGui::PopStyleColor();
            } else {
                // Show filename only (path can be long)
                std::string fname = item.meshPath;
                auto slash = fname.rfind('/');
                if (slash != std::string::npos) fname = fname.substr(slash+1);
                ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
                ImGui::TextWrapped("  %s", fname.c_str());
                ImGui::PopStyleColor();
            }

            ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
            if (ImGui::Button("Browse...", {-1, 0})) {
                std::string path = FileDialog::open("Choose Map Mesh", {"glb"});
                if (!path.empty()) {
                    item.meshPath = path;
                    ls.voxelWorld  = false;
                    ls.flatTerrain = false;
                    strncpy(ls.customMeshPath, path.c_str(), sizeof(ls.customMeshPath)-1);
                }
            }
            ImGui::PopStyleColor();
            if (!item.meshPath.empty()) {
                ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
                if (ImGui::SmallButton("Clear mesh")) {
                    item.meshPath.clear();
                    ls.customMeshPath[0] = '\0';
                }
                ImGui::PopStyleColor();
            }
        }

        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        if (ls.voxelWorld)
            ImGui::SliderInt("Render Dist", &ls.render_dist, 1, 24);
        ImGui::SliderFloat("Mist / Fog", &ls.fog_density, 0.f, 0.025f, "%.4f");
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }

    if (ImGui::CollapsingHeader("  Environment", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        ImGui::SliderFloat("Gravity",   &ls.gravity,    0.f,  40.f, "%.1f");
        ImGui::SliderFloat("Wind X",    &ls.wind_x,   -15.f,  15.f, "%.1f");
        ImGui::SliderFloat("Wind Z",    &ls.wind_z,   -15.f,  15.f, "%.1f");
        ImGui::SliderFloat("Sun Angle", &ls.sun_angle,  0.f, 360.f, "%.0f deg");
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }

    // ── Levels (MapManager) ──────────────────────────────────────────────
    // Placed last: switching a level clears and repopulates `items`, so
    // `item`/`ls` above must not be touched again after this fires.
    if (ImGui::CollapsingHeader("  Levels", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("Current: %s", mapManager.currentId().c_str());
        ImGui::PopStyleColor();

        static char levelIdBuf[64] = "";
        ImGui::SetNextItemWidth(-90);
        ImGui::InputTextWithHint("##levelId", "level id, e.g. level2", levelIdBuf, sizeof(levelIdBuf));
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        bool switchClicked = ImGui::Button("Switch", {80, 0});
        ImGui::PopStyleColor();
        if (switchClicked && levelIdBuf[0] != '\0') {
            std::string id = levelIdBuf;
            mapManager.switchLevel(id, world, entities, *this);
            statusMsg      = "Switched to level '" + id + "'.";
            levelIdBuf[0]  = '\0';
            ImGui::Unindent();
            return false; // `item`/`ls` are dangling past this point
        }

        auto levels = mapManager.listLevels();
        if (!levels.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
            ImGui::Text("Saved levels:");
            ImGui::PopStyleColor();
            for (auto& lvl : levels)
                ImGui::BulletText("%s", lvl.displayName.c_str());
        }
        ImGui::Unindent();
    }
    return false;
}

// ── Player inspector ──────────────────────────────────────────────────────

bool ScenePanel::renderPlayerInspector(GameItem& item, PlayerSettings& ps,
                                        bool& flyMode, Character& c,
                                        CharacterRenderer& cr, EntityManager& entities) {
    bool changed = false;
    renderMeshPicker(item, item.id.c_str());
    // Sync GameItem stats → ps (item is the source of truth while selected)
    if (ImGui::CollapsingHeader("  Movement", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::Checkbox("Fly Mode [G]", &flyMode);
        ImGui::PushStyleColor(ImGuiCol_Text, col(137,180,250));
        ImGui::SliderFloat("Fly Speed",   &ps.fly_speed,         1.f, 80.f,  "%.1f");
        ImGui::SliderFloat("Move Speed",  &ps.move_speed,        1.f, 40.f,  "%.1f");
        ImGui::SliderFloat("Sprint Mult", &ps.sprint_multiplier, 1.f,  6.f,  "%.1f x");
        ImGui::SliderFloat("Sensitivity", &ps.mouse_sensitivity, 0.01f, 0.5f,"%.3f");
        ImGui::SliderFloat("Jump Force",  &ps.jump_force,        0.f, 30.f,  "%.1f");
        ImGui::SliderFloat("Gravity",     &item.gravity,         0.f, 40.f,  "%.1f");
        ImGui::SliderFloat("Health",      &item.health,          1.f, 500.f, "%.0f");
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }
    ImGui::Spacing();
    if (charPanel.render(c, cr, entities)) changed = true;
    return changed;
}

// ── Weapon inspector ──────────────────────────────────────────────────────

bool ScenePanel::renderWeaponInspector(GameItem& item, std::vector<Weapon*>& weapons,
                                        int& activeSlot) {
    if (weapons.empty()) { ImGui::TextDisabled("No weapons available."); return false; }
    if (item.weaponIdx < 0 || item.weaponIdx >= (int)weapons.size()) {
        // Not yet mapped — let user pick
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("Map to runtime weapon:");
        ImGui::PopStyleColor();
        std::vector<const char*> wnames;
        for (auto* w : weapons) wnames.push_back(w->name.c_str());
        int pick = 0;
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("##wpick", &pick, wnames.data(), (int)wnames.size());
        if (ImGui::Button("Assign", {-1, 0})) item.weaponIdx = pick;
        return false;
    }
    ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
    ImGui::Text("Weapon: %s", weapons[item.weaponIdx]->name.c_str());
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::SmallButton("Equip")) activeSlot = item.weaponIdx;
    ImGui::Spacing();
    weapons[item.weaponIdx]->renderUI();

    renderMeshPicker(item, item.id.c_str());

    // Stat overrides (for display / future runtime use)
    if (ImGui::CollapsingHeader("  Stats")) {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, col(203,166,247));
        ImGui::SliderFloat("Damage",      &item.damage,     0.f, 200.f, "%.0f");
        ImGui::SliderFloat("Range",       &item.range,      1.f, 200.f, "%.0f");
        ImGui::SliderFloat("Fire Rate",   &item.fireRate,   0.05f, 5.f, "%.2f /s");
        ImGui::SliderInt  ("Ammo",        &item.ammo,       1,    300);
        ImGui::SliderFloat("Reload Time", &item.reloadTime, 0.1f, 10.f,"%.1f s");
        ImGui::Checkbox   ("Auto Fire",   &item.autoFire);
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }
    return false;
}

// ── Enemy inspector ───────────────────────────────────────────────────────

void ScenePanel::renderEnemyInspector(GameItem& item, EntityManager& entities,
                                       Character& tmpl) {
    renderMeshPicker(item, item.id.c_str());
    if (ImGui::CollapsingHeader("  Stats", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Indent();
        ImGui::PushStyleColor(ImGuiCol_Text, col(243,139,168));
        ImGui::SliderFloat("Health",          &item.health,       1.f,  500.f, "%.0f");
        ImGui::SliderFloat("Speed",           &item.speed,        1.f,   40.f, "%.1f");
        ImGui::SliderFloat("Damage",          &item.damage,       0.f,  200.f, "%.0f");
        ImGui::SliderFloat("Detect Radius",   &item.detectRadius, 1.f,  100.f, "%.0f");
        ImGui::Checkbox   ("Patrolling",      &item.patrolling);
        ImGui::Checkbox   ("Boss",            &item.boss);
        ImGui::PopStyleColor();
        ImGui::Unindent();
    }
    ImGui::Spacing();

    // Spawn / despawn entity
    CharacterEntity* e = entities.find(item.entityId);
    ImGui::PushStyleColor(ImGuiCol_Text, col(243,139,168));
    ImGui::Text("Spawn");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputFloat3("Position##enemypos", spawnPos, "%.1f");
    if (!e) {
        if (ImGui::Button("Spawn Enemy", {-1, 0})) {
            std::string id = "enemy_" + item.id;
            item.entityId  = id;
            float sy = std::max(spawnPos[1], safeSpawnY(spawnPos[0], spawnPos[2]));
            auto* ne = entities.spawn(id, item.name, {spawnPos[0], sy, spawnPos[2]});
            if (ne) { ne->character = tmpl; entities.markDirty(id); }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
        ImGui::Text("  Live: %s", item.entityId.c_str());
        ImGui::PopStyleColor();
        float pos[3] = { e->position.x, e->position.y, e->position.z };
        if (ImGui::InputFloat3("Pos##elive", pos, "%.1f"))
            e->position = {pos[0], pos[1], pos[2]};
        if (ImGui::Button("Despawn", {-1, 0})) {
            entities.remove(item.entityId);
            item.entityId.clear();
        }
    }
}

// ── NPC inspector ─────────────────────────────────────────────────────────

void ScenePanel::renderNPCInspector(GameItem& item, EntityManager& entities,
                                     Character& tmpl) {
    renderMeshPicker(item, item.id.c_str());
    CharacterEntity* e = entities.find(item.entityId);
    ImGui::PushStyleColor(ImGuiCol_Text, col(250,200,135));
    ImGui::Text("Spawn");
    ImGui::PopStyleColor();
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("Name##npcn", spawnName, sizeof(spawnName));
    ImGui::SetNextItemWidth(-1);
    ImGui::InputFloat3("Position##npcpos", spawnPos, "%.1f");
    if (!e) {
        if (ImGui::Button("Spawn NPC", {-1, 0})) {
            std::string id = "npc_" + item.id;
            item.entityId  = id;
            float sy = std::max(spawnPos[1], safeSpawnY(spawnPos[0], spawnPos[2]));
            auto* ne = entities.spawn(id, spawnName, {spawnPos[0], sy, spawnPos[2]});
            if (ne) { ne->character = tmpl; ne->name = spawnName; entities.markDirty(id); }
        }
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
        ImGui::Text("  Live: %s", item.entityId.c_str());
        ImGui::PopStyleColor();
        float pos[3] = { e->position.x, e->position.y, e->position.z };
        if (ImGui::InputFloat3("Pos##nlive", pos, "%.1f"))
            e->position = {pos[0], pos[1], pos[2]};
        if (ImGui::Button("Despawn", {-1, 0})) {
            entities.remove(item.entityId);
            item.entityId.clear();
        }
    }
}

// ── Generic inspector (Pickup, Vehicle, Camera, etc.) ────────────────────

void ScenePanel::renderGenericInspector(GameItem& item) {
    renderMeshPicker(item, item.id.c_str());
    ImVec4 tc = ITEM_TYPE_COLORS[(int)item.type];
    ImGui::PushStyleColor(ImGuiCol_Text, tc);

    switch (item.type) {
        case ItemType::Pickup:
            if (ImGui::CollapsingHeader("  Pickup", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::SliderFloat("Stack Size", &item.stackSize, 1, 99, "%.0f");
                ImGui::SliderFloat("Weight",     &item.weight,    0, 20, "%.1f kg");
                ImGui::Checkbox   ("Instant Use (no inventory)", &item.autoFire);
                ImGui::Unindent();
            }
            break;
        case ItemType::Vehicle:
            if (ImGui::CollapsingHeader("  Vehicle", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::SliderFloat("Top Speed",   &item.topSpeed,   1, 100, "%.0f");
                ImGui::SliderFloat("Turn Radius", &item.turnRadius, 1,  30, "%.1f");
                ImGui::SliderInt  ("Seat Count",  &item.ammo,       1,   8);
                ImGui::Unindent();
            }
            break;
        case ItemType::Camera:
            if (ImGui::CollapsingHeader("  Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::SliderFloat("FOV",       &item.fov,   30, 150, "%.0f deg");
                ImGui::SliderFloat("Smoothing", &item.speed,  0,   1, "%.2f");
                static const char* perspectives[] = {
                    "First Person","Third Person","Fixed","Side-Scroll","Top-Down"
                };
                ImGui::Combo("Perspective##cam", &item.ammo, perspectives, 5);
                ImGui::Unindent();
            }
            break;
        case ItemType::Projectile:
            if (ImGui::CollapsingHeader("  Projectile", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::SliderFloat("Speed",       &item.speed,        1, 200, "%.0f");
                ImGui::SliderFloat("AoE Radius",  &item.range,        0,  20, "%.1f");
                ImGui::SliderFloat("Pierce Count",&item.stackSize,    0,  10, "%.0f");
                ImGui::Checkbox   ("Gravity Affected", &item.patrolling);
                ImGui::Checkbox   ("Homing",           &item.autoFire);
                ImGui::Unindent();
            }
            break;
        case ItemType::ConstructionPiece:
            if (ImGui::CollapsingHeader("  Construction", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::Checkbox   ("Snap to Grid",     &item.patrolling);
                ImGui::Checkbox   ("Collision",        &item.autoFire);
                ImGui::Checkbox   ("Destructible",     &item.destructible);
                if (item.destructible)
                    ImGui::SliderFloat("HP", &item.hp, 1, 500, "%.0f");
                ImGui::Unindent();
            }
            break;
        case ItemType::TriggerZone:
            if (ImGui::CollapsingHeader("  Trigger Zone", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::Indent();
                ImGui::SliderFloat("Width",  &item.range,  0.5f, 50, "%.1f");
                ImGui::SliderFloat("Height", &item.hp,     0.5f, 50, "%.1f");
                ImGui::SliderFloat("Depth",  &item.speed,  0.5f, 50, "%.1f");
                ImGui::Unindent();
            }
            break;
        default:
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
            ImGui::TextWrapped("  Inspector coming soon for this type.");
            ImGui::PopStyleColor();
            break;
    }
    ImGui::PopStyleColor();
}

// ── Tag editor ────────────────────────────────────────────────────────────

void ScenePanel::renderTagEditor(std::vector<std::string>& tags, const char* uid) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::Text("  Tags:");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    for (int i = 0; i < (int)tags.size(); i++) {
        ImGui::PushStyleColor(ImGuiCol_Button,        col(49,50,68));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col(243,139,168,180));
        ImGui::PushStyleColor(ImGuiCol_Text,          col(203,166,247));
        char tlbl[64]; snprintf(tlbl, sizeof(tlbl), "%s x##t%s%d", tags[i].c_str(), uid, i);
        if (ImGui::SmallButton(tlbl)) { tags.erase(tags.begin()+i); ImGui::PopStyleColor(3); break; }
        ImGui::PopStyleColor(3);
        ImGui::SameLine();
    }
    char addId[32]; snprintf(addId, sizeof(addId), "##tagadd%s", uid);
    ImGui::SetNextItemWidth(80);
    ImGui::InputText(addId, newTagBuf, sizeof(newTagBuf));
    ImGui::SameLine();
    char btnId[32]; snprintf(btnId, sizeof(btnId), "+##tagbtn%s", uid);
    if (ImGui::SmallButton(btnId) && newTagBuf[0] != '\0') {
        tags.push_back(newTagBuf);
        newTagBuf[0] = '\0';
    }
}

// ── Shared mesh picker ────────────────────────────────────────────────────

void ScenePanel::renderMeshPicker(GameItem& item, const char* uid) {
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::Text("  Mesh");
    ImGui::PopStyleColor();

    if (!item.meshPath.empty()) {
        // Show just the filename, not the full path
        std::string fname = item.meshPath;
        auto sl = fname.find_last_of("/\\");
        if (sl != std::string::npos) fname = fname.substr(sl + 1);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
        ImGui::Text("%s", fname.c_str());
        ImGui::PopStyleColor();
        ImGui::SameLine();
        char clrid[32]; snprintf(clrid, sizeof(clrid), "x##mclr%s", uid);
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        if (ImGui::SmallButton(clrid)) item.meshPath.clear();
        ImGui::PopStyleColor();
    } else {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("(none)");
        ImGui::PopStyleColor();
    }

    char brid[32]; snprintf(brid, sizeof(brid), "Browse##mbr%s", uid);
    if (ImGui::Button(brid, {-1, 0})) {
        std::string p = FileDialog::open("Choose Mesh", {"glb", "obj"});
        if (!p.empty()) item.meshPath = p;
    }

    // Scale — critical for models authored in different unit systems
    ImGui::SetNextItemWidth(-1);
    char scid[32]; snprintf(scid, sizeof(scid), "##mscale%s", uid);
    ImGui::SliderFloat(scid, &item.meshScale, 0.001f, 100.f, "Scale %.3f", ImGuiSliderFlags_Logarithmic);

    ImGui::Spacing();
}

// ── Action list ───────────────────────────────────────────────────────────

void ScenePanel::renderActionList() {
    // Search bar
    ImGui::SetNextItemWidth(-1);
    ImGui::InputText("##asearch", actionSearch, sizeof(actionSearch),
                      ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::SameLine();
    ImGui::Text("search");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    std::string query(actionSearch);
    std::transform(query.begin(), query.end(), query.begin(), ::tolower);

    int idx = 0;
    bool erased = false;
    for (int i = 0; i < (int)actions.size() && !erased; i++) {
        GameAction& a = actions[i];
        if (!query.empty()) {
            std::string lower = a.name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(query) == std::string::npos) continue;
        }
        bool orphan  = isOrphan(a);
        bool broken  = hasBrokenRef(a);
        ImVec4 tc    = orphan ? warnCol() : actionCol();
        char badge[32]; snprintf(badge, sizeof(badge), "[%s]",
                                  ACTION_SUBTYPE_NAMES[(int)a.subtype]);
        bool sel = (a.id == selectedActionId);
        if (cardRow(idx++, sel, tc, badge, a.name.c_str(), orphan, broken)) {
            selectedActionId = a.id;
            view = PanelView::ActionInspector;
        }
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        char xid[24]; snprintf(xid, sizeof(xid), "x##al%d", i);
        if (ImGui::SmallButton(xid)) {
            if (selectedActionId == a.id) selectedActionId.clear();
            actions.erase(actions.begin() + i);
            erased = true;
        }
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    if (actions.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::TextWrapped("  No actions yet. Create one below.");
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }

    ImGui::Separator();
    ImGui::Spacing();

    if (creatingAction) {
        ImGui::PushStyleColor(ImGuiCol_Text, actionCol());
        ImGui::Text("New Action");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1);
        ImGui::Combo("Subtype##as", &createSubtype, ACTION_SUBTYPE_NAMES, ACTION_SUBTYPE_COUNT);
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::TextWrapped("  Suggested: Level_Trigger_Effect");
        ImGui::PopStyleColor();
        ImGui::SetNextItemWidth(-1);
        ImGui::InputText("Name##aname", createActionName, sizeof(createActionName));
        ImGui::PushStyleColor(ImGuiCol_Text, col(166,218,149));
        if (ImGui::Button("Create##ac", {-1, 0})) {
            GameAction a;
            a.id      = nextId();
            a.name    = (createActionName[0] != '\0') ? createActionName : "NewAction";
            a.subtype = (ActionSubtype)createSubtype;
            actions.push_back(a);
            selectedActionId   = a.id;
            view               = PanelView::ActionInspector;
            creatingAction     = false;
            createActionName[0]= '\0';
        }
        ImGui::PopStyleColor();
        ImGui::SameLine();
        if (ImGui::Button("Cancel##acx")) creatingAction = false;
    } else {
        ImGui::PushStyleColor(ImGuiCol_Text, actionCol());
        if (ImGui::Button("+ New Action", {-1, 0})) {
            creatingAction  = true;
            createSubtype   = 0;
            createActionName[0] = '\0';
        }
        ImGui::PopStyleColor();
    }
}

// ── Action inspector ──────────────────────────────────────────────────────

void ScenePanel::renderActionInspector() {
    GameAction* a = findAction(selectedActionId);
    if (!a) { view = PanelView::ActionList; return; }

    // Name + subtype
    ImGui::PushStyleColor(ImGuiCol_Text, actionCol());
    ImGui::SetWindowFontScale(1.02f);
    char nameBuf[64]; strncpy(nameBuf, a->name.c_str(), 63); nameBuf[63]='\0';
    if (ImGui::InputText("##aname2", nameBuf, sizeof(nameBuf)))
        a->name = nameBuf;
    ImGui::SetWindowFontScale(1.f);
    ImGui::PopStyleColor();

    ImGui::SetNextItemWidth(-1);
    int sub = (int)a->subtype;
    if (ImGui::Combo("Subtype##ainsp", &sub, ACTION_SUBTYPE_NAMES, ACTION_SUBTYPE_COUNT))
        a->subtype = (ActionSubtype)sub;

    // Description
    ImGui::SetNextItemWidth(-1);
    char descBuf[256]; strncpy(descBuf, a->description.c_str(), 255); descBuf[255]='\0';
    if (ImGui::InputText("##adesc", descBuf, sizeof(descBuf)))
        a->description = descBuf;
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::SameLine(); ImGui::Text("description");
    ImGui::PopStyleColor();

    renderTagEditor(a->tags, a->id.c_str());

    // Fire once toggle
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, a->fireOnce ? col(166,218,149) : col(148,226,213));
    ImGui::Text(a->fireOnce ? "  Fire Once" : "  Fire Continuously");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    if (ImGui::SmallButton(a->fireOnce ? "→ Continuous##ft" : "→ Once##ft"))
        a->fireOnce = !a->fireOnce;
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Text, a->holdMode ? col(249,226,175) : col(108,112,134));
    ImGui::Checkbox("Hold##hm", &a->holdMode);
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("While triggers hold: effects apply on entry,\nChange State reverts on exit (pressure plates)");

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, col(69,71,90,120));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── Triggers ──────────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, col(249,226,175));
    ImGui::Text("  TRIGGERS");
    ImGui::PopStyleColor();

    if (!a->triggers.empty()) {
        int maxTrig = (int)a->triggers.size();
        if (a->triggerCountRequired > maxTrig) a->triggerCountRequired = maxTrig;
        if (a->triggerCountRequired < 1)       a->triggerCountRequired = 1;

        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        ImGui::Text("  Fire when");
        ImGui::PopStyleColor();
        ImGui::SameLine();
        ImGui::SetNextItemWidth(60);
        ImGui::SliderInt("##tcnt", &a->triggerCountRequired, 1, maxTrig);
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        if (maxTrig == 1)
            ImGui::Text("of %d triggers active", maxTrig);
        else if (a->triggerCountRequired == 1)
            ImGui::Text("of %d active  (OR logic)", maxTrig);
        else if (a->triggerCountRequired == maxTrig)
            ImGui::Text("of %d active  (AND logic)", maxTrig);
        else
            ImGui::Text("of %d active  (%d-of-%d)", maxTrig, a->triggerCountRequired, maxTrig);
        ImGui::PopStyleColor();
    }

    bool removeTrig = false;
    int  removeTrigIdx = -1;
    for (int ti = 0; ti < (int)a->triggers.size(); ti++) {
        ActionTrigger& trig = a->triggers[ti];
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col(36,39,58));
        char cid[32]; snprintf(cid, sizeof(cid), "##trig%d", ti);
        float trigH = (trig.type == TriggerType::LocationEntered) ? 90.f : 72.f;
        ImGui::BeginChild(cid, {-1, trigH}, true);

        bool locTrig = trig.type == TriggerType::LocationEntered;
        ImGui::SetNextItemWidth(160);
        int tt = (int)trig.type;
        if (ImGui::Combo("##ttype", &tt, TRIGGER_TYPE_NAMES, TRIGGER_TYPE_COUNT))
            trig.type = (TriggerType)tt;

        if (trig.type == TriggerType::ItemInteraction ||
            trig.type == TriggerType::ItemDestroyed) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(-70);
            std::vector<const char*> itemNames = { "(none)" };
            std::vector<std::string> itemIds   = { "" };
            for (auto& it : items) { itemNames.push_back(it.name.c_str()); itemIds.push_back(it.id); }
            int pick = 0;
            for (int k = 0; k < (int)itemIds.size(); k++)
                if (itemIds[k] == trig.itemRef) { pick = k; break; }
            char cmbid[32]; snprintf(cmbid, sizeof(cmbid), "##titem%d", ti);
            if (ImGui::Combo(cmbid, &pick, itemNames.data(), (int)itemNames.size()))
                trig.itemRef = itemIds[pick];
        } else if (trig.type == TriggerType::VariableThreshold) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            char vnbuf[64]; strncpy(vnbuf, trig.itemRef.c_str(), 63); vnbuf[63]='\0';
            char vnid[32]; snprintf(vnid, sizeof(vnid), "##tvar%d", ti);
            if (ImGui::InputText(vnid, vnbuf, sizeof(vnbuf)))
                trig.itemRef = vnbuf;   // variable name lives in itemRef
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            char thbuf[32]; snprintf(thbuf, sizeof(thbuf), "%.2f", trig.threshold);
            char fid[32]; snprintf(fid, sizeof(fid), "##tval%d", ti);
            ImGui::InputText(fid, thbuf, sizeof(thbuf), ImGuiInputTextFlags_CharsDecimal);
            trig.threshold = (float)atof(thbuf);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(50);
            char opid[32]; snprintf(opid, sizeof(opid), "##tcmp%d", ti);
            ImGui::Combo(opid, &trig.comparison, COMPARISON_OPS, COMPARISON_OP_COUNT);
        } else if (trig.type == TriggerType::TimerElapsed) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            char fid[32]; snprintf(fid, sizeof(fid), "##tsec%d", ti);
            ImGui::SliderFloat(fid, &trig.threshold, 0.1f, 60.f, "%.1f s");
        } else if (trig.type == TriggerType::LocationEntered) {
            ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
            char xid2[24]; snprintf(xid2, sizeof(xid2), "x##tri%d", ti);
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
            if (ImGui::SmallButton(xid2)) { removeTrig = true; removeTrigIdx = ti; }
            ImGui::PopStyleColor();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
            ImGui::Text("  X"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(55);
            char zxid[32]; snprintf(zxid, sizeof(zxid), "##zx%d", ti);
            ImGui::InputFloat(zxid, &trig.zoneX, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("Y"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(55);
            char zyid[32]; snprintf(zyid, sizeof(zyid), "##zy%d", ti);
            ImGui::InputFloat(zyid, &trig.zoneY, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("Z"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(55);
            char zzid[32]; snprintf(zzid, sizeof(zzid), "##zz%d", ti);
            ImGui::InputFloat(zzid, &trig.zoneZ, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("r"); ImGui::PopStyleColor(); ImGui::SameLine();
            ImGui::SetNextItemWidth(55);
            char zrid[32]; snprintf(zrid, sizeof(zrid), "##zr%d", ti);
            ImGui::InputFloat(zrid, &trig.zoneRadius, 0,0,"%.0f");
            ImGui::SameLine();
            char anyid[32]; snprintf(anyid, sizeof(anyid), "any##za%d", ti);
            ImGui::Checkbox(anyid, &trig.anyEntity);
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Zone also counts NPCs/enemies (pressure plates)");
            ImGui::EndChild();
            ImGui::PopStyleColor();
            continue;
        } else if (trig.type == TriggerType::PulseTimer) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            char pid[32]; snprintf(pid, sizeof(pid), "##pp%d", ti);
            ImGui::SliderFloat(pid, &trig.threshold, 0.4f, 20.f, "T %.1fs");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            char pdid[32]; snprintf(pdid, sizeof(pdid), "##pd%d", ti);
            ImGui::SliderFloat(pdid, &trig.zoneY, 0.1f, 10.f, "on %.1fs");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(55);
            char poid[32]; snprintf(poid, sizeof(poid), "##po%d", ti);
            ImGui::InputFloat(poid, &trig.zoneX, 0, 0, "%.1f");
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Phase offset (s)");
        }

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 30);
        char xid[24]; snprintf(xid, sizeof(xid), "x##tri%d", ti);
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        if (ImGui::SmallButton(xid)) { removeTrig = true; removeTrigIdx = ti; }
        ImGui::PopStyleColor();

        if (!trig.itemRef.empty() && !findItem(trig.itemRef)) {
            ImGui::PushStyleColor(ImGuiCol_Text, warnCol());
            ImGui::Text("  ⚠ referenced item was deleted");
            ImGui::PopStyleColor();
        }

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    if (removeTrig) a->triggers.erase(a->triggers.begin() + removeTrigIdx);

    ImGui::PushStyleColor(ImGuiCol_Text, col(249,226,175));
    if (ImGui::SmallButton("+ Add Trigger")) a->triggers.push_back({});
    ImGui::PopStyleColor();

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Separator, col(69,71,90,120));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::Spacing();

    // ── State Changes ─────────────────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
    ImGui::Text("  STATE CHANGES");
    ImGui::PopStyleColor();

    // Build item name list once for effect pickers
    std::vector<const char*> effItemNames = { "(none)" };
    std::vector<std::string> effItemIds   = { "" };
    for (auto& it : items) { effItemNames.push_back(it.name.c_str()); effItemIds.push_back(it.id); }

    bool removeEff = false;
    int  removeEffIdx = -1;
    for (int ei = 0; ei < (int)a->effects.size(); ei++) {
        ActionEffect& eff = a->effects[ei];
        ImGui::PushStyleColor(ImGuiCol_ChildBg, col(36,39,58));
        char cid[32]; snprintf(cid, sizeof(cid), "##eff%d", ei);
        ImGui::BeginChild(cid, {-1, 72}, true);

        // Effect type combo
        ImGui::SetNextItemWidth(130);
        int et = (int)eff.type;
        char etid[32]; snprintf(etid, sizeof(etid), "##etype%d", ei);
        if (ImGui::Combo(etid, &et, EFFECT_TYPE_NAMES, EFFECT_TYPE_COUNT))
            eff.type = (EffectType)et;

        ImGui::SameLine(ImGui::GetContentRegionAvail().x - 10);
        char xid[24]; snprintf(xid, sizeof(xid), "x##eff%d", ei);
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        if (ImGui::SmallButton(xid)) { removeEff = true; removeEffIdx = ei; }
        ImGui::PopStyleColor();

        // Type-specific fields on second row
        if (eff.type == EffectType::SetVariable) {
            ImGui::SetNextItemWidth(100);
            char vnid[32]; snprintf(vnid, sizeof(vnid), "##evar%d", ei);
            ImGui::InputText(vnid, eff.varName, sizeof(eff.varName));
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80);
            char opid2[32]; snprintf(opid2, sizeof(opid2), "##evop%d", ei);
            ImGui::Combo(opid2, &eff.varOp, VAR_OP_NAMES, VAR_OP_COUNT);
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            char valid[32]; snprintf(valid, sizeof(valid), "##eval%d", ei);
            ImGui::InputFloat(valid, &eff.value, 0,0,"%.2f");
        } else if (eff.type == EffectType::ChangeState) {
            int pick = 0;
            for (int k=0;k<(int)effItemIds.size();k++) if(effItemIds[k]==eff.targetRef){pick=k;break;}
            ImGui::SetNextItemWidth(100);
            char epid[32]; snprintf(epid, sizeof(epid), "##etgt%d", ei);
            if (ImGui::Combo(epid, &pick, effItemNames.data(), (int)effItemNames.size()))
                eff.targetRef = effItemIds[pick];
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("→"); ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(90);
            char ssid[32]; snprintf(ssid, sizeof(ssid), "##estate%d", ei);
            ImGui::InputText(ssid, eff.stateStr, sizeof(eff.stateStr));
        } else if (eff.type == EffectType::SpawnItem) {
            int pick = 0;
            for (int k=0;k<(int)effItemIds.size();k++) if(effItemIds[k]==eff.targetRef){pick=k;break;}
            ImGui::SetNextItemWidth(100);
            char epid[32]; snprintf(epid, sizeof(epid), "##etgt%d", ei);
            if (ImGui::Combo(epid, &pick, effItemNames.data(), (int)effItemNames.size()))
                eff.targetRef = effItemIds[pick];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##epx", &eff.posX, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##epy", &eff.posY, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##epz", &eff.posZ, 0,0,"%.0f");
            ImGui::SameLine();
            // Snap Y to terrain surface
            char snapid[32]; snprintf(snapid, sizeof(snapid), "^##esnap%d", ei);
            ImGui::PushStyleColor(ImGuiCol_Text, col(249,226,175));
            if (ImGui::SmallButton(snapid))
                eff.posY = safeSpawnY(eff.posX, eff.posZ);
            ImGui::PopStyleColor();
            if (ImGui::IsItemHovered()) ImGui::SetTooltip("Snap to terrain surface");
        } else if (eff.type == EffectType::DespawnItem || eff.type == EffectType::GiveItem) {
            int pick = 0;
            for (int k=0;k<(int)effItemIds.size();k++) if(effItemIds[k]==eff.targetRef){pick=k;break;}
            ImGui::SetNextItemWidth(160);
            char epid[32]; snprintf(epid, sizeof(epid), "##etgt%d", ei);
            if (ImGui::Combo(epid, &pick, effItemNames.data(), (int)effItemNames.size()))
                eff.targetRef = effItemIds[pick];
        } else if (eff.type == EffectType::Teleport) {
            int pick = 0;
            for (int k=0;k<(int)effItemIds.size();k++) if(effItemIds[k]==eff.targetRef){pick=k;break;}
            ImGui::SetNextItemWidth(90);
            char epid[32]; snprintf(epid, sizeof(epid), "##etgt%d", ei);
            if (ImGui::Combo(epid, &pick, effItemNames.data(), (int)effItemNames.size()))
                eff.targetRef = effItemIds[pick];
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##etpx", &eff.posX, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##etpy", &eff.posY, 0,0,"%.0f");
            ImGui::SameLine();
            ImGui::SetNextItemWidth(48); ImGui::InputFloat("##etpz", &eff.posZ, 0,0,"%.0f");
        } else if (eff.type == EffectType::ModifyHealth) {
            int pick = 0;
            for (int k=0;k<(int)effItemIds.size();k++) if(effItemIds[k]==eff.targetRef){pick=k;break;}
            ImGui::SetNextItemWidth(110);
            char epid[32]; snprintf(epid, sizeof(epid), "##etgt%d", ei);
            if (ImGui::Combo(epid, &pick, effItemNames.data(), (int)effItemNames.size()))
                eff.targetRef = effItemIds[pick];
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("Δhp"); ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(70);
            char hpid[32]; snprintf(hpid, sizeof(hpid), "##ehp%d", ei);
            ImGui::InputFloat(hpid, &eff.value, 0,0,"%.0f");
        } else if (eff.type == EffectType::PlaySound) {
            ImGui::SetNextItemWidth(200);
            char snid[32]; snprintf(snid, sizeof(snid), "##esnd%d", ei);
            ImGui::InputText(snid, eff.varName, sizeof(eff.varName));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("sound"); ImGui::PopStyleColor();
        } else if (eff.type == EffectType::SwitchLevel) {
            ImGui::SetNextItemWidth(160);
            char lvbuf[64]; strncpy(lvbuf, eff.targetRef.c_str(), 63); lvbuf[63]='\0';
            char lvid[32]; snprintf(lvid, sizeof(lvid), "##elvl%d", ei);
            if (ImGui::InputText(lvid, lvbuf, sizeof(lvbuf)))
                eff.targetRef = lvbuf;  // MapManager level id
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("level id"); ImGui::PopStyleColor();
        } else if (eff.type == EffectType::SequenceStep) {
            ImGui::SetNextItemWidth(100);
            char sqid[32]; snprintf(sqid, sizeof(sqid), "##esq%d", ei);
            ImGui::InputText(sqid, eff.varName, sizeof(eff.varName));
            ImGui::SameLine();
            ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("step"); ImGui::PopStyleColor();
            ImGui::SameLine();
            ImGui::SetNextItemWidth(60);
            char svid[32]; snprintf(svid, sizeof(svid), "##esqv%d", ei);
            ImGui::InputFloat(svid, &eff.value, 0, 0, "%.0f");
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("This step's index (1-based).\nWrong order resets the sequence.");
        }

        // Delay row
        ImGui::SetNextItemWidth(80);
        char did[32]; snprintf(did, sizeof(did), "##edel%d", ei);
        ImGui::SliderFloat(did, &eff.delay, 0.f, 30.f, "%.1fs");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol()); ImGui::Text("delay"); ImGui::PopStyleColor();

        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    if (removeEff) a->effects.erase(a->effects.begin() + removeEffIdx);

    ImGui::PushStyleColor(ImGuiCol_Text, col(148,226,213));
    if (ImGui::SmallButton("+ Add State Change")) {
        a->effects.push_back({});
    }
    ImGui::PopStyleColor();

    // Orphan warning
    if (isOrphan(*a)) {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, warnCol());
        ImGui::TextWrapped("  No triggers — this action is an orphan and will never fire.");
        ImGui::PopStyleColor();
    }
}

// ── Global variables ──────────────────────────────────────────────────────

void ScenePanel::renderVarList() {
    ImGui::PushStyleColor(ImGuiCol_Text, varCol());
    ImGui::Text("  GLOBAL VARIABLES");
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::TextWrapped("  Readable by any Action as trigger or effect.");
    ImGui::PopStyleColor();
    ImGui::Spacing();

    bool remove = false; int removeIdx = -1;
    for (int i = 0; i < (int)globalVars.size(); i++) {
        GlobalVar& v = globalVars[i];
        ImGui::SetNextItemWidth(120);
        char nid[32]; snprintf(nid, sizeof(nid), "##vn%d", i);
        ImGui::InputText(nid, v.name, sizeof(v.name));
        ImGui::SameLine();
        ImGui::SetNextItemWidth(100);
        char vid[32]; snprintf(vid, sizeof(vid), "##vv%d", i);
        ImGui::InputFloat(vid, &v.value, 1.f, 10.f, "%.2f");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
        char xid[24]; snprintf(xid, sizeof(xid), "x##vr%d", i);
        if (ImGui::SmallButton(xid)) { remove = true; removeIdx = i; }
        ImGui::PopStyleColor();
    }
    if (remove) globalVars.erase(globalVars.begin() + removeIdx);

    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, varCol());
    if (ImGui::Button("+ Add Variable", {-1, 0})) {
        GlobalVar v; snprintf(v.name, sizeof(v.name), "var%d", (int)globalVars.size()+1);
        globalVars.push_back(v);
    }
    ImGui::PopStyleColor();
}

// ── Main render ───────────────────────────────────────────────────────────

bool ScenePanel::render(WorldSettings& ws, PlayerSettings& ps, bool& flyMode,
                        std::vector<Weapon*>& weapons, int& activeSlot,
                        Character& c, CharacterRenderer& cr, EntityManager& entities,
                        std::string& statusMsg, World& world,
                        const std::string& savePath) {
    bool changed = false;

    // Always sync active Level item's settings into ws
    if (!selectedItemId.empty()) {
        if (auto* item = findItem(selectedItemId)) {
            if (item->type == ItemType::Level) {
                // Keep customMeshPath consistent with meshPath
                strncpy(item->levelSettings.customMeshPath,
                        item->meshPath.c_str(),
                        sizeof(item->levelSettings.customMeshPath)-1);
                ws = item->levelSettings;
            }
        }
    }

    renderBreadcrumb();

    switch (view) {
        case PanelView::Home:
            renderHome();
            break;
        case PanelView::ItemTypeFolders:
            renderItemTypeFolders();
            break;
        case PanelView::ItemList:
            renderItemList(weapons, entities);
            break;
        case PanelView::ItemInspector:
            if (renderItemInspector(ws, ps, flyMode, weapons, activeSlot,
                                    c, cr, entities, statusMsg, world, savePath))
                changed = true;
            break;
        case PanelView::ActionList:
            renderActionList();
            break;
        case PanelView::ActionInspector:
            renderActionInspector();
            break;
        case PanelView::VarList:
            renderVarList();
            break;
    }

    // Footer
    int winH = 0; { ImVec2 sz = ImGui::GetWindowSize(); winH = (int)sz.y; }
    ImGui::SetCursorPosY((float)winH - 42.f);
    ImGui::PushStyleColor(ImGuiCol_Separator, col(69,71,90,120));
    ImGui::Separator();
    ImGui::PopStyleColor();
    ImGui::PushStyleColor(ImGuiCol_Text, dimCol());
    ImGui::Text("  %.0f FPS", ImGui::GetIO().Framerate);
    ImGui::PopStyleColor();

    return changed;
}
