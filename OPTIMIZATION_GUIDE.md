# Code Optimization Guide - Pub/Sub Messaging System

This document explains all optimizations applied to the codebase for smooth, secure, and efficient operation.

---

## 1. Protocol Definitions (protocol.h)

### Optimization: Message Size Limits & Socket Timeouts
**Lines:** `#define MAX_MESSAGE_SIZE` and `#define SOCKET_TIMEOUT_MS`

**What Changed:**
- Added `MAX_MESSAGE_SIZE = 10MB` limit
- Added `SOCKET_TIMEOUT_MS = 5000ms` timeout

**Why:**
- **DoS Attack Prevention**: Rejects malicious clients sending 1GB+ messages that crash server
- **Resource Protection**: Limits memory consumption per message to 10MB
- **Idle Client Cleanup**: 5-second timeout disconnects hung clients, freeing threads

**Example Problem Solved:**
- Before: Attacker sends `msgType=1, payloadLength=999999999` → server crashes trying to allocate 1GB
- After: Server rejects payload, logs error, closes connection safely

---

## 2. Message Broker (broker.h)

### Optimization: Input Validation Before Processing
**Lines:** `publishToTopic()` function, payload size checks

**What Changed:**
```cpp
if (!topic || payloadLen < 0) {
    std::cerr << "[BROKER] Invalid publish parameters" << std::endl;
    return 0;  // Reject immediately
}
if (payloadLen > MAX_MESSAGE_SIZE) {
    std::cerr << "[BROKER] Message exceeds maximum size" << std::endl;
    return 0;
}
```

**Why:**
- **Null Pointer Prevention**: Checks `topic` pointer before using it
- **Prevents Buffer Overflow**: Rejects oversized payloads before socket read
- **Early Failure Detection**: Catches errors at source, not during socket ops

**Example Problem Solved:**
- Before: `publishToTopic(nullptr, header, ...)` → crashes with segfault
- After: Function returns 0, logs error, continues running

---

## 3. Server (server.cpp)

### Optimization 3.1: Reliable Send/Receive Functions

**Lines:** `sendAllBytes()` and `recvAllBytes()`

**What Changed:**
```cpp
bool recvAllBytes(SOCKET sock, char* buffer, int totalBytes) {
    int recvd = 0;
    while (recvd < totalBytes) {
        int n = recv(sock, buffer + recvd, totalBytes - recvd, 0);
        if (n <= 0) return false;
        recvd += n;
    }
    return true;
}
```

**Why:**
- **Network Fragmentation Handling**: TCP splits large messages into multiple packets
- **Incomplete Data Prevention**: Loop ensures ALL bytes received before processing
- **Binary Data Safety**: Critical for file transfers (one corrupt byte breaks binary)

**Example Problem Solved:**
- Before: `recv(sock, header, 64, 0)` returns 32 bytes → `memset` fails, crashes
- After: `recvAllBytes()` loops, reads remaining 32 bytes, ensures complete header

---

### Optimization 3.2: Login State Tracking

**Lines:** `bool clientLoggedIn = false`

**What Changed:**
```cpp
bool clientLoggedIn = false;  // Track login state
// ... later in disconnection handler:
if (clientLoggedIn) {
    std::cout << "[THREAD " << clientId << "] Client disconnected." << std::endl;
} else {
    std::cout << "[THREAD " << clientId << "] Connection closed before login." << std::endl;
}
```

**Why:**
- **Better Diagnostics**: Distinguish between auth failures and network issues
- **Debugging**: Helps identify authentication problems vs. connection problems
- **Cleanup Logic**: Know whether to remove from broker's client registry

**Example Problem Solved:**
- Before: All disconnects print same message, unclear if client authenticated
- After: Clear message shows whether user was logged in or not

---

### Optimization 3.3: Payload Size Validation Before Allocation

**Lines:** `if (header->payloadLength > MAX_MESSAGE_SIZE)`

**What Changed:**
```cpp
if (header->payloadLength > MAX_MESSAGE_SIZE) {
    std::cerr << "[THREAD " << clientId << "] Payload exceeds max size: " 
              << header->payloadLength << std::endl;
    break;  // Close connection on invalid payload
}
```

**Why:**
- **DoS Protection**: Rejects fake size claims before allocating memory
- **Resource Limits**: Enforces consistent message size across system
- **Early Termination**: Closes malicious connections immediately

**Example Problem Solved:**
- Before: Malicious client sends `payloadLength=2000000000` → server out of memory
- After: Server reads header, validates size, rejects connection

---

### Optimization 3.4: Buffer Overflow Prevention

**Lines:** `if (header->payloadLength > sizeof(payloadBuffer))`

**What Changed:**
```cpp
if (header->payloadLength > sizeof(payloadBuffer)) {
    std::cerr << "[THREAD " << clientId << "] Payload buffer overflow attempt: " 
              << header->payloadLength << std::endl;
    break;  // Close connection
}
```

**Why:**
- **Stack Overflow Prevention**: Prevents writing beyond 4KB stack buffer
- **Security Hardening**: Catches buffer overflow attacks
- **Controlled Behavior**: Gracefully closes instead of undefined behavior

**Example Problem Solved:**
- Before: `recv(socket, buffer[4096], 10000, 0)` → stack smashing, crash
- After: Check before recv(), reject gracefully

---

### Optimization 3.5: Username Validation Before Login

**Lines:** `if (strlen(header->sender) == 0 || strlen(header->sender) >= MAX_USERNAME_LEN)`

**What Changed:**
```cpp
if (strlen(header->sender) == 0 || strlen(header->sender) >= MAX_USERNAME_LEN) {
    std::cout << "[THREAD " << clientId << "] LOGIN REJECTED: Invalid username length." 
              << std::endl;
    // Send error + close connection
    return;  // Exit thread
}
```

**Why:**
- **Empty Username Prevention**: Empty names don't make sense, reject early
- **Oversized Username Prevention**: Prevents buffer overflow in broker storage
- **Server Resources**: Don't spend cycles on obviously invalid logins

**Example Problem Solved:**
- Before: Client sends username of 500 chars → `strcpy()` overflows
- After: Validate length first, reject safely

---

### Optimization 3.6: Topic Name Validation

**Lines:** `if (strlen(header->topic) == 0)` in SUBSCRIBE/UNSUBSCRIBE/PUBLISH cases

**What Changed:**
```cpp
case MSG_SUBSCRIBE: {
    if (strlen(header->topic) == 0) {
        std::cerr << "[THREAD " << clientId << "] Invalid topic name in SUBSCRIBE" 
                  << std::endl;
        break;
    }
    // ... process subscription
}
```

**Why:**
- **Semantic Validation**: Empty topics are meaningless, reject them
- **Broker Efficiency**: Don't create subscriptions to invalid topics
- **Data Integrity**: Keeps topic map clean and consistent

**Example Problem Solved:**
- Before: Subscribe to empty topic "" → confusing subscription, wasted memory
- After: Reject, log error, continue

---

### Optimization 3.7: Message ID Echo in ACK

**Lines:** `ackHeader.messageId = header->messageId;  // Echo back message ID`

**What Changed:**
```cpp
case MSG_ACK: {
    PacketHeader ackHeader;
    std::memset(&ackHeader, 0, sizeof(ackHeader));
    ackHeader.msgType = MSG_ACK;
    ackHeader.payloadLength = 0;
    ackHeader.messageId = header->messageId;  // NEW: Echo back
    std::strcpy(ackHeader.sender, "SERVER");
    sendAllBytes(clientSocket, (const char*)&ackHeader, sizeof(PacketHeader));
}
```

**Why:**
- **Request Matching**: Client can correlate ACKs with original requests
- **Async Support**: For clients with multiple pending requests, know which was ACKed
- **Timeout Detection**: Client knows which message timed out if no ACK arrives

**Example Problem Solved:**
- Before: Client sends 3 publishes, gets 3 ACKs → doesn't know which is which
- After: ACK contains matching messageId, client knows exact completion order

---

## 4. Client (client.cpp)

### Optimization 4.1: Input Validation with Size Limits

**Lines:** Username, Topic, Message, File size validation

**What Changed:**
```cpp
if (user.empty() || user.length() >= MAX_USERNAME_LEN) {
    std::cout << "Usage: /login <username> (max " << MAX_USERNAME_LEN-1 << " chars)\n";
    continue;  // Reject locally, don't send to server
}
```

**Why:**
- **Local Validation**: Catch bad input before network transmission
- **User Feedback**: Tell user about limits immediately
- **Server Load**: Reduces invalid requests reaching server
- **Bandwidth**: Don't send invalid data across network

**Example Problem Solved:**
- Before: Type 500-char username, server rejects with network latency
- After: Client immediately rejects with helpful message

---

### Optimization 4.2: File Size Pre-Check

**Lines:** File size validation in `/sendfile` command

**What Changed:**
```cpp
ifs.seekg(0, std::ios::end);
size_t fileSize = ifs.tellg();
ifs.seekg(0, std::ios::beg);

if (fileSize > (10 * 1024 * 1024)) {  // 10MB limit
    std::cerr << "File too large (max 10MB): " << path << "\n";
    continue;
}
```

**Why:**
- **Early Failure**: Detect oversized files before transfer starts
- **User Experience**: Immediate feedback instead of hanging transfer
- **Resource Protection**: Don't attempt 100MB file transfers
- **Server Protection**: Server knows clients won't send huge files

**Example Problem Solved:**
- Before: `/sendfile topic /huge_file.iso` → hangs, timeouts, unclear error
- After: Immediate error "File too large (max 10MB)"

---

### Optimization 4.3: Type-Aware Message Display

**Lines:** `receiverThread()` payload handling

**What Changed:**
```cpp
if (header.msgType == MSG_PUBLISH_TEXT) {
    // Text messages: display full content
    std::cout << "\n[MSG from " << header.sender << " in '" << header.topic 
              << "']: " << std::string(payload.data(), header.payloadLength) << "\n";
} 
else if (header.msgType == MSG_PUBLISH_FILE) {
    // File chunks: only show size, NOT binary data
    std::cout << "\n[FILE] Received chunk (" << header.payloadLength 
              << " bytes) from " << header.sender 
              << " in topic '" << header.topic 
              << "' (Binary data - not displayed)\n";
}
```

**Why:**
- **Terminal Safety**: Binary data corrupts terminal output (garbled chars, colors, position)
- **Readability**: Text is readable, file chunks are not
- **User Experience**: Clear indication of what was received

**Example Problem Solved:**
- Before: Print 4KB of binary file data → terminal becomes unreadable, control chars run amok
- After: Just show "Received 4096 bytes" → terminal stays clean

---

### Optimization 4.4: Payload Size Validation in Receiver

**Lines:** `if (header.payloadLength > MAX_MESSAGE_SIZE)` and `if (header.payloadLength > MAX_BUFFER_SIZE)`

**What Changed:**
```cpp
if (header.payloadLength > MAX_MESSAGE_SIZE) {
    std::cerr << "\n[RECV] Invalid payload size: " << header.payloadLength << "\n";
    running = false;
    break;
}

if (header.payloadLength > MAX_BUFFER_SIZE) {
    std::cerr << "\n[RECV] Payload too large: " << header.payloadLength << " bytes\n";
    running = false;
    break;
}
```

**Why:**
- **Stack Protection**: MAX_BUFFER_SIZE check prevents stack overflow
- **Sanity Check**: MAX_MESSAGE_SIZE check catches corrupted headers
- **Graceful Failure**: Closes connection safely instead of crashing

**Example Problem Solved:**
- Before: Corrupted header claims 1GB payload → crash allocating vector
- After: Reject, close connection, continue running

---

### Optimization 4.5: Null Pointer Checks in Send/Recv

**Lines:** `sendAll()` and `recvAll()` functions

**What Changed:**
```cpp
bool sendAll(socket_t sock, const char *data, int total) {
    if (total <= 0 || !data) return false;  // NEW: Check parameters
    
    int sent = 0;
    while (sent < total) {
        int n = send(sock, data + sent, total - sent, 0);
        if (n == SOCKET_ERROR || n <= 0)
            return false;
        sent += n;
    }
    return true;
}
```

**Why:**
- **Crash Prevention**: Don't allow null pointers or invalid sizes
- **Early Detection**: Fail fast instead of mysterious crashes later
- **Parameter Validation**: Defensive programming practice

**Example Problem Solved:**
- Before: `sendAll(sock, nullptr, 100)` → crash in send() call
- After: Function returns false immediately, error handled

---

## 5. Summary of Optimization Categories

| Category | Optimizations | Benefit |
|----------|---|---|
| **Security** | Size limits, input validation, null checks | Prevent DoS, buffer overflow, crashes |
| **Reliability** | Reliable send/recv, fragmentation handling | Guaranteed message delivery |
| **Performance** | Early validation, batch operations | Reduce wasted network calls |
| **Debugging** | Login state, message IDs, detailed errors | Easier troubleshooting |
| **UX** | Local validation, file pre-check, type-aware display | Faster feedback, cleaner output |

---

## 6. Testing the Optimizations

### Test Case 1: Oversized Message
```bash
# Send message claiming 2GB payload
# Expected: Server rejects, logs "Payload exceeds max size"
# Before: Server crashes
```

### Test Case 2: Empty Topic
```bash
/subscribe ""
# Expected: Client rejects locally, "Invalid topic name"
# Before: Sent to server, wasted resources
```

### Test Case 3: 100MB File Upload
```bash
/sendfile topic /100mb_file.bin
# Expected: Client rejects, "File too large (max 10MB)"
# Before: Transfer hangs, confusion
```

### Test Case 4: Binary File Display
```bash
# Publish .jpg image file to topic
# Expected: "[FILE] Received chunk (4096 bytes)..."
# Before: Terminal corrupted with binary garbage
```

---

## 7. Performance Impact

| Operation | Before | After | Improvement |
|-----------|--------|-------|-------------|
| Oversized message handling | Crash | Graceful rejection | System stability |
| File transfer UX | Hangs | Immediate feedback | User experience |
| Terminal corruption | Yes (binary output) | No (text-only) | Readability |
| Invalid login attempts | Wasted server ops | Local rejection | Network efficiency |

---

## Conclusion

These optimizations make the system:
- **More Secure**: Prevents common attacks (DoS, buffer overflow)
- **More Reliable**: Handles network fragmentation, partial messages
- **More User-Friendly**: Local validation with clear error messages
- **More Debuggable**: Detailed logging and state tracking
- **More Efficient**: Rejects bad input early, reduces wasted operations
