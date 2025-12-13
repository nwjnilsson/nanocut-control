#include "jetCamView.h"
#include "DXFParsePathAdaptor/DXFParsePathAdaptor.h"
#include "PolyNest/PolyNest.h"
#include "ncControlView/ncControlView.h"
#include <EasyRender/EasyRender.h>
#include <EasyRender/gui/ImGuiFileDialog.h>
#include <EasyRender/gui/imgui.h>
#include <EasyRender/logging/loguru.h>
#include <dxf/dxflib/dl_dxf.h>
#include <ncControlView/util.h>

void jetCamView::ZoomEventCallback(const nlohmann::json& e)
{
  if (globals->jet_cam_view->CurrentTool == JetCamTool::Nesting) {
    for (std::vector<PrimitiveContainer*>::iterator it =
           globals->renderer->GetPrimitiveStack()->begin();
         it != globals->renderer->GetPrimitiveStack()->end();
         ++it) {
      if ((*it)->properties->view == globals->renderer->GetCurrentView()) {
        if ((*it)->type == "part" and (*it)->part->is_part_selected) {
          if ((float) e["scroll"] > 0) {
            if (globals->jet_cam_view->mods & GLFW_MOD_CONTROL) {
              (*it)->part->control.angle += 5;
            }
            if (globals->jet_cam_view->mods & GLFW_MOD_SHIFT) {
              (*it)->part->control.scale += .1f;
            }
          }
          else {
            if (globals->jet_cam_view->mods & GLFW_MOD_CONTROL) {
              (*it)->part->control.angle -= 5;
            }
            if (globals->jet_cam_view->mods & GLFW_MOD_SHIFT) {
              (*it)->part->control.scale -= .1f;
            }
          }
        }
      }
    }
  }
  if (globals->jet_cam_view->mods & (GLFW_MOD_SHIFT | GLFW_MOD_CONTROL)) {
    return;
  }
  // Zoom
  double_point_t matrix_mouse = globals->renderer->GetWindowMousePosition();
  matrix_mouse.x = (matrix_mouse.x - globals->pan.x) / globals->zoom;
  matrix_mouse.y = (matrix_mouse.y - globals->pan.y) / globals->zoom;
  if ((float) e["scroll"] > 0) {
    double old_zoom = globals->zoom;
    globals->zoom += globals->zoom * 0.125;
    if (globals->zoom > 1000000) {
      globals->zoom = 1000000;
    }
    double scalechange = old_zoom - globals->zoom;
    globals->pan.x += matrix_mouse.x * scalechange;
    globals->pan.y += matrix_mouse.y * scalechange;
  }
  else {
    double old_zoom = globals->zoom;
    globals->zoom += globals->zoom * -0.125;
    if (globals->zoom < 0.00001) {
      globals->zoom = 0.00001;
    }
    double scalechange = old_zoom - globals->zoom;
    globals->pan.x += matrix_mouse.x * scalechange;
    globals->pan.y += matrix_mouse.y * scalechange;
  }
}

void jetCamView::ViewMatrixCallback(PrimitiveContainer* p)
{
  p->properties->scale = globals->zoom;
  p->properties->offset[0] =
    globals->pan.x +
    globals->jet_cam_view->material_plane->width * globals->zoom;
  p->properties->offset[1] =
    globals->pan.y +
    globals->jet_cam_view->material_plane->height * globals->zoom;
}

void jetCamView::MouseCallback(const nlohmann::json& e)
{
  // LOG_F(INFO, "%s", e.dump().c_str());
  using EventType = EasyRender::EventType;
  if (!globals->renderer->imgui_io->WantCaptureKeyboard ||
      !globals->renderer->imgui_io->WantCaptureMouse) {
    if (e.contains("event")) {
      // Process event
      EventType event = static_cast<EventType>(e["event"]);
      if (event == EventType::MouseMove) {
        double_point_t mouse_drag = {
          (double) e["pos"]["x"] -
            globals->jet_cam_view->last_mouse_click_position.x,
          (double) e["pos"]["y"] -
            globals->jet_cam_view->last_mouse_click_position.y
        };
        if (globals->jet_cam_view->CurrentTool == JetCamTool::Nesting) {
          if (globals->jet_cam_view->left_click_pressed == true) {
            // LOG_F(INFO, "Dragging mouse!");
            for (std::vector<PrimitiveContainer*>::iterator it =
                   globals->renderer->GetPrimitiveStack()->begin();
                 it != globals->renderer->GetPrimitiveStack()->end();
                 ++it) {
              if ((*it)->properties->view ==
                  globals->renderer->GetCurrentView()) {
                if ((*it)->type == "part") {
                  if ((*it)->part->is_part_selected == true) {
                    // LOG_F(INFO, "Moving (%.4f, %.4f)", mouse_drag.x,
                    // mouse_drag.y);
                    (*it)->part->control.offset.x +=
                      (mouse_drag.x / globals->zoom) /
                      (*it)->part->control.scale;
                    (*it)->part->control.offset.y +=
                      (mouse_drag.y / globals->zoom) /
                      (*it)->part->control.scale;
                  }
                }
              }
            }
          }
        }
        globals->jet_cam_view->last_mouse_click_position = {
          (double) e["pos"]["x"], (double) e["pos"]["y"]
        };
      }
    }
    else {
      // Process buttons
      if ((int) e["button"] == GLFW_MOUSE_BUTTON_1) {
        if ((int) e["action"] & EasyRender::ActionFlagBits::Release) {
          globals->jet_cam_view->left_click_pressed = false;
        }
        if ((int) e["action"] & EasyRender::ActionFlagBits::Press) {
          globals->jet_cam_view->left_click_pressed = true;
          globals->jet_cam_view->show_viewer_context_menu = { -1000000,
                                                              -1000000 };
        }
      }
      else if ((int) e["button"] == GLFW_MOUSE_BUTTON_2) {
        if ((int) e["action"] & EasyRender::ActionFlagBits::Release) {
          if (globals->jet_cam_view->CurrentTool == JetCamTool::Contour) {
            if (globals->jet_cam_view->mouse_over_part != NULL)
              globals->jet_cam_view->show_viewer_context_menu = {
                globals->renderer->imgui_io->MousePos.x,
                globals->renderer->imgui_io->MousePos.y
              };
          }
        }
      }
    }
  }
  else {
    globals->jet_cam_view->left_click_pressed = false;
  }
}

void jetCamView::MouseEventCallback(PrimitiveContainer*   c,
                                    const nlohmann::json& e)
{
  // LOG_F(INFO, "%s", e.dump().c_str());
  using EventType = EasyRender::EventType;
  EasyRender::EventType event{};
  if (e.contains("event")) {
    event = e.at("event").get<EventType>();
    if (globals->jet_cam_view->CurrentTool == JetCamTool::Contour) {
      if (c->type == "part") {
        if (event == EventType::MouseIn) {
          size_t x = (size_t) e["path_index"];
          globals->renderer->SetColorByName(
            c->part->paths[x].color, globals->jet_cam_view->mouse_over_color);
          globals->jet_cam_view->mouse_over_part = c;
          globals->jet_cam_view->mouse_over_path = x;
        }
        if (event == EventType::MouseOut) {
          globals->jet_cam_view->mouse_over_part = NULL;
          for (size_t x = 0; x < c->part->paths.size(); x++) {
            if (c->part->paths[x].is_inside_contour == true) {
              if (c->part->paths[x].is_closed == true) {
                globals->renderer->SetColorByName(
                  c->part->paths[x].color,
                  globals->jet_cam_view->inside_contour_color);
              }
              else {
                globals->renderer->SetColorByName(
                  c->part->paths[x].color,
                  globals->jet_cam_view->open_contour_color);
              }
            }
            else {
              globals->renderer->SetColorByName(
                c->part->paths[x].color,
                globals->jet_cam_view->outside_contour_color);
            }
          }
        }
      }
    }
  }
  if (globals->jet_cam_view->CurrentTool == JetCamTool::Nesting) {
    if (c->type == "part" &&
        globals->jet_cam_view->left_click_pressed == false) {
      if (event == EventType::MouseIn) {
        c->part->is_part_selected = true;
        for (size_t x = 0; x < c->part->paths.size(); x++) {
          globals->renderer->SetColorByName(
            c->part->paths[x].color, globals->jet_cam_view->mouse_over_color);
        }
      }
      else if (event == EventType::MouseOut) {
        for (size_t x = 0; x < c->part->paths.size(); x++) {
          if (c->part->paths[x].is_inside_contour == true) {
            globals->renderer->SetColorByName(
              c->part->paths[x].color,
              globals->jet_cam_view->inside_contour_color);
          }
          else {
            globals->renderer->SetColorByName(
              c->part->paths[x].color,
              globals->jet_cam_view->outside_contour_color);
          }
        }
        c->part->is_part_selected = false;
      }
    }
  }
}

void jetCamView::TabKeyCallback(const nlohmann::json& e)
{
  // LOG_F(INFO, "%s", e.dump().c_str());
  globals->nc_control_view->MakeActive();
}

void jetCamView::ModKeyCallback(const nlohmann::json& e)
{
  LOG_F(INFO, "%s", e.dump().c_str());
  using Action = EasyRender::ActionFlagBits;
  auto& mods = globals->jet_cam_view->mods;
  auto  set_mod = [&](int bit) {
    if (e.at("action").get<int>() & Action::Press) {
      mods |= bit;
    }
    else {
      mods &= ~bit;
    }
  };
  if (e.at("key").get<int>() == GLFW_KEY_LEFT_CONTROL) {
    set_mod(GLFW_MOD_CONTROL);
  }
  if (e.at("key").get<int>() == GLFW_KEY_LEFT_SHIFT) {
    set_mod(GLFW_MOD_SHIFT);
  }
}

void jetCamView::RenderUI(void* self_pointer)
{
  static bool show_create_operation = false;
  static bool show_job_options = false;
  static bool show_tool_library = false;
  static bool show_new_tool = false;
  static int  show_tool_edit = -1;
  static int  show_edit_tool_operation = -1;
  static bool show_edit_contour = false;
  static bool show_dxf_importer = false;

  jetCamView* self = reinterpret_cast<jetCamView*>(self_pointer);
  if (self != NULL) {
    static std::string filePathName;
    static std::string fileName;
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
        globals->renderer->StringToFile(
          globals->renderer->GetConfigDirectory() + "last_dxf_open_path.conf",
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
        self->DxfFileOpen(filePathName, fileName, import_quality, import_scale);
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
        std::string filePathName =
          ImGuiFileDialog::Instance()->GetFilePathName();
        std::string filePath = ImGuiFileDialog::Instance()->GetCurrentPath();
        std::string fileName =
          ImGuiFileDialog::Instance()->GetCurrentFileName();
        LOG_F(INFO,
              "File Path: %s, File Path Name: %s, File Name: %s",
              filePath.c_str(),
              filePathName.c_str(),
              fileName.c_str());
        globals->renderer->StringToFile(
          globals->renderer->GetConfigDirectory() + "last_nc_post_path.conf",
          filePath + "/");
        action_t action;
        action.action_id = "post_process";
        action.data["file"] = filePathName;
        self->action_stack.push_back(action);
      }
      ImGuiFileDialog::Instance()->Close();
    }
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Post Process", "")) {
          LOG_F(INFO, "File->Post Process");
          std::string path = ".";
          std::string p = globals->renderer->FileToString(
            globals->renderer->GetConfigDirectory() + "last_nc_post_path.conf");
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
          std::string p = globals->renderer->FileToString(
            globals->renderer->GetConfigDirectory() +
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
          globals->quit = true;
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
          globals->nc_control_view->MakeActive();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("CAM Toolpaths", "")) {
          LOG_F(INFO, "Workbench->CAM Toolpaths");
          globals->jet_cam_view->MakeActive();
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Help")) {
        if (ImGui::MenuItem("Dump Stack", "")) {
          LOG_F(INFO, "Help->Dump Stack");
          globals->renderer->DumpJsonToFile(
            "stack.json", globals->renderer->DumpPrimitiveStack());
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
      ImGui::RadioButton("Contour Tool", (int*) &self->CurrentTool, 0);
      ImGui::SameLine();
      ImGui::RadioButton("Nesting Tool", (int*) &self->CurrentTool, 1);
      ImGui::SameLine();
      ImGui::RadioButton("Point Tool", (int*) &self->CurrentTool, 2);
      ImGui::EndMainMenuBar();
    }
    if (show_edit_contour == true) {
      // ImGui::SetNextWindowPos(ImVec2(self->show_viewer_context_menu.x,
      // self->show_viewer_context_menu.y));
      ImGui::Begin(
        "Edit Contour", &show_edit_contour, ImGuiWindowFlags_AlwaysAutoResize);
      char layer[1024];
      sprintf(layer,
              "%s",
              self->edit_contour_part->part->paths[self->edit_contour_path]
                .layer.c_str());
      ImGui::InputText("Layer", layer, IM_ARRAYSIZE(layer));
      self->edit_contour_part->part->paths[self->edit_contour_path].layer =
        std::string(layer);
      if (ImGui::Button("Ok")) {
        action_t action;
        action.action_id = "rebuild_part_toolpaths";
        self->action_stack.push_back(action);
        show_edit_contour = false;
      }
      ImGui::End();
    }
    if (self->show_viewer_context_menu.x != -1000000 &&
        self->show_viewer_context_menu.y != -1000000) {
      ImGui::SetNextWindowPos(ImVec2(self->show_viewer_context_menu.x,
                                     self->show_viewer_context_menu.y));
      ImGui::Begin("View Contect Menu",
                   NULL,
                   ImGuiWindowFlags_AlwaysAutoResize |
                     ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize);
      size_t selected_part = 0;
      size_t selected_contour = 0;
      if (ImGui::Button("Edit")) {
        show_edit_contour = true;
        self->edit_contour_part = self->mouse_over_part;
        self->edit_contour_path = self->mouse_over_path;
        self->show_viewer_context_menu = { -1000000, -1000000 };
      }
      if (ImGui::Button("Delete")) {
        self->mouse_over_part->part->paths.erase(
          self->mouse_over_part->part->paths.begin() + self->mouse_over_path);
        self->mouse_over_part->part->last_control.angle = 1; // Trigger rebuild
        self->show_viewer_context_menu = { -1000000, -1000000 };
      }
      ImGui::End();
    }
    if (show_job_options == true) {
      ImGui::Begin(
        "Job Options", &show_job_options, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Text("Material Size");
      ImGui::InputFloat("Width", &self->job_options.material_size[0]);
      ImGui::SameLine();
      ImGui::InputFloat("Height", &self->job_options.material_size[1]);
      ImGui::Text("Origin Corner");
      ImGui::RadioButton("Top Left", &self->job_options.origin_corner, 0);
      ImGui::SameLine();
      ImGui::RadioButton("Top Right", &self->job_options.origin_corner, 1);
      ImGui::RadioButton("Bottom Left", &self->job_options.origin_corner, 2);
      ImGui::SameLine();
      ImGui::RadioButton("Bottom Right", &self->job_options.origin_corner, 3);
      switch (self->job_options.origin_corner) {
        case 0:
          self->material_plane->bottom_left = {
            0, -self->job_options.material_size[1]
          };
          self->material_plane->width = self->job_options.material_size[0];
          self->material_plane->height = self->job_options.material_size[1];
          break;
        case 1:
          self->material_plane->bottom_left = {
            -self->job_options.material_size[0],
            -self->job_options.material_size[1]
          };
          self->material_plane->width = self->job_options.material_size[0];
          self->material_plane->height = self->job_options.material_size[1];
          break;
        case 2:
          self->material_plane->bottom_left = { 0, 0 };
          self->material_plane->width = self->job_options.material_size[0];
          self->material_plane->height = self->job_options.material_size[1];
          break;
        case 3:
          self->material_plane->bottom_left = {
            -self->job_options.material_size[0], 0
          };
          self->material_plane->width = self->job_options.material_size[0];
          self->material_plane->height = self->job_options.material_size[1];
          break;
        default:
          break;
      }
      if (ImGui::Button("Close")) {
        show_job_options = false;
      }
      ImGui::End();
    }
    if (show_create_operation == true) {
      ImGui::Begin("Create Operation",
                   &show_create_operation,
                   ImGuiWindowFlags_AlwaysAutoResize);
      static toolpath_operation_t operation;
      static int                  operation_tool = -1;
      if (self->tool_library.size() > operation_tool && operation_tool != -1 &&
          operation.lead_in_length == DEFAULT_LEAD_OUT &&
          operation.lead_out_length == DEFAULT_LEAD_OUT) {
        operation.lead_in_length =
          self->tool_library[operation_tool].params["kerf_width"] * 1.5;
        operation.lead_out_length =
          self->tool_library[operation_tool].params["kerf_width"] * 1.5;
      }
      std::vector<std::string> combo_options;
      for (size_t x = 0; x < self->tool_library.size(); x++)
        combo_options.push_back(self->tool_library[x].tool_name);
      static char* cp = NULL;
      if (cp == NULL) {
        int cp_i = 0;
        for (size_t x = 0; x < combo_options.size(); x++) {
          for (size_t i = 0; i < combo_options[x].size(); i++)
            cp_i++;
          cp_i++;
        }
        cp_i++;
        cp = (char*) malloc((cp_i + 1) * sizeof(char));
        if (cp != NULL) {
          int cp_i = 0;
          for (size_t x = 0; x < combo_options.size(); x++) {
            for (size_t i = 0; i < combo_options[x].size(); i++)
              cp[cp_i++] = combo_options[x][i];
            cp[cp_i++] = '\0';
          }
          cp[cp_i++] = '\0';
        }
      }
      ImGui::Combo("Choose Tool", &operation_tool, cp);
      static int layer_selection = -1;
      combo_options.clear();
      for (std::vector<PrimitiveContainer*>::iterator it =
             globals->renderer->GetPrimitiveStack()->begin();
           it != globals->renderer->GetPrimitiveStack()->end();
           ++it) {
        if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
            (*it)->type == "part") {
          for (size_t x = 0; x < (*it)->part->paths.size(); x++) {
            if (!(std::find(combo_options.begin(),
                            combo_options.end(),
                            (*it)->part->paths[x].layer) !=
                  combo_options.end())) {
              combo_options.push_back((*it)->part->paths[x].layer);
            }
          }
        }
      }
      static char* lp = NULL;
      if (lp == NULL) {
        int lp_i = 0;
        for (size_t x = 0; x < combo_options.size(); x++) {
          for (size_t i = 0; i < combo_options[x].size(); i++)
            lp_i++;
          lp_i++;
        }
        lp_i++;
        lp = (char*) malloc((lp_i + 1) * sizeof(char));
        lp_i = 0;
        if (lp != NULL) {
          int lp_i = 0;
          for (size_t x = 0; x < combo_options.size(); x++) {
            for (size_t i = 0; i < combo_options[x].size(); i++)
              lp[lp_i++] = combo_options[x][i];
            lp[lp_i++] = '\0';
          }
          lp[lp_i++] = '\0';
        }
      }
      ImGui::Combo("Choose Layer", &layer_selection, lp);
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
          operation.layer = combo_options[layer_selection];
          operation.tool_number = operation_tool;
          operation.operation_type = operation_types::jet_cutting;
          self->toolpath_operations.push_back(operation);
          operation.lead_in_length = -1;
          operation.lead_out_length = -1;
        }
        operation_tool = -1;
        layer_selection = -1;
        show_create_operation = false;
        free(cp);
        cp = NULL;
        free(lp);
        lp = NULL;
      }
      ImGui::End();
    }
    if (show_new_tool == true) {
      static tool_data_t tool;
      ImGui::Begin(
        "New Tool", &show_new_tool, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::InputText(
        "Tool Name", tool.tool_name, IM_ARRAYSIZE(tool.tool_name));

      for (auto& [key, value] : tool.params) {
        ImGui::InputFloat(key.c_str(), &value);
      }
      if (ImGui::Button("OK")) {
        // Quick sanity check
        bool skip_save{ false };
        for (const auto& [key, value] : tool.params) {
          if (value < 0.f || value > 50000.f) {
            // TODO: show popup
            LOG_F(WARNING, "Invalid tool input parameters.");
            skip_save = true;
            break;
          }
        }
        if (!skip_save) {
          self->tool_library.push_back(tool);
          nlohmann::json tool_library;
          for (size_t x = 0; x < self->tool_library.size(); x++) {
            nlohmann::json tool;
            tool["tool_name"] = std::string(self->tool_library[x].tool_name);
            for (const auto& [key, value] : self->tool_library[x].params) {
              tool[key] = value;
            }
            tool_library.push_back(tool);
          }

          globals->renderer->DumpJsonToFile(
            globals->renderer->GetConfigDirectory() + "tool_library.json",
            tool_library);
          show_new_tool = false;
        }
      }
      ImGui::End();
    }
    static tool_data_t tool;
    if (show_tool_edit != -1) {
      if (!tool.tool_name[0]) {
        sprintf(
          tool.tool_name, "%s", self->tool_library[show_tool_edit].tool_name);
        tool.params = self->tool_library[show_tool_edit].params;
      }
      ImGui::Begin("Tool Edit", NULL, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::InputText(
        "Tool Name",
        self->tool_library[show_tool_edit].tool_name,
        IM_ARRAYSIZE(self->tool_library[show_tool_edit].tool_name));
      for (const auto& [key, value] :
           self->tool_library[show_tool_edit].params) {
        ImGui::InputFloat(key.c_str(), &tool.params[key]);
      }
      if (ImGui::Button("OK")) {
        bool skip_save = false;
        for (const auto& [key, value] : tool.params) {
          if (value < 0.f || value > 50000.f) {
            // TODO: show popup
            LOG_F(WARNING, "Invalid tool input parameters.");
            skip_save = true;
            break;
          }
        }
        if (!skip_save) {
          self->tool_library[show_tool_edit].params = tool.params;
          nlohmann::json tool_library;
          for (size_t x = 0; x < self->tool_library.size(); x++) {
            nlohmann::json tool;
            tool["tool_name"] = std::string(self->tool_library[x].tool_name);
            for (const auto& [key, value] : self->tool_library[x].params) {
              tool[key] = value;
            }
            tool_library.push_back(tool);
          }
          globals->renderer->DumpJsonToFile(
            globals->renderer->GetConfigDirectory() + "tool_library.json",
            tool_library);
          show_tool_edit = -1;
        }
      }
      ImGui::End();
    }
    else {
      memset(tool.tool_name, 0, 1024);
    }
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
        ImGui::TableSetupColumn("ATHC");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        for (size_t x = 0; x < self->tool_library.size(); ++x) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Text("%s", self->tool_library[x].tool_name);
          ImGui::TableSetColumnIndex(1);
          ImGui::Text("%.4f", self->tool_library[x].params["pierce_height"]);
          ImGui::TableSetColumnIndex(2);
          ImGui::Text("%.4f", self->tool_library[x].params["pierce_delay"]);
          ImGui::TableSetColumnIndex(3);
          ImGui::Text("%.4f", self->tool_library[x].params["cut_height"]);
          ImGui::TableSetColumnIndex(4);
          ImGui::Text("%.4f", self->tool_library[x].params["kerf_width"]);
          ImGui::TableSetColumnIndex(5);
          ImGui::Text("%.4f", self->tool_library[x].params["feed_rate"]);
          ImGui::TableSetColumnIndex(6);
          ImGui::Text("%.4f", self->tool_library[x].params["athc"]);
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
    if (show_edit_tool_operation != -1) {
      ImGui::Begin("Edit Operation", NULL, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::InputDouble(
        "Lead In Length",
        &self->toolpath_operations[show_edit_tool_operation].lead_in_length);
      ImGui::InputDouble(
        "Lead Out Length",
        &self->toolpath_operations[show_edit_tool_operation].lead_out_length);
      if (ImGui::Button("OK")) {
        self->toolpath_operations[show_edit_tool_operation].last_enabled =
          false;
        show_edit_tool_operation = -1;
      }
      ImGui::End();
    }
    static PrimitiveContainer* selected_part = NULL;
    if (selected_part != NULL) {
      ImGui::Begin("Properties", NULL, ImGuiWindowFlags_AlwaysAutoResize);
      ImGui::Text(
        "Width: %.4f, Height: %.4f",
        (selected_part->part->bb_max.x - selected_part->part->bb_min.x),
        (selected_part->part->bb_max.y - selected_part->part->bb_min.y));
      ImGui::Text("Number of Paths: %lu", selected_part->part->paths.size());
      ImGui::Text("Number of Vertices: %lu",
                  selected_part->part->number_of_verticies);
      ImGui::Text("Offset X: %.4f", selected_part->part->control.offset.x);
      ImGui::Text("Offset Y: %.4f", selected_part->part->control.offset.y);
      ImGui::InputDouble("Scale", &selected_part->part->control.scale);
      ImGui::InputDouble("Angle", &selected_part->part->control.angle);
      ImGui::SliderFloat("Simplification",
                         &selected_part->part->control.smoothing,
                         0.f,
                         SCALE(5.f));
      if (ImGui::Button("Close")) {
        selected_part = NULL;
      }
      ImGui::End();
    }
    // ImGui::ShowDemoWindow();
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(ImVec2(300, globals->renderer->GetWindowSize().y));
    ImGui::PushStyleColor(ImGuiCol_WindowBg,
                          ImVec4(1.0f, 1.0f, 1.0f, 0.8f)); // Set window
                                                           // background to red
    ImGui::Begin("LeftPane",
                 NULL,
                 ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
                   ImGuiWindowFlags_NoMove |
                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                   ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_MenuBar);
    ImGui::PopStyleColor();
    if (ImGui::BeginTable(
          "parts_view_table",
          1,
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Parts Viewer");
      ImGui::TableHeadersRow();
      int count = 0;
      for (std::vector<PrimitiveContainer*>::iterator it =
             globals->renderer->GetPrimitiveStack()->begin();
           it != globals->renderer->GetPrimitiveStack()->end();
           ++it) {
        if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
            (*it)->type == "part" &&
            !((*it)->part->part_name.find(":") != std::string::npos)) {
          ImGui::TableNextRow();
          ImGui::TableSetColumnIndex(0);
          ImGui::Checkbox(std::string("##" + (*it)->part->part_name).c_str(),
                          &(*it)->properties->visible);
          ImGui::SameLine();
          bool tree_node_open = ImGui::TreeNode((*it)->part->part_name.c_str());
          if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("New Part")) {
              std::string path = ".";
              std::string p = globals->renderer->FileToString(
                globals->renderer->GetConfigDirectory() +
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
              action.data["part_name"] = (*it)->part->part_name;
              self->action_stack.push_back(action);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Show Duplicate Parts")) {
              for (std::vector<PrimitiveContainer*>::iterator dup =
                     globals->renderer->GetPrimitiveStack()->begin();
                   dup != globals->renderer->GetPrimitiveStack()->end();
                   ++dup) {
                if ((*dup)->properties->view ==
                      globals->renderer->GetCurrentView() &&
                    (*dup)->type == "part" &&
                    ((*dup)->part->part_name.find((*it)->part->part_name +
                                                  ":") != std::string::npos)) {
                  (*dup)->properties->visible = true;
                }
              }
            }
            if (ImGui::MenuItem("Hide Duplicate Parts")) {
              for (std::vector<PrimitiveContainer*>::iterator dup =
                     globals->renderer->GetPrimitiveStack()->begin();
                   dup != globals->renderer->GetPrimitiveStack()->end();
                   ++dup) {
                if ((*dup)->properties->view ==
                      globals->renderer->GetCurrentView() &&
                    (*dup)->type == "part" &&
                    ((*dup)->part->part_name.find((*it)->part->part_name +
                                                  ":") != std::string::npos)) {
                  (*dup)->properties->visible = false;
                }
              }
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete All Duplicates")) {
              LOG_F(INFO,
                    "Delete All Duplicated: %s",
                    (*it)->part->part_name.c_str());
              action_t action;
              action.action_id = "delete_duplicate_parts";
              action.data["part_name"] = (*it)->part->part_name;
              self->action_stack.push_back(action);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Delete Master Part")) {
              LOG_F(
                INFO, "Delete Master Part: %s", (*it)->part->part_name.c_str());
              action_t action;
              action.action_id = "delete_parts";
              action.data["part_name"] = (*it)->part->part_name;
              self->action_stack.push_back(action);
            }
            ImGui::Separator();
            if (ImGui::MenuItem("Properties")) {
              selected_part = (*it);
            }
            ImGui::EndPopup();
          }
          if (tree_node_open) {
            int dup_count = 0;
            for (std::vector<PrimitiveContainer*>::iterator dup =
                   globals->renderer->GetPrimitiveStack()->begin();
                 dup != globals->renderer->GetPrimitiveStack()->end();
                 ++dup) {
              if ((*dup)->properties->view ==
                    globals->renderer->GetCurrentView() &&
                  (*dup)->type == "part" &&
                  ((*dup)->part->part_name.find((*it)->part->part_name + ":") !=
                   std::string::npos)) {
                ImGui::Checkbox(std::string("Duplicate " +
                                            std::to_string(dup_count) + "##" +
                                            (*dup)->part->part_name)
                                  .c_str(),
                                &(*dup)->properties->visible);
                if (ImGui::BeginPopupContextItem()) {
                  if (ImGui::MenuItem("Delete Duplicate")) {
                    action_t action;
                    action.action_id = "delete_parts";
                    action.data["part_name"] = (*dup)->part->part_name;
                    self->action_stack.push_back(action);
                  }
                  ImGui::EndPopup();
                }
                dup_count++;
              }
            }
            ImGui::TreePop();
          }
          count++;
        }
      }
      ImGui::EndTable();
    }
    ImGui::Separator();
    if (ImGui::BeginTable(
          "operations_view_table",
          1,
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders)) {
      ImGui::TableSetupColumn("Operations Viewer");
      ImGui::TableHeadersRow();
      ImGui::TableNextRow();
      ImGui::TableSetColumnIndex(0);
      if (ImGui::Button("Create Operation")) {
        show_create_operation = true;
      }
      for (size_t x = 0; x < self->toolpath_operations.size(); x++) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Checkbox(std::string("##operation-" + std::to_string(x)).c_str(),
                        &self->toolpath_operations[x].enabled);
        ImGui::SameLine();
        if (self->toolpath_operations[x].operation_type ==
            operation_types::jet_cutting) {
          ImGui::Text("Jet: %s", self->toolpath_operations[x].layer.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Edit")) {
          show_edit_tool_operation = x;
        }
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
          action_t action;
          action.action_id = "delete_operation";
          action.data["index"] = x;
          self->action_stack.push_back(action);
        }
      }
      ImGui::EndTable();
    }
    ImGui::Separator();
    if (ImGui::BeginTable(
          "layers_table",
          1,
          ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Reorderable |
            ImGuiTableFlags_Hideable | ImGuiTableFlags_Borders)) {
      std::vector<std::string> layers_stack;
      ImGui::TableSetupColumn("Layers Viewer");
      ImGui::TableHeadersRow();
      for (std::vector<PrimitiveContainer*>::iterator it =
             globals->renderer->GetPrimitiveStack()->begin();
           it != globals->renderer->GetPrimitiveStack()->end();
           ++it) {
        if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
            (*it)->type == "part") {
          for (size_t x = 0; x < (*it)->part->paths.size(); x++) {
            if (!(std::find(layers_stack.begin(),
                            layers_stack.end(),
                            (*it)->part->paths[x].layer) !=
                  layers_stack.end())) {
              layers_stack.push_back((*it)->part->paths[x].layer);
            }
          }
        }
      }
      for (size_t x = 0; x < layers_stack.size(); x++) {
        ImGui::TableNextRow();
        ImGui::TableSetColumnIndex(0);
        ImGui::Text("%s", layers_stack[x].c_str());
      }
      ImGui::EndTable();
    }
    ImGui::End();
  }
}
void jetCamView::DeletePart(std::string part_name)
{
  std::vector<PrimitiveContainer*>::iterator it;
  for (it = globals->renderer->GetPrimitiveStack()->begin();
       it != globals->renderer->GetPrimitiveStack()->end();
       ++it) {
    if ((*it)->properties->view == globals->renderer->GetCurrentView()) {
      if ((*it)->type == "part") {
        if ((*it)->part->part_name == part_name) {
          (*it)->destroy();
          delete *it;
          it = globals->renderer->GetPrimitiveStack()->erase(it);
        }
      }
    }
  }
}
void jetCamView::RenderProgressWindow(void* p)
{
  jetCamView* self = reinterpret_cast<jetCamView*>(p);
  if (self != NULL) {
    ImGui::Begin("Progress",
                 &self->ProgressWindowHandle->visible,
                 ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::ProgressBar(self->ProgressWindowProgress, ImVec2(0.0f, 0.0f));
    ImGui::SameLine(0.0f, ImGui::GetStyle().ItemInnerSpacing.x);
    ImGui::Text("Progress");
    ImGui::End();
  }
}
void jetCamView::ShowProgressWindow(bool v)
{
  this->ProgressWindowProgress = 0.0;
  this->ProgressWindowHandle->visible = true;
}

bool jetCamView::DxfFileOpen(std::string filename,
                             std::string name,
                             int         import_quality,
                             float       import_scale)
{
  this->dxf_fp = fopen(filename.c_str(), "rt");
  if (this->dxf_fp) {
    this->dxf_nest = PolyNest::PolyNest();
    this->dxf_nest.SetExtents(
      { this->material_plane->bottom_left.x,
        this->material_plane->bottom_left.y },
      { this->material_plane->bottom_left.x + this->material_plane->width,
        this->material_plane->bottom_left.y + this->material_plane->height });
    for (auto* pPrimitive : *globals->renderer->GetPrimitiveStack()) {
      // Copy existing objects
      if (pPrimitive->properties->view == globals->renderer->GetCurrentView() &&
          pPrimitive->type == "part") {
        std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
        for (const auto& path : pPrimitive->part->paths) {
          if (path.is_inside_contour == false) {
            std::vector<PolyNest::PolyPoint> points;
            points.reserve(path.points.size());
            for (const double_point_t& p : path.points) {
              points.push_back({ p.x, p.y });
            }
            poly_part.push_back(std::move(points));
          }
        }
        this->dxf_nest.PushPlacedPolyPart(
          poly_part,
          &pPrimitive->part->control.offset.x,
          &pPrimitive->part->control.offset.y,
          &pPrimitive->part->control.angle,
          &pPrimitive->part->properties->visible);
      }
    }
    this->dl_dxf = new DL_Dxf();
    this->DXFcreationInterface = new DXFParsePathAdaptor(
      globals->renderer, &this->ViewMatrixCallback, &this->MouseEventCallback);
    this->DXFcreationInterface->SetFilename(name);
    this->DXFcreationInterface->SetImportScale(import_scale);
    this->DXFcreationInterface->SetImportQuality(import_quality);
    // this->DXFcreationInterface->SetSmoothing(0.030);
    globals->renderer->PushTimer(100, this->DxfFileParseTimer, this);
    LOG_F(INFO, "Parsing DXF File: %s", filename.c_str());
    return true;
  }
  return false;
}

bool jetCamView::DxfFileParseTimer(void* p)
{
  jetCamView* self = reinterpret_cast<jetCamView*>(p);
  if (self != NULL) {
    // self->DXFcreationInterface->SetScaleFactor(15.4);
    // self->DXFcreationInterface->SetSmoothing(0.0);
    // self->DXFcreationInterface->SetChainTolorance(0.001);
    while (
      self->dl_dxf->readDxfGroups(self->dxf_fp, self->DXFcreationInterface)) {
    }
    LOG_F(INFO, "Successfully parsed DXF groups.");

    // double_point_t bbox_min, bbox_max;
    // size_t         vertex_count;
    // self->DXFcreationInterface->
    // self->DXFcreationInterface->GetApproxBoundingBox(
    //   bbox_min, bbox_max, vertex_count);
    // const double area = (bbox_max.x - bbox_min.x) * (bbox_max.y -
    // bbox_min.y); const double avg_density = vertex_count / area; const
    // double target_density =
    //   1 / std::pow(self->DXFcreationInterface->chain_tolorance, 2);

    // LOG_F(INFO,
    //       "avg density = %f, target density = %f",
    //       avg_density,
    //       target_density);

    self->DXFcreationInterface->Finish();
    for (auto* pPrimitive : *globals->renderer->GetPrimitiveStack()) {
      if (pPrimitive->properties->view == globals->renderer->GetCurrentView() &&
          pPrimitive->type == "part" &&
          pPrimitive->part->part_name == self->DXFcreationInterface->filename) {
        std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
        for (const auto& path : pPrimitive->part->paths) {
          if (path.is_inside_contour == false) {
            std::vector<PolyNest::PolyPoint> points;
            points.reserve(path.points.size());
            for (const double_point_t& p : path.points) {
              points.push_back({ p.x, p.y });
            }
            poly_part.push_back(std::move(points));
          }
        }
        pPrimitive->part->properties->visible = true;
        self->dxf_nest.PushUnplacedPolyPart(
          poly_part,
          &pPrimitive->part->control.offset.x,
          &pPrimitive->part->control.offset.y,
          &pPrimitive->part->control.angle,
          &pPrimitive->part->properties->visible);
      }
    }
    self->dxf_nest.BeginPlaceUnplacedPolyParts();
    globals->renderer->PushTimer(
      0, self->dxf_nest.PlaceUnplacedPolyPartsTick, &self->dxf_nest);
    fclose(self->dxf_fp);
    delete self->DXFcreationInterface;
    delete self->dl_dxf;
    return false;
  }
  return false;
}
void jetCamView::PreInit()
{
  this->mouse_over_color = "blue";
  this->outside_contour_color = "white";
  this->inside_contour_color = "grey";
  this->open_contour_color = "yellow";

  this->show_viewer_context_menu.x = -1000000;
  this->show_viewer_context_menu.y = -1000000;

  this->preferences.background_color[0] = 58 / 255.0f;
  this->preferences.background_color[1] = 58 / 255.0;
  this->preferences.background_color[2] = 58 / 255.0f;

  this->CurrentTool = JetCamTool::Contour;
  this->left_click_pressed = false;

  nlohmann::json tool_library = globals->renderer->ParseJsonFromFile(
    globals->renderer->GetConfigDirectory() + "tool_library.json");
  if (tool_library != NULL) {
    LOG_F(
      INFO,
      "Found %s!",
      std::string(globals->renderer->GetConfigDirectory() + "tool_library.json")
        .c_str());
    for (size_t x = 0; x < tool_library.size(); x++) {
      tool_data_t tool;
      std::string tool_name = tool_library[x]["tool_name"];
      sprintf(tool.tool_name, "%s", tool_name.c_str());
      for (auto& [key, value] : tool.params) {
        value = static_cast<float>(tool_library[x][key]);
      }
      this->tool_library.push_back(tool);
    }
  }
  else {
    LOG_F(
      INFO,
      "Could not find %s!",
      std::string(globals->renderer->GetConfigDirectory() + "tool_library.json")
        .c_str());
  }
}
void jetCamView::Init()
{
  globals->renderer->SetCurrentView("jetCamView");
  using EventType = EasyRender::EventType;
  using Action = EasyRender::ActionFlagBits;
  globals->renderer->PushEvent(
    GLFW_KEY_UNKNOWN, EventType::Scroll, &this->ZoomEventCallback);
  globals->renderer->PushEvent(
    GLFW_MOUSE_BUTTON_1, +Action::Any, &this->MouseCallback);
  globals->renderer->PushEvent(
    GLFW_KEY_UNKNOWN, EventType::MouseMove, &this->MouseCallback);
  globals->renderer->PushEvent(
    GLFW_KEY_TAB, +Action::Press, &this->TabKeyCallback);
  globals->renderer->PushEvent(GLFW_KEY_LEFT_CONTROL,
                               Action::Press | Action::Release,
                               &this->ModKeyCallback);
  globals->renderer->PushEvent(GLFW_KEY_LEFT_SHIFT,
                               Action::Press | Action::Release,
                               &this->ModKeyCallback);
  this->menu_bar = globals->renderer->PushGui(true, &this->RenderUI, this);
  this->ProgressWindowHandle =
    globals->renderer->PushGui(false, &this->RenderProgressWindow, this);
  this->material_plane = globals->renderer->PushPrimitive(
    new EasyPrimitive::Box({ 0, 0 },
                           this->job_options.material_size[0],
                           this->job_options.material_size[1],
                           0));
  this->material_plane->properties->id = "material_plane";
  this->material_plane->properties->zindex = -20;
  this->material_plane->properties->color[0] = 100;
  this->material_plane->properties->color[1] = 0;
  this->material_plane->properties->color[2] = 0;
  this->material_plane->properties->matrix_callback = &this->ViewMatrixCallback;
  this->material_plane->properties->mouse_callback = &this->MouseEventCallback;
  // globals->renderer->SetShowFPS(false);
}
void jetCamView::Tick()
{
  for (std::vector<PrimitiveContainer*>::iterator it =
         globals->renderer->GetPrimitiveStack()->begin();
       it != globals->renderer->GetPrimitiveStack()->end();
       ++it) {
    if ((*it)->properties->view == globals->renderer->GetCurrentView()) {
      if ((*it)->type == "part") {
        (*it)->part->control.mouse_mode = static_cast<int>(this->CurrentTool);
      }
    }
  }
  if (this->toolpath_operations.size() > 0) {
    for (size_t x = 0; x < this->toolpath_operations.size(); x++) {
      if (this->toolpath_operations[x].enabled !=
          this->toolpath_operations[x].last_enabled) {
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();
             ++it) {
          if ((*it)->properties->view == globals->renderer->GetCurrentView()) {
            if ((*it)->type == "part" && (*it)->properties->visible == true) {
              for (size_t y = 0; y < (*it)->part->paths.size(); y++) {
                if ((*it)->part->paths[y].layer ==
                    this->toolpath_operations[x].layer) {
                  if (this->tool_library.size() >
                      this->toolpath_operations[x].tool_number) {
                    (*it)->part->paths[y].toolpath_offset =
                      this
                        ->tool_library[this->toolpath_operations[x].tool_number]
                        .params["kerf_width"];
                  }
                  (*it)->part->paths[y].toolpath_visible =
                    this->toolpath_operations[x].enabled;
                  (*it)->part->control.lead_in_length =
                    this->toolpath_operations[x].lead_in_length;
                  (*it)->part->control.lead_out_length =
                    this->toolpath_operations[x].lead_out_length;
                  (*it)->part->last_control.angle = 1; // Triggers rebuild
                }
              }
            }
          }
        }
      }
      this->toolpath_operations[x].last_enabled =
        this->toolpath_operations[x].enabled;
    }
  }
  /*else
  {
      for (std::vector<PrimitiveContainer*>::iterator it =
  globals->renderer->GetPrimitiveStack()->begin(); it !=
  globals->renderer->GetPrimitiveStack()->end(); ++it)
      {
          if ((*it)->properties->view == globals->renderer->GetCurrentView())
          {
              if ((*it)->type == "part")
              {
                  (*it)->part->last_control.angle = 1; //Triggers rebuild
                  for (size_t y = 0; y < (*it)->part->paths.size(); y++)
                  {
                      (*it)->part->paths[y].toolpath_visible = false;
                  }
              }
          }
      }
  }*/
  while (this->action_stack.size() > 0) {
    action_t action = this->action_stack[0];
    this->action_stack.erase(this->action_stack.begin());
    try {
      if (action.action_id == "post_process") {
        std::ofstream gcode_file;
        std::string   file = action.data["file"];
        gcode_file.open(file);
        if (gcode_file.is_open()) {
          for (size_t i = 0; i < this->toolpath_operations.size(); i++) {
            LOG_F(INFO,
                  "Posting toolpath operation: %lu on layer: %s",
                  i,
                  this->toolpath_operations[i].layer.c_str());
            for (std::vector<PrimitiveContainer*>::iterator it =
                   globals->renderer->GetPrimitiveStack()->begin();
                 it != globals->renderer->GetPrimitiveStack()->end();
                 ++it) {
              if ((*it)->properties->view ==
                    globals->renderer->GetCurrentView() &&
                  (*it)->properties->visible == true && (*it)->type == "part") {
                std::vector<std::vector<double_point_t>> tool_paths =
                  (*it)->part->GetOrderedToolpaths();
                if (this->tool_library.size() >
                    this->toolpath_operations[i].tool_number) {
                  // Output tool-specific THC target once at beginning to let us
                  // update the value internally. The fire_torch and torch_off
                  // macros will handle the rest
                  if (float thc_val =
                        this
                          ->tool_library[this->toolpath_operations[i]
                                           .tool_number]
                          .params["athc"];
                      thc_val > 0) {
                    gcode_file << "$T=" << thc_val << "\n";
                  }
                  for (size_t x = 0; x < tool_paths.size(); x++) {
                    gcode_file << "G0 X" << tool_paths[x][0].x << " Y"
                               << tool_paths[x][0].y << "\n";
                    gcode_file << "fire_torch "
                               << this
                                    ->tool_library[this->toolpath_operations[i]
                                                     .tool_number]
                                    .params["pierce_height"]
                               << " "
                               << this
                                    ->tool_library[this->toolpath_operations[i]
                                                     .tool_number]
                                    .params["pierce_delay"]
                               << " "
                               << this
                                    ->tool_library[this->toolpath_operations[i]
                                                     .tool_number]
                                    .params["cut_height"]
                               << "\n";
                    for (size_t z = 0; z < tool_paths[x].size(); z++) {
                      gcode_file
                        << "G1 X" << tool_paths[x][z].x << " Y"
                        << tool_paths[x][z].y << " F"
                        << this
                             ->tool_library[this->toolpath_operations[i]
                                              .tool_number]
                             .params["feed_rate"]
                        << "\n";
                    }
                    gcode_file << "torch_off\n";
                  }
                }
              }
            }
          }
          gcode_file << "M30\n";
          gcode_file.close();
          LOG_F(INFO, "Finished writing gcode file!");
        }
        else {
          LOG_F(WARNING, "Could not open gcode file for writing!");
        }
      }
      if (action.action_id == "rebuild_part_toolpaths") {
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();
             ++it) {
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part") {
            (*it)->part->last_control.angle = 1; // Rebuild toolpaths
          }
        }
        for (size_t x = 0; x < this->toolpath_operations.size(); x++) {
          this->toolpath_operations[x].last_enabled =
            false; // Rebuild all toolpaths
        }
      }
      if (action.action_id == "delete_operation") {
        size_t index = (size_t) action.data["index"];
        if (this->toolpath_operations.size() > index) {
          this->toolpath_operations.erase(this->toolpath_operations.begin() +
                                          index);
        }
      }
      if (action.action_id == "delete_duplicate_parts") {
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();) {
          std::string part_name = action.data["part_name"];
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part" &&
              ((*it)->part->part_name.find(part_name + ":") !=
               std::string::npos)) {
            (*it)->destroy();
            delete *it;
            it = globals->renderer->GetPrimitiveStack()->erase(it);
          }
          else {
            ++it;
          }
        }
      }
      if (action.action_id == "delete_parts") {
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();) {
          std::string part_name = action.data["part_name"];
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part" &&
              ((*it)->part->part_name.find(part_name) != std::string::npos)) {
            (*it)->destroy();
            delete *it;
            it = globals->renderer->GetPrimitiveStack()->erase(it);
          }
          else {
            ++it;
          }
        }
      }
      if (action.action_id == "delete_part") {
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();) {
          std::string part_name = action.data["part_name"];
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part" &&
              ((*it)->part->part_name.find(part_name) != std::string::npos)) {
            (*it)->destroy();
            delete *it;
            it = globals->renderer->GetPrimitiveStack()->erase(it);
          }
          else {
            ++it;
          }
          break;
        }
      }
      if (action.action_id == "duplicate_part") {
        int         dup_count = 1;
        std::string part_name = action.data["part_name"];
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();
             ++it) {
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part" &&
              ((*it)->part->part_name.find(part_name + ":") !=
               std::string::npos)) {
            dup_count++;
          }
        }
        EasyPrimitive::Part* p;
        this->dxf_nest = PolyNest::PolyNest();
        this->dxf_nest.SetExtents(
          PolyNest::PolyPoint(this->material_plane->bottom_left.x,
                              this->material_plane->bottom_left.y),
          PolyNest::PolyPoint(this->material_plane->bottom_left.x +
                                this->material_plane->width,
                              this->material_plane->bottom_left.y +
                                this->material_plane->height));
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();
             ++it) {
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part") {
            std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
            for (size_t x = 0; x < (*it)->part->paths.size(); x++) {
              if ((*it)->part->paths[x].is_inside_contour == false) {
                std::vector<PolyNest::PolyPoint> points;
                for (size_t i = 0; i < (*it)->part->paths[x].points.size();
                     i++) {
                  PolyNest::PolyPoint p;
                  p.x = (*it)->part->paths[x].points[i].x;
                  p.y = (*it)->part->paths[x].points[i].y;
                  points.push_back(p);
                }
                poly_part.push_back(points);
              }
            }
            this->dxf_nest.PushPlacedPolyPart(
              poly_part,
              &(*it)->part->control.offset.x,
              &(*it)->part->control.offset.y,
              &(*it)->part->control.angle,
              &(*it)->part->properties->visible);
          }
        }
        for (std::vector<PrimitiveContainer*>::iterator it =
               globals->renderer->GetPrimitiveStack()->begin();
             it != globals->renderer->GetPrimitiveStack()->end();
             ++it) {
          if ((*it)->properties->view == globals->renderer->GetCurrentView() &&
              (*it)->type == "part" && ((*it)->part->part_name == part_name)) {
            std::vector<EasyPrimitive::Part::path_t>      paths;
            std::vector<std::vector<PolyNest::PolyPoint>> poly_part;
            for (size_t x = 0; x < (*it)->part->paths.size(); x++) {
              paths.push_back((*it)->part->paths[x]);
              if ((*it)->part->paths[x].is_inside_contour == false) {
                std::vector<PolyNest::PolyPoint> points;
                for (size_t i = 0; i < (*it)->part->paths[x].points.size();
                     i++) {
                  PolyNest::PolyPoint p;
                  p.x = (*it)->part->paths[x].points[i].x;
                  p.y = (*it)->part->paths[x].points[i].y;
                  points.push_back(p);
                }
                poly_part.push_back(points);
              }
            }
            p = globals->renderer->PushPrimitive(new EasyPrimitive::Part(
              (*it)->part->part_name + ":" + std::to_string(dup_count), paths));
            if (dup_count > 1) {
              for (std::vector<PrimitiveContainer*>::iterator dup =
                     globals->renderer->GetPrimitiveStack()->begin();
                   dup != globals->renderer->GetPrimitiveStack()->end();
                   ++dup) {
                if ((*dup)->properties->view ==
                      globals->renderer->GetCurrentView() &&
                    (*dup)->type == "part" &&
                    ((*dup)->part->part_name ==
                     part_name + ":" + std::to_string(dup_count - 1))) {
                  p->control.offset.x = (*dup)->part->control.offset.x;
                  p->control.offset.y = (*dup)->part->control.offset.y;
                }
              }
            }
            else {
              p->control.offset.x = (*it)->part->control.offset.x;
              p->control.offset.y = (*it)->part->control.offset.y;
            }
            p->control.scale = (*it)->part->control.scale;
            p->properties->mouse_callback = (*it)->properties->mouse_callback;
            p->properties->matrix_callback = (*it)->properties->matrix_callback;
            this->dxf_nest.PushUnplacedPolyPart(poly_part,
                                                &p->control.offset.x,
                                                &p->control.offset.y,
                                                &p->control.angle,
                                                &p->properties->visible);
            break;
          }
        }
        this->dxf_nest.BeginPlaceUnplacedPolyParts();
        globals->renderer->PushTimer(
          0, this->dxf_nest.PlaceUnplacedPolyPartsTick, &this->dxf_nest);
      }
    }
    catch (const std::exception& e) {
      LOG_F(ERROR, "(jetCamView::Tick) Exception: %s", e.what());
    }
  }
}
void jetCamView::MakeActive()
{
  globals->renderer->SetCurrentView("jetCamView");
  globals->renderer->SetClearColor(
    this->preferences.background_color[0] * 255.0f,
    this->preferences.background_color[1] * 255.0f,
    this->preferences.background_color[2] * 255.0f);
}
void jetCamView::Close() {}
