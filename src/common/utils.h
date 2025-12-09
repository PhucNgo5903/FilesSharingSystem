// File: src/common/utils.h
#ifndef UTILS_H
#define UTILS_H

#include <stddef.h> // Để dùng size_t

// Khai báo hàm
char *base64_encode(const unsigned char *data, size_t input_length);

// Hàm lấy thời gian hiện tại dạng chuỗi: "[DD-MM-YYYY HH:MM:SS]"
void get_current_time_str(char *buffer, size_t size);

#endif