#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/container/string.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <iostream>

// test with multiple threads - if the lock works

using namespace boost::interprocess;

// typedefs for boost String creation in shared memory
typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef boost::container::basic_string<char, std::char_traits<char>, CharAllocator> MyShmString;
typedef allocator<MyShmString, managed_shared_memory::segment_manager> StringAllocator;

// typedefs for boost map creation in shared memory
typedef std::pair<const int, MyShmString> newtype;
typedef allocator<newtype, managed_shared_memory::segment_manager> newallocator;
typedef boost::interprocess::map<int, MyShmString, std::less<int>, newallocator> newmap;

// Mutex name for synchronization
const char* MUTEX_NAME = "SharedMapMutex";

class KvStore {
public:

    static KvStore& get_instance() {
        static KvStore instance;
        return instance;
    }

    void performInsert(int key, const std::string& value) {
        Insert(key, value);
    }
    
    void performUpdate(int key, const std::string& new_value) {
        UpdateI(key, new_value);
    }
    
    void performDelete(int key) {
        Delete(key);
    }

    void performFind(int key) {
        Find(key);
    }

private:
    KvStore() {
        create();  // I want the shared memory to be created only once.
    }

    // methods for reading the shared memory and map inside the shared memory
    managed_shared_memory get_shared_memory() {
        return managed_shared_memory(open_only, "Project");
    }
    
    newmap* get_shared_map(managed_shared_memory& shared) {
        return shared.find<newmap>("SharedMap").first;
    }

    // does not allow copying instance 
    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;

    void create() {
        try {
            managed_shared_memory segment(create_only, "Project", 10000); //  size: 10KB
            newallocator allocate(segment.get_segment_manager());
            segment.construct<newmap>("SharedMap")(std::less<int>(), allocate);

            // Creating a named mutex to protect the shared map
            named_mutex::remove(MUTEX_NAME);
            named_mutex mutex(create_only, MUTEX_NAME);
            std::cout << "Shared memory and mutex created successfully." << std::endl;
        } 
        catch (interprocess_exception &e) {
            std::cout << "Shared memory already exists. Skipping creation..." << std::endl;
        }
    }

    void Insert(int key, const std::string& value) {
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
        }
        catch (std::exception &e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    void UpdateI(int key, const std::string& new_value) {
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
            } 
            else {
                std::cout << "Key not found." << std::endl;
            }
        }
        catch (std::exception &e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    void Delete(int key) {
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
            } 
            else {
                std::cout << "Key not found." << std::endl;
            }
        } 
        catch (std::exception &e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }

    void Find(int key) {
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
            } 
            else {
                std::cout << "Key not found." << std::endl;
            }
        } 
        catch (std::exception &e) {
            std::cout << "Error: " << e.what() << std::endl;
        }
    }
};

int main() {
    KvStore& kv = KvStore::get_instance();

    int choice;
    while (true) {
        std::cout << "\n1. Find inside Shared Mem\n";
        std::cout << "2. Delete inside Shared Mem\n";
        std::cout << "3. Update inside Shared Mem\n";
        std::cout << "4. Insert inside Shared Mem\n";
        std::cout << "5. Exit\n";
        std::cout << "Enter your choice: ";
        std::cin >> choice;

        int key;
        std::string value;

        switch (choice) {
            case 1: {
                std::cout << "Enter the key to find its value: ";
                std::cin >> key;
                kv.performFind(key);
                break;
            }
            case 2: {
                std::cout << "Enter the key to delete: ";
                std::cin >> key;
                kv.performDelete(key);
                break;
            }
            case 3: {
                std::cout << "Enter the key you want to update: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the new value: ";
                std::getline(std::cin, value);
                kv.performUpdate(key, value);  // Updated to pass value directly
                break;
            }
            case 4: {
                std::cout << "Enter the key you want to insert: ";
                std::cin >> key;
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Enter the value: ";
                std::getline(std::cin, value);
                kv.performInsert(key, value);  // Updated to pass value directly
                break;
            }
            case 5: {
                std::cout << "Exiting program.\n";
                return 0;
            }
            default:
                std::cout << "Invalid Choice. Try again.\n";
        }
    }
}

