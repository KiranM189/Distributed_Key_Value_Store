#include "KVClient.hpp"
#include "kvstore.hpp"

int main() {
    uint16_t provider_id = 1;
    KvStore &kv = KvStore::get_instance(1024);
    KVClient client("ofi+tcp",kv,provider_id);
    std::string server_endpoint;
    std::cout << "Enter the endpoint of the KV server: \n";
    std::cin >> server_endpoint;

    int choice;
    while (true) {
        std::cout << "\n1. Find inside Shared Mem\n";
        std::cout << "2. Delete inside shared Mem\n";
        std::cout << "3. Update inside Shared Mem\n";
        std::cout << "4. Insert inside Shared Mem\n";
        std::cout << "5. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        int key;
        std::string value;

        switch (choice) {
            case 1: {
                std::cout << "Enter the key to find its value: ";
                std::cin >> key;
                client.fetch(key, server_endpoint);
                break;
            }
            case 2:{
                std::cout << "Enter the key to delete: ";
                std::cin >> key;
                client.delete_(key, server_endpoint);
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