#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include "application.h"
#include "EasyRender/EasyRender.h"
#include "EasyRender/logging/loguru.h"
#include "EasyRender/gui/imgui.h"
#include "WebsocketServer/WebsocketServer.h"
#include "WebsocketClient/WebsocketClient.h"

global_variables_t *globals;

void log_uptime()
{
    nlohmann::json uptime;
    unsigned long m = (EasyRender::Millis() - globals->start_timestamp);
    unsigned long seconds=(m/1000)%60;
    unsigned long minutes=(m/(1000*60))%60;
    unsigned long hours=(m/(1000*60*60))%24;
    LOG_F(INFO, "Shutting down, Adding Uptime: %luh %lum %lus to total", hours, minutes, seconds);
    nlohmann::json uptime_json = globals->renderer->ParseJsonFromFile(globals->renderer->GetConfigDirectory() + "uptime.json");
    if (uptime_json != NULL)
    {
        try
        {
            uptime["total_milliseconds"] = (unsigned long)uptime_json["total_milliseconds"] + m;
        }
        catch(...)
        {
            LOG_F(WARNING, "Error parsing uptime file!");
        }
        globals->renderer->DumpJsonToFile(globals->renderer->GetConfigDirectory() + "uptime.json", uptime);
    }
    else
    {
        uptime["total_milliseconds"] = m;
        globals->renderer->DumpJsonToFile(globals->renderer->GetConfigDirectory() + "uptime.json", uptime);
    }
}
int main(int argc, char **argv)
{
    if (argc > 1)
    {
        if (std::string(argv[1]) == "--daemon")
        {
            WebsocketServer Server;
            Server.Init();
            while(true)
            {
                Server.Poll();
            }
            return 0;
        }
    }
    globals = new global_variables_t;
    globals->quit = false;
    globals->zoom = 1;
    globals->pan.x = 0;
    globals->pan.y = 0;
    globals->start_timestamp = EasyRender::Millis();

    /* Allocate view pointers */
    globals->renderer = new EasyRender();
    globals->nc_control_view = new ncControlView();
    globals->jet_cam_view = new jetCamView();
    globals->admin_view = new adminView();

    globals->renderer->SetWindowTitle("NanoCut");
    globals->nc_control_view->PreInit();
    globals->jet_cam_view->PreInit();
    globals->renderer->SetGuiIniFileName(globals->renderer->GetConfigDirectory() + "gui.ini");
    globals->renderer->SetGuiLogFileName(globals->renderer->GetConfigDirectory() + "gui.log");
    globals->renderer->SetMainLogFileName(globals->renderer->GetConfigDirectory() + "NanoCut.log");
    //globals->renderer->SetShowFPS(true);
    globals->renderer->SetWindowSize(globals->nc_control_view->preferences.window_size[0], globals->nc_control_view->preferences.window_size[1]);
    globals->renderer->Init(argc, argv);

    globals->jet_cam_view->Init();
    globals->nc_control_view->Init();
    globals->admin_view->Init();

    globals->websocket = new WebsocketClient;
    /* This state needs to be saved persistantly */
    globals->websocket->SetIdAuto();
    globals->nc_control_view->MakeActive();
    //globals->jet_cam_view->MakeActive();
    if (argc > 1)
    {
        if (std::string(argv[1]) == "--admin")
        {
            globals->websocket->SetId("Admin");
            globals->admin_view->MakeActive();
        }
    }
    globals->websocket->Init();

    while(globals->renderer->Poll(globals->quit))
    {
        if (globals->renderer->GetCurrentView() == "ncControlView")
        {
            globals->nc_control_view->Tick();
        }
        if (globals->renderer->GetCurrentView() == "jetCamView")
        {
            globals->jet_cam_view->Tick();
        }
        if (globals->renderer->GetCurrentView() == "adminView")
        {
            globals->admin_view->Tick();
        }
        //globals->websocket->Poll();
    }
    log_uptime();
    globals->renderer->Close();
    globals->nc_control_view->Close();
    globals->admin_view->Close();
    globals->websocket->Close();
    delete globals->websocket;
    delete globals->renderer;
    delete globals->nc_control_view;
    delete globals->admin_view;
    delete globals;
    return 0;
}