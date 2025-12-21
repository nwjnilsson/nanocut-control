#ifndef BOX_
#define BOX_

#include "../../geometry/geometry.h"
#include "../Primitive.h"
#include <string>

class Box : public Primitive {
public:
  Point2d m_bottom_left;
  float   m_width;
  float   m_height;
  float   m_corner_radius;

  Box(Point2d bl, float w, float h, float cr)
    : m_bottom_left(bl), m_width(w), m_height(h), m_corner_radius(cr)
  {
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Box-specific methods
  void render_rectangle_with_radius(
    float x, float y, float width, float height, float radius);
};

#endif // BOX_
