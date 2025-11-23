#include "protocol.h"
#include <sys/socket.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stdio.h>

// Hàm đảm bảo nhận đủ n bytes (Fix lỗi TCP fragmentation)
int recv_n_bytes(int sock, void *buffer, int n) {
    int bytes_read = 0;
    int result;
    while (bytes_read < n) {
        result = recv(sock, (char*)buffer + bytes_read, n - bytes_read, 0);
        if (result <= 0) return result; // Error or Closed
        bytes_read += result;
    }
    return bytes_read;
}

// Gửi 1 chunk dữ liệu: Header (4 bytes) + Data
int send_chunk(int sock, char *data, int len) {
    ChunkHeader header;
    header.len = htonl(len); // Chuyển sang Big Endian (Network byte order)

    // 1. Gửi Header
    if (send(sock, &header, sizeof(header), 0) != sizeof(header)) return -1;

    // 2. Gửi Data (nếu độ dài > 0)
    if (len > 0) {
        if (send(sock, data, len, 0) != len) return -1;
    }
    return 0;
}

// Nhận 1 chunk dữ liệu
// Trả về: độ dài chunk nhận được, hoặc -1 nếu lỗi
int recv_chunk(int sock, char *buffer) {
    ChunkHeader header;
    
    // 1. Nhận Header (4 bytes)
    if (recv_n_bytes(sock, &header, sizeof(header)) <= 0) return -1;
    
    int len = ntohl(header.len); // Chuyển về Host byte order
    if (len == 0) return 0; // Chunk kết thúc

    // 2. Nhận Data
    if (recv_n_bytes(sock, buffer, len) <= 0) return -1;
    
    return len; // Trả về số byte dữ liệu thực tế
}