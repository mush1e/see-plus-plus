#pragma once

#include "notifier.hpp"
#include "../executor/thread_pool.hpp"
#include "../core/router.hpp"
#include "../core/connection_manager.hpp"

#include <mutex>
#include <atomic>
#include <thread>
#include <chrono>

namespace REACTOR {
    static constexpr uint32_t FLAG_READ       = 1;
    static constexpr uint32_t FLAG_DISCONNECT = 2;
    static constexpr uint32_t FLAG_ERROR      = 3;

    class EventLoop {
    public:
        EventLoop(EXECUTOR::ThreadPool& thread_pool, CORE::Router &router);
        ~EventLoop();

        bool setup_server_socket(uint16_t port);
        void run();        
        void stop();
        void close_connection(int fd);

        void set_keep_alive_enabled(bool enabled);
    
    private:
        void handle_event(const EventData& event);
        void handle_new_connections();
        void handle_client_event(int fd, uint32_t events);
        void handle_client_disconnect(int fd);
        void cleanup_timed_out_connections();
        void cleanup_worker();
        int make_socket_nonblocking(int socket_fd);
        void send_error_response(int fd, int status_code, const std::string& status_text);

        std::unique_ptr<EventNotifier> notifier;
        EXECUTOR::ThreadPool* thread_pool;
        CORE::Router &router;
        CORE::ConnectionManager connection_manager;
        
        int server_socket = -1;
        std::atomic<bool> should_stop{false};
        std::atomic<bool> keep_alive_enabled{false};
        
        // Cleanup thread for connection timeouts
        std::thread cleanup_thread;
        std::atomic<bool> cleanup_should_stop{false};
    };

} // namespace REACTOR