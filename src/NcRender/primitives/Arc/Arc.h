#ifndef ARC_
#define ARC_

#include "../Primitive.h"
#include "NanoCut.h"
#include <string>

class Arc : public Primitive {
public:
  Point2d     m_center;
  float       m_start_angle;
  float       m_end_angle;
  float       m_radius;
  float       m_width;
  std::string m_style; // solid, dashed

  Arc(Point2d c, float r, float sa, float ea)
    : m_center(c), m_radius(r), m_start_angle(sa), m_end_angle(ea), m_width(1),
      m_style("solid")
  {
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Override transform - adjust radius for specific pointer IDs (same as
  // Circle)
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

  // Arc-specific methods
  void renderArc(
    double cx, double cy, double radius, double start_angle, double end_angle);
};

#endif // ARC_
