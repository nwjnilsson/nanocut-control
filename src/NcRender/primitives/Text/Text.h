#ifndef TEXT_
#define TEXT_

#include <stb_truetype.h>
#include "../Primitive.h"
#include "NanoCut.h"
#include <string>

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

class Text : public Primitive {
public:
  std::string     m_textval;
  std::string     m_font_file;
  float           m_font_size;
  Point2d         m_position;
  float           m_width;
  float           m_height;
  GLuint          m_texture;
  stbtt_bakedchar m_cdata[96];

  Text(Point2d p, std::string_view t, float s)
    : m_position(p), m_width(0), m_height(0), m_textval(t),
      m_font_file("default"), m_font_size(s), m_texture(-1)
  {
  }

  ~Text() override
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

  // Text-specific methods
  bool InitFontFromFile(const char* filename, float font_size);
  void RenderFont(float pos_x, float pos_y, std::string text);
};

#endif // TEXT_
