cmake_minimum_required(VERSION 3.16)
project(redis_lite_server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(redis_lite_server
    src/main.cpp
    src/CommandProcessor.cpp
    src/KeyValueStore.cpp
    src/PersistenceManager.cpp
    src/Server.cpp
    src/ThreadPool.cpp
    src/Utilities.cpp
)

target_include_directories(redis_lite_server PRIVATE include)

target_link_libraries(redis_lite_server PRIVATE pthread)
