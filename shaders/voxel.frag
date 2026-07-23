#version 410 core

in vec3  vNormal;
in float vType;
in vec3  vWorldPos;

uniform vec3  uSunDir;
uniform float uFogDensity;
uniform vec3  uCameraPos;

out vec4 FragColor;

vec3 voxelColor(int t) {
    if (t == 1)  return vec3(0.45, 0.35, 0.25);   // dirt
    if (t == 2)  return vec3(0.30, 0.60, 0.20);   // grass
    if (t == 3)  return vec3(0.55, 0.55, 0.55);   // stone
    if (t == 4)  return vec3(0.20, 0.40, 0.80);   // water
    if (t == 5)  return vec3(0.90, 0.85, 0.70);   // sand
    if (t == 6)  return vec3(0.40, 0.25, 0.15);   // wood
    if (t == 7)  return vec3(0.15, 0.50, 0.10);   // leaves
    if (t == 8)  return vec3(0.85, 0.88, 0.95);   // snow
    if (t == 9)  return vec3(0.50, 0.47, 0.45);   // gravel
    if (t == 10) return vec3(0.65, 0.30, 0.20);   // red clay
    return vec3(0.8, 0.2, 0.8);                   // unknown → magenta
}

void main() {
    vec3 color = voxelColor(int(vType));

    // AO-style face shading: top bright, sides mid, bottom dark
    float faceBright = 1.0;
    if      (vNormal.y < -0.5) faceBright = 0.45;
    else if (vNormal.y >  0.5) faceBright = 1.00;
    else                        faceBright = 0.72;

    // Directional sun lighting
    float diff    = max(dot(vNormal, normalize(uSunDir)), 0.0);
    float ambient = 0.32;
    float light   = ambient + (1.0 - ambient) * diff;
    color *= light * faceBright;

    // Exponential fog
    float dist = length(vWorldPos - uCameraPos);
    float fog  = 1.0 - exp(-uFogDensity * dist);
    vec3 sky   = vec3(0.55, 0.72, 0.92);
    color = mix(color, sky, clamp(fog, 0.0, 1.0));

    FragColor = vec4(color, 1.0);
}
