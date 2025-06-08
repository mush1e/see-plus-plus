#include "http_request_task.hpp"

namespace THREADPOOL {

HTTPRequestTask::HTTPRequestTask(
    std::shared_ptr<CORE::ConnectionState> conn,
    const std::string& raw_headers
) : conn_(std::move(conn)), raw_headers_(raw_headers)
{}

void HTTPRequestTask::execute(int worker_id) {
    parse_request();

    std::cout << "[Worker" << worker_id << "] HTTP task for client" 
              << this->conn_->client_ip << std::endl; 

    send_response();
}

void HTTPRequestTask::parse_request() {
    std::istringstream stream(raw_headers_);
    std::string line;

    // --- Parse request line ---
    std::getline(stream, line);            // e.g. "GET /index.html HTTP/1.1"
    std::istringstream reqline(line);
    reqline >> method_ >> path_ >> version_;

    // --- Parse headers ---
    while (std::getline(stream, line) && !line.empty()) {
        // Remove any trailing '\r'
        if (line.back() == '\r') line.pop_back();
        auto colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            // Trim leading spaces
            if (!value.empty() && value[0] == ' ')
                value.erase(0, 1);
            headers_[key] = value;
        }
    }
}

void HTTPRequestTask::send_response() {
     const std::string body = "<html><body><h1>Hello from C++ Server!</h1></body></html>";

    // Build the response
    std::ostringstream oss;
    oss << "HTTP/1.1 200 OK\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Content-Type: text/html\r\n"
        << "Connection: close\r\n"
        << "\r\n"
        << body;

    const std::string resp = oss.str();

    // Send it (in one go)
    ssize_t sent = send(conn_->socket_fd,
                        resp.data(),
                        resp.size(),
                        0);
    if (sent < 0) {
        perror("send");
    }

    // Close the connection
    close(conn_->socket_fd);
}

} // namespace THREADPOOL