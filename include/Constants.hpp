#pragma once

#include <cstddef>
#include <string>

namespace redis_lite::constants {

inline constexpr int DEFAULT_PORT = 6379;
inline constexpr int LISTEN_BACKLOG = 10;
inline constexpr int BUFFER_SIZE = 1024;
inline constexpr std::size_t DEFAULT_STORE_CAPACITY = 100;
inline constexpr std::size_t DEFAULT_THREAD_COUNT = 4;

inline const std::string AOF_FILENAME = "appendonly.aof";

} // namespace redis_lite::constants
