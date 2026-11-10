#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

const std::string PROTOCOL_VERSION = "1.0";

// TCPBuffer handles continuous byte streams from TCP sockets and extracts messages delimited by '\n'
class TCPBuffer {
private:
    int socketFd;
    std::string buffer;

public:
    explicit TCPBuffer(int fd) : socketFd(fd) {}

    bool sendMessage(const std::string& message) {
        std::string formattedMsg = message;
        if (formattedMsg.empty() || formattedMsg.back() != '\n') {
            formattedMsg += '\n';
        }
        ssize_t totalSent = 0;
        ssize_t length = formattedMsg.size();
        while (totalSent < length) {
            ssize_t sent = send(socketFd, formattedMsg.c_str() + totalSent, length - totalSent, 0);
            if (sent <= 0) return false;
            totalSent += sent;
        }
        return true;
    }

    bool receiveMessage(std::string& outMessage) {
        size_t pos = buffer.find('\n');
        while (pos == std::string::npos) {
            char tempBuffer[1024];
            ssize_t bytesRead = recv(socketFd, tempBuffer, sizeof(tempBuffer), 0);
            if (bytesRead <= 0) {
                return false;
            }
            buffer.append(tempBuffer, bytesRead);
            pos = buffer.find('\n');
        }

        outMessage = buffer.substr(0, pos);
        buffer.erase(0, pos + 1);
        
        if (!outMessage.empty() && outMessage.back() == '\r') {
            outMessage.pop_back();
        }
        return true;
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  Host mode:    " << programName << " --listen <port> <nickname>\n"
              << "  Guest mode:   " << programName << " --connect <ip> <port> <nickname>\n";
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
    
    close(serverFd);
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

bool performHandshake(TCPBuffer& tcpBuffer, const std::string& nickname) {
    // Send OLA message
    std::string olaMsg = "OLA " + PROTOCOL_VERSION + " " + nickname;
    if (!tcpBuffer.sendMessage(olaMsg)) {
        std::cerr << "Failed to send OLA message.\n";
        return false;
    }

    // Receive OLA message from peer
    std::string response;
    if (!tcpBuffer.receiveMessage(response)) {
        std::cerr << "Failed to receive OLA response.\n";
        return false;
    }

    // Parse OLA: OLA <version> <nickname>
    if (response.rfind("OLA ", 0) != 0) {
        std::cerr << "Invalid handshake response: " << response << "\n";
        return false;
    }

    std::cout << "[Protocol] Peer handshake received: " << response << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    int socketFd = -1;
    std::string nickname;

    if (mode == "--listen") {
        int port = std::stoi(argv[2]);
        nickname = argv[3];
        socketFd = startServer(port);
    } else if (mode == "--connect") {
        std::string ip = argv[2];
        int port = std::stoi(argv[3]);
        if (argc < 5) {
            std::cerr << "Error: Nickname missing for --connect\n";
            printUsage(argv[0]);
            return 1;
        }
        nickname = argv[4];
        socketFd = startClient(ip, port);
    } else {
        printUsage(argv[0]);
        return 1;
    }

    if (socketFd >= 0) {
        TCPBuffer tcpBuffer(socketFd);

        // Perform Handshake (OLA)
        if (performHandshake(tcpBuffer, nickname)) {
            std::cout << "[Protocol] Handshake successful! Sending PRONTO...\n";
            tcpBuffer.sendMessage("PRONTO");

            std::string peerReady;
            if (tcpBuffer.receiveMessage(peerReady) && peerReady == "PRONTO") {
                std::cout << "[Protocol] Both peers are PRONTO! Game can start.\n";
            } else {
                std::cout << "[Protocol] Expected PRONTO from peer, got: " << peerReady << "\n";
            }
        }

        close(socketFd);
    }

    return 0;
}