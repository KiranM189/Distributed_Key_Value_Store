#ifndef THALLIUM_DISTRIBUTOR_HPP
#define THALLIUM_DISTRIBUTOR_HPP
#include "kvstore.hpp"  // Include the full class definition
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <thallium.hpp>
#include <boost/functional/hash.hpp>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <chrono>
#include <ifaddrs.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <mutex>

namespace tl = thallium;

// Forward declaration of KvStore class
class KvStore;

// Keep only ONE serialization method for std::string
namespace thallium {
    template<typename... CtxArg>
    void serialize(proc_output_archive<CtxArg...>& ar, std::string& str) {
        size_t size = str.size();
        ar.write(&size, sizeof(size));
        if(size > 0)
            ar.write(str.data(), size);
    }

    template<typename... CtxArg>
    void serialize(proc_input_archive<CtxArg...>& ar, std::string& str) {
        size_t size;
        ar.read(&size, sizeof(size));
        str.resize(size);
        if(size > 0)
            ar.read(&str[0], size);
    }
}

class ThalliumDistributor {
private:
    struct ConnectionInfo {
        tl::endpoint endpoint;
        tl::provider_handle provider_handle;
        std::chrono::steady_clock::time_point last_used;
        bool is_valid;
        
        ConnectionInfo() : is_valid(false) {}
        ConnectionInfo(tl::endpoint ep, tl::provider_handle ph)
            : endpoint(std::move(ep)), provider_handle(std::move(ph)),
              last_used(std::chrono::steady_clock::now()), is_valid(true) {}
    };
    
    std::vector<ConnectionInfo> connection_pool;
    std::mutex connection_mutex; // For thread safety
    
    // Reference to local KvStore instead of raw unordered_map
    KvStore* local_kv_store;
    
    // Pre-establish connections when adding nodes
    void establishConnection(int node_idx) {
        try {
            tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
            tl::provider_handle ph(server_ep, nodes[node_idx].second);
            
            // Expand connection pool if needed
            if (connection_pool.size() <= node_idx) {
                connection_pool.resize(node_idx + 1);
            }
            connection_pool[node_idx] = ConnectionInfo(std::move(server_ep), std::move(ph));
            std::cout << "Pre-established connection to node " << node_idx << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Failed to pre-establish connection to node " << node_idx
                      << ": " << e.what() << std::endl;
            if (connection_pool.size() > node_idx) {
                connection_pool[node_idx].is_valid = false;
            }
        }
    }
    
    // Get or create connection (with fallback)
    ConnectionInfo& getConnection(int node_idx) {
        std::lock_guard<std::mutex> lock(connection_mutex);
        
        // Ensure pool is large enough
        if (connection_pool.size() <= node_idx) {
            connection_pool.resize(node_idx + 1);
        }
        
        // Check if existing connection is valid and recent
        auto& conn = connection_pool[node_idx];
        auto now = std::chrono::steady_clock::now();
        auto age = std::chrono::duration_cast<std::chrono::seconds>(now - conn.last_used);

        if (!conn.is_valid || age.count() > 30) { // Refresh connections older than 30s
            try {
                tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                conn = ConnectionInfo(std::move(server_ep), std::move(ph));
            } catch (const std::exception& e) {
                std::cerr << "Failed to establish connection: " << e.what() << std::endl;
                conn.is_valid = false;
            }
        } else {
            conn.last_used = now; // Update last used time
        }
        return conn;
    }
    
    tl::engine& myEngine;
    std::vector<std::pair<std::string, uint16_t>> nodes; // endpoints and provider_ids
    std::unordered_map<int, int> key_to_node; // Stores known key-node mappings
    uint16_t provider_id;
    int local_node_id; // ID of the local node (will be auto-detected)
    
    // Get local IP addresses of the machine
    std::vector<std::string> getLocalIPAddresses() {
        std::vector<std::string> local_ips;
        struct ifaddrs *ifaddr, *ifa;
        char ip_str[INET_ADDRSTRLEN];
        
        if (getifaddrs(&ifaddr) == -1) {
            std::cerr << "Error getting network interfaces" << std::endl;
            return local_ips;
        }

        for (ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next) {
            if (ifa->ifa_addr == nullptr) continue;
            
            // Only consider IPv4 addresses
            if (ifa->ifa_addr->sa_family == AF_INET) {
                struct sockaddr_in* addr_in = (struct sockaddr_in*)ifa->ifa_addr;
                inet_ntop(AF_INET, &(addr_in->sin_addr), ip_str, INET_ADDRSTRLEN);
                std::string ip(ip_str);
                
                // Skip loopback addresses
                if (ip != "127.0.0.1") {
                    local_ips.push_back(ip);
                }
            }
        }
        freeifaddrs(ifaddr);
        return local_ips;
    }
    
    // Extract IP from Thallium endpoint (format: "tcp://IP:PORT")
    std::string extractIPFromEndpoint(const std::string& endpoint) {
        size_t start = endpoint.find("//");
        if (start == std::string::npos) return "";
        start += 2; // Skip "//"
        
        size_t end = endpoint.find(":", start);
        if (end == std::string::npos) return "";
        
        return endpoint.substr(start, end - start);
    }

    // Determine which node is local based on IP addresses
    void detectLocalNode() {
        local_node_id = -1; // Initialize as invalid
        std::vector<std::string> local_ips = getLocalIPAddresses();
        
        std::cout << "Local IP addresses detected: ";
        for (const auto& ip : local_ips) {
            std::cout << ip << " ";
        }
        std::cout << std::endl;
        
        // Check each node to see if its IP matches any local IP
        for (size_t i = 0; i < nodes.size(); i++) {
            std::string node_ip = extractIPFromEndpoint(nodes[i].first);
            std::cout << "Node " << i << " endpoint: " << nodes[i].first << " -> IP: " << node_ip << std::endl;
            
            for (const auto& local_ip : local_ips) {
                if (node_ip == local_ip) {
                    local_node_id = static_cast<int>(i);
                    std::cout << "*** DETECTED LOCAL NODE: Node " << local_node_id
                              << " (" << nodes[i].first << ") matches local IP " << local_ip << " ***" << std::endl;
                    return;
                }
            }
        }
        
        if (local_node_id == -1) {
            std::cout << "WARNING: Could not detect local node. No cluster node IP matches this machine's IP addresses." << std::endl;
            std::cout << "This client will operate in remote-only mode." << std::endl;
        }
    }

    void loadMappings() {
        std::ifstream file("mappings.txt");
        if (file.is_open()) {
            int key, node_idx;
            std::string node_endpoint;
            while (file >> key >> node_endpoint >> node_idx) {
                key_to_node[key] = node_idx;
            }
            file.close();
            std::cout << "Loaded " << key_to_node.size() << " key mappings from file.\n";
        } else {
            std::cout << "No existing mappings file found. Starting fresh.\n";
        }
    }
    
    void saveMapping(int key, int node_idx) {
        std::ofstream file("mappings.txt", std::ios::app);
        if (file.is_open()) {
            file << key << " " << nodes[node_idx].first << " " << node_idx << "\n";
            file.close();
        } else {
            std::cerr << "Error: Unable to open file for saving mappings.\n";
        }
    }
    
    int getNodeIndex(int key) {
        // Modulo-based hashing: key % number_of_nodes
        if (nodes.empty()) {
            throw std::runtime_error("No nodes available in the cluster");
        }
        return key % nodes.size();
    }
    
    // Check if a key should be accessed locally
    bool isLocalKey(int key) {
        if (local_node_id == -1) {
            return false; // No local node detected
        }
        if (key_to_node.find(key) == key_to_node.end()) {
            return false;
        }
        return key_to_node[key] == local_node_id;
    }
    
    // Modified fetch function - direct local access when possible
    std::string fetchFromNode(int key, int node_idx) {
        // Debug prints
        std::cout << "DEBUG: fetchFromNode - key=" << key << ", node_idx=" << node_idx 
                  << ", local_node_id=" << local_node_id 
                  << ", local_kv_store=" << (local_kv_store ? "valid" : "null") << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        
        // Direct local access using KvStore - NO RPC CALLS FOR LOCAL OPERATIONS
        if (node_idx == local_node_id && local_kv_store != nullptr) {
            try {
                std::string value = local_kv_store->Find(key);
                auto end = std::chrono::high_resolution_clock::now();
                auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
                std::cout << "Local fetch via direct KvStore access completed in "
                          << duration.count() << " microseconds" << std::endl;
                return value;
            } catch (const std::exception& e) {
                std::cerr << "Local KvStore fetch failed: " << e.what() << std::endl;
                return "Key not found";
            }
        }
        
        // Remote access via RPC (existing code)
        auto& conn = getConnection(node_idx);
        if (!conn.is_valid) {
            return "Connection failed";
        }

        try {
            tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
            std::string value = remote_kv_fetch.on(conn.provider_handle)(key);
            auto end = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            std::cout << "Remote fetch via RPC completed in "
                      << duration.count() << " microseconds" << std::endl;
            return value;
        } catch (const std::exception& e) {
            std::cerr << "RPC failed, invalidating connection: " << e.what() << std::endl;
            conn.is_valid = false; // Mark for refresh
            return "RPC fetch failed";
        }
    }
    
    // Modified send function - direct local access when possible
    bool sendToNode(int node_idx, int key, const std::string &value) {
        // Debug prints
        std::cout << "DEBUG: sendToNode - key=" << key << ", node_idx=" << node_idx 
                  << ", local_node_id=" << local_node_id 
                  << ", local_kv_store=" << (local_kv_store ? "valid" : "null") << std::endl;
        
        // Direct local access using KvStore - NO RPC CALLS FOR LOCAL OPERATIONS
        if (node_idx == local_node_id && local_kv_store != nullptr) {
            try {
                local_kv_store->Insert(key, value);
                std::cout << "Data stored via direct KvStore access successfully" << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Local KvStore put failed: " << e.what() << std::endl;
                return false;
            }
        }
        
        // Remote access via RPC (existing code)
        auto& conn = getConnection(node_idx);
        if (!conn.is_valid) {
            return false;
        }
        
        try {
            tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
            remote_kv_insert.on(conn.provider_handle)(key, value);
            std::cout << "Data sent via RPC to remote kvstore successfully" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
            conn.is_valid = false;
            return false;
        }
    }
    
    // Helper function for local delete operations - NO RPC CALLS
    bool deleteFromNode(int node_idx, int key) {
        // Debug prints
        std::cout << "DEBUG: deleteFromNode - key=" << key << ", node_idx=" << node_idx 
                  << ", local_node_id=" << local_node_id 
                  << ", local_kv_store=" << (local_kv_store ? "valid" : "null") << std::endl;
        
        // Direct local access using KvStore - NO RPC CALLS FOR LOCAL OPERATIONS
        if (node_idx == local_node_id && local_kv_store != nullptr) {
            try {
                local_kv_store->Delete(key);
                std::cout << "Key deleted via direct KvStore access successfully" << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Local KvStore delete failed: " << e.what() << std::endl;
                return false;
            }
        }
        
        // Remote delete via RPC
        auto& conn = getConnection(node_idx);
        if (!conn.is_valid) {
            return false;
        }
        
        try {
            tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
            remote_kv_delete.on(conn.provider_handle)(key);
            std::cout << "Key deleted via RPC from remote kvstore successfully" << std::endl;
            return true;
        } catch (const std::exception& e) {
            std::cerr << "Failed to delete via RPC: " << e.what() << std::endl;
            conn.is_valid = false;
            return false;
        }
    }
    
    void rebalanceKeys(int old_node_count) {
        if (nodes.size() <= old_node_count) {
            std::cout << "No rebalancing needed - node count hasn't increased.\n";
            return;
        }
        
        std::cout << "Starting key rebalancing...\n";
        std::vector<std::pair<int, std::string>> keys_to_rebalance;
        
        // Identify keys that need to be moved
        for (const auto& mapping : key_to_node) {
            int key = mapping.first;
            int current_node = mapping.second;
            int new_node = getNodeIndex(key);
            
            if (new_node != current_node) {
                std::cout << "Key " << key << " will move from node " << current_node
                          << " to node " << new_node << std::endl;
                
                // Fetch the value from the current node using unified fetch
                try {
                    std::string value = fetchFromNode(key, current_node);
                    if (value != "Connection failed" && value != "RPC fetch failed" && value != "Key not found") {
                        keys_to_rebalance.push_back({key, value});
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Failed to fetch key " << key << " for rebalancing: "
                              << e.what() << std::endl;
                }
            }
        }
        
        // Move the keys to their new nodes
        for (const auto& item : keys_to_rebalance) {
            int key = item.first;
            const std::string& value = item.second;
            int old_node = key_to_node[key];
            int new_node = getNodeIndex(key);
            
            // Insert to new node
            if (sendToNode(new_node, key, value)) {
                // Delete from old node using the unified delete function
                if (deleteFromNode(old_node, key)) {
                    // Update the mapping
                    key_to_node[key] = new_node;
                    std::cout << "Rebalanced key " << key << " from node " << old_node
                              << " to node " << new_node << std::endl;
                } else {
                    std::cerr << "Failed to delete key " << key << " from old node " << old_node << std::endl;
                }
            }
        }
        
        // Update the mappings file after rebalancing
        updateMappingsFile();
        std::cout << "Rebalancing complete. " << keys_to_rebalance.size()
                  << " keys were redistributed.\n";
    }
    
    void updateMappingsFile() {
        std::ofstream file("mappings.txt");
        if (file.is_open()) {
            for (const auto& mapping : key_to_node) {
                file << mapping.first << " " << nodes[mapping.second].first
                     << " " << mapping.second << "\n";
            }
            file.close();
            std::cout << "Updated mappings file with " << key_to_node.size() << " entries.\n";
        } else {
            std::cerr << "Error: Unable to open file for updating mappings.\n";
        }
    }

public:
    // Modified constructor to accept KvStore reference
    ThalliumDistributor(tl::engine& engine, uint16_t provider_id,
                       KvStore* local_store = nullptr)
        : myEngine(engine), provider_id(provider_id), local_node_id(-1),
          local_kv_store(local_store) {
        loadMappings();
        std::cout << "ThalliumDistributor initialized. Local node will be auto-detected when nodes are added." << std::endl;
    }
    
    // Method to set local store reference (if not provided in constructor)
    void setLocalStore(KvStore* local_store) {
        local_kv_store = local_store;
        std::cout << "Local KvStore reference set: " << (local_kv_store ? "valid" : "null") << std::endl;
    }
    
    // Update addNode to pre-establish connections
    void addNode(const std::string& endpoint, uint16_t node_provider_id) {
        int old_node_count = nodes.size();
        nodes.push_back(std::make_pair(endpoint, node_provider_id));
        int new_node_idx = nodes.size() - 1;
        
        std::cout << "Added node " << new_node_idx << " at " << endpoint << std::endl;
        
        // Pre-establish connection only for remote nodes
        if (new_node_idx != local_node_id) {
            establishConnection(new_node_idx);
        }

        if (nodes.size() == 1 || local_node_id == -1) {
            detectLocalNode();
        }
        
        if (old_node_count > 0) {
            rebalanceKeys(old_node_count);
        }
    }
    
    // Method to remove a node (with auto-rebalancing)
    bool removeNode(int node_idx) {
        if (node_idx < 0 || node_idx >= static_cast<int>(nodes.size())) {
            std::cerr << "Error: Invalid node index " << node_idx << std::endl;
            return false;
        }
        
        std::cout << "Removing node " << node_idx << " (" << nodes[node_idx].first << ")" << std::endl;
        
        // Find all keys stored on this node
        std::vector<int> keys_on_node;
        for (const auto& mapping : key_to_node) {
            if (mapping.second == node_idx) {
                keys_on_node.push_back(mapping.first);
            }
        }
        
        std::cout << "Found " << keys_on_node.size() << " keys on node " << node_idx << std::endl;
        
        // For each key on this node, get the value and redistribute
        std::vector<std::pair<int, std::string>> keys_to_redistribute;
        for (int key : keys_on_node) {
            try {
                std::string value = fetchFromNode(key, node_idx);
                if (value != "Connection failed" && value != "RPC fetch failed" && value != "Key not found") {
                    keys_to_redistribute.push_back({key, value});
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to fetch key " << key << " from node being removed: "
                          << e.what() << std::endl;
            }
        }
        
        // Check if we're removing the local node
        bool removing_local_node = (node_idx == local_node_id);
        
        // Remove the node from our list
        nodes.erase(nodes.begin() + node_idx);
        
        // Update local_node_id if necessary
        if (removing_local_node) {
            local_node_id = -1; // Reset
            detectLocalNode(); // Re-detect (indexes may have changed)
        } else if (local_node_id > node_idx) {
            local_node_id--; // Adjust for removed node
        }
        
        // Redistribute the keys
        for (const auto& item : keys_to_redistribute) {
            int key = item.first;
            const std::string& value = item.second;
            int new_node = getNodeIndex(key);
            
            if (sendToNode(new_node, key, value)) {
                key_to_node[key] = new_node;
                std::cout << "Redistributed key " << key << " to node " << new_node << std::endl;
            } else {
                std::cerr << "Failed to redistribute key " << key << std::endl;
            }
        }
        
        // Update mappings for all keys (their indexes might have changed)
        for (auto& mapping : key_to_node) {
            // If this key was on a node after the one we removed, adjust its node index
            if (mapping.second > node_idx) {
                mapping.second--;
            }
        }
        
        // Update mappings file
        updateMappingsFile();
        std::cout << "Node removed and " << keys_to_redistribute.size()
                  << " keys were redistributed." << std::endl;
        return true;
    }
    
    void put(int key, const std::string &value) {
        if (nodes.empty()) {
            std::cerr << "Error: No nodes available to store data.\n";
            return;
        }
        
        if (key_to_node.find(key) != key_to_node.end()) {
            std::cerr << "Error: Key " << key << " already exists and was assigned to Node "
                    << key_to_node[key] << "\n";
            return;
        }

        int node_idx = getNodeIndex(key);
        std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
        
        if (sendToNode(node_idx, key, value)) {
            key_to_node[key] = node_idx;
            saveMapping(key, node_idx);
            std::cout << "Stored: " << key << " -> Node " << node_idx
                    << " (" << nodes[node_idx].first << ")\n";
        }
    }
    
    // Modified update method - NO RPC CALLS FOR LOCAL OPERATIONS
    void update(int key, const std::string &value) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings. Cannot update.\n";
            return;
        }
        
        int node_idx = key_to_node[key];
        
        // Debug prints
        std::cout << "DEBUG: update - key=" << key << ", node_idx=" << node_idx 
                  << ", local_node_id=" << local_node_id 
                  << ", local_kv_store=" << (local_kv_store ? "valid" : "null") << std::endl;
        
        // Direct local access using KvStore - NO RPC CALLS FOR LOCAL OPERATIONS
        if (node_idx == local_node_id && local_kv_store != nullptr) {
            try {
                local_kv_store->Insert(key, value);
                std::cout << "Updated via direct KvStore access: " << key << " -> " << value
                          << " on local Node " << node_idx << " (" << nodes[node_idx].first << ")\n";
                return;
            } catch (const std::exception& e) {
                std::cerr << "Local KvStore update failed: " << e.what() << std::endl;
                return;
            }
        }
        
        // Remote access via RPC
        try {
            auto& conn = getConnection(node_idx);
            if (!conn.is_valid) {
                std::cerr << "Failed to establish connection to node " << node_idx << std::endl;
                return;
            }
            
            tl::remote_procedure remote_kv_update = myEngine.define("kv_update");
            remote_kv_update.on(conn.provider_handle)(key, value);
            std::cout << "Updated via RPC: " << key << " -> " << value << " on remote Node "
                      << node_idx << " (" << nodes[node_idx].first << ")\n";
        } catch (const std::exception& e) {
            std::cerr << "Failed to update key " << key << ": " << e.what() << std::endl;
        }
    }
    
    // Modified delete method - NO RPC CALLS FOR LOCAL OPERATIONS
    void deleteKey(int key) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings. Cannot delete.\n";
            return;
        }
        
        int node_idx = key_to_node[key];
        
        // Debug prints
        std::cout << "DEBUG: deleteKey - key=" << key << ", node_idx=" << node_idx 
                  << ", local_node_id=" << local_node_id 
                  << ", local_kv_store=" << (local_kv_store ? "valid" : "null") << std::endl;
        
        // Direct local access using KvStore - NO RPC CALLS FOR LOCAL OPERATIONS
        if (node_idx == local_node_id && local_kv_store != nullptr) {
            try {
                local_kv_store->Delete(key);
                key_to_node.erase(key);
                std::cout << "Deleted via direct KvStore access: Key " << key
                          << " from local Node " << node_idx << " (" << nodes[node_idx].first << ")\n";
                updateMappingsFile();
                return;
            } catch (const std::exception& e) {
                std::cerr << "Local KvStore delete failed: " << e.what() << std::endl;
                return;
            }
        }
        
        // Remote access via RPC
        try {
            auto& conn = getConnection(node_idx);
            if (!conn.is_valid) {
                std::cerr << "Failed to establish connection to node " << node_idx << std::endl;
                return;
            }
            
            tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
            remote_kv_delete.on(conn.provider_handle)(key);
            key_to_node.erase(key);
            std::cout << "Deleted via RPC: Key " << key << " from remote Node "
                      << node_idx << " (" << nodes[node_idx].first << ")\n";
            updateMappingsFile();
        } catch (const std::exception& e) {
            std::cerr << "Failed to delete key " << key << ": " << e.what() << std::endl;
        }
    }
    
    // Modified get method - NO RPC CALLS FOR LOCAL OPERATIONS
    std::string get(int key) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings\n";
            return "Key not found in mappings";
        }

        int node_idx = key_to_node[key];
        std::cout << "Key " << key << " hashes to Node " << node_idx << std::endl;
        
        if (node_idx == local_node_id) {
            std::cout << "Fetching from local Node " << node_idx << " via direct KvStore access" << std::endl;
        } else {
            std::cout << "Fetching from remote Node " << node_idx << " via RPC" << std::endl;
        }
        
        std::string value = fetchFromNode(key, node_idx);
        std::cout << "Fetched Key: " << key << " with Value: " << value
                  << " from Node " << node_idx << std::endl;
        return value;
    }
    
    // Method to list all nodes
    void listNodes() {
        std::cout << "Current nodes in the system:" << std::endl;
        for (size_t i = 0; i < nodes.size(); i++) {
            std::string local_marker = (i == local_node_id) ? " (LOCAL)" : " (REMOTE)";
            std::cout << "Node " << i << ": " << nodes[i].first << local_marker << std::endl;
        }
        if (local_node_id == -1) {
            std::cout << "*** WARNING: No local node detected! All operations will be remote. ***" << std::endl;
        }
    }
    // Get the number of nodes
    size_t getNodeCount() const {
        return nodes.size();
    }
    std::string getNodeEndpoint(int node_index) const {
        if (node_index < 0 || node_index >= static_cast<int>(nodes.size())) {
            std::cerr << "Error: Invalid node index " << node_index << std::endl;
            return ""; // Return empty string for invalid index
        }
        return nodes[node_index].first; // Return the endpoint string (first element of the pair)
    }
    // Get the distribution of keys across nodes
    void printKeyDistribution() {
        std::vector<int> keys_per_node(nodes.size(), 0);
        for (const auto& mapping : key_to_node) {
            if (mapping.second < static_cast<int>(keys_per_node.size())) {
                keys_per_node[mapping.second]++;
            }
        }
        std::cout << "\nKey distribution across nodes:" << std::endl;
        std::cout << "-----------------------------" << std::endl;
        for (size_t i = 0; i < nodes.size(); i++) {
            std::string local_marker = (i == local_node_id) ? " (LOCAL)" : " (REMOTE)";
            std::cout << "Node " << i << " (" << nodes[i].first << ")" << local_marker << ": "
                      << keys_per_node[i] << " keys" << std::endl;
        }
        std::cout << "Total keys: " << key_to_node.size() << std::endl;
    }
    // Method to manually trigger local node detection (useful for debugging)
    void detectAndShowLocalNode() {
        detectLocalNode();
        if (local_node_id != -1) {
            std::cout << "Local node detected: Node " << local_node_id
                      << " (" << nodes[local_node_id].first << ")" << std::endl;
        } else {
            std::cout << "No local node detected." << std::endl;
        }
    }
    // Get local node ID
    int getLocalNodeId() const {
        return local_node_id;
    }
};
#endif // THALLIUM_DISTRIBUTOR_HPP
