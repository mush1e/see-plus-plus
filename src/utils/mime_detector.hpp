#pragma once

#include "string_utils.hpp"  

#include <string>
#include <unordered_map>
#include <algorithm>
#include <string_view>

namespace UTILS {

    class MimeTypeDetector {
    public:

        static std::string get_mime_type(const std::string& file_path) {
            // Find the last dot in the filename to locate the extension
            size_t dot_pos = file_path.find_last_of('.');
            if (dot_pos == std::string::npos) {
                // No extension found, return binary default
                return "application/octet-stream";
            }
            
            // Extract everything after the last dot and convert to lowercase
            // This handles cases like "Photo.JPG" or "STYLE.CSS"
            std::string extension = file_path.substr(dot_pos + 1);
            std::transform(extension.begin(), extension.end(), 
                         extension.begin(), ::tolower);
            
            auto it = mime_type_map.find(extension);
            if (it != mime_type_map.end()) {
                return it->second;
            }
            
            // If we don't recognize the extension, return the generic binary type
            return "application/octet-stream";
        }
        
        static bool is_cacheable(const std::string& mime_type) {
            // Images are almost always safe to cache
            if (StringUtils::starts_with(mime_type, "image/")) return true;
            
            // CSS and JavaScript files are typically static
            if (StringUtils::starts_with(mime_type, "text/css")) return true;
            if (StringUtils::starts_with(mime_type, "text/javascript")) return true;
            if (StringUtils::starts_with(mime_type, "application/javascript")) return true;
            
            // Fonts don't change often
            if (StringUtils::starts_with(mime_type, "font/")) return true;
            if (StringUtils::starts_with(mime_type, "application/font")) return true;
            
            // Most other content (especially HTML) might be dynamic
            return false;
        }
        

        // Gets a human-readable description of a MIME type.
        // Useful for logging, debugging, or user interfaces.

        static std::string get_description(const std::string& mime_type) {
            if (StringUtils::starts_with(mime_type, "image/")) return "Image file";
            if (StringUtils::starts_with(mime_type, "text/html")) return "Web page";
            if (StringUtils::starts_with(mime_type, "text/css")) return "Stylesheet";
            if (StringUtils::starts_with(mime_type, "text/javascript") || 
                StringUtils::starts_with(mime_type, "application/javascript")) return "JavaScript";
            if (StringUtils::starts_with(mime_type, "application/json")) return "JSON data";
            if (StringUtils::starts_with(mime_type, "font/")) return "Font file";
            if (StringUtils::starts_with(mime_type, "video/")) return "Video file";
            if (StringUtils::starts_with(mime_type, "audio/")) return "Audio file";
            return "Binary file";
        }

    private:
        static const std::unordered_map<std::string, std::string> mime_type_map;
    };

    const std::unordered_map<std::string, std::string> MimeTypeDetector::mime_type_map = {
        // Web content - the core of what web servers serve
        {"html", "text/html"},
        {"htm",  "text/html"},
        {"css",  "text/css"},
        {"js",   "text/javascript"},
        {"json", "application/json"},
        {"xml",  "text/xml"},
        {"txt",  "text/plain"},
        
        // Images - probably the most common static files
        {"jpg",  "image/jpeg"},
        {"jpeg", "image/jpeg"},
        {"png",  "image/png"},
        {"gif",  "image/gif"},
        {"bmp",  "image/bmp"},
        {"webp", "image/webp"},
        {"svg",  "image/svg+xml"},
        {"ico",  "image/x-icon"},
        
        // Fonts - increasingly important for modern web design
        {"woff",  "font/woff"},
        {"woff2", "font/woff2"},
        {"ttf",   "font/ttf"},
        {"otf",   "font/otf"},
        {"eot",   "application/vnd.ms-fontobject"},
        
        // Documents - common file types users might serve
        {"pdf",  "application/pdf"},
        {"doc",  "application/msword"},
        {"docx", "application/vnd.openxmlformats-officedocument.wordprocessingml.document"},
        {"xls",  "application/vnd.ms-excel"},
        {"xlsx", "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet"},
        
        // Archives - for downloadable content
        {"zip",  "application/zip"},
        {"tar",  "application/x-tar"},
        {"gz",   "application/gzip"},
        {"7z",   "application/x-7z-compressed"},
        
        // Media files - for rich content
        {"mp3",  "audio/mpeg"},
        {"mp4",  "video/mp4"},
        {"avi",  "video/x-msvideo"},
        {"mov",  "video/quicktime"},
        {"wav",  "audio/wav"},
        {"ogg",  "audio/ogg"}
    };

} // namespace UTILS