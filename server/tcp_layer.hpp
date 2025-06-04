#pragma once

#include <iostream>
#include <vector>
#include <mutex>
#include <thread>
#include <chrono>        // For connection timeouts to prevent zombie connections
#include <atomic>        // For threadsafe atomic operations
#include <signal.h>      // For signals, graceful shutdown
#include <sys/socket.h>  // For socket functions
#include <netinet/in.h>  // For sockaddr_in structure
#include <unistd.h>      // For close() function
#include <string.h>      // For memset
#include <arpa/inet.h>   // For inet_addr

#define MAX_CLIENTS 10

namespace TRANSPORT {
    class Server {
    public:
        Server();
        ~Server();

        bool create_socket();
        bool bind_socket(uint16_t port = 8080);
        bool start_listening(int max_conns = 5);
        void run_server();


        void setup_signal_handlers();
        void handle_client();
        void handle_client_threaded(int client_sock, sockaddr_in client_addr);
        void await_all();
        void cleanup();


    private:
        int server_socket = -1;

        std::atomic<int> active_clients {0};
        sockaddr_in server_addr{};

        std::vector<std::thread> client_threads {};
        std::mutex cout_mtx {};

        static Server* instance;
        static void signal_handler(int signal);
    };
}
