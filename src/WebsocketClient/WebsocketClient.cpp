#define MG_ENABLE_LOG 0
#include "WebsocketClient.h"
#include "NcRender/NcRender.h"
#include "NcRender/logging/loguru.h"

bool WebsocketClient::reconnectTimer(void* self_pointer)
{
  WebsocketClient* self = reinterpret_cast<WebsocketClient*>(self_pointer);
  if (self != NULL) {
    if (self->m_is_connected == true) {
      LOG_F(INFO, "Canceled reconnect timer!");
      return false;
    }
    self->connect();
    return false;
  }
  return false;
}
void WebsocketClient::fn(struct mg_connection* c,
                         int                   ev,
                         void*                 ev_data,
                         void*                 fn_data)
{
  WebsocketClient* self = reinterpret_cast<WebsocketClient*>(fn_data);
  if (self != NULL) {
    if (ev == MG_EV_ERROR) {
      // LOG_F(ERROR, "%p %s", c->fd, (char *) ev_data);
    }
    else if (ev == MG_EV_WS_OPEN) {
      LOG_F(INFO,
            "(WebsocketClient::fn) Connection to server successful, "
            "registering ID!");
      self->m_is_connected = true;
      nlohmann::json packet;
      packet["id"] = self->m_id;
      self->send(packet);
    }
    else if (ev == MG_EV_WS_MSG) {
      struct mg_ws_message* wm = (struct mg_ws_message*) ev_data;
      char* buff = (char*) malloc((wm->data.len + 1) * sizeof(char));
      if (buff != NULL) {
        for (size_t x = 0; x < wm->data.len; x++) {
          buff[x] = wm->data.ptr[x];
          buff[x + 1] = '\0';
        }
        self->handleWebsocketMessage(buff);
        free(buff);
      }
      else {
        LOG_F(ERROR,
              "(WebsocketClient::fn) Could not allocate memory for buffer!");
      }
    }
    if (ev == MG_EV_CLOSE) {
      self->close();
      LOG_F(INFO, "(WebsocketClient::fn) Connection to server closed!");
      if (self->m_renderer) {
        self->m_renderer->pushTimer(
          10 * 1000, [self]() { return self->reconnectTimer(self); });
      }
    }
  }
}
void WebsocketClient::send(nlohmann::json packet) { send(packet.dump()); }
void WebsocketClient::send(std::string msg)
{
  LOG_F(INFO, "Sending: %s", msg.c_str());
  mg_ws_send(m_client, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
}
nlohmann::json WebsocketClient::sendPacketAndPollResponse(nlohmann::json packet)
{
  m_is_waiting_for_response = true;
  packet["id"] = m_id;
  send(packet);
  m_response_timer = m_renderer ? m_renderer->millis() : 0;
  while (m_is_waiting_for_response == true) {
    poll();
    if (m_renderer && ((m_renderer->millis() - m_response_timer) > 5 * 1000)) {
      LOG_F(ERROR, "(WebsocketClient::SendPacketAndPollResponse) Timed out!");
      m_is_waiting_for_response = false;
    }
  }
  return m_last_received_packet;
}
void WebsocketClient::handleWebsocketMessage(std::string msg)
{
  char peer_ip[100];
  mg_ntoa(&m_client->peer, peer_ip, sizeof(peer_ip));
  LOG_F(INFO,
        "(WebsocketClient::HandleWebsocketMessage&%s) \"%s\"",
        peer_ip,
        msg.c_str());
  try {
    nlohmann::json data = nlohmann::json::parse(msg.c_str());
    m_last_received_packet = data;
    m_is_waiting_for_response = false;

    if (m_last_received_packet.contains("peer_cmd")) {
      nlohmann::json response;
      response["id"] = m_id;
      if (m_renderer) {
        response["current_view"] = m_renderer->getCurrentView();
      }
      send(response);
    }
  }
  catch (const std::exception& e) {
    LOG_F(INFO, "(WebsocketClient::HandleWebsocketMessage) %s", e.what());
  }
}
void        WebsocketClient::setId(std::string id_) { m_id = id_; }
std::string WebsocketClient::getRandomString(size_t length)
{
  auto randchar = []() -> char {
    const char   charset[] = "0123456789"
                             "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                             "abcdefghijklmnopqrstuvwxyz";
    const size_t max_index = (sizeof(charset) - 1);
    return charset[rand() % max_index];
  };
  std::string str(length, 0);
  std::generate_n(str.begin(), length, randchar);
  return str;
}
void WebsocketClient::setIdAuto()
{
  std::string user =
    m_renderer ? m_renderer->getEnvironmentVariable("USER") : "unknown";
  setId(user + "-" + getRandomString(4));
}
void WebsocketClient::init()
{
  m_is_waiting_for_response = false;
  m_is_connected = false;
  connect();
}
void WebsocketClient::connect()
{
  mg_mgr_init(&m_mgr);
  static const char* s_url = "ws://jetcad.io:8000/websocket";
  m_client = mg_ws_connect(&m_mgr, s_url, fn, this, NULL);
}
void WebsocketClient::poll() { mg_mgr_poll(&m_mgr, 1); }
void WebsocketClient::close()
{
  m_is_connected = false;
  mg_mgr_free(&m_mgr);
}
