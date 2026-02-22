#pragma once

#include "ThemeColor.h"
#include <array>
#include <imgui.h>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>

// Forward declaration
class NcApp;

class ThemeManager {
public:
  struct Theme {
    std::string name;
    std::string description;
    std::string filename;

    // ImGui colors (RGBA values 0-1)
    std::map<std::string, std::array<float, 4>> imgui_colors;

    // App-specific colors (RGB values 0-1 range, converted to 0-255 in cache)
    std::array<float, 3> background_color = { 0.13f, 0.13f, 0.13f };
    std::array<float, 3> machine_plane_color = { 0.27f, 0.27f, 0.27f };
    std::array<float, 3> cuttable_plane_color = { 0.58f, 0.03f, 0.03f };

    // Extended app colors (RGBA 0-1 range)
    std::array<float, 4> backpane_background = { 0.098f, 0.173f, 0.278f, 1.0f };

    // Cached color array for O(1) lookup (0-255 range)
    std::array<Color4f, static_cast<int>(ThemeColor::COUNT)> color_cache;

    // Build the color cache from theme values
    void buildColorCache();

    // O(1) color lookup
    Color4f getColor(ThemeColor color) const
    {
      return color_cache[static_cast<int>(color)];
    }
  };

  explicit ThemeManager(NcApp*             app,
                        const std::string& theme_dir,
                        const std::string& config_dir);
  ~ThemeManager();

  // Load all available themes
  void loadThemes();

  // Get list of available themes
  const std::vector<Theme>& getAvailableThemes() const { return m_themes; }

  // Get current active theme
  const Theme* getActiveTheme() const { return m_active_theme; }

  // Set active theme by name
  bool setActiveTheme(const std::string& themeName);

  // Apply current theme to ImGui
  void applyTheme();

  // Save active theme preference
  void saveActiveTheme();

  // Load active theme preference
  void loadActiveTheme();

  // Get theme by name
  const Theme* getThemeByName(const std::string& name) const;

  // O(1) color lookup via active theme (falls back to defaults)
  Color4f getColor(ThemeColor color) const
  {
    if (m_active_theme) {
      return m_active_theme->getColor(color);
    }
    return THEME_COLOR_DEFAULTS[static_cast<int>(color)];
  }

  // Theme selector UI
  void renderThemeSelector();
  bool isThemeSelectorVisible() const { return m_show_theme_selector; }
  void showThemeSelector(bool visible);

private:
  std::string        m_theme_directory;
  std::string        m_config_directory;
  std::vector<Theme> m_themes;
  Theme*             m_active_theme = nullptr;
  std::string        m_active_theme_name;

  // UI state
  bool         m_show_theme_selector = false;
  class NcApp* m_app = nullptr;

  // Load a single theme from JSON file
  bool loadThemeFromFile(const std::string& filename);

  // Apply ImGui colors from theme
  void applyImGuiColors(const Theme& theme);

  // Convert color names to ImGuiCol_ enum
  int colorNameToImGuiCol(const std::string& colorName) const;
};
