#include "Path.h"
#include "../../geometry/geometry.h"
#include <loguru.hpp>
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

std::string Path::getTypeName() { return "path"; }
void        Path::processMouse(float mpos_x, float mpos_y)
{
  if (visible == true) {
    mpos_x = (mpos_x - offset[0]) / scale;
    mpos_y = (mpos_y - offset[1]) / scale;
    bool     mouse_is_over_path = false;
    for (int i = 1; i < m_points.size(); i++) {
      if (geo::lineIntersectsWithCircle(
            { { m_points.at(i - 1).x, m_points.at(i - 1).y },
              { m_points.at(i).x, m_points.at(i).y } },
            { mpos_x, mpos_y },
            mouse_over_padding / scale)) {
        mouse_is_over_path = true;
        break;
      }
    }
    if (m_is_closed == true) {
      if (geo::lineIntersectsWithCircle({ { m_points.at(0).x, m_points.at(0).y },
                                       { m_points.at(m_points.size() - 1).x,
                                         m_points.at(m_points.size() - 1).y } },
                                     { mpos_x, mpos_y },
                                     mouse_over_padding / scale)) {
        mouse_is_over_path = true;
      }
    }
    if (mouse_is_over_path == true) {
      if (mouse_over == false) {
        m_mouse_event = MouseHoverEvent(NcRender::EventType::MouseIn, mpos_x, mpos_y);
        mouse_over = true;
      }
    }
    else {
      if (mouse_over == true) {
        m_mouse_event = MouseHoverEvent(NcRender::EventType::MouseOut, mpos_x, mpos_y);
        mouse_over = false;
      }
    }
  }
}
void Path::render()
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
  if (m_is_closed == true) {
    glBegin(GL_LINE_LOOP);
  }
  else {
    glBegin(GL_LINE_STRIP);
  }
  for (int i = 0; i < m_points.size(); i++) {
    glVertex3f(m_points[i].x, m_points[i].y, 0.f);
  }
  glEnd();
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
