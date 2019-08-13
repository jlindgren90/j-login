/*
 * J-Login - consolekit.c
 * Copyright 2019 John Lindgren
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

#include <stdio.h>

#include <ck-connector.h>
#include <dbus/dbus.h>

#include "consolekit.h"
#include "utils.h"

void * open_consolekit (uid_t uid, int vt, int display) {
   const char * type = "x11";
   const char * dev = "";
   dbus_bool_t local = true;
   SPRINTF (disp_buf, ":%d", display);
   SPRINTF (tty_buf, "/dev/tty%d", vt);
   const char * disp = disp_buf, * tty = tty_buf;
   CkConnector * handle = ck_connector_new ();
   if (! handle)
      fail ("ck_connector_new");
   DBusError err;
   dbus_error_init (& err);
   if (! ck_connector_open_session_with_parameters (handle, & err, "unix-user",
    & uid, "session-type", & type, "display-device", & dev, "is-local", & local,
    "x11-display", & disp, "x11-display-device", & tty, NULL))
      fail ("ck_connector_open_session_with_parameters");
   my_setenv ("XDG_SESSION_COOKIE", ck_connector_get_cookie (handle));
   return handle;
}

void close_consolekit (void * handle) {
   DBusError err;
   dbus_error_init (& err);
   ck_connector_close_session (handle, & err);
   ck_connector_unref (handle);
}
