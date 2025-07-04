#include <iostream>
#include "server/server.hpp"
#include "controllers/hello_controller.hpp"
#include "controllers/json_controller.hpp"

int main() {
    try {
        // Create server on port 8080 with 4 worker threads
        SERVER::Server server(8080, 10);
        
        // Add routes
        server.add_route("GET", "/", std::make_shared<HelloController>());
        server.add_route("GET", "/hello", std::make_shared<HelloController>());
        server.add_route("GET", "/api/status", std::make_shared<JsonController>());
        
        // Configure server
        server.set_keep_alive(false);  // HTTP/1.0 style for now
        server.set_request_timeout(30);
        
        // Start server (blocking call)
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}