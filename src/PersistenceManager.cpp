#include "PersistenceManager.hpp"

#include "Utilities.hpp"

#include <fstream>
#include <iostream>
#include <utility>

namespace redis_lite {

PersistenceManager::PersistenceManager(std::string filename)
    : filename_(std::move(filename)) {}

void PersistenceManager::appendCommand(const std::string& command) {
    std::lock_guard<std::mutex> lock(fileMutex_);

    std::ofstream out(filename_, std::ios::app);
    if (!out.is_open()) {
        std::cerr << "Failed to open AOF file: " << filename_ << '\n';
        return;
    }

    out << command << '\n';
}

std::vector<std::string> PersistenceManager::loadCommands() {
    std::vector<std::string> commands;

    std::ifstream in(filename_);
    if (!in.is_open()) {
        return commands;
    }

    std::string line;
    while (std::getline(in, line)) {
        line = utils::trim(line);
        if (!line.empty()) {
            commands.push_back(line);
        }
    }

    return commands;
}

} // namespace redis_lite
