#pragma once

#include "http.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <string_view>  // C++17 feature for efficient string operations

namespace CORE {

    enum class ParseState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        COMPLETE,
        ERROR
    };

    enum class ParseError {
        NONE,
        BUFFER_TOO_LARGE,
        INVALID_REQUEST_LINE,
        INVALID_HEADERS,
        INVALID_CONTENT_LENGTH,
        MALFORMED_DATA,
        TOO_MANY_HEADERS
    };

    class HTTPParser {
    public:
        // Configuration constants
        static constexpr size_t MAX_BUFFER_SIZE = 8 * 1024 * 1024;      // 8MB max buffer
        static constexpr size_t MAX_REQUEST_LINE_SIZE = 8192;           // 8KB max request line
        static constexpr size_t MAX_HEADER_SIZE = 64 * 1024;            // 64KB max headers
        static constexpr size_t MAX_HEADERS_COUNT = 100;                // Max number of headers
        static constexpr size_t MAX_PARSE_ITERATIONS = 1000;            // Prevent infinite loops
        
        HTTPParser() = default;
        
        // Parse incoming data and return true if a complete request is ready
        bool parse(const std::string& data, Request& request) {
            // Prevent buffer from growing too large
            if (buffer.size() + data.size() > MAX_BUFFER_SIZE) {
                state = ParseState::ERROR;
                error = ParseError::BUFFER_TOO_LARGE;
                return false;
            }
            
            buffer += data;
            
            // Prevent infinite loops with iteration limit
            size_t iterations = 0;
            while (state != ParseState::COMPLETE && 
                   state != ParseState::ERROR && 
                   iterations < MAX_PARSE_ITERATIONS) {
                
                bool made_progress = false;
                
                switch (state) {
                    case ParseState::PARSING_REQUEST_LINE:
                        made_progress = parse_request_line(request);
                        break;
                    case ParseState::PARSING_HEADERS:
                        made_progress = parse_headers(request);
                        break;
                    case ParseState::PARSING_BODY:
                        made_progress = parse_body(request);
                        break;
                    default:
                        break;
                }
                
                // If we're not making progress, we need more data
                if (!made_progress) {
                    break;
                }
                
                iterations++;
            }
            
            // Check if we hit the iteration limit (possible infinite loop)
            if (iterations >= MAX_PARSE_ITERATIONS) {
                state = ParseState::ERROR;
                error = ParseError::MALFORMED_DATA;
                return false;
            }
            
            return state == ParseState::COMPLETE;
        }
        
        bool is_complete() const { return state == ParseState::COMPLETE; }
        bool has_error() const { return state == ParseState::ERROR; }
        ParseError get_error() const { return error; }
        
        std::string get_error_description() const {
            switch (error) {
                case ParseError::NONE: return "No error";
                case ParseError::BUFFER_TOO_LARGE: return "Request buffer too large";
                case ParseError::INVALID_REQUEST_LINE: return "Invalid request line format";
                case ParseError::INVALID_HEADERS: return "Invalid header format";
                case ParseError::INVALID_CONTENT_LENGTH: return "Invalid Content-Length value";
                case ParseError::MALFORMED_DATA: return "Malformed HTTP data";
                case ParseError::TOO_MANY_HEADERS: return "Too many headers";
                default: return "Unknown error";
            }
        }
        
        // Reset parser for next request
        void reset() {
            buffer.clear();
            state = ParseState::PARSING_REQUEST_LINE;
            error = ParseError::NONE;
            content_length = 0;
            headers_end_pos = 0;
            headers_count = 0;
        }
        
        // Get current buffer size for monitoring
        size_t get_buffer_size() const { return buffer.size(); }
        
    private:
        std::string buffer;
        ParseState state = ParseState::PARSING_REQUEST_LINE;
        ParseError error = ParseError::NONE;
        size_t content_length = 0;
        size_t headers_end_pos = 0;
        size_t headers_count = 0;
        
        bool parse_request_line(Request& request) {
            // Check if request line is too long before we find \r\n
            size_t line_end = buffer.find("\r\n");
            if (line_end == std::string::npos) {
                // Check if we have too much data without finding line ending
                if (buffer.size() > MAX_REQUEST_LINE_SIZE) {
                    state = ParseState::ERROR;
                    error = ParseError::INVALID_REQUEST_LINE;
                    return false;
                }
                return false; // Need more data
            }
            
            // Check if request line is too long
            if (line_end > MAX_REQUEST_LINE_SIZE) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            std::string request_line = buffer.substr(0, line_end);
            
            // Use string_view for efficient parsing without copies
            std::string_view line_view(request_line);
            
            // Parse method
            auto space1 = line_view.find(' ');
            if (space1 == std::string_view::npos) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            request.method = std::string(line_view.substr(0, space1));
            line_view = line_view.substr(space1 + 1);
            
            // Parse path
            auto space2 = line_view.find(' ');
            if (space2 == std::string_view::npos) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            request.path = std::string(line_view.substr(0, space2));
            line_view = line_view.substr(space2 + 1);
            
            // Parse version
            request.version = std::string(line_view);
            
            // Validate HTTP method (basic security check)
            if (!is_valid_http_method(request.method)) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            // Validate path (prevent directory traversal)
            if (!is_valid_http_path(request.path)) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            buffer.erase(0, line_end + 2);
            state = ParseState::PARSING_HEADERS;
            return true;
        }
        
        bool parse_headers(Request& request) {
            size_t headers_end = buffer.find("\r\n\r\n");
            if (headers_end == std::string::npos) {
                // Check if headers section is getting too large
                if (buffer.size() > MAX_HEADER_SIZE) {
                    state = ParseState::ERROR;
                    error = ParseError::INVALID_HEADERS;
                    return false;
                }
                return false; // Need more data
            }
            
            std::string headers_section = buffer.substr(0, headers_end);
            headers_end_pos = headers_end + 4;
            
            // Parse headers line by line
            std::string_view headers_view(headers_section);
            size_t line_start = 0;
            
            while (line_start < headers_view.size()) {
                // Check header count limit
                if (headers_count >= MAX_HEADERS_COUNT) {
                    state = ParseState::ERROR;
                    error = ParseError::TOO_MANY_HEADERS;
                    return false;
                }
                
                size_t line_end = headers_view.find("\r\n", line_start);
                if (line_end == std::string_view::npos) {
                    line_end = headers_view.size();
                }
                
                std::string_view line = headers_view.substr(line_start, line_end - line_start);
                
                if (!line.empty()) {
                    size_t colon_pos = line.find(':');
                    if (colon_pos == std::string_view::npos) {
                        state = ParseState::ERROR;
                        error = ParseError::INVALID_HEADERS;
                        return false;
                    }
                    
                    std::string key = std::string(line.substr(0, colon_pos));
                    std::string value = std::string(line.substr(colon_pos + 1));
                    
                    trim(key);
                    trim(value);
                    
                    // Convert header name to lowercase for case-insensitive lookup
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                    
                    // Basic header validation
                    if (key.empty() || !is_valid_header_name(key)) {
                        state = ParseState::ERROR;
                        error = ParseError::INVALID_HEADERS;
                        return false;
                    }
                    
                    request.headers[key] = value;
                    headers_count++;
                }
                
                line_start = line_end + 2; // Skip \r\n
            }
            
            // Check for Content-Length header
            auto content_length_it = request.headers.find("content-length");
            if (content_length_it != request.headers.end()) {
                try {
                    content_length = std::stoull(content_length_it->second);
                    // Reasonable content length limit (prevent memory exhaustion)
                    if (content_length > MAX_BUFFER_SIZE) {
                        state = ParseState::ERROR;
                        error = ParseError::INVALID_CONTENT_LENGTH;
                        return false;
                    }
                    if (content_length > 0) {
                        state = ParseState::PARSING_BODY;
                        return true;
                    }
                } catch (const std::exception&) {
                    state = ParseState::ERROR;
                    error = ParseError::INVALID_CONTENT_LENGTH;
                    return false;
                }
            }
            
            state = ParseState::COMPLETE;
            buffer.erase(0, headers_end_pos);
            return true;
        }
        
        bool parse_body(Request& request) {
            size_t available_body_data = buffer.size() - headers_end_pos;
            if (available_body_data < content_length) {
                return false; // Need more data
            }
            
            request.body = buffer.substr(headers_end_pos, content_length);
            state = ParseState::COMPLETE;
            buffer.erase(0, headers_end_pos + content_length);
            return true;
        }
        
        // Security validation functions
        bool is_valid_http_method(const std::string& method) const {
            // Only allow standard HTTP methods
            return method == "GET" || method == "POST" || method == "PUT" || 
                   method == "DELETE" || method == "HEAD" || method == "OPTIONS" ||
                   method == "PATCH" || method == "TRACE" || method == "CONNECT";
        }
        
        bool is_valid_http_path(const std::string& path) const {
            // Basic path validation to prevent directory traversal
            if (path.empty() || path[0] != '/') {
                return false;
            }
            
            // Check for directory traversal attempts
            if (path.find("..") != std::string::npos) {
                return false;
            }
            
            // Check for null bytes or other dangerous characters
            for (char c : path) {
                if (c == '\0' || c == '\r' || c == '\n') {
                    return false;
                }
            }
            
            return true;
        }
        
        bool is_valid_header_name(const std::string& name) const {
            // Header names should only contain valid characters
            for (char c : name) {
                if (!std::isalnum(c) && c != '-' && c != '_') {
                    return false;
                }
            }
            return true;
        }
        
        void trim(std::string& str) {
            // Remove leading whitespace
            str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            
            // Remove trailing whitespace
            str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), str.end());
        }
    };

} // namespace CORE