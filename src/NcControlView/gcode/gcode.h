#ifndef GCODE__
#define GCODE__

#include <nlohmann/json.hpp>
#include <NanoCut.h>
#include <fstream>
#include <string>
#include <vector>

// Forward declarations
class NcApp;
class NcControlView;

/**
 * GCode - G-code file parser and renderer
 * Handles loading, parsing, and visualizing G-code toolpaths
 * for the CNC plasma cutting machine.
 */
class GCode {
public:
  // Path data structure
  struct GPath {
    std::vector<Point2d> points;
  };

  // Constructor/Destructor
  GCode(NcApp* app, NcControlView* view);
  ~GCode() = default;

  // Non-copyable, non-movable
  GCode(const GCode&) = delete;
  GCode& operator=(const GCode&) = delete;
  GCode(GCode&&) = delete;
  GCode& operator=(GCode&&) = delete;

  // Public API
  bool        openFile(const std::string& filepath);
  std::string getFilename() const;
  bool        parseTimer();

private:
  // Application context
  NcApp*         m_app;
  NcControlView* m_view;

  // G-code file state
  std::ifstream m_file;
  std::string   m_filename;
  unsigned long m_line_count = 0;
  unsigned long m_lines_consumed = 0;
  unsigned long m_last_rapid_line = 0;

  // Path data
  GPath              m_current_path;
  std::vector<GPath> m_paths;

  // Private helper methods
  nlohmann::json parseLine(const std::string& line);
  void           pushCurrentPathToViewer(int rapid_line);
  unsigned long  countLines(const std::string& filepath);
};

#endif // GCODE__
