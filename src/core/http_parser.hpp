#pragma once

#include "http.hpp"
#include <string>
#include <sstream>
#include <algorithm>

namespace CORE {

    enum class ParseState {
        PARSING_REQUEST_LINE,
        PARSING_HEADERS,
        PARSING_BODY,
        COMPLETE,
        ERROR
    };

    class HTTPParser {
    public:
        HTTPParser() = default;
        
        // Parse incoming data and return true if a complete request is ready
        bool parse(const std::string& data, Request& request) {
            buffer += data;
            
            while (state != ParseState::COMPLETE && state != ParseState::ERROR) {
                switch (state) {
                    case ParseState::PARSING_REQUEST_LINE:
                        if (!parse_request_line(request)) return false;
                        break;
                    case ParseState::PARSING_HEADERS:
                        if (!parse_headers(request)) return false;
                        break;
                    case ParseState::PARSING_BODY:
                        if (!parse_body(request)) return false;
                        break;
                    default:
                        break;
                }
            }
            
            return state == ParseState::COMPLETE;
        }
        
        bool is_complete() const { return state == ParseState::COMPLETE; }
        bool has_error() const { return state == ParseState::ERROR; }
        
        // Reset parser for next request
        void reset() {
            buffer.clear();
            state = ParseState::PARSING_REQUEST_LINE;
            content_length = 0;
            headers_end_pos = 0;
        }
        
    private:
        std::string buffer;
        ParseState state = ParseState::PARSING_REQUEST_LINE;
        size_t content_length = 0;
        size_t headers_end_pos = 0;
        
        bool parse_request_line(Request& request) {
            size_t line_end = buffer.find("\r\n");
            if (line_end == std::string::npos) {
                return false; // Need more data
            }
            
            std::string request_line = buffer.substr(0, line_end);
            std::istringstream iss(request_line);
            if (!(iss >> request.method >> request.path >> request.version)) {
                state = ParseState::ERROR;
                return false;
            }
            
            buffer.erase(0, line_end + 2);
            state = ParseState::PARSING_HEADERS;
            return true;
        }
        
        bool parse_headers(Request& request) {
            size_t headers_end = buffer.find("\r\n\r\n");
            if (headers_end == std::string::npos) {
                return false;
            }
            
            std::string headers_section = buffer.substr(0, headers_end);
            headers_end_pos = headers_end + 4;
            
            std::istringstream headers_stream(headers_section);
            std::string line;
            
            while (std::getline(headers_stream, line)) {
                if (!line.empty() && line.back() == '\r') {
                    line.pop_back();
                }
                
                size_t colon_pos = line.find(':');
                if (colon_pos == std::string::npos) {
                    continue;
                }
                
                std::string key = line.substr(0, colon_pos);
                std::string value = line.substr(colon_pos + 1);
                
                trim(key);
                trim(value);
                std::transform(key.begin(), key.end(), key.begin(), ::tolower);
                
                request.headers[key] = value;
            }
            
            auto content_length_it = request.headers.find("content-length");
            if (content_length_it != request.headers.end()) {
                try {
                    content_length = std::stoull(content_length_it->second);
                    if (content_length > 0) {
                        state = ParseState::PARSING_BODY;
                        return true;
                    }
                } catch (const std::exception&) {
                    state = ParseState::ERROR;
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
                return false;
            }
            
            request.body = buffer.substr(headers_end_pos, content_length);
            state = ParseState::COMPLETE;
            buffer.erase(0, headers_end_pos + content_length);
            return true;
        }
        
        void trim(std::string& str) {
            str.erase(str.begin(), std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            }));
            
            str.erase(std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base(), str.end());
        }
    };

} // namespace CORE
