#pragma once
#include <memory>
#include "../core/types.hpp"

namespace THREADPOOL {

    // Abstract Task class
    class Task {
    public:
        virtual ~Task() = default;

        // Pure virtual function—must override in derived tasks
        virtual void execute(int worker_id) = 0;
    };

} // namespace THREADPOOL
