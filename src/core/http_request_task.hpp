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
        HTTPRequestTask(const Request& req, std::shared_ptr<ConnectionState> conn, Router& router)
            : request(req), connection(conn), router_ref(router) {}

        void execute(int worker_id) override {
            Response response;
            
            // Initialize response with defaults
            response.status_code = 500;
            response.status_text = "Internal Server Error";
            response.headers["Content-Type"] = "text/plain";
            response.headers["Connection"] = "close";
            response.headers["Server"] = "CustomHTTPServer/1.0";
            
            try {
                // Try to route the request
                if (!router_ref.route(request, response)) {
                    response.status_code = 404;
                    response.status_text = "Not Found";
                    response.body = "404 - Page Not Found";
                }
                
                response.headers["Content-Length"] = std::to_string(response.body.size());
                
            } catch (const std::exception& e) {
                response.status_code = 500;
                response.status_text = "Internal Server Error";
                response.body = "Internal Server Error";
                response.headers["Content-Length"] = std::to_string(response.body.size());
                
                std::cerr << "Error processing request: " << e.what() << std::endl;
            }
            
            send_response(response, worker_id);
        }

    private:
        Request request;
        std::shared_ptr<ConnectionState> connection;
        Router& router_ref;
        
        void send_response(const Response& response, int worker_id) {
            std::string response_str = response.str();
            
            ssize_t total_sent = 0;
            ssize_t response_size = response_str.size();
            
            while (total_sent < response_size) {
                ssize_t sent = send(connection->socket_fd, 
                                response_str.c_str() + total_sent, 
                                response_size - total_sent, 
                                MSG_NOSIGNAL);
                
                if (sent == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        usleep(1000);
                        continue;
                    } else {
                        std::cerr << "Failed to send response on worker " << worker_id 
                                << ": " << strerror(errno) << std::endl;
                        break;
                    }
                } else if (sent == 0) {
                    std::cerr << "Connection closed by peer during response send" << std::endl;
                    break;
                } else {
                    total_sent += sent;
                }
            }
            
            // Close the connection after sending response (HTTP/1.0 style)
            close(connection->socket_fd);
        }
    };

} // namespace CORE