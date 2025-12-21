#pragma once

#include "../NcRender/geometry/geometry.h"
#include "../input/InputEvents.h"
#include <GLFW/glfw3.h>
#include <functional>

class NcApp;
class InputState;
class Primitive;

class View {
public:
  View(NcApp* app);
  virtual ~View() = default;

  // Virtual event handlers with default empty implementations
  virtual void tick() {} // Views can override to implement per-frame logic
  virtual void handleMouseEvent(const MouseButtonEvent& e,
                                const InputState&       input)
  {
  } // Mouse button events
  virtual void handleKeyEvent(const KeyEvent& e, const InputState& input) {
  } // Keyboard events
  virtual void handleScrollEvent(const ScrollEvent& e, const InputState& input)
  {
  } // Mouse scroll events
  virtual void handleWindowResize(const WindowResizeEvent& e,
                                  const InputState&        input)
  {
  } // Window resize events
  virtual void handleMouseMoveEvent(const MouseMoveEvent& e,
                                    const InputState&     input)
  {
  } // Mouse movement events

  // View transform management
  double getZoom() const { return m_zoom; }
  void   setZoom(double zoom) { m_zoom = zoom; }
  void   adjustZoom(double zoom_factor, const Point2d& zoom_center);
  void   zoomIn(const Point2d& center = { 0, 0 })
  {
    adjustZoom(1.1, center);
  }
  void zoomOut(const Point2d& center = { 0, 0 })
  {
    adjustZoom(0.9, center);
  }
  void resetZoom()
  {
    m_zoom = 1.0;
    m_pan = { 0, 0 };
  }

  const Point2d& getPan() const { return m_pan; }
  void                  setPan(const Point2d& pan) { m_pan = pan; }
  void                  adjustPan(const Point2d& delta)
  {
    m_pan.x += delta.x;
    m_pan.y += delta.y;
  }

  // Pan/drag state management
  bool isMoveViewActive() const { return m_move_view; }
  void setMoveViewActive(bool active);
  void handleMouseMove(const Point2d& current_mouse_pos);

  // Coordinate transformations (using this view's transform)
  Point2d screenToMatrix(const Point2d& screen_pos) const;
  Point2d matrixToScreen(const Point2d& matrix_pos) const;

  // Matrix transformation callback for primitives
  void                            applyViewTransformation(Primitive* primitive);
  std::function<void(Primitive*)> getViewMatrixCallback();

protected:
  NcApp* m_app{ nullptr };

  // View-specific transform state
  double         m_zoom{ 1.0 };
  Point2d m_pan{ 0.0, 0.0 };

  // Pan/drag state
  bool           m_move_view{ false };
  Point2d m_last_mouse_pos{ 0.0, 0.0 };
};
