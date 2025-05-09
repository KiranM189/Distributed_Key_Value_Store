#include "KVServer.hpp"
#include "kvstore.hpp"


namespace tl = thallium;

int main(int argc, char** argv) {

        uint16_t provider_id = 1;
        tl::engine myEngine("tcp", THALLIUM_SERVER_MODE);
        std::cout << "Server running at " << myEngine.self() << '\n';
        KvStore &kv = KvStore::get_instance(1024);
        KVServer server(myEngine, kv, provider_id);
        myEngine.wait_for_finalize();
        return 0;
}