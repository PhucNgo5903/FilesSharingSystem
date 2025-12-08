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
void req_download(int sock, char *group_name, char *filename, char *dest_folder);

#define SERVER_IP "127.0.0.1"
#define PORT 5555

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
        printf("1. Upload File\n");
        printf("2. Download File\n");
        printf("3. Exit\n");
        printf("Your choice: ");
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        int choice = atoi(input);

        switch (choice) {
            
            case 1: // Upload
                {
                    char group[50], path[256]; // Tăng size path lên để chứa đường dẫn dài
                    printf("Target Group: "); fgets(group, 50, stdin); trim_newline(group);
                    
                    // Sửa dòng thông báo này:
                    printf("File Path (e.g., /mnt/c/Users/Name/Photo.jpg): "); 
                    fgets(path, 256, stdin); trim_newline(path);
                    
                    req_upload(sock, group, path);
                }
                break;

            case 2: // Download
                {
                    char group[50], filename[100], dest[256];
                    
                    printf("Source Group: "); 
                    fgets(group, 50, stdin); trim_newline(group);
                    
                    printf("Filename: "); 
                    fgets(filename, 100, stdin); trim_newline(filename);
                    
                    // Hỏi người dùng nơi lưu file
                    printf("Save to Folder (e.g., /mnt/c/Users/ADMIN/Downloads): ");
                    fgets(dest, 256, stdin); trim_newline(dest);

                    // Nếu người dùng không nhập gì, có thể set mặc định (tùy chọn)
                    if (strlen(dest) == 0) {
                        strcpy(dest, "client_storage"); // Mặc định cũ nếu lười nhập
                    }
                    
                    // Gọi hàm với tham số mới
                    req_download(sock, group, filename, dest);
                }
                break;

            case 3:
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