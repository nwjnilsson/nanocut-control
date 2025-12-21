#ifndef EASYRENDER_
#define EASYRENDER_

#include "geometry/geometry.h"
#include "gui/imgui.h"
#include "primitives/Primitives.h"
#include "json/json.h"
#include <algorithm>
#include <chrono>
#include <ctime>
#include <functional>
#include <memory>
#include <string>
#include <sys/time.h>
#include <variant>
#include <vector>

#ifdef __APPLE__
#  define GL_SILENCE_DEPRECATION
#endif
#include <GLFW/glfw3.h>

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

#if defined(_MSC_VER) && (_MSC_VER >= 1900) &&                                 \
  !defined(IMGUI_DISABLE_WIN32_FUNCTIONS)
#  pragma comment(lib, "legacy_stdio_definitions")
#endif

class NcRender {
private:
  struct NcRenderTimer {
    std::string           view;
    unsigned long         timestamp;
    unsigned long         interval;
    std::function<bool()> callback;
  };
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

  std::vector<std::unique_ptr<Primitive>> m_primitive_stack;
  std::vector<NcRenderTimer>            m_timer_stack;

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
  enum class ActionFlagBits {
    Release = 1 << GLFW_RELEASE,
    Press = 1 << GLFW_PRESS,
    Repeat = 1 << GLFW_REPEAT,
    Any = Release | Press | Repeat
  };
  typedef uint32_t ActionFlags;

  // Primitive-only events (for mouse hover callbacks)
  enum class EventType {
    MouseIn,
    MouseOut,
    MouseMove,
  };

  ImGuiIO* imgui_io;
  struct NcRenderGui {
    std::string           view;
    bool                  visible;
    std::function<void()> callback;
  };
  std::vector<NcRenderGui> m_gui_stack;

  // Constructor accepting existing GLFW window (NcApp owns window)
  explicit NcRender(GLFWwindow* window)
  {
    m_window = window;
    // Load Defaults
    setWindowTitle("NcRender");
    setWindowSize(800, 600);
    setShowCursor(true);
    setAutoMaximize(false);
    setGuiIniFileName("NcRenderGui.ini");
    setGuiLogFileName("NcRenderGui.log");
    setMainLogFileName("NcRender.log");
    setGuiStyle("light");
    setClearColor(21, 22, 34);
    setShowFPS(false);
    setCurrentView("main");
    m_fps_label = NULL;
    imgui_io = NULL;
  };
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
  void setColorByName(float* c, std::string color);
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
  std::vector<std::unique_ptr<Primitive>>& getPrimitiveStack();

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
  void handleKeyEvent(int key, int scancode, int action, int mods);
  void handleMouseButtonEvent(int button, int action, int mods);
  void handleScrollEvent(double xoffset, double yoffset);
  void handleCursorPositionEvent(double xpos, double ypos);
  void handleWindowSizeEvent(int width, int height);

  /* Main Operators */
  bool init(int argc, char** argv);
  bool poll(bool should_quit);
  void close();
};

NcRender::ActionFlags operator&(NcRender::ActionFlags    a,
                                  NcRender::ActionFlagBits b);
NcRender::ActionFlags operator|(NcRender::ActionFlagBits a,
                                  NcRender::ActionFlagBits b);
NcRender::ActionFlags operator+(NcRender::ActionFlagBits a);

#endif // EASYREANDER_
