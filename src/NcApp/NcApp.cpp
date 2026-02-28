#include "NcApp.h"
#include "../Input/InputEvents.h"
#include "../Input/InputState.h"
#include "../NcAdminView/NcAdminView.h"
#include "../NcCamView/NcCamView.h"
#include "../NcControlView/NcControlView.h"
#include "../NcRender/NcRender.h"
#include "../ThemeManager/ThemeColor.h"
#include "../ThemeManager/ThemeManager.h"
#include "../WebsocketClient/WebsocketClient.h"
#include <imgui.h>
#include <loguru.hpp>
#include <nlohmann/json.hpp>
#include <stb_image.h>
#include <stdexcept>
#include <sys/stat.h>

#include <pch/nanocut_128_png.h>
#include <pch/nanocut_256_png.h>
#include <pch/nanocut_64_png.h>

NcApp::NcApp()
  : m_start_timestamp(NcRender::millis()),
    m_input_state(std::make_unique<InputState>())
{
}

NcApp::~NcApp() { shutdown(); }

bool NcApp::initializeWindow(int argc, char** argv)
{
  // Initialize GLFW
  if (!glfwInit()) {
    return false;
  }

  // Create window (default size for now, will be updated from preferences)
  m_window = glfwCreateWindow(800, 600, "NanoCut", NULL, NULL);
  if (m_window == NULL) {
    glfwTerminate();
    return false;
  }

  // Set window icon (no-op on macOS where glfwSetWindowIcon is unsupported)
  setWindowIcon();

  // Set window user pointer to this NcApp instance for callbacks
  glfwSetWindowUserPointer(m_window, reinterpret_cast<void*>(this));

  // Register GLFW callbacks
  glfwSetKeyCallback(m_window, NcApp::keyCallback);
  glfwSetMouseButtonCallback(m_window, NcApp::mouseButtonCallback);
  glfwSetScrollCallback(m_window, NcApp::scrollCallback);
  glfwSetCursorPosCallback(m_window, NcApp::cursorPositionCallback);
  glfwSetWindowSizeCallback(m_window, NcApp::windowSizeCallback);

  return true;
}

void NcApp::initialize(int argc, char** argv)
{
  // Initialize GLFW window first
  if (!initializeWindow(argc, argv)) {
    throw std::runtime_error("Failed to initialize GLFW window");
  }

  // Create NcRender with existing window
  m_renderer = std::make_unique<NcRender>(this);
  m_renderer->setWindowTitle("NanoCut");
  m_renderer->setGuiIniFileName(m_renderer->getConfigDirectory() + "gui.ini");
  m_renderer->setGuiLogFileName(m_renderer->getConfigDirectory() + "gui.log");
  m_renderer->setMainLogFileName(m_renderer->getConfigDirectory() +
                                 "NanoCut.log");

  // Load window size preferences (theme system handles colors now)
  std::string pref_file = m_renderer->getConfigDirectory() + "preferences.json";
  nlohmann::json preferences = m_renderer->parseJsonFromFile(pref_file);
  if (preferences != NULL) {
    try {
      LOG_F(INFO, "Found %s!", std::string(pref_file).c_str());
      m_preferences.window_size[0] = preferences["window_width"].get<int>();
      m_preferences.window_size[1] = preferences["window_height"].get<int>();
    }
    catch (...) {
      LOG_F(WARNING, "Error parsing preferences file!");
    }
  }
  else {
    LOG_F(WARNING, "Preferences file does not exist, creating it!");
    m_renderer->dumpJsonToFile(
      pref_file,
      { { "window_width", m_preferences.window_size[0] },
        { "window_height", m_preferences.window_size[1] } });
  }

  m_renderer->setWindowSize(m_preferences.window_size[0],
                            m_preferences.window_size[1]);

  // Initialize renderer
  m_renderer->init(argc, argv);

  // Create theme manager (after renderer init, needs config directory)
  std::string config_dir = m_renderer->getConfigDirectory();
  std::string theme_dir = config_dir + "themes";
  m_theme_manager = std::make_unique<ThemeManager>(this, theme_dir, config_dir);
  m_theme_manager->applyTheme();

  // Set clear color from theme
  Color4f bg = m_theme_manager->getColor(ThemeColor::WindowBg);
  m_renderer->setClearColor(bg.r, bg.g, bg.b);

  // Create app-level dialogs (view-independent)
  m_dialogs = std::make_unique<NcDialogs>(this);

  // Create services and views with dependency injection
  m_control_view = std::make_unique<NcControlView>(this);
  m_cam_view = std::make_unique<NcCamView>(this);
  m_admin_view = std::make_unique<NcAdminView>(this);

  // Call PreInit methods with dependency injection
  m_control_view->preInit();
  m_cam_view->preInit();

  // Register centralized modifier key events
  registerModifierKeyEvents();

  // Initialize views with dependency injection
  m_cam_view->init();
  m_control_view->init();
  m_admin_view->init();

  // Initialize websocket with renderer dependency
  m_websocket = std::make_unique<WebsocketClient>(m_renderer.get());
  m_websocket->setIdAuto();
  m_control_view->makeActive();

  // Check for admin mode
  if (argc > 1 && std::string(argv[1]) == "--admin") {
    m_websocket->setId("Admin");
    m_admin_view->makeActive();
  }

  m_websocket->init();
}

void NcApp::shutdown()
{
  if (m_renderer) {
    logUptime();
    m_renderer->close();
  }

  if (m_control_view) {
    m_control_view->close();
  }

  if (m_admin_view) {
    m_admin_view->close();
  }

  if (m_websocket) {
    m_websocket->close();
  }

  // Smart pointers automatically handle cleanup
}

void NcApp::setWindowIcon()
{
#ifndef __APPLE__ // glfwSetWindowIcon is not supported on macOS
  struct IconData {
    const unsigned char* data;
    unsigned int         len;
  };

  const IconData icons[] = {
    { assets_nanocut_64_png, assets_nanocut_64_png_len },
    { assets_nanocut_128_png, assets_nanocut_128_png_len },
    { assets_nanocut_256_png, assets_nanocut_256_png_len },
  };

  GLFWimage images[3];
  int       loaded = 0;

  for (int i = 0; i < 3; i++) {
    int            w, h, channels;
    unsigned char* pixels =
      stbi_load_from_memory(icons[i].data, icons[i].len, &w, &h, &channels, 4);
    if (pixels) {
      images[loaded].width = w;
      images[loaded].height = h;
      images[loaded].pixels = pixels;
      loaded++;
    }
  }

  if (loaded > 0) {
    glfwSetWindowIcon(m_window, loaded, images);
  }

  for (int i = 0; i < loaded; i++) {
    stbi_image_free(images[i].pixels);
  }
#endif
}

void NcApp::logUptime()
{
  nlohmann::json uptime;
  unsigned long  m = (NcRender::millis() - m_start_timestamp);
  unsigned long  seconds = (m / 1000) % 60;
  unsigned long  minutes = (m / (1000 * 60)) % 60;
  unsigned long  hours = (m / (1000 * 60 * 60)) % 24;

  LOG_F(INFO,
        "Shutting down, Adding Uptime: %luh %lum %lus to total",
        hours,
        minutes,
        seconds);

  nlohmann::json uptime_json = m_renderer->parseJsonFromFile(
    m_renderer->getConfigDirectory() + "uptime.json");

  if (uptime_json != NULL) {
    try {
      uptime["total_milliseconds"] =
        (unsigned long) uptime_json["total_milliseconds"] + m;
    }
    catch (...) {
      LOG_F(WARNING, "Error parsing uptime file!");
    }
    m_renderer->dumpJsonToFile(m_renderer->getConfigDirectory() + "uptime.json",
                               uptime);
  }
  else {
    uptime["total_milliseconds"] = m;
    m_renderer->dumpJsonToFile(m_renderer->getConfigDirectory() + "uptime.json",
                               uptime);
  }
}

const Color4f& NcApp::getColor(ThemeColor color) const
{
  if (m_theme_manager) {
    return m_theme_manager->getColor(color);
  }
  return THEME_COLOR_DEFAULTS[static_cast<int>(color)];
}

void NcApp::setModifierPressed(int modifier_bit, bool pressed)
{
  if (pressed) {
    m_modifier_state |= modifier_bit;
  }
  else {
    m_modifier_state &= ~modifier_bit;
  }
}

void NcApp::modifierKeyCallback(const nlohmann::json& e)
{
  int  key = e.at("key").get<int>();
  int  action = e.at("action").get<int>();
  bool pressed = action == GLFW_PRESS;

  switch (key) {
    case GLFW_KEY_LEFT_CONTROL:
    case GLFW_KEY_RIGHT_CONTROL:
      setModifierPressed(GLFW_MOD_CONTROL, pressed);
      break;
    case GLFW_KEY_LEFT_SHIFT:
    case GLFW_KEY_RIGHT_SHIFT:
      setModifierPressed(GLFW_MOD_SHIFT, pressed);
      break;
    case GLFW_KEY_LEFT_ALT:
    case GLFW_KEY_RIGHT_ALT:
      setModifierPressed(GLFW_MOD_ALT, pressed);
      break;
    case GLFW_KEY_LEFT_SUPER:
    case GLFW_KEY_RIGHT_SUPER:
      setModifierPressed(GLFW_MOD_SUPER, pressed);
      break;
  }
}

void NcApp::registerModifierKeyEvents()
{
  // No longer needed - modifier keys are tracked directly in handleKeyEvent()
  // This function is now a no-op but kept for compatibility
}

// Event delegation helper implementations
bool NcApp::handleGlobalKeyEvent(int key, int action, int mods)
{
  // Handle global shortcuts here
  if (key == GLFW_KEY_Q && action == GLFW_PRESS && (mods & GLFW_MOD_CONTROL)) {
    // Ctrl+Q global quit shortcut
    requestQuit();
    return true;
  }

  // Add more global shortcuts here if needed
  return false; // No global shortcut handled
}

bool NcApp::handleGlobalMouseEvent(int button, int action, int mods)
{
  // Handle global mouse events here if needed
  // For now, no global mouse shortcuts
  return false;
}

void NcApp::delegateEventToActiveView()
{
  // This method can be used to set the active view based on context
  // For now, views are managed through their makeActive() methods
  // which call setCurrentActiveView() appropriately
}

// GLFW callback implementations
void NcApp::keyCallback(
  GLFWwindow* window, int key, int scancode, int action, int mods)
{
  NcApp* app = reinterpret_cast<NcApp*>(glfwGetWindowUserPointer(window));
  if (!app)
    return;

  // Update input state
  if (action == GLFW_PRESS) {
    app->m_input_state->setKeyPressed(key, true);
  }
  else if (action == GLFW_RELEASE) {
    app->m_input_state->setKeyPressed(key, false);
  }

  // Legacy modifier state tracking (will be removed later)
  if (key == GLFW_KEY_LEFT_CONTROL || key == GLFW_KEY_RIGHT_CONTROL) {
    app->setModifierPressed(GLFW_MOD_CONTROL, action != GLFW_RELEASE);
  }
  if (key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT) {
    app->setModifierPressed(GLFW_MOD_SHIFT, action != GLFW_RELEASE);
  }
  if (key == GLFW_KEY_LEFT_ALT || key == GLFW_KEY_RIGHT_ALT) {
    app->setModifierPressed(GLFW_MOD_ALT, action != GLFW_RELEASE);
  }
  if (key == GLFW_KEY_LEFT_SUPER || key == GLFW_KEY_RIGHT_SUPER) {
    app->setModifierPressed(GLFW_MOD_SUPER, action != GLFW_RELEASE);
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureKeyboard and io.WantCaptureMouse) {
    // Let ImGui handle the rest of this event
    // Maybe not ideal to require both but otherwise control->CAM via TAB
    // did not work. Opening the settings editor is the most sketchy part
    // about this since un-focusing the parameters and hitting arrow keys
    // can jog the machine. FIXME: full-screen the parameters or disable
    // motion when UI elements are showing
    return;
  }

  // Check for global shortcuts first
  if (app->handleGlobalKeyEvent(key, action, mods)) {
    return; // Global shortcut handled, don't propagate further
  }

  // Delegate to current active view if available
  KeyEvent event(key, scancode, action, mods);
  if (app->m_current_active_view) {
    app->m_current_active_view->handleKeyEvent(event, *app->m_input_state);
  }

  // Also delegate to NcRender for legacy compatibility and GUI events
  if (app->m_renderer) {
    app->m_renderer->handleKeyEvent(event);
  }
}

void NcApp::mouseButtonCallback(GLFWwindow* window,
                                int         button,
                                int         action,
                                int         mods)
{
  NcApp* app = reinterpret_cast<NcApp*>(glfwGetWindowUserPointer(window));
  if (!app)
    return;

  // Update input state
  if (action == GLFW_PRESS) {
    app->m_input_state->setMouseButtonPressed(button, true);
  }
  else if (action == GLFW_RELEASE) {
    app->m_input_state->setMouseButtonPressed(button, false);
  }

  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    // Let ImGui handle the rest of this event
    return;
  }

  // Check for global mouse shortcuts first
  if (app->handleGlobalMouseEvent(button, action, mods)) {
    return;
  }

  // Delegate to current active view if available
  if (app->m_current_active_view) {
    MouseButtonEvent event(button, action, mods);
    app->m_current_active_view->handleMouseEvent(event, *app->m_input_state);
  }

  // Also delegate to NcRender for legacy compatibility and GUI events
  if (app->m_renderer) {
    app->m_renderer->handleMouseButtonEvent(button, action, mods);
  }
}

void NcApp::scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
  NcApp* app = reinterpret_cast<NcApp*>(glfwGetWindowUserPointer(window));
  if (!app)
    return;

  ImGuiIO& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
    // Let ImGui handle the rest of this event
    return;
  }

  // Delegate to current active view if available
  if (app->m_current_active_view) {
    ScrollEvent event(xoffset, yoffset);
    app->m_current_active_view->handleScrollEvent(event, *app->m_input_state);
  }

  // Also delegate to NcRender for legacy compatibility and GUI events
  if (app->m_renderer) {
    app->m_renderer->handleScrollEvent(xoffset, yoffset);
  }
}

void NcApp::cursorPositionCallback(GLFWwindow* window, double xpos, double ypos)
{
  NcApp* app = reinterpret_cast<NcApp*>(glfwGetWindowUserPointer(window));
  if (!app)
    return;

  // Get previous position from input state for delta calculation
  const auto& prev_pos = app->m_input_state->getMousePosition();

  // Apply the same coordinate transformation as
  // NcRender::GetWindowMousePosition() This ensures consistency with the view
  // coordinate system
  double mouseX = xpos;
  double mouseY = ypos;
  mouseX -= 2; // Window decoration offset
  mouseY -= 4; // Window decoration offset

  // Get window size for center-based coordinates
  auto   window_size = app->m_renderer->getWindowSize();
  double transformed_x = mouseX - (window_size.x / 2.0f);
  double transformed_y = (window_size.y - mouseY) - (window_size.y / 2.0f);

  // Update input state with transformed position (also stores previous)
  app->m_input_state->setMousePosition(transformed_x, transformed_y);

  // Delegate to current active view if available
  if (app->m_current_active_view) {
    MouseMoveEvent event(transformed_x, transformed_y, prev_pos.x, prev_pos.y);
    app->m_current_active_view->handleMouseMoveEvent(event,
                                                     *app->m_input_state);
  }

  // Also delegate to NcRender for legacy compatibility and GUI events
  if (app->m_renderer) {
    app->m_renderer->handleCursorPositionEvent(
      xpos, ypos); // NcRender still expects raw coordinates
  }
}

void NcApp::windowSizeCallback(GLFWwindow* window, int width, int height)
{
  NcApp* app = reinterpret_cast<NcApp*>(glfwGetWindowUserPointer(window));
  if (!app)
    return;

  if (app->m_renderer) {
    app->m_renderer->handleWindowSizeEvent(width, height);
  }

  if (app->m_current_active_view) {
    WindowResizeEvent event(width, height);
    app->m_current_active_view->handleWindowResize(event, *app->m_input_state);
  }
}
