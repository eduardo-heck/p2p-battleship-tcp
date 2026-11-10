# P2P Battleship over TCP

A peer-to-peer Battleship game implemented in C++ using TCP sockets, developed for the Computer Networks course at UNISINOS.

## Features
- **Pure P2P Architecture**: No central server; hosts and guests connect directly via TCP sockets[cite: 2].
- **Robust Message Framing**: Uses a custom `TCPBuffer` to handle continuous byte streams and resolve packet fragmentation/concatenation via `\n` delimiters[cite: 2].
- **Application-Layer Protocol**: Includes `OLA`, `PRONTO`, `TIRO`, `RESULTADO`, `ERRO`, and `FROTA` messages with sequence numbering (`seq`)[cite: 3, 4].
- **Cross-platform / Terminal Interface**: 10x10 grid with clear command-line text output[cite: 2, 4].

## Compilation
```bash
g++ -std=c++17 batalha_naval.cpp -o batalha_naval