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
#include <gtk/gtk.h>

typedef struct {
   GtkWidget * window, * fixed, * frame, * pages, * log_in_page, * fail_page;
   GtkWidget * name_entry, * password_entry, * log_in_button, * back_button;
   GtkWidget * status_bar, * sleep_button, * shut_down_button, * reboot_button;
} ui_t;

void ui_create (ui_t * ui);
void ui_destroy (ui_t * ui);

bool ui_show (ui_t * ui);
void ui_hide (ui_t * ui);

void ui_set_status (ui_t * ui, const char * status);
void ui_set_can_quit (ui_t * ui, bool can_quit);

#endif
