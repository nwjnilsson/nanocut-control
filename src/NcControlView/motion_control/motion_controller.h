#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include "../serial/NcSerial.h"
#include <nlohmann/json.hpp>
#include <NanoCut.h>
#include <chrono>
#include <deque>
#include <functional>
#include <string>

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
};

struct RuntimeData {
  unsigned long hours{ 0 };
  unsigned long minutes{ 0 };
  unsigned long seconds{ 0 };
};

enum class GrblMessageType {
  DROData,           // JSON status data
  UnlockRequired,    // [MSG:'$H'|'$X' to unlock]
  ChecksumFailure,   // [CHECKSUM_FAILURE]
  Error,             // error:X
  Alarm,             // ALARM:X
  Crash,             // [CRASH]
  ProbeResult,       // [PRB
  GrblReady,         // Grbl startup message
  Unknown            // Unidentified message
};

class MotionController {
public:
  // Constructor/Destructor
  MotionController(class NcControlView* nc_view, class NcRender* renderer);
  ~MotionController() = default;

  // Core interface
  void initialize();
  void tick();
  void shutdown();

  // Command interface
  void sendCommand(const std::string& cmd);
  void pushGCode(const std::string& gcode);
  void runStack();
  void abort();
  void home();
  void send(const std::string& s);
  void sendWithCRC(const std::string& s);
  void sendRealTime(char s);
  void triggerReset();

  // State queries
  bool              isReady() const { return m_controller_ready; }
  bool              isTorchOn() const { return m_torch_on; }
  bool              needsHoming() const { return m_needs_homed; }
  const DROData&    getDRO() const { return m_dro_data; }
  const RuntimeData getRunTime() const;

  // Parameter management
  void saveParameters();
  void writeParametersToController();
  void setNeedsHomed(bool needs_homed) { m_needs_homed = needs_homed; }

  // Utility functions
  void clearStack() { m_gcode_queue.clear(); }
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

  // Counters and timing
  int                                   m_arc_retry_count{ 0 };
  std::chrono::steady_clock::time_point m_torch_on_timer;
  std::chrono::steady_clock::time_point m_arc_okay_timer;
  std::chrono::steady_clock::time_point m_program_start_time;
  unsigned long m_last_checksum_error{ static_cast<unsigned long>(-1) };

  // Callbacks using std::function
  std::function<void()> m_okay_callback;
  std::function<void()> m_probe_callback;
  std::function<void()> m_motion_sync_callback;
  std::function<void()> m_arc_okay_callback;

  // Dependencies (injected)
  class NcControlView* m_control_view{ nullptr };
  class NcRender*      m_renderer{ nullptr };

  // Private helper methods
  void     runPop();
  void     probe();
  bool     arcOkayExpireTimer();
  bool     statusTimer();
  void     logControllerError(int error);
  void     handleAlarm(int alarm);
  uint32_t calculateCRC32(uint32_t crc, const char* buf, size_t len);

  // Callback implementations (formerly global functions)
  void programFinished();
  void homingDoneCallback();
  void lowerToCutHeightAndRunProgram();
  void raiseToPierceHeightAndFireTorch();
  void touchTorchAndBackOff();
  void torchOffAndAbort();
  void torchOffAndRetract();

  // Instance methods for handling serial data
  bool byteHandler(uint8_t byte);
  void lineHandler(std::string line);
};

// Utility function (will remain global for now)
void removeSubstrs(std::string& s, std::string p);

// Forward declaration
class NcControlView;

#endif // MOTION_CONTROLLER_H
