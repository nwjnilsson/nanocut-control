#include "Arc.h"
#include <NcRender/NcRender.h>
#include <loguru.hpp>

#include <NcRender/gl.h>

std::string Arc::getTypeName() { return "arc"; }
void        Arc::processMouse(float mpos_x, float mpos_y)
{
  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  if (geo::distance(m_center, { mpos_x, mpos_y }) >
        (m_radius - (((mouse_over_padding / scale) / 2))) &&
      geo::distance(m_center, { mpos_x, mpos_y }) <
        (m_radius + (((mouse_over_padding / scale) / 2))) &&
      geo::linesIntersect(
        { geo::createPolarLine(m_center, m_start_angle, m_radius).end,
          geo::createPolarLine(m_center, m_end_angle, m_radius).end },
        { m_center, { mpos_x, mpos_y } })) {
    if (mouse_over == false) {
      m_mouse_event =
        MouseHoverEvent(NcRender::EventType::MouseIn, mpos_x, mpos_y);
      mouse_over = true;
    }
  }
  else {
    if (mouse_over == true) {
      m_mouse_event =
        MouseHoverEvent(NcRender::EventType::MouseOut, mpos_x, mpos_y);
      mouse_over = false;
    }
  }
}
void Arc::renderArc(
  double cx, double cy, double radius, double start_angle, double end_angle)
{
  double  num_segments = 50;
  Point2d start;
  Point2d sweeper;
  Point2d end;
  Point2d last_point;
  start.x = cx + (radius * cosf((start_angle) * 3.1415926f / 180.0f));
  start.y = cy + (radius * sinf((start_angle) * 3.1415926f / 180.0f));
  end.x = cx + (radius * cosf((end_angle) * 3.1415926f / 180.0f));
  end.y = cy + (radius * sinf((end_angle) * 3.1415926 / 180.0f));
  double diff =
    std::max(start_angle, end_angle) - std::min(start_angle, end_angle);
  if (diff > 180)
    diff = 360 - diff;
  double angle_increment = diff / num_segments;
  double angle_pointer = start_angle + angle_increment;
  last_point = start;
  for (int i = 0; i < num_segments; i++) {
    sweeper.x = cx + (radius * cosf((angle_pointer) * 3.1415926f / 180.0f));
    sweeper.y = cy + (radius * sinf((angle_pointer) * 3.1415926f / 180.0f));
    angle_pointer += angle_increment;
    glBegin(GL_LINES);
    glVertex3f(last_point.x, last_point.y, 0);
    glVertex3f(sweeper.x, sweeper.y, 0);
    glEnd();
    last_point = sweeper;
  }
  glBegin(GL_LINES);
  glVertex3f(last_point.x, last_point.y, 0);
  glVertex3f(end.x, end.y, 0);
  glEnd();
}
void Arc::render()
{
  glPushMatrix();
  glTranslatef(offset[0], offset[1], offset[2]);
  glScalef(scale, scale, scale);
  glColor4f(color->r / 255, color->g / 255, color->b / 255, color->a / 255);
  glLineWidth(m_width);
  if (m_style == "dashed") {
    glPushAttrib(GL_ENABLE_BIT);
    glLineStipple(10, 0xAAAA);
    glEnable(GL_LINE_STIPPLE);
  }
  renderArc(m_center.x, m_center.y, m_radius, m_start_angle, m_end_angle);
  glLineWidth(1);
  glDisable(GL_LINE_STIPPLE);
  glPopMatrix();
}
nlohmann::json Arc::serialize()
{
  nlohmann::json j;
  j["center"]["x"] = m_center.x;
  j["center"]["y"] = m_center.y;
  j["radius"] = m_radius;
  j["start_angle"] = m_start_angle;
  j["end_angle"] = m_end_angle;
  return j;
}
