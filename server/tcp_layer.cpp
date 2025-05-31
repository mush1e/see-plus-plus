#include "tcp_layer.hpp"

namespace net_layer {
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ INIT SETUP ~~~~~~~~~~~~~~~~~~~~~~~~~~ 

    Server* Server::instance = nullptr;

    bool Server::create_socket() {
        this->server_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (server_socket == -1) {
            perror("Failed to create socket");
            return false;
        }
        
        // Let socket address be reused immediately 
        int opt = 1;
        if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
            perror("setsockopt SO_REUSEADDR failed");
            close(server_socket);
            return false;
        }

        std::cout << "Socket created successfully" << std::endl;
        return true;
    }


    void Server::setup_signal_handlers() {
        this->instance = this;
        signal(SIGINT, signal_handler);
        signal(SIGTERM, signal_handler);
    }

    Server::Server(){
        create_socket();
        setup_signal_handlers();
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
            
            std::string message = std::string(buffer, bytes_received);

            // Clear carriage return and new lines 
            message.erase(message.find_last_not_of(" \t\r\n") + 1);

            std::cout << "Received: " << "|" << message << "|" << std::endl;
            
            if (message == "quit") {
                std::cout << "Client requested to quit" << std::endl;
                break;
            }
            
            std::string response = "Echo: " + message + "\n";
            send(client_socket, response.c_str(), response.length(), 0);
        }
    }


    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ CLEANUP AND DESTRUCTOR ~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void Server::cleanup() {
        std::cout << "Cleaning up server resources..." << std::endl;
        
        if (client_socket != -1) {
            shutdown(client_socket, SHUT_RDWR);  // Graceful shutdown
            close(client_socket);
            client_socket = -1;
        }
        
        if (server_socket != -1) {
            shutdown(server_socket, SHUT_RDWR);  // Graceful shutdown
            close(server_socket);
            server_socket = -1;
        }
        
        std::cout << "Server cleaned up successfully" << std::endl;
    }    

    void Server::signal_handler(int signal) {
        std::cout << std::endl << "Recieved signal : " << signal << std::endl << "initiating graceful shutdown." << std::endl;
        
        if (instance != nullptr)
            instance->cleanup();

        exit(0);
    }

    Server::~Server(){
        cleanup();
    }


} // namespace net_layer
