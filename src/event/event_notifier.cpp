#include "event_notifier.hpp"
#include <unistd.h>
#include <errno.h>
#include <string.h>

namespace EVENT {

EventNotifier::EventNotifier() {
#ifdef USE_EPOLL
    epoll_fd = epoll_create1(EPOLL_CLOEXEC);
    if (epoll_fd == -1)
        throw std::runtime_error("epoll_create1 failed");
#elif defined(USE_KQUEUE)
    kqueue_fd = kqueue();
    if (kqueue_fd == -1)
        throw std::runtime_error("kqueue creation failed");
#endif
}

EventNotifier::~EventNotifier() {
#ifdef USE_EPOLL
    if (epoll_fd != -1)
        close(epoll_fd);
#elif defined(USE_KQUEUE)
    if (kqueue_fd != -1)
        close(kqueue_fd);
#endif
}

bool EventNotifier::add_fd(int fd, bool listen_for_read) {
#ifdef USE_EPOLL
    epoll_event event{};
    event.events = EPOLLET | EPOLLIN;
    if (!listen_for_read)
        event.events |= EPOLLOUT;
    event.data.fd = fd;

    return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) != -1;
#elif defined(USE_KQUEUE)
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
    return kevent(kqueue_fd, &event, 1, nullptr, 0, nullptr) != -1;
#endif
}

bool EventNotifier::remove_fd(int fd) {
#ifdef USE_EPOLL
    return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) != -1;
#elif defined(USE_KQUEUE)
    struct kevent event;
    EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
    return kevent(kqueue_fd, &event, 1, nullptr, 0, nullptr) != -1;
#endif
}

std::vector<EventData> EventNotifier::wait_for_events(int timeout_ms) {
    std::vector<EventData> events_list;

#ifdef USE_EPOLL
    epoll_event events[MAX_EVENTS];
    int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
    for (int i = 0; i < event_count; ++i) {
        events_list.push_back({events[i].data.fd, events[i].events});
    }
#elif defined(USE_KQUEUE)
    struct kevent events[MAX_EVENTS];
    timespec timeout {timeout_ms / 1000, (timeout_ms % 1000) * 1000000};
    int event_count = kevent(kqueue_fd, nullptr, 0, events, MAX_EVENTS, &timeout);
    for (int i = 0; i < event_count; ++i) {
        uint32_t flags = 0;
        if (events[i].filter == EVFILT_READ)
            flags |= 1; // Read available
        if (events[i].flags & EV_EOF)
            flags |= 2; // EOF detected
        if (events[i].flags & EV_ERROR)
            flags |= 4; // Error detected

        events_list.push_back({static_cast<int>(events[i].ident), flags});
    }
#endif

    return events_list;
}

} // namespace EVENT
