#include "group_mgr.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define GROUP_FILE "data/groups.txt"
#define STORAGE_DIR "storage"

int handle_mkgrp_logic(const char *group_name) {
    // 1. Kiểm tra trong file text
    FILE *f = fopen(GROUP_FILE, "r");
    char existing_group[64];
    if (f) {
        while (fscanf(f, "%s", existing_group) == 1) {
            if (strcmp(existing_group, group_name) == 0) {
                fclose(f);
                return 0; // Đã tồn tại
            }
        }
        fclose(f);
    }

    // 2. Ghi vào file
    f = fopen(GROUP_FILE, "a");
    if (f) {
        fprintf(f, "%s\n", group_name);
        fclose(f);
    }

    // 3. Tạo thư mục vật lý
    char path[256];
    sprintf(path, "%s/%s", STORAGE_DIR, group_name);
    #ifdef _WIN32
        mkdir(path);
    #else
        mkdir(path, 0777);
    #endif
    
    return 1;
}

void get_group_list_string(char *buffer, int size) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) {
        snprintf(buffer, size, "LSGRP EMPTY");
        return;
    }
    
    char line[64];
    strcpy(buffer, "LSGRP_RESULT");
    while (fscanf(f, "%s", line) == 1) {
        strcat(buffer, " ");
        strcat(buffer, line);
    }
    strcat(buffer, "\n"); // Kết thúc bằng xuống dòng
    fclose(f);
}