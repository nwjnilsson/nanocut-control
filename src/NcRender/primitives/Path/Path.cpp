#include "Path.h"
#include "../../geometry/geometry.h"
#include <NcRender/NcRender.h>
#include <loguru.hpp>

#include <NcRender/gl.h>

std::string Path::getTypeName() { return "path"; }

void Path::rebuildHitCache()
{
  m_bbox = geo::calculateBoundingBox(m_points);
  if (m_points.size() >= kSimplifyThreshold) {
    m_simplified = geo::simplify(m_points, mouse_over_padding * 0.5);
  }
  else {
    m_simplified.clear();
  }
  m_hit_cache_dirty = false;
}

void Path::processMouse(float mpos_x, float mpos_y)
{
  if (!visible || m_points.size() < 2)
    return;

  if (m_hit_cache_dirty)
    rebuildHitCache();

  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;

  // Bounding box early-out
  double pad = mouse_over_padding / scale;
  if (mpos_x < m_bbox.min.x - pad || mpos_x > m_bbox.max.x + pad ||
      mpos_y < m_bbox.min.y - pad || mpos_y > m_bbox.max.y + pad) {
    if (mouse_over) {
      m_mouse_event =
        MouseHoverEvent(NcRender::EventType::MouseOut, mpos_x, mpos_y);
      mouse_over = false;
    }
    return;
  }

  // Use simplified points for hit-testing when available
  const auto& pts = m_simplified.empty() ? m_points : m_simplified;

  bool mouse_is_over_path = false;
  for (size_t i = 1; i < pts.size(); i++) {
    if (geo::lineIntersectsWithCircle(
          { { pts[i - 1].x, pts[i - 1].y }, { pts[i].x, pts[i].y } },
          { mpos_x, mpos_y },
          pad)) {
      mouse_is_over_path = true;
      break;
    }
  }
  if (m_is_closed && !mouse_is_over_path) {
    if (geo::lineIntersectsWithCircle(
          { { pts.front().x, pts.front().y }, { pts.back().x, pts.back().y } },
          { mpos_x, mpos_y },
          pad)) {
      mouse_is_over_path = true;
    }
  }

  if (mouse_is_over_path) {
    if (!mouse_over) {
      m_mouse_event =
        MouseHoverEvent(NcRender::EventType::MouseIn, mpos_x, mpos_y);
      mouse_over = true;
    }
  }
  else if (mouse_over) {
    m_mouse_event =
      MouseHoverEvent(NcRender::EventType::MouseOut, mpos_x, mpos_y);
    mouse_over = false;
  }
}
void Path::render()
{
  glPushMatrix();
  glTranslatef(offset[0], offset[1], offset[2]);
  glScalef(scale, scale, scale);
  glColor4f(color->r, color->g, color->b, color->a);
  glLineWidth(m_width);
  if (m_style == "dashed") {
    glPushAttrib(GL_ENABLE_BIT);
    glLineStipple(10, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
  }
  glEnableClientState(GL_VERTEX_ARRAY);
  glVertexPointer(2, GL_DOUBLE, sizeof(Point2d), m_points.data());
  glDrawArrays(m_is_closed ? GL_LINE_LOOP : GL_LINE_STRIP, 0, m_points.size());
  glDisableClientState(GL_VERTEX_ARRAY);
  glLineWidth(1);
  glDisable(GL_LINE_STIPPLE);
  glPopMatrix();
}
nlohmann::json Path::serialize()
{
  nlohmann::json p;
  for (int i = 0; i < m_points.size(); i++) {
    p.push_back({ { "x", m_points[i].x }, { "y", m_points[i].y }, { 0.f } });
  }
  nlohmann::json j;
  j["points"] = p;
  j["m_is_closed"] = m_is_closed;
  j["width"] = m_width;
  j["style"] = m_style;
  return j;
}
