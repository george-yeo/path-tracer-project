#include "scene/Camera.hpp"

#include <cmath>

namespace pt {

glm::vec3 Camera::forward() const {
    float yaw = glm::radians(yawDeg);
    float pitch = glm::radians(pitchDeg);
    return {std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch)};
}

glm::vec3 Camera::right() const {
    float yaw = glm::radians(yawDeg);
    return {std::cos(yaw), 0.0f, std::sin(yaw)};
}

glm::vec3 Camera::up() const {
    return glm::cross(right(), forward());
}

} // namespace pt
