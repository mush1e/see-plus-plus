#pragma once
#include "http.hpp"
namespace CORE {

class Controller {
public:
    virtual ~Controller() = default;
    virtual void handle(const Request& req, Response& res) = 0;
};

} // namespace CORE
