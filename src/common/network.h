// File: src/common/network.h
#ifndef NETWORK_H
#define NETWORK_H
#include <stddef.h>     // size_t
#include <sys/types.h>
// --- [THÊM CÁC DÒNG NÀY VÀO ĐẦU] ---

// -----------------------------------

// Khai báo nguyên mẫu hàm để các file khác có thể gọi
int recv_n_bytes(int sock, void *buffer, int n);
int send_chunk(int sock, char *data, int len);
int recv_chunk(int sock, char *buffer);
ssize_t recv_line(int sock, char *buffer, size_t max_len);
#endif