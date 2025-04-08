// kvstore.h
#ifndef KVSTORE_H
#define KVSTORE_H

#include <boost/interprocess/managed_shared_memory.hpp>
#include <boost/interprocess/containers/map.hpp>
#include <boost/interprocess/allocators/allocator.hpp>
#include <boost/interprocess/sync/named_mutex.hpp>
#include <boost/container/string.hpp>
#include <boost/interprocess/sync/scoped_lock.hpp>
#include <iostream>

using namespace boost::interprocess;

typedef allocator<char, managed_shared_memory::segment_manager> CharAllocator;
typedef boost::container::basic_string<char, std::char_traits<char>, CharAllocator> MyShmString;
typedef allocator<MyShmString, managed_shared_memory::segment_manager> StringAllocator;
typedef std::pair<const int, MyShmString> newtype;
typedef allocator<newtype, managed_shared_memory::segment_manager> newallocator;
typedef boost::interprocess::map<int, MyShmString, std::less<int>, newallocator> newmap;

extern const char* MUTEX_NAME;


class KvStore {
public:
    static KvStore& get_instance();
    void Insert(int key, const std::string& value);
    void Update(int key, const std::string& new_value);
    void Delete(int key);
    void Find(int key);

private:
    KvStore();
    managed_shared_memory get_shared_memory();
    newmap* get_shared_map(managed_shared_memory& shared);
    void create();
    KvStore(const KvStore&) = delete;
    KvStore& operator=(const KvStore&) = delete;
};

#endif