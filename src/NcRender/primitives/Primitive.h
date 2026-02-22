#ifndef PRIMITIVE_BASE_
#define PRIMITIVE_BASE_

#include "Input/InputEvents.h"
#include "NanoCut.h"

#include <nlohmann/json.hpp>

#include <functional>
#include <optional>
#include <string>
#include <variant>

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
 * PrimitiveFlags - Behavioral flags for primitives (separate identity from
 * behavior) These flags determine how primitives are treated by the rendering
 * system, independent of their string ID which is just for naming/debugging.
 */
enum class PrimitiveFlags : uint32_t {
  None = 0,
  GCode = 1 << 0,                  // Uses WCO transform, not screen transform
  GCodeArrow = 1 << 1,             // Direction indicator for gcode paths
  GCodeHighlight = 1 << 2 | GCode, // Highlighted path segment (e.g., arc-ok)
  MaterialPlane = 1 << 3,          // Material reference plane (CAM view)
  TorchPointer = 1 << 4,           // Current torch position marker
  WaypointPointer = 1 << 5,        // Target waypoint marker
  SystemUI = 1 << 6,               // System UI element (FPS counter, etc)
  MachinePlane = 1 << 7,           // Machine reference plane (Control view)
  CuttablePlane = 1 << 8,          // Cuttable area plane (Control view)
};

// Bitwise operators for PrimitiveFlags
inline PrimitiveFlags operator|(PrimitiveFlags a, PrimitiveFlags b)
{
  return static_cast<PrimitiveFlags>(static_cast<uint32_t>(a) |
                                     static_cast<uint32_t>(b));
}

inline PrimitiveFlags operator&(PrimitiveFlags a, PrimitiveFlags b)
{
  return static_cast<PrimitiveFlags>(static_cast<uint32_t>(a) &
                                     static_cast<uint32_t>(b));
}

inline PrimitiveFlags operator~(PrimitiveFlags f)
{
  return static_cast<PrimitiveFlags>(~static_cast<uint32_t>(f));
}

inline PrimitiveFlags& operator|=(PrimitiveFlags& a, PrimitiveFlags b)
{
  a = a | b;
  return a;
}

inline bool operator!(PrimitiveFlags f)
{
  return static_cast<uint32_t>(f) == 0;
}

inline bool hasFlag(PrimitiveFlags f1, PrimitiveFlags f2)
{
  return (f1 & f2) != PrimitiveFlags::None;
}

/**
 * PrimitiveUserData - Type-safe variant for storing primitive-specific data
 * Replaces the previous JSON-based storage for better performance and type
 * safety
 */
using PrimitiveUserData =
  std::variant<std::monostate, int, double, std::string>;

/**
 * Primitive - Base class for all renderable primitives
 */
class Primitive {
public:
  // Rendering properties
  bool              visible = true;
  bool              mouse_over = false;
  float             mouse_over_padding = 5.0f;
  int               zindex = 1;
  Color4f           color = { 255.0f, 255.0f, 255.0f, 255.0f };
  float             offset[3] = { 0.0f, 0.0f, 0.0f };
  float             scale = 1.0f;
  float             angle = 0.0f;
  std::string       id = "";
  std::string       view = "main";
  PrimitiveFlags    flags = PrimitiveFlags::None;
  PrimitiveUserData user_data; // Type-safe variant (replaces JSON data field)

  // Mouse event data - can be either a button event or hover event
  using MouseEventData = std::variant<MouseButtonEvent, MouseHoverEvent>;

  // Event callbacks
  std::function<void(Primitive*, const MouseEventData&)> mouse_callback;
  std::function<void(Primitive*)>                        matrix_callback;

  // Mouse event state (optional - only set when an event is pending)
  std::optional<MouseEventData> m_mouse_event;

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
  Primitive() = default;
};

#endif // PRIMITIVE_BASE_
