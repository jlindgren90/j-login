/*
 * J-Login - screen.h
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

#ifndef JLOGIN_SCREEN_H
#define JLOGIN_SCREEN_H

#include <stdbool.h>
#include <X11/Xlib.h>

typedef struct {
   int vt, display;
   pid_t process;
} console_t;

void init_vt (void);
void close_vt (void);
int get_vt (void);
int get_open_vt (void);
void release_vt (int vt);
void set_vt (int vt);

console_t * start_x (void);
void popup_x (const console_t * console);
void close_x (console_t * console);
void set_display (int display);
bool block_x (Display * handle, Window window);
void unblock_x (Display * handle);

#endif
