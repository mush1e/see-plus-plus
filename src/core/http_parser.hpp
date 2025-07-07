#pragma once

#include "http.hpp"
#include <string>
#include <sstream>
#include <algorithm>
#include <string_view>
#include <vector>

namespace CORE {

    enum class ParseState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        PARSING_BODY_CONTENT,
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
        TOO_MANY_HEADERS,
        INVALID_BODY_FORMAT
    };

    class HTTPParser {
    public:
        // Configuration constants
        static constexpr size_t MAX_BUFFER_SIZE = 8 * 1024 * 1024;
        static constexpr size_t MAX_REQUEST_LINE_SIZE = 8192;
        static constexpr size_t MAX_HEADER_SIZE = 64 * 1024;
        static constexpr size_t MAX_HEADERS_COUNT = 100;
        static constexpr size_t MAX_PARSE_ITERATIONS = 1000;
        
        HTTPParser() = default;
        
        bool parse(const std::string& data, Request& request) {
            if (buffer.size() + data.size() > MAX_BUFFER_SIZE) {
                state = ParseState::ERROR;
                error = ParseError::BUFFER_TOO_LARGE;
                return false;
            }
            
            buffer += data;
            
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
                    case ParseState::PARSING_BODY_CONTENT:
                        made_progress = parse_body_content(request);
                        break;
                    default:
                        break;
                }
                
                if (!made_progress) break;
                iterations++;
            }
            
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
                case ParseError::INVALID_BODY_FORMAT: return "Invalid body format";
                default: return "Unknown error";
            }
        }
        
        void reset() {
            buffer.clear();
            state = ParseState::PARSING_REQUEST_LINE;
            error = ParseError::NONE;
            content_length = 0;
            headers_end_pos = 0;
            headers_count = 0;
        }
        
        size_t get_buffer_size() const { return buffer.size(); }
        
    private:
        std::string buffer;
        ParseState state = ParseState::PARSING_REQUEST_LINE;
        ParseError error = ParseError::NONE;
        size_t content_length = 0;
        size_t headers_end_pos = 0;
        size_t headers_count = 0;
        
        // [Keep existing parse_request_line and parse_headers methods exactly the same]
        bool parse_request_line(Request& request) {
            size_t line_end = buffer.find("\r\n");
            if (line_end == std::string::npos) {
                if (buffer.size() > MAX_REQUEST_LINE_SIZE) {
                    state = ParseState::ERROR;
                    error = ParseError::INVALID_REQUEST_LINE;
                    return false;
                }
                return false;
            }
            
            if (line_end > MAX_REQUEST_LINE_SIZE) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            std::string request_line = buffer.substr(0, line_end);
            std::string_view line_view(request_line);
            
            auto space1 = line_view.find(' ');
            if (space1 == std::string_view::npos) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            request.method = std::string(line_view.substr(0, space1));
            line_view = line_view.substr(space1 + 1);
            
            auto space2 = line_view.find(' ');
            if (space2 == std::string_view::npos) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_REQUEST_LINE;
                return false;
            }
            
            request.path = std::string(line_view.substr(0, space2));
            line_view = line_view.substr(space2 + 1);
            request.version = std::string(line_view);
            
            if (!is_valid_http_method(request.method) || !is_valid_http_path(request.path)) {
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
                if (buffer.size() > MAX_HEADER_SIZE) {
                    state = ParseState::ERROR;
                    error = ParseError::INVALID_HEADERS;
                    return false;
                }
                return false;
            }
            
            std::string headers_section = buffer.substr(0, headers_end);
            headers_end_pos = headers_end + 4;
            
            std::string_view headers_view(headers_section);
            size_t line_start = 0;
            
            while (line_start < headers_view.size()) {
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
                    
                    trim_inplace(key);
                    trim_inplace(value);
                    std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                    
                    if (key.empty() || !is_valid_header_name(key)) {
                        state = ParseState::ERROR;
                        error = ParseError::INVALID_HEADERS;
                        return false;
                    }
                    
                    request.headers[key] = value;
                    headers_count++;
                }
                
                line_start = line_end + 2;
            }
            
            // Check for Content-Length
            auto content_length_it = request.headers.find("content-length");
            if (content_length_it != request.headers.end()) {
                try {
                    content_length = std::stoull(content_length_it->second);
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
            
            // No body, initialize parsed body and complete
            request.parsed_body.type = BodyType::NONE;
            request.parsed_body.success = true;
            request.parsed_body.raw_content = "";
            state = ParseState::COMPLETE;
            buffer.erase(0, headers_end_pos);
            return true;
        }
        
        bool parse_body(Request& request) {
            size_t available_body_data = buffer.size() - headers_end_pos;
            if (available_body_data < content_length) {
                return false; // Need more data
            }
            
            // Extract raw body
            request.body = buffer.substr(headers_end_pos, content_length);
            buffer.erase(0, headers_end_pos + content_length);
            
            // Now parse the body content based on Content-Type
            state = ParseState::PARSING_BODY_CONTENT;
            return true;
        }
        
        // NEW: Parse body content based on Content-Type
        bool parse_body_content(Request& request) {
            // Initialize parsed body
            request.parsed_body.raw_content = request.body;
            request.parsed_body.success = true; // Assume success unless we find errors
            
            if (request.body.empty()) {
                request.parsed_body.type = BodyType::NONE;
                request.parsed_body.success = true;  // Empty body is valid
                state = ParseState::COMPLETE;
                return true;
            }
            
            // Get content type
            auto content_type_it = request.headers.find("content-type");
            if (content_type_it == request.headers.end()) {
                request.parsed_body.type = BodyType::RAW;
                state = ParseState::COMPLETE;
                return true;
            }
            
            std::string content_type = to_lowercase(content_type_it->second);
            
            // Parse based on content type
            if (content_type.find("application/json") != std::string::npos) {
                parse_json_body(request);
            } else if (content_type.find("application/x-www-form-urlencoded") != std::string::npos) {
                parse_form_urlencoded_body(request);
            } else if (content_type.find("multipart/form-data") != std::string::npos) {
                parse_multipart_body(request, content_type);
            } else {
                request.parsed_body.type = BodyType::RAW;
            }
            
            // If parsing failed, set error state
            if (!request.parsed_body.success) {
                state = ParseState::ERROR;
                error = ParseError::INVALID_BODY_FORMAT;
                return false;
            }
            
            state = ParseState::COMPLETE;
            return true;
        }
        
        void parse_json_body(Request& request) {
            request.parsed_body.type = BodyType::JSON;
            request.parsed_body.json_string = request.body;
            
            // Basic JSON validation
            std::string trimmed = trim_copy(request.body);
            if (trimmed.empty()) {
                request.parsed_body.success = false;
                request.parsed_body.error_message = "Empty JSON body";
                return;
            }
            
            if ((trimmed.front() == '{' && trimmed.back() == '}') ||
                (trimmed.front() == '[' && trimmed.back() == ']')) {
                request.parsed_body.success = true;
            } else {
                request.parsed_body.success = false;
                request.parsed_body.error_message = "Invalid JSON format";
            }
        }
        
        void parse_form_urlencoded_body(Request& request) {
            request.parsed_body.type = BodyType::FORM_URLENCODED;
            
            std::vector<std::string> pairs = split(request.body, '&');
            for (const std::string& pair : pairs) {
                size_t eq_pos = pair.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = url_decode(pair.substr(0, eq_pos));
                    std::string value = url_decode(pair.substr(eq_pos + 1));
                    request.parsed_body.form_data[key] = value;
                }
            }
            
            request.parsed_body.success = true;
        }
        
        void parse_multipart_body(Request& request, const std::string& content_type) {
            request.parsed_body.type = BodyType::MULTIPART;
            
            // Extract boundary
            size_t boundary_pos = content_type.find("boundary=");
            if (boundary_pos == std::string::npos) {
                request.parsed_body.success = false;
                request.parsed_body.error_message = "Missing boundary in multipart content-type";
                return;
            }
            
            std::string boundary = "--" + content_type.substr(boundary_pos + 9);
            
            // TODO: Implement full multipart parsing
            // For now, just mark as raw
            request.parsed_body.type = BodyType::RAW;
            request.parsed_body.success = true;
        }
        
        // Utility methods (same as before)
        std::string to_lowercase(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }
        
        std::string trim_copy(const std::string& str) {
            auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            });
            auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base();
            return (start < end) ? std::string(start, end) : "";
        }
        
        std::vector<std::string> split(const std::string& str, char delimiter) {
            std::vector<std::string> result;
            std::string current;
            for (char c : str) {
                if (c == delimiter) {
                    if (!current.empty()) {
                        result.push_back(current);
                        current.clear();
                    }
                } else {
                    current += c;
                }
            }
            if (!current.empty()) {
                result.push_back(current);
            }
            return result;
        }
        
        std::string url_decode(const std::string& encoded) {
            std::string decoded;
            decoded.reserve(encoded.size());
            
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.size()) {
                    char hex_str[3] = {encoded[i+1], encoded[i+2], '\0'};
                    char* end_ptr;
                    long hex_value = std::strtol(hex_str, &end_ptr, 16);
                    
                    if (end_ptr == hex_str + 2) {
                        decoded += static_cast<char>(hex_value);
                        i += 2;
                    } else {
                        decoded += encoded[i];
                    }
                } else if (encoded[i] == '+') {
                    decoded += ' ';
                } else {
                    decoded += encoded[i];
                }
            }
            
            return decoded;
        }
        
        // Keep existing validation methods
        bool is_valid_http_method(const std::string& method) const {
            return method == "GET" || method == "POST" || method == "PUT" || 
                   method == "DELETE" || method == "HEAD" || method == "OPTIONS" ||
                   method == "PATCH" || method == "TRACE" || method == "CONNECT";
        }
        
        bool is_valid_http_path(const std::string& path) const {
            if (path.empty() || path[0] != '/') return false;
            if (path.find("..") != std::string::npos) return false;
            for (char c : path) {
                if (c == '\0' || c == '\r' || c == '\n') return false;
            }
            return true;
        }
        
        bool is_valid_header_name(const std::string& name) const {
            for (char c : name) {
                if (!std::isalnum(c) && c != '-' && c != '_') return false;
            }
            return true;
        }
        
        void trim_inplace(std::string& str) {
            str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), str.end());
        }
    };

} // namespace CORE