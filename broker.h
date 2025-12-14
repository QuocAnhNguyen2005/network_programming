#ifndef BROKER_H
#define BROKER_H

#include <iostream>
#include <map>
#include <vector>
#include <set>
#include <mutex>
#include <memory>
#include <cstring>
#include <algorithm>    // For std::find()
#include "protocol.h"

#ifdef _WIN32
    #include <winsock2.h>
    #define CLOSE_SOCKET(s) closesocket(s)
#else
    #include <unistd.h>
    #define CLOSE_SOCKET(s) close(s)
#endif

// Struct to hold client information
struct ClientInfo {
    int clientId;                           // Unique client identifier
    SOCKET socket;                          // Client socket
    char username[MAX_USERNAME_LEN];        // Client's username
    std::set<std::string> subscribedTopics; // Topics this client subscribed to
    bool isConnected;                       // Connection status

    ClientInfo() : clientId(-1), socket(INVALID_SOCKET), isConnected(false) {
        std::memset(username, 0, MAX_USERNAME_LEN);
    }
};

// Message Broker - manages all clients and pub/sub logic
class MessageBroker {
private:
    std::map<int, std::shared_ptr<ClientInfo>> clients;           // client_id -> ClientInfo
    std::map<std::string, std::vector<int>> topicSubscribers;     // topic -> list of client_ids
    std::mutex clientsMutex;                                       // Protect clients map
    std::mutex topicsMutex;                                        // Protect topics map
    int nextClientId;                                              // Auto-increment client ID

public:
    MessageBroker() : nextClientId(0) {}

    // Register a new client
    int registerClient(SOCKET clientSocket, const char* username) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        int clientId = nextClientId++;
        auto clientInfo = std::make_shared<ClientInfo>();
        clientInfo->clientId = clientId;
        clientInfo->socket = clientSocket;
        clientInfo->isConnected = true;
        std::strncpy(clientInfo->username, username, MAX_USERNAME_LEN - 1);
        
        clients[clientId] = clientInfo;
        std::cout << "[BROKER] Client registered: ID=" << clientId 
                  << ", Username=" << username << std::endl;
        return clientId;
    }

    // Unregister and disconnect a client
    void unregisterClient(int clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        if (clients.count(clientId)) {
            auto client = clients[clientId];
            client->isConnected = false;
            
            // Close socket
            if (client->socket != INVALID_SOCKET) {
                CLOSE_SOCKET(client->socket);
            }
            
            clients.erase(clientId);
            std::cout << "[BROKER] Client unregistered: ID=" << clientId << std::endl;
            
            // Remove from all topic subscriptions
            unsubscribeClientFromAllTopics(clientId);
        }
    }

    // Subscribe a client to a topic
    void subscribeToTopic(int clientId, const char* topic) {
        std::lock_guard<std::mutex> lock(topicsMutex);
        
        std::string topicStr(topic);
        
        // Add client to topic's subscriber list (avoid duplicates)
        auto& subscribers = topicSubscribers[topicStr];
        if (std::find(subscribers.begin(), subscribers.end(), clientId) == subscribers.end()) {
            subscribers.push_back(clientId);
            std::cout << "[BROKER] Client " << clientId << " subscribed to topic: " << topic << std::endl;
        }
    }

    // Unsubscribe a client from a topic
    void unsubscribeFromTopic(int clientId, const char* topic) {
        std::lock_guard<std::mutex> lock(topicsMutex);
        
        std::string topicStr(topic);
        
        if (topicSubscribers.count(topicStr)) {
            auto& subscribers = topicSubscribers[topicStr];
            auto it = std::find(subscribers.begin(), subscribers.end(), clientId);
            if (it != subscribers.end()) {
                subscribers.erase(it);
                std::cout << "[BROKER] Client " << clientId << " unsubscribed from topic: " << topic << std::endl;
            }
        }
    }

    // Unsubscribe a client from all topics
    void unsubscribeClientFromAllTopics(int clientId) {
        std::lock_guard<std::mutex> lock(topicsMutex);
        
        for (auto& pair : topicSubscribers) {
            auto& subscribers = pair.second;
            auto it = std::find(subscribers.begin(), subscribers.end(), clientId);
            if (it != subscribers.end()) {
                subscribers.erase(it);
            }
        }
    }

    // Publish a message to all subscribers of a topic
    // Returns number of clients the message was sent to
    int publishToTopic(const char* topic, const PacketHeader& header, const char* payload, int payloadLen) {
        std::vector<int> subscriberIds;
        
        // Get list of subscribers (under lock)
        {
            std::lock_guard<std::mutex> lock(topicsMutex);
            if (topicSubscribers.count(topic)) {
                subscriberIds = topicSubscribers[topic];
            }
        }

        int sentCount = 0;
        
        // Send to each subscriber (without holding lock during socket operations)
        for (int clientId : subscriberIds) {
            std::shared_ptr<ClientInfo> client;
            
            {
                std::lock_guard<std::mutex> lock(clientsMutex);
                if (clients.count(clientId)) {
                    client = clients[clientId];
                }
            }

            if (client && client->isConnected && client->socket != INVALID_SOCKET) {
                // Send header
                if (send(client->socket, (const char*)&header, sizeof(PacketHeader), 0) != SOCKET_ERROR) {
                    // Send payload if exists
                    if (payloadLen > 0 && payload) {
                        if (send(client->socket, payload, payloadLen, 0) != SOCKET_ERROR) {
                            sentCount++;
                            std::cout << "[BROKER] Message published to client " << clientId 
                                      << " on topic: " << topic << std::endl;
                        }
                    } else {
                        sentCount++;
                        std::cout << "[BROKER] Message published to client " << clientId 
                                  << " on topic: " << topic << std::endl;
                    }
                }
            }
        }

        return sentCount;
    }

    // Get client info by ID
    std::shared_ptr<ClientInfo> getClient(int clientId) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        if (clients.count(clientId)) {
            return clients[clientId];
        }
        return nullptr;
    }

    // Get all online clients count
    int getOnlineClientCount() {
        std::lock_guard<std::mutex> lock(clientsMutex);
        return clients.size();
    }

    // Get all subscribed clients for a topic
    std::vector<int> getTopicSubscribers(const char* topic) {
        std::lock_guard<std::mutex> lock(topicsMutex);
        if (topicSubscribers.count(topic)) {
            return topicSubscribers[topic];
        }
        return std::vector<int>();
    }

    // Check if username is already taken by an online client
    // Returns true if username exists and client is connected
    bool isUsernameTaken(const char* username) {
        std::lock_guard<std::mutex> lock(clientsMutex);
        
        // Iterate through all connected clients
        for (auto const& pair : clients) {
            auto const& client = pair.second;
            // Check if client is connected AND username matches (case-sensitive)
            if (client && client->isConnected && std::strcmp(client->username, username) == 0) {
                return true;
            }
        }
        return false;
    }
};

#endif // BROKER_H
