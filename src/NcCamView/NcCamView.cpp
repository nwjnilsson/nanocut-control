#include "NcCamView.h"
#include "../Input/InputEvents.h"
#include "../Input/InputState.h"
#include "../NcApp/NcApp.h"
#include "DXFParsePathAdaptor/DXFParsePathAdaptor.h"
#include "NcControlView/NcControlView.h"
#include "PolyNest/PolyNest.h"
#include <ImGuiFileDialog.h>
#include <NcControlView/util.h>
#include <NcRender/NcRender.h>
#include <dxflib/dl_dxf.h>
#include <imgui.h>
#include <loguru.hpp>

// JSON serialization for ToolData
void NcCamView::to_json(nlohmann::json& j, const ToolData& tool)
{
  j = nlohmann::json{ { "tool_name", tool.tool_name },
                      { "pierce_height", tool.pierce_height },
                      { "pierce_delay", tool.pierce_delay },
                      { "cut_height", tool.cut_height },
                      { "kerf_width", tool.kerf_width },
                      { "feed_rate", tool.feed_rate },
                      { "thc", tool.thc } };
}

void NcCamView::from_json(const nlohmann::json& j, ToolData& tool)
{
  tool.tool_name = j.at("tool_name").get<std::string>();
  tool.pierce_height = j.at("pierce_height").get<float>();
  tool.pierce_delay = j.at("pierce_delay").get<float>();
  tool.cut_height = j.at("cut_height").get<float>();
  tool.kerf_width = j.at("kerf_width").get<float>();
  tool.feed_rate = j.at("feed_rate").get<float>();
  tool.thc = j.at("thc").get<float>();
}

// Static reference to current instance for callback access

void NcCamView::zoomEventCallback(const ScrollEvent& e, const InputState& input)
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

void NcCamView::mouseEventCallback(Primitive*                       c,
                                   const Primitive::MouseEventData& e)
{
  using EventType = NcRender::EventType;
  if (!m_app)
    return;

  // Only handle hover events (not button clicks)
  if (std::holds_alternative<MouseHoverEvent>(e)) {
    const auto& hover = std::get<MouseHoverEvent>(e);

    if (m_current_tool == JetCamTool::Contour) {
      Part* part = dynamic_cast<Part*>(c);
      if (part) {
        if (hover.event == EventType::MouseIn) {
          size_t global_index =
            (hover.path_index >= 0) ? static_cast<size_t>(hover.path_index) : 0;
          m_mouse_over_part = part;
          m_mouse_over_path = global_index;

          // Find path by global index across all layers
          size_t current_index = 0;
          for (auto& [layer_name, layer] : part->m_layers) {
            for (auto& path : layer.paths) {
              if (current_index == global_index) {
                path.color = getColor(m_mouse_over_color);
                return; // Found it, exit early
              }
              current_index++;
            }
          }
        }
        if (hover.event == EventType::MouseOut) {
          m_mouse_over_part = NULL;
          for (auto& [layer_name, layer] : part->m_layers) {
            for (auto& path : layer.paths) {
              if (path.is_inside_contour == true) {
                if (path.is_closed == true) {
                  path.color = getColor(m_inside_contour_color);
                }
                else {
                  path.color = getColor(m_open_contour_color);
                }
              }
              else {
                path.color = getColor(m_outside_contour_color);
              }
            }
          }
        }
      }
    }

    if (m_current_tool == JetCamTool::Nesting) {
      Part* part = dynamic_cast<Part*>(c);
      if (part) {
        if (hover.event == EventType::MouseIn) {
          part->m_is_part_selected = true;
          for (auto& [layer_name, layer] : part->m_layers) {
            for (auto& path : layer.paths) {
              path.color = getColor(m_mouse_over_color);
            }
          }
        }
        else if (hover.event == EventType::MouseOut) {
          part->m_is_part_selected = false;
          for (auto& [layer_name, layer] : part->m_layers) {
            for (auto& path : layer.paths) {
              if (path.is_inside_contour == true) {
                if (path.is_closed == true) {
                  path.color = getColor(m_inside_contour_color);
                }
                else {
                  path.color = getColor(m_open_contour_color);
                }
              }
              else {
                path.color = getColor(m_outside_contour_color);
              }
            }
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
      m_action_stack.push_back(
        std::make_unique<PostProcessAction>(filePathName));
    }
    ImGuiFileDialog::Instance()->Close();
  }

  // Edit Contour Dialog
  if (show_edit_contour == true) {
    ImGui::Begin(
      "Edit Contour", &show_edit_contour, ImGuiWindowFlags_AlwaysAutoResize);
    Part*              part = dynamic_cast<Part*>(m_edit_contour_part);
    static char        layer_buffer[1024];
    static std::string current_layer_name;

    if (part) {
      // Find the path by global index and get its layer name
      size_t current_index = 0;
      bool   found = false;
      for (auto& [layer_name, layer] : part->m_layers) {
        for (size_t i = 0; i < layer.paths.size(); i++) {
          if (current_index == m_edit_contour_path) {
            current_layer_name = layer_name;
            sprintf(layer_buffer, "%s", layer_name.c_str());
            found = true;
            break;
          }
          current_index++;
        }
        if (found)
          break;
      }

      ImGui::InputText("Layer", layer_buffer, IM_ARRAYSIZE(layer_buffer));

      // If layer name changed, move the path to the new layer
      std::string new_layer_name(layer_buffer);
      if (new_layer_name != current_layer_name && !current_layer_name.empty()) {
        // Find and move the path
        current_index = 0;
        for (auto& [layer_name, layer] : part->m_layers) {
          for (auto it = layer.paths.begin(); it != layer.paths.end(); ++it) {
            if (current_index == m_edit_contour_path) {
              // Move path to new layer
              part->m_layers[new_layer_name].paths.push_back(std::move(*it));
              layer.paths.erase(it);
              current_layer_name = new_layer_name;
              goto done_moving;
            }
            current_index++;
          }
        }
      done_moving:;
      }
    }
    if (ImGui::Button("Ok")) {
      m_action_stack.push_back(std::make_unique<RebuildToolpathsAction>());
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
      bool has_toolpaths =
        std::any_of(m_toolpath_operations.begin(),
                    m_toolpath_operations.end(),
                    [](const auto& op) { return op.enabled; });
      if (ImGui::MenuItem("Save GCode", "", false, has_toolpaths)) {
        LOG_F(INFO, "File->Save GCode");
        std::string path = ".";
        auto&       renderer = m_app->getRenderer();
        std::string p = renderer.fileToString(renderer.getConfigDirectory() +
                                              "last_nc_post_path.conf");
        if (p != "") {
          path = p;
        }
        IGFD::FileDialogConfig config;
        config.path = path;
        ImGuiFileDialog::Instance()->OpenDialog(
          "PostNcDialog", "Choose File", ".nc", config);
      }
      if (ImGui::MenuItem("Send to Controller", "", false, has_toolpaths)) {
        LOG_F(INFO, "File->Send to Controller");
        m_action_stack.push_back(std::make_unique<SendToControllerAction>());
      }
      // ImGui::Separator();
      // if (ImGui::MenuItem("Save Job", "")) {
      // }
      // if (ImGui::MenuItem("Open Job", "")) {
      // }
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
        IGFD::FileDialogConfig config;
        config.path = path;
        ImGuiFileDialog::Instance()->OpenDialog(
          "ImportPartDialog", "Choose File", ".dxf", config);
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
        // Find and delete path by global index
        size_t current_index = 0;
        bool   found = false;
        for (auto& [layer_name, layer] : m_mouse_over_part->m_layers) {
          for (auto it = layer.paths.begin(); it != layer.paths.end(); ++it) {
            if (current_index == m_mouse_over_path) {
              layer.paths.erase(it);
              m_mouse_over_part->m_last_control.angle = 1; // Trigger rebuild
              found = true;
              break;
            }
            current_index++;
          }
          if (found)
            break;
        }
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

    // Count total paths across all layers
    size_t total_paths = 0;
    for (const auto& [layer_name, layer] : selected_part->m_layers) {
      total_paths += layer.paths.size();
    }
    ImGui::Text("Number of Paths: %lu", total_paths);
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
        operation_tool != -1 && operation.lead_in_length == DEFAULT_LEAD_IN &&
        operation.lead_out_length == DEFAULT_LEAD_OUT) {
      operation.lead_in_length =
        m_tool_library[operation_tool].kerf_width * 1.5;
      operation.lead_out_length =
        m_tool_library[operation_tool].kerf_width * 1.5;
    }

    // Tool selection combo
    if (ImGui::BeginCombo("Choose Tool",
                          operation_tool >= 0 &&
                              operation_tool <
                                static_cast<int>(m_tool_library.size())
                            ? m_tool_library[operation_tool].tool_name.c_str()
                            : "Select...")) {
      for (size_t i = 0; i < m_tool_library.size(); i++) {
        bool is_selected = (operation_tool == static_cast<int>(i));
        if (ImGui::Selectable(m_tool_library[i].tool_name.c_str(),
                              is_selected)) {
          operation_tool = i;
        }
        if (is_selected) {
          ImGui::SetItemDefaultFocus();
        }
      }
      ImGui::EndCombo();
    }

    // Layer selection combo
    static int               layer_selection = -1;
    std::vector<std::string> layers = getAllLayers();
    if (ImGui::BeginCombo("Choose Layer",
                          layer_selection >= 0 &&
                              layer_selection < static_cast<int>(layers.size())
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

    // Using a buffer for ImGui::InputText since it needs a mutable char array
    static char tool_name_buffer[1024] = "";
    ImGui::InputText(
      "Tool Name", tool_name_buffer, IM_ARRAYSIZE(tool_name_buffer));

    ImGui::InputFloat("pierce_height", &tool.pierce_height);
    ImGui::InputFloat("pierce_delay", &tool.pierce_delay);
    ImGui::InputFloat("cut_height", &tool.cut_height);
    ImGui::InputFloat("kerf_width", &tool.kerf_width);
    ImGui::InputFloat("feed_rate", &tool.feed_rate);
    ImGui::InputFloat("thc", &tool.thc);

    if (ImGui::Button("OK")) {
      bool skip_save = false;
      // Validate all parameters
      if (tool.pierce_height < 0.f || tool.pierce_height > 50000.f ||
          tool.pierce_delay < 0.f || tool.pierce_delay > 50000.f ||
          tool.cut_height < 0.f || tool.cut_height > 50000.f ||
          tool.kerf_width < 0.f || tool.kerf_width > 50000.f ||
          tool.feed_rate < 0.f || tool.feed_rate > 50000.f || tool.thc < 0.f ||
          tool.thc > 50000.f) {
        LOG_F(WARNING, "Invalid tool input parameters.");
        skip_save = true;
      }

      if (!skip_save) {
        tool.tool_name = std::string(tool_name_buffer);
        m_tool_library.push_back(tool);

        // Save to file using custom serialization
        nlohmann::json tool_library;
        for (size_t x = 0; x < m_tool_library.size(); x++) {
          nlohmann::json tool_json;
          NcCamView::to_json(tool_json, m_tool_library[x]);
          tool_library.push_back(tool_json);
        }

        renderer.dumpJsonToFile(
          renderer.getConfigDirectory() + "tool_library.json", tool_library);
        show_new_tool = false;

        // Reset tool data for next use
        memset(tool_name_buffer, 0, sizeof(tool_name_buffer));
        tool = ToolData(); // Reset to default values
      }
    }
    ImGui::End();
  }

  // Edit Tool Dialog
  static ToolData edit_tool;
  static char     edit_tool_name_buffer[1024] = "";
  if (show_tool_edit != -1) {
    if (edit_tool.tool_name.empty()) {
      edit_tool = m_tool_library[show_tool_edit];
      snprintf(edit_tool_name_buffer,
               sizeof(edit_tool_name_buffer),
               "%s",
               edit_tool.tool_name.c_str());
    }

    ImGui::Begin("Tool Edit", NULL, ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::InputText(
      "Tool Name", edit_tool_name_buffer, IM_ARRAYSIZE(edit_tool_name_buffer));

    ImGui::InputFloat("pierce_height", &edit_tool.pierce_height);
    ImGui::InputFloat("pierce_delay", &edit_tool.pierce_delay);
    ImGui::InputFloat("cut_height", &edit_tool.cut_height);
    ImGui::InputFloat("kerf_width", &edit_tool.kerf_width);
    ImGui::InputFloat("feed_rate", &edit_tool.feed_rate);
    ImGui::InputFloat("thc", &edit_tool.thc);

    if (ImGui::Button("OK")) {
      bool skip_save = false;
      // Validate all parameters
      if (edit_tool.pierce_height < 0.f || edit_tool.pierce_height > 50000.f ||
          edit_tool.pierce_delay < 0.f || edit_tool.pierce_delay > 50000.f ||
          edit_tool.cut_height < 0.f || edit_tool.cut_height > 50000.f ||
          edit_tool.kerf_width < 0.f || edit_tool.kerf_width > 50000.f ||
          edit_tool.feed_rate < 0.f || edit_tool.feed_rate > 50000.f ||
          edit_tool.thc < 0.f || edit_tool.thc > 50000.f) {
        LOG_F(WARNING, "Invalid tool input parameters.");
        skip_save = true;
      }

      if (!skip_save) {
        edit_tool.tool_name = std::string(edit_tool_name_buffer);
        m_tool_library[show_tool_edit] = edit_tool;

        // Save to file using custom serialization
        nlohmann::json tool_library;
        for (size_t x = 0; x < m_tool_library.size(); x++) {
          nlohmann::json tool_json;
          NcCamView::to_json(tool_json, m_tool_library[x]);
          tool_library.push_back(tool_json);
        }

        renderer.dumpJsonToFile(
          renderer.getConfigDirectory() + "tool_library.json", tool_library);
        show_tool_edit = -1;
      }
    }
    ImGui::End();
  }
  else {
    edit_tool = ToolData(); // Reset to default
    memset(edit_tool_name_buffer, 0, sizeof(edit_tool_name_buffer));
  }

  // Tool Library Dialog
  if (show_tool_library == true) {
    ImGui::Begin(
      "Tool Library", &show_tool_library, ImGuiWindowFlags_AlwaysAutoResize);
    if (ImGui::BeginTable(
          "Tools", 8, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
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
        ImGui::Text("%s", m_tool_library[x].tool_name.c_str());
        ImGui::TableSetColumnIndex(1);
        ImGui::Text("%.4f", m_tool_library[x].pierce_height);
        ImGui::TableSetColumnIndex(2);
        ImGui::Text("%.4f", m_tool_library[x].pierce_delay);
        ImGui::TableSetColumnIndex(3);
        ImGui::Text("%.4f", m_tool_library[x].cut_height);
        ImGui::TableSetColumnIndex(4);
        ImGui::Text("%.4f", m_tool_library[x].kerf_width);
        ImGui::TableSetColumnIndex(5);
        ImGui::Text("%.4f", m_tool_library[x].feed_rate);
        ImGui::TableSetColumnIndex(6);
        ImGui::Text("%.4f", m_tool_library[x].thc);
        ImGui::TableSetColumnIndex(7);
        if (ImGui::Button(
              std::string("Edit##Edit-" + std::to_string(x)).c_str())) {
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

  // Action buttons pinned to bottom of pane
  bool has_toolpaths = std::any_of(m_toolpath_operations.begin(),
                                   m_toolpath_operations.end(),
                                   [](const auto& op) { return op.enabled; });

  float button_area_height =
    ImGui::GetFrameHeightWithSpacing() * 3 + ImGui::GetStyle().ItemSpacing.y;
  float target_y = ImGui::GetWindowHeight() - button_area_height -
                   ImGui::GetStyle().WindowPadding.y;
  if (ImGui::GetCursorPosY() < target_y) {
    ImGui::SetCursorPosY(target_y);
  }

  bool has_parts = false;
  forEachPart([&](Part*) { has_parts = true; });
  ImGui::BeginDisabled(!has_parts || m_nesting_in_progress);
  if (ImGui::Button("Arrange", ImVec2(-1, 0))) {
    m_action_stack.push_back(std::make_unique<ArrangePartsAction>());
  }
  ImGui::EndDisabled();
  ImGui::Separator();

  ImGui::BeginDisabled(!has_toolpaths);
  if (ImGui::Button("Save GCode", ImVec2(-1, 0))) {
    std::string path = ".";
    std::string p = renderer.fileToString(renderer.getConfigDirectory() +
                                          "last_nc_post_path.conf");
    if (p != "") {
      path = p;
    }
    IGFD::FileDialogConfig config;
    config.path = path;
    ImGuiFileDialog::Instance()->OpenDialog(
      "PostNcDialog", "Choose File", ".nc", config);
  }
  if (ImGui::Button("Send to Controller", ImVec2(-1, 0))) {
    m_action_stack.push_back(std::make_unique<SendToControllerAction>());
  }
  ImGui::EndDisabled();

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
        IGFD::FileDialogConfig config;
        config.path = path;
        ImGuiFileDialog::Instance()->OpenDialog(
          "ImportPartDialog", "Choose File", ".dxf", config);
      }
      ImGui::Separator();
      if (ImGui::MenuItem("New Duplicate")) {
        m_action_stack.push_back(
          std::make_unique<DuplicatePartAction>(part->m_part_name));
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Delete Master Part")) {
        LOG_F(INFO, "Delete Master Part: %s", part->m_part_name.c_str());
        m_action_stack.push_back(
          std::make_unique<DeletePartsAction>(part->m_part_name));
      }
      ImGui::Separator();
      if (ImGui::MenuItem("Properties")) {
        selected_part = part;
      }
      ImGui::EndPopup();
    }
    if (tree_node_open) {
      // Layer visibility checkboxes
      if (!part->m_layers.empty()) {
        std::vector<std::string> layer_names;
        layer_names.reserve(part->m_layers.size());
        for (const auto& [name, layer] : part->m_layers) {
          layer_names.push_back(name);
        }
        std::sort(layer_names.begin(), layer_names.end());

        ImGui::Text("Layers:");
        for (const auto& layer_name : layer_names) {
          auto& layer = part->m_layers[layer_name];
          bool  visible = layer.visible;
          if (ImGui::Checkbox(
                ("##layer_" + part->m_part_name + "_" + layer_name).c_str(),
                &visible)) {
            layer.visible = visible;
            // Propagate to duplicates (prefix match)
            std::string prefix = part->m_part_name + ":";
            forEachPart([&](Part* dup) {
              if (dup->m_part_name.find(prefix) == 0) {
                auto dup_layer_it = dup->m_layers.find(layer_name);
                if (dup_layer_it != dup->m_layers.end()) {
                  dup_layer_it->second.visible = visible;
                  dup->m_last_control.angle = dup->m_control.angle + 1.0;
                }
              }
            });
            part->m_last_control.angle = part->m_control.angle + 1.0;
            reevaluateContours();
          }
          ImGui::SameLine();
          ImGui::Text("%s", layer_name.c_str());
        }
        ImGui::Separator();
      }

      std::string prefix = part->m_part_name + ":";
      forEachPart([&](Part* dup) {
        if (dup->m_part_name.find(prefix) == 0) {
          ImGui::Checkbox(std::string("##" + dup->m_part_name).c_str(),
                          &dup->visible);
          ImGui::SameLine();
          ImGui::TreeNodeEx(dup->m_part_name.c_str(),
                            ImGuiTreeNodeFlags_Leaf |
                              ImGuiTreeNodeFlags_NoTreePushOnOpen);
          if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Delete")) {
              m_action_stack.push_back(
                std::make_unique<DeletePartAction>(dup->m_part_name));
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Properties")) {
              selected_part = dup;
            }
            ImGui::EndPopup();
          }
        }
      });
      ImGui::TreePop();
    }
  });

  ImGui::EndTable();
}

void NcCamView::renderOperationsViewer(bool& show_create_operation,
                                       int&  show_edit_tool_operation)
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
    if (ImGui::Button(
          std::string("Edit##edit-op-" + std::to_string(x)).c_str())) {
      show_edit_tool_operation = x;
    }
    ImGui::SameLine();
    if (ImGui::Button(
          std::string("Delete##del-op-" + std::to_string(x)).c_str())) {
      m_action_stack.push_back(std::make_unique<DeleteOperationAction>(x));
    }
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
    for (const auto& [layer_name, layer] : part->m_layers) {
      if (std::find(layers.begin(), layers.end(), layer_name) == layers.end()) {
        layers.push_back(layer_name);
      }
    }
  });
  return layers;
}

void NcCamView::resetNesting()
{
  m_dxf_nest = PolyNest::PolyNest();
  m_dxf_nest.setExtents(
    {m_material_plane->m_bottom_left.x, m_material_plane->m_bottom_left.y},
    {m_material_plane->m_bottom_left.x + m_material_plane->m_width,
     m_material_plane->m_bottom_left.y + m_material_plane->m_height});
}

std::vector<std::vector<PolyNest::PolyPoint>>
NcCamView::collectOutsideContours(Part* part)
{
  std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
  for (const auto& [layer_name, layer] : part->m_layers) {
    for (const auto& path : layer.paths) {
      if (!path.is_inside_contour) {
        std::vector<PolyNest::PolyPoint> points;
        points.reserve(path.points.size());
        for (const auto& p : path.points) {
          points.push_back({p.x, p.y});
        }
        poly_part.push_back(std::move(points));
      }
    }
  }
  return poly_part;
}

void NcCamView::reevaluateContours()
{
  // Collect all paths from all parts with their layer information
  struct PathRef {
    Part*        part;
    std::string  layer_name;
    size_t       path_index;
    bool         is_closed;
    geo::Extents bbox;
  };

  std::vector<PathRef> all_path_refs;
  forEachPart([&](Part* part) {
    for (auto& [layer_name, layer] : part->m_layers) {
      for (size_t i = 0; i < layer.paths.size(); i++) {
        PathRef ref;
        ref.part = part;
        ref.layer_name = layer_name;
        ref.path_index = i;
        ref.is_closed = layer.paths[i].is_closed;
        ref.bbox = geo::calculateBoundingBox(layer.paths[i].points);

        all_path_refs.push_back(ref);
      }
    }
  });

  // Reset all paths to outside contour
  for (auto& ref : all_path_refs) {
    Part::path_t& path =
      ref.part->m_layers[ref.layer_name].paths[ref.path_index];
    path.is_inside_contour = false;
    path.color = getColor(m_outside_contour_color);
  }

  // Re-evaluate inside/outside relationships considering only visible layers
  for (size_t x = 0; x < all_path_refs.size(); x++) {
    Part::path_t& path_x = all_path_refs[x]
                             .part->m_layers[all_path_refs[x].layer_name]
                             .paths[all_path_refs[x].path_index];

    for (size_t i = 0; i < all_path_refs.size(); i++) {
      if (i == x or all_path_refs[i].part != all_path_refs[x].part)
        continue;

      // Skip non-closed paths - they can't contain other paths
      if (!all_path_refs[i].is_closed)
        continue;

      // Check if layer i is visible
      auto layer_it =
        all_path_refs[i].part->m_layers.find(all_path_refs[i].layer_name);
      bool is_visible = (layer_it != all_path_refs[i].part->m_layers.end()) &&
                        layer_it->second.visible;

      if (!is_visible)
        continue; // Skip invisible layers

      // Bounding box pre-check: if path_x's bbox isn't inside path_i's bbox,
      // then path_x can't be inside path_i
      if (!geo::extentsContain(all_path_refs[i].bbox, all_path_refs[x].bbox)) {
        continue;
      }

      Part::path_t& path_i = all_path_refs[i]
                               .part->m_layers[all_path_refs[i].layer_name]
                               .paths[all_path_refs[i].path_index];

      // Now do the expensive check
      if (all_path_refs[x].part->checkIfPathIsInsidePath(path_x.points,
                                                         path_i.points)) {
        path_x.is_inside_contour = true;
        if (all_path_refs[x].is_closed) {
          path_x.color = getColor(m_inside_contour_color);
        }
        else {
          path_x.color = getColor(m_open_contour_color);
        }
        break;
      }
    }
  }
}

// ============================================================================
// Action Handler Methods
// ============================================================================

// Generate G-code lines from current toolpath operations
std::vector<std::string> NcCamView::generateGCode()
{
  std::vector<std::string> lines;

  for (size_t i = 0; i < m_toolpath_operations.size(); i++) {
    LOG_F(INFO,
          "Generating toolpath operation: %lu on layer: %s",
          i,
          m_toolpath_operations[i].layer.c_str());

    forEachVisiblePart([&](Part* part) {
      std::vector<std::vector<Point2d>> tool_paths =
        part->getOrderedToolpaths();

      if (m_tool_library.size() > m_toolpath_operations[i].tool_number) {
        const auto& tool = m_tool_library[m_toolpath_operations[i].tool_number];

        if (tool.thc > 0) {
          lines.push_back("$T=" + std::to_string(tool.thc));
        }

        for (size_t x = 0; x < tool_paths.size(); x++) {
          lines.push_back("G0 X" + std::to_string(-tool_paths[x][0].x) + " Y" +
                          std::to_string(-tool_paths[x][0].y));
          lines.push_back("fire_torch " + std::to_string(tool.pierce_height) +
                          " " + std::to_string(tool.pierce_delay) + " " +
                          std::to_string(tool.cut_height));
          for (size_t z = 0; z < tool_paths[x].size(); z++) {
            lines.push_back("G1 X" + std::to_string(-tool_paths[x][z].x) +
                            " Y" + std::to_string(-tool_paths[x][z].y) + " F" +
                            std::to_string(tool.feed_rate));
          }
          lines.push_back("torch_off");
        }
      }
    });
  }

  lines.push_back("M30");
  return lines;
}

// PostProcessAction implementation
void NcCamView::PostProcessAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;

  std::ofstream gcode_file(m_file);
  if (!gcode_file.is_open()) {
    LOG_F(WARNING, "Could not open gcode file for writing!");
    return;
  }

  for (const auto& line : view->generateGCode()) {
    gcode_file << line << "\n";
  }

  gcode_file.close();
  LOG_F(INFO, "Finished writing gcode file!");
}

// SendToControllerAction implementation
void NcCamView::SendToControllerAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;

  view->m_app->getControlView().loadGCodeFromLines(view->generateGCode());
  LOG_F(INFO, "Sent G-code to controller!");
}

// RebuildToolpathsAction implementation
void NcCamView::RebuildToolpathsAction::execute(NcCamView* view)
{
  view->forEachPart([](Part* part) { part->m_last_control.angle = 1; });

  for (size_t x = 0; x < view->m_toolpath_operations.size(); x++) {
    view->m_toolpath_operations[x].last_enabled = false;
  }
}

// DeleteOperationAction implementation
void NcCamView::DeleteOperationAction::execute(NcCamView* view)
{
  if (view->m_toolpath_operations.size() > m_index) {
    view->m_toolpath_operations.erase(view->m_toolpath_operations.begin() +
                                      m_index);
  }
}

// DeleteDuplicatePartsAction implementation
void NcCamView::DeleteDuplicatePartsAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;
  auto& renderer = view->m_app->getRenderer();

  auto& stack = renderer.getPrimitiveStack();
  stack.erase(
    std::remove_if(stack.begin(),
                   stack.end(),
                   [&](const auto& primitive) {
                     auto* part = dynamic_cast<Part*>(primitive.get());
                     return part && part->view == renderer.getCurrentView() &&
                            part->m_part_name.find(m_part_name + ":") !=
                              std::string::npos;
                   }),
    stack.end());
}

// DeletePartsAction implementation
void NcCamView::DeletePartsAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;
  auto& renderer = view->m_app->getRenderer();

  auto& stack = renderer.getPrimitiveStack();
  stack.erase(std::remove_if(stack.begin(),
                             stack.end(),
                             [&](const auto& primitive) {
                               auto* part =
                                 dynamic_cast<Part*>(primitive.get());
                               return part &&
                                      part->view == renderer.getCurrentView() &&
                                      part->m_part_name.find(m_part_name) !=
                                        std::string::npos;
                             }),
              stack.end());
}

// DeletePartAction implementation
void NcCamView::DeletePartAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;
  auto& renderer = view->m_app->getRenderer();

  auto& stack = renderer.getPrimitiveStack();
  for (auto it = stack.begin(); it != stack.end(); ++it) {
    auto* part = dynamic_cast<Part*>(it->get());
    if (part && part->view == renderer.getCurrentView() &&
        part->m_part_name.find(m_part_name) != std::string::npos) {
      stack.erase(it);
      break;
    }
  }
}

void NcCamView::DuplicatePartAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;
  auto& renderer = view->m_app->getRenderer();

  // Count duplicates
  int dup_count = 1;
  view->forEachPart([&](Part* part) {
    if (part->m_part_name.find(m_part_name + ":") != std::string::npos) {
      dup_count++;
    }
  });

  // Setup nesting
  view->resetNesting();

  // Add existing parts to nesting
  view->forEachPart([&](Part* part) {
    auto poly_part = view->collectOutsideContours(part);
    view->m_dxf_nest.pushPlacedPolyPart(poly_part,
                                        &part->m_control.offset.x,
                                        &part->m_control.offset.y,
                                        &part->m_control.angle,
                                        &part->visible);
  });

  // Find master part first (read-only, no mutation of primitive stack)
  Part* master_part = view->findPartByName(m_part_name);
  if (!master_part)
    return;

  // Collect data from master part before modifying the primitive stack
  std::unordered_map<std::string, Part::Layer> layers_copy;
  for (const auto& [layer_name, layer] : master_part->m_layers) {
    layers_copy[layer_name] = layer;
  }
  auto poly_part = view->collectOutsideContours(master_part);

  // Create duplicate outside any iteration (avoids iterator invalidation)
  Part* new_part = renderer.pushPrimitive<Part>(
    m_part_name + ":" + std::to_string(dup_count), std::move(layers_copy));

  // Set initial position
  if (dup_count > 1) {
    Part* prev_dup =
      view->findPartByName(m_part_name + ":" + std::to_string(dup_count - 1));
    if (prev_dup) {
      new_part->m_control.offset = prev_dup->m_control.offset;
    }
  }
  else {
    new_part->m_control.offset = master_part->m_control.offset;
  }

  new_part->m_control.scale = master_part->m_control.scale;
  new_part->mouse_callback = master_part->mouse_callback;
  new_part->matrix_callback = master_part->matrix_callback;
  new_part->visible = false; // Hidden until nesting places it

  view->m_dxf_nest.pushUnplacedPolyPart(poly_part,
                                        &new_part->m_control.offset.x,
                                        &new_part->m_control.offset.y,
                                        &new_part->m_control.angle,
                                        &new_part->visible);

  view->m_dxf_nest.beginPlaceUnplacedPolyParts();
  view->startNestingThread();
}

void NcCamView::ArrangePartsAction::execute(NcCamView* view)
{
  if (!view->m_app)
    return;

  view->resetNesting();

  // Push all parts as unplaced, resetting their positions
  view->forEachPart([&](Part* part) {
    part->m_control.offset = { 0, 0 };
    part->m_control.angle = 0;
    part->visible = false;

    auto poly_part = view->collectOutsideContours(part);
    view->m_dxf_nest.pushUnplacedPolyPart(poly_part,
                                          &part->m_control.offset.x,
                                          &part->m_control.offset.y,
                                          &part->m_control.angle,
                                          &part->visible);
  });

  view->m_dxf_nest.beginPlaceUnplacedPolyParts();
  view->startNestingThread();
}

void NcCamView::renderProgressWindow()
{
  if (!m_nesting_in_progress)
    return;

  ImGui::Begin("Nesting", nullptr, ImGuiWindowFlags_AlwaysAutoResize);
  float progress = m_nesting_progress.load();
  ImGui::ProgressBar(progress, ImVec2(200.f, 0.0f));
  ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
  ImGui::Text("Placing parts...");
  ImGui::End();
}

void NcCamView::startNestingThread()
{
  if (m_nesting_thread && m_nesting_thread->joinable()) {
    m_nesting_thread->join();
  }

  m_nesting_in_progress = true;
  m_nesting_progress = 0.0f;

  m_nesting_thread = std::make_unique<std::thread>([this]() {
    m_dxf_nest.placeAllUnplacedParts(&m_nesting_progress);
    m_nesting_in_progress = false;
  });
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
    resetNesting();
    forEachPart([&](Part* part) {
      auto poly_part = collectOutsideContours(part);
      m_dxf_nest.pushPlacedPolyPart(poly_part,
                                    &part->m_control.offset.x,
                                    &part->m_control.offset.y,
                                    &part->m_control.angle,
                                    &part->visible);
    });
    m_dl_dxf = std::make_unique<DL_Dxf>();
    m_dxf_creation_interface = std::make_unique<DXFParsePathAdaptor>(
      &renderer,
      getTransformCallback(),
      [this](Primitive* c, const Primitive::MouseEventData& e) {
        mouseEventCallback(c, e);
      },
      this);
    m_dxf_creation_interface->setFilename(name);
    m_dxf_creation_interface->setImportScale(import_scale);
    m_dxf_creation_interface->setImportQuality(import_quality);
    parseDxfFile();
    LOG_F(INFO, "Parsing DXF File: %s", filename.c_str());
    return true;
  }
  return false;
}

void NcCamView::parseDxfFile()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();

  while (
    m_dl_dxf->readDxfGroups(m_dxf_fp.get(), m_dxf_creation_interface.get())) {
  }
  LOG_F(INFO, "Successfully parsed DXF groups.");

  m_dxf_creation_interface->finish();

  // Set frozen DXF layers to invisible on the Part
  for (auto& pPrimitive : renderer.getPrimitiveStack()) {
    auto* part = dynamic_cast<Part*>(pPrimitive.get());
    if (part && pPrimitive->view == renderer.getCurrentView() &&
        part->m_part_name == m_dxf_creation_interface->m_filename) {
      for (const auto& [layer_name, layer_props] :
           m_dxf_creation_interface->m_layer_props) {
        bool is_frozen = (layer_props.flags & 0x01) != 0;
        if (is_frozen) {
          auto layer_it = part->m_layers.find(layer_name);
          if (layer_it != part->m_layers.end()) {
            layer_it->second.visible = false;
          }
        }
      }
    }
  }

  for (auto& pPrimitive : renderer.getPrimitiveStack()) {
    auto* part = dynamic_cast<Part*>(pPrimitive.get());
    if (part and pPrimitive->view == renderer.getCurrentView() and
        part->m_part_name == m_dxf_creation_interface->m_filename) {
      part->visible = false; // Hidden until nesting places it
      auto poly_part = collectOutsideContours(part);
      m_dxf_nest.pushUnplacedPolyPart(poly_part,
                                      &part->m_control.offset.x,
                                      &part->m_control.offset.y,
                                      &part->m_control.angle,
                                      &part->visible);
    }
  }
  m_dxf_nest.beginPlaceUnplacedPolyParts();
  startNestingThread();

  // Smart pointers automatically handle cleanup
  m_dxf_fp.reset();
  m_dxf_creation_interface.reset();
  m_dl_dxf.reset();
}
void NcCamView::preInit()
{
  if (!m_app)
    return;
  auto& renderer = m_app->getRenderer();
  m_mouse_over_color = Color::Blue;
  m_outside_contour_color = Color::White;
  m_inside_contour_color = Color::Grey;
  m_open_contour_color = Color::Yellow;

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
      ToolData tool;
      NcCamView::from_json(tool_library[x], tool);
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
  m_progress_window_handle =
    m_app->getRenderer().pushGui(true, [this]() { renderProgressWindow(); });
  m_material_plane =
    m_app->getRenderer().pushPrimitive<Box>(Point2d{ 0, 0 },
                                            m_job_options.material_size[0],
                                            m_job_options.material_size[1],
                                            0);
  m_material_plane->id = "material_plane";
  m_material_plane->flags = PrimitiveFlags::MaterialPlane;
  m_material_plane->zindex = -20;
  m_material_plane->color.r = 100;
  m_material_plane->color.g = 0;
  m_material_plane->color.b = 0;
  m_material_plane->matrix_callback = getTransformCallback();
  m_material_plane->mouse_callback =
    [this](Primitive* c, const Primitive::MouseEventData& e) {
      mouseEventCallback(c, e);
    };
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
      const std::string& operation_layer = m_toolpath_operations[x].layer;

      forEachVisiblePart([&](Part* part) {
        // Direct layer access - no filtering needed!
        auto layer_it = part->m_layers.find(operation_layer);
        if (layer_it != part->m_layers.end()) {
          Part::Layer& layer = layer_it->second;

          // Set layer-level properties
          if (m_tool_library.size() > m_toolpath_operations[x].tool_number) {
            layer.toolpath_offset =
              m_tool_library[m_toolpath_operations[x].tool_number].kerf_width;
          }
          layer.toolpath_visible = m_toolpath_operations[x].enabled;

          part->m_control.lead_in_length =
            m_toolpath_operations[x].lead_in_length;
          part->m_control.lead_out_length =
            m_toolpath_operations[x].lead_out_length;

          // Force rebuild of toolpaths by invalidating last_control
          part->m_last_control.angle = part->m_control.angle + 1.0;
        }
      });
    }
    m_toolpath_operations[x].last_enabled = m_toolpath_operations[x].enabled;
  }
  // Process action stack
  // Process all pending actions using virtual dispatch
  while (!m_action_stack.empty()) {
    try {
      auto action = std::move(m_action_stack.front());
      m_action_stack.erase(m_action_stack.begin());
      action->execute(this);
    }
    catch (const std::exception& e) {
      LOG_F(ERROR,
            "(NcCamView::tick) Exception during action execution: %s",
            e.what());
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
void NcCamView::close()
{
  if (m_nesting_thread && m_nesting_thread->joinable()) {
    m_nesting_thread->join();
  }
}

void NcCamView::handleScrollEvent(const ScrollEvent& e, const InputState& input)
{
  zoomEventCallback(e, input);
}

void NcCamView::handleMouseEvent(const MouseButtonEvent& e,
                                 const InputState&       input)
{
  if (!m_app)
    return;

  // Check if ImGui wants to capture input
  auto& io = ImGui::GetIO();
  if (io.WantCaptureMouse) {
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
          m_show_viewer_context_menu = { io.MousePos.x, io.MousePos.y };
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
  auto& io = ImGui::GetIO();
  if (!io.WantCaptureKeyboard || !io.WantCaptureMouse) {

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
