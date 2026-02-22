#include "ThemeManager.h"
#include "NcApp/NcApp.h"
#include "NcControlView/NcControlView.h"
#include "NcRender/NcRender.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <loguru.hpp>
#include <sstream>

namespace fs = std::filesystem;

// Helper: convert RGBA 0-1 to Color4f 0-255
static Color4f toColor255(float r, float g, float b, float a = 1.0f)
{
  return { r * 255.0f, g * 255.0f, b * 255.0f, a * 255.0f };
}

static int imguiColorNameToThemeColor(const std::string& colorName)
{
  // Map ImGui color names to ThemeColor enum values
  static const std::map<std::string, ThemeColor> color_map = {
    { "Text", ThemeColor::Text },
    { "TextDisabled", ThemeColor::TextDisabled },
    { "WindowBg", ThemeColor::WindowBg },
    { "ChildBg", ThemeColor::ChildBg },
    { "PopupBg", ThemeColor::PopupBg },
    { "Border", ThemeColor::Border },
    { "BorderShadow", ThemeColor::BorderShadow },
    { "FrameBg", ThemeColor::FrameBg },
    { "FrameBgHovered", ThemeColor::FrameBgHovered },
    { "FrameBgActive", ThemeColor::FrameBgActive },
    { "TitleBg", ThemeColor::TitleBg },
    { "TitleBgActive", ThemeColor::TitleBgActive },
    { "TitleBgCollapsed", ThemeColor::TitleBgCollapsed },
    { "MenuBarBg", ThemeColor::MenuBarBg },
    { "ScrollbarBg", ThemeColor::ScrollbarBg },
    { "ScrollbarGrab", ThemeColor::ScrollbarGrab },
    { "ScrollbarGrabHovered", ThemeColor::ScrollbarGrabHovered },
    { "ScrollbarGrabActive", ThemeColor::ScrollbarGrabActive },
    { "CheckMark", ThemeColor::CheckMark },
    { "SliderGrab", ThemeColor::SliderGrab },
    { "SliderGrabActive", ThemeColor::SliderGrabActive },
    { "Button", ThemeColor::Button },
    { "ButtonHovered", ThemeColor::ButtonHovered },
    { "ButtonActive", ThemeColor::ButtonActive },
    { "Header", ThemeColor::Header },
    { "HeaderHovered", ThemeColor::HeaderHovered },
    { "HeaderActive", ThemeColor::HeaderActive },
    { "Separator", ThemeColor::Separator },
    { "SeparatorHovered", ThemeColor::SeparatorHovered },
    { "SeparatorActive", ThemeColor::SeparatorActive },
    { "ResizeGrip", ThemeColor::ResizeGrip },
    { "ResizeGripHovered", ThemeColor::ResizeGripHovered },
    { "ResizeGripActive", ThemeColor::ResizeGripActive },
    { "Tab", ThemeColor::Tab },
    { "TabHovered", ThemeColor::TabHovered },
    { "TabActive", ThemeColor::TabActive },
    { "TabUnfocused", ThemeColor::TabUnfocused },
    { "TabUnfocusedActive", ThemeColor::TabUnfocusedActive },
    { "PlotLines", ThemeColor::PlotLines },
    { "PlotLinesHovered", ThemeColor::PlotLinesHovered },
    { "PlotHistogram", ThemeColor::PlotHistogram },
    { "PlotHistogramHovered", ThemeColor::PlotHistogramHovered },
    { "TableHeaderBg", ThemeColor::TableHeaderBg },
    { "TableBorderStrong", ThemeColor::TableBorderStrong },
    { "TableBorderLight", ThemeColor::TableBorderLight },
    { "TableRowBg", ThemeColor::TableRowBg },
    { "TableRowBgAlt", ThemeColor::TableRowBgAlt },
    { "TextSelectedBg", ThemeColor::TextSelectedBg },
    { "DragDropTarget", ThemeColor::DragDropTarget },
    { "NavHighlight", ThemeColor::NavHighlight },
    { "NavWindowingHighlight", ThemeColor::NavWindowingHighlight },
    { "NavWindowingDimBg", ThemeColor::NavWindowingDimBg },
    { "ModalWindowDimBg", ThemeColor::ModalWindowDimBg }
  };

  auto it = color_map.find(colorName);
  if (it != color_map.end()) {
    return static_cast<int>(it->second);
  }

  LOG_F(WARNING,
        "Unknown ImGui color name for ThemeColor mapping: %s",
        colorName.c_str());
  return -1;
}

void ThemeManager::Theme::buildColorCache()
{
  // Start with defaults
  for (int i = 0; i < static_cast<int>(ThemeColor::COUNT); i++) {
    color_cache[i] = THEME_COLOR_DEFAULTS[i];
  }

  // Populate ImGui colors from theme
  for (const auto& [color_name, color] : imgui_colors) {
    int theme_color = imguiColorNameToThemeColor(color_name);
    if (theme_color >= 0) {
      color_cache[theme_color] =
        toColor255(color[0], color[1], color[2], color[3]);
    }
  }

  // App-specific colors from theme fields
  color_cache[static_cast<int>(ThemeColor::BackpaneBackground)] =
    toColor255(backpane_background[0],
               backpane_background[1],
               backpane_background[2],
               backpane_background[3]);

  // PanelBackground is now mapped to PopupBg, so it will be handled by the ImGui color mapping

  color_cache[static_cast<int>(ThemeColor::BackgroundColor)] =
    toColor255(background_color[0], background_color[1], background_color[2]);

  color_cache[static_cast<int>(ThemeColor::MachinePlaneColor)] = toColor255(
    machine_plane_color[0], machine_plane_color[1], machine_plane_color[2]);

  color_cache[static_cast<int>(ThemeColor::CuttablePlaneColor)] = toColor255(
    cuttable_plane_color[0], cuttable_plane_color[1], cuttable_plane_color[2]);
}

ThemeManager::ThemeManager(NcApp*             app,
                           const std::string& theme_dir,
                           const std::string& config_dir)
  : m_app(app), m_theme_directory(theme_dir), m_config_directory(config_dir)
{
  // Constructor
  loadThemes();
  loadActiveTheme();
}

ThemeManager::~ThemeManager()
{
  // Destructor
}

void ThemeManager::loadThemes()
{
  m_themes.clear();

  try {
    // Check if theme directory exists
    if (!fs::exists(m_theme_directory)) {
      LOG_F(WARNING,
            "Theme directory does not exist: %s",
            m_theme_directory.c_str());
      return;
    }

    // Iterate through all files in theme directory
    for (const auto& entry : fs::directory_iterator(m_theme_directory)) {
      if (entry.is_regular_file() && entry.path().extension() == ".json") {
        loadThemeFromFile(entry.path().string());
      }
    }

    if (m_themes.empty()) {
      LOG_F(WARNING,
            "No theme files found in directory: %s",
            m_theme_directory.c_str());
    }
    else {
      LOG_F(INFO,
            "Loaded %zu themes from %s",
            m_themes.size(),
            m_theme_directory.c_str());
    }
  }
  catch (const fs::filesystem_error& e) {
    LOG_F(ERROR, "Filesystem error while loading themes: %s", e.what());
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "Error loading themes: %s", e.what());
  }
}

bool ThemeManager::loadThemeFromFile(const std::string& filename)
{
  try {
    std::ifstream file(filename);
    if (!file.is_open()) {
      LOG_F(WARNING, "Failed to open theme file: %s", filename.c_str());
      return false;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    auto json_data = nlohmann::json::parse(content);

    Theme theme;
    theme.filename = filename;

    // Load metadata
    if (json_data.contains("name")) {
      theme.name = json_data["name"].get<std::string>();
    }
    else {
      // Use filename as name if not specified
      theme.name = fs::path(filename).stem().string();
    }

    if (json_data.contains("description")) {
      theme.description = json_data["description"].get<std::string>();
    }

    // Load ImGui colors
    if (json_data.contains("imgui_colors")) {
      for (auto& [color_name, color_value] :
           json_data["imgui_colors"].items()) {
        if (color_value.is_array() && color_value.size() >= 4) {
          std::array<float, 4> color = { color_value[0].get<float>(),
                                         color_value[1].get<float>(),
                                         color_value[2].get<float>(),
                                         color_value[3].get<float>() };
          theme.imgui_colors[color_name] = color;
        }
      }
    }

    // Load app-specific colors from app_colors section
    if (json_data.contains("app_colors") &&
        json_data["app_colors"].is_object()) {
      auto& app_colors = json_data["app_colors"];

      auto load_rgb = [&](const std::string&    key,
                          std::array<float, 3>& target) {
        if (app_colors.contains(key) && app_colors[key].is_array() &&
            app_colors[key].size() >= 3) {
          target = { app_colors[key][0].get<float>(),
                     app_colors[key][1].get<float>(),
                     app_colors[key][2].get<float>() };
        }
      };

      auto load_rgba = [&](const std::string&    key,
                           std::array<float, 4>& target) {
        if (app_colors.contains(key) && app_colors[key].is_array() &&
            app_colors[key].size() >= 4) {
          target = { app_colors[key][0].get<float>(),
                     app_colors[key][1].get<float>(),
                     app_colors[key][2].get<float>(),
                     app_colors[key][3].get<float>() };
        }
      };

      // Load background colors from app_colors
      load_rgb("background_color", theme.background_color);
      load_rgb("machine_plane_color", theme.machine_plane_color);
      load_rgb("cuttable_plane_color", theme.cuttable_plane_color);

      // Load other app colors
      load_rgba("backpane_background", theme.backpane_background);
    }
    else {
      // Fallback to old structure for backward compatibility
      LOG_F(WARNING, "Theme file uses deprecated structure. Please move background colors to app_colors section.");

      if (json_data.contains("background_color") &&
          json_data["background_color"].is_array()) {
        auto& bg_color = json_data["background_color"];
        if (bg_color.size() >= 3) {
          theme.background_color = { bg_color[0].get<float>(),
                                     bg_color[1].get<float>(),
                                     bg_color[2].get<float>() };
        }
      }

      if (json_data.contains("machine_plane_color") &&
          json_data["machine_plane_color"].is_array()) {
        auto& mp_color = json_data["machine_plane_color"];
        if (mp_color.size() >= 3) {
          theme.machine_plane_color = { mp_color[0].get<float>(),
                                        mp_color[1].get<float>(),
                                        mp_color[2].get<float>() };
        }
      }

      if (json_data.contains("cuttable_plane_color") &&
          json_data["cuttable_plane_color"].is_array()) {
        auto& cp_color = json_data["cuttable_plane_color"];
        if (cp_color.size() >= 3) {
          theme.cuttable_plane_color = { cp_color[0].get<float>(),
                                         cp_color[1].get<float>(),
                                         cp_color[2].get<float>() };
        }
      }
    }

    // Build the color cache for O(1) lookup
    theme.buildColorCache();

    m_themes.push_back(theme);
    LOG_F(INFO, "Loaded theme: %s", theme.name.c_str());
    return true;
  }
  catch (const nlohmann::json::exception& e) {
    LOG_F(ERROR,
          "JSON parsing error in theme file %s: %s",
          filename.c_str(),
          e.what());
    return false;
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "Error loading theme file %s: %s", filename.c_str(), e.what());
    return false;
  }
}

const ThemeManager::Theme*
ThemeManager::getThemeByName(const std::string& name) const
{
  for (const auto& theme : m_themes) {
    if (theme.name == name) {
      return &theme;
    }
  }
  return nullptr;
}

bool ThemeManager::setActiveTheme(const std::string& themeName)
{
  const Theme* theme = getThemeByName(themeName);
  if (!theme) {
    LOG_F(WARNING, "Theme not found: %s", themeName.c_str());
    return false;
  }

  m_active_theme = const_cast<Theme*>(theme); // Remove const for internal use
  m_active_theme_name = themeName;
  saveActiveTheme();
  return true;
}

void ThemeManager::applyTheme()
{
  if (!m_active_theme) {
    LOG_F(WARNING, "No active theme to apply");
    return;
  }

  // Apply ImGui colors
  applyImGuiColors(*m_active_theme);

  // Apply ImGui style settings
  ImGuiStyle& style = ImGui::GetStyle();
  style.ChildBorderSize = 1.0f;
  style.FrameBorderSize = 0.0f;
  style.PopupBorderSize = 1.0f;
  style.WindowBorderSize = 0.0f;
  style.FrameRounding = 3.0f;
  style.Alpha = 1.0f;
}

void ThemeManager::applyImGuiColors(const Theme& theme)
{
  ImGuiStyle& style = ImGui::GetStyle();

  // Apply each ImGui color from the theme
  for (const auto& [color_name, color] : theme.imgui_colors) {
    int imgui_col = colorNameToImGuiCol(color_name);
    if (imgui_col >= 0 && imgui_col < ImGuiCol_COUNT) {
      style.Colors[imgui_col] = ImVec4(color[0], color[1], color[2], color[3]);
    }
  }
}

int ThemeManager::colorNameToImGuiCol(const std::string& colorName) const
{
  // Map color names to ImGuiCol_ enum values
  static const std::map<std::string, int> color_map = {
    { "Text", ImGuiCol_Text },
    { "TextDisabled", ImGuiCol_TextDisabled },
    { "WindowBg", ImGuiCol_WindowBg },
    { "ChildBg", ImGuiCol_ChildBg },
    { "PopupBg", ImGuiCol_PopupBg },
    { "Border", ImGuiCol_Border },
    { "BorderShadow", ImGuiCol_BorderShadow },
    { "FrameBg", ImGuiCol_FrameBg },
    { "FrameBgHovered", ImGuiCol_FrameBgHovered },
    { "FrameBgActive", ImGuiCol_FrameBgActive },
    { "TitleBg", ImGuiCol_TitleBg },
    { "TitleBgActive", ImGuiCol_TitleBgActive },
    { "TitleBgCollapsed", ImGuiCol_TitleBgCollapsed },
    { "MenuBarBg", ImGuiCol_MenuBarBg },
    { "ScrollbarBg", ImGuiCol_ScrollbarBg },
    { "ScrollbarGrab", ImGuiCol_ScrollbarGrab },
    { "ScrollbarGrabHovered", ImGuiCol_ScrollbarGrabHovered },
    { "ScrollbarGrabActive", ImGuiCol_ScrollbarGrabActive },
    { "CheckMark", ImGuiCol_CheckMark },
    { "SliderGrab", ImGuiCol_SliderGrab },
    { "SliderGrabActive", ImGuiCol_SliderGrabActive },
    { "Button", ImGuiCol_Button },
    { "ButtonHovered", ImGuiCol_ButtonHovered },
    { "ButtonActive", ImGuiCol_ButtonActive },
    { "Header", ImGuiCol_Header },
    { "HeaderHovered", ImGuiCol_HeaderHovered },
    { "HeaderActive", ImGuiCol_HeaderActive },
    { "Separator", ImGuiCol_Separator },
    { "SeparatorHovered", ImGuiCol_SeparatorHovered },
    { "SeparatorActive", ImGuiCol_SeparatorActive },
    { "ResizeGrip", ImGuiCol_ResizeGrip },
    { "ResizeGripHovered", ImGuiCol_ResizeGripHovered },
    { "ResizeGripActive", ImGuiCol_ResizeGripActive },
    { "Tab", ImGuiCol_Tab },
    { "TabHovered", ImGuiCol_TabHovered },
    { "TabActive", ImGuiCol_TabActive },
    { "TabUnfocused", ImGuiCol_TabUnfocused },
    { "TabUnfocusedActive", ImGuiCol_TabUnfocusedActive },
    { "PlotLines", ImGuiCol_PlotLines },
    { "PlotLinesHovered", ImGuiCol_PlotLinesHovered },
    { "PlotHistogram", ImGuiCol_PlotHistogram },
    { "PlotHistogramHovered", ImGuiCol_PlotHistogramHovered },
    { "TableHeaderBg", ImGuiCol_TableHeaderBg },
    { "TableBorderStrong", ImGuiCol_TableBorderStrong },
    { "TableBorderLight", ImGuiCol_TableBorderLight },
    { "TableRowBg", ImGuiCol_TableRowBg },
    { "TableRowBgAlt", ImGuiCol_TableRowBgAlt },
    { "TextSelectedBg", ImGuiCol_TextSelectedBg },
    { "DragDropTarget", ImGuiCol_DragDropTarget },
    { "NavHighlight", ImGuiCol_NavHighlight },
    { "NavWindowingHighlight", ImGuiCol_NavWindowingHighlight },
    { "NavWindowingDimBg", ImGuiCol_NavWindowingDimBg },
    { "ModalWindowDimBg", ImGuiCol_ModalWindowDimBg }
  };

  auto it = color_map.find(colorName);
  if (it != color_map.end()) {
    return it->second;
  }

  LOG_F(WARNING, "Unknown ImGui color name: %s", colorName.c_str());
  return -1;
}

void ThemeManager::saveActiveTheme()
{
  if (m_active_theme_name.empty()) {
    return;
  }

  try {
    nlohmann::json config;
    config["active_theme"] = m_active_theme_name;

    std::string   config_file = m_config_directory + "theme_preferences.json";
    std::ofstream out(config_file);
    out << config.dump(4);
    out.close();

    LOG_F(INFO, "Saved active theme: %s", m_active_theme_name.c_str());
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "Error saving active theme: %s", e.what());
  }
}

void ThemeManager::loadActiveTheme()
{
  try {
    std::string   config_file = m_config_directory + "theme_preferences.json";
    std::ifstream in(config_file);

    if (in.is_open()) {
      std::stringstream buffer;
      buffer << in.rdbuf();
      std::string content = buffer.str();

      auto json_data = nlohmann::json::parse(content);

      if (json_data.contains("active_theme")) {
        std::string theme_name = json_data["active_theme"].get<std::string>();

        if (setActiveTheme(theme_name)) {
          LOG_F(INFO, "Loaded active theme: %s", theme_name.c_str());
        }
        else {
          LOG_F(WARNING,
                "Active theme not found: %s, using default",
                theme_name.c_str());
          // Try to set first available theme as fallback
          if (!m_themes.empty()) {
            setActiveTheme(m_themes[0].name);
          }
        }
      }
    }
    else {
      LOG_F(INFO, "No theme preferences file found, using default theme");
      // Set first available theme as default
      if (!m_themes.empty()) {
        setActiveTheme(m_themes[0].name);
      }
    }
  }
  catch (const std::exception& e) {
    LOG_F(ERROR, "Error loading active theme: %s", e.what());
    // Try to set first available theme as fallback
    if (!m_themes.empty()) {
      setActiveTheme(m_themes[0].name);
    }
  }
}

void ThemeManager::showThemeSelector(bool visible)
{
  m_show_theme_selector = visible;
}

void ThemeManager::renderThemeSelector()
{
  if (!m_show_theme_selector) {
    return;
  }

  ImGui::Begin(
    "Select Theme", &m_show_theme_selector, ImGuiWindowFlags_AlwaysAutoResize);

  if (m_themes.empty()) {
    ImGui::Text("No themes available.");
  }
  else {
    ImGui::Text("Available Themes:");
    ImGui::Separator();

    for (const auto& theme : m_themes) {
      bool is_active = m_active_theme && theme.name == m_active_theme->name;

      if (ImGui::Selectable(theme.name.c_str(), is_active)) {
        if (setActiveTheme(theme.name)) {
          applyTheme();

          // Update app colors from theme via color cache
          if (m_app) {
            auto&   renderer = m_app->getRenderer();
            Color4f bg = getColor(ThemeColor::BackgroundColor);
            renderer.setClearColor(bg.r, bg.g, bg.b);

            // Update machine plane colors
            auto& control_view = m_app->getControlView();
            control_view.m_machine_plane->color =
              getColor(ThemeColor::MachinePlaneColor);
            control_view.m_cuttable_plane->color =
              getColor(ThemeColor::CuttablePlaneColor);

            // Invalidate HMI colors to force update of all UI elements
            control_view.getHmi().invalidateColors();
          }
        }
      }

      if (ImGui::IsItemHovered()) {
        ImGui::BeginTooltip();
        ImGui::Text("%s", theme.description.c_str());
        ImGui::EndTooltip();
      }
    }
  }

  ImGui::Spacing();
  if (ImGui::Button("Close")) {
    showThemeSelector(false);
  }
  ImGui::End();
}
