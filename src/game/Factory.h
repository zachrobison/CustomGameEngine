#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include "../net/Net.h"
#include <glm/glm.hpp>
#include <vector>
#include <string>

class Props;

// Iron Command — first-person Satisfactory-style factory. Place machines on a
// grid, wire them with conveyor belts, pick each machine's recipe, and watch
// items flow through local buffers. Machines are solid (player collides) and
// can be dismantled.
class Factory {
public:
    ~Factory();
    void reset(unsigned seed);
    void setCollider(Props* p) { collider = p; }
    bool active = false;

    // Pre-match front-end: choose mode → lobby → drop location → play.
    enum Phase { P_MODE, P_LOBBY, P_DROP, P_PLAY };
    Phase phase = P_MODE;
    // Frozen ImGui screens (mode / drop map). The LOBBY is walkable, so it's
    // NOT a "menu phase" — you move around in it like the battlefield.
    bool  inMenuPhase() const { return phase == P_MODE || phase == P_DROP; }
    bool  defeated() const { return lost; }
    bool  victorious() const { return won; }
    bool  multiplayer = false;

    // ── LAN (increment 2): host/join over TCP, then a light snapshot exchange.
    // In a networked match the peer's hub stands in as the enemy base you fight,
    // and each side's robots are streamed to the other as red contacts.
    void        netHost();                 // listen on the LAN port
    void        netJoin(const char* ip);   // connect to a host by IP
    bool        netConnected() const;
    bool        netActive = false;         // a networked match is running
    std::string netStatus() const;
    char        joinIp[40] = "192.168.1.";  // editable in the mode screen

    // Your Hub — the base the enemy tries to destroy (lose it = defeat). Also
    // opens the top-down command map (like the Terminal) when you interact.
    glm::vec3 hubPosition() const { return hubPos; }
    bool  commandMapOpen() const { return cmdMapOpen; }
    // Set when the player picks a drop point; main reads it to place the player.
    bool  dropReady = false;
    glm::vec3 dropPos{0.f};
    // Walkable lobby: teleport request + war-table proximity for main/HUD.
    bool  lobbyEnter = false;
    bool  lobbyNearTable = false;
    glm::vec3 lobbyCenter() const { return {2000.f, GROUND, 0.f}; }

    bool buildMode = false;
    void toggleBuild();
    void selectType(int t);
    void cycleType(int d);
    void rotateGhost();

    // Per-frame from the FPS loop. camPos/camFwd aim the ghost/interaction;
    // placeEdge = LMB rising edge, deleteEdge = RMB rising edge (dismantle),
    // interactEdge = E rising edge (cycle recipe of the aimed machine).
    void update(float dt, double now, glm::vec3 camPos, glm::vec3 camFwd,
                bool placeEdge, bool deleteEdge, bool interactEdge,
                float* playerHp);
    void render(const glm::mat4& VP, glm::vec3 sun, float fog, glm::vec3 camPos, double now);
    void renderHud(int winW, int winH);

    // Tree positions for main to draw with the real OBJ model: x,z = pos,
    // y = radius/scale hint.
    const std::vector<glm::vec3>& treeList() const { return trees; }

    enum Item  { ORE, INGOT, SCREW, PLATE, ROD, PART, ROBOT_ITEM, ITEM_N };
    enum Tool  { MINER, SMELTER, CONSTRUCTOR, ASSEMBLER, BARRACKS, TERMINAL,
                 HUB, CONVEYOR, TOOL_N };
    static const int MTYPE_N = CONVEYOR;   // machine types (excludes conveyor)
    enum DKind { DK_ROBOT, DK_MINE, DK_FENCE, DK_TRIPWIRE, DK_TURRET };

    bool menuOpen = false;              // Satisfactory-style recipe popup
    bool targeting() const { return false; }   // (legacy 3D picker retired)

private:
    static const int CAP = 40;
    struct Machine {
        int   type, recipe;
        glm::vec3 pos;
        float yaw, prog;
        bool  onNode;
        int   in[ITEM_N];
        int   out;
        float hp = 120.f;              // structures take enemy fire
        glm::vec3 deployPt{0.f};        // where deployables go
        bool  hasDeploy = false;
        int   deployCount = 0;         // # deployed (for minefield grid layout)
    };
    // Belt path: machines[from] → pts[0] → … → machines[to]. Corners are the
    // waypoints in pts (grid-snapped). prog values are 0..1 over the whole path.
    struct Belt { int from, to, item; std::vector<glm::vec3> pts;
                  std::vector<float> prog; float feed; };
    struct Node { glm::vec3 pos; float r; };
    struct Deployable { int kind; glm::vec3 pos, goal; bool walking; float t;
                        float hp = 60.f; float fireCd = 0.f; };
    struct Enemy   { glm::vec3 pos, goal; float hp = 55.f; float fireCd = 0.f; };
    struct Tracer  { glm::vec3 a, b; float t; bool friendly; };

    std::vector<Machine>    machines;
    std::vector<Belt>       belts;
    std::vector<Node>       nodes;
    std::vector<glm::vec3>  trees;      // x,z + y=radius (obstacles/decor)
    std::vector<Deployable> deploys;
    std::vector<Enemy>      enemies;
    std::vector<Tracer>     tracers;

    // ── Opponent (an AI "player") — multiplayer-ready seam: this whole block
    // is one opponent's state; a networked opponent would drive the same
    // fields from remote input instead of the local AI economy in update().
    glm::vec3 ebasePos{0.f};
    float ebaseHp = 0.f, ebaseMax = 900.f;
    bool  ebaseAlive = false;
    float enemySpawnCd = 6.f;
    float aiEconomy = 0.f;         // grows over time → bigger, faster waves
    float peaceTime = 90.f;        // seconds before the opponent can attack
    float gameClock = 0.f;
    bool  won = false, lost = false;
    int   partsBank = 0;           // Parts = currency for upgrades
    int   fenceBank = 0;           // Fences stockpiled in machines, laid from the hub map
    // Player hub (home base) — placed at the drop point
    glm::vec3 hubPos{0.f};
    float hubHp = 0.f, hubMax = 1400.f;
    bool  hubAlive = false;
    bool  cmdMapOpen = false;      // hub-opened top-down command map

    // ── Net mirror: every other player, keyed by a random id. Each opponent's
    // hub is a hostile base you fight; their robots stream in as red contacts.
    struct Opp { unsigned id; glm::vec3 hub; float hp; bool alive;
                 std::vector<glm::vec2> units; float pendingDmg; float lastSeen; };
    Net   net;
    unsigned myId = 0;             // this player's random id (set on host/join)
    float netSendCd = 0.f;         // throttle snapshots to ~15 Hz
    std::vector<Opp> opps;
    bool  everSawOpp = false;      // for the "last hub standing" win check
    void  netTick(float dt);
    Opp*  findOpp(unsigned id);
    int   menuMachine  = -1;            // machine whose menu is open
    int   deployTarget = -1;            // (legacy) machine awaiting a 3D click
    bool  mapMode = false;             // menu is showing the deploy map
    int   pendingRecipe = -1;          // deploy recipe chosen, awaiting a map pick

    // Fog-of-war exploration grid (marked as the player walks)
    static constexpr float CELL = 4.f;
    int   gridN = 0;
    std::vector<unsigned char> explored;
    int   cellOf(float wx, float wz) const;
    Props* collider = nullptr;
    int   selType = 0;
    float ghostYaw = 0.f;
    glm::vec3 ghostPos{0.f};
    bool  ghostValid = false;
    int   pendingSrc = -1;
    std::vector<glm::vec3> beltPts;    // waypoints of the belt being drawn
    int   aimedMachine = -1;   // machine under the crosshair, in reach
    const float GROUND = 20.f;
    const float GRID   = 2.f;
    float mapHalf = 370.f;   // 2x bigger again (~10x the original was 260)

    GLuint vao = 0, vbo = 0, prog = 0;
    GLuint texFloor = 0, texMetal = 0, texBelt = 0;
    void   initCube();
    void   initTextures();
    void   cube(const glm::mat4& VP, glm::vec3 c, glm::vec3 h, glm::vec3 col,
                glm::vec3 sun, float fog, glm::vec3 cam, float alpha,
                GLuint tex, glm::vec2 uvRep = {1,1}, glm::vec2 uvOff = {0,0});
    void   drawMachine(const glm::mat4& VP, int type, glm::vec3 pos, float yaw,
                       glm::vec3 sun, float fog, glm::vec3 cam, float alpha);
    bool   rayGround(glm::vec3 ro, glm::vec3 rd, glm::vec3& out) const;
    bool   nodeAt(glm::vec3 p) const;
    int    pickMachine(glm::vec3 groundPt) const;
    int    pickBelt(glm::vec3 groundPt) const;
    int    outItem(const Machine& m) const;
    glm::vec3 footprint(int type) const;   // (hx, height, hz)
    void   eraseMachine(int k);
    void   syncCollision();
    void   totals(int inv[ITEM_N]) const;
    // Steer `dir` (unit, xz) away from machines/rocks near `pos`.
    glm::vec2 avoid(glm::vec3 pos, glm::vec2 dir) const;
};
