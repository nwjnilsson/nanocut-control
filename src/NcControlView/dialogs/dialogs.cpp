#include "../../NcApp/NcApp.h"
#include "../motion_control/motion_controller.h"
#include "NcControlView/NcControlView.h"
#include <algorithm>
#include <imgui.h>
#include <loguru.hpp>

namespace dialogs {

void renderControllerOfflineWindow(NcControlView::ControllerDialogs& d)
{
  ImGui::Begin("Controller Offline",
               &d.offline_window->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text(
    "The motion controller is offline! Verify that USB connection is secure!");
  ImGui::End();
}

void renderControllerAlarmWindow(NcControlView::ControllerDialogs& d,
                                 NcApp*                            app)
{
  ImGui::Begin("Controller Alarm",
               &d.alarm_window->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("%s", d.alarm_text.c_str());
  if (ImGui::Button("Clear Alarm")) {
    LOG_F(WARNING, "User cleared alarm!");
    if (app) {
      app->getControlView().getMotionController().triggerReset();
    }
    d.alarm_window->hide();
  }
  ImGui::End();
}

void renderControllerHomingWindow(NcControlView::ControllerDialogs& d,
                                  NcApp*                            app)
{
  ImGui::Begin("Home Machine",
               &d.homing_window->visible,
               ImGuiWindowFlags_AlwaysAutoResize);
  ImGui::Text("Machine needs to be homed. Click on Home button when ready!");
  if (ImGui::Button("Home")) {
    LOG_F(WARNING, "User initiated homing cycle!");
    if (app) {
      app->getControlView().getMotionController().sendCommand("home");
    }
    d.homing_window->hide();
  }
  ImGui::End();
}

void renderMachineParameters(NcControlView::ControllerDialogs& d,
                             NcApp*                            app,
                             NcControlView*                    view)
{
  auto& temp_parameters = d.machine_params_temp;

  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImGui::GetIO().DisplaySize);

  ImGui::Begin("Machine Parameters",
               &d.machine_params_window->visible,
               ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                 ImGuiWindowFlags_NoCollapse);
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
        temp_parameters.arc_voltage_divider > MAX_ARC_VOLTAGE_DIVIDER) {
      app->getDialogs().setInfoValue(
        "The arc voltage divider is insane. Pick something reasonable.");
      skip_save = true;
    }
    for (int i = 0; i < 3; ++i) {
      if (temp_parameters.machine_extents[i] < 0.f) {
        app->getDialogs().setInfoValue("Machine extents must not be negative.");
        skip_save = true;
        break;
      }
    }

    if (not skip_save) {
      view->m_machine_parameters = temp_parameters;
      view->getMotionController().saveParameters();
      view->getMotionController().writeParametersToController();
      d.machine_params_window->hide();
    }
  }
  ImGui::SameLine();
  if (ImGui::Button("Cancel")) {
    d.machine_params_window->hide();
  }
  ImGui::End();
}

} // namespace dialogs
