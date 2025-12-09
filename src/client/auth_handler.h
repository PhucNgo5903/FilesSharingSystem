#ifndef AUTH_HANDLER_H
#define AUTH_HANDLER_H

void req_signup(int sock);
int req_signin(int sock, char *current_user);
void req_logout(int sock);
void req_mkgrp(int sock);
void req_lsgrp(int sock);

#endif