#include <arpa/inet.h>
#include <chrono>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <optional>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <unistd.h>
#include <unordered_map>

class KeyValueStore {
private:
    struct Entry {
        std::string value;
        std::optional<std::chrono::steady_clock::time_point> expiryTime;
    };

    std::unordered_map<std::string, Entry> store;

    bool isExpired(const Entry& entry) const {
        return entry.expiryTime.has_value() &&
               std::chrono::steady_clock::now() >= entry.expiryTime.value();
    }

public:
    void set(const std::string& key, const std::string& value) {
        store[key] = {value, std::nullopt};
    }

    void setWithTTL(const std::string& key, const std::string& value, int seconds) {
        auto expiry = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
        store[key] = {value, expiry};
    }

    bool get(const std::string& key, std::string& value) {
        auto it = store.find(key);

        if (it == store.end()) return false;

        if (isExpired(it->second)) {
            store.erase(it);
            return false;
        }

        value = it->second.value;
        return true;
    }

    bool remove(const std::string& key) {
        return store.erase(key) > 0;
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

    return "Unknown command\n";
}

int main() {
    KeyValueStore kvStore;

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

    if (bind(serverFd, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        std::cerr << "Bind failed\n";
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, 10) < 0) {
        std::cerr << "Listen failed\n";
        close(serverFd);
        return 1;
    }

    std::cout << "Redis-lite server running on port 6379...\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        int clientFd = accept(serverFd, reinterpret_cast<sockaddr*>(&clientAddress), &clientLen);
        if (clientFd < 0) {
            std::cerr << "Failed to accept client\n";
            continue;
        }

        std::cout << "Client connected\n";

        char buffer[1024];

        while (true) {
            std::memset(buffer, 0, sizeof(buffer));

            int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);

            if (bytesRead <= 0) {
                std::cout << "Client disconnected\n";
                break;
            }

            std::string input(buffer);
            std::string response = processCommand(kvStore, input);

            send(clientFd, response.c_str(), response.size(), 0);
        }

        close(clientFd);
    }

    close(serverFd);
    return 0;
}
