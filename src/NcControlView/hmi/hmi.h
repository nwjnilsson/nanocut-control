#ifndef HMI_
#define HMI_

#include <NanoCut.h>
#include <NcRender/NcRender.h>
#include <array>
#include <string>
#include <vector>

// Forward declarations
class NcApp;
class NcControlView;
class InputState;
class Path;
class Primitive;
struct KeyEvent;
struct MouseMoveEvent;
struct WindowResizeEvent;

// HMI Button IDs (type-safe enum for button handling)
enum class HmiButtonId {
  Wpos,
  Park,
  ZeroX,
  ZeroY,
  ZeroZ,
  SpindleOn,
  SpindleOff,
  Retract,
  Touch,
  Run,
  TestRun,
  Abort,
  Clean,
  Fit,
  Unknown
};

// DRO display values (written by updateTimer, read by renderDro)
struct DroAxisValues {
  std::string work_readout = "0.0000";
  std::string absolute_readout = "0.0000";
};

struct DroValues {
  DroAxisValues x, y, z;
  std::string   feed = "FEED: 0";
  std::string   arc_readout = "ARC: 0.0V";
  std::string   arc_set = "SET: 0";
  std::string   run_time = "RUN: 0:0:0";
  bool          arc_ok = true;
  bool          torch_on = false;
};

// THC baby-step button definition
struct ThcButtonDef {
  float       delta;
  const char* label;
};

/**
 * NcHmi - Human Machine Interface for the NC Control View
 * Manages the UI elements, buttons, DRO displays, and user interactions
 * for the CNC machine control interface.
 */
class NcHmi {
public:
  // Constructor/Destructor
  NcHmi(NcApp* app, NcControlView* view);
  ~NcHmi() = default;

  // Non-copyable, non-movable
  NcHmi(const NcHmi&) = delete;
  NcHmi& operator=(const NcHmi&) = delete;
  NcHmi(NcHmi&&) = delete;
  NcHmi& operator=(NcHmi&&) = delete;

  // Initialization
  void init();

  // ImGui rendering functions
  void renderHmi();
  void renderDro();
  void renderThcWidget();

  // Event handlers
  void handleKeyEvent(const KeyEvent& e, const InputState& input);
  void handleMouseMoveEvent(const MouseMoveEvent& e, const InputState& input);
  void handleWindowResizeEvent(const WindowResizeEvent& e,
                               const InputState&        input);

  // Button handling
  void handleButton(const std::string& id);

  // Callbacks for primitives (uses MouseEventData from Primitive.h)
  void mouseCallback(Primitive* c, const Primitive::MouseEventData& e);

  // Utility methods
  void getBoundingBox(Point2d* bbox_min, Point2d* bbox_max);

  void clearHighlights();

private:
  // Application context
  NcApp*         m_app;
  NcControlView* m_view;

  // Layout dimensions (set once in init, used by render methods)
  float m_dro_backplane_height = 0.0f;
  float m_panel_width = 0.0f;

  // DRO display values (updated by timer, rendered by ImGui)
  DroValues m_dro;

  // THC offset readout (updated by timer)
  std::string m_thc_offset_readout = "+0.0";

  // World-space primitives (not managed by ImGui)
  Path* m_arc_okay_highlight_path = nullptr;

  // Private helper methods
  bool checkPathBounds();
  void goToWaypoint(Primitive* args);
  void jumpin(Primitive* p);
  void reverse(Primitive* p);

  // Key event callbacks
  void escapeKeyCallback(const KeyEvent& e);
  void upKeyCallback(const KeyEvent& e);
  void downKeyCallback(const KeyEvent& e);
  void rightKeyCallback(const KeyEvent& e);
  void leftKeyCallback(const KeyEvent& e);
  void pageUpKeyCallback(const KeyEvent& e);
  void pageDownKeyCallback(const KeyEvent& e);
  void tabKeyUpCallback(const KeyEvent& e);

  // Other callbacks
  void mouseMotionCallback(const MouseMoveEvent& e);
  void resizeCallback(const WindowResizeEvent& e);

  // Timer callback
  bool updateTimer();
};

#endif
