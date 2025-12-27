#include "hmi.h"
#include "../../NcRender/primitives/Primitives.h"
#include "../../application/NcApp.h"
#include "../../input/InputEvents.h"
#include "../../input/InputState.h"
#include "../gcode/gcode.h"
#include "../motion_control/motion_controller.h"
#include "../util.h"
#include "NcControlView/NcControlView.h"
#include <cmath>

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
  nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
  try {

    if (dro_data["IN_MOTION"] == false) {
      if (id == "Wpos") {
        LOG_F(INFO, "Clicked Wpos");
        control_view.m_motion_controller->pushGCode("G0 X0 Y0");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Park") {
        LOG_F(INFO, "Clicked Park");
        control_view.m_motion_controller->pushGCode("G53 G0 Z0");
        control_view.m_motion_controller->pushGCode("G53 G0 X0 Y0");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Zero X") {
        LOG_F(INFO, "Clicked Zero X");
        try {
          control_view.m_machine_parameters.work_offset[0] =
            control_view.m_motion_controller->getDRO()["MCS"]["x"].get<float>();
          control_view.m_motion_controller->pushGCode(
            "G10 L2 P0 X" +
            to_string_strip_zeros(
              control_view.m_machine_parameters.work_offset[0]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          control_view.m_motion_controller->saveParameters();
        }
        catch (...) {
          LOG_F(ERROR, "Could not set x work offset!");
        }
      }
      else if (id == "Zero Y") {
        LOG_F(INFO, "Clicked Zero Y");
        try {
          control_view.m_machine_parameters.work_offset[1] =
            control_view.m_motion_controller->getDRO()["MCS"]["y"].get<float>();
          control_view.m_motion_controller->pushGCode(
            "G10 L2 P0 Y" +
            to_string_strip_zeros(
              control_view.m_machine_parameters.work_offset[1]));
          control_view.m_motion_controller->pushGCode("M30");
          control_view.m_motion_controller->runStack();
          control_view.m_motion_controller->saveParameters();
        }
        catch (...) {
          LOG_F(ERROR, "Could not set y work offset!");
        }
      }
      //        else if (id == "Zero Z")
      //        {
      //            LOG_F(INFO, "Clicked Zero Z");
      //            try
      //            {
      //                control_view.m_machine_parameters.work_offset[2]
      //                =
      //                static_cast<float>(control_view.m_motion_controller->getDRO()["MCS"]["z"]);
      //                control_view.m_motion_controller->pushGCode("G10 L2
      //                P0 Z" +
      //                std::to_string(control_view.m_machine_parameters.work_offset[2]));
      //                control_view.m_motion_controller->pushGCode("M30");
      //                control_view.m_motion_controller->runStack();
      //                control_view.m_motion_controller->saveParameters();
      //            }
      //            catch (...)
      //            {
      //                LOG_F(ERROR, "Could not set z work offset!");
      //            }
      //        }
      else if (id == "Spindle On") {
        // Not used
        LOG_F(INFO, "Spindle On");
        control_view.m_motion_controller->pushGCode("M3 S1000");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Spindle Off") {
        // Not used
        LOG_F(INFO, "Spindle Off");
        control_view.m_motion_controller->pushGCode("M5");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Retract") {
        LOG_F(INFO, "Clicked Retract");
        control_view.m_motion_controller->pushGCode("G0 Z0");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Touch") {
        LOG_F(INFO, "Clicked Touch");
        control_view.m_motion_controller->pushGCode("touch_torch");
        control_view.m_motion_controller->pushGCode("M30");
        control_view.m_motion_controller->runStack();
      }
      else if (id == "Run") {
        LOG_F(INFO, "Clicked Run");
        if (checkPathBounds()) {
          try {
            std::string filename = control_view.getGCode().getFilename();
            if (filename != "") {
              std::ifstream gcode_file(filename);
              if (gcode_file.is_open()) {
                std::string line;
                while (std::getline(gcode_file, line)) {
                  control_view.m_motion_controller->pushGCode(line);
                }
                gcode_file.close();
                control_view.m_motion_controller->runStack();
              }
              else {
                m_view->getDialogs().setInfoValue("Could not open file!");
              }
            }
          }
          catch (...) {
            m_view->getDialogs().setInfoValue(
              "Caught exception while trying to read file!");
          }
        }
        else {
          m_view->getDialogs().setInfoValue(
            "Program is outside of machines cuttable extents!");
        }
      }
      else if (id == "Test Run") {
        LOG_F(INFO, "Clicked Test Run");
        if (checkPathBounds()) {
          try {
            std::string filename = control_view.getGCode().getFilename();
            if (filename != "") {
              std::ifstream gcode_file(filename);
              if (gcode_file.is_open()) {
                std::string line;
                while (std::getline(gcode_file, line)) {
                  if (line.find("fire_torch") != std::string::npos) {
                    removeSubstrs(line, "fire_torch");
                    control_view.m_motion_controller->pushGCode("touch_torch" +
                                                                line);
                  }
                  else {
                    control_view.m_motion_controller->pushGCode(line);
                  }
                }
                gcode_file.close();
                control_view.m_motion_controller->runStack();
              }
              else {
                m_view->getDialogs().setInfoValue("Could not open file!");
              }
            }
          }
          catch (...) {
            m_view->getDialogs().setInfoValue(
              "Caught exception while trying to read file!");
          }
        }
        else {
          m_view->getDialogs().setInfoValue(
            "Program is outside of machines cuttable extents!");
        }
      }
    }
    if (id == "Abort") {
      LOG_F(INFO, "Clicked Abort");
      control_view.m_motion_controller->abort();
    }
    if (id == "Clean") {
      LOG_F(INFO, "Clicked Clean");
      clearHighlights();
    }
    else if (id == "Fit") {
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
        pan.x = -((control_view.m_machine_parameters.machine_extents[0]) / 2.0);
        pan.y = -((control_view.m_machine_parameters.machine_extents[1]) / 2.0);
        m_view->setPan(pan);
        auto y_pair =
          minmax(m_app->getRenderer().getWindowSize().y,
                 control_view.m_machine_parameters.machine_extents[1]);
        auto x_pair =
          minmax(m_app->getRenderer().getWindowSize().x - m_backplane_width,
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
            (window_x - (m_backplane_width / 2.0)) / (machine_x + window_x);
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
        new_pan.y = -(bbox_min.y + (y_diff / 2) + 10); // 10 is for the menu bar
        m_view->setPan(new_pan);
        auto y_pair = minmax(m_app->getRenderer().getWindowSize().y, y_diff);
        auto x_pair = minmax(
          m_app->getRenderer().getWindowSize().x - m_backplane_width, x_diff);
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
                             (m_backplane_width / 2)) /
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
    }
    else if (id == "THC") {
      LOG_F(INFO, "Clicked THC");
      m_view->getDialogs().showThcWindow(true);
    }
  }
  catch (...) {
    LOG_F(ERROR, "JSON Parsing ERROR!");
  }
}

void NcHmi::jumpin(Primitive* p)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (checkPathBounds()) {
    try {
      std::string filename = control_view.getGCode().getFilename();
      if (filename != "") {
        std::ifstream gcode_file(filename);
        if (gcode_file.is_open()) {
          std::string   line;
          unsigned long count = 0;
          while (std::getline(gcode_file, line)) {
            if (count > (unsigned long) p->data["rapid_line"] - 1)
              control_view.m_motion_controller->pushGCode(line);
            count++;
          }
          gcode_file.close();
          control_view.m_motion_controller->runStack();
        }
        else {
          m_view->getDialogs().setInfoValue("Could not open file!");
        }
      }
    }
    catch (...) {
      m_view->getDialogs().setInfoValue(
        "Caught exception while trying to read file!");
    }
  }
  else {
    m_view->getDialogs().setInfoValue(
      "Program is outside of machines cuttable extents!");
  }
}

void NcHmi::reverse(Primitive* p)
{
  if (!m_app)
    return;
  auto&                    control_view = m_app->getControlView();
  std::vector<std::string> gcode_lines_before_reverse;
  std::vector<std::string> gcode_lines_to_reverse;
  std::vector<std::string> gcode_lines_after_reverse;
  bool                     found_path_end = false;
  std::string              filename = control_view.getGCode().getFilename();
  try {
    if (filename != "") {
      std::ifstream gcode_file(filename);
      if (gcode_file.is_open()) {
        std::string   line;
        unsigned long count = 0;
        while (std::getline(gcode_file, line)) {
          if (count <= (unsigned long) p->data["rapid_line"] + 1) {
            gcode_lines_before_reverse.push_back(line);
          }
          else if (count > (unsigned long) p->data["rapid_line"] + 1) {
            if (line.find("torch_off") != std::string::npos) {
              found_path_end = true;
            }
            if (found_path_end == true) {
              gcode_lines_after_reverse.push_back(line);
            }
            else {
              gcode_lines_to_reverse.push_back(line);
            }
          }
          count++;
        }
        gcode_file.close();
      }
      else {
        m_view->getDialogs().setInfoValue("Could not open file!");
      }
    }
  }
  catch (...) {
    m_view->getDialogs().setInfoValue(
      "Caught exception while trying to read file!");
  }
  try {
    std::ofstream out(filename);
    for (unsigned long x = 0; x < gcode_lines_before_reverse.size(); x++) {
      out << gcode_lines_before_reverse.at(x);
      out << std::endl;
    }
    while (gcode_lines_to_reverse.size() > 0) {
      out << gcode_lines_to_reverse.back();
      out << std::endl;
      gcode_lines_to_reverse.pop_back();
    }
    for (unsigned long x = 0; x < gcode_lines_after_reverse.size(); x++) {
      out << gcode_lines_after_reverse.at(x);
      out << std::endl;
    }
    out.close();
    clearHighlights();
    if (control_view.getGCode().openFile(filename)) {
      auto& gcode = control_view.getGCode();
      m_app->getRenderer().pushTimer(0,
                                     [&gcode]() { return gcode.parseTimer(); });
    }
  }
  catch (...) {
    m_view->getDialogs().setInfoValue("Could not write gcode file to disk!");
  }
}

void NcHmi::mouseCallback(Primitive* c, const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto&             control_view = m_app->getControlView();
  const std::string type = c->getTypeName();
  if (type == "path" && c->id == "gcode") {
    if (e.contains("event")) {
      auto event = e.at("event").get<NcRender::EventType>();
      if (event == NcRender::EventType::MouseIn and
          m_app->isModifierPressed(GLFW_MOD_CONTROL)) {
        // LOG_F(INFO, "Start Line => %lu", (unsigned
        // long)o->data["rapid_line"]);
        m_app->getRenderer().setColorByName(c->color, "light-green");
      }
      else if (event == NcRender::EventType::MouseOut) {
        // LOG_F(INFO, "Mouse Out - %s\n", e.dump().c_str());
        m_app->getRenderer().setColorByName(c->color, "white");
      }
    }
    else if (e["button"].get<int>() == GLFW_MOUSE_BUTTON_1) {
      if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
        if (e["action"] & NcRender::ActionFlagBits::Press) {
          m_app->getRenderer().setColorByName(c->color, "green");
        }
        else if (e["action"] & NcRender::ActionFlagBits::Release) {
          m_app->getRenderer().setColorByName(c->color, "light-green");
          m_view->getDialogs().askYesNo(
            "Are you sure you want to start the program at this path?",
            [this, c]() { jumpin(c); });
        }
      }
    }
    else if (e["button"].get<int>() == GLFW_MOUSE_BUTTON_2) {
      if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
        if (e["action"].get<int>() & NcRender::ActionFlagBits::Release) {
          m_app->getRenderer().setColorByName(c->color, "light-green");
          m_view->getDialogs().askYesNo(
            "Are you sure you want to reverse this paths direction?",
            [this, c]() { reverse(c); });
        }
        else if (e["action"].get<int>() & NcRender::ActionFlagBits::Press) {
          m_app->getRenderer().setColorByName(c->color, "green");
        }
      }
    }
  }
  else if (type == "box" && c->id != "cuttable_plane" &&
           c->id != "machine_plane") {
    if (e.contains("event")) {
      auto event = e.at("event").get<NcRender::EventType>();
      if (event == NcRender::EventType::MouseIn) {
        m_app->getRenderer().setColorByName(c->color, "light-green");
      }
      else if (event == NcRender::EventType::MouseOut) {
        m_app->getRenderer().setColorByName(c->color, "black");
      }
    }
    else if (e["button"].get<int>() == GLFW_MOUSE_BUTTON_1) {
      if (e["action"].get<int>() & NcRender::ActionFlagBits::Press) {
        m_app->getRenderer().setColorByName(c->color, "green");
      }
      if (e["action"].get<int>() & NcRender::ActionFlagBits::Release) {
        m_app->getRenderer().setColorByName(c->color, "light-green");
        handleButton(c->id);
      }
    }
  }
  else if ((c->id == "cuttable_plane" or c->id == "machine_plane") and
           e.contains("button")) {
    // Get mouse position in matrix coordinates from NcApp
    // Get mouse position in matrix coordinates using view transformation
    Point2d     screen_pos = m_app->getInputState().getMousePosition();
    Point2d     p = m_view->screenToMatrix(screen_pos);
    const auto* cp = control_view.m_cuttable_plane;
    const auto  bl = cp->m_bottom_left;
    // double check point against cuttable plane in case it was
    // "machine_plane" that triggered this code path
    bool is_within = p.x >= bl.x and p.x <= bl.x + cp->m_width and
                     p.y >= bl.y and p.y <= bl.y + cp->m_height;
    if (not is_within) {
      return;
    }
    if (e["button"].get<int>() == GLFW_MOUSE_BUTTON_1 and
        e["action"].get<int>() & NcRender::ActionFlagBits::Release) {
      nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
      try {
        if (dro_data["IN_MOTION"] == false) {
          LOG_F(INFO, "Add waypoint position [%f, %f]", p.x, p.y);
          control_view.m_way_point_position = p;

          // Show waypoint on machine plane
          control_view.m_waypoint_pointer->m_center = p;
          control_view.m_waypoint_pointer->visible = true;

          // Show popup asking user if they want to go to this location
          m_view->getDialogs().askYesNo(
            "Go to waypoint position?",
            [this]() { goToWaypoint(nullptr); },
            [&control_view]() {
              control_view.m_waypoint_pointer->visible = false;
            });
        }
      }
      catch (...) {
        LOG_F(ERROR, "Error parsing DRO Data!");
      }
    }
  }
}

bool NcHmi::updateTimer()
{
  if (!m_app)
    return false;
  auto&          control_view = m_app->getControlView();
  nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
  try {
    if (dro_data.contains("STATUS")) {
      // { "STATUS": "Idle", "MCS": { "x": 0.000,"y": 0.000,"z": 0.000 },
      // "WCS": { "x": -4.594,"y": -3.260,"z": 0.000 }, "FEED": 0, "V": 0,
      // "IN_MOTION": false, "ARC_OK": true }
      const float wcx = dro_data["WCS"]["x"].get<float>();
      const float wcy = dro_data["WCS"]["y"].get<float>();
      const float wcz = dro_data["WCS"]["z"].get<float>();
      m_dro.x.work_readout->m_textval = to_fixed_string(abs(wcx), 4);
      m_dro.y.work_readout->m_textval = to_fixed_string(abs(wcy), 4);
      m_dro.z.work_readout->m_textval = to_fixed_string(abs(wcz), 4);
      const float mcx = dro_data["MCS"]["x"].get<float>();
      const float mcy = dro_data["MCS"]["y"].get<float>();
      const float mcz = dro_data["MCS"]["z"].get<float>();
      m_dro.x.absolute_readout->m_textval = to_fixed_string(abs(mcx), 4);
      m_dro.y.absolute_readout->m_textval = to_fixed_string(abs(mcy), 4);
      m_dro.z.absolute_readout->m_textval = to_fixed_string(abs(mcz), 4);
      m_dro.feed->m_textval =
        "FEED: " + to_fixed_string(dro_data["FEED"].get<float>(), 1);
      m_dro.arc_readout->m_textval =
        "ARC: " +
        to_fixed_string(dro_data.at("V").get<int>() *
                          control_view.m_machine_parameters.arc_voltage_divider,
                        1) +
        "V";
      m_dro.arc_set->m_textval =
        "SET: " +
        to_fixed_string(control_view.m_machine_parameters.thc_set_value, 1);
      nlohmann::json runtime = control_view.m_motion_controller->getRunTime();
      if (runtime != NULL)
        m_dro.run_time->m_textval =
          "RUN: " + to_string_strip_zeros((int) runtime["hours"]) + ":" +
          to_string_strip_zeros((int) runtime["minutes"]) + ":" +
          to_string_strip_zeros((int) runtime["seconds"]);
      control_view.m_torch_pointer->m_center = { abs(mcx), abs(mcy) };
      if (control_view.m_motion_controller->isTorchOn()) {
        m_dro_backpane->color[0] = 100;
        m_dro_backpane->color[1] = 32;
        m_dro_backpane->color[2] = 48;
      }
      else {
        m_dro_backpane->color[0] = 29;
        m_dro_backpane->color[1] = 32;
        m_dro_backpane->color[2] = 48;
      }
      if ((bool) dro_data["ARC_OK"] == false) {
        m_app->getRenderer().setColorByName(m_dro.arc_readout->color, "red");
        // Keep adding m_points to current highlight path
        // Negated because this is how gcode is interpreted to be consistent
        // with grbl's machine coordinates
        const Point2d wc_point{ -wcx, -wcy };
        if (not m_arc_okay_highlight_path) {
          std::vector<Point2d> path{ wc_point };
          m_arc_okay_highlight_path =
            m_app->getRenderer().pushPrimitive<Path>(path);
          m_arc_okay_highlight_path->id = "gcode_highlights";
          m_arc_okay_highlight_path->m_is_closed = false;
          m_arc_okay_highlight_path->m_width = 3;
          m_app->getRenderer().setColorByName(m_arc_okay_highlight_path->color,
                                              "green");
          m_arc_okay_highlight_path->matrix_callback =
            m_view->getTransformCallback();
        }
        else {
          m_arc_okay_highlight_path->m_points.push_back(wc_point);
        }
      }
      else {
        m_arc_okay_highlight_path = NULL;
        m_dro.arc_readout->color[0] = 247;
        m_dro.arc_readout->color[1] = 104;
        m_dro.arc_readout->color[2] = 15;
      }
    }
  }
  catch (const nlohmann::json::out_of_range& e) {
    LOG_F(ERROR, "Failed to parse controller data: %s", e.what());
  }
  return true;
}

void NcHmi::resizeCallback(const nlohmann::json& e)
{
  // LOG_F(INFO, "(resizeCallback) %s", e.dump().c_str());
  m_backpane->m_bottom_left.x =
    (m_app->getRenderer().getWindowSize().x / 2) - m_backplane_width;
  m_backpane->m_bottom_left.y = -(m_app->getRenderer().getWindowSize().y / 2);
  m_backpane->m_width = m_backplane_width;
  m_backpane->m_height = m_app->getRenderer().getWindowSize().y - 15;

  m_dro_backpane->m_bottom_left.x = m_backpane->m_bottom_left.x + 5;
  m_dro_backpane->m_bottom_left.y =
    (m_app->getRenderer().getWindowSize().y / 2) - m_dro_backplane_height;
  m_dro_backpane->m_width = m_backplane_width - 10;
  m_dro_backpane->m_height = m_dro_backplane_height - 30;

  float dro_group_x = (float) m_dro_backpane->m_bottom_left.x;
  float dro_group_y = (float) m_dro_backpane->m_bottom_left.y;

  dro_group_y += 10;
  m_dro.feed->m_position = { (float) dro_group_x + 5,
                             (float) dro_group_y - m_dro.feed->m_height + 5 };
  m_dro.arc_readout->m_position = { (float) dro_group_x + 80,
                                    (float) dro_group_y -
                                      m_dro.arc_readout->m_height + 5 };
  m_dro.arc_set->m_position = {
    (float) dro_group_x + 150, (float) dro_group_y - m_dro.arc_set->m_height + 5
  };
  m_dro.run_time->m_position = { (float) dro_group_x + 210,
                                 (float) dro_group_y -
                                   m_dro.run_time->m_height + 5 };
  dro_group_y += 55;
  m_dro.z.label->m_position = { (float) dro_group_x + 5,
                                (float) dro_group_y - m_dro.z.label->m_height };
  m_dro.z.work_readout->m_position = {
    (float) dro_group_x + 5 + 50, (float) dro_group_y - m_dro.z.label->m_height
  };
  m_dro.z.absolute_readout->m_position = {
    (float) dro_group_x + 5 + 220, (float) dro_group_y - m_dro.z.label->m_height
  };
  m_dro.z.divider->m_bottom_left = { (float) dro_group_x + 5,
                                     (float) dro_group_y - 45 };
  m_dro.z.divider->m_width = m_backplane_width - 20;
  m_dro.z.divider->m_height = 5;
  dro_group_y += 55;
  m_dro.y.label->m_position = { (float) dro_group_x + 5,
                                (float) dro_group_y - m_dro.y.label->m_height };
  m_dro.y.work_readout->m_position = {
    (float) dro_group_x + 5 + 50, (float) dro_group_y - m_dro.y.label->m_height
  };
  m_dro.y.absolute_readout->m_position = {
    (float) dro_group_x + 5 + 220, (float) dro_group_y - m_dro.y.label->m_height
  };
  m_dro.y.divider->m_bottom_left = { (float) dro_group_x + 5,
                                     (float) dro_group_y - 45 };
  m_dro.y.divider->m_width = m_backplane_width - 20;
  m_dro.y.divider->m_height = 5;
  dro_group_y += 55;
  m_dro.x.label->m_position = { (float) dro_group_x + 5,
                                (float) dro_group_y - m_dro.x.label->m_height };
  m_dro.x.work_readout->m_position = {
    (float) dro_group_x + 5 + 50, (float) dro_group_y - m_dro.x.label->m_height
  };
  m_dro.x.absolute_readout->m_position = {
    (float) dro_group_x + 5 + 220, (float) dro_group_y - m_dro.x.label->m_height
  };
  m_dro.x.divider->m_bottom_left = { (float) dro_group_x + 5,
                                     (float) dro_group_y - 45 };
  m_dro.x.divider->m_width = m_backplane_width - 20;
  m_dro.x.divider->m_height = 5;

  m_button_backpane->m_bottom_left.x = m_backpane->m_bottom_left.x + 5;
  m_button_backpane->m_bottom_left.y = m_backpane->m_bottom_left.y + 20;
  m_button_backpane->m_width = m_backplane_width - 10;
  m_button_backpane->m_height =
    m_app->getRenderer().getWindowSize().y - (m_dro_backplane_height + 30);

  double button_group_x = m_button_backpane->m_bottom_left.x;
  double button_group_y =
    m_button_backpane->m_bottom_left.y + m_button_backpane->m_height;
  double button_height =
    (m_button_backpane->m_height - 10) / (double) m_button_groups.size();
  double button_width = m_button_backpane->m_width / 2;

  double center_x;
  double center_y;
  for (int x = 0; x < m_button_groups.size(); x++) {
    m_button_groups[x].button_one.object->m_bottom_left.x = button_group_x + 5;
    m_button_groups[x].button_one.object->m_bottom_left.y =
      (button_group_y - 2.5) - button_height;
    m_button_groups[x].button_one.object->m_width = button_width - 10;
    m_button_groups[x].button_one.object->m_height = button_height - 10;
    center_x = m_button_groups[x].button_one.object->m_bottom_left.x +
               (button_width / 2);
    center_y = m_button_groups[x].button_one.object->m_bottom_left.y +
               (button_height / 2);
    m_button_groups[x].button_one.label->m_position = {
      (float) center_x - (m_button_groups[x].button_one.label->m_width / 2.0f) -
        5,
      (float) center_y - (m_button_groups[x].button_one.label->m_height) + 5
    };

    button_group_x += button_width;
    m_button_groups[x].button_two.object->m_bottom_left.x = button_group_x + 5;
    m_button_groups[x].button_two.object->m_bottom_left.y =
      (button_group_y - 2.5) - button_height;
    m_button_groups[x].button_two.object->m_width = button_width - 10;
    m_button_groups[x].button_two.object->m_height = button_height - 10;
    center_x = m_button_groups[x].button_two.object->m_bottom_left.x +
               (button_width / 2);
    center_y = m_button_groups[x].button_two.object->m_bottom_left.y +
               (button_height / 2);

    m_button_groups[x].button_two.label->m_position = {
      (float) center_x - (m_button_groups[x].button_two.label->m_width / 2.0f) -
        5,
      (float) center_y - (m_button_groups[x].button_two.label->m_height) + 5
    };
    button_group_x -= button_width;

    button_group_y -= button_height;
  }
}

void NcHmi::pushButtonGroup(const std::string& b1, const std::string& b2)
{
  hmi_button_group_t group;
  group.button_one.name = b1;
  group.button_one.object =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 5);
  group.button_one.object->mouse_callback =
    [this](Primitive* c, const nlohmann::json& e) { mouseCallback(c, e); };
  m_app->getRenderer().setColorByName(group.button_one.object->color, "black");
  group.button_one.object->zindex = 200;
  group.button_one.object->id = b1;
  group.button_one.label = m_app->getRenderer().pushPrimitive<Text>(
    Point2d::infNeg(), group.button_one.name, 20);
  group.button_one.label->zindex = 210;
  m_app->getRenderer().setColorByName(group.button_one.label->color, "white");

  group.button_two.name = b2;
  group.button_two.object =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 5);
  group.button_two.object->mouse_callback =
    [this](Primitive* c, const nlohmann::json& e) { mouseCallback(c, e); };
  m_app->getRenderer().setColorByName(group.button_two.object->color, "black");
  group.button_two.object->zindex = 200;
  group.button_two.object->id = b2;
  group.button_two.label = m_app->getRenderer().pushPrimitive<Text>(
    Point2d::infNeg(), group.button_two.name, 20);
  group.button_two.label->zindex = 210;
  m_app->getRenderer().setColorByName(group.button_two.label->color, "white");

  m_button_groups.push_back(group);
}

void NcHmi::clearHighlights()
{
  if (!m_app)
    return;
  m_app->getRenderer().deletePrimitivesById("gcode_highlights");
  m_arc_okay_highlight_path = nullptr;
}

void NcHmi::tabKeyUpCallback(const nlohmann::json& e)
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
void NcHmi::escapeKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  handleButton("Abort");
  control_view.m_way_point_position = { std::numeric_limits<double>::min(),
                                        std::numeric_limits<double>::min() };
}
void NcHmi::upKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data["STATUS"] == "IDLE") {
      if ((e["action"].get<int>() & NcRender::ActionFlagBits::Press) ||
          e["action"].get<int>() == GLFW_REPEAT) {
        if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging Y positive %s", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 Y" + dist);
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
    if ((e["action"].get<int>() & NcRender::ActionFlagBits::Release) and
        not(e["mods"].get<int>() & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling Y positive jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}

void NcHmi::downKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data["STATUS"] == "IDLE") {
      if ((e["action"].get<int>() & NcRender::ActionFlagBits::Press) ||
          e["action"].get<int>() == GLFW_REPEAT) {
        if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging Y negative %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 Y-" + dist);
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
    if ((e["action"].get<int>() & NcRender::ActionFlagBits::Release) &&
        not(e["mods"].get<int>() & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling Y negative jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}

void NcHmi::rightKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data["STATUS"] == "IDLE") {
      if ((e["action"].get<int>() & NcRender::ActionFlagBits::Press) ||
          e["action"].get<int>() == GLFW_REPEAT) {
        if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging X positive %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 X" + dist);
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
    if (e["action"].get<int>() & NcRender::ActionFlagBits::Release &&
        not(e["mods"].get<int>() & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling X Positive jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}
void NcHmi::leftKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  try {
    nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
    if (dro_data["STATUS"] == "IDLE") {
      if ((e["action"].get<int>() & NcRender::ActionFlagBits::Press) ||
          e["action"].get<int>() == GLFW_REPEAT) {
        if (e["mods"].get<int>() & GLFW_MOD_CONTROL) {
          std::string dist =
            std::to_string(control_view.m_machine_parameters.precise_jog_units);
          LOG_F(INFO, "Jogging X negative %s!", dist.c_str());
          control_view.m_motion_controller->pushGCode("G91 X-" + dist);
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
    if ((e["action"].get<int>() & NcRender::ActionFlagBits::Release) &&
        not(e["mods"].get<int>() & GLFW_MOD_CONTROL)) {
      // key up
      LOG_F(INFO, "Cancelling X Negative jog!");
      handleButton("Abort");
    }
  }
  catch (...) {
    // DRO data not available
  }
}
void NcHmi::pageUpKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (e["action"].get<int>() & NcRender::ActionFlagBits::Press) {
    if (control_view.m_machine_parameters.homing_dir_invert[2]) {
      control_view.m_motion_controller->sendRealTime('<');
    }
    else {
      control_view.m_motion_controller->sendRealTime('>');
    }
  }
  else if (e["action"].get<int>() & NcRender::ActionFlagBits::Release) {
    control_view.m_motion_controller->sendRealTime('^');
  }
  /*try
  {
      nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
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
void NcHmi::pageDownKeyCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();
  if (e["action"].get<int>() & NcRender::ActionFlagBits::Press) {
    if (control_view.m_machine_parameters.homing_dir_invert[2]) {
      control_view.m_motion_controller->sendRealTime('>');
    }
    else {
      control_view.m_motion_controller->sendRealTime('<');
    }
  }
  else if (e["action"].get<int>() & NcRender::ActionFlagBits::Release) {
    control_view.m_motion_controller->sendRealTime('^');
  }
  /*try
  {
      nlohmann::json dro_data = control_view.m_motion_controller->getDRO();
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
void NcHmi::mouseMotionCallback(const nlohmann::json& e)
{
  if (!m_app)
    return;

  Point2d point = { (double) e["pos"]["x"], (double) e["pos"]["y"] };

  // Update global mouse position
  m_app->getInputState().setMousePosition(point);

  // Handle view-specific mouse movement (includes panning when move view is
  // active)
  m_view->handleMouseMove(point);

  double      zoom = m_view->getZoom();
  const auto& pan = m_view->getPan();
  Point2d     matrix_coords = { (e["pos"]["x"].get<double>() - pan.x) / zoom,
                                (e["pos"]["y"].get<double>() - pan.y) / zoom };
  // Matrix coordinates are calculated on demand via view transformation
}
void NcHmi::init()
{
  if (!m_app)
    return;
  auto& control_view = m_app->getControlView();

  control_view.m_way_point_position = { std::numeric_limits<int>::min(),
                                        std::numeric_limits<int>::min() };

  control_view.m_machine_plane = m_app->getRenderer().pushPrimitive<Box>(
    Point2d{ 0, 0 },
    control_view.m_machine_parameters.machine_extents[0],
    control_view.m_machine_parameters.machine_extents[1],
    0);
  control_view.m_machine_plane->id = "machine_plane";
  control_view.m_machine_plane->zindex = -20;
  control_view.m_machine_plane->color[0] =
    control_view.m_preferences.machine_plane_color[0] * 255.0f;
  control_view.m_machine_plane->color[1] =
    control_view.m_preferences.machine_plane_color[1] * 255.0f;
  control_view.m_machine_plane->color[2] =
    control_view.m_preferences.machine_plane_color[2] * 255.0f;
  control_view.m_machine_plane->matrix_callback =
    m_view->getTransformCallback();
  control_view.m_machine_plane->mouse_callback =
    [this](Primitive* c, const nlohmann::json& e) { mouseCallback(c, e); };

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
  control_view.m_cuttable_plane->zindex = -10;
  control_view.m_cuttable_plane->color[0] =
    control_view.m_preferences.cuttable_plane_color[0] * 255.0f;
  control_view.m_cuttable_plane->color[1] =
    control_view.m_preferences.cuttable_plane_color[1] * 255.0f;
  control_view.m_cuttable_plane->color[2] =
    control_view.m_preferences.cuttable_plane_color[2] * 255.0f;
  control_view.m_cuttable_plane->matrix_callback =
    m_view->getTransformCallback();

  m_backpane =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 5);
  m_backpane->color[0] = 25;
  m_backpane->color[1] = 44;
  m_backpane->color[2] = 71;
  m_backpane->zindex = 100;

  m_dro_backpane =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 5);
  m_dro_backpane->color[0] = 29;
  m_dro_backpane->color[1] = 32;
  m_dro_backpane->color[2] = 48;
  m_dro_backpane->zindex = 110;

  m_button_backpane =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 5);
  m_button_backpane->color[0] = 29;
  m_button_backpane->color[1] = 32;
  m_button_backpane->color[2] = 48;
  m_button_backpane->zindex = 115;

  pushButtonGroup("Zero X", "Zero Y");
  pushButtonGroup("Touch", "Retract");
  pushButtonGroup("Fit", "Clean");
  pushButtonGroup("Wpos", "Park");
  pushButtonGroup("Test Run", "THC");
  pushButtonGroup("Run", "Abort");

  m_dro.x.label =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "X", 50);
  m_dro.x.label->zindex = 210;
  m_app->getRenderer().setColorByName(m_dro.x.label->color, "white");
  m_dro.x.work_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 40);
  m_dro.x.work_readout->zindex = 210;
  m_dro.x.work_readout->color[0] = 10;
  m_dro.x.work_readout->color[1] = 150;
  m_dro.x.work_readout->color[2] = 10;
  m_dro.x.absolute_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 15);
  m_dro.x.absolute_readout->zindex = 210;
  m_dro.x.absolute_readout->color[0] = 247;
  m_dro.x.absolute_readout->color[1] = 104;
  m_dro.x.absolute_readout->color[2] = 15;
  m_dro.x.divider =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 3);
  m_dro.x.divider->zindex = 150;
  m_app->getRenderer().setColorByName(m_dro.x.divider->color, "black");

  m_dro.y.label =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "Y", 50);
  m_dro.y.label->zindex = 210;
  m_app->getRenderer().setColorByName(m_dro.y.label->color, "white");
  m_dro.y.work_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 40);
  m_dro.y.work_readout->zindex = 210;
  m_dro.y.work_readout->color[0] = 10;
  m_dro.y.work_readout->color[1] = 150;
  m_dro.y.work_readout->color[2] = 10;
  m_dro.y.absolute_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 15);
  m_dro.y.absolute_readout->zindex = 210;
  m_dro.y.absolute_readout->color[0] = 247;
  m_dro.y.absolute_readout->color[1] = 104;
  m_dro.y.absolute_readout->color[2] = 15;
  m_dro.y.divider =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 3);
  m_dro.y.divider->zindex = 150;
  m_app->getRenderer().setColorByName(m_dro.y.divider->color, "black");

  m_dro.z.label =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "Z", 50);
  m_dro.z.label->zindex = 210;
  m_app->getRenderer().setColorByName(m_dro.z.label->color, "white");
  m_dro.z.work_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 40);
  m_dro.z.work_readout->zindex = 210;
  m_dro.z.work_readout->color[0] = 10;
  m_dro.z.work_readout->color[1] = 150;
  m_dro.z.work_readout->color[2] = 10;
  m_dro.z.absolute_readout =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "0.0000", 15);
  m_dro.z.absolute_readout->zindex = 210;
  m_dro.z.absolute_readout->color[0] = 247;
  m_dro.z.absolute_readout->color[1] = 104;
  m_dro.z.absolute_readout->color[2] = 15;
  m_dro.z.divider =
    m_app->getRenderer().pushPrimitive<Box>(Point2d::infNeg(), 1, 1, 3);
  m_dro.z.divider->zindex = 150;
  m_app->getRenderer().setColorByName(m_dro.z.divider->color, "black");

  m_dro.feed =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "FEED: 0", 12);
  m_dro.feed->zindex = 210;
  m_dro.feed->color[0] = 247;
  m_dro.feed->color[1] = 104;
  m_dro.feed->color[2] = 15;

  m_dro.arc_readout = m_app->getRenderer().pushPrimitive<Text>(
    Point2d::infNeg(), "ARC: 0.0V", 12);
  m_dro.arc_readout->zindex = 210;
  m_dro.arc_readout->color[0] = 247;
  m_dro.arc_readout->color[1] = 104;
  m_dro.arc_readout->color[2] = 15;

  m_dro.arc_set =
    m_app->getRenderer().pushPrimitive<Text>(Point2d::infNeg(), "SET: 0", 12);
  m_dro.arc_set->zindex = 210;
  m_dro.arc_set->color[0] = 247;
  m_dro.arc_set->color[1] = 104;
  m_dro.arc_set->color[2] = 15;

  m_dro.run_time = m_app->getRenderer().pushPrimitive<Text>(
    Point2d::infNeg(), "RUN: 0:0:0", 12);
  m_dro.run_time->zindex = 210;
  m_dro.run_time->color[0] = 247;
  m_dro.run_time->color[1] = 104;
  m_dro.run_time->color[2] = 15;

  control_view.m_torch_pointer =
    m_app->getRenderer().pushPrimitive<Circle>(Point2d::infNeg(), 5);
  control_view.m_torch_pointer->zindex = 500;
  control_view.m_torch_pointer->id = "torch_pointer";
  m_app->getRenderer().setColorByName(control_view.m_torch_pointer->color,
                                      "green");
  control_view.m_torch_pointer->matrix_callback =
    m_view->getTransformCallback();

  control_view.m_waypoint_pointer =
    m_app->getRenderer().pushPrimitive<Circle>(Point2d::infNeg(), 5);
  control_view.m_waypoint_pointer->zindex = 501;
  control_view.m_waypoint_pointer->id = "waypoint_pointer";
  m_app->getRenderer().setColorByName(control_view.m_waypoint_pointer->color,
                                      "yellow");
  control_view.m_waypoint_pointer->matrix_callback =
    m_view->getTransformCallback();
  control_view.m_waypoint_pointer->visible = false;

  // Events now handled through NcControlView delegation system

  m_app->getRenderer().pushTimer(100, [this]() { return updateTimer(); });

  // Perform initial layout of HMI elements based on current window size
  // This is necessary because buttons are created at Point2d::infNeg() and need
  // to be positioned. With tiling window managers like i3, the resize event may
  // not fire immediately, so we do an initial layout here.
  nlohmann::json resize_event = {
    { "width", m_app->getRenderer().getWindowSize().x },
    { "height", m_app->getRenderer().getWindowSize().y }
  };
  resizeCallback(resize_event);

  handleButton("Fit");
}

// New typed event handlers that delegate to existing JSON-based handlers
void NcHmi::handleKeyEvent(const KeyEvent& e, const InputState& input)
{
  // Convert to JSON format for existing handlers
  nlohmann::json json_event = { { "key", e.key },
                                { "scancode", e.scancode },
                                { "action", e.action },
                                { "mods", e.mods } };

  // Route to appropriate handler based on key
  switch (e.key) {
    case GLFW_KEY_ESCAPE:
      escapeKeyCallback(json_event);
      break;
    case GLFW_KEY_UP:
      upKeyCallback(json_event);
      break;
    case GLFW_KEY_DOWN:
      downKeyCallback(json_event);
      break;
    case GLFW_KEY_LEFT:
      leftKeyCallback(json_event);
      break;
    case GLFW_KEY_RIGHT:
      rightKeyCallback(json_event);
      break;
    case GLFW_KEY_PAGE_UP:
      pageUpKeyCallback(json_event);
      break;
    case GLFW_KEY_PAGE_DOWN:
      pageDownKeyCallback(json_event);
      break;
    default:
      // Key not handled by HMI
      break;
  }
}

void NcHmi::handleMouseMoveEvent(const MouseMoveEvent& e,
                                 const InputState&     input)
{
  // Convert to JSON format for existing handler
  nlohmann::json json_event = { { "pos", { { "x", e.x }, { "y", e.y } } } };

  mouseMotionCallback(json_event);
}

void NcHmi::handleWindowResizeEvent(const WindowResizeEvent& e,
                                    const InputState&        input)
{
  // Convert to JSON format for existing handler
  nlohmann::json json_event = { { "width", e.width }, { "height", e.height } };

  resizeCallback(json_event);
}
