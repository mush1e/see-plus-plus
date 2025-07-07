#pragma once 

#include <string>
#include <unordered_map>
#include <vector>
#include <sstream>

namespace CORE {

    enum class BodyType {
        NONE,
        JSON,
        FORM_URLENCODED,
        MULTIPART,
        RAW
    };

    struct ParsedBody {
        BodyType type = BodyType::NONE;
        std::string raw_content;
        
        // For JSON
        std::string json_string;
        
        // For form data (both URL-encoded and multipart)
        std::unordered_map<std::string, std::string> form_data;
        
        // For multipart files
        struct FileUpload {
            std::string field_name;
            std::string filename;
            std::string content_type;
            std::string content;
        };
        std::vector<FileUpload> files;
        
        bool success = false;
        std::string error_message;
    };

    // Request represents a HTTP request being sent to 
    // our server over some transport protocol (TCP, UDP)
    struct Request {
        std::string method {};
        std::string path {};
        std::string version {};
        std::unordered_map<std::string, std::string> headers {};
        std::string body {};  // Raw body content
        ParsedBody parsed_body {}; // Parsed body based on Content-Type
    };

    // Response represents a HTTP Response being sent out
    // from our server over some transport protocol (TCP, UDP)
    struct Response {
        uint16_t status_code {};
        std::string status_text {};
        std::unordered_map<std::string, std::string> headers {};
        std::string body {};

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