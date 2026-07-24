#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>
#include <string>
#include <vector>

struct GLFWwindow;

// Iron Command — real-time factory-RTS layer (C&C × Satisfactory). This is a
// self-contained mode: its own top-down camera, procedural terrain, unit
// selection/command, and a minimal solid-cube renderer so it doesn't thread
// through the FPS main loop. Milestone 1: camera, terrain, select + move.
class Rts {
public:
    ~Rts();

    // Generate a fresh match (procedural terrain, starting hub + units).
    void start(unsigned seed = 0);
    bool active() const { return started; }

    // Per-frame: camera pan/zoom, drag-select, right-click command. Uses the
    // full window (RTS runs panel-hidden, like play mode). aspect = w/h.
    void handleInput(GLFWwindow* win, float dt, float aspect);
    void update(float dt);

    glm::mat4 view() const;
    glm::mat4 proj(float aspect) const;

    // Solid scene (ground, resources, buildings, units, selection rings).
    void render(const glm::mat4& VP, glm::vec3 sunDir, float fog);
    // Screen-space overlay (drag box, HUD) via ImGui foreground draw list.
    void renderHud();

    int   metal() const { return (int)metalStore; }
    void  addScroll(float y) { scrollAccum += y; }   // from GLFW scroll cb

private:
    struct Unit {
        glm::vec3 pos{0.f};
        glm::vec2 goal{0.f};       // move target (xz)
        bool  hasGoal = false;
        bool  selected = false;
        float hp = 100.f;
        int   kind = 0;            // 0 = robot
    };
    struct Building {
        glm::vec3 pos{0.f};        // centre, y=0
        glm::vec2 size{2.f};       // footprint (world units)
        int   kind = 0;            // 0 = hub, 1 = miner, 2 = turret
        float hp = 100.f;
        float mineCd = 0.f;        // miner production timer
    };
    struct Resource { glm::vec3 pos; float amount; };  // metal node
    struct Rock     { glm::vec3 pos; float r; };       // obstacle

    // Camera (angled top-down, perspective)
    glm::vec2 camTarget{0.f};      // ground point the camera looks at
    float camDist  = 42.f;
    float camYaw   = 0.7853f;      // 45°
    float camPitch = 0.95f;        // ~54° down

    std::vector<Unit>     units;
    std::vector<Building> buildings;
    std::vector<Resource> resources;
    std::vector<Rock>     rocks;
    float metalStore = 20.f;
    bool  started = false;
    float mapHalf = 90.f;          // terrain extent

    // drag-select state (screen px, within the 3D viewport)
    bool  dragging = false;
    glm::vec2 dragStart{0.f}, dragCur{0.f};
    bool  lmbPrev = false, rmbPrev = false;
    float scrollAccum = 0.f;

    // cube renderer
    GLuint cubeVao = 0, cubeVbo = 0, prog = 0;
    void   initCube();
    void   drawCube(const glm::mat4& VP, glm::vec3 center, glm::vec3 halfExtent,
                    glm::vec3 color, glm::vec3 sunDir, float fog);

    glm::vec3 eye() const;
    // ray from window pixel (sx,sy) to the y=0 ground plane
    bool screenToGround(float sx, float sy, glm::vec3& out) const;
    glm::vec2 winSize{1280.f, 800.f};
    float     lastAspect = 1.6f;
};
