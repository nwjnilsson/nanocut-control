#ifndef JET_CAM_VIEW_
#define JET_CAM_VIEW_

// Standard library includes
#include <cstdio>
#include <memory>

// System includes
#include <NcRender/NcRender.h>

// Project includes
#include <application.h>
#include <application/View.h>
#include <dxf/dxflib/dl_dxf.h>

// Local includes
#include "DXFParsePathAdaptor/DXFParsePathAdaptor.h"
#include "PolyNest/PolyNest.h"

// Forward declarations
class NcApp;
class InputState;
struct ScrollEvent;
struct MouseButtonEvent;
struct KeyEvent;
struct MouseMoveEvent;

// Custom deleter for FILE*
struct FileDeleter {
  void operator()(FILE* fp) const
  {
    if (fp) {
      fclose(fp);
    }
  }
};

#define DEFAULT_KERF_WIDTH SCALE(2.f)
#define DEFAULT_LEAD_IN (DEFAULT_KERF_WIDTH * 1.5f)
#define DEFAULT_LEAD_OUT DEFAULT_LEAD_IN

class NcCamView : public View {
private:
  struct Preferences {
    float background_color[3] = { 0.0f, 0.0f, 0.0f };
  };
  struct JobOptions {
    float material_size[2] = { DEFAULT_MATERIAL_SIZE, DEFAULT_MATERIAL_SIZE };
    int   origin_corner = 2;
  };
  struct ToolData {
    char                                   tool_name[1024];
    std::unordered_map<std::string, float> params{
      { "pierce_height", 1.f }, { "pierce_delay", 1.f },
      { "cut_height", 1.f },    { "kerf_width", DEFAULT_KERF_WIDTH },
      { "feed_rate", 1.f },     { "thc", 0.f }
    };
  };
  enum class OpType {
    Cut,
  };
  struct ToolOperation {
    bool        enabled;
    bool        last_enabled = false;
    OpType      type;
    int         tool_number;
    double      lead_in_length = DEFAULT_LEAD_IN;
    double      lead_out_length = DEFAULT_LEAD_OUT;
    std::string layer;
  };
  struct action_t {
    nlohmann::json data;
    std::string    action_id;
  };
  std::vector<action_t>      m_action_stack;
  NcRender::NcRenderGui* m_menu_bar;
  void zoomEventCallback(const ScrollEvent& e, const InputState& input);
  void mouseEventCallback(Primitive* c, const nlohmann::json& e);
  void RenderUI();

  // UI rendering helpers
  void renderDialogs(std::string& filePathName,
                     std::string& fileName,
                     bool&        show_edit_contour);
  void renderMenuBar(bool& show_job_options, bool& show_tool_library);
  void renderContextMenus(bool& show_edit_contour);
  void renderPropertiesWindow(Part*& selected_part);
  void renderLeftPane(bool&  show_create_operation,
                      bool&  show_job_options,
                      bool&  show_tool_library,
                      bool&  show_new_tool,
                      int&   show_tool_edit,
                      int&   show_edit_tool_operation,
                      Part*& selected_part);
  void renderPartsViewer(Part*& selected_part);
  void renderOperationsViewer(bool& show_create_operation, int& show_edit_tool_operation);
  void renderLayersViewer();

  // Action handlers
  void handlePostProcessAction(const action_t& action);
  void handleRebuildToolpathsAction(const action_t& action);
  void handleDeleteOperationAction(const action_t& action);
  void handleDeleteDuplicatePartsAction(const action_t& action);
  void handleDeletePartsAction(const action_t& action);
  void handleDeletePartAction(const action_t& action);
  void handleDuplicatePartAction(const action_t& action);

  // Iteration helpers for Part management
  template <typename Func> void forEachPart(Func&& func);
  template <typename Func> void forEachVisiblePart(Func&& func);
  Part*                         findPartByName(const std::string& name);
  std::vector<Part*>            getAllParts();
  std::vector<std::string>      getAllLayers();

  Point2d m_show_viewer_context_menu;
  Point2d m_last_mouse_click_position;
  bool    m_left_click_pressed;
  enum class JetCamTool : int { Contour, Nesting, Point };
  JetCamTool                           m_current_tool;
  std::unique_ptr<FILE, FileDeleter>   m_dxf_fp;
  std::unique_ptr<DL_Dxf>              m_dl_dxf;
  PolyNest::PolyNest                   m_dxf_nest;
  std::unique_ptr<DXFParsePathAdaptor> m_dxf_creation_interface;
  bool                                 dxfFileOpen(std::string filename,
                                                   std::string name,
                                                   int         import_quality,
                                                   float       import_scale);
  static bool                          dxfFileParseTimer(void* p);

  float                      m_progress_window_progress;
  NcRender::NcRenderGui* m_progress_window_handle;
  void                       showProgressWindow(bool v);
  static void                renderProgressWindow(void* p);

public:
  Part*  m_mouse_over_part;
  Part*  m_edit_contour_part;
  size_t m_mouse_over_path;
  size_t m_edit_contour_path;

  std::string m_mouse_over_color;
  std::string m_outside_contour_color;
  std::string m_inside_contour_color;
  std::string m_open_contour_color;

  Preferences                m_preferences;
  JobOptions                 m_job_options;
  std::vector<ToolData>      m_tool_library;
  std::vector<ToolOperation> m_toolpath_operations;
  Box*                       m_material_plane;

  // Application context dependency (injected)
  NcApp* m_app{ nullptr };

  explicit NcCamView(NcApp* app)
    : View(app), m_app(app), m_current_tool(JetCamTool::Contour),
      m_dxf_fp(nullptr), m_dl_dxf(nullptr),
      m_dxf_creation_interface(nullptr) {};

  // Move semantics (non-copyable due to resource management)
  NcCamView(const NcCamView&) = delete;
  NcCamView& operator=(const NcCamView&) = delete;
  NcCamView(NcCamView&&) = default;
  NcCamView& operator=(NcCamView&&) = default;
  void        deletePart(std::string part_name);
  void        preInit();
  void        init();
  void        tick() override;
  void        makeActive();
  void        close();

  // View interface implementation
  void handleScrollEvent(const ScrollEvent& e,
                         const InputState&  input) override;
  void handleMouseEvent(const MouseButtonEvent& e,
                        const InputState&       input) override;
  void handleKeyEvent(const KeyEvent& e, const InputState& input) override;
  void handleMouseMoveEvent(const MouseMoveEvent& e,
                            const InputState&     input) override;
};

#endif
