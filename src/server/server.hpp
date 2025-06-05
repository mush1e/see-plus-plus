#pragma once

#include "../loop/event_loop.hpp"
#include "../threadpool/threadpool.hpp"
#include <thread>

namespace SERVER {

class Server {
public:
    Server(int num_workers = 4);
    ~Server();

    bool start(uint16_t port);
    void stop();
    void wait();

private:
    THREADPOOL::ThreadPool thread_pool_;
    LOOP::EventLoop event_loop_;
    std::thread event_thread_;
};

} // namespace SERVER
