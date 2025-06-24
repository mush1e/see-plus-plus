#include "thread_pool.hpp"

#include <iostream>

namespace EXECUTOR {

    ThreadPool::ThreadPool(uint16_t num_workers) {
        workers.reserve(num_workers);
        for (auto i = 0u; i < num_workers; i++) 
            workers.emplace_back(&ThreadPool::worker_function, this, i);
        std::cout << "ThreadPool initialized with " << num_workers << " workers." << std::endl;
    }

    ThreadPool::~ThreadPool() {
        shutdown();
    }

    void ThreadPool::enqueue_task(std::unique_ptr<Task> task) {
        // Lock queue mutex
        {
            std::lock_guard<std::mutex> lock(queue_mtx);
            task_queue.push(std::move(task));
        }
        queue_cv.notify_one();
    }


    void ThreadPool::worker_function(uint16_t worker_id) {

        for(;!should_stop;) {
            std::unique_ptr<Task> task;
            {
                std::unique_lock<std::mutex> lock(queue_mtx);
                queue_cv.wait(lock, [this] {
                    return !task_queue.empty() || should_stop;
                });

                if (should_stop && task_queue.empty())
                    break;

                task = std::move(task_queue.front());
                task_queue.pop();
            }

            if (task)
                task->execute(worker_id);
        }

        {
            std::unique_lock<std::mutex> lock(cout_mtx);
            std::cout << "Worker [" << worker_id << "] stopping." << std::endl;
        }
    }

    void ThreadPool::shutdown() {
        should_stop = true;
        queue_cv.notify_all();

        for (auto& worker : workers) {
            if (worker.joinable())
                worker.join();
        }

        std::cout << "ThreadPool shutdown complete." << std::endl;
    }
} // namespace EXECUTOR