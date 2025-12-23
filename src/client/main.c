#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../common/protocol.h"
#include "../common/network.h"
#include "auth_handler.h" // Chứa các hàm xử lý đăng nhập/nhóm (Core)
#include "cmd_handler.h"  // Chứa các hàm xử lý logic mới (Filesystem, Invite, Member...)

// Khai báo hàm xử lý file (định nghĩa trong file_handler.c)
void req_upload(int sock, char *destination, char *path);
void req_download(int sock, char *server_path, char *local_destination);

#define SERVER_IP "127.0.0.1"
#define PORT 5555

// Hàm xóa ký tự xuống dòng thừa (Robust version: xóa cả \r và \n)
void trim_newline(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
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
        printf("\n==========================================\n");
        printf("           FILE SHARING CLI               \n");
        printf("==========================================\n");
        
        if (!is_logged_in) {
            // Menu khi chưa đăng nhập
            printf("1. Dang ky (SIGNUP)\n");
            printf("2. Dang nhap (SIGNIN)\n");
            printf("0. Thoat\n");
        } else {
            // Menu khi đã đăng nhập (Full Features)
            printf("User: %s\n", current_user);
            printf("------------------------------------------\n");
            printf("1.  Dang xuat (LOGOUT)\n");
            printf("2.  Tao nhom (MKGRP)\n");
            printf("3.  Liet ke nhom (LSGRP)\n");
            printf("4.  Upload File (UPLOAD)\n");
            printf("5.  Download File (DOWNLOAD)\n");
            printf("------------------------------------------\n");
            printf("6.  Xem thanh vien nhom (LSMEM)\n");
            printf("7.  Yeu cau tham gia nhom (JOIN_REQUEST)\n");
            printf("8.  Xem cac request join (VIEW_REQUEST)\n");
            printf("9.  Duyet request join (APPROVE_REQUEST)\n");
            printf("91. Kiem tra trang thai Join Request (JOIN_REQ_STATUS) [NEW]\n");
            printf("------------------------------------------\n");
            printf("10. Moi vao nhom (INVITE)\n");
            printf("11. Xem loi moi (VIEW_INVITE)\n");
            printf("12. Chap nhan loi moi (ACCEPT_INVITE)\n");
            printf("121. Kiem tra trang thai Invite (INVITE_STATUS) [NEW]\n");
            printf("13. Roi nhom (LEAVE)\n");
            printf("14. Xoa thanh vien khoi nhom (RMMEM)\n");
            printf("------------------------------------------\n");
            printf("15. Liet ke thu muc (LSDIR)\n");
            printf("16. Tao thu muc (MKDIR)\n");
            printf("17. Doi ten thu muc (REDIR)\n");
            printf("18. Di chuyen thu muc (MVDIR)\n");
            printf("19. Copy thu muc (COPDIR)\n");
            printf("20. Xoa thu muc (RMDIR)\n");
            printf("------------------------------------------\n");
            printf("21. Doi ten file (REFILE)\n");
            printf("22. Di chuyen file (MVFILE)\n");
            printf("23. Copy file (COPFILE)\n");
            printf("24. Xoa file (RMFILE)\n");
            printf("0.  Thoat\n");
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

                case 2: req_mkgrp(sock); break;
                case 3: req_lsgrp(sock); break;

                case 4: // Upload File
                    {
                        char destination[128], path[256];
                        printf("Destination (e.g., 'groupA' or 'groupA/docs'): "); 
                        fgets(destination, 128, stdin); trim_newline(destination);
                        printf("File Path: "); 
                        fgets(path, 256, stdin); trim_newline(path);
                        req_upload(sock, destination, path);
                    }
                    break;

                case 5: // Download File
                    {
                        char server_path[256], local_dest[256];
                        printf("Server File Path (e.g., 'groupA/docs/image.png'): "); 
                        fgets(server_path, 256, stdin); trim_newline(server_path);
                        printf("Local Destination (e.g., '/mnt/c/Users/ADMIN/Downloads'): ");
                        fgets(local_dest, 256, stdin); trim_newline(local_dest);
                        if (strlen(local_dest) == 0) strcpy(local_dest, "client_storage/");
                        req_download(sock, server_path, local_dest);
                    }
                    break;

                // --- NEW FEATURES (Maps to cmd_handler.c) ---
                case 6:  handle_lsmem(sock); break;
                case 7:  handle_join_request(sock); break;
                case 8:  handle_view_request(sock); break;
                case 9:  handle_approve_request(sock); break;
                case 91: handle_join_req_status(sock); break;
                case 10: handle_invite(sock); break;
                case 11: handle_view_invite(sock); break;
                case 12: handle_accept_invite(sock); break;
                case 121: handle_invite_status(sock); break;
                case 13: handle_leave(sock); break;
                case 14: handle_rmmem(sock); break;
                case 15: handle_lsdir(sock); break;
                case 16: handle_mkdir(sock); break;
                case 17: handle_redir(sock); break;
                case 18: handle_mvdir(sock); break;
                case 19: handle_copdir(sock); break;
                case 20: handle_rmdir(sock); break;
                case 21: handle_refile(sock); break;
                case 22: handle_mvfile(sock); break;
                case 23: handle_copfile(sock); break;
                case 24: handle_rmfile(sock); break;

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