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

// Cập nhật Prototype: thêm char *client_name vào cuối
void handle_upload(int client_sock, char *group_name, char *filename, long filesize, char *client_name);
void handle_download(int client_sock, char *group_name, char *filename, char *client_name);

#define PORT 5555

void server_log_main(const char *client_name, const char *type, const char *format, ...) {
    char time_str[64];
    get_current_time_str(time_str, sizeof(time_str));
    printf("%s  [CLIENT:%s] [%s]  ", time_str, client_name, type);
    va_list args;
    va_start(args, format);
    vprintf(format, args);
    va_end(args);
    printf("\n");
}

void sigchld_handler(int sig) {
    while (waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_client(int client_sock) {
    char buffer[1024];
    char client_name[64] = "UNKNOWN";

    while (1) {
        int i = 0; char c; int n;
        while ((n = recv(client_sock, &c, 1, 0)) > 0) {
            if (c == '\n') break;
            buffer[i++] = c;
            if (i >= sizeof(buffer) - 1) break;
        }
        buffer[i] = '\0';

        if (n <= 0) break; 

        // Đã bỏ log chung RECV ở đây

        char cmd[32], arg1[128], arg2[256], arg3[128], arg4[128];
        memset(cmd, 0, 32); memset(arg1, 0, 128); memset(arg2, 0, 256); 
        memset(arg3, 0, 128); memset(arg4, 0, 128);
        sscanf(buffer, "%s %s %s %s %s", cmd, arg1, arg2, arg3, arg4);

        if (strcmp(cmd, "LOGIN") == 0) {
            server_log_main(client_name, "RECV", "%s", buffer);
            char *response = "OK 200 TOKEN dummy_token_123\n";
            send(client_sock, response, strlen(response), 0);
            strncpy(client_name, arg1, sizeof(client_name) - 1);
            server_log_main(client_name, "SEND", "OK 200 TOKEN dummy_token_123");
        } 
        else if (strcmp(cmd, "UPLOAD") == 0) {
            // Log RECV bản tin gốc
            server_log_main(client_name, "RECV", "%s", buffer);

            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                if (filename[strlen(filename)-1] == '"') filename[strlen(filename)-1] = 0;
            }
            long filesize = atol(arg3);
            
            // Gọi hàm upload mới (thêm client_name)
            handle_upload(client_sock, arg1, filename, filesize, client_name);
        } 
        else if (strcmp(cmd, "DOWNLOAD") == 0) {
            char *filename = arg2;
            if (filename[0] == '"') {
                filename++;
                if (filename[strlen(filename)-1] == '"') filename[strlen(filename)-1] = 0;
            }

            // Tính size để log đẹp
            long filesize = 0;
            char filepath[512];
            sprintf(filepath, "storage/%s/%s", arg1, filename);
            FILE *f_check = fopen(filepath, "rb");
            if (f_check) {
                fseek(f_check, 0, SEEK_END);
                filesize = ftell(f_check);
                fclose(f_check);
            }

            // Log RECV bản tin gốc kèm size
            server_log_main(client_name, "RECV", "DOWNLOAD %s \"%s\" %ld", arg1, filename, filesize);
            
            // Gọi hàm download mới (thêm client_name)
            handle_download(client_sock, arg1, filename, client_name);
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