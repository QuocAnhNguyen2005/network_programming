# 📋 MỔRA TẬT GỬI THỨC MESSAGING CỦA PROJECT

## Tổng Quan

Giao thức này được thiết kế cho hệ thống **Publish-Subscribe (Pub/Sub) Messaging System** - một kiến trúc cho phép các client trao đổi tin nhắn thông qua một message broker trung tâm. Giao thức sử dụng kết nối **TCP** để đảm bảo độ tin cậy trong truyền tin.

---

## 1. CẤU TRÚC GIAO THỨC

### 1.1 Các Hằng Số Cơ Bản

```c
DEFAULT_PORT = 8080              // Cổng mặc định để server lắng nghe
MAX_BUFFER_SIZE = 4096           // Kích thước buffer tối đa (4 KB)
MAX_TOPIC_LEN = 32               // Chiều dài tối đa của tên topic (32 ký tự)
MAX_USERNAME_LEN = 32            // Chiều dài tối đa của tên người dùng (32 ký tự)
SOCKET_TIMEOUT_MS = 5000         // Timeout socket: 5 giây
MAX_MESSAGE_SIZE = 10MB          // Kích thước tin nhắn tối đa: 10 MB
```

### 1.2 Cấu Trúc Packet Header

Mỗi tin nhắn được truyền đi gồm 2 phần: **Header** (bắt buộc) + **Payload** (nếu có)

**PacketHeader - 128 bytes (có padding = 128 bytes)**

| Trường | Loại | Kích Thước | Mô Tả |
|--------|------|-----------|-------|
| `msgType` | uint32_t | 4 bytes | Loại tin nhắn (xem enum MessageType) |
| `payloadLength` | uint32_t | 4 bytes | Độ dài dữ liệu theo sau (0 nếu không có) |
| `messageId` | uint32_t | 4 bytes | ID duy nhất để theo dõi tin nhắn |
| `timestamp` | uint64_t | 8 bytes | Thời gian gửi tin nhắn (Unix timestamp) |
| `version` | uint8_t | 1 byte | Phiên bản giao thức (hiện tại = 1) |
| `flags` | uint8_t | 1 byte | Cờ thuộc tính: bit0=compressed, bit1=encrypted, ... |
| `sender[MAX_USERNAME_LEN]` | char[] | 32 bytes | Tên người gửi tin nhắn |
| `topic[MAX_TOPIC_LEN]` | char[] | 32 bytes | Tên chủ đề/kênh (nếu có) |
| `checksum` | uint32_t | 4 bytes | CRC32 kiểm tra tính toàn vẹn dữ liệu |

**Tổng cộng:** 32 + 4 = 128 bytes

---

## 2. CÁC LOẠI TIN NHẮN (MessageType)

Giao thức định nghĩa **9 loại tin nhắn** chính:

### 2.1 Xác Thực (Authentication)

#### **MSG_LOGIN = 1**
- **Mục đích:** Client đăng nhập vào server
- **Payload:** `username:password` (chuỗi)
- **Phản hồi:** `MSG_ACK` (thành công) hoặc `MSG_ERROR` (thất bại)
- **Luồng:**
  1. Client → Server: Gửi LOGIN với username/password
  2. Server: Xác thực thông tin
  3. Server → Client: Gửi ACK (đăng nhập thành công) hoặc ERROR (sai mật khẩu/trùng tên)

#### **MSG_LOGOUT = 2**
- **Mục đích:** Client ngắt kết nối khỏi server
- **Payload:** Trống (rỗng)
- **Phản hồi:** `MSG_ACK`
- **Tác dụng:** Server gỡ client khỏi danh sách và hủy các subscription

---

### 2.2 Subscription Management (Quản Lý Đăng Ký)

#### **MSG_SUBSCRIBE = 3**
- **Mục đích:** Client đăng ký lắng nghe một topic nào đó
- **Payload:** `topic_name` (tên chủ đề)
- **Phản hồi:** `MSG_ACK` hoặc `MSG_ERROR`
- **Ví dụ:** Client muốn nhận tin từ topic `news` → gửi SUBSCRIBE với topic=`news`
- **Quyền lợi:** Nhận tất cả tin nhắn được publish vào topic này

#### **MSG_UNSUBSCRIBE = 4**
- **Mục đích:** Client hủy đăng ký một topic
- **Payload:** `topic_name` (tên chủ đề)
- **Phản hồi:** `MSG_ACK`
- **Tác dụng:** Client sẽ không còn nhận tin từ topic này

---

### 2.3 Phát Hành Tin Nhắn (Publishing)

#### **MSG_PUBLISH_TEXT = 5**
- **Mục đích:** Client gửi tin nhắn văn bản vào một topic
- **Payload:** Nội dung tin nhắn (chuỗi văn bản)
- **Phản hồi:** `MSG_ACK` (server xác nhận đã lưu/phân phối)
- **Ví dụ:** Publish "Hello everyone!" vào topic `chat` → tất cả subscriber sẽ nhận được

#### **MSG_PUBLISH_FILE = 6**
- **Mục đích:** Bắt đầu gửi một file lớn
- **Payload:** Metadata của file (tên, kích thước, hash)
- **Tiếp theo:** Dùng `MSG_FILE_DATA` để gửi từng chunk
- **Phản hồi:** `MSG_ACK`
- **Cơ chế:**
  1. Client gửi MSG_PUBLISH_FILE với thông tin file
  2. Server trả MSG_ACK
  3. Client gửi MSG_FILE_DATA nhiều lần (chia nhỏ file)
  4. Mỗi chunk server trả ACK để xác nhận

#### **MSG_FILE_DATA = 7**
- **Mục đích:** Gửi dữ liệu file theo từng chunk/phần
- **Payload:** Binary data (tối đa MAX_MESSAGE_SIZE = 10MB)
- **Phản hồi:** `MSG_ACK`
- **Ghi chú:** Phải gửi sau MSG_PUBLISH_FILE

---

### 2.4 Xử Lý Lỗi & Xác Nhận

#### **MSG_ERROR = 8**
- **Mục đích:** Báo lỗi từ server hoặc client
- **Payload:** Mô tả lỗi (chuỗi)
- **Ví dụ:**
  - `"Username already exists"` - Tên người dùng trùng
  - `"Wrong password"` - Mật khẩu sai
  - `"Topic not found"` - Chủ đề không tồn tại
  - `"Invalid message format"` - Định dạng tin nhắn sai
  - `"NACK"` - Không Acknowledge

#### **MSG_ACK = 9**
- **Mục đích:** Xác nhận thành công một yêu cầu
- **Payload:** Có thể chứa thêm thông tin (tuỳ loại ACK)
- **Ý nghĩa:** "Server đã nhận và xử lý yêu cầu của bạn"

---

## 3. QUI TRÌNH TRUYỀN THÔNG (Communication Workflow)

### 3.1 Qui Trình Cơ Bản

```
┌─────────────────────────────────────────────────────────────┐
│                   CLIENT                                    │
└────────────────────────┬────────────────────────────────────┘
                         │
                         ├─── Kết nối TCP
                         │
                   ┌─────▼────────┐
                   │   Đăng Nhập   │ (MSG_LOGIN)
                   └─────┬────────┘
                         │
                   ┌─────▼────────────┐
                   │ Đăng Ký Topic(s) │ (MSG_SUBSCRIBE)
                   └─────┬───────────┘
                         │
        ┌────────────────┬─────────────────┐
        │                │                 │
   ┌────▼──────┐  ┌─────▼─────┐  ┌──────▼──────┐
   │ Publish    │  │ Subscribe │  │ Receive     │
   │ (MSG_PUB*) │  │ (MSG_SUB) │  │ (Incoming)  │
   └────┬──────┘  └─────┬─────┘  └──────┬──────┘
        │                │                │
        └────────────────┬────────────────┘
                         │
                   ┌─────▼──────────┐
                   │ Đăng Xuất       │ (MSG_LOGOUT)
                   └─────┬──────────┘
                         │
                  Đóng kết nối TCP
```

### 3.2 Ví Dụ Chi Tiết: Login

```
Thời gian: 1ms  │ Client                      │ Server
────────────────┼─────────────────────────────┼────────────────────
                │ Kết nối TCP đến 8080        │
         1ms    │ ──────────────────────────► │
                │ Gửi MSG_LOGIN               │
         2ms    │ [Header + "alice:pass123"]  │
                │ ──────────────────────────► │
         3ms    │                             │ Kiểm tra user/pass
         4ms    │ ◄────────────────────────── │ Gửi MSG_ACK
                │ Nhận MSG_ACK                │
         5ms    │ (Đăng nhập thành công)      │
```

### 3.3 Ví Dụ Chi Tiết: Publish & Subscribe

```
Timeline:

[Alice đăng nhập và SUBSCRIBE vào topic "news"]
Alice: MSG_LOGIN (username="alice") ───────────► Server
Alice: ◄────────────── MSG_ACK
Alice: MSG_SUBSCRIBE (topic="news") ───────────► Server
Alice: ◄────────────── MSG_ACK

[Bob đăng nhập và PUBLISH vào topic "news"]
Bob: MSG_LOGIN (username="bob") ───────────────► Server
Bob: ◄────────────── MSG_ACK
Bob: MSG_PUBLISH_TEXT (topic="news", payload="Breaking news!") ──► Server
Bob: ◄────────────── MSG_ACK

[Server phân phối tin cho Alice]
Server: ◄───────── Gửi "Breaking news!" cho Alice
Alice:  Nhận tin: "bob said: Breaking news!"
```

---

## 4. FLOW CONTROL & RELIABILITY (Điều Khiển & Độ Tin Cậy)

### 4.1 Cơ Chế ACK (Xác Nhận)

Mỗi yêu cầu từ client **luôn phải nhận MSG_ACK hoặc MSG_ERROR** từ server:

```
Client                          Server
  │                               │
  ├─── MSG_LOGIN ──────────────────►
  │                               │
  │                     (Xử lý)    │
  │                               │
  │◄───────── MSG_ACK/MSG_ERROR ──┤
  │                               │
```

**Tác dụng:** Đảm bảo không mất tin nhắn quan trọng

### 4.2 Checksum (CRC32)

Mỗi packet header chứa `checksum` field để kiểm tra lỗi truyền dữ liệu:

```
Sender:
  - Tính CRC32 của toàn bộ header + payload
  - Ghi vào field checksum
  
Receiver:
  - Nhận packet
  - Tính lại CRC32
  - So sánh: nếu khác → bỏ qua hoặc yêu cầu gửi lại
```

### 4.3 Message ID & Timestamp

- **messageId:** Dùng để track tin nhắn, tránh duplicate
- **timestamp:** Ghi lại thời gian gửi, hữu ích cho logging và debugging

---

## 5. VÀI TRÒ CỦA CÁC THÀNH PHẦN GIAO THỨC

### 5.1 PacketHeader - "Bì Thư"

**Mô tả:** Header là phần đầu của mỗi tin nhắn, chứa thông tin meta về tin nhắn

**Vai trò:**
- ✅ **Định danh loại tin:** msgType cho biết đây là LOGIN, PUBLISH, SUBSCRIBE, ...
- ✅ **Chỉ dẫn dữ liệu:** payloadLength cho server biết cần đọc bao nhiêu bytes payload
- ✅ **Theo dõi:** messageId giúp ghép cặp request-response, tránh confusing
- ✅ **Timestamp:** Dùng để logging, audit trail, và xác định thứ tự tin nhắn
- ✅ **Kiểm tra lỗi:** checksum phát hiện dữ liệu bị hỏng trong quá trình truyền
- ✅ **Định tuyến:** topic field giúp server biết gửi tin đến subscribers nào
- ✅ **Xác thực:** sender field ghi rõ ai gửi tin

### 5.2 MessageType Enum - "Loại Giao Dịch"

**Vai trò:** Định nghĩa 9 loại tin nhắn, mỗi loại có mục đích riêng

```
┌─── Authentication (Xác Thực)
│   ├─ MSG_LOGIN
│   └─ MSG_LOGOUT
│
├─── Subscription (Đăng Ký)
│   ├─ MSG_SUBSCRIBE
│   └─ MSG_UNSUBSCRIBE
│
├─── Publishing (Phát Hành)
│   ├─ MSG_PUBLISH_TEXT
│   ├─ MSG_PUBLISH_FILE
│   └─ MSG_FILE_DATA
│
└─── Control (Kiểm Soát)
    ├─ MSG_ERROR
    └─ MSG_ACK
```

**Lợi ích:** Server biết cách xử lý mỗi loại (ví dụ: LOGIN → check password, PUBLISH → phân phối cho subscribers)

### 5.3 Payload - "Nội Dung Chính"

**Vai trò:** Chứa dữ liệu thực tế của tin nhắn

| Loại Tin | Payload |
|----------|---------|
| MSG_LOGIN | username:password |
| MSG_SUBSCRIBE | topic_name |
| MSG_PUBLISH_TEXT | Nội dung tin nhắn (string) |
| MSG_PUBLISH_FILE | File metadata (name, size, hash) |
| MSG_FILE_DATA | Chunk dữ liệu file (binary) |
| MSG_ERROR | Thông báo lỗi |
| MSG_ACK | (Có thể trống hoặc chứa response data) |

---

## 6. CÁC ĐẶC ĐIỂM AN TOÀN & TỐI ƯU

### 6.1 Bảo Mật (Security)

1. **Username/Password:** Xác thực client trước khi cho phép hoạt động
2. **Checksum:** Phát hiện dữ liệu bị thay đổi không hợp pháp
3. **Flags field:** Hỗ trợ compression & encryption trong tương lai

### 6.2 Độ Tin Cậy (Reliability)

1. **ACK/NACK:** Xác nhận mỗi hoạt động quan trọng
2. **Timeout (5 giây):** Phát hiện kết nối bị treo
3. **Reliable socket ops:** Đảm bảo tất cả bytes được gửi đi
4. **CRC32 checksum:** Kiểm tra tính toàn vẹn dữ liệu

### 6.3 Hiệu Năng (Performance)

1. **Fixed header size (128 bytes):** Dễ parse
2. **Binary format:** Nhỏ gọn hơn JSON/XML
3. **TCP stream:** Đảm bảo thứ tự packet
4. **Max message size 10MB:** Hỗ trợ file lớn

### 6.4 Khả Năng Mở Rộng (Extensibility)

1. **version field:** Nếu muốn thay đổi giao thức, tăng version
2. **flags field:** Có thể thêm tính năng mới (encryption, compression, v.v.)
3. **Enum MessageType:** Dễ thêm loại tin nhắn mới

---

## 7. CÁCH THỨC HOẠT ĐỘNG CỦA PUBLISH-SUBSCRIBE

### 7.1 Kiến Trúc Pub/Sub

```
          Publisher (Client)
                 │
                 │ MSG_PUBLISH_TEXT
                 │ (topic="sports", payload="Goal!")
                 ▼
          ┌─────────────────┐
          │  Message Broker │  (Server)
          │  (MessageBroker)│
          └────────┬────────┘
                   │
         ┌─────────┼─────────┐
         │         │         │
         ▼         ▼         ▼
    Subscriber  Subscriber  Subscriber
      (Alice)    (Bob)       (Charlie)
      lắng       lắng        lắng
      nghe       nghe        nghe
      topic      topic       khác
      sports     sports      topics
      │          │
      ◄──────────┼──────────────┐
      Nhận: "Goal!"  │ Nhận: "Goal!"
```

### 7.2 Bước Lập Program Pub/Sub

```
Step 1: SUBSCRIBE (Đăng Ký)
  Client A → Server: MSG_SUBSCRIBE (topic="weather")
  Server: Thêm Client A vào danh sách subscribers của topic "weather"

Step 2: PUBLISH (Phát Hành)
  Client B → Server: MSG_PUBLISH_TEXT (topic="weather", payload="Sunny!")
  Server: Lấy danh sách subscribers của "weather"
         → Gửi payload cho từng subscriber

Step 3: RECEIVE (Nhận)
  Server → Client A: [MessageType: PUBLISH_TEXT, topic: "weather", 
                       sender: "B", payload: "Sunny!"]
  Client A: Hiển thị "B said in weather: Sunny!"
```

---

## 8. BẢNG THAM CHIẾU NHANH

### 8.1 Hằng Số

| Hằng Số | Giá Trị | Mô Tả |
|---------|--------|-------|
| DEFAULT_PORT | 8080 | Cổng server lắng nghe |
| MAX_BUFFER_SIZE | 4 KB | Kích thước buffer |
| MAX_TOPIC_LEN | 32 | Tên topic tối đa 32 ký tự |
| MAX_USERNAME_LEN | 32 | Tên user tối đa 32 ký tự |
| SOCKET_TIMEOUT_MS | 5000 | Timeout 5 giây |
| MAX_MESSAGE_SIZE | 10 MB | Tin nhắn tối đa 10 MB |

### 8.2 Tất Cả Loại Tin Nhắn

| MessageType | Giá Trị | Mục Đích |
|-------------|--------|---------|
| MSG_LOGIN | 1 | Đăng nhập |
| MSG_LOGOUT | 2 | Đăng xuất |
| MSG_SUBSCRIBE | 3 | Đăng ký topic |
| MSG_UNSUBSCRIBE | 4 | Hủy đăng ký topic |
| MSG_PUBLISH_TEXT | 5 | Gửi tin nhắn text |
| MSG_PUBLISH_FILE | 6 | Bắt đầu gửi file |
| MSG_FILE_DATA | 7 | Gửi chunk file |
| MSG_ERROR | 8 | Báo lỗi |
| MSG_ACK | 9 | Xác nhận thành công |

---

## 9. TÍNH NĂNG NỔI BẬT

✅ **Binary protocol** - Dữ liệu nhỏ gọn, nhanh  
✅ **Fixed header** - Dễ parse, hiệu quả  
✅ **CRC32 checksum** - Kiểm tra lỗi truyền  
✅ **Message ID tracking** - Tránh duplicate, dễ debug  
✅ **ACK/NACK flow control** - Đảm bảo độ tin cậy  
✅ **Pub/Sub pattern** - Linh hoạt, broadcast được  
✅ **File transfer support** - Hỗ trợ file lớn (10 MB)  
✅ **Topic-based routing** - Phân loại tin nhắn rõ ràng  
✅ **Version & flags** - Dễ mở rộng trong tương lai  

---

## 10. VÍ DỤ THỰC TẾ

### Kịch Bản: Alice & Bob Trò Chuyện

```
┌─────────────────────────────────────────────────────────────┐
│ 1. Alice Đăng Nhập                                          │
└─────────────────────────────────────────────────────────────┘

Alice Client                          Server
    │                                  │
    │─ [MSG_LOGIN, "alice", "123"] ──►│
    │                       (Header)   │
    │                       (Payload)  │
    │                                  │
    │                          Check:  │
    │                          User ok │
    │                          Pass ok │
    │                                  │
    │◄─ [MSG_ACK, messageId: 1] ─────│
    │   (Alice is online now)         │

┌─────────────────────────────────────────────────────────────┐
│ 2. Alice Đăng Ký Topic "chat"                               │
└─────────────────────────────────────────────────────────────┘

Alice: [MSG_SUBSCRIBE, topic: "chat", sender: "alice"] ──►
Server: (Thêm alice vào subscribers list của "chat")
Alice: ◄─ [MSG_ACK]

┌─────────────────────────────────────────────────────────────┐
│ 3. Bob Đăng Nhập & Gửi Tin                                 │
└─────────────────────────────────────────────────────────────┘

Bob: [MSG_LOGIN, "bob", "456"] ──────────────────────────►
Server: ◄─ [MSG_ACK]

Bob: [MSG_PUBLISH_TEXT, topic: "chat", 
      sender: "bob", payload: "Hi Alice!"] ──────────────►
Server: 
  1. Lấy danh sách subscribers của "chat" → {alice}
  2. Gửi tin cho alice
  3. Trả [MSG_ACK] cho bob

Bob: ◄─ [MSG_ACK, messageId: 5]

┌─────────────────────────────────────────────────────────────┐
│ 4. Alice Nhận Tin Nhắn                                      │
└─────────────────────────────────────────────────────────────┘

Server: ───► Alice: [MSG_PUBLISH_TEXT, topic: "chat",
                     sender: "bob", payload: "Hi Alice!"]

Alice: (Hiển thị) "bob (in chat): Hi Alice!"
```

---

## Tổng Kết

Giao thức này là **nhẹ, an toàn, và đáng tin cậy** dành cho hệ thống Pub/Sub real-time. Nó cân bằng tốt giữa **độ tin cậy, hiệu năng, và khả năng mở rộng**, khiến nó phù hợp cho các ứng dụng:

- 💬 Chat systems (Ứng dụng trò chuyện)
- 📰 News distribution (Phân phối tin tức)
- 🎮 Game servers (Server trò chơi)
- 📊 Real-time data feeds (Nguồn dữ liệu real-time)
- 📁 File sharing systems (Hệ thống chia sẻ file)

---

**Tài liệu này tạo ngày:** January 6, 2026  
**Phiên bản:** 1.0  
**Trạng thái:** ✅ Hoàn chỉnh
