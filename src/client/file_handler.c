#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>

// Client gửi yêu cầu upload file
void req_upload(int sock, char *group_name, char *filename) {
    char filepath[512];
    sprintf(filepath, "client_storage/%s", filename); // Lấy file từ folder client

    FILE *fp = fopen(filepath, "rb");
    if (!fp) {
        printf("[Client] Error: File %s not found.\n", filepath);
        return;
    }

    // Lấy size
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // 1. Gửi lệnh UPLOAD (Giả lập checksum là "dummy_checksum")
    char cmd[1024];
    sprintf(cmd, "UPLOAD %s \"%s\" %ld dummy_checksum\n", group_name, filename, filesize);
    send(sock, cmd, strlen(cmd), 0);

    // 2. Đợi phản hồi READY
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    response[len] = 0;

    if (strstr(response, "READY_UPLOAD") == NULL) {
        printf("[Client] Server Error: %s", response);
        fclose(fp);
        return;
    }

    // 3. Bắt đầu gửi chunk
    printf("[Client] Uploading...\n");
    char buffer[BUFFER_SIZE];
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        send_chunk(sock, buffer, bytes_read);
    }
    // Gửi chunk kết thúc
    send_chunk(sock, NULL, 0);

    fclose(fp);

    // 4. Đợi kết quả cuối cùng
    len = recv(sock, response, 1023, 0);
    response[len] = 0;
    printf("[Client] Server response: %s", response);
}

// Client gửi yêu cầu download file
void req_download(int sock, char *group_name, char *filename) {
    // 1. Gửi lệnh DOWNLOAD
    char cmd[1024];
    sprintf(cmd, "DOWNLOAD %s \"%s\"\n", group_name, filename);
    send(sock, cmd, strlen(cmd), 0);

    // 2. Nhận phản hồi OK <size>
    char response[1024];
    // Lưu ý: Ở đây cần hàm đọc từng dòng (readline) để tránh đọc lẹm vào binary data
    // Tuy nhiên để đơn giản demo, ta giả định recv nhận đúng dòng lệnh này.
    int len = recv(sock, response, 1023, 0); 
    response[len] = 0;

    if (strncmp(response, "OK 200 DOWNLOAD", 15) != 0) {
        printf("[Client] Download failed: %s", response);
        return;
    }

    printf("[Client] Starting download...\n");

    // 3. Mở file để ghi
    char savepath[512];
    sprintf(savepath, "client_storage/%s", filename);
    FILE *fp = fopen(savepath, "wb");
    
    // 4. Vòng lặp nhận chunk
    char buffer[BUFFER_SIZE];
    int chunk_len;
    long total = 0;
    while ((chunk_len = recv_chunk(sock, buffer)) > 0) {
        fwrite(buffer, 1, chunk_len, fp);
        total += chunk_len;
    }

    fclose(fp);
    printf("[Client] Downloaded %s saved to %s (%ld bytes)\n", filename, savepath, total);
}