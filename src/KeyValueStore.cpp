#include "KeyValueStore.hpp"

namespace redis_lite {

KeyValueStore::KeyValueStore(std::size_t maxCapacity)
    : capacity_(maxCapacity) {}

bool KeyValueStore::isExpired(const Entry& entry) const {
    return entry.expiryTime.has_value() &&
           std::chrono::steady_clock::now() >= entry.expiryTime.value();
}

void KeyValueStore::touch(const std::string& key) {
    auto it = store_.find(key);
    if (it == store_.end()) {
        return;
    }

    lruList_.erase(it->second.lruIterator);
    lruList_.push_front(key);
    it->second.lruIterator = lruList_.begin();
}

void KeyValueStore::evictIfNeeded() {
    while (store_.size() > capacity_) {
        const std::string keyToRemove = lruList_.back();
        lruList_.pop_back();
        store_.erase(keyToRemove);
    }
}

void KeyValueStore::set(const std::string& key, const std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);
    if (it != store_.end()) {
        it->second.value = value;
        it->second.expiryTime = std::nullopt;
        touch(key);
        return;
    }

    lruList_.push_front(key);
    store_[key] = Entry{value, std::nullopt, lruList_.begin()};
    evictIfNeeded();
}

void KeyValueStore::setWithTTL(const std::string& key, const std::string& value, int seconds) {
    std::lock_guard<std::mutex> lock(mutex_);

    const auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

    auto it = store_.find(key);
    if (it != store_.end()) {
        it->second.value = value;
        it->second.expiryTime = expiry;
        touch(key);
        return;
    }

    lruList_.push_front(key);
    store_[key] = Entry{value, expiry, lruList_.begin()};
    evictIfNeeded();
}

bool KeyValueStore::get(const std::string& key, std::string& value) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }

    if (isExpired(it->second)) {
        lruList_.erase(it->second.lruIterator);
        store_.erase(it);
        return false;
    }

    value = it->second.value;
    touch(key);
    return true;
}

bool KeyValueStore::remove(const std::string& key) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto it = store_.find(key);
    if (it == store_.end()) {
        return false;
    }

    lruList_.erase(it->second.lruIterator);
    store_.erase(it);
    return true;
}

} // namespace redis_lite
