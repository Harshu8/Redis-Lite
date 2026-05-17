#pragma once

#include "Common.hpp"
#include "KeyValueStore.hpp"
#include "PersistenceManager.hpp"

#include <string>

namespace redis_lite {

class CommandProcessor {
public:
    CommandProcessor(KeyValueStore& kvStore, PersistenceManager& persistence);

    CommandResult process(const std::string& rawInput, bool shouldPersist = true);

private:
    KeyValueStore& kvStore_;
    PersistenceManager& persistence_;
};

} // namespace redis_lite
