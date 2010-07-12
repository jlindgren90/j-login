/* J Login >> screen.c */
/* John Lindgren */
/* January 28, 2010 */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fcntl.h>
#include <stropts.h>
#include <sys/stat.h>
#include <unistd.h>

#include <linux/vt.h>

#include "screen.h"
#include "utils.h"

static int vt_handle;

void init_vt (void) {
   vt_handle = open ("/dev/console", O_RDONLY);
   if (vt_handle < 0)
      fail_two ("open", "/dev/console");
   fcntl (vt_handle, F_SETFD, FD_CLOEXEC);
}

void close_vt (void) {
   if (close (vt_handle))
      fail_two ("close", "/dev/console");
}

static int get_open_vt (void) {
  int vt;
   if (ioctl (vt_handle, VT_OPENQRY, & vt))
      fail ("VT_OPENQRY");
   return vt;
}

static void set_vt (int vt) {
   if (ioctl (vt_handle, VT_ACTIVATE, vt))
      fail ("VT_ACTIVATE");
   if (ioctl (vt_handle, VT_WAITACTIVE, vt))
      fail ("VT_WAITACTIVE");
}

static int get_open_display (void) {
  int display;
  char work [256];
   for (display = 0; display < 100; display ++) {
      snprintf (work, sizeof work, "/tmp/.X%d-lock", display);
      if (! exist (work))
         return display;
   }
   error ("no available display");
   abort (); /* never reached */
}

static int launch_x (int vt, int display) {
  char display_opt [256], vt_opt [256];
  char * args [4];
   snprintf (display_opt, sizeof display_opt, ":%d", display);
   snprintf (vt_opt, sizeof vt_opt, "vt%d", vt);
   args [0] = "Xorg";
   args [1] = display_opt;
   args [2] = vt_opt;
   args [3] = 0;
   return launch (args);
}

static void wait_x (int process, int display) {
  char work [256];
   snprintf (work, sizeof work, "/tmp/.X11-unix/X%d", display);
   while (! exist (work)) {
      if (exited (process))
         error ("X exited");
      usleep (100000);
   }
}

static Display * grab_x (int display) {
  char work [16];
  Display * handle;
   snprintf (work, sizeof work, ":%d", display);
   handle = XOpenDisplay (work);
   if (! handle)
      fail ("XOpenDisplay");
   fcntl (ConnectionNumber (handle), F_SETFD, FD_CLOEXEC);
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

void popup_x (struct console * console) {
   set_vt (console->vt);
}

void close_x (struct console * console) {
   ungrab_x (console->handle);
   my_kill (console->process);
   free (console);
}

void set_display (int display) {
  char work [256];
   snprintf (work, sizeof work, ":%d", display);
   my_setenv ("DISPLAY", work);
}

char block_x (Display * handle, Window window)
{
  int count;
  char mouse = 0, keyboard = 0;
   for (count = 0; count < 10; count ++)
   {
      if (! keyboard)
         keyboard = (XGrabKeyboard (handle, window, True, GrabModeAsync,
          GrabModeAsync, CurrentTime) == GrabSuccess);
      if (! mouse)
         mouse = (XGrabPointer (handle, window, True, 0, GrabModeAsync,
          GrabModeAsync, window, None, CurrentTime) == GrabSuccess);
      if (keyboard && mouse)
         return 1;
      usleep (100000);
   }
   unblock_x (handle);
   return 0;
}

void unblock_x (Display * handle)
{
   XUngrabPointer (handle, CurrentTime);
   XUngrabKeyboard (handle, CurrentTime);
}
