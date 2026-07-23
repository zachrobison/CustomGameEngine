#include "Player.h"
#include "Weapon.h"
#include "../voxel/World.h"
#include "../voxel/Noise.h"
#include "../character/EntityManager.h"
#include "../renderer/Props.h"
#include "../platform/KeyBinds.h"
#include "../platform/Audio.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <cmath>

// Returns the mesh-terrain surface Y for a world (x,z) position.
// Must match Terrain::generate(): N=256, cell=4.0
static float meshTerrainY(float wx, float wz) {
    float ix = wx / 4.f + 128.f;
    float iz = wz / 4.f + 128.f;
    return Noise::fractal(ix * 0.025f, iz * 0.025f, 4) * 28.f + 20.f;
}

Weapon* Player::currentWeapon() const {
    if (weapons.empty()) return nullptr;
    return weapons[activeSlot % (int)weapons.size()];
}

void Player::update(GLFWwindow* window, World& world, EntityManager& entities,
                    const WorldSettings& ws, const PlayerSettings& ps,
                    const KeyBinds& binds,
                    float dt, bool mouseCaptured, const Props* props) {
    // Ability timers tick every frame regardless of mode
    if (dashCooldown   > 0.f) dashCooldown   -= dt;
    if (rocketCooldown > 0.f) rocketCooldown -= dt;
    if (noControlTimer > 0.f) noControlTimer -= dt;

    // Car engine loop: pitch rises with speed; silent outside the car
    Audio::get().loop("engine", "engine_loop", inVehicle, 0.8f,
                      0.6f + std::abs(vehicleSpeed) / std::max(vehicleMaxSpeed, 1.f) * 0.9f);
    // Jetpack/grapple loops are owned by the walk branch; kill them elsewhere
    if (inVehicle || flyMode) {
        Audio::get().loop("jetpack", "jetpack_loop", false);
        Audio::get().loop("grapple", "grapple_loop", false);
    }

    // Weapon slot: keys 1–9 (skip while weapon wheel is open — F held)
    bool fHeld = glfwGetKey(window, GLFW_KEY_F) == GLFW_PRESS;
    if (!fHeld) {
        for (int i = 0; i < (int)weapons.size() && i < 9; i++) {
            if (glfwGetKey(window, GLFW_KEY_1 + i) == GLFW_PRESS)
                activeSlot = i;
        }
    }

    // Toggle fly mode
    static bool gWasDown = false;
    bool gDown = glfwGetKey(window, binds.key("fly_toggle")) == GLFW_PRESS;
    if (gDown && !gWasDown) flyMode = !flyMode;
    gWasDown = gDown;

    // Always update aim raycast so the highlight renders
    aimValid = world.raycast(camera.position, camera.forward(), 50.f, aimHit, aimNorm);

    if (!mouseCaptured) {
        Audio::get().loop("jetpack", "jetpack_loop", false);
        Audio::get().loop("grapple", "grapple_loop", false);
        return;
    }

    // ── Movement input ────────────────────────────────────────────────────
    glm::vec3 fwdH  = glm::normalize(glm::vec3(camera.forward().x, 0, camera.forward().z));
    glm::vec3 rightH = glm::normalize(glm::vec3(camera.right().x,   0, camera.right().z));

    float speed = flyMode ? ps.fly_speed : ps.move_speed;
    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS)
        speed *= ps.sprint_multiplier;

    glm::vec3 horiz = {0,0,0};
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) horiz += fwdH;
    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) horiz -= fwdH;
    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) horiz -= rightH;
    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) horiz += rightH;
    if (glm::length(horiz) > 0.001f) horiz = glm::normalize(horiz) * speed;

    if (inVehicle) {
        // ── NFS Heat-style arcade physics ─────────────────────────────────
        bool wDown   = glfwGetKey(window, GLFW_KEY_W)             == GLFW_PRESS;
        bool sDown   = glfwGetKey(window, GLFW_KEY_S)             == GLFW_PRESS;
        bool aDown   = glfwGetKey(window, GLFW_KEY_A)             == GLFW_PRESS;
        bool dDown   = glfwGetKey(window, GLFW_KEY_D)             == GLFW_PRESS;
        bool ebrake  = glfwGetKey(window, GLFW_KEY_SPACE)         == GLFW_PRESS;
        bool nitrous = glfwGetKey(window, GLFW_KEY_LEFT_SHIFT)    == GLFW_PRESS;

        float steer    = (aDown ? 1.f : 0.f) - (dDown ? 1.f : 0.f); // inverted steering
        float throttle = wDown ? 1.f : 0.f;
        float brake    = sDown ? 1.f : 0.f;

        // Car axes from current heading
        float hRad     = glm::radians(vehicleHeading);
        glm::vec2 carFwd   = {  std::sin(hRad),  std::cos(hRad) };
        glm::vec2 carRight = {  std::cos(hRad), -std::sin(hRad) };

        float speed   = glm::length(vehicleVel);
        float fwdSpd  = glm::dot(vehicleVel, carFwd);    // positive = moving forward
        float latSpd  = glm::dot(vehicleVel, carRight);  // positive = sliding right

        // ── Longitudinal forces (RWD feel) ────────────────────────────────
        float topSpeed = vehicleMaxSpeed * (nitrous ? 1.45f : 1.f);
        // Aggressive punch off the line, tapers near top speed
        float speedRatio = std::min(speed / topSpeed, 1.f);
        float driveForce = throttle * vehicleMaxSpeed * 5.5f * (1.f - speedRatio * 0.6f);
        vehicleVel += carFwd * driveForce * dt;

        // Foot brake while rolling forward; from (near) standstill S reverses
        if (brake > 0.f) {
            if (fwdSpd > 0.5f) {
                float brakeMag = vehicleMaxSpeed * 7.f * brake;
                glm::vec2 brakeDir = -glm::normalize(vehicleVel);
                vehicleVel += brakeDir * std::min(brakeMag * dt, speed);
            } else {
                float revTop = vehicleMaxSpeed * 0.35f;   // reverse gear is slow
                if (-fwdSpd < revTop)
                    vehicleVel -= carFwd * vehicleMaxSpeed * 3.f * dt;
            }
        }

        // Hard top-speed cap
        if (speed > topSpeed)
            vehicleVel = glm::normalize(vehicleVel) * topSpeed;

        // Coast drag (rolling resistance + air)
        float drag = ebrake ? 0.94f : (throttle > 0 ? 0.995f : 0.97f);
        vehicleVel *= std::pow(drag, dt * 60.f);

        // ── Lateral grip (the core of drift vs grip) ──────────────────────
        // Normal: high grip snaps tyres to heading.
        // Handbrake: rear loses grip → slide initiates.
        // Throttle at high slip angle increases yaw (RWD oversteer).
        float driftThresh = ebrake ? 0.5f : 4.f;
        vehicleDrifting   = std::abs(latSpd) > driftThresh;

        float gripCoeff;
        if (ebrake) {
            gripCoeff = 4.f;                        // very slidey
        } else if (vehicleDrifting) {
            // Mid-drift: moderate grip so you can sustain the slide
            gripCoeff = 9.f + std::abs(latSpd) * 0.3f;
        } else {
            gripCoeff = 28.f;                       // planted, responsive
        }
        vehicleVel -= carRight * latSpd * gripCoeff * dt;

        // RWD throttle-oversteer: throttle while sliding kicks rear further out
        if (vehicleDrifting && throttle > 0.5f)
            vehicleVel -= carRight * latSpd * (-6.f) * dt;

        // ── Steering & yaw ────────────────────────────────────────────────
        float speedFactor = std::min(speed / 5.f, 1.f);   // dead zone at standstill
        // Steering response tightens at speed (speed-sensitive rack)
        float steerMult = vehicleTurnRate * (1.f - speedRatio * 0.35f);
        float targetYaw = steer * steerMult * speedFactor * (ebrake ? 1.6f : 1.f);
        if (fwdSpd < -0.5f) targetYaw = -targetYaw;   // reversing: nose swings opposite
        // Smooth yaw (momentum in the steering)
        float yawResp   = vehicleDrifting ? 4.f : 10.f;
        vehicleYawRate += (targetYaw - vehicleYawRate) * yawResp * dt;

        // Physics yaw from sideslip: rear sliding right → car rotates right
        float driftYaw = latSpd * (vehicleDrifting ? 3.5f : 1.5f);
        float headingDelta = (vehicleYawRate + driftYaw) * dt;
        vehicleHeading += headingDelta;
        // Camera yaw is set explicitly in main from (car forward + lookYaw),
        // so the cockpit turns with the car while free-look pans within it.

        // ── Signed forward speed for speedometer ──────────────────────────
        vehicleSpeed = glm::dot(vehicleVel, carFwd);

        // ── 3-D position update ────────────────────────────────────────────
        glm::vec3 pos = camera.position;

        // Car AABB vs prop walls, per axis. Square footprint covers any
        // heading; hitting a wall kills most speed (small rebound).
        auto carBlocked = [&](glm::vec3 seat) {
            return props && props->overlapsBox(seat + glm::vec3(-1.5f, -1.0f, -1.5f),
                                               seat + glm::vec3( 1.5f,  0.3f,  1.5f));
        };
        glm::vec3 tryX = pos; tryX.x += vehicleVel.x * dt;
        if (!carBlocked(tryX)) pos.x = tryX.x;
        else {
            if (std::abs(vehicleVel.x) > 6.f) Audio::get().play("crash", 0.7f);
            vehicleVel.x *= -0.2f;
        }

        glm::vec3 tryZ = pos; tryZ.z += vehicleVel.y * dt;
        if (!carBlocked(tryZ)) pos.z = tryZ.z;
        else {
            if (std::abs(vehicleVel.y) > 6.f) Audio::get().play("crash", 0.7f);
            vehicleVel.y *= -0.2f;
        }

        velocity.y -= ws.gravity * dt;
        velocity.y  = std::max(velocity.y, -50.f);
        pos.y      += velocity.y * dt;

        if (!ws.voxelWorld) {
            float groundY = (ws.flatTerrain || ws.customMeshPath[0] != '\0')
                          ? 20.f : meshTerrainY(pos.x, pos.z);
            float seatY = groundY + 1.2f;
            if (pos.y <= seatY) { pos.y = seatY; velocity.y = 0.f; onGround = true; }
            else onGround = false;
        }
        camera.position = pos;
        return;
    } else if (flyMode) {
        // ── Fly mode: free vertical, no gravity ──────────────────────────
        float vy = 0;
        if (glfwGetKey(window, GLFW_KEY_SPACE)        == GLFW_PRESS) vy =  speed;
        if (glfwGetKey(window, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vy = -speed;
        camera.position += glm::vec3(horiz.x, vy, horiz.z) * dt;
    } else {
        // ── Walk mode: gravity + collision ────────────────────────────────
        bool spaceHeld = glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS;

        if (spaceHeld && onGround) {
            velocity.y = ps.jump_force;
            onGround   = false;
        }

        // ── Abilities ─────────────────────────────────────────────────────
        // Dash: 5u burst over 0.15s in horizontal look direction
        static bool qWas = false;
        bool qDown = allowDash && glfwGetKey(window, binds.key("dash")) == GLFW_PRESS;
        if (qDown && !qWas && dashCooldown <= 0.f && dashTimer <= 0.f) {
            glm::vec3 f = camera.forward();
            f.y = 0.f;
            if (glm::length(f) > 0.01f) {
                dashDir      = glm::normalize(f);
                dashTimer    = 0.15f;
                dashCooldown = 0.8f;
                Audio::get().play("dash", 0.5f, 1.2f);
            }
        }
        qWas = qDown;

        // Grapple [C, hold]: hook voxels or prop boxes up to 20u away.
        // Voxel raycast only in voxel mode — hidden voxel terrain still
        // exists under flat/mesh rendering and must not catch the hook.
        static bool cWas = false;
        bool cDown = allowGrapple && glfwGetKey(window, binds.key("grapple")) == GLFW_PRESS;
        if (cDown && !cWas && !grappling) {
            bool  hooked = false;
            float bestD  = 20.f;
            glm::vec3 pt;
            if (ws.voxelWorld) {
                glm::ivec3 hit, norm;
                if (world.raycast(camera.position, camera.forward(), bestD, hit, norm)) {
                    pt     = (glm::vec3(hit) + glm::vec3(0.5f)) * VOXEL_SIZE;
                    bestD  = glm::length(pt - camera.position);
                    hooked = true;
                }
            }
            glm::vec3 propPt;
            if (props && props->raycast(camera.position, camera.forward(), bestD, propPt)) {
                pt     = propPt;
                hooked = true;
            }
            if (hooked) {
                grappling = true;
                grapplePoint = pt;
                Audio::get().play("grapple", 0.6f);
            }
        }
        if (!cDown) grappling = false;
        cWas = cDown;
        // Reel whir while the line is pulling you
        Audio::get().loop("grapple", "grapple_loop", grappling, 0.4f, 1.6f);

        // Rocket: hold to charge (0–2s), release to launch in look dir
        bool xDown = allowRocket && glfwGetKey(window, binds.key("rocket")) == GLFW_PRESS;
        if (xDown && rocketCharge < 0.f && rocketCooldown <= 0.f)
            rocketCharge = 0.f;
        else if (xDown && rocketCharge >= 0.f)
            rocketCharge = std::min(rocketCharge + dt, 2.f);
        else if (!xDown && rocketCharge >= 0.f) {
            float spd = 20.f + (rocketCharge / 2.f) * 30.f; // 20–50 u/s
            Audio::get().play("rocket", 0.55f + rocketCharge * 0.2f);
            Audio::get().play("explosion", 0.35f + rocketCharge * 0.25f);
            velocity       = camera.forward() * spd;
            rocketCharge   = -1.f;
            rocketCooldown = 3.f;
            noControlTimer = 0.5f;
            onGround       = false;
        }

        // Jetpack: hold Space while airborne — kill fall, thrust upward
        bool jetActive = allowJetpack && spaceHeld && !onGround && jetFuel > 0.f;
        if (jetActive) {
            if (velocity.y < 0.f) velocity.y = 0.f;
            velocity.y = std::min(velocity.y + 20.f * dt, 10.f);
            jetFuel    = std::max(jetFuel - 15.f * dt, 0.f);
        }
        if (onGround)
            jetFuel = std::min(jetFuel + 25.f * dt, jetMaxFuel);
        Audio::get().loop("jetpack", "jetpack_loop", jetActive, 0.45f);

        // Footsteps while running on the ground
        {
            static float stepT = 0.f;
            float hSpd = glm::length(glm::vec2(velocity.x, velocity.z));
            if (onGround && hSpd > 3.f) {
                stepT -= dt;
                if (stepT <= 0.f) {
                    char nm[8]; snprintf(nm, sizeof(nm), "step%d", 1 + rand() % 4);
                    Audio::get().play(nm, 0.5f, 0.95f + (rand() % 100) * 0.001f);
                    stepT = 3.4f / hSpd;   // cadence scales with speed
                }
            } else {
                stepT = 0.05f;             // step immediately on next stride
            }
        }

        // ── Integrate velocity ────────────────────────────────────────────
        // While an ability owns the velocity, input doesn't overwrite it.
        bool momentum = dashTimer > 0.f || noControlTimer > 0.f || grappling;

        if (dashTimer > 0.f) {
            dashTimer -= dt;
            velocity.x = dashDir.x * 33.3f;   // 5u / 0.15s
            velocity.z = dashDir.z * 33.3f;
            velocity.y = 0.f;                 // gravity suspended mid-dash
        } else if (grappling) {
            glm::vec3 toHook = grapplePoint - camera.position;
            float     dist   = glm::length(toHook);
            if (dist < 1.5f) {
                grappling = false;
                velocity *= 0.3f;             // arrive gently
            } else {
                velocity += (toHook / dist) * 60.f * dt;  // reel accel, no gravity
                float spd = glm::length(velocity);
                if (spd > 25.f) velocity *= 25.f / spd;
            }
        } else {
            velocity.y -= ws.gravity * dt;
            velocity.y  = std::max(velocity.y, -50.f);
        }

        if (!momentum) {
            velocity.x = horiz.x;
            velocity.z = horiz.z;
        }

        glm::vec3 pos = camera.position;

        // Solid test = voxels (voxel mode only) + prop boxes (all modes)
        auto solid = [&](glm::vec3 eye) {
            if (ws.voxelWorld && world.overlapsVoxel(eye)) return true;
            return props && props->overlapsPlayer(eye);
        };
        // Step-up: while grounded, low obstacles (~one ramp step) are climbed
        auto tryStepUp = [&](glm::vec3 want) -> bool {
            if (!onGround) return false;
            glm::vec3 up = want + glm::vec3(0, 0.55f, 0);
            if (solid(up)) return false;
            pos = up;
            return true;
        };

        // Per-axis AABB sweep
        glm::vec3 tryX = pos; tryX.x += velocity.x * dt;
        if (!solid(tryX)) pos.x = tryX.x;
        else if (!tryStepUp(tryX)) velocity.x = 0;

        glm::vec3 tryY = pos; tryY.y += velocity.y * dt;
        if (!solid(tryY)) {
            pos.y    = tryY.y;
            onGround = false;
        } else {
            if (velocity.y < 0) onGround = true;
            velocity.y = 0;
        }

        glm::vec3 tryZ = pos; tryZ.z += velocity.z * dt;
        if (!solid(tryZ)) pos.z = tryZ.z;
        else if (!tryStepUp(tryZ)) velocity.z = 0;

        if (!ws.voxelWorld) {
            // Heightfield floor under everything (flat/custom = Y=20)
            float groundY = (ws.flatTerrain || ws.customMeshPath[0] != '\0')
                          ? 20.f
                          : meshTerrainY(pos.x, pos.z);
            float feetY = pos.y - 1.65f;
            // Stand on prop boxes: raise the ground to the highest box top
            // under us that's within stepping reach (fixes stairs/ramps).
            if (props) {
                float pt = props->supportHeight(pos.x, pos.z, feetY + 0.6f);
                if (pt > groundY) groundY = pt;
            }
            if (feetY <= groundY) {
                pos.y      = groundY + 1.65f;
                velocity.y = 0.f;
                onGround   = true;
            }
        }

        camera.position = pos;
    }

    // ── Weapon ────────────────────────────────────────────────────────────
    Weapon* w = currentWeapon();
    if (w) {
        w->tick(dt);
        if (!fHeld && !suppressFire &&
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS &&
            w->canFire()) {
            w->onFire(camera, world, &entities);
            w->startCooldown();
        }
    }
}
