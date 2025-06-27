#pragma once

#include "controller.hpp"

#include <string>
#include <regex>
#include <vector>

namespace CORE {


    struct Route {
        std::string                 method     {};
        std::regex                  path_regex {};
        std::shared_ptr<Controller> controller {};
    };

    class Router {
    public:
        void add_route(const std::string& method, const std::string& path_pattern, std::shared_ptr<Controller> ctrl) {
            registered_routes.push_back({method, std::regex(path_pattern), std::move(ctrl)});
        }

        bool route(const Request& req, Response& res) const {
            for (auto& r : registered_routes) 
                if (r.method == req.method && std::regex_match(req.path, r.path_regex)) {
                    r.controller->handle(req, res);
                    return true;
                }
            return false;
        }

    private:
        std::vector<Route> registered_routes {};
    };

} // namespace CORE