#include <iostream>
#include <signal.h>

// signal_handler represents a helper function
// to handle signals recieved from the user/OS
// and initiate graceful shutdown
void signal_handler(int sig) {
    switch (sig) {
        case SIGINT:
            std::cout << "Interupt signal recieved\n";
            break;
        case SIGTERM:
            std::cout << "Termination signal recieved\n";
            break;
        default:
            std::cout << "signal SIG : " << sig << " recieved\n";
    }
}

int main() {
    std::cout << "Hello world!\n";
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    return 0;
}