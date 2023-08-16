#include "adminView.h"
#include <EasyRender/EasyRender.h>
#include <EasyRender/logging/loguru.h>
#include <EasyRender/gui/imgui.h>

void adminView::ZoomEventCallback(nlohmann::json e)
{
    double_point_t matrix_mouse = globals->renderer->GetWindowMousePosition();
    matrix_mouse.x = (matrix_mouse.x - globals->pan.x) / globals->zoom;
    matrix_mouse.y = (matrix_mouse.y - globals->pan.y) / globals->zoom;
    if ((float)e["scroll"] > 0)
    {
        double old_zoom = globals->zoom;
        globals->zoom += globals->zoom * 0.125;
        if (globals->zoom > 1000000)
        {
            globals->zoom = 1000000;
        }
        double scalechange = old_zoom - globals->zoom;
        globals->pan.x += matrix_mouse.x * scalechange;
        globals->pan.y += matrix_mouse.y * scalechange;
    }
    else
    {
        double old_zoom = globals->zoom;
        globals->zoom += globals->zoom * -0.125;
        if (globals->zoom < 0.00001)
        {
            globals->zoom = 0.00001;
        }
        double scalechange = old_zoom - globals->zoom;
        globals->pan.x += matrix_mouse.x * scalechange;
        globals->pan.y += matrix_mouse.y * scalechange;
    }
}
void adminView::ViewMatrixCallback(PrimativeContainer *p)
{
    p->properties->scale = globals->zoom;
    p->properties->offset[0] = globals->pan.x;
    p->properties->offset[1] = globals->pan.y;
}
void adminView::RenderUI(void *self_pointer)
{
    adminView *self = reinterpret_cast<adminView *>(self_pointer);
    if (self != NULL)
    {
        static bool show_list_clients = false;
        static nlohmann::json response_data;
        if (show_list_clients == true)
        {
            ImGui::Begin("List Clients", &show_list_clients, ImGuiWindowFlags_AlwaysAutoResize);
                if (ImGui::BeginTable("Clients", 3, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    ImGui::TableSetupColumn("ID");
                    ImGui::TableSetupColumn("IP");
                    ImGui::TableSetupColumn("Action");
                    ImGui::TableHeadersRow();
                    if (response_data.empty())
                    {
                        response_data = globals->websocket->SendPacketAndPollResponse({
                            {"cmd", "list_clients"}
                        });
                    }
                    try
                    {
                        for (size_t x = 0; x < response_data["results"].size(); x++)
                        {
                            ImGui::TableNextRow();
                            std::string peer_id = response_data["results"][x]["peer_id"];
                            std::string peer_ip = response_data["results"][x]["peer_ip"];
                            ImGui::TableSetColumnIndex(0); ImGui::Text("%s", peer_id.c_str());
                            ImGui::TableSetColumnIndex(1); ImGui::Text("%s", peer_ip.c_str());
                            ImGui::TableSetColumnIndex(2);
                            if (ImGui::Button(std::string("Activate##Activate-" + std::to_string(x)).c_str()))
                            {
                                //LOG_F(INFO, "Activate: %lu", x);
                                globals->websocket->SendPacketAndPollResponse({
                                    {"peer_id", peer_id},
                                    {"peer_cmd", "test"}
                                });
                            }
                        }
                    }
                    catch(const std::exception& e)
                    {
                        LOG_F(ERROR, "(adminView::RenderUI) Exception: %s", e.what());
                    }
                    ImGui::EndTable();
                }
            ImGui::End();
        }
        else
        {
            if (!response_data.empty())
            {
                response_data.clear();
            }
        }
        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("File"))
            {
                if (ImGui::MenuItem("Close", ""))
                {
                    LOG_F(INFO, "Edit->Close");
                    globals->quit = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Utilities"))
            {
                if (ImGui::MenuItem("List Clients", ""))
                {
                    LOG_F(INFO, "Utilities->List Clients");
                    show_list_clients = true;
                }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Workbench"))
            {
                if (ImGui::MenuItem("Machine Control", ""))
                {
                    LOG_F(INFO, "Workbench->Machine Control");
                    globals->nc_control_view->MakeActive();
                }
                ImGui::Separator();
                if (ImGui::MenuItem("CAM Toolpaths", ""))
                {
                    LOG_F(INFO, "Workbench->CAM Toolpaths");
                    globals->jet_cam_view->MakeActive();
                }
                ImGui::EndMenu();
            }
            ImGui::EndMainMenuBar();
        }
        ImGui::End();
    }
}
void adminView::PreInit()
{
    
}
void adminView::Init()
{
    globals->renderer->SetCurrentView("adminView");
    globals->renderer->PushEvent("up", "scroll", &this->ZoomEventCallback);
    globals->renderer->PushEvent("down", "scroll", &this->ZoomEventCallback);
    this->ui = globals->renderer->PushGui(true, &this->RenderUI, this);
    //globals->renderer->SetShowFPS(true);
}
void adminView::Tick()
{
    
}
void adminView::MakeActive()
{
    globals->renderer->SetCurrentView("adminView");
    globals->renderer->SetClearColor(0.5f, 0.5f, 0.5f);
}
void adminView::Close()
{

}