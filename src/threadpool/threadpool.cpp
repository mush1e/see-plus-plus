#include "threadpool.hpp"
#include <iostream>

namespace THREADPOOL {

ThreadPool::ThreadPool(int num_workers) {
    for (int i = 0; i < num_workers; ++i) {
        workers_.emplace_back(&ThreadPool::worker_function, this, i);
    }
    std::cout << "ThreadPool initialized with " << num_workers << " workers." << std::endl;
}

ThreadPool::~ThreadPool() {
    shutdown();
}

void ThreadPool::enqueue_task(std::unique_ptr<Task> task) {
    {
        std::lock_guard<std::mutex> lock(queue_mutex_);
        task_queue_.push(std::move(task));
    }
    queue_cv_.notify_one();
}

void ThreadPool::worker_function(int worker_id) {

    while (!should_stop_) {
        std::unique_ptr<Task> task;
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] {
                return !task_queue_.empty() || should_stop_;
            });

            if (should_stop_ && task_queue_.empty())
                break;

            task = std::move(task_queue_.front());
            task_queue_.pop();
        }

        if (task)
            task->execute(worker_id);
    }

    std::cout << "Worker [" << worker_id << "] stopping." << std::endl;
}

void ThreadPool::shutdown() {
    should_stop_ = true;
    queue_cv_.notify_all();

    for (auto& worker : workers_) {
        if (worker.joinable())
            worker.join();
    }

    std::cout << "ThreadPool shutdown complete." << std::endl;
}

} // namespace THREADPOOL
