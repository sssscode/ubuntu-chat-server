#include "chat_server.h"

#include <csignal>
#include <iostream>
#include <thread>
#include <chrono>

static volatile std::sig_atomic_t stopRequested = 0;

static void signalHandler(int)
{
    stopRequested = 1;
}

int main(int argc, char* argv[])
{
    uint16_t port = 5000;

    if (argc >= 2)
    {
        int value = std::stoi(argv[1]);

        if (value < 1 || value > 65535)
        {
            std::cerr << "Invalid port" << std::endl;
            return 1;
        }

        port = static_cast<uint16_t>(value);
    }

    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);

    ChatServer server;

    if (!server.listen(port))
        return 1;

    while (!stopRequested)
    {
        std::this_thread::sleep_for(
            std::chrono::seconds(1));
    }

    std::cerr << "[CHAT SERVER] STOPPING"
              << std::endl;

    server.stop();

    return 0;
}
