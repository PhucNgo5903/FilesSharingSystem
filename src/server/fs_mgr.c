#include "fs_mgr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>

#define GROUP_DIR "storage"
#define PATH_MAX_LEN 4096

// --- Helper: Tạo thư mục đệ quy (Recursive MKDIR) ---
static int mkdir_p(const char *path) {
    char tmp[PATH_MAX_LEN];
    if (strlen(path) >= sizeof(tmp)) return 0; // Check length
    strcpy(tmp, path);
    
    size_t len = strlen(tmp);
    if (len == 0) return 0;
    if (tmp[len - 1] == '/') tmp[len - 1] = 0;

    for (char *p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            #ifdef _WIN32
                mkdir(tmp);
            #else
                if (mkdir(tmp, 0777) < 0 && errno != EEXIST) return 0;
            #endif
            *p = '/';
        }
    }
    
    #ifdef _WIN32
        return (mkdir(tmp) == 0 || errno == EEXIST) ? 1 : 0;
    #else
        return (mkdir(tmp, 0777) == 0 || errno == EEXIST) ? 1 : 0;
    #endif
}

// --- Helpers Khác ---
void ensure_fs_dirs() {
    mkdir_p(GROUP_DIR);
}

static int is_safe_path(const char *p) {
    if (!p || !p[0]) return 0;
    if (p[0] == '/') return 0; 
    if (strstr(p, "..")) return 0; 
    return 1;
}

static int copy_file_internal(const char *src, const char *dst) {
    int in = open(src, O_RDONLY);
    if (in < 0) return 0;
    
    int out = open(dst, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (out < 0) { close(in); return 0; }

    char buf[8192];
    ssize_t n;
    while ((n = read(in, buf, sizeof(buf))) > 0) {
        if (write(out, buf, n) != n) {
            close(in); close(out); return 0;
        }
    }
    close(in); close(out);
    return 1;
}

static int rm_rf(const char *path) {
    struct stat st;
    if (stat(path, &st) != 0) return 0;
    if (!S_ISDIR(st.st_mode)) return unlink(path) == 0;

    DIR *d = opendir(path);
    if (!d) return 0;
    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        
        char child[PATH_MAX_LEN];
        // --- Manual construct to avoid warning ---
        if (strlen(path) + 1 + strlen(de->d_name) >= sizeof(child)) continue;
        strcpy(child, path);
        strcat(child, "/");
        strcat(child, de->d_name);
        // -----------------------------------------
        
        rm_rf(child);
    }
    closedir(d);
    return rmdir(path) == 0;
}

static int cp_rf(const char *src, const char *dst) {
    struct stat st;
    if (stat(src, &st) != 0) return 0;
    
    if (S_ISDIR(st.st_mode)) {
        mkdir_p(dst);
        
        DIR *d = opendir(src);
        if (!d) return 0;
        struct dirent *de;
        while ((de = readdir(d)) != NULL) {
            if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
            
            char s_child[PATH_MAX_LEN], d_child[PATH_MAX_LEN];
            
            // Check limits manually
            if (strlen(src) + 1 + strlen(de->d_name) >= sizeof(s_child)) continue;
            if (strlen(dst) + 1 + strlen(de->d_name) >= sizeof(d_child)) continue;

            strcpy(s_child, src); strcat(s_child, "/"); strcat(s_child, de->d_name);
            strcpy(d_child, dst); strcat(d_child, "/"); strcat(d_child, de->d_name);
            
            cp_rf(s_child, d_child);
        }
        closedir(d);
        return 1;
    } else {
        return copy_file_internal(src, dst);
    }
}

// --- Implementation ---

int handle_mkdir(const char *group_name, const char *relpath) {
    if (!is_safe_path(relpath)) return -1; 
    char fullpath[PATH_MAX_LEN];
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", GROUP_DIR, group_name, relpath) >= sizeof(fullpath)) return -1;
    return mkdir_p(fullpath);
}

int handle_lsdir(const char *group_name, const char *relpath, char *resp, int maxlen) {
    if (!is_safe_path(relpath) && strcmp(relpath, ".") != 0) return -1;
    
    char fullpath[PATH_MAX_LEN];
    if (strcmp(relpath, ".") == 0) {
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s", GROUP_DIR, group_name) >= sizeof(fullpath)) return -1;
    } else {
        if (snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", GROUP_DIR, group_name, relpath) >= sizeof(fullpath)) return -1;
    }

    DIR *d = opendir(fullpath);
    if (!d) return 0; 

    // Reset buffer
    resp[0] = '\0';
    strncat(resp, "LSDIR OK", maxlen - 1);

    struct dirent *de;
    while ((de = readdir(d)) != NULL) {
        if (!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
        
        // Check output buffer limit
        if (strlen(resp) + 1 + strlen(de->d_name) >= maxlen) break;

        strcat(resp, " ");
        strcat(resp, de->d_name);
        
        struct stat st;
        char child[PATH_MAX_LEN];
        
        // --- FIX WARNING TRUNCATION (LSDIR) ---
        // Thay vì snprintf, dùng logic check + strcpy
        if (strlen(fullpath) + 1 + strlen(de->d_name) < sizeof(child)) {
            strcpy(child, fullpath);
            strcat(child, "/");
            strcat(child, de->d_name);
            
            if (stat(child, &st) == 0 && S_ISDIR(st.st_mode)) {
                if (strlen(resp) + 1 < maxlen) strcat(resp, "/");
            }
        }
    }
    if (strlen(resp) + 1 < maxlen) strcat(resp, "\n");
    closedir(d);
    return 1;
}

int handle_rmdir(const char *group_name, const char *relpath) {
    if (!is_safe_path(relpath)) return -1;
    char fullpath[PATH_MAX_LEN];
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", GROUP_DIR, group_name, relpath) >= sizeof(fullpath)) return -1;
    return rm_rf(fullpath);
}

int handle_copdir(const char *group_name, const char *src_rel, const char *dst_rel) {
    if (!is_safe_path(src_rel) || !is_safe_path(dst_rel)) return -1;
    char s_full[PATH_MAX_LEN], d_full[PATH_MAX_LEN];
    if (snprintf(s_full, sizeof(s_full), "%s/%s/%s", GROUP_DIR, group_name, src_rel) >= sizeof(s_full)) return -1;
    if (snprintf(d_full, sizeof(d_full), "%s/%s/%s", GROUP_DIR, group_name, dst_rel) >= sizeof(d_full)) return -1;
    return cp_rf(s_full, d_full);
}

int handle_mvdir(const char *group_name, const char *src_rel, const char *dst_rel) {
    if (!is_safe_path(src_rel) || !is_safe_path(dst_rel)) return -1;
    
    char s_full[PATH_MAX_LEN], d_full[PATH_MAX_LEN];
    if (snprintf(s_full, sizeof(s_full), "%s/%s/%s", GROUP_DIR, group_name, src_rel) >= sizeof(s_full)) return -1;
    if (snprintf(d_full, sizeof(d_full), "%s/%s/%s", GROUP_DIR, group_name, dst_rel) >= sizeof(d_full)) return -1;
    
    // Check: Nếu đích đến là một thư mục đang tồn tại -> Di chuyển vào BÊN TRONG nó
    struct stat st;
    if (stat(d_full, &st) == 0 && S_ISDIR(st.st_mode)) {
        // Lấy tên thư mục gốc của nguồn (VD: docs1/tailieu -> tailieu)
        const char *base = strrchr(src_rel, '/');
        base = base ? base + 1 : src_rel;

        // Tạo đường dẫn đích mới: d_full + "/" + base
        char new_dest[PATH_MAX_LEN];
        if (strlen(d_full) + 1 + strlen(base) >= sizeof(new_dest)) return -1;
        
        strcpy(new_dest, d_full);
        strcat(new_dest, "/");
        strcat(new_dest, base);
        
        return rename(s_full, new_dest) == 0;
    }

    // Nếu đích chưa tồn tại -> Đổi tên (Rename/Move to)
    return rename(s_full, d_full) == 0;
}
int handle_redir(const char *group_name, const char *old_rel, const char *new_name) {
    char parent[PATH_MAX_LEN];
    if (strlen(old_rel) >= sizeof(parent)) return -1;
    strcpy(parent, old_rel);
    
    char *last_slash = strrchr(parent, '/');
    if (last_slash) *last_slash = '\0';
    else strcpy(parent, "."); 

    char new_rel[PATH_MAX_LEN];
    
    // --- FIX WARNING TRUNCATION (REDIR) ---
    // Kiểm tra độ dài thủ công
    if (strcmp(parent, ".") == 0) {
        if (strlen(new_name) >= sizeof(new_rel)) return -1;
        strcpy(new_rel, new_name);
    } else {
        if (strlen(parent) + 1 + strlen(new_name) >= sizeof(new_rel)) return -1;
        strcpy(new_rel, parent);
        strcat(new_rel, "/");
        strcat(new_rel, new_name);
    }

    return handle_mvdir(group_name, old_rel, new_rel);
}

int handle_refile(const char *group_name, const char *old_rel, const char *new_name) {
    return handle_redir(group_name, old_rel, new_name);
}

int handle_rmfile(const char *group_name, const char *relpath) {
    if (!is_safe_path(relpath)) return -1;
    char fullpath[PATH_MAX_LEN];
    if (snprintf(fullpath, sizeof(fullpath), "%s/%s/%s", GROUP_DIR, group_name, relpath) >= sizeof(fullpath)) return -1;
    return unlink(fullpath) == 0;
}

int handle_copfile(const char *group_name, const char *src_rel, const char *dst_dir_rel) {
    if (!is_safe_path(src_rel) || !is_safe_path(dst_dir_rel)) return -1;
    
    const char *filename = strrchr(src_rel, '/');
    if (filename) filename++; else filename = src_rel;

    char s_full[PATH_MAX_LEN], d_full[PATH_MAX_LEN];
    if (snprintf(s_full, sizeof(s_full), "%s/%s/%s", GROUP_DIR, group_name, src_rel) >= sizeof(s_full)) return -1;
    if (snprintf(d_full, sizeof(d_full), "%s/%s/%s/%s", GROUP_DIR, group_name, dst_dir_rel, filename) >= sizeof(d_full)) return -1;
    
    return copy_file_internal(s_full, d_full);
}

int handle_mvfile(const char *group_name, const char *src_rel, const char *dst_dir_rel) {
    const char *filename = strrchr(src_rel, '/');
    if (filename) filename++; else filename = src_rel;

    char s_full[PATH_MAX_LEN], d_full[PATH_MAX_LEN];
    if (snprintf(s_full, sizeof(s_full), "%s/%s/%s", GROUP_DIR, group_name, src_rel) >= sizeof(s_full)) return -1;
    if (snprintf(d_full, sizeof(d_full), "%s/%s/%s/%s", GROUP_DIR, group_name, dst_dir_rel, filename) >= sizeof(d_full)) return -1;
    
    return rename(s_full, d_full) == 0;
}