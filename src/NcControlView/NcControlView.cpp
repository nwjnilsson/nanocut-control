#include "NcControlView.h"
#include "../Input/InputState.h"
#include "../NcApp/NcApp.h"
#include "NcCamView/NcCamView.h"
#include "gcode/gcode.h"
#include "hmi/hmi.h"
#include <ImGuiFileDialog.h>
#include <NcRender/NcRender.h>
#include <fstream>
#include <imgui.h>
#include <loguru.hpp>

void NcControlView::preInit()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  nlohmann::json preferences = renderer.parseJsonFromFile(
    renderer.getConfigDirectory() + "preferences.json");
  if (preferences != NULL) {
    try {
      LOG_F(INFO,
            "Found %s!",
            std::string(renderer.getConfigDirectory() + "preferences.json")
              .c_str());
      m_preferences.background_color[0] =
        (double) preferences["background_color"]["r"];
      m_preferences.background_color[1] =
        (double) preferences["background_color"]["g"];
      m_preferences.background_color[2] =
        (double) preferences["background_color"]["b"];
      m_preferences.machine_plane_color[0] =
        (double) preferences["machine_plane_color"]["r"];
      m_preferences.machine_plane_color[1] =
        (double) preferences["machine_plane_color"]["g"];
      m_preferences.machine_plane_color[2] =
        (double) preferences["machine_plane_color"]["b"];
      m_preferences.cuttable_plane_color[0] =
        (double) preferences["cuttable_plane_color"]["r"];
      m_preferences.cuttable_plane_color[1] =
        (double) preferences["cuttable_plane_color"]["g"];
      m_preferences.cuttable_plane_color[2] =
        (double) preferences["cuttable_plane_color"]["b"];
      m_preferences.window_size[0] = (double) preferences["window_width"];
      m_preferences.window_size[1] = (double) preferences["window_height"];
    }
    catch (...) {
      LOG_F(WARNING, "Error parsing preferences file!");
    }
  }
  else {
    LOG_F(WARNING, "Preferences file does not exist, creating it!");
    m_preferences.background_color[0] = 0.f;
    m_preferences.background_color[1] = 0.f;
    m_preferences.background_color[2] = 0.f;
    m_preferences.machine_plane_color[0] = 100.0f / 255.0f;
    m_preferences.machine_plane_color[1] = 100.0f / 255.0f;
    m_preferences.machine_plane_color[2] = 100.0f / 255.0f;
    m_preferences.cuttable_plane_color[0] = 151.0f / 255.0f;
    m_preferences.cuttable_plane_color[1] = 5.0f / 255.0f;
    m_preferences.cuttable_plane_color[2] = 5.0f / 255.0f;
    m_preferences.window_size[0] = 1280;
    m_preferences.window_size[1] = 720;
  }

  nlohmann::json parameters = renderer.parseJsonFromFile(
    renderer.getConfigDirectory() + "machine_parameters.json");
  if (parameters != NULL) {
    try {
      LOG_F(
        INFO,
        "Found %s!",
        std::string(renderer.getConfigDirectory() + "machine_parameters.json")
          .c_str());
      // Use automatic deserialization via from_json
      from_json(parameters, m_machine_parameters);
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
          std::string(renderer.getConfigDirectory() + "machine_parameters.json")
            .c_str());
    m_machine_parameters.work_offset[0] = 0.0f;
    m_machine_parameters.work_offset[1] = 0.0f;
    m_machine_parameters.work_offset[2] = 0.0f;
    m_machine_parameters.machine_extents[0] = SCALE(1400.f);
    m_machine_parameters.machine_extents[1] = SCALE(1400.f);
    m_machine_parameters.machine_extents[2] = SCALE(60.f);
    m_machine_parameters.cutting_extents[0] = SCALE(50.f);
    m_machine_parameters.cutting_extents[1] = SCALE(50.f);
    m_machine_parameters.cutting_extents[2] = -SCALE(50.f);
    m_machine_parameters.cutting_extents[3] = -SCALE(50.f);
    m_machine_parameters.axis_scale[0] = 200 * (0.25f / DEFAULT_UNIT);
    m_machine_parameters.axis_scale[1] = 200 * (0.25f / DEFAULT_UNIT);
    m_machine_parameters.axis_scale[2] = 200 * (0.25f / DEFAULT_UNIT);
    m_machine_parameters.max_vel[0] = SCALE(1000.f);
    m_machine_parameters.max_vel[1] = SCALE(1000.f);
    m_machine_parameters.max_vel[2] = SCALE(250.f);
    m_machine_parameters.max_accel[0] = SCALE(800.0f);
    m_machine_parameters.max_accel[1] = SCALE(800.0f);
    m_machine_parameters.max_accel[2] = SCALE(200.0f);
    m_machine_parameters.junction_deviation = SCALE(0.005f);
    m_machine_parameters.arc_stabilization_time = 2000;
    m_machine_parameters.arc_voltage_divider = 50.f;
    m_machine_parameters.floating_head_backlash = SCALE(10.f);
    m_machine_parameters.z_probe_feedrate = SCALE(100.f);
    m_machine_parameters.axis_invert[0] = true;
    m_machine_parameters.axis_invert[1] = true;
    m_machine_parameters.axis_invert[2] = true;
    m_machine_parameters.axis_invert[3] = true;
    m_machine_parameters.soft_limits_enabled = false;
    m_machine_parameters.homing_enabled = false;
    m_machine_parameters.homing_dir_invert[0] = false;
    m_machine_parameters.homing_dir_invert[1] = false;
    m_machine_parameters.homing_dir_invert[2] = false;
    m_machine_parameters.homing_feed = SCALE(400.f);
    m_machine_parameters.homing_seek = SCALE(800.f);
    m_machine_parameters.homing_debounce = SCALE(250.f);
    m_machine_parameters.homing_pull_off = SCALE(10.f);
    m_machine_parameters.invert_limit_pins = false;
    m_machine_parameters.invert_probe_pin = false;
    m_machine_parameters.invert_step_enable = false;
    m_machine_parameters.precise_jog_units = SCALE(5.f);
  }
}

void NcControlView::renderUI()
{
  if (!m_app)
    return;

  // File dialog handling for G-code loading
  if (ImGuiFileDialog::Instance()->Display(
        "ChooseFileDlgKey", ImGuiWindowFlags_NoCollapse, ImVec2(600, 500))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      LOG_F(INFO, "Opening G-code file: %s", filePathName.c_str());

      // Save the last opened path
      m_app->getRenderer().stringToFile(
        m_app->getRenderer().getConfigDirectory() + "last_gcode_open_path.conf",
        filePath + "/");

      // Load the G-code file
      if (m_gcode && m_gcode->openFile(filePathName)) {
        m_app->getRenderer().pushTimer(
          0, [this]() { return m_gcode->parseTimer(); });
      }
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Main menu bar
  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open", "")) {
        LOG_F(INFO, "File->Open");
        std::string   path = ".";
        std::ifstream f(m_app->getRenderer().getConfigDirectory() +
                        "last_gcode_open_path.conf");
        if (f.is_open()) {
          std::string p((std::istreambuf_iterator<char>(f)),
                        std::istreambuf_iterator<char>());
          path = p;
        }
        IGFD::FileDialogConfig config;
        config.path = path;
        ImGuiFileDialog::Instance()->OpenDialog(
          "ChooseFileDlgKey", "Choose File", ".nc", config);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Close", "")) {
        LOG_F(INFO, "File->Close");
        m_app->requestQuit();
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Preferences", "")) {
        LOG_F(INFO, "Edit->Preferences");
        m_dialogs->showPreferences(true);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Machine Parameters", "")) {
        LOG_F(INFO, "Edit->Machine Parameters");
        m_dialogs->showMachineParameters(true);
      }
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Workbench")) {
      if (ImGui::MenuItem("Machine Control", "")) {
        LOG_F(INFO, "Workbench->Machine Control");
        m_app->getControlView().makeActive();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("CAM Toolpaths", "")) {
        LOG_F(INFO, "Workbench->CAM Toolpaths");
        m_app->getCamView().makeActive();
      }
      ImGui::EndMenu();
    }

    ImGui::EndMainMenuBar();
  }
}

void NcControlView::init()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  renderer.setCurrentView("NcControlView");
  // Events now handled through view delegation system

  // Register menu bar rendering
  m_menu_bar = renderer.pushGui(true, [this]() { renderUI(); });

  // Initialize G-code parser with dependencies
  m_gcode = std::make_unique<GCode>(m_app, this);

  // Initialize motion controller with dependencies
  m_motion_controller =
    std::make_unique<MotionController>(this, &m_app->getRenderer());
  m_motion_controller->initialize();

  // Create and initialize HMI
  m_hmi = std::make_unique<NcHmi>(m_app, this);
  m_hmi->init();

  // Create and initialize dialogs
  m_dialogs = std::make_unique<NcDialogs>(m_app, this);
  m_dialogs->init();

  // Initialize view state
  setMoveViewActive(false);
}
void NcControlView::tick()
{
  if (m_motion_controller) {
    m_motion_controller->tick();
  }
}
void NcControlView::makeActive()
{
  if (!m_app)
    return;

  m_app->setCurrentActiveView(this); // Register for event delegation
  auto& renderer = m_app->getRenderer();

  renderer.setCurrentView("NcControlView");
  renderer.setClearColor(m_preferences.background_color[0] * 255.0f,
                         m_preferences.background_color[1] * 255.0f,
                         m_preferences.background_color[2] * 255.0f);
}
void NcControlView::loadGCodeFromLines(std::vector<std::string>&& lines)
{
  if (!m_app || !m_gcode)
    return;

  // Switch view first so the timer and any primitives created during parsing
  // are associated with NcControlView (timers only fire in their own view)
  makeActive();

  if (m_gcode->loadFromLines(std::move(lines))) {
    m_app->getRenderer().pushTimer(0,
                                   [this]() { return m_gcode->parseTimer(); });
  }
}

void NcControlView::close() {}

void NcControlView::handleMouseEvent(const MouseButtonEvent& e,
                                     const InputState&       input)
{
  if (!m_app)
    return;

  // Handle right-click drag for pan/move view
  if (e.button == GLFW_MOUSE_BUTTON_2) {
    if (e.action == GLFW_PRESS) {
      // Start pan/move view mode
      setMoveViewActive(true);
    }
    else if (e.action == GLFW_RELEASE) {
      // End pan/move view mode
      setMoveViewActive(false);
    }
  }
}

void NcControlView::handleKeyEvent(const KeyEvent& e, const InputState& input)
{
  if (!m_app)
    return;

  // Handle TAB key to switch to NcCamView
  if (e.key == GLFW_KEY_TAB && e.action == GLFW_PRESS) {
    m_app->getCamView().makeActive();
    return;
  }

  // Delegate all key events to HMI
  if (m_hmi) {
    m_hmi->handleKeyEvent(e, input);
  }
}

void NcControlView::handleScrollEvent(const ScrollEvent& e,
                                      const InputState&  input)
{
  if (!m_app)
    return;

  // Get mouse position for zoom center
  Point2d mouse_pos = input.getMousePosition();

  if (e.y_offset > 0) {
    // Scroll up = zoom in
    zoomIn(mouse_pos);
  }
  else if (e.y_offset < 0) {
    // Scroll down = zoom out
    zoomOut(mouse_pos);
  }
}

void NcControlView::handleMouseMoveEvent(const MouseMoveEvent& e,
                                         const InputState&     input)
{
  if (!m_app)
    return;

  // Delegate to HMI
  if (m_hmi) {
    m_hmi->handleMouseMoveEvent(e, input);
  }
}

void NcControlView::handleWindowResize(const WindowResizeEvent& e,
                                       const InputState&        input)
{
  if (!m_app)
    return;

  // Delegate to HMI
  if (m_hmi) {
    m_hmi->handleWindowResizeEvent(e, input);
  }
}
