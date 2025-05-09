#include "KVClient.hpp"
#include "ThalliumDistributor.hpp"
#include <limits>
#include <chrono>  // Add timing capabilities

int main() {
    uint16_t provider_id = 1;
    tl::engine myEngine("ofi+tcp", THALLIUM_CLIENT_MODE);
    ThalliumDistributor distributor(myEngine, provider_id);
    // Initialize with your two hosts - in specific order
    // Node 0: 10.10.1.56 (odd keys)
    distributor.addNode("ofi+tcp://10.10.3.49:8080", provider_id);
    // Node 1: 10.10.1.81 (even keys)
    distributor.addNode("ofi+tcp://10.10.1.81:8080", provider_id);
    std::cout << "Key distribution rule:" << std::endl;
    std::cout << "  Odd keys -> Node 0 (10.10.3.49)" << std::endl;
    std::cout << "  Even keys -> Node 1 (10.10.1.81)" << std::endl;
    std::cout << "Distributed Key-Value Store" << std::endl;
    std::cout << "===========================" << std::endl;
    distributor.listNodes();
    std::cout << "Commands:" << std::endl;
    std::cout << "  put <key> <value> - Store a key-value pair" << std::endl;
    std::cout << "  get <key> - Get a value for a key" << std::endl;
    std::cout << "  update <key> <value> - Update an existing key-value pair" << std::endl;
    std::cout << "  delete <key> - Delete a key-value pair" << std::endl;
    std::cout << "  check <key> - Check which node would handle a key" << std::endl;
    std::cout << "  add <endpoint> - Add a new node" << std::endl;
    std::cout << "  list - List all nodes" << std::endl;
    std::cout << "  direct - Switch to direct node mode" << std::endl;
    std::cout << "  demo - Demonstrate distribution and fetch" << std::endl;
    std::cout << "  exit - Exit the program" << std::endl;
    bool directMode = false;
    KVClient* client = nullptr;
    std::string current_server;
    while (true) {
        std::string command;
        std::cout << "\n> ";
        std::getline(std::cin, command);
        if (command.empty()) continue;
        std::string action = command.substr(0, command.find(' '));
        if (action == "exit") {
            break;
        } else if (action == "direct") {
            directMode = true;
            delete client;
            client = new KVClient("ofi+tcp", provider_id);
            std::cout << "Enter the endpoint of the KV server: ";
            std::cin >> current_server;
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Switched to direct mode with server: " << current_server << std::endl;
        } else if (action == "list") {
            distributor.listNodes();
        } else if (action == "add") {
            size_t space_pos = command.find(' ');
            if (space_pos == std::string::npos) {
                std::cout << "Usage: add <endpoint>\n";
                continue;            }
            std::string endpoint = command.substr(space_pos + 1);
            distributor.addNode(endpoint, provider_id);
        } else if (action == "put") {
            size_t first_space = command.find(' ');
            if (first_space == std::string::npos) {
                std::cout << "Usage: put <key> <value>\n";
                continue;
            }
            size_t second_space = command.find(' ', first_space + 1);
            if (second_space == std::string::npos) {
                std::cout << "Usage: put <key> <value>\n";
                continue;
            }
            try {
                int key = std::stoi(command.substr(first_space + 1, second_space - first_space - 1));
                std::string value = command.substr(second_space + 1);
                if (directMode && client) {
                    client->insert(key, value, current_server);
                } else {
                    distributor.put(key, value);
                }
            } catch (...) {
                std::cout << "Invalid key format. Key should be an integer.\n";
            }
        } else if (action == "update") {
            // Add update command handling
            size_t first_space = command.find(' ');
            if (first_space == std::string::npos) {
                std::cout << "Usage: update <key> <value>\n";
                continue;
            }
            size_t second_space = command.find(' ', first_space + 1);
            if (second_space == std::string::npos) {
                std::cout << "Usage: update <key> <value>\n";
                continue;
            }
            try {
                int key = std::stoi(command.substr(first_space + 1, second_space - first_space - 1));
                std::string value = command.substr(second_space + 1);
                if (directMode && client) {
                    client->update(key, value, current_server);
                } else {
                    distributor.update(key, value);
                }
            } catch (...) {
                std::cout << "Invalid key format. Key should be an integer.\n";
            }
        } else if (action == "delete") {
            // Add delete command handling
            size_t space_pos = command.find(' ');
            if (space_pos == std::string::npos) {
                std::cout << "Usage: delete <key>\n";
                continue;
            }
            try {
                int key = std::stoi(command.substr(space_pos + 1));
                if (directMode && client) {
                    client->deleteKey(key, current_server);
                } else {
                    distributor.deleteKey(key);
                }
            } catch (...) {
                std::cout << "Invalid key format. Key should be an integer.\n";
            }
        } else if (action == "get") {
            size_t space_pos = command.find(' ');
            if (space_pos == std::string::npos) {
                std::cout << "Usage: get <key>\n";
                continue;
            }
            try {
                int key = std::stoi(command.substr(space_pos + 1));
                std::string value;
                if (directMode && client) {
                    value = client->fetch(key, current_server);
                } else {
                    std::cout << "Key " << key << " is " << (key % 2 == 0 ? "even" : "odd") << std::endl;
                    std::cout << "Fetching from Node " << (key % 2 == 0 ? "1 (10.10.1.81)" : "0 (10.10.3.49)") << std::endl;
                    value = distributor.get(key);
                }
                std::cout << "Value: " << value << std::endl;
            } catch (...) {
                std::cout << "Invalid key format. Key should be an integer.\n";
            }
        } else if (action == "demo") {
            // Demonstrate the even-odd distribution
            std::cout << "\n==== DISTRIBUTION DEMONSTRATION ====\n" << std::endl;
            // Store sample key-value pairs
            int oddKey = 101;
            int evenKey = 102;
            std::cout << "Inserting keys..." << std::endl;
            std::cout << "  " << oddKey << " (odd) -> should go to Node 0 (10.10.3.49)" << std::endl;
            distributor.put(oddKey, "Odd key value");
            std::cout << "  " << evenKey << " (even) -> should go to Node 1 (10.10.1.81)" << std::endl;
            distributor.put(evenKey, "Even key value");
            // Fetch the keys to show they were stored correctly
            std::cout << "\nFetching keys..." << std::endl;
            std::cout << "  Key " << oddKey << " (odd) from Node 0: ";
            std::string oddValue = distributor.get(oddKey);
            std::cout << oddValue << std::endl;
            std::cout << "  Key " << evenKey << " (even) from Node 1: ";
            std::string evenValue = distributor.get(evenKey);
            std::cout << evenValue << std::endl;
            
            // Add update and delete demonstrations
            std::cout << "\nUpdating keys..." << std::endl;
            distributor.update(oddKey, "Updated odd key value");
            distributor.update(evenKey, "Updated even key value");
            
            std::cout << "\nFetching updated keys..." << std::endl;
            std::string updatedOddValue = distributor.get(oddKey);
            std::string updatedEvenValue = distributor.get(evenKey);
            
            std::cout << "\nDeleting keys..." << std::endl;
            distributor.deleteKey(oddKey);
            distributor.deleteKey(evenKey);
            
            std::cout << "\n==== DEMONSTRATION COMPLETE ====\n" << std::endl;
        } else {
            std::cout << "Unknown command: " << action << std::endl;
        }
    }
    delete client;
    std::cout << "Exiting...\n";
    return 0;
}
