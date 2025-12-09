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
    printf("Username: "); fgets(u, 32, stdin); trim_nl(u);
    printf("Password: "); fgets(p, 32, stdin); trim_nl(p);
    
    sprintf(buf, "SIGNUP %s %s\n", u, p);
    send(sock, buf, strlen(buf), 0);
    
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    buf[n] = 0;
    printf("Server: %s", buf);
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
        printf("Login successful!\n");
        return 1;
    } else {
        printf("Login failed: %s", buf);
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
    printf("Group Name: "); fgets(g, 32, stdin); trim_nl(g);
    sprintf(buf, "MKGRP %s\n", g);
    send(sock, buf, strlen(buf), 0);
    
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    buf[n] = 0;
    printf("Server: %s", buf);
}

void req_lsgrp(int sock) {
    send(sock, "LSGRP\n", 6, 0);
    char buf[1024];
    int n = recv(sock, buf, sizeof(buf)-1, 0);
    buf[n] = 0;
    printf("Server Group List:\n%s\n", buf);
}