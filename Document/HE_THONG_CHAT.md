# Hướng dẫn Hệ thống Chat

## 📖 Giới thiệu

Hệ thống chat này được xây dựng dựa trên mô hình **Publisher/Subscriber (Pub/Sub)**, cho phép người dùng giao tiếp thông qua các **topics** (chủ đề). Đây là kiến trúc linh hoạt, giúp hệ thống dễ dàng mở rộng và quản lý nhiều cuộc hội thoại đồng thời.

## 🎯 Mục đích

- Cung cấp nền tảng chat realtime cho nhiều người dùng
- Hỗ trợ chat theo nhóm (topics) và chat riêng tư (1-1)
- Cho phép gửi tin nhắn văn bản và file
- Đảm bảo tin nhắn được gửi đến đúng người nhận

## 🏗️ Kiến trúc Hệ thống

### Mô hình Pub/Sub

```
┌─────────────┐         ┌──────────────┐         ┌─────────────┐
│  Publisher  │────────>│    Broker    │<────────│ Subscriber  │
│  (Client A) │         │   (Server)   │         │  (Client B) │
└─────────────┘         └──────────────┘         └─────────────┘
       │                       │                        │
       │                       │                        │
       └──── Publish ──────────┤                        │
                               ├──── Distribute ────────┘
```

**Vai trò các thành phần:**

- **Publisher**: Người gửi tin nhắn đến một topic cụ thể
- **Subscriber**: Người đăng ký nhận tin từ một hoặc nhiều topics
- **Broker (Server)**: Trung tâm điều phối, nhận tin từ publishers và phân phối đến subscribers

### Luồng hoạt động cơ bản

1. **Đăng nhập**: Client kết nối và xác thực với server
2. **Subscribe Topics**: Client đăng ký các topics quan tâm
3. **Publish Messages**: Client gửi tin nhắn đến topics
4. **Message Distribution**: Server phân phối tin đến tất cả subscribers của topic đó
5. **Receive Messages**: Clients nhận tin từ các topics đã subscribe

## 🔐 Quy trình Đăng nhập

### Bước 1: Kết nối đến Server

Client thiết lập kết nối TCP đến server trên port **8080** (Chat Channel).

```cpp
// Pseudocode
connect_to_server("127.0.0.1", 8080);
```

### Bước 2: Gửi MSG_LOGIN

Client gửi packet đăng nhập với thông tin:

- **msgType**: `MSG_LOGIN` (1)
- **sender**: Username muốn sử dụng (tối đa 32 ký tự)

```
Client → Server:
PacketHeader {
    msgType = MSG_LOGIN,
    sender = "alice",
    ...
}
```

### Bước 3: Nhận phản hồi

**Thành công** - Server gửi `MSG_ACK`:

```
Server → Client:
PacketHeader {
    msgType = MSG_ACK,
    ...
}
```

- Client được thêm vào danh sách người dùng
- Tự động subscribe topic cùng tên với username (personal topic)

**Thất bại** - Server gửi `MSG_ERROR`:

```
Server → Client:
PacketHeader {
    msgType = MSG_ERROR,
    payloadLength = <length>,
    ...
}
Payload: "Username already exists"
```

Lý do thất bại có thể là:

- Username đã được sử dụng
- Username không hợp lệ (rỗng hoặc chứa ký tự đặc biệt)

## 📢 Subscribe/Unsubscribe Topics

### Subscribe (Đăng ký nhận tin)

Để nhận tin từ một topic, client phải subscribe:

```
Client → Server:
PacketHeader {
    msgType = MSG_SUBSCRIBE,
    sender = "alice",
    topic = "news",
    ...
}
```

Server xác nhận:

```
Server → Client:
PacketHeader {
    msgType = MSG_ACK,
    topic = "news",
    ...
}
```

**Lưu ý quan trọng:**

- Mỗi user khi đăng nhập tự động subscribe topic cùng tên với username
- Có thể subscribe nhiều topics cùng lúc
- Để chat riêng với user "bob", subscribe topic "bob"

### Unsubscribe (Hủy đăng ký)

```
Client → Server:
PacketHeader {
    msgType = MSG_UNSUBSCRIBE,
    sender = "alice",
    topic = "news",
    ...
}
```

## 💬 Gửi và Nhận Tin nhắn

### Gửi tin nhắn văn bản

Client publish tin nhắn đến một topic:

```
Client → Server:
PacketHeader {
    msgType = MSG_PUBLISH_TEXT,
    sender = "alice",
    topic = "news",
    payloadLength = 11,
    ...
}
Payload: "Hello World"
```

### Server phân phối

Server tìm tất cả subscribers của topic "news" và gửi tin:

```
Server → Subscribers (bob, charlie, ...):
PacketHeader {
    msgType = MSG_PUBLISH_TEXT,
    sender = "alice",
    topic = "news",
    payloadLength = 11,
    ...
}
Payload: "Hello World"
```

### Nhận tin nhắn

Mỗi client lắng nghe trên socket và xử lý tin nhắn đến:

```cpp
// Pseudocode
while (connected) {
    packet = receive_packet();
    if (packet.msgType == MSG_PUBLISH_TEXT) {
        display_message(packet.sender, packet.payload);
    }
}
```

## 💌 Chat Riêng tư (1-1)

Để chat riêng với một người, sử dụng username của họ làm topic:

**Bước 1**: Subscribe topic của người đó

```
alice → Server: MSG_SUBSCRIBE (topic="bob")
```

**Bước 2**: Gửi tin nhắn đến topic đó

```
alice → Server: MSG_PUBLISH_TEXT (topic="bob", payload="Hi Bob!")
```

**Kết quả**: Chỉ user "bob" nhận được tin (vì mặc định bob đã subscribe topic "bob")

## 🔄 Quy trình Chat đầy đủ

### Ví dụ: Alice chat với Bob

```
1. Alice: Connect → Server (port 8080)
2. Alice → Server: MSG_LOGIN (sender="alice")
3. Server → Alice: MSG_ACK
   [Alice tự động subscribe topic "alice"]

4. Bob: Connect → Server (port 8080)
5. Bob → Server: MSG_LOGIN (sender="bob")
6. Server → Bob: MSG_ACK
   [Bob tự động subscribe topic "bob"]

7. Alice → Server: MSG_SUBSCRIBE (topic="bob")
8. Server → Alice: MSG_ACK

9. Alice → Server: MSG_PUBLISH_TEXT (topic="bob", payload="Hi!")
10. Server → Bob: MSG_PUBLISH_TEXT (sender="alice", topic="bob", payload="Hi!")

11. Bob → Server: MSG_SUBSCRIBE (topic="alice")
12. Bob → Server: MSG_PUBLISH_TEXT (topic="alice", payload="Hello!")
13. Server → Alice: MSG_PUBLISH_TEXT (sender="bob", topic="alice", payload="Hello!")
```

## 👥 Chat Nhóm

Chat nhóm hoạt động tương tự, nhưng nhiều người subscribe cùng một topic:

```
# Tạo nhóm "developers"
alice → Server: MSG_SUBSCRIBE (topic="developers")
bob → Server: MSG_SUBSCRIBE (topic="developers")
charlie → Server: MSG_SUBSCRIBE (topic="developers")

# Alice gửi tin đến nhóm
alice → Server: MSG_PUBLISH_TEXT (topic="developers", payload="Team meeting at 3pm")

# Server gửi đến tất cả members
Server → alice: MSG_PUBLISH_TEXT (...)
Server → bob: MSG_PUBLISH_TEXT (...)
Server → charlie: MSG_PUBLISH_TEXT (...)
```

## 🚪 Đăng xuất

```
Client → Server:
PacketHeader {
    msgType = MSG_LOGOUT,
    sender = "alice",
    ...
}

Server:
- Unsubscribe alice khỏi tất cả topics
- Xóa alice khỏi danh sách users
- Đóng kết nối

Server → Client: MSG_ACK
```

## 📊 Sơ đồ Trạng thái Client

```
       ┌──────────────┐
       │  Disconnected│
       └──────┬───────┘
              │ connect()
              ▼
       ┌──────────────┐
       │  Connected   │
       └──────┬───────┘
              │ login()
              ▼
       ┌──────────────┐
       │ Authenticated│◄──────┐
       └──────┬───────┘       │
              │                │
              ├─subscribe()────┤
              ├─publish()──────┤
              ├─unsubscribe()──┤
              │                │
              │ logout()       │
              ▼                │
       ┌──────────────┐        │
       │ Disconnected │────────┘
       └──────────────┘
```

## 🎨 Giao diện Client (Qt)

### MainWindow Components

1. **Server Connection Panel**

   - Server IP input
   - Username input
   - Connect/Disconnect button

2. **Topic Management**

   - Subscribe topic input
   - Subscribe/Unsubscribe buttons
   - List of subscribed topics

3. **Chat Panel**

   - Message display area (QTextEdit)
   - Message input field
   - Topic/Recipient selector
   - Send button

4. **File Transfer**
   - Send file button
   - File transfer progress

## 🛡️ Xử lý Lỗi

### Lỗi thường gặp

1. **Kết nối thất bại**

   - Kiểm tra server có đang chạy
   - Kiểm tra firewall/port forwarding

2. **Login thất bại**

   - Username đã tồn tại → chọn username khác
   - Username không hợp lệ → sử dụng chữ cái/số

3. **Không nhận được tin nhắn**

   - Kiểm tra đã subscribe topic chưa
   - Kiểm tra kết nối vẫn còn active

4. **Mất kết nối**
   - Tự động reconnect (nếu có)
   - Thông báo user và yêu cầu login lại

## 💡 Best Practices

### Cho Client Developers

1. **Luôn kiểm tra kết nối** trước khi gửi tin
2. **Handle timeout**: Set timeout cho recv() để tránh block vô hạn
3. **Validate input**: Kiểm tra độ dài username, topic, message
4. **Thread-safe**: Sử dụng mutex khi truy cập shared resources
5. **Graceful shutdown**: Gửi MSG_LOGOUT trước khi đóng app

### Cho Server Developers

1. **Validate mọi input** từ client
2. **Limit message size**: Tránh DoS attacks
3. **Cleanup**: Xóa disconnected clients khỏi topics
4. **Logging**: Ghi log mọi hoạt động quan trọng
5. **Error handling**: Luôn gửi MSG_ERROR khi có lỗi

## 🔧 Cấu hình

### Server Configuration

```cpp
#define CHAT_PORT 8080
#define MAX_CLIENTS 100
#define MAX_MESSAGE_SIZE (10 * 1024 * 1024)  // 10MB
#define SOCKET_TIMEOUT_MS 5000
```

### Client Configuration

```cpp
#define DEFAULT_SERVER "127.0.0.1"
#define CHAT_PORT 8080
#define RECONNECT_INTERVAL 5000  // ms
```

## 📈 Performance Tips

1. **Sử dụng thread pool** cho server để xử lý nhiều clients
2. **Buffer messages**: Gộp nhiều tin nhỏ thành batch
3. **Compression**: Nén payload cho tin nhắn lớn
4. **Caching**: Cache danh sách subscribers của topics

## 🔐 Bảo mật (Tương lai)

- End-to-end encryption cho tin nhắn riêng tư
- Authentication với password/token
- TLS/SSL cho kết nối
- Rate limiting để chống spam

## 📝 Tóm tắt

Hệ thống chat Pub/Sub cung cấp:

- ✅ Chat linh hoạt theo topics
- ✅ Chat riêng tư 1-1
- ✅ Chat nhóm nhiều người
- ✅ Dễ dàng mở rộng
- ✅ Tách biệt logic gửi/nhận
- ✅ Server có thể route tin nhắn hiệu quả

---

**Xem thêm:**

- [Giao thức chi tiết](GIAO_THUC.md)
- [Hướng dẫn gửi file](GUI_FILE.md)
