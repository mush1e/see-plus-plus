#include "tcp_layer.hpp"

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

    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ BIND AND ACCEPT ~~~~~~~~~~~~~~~~~~~~~~~~~~

    bool Server::bind_socket(uint16_t port) {
        memset(&(this->server_addr), 0, sizeof(this->server_addr));

        this->server_addr.sin_family      = AF_INET;            // IPv4
        this->server_addr.sin_addr.s_addr = INADDR_ANY;         // Accept connections from any IP
        this->server_addr.sin_port        = htons(port);        // Convert port to network byte order (big-endian)

        if (bind(this->server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
            std::cerr << "Bind failed" << std::endl;
            return false;
        }

        std::cout << "Socket bound to port " << port << std::endl;
        return true;
    }

    bool Server::start_listening(int max_conns) {

        if(listen(server_socket, max_conns) == -1) {
            std::cerr << "Listen Failed" << std::endl;
            return false;
        }

        std::cout << "Server listening for connections..." << std::endl;
        return true;
    }

    bool Server::accept_connection() {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &client_addr_len);

        if (client_socket == -1) {
            std::cerr << "Accept failed" << std::endl;
            return false;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);      // Convert Client IP to Readable format
        
        std::cout << "Connection accepted from " << client_ip 
                  << ":" << ntohs(client_addr.sin_port) << std::endl;
        return true;
    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ HANDLE CLIENT ~~~~~~~~~~~~~~~~~~~~~~~~~~ 

    void Server::handle_client() {
        char buffer [1024];
    
        for (;;) {
            memset(buffer, 0, sizeof(buffer));

            ssize_t bytes_received = recv(client_socket, buffer, sizeof(buffer) - 1, 0);
            
            if (bytes_received <= 0) {
                if (bytes_received == 0) {
                    std::cout << "Client disconnected" << std::endl;
                } else {
                    std::cerr << "Receive failed" << std::endl;
                }
                break;
            }
            
            std::cout << "Received: " << buffer << std::endl;
            
            std::string response = "Echo: " + std::string(buffer);
            send(client_socket, response.c_str(), response.length(), 0);
            
            if (std::string(buffer) == "quit") {
                std::cout << "Client requested to quit" << std::endl;
                break;
            }

        }
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
