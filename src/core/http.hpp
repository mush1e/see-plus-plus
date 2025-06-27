#pragma once 

#include <string>
#include <unordered_map>
#include <sstream>

namespace CORE {

    // Request represents a HTTP request being sent to 
    // our server over some transport protocol (TCP, UDP)
    struct Request {
        std::string method                                   {};
        std::string path                                     {};
        std::string version                                  {};
        std::unordered_map<std::string, std::string> headers {};
        std::string body                                     {};
    };


    // Response represents a HTTP Response being sent out
    // from our server over some transport protocol (TCP, UDP)
    struct Response {
        uint16_t    status_code {};
        std::string status_text {};
        std::unordered_map<std::string, std::string> headers {};
        std::string body                                     {};

        std::string str() const {
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