#include "chat_server.h"

#include <arpa/inet.h>
#include <cerrno>
#include <cstring>
#include <iostream>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <algorithm>

ChatServer::ChatServer()
    : listenSocket(-1),
      running(false)
{
}

ChatServer::~ChatServer()
{
    stop();
}

bool ChatServer::listen(uint16_t port)
{
    if (running)
        return false;

    int newSocket = ::socket(
        AF_INET,
        SOCK_STREAM,
        0);

    if (newSocket < 0)
    {
        std::cerr << "[CHAT SERVER] SOCKET CREATE FAILED: "
                  << std::strerror(errno) << std::endl;
        return false;
    }

    int reuse = 1;
    setsockopt(
        newSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        &reuse,
        sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    if (::bind(
            newSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) < 0)
    {
        std::cerr << "[CHAT SERVER] BIND FAILED: "
                  << std::strerror(errno) << std::endl;
        ::close(newSocket);
        return false;
    }

    if (::listen(newSocket, SOMAXCONN) < 0)
    {
        std::cerr << "[CHAT SERVER] LISTEN FAILED: "
                  << std::strerror(errno) << std::endl;
        ::close(newSocket);
        return false;
    }

    listenSocket = newSocket;
    running = true;

    acceptThread = std::thread(
        &ChatServer::acceptLoop,
        this);

    std::cerr << "[CHAT SERVER] LISTENING PORT="
              << port << std::endl;
    std::cerr << "[CHAT SERVER] HARBOR RUNNING"
              << std::endl;

    return true;
}

void ChatServer::stop()
{
    if (!running)
        return;

    running = false;

    int fd = listenSocket;
    listenSocket = -1;

    if (fd >= 0)
    {
        shutdown(fd, SHUT_RDWR);
        ::close(fd);
    }

    if (acceptThread.joinable())
        acceptThread.join();

    std::vector<std::shared_ptr<TCPConnection>> oldConnections;

    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        oldConnections.swap(connections);
    }

    for (auto& connection : oldConnections)
    {
        if (connection)
            connection->close();
    }
}

bool ChatServer::isRunning() const
{
    return running;
}

void ChatServer::acceptLoop()
{
    while (running)
    {
        sockaddr_in clientAddress{};
        socklen_t addressLength = sizeof(clientAddress);

        int clientFd = ::accept(
            listenSocket,
            reinterpret_cast<sockaddr*>(&clientAddress),
            &addressLength);

        if (clientFd < 0)
        {
            if (running)
            {
                std::cerr << "[CHAT SERVER] ACCEPT FAILED: "
                          << std::strerror(errno) << std::endl;
            }
            continue;
        }

        char clientIp[INET_ADDRSTRLEN]{};
        inet_ntop(
            AF_INET,
            &clientAddress.sin_addr,
            clientIp,
            sizeof(clientIp));

        uint16_t clientPort = ntohs(clientAddress.sin_port);

        std::cerr << "[CHAT SERVER] CLIENT CONNECTED "
                  << clientIp << ":" << clientPort << std::endl;

        auto connection = std::make_shared<TCPConnection>(clientFd);

        {
            std::lock_guard<std::mutex> lock(connectionsMutex);
            connections.push_back(connection);
        }

        std::thread(
            &ChatServer::clientLoop,
            this,
            connection).detach();
    }
}

void ChatServer::clientLoop(
    std::shared_ptr<TCPConnection> connection)
{
    while (running && connection->isConnected())
    {
        std::vector<uint8_t> message;

        if (!connection->read(message))
            break;

        std::cerr << "[CHAT SERVER] MESSAGE RECEIVED SIZE="
                  << message.size() << std::endl;

        broadcast(message);
    }

    std::cerr << "[CHAT SERVER] CLIENT DISCONNECTED"
              << std::endl;

    removeConnection(connection);
    connection->close();
}

void ChatServer::broadcast(
    const std::vector<uint8_t>& message)
{
    auto currentConnections = [&]()
    {
        std::lock_guard<std::mutex> lock(connectionsMutex);
        return connections;
    }();

    std::cerr << "[CHAT SERVER] BROADCAST TO "
              << currentConnections.size()
              << " CONNECTIONS" << std::endl;

    for (auto& connection : currentConnections)
    {
        if (!connection || !connection->isConnected())
            continue;

        if (!connection->send(message))
            removeConnection(connection);
    }
}

void ChatServer::removeConnection(
    const std::shared_ptr<TCPConnection>& connection)
{
    std::lock_guard<std::mutex> lock(connectionsMutex);

    connections.erase(
        std::remove(
            connections.begin(),
            connections.end(),
            connection),
        connections.end());
}
