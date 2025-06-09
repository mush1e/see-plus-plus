#pragma once
#include "controller.hpp"
#include <vector>
#include <regex>
#include <memory>

namespace CORE {

struct RouteEntry {
    std::string method;        // "GET", "POST", etc.
    std::regex  path_regex;    // e.g. std::regex{"^/users/(\\d+)$"}
    std::shared_ptr<Controller> controller;
};

class Router {
public:
    void add_route(const std::string& method,
                   const std::string& path_pattern,
                   std::shared_ptr<Controller> ctrl)
    {
        routes_.push_back({method, std::regex(path_pattern), std::move(ctrl)});
    }

    // Returns true if matched+handled, false → 404
    bool route(const Request& req, Response& res) const {
        for (auto& r : routes_) {
            if (r.method == req.method
                && std::regex_match(req.path, r.path_regex))
            {
                r.controller->handle(req, res);
                return true;
            }
        }
        return false;
    }

private:
    std::vector<RouteEntry> routes_;
};

} // namespace CORE
