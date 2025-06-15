#include "KVClient.hpp"
#include "kvstore.hpp"
#include "ThalliumDistributor.hpp"
#include <limits>
#include <chrono>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <algorithm>
#include <random>
#include <iomanip>
#include <thread>
#include <numeric>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <algorithm>
#include <cctype>

// Helper function to parse command arguments
std::vector<std::string> parseCommand(const std::string& command) {
    std::vector<std::string> args;
    std::istringstream iss(command);
    std::string arg;
    while (iss >> arg) {
        args.push_back(arg);
    }
    return args;
}


// Helper function to get local IP address
std::string getLocalIPAddress() {
    // Simple method to get local IP by connecting to a remote address
    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == -1) return "";
    
    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_addr.s_addr = inet_addr("8.8.8.8"); // Google DNS
    serv.sin_port = htons(53);
    
    int err = connect(sock, (const struct sockaddr*)&serv, sizeof(serv));
    if (err == -1) {
        close(sock);
        return "";
    }
    
    struct sockaddr_in name;
    socklen_t namelen = sizeof(name);
    err = getsockname(sock, (struct sockaddr*)&name, &namelen);
    if (err == -1) {
        close(sock);
        return "";
    }
    
    char buffer[INET_ADDRSTRLEN];
    const char* p = inet_ntop(AF_INET, &name.sin_addr, buffer, INET_ADDRSTRLEN);
    close(sock);
    
    return p ? std::string(p) : "";
}



// Add this helper function after the existing helper functions
std::string extractIPFromEndpoint(const std::string& endpoint) {
    size_t protocol_end = endpoint.find("://");
    if (protocol_end != std::string::npos) {
        size_t ip_start = protocol_end + 3;
        size_t ip_end = endpoint.find(':', ip_start);
        if (ip_end != std::string::npos) {
            return endpoint.substr(ip_start, ip_end - ip_start);
        }
    } else {
        // Fallback for simple IP:PORT format
        size_t colon_pos = endpoint.find(':');
        if (colon_pos != std::string::npos) {
            return endpoint.substr(0, colon_pos);
        }
    }
    return "";
}



// Help function
void printHelp() {
    std::cout << "\nDistributed Key-Value Store Commands:" << std::endl;
    std::cout << "====================================" << std::endl;
    std::cout << "  put <key> <value>        - Store a key-value pair" << std::endl;
    std::cout << "  get <key>                - Get a value for a key" << std::endl;
    std::cout << "  update <key> <value>     - Update an existing key-value pair" << std::endl;
    std::cout << "  delete <key>             - Delete a key-value pair" << std::endl;
    std::cout << "  addnode <endpoint>       - Add a new node to the cluster" << std::endl;
    std::cout << "  removenode <node_index>  - Remove a node from the cluster" << std::endl;
    std::cout << "  listnodes                - List all nodes in the cluster" << std::endl;
    std::cout << "  distribution             - Show distribution of keys across nodes" << std::endl;
    std::cout << "  benchmark                - To run with sequential fetch pattern" << std::endl;
    std::cout << "  benchmark1               - Run benchmark with random fetch pattern" << std::endl;
    std::cout << "  status                   - Show storage mode and cluster status" << std::endl;
    std::cout << "  hash <key>               - Show which node a key would be assigned to" << std::endl;
    std::cout << "  help                     - Show this help message" << std::endl;
    std::cout << "  exit                     - Exit the program" << std::endl;
}

void runBenchmark(ThalliumDistributor& distributor) {
    const int NUM_KEYS = 2000000;
    const int VALUE_LENGTH = 50;
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "DISTRIBUTED KEY-VALUE STORE BENCHMARK" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    if (distributor.getNodeCount() == 0) {
        std::cout << "Error: No nodes available. Add nodes first." << std::endl;
        return;
    }
    std::cout << "Nodes in cluster: " << distributor.getNodeCount() << std::endl;
    std::cout << "Keys to test: " << NUM_KEYS << std::endl;
    std::cout << "Value length: ~" << VALUE_LENGTH << " characters" << std::endl;
    std::cout << std::endl;

    // Generate random test data
    std::vector<std::pair<int, std::string>> test_data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> char_dis(97, 122); // a-z
    std::cout << "Generating test data..." << std::endl;
    for (int i = 1; i <= NUM_KEYS; i++) {
        std::string value = "testvalue" + std::to_string(i) + "";
        // Add random characters to reach desired length
        while (value.length() < VALUE_LENGTH) {
            value += static_cast<char>(char_dis(gen));
        }
        test_data.push_back({i, value});
    }

    // Phase 1: Insert all key-value pairs
    std::cout << "\nPHASE 1: Inserting key-value pairs" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    std::vector<double> insert_times;
    int successful_inserts = 0;
    for (const auto& pair : test_data) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Inserting key " << pair.first << "... " << std::flush;
        try {
            distributor.put(pair.first, pair.second);
            successful_inserts++;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;
            insert_times.push_back(time_ms);
            std::cout << "OK (" << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")" << std::endl;
        }
        // Progress indicator
        if (pair.first % 100 == 0) {
            std::cout << "Progress: " << pair.first << "/" << NUM_KEYS << " insertions completed" << std::endl;
        }
    }
    std::cout << "\nInsertion complete: " << successful_inserts << "/" << NUM_KEYS << " successful" << std::endl;
    if (!insert_times.empty()) {
        double avg_insert = std::accumulate(insert_times.begin(), insert_times.end(), 0.0) / insert_times.size();
        std::cout << "Average insertion time: " << std::fixed << std::setprecision(2) << avg_insert << "ms" << std::endl;
    }

    // Small delay to ensure all insertions are complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Phase 2: Fetch all keys and categorize by local/remote
    std::cout << "\nPHASE 2: Fetching key-value pairs and measuring times" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    std::vector<double> local_fetch_times;
    std::vector<double> remote_fetch_times;

    // Get local IP address to determine local vs remote fetches
    std::string local_ip = getLocalIPAddress();
    std::cout << "Local IP detected: " << local_ip << std::endl;

    for (int key = 1; key <= NUM_KEYS; key++) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Fetching key " << key << "... " << std::flush;
        try {
            // Determine which node this key maps to
            int node_idx = key % distributor.getNodeCount();
            // Check if the target node is local by comparing IP addresses
            std::string target_node_endpoint = distributor.getNodeEndpoint(node_idx);
            std::string target_ip = extractIPFromEndpoint(target_node_endpoint);
            bool is_local = (target_ip == local_ip);
            std::cout << "DEBUG - Key: " << key << ", Node: " << node_idx 
            << ", Endpoint: " << target_node_endpoint 
            << ", Target IP: " << target_ip 
            << ", Local IP: " << local_ip 
            << ", Is Local: " << is_local << std::endl;
            
            // Always use the distributor.get() method - no local cache optimization
            std::string value = distributor.get(key);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;

            if (value != "Key not found in mappings" &&
                value != "RPC fetch failed" &&
                value != "Connection failed") {
                if (is_local) {
                    local_fetch_times.push_back(time_ms);
                    std::cout << "OK (LOCAL, " << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
                } else {
                    remote_fetch_times.push_back(time_ms);
                    std::cout << "OK (REMOTE, " << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
                }
            } else {
                std::cout << "FAILED (" << value << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")" << std::endl;
        }
        // Progress indicator
        if (key % 100 == 0) {
            std::cout << "Progress: " << key << "/" << NUM_KEYS << " fetches completed" << std::endl;
        }
    }

    // Phase 3: Calculate and display statistics
    std::cout << "\nPHASE 3: Performance Analysis" << std::endl;
    std::cout << std::string(30, '-') << std::endl;
    if (!local_fetch_times.empty()) {
        double avg_local = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0) / local_fetch_times.size();
        double min_local = *std::min_element(local_fetch_times.begin(), local_fetch_times.end());
        double max_local = *std::max_element(local_fetch_times.begin(), local_fetch_times.end());
        std::cout << "LOCAL FETCH STATISTICS:" << std::endl;
        std::cout << "  Count: " << local_fetch_times.size() << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg_local << "ms" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(2) << min_local << "ms" << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(2) << max_local << "ms" << std::endl;
    } else {
        std::cout << "No local fetch data available" << std::endl;
    }
    std::cout << std::endl;

    if (!remote_fetch_times.empty()) {
        double avg_remote = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0) / remote_fetch_times.size();
        double min_remote = *std::min_element(remote_fetch_times.begin(), remote_fetch_times.end());
        double max_remote = *std::max_element(remote_fetch_times.begin(), remote_fetch_times.end());
        std::cout << "REMOTE FETCH STATISTICS:" << std::endl;
        std::cout << "  Count: " << remote_fetch_times.size() << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg_remote << "ms" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(2) << min_remote << "ms" << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(2) << max_remote << "ms" << std::endl;
    } else {
        std::cout << "No remote fetch data available" << std::endl;
    }
    std::cout << std::endl;

    // Phase 4: Performance comparison
    if (!local_fetch_times.empty() && !remote_fetch_times.empty()) {
        std::cout << "PERFORMANCE COMPARISON:" << std::endl;
        std::cout << std::string(25, '-') << std::endl;
        double avg_local = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0) / local_fetch_times.size();
        double avg_remote = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0) / remote_fetch_times.size();
        double performance_ratio = avg_remote / avg_local;
        std::cout << "Local average:   " << std::fixed << std::setprecision(2) << avg_local << "ms" << std::endl;
        std::cout << "Remote average:  " << std::fixed << std::setprecision(2) << avg_remote << "ms" << std::endl;
        std::cout << "Performance ratio: " << std::fixed << std::setprecision(2) << performance_ratio << "x" << std::endl;
        if (avg_local < avg_remote) {
            double improvement = ((avg_remote - avg_local) / avg_remote) * 100;
            std::cout << "Local is " << std::fixed << std::setprecision(1) << improvement << "% faster than remote" << std::endl;
        } else {
            double degradation = ((avg_local - avg_remote) / avg_local) * 100;
            std::cout << "Remote is " << std::fixed << std::setprecision(1) << degradation << "% faster than local" << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "BENCHMARK COMPLETE" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

void runBenchmark1(ThalliumDistributor& distributor) {
    const int NUM_KEYS = 2000000;
    const int VALUE_LENGTH = 50;
    std::cout << "\n" << std::string(60, '=') << std::endl;
    std::cout << "DISTRIBUTED KEY-VALUE STORE BENCHMARK1 (RANDOM FETCH)" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    if (distributor.getNodeCount() == 0) {
        std::cout << "Error: No nodes available. Add nodes first." << std::endl;
        return;
    }
    std::cout << "Nodes in cluster: " << distributor.getNodeCount() << std::endl;
    std::cout << "Keys to test: " << NUM_KEYS << std::endl;
    std::cout << "Value length: ~" << VALUE_LENGTH << " characters" << std::endl;
    std::cout << std::endl;

    // Generate random test data
    std::vector<std::pair<int, std::string>> test_data;
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> char_dis(97, 122); // a-z
    std::cout << "Generating test data..." << std::endl;
    for (int i = 1; i <= NUM_KEYS; i++) {
        std::string value = "testvalue" + std::to_string(i) + "";
        // Add random characters to reach desired length
        while (value.length() < VALUE_LENGTH) {
            value += static_cast<char>(char_dis(gen));
        }
        test_data.push_back({i, value});
    }

    // Phase 1: Insert all key-value pairs (same as original benchmark)
    std::cout << "\nPHASE 1: Inserting key-value pairs" << std::endl;
    std::cout << std::string(40, '-') << std::endl;
    std::vector<double> insert_times;
    int successful_inserts = 0;
    for (const auto& pair : test_data) {
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Inserting key " << pair.first << "... " << std::flush;
        try {
            distributor.put(pair.first, pair.second);
            successful_inserts++;
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;
            insert_times.push_back(time_ms);
            std::cout << "OK (" << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")" << std::endl;
        }
        // Progress indicator
        if (pair.first % 100 == 0) {
            std::cout << "Progress: " << pair.first << "/" << NUM_KEYS << " insertions completed" << std::endl;
        }
    }
    std::cout << "\nInsertion complete: " << successful_inserts << "/" << NUM_KEYS << " successful" << std::endl;
    if (!insert_times.empty()) {
        double avg_insert = std::accumulate(insert_times.begin(), insert_times.end(), 0.0) / insert_times.size();
        std::cout << "Average insertion time: " << std::fixed << std::setprecision(2) << avg_insert << "ms" << std::endl;
    }
    // Small delay to ensure all insertions are complete
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

    // Phase 2: Create randomized key sequence and fetch
    std::cout << "\nPHASE 2: Fetching key-value pairs RANDOMLY and measuring times" << std::endl;
    std::cout << std::string(60, '-') << std::endl;
    // Create vector of keys and shuffle them randomly
    std::vector<int> keys_to_fetch;
    for (int i = 1; i <= NUM_KEYS; i++) {
        keys_to_fetch.push_back(i);
    }
    // Shuffle the keys randomly
    std::shuffle(keys_to_fetch.begin(), keys_to_fetch.end(), gen);
    std::cout << "Keys shuffled randomly for fetching..." << std::endl;

    // Get local IP address to determine local vs remote fetches
    std::string local_ip = getLocalIPAddress();
    std::cout << "Local IP detected: " << local_ip << std::endl;

    std::vector<double> local_fetch_times;
    std::vector<double> remote_fetch_times;
    int fetch_count = 0;

    // Fetch keys in random order
    for (int key : keys_to_fetch) {
        fetch_count++;
        auto start = std::chrono::high_resolution_clock::now();
        std::cout << "Fetching key " << key << " (random order #" << fetch_count << ")... " << std::flush;
        try {
            // Determine which node this key maps to
            int node_idx = key % distributor.getNodeCount();
            // Check if the target node is local by comparing IP addresses
            std::string target_node_endpoint = distributor.getNodeEndpoint(node_idx);
            std::string target_ip = extractIPFromEndpoint(target_node_endpoint);
            bool is_local = (target_ip == local_ip);
            std::cout << "DEBUG - Key: " << key << ", Node: " << node_idx 
            << ", Endpoint: " << target_node_endpoint 
            << ", Target IP: " << target_ip 
            << ", Local IP: " << local_ip 
            << ", Is Local: " << is_local << std::endl;
            
            // Always use the distributor.get() method - no local cache optimization
            std::string value = distributor.get(key);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;

            if (value != "Key not found in mappings" &&
                value != "RPC fetch failed" &&
                value != "Connection failed") {
                if (is_local) {
                    local_fetch_times.push_back(time_ms);
                    std::cout << "OK (LOCAL, " << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
                } else {
                    remote_fetch_times.push_back(time_ms);
                    std::cout << "OK (REMOTE, " << std::fixed << std::setprecision(2) << time_ms << "ms)" << std::endl;
                }
            } else {
                std::cout << "FAILED (" << value << ")" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "FAILED (" << e.what() << ")" << std::endl;
        }
        // Progress indicator
        if (fetch_count % 100 == 0) {
            std::cout << "Progress: " << fetch_count << "/" << NUM_KEYS << " random fetches completed" << std::endl;
        }
    }

    // Phase 3: Calculate and display statistics (same as original)
    std::cout << "\nPHASE 3: Performance Analysis (Random Fetch Pattern)" << std::endl;
    std::cout << std::string(45, '-') << std::endl;
    if (!local_fetch_times.empty()) {
        double avg_local = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0) / local_fetch_times.size();
        double min_local = *std::min_element(local_fetch_times.begin(), local_fetch_times.end());
        double max_local = *std::max_element(local_fetch_times.begin(), local_fetch_times.end());
        std::cout << "LOCAL FETCH STATISTICS:" << std::endl;
        std::cout << "  Count: " << local_fetch_times.size() << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg_local << "ms" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(2) << min_local << "ms" << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(2) << max_local << "ms" << std::endl;
    } else {
        std::cout << "No local fetch data available" << std::endl;
    }
    std::cout << std::endl;
    if (!remote_fetch_times.empty()) {
        double avg_remote = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0) / remote_fetch_times.size();
        double min_remote = *std::min_element(remote_fetch_times.begin(), remote_fetch_times.end());
        double max_remote = *std::max_element(remote_fetch_times.begin(), remote_fetch_times.end());
        std::cout << "REMOTE FETCH STATISTICS:" << std::endl;
        std::cout << "  Count: " << remote_fetch_times.size() << std::endl;
        std::cout << "  Average: " << std::fixed << std::setprecision(2) << avg_remote << "ms" << std::endl;
        std::cout << "  Min: " << std::fixed << std::setprecision(2) << min_remote << "ms" << std::endl;
        std::cout << "  Max: " << std::fixed << std::setprecision(2) << max_remote << "ms" << std::endl;
    } else {
        std::cout << "No remote fetch data available" << std::endl;
    }
    std::cout << std::endl;

    // Phase 4: Performance comparison
    if (!local_fetch_times.empty() && !remote_fetch_times.empty()) {
        std::cout << "PERFORMANCE COMPARISON (RANDOM FETCH):" << std::endl;
        std::cout << std::string(35, '-') << std::endl;
        double avg_local = std::accumulate(local_fetch_times.begin(), local_fetch_times.end(), 0.0) / local_fetch_times.size();
        double avg_remote = std::accumulate(remote_fetch_times.begin(), remote_fetch_times.end(), 0.0) / remote_fetch_times.size();
        double performance_ratio = avg_remote / avg_local;
        std::cout << "Local average:   " << std::fixed << std::setprecision(2) << avg_local << "ms" << std::endl;
        std::cout << "Remote average:  " << std::fixed << std::setprecision(2) << avg_remote << "ms" << std::endl;
        std::cout << "Performance ratio: " << std::fixed << std::setprecision(2) << performance_ratio << "x" << std::endl;
        if (avg_local < avg_remote) {
            double improvement = ((avg_remote - avg_local) / avg_remote) * 100;
            std::cout << "Local is " << std::fixed << std::setprecision(1) << improvement << "% faster than remote" << std::endl;
        } else {
            double degradation = ((avg_local - avg_remote) / avg_local) * 100;
            std::cout << "Remote is " << std::fixed << std::setprecision(1) << degradation << "% faster than local" << std::endl;
        }
    }
    std::cout << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << "BENCHMARK1 (RANDOM FETCH) COMPLETE" << std::endl;
    std::cout << std::string(60, '=') << std::endl;
}

int main(int argc, char** argv) {
    uint16_t provider_id = 1;
    tl::engine myEngine("ofi+tcp", THALLIUM_CLIENT_MODE);

std::cout << "\nModulo-based Key-Value Store" << std::endl;
std::cout << "===========================" << std::endl;

// Prompt user for storage mode
std::string mode;
std::cout << "Enter server storage mode (memory/persistent): ";
std::getline(std::cin, mode);

// Convert to lowercase for comparison
std::transform(mode.begin(), mode.end(), mode.begin(), ::tolower);

StorageMode storage_mode;
if (mode == "persistent") {
    storage_mode = StorageMode::PERSISTENT;
} else if (mode == "memory") {
    storage_mode = StorageMode::MEMORY;
} else {
    std::cout << "Invalid storage mode. Defaulting to memory mode." << std::endl;
    storage_mode = StorageMode::MEMORY;
}

// Create KvStore instance for local storage
size_t mem_size = 500ULL * 1024 * 1024; // 500MB - reasonable size
	KvStore& local_store = KvStore::get_instance(mem_size, storage_mode);
	std::cout << "Storage Mode: " << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "IN-MEMORY") << std::endl;
if (storage_mode == StorageMode::PERSISTENT) {
    std::cout << "Data will be saved to: kvstore_persistent.dat" << std::endl;
} else {
    std::cout << "Data will be stored in memory only" << std::endl;
}

	// Pass the local_store to ThalliumDistributor
	ThalliumDistributor distributor(myEngine, provider_id, &local_store);
    std::cout << "Hashing mechanism: key % number_of_nodes" << std::endl;

    // Handle initial nodes from command line or default setup
    std::vector<std::string> initial_nodes;
    bool setup_from_args = false;

    // Check if nodes are provided via command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        if (arg == "--node" || arg == "-n") {
            if (i + 1 < argc) {
                initial_nodes.push_back(argv[i+1]);
                i++; // Skip the next argument since we used it as the endpoint
                setup_from_args = true;
            }
        }
    }

    // If no nodes specified on command line, use defaults
    if (!setup_from_args) {
        std::cout << "No nodes specified via command line. Add nodes using 'addnode' command." << std::endl;
    } else {
        // Add the specified nodes
        for (const auto& endpoint : initial_nodes) {
            distributor.addNode(endpoint, provider_id);
        }
        std::cout << "Added " << initial_nodes.size() << " nodes to the cluster." << std::endl;
    }

    printHelp();

    while (true) {
        std::string input;
        std::cout << "\n> ";
        std::getline(std::cin, input);
        if (input.empty()) continue;

        std::vector<std::string> args = parseCommand(input);
        std::string action = args[0];

        if (action == "exit") {
            break;
        } else if (action == "help") {
            printHelp();
        } else if (action == "put" && args.size() >= 3) {
            try {
                int key = std::stoi(args[1]);
                // Combine all remaining arguments as the value (allows spaces in value)
                std::string value = args[2];
                for (size_t i = 3; i < args.size(); i++) {
                    value += " " + args[i];
                }
                if (distributor.getNodeCount() == 0) {
                    std::cout << "No nodes available. Add nodes first." << std::endl;
                    continue;
                }
                int node_idx = key % distributor.getNodeCount();
                std::string local_ip = getLocalIPAddress();
                std::string target_node_endpoint = distributor.getNodeEndpoint(node_idx);
                std::string target_ip = extractIPFromEndpoint(target_node_endpoint);
                bool is_local = (target_ip == local_ip);
                std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
                distributor.put(key, value);
                std::cout << "Put operation completed successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if (action == "get" && args.size() >= 2) {
            try {
                int key = std::stoi(args[1]);
                if (distributor.getNodeCount() == 0) {
                    std::cout << "No nodes available. Add nodes first." << std::endl;
                    continue;
                }
                int node_idx = key % distributor.getNodeCount();
                std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
                std::string value = distributor.get(key);
                std::cout << "Value: " << value << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if (action == "update" && args.size() >= 3) {
            try {
                int key = std::stoi(args[1]);
                std::string value = args[2];
                for (size_t i = 3; i < args.size(); i++) {
                    value += " " + args[i];
                }
                if (distributor.getNodeCount() == 0) {
                    std::cout << "No nodes available. Add nodes first." << std::endl;
                    continue;
                }
                int node_idx = key % distributor.getNodeCount();
                std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
                distributor.update(key, value);
                std::cout << "Update operation completed successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if (action == "delete" && args.size() >= 2) {
            try {
                int key = std::stoi(args[1]);
                if (distributor.getNodeCount() == 0) {
                    std::cout << "No nodes available. Add nodes first." << std::endl;
                    continue;
                }
                int node_idx = key % distributor.getNodeCount();
                std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
                distributor.deleteKey(key);
                std::cout << "Delete operation completed successfully" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if (action == "addnode" && args.size() >= 2) {
            try {
                std::string endpoint = args[1];
                distributor.addNode(endpoint, provider_id);
                std::cout << "Node added: " << endpoint << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error adding node: " << e.what() << std::endl;
            }
        } else if (action == "removenode" && args.size() >= 2) {
            try {
                int node_index = std::stoi(args[1]);
                distributor.removeNode(node_index);
                std::cout << "Node removed at index: " << node_index << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error removing node: " << e.what() << std::endl;
            }
        } else if (action == "listnodes") {
            distributor.listNodes();
        } else if (action == "distribution") {
            distributor.printKeyDistribution();
        } else if (action == "hash" && args.size() >= 2) {
            try {
                int key = std::stoi(args[1]);
                if (distributor.getNodeCount() == 0) {
                    std::cout << "No nodes available. Add nodes first." << std::endl;
                    continue;
                }
                int node_idx = key % distributor.getNodeCount();
                std::string endpoint = distributor.getNodeEndpoint(node_idx);
                std::cout << "Key " << key << " would be assigned to Node " << node_idx 
                         << " (" << endpoint << ")" << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << std::endl;
            }
        } else if (action == "benchmark") {
            runBenchmark(distributor);
        } else if (action == "benchmark1") {
            runBenchmark1(distributor);
        }

	else if (action == "status") {
    std::cout << "\n=== Cluster Status ===" << std::endl;
    std::cout << "Storage Mode: " << (storage_mode == StorageMode::PERSISTENT ? "PERSISTENT" : "IN-MEMORY") << std::endl;
    std::cout << "Nodes in cluster: " << distributor.getNodeCount() << std::endl;
    if (storage_mode == StorageMode::PERSISTENT) {
        std::cout << "Data file: kvstore_persistent.dat" << std::endl;
    } else {
        std::cout << "Memory size: " << mem_size / (1024*1024) << "MB" << std::endl;
    }
    distributor.listNodes();
	}

	 else {
            std::cout << "Unknown command. Type 'help' for available commands." << std::endl;
        }
    }

    std::cout << "Goodbye!" << std::endl;
    return 0;
}
