#pragma once

#include <NanoCut.h>
#include <imgui.h>

/**
 * ThemeColor - Unified color enum for the entire application.
 *
 * Replaces the old Color enum in NanoCut.h and provides themed colors
 * for all UI elements. Colors are indexed into a cached array on the
 * active Theme for O(1) lookup.
 *
 * Values are in 0-255 range (matching Color4f convention used by primitives).
 */
enum class ThemeColor : int {
  // ImGui color styles (unified with palette colors)
  Text,
  TextDisabled,
  WindowBg,
  ChildBg,
  PopupBg,
  Border,
  BorderShadow,
  FrameBg,
  FrameBgHovered,
  FrameBgActive,
  TitleBg,
  TitleBgActive,
  TitleBgCollapsed,
  MenuBarBg,
  ScrollbarBg,
  ScrollbarGrab,
  ScrollbarGrabHovered,
  ScrollbarGrabActive,
  CheckMark,
  SliderGrab,
  SliderGrabActive,
  Button,
  ButtonHovered,
  ButtonActive,
  Header,
  HeaderHovered,
  HeaderActive,
  Separator,
  SeparatorHovered,
  SeparatorActive,
  ResizeGrip,
  ResizeGripHovered,
  ResizeGripActive,
  Tab,
  TabHovered,
  TabActive,
  TabUnfocused,
  TabUnfocusedActive,
  PlotLines,
  PlotLinesHovered,
  PlotHistogram,
  PlotHistogramHovered,
  TableHeaderBg,
  TableBorderStrong,
  TableBorderLight,
  TableRowBg,
  TableRowBgAlt,
  TextSelectedBg,
  DragDropTarget,
  NavHighlight,
  NavWindowingHighlight,
  NavWindowingDimBg,
  ModalWindowDimBg,

  // App-specific UI colors
  MachinePlaneColor,
  CuttablePlaneColor,

  COUNT
};

/**
 * ThemeStyle - Unified style enum for ImGui styling parameters.
 */
enum class ThemeStyle : int {
  // Float style parameters
  WindowRounding,
  ChildRounding,
  FrameRounding,
  PopupRounding,
  ScrollbarRounding,
  GrabRounding,
  TabRounding,
  WindowBorderSize,
  ChildBorderSize,
  PopupBorderSize,
  FrameBorderSize,
  TabBorderSize,
  ScrollbarSize,
  GrabMinSize,
  IndentSpacing,
  DockingSeparatorSize,
  SeparatorTextBorderSize,

  // ImVec2 style parameters (must come after float parameters)
  WindowPadding,
  FramePadding,
  ItemSpacing,
  ItemInnerSpacing,

  COUNT // Must be last
};

// Default style values matching Dear ImGui defaults
constexpr float THEME_STYLE_DEFAULTS[] = {
  0.0f,  // WindowRounding
  0.0f,  // ChildRounding
  0.0f,  // FrameRounding
  0.0f,  // PopupRounding
  0.0f,  // ScrollbarRounding
  0.0f,  // GrabRounding
  4.0f,  // TabRounding
  1.0f,  // WindowBorderSize
  1.0f,  // ChildBorderSize
  1.0f,  // PopupBorderSize
  0.0f,  // FrameBorderSize
  0.0f,  // TabBorderSize
  14.0f, // ScrollbarSize
  10.0f, // GrabMinSize
  21.0f, // IndentSpacing
  2.0f,  // DockingSeparatorSize
  1.0f   // SeparatorTextBorderSize
};

// Default ImVec2 style values
constexpr ImVec2 THEME_STYLE_VEC2_DEFAULTS[] = {
  ImVec2(8.0f, 8.0f), // WindowPadding
  ImVec2(4.0f, 3.0f), // FramePadding
  ImVec2(8.0f, 4.0f), // ItemSpacing
  ImVec2(4.0f, 4.0f)  // ItemInnerSpacing
};

static_assert(sizeof(THEME_STYLE_DEFAULTS) / sizeof(THEME_STYLE_DEFAULTS[0]) ==
                static_cast<int>(ThemeStyle::WindowPadding),
              "THEME_STYLE_DEFAULTS must match float ThemeStyle count");

static_assert(sizeof(THEME_STYLE_VEC2_DEFAULTS) /
                  sizeof(THEME_STYLE_VEC2_DEFAULTS[0]) ==
                static_cast<int>(ThemeStyle::COUNT) -
                  static_cast<int>(ThemeStyle::WindowPadding),
              "THEME_STYLE_VEC2_DEFAULTS must match ImVec2 ThemeStyle count");

// Default color values (0-255 range) used when no theme is loaded
// These match the original hardcoded values in the codebase
constexpr Color4f THEME_COLOR_DEFAULTS[] = {
  { 255.0f, 255.0f, 255.0f, 255.0f }, // Text (was White)
  { 150.0f, 150.0f, 150.0f, 255.0f }, // TextDisabled
  { 20.0f, 20.0f, 20.0f, 255.0f },    // WindowBg
  { 0.0f, 0.0f, 0.0f, 255.0f },       // ChildBg
  { 29.0f, 32.0f, 48.0f, 255.0f },    // PopupBg
  { 0.0f, 0.0f, 0.0f, 255.0f },       // Border (was Black)
  { 0.0f, 0.0f, 0.0f, 0.0f },         // BorderShadow
  { 50.0f, 50.0f, 50.0f, 255.0f },    // FrameBg
  { 60.0f, 60.0f, 60.0f, 255.0f },    // FrameBgHovered
  { 70.0f, 70.0f, 70.0f, 255.0f },    // FrameBgActive
  { 30.0f, 30.0f, 30.0f, 255.0f },    // TitleBg
  { 40.0f, 40.0f, 40.0f, 255.0f },    // TitleBgActive
  { 25.0f, 25.0f, 25.0f, 255.0f },    // TitleBgCollapsed
  { 25.0f, 25.0f, 25.0f, 255.0f },    // MenuBarBg
  { 30.0f, 30.0f, 30.0f, 255.0f },    // ScrollbarBg
  { 60.0f, 60.0f, 60.0f, 255.0f },    // ScrollbarGrab
  { 70.0f, 70.0f, 70.0f, 255.0f },    // ScrollbarGrabHovered
  { 80.0f, 80.0f, 80.0f, 255.0f },    // ScrollbarGrabActive
  { 255.0f, 255.0f, 255.0f, 255.0f }, // CheckMark (was White)
  { 100.0f, 100.0f, 100.0f, 255.0f }, // SliderGrab (was Grey)
  { 120.0f, 120.0f, 120.0f, 255.0f }, // SliderGrabActive
  { 50.0f, 50.0f, 50.0f, 255.0f },    // Button
  { 60.0f, 60.0f, 60.0f, 255.0f },    // ButtonHovered
  { 70.0f, 70.0f, 70.0f, 255.0f },    // ButtonActive
  { 70.0f, 70.0f, 100.0f, 255.0f },   // Header
  { 80.0f, 80.0f, 110.0f, 255.0f },   // HeaderHovered
  { 90.0f, 90.0f, 120.0f, 255.0f },   // HeaderActive
  { 50.0f, 50.0f, 50.0f, 255.0f },    // Separator
  { 60.0f, 60.0f, 60.0f, 255.0f },    // SeparatorHovered
  { 70.0f, 70.0f, 70.0f, 255.0f },    // SeparatorActive
  { 100.0f, 100.0f, 100.0f, 255.0f }, // ResizeGrip (was Grey)
  { 120.0f, 120.0f, 120.0f, 255.0f }, // ResizeGripHovered
  { 140.0f, 140.0f, 140.0f, 255.0f }, // ResizeGripActive
  { 40.0f, 40.0f, 40.0f, 255.0f },    // Tab
  { 50.0f, 50.0f, 50.0f, 255.0f },    // TabHovered
  { 60.0f, 60.0f, 60.0f, 255.0f },    // TabActive
  { 30.0f, 30.0f, 30.0f, 255.0f },    // TabUnfocused
  { 40.0f, 40.0f, 40.0f, 255.0f },    // TabUnfocusedActive
  { 190.0f, 0.0f, 0.0f, 255.0f },     // PlotLines (was LightRed)
  { 255.0f, 0.0f, 0.0f, 255.0f },     // PlotLinesHovered (was Red)
  { 0.0f, 190.0f, 0.0f, 255.0f },     // PlotHistogram (was LightGreen)
  { 0.0f, 255.0f, 0.0f, 255.0f },     // PlotHistogramHovered (was Green)
  { 30.0f, 30.0f, 30.0f, 255.0f },    // TableHeaderBg
  { 50.0f, 50.0f, 50.0f, 255.0f },    // TableBorderStrong
  { 40.0f, 40.0f, 40.0f, 255.0f },    // TableBorderLight
  { 0.0f, 0.0f, 0.0f, 0.0f },         // TableRowBg
  { 10.0f, 10.0f, 10.0f, 255.0f },    // TableRowBgAlt
  { 0.0f, 0.0f, 190.0f, 255.0f },     // TextSelectedBg (was LightBlue)
  { 255.0f, 255.0f, 0.0f, 255.0f },   // DragDropTarget (was Yellow)
  { 0.0f, 0.0f, 190.0f, 255.0f },     // NavHighlight (was LightBlue)
  { 255.0f, 255.0f, 255.0f, 100.0f }, // NavWindowingHighlight
  { 20.0f, 20.0f, 20.0f, 200.0f },    // NavWindowingDimBg
  { 20.0f, 20.0f, 20.0f, 150.0f },    // ModalWindowDimBg

  { 69.0f, 69.0f, 69.0f, 255.0f }, // MachinePlaneColor
  { 148.0f, 8.0f, 8.0f, 255.0f },  // CuttablePlaneColor
};

static_assert(sizeof(THEME_COLOR_DEFAULTS) / sizeof(THEME_COLOR_DEFAULTS[0]) ==
                static_cast<int>(ThemeColor::COUNT),
              "THEME_COLOR_DEFAULTS must match ThemeColor::COUNT");
