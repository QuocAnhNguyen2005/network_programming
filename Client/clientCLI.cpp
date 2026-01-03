// clientCLI.cpp - Pub/Sub CLI Client
// Communicates with server using protocol.h
// Commands: /login <user>, /subscribe <topic>, /publish <topic> <msg>, /logout, /quit

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <atomic>
#include <sstream>
#include "../protocol.h"

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#define CLOSE_SOCKET(s) closesocket(s)
typedef SOCKET socket_t;
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <errno.h>
#define CLOSE_SOCKET(s) close(s)
typedef int socket_t;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#endif

std::atomic<bool> running(true);
std::atomic<int> nextMessageId(1);

// Thread-safe logging
void logMessage(const std::string &msg)
{
    std::cout << msg << std::endl;
}

// Receive all bytes reliably
bool recvAll(socket_t sock, char *buffer, int total)
{
    if (total <= 0 || !buffer)
        return false;

    int recvd = 0;
    while (recvd < total)
    {
        int n = recv(sock, buffer + recvd, total - recvd, 0);
        if (n <= 0)
            return false;
        recvd += n;
    }
    return true;
}

// Send all bytes reliably
bool sendAll(socket_t sock, const char *data, int total)
{
    if (total <= 0 || !data)
        return false;

    int sent = 0;
    while (sent < total)
    {
        int n = send(sock, data + sent, total - sent, 0);
        if (n == SOCKET_ERROR || n <= 0)
            return false;
        sent += n;
    }
    return true;
}

// Receiver thread - continuously receive and display messages
void receiverThread(socket_t sock)
{
    char headerBuffer[sizeof(PacketHeader)];

    while (running)
    {
        // Receive packet header
        if (!recvAll(sock, headerBuffer, sizeof(PacketHeader)))
        {
            if (running)
            {
                logMessage("\n[RECV] Connection closed or error");
            }
            running = false;
            break;
        }

        PacketHeader *header = (PacketHeader *)headerBuffer;

        // Validate payload size
        if (header->payloadLength > MAX_MESSAGE_SIZE)
        {
            logMessage("\n[RECV] Invalid payload size");
            running = false;
            break;
        }

        // Receive payload if present
        std::string payload;
        if (header->payloadLength > 0)
        {
            if (header->payloadLength > MAX_BUFFER_SIZE)
            {
                logMessage("\n[RECV] Payload too large for buffer");
                running = false;
                break;
            }

            std::vector<char> buf(header->payloadLength + 1);
            if (!recvAll(sock, buf.data(), header->payloadLength))
            {
                if (running)
                {
                    logMessage("\n[RECV] Error reading payload");
                }
                running = false;
                break;
            }
            buf[header->payloadLength] = '\0';
            payload = std::string(buf.data(), header->payloadLength);
        }

        // Handle different message types
        if (header->msgType == MSG_ACK)
        {
            logMessage("\n[ACK] Message " + std::to_string(header->messageId) + " acknowledged");
        }
        else if (header->msgType == MSG_ERROR)
        {
            logMessage("\n[ERROR] " + payload);
        }
        else if (header->msgType == MSG_PUBLISH_TEXT)
        {
            logMessage("\n[" + std::string(header->topic) + "] " +
                       std::string(header->sender) + ": " + payload);
        }
        else if (header->msgType == MSG_PUBLISH_FILE)
        {
            logMessage("\n[FILE] " + std::string(header->sender) +
                       " sent file (" + std::to_string(header->payloadLength) + " bytes)");
        }
        else if (header->msgType == MSG_STREAM_FRAME)
        {
            logMessage("\n[AUDIO] Received frame (" + std::to_string(header->payloadLength) + " bytes)");
        }
        else
        {
            logMessage("\n[MSG] Type=" + std::to_string(header->msgType) +
                       ", Size=" + std::to_string(header->payloadLength));
        }

        std::cout << "> ";
        std::flush(std::cout);
    }
}

// Send packet to server
bool sendPacket(socket_t sock, MessageType type, const std::string &sender,
                const std::string &topic, const std::string &payload)
{
    PacketHeader header;
    std::memset(&header, 0, sizeof(header));

    header.msgType = type;
    header.payloadLength = payload.length();
    header.messageId = nextMessageId++;
    header.timestamp = 0;
    std::strncpy(header.sender, sender.c_str(), MAX_USERNAME_LEN - 1);
    std::strncpy(header.topic, topic.c_str(), MAX_TOPIC_LEN - 1);

    if (!sendAll(sock, (char *)&header, sizeof(PacketHeader)))
        return false;

    if (payload.length() > 0)
    {
        if (!sendAll(sock, payload.c_str(), payload.length()))
            return false;
    }

    return true;
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        logMessage("WSAStartup failed");
        return 1;
    }
#endif

    std::string host = "127.0.0.1";
    int port = DEFAULT_PORT;

    if (argc >= 2)
        host = argv[1];
    if (argc >= 3)
        port = std::atoi(argv[2]);

    // Create socket
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        logMessage("socket() failed");
        return 1;
    }

    // Connect to server
    struct sockaddr_in srv;
    std::memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);

    if (inet_pton(AF_INET, host.c_str(), &srv.sin_addr) <= 0)
    {
        logMessage("Invalid address: " + host);
        CLOSE_SOCKET(sock);
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) == SOCKET_ERROR)
    {
        logMessage("connect() failed");
        CLOSE_SOCKET(sock);
        return 1;
    }

    logMessage("Connected to " + host + ":" + std::to_string(port));
    logMessage("Commands: /login <user>, /subscribe <topic>, /unsubscribe <topic>,");
    logMessage("          /publish <topic> <msg>, /logout, /quit");

    // Start receiver thread
    std::thread recvT(receiverThread, sock);

    // Main input loop
    std::string username;
    std::string line;

    std::cout << "> ";
    std::flush(std::cout);

    while (running && std::getline(std::cin, line))
    {
        if (line.empty())
        {
            std::cout << "> ";
            std::flush(std::cout);
            continue;
        }

        // Parse commands
        if (line.rfind("/login ", 0) == 0)
        {
            username = line.substr(7);
            if (username.empty() || username.length() >= MAX_USERNAME_LEN)
            {
                logMessage("Usage: /login <username> (max " +
                           std::to_string(MAX_USERNAME_LEN - 1) + " chars)");
            }
            else
            {
                if (sendPacket(sock, MSG_LOGIN, username, "", ""))
                {
                    logMessage("[SENT] LOGIN as " + username);
                }
                else
                {
                    logMessage("Failed to send login packet");
                    break;
                }
            }
        }
        else if (line.rfind("/subscribe ", 0) == 0)
        {
            std::string topic = line.substr(11);
            if (topic.empty() || topic.length() >= MAX_TOPIC_LEN)
            {
                logMessage("Usage: /subscribe <topic> (max " +
                           std::to_string(MAX_TOPIC_LEN - 1) + " chars)");
            }
            else if (sendPacket(sock, MSG_SUBSCRIBE, username, topic, ""))
            {
                logMessage("[SENT] SUBSCRIBE to " + topic);
            }
            else
            {
                logMessage("Failed to send subscribe packet");
                break;
            }
        }
        else if (line.rfind("/unsubscribe ", 0) == 0)
        {
            std::string topic = line.substr(13);
            if (topic.empty() || topic.length() >= MAX_TOPIC_LEN)
            {
                logMessage("Usage: /unsubscribe <topic> (max " +
                           std::to_string(MAX_TOPIC_LEN - 1) + " chars)");
            }
            else if (sendPacket(sock, MSG_UNSUBSCRIBE, username, topic, ""))
            {
                logMessage("[SENT] UNSUBSCRIBE from " + topic);
            }
            else
            {
                logMessage("Failed to send unsubscribe packet");
                break;
            }
        }
        else if (line.rfind("/publish ", 0) == 0)
        {
            std::string rest = line.substr(9);
            size_t spacePos = rest.find(' ');
            if (spacePos == std::string::npos)
            {
                logMessage("Usage: /publish <topic> <message>");
            }
            else
            {
                std::string topic = rest.substr(0, spacePos);
                std::string msg = rest.substr(spacePos + 1);

                if (topic.empty() || topic.length() >= MAX_TOPIC_LEN)
                {
                    logMessage("Invalid topic");
                }
                else if (msg.empty())
                {
                    logMessage("Message cannot be empty");
                }
                else if (sendPacket(sock, MSG_PUBLISH_TEXT, username, topic, msg))
                {
                    logMessage("[SENT] Message to " + topic);
                }
                else
                {
                    logMessage("Failed to send message");
                    break;
                }
            }
        }
        else if (line.rfind("/logout", 0) == 0)
        {
            if (sendPacket(sock, MSG_LOGOUT, username, "", ""))
            {
                logMessage("[SENT] LOGOUT");
            }
            else
            {
                logMessage("Failed to send logout packet");
            }
        }
        else if (line.rfind("/quit", 0) == 0)
        {
            logMessage("Exiting...");
            running = false;
            break;
        }
        else
        {
            logMessage("Unknown command. Use /login, /subscribe, /publish, /logout, /quit");
        }

        std::cout << "> ";
        std::flush(std::cout);
    }

    // Cleanup
    running = false;
    CLOSE_SOCKET(sock);

    if (recvT.joinable())
    {
        recvT.join();
    }

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
