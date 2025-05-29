#include "tcp_server.hpp"

namespace net_layer {
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ INIT SETUP ~~~~~~~~~~~~~~~~~~~~~~~~~~ 

    bool Server::create_socket() {
        this->server_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (server_socket == -1) {
            perror("Failed to create socket");
            return false;
        }
        
        std::cout << "Socket created successfully" << std::endl;
        return true;
    }

    Server::Server(){
        create_socket();
    }

    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ CLEANUP AND DESTRUCTOR ~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void Server::cleanup() {
        if (server_socket != -1) close(server_socket);
        if (client_socket != -1) close(client_socket);
    }    

    Server::~Server(){
        cleanup();
    }


} // namespace net_layer
