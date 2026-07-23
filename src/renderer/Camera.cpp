#include "Camera.h"
#include <glm/gtc/matrix_transform.hpp>

glm::vec3 Camera::forward() const {
    float yr = glm::radians(yaw), pr = glm::radians(pitch);
    return glm::normalize(glm::vec3(
        cos(yr) * cos(pr),
        sin(pr),
        sin(yr) * cos(pr)
    ));
}

glm::vec3 Camera::right() const {
    return glm::normalize(glm::cross(forward(), glm::vec3(0, 1, 0)));
}

glm::vec3 Camera::up() const {
    return glm::normalize(glm::cross(right(), forward()));
}

glm::mat4 Camera::getView() const {
    return glm::lookAt(position, position + forward(), glm::vec3(0, 1, 0));
}

glm::mat4 Camera::getProjection(float aspect) const {
    return glm::perspective(glm::radians(70.0f), aspect, 0.05f, 1200.0f);
}

void Camera::processMouse(float dx, float dy, float sensitivity) {
    yaw   += dx * sensitivity;
    pitch -= dy * sensitivity;
    if (pitch >  89.0f) pitch =  89.0f;
    if (pitch < -89.0f) pitch = -89.0f;
}
