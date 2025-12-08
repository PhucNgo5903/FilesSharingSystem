#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>

// Xử lý Upload File (Giữ nguyên)
void req_upload(int sock, char *group_name, char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("[Client] Error: Cannot open file at %s\n", path);
        return;
    }

    char *filename = strrchr(path, '/');
    if (filename) filename++; else filename = path;

    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    char cmd[1024];
    sprintf(cmd, "UPLOAD %s \"%s\" %ld\n", group_name, filename, filesize);
    send(sock, cmd, strlen(cmd), 0);

    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) { fclose(fp); return; }
    response[len] = 0;

    if (strstr(response, "READY_UPLOAD") == NULL) {
        printf("[Client] Server Error: %s", response);
        fclose(fp);
        return;
    }

    printf("READY-UPLOAD 200 %s \"%s\" %ld\n", group_name, filename, filesize);
    
    char buffer[BUFFER_SIZE];
    int bytes_read;
    long total_sent = 0;
    int chunk_count = 0;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(sock, buffer, bytes_read);
        total_sent += bytes_read;
        chunk_count++;
        printf("Chunk #%d: Sent %d bytes | Total: %ld bytes\n", chunk_count, bytes_read, total_sent);
    }
    
    send_chunk(sock, NULL, 0);
    fclose(fp);

    len = recv(sock, response, 1023, 0);
    response[len] = 0;

    if (strstr(response, "UPLOAD DONE")) {
        printf("DONE-UPLOAD 200 %s \"%s\" %ld\n", group_name, filename, total_sent);
    } else {
        printf("%s\n", response);
    }
}

// Xử lý Download File (Đã chỉnh sửa: Bỏ log chunk)
void req_download(int sock, char *group_name, char *filename, char *dest_folder) {
    // 1. Gửi lệnh DOWNLOAD
    char cmd[1024];
    sprintf(cmd, "DOWNLOAD %s \"%s\"\n", group_name, filename);
    send(sock, cmd, strlen(cmd), 0);

    // 2. Nhận phản hồi
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) return;
    response[len] = 0;

    long filesize = 0;
    if (strncmp(response, "DOWNLOAD 200", 12) == 0) {
        sscanf(response, "DOWNLOAD 200 %ld", &filesize);
    } else {
        printf("[Client] Download failed: %s", response);
        return;
    }

    // Log bắt đầu
    printf("READY-DOWNLOAD 200 %s \"%s\" %ld\n", group_name, filename, filesize);

    // 3. Tạo đường dẫn lưu file
    char savepath[512];
    if (dest_folder[strlen(dest_folder) - 1] == '/') {
        sprintf(savepath, "%s%s", dest_folder, filename);
    } else {
        sprintf(savepath, "%s/%s", dest_folder, filename);
    }

    FILE *fp = fopen(savepath, "wb");
    if (!fp) {
        printf("[Client] Error: Cannot create file at %s. Check permissions.\n", savepath);
        return;
    }
    
    // 4. Vòng lặp nhận chunk (Không in log nữa)
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total_received = 0;
    
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        total_received += chunk_len;
        
        // --- ĐÃ XÓA DÒNG printf Chunk Info ---
    }

    fclose(fp);

    // Log kết thúc
    printf("DONE-DOWNLOAD 200 %s \"%s\" %ld\n", group_name, filename, total_received);
}