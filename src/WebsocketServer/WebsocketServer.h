#ifndef WEBSOCKET_SERVER_
#define WEBSOCKET_SERVER_

#include "../mongoose/mongoose.h"
#include <application.h>

class WebsocketServer {
public:
  static void fn(struct mg_connection* c, int ev, void* ev_data, void* fn_data);
  struct websocket_client_t {
    std::string    m_peer_ip;
    std::string    m_peer_id;
    mg_connection* m_connection;
  };
  std::vector<websocket_client_t> m_websocket_clients;
  struct mg_mgr                   m_mgr;
  void                            send(std::string peer_id, std::string msg);
  void send(websocket_client_t client, std::string msg);
  void handleWebsocketMessage(websocket_client_t client, std::string msg);
  void init();
  void poll();
  void close();
};

#endif // WEBSOCKET_SERVER_
