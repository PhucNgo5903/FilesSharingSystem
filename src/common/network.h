// File: src/common/network.h
#ifndef NETWORK_H
#define NETWORK_H

// Khai báo nguyên mẫu hàm để các file khác có thể gọi
int recv_n_bytes(int sock, void *buffer, int n);
int send_chunk(int sock, char *data, int len);
int recv_chunk(int sock, char *buffer);

#endif