#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include "../common/protocol.h"
#include "../common/network.h"
#include "auth_handler.h" 
#include "cmd_handler.h"  

// Khai báo hàm xử lý file (từ file_handler.c)
void req_upload(int sock, char *destination, char *path);
void req_download(int sock, char *server_path, char *local_destination);

#define SERVER_IP "127.0.0.1"
#define PORT 5555

// Hàm xóa ký tự xuống dòng thừa
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
    printf("[+] Connected to Server successfully.\n");

    // --- TRẠNG THÁI ĐĂNG NHẬP ---
    int is_logged_in = 0;
    char current_user[64] = "";

    // 3. Vòng lặp chính
    while (1) {
        printf("\n==================================================\n");
        printf("             FILE SHARING SYSTEM (CLI)            \n");
        printf("==================================================\n");
        
        if (!is_logged_in) {
            // Menu khách
            printf("1. Sign Up (SIGNUP)\n");
            printf("2. Sign In (SIGNIN)\n");
            printf("0. Exit\n");
        } else {
            // Menu thành viên (Đã cập nhật lại nhóm Owner)
            printf("User: %s\n", current_user);
            printf("--------------------------------------------------\n");
            printf("1.  Log Out\n");
            
            printf("\n[ GROUP ACTIONS - GENERAL ]\n");
            printf("2.  List All Groups (LSGRP)\n");
            printf("3.  View Group Members (LSMEM)\n");
            printf("4.  Request to Join Group (JOIN_REQUEST)\n");
            printf("5.  Check Join Request Status\n");
            printf("6.  View My Invites (VIEW_INVITE)\n");
            printf("7.  Accept Invite (ACCEPT_INVITE)\n");
            printf("8.  Leave Group (LEAVE)\n");

            printf("\n[ GROUP ACTIONS - OWNER ONLY ]\n");
            printf("9.  Create Group (MKGRP)\n");
            printf("10. Invite to Group (INVITE)\n");
            printf("11. Check Invite Status (INVITE_STATUS)\n");
            printf("12. View Join Requests (VIEW_REQUEST)\n");
            printf("13. Approve Join Request (APPROVE_REQUEST)\n");
            printf("14. Remove Member (RMMEM)\n");

            printf("\n[ DIRECTORY OPERATIONS ]\n");
            printf("15. List Directory (LSDIR)\n");
            printf("16. Create Directory (MKDIR)\n");
            printf("17. Rename Directory (REDIR)\n");
            printf("18. Move Directory (MVDIR)\n");
            printf("19. Copy Directory (COPDIR)\n");
            printf("20. Remove Directory (RMDIR)\n");

            printf("\n[ FILE OPERATIONS ]\n");
            printf("21. Upload File\n");
            printf("22. Download File\n");
            printf("23. Rename File (REFILE)\n");
            printf("24. Move File (MVFILE)\n");
            printf("25. Copy File (COPFILE)\n");
            printf("26. Remove File (RMFILE)\n");
            
            printf("\n0.  Exit\n");
        }
        printf(">> Your choice: ");
        
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
                    // strcpy(current_user, "");
                    printf("Logged out successfully.\n");
                    strcpy(current_user, "");
                    break;

                // --- GROUP GENERAL ---
                case 2: req_lsgrp(sock); break;
                case 3: handle_lsmem(sock); break;
                case 4: handle_join_request(sock); break;
                case 5: handle_join_req_status(sock); break; 
                case 6: handle_view_invite(sock); break;
                case 7: handle_accept_invite(sock); break;
                case 8: handle_leave(sock); break; // LEAVE chuyển lên đây

                // --- GROUP OWNER ---
                case 9:  req_mkgrp(sock); break;
                case 10: handle_invite(sock); break; // INVITE chuyển xuống đây
                case 11: handle_invite_status(sock); break; // INVITE_STATUS chuyển xuống đây
                case 12: handle_view_request(sock); break;
                case 13: handle_approve_request(sock); break;
                case 14: handle_rmmem(sock); break;

                // --- DIRECTORY ---
                case 15: handle_lsdir(sock); break;
                case 16: handle_mkdir(sock); break;
                case 17: handle_redir(sock); break;
                case 18: handle_mvdir(sock); break;
                case 19: handle_copdir(sock); break;
                case 20: handle_rmdir(sock); break;

                // --- FILE & TRANSFER ---
                case 21: // Upload
                    {
                        char destination[128], path[256];
                        printf("Destination (e.g., 'groupA' or 'groupA/docs'): "); 
                        fgets(destination, 128, stdin); trim_newline(destination);
                        printf("Local File Path: "); 
                        fgets(path, 256, stdin); trim_newline(path);
                        req_upload(sock, destination, path);
                    }
                    break;
                case 22: // Download
                    {
                        char server_path[256], local_dest[256];
                        printf("Server File Path (e.g., 'groupA/docs/image.png'): "); 
                        fgets(server_path, 256, stdin); trim_newline(server_path);
                        printf("Local Destination: ");
                        fgets(local_dest, 256, stdin); trim_newline(local_dest);
                        if (strlen(local_dest) == 0) strcpy(local_dest, "client_storage/");
                        req_download(sock, server_path, local_dest);
                    }
                    break;
                case 23: handle_refile(sock); break;
                case 24: handle_mvfile(sock); break;
                case 25: handle_copfile(sock); break;
                case 26: handle_rmfile(sock); break;

                case 0:
                    printf("Exiting...\n");
                    close(sock);
                    return 0;

                default:
                    printf("Invalid choice. Please check the number.\n");
            }
        }
    }

    close(sock);
    return 0;
}