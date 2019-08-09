/*
 * J-Login - utils.h
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

#ifndef JLOGIN_UTILS_H
#define JLOGIN_UTILS_H

#include <stdbool.h>
#include <sys/types.h>

#define NAME "J Login"

#define NEW(t, n, ...) \
 t * n = my_malloc (sizeof (t)); \
 * n = (t) {__VA_ARGS__}

#define SPRINTF(n, ...) \
 char n[snprintf (NULL, 0, __VA_ARGS__) + 1]; \
 snprintf (n, sizeof n, __VA_ARGS__)

void error (const char * message);
void fail (const char * func);
void fail2 (const char * func, const char * param);
void * my_malloc (int size);
char * my_strdup (const char * string);
void my_setenv (const char * name, const char * value);
bool exist (const char * file);
void wait_for_exist (const char * folder, const char * file);
pid_t launch (const char * const * args);
pid_t launch_set_display (const char * const * args, int display);
bool exited (pid_t process);
void wait_for_exit (pid_t process);
void my_kill (pid_t process);
bool check_password (const char * name, const char * password);
void set_user (const char * user);
pid_t launch_set_user (const char * user, const char * password, int vt,
 int display, const char * const * args);

#endif
