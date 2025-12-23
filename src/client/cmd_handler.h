#ifndef CMD_HANDLER_H
#define CMD_HANDLER_H

// Group
void handle_lsmem(int sock);
void handle_leave(int sock);
void handle_rmmem(int sock);

// Request
void handle_join_request(int sock);
void handle_view_request(int sock);
void handle_approve_request(int sock);

// Invite
void handle_invite(int sock);
void handle_view_invite(int sock);
void handle_accept_invite(int sock);

// Filesystem
void handle_lsdir(int sock);
void handle_mkdir(int sock);
void handle_redir(int sock);
void handle_mvdir(int sock);
void handle_copdir(int sock);
void handle_rmdir(int sock);
void handle_refile(int sock);
void handle_mvfile(int sock);
void handle_copfile(int sock);
void handle_rmfile(int sock);
void handle_join_req_status(int sock);
void handle_invite_status(int sock);
#endif