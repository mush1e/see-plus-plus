#pragma once
#include "../core/controller.hpp"

class HelloController : public CORE::Controller {
public:
    void handle(const CORE::Request& req,
                CORE::Response& res) override
    {
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = "text/plain";
        res.body = "Hello from the HelloController!";
    }
};
