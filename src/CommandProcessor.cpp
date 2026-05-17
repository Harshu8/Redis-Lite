#include "CommandProcessor.hpp"

#include "Utilities.hpp"

#include <sstream>

namespace redis_lite {

CommandProcessor::CommandProcessor(KeyValueStore& kvStore, PersistenceManager& persistence)
    : kvStore_(kvStore), persistence_(persistence) {}

CommandResult CommandProcessor::process(const std::string& rawInput, bool shouldPersist) {
    const std::string input = utils::trim(rawInput);

    std::stringstream ss(input);
    std::string command;
    ss >> command;
    command = utils::toUpper(command);

    if (command == "SET") {
        std::string key;
        std::string value;
        ss >> key >> value;

        if (key.empty() || value.empty()) {
            return {"Usage: SET key value OR SET key value EX seconds\n", false};
        }

        std::string option;
        ss >> option;
        option = utils::toUpper(option);

        if (option == "EX") {
            int seconds = 0;
            ss >> seconds;

            if (seconds <= 0) {
                return {"TTL must be greater than 0\n", false};
            }

            kvStore_.setWithTTL(key, value, seconds);
        } else {
            kvStore_.set(key, value);
        }

        if (shouldPersist) {
            persistence_.appendCommand(input);
        }

        return {"OK\n", false};
    }

    if (command == "GET") {
        std::string key;
        ss >> key;

        std::string value;
        if (kvStore_.get(key, value)) {
            return {value + "\n", false};
        }

        return {"(nil)\n", false};
    }

    if (command == "DELETE") {
        std::string key;
        ss >> key;

        if (key.empty()) {
            return {"Usage: DELETE key\n", false};
        }

        const bool deleted = kvStore_.remove(key);

        if (shouldPersist && deleted) {
            persistence_.appendCommand(input);
        }

        return {deleted ? "Deleted\n" : "Key not found\n", false};
    }

    if (command == "EXIT") {
        return {"Bye!\n", true};
    }

    return {"Unknown command\n", false};
}

} // namespace redis_lite
