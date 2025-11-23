#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../common/protocol.h"
#include "../common/network.h"

// Khai báo hàm xử lý (định nghĩa trong file_handler.c)
void req_upload(int sock, char *group_name, char *filename);
void req_download(int sock, char *group_name, char *filename);

#define SERVER_IP "127.0.0.1"
#define PORT 8080

// Hàm xóa ký tự xuống dòng thừa khi dùng fgets
void trim_newline(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') {
        str[len-1] = '\0';
    }
}

int main() {
    int sock;
    struct sockaddr_in addr;
    char buffer[1024];
    char input[256];

    // 1. Tạo socket
    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("[-]Socket error");
        exit(1);
    }

    memset(&addr, '\0', sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = inet_addr(SERVER_IP);

    // 2. Kết nối
    if (connect(sock, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("[-]Connection error");
        exit(1);
    }
    printf("[+]Connected to Server.\n");

    // 3. Vòng lặp chính (Main Loop)
    int is_running = 1;
    while (is_running) {
        printf("\n--- FILE SHARING CLI ---\n");
        printf("1. Login\n");
        printf("2. Upload File\n");
        printf("3. Download File\n");
        printf("4. Exit\n");
        printf("Your choice: ");
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        int choice = atoi(input);

        switch (choice) {
            case 1: // Login
                {
                    char user[50], pass[50];
                    printf("Username: "); fgets(user, 50, stdin); trim_newline(user);
                    printf("Password: "); fgets(pass, 50, stdin); trim_newline(pass);
                    
                    sprintf(buffer, "LOGIN %s %s\n", user, pass);
                    send(sock, buffer, strlen(buffer), 0);
                    
                    // Nhận phản hồi
                    int n = recv(sock, buffer, sizeof(buffer)-1, 0);
                    buffer[n] = 0;
                    printf("Server: %s", buffer);
                }
                break;
                
            case 2: // Upload
                {
                    char group[50], path[256]; // Tăng size path lên để chứa đường dẫn dài
                    printf("Target Group: "); fgets(group, 50, stdin); trim_newline(group);
                    
                    // Sửa dòng thông báo này:
                    printf("File Path (e.g., /mnt/c/Users/Name/Photo.jpg): "); 
                    fgets(path, 256, stdin); trim_newline(path);
                    
                    req_upload(sock, group, path);
                }
                break;

            case 3: // Download
                {
                    char group[50], path[100];
                    printf("Source Group: "); fgets(group, 50, stdin); trim_newline(group);
                    printf("Filename: "); fgets(path, 100, stdin); trim_newline(path);
                    
                    // Gọi hàm xử lý trong file_handler.c
                    req_download(sock, group, path);
                }
                break;

            case 4:
                printf("Exiting...\n");
                is_running = 0;
                break;
                
            default:
                printf("Invalid choice.\n");
        }
    }

    close(sock);
    return 0;
}