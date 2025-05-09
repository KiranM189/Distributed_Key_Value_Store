#include "KVClient.hpp"
#include <thallium/serialization/stl/string.hpp>
#include <chrono>

KVClient::KVClient(const std::string& protocol, uint16_t provider_id)
        : myEngine(protocol, THALLIUM_CLIENT_MODE),provider_id(provider_id) {}

std::string KVClient::fetch(int key, std::string& server_endpoint) {

    auto start = std::chrono::high_resolution_clock::now();

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
