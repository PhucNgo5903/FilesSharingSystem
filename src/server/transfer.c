// File: src/server/transfer.c
#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

// Xử lý khi Client UPLOAD
// Đã thêm tham số 'filesize' để in log
void handle_upload(int client_sock, char *group_name, char *filename, long filesize) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[SERVER] File open error");
        send(client_sock, "ERROR 500 CANNOT_CREATE_FILE\n", 29, 0);
        return;
    }

    // 1. Gửi xác nhận sẵn sàng
    char *msg = "OK 200 READY_UPLOAD\n";
    send(client_sock, msg, strlen(msg), 0);
    
    // --- SỬA LOG 1: READY-UPLOAD ---
    printf("OK 200 READY-UPLOAD %s \"%s\" %ld\n", group_name, filename, filesize);
    // -------------------------------

    // 2. Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total_received = 0;
    int chunk_count = 0;

    while ((chunk_len = recv_chunk(client_sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        
        total_received += chunk_len;
        chunk_count++;
        
        // --- SỬA LOG 2: Chunk info (Bỏ tiền tố cũ) ---
        printf("Chunk #%d: Received %d bytes | Total: %ld bytes\n", chunk_count, chunk_len, total_received);
        // ---------------------------------------------
    }

    fclose(fp);
    
    // 3. Gửi xác nhận hoàn tất
    send(client_sock, "OK 200 UPLOAD DONE\n", 19, 0);
    
    // --- SỬA LOG 3: DONE-UPLOAD ---
    printf("OK 200 DONE-UPLOAD %s \"%s\" %ld\n", group_name, filename, total_received);
    // ------------------------------
}

// Xử lý khi Client DOWNLOAD
void handle_download(int client_sock, char *group_name, char *filename) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send(client_sock, "ERROR 404 FILE_NOT_FOUND\n", 25, 0);
        return;
    }

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // 1. Gửi thông báo OK kèm kích thước
    char response[256];
    sprintf(response, "OK 200 DOWNLOAD %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);

    // --- SỬA LOG 1: READY-DOWNLOAD ---
    printf("OK 200 READY-DOWNLOAD %s \"%s\" %ld\n", group_name, filename, filesize);
    // ---------------------------------

    // 2. Đọc file và gửi chunk
    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_sent = 0;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
        
        total_sent += bytes_read;
        chunk_count++;
        
        // --- SỬA LOG 2: Chunk Info ---
        printf("Chunk #%d: Sent %d bytes | Total: %ld bytes\n", chunk_count, bytes_read, total_sent);
        // -----------------------------
    }

    // 3. Gửi chunk kết thúc
    send_chunk(client_sock, NULL, 0);
    
    fclose(fp);

    // --- SỬA LOG 3: DONE-DOWNLOAD ---
    printf("OK 200 DONE-DOWNLOAD %s \"%s\" %ld\n", group_name, filename, total_sent);
    // --------------------------------
}