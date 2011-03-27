/* J Login >> utils-linux.c */
/* John Lindgren */
/* March 27, 2011 */

#define _BSD_SOURCE
#define _XOPEN_SOURCE

#include <limits.h>
#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <string.h>
#include <shadow.h>
#include <unistd.h>

#include "utils.h"

int get_user_id (const char * user) {
   const struct passwd * p = getpwnam (user);
   if (! p)
      return -1;
   return p->pw_uid;
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
      fail_two ("getpwnam", user);
   if (setgid (p->pw_gid) < 0)
      fail ("setgid");
   if (initgroups (user, p->pw_gid) < 0)
      fail ("initgroups");
   if (setuid (p->pw_uid) < 0)
      fail ("setuid");
   if (chdir (p->pw_dir) < 0)
      fail_two ("chdir", p->pw_dir);
   my_setenv ("USER", user);
   my_setenv ("LOGNAME", user);
   my_setenv ("HOME", p->pw_dir);
   char path [256 + NAME_MAX];
   snprintf (path, sizeof path, "%s:/usr/games:%s/bin", p->pw_uid ?
    "/bin:/usr/bin" : "/bin:/sbin:/usr/bin:/usr/sbin", p->pw_dir);
   my_setenv ("PATH", path);
}
