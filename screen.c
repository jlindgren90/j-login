/* J Login >> screen.c */
/* John Lindgren */
/* September 11, 2011 */

#include <limits.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include <linux/vt.h>
#include <sys/ioctl.h>

#include "screen.h"
#include "utils.h"

static int vt_handle;

void init_vt (void) {
   vt_handle = open ("/dev/console", O_RDONLY);
   if (vt_handle < 0)
      fail_two ("open", "/dev/console");
   if (fcntl (vt_handle, F_SETFD, FD_CLOEXEC) < 0)
      fail_two ("FD_CLOEXEC", "/dev/console");
}

void close_vt (void) {
   if (close (vt_handle) < 0)
      fail_two ("close", "/dev/console");
}

int get_vt (void) {
   struct vt_stat s;
   if (ioctl (vt_handle, VT_GETSTATE, & s) < 0)
      fail ("VT_GETSTATE");
   return s.v_active;
}

int get_open_vt (void) {
   int vt;
   if (ioctl (vt_handle, VT_OPENQRY, & vt) < 0)
      fail ("VT_OPENQRY");
   return vt;
}

void set_vt (int vt) {
   if (ioctl (vt_handle, VT_ACTIVATE, vt) < 0)
      fail ("VT_ACTIVATE");
   if (ioctl (vt_handle, VT_WAITACTIVE, vt) < 0)
      fail ("VT_WAITACTIVE");
}

void lock_vt (void) {
   if (ioctl (vt_handle, VT_LOCKSWITCH, 0) < 0)
      fail ("VT_LOCKSWITCH");
}

void unlock_vt (void) {
   if (ioctl (vt_handle, VT_UNLOCKSWITCH, 0) < 0)
      fail ("VT_UNLOCKSWITCH");
}

static int get_open_display (void) {
   for (int display = 0; display < 100; display ++) {
      char path[NAME_MAX];
      snprintf (path, sizeof path, "/tmp/.X%d-lock", display);
      if (! exist (path))
         return display;
   }
   error ("no available display");
}

static int launch_x (int vt, int display) {
   char display_opt[16], vt_opt[16];
   snprintf (display_opt, sizeof display_opt, ":%d", display);
   snprintf (vt_opt, sizeof vt_opt, "vt%d", vt);
   const char * const args[4] = {"X", display_opt, vt_opt, 0};
   return launch (args);
}

static void wait_x (int process, int display) {
   char path[32];
   snprintf (path, sizeof path, "/tmp/.X11-unix/X%d", display);
   wait_for_exist ("/tmp", "/tmp/.X11-unix");
   wait_for_exist ("/tmp/.X11-unix", path);
}

static Display * grab_x (int display) {
   char name[16];
   snprintf (name, sizeof name, ":%d", display);
   Display * handle = XOpenDisplay (name);
   if (! handle)
      fail_two ("XOpenDisplay", name);
   if (fcntl (ConnectionNumber (handle), F_SETFD, FD_CLOEXEC) < 0)
      fail_two ("FD_CLOEXEC", name);
   return handle;
}

static void ungrab_x (Display * handle) {
   XCloseDisplay (handle);
}

struct console * start_x (void) {
   struct console * console = malloc (sizeof (struct console));
   console->vt = get_open_vt ();
   set_vt (console->vt);
   console->display = get_open_display ();
   console->process = launch_x (console->vt, console->display);
   wait_x (console->process, console->display);
   console->handle = grab_x (console->display);
   return console;
}

void popup_x (const struct console * console) {
   set_vt (console->vt);
}

void close_x (struct console * console) {
   ungrab_x (console->handle);
   my_kill (console->process);
   free (console);
}

void set_display (int display) {
   char name[16];
   snprintf (name, sizeof name, ":%d", display);
   my_setenv ("DISPLAY", name);
}

char block_x (Display * handle, Window window) {
   char mouse = 0, keyboard = 0;
   for (int count = 0; count < 50; count ++)
   {
      if (! keyboard)
         keyboard = (XGrabKeyboard (handle, window, True, GrabModeAsync,
          GrabModeAsync, CurrentTime) == GrabSuccess);
      if (! mouse)
         mouse = (XGrabPointer (handle, window, True, 0, GrabModeAsync,
          GrabModeAsync, window, None, CurrentTime) == GrabSuccess);
      if (keyboard && mouse)
         return 1;
      struct timespec delay = {.tv_sec = 0, .tv_nsec = 20000000};
      nanosleep (& delay, 0);
   }
   unblock_x (handle);
   return 0;
}

void unblock_x (Display * handle) {
   XUngrabPointer (handle, CurrentTime);
   XUngrabKeyboard (handle, CurrentTime);
}
