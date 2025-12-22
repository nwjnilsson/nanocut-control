#include "NcCamView.h"
#include "../application/NcApp.h"
#include "../input/InputEvents.h"
#include "../input/InputState.h"
#include "DXFParsePathAdaptor/DXFParsePathAdaptor.h"
#include "PolyNest/PolyNest.h"
#include "NcControlView/NcControlView.h"
#include <NcRender/NcRender.h>
#include <NcRender/gui/ImGuiFileDialog.h>
#include <NcRender/gui/imgui.h>
#include <NcRender/logging/loguru.h>
#include <dxf/dxflib/dl_dxf.h>
#include <NcControlView/util.h>

// Static reference to current instance for callback access

void NcCamView::zoomEventCallback(const ScrollEvent& e,
                                   const InputState&  input)
{
  if (!m_app)
    return;

  if (m_current_tool == JetCamTool::Nesting) {
    for (auto it = m_app->getRenderer().getPrimitiveStack().begin();
         it != m_app->getRenderer().getPrimitiveStack().end();
         ++it) {
      if ((*it)->view == m_app->getRenderer().getCurrentView()) {
        auto* part = dynamic_cast<Part*>(it->get());
        if (part and part->m_is_part_selected) {
          if (e.y_offset > 0) {
            if (input.isCtrlPressed()) {
              part->m_control.angle += 5;
            }
            if (input.isShiftPressed()) {
              part->m_control.scale += .1f;
            }
          }
          else {
            if (input.isCtrlPressed()) {
              part->m_control.angle -= 5;
            }
            if (input.isShiftPressed()) {
              part->m_control.scale -= .1f;
            }
          }
        }
      }
    }
  }
  if (input.isCtrlPressed() || input.isShiftPressed()) {
    return;
  }
  // Zoom - use View's zoom functionality
  Point2d screen_center = input.getMousePosition();
  double  zoom_factor = (e.y_offset > 0) ? 1.125 : 0.875;
  adjustZoom(zoom_factor, screen_center);
}

void NcCamView::mouseEventCallback(Primitive* c, const nlohmann::json& e)
{
  // LOG_F(INFO, "%s", e.dump().c_str());
  using EventType = NcRender::EventType;
  NcRender::EventType event{};
  if (!m_app)
    return;

  if (e.contains("event")) {
    event = e.at("event").get<EventType>();
    if (m_current_tool == JetCamTool::Contour) {
      Part* part = dynamic_cast<Part*>(c);
      if (part) {
        if (event == EventType::MouseIn) {
          size_t x = (size_t) e["path_index"];
          m_app->getRenderer().setColorByName(part->m_paths[x].color,
                                              m_mouse_over_color);
          m_mouse_over_part = part;
          m_mouse_over_path = x;
        }
        if (event == EventType::MouseOut) {
          m_mouse_over_part = NULL;
          for (auto& path : part->m_paths) {
            if (path.is_inside_contour == true) {
              if (path.is_closed == true) {
                m_app->getRenderer().setColorByName(path.color,
                                                    m_inside_contour_color);
              }
              else {
                m_app->getRenderer().setColorByName(path.color,
                                                    m_open_contour_color);
              }
            }
            else {
              m_app->getRenderer().setColorByName(path.color,
                                                  m_outside_contour_color);
            }
          }
        }
      }
    }
  }
  if (m_current_tool == JetCamTool::Nesting) {
    Part* part = dynamic_cast<Part*>(c);
    if (part) {
      if (event == EventType::MouseIn) {
        part->m_is_part_selected = true;
        for (auto& path : part->m_paths) {
          m_app->getRenderer().setColorByName(path.color, m_mouse_over_color);
        }
      }
      else if (event == EventType::MouseOut) {
        part->m_is_part_selected = false;
        for (auto& path : part->m_paths) {
          if (path.is_inside_contour == true) {
            if (path.is_closed == true) {
              m_app->getRenderer().setColorByName(path.color,
                                                  m_inside_contour_color);
            }
            else {
              m_app->getRenderer().setColorByName(path.color,
                                                  m_open_contour_color);
            }
          }
          else {
            m_app->getRenderer().setColorByName(path.color,
                                                m_outside_contour_color);
          }
        }
      }
    }
  }
}

void NcCamView::RenderUI()
{
  // ImGui state that persists across frames
  static bool        show_create_operation = false;
  static bool        show_job_options = false;
  static bool        show_tool_library = false;
  static bool        show_new_tool = false;
  static int         show_tool_edit = -1;
  static int         show_edit_tool_operation = -1;
  static bool        show_edit_contour = false;
  static Part*       selected_part = nullptr;
  static std::string filePathName;
  static std::string fileName;

  // Render all UI components
  renderDialogs(filePathName, fileName, show_edit_contour);
  renderMenuBar(show_job_options, show_tool_library);
  renderContextMenus(show_edit_contour);
  renderPropertiesWindow(selected_part);
  renderLeftPane(show_create_operation,
                 show_job_options,
                 show_tool_library,
                 show_new_tool,
                 show_tool_edit,
                 show_edit_tool_operation,
                 selected_part);
}

// ============================================================================
// UI Rendering Helper Methods
// ============================================================================

void NcCamView::renderDialogs(std::string& filePathName,
                               std::string& fileName,
                               bool&        show_edit_contour)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  // Import Part Dialog
  if (ImGuiFileDialog::Instance()->Display(
        "ImportPartDialog", ImGuiWindowFlags_NoCollapse, ImVec2(600, 500))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
      LOG_F(INFO,
            "File Path: %s, File Path Name: %s, File Name: %s",
            filePath.c_str(),
            filePathName.c_str(),
            fileName.c_str());
      auto& renderer = m_app->getRenderer();
      renderer.stringToFile(renderer.getConfigDirectory() +
                              "last_dxf_open_path.conf",
                            filePath + "/");
      ImGui::OpenPopup("DXF Importer");
    }
    ImGuiFileDialog::Instance()->Close();
  }
  if (ImGui::BeginPopupModal(
        "DXF Importer", NULL, ImGuiWindowFlags_AlwaysAutoResize)) {
    ImGui::SetWindowFontScale(1.2f);
    ImGui::Text("Import Parameters");
    ImGui::SetWindowFontScale(1.0f);
    ImGui::Separator();

    static int   import_quality = 10;
    static float import_scale = 1.0;

    ImGui::Dummy(ImVec2{ 0.f, 10.f });
    ImGui::Text(R"(
The importer will detect the units of the DXF and scale it
accordingly. If the original model is very tiny, it is
recommended to apply additional scaling below. Otherwise,
the line segments may not be generated appropriately.

The default import resolution is usually more than enough.
Note that the simplification of the model can be adjusted
in the part's properties after importing.
)");
    ImGui::Dummy(ImVec2{ 0.f, 30.f });

    ImGui::Separator();

    ImGui::SliderInt("Import resolution", &import_quality, 8, 16);
    ImGui::Separator();
    ImGui::InputFloat("Scale", &import_scale);
    ImGui::Separator();

    ImGui::Spacing();

    if (ImGui::Button("Import", ImVec2{ 120, 0 })) {
      dxfFileOpen(filePathName, fileName, import_quality, import_scale);
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();

    if (ImGui::Button("Cancel", ImVec2(120, 0))) {
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
  }
  if (ImGuiFileDialog::Instance()->Display(
        "PostNcDialog", ImGuiWindowFlags_NoCollapse, ImVec2(600, 500))) {
    if (ImGuiFileDialog::Instance()->IsOk()) {
      std::string filePathName = ImGuiFileDialog::Instance()->GetFilePathName();
      std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
      std::string fileName = ImGuiFileDialog::Instance()->GetCurrentFileName();
      LOG_F(INFO,
            "File Path: %s, File Path Name: %s, File Name: %s",
            filePath.c_str(),
            filePathName.c_str(),
            fileName.c_str());
      auto& renderer = m_app->getRenderer();
      renderer.stringToFile(renderer.getConfigDirectory() +
                              "last_nc_post_path.conf",
                            filePath + "/");
      action_t action;
      action.action_id = "post_process";
      action.data["file"] = filePathName;
      m_action_stack.push_back(action);
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Edit Contour Dialog
  if (show_edit_contour == true) {
    ImGui::Begin(
      "Edit Contour", &show_edit_contour, ImGuiWindowFlags_AlwaysAutoResize);
    Part* part = dynamic_cast<Part*>(m_edit_contour_part);
    char  layer[1024];
    if (part) {
      sprintf(layer, "%s", part->m_paths[m_edit_contour_path].layer.c_str());
      ImGui::InputText("Layer", layer, IM_ARRAYSIZE(layer));
      part->m_paths[m_edit_contour_path].layer = std::string(layer);
    }
    if (ImGui::Button("Ok")) {
      action_t action;
      action.action_id = "rebuild_part_toolpaths";
      m_action_stack.push_back(action);
      show_edit_contour = false;
    }
    ImGui::End();
  }
}

void NcCamView::renderMenuBar(bool& show_job_options, bool& show_tool_library)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  if (ImGui::BeginMainMenuBar()) {
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Post Process", "")) {
        LOG_F(INFO, "File->Post Process");
        std::string path = ".";
        auto&       renderer = m_app->getRenderer();
        std::string p = renderer.fileToString(renderer.getConfigDirectory() +
                                              "last_nc_post_path.conf");
        if (p != "") {
          path = p;
        }
        ImGuiFileDialog::Instance()->OpenDialog(
          "PostNcDialog", "Choose File", ".nc", path.c_str());
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Save Job", "")) {
      }
      if (ImGui::MenuItem("Open Job", "")) {
      }
      ImGui::Separator();
      if (ImGui::MenuItem("New Part", "")) {
        LOG_F(INFO, "File->Open");
        std::string path = ".";
        auto&       renderer = m_app->getRenderer();
        std::string p = renderer.fileToString(renderer.getConfigDirectory() +
                                              "last_dxf_open_path.conf");
        if (p != "") {
          path = p;
        }
        ImGuiFileDialog::Instance()->OpenDialog(
          "ImportPartDialog", "Choose File", ".dxf", path.c_str());
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Close", "")) {
        LOG_F(INFO, "Edit->Close");
        m_app->requestQuit();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit")) {
      if (ImGui::MenuItem("Job Options", "")) {
        show_job_options = true;
      }
      if (ImGui::MenuItem("Tool Library", "")) {
        show_tool_library = true;
      }
      ImGui::Separator();
      /*if (ImGui::MenuItem("Preferences", ""))
      {
          LOG_F(INFO, "Edit->Preferences");
      }*/
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Workbench")) {
      if (ImGui::MenuItem("Machine Control", "")) {
        LOG_F(INFO, "Workbench->Machine Control");
        m_app->getControlView().makeActive();
      }
      ImGui::Separator();
      if (ImGui::MenuItem("CAM Toolpaths", "")) {
        LOG_F(INFO, "Workbench->CAM Toolpaths");
        makeActive();
      }
      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Help")) {
      if (ImGui::MenuItem("Dump Stack", "")) {
        LOG_F(INFO, "Help->Dump Stack");
        auto& renderer = m_app->getRenderer();
        renderer.dumpJsonToFile("stack.json", renderer.dumpPrimitiveStack());
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Documentation", "")) {
        LOG_F(INFO, "Help->Documentation");
      }
      ImGui::Separator();
      if (ImGui::MenuItem("About", "")) {
        LOG_F(INFO, "Help->About");
      }
      ImGui::EndMenu();
    }
    ImGui::RadioButton("Contour Tool", (int*) &m_current_tool, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Nesting Tool", (int*) &m_current_tool, 1);
    ImGui::SameLine();
    ImGui::RadioButton("Point Tool", (int*) &m_current_tool, 2);
    ImGui::EndMainMenuBar();
  }
}

void NcCamView::renderContextMenus(bool& show_edit_contour)
{
  if (!m_app)
    return;

  // Viewer context menu
  if (m_show_viewer_context_menu.x != -inf<double>() &&
      m_show_viewer_context_menu.y != -inf<double>()) {
    ImGui::SetNextWindowPos(
      ImVec2(m_show_viewer_context_menu.x, m_show_viewer_context_menu.y));
    ImGui::Begin("View Contect Menu",
                 NULL,
                 ImGuiWindowFlags_AlwaysAutoResize |
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
    if (ImGui::Button("Edit")) {
      show_edit_contour = true;
      m_edit_contour_part = m_mouse_over_part;
      m_edit_contour_path = m_mouse_over_path;
      m_show_viewer_context_menu = Point2d::infNeg();
    }
    if (ImGui::Button("Delete")) {
      if (m_mouse_over_part) {
        m_mouse_over_part->m_paths.erase(m_mouse_over_part->m_paths.begin() +
                                         m_mouse_over_path);
        m_mouse_over_part->m_last_control.angle = 1; // Trigger rebuild
      }
      m_show_viewer_context_menu = Point2d::infNeg();
    }
    ImGui::End();
  }
}

void NcCamView::renderPropertiesWindow(Part*& selected_part)
{
  if (selected_part != NULL) {
    ImGui::Begin("Properties", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Width: %.4f, Height: %.4f",
                (selected_part->m_bb_max.x - selected_part->m_bb_min.x),
                (selected_part->m_bb_max.y - selected_part->m_bb_min.y));
    ImGui::Text("Number of Paths: %lu", selected_part->m_paths.size());
    ImGui::Text("Number of Vertices: %lu",
                selected_part->m_number_of_verticies);
    ImGui::Text("Offset X: %.4f", selected_part->m_control.offset.x);
    ImGui::Text("Offset Y: %.4f", selected_part->m_control.offset.y);
    ImGui::InputDouble("Scale", &selected_part->m_control.scale);
    ImGui::InputDouble("Angle", &selected_part->m_control.angle);
    ImGui::SliderFloat(
      "Simplification", &selected_part->m_control.smoothing, 0.f, SCALE(5.f));
    if (ImGui::Button("Close")) {
      selected_part = NULL;
    }
    ImGui::End();
  }
}

void NcCamView::renderLeftPane(bool&  show_create_operation,
                                bool&  show_job_options,
                                bool&  show_tool_library,
                                bool&  show_new_tool,
                                int&   show_tool_edit,
                                int&   show_edit_tool_operation,
                                Part*& selected_part)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  // Job Options Dialog
  if (show_job_options == true) {
    ImGui::Begin(
      "Job Options", &show_job_options, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::Text("Material Size");
    ImGui::InputFloat("Width", &m_job_options.material_size[0]);
    ImGui::SameLine();
    ImGui::InputFloat("Height", &m_job_options.material_size[1]);
    ImGui::Text("Origin Corner");
    ImGui::RadioButton("Top Left", &m_job_options.origin_corner, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Top Right", &m_job_options.origin_corner, 1);
    ImGui::RadioButton("Bottom Left", &m_job_options.origin_corner, 2);
    ImGui::SameLine();
    ImGui::RadioButton("Bottom Right", &m_job_options.origin_corner, 3);
    switch (m_job_options.origin_corner) {
      case 0:
        m_material_plane->m_bottom_left = { 0,
                                            -m_job_options.material_size[1] };
        m_material_plane->m_width = m_job_options.material_size[0];
        m_material_plane->m_height = m_job_options.material_size[1];
        break;
      case 1:
        m_material_plane->m_bottom_left = { -m_job_options.material_size[0],
                                            -m_job_options.material_size[1] };
        m_material_plane->m_width = m_job_options.material_size[0];
        m_material_plane->m_height = m_job_options.material_size[1];
        break;
      case 2:
        m_material_plane->m_bottom_left = { 0, 0 };
        m_material_plane->m_width = m_job_options.material_size[0];
        m_material_plane->m_height = m_job_options.material_size[1];
        break;
      case 3:
        m_material_plane->m_bottom_left = { -m_job_options.material_size[0],
                                            0 };
        m_material_plane->m_width = m_job_options.material_size[0];
        m_material_plane->m_height = m_job_options.material_size[1];
        break;
      default:
        break;
    }
    if (ImGui::Button("Close")) {
      show_job_options = false;
    }
    ImGui::End();
  }

  // Create Operation Dialog
  if (show_create_operation == true) {
    ImGui::Begin("Create Operation",
                 &show_create_operation,
                 ImGuiWindowFlags_AlwaysAutoResize);
    static ToolOperation operation;
    static int           operation_tool = -1;

    // Auto-set lead in/out based on tool kerf width
    if (m_tool_library.size() > static_cast<size_t>(operation_tool) &&
        operation_tool != -1 &&
        operation.lead_in_length == DEFAULT_LEAD_IN &&
        operation.lead_out_length == DEFAULT_LEAD_OUT) {
      operation.lead_in_length =
        m_tool_library[operation_tool].params.at("kerf_width") * 1.5;
      operation.lead_out_length =
        m_tool_library[operation_tool].params.at("kerf_width") * 1.5;
    }

    // Tool selection combo
    if (ImGui::BeginCombo("Choose Tool",
                          operation_tool >= 0 && operation_tool < static_cast<int>(m_tool_library.size())
                          ? m_tool_library[operation_tool].tool_name
                          : "Select...")) {
      for (size_t i = 0; i < m_tool_library.size(); i++) {
        bool is_selected = (operation_tool == static_cast<int>(i));
        if (ImGui::Selectable(m_tool_library[i].tool_name, is_selected)) {
          operation_tool = i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Layer selection combo
    static int                   layer_selection = -1;
    std::vector<std::string>     layers = getAllLayers();
    if (ImGui::BeginCombo("Choose Layer",
                          layer_selection >= 0 && layer_selection < static_cast<int>(layers.size())
                          ? layers[layer_selection].c_str()
                          : "Select...")) {
      for (size_t i = 0; i < layers.size(); i++) {
        bool is_selected = (layer_selection == static_cast<int>(i));
        if (ImGui::Selectable(layers[i].c_str(), is_selected)) {
          layer_selection = i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Lead in/out settings
    if (operation_tool != -1) {
      ImGui::InputDouble("Lead In Length", &operation.lead_in_length);
      ImGui::InputDouble("Lead Out Length", &operation.lead_out_length);
    }

    if (ImGui::Button("OK")) {
      if (operation_tool != -1 && layer_selection != -1) {
        if (operation.lead_in_length < 0)
          operation.lead_in_length = 0;
        if (operation.lead_out_length < 0)
          operation.lead_out_length = 0;
        operation.enabled = true;
        operation.layer = layers[layer_selection];
        operation.tool_number = operation_tool;
        operation.type = OpType::Cut;
        m_toolpath_operations.push_back(operation);
        operation.lead_in_length = DEFAULT_LEAD_IN;
        operation.lead_out_length = DEFAULT_LEAD_OUT;
        operation_tool = -1;
        layer_selection = -1;
        show_create_operation = false;
      }
    }
    ImGui::End();
  }

  // New Tool Dialog
  if (show_new_tool == true) {
    static ToolData tool;
    ImGui::Begin("New Tool", &show_new_tool, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText("Tool Name", tool.tool_name, IM_ARRAYSIZE(tool.tool_name));

    for (auto& [key, value] : tool.params) {
      ImGui::InputFloat(key.c_str(), &value);
    }

    if (ImGui::Button("OK")) {
      bool skip_save = false;
      for (const auto& [key, value] : tool.params) {
        if (value < 0.f || value > 50000.f) {
          LOG_F(WARNING, "Invalid tool input parameters.");
          skip_save = true;
          break;
        }
      }
      if (!skip_save) {
        m_tool_library.push_back(tool);

        // Save to file
        nlohmann::json tool_library;
        for (size_t x = 0; x < m_tool_library.size(); x++) {
          nlohmann::json tool_json;
          tool_json["tool_name"] = std::string(m_tool_library[x].tool_name);
          for (const auto& [key, value] : m_tool_library[x].params) {
            tool_json[key] = value;
          }
          tool_library.push_back(tool_json);
        }

        renderer.dumpJsonToFile(renderer.getConfigDirectory() + "tool_library.json",
                                tool_library);
        show_new_tool = false;

        // Reset tool data for next use
        memset(tool.tool_name, 0, sizeof(tool.tool_name));
        tool.params = {
          { "pierce_height", 1.f }, { "pierce_delay", 1.f },
          { "cut_height", 1.f },    { "kerf_width", DEFAULT_KERF_WIDTH },
          { "feed_rate", 1.f },     { "thc", 0.f }
        };
      }
    }
    ImGui::End();
  }

  // Edit Tool Dialog
  static ToolData edit_tool;
  if (show_tool_edit != -1) {
    if (!edit_tool.tool_name[0]) {
      sprintf(edit_tool.tool_name, "%s", m_tool_library[show_tool_edit].tool_name);
      edit_tool.params = m_tool_library[show_tool_edit].params;
    }

    ImGui::Begin("Tool Edit", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText("Tool Name",
                     m_tool_library[show_tool_edit].tool_name,
                     IM_ARRAYSIZE(m_tool_library[show_tool_edit].tool_name));

    for (const auto& [key, value] : m_tool_library[show_tool_edit].params) {
      ImGui::InputFloat(key.c_str(), &edit_tool.params[key]);
    }

    if (ImGui::Button("OK")) {
      bool skip_save = false;
      for (const auto& [key, value] : edit_tool.params) {
        if (value < 0.f || value > 50000.f) {
          LOG_F(WARNING, "Invalid tool input parameters.");
          skip_save = true;
          break;
        }
      }
      if (!skip_save) {
        m_tool_library[show_tool_edit].params = edit_tool.params;

        // Save to file
        nlohmann::json tool_library;
        for (size_t x = 0; x < m_tool_library.size(); x++) {
          nlohmann::json tool_json;
          tool_json["tool_name"] = std::string(m_tool_library[x].tool_name);
          for (const auto& [key, value] : m_tool_library[x].params) {
            tool_json[key] = value;
          }
          tool_library.push_back(tool_json);
        }

        renderer.dumpJsonToFile(renderer.getConfigDirectory() + "tool_library.json",
                                tool_library);
        show_tool_edit = -1;
      }
    }
    ImGui::End();
  }
  else {
    memset(edit_tool.tool_name, 0, sizeof(edit_tool.tool_name));
  }

  // Tool Library Dialog
  if (show_tool_library == true) {
    ImGui::Begin("Tool Library", &show_tool_library, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::BeginTable("Tools", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
      ImGui::TableSetupColumn("Tool Name");
      ImGui::TableSetupColumn("Pierce Height");
      ImGui::TableSetupColumn("Pierce Delay");
      ImGui::TableSetupColumn("Cut Height");
      ImGui::TableSetupColumn("Kerf Width");
      ImGui::TableSetupColumn("Feed Rate");
      ImGui::TableSetupColumn("THC");
      ImGui::TableSetupColumn("Action");
      ImGui::TableHeadersRow();

      for (size_t x = 0; x < m_tool_library.size(); ++x) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", m_tool_library[x].tool_name);
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.4f", m_tool_library[x].params.at("pierce_height"));
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.4f", m_tool_library[x].params.at("pierce_delay"));
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.4f", m_tool_library[x].params.at("cut_height"));
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.4f", m_tool_library[x].params.at("kerf_width"));
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.4f", m_tool_library[x].params.at("feed_rate"));
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.4f", m_tool_library[x].params.at("thc"));
        ImGui::TableSetColumnIndex(7);
        if (ImGui::Button(std::string("Edit##Edit-" + std::to_string(x)).c_str())) {
          show_tool_edit = x;
          LOG_F(INFO, "show_tool_edit: %d", show_tool_edit);
        }
      }
      ImGui::EndTable();
    }
    if (ImGui::Button("New Tool")) {
      show_new_tool = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Close")) {
      show_tool_library = false;
    }
    ImGui::End();
  }

  // Edit Tool Operation Dialog
  if (show_edit_tool_operation != -1) {
    ImGui::Begin("Edit Operation", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputDouble(
      "Lead In Length",
      &m_toolpath_operations[show_edit_tool_operation].lead_in_length);
    ImGui::InputDouble(
      "Lead Out Length",
      &m_toolpath_operations[show_edit_tool_operation].lead_out_length);
    if (ImGui::Button("OK")) {
      m_toolpath_operations[show_edit_tool_operation].last_enabled = false;
      show_edit_tool_operation = -1;
    }
    ImGui::End();
  }

  // Main left pane
  ImGui::SetNextWindowPos(ImVec2(0, 0));
  ImGui::SetNextWindowSize(ImVec2(300, renderer.getWindowSize().y));
  ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(1.0f, 1.0f, 1.0f, 0.8f));
  ImGui::Begin("LeftPane",
               NULL,
               ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                 ImGuiWindowFlags_NoMove |
                 ImGuiWindowFlags_NoBringToFrontOnFocus |
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);
  ImGui::PopStyleColor();

  renderPartsViewer(selected_part);
  ImGui::Separator();
  renderOperationsViewer(show_create_operation, show_edit_tool_operation);
  ImGui::Separator();
  renderLayersViewer();

  ImGui::End();
}

void NcCamView::renderPartsViewer(Part*& selected_part)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  if (!ImGui::BeginTable("parts_view_table",
                         1,
                         ImGuiTableFlags_SizingFixedFit |
                           ImGuiTableFlags_Reorderable |
                           ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders))
    return;

  ImGui::TableSetupColumn("Parts Viewer");
  ImGui::TableHeadersRow();

  forEachPart([&](Part* part) {
    if (part->m_part_name.find(":") != std::string::npos)
      return;

    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Checkbox(std::string("##" + part->m_part_name).c_str(),
                    &part->visible);
    ImGui::SameLine();
    bool tree_node_open = ImGui::TreeNode(part->m_part_name.c_str());
    if (ImGui::BeginPopupContextItem()) {
      if (ImGui::MenuItem("New Part")) {
        std::string path = ".";
        std::string p = renderer.fileToString(renderer.getConfigDirectory() +
                                              "last_dxf_open_path.conf");
        if (p != "") {
          path = p;
        }
        ImGuiFileDialog::Instance()->OpenDialog(
          "ImportPartDialog", "Choose File", ".dxf", path.c_str());
      }
      ImGui::Separator();
      if (ImGui::MenuItem("New Duplicate")) {
        action_t action;
        action.action_id = "duplicate_part";
        action.data["part_name"] = part->m_part_name;
        m_action_stack.push_back(action);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Delete Master Part")) {
        LOG_F(INFO, "Delete Master Part: %s", part->m_part_name.c_str());
        action_t action;
        action.action_id = "delete_parts";
        action.data["part_name"] = part->m_part_name;
        m_action_stack.push_back(action);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Properties")) {
        selected_part = part;
      }
      ImGui::EndPopup();
    }
    if (tree_node_open) {
      // Render duplicates
      ImGui::TreePop();
    }
  });

  ImGui::EndTable();
}

void NcCamView::renderOperationsViewer(bool& show_create_operation, int& show_edit_tool_operation)
{
  if (!ImGui::BeginTable("operations_view_table",
                         1,
                         ImGuiTableFlags_SizingFixedFit |
                           ImGuiTableFlags_Reorderable |
                           ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders))
    return;

  ImGui::TableSetupColumn("Operations Viewer");
  ImGui::TableHeadersRow();

  // Add "New Operation" button
  ImGui::TableNextRow();
  ImGui::TableSetColumnIndex(0);
  if (ImGui::Button("New Operation")) {
    show_create_operation = true;
  }

  for (size_t x = 0; x < m_toolpath_operations.size(); x++) {
    auto& operation = m_toolpath_operations[x];
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Checkbox(std::string("##operation-" + std::to_string(x)).c_str(),
                    &operation.enabled);
    ImGui::SameLine();
    if (operation.type == OpType::Cut) {
      ImGui::Text("Jet: %s", operation.layer.c_str());
    }
    ImGui::SameLine();
    if (ImGui::Button(std::string("Edit##edit-op-" + std::to_string(x)).c_str())) {
      show_edit_tool_operation = x;
    }
    ImGui::SameLine();
    if (ImGui::Button(std::string("Delete##del-op-" + std::to_string(x)).c_str())) {
      action_t action;
      action.action_id = "delete_operation";
      action.data["index"] = x;
      m_action_stack.push_back(action);
    }
  }
  ImGui::EndTable();
}

void NcCamView::renderLayersViewer()
{
  if (!m_app)
    return;

  if (!ImGui::BeginTable("layers_table",
                         1,
                         ImGuiTableFlags_SizingFixedFit |
                           ImGuiTableFlags_Reorderable |
                           ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders))
    return;

  ImGui::TableSetupColumn("Layers Viewer");
  ImGui::TableHeadersRow();

  std::vector<std::string> layers = getAllLayers();
  for (const auto& layer : layers) {
    ImGui::TableNextRow();
    ImGui::TableSetColumnIndex(0);
    ImGui::Text("%s", layer.c_str());
  }

  ImGui::EndTable();
}

void NcCamView::deletePart(std::string part_name)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  // Use modern algorithm instead of manual loop
  auto& stack = renderer.getPrimitiveStack();
  stack.erase(std::remove_if(stack.begin(),
                             stack.end(),
                             [&](const auto& primitive) {
                               auto* part =
                                 dynamic_cast<Part*>(primitive.get());
                               return part &&
                                      part->view == renderer.getCurrentView() &&
                                      part->m_part_name == part_name;
                             }),
              stack.end());
}

// ============================================================================
// Iteration Helper Methods
// ============================================================================

template <typename Func> void NcCamView::forEachPart(Func&& func)
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();
  for (auto& primitive : renderer.getPrimitiveStack()) {
    if (auto* part = dynamic_cast<Part*>(primitive.get())) {
      if (part->view == renderer.getCurrentView()) {
        func(part);
      }
    }
  }
}

template <typename Func> void NcCamView::forEachVisiblePart(Func&& func)
{
  forEachPart([&](Part* part) {
    if (part->visible) {
      func(part);
    }
  });
}

Part* NcCamView::findPartByName(const std::string& name)
{
  Part* result = nullptr;
  forEachPart([&](Part* part) {
    if (part->m_part_name == name && !result) {
      result = part;
    }
  });
  return result;
}

std::vector<Part*> NcCamView::getAllParts()
{
  std::vector<Part*> parts;
  forEachPart([&](Part* part) { parts.push_back(part); });
  return parts;
}

std::vector<std::string> NcCamView::getAllLayers()
{
  std::vector<std::string> layers;
  forEachPart([&](Part* part) {
    for (const auto& path : part->m_paths) {
      if (std::find(layers.begin(), layers.end(), path.layer) == layers.end()) {
        layers.push_back(path.layer);
      }
    }
  });
  return layers;
}

// ============================================================================
// Action Handler Methods
// ============================================================================

void NcCamView::handlePostProcessAction(const action_t& action)
{
  if (!m_app)
    return;
  auto&         renderer = m_app->getRenderer();
  std::ofstream gcode_file;
  std::string   file = action.data.at("file").get<std::string>();
  gcode_file.open(file);

  if (!gcode_file.is_open()) {
    LOG_F(WARNING, "Could not open gcode file for writing!");
    return;
  }

  for (size_t i = 0; i < m_toolpath_operations.size(); i++) {
    LOG_F(INFO,
          "Posting toolpath operation: %lu on layer: %s",
          i,
          m_toolpath_operations[i].layer.c_str());

    forEachVisiblePart([&](Part* part) {
      std::vector<std::vector<Point2d>> tool_paths =
        part->getOrderedToolpaths();

      if (m_tool_library.size() > m_toolpath_operations[i].tool_number) {
        // Output tool-specific THC target
        if (float thc_val =
              m_tool_library[m_toolpath_operations[i].tool_number].params.at(
                "thc");
            thc_val > 0) {
          gcode_file << "$T=" << thc_val << "\n";
        }

        for (size_t x = 0; x < tool_paths.size(); x++) {
          gcode_file << "G0 X" << tool_paths[x][0].x << " Y"
                     << tool_paths[x][0].y << "\n";
          gcode_file
            << "fire_torch "
            << m_tool_library[m_toolpath_operations[i].tool_number].params.at(
                 "pierce_height")
            << " "
            << m_tool_library[m_toolpath_operations[i].tool_number].params.at(
                 "pierce_delay")
            << " "
            << m_tool_library[m_toolpath_operations[i].tool_number].params.at(
                 "cut_height")
            << "\n";
          for (size_t z = 0; z < tool_paths[x].size(); z++) {
            gcode_file << "G1 X" << tool_paths[x][z].x << " Y"
                       << tool_paths[x][z].y << " F"
                       << m_tool_library[m_toolpath_operations[i].tool_number]
                            .params.at("feed_rate")
                       << "\n";
          }
          gcode_file << "torch_off\n";
        }
      }
    });
  }

  gcode_file << "M30\n";
  gcode_file.close();
  LOG_F(INFO, "Finished writing gcode file!");
}

void NcCamView::handleRebuildToolpathsAction(const action_t& action)
{
  forEachPart([](Part* part) { part->m_last_control.angle = 1; });

  for (size_t x = 0; x < m_toolpath_operations.size(); x++) {
    m_toolpath_operations[x].last_enabled = false;
  }
}

void NcCamView::handleDeleteOperationAction(const action_t& action)
{
  size_t index = action.data.at("index").get<size_t>();
  if (m_toolpath_operations.size() > index) {
    m_toolpath_operations.erase(m_toolpath_operations.begin() + index);
  }
}

void NcCamView::handleDeleteDuplicatePartsAction(const action_t& action)
{
  if (!m_app)
    return;
  auto&       renderer = m_app->getRenderer();
  std::string part_name = action.data.at("part_name").get<std::string>();

  auto& stack = renderer.getPrimitiveStack();
  stack.erase(std::remove_if(stack.begin(),
                             stack.end(),
                             [&](const auto& primitive) {
                               auto* part =
                                 dynamic_cast<Part*>(primitive.get());
                               return part &&
                                      part->view == renderer.getCurrentView() &&
                                      part->m_part_name.find(part_name + ":") !=
                                        std::string::npos;
                             }),
              stack.end());
}

void NcCamView::handleDeletePartsAction(const action_t& action)
{
  if (!m_app)
    return;
  auto&       renderer = m_app->getRenderer();
  std::string part_name = action.data.at("part_name").get<std::string>();

  auto& stack = renderer.getPrimitiveStack();
  stack.erase(std::remove_if(
                stack.begin(),
                stack.end(),
                [&](const auto& primitive) {
                  auto* part = dynamic_cast<Part*>(primitive.get());
                  return part && part->view == renderer.getCurrentView() &&
                         part->m_part_name.find(part_name) != std::string::npos;
                }),
              stack.end());
}

void NcCamView::handleDeletePartAction(const action_t& action)
{
  if (!m_app)
    return;
  auto&       renderer = m_app->getRenderer();
  std::string part_name = action.data.at("part_name").get<std::string>();

  auto& stack = renderer.getPrimitiveStack();
  for (auto it = stack.begin(); it != stack.end(); ++it) {
    auto* part = dynamic_cast<Part*>(it->get());
    if (part && part->view == renderer.getCurrentView() &&
        part->m_part_name.find(part_name) != std::string::npos) {
      stack.erase(it);
      break;
    }
  }
}

void NcCamView::handleDuplicatePartAction(const action_t& action)
{
  if (!m_app)
    return;
  auto&       renderer = m_app->getRenderer();
  std::string part_name = action.data.at("part_name").get<std::string>();

  // Count duplicates
  int dup_count = 1;
  forEachPart([&](Part* part) {
    if (part->m_part_name.find(part_name + ":") != std::string::npos) {
      dup_count++;
    }
  });

  // Setup nesting
  m_dxf_nest = PolyNest::PolyNest();
  m_dxf_nest.setExtents(
    PolyNest::PolyPoint(m_material_plane->m_bottom_left.x,
                        m_material_plane->m_bottom_left.y),
    PolyNest::PolyPoint(
      m_material_plane->m_bottom_left.x + m_material_plane->m_width,
      m_material_plane->m_bottom_left.y + m_material_plane->m_height));

  // Add existing parts to nesting
  forEachPart([&](Part* part) {
    std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
    for (const auto& path : part->m_paths) {
      if (path.is_inside_contour == false) {
        std::vector<PolyNest::PolyPoint> points;
        points.reserve(path.points.size());
        for (const auto& p : path.points) {
          points.push_back({ p.x, p.y });
        }
        poly_part.push_back(std::move(points));
      }
    }
    m_dxf_nest.pushPlacedPolyPart(poly_part,
                                  &part->m_control.offset.x,
                                  &part->m_control.offset.y,
                                  &part->m_control.angle,
                                  &part->visible);
  });

  // Find and duplicate the master part
  Part* new_part = nullptr;
  forEachPart([&](Part* part) {
    if (part->m_part_name == part_name && !new_part) {
      std::vector<Part::path_t>                     paths;
      std::vector<std::vector<PolyNest::PolyPoint>> poly_part;

      for (const auto& path : part->m_paths) {
        paths.push_back(path);
        if (path.is_inside_contour == false) {
          std::vector<PolyNest::PolyPoint> points;
          points.reserve(path.points.size());
          for (const auto& p : path.points) {
            points.push_back({ p.x, p.y });
          }
          poly_part.push_back(std::move(points));
        }
      }

      new_part = renderer.pushPrimitive<Part>(
        part->m_part_name + ":" + std::to_string(dup_count), paths);

      // Set initial position
      if (dup_count > 1) {
        Part* prev_dup =
          findPartByName(part_name + ":" + std::to_string(dup_count - 1));
        if (prev_dup) {
          new_part->m_control.offset = prev_dup->m_control.offset;
        }
      }
      else {
        new_part->m_control.offset = part->m_control.offset;
      }

      new_part->m_control.scale = part->m_control.scale;
      new_part->mouse_callback = part->mouse_callback;
      new_part->matrix_callback = part->matrix_callback;

      m_dxf_nest.pushUnplacedPolyPart(poly_part,
                                      &new_part->m_control.offset.x,
                                      &new_part->m_control.offset.y,
                                      &new_part->m_control.angle,
                                      &new_part->visible);
    }
  });

  m_dxf_nest.beginPlaceUnplacedPolyParts();
  renderer.pushTimer(
    0, [this]() { return m_dxf_nest.placeUnplacedPolyPartsTick(&m_dxf_nest); });
}

void NcCamView::renderProgressWindow(void* p)
{
  NcCamView* self = reinterpret_cast<NcCamView*>(p);
  if (self != NULL) {
    ImGui::Begin("Progress",
                 &self->m_progress_window_handle->visible,
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ProgressBar(self->m_progress_window_progress, ImVec2(0.0f, 0.0f));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("Progress");
    ImGui::End();
  }
}
void NcCamView::showProgressWindow(bool v)
{
  m_progress_window_progress = 0.0;
  m_progress_window_handle->visible = true;
}

bool NcCamView::dxfFileOpen(std::string filename,
                             std::string name,
                             int         import_quality,
                             float       import_scale)
{
  if (!m_app)
    return false;
  auto& renderer = m_app->getRenderer();
  m_dxf_fp.reset(fopen(filename.c_str(), "rt"));
  if (m_dxf_fp) {
    m_dxf_nest = PolyNest::PolyNest();
    m_dxf_nest.setExtents(
      { m_material_plane->m_bottom_left.x, m_material_plane->m_bottom_left.y },
      { m_material_plane->m_bottom_left.x + m_material_plane->m_width,
        m_material_plane->m_bottom_left.y + m_material_plane->m_height });
    for (auto& pPrimitive : renderer.getPrimitiveStack()) {
      // Copy existing objects
      auto* part = dynamic_cast<Part*>(pPrimitive.get());
      if (part and part->view == renderer.getCurrentView()) {
        std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
        for (const auto& path : part->m_paths) {
          if (path.is_inside_contour == false) {
            std::vector<PolyNest::PolyPoint> points;
            points.reserve(path.points.size());
            for (const Point2d& p : path.points) {
              points.push_back({ p.x, p.y });
            }
            poly_part.push_back(std::move(points));
          }
        }
        m_dxf_nest.pushPlacedPolyPart(poly_part,
                                      &part->m_control.offset.x,
                                      &part->m_control.offset.y,
                                      &part->m_control.angle,
                                      &part->visible);
      }
    }
    m_dl_dxf = std::make_unique<DL_Dxf>();
    auto& renderer = m_app->getRenderer();
    m_dxf_creation_interface = std::make_unique<DXFParsePathAdaptor>(
      &renderer,
      getTransformCallback(),
      [this](Primitive* c, const nlohmann::json& e) {
        mouseEventCallback(c, e);
      },
      this);
    m_dxf_creation_interface->setFilename(name);
    m_dxf_creation_interface->setImportScale(import_scale);
    m_dxf_creation_interface->setImportQuality(import_quality);
    // m_dxf_creation_interface->SetSmoothing(0.030);
    renderer.pushTimer(100, [this]() { return dxfFileParseTimer(this); });
    LOG_F(INFO, "Parsing DXF File: %s", filename.c_str());
    return true;
  }
  return false;
}

bool NcCamView::dxfFileParseTimer(void* p)
{
  NcCamView* self = reinterpret_cast<NcCamView*>(p);
  if (self != NULL && self->m_app) {
    auto& renderer = self->m_app->getRenderer();
    // self->DXFcreationInterface->SetScaleFactor(15.4);
    // self->DXFcreationInterface->SetSmoothing(0.0);
    // self->DXFcreationInterface->SetChainTolorance(0.001);
    while (self->m_dl_dxf->readDxfGroups(
      self->m_dxf_fp.get(), self->m_dxf_creation_interface.get())) {
    }
    LOG_F(INFO, "Successfully parsed DXF groups.");

    // Point2d bbox_min, bbox_max;
    // size_t         vertex_count;
    // self->DXFcreationInterface->
    // self->DXFcreationInterface->GetApproxBoundingBox(
    //   bbox_min, bbox_max, vertex_count);
    // const double area = (bbox_max.x - bbox_min.x) * (bbox_max.y -
    // bbox_min.y); const double avg_density = vertex_count / area; const
    // double target_density =
    //   1 / std::pow(self->DXFcreationInterface->chain_tolerance, 2);

    // LOG_F(INFO,
    //       "avg density = %f, target density = %f",
    //       avg_density,
    //       target_density);

    self->m_dxf_creation_interface->finish();
    for (auto& pPrimitive : renderer.getPrimitiveStack()) {
      auto* part = dynamic_cast<Part*>(pPrimitive.get());
      if (part and pPrimitive->view == renderer.getCurrentView() and
          part->m_part_name == self->m_dxf_creation_interface->m_filename) {
        std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
        for (const auto& path : part->m_paths) {
          if (path.is_inside_contour == false) {
            std::vector<PolyNest::PolyPoint> points;
            points.reserve(path.points.size());
            for (const Point2d& p : path.points) {
              points.push_back({ p.x, p.y });
            }
            poly_part.push_back(std::move(points));
          }
        }
        part->visible = true;
        self->m_dxf_nest.pushUnplacedPolyPart(poly_part,
                                              &part->m_control.offset.x,
                                              &part->m_control.offset.y,
                                              &part->m_control.angle,
                                              &part->visible);
      }
    }
    self->m_dxf_nest.beginPlaceUnplacedPolyParts();
    renderer.pushTimer(0, [self]() {
      return self->m_dxf_nest.placeUnplacedPolyPartsTick(&self->m_dxf_nest);
    });
    // Smart pointers automatically handle cleanup
    self->m_dxf_fp.reset();
    self->m_dxf_creation_interface.reset();
    self->m_dl_dxf.reset();
    return false;
  }
  return false;
}
void NcCamView::preInit()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();
  m_mouse_over_color = "blue";       // wtf
  m_outside_contour_color = "white"; // wtf
  m_inside_contour_color = "grey";   // wtf
  m_open_contour_color = "yellow";   // wtf

  m_show_viewer_context_menu.x = -inf<double>();
  m_show_viewer_context_menu.y = -inf<double>();

  m_preferences.background_color[0] = 58 / 255.0f;
  m_preferences.background_color[1] = 58 / 255.0;
  m_preferences.background_color[2] = 58 / 255.0f;

  m_current_tool = JetCamTool::Contour;
  m_left_click_pressed = false;

  nlohmann::json tool_library = renderer.parseJsonFromFile(
    renderer.getConfigDirectory() + "tool_library.json");
  if (tool_library != NULL) {
    LOG_F(
      INFO,
      "Found %s!",
      std::string(renderer.getConfigDirectory() + "tool_library.json").c_str());
    for (size_t x = 0; x < tool_library.size(); x++) {
      ToolData    tool;
      std::string tool_name = tool_library[x]["tool_name"];
      sprintf(tool.tool_name, "%s", tool_name.c_str());
      for (auto& [key, value] : tool.params) {
        value = static_cast<float>(tool_library[x][key]);
      }
      m_tool_library.push_back(tool);
    }
  }
  else {
    LOG_F(
      INFO,
      "Could not find %s!",
      std::string(renderer.getConfigDirectory() + "tool_library.json").c_str());
  }
}
void NcCamView::init()
{
  if (!m_app)
    return;
  // Modern callback registration using lambdas

  m_app->getRenderer().setCurrentView("NcCamView");
  // Events now handled through view delegation system
  m_menu_bar = m_app->getRenderer().pushGui(true, [this]() { RenderUI(); });
  m_progress_window_handle = m_app->getRenderer().pushGui(
    false, [this]() { renderProgressWindow(this); });
  m_material_plane =
    m_app->getRenderer().pushPrimitive<Box>(Point2d{ 0, 0 },
                                            m_job_options.material_size[0],
                                            m_job_options.material_size[1],
                                            0);
  m_material_plane->id = "material_plane";
  m_material_plane->zindex = -20;
  m_material_plane->color[0] = 100;
  m_material_plane->color[1] = 0;
  m_material_plane->color[2] = 0;
  m_material_plane->matrix_callback = getTransformCallback();
  m_material_plane->mouse_callback =
    [this](Primitive* c, const nlohmann::json& e) { mouseEventCallback(c, e); };
  // TODO: Implement SetShowFPS in NcApp if needed
}
void NcCamView::tick()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  // Update mouse mode for all parts
  forEachPart([this](Part* part) {
    part->m_control.mouse_mode = static_cast<int>(m_current_tool);
  });
  // Update toolpath operations
  for (size_t x = 0; x < m_toolpath_operations.size(); x++) {
    if (m_toolpath_operations[x].enabled !=
        m_toolpath_operations[x].last_enabled) {
      forEachVisiblePart([&](Part* part) {
        for (auto& path : part->m_paths) {
          if (path.layer == m_toolpath_operations[x].layer) {
            if (m_tool_library.size() > m_toolpath_operations[x].tool_number) {
              path.toolpath_offset =
                m_tool_library[m_toolpath_operations[x].tool_number].params.at(
                  "kerf_width");
            }
            path.toolpath_visible = m_toolpath_operations[x].enabled;
            part->m_control.lead_in_length =
              m_toolpath_operations[x].lead_in_length;
            part->m_control.lead_out_length =
              m_toolpath_operations[x].lead_out_length;
          }
        }
      });
    }
    m_toolpath_operations[x].last_enabled = m_toolpath_operations[x].enabled;
  }
  // Process action stack
  while (m_action_stack.size() > 0) {
    action_t action = m_action_stack[0];
    m_action_stack.erase(m_action_stack.begin());

    try {
      if (action.action_id == "post_process") {
        handlePostProcessAction(action);
      }
      else if (action.action_id == "rebuild_part_toolpaths") {
        handleRebuildToolpathsAction(action);
      }
      else if (action.action_id == "delete_operation") {
        handleDeleteOperationAction(action);
      }
      else if (action.action_id == "delete_duplicate_parts") {
        handleDeleteDuplicatePartsAction(action);
      }
      else if (action.action_id == "delete_parts") {
        handleDeletePartsAction(action);
      }
      else if (action.action_id == "delete_part") {
        handleDeletePartAction(action);
      }
      else if (action.action_id == "duplicate_part") {
        handleDuplicatePartAction(action);
      }
    }
    catch (const std::exception& e) {
      LOG_F(ERROR, "(NcCamView::Tick) Exception: %s", e.what());
    }
  }
}
void NcCamView::makeActive()
{
  if (!m_app)
    return;
  // View is now active

  m_app->setCurrentActiveView(this); // Register for event delegation
  m_app->getRenderer().setCurrentView("NcCamView");
  m_app->getRenderer().setClearColor(m_preferences.background_color[0] * 255.0f,
                                     m_preferences.background_color[1] * 255.0f,
                                     m_preferences.background_color[2] *
                                       255.0f);
}
void NcCamView::close() {}

void NcCamView::handleScrollEvent(const ScrollEvent& e,
                                   const InputState&  input)
{
  zoomEventCallback(e, input);
}

void NcCamView::handleMouseEvent(const MouseButtonEvent& e,
                                  const InputState&       input)
{
  if (!m_app)
    return;

  // Check if ImGui wants to capture input
  if (m_app->getRenderer().imgui_io->WantCaptureMouse) {
    m_left_click_pressed = false;
    return;
  }

  // Handle left mouse button
  if (e.button == GLFW_MOUSE_BUTTON_1) {
    if (e.action == GLFW_RELEASE) {
      m_left_click_pressed = false;
    }
    else if (e.action == GLFW_PRESS) {
      m_left_click_pressed = true;
      m_show_viewer_context_menu = Point2d::infNeg();
    }
  }
  // Handle right mouse button
  else if (e.button == GLFW_MOUSE_BUTTON_2) {
    if (e.action == GLFW_PRESS) {
      // Start pan/move view mode
      setMoveViewActive(true);
    }
    else if (e.action == GLFW_RELEASE) {
      // End pan/move view mode
      setMoveViewActive(false);

      // Show context menu if hovering over a part (Contour tool only)
      if (m_current_tool == JetCamTool::Contour) {
        if (m_mouse_over_part != NULL) {
          m_show_viewer_context_menu = {
            m_app->getRenderer().imgui_io->MousePos.x,
            m_app->getRenderer().imgui_io->MousePos.y
          };
        }
      }
    }
  }
}

void NcCamView::handleKeyEvent(const KeyEvent& e, const InputState& input)
{
  // Handle TAB key switching to NcControlView
  if (e.key == GLFW_KEY_TAB && e.action == GLFW_PRESS) {
    if (m_app) {
      m_app->getControlView().makeActive();
    }
  }

  // Handle modifier key tracking (Ctrl/Shift) - these are already tracked by
  // InputState so we don't need to do anything special here
}

void NcCamView::handleMouseMoveEvent(const MouseMoveEvent& e,
                                      const InputState&     input)
{
  if (!m_app)
    return;

  // Handle view-specific mouse movement (includes panning when move view is
  // active)
  Point2d mouse_pos = { e.x, e.y };
  handleMouseMove(mouse_pos);

  // Check if ImGui wants to capture input
  if (!m_app->getRenderer().imgui_io->WantCaptureKeyboard ||
      !m_app->getRenderer().imgui_io->WantCaptureMouse) {

    // Calculate mouse drag distance
    Point2d mouse_drag = { e.x - m_last_mouse_click_position.x,
                           e.y - m_last_mouse_click_position.y };

    // Handle part dragging in Nesting tool mode
    if (m_current_tool == JetCamTool::Nesting) {
      if (m_left_click_pressed) {
        // Move all selected parts
        for (auto& primitive : m_app->getRenderer().getPrimitiveStack()) {
          auto* part = dynamic_cast<Part*>(primitive.get());
          if (part and part->view == m_app->getRenderer().getCurrentView()) {
            if (part->m_is_part_selected) {
              part->m_control.offset.x +=
                (mouse_drag.x / getZoom()) / part->m_control.scale;
              part->m_control.offset.y +=
                (mouse_drag.y / getZoom()) / part->m_control.scale;
            }
          }
        }
      }
    }
  }

  // Update last mouse position for next drag calculation
  m_last_mouse_click_position = { e.x, e.y };
}
