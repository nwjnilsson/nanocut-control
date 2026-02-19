#ifndef POLY_NEST_
#define POLY_NEST_

#include <NcRender/geometry/clipper.h>
#include <loguru.hpp>
#include <NanoCut.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string>
#include <vector>

namespace PolyNest {
class PolyPoint {
public:
  double x;
  double y;
  PolyPoint() {};
  PolyPoint(double x, double y)
  {
    this->x = x;
    this->y = y;
  };
};
class PolyGon {
public:
  ClipperLib::Path m_vertices;
  bool             m_is_closed;
  bool             m_is_inside;
};
class PolyPart {
public:
  std::vector<PolyGon> m_polygons; // Vector of polygons which are centered
                                   // along their extents at X0,Y0
  std::vector<PolyGon> m_built_polygons; // Polygons which are calculated from
                                         // polygons. Matrix is (rotated +
                                         // offset)
                                         // * scale
  PolyPoint   m_bbox_min;
  PolyPoint   m_bbox_max;
  double*     m_offset_x;
  double*     m_offset_y;
  double*     m_angle;
  bool*       m_visible;
  std::string m_part_name;
  PolyPart()
  {
    m_bbox_min.x = 0;
    m_bbox_min.y = 0;
    m_bbox_max.x = 0;
    m_bbox_max.y = 0;
    m_part_name = "";
  };
  PolyPoint rotatePoint(PolyPoint p, double angle);
  void      getBoundingBox(PolyPoint* bbox_min, PolyPoint* bbox_max);
  void      moveOutsidePolyGonToBack();
  void      build();
};
class PolyNest {
private:
  bool                  m_found_valid_placement;
  PolyPoint             m_min_extents;
  PolyPoint             m_max_extents;
  double                m_closed_tolerance;
  double                m_rotation_increments;
  PolyPoint             m_offset_increments;
  std::vector<PolyPart> m_unplaced_parts;
  std::vector<PolyPart> m_placed_parts;
  // For tracking best placement when no perfect fit is found
  struct BestPlacement {
    double score;
    double offset_x;
    double offset_y;
    double angle;
    bool   valid;
  } m_best_placement;
  bool   checkIfPointIsInsidePath(std::vector<PolyPoint> path, PolyPoint point);
  bool   checkIfPathIsInsidePath(std::vector<PolyPoint> path1,
                                 std::vector<PolyPoint> path2);
  bool   checkIfPolyPartTouchesAnyPlacedPolyPart(PolyPart p);
  bool   checkIfPolyPartIsInsideExtents(PolyPart  p,
                                        PolyPoint min_point,
                                        PolyPoint max_point);
  double measureDistanceBetweenPoints(PolyPoint a, PolyPoint b);
  double perpendicularDistance(const PolyPoint& pt,
                               const PolyPoint& lineStart,
                               const PolyPoint& lineEnd);
  void   simplify(const std::vector<PolyPoint>& pointList,
                  std::vector<PolyPoint>&       out,
                  double                        epsilon);
  PolyPart  buildPart(std::vector<std::vector<PolyPoint>> p,
                      double*                             offset_x,
                      double*                             offset_y,
                      double*                             angle,
                      bool*                               visible);
  PolyPoint getDefaultOffsetXY(const PolyPart&);
  double   calculatePlacementScore(const PolyPart& part,
                                   double              offset_x,
                                   double              offset_y,
                                   double              angle);
  void     resetBestPlacement();
  void     updateBestPlacement(const PolyPart& part,
                                double              offset_x,
                                double              offset_y,
                                double              angle);

public:
  PolyNest()
  {
    m_found_valid_placement = false;
    m_min_extents = PolyPoint(0, 0);
    m_max_extents = PolyPoint(SCALE(1000), SCALE(1000));
    m_closed_tolerance = SCALE(0.1);
    m_rotation_increments = 360.0 / 6;
    m_offset_increments.x = SCALE(5.0);
    m_offset_increments.y = SCALE(5.0);
    m_unplaced_parts.clear();
    m_placed_parts.clear();
    resetBestPlacement();
  };
  void        setExtents(PolyPoint min, PolyPoint max);
  void        pushUnplacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                                   double*                             offset_x,
                                   double*                             offset_y,
                                   double*                             angle,
                                   bool*                               visible);
  void        pushPlacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                                 double*                             offset_x,
                                 double*                             offset_y,
                                 double*                             angle,
                                 bool*                               visible);
  void        beginPlaceUnplacedPolyParts();
  static bool placeUnplacedPolyPartsTick(void* p);
};
}; // namespace PolyNest

#endif
