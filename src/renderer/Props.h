#pragma once
#define GL_SILENCE_DEPRECATION
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <vector>
#include <string>

// Static axis-aligned box props: playground geometry (ramps, platforms,
// towers) that renders in every terrain mode, blocks the player, and can be
// hit by the grapple raycast. Boxes only — sloped ramps are built as steps,
// which also plays nicely with the player's step-up.
class Props {
public:
    ~Props();
    Props() = default;
    Props(const Props&)            = delete;
    Props& operator=(const Props&) = delete;

    void addBox(glm::vec3 mn, glm::vec3 mx, glm::vec3 color, bool visible = true);
    // Stepped ramp rising along +dir (unit axis x or z), from base to
    // base+height over `length`.
    void addRamp(glm::vec3 base, glm::vec3 dir, float width, float length,
                 float height, glm::vec3 color);

    // Call once after all boxes are added.
    void buildMesh();

    // Per-level collision/geometry: replaces all boxes with the contents of
    // a props.json ({"boxes":[{"min":[..],"max":[..],"color":[..],
    // "visible":bool}]}). Missing file → empty set. Invisible boxes collide
    // and take grapples but aren't drawn (collision for GLB level meshes).
    bool loadFile(const std::string& path);
    void clear();

    // Door-glue: boxes with an "itemRef" in props.json belong to a group
    // driven by that item's active flag (Actions open/close them at runtime).
    // Returns true if anything changed (mesh is rebuilt internally).
    bool setGroupActive(const std::string& itemRef, bool on);
    const std::vector<std::string>& groups() const { return groupNames; }

    void render(const glm::mat4& VP, glm::vec3 sunDir,
                float fogDensity, glm::vec3 camPos);

    // Nearest ray hit against any box. Returns false if none within maxDist.
    bool raycast(glm::vec3 origin, glm::vec3 dir, float maxDist,
                 glm::vec3& outPoint) const;

    // Any box overlapping any prop?
    bool overlapsBox(glm::vec3 mn, glm::vec3 mx) const;

    // Player AABB (eye-centred, same extents as World::overlapsVoxel).
    bool overlapsPlayer(glm::vec3 eye) const;

    // Highest box top under the player's footprint at (x,z) whose top is at
    // or below maxTop — the surface to stand on / step up onto. Returns a
    // large negative number if none (fall through to terrain).
    float supportHeight(float x, float z, float maxTop) const;

private:
    struct Box {
        glm::vec3 mn, mx, color;
        bool visible = true, solid = true;
        std::string itemRef;      // "" = static; else toggled by item active
        bool groupOn = true;
    };
    std::vector<Box> boxes;
    std::vector<std::string> groupNames;

    GLuint vao = 0, vbo = 0, prog = 0;
    int    vertCount = 0;
    void   initShader();
};
