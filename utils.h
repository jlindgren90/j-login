/* J Login >> utils.h */
/* John Lindgren */
/* September 11, 2011 */

#ifndef JLOGIN_UTILS_H
#define JLOGIN_UTILS_H

#define NAME "J Login"

#define SPRINTF(n,...) \
 char n[snprintf (0, 0, __VA_ARGS__) + 1]; \
 snprintf (n, sizeof n, __VA_ARGS__);

void error (const char * message);
void fail (const char * func);
void fail2 (const char * func, const char * param);
void * my_malloc (int size);
char * my_strdup (const char * string);
void my_setenv (const char * name, const char * value);
char exist (const char * file);
void wait_for_exist (const char * folder, const char * file);
int launch (const char * const * args);
char exited (int process);
void wait_for_exit (int process);
void my_kill (int process);
int get_user_id (const char * user);
char check_password (const char * name, const char * password);
void set_user (const char * user);
int launch_set_user (const char * user, const char * password, int vt,
 int display, const char * const * args);

#endif
