#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include "../serial/NcSerial.h"
#include <NanoCut.h>
#include <atomic>
#include <chrono>
#include <deque>
#include <functional>
#include <mutex>
#include <nlohmann/json.hpp>
#include <optional>
#include <string>
#include <thread>
#include <vector>

// Type-safe data structures for runtime data (replacing JSON)

enum class MachineStatus {
  Idle,
  Cycle,
  Alarm,
  Homing,
  Hold,
  Check,
  Door,
  Unknown
};

struct MCSCoordinates {
  float x{ 0.0f };
  float y{ 0.0f };
  float z{ 0.0f };
};

struct DROData {
  MachineStatus  status{ MachineStatus::Unknown };
  MCSCoordinates mcs;
  MCSCoordinates wcs;
  float          feed{ 0 };
  float          voltage{ 0 };
  bool           in_motion{ false };
  // arc_ok just reflects the pin value on the MCU currently and the assumption
  // is made that arc_ok == false when we do have arc ok. TODO: fix?
  // I think most plasmas have a dry relay closed for arc okay so in 99% of
  // cases you'd pull up the input pin and let the relay glose it to GND, making
  // arc_ok = false.
  bool arc_ok{ false };
  bool torch_on{ false };
};

struct TorchParameters {
  double pierce_height{ 0.0 };
  double pierce_delay{ 0.0 };
  double cut_height{ 0.0 };
  double thc{ 0.0 };
};

struct RuntimeData {
  unsigned long hours{ 0 };
  unsigned long minutes{ 0 };
  unsigned long seconds{ 0 };
};

enum class GrblMessageType {
  DROData,         // JSON status data
  UnlockRequired,  // [MSG:'$H'|'$X' to unlock]
  ChecksumFailure, // [CHECKSUM_FAILURE]
  Error,           // error:X
  Alarm,           // ALARM:X
  Crash,           // [CRASH]
  ProbeResult,     // [PRB
  GrblReady,       // Grbl startup message
  Unknown          // Unidentified message
};

// Immutable status view published by the machine-runtime thread and consumed by
// the render/UI thread. The runtime writes m_published under a mutex; the render
// thread copies it into m_ui_snapshot once per frame (syncUiSnapshot). All public
// state-query getters read the render-side copy, so the UI never touches live
// runtime state directly.
struct MachineSnapshot {
  DROData     dro;
  RuntimeData runtime;
  float       thc_base{ 0.0f };
  float       thc_offset{ 0.0f };
  float       thc_effective{ 0.0f };
  bool        controller_ready{ false };
  bool        needs_homed{ false };
  bool        torch_on{ false };
  bool        program_running{ false };
  bool        homing_safe{ false };
  // Continuous UI states (applied every frame by the render thread).
  bool show_offline{ false };
  bool want_homing{ false };
  // Edge-triggered UI signals: the render thread acts only when the sequence
  // number changes, so a message shows exactly once per occurrence.
  uint32_t    alarm_seq{ 0 };
  std::string alarm_text;
  uint32_t    info_seq{ 0 };
  std::string info_text;
};

// UI -> runtime command. User actions on the render thread enqueue these; the
// runtime thread drains and executes them so all serial/state mutation stays on
// one thread.
enum class CommandType {
  RunProgram,   // run the staged gcode lines
  RealTime,     // single real-time byte (jog, feed-hold, etc.)
  Abort,        // sendCommand("abort")
  Home,         // sendCommand("home")
  AdjustThc,    // baby-step THC offset
  TriggerReset, // toggle DTR to reset the controller
  WriteParams   // push machine parameters to the controller
};

struct Command {
  CommandType              type;
  std::vector<std::string> lines;             // RunProgram
  char                     rt_byte{ 0 };      // RealTime
  float                    thc_delta{ 0.0f }; // AdjustThc
};

class MotionController {
public:
  // Constructor/Destructor
  MotionController(class NcControlView* nc_view, class NcRender* renderer);
  ~MotionController();

  // Core interface
  void initialize();
  void shutdown();

  // Copy the runtime's published status into the render-side cache. Call once
  // per frame on the render thread before reading any state-query getter.
  void                   syncUiSnapshot();
  const MachineSnapshot& uiSnapshot() const { return m_ui_snapshot; }

  // Command interface (render thread). User actions enqueue work for the runtime
  // thread; nothing here touches the serial port or live state directly.
  void pushGCode(const std::string& gcode); // stage a line for the next batch
  void runStack();                          // enqueue the staged batch to run
  void abort();
  void home();
  void sendRealTime(char s);
  void triggerReset();

  // State queries. These read the render-side snapshot cache (refreshed once per
  // frame by syncUiSnapshot), so they are safe to call from the render thread
  // while the runtime thread mutates live state. Do NOT call them from runtime
  // code - use the live members / thcEffectiveLive() there instead.
  bool isReady() const { return m_ui_snapshot.controller_ready; }
  bool isTorchOn() const { return m_ui_snapshot.torch_on; }
  bool needsHoming() const { return m_ui_snapshot.needs_homed; }
  bool isProgramRunning() const { return m_ui_snapshot.program_running; }
  const DROData&    getDRO() const { return m_ui_snapshot.dro; }
  const RuntimeData getRunTime() const { return m_ui_snapshot.runtime; }

  // THC baby-stepping
  float getThcBaseValue() const { return m_ui_snapshot.thc_base; }
  float getThcOffset() const { return m_ui_snapshot.thc_offset; }
  float getThcEffective() const { return m_ui_snapshot.thc_effective; }
  void  adjustThcOffset(float delta);

  // Homing safety check
  bool isHomingSafe() const { return m_ui_snapshot.homing_safe; }

  // Parameter management
  void saveParameters();              // render thread: update scene + persist file
  void writeParametersToController(); // enqueues a controller parameter write
  void logRuntime();

private:
  // Serial communication
  NcSerial m_serial;

  // Command queue
  std::deque<std::string> m_gcode_queue;

  // Data storage (type-safe structs instead of JSON)
  DROData         m_dro_data;
  TorchParameters m_torch_params;

  // State variables
  bool m_controller_ready{ false };
  bool m_torch_on{ false };
  bool m_abort_pending{ false };
  bool m_handling_crash{ false };
  bool m_needs_homed{ true };
  bool m_homing_in_progress{ false };

  // THC baby-stepping state
  float m_thc_base_value{ 0.0f };
  float m_thc_offset{ 0.0f };
  // Host-side mirror of the firmware's "THC target is settable" state. True
  // only while THC is genuinely engaged (a nonzero $T! target has been sent and
  // not yet cancelled by $T!0.0). Gates live baby-step injection so a click
  // during pierce/idle never sends a $T= the firmware would reject (error 18).
  bool m_thc_active{ false };

  // Counters and timing
  int                                   m_arc_retry_count{ 0 };
  std::chrono::steady_clock::time_point m_torch_on_timer;
  std::chrono::steady_clock::time_point m_arc_okay_timer;
  std::chrono::steady_clock::time_point m_program_start_time;
  // Set on the rising edge of arc-okay; cleared and accumulated into
  // consumable_arc_on_time_ms on torch-off. Reset if the arc never started.
  std::optional<std::chrono::steady_clock::time_point> m_arc_on_start;
  unsigned long m_last_checksum_error{ static_cast<unsigned long>(-1) };

  // Callbacks using std::function
  std::function<void()> m_okay_callback;
  std::function<void()> m_probe_callback;
  std::function<void()> m_motion_sync_callback;
  std::function<void()> m_arc_okay_callback;

  // Dependencies (injected)
  class NcControlView* m_control_view{ nullptr };
  class NcRender*      m_renderer{ nullptr };

  // --- Render/runtime thread boundary ---
  // m_published is written by the runtime thread under m_snapshot_mutex;
  // m_ui_snapshot is the render thread's private copy (refreshed by
  // syncUiSnapshot). m_cmd_queue carries UI actions to the runtime thread.
  // m_staging batches pushGCode() lines on the render thread until runStack().
  MachineSnapshot          m_published;
  MachineSnapshot          m_ui_snapshot;
  mutable std::mutex       m_snapshot_mutex;
  std::deque<Command>      m_cmd_queue;
  std::mutex               m_cmd_mutex;
  std::vector<std::string> m_staging;

  // Runtime thread lifecycle. m_running gates the loop; m_heartbeat is the
  // watchdog liveness timestamp (ms since steady-clock epoch).
  std::thread           m_runtime_thread;
  std::thread           m_watchdog_thread;
  std::atomic<bool>     m_running{ false };
  std::atomic<uint64_t> m_heartbeat{ 0 };

  // Edge-triggered UI signals recorded by runtime code and surfaced through the
  // snapshot; the render thread shows each once when the sequence number bumps.
  std::string m_live_alarm_text;
  uint32_t    m_alarm_seq{ 0 };
  std::string m_live_info_text;
  uint32_t    m_info_seq{ 0 };

  // Runtime-thread timer scheduling (replaces NcRender::pushTimer usage).
  std::chrono::steady_clock::time_point                m_last_status_poll{};
  std::optional<std::chrono::steady_clock::time_point> m_arc_expire_deadline;

  // Machine-runtime thread. runtimeLoop spins runtimeStep at ~1 kHz on
  // m_runtime_thread; watchdogLoop runs the safety net on m_watchdog_thread.
  void runtimeLoop();
  void watchdogLoop();
  void runtimeStep();
  void serviceTimers();
  void drainCommands();
  void publishSnapshot();

  // Push a command onto the UI->runtime queue (render thread).
  void enqueue(Command cmd);

  // Internal (runtime-thread) program control. startProgram loads a batch and
  // begins the ok-driven pump; runStackInternal starts the pump on the already
  // populated queue (used by callbacks that push directly onto m_gcode_queue).
  void startProgram(std::vector<std::string> lines);
  void runStackInternal();

  // Runtime-thread implementations behind the enqueue wrappers.
  void applyThcOffset(float delta);
  void doTriggerReset();
  void doWriteParametersToController();
  void emergencyStop(); // watchdog safe-stop: torch off + hold + soft reset

  // Parameter persistence (file only; safe on the runtime thread). saveParameters
  // additionally updates render primitives and is render-thread only.
  void persistParameters();

  // Edge-triggered UI signal recorders (runtime thread).
  void postInfo(const std::string& msg);
  void postAlarm(const std::string& msg);

  // Live THC target for runtime-thread code (getThcEffective reads the render
  // cache and must not be used off the render thread).
  float thcEffectiveLive() const
  {
    return m_thc_base_value == 0.0f ? 0.0f : m_thc_base_value + m_thc_offset;
  }
  RuntimeData computeRuntime() const;

  // Private helper methods
  void     runPop();
  void     probe();
  bool     arcOkayExpireTimer();
  bool     statusTimer();
  void     logControllerError(int error);
  void     handleAlarm(int alarm);
  uint32_t calculateCRC32(uint32_t crc, const char* buf, size_t len);

  // Raw serial send helpers (runtime thread only).
  void sendCommand(const std::string& cmd);
  void send(const std::string& s);
  void sendWithCRC(const std::string& s);

  // Callback implementations (formerly global functions)
  void programFinished();
  void homingDoneCallback();
  void lowerToCutHeightAndRunProgram();
  void raiseToPierceHeightAndFireTorch();
  void raiseAfterTouch();
  void torchOffAndAbort();
  void torchOffAndRetract();
  void accumulateArcOnTime();

  // Instance methods for handling serial data
  bool byteHandler(uint8_t byte);
  void lineHandler(std::string line);
};

// Utility function (will remain global for now)
void removeSubstrs(std::string& s, std::string p);

// Forward declaration
class NcControlView;

#endif // MOTION_CONTROLLER_H
