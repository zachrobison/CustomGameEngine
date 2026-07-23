#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <GLFW/glfw3.h>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#include "Settings.h"
#include "renderer/Shader.h"
#include "renderer/Camera.h"
#include "renderer/LineRenderer.h"
#include "renderer/Skybox.h"
#include "voxel/World.h"
#include "voxel/Noise.h"
#include "game/Player.h"
#include "game/Weapon.h"
#include "game/WaveController.h"
#include "game/GameConfig.h"
#include "game/Turrets.h"
#include "game/Bullets.h"
#include "game/BossController.h"
#include "game/RaceBots.h"
#include "game/Rts.h"
#include "game/Factory.h"
#include "ui/UI.h"
#include "platform/PlayerProfile.h"
#include "platform/KeyBinds.h"
#include "platform/Audio.h"
#include "character/Character.h"
#include "character/CharacterRenderer.h"
#include "character/EntityManager.h"
#include "renderer/Terrain.h"
#include "renderer/TreeRenderer.h"
#include "renderer/GltfModel.h"
#include "renderer/SkinnedModel.h"
#include "renderer/Props.h"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <string>
#include <map>

// Mouse-wheel routing (GLFW callbacks can't capture): RTS zoom + factory
// build-palette cycling.
static Rts*     g_rtsScroll = nullptr;
static Factory* g_factoryPtr = nullptr;
static void rtsScrollCb(GLFWwindow* w, double xo, double yo) {
    ImGui_ImplGlfw_ScrollCallback(w, xo, yo);
    if (g_rtsScroll) g_rtsScroll->addScroll((float)yo);
    if (g_factoryPtr && g_factoryPtr->buildMode && yo != 0.0)
        g_factoryPtr->cycleType(yo > 0 ? 1 : -1);
}

int main() {
    srand((unsigned)time(nullptr));

    PlayerProfile::get().init();
    Audio::get().init();

    if (!glfwInit()) { fputs("GLFW init failed\n", stderr); return 1; }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_COCOA_RETINA_FRAMEBUFFER, GL_TRUE);

    GLFWwindow* window = glfwCreateWindow(1400, 900, "Voxel Engine", nullptr, nullptr);
    if (!window) { fputs("Window creation failed\n", stderr); glfwTerminate(); return 1; }
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // ImGui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::GetIO().IniFilename = nullptr;
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 410");

    UI ui;
    ui.applyObsidianStyle();

    // Settings
    WorldSettings  worldSettings;
    PlayerSettings playerSettings;

    // Rebindable keys, per-project (profile save dir)
    KeyBinds    keyBinds;
    std::string keyBindsPath = PlayerProfile::get().saveDir() + "/keybinds.json";
    keyBinds.loadOrDefaults(keyBindsPath);
    ui.keyBinds     = &keyBinds;
    ui.keyBindsPath = keyBindsPath;

    // Weapons (owned here, Player holds non-owning pointers)
    VoxelSprayer sprayer;
    VoxelEraser  eraser;
    Gun          gun;
    MeleeWeapon  melee;
    TurretPlacer turretPlacer;

    // Systems
    World  world;
    Player player;
    player.weapons    = { &sprayer, &eraser, &gun, &melee };
    player.activeSlot = 0;

    // Spawn player above terrain at origin
    // Mesh terrain: index-based noise, grid centre is index (128,128)
    int   spawnH = world.getTerrainHeight(0, 0);   // voxel units
    float meshH  = Noise::fractal(128 * 0.025f, 128 * 0.025f, 4) * 28.f + 20.f;
    float startH;
    if      (worldSettings.voxelWorld)  startH = spawnH * VOXEL_SIZE + 3.f;
    else if (worldSettings.flatTerrain) startH = 20.f + 4.f;  // flat floor is Y=20
    else                                startH = meshH + 4.f;
    player.camera.position = { 0.5f, startH, 0.5f };

    // Try to load a previous save; otherwise stream fresh terrain
    std::string savePath = PlayerProfile::get().saveDir() + "/world.vox";
    if (!world.load(savePath)) {
        world.streamChunks(player.camera.position, worldSettings.render_dist);
        world.update();
    } else {
        world.update();
        ui.statusMsg = "Save loaded.";
    }

    Shader       shader("shaders/voxel.vert", "shaders/voxel.frag");
    LineRenderer lines;
    Skybox       skybox;
    Terrain      terrain;       // noise terrain (regenerated when mode changes)
    Terrain      flatTerrain;   // checker-grid flat floor
    flatTerrain.generate(256, 4.f, /*flat=*/true);
    TreeRenderer trees;

    // Per-level geometry/collision: loaded from maps/<id>/props.json by the
    // level watcher in the main loop (visible boxes draw; invisible ones are
    // collision volumes for GLB level meshes).
    Props props;
    BossController bossAI;
    bool  levelHadBoss   = false;   // this level has a boss to defeat
    float levelWinTimer  = 0.f;     // >0 = level cleared, counting to menu

    SkinnedModel playerModel;   // animated player avatar (third person)
    if (!playerModel.load("assets/models/retro_human.glb"))
        fputs("SkinnedModel: retro_human.glb failed to load (will be skipped)\n", stderr);

    GltfModel gunModel;         // first-person viewmodel (Kenney blaster, CC0)
    if (!gunModel.load("assets/models/gun.glb"))
        fputs("GltfModel: gun.glb failed to load (viewmodel skipped)\n", stderr);

    GltfModel swordModel;       // melee visual (Knight's Sword, CC0/OGA)
    if (!swordModel.load("assets/models/sword.obj"))
        fputs("GltfModel: sword.obj failed to load\n", stderr);

    GltfModel bulletModel;      // 3D round for delayed-parry bullets
    if (!bulletModel.load("assets/models/bullet.glb"))
        fputs("GltfModel: bullet.glb failed to load\n", stderr);

    TreeRenderer factoryTrees;  // procedural low-poly trees for Iron Command

    GltfModel carInterior;      // first-person cockpit (user's model)
    // Eye marker from the FBX camera, in glTF coords (left-hand drive).
    const glm::vec3 INT_EYE = {-0.545f, 0.740f, 0.378f};
    if (!carInterior.load("assets/models/car_interior.glb"))
        fputs("GltfModel: car_interior.glb failed to load\n", stderr);

    SkinnedModel enemyModel;    // shared enemy visual ("for now": same human)
    if (!enemyModel.load("assets/models/retro_human.glb"))
        fputs("SkinnedModel: enemy model failed to load\n", stderr);
    enemyModel.play("Run");

    // User's pig mafia gunner (authored in Blender, tommy-gun fire loop).
    // Source model is ~20.7 units tall with a mid-body origin — scale to
    // 1.9u and lift so the feet meet the entity's ground position.
    SkinnedModel pigGunnerModel;
    const float PIG_SCALE = 1.9f / 20.7f;
    const float PIG_YOFF  = 12.93f * PIG_SCALE;
    if (!pigGunnerModel.load("assets/models/pig_gunner.glb"))
        fputs("SkinnedModel: pig_gunner.glb failed to load\n", stderr);
    pigGunnerModel.setCombineAll(true);   // body + gun rigs play together
    pigGunnerModel.setSnapFps(12.f);      // stop-motion look (12 poses/s)

    SkinnedModel wizardHand;              // Iron Command first-person build hand
    if (!wizardHand.load("assets/models/wizard_hand.glb"))
        fputs("SkinnedModel: wizard_hand.glb failed to load\n", stderr);
    wizardHand.setCombineAll(true);

    // Car — mesh loaded dynamically from the Vehicle item's meshPath
    std::map<std::string, GltfModel> vehicleMeshes;
    std::string lastCarMeshPath = "\x01"; // force first-frame load
    float       carMeshScale    = 3.f;
    // Start car near player; gravity will settle it
    glm::vec3 carPos     = { 6.f, startH + 2.f, 6.f };
    glm::vec3 carVel     = { 0.f, 0.f, 0.f };
    float     carHeading = 0.f;

    // Per-level custom mesh cache  (level item id → loaded model)
    std::map<std::string, GltfModel> levelMeshes;
    std::string lastMeshPath; // detect path changes so we reload
    Character        character;
    CharacterRenderer charRenderer;
    charRenderer.rebuild(character);

    EntityManager entities;

    // Boss muzzle: sample the animated gun node so sprayed rounds start at
    // the gun and follow its sweep (arcing spray instead of appearing).
    bossAI.setMuzzle([&](const std::string& entId,
                         glm::vec3& origin, glm::vec3& dir) -> bool {
        glm::mat4 gunXf;
        if (!pigGunnerModel.nodeWorld("gun", gunXf)) return false;
        for (auto& e : entities.all()) {
            if (e.id != entId) continue;
            float sc   = PIG_SCALE * 1.7f;             // boss render transform
            float yoff = PIG_YOFF * 1.7f;
            glm::mat4 W = glm::translate(glm::mat4(1.f),
                                         e.position + glm::vec3(0, yoff, 0))
                        * glm::rotate(glm::mat4(1.f), e.facingY, glm::vec3(0,1,0))
                        * glm::scale(glm::mat4(1.f), glm::vec3(sc));
            glm::mat4 G = W * gunXf;
            origin = glm::vec3(G[3]);                  // gun position
            // Pick the gun-local axis best aligned with the pig's facing so
            // the same physical axis (the barrel) drives the whole sweep.
            glm::vec3 face = {std::sin(e.facingY), 0.f, std::cos(e.facingY)};
            glm::vec3 best = face; float bestDot = -2.f;
            for (int a = 0; a < 3; a++) {
                glm::vec3 ax = glm::normalize(glm::vec3(G[a]));
                for (float sgn : {1.f, -1.f}) {
                    float d = glm::dot(ax * sgn, face);
                    if (d > bestDot) { bestDot = d; best = ax * sgn; }
                }
            }
            best.y *= 0.35f;                           // don't dump into floor
            dir = glm::normalize(best);
            return true;
        }
        return false;
    });

    // Load player character and entity list from save dir
    std::string entitySavePath = PlayerProfile::get().saveDir() + "/entities.bin";
    entities.load(entitySavePath);

    // Ensure the local player entity exists
    std::string playerId = PlayerProfile::get().id();
    if (!entities.find(playerId)) {
        CharacterEntity* pe = entities.spawn(playerId, PlayerProfile::get().name(),
                                             player.camera.position, true);
        if (pe) pe->character = character;
    } else {
        // Restore saved character appearance
        CharacterEntity* pe = entities.find(playerId);
        character = pe->character;
        charRenderer.rebuild(character);
    }

    // Populate scene nodes (Items/Actions) from project.json if one exists,
    // otherwise fall back to the built-in defaults (Level, Player, Weapons).
    std::string projectPath = PlayerProfile::get().saveDir() + "/project.json";
    if (ui.scenePanel.loadProject(projectPath))
        ui.scenePanel.getFirstLevelSettings(worldSettings); // apply saved terrain mode
    else
        ui.scenePanel.addDefaultNodes(player.weapons);

    WaveController wavesCtl;   // active only on levels with a waves.json
    RaceBots       raceBots;   // active only on levels with a waypoints.json
    Rts            rts;        // Iron Command: top-down factory-RTS mode (legacy)
    Factory        factory;    // Iron Command: first-person build/factory
    g_rtsScroll   = &rts;
    g_factoryPtr  = &factory;
    glfwSetScrollCallback(window, rtsScrollCb);

    // ── Games are data projects: pick one on the main menu ───────────────
    std::vector<GameConfig> games =
        GameConfig::scan(PlayerProfile::get().saveDir() + "/games");
    GameConfig gameCfg;        // defaults = everything enabled
    bool       inMenu = true;

    std::string lastLevelId;   // level watcher: reload props on any switch

    auto applyGame = [&](const GameConfig& g) {
        gameCfg = g;
        player.allowDash    = g.dash;
        player.allowGrapple = g.grapple;
        player.allowRocket  = g.rocket;
        player.allowJetpack = g.jetpack;
        player.weapons.clear();
        for (auto& wn : g.weapons) {
            if      (wn == "sprayer") player.weapons.push_back(&sprayer);
            else if (wn == "eraser")  player.weapons.push_back(&eraser);
            else if (wn == "gun")     player.weapons.push_back(&gun);
            else if (wn == "melee")   player.weapons.push_back(&melee);
            else if (wn == "turret")  player.weapons.push_back(&turretPlacer);
        }
        Turrets::get().clear();
        if (player.weapons.empty()) player.weapons.push_back(&melee);
        player.activeSlot = 0;
        player.inVehicle  = false;
        player.flyMode    = false;   // games start on foot; G = debug fly
        if (g.rts) rts.start((unsigned)time(nullptr));   // Iron Command (legacy RTS)
        if (g.factory) {
            factory.setCollider(&props);
            factory.reset((unsigned)time(nullptr));
            factoryTrees.buildFrom(factory.treeList(), 20.f);   // procedural trees
            ui.extraHud = [&]{ int w,h; glfwGetWindowSize(window,&w,&h);
                               factory.renderHud(w,h); };
        } else {
            factory.active = false;
            ui.extraHud = nullptr;
        }

        // Stop-motion aesthetic for the iso/souls game: hold 12 poses/s
        float snap = g.isoCamera ? 12.f : 0.f;
        playerModel.setSnapFps(snap);
        enemyModel.setSnapFps(snap);
        ui.scenePanel.setRuntimePersist(false);   // playing, not editing
        ui.scenePanel.switchToLevel(g.bootLevel, world, entities);
        ui.scenePanel.getFirstLevelSettings(worldSettings);
        lastLevelId.clear();          // force the level watcher even on replay
        // Fresh spawn on non-voxel maps: drop in above the origin instead of
        // wherever the previous game left the camera.
        if (!worldSettings.voxelWorld) {
            player.camera.position = {0.f, 24.f, 0.f};
            player.velocity        = {0.f, 0.f, 0.f};
        }
    };

    // Input state
    bool   mouseCaptured = true;
    bool   tabWasDown    = false;
    bool   f5WasDown     = false, f6WasDown = false;
    bool   eWasDown      = false;
    bool   vWasDown      = false;
    bool   thirdPerson   = false;
    bool   wasPlayMode   = false;
    double lastX = 700, lastY = 450;
    bool   firstFrame = true;

    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    // Matches the noise formula used by Terrain.cpp and Player.cpp collision
    auto terrainGroundY = [](const WorldSettings& ws, float wx, float wz) -> float {
        if (ws.flatTerrain || ws.customMeshPath[0] != '\0') return 20.f;
        float ix = wx / 4.f + 128.f;
        float iz = wz / 4.f + 128.f;
        return Noise::fractal(ix * 0.025f, iz * 0.025f, 4) * 28.f + 20.f;
    };

    double prevTime = glfwGetTime();

    while (!glfwWindowShouldClose(window)) {
        double now = glfwGetTime();
        float  dt  = std::min((float)(now - prevTime), 0.05f); // cap at 50ms
        prevTime   = now;

        glfwPollEvents();

        Audio::get().update();

        // ── Main menu: pick the game project to load ──────────────────────
        if (inMenu) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            mouseCaptured = false;
            Audio::get().stopAllLoops();
            int fbW2, fbH2;
            glfwGetFramebufferSize(window, &fbW2, &fbH2);
            glViewport(0, 0, fbW2, fbH2);
            glClearColor(0.09f, 0.09f, 0.14f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            int pick = ui.renderMainMenu(games, window);
            if (pick >= 0) {
                applyGame(games[pick]);
                inMenu        = false;
                mouseCaptured = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                firstFrame = true;
            }
            glfwSwapBuffers(window);
            continue;
        }
        if (ui.requestMenu) {          // "Games" button in the panel
            ui.requestMenu = false;
            ui.playMode    = false;
            inMenu         = true;
            continue;
        }

        // ── Iron Command: self-contained top-down RTS frame ───────────────
        if (gameCfg.rts) {
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS) {
                inMenu = true; continue;
            }
            int fbW2, fbH2;
            glfwGetFramebufferSize(window, &fbW2, &fbH2);
            float aspect2 = fbH2 > 0 ? (float)fbW2 / fbH2 : 1.6f;

            rts.handleInput(window, dt, aspect2);
            rts.update(dt);

            glViewport(0, 0, fbW2, fbH2);
            glClearColor(0.55f, 0.62f, 0.72f, 1.f);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            glEnable(GL_DEPTH_TEST);

            glm::vec3 sunDir = glm::normalize(glm::vec3(0.4f, 0.85f, 0.35f));
            glm::mat4 VP = rts.proj(aspect2) * rts.view();
            rts.render(VP, sunDir, 0.00035f);

            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            rts.renderHud();
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(window);
            continue;
        }

        // ── Level watcher: fires on ANY level switch (T, portals, UI) ─────
        if (ui.scenePanel.currentLevelId() != lastLevelId) {
            if (!lastLevelId.empty())              // silent on boot
                Audio::get().play("shift", 0.7f);
            lastLevelId = ui.scenePanel.currentLevelId();
            std::string levelDir = PlayerProfile::get().saveDir() + "/maps/" + lastLevelId;
            props.loadFile(levelDir + "/props.json");
            wavesCtl.load(levelDir);   // horde mode iff waves.json exists
            raceBots.load(levelDir);   // race bots iff waypoints.json exists
            ui.scenePanel.getFirstLevelSettings(worldSettings);

            // Race: park the car on the start line, player beside it (pole).
            if (raceBots.active()) {
                glm::vec3 sp = raceBots.startPos();
                carPos     = {sp.x, 20.f, sp.z};
                carHeading = raceBots.startHeadingDeg();
                player.camera.position = carPos + glm::vec3(2.5f, 1.65f, 0.f);
                player.velocity = {0,0,0};
            }

            // Bosses ship in entities.bin with default stats; buff on load
            levelHadBoss = false;
            for (auto& e : entities.all())
                if (e.name == "Boss") { e.health = e.maxHealth = 600.f; levelHadBoss = true; }
            bossAI.reset();
            levelWinTimer = 0.f;

            // De-stick entities that loaded inside geometry: ring-search a
            // free spot nearby (pillar/wall overlaps from map generation)
            for (auto& e : entities.all()) {
                if (e.isPlayer) continue;
                glm::vec3 eye = e.position + glm::vec3(0, 1.65f, 0);
                if (!props.overlapsPlayer(eye) &&
                    !(worldSettings.voxelWorld && world.overlapsVoxel(eye))) continue;
                bool freed = false;
                for (int r = 1; r <= 4 && !freed; r++) {
                    for (int k = 0; k < 8 && !freed; k++) {
                        float a = k * 0.785398f;
                        glm::vec3 np = e.position +
                            glm::vec3(std::cos(a) * r, 0, std::sin(a) * r);
                        glm::vec3 ne = np + glm::vec3(0, 1.65f, 0);
                        if (!props.overlapsPlayer(ne) &&
                            !(worldSettings.voxelWorld && world.overlapsVoxel(ne))) {
                            e.position = np;
                            freed = true;
                        }
                    }
                }
            }

            // Depenetrate: if the new dimension has a wall where you stand,
            // pop up to the first free spot instead of spawning inside it.
            auto solidAt = [&](glm::vec3 eye) {
                return (worldSettings.voxelWorld && world.overlapsVoxel(eye))
                       || props.overlapsPlayer(eye);
            };
            if (solidAt(player.camera.position)) {
                for (int up = 1; up <= 24; up++) {
                    glm::vec3 p = player.camera.position + glm::vec3(0, (float)up, 0);
                    if (!solidAt(p)) {
                        player.camera.position = p;
                        player.velocity = {0, 0, 0};
                        break;
                    }
                }
            }
        }

        // Tab or Escape: toggle cursor / exit play mode
        {
            bool tabDown = glfwGetKey(window, GLFW_KEY_TAB)   == GLFW_PRESS;
            bool escDown = glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS;
            if (ui.playMode) {
                if (escDown || (tabDown && !tabWasDown)) {
                    ui.playMode   = false;
                    mouseCaptured = false;
                    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
                    firstFrame = true;
                }
            } else {
                bool toggled = (tabDown && !tabWasDown) || (escDown && mouseCaptured);
                if (toggled) {
                    mouseCaptured = !mouseCaptured;
                    glfwSetInputMode(window, GLFW_CURSOR,
                        mouseCaptured ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
                    firstFrame = true;
                }
            }
            tabWasDown = tabDown;
        }

        // F5 save, F6 load
        bool f5 = glfwGetKey(window, GLFW_KEY_F5) == GLFW_PRESS;
        if (f5 && !f5WasDown) {
            bool worldOk   = world.save(savePath);
            bool projectOk = ui.scenePanel.saveProject(projectPath);
            ui.statusMsg = (worldOk && projectOk) ? "World saved." : "Save failed!";
        }
        f5WasDown = f5;

        bool f6 = glfwGetKey(window, GLFW_KEY_F6) == GLFW_PRESS;
        if (f6 && !f6WasDown) {
            bool worldOk   = world.load(savePath);
            bool projectOk = ui.scenePanel.loadProject(projectPath);
            ui.statusMsg = (worldOk && projectOk) ? "World loaded." : "Load failed!";
        }
        f6WasDown = f6;

        // Reload active gun
        if (glfwGetKey(window, keyBinds.key("reload")) == GLFW_PRESS && mouseCaptured) {
            Weapon* w = player.currentWeapon();
            if (auto* g = dynamic_cast<Gun*>(w)) g->reload();
        }

        // Dynamic car mesh: reload whenever the Vehicle item's meshPath changes
        if (gameCfg.vehicle) {
            auto vinfo = ui.scenePanel.getFirstVehicleInfo();
            std::string wantPath = vinfo.meshPath.empty() ? "assets/models/car.glb"
                                                          : vinfo.meshPath;
            carMeshScale = vinfo.meshScale;
            if (wantPath != lastCarMeshPath) {
                lastCarMeshPath = wantPath;
                if (vehicleMeshes.find(wantPath) == vehicleMeshes.end()) {
                    GltfModel m;
                    bool ok = m.load(wantPath);
                    fprintf(stderr, "Car mesh %s: %s\n", ok ? "loaded" : "FAILED", wantPath.c_str());
                    vehicleMeshes[wantPath] = std::move(m);
                }
            }
        }

        // Toggle third-person camera
        bool vDown = gameCfg.thirdPerson &&
                     glfwGetKey(window, keyBinds.key("third_person")) == GLFW_PRESS && mouseCaptured;
        if (vDown && !vWasDown) thirdPerson = !thirdPerson;
        vWasDown = vDown;

        // Dimension shift — every map pairs with its "_b" twin
        // (arena <-> arena_b, default <-> default_b). You keep your
        // position; the world swaps out around you.
        {
            static bool tWasDown = false;
            bool tDown = glfwGetKey(window, keyBinds.key("dimension_shift")) == GLFW_PRESS && mouseCaptured;
            if (tDown && !tWasDown && !player.inVehicle && gameCfg.dimensionShift &&
                !(wavesCtl.active() && ui.playMode)) {   // no shifting mid-defense
                std::string cur = ui.scenePanel.currentLevelId();
                std::string next = (cur.size() > 2 && cur.compare(cur.size()-2, 2, "_b") == 0)
                                 ? cur.substr(0, cur.size()-2)
                                 : cur + "_b";
                ui.scenePanel.switchToLevel(next, world, entities);
                ui.scenePanel.getFirstLevelSettings(worldSettings);
                ui.statusMsg = "Shifted: " + cur + " -> " + next;
            }
            tWasDown = tDown;
        }

        // Enter / exit vehicle (edge doubles as the Action interact key)
        bool eDown = glfwGetKey(window, keyBinds.key("interact")) == GLFW_PRESS && mouseCaptured;
        bool interactPressed = eDown && !eWasDown;
        if (gameCfg.vehicle && eDown && !eWasDown) {
            if (player.inVehicle) {
                // Exit: step beside the car
                float rad = glm::radians(player.vehicleHeading);
                carPos    = player.camera.position - glm::vec3(0, 1.2f, 0);
                carVel    = { 0.f, 0.f, 0.f };
                player.camera.position = carPos + glm::vec3(std::cos(rad)*2.5f, 1.65f, -std::sin(rad)*2.5f);
                player.velocity   = {0,0,0};
                player.vehicleSpeed = 0.f;
                player.inVehicle  = false;
            } else {
                float dist = glm::length(player.camera.position - (carPos + glm::vec3(0,1.2f,0)));
                if (dist < 4.f) {
                    auto vinfo = ui.scenePanel.getFirstVehicleInfo();
                    player.inVehicle        = true;
                    player.vehicleHeading   = 90.f - player.camera.yaw;
                    player.lookYaw = 0.f; player.lookPitch = 0.f;  // face forward
                    carHeading              = player.vehicleHeading;
                    player.vehicleVel       = {0.f, 0.f};
                    player.vehicleYawRate   = 0.f;
                    player.vehicleSpeed     = 0.f;
                    player.vehicleMaxSpeed  = vinfo.topSpeed;
                    player.vehicleTurnRate  = vinfo.turnDegPerS;
                    player.camera.position  = carPos + glm::vec3(0, 1.2f, 0);
                    player.velocity         = {0,0,0};
                }
            }
        }
        eWasDown = eDown;

        // Mouse look (disabled in iso mode: the camera is locked)
        double curX, curY;
        glfwGetCursorPos(window, &curX, &curY);
        bool factoryMenu = gameCfg.factory && (factory.menuOpen || factory.inMenuPhase());
        if (mouseCaptured && !firstFrame && !gameCfg.isoCamera && !factoryMenu) {
            float mdx = (float)(curX-lastX), mdy = (float)(curY-lastY);
            if (player.inVehicle && !thirdPerson) {
                // Cockpit free-look: pan within the cabin, clamped ±110° yaw
                // / ±40° pitch. Car heading (set below) carries the cockpit.
                float s = playerSettings.mouse_sensitivity;
                player.lookYaw   = std::max(-110.f, std::min(110.f, player.lookYaw   + mdx * s));
                player.lookPitch = std::max(-40.f,  std::min(40.f,  player.lookPitch - mdy * s));
            } else {
                player.camera.processMouse(mdx, mdy, playerSettings.mouse_sensitivity);
            }
        }
        lastX = curX; lastY = curY;
        firstFrame = false;

        // Iso mode: lock the view angles (classic 45° yaw, 35.26° dimetric
        // pitch) so WASD is screen-relative; +/- zooms.
        static float isoZoom = 16.f;   // tighter default; =/- adjusts in-game
        if (gameCfg.isoCamera) {
            player.camera.yaw   = 45.f;
            player.camera.pitch = -35.264f;
            if (glfwGetKey(window, GLFW_KEY_EQUAL) == GLFW_PRESS)
                isoZoom = std::max(isoZoom - 20.f * dt, 8.f);
            if (glfwGetKey(window, GLFW_KEY_MINUS) == GLFW_PRESS)
                isoZoom = std::min(isoZoom + 20.f * dt, 70.f);
        }

        // Update (frozen during Iron Command's front-end menus)
        if (!(gameCfg.factory && factory.inMenuPhase()))
            player.update(window, world, entities, worldSettings, playerSettings,
                          keyBinds, dt, mouseCaptured, &props);

        // Cockpit: view = car-forward + free-look offset (car turns → view turns)
        if (player.inVehicle && !thirdPerson) {
            player.camera.yaw   = (90.f - player.vehicleHeading) + player.lookYaw;
            player.camera.pitch = player.lookPitch;
        }
        if (worldSettings.voxelWorld) {   // mesh/flat modes don't need chunks
            world.streamChunks(player.camera.position, worldSettings.render_dist,
                               worldSettings.proceduralTerrain);
            world.update();
        }
        if (raceBots.active()) raceBots.update(dt);

        // ── Iron Command: first-person build tool + factory tick ──────────
        if (gameCfg.factory && factory.active) {
            static bool bPrev=false, rPrev=false, lmbPrev=false, rmbPrev=false, ePrev=false;
            bool bNow = glfwGetKey(window, GLFW_KEY_B) == GLFW_PRESS;
            if (bNow && !bPrev) factory.toggleBuild();
            bPrev = bNow;
            for (int k = 0; k < 7; k++)
                if (glfwGetKey(window, GLFW_KEY_1 + k) == GLFW_PRESS) factory.selectType(k);
            bool rNow = glfwGetKey(window, GLFW_KEY_R) == GLFW_PRESS;
            if (factory.buildMode && rNow && !rPrev) factory.rotateGhost();
            rPrev = rNow;

            bool lmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
            bool rmb = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
            bool eNow = glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS;
            bool lmbEdge = lmb && !lmbPrev, rmbEdge = rmb && !rmbPrev;
            bool interactEdge = eNow && !ePrev;
            lmbPrev = lmb; rmbPrev = rmb; ePrev = eNow;
            // Build tool / menu / targeting all take over LMB (no weapon fire)
            player.suppressFire = factory.buildMode || factory.menuOpen || factory.targeting();
            // Menu releases the cursor so you can click recipe buttons
            // Front-end menus (mode/lobby/drop) release the cursor too
            glfwSetInputMode(window, GLFW_CURSOR,
                             (factory.menuOpen || factory.inMenuPhase()) ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED);
            // LMB places when building OR when picking a deploy point; menu
            // clicks are consumed by ImGui, so don't forward them.
            bool factoryPlace = lmbEdge && !factory.menuOpen &&
                                (factory.buildMode || factory.targeting());
            bool deleteEdge = factory.buildMode && rmbEdge;
            factory.update(dt, now, player.camera.position, player.camera.forward(),
                           factoryPlace, deleteEdge, interactEdge, &player.health);
            // Player picked a drop point → teleport them there and reveal
            if (factory.dropReady) {
                player.camera.position = { factory.dropPos.x, 21.65f, factory.dropPos.z };
                player.velocity = {0,0,0};
                factory.dropReady = false;
            }
            // Entering the walkable lobby → drop the player into the room
            if (factory.lobbyEnter) {
                glm::vec3 lc = factory.lobbyCenter();
                player.camera.position = { lc.x, 21.65f, lc.z + 9.f };
                player.camera.yaw = -90.f; player.camera.pitch = 0.f;
                player.velocity = {0,0,0};
                factory.lobbyEnter = false;
            }
            // Keep the player inside the lobby room walls
            if (factory.phase == Factory::P_LOBBY) {
                glm::vec3 lc = factory.lobbyCenter();
                player.camera.position.x = std::max(lc.x-12.5f, std::min(lc.x+12.5f, player.camera.position.x));
                player.camera.position.z = std::max(lc.z-12.5f, std::min(lc.z+12.5f, player.camera.position.z));
            }
        }

        // Kill floor: authored voxel maps end in void — falling out respawns
        if (player.camera.position.y < -20.f) {
            player.camera.position = {0.5f, startH, 0.5f};
            player.velocity        = {0, 0, 0};
            if (ui.playMode) player.health = player.maxHealth;
            ui.statusMsg = "Fell into the void.";
        }
        entities.update(dt);
        // Keep player entity position in sync with camera
        if (CharacterEntity* pe = entities.find(playerId))
            pe->position = player.camera.position - glm::vec3(0, 1.65f, 0);

        // ── Action executor (play mode only) ─────────────────────────────
        if (ui.playMode && !wasPlayMode) {
            ui.scenePanel.resetActionRuntime();
            player.health = player.maxHealth;
            wavesCtl.reset(entities);
            Turrets::get().clear();
            Bullets::get().clear();
            raceBots.reset();
        }
        wasPlayMode = ui.playMode;
        // While playing, level switches must not persist kills/door state
        ui.scenePanel.setRuntimePersist(!ui.playMode);
        if (ui.playMode) {
            ScenePanel::PlayCtx ctx;
            ctx.playerPos       = player.camera.position;
            ctx.interactPressed = interactPressed;
            ctx.world           = &world;
            ctx.entities        = &entities;
            ctx.playerHealth    = &player.health;
            ctx.playerMaxHealth = player.maxHealth;
            ui.scenePanel.updateActions(dt, ctx);

            // ── Horde Defense (levels with waves.json) ────────────────────
            if (wavesCtl.active() && interactPressed)
                wavesCtl.startNextWaveNow();
            wavesCtl.update(dt, entities, world, worldSettings.voxelWorld,
                            player.camera.position - glm::vec3(0, 1.65f, 0),
                            &player.health);
            Turrets::get().update(dt, entities);

            // ── Enemy behavior v0: chase in range, damage on contact ──────
            // (No pathfinding; horde units are driven by WaveController.)
            Bullets::get().update(dt, entities, player.camera.position, &player.health);

            // Boss state machine (telegraphed attacks + parryable volleys)
            glm::vec3 pFeet = player.camera.position - glm::vec3(0, 1.65f, 0);
            bool bossAlive = bossAI.update(dt, entities, &props, pFeet, &player.health);
            ui.bossActive = false;
            for (auto& e : entities.all())
                if (e.name == "Boss" && !e.dead) {
                    ui.bossActive     = true;
                    ui.bossHealthFrac = e.maxHealth > 0 ? e.health / e.maxHealth : 0.f;
                }

            for (auto& e : entities.all()) {
                if (e.isPlayer || e.dead) continue;
                if (e.id.rfind("horde_", 0) == 0) continue;
                if (e.name == "Boss") continue;   // driven by bossAI
                glm::vec3 feet = player.camera.position - glm::vec3(0, 1.65f, 0);

                // Gunners: kiting pigs — hold a ~9u firing range, back off
                // when crowded, close in when the player flees, and strafe.
                if (e.name == "Gunner") {
                    glm::vec2 gto = {feet.x - e.position.x, feet.z - e.position.z};
                    float gd = glm::length(gto);
                    if (gd > 22.f) continue;                    // asleep until noticed
                    glm::vec2 gdir = gd > 0.01f ? gto / gd : glm::vec2(0, 1);
                    e.facingY = std::atan2(gdir.x, gdir.y);

                    static std::map<std::string, float> gStrafe;
                    float& sf = gStrafe[e.id];
                    sf += dt;
                    const float IDEAL = 9.f, SPD = 2.8f;
                    glm::vec2 mv{0.f};
                    if      (gd < IDEAL - 1.5f) mv = -gdir;      // too close → retreat
                    else if (gd > IDEAL + 1.5f) mv =  gdir;      // too far → advance
                    mv += glm::vec2(-gdir.y, gdir.x) * std::sin(sf * 1.3f) * 0.6f;  // strafe
                    if (glm::length(mv) > 0.01f) {
                        mv = glm::normalize(mv);
                        glm::vec3 np = e.position + glm::vec3(mv.x, 0, mv.y) * SPD * dt;
                        glm::vec3 eye = np + glm::vec3(0, 1.65f, 0);
                        bool blocked = (worldSettings.voxelWorld && world.overlapsVoxel(eye))
                                       || props.overlapsPlayer(eye);
                        if (!blocked) e.position = np;
                    }

                    static std::map<std::string, float> gunnerCd;
                    float& cd = gunnerCd[e.id];
                    cd -= dt;
                    if (gd < 14.f && cd <= 0.f) {
                        Bullets::get().fire(e.position + glm::vec3(0, 1.4f, 0),
                                            feet + glm::vec3(0, 1.2f, 0), e.id);
                        cd = 2.2f;
                    }
                    continue;
                }
                glm::vec2 to   = {feet.x - e.position.x, feet.z - e.position.z};
                float     dist = glm::length(to);
                if (dist > 14.f) continue;              // out of aggro range
                if (dist > 1.2f) {
                    glm::vec2 dir = to / dist;
                    glm::vec3 np  = e.position + glm::vec3(dir.x, 0, dir.y) * 3.5f * dt;
                    glm::vec3 eye = np + glm::vec3(0, 1.65f, 0);
                    bool blocked = (worldSettings.voxelWorld && world.overlapsVoxel(eye))
                                   || props.overlapsPlayer(eye);
                    if (!blocked) e.position = np;
                    e.facingY = std::atan2(dir.x, dir.y);
                } else {
                    player.health -= 12.f * dt;
                    static float hurtCd = 0.f;
                    hurtCd -= dt;
                    if (hurtCd <= 0.f) {
                        Audio::get().play("hurt", 0.65f);
                        hurtCd = 0.7f;
                    }
                }
            }
            // Win condition: boss levels are cleared when the boss dies
            static bool bossWasAlive = false;
            if (levelHadBoss && bossWasAlive && !bossAlive && levelWinTimer <= 0.f) {
                ui.statusMsg = "THE DON IS DEAD — level cleared";
                Audio::get().play("win", 0.9f);
                levelWinTimer = 5.f;   // linger, then back to menu
            }
            bossWasAlive = bossAlive;

            if (ctx.teleported) {
                player.camera.position = ctx.teleportTo + glm::vec3(0, 1.65f, 0);
                player.velocity = {0, 0, 0};
            }
            if (ctx.levelSwitched)
                ui.scenePanel.getFirstLevelSettings(worldSettings);
            if (!ctx.statusMsg.empty()) ui.statusMsg = ctx.statusMsg;

            // Iron Command: death is a match loss (handled by the factory HUD),
            // so don't respawn there — everywhere else, respawn.
            if (player.health <= 0.f && !gameCfg.factory) {
                player.health          = player.maxHealth;
                player.camera.position = {0.5f, startH, 0.5f};
                player.velocity        = {0, 0, 0};
                ui.statusMsg = "You died — respawned.";
                Audio::get().play("death", 0.8f);
            }

            // Level-clear: count down, then drop back to the main menu
            if (levelWinTimer > 0.f) {
                levelWinTimer -= dt;
                if (levelWinTimer <= 0.f) { ui.requestMenu = true; ui.playMode = false; }
            }
        }
        ui.levelWin      = levelWinTimer;   // HUD banner

        // Car physics
        if (!gameCfg.vehicle) {
            ui.nearVehicle = false;
        } else if (player.inVehicle) {
            // Player drives — sync position back to car
            carPos     = player.camera.position - glm::vec3(0, 1.2f, 0);
            carHeading = player.vehicleHeading;
            // Boost pads: shove the car forward while over one
            if (raceBots.active() && raceBots.boostAt(carPos)) {
                float hr = glm::radians(player.vehicleHeading);
                player.vehicleVel += glm::vec2(std::sin(hr), std::cos(hr)) * 55.f * dt;
            }
        } else if (!worldSettings.voxelWorld) {
            // Car sitting idle — apply gravity so it rests on the terrain
            carVel.y -= worldSettings.gravity * dt;
            carVel.y  = std::max(carVel.y, -50.f);
            carPos.y += carVel.y * dt;
            float groundY = terrainGroundY(worldSettings, carPos.x, carPos.z) + 0.3f;
            if (carPos.y <= groundY) { carPos.y = groundY; carVel.y = 0.f; }
        }

        // Vehicle proximity + UI state
        float carDist   = glm::length(player.camera.position - (carPos + glm::vec3(0,1.2f,0)));
        ui.nearVehicle      = !player.inVehicle && carDist < 4.f;
        ui.inVehicle        = player.inVehicle;
        ui.vehicleSpeed     = player.vehicleSpeed;
        ui.vehicleMaxSpeed  = player.vehicleMaxSpeed;
        ui.vehicleHeading   = player.vehicleHeading;
        {
            // Angle between view direction and car-forward (heading was set
            // from "90 - yaw" on entry, so car-forward yaw = 90 - heading)
            float lookYaw = (90.f - player.vehicleHeading) - player.camera.yaw;
            while (lookYaw >  180.f) lookYaw -= 360.f;
            while (lookYaw < -180.f) lookYaw += 360.f;
            ui.vehicleLookYaw   = lookYaw;
            ui.vehicleLookPitch = player.camera.pitch;
        }
        ui.vehicleDrifting  = player.vehicleDrifting;
        ui.vehicleNitrous   = player.inVehicle &&
                              glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

        // Fly mode is no-clip — say so, or wall/damage bugs get reported
        {
            static bool prevFly = false;
            if (player.flyMode != prevFly) {
                prevFly = player.flyMode;
                ui.statusMsg = player.flyMode
                             ? "FLY MODE ON [G] — no collision"
                             : "Fly mode off — walking";
            }
        }

        // Crosshair hit marker: drain weapon hit flashes
        gun.hitFlash   = std::max(0.f, gun.hitFlash   - dt);
        melee.hitFlash = std::max(0.f, melee.hitFlash - dt);
        ui.hitMarker   = std::max(gun.hitFlash, melee.hitFlash);

        // Horde HUD state
        ui.hordePrestart = wavesCtl.active() && !ui.playMode;
        ui.hordeActive   = wavesCtl.active() && ui.playMode;
        ui.hordeWave     = wavesCtl.waveNumber;
        ui.hordeWaves    = wavesCtl.endless ? 0 : wavesCtl.waveCount;
        ui.hordeLeft     = wavesCtl.enemiesLeft;
        ui.hordeBase     = wavesCtl.baseHealth;
        ui.hordeBaseMax  = wavesCtl.baseMaxHealth;
        ui.hordeCountdown= wavesCtl.countdown;
        ui.hordeVictory  = wavesCtl.victory;
        ui.hordeDefeat   = wavesCtl.defeat;

        // Player HUD state
        ui.playerHealth    = player.health;
        ui.playerMaxHealth = player.maxHealth;
        ui.jetFuel         = player.jetFuel;
        ui.jetMaxFuel      = player.jetMaxFuel;
        ui.rocketCharge    = player.rocketCharge;

        // Viewport
        int winW, winH, fbW, fbH;
        glfwGetWindowSize(window, &winW, &winH);
        glfwGetFramebufferSize(window, &fbW, &fbH);
        float scale  = (float)fbW / (float)winW;
        int panelPx  = ui.playMode ? 0 : (int)(UI::PANEL_W * scale);
        int vpW      = fbW - panelPx;

        glViewport(0, 0, fbW, fbH);
        glClearColor(0.f, 0.f, 0.f, 1.f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 3D scene
        glViewport(panelPx, 0, vpW, fbH);
        float aspect = (vpW > 0 && fbH > 0) ? (float)vpW / (float)fbH : 1.f;

        // Sun orbits vertically: 0°=east horizon, 90°=noon, 180°=west, 270°=midnight
        float ang    = glm::radians(worldSettings.sun_angle);
        glm::vec3 sunDir = glm::normalize(glm::vec3(0.3f, std::sin(ang), std::cos(ang)));

        glm::mat4 view = player.camera.getView();
        glm::mat4 proj = player.camera.getProjection(aspect);

        // Locked isometric camera: orthographic, following the player from
        // a fixed diagonal. The logical eye (physics/aim) stays on the body.
        if (gameCfg.isoCamera && mouseCaptured)   // cursor stays visible for aiming
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        if (gameCfg.isoCamera) {
            glm::vec3 target = player.camera.position;
            float elev = glm::radians(35.264f);
            float hd   = isoZoom * std::cos(elev);
            glm::vec3 eye = target + glm::vec3(-0.7071f * hd,
                                               isoZoom * std::sin(elev),
                                               -0.7071f * hd);
            view = glm::lookAt(eye, target, glm::vec3(0, 1, 0));
            float halfH = isoZoom * 0.42f;
            proj = glm::ortho(-halfH * aspect, halfH * aspect,
                              -halfH, halfH, -200.f, 400.f);
        }

        // Third person: orbit the render eye behind the logical camera.
        // Physics, aim, and fire all still use player.camera.position.
        if (thirdPerson && !player.inVehicle && !gameCfg.isoCamera) {
            glm::vec3 fwd    = player.camera.forward();
            glm::vec3 target = player.camera.position;
            float     dist   = 6.f;
            if (worldSettings.voxelWorld) {
                glm::ivec3 hit, norm;   // pull in when a wall blocks the view
                if (world.raycast(target, -fwd, dist, hit, norm)) {
                    float d = glm::length((glm::vec3(hit) + glm::vec3(0.5f)) * VOXEL_SIZE - target);
                    dist = std::max(0.8f, d - 0.7f);
                }
            }
            view = glm::lookAt(target - fwd * dist, target, glm::vec3(0, 1, 0));
        }

        // Skybox first (writes at max depth, behind everything)
        skybox.render(player.camera, aspect, sunDir);

        // ── Load / cache custom level mesh if path changed ────────────────
        std::string curMeshPath(worldSettings.customMeshPath);
        if (curMeshPath != lastMeshPath) {
            lastMeshPath = curMeshPath;
            if (!curMeshPath.empty()) {
                auto it = levelMeshes.find(curMeshPath);
                if (it == levelMeshes.end()) {
                    GltfModel m;
                    if (m.load(curMeshPath))
                        levelMeshes[curMeshPath] = std::move(m);
                    else
                        fprintf(stderr, "Level mesh load failed: %s\n", curMeshPath.c_str());
                }
            }
        }

        // ── Scene geometry ────────────────────────────────────────────────
        if (worldSettings.voxelWorld) {
            shader.use();
            shader.setMat4 ("uView",       view);
            shader.setMat4 ("uProjection", proj);
            shader.setVec3 ("uSunDir",     sunDir);
            shader.setFloat("uFogDensity", worldSettings.fog_density);
            shader.setVec3 ("uCameraPos",  player.camera.position);
            world.render(shader, player.camera, worldSettings);
        } else if (!curMeshPath.empty()) {
            // Custom mesh as level terrain
            auto it = levelMeshes.find(curMeshPath);
            if (it != levelMeshes.end())
                it->second.render(proj * view, {0,0,0}, 0.f, 1.f,
                                  sunDir, worldSettings.fog_density,
                                  player.camera.position);
        } else if (worldSettings.flatTerrain) {
            if (!worldSettings.hideTerrain)   // level supplies its own floor
                flatTerrain.render(proj * view, sunDir,
                                   worldSettings.fog_density, player.camera.position);
        } else {
            terrain.render(proj * view, sunDir,
                           worldSettings.fog_density, player.camera.position);
            trees.render(proj * view, sunDir,
                         worldSettings.fog_density, player.camera.position);
        }

        // Playground props (every terrain mode)
        props.render(proj * view, sunDir, worldSettings.fog_density,
                     player.camera.position);
        Turrets::get().render(proj * view, sunDir, worldSettings.fog_density,
                              player.camera.position);
        if (gameCfg.factory && factory.active)
            factory.render(proj * view, sunDir, worldSettings.fog_density,
                           player.camera.position, now);
        // Procedural trees on the battlefield (drawn over the factory ground).
        // Cones/cylinders are single-sided, so draw them double-sided.
        if (gameCfg.factory && factory.active && factory.phase == Factory::P_PLAY) {
            glDisable(GL_CULL_FACE);
            factoryTrees.render(proj * view, sunDir, worldSettings.fog_density,
                                player.camera.position);
            glEnable(GL_CULL_FACE);
        }

        // Enemies/NPCs: shared retro human model (run cycle while active).
        // Shooters render red-tinted so their role reads at a glance.
        if (enemyModel.loaded()) {
            enemyModel.update(dt);
            if (pigGunnerModel.loaded()) pigGunnerModel.update(dt);
            for (auto& e : entities.all()) {
                if (e.isPlayer || e.dead) continue;
                float flash = e.hitFlash > 0.f ? e.hitFlash / 0.16f : 0.f;  // 0..1
                // Pigs (Gunner + Boss) use the user's animated pig model.
                if ((e.name == "Gunner" || e.name == "Boss") && pigGunnerModel.loaded()) {
                    bool  boss  = e.name == "Boss";
                    float sc    = boss ? PIG_SCALE * 1.7f : PIG_SCALE;   // the Don is big
                    float yoff  = boss ? PIG_YOFF * 1.7f  : PIG_YOFF;
                    glm::vec3 tint(1.f);
                    if (boss) {
                        float tg = bossAI.telegraph(e.id);   // flash gold on windup
                        tint = glm::mix(glm::vec3(1.f), glm::vec3(2.4f, 1.6f, 0.6f), tg);
                    }
                    tint = glm::mix(tint, glm::vec3(3.f), flash);   // white hit flash
                    pigGunnerModel.render(proj * view,
                                          e.position + glm::vec3(0, yoff, 0),
                                          e.facingY, sc, sunDir,
                                          worldSettings.fog_density,
                                          player.camera.position, tint);
                    continue;
                }
                glm::vec3 tint  = (e.name == "Shooter") ? glm::vec3(1.6f, 0.45f, 0.45f)
                                                        : glm::vec3(1.f);
                tint = glm::mix(tint, glm::vec3(3.f), flash);
                enemyModel.render(proj * view, e.position, e.facingY, 1.f,
                                  sunDir, worldSettings.fog_density,
                                  player.camera.position, tint);
            }
        }

        // Shooter tracers (bright red beams, fade fast)
        for (auto& b : wavesCtl.beams)
            lines.drawLine(b.a, b.b, proj * view,
                           {1.f, 0.25f, 0.2f, std::min(1.f, b.ttl / 0.12f)});

        // Delayed bullets (hang phase glows yellow — that's the parry window)
        Bullets::get().render(lines, proj * view);
        Bullets::get().renderModels(bulletModel, proj * view, sunDir,
                                    worldSettings.fog_density, player.camera.position);

        // Door-glue: prop groups mirror their linked item's active state
        for (auto& g : props.groups())
            props.setGroupActive(g, ui.scenePanel.isItemActive(g));

        // First-person car cockpit: interior locked to the camera so the
        // eye marker sits at the view point and the dashboard faces forward.
        // Depth-cleared so it draws over the world; exterior car still shows
        // to other viewpoints (third person / other players).
        if (player.inVehicle && !thirdPerson && carInterior.loaded()) {
            glClear(GL_DEPTH_BUFFER_BIT);
            // Lock to CAR forward (not the view), so free-look pans across a
            // cockpit that's fixed to the car and turns only when the car does.
            float cfr = glm::radians(90.f - player.vehicleHeading);
            glm::vec3 cf = {std::cos(cfr), 0, std::sin(cfr)};
            float yaw = std::atan2(-cf.x, -cf.z);
            glm::mat4 m = glm::translate(glm::mat4(1.f), player.camera.position)
                        * glm::rotate(glm::mat4(1.f), yaw, glm::vec3(0,1,0))
                        * glm::translate(glm::mat4(1.f), -INT_EYE);
            glDisable(GL_CULL_FACE);       // we're inside the shell — draw both sides
            carInterior.renderMatrix(proj * view, m, sunDir,
                                     worldSettings.fog_density, player.camera.position);
            glEnable(GL_CULL_FACE);
        }

        // First-person gun viewmodel: bottom-right, aimed with the camera.
        // Depth is cleared so the gun never clips into world geometry.
        // Right-side pistol viewmodel whenever a Gun is the held weapon.
        if (!thirdPerson && !player.inVehicle && !gameCfg.isoCamera && gunModel.loaded() &&
            dynamic_cast<Gun*>(player.currentWeapon()) != nullptr) {
            glClear(GL_DEPTH_BUFFER_BIT);
            glm::vec3 f = player.camera.forward();
            glm::vec3 r = player.camera.right();
            glm::vec3 u = player.camera.up();
            glm::vec3 gp = player.camera.position + f*0.55f + r*0.26f - u*0.24f;
            float s = 0.6f;
            glm::mat4 gm(glm::vec4(r*s, 0), glm::vec4(u*s, 0),
                         glm::vec4(-f*s, 0), glm::vec4(gp, 1));  // barrel is -Z
            gunModel.renderMatrix(proj * view, gm, sunDir,
                                  worldSettings.fog_density, player.camera.position);
        }

        // Iron Command: animated wizard hand as the first-person build tool
        // (left side of the view). Build-menu open/loop/close drives it:
        //   open  → play 0..frame70 once, then loop 22..70 while building;
        //   close → play frame70..end once, then rest.
        if (gameCfg.factory && !thirdPerson && wizardHand.loaded()) {
            const float T22 = 22.f/24.f, T70 = 70.f/24.f;
            float Tend = wizardHand.duration();
            static int  handPhase = -1;      // -1 uninit, 0 rest,1 open,2 loop,3 close
            static bool bmPrev = false;
            bool bm = factory.buildMode && factory.phase == Factory::P_PLAY;
            if (handPhase < 0) { wizardHand.holdAt(0.f); handPhase = 0; }
            if (bm && !bmPrev) { handPhase = 1; wizardHand.playSegment(0.f, T70, false); }
            if (!bm && bmPrev) { handPhase = 3; wizardHand.playSegment(T70, Tend, false); }
            bmPrev = bm;
            if (handPhase == 1 && wizardHand.segmentDone()) {
                handPhase = 2; wizardHand.playSegment(T22, T70, true);
            }
            if (handPhase == 3 && wizardHand.segmentDone()) {
                handPhase = 0; wizardHand.holdAt(Tend);
            }
            wizardHand.update(dt);
            glClear(GL_DEPTH_BUFFER_BIT);
            glm::vec3 f = player.camera.forward();
            glm::vec3 r = player.camera.right();
            glm::vec3 u = player.camera.up();
            glm::vec3 hp = player.camera.position + f*0.75f - r*0.34f - u*0.42f;
            float s = 0.09f;   // model is ~12u; shrink to hand size
            // Longest axis (model +X) points forward, out of the screen.
            glm::mat4 hm(glm::vec4(-f*s,0), glm::vec4(u*s,0), glm::vec4(r*s,0), glm::vec4(hp,1));
            wizardHand.renderMatrix(proj * view, hm, sunDir,
                                    worldSettings.fog_density, player.camera.position);
        }

        // Animated player avatar (visible in third person only)
        if (playerModel.loaded()) {
            bool moving = glm::length(glm::vec2(player.velocity.x, player.velocity.z)) > 0.5f;
            playerModel.play(moving ? "Run" : "Idle");
            playerModel.update(dt);
            if ((thirdPerson || gameCfg.isoCamera) && !player.inVehicle) {
                // Character rotates smoothly: faces movement while moving,
                // else the aim cursor (iso), else the camera.
                static float charYaw = 0.f;
                glm::vec2 hv = {player.velocity.x, player.velocity.z};
                float target;
                if (glm::length(hv) > 0.5f)
                    target = std::atan2(hv.x, hv.y);
                else if (melee.aimOverrideOn)
                    target = std::atan2(melee.aimOverride.x, melee.aimOverride.z);
                else {
                    glm::vec3 fwd = player.camera.forward();
                    target = std::atan2(fwd.x, fwd.z);
                }
                float d = target - charYaw;
                while (d >  3.14159265f) d -= 6.2831853f;
                while (d < -3.14159265f) d += 6.2831853f;
                float step = 9.f * dt;             // constant turn rate
                charYaw += glm::clamp(d, -step, step);
                playerModel.render(proj * view,
                                   player.camera.position - glm::vec3(0, 1.65f, 0),
                                   charYaw, 1.f, sunDir, worldSettings.fog_density,
                                   player.camera.position);
            }
        }

        // Car — render from the dynamic mesh cache; hidden while driving (first-person)
        if (gameCfg.vehicle && !player.inVehicle &&
            !lastCarMeshPath.empty() && lastCarMeshPath != "\x01") {
            auto it = vehicleMeshes.find(lastCarMeshPath);
            if (it != vehicleMeshes.end() && it->second.loaded())
                it->second.render(proj * view, carPos, glm::radians(carHeading), carMeshScale,
                                  sunDir, worldSettings.fog_density,
                                  player.camera.position);
        }

        // AI race bots — driven by the shared car mesh
        if (raceBots.active() && !lastCarMeshPath.empty() && lastCarMeshPath != "\x01") {
            auto it = vehicleMeshes.find(lastCarMeshPath);
            if (it != vehicleMeshes.end() && it->second.loaded())
                raceBots.render(it->second, proj * view, sunDir,
                                worldSettings.fog_density, player.camera.position);
        }

        // ── Iso aim cursor: mouse → ground point → melee swing direction ──
        if (gameCfg.isoCamera && mouseCaptured) {
            double mx, my;
            glfwGetCursorPos(window, &mx, &my);
            int panelW = ui.playMode ? 0 : UI::PANEL_W;
            float ndcX = 2.f * (float)((mx - panelW) / std::max(winW - panelW, 1)) - 1.f;
            float ndcY = 1.f - 2.f * (float)(my / std::max(winH, 1));
            glm::mat4 inv = glm::inverse(proj * view);
            glm::vec4 p0 = inv * glm::vec4(ndcX, ndcY, -1.f, 1.f); p0 /= p0.w;
            glm::vec4 p1 = inv * glm::vec4(ndcX, ndcY,  1.f, 1.f); p1 /= p1.w;
            glm::vec3 rd = glm::normalize(glm::vec3(p1 - p0));
            if (std::abs(rd.y) > 1e-4f) {
                float t = (20.f - p0.y) / rd.y;   // ground plane
                if (t > 0.f) {
                    glm::vec3 aim  = glm::vec3(p0) + rd * t;
                    glm::vec3 feet = player.camera.position - glm::vec3(0, 1.65f, 0);
                    glm::vec2 ad   = {aim.x - feet.x, aim.z - feet.z};
                    if (glm::length(ad) > 0.05f) {
                        melee.aimOverrideOn = true;
                        melee.aimOverride   = glm::normalize(glm::vec3(ad.x, 0, ad.y));
                    }
                    // Ground cursor + direction tick from the robot's feet
                    lines.drawBox(aim - glm::vec3(0.22f, -0.02f, 0.22f),
                                  aim + glm::vec3(0.22f, 0.06f, 0.22f),
                                  proj * view, {1.f, 0.85f, 0.3f, 0.9f});
                    glm::vec3 tip = feet + melee.aimOverride * 1.1f + glm::vec3(0, 0.05f, 0);
                    lines.drawLine(feet + glm::vec3(0, 0.05f, 0), tip,
                                   proj * view, {1.f, 0.85f, 0.3f, 0.8f});
                }
            }
        } else {
            melee.aimOverrideOn = false;
        }

        // Sword visual: held at the side; sweeps a 160° arc through the aim
        // direction while a swing is active.
        if (swordModel.loaded() && player.currentWeapon() == &melee && !player.inVehicle) {
            glm::vec3 feet = player.camera.position - glm::vec3(0, 1.65f, 0);
            glm::vec3 dir  = melee.aimOverrideOn
                           ? melee.aimOverride
                           : glm::normalize(glm::vec3(player.camera.forward().x, 0,
                                                       player.camera.forward().z));
            float baseAng = std::atan2(dir.x, dir.z);
            float ang     = baseAng - glm::radians(35.f);           // resting pose
            if (melee.swingT > 0.f) {
                float k = 1.f - melee.swingT / 0.25f;               // 0 → 1
                ang = baseAng + glm::radians(-80.f + 160.f * k);    // sweep
            }
            glm::vec3 hand = feet + glm::vec3(std::sin(ang), 0, std::cos(ang)) * 0.55f
                           + glm::vec3(0, 1.05f, 0);
            glm::mat4 sm = glm::translate(glm::mat4(1.f), hand)
                         * glm::rotate(glm::mat4(1.f), ang, glm::vec3(0, 1, 0))
                         * glm::rotate(glm::mat4(1.f), glm::radians(-8.f), glm::vec3(1, 0, 0))
                         * glm::scale(glm::mat4(1.f), glm::vec3(0.7f));
            swordModel.renderMatrix(proj * view, sm, sunDir,
                                    worldSettings.fog_density, player.camera.position);
        }
        melee.swingT = std::max(0.f, melee.swingT - dt);

        // Spawn markers: wireframe box at each SpawnItem position
        {
            auto markers = ui.scenePanel.getSpawnMarkers();
            glm::vec4 markerCol = {1.f, 0.8f, 0.1f, 0.9f};
            for (auto& m : markers) {
                // Small cube centred on the spawn point
                lines.drawBox(m - glm::vec3(0.3f), m + glm::vec3(0.3f), proj*view, markerCol);
                // Tall vertical pillar so it's visible from a distance
                lines.drawBox(m + glm::vec3(-0.05f, 0.3f, -0.05f),
                              m + glm::vec3( 0.05f, 2.5f,  0.05f), proj*view, markerCol);
            }
        }

        // Aim highlight
        if (worldSettings.voxelWorld && player.aimValid && mouseCaptured) {
            glm::vec3 bmin = glm::vec3(player.aimHit) * VOXEL_SIZE - glm::vec3(0.004f);
            glm::vec3 bmax = glm::vec3(player.aimHit + glm::ivec3(1)) * VOXEL_SIZE + glm::vec3(0.004f);
            lines.drawBox(bmin, bmax, proj * view, {1,1,1,0.8f});
        }

        // UI (full window)
        glViewport(0, 0, fbW, fbH);
        bool fHeld = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS && mouseCaptured;
        int prevSlot = player.activeSlot;
        int newSlot = ui.render(worldSettings, playerSettings, player.flyMode,
                                player.weapons, player.activeSlot, world,
                                character, charRenderer, entities,
                                savePath, window, mouseCaptured, fHeld);
        if (newSlot >= 0) player.activeSlot = newSlot;
        if (player.activeSlot != prevSlot)
            Audio::get().play("switch", 0.45f);

        // Entering play mode: grab cursor immediately
        if (ui.playMode && !mouseCaptured) {
            mouseCaptured = true;
            glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
            firstFrame = true;
        }

        glfwSwapBuffers(window);
    }

    // Auto-save on quit
    world.save(savePath);
    ui.scenePanel.saveProject(projectPath);
    // Persist player character before saving
    if (CharacterEntity* pe = entities.find(playerId))
        pe->character = character;
    entities.save(entitySavePath);

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
