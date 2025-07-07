#pragma once

#include "../core/controller.hpp"
#include "../core/http.hpp"
#include "../utils/mime_detector.hpp"
#include "../utils/path_security.hpp"
#include "../utils/file_reader.hpp"
#include <string>
#include <iostream>

class StaticFileController : public CORE::Controller {
public:

    explicit StaticFileController(const std::string& document_root) 
        : document_root_(document_root) {
        
        if (!document_root_.empty() && document_root_.back() != '/') {
            document_root_ += '/';
        }
        
        std::cout << "📁 StaticFileController: serving files from " 
                  << document_root_ << std::endl;
    }
    
    void handle(const CORE::Request& req, CORE::Response& res) override {
        std::string decoded_path = UTILS::PathSecurity::url_decode(req.path);
        
        std::string safe_file_path = UTILS::PathSecurity::resolve_safe_path(
            decoded_path, document_root_);
        
        if (safe_file_path.empty()) {
            send_error_response(res, 403, "Forbidden", 
                "Access to the requested path is not allowed");
            return;
        }
        
        if (decoded_path.back() == '/') {
            std::string index_path = UTILS::PathSecurity::resolve_safe_path(
                decoded_path + "index.html", document_root_);
                
            if (!index_path.empty() && UTILS::PathSecurity::file_exists_and_readable(index_path)) {
                safe_file_path = index_path;
            } else {
                send_directory_response(req, res, decoded_path);
                return;
            }
        }
        
        if (!UTILS::PathSecurity::file_exists_and_readable(safe_file_path)) {
            send_error_response(res, 404, "Not Found", 
                "The requested file could not be found");
            return;
        }
        
        if (handle_conditional_request(req, res, safe_file_path)) {
            return;
        }
        
        serve_file(res, safe_file_path);
    }
    
private:
    std::string document_root_;  
    
    void serve_file(CORE::Response& res, const std::string& file_path) {
        auto file_info = UTILS::FileReader::read_file(file_path);
        
        if (!file_info.success) {
            send_error_response(res, 500, "Internal Server Error", 
                "Error reading file: " + file_info.error_message);
            return;
        }
        
        res.status_code = 200;
        res.status_text = "OK";
        
        res.headers["Content-Type"] = file_info.mime_type;
        res.headers["Content-Length"] = std::to_string(file_info.file_size);
        
        res.headers["Server"] = "see-plus-plus/1.0";
        
        res.headers["Last-Modified"] = UTILS::FileReader::format_http_date(file_info.last_modified);
        res.headers["ETag"] = UTILS::FileReader::generate_etag(file_info.file_size, file_info.last_modified);
        res.headers["Cache-Control"] = UTILS::FileReader::generate_cache_control(file_info.mime_type);
        
        if (UTILS::StringUtils::starts_with(file_info.mime_type, "text/html")) {
            res.headers["X-Content-Type-Options"] = "nosniff";
        }
        
        res.body = std::move(file_info.content);
        
        std::cout << "✅ Served: " << file_path 
                  << " (" << file_info.file_size << " bytes, " 
                  << file_info.mime_type << ")" << std::endl;
    }
    

    bool handle_conditional_request(const CORE::Request& req, CORE::Response& res, 
                                  const std::string& file_path) {
        struct stat file_stat;
        if (stat(file_path.c_str(), &file_stat) != 0) {
            return false; // Can't get file info, serve normally
        }
        
        auto last_modified = std::chrono::system_clock::time_point(
            std::chrono::seconds(file_stat.st_mtime));
        std::string current_etag = UTILS::FileReader::generate_etag(
            file_stat.st_size, last_modified);
        
        auto if_none_match = req.headers.find("if-none-match");
        if (if_none_match != req.headers.end()) {
            std::string client_etag = UTILS::StringUtils::trim(if_none_match->second);
            
            if (client_etag == current_etag) {
                res.status_code = 304;
                res.status_text = "Not Modified";
                res.headers["ETag"] = current_etag;
                res.headers["Last-Modified"] = UTILS::FileReader::format_http_date(last_modified);
                res.headers["Cache-Control"] = UTILS::FileReader::generate_cache_control(
                    UTILS::MimeTypeDetector::get_mime_type(file_path));
                
                std::cout << "💾 304 Not Modified: " << file_path << std::endl;
                return true; // We handled the request
            }
        }
        
        return false; // File needs to be sent normally
    }
    

    void send_directory_response(const CORE::Request& req, CORE::Response& res, 
                               const std::string& dir_path) {
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = "text/html";
        res.headers["Server"] = "see-plus-plus/1.0";
        
        res.body = R"(<!DOCTYPE html>
<html>
<head>
    <title>Directory: )" + req.path + R"(</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; }
        .header { color: #333; border-bottom: 2px solid #007acc; padding-bottom: 10px; }
    </style>
</head>
<body>
    <h1 class="header">📁 Directory: )" + req.path + R"(</h1>
    <p>This directory exists, but no index.html file was found.</p>
    <p>Try accessing a specific file directly.</p>
    <hr>
    <small>see-plus-plus/1.0 static file server</small>
</body>
</html>)";
        
        res.headers["Content-Length"] = std::to_string(res.body.size());
    }
    

    void send_error_response(CORE::Response& res, int status_code, 
                           const std::string& status_text, const std::string& message) {
        res.status_code = status_code;
        res.status_text = status_text;
        res.headers["Content-Type"] = "text/html";
        res.headers["Server"] = "see-plus-plus/1.0";
        
        res.body = R"(<!DOCTYPE html>
<html>
<head>
    <title>)" + std::to_string(status_code) + " " + status_text + R"(</title>
    <style>
        body { font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }
        .error-box { background: white; padding: 30px; border-radius: 8px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }
        .error-code { font-size: 48px; color: #e74c3c; margin: 0; }
        .error-title { color: #2c3e50; margin: 10px 0; }
        .error-message { color: #7f8c8d; margin: 20px 0; }
    </style>
</head>
<body>
    <div class="error-box">
        <h1 class="error-code">)" + std::to_string(status_code) + R"(</h1>
        <h2 class="error-title">)" + status_text + R"(</h2>
        <p class="error-message">)" + message + R"(</p>
        <hr>
        <small>see-plus-plus/1.0 static file server</small>
    </div>
</body>
</html>)";
        
        res.headers["Content-Length"] = std::to_string(res.body.size());
    }
};