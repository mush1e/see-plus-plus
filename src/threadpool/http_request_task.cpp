#include "http_request_task.hpp"
#include <sstream>
#include <iostream>
#include <unistd.h>      // for close()
#include <sys/socket.h>  // for send()

namespace THREADPOOL {

HttpRequestTask::HttpRequestTask(
    std::shared_ptr<CORE::ConnectionState> conn,
    const std::string& raw_headers,
    CORE::Router &router
) : conn_(std::move(conn)), raw_headers_(raw_headers), router_(router) {}

void HttpRequestTask::execute(int worker_id) {
    // Parse raw headers into CORE::Request
    CORE::Request req;
    {
        std::istringstream stream(raw_headers_);
        std::string line;
        // Request line: METHOD PATH VERSION
        std::getline(stream, line);
        std::istringstream reqline(line);
        reqline >> req.method >> req.path >> req.version;
        
        // Headers
        while (std::getline(stream, line) && !line.empty()) {
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            auto colon = line.find(':');
            if (colon != std::string::npos) {
                std::string key   = line.substr(0, colon);
                std::string value = line.substr(colon + 1);
                if (!value.empty() && value.front() == ' ')
                    value.erase(0, 1);
                req.headers[key] = value;
            }
        }
    }

    // Route to controller
    CORE::Response res;
    if (!router_.route(req, res)) {
        res.status_code = 404;
        res.status_text = "Not Found";
        res.headers["Content-Type"] = "text/html";
        res.body = "<h1>404 Not Found</h1>";
    }

    // Build raw response
    std::string raw = res.to_string();

    // Send and close
    if (send(conn_->socket_fd, raw.data(), raw.size(), 0) < 0) {
        perror("send");
    }
    close(conn_->socket_fd);
}

} // namespace THREADPOOL