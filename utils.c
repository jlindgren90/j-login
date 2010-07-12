/* J Login >> utils.c */
/* John Lindgren */
/* December 20, 2009 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils.h"

void error (char * message) {
   fprintf (stderr, "%s: %s.\n", NAME, message);
   exit (1);
}

void fail (char * func) {
   fprintf (stderr, "%s: %s failed: %s.\n", NAME, func, strerror (errno));
   exit (1);
}

void fail_two (char * func, char * param) {
   fprintf (stderr, "%s: %s failed for %s: %s.\n", NAME, func, param,
    strerror (errno));
   exit (1);
}

void * my_malloc (int size) {
  void * mem;
	mem = malloc (size);
	if (! mem)
		fail ("malloc");
	return mem;
}

char * my_strdup (char * string) {
  char * new;
	new = strdup (string);
	if (! new)
		fail ("strdup");
	return new;
}

void my_setenv (char * name, char * value) {
   if (setenv (name, value, 1))
      fail ("setenv");
}

char exist (char * name) {
  struct stat buf;
   if (! lstat (name, & buf))
      return 1;
   if (errno == ENOENT)
      return 0;
   fail_two ("stat", name);
   abort (); /* never reached */
}

static void clear_signals (void)
{
  sigset_t signals;
   sigemptyset (& signals);
   if (sigprocmask (SIG_SETMASK, & signals, 0) < 0)
      fail ("sigprocmask");
}

int launch (char * * args) {
  int result;
   result = fork ();
   if (result < 0)
      fail ("fork");
   if (result)
      return result;
   clear_signals ();
   execvp (args [0], args);
   fail_two ("execvp", args [0]);
   abort (); /* never reached */
}

char exited (int process) {
  int result, status;
   result = waitpid (process, & status, WNOHANG);
   return (result == process && ! WIFSTOPPED (status) && ! WIFCONTINUED (status))
    || (result < 0 && errno != EINTR);
}

void wait_for_exit (int process) {
  int status;
   while (waitpid (process, & status, 0) != process || WIFSTOPPED (status) ||
    WIFCONTINUED (status));
}

void my_kill (int process) {
   if (kill (process, SIGTERM))
      fail ("kill");
   wait_for_exit (process);
}

void set_user (char * user) {
  struct passwd * buf;
  char work [256];
	buf = getpwnam (user);
	if (! buf)
		fail_two ("getpwnam", user);
	if (buf->pw_uid)
		snprintf (work, sizeof work, "/bin:/usr/bin:/usr/games:%s/bin",
		 buf->pw_dir);
	else
		snprintf (work, sizeof work,
		 "/bin:/sbin:/usr/bin:/usr/sbin:/usr/games:%s/bin", buf->pw_dir);
	if (setgid (buf->pw_gid))
		fail ("setgid");
	if (initgroups (user, buf->pw_gid))
		fail ("initgroups");
	if (setuid (buf->pw_uid))
		fail ("setuid");
	my_setenv ("USER", user);
	my_setenv ("LOGNAME", user);
	my_setenv ("HOME", buf->pw_dir);
	my_setenv ("PATH", work);
	if (chdir (buf->pw_dir))
		fail_two ("chdir", buf->pw_dir);
}

int launch_set_user (char * user, char * * args) {
  int result;
	result = fork ();
	if (result < 0)
		fail ("fork");
	if (result)
		return result;
   clear_signals ();
	set_user (user);
	execvp (args [0], args);
	fail_two ("execvp", args [0]);
	abort (); /* never reached */
}
