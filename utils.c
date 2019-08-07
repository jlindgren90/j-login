/*
 * J-Login - utils.c
 * Copyright 2010-2015 John Lindgren
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions, and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions, and the following disclaimer in the documentation
 *    provided with the distribution.
 *
 * This software is provided "as is" and without any warranty, express or
 * implied. In no event shall the authors be liable for any damages arising from
 * the use of this software.
 */

#define _DEFAULT_SOURCE
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

#include "pam.h"
#include "screen.h"
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
   if (setenv (name, value, true))
      fail ("setenv");
}

bool exist (const char * name) {
   struct stat buf;
   if (! lstat (name, & buf))
      return true;
   if (errno != ENOENT)
      fail2 ("stat", name);
   return false;
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
   if (sigprocmask (SIG_SETMASK, & signals, NULL) < 0)
      fail ("sigprocmask");
}

pid_t launch (const char * const * args) {
   pid_t process = fork ();
   if (! process) {
      clear_signals ();
      execvp (args[0], (char * const *) args);
      fail2 ("execvp", args[0]);
   } else if (process < 0)
      fail ("fork");
   return process;
}

pid_t launch_set_display (const char * const * args, int display) {
   pid_t process = fork ();
   if (! process) {
      SPRINTF (disp_name, ":%d", display);
      my_setenv ("DISPLAY", disp_name);
      clear_signals ();
      execvp (args[0], (char * const *) args);
      fail2 ("execvp", args[0]);
   } else if (process < 0)
      fail ("fork");
   return process;
}

bool exited (pid_t process) {
   int status;
   pid_t result = waitpid (process, & status, WNOHANG);
   return (result == process && ! WIFSTOPPED (status) && ! WIFCONTINUED
    (status)) || (result < 0 && errno != EINTR);
}

void wait_for_exit (pid_t process) {
   int status;
   while (waitpid (process, & status, 0) != process || WIFSTOPPED (status) ||
    WIFCONTINUED (status))
      ;
}

void my_kill (pid_t process) {
   if (kill (process, SIGTERM))
      fail ("kill");
   wait_for_exit (process);
}

bool check_password (const char * name, const char * password) {
   const struct passwd * p = getpwnam (name);
   if (! p)
      return false;
   const char * right = p->pw_passwd;
   if (! strcmp (right, "x")) {
      const struct spwd * s = getspnam (name);
      if (! s)
         return false;
      right = s->sp_pwdp;
   }
   const char * crypted = crypt (password, right);
   return crypted && ! strcmp (crypted, right);
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

pid_t launch_set_user (const char * user, const char * password, int vt,
 int display, const char * const * args) {
   pid_t process = fork ();
   if (! process) {
      SPRINTF (disp_name, ":%d", display);
      my_setenv ("DISPLAY", disp_name);
      void * pam = open_pam (user, password, vt, display);
      pid_t process2 = fork ();
      if (! process2) {
         clear_signals ();
         set_user (user);
         execvp (args[0], (char * const *) args);
         fail2 ("execvp", args[0]);
      } else if (process2 < 1)
         fail ("fork");
      wait_for_exit (process2);
      close_pam (pam);
      _exit (0);
   } else if (process < 1)
      fail ("fork");
   return process;
}
