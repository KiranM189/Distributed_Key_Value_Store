#include "kvstore.hpp"
#include <string.h>

const char* MUTEX_NAME = "SharedMapMutex";


KvStore& KvStore::get_instance(int size) {
    static KvStore instance(size);
    return instance;
}

KvStore::KvStore(int size) {
    try {
        shared_mem = new managed_shared_memory(create_only, "Project", size);
        std::cout << "[KvStore] Shared memory created\n";

        newallocator allocate(shared_mem->get_segment_manager());
        map_ptr = shared_mem->construct<newmap>("SharedMap")(0, boost::hash<int>(), std::equal_to<int>(), allocate);
        std::cout << "[KvStore] Unordered map constructed\n";

        if (!named_mutex::remove(MUTEX_NAME))
            std::cout << "[KvStore] Warning: failed to remove existing mutex\n";

        named_mutex mutex(open_or_create, MUTEX_NAME);
        std::cout << "[KvStore] Mutex created\n";

    } catch (interprocess_exception &e) {
        std::cout << "[KvStore] Caught interprocess exception: " << e.what() << std::endl;
        shared_mem = new managed_shared_memory(open_only, "Project");
        map_ptr = shared_mem->find<newmap>("SharedMap").first;
        std::cout << "[KvStore] Opened existing shared memory\n";
    }

    std::cout << "[KvStore] Constructor finished\n";
}

void KvStore::Insert(int key, const std::string& value) {
    try {
        if (!map_ptr) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }

        try {
            named_mutex mutex(open_only, MUTEX_NAME);
            boost::interprocess::scoped_lock<named_mutex> lock(mutex);

            try {
                CharAllocator char_allocator(shared_mem->get_segment_manager());

                if (map_ptr->find(key) == map_ptr->end()) {
                    try {
                        map_ptr->insert(std::make_pair(key, MyShmString(value.c_str(), char_allocator)));
                        std::cout << "Inserted key " << key << " with value: " << value << std::endl;
                    } catch (const std::exception& e) {
                        std::cout << "Error during insertion: " << e.what() << std::endl;
                    }
                } else {
                    std::cout << "Key already exists. Use update instead.\n";
                }
            } catch (const std::exception& e) {
                std::cout << "Error with shared memory allocation or map operations: " << e.what() << std::endl;
            }

        } catch (const std::exception& e) {
            std::cout << "Error acquiring mutex or locking: " << e.what() << std::endl;
        }

    } catch (const std::exception& e) {
        std::cout << "Unexpected error in Insert function: " << e.what() << std::endl;
    }
}

void KvStore::Update(int key, const std::string& new_value) {
    try {
        if (!map_ptr) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }

        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);

        auto it = map_ptr->find(key);
        if (it != map_ptr->end()) {
            CharAllocator char_alloc(shared_mem->get_segment_manager());            
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
        if (!map_ptr) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }

        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);

        auto it = map_ptr->find(key);
        if (it != map_ptr->end()) {
            map_ptr->erase(it);
            std::cout << "Key " << key << " deleted." << std::endl;
        } else {
            std::cout << "Key not found." << std::endl;
        }

    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

std::string KvStore::Find(int key) {
    try {
        if (!map_ptr) {
            std::cout << "Map not found in shared memory." << std::endl;
            return "Map not found";
        }

        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);

        auto it = map_ptr->find(key);
        if (it != map_ptr->end()) {
            std::string normal_str = std::string(it->second.c_str());
            return normal_str;
        } else {
            return "key not found";
        }
        
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
        return "Error occurred";
    }
}

KvStore::~KvStore() {
    try {
        managed_shared_memory segment(open_only, "Project");
        segment.destroy<newmap>("SharedMap");
        std::cout << "Destroyed unordered map from shared memory." << std::endl;
    } catch (...) {
        std::cout << "Map destruction failed or already done.\n";
    }

    if (shared_memory_object::remove("Project")) {
        std::cout << "Shared memory segment removed." << std::endl;
    } else {
        std::cout << "Shared memory segment removal failed or already removed.\n";
    }

    if (named_mutex::remove(MUTEX_NAME)) {
        std::cout << "Named mutex removed." << std::endl;
    } else {
        std::cout << "Named mutex removal failed or already removed.\n";
    }
}