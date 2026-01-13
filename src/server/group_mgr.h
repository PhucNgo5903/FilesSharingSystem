#ifndef GROUP_MGR_H
#define GROUP_MGR_H
#define MAX_NAME 64

void remove_line_from_file(const char *path, const char *content_to_remove);
// --- Các hàm cũ ---
int handle_mkgrp_logic(const char *group_name, const char *creator_name);
int check_user_in_group(const char *username, const char *group_name);
void get_group_list_string(char *buffer, int size);

// --- Các hàm MỚI (Member, Request, Invite) ---
void ensure_group_dirs(); // Tạo folder data/requests, data/invites

// Group Member Logic
int is_group_owner(const char *group_name, const char *user);
void get_group_members_string(const char *group_name, char *buffer, int size);
int remove_member_from_group(const char *group_name, const char *user);
int add_member_to_group(const char *group_name, const char *user);

// Request Logic
int add_join_request(const char *group_name, const char *user);
void build_view_request_response(const char *group_name, char *buffer, int size);
int approve_join_request(const char *group_name, const char *user); // Wrapper: Add member + Rm req
void build_join_req_status_all_response(const char *user, char *buffer, int size);
int group_exists(const char *group_name);

// Invite Logic
int add_invite(const char *user, const char *group_name);
void build_view_invite_response(const char *user, char *buffer, int size);
int accept_invite(const char *user, const char *group_name); // Wrapper: Add member + Rm invite
int check_user_exists_system(const char *username);

// NEW STATUS FUNCTIONS
int check_join_req_status(const char *user, const char *group_name);
int check_invite_status(const char *group_name, const char *target_user);
void build_invite_status_all_response(const char *group_name, char *buffer, int size);
void update_invite_status_accepted(const char *group_name, const char *user, const char *new_status);
#endif