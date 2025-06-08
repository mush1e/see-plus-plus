#pragma once

#include "./task.hpp"

#include <iostream>

namespace THREADPOOL {
    class HTTPRequestTask : public Task {
    public:
        HTTPRequestTask(std::shared_ptr<CORE::ConnectionState> conn,
                    const std::string& raw_headers);
        void execute(int worker_id) override;
    private:
        std::shared_ptr<CORE::ConnectionState> conn_;
        std::string raw_headers_;
    };
}