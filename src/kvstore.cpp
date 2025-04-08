#include "kvstore.hpp"

const char* MUTEX_NAME = "SharedMapMutex";

KvStore& KvStore::get_instance() {
    static KvStore instance;
    return instance;
}

KvStore::KvStore() {
    create();
}

managed_shared_memory KvStore::get_shared_memory() {
    return managed_shared_memory(open_only, "Project");
}

newmap* KvStore::get_shared_map(managed_shared_memory& shared) {
    return shared.find<newmap>("SharedMap").first;
}

void KvStore::create() {
    try {
        managed_shared_memory segment(create_only, "Project", 10000);
        newallocator allocate(segment.get_segment_manager());
        segment.construct<newmap>("SharedMap")(std::less<int>(), allocate);
        named_mutex::remove(MUTEX_NAME);
        named_mutex mutex(create_only, MUTEX_NAME);
        std::cout << "Shared memory and mutex created successfully." << std::endl;
    } catch (interprocess_exception &e) {
        std::cout << "Shared memory already exists. Skipping creation..." << std::endl;
    }
}

void KvStore::Insert(int key, const std::string& value) {
    try {
        managed_shared_memory shared = get_shared_memory();
        newmap* map = get_shared_map(shared);
        if (!map) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }
        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);
        CharAllocator char_allocator(shared.get_segment_manager());
        if (map->find(key) == map->end()) {
            map->insert(std::pair<int, MyShmString>(key, MyShmString(value.c_str(), char_allocator)));
            std::cout << "Inserted key " << key << " with value: " << value << std::endl;
        } else {
            std::cout << "Key already exists. Use update instead.\n";
        }
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void KvStore::Update(int key, const std::string& new_value) {
    try {
        managed_shared_memory shared = get_shared_memory();
        newmap* map = get_shared_map(shared);
        if (!map) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }
        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);
        auto it = map->find(key);
        if (it != map->end()) {
            CharAllocator char_alloc(shared.get_segment_manager());
            MyShmString shm_string(new_value.c_str(), char_alloc);
            it->second = shm_string;
            std::cout << "Key " << key << " updated to: " << new_value << std::endl;
        } else {
            std::cout << "Key not found." << std::endl;
        }
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void KvStore::Delete(int key) {
    try {
        managed_shared_memory shared = get_shared_memory();
        newmap* map = get_shared_map(shared);
        if (!map) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }
        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);
        auto it = map->find(key);
        if (it != map->end()) {
            map->erase(it);
            std::cout << "Key " << key << " deleted." << std::endl;
        } else {
            std::cout << "Key not found." << std::endl;
        }
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

void KvStore::Find(int key) {
    try {
        managed_shared_memory shared = get_shared_memory();
        newmap* map = get_shared_map(shared);
        if (!map) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }
        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);
        auto it = map->find(key);
        if (it != map->end()) {
            std::cout << "Found key " << key << " with value: " << it->second << std::endl;
        } else {
            std::cout << "Key not found." << std::endl;
        }
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}