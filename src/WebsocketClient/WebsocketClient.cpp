#define MG_ENABLE_LOG 0
#include "WebsocketClient.h"
#include "EasyRender/logging/loguru.h"

bool WebsocketClient::ReconnectTimer(void *self_pointer)
{
    WebsocketClient *self = reinterpret_cast<WebsocketClient *>(self_pointer);
    if (self != NULL)
    {
        if (self->isConnected == true)
        {
            LOG_F(INFO, "Canceled reconnect timer!");
            return false;
        }
        self->Connect();
        return false;
    }
    return false;
}
void WebsocketClient::fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data)
{
    WebsocketClient *self = reinterpret_cast<WebsocketClient *>(fn_data);
    if (self != NULL)
    {
        if (ev == MG_EV_ERROR)
        {
            //LOG_F(ERROR, "%p %s", c->fd, (char *) ev_data);
        }
        else if (ev == MG_EV_WS_OPEN)
        {
            LOG_F(INFO, "(WebsocketClient::fn) Connection to server successful, registering ID!");
            self->isConnected = true;
            nlohmann::json packet;
            packet["id"] = self->id;
            self->Send(packet);
        }
        else if (ev == MG_EV_WS_MSG)
        {
            struct mg_ws_message *wm = (struct mg_ws_message *) ev_data;
            char *buff = (char*)malloc((wm->data.len + 1) * sizeof(char));
            if (buff != NULL)
            {
                for (size_t x = 0; x < wm->data.len; x++)
                {
                    buff[x] = wm->data.ptr[x];
                    buff[x+1] = '\0';
                }
                self->HandleWebsocketMessage(buff);
                free(buff);
            }
            else
            {
                LOG_F(ERROR, "(WebsocketClient::fn) Could not allocate memory for buffer!");
            }
        }
        if (ev == MG_EV_CLOSE)
        {
            self->Close();
            LOG_F(INFO, "(WebsocketClient::fn) Connection to server closed!");
            globals->renderer->PushTimer(10 * 1000, self->ReconnectTimer, self);
        }
    }
}
void WebsocketClient::Send(nlohmann::json packet)
{
    this->Send(packet.dump());
}
void WebsocketClient::Send(std::string msg)
{
    LOG_F(INFO, "Sending: %s", msg.c_str());
    mg_ws_send(this->client, msg.c_str(), msg.size(), WEBSOCKET_OP_TEXT);
}
nlohmann::json WebsocketClient::SendPacketAndPollResponse(nlohmann::json packet)
{
    this->isWaitingForResponse = true;
    packet["id"] = this->id;
    this->Send(packet);
    this->responseTimer = globals->renderer->Millis();
    while(this->isWaitingForResponse == true)
    {
        this->Poll();
        if ((globals->renderer->Millis() - this->responseTimer) > 5 * 1000)
        {
            LOG_F(ERROR, "(WebsocketClient::SendPacketAndPollResponse) Timed out!");
            this->isWaitingForResponse = false;
        }
    }
    return this->lastRecievedPacket;
}
void WebsocketClient::HandleWebsocketMessage(std::string msg)
{
    char peer_ip[100];
    mg_ntoa(&this->client->peer, peer_ip, sizeof(peer_ip));
    LOG_F(INFO, "(WebsocketClient::HandleWebsocketMessage&%s) \"%s\"", peer_ip, msg.c_str());
    try
    {
        nlohmann::json data = nlohmann::json::parse(msg.c_str());
        this->lastRecievedPacket = data;
        this->isWaitingForResponse = false;

        if (this->lastRecievedPacket.contains("peer_cmd"))
        {
            this->Send({
                {"id", this->id},
                {"current_view", globals->renderer->GetCurrentView()},
            });
        }
    }
    catch(const std::exception& e)
    {
        LOG_F(INFO, "(WebsocketClient::HandleWebsocketMessage) %s", e.what());
    }
}
void WebsocketClient::SetId(std::string id_)
{
    this->id = id_;
}
std::string WebsocketClient::GetRandomString(size_t length)
{
    auto randchar = []() -> char
    {
        const char charset[] =
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz";
        const size_t max_index = (sizeof(charset) - 1);
        return charset[ rand() % max_index ];
    };
    std::string str(length,0);
    std::generate_n( str.begin(), length, randchar );
    return str;
}
void WebsocketClient::SetIdAuto()
{
    this->SetId(globals->renderer->GetEvironmentVariable("USER") + "-" + this->GetRandomString(4));
}
void WebsocketClient::Init()
{
    this->isWaitingForResponse = false;
    this->isConnected = false;
    this->Connect();
}
void WebsocketClient::Connect()
{
    mg_mgr_init(&this->mgr);
    static const char *s_url = "ws://jetcad.io:8000/websocket";
    this->client = mg_ws_connect(&this->mgr, s_url, this->fn, this, NULL);
}
void WebsocketClient::Poll()
{
    mg_mgr_poll(&this->mgr, 1);
}
void WebsocketClient::Close()
{
    this->isConnected = false;
    mg_mgr_free(&this->mgr);
}