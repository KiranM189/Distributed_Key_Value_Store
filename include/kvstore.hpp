#ifndef KVSTORE_H
#define KVSTORE_H
#include <boost/unordered_map.hpp>
#include <string.h>
#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/container/string.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <iostream>
#include <iomanip> // For pretty formatting of memory stats

using namespace boost::interprocess;

typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef boost::container::basic_string<char, std::char_traits<char>, CharAllocator> MyShmString;
typedef allocator<MyShmString, managed_shared_memory::segment_manager> StringAllocator;
typedef std::pair<const int, MyShmString> newtype;
typedef allocator<newtype, managed_shared_memory::segment_manager> newallocator;
typedef boost::unordered_map<int, MyShmString, boost::hash<int>, std::equal_to<int>, newallocator> newmap;

extern const char* MUTEX_NAME;

// Helper struct to track memory stats
struct MemoryStats {
    std::size_t total_size;
    std::size_t free_memory;
    std::size_t used_memory;
    double usage_percent;
};

class KvStore {
public:
    static KvStore& get_instance(std::size_t size);
    void Insert(int key, const std::string& value);
    void Update(int key, const std::string& new_value);
    void Delete(int key);
    std::string Find(int key);

    // Memory management functions
    MemoryStats GetMemoryStats() const;
    void PrintMemoryStats(const std::string& operation) const;
    std::size_t GetFreeMemory() const;
    std::size_t GetUsedMemory() const;

private:
    KvStore(std::size_t size);
    managed_shared_memory* shared_mem = nullptr;
    newmap* map_ptr = nullptr;
    std::size_t total_memory_size;

    void create();
    bool hasEnoughMemory(std::size_t needed_bytes) const;

    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;
    ~KvStore();
};
#endif
