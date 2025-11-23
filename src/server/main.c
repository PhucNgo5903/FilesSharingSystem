// File: src/server/main.c
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

// Cập nhật khai báo hàm (Thêm tham số filesize)
void handle_upload(int client_sock, char *group_name, char *filename, long filesize);
void handle_download(int client_sock, char *group_name, char *filename);

#define PORT 8080

// Xử lý zombie process
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// Hàm xử lý logic chính cho từng Client
void handle_client(int client_sock) {
    char buffer[1024];

    while (1) {
        // 1. Nhận lệnh text (kết thúc bằng \n)
        int i = 0;
        char c;
        while (recv(client_sock, &c, 1, 0) > 0) {
            if (c == '\n') break;
            buffer[i++] = c;
            if (i >= sizeof(buffer) - 1) break;
        }
        buffer[i] = '\0';

        if (i == 0) break; // Client đóng kết nối

        // --- SỬA LOG: In nguyên văn lệnh nhận được ---
        printf("%s\n", buffer); 
        // ---------------------------------------------

        // 2. Phân tích lệnh (Parsing)
        char cmd[32], arg1[128], arg2[256], arg3[128], arg4[128];
        // Reset buffers
        memset(cmd, 0, 32); memset(arg1, 0, 128); memset(arg2, 0, 256); 
        memset(arg3, 0, 128); memset(arg4, 0, 128);

        // Format: CMD arg1 arg2 arg3 arg4
        // VD: UPLOAD group "file" size checksum
        sscanf(buffer, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);

        // 3. Điều hướng chức năng
        if (strcmp(cmd, "LOGIN") == 0) {
            char *response = "OK 200 TOKEN dummy_token_123\n";
            send(client_sock, response, strlen(response), 0);
        } 
        else if (strcmp(cmd, "UPLOAD") == 0) {
            // arg1: group, arg2: path, arg3: filesize
            
            // Xử lý bỏ dấu ngoặc kép ở filename nếu có
            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                filename[strlen(filename)-1] = 0;
            }
            
            // Chuyển filesize từ string sang long
            long filesize = atol(arg3);
            
            // Gọi hàm xử lý với tham số filesize mới
            handle_upload(client_sock, arg1, filename, filesize);
        } 
        else if (strcmp(cmd, "DOWNLOAD") == 0) {
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
    exit(0);
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("[-]Socket error");
        exit(1);
    }
    printf("[+]Server TCP socket created.\n");

    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-]Bind error");
        exit(1);
    }
    printf("[+]Bind to port %d\n", PORT);

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

        pid_t pid = fork();
        if (pid == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else {
            close(client_sock);
        }
    }

    return 0;
}