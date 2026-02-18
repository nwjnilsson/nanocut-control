#ifndef GEOMETRY_H
#define GEOMETRY_H

/*********************
 *      INCLUDES
 *********************/
#include <nlohmann/json.hpp>
#include "NanoCut.h"
#include "clipper.h"
#include <stddef.h>
#include <string>
#include <vector>

/**********************
 *      TYPEDEFS
 **********************/

// Geometric primitives with proper types
namespace geo {

// Basic geometric types
struct Line {
  Point2d start;
  Point2d end;
};

struct Arc {
  Point2d center;
  double  radius;
  double  start_angle; // in degrees
  double  end_angle;   // in degrees
};

struct Circle {
  Point2d center;
  double  radius;
};

struct Extents {
  Point2d min;
  Point2d max;
};

// Aliases for clarity
using Path = std::vector<Point2d>;
using Contour = std::vector<Point2d>;

// Variant for mixed geometry (used in normalize and similar functions)
enum class GeometryType { Line, Arc, Circle };

struct GeometryPrimitive {
  GeometryType type;
  union {
    Line   line;
    Arc    arc;
    Circle circle;
  };

  // Constructors
  GeometryPrimitive(const Line& l) : type(GeometryType::Line), line(l) {}
  GeometryPrimitive(const Arc& a) : type(GeometryType::Arc), arc(a) {}
  GeometryPrimitive(const Circle& c)
    : type(GeometryType::Circle), circle(c)
  {}
};

/**********************
 * GEOMETRY NAMESPACE
 **********************/

// Basic utility functions
bool    between(double x, double min, double max);
bool    pointsMatch(Point2d p1, Point2d p2);
double  distance(Point2d p1, Point2d p2);
Point2d midpoint(Point2d p1, Point2d p2);

// Geometric transformations
Point2d rotatePoint(Point2d center, Point2d point, double angle);
Point2d mirrorPoint(Point2d point, const Line& line);

// Geometric conversions (proper typed versions)
std::vector<Line> arcToLineSegments(const Arc& arc, int num_segments = 100);
std::vector<Line> circleToLineSegments(const Circle& circle);
std::vector<Line> normalize(const std::vector<GeometryPrimitive>& geometry_stack);

// Geometric analysis
Extents getExtents(const std::vector<Line>& lines);
Extents getExtents(const std::vector<GeometryPrimitive>& geometry_stack);

// Bounding box helpers for containment optimization
Extents calculateBoundingBox(const Path& path);
bool    extentsContain(const Extents& outer, const Extents& inner);

// Path operations
std::vector<Contour>          chainify(const std::vector<Line>& lines,
                                       double                   tolerance);
std::vector<Path>             offset(const Path& path, double offset);
std::vector<Path>             slot(const Path& path, double offset);
std::vector<Point2d>          simplify(const std::vector<Point2d>& points,
                                       double                      smoothing);
void                          ramerDouglasPeucker(const std::vector<Point2d>& pointList,
                                                  double                      epsilon,
                                                  std::vector<Point2d>&       out);

// Geometric calculations
Line   createPolarLine(Point2d start_point, double angle, double length);
Point2d threePointCircleCenter(Point2d p1, Point2d p2, Point2d p3);
double  measurePolarAngle(Point2d p1, Point2d p2);
double  measureArcCircumference(double start_angle, double end_angle, double radius);

// Intersection tests
bool linesIntersect(const Line& l1, const Line& l2);
bool lineIntersectsWithCircle(const Line& l, Point2d center, double radius);
bool pointIsInsidePolygon(const Path& polygon, Point2d point);
bool polygonIsInsidePolygon(const Path& polygon1, const Path& polygon2);

} // namespace geo

#endif
