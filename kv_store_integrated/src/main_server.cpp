#include "KVServer.hpp"
#include "kvstore.hpp"


int main(int argc, char** argv) {
        // Default port and protocol
        std::string protocol = "ofi+tcp";
        int port = 8080;
        
        // Check command line arguments for protocol and port
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
        
        // Create the full address with protocol and port
        //std::string address = protocol + "://0.0.0.0:" + std::to_string(port);
        char ip_buffer[128];
	FILE* fp = popen("hostname -I | awk '{print $1}'", "r");
	fgets(ip_buffer, sizeof(ip_buffer), fp);
	pclose(fp);
	std::string address = protocol + "://" + std::string(ip_buffer) + ":" + std::to_string(port);
	address.erase(std::remove(address.begin(), address.end(), '\n'), address.end());

	uint16_t provider_id = 1;
        
        std::cout << "Starting server with address: " << address << std::endl;
        
        tl::engine myEngine(address, THALLIUM_SERVER_MODE);
        std::cout << "Server running at " << myEngine.self() << std::endl;
        
        // Get hostname for logging
        char hostname[256];
        gethostname(hostname, sizeof(hostname));
        std::cout << "Server hostname: " << hostname << std::endl;
        
        // Initialize the KvStore with shared memory
        KvStore &kv = KvStore::get_instance(1024 * 1024); // 1MB shared memory
        std::cout << "KvStore initialized" << std::endl;
        
        // Create and start the KVServer
        KVServer server(myEngine, kv, provider_id);
        std::cout << "KVServer started with provider ID: " << provider_id << std::endl;
        std::cout << "Server is running. Connect using: " << myEngine.self() << std::endl;
        
        myEngine.wait_for_finalize();
        return 0;
}
