#include "hmi.h"
#include "../../Input/InputEvents.h"
#include "../../Input/InputState.h"
#include "../../NcApp/NcApp.h"
#include "../../NcRender/primitives/Primitives.h"
#include "../gcode/gcode.h"
#include "../motion_control/motion_controller.h"
#include "../util.h"
#include "NcControlView/NcControlView.h"
#include "ThemeManager/ThemeManager.h"
#include <cmath>
#include <fstream>
#include <imgui.h>

// Helper function to parse button ID string into enum
static HmiButtonId parseButtonId(const std::string& id)
{
  if (id == "Wpos")
    return HmiButtonId::Wpos;
  if (id == "Park")
    return HmiButtonId::Park;
  if (id == "Zero X")
    return HmiButtonId::ZeroX;
  if (id == "Zero Y")
    return HmiButtonId::ZeroY;
  if (id == "Zero Z")
    return HmiButtonId::ZeroZ;
  if (id == "Spindle On")
    return HmiButtonId::SpindleOn;
  if (id == "Spindle Off")
    return HmiButtonId::SpindleOff;
  if (id == "Retract")
    return HmiButtonId::Retract;
  if (id == "Touch")
    return HmiButtonId::Touch;
  if (id == "Run")
    return HmiButtonId::Run;
  if (id == "Test Run")
    return HmiButtonId::TestRun;
  if (id == "Abort")
    return HmiButtonId::Abort;
  if (id == "Clean")
    return HmiButtonId::Clean;
  if (id == "Fit")
    return HmiButtonId::Fit;
  if (id == "Home")
    return HmiButtonId::Home;

  LOG_F(WARNING, "Unknown HMI button ID: %s", id.c_str());
  return HmiButtonId::Unknown;
}

// Helper: convert Color4f (0-255) to ImVec4 (0-1)
static ImVec4 toImGuiColor(const Color4f& c)
{
  return ImVec4(c.r / 255.0f, c.g / 255.0f, c.b / 255.0f, c.a / 255.0f);
}

// Render the DRO (Digital Read Out) panel
void NcHmi::renderDro()
{
  if (!m_app)
    return;

  ImVec2 window_size = ImGui::GetIO().DisplaySize;
  float  panel_x =
    window_size.x - m_panel_width - m_app->getRenderer().scaleUI(1.f);

  float menu_bar_height = ImGui::GetFrameHeight();
  ImGui::SetNextWindowPos(ImVec2(panel_x, menu_bar_height));
  ImGui::SetNextWindowSize(ImVec2(m_panel_width, m_dro_backplane_height));

  // Torch-on changes the DRO background color
  if (m_dro.torch_on) {
    const Color4f& base = m_app->getColor(ThemeColor::WindowBg);
    Color4f        torch_color;
    torch_color.r = std::min(255.0f, base.r * 1.2f);
    torch_color.g = std::max(0.0f, base.g * 0.3f);
    torch_color.b = std::max(0.0f, base.b * 0.5f);
    torch_color.a = base.a;
    ImGui::PushStyleColor(ImGuiCol_WindowBg, toImGuiColor(torch_color));
  }
  else {
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          toImGuiColor(m_app->getColor(ThemeColor::WindowBg)));
  }

  constexpr auto win_flags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

  if (ImGui::Begin("DRO Panel", nullptr, win_flags)) {
    ImVec4 text_color = toImGuiColor(m_app->getColor(ThemeColor::Text));
    ImVec4 work_color = toImGuiColor(m_app->getColor(ThemeColor::PlotLines));
    ImVec4 abs_color =
      toImGuiColor(m_app->getColor(ThemeColor::PlotLinesHovered));
    ImVec4 arc_color =
      m_dro.arc_ok ? toImGuiColor(m_app->getColor(ThemeColor::PlotLinesHovered))
                   : toImGuiColor(m_app->getColor(ThemeColor::PlotLines));
    ImVec4 sep_color = toImGuiColor(m_app->getColor(ThemeColor::Separator));

    // Status row: FEED | ARC | SET | RUN
    ImGui::SetWindowFontScale(0.65f);
    // float status_height = ImGui::GetTextLineHeightWithSpacing();
    // ImGui::SetCursorPosY(status_height);
    float component_width = m_panel_width / 4.f;
    ImGui::TextColored(abs_color, "%s", m_dro.feed.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(component_width * 1.25f);
    ImGui::TextColored(arc_color, "%s", m_dro.arc_readout.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(component_width * 2.20f);
    ImGui::TextColored(abs_color, "%s", m_dro.arc_set.c_str());
    ImGui::SameLine();
    ImGui::SetCursorPosX(component_width * 3.f);
    ImGui::TextColored(abs_color, "%s", m_dro.run_time.c_str());
    ImGui::SetWindowFontScale(1.0f);

    // Divide remaining height evenly among 3 axis rows
    float status_bottom = ImGui::GetCursorPosY();
    float total_height = ImGui::GetContentRegionAvail().y;
    float row_height = total_height / 3.0f;

    auto renderAxis = [&](int                  index,
                          const char*          label,
                          const DroAxisValues& axis) {
      float row_y = status_bottom + index * row_height;
      ImGui::SetCursorPosY(row_y);

      ImGui::PushStyleColor(ImGuiCol_Separator, sep_color);
      ImGui::Separator();
      ImGui::PopStyleColor();

      // Label (large)
      ImGui::SetWindowFontScale(2.5f);
      ImGui::TextColored(text_color, "%s", label);
      ImGui::SameLine();

      // Work coordinate readout (large)
      ImGui::SetWindowFontScale(2.0f);
      ImGui::TextColored(work_color, "%s", axis.work_readout.c_str());
      ImGui::SameLine();

      // Absolute coordinate readout (small, right-aligned)
      ImGui::SetWindowFontScale(0.75f);
      float  large_height = ImGui::GetFontSize() * 2.0f / 0.75f;
      float  small_height = ImGui::GetFontSize();
      ImVec2 abs_text_size = ImGui::CalcTextSize(axis.absolute_readout.c_str());
      float  right_edge =
        ImGui::GetContentRegionAvail().x + ImGui::GetCursorPosX();
      ImGui::SetCursorPosX(right_edge - abs_text_size.x);
      ImGui::SetCursorPosY(ImGui::GetCursorPosY() +
                           (large_height - small_height));
      ImGui::TextColored(abs_color, "%s", axis.absolute_readout.c_str());
      ImGui::SetWindowFontScale(1.0f);
    };

    renderAxis(0, "Z", m_dro.z);
    renderAxis(1, "Y", m_dro.y);
    renderAxis(2, "X", m_dro.x);
  }

  ImGui::End();
  ImGui::PopStyleColor(); // WindowBg
}

// Render THC baby-step widget
void NcHmi::renderThcWidget()
{
  if (!m_app)
    return;

  static constexpr ThcButtonDef thc_buttons[] = {
    { -4.0f, "-4" }, { -1.5f, "-1.5" }, { -0.5f, "-.5" },
    { 0.5f, "+.5" }, { 1.5f, "+1.5" },  { 4.0f, "+4" },
  };

  ImVec2 window_size = ImGui::GetIO().DisplaySize;
  float  panel_x =
    window_size.x - m_panel_width - m_app->getRenderer().scaleUI(1.f);
  float menu_bar_height = ImGui::GetFrameHeight();
  float thc_y = menu_bar_height + m_dro_backplane_height;
  float thc_height = 2.f * ImGui::GetTextLineHeightWithSpacing();

  ImGui::SetNextWindowPos(ImVec2(panel_x, thc_y));
  ImGui::SetNextWindowSize(ImVec2(m_panel_width, thc_height));

  constexpr auto win_flags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        toImGuiColor(m_app->getColor(ThemeColor::PopupBg)));

  if (ImGui::Begin("THC Widget", nullptr, win_flags)) {
    // Remove horizontal spacing, sharp corners, and add border for tight button
    // layout
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 2.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 0.0f);

    float available_width = ImGui::GetContentRegionAvail().x;
    // 7 cells: 3 buttons + 1 readout + 3 buttons
    float cell_w = available_width / 7.0f;
    float btn_height = ImGui::GetContentRegionAvail().y;

    ImGui::SetWindowFontScale(0.80f);

    // First 3 buttons (negative deltas)
    for (int i = 0; i < 3; i++) {
      if (i > 0)
        ImGui::SameLine();
      if (ImGui::Button(thc_buttons[i].label, ImVec2(cell_w, btn_height))) {
        m_view->getMotionController().adjustThcOffset(thc_buttons[i].delta);
      }
    }

    // Offset readout (center cell)
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg,
                          toImGuiColor(m_app->getColor(ThemeColor::FrameBg)));
    ImGui::BeginChild("thc_offset", ImVec2(cell_w, btn_height));
    ImGui::SetWindowFontScale(0.80f);
    ImVec2 text_size = ImGui::CalcTextSize(m_thc_offset_readout.c_str());
    ImGui::SetCursorPos(
      ImVec2((cell_w - text_size.x) / 2.0f, (btn_height - text_size.y) / 2.0f));
    ImGui::TextColored(
      toImGuiColor(m_app->getColor(ThemeColor::PlotLinesHovered)),
      "%s",
      m_thc_offset_readout.c_str());
    ImGui::EndChild();
    ImGui::PopStyleColor(); // ChildBg

    // Last 3 buttons (positive deltas)
    for (int i = 3; i < 6; i++) {
      ImGui::SameLine();
      if (ImGui::Button(thc_buttons[i].label, ImVec2(cell_w, btn_height))) {
        m_view->getMotionController().adjustThcOffset(thc_buttons[i].delta);
      }
    }

    ImGui::SetWindowFontScale(1.0f);
    ImGui::PopStyleVar(4);
  }
  ImGui::End();
  ImGui::PopStyleColor(); // WindowBg
}

// Helper function to render buttons with safety checks
void NcHmi::renderButtonWithSafety(const char* label,
                                   HmiButtonId id,
                                   float       width,
                                   float       height)
{
  ButtonCategory category = getButtonCategory(id);
  bool           should_disable = false;

  // Disable motion buttons when machine is in motion
  if (category == ButtonCategory::MotionButton && !isMotionAllowed()) {
    should_disable = true;
  }

  // Special handling for Home button (additional safeguards)
  if (id == HmiButtonId::Home && !isHomingAllowed()) {
    should_disable = true;
  }

  if (should_disable) {
    ImGui::BeginDisabled();
  }

  if (ImGui::Button(label, ImVec2(width, height))) {
    handleButton(label);
  }

  if (should_disable) {
    if (ImGui::IsItemHovered()) {
      if (id == HmiButtonId::Home) {
        ImGui::SetTooltip("Homing not available while machine is busy!");
      }
      else {
        ImGui::SetTooltip("Button disabled while machine is in motion");
      }
    }
    ImGui::EndDisabled();
  }
}

// Main ImGui rendering function for HMI - renders the button panel
void NcHmi::renderHmi()
{
  if (!m_app)
    return;

  // Render all three panels
  renderDro();
  renderThcWidget();

  ImVec2 window_size = ImGui::GetIO().DisplaySize;
  float  panel_x =
    window_size.x - m_panel_width - m_app->getRenderer().scaleUI(1.f);
  float menu_bar_height = ImGui::GetFrameHeight();
  float panel_y = menu_bar_height + m_dro_backplane_height +
                  2.f * ImGui::GetTextLineHeightWithSpacing();
  float panel_width = m_panel_width;
  float panel_height = window_size.y - panel_y;

  ImGui::SetNextWindowPos(ImVec2(panel_x, panel_y));
  ImGui::SetNextWindowSize(ImVec2(panel_width, panel_height));

  ImGui::PushStyleColor(ImGuiCol_WindowBg,
                        toImGuiColor(m_app->getColor(ThemeColor::PopupBg)));

  float scaled_spacing_x = ImGui::GetStyle().ItemSpacing.x;
  float scaled_spacing_y = ImGui::GetStyle().ItemSpacing.y;

  constexpr auto win_flags =
    ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoBringToFrontOnFocus;

  if (ImGui::Begin("HMI Buttons", nullptr, win_flags)) {
    float button_width = (panel_width - 3.f * scaled_spacing_x) / 2.0f;
    float total_spacing =
      8.0f * scaled_spacing_y; // Added extra row for new layout
    float button_height = (panel_height - total_spacing) / 6.0f;

    float total_group_width = button_width * 2.0f + scaled_spacing_x;
    float cursor_x = (panel_width - total_group_width) / 2.0f;

    // Row 1: Zero buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Zero X", HmiButtonId::ZeroX, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Zero Y", HmiButtonId::ZeroY, button_width, button_height);

    // Row 2: Touch/Retract buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Touch", HmiButtonId::Touch, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Retract", HmiButtonId::Retract, button_width, button_height);

    // Row 3: Fit/Clean buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Fit", HmiButtonId::Fit, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Clean", HmiButtonId::Clean, button_width, button_height);

    // Row 4: Wpos/Park buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Wpos", HmiButtonId::Wpos, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Park", HmiButtonId::Park, button_width, button_height);

    // Row 5: Home/Test Run buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Home", HmiButtonId::Home, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Test Run", HmiButtonId::TestRun, button_width, button_height);

    // Row 6: Run/Abort buttons
    ImGui::SetCursorPosX(cursor_x);
    renderButtonWithSafety(
      "Run", HmiButtonId::Run, button_width, button_height);
    ImGui::SameLine();
    renderButtonWithSafety(
      "Abort", HmiButtonId::Abort, button_width, button_height);
  }
  ImGui::End();
  ImGui::PopStyleColor(); // WindowBg
}

// Constructor
NcHmi::NcHmi(NcApp* app, NcControlView* view) : m_app(app), m_view(view)
{
  // Member variables are initialized via member initializer list or in-class
  // initializers
}

void NcHmi::getBoundingBox(Point2d* bbox_min, Point2d* bbox_max)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  auto& stack = m_app->getRenderer().getPrimitiveStack();
  bbox_max->x = std::numeric_limits<int>::min();
  bbox_max->y = std::numeric_limits<int>::min();
  bbox_min->x = std::numeric_limits<int>::max();
  bbox_min->y = std::numeric_limits<int>::max();
  for (int x = 0; x < stack.size(); x++) {
    auto* path = dynamic_cast<Path*>(stack.at(x).get());
    if (path) {
      for (int i = 0; i < path->m_points.size(); i++) {
        const float px = path->m_points.at(i).x -
                         control_view.m_machine_parameters.work_offset[0];
        const float py = path->m_points.at(i).y -
                         control_view.m_machine_parameters.work_offset[1];
        if (px < bbox_min->x)
          bbox_min->x = px;
        if (px > bbox_max->x)
          bbox_max->x = px;
        if (py < bbox_min->y)
          bbox_min->y = py;
        if (py > bbox_max->y)
          bbox_max->y = py;
      }
    }
  }
}

bool NcHmi::checkPathBounds()
{
  if (!m_app)
    return false;
  Point2d bbox_min, bbox_max;
  getBoundingBox(&bbox_min, &bbox_max);
  auto& control_view = m_app->getControlView();
  if (bbox_min.x >
        0.0f + control_view.m_machine_parameters.cutting_extents[0] &&
      bbox_min.y >
        0.0f + control_view.m_machine_parameters.cutting_extents[1] &&
      bbox_max.x < control_view.m_machine_parameters.machine_extents[0] +
                     control_view.m_machine_parameters.cutting_extents[2] &&
      bbox_max.y < control_view.m_machine_parameters.machine_extents[1] +
                     control_view.m_machine_parameters.cutting_extents[3]) {
    return true;
  }
  LOG_F(WARNING,
        "Code paths outside machine extents:\n  bbox{[%f, %f], [%f, %f]}",
        bbox_min.x,
        bbox_min.y,
        bbox_max.x,
        bbox_max.y);
  return false;
}

void NcHmi::handleButton(const std::string& id)
{
  if (!m_app)
    return;
  auto&          control_view = m_app->getControlView();
  const DROData& dro_data = control_view.m_motion_controller->getDRO();

  // Parse button ID string to enum for type-safe dispatch
  HmiButtonId button_id = parseButtonId(id);

  try {
    // Handle buttons that require machine to be stopped
    if (dro_data.in_motion == false) {
      switch (button_id) {
        case HmiButtonId::Wpos:
          LOG_F(INFO, "Clicked Wpos");
          control_view.m_motion_controller->pushGCode("G0 X0 Y0");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::Home: {
          LOG_F(INFO, "Clicked Home");
          if (isHomingAllowed()) {
            control_view.m_motion_controller->home();
          }
          else {
            m_app->getDialogs().setInfoValue(
              "Homing not available while machine is busy!");
          }
          break;
        }

        case HmiButtonId::Park:
          LOG_F(INFO, "Clicked Park");
          control_view.m_motion_controller->pushGCode("G53 G0 Z0");
          control_view.m_motion_controller->pushGCode("G53 G0 X0 Y0");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::ZeroX:
          LOG_F(INFO, "Clicked Zero X");
          control_view.m_machine_parameters.work_offset[0] = dro_data.mcs.x;
          control_view.m_motion_controller->pushGCode(
            "G10 L2 P0 X" +
            to_string_strip_zeros(
              control_view.m_machine_parameters.work_offset[0]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          control_view.m_motion_controller->saveParameters();
          break;

        case HmiButtonId::ZeroY:
          LOG_F(INFO, "Clicked Zero Y");
          control_view.m_machine_parameters.work_offset[1] = dro_data.mcs.y;
          control_view.m_motion_controller->pushGCode(
            "G10 L2 P0 Y" +
            to_string_strip_zeros(
              control_view.m_machine_parameters.work_offset[1]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          control_view.m_motion_controller->saveParameters();
          break;

        case HmiButtonId::SpindleOn:
          // Not used
          LOG_F(INFO, "Spindle On");
          control_view.m_motion_controller->pushGCode("M3 S1000");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::SpindleOff:
          // Not used
          LOG_F(INFO, "Spindle Off");
          control_view.m_motion_controller->pushGCode("M5");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::Retract:
          LOG_F(INFO, "Clicked Retract");
          control_view.m_motion_controller->pushGCode("G0 Z0");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::Touch:
          LOG_F(INFO, "Clicked Touch");
          control_view.m_motion_controller->pushGCode("touch_torch");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          break;

        case HmiButtonId::Run: {
          LOG_F(INFO, "Clicked Run");
          if (checkPathBounds()) {
            const auto& lines = control_view.getGCode().getLines();
            if (!lines.empty()) {
              for (const auto& line : lines) {
                control_view.m_motion_controller->pushGCode(line);
              }
              control_view.m_motion_controller->runStack();
            }
            else {
              m_app->getDialogs().setInfoValue("No G-code loaded!");
            }
          }
          else {
            m_app->getDialogs().setInfoValue(
              "Program is outside of machines cuttable extents!");
          }
        } break;

        case HmiButtonId::TestRun: {
          LOG_F(INFO, "Clicked Test Run");
          if (checkPathBounds()) {
            const auto& lines = control_view.getGCode().getLines();
            if (!lines.empty()) {
              for (const auto& line : lines) {
                if (line.find("fire_torch") != std::string::npos) {
                  std::string modified = line;
                  removeSubstrs(modified, "fire_torch");
                  control_view.m_motion_controller->pushGCode("touch_torch" +
                                                              modified);
                }
                else {
                  control_view.m_motion_controller->pushGCode(line);
                }
              }
              control_view.m_motion_controller->runStack();
            }
            else {
              m_app->getDialogs().setInfoValue("No G-code loaded!");
            }
          }
          else {
            m_app->getDialogs().setInfoValue(
              "Program is outside of machines cuttable extents!");
          }
        } break;

        default:
          // Other buttons are handled below
          break;
      }
    }

    // Handle buttons that can be pressed regardless of motion state
    switch (button_id) {
      case HmiButtonId::Abort:
        LOG_F(INFO, "Clicked Abort");
        control_view.m_motion_controller->abort();
        break;

      case HmiButtonId::Clean:
        LOG_F(INFO, "Clicked Clean");
        clearHighlights();
        break;

      case HmiButtonId::Fit: {
        LOG_F(INFO, "Clicked Fit");
        auto minmax = [](double a, double b) -> std::pair<double, double> {
          return a < b ? std::make_pair(a, b) : std::make_pair(b, a);
        };
        Point2d bbox_min, bbox_max;
        getBoundingBox(&bbox_min, &bbox_max);
        if (bbox_max.x == std::numeric_limits<int>::min() &&
            bbox_max.y == std::numeric_limits<int>::min() &&
            bbox_min.x == std::numeric_limits<int>::max() &&
            bbox_min.y == std::numeric_limits<int>::max()) {
          // Focus origin at center of view
          LOG_F(INFO, "No paths, fitting to machine extents!");
          m_view->setZoom(1);
          Point2d pan;
          pan.x =
            -((control_view.m_machine_parameters.machine_extents[0]) / 2.0);
          pan.y =
            -((control_view.m_machine_parameters.machine_extents[1]) / 2.0);
          m_view->setPan(pan);
          auto y_pair =
            minmax(m_app->getRenderer().getWindowSize().y,
                   control_view.m_machine_parameters.machine_extents[1]);
          auto x_pair =
            minmax(m_app->getRenderer().getWindowSize().x - m_panel_width,
                   control_view.m_machine_parameters.machine_extents[0]);
          if (y_pair.second - y_pair.first < x_pair.second - x_pair.first) {
            double machine_y =
              control_view.m_machine_parameters.machine_extents[1];
            double window_y = m_app->getRenderer().getWindowSize().y;
            double new_zoom = window_y / (window_y + machine_y);
            m_view->setZoom(new_zoom);
          }
          else // Fit X
          {
            double machine_x =
              control_view.m_machine_parameters.machine_extents[0];
            double window_x = m_app->getRenderer().getWindowSize().x;
            double new_zoom =
              (window_x - (m_panel_width / 2.0)) / (machine_x + window_x);
            m_view->setZoom(new_zoom);
          }
          // Update pan based on machine extents and zoom
          Point2d current_pan = m_view->getPan();
          double  current_zoom = m_view->getZoom();
          current_pan.x +=
            ((double) control_view.m_machine_parameters.machine_extents[0] /
             2.0) *
            current_zoom;
          current_pan.y +=
            ((double) control_view.m_machine_parameters.machine_extents[1] /
             2.0) *
            current_zoom;
          m_view->setPan(current_pan);
        }
        else {
          LOG_F(INFO,
                "Calculated bounding box: => bbox_min = (%.4f, %.4f) and "
                "bbox_max = (%.4f, %.4f)",
                bbox_min.x,
                bbox_min.y,
                bbox_max.x,
                bbox_max.y);
          const double x_diff = bbox_max.x - bbox_min.x;
          const double y_diff = bbox_max.y - bbox_min.y;
          m_view->setZoom(1.0);
          Point2d new_pan;
          new_pan.x = -bbox_min.x + (x_diff / 2);
          new_pan.y =
            -(bbox_min.y + (y_diff / 2) + 10); // 10 is for the menu bar
          m_view->setPan(new_pan);
          auto y_pair = minmax(m_app->getRenderer().getWindowSize().y, y_diff);
          auto x_pair = minmax(
            m_app->getRenderer().getWindowSize().x - m_panel_width, x_diff);
          if (y_pair.second - y_pair.first < x_pair.second - x_pair.first) {
            LOG_F(INFO, "Fitting to Y");
            double fit_zoom =
              ((double) m_app->getRenderer().getWindowSize().y) / (2 * y_diff);
            m_view->setZoom(fit_zoom);
          }
          else // Fit X
          {
            LOG_F(INFO, "Fitting to X");
            double fit_zoom = ((double) m_app->getRenderer().getWindowSize().x -
                               (m_panel_width / 2)) /
                              (2 * x_diff);
            m_view->setZoom(fit_zoom);
          }
          // Update pan based on zoom and offsets
          Point2d fit_pan = m_view->getPan();
          double  current_zoom = m_view->getZoom();
          fit_pan.x += (x_diff / 2.0) * current_zoom;
          fit_pan.y += (y_diff / 2.0) * current_zoom;
          fit_pan.x -=
            control_view.m_machine_parameters.work_offset[0] * current_zoom;
          fit_pan.y -=
            control_view.m_machine_parameters.work_offset[1] * current_zoom;
          m_view->setPan(fit_pan);
        }
      } break;

      default:
        // Unknown button ID - already logged in parseButtonId
        break;
    }
  }
  catch (...) {
    LOG_F(ERROR, "Error handling button event!");
  }
}

void NcHmi::jumpin(Primitive* p)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (checkPathBounds()) {
    const auto& lines = control_view.getGCode().getLines();
    if (!lines.empty()) {
      int           rapid_line = std::get<int>(p->user_data);
      unsigned long start =
        (rapid_line > 0) ? static_cast<unsigned long>(rapid_line) : 0;
      for (unsigned long i = start; i < lines.size(); i++) {
        control_view.m_motion_controller->pushGCode(lines[i]);
      }
      control_view.m_motion_controller->runStack();
    }
    else {
      m_app->getDialogs().setInfoValue("No G-code loaded!");
    }
  }
  else {
    m_app->getDialogs().setInfoValue(
      "Program is outside of machines cuttable extents!");
  }
}

void NcHmi::reverse(Primitive* p)
{
  if (!m_app)
    return;
  auto&       control_view = m_app->getControlView();
  const auto& source_lines = control_view.getGCode().getLines();
  if (source_lines.empty()) {
    m_app->getDialogs().setInfoValue("No G-code loaded!");
    return;
  }

  std::vector<std::string> gcode_lines_before_reverse;
  std::vector<std::string> gcode_lines_to_reverse;
  std::vector<std::string> gcode_lines_after_reverse;
  bool                     found_path_end = false;
  int                      rapid_line = std::get<int>(p->user_data);

  for (unsigned long count = 0; count < source_lines.size(); count++) {
    const auto& line = source_lines[count];
    if (count <= (unsigned long) rapid_line + 1) {
      gcode_lines_before_reverse.push_back(line);
    }
    else {
      if (line.find("torch_off") != std::string::npos) {
        found_path_end = true;
      }
      if (found_path_end) {
        gcode_lines_after_reverse.push_back(line);
      }
      else {
        gcode_lines_to_reverse.push_back(line);
      }
    }
  }

  // Build the new line set with the reversed section
  std::vector<std::string> new_lines;
  new_lines.reserve(source_lines.size());
  for (const auto& line : gcode_lines_before_reverse) {
    new_lines.push_back(line);
  }
  for (auto it = gcode_lines_to_reverse.rbegin();
       it != gcode_lines_to_reverse.rend();
       ++it) {
    new_lines.push_back(*it);
  }
  for (const auto& line : gcode_lines_after_reverse) {
    new_lines.push_back(line);
  }

  // Write back to file if there is a backing file
  std::string filename = control_view.getGCode().getFilename();
  if (!filename.empty()) {
    try {
      std::ofstream out(filename);
      for (const auto& line : new_lines) {
        out << line << std::endl;
      }
      out.close();
    }
    catch (...) {
      m_app->getDialogs().setInfoValue("Could not write gcode file to disk!");
      return;
    }
  }

  // Reload from the new lines
  clearHighlights();
  if (control_view.getGCode().loadFromLines(std::move(new_lines))) {
    auto& gcode = control_view.getGCode();
    m_app->getRenderer().pushTimer(0,
                                   [&gcode]() { return gcode.parseTimer(); });
  }
}

void NcHmi::mouseCallback(Primitive* c, const Primitive::MouseEventData& e)
{
  if (!m_app)
    return;
  auto&   control_view = m_app->getControlView();
  Point2d screen_pos = m_app->getInputState().getMousePosition();

  // Handle machine/cuttable plane clicks for waypoint functionality
  if ((hasFlag(c->flags, PrimitiveFlags::CuttablePlane) ||
       hasFlag(c->flags, PrimitiveFlags::MachinePlane)) &&
      std::holds_alternative<MouseButtonEvent>(e)) {
    const auto& button = std::get<MouseButtonEvent>(e);
    // Get mouse position in matrix coordinates using view transformation
    Point2d     p = m_view->screenToMatrix(screen_pos);
    const auto* cp = control_view.m_cuttable_plane;
    const auto  bl = cp->m_bottom_left;
    // double check point against cuttable plane in case it was
    // "machine_plane" that triggered this code path
    bool is_within = p.x >= bl.x && p.x <= bl.x + cp->m_width && p.y >= bl.y &&
                     p.y <= bl.y + cp->m_height;
    if (!is_within) {
      return;
    }
    if (button.button == GLFW_MOUSE_BUTTON_1 && button.action == GLFW_RELEASE) {
      const DROData& dro_data = control_view.m_motion_controller->getDRO();
      if (!dro_data.in_motion) {
        LOG_F(INFO, "Add waypoint position [%f, %f]", p.x, p.y);
        control_view.m_way_point_position = p;

        // Show waypoint on machine plane
        control_view.m_waypoint_pointer->m_center = p;
        control_view.m_waypoint_pointer->visible = true;

        // Show popup asking user if they want to go to this location
        // Position popup near the click location
        m_app->getDialogs().askYesNo(
          "Go to waypoint position?",
          [this]() { goToWaypoint(nullptr); },
          [&control_view]() {
            control_view.m_waypoint_pointer->visible = false;
          },
          screen_pos);
      }
    }
  }
}

void NcHmi::handleMouseMoveEvent(const MouseMoveEvent& e,
                                 const InputState&     input)
{
  mouseMotionCallback(e);
}

bool NcHmi::updateTimer()
{
  if (!m_app)
    return false;
  auto&          control_view = m_app->getControlView();
  const DROData& dro_data = control_view.m_motion_controller->getDRO();
  try {
    if (dro_data.status != MachineStatus::Unknown) {
      const float wcx = dro_data.wcs.x;
      const float wcy = dro_data.wcs.y;
      const float wcz = dro_data.wcs.z;
      m_dro.x.work_readout = to_fixed_string(fabs(wcx), 2);
      m_dro.y.work_readout = to_fixed_string(fabs(wcy), 2);
      m_dro.z.work_readout = to_fixed_string(fabs(wcz), 2);
      const float mcx = dro_data.mcs.x;
      const float mcy = dro_data.mcs.y;
      const float mcz = dro_data.mcs.z;
      m_dro.x.absolute_readout = to_fixed_string(fabs(mcx), 2);
      m_dro.y.absolute_readout = to_fixed_string(fabs(mcy), 2);
      m_dro.z.absolute_readout = to_fixed_string(fabs(mcz), 2);
      m_dro.feed = "FEED: " + to_fixed_string(dro_data.feed, 1);
      m_dro.arc_readout = "ARC: " + to_fixed_string(dro_data.voltage, 1) + "V";
      m_dro.arc_set =
        "SET: " +
        to_fixed_string(control_view.m_motion_controller->getThcEffective(), 1);
      {
        float ofs = control_view.m_motion_controller->getThcOffset();
        if (ofs >= 0.0f)
          m_thc_offset_readout = "+" + to_fixed_string(ofs, 1);
        else
          m_thc_offset_readout = "-" + to_fixed_string(ofs, 1);
      }
      RuntimeData runtime = control_view.m_motion_controller->getRunTime();
      if (runtime.hours != 0 || runtime.minutes != 0 || runtime.seconds != 0)
        m_dro.run_time = "RUN: " + to_string_strip_zeros((int) runtime.hours) +
                         ":" + to_string_strip_zeros((int) runtime.minutes) +
                         ":" + to_string_strip_zeros((int) runtime.seconds);
      control_view.m_torch_pointer->m_center = { fabs(mcx), fabs(mcy) };

      m_dro.torch_on = control_view.m_motion_controller->isTorchOn();
      m_dro.arc_ok = dro_data.arc_ok;

      if (!dro_data.arc_ok) {
        // Keep adding points to current highlight path
        // Negated because this is how gcode is interpreted to be consistent
        // with grbl's machine coordinates
        const Point2d wc_point{ -wcx, -wcy };
        if (not m_arc_okay_highlight_path) {
          std::vector<Point2d> path{ wc_point };
          m_arc_okay_highlight_path =
            m_app->getRenderer().pushPrimitive<Path>(path);
          m_arc_okay_highlight_path->id = "gcode_highlights";
          m_arc_okay_highlight_path->flags = PrimitiveFlags::GCodeHighlight;
          m_arc_okay_highlight_path->m_is_closed = false;
          m_arc_okay_highlight_path->m_width = 3;
          m_arc_okay_highlight_path->color =
            &m_app->getColor(ThemeColor::PlotLinesHovered);
          m_arc_okay_highlight_path->matrix_callback =
            m_view->getTransformCallback();
        }
        else {
          m_arc_okay_highlight_path->m_points.push_back(wc_point);
        }
      }
      else {
        m_arc_okay_highlight_path = nullptr;
      }
    }
  }
  catch (const nlohmann::json::out_of_range& e) {
    LOG_F(ERROR, "Failed to parse controller data: %s", e.what());
  }
  return true;
}

void NcHmi::resizeCallback(const WindowResizeEvent& e)
{
  // ImGui handles all UI panel layout automatically.
  // Nothing to do here - panels are positioned in
  // renderHmi/renderDro/renderThcWidget.
}

void NcHmi::handleWindowResizeEvent(const WindowResizeEvent& e,
                                    const InputState&        input)
{
  resizeCallback(e);
}

void NcHmi::clearHighlights()
{
  if (!m_app)
    return;
  m_app->getRenderer().deletePrimitivesById("gcode_highlights");
  m_arc_okay_highlight_path = nullptr;
}

// invalidateColors() removed - primitives now hold const Color4f* pointers
// into ThemeManager's stable color cache, which auto-updates on theme change.

void NcHmi::tabKeyUpCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (control_view.m_way_point_position.x != std::numeric_limits<int>::min() &&
      control_view.m_way_point_position.y != std::numeric_limits<int>::min()) {
    LOG_F(INFO,
          "Going to waypoint position: X%.4f Y%.4f",
          control_view.m_way_point_position.x,
          control_view.m_way_point_position.y);
    control_view.m_motion_controller->pushGCode(
      "G53 G0 X" + to_string_strip_zeros(control_view.m_way_point_position.x) +
      " Y" + to_string_strip_zeros(control_view.m_way_point_position.y));
    control_view.m_motion_controller->pushGCode("M30");
    control_view.m_motion_controller->runStack();
    control_view.m_way_point_position.x = std::numeric_limits<int>::min();
    control_view.m_way_point_position.y = std::numeric_limits<int>::min();
  }
}

bool NcHmi::isMotionAllowed() const
{
  if (!m_app)
    return false;
  const DROData& dro_data =
    m_app->getControlView().m_motion_controller->getDRO();
  return !dro_data.in_motion;
}

ButtonCategory NcHmi::getButtonCategory(HmiButtonId id) const
{
  switch (id) {
    case HmiButtonId::Abort:
      return ButtonCategory::AbortButton;
    case HmiButtonId::Clean:
    case HmiButtonId::Fit:
      return ButtonCategory::SafeButton;
    default:
      return ButtonCategory::MotionButton; // All others push GCode
  }
}

bool NcHmi::isHomingAllowed()
{
  if (!m_app)
    return false;
  auto&          control_view = m_app->getControlView();
  const DROData& dro_data = control_view.m_motion_controller->getDRO();
  return !dro_data.in_motion &&
         control_view.m_motion_controller->isHomingSafe();
}

void NcHmi::goToWaypoint(Primitive* args)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (control_view.m_way_point_position.x != std::numeric_limits<int>::min() &&
      control_view.m_way_point_position.y != std::numeric_limits<int>::min()) {
    LOG_F(INFO,
          "Going to waypoint position: X%.4f Y%.4f",
          control_view.m_way_point_position.x,
          control_view.m_way_point_position.y);
    control_view.m_motion_controller->pushGCode(
      "G53 G0 X-" + to_string_strip_zeros(control_view.m_way_point_position.x) +
      " Y-" + to_string_strip_zeros(control_view.m_way_point_position.y));
    control_view.m_motion_controller->pushGCode("M30");
    control_view.m_motion_controller->runStack();

    // Hide waypoint pointer after going to location
    control_view.m_waypoint_pointer->visible = false;

    // Clear waypoint position
    control_view.m_way_point_position.x = std::numeric_limits<int>::min();
    control_view.m_way_point_position.y = std::numeric_limits<int>::min();
  }
}
void NcHmi::escapeKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  handleButton("Abort");
  control_view.m_way_point_position = { std::numeric_limits<double>::min(),
                                        std::numeric_limits<double>::min() };
}
void NcHmi::upKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    const DROData& dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data.status == MachineStatus::Idle) {
      if (e.action == GLFW_PRESS) {
        if (e.mods & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging Y positive %s", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 Y-" + dist);
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
        else {
          // key down
          LOG_F(INFO, "Jogging Y positive Continuous!");
          control_view.m_motion_controller->pushGCode(
            "G53 G0 Y-" +
            std::to_string(
              control_view.m_machine_parameters.machine_extents[1]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
      }
    }
    if (e.action == GLFW_RELEASE && !(e.mods & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling Y positive jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}

void NcHmi::downKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    const DROData& dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data.status == MachineStatus::Idle) {
      if (e.action == GLFW_PRESS) {
        if (e.mods & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging Y negative %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 Y" + dist);
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
        else {
          // key down
          LOG_F(INFO, "Jogging Y negative Continuous!");
          control_view.m_motion_controller->pushGCode("G53 G0 Y0");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
      }
    }
    if (e.action == GLFW_RELEASE && !(e.mods & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling Y negative jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}

void NcHmi::rightKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    const DROData& dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data.status == MachineStatus::Idle) {
      if (e.action == GLFW_PRESS) {
        if (e.mods & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging X positive %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 X-" + dist);
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
        else {
          // key down
          LOG_F(INFO, "Jogging X Positive Continuous!");
          control_view.m_motion_controller->pushGCode(
            "G53 G0 X-" +
            std::to_string(
              control_view.m_machine_parameters.machine_extents[0]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
      }
    }
    if (e.action == GLFW_RELEASE && !(e.mods & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling X Positive jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}
void NcHmi::leftKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    const DROData& dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data.status == MachineStatus::Idle) {
      if (e.action == GLFW_PRESS) {
        if (e.mods & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging X negative %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 X" + dist);
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
        else {
          // key down
          LOG_F(INFO, "Jogging X Negative Continuous!");
          control_view.m_motion_controller->pushGCode("G53 G0 X0");
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
        }
      }
    }
    if (e.action == GLFW_RELEASE && not(e.mods & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling X Negative jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}
void NcHmi::pageUpKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (e.action == GLFW_PRESS) {
    if (control_view.m_machine_parameters.homing_dir_invert[2]) {
      control_view.m_motion_controller->sendRealTime('<');
    }
    else {
      control_view.m_motion_controller->sendRealTime('>');
    }
  }
  else if (e.action == GLFW_RELEASE) {
    control_view.m_motion_controller->sendRealTime('^');
  }
  /*try
  {
      const DROData& dro_data = control_view.m_motion_controller->getDRO();
      if (dro_data["STATUS"] == "IDLE")
      {
          if ((int)e["action"] == 1 || (int)e["action"] == 2)
          {
              if (left_control_key_is_pressed == true)
              {
                  LOG_F(INFO, "Jogging Z positive 0.010!");
                  control_view.m_motion_controller->pushGCode("G91 Z0.010");
                  control_view.m_motion_controller->pushGCode("M30");
                  control_view.m_motion_controller->runStack();
              }
              else
              {
                  //key down
                  LOG_F(INFO, "Jogging Z Positive Continuous!");
                  control_view.m_motion_controller->pushGCode("G53 G0 Z0");
                  control_view.m_motion_controller->pushGCode("M30");
                  control_view.m_motion_controller->runStack();
              }
          }
      }
      if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
      {
          //key up
          LOG_F(INFO, "Cancelling Z Positive jog!");
          hmi_handle_button("Abort");
      }
  }
  catch(...)
  {
      //DRO data not available
  }*/
}
void NcHmi::pageDownKeyCallback(const KeyEvent& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (e.action == GLFW_PRESS) {
    if (control_view.m_machine_parameters.homing_dir_invert[2]) {
      control_view.m_motion_controller->sendRealTime('>');
    }
    else {
      control_view.m_motion_controller->sendRealTime('<');
    }
  }
  else if (e.action == GLFW_RELEASE) {
    control_view.m_motion_controller->sendRealTime('^');
  }
  /*try
  {
      const DROData& dro_data = control_view.m_motion_controller->getDRO();
      if (dro_data["STATUS"] == "IDLE")
      {
          if ((int)e["action"] == 1 || (int)e["action"] == 2)
          {
              if (left_control_key_is_pressed == true)
              {
                  LOG_F(INFO, "Jogging Z negative 0.010!");
                  control_view.m_motion_controller->pushGCode("G91 Z-0.010");
                  control_view.m_motion_controller->pushGCode("M30");
                  control_view.m_motion_controller->runStack();
              }
              else
              {
                  //key down
                  LOG_F(INFO, "Jogging Z Negative Continuous!");
                  control_view.m_motion_controller->pushGCode("G53 G0 Z" +
  std::to_string(control_view.m_machine_parameters.machine_extents[2]));
                  control_view.m_motion_controller->pushGCode("M30");
                  control_view.m_motion_controller->runStack();
              }
          }
      }
      if ((int)e["action"] == 0 && left_control_key_is_pressed == false)
      {
          //key up
          LOG_F(INFO, "Cancelling Z Negative jog!");
          hmi_handle_button("Abort");
      }
  }
  catch(...)
  {
      //DRO data not available
  }*/
}
void NcHmi::mouseMotionCallback(const MouseMoveEvent& e)
{
  if (!m_app)
    return;

  Point2d point = { e.x, e.y };

  // Update global mouse position
  m_app->getInputState().setMousePosition(point);

  // Handle view-specific mouse movement (includes panning when move view is
  // active)
  m_view->handleMouseMove(point);

  double      zoom = m_view->getZoom();
  const auto& pan = m_view->getPan();
  Point2d     matrix_coords = { (e.x - pan.x) / zoom, (e.y - pan.y) / zoom };
  // Matrix coordinates are calculated on demand via view transformation
}

void NcHmi::init()
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();

  // Layout dimensions
  m_dro_backplane_height = m_app->getRenderer().scaleUI(65.f);
  m_panel_width = m_app->getRenderer().scaleUI(70.f);

  control_view.m_way_point_position = { std::numeric_limits<int>::min(),
                                        std::numeric_limits<int>::min() };

  // Setup timer for DRO updates
  m_app->getRenderer().pushTimer(100, [this]() { return updateTimer(); });

  // Initialize world-space primitives (machine/cuttable planes, pointers)
  control_view.m_machine_plane = m_app->getRenderer().pushPrimitive<Box>(
    Point2d{ 0, 0 },
    control_view.m_machine_parameters.machine_extents[0],
    control_view.m_machine_parameters.machine_extents[1],
    0);
  control_view.m_machine_plane->id = "machine_plane";
  control_view.m_machine_plane->flags = PrimitiveFlags::MachinePlane;
  control_view.m_machine_plane->zindex = -20;
  control_view.m_machine_plane->color =
    &m_app->getColor(ThemeColor::MachinePlaneColor);
  control_view.m_machine_plane->matrix_callback =
    m_view->getTransformCallback();
  control_view.m_machine_plane->mouse_callback =
    [this](Primitive* c, const Primitive::MouseEventData& e) {
      mouseCallback(c, e);
    };

  control_view.m_cuttable_plane = m_app->getRenderer().pushPrimitive<Box>(
    Point2d{ control_view.m_machine_parameters.cutting_extents[0],
             control_view.m_machine_parameters.cutting_extents[1] },
    (control_view.m_machine_parameters.machine_extents[0] +
     control_view.m_machine_parameters.cutting_extents[2]) -
      control_view.m_machine_parameters.cutting_extents[0],
    (control_view.m_machine_parameters.machine_extents[1] +
     control_view.m_machine_parameters.cutting_extents[3]) -
      control_view.m_machine_parameters.cutting_extents[1],
    0);
  control_view.m_cuttable_plane->id = "cuttable_plane";
  control_view.m_cuttable_plane->flags = PrimitiveFlags::CuttablePlane;
  control_view.m_cuttable_plane->zindex = -10;
  control_view.m_cuttable_plane->color =
    &m_app->getColor(ThemeColor::CuttablePlaneColor);
  control_view.m_cuttable_plane->matrix_callback =
    m_view->getTransformCallback();

  control_view.m_torch_pointer =
    m_app->getRenderer().pushPrimitive<Circle>(Point2d::infNeg(), 5);
  control_view.m_torch_pointer->zindex = 500;
  control_view.m_torch_pointer->id = "torch_pointer";
  control_view.m_torch_pointer->flags = PrimitiveFlags::TorchPointer;
  control_view.m_torch_pointer->color =
    &m_app->getColor(ThemeColor::PlotLinesHovered);
  control_view.m_torch_pointer->matrix_callback =
    m_view->getTransformCallback();

  control_view.m_waypoint_pointer =
    m_app->getRenderer().pushPrimitive<Circle>(Point2d::infNeg(), 5);
  control_view.m_waypoint_pointer->zindex = 501;
  control_view.m_waypoint_pointer->id = "waypoint_pointer";
  control_view.m_waypoint_pointer->flags = PrimitiveFlags::WaypointPointer;
  control_view.m_waypoint_pointer->color =
    &m_app->getColor(ThemeColor::DragDropTarget);
  control_view.m_waypoint_pointer->matrix_callback =
    m_view->getTransformCallback();
  control_view.m_waypoint_pointer->visible = false;

  handleButton("Fit");
}

// Typed event handlers - no JSON conversion needed
void NcHmi::handleKeyEvent(const KeyEvent& e, const InputState& input)
{
  // Route to appropriate handler based on key
  switch (e.key) {
    case GLFW_KEY_ESCAPE:
      escapeKeyCallback(e);
      break;
    case GLFW_KEY_UP:
      upKeyCallback(e);
      break;
    case GLFW_KEY_DOWN:
      downKeyCallback(e);
      break;
    case GLFW_KEY_LEFT:
      leftKeyCallback(e);
      break;
    case GLFW_KEY_RIGHT:
      rightKeyCallback(e);
      break;
    case GLFW_KEY_PAGE_UP:
      pageUpKeyCallback(e);
      break;
    case GLFW_KEY_PAGE_DOWN:
      pageDownKeyCallback(e);
      break;
    default:
      // Key not handled by HMI
      break;
  }
}
