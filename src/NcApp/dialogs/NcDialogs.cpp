#include "NcDialogs.h"
#include "../NcApp.h"
#include <GL/gl.h>
#include <NcRender/NcRender.h>
#include <ThemeManager/ThemeManager.h>
#include <fstream>
#include <imgui.h>
#include <loguru.hpp>
#include <stb_image.h>

// Windows header for ShellExecuteA
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
#  include <shellapi.h>
#  include <windows.h>
#endif

NcDialogs::NcDialogs(NcApp* app) : m_app(app), m_pimpl(std::make_unique<Impl>())
{
}
NcDialogs::~NcDialogs() = default;

struct NcDialogs::Impl {
  GLuint logo_texture = 0;
};

void NcDialogs::render()
{
  if (m_preferences_visible)
    renderPreferences();
  if (m_progress_visible)
    renderProgressWindow();
  if (m_info_visible)
    renderInfoWindow();
  if (m_ask_window.visible)
    renderAskWindow();
  if (m_about_visible)
    renderAboutWindow();
}

void NcDialogs::showPreferences(bool visible)
{
  m_preferences_visible = visible;
}

void NcDialogs::renderPreferences()
{
  ImGui::Begin(
    "Preferences", &m_preferences_visible, ImGuiWindowFlags_AlwaysAutoResize);

  ImGui::InputInt2("Default Window Size",
                   m_app->m_preferences.window_size.data());
  ImGui::SameLine();
  if (ImGui::Button("<= Current Size")) {
    m_app->m_preferences.window_size[0] =
      (int) m_app->getRenderer().getWindowSize().x;
    m_app->m_preferences.window_size[1] =
      (int) m_app->getRenderer().getWindowSize().y;
  }

  ImGui::Separator();

  // Theme selection
  auto&       theme_mgr = m_app->getThemeManager();
  const auto& themes = theme_mgr.getAvailableThemes();
  const auto* active = theme_mgr.getActiveTheme();
  const char* preview = active ? active->name.c_str() : "None";

  if (ImGui::BeginCombo("Color Theme", preview)) {
    for (const auto& theme : themes) {
      bool is_selected = active && theme.name == active->name;
      if (ImGui::Selectable(theme.name.c_str(), is_selected)) {
        if (theme_mgr.setActiveTheme(theme.name)) {
          theme_mgr.applyTheme();
          const Color4f& bg = theme_mgr.getColor(ThemeColor::WindowBg);
          m_app->getRenderer().setClearColor(bg.r, bg.g, bg.b);
        }
      }
      if (ImGui::IsItemHovered() && !theme.description.empty()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", theme.description.c_str());
        ImGui::EndTooltip();
      }
      if (is_selected)
        ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }

  ImGui::Spacing();
  if (ImGui::Button("OK")) {
    nlohmann::json preferences;
    preferences["window_width"] = m_app->m_preferences.window_size[0];
    preferences["window_height"] = m_app->m_preferences.window_size[1];

    std::ofstream out(m_app->getRenderer().getConfigDirectory() +
                      "preferences.json");
    out << preferences.dump();
    out.close();
    showPreferences(false);
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    showPreferences(false);
  }
  ImGui::End();
}

void NcDialogs::showProgressWindow(bool visible)
{
  m_progress_visible = visible;
}

void NcDialogs::setProgressValue(float progress) { m_progress = progress; }

void NcDialogs::renderProgressWindow()
{
  ImGui::Begin(
    "Progress", &m_progress_visible, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::ProgressBar(m_progress, ImVec2(0.0f, 0.0f));
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::Text("Progress");
  ImGui::End();
}

void NcDialogs::showInfoWindow(bool visible) { m_info_visible = visible; }

void NcDialogs::setInfoValue(const std::string& info)
{
  m_info = info;
  LOG_F(WARNING, "Info Window => %s", m_info.c_str());
  showInfoWindow(true);
}

void NcDialogs::renderInfoWindow()
{
  ImGui::Begin("Info", &m_info_visible, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", m_info.c_str());
  if (ImGui::Button("Close")) {
    showInfoWindow(false);
  }
  ImGui::End();
}

void NcDialogs::askYesNo(const std::string&     question,
                         std::function<void()>  yes_callback,
                         std::function<void()>  no_callback,
                         std::optional<Point2d> placement)
{
  m_ask_window.text = question;
  m_ask_window.yes_callback = yes_callback;
  m_ask_window.no_callback = no_callback;
  m_ask_window.placement = placement;
  m_ask_window.visible = true;
  LOG_F(INFO, "Ask Window => %s", m_ask_window.text.c_str());
}

void NcDialogs::renderAskWindow()
{
  if (m_ask_window.placement.has_value()) {
    const Point2d& pos = m_ask_window.placement.value();
    auto           window_size = ImGui::GetIO().DisplaySize;
    float window_x = static_cast<float>(pos.x) + (window_size.x / 2.0f);
    float window_y = (window_size.y / 2.0f) - static_cast<float>(pos.y);

    ImGui::SetNextWindowPos(ImVec2(window_x + 10.0f, window_y + 10.0f),
                            ImGuiCond_Always);
  }

  ImGui::Begin(
    "Question", &m_ask_window.visible, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", m_ask_window.text.c_str());
  if (ImGui::Button("Yes")) {
    if (m_ask_window.yes_callback)
      m_ask_window.yes_callback();
    m_ask_window.visible = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("No")) {
    if (m_ask_window.no_callback)
      m_ask_window.no_callback();
    m_ask_window.visible = false;
  }
  ImGui::End();
}

void NcDialogs::loadLogoTexture()
{
  if (m_pimpl->logo_texture != 0)
    return;

// Include the generated header
#include "pch/logo_png.h"

  int            w, h, channels;
  unsigned char* pixels = stbi_load_from_memory(assets_logo_feathered_png,
                                                assets_logo_feathered_png_len,
                                                &w,
                                                &h,
                                                &channels,
                                                4);

  if (pixels) {
    glGenTextures(1, &m_pimpl->logo_texture);
    glBindTexture(GL_TEXTURE_2D, m_pimpl->logo_texture);
    glTexImage2D(
      GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    stbi_image_free(pixels);
  }
}

void NcDialogs::renderAboutWindow()
{
  if (!m_about_visible)
    return;

  // Center the window and set size relative to main window
  auto  window_size = ImGui::GetIO().DisplaySize;
  float about_window_width = window_size.x * 0.5f; // Half of main window width
  float about_window_height = window_size.y * 0.5f;

  ImGui::SetNextWindowPos(ImVec2(window_size.x * 0.5f, window_size.y * 0.5f),
                          ImGuiCond_Always,
                          ImVec2(0.5f, 0.5f));
  ImGui::SetNextWindowSize(ImVec2(about_window_width, about_window_height),
                           ImGuiCond_Always);

  ImGui::Begin("About NanoCut",
               &m_about_visible,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoResize);

  // Load texture if not already loaded
  if (m_pimpl->logo_texture == 0) {
    loadLogoTexture();
  }

  // Calculate the split point: 3/4 for logo/metadata, 1/4 for
  // description/button
  float content_region_height = ImGui::GetContentRegionAvail().y;
  float top_section_height = content_region_height * 0.70f;
  float bottom_section_height = content_region_height * 0.30f;

  // Display logo
  if (m_pimpl->logo_texture != 0) {
    // Calculate logo size constrained by both width and height
    float max_logo_width = about_window_width * 0.85f;
    float max_logo_height =
      top_section_height * 0.75f; // 75% of top section height

    // Use the smaller dimension to maintain square aspect ratio
    float logo_size = fmin(max_logo_width, max_logo_height);

    float available_width = ImGui::GetContentRegionAvail().x;
    float offset_x = (available_width - logo_size) * 0.5f;
    if (offset_x > 0) {
      ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset_x);
    }
    ImGui::Image((void*) (intptr_t) m_pimpl->logo_texture,
                 ImVec2(logo_size, logo_size));
    ImGui::Spacing();
  }

  // Display description text - centered
  float available_width = ImGui::GetContentRegionAvail().x;
  ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x -
                        ImGui::CalcTextSize("NanoCut Control Software").x) *
                       0.5f);
  ImGui::Text("NanoCut Control Software");
  ImGui::Spacing();

  char version_text[64];
  snprintf(version_text,
           sizeof(version_text),
           "Version: %d.%d.%d",
           VERSION_MAJOR,
           VERSION_MINOR,
           VERSION_PATCH);
  ImGui::SetCursorPosX(
    (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize(version_text).x) *
    0.5f);
  ImGui::Text("%s", version_text);

  // Move to the bottom section (1/4 of window height)
  ImGui::SetCursorPosY(top_section_height +
                       ImGui::GetFrameHeightWithSpacing() * 2);

  // Center the wrapped text
  float text_width =
    about_window_width * 0.8f; // Text takes 80% of window width
  ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - text_width) * 0.5f);

  ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + text_width);
  ImGui::TextWrapped(
    "NanoCut is a hobbyist's CNC control system designed for plasma cutting "
    "applications. It provides precise motion and Torch Height Control, "
    "advanced toolpath generation, and real-time monitoring capabilities.");
  ImGui::PopTextWrapPos();

  // Position close button at bottom of bottom section
  ImGui::SetCursorPosY(top_section_height + bottom_section_height);

  // Center the close button at the bottom
  float button_width =
    ImGui::CalcTextSize("Close").x + ImGui::GetStyle().FramePadding.x * 2.0f;
  ImGui::SetCursorPosX((ImGui::GetContentRegionAvail().x - button_width) *
                       0.5f);
  if (ImGui::Button("Close")) {
    m_about_visible = false;
  }

  ImGui::End();
}

void NcDialogs::openUrlInBrowser(const std::string& url)
{
  // Cross-platform URL opening
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__)
  // Windows
  ShellExecuteA(NULL, "open", url.c_str(), NULL, NULL, SW_SHOWNORMAL);
#elif __APPLE__
  // macOS
  std::string command = "open \"" + url + "\"";
  system(command.c_str());
#elif __linux__
  // Linux
  std::string command = "xdg-open \"" + url + "\"";
  system(command.c_str());
#else
  // Fallback - log error
  LOG_F(ERROR, "Platform not supported for opening URLs: %s", url.c_str());
#endif
}

void NcDialogs::renderHelpMenuItem()
{
  if (ImGui::MenuItem("Documentation", "")) {
    LOG_F(INFO, "Help->Documentation");
    openUrlInBrowser("https://nwjnilsson.github.io/nanocut-control/");
  }
  ImGui::Separator();
  if (ImGui::MenuItem("About", "")) {
    m_about_visible = true;
    LOG_F(INFO, "Help->About");
  }
}
