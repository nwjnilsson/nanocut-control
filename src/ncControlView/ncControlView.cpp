#include "ncControlView.h"
#include "dialogs/dialogs.h"
#include "hmi/hmi.h"
#include "jetCamView/jetCamView.h"
#include "menu_bar/menu_bar.h"
#include "motion_control/motion_control.h"
#include <EasyRender/EasyRender.h>
#include <EasyRender/gui/imgui.h>
#include <EasyRender/logging/loguru.h>

void ncControlView::zoom_event_handle(const nlohmann::json& e)
{
  // LOG_F(INFO, "%s", e.dump().c_str());
  double_point_t matrix_mouse = globals->renderer->GetWindowMousePosition();
  matrix_mouse.x = (matrix_mouse.x - globals->pan.x) / globals->zoom;
  matrix_mouse.y = (matrix_mouse.y - globals->pan.y) / globals->zoom;
  if ((float) e["scroll"] > 0) {
    double old_zoom = globals->zoom;
    globals->zoom += globals->zoom * 0.125;
    if (globals->zoom > 1000000) {
      globals->zoom = 1000000;
    }
    double scalechange = old_zoom - globals->zoom;
    globals->pan.x += matrix_mouse.x * scalechange;
    globals->pan.y += matrix_mouse.y * scalechange;
  }
  else {
    double old_zoom = globals->zoom;
    globals->zoom += globals->zoom * -0.125;
    if (globals->zoom < 0.00001) {
      globals->zoom = 0.00001;
    }
    double scalechange = old_zoom - globals->zoom;
    globals->pan.x += matrix_mouse.x * scalechange;
    globals->pan.y += matrix_mouse.y * scalechange;
  }
}

void ncControlView::key_callback(const nlohmann::json& e)
{
  if ((int) e["key"] == GLFW_KEY_TAB and
      (int) e["action"] & EasyRender::ActionFlagBits::Press) {
    globals->jet_cam_view->MakeActive();
  }
}

void ncControlView::click_and_drag_event_handle(const nlohmann::json& e)
{
  if ((int) e["action"] & EasyRender::ActionFlagBits::Press)
    globals->move_view = true;
  else if ((int) e["action"] & EasyRender::ActionFlagBits::Release)
    globals->move_view = false;
}

void ncControlView::PreInit()
{
  nlohmann::json preferences = globals->renderer->ParseJsonFromFile(
    globals->renderer->GetConfigDirectory() + "preferences.json");
  if (preferences != NULL) {
    try {
      LOG_F(INFO,
            "Found %s!",
            std::string(globals->renderer->GetConfigDirectory() +
                        "preferences.json")
              .c_str());
      this->preferences.background_color[0] =
        (double) preferences["background_color"]["r"];
      this->preferences.background_color[1] =
        (double) preferences["background_color"]["g"];
      this->preferences.background_color[2] =
        (double) preferences["background_color"]["b"];
      this->preferences.machine_plane_color[0] =
        (double) preferences["machine_plane_color"]["r"];
      this->preferences.machine_plane_color[1] =
        (double) preferences["machine_plane_color"]["g"];
      this->preferences.machine_plane_color[2] =
        (double) preferences["machine_plane_color"]["b"];
      this->preferences.cuttable_plane_color[0] =
        (double) preferences["cuttable_plane_color"]["r"];
      this->preferences.cuttable_plane_color[1] =
        (double) preferences["cuttable_plane_color"]["g"];
      this->preferences.cuttable_plane_color[2] =
        (double) preferences["cuttable_plane_color"]["b"];
      this->preferences.window_size[0] = (double) preferences["window_width"];
      this->preferences.window_size[1] = (double) preferences["window_height"];
    }
    catch (...) {
      LOG_F(WARNING, "Error parsing preferences file!");
    }
  }
  else {
    LOG_F(WARNING, "Preferences file does not exist, creating it!");
    this->preferences.background_color[0] = 0.f;
    this->preferences.background_color[1] = 0.f;
    this->preferences.background_color[2] = 0.f;
    this->preferences.machine_plane_color[0] = 100.0f / 255.0f;
    this->preferences.machine_plane_color[1] = 100.0f / 255.0f;
    this->preferences.machine_plane_color[2] = 100.0f / 255.0f;
    this->preferences.cuttable_plane_color[0] = 151.0f / 255.0f;
    this->preferences.cuttable_plane_color[1] = 5.0f / 255.0f;
    this->preferences.cuttable_plane_color[2] = 5.0f / 255.0f;
    this->preferences.window_size[0] = 1280;
    this->preferences.window_size[1] = 720;
  }

  nlohmann::json parameters = globals->renderer->ParseJsonFromFile(
    globals->renderer->GetConfigDirectory() + "machine_parameters.json");
  if (parameters != NULL) {
    try {
      LOG_F(INFO,
            "Found %s!",
            std::string(globals->renderer->GetConfigDirectory() +
                        "machine_parameters.json")
              .c_str());
      this->machine_parameters.work_offset[0] =
        (float) parameters["work_offset"]["x"];
      this->machine_parameters.work_offset[1] =
        (float) parameters["work_offset"]["y"];
      this->machine_parameters.work_offset[2] =
        (float) parameters["work_offset"]["z"];
      this->machine_parameters.machine_extents[0] =
        (float) parameters["machine_extents"]["x"];
      this->machine_parameters.machine_extents[1] =
        (float) parameters["machine_extents"]["y"];
      this->machine_parameters.machine_extents[2] =
        (float) parameters["machine_extents"]["z"];
      this->machine_parameters.cutting_extents[0] =
        (float) parameters["cutting_extents"]["x1"];
      this->machine_parameters.cutting_extents[1] =
        (float) parameters["cutting_extents"]["y1"];
      this->machine_parameters.cutting_extents[2] =
        (float) parameters["cutting_extents"]["x2"];
      this->machine_parameters.cutting_extents[3] =
        (float) parameters["cutting_extents"]["y2"];
      this->machine_parameters.axis_scale[0] =
        (float) parameters["axis_scale"]["x"];
      this->machine_parameters.axis_scale[1] =
        (float) parameters["axis_scale"]["y"];
      this->machine_parameters.axis_scale[2] =
        (float) parameters["axis_scale"]["z"];
      this->machine_parameters.max_vel[0] = (float) parameters["max_vel"]["x"];
      this->machine_parameters.max_vel[1] = (float) parameters["max_vel"]["y"];
      this->machine_parameters.max_vel[2] = (float) parameters["max_vel"]["z"];
      this->machine_parameters.max_accel[0] =
        (float) parameters["max_accel"]["x"];
      this->machine_parameters.max_accel[1] =
        (float) parameters["max_accel"]["y"];
      this->machine_parameters.max_accel[2] =
        (float) parameters["max_accel"]["z"];
      this->machine_parameters.junction_deviation =
        (float) parameters["junction_deviation"];
      this->machine_parameters.arc_stabilization_time =
        (float) parameters["arc_stabilization_time"];
      this->machine_parameters.arc_voltage_divider =
        (float) parameters["arc_voltage_divider"];
      this->machine_parameters.floating_head_backlash =
        (float) parameters["floating_head_backlash"];
      this->machine_parameters.z_probe_feedrate =
        (float) parameters["z_probe_feedrate"];
      this->machine_parameters.axis_invert[0] =
        (bool) parameters["axis_invert"]["x"];
      this->machine_parameters.axis_invert[1] =
        (bool) parameters["axis_invert"]["y1"];
      this->machine_parameters.axis_invert[2] =
        (bool) parameters["axis_invert"]["y2"];
      this->machine_parameters.axis_invert[3] =
        (bool) parameters["axis_invert"]["z"];
      this->machine_parameters.soft_limits_enabled =
        (bool) parameters["soft_limits_enabled"];
      this->machine_parameters.homing_enabled =
        (bool) parameters["homing_enabled"];
      this->machine_parameters.homing_dir_invert[0] =
        (bool) parameters["homing_dir_invert"]["x"];
      this->machine_parameters.homing_dir_invert[1] =
        (bool) parameters["homing_dir_invert"]["y"];
      this->machine_parameters.homing_dir_invert[2] =
        (bool) parameters["homing_dir_invert"]["z"];
      this->machine_parameters.homing_feed = (float) parameters["homing_feed"];
      this->machine_parameters.homing_seek = (float) parameters["homing_seek"];
      this->machine_parameters.homing_debounce =
        (float) parameters["homing_debounce"];
      this->machine_parameters.homing_pull_off =
        (float) parameters["homing_pull_off"];
      this->machine_parameters.invert_limit_pins =
        (bool) parameters["invert_limit_pins"];
      this->machine_parameters.invert_probe_pin =
        (bool) parameters["invert_probe_pin"];
      this->machine_parameters.invert_step_enable =
        (bool) parameters["invert_step_enable"];
      this->machine_parameters.precise_jog_units =
        (float) parameters["precise_jog_units"];
    }
    catch (std::exception& e) {
      LOG_F(FATAL,
            "Error parsing Machine Parameters file! Reason:\n%s\nAborting to "
            "avoid damaging. Fix your machine_parameters.json or delete it to "
            "generate a new one.",
            e.what());
    }
  }
  else {
    LOG_F(WARNING,
          "%s does not exist, using default parameters!",
          std::string(globals->renderer->GetConfigDirectory() +
                      "machine_parameters.json")
            .c_str());
    this->machine_parameters.work_offset[0] = 0.0f;
    this->machine_parameters.work_offset[1] = 0.0f;
    this->machine_parameters.work_offset[2] = 0.0f;
    this->machine_parameters.machine_extents[0] = SCALE(1400.f);
    this->machine_parameters.machine_extents[1] = SCALE(1400.f);
    this->machine_parameters.machine_extents[2] = SCALE(-60.f);
    this->machine_parameters.cutting_extents[0] = SCALE(50.f);
    this->machine_parameters.cutting_extents[1] = SCALE(50.f);
    this->machine_parameters.cutting_extents[2] = -SCALE(50.f);
    this->machine_parameters.cutting_extents[3] = -SCALE(50.f);
    this->machine_parameters.axis_scale[0] = 200 * (0.25f / DEFAULT_UNIT);
    this->machine_parameters.axis_scale[1] = 200 * (0.25f / DEFAULT_UNIT);
    this->machine_parameters.axis_scale[2] = 200 * (0.25f / DEFAULT_UNIT);
    this->machine_parameters.max_vel[0] = SCALE(1000.f);
    this->machine_parameters.max_vel[1] = SCALE(1000.f);
    this->machine_parameters.max_vel[2] = SCALE(250.f);
    this->machine_parameters.max_accel[0] = SCALE(800.0f);
    this->machine_parameters.max_accel[1] = SCALE(800.0f);
    this->machine_parameters.max_accel[2] = SCALE(200.0f);
    this->machine_parameters.junction_deviation = SCALE(0.005f);
    this->machine_parameters.arc_stabilization_time = 2000;
    this->machine_parameters.arc_voltage_divider = 50.f;
    this->machine_parameters.floating_head_backlash = SCALE(10.f);
    this->machine_parameters.z_probe_feedrate = SCALE(100.f);
    this->machine_parameters.axis_invert[0] = true;
    this->machine_parameters.axis_invert[1] = true;
    this->machine_parameters.axis_invert[2] = true;
    this->machine_parameters.axis_invert[3] = true;
    this->machine_parameters.soft_limits_enabled = false;
    this->machine_parameters.homing_enabled = false;
    this->machine_parameters.homing_dir_invert[0] = false;
    this->machine_parameters.homing_dir_invert[1] = false;
    this->machine_parameters.homing_dir_invert[2] = false;
    this->machine_parameters.homing_feed = SCALE(400.f);
    this->machine_parameters.homing_seek = SCALE(800.f);
    this->machine_parameters.homing_debounce = SCALE(250.f);
    this->machine_parameters.homing_pull_off = SCALE(10.f);
    this->machine_parameters.invert_limit_pins = false;
    this->machine_parameters.invert_probe_pin = false;
    this->machine_parameters.invert_step_enable = false;
    this->machine_parameters.precise_jog_units = SCALE(5.f);
  }
  this->view_matrix = &hmi_view_matrix;
}

void ncControlView::mod_key_callback(const nlohmann::json& e)
{
  LOG_F(INFO, "%s", e.dump().c_str());
  using Action = EasyRender::ActionFlagBits;
  auto& mods = globals->nc_control_view->mods;
  auto  set_mod = [&](int bit) {
    if (e.at("action").get<int>() & Action::Press) {
      mods |= bit;
    }
    else {
      mods &= ~bit;
    }
  };
  if (e.at("key").get<int>() == GLFW_KEY_LEFT_CONTROL) {
    set_mod(GLFW_MOD_CONTROL);
  }
  if (e.at("key").get<int>() == GLFW_KEY_LEFT_SHIFT) {
    set_mod(GLFW_MOD_SHIFT);
  }
}
void ncControlView::Init()
{
  using Action = EasyRender::ActionFlagBits;
  globals->renderer->SetCurrentView("ncControlView");
  globals->renderer->PushEvent(
    GLFW_KEY_UNKNOWN, EasyRender::EventType::Scroll, &this->zoom_event_handle);
  globals->renderer->PushEvent(GLFW_MOUSE_BUTTON_2,
                               Action::Release | Action::Press,
                               &this->click_and_drag_event_handle);
  globals->renderer->PushEvent(GLFW_KEY_LEFT_CONTROL,
                               Action::Press | Action::Release,
                               &this->mod_key_callback);
  globals->renderer->PushEvent(
    GLFW_KEY_TAB, +EasyRender::ActionFlagBits::Press, &this->key_callback);
  menu_bar_init();
  dialogs_init();
  motion_control_init();
  hmi_init();
  globals->move_view = false;
}
void ncControlView::Tick() { motion_control_tick(); }
void ncControlView::MakeActive()
{
  globals->renderer->SetCurrentView("ncControlView");
  globals->renderer->SetClearColor(
    this->preferences.background_color[0] * 255.0f,
    this->preferences.background_color[1] * 255.0f,
    this->preferences.background_color[2] * 255.0f);
}
void ncControlView::Close() {}
