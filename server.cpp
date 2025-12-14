#include <iostream>
#include <cstring>
#include <thread>          // For std::thread
#include <vector>          // For storing active threads
#include "protocol.h"      // Include the protocol header
#include "broker.h"        // Include the message broker

#ifdef _WIN32
    #include <winsock2.h>    // Winsock2 API: sockets on Windows (must call WSAStartup())
    #include <ws2tcpip.h>    // Helper functions: getaddrinfo, inet_pton, IPv6 helpers
    #pragma comment(lib, "ws2_32.lib") // Link with Winsock library at link time
    #define CLOSE_SOCKET(s) closesocket(s) // Use closesocket() on Windows


#else
    #include <sys/types.h>  // Primitive system data types for sockets
    #include <sys/socket.h> // socket(), bind(), listen(), accept(), etc.
    #include <netinet/in.h> // sockaddr_in structure and IPPROTO_ constants
    #include <arpa/inet.h>  // inet_pton(), inet_ntop()
    #include <netdb.h>      // getaddrinfo(), getnameinfo()
    #include <unistd.h>     // close()
    #include <fcntl.h>      // fcntl() (useful for non-blocking sockets)
    #include <errno.h>      // errno for error reporting
    #define SOCKET int           // POSIX sockets are file descriptors (ints)
    #define INVALID_SOCKET -1    // Invalid socket descriptor value
    #define SOCKET_ERROR -1      // Generic socket error constant
    #define CLOSE_SOCKET(s) close(s) // Use close() on POSIX systems
#endif

// Global message broker instance (shared across all client threads)
MessageBroker g_broker;

// Client handler function - runs in a separate thread for each connected client
void handleClient(int clientId, SOCKET clientSocket) {
    std::cout << "[THREAD " << clientId << "] Client handler started." << std::endl;
    
    char buffer[MAX_BUFFER_SIZE]; // buffer for receiving packet header
    int bytesRecv = 0;

    while (true) {
        // Receive the packet header from client
        bytesRecv = recv(clientSocket, buffer, sizeof(PacketHeader), 0);
        
        if (bytesRecv == SOCKET_ERROR) {
            // Error on recv
#ifdef _WIN32
            std::cerr << "[THREAD " << clientId << "] recv() failed: " << WSAGetLastError() << std::endl;
#else
            std::cerr << "[THREAD " << clientId << "] recv() failed: " << strerror(errno) << std::endl;
#endif
            break; // exit recv loop on error
        } else if (bytesRecv == 0) {
            // Client closed the connection (graceful close)
            std::cout << "[THREAD " << clientId << "] Client disconnected." << std::endl;
            break; // exit recv loop
        }

        // Parse the received packet header
        PacketHeader* header = (PacketHeader*)buffer;
        std::cout << "[THREAD " << clientId << "] Received msgType=" << header->msgType 
                  << ", payloadLen=" << header->payloadLength
                  << ", sender=" << header->sender 
                  << ", topic=" << header->topic << std::endl;

        // Receive payload if needed
        char payloadBuffer[MAX_BUFFER_SIZE];
        std::memset(payloadBuffer, 0, MAX_BUFFER_SIZE);
        
        if (header->payloadLength > 0) {
            int payloadRecv = recv(clientSocket, payloadBuffer, header->payloadLength, 0);
            if (payloadRecv == SOCKET_ERROR) {
#ifdef _WIN32
                std::cerr << "[THREAD " << clientId << "] recv() payload failed: " << WSAGetLastError() << std::endl;
#else
                std::cerr << "[THREAD " << clientId << "] recv() payload failed: " << strerror(errno) << std::endl;
#endif
                break;
            } else if (payloadRecv == 0) {
                std::cout << "[THREAD " << clientId << "] Client disconnected while receiving payload." << std::endl;
                break;
            }
            std::cout << "[THREAD " << clientId << "] Payload (" << payloadRecv << " bytes): " 
                      << std::string(payloadBuffer, payloadRecv) << std::endl;
        }

        // Handle different message types
        switch (header->msgType) {
            case MSG_LOGIN: {
                // Check if username is already taken by another online client
                if (g_broker.isUsernameTaken(header->sender)) {
                    std::cout << "[THREAD " << clientId << "] LOGIN REJECTED: Username '" << header->sender 
                              << "' already taken." << std::endl;
                    
                    // Send ERROR response to client
                    PacketHeader errHeader;
                    std::memset(&errHeader, 0, sizeof(errHeader));
                    errHeader.msgType = MSG_ERROR;
                    errHeader.payloadLength = 0;
                    std::strcpy(errHeader.sender, "SERVER");
                    std::string errMsg = "Username already taken";
                    errHeader.payloadLength = (uint32_t)errMsg.size();
                    
                    send(clientSocket, (const char*)&errHeader, sizeof(PacketHeader), 0);
                    if (errHeader.payloadLength > 0) {
                        send(clientSocket, errMsg.c_str(), errHeader.payloadLength, 0);
                    }
                    
                    // Close connection and exit thread
                    CLOSE_SOCKET(clientSocket);
                    break;
                }
                
                // Register the client in broker (store username and socket)
                g_broker.registerClient(clientSocket, header->sender);
                
                // [AUTO-SUBSCRIBE] Automatically subscribe client to topic with their username
                // This allows other clients to send direct messages to this user by publishing to <username> topic
                g_broker.subscribeToTopic(clientId, header->sender);
                std::cout << "[THREAD " << clientId << "] Auto-subscribed to personal topic: " << header->sender << std::endl;
                
                // Send LOGIN ACK back
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                std::cout << "[THREAD " << clientId << "] LOGIN ACK sent for user: " << header->sender << std::endl;
                break;
            }

            case MSG_SUBSCRIBE: {
                // Subscribe to topic
                g_broker.subscribeToTopic(clientId, header->topic);
                
                // Send ACK
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                std::strcpy(ackHeader.topic, header->topic);
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                std::cout << "[THREAD " << clientId << "] SUBSCRIBE ACK sent for topic: " << header->topic << std::endl;
                break;
            }

            case MSG_UNSUBSCRIBE: {
                // Unsubscribe from topic
                g_broker.unsubscribeFromTopic(clientId, header->topic);
                
                // Send ACK
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                std::strcpy(ackHeader.topic, header->topic);
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                std::cout << "[THREAD " << clientId << "] UNSUBSCRIBE ACK sent for topic: " << header->topic << std::endl;
                break;
            }

            case MSG_PUBLISH_TEXT: {
                // Publish text message to topic subscribers
                int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
                std::cout << "[THREAD " << clientId << "] Published to " << sentCount << " subscribers on topic: " 
                          << header->topic << std::endl;
                
                // Send ACK to publisher
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                std::strcpy(ackHeader.topic, header->topic);
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                break;
            }

            case MSG_PUBLISH_FILE: {
                // Similar to text, but for files
                int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
                std::cout << "[THREAD " << clientId << "] File published to " << sentCount << " subscribers." << std::endl;
                
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                break;
            }

            case MSG_LOGOUT: {
                std::cout << "[THREAD " << clientId << "] Client logout request received." << std::endl;
                
                // Send ACK
                PacketHeader ackHeader;
                std::memset(&ackHeader, 0, sizeof(ackHeader));
                ackHeader.msgType = MSG_ACK;
                ackHeader.payloadLength = 0;
                std::strcpy(ackHeader.sender, "SERVER");
                send(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader), 0);
                
                // Close connection
                break;
            }

            default: {
                std::cout << "[THREAD " << clientId << "] Unknown message type: " << header->msgType << std::endl;
                
                // Send ERROR response
                PacketHeader errHeader;
                std::memset(&errHeader, 0, sizeof(errHeader));
                errHeader.msgType = MSG_ERROR;
                errHeader.payloadLength = 0;
                std::strcpy(errHeader.sender, "SERVER");
                send(clientSocket, (const char*)&errHeader, sizeof(PacketHeader), 0);
                break;
            }
        }
    }

    // Cleanup: unregister client and close socket
    g_broker.unregisterClient(clientId);
    std::cout << "[THREAD " << clientId << "] Client handler terminated." << std::endl;
}

int main() {
    // Initialize Winsock on Windows (no-op on POSIX)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl; // Windows error
        return 1;
    }
#endif

    std::cout << "=== PUB/SUB SERVER STARTING ===" << std::endl;

    // Create a TCP socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET) {
#ifdef _WIN32
        std::cerr << "socket() failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
#else
        std::cerr << "socket() failed: " << strerror(errno) << std::endl;
#endif
        return 1;
    }

    // Allow address reuse to avoid bind errors after quick restart
    int opt = 1;
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    // Bind to any address and DEFAULT_PORT from protocol.h
    struct sockaddr_in srvAddr;
    std::memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces
    srvAddr.sin_port = htons(DEFAULT_PORT);

    if (bind(serverSocket, (struct sockaddr*)&srvAddr, sizeof(srvAddr)) == SOCKET_ERROR) {
#ifdef _WIN32
        std::cerr << "bind() failed: " << WSAGetLastError() << std::endl;
        CLOSE_SOCKET(serverSocket);
        WSACleanup();
#else
        std::cerr << "bind() failed: " << strerror(errno) << std::endl;
        CLOSE_SOCKET(serverSocket);
#endif
        return 1;
    }

    // Start listening
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
#ifdef _WIN32
        std::cerr << "listen() failed: " << WSAGetLastError() << std::endl;
        CLOSE_SOCKET(serverSocket);
        WSACleanup();
#else
        std::cerr << "listen() failed: " << strerror(errno) << std::endl;
        CLOSE_SOCKET(serverSocket);
#endif
        return 1;
    }

    std::cout << "[MAIN] Server listening on port " << DEFAULT_PORT << std::endl;
    std::cout << "[MAIN] Waiting for clients..." << std::endl;

    // Vector to hold active client threads
    std::vector<std::thread> clientThreads;

    // Multi-client accept loop
    int clientIdCounter = 0;
    
    while (true) {
        // Accept incoming connection
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        
        SOCKET clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
        
        if (clientSock == INVALID_SOCKET) {
#ifdef _WIN32
            std::cerr << "[MAIN] accept() failed: " << WSAGetLastError() << std::endl;
#else
            std::cerr << "[MAIN] accept() failed: " << strerror(errno) << std::endl;
#endif
            continue; // Try accepting next connection
        }

        // New client connected
        int clientId = clientIdCounter++;
        std::cout << "[MAIN] *** New client connected: ID=" << clientId 
                  << ", IP=" << inet_ntoa(clientAddr.sin_addr) 
                  << ":" << ntohs(clientAddr.sin_port) << std::endl;
        std::cout << "[MAIN] Total online clients: " << (g_broker.getOnlineClientCount() + 1) << std::endl;

        // Create a thread to handle this client
        // Use std::thread to spawn handleClient in background
        clientThreads.push_back(std::thread(handleClient, clientId, clientSock));
        
        // Detach thread so it runs independently (server won't wait for it to finish)
        clientThreads.back().detach();
    }

    // Cleanup (unreachable in normal flow, but good practice)
    CLOSE_SOCKET(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

