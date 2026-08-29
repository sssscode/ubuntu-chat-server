#include "tcp_connection.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <unistd.h>

static constexpr uint32_t MAX_MESSAGE_SIZE = 1024 * 1024;

TCPConnection::TCPConnection(int socketFd)
    : socketFd(socketFd)
{
}

TCPConnection::~TCPConnection()
{
    close();
}

void TCPConnection::close()
{
    int currentFd = -1;

    {
        std::lock_guard<std::mutex> lock(socketMutex);
        currentFd = socketFd;
        socketFd = -1;
    }

    if (currentFd >= 0)
    {
        shutdown(currentFd, SHUT_RDWR);
        ::close(currentFd);
    }
}

bool TCPConnection::isConnected() const
{
    std::lock_guard<std::mutex> lock(socketMutex);
    return socketFd >= 0;
}

int TCPConnection::socket() const
{
    std::lock_guard<std::mutex> lock(socketMutex);
    return socketFd;
}

bool TCPConnection::sendExact(
    int fd,
    const void* buffer,
    size_t size)
{
    const char* data = static_cast<const char*>(buffer);
    size_t total = 0;

    while (total < size)
    {
        ssize_t sent = ::send(
            fd,
            data + total,
            size - total,
            MSG_NOSIGNAL);

        if (sent <= 0)
            return false;

        total += static_cast<size_t>(sent);
    }

    return true;
}

bool TCPConnection::recvExact(
    int fd,
    void* buffer,
    size_t size)
{
    char* data = static_cast<char*>(buffer);
    size_t total = 0;

    while (total < size)
    {
        ssize_t received = ::recv(
            fd,
            data + total,
            size - total,
            0);

        if (received <= 0)
            return false;

        total += static_cast<size_t>(received);
    }

    return true;
}

bool TCPConnection::send(
    const std::vector<uint8_t>& bytes)
{
    std::lock_guard<std::mutex> sendLock(sendMutex);

    int fd = socket();

    if (fd < 0)
    {
        std::cerr << "[TCP SEND] NO SOCKET" << std::endl;
        return false;
    }

    if (bytes.size() > MAX_MESSAGE_SIZE)
    {
        std::cerr << "[TCP SEND] MESSAGE TOO LARGE" << std::endl;
        return false;
    }

    uint32_t length = static_cast<uint32_t>(bytes.size());
    uint32_t networkLength = htonl(length);

    if (!sendExact(fd, &networkLength, sizeof(networkLength)))
    {
        std::cerr << "[TCP SEND] LENGTH SEND FAILED: "
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    if (length == 0)
        return true;

    if (!sendExact(fd, bytes.data(), bytes.size()))
    {
        std::cerr << "[TCP SEND] PAYLOAD SEND FAILED: "
                  << std::strerror(errno) << std::endl;
        close();
        return false;
    }

    std::cerr << "[TCP SEND] MESSAGE SENT SIZE="
              << length << std::endl;

    return true;
}

bool TCPConnection::read(
    std::vector<uint8_t>& bytes)
{
    int fd = socket();

    if (fd < 0)
        return false;

    uint32_t networkLength = 0;

    if (!recvExact(fd, &networkLength, sizeof(networkLength)))
    {
        std::cerr << "[TCP READ] LENGTH READ FAILED" << std::endl;
        close();
        return false;
    }

    uint32_t length = ntohl(networkLength);

    if (length > MAX_MESSAGE_SIZE)
    {
        std::cerr << "[TCP READ] MESSAGE TOO LARGE: "
                  << length << std::endl;
        close();
        return false;
    }

    bytes.resize(length);

    if (length == 0)
        return true;

    if (!recvExact(fd, bytes.data(), bytes.size()))
    {
        std::cerr << "[TCP READ] PAYLOAD READ FAILED" << std::endl;
        close();
        return false;
    }

    std::cerr << "[TCP READ] MESSAGE RECEIVED SIZE="
              << length << std::endl;

    return true;
}
