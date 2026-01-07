# Hướng dẫn Gửi và Nhận File

## 📖 Giới thiệu

Hệ thống hỗ trợ gửi file giữa các clients thông qua server với cơ chế truyền tải tin cậy. File được chia nhỏ thành các chunks (khối dữ liệu) để truyền qua mạng an toàn và hiệu quả.

## 🎯 Tính năng

- ✅ Gửi file đến một topic cụ thể hoặc user cụ thể
- ✅ Hỗ trợ nhiều loại file (text, images, audio, video, documents, ...)
- ✅ Chia file thành chunks để truyền
- ✅ Hiển thị progress bar (thanh tiến trình)
- ✅ Tự động lưu file nhận được
- ✅ Giới hạn kích thước file (mặc định 10MB)

## 🔧 Cơ chế Hoạt động

### Quy trình Gửi File

```
┌─────────┐                   ┌────────┐                   ┌─────────┐
│ Sender  │                   │ Server │                   │Receiver │
└────┬────┘                   └───┬────┘                   └────┬────┘
     │                            │                             │
     │ 1. MSG_PUBLISH_FILE        │                             │
     ├───────────────────────────>│                             │
     │    (filename, size, ...)   │                             │
     │                            │                             │
     │                            │ 2. Distribute to subscribers│
     │                            ├────────────────────────────>│
     │                            │    MSG_PUBLISH_FILE         │
     │                            │                             │
     │ 3. MSG_FILE_DATA (chunk1)  │                             │
     ├───────────────────────────>│                             │
     │                            │ 4. Forward chunk1           │
     │                            ├────────────────────────────>│
     │                            │                             │
     │ 5. MSG_FILE_DATA (chunk2)  │                             │
     ├───────────────────────────>│                             │
     │                            │ 6. Forward chunk2           │
     │                            ├────────────────────────────>│
     │                            │                             │
     │        ... (more chunks)   │                             │
     │                            │                             │
     │ N. Last chunk (with flag)  │                             │
     ├───────────────────────────>│                             │
     │                            │ Forward last chunk          │
     │                            ├────────────────────────────>│
     │                            │                             │
     │                            │ Receiver assembles file     │
     │                            │                             │
```

### Các bước chi tiết

**BƯỚC 1: Gửi MSG_PUBLISH_FILE**

Sender gửi metadata của file:

```cpp
PacketHeader {
    msgType = MSG_PUBLISH_FILE,
    sender = "alice",
    topic = "bob",  // hoặc topic khác
    payloadLength = <length of filename>
}
Payload: "photo.jpg"

// Server lưu thông tin: alice đang gửi file "photo.jpg" đến topic "bob"
```

**BƯỚC 2: Server phân phối MSG_PUBLISH_FILE**

Server gửi thông báo đến tất cả subscribers của topic:

```cpp
// Đến mỗi subscriber
PacketHeader {
    msgType = MSG_PUBLISH_FILE,
    sender = "alice",
    topic = "bob",
    payloadLength = <length>
}
Payload: "photo.jpg"
```

Receiver nhận được và chuẩn bị nhận file:

- Tạo buffer để lưu data
- Mở file để ghi (ví dụ: `received_photo.jpg`)

**BƯỚC 3-N: Gửi MSG_FILE_DATA chunks**

Sender chia file thành các chunks và gửi:

```cpp
// Chunk 1
PacketHeader {
    msgType = MSG_FILE_DATA,
    sender = "alice",
    topic = "bob",
    payloadLength = 4096,  // chunk size
    flags = 0  // not last chunk
}
Payload: <4096 bytes of file data>

// Chunk 2
PacketHeader {
    msgType = MSG_FILE_DATA,
    sender = "alice",
    topic = "bob",
    payloadLength = 4096,
    flags = 0
}
Payload: <4096 bytes>

// ... more chunks ...

// Last chunk
PacketHeader {
    msgType = MSG_FILE_DATA,
    sender = "alice",
    topic = "bob",
    payloadLength = 2048,  // remaining bytes
    flags = 0x01  // FLAG_LAST_CHUNK
}
Payload: <2048 bytes>
```

**BƯỚC 4: Server forward chunks**

Server nhận mỗi chunk và forward đến subscribers:

```cpp
for each chunk received:
    find subscribers of topic
    for each subscriber:
        send chunk to subscriber
```

**BƯỚC 5: Receiver nhận và lưu**

Receiver nhận từng chunk và ghi vào file:

```cpp
while (receiving chunks) {
    chunk = receive_chunk();
    write_to_file(chunk.payload);

    if (chunk.flags & FLAG_LAST_CHUNK) {
        close_file();
        show_notification("File received!");
        break;
    }
}
```

## 💻 Implementation Client-side

### Gửi File (Qt Example)

```cpp
void MainWindow::sendFile()
{
    // 1. Chọn file
    QString filePath = QFileDialog::getOpenFileName(
        this, "Select File", "", "All Files (*.*)"
    );

    if (filePath.isEmpty()) return;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::critical(this, "Error", "Cannot open file");
        return;
    }

    // 2. Lấy thông tin file
    QFileInfo fileInfo(filePath);
    QString filename = fileInfo.fileName();
    qint64 fileSize = file.size();

    // 3. Kiểm tra kích thước
    if (fileSize > MAX_MESSAGE_SIZE) {
        QMessageBox::critical(this, "Error", "File too large (max 10MB)");
        return;
    }

    // 4. Hỏi topic/recipient
    QString topic = QInputDialog::getText(
        this, "Send To", "Enter topic or username:"
    );

    // 5. Gửi MSG_PUBLISH_FILE
    PacketHeader header;
    memset(&header, 0, sizeof(header));
    header.msgType = MSG_PUBLISH_FILE;
    header.payloadLength = filename.toStdString().length();
    strcpy(header.sender, m_username.toStdString().c_str());
    strcpy(header.topic, topic.toStdString().c_str());
    header.messageId = generateMessageId();
    header.timestamp = getCurrentTimestamp();

    sendPacket(header, filename.toStdString().c_str());

    // 6. Gửi file data theo chunks
    const int CHUNK_SIZE = 4096;
    char buffer[CHUNK_SIZE];
    qint64 totalSent = 0;

    while (!file.atEnd()) {
        qint64 bytesRead = file.read(buffer, CHUNK_SIZE);

        PacketHeader dataHeader;
        memset(&dataHeader, 0, sizeof(dataHeader));
        dataHeader.msgType = MSG_FILE_DATA;
        dataHeader.payloadLength = bytesRead;
        strcpy(dataHeader.sender, m_username.toStdString().c_str());
        strcpy(dataHeader.topic, topic.toStdString().c_str());
        dataHeader.messageId = generateMessageId();

        // Đánh dấu chunk cuối
        if (file.atEnd()) {
            dataHeader.flags = 0x01;  // FLAG_LAST_CHUNK
        }

        sendPacket(dataHeader, buffer);

        totalSent += bytesRead;

        // Update progress bar
        int progress = (totalSent * 100) / fileSize;
        m_progressBar->setValue(progress);
    }

    file.close();
    QMessageBox::information(this, "Success", "File sent!");
}
```

### Nhận File

```cpp
void MainWindow::handleIncomingPacket(const PacketHeader& header, const char* payload)
{
    if (header.msgType == MSG_PUBLISH_FILE) {
        // Nhận thông báo file mới
        QString filename(payload);
        QString sender(header.sender);

        // Hiển thị thông báo
        m_chatDisplay->append(
            QString("[FILE] %1 is sending: %2").arg(sender).arg(filename)
        );

        // Chuẩn bị nhận file
        QString savePath = "received_" + filename;
        m_currentFile = new QFile(savePath);
        if (!m_currentFile->open(QIODevice::WriteOnly)) {
            QMessageBox::critical(this, "Error", "Cannot create file");
            delete m_currentFile;
            m_currentFile = nullptr;
            return;
        }

        m_receivingFile = true;
        m_currentFilename = filename;

    } else if (header.msgType == MSG_FILE_DATA) {
        // Nhận chunk data
        if (!m_receivingFile || !m_currentFile) {
            return;
        }

        // Ghi data vào file
        m_currentFile->write(payload, header.payloadLength);

        // Kiểm tra chunk cuối
        if (header.flags & 0x01) {  // FLAG_LAST_CHUNK
            m_currentFile->close();
            delete m_currentFile;
            m_currentFile = nullptr;
            m_receivingFile = false;

            // Thông báo hoàn thành
            m_chatDisplay->append(
                QString("[FILE] Received: %1").arg(m_currentFilename)
            );
            QMessageBox::information(
                this, "File Received",
                QString("File saved as: received_%1").arg(m_currentFilename)
            );
        }
    }
}
```

## 🖥️ Implementation Server-side

### Xử lý MSG_PUBLISH_FILE

```cpp
void handlePublishFile(ClientInfo* client, const PacketHeader& header, const char* payload)
{
    std::string filename(payload, header.payloadLength);
    std::string topic(header.topic);

    logMessage(
        "File transfer initiated: " + std::string(header.sender) +
        " sending '" + filename + "' to topic '" + topic + "'"
    );

    // Forward đến subscribers
    auto subscribers = g_broker.getSubscribers(topic);
    for (SOCKET subscriberSock : subscribers) {
        sendPacket(subscriberSock, header, payload);
    }

    // Lưu trạng thái file transfer
    client->sendingFile = true;
    client->currentFilename = filename;
    client->currentFileTopic = topic;
}
```

### Xử lý MSG_FILE_DATA

```cpp
void handleFileData(ClientInfo* client, const PacketHeader& header, const char* payload)
{
    std::string topic(header.topic);

    // Forward chunk đến subscribers
    auto subscribers = g_broker.getSubscribers(topic);
    for (SOCKET subscriberSock : subscribers) {
        sendPacket(subscriberSock, header, payload);
    }

    // Kiểm tra chunk cuối
    if (header.flags & 0x01) {  // FLAG_LAST_CHUNK
        logMessage(
            "File transfer completed: " + client->currentFilename
        );
        client->sendingFile = false;
        client->currentFilename.clear();
        client->currentFileTopic.clear();
    }
}
```

## 📊 Giao thức Chi tiết

### MSG_PUBLISH_FILE

```
PacketHeader {
    msgType: MSG_PUBLISH_FILE (6)
    payloadLength: <độ dài tên file>
    messageId: <unique ID>
    timestamp: <thời gian>
    sender: <username người gửi>
    topic: <topic hoặc recipient>
    flags: 0
    checksum: <CRC32>
}
Payload: <filename> (ví dụ: "document.pdf")
```

### MSG_FILE_DATA

```
PacketHeader {
    msgType: MSG_FILE_DATA (7)
    payloadLength: <kích thước chunk, thường 4096 bytes>
    messageId: <unique ID>
    timestamp: <thời gian>
    sender: <username người gửi>
    topic: <topic hoặc recipient>
    flags: 0x00 (normal chunk) hoặc 0x01 (last chunk)
    checksum: <CRC32>
}
Payload: <binary file data>
```

### Flags

```cpp
#define FLAG_LAST_CHUNK 0x01
#define FLAG_COMPRESSED 0x02  // future use
#define FLAG_ENCRYPTED  0x04  // future use
```

## 🎨 UI Components (Qt)

### File Send Dialog

```xml
┌─────────────────────────────────┐
│  Send File                      │
├─────────────────────────────────┤
│  File: [document.pdf       ][📁]│
│  Size: 2.5 MB                   │
│                                 │
│  Send to:                       │
│  ◉ Topic: [general      ▼]      │
│  ○ User:  [            ]        │
│                                 │
│  Progress:                      │
│  [████████░░░░░░░░░░░] 45%      │
│                                 │
│     [Cancel]      [Send]        │
└─────────────────────────────────┘
```

### File Receive Notification

```
┌─────────────────────────────────┐
│  Incoming File                  │
├─────────────────────────────────┤
│  From: alice                    │
│  File: photo.jpg                │
│  Size: 1.2 MB                   │
│                                 │
│  Save as:                       │
│  [received_photo.jpg      ]     │
│                                 │
│  [Reject]  [Accept & Download]  │
└─────────────────────────────────┘
```

## ⚠️ Xử lý Lỗi

### Lỗi thường gặp

1. **File quá lớn**

```cpp
if (fileSize > MAX_MESSAGE_SIZE) {
    sendErrorPacket(sock, messageId, "File exceeds size limit");
    return;
}
```

2. **Không thể mở file**

```cpp
if (!file.open(QIODevice::ReadOnly)) {
    QMessageBox::critical(this, "Error", "Cannot read file");
    return;
}
```

3. **Mất kết nối giữa chừng**

```cpp
// Timeout detection
if (time_since_last_chunk > TIMEOUT) {
    cleanup_partial_file();
    notify_user("Transfer interrupted");
}
```

4. **Checksum không khớp**

```cpp
if (calculated_checksum != header.checksum) {
    request_retransmit_chunk();
}
```

## 🔐 Bảo mật File Transfer

### Hiện tại

- ✅ Size validation
- ✅ Topic-based access control
- ✅ CRC32 checksum

### Tương lai

- 🔲 File encryption (AES-256)
- 🔲 Virus scanning
- 🔲 File type restrictions
- 🔲 User quotas
- 🔲 Resume interrupted transfers

## 📈 Tối ưu hóa

### Chunk Size Selection

```cpp
// Nhỏ (1KB): Ít buffer, nhiều overhead
#define SMALL_CHUNK 1024

// Trung bình (4KB): Cân bằng tốt
#define MEDIUM_CHUNK 4096  // RECOMMENDED

// Lớn (64KB): Ít overhead, cần buffer lớn
#define LARGE_CHUNK 65536
```

### Compression (Tương lai)

```cpp
if (file_is_compressible()) {
    payload_compressed = compress(payload);
    header.flags |= FLAG_COMPRESSED;
}
```

### Parallel Transfers

```cpp
// Gửi nhiều files đồng thời
QThreadPool::globalInstance()->start(new FileTransferTask(file1));
QThreadPool::globalInstance()->start(new FileTransferTask(file2));
```

## 📝 Best Practices

### Sender

1. **Validate file** trước khi gửi
2. **Hiển thị progress** cho user
3. **Handle cancellation**: Cho phép user hủy transfer
4. **Cleanup on error**: Xóa partial data nếu thất bại

### Receiver

1. **Ask permission**: Hỏi user trước khi tự động tải
2. **Check disk space**: Đảm bảo đủ chỗ trống
3. **Validate filename**: Tránh path traversal attacks
4. **Quarantine**: Scan virus trước khi mở

### Server

1. **Rate limiting**: Giới hạn số file/giây per user
2. **Bandwidth management**: Ưu tiên cho messages nhỏ
3. **Timeout handling**: Dọn dẹp transfers bị bỏ dở
4. **Logging**: Ghi log mọi file transfers

## 🧪 Testing

### Test Cases

```cpp
// Test 1: Small text file
send_file("test.txt", 1KB, "bob");
verify_received("test.txt", "bob");

// Test 2: Large binary file
send_file("video.mp4", 9MB, "public");
verify_received("video.mp4", "all_subscribers");

// Test 3: Empty file
send_file("empty.dat", 0, "alice");
verify_received("empty.dat", "alice");

// Test 4: Special characters in filename
send_file("文件.pdf", 500KB, "topic");
verify_received("文件.pdf", "topic");

// Test 5: Concurrent transfers
parallel_send([
    "file1.jpg",
    "file2.png",
    "file3.doc"
]);
```

## 💡 Ví dụ Thực tế

### Gửi ảnh đến nhóm

```
1. User clicks "Send File"
2. Selects "photo.jpg" (2.5MB)
3. Enters topic "friends"
4. Clicks "Send"
5. Progress bar: 0% → 100% (khoảng 2-3 giây)
6. All members of "friends" topic receive file
7. Notification: "photo.jpg received from alice"
```

### Gửi document đến cá nhân

```
1. User selects "report.pdf"
2. Enters recipient "boss"
3. File sent only to user "boss"
4. Boss receives and saves as "received_report.pdf"
```

## 📚 Tài liệu Liên quan

- [Giao thức chi tiết](GIAO_THUC.md) - MSG_PUBLISH_FILE và MSG_FILE_DATA
- [Hệ thống Chat](HE_THONG_CHAT.md) - Topics và subscriptions
- [README](../README.md) - Tổng quan hệ thống

---

**Phiên bản**: 1.0  
**Cập nhật**: 2026-01-07
