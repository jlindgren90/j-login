/* J Login >> utils.c */
/* John Lindgren */
/* May 26, 2012 */

#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <poll.h>
#include <pwd.h>
#include <shadow.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

#include "utils.h"

void error (const char * message) {
   fprintf (stderr, "%s: %s.\n", NAME, message);
   exit (1);
}

void fail (const char * func) {
   fprintf (stderr, "%s: %s failed: %s.\n", NAME, func, strerror (errno));
   exit (1);
}

void fail2 (const char * func, const char * param) {
   fprintf (stderr, "%s: %s failed for %s: %s.\n", NAME, func, param, strerror
    (errno));
   exit (1);
}

void * my_malloc (int size) {
   void * mem = malloc (size);
   if (! mem)
      fail ("malloc");
   return mem;
}

char * my_strdup (const char * string) {
   int size = strlen (string) + 1;
   char * new = my_malloc (size);
   memcpy (new, string, size);
   return new;
}

void my_setenv (const char * name, const char * value) {
   if (setenv (name, value, 1))
      fail ("setenv");
}

char exist (const char * name) {
   struct stat buf;
   if (! lstat (name, & buf))
      return 1;
   if (errno != ENOENT)
      fail2 ("stat", name);
   return 0;
}

void wait_for_exist (const char * folder, const char * file)
{
   int handle = inotify_init ();
   if (handle < 0)
      fail ("inotify_init");
   if (fcntl (handle, F_SETFD, FD_CLOEXEC) < 0)
      fail2 ("FD_CLOEXEC", "inotify");
   if (fcntl (handle, F_SETFL, O_NONBLOCK) < 0)
      fail2 ("O_NONBLOCK", "inotify");
   if (inotify_add_watch (handle, folder, IN_CREATE) < 0)
      fail2 ("inotify_add_watch", folder);
   while (! exist (file)) {
      struct pollfd polldata = {.fd = handle, .events = POLLIN};
      if (poll (& polldata, 1, 1000) < 0)
         fail2 ("poll", "inotify");
      struct inotify_event event;
      while (read (handle, & event, sizeof event) == sizeof event)
         ;
   }
   close (handle);
}

static void clear_signals (void) {
   sigset_t signals;
   sigemptyset (& signals);
   if (sigprocmask (SIG_SETMASK, & signals, 0) < 0)
      fail ("sigprocmask");
}

int launch (const char * const * args) {
   int process = fork ();
   if (! process) {
      clear_signals ();
      execvp (args[0], (char * const *) args);
      fail2 ("execvp", args[0]);
   } else if (process < 0)
      fail ("fork");
   return process;
}

char exited (int process) {
   int status;
   int result = waitpid (process, & status, WNOHANG);
   return (result == process && ! WIFSTOPPED (status) && ! WIFCONTINUED
    (status)) || (result < 0 && errno != EINTR);
}

void wait_for_exit (int process) {
   int status;
   while (waitpid (process, & status, 0) != process || WIFSTOPPED (status) ||
    WIFCONTINUED (status))
      ;
}

void my_kill (int process) {
   if (kill (process, SIGTERM))
      fail ("kill");
   wait_for_exit (process);
}

int get_user_id (const char * user) {
   const struct passwd * p = getpwnam (user);
   return p ? p->pw_uid : -1;
}

char check_password (const char * name, const char * password) {
   const struct passwd * p = getpwnam (name);
   if (! p)
      return 0;
   const char * right = p->pw_passwd;
   if (! strcmp (right, "x")) {
      const struct spwd * s = getspnam (name);
      if (! s)
         return 0;
      right = s->sp_pwdp;
   }
   const char * crypted = crypt (password, right);
   if (! crypted)
      fail ("crypt");
   return ! strcmp (crypted, right);
}

void set_user (const char * user) {
   const struct passwd * p = getpwnam (user);
   if (! p)
      fail2 ("getpwnam", user);
   if (setgid (p->pw_gid) < 0)
      fail ("setgid");
   if (initgroups (user, p->pw_gid) < 0)
      fail ("initgroups");
   if (setuid (p->pw_uid) < 0)
      fail ("setuid");
   if (chdir (p->pw_dir) < 0)
      fail2 ("chdir", p->pw_dir);
   my_setenv ("USER", user);
   my_setenv ("LOGNAME", user);
   my_setenv ("HOME", p->pw_dir);
   SPRINTF (path, "/bin:/sbin:/usr/bin:/usr/sbin:/usr/games:%s/bin", p->pw_dir);
   my_setenv ("PATH", path);
}

int launch_set_user (const char * user, const char * const * args) {
   int process = fork ();
   if (! process) {
      clear_signals ();
      set_user (user);
      execvp (args[0], (char * const *) args);
      fail2 ("execvp", args[0]);
   } else if (process < 1)
      fail ("fork");
   return process;
}
