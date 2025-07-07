#pragma once

#include "string_utils.hpp"

#include <string>
#include <vector>
#include <sys/stat.h>    // For file info on POSIX systems
#include <unistd.h>      // For access() function
#include <cstdlib>       // For realpath()
#include <iostream>

namespace UTILS {

    class PathSecurity {
    public:

        static std::string url_decode(const std::string& encoded) {
            std::string decoded;
            decoded.reserve(encoded.size()); // Pre-allocate for efficiency
            
            for (size_t i = 0; i < encoded.size(); ++i) {
                if (encoded[i] == '%' && i + 2 < encoded.size()) {
                    // Found a percent-encoded sequence, try to decode it
                    char hex_str[3] = {encoded[i+1], encoded[i+2], '\0'};
                    char* end_ptr;
                    long hex_value = std::strtol(hex_str, &end_ptr, 16);
                    
                    if (end_ptr == hex_str + 2) {
                        // Successfully converted hex digits to a character
                        decoded += static_cast<char>(hex_value);
                        i += 2; // Skip the two hex digits we just processed
                    } else {
                        // Invalid hex sequence, keep the % character as-is
                        decoded += encoded[i];
                    }
                } else if (encoded[i] == '+') {
                    // In URL encoding, + represents a space character
                    decoded += ' ';
                } else {
                    // Regular character, copy as-is
                    decoded += encoded[i];
                }
            }
            
            return decoded;
        }
        
        static bool file_exists_and_readable(const std::string& file_path) {
            // Use the stat system call to get file information
            struct stat file_stat;
            if (stat(file_path.c_str(), &file_stat) != 0) {
                // stat() failed, file probably doesn't exist or we don't have permission
                return false;
            }
            
            // Check if it's a regular file (not a directory, device file, etc.)
            if (!S_ISREG(file_stat.st_mode)) {
                return false;
            }
            
            // Check if we can read the file using the access() system call
            return access(file_path.c_str(), R_OK) == 0;
        }
        
        static std::string resolve_safe_path(const std::string& requested_path, 
                                           const std::string& document_root) {
            
            // Step 1: Clean up the document root path
            std::string clean_doc_root = document_root;
            if (!clean_doc_root.empty() && clean_doc_root.back() != '/') {
                clean_doc_root += '/';
            }
            
            // Step 2: Split the requested path into components and validate each one
            std::vector<std::string> path_components = split_path(requested_path);
            std::vector<std::string> safe_components;
            
            for (const std::string& component : path_components) {
                // Skip empty components (caused by double slashes like "//")
                if (component.empty()) {
                    continue;
                }
                
                // SECURITY CHECK: Reject any component that tries to go up directories
                if (component == "..") {
                    std::cout << "Security violation: Directory traversal attempt in " 
                              << requested_path << std::endl;
                    return ""; // Return empty string to indicate unsafe path
                }
                
                // Skip current directory references (they're unnecessary)
                if (component == ".") {
                    continue;
                }
                
                // SECURITY CHECK: Make sure the component doesn't contain dangerous characters
                if (!is_safe_filename_component(component)) {
                    std::cout << "🚨 Security violation: Unsafe characters in '" 
                              << component << "'" << std::endl;
                    return "";
                }
                
                // This component passed all security checks
                safe_components.push_back(component);
            }
            
            // Step 3: Build the final safe path
            std::string safe_path = clean_doc_root;
            for (const std::string& component : safe_components) {
                safe_path += component;
                safe_path += '/';
            }
            
            // Remove trailing slash if the original path didn't end with one
            if (!requested_path.empty() && requested_path.back() != '/' && 
                !safe_path.empty() && safe_path.back() == '/') {
                safe_path.pop_back();
            }
            
            return safe_path;
        }
        
        static bool verify_path_with_realpath(const std::string& constructed_path,
                                            const std::string& document_root) {
            // First, get the canonical (real) path of our document root
            char* real_doc_root = realpath(document_root.c_str(), nullptr);
            if (!real_doc_root) {
                // Can't resolve document root - this is a configuration error
                std::cerr << "⚠️  Warning: Cannot resolve document root: " << document_root << std::endl;
                return false;
            }
            
            std::string canonical_root(real_doc_root);
            free(real_doc_root); // realpath() allocates memory that we must free
            
            // Make sure canonical root ends with slash for comparison
            if (!canonical_root.empty() && canonical_root.back() != '/') {
                canonical_root += '/';
            }
            
            // Now get the real path of our constructed path
            char* real_requested = realpath(constructed_path.c_str(), nullptr);
            if (!real_requested) {
                // Path doesn't exist or can't be resolved - this might be OK
                // (the file might not exist yet, but the path structure is safe)
                // So we'll allow it if our basic checks passed
                return true;
            }
            
            std::string canonical_requested(real_requested);
            free(real_requested);
            
            // Final security check: make sure resolved path is within document root
            if (canonical_requested.length() < canonical_root.length()) {
                std::cout << "🚨 Security violation: Resolved path escapes document root" << std::endl;
                return false;
            }
            
            bool is_safe = StringUtils::starts_with(canonical_requested, canonical_root);
            if (!is_safe) {
                std::cout << "🚨 Security violation: " << constructed_path 
                          << " resolves outside document root" << std::endl;
            }
            
            return is_safe;
        }
        
    private:
        static std::vector<std::string> split_path(const std::string& path) {
            std::vector<std::string> components;
            std::string current_component;
            
            for (char c : path) {
                if (c == '/') {
                    if (!current_component.empty()) {
                        components.push_back(current_component);
                        current_component.clear();
                    }
                } else {
                    current_component += c;
                }
            }
            
            // Don't forget the last component
            if (!current_component.empty()) {
                components.push_back(current_component);
            }
            
            return components;
        }
        
        static bool is_safe_filename_component(const std::string& component) {
            // Empty components are not safe
            if (component.empty()) {
                return false;
            }
            
            // Check for dangerous characters that might cause problems
            for (char c : component) {
                // Control characters (0-31) are generally dangerous in filenames
                if (c < 32 && c != '\t') {
                    return false;
                }
                
                // These characters can cause problems in shells, URLs, or filesystems
                if (c == '<' || c == '>' || c == ':' || c == '"' || 
                    c == '|' || c == '?' || c == '*' || c == '\\') {
                    return false;
                }
                
                // Null byte is always dangerous
                if (c == '\0') {
                    return false;
                }
            }
            
            return true;
        }
    };

} // namespace UTILS