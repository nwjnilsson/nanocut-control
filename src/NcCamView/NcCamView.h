#ifndef JET_CAM_VIEW_
#define JET_CAM_VIEW_

// Standard library includes
#include <atomic>
#include <cstdio>
#include <memory>
#include <thread>

// System includes
#include <NcRender/NcRender.h>

// Project includes
#include <NanoCut.h>
#include <NcApp/View.h>
#include <dxflib/dl_dxf.h>

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

enum class BackgroundOperationType {
  None,
  Nesting,
  DxfImport,
  ToolpathGeneration  // Future extensibility
};

struct BackgroundOperationState {
  BackgroundOperationType type = BackgroundOperationType::None;
  std::atomic<bool>       in_progress{false};
  std::atomic<float>      progress{0.0f};       // 0.0 to 1.0
  std::string             status_text;          // Set once before thread starts
  std::atomic<int>        failed_count{0};      // For operations that can partially fail

  void reset() {
    type = BackgroundOperationType::None;
    in_progress = false;
    progress = 0.0f;
    status_text.clear();
    failed_count = 0;
  }
};

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
    std::string tool_name;
    float       pierce_height = 1.0f;
    float       pierce_delay = 1.0f;
    float       cut_height = 1.0f;
    float       kerf_width = DEFAULT_KERF_WIDTH;
    float       feed_rate = 1.0f;
    float       thc = 0.0f;
  };

  // JSON serialization for ToolData
  static void to_json(nlohmann::json& j, const ToolData& tool);
  static void from_json(const nlohmann::json& j, ToolData& tool);
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

  // Polymorphic Command Pattern for CAM Actions
  class CamAction {
  public:
    virtual ~CamAction() = default;
    virtual void execute(NcCamView* view) = 0;
  };

  class PostProcessAction : public CamAction {
    std::string m_file;

  public:
    explicit PostProcessAction(std::string file) : m_file(std::move(file)) {}
    void execute(NcCamView* view) override;
  };

  class RebuildToolpathsAction : public CamAction {
  public:
    void execute(NcCamView* view) override;
  };

  class DeleteOperationAction : public CamAction {
    size_t m_index;

  public:
    explicit DeleteOperationAction(size_t index) : m_index(index) {}
    void execute(NcCamView* view) override;
  };

  class DeleteDuplicatePartsAction : public CamAction {
    std::string m_part_name;

  public:
    explicit DeleteDuplicatePartsAction(std::string part_name)
      : m_part_name(std::move(part_name))
    {
    }
    void execute(NcCamView* view) override;
  };

  class DeletePartsAction : public CamAction {
    std::string m_part_name;

  public:
    explicit DeletePartsAction(std::string part_name)
      : m_part_name(std::move(part_name))
    {
    }
    void execute(NcCamView* view) override;
  };

  class DeletePartAction : public CamAction {
    std::string m_part_name;

  public:
    explicit DeletePartAction(std::string part_name)
      : m_part_name(std::move(part_name))
    {
    }
    void execute(NcCamView* view) override;
  };

  class SendToControllerAction : public CamAction {
  public:
    void execute(NcCamView* view) override;
  };

  class DuplicatePartAction : public CamAction {
    std::string m_part_name;

  public:
    explicit DuplicatePartAction(std::string part_name)
      : m_part_name(std::move(part_name))
    {
    }
    void execute(NcCamView* view) override;
  };

  class ArrangePartsAction : public CamAction {
  public:
    void execute(NcCamView* view) override;
  };

  std::vector<std::unique_ptr<CamAction>> m_action_stack;
  NcRender::NcRenderGui*                  m_menu_bar;
  void zoomEventCallback(const ScrollEvent& e, const InputState& input);
  void mouseEventCallback(Primitive* c, const Primitive::MouseEventData& e);
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
  void renderOperationsViewer(bool& show_create_operation,
                              int&  show_edit_tool_operation);
  void reevaluateContours();
  std::vector<std::string> generateGCode();

  // Iteration helpers for Part management
  template <typename Func> void forEachPart(Func&& func);
  template <typename Func> void forEachVisiblePart(Func&& func);
  Part*                         findPartByName(const std::string& name);
  std::vector<Part*>            getAllParts();
  std::vector<std::string>      getAllLayers();

  // Nesting helpers
  void resetNesting();
  std::vector<std::vector<PolyNest::PolyPoint>> collectOutsideContours(
    Part* part);

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

  // Unified threading for all background operations
  std::unique_ptr<std::thread> m_background_thread;
  BackgroundOperationState     m_operation;
  void                         startNestingThread();
  void                         startImportThread();

public:
  Part*  m_mouse_over_part;
  Part*  m_edit_contour_part;
  size_t m_mouse_over_path;
  size_t m_edit_contour_path;

  Color m_mouse_over_color;
  Color m_outside_contour_color;
  Color m_inside_contour_color;
  Color m_open_contour_color;

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
  void       deletePart(std::string part_name);
  void       preInit();
  void       init();
  void       tick() override;
  void       makeActive();
  void       close();

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
