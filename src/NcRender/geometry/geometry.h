#ifndef GEOMETRY_H
#define GEOMETRY_H

/*********************
 *      INCLUDES
 *********************/
#include "../json/json.h"
#include "application.h"
#include "clipper.h"
#include <stddef.h>
#include <string>
#include <utility>
/**********************
 *      TYPEDEFS
 **********************/
typedef std::pair<double, double> Point;
struct double_line_t {
  Point2d start;
  Point2d end;
};

/**********************
 * GLOBAL PROTOTYPES
 **********************/
class Geometry {
public:
  /*
      Check if number is bewteen a given min/max range
  */
  bool between(double x, double min, double max);

  /*
      Check if both points match
  */
  bool pointsMatch(Point2d p1, Point2d p2);

  /*
      Output line segments from described arc
  */
  nlohmann::json arcToLineSegments(double cx,
                                   double cy,
                                   double r,
                                   double start_angle,
                                   double end_angle,
                                   double num_segments);

  /*
      Output line segments from described circle
  */
  nlohmann::json circleToLineSegments(double cx, double cy, double r);

  /*
      Input a json stack of lines, arc, circles, etc and Output a stack with
     non-lines converted to line segments. ! Does not reguire stack to be
     vectorized !
  */
  nlohmann::json normalize(const nlohmann::json& geometry_stack);

  /*
      Input a json stack of lines, arcs, circles and return a json object
      min/max = {x: xxx, max: xxx};
      return {min, max};
  */
  nlohmann::json getExtents(const nlohmann::json& geometry_stack);

  /*
      Rotate a point around a specified point by angle in degrees
      Returns new point with rotation applied
  */
  Point2d rotatePoint(Point2d center, Point2d point, double angle);

  /*
      Mirror a point about a line
      Returns new point with mirror applied
  */
  Point2d mirrorPoint(Point2d point, double_line_t line);

  /*
      Chainify line segments
      Inputs a "normalized" (meaning only line segments) stack of entities and
     vectors them aka chaining Returns an array of contours
  */
  nlohmann::json chainify(const nlohmann::json& geometry_stack,
                          double                tolerance);

  /*
      Offset contour
      Inputs a list of points representing a closed contour and a distance to
     offset negative offsets offset inside and positive offsets offset to the
     outside Returns a list of points with offset applied
  */
  nlohmann::json offset(const nlohmann::json& path, double offset);

  /*
      Slot Path
      Inputs a list of points representing a closed contour and a distance to
     offset negative offsets offset inside and positive offsets offset to the
     outside Returns a list of points (polygon) with offset applied to path
  */
  nlohmann::json slot(const nlohmann::json& path, double offset);

  /*
      Simplify a vector of Point2d
          returns the simplified vector
  */
  std::vector<Point2d> simplify(const std::vector<Point2d> points,
                                double                     smoothing);

  /*
      Ramer Douglas Peucker Algorythm for simplifying points
  */
  void ramerDouglasPeucker(const std::vector<Point>& pointList,
                           double                    epsilon,
                           std::vector<Point>&       out);

  /*
      Get distance between two points
  */
  double distance(Point2d p1, Point2d p2);

  /*
      Get Midpoint between two points
  */
  Point2d midpoint(Point2d p1, Point2d p2);

  /*
      Create a polar line
  */
  double_line_t
  createPolarLine(Point2d start_point, double angle, double length);

  /*
      Calculate circle center given three points to describe the circle
  */
  Point2d threePointCircleCenter(Point2d p1, Point2d p2, Point2d p3);

  /*
      Return the polar angle between two lines
  */
  double measurePolarAngle(Point2d p1, Point2d p2);

  /*
      Return the circumference of an arc segment
  */
  double
  measureArcCircumference(double start_angle, double end_angle, double radius);

  /*
      Check if two lines intersect
          returns bool
  */
  bool linesIntersect(double_line_t l1, double_line_t l2);

  /*
      Check is line intersects with circle
  */
  bool lineIntersectsWithCircle(double_line_t l, Point2d center, double radius);

  /*
      Check if point lies inside polygon
  */
  bool pointIsInsidePolygon(const nlohmann::json& polygon,
                            const nlohmann::json& point);

  /*
      Check if polygon1 is completly inside polygon2
  */
  bool polygonIsInsidePolygon(const nlohmann::json& polygon1,
                              const nlohmann::json& polygon2);

private:
  /*
      Used by RamerDouglasPeucker method
  */
  double PerpendicularDistance(const Point& pt,
                               const Point& lineStart,
                               const Point& lineEnd);

  /*
      Check if a point is inside a polygon
          returns boolean
  */
  bool pointInPolygon(int                 polyCorners,
                      std::vector<double> polyX,
                      std::vector<double> polyY,
                      double              x,
                      double              y);
};

/**********************
 * VIEWER PROTOTYPES
 **********************/

/**********************
 *      MACROS
 **********************/

#endif
