#pragma once

#include <GLFW/glfw3.h>
#include <NcRender/geometry/geometry.h>
#include <unordered_set>

class InputState {
public:
  // Constructor/Destructor
  InputState() = default;
  ~InputState() = default;

  // Non-copyable, movable
  InputState(const InputState&) = delete;
  InputState& operator=(const InputState&) = delete;
  InputState(InputState&&) = default;
  InputState& operator=(InputState&&) = default;

  // Keyboard state management
  void setKeyPressed(int key, bool pressed);
  bool isKeyPressed(int key) const;
  bool isKeyDown(int key) const { return isKeyPressed(key); }

  // Mouse button state management
  void setMouseButtonPressed(int button, bool pressed);
  bool isMouseButtonPressed(int button) const;
  bool isMouseButtonDown(int button) const
  {
    return isMouseButtonPressed(button);
  }

  // Mouse position management
  void setMousePosition(double x, double y);
  void setMousePosition(const Point2d& pos);
  void setPreviousMousePosition(double x, double y);

  const Point2d& getMousePosition() const { return m_mouse_position; }
  const Point2d& getPreviousMousePosition() const
  {
    return m_previous_mouse_position;
  }

  double getMouseX() const { return m_mouse_position.x; }
  double getMouseY() const { return m_mouse_position.y; }
  double getPreviousMouseX() const { return m_previous_mouse_position.x; }
  double getPreviousMouseY() const { return m_previous_mouse_position.y; }

  // Mouse delta calculations
  double getMouseDeltaX() const
  {
    return m_mouse_position.x - m_previous_mouse_position.x;
  }
  double getMouseDeltaY() const
  {
    return m_mouse_position.y - m_previous_mouse_position.y;
  }
  Point2d getMouseDelta() const
  {
    return { getMouseDeltaX(), getMouseDeltaY() };
  }

  // Modifier key shortcuts (using GLFW constants)
  bool isCtrlPressed() const;
  bool isShiftPressed() const;
  bool isAltPressed() const;
  bool isSuperPressed() const;

  // Common key combinations
  bool isCtrlShiftPressed() const
  {
    return isCtrlPressed() && isShiftPressed();
  }
  bool isCtrlAltPressed() const { return isCtrlPressed() && isAltPressed(); }

  // Mouse button shortcuts
  bool isLeftMousePressed() const
  {
    return isMouseButtonPressed(GLFW_MOUSE_BUTTON_LEFT);
  }
  bool isRightMousePressed() const
  {
    return isMouseButtonPressed(GLFW_MOUSE_BUTTON_RIGHT);
  }
  bool isMiddleMousePressed() const
  {
    return isMouseButtonPressed(GLFW_MOUSE_BUTTON_MIDDLE);
  }

  // State management
  void clear();
  void clearKeys();
  void clearMouseButtons();

  // Debug/utility methods
  size_t getNumPressedKeys() const { return m_pressed_keys.size(); }
  size_t getNumPressedMouseButtons() const
  {
    return m_pressed_mouse_buttons.size();
  }

private:
  // State storage - unordered_set provides O(1) lookups
  std::unordered_set<int> m_pressed_keys;
  std::unordered_set<int> m_pressed_mouse_buttons;

  // Mouse position tracking
  Point2d m_mouse_position{ 0.0, 0.0 };
  Point2d m_previous_mouse_position{ 0.0, 0.0 };
};
