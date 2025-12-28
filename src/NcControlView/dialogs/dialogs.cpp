#include "dialogs.h"
#include "../../application/NcApp.h"
#include "../gcode/gcode.h"
#include "../motion_control/motion_controller.h"
#include "../util.h"
#include "NcControlView/NcControlView.h"
#include <NcRender/gui/ImGuiFileDialog.h>
#include <algorithm>

// Constructor
NcDialogs::NcDialogs(NcApp* app, NcControlView* view) : m_app(app), m_view(view)
{
}

void NcDialogs::renderFileOpen()
{
  if (ImGuiFileDialog::Instance()->Display(
        "ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(600, 500))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      LOG_F(INFO,
            "File Path: %s, File Path Name: %s",
            filePath.c_str(),
            filePathName.c_str());
      std::ofstream out(m_app->getRenderer().getConfigDirectory() +
                        "last_gcode_open_path.conf");
      out << filePath;
      out << "/";
      out.close();
      m_view->getHmi().clearHighlights();
      auto& gcode = m_app->getControlView().getGCode();
      if (gcode.openFile(filePathName)) {
        m_app->getRenderer().pushTimer(
          0, [&gcode]() { return gcode.parseTimer(); });
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }
}

void NcDialogs::showPreferences(bool visible)
{
  m_preferences_window_handle->visible = visible;
}

void NcDialogs::renderPreferences()
{
  ImGui::Begin("Preferences",
               &m_preferences_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::ColorEdit3(
    "Background Color",
    m_app->getControlView().m_preferences.background_color.data());
  ImGui::ColorEdit3(
    "Machine Plane Color",
    m_app->getControlView().m_preferences.machine_plane_color.data());
  ImGui::ColorEdit3(
    "Cuttable Plane Color",
    m_app->getControlView().m_preferences.cuttable_plane_color.data());
  ImGui::InputInt2("Default Window Size",
                   m_app->getControlView().m_preferences.window_size.data());
  ImGui::SameLine();
  if (ImGui::Button("<= Current Size")) {
    m_app->getControlView().m_preferences.window_size[0] =
      (int) m_app->getRenderer().getWindowSize().x;
    m_app->getControlView().m_preferences.window_size[1] =
      (int) m_app->getRenderer().getWindowSize().y;
  }
  ImGui::Spacing();
  if (ImGui::Button("OK")) {
    m_app->getRenderer().setClearColor(
      m_app->getControlView().m_preferences.background_color[0] * 255.0f,
      m_app->getControlView().m_preferences.background_color[1] * 255.0f,
      m_app->getControlView().m_preferences.background_color[2] * 255.0f);

    m_app->getControlView().m_machine_plane->color.r =
      m_app->getControlView().m_preferences.machine_plane_color[0] * 255.0f;
    m_app->getControlView().m_machine_plane->color.g =
      m_app->getControlView().m_preferences.machine_plane_color[1] * 255.0f;
    m_app->getControlView().m_machine_plane->color.b =
      m_app->getControlView().m_preferences.machine_plane_color[2] * 255.0f;

    m_app->getControlView().m_cuttable_plane->color.r =
      m_app->getControlView().m_preferences.cuttable_plane_color[0] * 255.0f;
    m_app->getControlView().m_cuttable_plane->color.g =
      m_app->getControlView().m_preferences.cuttable_plane_color[1] * 255.0f;
    m_app->getControlView().m_cuttable_plane->color.b =
      m_app->getControlView().m_preferences.cuttable_plane_color[2] * 255.0f;

    // Write preferences to file
    nlohmann::json preferences;
    preferences["background_color"]["r"] =
      m_app->getControlView().m_preferences.background_color[0];
    preferences["background_color"]["g"] =
      m_app->getControlView().m_preferences.background_color[1];
    preferences["background_color"]["b"] =
      m_app->getControlView().m_preferences.background_color[2];

    preferences["machine_plane_color"]["r"] =
      m_app->getControlView().m_preferences.machine_plane_color[0];
    preferences["machine_plane_color"]["g"] =
      m_app->getControlView().m_preferences.machine_plane_color[1];
    preferences["machine_plane_color"]["b"] =
      m_app->getControlView().m_preferences.machine_plane_color[2];

    preferences["cuttable_plane_color"]["r"] =
      m_app->getControlView().m_preferences.cuttable_plane_color[0];
    preferences["cuttable_plane_color"]["g"] =
      m_app->getControlView().m_preferences.cuttable_plane_color[1];
    preferences["cuttable_plane_color"]["b"] =
      m_app->getControlView().m_preferences.cuttable_plane_color[2];

    preferences["window_width"] =
      m_app->getControlView().m_preferences.window_size[0];
    preferences["window_height"] =
      m_app->getControlView().m_preferences.window_size[1];

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

void NcDialogs::showMachineParameters(bool visible)
{
  m_machine_parameters_window_handle->visible = visible;
}

void NcDialogs::renderMachineParameters()
{
  static NcControlView::MachineParameters temp_parameters =
    m_view->m_machine_parameters;
  ImGui::Begin("Machine Parameters",
               &m_machine_parameters_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Separator();
  ImGui::Text(
    "Machine extents is the max distance each axis can travel freely. X0 is "
    "the X negative stop, Y0 is Y negative stop, and Z0 is Z positive stop!");
  ImGui::InputFloat3("Machine Extents (X, Y, Z).",
                     temp_parameters.machine_extents.data());

  ImGui::Separator();
  ImGui::Text("Cutting extents are used to prevent accidentally cutting onto "
              "machine frames or generally any area outside of where cutting "
              "should happen.\nX1,Y1 is bottom left hand corner and X2, Y2 is "
              "top right hand corner, values are incremented off of machine "
              "extents, i.e X2 and Y2 should be negative");
  ImGui::InputFloat4("Cutting Extents (X1, Y1, X2, Y2)",
                     temp_parameters.cutting_extents.data());
  ImGui::Separator();
  ImGui::Text("Scale is in steps per your desired units. E.G. To use machine "
              "in\nInches, set scales to steps per inch.");
  ImGui::InputFloat3("Axis Scale (X, Y, Z)", temp_parameters.axis_scale.data());
  ImGui::Text("Manual precision jogging (ctrl + [MOVE]) can be used for small "
              "movements. The distance can be adjusted below.");
  ImGui::InputFloat("Precision jogging distance (units)",
                    &temp_parameters.precise_jog_units);
  ImGui::Separator();
  ImGui::Checkbox("Invert X", &temp_parameters.axis_invert[0]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert Y1", &temp_parameters.axis_invert[1]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert Y2", &temp_parameters.axis_invert[2]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert Z", &temp_parameters.axis_invert[3]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert stepper enable bit",
                  &temp_parameters.invert_step_enable);
  ImGui::Separator();

  ImGui::Text(
    "Homing and Soft Limits. Homing must be enabled to use soft limits!");
  ImGui::Checkbox("Enable Soft Limits", &temp_parameters.soft_limits_enabled);
  ImGui::Checkbox("Enable Homing", &temp_parameters.homing_enabled);
  ImGui::Checkbox("Invert homing direction X",
                  &temp_parameters.homing_dir_invert[0]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert homing direction Y",
                  &temp_parameters.homing_dir_invert[1]);
  ImGui::SameLine();
  ImGui::Checkbox("Invert homing direction Z",
                  &temp_parameters.homing_dir_invert[2]);
  ImGui::Checkbox("Invert limit pins", &temp_parameters.invert_limit_pins);
  ImGui::Checkbox("Invert probe pin", &temp_parameters.invert_probe_pin);
  ImGui::InputFloat("Homing Feedrate", &temp_parameters.homing_feed);
  ImGui::InputFloat("Homing Seekrate", &temp_parameters.homing_seek);
  ImGui::InputFloat("Homing Debounce", &temp_parameters.homing_debounce);
  ImGui::InputFloat("Homing Pull-Off", &temp_parameters.homing_pull_off);

  ImGui::Separator();
  ImGui::Text("Each axis maximum allowable velocity in units per minute. E.g. "
              "inch/min or mm/min");
  ImGui::InputFloat3("Max Velocity (X, Y, Z)", temp_parameters.max_vel.data());
  ImGui::Separator();
  ImGui::Text(
    "Each axis maximum allowable acceleration in units per second squared");
  ImGui::InputFloat3("Max Acceleration (X, Y, Z)",
                     temp_parameters.max_accel.data());
  ImGui::InputFloat("Junction Deviation", &temp_parameters.junction_deviation);
  ImGui::Separator();
  ImGui::Text(
    "The distance the floating head moves off of it's gravity stop to where it "
    "closes the probe switch. Ohmic sensing should have 0.0000 value");
  ImGui::InputFloat("Floating Head Backlash",
                    &temp_parameters.floating_head_backlash);
  ImGui::Separator();
  ImGui::Text("Velocity in units per minute when probing the torch");
  ImGui::InputFloat("Z Probe Feed", &temp_parameters.z_probe_feedrate);
  ImGui::Separator();
  ImGui::Text("The amount of time after motion starts after a probing cycle to "
              "consider the arc stabilized. This will affect THC accuracy!");
  ImGui::InputFloat("Arc Stabilization Time (ms)",
                    &temp_parameters.arc_stabilization_time);
  ImGui::Separator();
  ImGui::Text("The calibrated arc voltage divider of your system. This affects "
              "the DRO and THC settings. 1 means raw arc voltage, 50 means a "
              "1:50 divider is used.");
  ImGui::InputFloat("Arc Voltage Divider",
                    &temp_parameters.arc_voltage_divider);

  ImGui::Spacing();
  if (ImGui::Button("Save and write to controller")) {
    bool skip_save = false;
    if (temp_parameters.arc_voltage_divider < 1.f or
        temp_parameters.arc_voltage_divider > 500.f) {
      setInfoValue(
        "The arc voltage divider is insane. Pick something reasonable.");
      showInfoWindow(true);
      skip_save = true;
    }
    for (int i = 0; i < 3; ++i) {
      if (temp_parameters.machine_extents[i] < 0.f) {
        setInfoValue("Machine extents must not be negative.");
        showInfoWindow(true);
        skip_save = true;
        break;
      }
    }
    // Might want to do other sanity checks here

    if (not skip_save) {
      if (m_app) {
        m_app->getControlView().m_machine_parameters = temp_parameters;
        m_app->getControlView().getMotionController().saveParameters();
        m_app->getControlView()
          .getMotionController()
          .writeParametersToController();
      }
      showMachineParameters(false);
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    showMachineParameters(false);
  }
  ImGui::End();
}

void NcDialogs::showProgressWindow(bool visible)
{
  m_progress_window_handle->visible = visible;
}

void NcDialogs::setProgressValue(float progress) { m_progress = progress; }

void NcDialogs::renderProgressWindow()
{
  ImGui::Begin("Progress",
               &m_progress_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::ProgressBar(m_progress, ImVec2(0.0f, 0.0f));
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::Text("Progress");
  ImGui::End();
}

void NcDialogs::showInfoWindow(bool visible)
{
  m_info_window_handle->visible = visible;
}

void NcDialogs::setInfoValue(const std::string& info)
{
  m_info = info;
  LOG_F(WARNING, "Info Window => %s", m_info.c_str());
  showInfoWindow(true);
}

void NcDialogs::renderInfoWindow()
{
  ImGui::Begin(
    "Info", &m_info_window_handle->visible, ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", m_info.c_str());
  if (ImGui::Button("Close")) {
    showInfoWindow(false);
  }
  ImGui::End();
}

void NcDialogs::showControllerOfflineWindow(bool visible)
{
  m_controller_offline_window_handle->visible = visible;
}

void NcDialogs::setControllerOfflineValue(const std::string& info)
{
  m_info = info;
  LOG_F(WARNING, "Controller Offline Window => %s", m_info.c_str());
  showControllerOfflineWindow(true);
}

void NcDialogs::renderControllerOfflineWindow()
{
  ImGui::Begin("Controller Offline",
               &m_controller_offline_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text(
    "The motion controller is offline! Verify that USB connection is secure!");
  ImGui::End();
}

void NcDialogs::showControllerAlarmWindow(bool visible)
{
  m_controller_alarm_window_handle->visible = visible;
}

void NcDialogs::setControllerAlarmValue(const std::string& alarm_text)
{
  m_controller_alarm_text = alarm_text;
}

void NcDialogs::renderControllerAlarmWindow()
{
  ImGui::Begin("Controller Alarm",
               &m_controller_alarm_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", m_controller_alarm_text.c_str());
  if (ImGui::Button("Clear Alarm")) {
    LOG_F(WARNING, "User cleared alarm!");
    if (m_app) {
      m_app->getControlView().getMotionController().triggerReset();
    }
    showControllerAlarmWindow(false);
  }
  ImGui::End();
}

bool NcDialogs::isControllerAlarmWindowVisible() const
{
  return m_controller_alarm_window_handle->visible;
}

void NcDialogs::showControllerHomingWindow(bool visible)
{
  m_controller_homing_window_handle->visible = visible;
}

void NcDialogs::renderControllerHomingWindow()
{
  ImGui::Begin("Home Machine",
               &m_controller_homing_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Machine needs to be homed. Click on Home button when ready!");
  if (ImGui::Button("Home")) {
    LOG_F(WARNING, "User initiated homing cycle!");
    if (m_app) {
      m_app->getControlView().getMotionController().sendCommand("home");
    }
    showControllerHomingWindow(false);
  }
  ImGui::End();
}

void NcDialogs::askYesNo(const std::string&    question,
                         std::function<void()> yes_callback,
                         std::function<void()> no_callback)
{
  m_ask_text = question;
  m_ask_window_yes_callback = yes_callback;
  m_ask_window_no_callback = no_callback;
  LOG_F(INFO, "Ask Window => %s", m_ask_text.c_str());
  m_ask_window_handle->visible = true;
}

void NcDialogs::renderAskWindow()
{
  ImGui::Begin("Question",
               &m_ask_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", m_ask_text.c_str());
  if (ImGui::Button("Yes")) {
    if (m_ask_window_yes_callback)
      m_ask_window_yes_callback();
    m_ask_window_handle->visible = false;
  }
  ImGui::SameLine();
  if (ImGui::Button("No")) {
    if (m_ask_window_no_callback)
      m_ask_window_no_callback();
    m_ask_window_handle->visible = false;
  }
  ImGui::End();
}

void NcDialogs::showThcWindow(bool visible)
{
  m_thc_window_handle->visible = visible;
}

void NcDialogs::renderThcWindow()
{
  ImGui::Begin("Torch Height Control",
               &m_thc_window_handle->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Separator();

  static float new_value =
    m_app->getControlView().m_machine_parameters.thc_set_value;
  ImGui::Text("New target is set on next touch-off.");
  ImGui::Separator();
  ImGui::InputFloat("Set Voltage", &new_value, 0.1f, 0.5f);
  new_value = std::clamp(new_value, 0.f, 200.f);

  if (ImGui::Button("Set Target")) {
    askYesNo(
      "Set target arc voltage to " + std::to_string(new_value) + "?", [this]() {
        m_app->getControlView().m_machine_parameters.thc_set_value = new_value;
        showThcWindow(false);
      });
  }
  ImGui::SameLine();
  if (ImGui::Button("Close")) {
    showThcWindow(false);
  }
  ImGui::End();
}

void NcDialogs::init()
{
  m_file_open_handle =
    m_app->getRenderer().pushGui(true, [this]() { renderFileOpen(); });
  m_preferences_window_handle =
    m_app->getRenderer().pushGui(false, [this]() { renderPreferences(); });
  m_machine_parameters_window_handle = m_app->getRenderer().pushGui(
    false, [this]() { renderMachineParameters(); });
  m_progress_window_handle =
    m_app->getRenderer().pushGui(false, [this]() { renderProgressWindow(); });
  m_info_window_handle =
    m_app->getRenderer().pushGui(false, [this]() { renderInfoWindow(); });
  m_controller_offline_window_handle = m_app->getRenderer().pushGui(
    false, [this]() { renderControllerOfflineWindow(); });
  m_controller_alarm_window_handle = m_app->getRenderer().pushGui(
    false, [this]() { renderControllerAlarmWindow(); });
  m_controller_homing_window_handle = m_app->getRenderer().pushGui(
    false, [this]() { renderControllerHomingWindow(); });
  m_ask_window_handle =
    m_app->getRenderer().pushGui(false, [this]() { renderAskWindow(); });
  m_thc_window_handle =
    m_app->getRenderer().pushGui(false, [this]() { renderThcWindow(); });
}
