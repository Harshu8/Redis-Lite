#include <arpa/inet.h>
#include <chrono>
#include <condition_variable>
#include <cstring>
#include <fstream>
#include <iostream>
#include <list>
#include <mutex>
#include <netinet/in.h>
#include <optional>
#include <queue>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>
#include <unordered_map>
#include <vector>

std::string trim(const std::string& input) {
    size_t start = input.find_first_not_of(" \n\r\t");
    if (start == std::string::npos) return "";

    size_t end = input.find_last_not_of(" \n\r\t");
    return input.substr(start, end - start + 1);
}

class PersistenceManager {
private:
    std::string filename;
    std::mutex fileMutex;

public:
    explicit PersistenceManager(const std::string& file)
        : filename(file) {}

    void appendCommand(const std::string& command) {
        std::lock_guard<std::mutex> lock(fileMutex);

        std::ofstream out(filename, std::ios::app);
        if (!out.is_open()) {
            std::cerr << "Failed to open AOF file\n";
            return;
        }

        out << command << "\n";
    }

    std::vector<std::string> loadCommands() {
        std::vector<std::string> commands;

        std::ifstream in(filename);
        if (!in.is_open()) {
            return commands;
        }

        std::string line;
        while (std::getline(in, line)) {
            line = trim(line);
            if (!line.empty()) {
                commands.push_back(line);
            }
        }

        return commands;
    }
};

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
    explicit KeyValueStore(size_t maxCapacity = 100)
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

std::string processCommand(
    KeyValueStore& kvStore,
    PersistenceManager& persistence,
    const std::string& rawInput,
    bool shouldPersist = true
) {
    std::string input = trim(rawInput);

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

        if (shouldPersist) {
            persistence.appendCommand(input);
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

        if (key.empty()) {
            return "Usage: DELETE key\n";
        }

        bool deleted = kvStore.remove(key);

        if (shouldPersist && deleted) {
            persistence.appendCommand(input);
        }

        return deleted ? "Deleted\n" : "Key not found\n";
    }

    if (command == "EXIT") {
        return "Bye!\n";
    }

    return "Unknown command\n";
}

void handleClient(
    int clientFd,
    KeyValueStore& kvStore,
    PersistenceManager& persistence
) {
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
        std::string response =
            processCommand(kvStore, persistence, input, true);

        send(clientFd, response.c_str(), response.size(), 0);

        if (trim(input) == "EXIT") {
            break;
        }
    }

    close(clientFd);
}

class ThreadPool {
private:
    std::vector<std::thread> workers;
    std::queue<int> clientQueue;
    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;

    KeyValueStore& kvStore;
    PersistenceManager& persistence;

public:
    ThreadPool(
        size_t threadCount,
        KeyValueStore& store,
        PersistenceManager& persistenceManager
    )
        : stop(false),
          kvStore(store),
          persistence(persistenceManager) {
        for (size_t i = 0; i < threadCount; ++i) {
            workers.emplace_back([this]() {
                while (true) {
                    int clientFd;

                    {
                        std::unique_lock<std::mutex> lock(queueMutex);

                        condition.wait(lock, [this]() {
                            return stop || !clientQueue.empty();
                        });

                        if (stop && clientQueue.empty()) {
                            return;
                        }

                        clientFd = clientQueue.front();
                        clientQueue.pop();
                    }

                    handleClient(clientFd, kvStore, persistence);
                }
            });
        }
    }

    void addClient(int clientFd) {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            clientQueue.push(clientFd);
        }

        condition.notify_one();
    }

    ~ThreadPool() {
        {
            std::lock_guard<std::mutex> lock(queueMutex);
            stop = true;
        }

        condition.notify_all();

        for (std::thread& worker : workers) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }
};

int main() {
    KeyValueStore kvStore(100);
    PersistenceManager persistence("appendonly.aof");

    std::vector<std::string> oldCommands = persistence.loadCommands();

    for (const auto& command : oldCommands) {
        processCommand(kvStore, persistence, command, false);
    }

    std::cout << "Recovered "
              << oldCommands.size()
              << " commands from appendonly.aof\n";

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

    if (bind(
            serverFd,
            reinterpret_cast<sockaddr*>(&serverAddress),
            sizeof(serverAddress)
        ) < 0) {
        std::cerr << "Bind failed\n";
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "Listen failed\n";
        close(serverFd);
        return 1;
    }

    ThreadPool threadPool(4, kvStore, persistence);

    std::cout << "Redis-lite server running on port 6379...\n";
    std::cout << "AOF persistence enabled: appendonly.aof\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        int clientFd =
            accept(
                serverFd,
                reinterpret_cast<sockaddr*>(&clientAddress),
                &clientLen
            );

        if (clientFd < 0) {
            std::cerr << "Failed to accept client\n";
            continue;
        }

        threadPool.addClient(clientFd);
    }

    close(serverFd);
    return 0;
}