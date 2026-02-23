#include "Text.h"
#include "fonts/Sans.ttf.h"
#include <stdio.h>
#include <string>
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#include "../../geometry/geometry.h"

#include <NcRender/NcRender.h>
#include <loguru.hpp>

std::string Text::getTypeName() { return "text"; }
void        Text::processMouse(float mpos_x, float mpos_y)
{
  mpos_x = (mpos_x - offset[0]) / scale;
  mpos_y = (mpos_y - offset[1]) / scale;
  if (mpos_x > m_position.x && mpos_x < (m_position.x + m_width) &&
      mpos_y > m_position.y && mpos_y < (m_position.y + m_height)) {
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
bool Text::InitFontFromFile(const char* filename, float font_size)
{
  unsigned char  temp_bitmap[512 * 512];
  size_t         ttf_buffer_size = 0;
  unsigned char* ttf_buffer = NULL;
  if (std::string(filename) == "default") {
    ttf_buffer_size = sizeof(Sans_ttf);
    ttf_buffer =
      (unsigned char*) malloc(ttf_buffer_size * sizeof(unsigned char));
    if (ttf_buffer != NULL) {
      for (int x = 0; x < sizeof(Sans_ttf); x++) {
        ttf_buffer[x] = Sans_ttf[x];
      }
    }
    else {
      LOG_F(INFO, "Failed to allocate memory to copy Sans_ttf into!");
      return false;
    }
  }
  else {
    FILE* fp;
    fp = fopen(std::string(filename).c_str(), "rb");
    if (fp) {
      fseek(fp, 0L, SEEK_END);
      ttf_buffer_size = ftell(fp);
      fseek(fp, 0L, SEEK_SET);
      ttf_buffer =
        (unsigned char*) malloc(ttf_buffer_size * sizeof(unsigned char));
      if (ttf_buffer != NULL) {
        size_t read = fread(ttf_buffer, 1, ttf_buffer_size, fp);
        if (read != ttf_buffer_size) {
          LOG_F(INFO, "Expected %zu bytes, got %zu", ttf_buffer_size, read);
        }
      }
      else {
        LOG_F(INFO, "Failed to allocate memory to copy Sans_ttf into!");
        return false;
      }
      fclose(fp);
    }
    else {
      LOG_F(WARNING, "Could not open font file: %s", filename);
      return false;
    }
  }
  stbtt_BakeFontBitmap(
    ttf_buffer, 0, font_size, temp_bitmap, 512, 512, 32, 96, m_cdata);
  glGenTextures(1, &m_texture);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glTexImage2D(GL_TEXTURE_2D,
               0,
               GL_ALPHA,
               512,
               512,
               0,
               GL_ALPHA,
               GL_UNSIGNED_BYTE,
               temp_bitmap);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  return true;
}
void Text::RenderFont(float pos_x, float pos_y, std::string text)
{
  m_width = 0;
  m_height = 0;
  glEnable(GL_TEXTURE_2D);
  glBindTexture(GL_TEXTURE_2D, m_texture);
  glPushMatrix();
  glRotatef(angle, 0.0, 0.0, 1.0);
  glBegin(GL_QUADS);
  for (int x = 0; x < text.size(); x++) {
    if ((int) text[x] >= 32 && (int) text[x] < 128) {
      stbtt_aligned_quad q;
      stbtt_GetBakedQuad(
        m_cdata, 512, 512, text[x] - 32, &pos_x, &pos_y, &q, 1);
      glTexCoord2f(q.s0, q.t1);
      glVertex2f(q.x0, -q.y1);
      glTexCoord2f(q.s1, q.t1);
      glVertex2f(q.x1, -q.y1);
      glTexCoord2f(q.s1, q.t0);
      glVertex2f(q.x1, -q.y0);
      glTexCoord2f(q.s0, q.t0);
      glVertex2f(q.x0, -q.y0);
      m_width += std::max(q.x0, q.x1) - std::min(q.x0, q.x1);
      if ((std::max(q.y0, q.y1) - std::min(q.y0, q.y1)) > m_height) {
        m_height = (std::max(q.y0, q.y1) - std::min(q.y0, q.y1));
      }
    }
  }
  glEnd();
  glPopMatrix();
  glDisable(GL_TEXTURE_2D);
}
void Text::render()
{
  glPushMatrix();
  glTranslatef(offset[0], offset[1], offset[2]);
  glScalef(scale, scale, scale);
  glColor4f(color->r / 255, color->g / 255, color->b / 255, color->a / 255);
  if (m_texture == -1) {
    bool ret = InitFontFromFile(m_font_file.c_str(), m_font_size);
    if (ret == false) {
      LOG_F(WARNING, "Could not init font: %s\n", m_font_file.c_str());
      m_texture = -1;
    }
    else {
      render();
    }
  }
  else {
    RenderFont(m_position.x, -m_position.y, m_textval);
  }
  glPopMatrix();
}
nlohmann::json Text::serialize()
{
  nlohmann::json j;
  j["textval"] = m_textval;
  j["font_file"] = m_font_file;
  j["font_size"] = m_font_size;
  j["position"]["x"] = m_position.x;
  j["position"]["y"] = m_position.y;
  j["width"] = m_width;
  j["height"] = m_height;
  return j;
}
