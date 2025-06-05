#pragma once

#include "../event/event_notifier.hpp"
#include "../threadpool/threadpool.hpp"
#include "../core/types.hpp"

#include <map>
#include <atomic>
#include <mutex>

namespace LOOP {

class EventLoop {
public:
    EventLoop(THREADPOOL::ThreadPool* thread_pool);
    ~EventLoop();

    bool setup_server_socket(uint16_t port);
    void run();
    void stop();

private:
    void handle_event(const EVENT::EventData& event);
    void handle_new_connections();
    void handle_client_event(int fd, uint32_t events);
    void handle_client_disconnect(int fd);
    int make_socket_nonblocking(int socket_fd);

    std::unique_ptr<EVENT::EventNotifier> notifier_;
    THREADPOOL::ThreadPool* thread_pool_;
    int server_socket_;

    std::atomic<bool> should_stop_{false};

    // Manage active connections
    std::map<int, std::shared_ptr<CORE::ConnectionState>> connections_;
    std::mutex connections_mutex_;
};

} // namespace LOOP
