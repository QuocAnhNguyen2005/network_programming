# Tài liệu Giao thức (Protocol) - Hệ thống Pub/Sub

## Tổng quan

Hệ thống này sử dụng kiến trúc **Publisher/Subscriber (Pub/Sub)** dựa trên giao thức TCP/IP với hai loại kết nối chính:
- **Chat Channel (Cổng 8080)**: Dùng cho tin nhắn văn bản, quản lý đăng nhập/đăng xuất, và quản lý chủ đề (topics)
- **Stream Channel (Cổng 8081)**: Dùng cho truyền phát audio/stream dữ liệu thời gian thực

---

## Cấu trúc PacketHeader (Tiêu đề gói tin)

Mỗi thông điệp được gửi đi bao gồm một tiêu đề bắt buộc:

```c
struct PacketHeader {
    uint32_t msgType;              // Loại thông điệp (xem danh sách dưới)
    uint32_t payloadLength;        // Độ dài dữ liệu kèm theo (byte)
    uint32_t messageId;            // ID duy nhất của thông điệp
    uint64_t timestamp;            // Thời gian gửi
    uint8_t version;               // Phiên bản giao thức
    uint8_t flags;                 // Các cờ đặc tính
    char sender[32];               // Tên người gửi (username)
    char topic[32];                // Tên chủ đề (topic)
    uint32_t checksum;             // Mã kiểm tra CRC32
}
```

**Kích thước**: 94 byte (với #pragma pack(1) - không có padding)

---

## Các Loại Giao thức (Message Types)

### 1. **MSG_LOGIN** (Type = 1)
**Vai trò**: Xác thực và đăng nhập người dùng vào hệ thống

**Hướng**: Client → Server

**Yêu cầu**:
- `sender`: Tên đăng nhập (username) của khách hàng
- `payloadLength`: 0 (không có dữ liệu bổ sung)

**Phản hồi**:
- Nếu thành công: Server gửi `MSG_ACK`
- Nếu thất bại: Server gửi `MSG_ERROR` (tên trùng hoặc không hợp lệ)

**Ví dụ flow**:
```
Client (Alice) → Server: MSG_LOGIN (sender="alice")
Server → Client (Alice): MSG_ACK
Client (Bob) → Server: MSG_LOGIN (sender="bob")
Server → Client (Bob): MSG_ACK
```

---

### 2. **MSG_LOGOUT** (Type = 2)
**Vai trò**: Đăng xuất khỏi hệ thống

**Hướng**: Client → Server

**Yêu cầu**:
- `sender`: Tên người dùng
- `payloadLength`: 0

**Phản hồi**: Server gửi `MSG_ACK` xác nhận

**Hiệu ứng phụ**:
- Server hủy đăng ký client khỏi tất cả các topics
- Đóng kết nối với client

---

### 3. **MSG_SUBSCRIBE** (Type = 3)
**Vai trò**: Đăng ký nhận tin từ một chủ đề cụ thể

**Hướng**: Client → Server

**Yêu cầu**:
- `topic`: Tên chủ đề muốn đăng ký (ví dụ: "news", "music", hoặc tên người dùng khác)
- `sender`: Tên người dùng
- `payloadLength`: 0

**Phản hồi**: Server gửi `MSG_ACK` với field `topic` được điền

**Ví dụ**:
```
Client (Alice) → Server: MSG_SUBSCRIBE (sender="alice", topic="news")
Server → Client (Alice): MSG_ACK (topic="news")
```

**Ghi chú**: Khi đăng nhập, client tự động đăng ký topic cùng tên với username của mình (personal topic)

---

### 4. **MSG_UNSUBSCRIBE** (Type = 4)
**Vai trò**: Hủy đăng ký nhận tin từ một chủ đề

**Hướng**: Client → Server

**Yêu cầu**:
- `topic`: Tên chủ đề muốn hủy
- `sender`: Tên người dùng
- `payloadLength`: 0

**Phản hồi**: Server gửi `MSG_ACK`

**Ví dụ**:
```
Client (Alice) → Server: MSG_UNSUBSCRIBE (sender="alice", topic="news")
Server → Client (Alice): MSG_ACK
```

---

### 5. **MSG_PUBLISH_TEXT** (Type = 5)
**Vai trò**: Công bố tin nhắn văn bản đến tất cả các subscribers của một topic

**Hướng**: Client → Server → Tất cả Subscribers

**Yêu cầu**:
- `topic`: Topic đích
- `sender`: Người công bố
- `payload`: Nội dung tin nhắn (văn bản)
- `payloadLength`: Độ dài nội dung (≤ 10MB)

**Phản hồi**: Server gửi `MSG_ACK` cho người công bố

**Server sẽ**:
1. Xác thực người gửi đã đăng nhập
2. Xác thực topic không rỗng
3. Lấy danh sách tất cả subscribers của topic
4. Gửi toàn bộ packet (header + payload) tới mỗi subscriber

**Ví dụ**:
```
Client (Alice) → Server: MSG_PUBLISH_TEXT 
  sender="alice", topic="news", payload="Hello everyone!"
Server → Client (Alice): MSG_ACK
Server → Clients subscribed to "news": [Toàn bộ packet]
  sender="alice", topic="news", payload="Hello everyone!"
```

---

### 6. **MSG_PUBLISH_FILE** (Type = 6)
**Vai trò**: Công bố tệp tin đến tất cả subscribers của một topic

**Hướng**: Client → Server → Tất cả Subscribers

**Yêu cầu**:
- `topic`: Topic đích
- `sender`: Người gửi
- `payload`: Dữ liệu tệp
- `payloadLength`: Kích thước tệp

**Phản hồi**: Server gửi `MSG_ACK`

**Ghi chú**: Tương tự `MSG_PUBLISH_TEXT` nhưng dùng cho truyền tệp. Dữ liệu tệp nằm trong payload.

---

### 7. **MSG_FILE_DATA** (Type = 7)
**Vai trò**: Dữ liệu tệp con (dùng cho truyền tệp lớn chia nhỏ)

**Hướng**: Client ↔ Server

**Ghi chú**: Được dự trữ cho xử lý tệp theo từng đoạn nhỏ

---

### 8. **MSG_ERROR** (Type = 8)
**Vai trò**: Báo lỗi từ server khi có vấn đề

**Hướng**: Server → Client

**Payload**: Mô tả lỗi (text)

**Khi xảy ra**:
- Tên đăng nhập không hợp lệ hoặc trùng lặp
- Payload quá lớn (> 10MB)
- Chưa đăng nhập nhưng cố gắng thực hiện hành động khác
- Topic rỗng
- Loại thông điệp không xác định

**Ví dụ**:
```
Client → Server: MSG_LOGIN (sender="alice")
Server (lỗi: username trùng) → Client: MSG_ERROR 
  payload="Username already taken"
```

---

### 9. **MSG_ACK** (Type = 9)
**Vai trò**: Xác nhận thành công của một thao tác

**Hướng**: Server → Client

**Khi gửi**: 
- Sau khi đăng nhập thành công
- Sau khi đăng ký/hủy đăng ký topic
- Sau khi công bố thông điệp
- Sau khi đăng xuất

**Payload**: Rỗng (payloadLength = 0)

**Ghi chú**: Dùng `messageId` trong ACK để client biết ứng với request nào

---

### 10. **MSG_STREAM_START** (Type = 10)
**Vai trò**: Báo hiệu bắt đầu truyền phát audio/stream

**Hướng**: Client → Server → Subscribers

**Sử dụng trên**: Stream Channel (Cổng 8081)

**Yêu cầu**:
- `topic`: Topic của stream (ví dụ: tên phòng, tên người dùng)
- `sender`: Tên người phát
- `payloadLength`: 0

**Server sẽ**:
1. Ghi nhận session stream
2. Gửi thông báo tới tất cả subscribers
3. Chuẩn bị để nhận các frame stream

**Ví dụ**:
```
Publisher (Alice) → Server: MSG_STREAM_START 
  sender="alice", topic="room_audio"
Server → All Subscribers: MSG_STREAM_START (thông báo Alice bắt đầu phát)
```

---

### 11. **MSG_STREAM_READY** (Type = 11)
**Vai trò**: Xác nhận rằng stream đã sẵn sàng nhận dữ liệu

**Hướng**: Server → Client

**Sử dụng trên**: Stream Channel

**Ghi chú**: Báo hiệu subscriber đã sẵn sàng nhận audio frames

---

### 12. **MSG_STREAM_FRAME** (Type = 12)
**Vai trò**: Gửi một frame audio/dữ liệu trong stream

**Hướng**: Client → Server → Subscribers

**Sử dụng trên**: Stream Channel (Cổng 8081)

**Yêu cầu**:
- `topic`: Topic của stream
- `sender`: Tên người phát
- `payload`: Dữ liệu frame (audio data)
- `payloadLength`: Kích thước frame

**Server sẽ**:
1. Nhận frame từ publisher
2. Gửi lại tới tất cả subscribers của topic (trừ chính sender)
3. Xử lý nhanh để giảm độ trễ

**Ghi chú**: 
- Gửi liên tục từng frame để duy trì stream
- Mỗi frame có timestamp để sắp xếp lại nếu cần
- Quá trình này được tối ưu hóa để giảm logging spam

---

### 13. **MSG_STREAM_STOP** (Type = 13)
**Vai trộ**: Báo hiệu kết thúc truyền phát audio/stream

**Hướng**: Client → Server → Subscribers

**Sử dụng trên**: Stream Channel (Cổng 8081)

**Yêu cầu**:
- `topic`: Topic của stream
- `sender`: Tên người phát
- `payloadLength`: 0

**Server sẽ**:
1. Ghi nhận kết thúc stream
2. Gửi thông báo tới subscribers
3. Hủy session stream

**Ví dụ**:
```
Publisher (Alice) → Server: MSG_STREAM_STOP 
  sender="alice", topic="room_audio"
Server → All Subscribers: MSG_STREAM_STOP (thông báo Alice kết thúc phát)
Server: Hủy session stream
```

---

## Quy trình Giao tiếp Chính

### Quy trình Đăng nhập và Đăng ký

```
┌─────────────────────────────────────────────────────────────┐
│                      CHAT CHANNEL (Port 8080)               │
├─────────────────────────────────────────────────────────────┤
│                                                              │
│  Client (Alice)            Server                           │
│       │                       │                              │
│       ├──────MSG_LOGIN──────> │                              │
│       │  (sender="alice")     │ [Check username exists]     │
│       │                       ├─ Register client            │
│       │                       ├─ Auto-subscribe to "alice"  │
│       │ <─────MSG_ACK─────────┤                              │
│       │                       │                              │
│       ├──MSG_SUBSCRIBE───────>│                              │
│       │  (topic="news")       ├─ Add to "news" subscribers  │
│       │ <─────MSG_ACK─────────┤                              │
│       │                       │                              │
│       └───────────────────────┘                              │
│                                                              │
└─────────────────────────────────────────────────────────────┘
```

### Quy trình Công bố Tin nhắn Văn bản

```
┌─────────────────────────────────────────────────────────────────┐
│                   CHAT CHANNEL (Port 8080)                      │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  Publisher (Alice)    Server                  Subscribers       │
│       │                  │                        │             │
│       ├─MSG_PUBLISH_TEXT>│                        │             │
│       │ topic="news"     ├─ Find subscribers      │             │
│       │ "Hello all"      │   of "news"            │             │
│       │                  │                        │             │
│       │ <──MSG_ACK───────┤                        │             │
│       │                  ├─ Forward packet ──────>│ (Bob)       │
│       │                  ├─ Forward packet ──────>│ (Charlie)   │
│       │                  │                        │             │
│       │                  │                     [Receive         │
│       │                  │                      "Hello all"]    │
│       │                  │                        │             │
│       └──────────────────┘                        └─────────────┘
│                                                                  │
└─────────────────────────────────────────────────────────────────┘
```

### Quy trình Truyền phát Audio

```
┌──────────────────────────────────────────────────────────────────┐
│               STREAM CHANNEL (Port 8081)                         │
├──────────────────────────────────────────────────────────────────┤
│                                                                   │
│ Publisher (Alice)    Server              Subscribers (Bob, etc)  │
│      │                 │                       │                  │
│      ├─MSG_STREAM_START>│                       │                  │
│      │ topic="call"    ├─ Register session     │                  │
│      │ sender="alice"  ├─ Notify subscribers ─>│                  │
│      │                 │                      [Receiving...] │     │
│      │                 │                       │                  │
│      ├─MSG_STREAM_FRAME>                       │                  │
│      │ [audio frame 1] ├─ Forward to subs ────>│                  │
│      │                 │                       │                  │
│      ├─MSG_STREAM_FRAME>                       │                  │
│      │ [audio frame 2] ├─ Forward to subs ────>│                  │
│      │                 │  (suppress logging)   │                  │
│      │                 │                       │                  │
│      ├─MSG_STREAM_FRAME>                       │                  │
│      │ [audio frame N] ├─ Forward to subs ────>│                  │
│      │                 │                       │                  │
│      ├─MSG_STREAM_STOP>│                       │                  │
│      │                 ├─ Unregister session   │                  │
│      │                 ├─ Notify end ────────> │                  │
│      │                 │                      [Stream ended] │    │
│      │                 │                       │                  │
│      └─────────────────┘                       └──────────────────┘
│                                                                   │
└──────────────────────────────────────────────────────────────────┘
```

---

## Quy tắc An toàn và Xác thực

1. **Xác thực Đăng nhập**:
   - Username phải có độ dài 1-31 ký tự
   - Tên phải duy nhất (không trùng với user khác đang online)
   - Nếu lỗi → Server gửi `MSG_ERROR`

2. **Kiểm tra Trạng thái**:
   - Mỗi thao tác (except MSG_LOGIN) cần client phải đã đăng nhập
   - Nếu chưa đăng nhập → Server gửi `MSG_ERROR` "Not logged in"

3. **Xác thực Topic**:
   - Topic không được rỗng
   - Độ dài topic tối đa 31 ký tự

4. **Hạn chế Kích thước**:
   - Payload tối đa: 10MB (MAX_MESSAGE_SIZE)
   - Buffer tối đa: 4KB (MAX_BUFFER_SIZE)
   - Nếu vượt quá → Server từ chối và gửi `MSG_ERROR`

5. **Xác thực CRC32**:
   - Trường `checksum` dùng để kiểm tra tính toàn vẹn dữ liệu
   - Server có thể kiểm tra CRC để phát hiện dữ liệu bị hỏng

---

## Thread Safety (An toàn Luồng)

Server sử dụng **mutex locks** để bảo vệ:

1. **clientsMutex**: Bảo vệ danh sách clients
   - Dùng khi register/unregister client
   - Dùng khi lấy thông tin client

2. **topicsMutex**: Bảo vệ bản đồ topics
   - Dùng khi subscribe/unsubscribe
   - Dùng khi publish

3. **streamMutex**: Bảo vệ stream sessions
   - Dùng khi register/unregister stream session

**Nguyên tắc**:
- Lock được giữ **càng ngắn càng tốt**
- Tránh gửi socket data khi đang giữ lock
- Lấy danh sách subscribers dưới lock, rồi thả lock trước khi gửi

---

## Tóm tắt Vai trò của Các Giao thức

| Giao thức | Port | Hướng | Vai trò |
|-----------|------|-------|--------|
| MSG_LOGIN | 8080 | C→S | Xác thực người dùng |
| MSG_LOGOUT | 8080 | C→S | Thoát khỏi hệ thống |
| MSG_SUBSCRIBE | 8080 | C→S | Đăng ký topic |
| MSG_UNSUBSCRIBE | 8080 | C→S | Hủy đăng ký topic |
| MSG_PUBLISH_TEXT | 8080 | C→S→Subs | Công bố tin nhắn văn bản |
| MSG_PUBLISH_FILE | 8080 | C→S→Subs | Công bố tệp tin |
| MSG_FILE_DATA | 8080 | C↔S | Dữ liệu tệp (dự trữ) |
| MSG_ERROR | 8080/8081 | S→C | Báo lỗi |
| MSG_ACK | 8080/8081 | S→C | Xác nhận thành công |
| MSG_STREAM_START | 8081 | C→S→Subs | Bắt đầu stream audio |
| MSG_STREAM_READY | 8081 | S→C | Sẵn sàng nhận stream |
| MSG_STREAM_FRAME | 8081 | C→S→Subs | Gửi frame audio |
| MSG_STREAM_STOP | 8081 | C→S→Subs | Kết thúc stream |

---

## Ghi chú Quan trọng

1. **Độ trễ thấp (Low Latency)**:
   - Stream frames không ghi log chi tiết
   - Xử lý frame nhanh chóng để giảm độ trễ âm thanh

2. **Độ tin cậy (Reliability)**:
   - Sử dụng `recvAllBytes()` và `sendAllBytes()` để đảm bảo toàn bộ dữ liệu được nhận/gửi
   - Không phải xử lý các gói bị mất (TCP đảm bảo)

3. **Mở rộng (Scalability)**:
   - Mỗi client dùng một thread riêng
   - Broker quản lý tập trung tất cả subscriptions
   - Dễ dàng thêm streams hoặc topics mới

4. **Bảo mật**:
   - Username-based authentication (cơ bản)
   - Kiểm tra xác thực cho mỗi thao tác
   - Kiểm tra kích thước payload để phòng DoS

---

**Phiên bản**: 1.0  
**Ngày cập nhật**: 2025
