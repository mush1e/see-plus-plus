#pragma once

#include <string>
#include <vector>
#include <string_view>
#include <algorithm>

namespace UTILS {

    class StringUtils {
    public:
        /**
         * Checks if a string starts with a given prefix.
         * This is our C++17-compatible version of C++20's std::string::starts_with().
         * 
         * @param str The string to check
         * @param prefix The prefix to look for
         * @return true if str starts with prefix, false otherwise
         * 
         * Examples:
         *   starts_with("image/jpeg", "image/") → true
         *   starts_with("text/html", "image/") → false
         *   starts_with("css", "css") → true
         *   starts_with("js", "javascript") → false
         */
        static bool starts_with(const std::string& str, const std::string& prefix) {
            // If the prefix is longer than the string, it can't be a prefix
            if (prefix.length() > str.length()) {
                return false;
            }
            
            // Compare the beginning of the string with the prefix
            // This is equivalent to str.substr(0, prefix.length()) == prefix
            // but more efficient because it doesn't create a temporary substring
            return str.compare(0, prefix.length(), prefix) == 0;
        }
        
        /**
         * String_view version for better performance when we don't need to own the string.
         * string_view is like a "window" into an existing string - it doesn't copy the data.
         */
        static bool starts_with(std::string_view str, std::string_view prefix) {
            if (prefix.length() > str.length()) {
                return false;
            }
            return str.substr(0, prefix.length()) == prefix;
        }
        
        /**
         * Checks if a string ends with a given suffix.
         * This complements starts_with() and is equally useful for file extension checking.
         * 
         * @param str The string to check
         * @param suffix The suffix to look for
         * @return true if str ends with suffix, false otherwise
         * 
         * Examples:
         *   ends_with("photo.jpg", ".jpg") → true
         *   ends_with("style.css", ".js") → false
         */
        static bool ends_with(const std::string& str, const std::string& suffix) {
            if (suffix.length() > str.length()) {
                return false;
            }
            
            // Compare the end of the string with the suffix
            return str.compare(str.length() - suffix.length(), suffix.length(), suffix) == 0;
        }
        
        static bool ends_with(std::string_view str, std::string_view suffix) {
            if (suffix.length() > str.length()) {
                return false;
            }
            return str.substr(str.length() - suffix.length()) == suffix;
        }
        
        /**
         * Case-insensitive version of starts_with.
         * Useful for HTTP headers, which are case-insensitive.
         * 
         * @param str The string to check
         * @param prefix The prefix to look for (case will be ignored)
         * @return true if str starts with prefix (ignoring case)
         * 
         * Examples:
         *   starts_with_ignore_case("Content-Type", "content-") → true
         *   starts_with_ignore_case("IMAGE/JPEG", "image/") → true
         */
        static bool starts_with_ignore_case(const std::string& str, const std::string& prefix) {
            if (prefix.length() > str.length()) {
                return false;
            }
            
            // Convert both strings to lowercase and compare
            // This is not the most efficient approach for large strings, but it's simple and clear
            std::string str_lower = to_lowercase(str.substr(0, prefix.length()));
            std::string prefix_lower = to_lowercase(prefix);
            
            return str_lower == prefix_lower;
        }
        
        /**
         * Converts a string to lowercase.
         * Useful for case-insensitive comparisons.
         */
        static std::string to_lowercase(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::tolower);
            return result;
        }
        
        /**
         * Converts a string to uppercase.
         * Sometimes useful for HTTP header formatting.
         */
        static std::string to_uppercase(const std::string& str) {
            std::string result = str;
            std::transform(result.begin(), result.end(), result.begin(), ::toupper);
            return result;
        }
        
        /**
         * Trims whitespace from the beginning and end of a string.
         * Very useful for processing HTTP headers and user input.
         * 
         * @param str The string to trim
         * @return A new string with leading and trailing whitespace removed
         */
        static std::string trim(const std::string& str) {
            // Find the first non-whitespace character
            auto start = std::find_if(str.begin(), str.end(), [](unsigned char ch) {
                return !std::isspace(ch);
            });
            
            // Find the last non-whitespace character
            auto end = std::find_if(str.rbegin(), str.rend(), [](unsigned char ch) {
                return !std::isspace(ch);
            }).base();
            
            // If the string is all whitespace, start will be past end
            if (start >= end) {
                return "";
            }
            
            return std::string(start, end);
        }
        
        /**
         * Splits a string by a delimiter character.
         * Useful for parsing HTTP headers or processing file paths.
         * 
         * @param str The string to split
         * @param delimiter The character to split on
         * @return A vector of string pieces
         * 
         * Example:
         *   split("image/jpeg,text/html", ',') → ["image/jpeg", "text/html"]
         */
        static std::vector<std::string> split(const std::string& str, char delimiter) {
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
            
            // Don't forget the last piece
            if (!current.empty()) {
                result.push_back(current);
            }
            
            return result;
        }
    };

} // namespace UTILS