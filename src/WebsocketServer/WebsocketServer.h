#ifndef WEBSOCKET_SERVER_
#define WEBSOCKET_SERVER_

#include <application.h>
#include "../mongoose/mongoose.h"

class WebsocketServer
{
    public:
        static void fn(struct mg_connection *c, int ev, void *ev_data, void *fn_data);
        struct websocket_client_t{
            std::string peer_ip;
            std::string peer_id;
            mg_connection *connection;
        };
        std::vector<websocket_client_t> websocket_clients;
        struct mg_mgr mgr;
        void Send(std::string peer_id, std::string msg);
        void Send(websocket_client_t client, std::string msg);
        void HandleWebsocketMessage(websocket_client_t client, std::string msg);
        void Init();
        void Poll();
        void Close();
};

#endif //WEBSOCKET_SERVER_