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
            printf("Error: Destination folder '%s' does not exist on server.\n", destination);
        } 
        else if (strstr(response, "ERR_NO_PERMISSION")) {
            printf("Error: You do not have permission for this group.\n");
        }
        else {
            printf("Server Error: %s", response);
        }
        // ---------------------------------------------
        fclose(fp);
        return;
    }

    printf("Ready for uploading... Please wait.\n");
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
    if (strstr(response, "DONE_UPLOAD")) {
        printf("Success: File '%s' uploaded successfully.\n", filename);
    } else {
        printf("Upload failed: %s\n", response);
    }
}

void req_download(int sock, char *server_path, char *local_destination) {
    // 1. Gửi lệnh: DOWNLOAD <server_path>
    char cmd[BUFSIZ];
    snprintf(cmd, sizeof(cmd), "DOWNLOAD %s\n", server_path); 
    send(sock, cmd, strlen(cmd), 0);

    // 2. Nhận phản hồi
    char response[BUFSIZ];
    if (recv_line(sock, response, sizeof(response)) <= 0) return;

    long filesize = 0;

    // --- CHECK PROTOCOL ---
    if (strncmp(response, "READY_DOWNLOAD OK", 17) == 0) {
        sscanf(response, "READY_DOWNLOAD OK %ld", &filesize);
    } 
    else if (strncmp(response, "DOWNLOAD ERR_FILE_NOT_FOUND", 27) == 0) {
        printf("Error: File not found on server.\n");
        return;
    }
    else if (strncmp(response, "DOWNLOAD ERR_NOT_LOGIN", 22) == 0) {
        printf("Error: You must login first.\n");
        return;
    }
    else {
        printf("Download failed. Server response: %s", response);
        return;
    }

    printf("Server ready. File size: %ld bytes\n", filesize);

    // 3. Xác định đường dẫn lưu file cục bộ
    char savepath[512];
    struct stat st = {0};

    if (stat(local_destination, &st) != -1 && S_ISDIR(st.st_mode)) {
        char *filename = strrchr(server_path, '/');
        if (filename) filename++; else filename = server_path;
        
        if (local_destination[strlen(local_destination)-1] == '/')
             snprintf(savepath, sizeof(savepath), "%s%s", local_destination, filename);
        else
             snprintf(savepath, sizeof(savepath), "%s/%s", local_destination, filename);
    } else {
        strncpy(savepath, local_destination, sizeof(savepath));
    }

    FILE *fp = fopen(savepath, "wb");
    if (!fp) {
        printf("Error: Cannot create file at %s. Check permission.\n", savepath);
        char trash[BUFFER_SIZE];
        while (recv_chunk(sock, trash) > 0);
        return; 
    }
    
    printf("Downloading to: %s\n", savepath);
    
    // --- [SỬA ĐỔI ĐỂ GIỐNG UPLOAD] ---
    
    // 1. Tính tổng số chunk dự kiến
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;

    char buffer[BUFFER_SIZE];
    int chunk_len;
    int chunk_count = 0; // Biến đếm chunk

    // 2. Vòng lặp nhận và in ra từng chunk
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        
        chunk_count++;
        // In ra dòng thông báo giống Upload
        printf("Chunk %d/%ld downloaded\n", chunk_count, total_chunks);
    }

    fclose(fp);
    printf("Success: Download Complete!\n");
}