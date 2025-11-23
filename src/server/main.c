#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>

#include "../common/protocol.h"
#include "../common/network.h"

// Khai báo hàm xử lý (được định nghĩa trong transfer.c)
void handle_upload(int client_sock, char *group_name, char *filename);
void handle_download(int client_sock, char *group_name, char *filename);

#define PORT 8080

// Xử lý zombie process khi child process kết thúc
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Hàm xử lý logic chính cho từng Client
void handle_client(int client_sock) {
    char buffer[1024];
    

    while (1) {
        // 1. Nhận lệnh text (kết thúc bằng \n)
        // Lưu ý: Đọc từng byte cho đến khi gặp \n để tách lệnh chính xác
        int i = 0;
        char c;
        while (recv(client_sock, &c, 1, 0) > 0) {
            if (c == '\n') break;
            buffer[i++] = c;
            if (i >= sizeof(buffer) - 1) break;
        }
        buffer[i] = '\0';

        if (i == 0) break; // Client đóng kết nối

        printf("[Received]: %s\n", buffer);

        // 2. Phân tích lệnh (Parsing)
        char cmd[32], arg1[128], arg2[256], arg3[128], arg4[128];
        // Format: CMD arg1 arg2 ...
        // Reset buffers
        memset(cmd, 0, 32); memset(arg1, 0, 128); memset(arg2, 0, 256);

        // Parse sơ bộ (Lưu ý: Cần parser xịn hơn nếu tên file có dấu cách, ở đây demo đơn giản)
        sscanf(buffer, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);

        // 3. Điều hướng chức năng
        if (strcmp(cmd, "LOGIN") == 0) {
            // Demo: Luôn trả về thành công
            char *response = "OK 200 TOKEN dummy_token_123\n";
            send(client_sock, response, strlen(response), 0);
        } 
        else if (strcmp(cmd, "UPLOAD") == 0) {
            // UPLOAD <group> <path> <size> <checksum>
            // arg1: group, arg2: path (có thể có ngoặc kép cần xử lý thêm)
            
            // Loại bỏ dấu ngoặc kép ở arg2 nếu có (Code demo đơn giản)
            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                filename[strlen(filename)-1] = 0;
            }
            
            handle_upload(client_sock, arg1, filename);
        } 
        else if (strcmp(cmd, "DOWNLOAD") == 0) {
            // DOWNLOAD <group> <path>
            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                filename[strlen(filename)-1] = 0;
            }
            handle_download(client_sock, arg1, filename);
        }
        else {
            char *msg = "ERROR 400 UNKNOWN_COMMAND\n";
            send(client_sock, msg, strlen(msg), 0);
        }
    }

    close(client_sock);
    exit(0); // Kết thúc child process
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    // Xử lý signal để tránh zombie process
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // 1. Tạo socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server TCP socket created.\n");

    // Reuse Port để chạy lại server nhanh sau khi tắt
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    // 2. Bind
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to port %d\n", PORT);

    // 3. Listen
    listen(server_sock, 5);
    printf("[+]Listening...\n");

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }
        
        printf("[+]Client connected.\n");

        // 4. Fork process
        pid_t pid = fork();
        if (pid == 0) {
            // Child process
            close(server_sock); // Child không cần socket lắng nghe
            handle_client(client_sock);
        } else {
            // Parent process
            close(client_sock); // Parent không cần socket kết nối này nữa
        }
    }

    return 0;
}