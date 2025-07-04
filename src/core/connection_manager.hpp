#pragma once

#include "types.hpp"
#include "http_parser.hpp"

#include <memory>
#include <mutex>
#include <chrono>
#include <unordered_map>
#include <shared_mutex>  // C++17 feature for reader-writer locks

namespace CORE {

    class ConnectionManager {
    public:
        static constexpr size_t MAX_CONNECTIONS = 1024;
        static constexpr std::chrono::seconds CONNECTION_TIMEOUT{300}; // 5 minutes
        static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
        
        // RAII wrapper that ensures safe access to connection data
        class ConnectionHandle {
        public:
            ConnectionHandle(std::shared_ptr<ConnectionState> state, 
                           std::unique_ptr<HTTPParser>* parser_ptr)
                : connection_state_(std::move(state)), parser_ptr_(parser_ptr) {}
            
            // Access the connection state safely
            std::shared_ptr<ConnectionState> connection() const { 
                return connection_state_; 
            }
            
            // Access the parser safely  
            HTTPParser* parser() const { 
                return parser_ptr_ ? parser_ptr_->get() : nullptr; 
            }
            
            // Check if handle is valid
            bool is_valid() const { 
                return connection_state_ && parser_ptr_ && *parser_ptr_; 
            }
            
        private:
            std::shared_ptr<ConnectionState> connection_state_;
            std::unique_ptr<HTTPParser>* parser_ptr_;  // Non-owning pointer
        };
        
        struct ConnectionData {
            std::shared_ptr<ConnectionState> state;
            std::unique_ptr<HTTPParser> parser;
            size_t total_bytes_received = 0;
            std::chrono::steady_clock::time_point created_at;
            
            ConnectionData(std::shared_ptr<ConnectionState> conn_state) 
                : state(std::move(conn_state)), 
                  parser(std::make_unique<HTTPParser>()),
                  created_at(std::chrono::steady_clock::now()) {}
        };

        bool add_connection(int fd, const std::string& ip, uint16_t port) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            
            // Check connection limit
            if (connections_.size() >= MAX_CONNECTIONS) {
                return false;
            }
            
            auto conn_state = std::make_shared<ConnectionState>(fd, ip, port);
            connections_[fd] = std::make_unique<ConnectionData>(conn_state);
            
            return true;
        }
        
        // NEW: Safe way to get connection with RAII handle
        ConnectionHandle get_connection_handle(int fd) {
            std::shared_lock<std::shared_mutex> lock(mutex_);  // Read lock
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                return ConnectionHandle(it->second->state, &it->second->parser);
            }
            return ConnectionHandle(nullptr, nullptr);
        }
        
        // DEPRECATED: Keep for backward compatibility but mark as unsafe
        [[deprecated("Use get_connection_handle() for thread safety")]]
        std::shared_ptr<ConnectionState> get_connection(int fd) {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            auto it = connections_.find(fd);
            return (it != connections_.end()) ? it->second->state : nullptr;
        }
        
        bool check_request_size_limit(int fd, size_t additional_bytes) {
            std::unique_lock<std::shared_mutex> lock(mutex_);  // Write lock for modification
            auto it = connections_.find(fd);
            if (it == connections_.end()) return false;
            
            it->second->total_bytes_received += additional_bytes;
            return it->second->total_bytes_received <= MAX_REQUEST_SIZE;
        }
        
        void reset_parser(int fd) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                it->second->parser->reset();
                it->second->total_bytes_received = 0;
            }
        }
        
        void remove_connection(int fd) {
            std::unique_lock<std::shared_mutex> lock(mutex_);
            connections_.erase(fd);
        }
        
        // Clean up timed-out connections
        std::vector<int> get_timed_out_connections() {
            std::vector<int> timed_out;
            std::shared_lock<std::shared_mutex> lock(mutex_);  // Read lock for inspection
            
            auto now = std::chrono::steady_clock::now();
            for (const auto& [fd, conn_data] : connections_) {
                if (now - conn_data->state->last_activity > CONNECTION_TIMEOUT) {
                    timed_out.push_back(fd);
                }
            }
            
            return timed_out;
        }
        
        size_t connection_count() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            return connections_.size();
        }
        
        // NEW: Get connection statistics for monitoring
        struct ConnectionStats {
            size_t total_connections;
            size_t total_bytes_processed;
            std::chrono::steady_clock::time_point oldest_connection;
            double average_request_size;
        };
        
        ConnectionStats get_stats() const {
            std::shared_lock<std::shared_mutex> lock(mutex_);
            
            ConnectionStats stats{};
            stats.total_connections = connections_.size();
            
            if (connections_.empty()) {
                return stats;
            }
            
            size_t total_bytes = 0;
            auto oldest_time = std::chrono::steady_clock::now();
            
            for (const auto& [fd, conn_data] : connections_) {
                total_bytes += conn_data->total_bytes_received;
                if (conn_data->created_at < oldest_time) {
                    oldest_time = conn_data->created_at;
                }
            }
            
            stats.total_bytes_processed = total_bytes;
            stats.oldest_connection = oldest_time;
            stats.average_request_size = static_cast<double>(total_bytes) / connections_.size();
            
            return stats;
        }

    private:
        // Using shared_mutex for reader-writer locking
        // Multiple readers can access simultaneously
        // but writers get exclusive access
        mutable std::shared_mutex mutex_;
        std::unordered_map<int, std::unique_ptr<ConnectionData>> connections_;
    };

} // namespace CORE