#pragma once

#include "task.hpp"
#include "../core/http.hpp"
#include "../core/router.hpp"
#include <memory>
#include <string>

namespace THREADPOOL {

class HttpRequestTask : public Task {
public:
    HttpRequestTask(std::shared_ptr<CORE::ConnectionState> conn,
                    const std::string& raw_headers,
                    CORE::Router &router);

    void execute(int worker_id) override;

private:
    std::shared_ptr<CORE::ConnectionState> conn_;
    std::string                           raw_headers_;
    CORE::Router&                        router_;
};

} // namespace THREADPOOL