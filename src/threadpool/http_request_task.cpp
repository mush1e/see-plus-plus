#include "http_request_task.hpp"

namespace THREADPOOL {

HTTPRequestTask::HTTPRequestTask(
    std::shared_ptr<CORE::ConnectionState> conn,
    const std::string& raw_headers
) : conn_(std::move(conn)), raw_headers_(raw_headers)
{}

void HTTPRequestTask::execute(int worker_id) {
    std::cout << "[Worker" << worker_id << "] HTTP task for client" << this->conn_->client_ip << std::endl; 
}

} // namespace THREADPOOL