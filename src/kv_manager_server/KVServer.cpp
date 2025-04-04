#include "KVServer.hpp"

KVServer::KVServer(const std::string& protocol, std::unordered_map<int, double>& map_ref)
    : myEngine(protocol, THALLIUM_SERVER_MODE), mp(map_ref) {

    myEngine.define("kv_fetch",
        [this](const tl::request& req, tl::bulk& b, int key) {
            std::cout << "Client trying RDMA: " << key << std::endl;
            tl::endpoint ep = req.get_endpoint();
            if (mp.find(key) != mp.end()) {
                std::pair<int, double> p = {key, mp[key]};
                std::vector<std::pair<void*, std::size_t>> segments(1);
                segments[0].first = (void*)(&p);
                segments[0].second = sizeof(p);

                tl::bulk local = myEngine.expose(segments, tl::bulk_mode::read_only);
                b.on(ep) << local;
                std::cout << "Server sent data for key: " << key << " -> " << mp[key] << std::endl;
            } else {
                std::cout << "Key not found on server!" << std::endl;
            }
            req.respond();
        });

    myEngine.define("kv_insert",
        [this](const tl::request& req, int key, double value) {
            std::cout << "Client wants to insert: " << key << " -> " << value << std::endl;

            std::cout << "Before Insertion :" << std::endl;
            for(auto it = mp.begin(); it!=mp.end() ;it++) {
                std::cout << it->first << " - " << it->second << std::endl;
            }
            myMutex.lock();
            mp[key] = value;
            myMutex.unlock();

            std::cout << "After Insertion :" << std::endl;
            for(auto it = mp.begin(); it!=mp.end() ;it++) {
                std::cout << it->first << " - " << it->second << std::endl;
            }

            req.respond(1);
        });

    myEngine.define("kv_update",
        [this](const tl::request& req, int key, double value) {
            std::cout << "Client wants to update: " << key << " -> " << value << std::endl;

            if (mp.find(key) == mp.end()) {
                myMutex.unlock();
                std::cout << "Key does not exist\n";
                req.respond(0);
            }
            else {
                std::cout << "Before Updation :" << std::endl;
                for(auto it = mp.begin(); it!=mp.end() ;it++) {
                        std::cout << it->first << " - " << it->second << std::endl;
                }

                myMutex.lock();
                mp[key] = value;
                myMutex.unlock();

                std::cout << "After Updation :" << std::endl;
                for(auto it = mp.begin(); it!=mp.end() ;it++) {
                        std::cout << it->first << " - " << it->second << std::endl;
                }
                req.respond(1);
            }
        });
}

std::string KVServer::get_addr() {
    return myEngine.self();
}

void KVServer::run() {
    myEngine.wait_for_finalize();
}