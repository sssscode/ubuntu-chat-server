#pragma once

#include <cstdint>
#include <mutex>
#include <string>
#include <vector>

#include <sys/socket.h>

class TCPConnection
{
public:
    explicit TCPConnection(int socketFd);
    ~TCPConnection();

    TCPConnection(const TCPConnection&) = delete;
    TCPConnection& operator=(const TCPConnection&) = delete;

    bool send(const std::vector<uint8_t>& bytes);
    bool read(std::vector<uint8_t>& bytes);

    void close();
    bool isConnected() const;
    int socket() const;

private:
    static bool sendExact(int fd, const void* buffer, size_t size);
    static bool recvExact(int fd, void* buffer, size_t size);

private:
    int socketFd;
    mutable std::mutex socketMutex;
    std::mutex sendMutex;
};
