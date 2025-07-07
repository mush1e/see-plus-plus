#include "event_loop.hpp"
#include "../core/http_request_task.hpp"
#include "../core/logger.hpp"

#include <iostream>
#include <memory>
#include <sys/socket.h> // For the Berkeley sockets API
#include <unistd.h>     // For close
#include <fcntl.h>      // For file control shit
#include <arpa/inet.h>  // For sockaddr_in and stuff 
#include <errno.h>      // For errno checking error types
#include <thread>
#include <chrono>

namespace REACTOR {

    int EventLoop::make_socket_nonblocking(int socket_fd) {
        // Retrieve the current flags on the socket file descriptor
        int flags = fcntl(socket_fd, F_GETFL, 0);

        // Setting the O_NONBLOCK flag on the socket while keeping 
        // existing flags intact.
        // This makes all future I/O operations on the socket non-blocking.
        // - read()/recv() will return immediately if there's no data rather than blocking
        // - write()/send() will return immediately if the socket is not ready for writing
        // Useful for us since we're doing event driven shit
        return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }

    EventLoop::EventLoop(EXECUTOR::ThreadPool& threadpool, CORE::Router& r) 
        : thread_pool(&threadpool), router(r) {
        this->notifier = std::make_unique<EventNotifier>();
        
        // Start cleanup thread for connection management
        cleanup_thread = std::thread(&EventLoop::cleanup_worker, this);
        
        LOG_INFO("EventLoop initialized with connection manager and keep-alive support");
    }

    EventLoop::~EventLoop() {
        // Stop cleanup thread first
        cleanup_should_stop.store(true);
        if (cleanup_thread.joinable()) {
            cleanup_thread.join();
        }
        
        if (server_socket != -1) {
            close(server_socket);
        }
        
        LOG_INFO("EventLoop destroyed");
    }

    bool EventLoop::setup_server_socket(uint16_t port) {
        // AF_INET     - represents the IPv4 Address Family
        // SOCK_STREAM - represents the TCP socket type
        // 0           - represents the use of the default protocol 
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        
        if (server_socket == -1) {
            LOG_ERROR("Failed to create server socket:", strerror(errno));
            return false;
        }
        
        // Allows the socket to be quickly reused after it is closed, 
        // avoiding the "Address is already in use" error.
        // Useful during development when we restart our server often 
        // since it allows the program to rebind to the same port without
        // waiting for the OS to release it
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
            LOG_ERROR("Failed to set SO_REUSEADDR:", strerror(errno));
            close(server_socket);
            return false;
        }

        if (make_socket_nonblocking(server_socket) == -1) {
            LOG_ERROR("Failed to make server socket non-blocking:", strerror(errno));
            close(server_socket);
            return false;
        }
        
        sockaddr_in addr {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);            // Hardware to network byte order conversion

        if (bind(server_socket, (sockaddr*)&addr, sizeof(addr)) == -1) {
            LOG_ERROR("Failed to bind to port", port, ":", strerror(errno));
            close(server_socket);
            return false;
        }
        
        if (listen(server_socket, 128) == -1) {
            LOG_ERROR("Failed to listen on socket:", strerror(errno));
            close(server_socket);
            return false;
        }

        if (!notifier->add_fd(server_socket)) {
            LOG_ERROR("Failed to add server socket to event notifier");
            close(server_socket);
            return false;
        }

        LOG_INFO("Server socket setup successfully on port", port);
        return true;
    }

    void EventLoop::run() {
        LOG_INFO("🚀 Event loop started! Keep-alive:", 
                (keep_alive_enabled.load() ? "enabled" : "disabled"));
        while (!should_stop.load()) {
            auto events = notifier->wait_for_events(1000); // 1 second timeout
            for (const auto& event : events) {
                handle_event(event);
            }
        }
        LOG_INFO("Event loop stopped");
    }

    void EventLoop::stop() {
        LOG_INFO("Stopping event loop...");
        should_stop.store(true);
    }

    void EventLoop::handle_event(const EventData& event) {
        if (event.fd == server_socket) {
            handle_new_connections();
        } else {
            handle_client_event(event.fd, event.events);
        }
    }

    void EventLoop::handle_new_connections() {
        // Accept all pending connections
        for (;;) {
            sockaddr_in client_addr {};
            socklen_t client_len = sizeof(client_addr);

            // Try accepting a new client.
            // If there are no pending connections (EAGAIN/EWOULDBLOCK), 
            // break out — we're done for now.
            // If it's a real error, log it and bail out of the accept loop.
            int client_fd = accept(server_socket, (sockaddr*)&client_addr, &client_len);
            if (client_fd == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    break; // No more pending connections
                }
                LOG_ERROR("accept failed:", strerror(errno));
                break;
            }
            
            // Make client socket non-blocking
            if (make_socket_nonblocking(client_fd) == -1) {
                LOG_ERROR("Failed to make client socket non-blocking:", strerror(errno));
                close(client_fd);
                continue;
            }

            // Add to event notifier
            if (!notifier->add_fd(client_fd)) {
                LOG_ERROR("Failed to add client socket to event notifier");
                close(client_fd);
                continue;
            }

            // Extract client info
            char client_ip[INET_ADDRSTRLEN];
            inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
            uint16_t client_port = ntohs(client_addr.sin_port);

            // Add to connection manager
            if (!connection_manager.add_connection(client_fd, client_ip, client_port)) {
                LOG_WARN("Connection limit reached, rejecting client", client_ip, ":", client_port);
                close(client_fd);
                continue;
            }

            LOG_DEBUG("New client connected:", client_ip, ":", client_port, 
                     "(fd:", client_fd, ", total connections:", connection_manager.connection_count(), ")");
        }
    }

    void EventLoop::handle_client_event(int fd, uint32_t events) {
        if (events & FLAG_READ) {
            // Use thread-safe connection handle
            auto conn_handle = connection_manager.get_connection_handle(fd);
            if (!conn_handle.is_valid()) {
                LOG_WARN("Received event for invalid connection fd:", fd);
                return;
            }

            // Now we can safely access connection and parser
            auto conn = conn_handle.connection();
            auto parser = conn_handle.parser();
        
            constexpr size_t BUF_SIZE = 4096;
            char buffer[BUF_SIZE];
            bool should_disconnect = false;
            
            // Read all available data
            for (;;) {
                ssize_t n = recv(fd, buffer, BUF_SIZE, 0);
                if (n > 0) {
                    // Check request size limit - this modifies connection data
                    if (!connection_manager.check_request_size_limit(fd, n)) {
                        LOG_WARN("Request size limit exceeded for fd:", fd);
                        send_error_response(fd, 413, "Request Entity Too Large");
                        should_disconnect = true;
                        break;
                    }

                    // Update last activity - safe because we hold the handle
                    conn->last_activity = std::chrono::steady_clock::now();
                    
                    // Parse the incoming data
                    std::string data(buffer, n);
                    CORE::Request request;
                    
                    if (parser->parse(data, request)) {
                        // Complete request received - process it
                        LOG_DEBUG("Complete HTTP request received from fd:", fd, 
                                request.method, request.path);
                        
                        // Pass keep-alive setting to task
                        auto task = std::make_unique<CORE::HTTPRequestTask>(
                            request, conn, router, keep_alive_enabled.load()
                        );
                        thread_pool->enqueue_task(std::move(task));
                        
                        // Reset parser but DON'T disconnect for keep-alive
                        // The task will decide whether to close the connection
                        connection_manager.reset_parser(fd);
                        
                        // For keep-alive, we continue listening on this fd
                        // The connection stays in the event loop!
                        if (keep_alive_enabled.load()) {
                            LOG_DEBUG("Request processed, keeping connection alive for fd:", fd);
                        }
                        return;
                        
                    } else if (parser->has_error()) {
                        // Parsing error - send 400 Bad Request
                        LOG_WARN("HTTP parsing error for fd:", fd, "-", parser->get_error_description());
                        send_error_response(fd, 400, "Bad Request");
                        should_disconnect = true;
                        break;
                    }
                    // else: partial request, continue reading
                    
                } else if (n == 0) {
                    // Client closed connection gracefully
                    LOG_DEBUG("Client closed connection fd:", fd);
                    should_disconnect = true;
                    break;
                } else {
                    // recv error
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No more data available right now - this is normal for non-blocking sockets
                        break;
                    } else {
                        LOG_ERROR("recv error for fd:", fd, "-", strerror(errno));
                        should_disconnect = true;
                        break;
                    }
                }
            }
            
            if (should_disconnect) {
                handle_client_disconnect(fd);
            }
        }
        
        // Handle other event types (error, hangup, etc.)
        if (events & (FLAG_ERROR | FLAG_DISCONNECT)) {
            LOG_DEBUG("Client error/disconnect event for fd:", fd);
            handle_client_disconnect(fd);
        }
    }

    void EventLoop::close_connection(int fd) {
        // This gets called by worker threads when they want to close a connection
        // (either because keep-alive is disabled or there was an error)
        LOG_DEBUG("Explicit connection close requested for fd:", fd);
        handle_client_disconnect(fd);
    }

    void EventLoop::send_error_response(int fd, int status_code, const std::string& status_text) {
        CORE::Response response;
        response.status_code = status_code;
        response.status_text = status_text;
        response.headers["Content-Type"] = "text/html";
        response.headers["Connection"] = "close";
        response.headers["Server"] = "see-plus-plus/1.0";
        
        // Create a proper HTML error page
        response.body = R"(<!DOCTYPE html>
<html>
<head><title>)" + std::to_string(status_code) + " " + status_text + R"(</title></head>
<body>
    <h1>)" + std::to_string(status_code) + " " + status_text + R"(</h1>
    <p>The server encountered an error processing your request.</p>
    <hr>
    <small>see-plus-plus/1.0</small>
</body>
</html>)";
        
        response.headers["Content-Length"] = std::to_string(response.body.size());
        
        std::string response_str = response.str();
        ssize_t sent = send(fd, response_str.c_str(), response_str.size(), MSG_NOSIGNAL);
        
        if (sent == -1) {
            LOG_ERROR("Failed to send error response to fd:", fd, "-", strerror(errno));
        }
    }

    void EventLoop::handle_client_disconnect(int fd) {
        // Use thread-safe connection handle to check if connection exists
        auto conn_handle = connection_manager.get_connection_handle(fd);
        if (!conn_handle.is_valid()) {
            return; // Already disconnected
        }

        auto conn = conn_handle.connection();
        LOG_DEBUG("Disconnecting client fd:", fd, 
                 "(", conn->client_ip, ":", conn->client_port, ")");

        // Remove from event notifier
        notifier->remove_fd(fd);
        
        // Close socket
        close(fd);
        
        // Remove from connection manager (this cleans up parser too)
        connection_manager.remove_connection(fd);
        
        LOG_DEBUG("Client disconnected, remaining connections:", 
                 connection_manager.connection_count());
    }

    void EventLoop::cleanup_worker() {
        LOG_DEBUG("Cleanup worker thread started");
        
        while (!cleanup_should_stop.load()) {
            // Sleep for 30 seconds between cleanup cycles
            std::this_thread::sleep_for(std::chrono::seconds(30));
            
            if (!cleanup_should_stop.load()) {
                cleanup_timed_out_connections();
            }
        }
        
        LOG_DEBUG("Cleanup worker thread stopped");
    }

    void EventLoop::cleanup_timed_out_connections() {
        auto timed_out = connection_manager.get_timed_out_connections();
        
        if (!timed_out.empty()) {
            LOG_INFO("Cleaning up", timed_out.size(), "timed out connections");
            
            for (int fd : timed_out) {
                LOG_DEBUG("Cleaning up timed out connection fd:", fd);
                handle_client_disconnect(fd);
            }
        }
    }

} // namespace REACTOR