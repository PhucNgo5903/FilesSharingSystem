#ifndef AUTH_MGR_H
#define AUTH_MGR_H

int handle_signup_logic(const char *user, const char *pass);
int handle_signin_logic(const char *user, const char *pass);
void load_users_from_file();

#endif