#pragma once

#include "notifier.hpp"
#include "../executor/thread_pool.hpp"
#include "../core/router.hpp"
#include "../core/types.hpp"

#include <mutex>
#include <atomic>
#include <map>
namespace REACTOR {
    static constexpr uint32_t FLAG_READ       = 1;  // Flag for Readable    
    static constexpr uint32_t FLAG_DISCONNECT = 2;  // Flag for EOF/Closed
    static constexpr uint32_t FLAG_ERROR      = 3;  // Flag for error

    class EventLoop {
    public:
        EventLoop(EXECUTOR::ThreadPool& thread_pool, CORE::Router &router);
        ~EventLoop();

        bool setup_server_socket(uint16_t port);
        void run();        
        void stop();
    
    private:
        void handle_event(const EventData& event);
        void handle_new_connections();
        void handle_client_event(int fd, uint32_t events);
        void handle_client_disconnect(int fd);
        int make_socket_nonblocking(int socket_fd);

        std::unique_ptr<EventNotifier> notifier;
        EXECUTOR::ThreadPool* thread_pool;
        CORE::Router &router;
        int server_socket {};
        std::atomic<bool> should_stop {};

        // Manage active connections
        std::map<int, std::shared_ptr<CORE::ConnectionState>> connections_;
        std::mutex connections_mutex_;

    };

} // namespace REACTOR