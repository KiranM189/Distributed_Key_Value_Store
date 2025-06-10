#ifndef KVCLIENT_HPP
#define KVCLIENT_HPP
#include <iostream>
#include <thallium.hpp>
#include <unordered_map>
#include <chrono>

namespace tl = thallium;

// Remove the custom string serialization methods

class KVClient {
private:
    tl::engine myEngine;
    uint16_t provider_id;
    std::size_t shared_memory_size;
    
    // Private helper methods
    std::string getLocalAddress();
    bool isKeyLocal(int key, std::string& local_address);
    std::string fetchFromLocal(int key);
    bool insertToLocal(int key, const std::string& value);
    bool updateInLocal(int key, const std::string& value);
    bool deleteFromLocal(int key);
    
public:
    KVClient(const std::string& protocol, uint16_t provider_id, std::size_t mem_size = 100 * 1024 * 1024);
    std::string fetch(int key, std::string& server_endpoint);
    void insert(int key, const std::string value, const std::string& server_endpoint);
    void update(int key, const std::string value, const std::string& server_endpoint);
    void deleteKey(int key, const std::string& server_endpoint);
};
#endif // KVCLIENT_HPP
