#include "kvstore.hpp"
#include <string.h>
#include <sstream>
#include <iomanip>

const char* MUTEX_NAME = "SharedMapMutex";

// Memory size constants
const std::size_t MB = 1024 * 1024;
const std::size_t DEFAULT_MEMORY_SIZE = 100 * MB; // Default to 100MB

KvStore& KvStore::get_instance(std::size_t size) {
    std::cout << "[get_instance] Initializing KvStore with " << (size / MB) << "MB of shared memory\n";
    static KvStore instance(size > 0 ? size : DEFAULT_MEMORY_SIZE);
    return instance;
}

KvStore::KvStore(std::size_t size) : total_memory_size(size) {
    std::cout << "[KvStore] Constructor started with " << (size / MB) << "MB allocation\n";
    try {
        // Remove any existing shared memory with the same name
        shared_memory_object::remove("Project");

        // Create new shared memory
        shared_mem = new managed_shared_memory(create_only, "Project", size);
        std::cout << "[KvStore] Shared memory created successfully\n";

        // Construct the unordered map in shared memory
        newallocator allocate(shared_mem->get_segment_manager());
        map_ptr = shared_mem->construct<newmap>("SharedMap")(0, boost::hash<int>(), std::equal_to<int>(), allocate);
        std::cout << "[KvStore] Unordered map constructed successfully\n";

        // Handle mutex
        if (!named_mutex::remove(MUTEX_NAME))
            std::cout << "[KvStore] Warning: failed to remove existing mutex\n";

        named_mutex mutex(open_or_create, MUTEX_NAME);
        std::cout << "[KvStore] Mutex created successfully\n";

        // Print initial memory stats
        PrintMemoryStats("Initialization");

    } catch (interprocess_exception &e) {
        std::cout << "[KvStore] Caught interprocess exception: " << e.what() << std::endl;
        std::cout << "[KvStore] Attempting to open existing shared memory\n";

        try {
            shared_mem = new managed_shared_memory(open_only, "Project");
            map_ptr = shared_mem->find<newmap>("SharedMap").first;
            std::cout << "[KvStore] Opened existing shared memory\n";
            PrintMemoryStats("Opened Existing");
        } catch (interprocess_exception &e2) {
            std::cout << "[KvStore] Failed to open existing memory: " << e2.what() << std::endl;
            std::cout << "[KvStore] Creating new memory after removing old\n";

            shared_memory_object::remove("Project");
            named_mutex::remove(MUTEX_NAME);

            // Try again with fresh memory
            shared_mem = new managed_shared_memory(create_only, "Project", size);
            newallocator allocate(shared_mem->get_segment_manager());
            map_ptr = shared_mem->construct<newmap>("SharedMap")(0, boost::hash<int>(), std::equal_to<int>(), allocate);
            named_mutex mutex(open_or_create, MUTEX_NAME);

            std::cout << "[KvStore] Recovery successful\n";
            PrintMemoryStats("Recovery");
        }
    }
    std::cout << "[KvStore] Constructor finished\n";
}

// Memory management helper functions
MemoryStats KvStore::GetMemoryStats() const {
    if (!shared_mem) {
        return {0, 0, 0, 0.0};
    }

    MemoryStats stats;
    stats.total_size = total_memory_size;
    stats.free_memory = shared_mem->get_free_memory();
    stats.used_memory = stats.total_size - stats.free_memory;
    stats.usage_percent = (static_cast<double>(stats.used_memory) / stats.total_size) * 100.0;

    return stats;
}

void KvStore::PrintMemoryStats(const std::string& operation) const {
    MemoryStats stats = GetMemoryStats();

    std::cout << "\n========== MEMORY STATS [" << operation << "] ==========\n";
    std::cout << "  Total memory: " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.total_size) / MB) << " MB\n";
    std::cout << "  Used memory:  " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.used_memory) / MB) << " MB\n";
    std::cout << "  Free memory:  " << std::fixed << std::setprecision(2)
              << (static_cast<double>(stats.free_memory) / MB) << " MB\n";
    std::cout << "  Usage:        " << std::fixed << std::setprecision(2)
              << stats.usage_percent << "%\n";
    std::cout << "============================================\n";
}

std::size_t KvStore::GetFreeMemory() const {
    return shared_mem ? shared_mem->get_free_memory() : 0;
}

std::size_t KvStore::GetUsedMemory() const {
    return shared_mem ? (total_memory_size - shared_mem->get_free_memory()) : 0;
}

bool KvStore::hasEnoughMemory(std::size_t needed_bytes) const {
    // Add extra padding for overhead, index structures, etc
    const std::size_t OVERHEAD_FACTOR = 1.5;
    std::size_t estimated_need = static_cast<std::size_t>(needed_bytes * OVERHEAD_FACTOR);

    if (!shared_mem) return false;

    std::size_t free_memory = shared_mem->get_free_memory();
    return free_memory >= estimated_need;
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
                // Check if key already exists
                auto it = map_ptr->find(key);
                if (it != map_ptr->end()) {
                    std::cout << "Key " << key << " already exists. Use update instead.\n";
                    PrintMemoryStats("Insert - Key Exists");
                    return;
                }

                // Check memory before insertion
                std::size_t free_memory = shared_mem->get_free_memory();
                std::size_t entry_size = value.size() + sizeof(int) + 64; // Key + value + overhead

                std::cout << "Entry size estimate: " << entry_size << " bytes\n";
                std::cout << "Available shared memory before insertion: " << free_memory << " bytes\n";

                // Check if we have enough memory for the insertion
                if (!hasEnoughMemory(entry_size)) {
                    std::cout << "Error: Not enough shared memory for insertion. Need at least "
                              << entry_size << " bytes but only have " << free_memory << " available.\n";
                    PrintMemoryStats("Insert - Failed (Memory)");
                    return;
                }
                // Perform the insertion
                CharAllocator char_allocator(shared_mem->get_segment_manager());
                try {
                    map_ptr->insert(std::make_pair(key, MyShmString(value.c_str(), char_allocator)));
                    std::cout << "Inserted key " << key << " with value: " << value << std::endl;

                    // Print memory stats after insertion
                    PrintMemoryStats("After Insert");

                } catch (const boost::interprocess::bad_alloc& e) {
                    std::cout << "Error during insertion (bad_alloc): " << e.what() << std::endl;
                    PrintMemoryStats("Insert - Failed (Allocation)");
                } catch (const std::exception& e) {
                    std::cout << "Error during insertion: " << e.what() << std::endl;
                }

            } catch (const std::exception& e) {
                std::cout << "Error with shared memory operations: " << e.what() << std::endl;
            }
        } catch (const std::exception& e) {
            std::cout << "Error acquiring mutex: " << e.what() << std::endl;
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
            // Check if we have enough memory for the update
            // This is trickier since we need to account for the size difference
            std::size_t old_size = std::string(it->second.c_str()).size();
            std::size_t new_size = new_value.size();
            std::size_t size_diff = new_size > old_size ? (new_size - old_size) : 0;

            // Only need to check memory if new value is larger
            if (size_diff > 0 && !hasEnoughMemory(size_diff)) {
                std::cout << "Error: Not enough memory for update. Need " << size_diff
                          << " more bytes but only have " << shared_mem->get_free_memory() << " available.\n";
                PrintMemoryStats("Update - Failed (Memory)");
                return;
            }

            // Perform the update
            CharAllocator char_alloc(shared_mem->get_segment_manager());
            MyShmString shm_string(new_value.c_str(), char_alloc);
            it->second = shm_string;

            std::cout << "Key " << key << " updated to: " << new_value << std::endl;
            PrintMemoryStats("After Update");

        } else {
            std::cout << "Key " << key << " not found. Cannot update." << std::endl;
            PrintMemoryStats("Update - Key Not Found");
        }
    } catch (std::exception &e) {
        std::cout << "Error during update: " << e.what() << std::endl;
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
            std::cout << "Deleting key " << key << std::endl;

            // Get pre-delete memory stats
            std::size_t pre_free_memory = shared_mem->get_free_memory();

            // Erase the key
            map_ptr->erase(it);

            // Get post-delete memory stats
            std::size_t post_free_memory = shared_mem->get_free_memory();
            std::size_t memory_freed = post_free_memory - pre_free_memory;

            std::cout << "Key " << key << " deleted. Freed approximately "
                      << memory_freed << " bytes." << std::endl;

            PrintMemoryStats("After Delete");

        } else {
            std::cout << "Key " << key << " not found. Nothing to delete." << std::endl;
            PrintMemoryStats("Delete - Key Not Found");
        }
    } catch (std::exception &e) {
        std::cout << "Error during delete: " << e.what() << std::endl;
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
            std::cout << "Found key " << key << " with value: " << normal_str << std::endl;

            // Print memory stats for informational purposes
            PrintMemoryStats("After Find");

            return normal_str;
        } else {
            std::cout << "Key " << key << " not found in shared memory." << std::endl;
            PrintMemoryStats("Find - Key Not Found");
            return "key not found";
        }
    } catch (std::exception &e) {
        std::cout << "Error during find operation: " << e.what() << std::endl;
        return "Error: " + std::string(e.what());
    }
}

KvStore::~KvStore() {
    std::cout << "[KvStore] Destructor called, cleaning up resources" << std::endl;

    try {
        // Final memory stats
        PrintMemoryStats("Before Destruction");

        if (shared_mem) {
            try {
                // Destroy unordered map object in shared memory
                shared_mem->destroy<newmap>("SharedMap");
                std::cout << "Destroyed unordered map from shared memory." << std::endl;

                // Delete the shared_mem pointer
                delete shared_mem;
                shared_mem = nullptr;

            } catch (const std::exception& e) {
                std::cout << "Error during unordered map destruction: " << e.what() << std::endl;
            }
        }

        // Remove shared memory segment
        if (shared_memory_object::remove("Project")) {
            std::cout << "Shared memory segment removed." << std::endl;
        } else {
            std::cout << "Shared memory segment removal failed or already removed.\n";
        }

        // Remove named mutex
        if (named_mutex::remove(MUTEX_NAME)) {
            std::cout << "Named mutex removed." << std::endl;
        } else {
            std::cout << "Named mutex removal failed or already removed.\n";
        }

    } catch (const std::exception& e) {
        std::cout << "Error during KvStore cleanup: " << e.what() << std::endl;
    }
}
