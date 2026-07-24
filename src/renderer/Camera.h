#pragma once
#define GL_SILENCE_DEPRECATION
#include "../gl_compat.h"
#include <glm/glm.hpp>

class Camera {
public:
    glm::vec3 position { 0.0f, 8.0f, 0.0f };
    float yaw   = -90.0f;
    float pitch =   0.0f;

    glm::mat4 getView()                   const;
    glm::mat4 getProjection(float aspect) const;
    glm::vec3 forward()                   const;
    glm::vec3 right()                     const;
    glm::vec3 up()                        const;

    void processMouse(float dx, float dy, float sensitivity);
};
