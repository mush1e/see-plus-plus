#include "server.hpp"

#include <iostream>
namespace SERVER {

    std::atomic<Server*> Server::instance{nullptr};

    Server::Server(uint16_t port, uint16_t num_workers) 
        : server_port(port) {
        
        // Initialize components
        thread_pool = std::make_unique<EXECUTOR::ThreadPool>(num_workers);
        router = std::make_unique<CORE::Router>();
        event_loop = std::make_unique<REACTOR::EventLoop>(*thread_pool, *router);
        
        // Set up signal handling
        instance.store(this);
        setup_signal_handlers();
        
        std::cout << "Server initialized on port " << port 
                  << " with " << num_workers << " workers" << std::endl;
    }

    Server::~Server() {
        if (running.load()) {
            stop();
        }
        instance.store(nullptr);
    }

    void Server::add_route(const std::string& method, const std::string& path, 
                          std::shared_ptr<CORE::Controller> controller) {
        router->add_route(method, path, controller);
        std::cout << "Route added: " << method << " " << path << std::endl;
    }

    void Server::start() {
        if (running.load()) {
            std::cout << "Server is already running!" << std::endl;
            return;
        }
        
        std::cout << "🚀 Starting server on port " << server_port << "..." << std::endl;
        
        // Setup server socket
        if (!event_loop->setup_server_socket(server_port)) {
            throw std::runtime_error("Failed to setup server socket on port " + std::to_string(server_port));
        }
        
        running.store(true);
        should_stop.store(false);
        
        std::cout << "✅ Server started successfully!" << std::endl;
        std::cout << "📡 Listening on http://localhost:" << server_port << std::endl;
        std::cout << "Press Ctrl+C to stop the server" << std::endl;
        
        // Run event loop (blocking)
        event_loop->run();
        
        std::cout << "🛑 Server stopped" << std::endl;
        running.store(false);
    }

    void Server::stop() {
        if (!running.load()) {
            return;
        }
        
        std::cout << "\n🛑 Shutting down server..." << std::endl;
        should_stop.store(true);
        
        if (event_loop) {
            event_loop->stop();
        }
        
        if (thread_pool) {
            thread_pool->shutdown();
        }
        
        running.store(false);
        std::cout << "✅ Server shutdown complete" << std::endl;
    }

    bool Server::is_running() const {
        return running.load();
    }

    void Server::setup_signal_handlers() {
        std::signal(SIGINT, signal_handler);
        std::signal(SIGTERM, signal_handler);
    }

    void Server::signal_handler(int sig) {
        Server* server_instance = instance.load();
        if (server_instance) {
            switch (sig) {
                case SIGINT:
                    std::cout << "\nReceived SIGINT (Ctrl+C)" << std::endl;
                    break;
                case SIGTERM:
                    std::cout << "\nReceived SIGTERM" << std::endl;
                    break;
            }
            server_instance->stop();
        }
    }

} // namespace SERVER