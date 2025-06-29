#include "event_loop.hpp"

#include <iostream>
#include <sys/socket.h> // For the Berkeley sockets API
#include <unistd.h>     // For close
#include <fcntl.h>      // For file control shit
#include <arpa/inet.h>  // For sockaddr_in and stuff 

namespace REACTOR {

    int EventLoop::make_socket_nonblocking(int socket_fd) {
        // Retrieve the current flags on the socket file descriptor
        int flags = fcntl(socket_fd, F_GETFL, 0);

        // Setting the O_NONBLOCK flag on the socket while keeping 
        // existing flags intact.
        // This makes all future I/O operations on the socket non-blocking.
        // - read()/recv() will return immediately if there's no data rather than blocking
        // - wrtie()/send() will return immediately if the socket is not ready for writing
        // Usefull for us since we're doing event driven shit
        return fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK);
    }

    EventLoop::EventLoop(EXECUTOR::ThreadPool& threadpool, CORE::Router& r) : thread_pool(&threadpool), router(r) {
        this->notifier = std::make_unique<EventNotifier>();
    }

    EventLoop::~EventLoop() {
        if (server_socket != -1)
            close(server_socket);
    }

    bool EventLoop::setup_server_socket(uint16_t port) {
        // AF_INET     - represents the IPv4 Address Family
        // SOCK_STREAM - represents the TCP socket type
        // 0           - represents the use of the default protocol 
        server_socket = socket(AF_INET, SOCK_STREAM, 0);
        
        if (server_socket == -1)
            return false;
        
        // Allows the socket to be quickly reused after it is closed, 
        // avoiding the "Address is already in use" error.
        // Useful during development when we restart our server often 
        // since it allows the program to rebind to the same port without
        // waiting for the OS to release it
        int opt = 1;
        setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        if (make_socket_nonblocking(server_socket) == -1)
            return false;
        
        sockaddr_in addr {};
        addr.sin_family      = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons(port);            // Hardware to network byte order conversion

        if (bind(server_socket, (sockaddr*)&addr, sizeof(addr)) == -1)
            return false;
        
        if (listen(server_socket, 128) == -1)
            return false;

        return notifier->add_fd(server_socket);
    }

    void EventLoop::run() {
        std::cout << "🚀 Event loop started!" << std::endl;
        while (!should_stop) {
            auto events = notifier->wait_for_events();
            for (const auto& event : events) {
                handle_event(event);
            }
        }
    }

    void EventLoop::stop() {
        should_stop.store(false);
    }

    void EventLoop::handle_event(const EventData& event) {
        if (event.fd == server_socket)
            handle_new_connections();
        else
            handle_client_event(event.fd, event.events);
    }
} // namespace REACTOR