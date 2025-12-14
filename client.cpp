// client.cpp
// Simple Pub/Sub client compatible with server.cpp and protocol.h
// Usage (interactive):
//   /login <username>
//   /subscribe <topic>
//   /unsubscribe <topic>
//   /publish <topic> <message>
//   /sendfile <topic> <filepath>   (sends file in one chunk; for large files you can adapt to chunking)
//   /logout
//   /quit

#include <iostream>
#include <thread>
#include <string>
#include <vector>
#include <fstream>
#include <cstring>
#include <atomic>
#include <chrono>       // For std::chrono::milliseconds in sleep_for

#include "protocol.h" // uses PacketHeader, MessageType, DEFAULT_PORT, etc. :contentReference[oaicite:2]{index=2}

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

// Helpers: send all bytes, recv all bytes
bool sendAll(socket_t sock, const char *data, int total)
{
    int sent = 0;
    while (sent < total)
    {
        int n = send(sock, data + sent, total - sent, 0);
        if (n == SOCKET_ERROR)
            return false;
        sent += n;
    }
    return true;
}

bool recvAll(socket_t sock, char *buffer, int total)
{
    int recvd = 0;
    while (recvd < total)
    {
        int n = recv(sock, buffer + recvd, total - recvd, 0);
        if (n == 0)
            return false; // connection closed
        if (n == SOCKET_ERROR)
            return false;
        recvd += n;
    }
    return true;
}

std::atomic<bool> running(true);

void receiverThread(socket_t sock)
{
    // Continuously receive PacketHeader then payload
    while (running)
    {
        PacketHeader header;
        // read header
        if (!recvAll(sock, (char *)&header, sizeof(PacketHeader)))
        {
            // Only print error if it's not a graceful shutdown (running flag still true)
            if (running) {
                std::cerr << "[RECV] Connection closed or error while reading header.\n";
            }
            running = false;
            break;
        }

        // Print basic header info
        std::cout << "\n[INCOMING] msgType=" << header.msgType
                  << " messageId=" << header.messageId
                  << " from=" << header.sender
                  << " topic=" << header.topic
                  << " payloadLen=" << header.payloadLength
                  << "\n";

        // Read payload if present
        if (header.payloadLength > 0)
        {
            std::vector<char> payload(header.payloadLength + 1);
            if (!recvAll(sock, payload.data(), header.payloadLength))
            {
                if (running) {
                    std::cerr << "[RECV] Error reading payload.\n";
                }
                running = false;
                break;
            }
            payload[header.payloadLength] = '\0';
            // Print as text (if binary, this may be garbage)
            std::cout << "[INCOMING][payload]:\n"
                      << std::string(payload.data(), header.payloadLength) << "\n";
        }
        std::cout << "> "; // prompt
        std::flush(std::cout);
    }
}

int main(int argc, char **argv)
{
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        std::cerr << "WSAStartup failed\n";
        return 1;
    }
#endif

    std::string host = "127.0.0.1";
    int port = DEFAULT_PORT; // from protocol.h
    if (argc >= 2)
        host = argv[1];
    if (argc >= 3)
        port = atoi(argv[2]);

    // Create socket
    socket_t sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == INVALID_SOCKET)
    {
        std::cerr << "socket() failed\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    // Resolve and connect
    struct sockaddr_in srv;
    std::memset(&srv, 0, sizeof(srv));
    srv.sin_family = AF_INET;
    srv.sin_port = htons(port);
    if (inet_pton(AF_INET, host.c_str(), &srv.sin_addr) <= 0)
    {
        std::cerr << "Invalid address: " << host << "\n";
        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    if (connect(sock, (struct sockaddr *)&srv, sizeof(srv)) == SOCKET_ERROR)
    {
        std::cerr << "connect() failed\n";
        CLOSE_SOCKET(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";
    std::cout << "Type /login <username> to begin.\n";

    // Start receiver thread
    std::thread recvT(receiverThread, sock);

    // Interactive loop
    std::string line;
    int nextMessageId = 1;
    char username[MAX_USERNAME_LEN] = {0};

    while (running && std::getline(std::cin, line))
    {
        if (line.empty())
            continue;

        // parse commands
        if (line.rfind("/login ", 0) == 0)
        {
            std::string user = line.substr(7);
            if (user.empty())
            {
                std::cout << "Usage: /login <username>\n";
                continue;
            }
            std::strncpy(username, user.c_str(), MAX_USERNAME_LEN - 1);

            PacketHeader hdr;
            std::memset(&hdr, 0, sizeof(hdr));
            hdr.msgType = MSG_LOGIN;
            hdr.payloadLength = 0;
            hdr.messageId = nextMessageId++;
            std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);
            // send header
            if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
            {
                std::cerr << "send header failed\n";
                break;
            }
            std::cout << "[SENT] LOGIN as " << username << "\n";
        }
        else if (line.rfind("/subscribe ", 0) == 0)
        {
            std::string topic = line.substr(11);
            if (topic.empty())
            {
                std::cout << "Usage: /subscribe <topic>\n";
                continue;
            }

            PacketHeader hdr;
            std::memset(&hdr, 0, sizeof(hdr));
            hdr.msgType = MSG_SUBSCRIBE;
            hdr.payloadLength = 0;
            hdr.messageId = nextMessageId++;
            std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);
            std::strncpy(hdr.topic, topic.c_str(), MAX_TOPIC_LEN - 1);

            if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
            {
                std::cerr << "send failed\n";
                break;
            }
            std::cout << "[SENT] SUBSCRIBE " << topic << "\n";
        }
        else if (line.rfind("/unsubscribe ", 0) == 0)
        {
            std::string topic = line.substr(13);
            if (topic.empty())
            {
                std::cout << "Usage: /unsubscribe <topic>\n";
                continue;
            }

            PacketHeader hdr;
            std::memset(&hdr, 0, sizeof(hdr));
            hdr.msgType = MSG_UNSUBSCRIBE;
            hdr.payloadLength = 0;
            hdr.messageId = nextMessageId++;
            std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);
            std::strncpy(hdr.topic, topic.c_str(), MAX_TOPIC_LEN - 1);

            if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
            {
                std::cerr << "send failed\n";
                break;
            }
            std::cout << "[SENT] UNSUBSCRIBE " << topic << "\n";
        }
        else if (line.rfind("/publish ", 0) == 0)
        {
            // format: /publish <topic> <message...>
            size_t pos = line.find(' ', 9); // find end of topic
            if (pos == std::string::npos)
            {
                std::cout << "Usage: /publish <topic> <message>\n";
                continue;
            }
            std::string topic = line.substr(9, pos - 9);
            std::string msg = line.substr(pos + 1);
            if (topic.empty() || msg.empty())
            {
                std::cout << "Usage: /publish <topic> <message>\n";
                continue;
            }

            PacketHeader hdr;
            std::memset(&hdr, 0, sizeof(hdr));
            hdr.msgType = MSG_PUBLISH_TEXT;
            hdr.payloadLength = (uint32_t)msg.size();
            hdr.messageId = nextMessageId++;
            std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);
            std::strncpy(hdr.topic, topic.c_str(), MAX_TOPIC_LEN - 1);

            if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
            {
                std::cerr << "send header failed\n";
                break;
            }
            if (hdr.payloadLength > 0)
            {
                if (!sendAll(sock, msg.data(), (int)hdr.payloadLength))
                {
                    std::cerr << "send payload failed\n";
                    break;
                }
            }
            std::cout << "[SENT] PUBLISH to " << topic << "\n";
        }
        else if (line.rfind("/sendfile ", 0) == 0)
        {
            // Format: /sendfile <topic> <filepath>
            // This version sends file in chunks (streaming) instead of all at once
            size_t pos = line.find(' ', 10);
            if (pos == std::string::npos)
            {
                std::cout << "Usage: /sendfile <topic> <filepath>\n";
                continue;
            }
            std::string topic = line.substr(10, pos - 10);
            std::string path = line.substr(pos + 1);

            // Open file for reading in binary mode
            std::ifstream ifs(path, std::ios::binary);
            if (!ifs)
            {
                std::cerr << "Cannot open file: " << path << "\n";
                continue;
            }

            // Send file in chunks
            char fileBuf[MAX_BUFFER_SIZE];
            uint32_t totalBytesSent = 0;
            bool sendError = false;

            while (ifs.read(fileBuf, sizeof(fileBuf)) || ifs.gcount() > 0)
            {
                int bytesRead = (int)ifs.gcount(); // Number of bytes actually read
                
                // Create packet header for this chunk
                PacketHeader hdr;
                std::memset(&hdr, 0, sizeof(hdr));
                hdr.msgType = MSG_PUBLISH_FILE;          // File transmission message
                hdr.payloadLength = (uint32_t)bytesRead; // Size of this chunk
                hdr.messageId = nextMessageId++;
                std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);
                std::strncpy(hdr.topic, topic.c_str(), MAX_TOPIC_LEN - 1);

                // Send header
                if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
                {
                    std::cerr << "Error sending file header\n";
                    sendError = true;
                    break;
                }

                // Send payload chunk
                if (!sendAll(sock, fileBuf, bytesRead))
                {
                    std::cerr << "Error sending file chunk\n";
                    sendError = true;
                    break;
                }

                totalBytesSent += bytesRead;
                std::cout << "[SENDING] Chunk sent: " << bytesRead << " bytes (Total: " << totalBytesSent << " bytes)\n";

                // Small delay to avoid socket buffer overflow (optional but recommended for large files)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }

            if (!sendError)
            {
                std::cout << "[SENT] File transfer completed. Total: " << totalBytesSent << " bytes sent to topic '" << topic << "'\n";
            }
        }
        else if (line == "/logout")
        {
            PacketHeader hdr;
            std::memset(&hdr, 0, sizeof(hdr));
            hdr.msgType = MSG_LOGOUT;
            hdr.payloadLength = 0;
            hdr.messageId = nextMessageId++;
            std::strncpy(hdr.sender, username, MAX_USERNAME_LEN - 1);

            if (!sendAll(sock, (char *)&hdr, sizeof(hdr)))
            {
                std::cerr << "send failed\n";
            }
            std::cout << "[SENT] LOGOUT\n";
            // continue to allow server ACK receipt
        }
        else if (line == "/quit")
        {
            std::cout << "Quitting...\n";
            running = false;
            break;
        }
        else
        {
            std::cout << "Unknown command. Commands:\n"
                      << "  /login <username>\n"
                      << "  /subscribe <topic>\n"
                      << "  /unsubscribe <topic>\n"
                      << "  /publish <topic> <message>\n"
                      << "  /sendfile <topic> <path>\n"
                      << "  /logout\n"
                      << "  /quit\n";
        }
    }

    // cleanup
    running = false;
    CLOSE_SOCKET(sock);
    if (recvT.joinable())
        recvT.join();

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
