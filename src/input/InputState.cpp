#include "InputState.h"

void InputState::setKeyPressed(int key, bool pressed) {
    if (pressed) {
        m_pressed_keys.insert(key);
    } else {
        m_pressed_keys.erase(key);
    }
}

bool InputState::isKeyPressed(int key) const {
    return m_pressed_keys.find(key) != m_pressed_keys.end();
}

void InputState::setMouseButtonPressed(int button, bool pressed) {
    if (pressed) {
        m_pressed_mouse_buttons.insert(button);
    } else {
        m_pressed_mouse_buttons.erase(button);
    }
}

bool InputState::isMouseButtonPressed(int button) const {
    return m_pressed_mouse_buttons.find(button) != m_pressed_mouse_buttons.end();
}

void InputState::setMousePosition(double x, double y) {
    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = {x, y};
}

void InputState::setMousePosition(const Point2d& pos) {
    m_previous_mouse_position = m_mouse_position;
    m_mouse_position = pos;
}

void InputState::setPreviousMousePosition(double x, double y) {
    m_previous_mouse_position = {x, y};
}

bool InputState::isCtrlPressed() const {
    return isKeyPressed(GLFW_KEY_LEFT_CONTROL) || isKeyPressed(GLFW_KEY_RIGHT_CONTROL);
}

bool InputState::isShiftPressed() const {
    return isKeyPressed(GLFW_KEY_LEFT_SHIFT) || isKeyPressed(GLFW_KEY_RIGHT_SHIFT);
}

bool InputState::isAltPressed() const {
    return isKeyPressed(GLFW_KEY_LEFT_ALT) || isKeyPressed(GLFW_KEY_RIGHT_ALT);
}

bool InputState::isSuperPressed() const {
    return isKeyPressed(GLFW_KEY_LEFT_SUPER) || isKeyPressed(GLFW_KEY_RIGHT_SUPER);
}

void InputState::clear() {
    clearKeys();
    clearMouseButtons();
    m_mouse_position = {0.0, 0.0};
    m_previous_mouse_position = {0.0, 0.0};
}

void InputState::clearKeys() {
    m_pressed_keys.clear();
}

void InputState::clearMouseButtons() {
    m_pressed_mouse_buttons.clear();
}
