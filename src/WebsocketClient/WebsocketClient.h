#ifndef WEBSOCKET_CLIENT_
#define WEBSOCKET_CLIENT_

#include <application.h>
#include "../mongoose/mongoose.h"

class WebsocketClient
{
    public:
        bool isConnected;
        bool isWaitingForResponse;
        unsigned long responseTimer;
        nlohmann::json lastRecievedPacket;

        std::string id;
        static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
        static bool ReconnectTimer(void *self_pointer);

        struct mg_mgr mgr;
        struct mg_connection *client;

        std::string GetRandomString(size_t length);
        void SetIdAuto();
        void SetId(std::string id_);
        void Send(nlohmann::json packet);
        void Send(std::string msg);
        nlohmann::json SendPacketAndPollResponse(nlohmann::json packet);
        void HandleWebsocketMessage(std::string msg);
        void Connect();
        void Init();
        void Poll();
        void Close();
};

#endif //WEBSOCKET_CLIENT_