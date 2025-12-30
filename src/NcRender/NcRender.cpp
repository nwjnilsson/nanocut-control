#include <algorithm>
#include <chrono>
#include <ctime>
#include <fstream>
#include <ftw.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <string>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <vector>

#include "NcRender.h"

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl2.h>
#include <loguru.hpp>

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

const auto NcRenderProgramStartTime = std::chrono::steady_clock::now();

// Event delegation methods (called by NcApp)
void NcRender::handleKeyEvent(const KeyEvent& ev)
{
  // Only check if ImGui wants to capture input
  // NcApp now handles all key events directly through view delegation
}

void NcRender::handleMouseButtonEvent(int button, int action, int mods)
{
  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard || io.WantCaptureMouse) {
    return; // ImGui is handling input
  }

  // Handle primitive mouse events
  if (!io.WantCaptureKeyboard && !io.WantCaptureMouse) {
    bool ignore_next_mouse_events = false;
    for (auto it = m_primitive_stack.rbegin(); it != m_primitive_stack.rend();
         ++it) {
      if ((*it)->view == m_current_view) {
        if ((*it)->visible == true && (*it)->mouse_over == true) {
          if ((*it)->mouse_callback != NULL) {
            MouseButtonEvent btn_event(button, action, mods);
            (*it)->mouse_callback(it->get(), btn_event);
            ignore_next_mouse_events = true;
          }
        }
      }
      if (ignore_next_mouse_events) {
        break;
      }
    }
  }
}

void NcRender::handleScrollEvent(double xoffset, double yoffset)
{
  // Only check if ImGui wants to capture input
  // NcApp now handles all scroll events directly through view delegation
}

void NcRender::handleCursorPositionEvent(double xpos, double ypos)
{
  // Only check if ImGui wants to capture input
  // NcApp now handles all cursor position events directly through view
  // delegation
}

void NcRender::handleWindowSizeEvent(int width, int height)
{
  // Update window size immediately when resize event occurs
  // This ensures getWindowSize() returns current values for UI layout
  // calculations
  setWindowSize(width, height);

  // NcApp now handles all window resize events directly through view delegation
}

/*
    GLFW Key Callbacks (deprecated - now handled by NcApp delegation)
*/
// Removed old key_callback - no longer needed
// Removed old mouse_button_callback - no longer needed
// Primitive mouse handling is now done in handleMouseButtonEvent()
// Removed old scroll_callback - no longer needed
// Removed old cursor_position_callback - no longer needed
// Removed old window_size_callback - no longer needed
/*
    Each Primitive type must have a method
*/

void NcRender::pushTimer(unsigned long interval, std::function<bool()> c)
{
  m_timer_stack.emplace_back(m_current_view, millis(), interval, c);
}
NcRender::NcRenderGui* NcRender::pushGui(bool                  visible,
                                         std::function<void()> callback)
{
  m_gui_stack.push_back({ m_current_view, visible, callback });
  return &m_gui_stack.back();
}

// Removed PushEvent method - no longer needed with new delegation system

void NcRender::setWindowTitle(std::string w) { m_window_title = w; }

void NcRender::setWindowSize(int width, int height)
{
  m_window_size[0] = width;
  m_window_size[1] = height;
}

void NcRender::setShowCursor(bool s) { m_show_cursor = s; }

void NcRender::setAutoMaximize(bool m) { m_auto_maximize = m; }

void NcRender::setGuiIniFileName(std::string i)
{
  m_gui_ini_filename = (char*) malloc(sizeof(char) * (i.size() + 1));
  strcpy(m_gui_ini_filename, i.c_str());
}
void NcRender::setGuiLogFileName(std::string l)
{
  m_gui_log_filename = (char*) malloc(sizeof(char) * (l.size() + 1));
  strcpy(m_gui_log_filename, l.c_str());
}

void NcRender::setMainLogFileName(std::string l) { m_main_log_filename = l; }

void NcRender::setGuiStyle(std::string s) { m_gui_style = s; }

void NcRender::setClearColor(float r, float g, float b)
{
  m_clear_color[0] = r / 255;
  m_clear_color[1] = g / 255;
  m_clear_color[2] = b / 255;
  // LOG_F(INFO, "set Clear Color (%.4f, %.4f, %.4f)", m_clear_color[0],
  // m_clear_color[1], m_clear_color[2]);
}

void NcRender::setShowFPS(bool show_fps) { m_show_fps = show_fps; }

void NcRender::setCurrentView(std::string v) { m_current_view = v; }

unsigned long NcRender::millis()
{
  using std::chrono::duration_cast;
  using std::chrono::milliseconds;
  using std::chrono::seconds;
  using std::chrono::system_clock;
  auto          end = std::chrono::steady_clock::now();
  unsigned long m =
    (unsigned long) std::chrono::duration_cast<std::chrono::milliseconds>(
      end - NcRenderProgramStartTime)
      .count();
  return m;
}

void NcRender::delay(unsigned long ms)
{
  unsigned long delay_timer = NcRender::millis();
  while ((NcRender::millis() - delay_timer) < ms)
    ;
  return;
}

std::string NcRender::getEnvironmentVariable(const std::string& var)
{
  const char* val = std::getenv(var.c_str());
  if (val == nullptr) {
    return "";
  }
  else {
    return val;
  }
}

std::string NcRender::getConfigDirectory()
{
  struct stat info;
  std::string path = CONFIG_DIRECTORY;

  // Expand environment variables in the path
#ifdef _WIN32
  path = getEnvironmentVariable("APPDATA") + "/NanoCut/";
#elif __APPLE__
  path = getEnvironmentVariable("HOME") + "/Library/NanoCut/";
#else // Linux and other Unix-like systems
  path = getEnvironmentVariable("HOME") + "/.config/NanoCut/";
#endif

  if (stat(path.c_str(), &info) != 0) {
    LOG_F(INFO,
          "(NcRender::getConfigDirectory) %s does not exist, creating it!",
          path.c_str());
    mkdir(path.c_str(), (mode_t) 0733);
  }
  return path;
}

Point2d NcRender::getWindowMousePosition()
{
  double mouseX, mouseY;
  glfwGetCursorPos(m_window, &mouseX, &mouseY);
  mouseX -= 2;
  mouseY -= 4;
  return { mouseX - (m_window_size[0] / 2.0f),
           (m_window_size[1] - mouseY) - (m_window_size[1] / 2.0f) };
}

Point2d NcRender::getWindowSize()
{
  return { (double) m_window_size[0], (double) m_window_size[1] };
}

uint8_t NcRender::getFramesPerSecond()
{
  return (uint8_t) (1000.0f / (float) m_render_performance);
}

std::string NcRender::getCurrentView() const { return m_current_view; }

std::deque<std::unique_ptr<Primitive>>& NcRender::getPrimitiveStack()
{
  return m_primitive_stack;
}

nlohmann::json NcRender::dumpPrimitiveStack()
{
  nlohmann::json r;
  for (const auto& prim : m_primitive_stack) {
    r.push_back(prim->serialize());
  }
  return r;
}

nlohmann::json NcRender::parseJsonFromFile(std::string filename)
{
  std::ifstream json_file(filename);
  if (json_file.is_open()) {
    std::string json_string((std::istreambuf_iterator<char>(json_file)),
                            std::istreambuf_iterator<char>());
    return nlohmann::json::parse(json_string.c_str());
  }
  else {
    return NULL;
  }
}

void NcRender::dumpJsonToFile(std::string filename, const nlohmann::json& j)
{
  try {
    std::ofstream out(filename);
    out << j.dump(4);
    out.close();
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "(NcRender::dumpJsonToFile) %s", e.what());
  }
}

std::string NcRender::fileToString(std::string filename)
{
  std::ifstream f(filename);
  if (f.is_open()) {
    std::string s((std::istreambuf_iterator<char>(f)),
                  std::istreambuf_iterator<char>());
    return s;
  }
  else {
    return "";
  }
}

void NcRender::stringToFile(std::string filename, std::string s)
{
  try {
    std::ofstream out(filename);
    out << s;
    out.close();
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "(NcRender::stringToFile) %s", e.what());
  }
}

void NcRender::deletePrimitivesById(std::string id)
{
  auto it = m_primitive_stack.begin();
  while (it != m_primitive_stack.end()) {
    if ((*it)->id == id) {
      it = m_primitive_stack.erase(it);
    }
    else {
      ++it;
    }
  }
}

bool NcRender::init(int argc, char** argv)
{
  loguru::init(argc, argv);
#ifdef DEBUG
  loguru::g_stderr_verbosity = 1;
#else
  loguru::g_stderr_verbosity = 0;
#endif
  loguru::add_file(
    m_main_log_filename.c_str(), loguru::Append, loguru::Verbosity_MAX);

  // Window is always provided by NcApp - just set OpenGL context
  if (m_window == nullptr) {
    LOG_F(ERROR, "No GLFW window provided to NcRender!");
    return false;
  }

  glfwMakeContextCurrent(m_window);
  glfwSwapInterval(1); // Enable vsync
  // Set calbacks here
  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = m_gui_ini_filename;
  io.LogFilename = m_gui_log_filename;
  io.ConfigFlags |=
    ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
  if (m_gui_style == "light") {
    ImGui::StyleColorsLight();
  }
  else if (m_gui_style == "dark") {
    ImGui::StyleColorsDark();
  }
  ImGui_ImplGlfw_InitForOpenGL(m_window, true);
  ImGui_ImplOpenGL2_Init();

  // Setup fonts
  setupFonts();

  return true;
}

bool NcRender::poll(bool should_quit)
{
  ImGuiIO&      io = ImGui::GetIO();
  unsigned long begin_timestamp = millis();
  float         ratio = m_window_size[0] / (float) m_window_size[1];
  Point2d       window_mouse_pos = getWindowMousePosition();
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDepthRange(0, 1);
  glDepthFunc(GL_LEQUAL);
  glClearColor(m_clear_color[0], m_clear_color[1], m_clear_color[2], 255);
  /* IMGUI */
  ImGui_ImplOpenGL2_NewFrame();
  ImGui_ImplGlfw_NewFrame();
  ImGui::NewFrame();
  for (size_t x = 0; x < m_gui_stack.size(); x++) {
    if (m_gui_stack[x].visible and m_gui_stack[x].view == m_current_view) {
      if (m_gui_stack[x].callback) {
        m_gui_stack[x].callback();
      }
    }
  }
  ImGui::Render();
  /*********/
  glMatrixMode(GL_PROJECTION);
  glLoadIdentity();
  glOrtho(-(GLdouble) m_window_size[0] / 2,
          (GLdouble) m_window_size[0] / 2,
          -(GLdouble) m_window_size[1] / 2,
          (GLdouble) m_window_size[1] / 2,
          -1,
          1);
  glMatrixMode(GL_MODELVIEW);
  glLoadIdentity();
  glViewport(0, 0, m_window_size[0], m_window_size[1]);
  glClear(GL_COLOR_BUFFER_BIT);
  std::sort(
    m_primitive_stack.begin(),
    m_primitive_stack.end(),
    [](const auto& lhs, const auto& rhs) { return lhs->zindex < rhs->zindex; });
  bool ignore_next_mouse_events = false;
  for (size_t x = 0; x < m_primitive_stack.size(); x++) {
    if (m_primitive_stack[x]->visible == true &&
        m_primitive_stack[x]->view == m_current_view) {
      // Call matrix_callback to apply transforms before rendering
      if (m_primitive_stack[x]->matrix_callback) {
        m_primitive_stack[x]->matrix_callback(m_primitive_stack[x].get());
      }
      m_primitive_stack[x]->render();
      if ((!io.WantCaptureKeyboard || !io.WantCaptureMouse) &&
          ignore_next_mouse_events == false) {
        m_primitive_stack[x]->processMouse(window_mouse_pos.x,
                                           window_mouse_pos.y);
        // Dispatch mouse event if one was generated
        if (m_primitive_stack[x]->m_mouse_event.has_value() &&
            m_primitive_stack[x]->mouse_callback) {
          m_primitive_stack[x]->mouse_callback(
            m_primitive_stack[x].get(),
            m_primitive_stack[x]->m_mouse_event.value());
          m_primitive_stack[x]->m_mouse_event.reset(); // Clear event after
                                                       // dispatch
        }
      }
    }
  }
  ImGui_ImplOpenGL2_RenderDrawData(ImGui::GetDrawData());
  glfwMakeContextCurrent(m_window);
  glfwSwapBuffers(m_window);
  glfwPollEvents();
  for (size_t x = 0; x < m_timer_stack.size(); x++) {
    if (m_timer_stack.at(x).view == m_current_view) {
      if ((millis() - m_timer_stack.at(x).timestamp) >
          m_timer_stack.at(x).interval) {
        m_timer_stack[x].timestamp = millis();
        if (m_timer_stack[x].callback && m_timer_stack[x].callback() == false) {
          // Don't repeat - remove timer
          m_timer_stack.erase(m_timer_stack.begin() + x);
        }
      }
    }
  }
  m_render_performance = (millis() - begin_timestamp);
  if (m_show_fps == true) {
    if (m_fps_label == NULL) {
      LOG_F(INFO, "Creating FPS label!");
      m_fps_label = pushPrimitive<Text>(Point2d{ 0, 0 }, "0", 30.f);
      m_fps_label->visible = false;
      m_fps_label->id = "FPS";
      m_fps_label->flags = PrimitiveFlags::SystemUI;
      m_fps_label->color = getColor(Color::White);
    }
    else {
      m_fps_average.push_back(getFramesPerSecond());
      if (m_fps_average.size() > 10) {
        float avg = 0;
        for (size_t x = 0; x < m_fps_average.size(); x++) {
          avg += m_fps_average[x];
        }
        avg = avg / m_fps_average.size();
        m_fps_label->m_textval = std::to_string((int) avg);
        m_fps_label->visible = true;
        m_fps_label->m_position.x = -(m_window_size[0] / 2.0f) + 10;
        m_fps_label->m_position.y = -(m_window_size[1] / 2.0f) + 10;
        m_fps_average.erase(m_fps_average.begin());
      }
    }
  }
  if (should_quit == true)
    return false;
  return !glfwWindowShouldClose(m_window);
}

void NcRender::close()
{
  ImGui_ImplOpenGL2_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(m_window);
  glfwTerminate();
  free(m_gui_ini_filename);
  free(m_gui_log_filename);
}

void NcRender::setupFonts()
{
  std::string config_font_path =
    getConfigDirectory() + "fonts/Cabin/Cabin-SemiBold.ttf";
  struct stat font_stat;

  if (stat(config_font_path.c_str(), &font_stat) == 0) {
    // Font exists in config directory, use it
    ImFontConfig cfg;
    cfg.SizePixels = 20.f;
    ImFont* cabin_font = ImGui::GetIO().Fonts->AddFontFromFileTTF(
      config_font_path.c_str(), 20.0f, &cfg);
    if (cabin_font) {
      ImGui::GetIO().FontDefault = cabin_font;
    }
    else {
      // Fallback to default font
      ImGui::GetIO().Fonts->AddFontDefault(&cfg);
    }
  }
  else {
    // Font not in config directory, use default
    ImFontConfig cfg;
    cfg.SizePixels = 20.f;
    ImGui::GetIO().Fonts->AddFontDefault(&cfg);
  }
  ImGui::GetIO().Fonts->Build();
}
