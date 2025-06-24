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
        ThreadPool(int num_workers = 4);
        ~ThreadPool();
        bool enqueue_task(std::unique_ptr<Task> task);
        bool shutdown();

    private:
        std::atomic<bool> should_stop {};
        std::vector<std::thread> workers {};
        std::queue<Task> task_queue;
        std::condition_variable queue_cv;
        std::mutex queue_mtx;
        std::mutex cout_mtx;    // cout is not threadsafe

        // worker function to be executed by each worker thread
        void worker_function(int worker_id);    
    };

} // namespace EXECUTOR
