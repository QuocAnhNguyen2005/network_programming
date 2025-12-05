#include <iostream>
#include <cstring>
#include "protocol.h" // Include the protocol header

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

int main() {
    // Initialize Winsock on Windows (no-op on POSIX)
#ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed: " << WSAGetLastError() << std::endl; // Windows error
        return 1;
    }
#endif

    std::cout << "Server is starting..." << std::endl;

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
    setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt)); // on POSIX cast not required; on Windows use char*

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

    std::cout << "Server listening on port " << DEFAULT_PORT << std::endl;

    // Accept one connection (example). In real server, loop and handle each client (thread/select/etc.)
    struct sockaddr_in clientAddr;
    socklen_t clientLen = sizeof(clientAddr);
    SOCKET clientSock = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
    if (clientSock == INVALID_SOCKET) {
#ifdef _WIN32
        std::cerr << "accept() failed: " << WSAGetLastError() << std::endl;
#else
        std::cerr << "accept() failed: " << strerror(errno) << std::endl;
#endif
        CLOSE_SOCKET(serverSocket);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    std::cout << "Client connected." << std::endl;

    // ... handle client (recv/send) ...

    // Close client socket and server socket, cleanup Winsock
    CLOSE_SOCKET(clientSock);
    CLOSE_SOCKET(serverSocket);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}

