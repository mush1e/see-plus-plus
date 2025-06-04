#include "tcp_layer.hpp"

namespace TRANSPORT {
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ INIT SETUP ~~~~~~~~~~~~~~~~~~~~~~~~~~ 

    Server* Server::instance = nullptr;

    bool Server::create_socket() {
        this->server_socket = socket(AF_INET, SOCK_STREAM, 0);

        if (server_socket == -1) {
            perror("Failed to create socket");
            return false;
        }
        
        // FOR DEVELOPMENT ONLY
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
            perror("Bind failed");
            return false;
        }

        std::cout << "Socket bound to port " << port << std::endl;
        return true;
    }

    bool Server::start_listening(int max_conns) {

        if(listen(server_socket, max_conns) == -1) {
            perror("Listen Failed");
            return false;
        }

        std::cout << "Server listening for connections..." << std::endl;
        return true;
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ START RUNNING SERVER ~~~~~~~~~~~~~~~~~~~~~~~~~~

    void Server::run_server() {
        if (!(bind_socket(8080) && start_listening(10))) 
            return;
        
        std::cout << "Server running. Press Ctrl+C to stop" << std::endl;

        for (;;) {
            sockaddr_in client_addr {};
            socklen_t client_addr_len = sizeof(client_addr);

            int client_sock = accept(server_socket, (sockaddr*)&client_addr, &client_addr_len);
            
            if (client_sock == -1) {
                if (errno == EINTR) {
                    std::cout << "Accept interrupted. Shutting Down..." << std::endl;
                    break;
                }
                perror("Accept failed");
                continue;
            }

            if (active_clients.load() >= MAX_CLIENTS) {
                std::cout << "Server at maximum capacity. Rejecting new client." << std::endl;
                close(client_sock);
                continue;
            }

            active_clients.fetch_add(1);

            client_threads.emplace_back(&Server::handle_client_threaded, this, client_sock, client_addr);
            {
                std::lock_guard<std::mutex> lock(this->cout_mtx);
                std::cout << "Created thread for new client. Active clients: " 
                      << active_clients.load() << "/" << MAX_CLIENTS << std::endl;
            }   
        }

    }
    
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ HANDLE CLIENT ~~~~~~~~~~~~~~~~~~~~~~~~~~ 

    // In this handle client an external function accepts a connection and passes 
    // client socket and the socket address to this function treating it kinda 
    // like a worker function | also since std::cout is not thread safe we use a
    // std::mutex (cout_mtx) to avoid garbage output
    void Server::handle_client_threaded(int client_sock, sockaddr_in client_addr) {
        char buffer[1024];

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);      // Convert Client IP to Readable format

        timeval timeout;
        timeout.tv_sec = 30;
        timeout.tv_usec = 0;

        if (setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("Failed to set socket timeout");
        }

        // Locked to prevent jumbled output since std::cout isn't thread safe
        {
            std::lock_guard<std::mutex> lock(this->cout_mtx);
            std::cout << "Connection accepted from " << client_ip 
                      << ":" << ntohs(client_addr.sin_port) 
                      << " (timeout: " << timeout.tv_sec << "s)" 
                      << std::endl;
        }

        auto connection_start = std::chrono::steady_clock::now();

        for (;;) {
            memset(buffer, 0, sizeof(buffer));

            ssize_t bytes_recv = recv(client_sock, buffer, sizeof(buffer)-1, 0);
            if (bytes_recv <= 0) {
                if (bytes_recv == 0) {
                    std::lock_guard<std::mutex> lock(this->cout_mtx);
                    std::cout << "Client " << client_ip << " disconnected" << std::endl;
                } 
                // This is a timeout - client took too long to send data
                else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    auto now = std::chrono::steady_clock::now();
                    auto duration = std::chrono::duration_cast<std::chrono::seconds>(now - connection_start);
                    std::lock_guard<std::mutex> lock(this->cout_mtx);
                    std::cout << "Client " << client_ip << " timed out after " 
                              << duration.count() << " seconds - disconnecting zombie connection" << std::endl;
                } 
                else {
                    std::lock_guard<std::mutex> lock(this->cout_mtx);
                    std::cerr << "Receive failed for client " << client_ip << std::endl;
                }
                break;
            }
            
            // Reset connectiontimer once we recieve a message
            connection_start = std::chrono::steady_clock::now();

            std::string msg(buffer, bytes_recv);
            msg.erase(msg.find_last_not_of(" \t\r\n") + 1);

            // Locked to prevent jumbled output since std::cout isn't thread safe
            {
                std::lock_guard<std::mutex> lock(this->cout_mtx);
                std::cout << "Client - [" << client_ip << "] has sent : |" << msg << "|" << std::endl; 
            }

            if(msg == "quit") {
                std::lock_guard<std::mutex> lock(this->cout_mtx);
                std::cout << "Client " << client_ip << " requested to quit" << std::endl;
                break;
            }

            std::string resp = "Echo : " + msg + "\n";
            send(client_sock, resp.c_str(), resp.length(), 0);
        }

        close(client_sock);
        active_clients.fetch_sub(1);

        // Locked to prevent jumbled output since std::cout isn't thread safe
        {
            std::lock_guard<std::mutex> lock(this->cout_mtx);
            std::cout << "Thread for client " << client_ip << " finished. Active clients: " 
                      << active_clients.load() << "/" << MAX_CLIENTS << std::endl;
        }
    }

    // ~~~~~~~~~~~~~~~~~~~~~~~~~~ CLEANUP AND DESTRUCTOR ~~~~~~~~~~~~~~~~~~~~~~~~~~ 
    void Server::cleanup() {
        std::cout << "Cleaning up server resources..." << std::endl;
    
        
        if (server_socket != -1) {
            shutdown(server_socket, SHUT_RDWR);  // Graceful shutdown
            close(server_socket);
            server_socket = -1;
        }

        await_all();
        
        std::cout << "Server cleaned up successfully" << std::endl;
    }    

    void Server::signal_handler(int signal) {
        std::cout << std::endl << "Recieved signal : " << signal << std::endl << "initiating graceful shutdown." << std::endl;
        
        if (instance != nullptr)
            instance->cleanup();

        exit(0);
    }

    void Server::await_all() {
        std::cout << "Waiting for all client threads to finish..." << std::endl;
        
        for (auto& thread : client_threads) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        client_threads.clear();
        std::cout << "All client threads finished" << std::endl;
    }

    Server::~Server(){
        cleanup();
    }


} // namespace net_layer
