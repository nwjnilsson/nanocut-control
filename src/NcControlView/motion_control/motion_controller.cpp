#include "motion_controller.h"
#include "../../NcApp/NcApp.h"
#include "../util.h"
#include "NcControlView/NcControlView.h"
#include <algorithm>
#include <chrono>
#include <deque>
#include <sstream>

static constexpr float c_default_pierce_height = 3.0f;
static constexpr float c_default_cut_height = 1.5f;
static constexpr float c_default_pierce_delay_s = 1.0f;

void removeSubstrs(std::string& s, std::string p)
{
  std::string::size_type n = p.length();
  for (std::string::size_type i = s.find(p); i != std::string::npos;
       i = s.find(p))
    s.erase(i, n);
}

// Helper function to parse machine status string into enum
static MachineStatus parseMachineStatus(const std::string& status_str)
{
  if (status_str == "Idle" || status_str == "IDLE")
    return MachineStatus::Idle;
  if (status_str == "Cycle" || status_str == "CYCLE")
    return MachineStatus::Cycle;
  if (status_str == "Alarm" || status_str == "ALARM")
    return MachineStatus::Alarm;
  if (status_str == "Homing" || status_str == "HOMING")
    return MachineStatus::Homing;
  if (status_str == "Hold" || status_str == "HOLD")
    return MachineStatus::Hold;
  if (status_str == "Check" || status_str == "CHECK")
    return MachineStatus::Check;
  if (status_str == "Door" || status_str == "DOOR")
    return MachineStatus::Door;

  LOG_F(WARNING, "Unknown machine status: %s", status_str.c_str());
  return MachineStatus::Unknown;
}

// Helper function to parse GRBL message type for efficient dispatch
static GrblMessageType parseMessageType(const std::string& line)
{
  // Check for JSON data (most common case first for performance)
  if (line.find("{") != std::string::npos)
    return GrblMessageType::DROData;

  // Check for specific message patterns
  if (line.find("[MSG:'$H'|'$X' to unlock]") != std::string::npos)
    return GrblMessageType::UnlockRequired;

  if (line.find("[CHECKSUM_FAILURE]") != std::string::npos)
    return GrblMessageType::ChecksumFailure;

  if (line.find("error") != std::string::npos)
    return GrblMessageType::Error;

  if (line.find("ALARM") != std::string::npos)
    return GrblMessageType::Alarm;

  if (line.find("[CRASH]") != std::string::npos)
    return GrblMessageType::Crash;

  if (line.find("[PRB") != std::string::npos)
    return GrblMessageType::ProbeResult;

  if (line.find("Grbl") != std::string::npos)
    return GrblMessageType::GrblReady;

  return GrblMessageType::Unknown;
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
  m_thc_active = false;
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

  // Start the machine-runtime thread (owns the serial port and the streaming
  // pump) and the watchdog. From here on all serial and live-state access happens
  // on m_runtime_thread; the render thread only reads snapshots and posts
  // commands. Status polling and the arc-okay timeout are serviced by
  // serviceTimers() on this thread rather than the render-thread timer stack.
  m_running.store(true, std::memory_order_relaxed);
  m_heartbeat.store(static_cast<uint64_t>(NcRender::millis()),
                    std::memory_order_relaxed);
  m_runtime_thread = std::thread(&MotionController::runtimeLoop, this);
  m_watchdog_thread = std::thread(&MotionController::watchdogLoop, this);
}

MotionController::~MotionController() { shutdown(); }

// One iteration of the machine runtime: service the serial port (which dispatches
// byteHandler/lineHandler and thus the ok-driven streaming pump), apply queued UI
// commands, run time-based tasks, and publish a fresh status snapshot. Runs on
// m_runtime_thread via runtimeLoop().
void MotionController::runtimeStep()
{
  m_serial.tick();

  if (m_serial.m_is_connected == false) {
    m_controller_ready = false;
    m_needs_homed = true;
  }

  drainCommands();
  serviceTimers();
  publishSnapshot();

  m_heartbeat.store(static_cast<uint64_t>(NcRender::millis()),
                    std::memory_order_relaxed);
}

void MotionController::runtimeLoop()
{
  while (m_running.load(std::memory_order_relaxed)) {
    runtimeStep();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
}

// Defense-in-depth safety net. If the runtime loop stops updating its heartbeat
// while the torch is on, the control stream has wedged mid-cut. The firmware
// already halts and kills the torch on host-comms loss; this attempts an orderly
// PC-side stop as well and raises a prominent alarm. The threshold clears the
// longest intentional runtime stall (the ~600 ms abort dwell) with margin.
void MotionController::watchdogLoop()
{
  constexpr uint64_t c_stall_threshold_ms = 1500;
  bool               fired = false;

  while (m_running.load(std::memory_order_relaxed)) {
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    const uint64_t now = static_cast<uint64_t>(NcRender::millis());
    const uint64_t beat = m_heartbeat.load(std::memory_order_relaxed);
    const bool     stalled =
      (now > beat) && ((now - beat) > c_stall_threshold_ms);

    // torch_on as of the last successful publish. If the loop wedged with the
    // torch on, this reads true. Read under the snapshot lock (race-free); the
    // wedge is a blocking serial read, which never holds that lock.
    bool torch_on;
    {
      std::lock_guard<std::mutex> lock(m_snapshot_mutex);
      torch_on = m_published.torch_on;
    }

    if (stalled && torch_on && !fired) {
      LOG_F(ERROR,
            "Runtime watchdog: no heartbeat for %llu ms with torch on - "
            "attempting emergency stop!",
            static_cast<unsigned long long>(now - beat));
      fired = true;
      emergencyStop();
    }
    else if (!stalled) {
      fired = false;
    }
  }
}

void MotionController::emergencyStop()
{
  // Best-effort orderly stop from the watchdog thread. The runtime thread is
  // presumed wedged, so we touch the serial port directly here. wjwwood/serial
  // is not thread-safe, so this is the one deliberate cross-thread exception -
  // justified because the alternative is a torch dwelling in place, and the
  // firmware is the ultimate backstop if this write also blocks. sendByte /
  // sendString already swallow their own exceptions and no-op when disconnected.
  m_serial.sendByte('!');          // feed hold
  m_serial.sendString("M5\n");     // torch off
  m_serial.sendString("$T!0.0\n"); // disengage THC
  m_serial.sendByte(0x18);         // soft reset

  // Surface the alarm through the snapshot directly so the render thread shows it
  // even though the (wedged) runtime thread cannot publish.
  std::lock_guard<std::mutex> lock(m_snapshot_mutex);
  m_published.alarm_text =
    "Communication watchdog tripped: the control stream stalled while the torch "
    "was on. An emergency stop was attempted. Verify the machine is safe and "
    "restart the controller.";
  m_published.alarm_seq++;
}

void MotionController::serviceTimers()
{
  const auto now = std::chrono::steady_clock::now();

  // 100 ms GRBL status poll + deferred motion-sync callback (formerly statusTimer
  // on the render-thread timer stack).
  if (now - m_last_status_poll >= std::chrono::milliseconds{ 100 }) {
    m_last_status_poll = now;
    statusTimer();
  }

  // One-shot arc-okay expiry (formerly a one-shot render-thread pushTimer
  // scheduled in runPop on WAIT_FOR_ARC_OKAY).
  if (m_arc_expire_deadline && now >= *m_arc_expire_deadline) {
    m_arc_expire_deadline.reset();
    arcOkayExpireTimer();
  }
}

void MotionController::drainCommands()
{
  std::deque<Command> local;
  {
    std::lock_guard<std::mutex> lock(m_cmd_mutex);
    local.swap(m_cmd_queue);
  }
  for (auto& cmd : local) {
    switch (cmd.type) {
      case CommandType::RunProgram:
        startProgram(std::move(cmd.lines));
        break;
      case CommandType::RealTime:
        if (m_controller_ready) {
          LOG_F(INFO, "(motion_controller_send_rt) Byte: %c\n", cmd.rt_byte);
          m_serial.sendByte(static_cast<uint8_t>(cmd.rt_byte));
        }
        break;
      case CommandType::Abort:
        sendCommand("abort");
        break;
      case CommandType::Home:
        sendCommand("home");
        break;
      case CommandType::AdjustThc:
        applyThcOffset(cmd.thc_delta);
        break;
      case CommandType::TriggerReset:
        doTriggerReset();
        break;
      case CommandType::WriteParams:
        doWriteParametersToController();
        break;
    }
  }
}

void MotionController::publishSnapshot()
{
  MachineSnapshot s;
  s.dro = m_dro_data;
  s.dro.torch_on = m_torch_on;
  s.runtime = computeRuntime();
  s.thc_base = m_thc_base_value;
  s.thc_offset = m_thc_offset;
  s.thc_effective = thcEffectiveLive();
  s.controller_ready = m_controller_ready;
  s.needs_homed = m_needs_homed;
  s.torch_on = m_torch_on;
  s.program_running = m_torch_on || !m_gcode_queue.empty() ||
                      m_dro_data.status == MachineStatus::Cycle;
  s.homing_safe = m_controller_ready && !m_homing_in_progress &&
                  m_okay_callback == nullptr && m_gcode_queue.empty();
  s.show_offline = !m_serial.m_is_connected;
  bool homing_enabled;
  {
    std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
    homing_enabled = m_control_view->m_machine_parameters.homing_enabled;
  }
  s.want_homing = m_needs_homed && homing_enabled && m_serial.m_is_connected &&
                  m_controller_ready;
  s.alarm_seq = m_alarm_seq;
  s.alarm_text = m_live_alarm_text;
  s.info_seq = m_info_seq;
  s.info_text = m_live_info_text;

  std::lock_guard<std::mutex> lock(m_snapshot_mutex);
  m_published = std::move(s);
}

void MotionController::syncUiSnapshot()
{
  std::lock_guard<std::mutex> lock(m_snapshot_mutex);
  m_ui_snapshot = m_published;
}

RuntimeData MotionController::computeRuntime() const
{
  RuntimeData runtime;
  if (m_program_start_time != std::chrono::steady_clock::time_point{}) {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
      now - m_program_start_time);
    unsigned long m = duration.count();
    runtime.seconds = (m / 1000) % 60;
    runtime.minutes = (m / (1000 * 60)) % 60;
    runtime.hours = (m / (1000 * 60 * 60)) % 24;
  }
  return runtime;
}

void MotionController::postInfo(const std::string& msg)
{
  m_live_info_text = msg;
  ++m_info_seq;
}

void MotionController::postAlarm(const std::string& msg)
{
  m_live_alarm_text = msg;
  ++m_alarm_seq;
}

void MotionController::shutdown()
{
  m_running.store(false, std::memory_order_relaxed);
  if (m_runtime_thread.joinable())
    m_runtime_thread.join();
  if (m_watchdog_thread.joinable())
    m_watchdog_thread.join();

  m_gcode_queue.clear();
  m_okay_callback = nullptr;
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
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
    if (!m_homing_in_progress) {
      m_homing_in_progress = true;
      m_okay_callback = [this]() { homingDoneCallback(); };
      send("$H");
    }
  }
}

void MotionController::enqueue(Command cmd)
{
  std::lock_guard<std::mutex> lock(m_cmd_mutex);
  m_cmd_queue.push_back(std::move(cmd));
}

// Render thread: stage a line for the next batch. Consumed by runStack().
void MotionController::pushGCode(const std::string& gcode)
{
  m_staging.push_back(gcode);
}

// Render thread: hand the staged batch to the runtime thread to execute.
void MotionController::runStack()
{
  Command cmd{ CommandType::RunProgram };
  cmd.lines = std::move(m_staging);
  m_staging.clear();
  enqueue(std::move(cmd));
}

// Runtime thread: load a batch onto the queue and begin the ok-driven pump.
void MotionController::startProgram(std::vector<std::string> lines)
{
  for (auto& line : lines)
    m_gcode_queue.push_back(std::move(line));
  runStackInternal();
}

// Runtime thread: start the pump on the already-populated queue. Used by
// callbacks/bootstrap that push directly onto m_gcode_queue.
void MotionController::runStackInternal()
{
  m_program_start_time = std::chrono::steady_clock::now();
  runPop();
  m_okay_callback = [this]() { runPop(); };
}

void MotionController::abort() { enqueue(Command{ CommandType::Abort }); }

void MotionController::home() { enqueue(Command{ CommandType::Home }); }

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

// Render thread: queue a real-time byte (jog, feed-hold, etc.).
void MotionController::sendRealTime(char s)
{
  Command cmd{ CommandType::RealTime };
  cmd.rt_byte = s;
  enqueue(std::move(cmd));
}

void MotionController::triggerReset()
{
  enqueue(Command{ CommandType::TriggerReset });
}

// Runtime thread: pulse DTR to reset the controller.
void MotionController::doTriggerReset()
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
  // Parse message type once for efficient dispatch
  GrblMessageType msg_type = parseMessageType(line);

  if (m_controller_ready == true) {
    // Handle messages when controller is ready
    switch (msg_type) {
      case GrblMessageType::DROData:
        try {
          // Parse JSON into typed struct
          nlohmann::json j = nlohmann::json::parse(line);
          m_dro_data.status =
            parseMachineStatus(j.at("STATUS").get<std::string>());
          m_dro_data.mcs.x = j.at("MCS").at("x").get<float>();
          m_dro_data.mcs.y = j.at("MCS").at("y").get<float>();
          m_dro_data.mcs.z = j.at("MCS").at("z").get<float>();
          m_dro_data.wcs.x = j.at("WCS").at("x").get<float>();
          m_dro_data.wcs.y = j.at("WCS").at("y").get<float>();
          m_dro_data.wcs.z = j.at("WCS").at("z").get<float>();
          m_dro_data.feed = j.at("FEED").get<float>();
          m_dro_data.voltage = j.at("V").get<float>();
          m_dro_data.in_motion = j.at("IN_MOTION").get<bool>();
          m_dro_data.arc_ok = j.at("ARC_OK").get<bool>();
          m_dro_data.torch_on = m_torch_on;

          // LOG_F(INFO, "ARC VOLTAGE = %f", m_dro_data.voltage);

          if (m_dro_data.arc_ok == false) {
            if (m_arc_okay_callback) {
              m_arc_okay_callback();
              m_arc_okay_callback = nullptr;
              m_arc_okay_timer = std::chrono::steady_clock::now();
              m_arc_on_start = m_arc_okay_timer;
              {
                std::lock_guard<std::mutex> plock(
                  m_control_view->m_params_mutex);
                m_control_view->m_machine_parameters.consumable_pierce_count++;
              }
            }
          }
          if (m_abort_pending == true && m_dro_data.in_motion == false) {
            logRuntime();
            m_serial.delay(300);
            LOG_F(INFO, "Handling pending abort -> Sending Reset!");
            m_serial.sendByte(0x18);
            m_serial.delay(300);
            m_abort_pending = false;
            m_program_start_time = std::chrono::steady_clock::time_point{};
            m_arc_retry_count = 0;
            m_torch_on = false;
            m_thc_active = false;
            m_handling_crash = false;
            m_controller_ready = false;
          }
        }
        catch (...) {
          LOG_F(ERROR, "Error parsing DRO JSON data!");
        }
        break;

      case GrblMessageType::UnlockRequired:
        if (m_control_view->m_machine_parameters.homing_enabled == false) {
          LOG_F(WARNING,
                "Controller lockout for unknown reason. Automatically "
                "unlocking...");
          send("$X");
        }
        else {
          LOG_F(
            WARNING,
            "Controller lockout for unknown reason. Machine will need to be "
            "re-homed...");
          m_needs_homed = true;
        }
        break;

      case GrblMessageType::ChecksumFailure: {
        auto current_time = std::chrono::steady_clock::now();
        auto last_error_duration =
          std::chrono::duration_cast<std::chrono::milliseconds>(
            current_time -
            std::chrono::steady_clock::time_point{
              std::chrono::milliseconds{ m_last_checksum_error } });

        if (m_last_checksum_error > 0 &&
            last_error_duration < std::chrono::milliseconds{ 100 }) {
          postInfo("Program aborted due to multiple communication "
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
      } break;

      case GrblMessageType::Error: {
        removeSubstrs(line, "error:");
        int error_code = atoi(line.c_str());
        if (error_code == 18) {
          // STATUS_THC_ERROR: the firmware rejected a $T= target (machine state
          // was no longer CYCLE/HOLD, or the voltage was invalid). It is
          // returned *only* for the $T= command (firmware system.c, case 'T'),
          // so it can only be a live baby-step that raced the machine state. It
          // must NOT abort the cut, so we skip logControllerError (which would
          // clear the queue and halt with the torch still on).
          //
          // The firmware withholds the '>' ack for any rejected line
          // (report.c report_status_message only emits '>' on STATUS_OK), so
          // the normal runPop chain will not fire on its own. Advance the
          // stream here to stand in for the ack the firmware did not send,
          // otherwise the program would stall with the torch dwelling in place.
          LOG_F(WARNING,
                "Firmware Error 18 (THC target rejected) - ignoring and "
                "advancing the stream to keep the cut running.");
          if (m_okay_callback) {
            m_okay_callback();
          }
        }
        else {
          logControllerError(error_code);
        }
      } break;

      case GrblMessageType::Alarm:
        removeSubstrs(line, "ALARM:");
        handleAlarm(atoi(line.c_str()));
        break;

      case GrblMessageType::Crash:
        if (m_handling_crash == false) {
          auto current_time = std::chrono::steady_clock::now();
          auto torch_duration =
            std::chrono::duration_cast<std::chrono::milliseconds>(
              current_time - m_torch_on_timer);

          if (m_torch_on == true &&
              torch_duration > std::chrono::milliseconds{ 1500 }) {
            postInfo("Program was aborted because torch crash was detected! "
                     "Restart your controller to be safe.");
            sendCommand("abort");
            m_handling_crash = true;
          }
        }
        break;

      case GrblMessageType::ProbeResult:
        LOG_F(INFO, "Probe finished - Running callback!");
        if (m_probe_callback) {
          m_probe_callback();
        }
        break;

      case GrblMessageType::GrblReady:
        LOG_F(WARNING,
              "Received Grbl start message while in ready state. Was the "
              "controller reset?");
        break;

      case GrblMessageType::Unknown:
        LOG_F(WARNING, "Unidentified line received - %s", line.c_str());
        break;
    }
  }
  else if (m_controller_ready == false) {
    // Handle messages when controller is not ready
    switch (msg_type) {
      case GrblMessageType::GrblReady: {
        LOG_F(INFO, "Controller ready!");
        m_controller_ready = true;
        std::array<float, 3> wo;
        {
          std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
          wo = m_control_view->m_machine_parameters.work_offset;
        }
        m_gcode_queue.push_back("G10 L2 P0 X" + std::to_string(wo[0]) + " Y" +
                                std::to_string(wo[1]) + " Z" +
                                std::to_string(wo[2]));
        m_gcode_queue.push_back("M30");
        runStackInternal();
        break;
      }

      case GrblMessageType::UnlockRequired: {
        bool                 homing_enabled;
        std::array<float, 3> wo;
        {
          std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
          homing_enabled = m_control_view->m_machine_parameters.homing_enabled;
          wo = m_control_view->m_machine_parameters.work_offset;
        }
        if (homing_enabled == true) {
          LOG_F(INFO, "Controller ready, but needs homing.");
          m_controller_ready = true;
          m_needs_homed = true;
          m_gcode_queue.push_back("G10 L2 P0 X" + std::to_string(wo[0]) + " Y" +
                                  std::to_string(wo[1]) + " Z" +
                                  std::to_string(wo[2]));
          m_gcode_queue.push_back("M30");
          runStackInternal();
        }
        else {
          LOG_F(WARNING, "Controller locked out. Unlocking...");
          send("$X");
        }
        break;
      }

      default:
        // Ignore other messages when controller is not ready
        break;
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
      if (args.size() == 5) {
        m_torch_params.pierce_height =
          static_cast<double>(atof(args[1].c_str()));
        m_torch_params.pierce_delay =
          static_cast<double>(atof(args[2].c_str()));
        m_torch_params.cut_height = static_cast<double>(atof(args[3].c_str()));
        m_torch_params.thc = static_cast<double>(atof(args[4].c_str()));
        m_thc_base_value = static_cast<float>(m_torch_params.thc);
      }
      else {
        LOG_F(ERROR, "[fire_torch] Invalid arguments - Using defaults!");
        m_torch_params.pierce_height = c_default_pierce_height;
        m_torch_params.pierce_delay = c_default_pierce_delay_s;
        m_torch_params.cut_height = c_default_cut_height;
        m_torch_params.thc = 0.0;
        m_thc_base_value = 0.0f;
      }

      if (m_arc_retry_count > 3) {
        LOG_F(INFO,
              "[run_pop] Arc retry max count reached. Retracting and aborting "
              "program!");
        m_okay_callback = nullptr;
        m_motion_sync_callback = [this]() { torchOffAndAbort(); };
        postInfo(
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
      if (args.size() >= 4) {
        m_torch_params.pierce_height =
          static_cast<double>(atof(args[1].c_str()));
        m_torch_params.pierce_delay =
          static_cast<double>(atof(args[2].c_str()));
        m_torch_params.cut_height = static_cast<double>(atof(args[3].c_str()));
      }
      else {
        LOG_F(WARNING, "[touch_torch] No arguments - Using default!");
        m_torch_params.pierce_height = c_default_pierce_height;
        m_torch_params.pierce_delay = c_default_pierce_delay_s;
        m_torch_params.cut_height = c_default_cut_height;
      }
      m_okay_callback = nullptr;
      m_probe_callback = [this]() { raiseAfterTouch(); };
      probe();
    }
    else if (line.find("WAIT_FOR_ARC_OKAY") != std::string::npos) {
      const double arc_okay_timeout =
        1.5 * m_torch_params.pierce_delay * 1000.0;
      LOG_F(INFO,
            "[run_pop] Setting arc_okay callback and arc_okay expire timer => "
            "fires in %.4f ms!",
            arc_okay_timeout);
      // Serviced by serviceTimers() on the runtime thread (was a one-shot
      // render-thread pushTimer).
      m_arc_expire_deadline =
        std::chrono::steady_clock::now() +
        std::chrono::milliseconds(static_cast<long long>(arc_okay_timeout));
      m_arc_okay_callback = [this]() { lowerToCutHeightAndRunProgram(); };
      m_okay_callback = nullptr;
      m_probe_callback = nullptr;
    }
    else if (line.find("torch_off_async") != std::string::npos) {
      // Non-blocking torch off: kills arc + zeroes THC inline so the
      // overburn G1 moves queued after this can keep popping. Unlike the
      // blocking torch_off, does NOT register m_motion_sync_callback
      // (which would otherwise stall the queue until motion stops) and
      // does NOT retract Z (torch must keep moving in XY at cut height
      // during overburn so the arc extinguishes mid-flight).
      accumulateArcOnTime();
      m_torch_on = false;
      m_thc_active = false;
      m_okay_callback = [this]() { runPop(); };
      m_gcode_queue.push_front("$T!0.0");
      m_gcode_queue.push_front("M5");
      runPop();
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
    else if (line.find("$T!") != std::string::npos) {
      // Internal THC command - send as $T= to hardware, no base update.
      // This is the single point where THC targets are actually transmitted,
      // so it is the authoritative place to track engagement: a nonzero target
      // engages THC, a $T!0.0 disengages it. Tracking at send-time (not at
      // push-time) keeps the pierce window safe - the engaging $T! is queued
      // behind the lower-to-cut-height moves, so m_thc_active only flips true
      // once those have popped and the target actually goes out.
      std::string value = line.substr(line.find("$T!") + 3);
      m_thc_active = (atof(value.c_str()) != 0.0);
      std::string cmd = "$T=" + value;
      cmd.erase(std::remove(cmd.begin(), cmd.end(), ' '), cmd.end());
      DLOG_F(INFO, "(runpop) internal THC: sending %s", cmd.c_str());
      sendWithCRC(cmd);
    }
    else {
      line.erase(std::remove(line.begin(), line.end(), ' '), line.end());
      DLOG_F(INFO, "(runpop) sending %s", line.c_str());
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
    "G38.3Z-" +
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
    m_thc_active = false;

    m_gcode_queue.push_front(
      "fire_torch " + to_string_strip_zeros(m_torch_params.pierce_height) +
      " " + to_string_strip_zeros(m_torch_params.pierce_delay) + " " +
      to_string_strip_zeros(m_torch_params.cut_height) + " " +
      to_string_strip_zeros(m_torch_params.thc));
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
    if (m_dro_data.status != MachineStatus::Unknown) {
      auto current_time = std::chrono::steady_clock::now();
      auto program_duration =
        std::chrono::duration_cast<std::chrono::milliseconds>(
          current_time - m_program_start_time);

      if (m_motion_sync_callback && m_dro_data.in_motion == false &&
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
  m_thc_base_value = 0.0f;
  m_thc_offset = 0.0f;
  m_thc_active = false;
  m_okay_callback = nullptr;
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_gcode_queue.clear();
  // Persist consumable counters accumulated in RAM during the program. File-only
  // persistence (not saveParameters, which also touches render primitives).
  persistParameters();
}

void MotionController::homingDoneCallback()
{
  m_okay_callback = nullptr;
  m_needs_homed = false;
  m_homing_in_progress = false; // Release homing lock
  LOG_F(INFO, "Homing finished! Saving Z work offset...");
  {
    std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
    m_control_view->m_machine_parameters.work_offset[2] = m_dro_data.mcs.z;
  }
  m_gcode_queue.push_back("G10 L20 P0 Z0");
  m_gcode_queue.push_back("M30");
  runStackInternal();
  persistParameters();
}

void MotionController::lowerToCutHeightAndRunProgram()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_arc_retry_count = 0;
  m_gcode_queue.push_front("$T!" + to_string_strip_zeros(thcEffectiveLive()));
  m_gcode_queue.push_front("G90");
  m_gcode_queue.push_front("G91G0 Z-" +
                           to_string_strip_zeros(m_torch_params.pierce_height -
                                                 m_torch_params.cut_height));
  m_gcode_queue.push_front("G4P" +
                           to_string_strip_zeros(m_torch_params.pierce_delay));
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
  m_gcode_queue.push_front("G0Z" +
                           to_string_strip_zeros(m_torch_params.pierce_height));
  m_gcode_queue.push_front(
    "G0Z" + to_string_strip_zeros(
              m_control_view->m_machine_parameters.floating_head_backlash));
  m_gcode_queue.push_front("G91");
  LOG_F(INFO, "Running callback => raise_to_pierce_height_and_fire_torch()");
  m_torch_on = true;
  m_torch_on_timer = std::chrono::steady_clock::now();
  runPop();
}

void MotionController::raiseAfterTouch()
{
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;

  m_gcode_queue.push_front("G90");
  m_gcode_queue.push_front("G0Z" + std::to_string(m_torch_params.cut_height));
  m_gcode_queue.push_front(
    "G0Z" + to_string_strip_zeros(
              m_control_view->m_machine_parameters.floating_head_backlash));
  m_gcode_queue.push_front("G91");
  LOG_F(INFO, "Touching off torch and dry running!");
  runPop();
}

void MotionController::torchOffAndAbort()
{
  accumulateArcOnTime();
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_torch_on = false;
  m_thc_active = false;
  m_thc_offset = 0.0f;

  m_gcode_queue.push_front("M30");
  m_gcode_queue.push_front("G0 Z0");
  m_gcode_queue.push_front("M5");
  LOG_F(INFO, "Shutting torch off, retracting, and aborting!");
  runPop();
}

void MotionController::torchOffAndRetract()
{
  accumulateArcOnTime();
  m_okay_callback = [this]() { runPop(); };
  m_probe_callback = nullptr;
  m_motion_sync_callback = nullptr;
  m_arc_okay_callback = nullptr;
  m_torch_on = false;
  m_thc_active = false;

  m_gcode_queue.push_front("$T!0.0");
  m_gcode_queue.push_front("G0 Z0");
  m_gcode_queue.push_front("M5");
  LOG_F(INFO, "Shutting torch off and retracting!");
  runPop();
}

void MotionController::accumulateArcOnTime()
{
  if (!m_arc_on_start)
    return;
  const auto elapsed_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                            std::chrono::steady_clock::now() - *m_arc_on_start)
                            .count();
  m_arc_on_start.reset();
  if (elapsed_ms <= 0)
    return;
  std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
  m_control_view->m_machine_parameters.consumable_arc_on_time_ms +=
    static_cast<uint64_t>(elapsed_ms);
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

// Render thread: queue a THC baby-step for the runtime thread to apply.
void MotionController::adjustThcOffset(float delta)
{
  Command cmd{ CommandType::AdjustThc };
  cmd.thc_delta = delta;
  enqueue(std::move(cmd));
}

// Runtime thread: apply the THC baby-step against live state.
void MotionController::applyThcOffset(float delta)
{
  // Clamping to encourage proper targets in tool library instead
  m_thc_offset =
    std::clamp(m_thc_offset + delta, MIN_THC_OFFSET, MAX_THC_OFFSET);
  LOG_F(INFO,
        "THC offset adjusted: base=%.1f offset=%.1f effective=%.1f",
        m_thc_base_value,
        m_thc_offset,
        thcEffectiveLive());

  // Inject a live target only when THC is genuinely engaged AND the machine is
  // cutting; otherwise just remember the offset (above) - it is applied at the
  // next fire_torch via thcEffectiveLive(). Gating on m_thc_active (not just
  // m_torch_on) keeps a click during pierce/idle from sending a $T= the
  // firmware would reject with error 18.
  if (m_thc_active && m_dro_data.in_motion && m_thc_base_value != 0.f) {
    m_gcode_queue.push_front("$T!" + to_string_strip_zeros(thcEffectiveLive()));
    if (!m_okay_callback) {
      m_okay_callback = [this]() { runPop(); };
    }
  }
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
  postInfo(ret);
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
  postAlarm(ret);
  m_gcode_queue.clear();
}

// Render thread only: refresh the machine/cuttable plane render primitives from
// the current parameters, then persist to disk. Called after the user edits
// parameters or zeroes an axis.
void MotionController::saveParameters()
{
  // Update machine plane parameters (render primitives - render thread only).
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

  persistParameters();
}

// Serialize the machine parameters to disk. No render-primitive access, so it is
// safe to call from the runtime thread (programFinished, homingDoneCallback).
void MotionController::persistParameters()
{
  nlohmann::json preferences;
  {
    std::lock_guard<std::mutex> plock(m_control_view->m_params_mutex);
    NcControlView::to_json(preferences, m_control_view->m_machine_parameters);
  }
  try {
    std::ofstream out(m_renderer->getConfigDirectory() +
                      "machine_parameters.json");
    out << preferences.dump();
    out.close();
  }
  catch (...) {
    LOG_F(ERROR, "Could not write parameters file!");
  }
}

// Render thread: queue the controller-parameter write for the runtime thread.
void MotionController::writeParametersToController()
{
  enqueue(Command{ CommandType::WriteParams });
}

// Runtime thread: push all $ settings from the current parameters and run them.
void MotionController::doWriteParametersToController()
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
      "$11=" +
      std::to_string(m_control_view->m_machine_parameters.junction_deviation));
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
      "$33=" +
      std::to_string(m_control_view->m_machine_parameters.arc_voltage_divider));
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
    runStackInternal();
  }
}
