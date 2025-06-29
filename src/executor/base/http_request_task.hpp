#pragma once

#include "task.hpp"
#include "../core/http.hpp"
#include "../core/types.hpp"
#include "../core/router.hpp"

#include <iostream>
#include <unistd.h>
#include <memory>
#include <sys/socket.h>

namespace EXECUTOR {

    // HTTPRequestTask represents an inherited task which is responsible for
    // handling HTTP tasks on the threadpool (HTTP request parsing etc)
    class HTTPRequestTask : public Task {
    public:
        HTTPRequestTask(const CORE::Request& req, std::shared_ptr<CORE::ConnectionState>& conn, CORE::Router& rt)
            : request(req), connection(conn), router(rt) {}

        void execute(int worker_id) override {
            CORE::Response response;

            // Initialize response with defaults
            response.status_code = 500;
            response.status_text             = "Internal Server Error";
            response.headers["Content-Type"] = "text/plain";
            response.headers["Connection"]   = "close";
            response.headers["Server"]       = "CustomHTTPServer/1.0";

            try {
                if (!router.route(request, response)) {
                    response.status_code = 404;
                    response.status_text = "Not Found";
                    response.body = "404 - Page Not Found";
                }
            } catch (const std::exception& e) {
                // If anything blows up, send 500 and log it
                response.status_code = 500;
                response.status_text = "Internal Server Error";
                response.body = "Internal Server Error";
                response.headers["Content-Length"] = std::to_string(response.body.size());
                
                std::cerr << "Error processing request: " << e.what() << std::endl;
            };
            // Ship it back to the client
            send_response(response, worker_id);
        }
        
    private:
        CORE::Request request;
        std::shared_ptr<CORE::ConnectionState> connection;
        CORE::Router& router;

        // Push the response over the wire — chunked, partial-safe
        void send_response(const CORE::Response& response, int worker_id) {
            std::string response_str = response.str();
            
            ssize_t total_sent = 0;
            ssize_t response_size = response_str.size();
            
            while (total_sent < response_size) {
                ssize_t sent = send(connection->socket_fd, 
                                  response_str.c_str() + total_sent, 
                                  response_size - total_sent, 
                                  MSG_NOSIGNAL);
                
                if (sent == -1) {
                    // Socket buffer full — retry
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(1000);
                        continue;
                    } else {
                          // Fatal send failure
                        std::cerr << "Failed to send response on worker " << worker_id 
                                 << ": " << strerror(errno) << std::endl;
                        break;
                    }
                } else if (sent == 0) {
                    // Peer disconnected during send
                    std::cerr << "Connection closed by peer during response send" << std::endl;
                    break;
                } else {
                    total_sent += sent;
                }
            }
        }
    };

} // namespace EXECUTOR