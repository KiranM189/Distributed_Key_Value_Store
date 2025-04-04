// Entry point for kv_server_manager
#include "KVServer.hpp"

int main(int argc, char** argv) {
    std::unordered_map<int, double> mp;
    mp[3] = 3.33;
    mp[5] = 5.55;
    mp[6] = 6.66;
    mp[7] = 7.77;

    KVServer server("tcp", mp);
    std::cout << "Server running at " << server.get_addr() << std::endl;
    server.run();

    return 0;
}