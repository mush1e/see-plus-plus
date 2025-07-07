#include <iostream>
#include "server/server.hpp"
#include "controllers/hello_controller.hpp"
#include "controllers/json_controller.hpp"

int main() {
    try {
        // Create server on port 8080 with 10 worker threads
        SERVER::Server server(8080, 10);
        
        // Add routes
        server.add_route("GET", "/", std::make_shared<HelloController>());
        server.add_route("GET", "/hello", std::make_shared<HelloController>());
        server.add_route("GET", "/api/status", std::make_shared<JsonController>());
        
        server.set_keep_alive(true);   // Enable HTTP/1.1 keep-alive
        server.set_request_timeout(60); // 60 second timeout for keep-alive connections
        
        std::cout << "=== Server Configuration ===" << std::endl;
        std::cout << "Keep-alive: ENABLED" << std::endl;
        std::cout << "Request timeout: 60 seconds" << std::endl;
        std::cout << "============================" << std::endl;
        
        // Start server (blocking call)
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}