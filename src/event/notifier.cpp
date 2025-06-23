#include "event_notifier.hpp"

#include <stdexcept>    // for std::runtime_error
#include <unistd.h>     // for close

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

    bool EventNotifier::is_valid() const {
        #ifdef USE_EPOLL
            return epoll_fd != -1;
        #elif defined(USE_KQUEUE)
            return kqueue_fd != -1;
        #endif
    }

    uint32_t EventNotifier::convert_to_platform_events(uint32_t event_flags) {
        #ifdef USE_EPOLL
            uint32_t epoll_events = 0;
            if (event_flags & EVENT_READ) epoll_events |= EPOLLIN;
            if (event_flags & EVENT_WRITE) epoll_events |= EPOLLOUT;
            if (event_flags & EVENT_ERROR) epoll_events |= EPOLLERR;
            if (event_flags & EVENT_HANGUP) epoll_events |= EPOLLHUP;
            epoll_events |= EPOLLET; // Use edge-triggered mode
            return epoll_events;
        #elif defined(USE_KQUEUE)
            // kqueue handles read/write separately, return the original flags
            return event_flags;
        #endif
    }

    uint32_t EventNotifier::convert_from_platform_events(uint32_t platform_events) {
        #ifdef USE_EPOLL
            uint32_t event_flags = 0;
            if (platform_events & EPOLLIN) event_flags |= EVENT_READ;
            if (platform_events & EPOLLOUT) event_flags |= EVENT_WRITE;
            if (platform_events & EPOLLERR) event_flags |= EVENT_ERROR;
            if (platform_events & EPOLLHUP) event_flags |= EVENT_HANGUP;
            return event_flags;
        #elif defined(USE_KQUEUE)
            // For kqueue, the conversion happens during event processing
            return platform_events;
        #endif
    }

    bool EventNotifier::add_fd(int fd, uint32_t event_flags) {
        if (!is_valid())
            return false;
        #ifdef USE_EPOLL
            epoll_event event{};
            event.events = convert_to_platform_events(event_flags);
            event.data.fd = fd;
            return epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) != -1;
        #elif defined(USE_KQUEUE)
            std::vector<struct kevent> events;

            // Add read filter if requested
            if (event_flags & EVENT_READ) {
                struct kevent read_event;
                EV_SET(&read_event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
                events.push_back(read_event);
            }

            // Add write filter if requested
            if (event_flags & EVENT_WRITE) {
                struct kevent write_event;
                EV_SET(&write_event, fd, EVFILT_WRITE, EV_ADD | EV_ENABLE, 0, 0, nullptr);
                events.push_back(write_event);
            }

            if (events.empty())
                return false;

            return kevent(kqueue_fd, events.data(), events.size(), nullptr, 0, nullptr) != -1;
        #endif
    }

    bool EventNotifier::remove_fd(int fd) {
        if (!is_valid()) 
            return false;
        
        #ifdef USE_EPOLL
            return epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) != -1;
            
        #elif defined(USE_KQUEUE)
            std::vector<struct kevent> events;
            
            // Remove both read and write filters (ignore errors if they don't exist)
            struct kevent read_event, write_event;
            EV_SET(&read_event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            EV_SET(&write_event, fd, EVFILT_WRITE, EV_DELETE, 0, 0, nullptr);
            
            events.push_back(read_event);
            events.push_back(write_event);
            
            // For kqueue, we don't strictly need to check return value since
            // deleting non-existent filters just returns an error but is harmless
            kevent(kqueue_fd, events.data(), events.size(), nullptr, 0, nullptr);
            return true;
        #endif
    }

    std::vector<EventData> EventNotifier::wait_for_events(int timeout_ms) {
        std::vector<EventData> result;
        
        if (!is_valid())
            return result;
        
        #ifdef USE_EPOLL
            epoll_event events[MAX_EVENTS];
            int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
            
            if (nfds > 0) {
                result.reserve(nfds);
                for (int i = 0; i < nfds; ++i) {
                    EventData event_data;
                    event_data.fd = events[i].data.fd;
                    event_data.events = convert_from_platform_events(events[i].events);
                    result.push_back(event_data);
                }
            }
            
        #elif defined(USE_KQUEUE)
            struct kevent events[MAX_EVENTS];
            struct timespec timeout;
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
            
            int nfds = kevent(kqueue_fd, nullptr, 0, events, MAX_EVENTS, 
                            timeout_ms >= 0 ? &timeout : nullptr);
            
            if (nfds > 0) {
                result.reserve(nfds);
                for (int i = 0; i < nfds; ++i) {
                    EventData event_data;
                    event_data.fd = static_cast<int>(events[i].ident);
                    
                    // Convert kqueue filter to our event flags
                    uint32_t event_flags = 0;
                    if (events[i].filter == EVFILT_READ) {
                        event_flags |= EVENT_READ;
                    } else if (events[i].filter == EVFILT_WRITE) {
                        event_flags |= EVENT_WRITE;
                    }
                    
                    // Check for error conditions
                    if (events[i].flags & EV_EOF) {
                        event_flags |= EVENT_HANGUP;
                    }
                    if (events[i].flags & EV_ERROR) {
                        event_flags |= EVENT_ERROR;
                    }
                    
                    event_data.events = event_flags;
                    result.push_back(event_data);
                }
            }
        #endif
            
        return result;
    }

} // namespace EVENT