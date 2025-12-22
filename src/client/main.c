#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../common/protocol.h"
#include "../common/network.h"
#include "auth_handler.h" // Chứa các hàm xử lý đăng nhập/nhóm

// Khai báo hàm xử lý file (định nghĩa trong file_handler.c)
void req_upload(int sock, char *group_name, char *path);
void req_download(int sock, char *group_name, char *filename, char *dest_folder);

#define SERVER_IP "127.0.0.1"
#define PORT 5555

// Hàm xóa ký tự xuống dòng thừa
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

    // --- QUẢN LÝ TRẠNG THÁI ĐĂNG NHẬP ---
    int is_logged_in = 0;
    char current_user[64] = "";

    // 3. Vòng lặp chính (Main Loop)
    while (1) {
        printf("\n=============================\n");
        printf("      FILE SHARING CLI       \n");
        printf("=============================\n");
        
        if (!is_logged_in) {
            // Menu khi chưa đăng nhập
            printf("1. Signup (Dang ky)\n");
            printf("2. Signin (Dang nhap)\n");
            printf("0. Exit\n");
        } else {
            // Menu khi đã đăng nhập
            printf("User: %s\n", current_user);
            printf("-----------------------------\n");
            printf("1. Logout (Dang xuat)\n");
            printf("2. Create Group (Tao nhom)\n");
            printf("3. List Groups (DS nhom)\n");
            printf("4. Upload File\n");
            printf("5. Download File\n");
            printf("0. Exit\n");
        }
        printf("Your choice: ");
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        int choice = atoi(input);

        // --- XỬ LÝ SỰ KIỆN ---
        if (!is_logged_in) {
            switch (choice) {
                case 1: 
                    req_signup(sock); 
                    break;
                case 2: 
                    // req_signin trả về 1 nếu thành công, 0 nếu thất bại
                    if (req_signin(sock, current_user)) {
                        is_logged_in = 1;
                    }
                    break;
                case 0:
                    printf("Exiting...\n");
                    close(sock);
                    return 0;
                default:
                    printf("Invalid choice.\n");
            }
        } 
        else {
            switch (choice) {
                case 1: // Logout
                    req_logout(sock);
                    is_logged_in = 0;
                    strcpy(current_user, "");
                    break;

                case 2: // Create Group
                    req_mkgrp(sock);
                    break;

                case 3: // List Groups
                    req_lsgrp(sock);
                    break;

                case 4: // Upload File
                    {
                        char destination[128], path[256];
                        
                        // Đổi câu nhắc để rõ ràng hơn
                        printf("Destination (e.g., 'grouptest3' or 'grouptest3/docs'): "); 
                        fgets(destination, 128, stdin); trim_newline(destination);
                        
                        printf("File Path: "); 
                        fgets(path, 256, stdin); trim_newline(path);
                        
                        req_upload(sock, destination, path);
                    }
                    break;

                case 5: // Download File
                    {
                        char group[50], filename[100], dest[256];
                        
                        printf("Source Group: "); 
                        fgets(group, 50, stdin); trim_newline(group);
                        
                        printf("Filename: "); 
                        fgets(filename, 100, stdin); trim_newline(filename);
                        
                        printf("Save to Folder (e.g., /mnt/c/Users/ADMIN/Downloads): ");
                        fgets(dest, 256, stdin); trim_newline(dest);

                        if (strlen(dest) == 0) {
                            strcpy(dest, "client_storage");
                        }
                        
                        req_download(sock, group, filename, dest);
                    }
                    break;

                case 0:
                    printf("Exiting...\n");
                    close(sock);
                    return 0;

                default:
                    printf("Invalid choice.\n");
            }
        }
    }

    close(sock);
    return 0;
}