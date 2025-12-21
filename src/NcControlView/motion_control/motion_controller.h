#ifndef MOTION_CONTROLLER_H
#define MOTION_CONTROLLER_H

#include "../serial/NcSerial.h"
#include <NcRender/json/json.h>
#include <application.h>
#include <chrono>
#include <deque>
#include <functional>

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
  bool           isReady() const { return m_controller_ready; }
  bool           isTorchOn() const { return m_torch_on; }
  bool           needsHoming() const { return m_needs_homed; }
  nlohmann::json getDRO() const { return m_dro_data; }
  nlohmann::json getRunTime() const;

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

  // Data storage
  nlohmann::json m_dro_data;
  nlohmann::json m_callback_args;

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
