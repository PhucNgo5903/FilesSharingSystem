#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>

#define BUFFER_SIZE 4096
#define CMD_UPLOAD "UPLOAD"
#define CMD_DOWNLOAD "DOWNLOAD"

// Cấu trúc Header của 1 chunk (4 bytes length)
typedef struct {
    int32_t len; // Độ dài payload (Big Endian)
} ChunkHeader;

#endif