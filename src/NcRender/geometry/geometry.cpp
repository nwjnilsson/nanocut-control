#include <cmath>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <unistd.h>
#include <utility>
#include <vector>

#include "clipper.h"
#include "geometry.h"
#include <algorithm>

using namespace std;

namespace geo {

/**********************
 * BASIC UTILITIES
 **********************/

bool between(double x, double min, double max)
{
  return x >= min && x <= max;
}

bool pointsMatch(Point2d p1, Point2d p2)
{
  float tolerance = 0.001;
  if (between(p1.x, p2.x - tolerance, p2.x + tolerance) &&
      between(p1.y, p2.y - tolerance, p2.y + tolerance)) {
    return true;
  }
  else {
    return false;
  }
}

double distance(Point2d p1, Point2d p2)
{
  double x = p1.x - p2.x;
  double y = p1.y - p2.y;
  return sqrtf(x * x + y * y);
}

Point2d midpoint(Point2d p1, Point2d p2)
{
  Point2d mid;
  mid.x = (p1.x + p2.x) / 2;
  mid.y = (p1.y + p2.y) / 2;
  return mid;
}

/**********************
 * TRANSFORMATIONS
 **********************/

Point2d rotatePoint(Point2d center, Point2d point, double angle)
{
  Point2d return_point;
  double  radians = (3.1415926f / 180.0f) * angle;
  double  cos_val = cosf(radians);
  double  sin_val = sinf(radians);
  return_point.x = (cos_val * (point.x - center.x)) +
                   (sin_val * (point.y - center.y)) + center.x;
  return_point.y = (cos_val * (point.y - center.y)) -
                   (sin_val * (point.x - center.x)) + center.y;
  return return_point;
}

Point2d mirrorPoint(Point2d point, const Line& line)
{
  double  dx, dy, a, b, x, y;
  Point2d p, p0, p1;
  p = point;
  p0 = line.start;
  p1 = line.end;

  dx = p1.x - p0.x;
  dy = p1.y - p0.y;
  a = (dx * dx - dy * dy) / (dx * dx + dy * dy);
  b = 2 * dx * dy / (dx * dx + dy * dy);
  x = a * (p.x - p0.x) + b * (p.y - p0.y) + p0.x;
  y = b * (p.x - p0.x) - a * (p.y - p0.y) + p0.y;
  return { x, y };
}

/**********************
 * GEOMETRIC CONVERSIONS
 **********************/

std::vector<Line> arcToLineSegments(const Arc& arc, int num_segments)
{
  vector<Point2d> pointList;
  Point2d         start;
  Point2d         sweeper;
  Point2d         end;

  start.x = arc.center.x +
            (arc.radius * cosf((arc.start_angle) * 3.1415926f / 180.0f));
  start.y = arc.center.y +
            (arc.radius * sinf((arc.start_angle) * 3.1415926f / 180.0f));
  end.x =
    arc.center.x + (arc.radius * cosf((arc.end_angle) * 3.1415926f / 180.0f));
  end.y =
    arc.center.y + (arc.radius * sinf((arc.end_angle) * 3.1415926 / 180.0f));

  pointList.push_back(Point2d{ start.x, start.y });

  double diff = std::max(arc.start_angle, arc.end_angle) -
                std::min(arc.start_angle, arc.end_angle);
  if (diff > 180)
    diff = 360 - diff;
  double angle_increment = diff / num_segments;
  double angle_pointer = arc.start_angle + angle_increment;

  for (int i = 0; i < num_segments; i++) {
    sweeper.x =
      arc.center.x + (arc.radius * cosf((angle_pointer) * 3.1415926f / 180.0f));
    sweeper.y =
      arc.center.y + (arc.radius * sinf((angle_pointer) * 3.1415926f / 180.0f));
    angle_pointer += angle_increment;
    pointList.push_back(Point2d{ sweeper.x, sweeper.y });
  }
  pointList.push_back(Point2d{ end.x, end.y });

  // Convert points to lines
  std::vector<Line> result;
  for (size_t i = 1; i < pointList.size(); i++) {
    result.push_back(Line{ pointList[i - 1], pointList[i] });
  }
  return result;
}

std::vector<Line> circleToLineSegments(const Circle& circle)
{
  vector<Point2d> pointList;
  for (int i = 0; i < 360; i++) {
    double theta = 2.0f * 3.1415926f * i / 360.0f;
    double tx = circle.radius * cosf(theta);
    double ty = circle.radius * sinf(theta);
    pointList.push_back(Point2d{ (tx + circle.center.x), (ty + circle.center.y) });
  }

  // Convert points to lines
  std::vector<Line> result;
  for (size_t i = 1; i < pointList.size(); i++) {
    result.push_back(Line{ pointList[i - 1], pointList[i] });
  }
  // Close the circle
  result.push_back(Line{ pointList[pointList.size() - 1], pointList[0] });
  return result;
}

std::vector<Line>
normalize(const std::vector<GeometryPrimitive>& geometry_stack)
{
  std::vector<Line> return_stack;
  for (const auto& prim : geometry_stack) {
    switch (prim.type) {
    case GeometryType::Line:
      return_stack.push_back(prim.line);
      break;
    case GeometryType::Arc: {
      auto line_segments = arcToLineSegments(prim.arc, 100);
      return_stack.insert(
        return_stack.end(), line_segments.begin(), line_segments.end());
      break;
    }
    case GeometryType::Circle: {
      auto line_segments = circleToLineSegments(prim.circle);
      return_stack.insert(
        return_stack.end(), line_segments.begin(), line_segments.end());
      break;
    }
    }
  }
  return return_stack;
}

/**********************
 * GEOMETRIC ANALYSIS
 **********************/

Extents getExtents(const std::vector<Line>& lines)
{
  Extents extents;
  extents.min.x = inf<double>();
  extents.min.y = inf<double>();
  extents.max.x = -inf<double>();
  extents.max.y = -inf<double>();

  for (const auto& line : lines) {
    if (line.start.x < extents.min.x)
      extents.min.x = line.start.x;
    if (line.start.y < extents.min.y)
      extents.min.y = line.start.y;
    if (line.end.x < extents.min.x)
      extents.min.x = line.end.x;
    if (line.end.y < extents.min.y)
      extents.min.y = line.end.y;

    if (line.start.x > extents.max.x)
      extents.max.x = line.start.x;
    if (line.start.y > extents.max.y)
      extents.max.y = line.start.y;
    if (line.end.x > extents.max.x)
      extents.max.x = line.end.x;
    if (line.end.y > extents.max.y)
      extents.max.y = line.end.y;
  }
  return extents;
}

Extents getExtents(const std::vector<GeometryPrimitive>& geometry_stack)
{
  Extents extents;
  extents.min.x = inf<double>();
  extents.min.y = inf<double>();
  extents.max.x = -inf<double>();
  extents.max.y = -inf<double>();

  for (const auto& prim : geometry_stack) {
    switch (prim.type) {
    case GeometryType::Line:
      if (prim.line.start.x < extents.min.x)
        extents.min.x = prim.line.start.x;
      if (prim.line.start.y < extents.min.y)
        extents.min.y = prim.line.start.y;
      if (prim.line.end.x < extents.min.x)
        extents.min.x = prim.line.end.x;
      if (prim.line.end.y < extents.min.y)
        extents.min.y = prim.line.end.y;

      if (prim.line.start.x > extents.max.x)
        extents.max.x = prim.line.start.x;
      if (prim.line.start.y > extents.max.y)
        extents.max.y = prim.line.start.y;
      if (prim.line.end.x > extents.max.x)
        extents.max.x = prim.line.end.x;
      if (prim.line.end.y > extents.max.y)
        extents.max.y = prim.line.end.y;
      break;
    case GeometryType::Arc:
    case GeometryType::Circle: {
      const Point2d& center =
        (prim.type == GeometryType::Arc) ? prim.arc.center : prim.circle.center;
      double radius =
        (prim.type == GeometryType::Arc) ? prim.arc.radius : prim.circle.radius;

      if (center.x - radius < extents.min.x)
        extents.min.x = center.x - radius;
      if (center.y - radius < extents.min.y)
        extents.min.y = center.y - radius;
      if (center.x + radius > extents.max.x)
        extents.max.x = center.x + radius;
      if (center.y + radius > extents.max.y)
        extents.max.y = center.y + radius;
      break;
    }
    }
  }
  return extents;
}

Extents calculateBoundingBox(const Path& path)
{
  Extents bbox;
  if (path.empty()) {
    bbox.min = {0.0, 0.0};
    bbox.max = {0.0, 0.0};
    return bbox;
  }

  bbox.min = path[0];
  bbox.max = path[0];

  for (const auto& pt : path) {
    bbox.min.x = std::min(bbox.min.x, pt.x);
    bbox.min.y = std::min(bbox.min.y, pt.y);
    bbox.max.x = std::max(bbox.max.x, pt.x);
    bbox.max.y = std::max(bbox.max.y, pt.y);
  }

  return bbox;
}

bool extentsContain(const Extents& outer, const Extents& inner)
{
  return inner.min.x >= outer.min.x && inner.max.x <= outer.max.x &&
         inner.min.y >= outer.min.y && inner.max.y <= outer.max.y;
}

/**********************
 * PATH OPERATIONS
 **********************/

std::vector<Contour> chainify(const std::vector<Line>& lines, double tolerance)
{
  std::vector<Contour> contours;
  std::vector<Line>    haystack = lines; // Make a copy

  while (haystack.size() > 0) {
    Contour chain;
    Line    first = haystack.back();
    haystack.pop_back();

    chain.push_back(first.start);
    chain.push_back(first.end);

    bool didSomething;
    do {
      didSomething = false;
      Point2d current_point = chain.back();

      for (size_t x = 0; x < haystack.size(); x++) {
        if (distance(current_point, haystack[x].start) < tolerance) {
          chain.push_back(haystack[x].end);
          haystack.erase(haystack.begin() + x);
          didSomething = true;
          break;
        }
        if (distance(current_point, haystack[x].end) < tolerance) {
          chain.push_back(haystack[x].start);
          haystack.erase(haystack.begin() + x);
          didSomething = true;
          break;
        }
      }
    } while (didSomething);

    if (chain.size() > 2) {
      contours.push_back(chain);
    }
  }
  return contours;
}

std::vector<Path> offset(const Path& path, double offset_value)
{
  double scale = 100.0f;
  vector<Point2d> pointList;

  for (const auto& pt : path) {
    pointList.push_back(Point2d{ pt.x * scale, pt.y * scale });
  }

  std::vector<Path>     ret;
  ClipperLib::Path      subj;
  ClipperLib::Paths     solution;

  for (const auto& pt : pointList) {
    subj << ClipperLib::FPoint(pt.x, pt.y);
  }

  ClipperLib::ClipperOffset co;
  co.AddPath(subj, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
  co.Execute(solution, offset_value * scale);
  ClipperLib::CleanPolygons(solution, 0.001 * scale);

  for (const auto& sol : solution) {
    Path result_path;
    Point2d first_point;
    for (size_t y = 0; y < sol.size(); y++) {
      if (y == 0) {
        first_point.x = double(sol[y].X) / scale;
        first_point.y = double(sol[y].Y) / scale;
      }
      result_path.push_back({ double(sol[y].X) / scale, double(sol[y].Y) / scale });
    }
    // Close the path
    result_path.push_back(first_point);
    ret.push_back(result_path);
  }
  return ret;
}

std::vector<Path> slot(const Path& path, double offset_value)
{
  double scale = 100.0f;
  vector<Point2d> pointList;

  for (const auto& pt : path) {
    pointList.push_back(Point2d{ pt.x * scale, pt.y * scale });
  }

  std::vector<Path>     ret;
  ClipperLib::Path      subj;
  ClipperLib::Paths     solution;

  for (const auto& pt : pointList) {
    subj << ClipperLib::FPoint(pt.x, pt.y);
  }

  ClipperLib::ClipperOffset co;
  co.AddPath(subj, ClipperLib::jtRound, ClipperLib::etOpenRound);
  co.Execute(solution, offset_value * scale);
  ClipperLib::CleanPolygons(solution, 0.001 * scale);

  for (const auto& sol : solution) {
    Path result_path;
    Point2d first_point;
    for (size_t y = 0; y < sol.size(); y++) {
      if (y == 0) {
        first_point.x = double(sol[y].X) / scale;
        first_point.y = double(sol[y].Y) / scale;
      }
      result_path.push_back({ double(sol[y].X) / scale, double(sol[y].Y) / scale });
    }
    // Close the path
    result_path.push_back(first_point);
    ret.push_back(result_path);
  }
  return ret;
}

std::vector<Point2d> simplify(const std::vector<Point2d>& points,
                              double                      smoothing)
{
  double          scale = 100.0f;
  vector<Point2d> pointList;
  vector<Point2d> pointListOut;
  vector<Point2d> out;

  for (const auto& pt : points) {
    pointList.push_back(Point2d{ pt.x * scale, pt.y * scale });
  }

  ramerDouglasPeucker(pointList, smoothing * scale, pointListOut);

  for (const auto& pt : pointListOut) {
    out.push_back({ pt.x / scale, pt.y / scale });
  }
  return out;
}

/**********************
 * HELPER FUNCTIONS
 **********************/

static double PerpendicularDistance(const Point2d& pt,
                                    const Point2d& lineStart,
                                    const Point2d& lineEnd)
{
  double dx = lineEnd.x - lineStart.x;
  double dy = lineEnd.y - lineStart.y;

  // Normalise
  double mag = pow(pow(dx, 2.0) + pow(dy, 2.0), 0.5);
  if (mag > 0.0) {
    dx /= mag;
    dy /= mag;
  }

  double pvx = pt.x - lineStart.x;
  double pvy = pt.y - lineStart.y;

  // Get dot product (project pv onto normalized direction)
  double pvdot = dx * pvx + dy * pvy;

  // Scale line direction vector
  double dsx = pvdot * dx;
  double dsy = pvdot * dy;

  // Subtract this from pv
  double ax = pvx - dsx;
  double ay = pvy - dsy;

  return pow(pow(ax, 2.0) + pow(ay, 2.0), 0.5);
}

void ramerDouglasPeucker(const vector<Point2d>& pointList,
                         double                 epsilon,
                         vector<Point2d>&       out)
{
  if (pointList.size() < 2)
    throw invalid_argument("Not enough points to simplify");

  // Find the point with the maximum distance from line between start and end
  double dmax = 0.0;
  size_t index = 0;
  size_t end = pointList.size() - 1;
  for (size_t i = 1; i < end; i++) {
    double d =
      PerpendicularDistance(pointList[i], pointList[0], pointList[end]);
    if (d > dmax) {
      index = i;
      dmax = d;
    }
  }

  // If max distance is greater than epsilon, recursively simplify
  if (dmax > epsilon) {
    // Recursive call
    vector<Point2d> recResults1;
    vector<Point2d> recResults2;
    vector<Point2d> xLine(pointList.begin(), pointList.begin() + index + 1);
    vector<Point2d> lastLine(pointList.begin() + index, pointList.end());
    ramerDouglasPeucker(xLine, epsilon, recResults1);
    ramerDouglasPeucker(lastLine, epsilon, recResults2);

    // Build the result list
    out.assign(recResults1.begin(), recResults1.end() - 1);
    out.insert(out.end(), recResults2.begin(), recResults2.end());
    if (out.size() < 2)
      throw runtime_error("Problem assembling output");
  }
  else {
    // Just return start and end points
    out.clear();
    out.push_back(pointList[0]);
    out.push_back(pointList[end]);
  }
}

/**********************
 * GEOMETRIC CALCULATIONS
 **********************/

Line createPolarLine(Point2d start_point, double angle, double length)
{
  Line ret;
  ret.start.x = start_point.x;
  ret.start.y = start_point.y;
  ret.end.x = start_point.x + length;
  ret.end.y = start_point.y;
  Point2d rotated_endpoint = rotatePoint(ret.start, ret.end, angle * -1);
  ret.end.x = rotated_endpoint.x;
  ret.end.y = rotated_endpoint.y;
  return ret;
}

Point2d threePointCircleCenter(Point2d p1, Point2d p2, Point2d p3)
{
  Point2d center;
  double  ax = (p1.x + p2.x) / 2;
  double  ay = (p1.y + p2.y) / 2;
  double  ux = (p1.y - p2.y);
  double  uy = (p2.x - p1.x);
  double  bx = (p2.x + p3.x) / 2;
  double  by = (p2.y + p3.y) / 2;
  double  vx = (p2.y - p3.y);
  double  vy = (p3.x - p2.x);
  double  dx = ax - bx;
  double  dy = ay - by;
  double  vu = vx * uy - vy * ux;
  if (vu == 0) {
    center.x = 0;
    center.y = 0;
    return center;
  }
  double g = (dx * uy - dy * ux) / vu;
  center.x = bx + g * vx;
  center.y = by + g * vy;
  return center;
}

double measurePolarAngle(Point2d p1, Point2d p2)
{
  double angle = (180 / 3.1415926f) * (atan2f(p1.y - p2.y, p1.x - p2.x));
  angle += 180;
  if (angle >= 360)
    angle -= 360;
  return angle;
}

double measureArcCircumference(double start_angle, double end_angle, double radius)
{
  double angle = std::max(start_angle, end_angle) - std::min(start_angle, end_angle);
  double circle_circumference = 2 * 3.1415926f * radius;
  return (angle / 360.0f) * circle_circumference;
}

/**********************
 * INTERSECTION TESTS
 **********************/

bool linesIntersect(const Line& l1, const Line& l2)
{
  Point2d p1 = l1.start;
  Point2d p2 = l1.end;
  Point2d p3 = l2.start;
  Point2d p4 = l2.end;

  float x1 = p1.x, x2 = p2.x, x3 = p3.x, x4 = p4.x;
  float y1 = p1.y, y2 = p2.y, y3 = p3.y, y4 = p4.y;

  float d = (x1 - x2) * (y3 - y4) - (y1 - y2) * (x3 - x4);
  // If d is zero, there is no intersection
  if (d == 0)
    return false;

  // Get the x and y
  float pre = (x1 * y2 - y1 * x2), post = (x3 * y4 - y3 * x4);
  float x = (pre * (x3 - x4) - (x1 - x2) * post) / d;
  float y = (pre * (y3 - y4) - (y1 - y2) * post) / d;

  // Check if the x and y coordinates are within both lines
  if (x < min(x1, x2) || x > max(x1, x2) || x < min(x3, x4) || x > max(x3, x4))
    return false;
  if (y < min(y1, y2) || y > max(y1, y2) || y < min(y3, y4) || y > max(y3, y4))
    return false;

  return true;
}

bool lineIntersectsWithCircle(const Line& l, Point2d center, double radius)
{
  double r, cx, cy, ax, ay, bx, by, a, b, c, disc, t1, t2, sqrtdisc;
  cx = center.x;
  cy = center.y;
  ax = l.start.x;
  ay = l.start.y;
  bx = l.end.x;
  by = l.end.y;
  r = radius;

  ax -= cx;
  ay -= cy;
  bx -= cx;
  by -= cy;
  a = powf((bx - ax), 2) + powf((by - ay), 2);
  b = 2 * (ax * (bx - ax) + ay * (by - ay));
  c = powf(ax, 2) + powf(ay, 2) - powf(r, 2);
  disc = powf(b, 2) - 4 * a * c;
  if (disc <= 0)
    return false;
  sqrtdisc = sqrtf(disc);
  t1 = (-b + sqrtdisc) / (2 * a);
  t2 = (-b - sqrtdisc) / (2 * a);
  if ((0 < t1 && t1 < 1) || (0 < t2 && t2 < 1))
    return true;
  return false;
}

static bool pointInPolygon(int                 polyCorners,
                           std::vector<double> polyX,
                           std::vector<double> polyY,
                           double              x,
                           double              y)
{
  int  i;
  int  j = polyCorners - 1;
  bool oddNodes = false;
  for (i = 0; i < polyCorners; i++) {
    if (((polyY[i] < y && polyY[j] >= y) || (polyY[j] < y && polyY[i] >= y)) &&
        (polyX[i] <= x || polyX[j] <= x)) {
      oddNodes ^= (polyX[i] + (y - polyY[i]) / (polyY[j] - polyY[i]) *
                                (polyX[j] - polyX[i]) <
                   x);
    }
    j = i;
  }
  return oddNodes;
}

bool pointIsInsidePolygon(const Path& polygon, Point2d point)
{
  std::vector<double> polyX;
  std::vector<double> polyY;

  for (const auto& pt : polygon) {
    polyX.push_back(pt.x);
    polyY.push_back(pt.y);
  }

  return pointInPolygon(polyX.size(), polyX, polyY, point.x, point.y);
}

bool polygonIsInsidePolygon(const Path& polygon1, const Path& polygon2)
{
  for (const auto& pt : polygon1) {
    if (!pointIsInsidePolygon(polygon2, pt))
      return false;
  }
  return true;
}

/**********************
 * POLYGON / LEAD HELPERS
 **********************/

double signedPolygonArea(const Path& path)
{
  if (path.size() < 3)
    return 0.0;
  double a = 0.0;
  const size_t n = path.size();
  for (size_t i = 0; i < n; ++i) {
    const Point2d& p = path[i];
    const Point2d& q = path[(i + 1) % n];
    a += p.x * q.y - q.x * p.y;
  }
  return a * 0.5;
}

double polygonArea(const Path& path, size_t begin, size_t end)
{
  if (end <= begin || end - begin < 3 || end > path.size())
    return 0.0;
  double a = 0.0;
  const size_t n = end - begin;
  for (size_t i = 0; i < n; ++i) {
    const Point2d& p = path[begin + i];
    const Point2d& q = path[begin + ((i + 1) % n)];
    a += p.x * q.y - q.x * p.y;
  }
  return std::fabs(a) * 0.5;
}

Point2d tangentAt(const Path& path, size_t segment_index, bool closed)
{
  if (path.size() < 2)
    return { 1.0, 0.0 };
  const size_t n = path.size();
  const size_t a = segment_index % n;
  const size_t b = closed ? (a + 1) % n : std::min(a + 1, n - 1);
  double dx = path[b].x - path[a].x;
  double dy = path[b].y - path[a].y;
  double mag = std::sqrt(dx * dx + dy * dy);
  if (mag <= 0.0)
    return { 1.0, 0.0 };
  return { dx / mag, dy / mag };
}

size_t longestStraightSegment(const Path& path,
                              double      min_length,
                              double      /*max_kink_deg*/,
                              bool        closed)
{
  const size_t n = path.size();
  if (n < 2)
    return SIZE_MAX;

  const size_t seg_count = closed ? n : n - 1;
  if (seg_count == 0)
    return SIZE_MAX;

  // We used to also reject segments whose endpoints met neighbours at
  // angles above `max_kink_deg`, but that excluded the long sides of
  // shrunken-inward offset polygons (which keep sharp 90° convex corners
  // because jtRound doesn't round convex corners under negative offset).
  // The attach point is placed mid-segment with the arc backward extent
  // bounded by `min_length >= 2*radius`, so the arc never crosses into
  // adjacent segments regardless of endpoint angles. Length alone is
  // a sufficient filter.
  size_t best = SIZE_MAX;
  double best_len = -1.0;

  for (size_t s = 0; s < seg_count; ++s) {
    const Point2d& a = path[s];
    const Point2d& b = path[(s + 1) % n];
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len = std::sqrt(dx * dx + dy * dy);
    if (len < min_length || len <= best_len)
      continue;

    best = s;
    best_len = len;
  }
  return best;
}

bool longestStraightRun(const Path&  path,
                        double       min_length,
                        double       dev_tolerance,
                        StraightRun* out)
{
  const size_t n = path.size();
  if (n < 2 || out == nullptr)
    return false;

  out->chord_length = -1.0;
  bool found = false;

  for (size_t i = 0; i < n; ++i) {
    // Extend j around the cyclic contour while all intermediate vertices
    // stay within dev_tolerance of the chord (path[i], path[j]).
    size_t j = (i + 1) % n;
    while (j != i) {
      const double dx = path[j].x - path[i].x;
      const double dy = path[j].y - path[i].y;
      const double chord_len = std::sqrt(dx * dx + dy * dy);
      if (chord_len < 1e-9) {
        // Degenerate chord (likely a closing duplicate vertex); stop
        // extending from this i but keep searching from the next.
        break;
      }

      // Verify every intermediate vertex (i, j) lies within the
      // tolerance of the chord. O(n) per j; O(n^3) total which is fine
      // for typical n in the low hundreds.
      bool   ok = true;
      size_t k = (i + 1) % n;
      while (k != j) {
        const double pvx = path[k].x - path[i].x;
        const double pvy = path[k].y - path[i].y;
        const double perp = std::fabs(dx * pvy - dy * pvx) / chord_len;
        if (perp > dev_tolerance) {
          ok = false;
          break;
        }
        k = (k + 1) % n;
      }
      if (!ok)
        break;

      if (chord_len >= min_length && chord_len > out->chord_length) {
        out->start = i;
        out->end = j;
        out->chord_length = chord_len;
        found = true;
      }
      j = (j + 1) % n;
    }
  }

  if (!found)
    out->chord_length = 0.0;
  return found;
}

std::vector<Point2d> buildArcLead(Point2d attach,
                                  Point2d tangent_dir,
                                  double  radius,
                                  double  sweep_deg,
                                  int     sweep_sign,
                                  int     segments)
{
  std::vector<Point2d> out;
  if (segments < 2 || radius <= 0.0)
    return out;

  // Center is 90 deg from tangent on the requested sweep side.
  // sweep_sign = +1 (CCW): normal = rotate(tangent, +90)  = (-ty, tx)
  // sweep_sign = -1 (CW):  normal = rotate(tangent, -90)  = ( ty,-tx)
  Point2d normal = sweep_sign >= 0
                     ? Point2d{ -tangent_dir.y, tangent_dir.x }
                     : Point2d{ tangent_dir.y, -tangent_dir.x };
  Point2d center = { attach.x + radius * normal.x,
                     attach.y + radius * normal.y };

  // attach is at angle theta_end from center; the arc starts at
  // theta_end - sweep_sign * sweep (so motion ends tangent at attach).
  double theta_end = std::atan2(attach.y - center.y, attach.x - center.x);
  double sweep_rad = sweep_deg * 3.1415926535897932 / 180.0;
  double theta_start = theta_end - sweep_sign * sweep_rad;

  out.reserve(segments + 1);
  for (int i = 0; i <= segments; ++i) {
    const double t = static_cast<double>(i) / segments;
    const double th = theta_start + (theta_end - theta_start) * t;
    out.push_back({ center.x + radius * std::cos(th),
                    center.y + radius * std::sin(th) });
  }
  // Snap last vertex exactly to attach.
  out.back() = attach;
  return out;
}

} // namespace geo
