#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>
#include <stdarg.h>

// --- HÀM LOGGING CỤC BỘ CHO TRANSFER ---
// Định dạng: [Time] [CLIENT:name] [TYPE] Content
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
// Đã thêm tham số client_name
void handle_upload(int client_sock, char *group_name, char *filename, long filesize, char *client_name) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "wb");
    if (!fp) {
        transfer_log(client_name, "SEND", "DONE_UPLOAD ERR_CANNOT_CREATE_FILE");
        send(client_sock, "DONE_UPLOAD ERR_CANNOT_CREATE_FILE\n", 31, 0);
        return;
    }

    // Gửi: READY_UPLOAD OK
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
            // Log đúng format Upload cũ của bạn
            transfer_log(client_name, "RECV", "UPLOAD %ld %d/%ld \"%s\" %s", 
                         filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
    }

    fclose(fp);
    
    // Gửi: DONE_UPLOAD OK
    send(client_sock, "DONE_UPLOAD OK\n", 15, 0);
    transfer_log(client_name, "SEND", "DONE_UPLOAD OK");
}

// Xử lý khi Client DOWNLOAD
// Đã thêm tham số client_name
void handle_download(int client_sock, char *group_name, char *filename, char *client_name) {
    char filepath[512];
    sprintf(filepath, "storage/%s/%s", group_name, filename);

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        transfer_log(client_name, "SEND", "FILE_NOT_FOUND 404");
        send(client_sock, "FILE_NOT_FOUND 404\n", 19, 0);
        return;
    }

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // 1. Gửi thông báo Size (Protocol)
    char response[256];
    sprintf(response, "DOWNLOAD 200 %ld 0\n", filesize);
    send(client_sock, response, strlen(response), 0);

    // --- LOG 1: READY-DOWNLOAD OK ---
    transfer_log(client_name, "SEND", "READY-DOWNLOAD OK");
    // --------------------------------

    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    char buffer[BUFFER_SIZE];
    int bytes_read;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(client_sock, buffer, bytes_read);
        chunk_count++;
        
        // --- LOG 2: Chunk Info Base64 ---
        // Format: DOWNLOAD <size> <curr>/<total> <name> <base64>
        char *b64 = base64_encode((unsigned char*)buffer, bytes_read);
        if (b64) {
            transfer_log(client_name, "SEND", "DOWNLOAD %ld %d/%ld \"%s\" %s", 
                         filesize, chunk_count, total_chunks, filename, b64);
            free(b64);
        }
        // --------------------------------
    }

    // 3. Gửi chunk kết thúc
    send_chunk(client_sock, NULL, 0);
    fclose(fp);

    // --- LOG 3: DONE-DOWNLOAD OK ---
    transfer_log(client_name, "SEND", "DONE-DOWNLOAD OK");
    // -------------------------------
}