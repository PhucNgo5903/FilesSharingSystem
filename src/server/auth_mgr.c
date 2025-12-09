#include "auth_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DATA_FILE "data/users.txt"
#define MAX_USERS 100
#define MAX_NAME 32

typedef struct {
    char username[MAX_NAME];
    char password[MAX_NAME];
} User;

User users[MAX_USERS];
int user_count = 0;

void load_users_from_file() {
    FILE *f = fopen(DATA_FILE, "r");
    if (!f) return;
    user_count = 0;
    while (fscanf(f, "%s %s", users[user_count].username, users[user_count].password) == 2) {
        user_count++;
        if (user_count >= MAX_USERS) break;
    }
    fclose(f);
}

void save_user_to_file(const char *u, const char *p) {
    FILE *f = fopen(DATA_FILE, "a");
    if (f) {
        fprintf(f, "%s %s\n", u, p);
        fclose(f);
    }
}

int handle_signup_logic(const char *user, const char *pass) {
    load_users_from_file(); // Reload để đảm bảo dữ liệu mới nhất
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, user) == 0) return 0; // Đã tồn tại
    }
    save_user_to_file(user, pass);
    return 1; // Thành công
}

int handle_signin_logic(const char *user, const char *pass) {
    load_users_from_file();
    for (int i = 0; i < user_count; i++) {
        if (strcmp(users[i].username, user) == 0 && strcmp(users[i].password, pass) == 0) {
            return 1; // Đúng
        }
    }
    return 0; // Sai
}