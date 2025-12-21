#ifndef IMAGE_
#define IMAGE_

#include "../Primitive.h"
#include "../../geometry/geometry.h"
#include "../../gui/stb_image.h"
#include <string>

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
//define something for Windows (32-bit and 64-bit, this part is common)
#include <GL/freeglut.h>
#include <GL/gl.h>
#define GL_CLAMP_TO_EDGE 0x812F
#ifdef _WIN64
//define something for Windows (64-bit only)
#else
//define something for Windows (32-bit only)
#endif
#elif __APPLE__
#include <OpenGL/glu.h>
#elif __linux__
#include <GL/glu.h>
#elif __unix__
#include <GL/glu.h>
#elif defined(_POSIX_VERSION)
// POSIX
#else
#error "Unknown compiler"
#endif

class Image : public Primitive {
public:
  float       m_position[3];
  std::string m_image_file;
  float       m_image_size[2];
  float       m_width;
  float       m_height;
  GLuint      m_texture;

  Image(Point2d p, std::string f, float w, float h)
    : m_width(w), m_height(h), m_image_file(f), m_texture(-1)
  {
    m_position[0] = p.x;
    m_position[1] = p.y;
    m_position[2] = 0;
  }

  ~Image() override
  {
    if (m_texture != static_cast<GLuint>(-1)) {
      glDeleteTextures(1, &m_texture);
    }
  }

  // Implement Primitive interface
  std::string    getTypeName() override;
  void           processMouse(float mpos_x, float mpos_y) override;
  void           render() override;
  nlohmann::json serialize() override;

  // Image-specific methods
  bool ImageToTextureFromFile(const char* filename, GLuint* out_texture,
                               int* out_width, int* out_height);
};

#endif // IMAGE_
