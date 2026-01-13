#include "group_mgr.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h> // Cần cho hàm unlink, rename, mkstemp
#include <fcntl.h>  // Cần cho fdopen
#include <dirent.h>

#define GROUP_FILE "data/groups.txt"
#define REQ_DIR    "data/requests"
#define INV_DIR    "data/invites"
#define GRP_INV_DIR "data/group_invites"
#define STORAGE_DIR "storage"
#define MAX_NAME 64
#define MAX_LINE 4096

// --- Helper: Cắt ký tự xuống dòng (Fix lỗi Windows/WSL) ---
static void trim_line(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}

// --- Helper: Tạo thư mục dữ liệu ---
void ensure_group_dirs() {
    // Tạo thư mục data nếu chưa có
    #ifdef _WIN32
        mkdir("data");
        mkdir(REQ_DIR);
        mkdir(INV_DIR);
        mkdir(GRP_INV_DIR);
    #else
        mkdir("data", 0777);
        mkdir(REQ_DIR, 0777);
        mkdir(INV_DIR, 0777);
        mkdir(GRP_INV_DIR, 0777);
    #endif
}

int check_user_in_req_file(const char *path, const char *user) {
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char line[64];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        if (strcmp(line, user) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

int group_exists(const char *group_name) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        char current_group[MAX_NAME];
        
        // Lấy token đầu tiên là tên nhóm
        char *token = strtok(line, " ");
        if (token) {
            strcpy(current_group, token);
            if (strcmp(current_group, group_name) == 0) {
                fclose(f);
                return 1; // Tìm thấy
            }
        }
    }
    fclose(f);
    return 0; // Không tìm thấy
}

int check_user_exists_system(const char *username) {
    FILE *f = fopen("data/users.txt", "r"); // Hoặc đường dẫn file account của bạn
    if (!f) return 0;

    char line[256];
    int found = 0;
    while (fgets(line, sizeof(line), f)) {
        // Lấy token đầu tiên là username
        char *token = strtok(line, " ");
        if (token && strcmp(token, username) == 0) {
            found = 1;
            break;
        }
    }
    fclose(f);
    return found;
}

void remove_line_from_file(const char *path, const char *content_to_remove) {
    FILE *f = fopen(path, "r");
    if (!f) return;

    char temp_path[256];
    sprintf(temp_path, "%s.tmp", path);
    FILE *ft = fopen(temp_path, "w");

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        // Nếu dòng không khớp với nội dung cần xóa thì giữ lại
        if (strcmp(line, content_to_remove) != 0 && strlen(line) > 0) {
            fprintf(ft, "%s\n", line);
        }
    }
    fclose(f);
    fclose(ft);
    remove(path);
    rename(temp_path, path);
}

// Helper: Cập nhật trạng thái trong file lịch sử mời của nhóm (user:PENDING -> user:MEMBER)
void update_invite_status_accepted(const char *group_name, const char *user, const char *new_status) {
    char path[256];
    snprintf(path, sizeof(path), "%s/%s.txt", GRP_INV_DIR, group_name);
    
    FILE *f = fopen(path, "r");
    if (!f) return;
    
    char temp_path[512]; // Buffer lớn để tránh warning
    snprintf(temp_path, sizeof(temp_path), "%s.tmp", path);
    FILE *ft = fopen(temp_path, "w");
    
    char line[512];
    while (fgets(line, sizeof(line), f)) {
        // Trim line
        int len = strlen(line);
        while (len > 0 && (line[len-1] == '\n' || line[len-1] == '\r')) {
            line[len-1] = '\0';
            len--;
        }
        
        // Copy dòng ra biến tạm để cắt chuỗi (tránh làm hỏng dòng gốc 'line')
        char temp_line[512];
        strcpy(temp_line, line);
        
        // Tách lấy Username (trước dấu :)
        char *token_user = strtok(temp_line, ":");
        
        if (token_user && strcmp(token_user, user) == 0) {
            // [FIX QUAN TRỌNG] Tìm thấy user -> Ghi lại dòng MỚI HOÀN TOÀN
            // Format: username:NEW_STATUS
            fprintf(ft, "%s:%s\n", user, new_status);
        } else {
            // Không phải user cần tìm -> Ghi lại dòng CŨ nguyên vẹn
            fprintf(ft, "%s\n", line);
        }
    }
    fclose(f);
    fclose(ft);
    
    remove(path);
    rename(temp_path, path);
}

// ---------------------------------------------------------
// PHẦN 1: CORE GROUP MANAGEMENT (MKGRP, CHECK, LIST)
// ---------------------------------------------------------

int handle_mkgrp_logic(const char *group_name, const char *creator_name) {
    ensure_group_dirs();
    
    // 1. Kiểm tra tồn tại
    FILE *f = fopen(GROUP_FILE, "r");
    char line[MAX_LINE];
    char current_group[64];
    
    if (f) {
        while (fgets(line, sizeof(line), f)) {
            trim_line(line);
            if (sscanf(line, "%s", current_group) == 1) {
                if (strcmp(current_group, group_name) == 0) {
                    fclose(f);
                    return 0; // Đã tồn tại
                }
            }
        }
        fclose(f);
    }

    // 2. Ghi vào file: group owner (chưa có member khác)
    f = fopen(GROUP_FILE, "a");
    if (f) {
        fprintf(f, "%s %s\n", group_name, creator_name);
        fclose(f);
    } else {
        return 0;
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

// Kiểm tra user có trong nhóm không (Dùng lại logic cũ của bạn)
int check_user_in_group(const char *username, const char *group_name) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        trim_line(line); // Quan trọng: xóa \r\n

        // Copy dòng để strtok không làm hỏng logic
        char temp[MAX_LINE];
        strcpy(temp, line);

        char *token = strtok(temp, " ");
        if (token != NULL && strcmp(token, group_name) == 0) {
            // Nhóm khớp, tìm user trong các token tiếp theo
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

// Lấy danh sách nhóm (LSGRP)
void get_group_list_string(char *buffer, int size) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) {
        snprintf(buffer, size, "LSGRP EMPTY");
        return;
    }
    
    char line[MAX_LINE];
    char gname[64];
    
    strcpy(buffer, "LSGRP OK");
    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        if (sscanf(line, "%s", gname) == 1) {
            strcat(buffer, " ");
            strcat(buffer, gname);
        }
    }
    strcat(buffer, "\n");
    fclose(f);
}

// Kiểm tra chủ nhóm (người đứng thứ 2 trong dòng: group owner mem1...)
int is_group_owner(const char *group_name, const char *user) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0;

    char line[MAX_LINE];
    char gname[64], owner[64];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        // Đọc 2 token đầu tiên: Tên nhóm và Owner
        if (sscanf(line, "%s %s", gname, owner) == 2) {
            if (strcmp(gname, group_name) == 0) {
                if (strcmp(owner, user) == 0) found = 1;
                break;
            }
        }
    }
    fclose(f);
    return found;
}

// ---------------------------------------------------------
// PHẦN 2: ADVANCED MEMBER MANAGEMENT (ADD/REMOVE)
// ---------------------------------------------------------

// Thêm user vào nhóm (Chỉnh sửa file groups.txt)
int add_member_to_group(const char *group_name, const char *user) {
    if (check_user_in_group(user, group_name)) return 1; // Đã có rồi thì coi như thành công

    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0;

    // Tạo file tạm
    char tmp_filename[] = "data/groups_tmp_XXXXXX";
    int fd = mkstemp(tmp_filename); // Tạo file tạm an toàn
    if (fd == -1) { fclose(f); return 0; }
    
    FILE *ftmp = fdopen(fd, "w");
    
    char line[MAX_LINE];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        
        char gname[64];
        sscanf(line, "%s", gname);

        if (strcmp(gname, group_name) == 0) {
            // Đây là nhóm cần thêm -> Ghi thêm user vào cuối dòng
            fprintf(ftmp, "%s %s\n", line, user);
            found = 1;
        } else {
            // Nhóm khác -> Ghi lại y nguyên
            fprintf(ftmp, "%s\n", line);
        }
    }

    fclose(f);
    fclose(ftmp);

    if (found) {
        remove(GROUP_FILE); // Xóa file cũ
        rename(tmp_filename, GROUP_FILE); // Đổi tên file tạm thành file chính
        return 1;
    } else {
        remove(tmp_filename); // Không tìm thấy nhóm, xóa file tạm
        return 0;
    }
}

// Xóa user khỏi nhóm (Phức tạp: Phải parse lại dòng để loại bỏ user)
// int remove_member_from_group(const char *group_name, const char *user) {
//     // Không cho xóa owner
//     if (is_group_owner(group_name, user)) return 0;

//     FILE *f = fopen(GROUP_FILE, "r");
//     if (!f) return 0;

//     char tmp_filename[] = "data/groups_rm_XXXXXX";
//     int fd = mkstemp(tmp_filename);
//     if (fd == -1) { fclose(f); return 0; }
//     FILE *ftmp = fdopen(fd, "w");

//     char line[MAX_LINE];
//     int found_grp = 0;

//     while (fgets(line, sizeof(line), f)) {
//         trim_line(line);
        
//         char buffer_copy[MAX_LINE];
//         strcpy(buffer_copy, line); // Copy để strtok

//         char *token = strtok(buffer_copy, " "); // Token đầu tiên là tên nhóm
        
//         if (token != NULL && strcmp(token, group_name) == 0) {
//             found_grp = 1;
//             // Ghi lại tên nhóm trước
//             fprintf(ftmp, "%s", token);

//             // Duyệt các thành viên
//             while ((token = strtok(NULL, " ")) != NULL) {
//                 // Nếu token KHÁC user cần xóa thì ghi lại vào file
//                 if (strcmp(token, user) != 0) {
//                     fprintf(ftmp, " %s", token);
//                 }
//             }
//             fprintf(ftmp, "\n"); // Xuống dòng khi xong nhóm này
//         } else {
//             // Nhóm khác, ghi lại y nguyên
//             fprintf(ftmp, "%s\n", line);
//         }
//     }

//     fclose(f);
//     fclose(ftmp);

//     if (found_grp) {
//         remove(GROUP_FILE);
//         rename(tmp_filename, GROUP_FILE);
//         return 1;
//     } else {
//         remove(tmp_filename);
//         return 0;
//     }
// }

int remove_member_from_group(const char *group_name, const char *user) {
    // Không cho xóa owner (bảo vệ thêm lớp logic)
    if (is_group_owner(group_name, user)) return 0;

    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) return 0;

    char tmp_filename[] = "data/groups_rm_XXXXXX";
    int fd = mkstemp(tmp_filename);
    if (fd == -1) { fclose(f); return 0; }
    FILE *ftmp = fdopen(fd, "w");

    char line[MAX_LINE];
    int found_grp = 0;
    int user_deleted = 0; // [NEW] Biến này check xem có xóa được ai không

    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        
        char buffer_copy[MAX_LINE];
        strcpy(buffer_copy, line); 

        char *token = strtok(buffer_copy, " "); // Token đầu tiên là tên nhóm
        
        if (token != NULL && strcmp(token, group_name) == 0) {
            found_grp = 1;
            // Ghi lại tên nhóm trước
            fprintf(ftmp, "%s", token);

            // Duyệt các thành viên
            while ((token = strtok(NULL, " ")) != NULL) {
                // Nếu token KHÁC user cần xóa thì ghi lại vào file
                if (strcmp(token, user) != 0) {
                    fprintf(ftmp, " %s", token);
                } else {
                    // [NEW] Tìm thấy user cần xóa -> Đánh dấu đã xóa
                    user_deleted = 1;
                }
            }
            fprintf(ftmp, "\n"); 
        } else {
            // Nhóm khác, ghi lại y nguyên
            fprintf(ftmp, "%s\n", line);
        }
    }

    fclose(f);
    fclose(ftmp);

    // Chỉ thực hiện rename nếu tìm thấy nhóm
    if (found_grp) {
        remove(GROUP_FILE);
        rename(tmp_filename, GROUP_FILE);
        // [NEW] Trả về kết quả dựa trên việc có xóa được user hay không
        return user_deleted; 
    } else {
        remove(tmp_filename);
        return 0;
    }
}

// Lấy danh sách thành viên (LSMEM)
void get_group_members_string(const char *group_name, char *buffer, int size) {
    FILE *f = fopen(GROUP_FILE, "r");
    if (!f) {
        snprintf(buffer, size, "LSMEM ERR_INTERNAL");
        return;
    }

    char line[MAX_LINE];
    int found = 0;

    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        char temp[MAX_LINE];
        strcpy(temp, line);

        char *token = strtok(temp, " ");
        if (token != NULL && strcmp(token, group_name) == 0) {
            // Found group
            strcpy(buffer, "LSMEM OK ");
            
            // Bỏ qua tên nhóm, lấy token tiếp theo (owner)
            token = strtok(NULL, " "); 
            if (token) {
                strcat(buffer, token);
            }

            // Lấy các members còn lại
            while ((token = strtok(NULL, " ")) != NULL) {
                strcat(buffer, " ");
                strcat(buffer, token);
            }
            strcat(buffer, "\n");
            found = 1;
            break;
        }
    }
    fclose(f);

    if (!found) {
        snprintf(buffer, size, "LSMEM ERR_NOT_FOUND\n");
    }
}

// ---------------------------------------------------------
// PHẦN 3: REQUEST & INVITE SYSTEM
// (Sử dụng file riêng trong thư mục requests/ và invites/)
// ---------------------------------------------------------

// --- REQUESTS ---
// File: data/requests/<group_name>.req (Mỗi dòng 1 username)

int add_join_request(const char *group_name, const char *user) {
    ensure_group_dirs();
    char path[256];
    sprintf(path, "%s/%s.req", REQ_DIR, group_name);

    // Kiểm tra trùng lặp
    FILE *fr = fopen(path, "r");
    if (fr) {
        char line[64];
        while (fgets(line, sizeof(line), fr)) {
            trim_line(line);
            if (strcmp(line, user) == 0) { fclose(fr); return 1; } // Đã request rồi
        }
        fclose(fr);
    }

    FILE *f = fopen(path, "a");
    if (!f) return 0;
    fprintf(f, "%s\n", user);
    fclose(f);
    return 1;
}

void build_view_request_response(const char *group_name, char *buffer, int size) {
    char path[256];
    sprintf(path, "%s/%s.req", REQ_DIR, group_name);
    
    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buffer, size, "VIEW_REQUEST OK\n"); // Không có request nào
        return;
    }

    strcpy(buffer, "VIEW_REQUEST OK");
    char user[64];
    while (fgets(user, sizeof(user), f)) {
        trim_line(user);
        strcat(buffer, " ");
        strcat(buffer, user);
    }
    strcat(buffer, "\n");
    fclose(f);
}

void build_join_req_status_all_response(const char *user, char *buffer, int size) {
    strcpy(buffer, "JOIN_REQ_STATUS OK");
    int has_data = 0;

    // 1. Quét file groups.txt để tìm trạng thái MEMBER/APPROVED
    FILE *fg = fopen(GROUP_FILE, "r");
    if (fg) {
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), fg)) {
            trim_line(line);
            
            // Parse: groupname owner member1 member2 ...
            char group_name[64];
            char temp_line[MAX_LINE];
            strcpy(temp_line, line);
            
            char *token = strtok(temp_line, " ");
            if (token) {
                strcpy(group_name, token); // Lấy tên nhóm
                
                // Quét các token còn lại xem có user không
                int is_member = 0;
                while ((token = strtok(NULL, " ")) != NULL) {
                    if (strcmp(token, user) == 0) {
                        is_member = 1;
                        break;
                    }
                }

                if (is_member) {
                    char entry[128];
                    snprintf(entry, sizeof(entry), " %s:MEMBER", group_name);
                    if (strlen(buffer) + strlen(entry) < size - 1) strcat(buffer, entry);
                    has_data = 1;
                }
            }
        }
        fclose(fg);
    }

    // 2. Quét thư mục data/requests/ để tìm trạng thái PENDING
    DIR *d = opendir(REQ_DIR);
    if (d) {
        struct dirent *dir;
        while ((dir = readdir(d)) != NULL) {
            // Chỉ xét file có đuôi .req
            if (strstr(dir->d_name, ".req")) {
                char req_path[512];
                snprintf(req_path, sizeof(req_path), "%s/%s", REQ_DIR, dir->d_name);
                
                if (check_user_in_req_file(req_path, user)) {
                    // Lấy tên nhóm từ tên file (bỏ đuôi .req)
                    char group_name[64];
                    strncpy(group_name, dir->d_name, sizeof(group_name));
                    char *dot = strstr(group_name, ".req");
                    if (dot) *dot = '\0';

                    char entry[128];
                    snprintf(entry, sizeof(entry), " %s:PENDING", group_name);
                    if (strlen(buffer) + strlen(entry) < size - 1) strcat(buffer, entry);
                    has_data = 1;
                }
            }
        }
        closedir(d);
    }

    if (!has_data) {
        strcat(buffer, " EMPTY");
    }
    strcat(buffer, "\n");
}

// Duyệt Request: Thêm vào nhóm + Xóa khỏi file request
// int approve_join_request(const char *group_name, const char *user) {
//     // 1. Thêm vào nhóm
//     if (!add_member_to_group(group_name, user)) return 0;

//     // 2. Xóa khỏi file request (bằng cách ghi lại file req bỏ user đó)
//     char path[256];
//     sprintf(path, "%s/%s.req", REQ_DIR, group_name);
    
//     FILE *f = fopen(path, "r");
//     if (!f) return 1; // File req không còn (kỳ lạ nhưng coi như xong)

//     char tmp_path[256];
//     sprintf(tmp_path, "%s/%s.req.tmp", REQ_DIR, group_name);
//     FILE *ftmp = fopen(tmp_path, "w");

//     char line[64];
//     while (fgets(line, sizeof(line), f)) {
//         trim_line(line);
//         if (strcmp(line, user) != 0) {
//             fprintf(ftmp, "%s\n", line);
//         }
//     }
//     fclose(f);
//     fclose(ftmp);
//     remove(path);
//     rename(tmp_path, path);
//     return 1;
// }

int approve_join_request(const char *group_name, const char *user) {
    char path[256];
    sprintf(path, "%s/%s.req", REQ_DIR, group_name);

    // --- [BỔ SUNG QUAN TRỌNG] ---
    // 1. Kiểm tra xem user có thực sự nằm trong danh sách request không?
    if (!check_user_in_req_file(path, user)) {
        return -1; // Trả về mã lỗi: Request không tồn tại
    }
    // ----------------------------

    // 2. Thêm vào nhóm (Logic cũ của bạn)
    if (!add_member_to_group(group_name, user)) return 0; // Lỗi thêm file

    // 3. Xóa khỏi file request (Logic cũ của bạn - Giữ nguyên)
    FILE *f = fopen(path, "r");
    if (!f) return 1; 

    char tmp_path[256];
    sprintf(tmp_path, "%s/%s.req.tmp", REQ_DIR, group_name);
    FILE *ftmp = fopen(tmp_path, "w");

    char line[64];
    while (fgets(line, sizeof(line), f)) {
        // Trim line
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, user) != 0) {
            fprintf(ftmp, "%s\n", line);
        }
    }
    fclose(f);
    fclose(ftmp);
    remove(path);
    rename(tmp_path, path);
    
    return 1; // Success
}

// --- INVITES ---
// File: data/invites/<username>.inv (Mỗi dòng 1 group_name)

// int add_invite(const char *user, const char *group_name) {
//     ensure_group_dirs();
    
//     // 1. Ghi vào Inbox của User (để user biết mình được mời) -> data/invites/user.inv
//     char user_path[256];
//     sprintf(user_path, "%s/%s.inv", INV_DIR, user);

//     // Kiểm tra trùng trong inbox user
//     FILE *fr = fopen(user_path, "r");
//     if (fr) {
//         char line[64];
//         while (fgets(line, sizeof(line), fr)) {
//             trim_line(line);
//             if (strcmp(line, group_name) == 0) { fclose(fr); return 1; } // Đã có trong inbox
//         }
//         fclose(fr);
//     }

//     FILE *f = fopen(user_path, "a");
//     if (!f) return 0;
//     fprintf(f, "%s\n", group_name);
//     fclose(f);

//     // 2. [NEW] Ghi vào Lịch sử của Group (để chủ nhóm track status) -> data/group_invites/group.txt
//     char grp_path[256];
//     sprintf(grp_path, "%s/%s.txt", GRP_INV_DIR, group_name);
    
//     // Kiểm tra xem đã từng mời user này chưa (để tránh ghi lặp trong lịch sử)
//     int already_logged = 0;
//     FILE *fg = fopen(grp_path, "r");
//     if (fg) {
//         char line[64];
//         while (fgets(line, sizeof(line), fg)) {
//             trim_line(line);
//             if (strcmp(line, user) == 0) { already_logged = 1; break; }
//         }
//         fclose(fg);
//     }

//     if (!already_logged) {
//         FILE *fg_append = fopen(grp_path, "a");
//         if (fg_append) {
//             fprintf(fg_append, "%s\n", user);
//             fclose(fg_append);
//         }
//     }

//     return 1;
// }
int add_invite(const char *user, const char *group_name) {
    // [FIX VẤN ĐỀ 2] Kiểm tra user có tồn tại trong hệ thống không
    if (!check_user_exists_system(user)) {
        return -1; // Mã lỗi: User không tìm thấy
    }

    ensure_group_dirs();
    
    // 1. Ghi vào Inbox của User (Logic cũ của bạn - Giữ nguyên)
    char user_path[256];
    sprintf(user_path, "%s/%s.inv", INV_DIR, user);

    FILE *fr = fopen(user_path, "r");
    if (fr) {
        char line[64];
        while (fgets(line, sizeof(line), fr)) {
            // Trim line...
            int len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            if (strcmp(line, group_name) == 0) { fclose(fr); return 1; }
        }
        fclose(fr);
    }

    FILE *f = fopen(user_path, "a");
    if (!f) return 0;
    fprintf(f, "%s\n", group_name);
    fclose(f);

    // 2. Ghi vào Lịch sử của Group (Logic cũ của bạn - Giữ nguyên)
    char grp_path[256];
    sprintf(grp_path, "%s/%s.txt", GRP_INV_DIR, group_name);
    
    // Lưu ý: Để khớp với chức năng INVITE_STATUS ở bài trước, 
    // bạn nên ghi format "user:PENDING". Nhưng ở đây tôi giữ logic của bạn
    // để tránh conflict code hiện tại.
    
    int already_logged = 0;
    FILE *fg = fopen(grp_path, "r");
    if (fg) {
        char line[64];
        while (fgets(line, sizeof(line), fg)) {
             // Trim...
            int len = strlen(line);
            if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
            
            // So sánh (Lưu ý nếu file có format user:STATUS thì phải cắt chuỗi để so sánh)
            if (strncmp(line, user, strlen(user)) == 0) { already_logged = 1; break; }
        }
        fclose(fg);
    }

    if (!already_logged) {
        FILE *fg_append = fopen(grp_path, "a");
        if (fg_append) {
            // Ghi trạng thái mặc định là PENDING để sau này dùng INVITE_STATUS cho đẹp
            fprintf(fg_append, "%s:PENDING\n", user); 
            fclose(fg_append);
        }
    }

    return 1; // Thành công
}

void build_view_invite_response(const char *user, char *buffer, int size) {
    char path[256];
    sprintf(path, "%s/%s.inv", INV_DIR, user);

    FILE *f = fopen(path, "r");
    if (!f) {
        snprintf(buffer, size, "VIEW_INVITE OK\n");
        return;
    }

    strcpy(buffer, "VIEW_INVITE OK");
    char gname[64];
    while (fgets(gname, sizeof(gname), f)) {
        trim_line(gname);
        strcat(buffer, " ");
        strcat(buffer, gname);
    }
    strcat(buffer, "\n");
    fclose(f);
}

// int accept_invite(const char *user, const char *group_name) {
//     // 1. Kiểm tra xem có invite không
//     char path[256];
//     sprintf(path, "%s/%s.inv", INV_DIR, user);
//     FILE *f = fopen(path, "r");
//     if (!f) return -1; // Không có invite nào

//     int found = 0;
//     char line[64];
//     while (fgets(line, sizeof(line), f)) {
//         trim_line(line);
//         if (strcmp(line, group_name) == 0) { found = 1; break; }
//     }
//     fclose(f);

//     if (!found) return -1; // Không tìm thấy lời mời cho nhóm này

//     // 2. Thêm vào nhóm
//     if (!add_member_to_group(group_name, user)) return 0;

//     // 3. Xóa invite
//     char tmp_path[256];
//     sprintf(tmp_path, "%s/%s.inv.tmp", INV_DIR, user);
    
//     f = fopen(path, "r");
//     FILE *ftmp = fopen(tmp_path, "w");
    
//     while (fgets(line, sizeof(line), f)) {
//         trim_line(line);
//         if (strcmp(line, group_name) != 0) {
//             fprintf(ftmp, "%s\n", line);
//         }
//     }
//     fclose(f);
//     fclose(ftmp);
//     remove(path);
//     rename(tmp_path, path);

//     return 1;
// }
int accept_invite(const char *user, const char *group_name) {
    // 1. Kiểm tra nhóm có tồn tại không?
    if (!group_exists(group_name)) return -2; // Mã lỗi: Nhóm không tồn tại

    // 2. Kiểm tra user đã ở trong nhóm chưa?
    if (check_user_in_group(user, group_name)) return -3; // Mã lỗi: Đã là thành viên

    // 3. Kiểm tra xem có invite trong Inbox không
    char path[256];
    sprintf(path, "%s/%s.inv", INV_DIR, user);
    FILE *f = fopen(path, "r");
    if (!f) return -1; // File không tồn tại coi như không có invite

    int found = 0;
    char line[64];
    while (fgets(line, sizeof(line), f)) {
        // Trim line
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        
        if (strcmp(line, group_name) == 0) { found = 1; break; }
    }
    fclose(f);

    if (!found) return -1; // Không tìm thấy lời mời

    // 4. Thêm vào nhóm
    if (!add_member_to_group(group_name, user)) return 0; // Lỗi hệ thống

    // 5. Cập nhật trạng thái mời của nhóm (PENDING -> MEMBER)
    update_invite_status_accepted(group_name, user, "MEMBER");

    // 6. Xóa invite khỏi Inbox
    char tmp_path[256];
    sprintf(tmp_path, "%s/%s.inv.tmp", INV_DIR, user);
    
    f = fopen(path, "r");
    FILE *ftmp = fopen(tmp_path, "w");
    
    while (fgets(line, sizeof(line), f)) {
        int len = strlen(line);
        if (len > 0 && line[len-1] == '\n') line[len-1] = '\0';
        if (strcmp(line, group_name) != 0) {
            fprintf(ftmp, "%s\n", line);
        }
    }
    fclose(f);
    fclose(ftmp);
    remove(path);
    rename(tmp_path, path);

    return 1; // Thành công
}

// ---------------------------------------------------------
// PHẦN 4: STATUS CHECKING (NEW)
// ---------------------------------------------------------

// Kiểm tra trạng thái yêu cầu tham gia của chính mình
// Return: 1 (Approved), 0 (Pending), -1 (None/Rejected)
int check_join_req_status(const char *user, const char *group_name) {
    if (check_user_in_group(user, group_name)) return 1; // Approved

    char path[256];
    sprintf(path, "%s/%s.req", REQ_DIR, group_name);
    FILE *f = fopen(path, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            trim_line(line);
            if (strcmp(line, user) == 0) { fclose(f); return 0; } // Pending
        }
        fclose(f);
    }
    return -1; // Rejected/None
}

// Kiểm tra trạng thái lời mời (Chỉ dành cho chủ nhóm check user khác)
// Return: 1 (Accepted), 0 (Pending), -1 (None/Rejected)
int check_invite_status(const char *group_name, const char *target_user) {
    // 1. Kiểm tra target_user đã vào nhóm chưa (Accepted)
    if (check_user_in_group(target_user, group_name)) {
        return 1;
    }

    // 2. Kiểm tra lời mời có còn nằm trong file invite của user đó không (Pending)
    ensure_group_dirs();
    char path[256];
    sprintf(path, "%s/%s.inv", INV_DIR, target_user);

    FILE *f = fopen(path, "r");
    if (f) {
        char line[64];
        while (fgets(line, sizeof(line), f)) {
            trim_line(line);
            if (strcmp(line, group_name) == 0) {
                fclose(f);
                return 0; // Pending
            }
        }
        fclose(f);
    }

    return -1; // Không tìm thấy
}

// 2. Check INVITE_STATUS (Chủ nhóm check danh sách mời)
// Logic: Đọc data/group_invites/<group>.txt -> Với mỗi user, check xem đang ở đâu
// void build_invite_status_all_response(const char *group_name, char *buffer, int size) {
//     char grp_path[256];
//     sprintf(grp_path, "%s/%s.txt", GRP_INV_DIR, group_name);

//     FILE *f = fopen(grp_path, "r");
//     if (!f) {
//         snprintf(buffer, size, "STATUS_LIST EMPTY");
//         return;
//     }

//     strcpy(buffer, "INVITE_STATUS OK ");
//     char user[64];
    
//     while (fgets(user, sizeof(user), f)) {
//         trim_line(user);
//         if (strlen(user) == 0) continue;

//         const char *status = "UNKNOWN";

//         // Check 1: Đã là thành viên chưa?
//         if (check_user_in_group(user, group_name)) {
//             status = "ACCEPTED"; // Đã vào nhóm
//         } else {
//             // Check 2: Còn trong inbox của user đó không?
//             char user_inv_path[256];
//             sprintf(user_inv_path, "%s/%s.inv", INV_DIR, user);
//             FILE *fu = fopen(user_inv_path, "r");
//             int is_pending = 0;
//             if (fu) {
//                 char g_line[64];
//                 while(fgets(g_line, sizeof(g_line), fu)) {
//                     trim_line(g_line);
//                     if (strcmp(g_line, group_name) == 0) { is_pending = 1; break; }
//                 }
//                 fclose(fu);
//             }
            
//             if (is_pending) status = "PENDING";
//             else status = "REJECTED"; // User đã xóa invite hoặc từ chối
//         }

//         char entry[128];
//         snprintf(entry, sizeof(entry), " %s:%s", user, status);
//         if (strlen(buffer) + strlen(entry) < size - 1) {
//             strcat(buffer, entry);
//         }
//     }
//     strcat(buffer, "\n");
//     fclose(f);
// }
void build_invite_status_all_response(const char *group_name, char *buffer, int size) {
    char grp_path[256];
    sprintf(grp_path, "%s/%s.txt", GRP_INV_DIR, group_name);

    FILE *f = fopen(grp_path, "r");
    if (!f) {
        snprintf(buffer, size, "INVITE_STATUS OK EMPTY");
        return;
    }

    strcpy(buffer, "INVITE_STATUS OK"); 
    char line[256]; 
    
    while (fgets(line, sizeof(line), f)) {
        trim_line(line);
        if (strlen(line) == 0) continue;

        // [QUAN TRỌNG] Tách lấy Username sạch
        // Dùng strtok để cắt bỏ phần :PENDING hay :REJECTED nếu lỡ có trong file
        // Nếu file chỉ có "tung1" -> nó lấy "tung1"
        // Nếu file có "phuc1:PENDING" -> nó lấy "phuc1"
        char temp_line[256];
        strcpy(temp_line, line);
        char *real_user = strtok(temp_line, ":");
        
        if (!real_user) continue;

        // [TÍNH TOÁN TRẠNG THÁI]
        const char *status = "UNKNOWN";

        // 1. Kiểm tra đã là thành viên chưa?
        if (check_user_in_group(real_user, group_name)) {
            status = "ACCEPTED"; 
        } else {
            // 2. Kiểm tra còn trong Inbox không?
            char user_inv_path[256];
            sprintf(user_inv_path, "%s/%s.inv", INV_DIR, real_user);
            FILE *fu = fopen(user_inv_path, "r");
            int is_pending = 0;
            
            if (fu) {
                char g_line[64];
                while(fgets(g_line, sizeof(g_line), fu)) {
                    trim_line(g_line);
                    // Inbox chỉ lưu tên nhóm
                    if (strcmp(g_line, group_name) == 0) { 
                        is_pending = 1; 
                        break; 
                    }
                }
                fclose(fu);
            }
            
            if (is_pending) status = "PENDING";
            else status = "REJECTED"; // Không trong nhóm, không trong inbox -> Rejected
        }

        // Ghi vào buffer trả về: " user:STATUS"
        char entry[128];
        snprintf(entry, sizeof(entry), " %s:%s", real_user, status);
        
        if (strlen(buffer) + strlen(entry) < size - 1) {
            strcat(buffer, entry);
        }
    }
    strcat(buffer, "\n");
    fclose(f);
}