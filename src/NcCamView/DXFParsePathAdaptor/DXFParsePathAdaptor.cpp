#include "DXFParsePathAdaptor.h"
#include "NanoCut.h"
#include "NcCamView/NcCamView.h"
#include "NcApp/NcApp.h"
#include <algorithm>
#include <cctype>
#include <cmath>
#include <loguru.hpp>

// ============================================================================
// NURBS Implementation for proper DXF spline handling
// ============================================================================

class NURBSCurve {
private:
  std::vector<Point2d> control_points;
  std::vector<double>  knots;
  std::vector<double>  weights;
  int                  degree;
  bool                 closed;

  // Cox-de Boor recursion formula for B-spline basis functions
  double basis_function(int i, int p, double u) const
  {
    if (p == 0) {
      return (u >= knots[i] && u < knots[i + 1]) ? 1.0 : 0.0;
    }

    double left = 0.0, right = 0.0;
    double denom1 = knots[i + p] - knots[i];
    double denom2 = knots[i + p + 1] - knots[i + 1];

    if (denom1 > 1e-10) {
      left = ((u - knots[i]) / denom1) * basis_function(i, p - 1, u);
    }

    if (denom2 > 1e-10) {
      right =
        ((knots[i + p + 1] - u) / denom2) * basis_function(i + 1, p - 1, u);
    }

    return left + right;
  }

  // NURBS basis function (rational B-spline)
  double nurbs_basis(int i, int p, double u) const
  {
    double numerator = weights[i] * basis_function(i, p, u);
    double denominator = 0.0;

    for (size_t j = 0; j < control_points.size(); j++) {
      denominator += weights[j] * basis_function(j, p, u);
    }

    return (denominator > 1e-10) ? (numerator / denominator) : 0.0;
  }

public:
  NURBSCurve() : degree(3), closed(false) {}

  void setDegree(int d) { degree = d; }
  void setClosed(bool c) { closed = c; }

  void addControlPoint(Point2d p, double weight = 1.0)
  {
    control_points.push_back(p);
    weights.push_back(weight);
  }

  void addKnot(double k) { knots.push_back(k); }

  void clear()
  {
    control_points.clear();
    knots.clear();
    weights.clear();
  }

  bool is_valid() const
  {
    if (control_points.size() < 2)
      return false;
    // Knot vector should have size: control_points + degree + 1
    return knots.size() == control_points.size() + degree + 1;
  }

  void generate_uniform_knots()
  {
    knots.clear();
    int n = control_points.size();
    int m = n + degree + 1;

    // Clamped knot vector (standard for open curves)
    for (int i = 0; i < m; i++) {
      if (i <= degree) {
        knots.push_back(0.0);
      }
      else if (i >= n) {
        knots.push_back(1.0);
      }
      else {
        knots.push_back((double) (i - degree) / (n - degree));
      }
    }
  }

  int find_span(double u) const
  {
    int n = control_points.size() - 1;
    int p = degree;

    // Handle boundary case
    if (u >= knots[n + 1])
      return n;

    // Binary search for the interval
    int low = p;
    int high = n + 1;
    int mid = (low + high) / 2;

    while (u < knots[mid] || u >= knots[mid + 1]) {
      if (u < knots[mid])
        high = mid;
      else
        low = mid;
      mid = (low + high) / 2;
    }
    return mid;
  }

  void basis_funcs(int span, double u, std::vector<double>& N) const
  {
    int p = degree;
    N.assign(p + 1, 0.0);

    std::vector<double> left(p + 1), right(p + 1);
    N[0] = 1.0;

    for (int j = 1; j <= p; j++) {
      left[j] = u - knots[span + 1 - j];
      right[j] = knots[span + j] - u;

      double saved = 0.0;
      for (int r = 0; r < j; r++) {
        double temp = N[r] / (right[r + 1] + left[j - r]);
        N[r] = saved + right[r + 1] * temp;
        saved = left[j - r] * temp;
      }
      N[j] = saved;
    }
  }

  // Evaluate curve at parameter u
  Point2d evaluate(double u) const
  {
    Point2d result{ 0.0, 0.0 };
    int     p = degree;

    // Find the active span
    int span = find_span(u);

    // Compute only p+1 basis functions
    std::vector<double> N;
    basis_funcs(span, u, N);

    // Apply only the p+1 contributing control points
    int start = span - p;
    for (int j = 0; j <= p; j++) {
      const auto& cp = control_points[start + j];
      double      Nj = N[j];

      result.x += cp.x * Nj;
      result.y += cp.y * Nj;
    }

    return result;
  }

  // Calculate curvature at parameter u
  double calculate_curvature(double u) const
  {
    const double eps = 1e-6;

    // First derivative (tangent)
    Point2d p1 = evaluate(u - eps);
    Point2d p2 = evaluate(u + eps);
    double  dx = (p2.x - p1.x) / (2.0 * eps);
    double  dy = (p2.y - p1.y) / (2.0 * eps);

    // Second derivative
    Point2d p0 = evaluate(u);
    double  d2x = (p2.x - 2.0 * p0.x + p1.x) / (eps * eps);
    double  d2y = (p2.y - 2.0 * p0.y + p1.y) / (eps * eps);

    // Curvature formula: |x'y'' - y'x''| / (x'^2 + y'^2)^(3/2)
    double numerator = fabs(dx * d2y - dy * d2x);
    double denominator = pow(dx * dx + dy * dy, 1.5);

    return (denominator > 1e-10) ? (numerator / denominator) : 0.0;
  }

  // Adaptive sampling based on curvature
  std::vector<Point2d> sampleAdaptive(double max_error, int max_depth)
  {
    std::vector<Point2d> points;

    if (!is_valid()) {
      generate_uniform_knots();
      if (!is_valid()) {
        LOG_F(WARNING, "Invalid NURBS curve, returning control points");
        return control_points;
      }
    }

    double u_start = knots[degree];
    double u_end = knots[control_points.size()];

    points.push_back(evaluate(u_start));
    adaptiveSampleSegment(u_start, u_end, max_error, 0, max_depth, points);
    points.push_back(evaluate(u_end));

    return points;
  }

private:
  void adaptiveSampleSegment(double                u1,
                             double                u2,
                             double                max_error,
                             int                   depth,
                             int                   max_depth,
                             std::vector<Point2d>& points)
  {
    double u_mid = (u1 + u2) * 0.5;

    Point2d p1 = evaluate(u1);
    Point2d p2 = evaluate(u2);
    Point2d p_mid = evaluate(u_mid);

    // Linear interpolation between p1 and p2
    Point2d p_linear;
    p_linear.x = (p1.x + p2.x) * 0.5;
    p_linear.y = (p1.y + p2.y) * 0.5;

    // Calculate error (distance between actual midpoint and linear
    // approximation)
    double dx = p_mid.x - p_linear.x;
    double dy = p_mid.y - p_linear.y;
    double error = sqrt(dx * dx + dy * dy);

    // Also consider curvature to force subdivision in high-curvature areas
    double curvature = calculate_curvature(u_mid);
    double curvature_threshold = SCALE(0.001); // Adjust based on your needs

    if ((error > max_error || curvature > curvature_threshold) &&
        depth < max_depth) {
      // Subdivide
      adaptiveSampleSegment(u1, u_mid, max_error, depth + 1, max_depth, points);
      points.push_back(p_mid);
      adaptiveSampleSegment(u_mid, u2, max_error, depth + 1, max_depth, points);
    }
  }

public:
  size_t getControlPointCount() const { return control_points.size(); }
};

// ============================================================================
// Helper class for fit point based spline approximation
// ============================================================================

class FitPointSpline {
private:
  std::vector<Point2d> fit_points;
  int                  degree;

  // Calculate distance between point and line segment
  double pointToSegmentDistance(const Point2d& p,
                                const Point2d& a,
                                const Point2d& b) const
  {
    double dx = b.x - a.x;
    double dy = b.y - a.y;
    double len_sq = dx * dx + dy * dy;

    if (len_sq < 1e-10) {
      dx = p.x - a.x;
      dy = p.y - a.y;
      return sqrt(dx * dx + dy * dy);
    }

    double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len_sq;
    t = std::max(0.0, std::min(1.0, t));

    double proj_x = a.x + t * dx;
    double proj_y = a.y + t * dy;

    dx = p.x - proj_x;
    dy = p.y - proj_y;
    return sqrt(dx * dx + dy * dy);
  }

  // Evaluate Catmull-Rom at parameter t
  Point2d evalCatmullRom(const Point2d& p0,
                         const Point2d& p1,
                         const Point2d& p2,
                         const Point2d& p3,
                         double         t) const
  {
    double t2 = t * t;
    double t3 = t2 * t;

    Point2d pt;
    pt.x = 0.5 * ((2 * p1.x) + (-p0.x + p2.x) * t +
                  (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
                  (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
    pt.y = 0.5 * ((2 * p1.y) + (-p0.y + p2.y) * t +
                  (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
                  (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
    return pt;
  }

  // Adaptive subdivision of a segment
  void adaptiveSubdivide(const Point2d&        p0,
                         const Point2d&        p1,
                         const Point2d&        p2,
                         const Point2d&        p3,
                         double                t_start,
                         double                t_end,
                         double                max_error,
                         int                   depth,
                         int                   max_depth,
                         std::vector<Point2d>& result) const
  {
    double t_mid = (t_start + t_end) * 0.5;

    Point2d p_start = evalCatmullRom(p0, p1, p2, p3, t_start);
    Point2d p_end = evalCatmullRom(p0, p1, p2, p3, t_end);
    Point2d p_mid = evalCatmullRom(p0, p1, p2, p3, t_mid);

    // Check if midpoint deviates too much from linear interpolation
    double error = pointToSegmentDistance(p_mid, p_start, p_end);

    if (error > max_error && depth < max_depth) {
      // Subdivide further
      adaptiveSubdivide(p0,
                        p1,
                        p2,
                        p3,
                        t_start,
                        t_mid,
                        max_error,
                        depth + 1,
                        max_depth,
                        result);
      result.push_back(p_mid);
      adaptiveSubdivide(
        p0, p1, p2, p3, t_mid, t_end, max_error, depth + 1, max_depth, result);
    }
  }

public:
  FitPointSpline() : degree(3) {}

  void addFitPoint(Point2d p) { fit_points.push_back(p); }

  void clear() { fit_points.clear(); }

  // Adaptive Catmull-Rom spline interpolation
  std::vector<Point2d> interpolateAdaptive(double max_error, int max_depth)
  {
    if (fit_points.size() < 2)
      return fit_points;
    if (fit_points.size() == 2)
      return fit_points;

    std::vector<Point2d> result;
    result.push_back(fit_points[0]);

    for (size_t i = 0; i < fit_points.size() - 1; i++) {
      Point2d p0 = (i > 0) ? fit_points[i - 1] : fit_points[i];
      Point2d p1 = fit_points[i];
      Point2d p2 = fit_points[i + 1];
      Point2d p3 =
        (i + 2 < fit_points.size()) ? fit_points[i + 2] : fit_points[i + 1];

      // Start point
      Point2d p_start = evalCatmullRom(p0, p1, p2, p3, 0.0);
      result.push_back(p_start);

      // Adaptive subdivision
      adaptiveSubdivide(
        p0, p1, p2, p3, 0.0, 1.0, max_error, 0, max_depth, result);

      // End point
      Point2d p_end = evalCatmullRom(p0, p1, p2, p3, 1.0);
      result.push_back(p_end);
    }

    return result;
  }

  size_t size() const { return fit_points.size(); }
};

// ============================================================================
// DXFParsePathAdaptor implementation
// ============================================================================

/**
 * Default constructor.
 */
DXFParsePathAdaptor::DXFParsePathAdaptor(
  NcRender*                       nc_render_instance,
  std::function<void(Primitive*)> view_callback,
  std::function<void(Primitive*, const Primitive::MouseEventData&)>
             mouse_callback,
  NcCamView* cam_view)
{
  m_filename = "";
  m_nc_render_instance = nc_render_instance;
  m_view_callback = view_callback;
  m_mouse_callback = mouse_callback;
  m_cam_view = cam_view;
  m_smoothing = SCALE(0.25);
  m_import_scale = 1.0;
  m_chain_tolerance = SCALE(0.25);
}

void DXFParsePathAdaptor::setImportScale(double scale)
{
  m_import_scale = scale;
}

void DXFParsePathAdaptor::setImportQuality(int quality)
{
  m_import_quality = quality;
}

void DXFParsePathAdaptor::setSmoothing(float smoothing)
{
  m_smoothing = smoothing;
}

void DXFParsePathAdaptor::setFilename(std::string f) { m_filename = f; }

void DXFParsePathAdaptor::setChainTolerance(double chain_tolerance)
{
  m_chain_tolerance = chain_tolerance;
}

void DXFParsePathAdaptor::setProgressTracking(std::atomic<float>* progress)
{
  m_progress_ptr = progress;
}

void DXFParsePathAdaptor::getApproxBoundingBox(Point2d& bbox_min,
                                               Point2d& bbox_max,
                                               size_t&  vertex_count)
{
  vertex_count = 0;
  bbox_max.x = -std::numeric_limits<double>::infinity();
  bbox_max.y = -std::numeric_limits<double>::infinity();
  bbox_min.x = std::numeric_limits<double>::infinity();
  bbox_min.y = std::numeric_limits<double>::infinity();

  auto visit = [&](Point2d point) {
    bbox_min.x = std::min(point.x, bbox_min.x);
    bbox_max.x = std::max(point.x, bbox_max.x);
    bbox_min.y = std::min(point.y, bbox_min.y);
    bbox_max.y = std::max(point.y, bbox_max.y);
    vertex_count++;
  };

  // Visit all geometry across all layers
  for (const auto& [layer_name, layer_data] : m_layers) {
    for (const auto& line : layer_data.lines) {
      visit(line.start);
      visit(line.end);
    }

    for (const auto& polyline : layer_data.polylines) {
      for (const auto& vertex : polyline.points) {
        visit(vertex.point);
      }
    }

    for (const auto& spline : layer_data.splines) {
      for (const auto& point : spline.control_points) {
        visit(point);
      }
      for (const auto& point : spline.fit_points) {
        visit(point);
      }
    }
  }
}

void DXFParsePathAdaptor::getBoundingBox(
  const std::vector<std::vector<Point2d>>& path_stack,
  Point2d&                                 bbox_min,
  Point2d&                                 bbox_max)
{
  bbox_max.x = -std::numeric_limits<double>::infinity();
  bbox_max.y = -std::numeric_limits<double>::infinity();
  bbox_min.x = std::numeric_limits<double>::infinity();
  bbox_min.y = std::numeric_limits<double>::infinity();

  for (const auto& path : path_stack) {
    for (const auto& point : path) {
      bbox_min.x = std::min(point.x, bbox_min.x);
      bbox_max.x = std::max(point.x, bbox_max.x);
      bbox_min.y = std::min(point.y, bbox_min.y);
      bbox_max.y = std::max(point.y, bbox_max.y);
    }
  }
}

bool DXFParsePathAdaptor::checkIfPointIsInsidePath(std::vector<Point2d> path,
                                                   Point2d              point)
{
  // Delegate to geo namespace
  return geo::pointIsInsidePolygon(path, point);
}

bool DXFParsePathAdaptor::checkIfPathIsInsidePath(std::vector<Point2d> path1,
                                                  std::vector<Point2d> path2)
{
  // Check if ANY point of path1 is inside path2 (partial containment)
  // Note: For full containment, use geo::polygonIsInsidePolygon
  for (const auto& pt : path1) {
    if (geo::pointIsInsidePolygon(path2, pt)) {
      return true;
    }
  }
  return false;
}

struct PointHash {
  std::size_t operator()(const std::pair<int64_t, int64_t>& p) const
  {
    return std::hash<int64_t>()(p.first) ^
           (std::hash<int64_t>()(p.second) << 1);
  }
};

std::vector<std::vector<Point2d>>
DXFParsePathAdaptor::chainify(const std::vector<DxfLine>& haystack,
                              double                      tolerance)
{
  if (haystack.empty()) {
    return {};
  }

  if (m_progress_ptr)
    m_progress_ptr->store(0.f);

  std::vector<std::vector<Point2d>> contours;

  // Spatial hash grid size based on tolerance
  const double grid_size = tolerance * 2.0;
  const double inv_grid_size = 1.0 / grid_size;

  // Lambda to convert point to grid cell
  auto point_to_cell =
    [inv_grid_size](const Point2d& p) -> std::pair<int64_t, int64_t> {
    return { static_cast<int64_t>(std::floor(p.x * inv_grid_size)),
             static_cast<int64_t>(std::floor(p.y * inv_grid_size)) };
  };

  // Build spatial hash map: grid cell -> list of line indices with endpoints in
  // that cell
  std::
    unordered_map<std::pair<int64_t, int64_t>, std::vector<size_t>, PointHash>
      start_map;
  std::
    unordered_map<std::pair<int64_t, int64_t>, std::vector<size_t>, PointHash>
      end_map;

  for (size_t i = 0; i < haystack.size(); ++i) {
    auto start_cell = point_to_cell(haystack[i].start);
    auto end_cell = point_to_cell(haystack[i].end);

    start_map[start_cell].push_back(i);
    end_map[end_cell].push_back(i);
  }

  // Track which lines have been used
  std::vector<bool> used(haystack.size(), false);

  // Lambda to check if a point is shared with any line in haystack (excluding
  // specific index)
  auto is_point_shared = [&](const Point2d& p, size_t exclude_idx) -> bool {
    auto cell = point_to_cell(p);

    // Check 3x3 grid of cells
    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        auto check_cell = std::make_pair(cell.first + dx, cell.second + dy);

        // Check start points
        auto it_start = start_map.find(check_cell);
        if (it_start != start_map.end()) {
          for (size_t idx : it_start->second) {
            if (idx != exclude_idx && !used[idx] &&
                geo::distance(p, haystack[idx].start) < tolerance) {
              return true;
            }
          }
        }

        // Check end points
        auto it_end = end_map.find(check_cell);
        if (it_end != end_map.end()) {
          for (size_t idx : it_end->second) {
            if (idx != exclude_idx && !used[idx] &&
                geo::distance(p, haystack[idx].end) < tolerance) {
              return true;
            }
          }
        }
      }
    }
    return false;
  };

  // Lambda to find next connecting line - MUST maintain original search order
  auto find_next_line = [&](const Point2d& p1) -> int {
    auto cell = point_to_cell(p1);

    // Collect all candidates from spatial hash
    std::vector<size_t> candidates;

    for (int dx = -1; dx <= 1; ++dx) {
      for (int dy = -1; dy <= 1; ++dy) {
        auto check_cell = std::make_pair(cell.first + dx, cell.second + dy);

        auto it_start = start_map.find(check_cell);
        if (it_start != start_map.end()) {
          for (size_t idx : it_start->second) {
            if (!used[idx]) {
              candidates.push_back(idx);
            }
          }
        }

        auto it_end = end_map.find(check_cell);
        if (it_end != end_map.end()) {
          for (size_t idx : it_end->second) {
            if (!used[idx]) {
              candidates.push_back(idx);
            }
          }
        }
      }
    }

    // Remove duplicates and sort to maintain order
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());

    // Find the CLOSEST match within tolerance, not just the first
    Point2d p2;
    int     best_idx = -1;
    double  best_dist = tolerance;

    for (size_t idx : candidates) {
      // Check start point
      p2.x = haystack[idx].start.x;
      p2.y = haystack[idx].start.y;
      double dist_start = geo::distance(p1, p2);
      if (dist_start < best_dist) {
        best_dist = dist_start;
        best_idx = idx;
      }

      // Check end point
      p2.x = haystack[idx].end.x;
      p2.y = haystack[idx].end.y;
      double dist_end = geo::distance(p1, p2);
      if (dist_end < best_dist) {
        best_dist = dist_end;
        best_idx = idx;
      }
    }

    return best_idx;
  };

  // Process all lines
  size_t       processed = 0;
  const size_t total = haystack.size();

  for (size_t start_idx = 0; start_idx < haystack.size(); ++start_idx) {
    if (used[start_idx])
      continue;

    m_progress_ptr->store(static_cast<float>(processed) /
                          static_cast<float>(total));

    std::vector<Point2d> chain;
    Point2d              point;

    const DxfLine& first = haystack[start_idx];

    // Check if end is shared BEFORE marking this line as used
    if (is_point_shared(first.end, start_idx)) {
      point.x = first.start.x;
      point.y = first.start.y;
      chain.push_back(point);
      point.x = first.end.x;
      point.y = first.end.y;
      chain.push_back(point);
    }
    else {
      point.x = first.end.x;
      point.y = first.end.y;
      chain.push_back(point);
      point.x = first.start.x;
      point.y = first.start.y;
      chain.push_back(point);
    }

    used[start_idx] = true;
    processed++;

    // Extend chain
    bool didSomething;
    do {
      didSomething = false;
      Point2d current_point = chain.back();
      Point2d p1, p2;
      p1.x = current_point.x;
      p1.y = current_point.y;

      // Find next line in order
      int next_idx = find_next_line(p1);

      if (next_idx >= 0) {
        // Check which end matches
        p2.x = haystack[next_idx].start.x;
        p2.y = haystack[next_idx].start.y;

        if (geo::distance(p1, p2) < tolerance) {
          // Start matches, add end
          point.x = haystack[next_idx].end.x;
          point.y = haystack[next_idx].end.y;
          chain.push_back(point);
        }
        else {
          // End matches, add start
          point.x = haystack[next_idx].start.x;
          point.y = haystack[next_idx].start.y;
          chain.push_back(point);
        }

        used[next_idx] = true;
        processed++;
        didSomething = true;
      }
    } while (didSomething);

    contours.push_back(std::move(chain));
  }

  LOG_F(INFO, "Chaining complete: %zu contours", contours.size());
  return contours;
}

void DXFParsePathAdaptor::scaleAllPoints(double scale)
{
  LOG_F(INFO, "Scaling input by %.4f", scale);

  // Scale all geometry in all layers
  for (auto& [layer_name, layer_data] : m_layers) {
    // Scale lines
    for (auto& line : layer_data.lines) {
      line.start.x *= scale;
      line.start.y *= scale;
      line.end.x *= scale;
      line.end.y *= scale;
    }

    // Scale polylines
    for (auto& polyline : layer_data.polylines) {
      for (auto& vertex : polyline.points) {
        vertex.point.x *= scale;
        vertex.point.y *= scale;
        // Note: bulge is a dimensionless ratio (tan(angle/4)), not scaled
      }
    }

    // Scale splines
    for (auto& spline : layer_data.splines) {
      for (auto& p : spline.control_points) {
        p.x *= scale;
        p.y *= scale;
      }
      for (auto& p : spline.fit_points) {
        p.x *= scale;
        p.y *= scale;
      }
      // Note: knots are parameter values, not spatial coordinates - don't scale
      // them
    }
  }
}

void DXFParsePathAdaptor::finish()
{
  // Finalize any pending polyline
  if (m_current_polyline.points.size() > 0 && !m_skip_current_polyline) {
    m_layers[m_current_entity_layer].polylines.push_back(m_current_polyline);
    m_current_polyline.points.clear();
  }

  // Finalize any pending spline
  if ((m_current_spline.control_points.size() > 0 ||
       m_current_spline.fit_points.size() > 0) &&
      !m_skip_current_spline) {
    m_layers[m_current_entity_layer].splines.push_back(m_current_spline);
    m_current_spline = DxfSpline();
  }

  double scale = m_import_scale;
  switch (m_units) {
    case Units::Inch:
      scale *= SCALE(25.4);
      break;
    case Units::Millimeter:
      scale *= SCALE(1.0);
      break;
    case Units::Centimeter:
      scale *= SCALE(10.0);
      break;
    case Units::Meter:
      scale *= SCALE(1000.0);
      break;
    default:
      break;
  }
  scaleAllPoints(scale);

  // Process each layer's geometry independently
  for (auto& [layer_name, layer_data] : m_layers) {
    LOG_F(INFO,
          "(DXFParsePathAdaptor::Finish) Processing layer '%s': %lu splines, "
          "%lu polylines",
          layer_name.c_str(),
          layer_data.splines.size(),
          layer_data.polylines.size());

    // Process splines: convert to line segments and add to this layer's lines
    for (const auto& spline : layer_data.splines) {
      std::vector<Point2d> sampled_points;

      // Prioritize fit points if available
      if (spline.has_fit_points && spline.fit_points.size() > 0) {
        FitPointSpline fit_spline;
        for (const auto& pt : spline.fit_points) {
          fit_spline.addFitPoint(pt);
        }
        sampled_points =
          fit_spline.interpolateAdaptive(SCALE(0.75), m_import_quality);
      }
      // Use NURBS with control points
      else if (spline.control_points.size() > 0) {
        NURBSCurve nurbs;
        nurbs.setDegree(spline.degree);
        nurbs.setClosed(spline.is_closed);

        // Add control points with weights
        for (size_t i = 0; i < spline.control_points.size(); i++) {
          double weight = (i < spline.weights.size()) ? spline.weights[i] : 1.0;
          nurbs.addControlPoint(spline.control_points[i], weight);
        }

        // Add knot vector if available
        if (spline.has_knots && spline.knots.size() > 0) {
          for (double knot : spline.knots) {
            nurbs.addKnot(knot);
          }
        }
        sampled_points = nurbs.sampleAdaptive(SCALE(0.75), m_import_quality);
      }

      if (sampled_points.size() > 1) {
        // Create line segments and add directly to this layer
        for (size_t i = 1; i < sampled_points.size(); i++) {
          DxfLine line;
          line.start = sampled_points[i - 1];
          line.end = sampled_points[i];
          layer_data.lines.push_back(line);
        }

        // Close the spline if needed
        if (spline.is_closed && sampled_points.size() > 2) {
          DxfLine line;
          line.start = sampled_points.back();
          line.end = sampled_points.front();
          layer_data.lines.push_back(line);
        }
      }
    }
  }

  // Process polylines: convert bulges to arcs/lines and add to this layer's
  // lines
  for (auto& [layer_name, layer_data] : m_layers) {
    for (const auto& polyline : layer_data.polylines) {
      for (size_t y = 0; y < polyline.points.size() - 1; y++) {
        if (polyline.points[y].bulge != 0) {
          // Handle bulge (arc segment)
          Point2d bulgeStart = polyline.points[y].point;
          Point2d bulgeEnd = polyline.points[y + 1].point;

          Point2d midpoint = geo::midpoint(bulgeStart, bulgeEnd);
          double  distance = geo::distance(bulgeStart, midpoint);
          double  sagitta = polyline.points[y].bulge * distance;

          geo::Line bulgeLine = geo::createPolarLine(
            midpoint,
            geo::measurePolarAngle(bulgeStart, bulgeEnd) + 270,
            sagitta);
          Point2d arc_center =
            geo::threePointCircleCenter(bulgeStart, bulgeLine.end, bulgeEnd);

          double arc_endAngle, arc_startAngle = 0;
          if (sagitta > 0) {
            arc_endAngle = geo::measurePolarAngle(arc_center, bulgeEnd);
            arc_startAngle = geo::measurePolarAngle(arc_center, bulgeStart);
          }
          else {
            arc_endAngle = geo::measurePolarAngle(arc_center, bulgeStart);
            arc_startAngle = geo::measurePolarAngle(arc_center, bulgeEnd);
          }

          // Explode arc to lines and add directly to this layer
          double               radius = geo::distance(arc_center, bulgeStart);
          std::vector<Point2d> arc_points;
          Point2d              start, end;

          start.x =
            arc_center.x + (radius * cosf(arc_startAngle * M_PI / 180.0f));
          start.y =
            arc_center.y + (radius * sinf(arc_startAngle * M_PI / 180.0f));
          end.x = arc_center.x + (radius * cosf(arc_endAngle * M_PI / 180.0f));
          end.y = arc_center.y + (radius * sinf(arc_endAngle * M_PI / 180.0f));

          arc_points.push_back(start);

          double angle_span = arc_endAngle - arc_startAngle;
          if (angle_span < 0.0)
            angle_span += 360.0;

          int    num_segments = 100;
          double angle_increment = angle_span / num_segments;
          double angle_pointer = arc_startAngle + angle_increment;

          for (int i = 0; i < num_segments - 1; i++) {
            Point2d sweeper;
            sweeper.x =
              arc_center.x + (radius * cosf(angle_pointer * M_PI / 180.0f));
            sweeper.y =
              arc_center.y + (radius * sinf(angle_pointer * M_PI / 180.0f));
            angle_pointer += angle_increment;
            arc_points.push_back(sweeper);
          }

          arc_points.push_back(end);

          for (size_t i = 1; i < arc_points.size(); i++) {
            DxfLine line;
            line.start = arc_points[i - 1];
            line.end = arc_points[i];
            layer_data.lines.push_back(line);
          }
        }
        else {
          // Regular line segment
          DxfLine line;
          line.start = polyline.points[y].point;
          line.end = polyline.points[y + 1].point;
          layer_data.lines.push_back(line);
        }
      }

      // Check if polyline should be closed
      int     shared = 0;
      Point2d our_endpoint = polyline.points.back().point;
      Point2d our_startpoint = polyline.points.front().point;

      for (const auto& line : layer_data.lines) {
        if (geo::distance(line.start, our_endpoint) < m_chain_tolerance) {
          shared++;
        }
        if (geo::distance(line.start, our_startpoint) < m_chain_tolerance) {
          shared++;
        }
        if (geo::distance(line.end, our_startpoint) < m_chain_tolerance) {
          shared++;
        }
        if (geo::distance(line.end, our_endpoint) < m_chain_tolerance) {
          shared++;
        }
      }

      // Close polyline if appropriate
      if (shared == 2) {
        if (polyline.points.back().bulge == 0.0f) {
          // Simple line closure
          DxfLine line;
          line.start = polyline.points.back().point;
          line.end = polyline.points.front().point;
          layer_data.lines.push_back(line);
        }
        else {
          // Handle bulge on closing segment
          Point2d bulgeStart = polyline.points.back().point;
          Point2d bulgeEnd = polyline.points.front().point;

          Point2d midpoint = geo::midpoint(bulgeStart, bulgeEnd);
          double  distance = geo::distance(bulgeStart, midpoint);
          double  sagitta = polyline.points.back().bulge * distance;

          geo::Line bulgeLine = geo::createPolarLine(
            midpoint,
            geo::measurePolarAngle(bulgeStart, bulgeEnd) + 270,
            sagitta);
          Point2d arc_center =
            geo::threePointCircleCenter(bulgeStart, bulgeLine.end, bulgeEnd);

          double arc_endAngle, arc_startAngle = 0;
          if (sagitta > 0) {
            arc_endAngle = geo::measurePolarAngle(arc_center, bulgeEnd);
            arc_startAngle = geo::measurePolarAngle(arc_center, bulgeStart);
          }
          else {
            arc_endAngle = geo::measurePolarAngle(arc_center, bulgeStart);
            arc_startAngle = geo::measurePolarAngle(arc_center, bulgeEnd);
          }

          // Explode arc to lines
          double               radius = geo::distance(arc_center, bulgeStart);
          std::vector<Point2d> arc_points;
          Point2d              start, end;

          start.x =
            arc_center.x + (radius * cosf(arc_startAngle * M_PI / 180.0f));
          start.y =
            arc_center.y + (radius * sinf(arc_startAngle * M_PI / 180.0f));
          end.x = arc_center.x + (radius * cosf(arc_endAngle * M_PI / 180.0f));
          end.y = arc_center.y + (radius * sinf(arc_endAngle * M_PI / 180.0f));

          arc_points.push_back(start);

          double angle_span = arc_endAngle - arc_startAngle;
          if (angle_span < 0.0)
            angle_span += 360.0;

          int    num_segments = 100;
          double angle_increment = angle_span / num_segments;
          double angle_pointer = arc_startAngle + angle_increment;

          for (int i = 0; i < num_segments - 1; i++) {
            Point2d sweeper;
            sweeper.x =
              arc_center.x + (radius * cosf(angle_pointer * M_PI / 180.0f));
            sweeper.y =
              arc_center.y + (radius * sinf(angle_pointer * M_PI / 180.0f));
            angle_pointer += angle_increment;
            arc_points.push_back(sweeper);
          }

          arc_points.push_back(end);

          for (size_t i = 1; i < arc_points.size(); i++) {
            DxfLine line;
            line.start = arc_points[i - 1];
            line.end = arc_points[i];
            layer_data.lines.push_back(line);
          }
        }
      }
    }
  }

  // Collect all chains from all layers to calculate global bounding box
  std::vector<std::vector<Point2d>> all_chains;
  std::vector<std::string> chain_layers; // Track which layer each chain belongs
                                         // to

  for (const auto& [layer_name, layer_data] : m_layers) {
    if (layer_data.lines.empty())
      continue;

    LOG_F(INFO,
          "(DXFParsePathAdaptor::Finish) Chaining layer '%s': %lu lines",
          layer_name.c_str(),
          layer_data.lines.size());

    std::vector<std::vector<Point2d>> chains =
      chainify(layer_data.lines, m_chain_tolerance);

    // Add to combined list
    for (const auto& chain : chains) {
      all_chains.push_back(chain);
      chain_layers.push_back(layer_name);
    }
  }

  // Calculate global bounding box
  Point2d bb_min, bb_max;
  getBoundingBox(all_chains, bb_min, bb_max);

  // Create layer-organized structure
  std::unordered_map<std::string, Part::Layer> part_layers;

  // Create paths from all chains, organized by layer
  for (size_t i = 0; i < all_chains.size(); i++) {
    Part::path_t path;
    path.is_inside_contour = false;

    if (geo::distance(all_chains[i].front(), all_chains[i].back()) >
        m_chain_tolerance) {
      path.is_closed = false;
    }
    else {
      path.is_closed = true;
    }

    path.color = m_cam_view->m_app->getColor(m_cam_view->m_outside_contour_color);

    // Adjust coordinates relative to bounding box
    for (size_t j = 0; j < all_chains[i].size(); j++) {
      const double px = all_chains[i][j].x - bb_min.x;
      const double py = all_chains[i][j].y - bb_min.y;
      path.points.push_back({ px, py });
    }

    // Add path to appropriate layer
    const std::string& layer_name = chain_layers[i];
    part_layers[layer_name].paths.push_back(path);
  }

  // Determine inside/outside contours (check across ALL layers)
  // Create temporary flat list for containment checking
  struct PathRef {
    std::string  layer_name;
    size_t       path_index;
    bool         is_closed;
    geo::Extents bbox;
  };
  std::vector<PathRef> path_refs;

  for (auto& [layer_name, layer] : part_layers) {
    for (size_t i = 0; i < layer.paths.size(); i++) {
      PathRef ref;
      ref.layer_name = layer_name;
      ref.path_index = i;
      ref.is_closed = layer.paths[i].is_closed;
      ref.bbox = geo::calculateBoundingBox(layer.paths[i].points);

      path_refs.push_back(ref);
    }
  }

  for (size_t x = 0; x < path_refs.size(); x++) {
    auto& path_x =
      part_layers[path_refs[x].layer_name].paths[path_refs[x].path_index];

    for (size_t i = 0; i < path_refs.size(); i++) {
      if (i == x)
        continue;

      // Skip non-closed paths - they can't contain other paths
      if (!path_refs[i].is_closed)
        continue;

      // Bounding box pre-check: if path_x's bbox isn't inside path_i's bbox,
      // skip expensive check
      if (!geo::extentsContain(path_refs[i].bbox, path_refs[x].bbox)) {
        continue;
      }

      auto& path_i =
        part_layers[path_refs[i].layer_name].paths[path_refs[i].path_index];

      if (checkIfPathIsInsidePath(path_x.points, path_i.points)) {
        path_x.is_inside_contour = true;
        if (path_x.is_closed) {
          path_x.color = m_cam_view->m_app->getColor(m_cam_view->m_inside_contour_color);
        }
        else {
          path_x.color = m_cam_view->m_app->getColor(m_cam_view->m_open_contour_color);
        }
        break;
      }
    }
  }

  size_t total_paths = 0;
  for (const auto& [layer_name, layer] : part_layers) {
    total_paths += layer.paths.size();
  }

  LOG_F(INFO,
        "(DXFParsePathAdaptor::Finish) Created %lu paths from %lu layers",
        total_paths,
        part_layers.size());

  // Create the final primitive with layer-organized structure
  Part* p = m_nc_render_instance->pushPrimitive<Part>(m_filename,
                                                      std::move(part_layers));
  p->m_control.smoothing = m_smoothing;
  p->m_control.scale = 1.0;
  p->mouse_callback = m_mouse_callback;
  p->matrix_callback = m_view_callback;
}

void DXFParsePathAdaptor::explodeArcToLines(double cx,
                                            double cy,
                                            double r,
                                            double start_angle,
                                            double end_angle,
                                            double num_segments)
{
  std::vector<Point2d> pointList;
  Point2d              start;
  Point2d              sweeper;
  Point2d              end;

  start.x = cx + (r * cosf((start_angle) *M_PI / 180.0f));
  start.y = cy + (r * sinf((start_angle) *M_PI / 180.0f));
  end.x = cx + (r * cosf((end_angle) *M_PI / 180.0f));
  end.y = cy + (r * sinf((end_angle) *M_PI / 180.0f));

  pointList.push_back(Point2d{ start.x, start.y });

  // DXF arcs are always counter-clockwise from start_angle to end_angle
  double angle_span = end_angle - start_angle;
  if (angle_span < 0.0)
    angle_span += 360.0;

  double angle_increment = angle_span / num_segments;
  double angle_pointer = start_angle + angle_increment;

  // Generate num_segments-1 intermediate points (the last iteration would equal
  // end_angle)
  for (int i = 0; i < num_segments - 1; i++) {
    sweeper.x = cx + (r * cosf((angle_pointer) *M_PI / 180.0f));
    sweeper.y = cy + (r * sinf((angle_pointer) *M_PI / 180.0f));
    angle_pointer += angle_increment;
    pointList.push_back(Point2d{ sweeper.x, sweeper.y });
  }

  // Always add the exact end point
  pointList.push_back(Point2d{ end.x, end.y });

  for (size_t i = 1; i < pointList.size(); i++) {
    addLine(DL_LineData((double) pointList[i - 1].x,
                        (double) pointList[i - 1].y,
                        0,
                        (double) pointList[i].x,
                        (double) pointList[i].y,
                        0));
  }
}

void DXFParsePathAdaptor::explodeEllipseToLines(double cx,
                                                double cy,
                                                double mx,
                                                double my,
                                                double ratio,
                                                double start_angle,
                                                double end_angle,
                                                int    num_segments)
{
  std::vector<Point2d> pointList;

  // Calculate major and minor axis lengths
  double major_axis = sqrt(mx * mx + my * my);
  double minor_axis = major_axis * ratio;

  // Calculate rotation angle of the ellipse (angle of major axis)
  double rotation = atan2(my, mx);

  // Handle angle wrapping for partial ellipses
  // DXF ellipse angles are already in radians
  double angle_span = end_angle - start_angle;
  if (angle_span < 0.0)
    angle_span += 2.0 * M_PI;

  double angle_increment = angle_span / num_segments;

  // Generate points using parametric ellipse equations
  for (int i = 0; i <= num_segments; i++) {
    double t = start_angle + i * angle_increment;

    // Parametric point on ellipse (before rotation)
    double x_local = major_axis * cos(t);
    double y_local = minor_axis * sin(t);

    // Rotate and translate to world coordinates
    Point2d pt;
    pt.x = cx + x_local * cos(rotation) - y_local * sin(rotation);
    pt.y = cy + x_local * sin(rotation) + y_local * cos(rotation);

    pointList.push_back(pt);
  }

  // Convert to line segments
  for (size_t i = 1; i < pointList.size(); i++) {
    addLine(DL_LineData((double) pointList[i - 1].x,
                        (double) pointList[i - 1].y,
                        0,
                        (double) pointList[i].x,
                        (double) pointList[i].y,
                        0));
  }
}

// ============================================================================
// DXF Entity Handlers
// ============================================================================

bool DXFParsePathAdaptor::shouldSkipCurrentEntity()
{
  // Get current entity attributes (includes layer and linetype)
  DL_Attributes attribs = getAttributes();

  // Get layer name (before lowercase transform for flag lookup)
  std::string layer_name = attribs.getLayer();

  // Skip construction layer entities
  std::string layer_name_lower = layer_name;
  std::transform(layer_name_lower.begin(),
                 layer_name_lower.end(),
                 layer_name_lower.begin(),
                 ::tolower);
  if (layer_name_lower.find("constr") != std::string::npos) {
    return true;
  }

  std::string linetype = attribs.getLinetype();

  // Convert linetype to uppercase for case-insensitive comparison
  std::string linetype_upper = linetype;
  std::transform(linetype_upper.begin(),
                 linetype_upper.end(),
                 linetype_upper.begin(),
                 ::toupper);

  // For cutting applications, only process continuous lines
  // Accept CONTINUOUS and BYLAYER (which inherits from layer definition)
  // Skip everything else (DASHED, DOTTED, etc.)
  bool is_continuous =
    (linetype_upper == "CONTINUOUS" || linetype_upper == "BYLAYER");

  return !is_continuous;
}

void DXFParsePathAdaptor::addLayer(const DL_LayerData& data)
{
  LOG_F(INFO,
        "(DXFParsePathAdaptor::addLayer) Layer: %s (flags=%d)",
        data.name.c_str(),
        data.flags);

  // Store layer flags for later lookup
  m_layer_props[data.name] = { .flags = data.flags, .visible = true };
}

void DXFParsePathAdaptor::addPoint(const DL_PointData& data)
{
  LOG_F(INFO,
        "(DXFParsePathAdaptor::addPoint) Point at (%.2f, %.2f)",
        data.x,
        data.y);
}

void DXFParsePathAdaptor::addLine(const DL_LineData& data)
{
  if (shouldSkipCurrentEntity()) {
    return;
  }

  DL_Attributes attribs = getAttributes();
  std::string   layer = attribs.getLayer();

  DxfLine line;
  line.start = { data.x1, data.y1 };
  line.end = { data.x2, data.y2 };

  m_layers[layer].lines.push_back(line);
}

void DXFParsePathAdaptor::addXLine(const DL_XLineData& data)
{
  LOG_F(INFO,
        "(DXFParsePathAdaptor::addXline) XLine from (%.2f, %.2f) direction "
        "(%.2f, %.2f)",
        data.bx,
        data.by,
        data.dx,
        data.dy);
}

void DXFParsePathAdaptor::addArc(const DL_ArcData& data)
{
  if (shouldSkipCurrentEntity()) {
    return;
  }

  explodeArcToLines(
    data.cx, data.cy, data.radius, data.angle1, data.angle2, 100);
}

void DXFParsePathAdaptor::addCircle(const DL_CircleData& data)
{
  DL_Attributes attribs = getAttributes();
  if (shouldSkipCurrentEntity()) {
    return;
  }

  explodeArcToLines(data.cx, data.cy, data.radius, 0, 180, 100);
  explodeArcToLines(data.cx, data.cy, data.radius, 180, 360, 100);
}

void DXFParsePathAdaptor::addEllipse(const DL_EllipseData& data)
{
  if (shouldSkipCurrentEntity()) {
    return;
  }

  // Use proper ellipse rendering with major axis, minor axis ratio, and
  // rotation
  explodeEllipseToLines(data.cx,
                        data.cy,
                        data.mx,
                        data.my,
                        data.ratio,
                        data.angle1,
                        data.angle2,
                        100);
}

void DXFParsePathAdaptor::addPolyline(const DL_PolylineData& data)
{
  // Finalize any previous polyline first (only if not skipped)
  if (m_current_polyline.points.size() > 0 && !m_skip_current_polyline) {
    m_layers[m_current_entity_layer].polylines.push_back(m_current_polyline);
  }
  m_current_polyline.points.clear();
  m_current_polyline.is_closed = false;

  // Check if we should skip this polyline
  if (shouldSkipCurrentEntity()) {
    m_skip_current_polyline = true;
    return;
  }

  m_skip_current_polyline = false;
  m_current_entity_layer = getAttributes().getLayer();

  if ((data.flags & (1 << 0))) {
    m_current_polyline.is_closed = true;
  }
  else {
    m_current_polyline.is_closed = false;
  }
}

void DXFParsePathAdaptor::addVertex(const DL_VertexData& data)
{
  // Skip vertices if the current polyline is being skipped
  if (m_skip_current_polyline) {
    return;
  }

  DxfPolylineVertex vertex;
  vertex.point = { data.x, data.y };
  vertex.bulge = data.bulge;
  m_current_polyline.points.push_back(vertex);
}

void DXFParsePathAdaptor::addSpline(const DL_SplineData& data)
{
  // Finalize any previous spline first (only if not skipped)
  if ((m_current_spline.control_points.size() > 0 ||
       m_current_spline.fit_points.size() > 0) &&
      !m_skip_current_spline) {
    m_layers[m_current_entity_layer].splines.push_back(m_current_spline);
  }
  m_current_spline = DxfSpline();

  // Check if we should skip this spline
  if (shouldSkipCurrentEntity()) {
    m_skip_current_spline = true;
    return;
  }

  m_skip_current_spline = false;
  m_current_entity_layer = getAttributes().getLayer();

  // Set spline properties
  m_current_spline.degree = data.degree;
  m_current_spline.is_closed = (data.flags & (1 << 0)) != 0;

  // LOG_F(INFO,
  //       "(DXFParsePathAdaptor::addSpline) Spline degree=%d, closed=%d, "
  //       "nKnots=%d, nControl=%d, nFit=%d",
  //       data.degree,
  //       m_current_spline.is_closed,
  //       data.nKnots,
  //       data.nControl,
  //       data.nFit);
}

void DXFParsePathAdaptor::addControlPoint(const DL_ControlPointData& data)
{
  // Skip control points if the current spline is being skipped
  if (m_skip_current_spline) {
    return;
  }

  Point2d p;
  p.x = data.x;
  p.y = data.y;

  m_current_spline.control_points.push_back(p);
  m_current_spline.weights.push_back(data.w);

  // if (data.w != 1.0) {
  //   LOG_F(INFO,
  //         "(DXFParsePathAdaptor::addControlPoint) Control point (%.2f, %.2f)
  //         " "weight=%.2f", data.x, data.y, data.w);
  // }
}

void DXFParsePathAdaptor::addFitPoint(const DL_FitPointData& data)
{
  // Skip fit points if the current spline is being skipped
  if (m_skip_current_spline) {
    return;
  }

  Point2d p;
  p.x = data.x;
  p.y = data.y;

  m_current_spline.fit_points.push_back(p);
  m_current_spline.has_fit_points = true;

  // LOG_F(INFO,
  //       "(DXFParsePathAdaptor::addFitPoint) Fit point (%.2f, %.2f)",
  //       data.x,
  //       data.y);
}

void DXFParsePathAdaptor::addKnot(const DL_KnotData& data)
{
  // Skip knots if the current spline is being skipped
  if (m_skip_current_spline) {
    return;
  }

  m_current_spline.knots.push_back(data.k);
  m_current_spline.has_knots = true;

  // LOG_F(INFO, "(DXFParsePathAdaptor::addKnot) Knot value: %.4f", data.k);
}

void DXFParsePathAdaptor::addRay(const DL_RayData& data)
{
  LOG_F(INFO,
        "(DXFParsePathAdaptor::addRay) Not yet implemented! Ray info: from "
        "(%.2f, %.2f) direction (%.2f, "
        "%.2f)",
        data.bx,
        data.by,
        data.dx,
        data.dy);
}

void DXFParsePathAdaptor::setVariableInt(const std::string& var,
                                         int                value,
                                         int                code)
{
  // LOG_F(INFO, "setVariableInt %s", var.c_str());
  // LOG_F(INFO, "   Value = %d, code = %d", value, code);
  if (var == "$INSUNITS" && code == 70) {
    switch (static_cast<Units>(value)) {
      case Units::Inch:
        m_units = Units::Inch;
        break;
      case Units::Millimeter:
        m_units = Units::Millimeter;
        break;
      case Units::Centimeter:
        m_units = Units::Centimeter;
        break;
      case Units::Meter:
        m_units = Units::Meter;
        break;
      default:
        LOG_F(INFO,
              "(DXFParsePathAdaptor::setVariableInt) Unknown units. No "
              "conversion will take place");
        m_units = Units::None;
        break;
    }
  }
}
