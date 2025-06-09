#pragma once

#include "../core/router.hpp"
#include "../loop/event_loop.hpp"
#include "../threadpool/threadpool.hpp"
#include <thread>

namespace SERVER {

class Server {
public:
    Server(int num_workers, CORE::Router &router);
    ~Server();

    bool start(uint16_t port);
    void stop();
    void wait();

private:
    THREADPOOL::ThreadPool thread_pool_;
    LOOP::EventLoop        event_loop_;
    CORE::Router           &router_;           
    std::thread            event_thread_;
};

} // namespace SERVER
