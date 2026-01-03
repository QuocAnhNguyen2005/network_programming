# Quick Start Guide

## 1. Build Instructions

### Server (Windows with MinGW)

```bash
cd Server
g++ -std=c++17 -pthread server.cpp -o server.exe -lws2_32
./server.exe
```

### CLI Client (Windows with MinGW)

```bash
cd Client
g++ -std=c++17 -pthread clientCLI.cpp -o client.exe -lws2_32
./client.exe 127.0.0.1 8080
```

### GUI Client (requires Qt6)

```bash
cd Client
qmake -project -o Client.pro
qmake
mingw32-make
./Client.exe
```

## 2. Running a Simple Test

### Terminal 1 - Start Server

```bash
.\Server\server.exe
```

Output: `[MAIN] Chat server listening on port 8080`

### Terminal 2 - Start Client 1

```bash
.\Client\client.exe
> /login alice
> /subscribe general
> /subscribe alice
```

### Terminal 3 - Start Client 2

```bash
.\Client\client.exe
> /login bob
> /subscribe general
> /subscribe bob
```

### Terminal 2 - Send Message

```bash
> /publish general Hello from Alice!
```

### Terminal 3 - Should Receive

```
[general] alice: Hello from Alice!
```

## 3. Direct Messaging

### Send Direct Message from Client 1

```bash
> /publish bob Hello Bob, this is a private message
```

### Client 2 Receives

```
[bob] alice: Hello Bob, this is a private message
```

## 4. GUI Client Usage

1. Enter connection details:

   - Host: 127.0.0.1
   - Port: 8080
   - Username: (unique name)

2. Click "Connect"

3. Subscribe to topics:

   - Enter topic name in "Topic" field
   - Click "Subscribe"

4. Send messages:

   - Select topic from dropdown
   - Type message
   - Click "Send"

5. Send files:

   - Click "Browse File"
   - Select file to send
   - File sent to all subscribers

6. Audio streaming:
   - Click "Audio" button
   - Select audio quality
   - Click "Start Audio"
   - Audio transmitted to subscribers

## 5. Audio Streaming Setup

The audio system uses separate stream connection (port 8081).

### Requirements

- Qt6 with audio support
- System audio input device
- Network connectivity between clients

### Quality Settings

| Setting | Sample Rate | Bandwidth |
| ------- | ----------- | --------- |
| Low     | 8 kHz       | ~8 kB/s   |
| Medium  | 16 kHz      | ~16 kB/s  |
| High    | 48 kHz      | ~48 kB/s  |

## 6. Message Types Reference

| Command      | Protocol Type    | Description                |
| ------------ | ---------------- | -------------------------- |
| /login       | MSG_LOGIN        | Authenticate with username |
| /subscribe   | MSG_SUBSCRIBE    | Join topic                 |
| /unsubscribe | MSG_UNSUBSCRIBE  | Leave topic                |
| /publish     | MSG_PUBLISH_TEXT | Send text message          |
| File Send    | MSG_PUBLISH_FILE | Send file                  |
| Audio Start  | MSG_STREAM_START | Begin audio                |
| Audio Frame  | MSG_STREAM_FRAME | Send audio data            |
| Audio Stop   | MSG_STREAM_STOP  | End audio                  |

## 7. Troubleshooting

### "Connection refused"

- Verify server is running
- Check firewall settings
- Verify port 8080 is not in use

### "Username already taken"

- Each client needs unique username
- Wait for previous session to timeout (server doesn't auto-cleanup)
- Restart server if stuck

### Audio not working

- Check system audio input device
- Verify stream port 8081 is available
- Check Qt audio plugins are installed

### "Payload too large"

- Maximum single message: 10 MB
- GUI file size limit: 4 KB buffer
- For larger files, implement chunking

## 8. Recommended Test Cases

### Test 1: Multi-Topic Subscription

```
Client 1:
  /login alice
  /subscribe news
  /subscribe weather
  /subscribe sports

Client 2:
  /login bob
  /subscribe news
  /subscribe weather

Client 1:
  /publish news Breaking news item
  /publish weather Rainy today
  /publish sports Team wins!

Expected: Bob receives news and weather, not sports
```

### Test 2: Broadcast Message

```
Client 1:
  /login alice
  /subscribe broadcast

Client 2:
  /login bob
  /subscribe broadcast

Client 3:
  /login charlie
  /subscribe broadcast

Client 1:
  /publish broadcast Broadcast to all!

Expected: Bob and Charlie both receive
```

### Test 3: Topic Cleanup

```
Client 1:
  /login alice
  /subscribe test
  /unsubscribe test
  /subscribe test  (re-subscribe)

Expected: No errors, re-subscription succeeds
```

## 9. Monitoring Server

Server outputs connection information:

```
[CHAT] Client handler started for ID=0
[CHAT] Client 0 logged in as: alice
[CHAT] Client alice subscribed to: general
[CHAT] Published to 2 subscribers on topic: general
```

## 10. Common Issues & Solutions

### Issue: Payload validation errors

**Solution**: Ensure message size < 10 MB, topic < 32 chars, username < 32 chars

### Issue: Socket timeout

**Solution**: Set socket timeout to 5 seconds (configurable in protocol.h)

### Issue: Thread deadlocks

**Solution**: Server uses separate threads per client with proper mutex locking

### Issue: Memory leaks

**Solution**: Always call /logout before /quit to properly cleanup

## Performance Tips

1. **High message volume**: Use batching to reduce overhead
2. **Large files**: Chunk into smaller packets (not implemented in current version)
3. **Many subscribers**: Index subscriptions by topic for faster lookup
4. **Audio quality**: Start with Medium quality, upgrade if network allows

## Next Steps

1. Review IMPLEMENTATION_GUIDE.md for detailed architecture
2. Examine protocol.h for message format details
3. Read server.cpp for broker implementation
4. Study clientCLI.cpp for basic client pattern
5. Explore mainwindow.cpp/audiodialog.cpp for Qt integration
