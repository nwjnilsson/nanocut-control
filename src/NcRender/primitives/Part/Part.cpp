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
        if (mpos_x < path.bbox.min.x - pad || mpos_x > path.bbox.max.x + pad ||
            mpos_y < path.bbox.min.y - pad || mpos_y > path.bbox.max.y + pad) {
          global_index++;
          continue;
        }
        for (size_t i = 1; i < path.built_points.size(); i++) {
          if (geo::lineIntersectsWithCircle(
                { { path.built_points[i - 1].x, path.built_points[i - 1].y },
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
                { { path.built_points.front().x, path.built_points.front().y },
                  { path.built_points.back().x, path.built_points.back().y } },
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
          if (checkIfPointIsInsidePath(path.built_points, { mpos_x, mpos_y })) {
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
    m_tool_path_arrows.clear();

    // Build small V-shaped direction arrows along a toolpath's contour proper
    // (clear of the lead-in / lead-out), indicating cut direction. Mirrors the
    // control view's gcode arrows (gcode.cpp). One arrow per ~arrow_spacing of
    // travel, at least one. Both lengths are in built-point space (scaled by
    // m_control.scale) so they track the geometry.
    const double arrow_len = 2.0 * m_control.scale;
    const double arrow_spacing = 20.0 * m_control.scale;
    auto build_arrows =
      [arrow_len, arrow_spacing](
        const Toolpath& tp) -> std::vector<std::vector<Point2d>> {
      const size_t n = tp.points.size();
      if (n < 2)
        return {};
      const size_t cfirst = std::min(tp.lead_in_count, n - 1);
      const size_t clast =
        (tp.lead_out_count < n) ? (n - 1 - tp.lead_out_count) : (n - 1);
      if (clast <= cfirst)
        return {};

      // Cumulative arc length across the contour-proper segments. cum[k] is the
      // distance from cfirst to vertex cfirst+k.
      const size_t        seg_count = clast - cfirst;
      std::vector<double> cum(seg_count + 1, 0.0);
      for (size_t k = 0; k < seg_count; ++k)
        cum[k + 1] = cum[k] + geo::distance(tp.points[cfirst + k],
                                            tp.points[cfirst + k + 1]);
      const double total = cum[seg_count];
      if (total <= 0.0)
        return {};

      // Divide the contour into `count` equal spans and drop an arrow at each
      // span's centre, so arrows sit ~arrow_spacing apart and clear of the ends.
      const long count = std::max<long>(1, std::lround(total / arrow_spacing));
      const double step = total / static_cast<double>(count);

      std::vector<std::vector<Point2d>> arrows;
      arrows.reserve(count);
      size_t seg = 0;
      for (long a = 0; a < count; ++a) {
        const double target = (static_cast<double>(a) + 0.5) * step;
        while (seg + 1 < seg_count && cum[seg + 1] < target)
          ++seg;
        const Point2d& pa = tp.points[cfirst + seg];
        const Point2d& pb = tp.points[cfirst + seg + 1];
        const double   seg_len = cum[seg + 1] - cum[seg];
        const double   t = seg_len > 0.0 ? (target - cum[seg]) / seg_len : 0.0;
        const Point2d  apex = { pa.x + (pb.x - pa.x) * t,
                                pa.y + (pb.y - pa.y) * t };
        const double   angle = geo::measurePolarAngle(pb, pa);
        const Point2d p1 = geo::createPolarLine(apex, angle + 30, arrow_len).end;
        const Point2d p2 = geo::createPolarLine(apex, angle - 30, arrow_len).end;
        arrows.push_back({ p1, apex, p2 });
      }
      return arrows;
    };

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
              // Lead-IN is sized by lead_in_length for BOTH inside and
              // outside contours; createToolpathLeads applies the
              // lead_out_length as a real lead-out on outside contours only.
              const int base_direction = path.is_inside_contour ? -1 : +1;
              std::vector<std::vector<Point2d>> tpaths = offsetPath(
                path.built_points,
                base_direction * (double) fabs(layer.toolpath_offset));

              // A single source contour can offset into MORE than one polygon.
              // When the kerf offset is large relative to thin features (e.g. a
              // narrow slot or sliver, or the whole part scaled down), the
              // offset self-intersects and Clipper emits, alongside the primary
              // boundary, one or more polygons of OPPOSITE winding: voids
              // trapped inside the material (outside source) or islands of
              // material (inside source). These flip the inside/outside sense,
              // so each result polygon must be classified by its OWN winding
              // rather than inheriting the source path's -- otherwise a trapped
              // void gets an outside-style lead-in/lead-out cut into finished
              // material. Clipper canonicalizes winding and the outermost
              // boundary encloses the rest, so the largest-|area| polygon is
              // nesting depth 0; a polygon whose winding is opposite to it sits
              // at odd depth and flips the source inside/outside flag (this XOR
              // composes correctly through any nesting depth).
              // A contour can also offset away to nothing (e.g. a small hole
              // shrunk past its inradius), leaving no polygons -- skip it.
              if (tpaths.empty())
                continue;
              std::vector<double> tp_area(tpaths.size(), 0.0);
              size_t              dominant = 0;
              for (size_t x = 0; x < tpaths.size(); x++) {
                tp_area[x] = geo::signedPolygonArea(tpaths[x]);
                if (std::fabs(tp_area[x]) > std::fabs(tp_area[dominant]))
                  dominant = x;
              }
              const bool dominant_positive = tp_area[dominant] >= 0.0;

              for (size_t x = 0; x < tpaths.size(); x++) {
                const bool opposite_winding =
                  (tp_area[x] >= 0.0) != dominant_positive;
                const bool tp_is_inside =
                  path.is_inside_contour != opposite_winding;
                const int direction = tp_is_inside ? -1 : +1;

                // Default cut direction, for plasma cut quality. The right-hand
                // side of a plasma kerf comes out square while the left side
                // bevels, so we keep the finished part on the square side by
                // running OUTSIDE (external) profiles clockwise and INSIDE
                // (holes) counter-clockwise -- the same alternation SheetCam
                // applies automatically. signedPolygonArea is +ve for CCW, -ve
                // for CW; built-point space is Y-up and the emitter's X/Y
                // negation is a 180 deg rotation that preserves winding, so this
                // winding reaches the machine unchanged. Clipper canonicalizes
                // its own output winding, so normalize explicitly here rather
                // than rely on it.
                const bool want_ccw = tp_is_inside; // hole -> CCW, profile -> CW
                if ((geo::signedPolygonArea(tpaths[x]) >= 0.0) != want_ccw)
                  std::reverse(tpaths[x].begin(), tpaths[x].end());

                // Manual override (CAM context menu "Reverse Direction") flips
                // the default winding. Must happen AFTER offsetPath/normalize;
                // createToolpathLeads then derives a valid
                // lead-in/lead-out/overburn for the resulting direction.
                if (path.reversed)
                  std::reverse(tpaths[x].begin(), tpaths[x].end());
                Toolpath tp;
                if (createToolpathLeads(&tp,
                                        tpaths[x],
                                        m_control.lead_in_length,
                                        m_control.lead_out_length,
                                        direction)) {
                  tp.is_closed_contour = true;
                  tp.is_inside_contour = tp_is_inside;
                  tp.kerf_width = std::fabs(layer.toolpath_offset) * 2.0;
                  for (auto& arrow : build_arrows(tp))
                    m_tool_path_arrows.push_back(std::move(arrow));
                  m_tool_paths.push_back(std::move(tp));
                }
              }
            }
            else {
              Toolpath tp;
              tp.points = path.built_points;
              if (path.reversed)
                std::reverse(tp.points.begin(), tp.points.end());
              tp.is_closed_contour = false;
              tp.is_inside_contour = false;
              tp.kerf_width = std::fabs(layer.toolpath_offset) * 2.0;
              for (auto& arrow : build_arrows(tp))
                m_tool_path_arrows.push_back(std::move(arrow));
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
      glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), path.built_points.data());
      glDrawArrays(path.is_closed ? GL_LINE_LOOP : GL_LINE_STRIP,
                   0,
                   path.built_points.size());
    }
  }
  // Toolpaths, colored per segment: lead-ins/lead-outs in the lead color, the
  // contour proper in the cut color. Sub-strips share their boundary vertex so
  // there are no gaps. Inside (hole) contours have no real lead-out (just the
  // closing duplicate), so their trailing vertices stay in the cut color.
  // With kerf-width display on, each sub-strip is stroked into a filled swath
  // of the actual kerf (in built-point space, so it scales with zoom); inner
  // edge sits on the finished contour, outer edge half a kerf into the scrap.
  const Color4f* cut_c = m_toolpath_cut_color;
  const Color4f* lead_c = m_toolpath_lead_color;
  for (auto& tp : m_tool_paths) {
    const size_t n = tp.points.size();
    if (n < 2)
      continue;
    const Point2d* pts = tp.points.data();

    const bool   has_lead_out = !tp.is_inside_contour && tp.lead_out_count > 1;
    const size_t contour_end =
      has_lead_out ? (n - 1 - tp.lead_out_count) : (n - 1);
    const size_t lead_in_end = std::min(tp.lead_in_count, n - 1);

    const double half_kerf = tp.kerf_width * 0.5;
    const bool   ribbon = m_show_kerf_width && half_kerf > 0.0;

    auto draw_strip = [&](size_t first, size_t last, const Color4f* c) {
      if (last <= first)
        return;
      glColor4f(c->r, c->g, c->b, c->a);
      glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), pts + first);
      glDrawArrays(GL_LINE_STRIP, 0, last - first + 1);
    };

    // Stroke polyline pts[first..last] into a kerf-width swath. Each vertex is
    // offset +/- half_kerf along the angle-bisector (miter) normal so adjacent
    // segments join without gaps; the miter is clamped to keep sharp corners
    // from spiking. Emitted as one GL_TRIANGLE_STRIP of interleaved
    // left/right edge vertices.
    auto seg_normal = [](const Point2d& a, const Point2d& b) -> Point2d {
      const double dx = b.x - a.x, dy = b.y - a.y;
      const double len = std::sqrt(dx * dx + dy * dy);
      if (len <= 1e-12)
        return { 0.0, 0.0 };
      return { -dy / len, dx / len }; // left-hand normal
    };
    auto draw_ribbon = [&](size_t first, size_t last, const Color4f* c) {
      if (last <= first)
        return;
      constexpr double miter_limit = 3.0;
      m_ribbon_buf.clear();
      m_ribbon_buf.reserve((last - first + 1) * 2);
      for (size_t i = first; i <= last; ++i) {
        Point2d nrm;
        double  scale = half_kerf;
        if (i == first) {
          nrm = seg_normal(pts[i], pts[i + 1]);
        }
        else if (i == last) {
          nrm = seg_normal(pts[i - 1], pts[i]);
        }
        else {
          const Point2d n1 = seg_normal(pts[i - 1], pts[i]);
          const Point2d n2 = seg_normal(pts[i], pts[i + 1]);
          const double  mx = n1.x + n2.x, my = n1.y + n2.y;
          const double  mlen = std::sqrt(mx * mx + my * my);
          if (mlen <= 1e-9) {
            nrm = n1; // ~180 deg turn-back; just use the incoming normal
          }
          else {
            nrm = { mx / mlen, my / mlen };
            double d = nrm.x * n1.x + nrm.y * n1.y; // cos(half-angle)
            if (d < 1.0 / miter_limit)
              d = 1.0 / miter_limit; // clamp to the miter limit
            scale = half_kerf / d;
          }
        }
        m_ribbon_buf.push_back(
          { pts[i].x + nrm.x * scale, pts[i].y + nrm.y * scale });
        m_ribbon_buf.push_back(
          { pts[i].x - nrm.x * scale, pts[i].y - nrm.y * scale });
      }
      glColor4f(c->r, c->g, c->b, c->a);
      glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), m_ribbon_buf.data());
      glDrawArrays(GL_TRIANGLE_STRIP, 0, m_ribbon_buf.size());
    };

    auto draw_seg = [&](size_t first, size_t last, const Color4f* c) {
      if (ribbon)
        draw_ribbon(first, last, c);
      else
        draw_strip(first, last, c);
    };

    // Lead-in [0 .. lead_in_end] (joins the first contour vertex).
    if (lead_in_end > 0)
      draw_seg(0, lead_in_end, lead_c);
    // Contour proper [lead_in_end .. contour_end].
    draw_seg(lead_in_end, contour_end, cut_c);
    // Lead-out [contour_end .. n-1] (outside contours only).
    if (has_lead_out)
      draw_seg(contour_end, n - 1, lead_c);
  }
  // Cut-direction arrows over the toolpaths.
  const Color4f* arrow_c = m_toolpath_arrow_color;
  glColor4f(arrow_c->r, arrow_c->g, arrow_c->b, arrow_c->a);
  for (auto& arrow : m_tool_path_arrows) {
    if (arrow.empty())
      continue;
    glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), arrow.data());
    glDrawArrays(GL_LINE_STRIP, 0, arrow.size());
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

struct StraightLead {
  Point2d pierce;
  size_t  attach_index;
};

// Distance from `origin` along the unit vector `dir` to the first crossing
// of the polygon boundary, ignoring hits within `t_eps` (the two edges that
// meet at the attach vertex the ray starts on). Returns +inf when the ray
// never re-crosses the contour -- e.g. it heads out into open scrap. Used to
// size a straight lead so its pierce stops short of the opposite wall.
double rayPolygonClearance(const std::vector<Point2d>& poly,
                           Point2d                     origin,
                           Point2d                     dir,
                           double                      t_eps)
{
  const size_t N = poly.size();
  double       best = std::numeric_limits<double>::infinity();
  for (size_t i = 0; i < N; ++i) {
    const Point2d& a = poly[i];
    const Point2d& b = poly[(i + 1) % N];
    const double   ex = b.x - a.x;
    const double   ey = b.y - a.y;
    const double   det = ex * dir.y - ey * dir.x;
    if (std::fabs(det) < 1e-12)
      continue; // ray parallel to this edge
    const double rx = a.x - origin.x;
    const double ry = a.y - origin.y;
    const double t = (ex * ry - ey * rx) / det;       // distance along unit dir
    const double u = (dir.x * ry - dir.y * rx) / det; // param along edge
    if (t > t_eps && u >= 0.0 && u <= 1.0 && t < best)
      best = t;
  }
  return best;
}

// Straight-lead pierce. A straight lead is fully defined by an attach vertex
// and the inward normal there (pierce = attach + L * normal), so rather than
// offsetting the whole polygon and snapping to a vertex we compute the pierce
// analytically and size L directly. For each contour vertex we shoot the
// scrap-side normal across the opening; the lead length is the requested
// radius clamped to half that clearance, so the pierce lands near the middle
// of the opening instead of grazing the far wall.
//
// We then keep the vertex whose pierce sits FARTHEST from every wall (the most
// centred spot in the scrap), NOT the one with the longest lead. Maximising
// length rewards shooting down the long axis of a thin sliver and parks the
// pierce in the narrow wedge at its tip, pointing out toward the corner;
// maximising pierce clearance instead lands it on the wide centreline aimed
// into the body. It is also self-limiting: it never lengthens a lead past the
// point where the pierce starts closing on a wall. Returns the pierce and the
// attach index, or std::nullopt if no vertex can support even a minimal lead.
std::optional<StraightLead> findStraightPierce(
  const std::vector<Point2d>& contour, double radius, int direction)
{
  const size_t N = contour.size();
  if (N < 3)
    return std::nullopt;

  const double radius_abs = std::fabs(radius);
  const double min_feasible = 0.05;
  const bool   is_inside = direction < 0;
  // A short probe to pick which normal points into the scrap, and a small
  // ray epsilon to skip the self-intersection at the attach vertex.
  const double probe =
    std::max(static_cast<double>(min_feasible), radius_abs * 0.01);
  const double t_eps =
    std::max(static_cast<double>(1e-3), radius_abs * 1e-3);
  // Clearances within this band count as a tie; break ties toward the longer
  // lead so we don't pick a stubby one when an equally-centred fuller lead
  // fits.
  const double tie_eps = 1e-3;

  std::optional<StraightLead> best;
  double                      best_clearance = -1.0;
  double                      best_len = -1.0;

  for (size_t i = 0; i < N; ++i) {
    const Point2d attach = contour[i];
    const Point2d nxt = contour[(i + 1) % N];
    const double  ex = nxt.x - attach.x;
    const double  ey = nxt.y - attach.y;
    const double  elen = std::sqrt(ex * ex + ey * ey);
    if (elen <= 0.0)
      continue;

    // Unit normal to the local edge, flipped to the scrap side: the slug
    // interior for inside contours, the open scrap for outside contours, so
    // the pierce never sits in finished material.
    Point2d       n = { -ey / elen, ex / elen };
    const Point2d probe_pt = { attach.x + n.x * probe, attach.y + n.y * probe };
    if (geo::pointIsInsidePolygon(contour, probe_pt) != is_inside)
      n = { -n.x, -n.y };

    // Longest lead this vertex can support. Clamp to half the clearance to
    // the opposite wall (centres the pierce); open scrap keeps full radius.
    const double clearance = rayPolygonClearance(contour, attach, n, t_eps);
    double       len = radius_abs;
    if (clearance < std::numeric_limits<double>::infinity())
      len = std::min(len, clearance * 0.5);
    if (len < min_feasible)
      continue;

    const Point2d pierce = { attach.x + n.x * len, attach.y + n.y * len };
    // Drop candidates whose pierce isn't genuinely in the scrap: at a sharp
    // tip the short probe can cross a near wall and mis-flip the normal, which
    // would otherwise place the pierce in finished material.
    if (geo::pointIsInsidePolygon(contour, pierce) != is_inside)
      continue;

    const double pierce_clearance =
      geo::pointToPolygonDistance(contour, pierce);
    const bool better =
      pierce_clearance > best_clearance + tie_eps ||
      (pierce_clearance > best_clearance - tie_eps && len > best_len);
    if (better) {
      if (pierce_clearance > best_clearance)
        best_clearance = pierce_clearance;
      best_len = len;
      best = StraightLead{ pierce, i };
    }
  }

  return best;
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

// Build a lead-OUT polyline that departs the contour's start vertex
// (contour[0], where the cut loop closes) tangent to the direction of
// travel and curves outward into the scrap. `contour` is the rotated,
// distinct-vertex offset polygon. Returns the lead-out vertices AFTER the
// start point (the caller has already appended contour[0] as the closing
// vertex), or empty if no lead-out stays clear of the part within slop.
//
// Mirrors the lead-in: geo::buildArcLead(attach, T, r, 90, s) returns a
// polyline [pierce ... attach] whose motion ARRIVES at attach along T.
// Reversing it gives a path that DEPARTS attach along -T, so we pass
// T = -forward to depart along the forward travel tangent. The correct
// sweep side (into the scrap) is found by trying both signs and keeping the
// one whose vertices stay outside the contour.
std::vector<Point2d> buildLeadOut(const std::vector<Point2d>& contour,
                                  double                      lead_out_abs,
                                  double                      side_slop)
{
  const size_t N = contour.size();
  if (N < 3 || lead_out_abs <= 0.0)
    return {};

  const Point2d start = contour[0];
  const double  fdx = contour[1].x - start.x;
  const double  fdy = contour[1].y - start.y;
  const double  flen = std::sqrt(fdx * fdx + fdy * fdy);
  if (flen <= 0.0)
    return {};
  const Point2d forward = { fdx / flen, fdy / flen };

  // A lead-out vertex is acceptable unless it sits INSIDE the contour by
  // more than the slop tolerance (i.e. it has cut back into the part).
  auto stays_in_scrap = [&](const std::vector<Point2d>& pts,
                            size_t                      first) -> bool {
    for (size_t i = first; i < pts.size(); ++i) {
      if (geo::pointIsInsidePolygon(contour, pts[i]) &&
          geo::pointToPolygonDistance(contour, pts[i]) > side_slop) {
        return false;
      }
    }
    return true;
  };

  // ---- Arc lead-out -------------------------------------------------------
  const Point2d tangent = { -forward.x, -forward.y };
  for (int s = 1; s >= -1; s -= 2) {
    std::vector<Point2d> arc =
      geo::buildArcLead(start, tangent, lead_out_abs, 90.0, s, 16);
    if (arc.size() < 2)
      continue;
    std::reverse(arc.begin(), arc.end()); // arc.front() == start
    if (stays_in_scrap(arc, 1))
      return std::vector<Point2d>(arc.begin() + 1, arc.end());
  }

  // ---- Straight lead-out (outward normal) ---------------------------------
  // Last resort when no tangent arc fits: leave along the outward normal so
  // the torch-off point is guaranteed off the finished edge.
  Point2d      n = { -forward.y, forward.x };
  const double probe =
    std::max(static_cast<double>(0.1), lead_out_abs * 0.05);
  const Point2d test = { start.x + n.x * probe, start.y + n.y * probe };
  if (geo::pointIsInsidePolygon(contour, test))
    n = { forward.y, -forward.x };
  std::vector<Point2d> line = { { start.x + n.x * lead_out_abs,
                                  start.y + n.y * lead_out_abs } };
  if (stays_in_scrap(line, 0))
    return line;

  return {};
}

} // namespace

bool Part::createToolpathLeads(Toolpath*                   out,
                               const std::vector<Point2d>& contour_in,
                               double                      lead_in_len,
                               double                      lead_out_len,
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

  const bool   is_inside = direction < 0;
  const double lead_in_abs = std::fabs(lead_in_len);
  // Lead-outs are meaningful only on outside contours; inside contours use
  // overburn in the emitter (so the arc extinguishes inside the falling
  // slug) and must never get a lead-out.
  const double lead_out_abs = is_inside ? 0.0 : std::fabs(lead_out_len);
  const double orientation = geo::signedPolygonArea(contour) >= 0 ? 1.0 : -1.0;

  // Slop tolerance shared by the lead-in and lead-out side checks: RDP
  // simplification with `m_control.smoothing` plus Clipper offset rounding /
  // CleanPolygons let the polygon deviate from the true contour, so allow at
  // least 0.5 mm of slop or 2x smoothing, whichever is larger.
  const double side_slop =
    std::max(static_cast<double>(0.5), m_control.smoothing * 2.0);

  // ---- Build the lead-IN --------------------------------------------------
  // Produces `lead_in_pts` (pierce -> ... -> just before attach) and the
  // contour index `attach_index` the lead lands on. Empty lead_in_pts means
  // no lead-in: either lead_in_len == 0, or geometry too tight to attach a
  // lead -- in which case we start the cut on the contour rather than drop
  // the toolpath entirely.
  std::vector<Point2d> lead_in_pts;
  size_t               attach_index = 0;

  if (lead_in_abs > 0.0) {
    const double radius_abs = lead_in_abs;
    // Look for a "straight run": a span of vertices [start, end] where the
    // chord approximates the contour within dev_tolerance. This bridges the
    // natural curvature of dense polygons (e.g. circles discretized to many
    // short segments by jtRound + Clipper) where no single segment would be
    // long enough for an arc lead. The arc curves AWAY from the contour, so
    // the run length needn't be 2*radius -- the real feasibility test is the
    // side check below.
    const double min_segment_len =
      std::max(static_cast<double>(0.5), radius_abs * 0.5);
    const double dev_tolerance =
      std::max(static_cast<double>(0.25), radius_abs * 0.1);
    geo::StraightRun run;
    const bool       run_found =
      geo::longestStraightRun(contour, min_segment_len, dev_tolerance, &run);

    bool arc_built = false;
    if (run_found) {
      // Place attach AT an actual contour vertex (mid-run by vertex index)
      // and use the LOCAL edge direction at that vertex as the tangent, so
      // the cut continues from arc-end into contour[mid+1] along an actual
      // contour edge with no perpendicular jump ("spike").
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

        // buildArcLead's sweep_sign chooses the side the arc curves toward.
        // We want the INTERIOR side for inside contours (pierce in the slug)
        // and the EXTERIOR side for outside contours (pierce in the scrap):
        // sweep_sign = -direction * orientation.
        const int sweep_sign = static_cast<int>(-direction * orientation);

        // 90 deg sweep, sized at the user's lead-in length.
        auto arc_pts = geo::buildArcLead(
          attach, tangent_dir, radius_abs, 90.0, sweep_sign, 16);

        if (arc_pts.size() >= 2) {
          // (1) Whole-arc side check: every arc vertex (except `attach`, a
          // boundary point) must sit on the correct side of the contour,
          // within slop. (2) Pierce headroom: the pierce must clear the
          // nearest contour edge by >= radius_abs * 0.5 so the lead doesn't
          // graze the opposite wall of a narrow pocket.
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
            // Pierce headroom: the pierce (arc start) must clear the nearest
            // contour edge by >= radius_abs * 0.5. pointToPolygonDistance
            // returns the distance to the closest wall, so in a narrow pocket
            // this is the OPPOSITE-wall distance -- rejecting here keeps the
            // pierce from landing right against the far wall.
            const double pierce_clearance =
              geo::pointToPolygonDistance(contour, arc_pts.front());
            if (pierce_clearance < radius_abs * 0.5)
              arc_ok = false;
          }
          // Only commit the arc when it passed BOTH checks; otherwise leave
          // arc_built false so we fall through to the straight lead-in.
          if (arc_ok) {
            // attach == contour[mid]; after rotating the contour to start at
            // mid, the arc's snapped final vertex == rotated[0], so drop it.
            attach_index = mid;
            lead_in_pts.assign(arc_pts.begin(), arc_pts.end() - 1);
            out->lead_in_is_arc = true;
            arc_built = true;
          }
        }
      }
    }

    if (!arc_built) {
      // ---- Fallback: straight lead-in ----
      auto straight = findStraightPierce(contour, radius_abs, direction);
      if (straight) {
        attach_index = straight->attach_index;
        lead_in_pts = { straight->pierce };
        out->lead_in_is_arc = false;
      }
      else {
        LOG_F(INFO,
              "[createToolpathLeads] No feasible straight lead-in; cut will "
              "start on the contour (no lead-in).");
      }
    }
  }

  // Rotate the contour so the attach vertex sits at index 0. With no lead-in
  // this is the identity (attach_index == 0).
  std::vector<Point2d> rotated = rotateContour(contour, attach_index);

  // ---- Assemble lead-in + contour + closing vertex ------------------------
  out->points.reserve(lead_in_pts.size() + rotated.size() + 18);
  out->points.insert(out->points.end(), lead_in_pts.begin(), lead_in_pts.end());
  out->lead_in_count = lead_in_pts.size();
  out->points.insert(out->points.end(), rotated.begin(), rotated.end());
  // Closing duplicate: returns to the start vertex so the final contour edge
  // is cut / the kerf is closed. Marked as lead-out (lead_out_count starts at
  // 1) so [contour_first, contour_last] bounds the cyclic contour vertices.
  // For inside contours the emitter fires torch_off here and walks the
  // overburn forward from this point; for outside contours the lead-out
  // below continues the cut from here into the scrap.
  out->points.push_back(rotated.front());
  out->lead_out_count = 1;

  // ---- Build the lead-OUT (outside contours only) -------------------------
  if (lead_out_abs > 0.0) {
    std::vector<Point2d> lead_out_pts =
      buildLeadOut(rotated, lead_out_abs, side_slop);
    if (!lead_out_pts.empty()) {
      out->points.insert(
        out->points.end(), lead_out_pts.begin(), lead_out_pts.end());
      out->lead_out_count += lead_out_pts.size();
    }
    else {
      LOG_F(INFO,
            "[createToolpathLeads] Lead-out rejected (arc and straight both "
            "cross the contour); closing without a lead-out.");
    }
  }

  return true;
}
