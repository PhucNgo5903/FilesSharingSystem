#include "auth_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>

void trim_nl(char *str) {
    int len = strlen(str);
    if (len > 0 && str[len-1] == '\n') str[len-1] = '\0';
}

void req_signup(int sock) {
    char u[32], p[32], buf[256];
    
    printf("Enter new Username: "); 
    if (fgets(u, 32, stdin) == NULL) return;
    trim_nl(u);

    printf("Enter Password: "); 
    if (fgets(p, 32, stdin) == NULL) return;
    trim_nl(p);
    
    // Kiểm tra đầu vào rỗng
    if (strlen(u) == 0 || strlen(p) == 0) {
        printf("Error: Username and Password cannot be empty.\n");
        return;
    }

    // Gửi lệnh: SIGNUP <user> <pass>
    sprintf(buf, "SIGNUP %s %s\n", u, p);
    send(sock, buf, strlen(buf), 0);
    
    // Nhận phản hồi
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) return;
    buf[n] = 0;
    
    // Xử lý kết quả trả về
    if (strncmp(buf, "SIGNUP OK", 9) == 0) {
        printf("Success: Account created successfully! You can now Sign In.\n");
    } 
    else if (strstr(buf, "ERR_EXIST")) {
        printf("Error: Username '%s' is already taken. Please try another one.\n", u);
    } 
    else {
        // Lỗi khác
        printf("Server Error: %s", buf);
    }
}

int req_signin(int sock, char *current_user) {
    char u[32], p[32], buf[256];
    printf("Username: "); fgets(u, 32, stdin); trim_nl(u);
    printf("Password: "); fgets(p, 32, stdin); trim_nl(p);
    
    sprintf(buf, "SIGNIN %s %s\n", u, p);
    send(sock, buf, strlen(buf), 0);
    
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    buf[n] = 0;
    
    if (strncmp(buf, "SIGNIN OK", 9) == 0) {
        strcpy(current_user, u);
        printf("Sign in successful. Welcome!\n");
        return 1;
    } else {
        printf("Sign in failed: %s", buf);
        return 0;
    }
}

void req_logout(int sock) {
    send(sock, "LOGOUT\n", 7, 0);
    char buf[128];
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    buf[n] = 0;
    printf("%s", buf);
}

void req_mkgrp(int sock) {
    char g[32], buf[128];
    
    // Câu nhắc nhập liệu rõ ràng hơn
    printf("Enter name for the new group: "); 
    if (fgets(g, 32, stdin) == NULL) return;
    trim_nl(g);

    // Kiểm tra rỗng
    if (strlen(g) == 0) {
        printf("Error: Group name cannot be empty.\n");
        return;
    }

    sprintf(buf, "MKGRP %s\n", g);
    send(sock, buf, strlen(buf), 0);
    
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) return;
    buf[n] = 0;

    // --- XỬ LÝ PHẢN HỒI THÂN THIỆN ---
    if (strncmp(buf, "MKGRP OK", 8) == 0) {
        printf("Success: Group '%s' created. You are now the owner.\n", g);
    } 
    else if (strstr(buf, "ERR_EXIST")) {
        printf("Error: Group '%s' already exists. Please choose a different name.\n", g);
    } 
    else if (strstr(buf, "ERR_NOT_LOGIN")) {
        printf("Error: You must be logged in to create a group.\n");
    }
    else {
        // Lỗi khác không xác định thì mới in nguyên văn để debug
        printf("Server Error: %s", buf);
    }
}

void req_lsgrp(int sock) {
    send(sock, "LSGRP\n", 6, 0);
    
    char buf[4096]; 
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    if (n <= 0) return;
    buf[n] = 0;

    if (strncmp(buf, "LSGRP OK", 8) == 0) {
        char *content = buf + 8; 

        while (*content == ' ') content++;

        if (*content == '\0' || *content == '\n') {
            printf("No groups available.\n");
        } else {
            printf("\nAVAILABLE GROUPS\n");
            
            // Tách chuỗi bằng khoảng trắng hoặc xuống dòng
            char *token = strtok(content, " \n");
            int count = 1;
            while (token != NULL) {
                printf("%d. %s\n", count++, token);
                token = strtok(NULL, " \n");
            }
        }
    } 
    else if (strstr(buf, "ERR_NOT_LOGIN")) {
        printf("Error: Please login first.\n");
    }
    else {
        printf("Server Error: %s", buf);
    }
}