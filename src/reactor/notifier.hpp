#pragma once

#ifdef __linux__
    #include <sys/epoll.h>
    #define USE_EPOLL
#elif defined(__APPLE__) || defined(__FreeBSD__) 
    #include <sys/event.h>
    #include <sys/time.h>
    #define USE_KQUEUE
#else
    #error "unsupported platform"
#endif

#include <vector>
#include <cstdint>  // for uint32_t

namespace REACTOR {

    // Event flags for cross-platform compatibility
    enum EventFlags : uint32_t {
        EVENT_READ   = 1 << 0,
        EVENT_WRITE  = 1 << 1,
        EVENT_ERROR  = 1 << 2,
        EVENT_HANGUP = 1 << 3
    };

    struct EventData {
        int fd {};
        uint32_t events {};     // Bitmask for events (Readable, Writable, Error)
    };

    class EventNotifier {
    public:
        EventNotifier();
        ~EventNotifier();

        // Start/stop monitoring a certain file descriptor
        bool add_fd(int fd, uint32_t event_flags = EVENT_READ);
        bool remove_fd(int fd);

        // Wait for events and return them
        std::vector<EventData> wait_for_events(int timeout_ms = 1000);

        // Check if notifier is valid
        bool is_valid() const;
    
    private:
       static const int MAX_EVENTS = 64;

        // Convert between platform specific flags to our crossplatform event flags
        uint32_t convert_to_platform_events(uint32_t event_flags);
        uint32_t convert_from_platform_events(uint32_t platform_events);

        // Platform specific shiz
        #ifdef USE_EPOLL
            int epoll_fd;
        #elif defined(USE_KQUEUE)
            int kqueue_fd;
        #endif
    };

} // namespace REACTOR