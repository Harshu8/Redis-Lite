#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <list>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>

class KeyValueStore {
private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expiryTime;
        std::list<std::string>::iterator lruIterator;
    };

    size_t capacity;
    std::unordered_map<std::string, Entry> store;
    std::list<std::string> lruList;
    std::mutex mutex;

    bool isExpired(const Entry& entry) const {
        return entry.expiryTime.has_value() &&
               std::chrono::steady_clock::now() >= entry.expiryTime.value();
    }

    void touch(const std::string& key) {
        auto it = store.find(key);
        if (it == store.end()) return;

        lruList.erase(it->second.lruIterator);
        lruList.push_front(key);
        it->second.lruIterator = lruList.begin();
    }

    void evictIfNeeded() {
        while (store.size() > capacity) {
            std::string keyToRemove = lruList.back();
            lruList.pop_back();
            store.erase(keyToRemove);
        }
    }

public:
    explicit KeyValueStore(size_t maxCapacity = 3)
        : capacity(maxCapacity) {}

    void set(const std::string& key, const std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = store.find(key);

        if (it != store.end()) {
            it->second.value = value;
            it->second.expiryTime = std::nullopt;
            touch(key);
            return;
        }

        lruList.push_front(key);
        store[key] = {value, std::nullopt, lruList.begin()};
        evictIfNeeded();
    }

    void setWithTTL(const std::string& key, const std::string& value, int seconds) {
        std::lock_guard<std::mutex> lock(mutex);

        auto expiry =
            std::chrono::steady_clock::now() + std::chrono::seconds(seconds);

        auto it = store.find(key);

        if (it != store.end()) {
            it->second.value = value;
            it->second.expiryTime = expiry;
            touch(key);
            return;
        }

        lruList.push_front(key);
        store[key] = {value, expiry, lruList.begin()};
        evictIfNeeded();
    }

    bool get(const std::string& key, std::string& value) {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = store.find(key);

        if (it == store.end()) {
            return false;
        }

        if (isExpired(it->second)) {
            lruList.erase(it->second.lruIterator);
            store.erase(it);
            return false;
        }

        value = it->second.value;
        touch(key);
        return true;
    }

    bool remove(const std::string& key) {
        std::lock_guard<std::mutex> lock(mutex);

        auto it = store.find(key);

        if (it == store.end()) {
            return false;
        }

        lruList.erase(it->second.lruIterator);
        store.erase(it);
        return true;
    }
};

std::string processCommand(KeyValueStore& kvStore, const std::string& input) {
    std::stringstream ss(input);
    std::string command;
    ss >> command;

    if (command == "SET") {
        std::string key, value;
        ss >> key >> value;

        if (key.empty() || value.empty()) {
            return "Usage: SET key value OR SET key value EX seconds\n";
        }

        std::string option;
        ss >> option;

        if (option == "EX") {
            int seconds;
            ss >> seconds;

            if (seconds <= 0) {
                return "TTL must be greater than 0\n";
            }

            kvStore.setWithTTL(key, value, seconds);
        } else {
            kvStore.set(key, value);
        }

        return "OK\n";
    }

    if (command == "GET") {
        std::string key;
        ss >> key;

        std::string value;
        if (kvStore.get(key, value)) {
            return value + "\n";
        }

        return "(nil)\n";
    }

    if (command == "DELETE") {
        std::string key;
        ss >> key;

        if (kvStore.remove(key)) {
            return "Deleted\n";
        }

        return "Key not found\n";
    }

    if (command == "EXIT") {
        return "Bye!\n";
    }

    return "Unknown command\n";
}

void handleClient(int clientFd, KeyValueStore& kvStore) {
    std::cout << "Client connected. Thread ID: "
              << std::this_thread::get_id() << "\n";

    char buffer[1024];

    while (true) {
        std::memset(buffer, 0, sizeof(buffer));

        int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            std::cout << "Client disconnected. Thread ID: "
                      << std::this_thread::get_id() << "\n";
            break;
        }

        std::string input(buffer);
        std::string response = processCommand(kvStore, input);

        send(clientFd, response.c_str(), response.size(), 0);

        if (input.find("EXIT") == 0) {
            break;
        }
    }

    close(clientFd);
}

int main() {
    KeyValueStore kvStore(3);

    int serverFd = socket(AF_INET, SOCK_STREAM, 0);

    if (serverFd == -1) {
        std::cerr << "Failed to create socket\n";
        return 1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(6379);

    if (bind(serverFd,
             reinterpret_cast<sockaddr*>(&serverAddress),
             sizeof(serverAddress)) < 0) {
        std::cerr << "Bind failed\n";
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "Listen failed\n";
        close(serverFd);
        return 1;
    }

    std::cout << "Redis-lite multithreaded server running on port 6379...\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        int clientFd =
            accept(serverFd,
                   reinterpret_cast<sockaddr*>(&clientAddress),
                   &clientLen);

        if (clientFd < 0) {
            std::cerr << "Failed to accept client\n";
            continue;
        }

        std::thread clientThread(handleClient, clientFd, std::ref(kvStore));
        clientThread.detach();
    }

    close(serverFd);
    return 0;
}