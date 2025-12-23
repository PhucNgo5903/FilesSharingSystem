// File: src/server/transfer.c
#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/stat.h> // <--- THÊM THƯ VIỆN ĐỂ CHECK FOLDER

// Hàm log cục bộ (Giữ nguyên)
void transfer_log(const char *client_name, const char *type, const char *format, ...) {
    char time_str[64];
    get_current_time_str(time_str, sizeof(time_str));
    printf("%s  [CLIENT:%s] [%s]  ", time_str, client_name, type);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

// Xử lý khi Client UPLOAD
void handle_upload(int client_sock, char *destination, char *filename, long filesize, char *client_name) {
    
    // 1. Xác định đường dẫn thư mục đích
    char folder_path[512];
    sprintf(folder_path, "storage/%s", destination);

    // 2. --- CHECK: THƯ MỤC CÓ TỒN TẠI KHÔNG? ---
    struct stat st = {0};
    if (stat(folder_path, &st) == -1) {
        // Nếu stat trả về -1 nghĩa là thư mục không tồn tại (hoặc lỗi truy cập)
        transfer_log(client_name, "SEND", "UPLOAD ERR_DIR_NOT_FOUND (Path: %s)", folder_path);
        send(client_sock, "UPLOAD ERR_DIR_NOT_FOUND\n", 25, 0);
        return; // Dừng xử lý ngay
    }
    // ---------------------------------------------

    // 3. Tạo đường dẫn file
    char filepath[1024]; // Tăng lên 1024 cho an toàn
    snprintf(filepath, sizeof(filepath), "%s/%s", folder_path, filename); // Dùng snprintf an toàn hơn sprintf

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        transfer_log(client_name, "SEND", "DONE_UPLOAD ERR_CANNOT_CREATE_FILE");
        send(client_sock, "DONE_UPLOAD ERR_CANNOT_CREATE_FILE\n", 31, 0);
        return;
    }

    // 4. Gửi xác nhận sẵn sàng
    send(client_sock, "READY_UPLOAD OK\n", 16, 0);
    transfer_log(client_name, "SEND", "READY_UPLOAD OK");

    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    char buffer[BUFFER_SIZE];
    int chunk_len;
    int chunk_count = 0;

    while ((chunk_len = recv_chunk(client_sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        chunk_count++;
        
        char *b64 = base64_encode((unsigned char*)buffer, chunk_len);
        if (b64) {
            transfer_log(client_name, "RECV", "UPLOAD %ld %d/%ld \"%s\" %s", 
                         filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
    }

    fclose(fp);
    
    // 5. Gửi xác nhận hoàn tất
    send(client_sock, "DONE_UPLOAD OK\n", 15, 0);
    transfer_log(client_name, "SEND", "DONE_UPLOAD OK");
}

// Hàm handle_download giữ nguyên...
void handle_download(int client_sock, char *server_path, char *client_name) {
    char filepath[512];
    sprintf(filepath, "storage/%s", server_path);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        // Phòng hờ, dù main.c đã check rồi
        transfer_log(client_name, "SEND", "FILE_NOT_FOUND 404");
        send(client_sock, "FILE_NOT_FOUND 404\n", 19, 0);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // Gửi thông tin file
    char response[256];
    sprintf(response, "DOWNLOAD 200 %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);
    transfer_log(client_name, "SEND", "READY-DOWNLOAD OK");

    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    char buffer[BUFFER_SIZE];
    int bytes_read;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
        chunk_count++;
        
        char *b64 = base64_encode((unsigned char*)buffer, bytes_read);
        if (b64) {
            // Log server_path cho gọn
            transfer_log(client_name, "SEND", "DOWNLOAD %ld %d/%ld \"%s\" %s", 
                         filesize, chunk_count, total_chunks, server_path, b64);
            free(b64);
        }
    }

    send_chunk(client_sock, NULL, 0);
    fclose(fp);

    transfer_log(client_name, "SEND", "DONE-DOWNLOAD OK");
}