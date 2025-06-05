#pragma once
#include "task.hpp"
#include <iostream>
#include <mutex>

namespace THREADPOOL {

    class SimpleTask : public Task {
    public:
        SimpleTask(int data) : data_(data) {}

        void execute(int worker_id) override {
            static std::mutex cout_mutex;  // Protect std::cout
            std::lock_guard<std::mutex> lock(cout_mutex);

            std::cout << "Worker [" << worker_id << "] executing task with data: " 
                      << data_ << std::endl;
        }

    private:
        int data_;
    };

} // namespace THREADPOOL
