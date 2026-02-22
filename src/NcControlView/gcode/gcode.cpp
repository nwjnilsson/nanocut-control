#include "gcode.h"
#include "../../NcApp/NcApp.h"
#include "../hmi/hmi.h"
#include <NcControlView/NcControlView.h>
#include <NcRender/geometry/geometry.h>
#include <fstream>
#include <loguru.hpp>

// Helper function for splitting strings (can remain as a free function)
std::vector<std::string> gcode_split(std::string str, char delimiter)
{
  std::vector<std::string> internal;
  std::stringstream        ss(str); // Turn the string into a stream.
  std::string              tok;
  while (std::getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }
  return internal;
}

// Constructor
GCode::GCode(NcApp* app, NcControlView* view) : m_app(app), m_view(view) {}

// Public API
std::string GCode::getFilename() const { return m_filename; }

void GCode::resetParseState()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  renderer.deletePrimitivesById("gcode");
  renderer.deletePrimitivesById("gcode_arrows");
  m_line_count = m_lines.size();
  m_lines_consumed = 0;
  m_line_index = 0;
  m_last_rapid_line = 0;
  m_current_path.points.clear();
  m_paths.clear();
  m_view->getDialogs().showProgressWindow(true);
  m_view->getDialogs().setProgressValue(0.0f);
}

bool GCode::openFile(const std::string& filepath)
{
  if (!m_app)
    return false;

  std::ifstream file(filepath);
  if (!file.is_open()) {
    LOG_F(ERROR, "Could not open file: %s", filepath.c_str());
    return false;
  }

  m_lines.clear();
  std::string line;
  while (std::getline(file, line)) {
    m_lines.push_back(std::move(line));
  }
  file.close();

  m_filename = filepath;
  resetParseState();
  LOG_F(
    INFO, "Opened file: %s, size: %lu lines", filepath.c_str(), m_line_count);
  return true;
}

bool GCode::loadFromLines(std::vector<std::string>&& lines)
{
  if (!m_app)
    return false;

  m_lines = lines;
  m_filename = "";
  resetParseState();
  LOG_F(INFO, "Loaded %lu lines from memory", m_line_count);
  return true;
}

const std::vector<std::string>& GCode::getLines() const { return m_lines; }

nlohmann::json GCode::parseLine(const std::string& line)
{
  nlohmann::json ret;
  std::string    line_upper = line;
  std::transform(
    line_upper.begin(), line_upper.end(), line_upper.begin(), ::toupper);
  std::string active_word = "";
  std::string x_value = "";
  std::string y_value = "";
  for (int x = 0; x < line_upper.size(); x++) {
    if (line_upper[x] == 'X') {
      active_word = "X";
    }
    else if (line_upper[x] == 'Y') {
      active_word = "Y";
    }
    else if (line_upper[x] == 'F') {
      active_word = "F";
    }
    else if (line_upper[x] == 'G') {
      active_word = "G";
    }
    else if (line_upper[x] == ' ') {
      // Eat white spaces....
    }
    else {
      if (active_word == "X") {
        x_value += line_upper[x];
      }
      if (active_word == "Y") {
        y_value += line_upper[x];
      }
    }
  }
  // Store gcode internally as negated values since
  // grbl does this for machine coordinates
  ret["x"] = -atof(x_value.c_str());
  ret["y"] = -atof(y_value.c_str());
  return ret;
}

void GCode::pushCurrentPathToViewer(int rapid_line)
{
  if (!m_app || m_current_path.points.size() == 0)
    return;
  auto& renderer = m_app->getRenderer();

  try {
    if (m_current_path.points.size() > 1) {
      std::vector<Point2d> simplified =
        geo::simplify(m_current_path.points, SCALE(0.25));
      std::vector<Point2d> path;
      int                  point_count = 0;
      for (int i = 0; i < simplified.size(); i++) {
        if ((i == 0 && i + 1 < simplified.size()) ||
            (point_count == 0 && i + 1 < simplified.size())) {
          std::vector<Point2d> arrow_path;
          Point2d midpoint = geo::midpoint(simplified[i + 1], simplified[i]);
          double  angle =
            geo::measurePolarAngle(simplified[i + 1], simplified[i]);
          Point2d p1 =
            geo::createPolarLine(midpoint, angle + 30, SCALE(0.5)).end;
          Point2d p2 =
            geo::createPolarLine(midpoint, angle - 30, SCALE(0.5)).end;
          arrow_path.push_back({ p1.x, p1.y });
          arrow_path.push_back({ midpoint.x, midpoint.y });
          arrow_path.push_back({ p2.x, p2.y });
          Path* direction_indicator = renderer.pushPrimitive<Path>(arrow_path);
          direction_indicator->m_is_closed = false; // V shaped
          direction_indicator->color = &m_app->getColor(ThemeColor::TextSelectedBg);
          direction_indicator->id = "gcode_arrows";
          direction_indicator->flags =
            PrimitiveFlags::GCode | PrimitiveFlags::GCodeArrow;
          direction_indicator->matrix_callback = m_view->getTransformCallback();
          direction_indicator->visible = false;
        }
        path.push_back({ simplified[i].x, simplified[i].y });
        point_count++;
        if (point_count > 2) {
          point_count = 0;
        }
      }
      Path* g = renderer.pushPrimitive<Path>(path);
      g->m_is_closed = false;
      g->id = "gcode";
      g->flags = PrimitiveFlags::GCode;
      g->user_data = rapid_line; // Store rapid_line index as type-safe int
      g->color = &m_app->getColor(ThemeColor::Text);
      g->matrix_callback = m_view->getTransformCallback();
      g->mouse_callback = [view = m_view](Primitive*                       c,
                                          const Primitive::MouseEventData& e) {
        view->getHmi().mouseCallback(c, e);
      };
      g->visible = false;
    }
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "Caught Exception: %s", e.what());
  }
}

bool GCode::parseTimer()
{
  if (!m_app)
    return false;
  auto& renderer = m_app->getRenderer();

  for (int x = 0; x < 1000; x++) {
    if (m_line_index < m_lines.size()) {
      const std::string& line = m_lines[m_line_index++];
      m_lines_consumed++;
      m_view->getDialogs().setProgressValue((float) m_lines_consumed /
                                            (float) m_line_count);
      if (line.find("G0") != std::string::npos) {
        Point2d last_path_endpoint = { std::numeric_limits<int>::min(),
                                       std::numeric_limits<int>::min() };
        if (m_current_path.points.size() > 0) {
          last_path_endpoint =
            m_current_path.points[m_current_path.points.size() - 1];
        }
        pushCurrentPathToViewer(m_last_rapid_line);
        if (m_current_path.points.size() > 0)
          m_paths.push_back(m_current_path);
        m_current_path.points.clear();
        nlohmann::json g = parseLine(line);
        try {
          m_current_path.points.push_back({ (double) g["x"], (double) g["y"] });
          if (last_path_endpoint.x != std::numeric_limits<int>::min() &&
              last_path_endpoint.y != std::numeric_limits<int>::min()) {
            Line* l = renderer.pushPrimitive<Line>(
              last_path_endpoint, Point2d{ (double) g["x"], (double) g["y"] });
            l->id = "gcode";
            l->flags = PrimitiveFlags::GCode;
            l->m_style = "dashed";
            l->color = &m_app->getColor(ThemeColor::TextDisabled);
            l->matrix_callback = m_view->getTransformCallback();
            l->visible = false;
          }
        }
        catch (...) {
          LOG_F(ERROR,
                "Gcode parsing error at line %lu in file %s",
                m_lines_consumed,
                m_filename.c_str());
        }
        m_last_rapid_line = m_lines_consumed - 1;
      }
      else if (line.find("G1") != std::string::npos) {
        nlohmann::json g = parseLine(line);
        try {
          m_current_path.points.push_back({ (double) g["x"], (double) g["y"] });
        }
        catch (...) {
          LOG_F(ERROR,
                "Gcode parsing error at line %lu in file %s",
                m_lines_consumed,
                m_filename.c_str());
        }
      }
    }
    else {
      LOG_F(INFO, "Reached end of G-code lines!");
      pushCurrentPathToViewer(m_last_rapid_line);
      if (m_current_path.points.size() > 0)
        m_paths.push_back(m_current_path);
      m_current_path.points.clear();
      m_view->getDialogs().setProgressValue(1.0f);
      m_view->getDialogs().showProgressWindow(false);
      auto& stack = renderer.getPrimitiveStack();
      for (size_t x = 0; x < stack.size(); x++) {
        if ((stack.at(x)->flags & PrimitiveFlags::GCode) !=
            PrimitiveFlags::None) {
          stack.at(x)->visible = true;
        }
      }
      return false;
    }
  }
  return true;
}
