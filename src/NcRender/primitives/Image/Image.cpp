#include "Image.h"
#include <stdio.h>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "../../geometry/geometry.h"
#include "../../gui/stb_image.h"
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

std::string Image::getTypeName() { return "image"; }
void        Image::processMouse(float mpos_x, float mpos_y)
{
  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  if (mpos_x > m_position[0] &&
      mpos_x < (m_position[0] + m_width) &&
      mpos_y > m_position[1] &&
      mpos_y < (m_position[1] + m_height)) {
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
bool Image::ImageToTextureFromFile(const char* filename,
                                                  GLuint*     out_texture,
                                                  int*        out_width,
                                                  int*        out_height)
{
  int            image_width = 0;
  int            image_height = 0;
  unsigned char* image_data =
    stbi_load(filename, &image_width, &image_height, NULL, 4);
  if (image_data == NULL) {
    return false;
  }
  // Create a OpenGL texture identifier
  GLuint image_texture;
  glGenTextures(1, &image_texture);
  glBindTexture(GL_TEXTURE_2D, image_texture);
  // Setup filtering parameters for display
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D,
                  GL_TEXTURE_WRAP_S,
                  GL_CLAMP_TO_EDGE); // This is required on WebGL for non
                                     // power-of-two textures
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE); // Same
// Upload pixels into texture
#if defined(GL_UNPACK_ROW_LENGTH) && !defined(__EMSCRIPTEN__)
  glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
#endif
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_RGBA,
               image_width,
               image_height,
               0,
               GL_RGBA,
               GL_UNSIGNED_BYTE,
               image_data);
  stbi_image_free(image_data);
  *out_texture = image_texture;
  *out_width = image_width;
  *out_height = image_height;
  return true;
}

void Image::render()
{
  glPushMatrix();
  glTranslatef(offset[0],
               offset[1],
               offset[2]);
  glScalef(
    scale, scale, scale);
  glColor4f(color.a / 255,
            color.a / 255,
            color.a / 255,
            color.a / 255);
  if (m_texture == -1) {
    int    my_image_width = 0;
    int    my_image_height = 0;
    GLuint my_image_texture = 0;
    bool   ret = ImageToTextureFromFile(m_image_file.c_str(),
                                            &m_texture,
                                            &my_image_width,
                                            &my_image_height);
    if (ret) {
      // printf("Loaded image %s, width: %d, height: %d\n",
      // m_image_file.c_str(), my_image_width, my_image_height);
      m_image_size[0] = my_image_width;
      m_image_size[1] = my_image_height;
      if (m_width == 0 && m_height == 0) {
        m_width = my_image_width;
        m_height = my_image_height;
      }
    }
    else {
      LOG_F(ERROR, "Error loading image!");
      m_texture = -1;
    }
  }
  else {
    glPushMatrix();
    glTranslatef(m_position[0], m_position[1], 0.0);
    glRotatef(angle, 0.0, 0.0, 1.0);
    glScalef(1.0f, -1.0f, 1.0f);
    double imgWidth = m_width;
    double imgHeight = m_height;
    glBindTexture(GL_TEXTURE_2D, m_texture);
    glEnable(GL_TEXTURE_2D);
    glBegin(GL_QUADS);
    glTexCoord2f(0, 0);
    glVertex2f(-imgWidth, -imgHeight);
    glTexCoord2f(1, 0);
    glVertex2f(imgWidth, -imgHeight);
    glTexCoord2f(1, 1);
    glVertex2f(imgWidth, imgHeight);
    glTexCoord2f(0, 1);
    glVertex2f(-imgWidth, imgHeight);
    glEnd();
    glDisable(GL_TEXTURE_2D);
    glPopMatrix();
  }
  glPopMatrix();
}
nlohmann::json Image::serialize()
{
  nlohmann::json j;
  j["position"]["x"] = m_position[0];
  j["position"]["y"] = m_position[1];
  j["position"]["z"] = m_position[2];
  j["width"] = m_width;
  j["height"] = m_height;
  j["image_file"] = m_image_file;
  return j;
}
