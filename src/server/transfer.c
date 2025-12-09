// File: src/server/transfer.c
#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdarg.h> // Để viết hàm log biến đổi tham số

// --- HÀM HỖ TRỢ GHI LOG CHUẨN ---
// Định dạng: [Time] [CLIENT:UNKNOWN] [TYPE] Content
void server_log(const char *type, const char *format, ...) {
    char time_str[64];
    get_current_time_str(time_str, sizeof(time_str));

    printf("%s  [CLIENT:UNKNOWN] [%s]  ", time_str, type);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n"); // Xuống dòng cuối log
}
// ---------------------------------

// Xử lý khi Client UPLOAD
void handle_upload(int client_sock, char *group_name, char *filename, long filesize) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        // Log lỗi gửi đi
        server_log("SEND", "DONE_UPLOAD ERR_CANNOT_CREATE_FILE");
        send(client_sock, "DONE_UPLOAD ERR_CANNOT_CREATE_FILE\n", 31, 0);
        return;
    }

    // 1. Gửi xác nhận sẵn sàng (Server -> Client)
    // Bản tin: READY_UPLOAD OK\n
    char *msg = "READY_UPLOAD OK\n";
    send(client_sock, msg, strlen(msg), 0);
    
    // LOG SEND
    server_log("SEND", "READY_UPLOAD OK");

    // Tính tổng số chunk
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    // 2. Vòng lặp nhận chunk (Client -> Server)
    char buffer[BUFFER_SIZE];
    int chunk_len;
    int chunk_count = 0;

    while ((chunk_len = recv_chunk(client_sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        chunk_count++;
        
        // --- LOG RECV: Bản tin chunk ---
        // Format: UPLOAD <file_size> <curr>/<total> "<path>" <base64>
        char *b64 = base64_encode((unsigned char*)buffer, chunk_len);
        if (b64) {
            server_log("RECV", "UPLOAD %ld %d/%ld \"%s\" %s", 
                       filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
    }

    fclose(fp);
    
    // 3. Gửi xác nhận hoàn tất (Server -> Client)
    // Bản tin: DONE_UPLOAD OK\n
    send(client_sock, "DONE_UPLOAD OK\n", 15, 0);
    
    // LOG SEND
    server_log("SEND", "DONE_UPLOAD OK");
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

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // --- ĐÃ XÓA DÒNG LOG "DOWNLOAD..." TẠI ĐÂY ---
    // (Vì đã được in trong main.c)

    char response[256];
    sprintf(response, "DOWNLOAD 200 %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);

    // --- SỬA: READY-DOWNLOAD KHÔNG CÓ SIZE Ở CUỐI ---
    printf("READY-DOWNLOAD 200 %s \"%s\"\n", group_name, filename);
    // ------------------------------------------------

    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_sent = 0;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
        total_sent += bytes_read;
        chunk_count++;
        
        // Log Base64 cho Download
        char *b64 = base64_encode((unsigned char*)buffer, bytes_read);
        if (b64) {
            printf("DOWNLOAD 200 %ld %d/%ld %s %s\n", filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
    }

    send_chunk(client_sock, NULL, 0);
    fclose(fp);

    printf("DONE-DOWNLOAD 200 %s \"%s\" %ld\n", group_name, filename, total_sent);
}