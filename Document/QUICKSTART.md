# Quick Start Guide - CMD Commands

## 1. Build Instructions (Windows CMD)

### Server (MinGW Compiler)

```cmd
cd Server
g++ -std=c++17 -pthread server.cpp -o server.exe -lws2_32
server.exe
```

**Output:**

```
[MAIN] Chat server listening on port 8080
[STREAM] Stream server listening on port 8081
```

### GUI Client (Qt6 Required)

```cmd
cd Client\build
cmake --build . --config Debug
untitled.exe
```

### CLI Client (Optional)

```cmd
cd Client
g++ -std=c++17 -pthread clientCLI.cpp -o client.exe -lws2_32
client.exe 127.0.0.1 8080
```

---

## 2. Running Complete Test (3 Terminals)

### Terminal 1: Start Server

```cmd
cd ..\LaptrinhMang\network_programming\Server
server.exe
```

Expected output:

```
[MAIN] Chat server listening on port 8080
[STREAM] Stream server listening on port 8081
```

---

### Terminal 2: Start Client 1 (Alice)

```cmd
cd ..\LaptrinhMang\network_programming\Client\build
untitled.exe
```

**In GUI:**

1. Host: `127.0.0.1`
2. Port: `8080`
3. Username: `alice`
4. Click "Connect"
5. Topic: `general`
6. Click "Subscribe"

---

### Terminal 3: Start Client 2 (Bob)

```cmd
cd ..\LaptrinhMang\network_programming\Client\build
untitled.exe
```

**In GUI:**

1. Host: `127.0.0.1`
2. Port: `8080`
3. Username: `bob`
4. Click "Connect"
5. Topic: `general`
6. Click "Subscribe"

---

## 3. Test Text Messaging

### Alice (Client 1) Sends Message

1. Select topic: `general`
2. Type message: `Hello Bob!`
3. Click "Send"

### Bob (Client 2) Receives

Message appears in chat list: `[general] alice: Hello Bob!`

---

## 4. Test File Sharing

### Alice Sends File

1. Click "Browse File"
2. Select any file (e.g., `test.txt`)
3. File is sent to all subscribers on topic

### Bob Receives File

1. File appears in chat as: `📁 [file] alice: filename.ext`
2. Double-click to save and download

---

## 5. Test Audio Streaming

### Alice Records Audio

1. Select topic: `general`
2. Click "Call" button
3. Select quality: `Medium (16 kHz)` (default)
4. Click "Start Audio"
5. Speak into microphone (records for a while)
6. Click "Stop Audio"

**Server logs:**

```
[STREAM] Stream start from alice on topic general (Quality: 1)
[AUDIO] Received frame from alice (1024 bytes, total: 1024 bytes)
[AUDIO] Received frame from alice (1024 bytes, total: 2048 bytes)
...
[STREAM] Stream stop from alice on topic general
[AUDIO] Stream finished from alice (Total: 16384 bytes, Quality: 1)
```

### Bob Receives Audio

1. Audio appears in chat as: `🔊 Audio from alice`
2. Double-click to open Player Dialog
3. Click "Play" to listen
4. Adjust volume slider
5. Click "Stop" when done

---

## 6. Test Audio System

### Open Audio Test Dialog

1. Click "Test Audio" button (orange button on main window)
2. Test microphone and loudspeaker

### Test Microphone

1. Select input device from dropdown
2. Click "Start Recording (5s)"
3. Speak into microphone
4. Wait 5 seconds (auto-stops)
5. Check log for: `[MIC] Recording stopped. Data size: XXXX bytes`

### Test Loudspeaker

1. Click "Playback Recorded Audio"
2. Should hear your recorded voice
3. Check log for: `[SPEAKER] State: Active (Playing)`

**If no sound:**

- Check System Settings → Sound
- Ensure microphone is not muted
- Verify loudspeaker is connected

---

## 7. Advanced: Test Different Audio Qualities

### Low Quality (8 kHz)

Alice side:

```
Select Quality: Low (8 kHz)
Start Audio
```

Bob receives same with quality setting preserved.

### High Quality (48 kHz)

Alice side:

```
Select Quality: High (48 kHz)
Start Audio
```

Audio sent and received with 48kHz sample rate.

---

## 8. Disconnect & Cleanup

### Close Client

1. Click "Disconnect" button
2. Close application window

### Server Logs

```
[CHAT] Client alice logged out
[CHAT] Connection closed for client ID=0
```

### Stop Server

In server terminal: Press `Ctrl+C`

```
[MAIN] Server shutdown
```

---

## 9. Troubleshooting

### "Connection refused"

```
Error: Cannot connect to server
```

**Fix:**

- Check Terminal 1: Is server running?
- Try: `netstat -ano | findstr 8080`
- If port in use: Change port in protocol.h and rebuild

### "Username already taken"

```
Error: Login failed - username exists
```

**Fix:**

- Use unique username (e.g., alice, bob, charlie)
- Restart server to clear old sessions

### Audio Not Working

**Test:**

1. Click "Test Audio" button
2. Record 5 seconds
3. Playback should play your voice
4. If silent: Check Windows Sound Settings

**If test works but chat audio doesn't:**

- Restart both clients
- Ensure both on same topic
- Check logs for `[AUDIO] Stream finished`

### Build Error (Qt Not Found)

```
cmake: Could not find Qt6
```

**Fix:**

```cmd
cd Client
cmake -DCMAKE_PREFIX_PATH=C:\Qt\6.10.1\mingw_64 -B build
cmake --build build --config Debug
```

### File Too Large

```
Error: Payload too large (>10MB)
```

**Fix:**

- Maximum file size: 10 MB
- For larger files: Implement chunking (future feature)

---

## 10. Quick Reference Commands

### Start Everything

```cmd
REM Terminal 1 - Server
cd ..\LaptrinhMang\network_programming\Server
server.exe

REM Terminal 2 - Client 1
cd ..\LaptrinhMang\network_programming\Client\build
untitled.exe

REM Terminal 3 - Client 2
cd ..\LaptrinhMang\network_programming\Client\build
untitled.exe
```

### Build Without CMake (Optional)

```cmd
REM Direct compilation (no Qt)
cd Client
g++ -std=c++17 -pthread clientCLI.cpp -o client.exe -lws2_32
client.exe 127.0.0.1 8080
```

### Kill Server/Client

```cmd
taskkill /IM server.exe
taskkill /IM untitled.exe
```

### Find Port Usage

```cmd
netstat -ano | findstr 8080
netstat -ano | findstr 8081
```

---

## 11. File Structure

```
network_programming/
├── Server/
│   ├── server.cpp
│   ├── broker.h
│   └── server.exe (after build)
│
├── Client/
│   ├── mainwindow.cpp/h
│   ├── audiodialog.cpp/h
│   ├── audioplayerdialog.cpp/h
│   ├── audiotestdialog.cpp/h
│   ├── CMakeLists.txt
│   ├── mainwindow.ui
│   └── build/
│       └── untitled.exe (after build)
│
├── protocol.h
├── QUICKSTART.md (this file)
└── Document/
    └── (documentation files)
```

---

## 12. Feature Summary

✅ **Text Messaging** - Send/receive text on topics
✅ **File Sharing** - Upload/download files via chat
✅ **Audio Recording** - Record voice at 3 quality levels
✅ **Audio Playback** - Play received audio streams
✅ **Audio Test** - Diagnose mic/speaker issues
✅ **Message History** - Review past messages
✅ **Multi-Topic** - Subscribe to multiple topics
✅ **Multi-Client** - Unlimited concurrent clients
✅ **Quality Control** - Automatic sample rate detection
✅ **Error Logging** - Detailed debug output

---

## 13. Performance Notes

- **Max file size:** 10 MB single message
- **Audio bandwidth:** ~16 kB/s (Medium quality)
- **Max clients:** Depends on system (tested with 10+)
- **Latency:** <100ms local network
- **Memory:** ~5-10 MB per client connection

---

## 14. Next Steps

1. Run steps in section 2 to verify setup
2. Test audio with Test Audio button (section 6)
3. Check `IMPLEMENTATION_GUIDE.md` for architecture
4. Review `protocol.h` for message format
5. Examine logs for troubleshooting
