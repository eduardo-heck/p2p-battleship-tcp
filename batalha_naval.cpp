#include <iostream>
#include <string>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <sstream>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netdb.h>

const std::string PROTOCOL_VERSION = "1";
const int BOARD_SIZE = 10;

struct Ship {
    std::string code;
    int size;
    char startRow;
    int startCol;
    char orientation;
    int hits;
};

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

class Board {
private:
    std::vector<std::vector<char>> grid;
    std::vector<Ship> ships;

public:
    Board() {
        grid = std::vector<std::vector<char>>(BOARD_SIZE, std::vector<char>(BOARD_SIZE, '~'));
    }

    bool placeShip(const std::string& code, int size, char row, int col, char orientation) {
        int r = row - 'A';
        int c = col - 1;

        if (orientation == 'H') {
            if (c + size > BOARD_SIZE) return false;
            for (int i = 0; i < size; ++i) {
                if (grid[r][c + i] != '~') return false;
            }
            for (int i = 0; i < size; ++i) {
                grid[r][c + i] = code[0];
            }
        } else if (orientation == 'V') {
            if (r + size > BOARD_SIZE) return false;
            for (int i = 0; i < size; ++i) {
                if (grid[r + i][c] != '~') return false;
            }
            for (int i = 0; i < size; ++i) {
                grid[r + i][c] = code[0];
            }
        } else {
            return false;
        }

        ships.push_back({code, size, row, col, orientation, 0});
        return true;
    }

    void setupDefaultFleet() {
        placeShip("PA", 5, 'A', 1, 'H');
        placeShip("EN", 4, 'C', 1, 'H');
        placeShip("CR", 3, 'E', 1, 'H');
        placeShip("CR", 3, 'G', 1, 'H');
        placeShip("DE", 2, 'I', 1, 'H');
        placeShip("DE", 2, 'I', 4, 'H');
    }

    std::string getFleetString() const {
        std::string result = "FROTA";
        for (const auto& ship : ships) {
            result += " " + ship.code + ":" + ship.startRow + std::to_string(ship.startCol) + ":" + ship.orientation;
        }
        return result;
    }

    std::string receiveShot(char row, int col, bool& isGameOver) {
        int r = row - 'A';
        int c = col - 1;

        if (r < 0 || r >= BOARD_SIZE || c < 0 || c >= BOARD_SIZE) {
            return "ERRO Coordenada invalida";
        }

        char& cell = grid[r][c];
        if (cell == '~') {
            cell = 'O';
            return "AGUA";
        } else if (cell == 'O' || cell == 'X') {
            return "ERRO Coordenada ja usada";
        } else {
            char shipChar = cell;
            cell = 'X';

            for (auto& ship : ships) {
                if (ship.code[0] == shipChar) {
                    ship.hits++;
                    if (ship.hits == ship.size) {
                        bool allSunk = true;
                        for (const auto& s : ships) {
                            if (s.hits < s.size) {
                                allSunk = false;
                                break;
                            }
                        }
                        if (allSunk) {
                            isGameOver = true;
                            return "AFUNDOU " + ship.code + " FIM";
                        }
                        return "AFUNDOU " + ship.code;
                    }
                    return "ACERTOU";
                }
            }
            return "ACERTOU";
        }
    }

    void printBoard() const {
        std::cout << "   1 2 3 4 5 6 7 8 9 10\n";
        for (int i = 0; i < BOARD_SIZE; ++i) {
            std::cout << (char)('A' + i) << "  ";
            for (int j = 0; j < BOARD_SIZE; ++j) {
                std::cout << grid[i][j] << " ";
            }
            std::cout << "\n";
        }
    }
};

void printUsage(const char* programName) {
    std::cout << "Usage:\n"
              << "  Host mode:    " << programName << " --listen <port> <nickname>\n"
              << "  Guest mode:   " << programName << " --connect <ip> <port> <nickname>\n";
}

int startServer(int port) {
    int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0) return -1;
    int opt = 1;
    setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(serverFd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        close(serverFd);
        return -1;
    }
    if (listen(serverFd, 1) < 0) {
        close(serverFd);
        return -1;
    }
    std::cout << "[Host] Waiting for connection on port " << port << "...\n";
    sockaddr_in clientAddress{};
    socklen_t clientLen = sizeof(clientAddress);
    int clientFd = accept(serverFd, (struct sockaddr*)&clientAddress, &clientLen);
    close(serverFd);
    return clientFd;
}

int startClient(const std::string& ip, int port) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;
    sockaddr_in servAddr{};
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &servAddr.sin_addr) <= 0) {
        close(sock);
        return -1;
    }
    std::cout << "[Guest] Connecting to " << ip << ":" << port << "...\n";
    if (connect(sock, (struct sockaddr*)&servAddr, sizeof(servAddr)) < 0) {
        close(sock);
        return -1;
    }
    return sock;
}

bool performHandshake(TCPBuffer& tcpBuffer, const std::string& nickname) {
    std::string olaMsg = "OLA " + PROTOCOL_VERSION + " " + nickname;
    if (!tcpBuffer.sendMessage(olaMsg)) return false;

    std::string response;
    if (!tcpBuffer.receiveMessage(response)) return false;

    if (response.rfind("OLA ", 0) != 0) {
        tcpBuffer.sendMessage("ERRO Versao incompativel");
        return false;
    }

    std::cout << "[Protocol] Handshake successful with peer: " << response << "\n";
    return true;
}

int main(int argc, char* argv[]) {
    if (argc < 4) {
        printUsage(argv[0]);
        return 1;
    }

    std::string mode = argv[1];
    bool isHost = (mode == "--listen");
    int socketFd = -1;
    std::string nickname;

    if (isHost) {
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
        Board myBoard;
        myBoard.setupDefaultFleet();

        std::cout << "[Game] Fleet positioned. Your board:\n";
        myBoard.printBoard();

        if (performHandshake(tcpBuffer, nickname)) {
            tcpBuffer.sendMessage("PRONTO");
            std::string peerReady;
            if (tcpBuffer.receiveMessage(peerReady) && peerReady == "PRONTO") {
                std::cout << "[Protocol] Both players PRONTO. Game started!\n";

                bool myTurn = isHost; // Host shoots first[cite: 6]
                int mySeq = 0;
                int peerLastSeq = 0;
                bool gameOver = false;

                while (!gameOver) {
                    if (myTurn) {
                        std::cout << "\n--- YOUR TURN ---\n";
                        std::cout << "Enter coordinate (e.g. B7): ";
                        std::string coord;
                        if (!(std::cin >> coord) || coord.length() < 2) break;

                        mySeq++;
                        std::string tiroMsg = "TIRO " + std::to_string(mySeq) + " " + coord;
                        tcpBuffer.sendMessage(tiroMsg);
                        std::cout << "[Sent] " << tiroMsg << "\n";

                        std::string resultMsg;
                        if (!tcpBuffer.receiveMessage(resultMsg)) break;
                        std::cout << "[Received] " << resultMsg << "\n";

                        if (resultMsg.find("FIM") != std::string::npos) {
                            std::cout << "Victory! You sunk the enemy fleet!\n";
                            gameOver = true;
                        }
                        myTurn = false;
                    } else {
                        std::cout << "\n--- OPPONENT'S TURN ---\n";
                        std::string incomingMsg;
                        if (!tcpBuffer.receiveMessage(incomingMsg)) break;
                        std::cout << "[Received] " << incomingMsg << "\n";

                        std::stringstream ss(incomingMsg);
                        std::string type;
                        ss >> type;

                        if (type == "TIRO") {
                            int seq;
                            std::string coordStr;
                            ss >> seq >> coordStr;

                            if (seq <= peerLastSeq) {
                                // Duplicate sequence handling: re-send same result without recounting[cite: 6]
                                continue;
                            }
                            peerLastSeq = seq;

                            char r = coordStr[0];
                            int c = std::stoi(coordStr.substr(1));

                            bool isGameOver = false;
                            std::string res = myBoard.receiveShot(r, c, isGameOver);
                            std::string resultReply = "RESULTADO " + std::to_string(seq) + " " + res;
                            tcpBuffer.sendMessage(resultReply);
                            std::cout << "[Sent] " << resultReply << "\n";

                            if (isGameOver) {
                                std::cout << "Defeat! All your ships were sunk.\n";
                                gameOver = true;
                            }
                            myTurn = true;
                        } else if (type == "ERRO") {
                            std::cout << "[Protocol Error]: " << incomingMsg << "\n";
                        } else if (type == "TCHAU") {
                            break;
                        }
                    }
                }

                // Final Fleet Conference[cite: 6]
                std::string myFleetMsg = myBoard.getFleetString();
                tcpBuffer.sendMessage(myFleetMsg);
                std::cout << "[Protocol] Sent fleet configuration: " << myFleetMsg << "\n";

                std::string peerFleetMsg;
                if (tcpBuffer.receiveMessage(peerFleetMsg)) {
                    std::cout << "[Protocol] Received opponent fleet configuration: " << peerFleetMsg << "\n";
                }

                tcpBuffer.sendMessage("TCHAU");
            }
        }

        close(socketFd);
    }

    return 0;
}