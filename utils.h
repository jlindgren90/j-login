/* J Login >> utils.h */
/* John Lindgren */
/* December 20, 2009 */

#define NAME "J Login"

void error (char * message);
void fail (char * func);
void fail_two (char * func, char * param);
void * my_malloc (int size);
char * my_strdup (char * string);
void my_setenv (char * name, char * value);
char exist (char * file);
int launch (char * * args);
char exited (int process);
void wait_for_exit (int process);
void my_kill (int process);
void set_user (char * user);
int launch_set_user (char * user, char * * args);
