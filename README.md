# Hệ thống Chat & Truyền File Pub/Sub

Hệ thống chat và chia sẻ file realtime sử dụng kiến trúc Publisher/Subscriber (Pub/Sub) với giao thức TCP/IP.

## 📋 Tổng quan

Đây là một ứng dụng chat đa người dùng với khả năng:

- **Nhắn tin theo chủ đề (Topics)**: Người dùng có thể đăng ký và nhận tin từ các chủ đề khác nhau
- **Truyền file**: Gửi và nhận file giữa các người dùng
- **Truyền phát audio**: Phát và nghe audio realtime
- **Chat riêng tư**: Gửi tin nhắn trực tiếp đến người dùng cụ thể

## 🏗️ Kiến trúc Hệ thống

Hệ thống bao gồm 2 thành phần chính:

### Server (Broker)

- **Chat Channel** (Port 8080): Xử lý đăng nhập, đăng ký topics, và tin nhắn văn bản
- **Stream Channel** (Port 8081): Xử lý truyền phát audio/stream realtime
- Quản lý kết nối từ nhiều clients đồng thời
- Định tuyến tin nhắn đến đúng subscribers

### Client (Qt Application)

- Giao diện đồ họa thân thiện (Qt Widgets)
- Kết nối đồng thời đến cả chat và stream channels
- Hỗ trợ chat, gửi file, và phát/nghe audio

## 🚀 Tính năng

### ✅ Đã hoàn thành

- ✔️ Đăng nhập/Đăng xuất người dùng
- ✔️ Subscribe/Unsubscribe topics
- ✔️ Gửi và nhận tin nhắn văn bản
- ✔️ Gửi và nhận file
- ✔️ Truyền phát audio realtime
- ✔️ Xử lý đồng thời nhiều kết nối
- ✔️ Giao thức với PacketHeader chuẩn hóa

### 🔜 Phát triển tiếp

- 🔲 Mã hóa tin nhắn end-to-end
- 🔲 Lưu trữ lịch sử chat
- 🔲 Giao diện web
- 🔲 Video call/conference

## 📦 Công nghệ sử dụng

- **Ngôn ngữ**: C++ (C++11 trở lên)
- **Framework GUI**: Qt 5/6
- **Mạng**: TCP/IP sockets (WinSock2 trên Windows, POSIX sockets trên Linux)
- **Build System**: CMake, qmake
- **Platform**: Cross-platform (Windows, Linux)

## 📁 Cấu trúc Thư mục

```
network_programming/
├── Client/                 # Mã nguồn ứng dụng Client
│   ├── main.cpp           # Entry point
│   ├── mainwindow.h/cpp   # Giao diện chính
│   ├── audiodialog.h/cpp  # Quản lý audio
│   └── CMakeLists.txt     # Build config
├── Server/                 # Mã nguồn Server
│   ├── server.cpp         # Main server logic
│   └── broker.h           # Message broker implementation
├── Document/               # Tài liệu hướng dẫn
│   ├── GIAO_THUC.md       # Chi tiết giao thức
│   ├── HE_THONG_CHAT.md   # Hướng dẫn hệ thống chat
│   └── GUI_FILE.md        # Hướng dẫn gửi file
└── protocol.h              # Định nghĩa giao thức chung
```

## 🔧 Hướng dẫn Cài đặt

### Yêu cầu hệ thống

- C++ compiler hỗ trợ C++11+ (GCC, MSVC, Clang)
- Qt 5.12+ hoặc Qt 6.x
- CMake 3.10+

### Build Server

```bash
# Trên Windows (MinGW/MSYS2)
cd Server
g++ -std=c++11 server.cpp -o server.exe -lws2_32

# Trên Linux
cd Server
g++ -std=c++11 server.cpp -o server -lpthread
```

### Build Client

#### Sử dụng CMake:

```bash
cd Client
mkdir build && cd build
cmake ..
cmake --build .
```

#### Sử dụng Qt Creator:

1. Mở file `Client/Client.pro` trong Qt Creator
2. Configure project với Qt kit
3. Build & Run

## 🎮 Hướng dẫn Sử dụng

### 1. Khởi động Server

```bash
cd Server
./server.exe    # Windows
./server        # Linux
```

Server sẽ lắng nghe trên:

- Port 8080: Chat channel
- Port 8081: Stream channel

### 2. Khởi động Client

#### Kết nối:

1. Chạy ứng dụng client
2. Nhập Server IP (mặc định: `127.0.0.1`)
3. Nhập Username
4. Nhấn "Connect"

#### Chat:

- **Chat công khai**: Gửi tin đến topic mặc định hoặc topic đã subscribe
- **Chat riêng**: Nhập username người nhận vào trường "Topic/Recipient"

#### Gửi File:

1. Nhấn nút "Send File"
2. Chọn file cần gửi
3. Nhập topic hoặc username người nhận
4. File sẽ được tự động chuyển đến subscribers

#### Audio:

1. Mở Audio Dialog
2. Chọn topic để phát
3. Nhấn "Start Stream" để phát hoặc "Start Listening" để nghe

## 📚 Tài liệu Chi tiết

- [Giao thức (Protocol)](Document/GIAO_THUC.md) - Chi tiết về message types và packet structure
- [Hệ thống Chat](Document/HE_THONG_CHAT.md) - Hướng dẫn sử dụng chat
- [Gửi File](Document/GUI_FILE.md) - Hướng dẫn gửi và nhận file
