/*
 * J-Login - j-login-lock.c
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

#include <unistd.h>

int main (void) {
   /* we are setuid, so use the full path */
   static const char * const args[] = {"/usr/bin/pkill", "-USR1", "j-login", NULL};
   execv (args[0], (char * const *) args);
   /* if we get here, return an error */
   return 1;
}
