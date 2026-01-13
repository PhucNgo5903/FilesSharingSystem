#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h> 

#include "../common/protocol.h"
#include "../common/network.h"
#include "../common/utils.h"

#include "auth_mgr.h"
#include "group_mgr.h"
#include "fs_mgr.h"

// Prototype cho transfer.c
void handle_upload(int client_sock, char *destination, char *filename, long filesize, char *client_name);
void handle_download(int client_sock, char *server_path, char *client_name);

#define PORT 5555

// --- LOGGING ---
// Format: [dd-mm-yyyy HH:MM:SS] [CLIENT: username] [TYPE] message_content
void server_log_main(const char *client_name, const char *type, const char *format, ...) {
    char ts[64];
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    strftime(ts, sizeof(ts), "%d-%m-%Y %H:%M:%S", &tmv);

    char msg[4096];
    va_list args;
    va_start(args, format);
    vsnprintf(msg, sizeof(msg), format, args);
    va_end(args);

    // Cắt bỏ ký tự xuống dòng ở cuối để log hiển thị đẹp trên 1 dòng
    size_t len = strlen(msg);
    while (len > 0 && (msg[len-1] == '\n' || msg[len-1] == '\r')) {
        msg[--len] = '\0';
    }

    printf("[%s] [CLIENT: %s] [%s] %s\n", ts, client_name, type, msg);
    fflush(stdout); 
}

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void trim_str(char *str) {
    int n = strlen(str);
    while (n > 0 && (str[n-1] == '\n' || str[n-1] == '\r')) {
        str[n-1] = '\0';
        n--;
    }
}

void handle_client(int client_sock) {
    char buffer[4096];
    char client_name[64] = "UNKNOWN";
    int is_logged_in = 0;

    while (1) {
        int i = 0; char c; int n;
        while ((n = recv(client_sock, &c, 1, 0)) > 0) {
            if (c == '\n') break;
            buffer[i++] = c;
            if (i >= sizeof(buffer) - 1) break;
        }
        buffer[i] = '\0';

        if (n <= 0) break; 
        trim_str(buffer);

        char cmd[32], arg1[128], arg2[256], arg3[128], arg4[128];
        memset(cmd, 0, 32); memset(arg1, 0, 128); memset(arg2, 0, 256); 
        memset(arg3, 0, 128); memset(arg4, 0, 128);
        
        sscanf(buffer, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);
        trim_str(cmd); trim_str(arg1); trim_str(arg2); trim_str(arg3); trim_str(arg4);

        // ================== AUTHENTICATION ==================
        if (strcmp(cmd, "SIGNUP") == 0) {
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
            server_log_main(client_name, "RECV", "%s", buffer);
            if (handle_signin_logic(arg1, arg2)) {
                is_logged_in = 1;
                strncpy(client_name, arg1, sizeof(client_name) - 1);
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
            strcpy(client_name, "UNKNOWN");
            send(client_sock, "LOGOUT OK\n", 10, 0);
            server_log_main(client_name, "SEND", "LOGOUT OK");
        }

        // ================== CORE GROUP ==================
        else if (strcmp(cmd, "MKGRP") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "MKGRP ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "MKGRP ERR_NOT_LOGIN");
                continue; 
            }
            
            if (handle_mkgrp_logic(arg1, client_name)) {
                send(client_sock, "MKGRP OK\n", 9, 0);
                server_log_main(client_name, "SEND", "MKGRP OK");
            } else {
                send(client_sock, "MKGRP ERR_EXIST\n", 16, 0);
                server_log_main(client_name, "SEND", "MKGRP ERR_EXIST");
            }
        }
        else if (strcmp(cmd, "LSGRP") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "LSGRP ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "LSGRP ERR_NOT_LOGIN");
                continue; 
            }
            
            char list_resp[4096];
            get_group_list_string(list_resp, sizeof(list_resp));
            send(client_sock, list_resp, strlen(list_resp), 0);
            
            // --- SỬA ĐỔI: Log chính xác chuỗi gửi đi (LSGRP OK ...) ---
            server_log_main(client_name, "SEND", "%s", list_resp);
        }

        // ================== MEMBER MANAGEMENT ==================
        else if (strcmp(cmd, "LSMEM") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            char *group_name = arg1; // Tên nhóm gửi lên
            char result[BUFSIZ];
            // Hàm này của bạn đã viết đúng, nó sẽ lấy toàn bộ thành viên
            get_group_members_string(group_name, result, sizeof(result));
            
            send(client_sock, result, strlen(result), 0);
            server_log_main(client_name, "SEND", "%s", result);
        }

        else if (strcmp(cmd, "LEAVE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "LEAVE ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "LEAVE ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "LEAVE ERR_NOT_IN\n", 17, 0); 
                server_log_main(client_name, "SEND", "LEAVE ERR_NOT_IN");
                continue; 
            }
            if (is_group_owner(arg1, client_name)) { 
                send(client_sock, "LEAVE ERR_OWNER\n", 16, 0); 
                server_log_main(client_name, "SEND", "LEAVE ERR_OWNER");
                continue; 
            }

            if (remove_member_from_group(arg1, client_name)) {
                send(client_sock, "LEAVE OK\n", 9, 0);
                server_log_main(client_name, "SEND", "LEAVE OK");
            } else {
                send(client_sock, "LEAVE ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "LEAVE ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "RMMEM") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "RMMEM ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "RMMEM ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "RMMEM ERR_NO_PERM\n", 18, 0); 
                server_log_main(client_name, "SEND", "RMMEM ERR_NO_PERM");
                continue; 
            }
            if (is_group_owner(arg1, arg2)) { 
                send(client_sock, "RMMEM ERR_OWNER\n", 16, 0); 
                server_log_main(client_name, "SEND", "RMMEM ERR_OWNER");
                continue; 
            }

            if (remove_member_from_group(arg1, arg2)) {
                send(client_sock, "RMMEM OK\n", 9, 0);
                server_log_main(client_name, "SEND", "RMMEM OK");
            } else {
                send(client_sock, "RMMEM ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "RMMEM ERR_INTERNAL");
            }
        }

        // ================== REQUEST SYSTEM ==================
        else if (strcmp(cmd, "JOIN_REQUEST") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "JOIN_REQUEST ERR_NOT_LOGIN\n", 27, 0); 
                server_log_main(client_name, "SEND", "JOIN_REQUEST ERR_NOT_LOGIN");
                continue; 
            }
            if (check_user_in_group(client_name, arg1)) { 
                send(client_sock, "JOIN_REQUEST ERR_ALREADY_IN\n", 28, 0); 
                server_log_main(client_name, "SEND", "JOIN_REQUEST ERR_ALREADY_IN");
                continue; 
            }
            
            if (add_join_request(arg1, client_name)) {
                send(client_sock, "JOIN_REQUEST OK\n", 16, 0);
                server_log_main(client_name, "SEND", "JOIN_REQUEST OK");
            } else {
                send(client_sock, "JOIN_REQUEST ERR_INTERNAL\n", 26, 0);
                server_log_main(client_name, "SEND", "JOIN_REQUEST ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "VIEW_REQUEST") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "VIEW_REQUEST ERR_NOT_LOGIN\n", 27, 0); 
                server_log_main(client_name, "SEND", "VIEW_REQUEST ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "VIEW_REQUEST ERR_NO_PERM\n", 25, 0); 
                server_log_main(client_name, "SEND", "VIEW_REQUEST ERR_NO_PERM");
                continue; 
            }

            char resp[4096];
            build_view_request_response(arg1, resp, sizeof(resp));
            send(client_sock, resp, strlen(resp), 0);
            
            // --- SỬA ĐỔI: Log chính xác chuỗi gửi đi ---
            server_log_main(client_name, "SEND", "%s", resp);
        }
        else if (strcmp(cmd, "APPROVE_REQUEST") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "APPROVE_REQUEST ERR_NOT_LOGIN\n", 30, 0); 
                server_log_main(client_name, "SEND", "APPROVE_REQUEST ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "APPROVE_REQUEST ERR_NO_PERM\n", 28, 0); 
                server_log_main(client_name, "SEND", "APPROVE_REQUEST ERR_NO_PERM");
                continue; 
            }

            if (approve_join_request(arg1, arg2)) {
                send(client_sock, "APPROVE_REQUEST OK\n", 19, 0);
                server_log_main(client_name, "SEND", "APPROVE_REQUEST OK");
            } else {
                send(client_sock, "APPROVE_REQUEST ERR_REQ_NOT_FOUND\n", 34, 0);
                server_log_main(client_name, "SEND", "APPROVE_REQUEST ERR_REQ_NOT_FOUND");
            }
        }

        // ================== INVITE SYSTEM ==================
        else if (strcmp(cmd, "INVITE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "INVITE ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "INVITE ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "INVITE ERR_NO_PERM\n", 19, 0); 
                server_log_main(client_name, "SEND", "INVITE ERR_NO_PERM");
                continue; 
            }
            if (check_user_in_group(arg2, arg1)) { 
                send(client_sock, "INVITE ERR_ALREADY_IN\n", 22, 0); 
                server_log_main(client_name, "SEND", "INVITE ERR_ALREADY_IN");
                continue; 
            }

            if (add_invite(arg2, arg1)) {
                send(client_sock, "INVITE OK\n", 10, 0);
                server_log_main(client_name, "SEND", "INVITE OK");
            } else {
                send(client_sock, "INVITE ERR_INTERNAL\n", 20, 0);
                server_log_main(client_name, "SEND", "INVITE ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "VIEW_INVITE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "VIEW_INVITE ERR_NOT_LOGIN\n", 26, 0); 
                server_log_main(client_name, "SEND", "VIEW_INVITE ERR_NOT_LOGIN");
                continue; 
            }
            
            char resp[4096];
            build_view_invite_response(client_name, resp, sizeof(resp));
            send(client_sock, resp, strlen(resp), 0);
            
            // --- SỬA ĐỔI: Log chính xác chuỗi gửi đi ---
            server_log_main(client_name, "SEND", "%s", resp);
        }
        else if (strcmp(cmd, "ACCEPT_INVITE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "ACCEPT_INVITE ERR_NOT_LOGIN\n", 28, 0); 
                server_log_main(client_name, "SEND", "ACCEPT_INVITE ERR_NOT_LOGIN");
                continue; 
            }

            if (accept_invite(client_name, arg1)) {
                send(client_sock, "ACCEPT_INVITE OK\n", 17, 0);
                server_log_main(client_name, "SEND", "ACCEPT_INVITE OK");
            } else {
                send(client_sock, "ACCEPT_INVITE ERR_INVITE_NOT_FOUND\n", 35, 0);
                server_log_main(client_name, "SEND", "ACCEPT_INVITE ERR_INVITE_NOT_FOUND");
            }
        }

        // ================== FILESYSTEM COMMANDS ==================
        else if (strcmp(cmd, "MKDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "MKDIR ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "MKDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "MKDIR ERR_NOT_IN\n", 17, 0); 
                server_log_main(client_name, "SEND", "MKDIR ERR_NOT_IN");
                continue; 
            }

            if (handle_mkdir(arg1, arg2)) {
                send(client_sock, "MKDIR OK\n", 9, 0);
                server_log_main(client_name, "SEND", "MKDIR OK");
            } else {
                send(client_sock, "MKDIR ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "MKDIR ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "LSDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "LSDIR ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "LSDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "LSDIR ERR_NOT_IN\n", 17, 0); 
                server_log_main(client_name, "SEND", "LSDIR ERR_NOT_IN");
                continue; 
            }

            char resp[4096];
            if (handle_lsdir(arg1, arg2, resp, sizeof(resp))) {
                send(client_sock, resp, strlen(resp), 0);
                // --- SỬA ĐỔI: Log chính xác chuỗi gửi đi (LSDIR OK ...) ---
                server_log_main(client_name, "SEND", "%s", resp);
            } else {
                send(client_sock, "LSDIR ERR_DIR_NOT_FOUND\n", 24, 0);
                server_log_main(client_name, "SEND", "LSDIR ERR_DIR_NOT_FOUND");
            }
        }
        else if (strcmp(cmd, "RMDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "RMDIR ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "RMDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "RMDIR ERR_NO_PERM\n", 18, 0); 
                server_log_main(client_name, "SEND", "RMDIR ERR_NO_PERM");
                continue; 
            }

            if (handle_rmdir(arg1, arg2)) {
                send(client_sock, "RMDIR OK\n", 9, 0);
                server_log_main(client_name, "SEND", "RMDIR OK");
            } else {
                send(client_sock, "RMDIR ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "RMDIR ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "COPDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "COPDIR ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "COPDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "COPDIR ERR_NOT_IN\n", 18, 0); 
                server_log_main(client_name, "SEND", "COPDIR ERR_NOT_IN");
                continue; 
            }

            if (handle_copdir(arg1, arg2, arg3)) {
                send(client_sock, "COPDIR OK\n", 10, 0);
                server_log_main(client_name, "SEND", "COPDIR OK");
            } else {
                send(client_sock, "COPDIR ERR_INTERNAL\n", 20, 0);
                server_log_main(client_name, "SEND", "COPDIR ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "MVDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "MVDIR ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "MVDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "MVDIR ERR_NOT_IN\n", 17, 0); 
                server_log_main(client_name, "SEND", "MVDIR ERR_NOT_IN");
                continue; 
            }

            if (handle_mvdir(arg1, arg2, arg3)) {
                send(client_sock, "MVDIR OK\n", 9, 0);
                server_log_main(client_name, "SEND", "MVDIR OK");
            } else {
                send(client_sock, "MVDIR ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "MVDIR ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "REDIR") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "REDIR ERR_NOT_LOGIN\n", 20, 0); 
                server_log_main(client_name, "SEND", "REDIR ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "REDIR ERR_NO_PERM\n", 18, 0); 
                server_log_main(client_name, "SEND", "REDIR ERR_NO_PERM");
                continue; 
            }

            if (handle_redir(arg1, arg2, arg3)) {
                send(client_sock, "REDIR OK\n", 9, 0);
                server_log_main(client_name, "SEND", "REDIR OK");
            } else {
                send(client_sock, "REDIR ERR_INTERNAL\n", 19, 0);
                server_log_main(client_name, "SEND", "REDIR ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "REFILE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "REFILE ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "REFILE ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "REFILE ERR_NO_PERM\n", 19, 0); 
                server_log_main(client_name, "SEND", "REFILE ERR_NO_PERM");
                continue; 
            }

            if (handle_refile(arg1, arg2, arg3)) {
                send(client_sock, "REFILE OK\n", 10, 0);
                server_log_main(client_name, "SEND", "REFILE OK");
            } else {
                send(client_sock, "REFILE ERR_INTERNAL\n", 20, 0);
                server_log_main(client_name, "SEND", "REFILE ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "MVFILE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "MVFILE ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "MVFILE ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "MVFILE ERR_NOT_IN\n", 18, 0); 
                server_log_main(client_name, "SEND", "MVFILE ERR_NOT_IN");
                continue; 
            }

            if (handle_mvfile(arg1, arg2, arg3)) {
                send(client_sock, "MVFILE OK\n", 10, 0);
                server_log_main(client_name, "SEND", "MVFILE OK");
            } else {
                send(client_sock, "MVFILE ERR_INTERNAL\n", 20, 0);
                server_log_main(client_name, "SEND", "MVFILE ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "COPFILE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "COPFILE ERR_NOT_LOGIN\n", 22, 0); 
                server_log_main(client_name, "SEND", "COPFILE ERR_NOT_LOGIN");
                continue; 
            }
            if (!check_user_in_group(client_name, arg1)) { 
                send(client_sock, "COPFILE ERR_NOT_IN\n", 19, 0); 
                server_log_main(client_name, "SEND", "COPFILE ERR_NOT_IN");
                continue; 
            }

            if (handle_copfile(arg1, arg2, arg3)) {
                send(client_sock, "COPFILE OK\n", 11, 0);
                server_log_main(client_name, "SEND", "COPFILE OK");
            } else {
                send(client_sock, "COPFILE ERR_INTERNAL\n", 21, 0);
                server_log_main(client_name, "SEND", "COPFILE ERR_INTERNAL");
            }
        }
        else if (strcmp(cmd, "RMFILE") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "RMFILE ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "RMFILE ERR_NOT_LOGIN");
                continue; 
            }
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "RMFILE ERR_NO_PERM\n", 19, 0); 
                server_log_main(client_name, "SEND", "RMFILE ERR_NO_PERM");
                continue; 
            }

            if (handle_rmfile(arg1, arg2)) {
                send(client_sock, "RMFILE OK\n", 10, 0);
                server_log_main(client_name, "SEND", "RMFILE OK");
            } else {
                send(client_sock, "RMFILE ERR_INTERNAL\n", 20, 0);
                server_log_main(client_name, "SEND", "RMFILE ERR_INTERNAL");
            }
        }

        else if (strcmp(cmd, "JOIN_REQ_STATUS") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "STATUS ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "STATUS ERR_NOT_LOGIN");
                continue; 
            }

            // Gọi hàm mới xử lý danh sách
            char resp[4096];
            build_join_req_status_all_response(client_name, resp, sizeof(resp));
            
            send(client_sock, resp, strlen(resp), 0);
            server_log_main(client_name, "SEND", "%s", resp); // Log danh sách trả về
        }

        else if (strcmp(cmd, "INVITE_STATUS") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "STATUS ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "STATUS ERR_NOT_LOGIN");
                continue; 
            }
            
            if (!is_group_owner(arg1, client_name)) { 
                send(client_sock, "STATUS ERR_NO_PERM\n", 19, 0); 
                server_log_main(client_name, "SEND", "STATUS ERR_NO_PERM");
                continue; 
            }

            char resp[4096];
            build_invite_status_all_response(arg1, resp, sizeof(resp));
            send(client_sock, resp, strlen(resp), 0);
            
            // --- SỬA ĐỔI: Log chính xác chuỗi gửi đi ---
            server_log_main(client_name, "SEND", "%s", resp);
        }

        // ================== FILE TRANSFER (UPLOAD/DOWNLOAD) ==================
        else if (strcmp(cmd, "UPLOAD") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            if (!is_logged_in) { 
                send(client_sock, "UPLOAD ERR_NOT_LOGIN\n", 21, 0); 
                server_log_main(client_name, "SEND", "UPLOAD ERR_NOT_LOGIN");
                continue; 
            }

            char *filename = arg1;
            if (filename[0] == '"') { filename++; if (filename[strlen(filename)-1] == '"') filename[strlen(filename)-1] = 0; }
            
            char *destination = arg2; 
            long filesize = atol(arg3);

            char group_name[64];
            char temp_dest[256];
            strcpy(temp_dest, destination);
            char *token = strtok(temp_dest, "/"); 
            if (token != NULL) strcpy(group_name, token);
            else strcpy(group_name, destination);

            if (!check_user_in_group(client_name, group_name)) {
                send(client_sock, "UPLOAD ERR_NO_PERMISSION\n", 25, 0);
                server_log_main(client_name, "SEND", "UPLOAD ERR_NO_PERMISSION");
                continue;
            }
            handle_upload(client_sock, destination, filename, filesize, client_name);
        } 
        
        else if (strcmp(cmd, "DOWNLOAD") == 0) {
            char *server_path = arg1;
            if (server_path[0] == '"') { server_path++; if (server_path[strlen(server_path)-1] == '"') server_path[strlen(server_path)-1] = 0; }

            if (!is_logged_in) {
                server_log_main(client_name, "RECV", "DOWNLOAD %s", server_path);
                send(client_sock, "DOWNLOAD ERR_NOT_LOGIN\n", 23, 0);
                server_log_main(client_name, "SEND", "DOWNLOAD ERR_NOT_LOGIN");
                continue;
            }

            char group_name[64];
            char temp_path[256];
            strcpy(temp_path, server_path);
            char *token = strtok(temp_path, "/");
            if (token != NULL) strcpy(group_name, token);
            else strcpy(group_name, server_path);

            if (!check_user_in_group(client_name, group_name)) {
                server_log_main(client_name, "RECV", "DOWNLOAD %s (Check Permission)", server_path);
                send(client_sock, "DOWNLOAD ERR_NO_PERMISSION\n", 27, 0);
                server_log_main(client_name, "SEND", "DOWNLOAD ERR_NO_PERMISSION");
                continue;
            }

            long filesize = 0;
            char full_path[1024];
            sprintf(full_path, "storage/%s", server_path);

            FILE *f_check = fopen(full_path, "rb");
            if (f_check) {
                fseek(f_check, 0, SEEK_END);
                filesize = ftell(f_check);
                fclose(f_check);
            } else {
                server_log_main(client_name, "RECV", "DOWNLOAD %s (File Not Found)", server_path);
                send(client_sock, "FILE_NOT_FOUND 404\n", 19, 0);
                server_log_main(client_name, "SEND", "FILE_NOT_FOUND 404");
                continue;
            }

            server_log_main(client_name, "RECV", "DOWNLOAD %s %ld", server_path, filesize);
            handle_download(client_sock, server_path, client_name);
        }
        else {
            server_log_main(client_name, "RECV", "%s", buffer);
            char *msg = "ERROR 400 UNKNOWN_COMMAND\n";
            send(client_sock, msg, strlen(msg), 0);
            server_log_main(client_name, "SEND", "ERROR 400 UNKNOWN_COMMAND");
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

    ensure_group_dirs(); 
    ensure_fs_dirs();

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) { perror("[-]Socket error"); exit(1); }
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&server_addr, '\0', sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("[-]Bind error"); exit(1);
    }
    if (listen(server_sock, 10) == 0) printf("[SYSTEM] Server listening on port %d...\n", PORT);
    else { perror("[-]Listen error"); exit(1); }

    while (1) {
        addr_size = sizeof(client_addr);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) continue;
        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        printf("[SYSTEM] New connection from %s:%d\n", client_ip, ntohs(client_addr.sin_port));
        if (fork() == 0) {
            close(server_sock);
            handle_client(client_sock);
        } else {
            close(client_sock);
        }
    }
    return 0;
}