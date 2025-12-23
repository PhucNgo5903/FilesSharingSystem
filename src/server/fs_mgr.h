#ifndef FS_MGR_H
#define FS_MGR_H

void ensure_fs_dirs();
int handle_mkdir(const char *group_name, const char *relpath);
int handle_lsdir(const char *group_name, const char *relpath, char *response, int max_len);
int handle_rmdir(const char *group_name, const char *relpath);
int handle_copdir(const char *group_name, const char *src_rel, const char *dst_rel);
int handle_mvdir(const char *group_name, const char *src_rel, const char *dst_rel);
int handle_redir(const char *group_name, const char *old_rel, const char *new_name);

int handle_refile(const char *group_name, const char *old_rel, const char *new_name);
int handle_rmfile(const char *group_name, const char *relpath);
int handle_copfile(const char *group_name, const char *src_rel, const char *dst_dir_rel);
int handle_mvfile(const char *group_name, const char *src_rel, const char *dst_dir_rel);

#endif