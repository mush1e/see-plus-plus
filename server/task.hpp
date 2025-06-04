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
#include <unistd.h>         // For close() function
#include <netinet/in.h>     // For sockaddr_in structure


namespace TRANSPORT {

    // Task signifies a job to be completed which could be of type ,
    // new connection, send something, recieve something, and cleanup
    struct Task {
        enum Type {
            NEW_CONNECTION,
            READ_DATA,
            SEND_RESPONSE,
            CLEANUP
        };
        
        Type type;
        int client_socket;
        std::string client_ip;
        std::chrono::steady_clock::time_point time_created;
        
        // Optional data for different task types
        std::string response_data;  // Used by SEND_RESPONSE tasks
        sockaddr_in client_addr;    // Used by NEW_CONNECTION tasks
        
        // Constructor to automatically set creation time
        Task(Type t, int socket, const std::string& ip) 
            : type(t), client_socket(socket), client_ip(ip), 
              time_created(std::chrono::steady_clock::now()) {}
    };

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
        void enqueue_task(Task task) {
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
                Task task{Task::CLEANUP, -1, ""};  // Initialize with dummy values
                bool got_task = false;
                
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
                        got_task = true;
                    }
                }
                
                // Process the task outside the lock
                if (got_task) {
                    active_workers.fetch_add(1);
                    process_task(task, worker_id);
                    active_workers.fetch_sub(1);
                    total_tasks_processed.fetch_add(1);
                }
            }
            
            std::cout << "Worker " << worker_id << " finished" << std::endl;
        }
        
        // This is where the actual work happens
        void process_task(const Task& task, int worker_id) {
            auto task_start = std::chrono::steady_clock::now();
            
            std::cout << "Worker " << worker_id << " processing task type " 
                      << task.type << " for client " << task.client_ip << std::endl;
            
            switch (task.type) {
                case Task::NEW_CONNECTION:
                    handle_new_connection(task, worker_id);
                    break;
                    
                case Task::READ_DATA:
                    handle_read_data(task, worker_id);
                    break;
                    
                case Task::SEND_RESPONSE:
                    handle_send_response(task, worker_id);
                    break;
                    
                case Task::CLEANUP:
                    handle_cleanup(task, worker_id);
                    break;
            }
            
            auto task_end = std::chrono::steady_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(task_end - task_start);
            std::cout << "Worker " << worker_id << " completed task in " 
                      << duration.count() << "ms" << std::endl;
        }
        
        // Task handlers - these will contain your actual business logic
        void handle_new_connection(const Task& task, int worker_id) {
            // Set up socket timeouts, log the connection, etc.
            // This is similar to the setup part of your current handle_client_threaded
            std::cout << "Setting up new connection from " << task.client_ip << std::endl;
            
            // TODO: Set socket options, timeouts, etc.
            // TODO: Add connection to monitoring system
        }
        
        void handle_read_data(const Task& task, int worker_id) {
            // Read data from the socket
            char buffer[1024];
            ssize_t bytes_recv = recv(task.client_socket, buffer, sizeof(buffer)-1, 0);
            
            if (bytes_recv > 0) {
                buffer[bytes_recv] = '\0';
                std::string message(buffer);
                std::cout << "Worker " << worker_id << " read: " << message 
                          << " from " << task.client_ip << std::endl;
                
                // Create a response task
                Task response_task(Task::SEND_RESPONSE, task.client_socket, task.client_ip);
                response_task.response_data = "Echo: " + message + "\n";
                
                // Add the response task back to the queue
                enqueue_task(std::move(response_task));
            } else {
                // Connection closed or error - create cleanup task
                Task cleanup_task(Task::CLEANUP, task.client_socket, task.client_ip);
                enqueue_task(std::move(cleanup_task));
            }
        }
        
        void handle_send_response(const Task& task, int worker_id) {
            // Send the response data to the client
            ssize_t bytes_sent = send(task.client_socket, 
                                    task.response_data.c_str(), 
                                    task.response_data.length(), 0);
            
            if (bytes_sent > 0) {
                std::cout << "Worker " << worker_id << " sent response to " 
                          << task.client_ip << std::endl;
            } else {
                std::cout << "Failed to send response to " << task.client_ip << std::endl;
                // Could create a cleanup task here
            }
        }
        
        void handle_cleanup(const Task& task, int worker_id) {
            // Close the socket and clean up any resources
            std::cout << "Worker " << worker_id << " cleaning up connection to " 
                      << task.client_ip << std::endl;
            close(task.client_socket);
            // TODO: Remove from connection tracking
        }

    private:
        // The task queue - this is where all work items wait to be processed
        std::queue<Task> task_queue;
        
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

} // namespace TRANSPORT