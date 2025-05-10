#include "KVClient.hpp"
#include "ThalliumDistributor.hpp"
#include <limits>
#include <chrono>
#include <sstream>
#include <cstdlib>
#include <vector>
#include <algorithm>

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
    std::cout << "  hash <key>               - Show which node a key would be assigned to" << std::endl;
    std::cout << "  help                     - Show this help message" << std::endl;
    std::cout << "  exit                     - Exit the program" << std::endl;
}

int main(int argc, char** argv) {
    uint16_t provider_id = 1;
    tl::engine myEngine("ofi+tcp", THALLIUM_CLIENT_MODE);
    ThalliumDistributor distributor(myEngine, provider_id);
    
    std::cout << "\nModulo-based Key-Value Store" << std::endl;
    std::cout << "===========================" << std::endl;
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
                distributor.put(key, value);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: put <key> <value>\n";
            }
        } else if (action == "update" && args.size() >= 3) {
            try {
                int key = std::stoi(args[1]);
                // Combine all remaining arguments as the value
                std::string value = args[2];
                for (size_t i = 3; i < args.size(); i++) {
                    value += " " + args[i];
                }
                distributor.update(key, value);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: update <key> <value>\n";
            }
        } else if (action == "delete" && args.size() >= 2) {
            try {
                int key = std::stoi(args[1]);
                distributor.deleteKey(key);
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: delete <key>\n";
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
                if (value != "Key not found in mappings" && value != "RPC fetch failed" && value != "Connection failed") {
                    std::cout << "Value: " << value << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: get <key>\n";
            }
        } else if (action == "addnode" && args.size() >= 2) {
            std::string endpoint = args[1];
            distributor.addNode(endpoint, provider_id);
            std::cout << "Node added. New cluster size: " << distributor.getNodeCount() << std::endl;
        } else if (action == "removenode" && args.size() >= 2) {
            try {
                int node_idx = std::stoi(args[1]);
                if (distributor.removeNode(node_idx)) {
                    std::cout << "Node removed. New cluster size: " << distributor.getNodeCount() << std::endl;
                }
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: removenode <node_index>\n";
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
                std::cout << "Key " << key << " would be assigned to Node " << node_idx << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Error: " << e.what() << "\nUsage: hash <key>\n";
            }
        } else {
            std::cout << "Unknown or incomplete command: " << action << std::endl;
            std::cout << "Type 'help' for available commands." << std::endl;
        }
    }
    
    std::cout << "Exiting distributed key-value store client..." << std::endl;
    return 0;
}
