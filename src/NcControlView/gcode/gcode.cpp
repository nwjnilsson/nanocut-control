#include "gcode.h"
#include "../../application/NcApp.h"
#include "../hmi/hmi.h"
#include <NcControlView/NcControlView.h>
#include <NcRender/geometry/geometry.h>
#include <NcRender/logging/loguru.h>

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

unsigned long GCode::countLines(const std::string& filepath)
{
  std::string   line;
  unsigned long count = 0;
  std::ifstream in(filepath);
  while (std::getline(in, line)) {
    count++;
  }
  return count;
}

bool GCode::openFile(const std::string& filepath)
{
  if (!m_app)
    return false;
  auto& renderer = m_app->getRenderer();

  m_file.open(filepath);
  if (m_file.is_open()) {
    renderer.deletePrimitivesById("gcode");
    renderer.deletePrimitivesById("gcode_arrows");
    renderer.deletePrimitivesById("gcode_highlights");
    m_line_count = countLines(filepath);
    m_lines_consumed = 0;
    m_filename = filepath;
    LOG_F(
      INFO, "Opened file: %s, size: %lu lines", filepath.c_str(), m_line_count);
    m_view->getDialogs().showProgressWindow(true);
    m_view->getDialogs().setProgressValue(0.0f);
    return true;
  }
  else {
    LOG_F(ERROR, "Could not open file: %s", filepath.c_str());
    return false;
  }
}

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
  ret["x"] = atof(x_value.c_str());
  ret["y"] = atof(y_value.c_str());
  return ret;
}

void GCode::pushCurrentPathToViewer(int rapid_line)
{
  if (!m_app || m_current_path.points.size() == 0)
    return;
  auto& renderer = m_app->getRenderer();

  Geometry geo;
  try {
    if (m_current_path.points.size() > 1) {
      std::vector<Point2d> simplified =
        geo.simplify(m_current_path.points, SCALE(0.25));
      std::vector<Point2d> path;
      int                  point_count = 0;
      for (int i = 0; i < simplified.size(); i++) {
        if ((i == 0 && i + 1 < simplified.size()) ||
            (point_count == 0 && i + 1 < simplified.size())) {
          std::vector<Point2d> arrow_path;
          Point2d midpoint = geo.midpoint(simplified[i + 1], simplified[i]);
          arrow_path.push_back({ midpoint.x, midpoint.y });
          double angle =
            geo.measurePolarAngle(simplified[i + 1], simplified[i]);
          Point2d p1 =
            geo.createPolarLine(midpoint, angle + 30, SCALE(0.5)).end;
          Point2d p2 =
            geo.createPolarLine(midpoint, angle - 30, SCALE(0.5)).end;
          arrow_path.push_back({ p1.x, p1.y });
          arrow_path.push_back({ p2.x, p2.y });
          Path* direction_indicator = renderer.pushPrimitive<Path>(arrow_path);
          renderer.setColorByName(direction_indicator->color, "blue");
          direction_indicator->id = "gcode_arrows";
          direction_indicator->matrix_callback = m_view->m_view_matrix;
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
      g->data = { { "rapid_line", rapid_line } };
      renderer.setColorByName(g->color, "white");
      g->matrix_callback = m_view->m_view_matrix;
      g->mouse_callback = [view = m_view](Primitive*            c,
                                          const nlohmann::json& e) {
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

  std::string line;
  for (int x = 0; x < 1000; x++) {
    if (std::getline(m_file, line)) {
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
            l->m_style = "dashed";
            renderer.setColorByName(l->color, "grey");
            l->matrix_callback = m_view->m_view_matrix;
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
      LOG_F(INFO, "Reached end of file!");
      pushCurrentPathToViewer(m_last_rapid_line);
      if (m_current_path.points.size() > 0)
        m_paths.push_back(m_current_path);
      m_current_path.points.clear();
      m_file.close();
      m_view->getDialogs().setProgressValue(1.0f);
      m_view->getDialogs().showProgressWindow(false);
      auto& stack = renderer.getPrimitiveStack();
      for (size_t x = 0; x < stack.size(); x++) {
        if (stack.at(x)->id == "gcode" || stack.at(x)->id == "gcode_arrows" ||
            stack.at(x)->id == "gcode_highlights") {
          stack.at(x)->visible = true;
        }
      }
      return false;
    }
  }
  // LOG_F(INFO, "Progress: %.4f", (float)(((float)m_lines_consumed /
  // (float)m_line_count) * 100.0f));
  return true;
}
