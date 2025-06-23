#pragma once

#include <string>
#include <chrono>

namespace CORE {

    // Protocol types supported server
    enum class Protocol { 
        HTTP, 
        WEBSOCKET 
    };

    // Tracks state per connection
    struct ConnectionState {
        int socket_fd;                                       // Socket descriptor
        std::string client_ip;                               // Client's IP address
        uint16_t client_port;                                // Client's port
        Protocol protocol = Protocol::HTTP;                  // Connection protocol (HTTP/WS)
        std::chrono::steady_clock::time_point last_activity; // Timestamp for activity tracking
        std::string http_buffer;                             // Stores partial HTTP requests
        bool http_headers_complete = false;                  // Flag indicating if HTTP headers were fully read
        bool websocket_handshake_complete = false;           // WebSocket handshake status

        ConnectionState(int fd, const std::string& ip, uint16_t port)
            : socket_fd(fd), client_ip(ip), client_port(port), 
              last_activity(std::chrono::steady_clock::now()) {}
    };

} // namespace CORE