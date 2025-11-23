#include "../common/protocol.h"
#include "../common/network.h" // Include file header tự tạo ở trên
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>        // <--- Thêm dòng này để dùng được hàm send()
#include <unistd.h>            // <--- Thêm dòng này (chứa các hàm chuẩn POSIX)

// Xử lý khi Client muốn UPLOAD file lên Server
void handle_upload(int client_sock, char *group_name, char *filename) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename); // Path: storage/group/file

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        send(client_sock, "ERROR 500 CANNOT_CREATE_FILE\n", 29, 0);
        return;
    }

    // 1. Gửi xác nhận sẵn sàng
    char *msg = "OK 200 READY_UPLOAD\n";
    send(client_sock, msg, strlen(msg), 0);
    printf("[Server] Ready to receive file: %s\n", filepath);

    // 2. Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total_received = 0;

    while ((chunk_len = recv_chunk(client_sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        total_received += chunk_len;
    }

    fclose(fp);
    
    // 3. Gửi xác nhận hoàn tất
    send(client_sock, "OK 200 UPLOAD DONE\n", 19, 0);
    printf("[Server] Upload finished. Total: %ld bytes\n", total_received);
}

// Xử lý khi Client muốn DOWNLOAD file từ Server
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

    // 1. Gửi thông báo OK kèm kích thước (giả lập checksum là 0)
    char response[256];
    sprintf(response, "OK 200 DOWNLOAD %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);

    // 2. Đọc file và gửi chunk
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
    }

    // 3. Gửi chunk kết thúc (len = 0)
    send_chunk(client_sock, NULL, 0);
    
    fclose(fp);
    printf("[Server] Sent file %s (%ld bytes) to client.\n", filename, filesize);
}