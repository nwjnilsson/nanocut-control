#ifndef HMI_
#define HMI_

#include <NcRender/NcRender.h>
#include <application.h>
#include <vector>

// Forward declarations
class NcApp;
class NcControlView;
class InputState;
class Box;
class Path;
struct KeyEvent;
struct MouseMoveEvent;
struct WindowResizeEvent;

// HMI data structures
struct hmi_button_t {
  std::string name;
  Box*        object;
  Text*       label;
};

struct hmi_button_group_t {
  hmi_button_t button_one;
  hmi_button_t button_two;
};

struct dro_data_t {
  Text* label;
  Text* work_readout;
  Text* absolute_readout;
  Box*  divider;
};

struct dro_group_data_t {
  dro_data_t x;
  dro_data_t y;
  dro_data_t z;
  Text*      feed;
  Text*      arc_readout;
  Text*      arc_set;
  Text*      run_time;
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

  // Event handlers
  void handleKeyEvent(const KeyEvent& e, const InputState& input);
  void handleMouseMoveEvent(const MouseMoveEvent& e, const InputState& input);
  void handleWindowResizeEvent(const WindowResizeEvent& e,
                               const InputState&        input);

  // Button handling
  void handleButton(const std::string& id);

  // Callbacks for primitives
  void mouseCallback(Primitive* c, const nlohmann::json& e);

  // Utility methods
  void getBoundingBox(Point2d* bbox_min, Point2d* bbox_max);

  void clearHighlights();

private:
  // Application context
  NcApp*         m_app;
  NcControlView* m_view;

  // HMI UI elements
  double                          m_backplane_width = 300.0;
  Box*                            m_backpane = nullptr;
  double                          m_dro_backplane_height = 220.0;
  Box*                            m_dro_backpane = nullptr;
  Box*                            m_button_backpane = nullptr;
  Path*                           m_arc_okay_highlight_path = nullptr;
  dro_group_data_t                m_dro;
  std::vector<hmi_button_group_t> m_button_groups;

  // Private helper methods
  bool checkPathBounds();
  void goToWaypoint(Primitive* args);
  void jumpin(Primitive* p);
  void reverse(Primitive* p);
  void pushButtonGroup(const std::string& b1, const std::string& b2);

  // Key event callbacks
  void escapeKeyCallback(const nlohmann::json& e);
  void upKeyCallback(const nlohmann::json& e);
  void downKeyCallback(const nlohmann::json& e);
  void rightKeyCallback(const nlohmann::json& e);
  void leftKeyCallback(const nlohmann::json& e);
  void pageUpKeyCallback(const nlohmann::json& e);
  void pageDownKeyCallback(const nlohmann::json& e);
  void tabKeyUpCallback(const nlohmann::json& e);

  // Other callbacks
  void mouseMotionCallback(const nlohmann::json& e);
  void resizeCallback(const nlohmann::json& e);

  // Timer callback
  bool updateTimer();
};

#endif
