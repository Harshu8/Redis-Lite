#pragma once

#include <string>

namespace redis_lite {

enum class CommandType {
    Set,
    Get,
    Delete,
    Exit,
    Unknown
};

struct CommandResult {
    std::string response;
    bool shouldCloseConnection{false};
};

} // namespace redis_lite
