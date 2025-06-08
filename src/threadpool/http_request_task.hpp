#pragma once

#include "./task.hpp"

#include <iostream>
#include <sstream>
#include <unordered_map>
#include <unistd.h>    // for close()
#include <sys/socket.h> // for send() 

namespace THREADPOOL {
    class HTTPRequestTask : public Task {
    public:
        HTTPRequestTask(std::shared_ptr<CORE::ConnectionState> conn,
                    const std::string& raw_headers);

        void execute(int worker_id) override;

    private:
        std::shared_ptr<CORE::ConnectionState> conn_;
        std::string raw_headers_;

        // Parsed fields
        std::string                                  method_;
        std::string                                  path_;
        std::string                                  version_;
        std::unordered_map<std::string, std::string> headers_;

        void parse_request(); 
        void send_response();
    };
}