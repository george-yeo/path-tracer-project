#include "app/CameraController.hpp"

#include "scene/Camera.hpp"

#include <GLFW/glfw3.h>

#include <algorithm>

namespace pt {

namespace {

constexpr float kTurnSpeedDeg = 60.0f;     // arrow keys, degrees per second
constexpr float kMouseSensitivity = 0.15f; // degrees per pixel
constexpr float kFastMultiplier = 4.0f;
constexpr float kMaxPitch = 89.0f;

bool held(GLFWwindow* window, int key) {
    return glfwGetKey(window, key) == GLFW_PRESS;
}

} // namespace

void CameraController::update(GLFWwindow* window, Camera& camera, float dt, float moveSpeed,
                              bool keyboardFree, bool mouseFree) {
    active = false;

    if (keyboardFree) {
        float speed = moveSpeed * dt;
        if (held(window, GLFW_KEY_LEFT_SHIFT) || held(window, GLFW_KEY_RIGHT_SHIFT)) {
            speed *= kFastMultiplier;
        }

        glm::vec3 move{0.0f};
        if (held(window, GLFW_KEY_W)) move += camera.forward();
        if (held(window, GLFW_KEY_S)) move -= camera.forward();
        if (held(window, GLFW_KEY_D)) move += camera.right();
        if (held(window, GLFW_KEY_A)) move -= camera.right();
        if (held(window, GLFW_KEY_E)) move += camera.up();
        if (held(window, GLFW_KEY_Q)) move -= camera.up();
        if (move != glm::vec3(0.0f)) {
            camera.position += move * speed;
            active = true;
        }

        float turn = kTurnSpeedDeg * dt;
        if (held(window, GLFW_KEY_LEFT)) camera.yawDeg -= turn;
        if (held(window, GLFW_KEY_RIGHT)) camera.yawDeg += turn;
        if (held(window, GLFW_KEY_UP)) camera.pitchDeg += turn;
        if (held(window, GLFW_KEY_DOWN)) camera.pitchDeg -= turn;
        active = active || held(window, GLFW_KEY_LEFT) || held(window, GLFW_KEY_RIGHT) ||
                 held(window, GLFW_KEY_UP) || held(window, GLFW_KEY_DOWN);
    }

    double x = 0.0;
    double y = 0.0;
    glfwGetCursorPos(window, &x, &y);
    bool rightButton = glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;
    if (rightButton && (looking || mouseFree)) {
        if (looking) {
            camera.yawDeg += static_cast<float>(x - lastX) * kMouseSensitivity;
            camera.pitchDeg -= static_cast<float>(y - lastY) * kMouseSensitivity;
        }
        looking = true;
        active = true;
    } else {
        looking = false;
    }
    lastX = x;
    lastY = y;

    camera.pitchDeg = std::clamp(camera.pitchDeg, -kMaxPitch, kMaxPitch);
}

} // namespace pt
