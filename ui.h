/*
 * J-Login - ui.h
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

#ifndef JLOGIN_UI_H
#define JLOGIN_UI_H

#include <stdbool.h>

typedef struct ui_s ui_t;

ui_t * ui_create (GdkDisplay * display, const char * status, bool can_quit);
void ui_update (ui_t * ui, const char * status, bool can_quit);
void ui_destroy (ui_t * ui);

#endif
