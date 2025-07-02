#pragma once

#include "../executor/thread_pool.hpp"
#include "../reactor/event_loop.hpp"
#include "../core/router.hpp"
#include "../core/controller.hpp"

#include <memory>

namespace SERVER {

    // Server represents a class encapsulating the server 
    // behaviours for our backend. This is a wrapper around 
    // our event loop, router and threadpool. Also gives you 
    // the ability to add a route to our router
    class Server {
    public:
        Server(uint16_t port = 8080, uint16_t num_workers = 4);

        void add_route(const std::string& method, const std::string& path, 
                   std::shared_ptr<CORE::Controller> controller);

        // Server Lifetime Methods
        void start();
        void stop();
        bool is_running() const;

        // Server config methods
        void set_keep_alive(bool enabled);
        void set_request_timeout(int seconds);

    private:
        std::unique_ptr<CORE::Router> router {};
        std::unique_ptr<REACTOR::EventLoop> event_loop {};
        std::unique_ptr<EXECUTOR::ThreadPool> thread_pool {};
    };

} // namespace SERVER