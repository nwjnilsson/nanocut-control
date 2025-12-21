#ifndef PRIMITIVE_BASE_
#define PRIMITIVE_BASE_

#include "../json/json.h"
#include "application.h"
#include <functional>
#include <string>

/**
 * TransformData - Contains all data needed for primitive transformations
 */
struct TransformData {
  double zoom;
  double pan_x;
  double pan_y;
  double wco_x; // Work coordinate offset (pan + work_offset * zoom)
  double wco_y;
};

/**
 * Primitive - Base class for all renderable primitives
 */
class Primitive {
public:
  // Rendering properties
  bool           visible = true;
  bool           mouse_over = false;
  float          mouse_over_padding = 5.0f;
  int            zindex = 1;
  float          color[4] = { 255.0f, 255.0f, 255.0f, 255.0f };
  float          offset[3] = { 0.0f, 0.0f, 0.0f };
  float          scale = 1.0f;
  float          angle = 0.0f;
  std::string    id = "";
  std::string    view = "main";
  nlohmann::json data = nullptr;

  // Event callbacks
  std::function<void(Primitive*, const nlohmann::json&)> mouse_callback;
  std::function<void(Primitive*)>                        matrix_callback;

  // Mouse event state
  nlohmann::json m_mouse_event;

  // Virtual destructor for proper cleanup of derived classes
  virtual ~Primitive() = default;

  // Pure virtual methods that all primitives must implement
  virtual std::string    getTypeName() = 0;
  virtual void           processMouse(float mpos_x, float mpos_y) = 0;
  virtual void           render() = 0;
  virtual nlohmann::json serialize() = 0;

  // Transform method - can be overridden by derived classes for custom behavior
  // Default implementation: apply screen-coordinate transform (pan-based)
  virtual void applyTransform(const TransformData& transform)
  {
    scale = transform.zoom;
    offset[0] = transform.pan_x;
    offset[1] = transform.pan_y;
  }

protected:
  // Protected constructor - only derived classes can instantiate
  Primitive() : m_mouse_event(nullptr) {}
};

#endif // PRIMITIVE_BASE_
