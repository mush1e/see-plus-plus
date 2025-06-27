#pragma once

#include "notifier.hpp"
#include "../executor/thread_pool.hpp"

namespace REACTOR {
    static constexpr uint32_t FLAG_READ       = 1;  // Flag for Readable    
    static constexpr uint32_t FLAG_DISCONNECT = 2;  // Flag for EOF/Closed
    static constexpr uint32_t FLAG_ERROR      = 3;  // Flag for error

    class EventLoop {
    public:
        EventLoop(EXECUTOR::ThreadPool* thread_pool);
        
    };

} // namespacr REACTOR