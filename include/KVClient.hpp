#ifndef KVCLIENT_HPP
#define KVCLIENT_HPP

#include <iostream>
#include <thallium.hpp>
#include <unordered_map>

namespace tl = thallium;

class KVClient {
private:
    tl::engine myEngine;

public:
    KVClient(const std::string& protocol);
    void fetch(int key, std::string& server_endpoint, std::unordered_map<int, double>& mp);
    void insert(int key, double value, const std::string& server_endpoint);
    void update(int key, double value, const std::string& server_endpoint);
};

#endif // KVCLIENT_HPP