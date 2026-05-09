#include <chrono>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
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

        if (it == store.end()) {
            return false;
        }

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

int main() {
    KeyValueStore kvStore;
    std::string input;

    std::cout << "Redis-lite CLI started. Type EXIT to quit.\n";

    while (true) {
        std::cout << "> ";
        std::getline(std::cin, input);

        std::stringstream ss(input);
        std::string command;
        ss >> command;

        if (command == "SET") {
            std::string key, value;
            ss >> key >> value;

            if (key.empty() || value.empty()) {
                std::cout << "Usage: SET key value OR SET key value EX seconds\n";
                continue;
            }

            std::string option;
            ss >> option;

            if (option == "EX") {
                int seconds;
                ss >> seconds;

                if (seconds <= 0) {
                    std::cout << "TTL must be greater than 0\n";
                    continue;
                }

                kvStore.setWithTTL(key, value, seconds);
            } else {
                kvStore.set(key, value);
            }

            std::cout << "OK\n";
        }
        else if (command == "GET") {
            std::string key;
            ss >> key;

            std::string value;
            if (kvStore.get(key, value)) {
                std::cout << value << "\n";
            } else {
                std::cout << "(nil)\n";
            }
        }
        else if (command == "DELETE") {
            std::string key;
            ss >> key;

            if (kvStore.remove(key)) {
                std::cout << "Deleted\n";
            } else {
                std::cout << "Key not found\n";
            }
        }
        else if (command == "EXIT") {
            std::cout << "Bye!\n";
            break;
        }
        else {
            std::cout << "Unknown command\n";
        }
    }

    return 0;
}