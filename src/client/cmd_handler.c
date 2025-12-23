#include "cmd_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

#define BUF_SIZE 2048
#define MAX_NAME 64

// --- HELPERS (Nội bộ) ---

// Đọc 1 dòng từ socket (tương tự client3.c)
static int recv_line(int sock, char *buf, size_t maxlen) {
    size_t len = 0;
    while (len < maxlen - 1) {
        char c;
        int n = recv(sock, &c, 1, 0);
        if (n == 0) { // Kết nối đóng
            if (len == 0) return 0;
            break;
        }
        if (n < 0) {
            perror("[Client] recv error");
            return -1;
        }
        buf[len++] = c;
        if (c == '\n') break;
    }
    buf[len] = '\0';
    return (int)len;
}

// Xóa ký tự xuống dòng thừa (\n, \r)
static void trim_str(char *str) {
    int len = strlen(str);
    while (len > 0 && (str[len-1] == '\n' || str[len-1] == '\r')) {
        str[len-1] = '\0';
        len--;
    }
}

// Hàm nhập liệu chuẩn (In prompt -> fgets -> trim)
static void input_text(const char *prompt, char *buf, size_t sz) {
    printf("%s", prompt);
    fflush(stdout);
    if (!fgets(buf, sz, stdin)) {
        buf[0] = '\0';
        return;
    }
    trim_str(buf);
}

// ============================================================
// PHẦN 1: QUẢN LÝ NHÓM (MEMBER, LEAVE)
// ============================================================

void handle_lsmem(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm cần xem thành viên: ", gname, sizeof(gname));
    if (!gname[0]) { printf("Tên nhóm không được rỗng.\n"); return; }

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LSMEM %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "LSMEM ERR_NOT_LOGIN", 19) == 0) {
        printf("Lỗi: Bạn chưa đăng nhập.\n");
    } else if (strncmp(resp, "LSMEM ERR_NOT_FOUND", 19) == 0) {
        printf("Lỗi: Nhóm không tồn tại.\n");
    } else if (strncmp(resp, "LSMEM OK", 8) == 0) {
        char *p = resp + 8;
        while (*p == ' ') p++;
        printf("Thành viên trong nhóm '%s': %s", gname, p); // Server gửi kèm \n
    } else {
        printf("Server: %s", resp);
    }
}

void handle_leave(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm muốn rời: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LEAVE %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "LEAVE OK", 8) == 0) printf("Đã rời nhóm '%s'.\n", gname);
    else if (strncmp(resp, "LEAVE ERR_OWNER", 15) == 0) printf("Lỗi: Bạn là trưởng nhóm, không thể rời nhóm.\n");
    else if (strncmp(resp, "LEAVE ERR_NOT_IN", 16) == 0) printf("Lỗi: Bạn không ở trong nhóm này.\n");
    else printf("Server: %s", resp);
}

void handle_rmmem(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập username muốn xóa: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMMEM %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "RMMEM OK", 8) == 0) printf("Đã xóa '%s' khỏi nhóm '%s'.\n", uname, gname);
    else if (strncmp(resp, "RMMEM ERR_NO_PERM", 17) == 0) printf("Lỗi: Bạn không phải trưởng nhóm.\n");
    else if (strncmp(resp, "RMMEM ERR_OWNER", 15) == 0) printf("Lỗi: Không thể xóa trưởng nhóm.\n");
    else printf("Server: %s", resp);
}

// ============================================================
// PHẦN 2: REQUEST JOIN SYSTEM
// ============================================================

void handle_join_request(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm muốn tham gia: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "JOIN_REQUEST %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "JOIN_REQUEST OK", 15) == 0) printf("Đã gửi yêu cầu tham gia nhóm '%s'.\n", gname);
    else if (strncmp(resp, "JOIN_REQUEST ERR_ALREADY_IN", 27) == 0) printf("Bạn đã ở trong nhóm này rồi.\n");
    else printf("Server: %s", resp);
}

void handle_view_request(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm (bạn là trưởng nhóm): ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "VIEW_REQUEST %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "VIEW_REQUEST OK", 15) == 0) {
        char *p = resp + 15;
        while (*p == ' ') p++;
        if (*p == '\n' || *p == '\0') printf("Không có yêu cầu nào.\n");
        else printf("Danh sách yêu cầu: %s", p);
    } else if (strncmp(resp, "VIEW_REQUEST ERR_NO_PERM", 24) == 0) {
        printf("Lỗi: Bạn không có quyền xem yêu cầu của nhóm này.\n");
    } else {
        printf("Server: %s", resp);
    }
}

void handle_approve_request(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập username cần duyệt: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "APPROVE_REQUEST %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "APPROVE_REQUEST OK", 18) == 0) printf("Đã duyệt thành công cho '%s'.\n", uname);
    else if (strncmp(resp, "APPROVE_REQUEST ERR_REQ_NOT_FOUND", 33) == 0) printf("Lỗi: Không tìm thấy yêu cầu này.\n");
    else printf("Server: %s", resp);
}

// ============================================================
// PHẦN 3: INVITE SYSTEM
// ============================================================

void handle_invite(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập username muốn mời: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "INVITE %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "INVITE OK", 9) == 0) printf("Đã gửi lời mời tới '%s'.\n", uname);
    else if (strncmp(resp, "INVITE ERR_MEMBER_NOT_FOUND", 27) == 0) printf("Lỗi: User không tồn tại.\n");
    else if (strncmp(resp, "INVITE ERR_ALREADY_IN", 21) == 0) printf("Lỗi: User đã ở trong nhóm.\n");
    else printf("Server: %s", resp);
}

void handle_view_invite(int sock) {
    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "VIEW_INVITE\n");
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "VIEW_INVITE OK", 14) == 0) {
        char *p = resp + 14;
        while (*p == ' ') p++;
        if (*p == '\n' || *p == '\0') printf("Bạn không có lời mời nào.\n");
        else printf("Các lời mời đang chờ: %s", p);
    } else {
        printf("Server: %s", resp);
    }
}

void handle_accept_invite(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm muốn chấp nhận: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "ACCEPT_INVITE %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "ACCEPT_INVITE OK", 17) == 0) printf("Đã tham gia nhóm '%s'.\n", gname);
    else if (strncmp(resp, "ACCEPT_INVITE ERR_INVITE_NOT_FOUND", 34) == 0) printf("Lỗi: Không tìm thấy lời mời nào cho nhóm này.\n");
    else printf("Server: %s", resp);
}

// ============================================================
// PHẦN 4: FILE SYSTEM COMMANDS (MKDIR, COPY, MOVE...)
// ============================================================

void handle_lsdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn (VD: . hoặc subfolder): ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LSDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "LSDIR OK", 8) == 0) {
        char *p = resp + 8;
        if (*p == '\n' || *p == '\0') printf("Thư mục rỗng.\n");
        else printf("Nội dung: %s", p);
    } else {
        printf("Server: %s", resp);
    }
}

void handle_mkdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn thư mục mới (VD: docs/tailieu): ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MKDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MKDIR OK", 8) == 0) printf("Tạo thư mục thành công.\n");
    else printf("Server: %s", resp);
}

void handle_redir(int sock) {
    char gname[MAX_NAME], oldp[256], newname[128];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn cũ (VD: folder1): ", oldp, sizeof(oldp));
    input_text("Nhập tên mới (VD: folder_new): ", newname, sizeof(newname));
    if (!gname[0] || !oldp[0] || !newname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "REDIR %s %s %s\n", gname, oldp, newname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "REDIR OK", 8) == 0) printf("Đổi tên thư mục thành công.\n");
    else printf("Server: %s", resp);
}

void handle_mvdir(int sock) {
    char gname[MAX_NAME], oldp[256], newp[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn nguồn: ", oldp, sizeof(oldp));
    input_text("Nhập đường dẫn đích: ", newp, sizeof(newp));
    if (!gname[0] || !oldp[0] || !newp[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MVDIR %s %s %s\n", gname, oldp, newp);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MVDIR OK", 8) == 0) printf("Di chuyển thư mục thành công.\n");
    else printf("Server: %s", resp);
}

void handle_copdir(int sock) {
    char gname[MAX_NAME], oldp[256], newp[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn nguồn: ", oldp, sizeof(oldp));
    input_text("Nhập đường dẫn đích: ", newp, sizeof(newp));
    if (!gname[0] || !oldp[0] || !newp[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "COPDIR %s %s %s\n", gname, oldp, newp);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "COPDIR OK", 9) == 0) printf("Copy thư mục thành công.\n");
    else printf("Server: %s", resp);
}

void handle_rmdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn thư mục cần xóa: ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "RMDIR OK", 8) == 0) printf("Xóa thư mục thành công.\n");
    else printf("Server: %s", resp);
}

void handle_refile(int sock) {
    char gname[MAX_NAME], oldp[256], newname[128];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn file cũ: ", oldp, sizeof(oldp));
    input_text("Nhập tên file mới: ", newname, sizeof(newname));
    if (!gname[0] || !oldp[0] || !newname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "REFILE %s %s %s\n", gname, oldp, newname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "REFILE OK", 9) == 0) printf("Đổi tên file thành công.\n");
    else printf("Server: %s", resp);
}

void handle_mvfile(int sock) {
    char gname[MAX_NAME], src[256], dst[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn file nguồn: ", src, sizeof(src));
    input_text("Nhập đường dẫn thư mục đích: ", dst, sizeof(dst));
    if (!gname[0] || !src[0] || !dst[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MVFILE %s %s %s\n", gname, src, dst);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MVFILE OK", 9) == 0) printf("Di chuyển file thành công.\n");
    else printf("Server: %s", resp);
}

void handle_copfile(int sock) {
    char gname[MAX_NAME], src[256], dst[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn file nguồn: ", src, sizeof(src));
    input_text("Nhập đường dẫn thư mục đích: ", dst, sizeof(dst));
    if (!gname[0] || !src[0] || !dst[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "COPFILE %s %s %s\n", gname, src, dst);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "COPFILE OK", 10) == 0) printf("Copy file thành công.\n");
    else printf("Server: %s", resp);
}

void handle_rmfile(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Nhập tên nhóm: ", gname, sizeof(gname));
    input_text("Nhập đường dẫn file cần xóa: ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMFILE %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "RMFILE OK", 9) == 0) printf("Xóa file thành công.\n");
    else printf("Server: %s", resp);
}

void handle_join_req_status(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm bạn đã gửi yêu cầu: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "JOIN_REQ_STATUS %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "STATUS APPROVED", 15) == 0) 
        printf("--> Bạn ĐÃ LÀ THÀNH VIÊN của nhóm '%s'.\n", gname);
    else if (strncmp(resp, "STATUS PENDING", 14) == 0) 
        printf("--> Yêu cầu đang CHỜ DUYỆT.\n");
    else if (strncmp(resp, "STATUS REJECTED", 15) == 0) 
        printf("--> Yêu cầu đã bị TỪ CHỐI (hoặc chưa từng gửi).\n");
    else 
        printf("Server: %s", resp);
}

// Kiểm tra danh sách mời của nhóm (Chỉ chủ nhóm)
void handle_invite_status(int sock) {
    char gname[MAX_NAME];
    input_text("Nhập tên nhóm (bạn là chủ): ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "INVITE_STATUS %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[4096]; // Buffer lớn cho danh sách
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "STATUS_LIST", 11) == 0) {
        char *p = resp + 11; 
        if (strncmp(p, " EMPTY", 6) == 0) {
            printf("--> Nhóm này chưa từng mời ai.\n");
        } else {
            printf("\n--- DANH SÁCH LỊCH SỬ MỜI ---\n");
            printf("%-20s | %-15s\n", "User", "Trạng thái");
            printf("---------------------+----------------\n");
            
            // Server gửi: " User1:PENDING User2:ACCEPTED"
            char *token = strtok(p, " \n");
            while (token != NULL) {
                char *colon = strchr(token, ':');
                if (colon) {
                    *colon = '\0';
                    char *user = token;
                    char *status = colon + 1;
                    
                    // In màu hoặc định dạng cho đẹp
                    printf("%-20s | %s\n", user, status);
                }
                token = strtok(NULL, " \n");
            }
            printf("--------------------------------------\n");
        }
    } 
    else if (strncmp(resp, "STATUS ERR_NO_PERM", 18) == 0) {
        printf("Lỗi: Bạn không phải trưởng nhóm.\n");
    } 
    else {
        printf("Server: %s", resp);
    }
}