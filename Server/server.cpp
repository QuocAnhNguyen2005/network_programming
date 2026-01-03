#include <iostream>
#include <cstring>
#include <thread>
#include <vector>
#include <mutex>
#include <atomic>
#include <map>
#include "../protocol.h"
#include "broker.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#define SOCKET int
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define CLOSE_SOCKET(s) close(s)
#endif

#define STREAM_PORT 8081
#define CHAT_PORT 8080

// Global message broker
MessageBroker g_broker;
std::mutex cout_mutex;

// Thread-safe logging
void logMessage(const std::string &msg)
{
    std::lock_guard<std::mutex> lock(cout_mutex);
    std::cout << msg << std::endl;
}

// Receive all bytes reliably
bool recvAllBytes(SOCKET sock, char *buffer, int totalBytes)
{
    if (!buffer || totalBytes <= 0)
        return false;

    int recvd = 0;
    while (recvd < totalBytes)
    {
        int n = recv(sock, buffer + recvd, totalBytes - recvd, 0);
        if (n <= 0)
            return false;
        recvd += n;
    }
    return true;
}

// Send all bytes reliably
bool sendAllBytes(SOCKET sock, const char *buffer, int totalBytes)
{
    if (!buffer || totalBytes <= 0)
        return false;

    int sent = 0;
    while (sent < totalBytes)
    {
        int n = send(sock, buffer + sent, totalBytes - sent, 0);
        if (n <= 0)
            return false;
        sent += n;
    }
    return true;
}

// Send error packet to client
void sendErrorPacket(SOCKET sock, uint32_t messageId, const std::string &reason)
{
    PacketHeader errorHeader;
    std::memset(&errorHeader, 0, sizeof(errorHeader));
    errorHeader.msgType = MSG_ERROR;
    errorHeader.payloadLength = reason.length();
    errorHeader.messageId = messageId;
    errorHeader.timestamp = 0;
    std::strcpy(errorHeader.sender, "SERVER");

    sendAllBytes(sock, (const char *)&errorHeader, sizeof(PacketHeader));
    if (reason.length() > 0)
    {
        sendAllBytes(sock, reason.c_str(), reason.length());
    }
}

// Send ACK packet to client
void sendAckPacket(SOCKET sock, uint32_t messageId, const std::string &topic = "")
{
    PacketHeader ackHeader;
    std::memset(&ackHeader, 0, sizeof(ackHeader));
    ackHeader.msgType = MSG_ACK;
    ackHeader.payloadLength = 0;
    ackHeader.messageId = messageId;
    ackHeader.timestamp = 0;
    std::strcpy(ackHeader.sender, "SERVER");
    if (!topic.empty())
    {
        std::strncpy(ackHeader.topic, topic.c_str(), MAX_TOPIC_LEN - 1);
    }

    sendAllBytes(sock, (const char *)&ackHeader, sizeof(PacketHeader));
}

// Forward declaration
void handleClient(int clientId, SOCKET clientSocket);
void handleStreamClient(int clientId, SOCKET streamSocket);

// Stream handler function - handles audio frames
void handleStreamClient(int clientId, SOCKET streamSocket)
{
    logMessage("[STREAM] Client handler started for ID=" + std::to_string(clientId));

    char headerBuffer[sizeof(PacketHeader)];

    while (true)
    {
        // Receive packet header
        if (!recvAllBytes(streamSocket, headerBuffer, sizeof(PacketHeader)))
        {
            logMessage("[STREAM] Connection closed for client " + std::to_string(clientId));
            break;
        }

        PacketHeader *header = (PacketHeader *)headerBuffer;

        // Receive payload if present
        char payloadBuffer[MAX_BUFFER_SIZE] = {0};
        if (header->payloadLength > 0)
        {
            if (header->payloadLength > MAX_BUFFER_SIZE)
            {
                logMessage("[STREAM] Frame too large");
                break;
            }

            if (!recvAllBytes(streamSocket, payloadBuffer, header->payloadLength))
            {
                logMessage("[STREAM] Error reading frame data");
                break;
            }
        }

        // Relay audio frames to all subscribers of the topic
        if (header->msgType == MSG_STREAM_FRAME && strlen(header->topic) > 0)
        {
            int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            // Suppress log spam for frame relay (commented out)
            // logMessage("[STREAM] Relayed frame to " + std::to_string(sentCount) + " subscribers");
        }
    }

    CLOSE_SOCKET(streamSocket);
    logMessage("[STREAM] Client handler terminated for ID=" + std::to_string(clientId));
}

// Client handler function
void handleClient(int clientId, SOCKET clientSocket)
{
    logMessage("[CHAT] Client handler started for ID=" + std::to_string(clientId));

    char headerBuffer[sizeof(PacketHeader)];
    bool clientLoggedIn = false;
    char clientUsername[MAX_USERNAME_LEN] = {0};

    while (true)
    {
        // Receive packet header
        if (!recvAllBytes(clientSocket, headerBuffer, sizeof(PacketHeader)))
        {
            logMessage("[CHAT] Connection closed or error reading header for client " + std::to_string(clientId));
            break;
        }

        PacketHeader *header = (PacketHeader *)headerBuffer;

        // Validate payload size
        if (header->payloadLength > MAX_MESSAGE_SIZE)
        {
            logMessage("[CHAT] Invalid payload size: " + std::to_string(header->payloadLength));
            sendErrorPacket(clientSocket, header->messageId, "Payload too large");
            break;
        }

        // Receive payload if present
        char payloadBuffer[MAX_BUFFER_SIZE] = {0};
        if (header->payloadLength > 0)
        {
            if (header->payloadLength > MAX_BUFFER_SIZE)
            {
                sendErrorPacket(clientSocket, header->messageId, "Payload exceeds buffer size");
                break;
            }

            if (!recvAllBytes(clientSocket, payloadBuffer, header->payloadLength))
            {
                logMessage("[CHAT] Error reading payload for client " + std::to_string(clientId));
                break;
            }
        }

        // Handle different message types
        switch (header->msgType)
        {
        case MSG_LOGIN:
        {
            // Validate username
            if (strlen(header->sender) == 0 || strlen(header->sender) >= MAX_USERNAME_LEN)
            {
                sendErrorPacket(clientSocket, header->messageId, "Invalid username");
                break;
            }

            // Check if username is already taken
            if (g_broker.isUsernameTaken(header->sender))
            {
                sendErrorPacket(clientSocket, header->messageId, "Username already taken");
                break;
            }

            // Register client
            g_broker.registerClient(clientSocket, header->sender);
            clientLoggedIn = true;
            std::strncpy(clientUsername, header->sender, MAX_USERNAME_LEN - 1);

            // Auto-subscribe to personal topic
            g_broker.subscribeToTopic(clientId, header->sender);

            logMessage("[CHAT] Client " + std::to_string(clientId) + " logged in as: " + std::string(header->sender));
            sendAckPacket(clientSocket, header->messageId);
            break;
        }

        case MSG_SUBSCRIBE:
        {
            if (!clientLoggedIn)
            {
                sendErrorPacket(clientSocket, header->messageId, "Not logged in");
                break;
            }

            if (strlen(header->topic) == 0)
            {
                sendErrorPacket(clientSocket, header->messageId, "Empty topic");
                break;
            }

            g_broker.subscribeToTopic(clientId, header->topic);
            logMessage("[CHAT] Client " + std::string(clientUsername) + " subscribed to: " + std::string(header->topic));
            sendAckPacket(clientSocket, header->messageId, header->topic);
            break;
        }

        case MSG_UNSUBSCRIBE:
        {
            if (!clientLoggedIn)
            {
                sendErrorPacket(clientSocket, header->messageId, "Not logged in");
                break;
            }

            if (strlen(header->topic) == 0)
            {
                sendErrorPacket(clientSocket, header->messageId, "Empty topic");
                break;
            }

            g_broker.unsubscribeFromTopic(clientId, header->topic);
            logMessage("[CHAT] Client " + std::string(clientUsername) + " unsubscribed from: " + std::string(header->topic));
            sendAckPacket(clientSocket, header->messageId, header->topic);
            break;
        }

        case MSG_PUBLISH_TEXT:
        {
            if (!clientLoggedIn)
            {
                sendErrorPacket(clientSocket, header->messageId, "Not logged in");
                break;
            }

            if (strlen(header->topic) == 0)
            {
                sendErrorPacket(clientSocket, header->messageId, "Empty topic");
                break;
            }

            int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            logMessage("[CHAT] Published to " + std::to_string(sentCount) + " subscribers on topic: " + std::string(header->topic));
            sendAckPacket(clientSocket, header->messageId, header->topic);
            break;
        }

        case MSG_PUBLISH_FILE:
        {
            if (!clientLoggedIn)
            {
                sendErrorPacket(clientSocket, header->messageId, "Not logged in");
                break;
            }

            if (strlen(header->topic) == 0)
            {
                sendErrorPacket(clientSocket, header->messageId, "Empty topic");
                break;
            }

            int sentCount = g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            logMessage("[CHAT] Published file to " + std::to_string(sentCount) + " subscribers");
            sendAckPacket(clientSocket, header->messageId, header->topic);
            break;
        }

        case MSG_STREAM_FRAME:
        {
            // Forward audio frame to subscribers
            if (strlen(header->topic) > 0)
            {
                g_broker.publishToTopic(header->topic, *header, payloadBuffer, header->payloadLength);
            }
            break;
        }

        case MSG_LOGOUT:
        {
            logMessage("[CHAT] Client " + std::string(clientUsername) + " logged out");
            sendAckPacket(clientSocket, header->messageId);
            clientLoggedIn = false;
            break;
        }

        default:
        {
            logMessage("[CHAT] Unknown message type: " + std::to_string(header->msgType));
            sendErrorPacket(clientSocket, header->messageId, "Unknown message type");
            break;
        }
        }
    }

    // Cleanup
    if (clientLoggedIn)
    {
        g_broker.unregisterClient(clientId);
    }
    CLOSE_SOCKET(clientSocket);
    logMessage("[CHAT] Client handler terminated for ID=" + std::to_string(clientId));
}

int main()
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        logMessage("WSAStartup failed");
        return 1;
    }
#endif

    logMessage("=== PUB/SUB SERVER STARTING ===");

    // Create chat socket
    SOCKET chatSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (chatSocket == INVALID_SOCKET)
    {
        logMessage("Failed to create chat socket");
        return 1;
    }

    // Allow address reuse
    int opt = 1;
    setsockopt(chatSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // Bind chat socket
    sockaddr_in chatAddr{};
    chatAddr.sin_family = AF_INET;
    chatAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    chatAddr.sin_port = htons(CHAT_PORT);

    if (bind(chatSocket, (sockaddr *)&chatAddr, sizeof(chatAddr)) == SOCKET_ERROR)
    {
        logMessage("Failed to bind chat socket");
        CLOSE_SOCKET(chatSocket);
        return 1;
    }

    // Listen on chat socket
    if (listen(chatSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        logMessage("Failed to listen on chat socket");
        CLOSE_SOCKET(chatSocket);
        return 1;
    }

    logMessage("[MAIN] Chat server listening on port " + std::to_string(CHAT_PORT));
    logMessage("[MAIN] Waiting for clients...");

    // Create stream socket
    SOCKET streamSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (streamSocket == INVALID_SOCKET)
    {
        logMessage("Failed to create stream socket");
        CLOSE_SOCKET(chatSocket);
        return 1;
    }

    // Allow address reuse
    opt = 1;
    setsockopt(streamSocket, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof(opt));

    // Bind stream socket
    sockaddr_in streamAddr{};
    streamAddr.sin_family = AF_INET;
    streamAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    streamAddr.sin_port = htons(STREAM_PORT);

    if (bind(streamSocket, (sockaddr *)&streamAddr, sizeof(streamAddr)) == SOCKET_ERROR)
    {
        logMessage("Failed to bind stream socket");
        CLOSE_SOCKET(chatSocket);
        CLOSE_SOCKET(streamSocket);
        return 1;
    }

    // Listen on stream socket
    if (listen(streamSocket, SOMAXCONN) == SOCKET_ERROR)
    {
        logMessage("Failed to listen on stream socket");
        CLOSE_SOCKET(chatSocket);
        CLOSE_SOCKET(streamSocket);
        return 1;
    }

    logMessage("[MAIN] Stream server listening on port " + std::to_string(STREAM_PORT));

    // Accept client connections
    int clientIdCounter = 0;

    // Launch stream accept thread
    std::thread streamAcceptThread([&]()
                                   {
        int streamIdCounter = 0;
        while (true)
        {
            sockaddr_in clientAddr{};
            int addrLen = sizeof(clientAddr);
            SOCKET clientStreamSocket = accept(streamSocket, (sockaddr *)&clientAddr, &addrLen);

            if (clientStreamSocket == INVALID_SOCKET)
            {
                logMessage("Failed to accept stream client connection");
                continue;
            }

            logMessage("[MAIN] New stream client connection accepted");

            // Handle stream client in separate thread
            std::thread streamThread(handleStreamClient, streamIdCounter++, clientStreamSocket);
            streamThread.detach();
        } });
    streamAcceptThread.detach();

    // Chat accept loop
    while (true)
    {
        sockaddr_in clientAddr{};
        int addrLen = sizeof(clientAddr);
        SOCKET clientSocket = accept(chatSocket, (sockaddr *)&clientAddr, &addrLen);

        if (clientSocket == INVALID_SOCKET)
        {
            logMessage("Failed to accept client connection");
            continue;
        }

        logMessage("[MAIN] New chat client connection accepted");

        // Handle client in separate thread
        std::thread clientThread(handleClient, clientIdCounter++, clientSocket);
        clientThread.detach(); // Let thread run independently
    }

    CLOSE_SOCKET(chatSocket);
    CLOSE_SOCKET(streamSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
