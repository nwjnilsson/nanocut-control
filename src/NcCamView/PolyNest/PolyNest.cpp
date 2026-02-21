#include "PolyNest.h"
#include "NanoCut.h"
#include "NcCamView/NcCamView.h"

// ---------- PolyPart methods (unchanged) ----------

PolyNest::PolyPoint PolyNest::PolyPart::rotatePoint(PolyPoint p, double a)
{
  PolyPoint return_point;
  double    radians = (M_PI / 180.0) * a;
  double    cos = std::cos(radians);
  double    sin = std::sin(radians);
  return_point.x = (cos * (p.x)) + (sin * (p.y));
  return_point.y = (cos * (p.y)) - (sin * (p.x));
  return return_point;
}

void PolyNest::PolyPart::getBoundingBox(PolyPoint* bbox_min,
                                        PolyPoint* bbox_max)
{
  bbox_max->x = -std::numeric_limits<double>::infinity();
  bbox_max->y = -std::numeric_limits<double>::infinity();
  bbox_min->x = std::numeric_limits<double>::infinity();
  bbox_min->y = std::numeric_limits<double>::infinity();

  for (auto& polygon : m_built_polygons) {
    if (polygon.m_vertices.size() == 0) {
      bbox_max->x = 0;
      bbox_max->y = 0;
      bbox_min->x = 0;
      bbox_min->y = 0;
    }
    for (size_t i = 0; i < polygon.m_vertices.size(); i++) {
      bbox_min->x = std::min(polygon.m_vertices[i].X, bbox_min->x);
      bbox_min->y = std::min(polygon.m_vertices[i].Y, bbox_min->y);
      bbox_max->x = std::max(polygon.m_vertices[i].X, bbox_max->x);
      bbox_max->y = std::max(polygon.m_vertices[i].Y, bbox_max->y);
    }
  }
}

void PolyNest::PolyPart::moveOutsidePolyGonToBack()
{
  PolyGon outside;
  for (size_t x = 0; x < m_polygons.size(); x++) {
    if (m_polygons[x].m_is_inside == false) {
      outside = m_polygons[x];
      m_polygons.erase(m_polygons.begin() + x);
      break;
    }
  }
  m_polygons.push_back(outside);
}

void PolyNest::PolyPart::build()
{
  m_built_polygons.clear();
  for (size_t x = 0; x < m_polygons.size(); x++) {
    PolyGon p;
    p.m_is_closed = m_polygons[x].m_is_closed;
    for (size_t i = 0; i < m_polygons[x].m_vertices.size(); i++) {
      PolyPoint point = rotatePoint(
        PolyPoint(m_polygons[x].m_vertices[i].X, m_polygons[x].m_vertices[i].Y),
        *m_angle);
      point.x += *m_offset_x;
      point.y += *m_offset_y;
      p.m_vertices << ClipperLib::FPoint(point.x, point.y);
    }
    m_built_polygons.push_back(p);
  }
  getBoundingBox(&m_bbox_min, &m_bbox_max);
}

// ---------- PolyNest helper methods ----------

double PolyNest::PolyNest::measureDistanceBetweenPoints(PolyPoint a,
                                                        PolyPoint b)
{
  double x = a.x - b.x;
  double y = a.y - b.y;
  return std::sqrt(x * x + y * y);
}

bool PolyNest::PolyNest::checkIfPointIsInsidePath(std::vector<PolyPoint> path,
                                                  PolyPoint              point)
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

bool PolyNest::PolyNest::checkIfPathIsInsidePath(std::vector<PolyPoint> path1,
                                                 std::vector<PolyPoint> path2)
{
  for (auto& point : path1) {
    if (checkIfPointIsInsidePath(path2, point)) {
      return true;
    }
  }
  return false;
}

PolyNest::PolyPart
PolyNest::PolyNest::buildPart(std::vector<std::vector<PolyPoint>> p,
                              double*                             offset_x,
                              double*                             offset_y,
                              double*                             angle,
                              bool*                               visible)
{
  PolyPart part;
  for (size_t x = 0; x < p.size(); x++) {
    if (p[x].size() > 2) {
      PolyGon polygon;
      polygon.m_is_inside = false;
      for (size_t i = 0; i < p.size(); i++) {
        if (i != x) {
          if (checkIfPathIsInsidePath(p[x], p[i])) {
            polygon.m_is_inside = true;
            break;
          }
        }
      }
      std::vector<PolyPoint> simplified = p[x];
      ClipperLib::Path       subj;
      for (size_t i = 0; i < simplified.size(); i++) {
        subj << ClipperLib::FPoint(simplified[i].x, simplified[i].y);
      }
      ClipperLib::Paths         solution;
      ClipperLib::ClipperOffset co;
      co.AddPath(subj, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
      if (polygon.m_is_inside == true) {
        co.Execute(solution, -SCALE(4.762));
      }
      else {
        co.Execute(solution, SCALE(4.762));
      }

      if (solution.size() > 0) {
        polygon.m_vertices = solution.front();
      }
      else {
        polygon.m_vertices = subj;
        LOG_F(
          ERROR,
          "Zero solutions on offset! Perhaps some pre-processing is needed?");
        continue;
      }
      if (measureDistanceBetweenPoints(PolyPoint(polygon.m_vertices.front().X,
                                                 polygon.m_vertices.front().Y),
                                       PolyPoint(polygon.m_vertices.back().X,
                                                 polygon.m_vertices.back().Y)) <
          m_closed_tolerance) {
        polygon.m_is_closed = true;
      }
      else {
        polygon.m_is_closed = false;
      }
      part.m_polygons.push_back(polygon);
    }
  }
  part.m_offset_x = offset_x;
  part.m_offset_y = offset_y;
  part.m_angle = angle;
  part.m_visible = visible;
  part.moveOutsidePolyGonToBack();
  part.build();
  return part;
}

// ---------- NFP Utilities ----------

ClipperLib::Path
PolyNest::PolyNest::reflectPolygon(const ClipperLib::Path& poly)
{
  ClipperLib::Path reflected;
  reflected.reserve(poly.size());
  for (const auto& pt : poly)
    reflected.push_back(ClipperLib::FPoint(-pt.X, -pt.Y));
  return reflected;
}

ClipperLib::Path PolyNest::PolyNest::rotatePolygon(const ClipperLib::Path& poly,
                                                   double angle)
{
  if (angle == 0.0)
    return poly;
  double           rad = (M_PI / 180.0) * angle;
  double           c = std::cos(rad), s = std::sin(rad);
  ClipperLib::Path result;
  result.reserve(poly.size());
  for (const auto& pt : poly)
    result.push_back(
      ClipperLib::FPoint(c * pt.X + s * pt.Y, c * pt.Y - s * pt.X));
  return result;
}

ClipperLib::Path PolyNest::PolyNest::getBaseContour(const PolyPart& part) const
{
  if (part.m_polygons.empty())
    return {};
  ClipperLib::Path contour = part.m_polygons.back().m_vertices;
  // Simplify the dense jtRound contour for NFP computation
  ClipperLib::CleanPolygon(contour, SCALE(1.0));
  return contour;
}

// Convex hull using Andrew's monotone chain algorithm.
// Returns CCW-wound hull.
static ClipperLib::Path convexHull(ClipperLib::Path points)
{
  size_t n = points.size();
  if (n < 3)
    return points;

  // Sort lexicographically (X first, then Y)
  std::sort(points.begin(),
            points.end(),
            [](const ClipperLib::FPoint& a, const ClipperLib::FPoint& b) {
              return a.X < b.X || (a.X == b.X && a.Y < b.Y);
            });

  ClipperLib::Path hull(2 * n);
  size_t           k = 0;

  // Build lower hull
  for (size_t i = 0; i < n; i++) {
    while (k >= 2) {
      double cross =
        (hull[k - 1].X - hull[k - 2].X) * (points[i].Y - hull[k - 2].Y) -
        (hull[k - 1].Y - hull[k - 2].Y) * (points[i].X - hull[k - 2].X);
      if (cross <= 0)
        k--;
      else
        break;
    }
    hull[k++] = points[i];
  }

  // Build upper hull
  size_t lower_size = k + 1;
  for (size_t i = n - 1; i-- > 0;) {
    while (k >= lower_size) {
      double cross =
        (hull[k - 1].X - hull[k - 2].X) * (points[i].Y - hull[k - 2].Y) -
        (hull[k - 1].Y - hull[k - 2].Y) * (points[i].X - hull[k - 2].X);
      if (cross <= 0)
        k--;
      else
        break;
    }
    hull[k++] = points[i];
  }

  hull.resize(k - 1); // Remove last point (same as first)
  return hull;
}

// Minkowski sum of two convex CCW polygons using the rotating calipers method.
// O(n+m) time, no Clipper boolean ops — numerically robust.
static ClipperLib::Path convexMinkowskiSum(const ClipperLib::Path& P,
                                           const ClipperLib::Path& Q)
{
  if (P.empty() || Q.empty())
    return {};

  size_t pn = P.size(), qn = Q.size();

  // Find bottom-most (then left-most) vertex of each polygon
  size_t pBot = 0, qBot = 0;
  for (size_t i = 1; i < pn; i++)
    if (P[i].Y < P[pBot].Y || (P[i].Y == P[pBot].Y && P[i].X < P[pBot].X))
      pBot = i;
  for (size_t i = 1; i < qn; i++)
    if (Q[i].Y < Q[qBot].Y || (Q[i].Y == Q[qBot].Y && Q[i].X < Q[qBot].X))
      qBot = i;

  ClipperLib::Path result;
  result.reserve(pn + qn);

  size_t pi = 0, qi = 0;
  while (pi < pn || qi < qn) {
    size_t pIdx = (pBot + pi) % pn;
    size_t qIdx = (qBot + qi) % qn;
    result.push_back(
      ClipperLib::FPoint(P[pIdx].X + Q[qIdx].X, P[pIdx].Y + Q[qIdx].Y));

    if (pi >= pn) {
      qi++;
      continue;
    }
    if (qi >= qn) {
      pi++;
      continue;
    }

    // Edge vectors
    size_t pNext = (pBot + pi + 1) % pn;
    size_t qNext = (qBot + qi + 1) % qn;
    double peX = P[pNext].X - P[pIdx].X;
    double peY = P[pNext].Y - P[pIdx].Y;
    double qeX = Q[qNext].X - Q[qIdx].X;
    double qeY = Q[qNext].Y - Q[qIdx].Y;

    // Cross product determines which edge has smaller polar angle
    double cross = peX * qeY - peY * qeX;
    if (cross > 0)
      pi++; // P's edge first
    else if (cross < 0)
      qi++; // Q's edge first
    else {
      pi++;
      qi++;
    } // Parallel, advance both
  }

  return result;
}

ClipperLib::Paths
PolyNest::PolyNest::computeNfp(const ClipperLib::Path& stationary,
                               const ClipperLib::Path& orbiting)
{
  // NFP(A, B) = MinkowskiSum(convexHull(A), convexHull(reflect(B)))
  // Using convex hulls with our own Minkowski sum avoids Clipper 6.1.3's
  // MinkowskiSum crash (null pointer in IntersectEdges due to degenerate
  // quads). Convex hull is conservative for concave parts (slightly larger
  // forbidden region = slightly less tight packing) but guarantees no overlap.
  ClipperLib::Path hullA = convexHull(stationary);
  if (!ClipperLib::Orientation(hullA))
    ClipperLib::ReversePath(hullA); // Ensure CCW

  ClipperLib::Path reflected = reflectPolygon(orbiting);
  ClipperLib::Path hullB = convexHull(reflected);
  if (!ClipperLib::Orientation(hullB))
    ClipperLib::ReversePath(hullB); // Ensure CCW

  if (hullA.size() < 3 || hullB.size() < 3)
    return {};

  ClipperLib::Path nfp = convexMinkowskiSum(hullA, hullB);
  if (nfp.size() < 3)
    return {};

  return { nfp };
}

ClipperLib::Paths
PolyNest::PolyNest::computeIfp(const ClipperLib::Path& part_contour)
{
  // IFP = the set of reference points where the ENTIRE part fits inside
  // the material rectangle (Minkowski erosion of material by part).
  //
  // For rectangular material, this has a simple closed form:
  //   IFP = [mat_min_x - min(part_x), mat_max_x - max(part_x)]
  //       x [mat_min_y - min(part_y), mat_max_y - max(part_y)]
  if (part_contour.empty())
    return {};

  double part_min_x = std::numeric_limits<double>::infinity();
  double part_max_x = -std::numeric_limits<double>::infinity();
  double part_min_y = std::numeric_limits<double>::infinity();
  double part_max_y = -std::numeric_limits<double>::infinity();

  for (const auto& pt : part_contour) {
    part_min_x = std::min(part_min_x, pt.X);
    part_max_x = std::max(part_max_x, pt.X);
    part_min_y = std::min(part_min_y, pt.Y);
    part_max_y = std::max(part_max_y, pt.Y);
  }

  double ifp_min_x = m_min_extents.x - part_min_x;
  double ifp_max_x = m_max_extents.x - part_max_x;
  double ifp_min_y = m_min_extents.y - part_min_y;
  double ifp_max_y = m_max_extents.y - part_max_y;

  if (ifp_min_x > ifp_max_x || ifp_min_y > ifp_max_y)
    return {}; // Part is larger than material

  ClipperLib::Path ifp_rect;
  ifp_rect.push_back(ClipperLib::FPoint(ifp_min_x, ifp_min_y));
  ifp_rect.push_back(ClipperLib::FPoint(ifp_max_x, ifp_min_y));
  ifp_rect.push_back(ClipperLib::FPoint(ifp_max_x, ifp_max_y));
  ifp_rect.push_back(ClipperLib::FPoint(ifp_min_x, ifp_max_y));

  if (!ClipperLib::Orientation(ifp_rect))
    ClipperLib::ReversePath(ifp_rect);

  return { ifp_rect };
}

const ClipperLib::Paths& PolyNest::PolyNest::getNfpCached(size_t stat_idx,
                                                          double stat_angle,
                                                          size_t orb_idx,
                                                          double orb_angle)
{
  NfpKey key{ stat_idx, orb_idx, stat_angle, orb_angle };
  auto   it = m_nfp_cache.find(key);
  if (it != m_nfp_cache.end())
    return it->second;

  // Compute and cache using the unified base contour array
  ClipperLib::Path stat_contour =
    rotatePolygon(m_base_contours[stat_idx], stat_angle);
  ClipperLib::Path orb_contour =
    rotatePolygon(m_base_contours[orb_idx], orb_angle);

  auto& cached = m_nfp_cache[key];
  cached = computeNfp(stat_contour, orb_contour);
  return cached;
}

// ---------- Public interface ----------

void PolyNest::PolyNest::pushUnplacedPolyPart(
  std::vector<std::vector<PolyPoint>> p,
  double*                             offset_x,
  double*                             offset_y,
  double*                             angle,
  bool*                               visible)
{
  PolyPart part = buildPart(p, offset_x, offset_y, angle, visible);
  m_unplaced_parts.push_back(part);
}

void PolyNest::PolyNest::pushPlacedPolyPart(
  std::vector<std::vector<PolyPoint>> p,
  double*                             offset_x,
  double*                             offset_y,
  double*                             angle,
  bool*                               visible)
{
  PolyPart part = buildPart(p, offset_x, offset_y, angle, visible);
  m_placed_parts.push_back(part);
}

void PolyNest::PolyNest::setExtents(PolyPoint min, PolyPoint max)
{
  m_min_extents = min;
  m_max_extents = max;
}

void PolyNest::PolyNest::beginPlaceUnplacedPolyParts()
{
  // Build unified base contour array:
  //   indices [0, F) = fixed (placed) parts
  //   indices [F, F+N) = unplaced parts
  m_base_contours.clear();
  m_fixed_placed.clear();
  m_nfp_cache.clear();

  // Record already-placed parts as fixed obstacles
  for (size_t i = 0; i < m_placed_parts.size(); i++) {
    FixedPart fp;
    fp.part_idx = m_base_contours.size(); // index into m_base_contours
    fp.angle = *m_placed_parts[i].m_angle;
    fp.x = *m_placed_parts[i].m_offset_x;
    fp.y = *m_placed_parts[i].m_offset_y;
    fp.contour = getBaseContour(m_placed_parts[i]);
    m_fixed_placed.push_back(fp);
    m_base_contours.push_back(fp.contour);
  }
  m_num_fixed_contours = m_base_contours.size();

  // Sort unplaced parts by area (largest first) for initial ordering
  std::sort(m_unplaced_parts.begin(),
            m_unplaced_parts.end(),
            [](const PolyPart& lhs, const PolyPart& rhs) {
              return (lhs.m_bbox_max.x - lhs.m_bbox_min.x) *
                       (lhs.m_bbox_max.y - lhs.m_bbox_min.y) >
                     (rhs.m_bbox_max.x - rhs.m_bbox_min.x) *
                       (rhs.m_bbox_max.y - rhs.m_bbox_min.y);
            });

  // Add unplaced parts to the contour array (after sorting)
  for (size_t i = 0; i < m_unplaced_parts.size(); i++) {
    m_base_contours.push_back(getBaseContour(m_unplaced_parts[i]));
  }
}

// ---------- Bottom-Left Fill ----------

ClipperLib::FPoint
PolyNest::PolyNest::findBottomLeftPoint(const ClipperLib::Paths& feasible)
{
  ClipperLib::FPoint best(std::numeric_limits<double>::max(),
                          std::numeric_limits<double>::max());
  bool               found = false;
  for (const auto& path : feasible) {
    for (const auto& pt : path) {
      if (!found || pt.Y < best.Y || (pt.Y == best.Y && pt.X < best.X)) {
        best = pt;
        found = true;
      }
    }
  }
  return best;
}

PolyNest::NestingSolution
PolyNest::PolyNest::runBLF(const std::vector<size_t>& order,
                           const std::vector<double>& angles)
{
  const size_t    n = m_unplaced_parts.size();
  NestingSolution sol;
  sol.part_order = order;
  sol.part_angles = angles;
  sol.placed_x.resize(n, 0);
  sol.placed_y.resize(n, 0);
  sol.placed_ok.resize(n, false);
  sol.parts_placed = 0;
  sol.bounding_width = 0;

  // Track placements: contour index + position
  struct PlacedEntry {
    size_t contour_idx; // index into m_base_contours
    double angle;
    double x, y;
  };
  std::vector<PlacedEntry> placed;

  // Include fixed (pre-placed) parts as obstacles
  for (const auto& fp : m_fixed_placed) {
    placed.push_back({ fp.part_idx, fp.angle, fp.x, fp.y });
  }

  for (size_t idx : order) {
    double           angle = angles[idx];
    size_t           contour_idx = m_num_fixed_contours + idx;
    ClipperLib::Path rotated =
      rotatePolygon(m_base_contours[contour_idx], angle);

    // 1. Compute IFP (where part fits inside material)
    ClipperLib::Paths ifp = computeIfp(rotated);
    if (ifp.empty())
      continue;

    // 2. Compute NFPs against all placed parts and union them
    ClipperLib::Paths nfp_union;
    bool              has_obstacles = false;

    for (const auto& pe : placed) {
      // Look up or compute NFP (cached by contour index + angles)
      const ClipperLib::Paths& nfp_base =
        getNfpCached(pe.contour_idx, pe.angle, contour_idx, angle);

      if (nfp_base.empty())
        continue;

      // Translate NFP to placed part's position
      ClipperLib::Paths nfp_translated;
      nfp_translated.reserve(nfp_base.size());
      for (const auto& nfp_path : nfp_base) {
        ClipperLib::Path translated;
        translated.reserve(nfp_path.size());
        for (const auto& pt : nfp_path) {
          translated.push_back(ClipperLib::FPoint(pt.X + pe.x, pt.Y + pe.y));
        }
        nfp_translated.push_back(std::move(translated));
      }

      has_obstacles = true;
      if (nfp_union.empty()) {
        nfp_union = std::move(nfp_translated);
      }
      else {
        ClipperLib::Clipper c;
        c.AddPaths(nfp_union, ClipperLib::ptSubject, true);
        c.AddPaths(nfp_translated, ClipperLib::ptClip, true);
        ClipperLib::Paths merged;
        c.Execute(ClipperLib::ctUnion,
                  merged,
                  ClipperLib::pftNonZero,
                  ClipperLib::pftNonZero);
        nfp_union = std::move(merged);
      }
    }

    // 3. Feasible region = IFP - NFP_union
    ClipperLib::Paths feasible;
    if (!has_obstacles) {
      feasible = ifp;
    }
    else {
      ClipperLib::Clipper c;
      c.AddPaths(ifp, ClipperLib::ptSubject, true);
      c.AddPaths(nfp_union, ClipperLib::ptClip, true);
      c.Execute(ClipperLib::ctDifference,
                feasible,
                ClipperLib::pftNonZero,
                ClipperLib::pftNonZero);
    }

    if (feasible.empty())
      continue;

    // 4. Find bottom-left point in feasible region
    ClipperLib::FPoint bl = findBottomLeftPoint(feasible);

    sol.placed_x[idx] = bl.X;
    sol.placed_y[idx] = bl.Y;
    sol.placed_ok[idx] = true;
    sol.parts_placed++;

    // Track bounding width (rightmost extent of placed parts)
    for (const auto& pt : rotated) {
      double world_x = pt.X + bl.X;
      sol.bounding_width =
        std::max(sol.bounding_width, world_x - m_min_extents.x);
    }

    // Add to placed list for subsequent parts
    placed.push_back({ contour_idx, angle, bl.X, bl.Y });
  }

  return sol;
}

// ---------- Simulated Annealing ----------

double PolyNest::PolyNest::evaluateFitness(const NestingSolution& sol)
{
  double material_width = m_max_extents.x - m_min_extents.x;
  double utilization =
    (material_width > 0) ? (1.0 - sol.bounding_width / material_width) : 0.0;
  return sol.parts_placed * 1000.0 + utilization * 100.0;
}

PolyNest::NestingSolution
PolyNest::PolyNest::runSimulatedAnnealing(std::atomic<float>* progress)
{
  const size_t n = m_unplaced_parts.size();
  std::mt19937 rng(42);

  // Initial solution: sorted by area descending, all at 0 degrees
  std::vector<size_t> initial_order(n);
  std::iota(initial_order.begin(), initial_order.end(), 0);
  std::vector<double> initial_angles(n, 0.0);

  NestingSolution current = runBLF(initial_order, initial_angles);
  NestingSolution best = current;
  double          current_fitness = evaluateFitness(current);
  double          best_fitness = current_fitness;

  double    temp = m_sa_initial_temp;
  int       no_improve_count = 0;
  const int early_stop_threshold = 2000;

  std::uniform_real_distribution<double> accept_dist(0.0, 1.0);
  std::uniform_int_distribution<size_t>  part_dist(0, n - 1);
  std::uniform_int_distribution<int>     move_dist(0, 2);
  std::uniform_int_distribution<size_t>  rot_dist(
    0, m_allowed_rotations.size() - 1);

  for (int iter = 0; iter < m_sa_max_iterations; iter++) {
    // Generate neighbor
    std::vector<size_t> new_order = current.part_order;
    std::vector<double> new_angles = current.part_angles;

    int move_type = move_dist(rng);

    if (move_type == 0 && n >= 2) {
      // Swap two parts in order
      size_t i = part_dist(rng);
      size_t j = part_dist(rng);
      while (j == i)
        j = part_dist(rng);
      std::swap(new_order[i], new_order[j]);
    }
    else if (move_type == 1 && n >= 2) {
      // Move one part to a different position in the order
      size_t from = part_dist(rng);
      size_t to = part_dist(rng);
      while (to == from)
        to = part_dist(rng);
      size_t val = new_order[from];
      new_order.erase(new_order.begin() + from);
      new_order.insert(new_order.begin() + to, val);
    }
    else {
      // Change one part's rotation
      size_t i = part_dist(rng);
      new_angles[new_order[i]] = m_allowed_rotations[rot_dist(rng)];
    }

    NestingSolution neighbor = runBLF(new_order, new_angles);
    double          neighbor_fitness = evaluateFitness(neighbor);

    // Accept or reject
    double delta = neighbor_fitness - current_fitness;
    if (delta > 0 || accept_dist(rng) < std::exp(delta / temp)) {
      current = std::move(neighbor);
      current_fitness = neighbor_fitness;

      if (current_fitness > best_fitness) {
        best = current;
        best_fitness = current_fitness;
        no_improve_count = 0;
      }
      else {
        no_improve_count++;
      }
    }
    else {
      no_improve_count++;
    }

    temp *= m_sa_cooling_rate;

    bool all_parts_placed = best.parts_placed == static_cast<int>(n);
    if (progress) {
      // Contribution from iterations
      float p1 = 0.1f + 0.9f * static_cast<float>(iter) /
                          static_cast<float>(m_sa_max_iterations);
      // Push convergence to 100% when we're exiting early
      float p2 = (1.f - p1) * static_cast<float>(no_improve_count) /
                 static_cast<float>(early_stop_threshold);
      progress->store(all_parts_placed ? p1 + p2 : p1);
    }

    // Early termination if all parts placed and no improvement
    if (all_parts_placed && no_improve_count >= early_stop_threshold) {
      LOG_F(
        INFO, "SA early termination at iteration %d (no improvement)", iter);
      break;
    }
  }

  LOG_F(INFO,
        "SA complete: placed %d/%zu parts, bounding width: %.1f",
        best.parts_placed,
        n,
        best.bounding_width);
  return best;
}

// ---------- Main entry point ----------

int PolyNest::PolyNest::placeAllUnplacedParts(std::atomic<float>* progress)
{
  if (m_unplaced_parts.empty()) {
    if (progress)
      progress->store(1.0f);
    return 0;
  }

  const size_t    n = m_unplaced_parts.size();
  NestingSolution sol;

  if (n == 1) {
    // Single part: try each rotation with BLF, pick best
    if (progress)
      progress->store(0.1f);

    NestingSolution best_single;
    double          best_fitness = -1;

    for (double angle : m_allowed_rotations) {
      std::vector<size_t> order = { 0 };
      std::vector<double> angles = { angle };
      NestingSolution     candidate = runBLF(order, angles);
      double              fitness = evaluateFitness(candidate);
      if (fitness > best_fitness) {
        best_fitness = fitness;
        best_single = candidate;
      }
    }
    sol = best_single;
  }
  else {
    // Multiple parts: run simulated annealing
    sol = runSimulatedAnnealing(progress);
  }

  // Apply results back to parts through their pointers
  int failed_count = 0;
  for (size_t i = 0; i < n; i++) {
    auto& part = m_unplaced_parts[i];
    if (sol.placed_ok[i]) {
      *part.m_offset_x = sol.placed_x[i];
      *part.m_offset_y = sol.placed_y[i];
      *part.m_angle = sol.part_angles[i];
      *part.m_visible = true;
      part.build();
    }
    else {
      *part.m_visible = true;
      failed_count++;
      LOG_F(WARNING,
            "Could not place part %zu ('%s') — not enough material",
            i,
            part.m_part_name.c_str());
    }
  }

  if (progress)
    progress->store(1.0f);
  return failed_count;
}
