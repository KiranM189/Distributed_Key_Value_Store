#include "KVClient.hpp"
#include <thallium/serialization/stl/string.hpp>
#include <chrono>
#include <fstream>
#include <sstream>
#include <unistd.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "kvstore.hpp"

std::string KVClient::getLocalAddress() {
    // Get local hostname/IP
    char hostname[256];
    if (gethostname(hostname, sizeof(hostname)) != 0) {
        return "";
    }
    
    // Convert hostname to IP if needed
    struct hostent* host_entry = gethostbyname(hostname);
    if (host_entry == nullptr) {
        return "";
    }
    
    // Get the first IP address
    char* ip = inet_ntoa(*((struct in_addr*)host_entry->h_addr_list[0]));
    
    // Return in the format matching mappings.txt (ofi+tcp://IP:port)
    // Note: You may need to adjust the port based on your setup
    return std::string("ofi+tcp://") + ip + ":8080";
}

bool KVClient::isKeyLocal(int key, std::string& local_address) {
    std::ifstream file("mappings.txt");
    if (!file.is_open()) {
        return false;
    }
    
    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        int file_key;
        std::string address;
        int node_number;
        
        if (iss >> file_key >> address >> node_number) {
            if (file_key == key) {
                // Check if this address matches local address
                if (address == local_address) {
                    return true;
                }
                return false;
            }
        }
    }
    return false;
}

// Updated fetchFromLocal method
std::string KVClient::fetchFromLocal(int key) {
    try {
        // Get the KvStore instance using the configured shared memory size
        KvStore& store = KvStore::get_instance(shared_memory_size);
        std::string value = store.Find(key);
        
        // Check if key was found (KvStore returns "key not found" if not found)
        if (value == "key not found" || value.find("Error:") == 0) {
            return "";
        }
        
        return value;
    } catch (const std::exception& e) {
        std::cerr << "Error fetching from local storage: " << e.what() << std::endl;
        return "";
    }
}


// Updated Constructor
KVClient::KVClient(const std::string& protocol, uint16_t provider_id, std::size_t mem_size)
        : myEngine(protocol, THALLIUM_CLIENT_MODE), provider_id(provider_id), shared_memory_size(mem_size) {}

std::string KVClient::fetch(int key, std::string& server_endpoint) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // Check if key is local
    std::string local_address = getLocalAddress();
    if (!local_address.empty() && isKeyLocal(key, local_address)) {
        // Fetch from local storage
        std::string value = fetchFromLocal(key);
        
        if (!value.empty()) {
            auto end = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> elapsed = end - start;
            
            std::cout << "Fetched Key: " << key << " with Value: " << value << std::endl;
            std::cout << "Fetch operation completed in " << elapsed.count() << " ms" << std::endl;
            return value;
        }
    }
    
    // Key is remote or not found locally, use RPC
    tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);
    try {
        tl::provider_handle ph(server_ep, this->provider_id);
        std::string value = remote_kv_fetch.on(ph)(key);
        // Calculate elapsed time
        auto end = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double, std::milli> elapsed = end - start;
        
        // Print both key and value
        std::cout << "Fetched Key: " << key << " with Value: " << value << std::endl;
        std::cout << "Fetch operation completed in " << elapsed.count() << " ms" << std::endl;
        return value;
    }
    catch (const std::exception &e){
        std::cerr << "Insert operation failed: " << e.what() << std::endl;
        return e.what();
    }
}

void KVClient::insert(int key, const std::string value, const std::string &server_endpoint) {
    tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);
    try {
        tl::provider_handle ph(server_ep, this->provider_id);
        remote_kv_insert.on(ph)(key, value);
        std::cout << "Inserted successfully: " << key << " -> " << value << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Insert operation failed: " << e.what() << std::endl;
    }
}

void KVClient::update(int key, const std::string value, const std::string &server_endpoint) {
    tl::remote_procedure remote_kv_update = myEngine.define("kv_update");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);
    try {
        tl::provider_handle ph(server_ep, this->provider_id);
        remote_kv_update.on(ph)(key, value);
        std::cout << "Updated successfully: " << key << " -> " << value << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Update operation failed: " << e.what() << std::endl;
    }
}

// Add delete function implementation
void KVClient::deleteKey(int key, const std::string &server_endpoint) {
    tl::remote_procedure remote_kv_delete = myEngine.define("kv_delete");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);
    try {
        tl::provider_handle ph(server_ep, this->provider_id);
        remote_kv_delete.on(ph)(key);
        std::cout << "Deleted successfully: Key " << key << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Delete operation failed: " << e.what() << std::endl;
    }
}
