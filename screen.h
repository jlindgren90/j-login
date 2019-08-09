/*
 * J-Login - screen.h
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

#ifndef JLOGIN_SCREEN_H
#define JLOGIN_SCREEN_H

#include <X11/Xlib.h>

void init_vt (void);
void set_vt (int vt);

void start_x (int * display, int * vt);

void ssaver_init (Display * display);
int ssaver_active_ms (Display * display);

#endif
