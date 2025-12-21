#include "Box.h"
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

std::string Box::getTypeName() { return "box"; }
void        Box::processMouse(float mpos_x, float mpos_y)
{
  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  if (mpos_x > m_bottom_left.x &&
      mpos_x < (m_bottom_left.x + m_width) &&
      mpos_y < (m_bottom_left.y + m_height) &&
      mpos_y > m_bottom_left.y) {
    if (mouse_over == false) {
      m_mouse_event = {
        { "event", NcRender::EventType::MouseIn },
        { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
      };
      mouse_over = true;
    }
  }
  else {
    if (mouse_over == true) {
      m_mouse_event = {
        { "event", NcRender::EventType::MouseOut },
        { "pos", { { "x", mpos_x }, { "y", mpos_y } } },
      };
      mouse_over = false;
    }
  }
}
void Box::render_rectangle_with_radius(
  float x, float y, float width, float height, float radius)
{
#define ROUNDING_POINT_COUNT 8
  Point2d top_left[ROUNDING_POINT_COUNT];
  Point2d bottom_left[ROUNDING_POINT_COUNT];
  Point2d top_right[ROUNDING_POINT_COUNT];
  Point2d bottom_right[ROUNDING_POINT_COUNT];
  if (radius == 0.0) {
    radius = std::min(width, height);
    radius *= 0.00001;
  }
  int   i = 0;
  float x_offset, y_offset;
  float step = (2.0f * 3.14159265359) / (ROUNDING_POINT_COUNT * 4),
        angle = 0.0f;
  unsigned int   index = 0, segment_count = ROUNDING_POINT_COUNT;
  Point2d bottom_left_corner = { x + radius, y - height + radius };
  while (i != segment_count) {
    x_offset = cosf(angle);
    y_offset = sinf(angle);
    top_left[index].x = bottom_left_corner.x - (x_offset * radius);
    top_left[index].y =
      (height - (radius * 2.0f)) + bottom_left_corner.y - (y_offset * radius);
    top_right[index].x =
      (width - (radius * 2.0f)) + bottom_left_corner.x + (x_offset * radius);
    top_right[index].y =
      (height - (radius * 2.0f)) + bottom_left_corner.y - (y_offset * radius);
    bottom_right[index].x =
      (width - (radius * 2.0f)) + bottom_left_corner.x + (x_offset * radius);
    bottom_right[index].y = bottom_left_corner.y + (y_offset * radius);
    bottom_left[index].x = bottom_left_corner.x - (x_offset * radius);
    bottom_left[index].y = bottom_left_corner.y + (y_offset * radius);
    top_left[index].x = top_left[index].x;
    top_left[index].y = top_left[index].y;
    top_right[index].x = top_right[index].x;
    top_right[index].y = top_right[index].y;
    bottom_right[index].x = bottom_right[index].x;
    bottom_right[index].y = bottom_right[index].y;
    bottom_left[index].x = bottom_left[index].x;
    bottom_left[index].y = bottom_left[index].y;
    angle -= step;
    ++index;
    ++i;
  }
  glBegin(GL_TRIANGLE_STRIP);
  {
    for (i = segment_count - 1; i >= 0; i--) {
      glVertex2f(top_left[i].x, top_left[i].y);
      glVertex2f(top_right[i].x, top_right[i].y);
    }
    glVertex2f(top_right[0].x, top_right[0].y);
    glVertex2f(top_right[0].x, top_right[0].y);
    glVertex2f(top_right[0].x, top_right[0].y);
    glVertex2f(top_left[0].x, top_left[0].y);
    glVertex2f(bottom_right[0].x, bottom_right[0].y);
    glVertex2f(bottom_left[0].x, bottom_left[0].y);
    for (i = 0; i != segment_count; i++) {
      glVertex2f(bottom_right[i].x, bottom_right[i].y);
      glVertex2f(bottom_left[i].x, bottom_left[i].y);
    }
  }
  glEnd();
}
void Box::render()
{
  if (visible == true) {
    glPushMatrix();
    glTranslatef(offset[0],
                 offset[1],
                 offset[2]);
    glScalef(scale,
             scale,
             scale);
    glColor4f(color[0] / 255,
              color[1] / 255,
              color[2] / 255,
              color[3] / 255);
    render_rectangle_with_radius(m_bottom_left.x,
                                       m_bottom_left.y + m_height,
                                       m_width,
                                       m_height,
                                       m_corner_radius);
    glPopMatrix();
  }
}
nlohmann::json Box::serialize()
{
  nlohmann::json j;
  j["bottom_left"]["x"] = m_bottom_left.x;
  j["bottom_left"]["y"] = m_bottom_left.y;
  j["width"] = m_width;
  j["height"] = m_height;
  j["corner_radius"] = m_corner_radius;
  return j;
}
