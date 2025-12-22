#ifndef NC_CONTROL_VIEW_
#define NC_CONTROL_VIEW_

// Standard library includes
#include <array>
#include <functional>
#include <memory>

// System includes
#include <NcRender/NcRender.h>

// Project includes
#include <application.h>
#include <application/View.h>

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
  Preferences                     m_preferences;
  Box*                            m_machine_plane;
  Box*                            m_cuttable_plane;
  Circle*                         m_torch_pointer;
  Circle*                         m_waypoint_pointer;
  MachineParameters               m_machine_parameters;
  Point2d                         m_way_point_position;

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
      m_way_point_position{}, m_motion_controller(nullptr),
      m_hmi(nullptr), m_app(app) {
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
