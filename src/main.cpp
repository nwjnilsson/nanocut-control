#include "NcRender/NcRender.h"
#include "NcRender/gui/imgui.h"
#include "NcRender/logging/loguru.h"
#include "WebsocketClient/WebsocketClient.h"
#include "WebsocketServer/WebsocketServer.h"
#include "NcAdminView/NcAdminView.h"
#include "application.h"
#include "application/NcApp.h"
#include "NcCamView/NcCamView.h"
#include "NcControlView/NcControlView.h"
#include <memory>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

int main(int argc, char** argv)
{
  // Handle daemon mode first
  if (argc > 1) {
    if (std::string(argv[1]) == "--daemon") {
      WebsocketServer server;
      server.init();
      while (true) {
        server.poll();
      }
      return 0;
    }
  }

  // Create application context for modern dependency injection
  NcApp app;
  app.initialize(argc, argv);

  // Main application loop
  while (app.getRenderer().poll(app.shouldQuit())) {
    // Handle view ticking
    if (app.getRenderer().getCurrentView() == "NcControlView") {
      app.getControlView().tick();
    }
    if (app.getRenderer().getCurrentView() == "NcCamView") {
      app.getCamView().tick();
    }
    if (app.getRenderer().getCurrentView() == "NcAdminView") {
      app.getAdminView().tick();
    }
  }

  // NcApp automatically handles cleanup via RAII (including uptime logging)
  return 0;
}
