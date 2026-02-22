#pragma once

#include <NanoCut.h>

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
  BackpaneBackground, // Side panel background (was hardcoded 25,44,71)
  BackgroundColor,    // GL clear color (from theme background_color)
  MachinePlaneColor,  // Machine plane box (from theme machine_plane_color)
  CuttablePlaneColor, // Cuttable/material plane box (from theme
                      // cuttable_plane_color)

  COUNT // Must be last
};

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

  { 25.0f, 44.0f, 71.0f, 255.0f }, // BackpaneBackground
  { 33.0f, 33.0f, 33.0f, 255.0f }, // BackgroundColor
  { 69.0f, 69.0f, 69.0f, 255.0f }, // MachinePlaneColor
  { 148.0f, 8.0f, 8.0f, 255.0f },  // CuttablePlaneColor
};

static_assert(sizeof(THEME_COLOR_DEFAULTS) / sizeof(THEME_COLOR_DEFAULTS[0]) ==
                static_cast<int>(ThemeColor::COUNT),
              "THEME_COLOR_DEFAULTS must match ThemeColor::COUNT");
