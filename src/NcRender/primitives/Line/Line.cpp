#include "Line.h"
#include "../../geometry/geometry.h"
#include <NcRender/NcRender.h>
#include <loguru.hpp>

#include <GL/glu.h>

std::string Line::getTypeName() { return "line"; }
void        Line::processMouse(float mpos_x, float mpos_y)
{
  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  if (geo::lineIntersectsWithCircle(
        { { m_start.x, m_start.y }, { m_end.x, m_end.y } },
        { mpos_x, mpos_y },
        (mouse_over_padding / scale))) {
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
void Line::render()
{
  if (visible == true) {
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
    glBegin(GL_LINES);
    glVertex3f(m_start.x, m_start.y, 0.f);
    glVertex3f(m_end.x, m_end.y, 0.f);
    glEnd();
    glLineWidth(1);
    glDisable(GL_LINE_STIPPLE);
    glPopMatrix();
  }
}
nlohmann::json Line::serialize()
{
  nlohmann::json j;
  j["start"]["x"] = m_start.x;
  j["start"]["y"] = m_start.y;
  j["end"]["x"] = m_end.x;
  j["end"]["y"] = m_end.y;
  j["width"] = m_width;
  j["style"] = m_style;
  return j;
}
