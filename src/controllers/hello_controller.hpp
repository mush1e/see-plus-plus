#pragma once
#include "../core/controller.hpp"

class HelloController : public CORE::Controller {
public:
    void handle(const CORE::Request& req, CORE::Response& res) override {
        res.status_code = 200;
        res.status_text = "OK";
        res.headers["Content-Type"] = "text/html";
        res.body = R"(
<!DOCTYPE html>
<html>
<head><title>Hello World</title></head>
<body>
    <h1>Hello World!!</h1>
    <p>Your HTTP server is working!</p>
    <p>Request method: )" + req.method + R"(</p>
    <p>Request path: )" + req.path + R"(</p>
</body>
</html>)";
    }
};