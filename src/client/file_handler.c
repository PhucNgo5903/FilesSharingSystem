// File: src/client/file_handler.c
#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h> // Thư viện Socket chuẩn

// Xử lý Upload File
void req_upload(int sock, char *group_name, char *path) {
    // 1. Mở file từ đường dẫn người dùng nhập
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("[Client] Error: Cannot open file at %s\n", path);
        return;
    }

    // 2. Tách tên file gốc
    char *filename = strrchr(path, '/');
    if (filename) {
        filename++;
    } else {
        filename = path;
    }

    // 3. Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // 4. Gửi lệnh UPLOAD
    char cmd[1024];
    sprintf(cmd, "UPLOAD %s \"%s\" %ld dummy_checksum\n", group_name, filename, filesize);
    send(sock, cmd, strlen(cmd), 0);

    // 5. Đợi Server phản hồi READY
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) { fclose(fp); return; }
    response[len] = 0;

    if (strstr(response, "READY_UPLOAD") == NULL) {
        printf("[Client] Server Error: %s", response);
        fclose(fp);
        return;
    }

    // --- SỬA LOG 1: READY-UPLOAD ---
    // Thay vì in "[Client] Start uploading...", in format chuẩn:
    printf("OK 200 READY-UPLOAD %s \"%s\" %ld\n", group_name, filename, filesize);
    // -------------------------------
    
    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_sent = 0;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(sock, buffer, bytes_read);
        
        total_sent += bytes_read;
        chunk_count++;

        // --- SỬA LOG 2: Chunk Info ---
        // Format: Chunk #... | Total: ...
        printf("Chunk #%d: Sent %d bytes | Total: %ld bytes\n", chunk_count, bytes_read, total_sent);
        // -----------------------------
    }
    
    send_chunk(sock, NULL, 0); // Kết thúc
    fclose(fp);

    // 7. Nhận kết quả cuối cùng
    len = recv(sock, response, 1023, 0);
    response[len] = 0;

    // --- SỬA LOG 3: DONE-UPLOAD ---
    // Kiểm tra nếu server báo DONE thì in dòng DONE-UPLOAD chuẩn format
    if (strstr(response, "UPLOAD DONE")) {
        printf("OK 200 DONE-UPLOAD %s \"%s\" %ld\n", group_name, filename, total_sent);
    } else {
        // Nếu lỗi thì in nguyên văn server báo
        printf("%s\n", response);
    }
    // ------------------------------
}

// Xử lý Download File
void req_download(int sock, char *group_name, char *filename, char *dest_folder) {
    // 1. Gửi lệnh DOWNLOAD
    char cmd[1024];
    sprintf(cmd, "DOWNLOAD %s \"%s\"\n", group_name, filename);
    send(sock, cmd, strlen(cmd), 0);

    // 2. Nhận phản hồi OK <size>
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) return;
    response[len] = 0;

    // Parse phản hồi
    long filesize = 0;
    if (strncmp(response, "OK 200 DOWNLOAD", 15) == 0) {
        sscanf(response, "OK 200 DOWNLOAD %ld", &filesize);
    } else {
        printf("[Client] Download failed: %s", response);
        return;
    }

    // --- SỬA LOG 1: READY-DOWNLOAD ---
    printf("OK 200 READY-DOWNLOAD %s \"%s\" %ld\n", group_name, filename, filesize);
    // ---------------------------------

    // 3. Tạo đường dẫn lưu file (dest_folder + / + filename)
    char savepath[512];
    // Kiểm tra xem dest_folder có tận cùng là / hay chưa để ghép chuỗi cho đúng
    if (dest_folder[strlen(dest_folder) - 1] == '/') {
        sprintf(savepath, "%s%s", dest_folder, filename);
    } else {
        sprintf(savepath, "%s/%s", dest_folder, filename);
    }

    FILE *fp = fopen(savepath, "wb");
    if (!fp) {
        printf("[Client] Error: Cannot create file at %s. Check path or permissions.\n", savepath);
        return;
    }
    
    // 4. Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total_received = 0;
    int chunk_count = 0;
    
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        
        total_received += chunk_len;
        chunk_count++;

        // --- SỬA LOG 2: Chunk Info ---
        printf("Chunk #%d: Received %d bytes | Total: %ld bytes\n", chunk_count, chunk_len, total_received);
        // -----------------------------
    }

    fclose(fp);

    // --- SỬA LOG 3: DONE-DOWNLOAD ---
    printf("OK 200 DONE-DOWNLOAD %s \"%s\" %ld\n", group_name, filename, total_received);
    // --------------------------------
}