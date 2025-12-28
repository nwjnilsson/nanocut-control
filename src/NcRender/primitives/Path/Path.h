#ifndef PATH_
#define PATH_

#include "../../geometry/geometry.h"
#include "../Primitive.h"
#include <string>
#include <vector>

class Path : public Primitive {
public:
  std::vector<Point2d> m_points;
  bool                 m_is_closed;
  float                m_width;
  std::string          m_style; // solid, dashed

  Path(const std::vector<Point2d>& p)
    : m_points(p), m_is_closed(true), m_width(2), m_style("solid")
  {
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Override transform - gcode paths use WCO, others use default
  void applyTransform(const TransformData& transform) override
  {
    if ((flags & PrimitiveFlags::GCode) != PrimitiveFlags::None) {
      // Use work coordinate offset for gcode paths
      scale = transform.zoom;
      offset[0] = transform.wco_x;
      offset[1] = transform.wco_y;
    }
    else {
      // Use default screen-coordinate transform
      Primitive::applyTransform(transform);
    }
  }
};

#endif // PATH_
