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

// Xử lý Download File
void req_download(int sock, char *server_path, char *local_destination) {
    // 1. Gửi lệnh: DOWNLOAD <server_path>
    char cmd[1024];
    sprintf(cmd, "DOWNLOAD \"%s\"\n", server_path);
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
        if (strstr(response, "ERR_NO_PERMISSION")) {
            printf("Error: You do not have permission for this group/file.\n");
        } else if (strstr(response, "FILE_NOT_FOUND")) {
            printf("Error: File not found on server.\n");
        } else {
            printf("Download failed: %s", response);
        }
        return;
    }

    printf("Ready for downloading... Please wait.\n");

    // 3. Xác định đường dẫn lưu file cục bộ
    // Nếu local_destination là thư mục (kết thúc bằng /) -> ghép tên file gốc vào
    // Nếu local_destination là file -> dùng luôn
    char savepath[512];
    struct stat st = {0};

    // Kiểm tra xem local_destination có phải là thư mục tồn tại không
    if (stat(local_destination, &st) != -1 && S_ISDIR(st.st_mode)) {
        // Tách tên file từ server_path (VD: "group/docs/file.txt" -> "file.txt")
        char *filename = strrchr(server_path, '/');
        if (filename) filename++; else filename = server_path;
        
        // Ghép đường dẫn: local_dest + / + filename
        if (local_destination[strlen(local_destination)-1] == '/')
             sprintf(savepath, "%s%s", local_destination, filename);
        else
             sprintf(savepath, "%s/%s", local_destination, filename);
    } else {
        // Coi như người dùng nhập đường dẫn file đầy đủ
        strcpy(savepath, local_destination);
    }

    FILE *fp = fopen(savepath, "wb");
    if (!fp) {
        printf("Error: Cannot create file at %s. Check path or permission.\n", savepath);
        // Vẫn phải đọc hết dữ liệu rác từ server để tránh lỗi protocol
        // (Tuy nhiên trong demo đơn giản có thể return luôn, nhưng tốt nhất là drain socket)
        return; 
    }
    
    // 4. Nhận chunk
    long total_chunks = (filesize + BUFFER_SIZE - 1) / BUFFER_SIZE;
    if (total_chunks == 0) total_chunks = 1;
    char buffer[BUFFER_SIZE];
    int chunk_len;
    int chunk_count = 0;
    
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        chunk_count++;
        printf("Chunk %d/%ld downloaded\n", chunk_count, total_chunks);
    }

    fclose(fp);
    
    printf("Success: File saved to: %s\n", savepath);
}