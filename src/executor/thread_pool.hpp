#pragma once

#include "base/task.hpp"

#include <vector>
#include <thread>
#include <atomic>
#include <queue>
#include <mutex>
#include <memory>
#include <condition_variable>

namespace EXECUTOR {

    class ThreadPool {
    public:
        ThreadPool(uint16_t num_workers = 4);
        ~ThreadPool();
        void enqueue_task(std::unique_ptr<Task> task);
        void shutdown();

    private:
        std::atomic<bool> should_stop {};
        std::vector<std::thread> workers {};
        std::queue<std::unique_ptr<Task>> task_queue;
        std::condition_variable queue_cv;
        std::mutex queue_mtx;
        std::mutex cout_mtx;    // cout is not threadsafe

        // worker function to be executed by each worker thread
        void worker_function(uint16_t worker_id);    
    };

} // namespace EXECUTOR
