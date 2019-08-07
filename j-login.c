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

#include <gtk/gtk.h>

#include "actions.h"
#include "screen.h"
#include "ui.h"
#include "utils.h"

typedef struct {
   xhandle_t * xhandle;
   GdkDisplay * display;
   ui_t * ui;
   char * user;
   pid_t process;
} console_t;

static GList * consoles;
static int user_count;
static char status[256];

static void run_setup (void) {
   static const char * const args[] = {"/usr/bin/j-login-setup", NULL};
   wait_for_exit (launch (args));
}

static void update_ui (void) {
   for (GList * node = consoles; node; node = node->next) {
      console_t * console = node->data;
      if (console->ui)
         ui_update (console->ui, status, ! user_count);
   }
}

static bool show_ui (console_t * console) {
   if (! console->ui) {
      console->ui = ui_create (console->display, status, ! user_count);
      if (! console->ui)
         return false;
   }
   return true;
}

static void hide_ui (console_t * console) {
   if (! console->ui)
      return;
   ui_destroy (console->ui);
   console->ui = NULL;
}

static console_t * open_console (void) {
   xhandle_t * xhandle = start_x ();
   SPRINTF (disp_name, ":%d", xhandle->display);
   GdkDisplay * display;
   if (! consoles) {
      /* open the primary display */
      my_setenv ("DISPLAY", disp_name);
      gtk_init (NULL, NULL);
      display = gdk_display_get_default ();
   } else {
      /* open a secondary display */
      display = gdk_display_open (disp_name);
      if (! display)
         fail2 ("gdk_display_open", disp_name);
   }
   NEW (console_t, console, xhandle, display, NULL, NULL, -1);
   consoles = g_list_append (consoles, console);
   return console;
}

static console_t * get_unused_console (void) {
   for (GList * node = consoles; node; node = node->next) {
      console_t * console = node->data;
      if (! console->user)
         return console;
   }
   return NULL;
}

static bool lock_consoles (void) {
   bool locked = true;
   for (GList * node = consoles; node; node = node->next) {
      console_t * console = node->data;
      if (! show_ui (console))
         locked = false;
   }
   return locked;
}

static void activate_console (console_t * console) {
   set_vt (console->xhandle->vt);
}

static void start_session (const char * user, const char * pass) {
   console_t * console = get_unused_console ();
   if (console)
      hide_ui (console);
   else
      console = open_console ();
   activate_console (console);
   console->user = my_strdup (user);
   static const char * const args[] = {"/usr/bin/j-session", NULL};
   console->process = launch_set_user (user, pass, console->xhandle->vt,
    console->xhandle->display, args);
}

static bool try_activate_session (const char * user) {
   for (GList * node = consoles; node; node = node->next) {
      console_t * console = node->data;
      if (! console->user || strcmp (console->user, user))
         continue;
      hide_ui (console);
      activate_console (console);
      return true;
   }
   return false;
}

static void end_session (console_t * console) {
   free (console->user);
   console->user = NULL;
   console->process = -1;
   show_ui (console);
}

static int update_cb (void * unused) {
   (void) unused;
   user_count = 0;
   snprintf (status, sizeof status, "Logged in:");
   for (GList * node = consoles; node;) {
      GList * next = node->next;
      console_t * console = node->data;
      if (console->user) {
         if (exited (console->process))
            end_session (console);
         else {
            user_count ++;
            int length = strlen (status);
            snprintf (status + length, sizeof status - length, " %s", console->user);
         }
      }
      node = next;
   }
   update_ui ();
   return G_SOURCE_REMOVE;
}

static int popup_cb (void * unused) {
   (void) unused;
   lock_consoles ();
   return G_SOURCE_REMOVE;
}

void do_sleep (void) {
   static const char * const args[] = {"/usr/bin/j-login-sleep", NULL};
   while (gtk_events_pending ())
      gtk_main_iteration ();
   wait_for_exit (launch (args));
}

static int sleep_cb (void * unused) {
   (void) unused;
   if (lock_consoles ())
      do_sleep ();
   return G_SOURCE_REMOVE;
}

static void * signal_thread (void * unused) {
   (void) unused;
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

bool log_in (const char * name, const char * password) {
   if (! strcmp (name, "root") || ! check_password (name, password))
      return false;
   if (try_activate_session (name))
      return true;
   start_session (name, password);
   update_cb (NULL);
   return true;
}

void queue_reboot (void) {
   const char * const args[] = {"reboot", NULL};
   wait_for_exit (launch (args));
}

void queue_shutdown (void) {
   const char * const args[] = {"poweroff", NULL};
   wait_for_exit (launch (args));
}

int main (void) {
   set_user ("root");
   init_vt ();
   open_console ();
   run_setup ();
   start_signal_thread ();
   update_cb (NULL);
   show_ui (consoles->data);
   gtk_main ();
   return 0;
}
