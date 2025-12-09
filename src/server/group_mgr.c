#include "group_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#define GROUP_FILE "data/groups.txt"
#define STORAGE_DIR "storage"

// Hàm tạo nhóm: Lưu người tạo làm nhóm trưởng (đứng đầu)
int handle_mkgrp_logic(const char *group_name, const char *creator_name) {
    // 1. Kiểm tra nhóm đã tồn tại chưa
    FILE *f = fopen(GROUP_FILE, "r");
    char line[1024];
    char current_group[64];
    
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            // Lấy token đầu tiên là tên nhóm
            if (sscanf(line, "%s", current_group) == 1) {
                if (strcmp(current_group, group_name) == 0) {
                    fclose(f);
                    return 0; // Đã tồn tại
                }
            }
        }
        fclose(f);
    }

    // 2. Ghi vào file: <tên nhóm> <người tạo>
    f = fopen(GROUP_FILE, "a");
    if (f) {
        fprintf(f, "%s %s\n", group_name, creator_name);
        fclose(f);
    } else {
        return 0; // Lỗi không mở được file
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

// Hàm kiểm tra: User có trong nhóm không?
// Return: 1 (Có), 0 (Không)
int check_user_in_group(const char *username, const char *group_name) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0; // Không có file dữ liệu

    char line[4096]; // Buffer lớn để chứa danh sách thành viên dài
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        // Xóa ký tự xuống dòng ở cuối nếu có
        line[strcspn(line, "\n")] = 0;

        // Tách token đầu tiên (Tên nhóm)
        char *token = strtok(line, " ");
        if (token != NULL && strcmp(token, group_name) == 0) {
            // Nhóm trùng khớp, duyệt tiếp các token sau (các thành viên)
            while ((token = strtok(NULL, " ")) != NULL) {
                if (strcmp(token, username) == 0) {
                    found = 1;
                    break;
                }
            }
        }
        if (found) break;
    }
    fclose(f);
    return found;
}

// Hàm liệt kê nhóm (Giữ nguyên hoặc sửa chút để chỉ hiện tên nhóm)
void get_group_list_string(char *buffer, int size) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) {
        snprintf(buffer, size, "LSGRP EMPTY");
        return;
    }
    
    char line[4096];
    char gname[64];
    
    strcpy(buffer, "LSGRP_RESULT");
    
    while (fgets(line, sizeof(line), f)) {
        // Chỉ lấy token đầu tiên (tên nhóm) để hiển thị
        if (sscanf(line, "%s", gname) == 1) {
            strcat(buffer, " ");
            strcat(buffer, gname);
        }
    }
    strcat(buffer, "\n");
    fclose(f);
}