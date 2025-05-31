#include <iostream>
#include <signal.h>
#include <sys/socket.h>

#include "tcp_layer.hpp"

int main() {
    
    net_layer::Server srv;

    srv.run_server();

    return 0;
}