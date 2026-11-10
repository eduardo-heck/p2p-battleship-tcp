#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  Host mode:    " << programName << " --listen <port>\n"
              << "  Guest mode:   " << programName << " --connect <ip> <port>\n";
}

int startServer(int port) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return -1;
    }

    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        std::cerr << "Bind failed on port " << port << std::endl;
        close(serverFd);
        return -1;
    }

    if (listen(serverFd, 1) < 0) {
        std::cerr << "Listen failed." << std::endl;
        close(serverFd);
        return -1;
    }

    std::cout << "[Host] Waiting for incoming connection on port " << port << "...\n";
    sockaddr_in clientAddress{};
    socklen_t clientLen = sizeof(clientAddress);
    int clientFd = accept(serverFd, (struct sockaddr*)&clientAddress, &clientLen);
    
    close(serverFd); // Close listening socket as P2P uses 1-to-1 connection
    if (clientFd < 0) {
        std::cerr << "Accept failed." << std::endl;
        return -1;
    }

    std::cout << "[Host] Peer connected successfully!\n";
    return clientFd;
}

int startClient(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        std::cerr << "Error creating socket." << std::endl;
        return -1;
    }

    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr) <= 0) {
        std::cerr << "Invalid address / Address not supported: " << ip << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "[Guest] Connecting to " << ip << ":" << port << "...\n";
    if (connect(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        std::cerr << "Connection failed." << std::endl;
        close(sock);
        return -1;
    }

    std::cout << "[Guest] Connected to host successfully!\n";
    return sock;
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    int socketFd = -1;

    if (mode == "--listen") {
        if (argc < 3) {
            std::cerr << "Error: Port missing for --listen\n";
            printUsage(argv[0]);
            return 1;
        }
        int port = std::stoi(argv[2]);
        socketFd = startServer(port);
    } else if (mode == "--connect") {
        if (argc < 4) {
            std::cerr << "Error: IP and Port missing for --connect\n";
            printUsage(argv[0]);
            return 1;
        }
        std::string ip = argv[2];
        int port = std::stoi(argv[3]);
        socketFd = startClient(ip, port);
    } else {
        printUsage(argv[0]);
        return 1;
    }

    if (socketFd >= 0) {
        // Keep connection open for now; next steps will add the game protocol
        std::cout << "Connection established. Press Enter to exit...\n";
        std::cin.get();
        close(socketFd);
    }

    return 0;
}