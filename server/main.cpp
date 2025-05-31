#include <iostream>
#include <signal.h>
#include <sys/socket.h>

#include "tcp_layer.hpp"

int main() {
    
    net_layer::Server srv;

    if (!(srv.bind_socket(8080) && srv.start_listening()))
        return 1;
    
    if (srv.accept_connection()) {
        srv.handle_client();
    }

    return 0;
}