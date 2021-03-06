/*
 * J-Login - screen.c
 * Copyright 2010-2019 John Lindgren
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

#include <X11/extensions/scrnsaver.h>

#include "screen.h"
#include "utils.h"

static int vt_handle;
static int next_vt = 7;

void init_vt (void) {
   if ((vt_handle = open ("/dev/console", O_RDONLY)) < 0)
      fail2 ("open", "/dev/console");
   if (fcntl (vt_handle, F_SETFD, FD_CLOEXEC) < 0)
      fail2 ("FD_CLOEXEC", "/dev/console");
}

void set_vt (int vt) {
   if (ioctl (vt_handle, VT_ACTIVATE, vt) < 0)
      fail ("VT_ACTIVATE");
   if (ioctl (vt_handle, VT_WAITACTIVE, vt) < 0)
      fail ("VT_WAITACTIVE");
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

void start_x (int * vt, int * display) {
   * vt = next_vt ++;
   * display = get_open_display ();
   SPRINTF (display_opt, ":%d", * display);
   SPRINTF (vt_opt, "vt%d", * vt);
   launch ((const char * []){"X", display_opt, vt_opt, NULL});
   SPRINTF (path, "/tmp/.X11-unix/X%d", * display);
   wait_for_exist ("/tmp", "/tmp/.X11-unix");
   wait_for_exist ("/tmp/.X11-unix", path);
}

void ssaver_init (Display * display) {
   int event_base, error_base;
   if (! XScreenSaverQueryExtension (display, & event_base, & error_base))
      fail ("XScreenSaverQueryExtension");
}

int ssaver_active_ms (Display * display) {
   XScreenSaverInfo info;
   if (! XScreenSaverQueryInfo (display, DefaultRootWindow (display), & info))
      fail ("XScreenSaverQueryInfo");
   return info.state == ScreenSaverOff ? -info.til_or_since : info.til_or_since;
}
