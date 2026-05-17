#include "CommandProcessor.hpp"
#include "Constants.hpp"
#include "KeyValueStore.hpp"
#include "PersistenceManager.hpp"
#include "Server.hpp"

#include <iostream>

int main() {
    using namespace redis_lite;

    try {
        KeyValueStore kvStore(constants::DEFAULT_STORE_CAPACITY);
        PersistenceManager persistence(constants::AOF_FILENAME);
        CommandProcessor commandProcessor(kvStore, persistence);

        const auto oldCommands = persistence.loadCommands();
        for (const auto& command : oldCommands) {
            commandProcessor.process(command, false);
        }

        std::cout << "Recovered " << oldCommands.size()
                  << " commands from " << constants::AOF_FILENAME << '\n';
        std::cout << "AOF persistence enabled: " << constants::AOF_FILENAME << '\n';

        Server server(constants::DEFAULT_PORT, commandProcessor);
        server.start();
    } catch (const std::exception& ex) {
        std::cerr << ex.what() << '\n';
        return 1;
    }

    return 0;
}
