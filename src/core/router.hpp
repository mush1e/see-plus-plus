#pragma once

#include "controller.hpp"
#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <regex>

namespace CORE {

    struct RouteKey {
        std::string method;
        std::string path;
        
        bool operator==(const RouteKey& other) const {
            return method == other.method && path == other.path;
        }
    };

    // Custom hash for RouteKey
    struct RouteKeyHash {
        std::size_t operator()(const RouteKey& k) const {
            return std::hash<std::string>{}(k.method) ^ 
                   (std::hash<std::string>{}(k.path) << 1);
        }
    };

    // For pattern routes that need regex (use sparingly)
    struct PatternRoute {
        std::string method;
        std::regex path_regex;
        std::shared_ptr<Controller> controller;
    };

    class Router {
    public:
        // Add exact route (fast O(1) lookup)
        void add_route(const std::string& method, const std::string& path, 
                      std::shared_ptr<Controller> ctrl) {
            RouteKey key{method, path};
            exact_routes[key] = std::move(ctrl);
        }
        
        // Add pattern route (slower regex matching, use for wildcards only)
        void add_pattern_route(const std::string& method, const std::string& path_pattern, 
                              std::shared_ptr<Controller> ctrl) {
            pattern_routes.emplace_back(PatternRoute{method, std::regex(path_pattern), std::move(ctrl)});
        }

        bool route(const Request& req, Response& res) const {
            // First try exact match (O(1))
            RouteKey key{req.method, req.path};
            auto exact_it = exact_routes.find(key);
            if (exact_it != exact_routes.end()) {
                exact_it->second->handle(req, res);
                return true;
            }
            
            // Fall back to pattern matching (O(n))
            for (const auto& pattern_route : pattern_routes) {
                if (pattern_route.method == req.method && 
                    std::regex_match(req.path, pattern_route.path_regex)) {
                    pattern_route.controller->handle(req, res);
                    return true;
                }
            }
            
            return false;
        }

        // Add some useful utility routes
        void add_static_file_route(const std::string& prefix, 
                                 [[maybe_unused]] const std::string& directory) {
            // TODO: Implement static file serving
            // For now, just add a pattern route as placeholder
            // The directory parameter will be used when we implement static serving
            add_pattern_route("GET", prefix + "/(.*)", nullptr);
        }

    private:
        // Fast exact matches
        std::unordered_map<RouteKey, std::shared_ptr<Controller>, RouteKeyHash> exact_routes;
        
        // Slower pattern matches (use sparingly)
        std::vector<PatternRoute> pattern_routes;
    };

} // namespace CORE