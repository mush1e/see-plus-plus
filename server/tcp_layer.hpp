#pragma once

#include <iostream>
#include <signal.h>      // For signals, graceful shutdown
#include <sys/socket.h>  // For socket functions
#include <netinet/in.h>  // For sockaddr_in structure
#include <unistd.h>      // For close() function
#include <string.h>      // For memset
#include <arpa/inet.h>   // For inet_addr

namespace net_layer {
    class Server {
    public:
        Server();
        ~Server();

        bool create_socket();
        bool bind_socket(uint16_t port = 8080);
        bool start_listening(int max_conns = 5);
        bool accept_connection();
        
        void setup_signal_handlers();
        void handle_client();
        void cleanup();

    private:
        int server_socket = -1;
        int client_socket = -1;
        sockaddr_in server_addr{};
        sockaddr_in client_addr{};
        socklen_t client_addr_len = sizeof(client_addr);

        static Server* instance;
        static void signal_handler(int signal);
    };
}
