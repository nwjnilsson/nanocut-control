#include "motion_controller.h"
#include "../util.h"
#include "NcControlView/NcControlView.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <sstream>

static constexpr float c_default_pierce_height = SCALE(2.8f);
static constexpr float c_default_cut_height = SCALE(1.75f);
static constexpr float c_default_pierce_delay_s = 1.2f;

void removeSubstrs(std::string& s, std::string p)
{
  std::string::size_type n = p.length();
  for (std::string::size_type i = s.find(p); i != std::string::npos;
       i = s.find(p))
    s.erase(i, n);
}

std::vector<std::string> split(std::string str, char delimiter)
{
  std::vector<std::string> internal;
  std::stringstream        ss(str);
  std::string              tok;
  while (std::getline(ss, tok, delimiter)) {
    internal.push_back(tok);
  }
  return internal;
}

MotionController::MotionController(NcControlView* nc_view, NcRender* renderer)
  : m_serial(
      "arduino|USB",
      std::bind(&MotionController::byteHandler, this, std::placeholders::_1),
      std::bind(&MotionController::lineHandler, this, std::placeholders::_1)),
    m_control_view(nc_view), m_renderer(renderer)
{
  if (!m_control_view || !m_renderer) {
    LOG_F(ERROR, "MotionController dependencies cannot be null!");
    throw std::invalid_argument("MotionController dependencies cannot be null");
  }

  // Initialize timing points
  m_torch_on_timer = std::chrono::steady_clock::now();
  m_arc_okay_timer = std::chrono::steady_clock::now();
  m_program_start_time = std::chrono::steady_clock::now();
}

void MotionController::initialize()
{
  m_controller_ready = false;
  m_torch_on = false;
  m_torch_on_timer = std::chrono::steady_clock::now();
  m_arc_okay_timer = std::chrono::steady_clock::now();
  m_abort_pending = false;
  m_handling_crash = false;
  m_program_start_time = std::chrono::steady_clock::now();
  m_arc_retry_count = 0;

  if (m_control_view->m_machine_parameters.homing_enabled == true) {
    m_needs_homed = true;
  }
  else {
    m_needs_homed = false;
  }

  m_renderer->pushTimer(100, std::bind(&MotionController::statusTimer, this));
}

void MotionController::tick()
{
  m_serial.tick();
  m_control_view->getDialogs().showControllerOfflineWindow(
    !m_serial.m_is_connected);

  if (m_needs_homed == true &&
      m_control_view->m_machine_parameters.homing_enabled == true &&
      m_serial.m_is_connected == true && m_controller_ready == true &&
      m_control_view->getDialogs().isControllerAlarmWindowVisible() ==
        false) {
    m_control_view->getDialogs().showControllerHomingWindow(true);
  }
  else {
    m_control_view->getDialogs().showControllerHomingWindow(false);
  }

  if (m_serial.m_is_connected == false) {
    m_controller_ready = false;
    m_needs_homed = true;
  }

  try {
    if (m_torch_on == true) {
      auto current_time = std::chrono::steady_clock::now();
      auto arc_stable_duration = std::chrono::milliseconds(static_cast<long>(
        m_control_view->m_machine_parameters.arc_stabilization_time +
        (m_callback_args.at("pierce_delay").get<double>() * 1000)));

      if ((current_time - m_arc_okay_timer) > arc_stable_duration) {
        // LOG_F(INFO, "Arc is stabalized!");
        /*
            Need to implement real-time command that takes 4 bytes.
            byte 1 -> THC set command
            byte 2 -> value low bit
            byte 3 -> value high bit
            byte 4 -> End THC set command

            This way THC can be changed on the fly withouth injecting into the
           gcode stream which under long line moves could b some time after this
           event occurs... Also would allow setting thc on the fly manually
        */
      }
    }
  }
  catch (...) {
    LOG_F(ERROR, "Error parsing callback args for Smart THC");
  }
}

void MotionController::shutdown()
{
  m_gcode_queue.clear();
  m_okay_callback = nullptr;
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
}

nlohmann::json MotionController::getRunTime() const
{
  unsigned long seconds{};
  unsigned long minutes{};
  unsigned long hours{};
  if (m_program_start_time != std::chrono::steady_clock::time_point{}) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - m_program_start_time);
    unsigned long m = duration.count();
    seconds = (m / 1000) % 60;
    minutes = (m / (1000 * 60)) % 60;
    hours = (m / (1000 * 60 * 60)) % 24;
  }
  return { { "seconds", seconds }, { "minutes", minutes }, { "hours", hours } };
}

void MotionController::sendCommand(const std::string& cmd)
{
  if (cmd == "abort") {
    LOG_F(INFO, "Aborting!");
    send("!");
    m_abort_pending = true;
    m_okay_callback = nullptr;
    m_probe_callback = nullptr;
    m_motion_sync_callback = nullptr;
    m_arc_okay_callback = nullptr;
    m_gcode_queue.clear();
  }
  else if (cmd == "home") {
    m_okay_callback = [this]() { homingDoneCallback(); };
    send("$H");
  }
}

void MotionController::pushGCode(const std::string& gcode)
{
  m_gcode_queue.push_back(gcode);
}

void MotionController::runStack()
{
  m_program_start_time = std::chrono::steady_clock::now();
  runPop();
  m_okay_callback = [this]() { runPop(); };
}

void MotionController::abort() { sendCommand("abort"); }

void MotionController::home() { sendCommand("home"); }

void MotionController::send(const std::string& s)
{
  if (m_controller_ready == true) {
    std::string cleaned = s;
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), ' '), cleaned.end());
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), '\n'), cleaned.end());
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());
    m_serial.sendString(cleaned + "\n");
  }
}

void MotionController::sendWithCRC(const std::string& s)
{
  if (m_controller_ready == true) {
    std::string cleaned = s;
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), ' '), cleaned.end());
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), '\n'), cleaned.end());
    cleaned.erase(remove(cleaned.begin(), cleaned.end(), '\r'), cleaned.end());
    uint32_t checksum = calculateCRC32(0, cleaned.c_str(), cleaned.size());
    m_serial.sendString(cleaned + "*" + std::to_string(checksum) + "\n");
  }
}

void MotionController::sendRealTime(char s)
{
  if (m_controller_ready == true) {
    LOG_F(INFO, "(motion_controller_send_rt) Byte: %c\n", s);
    m_serial.sendByte(s);
  }
}

void MotionController::triggerReset()
{
  LOG_F(INFO, "Resetting Motion Controller!");
  m_serial.m_serial.setDTR(true);
  m_serial.delay(100);
  m_serial.m_serial.setDTR(false);
  m_controller_ready = false;
}

// Instance methods for handling serial data
bool MotionController::byteHandler(uint8_t b)
{
  if (m_controller_ready == true) {
    if (b == '>') {
      if (m_okay_callback) {
        m_okay_callback();
      }
      return true;
    }
  }
  return false;
}

void MotionController::lineHandler(std::string line)
{
  if (m_controller_ready == true) {
    if (line.find("{") != std::string::npos) {
      try {
        m_dro_data = nlohmann::json::parse(line);
        if (m_dro_data["ARC_OK"] == false) {
          if (m_arc_okay_callback) {
            m_arc_okay_callback();
            m_arc_okay_callback = nullptr;
            m_arc_okay_timer = std::chrono::steady_clock::now();
          }
        }
        if (m_abort_pending == true &&
            static_cast<bool>(m_dro_data["IN_MOTION"]) == false) {
          logRuntime();
          m_serial.delay(300);
          LOG_F(INFO, "Handling pending abort -> Sending Reset!");
          m_serial.sendByte(0x18);
          m_serial.delay(300);
          m_abort_pending = false;
          m_program_start_time = std::chrono::steady_clock::time_point{};
          m_arc_retry_count = 0;
          m_torch_on = false;
          m_handling_crash = false;
          m_controller_ready = false;
        }
      }
      catch (...) {
        LOG_F(ERROR, "Error parsing DRO JSON data!");
      }
    }
    else if (line.find("[MSG:'$H'|'$X' to unlock]") != std::string::npos) {
      if (m_control_view->m_machine_parameters.homing_enabled == false) {
        LOG_F(
          WARNING,
          "Controller lockout for unknown reason. Automatically unlocking...");
        send("$X");
      }
      else {
        LOG_F(WARNING,
              "Controller lockout for unknown reason. Machine will need to be "
              "re-homed...");
        m_needs_homed = true;
      }
    }
    else if (line.find("[CHECKSUM_FAILURE]") != std::string::npos) {
      auto current_time = std::chrono::steady_clock::now();
      auto last_error_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
          current_time -
          std::chrono::steady_clock::time_point{
            std::chrono::milliseconds{ m_last_checksum_error } });

      if (m_last_checksum_error > 0 &&
          last_error_duration < std::chrono::milliseconds{ 100 }) {
        m_control_view->getDialogs().setInfoValue(
          "Program aborted due to multiple communication "
          "checksum errors in a short period of time!");
        sendCommand("abort");
      }
      else {
        LOG_F(WARNING,
              "(motion_control) Communication checksum error, resending last "
              "send communication!");
        m_last_checksum_error =
          std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time.time_since_epoch())
            .count();
        m_serial.resend();
      }
    }
    else if (line.find("error") != std::string::npos) {
      removeSubstrs(line, "error:");
      logControllerError(atoi(line.c_str()));
    }
    else if (line.find("ALARM") != std::string::npos) {
      removeSubstrs(line, "ALARM:");
      handleAlarm(atoi(line.c_str()));
    }
    else if (line.find("[CRASH]") != std::string::npos &&
             m_handling_crash == false) {
      auto current_time = std::chrono::steady_clock::now();
      auto torch_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(current_time -
                                                              m_torch_on_timer);

      if (m_torch_on == true &&
          torch_duration > std::chrono::milliseconds{ 2000 }) {
        m_control_view->getDialogs().setInfoValue(
          "Program was aborted because torch crash was detected!");
        sendCommand("abort");
        m_handling_crash = true;
      }
    }
    else if (line.find("[PRB") != std::string::npos) {
      LOG_F(INFO, "Probe finished - Running callback!");
      if (m_probe_callback) {
        m_probe_callback();
      }
    }
    else if (line.find("Grbl") != std::string::npos) {
      LOG_F(WARNING,
            "Received Grbl start message while in ready state. Was the "
            "controller reset?");
    }
    else {
      LOG_F(WARNING, "Unidentified line received - %s", line.c_str());
    }
  }
  else if (m_controller_ready == false) {
    if (line.find("Grbl") != std::string::npos) {
      LOG_F(INFO, "Controller ready!");
      m_controller_ready = true;
      m_gcode_queue.push_back(
        "G10 L2 P0 X" +
        std::to_string(m_control_view->m_machine_parameters.work_offset[0]) +
        " Y" +
        std::to_string(m_control_view->m_machine_parameters.work_offset[1]) +
        " Z" +
        std::to_string(m_control_view->m_machine_parameters.work_offset[2]));
      m_gcode_queue.push_back("M30");
      runStack();
    }
    else if (line.find("[MSG:'$H'|'$X' to unlock]") != std::string::npos) {
      if (m_control_view->m_machine_parameters.homing_enabled == true) {
        LOG_F(INFO, "Controller ready, but needs homing.");
        m_controller_ready = true;
        m_needs_homed = true;
        m_gcode_queue.push_back(
          "G10 L2 P0 X" +
          std::to_string(
            m_control_view->m_machine_parameters.work_offset[0]) +
          " Y" +
          std::to_string(
            m_control_view->m_machine_parameters.work_offset[1]) +
          " Z" +
          std::to_string(
            m_control_view->m_machine_parameters.work_offset[2]));
        m_gcode_queue.push_back("M30");
        runStack();
      }
      else {
        LOG_F(WARNING, "Controller locked out. Unlocking...");
        send("$X");
      }
    }
  }
}

// Private helper methods implementation

void MotionController::runPop()
{
  if (m_gcode_queue.size() > 0) {
    std::string line = m_gcode_queue.front();
    m_gcode_queue.pop_front();

    if (line.find("fire_torch") != std::string::npos) {
      LOG_F(
        INFO,
        "[fire_torch] Sending probing cycle! - Waiting for probe to finish!");
      std::vector<std::string> args = split(line, ' ');
      if (args.size() == 4) {
        m_callback_args["pierce_height"] =
          static_cast<double>(atof(args[1].c_str()));
        m_callback_args["pierce_delay"] =
          static_cast<double>(atof(args[2].c_str()));
        m_callback_args["cut_height"] =
          static_cast<double>(atof(args[3].c_str()));
      }
      else {
        LOG_F(ERROR, "[fire_torch] Invalid arguments - Using defaults!");
        m_callback_args["pierce_height"] = c_default_pierce_height;
        m_callback_args["pierce_delay"] = c_default_pierce_delay_s;
        m_callback_args["cut_height"] = c_default_cut_height;
      }

      if (m_arc_retry_count > 3) {
        LOG_F(INFO,
              "[run_pop] Arc retry max count reached. Retracting and aborting "
              "program!");
        m_okay_callback = nullptr;
        m_motion_sync_callback = [this]() { torchOffAndAbort(); };
        m_control_view->getDialogs().setInfoValue(
          "Arc Strike Retry max count expired!\nLikely causes are:\n1. Bad or "
          "worn out consumables.\n2. Faulty work clamp connection or wire\n3. "
          "Inadequate pressure and/or moisture in the air system\n4. Dirty "
          "material and/or non-conductive surface\n5. Faulty Cutting unit, eg. "
          "Plasma Power Unit is worn out or broken");
      }
      else {
        m_okay_callback = nullptr;
        m_probe_callback = [this]() { raiseToPierceHeightAndFireTorch(); };
        probe();
      }
    }
    else if (line.find("touch_torch") != std::string::npos) {
      LOG_F(
        INFO,
        "[touch_torch] Sending probing cycle! - Waiting for probe to finish!");
      std::vector<std::string> args = split(line, ' ');
      if (args.size() == 4) {
        m_callback_args["pierce_height"] =
          static_cast<double>(atof(args[1].c_str()));
        m_callback_args["pierce_delay"] =
          static_cast<double>(atof(args[2].c_str()));
        m_callback_args["cut_height"] =
          static_cast<double>(atof(args[3].c_str()));
      }
      else {
        LOG_F(WARNING, "[touch_torch] No arguments - Using default!");
        m_callback_args["pierce_height"] = c_default_pierce_height;
        m_callback_args["pierce_delay"] = c_default_pierce_delay_s;
        m_callback_args["cut_height"] = c_default_cut_height;
      }
      m_okay_callback = nullptr;
      m_probe_callback = [this]() { touchTorchAndBackOff(); };
      probe();
    }
    else if (line.find("WAIT_FOR_ARC_OKAY") != std::string::npos) {
      LOG_F(INFO,
            "[run_pop] Setting arc_okay callback and arc_okay expire timer => "
            "fires in %.4f ms!",
            (3 + static_cast<double>(m_callback_args["pierce_delay"])) * 1000);
      m_renderer->pushTimer(
        (3 + static_cast<double>(m_callback_args["pierce_delay"])) * 1000,
        std::bind(&MotionController::arcOkayExpireTimer, this));
      m_arc_okay_callback = [this]() { lowerToCutHeightAndRunProgram(); };
      m_okay_callback = nullptr;
      m_probe_callback = nullptr;
    }
    else if (line.find("torch_off") != std::string::npos) {
      m_okay_callback = nullptr;
      m_motion_sync_callback = [this]() { torchOffAndRetract(); };
    }
    else if (line.find("M30") != std::string::npos) {
      m_motion_sync_callback = [this]() { programFinished(); };
      m_okay_callback = nullptr;
      m_probe_callback = nullptr;
    }
    else {
      line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
      LOG_F(INFO, "(runpop) sending %s", line.c_str());
      sendWithCRC(line);
    }
  }
  else {
    m_okay_callback = nullptr;
  }
}

void MotionController::probe()
{
  sendWithCRC(
    "G38.3Z" +
    std::to_string(m_control_view->m_machine_parameters.machine_extents[2] -
                   m_control_view->m_machine_parameters.homing_pull_off) +
    "F" +
    std::to_string(m_control_view->m_machine_parameters.z_probe_feedrate));
}

bool MotionController::arcOkayExpireTimer()
{
  if (m_abort_pending == true)
    return false;

  if (m_arc_okay_callback) {
    m_okay_callback = [this]() { runPop(); };
    m_probe_callback = nullptr;
    m_motion_sync_callback = nullptr;
    m_arc_okay_callback = nullptr;
    m_torch_on = false;

    m_gcode_queue.push_front("fire_torch " +
                             to_string_strip_zeros(static_cast<double>(
                               m_callback_args["pierce_height"])) +
                             " " +
                             to_string_strip_zeros(static_cast<double>(
                               m_callback_args["pierce_delay"])) +
                             " " +
                             to_string_strip_zeros(static_cast<double>(
                               m_callback_args["cut_height"])));
    m_gcode_queue.push_front("G0 Z0");
    m_gcode_queue.push_front("M5");

    m_arc_retry_count++;
    LOG_F(WARNING,
          "No arc OK received! Running retry callback => "
          "arc_okay_expire_timer() - retry count: %d",
          m_arc_retry_count);
    runPop();
  }
  return false;
}

bool MotionController::statusTimer()
{
  if (m_controller_ready == true) {
    if (m_dro_data.contains("STATUS")) {
      auto current_time = std::chrono::steady_clock::now();
      auto program_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
          current_time - m_program_start_time);

      if (m_motion_sync_callback &&
          static_cast<bool>(m_dro_data["IN_MOTION"]) == false &&
          program_duration > std::chrono::milliseconds{ 500 }) {
        LOG_F(INFO, "Motion is synced, calling pending callback!");
        m_motion_sync_callback();
      }
    }
  }
  send("?");
  return true;
}

// Callback implementations (formerly global functions)

void MotionController::programFinished()
{
  LOG_F(INFO, "M30 Program finished!");
  logRuntime();
  m_program_start_time = std::chrono::steady_clock::time_point{};
  m_arc_retry_count = 0;
  m_okay_callback = nullptr;
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_gcode_queue.clear();
}

void MotionController::homingDoneCallback()
{
  m_okay_callback = nullptr;
  m_needs_homed = false;
  LOG_F(INFO, "Homing finished! Saving Z work offset...");
  m_control_view->m_machine_parameters.work_offset[2] =
    static_cast<float>(getDRO()["MCS"]["z"]);
  m_gcode_queue.push_back("G10 L20 P0 Z0");
  m_gcode_queue.push_back("M30");
  runStack();
  saveParameters();
}

void MotionController::lowerToCutHeightAndRunProgram()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_arc_retry_count = 0;
  m_gcode_queue.push_front(
    "$T=" + to_string_strip_zeros(
              m_control_view->m_machine_parameters.thc_set_value));
  m_gcode_queue.push_front("G90");
  m_gcode_queue.push_front(
    "G91G0 Z" + to_string_strip_zeros(
                  static_cast<double>(m_callback_args["pierce_height"]) -
                  static_cast<double>(m_callback_args["cut_height"])));
  m_gcode_queue.push_front("G4P" + to_string_strip_zeros(static_cast<double>(
                                     m_callback_args["pierce_delay"])));
  LOG_F(INFO, "Running callback => lower_to_cut_height_and_run_program()");
  runPop();
}

void MotionController::raiseToPierceHeightAndFireTorch()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;

  m_gcode_queue.push_front("WAIT_FOR_ARC_OKAY");
  m_gcode_queue.push_front("G90");
  m_gcode_queue.push_front("M3S1000");
  m_gcode_queue.push_front("G0Z-" + to_string_strip_zeros(static_cast<double>(
                                      m_callback_args["pierce_height"])));
  m_gcode_queue.push_front(
    "G0Z-" + to_string_strip_zeros(
               m_control_view->m_machine_parameters.floating_head_backlash));
  m_gcode_queue.push_front("G91");
  LOG_F(INFO, "Running callback => raise_to_pierce_height_and_fire_torch()");
  m_torch_on = true;
  m_torch_on_timer = std::chrono::steady_clock::now();
  runPop();
}

void MotionController::touchTorchAndBackOff()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;

  m_gcode_queue.push_front("G90");
  m_gcode_queue.push_front("G0Z-0.5");
  m_gcode_queue.push_front(
    "G0Z-" + to_string_strip_zeros(
               m_control_view->m_machine_parameters.floating_head_backlash));
  m_gcode_queue.push_front("G91");
  LOG_F(INFO, "Touching off torch and dry running!");
  runPop();
}

void MotionController::torchOffAndAbort()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_torch_on = false;

  m_gcode_queue.push_front("M30");
  m_gcode_queue.push_front("G0 Z0");
  m_gcode_queue.push_front("M5");
  LOG_F(INFO, "Shutting torch off, retracting, and aborting!");
  runPop();
}

void MotionController::torchOffAndRetract()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_torch_on = false;

  m_gcode_queue.push_front("$T=0.0");
  m_gcode_queue.push_front("G0 Z0");
  m_gcode_queue.push_front("M5");
  LOG_F(INFO, "Shutting torch off and retracting!");
  runPop();
}

uint32_t
MotionController::calculateCRC32(uint32_t crc, const char* buf, size_t len)
{
  const uint32_t CRC_POLY = 0x82f63b78;
  int            k;
  crc = ~crc;
  while (len--) {
    crc ^= *buf++;
    for (k = 0; k < 8; k++)
      crc = crc & 1 ? (crc >> 1) ^ CRC_POLY : crc >> 1;
  }
  return ~crc;
}

void MotionController::logRuntime()
{
  auto current_time = std::chrono::steady_clock::now();
  auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
    current_time - m_program_start_time);
  unsigned long m = duration.count();
  unsigned long seconds = (m / 1000) % 60;
  unsigned long minutes = (m / (1000 * 60)) % 60;
  unsigned long hours = (m / (1000 * 60 * 60)) % 24;
  LOG_F(INFO,
        "Logging program run time %luh %lum %lus to total",
        hours,
        minutes,
        seconds);

  nlohmann::json runtime_json = m_renderer->parseJsonFromFile(
    m_renderer->getConfigDirectory() + "runtime.json");

  nlohmann::json runtime;
  if (runtime_json != nullptr) {
    try {
      runtime["total_milliseconds"] =
        static_cast<unsigned long>(runtime_json["total_milliseconds"]) + m;
    }
    catch (...) {
      LOG_F(WARNING, "Error parsing runtime file!");
    }
  }
  else {
    runtime["total_milliseconds"] = m;
  }

  m_renderer->dumpJsonToFile(m_renderer->getConfigDirectory() + "runtime.json",
                             runtime);
}

void MotionController::logControllerError(int error)
{
  std::string ret;
  switch (error) {
    case 1:
      ret =
        "G-code words consist of a letter and a value. Letter was not found";
      break;
    case 2:
      ret = "Numeric value format is not valid or missing an expected value.";
      break;
    case 3:
      ret = "System command was not recognized or supported.";
      break;
    case 4:
      ret = "Negative value received for an expected positive value.";
      break;
    case 5:
      ret = "Homing cycle is not enabled via settings.";
      break;
    case 6:
      ret = "Minimum step pulse time must be greater than 3usec";
      break;
    case 7:
      ret = "EEPROM read failed. Reset and restored to default values.";
      break;
    case 8:
      ret = "Real-Time command cannot be used unless machine is IDLE. Ensures "
            "smooth operation during a job.";
      break;
    case 9:
      ret = "G-code locked out during alarm or jog state. Z Probe input may "
            "have been engaged before probing cycle began.";
      m_needs_homed = true;
      break;
    case 10:
      ret = "Soft limits cannot be enabled without homing also enabled.";
      break;
    case 11:
      ret = "Max characters per line exceeded. Line was not processed and "
            "executed.";
      break;
    case 12:
      ret = "Setting value exceeds the maximum step rate supported.";
      break;
    case 13:
      ret = "Safety door detected as opened and door state initiated.";
      break;
    case 14:
      ret = "Build info or startup line exceeded EEPROM line length limit.";
      break;
    case 15:
      ret = "Jog target exceeds machine travel. Command ignored.";
      break;
    case 16:
      ret = "Jog command with no '=' or contains prohibited g-code.";
      break;
    case 17:
      ret = "Laser mode requires PWM output.";
      break;
    case 18:
      ret = "Target voltage is invalid given the set voltage divider, or the "
            "current state does not allow setting THC target.";
      break;
    case 20:
      ret = "Unsupported or invalid g-code command found in block.";
      break;
    case 21:
      ret =
        "More than one g-code command from same modal group found in block.";
      break;
    case 22:
      ret = "Feed rate has not yet been set or is undefined.";
      break;
    case 23:
      ret = "G-code command in block requires an integer value.";
      break;
    case 24:
      ret = "Two G-code commands that both require the use of the XYZ axis "
            "words were detected in the block.";
      break;
    case 25:
      ret = "A G-code word was repeated in the block.";
      break;
    case 26:
      ret = "A G-code command implicitly or explicitly requires XYZ axis words "
            "in the block, but none were detected.";
      break;
    case 27:
      ret =
        "N line number value is not within the valid range of 1 - 9,999,999.";
      break;
    case 28:
      ret = "A G-code command was sent, but is missing some required P or L "
            "value words in the line.";
      break;
    case 29:
      ret = "System only supports six work coordinate systems G54-G59. G59.1, "
            "G59.2, and G59.3 are not supported.";
      break;
    case 30:
      ret = "The G53 G-code command requires either a G0 seek or G1 feed "
            "motion mode to be active. A different motion was active.";
      break;
    case 31:
      ret = "There are unused axis words in the block and G80 motion mode "
            "cancel is active.";
      break;
    case 32:
      ret = "A G2 or G3 arc was commanded but there are no XYZ axis words in "
            "the selected plane to trace the arc.";
      break;
    case 33:
      ret = "The motion command has an invalid target. G2, G3, and G38.2 "
            "generates this error, if the arc is impossible to generate or if "
            "the probe target is the current position.";
      break;
    case 34:
      ret = "A G2 or G3 arc, traced with the radius definition, had a "
            "mathematical error when computing the arc geometry. Try either "
            "breaking up the arc into semi-circles or quadrants, or redefine "
            "them with the arc offset definition.";
      break;
    case 35:
      ret = "A G2 or G3 arc, traced with the offset definition, is missing the "
            "IJK offset word in the selected plane to trace the arc.";
      break;
    case 36:
      ret = "There are unused, leftover G-code words that aren't used by any "
            "command in the block.";
      break;
    case 37:
      ret = "The G43.1 dynamic tool length offset command cannot apply an "
            "offset to an axis other than its configured axis. The Grbl "
            "default axis is the Z-axis.";
      break;
    case 38:
      ret = "Tool number greater than max supported value.";
      break;
    case 100:
      ret = "Communication Redundancy Check Failed. Program Aborted to ensure "
            "unintended behavior does not occur!";
      break;
    default:
      ret =
        "Firmware Error is unknown. Motion Controller may be malfunctioning or "
        "Electrical noise is interfering with serial communication!";
      break;
  }
  LOG_F(ERROR, "Firmware Error %d => %s", error, ret.c_str());
  m_control_view->getDialogs().setInfoValue(ret.c_str());
  m_gcode_queue.clear();
}

void MotionController::handleAlarm(int alarm)
{
  std::string ret;
  switch (alarm) {
    case 1:
      ret = "Hard limit triggered. Machine position is likely lost due to "
            "sudden and immediate halt. Re-homing is highly recommended.";
      m_needs_homed = true;
      break;
    case 2:
      ret = "G-code motion target exceeds machine travel. Machine position "
            "safely retained. Alarm may be unlocked.";
      break;
    case 3:
      ret = "Reset while in motion. Grbl cannot guarantee position. Lost steps "
            "are likely. Re-homing is highly recommended.";
      m_needs_homed = true;
      break;
    case 4:
      ret = "Probe fail. The probe is not in the expected initial state before "
            "starting probe cycle, where G38.2 and G38.3 is not triggered and "
            "G38.4 and G38.5 is triggered.";
      break;
    case 5:
      ret = "Probe fail. Probe did not contact the workpiece within the "
            "programmed travel for G38.2 and G38.4.";
      break;
    case 6:
      ret = "Homing fail. Reset during active homing cycle.";
      m_needs_homed = true;
      break;
    case 7:
      ret = "Homing fail. Safety door was opened during active homing cycle.";
      m_needs_homed = true;
      break;
    case 8:
      ret = "Homing fail. Cycle failed to clear limit switch when pulling off. "
            "Try increasing pull-off setting or check wiring.";
      m_needs_homed = true;
      break;
    case 9:
      ret = "Homing fail. Could not find limit switch within search distance. "
            "Defined as 1.5 * max_travel on search and 5 * pulloff on locate "
            "phases.";
      m_needs_homed = true;
      break;
    default:
      ret = "Alarm code is unknown. Motion Controller may be malfunctioning or "
            "Electrical noise is interfering with serial communication!";
      break;
  }
  m_controller_ready = false;
  LOG_F(ERROR, "Alarm %d => %s", alarm, ret.c_str());
  m_control_view->getDialogs().setControllerAlarmValue(ret);
  m_control_view->getDialogs().showControllerAlarmWindow(true);
  m_gcode_queue.clear();
}

void MotionController::saveParameters()
{
  nlohmann::json preferences;
  preferences["work_offset"]["x"] =
    m_control_view->m_machine_parameters.work_offset[0];
  preferences["work_offset"]["y"] =
    m_control_view->m_machine_parameters.work_offset[1];
  preferences["work_offset"]["z"] =
    m_control_view->m_machine_parameters.work_offset[2];
  preferences["machine_extents"]["x"] =
    m_control_view->m_machine_parameters.machine_extents[0];
  preferences["machine_extents"]["y"] =
    m_control_view->m_machine_parameters.machine_extents[1];
  preferences["machine_extents"]["z"] =
    m_control_view->m_machine_parameters.machine_extents[2];
  preferences["cutting_extents"]["x1"] =
    m_control_view->m_machine_parameters.cutting_extents[0];
  preferences["cutting_extents"]["y1"] =
    m_control_view->m_machine_parameters.cutting_extents[1];
  preferences["cutting_extents"]["x2"] =
    m_control_view->m_machine_parameters.cutting_extents[2];
  preferences["cutting_extents"]["y2"] =
    m_control_view->m_machine_parameters.cutting_extents[3];
  preferences["axis_scale"]["x"] =
    m_control_view->m_machine_parameters.axis_scale[0];
  preferences["axis_scale"]["y"] =
    m_control_view->m_machine_parameters.axis_scale[1];
  preferences["axis_scale"]["z"] =
    m_control_view->m_machine_parameters.axis_scale[2];
  preferences["max_vel"]["x"] =
    m_control_view->m_machine_parameters.max_vel[0];
  preferences["max_vel"]["y"] =
    m_control_view->m_machine_parameters.max_vel[1];
  preferences["max_vel"]["z"] =
    m_control_view->m_machine_parameters.max_vel[2];
  preferences["max_accel"]["x"] =
    m_control_view->m_machine_parameters.max_accel[0];
  preferences["max_accel"]["y"] =
    m_control_view->m_machine_parameters.max_accel[1];
  preferences["max_accel"]["z"] =
    m_control_view->m_machine_parameters.max_accel[2];
  preferences["junction_deviation"] =
    m_control_view->m_machine_parameters.junction_deviation;
  preferences["arc_stabilization_time"] =
    m_control_view->m_machine_parameters.arc_stabilization_time;
  preferences["arc_voltage_divider"] =
    m_control_view->m_machine_parameters.arc_voltage_divider;
  preferences["floating_head_backlash"] =
    m_control_view->m_machine_parameters.floating_head_backlash;
  preferences["z_probe_feedrate"] =
    m_control_view->m_machine_parameters.z_probe_feedrate;
  preferences["axis_invert"]["x"] =
    m_control_view->m_machine_parameters.axis_invert[0];
  preferences["axis_invert"]["y1"] =
    m_control_view->m_machine_parameters.axis_invert[1];
  preferences["axis_invert"]["y2"] =
    m_control_view->m_machine_parameters.axis_invert[2];
  preferences["axis_invert"]["z"] =
    m_control_view->m_machine_parameters.axis_invert[3];
  preferences["soft_limits_enabled"] =
    m_control_view->m_machine_parameters.soft_limits_enabled;
  preferences["homing_enabled"] =
    m_control_view->m_machine_parameters.homing_enabled;
  preferences["homing_dir_invert"]["x"] =
    m_control_view->m_machine_parameters.homing_dir_invert[0];
  preferences["homing_dir_invert"]["y"] =
    m_control_view->m_machine_parameters.homing_dir_invert[1];
  preferences["homing_dir_invert"]["z"] =
    m_control_view->m_machine_parameters.homing_dir_invert[2];
  preferences["homing_feed"] =
    m_control_view->m_machine_parameters.homing_feed;
  preferences["homing_seek"] =
    m_control_view->m_machine_parameters.homing_seek;
  preferences["homing_debounce"] =
    m_control_view->m_machine_parameters.homing_debounce;
  preferences["homing_pull_off"] =
    m_control_view->m_machine_parameters.homing_pull_off;
  preferences["invert_limit_pins"] =
    m_control_view->m_machine_parameters.invert_limit_pins;
  preferences["invert_probe_pin"] =
    m_control_view->m_machine_parameters.invert_probe_pin;
  preferences["invert_step_enable"] =
    m_control_view->m_machine_parameters.invert_step_enable;
  preferences["precise_jog_units"] =
    m_control_view->m_machine_parameters.precise_jog_units;

  // Update machine plane parameters
  m_control_view->m_machine_plane->m_bottom_left.x = 0;
  m_control_view->m_machine_plane->m_bottom_left.y = 0;
  m_control_view->m_machine_plane->m_width =
    m_control_view->m_machine_parameters.machine_extents[0];
  m_control_view->m_machine_plane->m_height =
    m_control_view->m_machine_parameters.machine_extents[1];
  m_control_view->m_cuttable_plane->m_bottom_left.x =
    m_control_view->m_machine_parameters.cutting_extents[0];
  m_control_view->m_cuttable_plane->m_bottom_left.y =
    m_control_view->m_machine_parameters.cutting_extents[1];
  m_control_view->m_cuttable_plane->m_width =
    (m_control_view->m_machine_parameters.machine_extents[0] +
     m_control_view->m_machine_parameters.cutting_extents[2]) -
    m_control_view->m_machine_parameters.cutting_extents[0];
  m_control_view->m_cuttable_plane->m_height =
    (m_control_view->m_machine_parameters.machine_extents[1] +
     m_control_view->m_machine_parameters.cutting_extents[3]) -
    m_control_view->m_machine_parameters.cutting_extents[1];

  try {
    std::ofstream out(m_renderer->getConfigDirectory() +
                      "m_machine_parameters.json");
    out << preferences.dump();
    out.close();
  }
  catch (...) {
    LOG_F(ERROR, "Could not write parameters file!");
  }
}

void MotionController::writeParametersToController()
{
  if (m_serial.m_is_connected) {
    uint8_t dir_invert_mask = 0b00000000;
    if (m_control_view->m_machine_parameters.axis_invert[0])
      dir_invert_mask |= 0b00000001;
    if (m_control_view->m_machine_parameters.axis_invert[1])
      dir_invert_mask |= 0b00000010;
    if (m_control_view->m_machine_parameters.axis_invert[3])
      dir_invert_mask |= 0b00000100;
    if (m_control_view->m_machine_parameters.axis_invert[2])
      dir_invert_mask |= 0b00001000;

    uint8_t homing_dir_invert_mask = 0b00000000;
    if (m_control_view->m_machine_parameters.homing_dir_invert[0])
      homing_dir_invert_mask |= 0b00000001;
    if (m_control_view->m_machine_parameters.homing_dir_invert[1])
      homing_dir_invert_mask |= 0b00000010;
    if (m_control_view->m_machine_parameters.homing_dir_invert[2])
      homing_dir_invert_mask |= 0b00000100;

    m_gcode_queue.push_back("$X");
    m_gcode_queue.push_back("$0=" + std::to_string(10));
    m_gcode_queue.push_back("$1=" + std::to_string(255));
    m_gcode_queue.push_back("$3=" + std::to_string(dir_invert_mask));
    m_gcode_queue.push_back(
      "$4=" + std::to_string(static_cast<int>(
                m_control_view->m_machine_parameters.invert_step_enable)));
    m_gcode_queue.push_back(
      "$5=" + std::to_string(static_cast<int>(
                m_control_view->m_machine_parameters.invert_limit_pins)));
    m_gcode_queue.push_back(
      "$6=" + std::to_string(static_cast<int>(
                m_control_view->m_machine_parameters.invert_probe_pin)));
    m_gcode_queue.push_back("$10=" + std::to_string(3));
    m_gcode_queue.push_back(
      "$11=" + std::to_string(
                 m_control_view->m_machine_parameters.junction_deviation));
    m_gcode_queue.push_back("$12=" + std::to_string(0.002));
    m_gcode_queue.push_back("$13=" + std::to_string(0));
    m_gcode_queue.push_back(
      "$22=" + std::to_string(static_cast<int>(
                 m_control_view->m_machine_parameters.homing_enabled)));
    m_gcode_queue.push_back(
      "$20=" + std::to_string(static_cast<int>(
                 m_control_view->m_machine_parameters.soft_limits_enabled)));
    m_gcode_queue.push_back("$21=" + std::to_string(0));
    m_gcode_queue.push_back("$23=" + std::to_string(homing_dir_invert_mask));
    m_gcode_queue.push_back(
      "$24=" +
      std::to_string(m_control_view->m_machine_parameters.homing_feed));
    m_gcode_queue.push_back(
      "$25=" +
      std::to_string(m_control_view->m_machine_parameters.homing_seek));
    m_gcode_queue.push_back(
      "$26=" +
      std::to_string(m_control_view->m_machine_parameters.homing_debounce));
    m_gcode_queue.push_back(
      "$33=" + std::to_string(
                 m_control_view->m_machine_parameters.arc_voltage_divider));
    m_gcode_queue.push_back(
      "$27=" +
      std::to_string(m_control_view->m_machine_parameters.homing_pull_off));
    m_gcode_queue.push_back(
      "$100=" +
      std::to_string(m_control_view->m_machine_parameters.axis_scale[0]));
    m_gcode_queue.push_back(
      "$101=" +
      std::to_string(m_control_view->m_machine_parameters.axis_scale[1]));
    m_gcode_queue.push_back(
      "$102=" +
      std::to_string(m_control_view->m_machine_parameters.axis_scale[2]));
    m_gcode_queue.push_back(
      "$110=" +
      std::to_string(m_control_view->m_machine_parameters.max_vel[0]));
    m_gcode_queue.push_back(
      "$111=" +
      std::to_string(m_control_view->m_machine_parameters.max_vel[1]));
    m_gcode_queue.push_back(
      "$112=" +
      std::to_string(m_control_view->m_machine_parameters.max_vel[2]));
    m_gcode_queue.push_back(
      "$120=" +
      std::to_string(m_control_view->m_machine_parameters.max_accel[0]));
    m_gcode_queue.push_back(
      "$121=" +
      std::to_string(m_control_view->m_machine_parameters.max_accel[1]));
    m_gcode_queue.push_back(
      "$122=" +
      std::to_string(m_control_view->m_machine_parameters.max_accel[2]));
    m_gcode_queue.push_back(
      "$130=" + std::to_string(fabs(
                  m_control_view->m_machine_parameters.machine_extents[0])));
    m_gcode_queue.push_back(
      "$131=" + std::to_string(fabs(
                  m_control_view->m_machine_parameters.machine_extents[1])));
    m_gcode_queue.push_back(
      "$132=" + std::to_string(fabs(
                  m_control_view->m_machine_parameters.machine_extents[2])));
    m_gcode_queue.push_back("M30");
    runStack();
  }
}
