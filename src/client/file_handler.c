#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>

// Xử lý Upload File
void req_upload(int sock, char *destination, char *path) {
    // ... (Đoạn mở file và gửi lệnh UPLOAD giữ nguyên) ...
    FILE *fp = fopen(path, "rb");
    if (!fp) { printf("[Client] Error: Cannot open file at %s\n", path); return; }
    char *filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);
    char cmd[1024];
    sprintf(cmd, "UPLOAD \"%s\" %s %ld\n", filename, destination, filesize);
    send(sock, cmd, strlen(cmd), 0);

    // --- XỬ LÝ PHẢN HỒI ---
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) { fclose(fp); return; }
    response[len] = 0;

    // Kiểm tra Ready
    if (strstr(response, "READY_UPLOAD") == NULL) {
        // --- CHECK LỖI MỚI: THƯ MỤC KHÔNG TỒN TẠI ---
        if (strstr(response, "ERR_DIR_NOT_FOUND")) {
            printf("[Client] Error: Destination folder '%s' does not exist on server.\n", destination);
        } 
        else if (strstr(response, "ERR_NO_PERMISSION")) {
            printf("[Client] Error: You do not have permission for this group.\n");
        }
        else {
            printf("[Client] Server Error: %s", response);
        }
        // ---------------------------------------------
        fclose(fp);
        return;
    }

    printf("READY_UPLOAD OK\n");
    // ... (Đoạn gửi chunk giữ nguyên) ...
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;
    char buffer[BUFFER_SIZE];
    int bytes_read;
    int chunk_count = 0;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(sock, buffer, bytes_read);
        chunk_count++;
        printf("Chunk %d/%ld uploaded\n", chunk_count, total_chunks);
    }
    send_chunk(sock, NULL, 0);
    fclose(fp);

    len = recv(sock, response, 1023, 0);
    response[len] = 0;
    if (strstr(response, "UPLOAD DONE")) {
        printf("DONE_UPLOAD OK\n");
    } else {
        printf("%s\n", response);
    }
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

    long filesize = 0;
    if (strncmp(response, "DOWNLOAD 200", 12) == 0) {
        sscanf(response, "DOWNLOAD 200 %ld", &filesize);
    } else {
        // Thêm check lỗi quyền
        if (strstr(response, "ERR_NO_PERMISSION")) {
            printf("[Client] Error: You are not a member of this group.\n");
        } else {
            printf("[Client] Download failed: %s", response);
        }
        return;
    }

    // --- SỬA LOG 1: In thông báo ngắn gọn ---
    printf("READY_DOWNLOAD OK\n");
    // ----------------------------------------

    // 3. Tạo file
    char savepath[512];
    if (dest_folder[strlen(dest_folder) - 1] == '/') {
        sprintf(savepath, "%s%s", dest_folder, filename);
    } else {
        sprintf(savepath, "%s/%s", dest_folder, filename);
    }

    FILE *fp = fopen(savepath, "wb");
    if (!fp) {
        printf("[Client] Error: Cannot create file at %s.\n", savepath);
        return;
    }
    
    // Tính tổng số chunk
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    // 4. Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    int chunk_count = 0;
    
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        chunk_count++;

        // --- SỬA LOG 2: In Chunk X/Y downloaded ---
        printf("Chunk %d/%ld downloaded\n", chunk_count, total_chunks);
        // ------------------------------------------
    }

    fclose(fp);

    // --- SỬA LOG 3: In DONE ngắn gọn ---
    printf("DONE_DOWNLOAD OK\n");
    // -----------------------------------
}