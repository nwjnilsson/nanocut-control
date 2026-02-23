#ifndef TEXT_
#define TEXT_

#include "../Primitive.h"
#include "NanoCut.h"
#include <stb_truetype.h>
#include <string>

#include <NcRender/gl.h>

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
