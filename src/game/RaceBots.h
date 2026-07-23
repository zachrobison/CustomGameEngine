#pragma once
#include <glm/glm.hpp>
#include <string>
#include <vector>

class GltfModel;

// AI racers: cars that follow a loop of waypoints, steering + throttling
// toward the next gate and advancing when they reach it. Rendered with the
// shared car mesh. Simple arcade steering — good enough to race against.
class RaceBots {
public:
    // Loads maps/<dir>/waypoints.json ({ "loop":[[x,z],...], "bots":N,
    // "speed":.. }). Returns false if absent (no bots this level).
    bool load(const std::string& levelDir);
    bool active() const { return !waypoints.empty(); }

    void reset();                      // put bots back on the start line
    void update(float dt);
    void render(GltfModel& carMesh, const glm::mat4& VP, glm::vec3 sunDir,
                float fog, glm::vec3 camPos);

    const std::vector<glm::vec3>& gates() const { return waypoints; }

    // True if a world position is on a boost pad (player speed boost).
    bool boostAt(glm::vec3 pos) const;

    // Start line for placing the player's car (pole position).
    glm::vec3 startPos() const { return waypoints.empty() ? glm::vec3(0) : waypoints[0]; }
    float     startHeadingDeg() const;   // in the engine's vehicleHeading frame
    float     carScale = 1.0f;

private:
    struct Bot {
        glm::vec3 pos{0.f};
        float heading = 0.f;   // radians
        float speed   = 0.f;
        int   target  = 0;     // next waypoint index
        int   lap     = 0;
    };
    std::vector<Bot>       bots;
    std::vector<glm::vec3> waypoints;   // y = ground height
    std::vector<glm::vec3> boosts;      // boost pad centers (radius ~4)
    float maxSpeed  = 22.f;
    float accel     = 14.f;
    float turnRate  = 2.4f;   // rad/s
};
