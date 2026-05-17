#pragma once

#include "CommandProcessor.hpp"

namespace redis_lite {

class Server {
public:
    Server(int port, CommandProcessor& commandProcessor);
    ~Server();

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

    void start();

private:
    void handleClient(int clientFd);
    void setupSocket();

    int port_;
    int serverFd_{-1};
    CommandProcessor& commandProcessor_;
};

} // namespace redis_lite
