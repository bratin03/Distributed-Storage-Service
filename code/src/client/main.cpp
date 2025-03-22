// main.cpp
#include "dss_client.hpp"
#include <csignal>
#include <chrono>
#include <atomic>

using namespace std::chrono_literals;

// Global atomic flag for shutdown control
std::atomic<bool> running{true};

// Signal handler for graceful shutdown
void signal_handler(int signum) {
    std::cout << "\nReceived termination signal (" << signum << "), shutting down...\n";
    running = false;
}

int main(int argc, char* argv[]) {
    // Register signal handlers
    std::signal(SIGINT, signal_handler);  // Ctrl+C
    std::signal(SIGTERM, signal_handler); // Systemd/kill command

    try {
        // Configuration - could be read from config file or command line
        const std::string root_dir = (argc > 1) ? argv[1] : "root_dss";
        const std::string server_url = (argc > 2) ? argv[2] : "http://localhost:8080";

        // Create and configure client
        DssClient client(root_dir, server_url);
        
        std::cout << "\n=== DSS Client Starting ===\n";
        std::cout << "Root Directory: " << fs::absolute(root_dir) << "\n";
        std::cout << "Server Endpoint: " << server_url << "\n\n";

        // Initialize client components
        client.initialize();

        // Start synchronization and monitoring
        client.start();

        // Main loop - keep alive until shutdown signal
        while(running) {
            std::this_thread::sleep_for(500ms); // Reduce CPU usage
            
            // Could add health checks or periodic sync here
        }

        // Shutdown sequence
        std::cout << "\n=== Initiating Shutdown ===\n";
        client.stop();
        std::cout << "Shutdown complete.\n";

    } catch(const std::exception& e) {
        std::cerr << "\n!!! Critical Error: " << e.what() << " !!!\n";
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
