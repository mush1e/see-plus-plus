#pragma once

#include "mime_detector.hpp"
#include <string>
#include <fstream>
#include <sys/stat.h>    // For file metadata
#include <chrono>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace UTILS {


    struct FileInfo {
        std::string content;                                    // The actual file contents
        std::string mime_type;                                  // Content-Type for HTTP header
        size_t file_size;                                       // Content-Length for HTTP header
        std::chrono::system_clock::time_point last_modified;    // Last-Modified for HTTP header
        bool success;                                           // Did we successfully read it?
        std::string error_message;                              // If not, what went wrong?
        
        FileInfo() : file_size(0), success(false) {}
    };


    class FileReader {
    public:
        // Safety limits to prevent memory exhaustion or DoS attacks
        static constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;  // 10MB max
        static constexpr size_t CHUNK_SIZE = 64 * 1024;            // 64KB read chunks
        
        static FileInfo read_file(const std::string& file_path) {
            FileInfo info;
            
            try {
                // Step 1: Get file metadata using POSIX stat() call
                struct stat file_stat;
                if (stat(file_path.c_str(), &file_stat) != 0) {
                    info.error_message = "File not found or inaccessible";
                    return info;
                }
                
                // Step 2: Validate it's a regular file (not directory, device, etc.)
                if (!S_ISREG(file_stat.st_mode)) {
                    info.error_message = "Not a regular file";
                    return info;
                }
                
                // Step 3: Check file size limits for safety
                if (static_cast<size_t>(file_stat.st_size) > MAX_FILE_SIZE) {
                    info.error_message = "File too large (max " + 
                        std::to_string(MAX_FILE_SIZE / (1024*1024)) + "MB)";
                    return info;
                }
                
                // Step 4: Convert Unix timestamp to C++ time_point
                auto time_since_epoch = std::chrono::seconds(file_stat.st_mtime);
                info.last_modified = std::chrono::system_clock::time_point(time_since_epoch);
                info.file_size = static_cast<size_t>(file_stat.st_size);
                
                // Step 5: Actually read the file content
                std::ifstream file(file_path, std::ios::binary);
                if (!file) {
                    info.error_message = "Cannot open file for reading";
                    return info;
                }
                
                // Step 6: Read efficiently in chunks
                info.content.reserve(info.file_size); // Pre-allocate memory
                
                char buffer[CHUNK_SIZE];
                while (file.read(buffer, CHUNK_SIZE) || file.gcount() > 0) {
                    info.content.append(buffer, file.gcount());
                }
                
                // Step 7: Detect MIME type from file extension
                info.mime_type = MimeTypeDetector::get_mime_type(file_path);
                
                // Step 8: Mark as successful
                info.success = true;
                
            } catch (const std::exception& e) {
                info.error_message = "Error reading file: " + std::string(e.what());
                info.success = false;
            }
            
            return info;
        }
        
        static std::string format_http_date(std::chrono::system_clock::time_point time_point) {
            // Convert to time_t for compatibility with C time functions
            auto time_t = std::chrono::system_clock::to_time_t(time_point);
            
            // Convert to GMT (UTC) as required by HTTP spec
            std::tm* gmt = std::gmtime(&time_t);
            
            // Format: "Wed, 21 Oct 2015 07:28:00 GMT"
            char buffer[64];
            std::strftime(buffer, sizeof(buffer), "%a, %d %b %Y %H:%M:%S GMT", gmt);
            return std::string(buffer);
        }
        
        static std::string generate_etag(size_t file_size, 
                                       std::chrono::system_clock::time_point last_modified) {
            // Convert timestamp to seconds since epoch
            auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
                last_modified.time_since_epoch()).count();
            
            // ETag format: "size-timestamp" in quotes (quotes required by HTTP spec)
            return "\"" + std::to_string(file_size) + "-" + std::to_string(timestamp) + "\"";
        }
        
        static std::string generate_cache_control(const std::string& mime_type) {
            if (MimeTypeDetector::is_cacheable(mime_type)) {
                // Static assets can be cached for 1 hour
                if (StringUtils::starts_with(mime_type, "image/") || 
                    StringUtils::starts_with(mime_type, "font/")) {
                    // Images and fonts can be cached longer (24 hours)
                    return "public, max-age=86400";
                } else {
                    // CSS, JS can be cached for 1 hour
                    return "public, max-age=3600";
                }
            } else {
                // HTML and other potentially dynamic content
                return "no-cache, must-revalidate";
            }
        }
        
        static bool should_use_sendfile(size_t file_size) {
            // sendfile() has overhead, only worth it for files > 32KB
            return file_size > 32 * 1024;
        }
    };

} // namespace UTILS