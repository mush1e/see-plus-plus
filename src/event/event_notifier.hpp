#pragma once

#include <vector>
#include <stdexcept>
#include <cstdint>

#ifdef __linux__
    #include <sys/epoll.h>
    #define USE_EPOLL
#elif defined(__APPLE__) || defined(__FreeBSD__)
    #include <sys/event.h>
    #include <sys/time.h>
    #define USE_KQUEUE
#else
    #error "Unsupported platform"
#endif

namespace EVENT {

struct EventData {
    int fd;
    uint32_t events;  // Bitmask of events (readable, writable, error)
};

class EventNotifier {
public:
    EventNotifier();
    ~EventNotifier();

    bool add_fd(int fd, bool listen_for_read = true);
    bool remove_fd(int fd);
    std::vector<EventData> wait_for_events(int timeout_ms = 1000);

private:
#ifdef USE_EPOLL
    int epoll_fd;
    static const int MAX_EVENTS = 64;
#elif defined(USE_KQUEUE)
    int kqueue_fd;
    static const int MAX_EVENTS = 64;
#endif
};

} // namespace EVENT
