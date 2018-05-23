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
#include <security/pam_appl.h>

#include "pam.h"
#include "utils.h"

static int callback (int count, const struct pam_message * * msgs,
 struct pam_response * * resps, void * pass) {
   if (count != 1 || (* msgs)->msg_style != PAM_PROMPT_ECHO_OFF)
      return PAM_CONV_ERR;
   * resps = my_malloc (sizeof (struct pam_response));
   (* resps)->resp = my_strdup (pass);
   (* resps)->resp_retcode = 0;
   return PAM_SUCCESS;
}

void * open_pam (const char * user, const char * pass, int vt, int display) {
   struct pam_conv conv;
   pam_handle_t * handle;
   conv.conv = callback;
   conv.appdata_ptr = (void *) pass;
   if (pam_start ("login", user, & conv, & handle) != PAM_SUCCESS)
      return 0;
   pam_authenticate (handle, 0);
   SPRINTF (vt_name, "/dev/tty%d", vt);
   pam_set_item (handle, PAM_TTY, vt_name);
   SPRINTF (disp_name, ":%d", display);
   pam_set_item (handle, PAM_XDISPLAY, disp_name);
   pam_open_session (handle, 0);
   return handle;
}

void close_pam (void * handle)
{
   if (handle) {
      pam_close_session (handle, 0);
      pam_end (handle, PAM_SUCCESS);
   }
}
