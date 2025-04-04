// Entry point for kv_client_manager
#include "KVClient.hpp"

int main() {
    KVClient client("ofi+tcp");

    std::string server_endpoint;
    std::cout << "Enter the endpoint of the KV server: ";
    std::cin >> server_endpoint;

    int key;
    std::cout << "Enter key to fetch: ";
    std::cin >> key;

    std::unordered_map<int, double> mp;
    client.fetch(key, server_endpoint, mp);

    std::cout << "Enter key and value to insert: ";
    double value;
    std::cin >> key >> value;
    client.insert(key, value, server_endpoint);

    std::cout << "Enter key and new value to update: ";
    std::cin >> key >> value;
    client.update(key, value, server_endpoint);

    return 0;
}