## Cấu Trúc Dự Án

```text
FileSharingSystem/
├── Makefile                # File cấu hình biên dịch tự động
├── README.md               # Tài liệu hướng dẫn
├── bin/                    # Chứa file thực thi (Binary) sau khi build
│   ├── server
│   └── client
├── data/                   # Cơ sở dữ liệu (Metadata) dạng File Text
│   ├── users.txt           # Thông tin đăng nhập
│   ├── groups.txt          # Danh sách nhóm
│   └── ...
├── storage/                # Kho lưu trữ file thực tế của Server
│   ├── coffee_lovers/      # Thư mục chứa file của nhóm Coffee Lovers
│   └── ...
├── client_storage/         # Thư mục giả lập kho file của Client (để test upload/down)
└── src/                    # Mã nguồn chính
    ├── common/             # [Shared] Thư viện dùng chung
    │   ├── protocol.h      # Định nghĩa cấu trúc gói tin, OpCode
    │   ├── network.c       # Wrapper hàm send/recv socket
    │   └── network.h
    ├── server/             # [Backend] Mã nguồn Server
    │   ├── main.c          # Vòng lặp chính, xử lý kết nối & Fork
    │   ├── transfer.c      # Logic nhận/gửi file Chunked
    │   └── ...
    └── client/             # [Frontend] Mã nguồn Client CLI
        ├── main.c          # Giao diện dòng lệnh (Menu)
        ├── file_handler.c  # Xử lý đọc file local để gửi đi
        └── ...
