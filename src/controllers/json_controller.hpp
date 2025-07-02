#pragma once
#include "../core/controller.hpp"

class JsonController : public CORE::Controller {
public:
    void handle(const CORE::Request& req, CORE::Response& res) override {
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = "application/json";
        res.body = R"({
    "message": "Hello from JSON API!",
    "method": ")" + req.method + R"(",
    "path": ")" + req.path + R"(",
    "timestamp": ")" + std::to_string(std::time(nullptr)) + R"("
})";
    }
};