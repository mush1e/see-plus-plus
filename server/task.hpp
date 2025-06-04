#pragma once
#include <iostream>
#include <queue>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <string>
#include <condition_variable>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <map>
#include <memory>
#include <sys/socket.h>

// Platform-specific includes
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

namespace TRANSPORT {
    
    // Forward declaration
    class Task;
    
    class ThreadPool {
    public:
        ThreadPool(int num_workers = 4) {
            std::cout << "Creating thread pool with " << num_workers << " workers" << std::endl;
            
            // Create worker threads
            for (int i = 0; i < num_workers; ++i) {
                workers.emplace_back(&ThreadPool::worker_function, this, i);
            }
        }
        
        ~ThreadPool() {
            shutdown();
        }
        
        // Add a task to the queue - this is thread-safe and can be called from any thread
        void enqueue_task(std::unique_ptr<Task> task) {
            {
                std::lock_guard<std::mutex> lock(queue_mutex);
                task_queue.push(std::move(task));
                std::cout << "Task queued. Queue size: " << task_queue.size() << std::endl;
            }
            
            // Wake up one sleeping worker to handle this task
            queue_cv.notify_one();
        }
        
        // Get current queue size - useful for monitoring
        size_t get_queue_size() {
            std::lock_guard<std::mutex> lock(queue_mutex);
            return task_queue.size();
        }
        
        // Get statistics
        int get_tasks_processed() { return total_tasks_processed.load(); }
        int get_active_workers() { return active_workers.load(); }
        
        void shutdown() {
            std::cout << "Shutting down thread pool..." << std::endl;
            
            // Signal all workers to stop
            should_stop.store(true);
            queue_cv.notify_all();
            
            // Wait for all workers to finish
            for (auto& worker : workers) {
                if (worker.joinable()) {
                    worker.join();
                }
            }
            
            std::cout << "Thread pool shutdown complete. Total tasks processed: " 
                      << total_tasks_processed.load() << std::endl;
        }
        
    private:
        // This is the function that each worker thread runs
        void worker_function(int worker_id) {
            std::cout << "Worker " << worker_id << " started" << std::endl;
            
            while (!should_stop.load()) {
                std::unique_ptr<Task> task = nullptr;
                
                // Try to get a task from the queue
                {
                    std::unique_lock<std::mutex> lock(queue_mutex);
                    
                    // Wait until there's a task available or we're told to stop
                    queue_cv.wait(lock, [this] { 
                        return !task_queue.empty() || should_stop.load(); 
                    });
                    
                    // If we're stopping and no tasks left, exit
                    if (should_stop.load() && task_queue.empty()) {
                        break;
                    }
                    
                    // Get a task if available
                    if (!task_queue.empty()) {
                        task = std::move(task_queue.front());
                        task_queue.pop();
                    }
                }
                
                // Process the task outside the lock
                if (task) {
                    active_workers.fetch_add(1);
                    task->execute(worker_id);
                    active_workers.fetch_sub(1);
                    total_tasks_processed.fetch_add(1);
                }
            }
            
            std::cout << "Worker " << worker_id << " finished" << std::endl;
        }

    private:
        // The task queue - this is where all work items wait to be processed
        std::queue<std::unique_ptr<Task>> task_queue;
        
        // Synchronization primitives for the queue
        std::mutex queue_mutex;           // Protects the task queue from race conditions
        std::condition_variable queue_cv; // Allows workers to sleep until work is available
        
        // Worker threads
        std::vector<std::thread> workers;
        
        // Control flags
        std::atomic<bool> should_stop{false};
        std::atomic<int> active_workers{0};
        
        // Statistics for monitoring
        std::atomic<int> total_tasks_processed{0};
    };

     // Connection state that persists across multiple tasks
    struct ConnectionState {
        int socket_fd;
        std::string client_ip;
        uint16_t client_port;
        std::chrono::steady_clock::time_point last_activity;
        
        // Protocol state - starts as HTTP, might upgrade to WebSocket
        enum Protocol { HTTP, WEBSOCKET } protocol = HTTP;
        
        // HTTP parsing state
        std::string http_buffer;  // Accumulates partial HTTP requests
        bool http_headers_complete = false;
        
        // WebSocket state (for future use)
        bool websocket_handshake_complete = false;
        
        ConnectionState(int fd, const std::string& ip, uint16_t port) 
            : socket_fd(fd), client_ip(ip), client_port(port),
              last_activity(std::chrono::steady_clock::now()) {}
    };

    // Enhanced task system with inheritance
    class Task {
    public:
        enum Type { NEW_CONNECTION, HTTP_TASK, WEBSOCKET_TASK, CLEANUP };
        
        Type type;
        int client_socket;
        std::string client_ip;
        std::chrono::steady_clock::time_point time_created;
        
        // Shared connection state reference
        std::shared_ptr<ConnectionState> connection;
        
        Task(Type t, int socket, const std::string& ip, 
             std::shared_ptr<ConnectionState> conn_state = nullptr) 
            : type(t), client_socket(socket), client_ip(ip),
              time_created(std::chrono::steady_clock::now()),
              connection(conn_state) {}
        
        virtual ~Task() = default;
        
        // Virtual method that specialized tasks can override
        virtual void execute(int worker_id) = 0;
    };

    // HTTP-specific task for handling HTTP requests
    class HttpTask : public Task {
    public:
        std::string http_data;  // The actual HTTP request data
        
        HttpTask(int socket, const std::string& ip, 
                std::shared_ptr<ConnectionState> conn_state,
                const std::string& data)
            : Task(HTTP_TASK, socket, ip, conn_state), http_data(data) {}
        
        void execute(int worker_id) override {
            std::cout << "Worker " << worker_id << " processing HTTP request from " 
                      << client_ip << std::endl;
            
            // Parse HTTP request and generate appropriate response
            process_http_request();
        }
        
    private:
        void process_http_request() {
            // Simple HTTP response
            std::string response = 
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html\r\n"
                "Content-Length: 13\r\n"
                "\r\n"
                "Hello, World!";
            
            send(client_socket, response.c_str(), response.length(), 0);
        }
    };

    // WebSocket-specific task (placeholder for future implementation)
    class WebSocketTask : public Task {
    public:
        std::vector<uint8_t> websocket_frame;  // Raw WebSocket frame data
        
        WebSocketTask(int socket, const std::string& ip,
                     std::shared_ptr<ConnectionState> conn_state,
                     const std::vector<uint8_t>& frame_data)
            : Task(WEBSOCKET_TASK, socket, ip, conn_state), websocket_frame(frame_data) {}
        
        void execute(int worker_id) override {
            std::cout << "Worker " << worker_id << " processing WebSocket frame from " 
                      << client_ip << std::endl;
            
            // Process WebSocket frame
            process_websocket_frame();
        }
        
    private:
        void process_websocket_frame() {
            // TODO: Implement WebSocket frame parsing and processing
        }
    };

    // Cross-platform event notification abstraction
    class EventNotifier {
    public:
        struct EventData {
            int fd;
            uint32_t events;
        };
        
    private:
#ifdef USE_EPOLL
        int epoll_fd;
        static const int MAX_EVENTS = 64;
#elif defined(USE_KQUEUE)
        int kqueue_fd;
        static const int MAX_EVENTS = 64;
#endif
        
    public:
        EventNotifier() {
#ifdef USE_EPOLL
            epoll_fd = epoll_create1(EPOLL_CLOEXEC);
            if (epoll_fd == -1) {
                perror("epoll_create1 failed");
                throw std::runtime_error("Failed to create epoll instance");
            }
#elif defined(USE_KQUEUE)
            kqueue_fd = kqueue();
            if (kqueue_fd == -1) {
                perror("kqueue failed");
                throw std::runtime_error("Failed to create kqueue instance");
            }
#endif
            std::cout << "Event notifier created successfully" << std::endl;
        }
        
        ~EventNotifier() {
#ifdef USE_EPOLL
            if (epoll_fd != -1) {
                close(epoll_fd);
            }
#elif defined(USE_KQUEUE)
            if (kqueue_fd != -1) {
                close(kqueue_fd);
            }
#endif
        }
        
        bool add_fd(int fd, bool listen_for_read = true) {
#ifdef USE_EPOLL
            epoll_event event{};
            event.events = EPOLLIN | EPOLLET;  // Edge-triggered mode
            if (!listen_for_read) {
                event.events |= EPOLLOUT;
            }
            event.data.fd = fd;
            
            if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event) == -1) {
                perror("epoll_ctl: add failed");
                return false;
            }
#elif defined(USE_KQUEUE)
            struct kevent event;
            EV_SET(&event, fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);
            
            if (kevent(kqueue_fd, &event, 1, nullptr, 0, nullptr) == -1) {
                perror("kevent: add failed");
                return false;
            }
#endif
            return true;
        }
        
        bool remove_fd(int fd) {
#ifdef USE_EPOLL
            if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, nullptr) == -1) {
                perror("epoll_ctl: delete failed");
                return false;
            }
#elif defined(USE_KQUEUE)
            struct kevent event;
            EV_SET(&event, fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
            
            if (kevent(kqueue_fd, &event, 1, nullptr, 0, nullptr) == -1) {
                perror("kevent: delete failed");
                return false;
            }
#endif
            return true;
        }
        
        std::vector<EventData> wait_for_events(int timeout_ms = 1000) {
            std::vector<EventData> result;
            
#ifdef USE_EPOLL
            epoll_event events[MAX_EVENTS];
            int event_count = epoll_wait(epoll_fd, events, MAX_EVENTS, timeout_ms);
            
            if (event_count == -1) {
                if (errno != EINTR) {
                    perror("epoll_wait failed");
                }
                return result;
            }
            
            for (int i = 0; i < event_count; ++i) {
                EventData data;
                data.fd = events[i].data.fd;
                data.events = events[i].events;
                result.push_back(data);
            }
            
#elif defined(USE_KQUEUE)
            struct kevent events[MAX_EVENTS];
            struct timespec timeout;
            timeout.tv_sec = timeout_ms / 1000;
            timeout.tv_nsec = (timeout_ms % 1000) * 1000000;
            
            int event_count = kevent(kqueue_fd, nullptr, 0, events, MAX_EVENTS, &timeout);
            
            if (event_count == -1) {
                if (errno != EINTR) {
                    perror("kevent failed");
                }
                return result;
            }
            
            for (int i = 0; i < event_count; ++i) {
                EventData data;
                data.fd = static_cast<int>(events[i].ident);
                data.events = 0;
                
                if (events[i].filter == EVFILT_READ) {
                    data.events |= 1;  // Read available
                }
                if (events[i].flags & EV_EOF) {
                    data.events |= 2;  // Connection closed
                }
                if (events[i].flags & EV_ERROR) {
                    data.events |= 4;  // Error occurred
                }
                
                result.push_back(data);
            }
#endif
            
            return result;
        }
    };

    // The cross-platform event loop that monitors all connections
    class EventLoop {
    private:
        std::unique_ptr<EventNotifier> notifier;
        int server_socket;
        
        // Connection state management
        std::map<int, std::shared_ptr<ConnectionState>> connections;
        std::mutex connections_mutex;  // Protects the connections map
        
        // Reference to our thread pool for task processing
        ThreadPool* thread_pool;
        
        // Control flags
        std::atomic<bool> should_stop{false};
        
    public:
        EventLoop(ThreadPool* pool) : thread_pool(pool) {
            notifier = std::make_unique<EventNotifier>();
            std::cout << "Event loop created successfully" << std::endl;
        }
        
        ~EventLoop() {
            cleanup();
        }
        
        bool setup_server_socket(uint16_t port) {
            // Create and configure the server socket
            server_socket = socket(AF_INET, SOCK_STREAM, 0);
            if (server_socket == -1) {
                perror("Failed to create server socket");
                return false;
            }
            
            // Set socket options for development
            int opt = 1;
            if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
                perror("setsockopt failed");
                close(server_socket);
                return false;
            }
            
            // Make the server socket non-blocking
            if (make_socket_nonblocking(server_socket) == -1) {
                close(server_socket);
                return false;
            }
            
            // Bind and listen
            sockaddr_in server_addr{};
            server_addr.sin_family = AF_INET;
            server_addr.sin_addr.s_addr = INADDR_ANY;
            server_addr.sin_port = htons(port);
            
            if (bind(server_socket, (sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
                perror("bind failed");
                close(server_socket);
                return false;
            }
            
            if (listen(server_socket, 128) == -1) {  // Larger backlog for high load
                perror("listen failed");
                close(server_socket);
                return false;
            }
            
            // Add server socket to event notifier for monitoring new connections
            if (!notifier->add_fd(server_socket)) {
                close(server_socket);
                return false;
            }
            
            std::cout << "Server socket bound to port " << port << " and added to event notifier" << std::endl;
            return true;
        }
        
        // Main event loop - this is the heart of your server
        void run() {
            std::cout << "Starting event loop..." << std::endl;
            
            while (!should_stop.load()) {
                // Wait for events on any monitored file descriptors
                auto events = notifier->wait_for_events(1000);  // 1 second timeout
                
                // Process each event that occurred
                for (const auto& event : events) {
                    handle_event(event);
                }
                
                // Periodically clean up idle connections
                cleanup_idle_connections();
            }
            
            std::cout << "Event loop finished" << std::endl;
        }
        
        void stop() {
            should_stop.store(true);
        }
        
    private:
        void handle_event(const EventNotifier::EventData& event) {
            int fd = event.fd;
            
            if (fd == server_socket) {
                // New connection arriving
                handle_new_connections();
            } else {
                // Data available on existing connection
                handle_client_event(fd, event.events);
            }
        }
        
        void handle_new_connections() {
            // Accept all pending connections
            while (true) {
                sockaddr_in client_addr{};
                socklen_t client_len = sizeof(client_addr);
                
                int client_fd = accept(server_socket, (sockaddr*)&client_addr, &client_len);
                if (client_fd == -1) {
                    if (errno == EAGAIN || errno == EWOULDBLOCK) {
                        // No more connections to accept
                        break;
                    }
                    perror("accept failed");
                    continue;
                }
                
                // Make client socket non-blocking
                if (make_socket_nonblocking(client_fd) == -1) {
                    close(client_fd);
                    continue;
                }
                
                // Get client IP address
                char client_ip[INET_ADDRSTRLEN];
                inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
                uint16_t client_port = ntohs(client_addr.sin_port);
                
                // Create connection state
                auto connection_state = std::make_shared<ConnectionState>(client_fd, client_ip, client_port);
                
                // Add to event notifier for monitoring
                if (!notifier->add_fd(client_fd)) {
                    close(client_fd);
                    continue;
                }
                
                // Store connection state
                {
                    std::lock_guard<std::mutex> lock(connections_mutex);
                    connections[client_fd] = connection_state;
                }
                
                std::cout << "New connection from " << client_ip << ":" << client_port 
                          << " (fd=" << client_fd << ")" << std::endl;
            }
        }
        
        void handle_client_event(int client_fd, uint32_t events) {
            // Get connection state
            std::shared_ptr<ConnectionState> connection;
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                auto it = connections.find(client_fd);
                if (it == connections.end()) {
                    std::cout << "Event for unknown connection fd=" << client_fd << std::endl;
                    return;
                }
                connection = it->second;
            }
            
            if (events & 1) {  // Read available
                handle_client_data(connection);
            }
            
            if (events & (2 | 4)) {  // Connection closed or error
                handle_client_disconnect(client_fd);
            }
        }
        
        void handle_client_data(std::shared_ptr<ConnectionState> connection) {
            char buffer[4096];
            ssize_t bytes_read;
            
            // Read all available data
            while ((bytes_read = recv(connection->socket_fd, buffer, sizeof(buffer), 0)) > 0) {
                connection->last_activity = std::chrono::steady_clock::now();
                
                // Determine what type of task to create based on protocol state
                if (connection->protocol == ConnectionState::HTTP) {
                    // Accumulate HTTP data
                    connection->http_buffer.append(buffer, bytes_read);
                    
                    // Check if we have a complete HTTP request
                    if (is_http_request_complete(connection->http_buffer)) {
                        // Create HTTP task
                        auto http_task = std::make_unique<HttpTask>(
                            connection->socket_fd, connection->client_ip, 
                            connection, connection->http_buffer);
                        
                        // Add to thread pool
                        thread_pool->enqueue_task(std::move(http_task));
                        
                        connection->http_buffer.clear();
                    }
                } else if (connection->protocol == ConnectionState::WEBSOCKET) {
                    // Handle WebSocket frame
                    std::vector<uint8_t> frame_data(buffer, buffer + bytes_read);
                    
                    auto ws_task = std::make_unique<WebSocketTask>(
                        connection->socket_fd, connection->client_ip,
                        connection, frame_data);
                    
                    thread_pool->enqueue_task(std::move(ws_task));
                }
            }
            
            if (bytes_read == 0) {
                // Connection closed gracefully
                handle_client_disconnect(connection->socket_fd);
            } else if (bytes_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK) {
                // Error occurred
                perror("recv failed");
                handle_client_disconnect(connection->socket_fd);
            }
        }
        
        void handle_client_disconnect(int client_fd) {
            std::cout << "Client disconnected (fd=" << client_fd << ")" << std::endl;
            
            // Remove from event notifier
            notifier->remove_fd(client_fd);
            
            // Close socket
            close(client_fd);
            
            // Remove from connections map
            {
                std::lock_guard<std::mutex> lock(connections_mutex);
                connections.erase(client_fd);
            }
        }
        
        void cleanup_idle_connections() {
            auto now = std::chrono::steady_clock::now();
            auto timeout_duration = std::chrono::seconds(30);
            
            std::lock_guard<std::mutex> lock(connections_mutex);
            for (auto it = connections.begin(); it != connections.end(); ) {
                auto duration = now - it->second->last_activity;
                if (duration > timeout_duration) {
                    std::cout << "Cleaning up idle connection " << it->second->client_ip << std::endl;
                    notifier->remove_fd(it->first);
                    close(it->first);
                    it = connections.erase(it);
                } else {
                    ++it;
                }
            }
        }
        
        int make_socket_nonblocking(int socket_fd) {
            int flags = fcntl(socket_fd, F_GETFL, 0);
            if (flags == -1) {
                perror("fcntl F_GETFL");
                return -1;
            }
            
            if (fcntl(socket_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
                perror("fcntl F_SETFL O_NONBLOCK");
                return -1;
            }
            
            return 0;
        }
        
        bool is_http_request_complete(const std::string& buffer) {
            // Simple check for complete HTTP request (headers end with \r\n\r\n)
            return buffer.find("\r\n\r\n") != std::string::npos;
        }
        
        void cleanup() {
            if (server_socket != -1) {
                close(server_socket);
                server_socket = -1;
            }
        }
    };

    // Simple server class that ties everything together
    class Server {
    private:
        ThreadPool thread_pool;
        EventLoop event_loop;
        std::thread event_thread;
        
    public:
        Server(int num_workers = 4) 
            : thread_pool(num_workers), event_loop(&thread_pool) {}
        
        bool start(uint16_t port) {
            if (!event_loop.setup_server_socket(port)) {
                return false;
            }
            
            // Start the event loop in a separate thread
            event_thread = std::thread([this]() {
                event_loop.run();
            });
            
            std::cout << "Server started on port " << port << std::endl;
            return true;
        }
        
        void stop() {
            event_loop.stop();
            if (event_thread.joinable()) {
                event_thread.join();
            }
        }
        
        void wait() {
            if (event_thread.joinable()) {
                event_thread.join();
            }
        }
    };

} // namespace TRANSPORT