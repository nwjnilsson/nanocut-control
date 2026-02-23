#ifndef WEBSOCKET_CLIENT_
#define WEBSOCKET_CLIENT_

extern "C" {
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
#  include <winsock2.h>
#endif
#include <mongoose.h>
}
#include <nlohmann/json.hpp>

// Forward declaration
class NcApp;
class NcRender;

class WebsocketClient {
public:
  bool           m_is_connected;
  bool           m_is_waiting_for_response;
  unsigned long  m_response_timer;
  nlohmann::json m_last_received_packet;

  std::string m_id;

private:
  NcRender* m_renderer{ nullptr }; // Reference to renderer for timer
                                   // functionality

public:
  static void fn(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
  static bool reconnectTimer(void* self_pointer);

  struct mg_mgr         m_mgr;
  struct mg_connection* m_client;

  std::string getRandomString(size_t length);
  // Constructor with dependency injection
  WebsocketClient(NcRender* renderer = nullptr) : m_renderer(renderer) {}

  void           setRenderer(NcRender* renderer) { m_renderer = renderer; }
  void           setIdAuto();
  void           setId(std::string id_);
  void           send(nlohmann::json packet);
  void           send(std::string msg);
  nlohmann::json sendPacketAndPollResponse(nlohmann::json packet);
  void           handleWebsocketMessage(std::string msg);
  void           connect();
  void           init();
  void           poll();
  void           close();
};

#endif // WEBSOCKET_CLIENT_
