#pragma once
#include <cstdint>

struct WorldSettings {
    bool  voxelWorld    = false;  // false = mesh terrain
    bool  flatTerrain   = true;   // flat checker-grid floor (overrides noise when !voxelWorld)
    bool  hideTerrain   = false;  // don't draw the base ground — the level's
                                  // own floor props are the visual ground
                                  // (flat y=20 collision plane stays active)
    bool  proceduralTerrain = true; // voxel mode: false = authored map ends in void
    char  customMeshPath[512] = ""; // .glb path; rendered as level mesh when non-empty
    float gravity       = 9.8f;
    float wind_x        = 0.0f;
    float wind_z        = 0.0f;
    int   render_dist   = 8;
    float fog_density   = 0.003f;
    float sun_angle     = 45.0f;
};

struct PlayerSettings {
    float move_speed        = 7.0f;
    float fly_speed         = 12.0f;
    float mouse_sensitivity = 0.12f;
    float sprint_multiplier = 1.15f;
    float jump_force        = 8.0f;
};

struct SpraySettings {
    int     spray_count   = 12;
    float   spread_angle  = 15.0f;
    float   spray_range   = 25.0f;
    uint8_t voxel_type    = 2;
    float   spray_rate    = 0.12f;
};

static const char* VOXEL_NAMES[] = {
    "Air", "Dirt", "Grass", "Stone", "Water",
    "Sand", "Wood", "Leaves", "Snow", "Gravel", "Red Clay"
};
static const int VOXEL_TYPE_COUNT = 10;
