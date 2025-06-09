#pragma once
#include <string>
#include <sstream>
#include <unordered_map>

namespace CORE {

struct Request {
    std::string method;
    std::string path;
    std::string version;
    std::unordered_map<std::string, std::string> headers;
    std::string body;          
};

struct Response {
    int status_code   = 200;
    std::string status_text = "OK";
    std::unordered_map<std::string, std::string> headers;
    std::string body;

    // Helper to build the raw response bytes
    std::string to_string() const {
        std::ostringstream oss;
        oss << "HTTP/1.1 " << status_code << " " << status_text << "\r\n";
        for (auto& [k,v]: headers) {
            oss << k << ": " << v << "\r\n";
        }
        oss << "\r\n" << body;
        return oss.str();
    }
};

} // namespace CORE
