#ifndef POLY_NEST_
#define POLY_NEST_

#include <EasyRender/geometry/clipper.h>
#include <EasyRender/logging/loguru.h>
#include <application.h>

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
  ClipperLib::Path vertices;
  bool             is_closed;
  bool             is_inside;
};
class PolyPart {
public:
  std::vector<PolyGon> polygons; // Vector of polygons which are centered along
                                 // their extents at X0,Y0
  std::vector<PolyGon> built_polygons; // Polygons which are calculated from
                                       // polygons. Matrix is (rotated + offset)
                                       // * scale
  PolyPoint   bbox_min;
  PolyPoint   bbox_max;
  double*     offset_x;
  double*     offset_y;
  double*     angle;
  bool*       visible;
  std::string part_name;
  PolyPart()
  {
    this->bbox_min.x = 0;
    this->bbox_min.y = 0;
    this->bbox_max.x = 0;
    this->bbox_max.y = 0;
    this->part_name  = "";
  };
  PolyPoint RotatePoint(PolyPoint p, double angle);
  void      GetBoundingBox(PolyPoint* bbox_min, PolyPoint* bbox_max);
  void      MoveOutsidePolyGonToBack();
  void      Build();
};
class PolyNest {
private:
  bool                  found_valid_placement;
  PolyPoint             min_extents;
  PolyPoint             max_extents;
  double                closed_tolerance;
  double                rotation_increments;
  PolyPoint             offset_increments;
  std::vector<PolyPart> unplaced_parts;
  std::vector<PolyPart> placed_parts;
  bool   CheckIfPointIsInsidePath(std::vector<PolyPoint> path, PolyPoint point);
  bool   CheckIfPathIsInsidePath(std::vector<PolyPoint> path1,
                                 std::vector<PolyPoint> path2);
  bool   CheckIfPolyPartTouchesAnyPlacedPolyPart(PolyPart p);
  bool   CheckIfPolyPartIsInsideExtents(PolyPart  p,
                                        PolyPoint min_point,
                                        PolyPoint max_point);
  double MeasureDistanceBetweenPoints(PolyPoint a, PolyPoint b);
  double PerpendicularDistance(const PolyPoint& pt,
                               const PolyPoint& lineStart,
                               const PolyPoint& lineEnd);
  void   Simplify(const std::vector<PolyPoint>& pointList,
                  std::vector<PolyPoint>&       out,
                  double                        epsilon);
  PolyPart  BuildPart(std::vector<std::vector<PolyPoint>> p,
                      double*                             offset_x,
                      double*                             offset_y,
                      double*                             angle,
                      bool*                               visible);
  PolyPoint GetDefaultOffsetXY(const PolyPart&);

public:
  PolyNest()
  {
    this->found_valid_placement = false;
    min_extents                 = PolyPoint(0, 0);
    max_extents                 = PolyPoint(SCALE(1000), SCALE(1000));
    this->closed_tolerance      = SCALE(0.1);
    this->rotation_increments   = 360.0 / 6;
    this->offset_increments.x   = SCALE(5.0);
    this->offset_increments.y   = SCALE(5.0);
    this->unplaced_parts.clear();
    this->placed_parts.clear();
  };
  void        SetExtents(PolyPoint min, PolyPoint max);
  void        PushUnplacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                                   double*                             offset_x,
                                   double*                             offset_y,
                                   double*                             angle,
                                   bool*                               visible);
  void        PushPlacedPolyPart(std::vector<std::vector<PolyPoint>> p,
                                 double*                             offset_x,
                                 double*                             offset_y,
                                 double*                             angle,
                                 bool*                               visible);
  void        BeginPlaceUnplacedPolyParts();
  static bool PlaceUnplacedPolyPartsTick(void* p);
};
}; // namespace PolyNest

#endif
