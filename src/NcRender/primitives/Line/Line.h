#ifndef LINE_
#define LINE_

#include "../Primitive.h"
#include "../../geometry/geometry.h"
#include <string>

class Line : public Primitive {
public:
  Point2d m_start;
  Point2d m_end;
  float          m_width;
  std::string    m_style; // solid, dashed

  Line(Point2d s, Point2d e)
    : m_start(s), m_end(e), m_width(1), m_style("solid")
  {
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Override transform to use work coordinate offset
  void applyTransform(const TransformData& transform) override
  {
    scale = transform.zoom;
    offset[0] = transform.wco_x;
    offset[1] = transform.wco_y;
  }
};

#endif // LINE_
