#version 410 core

layout(location = 0) in vec3 aPos;
layout(location = 1) in vec3 aNormal;
layout(location = 2) in float aType;

uniform mat4 uView;
uniform mat4 uProjection;

out vec3  vNormal;
out float vType;
out vec3  vWorldPos;

void main() {
    vNormal   = aNormal;
    vType     = aType;
    vWorldPos = aPos;
    gl_Position = uProjection * uView * vec4(aPos, 1.0);
}
