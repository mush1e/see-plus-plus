#pragma once

#include "types.hpp"
#include "http_parser.hpp"

#include <memory>
#include <mutex>
#include <chrono>
#include <unordered_map>

namespace CORE {

    class ConnectionManager {
    public:
        static constexpr size_t MAX_CONNECTIONS = 1024;
        static constexpr std::chrono::seconds CONNECTION_TIMEOUT{300}; // 5 minutes
        static constexpr size_t MAX_REQUEST_SIZE = 1024 * 1024; // 1MB
        
        struct ConnectionData {
            std::shared_ptr<ConnectionState> state;
            std::unique_ptr<HTTPParser> parser;
            size_t total_bytes_received = 0;
            
            ConnectionData(std::shared_ptr<ConnectionState> conn_state) 
                : state(std::move(conn_state)), parser(std::make_unique<HTTPParser>()) {}
        };

        bool add_connection(int fd, const std::string& ip, uint16_t port) {
            std::lock_guard<std::mutex> lock(mutex_);
            
            // Check connection limit
            if (connections_.size() >= MAX_CONNECTIONS) {
                return false;
            }
            
            auto conn_state = std::make_shared<ConnectionState>(fd, ip, port);
            connections_[fd] = std::make_unique<ConnectionData>(conn_state);
            
            return true;
        }
        
        std::shared_ptr<ConnectionState> get_connection(int fd) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(fd);
            return (it != connections_.end()) ? it->second->state : nullptr;
        }
        
        HTTPParser* get_parser(int fd) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(fd);
            return (it != connections_.end()) ? it->second->parser.get() : nullptr;
        }
        
        bool check_request_size_limit(int fd, size_t additional_bytes) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(fd);
            if (it == connections_.end()) return false;
            
            it->second->total_bytes_received += additional_bytes;
            return it->second->total_bytes_received <= MAX_REQUEST_SIZE;
        }
        
        void reset_parser(int fd) {
            std::lock_guard<std::mutex> lock(mutex_);
            auto it = connections_.find(fd);
            if (it != connections_.end()) {
                it->second->parser->reset();
                it->second->total_bytes_received = 0;
            }
        }
        
        void remove_connection(int fd) {
            std::lock_guard<std::mutex> lock(mutex_);
            connections_.erase(fd);
        }
        
        // Clean up timed-out connections
        std::vector<int> get_timed_out_connections() {
            std::vector<int> timed_out;
            std::lock_guard<std::mutex> lock(mutex_);
            
            auto now = std::chrono::steady_clock::now();
            for (const auto& [fd, conn_data] : connections_) {
                if (now - conn_data->state->last_activity > CONNECTION_TIMEOUT) {
                    timed_out.push_back(fd);
                }
            }
            
            return timed_out;
        }
        
        size_t connection_count() const {
            std::lock_guard<std::mutex> lock(mutex_);
            return connections_.size();
        }

    private:
        mutable std::mutex mutex_;
        std::unordered_map<int, std::unique_ptr<ConnectionData>> connections_;
    };

} // namespace CORE