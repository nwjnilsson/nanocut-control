#ifndef CIRCLE_
#define CIRCLE_

#include "../../geometry/geometry.h"
#include "../Primitive.h"
#include <string>

class Circle : public Primitive {
public:
  Point2d     m_center;
  float       m_radius;
  float       m_width;
  std::string m_style; // solid, dashed

  Circle(Point2d c, float r)
    : m_center(c), m_radius(r), m_width(1), m_style("solid")
  {
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Override transform - adjust radius for specific pointer IDs
  void applyTransform(const TransformData& transform) override
  {
    // Apply default screen-coordinate transform
    scale = transform.zoom;
    offset[0] = transform.pan_x;
    offset[1] = transform.pan_y;

    // Adjust radius inversely with zoom for specific IDs
    if (id == "torch_pointer") {
      m_radius = 5.0f / transform.zoom;
    }
    else if (id == "waypoint_pointer") {
      m_radius = 8.0f / transform.zoom;
    }
  }

  // Circle-specific methods
  void renderArc(
    double cx, double cy, double radius, double start_angle, double end_angle);
};

#endif // CIRCLE_
