#ifndef NC_APP_H
#define NC_APP_H

#include <GLFW/glfw3.h>
#include <NcRender/geometry/geometry.h>
#include <ThemeManager/ThemeColor.h>
#include <memory>

// Forward declarations
class NcRender;
class WebsocketClient;
class NcControlView;
class NcCamView;
class NcAdminView;
class ThemeManager;
class View;
class InputState;

/**
 * NcApp manages the lifetime and dependencies of all major
 * application subsystems, and owns the primary GLFW window.
 *
 * This class follows RAII principles and provides dependency injection
 * for views and services. It handles global GLFW events and delegates
 * to the appropriate subsystems.
 */
class NcApp {
public:
  // Constructor/Destructor
  NcApp();
  ~NcApp();

  // Non-copyable, movable
  NcApp(const NcApp&) = delete;
  NcApp& operator=(const NcApp&) = delete;
  NcApp(NcApp&&) = default;
  NcApp& operator=(NcApp&&) = default;

  // Initialization
  void initialize(int argc, char** argv);
  bool initializeWindow(int argc, char** argv);
  void shutdown();

  // Core application state
  bool shouldQuit() const { return m_quit; }
  void requestQuit() { m_quit = true; }

  // Input state access (replaces scattered mouse position tracking)
  InputState&       getInputState() { return *m_input_state; }
  const InputState& getInputState() const { return *m_input_state; }

  // View management for event delegation
  View* getCurrentActiveView() const { return m_current_active_view; }
  void  setCurrentActiveView(View* view) { m_current_active_view = view; }

  // Modifier key state management
  int  getModifierState() const { return m_modifier_state; }
  bool isModifierPressed(int modifier_bit) const
  {
    return (m_modifier_state & modifier_bit) != 0;
  }
  void setModifierPressed(int modifier_bit, bool pressed);

  unsigned long getStartTimestamp() const { return m_start_timestamp; }

  // Service accessors (dependency injection)
  NcRender&        getRenderer() { return *m_renderer; }
  WebsocketClient& getWebsocketClient() { return *m_websocket; }
  NcControlView&   getControlView() { return *m_control_view; }
  NcCamView&       getCamView() { return *m_cam_view; }
  NcAdminView&     getAdminView() { return *m_admin_view; }

  // Const accessors for read-only access
  const NcRender&        getRenderer() const { return *m_renderer; }
  const WebsocketClient& getWebsocketClient() const { return *m_websocket; }
  const NcControlView&   getControlView() const { return *m_control_view; }
  const NcCamView&       getCamView() const { return *m_cam_view; }
  const NcAdminView&     getAdminView() const { return *m_admin_view; }

  // Theme management
  ThemeManager&       getThemeManager() { return *m_theme_manager; }
  const ThemeManager& getThemeManager() const { return *m_theme_manager; }
  const Color4f&      getColor(ThemeColor color) const;

  GLFWwindow* getGlfwWindow() const { return m_window; };

  struct Preferences {
    std::array<int, 2> window_size{ 1280, 720 };
  } m_preferences;

private:
  // Core application state
  bool m_quit{ false };
  int m_modifier_state{ 0 }; // GLFW_MOD_CONTROL, GLFW_MOD_SHIFT, etc. (legacy -
                             // will be removed)
  unsigned long m_start_timestamp{ 0 };
  View* m_current_active_view{ nullptr }; // Currently active view for event
                                          // delegation

  // Input system (replaces scattered mouse/key tracking)
  std::unique_ptr<InputState> m_input_state;

  // GLFW window ownership
  GLFWwindow* m_window{ nullptr };

  // GLFW callbacks (static as required by GLFW C API)
  static void
  keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);
  static void
  mouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
  static void
  scrollCallback(GLFWwindow* window, double xoffset, double yoffset);
  static void
  cursorPositionCallback(GLFWwindow* window, double xpos, double ypos);
  static void windowSizeCallback(GLFWwindow* window, int width, int height);

  // Services and subsystems (owned by this context)
  std::unique_ptr<NcRender>        m_renderer;
  std::unique_ptr<ThemeManager>    m_theme_manager;
  std::unique_ptr<WebsocketClient> m_websocket;

  // Views (owned by this context)
  std::unique_ptr<NcControlView> m_control_view;
  std::unique_ptr<NcCamView>     m_cam_view;
  std::unique_ptr<NcAdminView>   m_admin_view;

  // Helper methods
  void logUptime();
  void modifierKeyCallback(const nlohmann::json& e);
  void registerModifierKeyEvents();

  // Event delegation helpers
  bool handleGlobalKeyEvent(int key, int action, int mods);
  bool handleGlobalMouseEvent(int button, int action, int mods);
  void delegateEventToActiveView();
};

#endif // NC_APP_H
