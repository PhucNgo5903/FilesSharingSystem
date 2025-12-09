#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>

#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"

// Includes for Auth and Group Managers
#include "auth_mgr.h"
#include "group_mgr.h"

// Prototype declarations for transfer.c functions
// Updated to accept client_name for logging
void handle_upload(int client_sock, char *group_name, char *filename, long filesize, char *client_name);
void handle_download(int client_sock, char *group_name, char *filename, char *client_name);

#define PORT 5555

// --- MAIN SERVER LOGGING FUNCTION ---
// Logs format: [YYYY-MM-DD HH:MM:SS]  [CLIENT:username] [TYPE]  Message
void server_log_main(const char *client_name, const char *type, const char *format, ...) {
    char time_str[64];
    // This function must be defined in src/common/utils.c
    get_current_time_str(time_str, sizeof(time_str)); 

    printf("%s  [CLIENT:%s] [%s]  ", time_str, client_name, type);

    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);

    printf("\n");
}

// --- ZOMBIE PROCESS HANDLER ---
void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

// --- CLIENT HANDLER LOGIC (CHILD PROCESS) ---
void handle_client(int client_sock) {
    char buffer[1024];
    char client_name[64] = "UNKNOWN"; // Default name before login
    int is_logged_in = 0;             // Login status flag

    while (1) {
        // 1. Receive command (read until newline)
        int i = 0; char c; int n;
        while ((n = recv(client_sock, &c, 1, 0)) > 0) {
            if (c == '\n') break;
            buffer[i++] = c;
            if (i >= sizeof(buffer) - 1) break;
        }
        buffer[i] = '\0';

        if (n <= 0) break; // Client disconnected

        // 2. Parse Command
        char cmd[32], arg1[128], arg2[256], arg3[128], arg4[128];
        memset(cmd, 0, 32); memset(arg1, 0, 128); memset(arg2, 0, 256); 
        memset(arg3, 0, 128); memset(arg4, 0, 128);
        
        // Scan up to 5 arguments
        sscanf(buffer, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);

        // 3. Command Routing

        // --- AUTHENTICATION ---
        if (strcmp(cmd, "SIGNUP") == 0) {
            // SIGNUP <user> <pass>
            server_log_main(client_name, "RECV", "%s", buffer);
            
            if (handle_signup_logic(arg1, arg2)) {
                send(client_sock, "SIGNUP OK\n", 10, 0);
                server_log_main(client_name, "SEND", "SIGNUP OK");
            } else {
                send(client_sock, "SIGNUP ERR_EXIST\n", 17, 0);
                server_log_main(client_name, "SEND", "SIGNUP ERR_EXIST");
            }
        }
        else if (strcmp(cmd, "SIGNIN") == 0) {
            // SIGNIN <user> <pass>
            server_log_main(client_name, "RECV", "%s", buffer);

            if (handle_signin_logic(arg1, arg2)) {
                is_logged_in = 1;
                strncpy(client_name, arg1, sizeof(client_name) - 1); // Update client name
                send(client_sock, "SIGNIN OK\n", 10, 0);
                server_log_main(client_name, "SEND", "SIGNIN OK");
            } else {
                send(client_sock, "SIGNIN ERR_FAIL\n", 16, 0);
                server_log_main(client_name, "SEND", "SIGNIN ERR_FAIL");
            }
        }
        else if (strcmp(cmd, "LOGOUT") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            is_logged_in = 0;
            strcpy(client_name, "UNKNOWN"); // Reset name
            send(client_sock, "LOGOUT OK\n", 10, 0);
            server_log_main(client_name, "SEND", "LOGOUT OK");
        }

        // --- GROUP MANAGEMENT ---
        else if (strcmp(cmd, "MKGRP") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) {
                send(client_sock, "MKGRP ERR_NOT_LOGIN\n", 20, 0);
                server_log_main(client_name, "SEND", "MKGRP ERR_NOT_LOGIN");
            } else {
                // UPDATE: Truyền client_name vào làm trưởng nhóm
                if (handle_mkgrp_logic(arg1, client_name)) {
                    send(client_sock, "MKGRP OK\n", 9, 0);
                    server_log_main(client_name, "SEND", "MKGRP OK");
                } else {
                    send(client_sock, "MKGRP ERR_EXIST\n", 16, 0);
                    server_log_main(client_name, "SEND", "MKGRP ERR_EXIST");
                }
            }
        }
        else if (strcmp(cmd, "LSGRP") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) {
                send(client_sock, "LSGRP ERR_NOT_LOGIN\n", 20, 0);
                server_log_main(client_name, "SEND", "LSGRP ERR_NOT_LOGIN");
            } else {
                char list_resp[1024];
                get_group_list_string(list_resp, sizeof(list_resp));
                send(client_sock, list_resp, strlen(list_resp), 0);
                // Log content is too long, just log status
                server_log_main(client_name, "SEND", "LSGRP_RESULT ..."); 
            }
        }

        // --- FILE TRANSFER ---
        else if (strcmp(cmd, "UPLOAD") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);

            if (!is_logged_in) {
                send(client_sock, "UPLOAD ERR_NOT_LOGIN\n", 21, 0);
                server_log_main(client_name, "SEND", "UPLOAD ERR_NOT_LOGIN");
                continue;
            }

            // CHECK QUYỀN: User có trong Group không?
            if (!check_user_in_group(client_name, arg1)) {
                // arg1 là group_name
                send(client_sock, "UPLOAD ERR_NO_PERMISSION\n", 25, 0);
                server_log_main(client_name, "SEND", "UPLOAD ERR_NO_PERMISSION");
                continue;
            }

            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                if (filename[strlen(filename)-1] == '"') filename[strlen(filename)-1] = 0;
            }
            long filesize = atol(arg3);
            
            handle_upload(client_sock, arg1, filename, filesize, client_name);
        } 
        else if (strcmp(cmd, "DOWNLOAD") == 0) {
            // DOWNLOAD <group> <file>
            char *filename = arg2;
            // Xử lý bỏ dấu ngoặc kép ở tên file nếu có
            if (filename[0] == '"') {
                filename++;
                if (filename[strlen(filename)-1] == '"') filename[strlen(filename)-1] = 0;
            }

            // 1. Kiểm tra đăng nhập
            if (!is_logged_in) {
                server_log_main(client_name, "RECV", "DOWNLOAD %s \"%s\"", arg1, filename);
                send(client_sock, "DOWNLOAD ERR_NOT_LOGIN\n", 23, 0);
                server_log_main(client_name, "SEND", "DOWNLOAD ERR_NOT_LOGIN");
                continue;
            }

            // 2. CHECK QUYỀN: User có trong Group không? (Logic mới bổ sung)
            // arg1 chính là tên nhóm
            if (!check_user_in_group(client_name, arg1)) {
                // Log nhận lệnh nhưng báo thất bại kiểm tra quyền
                server_log_main(client_name, "RECV", "DOWNLOAD %s \"%s\" (Check Permission)", arg1, filename);
                
                // Gửi lỗi về Client
                send(client_sock, "DOWNLOAD ERR_NO_PERMISSION\n", 27, 0);
                
                // Log gửi lỗi
                server_log_main(client_name, "SEND", "DOWNLOAD ERR_NO_PERMISSION (User not in group)");
                continue;
            }

            // 3. Tính toán kích thước file để in log đẹp
            long filesize = 0;
            char filepath[512];
            sprintf(filepath, "storage/%s/%s", arg1, filename);
            FILE *f_check = fopen(filepath, "rb");
            if (f_check) {
                fseek(f_check, 0, SEEK_END);
                filesize = ftell(f_check);
                fclose(f_check);
            }

            // 4. Log lệnh hợp lệ (kèm filesize)
            server_log_main(client_name, "RECV", "DOWNLOAD %s \"%s\" %ld", arg1, filename, filesize);
            
            // 5. Chuyển sang module transfer để xử lý gửi file
            handle_download(client_sock, arg1, filename, client_name);
        }
        else {
            // Unknown command
            server_log_main(client_name, "RECV", "%s", buffer);
            char *msg = "ERROR 400 UNKNOWN_COMMAND\n";
            send(client_sock, msg, strlen(msg), 0);
            server_log_main(client_name, "SEND", "ERROR 400 UNKNOWN_COMMAND");
        }
    }
    
    close(client_sock);
    exit(0);
}

// --- MAIN LOOP ---
int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;

    // Signal handling for zombies
    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGCHLD, &sa, NULL);

    // Create Socket
    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("[-]Socket error"); exit(1); }
    
    // Reuse Port
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // Bind
    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-]Bind error"); exit(1);
    }
    
    // Listen
    if (listen(server_sock, 10) == 0) printf("[SYSTEM] Server listening on port %d...\n", PORT);
    else { perror("[-]Listen error"); exit(1); }

    // Accept Loop
    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        
        if (client_sock < 0) continue;
        
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[SYSTEM] New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));

        // Fork Process
        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else {
            close(client_sock);
        }
    }
    return 0;
}