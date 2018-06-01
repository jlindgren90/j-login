/*
 * J-Login - screen.c
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

#include <fcntl.h>
#include <linux/vt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include "screen.h"
#include "utils.h"

#define FIRST_VT 7
#define MAX_VT 16

static int vt_handle;
static char vt_used[MAX_VT];

void init_vt (void) {
   if ((vt_handle = open ("/dev/console", O_RDONLY)) < 0)
      fail2 ("open", "/dev/console");
   if (fcntl (vt_handle, F_SETFD, FD_CLOEXEC) < 0)
      fail2 ("FD_CLOEXEC", "/dev/console");
}

void close_vt (void) {
   if (close (vt_handle) < 0)
      fail2 ("close", "/dev/console");
}

int get_vt (void) {
   struct vt_stat s;
   if (ioctl (vt_handle, VT_GETSTATE, & s) < 0)
      fail ("VT_GETSTATE");
   return s.v_active;
}

int get_open_vt (void) {
   for (int vt = FIRST_VT; vt < FIRST_VT + MAX_VT; vt ++) {
      if (! vt_used[vt - FIRST_VT]) {
         vt_used[vt - FIRST_VT] = 1;
         return vt;
      }
   }
   error ("too many open VTs");
   return -1;
}

void release_vt (int vt) {
   if (vt >= FIRST_VT && vt < FIRST_VT + MAX_VT)
      vt_used[vt - FIRST_VT] = 0;
}

void set_vt (int vt) {
   if (ioctl (vt_handle, VT_ACTIVATE, vt) < 0)
      fail ("VT_ACTIVATE");
   if (ioctl (vt_handle, VT_WAITACTIVE, vt) < 0)
      fail ("VT_WAITACTIVE");
}

void lock_vt (void) {
#if 0
   if (ioctl (vt_handle, VT_LOCKSWITCH, 0) < 0)
      fail ("VT_LOCKSWITCH");
#endif
}

void unlock_vt (void) {
#if 0
   if (ioctl (vt_handle, VT_UNLOCKSWITCH, 0) < 0)
      fail ("VT_UNLOCKSWITCH");
#endif
}

static int get_open_display (void) {
   for (int display = 0; display < 100; display ++) {
      SPRINTF (path, "/tmp/.X%d-lock", display);
      if (! exist (path))
         return display;
   }
   error ("no available display");
   return -1;
}

static int launch_x (int vt, int display) {
   SPRINTF (display_opt, ":%d", display);
   SPRINTF (vt_opt, "vt%d", vt);
   const char * const args[] = {"X", display_opt, vt_opt, 0};
   return launch (args);
}

static void wait_x (int process, int display) {
   SPRINTF (path, "/tmp/.X11-unix/X%d", display);
   wait_for_exist ("/tmp", "/tmp/.X11-unix");
   wait_for_exist ("/tmp/.X11-unix", path);
}

static Display * grab_x (int display) {
   SPRINTF (name, ":%d", display);
   Display * handle = XOpenDisplay (name);
   if (! handle)
      fail2 ("XOpenDisplay", name);
   if (fcntl (ConnectionNumber (handle), F_SETFD, FD_CLOEXEC) < 0)
      fail2 ("FD_CLOEXEC", name);
   return handle;
}

static void ungrab_x (Display * handle) {
   XCloseDisplay (handle);
}

struct console * start_x (void) {
   struct console * console = my_malloc (sizeof (struct console));
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
   release_vt (console->vt);
   free (console);
}

void set_display (int display) {
   SPRINTF (name, ":%d", display);
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
