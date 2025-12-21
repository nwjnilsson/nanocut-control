#include "NcAdminView.h"
#include "../application/NcApp.h"
#include "../input/InputState.h"
#include "WebsocketClient/WebsocketClient.h"
#include "NcCamView/NcCamView.h"
#include "NcControlView/NcControlView.h"
#include <NcRender/NcRender.h>
#include <NcRender/gui/imgui.h>
#include <NcRender/logging/loguru.h>

void NcAdminView::zoomEventCallback(const ScrollEvent& e, const InputState& input)
{
  if (!m_app)
    return;
  // Zoom - use View's zoom functionality
  Point2d screen_center = input.getMousePosition();
  double  zoom_factor = (e.y_offset > 0) ? 1.125 : 0.875;
  adjustZoom(zoom_factor, screen_center);
}
void NcAdminView::renderUI()
{
  if (m_app) {
    static bool           show_list_clients = false;
    static nlohmann::json response_data;
    if (show_list_clients == true) {
      ImGui::Begin(
        "List Clients", &show_list_clients, ImGuiWindowFlags_AlwaysAutoResize);
      if (ImGui::BeginTable(
            "Clients", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg)) {
        ImGui::TableSetupColumn("ID");
        ImGui::TableSetupColumn("IP");
        ImGui::TableSetupColumn("Action");
        ImGui::TableHeadersRow();
        if (response_data.empty()) {
          response_data = m_app->getWebsocketClient().sendPacketAndPollResponse(
            { { "cmd", "list_clients" } });
        }
        try {
          for (size_t x = 0; x < response_data["results"].size(); x++) {
            ImGui::TableNextRow();
            std::string peer_id = response_data["results"][x]["peer_id"];
            std::string peer_ip = response_data["results"][x]["peer_ip"];
            ImGui::TableSetColumnIndex(0);
            ImGui::Text("%s", peer_id.c_str());
            ImGui::TableSetColumnIndex(1);
            ImGui::Text("%s", peer_ip.c_str());
            ImGui::TableSetColumnIndex(2);
            if (ImGui::Button(
                  std::string("Activate##Activate-" + std::to_string(x))
                    .c_str())) {
              // LOG_F(INFO, "Activate: %lu", x);
              m_app->getWebsocketClient().sendPacketAndPollResponse(
                { { "peer_id", peer_id }, { "peer_cmd", "test" } });
            }
          }
        }
        catch (const std::exception& e) {
          LOG_F(ERROR, "(NcAdminView::RenderUI) Exception: %s", e.what());
        }
        ImGui::EndTable();
      }
      ImGui::End();
    }
    else {
      if (!response_data.empty()) {
        response_data.clear();
      }
    }
    if (ImGui::BeginMainMenuBar()) {
      if (ImGui::BeginMenu("File")) {
        if (ImGui::MenuItem("Close", "")) {
          LOG_F(INFO, "Edit->Close");
          m_app->requestQuit();
        }
        ImGui::EndMenu();
      }
      if (ImGui::BeginMenu("Utilities")) {
        if (ImGui::MenuItem("List Clients", "")) {
          LOG_F(INFO, "Utilities->List Clients");
          show_list_clients = true;
        }
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
          m_app->getCamView().makeActive();
        }
        ImGui::EndMenu();
      }
      ImGui::EndMainMenuBar();
    }
    // ImGui::End();
  }
}
void NcAdminView::preInit() {}
void NcAdminView::init()
{
  if (!m_app)
    return;
  // Initialize UI - scroll events now handled through view delegation

  m_app->getRenderer().setCurrentView("NcAdminView");
  ui = m_app->getRenderer().pushGui(true, [this]() { this->renderUI(); });
  // globals->renderer->SetShowFPS(true);
}
void NcAdminView::tick() {}
void NcAdminView::makeActive()
{
  if (!m_app)
    return;
  // View is now active

  m_app->setCurrentActiveView(this); // Register for event delegation
  m_app->getRenderer().setCurrentView("NcAdminView");
  m_app->getRenderer().setClearColor(0.5f, 0.5f, 0.5f);
}
void NcAdminView::close() {}

void NcAdminView::handleScrollEvent(const ScrollEvent& e, const InputState& input)
{
  zoomEventCallback(e, input);
}
