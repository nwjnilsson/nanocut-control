#include "DXFParsePathAdaptor.h"
#include "application.h"
#include "jetCamView/jetCamView.h"
#include <EasyRender/logging/loguru.h>
#include <cmath>
#include <dxf/spline/Bezier.h>

// ============================================================================
// NURBS Implementation for proper DXF spline handling
// ============================================================================

class NURBSCurve {
private:
  std::vector<double_point_t> control_points;
  std::vector<double>         knots;
  std::vector<double>         weights;
  int                         degree;
  bool                        closed;

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

  void set_degree(int d) { degree = d; }
  void set_closed(bool c) { closed = c; }

  void add_control_point(double_point_t p, double weight = 1.0)
  {
    control_points.push_back(p);
    weights.push_back(weight);
  }

  void add_knot(double k) { knots.push_back(k); }

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
  double_point_t evaluate(double u) const
  {
    double_point_t result{ 0.0, 0.0 };
    int            p = degree;

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
    double_point_t p1 = evaluate(u - eps);
    double_point_t p2 = evaluate(u + eps);
    double         dx = (p2.x - p1.x) / (2.0 * eps);
    double         dy = (p2.y - p1.y) / (2.0 * eps);

    // Second derivative
    double_point_t p0 = evaluate(u);
    double         d2x = (p2.x - 2.0 * p0.x + p1.x) / (eps * eps);
    double         d2y = (p2.y - 2.0 * p0.y + p1.y) / (eps * eps);

    // Curvature formula: |x'y'' - y'x''| / (x'^2 + y'^2)^(3/2)
    double numerator = fabs(dx * d2y - dy * d2x);
    double denominator = pow(dx * dx + dy * dy, 1.5);

    return (denominator > 1e-10) ? (numerator / denominator) : 0.0;
  }

  // Adaptive sampling based on curvature
  std::vector<double_point_t> sample_adaptive(double max_error, int max_depth)
  {
    std::vector<double_point_t> points;

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
    adaptive_sample_segment(u_start, u_end, max_error, 0, max_depth, points);
    points.push_back(evaluate(u_end));

    return points;
  }

private:
  void adaptive_sample_segment(double                       u1,
                               double                       u2,
                               double                       max_error,
                               int                          depth,
                               int                          max_depth,
                               std::vector<double_point_t>& points)
  {
    double u_mid = (u1 + u2) * 0.5;

    double_point_t p1 = evaluate(u1);
    double_point_t p2 = evaluate(u2);
    double_point_t p_mid = evaluate(u_mid);

    // Linear interpolation between p1 and p2
    double_point_t p_linear;
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
      adaptive_sample_segment(
        u1, u_mid, max_error, depth + 1, max_depth, points);
      points.push_back(p_mid);
      adaptive_sample_segment(
        u_mid, u2, max_error, depth + 1, max_depth, points);
    }
  }

public:
  size_t get_control_point_count() const { return control_points.size(); }
};

// ============================================================================
// Helper class for fit point based spline approximation
// ============================================================================

class FitPointSpline {
private:
  std::vector<double_point_t> fit_points;
  int                         degree;

  // Calculate distance between point and line segment
  double point_to_segment_distance(const double_point_t& p,
                                   const double_point_t& a,
                                   const double_point_t& b) const
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
  double_point_t eval_catmull_rom(const double_point_t& p0,
                                  const double_point_t& p1,
                                  const double_point_t& p2,
                                  const double_point_t& p3,
                                  double                t) const
  {
    double t2 = t * t;
    double t3 = t2 * t;

    double_point_t pt;
    pt.x = 0.5 * ((2 * p1.x) + (-p0.x + p2.x) * t +
                  (2 * p0.x - 5 * p1.x + 4 * p2.x - p3.x) * t2 +
                  (-p0.x + 3 * p1.x - 3 * p2.x + p3.x) * t3);
    pt.y = 0.5 * ((2 * p1.y) + (-p0.y + p2.y) * t +
                  (2 * p0.y - 5 * p1.y + 4 * p2.y - p3.y) * t2 +
                  (-p0.y + 3 * p1.y - 3 * p2.y + p3.y) * t3);
    return pt;
  }

  // Adaptive subdivision of a segment
  void adaptive_subdivide(const double_point_t&        p0,
                          const double_point_t&        p1,
                          const double_point_t&        p2,
                          const double_point_t&        p3,
                          double                       t_start,
                          double                       t_end,
                          double                       max_error,
                          int                          depth,
                          int                          max_depth,
                          std::vector<double_point_t>& result) const
  {
    double t_mid = (t_start + t_end) * 0.5;

    double_point_t p_start = eval_catmull_rom(p0, p1, p2, p3, t_start);
    double_point_t p_end = eval_catmull_rom(p0, p1, p2, p3, t_end);
    double_point_t p_mid = eval_catmull_rom(p0, p1, p2, p3, t_mid);

    // Check if midpoint deviates too much from linear interpolation
    double error = point_to_segment_distance(p_mid, p_start, p_end);

    if (error > max_error && depth < max_depth) {
      // Subdivide further
      adaptive_subdivide(p0,
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
      adaptive_subdivide(
        p0, p1, p2, p3, t_mid, t_end, max_error, depth + 1, max_depth, result);
    }
  }

public:
  FitPointSpline() : degree(3) {}

  void add_fit_point(double_point_t p) { fit_points.push_back(p); }

  void clear() { fit_points.clear(); }

  // Adaptive Catmull-Rom spline interpolation
  std::vector<double_point_t> interpolate_adaptive(double max_error,
                                                   int    max_depth)
  {
    if (fit_points.size() < 2)
      return fit_points;
    if (fit_points.size() == 2)
      return fit_points;

    std::vector<double_point_t> result;
    result.push_back(fit_points[0]);

    for (size_t i = 0; i < fit_points.size() - 1; i++) {
      double_point_t p0 = (i > 0) ? fit_points[i - 1] : fit_points[i];
      double_point_t p1 = fit_points[i];
      double_point_t p2 = fit_points[i + 1];
      double_point_t p3 =
        (i + 2 < fit_points.size()) ? fit_points[i + 2] : fit_points[i + 1];

      // Start point
      double_point_t p_start = eval_catmull_rom(p0, p1, p2, p3, 0.0);
      result.push_back(p_start);

      // Adaptive subdivision
      adaptive_subdivide(
        p0, p1, p2, p3, 0.0, 1.0, max_error, 0, max_depth, result);

      // End point
      double_point_t p_end = eval_catmull_rom(p0, p1, p2, p3, 1.0);
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
DXFParsePathAdaptor::DXFParsePathAdaptor(EasyRender* easy_render_instance,
                                         void (*v)(PrimitiveContainer*),
                                         void (*m)(PrimitiveContainer*,
                                                   const nlohmann::json&))
{
  this->current_layer = "default";
  this->filename = "";
  this->easy_render_instance = easy_render_instance;
  this->view_callback = v;
  this->mouse_callback = m;
  this->smoothing = SCALE(0.25);
  this->import_scale = 1.0;
  this->chain_tolerance = SCALE(0.5);
}

void DXFParsePathAdaptor::SetImportScale(double scale)
{
  this->import_scale = scale;
}

void DXFParsePathAdaptor::SetImportQuality(int quality)
{
  this->import_quality = quality;
}

void DXFParsePathAdaptor::SetSmoothing(float smoothing)
{
  this->smoothing = smoothing;
}

void DXFParsePathAdaptor::SetFilename(std::string f) { this->filename = f; }

void DXFParsePathAdaptor::SetChainTolerance(double chain_tolerance)
{
  this->chain_tolerance = chain_tolerance;
}

void DXFParsePathAdaptor::GetApproxBoundingBox(double_point_t& bbox_min,
                                               double_point_t& bbox_max,
                                               size_t&         vertex_count)
{
  vertex_count = 0;
  bbox_max.x = -std::numeric_limits<double>::infinity();
  bbox_max.y = -std::numeric_limits<double>::infinity();
  bbox_min.x = std::numeric_limits<double>::infinity();
  bbox_min.y = std::numeric_limits<double>::infinity();

  auto visit = [&](double_point_t point) {
    bbox_min.x = std::min(point.x, bbox_min.x);
    bbox_max.x = std::max(point.x, bbox_max.x);
    bbox_min.y = std::min(point.y, bbox_min.y);
    bbox_max.y = std::max(point.y, bbox_max.y);
    vertex_count++;
  };

  for (const auto& line : line_stack) {
    visit(line.start);
    visit(line.end);
  }

  for (const auto& polyline : polylines) {
    for (const auto& vertex : polyline.points) {
      visit(vertex.point);
    }
  }

  for (const auto& spline : splines) {
    for (const auto& point : spline.control_points) {
      visit(point);
    }
    for (const auto& point : spline.fit_points) {
      visit(point);
    }
  }
}

void DXFParsePathAdaptor::GetBoundingBox(
  const std::vector<std::vector<double_point_t>>& path_stack,
  double_point_t&                                 bbox_min,
  double_point_t&                                 bbox_max)
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

bool DXFParsePathAdaptor::CheckIfPointIsInsidePath(
  std::vector<double_point_t> path, double_point_t point)
{
  size_t polyCorners = path.size();
  size_t j = polyCorners - 1;
  bool   oddNodes = false;

  for (size_t i = 0; i < polyCorners; i++) {
    if (((path[i].y < point.y && path[j].y >= point.y) ||
         (path[j].y < point.y && path[i].y >= point.y)) &&
        (path[i].x <= point.x || path[j].x <= point.x)) {
      oddNodes ^= (path[i].x + (point.y - path[i].y) / (path[j].y - path[i].y) *
                                 (path[j].x - path[i].x) <
                   point.x);
    }
    j = i;
  }
  return oddNodes;
}

bool DXFParsePathAdaptor::CheckIfPathIsInsidePath(
  std::vector<double_point_t> path1, std::vector<double_point_t> path2)
{
  for (std::vector<double_point_t>::iterator it = path1.begin();
       it != path1.end();
       ++it) {
    if (this->CheckIfPointIsInsidePath(path2, (*it))) {
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

std::vector<std::vector<double_point_t>>
DXFParsePathAdaptor::Chainify(std::vector<double_line_t> haystack,
                              double                     tolerance)
{
  if (haystack.empty()) {
    return {};
  }

  Geometry                                 g;
  std::vector<std::vector<double_point_t>> contours;

  // Spatial hash grid size based on tolerance
  const double grid_size = tolerance * 2.0;
  const double inv_grid_size = 1.0 / grid_size;

  // Lambda to convert point to grid cell
  auto point_to_cell =
    [inv_grid_size](const double_point_t& p) -> std::pair<int64_t, int64_t> {
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
  auto is_point_shared = [&](const double_point_t& p,
                             size_t                exclude_idx) -> bool {
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
                g.distance(p, haystack[idx].start) < tolerance) {
              return true;
            }
          }
        }

        // Check end points
        auto it_end = end_map.find(check_cell);
        if (it_end != end_map.end()) {
          for (size_t idx : it_end->second) {
            if (idx != exclude_idx && !used[idx] &&
                g.distance(p, haystack[idx].end) < tolerance) {
              return true;
            }
          }
        }
      }
    }
    return false;
  };

  // Lambda to find next connecting line - MUST maintain original search order
  auto find_next_line = [&](const double_point_t& p1) -> int {
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
              break;
            }
          }
        }

        auto it_end = end_map.find(check_cell);
        if (it_end != end_map.end()) {
          for (size_t idx : it_end->second) {
            if (!used[idx]) {
              candidates.push_back(idx);
              break;
            }
          }
        }
      }
    }

    // Remove duplicates and sort to maintain order
    std::sort(candidates.begin(), candidates.end());
    candidates.erase(std::unique(candidates.begin(), candidates.end()),
                     candidates.end());

    // Search in index order (matching original's for loop order)
    double_point_t p2;
    for (size_t idx : candidates) {
      // Check start point
      p2.x = haystack[idx].start.x;
      p2.y = haystack[idx].start.y;
      if (g.distance(p1, p2) < tolerance) {
        return idx;
      }

      // Check end point
      p2.x = haystack[idx].end.x;
      p2.y = haystack[idx].end.y;
      if (g.distance(p1, p2) < tolerance) {
        return idx;
      }
    }

    return -1;
  };

  // Process all lines
  size_t       processed = 0;
  const size_t total = haystack.size();

  for (size_t start_idx = 0; start_idx < haystack.size(); ++start_idx) {
    if (used[start_idx])
      continue;

    if (processed % 100 == 0) {
      LOG_F(INFO, "Chaining %u%%", (uint32_t) (100 * processed / total));
    }

    std::vector<double_point_t> chain;
    double_point_t              point;

    double_line_t first = haystack[start_idx];

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
      double_point_t current_point = chain.back();
      double_point_t p1, p2;
      p1.x = current_point.x;
      p1.y = current_point.y;

      // Find next line in order
      int next_idx = find_next_line(p1);

      if (next_idx >= 0) {
        // Check which end matches
        p2.x = haystack[next_idx].start.x;
        p2.y = haystack[next_idx].start.y;

        if (g.distance(p1, p2) < tolerance) {
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

void DXFParsePathAdaptor::ScaleAllPoints(double scale)
{
  LOG_F(INFO, "Scaling input by %.4f", scale);
  for (auto& pl : polylines) {
    for (auto& v : pl.points) {
      v.point.x *= scale;
      v.point.y *= scale;
      v.bulge *= scale;
    }
  }
  for (auto& s : splines) {
    for (auto& p : s.control_points) {
      p.x *= scale;
      p.y *= scale;
      p.z *= scale;
    }
    for (auto& p : s.fit_points) {
      p.x *= scale;
      p.y *= scale;
      p.z *= scale;
    }
    for (auto& k : s.knots) {
      k *= scale;
    }
  }
  for (auto& l : line_stack) {
    l.start.x *= scale;
    l.start.y *= scale;
    l.start.z *= scale;
    l.end.x *= scale;
    l.end.y *= scale;
    l.end.z *= scale;
  }
}

void DXFParsePathAdaptor::Finish()
{
  Geometry g = Geometry();

  // Finalize any pending polyline
  if (this->current_polyline.points.size() > 0) {
    this->polylines.push_back(this->current_polyline);
    this->current_polyline.points.clear();
  }

  // Finalize any pending spline
  if (this->current_spline.control_points.size() > 0 ||
      this->current_spline.fit_points.size() > 0) {
    this->splines.push_back(this->current_spline);
    this->current_spline = spline_t();
  }

  double scale = this->import_scale;
  switch (this->units) {
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
  ScaleAllPoints(scale);

  LOG_F(INFO,
        "(DXFParsePathAdaptor::Finish) Processing %lu splines",
        this->splines.size());

  // Process splines with proper NURBS/fit point handling
  for (size_t x = 0; x < this->splines.size(); x++) {
    std::vector<double_point_t> sampled_points;
    // ImGui::ProgressBar(x / (double) this->splines.size());

    // Prioritize fit points if available
    if (splines[x].has_fit_points && splines[x].fit_points.size() > 0) {
      // LOG_F(INFO, "Spline %lu: Using %lu fit points", x,
      // splines[x].fit_points.size());

      FitPointSpline fit_spline;
      for (const auto& pt : splines[x].fit_points) {
        fit_spline.add_fit_point(pt);
      }
      sampled_points =
        fit_spline.interpolate_adaptive(SCALE(0.75), import_quality);
    }
    // Use NURBS with control points
    else if (splines[x].control_points.size() > 0) {
      // LOG_F(INFO, "Spline %lu: Using %lu control points, %lu knots,
      // degree=%d",
      //       x, splines[x].control_points.size(), splines[x].knots.size(),
      //       splines[x].degree);

      NURBSCurve nurbs;
      nurbs.set_degree(splines[x].degree);
      nurbs.set_closed(splines[x].is_closed);

      // Add control points with weights
      for (size_t i = 0; i < splines[x].control_points.size(); i++) {
        double weight =
          (i < splines[x].weights.size()) ? splines[x].weights[i] : 1.0;
        nurbs.add_control_point(splines[x].control_points[i], weight);
      }

      // Add knot vector if available
      if (splines[x].has_knots && splines[x].knots.size() > 0) {
        for (double knot : splines[x].knots) {
          nurbs.add_knot(knot);
        }
      }
      sampled_points = nurbs.sample_adaptive(SCALE(0.75), import_quality);
    }

    if (sampled_points.size() > 1) {
      std::vector<Point> pointList;
      for (const auto& pt : sampled_points) {
        pointList.push_back(Point(pt.x, pt.y));
      }

      // Create line segments from simplified points
      for (size_t i = 1; i < pointList.size(); i++) {
        this->addLine(DL_LineData((double) pointList[i - 1].first,
                                  (double) pointList[i - 1].second,
                                  0,
                                  (double) pointList[i].first,
                                  (double) pointList[i].second,
                                  0));
      }

      // Close the spline if needed
      if (splines[x].is_closed && pointList.size() > 2) {
        this->addLine(DL_LineData((double) pointList.back().first,
                                  (double) pointList.back().second,
                                  0,
                                  (double) pointList[0].first,
                                  (double) pointList[0].second,
                                  0));
      }
    }
  }

  LOG_F(INFO,
        "(DXFParsePathAdaptor::Finish) Processing %lu polylines",
        this->polylines.size());

  // Process polylines with bulge handling
  for (int x = 0; x < this->polylines.size(); x++) {
    for (int y = 0; y < this->polylines[x].points.size() - 1; y++) {
      if (this->polylines[x].points[y].bulge != 0) {
        // Handle bulge (arc segment)
        double_point_t bulgeStart;
        bulgeStart.x = this->polylines[x].points[y].point.x;
        bulgeStart.y = this->polylines[x].points[y].point.y;

        double_point_t bulgeEnd;
        bulgeEnd.x = this->polylines[x].points[y + 1].point.x;
        bulgeEnd.y = this->polylines[x].points[y + 1].point.y;

        double_point_t midpoint = g.midpoint(bulgeStart, bulgeEnd);
        double         distance = g.distance(bulgeStart, midpoint);
        double         sagitta = this->polylines[x].points[y].bulge * distance;

        double_line_t bulgeLine = g.create_polar_line(
          midpoint, g.measure_polar_angle(bulgeStart, bulgeEnd) + 270, sagitta);
        double_point_t arc_center =
          g.three_point_circle_center(bulgeStart, bulgeLine.end, bulgeEnd);

        double arc_endAngle, arc_startAngle = 0;
        if (sagitta > 0) {
          arc_endAngle = g.measure_polar_angle(arc_center, bulgeEnd);
          arc_startAngle = g.measure_polar_angle(arc_center, bulgeStart);
        }
        else {
          arc_endAngle = g.measure_polar_angle(arc_center, bulgeStart);
          arc_startAngle = g.measure_polar_angle(arc_center, bulgeEnd);
        }

        this->addArc(DL_ArcData((double) arc_center.x,
                                (double) arc_center.y,
                                0,
                                g.distance(arc_center, bulgeStart),
                                arc_startAngle,
                                arc_endAngle));
      }
      else {
        // Regular line segment
        this->addLine(
          DL_LineData((double) this->polylines[x].points[y].point.x,
                      (double) this->polylines[x].points[y].point.y,
                      0,
                      (double) this->polylines[x].points[y + 1].point.x,
                      (double) this->polylines[x].points[y + 1].point.y,
                      0));
      }
    }

    // Check if polyline should be closed
    int            shared = 0;
    double_point_t our_endpoint = { this->polylines[x].points.back().point.x,
                                    this->polylines[x].points.back().point.y };
    double_point_t our_startpoint = {
      this->polylines[x].points.front().point.x,
      this->polylines[x].points.front().point.y
    };

    for (std::vector<double_line_t>::iterator it = this->line_stack.begin();
         it != this->line_stack.end();
         ++it) {
      if (g.distance(it->start, our_endpoint) < this->chain_tolerance) {
        shared++;
      }
      if (g.distance(it->start, our_startpoint) < this->chain_tolerance) {
        shared++;
      }
      if (g.distance(it->end, our_startpoint) < this->chain_tolerance) {
        shared++;
      }
      if (g.distance(it->end, our_endpoint) < this->chain_tolerance) {
        shared++;
      }
    }

    // Close polyline if appropriate
    if (shared == 2) {
      if (this->polylines[x].points.back().bulge == 0.0f) {
        this->addLine(
          DL_LineData((double) this->polylines[x].points.front().point.x,
                      (double) this->polylines[x].points.front().point.y,
                      0,
                      (double) this->polylines[x].points.back().point.x,
                      (double) this->polylines[x].points.back().point.y,
                      0));
      }
      else {
        // Handle bulge on closing segment
        double_point_t bulgeStart;
        bulgeStart.x = this->polylines[x].points.back().point.x;
        bulgeStart.y = this->polylines[x].points.back().point.y;

        double_point_t bulgeEnd;
        bulgeEnd.x = this->polylines[x].points.front().point.x;
        bulgeEnd.y = this->polylines[x].points.front().point.y;

        double_point_t midpoint = g.midpoint(bulgeStart, bulgeEnd);
        double         distance = g.distance(bulgeStart, midpoint);
        double sagitta = this->polylines[x].points.back().bulge * distance;

        double_line_t bulgeLine = g.create_polar_line(
          midpoint, g.measure_polar_angle(bulgeStart, bulgeEnd) + 270, sagitta);
        double_point_t arc_center =
          g.three_point_circle_center(bulgeStart, bulgeLine.end, bulgeEnd);

        double arc_endAngle, arc_startAngle = 0;
        if (sagitta > 0) {
          arc_endAngle = g.measure_polar_angle(arc_center, bulgeEnd);
          arc_startAngle = g.measure_polar_angle(arc_center, bulgeStart);
        }
        else {
          arc_endAngle = g.measure_polar_angle(arc_center, bulgeStart);
          arc_startAngle = g.measure_polar_angle(arc_center, bulgeEnd);
        }

        this->addArc(DL_ArcData((double) arc_center.x,
                                (double) arc_center.y,
                                0,
                                g.distance(arc_center, bulgeStart),
                                arc_startAngle,
                                arc_endAngle));
      }
    }
  }

  // Chain all lines into contours
  std::vector<std::vector<double_point_t>> chains =
    this->Chainify(this->line_stack, this->chain_tolerance);
  std::vector<EasyPrimitive::Part::path_t> paths;
  double_point_t                           bb_min, bb_max;
  this->GetBoundingBox(chains, bb_min, bb_max);

  // Create paths from chains
  for (size_t i = 0; i < chains.size(); i++) {
    EasyPrimitive::Part::path_t path;
    path.is_inside_contour = false;

    if (g.distance(chains[i].front(), chains[i].back()) >
        this->chain_tolerance) {
      path.is_closed = false;
    }
    else {
      path.is_closed = true;
    }

    this->easy_render_instance->SetColorByName(
      path.color, globals->jet_cam_view->outside_contour_color);

    for (size_t j = 0; j < chains[i].size(); j++) {
      const double px = chains[i][j].x - bb_min.x;
      const double py = chains[i][j].y - bb_min.y;
      path.points.push_back({ px, py });
    }

    path.layer = "default";
    path.toolpath_offset = 0.0f;
    path.toolpath_visible = false;
    paths.push_back(path);
  }

  // Determine inside/outside contours
  for (size_t x = 0; x < paths.size(); x++) {
    for (size_t i = 0; i < paths.size(); i++) {
      if (i != x) {
        if (this->CheckIfPathIsInsidePath(paths[x].points, paths[i].points)) {
          paths[x].is_inside_contour = true;
          if (paths[x].is_closed == true) {
            this->easy_render_instance->SetColorByName(
              paths[x].color, globals->jet_cam_view->inside_contour_color);
          }
          else {
            this->easy_render_instance->SetColorByName(
              paths[x].color, globals->jet_cam_view->open_contour_color);
          }
          break;
        }
      }
    }
  }

  // Create the final primitive
  EasyPrimitive::Part* p = this->easy_render_instance->PushPrimitive(
    new EasyPrimitive::Part(this->filename, paths));
  p->control.smoothing = this->smoothing;
  p->control.scale = 1.0;
  p->properties->mouse_callback = this->mouse_callback;
  p->properties->matrix_callback = this->view_callback;
}

void DXFParsePathAdaptor::ExplodeArcToLines(double cx,
                                            double cy,
                                            double r,
                                            double start_angle,
                                            double end_angle,
                                            double num_segments)
{
  Geometry           g;
  std::vector<Point> pointList;
  double_point_t     start;
  double_point_t     sweeper;
  double_point_t     end;

  start.x = cx + (r * cosf((start_angle) *M_PI / 180.0f));
  start.y = cy + (r * sinf((start_angle) *M_PI / 180.0f));
  end.x = cx + (r * cosf((end_angle) *M_PI / 180.0f));
  end.y = cy + (r * sinf((end_angle) *M_PI / 180.0f));

  pointList.push_back(Point(start.x, start.y));

  double diff =
    std::max(start_angle, end_angle) - std::min(start_angle, end_angle);
  if (diff > 180)
    diff = 360 - diff;
  double angle_increment = diff / num_segments;
  double angle_pointer = start_angle + angle_increment;

  for (int i = 0; i < num_segments; i++) {
    sweeper.x = cx + (r * cosf((angle_pointer) *M_PI / 180.0f));
    sweeper.y = cy + (r * sinf((angle_pointer) *M_PI / 180.0f));
    angle_pointer += angle_increment;
    pointList.push_back(Point(sweeper.x, sweeper.y));
  }

  pointList.push_back(Point(end.x, end.y));

  for (size_t i = 1; i < pointList.size(); i++) {
    this->addLine(DL_LineData((double) pointList[i - 1].first,
                              (double) pointList[i - 1].second,
                              0,
                              (double) pointList[i].first,
                              (double) pointList[i].second,
                              0));
  }
}

// ============================================================================
// DXF Entity Handlers
// ============================================================================

void DXFParsePathAdaptor::addLayer(const DL_LayerData& data)
{
  current_layer = data.name;
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
  double_line_t l;
  l.start.x = data.x1;
  l.start.y = data.y1;
  l.end.x = data.x2;
  l.end.y = data.y2;
  this->line_stack.push_back(l);
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
  this->ExplodeArcToLines(
    data.cx, data.cy, data.radius, data.angle1, data.angle2, 100);
}

void DXFParsePathAdaptor::addCircle(const DL_CircleData& data)
{
  this->ExplodeArcToLines(data.cx, data.cy, data.radius, 0, 180, 100);
  this->ExplodeArcToLines(data.cx, data.cy, data.radius, 180, 360, 100);
}

void DXFParsePathAdaptor::addEllipse(const DL_EllipseData& data)
{
  LOG_F(INFO,
        "(DXFParsePathAdaptor::addEllipse) Ellipse at (%.2f, %.2f) - "
        "approximating as circle",
        data.cx,
        data.cy);
  // Approximate ellipse as a circle with radius = major axis length
  double r = sqrt(data.mx * data.mx + data.my * data.my);
  this->addCircle(DL_CircleData(data.cx, data.cy, 0, r));
}

void DXFParsePathAdaptor::addPolyline(const DL_PolylineData& data)
{
  if ((data.flags & (1 << 0))) {
    current_polyline.is_closed = true;
  }
  else {
    current_polyline.is_closed = false;
  }

  if (current_polyline.points.size() > 0) {
    polylines.push_back(current_polyline);
    current_polyline.points.clear();
  }
}

void DXFParsePathAdaptor::addVertex(const DL_VertexData& data)
{
  polyline_vertex_t vertex;
  vertex.point.x = data.x;
  vertex.point.y = data.y;
  vertex.bulge = data.bulge;
  current_polyline.points.push_back(vertex);
}

void DXFParsePathAdaptor::addSpline(const DL_SplineData& data)
{
  // Start a new spline
  if (current_spline.control_points.size() > 0 ||
      current_spline.fit_points.size() > 0) {
    splines.push_back(current_spline);
    current_spline = spline_t();
  }

  // Set spline properties
  current_spline.degree = data.degree;
  current_spline.is_closed = (data.flags & (1 << 0)) != 0;

  // LOG_F(INFO,
  //       "(DXFParsePathAdaptor::addSpline) Spline degree=%d, closed=%d, "
  //       "nKnots=%d, nControl=%d, nFit=%d",
  //       data.degree,
  //       current_spline.is_closed,
  //       data.nKnots,
  //       data.nControl,
  //       data.nFit);
}

void DXFParsePathAdaptor::addControlPoint(const DL_ControlPointData& data)
{
  double_point_t p;
  p.x = data.x;
  p.y = data.y;

  current_spline.control_points.push_back(p);
  current_spline.weights.push_back(data.w);

  // if (data.w != 1.0) {
  //   LOG_F(INFO,
  //         "(DXFParsePathAdaptor::addControlPoint) Control point (%.2f, %.2f)
  //         " "weight=%.2f", data.x, data.y, data.w);
  // }
}

void DXFParsePathAdaptor::addFitPoint(const DL_FitPointData& data)
{
  double_point_t p;
  p.x = data.x;
  p.y = data.y;

  current_spline.fit_points.push_back(p);
  current_spline.has_fit_points = true;

  // LOG_F(INFO,
  //       "(DXFParsePathAdaptor::addFitPoint) Fit point (%.2f, %.2f)",
  //       data.x,
  //       data.y);
}

void DXFParsePathAdaptor::addKnot(const DL_KnotData& data)
{
  current_spline.knots.push_back(data.k);
  current_spline.has_knots = true;

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
        this->units = Units::Inch;
        break;
      case Units::Millimeter:
        this->units = Units::Millimeter;
        break;
      case Units::Centimeter:
        this->units = Units::Centimeter;
        break;
      case Units::Meter:
        this->units = Units::Meter;
        break;
      default:
        LOG_F(INFO,
              "(DXFParsePathAdaptor::setVariableInt) Unknown units. No "
              "conversion will take place");
        this->units = Units::None;
        break;
    }
  }
}
