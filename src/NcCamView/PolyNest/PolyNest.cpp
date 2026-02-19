#include "PolyNest.h"
#include "NcCamView/NcCamView.h"
#include "NanoCut.h"

PolyNest::PolyPoint PolyNest::PolyPart::rotatePoint(PolyPoint p, double a)
{
  /*
      Assumes parts extents are already centered!
  */
  PolyPoint return_point;
  double    radians = (3.1415926f / 180.0f) * a;
  double    cos = cosf(radians);
  double    sin = sinf(radians);
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

double PolyNest::PolyNest::measureDistanceBetweenPoints(PolyPoint a,
                                                        PolyPoint b)
{
  double x = a.x - b.x;
  double y = a.y - b.y;
  return sqrtf(x * x + y * y);
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

bool PolyNest::PolyNest::checkIfPolyPartTouchesAnyPlacedPolyPart(PolyPart p)
{
  for (size_t x = 0; x < m_placed_parts.size(); x++) {
    ClipperLib::Clipper c;
    c.AddPath(
      p.m_built_polygons.front().m_vertices, ClipperLib::ptSubject, true);
    c.AddPath(m_placed_parts[x].m_built_polygons.back().m_vertices,
              ClipperLib::ptClip,
              true);
    ClipperLib::Paths solution;
    c.Execute(ClipperLib::ctIntersection,
              solution,
              ClipperLib::pftNonZero,
              ClipperLib::pftNonZero);
    if (solution.size() > 0) {
      return true;
    }
  }
  return false;
}

bool PolyNest::PolyNest::checkIfPolyPartIsInsideExtents(PolyPart  p,
                                                        PolyPoint min_point,
                                                        PolyPoint max_point)
{
  if (p.m_bbox_min.x >= min_point.x && p.m_bbox_max.x <= max_point.x &&
      p.m_bbox_min.y >= min_point.y && p.m_bbox_max.y <= max_point.y) {
    return true;
  }
  else {
    return false;
  }
}

double PolyNest::PolyNest::perpendicularDistance(const PolyPoint& pt,
                                                 const PolyPoint& lineStart,
                                                 const PolyPoint& lineEnd)
{
  double dx = lineEnd.x - lineStart.x;
  double dy = lineEnd.y - lineStart.y;
  double mag = pow(pow(dx, 2.0) + pow(dy, 2.0), 0.5);
  if (mag > 0.0) {
    dx /= mag;
    dy /= mag;
  }

  double pvx = pt.x - lineStart.x;
  double pvy = pt.y - lineStart.y;
  double pvdot = dx * pvx + dy * pvy;
  double dsx = pvdot * dx;
  double dsy = pvdot * dy;
  double ax = pvx - dsx;
  double ay = pvy - dsy;
  return pow(pow(ax, 2.0) + pow(ay, 2.0), 0.5);
}

void PolyNest::PolyNest::simplify(const std::vector<PolyPoint>& pointList,
                                  std::vector<PolyPoint>&       out,
                                  double                        epsilon)
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
    std::vector<PolyPoint> recResults1;
    std::vector<PolyPoint> recResults2;
    std::vector<PolyPoint> firstLine(pointList.begin(),
                                     pointList.begin() + index + 1);
    std::vector<PolyPoint> lastLine(pointList.begin() + index, pointList.end());
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
      // simplify(p[x], simplified, SCALE(5.0));
      ClipperLib::Path subj;
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

PolyNest::PolyPoint PolyNest::PolyNest::getDefaultOffsetXY(const PolyPart& part)
{
  double px =
    m_min_extents.x - part.m_bbox_min.x + DEFAULT_LEAD_IN + DEFAULT_KERF_WIDTH;
  double py =
    m_min_extents.y - part.m_bbox_min.y + DEFAULT_LEAD_IN + DEFAULT_KERF_WIDTH;
  return { px, py };
}

void PolyNest::PolyNest::resetBestPlacement()
{
  m_best_placement.score = -1.0;
  m_best_placement.offset_x = 0.0;
  m_best_placement.offset_y = 0.0;
  m_best_placement.angle = 0.0;
  m_best_placement.valid = false;
}

double PolyNest::PolyNest::calculatePlacementScore(const PolyPart& part,
                                                   double              offset_x,
                                                   double              offset_y,
                                                   double              angle)
{
  // Calculate how much of the part's bounding box is within material bounds
  double part_width = part.m_bbox_max.x - part.m_bbox_min.x;
  double part_height = part.m_bbox_max.y - part.m_bbox_min.y;
  
  if (part_width <= 0 || part_height <= 0) {
    return -1.0; // Invalid part
  }
  
  // Calculate the actual bounding box position
  double bbox_min_x = part.m_bbox_min.x + offset_x;
  double bbox_min_y = part.m_bbox_min.y + offset_y;
  double bbox_max_x = part.m_bbox_max.x + offset_x;
  double bbox_max_y = part.m_bbox_max.y + offset_y;
  
  // Calculate overlap with material bounds
  double overlap_x = std::min(bbox_max_x, m_max_extents.x) - std::max(bbox_min_x, m_min_extents.x);
  double overlap_y = std::min(bbox_max_y, m_max_extents.y) - std::max(bbox_min_y, m_min_extents.y);
  
  if (overlap_x <= 0 || overlap_y <= 0) {
    return -1.0; // No overlap
  }
  
  // Calculate percentage of part area within material
  double overlap_area = overlap_x * overlap_y;
  double part_area = part_width * part_height;
  double area_score = overlap_area / part_area;
  
  // Check if part collides with any placed parts
  bool collides = false;
  PolyPart temp_part = part;
  *temp_part.m_offset_x = offset_x;
  *temp_part.m_offset_y = offset_y;
  *temp_part.m_angle = angle;
  temp_part.build();
  
  for (const auto& placed_part : m_placed_parts) {
    if (checkIfPolyPartTouchesAnyPlacedPolyPart(temp_part)) {
      collides = true;
      break;
    }
  }
  
  if (collides) {
    area_score *= 0.1; // Heavy penalty for collisions
  }
  
  // Bonus for being closer to the material edge (encourages placement near material)
  double distance_to_edge = std::min(
    std::abs(bbox_min_x - m_min_extents.x),
    std::abs(bbox_max_x - m_max_extents.x));
  distance_to_edge = std::min(distance_to_edge,
                              std::abs(bbox_min_y - m_min_extents.y));
  distance_to_edge = std::min(distance_to_edge,
                              std::abs(bbox_max_y - m_max_extents.y));
  
  double edge_bonus = 1.0 - std::min(distance_to_edge / (std::max(part_width, part_height) * 2.0), 1.0);
  
  return area_score * (1.0 + edge_bonus * 0.2); // 20% bonus for edge proximity
}

void PolyNest::PolyNest::updateBestPlacement(const PolyPart& part,
                                              double              offset_x,
                                              double              offset_y,
                                              double              angle)
{
  double score = calculatePlacementScore(part, offset_x, offset_y, angle);
  
  if (score > m_best_placement.score) {
    m_best_placement.score = score;
    m_best_placement.offset_x = offset_x;
    m_best_placement.offset_y = offset_y;
    m_best_placement.angle = angle;
    m_best_placement.valid = true;
  }
}

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

void PolyNest::PolyNest::beginPlaceUnplacedPolyParts()
{
  std::sort(m_unplaced_parts.begin(),
            m_unplaced_parts.end(),
            [](const PolyPart& lhs, const PolyPart& rhs) {
              return (lhs.m_bbox_max.x - lhs.m_bbox_min.x) *
                       (lhs.m_bbox_max.y - lhs.m_bbox_min.y) >
                     (rhs.m_bbox_max.x - rhs.m_bbox_min.x) *
                       (rhs.m_bbox_max.y - rhs.m_bbox_min.y);
            });
  if (m_unplaced_parts.size() > 0 &&
      *m_unplaced_parts.front().m_offset_x == 0 &&
      *m_unplaced_parts.front().m_offset_y == 0) {
    auto& part = m_unplaced_parts.front();
    *part.m_offset_x = getDefaultOffsetXY(part).x;
    *part.m_offset_y = getDefaultOffsetXY(part).y;
  }
  m_found_valid_placement = false;
  resetBestPlacement(); // Reset best placement tracking for new nesting session
}

void PolyNest::PolyNest::setExtents(PolyPoint min, PolyPoint max)
{
  m_min_extents = min;
  m_max_extents = max;
}

bool PolyNest::PolyNest::placeUnplacedPolyPartsTick(void* p)
{
  PolyNest* self = reinterpret_cast<PolyNest*>(p);
  if (self == NULL || self->m_unplaced_parts.empty()) {
    return false; // Nothing to do, stop the timer
  }

  while (not self->m_unplaced_parts.empty()) {
    auto& part = self->m_unplaced_parts.front();

    // Use part-proportional increments for the grid search
    double part_width = part.m_bbox_max.x - part.m_bbox_min.x;
    double part_height = part.m_bbox_max.y - part.m_bbox_min.y;
    double x_step = std::max(part_width * 0.1, self->m_offset_increments.x);
    double y_step = std::max(part_height * 0.1, self->m_offset_increments.y);

    if (self->m_found_valid_placement) {
      LOG_F(INFO,
            "Found valid placement at offset (%.3f, %.3f) (rotation: %.3f)",
            *part.m_offset_x,
            *part.m_offset_y,
            *part.m_angle);
      *part.m_visible = true;
      self->m_found_valid_placement = false;
      self->m_placed_parts.push_back(self->m_unplaced_parts.front());
      self->m_unplaced_parts.erase(self->m_unplaced_parts.begin());
      if (not self->m_unplaced_parts.empty()) {
        auto& next_part = self->m_unplaced_parts.front();
        *next_part.m_offset_x = self->getDefaultOffsetXY(next_part).x;
        *next_part.m_offset_y = self->getDefaultOffsetXY(next_part).y;
      }
      self->resetBestPlacement(); // Reset best placement for next part
      continue;
    }

    // Try all rotations at the current position
    for (size_t x = 0; x < (360.0 / self->m_rotation_increments); x++) {
      // Check extents first (cheap) before collision (expensive)
      if (self->checkIfPolyPartIsInsideExtents(
            part, self->m_min_extents, self->m_max_extents) &&
          !self->checkIfPolyPartTouchesAnyPlacedPolyPart(part)) {
        self->m_found_valid_placement = true;
        break;
      }
      
      // Track near-miss placements for fallback
      self->updateBestPlacement(part, *part.m_offset_x, *part.m_offset_y, *part.m_angle);
      
      *part.m_angle += self->m_rotation_increments;
      if (*part.m_angle >= 360)
        *part.m_angle -= 360.0;
      part.build();
    }

    if (self->m_found_valid_placement) {
      continue; // Will be placed on next while iteration
    }

    // No valid rotation at this position, advance to next grid position
    *part.m_offset_x += x_step;
    if (*part.m_offset_x > self->m_max_extents.x) {
      *part.m_offset_x = self->getDefaultOffsetXY(part).x;
      *part.m_offset_y += y_step;
    }
    if (*part.m_offset_y > self->m_max_extents.y) {
      // Out of space, use best near-miss placement if available
      if (self->m_best_placement.valid && self->m_best_placement.score > 0.0) {
        LOG_F(INFO, "Using best near-miss placement (score: %.3f) at offset (%.3f, %.3f) (rotation: %.3f)",
              self->m_best_placement.score,
              self->m_best_placement.offset_x,
              self->m_best_placement.offset_y,
              self->m_best_placement.angle);
        
        *part.m_offset_x = self->m_best_placement.offset_x;
        *part.m_offset_y = self->m_best_placement.offset_y;
        *part.m_angle = self->m_best_placement.angle;
        part.build();
        *part.m_visible = true;
        self->m_placed_parts.push_back(self->m_unplaced_parts.front());
        self->m_unplaced_parts.erase(self->m_unplaced_parts.begin());
        self->resetBestPlacement();
        
        if (not self->m_unplaced_parts.empty()) {
          auto& next_part = self->m_unplaced_parts.front();
          *next_part.m_offset_x = self->getDefaultOffsetXY(next_part).x;
          *next_part.m_offset_y = self->getDefaultOffsetXY(next_part).y;
        }
        continue;
      }
      else {
        LOG_F(INFO, "Not enough material to place part!");
        *part.m_visible = true;
        return false; // Out of space, stop the timer
      }
    }
    part.build();
  }

  return false; // All parts placed, stop the timer
}
