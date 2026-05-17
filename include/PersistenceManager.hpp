#pragma once

#include <mutex>
#include <string>
#include <vector>

namespace redis_lite {

class PersistenceManager {
public:
    explicit PersistenceManager(std::string filename);

    void appendCommand(const std::string& command);
    std::vector<std::string> loadCommands();

private:
    std::string filename_;
    std::mutex fileMutex_;
};

} // namespace redis_lite
