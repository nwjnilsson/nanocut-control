#ifndef NC_CONTROL_VIEW_
#define NC_CONTROL_VIEW_

// Standard library includes
#include <array>
#include <functional>
#include <memory>

// System includes
#include <NcRender/NcRender.h>

// Project includes
#include <NanoCut.h>
#include <NcApp/View.h>

// Local includes
#include "dialogs/dialogs.h"
#include "gcode/gcode.h"
#include "hmi/hmi.h"
#include "motion_control/motion_controller.h"

// Forward declarations
class NcApp;
class InputState;
struct MouseButtonEvent;
struct KeyEvent;
struct ScrollEvent;
struct MouseMoveEvent;
struct WindowResizeEvent;

class NcControlView : public View {
private:
  // Menu bar rendering
  void renderUI();

public:
  struct Preferences {
    std::array<float, 3> background_color = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> machine_plane_color = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> cuttable_plane_color = { 0.0f, 0.0f, 0.0f };
    std::array<int, 2>   window_size = { 0, 0 };
  };
  struct MachineParameters {
    // GRBL
    float                homing_feed = 0.0f;
    float                homing_seek = 0.0f;
    float                homing_debounce = 0.0f;
    float                homing_pull_off = 0.0f;
    std::array<float, 3> machine_extents = {
      0.0f, 0.0f, 0.0f
    }; // Stored as positive values
    std::array<float, 4> cutting_extents = { 0.0f, 0.0f, 0.0f, 0.0f };
    std::array<float, 3> axis_scale = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> max_vel = { 0.0f, 0.0f, 0.0f };
    std::array<float, 3> max_accel = { 0.0f, 0.0f, 0.0f };
    float                junction_deviation = 0;
    float                z_probe_feedrate = 0;
    std::array<bool, 4>  axis_invert = { false, false, false, false };
    bool                 soft_limits_enabled = false;
    bool                 homing_enabled = false;
    std::array<bool, 3>  homing_dir_invert = { false, false, false };
    bool                 invert_step_enable{ false };
    bool                 invert_limit_pins{ false };
    bool                 invert_probe_pin{ false };

    // Non-GRBL
    std::array<float, 3> work_offset = { 0.0f, 0.0f, 0.0f };
    float                arc_stabilization_time = 0;
    float                arc_voltage_divider = 0;
    float                floating_head_backlash = 0;
    // Not Included in machine_parameters.json
    float thc_set_value{ 0.f };
    float precise_jog_units = 0.0f;
  };

  // JSON serialization functions for MachineParameters
  static void to_json(nlohmann::json& j, const MachineParameters& p)
  {
    j["work_offset"]["x"] = p.work_offset[0];
    j["work_offset"]["y"] = p.work_offset[1];
    j["work_offset"]["z"] = p.work_offset[2];
    j["machine_extents"]["x"] = p.machine_extents[0];
    j["machine_extents"]["y"] = p.machine_extents[1];
    j["machine_extents"]["z"] = p.machine_extents[2];
    j["cutting_extents"]["x1"] = p.cutting_extents[0];
    j["cutting_extents"]["y1"] = p.cutting_extents[1];
    j["cutting_extents"]["x2"] = p.cutting_extents[2];
    j["cutting_extents"]["y2"] = p.cutting_extents[3];
    j["axis_scale"]["x"] = p.axis_scale[0];
    j["axis_scale"]["y"] = p.axis_scale[1];
    j["axis_scale"]["z"] = p.axis_scale[2];
    j["max_vel"]["x"] = p.max_vel[0];
    j["max_vel"]["y"] = p.max_vel[1];
    j["max_vel"]["z"] = p.max_vel[2];
    j["max_accel"]["x"] = p.max_accel[0];
    j["max_accel"]["y"] = p.max_accel[1];
    j["max_accel"]["z"] = p.max_accel[2];
    j["junction_deviation"] = p.junction_deviation;
    j["arc_stabilization_time"] = p.arc_stabilization_time;
    j["arc_voltage_divider"] = p.arc_voltage_divider;
    j["floating_head_backlash"] = p.floating_head_backlash;
    j["z_probe_feedrate"] = p.z_probe_feedrate;
    j["axis_invert"]["x"] = p.axis_invert[0];
    j["axis_invert"]["y1"] = p.axis_invert[1];
    j["axis_invert"]["y2"] = p.axis_invert[2];
    j["axis_invert"]["z"] = p.axis_invert[3];
    j["soft_limits_enabled"] = p.soft_limits_enabled;
    j["homing_enabled"] = p.homing_enabled;
    j["homing_dir_invert"]["x"] = p.homing_dir_invert[0];
    j["homing_dir_invert"]["y"] = p.homing_dir_invert[1];
    j["homing_dir_invert"]["z"] = p.homing_dir_invert[2];
    j["homing_feed"] = p.homing_feed;
    j["homing_seek"] = p.homing_seek;
    j["homing_debounce"] = p.homing_debounce;
    j["homing_pull_off"] = p.homing_pull_off;
    j["invert_limit_pins"] = p.invert_limit_pins;
    j["invert_probe_pin"] = p.invert_probe_pin;
    j["invert_step_enable"] = p.invert_step_enable;
    j["precise_jog_units"] = p.precise_jog_units;
  }

  static void from_json(const nlohmann::json& j, MachineParameters& p)
  {
    p.work_offset[0] = j.at("work_offset").at("x").get<float>();
    p.work_offset[1] = j.at("work_offset").at("y").get<float>();
    p.work_offset[2] = j.at("work_offset").at("z").get<float>();
    p.machine_extents[0] = j.at("machine_extents").at("x").get<float>();
    p.machine_extents[1] = j.at("machine_extents").at("y").get<float>();
    p.machine_extents[2] = j.at("machine_extents").at("z").get<float>();
    p.cutting_extents[0] = j.at("cutting_extents").at("x1").get<float>();
    p.cutting_extents[1] = j.at("cutting_extents").at("y1").get<float>();
    p.cutting_extents[2] = j.at("cutting_extents").at("x2").get<float>();
    p.cutting_extents[3] = j.at("cutting_extents").at("y2").get<float>();
    p.axis_scale[0] = j.at("axis_scale").at("x").get<float>();
    p.axis_scale[1] = j.at("axis_scale").at("y").get<float>();
    p.axis_scale[2] = j.at("axis_scale").at("z").get<float>();
    p.max_vel[0] = j.at("max_vel").at("x").get<float>();
    p.max_vel[1] = j.at("max_vel").at("y").get<float>();
    p.max_vel[2] = j.at("max_vel").at("z").get<float>();
    p.max_accel[0] = j.at("max_accel").at("x").get<float>();
    p.max_accel[1] = j.at("max_accel").at("y").get<float>();
    p.max_accel[2] = j.at("max_accel").at("z").get<float>();
    p.junction_deviation = j.at("junction_deviation").get<float>();
    p.arc_stabilization_time = j.at("arc_stabilization_time").get<float>();
    p.arc_voltage_divider = j.at("arc_voltage_divider").get<float>();
    p.floating_head_backlash = j.at("floating_head_backlash").get<float>();
    p.z_probe_feedrate = j.at("z_probe_feedrate").get<float>();
    p.axis_invert[0] = j.at("axis_invert").at("x").get<bool>();
    p.axis_invert[1] = j.at("axis_invert").at("y1").get<bool>();
    p.axis_invert[2] = j.at("axis_invert").at("y2").get<bool>();
    p.axis_invert[3] = j.at("axis_invert").at("z").get<bool>();
    p.soft_limits_enabled = j.at("soft_limits_enabled").get<bool>();
    p.homing_enabled = j.at("homing_enabled").get<bool>();
    p.homing_dir_invert[0] = j.at("homing_dir_invert").at("x").get<bool>();
    p.homing_dir_invert[1] = j.at("homing_dir_invert").at("y").get<bool>();
    p.homing_dir_invert[2] = j.at("homing_dir_invert").at("z").get<bool>();
    p.homing_feed = j.at("homing_feed").get<float>();
    p.homing_seek = j.at("homing_seek").get<float>();
    p.homing_debounce = j.at("homing_debounce").get<float>();
    p.homing_pull_off = j.at("homing_pull_off").get<float>();
    p.invert_limit_pins = j.at("invert_limit_pins").get<bool>();
    p.invert_probe_pin = j.at("invert_probe_pin").get<bool>();
    p.invert_step_enable = j.at("invert_step_enable").get<bool>();
    p.precise_jog_units = j.at("precise_jog_units").get<float>();
  }

  Preferences       m_preferences;
  Box*              m_machine_plane;
  Box*              m_cuttable_plane;
  Circle*           m_torch_pointer;
  Circle*           m_waypoint_pointer;
  MachineParameters m_machine_parameters;
  Point2d           m_way_point_position;

  // UI elements
  NcRender::NcRenderGui* m_menu_bar = nullptr;

  // Motion control system (public for compatibility wrapper access)
  std::unique_ptr<MotionController> m_motion_controller;

  // HMI (Human-Machine Interface)
  std::unique_ptr<NcHmi> m_hmi;

  // G-code parser and renderer
  std::unique_ptr<GCode> m_gcode;

  // Dialog window manager
  std::unique_ptr<NcDialogs> m_dialogs;

  // Application context dependency (injected)
  NcApp* m_app{ nullptr };

  // Default constructor for backward compatibility
  explicit NcControlView(NcApp* app)
    : View(app), m_machine_plane(nullptr), m_cuttable_plane(nullptr),
      m_torch_pointer(nullptr), m_waypoint_pointer(nullptr),
      m_way_point_position{}, m_motion_controller(nullptr), m_hmi(nullptr),
      m_app(app) {
        // Dependency injection constructor
      };

  // Move semantics (non-copyable due to resource management)
  NcControlView(const NcControlView&) = delete;
  NcControlView& operator=(const NcControlView&) = delete;
  NcControlView(NcControlView&&) = default;
  NcControlView& operator=(NcControlView&&) = default;
  void           preInit();
  void           init();
  void           tick() override;
  void           makeActive();
  void           close();
  void           loadGCodeFromLines(std::vector<std::string>&& lines);

  void handleMouseEvent(const MouseButtonEvent& e,
                        const InputState&       input) override;
  void handleKeyEvent(const KeyEvent& e, const InputState& input) override;
  void handleScrollEvent(const ScrollEvent& e,
                         const InputState&  input) override;
  void handleMouseMoveEvent(const MouseMoveEvent& e,
                            const InputState&     input) override;
  void handleWindowResize(const WindowResizeEvent& e,
                          const InputState&        input) override;

  // Override to provide machine work coordinate offset for transformations
  Point2d getWorkOffset() const override
  {
    return { m_machine_parameters.work_offset[0],
             m_machine_parameters.work_offset[1] };
  }

  // Motion controller access
  MotionController& getMotionController() { return *m_motion_controller; }

  // HMI access
  NcHmi& getHmi() { return *m_hmi; }

  // GCode access
  GCode& getGCode() { return *m_gcode; }

  // Dialogs access
  NcDialogs& getDialogs() { return *m_dialogs; }
};

#endif
