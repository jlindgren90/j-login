/*
 * J-Login - ui.c
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

#include <stdlib.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <X11/Xlib.h>

#include "actions.h"
#include "ui.h"
#include "utils.h"

struct ui_s {
   GtkWidget * window, * fixed, * frame, * pages, * log_in_page, * fail_page;
   GtkWidget * name_entry, * password_entry, * log_in_button, * back_button;
   GtkWidget * status_bar, * sleep_button, * shut_down_button, * reboot_button;
   GList * extra_windows;
};

/* override GTK symbol so that GTK never releases our grab */
GdkGrabStatus gdk_pointer_grab (GdkWindow * window, gboolean owner_events,
 GdkEventMask event_mask, GdkWindow * confine_to, GdkCursor * cursor,
 guint32 time) {
   (void) window;
   (void) owner_events;
   (void) event_mask;
   (void) confine_to;
   (void) cursor;
   (void) time;
   return GDK_GRAB_ALREADY_GRABBED;
}

/* override GTK symbol so that GTK never releases our grab */
void gdk_pointer_ungrab (guint32 time) {
   (void) time;
}

/* override GTK symbol so that GTK never releases our grab */
GdkGrabStatus gdk_keyboard_grab (GdkWindow * window, gboolean owner_events, guint32 time) {
   (void) window;
   (void) owner_events;
   (void) time;
   return GDK_GRAB_ALREADY_GRABBED;
}

/* override GTK symbol so that GTK never releases our grab */
void gdk_keyboard_ungrab (guint32 time) {
   (void) time;
}

static void unblock_x (Display * handle) {
   XUngrabPointer (handle, CurrentTime);
   XUngrabKeyboard (handle, CurrentTime);
}

static bool block_x (Display * handle, Window window) {
   bool mouse = false, keyboard = false;
   for (int count = 0; count < 50; count ++) {
      if (! keyboard)
         keyboard = (XGrabKeyboard (handle, window, true, GrabModeAsync,
          GrabModeAsync, CurrentTime) == GrabSuccess);
      if (! mouse)
         mouse = (XGrabPointer (handle, window, true, 0, GrabModeAsync,
          GrabModeAsync, window, None, CurrentTime) == GrabSuccess);
      if (keyboard && mouse)
         return true;
      struct timespec delay = {.tv_sec = 0, .tv_nsec = 20000000};
      nanosleep (& delay, 0);
   }
   unblock_x (handle);
   return false;
}

static GtkWidget * make_window_for_screen (GdkScreen * screen) {
   GtkWidget * window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_screen ((GtkWindow *) window, screen);
   gtk_window_set_keep_above ((GtkWindow *) window, true);
   return window;
}

static void resize_window_to_screen (GtkWidget * window, GdkScreen * screen)
{
   int w = gdk_screen_get_width (screen), h = gdk_screen_get_height (screen);
   gtk_window_resize ((GtkWindow *) window, w, h);
}

static void make_window (ui_t * ui, GdkDisplay * display) {
   GdkScreen * screen = gdk_display_get_default_screen (display);
   ui->window = make_window_for_screen (screen);
   ui->fixed = gtk_fixed_new ();
   ui->frame = gtk_vbox_new (false, 6);
   GtkWidget * icon = gtk_image_new_from_file ("/usr/share/pixmaps/j-login.png");
   gtk_box_pack_start ((GtkBox *) ui->frame, icon, false, false, 0);
   ui->pages = gtk_hbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) ui->frame, ui->pages, true, false, 0);
   gtk_fixed_put ((GtkFixed *) ui->fixed, ui->frame, 0, 0);
   gtk_container_add ((GtkContainer *) ui->window, ui->fixed);
}

static void make_extra_windows (ui_t * ui, GdkDisplay * display) {
   ui->extra_windows = NULL;
   GdkScreen * def_screen = gdk_display_get_default_screen (display);
   int n_screens = gdk_display_get_n_screens (display);
   for (int s = 0; s < n_screens; s ++) {
      GdkScreen * screen = gdk_display_get_screen (display, s);
      if (screen == def_screen)
         continue;
      GtkWidget * window = make_window_for_screen (screen);
      ui->extra_windows = g_list_append (ui->extra_windows, window);
      resize_window_to_screen (window, screen);
      g_signal_connect_object (screen, "size-changed",
       (GCallback) resize_window_to_screen, window, G_CONNECT_SWAPPED);
   }
}

static void make_log_in_page (ui_t * ui) {
   ui->log_in_page = gtk_vbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) ui->pages, ui->log_in_page, true, false, 0);
   GtkWidget * prompt_box = gtk_hbox_new (false, 6);
   GtkWidget * prompt = gtk_label_new ("Name and password:");
   gtk_box_pack_start ((GtkBox *) prompt_box, prompt, false, false, 0);
   gtk_box_pack_start ((GtkBox *) ui->log_in_page, prompt_box, false, false, 0);
   ui->name_entry = gtk_entry_new ();
   gtk_box_pack_start ((GtkBox *) ui->log_in_page, ui->name_entry, false, false, 0);
   ui->password_entry = gtk_entry_new ();
   gtk_entry_set_visibility ((GtkEntry *) ui->password_entry, false);
   gtk_entry_set_activates_default ((GtkEntry *) ui->password_entry, true);
   gtk_box_pack_start ((GtkBox *) ui->log_in_page, ui->password_entry, false, false, 0);
   g_signal_connect_swapped (ui->name_entry, "activate", (GCallback)
    gtk_widget_grab_focus, ui->password_entry);
   GtkWidget * log_in_button_box = gtk_hbox_new (false, 6);
   ui->log_in_button = gtk_button_new_with_label ("Log in");
   gtk_widget_set_can_focus (ui->log_in_button, false);
   gtk_widget_set_can_default (ui->log_in_button, true);
   gtk_box_pack_end ((GtkBox *) log_in_button_box, ui->log_in_button, false, false, 0);
   gtk_box_pack_start ((GtkBox *) ui->log_in_page, log_in_button_box, false, false, 0);
   gtk_widget_show_all (ui->log_in_page);
}

static void make_fail_page (ui_t * ui) {
   ui->fail_page = gtk_vbox_new (false, 6);
   gtk_widget_set_no_show_all (ui->fail_page, true);
   gtk_box_pack_start ((GtkBox *) ui->pages, ui->fail_page, true, false, 0);
   GtkWidget * fail_message_box = gtk_hbox_new (false, 6);
   GtkWidget * fail_message = gtk_label_new ("Authentication failed.");
   gtk_box_pack_start ((GtkBox *) fail_message_box, fail_message, false, false, 0);
   gtk_widget_show_all (fail_message_box);
   gtk_box_pack_start ((GtkBox *) ui->fail_page, fail_message_box, false, false, 0);
   GtkWidget * back_button_box = gtk_hbox_new (false, 6);
   ui->back_button = gtk_button_new_with_label ("Go back");
   gtk_widget_set_can_default (ui->back_button, true);
   gtk_box_pack_end ((GtkBox *) back_button_box, ui->back_button, false, false, 0);
   gtk_widget_show_all (back_button_box);
   gtk_box_pack_start ((GtkBox *) ui->fail_page, back_button_box, false, false, 0);
}

static void make_tool_box (ui_t * ui) {
   GtkWidget * tool_box = gtk_hbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) ui->frame, tool_box, false, false, 0);
   ui->status_bar = gtk_label_new ("");
   gtk_box_pack_start ((GtkBox *) tool_box, ui->status_bar, false, false, 0);
   ui->reboot_button = gtk_button_new_with_mnemonic ("_Reboot");
   gtk_box_pack_end ((GtkBox *) tool_box, ui->reboot_button, false, false, 0);
   ui->shut_down_button = gtk_button_new_with_mnemonic ("Sh_ut down");
   gtk_box_pack_end ((GtkBox *) tool_box, ui->shut_down_button, false, false, 0);
   ui->sleep_button = gtk_button_new_with_mnemonic ("_Sleep");
   gtk_box_pack_end ((GtkBox *) tool_box, ui->sleep_button, false, false, 0);
   gtk_widget_set_can_focus (ui->reboot_button, false);
   gtk_widget_set_can_focus (ui->shut_down_button, false);
   gtk_widget_set_can_focus (ui->sleep_button, false);
}

static void do_layout (ui_t * ui) {
   GdkScreen * screen = gtk_window_get_screen ((GtkWindow *) ui->window);
   resize_window_to_screen (ui->window, screen);
   int monitor = gdk_screen_get_primary_monitor (screen);
   GdkRectangle rect;
   gdk_screen_get_monitor_geometry (screen, monitor, & rect);
   gtk_fixed_move ((GtkFixed *) ui->fixed, ui->frame, rect.x + 6, rect.y + 6);
   gtk_widget_set_size_request (ui->frame, rect.width - 12, rect.height - 12);
}

static void reset (ui_t * ui) {
   gtk_widget_hide (ui->fail_page);
   gtk_entry_set_text ((GtkEntry *) ui->name_entry, "");
   gtk_entry_set_text ((GtkEntry *) ui->password_entry, "");
   gtk_widget_show (ui->log_in_page);
   gtk_widget_grab_focus (ui->name_entry);
   gtk_widget_grab_default (ui->log_in_button);
}

static void realize_cb (ui_t * ui) {
   GdkWindow * gdkw = gtk_widget_get_window (ui->window);
   gdk_window_set_override_redirect (gdkw, true);
}

static void screen_changed (ui_t * ui) {
   if (gtk_widget_get_visible (ui->window))
      do_layout (ui);
}

static void attempt_login (ui_t * ui) {
   char * name = my_strdup (gtk_entry_get_text ((GtkEntry *) ui->name_entry));
   char * password = my_strdup (gtk_entry_get_text ((GtkEntry *) ui->password_entry));
   reset (ui);
   if (! log_in (name, password)) {
      gtk_widget_hide (ui->log_in_page);
      gtk_widget_show (ui->fail_page);
      gtk_widget_grab_focus (ui->back_button);
      gtk_widget_grab_default (ui->back_button);
   }
   free (name);
   free (password);
}

static void set_up_window (ui_t * ui) {
   g_signal_connect_swapped (ui->window, "realize", (GCallback) realize_cb, ui);
   GdkScreen * screen = gtk_widget_get_screen (ui->window);
   g_signal_connect_swapped (screen, "monitors-changed", (GCallback) screen_changed, ui);
   g_signal_connect_swapped (screen, "size-changed", (GCallback) screen_changed, ui);
   g_signal_connect_swapped (ui->log_in_button, "clicked", (GCallback) attempt_login, ui);
   g_signal_connect_swapped (ui->back_button, "clicked", (GCallback) reset, ui);
   g_signal_connect (ui->sleep_button, "clicked", (GCallback) do_sleep, NULL);
   g_signal_connect (ui->shut_down_button, "clicked", (GCallback) queue_shutdown, NULL);
   g_signal_connect (ui->reboot_button, "clicked", (GCallback) queue_reboot, NULL);
   gtk_widget_grab_focus (ui->name_entry);
   gtk_widget_grab_default (ui->log_in_button);
}

static void show_windows (ui_t * ui) {
   for (GList * node = ui->extra_windows; node; node = node->next)
      gtk_widget_show_all ((GtkWidget *) node->data);
   gtk_widget_show_all (ui->window);
   gtk_window_present ((GtkWindow *) ui->window);
}

ui_t * ui_create (GdkDisplay * display, const char * status, bool can_quit) {
   ui_t * ui = my_malloc (sizeof (ui_t));
   make_window (ui, display);
   make_extra_windows (ui, display);
   make_log_in_page (ui);
   make_fail_page (ui);
   make_tool_box (ui);
   set_up_window (ui);
   do_layout (ui);
   ui_update (ui, status, can_quit);
   show_windows (ui);
   GdkWindow * gdkw = gtk_widget_get_window (ui->window);
   if (block_x (GDK_WINDOW_XDISPLAY (gdkw), GDK_WINDOW_XID (gdkw)))
      return ui;
   ui_destroy (ui);
   return NULL;
}

void ui_update (ui_t * ui, const char * status, bool can_quit) {
   gtk_label_set_text ((GtkLabel *) ui->status_bar, status);
   gtk_widget_set_sensitive (ui->shut_down_button, can_quit);
   gtk_widget_set_sensitive (ui->reboot_button, can_quit);
}

void ui_destroy (ui_t * ui) {
   GdkWindow * gdkw = gtk_widget_get_window (ui->window);
   unblock_x (GDK_WINDOW_XDISPLAY (gdkw));
   GdkScreen * screen = gtk_widget_get_screen (ui->window);
   g_signal_handlers_disconnect_by_data (screen, ui);
   gtk_widget_destroy (ui->window);
   g_list_free_full (ui->extra_windows, (GDestroyNotify) gtk_widget_destroy);
   free (ui);
}
