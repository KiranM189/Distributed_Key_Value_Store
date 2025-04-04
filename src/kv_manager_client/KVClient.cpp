#include "KVClient.hpp"

KVClient::KVClient(const std::string& protocol) : myEngine(protocol, THALLIUM_CLIENT_MODE) {}

void KVClient::fetch(int key, std::string& server_endpoint, std::unordered_map<int, double>& mp) {
    if (mp.find(key) != mp.end()) {
        std::cout << "Key " << key << " found locally: " << mp[key] << std::endl;
        return;
    }

    std::cout << "Key not found locally. Proceeding to server lookup." << std::endl;
    tl::remote_procedure remote_kv_fetch = myEngine.define("kv_fetch");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);

    std::pair<int, double> p;
    std::vector<std::pair<void*, std::size_t>> segments = {
        {&p, sizeof(p)}
    };
    tl::bulk local_bulk = myEngine.expose(segments, tl::bulk_mode::write_only);

    remote_kv_fetch.on(server_ep)(local_bulk, key);

    std::cout << "Received from server: " << p.first << " -> " << p.second << std::endl;
}

void KVClient::insert(int key, double value, const std::string& server_endpoint) {
    tl::remote_procedure remote_kv_insert = myEngine.define("kv_insert");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);

    try {
        remote_kv_insert.on(server_ep)(key, value);
        std::cout << "Inserted successfully: " << key << " -> " << value << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Insert operation failed: " << e.what() << std::endl;
    }
}

void KVClient::update(int key, double value, const std::string& server_endpoint) {
    tl::remote_procedure remote_kv_update = myEngine.define("kv_update");
    tl::endpoint server_ep = myEngine.lookup(server_endpoint);

    try {
        remote_kv_update.on(server_ep)(key, value);
        std::cout << "Updated successfully: " << key << " -> " << value << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Update operation failed: " << e.what() << std::endl;
    }
}