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

  // Add a point and invalidate the hit-test cache
  void addPoint(const Point2d& p)
  {
    m_points.push_back(p);
    m_hit_cache_dirty = true;
  }

  // Invalidate cache (for any direct m_points mutation)
  void invalidateHitCache() { m_hit_cache_dirty = true; }

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

private:
  static constexpr size_t kSimplifyThreshold = 32;
  bool                    m_hit_cache_dirty = true;
  geo::Extents            m_bbox;
  std::vector<Point2d>    m_simplified;

  void rebuildHitCache();
};

#endif // PATH_
