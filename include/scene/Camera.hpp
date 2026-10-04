#pragma once

#include <glm/glm.hpp>

namespace pt {

// Pinhole camera. Yaw/pitch are in degrees: yaw 0 looks down -Z, positive yaw turns right,
// positive pitch looks up. (Matches the old quaternion camera's conventions exactly.)
struct Camera {
    glm::vec3 position{0.0f};
    float yawDeg = 0.0f;
    float pitchDeg = 0.0f;
    float fovYDeg = 70.0f;

    glm::vec3 forward() const;
    glm::vec3 right() const;
    glm::vec3 up() const;

    bool operator==(const Camera&) const = default;
};

} // namespace pt
