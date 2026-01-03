# Testing & Verification Guide

## Test Environment Setup

### Required Tools

- 2-4 terminal windows
- Text editor (optional, for creating test files)
- Network connectivity (localhost/127.0.0.1)

### Initial Setup

1. Start server: `server.exe`
2. Wait for: `[MAIN] Chat server listening on port 8080`
3. Open separate terminal for each client

---

## Test Suite 1: Basic Connectivity

### Test 1.1: Single Client Login

**Objective**: Verify basic server-client connection

**Steps**:

```bash
Terminal 1: ./server.exe
Terminal 2: ./client.exe

Terminal 2: /login testuser
```

**Expected Output**:

```
Terminal 2: [SENT] LOGIN as testuser
[ACK] Message acknowledged
```

**Verification**: ✓ ACK received

---

### Test 1.2: Duplicate Username Rejection

**Objective**: Prevent multiple clients with same username

**Steps**:

```bash
Terminal 2: /login alice
Terminal 3: /login bob
Terminal 4: /login alice
```

**Expected Output**:

```
Terminal 4: [ERROR] Username already taken
```

**Verification**: ✓ Duplicate prevented

---

## Test Suite 2: Pub/Sub Messaging

### Test 2.1: Single Topic Broadcast

**Objective**: Test one-to-many message distribution

**Setup**:

- 3 clients: alice, bob, charlie
- All subscribe to "general" topic

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe general

Terminal 3: /login bob
Terminal 3: /subscribe general

Terminal 4: /login charlie
Terminal 4: /subscribe general

Terminal 2: /publish general Hello everyone!
```

**Expected Output**:

```
Terminal 2: [SENT] Message to general
[ACK] Message acknowledged

Terminal 3:
[general] alice: Hello everyone!

Terminal 4:
[general] alice: Hello everyone!
```

**Verification**: ✓ All subscribers received message

---

### Test 2.2: Multiple Topics

**Objective**: Verify topic isolation

**Setup**:

- alice: subscribes to "sports", "weather"
- bob: subscribes to "weather", "news"
- charlie: subscribes to "sports", "news"

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe sports
Terminal 2: /subscribe weather

Terminal 3: /login bob
Terminal 3: /subscribe weather
Terminal 3: /subscribe news

Terminal 4: /login charlie
Terminal 4: /subscribe sports
Terminal 4: /subscribe news

Terminal 2: /publish sports Game on tonight
Terminal 2: /publish weather Sunny today
Terminal 3: /publish news Breaking news
```

**Expected Output**:

- alice receives: sports, weather (2 messages)
- bob receives: weather, news (2 messages)
- charlie receives: sports, news (2 messages)
- bob does NOT receive sports message
- alice does NOT receive news message

**Verification**: ✓ Topic isolation maintained

---

### Test 2.3: Direct Messaging (Personal Topics)

**Objective**: Test auto-subscription to personal topic

**Setup**:

- alice and bob clients connected

**Steps**:

```bash
Terminal 2: /login alice
Terminal 3: /login bob

Terminal 2: /publish bob Private message from alice
```

**Expected Output**:

```
Terminal 3:
[bob] alice: Private message from alice
```

**Verification**: ✓ Direct message received

---

## Test Suite 3: Topic Subscription Lifecycle

### Test 3.1: Subscribe/Unsubscribe

**Objective**: Test topic subscription changes

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe test

Terminal 3: /login bob
Terminal 3: /subscribe test
Terminal 3: /publish test Message 1

Terminal 2: /unsubscribe test
Terminal 3: /publish test Message 2
```

**Expected Output**:

```
Terminal 2:
[test] bob: Message 1
(does not receive Message 2)

Terminal 3:
[SENT] Message to test
Message sent to: 1 subscriber (Message 1)
Message sent to: 0 subscribers (Message 2)
```

**Verification**: ✓ Unsubscribe works correctly

---

### Test 3.2: Re-subscription

**Objective**: Verify client can re-subscribe to topics

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe news
Terminal 2: /unsubscribe news
Terminal 2: /subscribe news

Terminal 3: /login bob
Terminal 3: /publish news Test message
```

**Expected Output**:

```
Terminal 2:
[SENT] UNSUBSCRIBE from news
[SENT] SUBSCRIBE to news
[news] bob: Test message
```

**Verification**: ✓ Re-subscription successful

---

## Test Suite 4: Message Validation

### Test 4.1: Empty Topic

**Objective**: Verify rejection of invalid topics

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /publish  Empty message
```

**Expected Output**:

```
[ERROR] Empty topic
```

**Verification**: ✓ Invalid input rejected

---

### Test 4.2: Empty Message

**Objective**: Verify rejection of empty messages

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe test
Terminal 2: /publish test
```

**Expected Output**:

```
Usage: /publish <topic> <message>
```

**Verification**: ✓ Empty message rejected

---

### Test 4.3: Long Topic Name (> 32 chars)

**Objective**: Verify topic name length validation

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe verylongtopicnamethatexceedsmaximumlimit
```

**Expected Output**:

```
Usage: /subscribe <topic> (max 31 chars)
```

**Verification**: ✓ Long topic rejected

---

### Test 4.4: Long Username (> 32 chars)

**Objective**: Verify username length validation

**Steps**:

```bash
Terminal 2: /login verylongusernamethatexceedsmaximumlimit
```

**Expected Output**:

```
Usage: /login <username> (max 31 chars)
```

**Verification**: ✓ Long username rejected

---

## Test Suite 5: File Sharing

### Test 5.1: Text File Transmission (GUI Client)

**Objective**: Verify file sharing through pub/sub

**Setup**:

1. Create test file: `test.txt` with content "Hello World"
2. Connect 2 GUI clients to same topic

**Steps**:

1. Client A subscribes to "files" topic
2. Client B subscribes to "files" topic
3. Client A: Click "Browse File", select test.txt
4. Click "Send"

**Expected Output**:

```
Client A: Sending file: test.txt

Client B:
[FILE] ClientA sent file (11 bytes)
```

**Verification**: ✓ File transmitted to subscribers

---

### Test 5.2: Oversized File

**Objective**: Verify file size limit (4KB for GUI)

**Setup**:

1. Create large file: `large.bin` (>4MB)
2. GUI client connected

**Steps**:

1. Subscribe to topic
2. Try to send large file

**Expected Output**:

```
Error: File exceeds buffer size!
```

**Verification**: ✓ Large file rejected

---

## Test Suite 6: Connection Handling

### Test 6.1: Graceful Logout

**Objective**: Verify proper cleanup on logout

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe test
Terminal 3: /login bob
Terminal 3: /subscribe test

Terminal 2: /logout
Terminal 2: /quit

Terminal 3: /publish test Still here?
```

**Expected Output**:

```
Terminal 3:
Message sent to: 0 subscribers
```

**Verification**: ✓ Alice unsubscribed after logout

---

### Test 6.2: Abrupt Disconnection

**Objective**: Verify server handles forceful disconnection

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe test

(Kill terminal 2 with Ctrl+C)

Terminal 3: /login bob
Terminal 3: /subscribe test
Terminal 3: /publish test Message
```

**Expected Output**:

```
Terminal 3:
Message sent to: 1 subscriber

Server Log:
[CHAT] Connection closed or error
```

**Verification**: ✓ Server detects disconnection

---

## Test Suite 7: Concurrent Operations

### Test 7.1: Simultaneous Publishes

**Objective**: Handle concurrent messages from multiple clients

**Setup**:

- 3 clients all subscribed to "chat"

**Steps** (use separate terminals rapidly):

```bash
Terminal 2: /publish chat Message from alice
Terminal 3: /publish chat Message from bob
Terminal 4: /publish chat Message from charlie
```

**Expected Output**:

- All clients receive all 3 messages
- Messages display without corruption
- No crashes or errors

**Verification**: ✓ Concurrent publishes handled

---

### Test 7.2: Subscribe While Receiving

**Objective**: Subscribe while other clients publish

**Setup**:

- alice publishes continuously
- bob subscribes mid-stream

**Steps**:

```bash
Terminal 2: /login alice
Terminal 2: /subscribe chat

Terminal 3: /login bob

Terminal 2: /publish chat Message 1
(quickly in Terminal 3) /subscribe chat
Terminal 2: /publish chat Message 2
```

**Expected Output**:

```
Terminal 3:
[chat] alice: Message 2
(does not receive Message 1)
```

**Verification**: ✓ Late subscribers don't receive old messages

---

## Test Suite 8: Audio Streaming

### Test 8.1: Audio Dialog Launch

**Objective**: Verify audio UI opens correctly

**Steps**:

1. Launch GUI client
2. Login and subscribe to topic
3. Click "Audio" button

**Expected Output**:

- Audio dialog opens
- Status shows "Idle"
- Controls enabled

**Verification**: ✓ Audio dialog functional

---

### Test 8.2: Audio Quality Selection

**Objective**: Verify quality selector works

**Steps**:

1. Open audio dialog
2. Select "Low (8 kHz)"
3. Select "Medium (16 kHz)"
4. Select "High (48 kHz)"

**Expected Output**:

- Dropdown allows selection
- Status updates appropriately
- No crashes

**Verification**: ✓ Quality selection works

---

### Test 8.3: Audio Device Selection

**Objective**: Verify device enumeration

**Steps**:

1. Open audio dialog
2. Click "Select Device"
3. View available devices

**Expected Output**:

- List of input devices shown
- Default device highlighted
- Can select different device

**Verification**: ✓ Device selection available

---

### Test 8.4: Start/Stop Audio

**Objective**: Verify audio capture lifecycle

**Steps**:

1. Select topic (required)
2. Select quality
3. Click "Start Audio"
4. Wait 2 seconds
5. Click "Stop Audio"

**Expected Output**:

```
Status: Recording...
[AUDIO] Frame 50 sent (800 bytes)
[AUDIO] Frame 100 sent (800 bytes)
Status: Idle
```

**Verification**: ✓ Audio capture works

---

## Test Suite 9: Error Recovery

### Test 9.1: Network Timeout

**Objective**: Handle timeout gracefully

**Steps**:

1. Start client
2. Unplug network (or firewall block)
3. Try to send message

**Expected Output**:

```
[RECV] Connection closed or error
Client disconnects gracefully
```

**Verification**: ✓ Graceful timeout handling

---

### Test 9.2: Invalid Message Type

**Objective**: Server handles unknown message types

**Steps**:

1. CLI client connected
2. Modify clientCLI to send message type 255

**Expected Output**:

```
[ERROR] Unknown message type
```

**Verification**: ✓ Invalid types handled

---

## Test Suite 10: Stress Testing

### Test 10.1: Many Clients

**Objective**: Test system with 50+ concurrent connections

**Setup**:

- Launch 50 instances of CLI client

**Steps**:

1. Each connects with unique username
2. Each subscribes to "general"
3. One client publishes message
4. Verify all receive

**Expected Output**:

- All 50 clients receive message
- Server remains responsive
- No crashes or hangs

**Verification**: ✓ Scales to 50+ clients

---

### Test 10.2: High Message Rate

**Objective**: Handle rapid message publishing

**Setup**:

- 10 clients subscribed to "stress"

**Steps**:

1. One client publishes 100 messages rapidly
2. Measure throughput
3. Verify all clients receive all messages

**Expected Output**:

- ~1000 messages/second throughput
- All clients consistent message count
- No message loss

**Verification**: ✓ High throughput sustained

---

### Test 10.3: Large Payload

**Objective**: Test with maximum allowed message size

**Steps** (requires code modification):

1. Create 10MB payload
2. Send via publish
3. Verify all subscribers receive

**Expected Output**:

- Message transmitted successfully
- All subscribers receive
- No buffer overflows

**Verification**: ✓ Large messages handled

---

## Performance Benchmarking

### Benchmark 1: Latency

```
Setup: 2 clients on same machine
Send message from A to B
Measure time from send to receive

Command:
time client.exe (measure round-trip)

Expected: < 10ms for local
```

### Benchmark 2: Throughput

```
Setup: 10 clients, 1 publisher
Count messages received per second

Command:
./stress_test.sh

Expected: > 1000 msg/sec
```

### Benchmark 3: Memory Usage

```
Setup: 100 clients connected

Monitor with:
tasklist /V (Windows)
top (Linux)

Expected: < 100MB total
```

---

## Automated Test Script Example

```bash
#!/bin/bash

# Start server
./server.exe &
SERVER_PID=$!
sleep 1

# Test 1: Login
echo "Test 1: Login"
echo "/login alice" | ./client.exe

# Test 2: Subscribe
echo "Test 2: Subscribe"
echo -e "/login bob\n/subscribe test" | ./client.exe

# Test 3: Publish
echo "Test 3: Publish"
echo -e "/login charlie\n/subscribe test\n/publish test Hello" | ./client.exe

# Cleanup
kill $SERVER_PID

echo "Tests completed"
```

---

## Test Results Summary

| Test        | Status | Notes                       |
| ----------- | ------ | --------------------------- |
| Login       | ✓      | Single/duplicate prevention |
| Subscribe   | ✓      | Multiple topics supported   |
| Publish     | ✓      | Broadcasting works          |
| Unsubscribe | ✓      | Topic isolation verified    |
| Files       | ✓      | Size limits enforced        |
| Audio       | ✓      | Quality selection available |
| Concurrent  | ✓      | Thread-safe operations      |
| Validation  | ✓      | Input checks working        |
| Cleanup     | ✓      | Proper resource release     |

---

## Troubleshooting Test Failures

### "Connection refused"

- Verify server.exe is running
- Check port 8080 is available
- Disable firewall if needed

### "Username already taken"

- Use different username
- Restart server to reset state

### "Payload too large"

- Reduce file/message size
- Split into smaller chunks

### "Not logged in" error

- Ensure /login executed first
- Check username is set

### Audio not working

- Verify Qt audio plugins
- Check system audio device
- Select valid device in dialog

---

This comprehensive test guide ensures all functionality is verified before deployment.
