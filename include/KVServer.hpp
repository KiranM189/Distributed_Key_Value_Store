#ifndef KVSERVER_HPP
#define KVSERVER_HPP

#include <iostream>
#include <thallium.hpp>
#include <thallium/serialization/stl/string.hpp>
#include <unordered_map>

namespace tl = thallium;

class KVServer {
private:
    tl::engine myEngine;
    std::unordered_map<int, double>& mp;
    tl::mutex myMutex;

public:
    KVServer(const std::string& protocol, std::unordered_map<int, double>& map_ref);
    std::string get_addr();
    void run();
};

#endif // KVSERVER_HPP