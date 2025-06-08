#include "websocket_handshake_task.hpp"

namespace THREADPOOL {

WebSocketHandshakeTask::WebSocketHandshakeTask(
    std::shared_ptr<CORE::ConnectionState> conn,
    const std::string& raw_headers
) : conn_(std::move(conn)), raw_headers_(raw_headers)
{}

void WebSocketHandshakeTask::execute(int worker_id) {
    std::cout << "[Worker " << worker_id << "] WS handshake for fd "
              << conn_->socket_fd << "\n";
    // later: compute Sec-WebSocket-Accept, send 101…
}


} // namespace THREADPOOL