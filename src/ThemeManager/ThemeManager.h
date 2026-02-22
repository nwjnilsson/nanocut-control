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
    std::array<float, 3> machine_plane_color = { 0.27f, 0.27f, 0.27f };
    std::array<float, 3> cuttable_plane_color = { 0.58f, 0.03f, 0.03f };

    // Cached color array for O(1) lookup (0-255 range)
    std::array<Color4f, static_cast<int>(ThemeColor::COUNT)> color_cache;

    // ImGui style parameters (using same pattern as colors)
    std::array<float, static_cast<int>(ThemeStyle::WindowPadding)> style_floats;
    std::array<ImVec2, static_cast<int>(ThemeStyle::COUNT) - static_cast<int>(ThemeStyle::WindowPadding)> style_vec2s;

    // Build the color cache from theme values
    void buildColorCache();

    // O(1) color lookup
    Color4f getColor(ThemeColor color) const
    {
      return color_cache[static_cast<int>(color)];
    }

    // Initialize with defaults
    Theme() {
        // Initialize float styles with defaults
        for (int i = 0; i < static_cast<int>(ThemeStyle::WindowPadding); i++) {
            style_floats[i] = THEME_STYLE_DEFAULTS[i];
        }

        // Initialize ImVec2 styles with defaults
        for (int i = 0; i < static_cast<int>(ThemeStyle::COUNT) - static_cast<int>(ThemeStyle::WindowPadding); i++) {
            style_vec2s[i] = THEME_STYLE_VEC2_DEFAULTS[i];
        }
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
  bool setActiveTheme(const std::string& theme_name);

  // Apply current theme to ImGui
  void applyTheme();

  // Save active theme preference
  void saveActiveTheme();

  // Load active theme preference
  void loadActiveTheme();

  // Get theme by name
  const Theme* getThemeByName(const std::string& name) const;

  // O(1) color lookup via stable color cache (pointers into this are stable)
  const Color4f& getColor(ThemeColor color) const
  {
    return m_color_cache[static_cast<int>(color)];
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

  // Stable color cache - primitives hold const pointers into this array.
  // Updated in-place on theme change so all pointers remain valid.
  std::array<Color4f, static_cast<int>(ThemeColor::COUNT)> m_color_cache;

  // UI state
  bool         m_show_theme_selector = false;
  class NcApp* m_app = nullptr;

  // Load a single theme from JSON file
  bool loadThemeFromFile(const std::string& filename);

  // Apply ImGui colors from theme
  void applyImGuiColors(const Theme& theme);

  // Convert color names to ImGuiCol_ enum
  int colorNameToImGuiCol(const std::string& colorName) const;

  // Apply ImGui style settings from theme
  void applyImGuiStyle(const Theme& theme);
};
