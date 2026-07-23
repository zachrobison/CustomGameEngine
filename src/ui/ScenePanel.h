#pragma once
#include <glm/glm.hpp>
#include "../Settings.h"
#include "../game/Weapon.h"
#include "../character/Character.h"
#include "../character/CharacterRenderer.h"
#include "../character/CharacterPanel.h"
#include "../character/EntityManager.h"
#include "../game/MapManager.h"
#include <vector>
#include <string>

// ── Item types ────────────────────────────────────────────────────────────
enum class ItemType : int {
    Player=0, Enemy, Weapon, Projectile, Level, NPC,
    Pickup, Vehicle, Camera, HUD, Audio, Effect,
    ConstructionPiece, TriggerZone
};
static constexpr const char* ITEM_TYPE_NAMES[] = {
    "Player","Enemy","Weapon","Projectile","Level","NPC",
    "Pickup","Vehicle","Camera","HUD","Audio","Effect",
    "Construction Piece","Trigger Zone"
};
static constexpr const char* ITEM_TYPE_PLURAL[] = {
    "Players","Enemies","Weapons","Projectiles","Levels","NPCs",
    "Pickups","Vehicles","Cameras","HUDs","Audio","Effects",
    "Construction Pieces","Trigger Zones"
};
static constexpr int ITEM_TYPE_COUNT = 14;

// ── Action subtypes ───────────────────────────────────────────────────────
enum class ActionSubtype : int {
    Door=0, Spawn, Despawn, Cutscene, CameraChange,
    GiveItem, TakeItem, ModifyVariable, PlaySound, TriggerEffect,
    Teleport, DamageHeal, LockUnlock, Dialogue, SceneTransition, BossPhase
};
static constexpr const char* ACTION_SUBTYPE_NAMES[] = {
    "Door","Spawn","Despawn","Cutscene","Camera Change",
    "Give Item","Take Item","Modify Variable","Play Sound","Trigger Effect",
    "Teleport","Damage / Heal","Lock / Unlock","Dialogue","Scene Transition","Boss Phase"
};
static constexpr int ACTION_SUBTYPE_COUNT = 16;

// ── Trigger types ─────────────────────────────────────────────────────────
enum class TriggerType : int {
    ItemInteraction=0, VariableThreshold, ItemDestroyed, TimerElapsed,
    GameStateChange, LocationEntered, PulseTimer
};
static constexpr const char* TRIGGER_TYPE_NAMES[] = {
    "Item Interaction","Variable Threshold","Item Destroyed",
    "Timer Elapsed","Game State Change","Location Entered","Pulse Timer"
};
static constexpr int TRIGGER_TYPE_COUNT = 7;
static constexpr const char* COMPARISON_OPS[] = {">=",">","<=","<","==","!="};
static constexpr int COMPARISON_OP_COUNT = 6;

struct ActionTrigger {
    TriggerType type = TriggerType::ItemInteraction;
    std::string itemRef;
    float threshold  = 0.f;    // PulseTimer: period (s)
    int   comparison = 0;
    // LocationEntered
    float zoneX = 0.f, zoneY = 0.f, zoneZ = 0.f, zoneRadius = 5.f;
    bool  anyEntity = false;   // zone counts NPCs/enemies too (pressure plates)
    // PulseTimer reuses zoneX = phase offset (s), zoneY = on-duration (s)
};

// ── Effect types ──────────────────────────────────────────────────────────
enum class EffectType : int {
    ChangeState=0, SetVariable, SpawnItem, DespawnItem,
    Teleport, ModifyHealth, GiveItem, PlaySound, SwitchLevel, SequenceStep
};
static constexpr const char* EFFECT_TYPE_NAMES[] = {
    "Change State","Set Variable","Spawn Item","Despawn Item",
    "Teleport","Modify Health","Give Item","Play Sound","Switch Level",
    "Sequence Step"
};
static constexpr int EFFECT_TYPE_COUNT = 10;

// SetVariable operation mode.
static constexpr const char* VAR_OP_NAMES[] = { "Set","Add","Subtract","Multiply" };
static constexpr int VAR_OP_COUNT = 4;

struct ActionEffect {
    EffectType  type     = EffectType::ChangeState;
    float       delay    = 0.f;
    std::string targetRef;         // item id target / SwitchLevel: level id
    char        varName[64]  = ""; // SetVariable
    float       value        = 0.f;// SetVariable / ModifyHealth
    int         varOp        = 0;  // SetVariable: 0=Set,1=Add,2=Subtract,3=Multiply
    char        stateStr[64] = "active"; // ChangeState
    float       posX=0, posY=0, posZ=0;  // SpawnItem / Teleport
};
struct GlobalVar {
    char  name[64] = "variable";
    float value    = 0.f;
};

// ── Game item ─────────────────────────────────────────────────────────────
struct GameItem {
    std::string id, name = "Untitled";
    ItemType    type = ItemType::Player;
    std::string description;
    std::vector<std::string> tags;

    // Engine-backed references
    WorldSettings  levelSettings;
    PlayerSettings playerSettings;
    int            weaponIdx = -1;   // index into runtime weapons vector
    std::string    entityId;
    std::string    meshPath;         // Level: path to custom .glb terrain mesh

    // Mesh presentation
    float meshScale = 1.f;

    // Shared stats — which are shown depends on type
    float health = 100.f, speed = 10.f, jumpForce = 8.f, gravity = 20.f;
    float damage = 10.f,  range = 20.f, fireRate = 0.5f;
    int   ammo = 30;
    float reloadTime = 1.5f, detectRadius = 10.f;
    bool  boss = false, patrolling = true, autoFire = false;
    float stackSize = 1.f, weight = 1.f;
    float topSpeed = 20.f, turnRadius = 5.f;
    float fov = 70.f;
    bool  destructible = true;
    float hp = 100.f;

    // Generic on/off state, driven by ChangeState effects (doors, trigger
    // zones, lockable items). Meaning is type-dependent; engine-interpreted
    // only for TriggerZone (an inactive zone doesn't fire) so far.
    bool active = true;
};

// ── Game action ───────────────────────────────────────────────────────────
struct GameAction {
    std::string id, name = "NewAction";
    ActionSubtype subtype = ActionSubtype::Door;
    std::vector<ActionTrigger> triggers;
    std::vector<ActionEffect>  effects;
    bool fireOnce = true;
    bool holdMode = false;  // effects apply while triggers hold; ChangeState
                            // reverts when they release (pressure plates)
    int  triggerCountRequired = 1;
    std::string description, levelScope = "global";
    std::vector<std::string> tags;

    bool fired  = false; // runtime only (play mode), never serialized
    bool wasSat = false; // runtime: holdMode edge detection
};

// ── Navigation ────────────────────────────────────────────────────────────
enum class PanelView {
    Home, ItemTypeFolders, ItemList, ItemInspector,
    ActionList, ActionInspector, VarList
};

// ── Scene panel ───────────────────────────────────────────────────────────
class ScenePanel {
public:
    ScenePanel();

    void addDefaultNodes(const std::vector<Weapon*>& weapons);

    // Project persistence: items, actions, and global vars as JSON.
    // Lets a project.json be hand-authored/generated externally, then edited
    // further from the in-app GUI and saved back to the same file.
    bool saveProject(const std::string& path) const;
    bool loadProject(const std::string& path);

    // Copies the first Level item's settings into `out` (true if one exists).
    // Called after loadProject so saved terrain settings apply on launch.
    bool getFirstLevelSettings(WorldSettings& out) const;

    // Returns world positions of all SpawnItem effects (for marker rendering).
    std::vector<glm::vec3> getSpawnMarkers() const;

    // ── Play-mode action executor ─────────────────────────────────────────
    // Per-frame context handed in by the game loop; out-fields are read back
    // after the call (teleport request, status message).
    struct PlayCtx {
        glm::vec3      playerPos       = {0,0,0};
        bool           interactPressed = false;   // E edge, for ItemInteraction
        class World*   world           = nullptr;
        EntityManager* entities        = nullptr;
        float*         playerHealth    = nullptr; // ModifyHealth on Player items
        float          playerMaxHealth = 100.f;
        // Out:
        glm::vec3      teleportTo = {0,0,0};
        bool           teleported = false;
        bool           levelSwitched = false; // re-sync WorldSettings after
        std::string    statusMsg;
    };
    // Call once when entering play mode: clears fired flags, timers, queues.
    void resetActionRuntime();
    // Call every frame while in play mode.
    void updateActions(float dt, PlayCtx& ctx);

    // Play mode must not write kills/door-state back into level files.
    void setRuntimePersist(bool on) { mapManager.persistRuntime = on; }

    // Direct level swap (dimension-shift hotkey): saves the current level,
    // loads `id` (fresh if it doesn't exist yet), resets action runtime.
    void switchToLevel(const std::string& id, class World& world, EntityManager& entities) {
        mapManager.switchLevel(id, world, entities, *this);
        resetActionRuntime();
    }
    const std::string& currentLevelId() const { return mapManager.currentId(); }

    // Door-glue: props groups query an item's active flag by id each frame.
    bool isItemActive(const std::string& id) const {
        for (auto& it : items) if (it.id == id) return it.active;
        return true;
    }

    struct VehicleItemInfo {
        std::string meshPath;
        float topSpeed    = 28.f;
        float turnDegPerS = 80.f;
        float meshScale   = 1.f;
    };
    // Returns info for the first Vehicle-type item (empty meshPath if none).
    VehicleItemInfo getFirstVehicleInfo() const;

    bool render(WorldSettings& ws, PlayerSettings& ps, bool& flyMode,
                std::vector<Weapon*>& weapons, int& activeSlot,
                Character& c, CharacterRenderer& cr, EntityManager& entities,
                std::string& statusMsg,
                class World& world, const std::string& savePath);

private:
    std::vector<GameItem>   items;
    std::vector<GameAction> actions;
    std::vector<GlobalVar>  globalVars;

    PanelView   view       = PanelView::Home;
    ItemType    typeFolder = ItemType::Player;
    std::string selectedItemId, selectedActionId;
    char        actionSearch[128] = "";

    bool creating     = false;
    int  createTypeIdx = 0;
    char createName[64] = "";

    bool creatingAction  = false;
    int  createSubtype   = 0;
    char createActionName[64] = "";

    int  idCtr      = 0;
    char newTagBuf[32] = "";
    int  npcCtr     = 1;
    char spawnName[64] = "NPC";
    float spawnPos[3] = {5.f, 0.f, 0.f};

    CharacterPanel charPanel;
    MapManager     mapManager;

    // Action executor runtime state (play mode only)
    struct PendingEffect { ActionEffect fx; float timeLeft; };
    std::vector<PendingEffect> pendingFx;
    float       playClock = 0.f;
    std::string pendingLevelSwitch;

    GlobalVar* findVar(const char* name, bool createIfMissing);
    bool evalTrigger(const ActionTrigger& t, const PlayCtx& ctx);
    void applyEffect(const ActionEffect& e, PlayCtx& ctx);

    std::string nextId() { return "obj" + std::to_string(++idCtr); }
    GameItem*   findItem  (const std::string& id);
    GameAction* findAction(const std::string& id);
    bool        isOrphan  (const GameAction& a) const;
    bool        hasBrokenRef(const GameAction& a) const;

    void goBack();
    void renderBreadcrumb();
    void renderHome();
    void renderItemTypeFolders();
    void renderItemList   (std::vector<Weapon*>& weapons, EntityManager& entities);
    bool renderItemInspector(WorldSettings& ws, PlayerSettings& ps, bool& flyMode,
                             std::vector<Weapon*>& weapons, int& activeSlot,
                             Character& c, CharacterRenderer& cr,
                             EntityManager& entities, std::string& statusMsg,
                             class World& world, const std::string& savePath);
    void renderActionList ();
    void renderActionInspector();
    void renderVarList    ();

    bool renderLevelInspector  (GameItem&, std::string&, class World&, const std::string&, EntityManager&);
    bool renderPlayerInspector (GameItem&, PlayerSettings&, bool&, Character&,
                                CharacterRenderer&, EntityManager&);
    bool renderWeaponInspector (GameItem&, std::vector<Weapon*>&, int&);
    void renderEnemyInspector  (GameItem&, EntityManager&, Character& tmpl);
    void renderNPCInspector    (GameItem&, EntityManager&, Character& tmpl);
    void renderGenericInspector(GameItem&);

    void renderTagEditor(std::vector<std::string>& tags, const char* uid);
    void renderMeshPicker(GameItem& item, const char* uid);
};
