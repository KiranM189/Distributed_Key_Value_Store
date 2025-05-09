#include "KVClient.hpp"
#include "kvstore.hpp"
#include <limits>
#include <chrono>  // Add timing capabilities

int main() {
    uint16_t provider_id = 1;
    KVClient client("ofi+tcp",provider_id);
    std::string server_endpoint;
    std::cout << "Enter the endpoint of the KV server: ";
    std::cin >> server_endpoint;
    int choice;
    while (true) {
        std::cout << "\n1. Find inside Shared Mem" << std::endl;
        std::cout << "2. Delete inside Shared Mem" << std::endl;  // Added delete option
        std::cout << "3. Update inside Shared Mem" << std::endl;
        std::cout << "4. Insert inside Shared Mem" << std::endl;
        std::cout << "5. Exit" << std::endl;
        std::cout << "Enter your choice: ";
        std::cin >> choice;
        int key;
        std::string value;
        switch (choice) {
            case 1: {
                std::cout << "Enter the key to find its value: ";
                std::cin >> key;
                
                // Add timing
                auto start = std::chrono::high_resolution_clock::now();
                
                std::string value = client.fetch(key, server_endpoint);
                
                // Calculate elapsed time
                auto end = std::chrono::high_resolution_clock::now();
                std::chrono::duration<double, std::milli> elapsed = end - start;
                
                std::cout << "Key: " << key << ", Value: " << value << std::endl;
                std::cout << "Fetch operation completed in " << elapsed.count() << " ms" << std::endl;
                break;
            }
            case 2: {  // Add delete case
                std::cout << "Enter the key you want to delete: ";
                std::cin >> key;
                client.deleteKey(key, server_endpoint);
                break;
            }
            case 3: {
                std::cout << "Enter the key you want to update: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the new value: ";
                std::getline(std::cin, value);
                client.update(key, value, server_endpoint);
                break;
            }
            case 4: {
                std::cout << "Enter the key you want to insert: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the value: ";
                std::getline(std::cin, value);
                client.insert(key, value, server_endpoint);
                break;
            }
            case 5: {
                std::cout << "Exiting program.\n";
                return 0;
            }
            default:
                std::cout << "Invalid Choice. Try again.\n";
        }
    }
}
