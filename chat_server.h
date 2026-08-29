#pragma once

#include <atomic>
#include <cstdint>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

#include "tcp_connection.h"

class ChatServer
{
public:
    ChatServer();
    ~ChatServer();

    ChatServer(const ChatServer&) = delete;
    ChatServer& operator=(const ChatServer&) = delete;

    bool listen(uint16_t port);
    void stop();
    bool isRunning() const;

private:
    void acceptLoop();
    void clientLoop(std::shared_ptr<TCPConnection> connection);
    void broadcast(const std::vector<uint8_t>& message);
    void removeConnection(const std::shared_ptr<TCPConnection>& connection);

private:
    int listenSocket;
    std::atomic<bool> running;
    std::thread acceptThread;

    mutable std::mutex connectionsMutex;
    std::vector<std::shared_ptr<TCPConnection>> connections;
};
