#ifndef NC_APP_DIALOGS_
#define NC_APP_DIALOGS_

#include <NcRender/geometry/geometry.h>
#include <functional>
#include <optional>
#include <string>

class NcApp;

class NcDialogs {
public:
  explicit NcDialogs(NcApp* app);
  ~NcDialogs();

  NcDialogs(const NcDialogs&) = delete;
  NcDialogs& operator=(const NcDialogs&) = delete;

  void render();

  // Preferences
  void showPreferences(bool visible);

  // Progress window
  void showProgressWindow(bool visible);
  void setProgressValue(float progress);

  // Info window
  void showInfoWindow(bool visible);
  void setInfoValue(const std::string& info);

  // Yes/No question dialog
  void askYesNo(const std::string&     question,
                std::function<void()>  yes_callback,
                std::function<void()>  no_callback = nullptr,
                std::optional<Point2d> placement = std::nullopt);

  // Help menu
  void renderHelpMenuItem();

private:
  void openUrlInBrowser(const std::string& url);
  NcApp* m_app;

  // Visibility flags (app-level dialogs render outside view-scoped pushGui)
  bool m_preferences_visible = false;
  bool m_progress_visible = false;
  bool m_info_visible = false;

  // Dialog state
  float       m_progress = 0.0f;
  std::string m_info;

  // About window state
  bool m_about_visible = false;

  struct Impl;
  std::unique_ptr<Impl> m_pimpl;

  // Ask window data
  struct AskWindowData {
    std::string            text;
    std::function<void()>  yes_callback;
    std::function<void()>  no_callback;
    std::optional<Point2d> placement;
    bool                   visible = false;
  };
  AskWindowData m_ask_window;

  // Render methods
  void renderPreferences();
  void renderProgressWindow();
  void renderInfoWindow();
  void renderAskWindow();
  void renderAboutWindow();
  void loadLogoTexture();
};

#endif // NC_APP_DIALOGS_
