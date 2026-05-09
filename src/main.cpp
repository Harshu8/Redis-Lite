#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>

class KeyValueStore {
private:
    std::unordered_map<std::string, std::string> store;

public:
    void set(const std::string& key, const std::string& value) {
        store[key] = value;
    }

    bool get(const std::string& key, std::string& value) const {
        auto it = store.find(key);
        if (it == store.end()) {
            return false;
        }

        value = it->second;
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
                std::cout << "Usage: SET key value\n";
                continue;
            }

            kvStore.set(key, value);
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
