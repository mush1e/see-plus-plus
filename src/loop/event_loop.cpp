#include "event_loop.hpp"
#include "../threadpool/http_request_task.hpp"
#include "../threadpool/websocket_handshake_task.hpp"

#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <iostream>



namespace LOOP {

EventLoop::EventLoop(THREADPOOL::ThreadPool* thread_pool) : thread_pool_(thread_pool) {
    notifier_ = std::make_unique<EVENT::EventNotifier>();
}

EventLoop::~EventLoop() {
    if (server_socket_ != -1)
        close(server_socket_);
}

bool EventLoop::setup_server_socket(uint16_t port) {
    server_socket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket_ == -1)
        return false;

    int opt = 1;
    setsockopt(server_socket_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    if (make_socket_nonblocking(server_socket_) == -1)
        return false;

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(server_socket_, (sockaddr*)&addr, sizeof(addr)) == -1)
        return false;

    if (listen(server_socket_, 128) == -1)
        return false;

    return notifier_->add_fd(server_socket_);
}

void EventLoop::run() {
    std::cout << "🚀 Event loop started!" << std::endl;
    while (!should_stop_) {
        auto events = notifier_->wait_for_events(1000);
        for (const auto& event : events) {
            handle_event(event);
        }
    }
}

void EventLoop::stop() {
    should_stop_ = true;
}

void EventLoop::handle_event(const EVENT::EventData& event) {
    if (event.fd == server_socket_)
        handle_new_connections();
    else
        handle_client_event(event.fd, event.events);
}

void EventLoop::handle_new_connections() {
    while (true) {
        sockaddr_in client_addr{};
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_socket_, (sockaddr*)&client_addr, &client_len);
        if (client_fd == -1)
            break;

        make_socket_nonblocking(client_fd);
        notifier_->add_fd(client_fd);

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        uint16_t client_port = ntohs(client_addr.sin_port);

        auto conn = std::make_shared<CORE::ConnectionState>(client_fd, client_ip, client_port);

        {
            std::lock_guard<std::mutex> lock(connections_mutex_);
            connections_[client_fd] = conn;
        }

        std::cout << "New client: " << client_ip << ":" << client_port << std::endl;
    }
}

void EventLoop::handle_client_event(int fd, uint32_t events) {
    if (events & FLAG_READ) { // Read available
        constexpr size_t BUF_SIZE = 4096;
        char buffer[BUF_SIZE];
        for (;;) {
            ssize_t n = recv(fd, buffer, BUF_SIZE, 0);
            if (n > 0) {
                auto conn = this->connections_[fd];
                conn->http_buffer.append(buffer, n);
            } else if (n == 0) {
                handle_client_disconnect(fd);
                return;
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break;
                } else {
                    perror("recv");
                    handle_client_disconnect(fd);
                    return;
                }
            }
        }
        auto& buf = this->connections_[fd]->http_buffer;
        auto header_end = buf.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            // Extract the raw header block
            std::string raw = buf.substr(0, header_end + 4);

            // Sniff for WebSocket upgrade
            bool is_ws =
                raw.find("Upgrade: websocket") != std::string::npos &&
                raw.find("Connection: Upgrade") != std::string::npos;

            if (is_ws) {
                this->thread_pool_->enqueue_task(
                    std::make_unique<THREADPOOL::WebSocketHandshakeTask>(
                        this->connections_[fd], raw));
            } else {
                this->thread_pool_->enqueue_task(
                    std::make_unique<THREADPOOL::HTTPRequestTask>(
                        this->connections_[fd], raw));
            }

            // Erase consumed bytes
            buf.erase(0, header_end + 4);
        }
    }
    if (events & (FLAG_DISCONNECT | FLAG_ERROR)) // Disconnect or error
        handle_client_disconnect(fd);
}

void EventLoop::handle_client_disconnect(int fd) {
    std::lock_guard<std::mutex> lock(connections_mutex_);
    if (connections_.find(fd) == connections_.end()) {
        // already handled, skip.
        return;
    }

    notifier_->remove_fd(fd);
    close(fd);
    connections_.erase(fd);
    
    std::cout << "Client disconnected fd: " << fd << std::endl;
}


int EventLoop::make_socket_nonblocking(int socket_fd) {
    int flags = fcntl(socket_fd, F_GETFL, 0);
    return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
}

} // namespace LOOP
