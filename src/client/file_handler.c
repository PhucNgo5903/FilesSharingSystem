#include "../common/protocol.h"
#include "../common/network.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/socket.h>

// Client gửi yêu cầu upload file
void req_upload(int sock, char *group_name, char *path) {
    // 1. Mở file theo đường dẫn đầy đủ người dùng nhập (VD: /mnt/c/Users/Anh/hinh.jpg)
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        printf("[Client] Error: Cannot open file at %s\n", path);
        return;
    }

    // 2. Lấy tên file gốc từ đường dẫn (Tách bỏ phần /mnt/c/...)
    // Ví dụ: /mnt/c/data/image.png -> lấy "image.png"
    char *filename = strrchr(path, '/'); 
    if (filename) {
        filename++; // Bỏ qua dấu gạch chéo '/'
    } else {
        filename = path; // Nếu không có dấu gạch chéo, chính là tên file
    }

    // 3. Lấy kích thước file
    fseek(fp, 0, SEEK_END);
    long filesize = ftell(fp);
    rewind(fp);

    // 4. Gửi lệnh UPLOAD với TÊN FILE GỐC (Clean filename)
    char cmd[1024];
    sprintf(cmd, "UPLOAD %s \"%s\" %ld dummy_checksum\n", group_name, filename, filesize);
    send(sock, cmd, strlen(cmd), 0);

    // 5. Đợi Server phản hồi READY
    char response[1024];
    int len = recv(sock, response, 1023, 0);
    if (len <= 0) { printf("Server disconnected\n"); fclose(fp); return; }
    response[len] = 0;

    if (strstr(response, "READY_UPLOAD") == NULL) {
        printf("[Client] Server Error: %s", response);
        fclose(fp);
        return;
    }

    // 6. Bắt đầu gửi chunk
    printf("[Client] Uploading file: %s (%ld bytes)...\n", filename, filesize);
    char buffer[4096]; // BUFFER_SIZE
    int bytes_read;
    while ((bytes_read = fread(buffer, 1, 4096, fp)) > 0) {
        send_chunk(sock, buffer, bytes_read);
    }
    // Gửi chunk kết thúc
    send_chunk(sock, NULL, 0);

    fclose(fp);

    // 7. Nhận kết quả cuối cùng
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