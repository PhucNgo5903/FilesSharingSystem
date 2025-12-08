// File: src/server/transfer.c
#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

// Xử lý khi Client UPLOAD
// Đã thêm tham số 'filesize' để in log
// Xử lý khi Client UPLOAD
void handle_upload(int client_sock, char *group_name, char *filename, long filesize) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        perror("[SERVER] File open error");
        send(client_sock, "ERROR 500 CANNOT_CREATE_FILE\n", 29, 0);
        return;
    }

    // Gửi xác nhận sẵn sàng
    char *msg = "READY_UPLOAD 200\n";
    send(client_sock, msg, strlen(msg), 0);
    
    // Tính tổng số chunk dự kiến
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1; // Tránh chia cho 0 nếu file rỗng

    printf("READY-UPLOAD 200 %s \"%s\" %ld )\n", group_name, filename, filesize);

    // Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total_received = 0;
    int chunk_count = 0;

    while ((chunk_len = recv_chunk(client_sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        total_received += chunk_len;
        chunk_count++;
        
        // --- LOG MỚI ĐÚNG FORMAT YÊU CẦU ---
        // UPLOAD 200 <filesize> <current>/<total> <filename> <base64>
        char *b64 = base64_encode((unsigned char*)buffer, chunk_len);
        if (b64) {
            printf("UPLOAD 200 %ld %d/%ld %s %s\n", filesize, chunk_count, total_chunks, filename, b64);
            free(b64); // Giải phóng bộ nhớ Base64
        }
        // -----------------------------------
    }

    fclose(fp);
    
    // Gửi xác nhận hoàn tất
    send(client_sock, "DONE-UPLOAD 200\n", 19, 0);
    
    printf("DONE-UPLOAD 200 %s \"%s\" %ld\n", group_name, filename, total_received);
}

// Xử lý khi Client DOWNLOAD
void handle_download(int client_sock, char *group_name, char *filename) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        send(client_sock, "FILE_NOT_FOUND 404\n", 25, 0);
        return;
    }

    // Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // Gửi thông báo OK kèm kích thước
    char response[256];
    sprintf(response, "DOWNLOAD 200 %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);

    // Tính tổng số chunk
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    printf("READY-DOWNLOAD 200 %s \"%s\" %ld\n", group_name, filename, filesize);

    // Đọc file và gửi chunk
    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_sent = 0;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
        total_sent += bytes_read;
        chunk_count++;
        
        // --- LOG MỚI ĐÚNG FORMAT YÊU CẦU CHO DOWNLOAD (Tùy chọn nếu bạn muốn cả download cũng hiện thế này) ---
        // DOWNLOAD 200 <filesize> <current>/<total> <filename> <base64>
        char *b64 = base64_encode((unsigned char*)buffer, bytes_read);
        if (b64) {
            printf("DOWNLOAD 200 %ld %d/%ld %s %s\n", filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
        // -----------------------------------------------------------------------------------------------------
    }

    // Gửi chunk kết thúc
    send_chunk(client_sock, NULL, 0);
    
    fclose(fp);

    printf("DONE-DOWNLOAD 200 %s \"%s\" %ld\n", group_name, filename, total_sent);
}