#include "server/server.hpp"
#include <signal.h>
#include <iostream>

SERVER::Server* global_server = nullptr;

void signal_handler(int sig) {
    (void) sig;
    if (global_server)
        global_server->stop();
}

int main() {
    SERVER::Server server(8);
    global_server = &server;

    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    if (!server.start(8080)) {
        std::cerr << "Could not start server\n";
        return 1;
    }

    server.wait();
    std::cout << "Server shutdown complete.\n";
    return 0;
}
