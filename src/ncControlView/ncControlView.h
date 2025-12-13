#ifndef NC_CONTROL_VIEW_
#define NC_CONTROL_VIEW_

#include <EasyRender/EasyRender.h>
#include <application.h>

class ncControlView {
private:
  static void zoom_event_handle(const nlohmann::json& e);
  static void click_and_drag_event_handle(const nlohmann::json& e);
  static void key_callback(const nlohmann::json& e);
  static void mod_key_callback(const nlohmann::json& e);


public:
  int mods; // TODO: get rid of this
  struct preferences_data_t {
    float background_color[3] = { 0.0f, 0.0f, 0.0f };
    float machine_plane_color[3] = { 0.0f, 0.0f, 0.0f };
    float cuttable_plane_color[3] = { 0.0f, 0.0f, 0.0f };
    int   window_size[2] = { 0, 0 };
  };
  struct machine_parameters_data_t {
    // GRBL
    float homing_feed = 0.0f;
    float homing_seek = 0.0f;
    float homing_debounce = 0.0f;
    float homing_pull_off = 0.0f;
    float machine_extents[3] = { 0.0f, 0.0f, 0.0f }; // Stored as positive
                                                     // values
    float cutting_extents[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    float axis_scale[3] = { 0.0f, 0.0f, 0.0f };
    float max_vel[3] = { 0.0f, 0.0f, 0.0f };
    float max_accel[3] = { 0.0f, 0.0f, 0.0f };
    float junction_deviation = 0;
    float z_probe_feedrate = 0;
    bool  axis_invert[4] = { false, false, false, false };
    bool  soft_limits_enabled = false;
    bool  homing_enabled = false;
    bool  homing_dir_invert[3] = { false, false, false };
    bool  invert_step_enable{ false };
    bool  invert_limit_pins{ false };
    bool  invert_probe_pin{ false };

    // Non-GRBL
    float work_offset[3] = { 0.0f, 0.0f, 0.0f };
    float arc_stabilization_time = 0;
    float arc_voltage_divider = 0;
    float floating_head_backlash = 0;
    // Not Included in machine_parameters.json
    float thc_set_value{ 0.f };
    float precise_jog_units = 0.0f;
  };
  preferences_data_t        preferences;
  EasyPrimitive::Box*       machine_plane;
  EasyPrimitive::Box*       cuttable_plane;
  EasyPrimitive::Circle*    torch_pointer;
  EasyPrimitive::Circle*    waypoint_pointer;
  machine_parameters_data_t machine_parameters;
  double_point_t            way_point_position;
  void (*view_matrix)(PrimitiveContainer* p);
  ncControlView() {
    // Do stuff here
  };
  void PreInit();
  void Init();
  void Tick();
  void MakeActive();
  void Close();
};

#endif
