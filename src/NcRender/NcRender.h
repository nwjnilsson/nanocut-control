#ifndef EASYRENDER_
#define EASYRENDER_

#include "Input/InputEvents.h"
#include "geometry/geometry.h"
#include "primitives/Primitives.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <deque>
#include <functional>
#include <imgui.h>
#include <memory>
#include <nlohmann/json.hpp>
#include <string>
#include <sys/time.h>
#include <variant>

#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

#include <GL/glu.h>

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
  !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#  pragma comment(lib, "legacy_stdio_definitions")
#endif

class NcApp;

class NcRender {
private:
  struct NcRenderTimer {
    std::string           view;
    unsigned long         timestamp;
    unsigned long         interval;
    std::function<bool()> callback;
  };
  NcApp*             m_app;
  GLFWwindow*        m_window;
  unsigned long      m_render_performance;
  float              m_clear_color[3];
  std::string        m_window_title;
  int                m_window_size[2];
  bool               m_show_cursor;
  bool               m_auto_maximize;
  bool               m_show_fps;
  Text*              m_fps_label;
  std::vector<float> m_fps_average;
  char*              m_gui_ini_filename;
  char*              m_gui_log_filename;
  std::string        m_main_log_filename;
  std::string        m_gui_style;
  std::string        m_current_view;

  std::deque<std::unique_ptr<Primitive>> m_primitive_stack;
  std::vector<NcRenderTimer>             m_timer_stack;

  static void
  keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void
  mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
  static void
  scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
  static void
  cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
  static void windowSizeCallback(GLFWwindow* window, int width, int height);

public:
  // Type alias for MouseEventType (defined in InputEvents.h to avoid circular
  // dependency)
  using EventType = MouseEventType;

  struct NcRenderGui {
    std::string           view;
    bool                  visible;
    std::function<void()> callback;
  };
  std::deque<NcRenderGui> m_gui_stack;

  // Constructor accepting existing GLFW window (NcApp owns window)
  explicit NcRender(NcApp* app);

  /* Primitive Creation */
  template <typename T, typename... Ts> T* pushPrimitive(Ts&&... args)
  {
    auto prim = std::make_unique<T>(std::forward<Ts>(args)...);
    T*   raw_ptr = prim.get();
    raw_ptr->view = m_current_view;
    m_primitive_stack.push_back(std::move(prim));
    return raw_ptr;
  }

  /* Timer Creation */
  void pushTimer(unsigned long interval, std::function<bool()> c);

  /* Gui Creation */
  NcRenderGui* pushGui(bool visible, std::function<void()> callback);

  /* Setters */
  void setWindowTitle(std::string w);
  void setWindowSize(int width, int height);
  void setShowCursor(bool s);
  void setAutoMaximize(bool m);
  void setGuiIniFileName(std::string i);
  void setGuiLogFileName(std::string l);
  void setMainLogFileName(std::string l);
  void setGuiStyle(std::string s);
  void setClearColor(float r, float g, float b);
  void setShowFPS(bool show_fps);
  void setCurrentView(std::string v);

  /* Time */
  static unsigned long millis();
  static void          delay(unsigned long ms);

  /* Getters */
  std::string getEnvironmentVariable(const std::string& var);
  std::string getConfigDirectory();
  Point2d     getWindowMousePosition();
  Point2d     getWindowSize();
  uint8_t     getFramesPerSecond();
  std::string getCurrentView() const;
  std::deque<std::unique_ptr<Primitive>>& getPrimitiveStack();

  /* Debugging */
  nlohmann::json dumpPrimitiveStack();

  /* ImGui Setup */
  void setupFonts();

  /* File I/O */
  nlohmann::json parseJsonFromFile(std::string filename);
  void           dumpJsonToFile(std::string filename, const nlohmann::json& j);
  std::string    fileToString(std::string filename);
  void           stringToFile(std::string filename, std::string s);

  /* Primitive Manipulators */
  void deletePrimitivesById(std::string id);

  /* Event Delegation (called by NcApp) */
  void handleKeyEvent(const KeyEvent& ev);
  void handleMouseButtonEvent(int button, int action, int mods);
  void handleScrollEvent(double xoffset, double yoffset);
  void handleCursorPositionEvent(double xpos, double ypos);
  void handleWindowSizeEvent(int width, int height);

  /* Main Operators */
  bool init(int argc, char** argv);
  bool poll(bool should_quit);
  void close();
};

#endif // EASYREANDER_
