#include "event_loop.hpp"
#include "../core/http_request_task.hpp"

#include <iostream>
#include <memory>
#include <sys/socket.h> // For the Berkeley sockets API
#include <unistd.h>     // For close
#include <fcntl.h>      // For file control shit
#include <arpa/inet.h>  // For sockaddr_in and stuff 
#include <errno.h>      // For errno checking error types


namespace REACTOR {

    int EventLoop::make_socket_nonblocking(int socket_fd) {
        // Retrieve the current flags on the socket file descriptor
        int flags = fcntl(socket_fd, F_GETFL, 0);

        // Setting the O_NONBLOCK flag on the socket while keeping 
        // existing flags intact.
        // This makes all future I/O operations on the socket non-blocking.
        // - read()/recv() will return immediately if there's no data rather than blocking
        // - wrtie()/send() will return immediately if the socket is not ready for writing
        // Usefull for us since we're doing event driven shit
        return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }

    EventLoop::EventLoop(EXECUTOR::ThreadPool& threadpool, CORE::Router& r) : thread_pool(&threadpool), router(r) {
        this->notifier = std::make_unique<EventNotifier>();
    }

    EventLoop::~EventLoop() {
        if (server_socket != -1)
            close(server_socket);
    }

    bool EventLoop::setup_server_socket(uint16_t port) {
        // AF_INET     - represents the IPv4 Address Family
        // SOCK_STREAM - represents the TCP socket type
        // 0           - represents the use of the default protocol 
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        
        if (server_socket == -1)
            return false;
        
        // Allows the socket to be quickly reused after it is closed, 
        // avoiding the "Address is already in use" error.
        // Useful during development when we restart our server often 
        // since it allows the program to rebind to the same port without
        // waiting for the OS to release it
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (make_socket_nonblocking(server_socket) == -1)
            return false;
        
        sockaddr_in addr {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);            // Hardware to network byte order conversion

        if (bind(server_socket, (sockaddr*)&addr, sizeof(addr)) == -1)
            return false;
        
        if (listen(server_socket, 128) == -1)
            return false;

        return notifier->add_fd(server_socket);
    }

    void EventLoop::run() {
        std::cout << "🚀 Event loop started!" << std::endl;
        while (!should_stop) {
            auto events = notifier->wait_for_events();
            for (const auto& event : events) {
                handle_event(event);
            }
        }
    }

    void EventLoop::stop() {
        // flip the flag - we out this bih
        should_stop.store(true);
    }

    void EventLoop::handle_event(const EventData& event) {
        if (event.fd == server_socket)
            handle_new_connections();
        else
            handle_client_event(event.fd, event.events);
    }

    void EventLoop::handle_new_connections() {
        // Accepting pending connections 
        for (;;) {
            sockaddr_in client_addr {};
            socklen_t client_len = sizeof(client_addr);

            // Try accepting a new client.
            // If there are no pending connections (EAGAIN/EWOULDBLOCK), 
            // break out — we're done for now.
            // If it's a real error, log it and bail out of the accept loop.
            int client_fd = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                    break;
                perror("accept failed");
                break;
            }
            
            make_socket_nonblocking(client_fd);
            notifier->add_fd(client_fd);

            // Make client ip something readable
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // Wrap client data into neat lil shared pointer
            auto conn = std::make_shared<CORE::ConnectionState>(client_fd, client_ip, client_port);

            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections[client_fd] = conn;
            }

            {
                // COUT IS NOT THREAD SAFE
                std::lock_guard<std::mutex> lock(cout_mutex);
                std::cout << "New Client " << client_ip << ":" << client_port << std::endl;
            }
        }
    }

    void EventLoop::handle_client_event(int fd, uint32_t events) {
        if (events & FLAG_READ) {
            std::shared_ptr<CORE::ConnectionState> conn;
            {   
                std::lock_guard<std::mutex> lock(connections_mutex);
                auto it = connections.find(fd);
                if (it == connections.end()) {
                    return;
                }
                conn = it->second;
            }
        
            constexpr size_t BUF_SIZE = 4096;
            char buffer[BUF_SIZE];
            bool should_disconnect = false;
            
            for (;;) {
                ssize_t n = recv(fd, buffer, BUF_SIZE, 0);
                if (n > 0) {
                    // Get or create parser for this connection
                    std::unique_ptr<CORE::HTTPParser>* parser_ptr = nullptr;
                    {
                        std::lock_guard<std::mutex> lock(parsers_mutex);
                        auto& parser = parsers[fd];
                        if (!parser) {
                            parser = std::make_unique<CORE::HTTPParser>();
                        }
                        parser_ptr = &parser;
                    }
                    
                    // Parse the incoming data
                    std::string data(buffer, n);
                    CORE::Request request;
                    
                    if ((*parser_ptr)->parse(data, request)) {
                        // Complete request received - process it
                        auto task = std::make_unique<CORE::HTTPRequestTask>(request, conn, router);
                        thread_pool->enqueue_task(std::move(task));
                        
                        // Reset parser for next request
                        (*parser_ptr)->reset();
                        
                        // Close connection after each request (HTTP/1.0 style)
                        should_disconnect = true;
                        break;
                    } else if ((*parser_ptr)->has_error()) {
                        // Parsing error - send 400 Bad Request
                        send_error_response(fd, 400, "Bad Request");
                        should_disconnect = true;
                        break;
                    }
                    
                } else if (n == 0) {
                    should_disconnect = true;
                    break;
                } else {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        break;
                    } else {
                        perror("recv");
                        should_disconnect = true;
                        break;
                    }
                }
            }
            
            if (should_disconnect) {
                handle_client_disconnect(fd);
                return;
            }
        }
    }

    void EventLoop::send_error_response(int fd, int status_code, const std::string& status_text) {
        CORE::Response response;
        response.status_code = status_code;
        response.status_text = status_text;
        response.headers["Content-Type"] = "text/plain";
        response.headers["Connection"] = "close";
        response.headers["Server"] = "CustomHTTPServer/1.0";
        response.body = std::to_string(status_code) + " " + status_text;
        response.headers["Content-Length"] = std::to_string(response.body.size());
        
        std::string response_str = response.str();
        send(fd, response_str.c_str(), response_str.size(), MSG_NOSIGNAL);
    }

    void REACTOR::EventLoop::handle_client_disconnect(int fd) {
        std::lock_guard<std::mutex> connections_lock(connections_mutex);
        std::lock_guard<std::mutex> parsers_lock(parsers_mutex);
        
        if (connections.find(fd) == connections.end()) {
            return;
        }

        notifier->remove_fd(fd);
        close(fd);
        connections.erase(fd);
        parsers.erase(fd);  // Clean up parser
        
        std::cout << "Client disconnected fd: " << fd << std::endl;
    }
} // namespace REACTOR