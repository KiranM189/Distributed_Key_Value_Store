#ifndef THALLIUM_DISTRIBUTOR_HPP
#define THALLIUM_DISTRIBUTOR_HPP
#include <iostream>
#include <vector>
#include <fstream>
#include <unordered_map>
#include <string>
#include <thallium.hpp>
#include <boost/functional/hash.hpp>
// REMOVE these includes - they're causing the conflict
// #include <cereal/types/string.hpp>
// #include <cereal/types/tuple.hpp>

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
        //boost::hash<int> int_hash;
        //return int_hash(key) % nodes.size();
	return key % 2 ==0 ? 1:0;
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
public:
    ThalliumDistributor(tl::engine& engine, uint16_t provider_id)
        : myEngine(engine), provider_id(provider_id) {
        loadMappings();
    }

    void put(int key, const std::string &value) {
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
                
                // Also update the mappings file by rewriting it
                // This is a simple implementation - you might want to optimize this for large sets
                std::ofstream file("mappings.txt");
                if (file.is_open()) {
                    for (const auto& mapping : key_to_node) {
                        file << mapping.first << " " << nodes[mapping.second].first << " " << mapping.second << "\n";
                    }
                    file.close();
                } else {
                    std::cerr << "Error: Unable to open file for updating mappings.\n";
                }
            } catch (const std::exception& e) {
                std::cerr << "Failed to invoke RPC: " << e.what() << std::endl;
            }
        } catch (std::exception &e) {
            std::cerr << "Failed to connect to node " << node_idx << " for delete operation." << std::endl;
        }
    }

    // Method to add a new node
    void addNode(const std::string& endpoint, uint16_t node_provider_id) {
        nodes.push_back(std::make_pair(endpoint, node_provider_id));
        std::cout << "Added node " << (nodes.size() - 1) << " at " << endpoint << std::endl;
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
};
#endif // THALLIUM_DISTRIBUTOR_HPP
