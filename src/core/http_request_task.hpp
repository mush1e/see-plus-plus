#pragma once

#include "../executor/base/task.hpp"
#include "http.hpp"
#include "router.hpp"
#include "types.hpp"
#include <memory>
#include <sys/socket.h>
#include <iostream>
#include <unistd.h>

namespace CORE {

    class HTTPRequestTask : public EXECUTOR::Task {
    public:
        HTTPRequestTask(const Request& req, std::shared_ptr<ConnectionState> conn, 
                       Router& router, bool keep_alive_enabled = false)
            : request(req), connection(conn), router_ref(router), 
              keep_alive_enabled(keep_alive_enabled) {}

        void execute(int worker_id) override {
            Response response;
            
            // Initialize response with defaults
            response.status_code = 500;
            response.status_text = "Internal Server Error";
            response.headers["Content-Type"] = "text/plain";
            response.headers["Server"] = "see-plus-plus/1.0";
            
            // Determine if we should keep connection alive
            bool should_keep_alive = determine_keep_alive();
            response.headers["Connection"] = should_keep_alive ? "keep-alive" : "close";
            
            try {
                // Try to route the request
                if (!router_ref.route(request, response)) {
                    response.status_code = 404;
                    response.status_text = "Not Found";
                    response.body = generate_404_page();
                }
                
                response.headers["Content-Length"] = std::to_string(response.body.size());
                
            } catch (const std::exception& e) {
                response.status_code = 500;
                response.status_text = "Internal Server Error";
                response.body = "Internal Server Error";
                response.headers["Content-Length"] = std::to_string(response.body.size());
                
                std::cerr << "Error processing request: " << e.what() << std::endl;
                should_keep_alive = false; // Close on error
            }
            
            send_response(response, should_keep_alive, worker_id);
        }

    private:
        Request request;
        std::shared_ptr<ConnectionState> connection;
        Router& router_ref;
        bool keep_alive_enabled;
        
        bool determine_keep_alive() {
            // Server must support keep-alive
            if (!keep_alive_enabled) {
                return false;
            }
            
            // Check HTTP version - only HTTP/1.1 has keep-alive by default
            if (request.version == "HTTP/1.1") {
                // In HTTP/1.1, keep-alive is default unless client says "Connection: close"
                auto conn_header = request.headers.find("connection");
                if (conn_header != request.headers.end()) {
                    // Convert to lowercase for comparison
                    std::string conn_value = conn_header->second;
                    std::transform(conn_value.begin(), conn_value.end(), 
                                 conn_value.begin(), ::tolower);
                    return conn_value != "close";
                }
                return true; // Default for HTTP/1.1
            } else {
                // HTTP/1.0 - keep-alive only if explicitly requested
                auto conn_header = request.headers.find("connection");
                if (conn_header != request.headers.end()) {
                    std::string conn_value = conn_header->second;
                    std::transform(conn_value.begin(), conn_value.end(), 
                                 conn_value.begin(), ::tolower);
                    return conn_value == "keep-alive";
                }
                return false; // Default for HTTP/1.0
            }
        }
        
        void send_response(const Response& response, bool keep_alive, int worker_id) {
            std::string response_str = response.str();
            
            ssize_t total_sent = 0;
            ssize_t response_size = response_str.size();
            
            // Send the response
            while (total_sent < response_size) {
                ssize_t sent = send(connection->socket_fd, 
                                response_str.c_str() + total_sent, 
                                response_size - total_sent, 
                                MSG_NOSIGNAL);
                
                if (sent == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(1000); // Brief pause for non-blocking socket
                        continue;
                    } else {
                        std::cerr << "Failed to send response on worker " << worker_id 
                                << ": " << strerror(errno) << std::endl;
                        keep_alive = false; // Force close on send error
                        break;
                    }
                } else if (sent == 0) {
                    std::cerr << "Connection closed by peer during response send" << std::endl;
                    keep_alive = false;
                    break;
                } else {
                    total_sent += sent;
                }
            }
            
            // Only close if we're not keeping alive!
            if (!keep_alive) {
                std::cout << "Closing connection fd: " << connection->socket_fd 
                         << " (keep-alive disabled)" << std::endl;
                close(connection->socket_fd);
            } else {
                std::cout << "Keeping connection fd: " << connection->socket_fd 
                         << " alive for next request" << std::endl;
                // Update last activity time for timeout management
                connection->last_activity = std::chrono::steady_clock::now();
            }
        }
        
        std::string generate_404_page() {
            return R"(<!DOCTYPE html>
<html>
<head><title>404 Not Found</title></head>
<body>
    <h1>404 - Page Not Found</h1>
    <p>The requested resource was not found on this server.</p>
    <p>Request: )" + request.method + " " + request.path + R"(</p>
</body>
</html>)";
        }
    };

} // namespace CORE