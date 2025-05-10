#ifndef THALLIUM_DISTRIBUTOR_HPP
#define THALLIUM_DISTRIBUTOR_HPP
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <thallium.hpp>
#include <boost/functional/hash.hpp>

namespace tl = thallium;

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
    tl::engine& myEngine;
    std::vector<std::pair<std::string, uint16_t>> nodes; // endpoints and provider_ids
    std::unordered_map<int, int> key_to_node; // Stores known key-node mappings
    uint16_t provider_id;
    
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
    
    bool sendToNode(int node_idx, int key, const std::string &value) {
        try {
            tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
            tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
            std::cout << "Attempting to connect to " << nodes[node_idx].first << std::endl;
            try {
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                remote_kv_insert.on(ph)(key, value);
                std::cout << "Data sent to node successfully" << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
                return false;
            }
        } catch (std::exception &e) {
            std::cerr << "Failed to send data to node " << node_idx << " ("
                    << nodes[node_idx].first << "): " << e.what() << std::endl;
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
                
                // Fetch the value from the current node
                try {
                    tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
                    tl::endpoint server_ep = myEngine.lookup(nodes[current_node].first);
                    tl::provider_handle ph(server_ep, nodes[current_node].second);
                    std::string value = remote_kv_fetch.on(ph)(key);
                    
                    // Store for later rebalancing
                    keys_to_rebalance.push_back({key, value});
                    
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
                // Delete from old node
                try {
                    tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
                    tl::endpoint server_ep = myEngine.lookup(nodes[old_node].first);
                    tl::provider_handle ph(server_ep, nodes[old_node].second);
                    remote_kv_delete.on(ph)(key);
                    
                    // Update the mapping
                    key_to_node[key] = new_node;
                    std::cout << "Rebalanced key " << key << " from node " << old_node 
                              << " to node " << new_node << std::endl;
                    
                } catch (const std::exception& e) {
                    std::cerr << "Failed to delete key " << key << " from old node: " 
                              << e.what() << std::endl;
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
    ThalliumDistributor(tl::engine& engine, uint16_t provider_id)
        : myEngine(engine), provider_id(provider_id) {
        loadMappings();
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
        if (sendToNode(node_idx, key, value)) {
            key_to_node[key] = node_idx;
            saveMapping(key, node_idx);
            std::cout << "Stored: " << key << " -> Node " << node_idx
                    << " (" << nodes[node_idx].first << ")\n";
        }
    }
    
    void update(int key, const std::string &value) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings. Cannot update.\n";
            return;
        }
        
        int node_idx = key_to_node[key];
        try {
            tl::remote_procedure remote_kv_update = myEngine.define("kv_update");
            tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
            std::cout << "Updating on " << nodes[node_idx].first << std::endl;
            try {
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                remote_kv_update.on(ph)(key, value);
                std::cout << "Updated: " << key << " -> " << value << " on Node " << node_idx
                        << " (" << nodes[node_idx].first << ")\n";
            } catch (const std::exception& e) {
                std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
            }
        } catch (std::exception &e) {
            std::cerr << "Failed to connect to node " << node_idx << " for update operation." << std::endl;
        }
    }
    
    void deleteKey(int key) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings. Cannot delete.\n";
            return;
        }
        
        int node_idx = key_to_node[key];
        try {
            tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
            tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
            std::cout << "Deleting from " << nodes[node_idx].first << std::endl;
            try {
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                remote_kv_delete.on(ph)(key);
                // Remove the key from our mapping
                key_to_node.erase(key);
                std::cout << "Deleted: Key " << key << " from Node " << node_idx
                        << " (" << nodes[node_idx].first << ")\n";
                
                // Update the mappings file
                updateMappingsFile();
                
            } catch (const std::exception& e) {
                std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
            }
        } catch (std::exception &e) {
            std::cerr << "Failed to connect to node " << node_idx << " for delete operation." << std::endl;
        }
    }
    
    // Method to add a new node
    void addNode(const std::string& endpoint, uint16_t node_provider_id) {
        int old_node_count = nodes.size();
        nodes.push_back(std::make_pair(endpoint, node_provider_id));
        std::cout << "Added node " << (nodes.size() - 1) << " at " << endpoint << std::endl;
        
        // Rebalance keys when adding nodes after initial setup 
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
                tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
                tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                std::string value = remote_kv_fetch.on(ph)(key);
                keys_to_redistribute.push_back({key, value});
            } catch (const std::exception& e) {
                std::cerr << "Failed to fetch key " << key << " from node being removed: " 
                          << e.what() << std::endl;
            }
        }
        
        // Remove the node from our list
        nodes.erase(nodes.begin() + node_idx);
        
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
    
    std::string get(int key) {
        if (key_to_node.find(key) == key_to_node.end()) {
            std::cerr << "Error: Key " << key << " not found in mappings\n";
            return "Key not found in mappings";
        }
        
        // Add timing
        auto start = std::chrono::high_resolution_clock::now();
        int node_idx = key_to_node[key];
        try {
            tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
            tl::endpoint server_ep = myEngine.lookup(nodes[node_idx].first);
            std::cout << "Fetching from " << nodes[node_idx].first << std::endl;
            try {
                tl::provider_handle ph(server_ep, nodes[node_idx].second);
                std::string value = remote_kv_fetch.on(ph)(key);
                // Calculate elapsed time
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - start;
                // Print key-value pair and timing
                std::cout << "Fetched Key: " << key << " with Value: " << value << std::endl;
                std::cout << "Distributed fetch completed in " << elapsed.count() << " ms" << std::endl;
                return value;
            } catch (const std::exception& e) {
                std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
                return "RPC fetch failed";
            }
        } catch (std::exception &e) {
            std::cerr << "Failed to connect to node " << node_idx << std::endl;
            return "Connection failed";
        }
    }
    
    // Method to list all nodes
    void listNodes() {
        std::cout << "Current nodes in the system:" << std::endl;
        for (size_t i = 0; i < nodes.size(); i++) {
            std::cout << "Node " << i << ": " << nodes[i].first << std::endl;
        }
    }
    
    // Get the number of nodes
    size_t getNodeCount() const {
        return nodes.size();
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
            std::cout << "Node " << i << " (" << nodes[i].first << "): " 
                      << keys_per_node[i] << " keys" << std::endl;
        }
        std::cout << "Total keys: " << key_to_node.size() << std::endl;
    }
};
#endif // THALLIUM_DISTRIBUTOR_HPP
