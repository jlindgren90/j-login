/*
 * J-Login - pam.c
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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <security/pam_appl.h>

#include "pam.h"
#include "utils.h"

static int callback (int count, const struct pam_message * * msgs,
 struct pam_response * * resps, void * pass) {
   if (count != 1 || (* msgs)->msg_style != PAM_PROMPT_ECHO_OFF)
      return PAM_CONV_ERR;
   NEW (struct pam_response, resp, my_strdup (pass), 0);
   * resps = resp;
   return PAM_SUCCESS;
}

static void import_pam_env (pam_handle_t * handle) {
   char * * envlist = pam_getenvlist (handle);
   if (! envlist)
      return;
   for (char * * env = envlist; * env; env ++) {
      char * eq = strchr (* env, '=');
      if (eq) {
         * eq = 0;
         my_setenv (* env, eq + 1);
      }
      free (* env);
   }
   free (envlist);
}

void * open_pam (const char * user, const char * pass, int vt, int display) {
   struct pam_conv conv = {callback, (void *) pass};
   pam_handle_t * handle;
   if (pam_start ("login", user, & conv, & handle) != PAM_SUCCESS)
      fail ("pam_start");
   if (pam_authenticate (handle, 0) != PAM_SUCCESS)
      fail ("pam_authenticate");
   SPRINTF (vt_name, "/dev/tty%d", vt);
   pam_set_item (handle, PAM_TTY, vt_name);
   SPRINTF (disp_name, ":%d", display);
   pam_set_item (handle, PAM_XDISPLAY, disp_name);
   if (pam_setcred (handle, PAM_ESTABLISH_CRED) != PAM_SUCCESS)
      fail ("pam_setcred");
   if (pam_open_session (handle, 0) != PAM_SUCCESS)
      fail ("pam_open_session");
   import_pam_env (handle);
   return handle;
}

void close_pam (void * handle)
{
   pam_close_session (handle, 0);
   pam_end (handle, PAM_SUCCESS);
}
