#ifndef DIALOGS_
#define DIALOGS_

#include <NcRender/NcRender.h>
#include <functional>
#include <optional>
#include <string>

// Forward declarations
class NcApp;
class NcControlView;

/**
 * NcDialogs - Dialog window manager for the NC Control View
 * Manages all ImGui dialogs including preferences, machine parameters,
 * progress indicators, information popups, and controller status windows.
 */
class NcDialogs {
public:
  // Constructor/Destructor
  NcDialogs(NcApp* app, NcControlView* view);
  ~NcDialogs() = default;

  // Non-copyable, non-movable
  NcDialogs(const NcDialogs&) = delete;
  NcDialogs& operator=(const NcDialogs&) = delete;
  NcDialogs(NcDialogs&&) = delete;
  NcDialogs& operator=(NcDialogs&&) = delete;

  // Initialization
  void init();

  // Preferences dialog
  void showPreferences(bool visible);

  // Machine parameters dialog
  void showMachineParameters(bool visible);

  // Progress window
  void showProgressWindow(bool visible);
  void setProgressValue(float progress);

  // Info window
  void showInfoWindow(bool visible);
  void setInfoValue(const std::string& info);

  // Controller offline window
  void showControllerOfflineWindow(bool visible);
  void setControllerOfflineValue(const std::string& info);

  // Controller alarm window
  void showControllerAlarmWindow(bool visible);
  void setControllerAlarmValue(const std::string& alarm_text);
  bool isControllerAlarmWindowVisible() const;

  // Controller homing window
  void showControllerHomingWindow(bool visible);

  // THC (Torch Height Control) window
  void showThcWindow(bool visible);
  


  // Yes/No question dialog
  void askYesNo(const std::string&       question,
                std::function<void()>    yes_callback,
                std::function<void()>    no_callback = nullptr,
                std::optional<Point2d>   placement = std::nullopt);

private:
  // Application context
  NcApp*         m_app;
  NcControlView* m_view;

  // GUI window handles
  NcRender::NcRenderGui* m_file_open_handle = nullptr;
  NcRender::NcRenderGui* m_thc_window_handle = nullptr;
  NcRender::NcRenderGui* m_preferences_window_handle = nullptr;
  NcRender::NcRenderGui* m_machine_parameters_window_handle = nullptr;
  NcRender::NcRenderGui* m_progress_window_handle = nullptr;
  NcRender::NcRenderGui* m_info_window_handle = nullptr;
  NcRender::NcRenderGui* m_controller_offline_window_handle = nullptr;
  NcRender::NcRenderGui* m_controller_alarm_window_handle = nullptr;
  NcRender::NcRenderGui* m_controller_homing_window_handle = nullptr;

  // Ask window data
  struct AskWindowData {
    std::string           text;
    std::function<void()> yes_callback;
    std::function<void()> no_callback;
    NcRender::NcRenderGui* handle = nullptr;
    std::optional<Point2d> placement;
  };

  // Dialog state
  float                 m_progress = 0.0f;
  std::string           m_info;
  std::string           m_controller_alarm_text;
  AskWindowData         m_ask_window;

  // Private rendering methods
  void renderFileOpen();
  void renderPreferences();
  void renderMachineParameters();
  void renderProgressWindow();
  void renderInfoWindow();
  void renderControllerOfflineWindow();
  void renderControllerAlarmWindow();
  void renderControllerHomingWindow();
  void renderThcWindow();
  void renderAskWindow();
};

#endif // DIALOGS_
