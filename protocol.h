#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <cstdint> // using for uint32_t, uint64_t
#include <cstring> // using for memset

// Default ports and buffer sizes
#define DEFAULT_PORT 8080
#define MAX_BUFFER_SIZE 4096
#define MAX_TOPIC_LEN 32
#define MAX_USERNAME_LEN 32
#define SOCKET_TIMEOUT_MS 5000  // 5 second socket timeout
#define MAX_MESSAGE_SIZE (10 * 1024 * 1024)  // 10MB max message size


//Message types
enum MessageType {
    MSG_LOGIN = 1,
    MSG_LOGOUT,

    MSG_SUBSCRIBE,
    MSG_UNSUBSCRIBE,

    MSG_PUBLISH_TEXT,
    MSG_PUBLISH_FILE,
    MSG_FILE_DATA,

    MSG_ERROR, //Báo lỗi (vd: sai pass, trùng tên hoặc NACK)
    MSG_ACK // Xác nhận (ACK)
};
#pragma pack(push, 1) // ensure no padding
struct PacketHeader {
    uint32_t msgType; // MessageType
    uint32_t payloadLength; // Length of the payload
    uint32_t messageId; // Unique message ID
    uint64_t timestamp; // Timestamp of the message
    uint8_t version; // Protocol version
    uint8_t flags; // Bit flags for properities
    char sender[MAX_USERNAME_LEN]; // Sender's username
    char topic[MAX_TOPIC_LEN]; // Topic name
    uint32_t checksum; // CRC32 for integrity check
};
#pragma pack(pop)
#endif