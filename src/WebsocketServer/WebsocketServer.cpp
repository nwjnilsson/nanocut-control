#define MG_ENABLE_LOG 0
#include "WebsocketServer.h"
#include <nlohmann/json.hpp>
#include <loguru.hpp>

void WebsocketServer::fn(struct mg_connection* c,
                         int                   ev,
                         void*                 ev_data,
                         void*                 fn_data)
{
  WebsocketServer* self = reinterpret_cast<WebsocketServer*>(fn_data);
  if (self != NULL) {
    if (ev == MG_EV_HTTP_MSG) {
      struct mg_http_message* hm = (struct mg_http_message*) ev_data;
      if (mg_http_match_uri(hm, "/websocket")) {
        char peer_ip[100];
        mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
        LOG_F(INFO, "Websocket connection opened from %s", peer_ip);
        mg_ws_upgrade(c, hm, NULL);
        WebsocketServer::websocket_client_t client;
        client.m_connection = c;
        client.m_peer_ip = std::string(peer_ip);
        client.m_peer_id = "None";
        self->m_websocket_clients.push_back(client);
      }
    }
    else if (ev == MG_EV_WS_MSG) {
      struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;
      char                  peer_ip[100];
      mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
      char* buff = (char*) malloc((wm->data.len + 1) * sizeof(char));
      if (buff != NULL) {
        for (size_t x = 0; x < wm->data.len; x++) {
          buff[x] = wm->data.ptr[x];
          buff[x + 1] = '\0';
        }
        LOG_F(INFO, "Recieved %s from %s", buff, peer_ip);
        for (size_t x = 0; x < self->m_websocket_clients.size(); x++) {
          if (std::string(peer_ip) == self->m_websocket_clients[x].m_peer_ip) {
            if (std::string(buff) != "")
              self->handleWebsocketMessage(self->m_websocket_clients[x],
                                           std::string(buff));
            break;
          }
        }
        free(buff);
      }
      else {
        LOG_F(ERROR, "Malloc failed to allocate memory for buffer!");
      }
      // mg_ws_send(c, wm->data.ptr, wm->data.len, WEBSOCKET_OP_TEXT);
      mg_iobuf_delete(&c->recv, c->recv.len);
    }
    else if (ev == MG_EV_CLOSE) {
      char peer_ip[100];
      mg_ntoa(&c->peer, peer_ip, sizeof(peer_ip));
      LOG_F(INFO, "%s closed websocket connection", peer_ip);
      for (size_t x = 0; x < self->m_websocket_clients.size(); x++) {
        if (std::string(peer_ip) == self->m_websocket_clients[x].m_peer_ip) {
          self->m_websocket_clients.erase(self->m_websocket_clients.begin() +
                                          x);
        }
      }
    }
  }
}
void WebsocketServer::send(std::string peer_id, std::string msg)
{
  for (size_t x = 0; x < m_websocket_clients.size(); x++) {
    if (m_websocket_clients[x].m_peer_id == peer_id) {
      mg_ws_send(m_websocket_clients[x].m_connection,
                 msg.c_str(),
                 msg.size(),
                 WEBSOCKET_OP_TEXT);
      break;
    }
  }
}
void WebsocketServer::send(websocket_client_t client, std::string msg)
{
  mg_ws_send(client.m_connection, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
}
void WebsocketServer::handleWebsocketMessage(websocket_client_t client,
                                             std::string        msg)
{
  LOG_F(INFO,
        "(WebsocketServer::HandleWebsocketMessage&%s) \"%s\"",
        client.m_peer_ip.c_str(),
        msg.c_str());
  try {
    nlohmann::json data = nlohmann::json::parse(msg.c_str());
    // Make sure id is updated for this peer
    for (size_t x = 0; x < m_websocket_clients.size(); x++) {
      if (m_websocket_clients[x].m_peer_ip == client.m_peer_ip) {
        if (m_websocket_clients[x].m_peer_id == "None") {
          std::string id = data["id"];
          LOG_F(INFO,
                "\tRegistered ip: %s with id: %s",
                client.m_peer_ip.c_str(),
                id.c_str());
          m_websocket_clients[x].m_peer_id = data["id"];
        }
      }
    }
    // Echo any message from any client to Admin, if Admin exists
    if (data["id"] != "Admin") {
      send("Admin", msg);
    }
    else // Message from Admin
    {
      if (data.contains("cmd")) {
        // cmd needs to be evaluated on the server and send results back to
        // Admin
        nlohmann::json cmd_results;
        cmd_results["cmd"] = data["cmd"];
        if (data["cmd"] == "list_clients") {
          LOG_F(
            INFO,
            "Evalutating command sent by Admin-> Sending connected clients");
          nlohmann::json c;
          for (size_t x = 0; x < m_websocket_clients.size(); x++) {
            nlohmann::json o;
            o["peer_id"] = m_websocket_clients[x].m_peer_id;
            o["peer_ip"] = m_websocket_clients[x].m_peer_ip;
            c.push_back(o);
          }
          cmd_results["results"] = c;
        }
        send("Admin", cmd_results.dump());
      }
      else {
        // Make sure messages recieved from Admin are sent to propper peer which
        // Admin specified
        send(data["peer_id"], msg);
      }
    }
  }
  catch (const std::exception& e) {
    LOG_F(INFO, "(WebsocketServer::HandleWebsocketMessage) %s", e.what());
  }
}
void WebsocketServer::init()
{
  const char* listen_on = "TODO";
  mg_mgr_init(&m_mgr);
  mg_http_listen(&m_mgr, listen_on, fn, this);
  LOG_F(INFO, "Starting websocket server on: %s", listen_on);
}
void WebsocketServer::poll() { mg_mgr_poll(&m_mgr, 1); }
void WebsocketServer::close() { mg_mgr_free(&m_mgr); }
