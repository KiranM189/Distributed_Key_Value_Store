#include "kvstore.hpp"
#include <string.h>

// input validation 

const char* MUTEX_NAME = "SharedMapMutex";

KvStore& KvStore::get_instance(int size) {
    static KvStore instance(size);
    return instance;
}

KvStore::KvStore(int size) {
    try {
        
        shared_mem = new managed_shared_memory(create_only, "Project", size);
        newallocator allocate(shared_mem->get_segment_manager());
        map_ptr = shared_mem->construct<newmap>("SharedMap")(std::less<int>(), allocate);

        named_mutex::remove(MUTEX_NAME);
        named_mutex mutex(create_only, MUTEX_NAME);

        std::cout << "Shared memory and mutex created successfully." << std::endl;
    } catch (interprocess_exception &e) {
        // If already exists, open it
        shared_mem = new managed_shared_memory(open_only, "Project");
        map_ptr = shared_mem->find<newmap>("SharedMap").first;
        std::cout << "Shared memory already exists. Opened instead.\n";
    }
}



void KvStore::Insert(int key, const std::string& value) {
    try {
        if (!map_ptr) {
            std::cout << "Map not found in shared memory." << std::endl;
            return;
        }

        named_mutex mutex(open_only, MUTEX_NAME);
        boost::interprocess::scoped_lock<named_mutex> lock(mutex);

        CharAllocator char_allocator(shared_mem->get_segment_manager());

        if (map_ptr->find(key) == map_ptr->end()) {
            map_ptr->insert(std::make_pair(key, MyShmString(value.c_str(), char_allocator)));
            std::cout << "Inserted key " << key << " with value: " << value << std::endl;
        } else {
            std::cout << "Key already exists. Use update instead.\n";
        }

    } catch (const std::exception& e) {
        std::cout << "Error: " << e.what() << std::endl;
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
            std::string normal_str = static_cast<std::string>(it->second);
            return normal_str;


            std::cout << "Found key " << key << " with value: " << it->second << std::endl;
        } else {
            return "key not found";
        }
        
    } catch (std::exception &e) {
        std::cout << "Error: " << e.what() << std::endl;
    }
}

KvStore::~KvStore() {
    try {

        managed_shared_memory segment(open_only, "Project");

        segment.destroy<newmap>("SharedMap");

        std::cout << "Destroyed map from shared memory." << std::endl;
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
