#pragma once

struct GLFWwindow;

namespace pt {

struct Camera;

// Fly camera: WASD move, Q/E down/up, arrow keys or right-mouse drag to look, Shift for speed.
class CameraController {
    public:
    // Applies this frame's input to `camera`. Input the UI has captured is ignored.
    void update(GLFWwindow* window, Camera& camera, float dt, float moveSpeed, bool keyboardFree,
                bool mouseFree);

    // True while keys are held or the mouse is dragging (the app avoids idling then).
    bool isActive() const { return active; }

    private:
    bool looking = false;
    double lastX = 0.0;
    double lastY = 0.0;
    bool active = false;
};

} // namespace pt
