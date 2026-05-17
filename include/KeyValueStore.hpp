#pragma once

#include <chrono>
#include <list>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace redis_lite {

class KeyValueStore {
public:
    explicit KeyValueStore(std::size_t maxCapacity);

    void set(const std::string& key, const std::string& value);
    void setWithTTL(const std::string& key, const std::string& value, int seconds);
    bool get(const std::string& key, std::string& value);
    bool remove(const std::string& key);

private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expiryTime;
        std::list<std::string>::iterator lruIterator;
    };

    bool isExpired(const Entry& entry) const;
    void touch(const std::string& key);
    void evictIfNeeded();

    std::size_t capacity_;
    std::unordered_map<std::string, Entry> store_;
    std::list<std::string> lruList_;
    std::mutex mutex_;
};

} // namespace redis_lite
