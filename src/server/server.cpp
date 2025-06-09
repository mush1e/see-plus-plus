#include "server.hpp"
#include <iostream>

namespace SERVER {

Server::Server(int num_workers, CORE::Router &router)
    : thread_pool_(num_workers), event_loop_(&thread_pool_, router), router_(router){}

Server::~Server() {
    stop();
}

bool Server::start(uint16_t port) {
    if (!event_loop_.setup_server_socket(port)) {
        std::cerr << "Failed to setup server socket\n";
        return false;
    }

    event_thread_ = std::thread([this] {
        event_loop_.run();
    });

    std::cout << "Server started on port " << port << "\n";
    return true;
}

void Server::stop() {
    event_loop_.stop();
}

void Server::wait() {
    if (event_thread_.joinable())
        event_thread_.join();
}

} // namespace SERVER
