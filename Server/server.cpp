#include <iostream>
#include <cstring>
#include <thread>        // For std::thread
#include <vector>        // For storing active threads
#include "../protocol.h" // Include the protocol header
#include "broker.h"      // Include the message broker

#ifdef _WIN32
#include <winsock2.h>                  // Winsock2 API: sockets on Windows (must call WSAStartup())
#include <ws2tcpip.h>                  // Helper functions: getaddrinfo, inet_pton, IPv6 helpers
#pragma comment(lib, "ws2_32.lib")     // Link with Winsock library at link time
#define CLOSE_SOCKET(s) closesocket(s) // Use closesocket() on Windows

#else
#include <sys/types.h>           // Primitive system data types for sockets
#include <sys/socket.h>          // socket(), bind(), listen(), accept(), etc.
#include <netinet/in.h>          // sockaddr_in structure and IPPROTO_ constants
#include <arpa/inet.h>           // inet_pton(), inet_ntop()
#include <netdb.h>               // getaddrinfo(), getnameinfo()
#include <unistd.h>              // close()
#include <fcntl.h>               // fcntl() (useful for non-blocking sockets)
#include <errno.h>               // errno for error reporting
#define SOCKET int               // POSIX sockets are file descriptors (ints)
#define INVALID_SOCKET -1        // Invalid socket descriptor value
#define SOCKET_ERROR -1          // Generic socket error constant
#define CLOSE_SOCKET(s) close(s) // Use close() on POSIX systems
#endif

// Global message broker instance (shared across all client threads)
MessageBroker g_broker;

// ===== OPTIMIZATION: Reliable socket operations =====
// Purpose: Ensure all bytes are transmitted/received, handle partial operations
// - Standard recv/send can return partial data counts
// - We loop until all bytes transferred to guarantee message integrity
// - Critical for binary data like files and structured packet headers

// Receive all bytes reliably
bool recvAllBytes(SOCKET sock, char *buffer, int totalBytes)
{
    int recvd = 0;
    while (recvd < totalBytes)
    {
        int n = recv(sock, buffer + recvd, totalBytes - recvd, 0);
        if (n <= 0)
            return false; // Connection closed or error
        recvd += n;
    }
    return true;
}

// Send all bytes reliably
bool sendAllBytes(SOCKET sock, const char *buffer, int totalBytes)
{
    int sent = 0;
    while (sent < totalBytes)
    {
        int n = send(sock, buffer + sent, totalBytes - sent, 0);
        if (n <= 0)
            return false; // Error or connection closed
        sent += n;
    }
    return true;
}

// Client handler function - runs in a separate thread for each connected client
void handleClient(int clientId, SOCKET clientSocket)
{
    std::cout << "[THREAD " << clientId << "] Client handler started." << std::endl;

    char buffer[MAX_BUFFER_SIZE]; // buffer for receiving packet header
    // ===== OPTIMIZATION: Track login state =====
    // Purpose: Properly log connection state for debugging and cleanup
    // - Know if client successfully authenticated before disconnecting
    // - Helps identify network vs authentication failures
    bool clientLoggedIn = false; // Track login state

    while (true)
    {
        // ===== OPTIMIZATION: Use reliable receive function =====
        // Purpose: Ensure packet headers arrive completely
        // - Prevents partial header reads that corrupt message parsing
        // - Handles network fragmentation transparently
        if (!recvAllBytes(clientSocket, buffer, sizeof(PacketHeader)))
        {
            if (clientLoggedIn)
            {
                std::cout << "[THREAD " << clientId << "] Client disconnected." << std::endl;
            }
            else
            {
                std::cout << "[THREAD " << clientId << "] Connection closed before login." << std::endl;
            }
            break; // exit recv loop
        }

        // Parse the received packet header
        PacketHeader *header = (PacketHeader *)buffer;
        std::cout << "[THREAD " << clientId << "] Received msgType=" << header->msgType
                  << ", payloadLen=" << header->payloadLength
                  << ", sender=" << header->sender
                  << ", topic=" << header->topic << std::endl;

        // ===== OPTIMIZATION: Validate payload size before allocation =====
        // Purpose: Prevent buffer overflow and DoS attacks
        // - Reject oversized payloads before processing
        // - Protects against malicious clients sending huge buffers
        // - Enforces MAX_MESSAGE_SIZE limit across all operations
        if (header->payloadLength > MAX_MESSAGE_SIZE)
        {
            std::cerr << "[THREAD " << clientId << "] Payload exceeds max size: " << header->payloadLength << std::endl;
            break; // Close connection on invalid payload
        }

        // Receive payload if needed
        char payloadBuffer[MAX_BUFFER_SIZE];
        std::memset(payloadBuffer, 0, MAX_BUFFER_SIZE);

        if (header->payloadLength > 0)
        {
            // ===== OPTIMIZATION: Bounds checking before socket read =====
            // Purpose: Prevent buffer overflow on read
            // - Check if payload fits in our buffer before recv() call
            // - Protects against malformed packets claiming size > MAX_BUFFER_SIZE
            if (header->payloadLength > sizeof(payloadBuffer))
            {
                std::cerr << "[THREAD " << clientId << "] Payload buffer overflow attempt: " << header->payloadLength << std::endl;
                break; // Close connection
            }

            // ===== OPTIMIZATION: Reliable receive for payload =====
            // Purpose: Ensure complete payload delivery
            // - Large payloads may arrive in multiple TCP packets
            // - recvAllBytes ensures we get every byte before processing
            if (!recvAllBytes(clientSocket, payloadBuffer, header->payloadLength))
            {
                std::cout << "[THREAD " << clientId << "] Client disconnected while receiving payload." << std::endl;
                break;
            }
            std::cout << "[THREAD " << clientId << "] Received " << header->payloadLength << " bytes payload" << std::endl;
        } // Handle different message types
        switch (header->msgType)
        {
        case MSG_LOGIN:
        {
            // ===== OPTIMIZATION: Validate username before broker operations =====
            // Purpose: Reject invalid credentials early before authentication
            // - Empty or oversized usernames indicate client errors
            // - Prevents storing malformed usernames in broker
            // - Reduces unnecessary broker operations for bad requests
            if (strlen(header->sender) == 0 || strlen(header->sender) >= MAX_USERNAME_LEN)
            {
                std::cout << "[THREAD " << clientId << "] LOGIN REJECTED: Invalid username length." << std::endl;

                PacketHeader errHeader;
                std::memset(&errHeader, 0, sizeof(errHeader));
                errHeader.msgType = MSG_ERROR;
                errHeader.payloadLength = 0;
                std::strcpy(errHeader.sender, "SERVER");
                sendAllBytes(clientSocket, (const char *)&errHeader, sizeof(PacketHeader));

                CLOSE_SOCKET(clientSocket);
                return; // Exit thread
            }

            // Check if username is already taken by another online client
            if (g_broker.isUsernameTaken(header->sender))
            {
                // Send ERROR response to client
                PacketHeader errHeader;
                std::memset(&errHeader, 0, sizeof(errHeader));
                errHeader.msgType = MSG_ERROR;
                errHeader.payloadLength = 0;
                std::strcpy(errHeader.sender, "SERVER");
                std::string errMsg = "Username already taken";
                errHeader.payloadLength = (uint32_t)errMsg.size();

                sendAllBytes(clientSocket, (const char *)&errHeader, sizeof(PacketHeader));
                if (errHeader.payloadLength > 0)
                {
                    sendAllBytes(clientSocket, errMsg.c_str(), errHeader.payloadLength);
                }

                // Close connection and exit thread
                CLOSE_SOCKET(clientSocket);
                return; // Exit thread
            }

            // Register the client in broker (store username and socket)
            g_broker.registerClient(clientSocket, header->sender);
            clientLoggedIn = true; // Mark as logged in

            // [AUTO-SUBSCRIBE] Automatically subscribe client to topic with their username
            // This allows other clients to send direct messages to this user by publishing to <username> topic
            g_broker.subscribeToTopic(clientId, header->sender);
            std::cout << "[THREAD " << clientId << "] Auto-subscribed to personal topic: " << header->sender << std::endl;

            // Send LOGIN ACK back
            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));
            std::cout << "[THREAD " << clientId << "] LOGIN ACK sent for user: " << header->sender << std::endl;
            break;
        }

        case MSG_SUBSCRIBE:
        {
            // Subscribe to topic
            if (strlen(header->topic) == 0)
            {
                std::cerr << "[THREAD " << clientId << "] Invalid topic name in SUBSCRIBE" << std::endl;
                break;
            }
            g_broker.subscribeToTopic(clientId, header->topic);

            // Send ACK
            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            std::strcpy(ackHeader.topic, header->topic);
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));
            std::cout << "[THREAD " << clientId << "] SUBSCRIBE ACK sent for topic: " << header->topic << std::endl;
            break;
        }

        case MSG_UNSUBSCRIBE:
        {
            // Unsubscribe from topic
            if (strlen(header->topic) == 0)
            {
                std::cerr << "[THREAD " << clientId << "] Invalid topic name in UNSUBSCRIBE" << std::endl;
                break;
            }
            g_broker.unsubscribeFromTopic(clientId, header->topic);

            // Send ACK
            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            std::strcpy(ackHeader.topic, header->topic);
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));
            std::cout << "[THREAD " << clientId << "] UNSUBSCRIBE ACK sent for topic: " << header->topic << std::endl;
            break;
        }

        case MSG_PUBLISH_TEXT:
        {
            // Publish text message to topic subscribers
            if (strlen(header->topic) == 0)
            {
                std::cerr << "[THREAD " << clientId << "] Invalid topic name in PUBLISH_TEXT" << std::endl;
                break;
            }
            int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            std::cout << "[THREAD " << clientId << "] Published to " << sentCount << " subscribers on topic: "
                      << header->topic << std::endl;

            // Send ACK to publisher
            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            std::strcpy(ackHeader.topic, header->topic);
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));
            break;
        }

        case MSG_PUBLISH_FILE:
        {
            // Similar to text, but for files
            if (strlen(header->topic) == 0)
            {
                std::cerr << "[THREAD " << clientId << "] Invalid topic name in PUBLISH_FILE" << std::endl;
                break;
            }
            int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            std::cout << "[THREAD " << clientId << "] File published to " << sentCount << " subscribers." << std::endl;

            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            std::strcpy(ackHeader.topic, header->topic);
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));
            break;
        }

        case MSG_LOGOUT:
        {
            std::cout << "[THREAD " << clientId << "] Client logout request received." << std::endl;
            clientLoggedIn = false;

            // Send ACK
            PacketHeader ackHeader;
            std::memset(&ackHeader, 0, sizeof(ackHeader));
            ackHeader.msgType = MSG_ACK;
            ackHeader.payloadLength = 0;
            ackHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(ackHeader.sender, "SERVER");
            sendAllBytes(clientSocket, (const char *)&ackHeader, sizeof(PacketHeader));

            // Close connection
            break;
        }

        default:
        {
            std::cout << "[THREAD " << clientId << "] Unknown message type: " << header->msgType << std::endl;

            // Send ERROR response
            PacketHeader errHeader;
            std::memset(&errHeader, 0, sizeof(errHeader));
            errHeader.msgType = MSG_ERROR;
            errHeader.payloadLength = 0;
            errHeader.messageId = header->messageId; // Echo back message ID
            std::strcpy(errHeader.sender, "SERVER");
            sendAllBytes(clientSocket, (const char *)&errHeader, sizeof(PacketHeader));
            break;
        }
        }
    }

    // Cleanup: unregister client and close socket
    g_broker.unregisterClient(clientId);
    std::cout << "[THREAD " << clientId << "] Client handler terminated." << std::endl;
}

int main()
{
    // Initialize Winsock on Windows (no-op on POSIX)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
    {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl; // Windows error
        return 1;
    }
#endif

    std::cout << "=== PUB/SUB SERVER STARTING ===" << std::endl;

    // Create a TCP socket
    SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (serverSocket == INVALID_SOCKET)
    {
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
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // Bind to any address and DEFAULT_PORT from protocol.h
    struct sockaddr_in srvAddr;
    std::memset(&srvAddr, 0, sizeof(srvAddr));
    srvAddr.sin_family = AF_INET;
    srvAddr.sin_addr.s_addr = htonl(INADDR_ANY); // listen on all interfaces
    srvAddr.sin_port = htons(DEFAULT_PORT);

    if (bind(serverSocket, (struct sockaddr *)&srvAddr, sizeof(srvAddr)) == SOCKET_ERROR)
    {
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
    if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR)
    {
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

    while (true)
    {
        // Accept incoming connection
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);

        SOCKET clientSock = accept(serverSocket, (struct sockaddr *)&clientAddr, &clientLen);

        if (clientSock == INVALID_SOCKET)
        {
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
