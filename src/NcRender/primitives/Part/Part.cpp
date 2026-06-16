#include "Part.h"
#include "../../geometry/clipper.h"
#include "../../geometry/geometry.h"

#include <NcRender/NcRender.h>

#include <loguru.hpp>

#include <NcRender/gl.h>

#include <algorithm>
#include <limits>
#include <optional>

std::string Part::getTypeName() { return "part"; }
void        Part::processMouse(float mpos_x, float mpos_y)
{
  if (!visible)
    return;

  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  double pad = mouse_over_padding / scale;

  // Part-level bounding box early-out
  if (mpos_x < m_bb_min.x - pad || mpos_x > m_bb_max.x + pad ||
      mpos_y < m_bb_min.y - pad || mpos_y > m_bb_max.y + pad) {
    if (mouse_over) {
      m_mouse_event =
        MouseHoverEvent(NcRender::EventType::MouseOut, mpos_x, mpos_y);
      mouse_over = false;
    }
    return;
  }

  size_t path_index = -1;
  if (m_control.mouse_mode == 0) {
    bool   mouse_is_over_path = false;
    size_t global_index = 0;
    for (auto& [layer_name, layer] : m_layers) {
      if (!layer.visible) {
        global_index += layer.paths.size();
        continue;
      }
      for (auto& path : layer.paths) {
        if (path.built_points.size() < 2) {
          global_index++;
          continue;
        }
        // Per-path bounding box early-out
        if (mpos_x < path.bbox.min.x - pad ||
            mpos_x > path.bbox.max.x + pad ||
            mpos_y < path.bbox.min.y - pad ||
            mpos_y > path.bbox.max.y + pad) {
          global_index++;
          continue;
        }
        for (size_t i = 1; i < path.built_points.size(); i++) {
          if (geo::lineIntersectsWithCircle(
                { { path.built_points[i - 1].x,
                    path.built_points[i - 1].y },
                  { path.built_points[i].x, path.built_points[i].y } },
                { mpos_x, mpos_y },
                pad)) {
            path_index = global_index;
            mouse_is_over_path = true;
            break;
          }
        }
        if (!mouse_is_over_path && path.is_closed) {
          if (geo::lineIntersectsWithCircle(
                { { path.built_points.front().x,
                    path.built_points.front().y },
                  { path.built_points.back().x,
                    path.built_points.back().y } },
                { mpos_x, mpos_y },
                pad)) {
            path_index = global_index;
            mouse_is_over_path = true;
          }
        }
        if (mouse_is_over_path)
          break;
        global_index++;
      }
      if (mouse_is_over_path)
        break;
    }
    if (mouse_is_over_path) {
      if (!mouse_over) {
        m_mouse_event = MouseHoverEvent(
          NcRender::EventType::MouseIn, mpos_x, mpos_y, path_index);
        mouse_over = true;
      }
    }
    else if (mouse_over) {
      m_mouse_event = MouseHoverEvent(
        NcRender::EventType::MouseOut, mpos_x, mpos_y, path_index);
      mouse_over = false;
    }
  }
  else if (m_control.mouse_mode == 1) { // nesting
    bool   mouse_is_inside_perimeter = false;
    size_t global_index = 0;
    for (auto& [layer_name, layer] : m_layers) {
      if (!layer.visible) {
        global_index += layer.paths.size();
        continue;
      }
      for (auto& path : layer.paths) {
        if (path.is_inside_contour == false) {
          // Per-path bounding box early-out
          if (mpos_x < path.bbox.min.x || mpos_x > path.bbox.max.x ||
              mpos_y < path.bbox.min.y || mpos_y > path.bbox.max.y) {
            global_index++;
            continue;
          }
          if (checkIfPointIsInsidePath(path.built_points,
                                       { mpos_x, mpos_y })) {
            path_index = global_index;
            mouse_is_inside_perimeter = true;
            break;
          }
        }
        global_index++;
      }
      if (mouse_is_inside_perimeter)
        break;
    }
    if (mouse_is_inside_perimeter) {
      if (!mouse_over) {
        m_mouse_event = MouseHoverEvent(
          NcRender::EventType::MouseIn, mpos_x, mpos_y, path_index);
        mouse_over = true;
      }
    }
    else if (mouse_over) {
      m_mouse_event = MouseHoverEvent(
        NcRender::EventType::MouseOut, mpos_x, mpos_y, path_index);
      mouse_over = false;
    }
  }
}
std::vector<std::vector<Point2d>> Part::offsetPath(std::vector<Point2d> path,
                                                   double               offset)
{
  double                            scale = 100.0f;
  std::vector<std::vector<Point2d>> ret;
  ClipperLib::Path                  subj;
  ClipperLib::Paths                 solution;
  // Drop a near-duplicate closing vertex. Clipper's own dedup uses exact ==,
  // which misses sub-ULP seam vertices from chainify and produces spurious
  // jtRound spikes at the seam.
  auto end = path.end();
  if (path.size() > 1) {
    const double dx = path.front().x - path.back().x;
    const double dy = path.front().y - path.back().y;
    // 1e-6 unit (~1 nm) squared — well below any intentional geometry,
    // well above fp drift from cos/sin and DXF precision noise.
    if (dx * dx + dy * dy < 1e-12)
      --end;
  }
  for (std::vector<Point2d>::iterator it = path.begin(); it != end; ++it) {
    subj << ClipperLib::FPoint((it->x * scale), (it->y * scale));
  }
  ClipperLib::ClipperOffset co;
  co.AddPath(subj, ClipperLib::jtRound, ClipperLib::etClosedPolygon);
  co.Execute(solution, offset * scale);
  ClipperLib::CleanPolygons(solution, 0.001 * scale);
  for (int x = 0; x < solution.size(); x++) {
    std::vector<Point2d> new_path;
    Point2d              first_point;
    for (int y = 0; y < solution[x].size(); y++) {
      if (y == 0) {
        first_point.x = double(solution[x][y].X) / scale;
        first_point.y = double(solution[x][y].Y) / scale;
      }
      Point2d point;
      point.x = double(solution[x][y].X) / scale;
      point.y = double(solution[x][y].Y) / scale;
      new_path.push_back(point);
    }
    Point2d point;
    point.x = first_point.x;
    point.y = first_point.y;
    new_path.push_back(point);
    ret.push_back(new_path);
  }
  return ret;
}
double Part::perpendicularDistance(const Point2d& pt,
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
void Part::simplify(const std::vector<Point2d>& pointList,
                    std::vector<Point2d>&       out,
                    double                      epsilon)
{
  if (pointList.size() < 2) {
    throw "Not enough points to simplify";
  }
  double dmax = 0.0;
  size_t index = 0;
  size_t end = pointList.size() - 1;
  for (size_t i = 1; i < end; i++) {
    double d =
      perpendicularDistance(pointList[i], pointList[0], pointList[end]);
    if (d > dmax) {
      index = i;
      dmax = d;
    }
  }
  if (dmax > epsilon) {
    // Recursive call
    std::vector<Point2d> recResults1;
    std::vector<Point2d> recResults2;
    std::vector<Point2d> firstLine(pointList.begin(),
                                   pointList.begin() + index + 1);
    std::vector<Point2d> lastLine(pointList.begin() + index, pointList.end());
    simplify(firstLine, recResults1, epsilon);
    simplify(lastLine, recResults2, epsilon);
    // Build the result list
    out.assign(recResults1.begin(), recResults1.end() - 1);
    out.insert(out.end(), recResults2.begin(), recResults2.end());
    if (out.size() < 2) {
      throw "Problem assembling output";
    }
  }
  else {
    // Just return start and end points
    out.clear();
    out.push_back(pointList[0]);
    out.push_back(pointList[end]);
  }
}
void Part::render()
{
  if (!(m_last_control == m_control)) {
    bool smoothing_changed = m_last_control.smoothing != m_control.smoothing;

    m_number_of_verticies = 0;
    m_tool_paths.clear();
    for (auto& [layer_name, layer] : m_layers) {
      // Skip invisible layers
      if (!layer.visible)
        continue;

      for (auto& path : layer.paths) {
        path.built_points.clear();
        try {
          // Only re-simplify when smoothing changed or first build
          if (smoothing_changed || path.simplified_points.empty()) {
            path.simplified_points.clear();
            // For closed paths with very few points, skip simplification to
            // preserve geometry
            if (path.is_closed && path.points.size() <= 6) {
              path.simplified_points = path.points;
            }
            else {
              simplify(
                path.points, path.simplified_points, m_control.smoothing);
            }
          }

          // Transform cached simplified points
          for (size_t i = 0; i < path.simplified_points.size(); i++) {
            Point2d rotated = geo::rotatePoint(
              { 0, 0 }, path.simplified_points[i], m_control.angle);
            path.built_points.push_back(
              { (rotated.x + m_control.offset.x) * m_control.scale,
                (rotated.y + m_control.offset.y) * m_control.scale });
            m_number_of_verticies++;
          }
          path.bbox = geo::calculateBoundingBox(path.built_points);
          if (layer.toolpath_visible == true) {
            if (path.is_closed == true) {
              // Need at least 3 points to form a valid closed polygon
              if (path.built_points.size() < 3) {
                continue;
              }
              const int    direction = path.is_inside_contour ? -1 : +1;
              const double radius = path.is_inside_contour
                                      ? m_control.lead_in_length
                                      : m_control.lead_out_length;
              std::vector<std::vector<Point2d>> tpaths = offsetPath(
                path.built_points,
                direction * (double) fabs(layer.toolpath_offset));
              for (size_t x = 0; x < tpaths.size(); x++) {
                Toolpath tp;
                if (createToolpathLeads(&tp, tpaths[x], radius, direction)) {
                  tp.is_closed_contour = true;
                  tp.is_inside_contour = path.is_inside_contour;
                  m_tool_paths.push_back(std::move(tp));
                }
              }
            }
            else {
              Toolpath tp;
              tp.points = path.built_points;
              tp.is_closed_contour = false;
              tp.is_inside_contour = false;
              m_tool_paths.push_back(std::move(tp));
            }
          }
        }
        catch (std::exception& e) {
          LOG_F(ERROR,
                "(Part::render) Exception: %s, setting visability "
                "to false to avoid further exceptions!",
                e.what());
          visible = false;
        }
      }
    }
    getBoundingBox(&m_bb_min, &m_bb_max);
  }
  m_last_control = m_control;
  glPushMatrix();
  glTranslatef(offset[0], offset[1], offset[2]);
  glScalef(scale, scale, scale);
  glLineWidth(m_width);
  if (m_style == "dashed") {
    glPushAttrib(GL_ENABLE_BIT);
    glLineStipple(10, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
  }
  glEnableClientState(GL_VERTEX_ARRAY);
  for (auto& [layer_name, layer] : m_layers) {
    // Skip invisible layers
    if (!layer.visible)
      continue;

    for (auto& path : layer.paths) {
      if (path.built_points.empty())
        continue;
      glColor4f(path.color->r, path.color->g, path.color->b, path.color->a);
      glVertexPointer(
        2, GL_DOUBLE, sizeof(Point2d), path.built_points.data());
      glDrawArrays(path.is_closed ? GL_LINE_LOOP : GL_LINE_STRIP,
                   0,
                   path.built_points.size());
    }
  }
  glColor4f(0, 1, 0, 0.5);
  for (auto& tp : m_tool_paths) {
    if (tp.points.empty())
      continue;
    glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), tp.points.data());
    glDrawArrays(GL_LINE_STRIP, 0, tp.points.size());
  }
  glDisableClientState(GL_VERTEX_ARRAY);
  glLineWidth(1);
  glDisable(GL_LINE_STIPPLE);
  glPopMatrix();
}
nlohmann::json Part::serialize()
{
  nlohmann::json layers_json;
  for (auto& [layer_name, layer] : m_layers) {
    nlohmann::json layer_json;
    layer_json["toolpath_offset"] = layer.toolpath_offset;
    layer_json["toolpath_visible"] = layer.toolpath_visible;
    nlohmann::json paths_json;
    for (auto& path : layer.paths) {
      nlohmann::json path_json;
      path_json["is_closed"] = path.is_closed;
      for (int i = 0; i < path.points.size(); i++) {
        path_json["points"].push_back(
          { { "x", path.points[i].x }, { "y", path.points[i].y }, { 0.f } });
      }
      paths_json.push_back(path_json);
    }
    layer_json["paths"] = paths_json;
    layers_json[layer_name] = layer_json;
  }
  nlohmann::json j;
  j["layers"] = layers_json;
  j["part_name"] = m_part_name;
  j["width"] = m_width;
  j["style"] = m_style;
  return j;
}
void Part::getBoundingBox(Point2d* bbox_min, Point2d* bbox_max)
{
  bbox_max->x = -inf<double>();
  bbox_max->y = -inf<double>();
  bbox_min->x = inf<double>();
  bbox_min->y = inf<double>();
  for (auto& [layer_name, layer] : m_layers) {
    for (auto& path : layer.paths) {
      for (std::vector<Point2d>::iterator p = path.built_points.begin();
           p != path.built_points.end();
           ++p) {
        if ((double) (*p).x < bbox_min->x)
          bbox_min->x = (double) (*p).x;
        if ((double) (*p).x > bbox_max->x)
          bbox_max->x = (double) (*p).x;
        if ((double) (*p).y < bbox_min->y)
          bbox_min->y = (double) (*p).y;
        if ((double) (*p).y > bbox_max->y)
          bbox_max->y = (double) (*p).y;
      }
    }
  }
}
bool Part::checkIfPointIsInsidePath(std::vector<Point2d> path, Point2d point)
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
bool Part::checkIfPathIsInsidePath(std::vector<Point2d> path1,
                                   std::vector<Point2d> path2)
{
  for (std::vector<Point2d>::iterator it = path1.begin(); it != path1.end();
       ++it) {
    if (checkIfPointIsInsidePath(path2, (*it))) {
      return true;
    }
  }
  return false;
}

std::vector<Part::Toolpath> Part::getOrderedToolpaths()
{
  std::vector<Toolpath> toolpaths = m_tool_paths;
  std::vector<Toolpath> ret;
  if (toolpaths.size() > 0) {
    ret.push_back(std::move(toolpaths.back())); // Prime the sort
    toolpaths.pop_back();
    float  smallest_distance = 100000000000.0f;
    size_t winner_index = 0;
    while (toolpaths.size() > 0) {
      for (size_t x = 0; x < toolpaths.size(); x++) {
        if (toolpaths[x].points.size() > 0) {
          if (geo::distance(toolpaths[x].points[0], ret.back().points[0]) <
              smallest_distance) {
            smallest_distance =
              geo::distance(toolpaths[x].points[0], ret.back().points[0]);
            winner_index = x;
          }
        }
        else {
          toolpaths.erase(toolpaths.begin() + x);
          winner_index = -1;
          break;
        }
      }
      if (winner_index == -1) {
        LOG_F(WARNING,
              "(Part::getOrderedToolpaths) Discarded empty "
              "toolpath!");
      }
      else {
        ret.push_back(std::move(toolpaths[winner_index]));
        toolpaths.erase(toolpaths.begin() + winner_index);
      }
      smallest_distance = 100000000000.0f;
      winner_index = 0;
    }
    // Find the toolpaths that have other paths inside them and push them to the
    // back of stack because they are the outside cuts and need to be cut last

    // Cache bounding boxes for all toolpaths (over the points field).
    std::vector<geo::Extents> bboxes;
    bboxes.reserve(ret.size());
    for (const auto& tp : ret) {
      bboxes.push_back(geo::calculateBoundingBox(tp.points));
    }

    std::vector<Toolpath> outside_paths;
    for (size_t x = 0; x < ret.size(); x++) {
      bool has_paths_inside = false;
      for (size_t i = 0; i < ret.size(); i++) {
        if (i != x) {
          if (!geo::extentsContain(bboxes[x], bboxes[i])) {
            continue;
          }

          if (checkIfPathIsInsidePath(ret[i].points, ret[x].points) == true) {
            has_paths_inside = true;
          }
        }
      }
      if (has_paths_inside == true) {
        outside_paths.push_back(std::move(ret[x]));
        ret.erase(ret.begin() + x);
      }
    }
    for (size_t x = 0; x < outside_paths.size(); x++) {
      ret.push_back(std::move(outside_paths[x]));
    }
  }
  return ret;
}
Point2d* Part::getClosestPoint(size_t*               index,
                               Point2d               point,
                               std::vector<Point2d>* points)
{
  double smallest_distance = 1000000;
  int    smallest_index = 0;
  for (size_t x = 0; x < points->size(); x++) {
    double dist = geo::distance(point, (*points)[x]);
    if (dist < smallest_distance) {
      smallest_distance = dist;
      smallest_index = x;
    }
  }
  if (smallest_index < points->size()) {
    if (index != NULL)
      *index = smallest_index;
    return &(*points)[smallest_index];
  }
  else {
    return NULL;
  }
}
namespace {

// Straight-lead pierce: walk every contour vertex and pick the attach
// point whose nearest offset-polygon vertex sits closest to the
// requested lead length. Only when no vertex on the contour yields a
// feasible attach at the full radius do we shrink the offset distance
// -- reducing lead length is a last resort for contours too small to
// accommodate the user's setting at any vertex. Returns the chosen
// pierce point and the contour index it attaches to (or std::nullopt
// if no attach is feasible at any tried radius).
struct StraightLead {
  Point2d pierce;
  size_t  attach_index;
};
std::optional<StraightLead> findStraightPierce(Part&                       part,
                                               const std::vector<Point2d>& contour,
                                               double                      radius,
                                               int                         direction)
{
  const double min_feasible = SCALE(0.05);
  double       feasible = std::fabs(radius);
  while (feasible >= min_feasible) {
    auto                        lead_offset =
      part.offsetPath(contour, feasible * (double) direction);
    std::optional<StraightLead> best;
    double                      best_score = std::numeric_limits<double>::max();
    for (size_t i = 0; i < contour.size(); ++i) {
      for (auto& poly : lead_offset) {
        Point2d* point = part.getClosestPoint(nullptr, contour[i], &poly);
        if (!point)
          continue;
        const double d = geo::distance(contour[i], *point);
        if (d > feasible * 1.1)
          continue;
        // Prefer attach points where the lead length matches the
        // requested feasible distance most closely (longest physically
        // realisable lead without overshooting).
        const double score = std::fabs(d - feasible);
        if (score < best_score) {
          best_score = score;
          best = StraightLead{ *point, i };
        }
      }
    }
    if (best)
      return best;
    feasible *= 0.5;
  }
  return std::nullopt;
}

// Rotate `contour` so that index `new_first` becomes index 0. If
// `new_first` is 0 the contour is returned unchanged.
std::vector<Point2d> rotateContour(const std::vector<Point2d>& contour,
                                   size_t                      new_first)
{
  if (new_first == 0 || new_first >= contour.size())
    return contour;
  std::vector<Point2d> out;
  out.reserve(contour.size());
  out.insert(out.end(), contour.begin() + new_first, contour.end());
  out.insert(out.end(), contour.begin(), contour.begin() + new_first);
  return out;
}

} // namespace

bool Part::createToolpathLeads(Toolpath*                   out,
                               const std::vector<Point2d>& contour_in,
                               double                      radius,
                               int                         direction)
{
  out->points.clear();
  out->lead_in_count = 0;
  out->lead_out_count = 0;
  out->is_closed_contour = true;
  out->is_inside_contour = (direction < 0);
  out->lead_in_is_arc = false;

  // Strip the closing-duplicate vertex that Part::offsetPath appends
  // (last == first). Working with a polygon of distinct vertices makes
  // cyclic indexing in the run search and the rotation step unambiguous.
  std::vector<Point2d> contour = contour_in;
  if (contour.size() > 1) {
    const double dx = contour.front().x - contour.back().x;
    const double dy = contour.front().y - contour.back().y;
    if (dx * dx + dy * dy < 1e-12)
      contour.pop_back();
  }

  if (contour.size() < 3)
    return false;

  const bool is_inside = direction < 0;

  if (radius == 0.0) {
    // Zero-length lead: no pierce vertex, just the contour itself.
    out->points = contour;
    if (!is_inside) {
      out->points.push_back(contour.front());
      out->lead_out_count = 1;
    }
    return true;
  }

  // ---- Attempt arc lead-in ------------------------------------------------
  const double radius_abs = std::fabs(radius);
  // Look for a "straight run": a span of vertices [start, end] where
  // the chord approximates the contour within dev_tolerance. This
  // bridges the natural curvature of dense polygons (e.g. circles
  // discretized to many short segments by jtRound + Clipper) where no
  // single segment would be long enough for an arc lead.
  // The arc curves AWAY from the contour (inward for inside contours,
  // outward for outside), so the straight-run length needn't be 2*radius
  // -- it only needs to give a locally smooth attach vertex. The real
  // feasibility test is "does the discretized arc lie on the correct
  // side of the contour", performed below.
  const double min_segment_len =
    std::max(static_cast<double>(SCALE(0.5)), radius_abs * 0.5);
  const double dev_tolerance =
    std::max(static_cast<double>(SCALE(0.25)), radius_abs * 0.1);
  geo::StraightRun run;
  const bool       run_found =
    geo::longestStraightRun(contour, min_segment_len, dev_tolerance, &run);
  if (!run_found) {
    LOG_F(INFO,
          "[createToolpathLeads] No straight run >= %.3f mm (tol=%.3f) "
          "in %zu-vertex contour (direction=%d); falling back to straight "
          "lead.",
          min_segment_len,
          dev_tolerance,
          contour.size(),
          direction);
  }

  bool arc_built = false;
  if (run_found) {
    // Place attach AT an actual contour vertex (mid-run by vertex index)
    // and use the LOCAL edge direction at that vertex as the tangent.
    // Putting attach on the chord (instead of on the contour) created a
    // perpendicular jump between arc-end and the first contour vertex
    // equal to the local sagitta — that's the "spike" we want to avoid.
    // With attach on the contour, the cut continues from arc-end into
    // contour[mid+1] along an actual contour edge, with no jump.
    const size_t N = contour.size();
    const size_t run_len_verts =
      (run.end + N - run.start) % N; // 0 disallowed (chord_len>0)
    const size_t mid_offset = run_len_verts / 2;
    const size_t mid = (run.start + mid_offset) % N;
    const size_t mid_next = (mid + 1) % N;

    const Point2d attach = contour[mid];
    const double  edx = contour[mid_next].x - attach.x;
    const double  edy = contour[mid_next].y - attach.y;
    const double  edge_len = std::sqrt(edx * edx + edy * edy);
    if (edge_len > 0.0) {
      const Point2d tangent_dir = { edx / edge_len, edy / edge_len };

      // Polygon orientation: positive shoelace = CCW. For a CCW polygon
      // the interior is on the LEFT of each edge direction; for CW the
      // interior is on the RIGHT. buildArcLead's sweep_sign chooses the
      // side the arc curves toward: +1 -> LEFT of tangent, -1 -> RIGHT.
      // We want the arc on the INTERIOR side for inside contours (so
      // the pierce ends up in the slug) and on the EXTERIOR side for
      // outside contours (pierce in the scrap). For CCW this is
      // sweep_sign = +1 (inside) or -1 (outside); for CW it's the
      // opposite. Combined: sweep_sign = -direction * orientation.
      const double orientation =
        geo::signedPolygonArea(contour) >= 0 ? 1.0 : -1.0;
      const int sweep_sign = static_cast<int>(-direction * orientation);

      // 90 deg sweep, sized at the user's lead_in_length.
      auto arc_pts =
        geo::buildArcLead(attach, tangent_dir, radius_abs, 90.0, sweep_sign, 16);

      if (arc_pts.size() >= 2) {
        // Two checks combined.
        //
        // (1) Whole-arc side check: every arc vertex (except `attach`,
        // a boundary point pointIsInsidePolygon may classify
        // ambiguously) must sit on the correct side of the contour,
        // with slop to absorb the deviation between the simplified +
        // Clipper-offset contour and the true geometry. Catches arcs
        // that exit via tangent-direction errors.
        //
        // (2) Pierce headroom check: the pierce -- the arc's deepest
        // vertex into the slug -- must have room to breathe so the
        // lead-in doesn't end up grazing the opposite wall of a
        // narrow pocket (the divot risk in banana-shaped pockets).
        // We measure the pierce's distance to the nearest contour
        // edge directly. For a clean parallel-wall pocket the nearest
        // edge is the attach edge at distance ~`radius_abs`; if any
        // other wall comes closer than `radius_abs * 0.5`, that's the
        // banana case and we reject. Using a direct distance check
        // (rather than offsetting the contour by 0.9*r and asking
        // Clipper if the pierce lies inside) means small holes whose
        // inradius is less than 0.9*r still pass when the arc fits
        // geometrically -- the side check has already proven that.
        //
        // Slop tolerance: RDP simplification with `m_control.smoothing`
        // permits the polygon to deviate from the true contour by up
        // to that amount perpendicular, and Clipper's offset rounding
        // / CleanPolygons add a similar amount of vertex jitter on
        // top. Allow at least SCALE(0.5) mm of slop or 2x smoothing,
        // whichever is larger.
        const double side_slop =
          std::max(static_cast<double>(SCALE(0.5)),
                   m_control.smoothing * 2.0);
        bool arc_ok = true;
        for (size_t i = 0; i + 1 < arc_pts.size(); ++i) {
          const bool inside = geo::pointIsInsidePolygon(contour, arc_pts[i]);
          const bool wrong_side = is_inside ? !inside : inside;
          if (wrong_side) {
            const double edge_dist =
              geo::pointToPolygonDistance(contour, arc_pts[i]);
            if (edge_dist > side_slop) {
              arc_ok = false;
              break;
            }
          }
        }
        if (arc_ok) {
          const double pierce_clearance =
            geo::pointToPolygonDistance(contour, arc_pts.front());
          if (pierce_clearance < radius_abs * 0.5) {
            arc_ok = false;
          }
        }
        if (!arc_ok) {
          LOG_F(INFO,
                "[createToolpathLeads] Arc lead rejected -- crosses "
                "contour or insufficient pierce headroom "
                "(is_inside=%d, edge_len=%.3f, radius=%.3f); falling "
                "back to straight lead.",
                static_cast<int>(is_inside),
                edge_len,
                radius_abs);
        }
        if (arc_ok) {
          // Rotate the contour so it starts at `mid` (the contour vertex
          // where the arc lands). Since attach == contour[mid], the arc's
          // last vertex (snapped to attach) equals rotated[0], so we drop
          // the arc's final vertex when assembling. No extra `attach`
          // insertion needed — rotated[0] already serves as the contour's
          // first vertex after the arc.
          std::vector<Point2d> rotated = rotateContour(contour, mid);

          // Assemble: arc (pierce -> ... -> attach=rotated[0]), then
          // contour from rotated[0] all the way around back to attach
          // (closing duplicate at the end).
          out->points.reserve(arc_pts.size() - 1 + rotated.size() + 1);
          out->points.insert(out->points.end(), arc_pts.begin(),
                             arc_pts.end() - 1);
          // lead_in_count counts the arc-only vertices BEFORE attach.
          out->lead_in_count = arc_pts.size() - 1;
          out->points.insert(out->points.end(), rotated.begin(), rotated.end());
          // Trailing closing vertex marked as lead-out (lead_out_count=1)
          // for both inside and outside contours. For inside contours the
          // emitter replaces the closing G1 with overburn (torch off);
          // for outside contours the emitter walks the closing G1 with
          // torch on. The duplicate keeps the [contour_first, contour_last]
          // range describing the cyclic contour vertices.
          out->points.push_back(attach);
          out->lead_out_count = 1;
          out->lead_in_is_arc = true;
          arc_built = true;
        }
      }
    }
  }

  if (arc_built)
    return true;

  // ---- Fallback: straight lead --------------------------------------------
  auto straight = findStraightPierce(*this, contour, radius_abs, direction);
  if (!straight)
    return false;

  // Rotate the contour so the chosen attach vertex sits at index 0 --
  // findStraightPierce can land anywhere along the contour, not just at
  // contour.front(). The assembly layout below assumes rotated[0] is the
  // attach point that the pierce -> attach segment lands on.
  std::vector<Point2d> rotated =
    rotateContour(contour, straight->attach_index);

  // Layout: [pierce, c0, c1, ..., c_{N-1}, c0, (pierce if outside)].
  // The explicit closing-c0 vertex matters: contour was stripped of its
  // closing duplicate early in this function, so without re-adding it the
  // rendered line strip (and the outside-contour emitter that walks every
  // vertex) would draw c_{N-1} -> pierce directly across the slug. That's
  // the "spike" visible in the preview for straight-lead fallbacks.
  //
  // For inside contours `lead_out_count = 1` makes contour_last point at
  // c_{N-1}; torch_off_async fires there, then the overburn loop walks
  // forward from c_{N-1} -> c0 (kerf closing, torch off) and onward into
  // the slug -- mirroring the arc-lead behaviour.
  out->points.reserve(rotated.size() + 3);
  out->points.push_back(straight->pierce);
  out->points.insert(out->points.end(), rotated.begin(), rotated.end());
  out->points.push_back(rotated.front());
  out->lead_in_count = 1;
  if (is_inside) {
    out->lead_out_count = 1;
  }
  else {
    out->points.push_back(straight->pierce);
    out->lead_out_count = 1;
  }
  out->lead_in_is_arc = false;
  return true;
}
