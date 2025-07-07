#include <iostream>
#include "server/server.hpp"
#include "controllers/hello_controller.hpp"
#include "controllers/json_controller.hpp"
#include "controllers/static_file_controller.hpp"
#include "controllers/test_body_controller.hpp"

int main() {
    try {
        // Create server on port 8080 with 10 worker threads
        SERVER::Server server(8080, 10);
        
        // Configure static file serving
        std::string document_root = "./public";
        auto static_controller = std::make_shared<StaticFileController>(document_root);
        
        // Add API routes (these get checked first)
        server.add_route("GET", "/hello", std::make_shared<HelloController>());
        server.add_route("GET", "/api/status", std::make_shared<JsonController>());
        
        // Add static file routes
        server.add_route("GET", "/", static_controller);
        server.add_route("GET", "/index.html", static_controller);
        
        server.add_route("POST", "/test/body", std::make_shared<TestBodyController>());
        server.add_route("PUT", "/test/body", std::make_shared<TestBodyController>());

        // Enable performance features
        server.set_keep_alive(true);
        server.set_request_timeout(60);
        
        // Display configuration
        std::cout << "=== see-plus-plus HTTP Server ===" << std::endl;
        std::cout << "Port: 8080" << std::endl;
        std::cout << "Workers: 10" << std::endl;
        std::cout << "Keep-alive: ENABLED" << std::endl;
        std::cout << "Static files: " << document_root << std::endl;
        std::cout << "=================================" << std::endl;
        std::cout << "🌐 Visit http://localhost:8080/" << std::endl;
        std::cout << "🔧 API: http://localhost:8080/api/status" << std::endl;
        std::cout << "👋 Test: http://localhost:8080/hello" << std::endl;
        std::cout << "Press Ctrl+C to stop" << std::endl;
        
        // Start server (blocking call)
        server.start();
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Server error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}