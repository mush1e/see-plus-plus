#pragma once

#include "../executor/thread_pool.hpp"
#include "../reactor/event_loop.hpp"
#include "../core/router.hpp"
#include "../core/controller.hpp"

#include <memory>   // For uniqur_ptr
#include <thread>   // For std::thread
#include <atomic>   // For atomic
#include <csignal>  // For signal stuff
namespace SERVER {

    // Server represents a class encapsulating the server 
    // behaviours for our backend. This is a wrapper around 
    // our event loop, router and threadpool. Also gives you 
    // the ability to add a route to our router
    class Server {
    public:
        Server(uint16_t port = 8080, uint16_t num_workers = 4);
        ~Server();
        // Route Management
        void add_route(const std::string& method, const std::string& path, 
                   std::shared_ptr<CORE::Controller> controller);

        // Server Lifetime Methods
        void start();                   // Blocking call
        void stop();                    // Graceful shutdown
        bool is_running() const;        

        // Configuration
        void set_keep_alive(bool enabled) { keep_alive_enabled = enabled; }
        void set_request_timeout(int seconds) { request_timeout_seconds = seconds; }

    private:

        uint16_t server_port;
        std::atomic<bool> running{false};
        std::atomic<bool> should_stop{false};   

        std::unique_ptr<CORE::Router> router {};
        std::unique_ptr<REACTOR::EventLoop> event_loop {};
        std::unique_ptr<EXECUTOR::ThreadPool> thread_pool {};

        bool keep_alive_enabled {};
        int request_timeout_seconds {};

        // Server Thread
        std::thread server_thread;

        void server_main();
        void setup_signal_handlers();

        static std::atomic<Server*> instance;
        static void signal_handler(int sig);

    };

} // namespace SERVER