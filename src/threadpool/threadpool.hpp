#pragma once

#include "task.hpp"

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <atomic>
#include <memory>
#include <condition_variable>

namespace THREADPOOL {

    class ThreadPool {
    public:
        ThreadPool(int num_workers = 4);
        ~ThreadPool();

        void enqueue_task(std::unique_ptr<Task> task);
        void shutdown();

    private:
        void worker_function(int worker_id);

        std::queue<std::unique_ptr<Task>> task_queue_;
        std::vector<std::thread> workers_;
        std::mutex queue_mutex_;
        std::condition_variable queue_cv_;
        std::atomic<bool> should_stop_{false};
    };

} // namespace THREADPOOL