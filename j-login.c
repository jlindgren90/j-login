/*
 * J-Login - j-login.c
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

#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "screen.h"
#include "utils.h"

struct session {
   char * user;
   struct console * console;
   int process;
};

static struct console * first_console;
static GtkWidget * window, * fixed, * frame, * pages, * log_in_page,
 * fail_page, * name_entry, * password_entry, * log_in_button, * ok_button,
 * status_bar, * sleep_button, * shut_down_button, * reboot_button;
static const struct session * active_session;
static GList * sessions;
static bool reboot;

static void set_up_window (void);
static int update_cb (void * unused);

static void run_setup (void) {
   static const char * const args[] = {"/usr/sbin/j-login-setup", NULL};
   wait_for_exit (launch (args));
}

static void make_window (void) {
   window = gtk_window_new (GTK_WINDOW_TOPLEVEL);
   gtk_window_set_keep_above ((GtkWindow *) window, true);
   fixed = gtk_fixed_new ();
   frame = gtk_vbox_new (false, 6);
   GtkWidget * icon = gtk_image_new_from_file ("/usr/share/pixmaps/j-login.png");
   gtk_box_pack_start ((GtkBox *) frame, icon, false, false, 0);
   pages = gtk_hbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) frame, pages, true, false, 0);
   gtk_fixed_put ((GtkFixed *) fixed, frame, 0, 0);
   gtk_container_add ((GtkContainer *) window, fixed);
}

static void make_log_in_page (void) {
   log_in_page = gtk_vbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) pages, log_in_page, true, false, 0);
   GtkWidget * prompt_box = gtk_hbox_new (false, 6);
   GtkWidget * prompt = gtk_label_new ("Name and password:");
   gtk_box_pack_start ((GtkBox *) prompt_box, prompt, false, false, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, prompt_box, false, false, 0);
   name_entry = gtk_entry_new ();
   gtk_box_pack_start ((GtkBox *) log_in_page, name_entry, false, false, 0);
   password_entry = gtk_entry_new ();
   gtk_entry_set_visibility ((GtkEntry *) password_entry, false);
   gtk_entry_set_activates_default ((GtkEntry *) password_entry, true);
   gtk_box_pack_start ((GtkBox *) log_in_page, password_entry, false, false, 0);
   g_signal_connect_swapped (name_entry, "activate", (GCallback)
    gtk_widget_grab_focus, password_entry);
   GtkWidget * log_in_button_box = gtk_hbox_new (false, 6);
   log_in_button = gtk_button_new_with_label ("Log in");
   gtk_widget_set_can_focus (log_in_button, false);
   gtk_widget_set_can_default (log_in_button, true);
   gtk_box_pack_end ((GtkBox *) log_in_button_box, log_in_button, false, false, 0);
   gtk_box_pack_start ((GtkBox *) log_in_page, log_in_button_box, false, false, 0);
   gtk_widget_show_all (log_in_page);
}

static void make_fail_page (void) {
   fail_page = gtk_vbox_new (false, 6);
   gtk_widget_set_no_show_all (fail_page, true);
   gtk_box_pack_start ((GtkBox *) pages, fail_page, true, false, 0);
   GtkWidget * fail_message_box = gtk_hbox_new (false, 6);
   GtkWidget * fail_message = gtk_label_new ("Authentication failed.");
   gtk_box_pack_start ((GtkBox *) fail_message_box, fail_message, false, false, 0);
   gtk_widget_show_all (fail_message_box);
   gtk_box_pack_start ((GtkBox *) fail_page, fail_message_box, false, false, 0);
   GtkWidget * ok_button_box = gtk_hbox_new (false, 6);
   ok_button = gtk_button_new_with_label ("O.K.");
   gtk_widget_set_can_default (ok_button, true);
   gtk_box_pack_end ((GtkBox *) ok_button_box, ok_button, false, false, 0);
   gtk_widget_show_all (ok_button_box);
   gtk_box_pack_start ((GtkBox *) fail_page, ok_button_box, false, false, 0);
}

static void make_tool_box (void) {
   GtkWidget * tool_box = gtk_hbox_new (false, 6);
   gtk_box_pack_start ((GtkBox *) frame, tool_box, false, false, 0);
   status_bar = gtk_label_new ("");
   gtk_box_pack_start ((GtkBox *) tool_box, status_bar, false, false, 0);
   reboot_button = gtk_button_new_with_mnemonic ("_Reboot");
   gtk_box_pack_end ((GtkBox *) tool_box, reboot_button, false, false, 0);
   shut_down_button = gtk_button_new_with_mnemonic ("Sh_ut down");
   gtk_box_pack_end ((GtkBox *) tool_box, shut_down_button, false, false, 0);
   sleep_button = gtk_button_new_with_mnemonic ("_Sleep");
   gtk_box_pack_end ((GtkBox *) tool_box, sleep_button, false, false, 0);
   gtk_widget_set_can_focus (reboot_button, false);
   gtk_widget_set_can_focus (shut_down_button, false);
   gtk_widget_set_can_focus (sleep_button, false);
}

static void reset (void) {
   gtk_widget_hide (fail_page);
   gtk_entry_set_text ((GtkEntry *) name_entry, "");
   gtk_entry_set_text ((GtkEntry *) password_entry, "");
   gtk_widget_show (log_in_page);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static struct session * add_session (const char * user,
 struct console * console, int process) {
   struct session * new = my_malloc (sizeof (struct session));
   new->user = my_strdup (user);
   new->console = console;
   new->process = process;
   sessions = g_list_append (sessions, new);
   return new;
}

static void remove_session (struct session * session) {
   sessions = g_list_remove (sessions, session);
   free (session->user);
   free (session);
}

static void do_layout (void) {
   GdkScreen * screen = gtk_window_get_screen ((GtkWindow *) window);
   gtk_window_resize ((GtkWindow *) window, gdk_screen_get_width (screen),
    gdk_screen_get_height (screen));
   GdkRectangle rect;
   gdk_screen_get_monitor_geometry (screen, gdk_screen_get_primary_monitor
    (screen), & rect);
   gtk_fixed_move ((GtkFixed *) fixed, frame, rect.x + 6, rect.y + 6);
   gtk_widget_set_size_request (frame, rect.width - 12, rect.height - 12);
}

static bool show_window (void) {
   do_layout ();
   reset ();
   gtk_widget_show_all (window);
   gtk_window_present ((GtkWindow *) window);
   GdkWindow * gdkw = gtk_widget_get_window (window);
   if (! block_x (GDK_WINDOW_XDISPLAY (gdkw), GDK_WINDOW_XID (gdkw))) {
      gtk_widget_hide (window);
      return false;
   }
   popup_x (first_console);
   return true;
}

static void hide_window (void) {
   GdkWindow * gdkw = gtk_widget_get_window (window);
   unblock_x (GDK_WINDOW_XDISPLAY (gdkw));
   gtk_widget_hide (window);
}

static void realize_cb (void) {
   GdkWindow * gdkw = gtk_widget_get_window (window);
   gdk_window_set_override_redirect (gdkw, true);
}

static void screen_changed (void) {
   if (gtk_widget_get_visible (window))
      do_layout ();
}

static int launch_session (const char * user, const char * pass,
 struct console * console) {
   static const char * const args[] = {"/usr/bin/j-session", NULL};
   return launch_set_user (user, pass, console->vt, console->display, args);
}

static void start_session (const char * user, const char * pass) {
   struct console * console;
   if (sessions)
      console = start_x ();
   else {
      hide_window ();
      popup_x (first_console);
      console = first_console;
   }
   int process = launch_session (user, pass, console);
   active_session = add_session (user, console, process);
}

static int find_session_cb (const struct session * session, const char * user) {
   return strcmp (session->user, user);
}

static bool try_activate_session (const char * user) {
   GList * node = g_list_find_custom (sessions, user, (GCompareFunc)
    find_session_cb);
   if (! node)
      return false;
   const struct session * session = node->data;
   if (session->console == first_console)
      hide_window ();
   popup_x (session->console);
   active_session = session;
   return true;
}

static void end_session (struct session * session) {
   if (session->console != first_console)
      close_x (session->console);
   remove_session (session);
   if (session == active_session) {
      active_session = NULL;
      show_window ();
   }
}

static void check_session_cb (struct session * session) {
   if (exited (session->process))
      end_session (session);
}

static void print_session_cb (const struct session * session, char * status) {
   int length = strlen (status);
   snprintf (status + length, 256 - length, " %s", session->user);
}

static int update_cb (void * unused) {
   g_list_foreach (sessions, (GFunc) check_session_cb, NULL);
   char status[256];
   snprintf (status, sizeof status, "Logged in:");
   g_list_foreach (sessions, (GFunc) print_session_cb, status);
   gtk_label_set_text ((GtkLabel *) status_bar, status);
   if (sessions) {
      gtk_widget_set_sensitive (shut_down_button, false);
      gtk_widget_set_sensitive (reboot_button, false);
   } else {
      gtk_widget_set_sensitive (shut_down_button, true);
      gtk_widget_set_sensitive (reboot_button, true);
   }
   return G_SOURCE_REMOVE;
}

static int popup_cb (void * unused) {
   show_window ();
   return G_SOURCE_REMOVE;
}

static void do_sleep (void) {
   static const char * const args[] = {"/usr/sbin/j-login-sleep", NULL};
   while (gtk_events_pending ())
      gtk_main_iteration ();
   wait_for_exit (launch (args));
}

static int sleep_cb (void * unused) {
   if (show_window ())
      do_sleep ();
   return G_SOURCE_REMOVE;
}

static void * signal_thread (void * unused) {
   sigset_t signals;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   int signal;
   while (! sigwait (& signals, & signal)) {
      if (signal == SIGCHLD)
         g_timeout_add (0, update_cb, NULL);
      else if (signal == SIGUSR1)
         g_timeout_add (0, popup_cb, NULL);
      else if (signal == SIGUSR2)
         g_timeout_add (0, sleep_cb, NULL);
   }
   fail ("sigwait");
   return 0;
}

static void start_signal_thread (void) {
   sigset_t signals;
   sigemptyset (& signals);
   sigaddset (& signals, SIGCHLD);
   sigaddset (& signals, SIGUSR1);
   sigaddset (& signals, SIGUSR2);
   if (sigprocmask (SIG_SETMASK, & signals, NULL) < 0)
      fail ("sigprocmask");
   pthread_t thread;
   if (pthread_create (& thread, NULL, signal_thread, NULL) < 0)
      fail ("pthread_create");
}

static void log_in (void) {
   char * name = my_strdup (gtk_entry_get_text ((GtkEntry *) name_entry));
   char * password = my_strdup (gtk_entry_get_text ((GtkEntry *) password_entry));
   reset ();
   if (strcmp (name, "root") && check_password (name, password)) {
      if (! try_activate_session (name)) {
         start_session (name, password);
         update_cb (NULL);
      }
   } else {
      gtk_widget_hide (log_in_page);
      gtk_widget_show (fail_page);
      gtk_widget_grab_focus (ok_button);
      gtk_widget_grab_default (ok_button);
   }
   free (name);
   free (password);
}

static void queue_reboot (void) {
   reboot = true;
   gtk_main_quit ();
}

static void set_up_window (void) {
   g_signal_connect (window, "realize", (GCallback) realize_cb, NULL);
   GdkScreen * screen = gtk_widget_get_screen (window);
   g_signal_connect (screen, "monitors-changed", (GCallback) screen_changed, NULL);
   g_signal_connect (screen, "size-changed", (GCallback) screen_changed, NULL);
   g_signal_connect (log_in_button, "clicked", (GCallback) log_in, NULL);
   g_signal_connect (ok_button, "clicked", (GCallback) reset, NULL);
   g_signal_connect (sleep_button, "clicked", (GCallback) do_sleep, NULL);
   g_signal_connect (shut_down_button, "clicked", (GCallback) gtk_main_quit, NULL);
   g_signal_connect (reboot_button, "clicked", (GCallback) queue_reboot, NULL);
   gtk_widget_grab_focus (name_entry);
   gtk_widget_grab_default (log_in_button);
}

static void set_up_interface (void) {
   make_window ();
   make_log_in_page ();
   make_fail_page ();
   make_tool_box ();
   set_up_window ();
   update_cb (NULL);
}

static void poweroff ()
{
   const char * const poweroff_args[] = {"poweroff", NULL};
   const char * const reboot_args[] = {"reboot", NULL};
   wait_for_exit (launch (reboot ? reboot_args : poweroff_args));
}

int main (int argc, char * * argv) {
   set_user ("root");
   init_vt ();
   int old_vt = get_vt ();
   first_console = start_x ();
   set_display (first_console->display);
   gtk_init (NULL, NULL);
   run_setup ();
   start_signal_thread ();
   set_up_interface ();
   show_window ();
   gtk_main ();
   gtk_widget_destroy (window);
   close_x (first_console);
   set_vt (old_vt);
   close_vt ();
   poweroff ();
   return 0;
}
