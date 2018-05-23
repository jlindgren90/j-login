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

#include <X11/Xlib.h>

struct console {
   int vt, display, process;
   Display * handle;
};

void init_vt (void);
void close_vt (void);
int get_vt (void);
int get_open_vt (void);
void release_vt (int vt);
void set_vt (int vt);
void lock_vt (void);
void unlock_vt (void);

struct console * start_x (void);
void popup_x (const struct console * console);
void close_x (struct console * console);
void set_display (int display);
char block_x (Display * handle, Window window);
void unblock_x (Display * handle);

#endif
