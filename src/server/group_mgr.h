#ifndef GROUP_MGR_H
#define GROUP_MGR_H

// Cập nhật: Thêm tham số creator_name
int handle_mkgrp_logic(const char *group_name, const char *creator_name);

// Hàm mới: Kiểm tra thành viên
int check_user_in_group(const char *username, const char *group_name);

void get_group_list_string(char *buffer, int size);

#endif