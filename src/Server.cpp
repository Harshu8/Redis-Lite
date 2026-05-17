#include "Server.hpp"

#include "Constants.hpp"
#include "ThreadPool.hpp"
#include "Utilities.hpp"

#include <arpa/inet.h>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <thread>
#include <unistd.h>

namespace redis_lite {

Server::Server(int port, CommandProcessor& commandProcessor)
    : port_(port), commandProcessor_(commandProcessor) {}

Server::~Server() {
    if (serverFd_ != -1) {
        close(serverFd_);
    }
}

void Server::setupSocket() {
    serverFd_ = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd_ == -1) {
        throw std::runtime_error("Failed to create socket");
    }

    int opt = 1;
    setsockopt(serverFd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in serverAddress{};
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    serverAddress.sin_port = htons(port_);

    if (bind(serverFd_, reinterpret_cast<sockaddr*>(&serverAddress), sizeof(serverAddress)) < 0) {
        throw std::runtime_error("Bind failed");
    }

    if (listen(serverFd_, constants::LISTEN_BACKLOG) < 0) {
        throw std::runtime_error("Listen failed");
    }
}

void Server::start() {
    setupSocket();

    ThreadPool threadPool(constants::DEFAULT_THREAD_COUNT, [this](int clientFd) {
        handleClient(clientFd);
    });

    std::cout << "Redis-lite server running on port " << port_ << "...\n";

    while (true) {
        sockaddr_in clientAddress{};
        socklen_t clientLen = sizeof(clientAddress);

        const int clientFd = accept(
            serverFd_,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &clientLen
        );

        if (clientFd < 0) {
            std::cerr << "Failed to accept client\n";
            continue;
        }

        threadPool.addClient(clientFd);
    }
}

void Server::handleClient(int clientFd) {
    std::cout << "Client connected. Thread ID: " << std::this_thread::get_id() << '\n';

    char buffer[constants::BUFFER_SIZE];

    while (true) {
        std::memset(buffer, 0, sizeof(buffer));

        const int bytesRead = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
        if (bytesRead <= 0) {
            std::cout << "Client disconnected. Thread ID: " << std::this_thread::get_id() << '\n';
            break;
        }

        const std::string input(buffer, bytesRead);
        const CommandResult result = commandProcessor_.process(input, true);

        send(clientFd, result.response.c_str(), result.response.size(), 0);

        if (result.shouldCloseConnection) {
            break;
        }
    }

    close(clientFd);
}

} // namespace redis_lite
