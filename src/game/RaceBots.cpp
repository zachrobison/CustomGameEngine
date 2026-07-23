#include "RaceBots.h"
#include "../renderer/GltfModel.h"
#include "../vendor/json.hpp"
#include <fstream>
#include <cmath>

using json = nlohmann::json;

bool RaceBots::load(const std::string& levelDir) {
    bots.clear(); waypoints.clear();
    std::ifstream f(levelDir + "/waypoints.json");
    if (!f) return false;
    try {
        json j; f >> j;
        float gy = j.value("groundY", 20.f);
        for (auto& p : j["loop"])
            waypoints.push_back({p[0].get<float>(), gy, p[1].get<float>()});
        maxSpeed = j.value("speed", 22.f);
        accel    = j.value("accel", 14.f);
        turnRate = j.value("turnRate", 2.4f);
        carScale = j.value("carScale", 1.0f);
        int n    = j.value("bots", 3);
        bots.resize(std::max(0, n));
        if (j.contains("boosts"))
            for (auto& p : j["boosts"])
                boosts.push_back({p[0].get<float>(), gy, p[1].get<float>()});
    } catch (...) { waypoints.clear(); return false; }
    if (waypoints.size() < 2) { waypoints.clear(); return false; }
    reset();
    return true;
}

void RaceBots::reset() {
    if (waypoints.size() < 2) return;
    glm::vec3 a = waypoints[0], b = waypoints[1];
    float startHeading = std::atan2(b.x - a.x, b.z - a.z);
    glm::vec3 side = {std::cos(startHeading), 0, -std::sin(startHeading)};
    for (int i = 0; i < (int)bots.size(); i++) {
        Bot& bt = bots[i];
        // stagger the grid across the track and back from the line
        bt.pos     = a + side * ((i % 2 == 0 ? 1.f : -1.f) * 2.2f)
                       - glm::vec3(std::sin(startHeading), 0, std::cos(startHeading)) * (2.f * (i/2));
        bt.pos.y   = a.y;
        bt.heading = startHeading;
        bt.speed   = 0.f;
        bt.target  = 1;
        bt.lap     = 0;
    }
}

void RaceBots::update(float dt) {
    for (auto& b : bots) {
        if (waypoints.empty()) continue;
        glm::vec3 tgt = waypoints[b.target];
        glm::vec2 to  = {tgt.x - b.pos.x, tgt.z - b.pos.z};
        float dist = glm::length(to);
        if (dist < 4.f) {                       // reached gate → next
            b.target++;
            if (b.target >= (int)waypoints.size()) { b.target = 0; b.lap++; }
        }
        // Steer toward target: rotate heading toward the bearing, capped
        float want = std::atan2(to.x, to.y);
        float d = want - b.heading;
        while (d >  3.14159265f) d -= 6.2831853f;
        while (d < -3.14159265f) d += 6.2831853f;
        float step = turnRate * dt;
        b.heading += std::max(-step, std::min(step, d));
        // Throttle: ease off in sharp turns so they don't overshoot
        float turnEase = 1.f - std::min(std::abs(d) / 1.2f, 0.6f);
        b.speed += (maxSpeed * turnEase - b.speed) * std::min(1.f, accel * dt / maxSpeed);
        b.pos += glm::vec3(std::sin(b.heading), 0, std::cos(b.heading)) * b.speed * dt;
    }
}

void RaceBots::render(GltfModel& carMesh, const glm::mat4& VP, glm::vec3 sunDir,
                      float fog, glm::vec3 camPos) {
    if (!carMesh.loaded()) return;
    for (auto& b : bots)
        carMesh.render(VP, b.pos, b.heading, carScale, sunDir, fog, camPos);
}

float RaceBots::startHeadingDeg() const {
    if (waypoints.size() < 2) return 0.f;
    glm::vec3 a = waypoints[0], b = waypoints[1];
    // vehicleHeading frame: heading = 90 - cameraYaw, and camera faces
    // (cos,sin) of yaw. Face down the track toward waypoint 1.
    float dirAng = std::atan2(b.z - a.z, b.x - a.x);      // world angle of forward
    return 90.f - glm::degrees(dirAng);
}

bool RaceBots::boostAt(glm::vec3 pos) const {
    for (auto& p : boosts) {
        float dx = pos.x - p.x, dz = pos.z - p.z;
        if (dx*dx + dz*dz < 16.f) return true;   // ~4u radius
    }
    return false;
}
