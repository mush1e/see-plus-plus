#pragma once 

#include "http.hpp"

namespace CORE {

    // Controller represents an abstract base class for a user to use 
    // to build their own controllers, each controller is associated 
    // with a handle function 
    class Controller {
    public:
        virtual ~Controller() = default;
        virtual void handle(const Request& req, Response& res) = 0;
    };

} // namespace CORE