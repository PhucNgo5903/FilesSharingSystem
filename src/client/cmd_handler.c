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
    input_text("Enter the group name to view members: ", gname, sizeof(gname));
    
    if (strlen(gname) == 0) {
        printf("Error: Group name cannot be empty.\n");
        return; 
    }

    // Gửi lệnh: LSMEM <group_name>
    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LSMEM %s\n", gname);
    send(sock, req, strlen(req), 0);

    // Nhận phản hồi
    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // Xử lý hiển thị
    if (strncmp(resp, "LSMEM OK", 8) == 0) {
        char *p = resp + 8; // Bỏ qua "LSMEM OK"
        while (*p == ' ') p++; // Bỏ khoảng trắng thừa
        
        // Chuỗi server trả về dạng: "Owner Mem1 Mem2 Mem3..."
        // Token đầu tiên luôn là Owner
        char *token = strtok(p, " \n");
        
        printf("\nMEMBERS OF GROUP '%s'\n", gname);
        if (token != NULL) {
            printf("OWNER: %s\n", token); // Người đầu tiên
            
            // Các token tiếp theo là thành viên
            int count = 0;
            printf("MEMBERS: ");
            while ((token = strtok(NULL, " \n")) != NULL) {
                if (count > 0) printf(", ");
                printf("%s", token);
                count++;
            }
            
            if (count == 0) printf("(None)");
            printf("\n");
        } else {
            printf("Group is empty (Strange error).\n");
        }
    } 
    else if (strncmp(resp, "LSMEM ERR_NOT_FOUND", 19) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    } 
    else if (strncmp(resp, "LSMEM ERR_NOT_LOGIN", 19) == 0) {
        printf("Error: You must login first.\n");
    } 
    else {
        printf("Server Error: %s", resp);
    }
}

void handle_leave(int sock) {
    char gname[MAX_NAME];
    input_text("Enter the group name to leave: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LEAVE %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "LEAVE OK", 8) == 0) printf("You have left the group '%s'.\n", gname);
    else if (strncmp(resp, "LEAVE ERR_OWNER", 15) == 0) printf("Error: You are the group owner and cannot leave.\n");
    else if (strncmp(resp, "LEAVE ERR_NOT_IN", 16) == 0) printf("Error: You are not a member of this group.\n");
    else printf("Server: %s", resp);
}

// void handle_rmmem(int sock) {
//     char gname[MAX_NAME], uname[MAX_NAME];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter username to remove: ", uname, sizeof(uname));
//     if (!gname[0] || !uname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "RMMEM %s %s\n", gname, uname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "RMMEM OK", 8) == 0) printf("Removed '%s' from group '%s'.\n", uname, gname);
//     else if (strncmp(resp, "RMMEM ERR_NO_PERM", 17) == 0) printf("Error: You do not have permission (not a group owner).\n");
//     else if (strncmp(resp, "RMMEM ERR_OWNER", 15) == 0) printf("Error: Cannot remove the group owner.\n");
//     else printf("Server: %s", resp);
// }
void handle_rmmem(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter username to remove: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMMEM %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // Handle responses
    if (strncmp(resp, "RMMEM OK", 8) == 0) {
        printf("Success: Removed '%s' from group '%s'.\n", uname, gname);
    }
    else if (strncmp(resp, "RMMEM ERR_NO_PERM", 17) == 0) {
        printf("Error: You do not have permission (not a group owner).\n");
    }
    else if (strncmp(resp, "RMMEM ERR_OWNER", 15) == 0) {
        printf("Error: Cannot remove the group owner (yourself).\n");
    }
    // [NEW] Xử lý nhóm không tồn tại
    else if (strncmp(resp, "RMMEM ERR_GRP_NOT_FOUND", 23) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    // [NEW] Xử lý thành viên không tồn tại
    else if (strncmp(resp, "RMMEM ERR_MEMBER_NOT_FOUND", 26) == 0) {
        printf("Error: User '%s' is not in group '%s'.\n", uname, gname);
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// ============================================================
// PHẦN 2: REQUEST JOIN SYSTEM
// ============================================================

void handle_join_request(int sock) {
    char gname[MAX_NAME];
    input_text("Enter the group name you want to join: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "JOIN_REQUEST %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // --- CẬP NHẬT LOGIC PHẢN HỒI ---
    if (strncmp(resp, "JOIN_REQUEST OK", 15) == 0) {
        printf("Success: Request sent. Please wait for the owner to approve.\n");
    } 
    else if (strncmp(resp, "JOIN_REQUEST ERR_GRP_NOT_FOUND", 30) == 0) {
        printf("Error: Group '%s' does not exist. Please check the name.\n", gname);
    }
    else if (strncmp(resp, "JOIN_REQUEST ERR_ALREADY_IN", 27) == 0) {
        printf("Info: You are already a member of this group.\n");
    }
    else if (strncmp(resp, "JOIN_REQUEST ERR_REQ_EXIST", 26) == 0) {
        printf("Info: You have already sent a request to this group.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}
void handle_view_request(int sock) {
    char gname[MAX_NAME];
    input_text("Enter the group name (you must be the owner): ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "VIEW_REQUEST %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "VIEW_REQUEST OK", 15) == 0) {
        char *p = resp + 15;
        while (*p == ' ') p++;
        if (*p == '\n' || *p == '\0') printf("No pending requests.\n");
        else printf("Pending requests: %s", p);
    }
    else if (strncmp(resp, "VIEW_REQUEST ERR_GRP_NOT_FOUND", 30) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    else if (strncmp(resp, "VIEW_REQUEST ERR_NO_PERM", 24) == 0) {
        printf("Error: You do not have permission to view requests for this group.\n");
    } else {
        printf("Server: %s", resp);
    }
}

// void handle_approve_request(int sock) {
//     char gname[MAX_NAME], uname[MAX_NAME];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter username to approve: ", uname, sizeof(uname));
//     if (!gname[0] || !uname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "APPROVE_REQUEST %s %s\n", gname, uname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "APPROVE_REQUEST OK", 18) == 0) printf("Successfully approved '%s'.\n", uname);
//     else if (strncmp(resp, "APPROVE_REQUEST ERR_REQ_NOT_FOUND", 33) == 0) printf("Error: Request not found.\n");
//     else printf("Server: %s", resp);
// }
void handle_approve_request(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter username to approve: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "APPROVE_REQUEST %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // --- XỬ LÝ PHẢN HỒI ---
    if (strncmp(resp, "APPROVE_REQUEST OK", 18) == 0) {
        printf("Success: User '%s' has been approved into group '%s'.\n", uname, gname);
    }
    // Lỗi: User không có trong danh sách chờ
    else if (strncmp(resp, "APPROVE_REQUEST ERR_REQ_NOT_FOUND", 33) == 0) {
        printf("Error: Request not found. User '%s' did not ask to join this group.\n", uname);
    }
    // Lỗi: Nhóm không tồn tại
    else if (strncmp(resp, "APPROVE_REQUEST ERR_GRP_NOT_FOUND", 33) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    // Lỗi: Không phải chủ nhóm
    else if (strncmp(resp, "APPROVE_REQUEST ERR_NO_PERM", 27) == 0) {
        printf("Error: You do not have permission. Only the group owner can approve requests.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// ============================================================
// PHẦN 3: INVITE SYSTEM
// ============================================================

// void handle_invite(int sock) {
//     char gname[MAX_NAME], uname[MAX_NAME];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter username to invite: ", uname, sizeof(uname));
//     if (!gname[0] || !uname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "INVITE %s %s\n", gname, uname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "INVITE OK", 9) == 0) printf("Invitation sent to '%s'.\n", uname);
//     else if (strncmp(resp, "INVITE ERR_MEMBER_NOT_FOUND", 27) == 0) printf("Error: User not found.\n");
//     else if (strncmp(resp, "INVITE ERR_ALREADY_IN", 21) == 0) printf("Error: User is already in the group.\n");
//     else printf("Server: %s", resp);
// }
void handle_invite(int sock) {
    char gname[MAX_NAME], uname[MAX_NAME];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter username to invite: ", uname, sizeof(uname));
    if (!gname[0] || !uname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "INVITE %s %s\n", gname, uname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "INVITE OK", 9) == 0) {
        printf("Success: Invitation sent to '%s'.\n", uname);
    }
    // [FIX VẤN ĐỀ 1]
    else if (strncmp(resp, "INVITE ERR_GRP_NOT_FOUND", 24) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    // [FIX VẤN ĐỀ 2]
    else if (strncmp(resp, "INVITE ERR_USER_NOT_FOUND", 25) == 0) {
        printf("Error: User '%s' does not exist in the system.\n", uname);
    }
    else if (strncmp(resp, "INVITE ERR_ALREADY_IN", 21) == 0) {
        printf("Error: User is already in the group.\n");
    }
    else if (strncmp(resp, "INVITE ERR_NO_PERM", 18) == 0) {
        printf("Error: You are not the owner of this group.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
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
        if (*p == '\n' || *p == '\0') printf("You have no invites.\n");
        else printf("Pending invites: %s", p);
    } else {
        printf("Server: %s", resp);
    }
}

// void handle_accept_invite(int sock) {
//     char gname[MAX_NAME];
//     input_text("Enter the group name to accept invite: ", gname, sizeof(gname));
//     if (!gname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "ACCEPT_INVITE %s\n", gname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "ACCEPT_INVITE OK", 16) == 0) {
//         printf("Success: You have successfully joined group '%s'.\n", gname);
//     } 
//     else if (strncmp(resp, "ACCEPT_INVITE ERR_INVITE_NOT_FOUND", 34) == 0) {
//         printf("Error: You do not have an invitation for group '%s'.\n", gname);
//     }
//     else if (strncmp(resp, "ACCEPT_INVITE ERR_NOT_FOUND", 27) == 0) {
//         printf("Error: Group '%s' does not exist.\n", gname);
//     }
//     else {
//         printf("Server Error: %s", resp);
//     }
// }

void handle_accept_invite(int sock) {
    char gname[MAX_NAME];
    input_text("Enter the group name to accept invite: ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "ACCEPT_INVITE %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "ACCEPT_INVITE OK", 16) == 0) {
        printf("Success: You have successfully joined group '%s'.\n", gname);
    } 
    else if (strncmp(resp, "ACCEPT_INVITE ERR_INVITE_NOT_FOUND", 34) == 0) {
        printf("Error: You do not have an invitation for group '%s'.\n", gname);
    }
    // [MỚI] Xử lý nhóm không tồn tại
    else if (strncmp(resp, "ACCEPT_INVITE ERR_GRP_NOT_FOUND", 31) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    // [MỚI] Xử lý đã là thành viên
    else if (strncmp(resp, "ACCEPT_INVITE ERR_ALREADY_IN", 28) == 0) {
        printf("Info: You are already a member of group '%s'.\n", gname);
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// ============================================================
// PHẦN 4: FILE SYSTEM COMMANDS (MKDIR, COPY, MOVE...)
// ============================================================

void handle_lsdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter path (e.g., . or subfolder): ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "LSDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "LSDIR OK", 8) == 0) {
        char *p = resp + 8;
        if (*p == '\n' || *p == '\0') printf("Directory is empty.\n");
        else printf("Contents: %s", p);
    } else {
        printf("Server: %s", resp);
    }
}

// void handle_mkdir(int sock) {
//     char gname[MAX_NAME], path[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter new directory path (e.g., docs/tailieu): ", path, sizeof(path));
//     if (!gname[0] || !path[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "MKDIR %s %s\n", gname, path);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "MKDIR OK", 8) == 0) printf("Directory created successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_redir(int sock) {
//     char gname[MAX_NAME], oldp[256], newname[128];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter old path (e.g., folder1): ", oldp, sizeof(oldp));
//     input_text("Enter new name (e.g., folder_new): ", newname, sizeof(newname));
//     if (!gname[0] || !oldp[0] || !newname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "REDIR %s %s %s\n", gname, oldp, newname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "REDIR OK", 8) == 0) printf("Directory renamed successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_mvdir(int sock) {
//     char gname[MAX_NAME], oldp[256], newp[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter source path: ", oldp, sizeof(oldp));
//     input_text("Enter destination path: ", newp, sizeof(newp));
//     if (!gname[0] || !oldp[0] || !newp[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "MVDIR %s %s %s\n", gname, oldp, newp);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "MVDIR OK", 8) == 0) printf("Directory moved successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_copdir(int sock) {
//     char gname[MAX_NAME], oldp[256], newp[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter source path: ", oldp, sizeof(oldp));
//     input_text("Enter destination path: ", newp, sizeof(newp));
//     if (!gname[0] || !oldp[0] || !newp[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "COPDIR %s %s %s\n", gname, oldp, newp);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "COPDIR OK", 9) == 0) printf("Directory copied successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_rmdir(int sock) {
//     char gname[MAX_NAME], path[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter directory path to remove: ", path, sizeof(path));
//     if (!gname[0] || !path[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "RMDIR %s %s\n", gname, path);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "RMDIR OK", 8) == 0) printf("Directory removed successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_refile(int sock) {
//     char gname[MAX_NAME], oldp[256], newname[128];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter current file path: ", oldp, sizeof(oldp));
//     input_text("Enter new filename: ", newname, sizeof(newname));
//     if (!gname[0] || !oldp[0] || !newname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "REFILE %s %s %s\n", gname, oldp, newname);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "REFILE OK", 9) == 0) printf("File renamed successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_mvfile(int sock) {
//     char gname[MAX_NAME], src[256], dst[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter source file path: ", src, sizeof(src));
//     input_text("Enter destination directory path: ", dst, sizeof(dst));
//     if (!gname[0] || !src[0] || !dst[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "MVFILE %s %s %s\n", gname, src, dst);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "MVFILE OK", 9) == 0) printf("File moved successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_copfile(int sock) {
//     char gname[MAX_NAME], src[256], dst[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter source file path: ", src, sizeof(src));
//     input_text("Enter destination directory path: ", dst, sizeof(dst));
//     if (!gname[0] || !src[0] || !dst[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "COPFILE %s %s %s\n", gname, src, dst);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "COPFILE OK", 10) == 0) printf("File copied successfully.\n");
//     else printf("Server: %s", resp);
// }

// void handle_rmfile(int sock) {
//     char gname[MAX_NAME], path[256];
//     input_text("Enter group name: ", gname, sizeof(gname));
//     input_text("Enter file path to remove: ", path, sizeof(path));
//     if (!gname[0] || !path[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "RMFILE %s %s\n", gname, path);
//     send(sock, req, strlen(req), 0);

//     char resp[BUF_SIZE];
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "RMFILE OK", 9) == 0) printf("File removed successfully.\n");
//     else printf("Server: %s", resp);
// }

// 1. Handle MKDIR (Tạo thư mục)
void handle_mkdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter new directory path (e.g., docs/tailieu): ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MKDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MKDIR OK", 8) == 0) {
        printf("Success: Directory '%s' created.\n", path);
    } 
    else if (strncmp(resp, "MKDIR ERR_NOT_IN", 16) == 0) {
        printf("Error: You are not a member of group '%s'.\n", gname);
    }
    else if (strncmp(resp, "MKDIR ERR_NOT_LOGIN", 19) == 0) {
        printf("Error: You must login first.\n");
    }
    else if (strncmp(resp, "MKDIR ERR_INTERNAL", 18) == 0) {
        printf("Error: Failed to create directory. (Path might be invalid or already exists)\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 2. Handle REDIR (Đổi tên thư mục)
void handle_redir(int sock) {
    char gname[MAX_NAME], oldp[256], newname[128];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter old path (e.g., folder1): ", oldp, sizeof(oldp));
    input_text("Enter new name (e.g., folder_new): ", newname, sizeof(newname));
    if (!gname[0] || !oldp[0] || !newname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "REDIR %s %s %s\n", gname, oldp, newname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "REDIR OK", 8) == 0) {
        printf("Success: Directory renamed to '%s'.\n", newname);
    }
    else if (strncmp(resp, "REDIR ERR_NO_PERM", 17) == 0) {
        printf("Error: Permission denied. Only the group owner can rename directories.\n");
    }
    else if (strncmp(resp, "REDIR ERR_INTERNAL", 18) == 0) {
        printf("Error: Failed to rename. Check if source exists or name is invalid.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 3. Handle MVDIR (Di chuyển thư mục)
void handle_mvdir(int sock) {
    char gname[MAX_NAME], oldp[256], newp[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter source path: ", oldp, sizeof(oldp));
    input_text("Enter destination path: ", newp, sizeof(newp));
    if (!gname[0] || !oldp[0] || !newp[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MVDIR %s %s %s\n", gname, oldp, newp);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MVDIR OK", 8) == 0) {
        printf("Success: Directory moved.\n");
    }
    else if (strncmp(resp, "MVDIR ERR_NOT_IN", 16) == 0) {
        printf("Error: You are not a member of group '%s'.\n", gname);
    }
    else if (strncmp(resp, "MVDIR ERR_INTERNAL", 18) == 0) {
        printf("Error: Failed to move directory. Check paths.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 4. Handle COPDIR (Copy thư mục)
void handle_copdir(int sock) {
    char gname[MAX_NAME], oldp[256], newp[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter source path: ", oldp, sizeof(oldp));
    input_text("Enter destination path: ", newp, sizeof(newp));
    if (!gname[0] || !oldp[0] || !newp[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "COPDIR %s %s %s\n", gname, oldp, newp);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "COPDIR OK", 9) == 0) {
        printf("Success: Directory copied.\n");
    }
    else if (strncmp(resp, "COPDIR ERR_NOT_IN", 17) == 0) {
        printf("Error: You are not a member of group '%s'.\n", gname);
    }
    else if (strncmp(resp, "COPDIR ERR_INTERNAL", 19) == 0) {
        printf("Error: Failed to copy. Check if source exists.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 5. Handle RMDIR (Xóa thư mục)
void handle_rmdir(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter directory path to remove: ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMDIR %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "RMDIR OK", 8) == 0) {
        printf("Success: Directory removed.\n");
    }
    else if (strncmp(resp, "RMDIR ERR_NO_PERM", 17) == 0) {
        printf("Error: Permission denied. Only the group owner can delete directories.\n");
    }
    else if (strncmp(resp, "RMDIR ERR_INTERNAL", 18) == 0) {
        printf("Error: Failed to remove directory. (Check if empty or exists)\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 6. Handle REFILE (Đổi tên file)
void handle_refile(int sock) {
    char gname[MAX_NAME], oldp[256], newname[128];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter current file path: ", oldp, sizeof(oldp));
    input_text("Enter new filename: ", newname, sizeof(newname));
    if (!gname[0] || !oldp[0] || !newname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "REFILE %s %s %s\n", gname, oldp, newname);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "REFILE OK", 9) == 0) {
        printf("Success: File renamed to '%s'.\n", newname);
    }
    else if (strncmp(resp, "REFILE ERR_NO_PERM", 18) == 0) {
        printf("Error: Permission denied. Only the group owner can rename files.\n");
    }
    else if (strncmp(resp, "REFILE ERR_INTERNAL", 19) == 0) {
        printf("Error: Failed to rename file. Check paths.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 7. Handle MVFILE (Di chuyển file)
void handle_mvfile(int sock) {
    char gname[MAX_NAME], src[256], dst[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter source file path: ", src, sizeof(src));
    input_text("Enter destination directory path: ", dst, sizeof(dst));
    if (!gname[0] || !src[0] || !dst[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "MVFILE %s %s %s\n", gname, src, dst);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "MVFILE OK", 9) == 0) {
        printf("Success: File moved.\n");
    }
    else if (strncmp(resp, "MVFILE ERR_NOT_IN", 17) == 0) {
        printf("Error: You are not a member of group '%s'.\n", gname);
    }
    else if (strncmp(resp, "MVFILE ERR_INTERNAL", 19) == 0) {
        printf("Error: Failed to move file. Check paths.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 8. Handle COPFILE (Copy file)
void handle_copfile(int sock) {
    char gname[MAX_NAME], src[256], dst[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter source file path: ", src, sizeof(src));
    input_text("Enter destination directory path: ", dst, sizeof(dst));
    if (!gname[0] || !src[0] || !dst[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "COPFILE %s %s %s\n", gname, src, dst);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "COPFILE OK", 10) == 0) {
        printf("Success: File copied.\n");
    }
    else if (strncmp(resp, "COPFILE ERR_NOT_IN", 18) == 0) {
        printf("Error: You are not a member of group '%s'.\n", gname);
    }
    else if (strncmp(resp, "COPFILE ERR_INTERNAL", 20) == 0) {
        printf("Error: Failed to copy file. Check paths.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

// 9. Handle RMFILE (Xóa file)
void handle_rmfile(int sock) {
    char gname[MAX_NAME], path[256];
    input_text("Enter group name: ", gname, sizeof(gname));
    input_text("Enter file path to remove: ", path, sizeof(path));
    if (!gname[0] || !path[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "RMFILE %s %s\n", gname, path);
    send(sock, req, strlen(req), 0);

    char resp[BUF_SIZE];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    if (strncmp(resp, "RMFILE OK", 9) == 0) {
        printf("Success: File removed.\n");
    }
    else if (strncmp(resp, "RMFILE ERR_NO_PERM", 18) == 0) {
        printf("Error: Permission denied. Only the group owner can delete files.\n");
    }
    else if (strncmp(resp, "RMFILE ERR_INTERNAL", 19) == 0) {
        printf("Error: Failed to remove file (File might not exist).\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}

void handle_join_req_status(int sock) {
    printf("Checking your request status for all groups...\n");

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "JOIN_REQ_STATUS\n");
    send(sock, req, strlen(req), 0);

    char resp[4096];
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // --- SỬA ĐOẠN NÀY ---
    // Cập nhật header mới: "JOIN_REQ_STATUS OK" (độ dài 18 ký tự)
    if (strncmp(resp, "JOIN_REQ_STATUS OK", 18) == 0) {
        char *p = resp + 18; // Nhảy qua chuỗi "JOIN_REQ_STATUS OK"
        
        if (strncmp(p, " EMPTY", 6) == 0 || *p == '\0' || *p == '\n') {
            printf("You have not joined or requested to join any group yet.\n");
        } else {
            printf("\n--- YOUR GROUP STATUS ---\n");
            printf("%-20s | %-15s\n", "Group Name", "Status");
            printf("---------------------+----------------\n");
            
            // Parse nội dung: " group1:MEMBER group2:PENDING"
            char *token = strtok(p, " \n");
            while (token != NULL) {
                char *colon = strchr(token, ':');
                if (colon) {
                    *colon = '\0';
                    char *gname = token;
                    char *status = colon + 1;
                    
                    printf("%-20s | %s\n", gname, status);
                }
                token = strtok(NULL, " \n");
            }
            printf("--------------------------------------\n");
        }
    } 
    else if (strncmp(resp, "STATUS ERR_NOT_LOGIN", 20) == 0) {
        printf("Error: You are not logged in.\n");
    }
    else {
        printf("Server Response: %s", resp);
    }
}

// // Kiểm tra danh sách mời của nhóm (Chỉ chủ nhóm)
// void handle_invite_status(int sock) {
//     char gname[MAX_NAME];
//     input_text("Enter group name (you must be the owner): ", gname, sizeof(gname));
//     if (!gname[0]) return;

//     char req[BUF_SIZE];
//     snprintf(req, sizeof(req), "INVITE_STATUS %s\n", gname);
//     send(sock, req, strlen(req), 0);

//     char resp[4096]; // Buffer lớn cho danh sách
//     if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

//     if (strncmp(resp, "INVITE_STATUS OK", 16) == 0) {
//         char *p = resp + 16; 
//         if (strncmp(p, " EMPTY", 6) == 0) {
//             printf("This group has not invited anyone yet.\n");
//         } else {
//             printf("\n--- INVITE HISTORY ---\n");
//             printf("%-20s | %-15s\n", "User", "Status");
//             printf("---------------------+----------------\n");
            
//             // Server gửi: " User1:PENDING User2:ACCEPTED"
//             char *token = strtok(p, " \n");
//             while (token != NULL) {
//                 char *colon = strchr(token, ':');
//                 if (colon) {
//                     *colon = '\0';
//                     char *user = token;
//                     char *status = colon + 1;
                    
//                     // In màu hoặc định dạng cho đẹp
//                     printf("%-20s | %s\n", user, status);
//                 }
//                 token = strtok(NULL, " \n");
//             }
//             printf("--------------------------------------\n");
//         }
//     } 
//     else if (strncmp(resp, "STATUS ERR_NO_PERM", 18) == 0) {
//         printf("Error: You are not the group owner.\n");
//     } 
//     else {
//         printf("Server: %s", resp);
//     }
// }
void handle_invite_status(int sock) {
    char gname[MAX_NAME];
    input_text("Enter group name (you must be the owner): ", gname, sizeof(gname));
    if (!gname[0]) return;

    char req[BUF_SIZE];
    snprintf(req, sizeof(req), "INVITE_STATUS %s\n", gname);
    send(sock, req, strlen(req), 0);

    char resp[4096]; 
    if (recv_line(sock, resp, sizeof(resp)) <= 0) return;

    // Xử lý thành công
    if (strncmp(resp, "INVITE_STATUS OK", 16) == 0) {
        char *p = resp + 16; 
        while (*p == ' ') p++; // Bỏ khoảng trắng thừa

        if (strncmp(p, "EMPTY", 5) == 0 || *p == '\0' || *p == '\n') {
            printf("This group has not invited anyone yet.\n");
        } else {
            printf("\n--- INVITE HISTORY ---\n");
            printf("%-20s | %-15s\n", "User", "Status");
            printf("---------------------+----------------\n");
            
            char *token = strtok(p, " \n");
            while (token != NULL) {
                char *colon = strchr(token, ':');
                if (colon) {
                    *colon = '\0';
                    char *user = token;
                    char *status = colon + 1;
                    printf("%-20s | %s\n", user, status);
                }
                token = strtok(NULL, " \n");
            }
            printf("--------------------------------------\n");
        }
    } 
    // Xử lý lỗi
    else if (strncmp(resp, "INVITE_STATUS ERR_GRP_NOT_FOUND", 31) == 0) {
        printf("Error: Group '%s' does not exist.\n", gname);
    }
    else if (strncmp(resp, "INVITE_STATUS ERR_NO_PERM", 25) == 0) {
        printf("Error: You are not the owner of this group.\n");
    } 
    else if (strncmp(resp, "INVITE_STATUS ERR_NOT_LOGIN", 27) == 0) {
        printf("Error: You must login first.\n");
    }
    else {
        printf("Server Error: %s", resp);
    }
}