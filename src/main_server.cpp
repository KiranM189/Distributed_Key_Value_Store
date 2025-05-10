#include "KVServer.hpp"
#include "kvstore.hpp"
#include <string>
#include <cstdlib>
#include <iostream>
#include <algorithm>

// Helper function to parse memory size from string with unit suffix
std::size_t parseMemorySize(const std::string& size_str) {
    const std::size_t KB = 1024;
    const std::size_t MB = 1024 * 1024;
    const std::size_t GB = 1024 * 1024 * 1024;
    
    // Default to 100MB if parsing fails
    std::size_t size = 100 * MB;
    
    try {
        // Check if the string has a unit suffix (K/M/G)
        std::string num_part = size_str;
        std::transform(num_part.begin(), num_part.end(), num_part.begin(), ::toupper);
        
        char unit = 'M'; // Default to MB
        double num_value = 0;
        
        if (num_part.back() == 'K' || num_part.back() == 'M' || num_part.back() == 'G') {
            unit = num_part.back();
            num_part.pop_back();
            num_value = std::stod(num_part);
        } else {
            // If no unit specified, assume MB
            num_value = std::stod(num_part);
        }
        
        // Convert to bytes based on unit
        switch (unit) {
            case 'K': size = static_cast<std::size_t>(num_value * KB); break;
            case 'M': size = static_cast<std::size_t>(num_value * MB); break;
            case 'G': size = static_cast<std::size_t>(num_value * GB); break;
            default:  size = static_cast<std::size_t>(num_value * MB); break;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error parsing memory size: " << e.what() << "\n";
        std::cerr << "Using default size of 100MB\n";
    }
    
    return size;
}

int main(int argc, char** argv) {
    // Default values
    std::string protocol = "ofi+tcp";
    int port = 8080;
    std::size_t mem_size = 100 * 1024 * 1024; // Default 100MB
    
    // Parse command line arguments
    if (argc >= 2) {
        protocol = argv[1];
    }
    
    if (argc >= 3) {
        try {
            port = std::stoi(argv[2]);
        } catch (...) {
            std::cerr << "Invalid port number. Using default: " << port << "\n";
        }
    }
    
    if (argc >= 4) {
        // Parse memory size with unit support
        mem_size = parseMemorySize(argv[3]);
    }
    
    // Get server IP address
    char ip_buffer[128];
    FILE* fp = popen("hostname -I | awk '{print $1}'", "r");
    fgets(ip_buffer, sizeof(ip_buffer), fp);
    pclose(fp);
    
    std::string address = protocol + "://" + std::string(ip_buffer) + ":" + std::to_string(port);
    address.erase(std::remove(address.begin(), address.end(), '\n'), address.end());
    
    uint16_t provider_id = 1;
    
    // Print startup configuration
    std::cout << "\n==== KV Server Configuration ====\n";
    std::cout << "Protocol:      " << protocol << "\n";
    std::cout << "Port:          " << port << "\n";
    std::cout << "Shared Memory: " << (mem_size / (1024 * 1024)) << "MB\n";
    std::cout << "Address:       " << address << "\n";
    std::cout << "================================\n\n";
    
    // Start the Thallium engine
    tl::engine myEngine(address, THALLIUM_SERVER_MODE);
    std::cout << "Server running at " << myEngine.self() << std::endl;
    
    // Get hostname for logging
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    std::cout << "Server hostname: " << hostname << std::endl;
    
    // Initialize the KvStore with shared memory
    std::cout << "Initializing KvStore with " << (mem_size / (1024 * 1024)) << "MB shared memory\n";
    KvStore &kv = KvStore::get_instance(mem_size);
    std::cout << "KvStore initialized" << std::endl;
    
    // Create and start the KVServer
    KVServer server(myEngine, kv, provider_id);
    std::cout << "KVServer started with provider ID: " << provider_id << std::endl;
    std::cout << "Server is running. Connect using: " << myEngine.self() << std::endl;
    
    // Wait for completion
    myEngine.wait_for_finalize();
    
    return 0;
}
