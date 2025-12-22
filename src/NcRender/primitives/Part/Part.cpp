#include "Part.h"
#include "../../geometry/clipper.h"
#include "../../geometry/geometry.h"
#include "../../logging/loguru.h"
#include <NcRender/NcRender.h>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
// define something for Windows (32-bit and 64-bit, this part is common)
#  include <GL/freeglut.h>
#  include <GL/gl.h>
#  define GL_CLAMP_TO_EDGE 0x812F
#  ifdef _WIN64
// define something for Windows (64-bit only)
#  else
// define something for Windows (32-bit only)
#  endif
#elif __APPLE__
#  include <OpenGL/glu.h>
#elif __linux__
#  include <GL/glu.h>
#elif __unix__
#  include <GL/glu.h>
#elif defined(_POSIX_VERSION)
// POSIX
#else
#  error "Unknown compiler"
#endif

std::string Part::getTypeName() { return "part"; }
void        Part::processMouse(float mpos_x, float mpos_y)
{
  if (visible == true) {
    mpos_x = (mpos_x - offset[0]) / scale;
    mpos_y = (mpos_y - offset[1]) / scale;
    Geometry g;
    size_t   path_index = -1;
    if (m_control.mouse_mode == 0) {
      bool mouse_is_over_path = false;
      for (size_t x = 0; x < m_paths.size(); x++) {
        for (size_t i = 1; i < m_paths[x].built_points.size(); i++) {
          if (g.lineIntersectsWithCircle(
                { { m_paths[x].built_points.at(i - 1).x,
                    m_paths[x].built_points.at(i - 1).y },
                  { m_paths[x].built_points.at(i).x,
                    m_paths[x].built_points.at(i).y } },
                { mpos_x, mpos_y },
                mouse_over_padding / scale)) {
            path_index = x;
            mouse_is_over_path = true;
            break;
          }
        }
        if (m_paths[x].is_closed == true) {
          if (g.lineIntersectsWithCircle(
                { { m_paths[x].built_points.at(0).x,
                    m_paths[x].built_points.at(0).y },
                  { m_paths[x]
                      .built_points.at(m_paths[x].built_points.size() - 1)
                      .x,
                    m_paths[x]
                      .built_points.at(m_paths[x].built_points.size() - 1)
                      .y } },
                { mpos_x, mpos_y },
                mouse_over_padding / scale)) {
            path_index = x;
            mouse_is_over_path = true;
          }
        }
      }
      if (mouse_is_over_path == true) {
        if (mouse_over == false) {
          m_mouse_event = {
            { "event", NcRender::EventType::MouseIn },
            { "path_index", path_index },
            { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
          };
          mouse_over = true;
        }
      }
      else {
        if (mouse_over == true) {
          m_mouse_event = {
            { "event", NcRender::EventType::MouseOut },
            { "path_index", path_index },
            { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
          };
          mouse_over = false;
        }
      }
    }
    else if (m_control.mouse_mode == 1) { // nesting
      bool mouse_is_inside_perimeter = false;
      for (size_t x = 0; x < m_paths.size(); x++) {
        if (m_paths[x].is_inside_contour == false) {
          if (checkIfPointIsInsidePath(m_paths[x].built_points,
                                       { mpos_x, mpos_y })) {
            path_index = x;
            mouse_is_inside_perimeter = true;
            break;
          }
        }
      }
      if (mouse_is_inside_perimeter == true) {
        if (mouse_over == false) {
          m_mouse_event = {
            { "event", NcRender::EventType::MouseIn },
            { "path_index", path_index },
            { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
          };
          mouse_over = true;
        }
      }
      else {
        if (mouse_over == true) {
          m_mouse_event = {
            { "event", NcRender::EventType::MouseOut },
            { "path_index", path_index },
            { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
          };
          mouse_over = false;
        }
      }
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
  for (std::vector<Point2d>::iterator it = path.begin(); it != path.end();
       ++it) {
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
    m_number_of_verticies = 0;
    Geometry g;
    m_tool_paths.clear();
    for (std::vector<path_t>::iterator it = m_paths.begin();
         it != m_paths.end();
         ++it) {
      it->built_points.clear();
      try {
        std::vector<Point2d> simplified;

        // For closed paths with very few points, skip simplification to preserve geometry
        if (it->is_closed && it->points.size() <= 6) {
          simplified = it->points;
        } else {
          simplify(it->points, simplified, m_control.smoothing);
        }

        for (size_t i = 0; i < simplified.size(); i++) {
          Point2d rotated =
            g.rotatePoint({ 0, 0 }, simplified[i], m_control.angle);
          it->built_points.push_back(
            { (rotated.x + m_control.offset.x) * m_control.scale,
              (rotated.y + m_control.offset.y) * m_control.scale });
          m_number_of_verticies++;
        }
        if (it->toolpath_visible == true) {
          if (it->is_closed == true) {
            // Need at least 3 points to form a valid closed polygon
            if (it->built_points.size() < 3) {
              continue;
            }
            if (it->is_inside_contour == true) {
              std::vector<std::vector<Point2d>> tpaths = offsetPath(
                it->built_points, -(double) fabs(it->toolpath_offset));
              for (size_t x = 0; x < tpaths.size(); x++) {
                size_t count = 0;
                while (createToolpathLeads(&tpaths[x],
                                           m_control.lead_in_length,
                                           tpaths[x].front(),
                                           -1) == false &&
                       count < tpaths[x].size()) {
                  if (tpaths[x].size() > 2) {
                    tpaths[x].push_back(tpaths[x][1]);
                    tpaths[x].erase(tpaths[x].begin());
                  }
                  count++;
                }
                m_tool_paths.push_back(tpaths[x]);
              }
            }
            else {
              std::vector<std::vector<Point2d>> tpaths = offsetPath(
                it->built_points, (double) fabs(it->toolpath_offset));
              for (size_t x = 0; x < tpaths.size(); x++) {
                size_t count = 0;
                while (createToolpathLeads(&tpaths[x],
                                           m_control.lead_out_length,
                                           tpaths[x].front(),
                                           +1) == false &&
                       count < tpaths[x].size()) {
                  if (tpaths[x].size() > 2) {
                    tpaths[x].push_back(tpaths[x][1]);
                    tpaths[x].erase(tpaths[x].begin());
                  }
                  count++;
                }
                m_tool_paths.push_back(tpaths[x]);
              }
            }
          }
          else {
            m_tool_paths.push_back(it->built_points);
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
      getBoundingBox(&m_bb_min, &m_bb_max);
    }
  }
  m_last_control = m_control;
  glPushMatrix();
  glTranslatef(offset[0], offset[1], offset[2]);
  glScalef(scale, scale, scale);
  glLineWidth(m_width);
  for (std::vector<path_t>::iterator it = m_paths.begin(); it != m_paths.end();
       ++it) {
    glColor4f(it->color[0] / 255,
              it->color[1] / 255,
              it->color[2] / 255,
              it->color[3] / 255);
    if (it->is_closed == true) {
      glBegin(GL_LINE_LOOP);
    }
    else {
      glBegin(GL_LINE_STRIP);
    }
    if (m_style == "dashed") {
      glPushAttrib(GL_ENABLE_BIT);
      glLineStipple(10, 0xAAAA);
      glEnable(GL_LINE_STIPPLE);
    }
    try {
      for (int i = 0; i < it->built_points.size(); i++) {
        glVertex3f(it->built_points[i].x, it->built_points[i].y, 0.f);
      }
    }
    catch (std::exception& e) {
      LOG_F(ERROR,
            "(Part::render) Exception: %s, setting visability "
            "to false to avoid further exceptions!",
            e.what());
      visible = false;
    }
    glEnd();
  }
  for (int x = 0; x < m_tool_paths.size(); x++) {
    glColor4f(0, 1, 0, 0.5);
    glBegin(GL_LINE_STRIP);
    for (int i = 0; i < m_tool_paths[x].size(); i++) {
      glVertex3f(m_tool_paths[x][i].x, m_tool_paths[x][i].y, 0.f);
    }
    glEnd();
  }
  glLineWidth(1);
  glDisable(GL_LINE_STIPPLE);
  glPopMatrix();
}
nlohmann::json Part::serialize()
{
  nlohmann::json paths;
  for (std::vector<path_t>::iterator it = m_paths.begin(); it != m_paths.end();
       ++it) {
    nlohmann::json path;
    path["is_closed"] = it->is_closed;
    for (int i = 0; i < it->points.size(); i++) {
      path["points"].push_back(
        { { "x", it->points[i].x }, { "y", it->points[i].y }, { 0.f } });
    }
    paths.push_back(path);
  }
  nlohmann::json j;
  j["paths"] = paths;
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
  for (std::vector<path_t>::iterator it = m_paths.begin(); it != m_paths.end();
       ++it) {
    for (std::vector<Point2d>::iterator p = it->built_points.begin();
         p != it->built_points.end();
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
std::vector<std::vector<Point2d>> Part::getOrderedToolpaths()
{
  Geometry                          g;
  std::vector<std::vector<Point2d>> toolpaths = m_tool_paths;
  std::vector<std::vector<Point2d>> ret;
  if (toolpaths.size() > 0) {
    ret.push_back(toolpaths.back()); // Prime the sort
    toolpaths.pop_back();            // Remove the primed item from the haystack
    float  smallest_distance = 100000000000.0f;
    size_t winner_index = 0;
    while (toolpaths.size() > 0) {
      for (size_t x = 0; x < toolpaths.size(); x++) {
        if (toolpaths[x].size() > 0) {
          if (g.distance(toolpaths[x][0], ret.back()[0]) < smallest_distance) {
            smallest_distance = g.distance(toolpaths[x][0], ret.back()[0]);
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
        ret.push_back(toolpaths[winner_index]);
        toolpaths.erase(toolpaths.begin() + winner_index);
      }
      smallest_distance = 100000000000.0f;
      winner_index = 0;
    }
    // Find the toolpaths that have other paths inside them and push them to the
    // back of stack because they are the outside cuts and need to be cut last
    std::vector<std::vector<Point2d>> outside_paths;
    for (size_t x = 0; x < ret.size(); x++) {
      bool has_paths_inside = false;
      for (size_t i = 0; i < ret.size(); i++) {
        if (i != x) {
          if (checkIfPathIsInsidePath(ret[i], ret[x]) == true) {
            has_paths_inside = true;
          }
        }
      }
      if (has_paths_inside == true) // We are an outside contour
      {
        outside_paths.push_back(ret[x]);
        ret.erase(ret.begin() + x);
      }
    }
    // LOG_F(INFO, "Found %lu outside path/s", outside_paths.size());
    for (size_t x = 0; x < outside_paths.size(); x++) {
      ret.push_back(outside_paths[x]);
    }
  }
  return ret;
}
Point2d* Part::getClosestPoint(size_t*               index,
                               Point2d               point,
                               std::vector<Point2d>* points)
{
  Geometry g;
  double   smallest_distance = 1000000;
  int      smallest_index = 0;
  for (size_t x = 0; x < points->size(); x++) {
    double dist = g.distance(point, (*points)[x]);
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
bool Part::createToolpathLeads(std::vector<Point2d>* tpath,
                               double                length,
                               Point2d               position,
                               int                   direction)
{
  if (length == 0)
    return true;
  Geometry                          g;
  std::vector<std::vector<Point2d>> lead_offset =
    offsetPath(*tpath, (double) fabs(length) * (double) direction);
  for (size_t x = 0; x < lead_offset.size(); x++) {
    Point2d* point = getClosestPoint(NULL, position, &lead_offset[x]);
    if (point != NULL) {
      if (g.distance(position, (*point)) <= (length * 1.1)) {
        tpath->insert(tpath->begin(), 1, (*point));
        tpath->push_back((*point));
        return true;
      }
    }
  }
  return false; // Could not create a leadin from supplied point!
}
