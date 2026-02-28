#ifndef USER_H
#define USER_H

#define MAX_USERS    8
#define MAX_USERNAME 16

void user_init(void);
int  user_login(void);                     /* Interactive login, returns 1 on success */
const char *user_current(void);            /* Get current username */
int  user_register(const char *username);  /* Register a new user */

#endif
